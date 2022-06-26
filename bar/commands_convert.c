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
#include <inttypes.h>
#include <errno.h>
#include <assert.h>

#include "common/global.h"
#include "common/autofree.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/msgqueues.h"
#include "common/patterns.h"
#include "common/files.h"
#include "common/filesystems.h"

#include "bar.h"
#include "errors.h"
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
  StorageInfo             storageInfo;
  const char              *newJobUUID;
  const char              *newScheduleUUID;
  uint64                  newCreatedDateTime;
  const JobOptions        *newJobOptions;
  GetNamePasswordFunction getNamePasswordFunction;
  void                    *getNamePasswordUserData;
  LogHandle               *logHandle;                         // log handle

  String                  archiveName;                        // archive name (converted archive)
  String                  newArchiveName;                     // temporary new archive name (converted archive)
  ArchiveHandle           destinationArchiveHandle;

  MsgQueue                entryMsgQueue;                      // queue with entries to convert
  MsgQueue                storageMsgQueue;                    // queue with waiting storage files
  bool                    storageThreadExitFlag;

  Errors                  failError;                          // failure error
} ConvertInfo;

// entry message send to convert threads
typedef struct
{
//  StorageInfo            *storageInfo;
  uint                   archiveIndex;                        // still not used
  const ArchiveHandle    *archiveHandle;
  ArchiveEntryTypes      archiveEntryType;
  const ArchiveCryptInfo *archiveCryptInfo;
  uint64                 offset;                              // offset in archive
} EntryMsg;

// storage message, send from convert threads -> storage thread
typedef struct
{
  String intermediateFileName;                                // intermediate archive file name
  uint64 intermediateFileSize;                                // intermediate archive size [bytes]
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

  String_delete(storageMsg->intermediateFileName);
}

