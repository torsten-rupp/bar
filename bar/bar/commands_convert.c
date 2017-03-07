/***********************************************************************\
*
* $Revision: 7195 $
* $Date: 2017-02-22 03:54:06 +0100 (Wed, 22 Feb 2017) $
* $Author: torsten $
* Contents: Backup ARchiver archive convert functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "autofree.h"
#include "strings.h"
#include "stringlists.h"
#include "msgqueues.h"

#include "errors.h"
#include "patterns.h"
#include "files.h"
#include "filesystems.h"
#include "archive.h"

#include "commands_convert.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// data buffer size
#define BUFFER_SIZE (64*1024)

// max. number of entry messages
#define MAX_ENTRY_MSG_QUEUE 256

/***************************** Datatypes *******************************/

// convert info
typedef struct
{
  StorageSpecifier    *storageSpecifier;                  // storage specifier structure
  StorageInfo         storageInfo;
  ArchiveHandle       *destinationArchiveHandle;
  const JobOptions    *jobOptions;
  GetPasswordFunction getPasswordFunction;
  void                *getPasswordUserData;
  LogHandle           *logHandle;                         // log handle

  bool                *pauseTestFlag;                     // TRUE for pause creation
  bool                *requestedAbortFlag;                // TRUE to abort create

  MsgQueue            entryMsgQueue;                      // queue with entries to convert
  MsgQueue            storageMsgQueue;                    // queue with waiting storage files
  bool                storageThreadExitFlag;

  Errors              failError;                          // failure error
} ConvertInfo;

// entry message send to convert threads
typedef struct
{
  StorageInfo       *storageInfo;
  byte              cryptSalt[CRYPT_SALT_LENGTH];
  uint              cryptMode;
  ArchiveEntryTypes archiveEntryType;
  uint64            offset;
} EntryMsg;

// storage message, send from convert threads -> storage thread
typedef struct
{
  ArchiveTypes archiveType;
  String       fileName;                                          // intermediate archive file name
  uint64       fileSize;                                          // intermediate archive size [bytes]
  String       archiveName;                                       // destination archive name
} StorageMsg;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : freeEntryMsg
* Purpose: free file entry message call back
* Input  : entryMsg - entry message
*          userData - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeEntryMsg(EntryMsg *entryMsg, void *userData)
{
  assert(entryMsg != NULL);

  UNUSED_VARIABLE(entryMsg);
  UNUSED_VARIABLE(userData);
}

