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
  StorageInfo         storageInfo;
  const JobOptions    *jobOptions;
  GetPasswordFunction getPasswordFunction;
  void                *getPasswordUserData;
  LogHandle           *logHandle;                         // log handle

//TODO: used?
  bool                *pauseTestFlag;                     // TRUE for pause creation
  bool                *requestedAbortFlag;                // TRUE to abort create

  String              archiveName;                        // archive name (converted archive)
  String              newArchiveName;                     // temporary new archive name (converted archive)
  ArchiveHandle       destinationArchiveHandle;

  MsgQueue            entryMsgQueue;                      // queue with entries to convert
  MsgQueue            storageMsgQueue;                    // queue with waiting storage files
  bool                storageThreadExitFlag;

  Errors              failError;                          // failure error
} ConvertInfo;

// entry message send to convert threads
typedef struct
{
  StorageInfo            *storageInfo;
  ArchiveEntryTypes      archiveEntryType;
  const ArchiveCryptInfo *archiveCryptInfo;
  uint64                 offset;
} EntryMsg;

// storage message, send from convert threads -> storage thread
typedef struct
{
  ArchiveTypes archiveType;
  String       fileName;                                          // intermediate archive file name
  uint64       fileSize;                                          // intermediate archive size [bytes]
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

  String_delete(storageMsg->fileName);
}