/***********************************************************************\
* Name   : initConvertInfo
* Purpose: initialize convert info
* Input  : convertInfo             - convert info variable
*          newJobUUID              - new job UUID or NULL
*          newScheduleUUID         - new schedule UUID or NULL
*          newCreatedDateTime      - new created date/time or 0
*          newJobOptions           - new job options
*          getNamePasswordFunction - get password call back
*          getNamePasswordUserData - user data for get password call back
*          logHandle               - log handle (can be NULL)
* Output : convertInfo - initialized convert info variable
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initConvertInfo(ConvertInfo             *convertInfo,
                           const char              *newJobUUID,
                           const char              *newScheduleUUID,
                           uint64                  newCreatedDateTime,
                           const JobOptions        *newJobOptions,
                           GetNamePasswordFunction getNamePasswordFunction,
                           void                    *getNamePasswordUserData,
                           LogHandle               *logHandle
                          )
{
  assert(convertInfo != NULL);
  assert(newJobOptions != NULL);

  // init variables
  convertInfo->newJobUUID              = newJobUUID;
  convertInfo->newScheduleUUID         = newScheduleUUID;
  convertInfo->newCreatedDateTime      = newCreatedDateTime;
  convertInfo->newJobOptions           = newJobOptions;
  convertInfo->getNamePasswordFunction = getNamePasswordFunction;
  convertInfo->getNamePasswordUserData = getNamePasswordUserData;
  convertInfo->logHandle               = logHandle;
  convertInfo->archiveName             = String_new();
  convertInfo->newArchiveName          = String_new();
  convertInfo->failError               = ERROR_NONE;
  convertInfo->storageThreadExitFlag   = FALSE;

  // init entry name queue, storage queue
  if (!MsgQueue_init(&convertInfo->entryMsgQueue,
                     MAX_ENTRY_MSG_QUEUE,
                     CALLBACK_((MsgQueueMsgFreeFunction)freeEntryMsg,NULL)
                    )
     )
  {
    HALT_FATAL_ERROR("Cannot initialize entry message queue!");
  }
  if (!MsgQueue_init(&convertInfo->storageMsgQueue,
// TODO: 0?
                     0,
                     CALLBACK_((MsgQueueMsgFreeFunction)freeStorageMsg,NULL)
                    )
     )
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

  DEBUG_ADD_RESOURCE_TRACE(convertInfo,ConvertInfo);
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

  DEBUG_REMOVE_RESOURCE_TRACE(convertInfo,ConvertInfo);

  MsgQueue_done(&convertInfo->storageMsgQueue);
  MsgQueue_done(&convertInfo->entryMsgQueue);
  String_delete(convertInfo->newArchiveName);
  String_delete(convertInfo->archiveName);
}

/***********************************************************************\
* Name   : archiveStore
* Purpose: call back to store archive file
* Input  : storageInfo          - storage info
*          jobUUID              - job UUID id
*          scheduleUUID         - schedule UUID id
*          entityId             - index entity id
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
                          IndexId      uuidId,
                          ConstString  jobUUID,
                          ConstString  scheduleUUID,
                          IndexId      entityId,
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

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(uuidId);
  UNUSED_VARIABLE(jobUUID);
  UNUSED_VARIABLE(scheduleUUID);
  UNUSED_VARIABLE(entityId);
  UNUSED_VARIABLE(storageId);
  UNUSED_VARIABLE(partNumber);

  // get file info
// TODO replace by getFileSize()
  error = File_getInfo(&fileInfo,intermediateFileName);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // send to storage thread
  storageMsg.intermediateFileName = String_duplicate(intermediateFileName);
  storageMsg.intermediateFileSize = intermediateFileSize;
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
  assert(convertInfo->newJobOptions != NULL);

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

    // get next archive to store
    if (!MsgQueue_get(&convertInfo->storageMsgQueue,&storageMsg,NULL,sizeof(storageMsg),WAIT_FOREVER))
    {
      break;
    }
    AUTOFREE_ADD(&autoFreeList,&storageMsg,
                 {
                   File_delete(storageMsg.intermediateFileName,FALSE);
                   freeStorageMsg(&storageMsg,NULL);
                 }
                );

    // get file info
    error = File_getInfo(&fileInfo,storageMsg.intermediateFileName);
    if (error != ERROR_NONE)
    {
      printError("cannot get information for file '%s' (error: %s)!",
                 String_cString(storageMsg.intermediateFileName),
                 Error_getText(error)
                );
      convertInfo->failError = error;

      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }
    DEBUG_TESTCODE() { convertInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); break; }

    // get printable storage name
    Storage_getPrintableName(printableStorageName,&convertInfo->storageInfo.storageSpecifier,convertInfo->archiveName);

    // open file to store
    #ifndef NDEBUG
      printInfo(1,"Store '%s' to '%s'...",String_cString(storageMsg.intermediateFileName),String_cString(printableStorageName));
    #else /* not NDEBUG */
      printInfo(1,"Store '%s'...",String_cString(printableStorageName));
    #endif /* NDEBUG */
    error = File_open(&fileHandle,storageMsg.intermediateFileName,FILE_OPEN_READ);
    if (error != ERROR_NONE)
    {
      printInfo(0,"FAIL!\n");
      printError("cannot open file '%s' (error: %s)!",
                 String_cString(storageMsg.intermediateFileName),
                 Error_getText(error)
                );
      convertInfo->failError = error;
      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }
    AUTOFREE_ADD(&autoFreeList,&fileHandle,{ File_close(&fileHandle); });
    DEBUG_TESTCODE() { convertInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); continue; }

    // rename original storage file to temporary file
    if (Storage_exists(&convertInfo->storageInfo,convertInfo->archiveName))
    {
      Storage_getTmpName(tmpArchiveName,&convertInfo->storageInfo);
//fprintf(stderr,"%s, %d: rename original %s -> %s\n",__FILE__,__LINE__,String_cString(convertInfo->archiveName),String_cString(tmpArchiveName));
      error = Storage_rename(&convertInfo->storageInfo,convertInfo->archiveName,tmpArchiveName);
      if (error != ERROR_NONE)
      {
        printInfo(0,"FAIL!\n");
        printError("cannot store '%s' (error: %s)",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
        convertInfo->failError = error;
        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        continue;
      }
      AUTOFREE_ADD(&autoFreeList,&tmpArchiveName, { Storage_rename(&convertInfo->storageInfo,tmpArchiveName,convertInfo->archiveName); });
    }
    else
    {
      String_clear(tmpArchiveName);
    }

    // create storage file
    if (!String_isEmpty(convertInfo->newJobOptions->destination))
    {
// TODO: create correct destination name

//TODO: remove?
      assert(convertInfo->destinationArchiveHandle.mode == ARCHIVE_MODE_CREATE);

      // create local file
      error = File_open(&toFileHandle,convertInfo->archiveName,FILE_OPEN_CREATE);
      if (error != ERROR_NONE)
      {
        printInfo(0,"FAIL!\n");
        printError("cannot store '%s' (error: %s)",
                   String_cString(convertInfo->destinationArchiveHandle.archiveName),
                   Error_getText(error)
                  );
        break;
      }
      DEBUG_TESTCODE() { Storage_close(&storageHandle); error = DEBUG_TESTCODE_ERROR(); break; }
      AUTOFREE_ADD(&autoFreeList,&toFileHandle,{ File_close(&toFileHandle); });

      // transfer data from temporary file to local file
      File_seek(&fileHandle,0);
      do
      {
        // read data from local intermediate file
        error = File_read(&fileHandle,buffer,BUFFER_SIZE,&bufferLength);
        if (error != ERROR_NONE)
        {
          printInfo(0,"FAIL!\n");
          printError("cannot read file '%s' (error: %s)!",
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
          printError("cannot write file '%s' (error: %s)!",
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

        // create storage file
        error = Storage_create(&storageHandle,
                               &convertInfo->storageInfo,
                               convertInfo->archiveName,
                               fileInfo.size,
                               TRUE  // forceFlag
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
            printError("cannot store '%s' (error: %s)",
                       String_cString(printableStorageName),
                       Error_getText(error)
                      );
            break;
          }
        }
        DEBUG_TESTCODE() { Storage_close(&storageHandle); error = DEBUG_TESTCODE_ERROR(); break; }
        AUTOFREE_ADD(&autoFreeList,&storageHandle,{ Storage_close(&storageHandle); Storage_delete(&convertInfo->storageInfo,convertInfo->archiveName); });

        // transfer data from temporary file to storage
        File_seek(&fileHandle,0);
        do
        {
          // read data from local intermediate file
          error = File_read(&fileHandle,buffer,BUFFER_SIZE,&bufferLength);
          if (error != ERROR_NONE)
          {
            printInfo(0,"FAIL!\n");
            printError("cannot read file '%s' (error: %s)!",
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
              printError("cannot write file '%s' (error: %s)!",
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
      while (   (convertInfo->failError == ERROR_NONE)                           // no eror
  //           && !isAborted(convertInfo)                                        // not aborted
             && ((error != ERROR_NONE) && (Error_getErrno(error) != ENOSPC))     // some error and not "no space left"
             && (retryCount <= MAX_RETRIES)                                      // still some retry left
            );
    }
    if (error != ERROR_NONE)
    {
      if (convertInfo->failError == ERROR_NONE) convertInfo->failError = error;
      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }

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

    if (!String_isEmpty(tmpArchiveName))
    {
      // delete original storage file
//fprintf(stderr,"%s, %d: delete saved orginal %s\n",__FILE__,__LINE__,String_cString(tmpArchiveName));
      error = Storage_delete(&convertInfo->storageInfo,tmpArchiveName);
      if (error != ERROR_NONE)
      {
        printInfo(0,"FAIL!\n");
        printError("cannot store '%s' (error: %s)",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
        convertInfo->failError = error;
        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        continue;
      }
      AUTOFREE_REMOVE(&autoFreeList,&tmpArchiveName);
    }

    // done
    printInfo(1,"OK\n");

    // delete temporary storage file
    error = File_delete(storageMsg.intermediateFileName,FALSE);
    if (error != ERROR_NONE)
    {
      printWarning("cannot delete file '%s' (error: %s)!",
                   String_cString(storageMsg.intermediateFileName),
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
    error = File_delete(storageMsg.intermediateFileName,FALSE);
    if (error != ERROR_NONE)
    {
      printWarning("cannot delete file '%s' (error: %s)!",
                   String_cString(storageMsg.intermediateFileName),
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
*          newJobOptions            - new job options
*          buffer                   - buffer for temporary data
*          bufferSize               - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertFileEntry(ArchiveHandle    *sourceArchiveHandle,
                              ArchiveHandle    *destinationArchiveHandle,
                              const JobOptions *newJobOptions,
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
  String                    fileName;
  FileInfo                  fileInfo;
  FileExtendedAttributeList fileExtendedAttributeList;
  ArchiveEntryInfo          destinationArchiveEntryInfo;
  uint64                    fragmentOffset,fragmentSize;
  char                      sizeString[32];
  char                      fragmentString[256];
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
    printError("cannot read 'file' content of archive '%s' (error: %s)!",
               String_cString(sourceArchiveHandle->printableStorageName),
               Error_getText(error)
              );
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(fileName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }

  // get size/fragment info
  if (globalOptions.humanFormatFlag)
  {
    getHumanSizeString(sizeString,sizeof(sizeString),fileInfo.size);
  }
  else
  {
    stringFormat(sizeString,sizeof(sizeString),"%*"PRIu64,stringInt64Length(globalOptions.fragmentSize),fileInfo.size);
  }
  stringClear(fragmentString);
  if (fragmentSize < fileInfo.size)
  {
    stringFormat(fragmentString,sizeof(fragmentString),
                 ", fragment %*"PRIu64"..%*"PRIu64,
                 stringInt64Length(fileInfo.size),fragmentOffset,
                 stringInt64Length(fileInfo.size),fragmentOffset+fragmentSize-1LL
                );
  }
  printInfo(1,"  Convert file      '%s' (%s bytes%s)...",String_cString(fileName),sizeString,fragmentString);

  // set new compression, crypt settings
  if (CmdOption_isSet(&globalOptions.compressAlgorithms)) byteCompressAlgorithm = newJobOptions->compressAlgorithms.byte;
  if (CmdOption_isSet(globalOptions.cryptAlgorithms    )) cryptAlgorithm        = newJobOptions->cryptAlgorithms[0];

  archiveFlags = ARCHIVE_FLAG_NONE;

  // check if file data should be byte compressed
  if (   (fileInfo.size > globalOptions.compressMinFileSize)
//TODO
//      && !PatternList_match(newJobOptions->compressExcludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
     )
  {
     archiveFlags |= ARCHIVE_FLAG_TRY_BYTE_COMPRESS;
  }

  // create new file entry
  error = Archive_newFileEntry(&destinationArchiveEntryInfo,
                               destinationArchiveHandle,
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
    printError("cannot create new archive file entry '%s' (error: %s)",
               String_cString(sourceArchiveHandle->printableStorageName),
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
      printError("cannot read content of file '%s' (error: %s)!",
                 String_cString(sourceArchiveHandle->printableStorageName),
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
      printError("cannot write content of file '%s' (error: %s)!",
                 String_cString(destinationArchiveHandle->printableStorageName),
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
    printError("cannot close archive file entry (error: %s)!",
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
    printWarning("unexpected data at end of file entry '%S'",fileName);
  }

  // close source archive entry
  error = Archive_closeEntry(&sourceArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'file' entry fail (error: %s)",Error_getText(error));
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
*          newJobOptions            - new job options
*          buffer                   - buffer for temporary data
*          bufferSize               - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertImageEntry(ArchiveHandle    *sourceArchiveHandle,
                               ArchiveHandle    *destinationArchiveHandle,
                               const JobOptions *newJobOptions,
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
  String             deviceName;
  DeviceInfo         deviceInfo;
  ArchiveEntryInfo   destinationArchiveEntryInfo;
  uint64             blockOffset,blockCount;
  FileSystemTypes    fileSystemType;
  char               sizeString[32];
  char               fragmentString[256];
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
    printError("cannot read 'image' content of archive '%s' (error: %s)!",
               String_cString(sourceArchiveHandle->printableStorageName),
               Error_getText(error)
              );
    String_delete(deviceName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }
  if (deviceInfo.blockSize > bufferSize)
  {
    printError("device block size %llu on '%s' is too big (max: %llu)",
               deviceInfo.blockSize,
               String_cString(deviceName),
               bufferSize
              );
    String_delete(deviceName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }
  assert(deviceInfo.blockSize > 0);

  // get size/fragment info
  if (globalOptions.humanFormatFlag)
  {
    getHumanSizeString(sizeString,sizeof(sizeString),deviceInfo.size);
  }
  else
  {
    stringFormat(sizeString,sizeof(sizeString),"%*"PRIu64,stringInt64Length(globalOptions.fragmentSize),blockCount*(uint64)deviceInfo.blockSize);
  }
  stringClear(fragmentString);
  if ((blockCount*(uint64)deviceInfo.blockSize) < deviceInfo.size)
  {
    stringFormat(fragmentString,sizeof(fragmentString),
                 ", fragment %*"PRIu64"..%*"PRIu64,
                 stringInt64Length(deviceInfo.size),blockOffset*(uint64)deviceInfo.blockSize,
                 stringInt64Length(deviceInfo.size),(blockOffset*(uint64)deviceInfo.blockSize)+(blockCount*(uint64)deviceInfo.blockSize)-1LL
                );
  }
  printInfo(1,"  Convert image     '%s' (%s bytes%s)...",String_cString(deviceName),sizeString,fragmentString);

  // set new compression, crypt settings
  if (CmdOption_isSet(&globalOptions.compressAlgorithms)) byteCompressAlgorithm = newJobOptions->compressAlgorithms.byte;
  if (CmdOption_isSet(globalOptions.cryptAlgorithms    )) cryptAlgorithm        = newJobOptions->cryptAlgorithms[0];

  archiveFlags = ARCHIVE_FLAG_NONE;

  // check if file data should be byte compressed
  if (   (deviceInfo.size > globalOptions.compressMinFileSize)
//TODO
//      && !PatternList_match(createInfo->compressExcludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
     )
  {
     archiveFlags |= ARCHIVE_FLAG_TRY_BYTE_COMPRESS;
  }

  // create new image entry
  error = Archive_newImageEntry(&destinationArchiveEntryInfo,
                                destinationArchiveHandle,
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
    printError("cannot create new archive image entry '%s' (error: %s)!",
               String_cString(destinationArchiveHandle->printableStorageName),
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
      printError("cannot read content of image '%s' (error: %s)!",
                 String_cString(sourceArchiveHandle->printableStorageName),
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
      printError("cannot write content of image '%s' (error: %s)!",
                 String_cString(destinationArchiveHandle->printableStorageName),
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
  DEBUG_TESTCODE() { (void)Archive_closeEntry(&destinationArchiveEntryInfo); (void)Archive_closeEntry(&sourceArchiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }

  printInfo(2,"    \b\b\b\b");

  // close destination archive entry
  error = Archive_closeEntry(&destinationArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printInfo(1,"FAIL\n");
    printError("cannot close archive image entry (error: %s)!",
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
    printWarning("unexpected data at end of image entry '%S'",deviceName);
  }

  // close source archive entry
  error = Archive_closeEntry(&sourceArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'image' entry fail (error: %s)",Error_getText(error));
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
*          newJobOptions            - new job options
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertDirectoryEntry(ArchiveHandle    *sourceArchiveHandle,
                                   ArchiveHandle    *destinationArchiveHandle,
                                   const JobOptions *newJobOptions
                                  )
{
  String                    directoryName;
  FileExtendedAttributeList fileExtendedAttributeList;
  Errors                    error;
  ArchiveEntryInfo          sourceArchiveEntryInfo;
  CryptTypes                cryptType;
  CryptAlgorithms           cryptAlgorithm;
  FileInfo                  fileInfo;
  ArchiveEntryInfo          destinationArchiveEntryInfo;

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
    printError("cannot read 'directory' content of archive '%s' (error: %s)!",
               String_cString(sourceArchiveHandle->printableStorageName),
               Error_getText(error)
              );
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(directoryName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); String_delete(directoryName); return DEBUG_TESTCODE_ERROR(); }

  printInfo(1,"  Convert directory '%s'...",String_cString(directoryName));

  // set new crypt settings
  if (CmdOption_isSet(globalOptions.cryptAlgorithms)) cryptAlgorithm = newJobOptions->cryptAlgorithms[0];

  // create new directory entry
  error = Archive_newDirectoryEntry(&destinationArchiveEntryInfo,
                                    destinationArchiveHandle,
                                    directoryName,
                                    &fileInfo,
                                    &fileExtendedAttributeList
                                   );
  if (error != ERROR_NONE)
  {
    printError("cannot create new archive directory entry '%s' (error: %s)",
               String_cString(destinationArchiveHandle->printableStorageName),
               Error_getText(error)
              );
    (void)Archive_closeEntry(&sourceArchiveEntryInfo);
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(directoryName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&destinationArchiveEntryInfo); (void)Archive_closeEntry(&sourceArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); String_delete(directoryName); return DEBUG_TESTCODE_ERROR(); }

  // close destination archive entry
  error = Archive_closeEntry(&destinationArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printInfo(1,"FAIL\n");
    printError("cannot close archive directory entry (error: %s)!",
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
    printWarning("unexpected data at end of directory entry '%S'",directoryName);
  }

  // close source archive entry
  error = Archive_closeEntry(&sourceArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'directory' entry fail (error: %s)",Error_getText(error));
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
*          newJobOptions            - new job options
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertLinkEntry(ArchiveHandle    *sourceArchiveHandle,
                              ArchiveHandle    *destinationArchiveHandle,
                              const JobOptions *newJobOptions
                             )
{
  String                    linkName;
  String                    fileName;
  CryptTypes                cryptType;
  CryptAlgorithms           cryptAlgorithm;
  FileExtendedAttributeList fileExtendedAttributeList;
  Errors                    error;
  ArchiveEntryInfo          sourceArchiveEntryInfo;
  FileInfo                  fileInfo;
  ArchiveEntryInfo          destinationArchiveEntryInfo;

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
    printError("cannot read 'link' content of archive '%s' (error: %s)!",
               String_cString(sourceArchiveHandle->printableStorageName),
               Error_getText(error)
              );
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(fileName);
    String_delete(linkName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); String_delete(fileName); String_delete(linkName); return DEBUG_TESTCODE_ERROR(); }

  printInfo(1,"  Convert link      '%s'...",String_cString(linkName));

  // set new crypt settings
  if (CmdOption_isSet(globalOptions.cryptAlgorithms)) cryptAlgorithm = newJobOptions->cryptAlgorithms[0];

  // create new link entry
  error = Archive_newLinkEntry(&destinationArchiveEntryInfo,
                               destinationArchiveHandle,
                               linkName,
                               fileName,
                               &fileInfo,
                               &fileExtendedAttributeList
                              );
  if (error != ERROR_NONE)
  {
    printInfo(1,"FAIL\n");
    printError("cannot create new archive link entry '%s' (error: %s)",
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
    printError("cannot close archive link entry (error: %s)!",
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
    printWarning("unexpected data at end of link entry '%S'",linkName);
  }

  // close source archive entry
  error = Archive_closeEntry(&sourceArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'link' entry fail (error: %s)",Error_getText(error));
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
*          newJobOptions            - new job options
*          buffer                   - buffer for temporary data
*          bufferSize               - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertHardLinkEntry(ArchiveHandle    *sourceArchiveHandle,
                                  ArchiveHandle    *destinationArchiveHandle,
                                  const JobOptions *newJobOptions,
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
  FileInfo                  fileInfo;
  ArchiveEntryInfo          destinationArchiveEntryInfo;
  uint64                    fragmentOffset,fragmentSize;
  char                      sizeString[32];
  char                      fragmentString[256];
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
    printError("cannot read 'hard link' content of archive '%s' (error: %s)!",
               String_cString(sourceArchiveHandle->printableStorageName),
               Error_getText(error)
              );
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    StringList_done(&fileNameList);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); StringList_done(&fileNameList); return DEBUG_TESTCODE_ERROR(); }

  // get size/fragment info
  if (globalOptions.humanFormatFlag)
  {
    getHumanSizeString(sizeString,sizeof(sizeString),fileInfo.size);
  }
  else
  {
    stringFormat(sizeString,sizeof(sizeString),"%*"PRIu64,stringInt64Length(globalOptions.fragmentSize),fileInfo.size);
  }
  stringClear(fragmentString);
  if (fragmentSize < fileInfo.size)
  {
    stringFormat(fragmentString,sizeof(fragmentString),", fragment %"PRIu64"..%"PRIu64,fragmentOffset,fragmentOffset+fragmentSize-1LL);
  }
  printInfo(1,"  Convert hard link '%s' (%s bytes%s)...",String_cString(StringList_first(&fileNameList,NULL)),sizeString,fragmentString);

  // set new compression, crypt settings
  if (CmdOption_isSet(&globalOptions.compressAlgorithms)) byteCompressAlgorithm = newJobOptions->compressAlgorithms.byte;
  if (CmdOption_isSet(globalOptions.cryptAlgorithms    )) cryptAlgorithm        = newJobOptions->cryptAlgorithms[0];

  archiveFlags = ARCHIVE_FLAG_NONE;

  // check if file data should be byte compressed
  if (   (fileInfo.size > globalOptions.compressMinFileSize)
//TODO
//      && !PatternList_match(createInfo->compressExcludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
     )
  {
     archiveFlags |= ARCHIVE_FLAG_TRY_BYTE_COMPRESS;
  }

  // create new hard link entry
  error = Archive_newHardLinkEntry(&destinationArchiveEntryInfo,
                                   destinationArchiveHandle,
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
      printError("cannot read content of hard link '%s' (error: %s)!",
                 String_cString(sourceArchiveHandle->printableStorageName),
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
      printError("cannot write content of hard link '%s' (error: %s)!",
                 String_cString(destinationArchiveHandle->printableStorageName),
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
    printError("cannot close archive hard link entry (error: %s)!",
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
    printWarning("unexpected data at end of hard link entry '%S'",StringList_first(&fileNameList,NULL));
  }

  // close source archive entry
  error = Archive_closeEntry(&sourceArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'hard link' entry fail (error: %s)",Error_getText(error));
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
*          newJobOptions            - new job options
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertSpecialEntry(ArchiveHandle    *sourceArchiveHandle,
                                 ArchiveHandle    *destinationArchiveHandle,
                                 const JobOptions *newJobOptions
                                )
{
  String                    fileName;
  FileExtendedAttributeList fileExtendedAttributeList;
  Errors                    error;
  ArchiveEntryInfo          sourceArchiveEntryInfo;
  CryptTypes                cryptType;
  CryptAlgorithms           cryptAlgorithm;
  FileInfo                  fileInfo;
  ArchiveEntryInfo          destinationArchiveEntryInfo;

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
    printError("cannot read 'special' content of archive '%s' (error: %s)!",
               String_cString(sourceArchiveHandle->printableStorageName),
               Error_getText(error)
              );
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(fileName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }

  printInfo(1,"  Convert special   '%s'...",String_cString(fileName));

  // set new crypt settings
  if (CmdOption_isSet(globalOptions.cryptAlgorithms)) cryptAlgorithm = newJobOptions->cryptAlgorithms[0];

  // create new special entry
  error = Archive_newSpecialEntry(&destinationArchiveEntryInfo,
                                  destinationArchiveHandle,
                                  fileName,
                                  &fileInfo,
                                  &fileExtendedAttributeList
                                 );
  if (error != ERROR_NONE)
  {
    printError("cannot create new archive special entry '%s' (error: %s)",
               String_cString(destinationArchiveHandle->printableStorageName),
               Error_getText(error)
              );
    (void)Archive_closeEntry(&sourceArchiveEntryInfo);
    File_doneExtendedAttributes(&fileExtendedAttributeList);
    String_delete(fileName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&destinationArchiveEntryInfo); (void)Archive_closeEntry(&sourceArchiveEntryInfo); File_doneExtendedAttributes(&fileExtendedAttributeList); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }

  // close destination archive entry
  error = Archive_closeEntry(&destinationArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printInfo(1,"FAIL\n");
    printError("cannot close archive special entry (error: %s)!",
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
    printWarning("unexpected data at end of special entry '%S'",fileName);
  }

  // close source archive entry
  error = Archive_closeEntry(&sourceArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'special' entry fail (error: %s)",Error_getText(error));
  }

  // free resources
  File_doneExtendedAttributes(&fileExtendedAttributeList);
  String_delete(fileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : convertMetaEntry
* Purpose: convert a meta entry in archive
* Input  : sourceArchiveHandle      - source archive handle
*          destinationArchiveHandle - destination archive handle
*          newJobUUID               - new job UUID or NULL
*          newScheduleUUID          - new schedule UUID or NULL
*          newCreatedDateTime       - new created date/time or 0
*          newJobOptions            - new job options or NULL
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors convertMetaEntry(ArchiveHandle    *sourceArchiveHandle,
                              ArchiveHandle    *destinationArchiveHandle,
                              const char       *newJobUUID,
                              const char       *newScheduleUUID,
                              uint64           newCreatedDateTime,
                              const JobOptions *newJobOptions
                             )
{
  String           hostName;
  String           userName;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString     (scheduleUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes     archiveType;
  uint64           createdDateTime;
  String           comment;
  Errors           error;
  ArchiveEntryInfo sourceArchiveEntryInfo;
  ArchiveEntryInfo destinationArchiveEntryInfo;

  UNUSED_VARIABLE(newJobOptions);

  // read source meta entry
  hostName = String_new();
  userName = String_new();
  comment  = String_new();
  error = Archive_readMetaEntry(&sourceArchiveEntryInfo,
                                sourceArchiveHandle,
                                hostName,
                                userName,
                                jobUUID,
                                scheduleUUID,
                                &archiveType,
                                &createdDateTime,
                                comment
                               );
  if (error != ERROR_NONE)
  {
    printError("cannot read 'meta' content of archive '%s' (error: %s)!",
               String_cString(sourceArchiveHandle->printableStorageName),
               Error_getText(error)
              );
    String_delete(comment);
    String_delete(userName);
    String_delete(hostName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&sourceArchiveEntryInfo); String_delete(comment); String_delete(userName); String_delete(hostName); return DEBUG_TESTCODE_ERROR(); }

  printInfo(1,"  Convert meta      ...");

  // set new job UUID, schedule UUOD, created date/time comment
  if (!stringIsEmpty(newJobUUID)) String_setCString(jobUUID,newJobUUID);
  if (!stringIsEmpty(newScheduleUUID)) String_setCString(scheduleUUID,newScheduleUUID);
  if (newCreatedDateTime != 0LL) createdDateTime = newCreatedDateTime;
  if (CmdOption_isSet(&globalOptions.comment)) String_set(comment,newJobOptions->comment);

  // create new meta entry
  error = Archive_newMetaEntry(&destinationArchiveEntryInfo,
                               destinationArchiveHandle,
                               String_cString(hostName),
                               String_cString(userName),
                               String_cString(jobUUID),
                               String_cString(scheduleUUID),
                               archiveType,
                               createdDateTime,
                               String_cString(comment)
                              );
  if (error != ERROR_NONE)
  {
    printError("cannot create new archive meta entry '%s' (error: %s)",
               String_cString(destinationArchiveHandle->printableStorageName),
               Error_getText(error)
              );
    (void)Archive_closeEntry(&sourceArchiveEntryInfo);
    String_delete(comment);
    String_delete(userName);
    String_delete(hostName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&destinationArchiveEntryInfo); (void)Archive_closeEntry(&sourceArchiveEntryInfo); String_delete(comment); String_delete(userName); String_delete(hostName); return DEBUG_TESTCODE_ERROR(); }

  // close destination archive entry
  error = Archive_closeEntry(&destinationArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printInfo(1,"FAIL\n");
    printError("cannot close archive meta entry (error: %s)!",
               Error_getText(error)
              );
    (void)Archive_closeEntry(&sourceArchiveEntryInfo);
    String_delete(comment);
    String_delete(userName);
    String_delete(hostName);
    return error;
  }

  printInfo(1,"OK\n");

  // check if all data read
  if (!Archive_eofData(&sourceArchiveEntryInfo))
  {
    printWarning("unexpected data at end of meta entry");
  }

  // close source archive entry
  error = Archive_closeEntry(&sourceArchiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'meta' entry fail (error: %s)",Error_getText(error));
  }

  // free resources
  String_delete(comment);
  String_delete(userName);
  String_delete(hostName);

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
  byte          *buffer;
  uint          archiveIndex;
  ArchiveHandle sourceArchiveHandle;
  EntryMsg      entryMsg;
  Errors        error;

  assert(convertInfo != NULL);

  // init variables
//  printableStorageName = String_new();
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  archiveIndex = 0;

  // convert entries
  while (MsgQueue_get(&convertInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER))
  {
//fprintf(stderr,"%s, %d: %p %d %llu\n",__FILE__,__LINE__,pthread_self(),entryMsg.archiveEntryType,entryMsg.offset);
    if (   ((convertInfo->failError == ERROR_NONE) || !convertInfo->newJobOptions->noStopOnErrorFlag)
//TODO
//         && !isAborted(convertInfo)
       )
    {
      if (archiveIndex < entryMsg.archiveIndex)
      {
        // open source archive
        error = Archive_openHandle(&sourceArchiveHandle,
                                   entryMsg.archiveHandle
                                  );
        if (error != ERROR_NONE)
        {
          printError("cannot open archive '%s' (error: %s)!",
                     String_cString(entryMsg.archiveHandle->printableStorageName),
                     Error_getText(error)
                    );
          if (convertInfo->failError == ERROR_NONE) convertInfo->failError = error;
          freeEntryMsg(&entryMsg,NULL);
          break;
        }

        // store current archive index
        archiveIndex = entryMsg.archiveIndex;
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
        printError("cannot read storage '%s' (error: %s)!",
                   String_cString(sourceArchiveHandle.printableStorageName),
                   Error_getText(error)
                  );
        if (convertInfo->failError == ERROR_NONE) convertInfo->failError = error;
        freeEntryMsg(&entryMsg,NULL);
        break;
      }

      switch (entryMsg.archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_NONE:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNREACHABLE();
          #endif /* NDEBUG */
          break; /* not reached */
        case ARCHIVE_ENTRY_TYPE_FILE:
          error = convertFileEntry(&sourceArchiveHandle,
                                   &convertInfo->destinationArchiveHandle,
                                   convertInfo->newJobOptions,
                                   buffer,
                                   BUFFER_SIZE
                                  );
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          error = convertImageEntry(&sourceArchiveHandle,
                                    &convertInfo->destinationArchiveHandle,
                                    convertInfo->newJobOptions,
                                    buffer,
                                    BUFFER_SIZE
                                   );
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          error = convertDirectoryEntry(&sourceArchiveHandle,
                                        &convertInfo->destinationArchiveHandle,
                                        convertInfo->newJobOptions
                                       );
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          error = convertLinkEntry(&sourceArchiveHandle,
                                   &convertInfo->destinationArchiveHandle,
                                   convertInfo->newJobOptions
                                  );
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          error = convertHardLinkEntry(&sourceArchiveHandle,
                                       &convertInfo->destinationArchiveHandle,
                                       convertInfo->newJobOptions,
                                       buffer,
                                       BUFFER_SIZE
                                      );
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          error = convertSpecialEntry(&sourceArchiveHandle,
                                      &convertInfo->destinationArchiveHandle,
                                      convertInfo->newJobOptions
                                     );
          break;
        case ARCHIVE_ENTRY_TYPE_META:
          error = convertMetaEntry(&sourceArchiveHandle,
                                   &convertInfo->destinationArchiveHandle,
                                   convertInfo->newJobUUID,
                                   convertInfo->newScheduleUUID,
                                   convertInfo->newCreatedDateTime,
                                   convertInfo->newJobOptions
                                  );
          break;
        case ARCHIVE_ENTRY_TYPE_SIGNATURE:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNREACHABLE();
          #else
            error = Archive_skipNextEntry(&sourceArchiveHandle);
          #endif /* NDEBUG */
          break;
        case ARCHIVE_ENTRY_TYPE_UNKNOWN:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNREACHABLE();
          #endif /* NDEBUG */
          break; /* not reached */
      }
      if (error != ERROR_NONE)
      {
        if (convertInfo->failError == ERROR_NONE) convertInfo->failError = error;
        freeEntryMsg(&entryMsg,NULL);
        break;
      }
    }

    freeEntryMsg(&entryMsg,NULL);
  }

  // close archive
  if (archiveIndex != 0)
  {
    Archive_close(&sourceArchiveHandle,FALSE);
  }

  // discard processing all other entries
  while (MsgQueue_get(&convertInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER))
  {
    freeEntryMsg(&entryMsg,NULL);
  }

  // free resources
  free(buffer);
}

/***********************************************************************\
* Name   : convertArchive
* Purpose: convert archive
* Input  : convertInfo      - convert info
*          storageSpecifier - storage specifier
*          archiveName      - archive name (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors convertArchive(ConvertInfo      *convertInfo,
                            StorageSpecifier *storageSpecifier,
                            ConstString      archiveName
                           )
{
  AutoFreeList           autoFreeList;
  String                 printableStorageName;
  uint                   convertThreadCount;
  Errors                 error;
  String                 baseName;
  Thread                 storageThread;
  uint                   i;
  ArchiveHandle          sourceArchiveHandle;
  CryptSignatureStates   sourceAllCryptSignatureState;
  uint64                 sourceLastSignatureOffset;
  ArchiveEntryTypes      archiveEntryType;
  const ArchiveCryptInfo *archiveCryptInfo;
  uint64                 offset;
  EntryMsg               entryMsg;

  assert(convertInfo != NULL);
  assert(convertInfo->newJobOptions != NULL);
  assert(storageSpecifier != NULL);

  // init variables
  AutoFree_init(&autoFreeList);
  printableStorageName = String_new();
  AUTOFREE_ADD(&autoFreeList,printableStorageName,{ String_delete(printableStorageName); });

  // Note: still not supported
  if (convertInfo->newJobOptions->archivePartSize > 0LL)
  {
    printWarning(_("Archive part size not supported for convert - ignored"));
  }
//TODO
//  convertInfo->newJobOptions->archivePartSize = 0LL;

  // set printable storage name
  Storage_getPrintableName(printableStorageName,storageSpecifier,archiveName);

  // init storage
  error = Storage_init(&convertInfo->storageInfo,
                       NULL,  // masterIO
                       storageSpecifier,
                       convertInfo->newJobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK_(NULL,NULL),  // updateStatusInfo
                       CALLBACK_(NULL,NULL),  // getPassword
                       CALLBACK_(NULL,NULL),  // requestVolume
                       CALLBACK_(NULL,NULL),  // isPause
                       CALLBACK_(NULL,NULL),  // isAborted
                       NULL  // logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError("cannot initialize storage '%s' (error: %s)!",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&convertInfo->storageInfo,{ (void)Storage_done(&convertInfo->storageInfo); });

  // check if storage exists
  if (!Storage_exists(&convertInfo->storageInfo,archiveName))
  {
    printError("achive not found '%s'!",
               String_cString(printableStorageName)
              );
    AutoFree_cleanup(&autoFreeList);
    return ERROR_ARCHIVE_NOT_FOUND;
  }

  // open source archive
  error = Archive_open(&sourceArchiveHandle,
                       &convertInfo->storageInfo,
                       archiveName,
                       NULL,  // deltaSourceList,
                       CALLBACK_(convertInfo->getNamePasswordFunction,convertInfo->getNamePasswordUserData),
                       convertInfo->logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError("cannot open archive '%s' (error: %s)!",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Archive_close(&sourceArchiveHandle,FALSE); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&sourceArchiveHandle,{ Archive_close(&sourceArchiveHandle,FALSE); });

  // check signatures
  sourceAllCryptSignatureState = CRYPT_SIGNATURE_STATE_NONE;
  sourceLastSignatureOffset    = Archive_tell(&sourceArchiveHandle);
  if (!convertInfo->newJobOptions->skipVerifySignaturesFlag)
  {
    error = Archive_verifySignatures(&sourceArchiveHandle,
                                     &sourceAllCryptSignatureState
                                    );
    if (error != ERROR_NONE)
    {
      if (!convertInfo->newJobOptions->forceVerifySignaturesFlag && (Error_getCode(error) == ERROR_CODE_NO_PUBLIC_SIGNATURE_KEY))
      {
        sourceAllCryptSignatureState = CRYPT_SIGNATURE_STATE_SKIPPED;
      }
      else
      {
        // signature error
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }
    if (!Crypt_isValidSignatureState(sourceAllCryptSignatureState))
    {
      if (convertInfo->newJobOptions->forceVerifySignaturesFlag)
      {
        // signature error
        printError("invalid signature in '%s'!",
                   String_cString(printableStorageName)
                  );
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_SIGNATURE;
      }
      else
      {
        // print signature warning
        printWarning("invalid signature in '%s'!",
                     String_cString(printableStorageName)
                    );
      }
    }
  }

  // create destination archive name
  baseName = File_getBaseName(String_new(),(archiveName != NULL) ? archiveName : storageSpecifier->archiveName);
  if (!String_isEmpty(convertInfo->newJobOptions->destination))
  {
    File_setFileName(convertInfo->archiveName,convertInfo->newJobOptions->destination);
    File_appendFileName(convertInfo->archiveName,baseName);
  }
  else
  {
    File_setFileName(convertInfo->archiveName,(archiveName != NULL) ? archiveName : storageSpecifier->archiveName);
  }
  String_delete(baseName);

  // create new archive
  error = Archive_create(&convertInfo->destinationArchiveHandle,
                         NULL,  // hostName
                         NULL,  // userName
                         &convertInfo->storageInfo,
                         NULL,  // archiveName,
                         INDEX_ID_NONE,  // uuidId,
                         INDEX_ID_NONE,  // entityId,
                         NULL,  // jobUUID,
                         NULL,  // scheduleUUID,
//TODO
                         NULL,  // deltaSourceList,
                         sourceArchiveHandle.archiveType,
                         FALSE, // dryRun
                         Misc_getCurrentDateTime(),
                         FALSE,  // createMeta
                         &globalOptions.cryptNewPassword,
                         CALLBACK_(NULL,NULL),  // archiveInitFunction
                         CALLBACK_(NULL,NULL),  // archiveDoneFunction
                         CALLBACK_(NULL,NULL),  // archiveGetSizeFunction
                         CALLBACK_(NULL,NULL),  // archiveTestFunction
                         CALLBACK_(archiveStore,convertInfo),
                         CALLBACK_(convertInfo->getNamePasswordFunction,convertInfo->getNamePasswordUserData),
                         convertInfo->logHandle
                        );
  if (error != ERROR_NONE)
  {
    printError("cannot create temporary storage for '%s' (error: %s)",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Archive_close(&convertInfo->destinationArchiveHandle,FALSE); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&convertInfo->destinationArchiveHandle,{ Archive_close(&convertInfo->destinationArchiveHandle,FALSE); });

//TODO: really required? If convert is done for each archive storage can be done as the final step without a separated thread
  // start storage thread
  if (!Thread_init(&storageThread,"BAR storage",globalOptions.niceLevel,storageThreadCode,convertInfo))
  {
    HALT_FATAL_ERROR("Cannot initialize storage thread!");
  }
  AUTOFREE_ADD(&autoFreeList,&storageThread,{ MsgQueue_setEndOfMsg(&convertInfo->storageMsgQueue); Thread_join(&storageThread); Thread_done(&storageThread); });

  // start convert threads
  MsgQueue_reset(&convertInfo->entryMsgQueue);
  convertThreadCount = (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores();
  for (i = 0; i < convertThreadCount; i++)
  {
    ThreadPool_run(&workerThreadPool,convertThreadCode,convertInfo);
  }

  // output info
  printInfo(0,
            "Convert storage '%s'%s",
            String_cString(printableStorageName),
            !isPrintInfo(1) ? "..." : ":\n"
           );

  // read archive entries
  while (   (convertInfo->failError == ERROR_NONE)
         && !Archive_eof(&sourceArchiveHandle,
                         ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS|(isPrintInfo(3)
                                                            ? ARCHIVE_FLAG_PRINT_UNKNOWN_CHUNKS
                                                            : ARCHIVE_FLAG_NONE
                                                          )
                        )
        )
  {
    // get next archive entry type
    error = Archive_getNextArchiveEntry(&sourceArchiveHandle,
                                        &archiveEntryType,
                                        &archiveCryptInfo,
                                        &offset,
                                        ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS|(isPrintInfo(3)
                                                                           ? ARCHIVE_FLAG_PRINT_UNKNOWN_CHUNKS
                                                                           : ARCHIVE_FLAG_NONE
                                                                         )
                                       );
    if (error != ERROR_NONE)
    {
      printError("cannot read next entry in archive '%s' (error: %s)!",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      if (convertInfo->failError == ERROR_NONE) convertInfo->failError = error;
      break;
    }
    DEBUG_TESTCODE() { convertInfo->failError = DEBUG_TESTCODE_ERROR(); break; }
//TODO: remove
//fprintf(stderr,"%s, %d: archiveEntryType=%s\n",__FILE__,__LINE__,Archive_archiveEntryTypeToString(archiveEntryType,NULL));

    // convert entries
    if (archiveEntryType != ARCHIVE_ENTRY_TYPE_SIGNATURE)
    {
      // send entry to convert threads
//TODO: increment on multiple archives and when threads are not restarted each time
      entryMsg.archiveIndex     = 1;
      entryMsg.archiveHandle    = &sourceArchiveHandle;
      entryMsg.archiveEntryType = archiveEntryType;
      entryMsg.archiveCryptInfo = archiveCryptInfo;
      entryMsg.offset           = offset;
      if (!MsgQueue_put(&convertInfo->entryMsgQueue,&entryMsg,sizeof(entryMsg)))
      {
        HALT_INTERNAL_ERROR("Send message to convert threads fail!");
      }

      // skip entry
      error = Archive_skipNextEntry(&sourceArchiveHandle);
      if (error != ERROR_NONE)
      {
        if (convertInfo->failError == ERROR_NONE) convertInfo->failError = error;
        break;
      }
    }
    else
    {
      if (!convertInfo->newJobOptions->skipVerifySignaturesFlag)
      {
        // check signature
        error = Archive_verifySignatureEntry(&sourceArchiveHandle,sourceLastSignatureOffset,&sourceAllCryptSignatureState);
      }
      else
      {
        // skip signature
        error = Archive_skipNextEntry(&sourceArchiveHandle);
      }
      if (error != ERROR_NONE)
      {
        if (convertInfo->failError == ERROR_NONE) convertInfo->failError = error;
        break;
      }
      sourceLastSignatureOffset = Archive_tell(&sourceArchiveHandle);
    }
  }

  // wait for convert threads
  MsgQueue_setEndOfMsg(&convertInfo->entryMsgQueue);
  ThreadPool_joinAll(&workerThreadPool);

  // close destination archive
  AUTOFREE_REMOVE(&autoFreeList,&convertInfo->destinationArchiveHandle);
  error = Archive_close(&convertInfo->destinationArchiveHandle,convertInfo->failError == ERROR_NONE);
  if (error != ERROR_NONE)
  {
    if (!isPrintInfo(1)) printInfo(0,"FAIL\n");
    printError("cannot close archive '%s' (error: %s)",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    if (convertInfo->failError == ERROR_NONE) convertInfo->failError = error;
    return error;
  }
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  // wait for storage thread
  AUTOFREE_REMOVE(&autoFreeList,&storageThread);
  MsgQueue_setEndOfMsg(&convertInfo->storageMsgQueue);
  if (!Thread_join(&storageThread))
  {
    HALT_INTERNAL_ERROR("Cannot stop storage thread!");
  }
  Thread_done(&storageThread);

  // close source archive
  AUTOFREE_REMOVE(&autoFreeList,&sourceArchiveHandle);
  (void)Archive_close(&sourceArchiveHandle,FALSE);

  // done storage
  (void)Storage_done(&convertInfo->storageInfo);

  // output info
  if (!isPrintInfo(1)) printInfo(0,
                                 "%s",
                                 (convertInfo->failError == ERROR_NONE)
                                   ? "OK\n"
                                   : "FAIL!\n"
                                );

  // output signature error/warning
  if (!Crypt_isValidSignatureState(sourceAllCryptSignatureState))
  {
    if (convertInfo->newJobOptions->forceVerifySignaturesFlag)
    {
      printError("invalid signature in '%s'!",
                 String_cString(printableStorageName)
                );
      if (convertInfo->failError == ERROR_NONE) convertInfo->failError = ERROR_INVALID_SIGNATURE;
    }
    else
    {
      printWarning("invalid signature in '%s'!",
                   String_cString(printableStorageName)
                  );
    }
  }

  // free resources
  String_delete(printableStorageName);
  AutoFree_done(&autoFreeList);

  return convertInfo->failError;
}

/*---------------------------------------------------------------------*/

Errors Command_convert(const StringList        *storageNameList,
                       const char              *newJobUUID,
                       const char              *newScheduleUUID,
                       uint64                  newCreatedDateTime,
                       JobOptions              *newJobOptions,
                       GetNamePasswordFunction getNamePasswordFunction,
                       void                    *getNamePasswordUserData,
                       LogHandle               *logHandle
                      )
{
  StorageSpecifier           storageSpecifier;
  ConvertInfo                convertInfo;
  StringNode                 *stringNode;
  String                     storageName;
  Errors                     failError;
  bool                       someStorageFound;
  Errors                     error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  String                     fileName;
  FileInfo                   fileInfo;

  assert(storageNameList != NULL);
  assert(newJobOptions != NULL);

  // init variables
  Storage_initSpecifier(&storageSpecifier);

  // init convert info
  initConvertInfo(&convertInfo,
                  newJobUUID,
                  newScheduleUUID,
                  newCreatedDateTime,
                  newJobOptions,
                  CALLBACK_(getNamePasswordFunction,getNamePasswordUserData),
                  logHandle
                 );

  failError        = ERROR_NONE;
  someStorageFound = FALSE;
  STRINGLIST_ITERATE(storageNameList,stringNode,storageName)
  {
    // parse storage name
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error != ERROR_NONE)
    {
      printError("invalid storage '%s' (error: %s)!",
                 String_cString(storageName),
                 Error_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      continue;
    }
    DEBUG_TESTCODE() { failError = DEBUG_TESTCODE_ERROR(); break; }

    error = ERROR_UNKNOWN;

    if (error != ERROR_NONE)
    {
      if (String_isEmpty(storageSpecifier.archivePatternString))
      {
        // convert archive content
        error = convertArchive(&convertInfo,
                               &storageSpecifier,
                               NULL
                              );
        someStorageFound = TRUE;
      }
    }
    if (error != ERROR_NONE)
    {
      error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                        &storageSpecifier,
                                        NULL,  // pathName
                                        newJobOptions,
                                        SERVER_CONNECTION_PRIORITY_HIGH
                                       );
      if (error == ERROR_NONE)
      {
        fileName = String_new();
        while (!Storage_endOfDirectoryList(&storageDirectoryListHandle))
        {
          // read next directory entry
          error = Storage_readDirectoryList(&storageDirectoryListHandle,fileName,&fileInfo);
          if (error != ERROR_NONE)
          {
            continue;
          }

          // match pattern
          if (!String_isEmpty(storageSpecifier.archivePatternString))
          {
            if (!Pattern_match(&storageSpecifier.archivePattern,fileName,STRING_BEGIN,PATTERN_MATCH_MODE_EXACT,NULL,NULL))
            {
              continue;
            }
          }

          // convert archive content
          if (   (fileInfo.type == FILE_TYPE_FILE)
              || (fileInfo.type == FILE_TYPE_LINK)
              || (fileInfo.type == FILE_TYPE_HARDLINK)
             )
          {
            error = convertArchive(&convertInfo,
                                   &storageSpecifier,
                                   fileName
                                  );
            if (error != ERROR_NONE)
            {
              if (failError == ERROR_NONE) failError = error;
            }
          }
          someStorageFound = TRUE;
        }
        String_delete(fileName);

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
  if ((failError == ERROR_NONE) && !StringList_isEmpty(storageNameList) && !someStorageFound)
  {
    printError("no matching storage files found!");
    failError = ERROR_FILE_NOT_FOUND_;
  }

  // get error
// TODO:
#if 0
  if ((isAbortedFunction == NULL) || !isAbortedFunction(isAbortedUserData))
  {
    error = convertInfo.failError;
  }
  else
  {
    error = ERROR_ABORTED;
  }
#else
error = convertInfo.failError;
#endif

  // done convert info
  doneConvertInfo(&convertInfo);

  // free resources
  Storage_doneSpecifier(&storageSpecifier);

  // output info
  if (error != ERROR_NONE)
  {
    printInfo(1,tr("Convert fail: %s\n"),Error_getText(error));
  }

  return failError;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