/***********************************************************************\
* Name   : freeStorageMsg
* Purpose: free storage msg
* Input  : storageMsg - storage message
*          userData   - user data (ignored)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeStorageMsg(StorageMsg *storageMsg, void *userData)
{
  assert(storageMsg != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(storageMsg->archiveName);
  String_delete(storageMsg->fileName);
}

/***********************************************************************\
* Name   : initConvertInfo
* Purpose: initialize convert info
* Input  : convertInfo                - convert info variable
*          storageSpecifier           - storage specifier structure
*destinationArchiveHandle
*          jobOptions                 - job options
*          pauseTestFlag              - pause creation flag (can be
*                                       NULL)
*          requestedAbortFlag         - request abort flag (can be NULL)
*          logHandle                  - log handle (can be NULL)
* Output : createInfo - initialized convert info variable
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initConvertInfo(ConvertInfo       *convertInfo,
                           StorageSpecifier  *storageSpecifier,
                           ArchiveHandle *destinationArchiveHandle,
                           const JobOptions  *jobOptions,
                           bool              *pauseTestFlag,
                           bool              *requestedAbortFlag,
                           LogHandle         *logHandle
                          )
{
  assert(convertInfo != NULL);

  // init variables
  convertInfo->storageSpecifier      = storageSpecifier;
  convertInfo->destinationArchiveHandle = destinationArchiveHandle;
  convertInfo->jobOptions            = jobOptions;
  convertInfo->pauseTestFlag         = pauseTestFlag;
  convertInfo->requestedAbortFlag    = requestedAbortFlag;
  convertInfo->logHandle             = logHandle;
  convertInfo->failError             = ERROR_NONE;
  convertInfo->storageThreadExitFlag = FALSE;

  // init entry name queue, storage queue
  if (!MsgQueue_init(&convertInfo->entryMsgQueue,MAX_ENTRY_MSG_QUEUE))
  {
    HALT_FATAL_ERROR("Cannot initialize entry message queue!");
  }
  if (!MsgQueue_init(&convertInfo->storageMsgQueue,0))
  {
    HALT_FATAL_ERROR("Cannot initialize storage message queue!");
  }

#if 0
  // init locks
  if (!Semaphore_init(&convertInfo->storageInfoLock))
  {
    HALT_FATAL_ERROR("Cannot initialize storage semaphore!");
  }
#endif

  DEBUG_ADD_RESOURCE_TRACE(convertInfo,sizeof(ConvertInfo));
}

/***********************************************************************\
* Name   : doneConvertInfo
* Purpose: deinitialize convert info
* Input  : convertInfo - convert info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneConvertInfo(ConvertInfo *convertInfo)
{
  assert(convertInfo != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(convertInfo,sizeof(ConvertInfo));

  MsgQueue_done(&convertInfo->entryMsgQueue,(MsgQueueMsgFreeFunction)freeEntryMsg,NULL);
}

/***********************************************************************\
* Name   : archiveStore
* Purpose: call back to store archive file
* Input  : storageInfo          - storage info
*          indexHandle          - index handle or NULL if no index
*          jobUUID              - job UUID id
*          scheduleUUID         - schedule UUID id
*          entityId             - index entity id
*          archiveType          - archive type
*          storageId            - index storage id
*          partNumber           - part number or ARCHIVE_PART_NUMBER_NONE
*                                 for single part
*          intermediateFileName - intermediate archive file name
*          intermediateFileSize - intermediate archive size [bytes]
*          userData             - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors archiveStore(StorageInfo  *storageInfo,
                          IndexHandle  *indexHandle,
                          IndexId      uuidId,
                          ConstString  jobUUID,
                          ConstString  scheduleUUID,
                          IndexId      entityId,
                          ArchiveTypes archiveType,
                          IndexId      storageId,
                          int          partNumber,
                          ConstString  intermediateFileName,
                          uint64       intermediateFileSize,
                          void         *userData
                         )
{
  ConvertInfo   *convertInfo = (ConvertInfo*)userData;
  Errors        error;
  FileInfo      fileInfo;
  String        archiveName;
  StorageMsg    storageMsg;
  SemaphoreLock semaphoreLock;

  assert(storageInfo != NULL);
  assert(!String_isEmpty(intermediateFileName));
  assert(convertInfo != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(jobUUID);
  UNUSED_VARIABLE(scheduleUUID);

  // get file info
// TODO replace by getFileSize()
  error = File_getFileInfo(intermediateFileName,&fileInfo);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get archive file name
  archiveName = String_new();
String_setCString(archiveName,"/tmp/t1.bar");
  DEBUG_TESTCODE() { String_delete(archiveName); return DEBUG_TESTCODE_ERROR(); }

  // send to storage thread
  storageMsg.archiveType = archiveType;
  storageMsg.fileName    = String_duplicate(intermediateFileName);
  storageMsg.fileSize    = intermediateFileSize;
  storageMsg.archiveName = archiveName;
  DEBUG_TESTCODE() { freeStorageMsg(&storageMsg,NULL); return DEBUG_TESTCODE_ERROR(); }
  if (!MsgQueue_put(&convertInfo->storageMsgQueue,&storageMsg,sizeof(storageMsg)))
  {
fprintf(stderr,"%s, %d: XXXXXXXXXX\n",__FILE__,__LINE__);
    freeStorageMsg(&storageMsg,NULL);
  }

//TODO
#if 0
  // wait for space in temporary directory
  if (globalOptions.maxTmpSize > 0)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->storageInfoLock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
    {
      while (   (createInfo->storage.count > 2)                           // more than 2 archives are waiting
             && (createInfo->storage.bytes > globalOptions.maxTmpSize)    // temporary space limit exceeded
             && (createInfo->failError == ERROR_NONE)
             && !isAborted(createInfo)
            )
      {
        Semaphore_waitModified(&createInfo->storageInfoLock,30*1000);
      }
    }
  }
#endif

  // free resources

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storageThreadCode
* Purpose: archive storage thread
* Input  : createInfo - create info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void storageThreadCode(ConvertInfo *convertInfo)
{
  #define MAX_RETRIES 3

  AutoFreeList     autoFreeList;
  byte             *buffer;
  void             *autoFreeSavePoint;
  StorageMsg       storageMsg;
  Errors           error;
  String           printableStorageName;
  FileInfo         fileInfo;
  Server           server;
  FileHandle       fileHandle;
  uint             retryCount;
  uint64           archiveSize;
  bool             appendFlag;
  StorageHandle    storageHandle;
  ulong            bufferLength;
  SemaphoreLock    semaphoreLock;
  String           pattern;

  String           pathName;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;
  IndexId          existingEntityId;
  IndexId          existingStorageId;
  String           existingStorageName;
  String           existingPathName;
  StorageSpecifier existingStorageSpecifier;

  assert(convertInfo != NULL);
  assert(convertInfo->jobOptions != NULL);

  // init variables
  AutoFree_init(&autoFreeList);
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  printableStorageName = String_new();
  pathName             = String_new();
  existingStorageName  = String_new();
  existingPathName     = String_new();
  Storage_initSpecifier(&existingStorageSpecifier);
  AUTOFREE_ADD(&autoFreeList,buffer,{ free(buffer); });
  AUTOFREE_ADD(&autoFreeList,pathName,{ String_delete(printableStorageName); });
  AUTOFREE_ADD(&autoFreeList,pathName,{ String_delete(pathName); });
  AUTOFREE_ADD(&autoFreeList,existingStorageName,{ String_delete(existingStorageName); });
  AUTOFREE_ADD(&autoFreeList,existingPathName,{ String_delete(existingPathName); });
  AUTOFREE_ADD(&autoFreeList,&existingStorageSpecifier,{ Storage_doneSpecifier(&existingStorageSpecifier); });

fprintf(stderr,"%s, %d: -----+++++\n",__FILE__,__LINE__);
  // store archives
  while (   (convertInfo->failError == ERROR_NONE)
//         && !isAborted(createInfo)
        )
  {
    autoFreeSavePoint = AutoFree_save(&autoFreeList);

    // pause
//    pauseStorage(createInfo);
//    if (isAborted(createInfo)) break;

    // get next archive to store
    if (!MsgQueue_get(&convertInfo->storageMsgQueue,&storageMsg,NULL,sizeof(storageMsg),WAIT_FOREVER))
    {
      break;
    }
fprintf(stderr,"%s, %d: get store %s\n",__FILE__,__LINE__,String_cString(storageMsg.archiveName));
    AUTOFREE_ADD(&autoFreeList,&storageMsg,
                 {
                   File_delete(storageMsg.fileName,FALSE);
fprintf(stderr,"%s, %d: freeStorageMsg in au\n",__FILE__,__LINE__);
                   freeStorageMsg(&storageMsg,NULL);
                 }
                );

    // get printable storage name
    Storage_getPrintableName(printableStorageName,&convertInfo->storageInfo.storageSpecifier,storageMsg.archiveName);

    // get file info
    error = File_getFileInfo(storageMsg.fileName,&fileInfo);
    if (error != ERROR_NONE)
    {
      printError("Cannot get information for file '%s' (error: %s)!\n",
                 String_cString(storageMsg.fileName),
                 Error_getText(error)
                );
      convertInfo->failError = error;

      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }
    DEBUG_TESTCODE() { convertInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); break; }

    // open file to store
    #ifndef NDEBUG
      printInfo(1,"Store '%s' to '%s'...",String_cString(storageMsg.fileName),String_cString(printableStorageName));
    #else /* not NDEBUG */
      printInfo(1,"Store '%s'...",String_cString(printableStorageName));
    #endif /* NDEBUG */
    error = File_open(&fileHandle,storageMsg.fileName,FILE_OPEN_READ);
    if (error != ERROR_NONE)
    {
      printInfo(0,"FAIL!\n");
      printError("Cannot open file '%s' (error: %s)!\n",
                 String_cString(storageMsg.fileName),
                 Error_getText(error)
                );
      convertInfo->failError = error;

      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }
    AUTOFREE_ADD(&autoFreeList,&fileHandle,{ File_close(&fileHandle); });
    DEBUG_TESTCODE() { convertInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); continue; }

    // write data to storage
    retryCount  = 0;
    appendFlag  = FALSE;
    archiveSize = 0LL;
    do
    {
      // next try
      if (retryCount > MAX_RETRIES)
      {
        break;
      }
      retryCount++;

      // pause
//      pauseStorage(createInfo);
//      if (isAborted(createInfo)) break;

      // create storage file
      error = Storage_create(&storageHandle,
                             &convertInfo->storageInfo,
                             storageMsg.archiveName,
                             fileInfo.size
                            );
      if (error != ERROR_NONE)
      {
        if (retryCount <= MAX_RETRIES)
        {
          // retry
          continue;
        }
        else
        {
          printInfo(0,"FAIL!\n");
          printError("Cannot store '%s' (error: %s)\n",
                     String_cString(printableStorageName),
                     Error_getText(error)
                    );
          break;
        }
      }
      DEBUG_TESTCODE() { Storage_close(&storageHandle); error = DEBUG_TESTCODE_ERROR(); break; }

      // store data
      File_seek(&fileHandle,0);
      do
      {
        // pause
//        pauseStorage(convertInfo);
//        if (isAborted(convertInfo)) break;

        // read data from local intermediate file
        error = File_read(&fileHandle,buffer,BUFFER_SIZE,&bufferLength);
        if (error != ERROR_NONE)
        {
          printInfo(0,"FAIL!\n");
          printError("Cannot read file '%s' (error: %s)!\n",
                     String_cString(printableStorageName),
                     Error_getText(error)
                    );
          break;
        }
        DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

        // store data into storage file
        error = Storage_write(&storageHandle,buffer,bufferLength);
        if (error != ERROR_NONE)
        {
          if (retryCount <= MAX_RETRIES)
          {
            // retry
            break;
          }
          else
          {
            printInfo(0,"FAIL!\n");
            printError("Cannot write file '%s' (error: %s)!\n",
                       String_cString(printableStorageName),
                       Error_getText(error)
                      );
            break;
          }
        }
        DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }
      }
      while (   (convertInfo->failError == ERROR_NONE)
//             && !isAborted(createInfo)
             && !File_eof(&fileHandle)
            );

//TODO: on error restore to original size/delete

      // get archive size
      archiveSize = Storage_getSize(&storageHandle);

      // close storage
      Storage_close(&storageHandle);
    }
    while (   (convertInfo->failError == ERROR_NONE)                            // no eror
//           && !isAborted(convertInfo)                                           // not aborted
           && ((error != ERROR_NONE) && (Error_getCode(error) != ENOSPC))      // some error amd not "no space left"
           && (retryCount <= MAX_RETRIES)                                      // still some retry left
          );
    if (error != ERROR_NONE)
    {
      if (convertInfo->failError == ERROR_NONE) convertInfo->failError = error;

      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }

    // close file to store
    File_close(&fileHandle);
    AUTOFREE_REMOVE(&autoFreeList,&fileHandle);

#if 0
    // check if aborted
    if (isAborted(createInfo))
    {
      printInfo(1,"ABORTED\n");
      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }
#endif

    // done
    printInfo(1,"OK\n");
    logMessage(convertInfo->logHandle,
               LOG_TYPE_STORAGE,
               "%s '%s' (%llu bytes)\n",
               appendFlag ? "Appended to" : "Storged",
               String_cString(printableStorageName),
               archiveSize
              );

    // delete temporary storage file
    error = File_delete(storageMsg.fileName,FALSE);
    if (error != ERROR_NONE)
    {
      printWarning("Cannot delete file '%s' (error: %s)!\n",
                   String_cString(storageMsg.fileName),
                   Error_getText(error)
                  );
    }

    // free resources
    freeStorageMsg(&storageMsg,NULL);

    AutoFree_restore(&autoFreeList,autoFreeSavePoint,FALSE);
  }

  // discard unprocessed archives
  while (MsgQueue_get(&convertInfo->storageMsgQueue,&storageMsg,NULL,sizeof(storageMsg),NO_WAIT))
  {
    // delete temporary storage file
    error = File_delete(storageMsg.fileName,FALSE);
    if (error != ERROR_NONE)
    {
      printWarning("Cannot delete file '%s' (error: %s)!\n",
                   String_cString(storageMsg.fileName),
                   Error_getText(error)
                  );
    }

    // free resources
    freeStorageMsg(&storageMsg,NULL);
  }

  // free resoures
  Storage_doneSpecifier(&existingStorageSpecifier);
  String_delete(existingPathName);
  String_delete(existingStorageName);
  String_delete(pathName);
  String_delete(printableStorageName);
  free(buffer);
  AutoFree_done(&autoFreeList);

  convertInfo->storageThreadExitFlag = TRUE;
}