/***********************************************************************\
* Name   : initConvertInfo
* Purpose: initialize convert info
* Input  : convertInfo         - convert info variable
*          jobOptions          - job options
*          getPasswordFunction - get password call back
*          getPasswordUserData - user data for get password call back
*          pauseTestFlag       - pause creation flag (can be NULL)
*          requestedAbortFlag  - request abort flag (can be NULL)
*          logHandle           - log handle (can be NULL)
* Output : convertInfo - initialized convert info variable
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initConvertInfo(ConvertInfo         *convertInfo,
                           const JobOptions    *jobOptions,
                           GetPasswordFunction getPasswordFunction,
                           void                *getPasswordUserData,
                           bool                *pauseTestFlag,
                           bool                *requestedAbortFlag,
                           LogHandle           *logHandle
                          )
{
  assert(convertInfo != NULL);

  // init variables
  convertInfo->jobOptions            = jobOptions;
  convertInfo->getPasswordFunction   = getPasswordFunction;
  convertInfo->getPasswordUserData   = getPasswordUserData;
  convertInfo->logHandle             = logHandle;
  convertInfo->pauseTestFlag         = pauseTestFlag;
  convertInfo->requestedAbortFlag    = requestedAbortFlag;
  convertInfo->archiveName           = String_new();
  convertInfo->newArchiveName        = String_new();
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

  MsgQueue_done(&convertInfo->storageMsgQueue,(MsgQueueMsgFreeFunction)freeStorageMsg,NULL);
  MsgQueue_done(&convertInfo->entryMsgQueue,(MsgQueueMsgFreeFunction)freeEntryMsg,NULL);
  String_delete(convertInfo->newArchiveName);
  String_delete(convertInfo->archiveName);
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
  ConvertInfo *convertInfo = (ConvertInfo*)userData;
  Errors      error;
  FileInfo    fileInfo;
  StorageMsg  storageMsg;

  assert(storageInfo != NULL);
  assert(!String_isEmpty(intermediateFileName));
  assert(convertInfo != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(uuidId);
  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(jobUUID);
  UNUSED_VARIABLE(scheduleUUID);
  UNUSED_VARIABLE(entityId);
  UNUSED_VARIABLE(storageId);
  UNUSED_VARIABLE(partNumber);

  // get file info
// TODO replace by getFileSize()
  error = File_getFileInfo(intermediateFileName,&fileInfo);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // send to storage thread
  storageMsg.archiveType = archiveType;
  storageMsg.fileName    = String_duplicate(intermediateFileName);
  storageMsg.fileSize    = intermediateFileSize;
  DEBUG_TESTCODE() { freeStorageMsg(&storageMsg,NULL); return DEBUG_TESTCODE_ERROR(); }
  if (!MsgQueue_put(&convertInfo->storageMsgQueue,&storageMsg,sizeof(storageMsg)))
  {
    freeStorageMsg(&storageMsg,NULL);
  }

//TODO
#if 0
  // wait for space in temporary directory
  if (globalOptions.maxTmpSize > 0)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&convertInfo->storageInfoLock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
    {
      while (   (convertInfo->storage.count > 2)                           // more than 2 archives are waiting
             && (convertInfo->storage.bytes > globalOptions.maxTmpSize)    // temporary space limit exceeded
             && (convertInfo->failError == ERROR_NONE)
             && !isAborted(convertInfo)
            )
      {
        Semaphore_waitModified(&convertInfo->storageInfoLock,30*1000);
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
* Input  : convertInfo - convert info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void storageThreadCode(ConvertInfo *convertInfo)
{
  #define MAX_RETRIES 3

  AutoFreeList     autoFreeList;
  byte             *buffer;
  String           printableStorageName;
  String           tmpArchiveName;
  void             *autoFreeSavePoint;
  StorageMsg       storageMsg;
  Errors           error;
  FileInfo         fileInfo;
  FileHandle       fileHandle;
  uint             retryCount;
  FileHandle       toFileHandle;
  StorageHandle    storageHandle;
  ulong            bufferLength;

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
  tmpArchiveName       = String_new();
  AUTOFREE_ADD(&autoFreeList,buffer,{ free(buffer); });
  AUTOFREE_ADD(&autoFreeList,printableStorageName,{ String_delete(printableStorageName); });
  AUTOFREE_ADD(&autoFreeList,tmpArchiveName,{ String_delete(tmpArchiveName); });

  // store archives
  while (   (convertInfo->failError == ERROR_NONE)
//         && !isAborted(convertInfo)
        )
  {
    autoFreeSavePoint = AutoFree_save(&autoFreeList);

    // pause
//    pauseStorage(convertInfo);
//    if (isAborted(convertInfo)) break;

    // get next archive to store
    if (!MsgQueue_get(&convertInfo->storageMsgQueue,&storageMsg,NULL,sizeof(storageMsg),WAIT_FOREVER))
    {
      break;
    }
    AUTOFREE_ADD(&autoFreeList,&storageMsg,
                 {
                   File_delete(storageMsg.fileName,FALSE);
                   freeStorageMsg(&storageMsg,NULL);
                 }
                );

    // get printable storage name
    if (!String_isEmpty(convertInfo->jobOptions->destination))
    {
      Storage_getPrintableName(printableStorageName,&convertInfo->storageInfo.storageSpecifier,convertInfo->archiveName);
    }
    else
    {
      Storage_getPrintableName(printableStorageName,&convertInfo->storageInfo.storageSpecifier,NULL);
    }

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

    // save original storage file
    Storage_getTmpName(tmpArchiveName,&convertInfo->storageInfo);
    error = Storage_rename(&convertInfo->storageInfo,convertInfo->newArchiveName,tmpArchiveName);
    if (error != ERROR_NONE)
    {
      printInfo(0,"FAIL!\n");
      printError("Cannot store '%s' (error: %s)\n",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      convertInfo->failError = error;
      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }
    AUTOFREE_ADD(&autoFreeList,&tmpArchiveName,{ Storage_rename(&convertInfo->storageInfo,tmpArchiveName,convertInfo->newArchiveName); });

    // create storage file
    if (!String_isEmpty(convertInfo->jobOptions->destination))
    {
//TODO: remove?
      assert(convertInfo->destinationArchiveHandle.mode == ARCHIVE_MODE_CREATE);

      // create local file
      error = File_open(&toFileHandle,convertInfo->newArchiveName,FILE_OPEN_CREATE);
      if (error != ERROR_NONE)
      {
        printInfo(0,"FAIL!\n");
        printError("Cannot store '%s' (error: %s)\n",
                   String_cString(convertInfo->destinationArchiveHandle.archiveName),
                   Error_getText(error)
                  );
        break;
      }
      DEBUG_TESTCODE() { Storage_close(&storageHandle); error = DEBUG_TESTCODE_ERROR(); break; }
      AUTOFREE_ADD(&autoFreeList,&toFileHandle,{ File_close(&toFileHandle); });

      // transfer data
      File_seek(&fileHandle,0);
      do
      {
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

        // store data into local file
        error = File_write(&toFileHandle,buffer,bufferLength);
        if (error != ERROR_NONE)
        {
          printInfo(0,"FAIL!\n");
          printError("Cannot write file '%s' (error: %s)!\n",
                     String_cString(printableStorageName),
                     Error_getText(error)
                    );
          break;
        }
        DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }
      }
      while (   (convertInfo->failError == ERROR_NONE)
//             && !isAborted(convertInfo)
             && !File_eof(&fileHandle)
            );

      // close local file
      File_close(&toFileHandle);
      AUTOFREE_REMOVE(&autoFreeList,&toFileHandle);
    }
    else
    {
      retryCount = 0;
      do
      {
        // next try
        if (retryCount > MAX_RETRIES)
        {
          break;
        }
        retryCount++;

        // pause
//        pauseStorage(convertInfo);
//        if (isAborted(convertInfo)) break;

        // create storage file
        error = Storage_create(&storageHandle,
                               &convertInfo->storageInfo,
                               convertInfo->newArchiveName,
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
        AUTOFREE_ADD(&autoFreeList,&storageHandle,{ Storage_close(&storageHandle); Storage_delete(&convertInfo->storageInfo,convertInfo->newArchiveName); });

        // copy data
        File_seek(&fileHandle,0);
        do
        {
          // pause
//          pauseStorage(convertInfo);
//          if (isAborted(convertInfo)) break;

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

          // store data into storage
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
  //             && !isAborted(convertInfo)
               && !File_eof(&fileHandle)
              );

        // close local file
        Storage_close(&storageHandle);
        AUTOFREE_REMOVE(&autoFreeList,&storageHandle);

        DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); error = DEBUG_TESTCODE_ERROR(); }
      }
      while (   (convertInfo->failError == ERROR_NONE)                            // no eror
  //           && !isAborted(convertInfo)                                           // not aborted
             && ((error != ERROR_NONE) && (Error_getCode(error) != ENOSPC))      // some error amd not "no space left"
             && (retryCount <= MAX_RETRIES)                                      // still some retry left
            );
    }
    if (error != ERROR_NONE)
    {
      if (convertInfo->failError == ERROR_NONE) convertInfo->failError = error;
      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }
    AUTOFREE_ADD(&autoFreeList,convertInfo->newArchiveName,{ Storage_delete(&convertInfo->storageInfo,convertInfo->newArchiveName); });

    // delete saved original storage file
    error = Storage_delete(&convertInfo->storageInfo,tmpArchiveName);
    if (error != ERROR_NONE)
    {
      printInfo(0,"FAIL!\n");
      printError("Cannot store '%s' (error: %s)\n",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      convertInfo->failError = error;
      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }
    AUTOFREE_REMOVE(&autoFreeList,&tmpArchiveName);

    // close file
    File_close(&fileHandle);
    AUTOFREE_REMOVE(&autoFreeList,&fileHandle);

#if 0
    // check if aborted
    if (isAborted(convertInfo))
    {
      printInfo(1,"ABORTED\n");
      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }
#endif

    // rename storage file
    error = Storage_rename(&convertInfo->storageInfo,
                           convertInfo->newArchiveName,
                           convertInfo->archiveName
                          );
    if (error != ERROR_NONE)
    {
      printInfo(0,"FAIL!\n");
      printError("Cannot rename temporary file to '%s' (error: %s)!\n",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      if (convertInfo->failError == ERROR_NONE) convertInfo->failError = error;
      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }

    // done
    printInfo(1,"OK\n");

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
  String_delete(tmpArchiveName);
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
*          buffer                   - buffer for temporary data
*          bufferSize               - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertFileEntry(ArchiveHandle    *sourceArchiveHandle,
                              ArchiveHandle    *destinationArchiveHandle,
//TODO: move to covnertInfo?
                              const char       *printableStorageName,
                              const JobOptions *jobOptions,
                              byte             *buffer,
                              uint             bufferSize
                             )
{
  Errors                    error;
  ArchiveFlags              archiveFlags;
  ArchiveEntryInfo          sourceArchiveEntryInfo;
  CompressAlgorithms        deltaCompressAlgorithm,byteCompressAlgorithm;
  CryptTypes                cryptType;
  CryptAlgorithms           cryptAlgorithm;
  Password                  *cryptPassword;
  String                    fileName;
  FileInfo                  fileInfo;
  FileExtendedAttributeList fileExtendedAttributeList;
  ArchiveEntryInfo          destinationArchiveEntryInfo;
  uint64                    fragmentOffset,fragmentSize;
  uint64                    length;
  ulong                     bufferLength;

  // read source file entry
  fileName = String_new();
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = Archive_readFileEntry(&sourceArchiveEntryInfo,
                                sourceArchiveHandle,
                                &deltaCompressAlgorithm,
                                &byteCompressAlgorithm,
                                &cryptType,
                                &cryptAlgorithm,
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

  // get new compression, crypt settings
//TODO
  if (jobOptions->compressAlgorithms.isSet) byteCompressAlgorithm = jobOptions->compressAlgorithms.value.byte;
  if (jobOptions->cryptAlgorithms.isSet   ) cryptAlgorithm        = jobOptions->cryptAlgorithms.values[0];
  cryptPassword = jobOptions->cryptNewPassword;

  archiveFlags = ARCHIVE_FLAG_NONE;

//TODO
#if 0
  // check if file data should be delta compressed
  if (   (fileInfo.size > globalOptions.compressMinFileSize)
      && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.value.delta)
     )
  {
     archiveFlags |= ARCHIVE_FLAG_TRY_DELTA_COMPRESS;
  }

  // check if file data should be byte compressed
  if (   (fileInfo.size > globalOptions.compressMinFileSize)
      && !PatternList_match(createInfo->compressExcludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
     )
  {
     archiveFlags |= ARCHIVE_FLAG_TRY_BYTE_COMPRESS;
  }
#endif

  // create new file entry
  error = Archive_newFileEntry(&destinationArchiveEntryInfo,
                               destinationArchiveHandle,
                               NULL,  // indexHandle,
                               deltaCompressAlgorithm,
                               byteCompressAlgorithm,
                               fileName,
                               &fileInfo,
                               &fileExtendedAttributeList,
                               fragmentOffset,
                               fragmentSize,
                               archiveFlags
                              );
  if (error != ERROR_NONE)
  {
    printError("Cannot create new archive file entry '%s' (error: %s)!\n",
               printableStorageName,
               Error_getText(error)
              );
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(fileName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&destinationArchiveEntryInfo); Archive_closeEntry(&sourceArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }

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
      printError("Cannot read content of file '%s' (error: %s)!\n",
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
      printError("Cannot write content of file '%s' (error: %s)!\n",
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
  DEBUG_TESTCODE() { (void)Archive_closeEntry(&destinationArchiveEntryInfo); (void)Archive_closeEntry(&sourceArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }

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
*          buffer                   - buffer for temporary data
*          bufferSize               - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertImageEntry(ArchiveHandle    *sourceArchiveHandle,
                               ArchiveHandle    *destinationArchiveHandle,
                               const char       *printableStorageName,
                               const JobOptions *jobOptions,
                               byte             *buffer,
                               uint             bufferSize
                              )
{
  Errors             error;
  ArchiveFlags              archiveFlags;
  ArchiveEntryInfo   sourceArchiveEntryInfo;
  CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
  CryptTypes         cryptType;
  CryptAlgorithms    cryptAlgorithm;
  Password           *cryptPassword;
  String             deviceName;
  DeviceInfo         deviceInfo;
  ArchiveEntryInfo   destinationArchiveEntryInfo;
  uint64             blockOffset,blockCount;
  FileSystemTypes    fileSystemType;
  uint64             block;

  // read source image entry
  deviceName = String_new();
  error = Archive_readImageEntry(&sourceArchiveEntryInfo,
                                 sourceArchiveHandle,
                                 &deltaCompressAlgorithm,
                                 &byteCompressAlgorithm,
                                 &cryptType,
                                 &cryptAlgorithm,
                                 deviceName,
                                 &deviceInfo,
                                 &fileSystemType,
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
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }
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
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }
  assert(deviceInfo.blockSize > 0);

  printInfo(1,"  Convert image '%s'...",String_cString(deviceName));

  // get new compression, crypt settings
  if (jobOptions->compressAlgorithms.isSet) byteCompressAlgorithm = jobOptions->compressAlgorithms.value.byte;
  if (jobOptions->cryptAlgorithms.isSet   ) cryptAlgorithm        = jobOptions->cryptAlgorithms.values[0];
  cryptPassword = jobOptions->cryptNewPassword;

  archiveFlags = ARCHIVE_FLAG_NONE;

//TODO
#if 0
  // check if file data should be delta compressed
  if (   (fileInfo.size > globalOptions.compressMinFileSize)
      && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.value.delta)
     )
  {
     archiveFlags |= ARCHIVE_FLAG_TRY_DELTA_COMPRESS;
  }

  // check if file data should be byte compressed
  if (   (fileInfo.size > globalOptions.compressMinFileSize)
      && !PatternList_match(createInfo->compressExcludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
     )
  {
     archiveFlags |= ARCHIVE_FLAG_TRY_BYTE_COMPRESS;
  }
#endif

  // create new image entry
  error = Archive_newImageEntry(&destinationArchiveEntryInfo,
                                destinationArchiveHandle,
                                NULL,  // indexHandle,
                                deltaCompressAlgorithm,
                                byteCompressAlgorithm,
                                deviceName,
                                &deviceInfo,
                                fileSystemType,
                                blockOffset,
                                blockCount,
                                archiveFlags
                               );
  if (error != ERROR_NONE)
  {
    printError("Cannot create new archive image entry '%s' (error: %s)!\n",
               printableStorageName,
               Error_getText(error)
              );
    String_delete(deviceName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&destinationArchiveEntryInfo); Archive_closeEntry(&sourceArchiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }

  // convert archive and device/image content
  block = 0LL;
  while (block < blockCount)
  {
    // read data from archive (only single block)
    error = Archive_readData(&sourceArchiveEntryInfo,buffer,deviceInfo.blockSize);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL!\n");
      printError("Cannot read content of image '%s' (error: %s)!\n",
                 printableStorageName,
                 Error_getText(error)
                );
      break;
    }
    DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

    // write data to destination archive
    error = Archive_writeData(&destinationArchiveEntryInfo,buffer,deviceInfo.blockSize,1);
    if (error != ERROR_NONE)
    {
      printInfo(0,"FAIL!\n");
      printError("Cannot write content of image '%s' (error: %s)!\n",
                 printableStorageName,
                 Error_getText(error)
                );
      break;
    }
    DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

    block += 1LL;

    printInfo(2,"%3d%%\b\b\b\b",(uint)((block*100LL)/blockCount));
  }
  if (error != ERROR_NONE)
  {
    (void)Archive_closeEntry(&destinationArchiveEntryInfo);
    (void)Archive_closeEntry(&sourceArchiveEntryInfo);
    String_delete(deviceName);
    return error;
  }
  DEBUG_TESTCODE() { (void)Archive_closeEntry(&destinationArchiveEntryInfo); (void)Archive_closeEntry(&sourceArchiveEntryInfo);  String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }

  printInfo(2,"    \b\b\b\b");

  // close destination archive entry
  error = Archive_closeEntry(&destinationArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printInfo(1,"FAIL\n");
    printError("Cannot close archive image entry (error: %s)!\n",
               Error_getText(error)
              );
    (void)Archive_closeEntry(&sourceArchiveEntryInfo);
    String_delete(deviceName);
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
    printWarning("unexpected data at end of image entry '%S'.\n",deviceName);
  }

  // close source archive file
  error = Archive_closeEntry(&sourceArchiveEntryInfo);
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
  String                    directoryName;
  FileExtendedAttributeList fileExtendedAttributeList;
  Errors                    error;
  ArchiveEntryInfo          sourceArchiveEntryInfo;
  CryptTypes                cryptType;
  CryptAlgorithms           cryptAlgorithm;
  Password                  *cryptPassword;
  FileInfo                  fileInfo;
  ArchiveEntryInfo          destinationArchiveEntryInfo;

  UNUSED_VARIABLE(jobOptions);

  // read source directory entry
  directoryName = String_new();
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = Archive_readDirectoryEntry(&sourceArchiveEntryInfo,
                                     sourceArchiveHandle,
                                     &cryptType,
                                     &cryptAlgorithm,
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

  // get new crypt settings
  if (jobOptions->cryptAlgorithms.isSet) cryptAlgorithm = jobOptions->cryptAlgorithms.values[0];
  cryptPassword = jobOptions->cryptNewPassword;

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
  DEBUG_TESTCODE() { Archive_closeEntry(&destinationArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); String_delete(directoryName); return DEBUG_TESTCODE_ERROR(); }

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
  String                    linkName;
  String                    fileName;
  CryptTypes                cryptType;
  CryptAlgorithms           cryptAlgorithm;
  Password                  *cryptPassword;
  FileExtendedAttributeList fileExtendedAttributeList;
  Errors                    error;
  ArchiveEntryInfo          sourceArchiveEntryInfo;
  FileInfo                  fileInfo;
  ArchiveEntryInfo          destinationArchiveEntryInfo;

  UNUSED_VARIABLE(jobOptions);

  // read source link entry
  linkName = String_new();
  fileName = String_new();
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = Archive_readLinkEntry(&sourceArchiveEntryInfo,
                                sourceArchiveHandle,
                                &cryptType,
                                &cryptAlgorithm,
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

  // get new crypt settings
  if (jobOptions->cryptAlgorithms.isSet) cryptAlgorithm = jobOptions->cryptAlgorithms.values[0];
  cryptPassword = jobOptions->cryptNewPassword;

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
* Purpose: convert a hard link entry in archive
* Input  : sourceArchiveHandle      - source archive handle
*          destinationArchiveHandle - destination archive handle
*          printableStorageName     - printable storage name
*          jobOptions               - job options
*          buffer                   - buffer for temporary data
*          bufferSize               - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertHardLinkEntry(ArchiveHandle    *sourceArchiveHandle,
                                  ArchiveHandle    *destinationArchiveHandle,
                                  const char       *printableStorageName,
                                  const JobOptions *jobOptions,
                                  byte             *buffer,
                                  uint             bufferSize
                                 )
{
  StringList                fileNameList;
  FileExtendedAttributeList fileExtendedAttributeList;
  Errors                    error;
  ArchiveFlags              archiveFlags;
  ArchiveEntryInfo          sourceArchiveEntryInfo;
  CompressAlgorithms        deltaCompressAlgorithm,byteCompressAlgorithm;
  CryptTypes                cryptType;
  CryptAlgorithms           cryptAlgorithm;
  Password                  *cryptPassword;
  FileInfo                  fileInfo;
  ArchiveEntryInfo          destinationArchiveEntryInfo;
  uint64                    fragmentOffset,fragmentSize;
  uint64                    length;
  ulong                     bufferLength;

  // read source hard link entry
  StringList_init(&fileNameList);
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = Archive_readHardLinkEntry(&sourceArchiveEntryInfo,
                                    sourceArchiveHandle,
                                    &deltaCompressAlgorithm,
                                    &byteCompressAlgorithm,
                                    &cryptType,
                                    &cryptAlgorithm,
                                    &fileNameList,
                                    &fileInfo,
                                    &fileExtendedAttributeList,
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
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    StringList_done(&fileNameList);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); StringList_done(&fileNameList); return DEBUG_TESTCODE_ERROR(); }

  printInfo(1,"  Convert hard link '%s'...",String_cString(StringList_first(&fileNameList,NULL)));

  // get new compression, crypt settings
  if (jobOptions->compressAlgorithms.isSet) byteCompressAlgorithm = jobOptions->compressAlgorithms.value.byte;
  if (jobOptions->cryptAlgorithms.isSet   ) cryptAlgorithm        = jobOptions->cryptAlgorithms.values[0];
  cryptPassword = jobOptions->cryptNewPassword;

  archiveFlags = ARCHIVE_FLAG_NONE;

//TODO
#if 0
  // check if file data should be delta compressed
  if (   (fileInfo.size > globalOptions.compressMinFileSize)
      && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.value.delta)
     )
  {
     archiveFlags |= ARCHIVE_FLAG_TRY_DELTA_COMPRESS;
  }

  // check if file data should be byte compressed
  if (   (fileInfo.size > globalOptions.compressMinFileSize)
      && !PatternList_match(createInfo->compressExcludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
     )
  {
     archiveFlags |= ARCHIVE_FLAG_TRY_BYTE_COMPRESS;
  }
#endif

  // create new hard link entry
  error = Archive_newHardLinkEntry(&destinationArchiveEntryInfo,
                                   destinationArchiveHandle,
                                   NULL,  // indexHandle,
                                   deltaCompressAlgorithm,
                                   byteCompressAlgorithm,
                                   &fileNameList,
                                   &fileInfo,
                                   &fileExtendedAttributeList,
                                   fragmentOffset,
                                   fragmentSize,
                                   archiveFlags
                                  );



  // convert archive and hard link content
  length = 0LL;
  while (length < fragmentSize)
  {
    bufferLength = (ulong)MIN(fragmentSize-length,bufferSize);

    // read source archive
    error = Archive_readData(&sourceArchiveEntryInfo,buffer,bufferLength);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL!\n");
fprintf(stderr,"%s, %d: fragmentSize=%llu\n",__FILE__,__LINE__,fragmentSize);
fprintf(stderr,"%s, %d: read %lu\n",__FILE__,__LINE__,bufferLength);
      printError("Cannot read content of hard link '%s' (error: %s)!\n",
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
      printError("Cannot write content of hard link '%s' (error: %s)!\n",
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
    StringList_done(&fileNameList);
    return error;
  }
  printInfo(2,"    \b\b\b\b");

  // close destination archive entry
  error = Archive_closeEntry(&destinationArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printInfo(1,"FAIL\n");
    printError("Cannot close archive hard link entry (error: %s)!\n",
               Error_getText(error)
              );
    (void)Archive_closeEntry(&sourceArchiveEntryInfo);
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    StringList_done(&fileNameList);
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
    printWarning("unexpected data at end of hard link entry '%S'.\n",StringList_first(&fileNameList,NULL));
  }

  // close source archive file
  error = Archive_closeEntry(&sourceArchiveEntryInfo);
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
  String                    fileName;
  FileExtendedAttributeList fileExtendedAttributeList;
  Errors                    error;
  ArchiveEntryInfo          sourceArchiveEntryInfo;
  CryptTypes                cryptType;
  CryptAlgorithms           cryptAlgorithm;
  Password                  *cryptPassword;
  FileInfo                  fileInfo;
  ArchiveEntryInfo          destinationArchiveEntryInfo;

  UNUSED_VARIABLE(jobOptions);

  // read source special entry
  fileName = String_new();
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = Archive_readSpecialEntry(&sourceArchiveEntryInfo,
                                   sourceArchiveHandle,
                                   &cryptType,
                                   &cryptAlgorithm,
                                   fileName,
                                   &fileInfo,
                                   &fileExtendedAttributeList
                                  );
  if (error != ERROR_NONE)
  {
    printError("Cannot read 'special' content of archive '%s' (error: %s)!\n",
               printableStorageName,
               Error_getText(error)
              );
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(fileName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }

  printInfo(1,"  Convert special device '%s'...",String_cString(fileName));

  // get new crypt settings
  if (jobOptions->cryptAlgorithms.isSet) cryptAlgorithm = jobOptions->cryptAlgorithms.values[0];
  cryptPassword = jobOptions->cryptNewPassword;

  // create new special entry
  error = Archive_newSpecialEntry(&destinationArchiveEntryInfo,
                                  destinationArchiveHandle,
                                  NULL,  // indexHandle,
                                  fileName,
                                  &fileInfo,
                                  &fileExtendedAttributeList
                                 );
  if (error != ERROR_NONE)
  {
    printError("Cannot create new archive special entry '%s' (error: %s)\n",
               printableStorageName,
               Error_getText(error)
              );
    (void)Archive_closeEntry(&sourceArchiveEntryInfo);
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(fileName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&destinationArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }

  // close destination archive entry
  error = Archive_closeEntry(&destinationArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printInfo(1,"FAIL\n");
    printError("Cannot close archive special entry (error: %s)!\n",
               Error_getText(error)
              );
    (void)Archive_closeEntry(&sourceArchiveEntryInfo);
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(fileName);
    return error;
  }

  printInfo(1,"OK\n");

  // check if all data read
  if (!Archive_eofData(&sourceArchiveEntryInfo))
  {
    printWarning("unexpected data at end of special entry '%S'.\n",fileName);
  }

  // close source archive file
  error = Archive_closeEntry(&sourceArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'special' entry fail (error: %s)\n",Error_getText(error));
  }

  // free resources
  File_doneExtendedAttributes(&fileExtendedAttributeList);
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
  byte          *buffer;
  ArchiveHandle sourceArchiveHandle;
  EntryMsg      entryMsg;
  Errors        error;

  assert(convertInfo != NULL);

  // init variables
  printableStorageName = String_new();
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // get printable storage name
  Storage_getPrintableName(printableStorageName,&convertInfo->storageInfo.storageSpecifier,NULL);

  // convert entries
  while (   (convertInfo->failError == ERROR_NONE)
//TODO
//         && !isAborted(convertInfo)
         && MsgQueue_get(&convertInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER)
        )
  {
//fprintf(stderr,"%s, %d: %p %d %llu\n",__FILE__,__LINE__,pthread_self(),entryMsg.archiveEntryType,entryMsg.offset);
//TODO: open only when changed
    // open source archive
    error = Archive_open(&sourceArchiveHandle,
                         entryMsg.storageInfo,
                         NULL,  // fileName,
NULL,//                         deltaSourceList,
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

//TODO: required?
    // set crypt salt, crypt key derive type, and crypt mode
    Archive_setCryptInfo(&sourceArchiveHandle,entryMsg.archiveCryptInfo);
//    Archive_setCryptSalt(&sourceArchiveHandle,entryMsg.cryptSalt.data,sizeof(entryMsg.cryptSalt.data));
//    Archive_setCryptMode(&sourceArchiveHandle,entryMsg.cryptMode);
//    Archive_setCryptKeyDeriveType(&sourceArchiveHandle,entryMsg.cryptKeyDeriveType);

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
                                 &convertInfo->destinationArchiveHandle,
                                 String_cString(printableStorageName),
                                 convertInfo->jobOptions,
                                 buffer,
                                 BUFFER_SIZE
                                );
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        error = convertImageEntry(&sourceArchiveHandle,
                                  &convertInfo->destinationArchiveHandle,
                                  String_cString(printableStorageName),
                                  convertInfo->jobOptions,
                                  buffer,
                                  BUFFER_SIZE
                                 );
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        error = convertDirectoryEntry(&sourceArchiveHandle,
                                      &convertInfo->destinationArchiveHandle,
                                      String_cString(printableStorageName),
                                      convertInfo->jobOptions
                                     );
        break;
      case ARCHIVE_ENTRY_TYPE_LINK:
        error = convertLinkEntry(&sourceArchiveHandle,
                                 &convertInfo->destinationArchiveHandle,
                                 String_cString(printableStorageName),
                                 convertInfo->jobOptions
                                );
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        error = convertHardLinkEntry(&sourceArchiveHandle,
                                     &convertInfo->destinationArchiveHandle,
                                     String_cString(printableStorageName),
                                     convertInfo->jobOptions,
                                     buffer,
                                     BUFFER_SIZE
                                    );
        break;
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
        error = convertSpecialEntry(&sourceArchiveHandle,
                                    &convertInfo->destinationArchiveHandle,
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
  free(buffer);
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
  AutoFreeList           autoFreeList;
  String                 printableStorageName;
  Thread                 *convertThreads;
  uint                   convertThreadCount;
  ConvertInfo            convertInfo;
  Errors                 error;
  CryptSignatureStates   allCryptSignatureState;
  String                 baseName;
  Thread                 storageThread;
  uint                   i;
  Errors                 failError;
  ArchiveHandle          sourceArchiveHandle;
  ArchiveEntryTypes      archiveEntryType;
  const ArchiveCryptInfo *archiveCryptInfo;
  uint64                 offset;
  EntryMsg               entryMsg;

  assert(storageSpecifier != NULL);
  assert(jobOptions != NULL);

  // init variables
  AutoFree_init(&autoFreeList);
  printableStorageName = String_new();
  convertThreadCount   = (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores();
  convertThreads       = (Thread*)malloc(convertThreadCount*sizeof(Thread));
  if (convertThreads == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,printableStorageName,{ String_delete(printableStorageName); });
  AUTOFREE_ADD(&autoFreeList,convertThreads,{ free(convertThreads); });

  // init convert info
  initConvertInfo(&convertInfo,
                  jobOptions,
                  getPasswordFunction,
                  getPasswordUserData,
//TODO
NULL,  //               pauseTestFlag,
NULL,  //               requestedAbortFlag,
                  logHandle
                 );
  AUTOFREE_ADD(&autoFreeList,&convertInfo,{ (void)doneConvertInfo(&convertInfo); });

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

  // check if storage exists
  if (!Storage_exists(&convertInfo.storageInfo,archiveName))
  {
    printError("Archive not found '%s'!\n",
               String_cString(printableStorageName)
              );
    AutoFree_cleanup(&autoFreeList);
    return ERROR_ARCHIVE_NOT_FOUND;
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

  // check signatures
  if (!jobOptions->skipVerifySignaturesFlag)
  {
    error = Archive_verifySignatures(&sourceArchiveHandle,
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

  // create destination archive
  baseName = File_getBaseName(String_new(),(archiveName != NULL) ? archiveName : storageSpecifier->archiveName);
  if (!String_isEmpty(jobOptions->destination))
  {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,String_cString(baseName));

    File_setFileName(convertInfo.archiveName,jobOptions->destination);
    File_appendFileName(convertInfo.archiveName,baseName);

    error = File_getTmpFileName(convertInfo.newArchiveName,
                                String_cString(baseName),
                                jobOptions->destination
                               );
fprintf(stderr,"%s, %d: convertInfo.newArchiveName=%s\n",__FILE__,__LINE__,String_cString(convertInfo.newArchiveName));
  }
  else
  {
    File_setFileName(convertInfo.archiveName,(archiveName != NULL) ? archiveName : storageSpecifier->archiveName);

    error = Storage_getTmpName(convertInfo.newArchiveName,
                               &convertInfo.storageInfo
                              );
  }
  String_delete(baseName);
  if (error != ERROR_NONE)
  {
    printError("Cannot create temporary file (error: %s)!\n",
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&convertInfo.newArchiveName,{ (void)Storage_delete(&convertInfo.storageInfo,convertInfo.newArchiveName); });
  error = Archive_create(&convertInfo.destinationArchiveHandle,
                         &convertInfo.storageInfo,
                         convertInfo.newArchiveName,
                         NULL,  // indexHandle,
                         INDEX_ID_NONE,  // uuidId,
                         INDEX_ID_NONE,  // entityId,
                         NULL,  // jobUUID,
                         NULL,  // scheduleUUID,
NULL,//                         deltaSourceList,
ARCHIVE_TYPE_NONE,//                         archiveType,
                         CALLBACK(NULL,NULL),  // archiveInitFunction
                         CALLBACK(NULL,NULL),  // archiveDoneFunction
CALLBACK(NULL,NULL),//                         CALLBACK(archiveGetSize,&convertInfo),
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
  DEBUG_TESTCODE() { Archive_close(&convertInfo.destinationArchiveHandle); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&convertInfo.destinationArchiveHandle,{ Archive_close(&convertInfo.destinationArchiveHandle); });

//TODO: really required? If convert is done for each archive storage can be done as the final step without a separated thread
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
  while (   !Archive_eof(&sourceArchiveHandle,ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS|(isPrintInfo(3) ? ARCHIVE_FLAG_PRINT_UNKNOWN_CHUNKS : ARCHIVE_FLAG_NONE))
         && (failError == ERROR_NONE)
        )
  {
    // get next archive entry type
    error = Archive_getNextArchiveEntry(&sourceArchiveHandle,
                                        &archiveEntryType,
                                        &archiveCryptInfo,
                                        &offset,
                                        ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS|(isPrintInfo(3) ? ARCHIVE_FLAG_PRINT_UNKNOWN_CHUNKS : ARCHIVE_FLAG_NONE)
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
//TODO: remove
//fprintf(stderr,"%s, %d: archiveEntryType=%s\n",__FILE__,__LINE__,Archive_archiveEntryTypeToString(archiveEntryType,NULL));

    // send entry to convert threads
    entryMsg.storageInfo        = &convertInfo.storageInfo;
    entryMsg.archiveEntryType = archiveEntryType;
    entryMsg.archiveCryptInfo = archiveCryptInfo;
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
  AUTOFREE_REMOVE(&autoFreeList,&convertInfo.destinationArchiveHandle);
  error = Archive_close(&convertInfo.destinationArchiveHandle);
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

  // close source archive
  AUTOFREE_REMOVE(&autoFreeList,&sourceArchiveHandle);
  (void)Archive_close(&sourceArchiveHandle);

  // done storage
  (void)Storage_done(&convertInfo.storageInfo);

  // done convert info
  doneConvertInfo(&convertInfo);

  // free resources
  free(convertThreads);
  String_delete(printableStorageName);
  AutoFree_done(&autoFreeList);

fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
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