/***********************************************************************\
* Name   : convertFileEntry
* Purpose: convert a file entry in archive
* Input  : sourceArchiveHandle      - source archive handle
*          destinationArchiveHandle - destination archive handle
*          printableStorageName     - printable storage name
*          jobOptions               - job options
*          buffer0,buffer1          - buffers for temporary data
*          bufferSize               - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertFileEntry(ArchiveHandle    *sourceArchiveHandle,
                              ArchiveHandle    *destinationArchiveHandle,
                              const char       *printableStorageName,
                              const JobOptions *jobOptions,
                              byte             *buffer,
                              uint             bufferSize
                             )
{
  Errors                    error;
  ArchiveEntryInfo          sourceArchiveEntryInfo;
  CompressAlgorithms        deltaCompressAlgorithm,byteCompressAlgorithm;
  String                    fileName;
  FileInfo                  fileInfo;
  FileExtendedAttributeList fileExtendedAttributeList;
  ArchiveEntryInfo          destinationArchiveEntryInfo;
  uint64                    fragmentOffset,fragmentSize;
  FileHandle                fileHandle;
  uint64                    length;
  ulong                     bufferLength;
  SemaphoreLock             semaphoreLock;

  // read source file entry
  fileName = String_new();
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = Archive_readFileEntry(&sourceArchiveEntryInfo,
                                sourceArchiveHandle,
                                &deltaCompressAlgorithm,
                                &byteCompressAlgorithm,
                                NULL,  // cryptAlgorithm
                                NULL,  // cryptType
                                fileName,
                                &fileInfo,
                                &fileExtendedAttributeList,
                                NULL,  // deltaSourceName
                                NULL,  // deltaSourceSize
                                &fragmentOffset,
                                &fragmentSize
                               );
  if (error != ERROR_NONE)
  {
    printError("Cannot read 'file' content of archive '%s' (error: %s)!\n",
               printableStorageName,
               Error_getText(error)
              );
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(fileName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }

  printInfo(1,"  Convert file '%s'...",String_cString(fileName));

  // create new file entry
  error = Archive_newFileEntry(&destinationArchiveEntryInfo,
                               destinationArchiveHandle,
                               NULL,  // indexHandle,
                               fileName,
                               &fileInfo,
                               &fileExtendedAttributeList,
                               fragmentOffset,
                               fragmentSize,
FALSE,//                               tryDeltaCompressFlag,
FALSE//                               tryByteCompressFlag
                              );

  // convert archive and file content
  length = 0LL;
  while (length < fragmentSize)
  {
    bufferLength = (ulong)MIN(fragmentSize-length,bufferSize);

    // read source archive
    error = Archive_readData(&sourceArchiveEntryInfo,buffer,bufferLength);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL!\n");
      printError("Cannot read content of archive '%s' (error: %s)!\n",
                 printableStorageName,
                 Error_getText(error)
                );
      break;
    }
    DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

    // write data to destination archive
    error = Archive_writeData(&destinationArchiveEntryInfo,buffer,bufferLength,1);
    if (error != ERROR_NONE)
    {
      printInfo(0,"FAIL!\n");
      printError("Cannot write file '%s' (error: %s)!\n",
                 printableStorageName,
                 Error_getText(error)
                );
      break;
    }
    DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

    length += (uint64)bufferLength;

    printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
  }
  if (error != ERROR_NONE)
  {
    (void)Archive_closeEntry(&destinationArchiveEntryInfo);
    (void)Archive_closeEntry(&sourceArchiveEntryInfo);
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(fileName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }

  printInfo(2,"    \b\b\b\b");

  // close destination archive entry
  error = Archive_closeEntry(&destinationArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printInfo(1,"FAIL\n");
    printError("Cannot close archive file entry (error: %s)!\n",
               Error_getText(error)
              );
    (void)Archive_closeEntry(&sourceArchiveEntryInfo);
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(fileName);
    return error;
  }

  printInfo(1,"OK\n");

  /* check if all data read.
     Note: it is not possible to check if all data is read when
     compression is used. The decompressor may not be at the end
     of a compressed data chunk even compressed data is _not_
     corrupt.
  */
  if (   !Compress_isCompressed(deltaCompressAlgorithm)
      && !Compress_isCompressed(byteCompressAlgorithm)
      && !Archive_eofData(&sourceArchiveEntryInfo))
  {
    printWarning("unexpected data at end of file entry '%S'.\n",fileName);
  }

  // close source archive file
  error = Archive_closeEntry(&sourceArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'file' entry fail (error: %s)\n",Error_getText(error));
  }

  // free resources
  File_doneExtendedAttributes(&fileExtendedAttributeList);
  String_delete(fileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : convertImageEntry
* Purpose: convert a image entry in archive
* Input  : sourceArchiveHandle      - source archive handle
*          destinationArchiveHandle - destination archive handle
*          printableStorageName     - printable storage name
*          jobOptions               - job options
*          buffer0,buffer1          - buffers for temporary data
*          bufferSize               - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertImageEntry(ArchiveHandle    *sourceArchiveHandle,
                               ArchiveHandle    *destinationArchiveHandle,
                               const char       *printableStorageName,
                               const JobOptions *jobOptions,
                               byte             *buffer0,
                               byte             *buffer1,
                               uint             bufferSize
                              )
{
  Errors             error;
  ArchiveEntryInfo   archiveEntryInfo;
  CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
  String             deviceName;
  DeviceInfo         deviceInfo;
  uint64             blockOffset,blockCount;
  DeviceHandle       deviceHandle;
  bool               fileSystemFlag;
  FileSystemHandle   fileSystemHandle;
  bool               equalFlag;
  uint64             block;
  ulong              diffIndex;
  SemaphoreLock      semaphoreLock;

  // read image
  deviceName = String_new();
  error = Archive_readImageEntry(&archiveEntryInfo,
                                 sourceArchiveHandle,
                                 &deltaCompressAlgorithm,
                                 &byteCompressAlgorithm,
                                 NULL,  // cryptAlgorithm
                                 NULL,  // cryptType
                                 deviceName,
                                 &deviceInfo,
                                 NULL,  // fileSystemType
                                 NULL,  // deltaSourceName
                                 NULL,  // deltaSourceSize
                                 &blockOffset,
                                 &blockCount
                                );
  if (error != ERROR_NONE)
  {
    printError("Cannot read 'image' content of archive '%s' (error: %s)!\n",
               printableStorageName,
               Error_getText(error)
              );
    String_delete(deviceName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }
  if (deviceInfo.blockSize > bufferSize)
  {
    printError("Device block size %llu on '%s' is too big (max: %llu)\n",
               deviceInfo.blockSize,
               String_cString(deviceName),
               bufferSize
              );
    String_delete(deviceName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }
  assert(deviceInfo.blockSize > 0);

  printInfo(1,"  Convert image '%s'...",String_cString(deviceName));

  // check if device/image exists
  if (!File_exists(deviceName))
  {
    printInfo(1,"FAIL!\n");
    printError("Device '%s' not found!\n",String_cString(deviceName));
    (void)Archive_closeEntry(&archiveEntryInfo);
    String_delete(deviceName);
    return error;
  }

  // get device info
  error = Device_getDeviceInfo(&deviceInfo,deviceName);
  if (error != ERROR_NONE)
  {
    (void)Archive_closeEntry(&archiveEntryInfo);
    if (jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      String_delete(deviceName);
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot open device '%s' (error: %s)\n",
                 String_cString(deviceName),
                 Error_getText(error)
                );
      String_delete(deviceName);
      return error;
    }
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }

  // check device block size, get max. blocks in buffer
  if (deviceInfo.blockSize > bufferSize)
  {
    printInfo(1,"FAIL\n");
    printError("Device block size %llu on '%s' is too big (max: %llu)\n",
               deviceInfo.blockSize,
               String_cString(deviceName),
               bufferSize
              );
    (void)Archive_closeEntry(&archiveEntryInfo);
    String_delete(deviceName);
    return ERROR_INVALID_DEVICE_BLOCK_SIZE;
  }
  assert(deviceInfo.blockSize > 0);

  // open device
  error = Device_open(&deviceHandle,deviceName,DEVICE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    printInfo(1,"FAIL!\n");
    printError("Cannot open file '%s' (error: %s)\n",
               String_cString(deviceName),
               Error_getText(error)
              );
    (void)Archive_closeEntry(&archiveEntryInfo);
    String_delete(deviceName);
    return error;
  }
  DEBUG_TESTCODE() { Device_close(&deviceHandle); Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }

  // check image size
  if (deviceInfo.size != Device_getSize(&deviceHandle))
  {
    printInfo(1,"FAIL!\n");
    printError("'%s' differ in size: expected %lld bytes, found %lld bytes\n",
               String_cString(deviceName),
               deviceInfo.size,
               Device_getSize(&deviceHandle)
              );
    Device_close(&deviceHandle);
    (void)Archive_closeEntry(&archiveEntryInfo);
    String_delete(deviceName);
    return ERROR_ENTRIES_DIFFER;
  }

  // check if device contain a known file system or a raw image should be convertd
  if (!jobOptions->rawImagesFlag)
  {
    fileSystemFlag = (FileSystem_init(&fileSystemHandle,&deviceHandle) == ERROR_NONE);
  }
  else
  {
    fileSystemFlag = FALSE;
  }

  // seek to fragment position
  error = Device_seek(&deviceHandle,blockOffset*(uint64)deviceInfo.blockSize);
  if (error != ERROR_NONE)
  {
    printInfo(1,"FAIL!\n");
    printError("Cannot write to device '%s' (error: %s)\n",
               String_cString(deviceName),
               Error_getText(error)
              );
    Device_close(&deviceHandle);
    (void)Archive_closeEntry(&archiveEntryInfo);
    String_delete(deviceName);
    return error;
  }
  DEBUG_TESTCODE() { Device_close(&deviceHandle); Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }

  // convert archive and device/image content
  block     = 0LL;
  equalFlag = TRUE;
  diffIndex = 0L;
  while (   (block < blockCount)
         && equalFlag
        )
  {
    // read data from archive (only single block)
    error = Archive_readData(&archiveEntryInfo,buffer0,deviceInfo.blockSize);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL!\n");
      printError("Cannot read content of archive '%s' (error: %s)!\n",
                 printableStorageName,
                 Error_getText(error)
                );
      break;
    }
    DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

    if (   !fileSystemFlag
        || FileSystem_blockIsUsed(&fileSystemHandle,(blockOffset+block)*(uint64)deviceInfo.blockSize)
       )
    {
      // seek to device/image position
      error = Device_seek(&deviceHandle,(blockOffset+block)*(uint64)deviceInfo.blockSize);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("Cannot seek device '%s' (error: %s)\n",
                   String_cString(deviceName),
                   Error_getText(error)
                  );
        break;
      }
      DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

      // read data from device/image
      error = Device_read(&deviceHandle,buffer1,deviceInfo.blockSize,NULL);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("Cannot read device '%s' (error: %s)\n",
                   String_cString(deviceName),
                   Error_getText(error)
                  );
        break;
      }
      DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

//TODO
#if 0
      // compare
      diffIndex = compare(buffer0,buffer1,deviceInfo.blockSize);
      equalFlag = (diffIndex >= deviceInfo.blockSize);
      if (!equalFlag)
      {
        error = ERROR_ENTRIES_DIFFER;

        printInfo(1,"FAIL!\n");
        printError("'%s' differ at offset %llu\n",
                   String_cString(deviceName),
                   blockOffset*(uint64)deviceInfo.blockSize+block*(uint64)deviceInfo.blockSize+(uint64)diffIndex
                  );
        break;
      }
#endif
    }

    block += 1LL;

    printInfo(2,"%3d%%\b\b\b\b",(uint)((block*100LL)/blockCount));
  }
  if (error != ERROR_NONE)
  {
    if (fileSystemFlag) FileSystem_done(&fileSystemHandle);
    Device_close(&deviceHandle);
    (void)Archive_closeEntry(&archiveEntryInfo);
    String_delete(deviceName);
    return error;
  }
  DEBUG_TESTCODE() { if (fileSystemFlag) { FileSystem_done(&fileSystemHandle); } Device_close(&deviceHandle); Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }
  printInfo(2,"    \b\b\b\b");

  printInfo(1,"OK TODO\n");

  /* check if all data read.
     Note: it is not possible to check if all data is read when
     compression is used. The decompressor may not be at the end
     of a compressed data chunk even compressed data is _not_
     corrupt.
  */
  if (   !Compress_isCompressed(deltaCompressAlgorithm)
      && !Compress_isCompressed(byteCompressAlgorithm)
      && !Archive_eofData(&archiveEntryInfo))
  {
    printWarning("unexpected data at end of image entry '%S'.\n",deviceName);
  }

  // done file system
  if (fileSystemFlag)
  {
    FileSystem_done(&fileSystemHandle);
  }

  // close device
  Device_close(&deviceHandle);

  // close archive file, free resources
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'image' entry fail (error: %s)\n",Error_getText(error));
    // ignore error
  }

  // free resources
  String_delete(deviceName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : convertDirectoryEntry
* Purpose: convert a directory entry in archive
* Input  : sourceArchiveHandle      - source archive handle
*          destinationArchiveHandle - destination archive handle
*          printableStorageName     - printable storage name
*          jobOptions               - job options
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertDirectoryEntry(ArchiveHandle    *sourceArchiveHandle,
                                   ArchiveHandle    *destinationArchiveHandle,
                                   const char       *printableStorageName,
                                   const JobOptions *jobOptions
                                  )
{
  Errors                    error;
  String                    directoryName;
  ArchiveEntryInfo          sourceArchiveEntryInfo;
  FileInfo                  fileInfo;
  FileExtendedAttributeList fileExtendedAttributeList;
  ArchiveEntryInfo          destinationArchiveEntryInfo;

  UNUSED_VARIABLE(jobOptions);

  // read source directory entry
  directoryName = String_new();
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = Archive_readDirectoryEntry(&sourceArchiveEntryInfo,
                                     sourceArchiveHandle,
                                     NULL,  // cryptAlgorithm
                                     NULL,  // cryptType
                                     directoryName,
                                     &fileInfo,
                                     &fileExtendedAttributeList
                                    );
  if (error != ERROR_NONE)
  {
    printError("Cannot read 'directory' content of archive '%s' (error: %s)!\n",
               printableStorageName,
               Error_getText(error)
              );
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(directoryName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); String_delete(directoryName); return DEBUG_TESTCODE_ERROR(); }

  printInfo(1,"  Convert directory '%s'...",String_cString(directoryName));

  // create new directory entry
  error = Archive_newDirectoryEntry(&destinationArchiveEntryInfo,
                                    destinationArchiveHandle,
                                    NULL,  // indexHandle,
                                    directoryName,
                                    &fileInfo,
                                    &fileExtendedAttributeList
                                   );
  if (error != ERROR_NONE)
  {
    printError("Cannot create new archive directory entry '%s' (error: %s)\n",
               printableStorageName,
               Error_getText(error)
              );
    (void)Archive_closeEntry(&sourceArchiveEntryInfo);
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(directoryName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&destinationArchiveEntryInfo); String_delete(directoryName); return DEBUG_TESTCODE_ERROR(); }

  // close destination archive entry
  error = Archive_closeEntry(&destinationArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printInfo(1,"FAIL\n");
    printError("Cannot close archive directory entry (error: %s)!\n",
               Error_getText(error)
              );
    (void)Archive_closeEntry(&sourceArchiveEntryInfo);
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(directoryName);
    return error;
  }

  printInfo(1,"OK\n");

  // check if all data read
  if (!Archive_eofData(&sourceArchiveEntryInfo))
  {
    printWarning("unexpected data at end of directory entry '%S'.\n",directoryName);
  }

  // close source archive file
  error = Archive_closeEntry(&sourceArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'directory' entry fail (error: %s)\n",Error_getText(error));
  }

  // free resources
  File_doneExtendedAttributes(&fileExtendedAttributeList);
  String_delete(directoryName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : convertLinkEntry
* Purpose: convert a link entry in archive
* Input  : sourceArchiveHandle      - source archive handle
*          destinationArchiveHandle - destination archive handle
*          printableStorageName     - printable storage name
*          jobOptions               - job options
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertLinkEntry(ArchiveHandle    *sourceArchiveHandle,
                              ArchiveHandle    *destinationArchiveHandle,
                              const char       *printableStorageName,
                              const JobOptions *jobOptions
                             )
{
  Errors                    error;
  ArchiveEntryInfo          sourceArchiveEntryInfo;
  String                    linkName;
  String                    fileName;
  FileInfo                  fileInfo;
  FileExtendedAttributeList fileExtendedAttributeList;
  ArchiveEntryInfo          destinationArchiveEntryInfo;

  UNUSED_VARIABLE(jobOptions);

  // read link
  linkName = String_new();
  fileName = String_new();
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = Archive_readLinkEntry(&sourceArchiveEntryInfo,
                                sourceArchiveHandle,
                                NULL,  // cryptAlgorithm
                                NULL,  // cryptType
                                linkName,
                                fileName,
                                &fileInfo,
                                &fileExtendedAttributeList
                               );
  if (error != ERROR_NONE)
  {
    printError("Cannot read 'link' content of archive '%s' (error: %s)!\n",
               printableStorageName,
               Error_getText(error)
              );
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(fileName);
    String_delete(linkName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); String_delete(fileName); String_delete(linkName); return DEBUG_TESTCODE_ERROR(); }

  printInfo(1,"  Convert link '%s'...",String_cString(linkName));

  // create new link entry
  error = Archive_newLinkEntry(&destinationArchiveEntryInfo,
                               destinationArchiveHandle,
                               NULL,  // indexHandle,
                               linkName,
                               fileName,
                               &fileInfo,
                               &fileExtendedAttributeList
                              );
  if (error != ERROR_NONE)
  {
    printInfo(1,"FAIL\n");
    printError("Cannot create new archive link entry '%s' (error: %s)\n",
               String_cString(linkName),
               Error_getText(error)
              );
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(fileName);
    String_delete(linkName);
    return error;
  }

  // close destination archive entry
  error = Archive_closeEntry(&destinationArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printInfo(1,"FAIL\n");
    printError("Cannot close archive link entry (error: %s)!\n",
               Error_getText(error)
              );
    (void)Archive_closeEntry(&sourceArchiveEntryInfo);
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(fileName);
    String_delete(linkName);
    return error;
  }

  printInfo(1,"OK\n");

  // check if all data read
  if (!Archive_eofData(&sourceArchiveEntryInfo))
  {
    printWarning("unexpected data at end of link entry '%S'.\n",linkName);
  }

  // close source archive file
  error = Archive_closeEntry(&sourceArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'link' entry fail (error: %s)\n",Error_getText(error));
  }

  // free resources
  File_doneExtendedAttributes(&fileExtendedAttributeList);
  String_delete(fileName);
  String_delete(linkName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : convertHardLinkEntry
* Purpose: convert a hardlink entry in archive
* Input  : sourceArchiveHandle      - source archive handle
*          destinationArchiveHandle - destination archive handle
*          printableStorageName     - printable storage name
*          jobOptions               - job options
*          buffer0,buffer1          - buffers for temporary data
*          bufferSize               - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertHardLinkEntry(ArchiveHandle    *sourceArchiveHandle,
                                  ArchiveHandle    *destinationArchiveHandle,
                                  const char       *printableStorageName,
                                  const JobOptions *jobOptions,
                                  byte             *buffer0,
                                  byte             *buffer1,
                                  uint             bufferSize
                                 )
{
  Errors             error;
  ArchiveEntryInfo   archiveEntryInfo;
  CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
  StringList         fileNameList;
  FileInfo           fileInfo;
  FileExtendedAttributeList fileExtendedAttributeList;
  ArchiveEntryInfo          destinationArchiveEntryInfo;
  uint64             fragmentOffset,fragmentSize;
  bool               convertdDataFlag;
  const StringNode   *stringNode;
  String             fileName;
//            FileInfo         localFileInfo;
  FileHandle         fileHandle;
  bool               equalFlag;
  uint64             length;
  ulong              bufferLength;
  ulong              diffIndex;
  SemaphoreLock      semaphoreLock;

  // read hard link
  StringList_init(&fileNameList);
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                    sourceArchiveHandle,
                                    &deltaCompressAlgorithm,
                                    &byteCompressAlgorithm,
                                    NULL,  // cryptAlgorithm
                                    NULL,  // cryptType
                                    &fileNameList,
                                    &fileInfo,
                                    NULL,  // fileExtendedAttributeList
                                    NULL,  // deltaSourceName
                                    NULL,  // deltaSourceSize
                                    &fragmentOffset,
                                    &fragmentSize
                                   );
  if (error != ERROR_NONE)
  {
    printError("Cannot read 'hard link' content of archive '%s' (error: %s)!\n",
               printableStorageName,
               Error_getText(error)
              );
    StringList_done(&fileNameList);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); StringList_done(&fileNameList); return DEBUG_TESTCODE_ERROR(); }

  convertdDataFlag = FALSE;
  STRINGLIST_ITERATE(&fileNameList,stringNode,fileName)
  {
    printInfo(1,"  Convert hard link '%s'...",String_cString(fileName));

    // check file if exists and file type
    if (!File_exists(fileName))
    {
      printInfo(1,"FAIL!\n");
      printError("File '%s' not found!\n",String_cString(fileName));
      if (!jobOptions->noStopOnErrorFlag)
      {
        error = ERROR_FILE_NOT_FOUND_;
        break;
      }
      else
      {
        continue;
      }
    }
    if (File_getType(fileName) != FILE_TYPE_HARDLINK)
    {
      printInfo(1,"FAIL!\n");
      printError("'%s' is not a hard link!\n",String_cString(fileName));
      if (!jobOptions->noStopOnErrorFlag)
      {
        error = ERROR_WRONG_ENTRY_TYPE;
        break;
      }
      else
      {
        continue;
      }
    }

    if (!convertdDataFlag && (error == ERROR_NONE))
    {
      // convert hard link data

      // open file
      error = File_open(&fileHandle,fileName,FILE_OPEN_READ|FILE_OPEN_NO_ATIME|FILE_OPEN_NO_CACHE);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("Cannot open file '%s' (error: %s)\n",
                   String_cString(fileName),
                   Error_getText(error)
                  );
        if (!jobOptions->noStopOnErrorFlag)
        {
          break;
        }
        else
        {
          continue;
        }
      }
      DEBUG_TESTCODE() { (void)File_close(&fileHandle); error = DEBUG_TESTCODE_ERROR(); break; }

      // check file size
      if (fileInfo.size != File_getSize(&fileHandle))
      {
        printInfo(1,"FAIL!\n");
        printError("'%s' differ in size: expected %lld bytes, found %lld bytes\n",
                   String_cString(fileName),
                   fileInfo.size,
                   File_getSize(&fileHandle)
                  );
        File_close(&fileHandle);
        if (!jobOptions->noStopOnErrorFlag)
        {
          error = ERROR_ENTRIES_DIFFER;
          break;
        }
        else
        {
          continue;
        }
      }

      // seek to fragment position
      error = File_seek(&fileHandle,fragmentOffset);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("Cannot read file '%s' (error: %s)\n",
                   String_cString(fileName),
                   Error_getText(error)
                  );
        File_close(&fileHandle);
        if (!jobOptions->noStopOnErrorFlag)
        {
          break;
        }
        else
        {
          continue;
        }
      }
      DEBUG_TESTCODE() { (void)File_close(&fileHandle); error = DEBUG_TESTCODE_ERROR(); break; }

      // convert archive and hard link content
      length    = 0LL;
      equalFlag = TRUE;
      diffIndex = 0L;
      while (   (length < fragmentSize)
             && equalFlag
            )
      {
        bufferLength = (ulong)MIN(fragmentSize-length,bufferSize);

        // read archive, file
        error = Archive_readData(&archiveEntryInfo,buffer0,bufferLength);
        if (error != ERROR_NONE)
        {
          printInfo(1,"FAIL!\n");
          printError("Cannot read content of archive '%s' (error: %s)!\n",
                     printableStorageName,
                     Error_getText(error)
                    );
          break;
        }
        DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }
        error = File_read(&fileHandle,buffer1,bufferLength,NULL);
        if (error != ERROR_NONE)
        {
          printInfo(1,"FAIL!\n");
          printError("Cannot read file '%s' (error: %s)\n",
                     String_cString(fileName),
                     Error_getText(error)
                    );
          break;
        }
        DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

//TODO
#if 0
        // compare
        diffIndex = compare(buffer0,buffer1,bufferLength);
        equalFlag = (diffIndex >= bufferLength);
        if (!equalFlag)
        {
          error = ERROR_ENTRIES_DIFFER;

          printInfo(1,"FAIL!\n");
          printError("'%s' differ at offset %llu\n",
                     String_cString(fileName),
                     fragmentOffset+length+(uint64)diffIndex
                    );
          break;
        }
#endif

        length += (uint64)bufferLength;

        printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
      }
      if (error != ERROR_NONE)
      {
        (void)File_close(&fileHandle);
        return error;
      }
      printInfo(2,"    \b\b\b\b");

      // close file
      (void)File_close(&fileHandle);

      printInfo(1,"OK TODO\n");

      /* check if all data read.
         Note: it is not possible to check if all data is read when
         compression is used. The decompressor may not be at the end
         of a compressed data chunk even compressed data is _not_
         corrupt.
      */
      if (   !Compress_isCompressed(deltaCompressAlgorithm)
          && !Compress_isCompressed(byteCompressAlgorithm)
          && !Archive_eofData(&archiveEntryInfo))
      {
        printWarning("unexpected data at end of hard link entry '%S'.\n",fileName);
      }

      convertdDataFlag = TRUE;
    }
    else
    {
      // convert hard link data already done
      if (error == ERROR_NONE)
      {
        printInfo(1,"OK TODO\n");
      }
      else
      {
        printInfo(1,"FAIL!\n");
      }
    }
  }

  // close archive file, free resources
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'hard link' entry fail (error: %s)\n",Error_getText(error));
  }

  // free resources
  File_doneExtendedAttributes(&fileExtendedAttributeList);
  StringList_done(&fileNameList);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : convertSpecialEntry
* Purpose: convert a special entry in archive
* Input  : sourceArchiveHandle      - source archive handle
*          destinationArchiveHandle - destination archive handle
*          printableStorageName     - printable storage name
*          jobOptions               - job options
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertSpecialEntry(ArchiveHandle    *sourceArchiveHandle,
                                ArchiveHandle    *destinationArchiveHandle,
                                 const char       *printableStorageName,
                                 const JobOptions *jobOptions
                                )
{
  Errors           error;
  ArchiveEntryInfo archiveEntryInfo;
  String           fileName;
  FileInfo         fileInfo;
  FileInfo         localFileInfo;

  UNUSED_VARIABLE(jobOptions);

  // read special
  fileName = String_new();
  error = Archive_readSpecialEntry(&archiveEntryInfo,
                                   sourceArchiveHandle,
                                   NULL,  // cryptAlgorithm
                                   NULL,  // cryptType
                                   fileName,
                                   &fileInfo,
                                   NULL   // fileExtendedAttributeList
                                  );
  if (error != ERROR_NONE)
  {
    printError("Cannot read 'special' content of archive '%s' (error: %s)!\n",
               printableStorageName,
               Error_getText(error)
              );
    String_delete(fileName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }

  printInfo(1,"  Convert special device '%s'...",String_cString(fileName));

  // check if file exists and file type
  if (!File_exists(fileName))
  {
    printInfo(1,"FAIL!\n");
    printError("Special device '%s' does not exists!\n",
               String_cString(fileName)
              );
    (void)Archive_closeEntry(&archiveEntryInfo);
    String_delete(fileName);
    return error;
  }
  if (File_getType(fileName) != FILE_TYPE_SPECIAL)
  {
    printInfo(1,"FAIL!\n");
    printError("'%s' is not a special device!\n",
               String_cString(fileName)
              );
    (void)Archive_closeEntry(&archiveEntryInfo);
    String_delete(fileName);
    return error;
  }

  // check special settings
  error = File_getFileInfo(fileName,&localFileInfo);
  if (error != ERROR_NONE)
  {
    printError("Cannot read local file '%s' (error: %s)!\n",
               String_cString(fileName),
               Error_getText(error)
              );
    (void)Archive_closeEntry(&archiveEntryInfo);
    String_delete(fileName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }
  if (fileInfo.specialType != localFileInfo.specialType)
  {
    printError("Different types of special device '%s'!\n",
               String_cString(fileName)
              );
    (void)Archive_closeEntry(&archiveEntryInfo);
    String_delete(fileName);
    return error;
  }
  if (   (fileInfo.specialType == FILE_SPECIAL_TYPE_CHARACTER_DEVICE)
      || (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
     )
  {
    if (fileInfo.major != localFileInfo.major)
    {
      printError("Different major numbers of special device '%s'!\n",
                 String_cString(fileName)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return error;
    }
    if (fileInfo.minor != localFileInfo.minor)
    {
      printError("Different minor numbers of special device '%s'!\n",
                 String_cString(fileName)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return error;
    }
  }

#if 0

  // check file time, permissions, file owner/group
#endif /* 0 */

  printInfo(1,"OK TODO\n");

  // check if all data read
  if (!Archive_eofData(&archiveEntryInfo))
  {
    printWarning("unexpected data at end of special entry '%S'.\n",fileName);
  }

  // close archive file
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'special' entry fail (error: %s)\n",Error_getText(error));
  }

  // free resources
  String_delete(fileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : convertThreadCode
* Purpose: convert worker thread
* Input  : convertInfo - convert info structure
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void convertThreadCode(ConvertInfo *convertInfo)
{
  String        printableStorageName;
  byte          *buffer0,*buffer1;
  ArchiveHandle sourceArchiveHandle;
  EntryMsg      entryMsg;
  Errors        error;

  assert(convertInfo != NULL);

  // init variables
  printableStorageName = String_new();
  buffer0 = (byte*)malloc(BUFFER_SIZE);
  if (buffer0 == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  buffer1 = (byte*)malloc(BUFFER_SIZE);
  if (buffer1 == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // get printable storage name
  Storage_getPrintableName(printableStorageName,convertInfo->storageSpecifier,NULL);

  // convert entries
  while (   (convertInfo->failError == ERROR_NONE)
//TODO
//         && !isAborted(convertInfo)
         && MsgQueue_get(&convertInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER)
        )
  {
//fprintf(stderr,"%s, %d: %p %d %llu\n",__FILE__,__LINE__,pthread_self(),entryMsg.archiveEntryType,entryMsg.offset);
//TODO: open only when changed
    // open archive
    error = Archive_open(&sourceArchiveHandle,
                         entryMsg.storageInfo,
                         NULL,  // fileName,
NULL,//                         convertInfo->deltaSourceList,
                         convertInfo->getPasswordFunction,
                         convertInfo->getPasswordUserData,
                         convertInfo->logHandle
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot open storage '%s' (error: %s)!\n",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      if (convertInfo->failError == ERROR_NONE) convertInfo->failError = error;
      break;
    }

    // set crypt salt and crypt mode
    Archive_setCryptSalt(&sourceArchiveHandle,entryMsg.cryptSalt,sizeof(entryMsg.cryptSalt));
    Archive_setCryptMode(&sourceArchiveHandle,entryMsg.cryptMode);

    // seek to start of entry
    error = Archive_seek(&sourceArchiveHandle,entryMsg.offset);
    if (error != ERROR_NONE)
    {
      printError("Cannot read storage '%s' (error: %s)!\n",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      if (convertInfo->failError == ERROR_NONE) convertInfo->failError = error;
      break;
    }

    switch (entryMsg.archiveEntryType)
    {
      case ARCHIVE_ENTRY_TYPE_FILE:
        error = convertFileEntry(&sourceArchiveHandle,
                                 convertInfo->destinationArchiveHandle,
                                 String_cString(printableStorageName),
                                 convertInfo->jobOptions,
                                 buffer0,
                                 BUFFER_SIZE
                                );
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        error = convertImageEntry(&sourceArchiveHandle,
                                  convertInfo->destinationArchiveHandle,
                                  String_cString(printableStorageName),
                                  convertInfo->jobOptions,
                                  buffer0,
                                  buffer1,
                                  BUFFER_SIZE
                                 );
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        error = convertDirectoryEntry(&sourceArchiveHandle,
                                      convertInfo->destinationArchiveHandle,
                                      String_cString(printableStorageName),
                                      convertInfo->jobOptions
                                     );
        break;
      case ARCHIVE_ENTRY_TYPE_LINK:
        error = convertLinkEntry(&sourceArchiveHandle,
                                 convertInfo->destinationArchiveHandle,
                                 String_cString(printableStorageName),
                                 convertInfo->jobOptions
                                );
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        error = convertHardLinkEntry(&sourceArchiveHandle,
                                     convertInfo->destinationArchiveHandle,
                                     String_cString(printableStorageName),
                                     convertInfo->jobOptions,
                                     buffer0,
                                     buffer1,
                                     BUFFER_SIZE
                                    );
        break;
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
        error = convertSpecialEntry(&sourceArchiveHandle,
                                      convertInfo->destinationArchiveHandle,
                                    String_cString(printableStorageName),
                                    convertInfo->jobOptions
                                   );
        break;
      case ARCHIVE_ENTRY_TYPE_META:
        error = Archive_skipNextEntry(&sourceArchiveHandle);
        break;
      case ARCHIVE_ENTRY_TYPE_SIGNATURE:
        error = Archive_skipNextEntry(&sourceArchiveHandle);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; /* not reached */
    }
    if (error != ERROR_NONE)
    {
      if (!convertInfo->jobOptions->noStopOnErrorFlag)
      {
        if (convertInfo->failError == ERROR_NONE) convertInfo->failError = error;
      }
    }

    // close archive
    Archive_close(&sourceArchiveHandle);

    freeEntryMsg(&entryMsg,NULL);
  }
  if (!isPrintInfo(1)) printInfo(0,"%s",(convertInfo->failError == ERROR_NONE) ? "OK\n" : "FAIL!\n");

  // free resources
  free(buffer1);
  free(buffer0);
  String_delete(printableStorageName);
}

/***********************************************************************\
* Name   : convertArchive
* Purpose: convert archive
* Input  : storageSpecifier    - storage specifier
*          archiveName         - archive name (can be NULL)
*          jobOptions          - job options
*          getPasswordFunction - get password call back
*          getPasswordUserData - user data for get password
*          logHandle           - log handle (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors convertArchive(StorageSpecifier    *storageSpecifier,
                            ConstString         archiveName,
                            JobOptions          *jobOptions,
                            GetPasswordFunction getPasswordFunction,
                            void                *getPasswordUserData,
                            LogHandle           *logHandle
                           )
{
  AutoFreeList         autoFreeList;
  String               printableStorageName;
  String               tmpArchiveName;
  Thread               storageThread;
  Thread               *convertThreads;
  uint                 convertThreadCount;
  Errors               error;
  CryptSignatureStates allCryptSignatureState;
  ConvertInfo          convertInfo;
  uint                 i;
  Errors               failError;
  ArchiveHandle        sourceArchiveHandle,destinationArchiveHandle;
  ArchiveEntryTypes    archiveEntryType;
  uint64               offset;
  EntryMsg             entryMsg;

  assert(storageSpecifier != NULL);
  assert(jobOptions != NULL);

  // init variables
  AutoFree_init(&autoFreeList);
  printableStorageName = String_new();
  tmpArchiveName       = String_new();
  convertThreadCount   = (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores();
  convertThreads       = (Thread*)malloc(convertThreadCount*sizeof(Thread));
  if (convertThreads == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,printableStorageName,{ String_delete(printableStorageName); });
  AUTOFREE_ADD(&autoFreeList,tmpArchiveName,{ String_delete(tmpArchiveName); });
  AUTOFREE_ADD(&autoFreeList,convertThreads,{ free(convertThreads); });

  // init convert info
  initConvertInfo(&convertInfo,
                  storageSpecifier,
                  &destinationArchiveHandle,
                  jobOptions,
//TODO
NULL,  //               pauseTestFlag,
NULL,  //               requestedAbortFlag,
                  logHandle
                 );

  // get printable storage name
  Storage_getPrintableName(printableStorageName,storageSpecifier,archiveName);

  // init storage
  error = Storage_init(&convertInfo.storageInfo,
                       NULL,  // masterSocketHandle
                       storageSpecifier,
                       jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK(NULL,NULL),  // updateStatusInfo
                       CALLBACK(NULL,NULL),  // getPassword
                       CALLBACK(NULL,NULL)  // requestVolume
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize storage '%s' (error: %s)!\n",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&convertInfo.storageInfo,{ (void)Storage_done(&convertInfo.storageInfo); });

  // check signatures
  if (!jobOptions->skipVerifySignaturesFlag)
  {
    error = Archive_verifySignatures(&convertInfo.storageInfo,
                                     archiveName,
                                     jobOptions,
                                     &allCryptSignatureState
                                    );
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    if (!Crypt_isValidSignatureState(allCryptSignatureState))
    {
      printError("Invalid signature in '%s'!\n",
                 String_cString(printableStorageName)
                );
      AutoFree_cleanup(&autoFreeList);
      return ERROR_INVALID_SIGNATURE;
    }
  }

  // open source archive
  error = Archive_open(&sourceArchiveHandle,
                       &convertInfo.storageInfo,
                       archiveName,
                       NULL,  // deltaSourceList,
                       getPasswordFunction,
                       getPasswordUserData,
                       logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot open storage '%s' (error: %s)!\n",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Archive_close(&sourceArchiveHandle); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&sourceArchiveHandle,{ Archive_close(&sourceArchiveHandle); });

  // create destination archive
  error = File_getTmpFileName(tmpArchiveName,String_cString(archiveName),NULL);
  if (error != ERROR_NONE)
  {
    printError("Cannot create temporary file (error: %s)!\n",
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&tmpArchiveName,{ (void)File_delete(tmpArchiveName,FALSE); });
  error = Archive_create(&destinationArchiveHandle,
                         &convertInfo.storageInfo,
                         tmpArchiveName,
NULL,//                         indexHandle,
INDEX_ID_NONE,//                         uuidId,
INDEX_ID_NONE,//                         entityId,
NULL,//                         jobUUID,
NULL,//                         scheduleUUID,
NULL,//                         deltaSourceList,
ARCHIVE_TYPE_NONE,//                         archiveType,
                         CALLBACK(NULL,NULL),  // archiveInitFunction
                         CALLBACK(NULL,NULL),  // archiveDoneFunction
CALLBACK(NULL,NULL),//                         CALLBACK(archiveGetSize,&createInfo),
                         CALLBACK(archiveStore,&convertInfo),
                         CALLBACK(getPasswordFunction,getPasswordUserData),
                         logHandle
                        );
  if (error != ERROR_NONE)
  {
    printError("Cannot create temporary storage for '%s' (error: %s)!\n",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Archive_close(&destinationArchiveHandle); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&destinationArchiveHandle,{ Archive_close(&destinationArchiveHandle); });

  // start storage thread
  if (!Thread_init(&storageThread,"BAR storage",globalOptions.niceLevel,storageThreadCode,&convertInfo))
  {
    HALT_FATAL_ERROR("Cannot initialize storage thread!");
  }
  AUTOFREE_ADD(&autoFreeList,&storageThread,{ MsgQueue_setEndOfMsg(&convertInfo.storageMsgQueue); Thread_join(&storageThread); Thread_done(&storageThread); });

  // start convert threads
  for (i = 0; i < convertThreadCount; i++)
  {
    if (!Thread_init(&convertThreads[i],"BAR convert",globalOptions.niceLevel,convertThreadCode,&convertInfo))
    {
      HALT_FATAL_ERROR("Cannot initialize convertthread #%d!",i);
    }
    AUTOFREE_ADD(&autoFreeList,&convertThreads[i],{ MsgQueue_setEndOfMsg(&convertInfo.entryMsgQueue); Thread_join(&convertThreads[i]); Thread_done(&convertThreads[i]); });
  }

  // read archive entries
  printInfo(0,
            "Convert storage '%s'%s",
            String_cString(printableStorageName),
            !isPrintInfo(1) ? "..." : ":\n"
           );
  failError = ERROR_NONE;
  while (   !Archive_eof(&sourceArchiveHandle,TRUE,isPrintInfo(3))
         && (failError == ERROR_NONE)
        )
  {
    // get next archive entry type
    error = Archive_getNextArchiveEntry(&sourceArchiveHandle,
                                        &archiveEntryType,
                                        &offset,
                                        TRUE,
                                        isPrintInfo(3)
                                       );
    if (error != ERROR_NONE)
    {
      printError("Cannot read next entry in archive '%s' (error: %s)!\n",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      break;
    }
    DEBUG_TESTCODE() { failError = DEBUG_TESTCODE_ERROR(); break; }

    // send entry to test threads
    entryMsg.storageInfo      = &convertInfo.storageInfo;
    memCopyFast(entryMsg.cryptSalt,sizeof(entryMsg.cryptSalt),sourceArchiveHandle.cryptSalt,sizeof(sourceArchiveHandle.cryptSalt));
    entryMsg.cryptMode        = sourceArchiveHandle.cryptMode;
    entryMsg.archiveEntryType = archiveEntryType;
    entryMsg.offset           = offset;
    if (!MsgQueue_put(&convertInfo.entryMsgQueue,&entryMsg,sizeof(entryMsg)))
    {
      HALT_INTERNAL_ERROR("Send message to convert threads fail!");
    }

    // next entry
    error = Archive_skipNextEntry(&sourceArchiveHandle);
    if (error != ERROR_NONE)
    {
      if (failError == ERROR_NONE) failError = error;
      break;
    }
  }
  if (!isPrintInfo(1)) printInfo(0,"%s",(failError == ERROR_NONE) ? "OK\n" : "FAIL!\n");

  // close source archive
  AUTOFREE_REMOVE(&autoFreeList,&sourceArchiveHandle);
  (void)Archive_close(&sourceArchiveHandle);

  // wait for convert threads
  MsgQueue_setEndOfMsg(&convertInfo.entryMsgQueue);
  for (i = 0; i < convertThreadCount; i++)
  {
    AUTOFREE_REMOVE(&autoFreeList,&convertThreads[i]);
    if (!Thread_join(&convertThreads[i]))
    {
      HALT_INTERNAL_ERROR("Cannot stop convert thread #%d!",i);
    }
    Thread_done(&convertThreads[i]);
  }

  // close destination archive
  AUTOFREE_REMOVE(&autoFreeList,&destinationArchiveHandle);
  error = Archive_close(&destinationArchiveHandle);
  if (error != ERROR_NONE)
  {
    printError("Cannot close archive '%s' (error: %s)\n",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  // wait for storage thread
  AUTOFREE_REMOVE(&autoFreeList,&storageThread);
  MsgQueue_setEndOfMsg(&convertInfo.storageMsgQueue);
  if (!Thread_join(&storageThread))
  {
    HALT_INTERNAL_ERROR("Cannot stop storage thread!");
  }
  Thread_done(&storageThread);

  // done storage
  (void)Storage_done(&convertInfo.storageInfo);

  // done convert info
  doneConvertInfo(&convertInfo);

  // free resources
  free(convertThreads);
  String_delete(tmpArchiveName);
  String_delete(printableStorageName);
  AutoFree_done(&autoFreeList);

  return error;
}

/*---------------------------------------------------------------------*/

Errors Command_convert(const StringList    *storageNameList,
                       JobOptions          *jobOptions,
                       GetPasswordFunction getPasswordFunction,
                       void                *getPasswordUserData,
                       LogHandle           *logHandle
                      )
{
  StorageSpecifier           storageSpecifier;
  StringNode                 *stringNode;
  String                     storageName;
  Errors                     failError;
  Errors                     error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  Pattern                    pattern;
  String                     fileName;

  assert(storageNameList != NULL);
  assert(jobOptions != NULL);

  // init variables
  Storage_initSpecifier(&storageSpecifier);

  failError = ERROR_NONE;
  STRINGLIST_ITERATE(storageNameList,stringNode,storageName)
  {
    // parse storage name
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error != ERROR_NONE)
    {
      printError("Invalid storage '%s' (error: %s)!\n",
                 String_cString(storageName),
                 Error_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      continue;
    }
    DEBUG_TESTCODE() { failError = DEBUG_TESTCODE_ERROR(); break; }

    if (String_isEmpty(storageSpecifier.archivePatternString))
    {
      // convert archive content
      error = convertArchive(&storageSpecifier,
                             NULL,
                             jobOptions,
                             getPasswordFunction,
                             getPasswordUserData,
                             logHandle
                            );
    }
    else
    {
      error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                        &storageSpecifier,
                                        NULL,  // pathName
                                        jobOptions,
                                        SERVER_CONNECTION_PRIORITY_HIGH
                                       );
      if (error == ERROR_NONE)
      {
        error = Pattern_init(&pattern,storageSpecifier.archivePatternString,
                             jobOptions->patternType,
                             PATTERN_FLAG_NONE
                            );
        if (error == ERROR_NONE)
        {
          fileName = String_new();
          while (!Storage_endOfDirectoryList(&storageDirectoryListHandle) && (error == ERROR_NONE))
          {
            // read next directory entry
            error = Storage_readDirectoryList(&storageDirectoryListHandle,fileName,NULL);
            if (error != ERROR_NONE)
            {
              continue;
            }

            // match pattern
            if (!Pattern_match(&pattern,fileName,PATTERN_MATCH_MODE_EXACT))
            {
              continue;
            }

            // convert archive content
            error = convertArchive(&storageSpecifier,
                                   fileName,
                                   jobOptions,
                                   getPasswordFunction,
                                   getPasswordUserData,
                                   logHandle
                                  );
          }
          String_delete(fileName);
          Pattern_done(&pattern);
        }

        Storage_closeDirectoryList(&storageDirectoryListHandle);
      }
    }
    if (error != ERROR_NONE)
    {
      if (failError == ERROR_NONE) failError = error;
      continue;
    }

    if (failError != ERROR_NONE) break;
  }

  // free resources
  Storage_doneSpecifier(&storageSpecifier);

  return failError;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
