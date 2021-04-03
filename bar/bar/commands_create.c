/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive create functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "common/global.h"
#include "common/autofree.h"
#include "common/strings.h"
#include "common/stringmaps.h"
#include "common/lists.h"
#include "common/stringlists.h"
#include "common/arrays.h"
#include "common/threads.h"
#include "common/msgqueues.h"
#include "common/semaphores.h"
#include "common/dictionaries.h"
#include "common/misc.h"
#include "common/patterns.h"
#include "common/patternlists.h"
#include "common/files.h"
#include "common/devices.h"
#include "common/filesystems.h"
#include "common/fragmentlists.h"

#include "errors.h"
#include "entrylists.h"
#include "archive.h"
#include "deltasources.h"
#include "crypt.h"
#include "storage.h"
#include "common/database.h"
#include "continuous.h"
#include "bar_global.h"
#include "bar.h"

#include "commands_create.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define MAX_ENTRY_MSG_QUEUE 256

// file data buffer size
#define BUFFER_SIZE                   (64*1024)

#define INCREMENTAL_LIST_FILE_ID      "BAR incremental list"
#define INCREMENTAL_LIST_FILE_VERSION 1

typedef enum
{
  FORMAT_MODE_ARCHIVE_FILE_NAME,
  FORMAT_MODE_PATTERN,
} FormatModes;

typedef enum
{
  INCREMENTAL_FILE_STATE_UNKNOWN,
  INCREMENTAL_FILE_STATE_OK,
  INCREMENTAL_FILE_STATE_ADDED,
} IncrementalFileStates;

/***************************** Datatypes *******************************/

// incremental data info prefix
typedef struct
{
  IncrementalFileStates state;
  FileCast              cast;
} IncrementalListInfo;

// create info
typedef struct
{
  StorageInfo                 storageInfo;                          // storage info
  IndexHandle                 *indexHandle;
  ConstString                 jobUUID;                              // unique job id to store or NULL
  ConstString                 scheduleUUID;                         // unique schedule id to store or NULL
  ConstString                 scheduleTitle;                        // schedule title or NULL
  ConstString                 scheduleCustomText;                   // schedule custom text or NULL
  const EntryList             *includeEntryList;                    // list of included entries
  const PatternList           *excludePatternList;                  // list of exclude patterns
  JobOptions                  *jobOptions;
  ArchiveTypes                archiveType;                          // archive type to create
  uint64                      createdDateTime;                      // date/time of created [s]
  StorageFlags                storageFlags;                         // storage flags; see STORAGE_FLAGS_...

  LogHandle                   *logHandle;                           // log handle

  bool                        partialFlag;                          // TRUE for create incremental/differential archive
  Dictionary                  namesDictionary;                      // dictionary with files (used for incremental/differental backup)
  bool                        storeIncrementalFileInfoFlag;         // TRUE to store incremental file data

  MsgQueue                    entryMsgQueue;                        // queue with entries to store

  ArchiveHandle               archiveHandle;

  bool                        collectorSumThreadExitedFlag;         // TRUE iff collector sum thread exited

  MsgQueue                    storageMsgQueue;                      // queue with waiting storage files
  Semaphore                   storageInfoLock;                      // lock semaphore for storage info
  struct
  {
    uint                      count;                                // number of current storage files
    uint64                    bytes;                                // number of bytes in current storage files
  }                           storage;
  bool                        storageThreadExitFlag;
  StringList                  storageFileList;                      // list with stored storage files

  Errors                      failError;                            // failure error

  IsPauseFunction             isPauseCreateFunction;                // pause create check callback (can be NULL)
  void                        *isPauseCreateUserData;               // user data for pause create check

  IsAbortedFunction           isAbortedFunction;                    // abort create check callback (can be NULL)
  void                        *isAbortedUserData;                   // user data for abort create check

  FragmentList                statusInfoFragmentList;               // status info fragment list

  StatusInfoFunction          statusInfoFunction;                   // status info callback
  void                        *statusInfoUserData;                  // user data for status info call back
  Semaphore                   statusInfoLock;                       // status info lock
  StatusInfo                  statusInfo;                           // status info
  const FragmentNode          *statusInfoCurrentFragmentNode;       // current fragment node in status info
  uint64                      statusInfoCurrentLastUpdateTimestamp; // timestamp of last update current fragment node
} CreateInfo;

// hard link info
typedef struct
{
  uint       count;                                                 // number of hard links
  StringList nameList;                                              // list of hard linked names
  FileInfo   fileInfo;
} HardLinkInfo;

// entry message, send from collector thread -> main
typedef struct
{
  EntryTypes entryType;
  FileTypes  fileType;
  String     name;                                                // file/image/directory/link/special name
  StringList nameList;                                            // list of hard link names
  union
  {
    FileInfo   fileInfo;
    DeviceInfo deviceInfo;
  };
  uint       fragmentNumber;                                      // fragment number [0..n-1]
  uint       fragmentCount;                                       // fragment count
  uint64     fragmentOffset;
  uint64     fragmentSize;
} EntryMsg;

// storage message, send from create threads -> storage thread
typedef struct
{
  IndexId      uuidId;
  IndexId      entityId;
  ArchiveTypes archiveType;
  IndexId      storageId;
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

  UNUSED_VARIABLE(userData);

  switch (entryMsg->fileType)
  {
    case FILE_TYPE_FILE:
    case FILE_TYPE_DIRECTORY:
    case FILE_TYPE_LINK:
    case FILE_TYPE_SPECIAL:
      StringList_done(&entryMsg->nameList);
      String_delete(entryMsg->name);
      break;
    case FILE_TYPE_HARDLINK:
      StringList_done(&entryMsg->nameList);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
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
* Name   : initCreateInfo
* Purpose: initialize create info
* Input  : createInfo                 - create info variable
*          indexHandle                - index handle
*          jobUUID                    - unique job id to store or NULL
*          scheduleUUID               - unique schedule id to store or NULL
*          scheduleTitle              - schedule title
*          scheduleCustomText         - schedule custome text or NULL
*          includeEntryList           - include entry list
*          excludePatternList         - exclude pattern list
*          jobOptions                 - job options
*          archiveType                - archive type; see ArchiveTypes
*                                       (normal/full/incremental)
*          createdDateTime            - date/time of created [s]
*          storageFlags               - storage flags; see STORAGE_FLAGS_...
*          isPauseCreateFunction      - is pause check callback (can
*                                       be NULL)
*          isPauseCreateUserData      - user data for is pause create
*                                       check
*          statusInfoFunction         - status info call back function
*                                       (can be NULL)
*          statusInfoUserData         - user data for status info
*                                       function
*          isAbortedFunction          - is abort check callback (can be
*                                       NULL)
*          isAbortedUserData          - user data for is aborted check
*          logHandle                  - log handle (can be NULL)
* Output : createInfo - initialized create info variable
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initCreateInfo(CreateInfo         *createInfo,
                          IndexHandle        *indexHandle,
                          ConstString        jobUUID,
                          ConstString        scheduleUUID,
                          ConstString        scheduleTitle,
                          ConstString        scheduleCustomText,
                          const EntryList    *includeEntryList,
                          const PatternList  *excludePatternList,
                          JobOptions         *jobOptions,
                          ArchiveTypes       archiveType,
                          uint64             createdDateTime,
                          StorageFlags       storageFlags,
                          IsPauseFunction    isPauseCreateFunction,
                          void               *isPauseCreateUserData,
                          StatusInfoFunction statusInfoFunction,
                          void               *statusInfoUserData,
                          IsAbortedFunction  isAbortedFunction,
                          void               *isAbortedUserData,
                          LogHandle          *logHandle
                         )
{
  assert(createInfo != NULL);

  // init variables
  createInfo->indexHandle                          = indexHandle;
  createInfo->jobUUID                              = jobUUID;
  createInfo->scheduleUUID                         = scheduleUUID;
  createInfo->includeEntryList                     = includeEntryList;
  createInfo->excludePatternList                   = excludePatternList;
  createInfo->jobOptions                           = jobOptions;
  createInfo->scheduleTitle                        = scheduleTitle;
  createInfo->scheduleCustomText                   = scheduleCustomText;
  createInfo->createdDateTime                      = createdDateTime;
  createInfo->storageFlags                         = storageFlags;

  createInfo->logHandle                            = logHandle;

  createInfo->storeIncrementalFileInfoFlag         = FALSE;

  createInfo->collectorSumThreadExitedFlag         = FALSE;

  createInfo->storage.count                        = 0;
  createInfo->storage.bytes                        = 0LL;
  createInfo->storageThreadExitFlag                = FALSE;
  StringList_init(&createInfo->storageFileList);

  createInfo->failError                            = ERROR_NONE;

  createInfo->statusInfoFunction                   = statusInfoFunction;
  createInfo->statusInfoUserData                   = statusInfoUserData;
  FragmentList_init(&createInfo->statusInfoFragmentList);
  createInfo->statusInfoCurrentFragmentNode        = NULL;
  createInfo->statusInfoCurrentLastUpdateTimestamp = 0LL;

  createInfo->isPauseCreateFunction                = isPauseCreateFunction;
  createInfo->isPauseCreateUserData                = isPauseCreateUserData;

  createInfo->isAbortedFunction                    = isAbortedFunction;
  createInfo->isAbortedUserData                    = isAbortedUserData;

  initStatusInfo(&createInfo->statusInfo);

  // get archive type
  if (archiveType != ARCHIVE_TYPE_NONE)
  {
    createInfo->archiveType = archiveType;
  }
  else
  {
    createInfo->archiveType = jobOptions->archiveType;
  }

  // set partial flag
  createInfo->partialFlag =    (createInfo->archiveType == ARCHIVE_TYPE_INCREMENTAL)
                            || (createInfo->archiveType == ARCHIVE_TYPE_DIFFERENTIAL);
//TODO
//                            || (createInfo->archiveType == ARCHIVE_TYPE_CONTINUOUS);

  // init entry name queue, storage queue
  if (!MsgQueue_init(&createInfo->entryMsgQueue,MAX_ENTRY_MSG_QUEUE))
  {
    HALT_FATAL_ERROR("Cannot initialize entry message queue!");
  }
  if (!MsgQueue_init(&createInfo->storageMsgQueue,0))
  {
    HALT_FATAL_ERROR("Cannot initialize storage message queue!");
  }

  // init locks
  if (!Semaphore_init(&createInfo->storageInfoLock,SEMAPHORE_TYPE_BINARY))
  {
    HALT_FATAL_ERROR("Cannot initialize storage semaphore!");
  }
  if (!Semaphore_init(&createInfo->statusInfoLock,SEMAPHORE_TYPE_BINARY))
  {
    HALT_FATAL_ERROR("Cannot initialize status info semaphore!");
  }

  DEBUG_ADD_RESOURCE_TRACE(createInfo,CreateInfo);
}

/***********************************************************************\
* Name   : doneCreateInfo
* Purpose: deinitialize create info
* Input  : createInfo - create info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneCreateInfo(CreateInfo *createInfo)
{
  assert(createInfo != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(createInfo,CreateInfo);

  Semaphore_done(&createInfo->statusInfoLock);
  Semaphore_done(&createInfo->storageInfoLock);

  MsgQueue_done(&createInfo->storageMsgQueue,(MsgQueueMsgFreeFunction)freeStorageMsg,NULL);
  MsgQueue_done(&createInfo->entryMsgQueue,(MsgQueueMsgFreeFunction)freeEntryMsg,NULL);

  doneStatusInfo(&createInfo->statusInfo);
  FragmentList_done(&createInfo->statusInfoFragmentList);
  StringList_done(&createInfo->storageFileList);
}

/***********************************************************************\
* Name   : isAborted
* Purpose: check if aborted
* Input  : createInfo - create info
* Output : -
* Return : TRUE iff aborted
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isAborted(const CreateInfo *createInfo)
{
  assert(createInfo != NULL);

  return (createInfo->isAbortedFunction != NULL) && createInfo->isAbortedFunction(createInfo->isAbortedUserData);
}

/***********************************************************************\
* Name   : pauseCreate
* Purpose: pause create
* Input  : createInfo - create info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void pauseCreate(const CreateInfo *createInfo)
{
  assert(createInfo != NULL);

  while (   ((createInfo->isPauseCreateFunction != NULL) && createInfo->isPauseCreateFunction(createInfo->isPauseCreateUserData))
         && !isAborted(createInfo)
        )
  {
    Misc_udelay(500LL*US_PER_MS);
  }
}

/***********************************************************************\
* Name   : readIncrementalList
* Purpose: read data of incremental list from file
* Input  : createInfo      - create info
*          fileName        - file name
*          namesDictionary - names dictionary variable
* Output : -
* Return : ERROR_NONE if incremental list read in files dictionary,
*          error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors readIncrementalList(const CreateInfo *createInfo,
                                 ConstString      fileName,
                                 Dictionary       *namesDictionary
                                )
{
  #define MAX_KEY_DATA (64*1024)

  void                *keyData;
  Errors              error;
  FileHandle          fileHandle;
  char                id[32];
  uint16              version;
  IncrementalListInfo incrementalListInfo;
  uint16              keyLength;

  assert(createInfo != NULL);
  assert(fileName != NULL);
  assert(namesDictionary != NULL);

  // initialize variables
  keyData = malloc(MAX_KEY_DATA);
  if (keyData == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init variables
  Dictionary_clear(namesDictionary);

  // open file
  error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    free(keyData);
    return error;
  }

  // read and check header
  error = File_read(&fileHandle,id,sizeof(id),NULL);
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    free(keyData);
    return error;
  }
  if (!stringEquals(id,INCREMENTAL_LIST_FILE_ID))
  {
    File_close(&fileHandle);
    free(keyData);
    return ERROR_NOT_AN_INCREMENTAL_FILE;
  }
  error = File_read(&fileHandle,&version,sizeof(version),NULL);
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    free(keyData);
    return error;
  }
  if (version != INCREMENTAL_LIST_FILE_VERSION)
  {
    File_close(&fileHandle);
    free(keyData);
    return ERROR_WRONG_INCREMENTAL_FILE_VERSION;
  }

  // read entries
  while (!File_eof(&fileHandle) && !isAborted(createInfo))
  {
    // read entry
    incrementalListInfo.state = INCREMENTAL_FILE_STATE_UNKNOWN;
    error = File_read(&fileHandle,&incrementalListInfo.cast,sizeof(incrementalListInfo.cast),NULL);
    if (error != ERROR_NONE) break;
    error = File_read(&fileHandle,&keyLength,sizeof(keyLength),NULL);
    if (error != ERROR_NONE) break;
    error = File_read(&fileHandle,keyData,keyLength,NULL);
    if (error != ERROR_NONE) break;

    // store in dictionary
    Dictionary_add(namesDictionary,
                   keyData,
                   keyLength,
                   &incrementalListInfo,
                   sizeof(incrementalListInfo)
                  );
  }

  // close file
  File_close(&fileHandle);

  // free resources
  free(keyData);

  return error;
}

/***********************************************************************\
* Name   : writeIncrementalList
* Purpose: write incremental list data to file
* Input  : createInfo      - create info
*          fileName        - file name
*          namesDictionary - names dictionary
* Output : -
* Return : ERROR_NONE if incremental list file written, error code
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors writeIncrementalList(const CreateInfo *createInfo,
                                  ConstString      fileName,
                                  Dictionary       *namesDictionary
                                 )
{
  String                    directoryName;
  String                    tmpFileName;
  Errors                    error;
  FileHandle                fileHandle;
  char                      id[32];
  uint16                    version;
  DictionaryIterator        dictionaryIterator;
  const void                *keyData;
  ulong                     keyLength;
  void                      *data;
  ulong                     length;
  uint16                    n;
  const IncrementalListInfo *incrementalListInfo;

  assert(createInfo != NULL);
  assert(fileName != NULL);
  assert(namesDictionary != NULL);

  // get directory of .bid file
  directoryName = File_getDirectoryName(String_new(),fileName);

  // create directory if not existing
  if (!String_isEmpty(directoryName))
  {
    if      (!File_exists(directoryName))
    {
      error = File_makeDirectory(directoryName,FILE_DEFAULT_USER_ID,FILE_DEFAULT_GROUP_ID,FILE_DEFAULT_PERMISSION);
      if (error != ERROR_NONE)
      {
        String_delete(directoryName);
        return error;
      }
    }
    else if (!File_isDirectory(directoryName))
    {
      error = ERRORX_(NOT_A_DIRECTORY,0,"%s",String_cString(directoryName));
      String_delete(directoryName);
      return error;
    }
  }

  // check write permission
  if (!File_isWritable(directoryName))
  {
    String_delete(directoryName);
    return ERRORX_(WRITE_FILE,0,"%s",String_cString(fileName));
  }

  // get temporary name for new .bid file
  tmpFileName = String_new();
  error = File_getTmpFileName(tmpFileName,"bid",directoryName);
  if (error != ERROR_NONE)
  {
    String_delete(tmpFileName);
    String_delete(directoryName);
    return error;
  }

  // open file new .bid file
  error = File_open(&fileHandle,tmpFileName,FILE_OPEN_CREATE);
  if (error != ERROR_NONE)
  {
    File_delete(tmpFileName,FALSE);
    String_delete(tmpFileName);
    String_delete(directoryName);
    return error;
  }

  // write header
  memClear(id,sizeof(id));
  strncpy(id,INCREMENTAL_LIST_FILE_ID,sizeof(id)-1);
  error = File_write(&fileHandle,id,sizeof(id));
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    File_delete(tmpFileName,FALSE);
    String_delete(tmpFileName);
    String_delete(directoryName);
    return error;
  }
  version = INCREMENTAL_LIST_FILE_VERSION;
  error = File_write(&fileHandle,&version,sizeof(version));
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    File_delete(tmpFileName,FALSE);
    String_delete(tmpFileName);
    String_delete(directoryName);
    return error;
  }

  // write entries
  Dictionary_initIterator(&dictionaryIterator,namesDictionary);
  while (   Dictionary_getNext(&dictionaryIterator,
                               &keyData,
                               &keyLength,
                               &data,
                               &length
                              )
         && !isAborted(createInfo)
        )
  {
    assert(keyData != NULL);
    assert(keyLength <= 65535);
    assert(data != NULL);
    assert(length == sizeof(IncrementalListInfo));

    incrementalListInfo = (IncrementalListInfo*)data;

    error = File_write(&fileHandle,&incrementalListInfo->cast,sizeof(incrementalListInfo->cast));
    if (error != ERROR_NONE) break;
    n = (uint16)keyLength;
    error = File_write(&fileHandle,&n,sizeof(n));
    if (error != ERROR_NONE) break;
    error = File_write(&fileHandle,keyData,n);
    if (error != ERROR_NONE) break;
  }
  Dictionary_doneIterator(&dictionaryIterator);

  // close file .bid file
  File_close(&fileHandle);
  if (error != ERROR_NONE)
  {
    File_delete(tmpFileName,FALSE);
    File_delete(tmpFileName,FALSE);
    String_delete(tmpFileName);
    String_delete(directoryName);
    return error;
  }

  // rename files
  if (!isAborted(createInfo))
  {
    error = File_rename(tmpFileName,fileName,NULL);
    if (error != ERROR_NONE)
    {
      File_delete(tmpFileName,FALSE);
      String_delete(tmpFileName);
      String_delete(directoryName);
      return error;
    }
  }
  else
  {
    File_delete(tmpFileName,FALSE);
  }

  // free resources
  String_delete(tmpFileName);
  String_delete(directoryName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : isFileChanged
* Purpose: check if file changed
* Input  : namesDictionary - names dictionary
*          fileName        - file name
*          fileInfo        - file info with file cast data
* Output : -
* Return : TRUE iff file changed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool isFileChanged(Dictionary     *namesDictionary,
                         ConstString    fileName,
                         const FileInfo *fileInfo
                        )
{
  union
  {
    void                *value;
    IncrementalListInfo *incrementalListInfo;
  } data;
  ulong length;

  assert(namesDictionary != NULL);
  assert(fileName != NULL);
  assert(fileInfo != NULL);

  // check if exists
  if (!Dictionary_find(namesDictionary,
                       String_cString(fileName),
                       String_length(fileName),
                       &data.value,
                       &length
                      )
     )
  {
    return TRUE;
  }
  assert(length == sizeof(IncrementalListInfo));

  // check if modified
  if (!File_isEqualsCast(&data.incrementalListInfo->cast,&fileInfo->cast))
  {
    return TRUE;
  }

  return FALSE;
}

/***********************************************************************\
* Name   : printIncrementalInfo
* Purpose: print incremental info for file
* Input  : dictionary - incremental dictionary
*          name       - name
*          fileCast   - file cast data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printIncrementalInfo(Dictionary     *dictionary,
                                ConstString    name,
                                const FileCast *fileCast
                               )
{
  union
  {
    void                *value;
    IncrementalListInfo *incrementalListInfo;
  } data;
  ulong length;

  String s = String_new();

  printInfo(2,"Include '%s':\n",String_cString(name));
  printInfo(2,"  new: %s\n",String_cString(File_castToString(String_clear(s),fileCast)));
  if (Dictionary_find(dictionary,
                      String_cString(name),
                      String_length(name),
                      &data.value,
                      &length
                     )
      )
  {
    printInfo(2,"  old: %s\n",String_cString(File_castToString(String_clear(s),&data.incrementalListInfo->cast)));
  }
  else
  {
    printInfo(2,"  old: not exists\n");
  }

  String_delete(s);
}


/***********************************************************************\
* Name   : addIncrementalList
* Purpose: add file to incremental list
* Input  : namesDictionary - names dictionary
*          fileName        - file name
*          fileInfo        - file info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addIncrementalList(Dictionary     *namesDictionary,
                              ConstString    fileName,
                              const FileInfo *fileInfo
                             )
{
  IncrementalListInfo incrementalListInfo;

  assert(namesDictionary != NULL);
  assert(fileName != NULL);
  assert(fileInfo != NULL);

  incrementalListInfo.state = INCREMENTAL_FILE_STATE_ADDED;
  memcpy(&incrementalListInfo.cast,&fileInfo->cast,sizeof(FileCast));

  Dictionary_add(namesDictionary,
                 String_cString(fileName),
                 String_length(fileName),
                 &incrementalListInfo,
                 sizeof(incrementalListInfo)
                );
}

/***********************************************************************\
* Name   : updateStatusInfo
* Purpose: update status info
* Input  : createInfo  - create info
*          forceUpdate - true to force update
* Output : -
* Return : -
* Notes  : Update only every 500ms or if forced
\***********************************************************************/

LOCAL void updateStatusInfo(CreateInfo *createInfo, bool forceUpdate)
{
  static uint64 lastTimestamp = 0LL;
  uint64        timestamp;

  assert(createInfo != NULL);
  assert(Semaphore_isLocked(&createInfo->statusInfoLock));

  if (createInfo->statusInfoFunction != NULL)
  {
    timestamp = Misc_getTimestamp();
    if (forceUpdate || (timestamp > (lastTimestamp+500LL*US_PER_MS)))
    {
      createInfo->statusInfoFunction(createInfo->failError,
                                     &createInfo->statusInfo,
                                     createInfo->statusInfoUserData
                                    );
      lastTimestamp = timestamp;
    }
  }
}

/***********************************************************************\
* Name   : statusInfoUpdateLock
* Purpose: lock status info update
* Input  : createInfo   - create info structure
*          fragmentNode - fragment node (can be NULL)
* Output : -
* Return : always TRUE
* Notes  : -
\***********************************************************************/

LOCAL SemaphoreLock statusInfoUpdateLock(CreateInfo *createInfo, ConstString name, FragmentNode **foundFragmentNode)
{
  assert(createInfo != NULL);

  // lock
  Semaphore_lock(&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);

  if (foundFragmentNode != NULL)
  {
    // find fragment node
    (*foundFragmentNode) = FragmentList_find(&createInfo->statusInfoFragmentList,name);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : statusInfoUpdateUnlock
* Purpose: status info update unlock
* Input  : createInfo - create info structure
*          name       - name of entry
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void statusInfoUpdateUnlock(CreateInfo *createInfo, ConstString name)
{
  const FragmentNode *fragmentNode;

  assert(createInfo != NULL);

  if (name != NULL)
  {
    // update current status info if not set or timeout
    if (   (createInfo->statusInfoCurrentFragmentNode == NULL)
        || ((Misc_getTimestamp()-createInfo->statusInfoCurrentLastUpdateTimestamp) >= 10*US_PER_S)
       )
    {
      // find fragment node
      fragmentNode = FragmentList_find(&createInfo->statusInfoFragmentList,name);

      // set new current status info
      String_set(createInfo->statusInfo.entry.name,name);
      if (fragmentNode != NULL)
      {
        createInfo->statusInfo.entry.doneSize  = FragmentList_getSize(fragmentNode);
        createInfo->statusInfo.entry.totalSize = FragmentList_getTotalSize(fragmentNode);

        createInfo->statusInfoCurrentFragmentNode = !FragmentList_isComplete(fragmentNode) ? fragmentNode : NULL;
      }
      else
      {
        createInfo->statusInfoCurrentFragmentNode = NULL;
      }

      // save last update time
      createInfo->statusInfoCurrentLastUpdateTimestamp = Misc_getTimestamp();
    }
  }

  // update status info
  updateStatusInfo(createInfo,TRUE);

  // unlock
  Semaphore_unlock(&createInfo->statusInfoLock);
}

//TODO: comment
#define STATUS_INFO_GET(createInfo,name) \
  for (SemaphoreLock semaphoreLock = Semaphore_lock(&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER); \
       semaphoreLock; \
       Semaphore_unlock(&createInfo->statusInfoLock), semaphoreLock = FALSE \
      )

/***********************************************************************\
* Name   : STATUS_INFO_UPDATE
* Purpose: update status info
* Input  : createInfo   - create info structure
*          name         - name of entry
*          fragmentNode - fragment node variable (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define STATUS_INFO_UPDATE(createInfo,name,fragmentNode) \
  for (SemaphoreLock semaphoreLock = statusInfoUpdateLock(createInfo,name,fragmentNode); \
       semaphoreLock; \
       statusInfoUpdateUnlock(createInfo,name), semaphoreLock = FALSE \
      )

/***********************************************************************\
* Name   : updateStorageStatusInfo
* Purpose: update storage info data
* Input  : userData          - user data: create info
*          storageStatusInfo - storage status info
* Output : -
* Return : TRUE to continue, FALSE to abort
* Notes  : -
\***********************************************************************/

LOCAL bool updateStorageStatusInfo(const StorageStatusInfo *storageStatusInfo,
                                   void                    *userData
                                  )
{
  CreateInfo *createInfo = (CreateInfo*)userData;

  assert(createInfo != NULL);
  assert(storageStatusInfo != NULL);

  STATUS_INFO_UPDATE(createInfo,NULL,NULL)
  {
    createInfo->statusInfo.storage.doneSize = storageStatusInfo->storageDoneBytes;
    createInfo->statusInfo.volume.number    = storageStatusInfo->volumeNumber;
    createInfo->statusInfo.volume.progress  = storageStatusInfo->volumeProgress;
  }

  return !isAborted(createInfo);
}

/***********************************************************************\
* Name   : appendFileToEntryList
* Purpose: append file to entry list
* Input  : entryMsgQueue    - entry message queue
*          entryType        - entry type
*          name             - name (will be copied!)
*          fileInfo         - file info
*          maxFragmentSize  - max. fragment size or 0
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendFileToEntryList(MsgQueue       *entryMsgQueue,
                                 EntryTypes     entryType,
                                 ConstString    name,
                                 const FileInfo *fileInfo,
                                 uint64         maxFragmentSize
                                )
{
  uint     fragmentCount;
  uint     fragmentNumber;
  uint64   fragmentOffset,fragmentSize;
  EntryMsg entryMsg;

  assert(entryMsgQueue != NULL);
  assert(name != NULL);
  assert(fileInfo != NULL);

  fragmentCount  = (maxFragmentSize > 0LL)
                     ? (fileInfo->size+maxFragmentSize-1)/maxFragmentSize
                     : 1;
  fragmentNumber = 0;
  fragmentOffset = 0LL;
  do
  {
    // calculate fragment size
    fragmentSize = ((maxFragmentSize > 0LL) && ((fileInfo->size-fragmentOffset) > maxFragmentSize))
                     ? maxFragmentSize
                     : fileInfo->size-fragmentOffset;

    // init
    entryMsg.entryType         = entryType;
    entryMsg.fileType          = FILE_TYPE_FILE;
    entryMsg.name              = String_duplicate(name);
    StringList_init(&entryMsg.nameList);
    memCopyFast(&entryMsg.fileInfo,sizeof(entryMsg.fileInfo),fileInfo,sizeof(FileInfo));
    entryMsg.fragmentNumber    = fragmentNumber;
    entryMsg.fragmentCount     = fragmentCount;
    entryMsg.fragmentOffset    = fragmentOffset;
    entryMsg.fragmentSize      = fragmentSize;

    // put into message queue
    if (!MsgQueue_put(entryMsgQueue,&entryMsg,sizeof(entryMsg)))
    {
      freeEntryMsg(&entryMsg,NULL);
    }

    // next fragment offset
    fragmentNumber++;
    fragmentOffset += fragmentSize;
  }
  while (fragmentOffset < fileInfo->size);
}

/***********************************************************************\
* Name   : appendImageToEntryList
* Purpose: append image to entry list
* Input  : entryMsgQueue    - entry message queue
*          entryType        - entry type
*          name             - name (will be copied!)
*          deviceInfo       - device info
*          maxFragmentSize  - max. fragment size or 0
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendImageToEntryList(MsgQueue         *entryMsgQueue,
                                  EntryTypes       entryType,
                                  ConstString      name,
                                  const DeviceInfo *deviceInfo,
                                  uint64           maxFragmentSize
                                 )
{
  uint     fragmentCount;
  uint     fragmentNumber;
  uint64   fragmentOffset,fragmentSize;
  EntryMsg entryMsg;

  assert(entryMsgQueue != NULL);
  assert(name != NULL);
  assert(deviceInfo != NULL);

  fragmentCount  = (maxFragmentSize > 0LL)
                     ? (deviceInfo->size+maxFragmentSize-1)/maxFragmentSize
                     : 1;
  fragmentNumber = 0;
  fragmentOffset = 0LL;
  do
  {
    // calculate fragment size
    fragmentSize = ((maxFragmentSize > 0LL) && ((deviceInfo->size-fragmentOffset) > maxFragmentSize))
                     ? maxFragmentSize
                     : deviceInfo->size-fragmentOffset;

    // init
    entryMsg.entryType         = entryType;
    entryMsg.fileType          = FILE_TYPE_SPECIAL;
    entryMsg.name              = String_duplicate(name);
    StringList_init(&entryMsg.nameList);
    memCopyFast(&entryMsg.deviceInfo,sizeof(entryMsg.deviceInfo),deviceInfo,sizeof(DeviceInfo));
    entryMsg.fragmentNumber    = fragmentNumber;
    entryMsg.fragmentCount     = fragmentCount;
    entryMsg.fragmentOffset    = fragmentOffset;
    entryMsg.fragmentSize      = fragmentSize;

    // put into message queue
    if (!MsgQueue_put(entryMsgQueue,&entryMsg,sizeof(entryMsg)))
    {
      freeEntryMsg(&entryMsg,NULL);
    }

    // next fragment offset
    fragmentNumber++;
    fragmentOffset += fragmentSize;
  }
  while (fragmentOffset < deviceInfo->size);
}

/***********************************************************************\
* Name   : appendDirectoryToEntryList
* Purpose: append directory to entry list
* Input  : entryMsgQueue - entry message queue
*          entryType     - entry type
*          name          - name (will be copied!)
*          fileInfo      - file info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendDirectoryToEntryList(MsgQueue       *entryMsgQueue,
                                      EntryTypes     entryType,
                                      ConstString    name,
                                      const FileInfo *fileInfo
                                     )
{
  EntryMsg entryMsg;

  assert(entryMsgQueue != NULL);
  assert(name != NULL);
  assert(fileInfo != NULL);

  // init
  entryMsg.entryType      = entryType;
  entryMsg.fileType       = FILE_TYPE_DIRECTORY;
  entryMsg.name           = String_duplicate(name);
  StringList_init(&entryMsg.nameList);
  memCopyFast(&entryMsg.fileInfo,sizeof(entryMsg.fileInfo),fileInfo,sizeof(FileInfo));
  entryMsg.fragmentNumber = 0;
  entryMsg.fragmentCount  = 0;
  entryMsg.fragmentOffset = 0LL;
  entryMsg.fragmentSize   = 0LL;

  // put into message queue
  if (!MsgQueue_put(entryMsgQueue,&entryMsg,sizeof(entryMsg)))
  {
    freeEntryMsg(&entryMsg,NULL);
  }
}

/***********************************************************************\
* Name   : appendLinkToEntryList
* Purpose: append link to entry list
* Input  : entryMsgQueue - entry message queue
*          entryType     - entry type
*          name          - name (will be copied!)
*          fileInfo      - file info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendLinkToEntryList(MsgQueue       *entryMsgQueue,
                                 EntryTypes     entryType,
                                 ConstString    name,
                                 const FileInfo *fileInfo
                                )
{
  EntryMsg entryMsg;

  assert(entryMsgQueue != NULL);
  assert(name != NULL);
  assert(fileInfo != NULL);

  // init
  entryMsg.entryType      = entryType;
  entryMsg.fileType       = FILE_TYPE_LINK;
  entryMsg.name           = String_duplicate(name);
  StringList_init(&entryMsg.nameList);
  memCopyFast(&entryMsg.fileInfo,sizeof(entryMsg.fileInfo),fileInfo,sizeof(FileInfo));
  entryMsg.fragmentNumber = 0;
  entryMsg.fragmentCount  = 0;
  entryMsg.fragmentOffset = 0LL;
  entryMsg.fragmentSize   = 0LL;

  // put into message queue
  if (!MsgQueue_put(entryMsgQueue,&entryMsg,sizeof(entryMsg)))
  {
    freeEntryMsg(&entryMsg,NULL);
  }
}

/***********************************************************************\
* Name   : appendHardLinkToEntryList
* Purpose: append hard link to entry list
* Input  : entryMsgQueue   - entry message queue
*          entryType       - entry type
*          nameList        - name list
*          fileInfo        - file info
*          maxFragmentSize - max. fragment size or 0
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendHardLinkToEntryList(MsgQueue       *entryMsgQueue,
                                     EntryTypes     entryType,
                                     StringList     *nameList,
                                     const FileInfo *fileInfo,
                                     uint64         maxFragmentSize
                                    )
{
  uint     fragmentCount;
  uint     fragmentNumber;
  uint64   fragmentOffset,fragmentSize;
  EntryMsg entryMsg;

  assert(entryMsgQueue != NULL);
  assert(nameList != NULL);
  assert(!StringList_isEmpty(nameList));
  assert(fileInfo != NULL);

  fragmentCount     = (maxFragmentSize > 0LL)
                        ? (fileInfo->size+maxFragmentSize-1)/maxFragmentSize
                        : 1;
  fragmentNumber    = 0;
  fragmentOffset    = 0LL;
  do
  {
    // calculate fragment size
    fragmentSize = ((maxFragmentSize > 0LL) && ((fileInfo->size-fragmentOffset) > maxFragmentSize))
                     ? maxFragmentSize
                     : fileInfo->size-fragmentOffset;

    // init
    entryMsg.entryType      = entryType;
    entryMsg.fileType       = FILE_TYPE_HARDLINK;
    entryMsg.name           = NULL;
    StringList_initDuplicate(&entryMsg.nameList,nameList);
    memCopyFast(&entryMsg.fileInfo,sizeof(entryMsg.fileInfo),fileInfo,sizeof(FileInfo));
    entryMsg.fragmentNumber = fragmentNumber;
    entryMsg.fragmentCount  = fragmentCount;
    entryMsg.fragmentOffset = fragmentOffset;
    entryMsg.fragmentSize   = fragmentSize;

    // put into message queue
    if (!MsgQueue_put(entryMsgQueue,&entryMsg,sizeof(entryMsg)))
    {
      freeEntryMsg(&entryMsg,NULL);
    }

    // next fragment offset
    fragmentNumber++;
    fragmentOffset += fragmentSize;
  }
  while (fragmentOffset < fileInfo->size);
  StringList_clear(nameList);
}

/***********************************************************************\
* Name   : appendSpecialToEntryList
* Purpose: append special to entry list
* Input  : entryMsgQueue - entry message queue
*          entryType     - entry type
*          name          - name (will be copied!)
*          fileInfo      - file info or NULL
*          deviceInfo    - device info or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendSpecialToEntryList(MsgQueue         *entryMsgQueue,
                                    EntryTypes       entryType,
                                    ConstString      name,
                                    const FileInfo   *fileInfo,
                                    const DeviceInfo *deviceInfo
                                   )
{
  EntryMsg entryMsg;

  assert(entryMsgQueue != NULL);
  assert(name != NULL);
  assert((fileInfo != NULL) || (deviceInfo != NULL));

  // init
  entryMsg.entryType      = entryType;
  entryMsg.fileType       = FILE_TYPE_SPECIAL;
  entryMsg.name           = String_duplicate(name);
  StringList_init(&entryMsg.nameList);
  switch (entryType)
  {
    case ENTRY_TYPE_FILE:
      assert(fileInfo != NULL);
      memCopyFast(&entryMsg.fileInfo,sizeof(entryMsg.fileInfo),fileInfo,sizeof(FileInfo));
      break;
    case ENTRY_TYPE_IMAGE:
      assert(deviceInfo != NULL);
      memCopyFast(&entryMsg.deviceInfo,sizeof(entryMsg.deviceInfo),deviceInfo,sizeof(DeviceInfo));
      break;
    case ENTRY_TYPE_UNKNOWN:
      break;
  }
  entryMsg.fragmentNumber = 0;
  entryMsg.fragmentCount  = 0;
  entryMsg.fragmentOffset = 0LL;
  entryMsg.fragmentSize   = 0LL;

  // put into message queue
  if (!MsgQueue_put(entryMsgQueue,&entryMsg,sizeof(entryMsg)))
  {
    freeEntryMsg(&entryMsg,NULL);
  }
}

/***********************************************************************\
* Name   : formatArchiveFileName
* Purpose: get archive file name
* Input  : fileName           - file name variable
*          templateFileName   - template file name
*          expandMacroMode    - expand macro mode; see
*                               EXPAND_MACRO_MODE_*
*          archiveType        - archive type; see ARCHIVE_TYPE_*
*          scheduleTitle      - schedule title or NULL
*          scheduleCustomText - schedule custom text or NULL
*          dateTime           - date/time
*          partNumber         - part number (>=0 for parts,
*                               ARCHIVE_PART_NUMBER_NONE for single
*                               part archive)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors formatArchiveFileName(String           fileName,
                                   ConstString      templateFileName,
                                   ExpandMacroModes expandMacroMode,
                                   ArchiveTypes     archiveType,
                                   ConstString      scheduleTitle,
                                   ConstString      scheduleCustomText,
                                   uint64           dateTime,
                                   int              partNumber
                                  )
{
  StaticString   (uuid,MISC_UUID_STRING_LENGTH);
  bool           partNumberFlag;
  TemplateHandle templateHandle;
  TextMacros     (textMacros,5);
  ulong          i,j;
  char           buffer[256];
  ulong          divisor;
  ulong          n;
  uint           z;
  int            d;

  assert(fileName != NULL);
  assert(templateFileName != NULL);

  // init variables
  Misc_getUUID(uuid);

  // init template
  templateInit(&templateHandle,
               String_cString(templateFileName),
               expandMacroMode,
               dateTime
              );

  // expand template
  TEXT_MACROS_INIT(textMacros)
  {
    TEXT_MACRO_X_CSTRING("%type", Archive_archiveTypeToString(archiveType),TEXT_MACRO_PATTERN_CSTRING);
    TEXT_MACRO_X_CSTRING("%T",    Archive_archiveTypeToShortString(archiveType),".");
    TEXT_MACRO_X_STRING ("%uuid", uuid,TEXT_MACRO_PATTERN_CSTRING);
    TEXT_MACRO_X_CSTRING("%title",(scheduleTitle != NULL) ? String_cString(scheduleTitle) : "",TEXT_MACRO_PATTERN_CSTRING);
    TEXT_MACRO_X_CSTRING("%text", (scheduleCustomText != NULL) ? String_cString(scheduleCustomText) : "",TEXT_MACRO_PATTERN_CSTRING);
  }
  templateMacros(&templateHandle,
                 textMacros.data,
                 textMacros.count
                );

  // done template
  if (templateDone(&templateHandle,fileName) == NULL)
  {
    return ERROR_EXPAND_TEMPLATE;
  }

  // expand part number
  partNumberFlag = FALSE;
  i = 0L;
  while (i < String_length(fileName))
  {
    switch (String_index(fileName,i))
    {
      case '%':
        if ((i+1) < String_length(fileName))
        {
          switch (String_index(fileName,i+1))
          {
            case '%':
              // keep %%
              i += 2L;
              break;
            case '#':
              // %# -> #
              String_remove(fileName,i,1);
              i += 1L;
              break;
          }
        }
        else
        {
          // keep % at end of string
          i += 1L;
        }
        break;
      case '#':
        // #...#
        switch (expandMacroMode)
        {
          case EXPAND_MACRO_MODE_STRING:
            if (partNumber != ARCHIVE_PART_NUMBER_NONE)
            {
              // find #...# and get max. divisor for part number
              divisor = 1L;
              j = i+1L;
              while ((j < String_length(fileName) && String_index(fileName,j) == '#'))
              {
                j++;
                if (divisor < 1000000000L) divisor*=10;
              }
              if ((ulong)partNumber >= (divisor*10L))
              {
                return ERROR_INSUFFICIENT_SPLIT_NUMBERS;
              }

              // replace #...# by part number
              n = partNumber;
              z = 0;
              while (divisor > 0L)
              {
                d = n/divisor; n = n%divisor; divisor = divisor/10;
                if (z < sizeof(buffer)-1)
                {
                  buffer[z] = '0'+d; z++;
                }
              }
              buffer[z] = '\0';
              String_replaceCString(fileName,i,j-i,buffer);
              i = j;

              partNumberFlag = TRUE;
            }
            else
            {
              i += 1L;
            }
            break;
          case EXPAND_MACRO_MODE_PATTERN:
            // replace by "."
            String_replaceChar(fileName,i,1,'.');
            i += 1L;
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
            #endif /* NDEBUG */
        }
        break;
      default:
        i += 1L;
        break;
    }
  }

  // append part number if multipart mode and there is no part number in format string
  if ((partNumber != ARCHIVE_PART_NUMBER_NONE) && !partNumberFlag)
  {
    switch (expandMacroMode)
    {
      case EXPAND_MACRO_MODE_STRING:
        String_appendFormat(fileName,".%06d",partNumber);
        break;
      case EXPAND_MACRO_MODE_PATTERN:
        String_appendCString(fileName,"......");
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
        #endif /* NDEBUG */
    }
  }

  // free resources

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : getIncrementalFileNameFromStorage
* Purpose: get incremental file name from storage
* Input  : fileName         - file name variable
*          storageSpecifier - storage specifier
* Output : -
* Return : file name
* Notes  : -
\***********************************************************************/

LOCAL String getIncrementalFileNameFromStorage(String                 fileName,
                                               const StorageSpecifier *storageSpecifier
                                              )
{
  #define SEPARATOR_CHARS "-_"

  ulong i;
  char  ch;

  assert(fileName != NULL);
  assert(storageSpecifier != NULL);

  // remove all macros and leading and trailing separator characters
  String_clear(fileName);
  i = 0L;
  while (i < String_length(storageSpecifier->archiveName))
  {
    ch = String_index(storageSpecifier->archiveName,i);
    switch (ch)
    {
      case '%':
        i += 1L;
        if (i < String_length(storageSpecifier->archiveName))
        {
          // removed previous separator characters
          String_trimEnd(fileName,SEPARATOR_CHARS);

          ch = String_index(storageSpecifier->archiveName,i);
          switch (ch)
          {
            case '%':
              // %%
              String_appendChar(fileName,'%');
              i += 1L;
              break;
            case '#':
              // %#
              String_appendChar(fileName,'#');
              i += 1L;
              break;
            default:
              // discard %xyz
              if (isalpha(ch))
              {
                while (   (i < String_length(storageSpecifier->archiveName))
                       && isalpha(ch)
                      )
                {
                  i += 1L;
                  ch = String_index(storageSpecifier->archiveName,i);
                }
              }

              // discard following separator characters
              if (strchr(SEPARATOR_CHARS,ch) != NULL)
              {
                while (   (i < String_length(storageSpecifier->archiveName))
                       && (strchr(SEPARATOR_CHARS,ch) != NULL)
                      )
                {
                  i += 1L;
                  ch = String_index(storageSpecifier->archiveName,i);
                }
              }
              break;
          }
        }
        break;
      case '#':
        i += 1L;
        break;
      default:
        String_appendChar(fileName,ch);
        i += 1L;
        break;
    }
  }

  // replace or add file name extension
  if (String_subEqualsCString(fileName,
                              FILE_NAME_EXTENSION_ARCHIVE_FILE,
                              String_length(fileName)-strlen(FILE_NAME_EXTENSION_ARCHIVE_FILE),
                              strlen(FILE_NAME_EXTENSION_ARCHIVE_FILE)
                             )
     )
  {
    String_replaceCString(fileName,
                          String_length(fileName)-strlen(FILE_NAME_EXTENSION_ARCHIVE_FILE),
                          strlen(FILE_NAME_EXTENSION_ARCHIVE_FILE),
                          FILE_NAME_EXTENSION_INCREMENTAL_FILE
                         );
  }
  else
  {
    String_appendCString(fileName,FILE_NAME_EXTENSION_INCREMENTAL_FILE);
  }

  return fileName;
}

/***********************************************************************\
* Name   : getIncrementalFileNameFromJobUUID
* Purpose: get incremental file name from job UUID
* Input  : fileName - file name variable
*          jobUUID  - storage specifier
* Output : -
* Return : file name
* Notes  : -
\***********************************************************************/

LOCAL String getIncrementalFileNameFromJobUUID(String fileName, ConstString jobUUID)
{
  assert(fileName != NULL);
  assert(jobUUID != NULL);

  File_setFileName(fileName,globalOptions.incrementalDataDirectory);
  File_appendFileName(fileName,jobUUID);
  String_appendCString(fileName,FILE_NAME_EXTENSION_INCREMENTAL_FILE);

  return fileName;
}

/***********************************************************************\
* Name   : collectorSumThreadCode
* Purpose: file collector sum thread: only collect files and update
*          total files/bytes values
* Input  : createInfo - create info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void collectorSumThreadCode(CreateInfo *createInfo)
{
  /***********************************************************************\
  * Name   : freeHardlinkInfo
  * Purpose: free hardlink info
  * Input  : data     - data
  *          length   - data length
  *          userData - user data (not used)
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void freeHardlinkInfo(const void *data, ulong length, void *userData);
  void freeHardlinkInfo(const void *data, ulong length, void *userData)
  {
    HardLinkInfo *hardLinkInfo = (HardLinkInfo*)data;

    UNUSED_VARIABLE(length);
    UNUSED_VARIABLE(userData);

    StringList_done(&hardLinkInfo->nameList);
  }

  Dictionary     duplicateNamesDictionary;
  String         name;
  Dictionary     hardLinksDictionary;
  Errors         error;
  DatabaseHandle continuousDatabaseHandle;

  assert(createInfo != NULL);
  assert(createInfo->includeEntryList != NULL);
  assert(createInfo->excludePatternList != NULL);
  assert(createInfo->jobOptions != NULL);

  // initialize variables
  if (!Dictionary_init(&duplicateNamesDictionary,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL)))
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  name = String_new();
  if (!Dictionary_init(&hardLinksDictionary,DICTIONARY_BYTE_COPY,CALLBACK_(freeHardlinkInfo,NULL),CALLBACK_(NULL,NULL)))
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  if (Continuous_isAvailable())
  {
    // open continuous database
    error = Continuous_open(&continuousDatabaseHandle);
    if (error != ERROR_NONE)
    {
      Dictionary_done(&hardLinksDictionary);
      String_delete(name);
      Dictionary_done(&duplicateNamesDictionary);
      return;
    }
  }

  if (createInfo->archiveType == ARCHIVE_TYPE_CONTINUOUS)
  {
    DatabaseQueryHandle databaseQueryHandle;
    DatabaseId          databaseId;

    // process entries from continuous database
    if (Continuous_isAvailable())
    {
      error = Continuous_initList(&databaseQueryHandle,&continuousDatabaseHandle,createInfo->jobUUID,createInfo->scheduleUUID);
      if (error == ERROR_NONE)
      {
        while (Continuous_getNext(&databaseQueryHandle,&databaseId,name))
        {
          FileInfo fileInfo;

          // pause
          pauseCreate(createInfo);

          // check if file still exists
          if (!File_exists(name))
          {
            continue;
          }

          // read file info
          error = File_getInfo(&fileInfo,name);
          if (error != ERROR_NONE)
          {
            continue;
          }

          if (createInfo->jobOptions->ignoreNoDumpAttributeFlag || !File_hasAttributeNoDump(&fileInfo))
          {
            switch (fileInfo.type)
            {
              case FILE_TYPE_FILE:
                if (   ((globalOptions.continuousMaxSize == 0LL) || fileInfo.size <= globalOptions.continuousMaxSize)
                    && isInIncludedList(createInfo->includeEntryList,name)
                    && !isInExcludedList(createInfo->excludePatternList,name)
                    && !Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name))
                   )
                {
                  if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                  {
                    // add to known names history
                    Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                    STATUS_INFO_UPDATE(createInfo,name,NULL)
                    {
                      createInfo->statusInfo.total.count++;
                      createInfo->statusInfo.total.size += fileInfo.size;
                    }
                  }
                }
                break;
              case FILE_TYPE_DIRECTORY:
                // add to known names history
                Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                STATUS_INFO_UPDATE(createInfo,name,NULL)
                {
                  createInfo->statusInfo.total.count++;
                }
                break;
              case FILE_TYPE_LINK:
                if (   isInIncludedList(createInfo->includeEntryList,name)
                    && !isInExcludedList(createInfo->excludePatternList,name)
                    && !Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name))
                   )
                {
                  if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                  {
                    // add to known names history
                    Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                    STATUS_INFO_UPDATE(createInfo,name,NULL)
                    {
                      createInfo->statusInfo.total.count++;
                    }
                  }
                }
                break;
              case FILE_TYPE_HARDLINK:
                if (   ((globalOptions.continuousMaxSize == 0LL) || fileInfo.size <= globalOptions.continuousMaxSize)
                    && isInIncludedList(createInfo->includeEntryList,name)
                    && !isInExcludedList(createInfo->excludePatternList,name)
                   )
                {
                  if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                  {
                    union { void *value; HardLinkInfo *hardLinkInfo; } data;
                    HardLinkInfo                                       hardLinkInfo;

                    // add to known names history
                    Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                    if (  !createInfo->partialFlag
                        || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                        )
                    {
                      if (Dictionary_find(&hardLinksDictionary,
                                          &fileInfo.id,
                                          sizeof(fileInfo.id),
                                          &data.value,
                                          NULL
                                         )
                          )
                      {
                        // append name to hard link name list
                        StringList_append(&data.hardLinkInfo->nameList,name);

                        if (StringList_count(&data.hardLinkInfo->nameList) >= data.hardLinkInfo->count)
                        {
                          // found last hardlink -> clear entry
                          Dictionary_remove(&hardLinksDictionary,
                                            &fileInfo.id,
                                            sizeof(fileInfo.id)
                                           );
                        }
                      }
                      else
                      {
                        // create hard link name list
                        if (createInfo->partialFlag && isPrintInfo(2))
                        {
                          printIncrementalInfo(&createInfo->namesDictionary,
                                                name,
                                                &fileInfo.cast
                                              );
                        }

                        hardLinkInfo.count = fileInfo.linkCount;
                        StringList_init(&hardLinkInfo.nameList);
                        StringList_append(&hardLinkInfo.nameList,name);
                        memCopyFast(&hardLinkInfo.fileInfo,sizeof(hardLinkInfo.fileInfo),&fileInfo,sizeof(fileInfo));

                        if (!Dictionary_add(&hardLinksDictionary,
                                            &fileInfo.id,
                                            sizeof(fileInfo.id),
                                            &hardLinkInfo,
                                            sizeof(hardLinkInfo)
                                           )
                            )
                        {
                          HALT_INSUFFICIENT_MEMORY();
                        }

                        // update status
                        STATUS_INFO_UPDATE(createInfo,name,NULL)
                        {
                          createInfo->statusInfo.total.count++;
                          createInfo->statusInfo.total.size += fileInfo.size;
                        }
                      }
                    }
                  }
                }
                break;
              case FILE_TYPE_SPECIAL:
                if (   isInIncludedList(createInfo->includeEntryList,name)
                    && !isInExcludedList(createInfo->excludePatternList,name)
                    && !Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name))
                   )
                {
                  if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                  {
                    // add to known names history
                    Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                    STATUS_INFO_UPDATE(createInfo,name,NULL)
                    {
                      createInfo->statusInfo.total.count++;
                    }
                  }
                }
                break;
              default:
                break;
            }
          }

          // free resources
        }
        Continuous_doneList(&databaseQueryHandle);
      }
    }
  }
  else
  {
    StringList          nameList;
    String              basePath;
    String              fileName;
    EntryNode           *includeEntryNode;
    StringTokenizer     fileNameTokenizer;
    ConstString         token;
    DirectoryListHandle directoryListHandle;

    StringList_init(&nameList);
    basePath = String_new();
    fileName = String_new();

    // process include entries
    includeEntryNode = createInfo->includeEntryList->head;
    while (   (createInfo->failError == ERROR_NONE)
           && !isAborted(createInfo)
           && (includeEntryNode != NULL)
          )
    {
      // pause
      pauseCreate(createInfo);

      // find base path
      File_initSplitFileName(&fileNameTokenizer,includeEntryNode->string);
      if (File_getNextSplitFileName(&fileNameTokenizer,&token) && !Pattern_checkIsPattern(token))
      {
        if (!String_isEmpty(token))
        {
          File_setFileName(basePath,token);
        }
        else
        {
          File_setFileNameChar(basePath,FILE_PATHNAME_SEPARATOR_CHAR);
        }
      }
      while (File_getNextSplitFileName(&fileNameTokenizer,&token) && !Pattern_checkIsPattern(token))
      {
        File_appendFileName(basePath,token);
      }
      File_doneSplitFileName(&fileNameTokenizer);

      // find files
      StringList_append(&nameList,basePath);
      while (   (createInfo->failError == ERROR_NONE)
             && !isAborted(createInfo)
             && !StringList_isEmpty(&nameList)
            )
      {
        FileInfo fileInfo;

        // pause
        pauseCreate(createInfo);

        // get next entry to process
        StringList_removeLast(&nameList,name);

        // read file info
        error = File_getInfo(&fileInfo,name);
        if (error != ERROR_NONE)
        {
          continue;
        }

        if (createInfo->jobOptions->ignoreNoDumpAttributeFlag || !File_hasAttributeNoDump(&fileInfo))
        {
          switch (fileInfo.type)
          {
            case FILE_TYPE_FILE:
              if (isIncluded(includeEntryNode,name))
              {
                if (!isInExcludedList(createInfo->excludePatternList,name))
                {
                  if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                  {
                    // add to known names history
                    Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                    switch (includeEntryNode->type)
                    {
                      case ENTRY_TYPE_FILE:
                        if (   !createInfo->partialFlag
                            || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                           )
                        {
                          STATUS_INFO_UPDATE(createInfo,name,NULL)
                          {
                            createInfo->statusInfo.total.count++;
                            createInfo->statusInfo.total.size += fileInfo.size;
                          }
                        }
                        break;
                      case ENTRY_TYPE_IMAGE:
                        // nothing to do
                        break;
                      default:
                        #ifndef NDEBUG
                          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                        #endif /* NDEBUG */
                        break; /* not reached */
                    }
                  }
                }
              }
              break;
            case FILE_TYPE_DIRECTORY:
              // add to known names history
              Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

              if (globalOptions.ignoreNoBackupFileFlag || !hasNoBackup(name))
              {
                if (isIncluded(includeEntryNode,name))
                {
                  if (!isInExcludedList(createInfo->excludePatternList,name))
                  {
                    switch (includeEntryNode->type)
                    {
                      case ENTRY_TYPE_FILE:
                        if (   !createInfo->partialFlag
                            || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                           )
                        {
                          STATUS_INFO_UPDATE(createInfo,name,NULL)
                          {
                            createInfo->statusInfo.total.count++;
                          }
                        }
                        break;
                      case ENTRY_TYPE_IMAGE:
                        // nothing to do
                        break;
                      default:
                        #ifndef NDEBUG
                          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                        #endif /* NDEBUG */
                        break; /* not reached */
                    }
                  }
                }

                // open directory contents
                error = File_openDirectoryList(&directoryListHandle,name);
                if (error == ERROR_NONE)
                {
                  // read directory contents
                  while (   (createInfo->failError == ERROR_NONE)
                         && !isAborted(createInfo)
                         && !File_endOfDirectoryList(&directoryListHandle)
                        )
                  {
                    // pause
                    pauseCreate(createInfo);

                    // read next directory entry
                    error = File_readDirectoryList(&directoryListHandle,fileName);
                    if (error != ERROR_NONE)
                    {
                      continue;
                    }

                    // read file info
                    error = File_getInfo(&fileInfo,fileName);
                    if (error != ERROR_NONE)
                    {
                      continue;
                    }

                    if (isIncluded(includeEntryNode,fileName))
                    {
                      if (!isInExcludedList(createInfo->excludePatternList,fileName))
                      {
                        if (createInfo->jobOptions->ignoreNoDumpAttributeFlag || !File_hasAttributeNoDump(&fileInfo))
                        {
                          switch (fileInfo.type)
                          {
                            case FILE_TYPE_FILE:
                              if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName)))
                              {
                                // add to known names history
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0);

                                switch (includeEntryNode->type)
                                {
                                  case ENTRY_TYPE_FILE:
                                    if (   !createInfo->partialFlag
                                        || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                        )
                                    {
                                      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                                      {
                                        createInfo->statusInfo.total.count++;
                                        createInfo->statusInfo.total.size += fileInfo.size;
                                      }
                                    }
                                    break;
                                  case ENTRY_TYPE_IMAGE:
                                    // nothing to do
                                    break;
                                  default:
                                    #ifndef NDEBUG
                                      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                                    #endif /* NDEBUG */
                                    break; /* not reached */
                                }
                              }
                              break;
                            case FILE_TYPE_DIRECTORY:
                              // add to name list
                              StringList_append(&nameList,fileName);
                              break;
                            case FILE_TYPE_LINK:
                              if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName)))
                              {
                                // add to known names history
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0);

                                switch (includeEntryNode->type)
                                {
                                  case ENTRY_TYPE_FILE:
                                    if (   !createInfo->partialFlag
                                        || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                       )
                                    {
                                      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                                      {
                                        createInfo->statusInfo.total.count++;
                                      }
                                    }
                                    break;
                                  case ENTRY_TYPE_IMAGE:
                                    {
                                      DeviceInfo deviceInfo;

                                      if (File_readLink(fileName,name,TRUE) == ERROR_NONE)
                                      {
                                        // get device info
                                        error = Device_getInfo(&deviceInfo,fileName,FALSE);
                                        if (error != ERROR_NONE)
                                        {
                                          continue;
                                        }

                                        if (deviceInfo.type == DEVICE_TYPE_BLOCK)
                                        {
                                          STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                                          {
                                            createInfo->statusInfo.total.count++;
                                            createInfo->statusInfo.total.size += deviceInfo.size;
                                          }
                                        }
                                      }
                                    }
                                    break;
                                  default:
                                    #ifndef NDEBUG
                                      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                                    #endif /* NDEBUG */
                                    break; /* not reached */
                                }
                              }
                              break;
                            case FILE_TYPE_HARDLINK:
                              if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName)))
                              {
                                // add to known names history
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0);

                                switch (includeEntryNode->type)
                                {
                                  case ENTRY_TYPE_FILE:
                                    {
                                      union { void *value; HardLinkInfo *hardLinkInfo; } data;
                                      HardLinkInfo                                       hardLinkInfo;

                                      if (  !createInfo->partialFlag
                                          || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                          )
                                      {
                                        if (Dictionary_find(&hardLinksDictionary,
                                                            &fileInfo.id,
                                                            sizeof(fileInfo.id),
                                                            &data.value,
                                                            NULL
                                                           )
                                            )
                                        {
                                          // append name to hard link name list
                                          StringList_append(&data.hardLinkInfo->nameList,fileName);

                                          if (StringList_count(&data.hardLinkInfo->nameList) >= data.hardLinkInfo->count)
                                          {
                                            // found last hardlink -> clear entry
                                            Dictionary_remove(&hardLinksDictionary,
                                                              &fileInfo.id,
                                                              sizeof(fileInfo.id)
                                                             );
                                          }
                                        }
                                        else
                                        {
                                          // create hard link name list
                                          if (createInfo->partialFlag && isPrintInfo(2))
                                          {
                                            printIncrementalInfo(&createInfo->namesDictionary,
                                                                  fileName,
                                                                  &fileInfo.cast
                                                                );
                                          }

                                          hardLinkInfo.count = fileInfo.linkCount;
                                          StringList_init(&hardLinkInfo.nameList);
                                          StringList_append(&hardLinkInfo.nameList,fileName);
                                          memCopyFast(&hardLinkInfo.fileInfo,sizeof(hardLinkInfo.fileInfo),&fileInfo,sizeof(fileInfo));

                                          if (!Dictionary_add(&hardLinksDictionary,
                                                              &fileInfo.id,
                                                              sizeof(fileInfo.id),
                                                              &hardLinkInfo,
                                                              sizeof(hardLinkInfo)
                                                             )
                                              )
                                          {
                                            HALT_INSUFFICIENT_MEMORY();
                                          }

                                          // update status
                                          STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                                          {
                                            createInfo->statusInfo.total.count++;
                                            createInfo->statusInfo.total.size += fileInfo.size;
                                          }
                                        }
                                      }
                                    }
                                    break;
                                  case ENTRY_TYPE_IMAGE:
                                    // nothing to do
                                    break;
                                  default:
                                    #ifndef NDEBUG
                                      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                                    #endif /* NDEBUG */
                                    break; /* not reached */
                                }
                              }
                              break;
                            case FILE_TYPE_SPECIAL:
                              if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName)))
                              {
                                // add to known names history
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0);

                                switch (includeEntryNode->type)
                                {
                                  case ENTRY_TYPE_FILE:
                                    if (  !createInfo->partialFlag
                                        || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                        )
                                    {
                                      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                                      {
                                        createInfo->statusInfo.total.count++;
                                        if (   (includeEntryNode->type == ENTRY_TYPE_IMAGE)
                                            && (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                                           )
                                        {
                                          createInfo->statusInfo.total.size += fileInfo.size;
                                        }
                                      }
                                    }
                                    break;
                                  case ENTRY_TYPE_IMAGE:
                                    if (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                                    {
                                      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                                      {
                                        createInfo->statusInfo.total.count++;
                                        createInfo->statusInfo.total.size += fileInfo.size;
                                      }
                                    }
                                    break;
                                  default:
                                    #ifndef NDEBUG
                                      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                                    #endif /* NDEBUG */
                                    break; /* not reached */
                                }
                              }
                              break;
                            default:
                              break;
                          }
                        }
                      }
                    }
                  }

                  // close directory
                  File_closeDirectoryList(&directoryListHandle);
                }
              }
              break;
            case FILE_TYPE_LINK:
              if (isIncluded(includeEntryNode,name))
              {
                if (!isInExcludedList(createInfo->excludePatternList,name))
                {
                  if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                  {
                    // add to known names history
                    Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                    switch (includeEntryNode->type)
                    {
                      case ENTRY_TYPE_FILE:
                        if (   !createInfo->partialFlag
                            || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                            )
                        {
                          STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                          {
                            createInfo->statusInfo.total.count++;
                          }
                        }
                        break;
                      case ENTRY_TYPE_IMAGE:
                        {
                          DeviceInfo deviceInfo;

                          if (File_readLink(fileName,name,TRUE) == ERROR_NONE)
                          {
                            // get device info
                            error = Device_getInfo(&deviceInfo,fileName,FALSE);
                            if (error != ERROR_NONE)
                            {
                              continue;
                            }

                            if (deviceInfo.type == DEVICE_TYPE_BLOCK)
                            {
                              STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                              {
                                createInfo->statusInfo.total.count++;
                                createInfo->statusInfo.total.size += deviceInfo.size;
                              }
                            }
                          }
                        }
                        break;
                      default:
                        #ifndef NDEBUG
                          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                        #endif /* NDEBUG */
                        break; /* not reached */
                    }
                  }
                }
              }
              break;
            case FILE_TYPE_HARDLINK:
              if (isIncluded(includeEntryNode,name))
              {
                if (!isInExcludedList(createInfo->excludePatternList,name))
                {
                  if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                  {
                    // add to known names history
                    Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                    switch (includeEntryNode->type)
                    {
                      case ENTRY_TYPE_FILE:
                        {
                          union { void *value; HardLinkInfo *hardLinkInfo; } data;
                          HardLinkInfo                                       hardLinkInfo;

                          if (   !createInfo->partialFlag
                              || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                              )
                          {
                            if (Dictionary_find(&hardLinksDictionary,
                                                &fileInfo.id,
                                                sizeof(fileInfo.id),
                                                &data.value,
                                                NULL
                                               )
                                )
                            {
                              // append name to hard link name list
                              StringList_append(&data.hardLinkInfo->nameList,fileName);

                              if (StringList_count(&data.hardLinkInfo->nameList) >= data.hardLinkInfo->count)
                              {
                                // found last hardlink -> clear entry
                                Dictionary_remove(&hardLinksDictionary,
                                                  &fileInfo.id,
                                                  sizeof(fileInfo.id)
                                                 );
                              }
                            }
                            else
                            {
                              // create hard link name list
                              if (createInfo->partialFlag && isPrintInfo(2))
                              {
                                printIncrementalInfo(&createInfo->namesDictionary,
                                                      fileName,
                                                      &fileInfo.cast
                                                    );
                              }

                              hardLinkInfo.count = fileInfo.linkCount;
                              StringList_init(&hardLinkInfo.nameList);
                              StringList_append(&hardLinkInfo.nameList,fileName);
                              memCopyFast(&hardLinkInfo.fileInfo,sizeof(hardLinkInfo.fileInfo),&fileInfo,sizeof(fileInfo));

                              if (!Dictionary_add(&hardLinksDictionary,
                                                  &fileInfo.id,
                                                  sizeof(fileInfo.id),
                                                  &hardLinkInfo,
                                                  sizeof(hardLinkInfo)
                                                 )
                                  )
                              {
                                HALT_INSUFFICIENT_MEMORY();
                              }

                              // update status
                              STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                              {
                                createInfo->statusInfo.total.count++;
                                createInfo->statusInfo.total.size += fileInfo.size;
                              }
                            }
                          }
                        }
                        break;
                      case ENTRY_TYPE_IMAGE:
                        // nothing to do
                        break;
                      default:
                        #ifndef NDEBUG
                          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                        #endif /* NDEBUG */
                        break; /* not reached */
                    }
                  }
                }
              }
              break;
            case FILE_TYPE_SPECIAL:
              if (isIncluded(includeEntryNode,name))
              {
                if (!isInExcludedList(createInfo->excludePatternList,name))
                {
                  if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                  {
                    // add to known names history
                    Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                    switch (includeEntryNode->type)
                    {
                      case ENTRY_TYPE_FILE:
                        if (   !createInfo->partialFlag
                            || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                            )
                        {
                          STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                          {
                            createInfo->statusInfo.total.count++;
                          }
                        }
                        break;
                      case ENTRY_TYPE_IMAGE:
                        {
                          DeviceInfo deviceInfo;

                          if (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                          {
                            // get device info
                            error = Device_getInfo(&deviceInfo,name,FALSE);
                            if (error != ERROR_NONE)
                            {
                              continue;
                            }
                            UNUSED_VARIABLE(deviceInfo);

                            STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                            {
                              createInfo->statusInfo.total.count++;
                              createInfo->statusInfo.total.size += deviceInfo.size;
                            }
                          }
                        }
                        break;
                      default:
                        #ifndef NDEBUG
                          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                        #endif /* NDEBUG */
                        break; /* not reached */
                    }
                  }
                }
              }
              break;
            default:
              break;
          }
        }

        // free resources
      }

      // next include entry
      includeEntryNode = includeEntryNode->next;
    }

    // free resoures
    String_delete(fileName);
    String_delete(basePath);
    StringList_done(&nameList);
  }

  // done
  STATUS_INFO_UPDATE(createInfo,NULL,NULL)
  {
    createInfo->statusInfo.collectTotalSumDone = TRUE;
  }

  if (Continuous_isAvailable())
  {
    // close continuous database
    Continuous_close(&continuousDatabaseHandle);
  }

  // free resoures
  Dictionary_done(&hardLinksDictionary);
  String_delete(name);
  Dictionary_done(&duplicateNamesDictionary);

  // terminate
  createInfo->collectorSumThreadExitedFlag = TRUE;
}

//TODO: combine collectorSumThreadCode, collectorThreadCode?
/***********************************************************************\
* Name   : collectorThreadCode
* Purpose: file collector thread
* Input  : createInfo - create info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void collectorThreadCode(CreateInfo *createInfo)
{
  /***********************************************************************\
  * Name   : freeHardlinkInfo
  * Purpose: free hardlink info
  * Input  : data     - data
  *          length   - data length
  *          userData - user data (not used)
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void freeHardlinkInfo(const void *data, ulong length, void *userData);
  void freeHardlinkInfo(const void *data, ulong length, void *userData)
  {
    HardLinkInfo *hardLinkInfo = (HardLinkInfo*)data;

    UNUSED_VARIABLE(length);
    UNUSED_VARIABLE(userData);

    StringList_done(&hardLinkInfo->nameList);
  }

  AutoFreeList       autoFreeList;
  Dictionary         duplicateNamesDictionary;
  String             name;
  Dictionary         hardLinksDictionary;
  Errors             error;
  DatabaseHandle     continuousDatabaseHandle;
  DictionaryIterator dictionaryIterator;
//???
union { const void *value; const uint64 *id; } keyData;
union { void *value; HardLinkInfo *hardLinkInfo; } data;

  assert(createInfo != NULL);
  assert(createInfo->includeEntryList != NULL);
  assert(createInfo->excludePatternList != NULL);
  assert(createInfo->jobOptions != NULL);

  // initialize variables
  AutoFree_init(&autoFreeList);
  if (!Dictionary_init(&duplicateNamesDictionary,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL)))
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  name = String_new();
  if (!Dictionary_init(&hardLinksDictionary,DICTIONARY_BYTE_COPY,CALLBACK_(freeHardlinkInfo,NULL),CALLBACK_(NULL,NULL)))
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,&duplicateNamesDictionary,{ Dictionary_done(&duplicateNamesDictionary); });
  AUTOFREE_ADD(&autoFreeList,name,{ String_delete(name); });
  AUTOFREE_ADD(&autoFreeList,&hardLinksDictionary,{ Dictionary_done(&hardLinksDictionary); });

  if (Continuous_isAvailable())
  {
    // open continuous database
    error = Continuous_open(&continuousDatabaseHandle);
    if (error != ERROR_NONE)
    {
      printError("Cannot initialise continuous database (error: %s)!",
                 Error_getText(error)
                );
      AutoFree_freeAll(&autoFreeList);
      return;
    }
    AUTOFREE_ADD(&autoFreeList,&continuousDatabaseHandle,{ Continuous_close(&continuousDatabaseHandle); });
  }

  if (createInfo->archiveType == ARCHIVE_TYPE_CONTINUOUS)
  {
    if (Continuous_isAvailable())
    {
      // process entries from continuous database
      while (   (createInfo->failError == ERROR_NONE)
             && !isAborted(createInfo)
             && Continuous_getEntry(&continuousDatabaseHandle,
                                    createInfo->jobUUID,
                                    createInfo->scheduleUUID,
                                    NULL,  // databaseId
                                    name
                                   )
            )
      {
        FileInfo fileInfo;

        // pause
        pauseCreate(createInfo);

        // check if file still exists
        if (!File_exists(name))
        {
          continue;
        }

        // read file info
        error = File_getInfo(&fileInfo,name);
        if (error != ERROR_NONE)
        {
          if (createInfo->jobOptions->skipUnreadableFlag)
          {
            printInfo(2,"Cannot get info for '%s' (error: %s) - skipped\n",String_cString(name),Error_getText(error));
            logMessage(createInfo->logHandle,
                       LOG_TYPE_ENTRY_ACCESS_DENIED,
                       "Access denied '%s' (error: %s)",
                       String_cString(name),
                       Error_getText(error)
                      );
          }
          else
          {
            printError("Cannot get info for '%s' (error: %s)",
                       String_cString(name),
                       Error_getText(error)
                      );
          }

          STATUS_INFO_UPDATE(createInfo,name,NULL)
          {
            createInfo->statusInfo.error.count++;
          }

          continue;
        }

        if (createInfo->jobOptions->ignoreNoDumpAttributeFlag || !File_hasAttributeNoDump(&fileInfo))
        {
          switch (fileInfo.type)
          {
            case FILE_TYPE_FILE:
              if (   isInIncludedList(createInfo->includeEntryList,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                 )
              {
                if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                {
                  // add to known names history
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                  if ((globalOptions.continuousMaxSize == 0LL) || fileInfo.size <= globalOptions.continuousMaxSize)
                  {
                    // add to entry list
                    if (createInfo->partialFlag && isPrintInfo(2))
                    {
                      printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
                    }
                    appendFileToEntryList(&createInfo->entryMsgQueue,
                                          ENTRY_TYPE_FILE,
                                          name,
                                          &fileInfo,
                                          !createInfo->storageFlags.noStorage ? globalOptions.fragmentSize : 0LL
                                         );
                  }
                  else
                  {
                    logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Size exceeded limit '%s'",String_cString(name));

                    STATUS_INFO_UPDATE(createInfo,name,NULL)
                    {
                      createInfo->statusInfo.skipped.count++;
                      createInfo->statusInfo.skipped.size += fileInfo.size;
                    }
                  }
                }
              }
              else
              {
                logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                STATUS_INFO_UPDATE(createInfo,name,NULL)
                {
                  createInfo->statusInfo.skipped.count++;
                  createInfo->statusInfo.skipped.size += fileInfo.size;
                }
              }
              break;
            case FILE_TYPE_DIRECTORY:
              // add to known names history
              Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

              // add to entry list
              if (createInfo->partialFlag && isPrintInfo(2))
              {
                printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
              }
              appendDirectoryToEntryList(&createInfo->entryMsgQueue,
                                         ENTRY_TYPE_FILE,
                                         name,
                                         &fileInfo
                                        );
              break;
            case FILE_TYPE_LINK:
              if (   isInIncludedList(createInfo->includeEntryList,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                 )
              {
                if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                {
                  // add to known names history
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                  // add to entry list
                  if (createInfo->partialFlag && isPrintInfo(2))
                  {
                    printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
                  }
                  appendLinkToEntryList(&createInfo->entryMsgQueue,
                                        ENTRY_TYPE_FILE,
                                        name,
                                        &fileInfo
                                       );
                }
              }
              else
              {
                logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                STATUS_INFO_UPDATE(createInfo,name,NULL)
                {
                  createInfo->statusInfo.skipped.count++;
                  createInfo->statusInfo.skipped.size += fileInfo.size;
                }
              }
              break;
            case FILE_TYPE_HARDLINK:
              if (   isInIncludedList(createInfo->includeEntryList,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                 )
              {
                if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                {
                  union { void *value; HardLinkInfo *hardLinkInfo; } data;
                  HardLinkInfo                                       hardLinkInfo;

                  // add to known names history
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                  if ((globalOptions.continuousMaxSize == 0LL) || fileInfo.size <= globalOptions.continuousMaxSize)
                  {
                    if (Dictionary_find(&hardLinksDictionary,
                                        &fileInfo.id,
                                        sizeof(fileInfo.id),
                                        &data.value,
                                        NULL
                                       )
                        )
                    {
                      // append name to hard link name list
                      StringList_append(&data.hardLinkInfo->nameList,name);

                      if (StringList_count(&data.hardLinkInfo->nameList) >= data.hardLinkInfo->count)
                      {
                        // found last hardlink -> add to entry list
                        appendHardLinkToEntryList(&createInfo->entryMsgQueue,
                                                  ENTRY_TYPE_FILE,
                                                  &data.hardLinkInfo->nameList,
                                                  &data.hardLinkInfo->fileInfo,
                                                  !createInfo->storageFlags.noStorage ? globalOptions.fragmentSize : 0LL
                                                 );

                        // clear entry
                        Dictionary_remove(&hardLinksDictionary,
                                          &fileInfo.id,
                                          sizeof(fileInfo.id)
                                         );
                      }
                    }
                    else
                    {
                      // create hard link name list
                      if (createInfo->partialFlag && isPrintInfo(2))
                      {
                        printIncrementalInfo(&createInfo->namesDictionary,
                                              name,
                                              &fileInfo.cast
                                            );
                      }

                      hardLinkInfo.count = fileInfo.linkCount;
                      StringList_init(&hardLinkInfo.nameList);
                      StringList_append(&hardLinkInfo.nameList,name);
                      memCopyFast(&hardLinkInfo.fileInfo,sizeof(hardLinkInfo.fileInfo),&fileInfo,sizeof(fileInfo));

                      if (!Dictionary_add(&hardLinksDictionary,
                                          &fileInfo.id,
                                          sizeof(fileInfo.id),
                                          &hardLinkInfo,
                                          sizeof(hardLinkInfo)
                                         )
                          )
                      {
                        HALT_INSUFFICIENT_MEMORY();
                      }
                    }
                  }
                  else
                  {
                    logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Size exceeded limit '%s'",String_cString(name));

                    STATUS_INFO_UPDATE(createInfo,name,NULL)
                    {
                      createInfo->statusInfo.skipped.count++;
                      createInfo->statusInfo.skipped.size += fileInfo.size;
                    }
                  }
                }
              }
              else
              {
                logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                STATUS_INFO_UPDATE(createInfo,name,NULL)
                {
                  createInfo->statusInfo.skipped.count++;
                  createInfo->statusInfo.skipped.size += fileInfo.size;
                }
              }
              break;
            case FILE_TYPE_SPECIAL:
              if (   isInIncludedList(createInfo->includeEntryList,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                 )
              {
                if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                {
                  // add to known names history
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                  // add to entry list
                  if (createInfo->partialFlag && isPrintInfo(2))
                  {
                    printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
                  }
                  appendSpecialToEntryList(&createInfo->entryMsgQueue,
                                           ENTRY_TYPE_FILE,
                                           name,
                                           &fileInfo,
                                           NULL  // deviceInfo
                                          );
                }
              }
              else
              {
                logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                STATUS_INFO_UPDATE(createInfo,name,NULL)
                {
                  createInfo->statusInfo.skipped.count++;
                  createInfo->statusInfo.skipped.size += fileInfo.size;
                }
              }
              break;
            default:
              printInfo(2,"Unknown type of file '%s' - skipped\n",String_cString(name));
              logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_TYPE_UNKNOWN,"Unknown type '%s'",String_cString(name));

              STATUS_INFO_UPDATE(createInfo,name,NULL)
              {
                createInfo->statusInfo.error.count++;
                createInfo->statusInfo.error.size += (uint64)fileInfo.size;
              }
              break;
          }
        }

        // free resources
      }
    }
  }
  else
  {
    StringList          nameList;
    String              basePath;
    String              fileName;
    EntryNode           *includeEntryNode;
    StringTokenizer     fileNameTokenizer;
    ConstString         token;
    DirectoryListHandle directoryListHandle;

    // initialize variables
    StringList_init(&nameList);
    basePath = String_new();
    fileName = String_new();
    AUTOFREE_ADD(&autoFreeList,&nameList,{ StringList_done(&nameList); });
    AUTOFREE_ADD(&autoFreeList,basePath,{ String_delete(basePath); });

    // process include entries
    includeEntryNode = createInfo->includeEntryList->head;
    while (   (createInfo->failError == ERROR_NONE)
           && !isAborted(createInfo)
           && (includeEntryNode != NULL)
          )
    {
      ulong n;

      // pause
      pauseCreate(createInfo);

      // find base path
      File_initSplitFileName(&fileNameTokenizer,includeEntryNode->string);
      if (File_getNextSplitFileName(&fileNameTokenizer,&token) && !Pattern_checkIsPattern(token))
      {
        if (!String_isEmpty(token))
        {
          File_setFileName(basePath,token);
        }
        else
        {
          File_setFileNameChar(basePath,FILE_PATHNAME_SEPARATOR_CHAR);
        }
      }
      while (File_getNextSplitFileName(&fileNameTokenizer,&token) && !Pattern_checkIsPattern(token))
      {
        File_appendFileName(basePath,token);
      }
      File_doneSplitFileName(&fileNameTokenizer);

      // find files
      n = 0;
      StringList_append(&nameList,basePath);
      while (   (createInfo->failError == ERROR_NONE)
             && !isAborted(createInfo)
             && !StringList_isEmpty(&nameList)
            )
      {
        FileInfo fileInfo;

        // pause
        pauseCreate(createInfo);

        // get next entry to process
        StringList_removeLast(&nameList,name);

        // read file info
        error = File_getInfo(&fileInfo,name);
        if (error != ERROR_NONE)
        {
          if (createInfo->jobOptions->skipUnreadableFlag)
          {
            printInfo(2,"Cannot get info for '%s' (error: %s) - skipped\n",String_cString(name),Error_getText(error));
            logMessage(createInfo->logHandle,
                       LOG_TYPE_ENTRY_ACCESS_DENIED,
                       "Access denied '%s' (error: %s)",
                       String_cString(name),
                       Error_getText(error)
                      );
          }
          else
          {
            printError("Cannot get info for '%s' (error: %s)",
                       String_cString(fileName),
                       Error_getText(error)
                      );
          }

          STATUS_INFO_UPDATE(createInfo,name,NULL)
          {
            createInfo->statusInfo.error.count++;
          }

          continue;
        }

        // increment number of possible found entries
        n++;

        if (createInfo->jobOptions->ignoreNoDumpAttributeFlag || !File_hasAttributeNoDump(&fileInfo))
        {
          switch (fileInfo.type)
          {
            case FILE_TYPE_FILE:
              if (isIncluded(includeEntryNode,name))
              {
                if (!isInExcludedList(createInfo->excludePatternList,name))
                {
                  if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                  {
                    // add to known names history
                    Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                    switch (includeEntryNode->type)
                    {
                      case ENTRY_TYPE_FILE:
                        if (   !createInfo->partialFlag
                            || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                            )
                        {
                          // add to entry list
                          if (createInfo->partialFlag && isPrintInfo(2))
                          {
                            printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
                          }
                          appendFileToEntryList(&createInfo->entryMsgQueue,
                                                ENTRY_TYPE_FILE,
                                                name,
                                                &fileInfo,
                                                !createInfo->storageFlags.noStorage ? globalOptions.fragmentSize : 0LL
                                               );
                        }
                        break;
                      case ENTRY_TYPE_IMAGE:
                        // nothing to do
                        break;
                      default:
                        #ifndef NDEBUG
                          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                        #endif /* NDEBUG */
                        break; /* not reached */
                    }
                  }
                }
                else
                {
                  logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                  STATUS_INFO_UPDATE(createInfo,name,NULL)
                  {
                    createInfo->statusInfo.skipped.count++;
                    createInfo->statusInfo.skipped.size += fileInfo.size;
                  }
                }
              }
              break;
            case FILE_TYPE_DIRECTORY:
              // add to known names history
              Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

              if (globalOptions.ignoreNoBackupFileFlag || !hasNoBackup(name))
              {
                if (isIncluded(includeEntryNode,name))
                {
                  if (!isInExcludedList(createInfo->excludePatternList,name))
                  {
                    switch (includeEntryNode->type)
                    {
                      case ENTRY_TYPE_FILE:
                        if (   !createInfo->partialFlag
                            || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                          )
                        {
                          // add to entry list
                          if (createInfo->partialFlag && isPrintInfo(2))
                          {
                            printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
                          }
                          appendDirectoryToEntryList(&createInfo->entryMsgQueue,
                                                     ENTRY_TYPE_FILE,
                                                     name,
                                                     &fileInfo
                                                    );
                        }
                        break;
                      case ENTRY_TYPE_IMAGE:
                        // nothing to do
                        break;
                      default:
                        #ifndef NDEBUG
                          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                        #endif /* NDEBUG */
                        break; /* not reached */
                    }
                  }
                  else
                  {
                    logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                    STATUS_INFO_UPDATE(createInfo,name,NULL)
                    {
                      createInfo->statusInfo.skipped.count++;
                      createInfo->statusInfo.skipped.size += fileInfo.size;
                    }
                  }
                }

                // open directory contents
                error = File_openDirectoryList(&directoryListHandle,name);
                if (error == ERROR_NONE)
                {
                  // read directory content
                  while (   (createInfo->failError == ERROR_NONE)
                         && !isAborted(createInfo)
                         && !File_endOfDirectoryList(&directoryListHandle)
                        )
                  {
                    // pause
                    pauseCreate(createInfo);

                    // read next directory entry
                    error = File_readDirectoryList(&directoryListHandle,fileName);
                    if (error != ERROR_NONE)
                    {
                      printInfo(2,"Cannot read directory '%s' (error: %s) - skipped\n",String_cString(name),Error_getText(error));
                      logMessage(createInfo->logHandle,
                                 LOG_TYPE_ENTRY_ACCESS_DENIED,
                                 "Access denied '%s' (error: %s)",
                                 String_cString(name),
                                 Error_getText(error)
                                );

                      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                      {
                        createInfo->statusInfo.error.count++;
                        createInfo->statusInfo.error.size += (uint64)fileInfo.size;
                      }

                      continue;
                    }

                    // read file info
                    error = File_getInfo(&fileInfo,fileName);
                    if (error != ERROR_NONE)
                    {
                      printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(fileName),Error_getText(error));
                      logMessage(createInfo->logHandle,
                                 LOG_TYPE_ENTRY_ACCESS_DENIED,
                                 "Access denied '%s' (error: %s)",
                                 String_cString(fileName),
                                 Error_getText(error)
                                );

                      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                      {
                        createInfo->statusInfo.error.count++;
                      }

                      continue;
                    }

                    if (isIncluded(includeEntryNode,fileName))
                    {
                      if (!isInExcludedList(createInfo->excludePatternList,fileName))
                      {
                        if (createInfo->jobOptions->ignoreNoDumpAttributeFlag || !File_hasAttributeNoDump(&fileInfo))
                        {
                          switch (fileInfo.type)
                          {
                            case FILE_TYPE_FILE:
                              if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName)))
                              {
                                // add to known names history
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0);

                                switch (includeEntryNode->type)
                                {
                                  case ENTRY_TYPE_FILE:
                                    if (   !createInfo->partialFlag
                                        || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                        )
                                    {
                                      // add to entry list
                                      if (createInfo->partialFlag && isPrintInfo(2))
                                      {
                                        printIncrementalInfo(&createInfo->namesDictionary,fileName,&fileInfo.cast);
                                      }
                                      appendFileToEntryList(&createInfo->entryMsgQueue,
                                                            ENTRY_TYPE_FILE,
                                                            fileName,
                                                            &fileInfo,
                                                            !createInfo->storageFlags.noStorage ? globalOptions.fragmentSize : 0LL
                                                           );
                                    }
                                    break;
                                  case ENTRY_TYPE_IMAGE:
                                    // nothing to do
                                    break;
                                  default:
                                    #ifndef NDEBUG
                                      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                                    #endif /* NDEBUG */
                                    break; /* not reached */
                                }
                              }
                              break;
                            case FILE_TYPE_DIRECTORY:
                              // add to directory search list
                              StringList_append(&nameList,fileName);
                              break;
                            case FILE_TYPE_LINK:
                              if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName)))
                              {
                                // add to known names history
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0);

                                switch (includeEntryNode->type)
                                {
                                  case ENTRY_TYPE_FILE:
                                    if (   !createInfo->partialFlag
                                        || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                        )
                                    {
                                      // add to entry list
                                      if (createInfo->partialFlag && isPrintInfo(2))
                                      {
                                        printIncrementalInfo(&createInfo->namesDictionary,fileName,&fileInfo.cast);
                                      }
                                      appendLinkToEntryList(&createInfo->entryMsgQueue,
                                                            ENTRY_TYPE_FILE,
                                                            fileName,
                                                            &fileInfo
                                                           );
                                    }
                                    break;
                                  case ENTRY_TYPE_IMAGE:
                                    {
                                      DeviceInfo deviceInfo;

                                      if (File_readLink(fileName,name,TRUE) == ERROR_NONE)
                                      {
                                        // get device info
                                        error = Device_getInfo(&deviceInfo,fileName,TRUE);
                                        if (error != ERROR_NONE)
                                        {
                                          printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(name),Error_getText(error));
                                          logMessage(createInfo->logHandle,
                                                     LOG_TYPE_ENTRY_ACCESS_DENIED,
                                                     "Access denied '%s' (error: %s)",
                                                     String_cString(name),
                                                     Error_getText(error)
                                                    );

                                          STATUS_INFO_UPDATE(createInfo,name,NULL)
                                          {
                                            createInfo->statusInfo.error.count++;
                                          }

                                          continue;
                                        }

                                        // add to entry list
                                        appendImageToEntryList(&createInfo->entryMsgQueue,
                                                               ENTRY_TYPE_IMAGE,
                                                               name,
                                                               &deviceInfo,
                                                               !createInfo->storageFlags.noStorage ? globalOptions.fragmentSize : 0LL
                                                              );
                                      }
                                    }
                                    break;
                                  default:
                                    #ifndef NDEBUG
                                      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                                    #endif /* NDEBUG */
                                    break; /* not reached */
                                }
                              }
                              break;
                            case FILE_TYPE_HARDLINK:
                              if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName)))
                              {
                                // add to known names history
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0);

                                switch (includeEntryNode->type)
                                {
                                  case ENTRY_TYPE_FILE:
                                    {
                                      union { void *value; HardLinkInfo *hardLinkInfo; } data;
                                      HardLinkInfo                                       hardLinkInfo;

                                      if (   !createInfo->partialFlag
                                          || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                          )
                                      {
                                        if (Dictionary_find(&hardLinksDictionary,
                                                            &fileInfo.id,
                                                            sizeof(fileInfo.id),
                                                            &data.value,
                                                            NULL
                                                           )
                                            )
                                        {
                                          // append name to hard link name list
                                          StringList_append(&data.hardLinkInfo->nameList,fileName);

                                          if (StringList_count(&data.hardLinkInfo->nameList) >= data.hardLinkInfo->count)
                                          {
                                            // found last hardlink -> add to entry list
                                            appendHardLinkToEntryList(&createInfo->entryMsgQueue,
                                                                      ENTRY_TYPE_FILE,
                                                                      &data.hardLinkInfo->nameList,
                                                                      &data.hardLinkInfo->fileInfo,
                                                                      !createInfo->storageFlags.noStorage ? globalOptions.fragmentSize : 0LL
                                                                     );

                                            // clear entry
                                            Dictionary_remove(&hardLinksDictionary,
                                                              &fileInfo.id,
                                                              sizeof(fileInfo.id)
                                                             );
                                          }
                                        }
                                        else
                                        {
                                          // create hard link name list
                                          if (createInfo->partialFlag && isPrintInfo(2))
                                          {
                                            printIncrementalInfo(&createInfo->namesDictionary,
                                                                  fileName,
                                                                  &fileInfo.cast
                                                                );
                                          }

                                          hardLinkInfo.count = fileInfo.linkCount;
                                          StringList_init(&hardLinkInfo.nameList);
                                          StringList_append(&hardLinkInfo.nameList,fileName);
                                          memCopyFast(&hardLinkInfo.fileInfo,sizeof(hardLinkInfo.fileInfo),&fileInfo,sizeof(fileInfo));

                                          if (!Dictionary_add(&hardLinksDictionary,
                                                              &fileInfo.id,
                                                              sizeof(fileInfo.id),
                                                              &hardLinkInfo,
                                                              sizeof(hardLinkInfo)
                                                             )
                                              )
                                          {
                                            HALT_INSUFFICIENT_MEMORY();
                                          }
                                        }
                                      }
                                    }
                                    break;
                                  case ENTRY_TYPE_IMAGE:
                                    // nothing to do
                                    break;
                                  default:
                                    #ifndef NDEBUG
                                      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                                    #endif /* NDEBUG */
                                    break; /* not reached */
                                }
                              }
                              break;
                            case FILE_TYPE_SPECIAL:
                              if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName)))
                              {
                                // add to known names history
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0);

                                switch (includeEntryNode->type)
                                {
                                  case ENTRY_TYPE_FILE:
                                    if (   !createInfo->partialFlag
                                        || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                       )
                                    {
                                      // add to entry list
                                      if (createInfo->partialFlag && isPrintInfo(2))
                                      {
                                        printIncrementalInfo(&createInfo->namesDictionary,fileName,&fileInfo.cast);
                                      }
                                      appendSpecialToEntryList(&createInfo->entryMsgQueue,
                                                               ENTRY_TYPE_FILE,
                                                               fileName,
                                                               &fileInfo,
                                                               NULL  // deviceInfo
                                                              );
                                    }
                                    break;
                                  case ENTRY_TYPE_IMAGE:
                                    if (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                                    {
                                      DeviceInfo deviceInfo;

                                      // get device info
                                      error = Device_getInfo(&deviceInfo,fileName,TRUE);
                                      if (error != ERROR_NONE)
                                      {
                                        printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(name),Error_getText(error));
                                        logMessage(createInfo->logHandle,
                                                   LOG_TYPE_ENTRY_ACCESS_DENIED,
                                                   "Access denied '%s' (error: %s)",
                                                   String_cString(name),
                                                   Error_getText(error)
                                                  );

                                        STATUS_INFO_UPDATE(createInfo,name,NULL)
                                        {
                                          createInfo->statusInfo.error.count++;
                                        }

                                        continue;
                                      }

                                      // add to entry list
                                      appendImageToEntryList(&createInfo->entryMsgQueue,
                                                             ENTRY_TYPE_IMAGE,
                                                             fileName,
                                                             &deviceInfo,
                                                             !createInfo->storageFlags.noStorage ? globalOptions.fragmentSize : 0LL
                                                            );
                                    }
                                    break;
                                  default:
                                    #ifndef NDEBUG
                                      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                                    #endif /* NDEBUG */
                                    break; /* not reached */
                                }
                              }
                              break;
                            default:
                              printInfo(2,"Unknown type of file '%s' - skipped\n",String_cString(fileName));
                              logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_TYPE_UNKNOWN,"Unknown type '%s'",String_cString(fileName));

                              STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                              {
                                createInfo->statusInfo.error.count++;
                                createInfo->statusInfo.error.size += (uint64)fileInfo.size;
                              }
                              break;
                          }
                        }
                        else
                        {
                          logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s' (no dump attribute)",String_cString(fileName));

                          STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                          {
                            createInfo->statusInfo.skipped.count++;
                            createInfo->statusInfo.skipped.size += fileInfo.size;
                          }
                        }
                      }
                      else
                      {
                        logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(fileName));

                        STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                        {
                          createInfo->statusInfo.skipped.count++;
                          createInfo->statusInfo.skipped.size += fileInfo.size;
                        }
                      }
                    }
                  }

                  // close directory
                  File_closeDirectoryList(&directoryListHandle);
                }
                else
                {
                  printInfo(2,"Cannot open directory '%s' (error: %s) - skipped\n",String_cString(name),Error_getText(error));
                  logMessage(createInfo->logHandle,
                             LOG_TYPE_ENTRY_ACCESS_DENIED,
                             "Access denied '%s' (error: %s)",
                             String_cString(name),
                             Error_getText(error)
                            );

                  STATUS_INFO_UPDATE(createInfo,name,NULL)
                  {
                    createInfo->statusInfo.error.count++;
                  }
                }
              }
              else
              {
                logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s' (.nobackup file)",String_cString(name));
              }
              break;
            case FILE_TYPE_LINK:
              if (isIncluded(includeEntryNode,name))
              {
                if (!isInExcludedList(createInfo->excludePatternList,name))
                {
                  if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                  {
                    // add to known names history
                    Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                    switch (includeEntryNode->type)
                    {
                      case ENTRY_TYPE_FILE:
                        if (  !createInfo->partialFlag
                            || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                          )
                        {
                          // add to entry list
                          if (createInfo->partialFlag && isPrintInfo(2))
                          {
                            printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
                          }
                          appendLinkToEntryList(&createInfo->entryMsgQueue,
                                                ENTRY_TYPE_FILE,
                                                name,
                                                &fileInfo
                                               );
                        }
                        break;
                      case ENTRY_TYPE_IMAGE:
                        {
                          DeviceInfo deviceInfo;

                          if (File_readLink(fileName,name,TRUE) == ERROR_NONE)
                          {
                            // get device info
                            error = Device_getInfo(&deviceInfo,fileName,TRUE);
                            if (error != ERROR_NONE)
                            {
                              printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(name),Error_getText(error));
                              logMessage(createInfo->logHandle,
                                         LOG_TYPE_ENTRY_ACCESS_DENIED,
                                         "Access denied '%s' (error: %s)",
                                         String_cString(name),
                                         Error_getText(error)
                                        );

                              STATUS_INFO_UPDATE(createInfo,name,NULL)
                              {
                                createInfo->statusInfo.error.count++;
                              }

                              continue;
                            }

                            // add to entry list
                            appendImageToEntryList(&createInfo->entryMsgQueue,
                                                   ENTRY_TYPE_IMAGE,
                                                   name,
                                                   &deviceInfo,
                                                   !createInfo->storageFlags.noStorage ? globalOptions.fragmentSize : 0LL
                                                  );
                          }
                        }
                        break;
                      default:
                        #ifndef NDEBUG
                          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                        #endif /* NDEBUG */
                        break; /* not reached */
                    }
                  }
                }
                else
                {
                  logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                  STATUS_INFO_UPDATE(createInfo,name,NULL)
                  {
                    createInfo->statusInfo.skipped.count++;
                    createInfo->statusInfo.skipped.size += fileInfo.size;
                  }
                }
              }
              break;
            case FILE_TYPE_HARDLINK:
              if (isIncluded(includeEntryNode,name))
              {
                if (!isInExcludedList(createInfo->excludePatternList,name))
                {
                  if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                  {
                    // add to known names history
                    Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                    switch (includeEntryNode->type)
                    {
                      case ENTRY_TYPE_FILE:
                        if (   !createInfo->partialFlag
                            || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                            )
                        {
                          union { void *value; HardLinkInfo *hardLinkInfo; } data;
                          HardLinkInfo                                       hardLinkInfo;

                          if (Dictionary_find(&hardLinksDictionary,
                                              &fileInfo.id,
                                              sizeof(fileInfo.id),
                                              &data.value,
                                              NULL
                                             )
                              )
                          {
                            // append name to hard link name list
                            StringList_append(&data.hardLinkInfo->nameList,name);

                            if (StringList_count(&data.hardLinkInfo->nameList) >= data.hardLinkInfo->count)
                            {
                              // found last hardlink -> add to entry list
                              appendHardLinkToEntryList(&createInfo->entryMsgQueue,
                                                        ENTRY_TYPE_FILE,
                                                        &data.hardLinkInfo->nameList,
                                                        &data.hardLinkInfo->fileInfo,
                                                        !createInfo->storageFlags.noStorage ? globalOptions.fragmentSize : 0LL
                                                       );

                              // clear entry
                              Dictionary_remove(&hardLinksDictionary,
                                                &fileInfo.id,
                                                sizeof(fileInfo.id)
                                               );
                            }
                          }
                          else
                          {
                            // create hard link name list
                            if (createInfo->partialFlag && isPrintInfo(2))
                            {
                              printIncrementalInfo(&createInfo->namesDictionary,
                                                   name,
                                                   &fileInfo.cast
                                                  );
                            }

                            hardLinkInfo.count = fileInfo.linkCount;
                            StringList_init(&hardLinkInfo.nameList);
                            StringList_append(&hardLinkInfo.nameList,name);
                            memCopyFast(&hardLinkInfo.fileInfo,sizeof(hardLinkInfo.fileInfo),&fileInfo,sizeof(fileInfo));

                            if (!Dictionary_add(&hardLinksDictionary,
                                                &fileInfo.id,
                                                sizeof(fileInfo.id),
                                                &hardLinkInfo,
                                                sizeof(hardLinkInfo)
                                               )
                                )
                            {
                              HALT_INSUFFICIENT_MEMORY();
                            }
                          }
                        }
                        break;
                      case ENTRY_TYPE_IMAGE:
                        // nothing to do
                        break;
                      default:
                        #ifndef NDEBUG
                          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                        #endif /* NDEBUG */
                        break; /* not reached */
                    }
                  }
                }
                else
                {
                  logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                  STATUS_INFO_UPDATE(createInfo,name,NULL)
                  {
                    createInfo->statusInfo.skipped.count++;
                    createInfo->statusInfo.skipped.size += fileInfo.size;
                  }
                }
              }
              break;
            case FILE_TYPE_SPECIAL:
              if (isIncluded(includeEntryNode,name))
              {
                if (!isInExcludedList(createInfo->excludePatternList,name))
                {
                  if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                  {
                    // add to known names history
                    Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                    switch (includeEntryNode->type)
                    {
                      case ENTRY_TYPE_FILE:
                        if (   !createInfo->partialFlag
                            || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                            )
                        {
                          // add to entry list
                          if (createInfo->partialFlag && isPrintInfo(2))
                          {
                            printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
                          }
                          appendSpecialToEntryList(&createInfo->entryMsgQueue,
                                                   ENTRY_TYPE_FILE,
                                                   name,
                                                   &fileInfo,
                                                   NULL  // deviceInfo
                                                  );
                        }
                        break;
                      case ENTRY_TYPE_IMAGE:
                        {
                          DeviceInfo deviceInfo;

                          if (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                          {
                            // get device info
                            error = Device_getInfo(&deviceInfo,name,TRUE);
                            if (error != ERROR_NONE)
                            {
                              printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(name),Error_getText(error));
                              logMessage(createInfo->logHandle,
                                         LOG_TYPE_ENTRY_ACCESS_DENIED,
                                         "Access denied '%s' (error: %s)",
                                         String_cString(name),
                                         Error_getText(error)
                                        );

                              STATUS_INFO_UPDATE(createInfo,name,NULL)
                              {
                                createInfo->statusInfo.error.count++;
                              }

                              continue;
                            }

                            // add to entry list
                            appendImageToEntryList(&createInfo->entryMsgQueue,
                                                   ENTRY_TYPE_IMAGE,
                                                   name,
                                                   &deviceInfo,
                                                   !createInfo->storageFlags.noStorage ? globalOptions.fragmentSize : 0LL
                                                  );
                          }
                        }
                        break;
                      default:
                        #ifndef NDEBUG
                          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                        #endif /* NDEBUG */
                        break; /* not reached */
                    }
                  }
                }
                else
                {
                  logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                  STATUS_INFO_UPDATE(createInfo,name,NULL)
                  {
                    createInfo->statusInfo.skipped.count++;
                    createInfo->statusInfo.skipped.size += fileInfo.size;
                  }
                }
              }
              break;
            default:
              printInfo(2,"Unknown type of file '%s' - skipped\n",String_cString(name));
              logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_TYPE_UNKNOWN,"Unknown type '%s'",String_cString(name));

              STATUS_INFO_UPDATE(createInfo,name,NULL)
              {
                createInfo->statusInfo.error.count++;
                createInfo->statusInfo.error.size += (uint64)fileInfo.size;
              }
              break;
          }
        }
        else
        {
          logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s' (no dump attribute)",String_cString(name));

          STATUS_INFO_UPDATE(createInfo,name,NULL)
          {
            createInfo->statusInfo.skipped.count++;
            createInfo->statusInfo.skipped.size += fileInfo.size;
          }
        }

        // free resources
      }
      if (n <= 0)
      {
        if (createInfo->jobOptions->skipUnreadableFlag)
        {
          printWarning("No matching entry found for '%s' - skipped",
                       String_cString(includeEntryNode->string)
                      );
          logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_MISSING,"No matching entry found for '%s' - skipped",String_cString(includeEntryNode->string));
        }
        else
        {
          printError("No matching entry found for '%s'!",
                     String_cString(includeEntryNode->string)
                    );
          logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_MISSING,"No matching entry found for '%s'",String_cString(includeEntryNode->string));
          createInfo->failError = ERRORX_(FILE_NOT_FOUND_,0,"%s",String_cString(includeEntryNode->string));
        }

        STATUS_INFO_UPDATE(createInfo,NULL,NULL)
        {
          createInfo->statusInfo.error.count++;
        }
      }

      // next include entry
      includeEntryNode = includeEntryNode->next;
    }

    // free resoures
    String_delete(fileName);
    String_delete(basePath);
    StringList_done(&nameList);
  }

  // add incomplete hard link entries (not all hard links found) to entry list
  Dictionary_initIterator(&dictionaryIterator,&hardLinksDictionary);
  while (Dictionary_getNext(&dictionaryIterator,
                            &keyData.value,
                            NULL,
                            &data.value,
                            NULL
                           )
        )
  {
    appendHardLinkToEntryList(&createInfo->entryMsgQueue,
                              ENTRY_TYPE_FILE,
                              &data.hardLinkInfo->nameList,
                              &data.hardLinkInfo->fileInfo,
                              !createInfo->storageFlags.noStorage ? globalOptions.fragmentSize : 0LL
                             );
  }
  Dictionary_doneIterator(&dictionaryIterator);

  if (Continuous_isAvailable())
  {
    // close continuous database
    Continuous_close(&continuousDatabaseHandle);
  }

  // free resoures
  Dictionary_done(&hardLinksDictionary);
  String_delete(name);
  Dictionary_done(&duplicateNamesDictionary);
  AutoFree_done(&autoFreeList);
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : storageInfoIncrement
* Purpose: increment storage info
* Input  : createInfo - create info
*          size       - storage file size
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void storageInfoIncrement(CreateInfo *createInfo, uint64 size)
{
  assert(createInfo != NULL);

  SEMAPHORE_LOCKED_DO(&createInfo->storageInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    createInfo->storage.count += 1;
    createInfo->storage.bytes += size;
  }
}

/***********************************************************************\
* Name   : storageInfoDecrement
* Purpose: decrement storage info
* Input  : createInfo - create info
*          size       - storage file size
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void storageInfoDecrement(CreateInfo *createInfo, uint64 size)
{
  assert(createInfo != NULL);

  SEMAPHORE_LOCKED_DO(&createInfo->storageInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    assert(createInfo->storage.count > 0);
    assert(createInfo->storage.bytes >= size);

    createInfo->storage.count -= 1;
    createInfo->storage.bytes -= size;
  }
}

/***********************************************************************\
* Name   : waitForTemporaryFileSpace
* Purpose: wait for temporary file space
* Input  : createInfo - create info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void waitForTemporaryFileSpace(CreateInfo *createInfo)
{
  assert(createInfo != NULL);

  // wait for space in temporary directory
  if (globalOptions.maxTmpSize > 0)
  {
    SEMAPHORE_LOCKED_DO(&createInfo->storageInfoLock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
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
}

/***********************************************************************\
* Name   : archiveGetSize
* Purpose: call back to get archive size
* Input  : storageInfo - storage info
*          storageId   - index storage id
*          partNumber  - part number or ARCHIVE_PART_NUMBER_NONE for
*                        single part
*          userData    - user data
* Output : -
* Return : archive size [bytes] or 0
* Notes  : -
\***********************************************************************/

LOCAL uint64 archiveGetSize(StorageInfo *storageInfo,
                            IndexId     storageId,
                            int         partNumber,
                            void        *userData
                           )
{
  CreateInfo    *createInfo = (CreateInfo*)userData;
  String        archiveName;
  Errors        error;
  StorageHandle storageHandle;
  uint64        archiveSize;

  assert(storageInfo != NULL);
  assert(createInfo != NULL);

  UNUSED_VARIABLE(storageId);

  archiveSize = 0LL;

  // get archive file name
  archiveName = String_new();
  error = formatArchiveFileName(archiveName,
                                storageInfo->storageSpecifier.archiveName,
                                EXPAND_MACRO_MODE_STRING,
                                createInfo->archiveType,
                                createInfo->scheduleTitle,
                                createInfo->scheduleCustomText,
                                createInfo->createdDateTime,
                                partNumber
                               );
  if (error != ERROR_NONE)
  {
    String_delete(archiveName);
    return 0LL;
  }

  // get archive size
  error = Storage_open(&storageHandle,storageInfo,archiveName);
  if (error != ERROR_NONE)
  {
    String_delete(archiveName);
    return 0LL;
  }
  archiveSize = Storage_getSize(&storageHandle);
  Storage_close(&storageHandle);

  // free resources
  String_delete(archiveName);

  return archiveSize;
}

/***********************************************************************\
* Name   : archiveStore
* Purpose: call back to store archive file
* Input  : storageInfo          - storage info
*          uuidId               - index UUID id
*          jobUUID              - job UUID
*          scheduleUUID         - schedule UUID
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
  CreateInfo    *createInfo = (CreateInfo*)userData;
  Errors        error;
  FileInfo      fileInfo;
  String        archiveName;
  StorageMsg    storageMsg;

  assert(storageInfo != NULL);
  assert(!String_isEmpty(intermediateFileName));
  assert(createInfo != NULL);

  UNUSED_VARIABLE(jobUUID);
  UNUSED_VARIABLE(scheduleUUID);

  // get file info
  error = File_getInfo(&fileInfo,intermediateFileName);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get archive file name
  archiveName = String_new();
  error = formatArchiveFileName(archiveName,
                                storageInfo->storageSpecifier.archiveName,
                                EXPAND_MACRO_MODE_STRING,
                                createInfo->archiveType,
                                createInfo->scheduleTitle,
                                createInfo->scheduleCustomText,
                                createInfo->createdDateTime,
                                partNumber
                               );
  if (error != ERROR_NONE)
  {
    (void)File_delete(intermediateFileName,FALSE);
    String_delete(archiveName);
    return error;
  }
  DEBUG_TESTCODE() { String_delete(archiveName); return DEBUG_TESTCODE_ERROR(); }

  // send to storage thread
  storageMsg.uuidId      = uuidId;
  storageMsg.entityId    = entityId;
  storageMsg.archiveType = archiveType;
  storageMsg.storageId   = storageId;
  storageMsg.fileName    = String_duplicate(intermediateFileName);
  storageMsg.fileSize    = intermediateFileSize;
  storageMsg.archiveName = archiveName;
  storageInfoIncrement(createInfo,fileInfo.size);
  DEBUG_TESTCODE() { freeStorageMsg(&storageMsg,NULL); return DEBUG_TESTCODE_ERROR(); }
  if (!MsgQueue_put(&createInfo->storageMsgQueue,&storageMsg,sizeof(storageMsg)))
  {
    freeStorageMsg(&storageMsg,NULL);
    (void)File_delete(intermediateFileName,FALSE);
    return ERROR_ABORTED;
  }

  // update status info
  STATUS_INFO_UPDATE(createInfo,NULL,NULL)
  {
    createInfo->statusInfo.storage.totalSize += fileInfo.size;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : deleteStorage
* Purpose: delete storage
* Input  : indexHandle - index handle
*          storageId   - storage to delete
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors deleteStorage(IndexHandle *indexHandle,
                           IndexId     storageId
                          )
{
  Errors           error;
  String           storageName;
  StorageSpecifier storageSpecifier;
  StorageInfo      storageInfo;

  assert(indexHandle != NULL);

  // init variables
  storageName = String_new();

  // find storage
  if (!Index_findStorageById(indexHandle,
                             storageId,
                             NULL,  // jobUUID,
                             NULL,  // scheduleUUID
                             NULL,  // uuidId
                             NULL,  // entityId
                             storageName,
                             NULL,  // createdDateTime,
                             NULL,  // size
                             NULL,  // indexState
                             NULL,  // indexMode
                             NULL,  // lastCheckedDateTime
                             NULL,  // errorMessage
                             NULL,  // totalEntryCount
                             NULL  // totalEntrySize
                            )
     )
  {
    String_delete(storageName);
    return ERROR_DATABASE_INDEX_NOT_FOUND;
  }

  error = ERROR_NONE;

  if (!String_isEmpty(storageName))
  {
    // delete storage file
    Storage_initSpecifier(&storageSpecifier);
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error == ERROR_NONE)
    {
//TODO
#ifndef WERROR
#warning NYI: move this special handling of limited scp into Storage_delete()?
#endif
      // init storage
      if (storageSpecifier.type == STORAGE_TYPE_SCP)
      {
        // try to init scp-storage first with sftp
        storageSpecifier.type = STORAGE_TYPE_SFTP;
        error = Storage_init(&storageInfo,
                             NULL,  // masterIO
                             &storageSpecifier,
                             NULL,  // jobOptions
                             &globalOptions.indexDatabaseMaxBandWidthList,
                             SERVER_CONNECTION_PRIORITY_HIGH,
                             STORAGE_FLAGS_NONE,
                             CALLBACK_(NULL,NULL),  // updateStatusInfo
                             CALLBACK_(NULL,NULL),  // getNamePassword
                             CALLBACK_(NULL,NULL),  // requestVolume
                             CALLBACK_(NULL,NULL),  // isPause
                             CALLBACK_(NULL,NULL),  // isAborted
                             NULL  // logHandle
                            );
        if (error != ERROR_NONE)
        {
          // init scp-storage
          storageSpecifier.type = STORAGE_TYPE_SCP;
          error = Storage_init(&storageInfo,
                               NULL,  // masterIO
                               &storageSpecifier,
                               NULL,  // jobOptions
                               &globalOptions.indexDatabaseMaxBandWidthList,
                               SERVER_CONNECTION_PRIORITY_HIGH,
                               STORAGE_FLAGS_NONE,
                               CALLBACK_(NULL,NULL),  // updateStatusInfo
                               CALLBACK_(NULL,NULL),  // getNamePassword
                               CALLBACK_(NULL,NULL),  // requestVolume
                               CALLBACK_(NULL,NULL),  // isPause
                               CALLBACK_(NULL,NULL),  // isAborted
                               NULL  // logHandle
                              );
        }
      }
      else
      {
        // init other storage types
        error = Storage_init(&storageInfo,
                             NULL,  // masterIO
                             &storageSpecifier,
                             NULL,  // jobOptions
                             &globalOptions.indexDatabaseMaxBandWidthList,
                             SERVER_CONNECTION_PRIORITY_HIGH,
                             STORAGE_FLAGS_NONE,
                             CALLBACK_(NULL,NULL),  // updateStatusInfo
                             CALLBACK_(NULL,NULL),  // getNamePassword
                             CALLBACK_(NULL,NULL),  // requestVolume
                             CALLBACK_(NULL,NULL),  // isPause
                             CALLBACK_(NULL,NULL),  // isAborted
                             NULL  // logHandle
                            );
      }
      if (error == ERROR_NONE)
      {
        if (Storage_exists(&storageInfo,
                           NULL  // archiveName
                          )
           )
        {
          // delete storage
          error = Storage_delete(&storageInfo,
                                 NULL  // archiveName
                                );
        }

        // prune empty directories
        Storage_pruneDirectories(&storageInfo,
                                 NULL  // archiveName
                                );

        // close storage
        Storage_done(&storageInfo);
      }
    }
    Storage_doneSpecifier(&storageSpecifier);
  }

  // delete index
  if (error == ERROR_NONE)
  {
    error = Index_deleteStorage(indexHandle,storageId);
  }

  // free resources
  String_delete(storageName);

  return error;
}

/***********************************************************************\
* Name   : deleteEntity
* Purpose: delete entity index and all attached storage files
* Input  : indexHandle - index handle
*          entityId    - index id of entity
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors deleteEntity(IndexHandle *indexHandle,
                          IndexId     entityId
                         )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;

  assert(indexHandle != NULL);

  // init variables

  // delete all storages of entity
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 entityId,
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID,
                                 NULL,  // indexIds
                                 0,  // indexIdCount
                                 INDEX_TYPE_SET_ALL,
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // hostName
                                 NULL,  // userName
                                 NULL,  // name
                                 INDEX_STORAGE_SORT_MODE_NONE,
                                 DATABASE_ORDERING_NONE,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (   (error == ERROR_NONE)
         && Index_getNextStorage(&indexQueryHandle,
                                 NULL,  // uuidId
                                 NULL,  // jobUUID
                                 NULL,  // entityId
                                 NULL,  // scheduleUUID
                                 NULL,  // hostName
                                 NULL,  // userName
                                 NULL,  // comment
                                 NULL,  // createdDateTime
                                 NULL,  // archiveType
                                 &storageId,
                                 NULL,  // storageName
                                 NULL,  // createdDateTime
                                 NULL,  // size
                                 NULL,  // indexState
                                 NULL,  // indexMode
                                 NULL,  // lastCheckedDateTime
                                 NULL,  // errorMessage
                                 NULL,  // totalEntryCount
                                 NULL  // totalEntrySize
                                )
        )
  {
    error = deleteStorage(indexHandle,storageId);
  }
  Index_doneList(&indexQueryHandle);

  // delete entity index
  if (error == ERROR_NONE)
  {
    error = Index_deleteEntity(indexHandle,entityId);
  }

  // free resources

  return error;
}

/***********************************************************************\
* Name   : purgeStorageIndex
* Purpose: purge storage index and delete entity, uuid if empty and not
*          locked
* Input  : indexHandle      - index handle or NULL if no index
*          storageId        - index storage id to purge
*          storageSpecifier - storage specifier
*          archiveName      - storage archive name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors purgeStorageIndex(IndexHandle      *indexHandle,
                               IndexId          storageId,
                               StorageSpecifier *storageSpecifier,
                               ConstString      archiveName
                              )
{
  IndexId          oldUUIDId,oldEntityId,oldStorageId;
  String           oldStorageName;
  StorageSpecifier oldStorageSpecifier;
  Array            uuidIds,entityIds,storageIds;
  Errors           error;
  ulong            iterator;
  IndexId          indexId;
  IndexQueryHandle indexQueryHandle;

  // init variables
  oldStorageName = String_new();
  Storage_initSpecifier(&oldStorageSpecifier);
  Array_init(&uuidIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Array_init(&entityIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  Array_init(&storageIds,sizeof(DatabaseId),256,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  if (indexHandle != NULL)
  {
    // get storage ids to purge
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY, // uuidId
                                   INDEX_ID_ANY, // entityId
                                   NULL,  // jobUUID,
                                   NULL,  // scheduleUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_TYPE_SET_ALL,
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // hostName
                                   NULL,  // userName
                                   archiveName,
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      Storage_doneSpecifier(&oldStorageSpecifier);
      String_delete(oldStorageName);
      return error;
    }
    while (Index_getNextStorage(&indexQueryHandle,
                                &oldUUIDId,
                                NULL,  // job UUID
                                &oldEntityId,
                                NULL,  // schedule UUID
                                NULL,  // hostName
                                NULL,  // userName
                                NULL,  // comment
                                NULL,  // createdDateTime
                                NULL,  // archiveType
                                &oldStorageId,
                                oldStorageName,
                                NULL,  // createdDateTime
                                NULL,  // size
                                NULL,  // indexState
                                NULL,  // indexMode
                                NULL,  // lastCheckedDateTime
                                NULL,  // errorMessage
                                NULL,  // totalEntryCount
                                NULL  // totalEntrySize
                               )
          )
    {
      if (   !INDEX_ID_EQUALS(oldStorageId,storageId)
          && (Storage_parseName(&oldStorageSpecifier,oldStorageName) == ERROR_NONE)
          && Storage_equalSpecifiers(storageSpecifier,archiveName,&oldStorageSpecifier,NULL)
         )
      {
        if (!INDEX_ID_IS_NONE(oldUUIDId)) Array_append(&uuidIds,&oldUUIDId);
        if (!INDEX_ID_IS_DEFAULT_ENTITY(oldEntityId)) Array_append(&entityIds,&oldEntityId);
        if (!INDEX_ID_IS_NONE(oldStorageId)) Array_append(&storageIds,&oldStorageId);
      }
    }
    Index_doneList(&indexQueryHandle);
    if (error != ERROR_NONE)
    {
      Array_done(&storageIds);
      Array_done(&entityIds);
      Array_done(&uuidIds);
      Storage_doneSpecifier(&oldStorageSpecifier);
      String_delete(oldStorageName);
      return error;
    }
  }

  // delete old indizes for same storage file
  ARRAY_ITERATEX(&storageIds,iterator,indexId,error == ERROR_NONE)
  {
    // delete old index of storage
    error = Index_deleteStorage(indexHandle,indexId);
    if (error != ERROR_NONE)
    {
      break;
    }
    DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }
  }
  // delete entity index if empty and not locked
  ARRAY_ITERATEX(&entityIds,iterator,indexId,error == ERROR_NONE)
  {
    error = Index_pruneEntity(indexHandle,indexId);
    if (error != ERROR_NONE)
    {
      break;
    }
    DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }
  }
  // delete uuid index if empty
  ARRAY_ITERATEX(&uuidIds,iterator,indexId,error == ERROR_NONE)
  {
    error = Index_pruneUUID(indexHandle,indexId);
    if (error != ERROR_NONE)
    {
      break;
    }
    DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }
  }
  if (error != ERROR_NONE)
  {
    Array_done(&storageIds);
    Array_done(&entityIds);
    Array_done(&uuidIds);
    Storage_doneSpecifier(&oldStorageSpecifier);
    String_delete(oldStorageName);
    return error;
  }

  // free resoruces
  Array_done(&storageIds);
  Array_done(&entityIds);
  Array_done(&uuidIds);
  Storage_doneSpecifier(&oldStorageSpecifier);
  String_delete(oldStorageName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : purgeStorageByJobUUID
* Purpose: purge old storages by job UUID
* Input  : indexHandle    - index handle or NULL if no index
*          jobUUID        - job UUID
*          limit          - size limit of existing storages [bytes] or 0
*          maxStorageSize - max. storage size [bytes]
*          logHandle      - log handle (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void purgeStorageByJobUUID(IndexHandle *indexHandle,
                                 ConstString jobUUID,
                                 uint64      limit,
                                 uint64      maxStorageSize,
                                 LogHandle   *logHandle
                                )
{
  String           storageName;
  StorageSpecifier storageSpecifier;
  Errors           error;
  uint64           totalStorageSize;
  IndexId          oldestUUIDId;
  IndexId          oldestEntityId;
  IndexId          oldestStorageId;
  String           oldestStorageName;
  uint64           oldestCreatedDateTime;
  uint64           oldestSize;
  IndexQueryHandle indexQueryHandle;
  IndexId          uuidId;
  IndexId          entityId;
  IndexId          storageId;
  uint64           createdDateTime;
  uint64           size;
  StorageInfo      storageInfo;
  String           dateTime;

  assert(jobUUID != NULL);
  assert(maxStorageSize > 0LL);

  // init variables
  storageName       = String_new();
  oldestStorageName = String_new();
  Storage_initSpecifier(&storageSpecifier);
  dateTime          = String_new();

  if (indexHandle != NULL)
  {
    do
    {
      // get total storage size, find oldest storage entry
      totalStorageSize      = 0LL;
      oldestUUIDId          = INDEX_ID_NONE;
      oldestStorageId       = INDEX_ID_NONE;
      oldestEntityId        = INDEX_ID_NONE;
      String_clear(oldestStorageName);
      oldestCreatedDateTime = MAX_UINT64;
      oldestSize            = 0LL;
      error = Index_initListStorages(&indexQueryHandle,
                                     indexHandle,
                                     INDEX_ID_ANY,  // uuidId
                                     INDEX_ID_ANY,  // entityId
                                     jobUUID,
                                     NULL,  // scheduleUUID,
                                     NULL,  // indexIds
                                     0,   // indexIdCount
                                     INDEX_TYPE_SET_ALL,
                                       INDEX_STATE_SET(INDEX_STATE_OK)
                                     | INDEX_STATE_SET(INDEX_STATE_UPDATE_REQUESTED)
                                     | INDEX_STATE_SET(INDEX_STATE_ERROR),
                                     INDEX_MODE_SET(INDEX_MODE_AUTO),
                                     NULL,  // hostName
                                     NULL,  // userName
                                     NULL,  // name
                                     INDEX_STORAGE_SORT_MODE_NONE,
                                     DATABASE_ORDERING_NONE,
                                     0LL,  // offset
                                     INDEX_UNLIMITED
                                    );
      if (error != ERROR_NONE)
      {
        logMessage(logHandle,
                   LOG_TYPE_STORAGE,
                   "Purging storage for job '%s' fail (error: %s)",
                   String_cString(jobUUID),
                   Error_getText(error)
                  );
        break;
      }
      while (Index_getNextStorage(&indexQueryHandle,
                                  &uuidId,
                                  NULL,  // jobUUID,
                                  &entityId,
                                  NULL,  // scheduleUUID
                                  NULL,  // hostName
                                  NULL,  // userName
                                  NULL,  // comment
                                  NULL,  // createdDateTime
                                  NULL,  // archiveType
                                  &storageId,
                                  storageName,
                                  &createdDateTime,
                                  &size,
                                  NULL,  // indexState
                                  NULL,  // indexMode
                                  NULL,  // lastCheckedDateTime
                                  NULL,  // errorMessage
                                  NULL,  // totalEntryCount
                                  NULL  // totalEntrySize
                                 )
            )
      {
//fprintf(stderr,"%s, %d: %llu %s: createdDateTime=%llu size=%llu\n",__FILE__,__LINE__,storageId,String_cString(storageName),createdDateTime,size);
        if (createdDateTime < oldestCreatedDateTime)
        {
          oldestUUIDId          = uuidId;
          oldestEntityId        = entityId;
          oldestStorageId       = storageId;
          String_set(oldestStorageName,storageName);
          oldestCreatedDateTime = createdDateTime;
          oldestSize            = size;
        }
        totalStorageSize += size;
      }
      Index_doneList(&indexQueryHandle);

//fprintf(stderr,"%s, %d: totalStorageSize=%llu limit=%llu oldestStorageId=%llu\n",__FILE__,__LINE__,totalStorageSize,limit,oldestStorageId);
      if ((totalStorageSize > limit) && !INDEX_ID_IS_NONE((oldestStorageId)))
      {
        // delete oldest storage entry
        error = Storage_parseName(&storageSpecifier,oldestStorageName);
        if (error == ERROR_NONE)
        {
          error = Storage_init(&storageInfo,
//TODO
NULL, // masterIO
                               &storageSpecifier,
                               NULL,  // jobOptions
                               &globalOptions.indexDatabaseMaxBandWidthList,
                               SERVER_CONNECTION_PRIORITY_HIGH,
                               STORAGE_FLAGS_NONE,
                               CALLBACK_(NULL,NULL),  // updateStatusInfo
                               CALLBACK_(NULL,NULL),  // getPassword
                               CALLBACK_(NULL,NULL),  // requestVolume
                               CALLBACK_(NULL,NULL),  // isPause
                               CALLBACK_(NULL,NULL),  // isAborte
                               logHandle
                              );
          if (error == ERROR_NONE)
          {
            // delete storage
            (void)Storage_delete(&storageInfo,
                                 NULL  // archiveName
                                );

            // prune empty directories
            (void)Storage_pruneDirectories(&storageInfo,oldestStorageName);
          }
          else
          {
            logMessage(logHandle,
                       LOG_TYPE_STORAGE,
                       "Purging storage '%s', %.1f%s (%llu bytes) fail (error: %s)",
                       String_cString(oldestStorageName),
                       BYTES_SHORT(oldestSize),
                       BYTES_UNIT(oldestSize),
                       oldestSize,
                       Error_getText(error)
                      );
          }
          Storage_done(&storageInfo);
        }

        // delete index of storage
        error = Index_deleteStorage(indexHandle,oldestStorageId);
        if (error != ERROR_NONE)
        {
          logMessage(logHandle,
                     LOG_TYPE_STORAGE,
                     "Purging storage index #%llu fail (error: %s)",
                     oldestStorageId,
                     Error_getText(error)
                    );
          break;
        }
        (void)Index_pruneEntity(indexHandle,oldestEntityId);
        (void)Index_pruneUUID(indexHandle,oldestUUIDId);

        // log
        Misc_formatDateTime(String_clear(dateTime),oldestCreatedDateTime,NULL);
        logMessage(logHandle,
                   LOG_TYPE_STORAGE,
                   "Job size limit exceeded (max %.1f%s): purged storage '%s', created at %s, %llu bytes",
                   BYTES_SHORT(maxStorageSize),
                   BYTES_UNIT(maxStorageSize),
                   String_cString(oldestStorageName),
                   String_cString(dateTime),
                   oldestSize
                  );
      }
    }
    while (   (totalStorageSize > limit)
           && !INDEX_ID_IS_NONE(oldestStorageId)
          );
  }

  // free resources
  String_delete(dateTime);
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(oldestStorageName);
  String_delete(storageName);
}

/***********************************************************************\
* Name   : purgeStorageByServer
* Purpose: purge old storages by serer
* Input  : indexHandle    - index handle or NULL if no index
*          jobUUID        - job UUID
*          limit          - limit [bytes] or 0
*          maxStorageSize - max. storage size [bytes] or 0
*          logHandle      - log handle (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void purgeStorageByServer(IndexHandle  *indexHandle,
                                const Server *server,
                                uint64       limit,
                                uint64       maxStorageSize,
                                LogHandle    *logHandle
                               )
{
  String           storageName;
  StorageSpecifier storageSpecifier;
  Errors           error;
  uint64           totalStorageSize;
  IndexId          oldestUUIDId;
  IndexId          oldestStorageId;
  IndexId          oldestEntityId;
  String           oldestStorageName;
  uint64           oldestCreatedDateTime;
  uint64           oldestSize;
  IndexQueryHandle indexQueryHandle;
  IndexId          uuidId;
  IndexId          entityId;
  IndexId          storageId;
  uint64           createdDateTime;
  uint64           size;
  StorageInfo      storageInfo;
  String           dateTime;

  assert(server != NULL);
  assert(maxStorageSize > 0LL);

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  oldestStorageName    = String_new();
  dateTime             = String_new();

  if (indexHandle != NULL)
  {
    do
    {
      // get total storage size, find oldest storage entry
      totalStorageSize      = 0LL;
      oldestUUIDId          = INDEX_ID_NONE;
      oldestStorageId       = INDEX_ID_NONE;
      oldestEntityId        = INDEX_ID_NONE;
      String_clear(oldestStorageName);
      oldestCreatedDateTime = MAX_UINT64;
      oldestSize            = 0LL;
      error = Index_initListStorages(&indexQueryHandle,
                                     indexHandle,
                                     INDEX_ID_ANY,  // uuidId
                                     INDEX_ID_ANY,  // entityId
                                     NULL,  // jobUUID,
                                     NULL,  // scheduleUUID,
                                     NULL,  // indexIds
                                     0,   // indexIdCount
                                     INDEX_TYPE_SET_ALL,
                                       INDEX_STATE_SET(INDEX_STATE_OK)
                                     | INDEX_STATE_SET(INDEX_STATE_UPDATE_REQUESTED)
                                     | INDEX_STATE_SET(INDEX_STATE_ERROR),
                                     INDEX_MODE_SET(INDEX_MODE_AUTO),
                                     NULL,  // hostName
                                     NULL,  // userName
                                     NULL,  // name
                                     INDEX_STORAGE_SORT_MODE_NONE,
                                     DATABASE_ORDERING_NONE,
                                     0LL,  // offset
                                     INDEX_UNLIMITED
                                    );
      if (error != ERROR_NONE)
      {
        logMessage(logHandle,
                   LOG_TYPE_STORAGE,
                   "Purging storage for server '%s' fail (error: %s)",
                   String_cString(server->name),
                   Error_getText(error)
                  );
        break;
      }
      while (Index_getNextStorage(&indexQueryHandle,
                                  &uuidId,
                                  NULL,  // jobUUID,
                                  &entityId,
                                  NULL,  // scheduleUUID
                                  NULL,  // hostName
                                  NULL,  // userName
                                  NULL,  // comment
                                  NULL,  // createdDateTime
                                  NULL,  // archiveType
                                  &storageId,
                                  storageName,
                                  &createdDateTime,
                                  &size,
                                  NULL,  // indexState
                                  NULL,  // indexMode
                                  NULL,  // lastCheckedDateTime
                                  NULL,  // errorMessage
                                  NULL,  // totalEntryCount
                                  NULL  // totalEntrySize
                                 )
            )
      {
//fprintf(stderr,"%s, %d: %llu %s: %llu\n",__FILE__,__LINE__,storageId,String_cString(storageName),createdDateTime);
        error = Storage_parseName(&storageSpecifier,storageName);
        if (   (error == ERROR_NONE)
            && String_equals(storageSpecifier.hostName,server->name)
           )
        {
          if (createdDateTime < oldestCreatedDateTime)
          {
            oldestUUIDId          = uuidId;
            oldestEntityId        = entityId;
            oldestStorageId       = storageId;
            String_set(oldestStorageName,storageName);
            oldestCreatedDateTime = createdDateTime;
            oldestSize            = size;
          }
          totalStorageSize += size;
        }
      }
      Index_doneList(&indexQueryHandle);

      if ((totalStorageSize > limit) && !INDEX_ID_IS_NONE(oldestStorageId))
      {
        // delete oldest storage entry
        error = Storage_parseName(&storageSpecifier,oldestStorageName);
        if (error == ERROR_NONE)
        {
          error = Storage_init(&storageInfo,
//TODO
NULL, // masterIO
                               &storageSpecifier,
                               NULL,  // jobOptions
                               &globalOptions.indexDatabaseMaxBandWidthList,
                               SERVER_CONNECTION_PRIORITY_HIGH,
                               STORAGE_FLAGS_NONE,
                               CALLBACK_(NULL,NULL),  // updateStatusInfo
                               CALLBACK_(NULL,NULL),  // getPassword
                               CALLBACK_(NULL,NULL),  // requestVolume
                               CALLBACK_(NULL,NULL),  // isPause
                               CALLBACK_(NULL,NULL),  // isAborted
                               logHandle
                              );
          if (error == ERROR_NONE)
          {
            // delete storage
            (void)Storage_delete(&storageInfo,
                                 NULL  // archiveName
                                );

            // prune empty directories
            (void)Storage_pruneDirectories(&storageInfo,oldestStorageName);
          }
          else
          {
            logMessage(logHandle,
                       LOG_TYPE_STORAGE,
                       "Purging storage '%s', %.1f%s (%llu bytes) fail (error: %s)",
                       String_cString(oldestStorageName),
                       BYTES_SHORT(oldestSize),
                       BYTES_UNIT(oldestSize),
                       oldestSize,
                       Error_getText(error)
                      );
          }
          Storage_done(&storageInfo);
        }

        // delete index of storage
        error = Index_deleteStorage(indexHandle,oldestStorageId);
        if (error != ERROR_NONE)
        {
          logMessage(logHandle,
                     LOG_TYPE_STORAGE,
                     "Purging storage index #%llu fail (error: %s)",
                     oldestStorageId,
                     Error_getText(error)
                    );
          break;
        }
        (void)Index_pruneEntity(indexHandle,oldestEntityId);
        (void)Index_pruneUUID(indexHandle,oldestUUIDId);

        // log
        Misc_formatDateTime(dateTime,oldestCreatedDateTime,NULL);
        logMessage(logHandle,
                   LOG_TYPE_STORAGE,
                   "Server size limit exceeded (max %.1f%s): purged storage '%s', created at %s, %llu bytes",
                   BYTES_SHORT(maxStorageSize),
                   BYTES_UNIT(maxStorageSize),
                   String_cString(oldestStorageName),
                   String_cString(dateTime),
                   oldestSize
                  );
      }
    }
    while (   (totalStorageSize > limit)
           && !INDEX_ID_IS_NONE(oldestStorageId)
          );
  }

  // free resources
  String_delete(dateTime);
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(oldestStorageName);
  String_delete(storageName);
}

/***********************************************************************\
* Name   : storageThreadCode
* Purpose: archive storage thread
* Input  : createInfo - create info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void storageThreadCode(CreateInfo *createInfo)
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
  uint64           storageSize;
  bool             appendFlag;
  StorageHandle    storageHandle;
//  ulong            bufferLength;
  String           pattern;

  String           directoryName;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;
  IndexId          existingEntityId;
  IndexId          existingStorageId;
  String           existingStorageName;
  String           existingDirectoryName;
  StorageSpecifier existingStorageSpecifier;

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);

  // init variables
  AutoFree_init(&autoFreeList);
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  printableStorageName  = String_new();
  directoryName         = String_new();
  existingStorageName   = String_new();
  existingDirectoryName = String_new();
  Storage_initSpecifier(&existingStorageSpecifier);
  AUTOFREE_ADD(&autoFreeList,buffer,{ free(buffer); });
  AUTOFREE_ADD(&autoFreeList,printableStorageName,{ String_delete(printableStorageName); });
  AUTOFREE_ADD(&autoFreeList,directoryName,{ String_delete(directoryName); });
  AUTOFREE_ADD(&autoFreeList,existingStorageName,{ String_delete(existingStorageName); });
  AUTOFREE_ADD(&autoFreeList,existingDirectoryName,{ String_delete(existingDirectoryName); });
  AUTOFREE_ADD(&autoFreeList,&existingStorageSpecifier,{ Storage_doneSpecifier(&existingStorageSpecifier); });

  // initial storage pre-processing
  if (   (createInfo->failError == ERROR_NONE)
      && !createInfo->storageFlags.dryRun
     )
  {
    // pause
    Storage_pause(&createInfo->storageInfo);

    // pre-process
    if (!isAborted(createInfo))
    {
      error = Storage_preProcess(&createInfo->storageInfo,NULL,createInfo->createdDateTime,TRUE);
      if (error != ERROR_NONE)
      {
        printError("Cannot pre-process storage (error: %s)!",
                   Error_getText(error)
                  );
        createInfo->failError = error;
        AutoFree_cleanup(&autoFreeList);
        return;
      }
    }
  }

  // store archives
  while (   (createInfo->failError == ERROR_NONE)
         && !isAborted(createInfo)
        )
  {
    autoFreeSavePoint = AutoFree_save(&autoFreeList);

    // pause, check abort
    Storage_pause(&createInfo->storageInfo);
    if ((createInfo->failError != ERROR_NONE) || isAborted(createInfo))
    {
      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      break;
    }

    // get next archive to store
    if (!MsgQueue_get(&createInfo->storageMsgQueue,&storageMsg,NULL,sizeof(storageMsg),WAIT_FOREVER))
    {
      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      break;
    }
    AUTOFREE_ADD(&autoFreeList,&storageMsg,
                 {
                   storageInfoDecrement(createInfo,storageMsg.fileSize);
                   File_delete(storageMsg.fileName,FALSE);
                   freeStorageMsg(&storageMsg,NULL);
                 }
                );
    if ((createInfo->failError != ERROR_NONE) || isAborted(createInfo))
    {
      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      break;
    }

    if (!createInfo->storageFlags.dryRun)
    {
      // get printable storage name
      Storage_getPrintableName(printableStorageName,&createInfo->storageInfo.storageSpecifier,storageMsg.archiveName);

      // pre-process
      error = Storage_preProcess(&createInfo->storageInfo,storageMsg.archiveName,createInfo->createdDateTime,FALSE);
      if (error != ERROR_NONE)
      {
        if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

        printError("Cannot pre-process file '%s' (error: %s)!",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );

        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        break;
      }
      DEBUG_TESTCODE() { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); break; }

      // get file info
      error = File_getInfo(&fileInfo,storageMsg.fileName);
      if (error != ERROR_NONE)
      {
        if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

        printError("Cannot get information for file '%s' (error: %s)",
                   String_cString(storageMsg.fileName),
                   Error_getText(error)
                  );

        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        break;
      }
      DEBUG_TESTCODE() { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); break; }

      // check storage size, purge old archives
      if (!createInfo->storageFlags.dryRun && (createInfo->jobOptions->maxStorageSize > 0LL))
      {
        // purge archives by max. job storage size
        purgeStorageByJobUUID(createInfo->indexHandle,
                              createInfo->jobUUID,
                              (createInfo->jobOptions->maxStorageSize > fileInfo.size)
                                ? createInfo->jobOptions->maxStorageSize-fileInfo.size
                                : 0LL,
                              createInfo->jobOptions->maxStorageSize,
                              createInfo->logHandle
                             );

        // purge archives by max. server storage size
        Configuration_initServer(&server,NULL,SERVER_TYPE_NONE);
        Storage_getServerSettings(&server,&createInfo->storageInfo.storageSpecifier,createInfo->jobOptions);
        if (server.maxStorageSize > fileInfo.size)
        {
          purgeStorageByServer(createInfo->indexHandle,
                               &server,
                               server.maxStorageSize-fileInfo.size,
                               server.maxStorageSize,
                               createInfo->logHandle
                              );
        }
        Configuration_doneServer(&server);
      }

      // open file to store
      #ifndef NDEBUG
        printInfo(1,"Store '%s' to '%s'...",String_cString(storageMsg.fileName),String_cString(printableStorageName));
      #else /* not NDEBUG */
        printInfo(1,"Store '%s'...",String_cString(printableStorageName));
      #endif /* NDEBUG */
      error = File_open(&fileHandle,storageMsg.fileName,FILE_OPEN_READ);
      if (error != ERROR_NONE)
      {
        if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

        printInfo(0,"FAIL!\n");
        printError("Cannot open file '%s' (error: %s)!",
                   String_cString(storageMsg.fileName),
                   Error_getText(error)
                  );

        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        break;
      }
      AUTOFREE_ADD(&autoFreeList,&fileHandle,{ File_close(&fileHandle); });
      DEBUG_TESTCODE() { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); continue; }

      // create storage
      retryCount  = 0;
      appendFlag  = FALSE;
      storageSize = 0LL;
      do
      {
        // next try
        retryCount++;

        // pause, check abort/error
        Storage_pause(&createInfo->storageInfo);
        if (isAborted(createInfo) || (createInfo->failError != ERROR_NONE))
        {
          break;
        }

        // check if append to storage
        appendFlag =    (createInfo->storageInfo.jobOptions != NULL)
                     && (createInfo->storageInfo.jobOptions->archiveFileMode == ARCHIVE_FILE_MODE_APPEND)
                     && Storage_exists(&createInfo->storageInfo,storageMsg.archiveName);

        // create/append storage file
        error = Storage_create(&storageHandle,
                               &createInfo->storageInfo,
                               storageMsg.archiveName,
                               fileInfo.size,
                               FALSE  // forceFlag
                              );
        if (error != ERROR_NONE)
        {
          if (retryCount < MAX_RETRIES)
          {
            // retry
            continue;
          }
          else
          {
            // create file -> abort
            break;
          }
        }
        DEBUG_TESTCODE() { Storage_close(&storageHandle); error = DEBUG_TESTCODE_ERROR(); break; }
        AUTOFREE_ADD(&autoFreeList,&storageMsg,
                     {
                       if (!appendFlag && !INDEX_ID_IS_NONE(storageMsg.storageId))
                       {
                         Index_deleteStorage(createInfo->indexHandle,storageMsg.storageId);
                       }
                     }
                    );

        // update status info
        STATUS_INFO_UPDATE(createInfo,NULL,NULL)
        {
          String_set(createInfo->statusInfo.storage.name,printableStorageName);
        }

        // pause, check abort/error
        Storage_pause(&createInfo->storageInfo);
        if (isAborted(createInfo) || (createInfo->failError != ERROR_NONE))
        {
          (void)Storage_close(&storageHandle);
          (void)Storage_delete(&createInfo->storageInfo,storageMsg.archiveName);
          break;
        }

        // transfer file data into storage
        error = Storage_transfer(&storageHandle,&fileHandle);
        if (error != ERROR_NONE)
        {
          (void)Storage_close(&storageHandle);
          (void)Storage_delete(&createInfo->storageInfo,storageMsg.archiveName);

          if (retryCount < MAX_RETRIES)
          {
            // retry
            continue;
          }
          else
          {
            // write fail -> abort
            break;
          }
        }
        DEBUG_TESTCODE() { Storage_close(&storageHandle); Storage_delete(&createInfo->storageInfo,storageMsg.archiveName); error = DEBUG_TESTCODE_ERROR(); break; }

        // pause, check abort/error
        Storage_pause(&createInfo->storageInfo);
        if (isAborted(createInfo) || (createInfo->failError != ERROR_NONE))
        {
          (void)Storage_close(&storageHandle);
          (void)Storage_delete(&createInfo->storageInfo,storageMsg.archiveName);
          break;
        }

//TODO: on error restore to original size/delete

        // get storage size
        storageSize = Storage_getSize(&storageHandle);

        // close storage
        Storage_close(&storageHandle);
      }
      while (   (createInfo->failError == ERROR_NONE)                            // no eror
             && !isAborted(createInfo)                                           // not aborted
             && ((error != ERROR_NONE) && (Error_getErrno(error) != ENOSPC))     // some error and not "no space left"
             && (retryCount < MAX_RETRIES)                                       // still some retry left
            );
      if (error != ERROR_NONE)
      {
        if (createInfo->failError == ERROR_NONE) createInfo->failError = error;
      }

      // close file to store
      File_close(&fileHandle);
      AUTOFREE_REMOVE(&autoFreeList,&fileHandle);

      // check if aborted/error
      if      (isAborted(createInfo))
      {
        printInfo(1,"ABORTED\n");
        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        break;
      }
      else if (createInfo->failError != ERROR_NONE)
      {
        printInfo(0,"FAIL!\n");
        printError("Cannot store '%s' (error: %s)!",
                   String_cString(printableStorageName),
                   Error_getText(createInfo->failError)
                  );
        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        break;
      }

      // update index database and set state
      if (   (createInfo->indexHandle != NULL)
          && !INDEX_ID_IS_NONE(storageMsg.storageId)
         )
      {
        assert(!INDEX_ID_IS_NONE(storageMsg.entityId));

        // check if append and storage exists => assign to existing storage index
        if (   appendFlag
            && Index_findStorageByName(createInfo->indexHandle,
                                       &createInfo->storageInfo.storageSpecifier,
                                       storageMsg.archiveName,
                                       NULL,  // uuidId
                                       NULL,  // entityId
                                       NULL,  // jobUUID,
                                       NULL,  // scheduleUUID,
                                       &storageId,
                                       NULL,  // createdDateTime
                                       NULL,  // size
                                       NULL,  // indexMode
                                       NULL,  // indexState
                                       NULL,  // lastCheckedDateTime
                                       NULL,  // errorMessage
                                       NULL,  // totalEntryCount
                                       NULL  // totalEntrySize
                                      )
           )
        {
          // set index database state
          error = Index_setStorageState(createInfo->indexHandle,
                                        storageId,
                                        INDEX_STATE_CREATE,
                                        0LL,  // lastCheckedDateTime
                                        NULL // errorMessage
                                       );
          if (error != ERROR_NONE)
          {
            if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

            printInfo(1,"FAIL\n");
            printError("Cannot update index for storage '%s' (error: %s)!",
                       String_cString(printableStorageName),
                       Error_getText(error)
                      );

            AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
            break;
          }
          AUTOFREE_ADD(&autoFreeList,&storageMsg.storageId,
          {
            (void)Index_setStorageState(createInfo->indexHandle,
                                        storageId,
                                        INDEX_STATE_ERROR,
                                        0LL,  // lastCheckedDateTime
                                        NULL // errorMessage
                                       );
          });

          // append index: assign storage index entries to existing storage index
//fprintf(stderr,"%s, %d: append to storage %llu\n",__FILE__,__LINE__,storageId);
          error = Index_assignTo(createInfo->indexHandle,
                                 NULL,  // jobUUID
                                 INDEX_ID_NONE,  // entityId
                                 storageMsg.storageId,
                                 NULL,  // jobUUID
                                 INDEX_ID_NONE,  // toEntityId
                                 ARCHIVE_TYPE_NONE,
                                 storageId
                                );
          if (error != ERROR_NONE)
          {
            if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

            printInfo(1,"FAIL\n");
            printError("Cannot update index for storage '%s' (error: %s)!",
                       String_cString(printableStorageName),
                       Error_getText(error)
                      );

            AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
            break;
          }

          // prune storage (maybe empty now)
          (void)Index_pruneStorage(createInfo->indexHandle,storageMsg.storageId);

          // prune entity (maybe empty now)
          (void)Index_pruneEntity(createInfo->indexHandle,storageMsg.entityId);
        }
        else
        {
//fprintf(stderr,"%s, %d: --- new storage \n",__FILE__,__LINE__);
          // replace index: keep new storage
          storageId = storageMsg.storageId;
          AUTOFREE_ADD(&autoFreeList,&storageMsg.storageId,
          {
            // nothing to do
          });

          // delete old indizes for same storage file
          error = purgeStorageIndex(createInfo->indexHandle,
                                    storageMsg.storageId,
                                    &createInfo->storageInfo.storageSpecifier,
                                    storageMsg.archiveName
                                   );
          if (error != ERROR_NONE)
          {
            if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

            printInfo(1,"FAIL\n");
            printError("Cannot delete old index for storage '%s' (error: %s)!",
                       String_cString(printableStorageName),
                       Error_getText(error)
                      );

            AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
            break;
          }

          // append storage to existing entity which has the same storage directory
          if (createInfo->storageInfo.jobOptions->archiveFileMode == ARCHIVE_FILE_MODE_APPEND)
          {
//fprintf(stderr,"%s, %d: append to entity of uuid %llu\n",__FILE__,__LINE__,storageMsg.uuidId);
            Storage_getPrintableName(printableStorageName,&createInfo->storageInfo.storageSpecifier,storageMsg.archiveName);

            // find matching entity and assign storage to entity
            File_getDirectoryName(directoryName,storageMsg.archiveName);
            error = Index_initListStorages(&indexQueryHandle,
                                           createInfo->indexHandle,
                                           storageMsg.uuidId,
                                           INDEX_ID_ANY, // entityId
                                           NULL,  // jobUUID,
                                           NULL,  // scheduleUUID,
                                           NULL,  // indexIds
                                           0,  // indexIdCount
                                           INDEX_TYPE_SET_ALL,
                                           INDEX_STATE_SET_ALL,
                                           INDEX_MODE_SET_ALL,
                                           NULL,  // hostName
                                           NULL,  // userName
                                           NULL,  // name,
                                           INDEX_STORAGE_SORT_MODE_NONE,
                                           DATABASE_ORDERING_NONE,
                                           0LL,  // offset
                                           INDEX_UNLIMITED
                                          );
            if (error != ERROR_NONE)
            {
              if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

              AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
              break;
            }
            while (Index_getNextStorage(&indexQueryHandle,
                                        NULL,  // uuidId
                                        NULL,  // job UUID
                                        &existingEntityId,
                                        NULL,  // schedule UUID
                                        NULL,  // hostName
                                        NULL,  // userName
                                        NULL,  // comment
                                        NULL,  // createdDateTime
                                        NULL,  // archiveType
                                        &existingStorageId,
                                        existingStorageName,
                                        NULL,  // createdDateTime
                                        NULL,  // size
                                        NULL,  // indexState
                                        NULL,  // indexMode
                                        NULL,  // lastCheckedDateTime
                                        NULL,  // errorMessage
                                        NULL,  // totalEntryCount
                                        NULL  // totalEntrySize
                                       )
                  )
            {
              File_getDirectoryName(existingDirectoryName,existingStorageName);
              if (   !INDEX_ID_EQUALS(storageId,existingStorageId)
                  && (Storage_parseName(&existingStorageSpecifier,existingStorageName) == ERROR_NONE)
                  && Storage_equalSpecifiers(&existingStorageSpecifier,directoryName,&existingStorageSpecifier,existingDirectoryName)
                 )
              {
//fprintf(stderr,"%s, %d: assign to existingStorageName=%s\n",__FILE__,__LINE__,String_cString(existingStorageName));
                error = Index_assignTo(createInfo->indexHandle,
                                       NULL,  // jobUUID
                                       INDEX_ID_NONE,  // entityId
                                       storageId,
                                       NULL,  // jobUUID
                                       existingEntityId,
                                       ARCHIVE_TYPE_NONE,
                                       INDEX_ID_NONE  // toStorageId
                                      );
                break;
              }
            }
            Index_doneList(&indexQueryHandle);
            if (error != ERROR_NONE)
            {
              if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

              printInfo(1,"FAIL\n");
              printError("Cannot delete old index for storage '%s' (error: %s)!",
                         String_cString(printableStorageName),
                         Error_getText(error)
                        );

              AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
              break;
            }

            // prune entity (maybe empty now)
            (void)Index_pruneEntity(createInfo->indexHandle,storageMsg.entityId);
          }
        }

        // update index database storage name and size
        if (error == ERROR_NONE)
        {
          error = Index_updateStorage(createInfo->indexHandle,
                                      storageId,
                                      NULL,  // hostName
                                      NULL,  // userName
                                      printableStorageName,
                                      0,  // createDateTime
                                      storageSize,
                                      NULL,  // comment
                                      TRUE  // update newest entries
                                     );
        }

        // update storages info (aggregated values)
        if (error == ERROR_NONE)
        {
          error = Index_updateStorageInfos(createInfo->indexHandle,
                                           storageId
                                          );
        }

        // set index database state and time stamp
        if (error == ERROR_NONE)
        {
          error = Index_setStorageState(createInfo->indexHandle,
                                        storageId,
                                        ((createInfo->failError == ERROR_NONE) && !isAborted(createInfo))
                                          ? INDEX_STATE_OK
                                          : INDEX_STATE_ERROR,
                                        Misc_getCurrentDateTime(),
                                        NULL // errorMessage
                                       );
        }
        if (error != ERROR_NONE)
        {
          if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

          printInfo(1,"FAIL\n");
          printError("Cannot update index for storage '%s' (error: %s)!",
                     String_cString(printableStorageName),
                     Error_getText(error)
                    );

          AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
          break;
        }
        DEBUG_TESTCODE() { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); continue; }

        AUTOFREE_REMOVE(&autoFreeList,&storageMsg.storageId);
      }

      // done
      printInfo(1,"OK (%llu bytes)\n",storageSize);
      logMessage(createInfo->logHandle,
                 LOG_TYPE_STORAGE,
                 "%s '%s' (%llu bytes)",
                 appendFlag ? "Appended to" : "Stored",
                 String_cString(printableStorageName),
                 storageSize
                );

      // post-process
      error = Storage_postProcess(&createInfo->storageInfo,storageMsg.archiveName,createInfo->createdDateTime,FALSE);
      if (error != ERROR_NONE)
      {
        printError("Cannot post-process storage file '%s' (error: %s)!",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
        if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        break;
      }
      DEBUG_TESTCODE() { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); continue; }

      // delete temporary storage file
      error = File_delete(storageMsg.fileName,FALSE);
      if (error != ERROR_NONE)
      {
        printWarning("Cannot delete file '%s' (error: %s)!",
                     String_cString(storageMsg.fileName),
                     Error_getText(error)
                    );
      }

      // add to list of stored archive files
      StringList_append(&createInfo->storageFileList,storageMsg.archiveName);

      // update storage info
      storageInfoDecrement(createInfo,storageMsg.fileSize);

    }

    // free resources
    freeStorageMsg(&storageMsg,NULL);

    AutoFree_restore(&autoFreeList,autoFreeSavePoint,FALSE);
  }

  // discard unprocessed archives
  while (MsgQueue_get(&createInfo->storageMsgQueue,&storageMsg,NULL,sizeof(storageMsg),NO_WAIT))
  {
    // discard index
    if (!INDEX_ID_IS_NONE(storageMsg.storageId)) Index_deleteStorage(createInfo->indexHandle,storageMsg.storageId);

    // delete temporary storage file
    error = File_delete(storageMsg.fileName,FALSE);
    if (error != ERROR_NONE)
    {
      printWarning("Cannot delete file '%s' (error: %s)!",
                   String_cString(storageMsg.fileName),
                   Error_getText(error)
                  );
    }

    // free resources
    freeStorageMsg(&storageMsg,NULL);
  }

  // final storage post-processing
  if (   (createInfo->failError == ERROR_NONE)
      && !createInfo->storageFlags.dryRun
     )
  {
    // pause
    Storage_pause(&createInfo->storageInfo);

    // post-processing
    if (!isAborted(createInfo))
    {
      error = Storage_postProcess(&createInfo->storageInfo,NULL,createInfo->createdDateTime,TRUE);
      if (error != ERROR_NONE)
      {
        printError("Cannot post-process storage (error: %s)!",
                   Error_getText(error)
                  );
        if (createInfo->failError == ERROR_NONE) createInfo->failError = error;
      }
    }
  }

//TODO: required?
  // delete old storage files if no database
  if (   (createInfo->failError == ERROR_NONE)
      && !isAborted(createInfo)
      && (createInfo->indexHandle == NULL)
     )
  {
    if (globalOptions.deleteOldArchiveFilesFlag)
    {
      // get archive name pattern
      pattern = String_new();
      error = formatArchiveFileName(pattern,
                                    createInfo->storageInfo.storageSpecifier.archiveName,
                                    EXPAND_MACRO_MODE_PATTERN,
                                    createInfo->archiveType,
                                    createInfo->scheduleTitle,
                                    createInfo->scheduleCustomText,
                                    createInfo->createdDateTime,
                                    ARCHIVE_PART_NUMBER_NONE
                                   );
      if (error == ERROR_NONE)
      {
        // delete all matching storage files which are unknown
        (void)Storage_forAll(&createInfo->storageInfo.storageSpecifier,
                             NULL,  // directory
                             String_cString(pattern),
                             CALLBACK_INLINE(Errors,(ConstString storageName, const FileInfo *fileInfo, void *userData),
                             {
                               StorageSpecifier storageSpecifier;
                               Errors           error;

                               UNUSED_VARIABLE(fileInfo);
                               UNUSED_VARIABLE(userData);

                               // init variables
                               Storage_initSpecifier(&storageSpecifier);

                               error = Storage_parseName(&storageSpecifier,storageName);
                               if (error == ERROR_NONE)
                               {
                                 // find in storage list
                                 if (StringList_find(&createInfo->storageFileList,storageSpecifier.archiveName) == NULL)
                                 {
                                   Storage_delete(&createInfo->storageInfo,storageName);
                                 }
                               }

                               // free resources
                               Storage_doneSpecifier(&storageSpecifier);

                               return error;
                             },NULL),
                             CALLBACK_(NULL,NULL)
                            );
      }
      String_delete(pattern);
    }
  }

  // free resoures
  Storage_doneSpecifier(&existingStorageSpecifier);
  String_delete(existingDirectoryName);
  String_delete(existingStorageName);
  String_delete(directoryName);
  String_delete(printableStorageName);
  free(buffer);
  AutoFree_done(&autoFreeList);

  createInfo->storageThreadExitFlag = TRUE;
}

/***********************************************************************\
* Name   : fragmentInit
* Purpose: init fragment
* Input  : createInfo    - create info structure
*          name          - name of entry
*          size          - total size of entry [bytes] or 0
*          fragmentCount - fragment count
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void fragmentInit(CreateInfo *createInfo, ConstString name, uint64 size, uint fragmentCount)
{
  FragmentNode *fragmentNode;

  assert(createInfo != NULL);
  assert(name != NULL);

  STATUS_INFO_UPDATE(createInfo,name,NULL)
  {
    // get/create fragment node
    fragmentNode = FragmentList_find(&createInfo->statusInfoFragmentList,name);
    if (fragmentNode == NULL)
    {
      fragmentNode = FragmentList_add(&createInfo->statusInfoFragmentList,name,size,NULL,0,fragmentCount);
      if (fragmentNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
    }
    assert(fragmentNode != NULL);

    // status update
    if (   (createInfo->statusInfoCurrentFragmentNode == NULL)
        || ((Misc_getTimestamp()-createInfo->statusInfoCurrentLastUpdateTimestamp) >= 10*US_PER_S)
       )
    {
      createInfo->statusInfoCurrentFragmentNode        = fragmentNode;
      createInfo->statusInfoCurrentLastUpdateTimestamp = Misc_getTimestamp();

      String_set(createInfo->statusInfo.entry.name,name);
      createInfo->statusInfo.entry.doneSize  = FragmentList_getSize(fragmentNode);
      createInfo->statusInfo.entry.totalSize = FragmentList_getTotalSize(fragmentNode);
    }
    assert(createInfo->statusInfoCurrentFragmentNode != NULL);
  }
}

/***********************************************************************\
* Name   : fragmentDone
* Purpose: done fragment
* Input  : createInfo - create info structure
*          name       - name of entry
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void fragmentDone(CreateInfo *createInfo, ConstString name)
{
  FragmentNode *fragmentNode;

  assert(createInfo != NULL);
  assert(name != NULL);

  SEMAPHORE_LOCKED_DO(&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // get fragment node
    fragmentNode = FragmentList_find(&createInfo->statusInfoFragmentList,name);
    if (fragmentNode != NULL)
    {
      if (fragmentNode->size > 0LL)
      {
        // unlock
        FragmentList_unlockNode(fragmentNode);
      }

      // check if fragment complete
      if (FragmentList_isComplete(fragmentNode))
      {
        if (fragmentNode == createInfo->statusInfoCurrentFragmentNode)
        {
          createInfo->statusInfoCurrentFragmentNode = NULL;
        }

        FragmentList_discard(&createInfo->statusInfoFragmentList,fragmentNode);
      }
    }
  }
}

/***********************************************************************\
* Name   : storeFileEntry
* Purpose: store a file entry into archive
* Input  : createInfo     - create info structure
*          fileName       - file name to store
*          fileInfo       - file info
*          fragmentNumber - fragment number [0..n-1]
*          fragmentCount  - fragment count
*          fragmentOffset - fragment offset [bytes]
*          fragmentSize   - fragment size [bytes]
*          buffer         - buffer for temporary data
*          bufferSize     - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeFileEntry(CreateInfo     *createInfo,
                            ConstString    fileName,
                            const FileInfo *fileInfo,
                            uint           fragmentNumber,
                            uint           fragmentCount,
                            uint64         fragmentOffset,
                            uint64         fragmentSize,
                            byte           *buffer,
                            uint           bufferSize
                           )
{
  Errors                    error;
  FileExtendedAttributeList fileExtendedAttributeList;
  FileHandle                fileHandle;
  ArchiveFlags              archiveFlags;
  ArchiveEntryInfo          archiveEntryInfo;
  uint64                    offset;
  uint64                    size;
  ulong                     bufferLength;
  FragmentNode              *fragmentNode;
  uint64                    archiveSize;
  uint                      percentageDone;
  double                    compressionRatio;
  char                      t1[16],t2[16];
  char                      s1[256],s2[256];

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);
  assert(fileName != NULL);
  assert(fileInfo != NULL);
  assert((fragmentCount == 0) || (fragmentNumber < fragmentCount));
  assert(buffer != NULL);

  printInfo(1,"Add file      '%s'...",String_cString(fileName));

  // get file extended attributes
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,fileName);
  if (error != ERROR_NONE)
  {
//TODO
    if      (createInfo->jobOptions->noStopOnAttributeErrorFlag)
    {
      printInfo(2,"Cannot not get extended attributes for '%s' - continue (error: %s)!",
                String_cString(fileName),
                Error_getText(error)
               );
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_INCOMPLETE,
                 "Cannot get extended attributes for '%s' - continue (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );
    }
    else if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_ACCESS_DENIED,
                 "Access denied '%s' (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
      {
        createInfo->statusInfo.error.count++;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get extended attributes for '%s' (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  // open file
  error = File_open(&fileHandle,fileName,FILE_OPEN_READ|FILE_OPEN_NO_ATIME|FILE_OPEN_NO_CACHE);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_ACCESS_DENIED,
                 "Open file failed '%s' (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
      {
        createInfo->statusInfo.error.count++;
        createInfo->statusInfo.error.size += (uint64)fileInfo->size;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot open file '%s' (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  // init fragment
  fragmentInit(createInfo,fileName,fileInfo->size,fragmentCount);

  if (!createInfo->storageFlags.noStorage)
  {
    offset       = 0LL;
    size         = 0LL;
    archiveFlags = ARCHIVE_FLAG_NONE;

    // check if file data should be delta compressed
    if (   (fileInfo->size > globalOptions.compressMinFileSize)
        && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.delta)
       )
    {
       archiveFlags |= ARCHIVE_FLAG_TRY_DELTA_COMPRESS;
    }

    // check if file data should be byte compressed
    if (   (fileInfo->size > globalOptions.compressMinFileSize)
        && !PatternList_match(&createInfo->jobOptions->compressExcludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
       )
    {
       archiveFlags |= ARCHIVE_FLAG_TRY_BYTE_COMPRESS;
    }

    // create new archive file entry
    error = Archive_newFileEntry(&archiveEntryInfo,
                                 &createInfo->archiveHandle,
                                 createInfo->jobOptions->compressAlgorithms.delta,
                                 createInfo->jobOptions->compressAlgorithms.byte,
                                 fileName,
                                 fileInfo,
                                 &fileExtendedAttributeList,
                                 fragmentOffset,
                                 fragmentSize,
                                 archiveFlags
                                );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive file entry '%s' (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );
      (void)File_close(&fileHandle);
      fragmentDone(createInfo,fileName);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }

    // seek to start offset
    error = File_seek(&fileHandle,fragmentOffset);
    if (error == ERROR_NONE)
    {
      // write file content to archive
      offset = fragmentOffset;
      size   = fragmentSize;
      error  = ERROR_NONE;
      while (   (createInfo->failError == ERROR_NONE)
             && !isAborted(createInfo)
             && (error == ERROR_NONE)
             && (size > 0LL)
            )
      {
//fprintf(stderr,"%s, %d: fragmentOffset=%llu size=%llu\n",__FILE__,__LINE__,fragmentOffset,size);
        // pause
        Storage_pause(&createInfo->storageInfo);

        // read file data
        error = File_read(&fileHandle,buffer,MIN(size,bufferSize),&bufferLength);
        if (error == ERROR_NONE)
        {
          if (bufferLength > 0L)
          {
            // write data to archive
            error = Archive_writeData(&archiveEntryInfo,buffer,bufferLength,1);
            if (error == ERROR_NONE)
            {
              // get current archive size
              archiveSize = Archive_getSize(&createInfo->archiveHandle);

              STATUS_INFO_UPDATE(createInfo,fileName,&fragmentNode)
              {
                if (fragmentNode != NULL)
                {
                  // add fragment
                  FragmentList_addRange(fragmentNode,offset,bufferLength);

                  // update status info
                  if (fragmentNode == createInfo->statusInfoCurrentFragmentNode)
                  {
                    createInfo->statusInfo.entry.doneSize   = FragmentList_getSize(createInfo->statusInfoCurrentFragmentNode);
                    createInfo->statusInfo.entry.totalSize  = FragmentList_getTotalSize(createInfo->statusInfoCurrentFragmentNode);
                  }
                }
                createInfo->statusInfo.done.size        = createInfo->statusInfo.done.size+(uint64)bufferLength;
                createInfo->statusInfo.archiveSize      = archiveSize+createInfo->statusInfo.storage.totalSize;
                createInfo->statusInfo.compressionRatio = (!createInfo->storageFlags.dryRun && (createInfo->statusInfo.done.size > 0))
                                                            ? 100.0-(archiveSize*100.0)/createInfo->statusInfo.done.size
                                                            : 0.0;
              }
              offset += bufferLength;
            }
            else
            {
              logMessage(createInfo->logHandle,
                         LOG_TYPE_ERROR,
                         "Write archive failed (error: %s)",
                         Error_getText(error)
                        );
            }

            if (isPrintInfo(2))
            {
              percentageDone = 0;
              STATUS_INFO_GET(createInfo,fileName)
              {
                percentageDone = (createInfo->statusInfo.entry.totalSize > 0LL)
                                   ? (uint)((createInfo->statusInfo.entry.doneSize*100LL)/createInfo->statusInfo.entry.totalSize)
                                   : 100;
              }
              printInfo(2,"%3d%%\b\b\b\b",percentageDone);
            }

            assert(size >= bufferLength);
            size -= bufferLength;
          }
          else
          {
            // read nohting -> file size changed -> done
            size = 0;
          }
        }
        else
        {
          logMessage(createInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "Read file failed '%s' (error: %s)",
                     String_cString(fileName),
                     Error_getText(error)
                    );
        }

        // wait for temporary file space
        waitForTemporaryFileSpace(createInfo);
      }
      if (isAborted(createInfo))
      {
        printInfo(1,"ABORTED\n");
        (void)Archive_closeEntry(&archiveEntryInfo);
        (void)File_close(&fileHandle);
        fragmentDone(createInfo,fileName);
        File_doneExtendedAttributes(&fileExtendedAttributeList);
        return FALSE;
      }
    }
    if (error != ERROR_NONE)
    {
      if (createInfo->jobOptions->skipUnreadableFlag)
      {
        printInfo(1,"skipped (reason: %s)\n",Error_getText(error));

        STATUS_INFO_UPDATE(createInfo,fileName,NULL)
        {
          createInfo->statusInfo.error.count++;
          createInfo->statusInfo.error.size += fragmentSize;
        }

        (void)Archive_closeEntry(&archiveEntryInfo);
        (void)File_close(&fileHandle);
        fragmentDone(createInfo,fileName);
        File_doneExtendedAttributes(&fileExtendedAttributeList);

        return ERROR_NONE;
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError("Cannot store file entry (error: %s)!",
                   Error_getText(error)
                  );

        (void)Archive_closeEntry(&archiveEntryInfo);
        (void)File_close(&fileHandle);
        fragmentDone(createInfo,fileName);
        File_doneExtendedAttributes(&fileExtendedAttributeList);

        return error;
      }
    }
    printInfo(2,"    \b\b\b\b");

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive file entry (error: %s)!",
                 Error_getText(error)
                );
      (void)File_close(&fileHandle);
      fragmentDone(createInfo,fileName);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }

    // get final compression ratio
    if (archiveEntryInfo.file.chunkFileData.fragmentSize > 0LL)
    {
      compressionRatio = 100.0-archiveEntryInfo.file.chunkFileData.info.size*100.0/archiveEntryInfo.file.chunkFileData.fragmentSize;
    }
    else
    {
      compressionRatio = 0.0;
    }

    // get fragment info
    stringClear(s1);
    if (fragmentSize < fileInfo->size)
    {
      stringFormat(t1,sizeof(t1),"%u",fragmentCount);
      stringFormat(t2,sizeof(t2),"%u",fragmentNumber+1);
      stringAppend(s1,sizeof(s1),", fragment #");
      stringFill(s1,sizeof(s1),stringLength(t1)-stringLength(t2),' ');
      stringFormatAppend(s1,sizeof(s1),"%s/%s",t2,t1);
    }

    // ratio info
    stringClear(s2);
    if (   ((archiveFlags & ARCHIVE_FLAG_TRY_DELTA_COMPRESS) && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.delta))
        || ((archiveFlags & ARCHIVE_FLAG_TRY_BYTE_COMPRESS ) && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.byte ))
       )
    {
      stringFormat(s2,sizeof(s2),", ratio %5.1f%%",compressionRatio);
    }

    if (!createInfo->storageFlags.dryRun)
    {
      printInfo(1,"OK (%llu bytes%s%s)\n",
                fragmentSize,
                s1,
                s2
               );
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_OK,
                 "Added file '%s' (%llu bytes%s%s)",
                 String_cString(fileName),
                 fragmentSize,
                 s1,
                 s2
                );
    }
    else
    {
      printInfo(1,"OK (%llu bytes%s%s, dry-run)\n",
                fragmentSize,
                s1,
                s2
               );
    }
  }
  else
  {
    STATUS_INFO_UPDATE(createInfo,fileName,&fragmentNode)
    {
      if (fragmentNode != NULL)
      {
        // add fragment
        FragmentList_addRange(fragmentNode,0,fileInfo->size);
      }
    }

    printInfo(1,"OK (%llu bytes, not stored)\n",
              fragmentSize
             );
  }

  // update status info
  STATUS_INFO_UPDATE(createInfo,fileName,&fragmentNode)
  {
    if (fragmentNode != NULL)
    {
      if (fragmentNode == createInfo->statusInfoCurrentFragmentNode)
      {
        createInfo->statusInfo.entry.doneSize   = FragmentList_getSize(createInfo->statusInfoCurrentFragmentNode);
        createInfo->statusInfo.entry.totalSize  = FragmentList_getTotalSize(createInfo->statusInfoCurrentFragmentNode);
      }
      if (FragmentList_isComplete(fragmentNode))
      {
        createInfo->statusInfo.done.count++;
      }
    }
  }

  // close file
  (void)File_close(&fileHandle);

  // free resources
  fragmentDone(createInfo,fileName);
  File_doneExtendedAttributes(&fileExtendedAttributeList);

  // add to incremental list
  if (createInfo->storeIncrementalFileInfoFlag)
  {
    addIncrementalList(&createInfo->namesDictionary,fileName,fileInfo);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeImageEntry
* Purpose: store an image entry into archive
* Input  : createInfo     - create info structure
*          deviceName     - device name
*          deviceInfo     - device info
*          fragmentNumber - fragment number [0..n-1]
*          fragmentCount  - fragment count
*          fragmentOffset - fragment offset [blocks]
*          fragmentSize   - fragment count [blocks]
*          buffer         - buffer for temporary data
*          bufferSize     - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeImageEntry(CreateInfo       *createInfo,
                             ConstString      deviceName,
                             const DeviceInfo *deviceInfo,
                             uint             fragmentNumber,
                             uint             fragmentCount,
                             uint64           fragmentOffset,
                             uint64           fragmentSize,
                             byte             *buffer,
                             uint             bufferSize
                            )
{
  Errors           error;
  uint             maxBufferBlockCount;
  DeviceHandle     deviceHandle;
  bool             fileSystemFlag;
  FileSystemHandle fileSystemHandle;
  ArchiveFlags     archiveFlags;
  ArchiveEntryInfo archiveEntryInfo;
  uint64           blockOffset;
  uint64           blockCount;
  uint             bufferBlockCount;
  FragmentNode     *fragmentNode;
  uint64           archiveSize;
  uint             percentageDone;
  double           compressionRatio;
  char             t1[16],t2[16];
  char             s1[256],s2[256];

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);
  assert(deviceName != NULL);
  assert(deviceInfo != NULL);
  assert((fragmentCount == 0) || (fragmentNumber < fragmentCount));
  assert(buffer != NULL);

  printInfo(1,"Add image     '%s'...",String_cString(deviceName));

  // check device block size, get max. blocks in buffer
  if (deviceInfo->blockSize > bufferSize)
  {
    printInfo(1,"FAIL\n");
    printError("Device block size %llu on '%s' is too big (max: %llu)",
               deviceInfo->blockSize,
               String_cString(deviceName),
               bufferSize
              );
    return ERROR_INVALID_DEVICE_BLOCK_SIZE;
  }
  if (deviceInfo->blockSize <= 0)
  {
    printInfo(1,"FAIL\n");
    printError("Invalid device block size for '%s'",
               String_cString(deviceName)
              );
    return ERROR_INVALID_DEVICE_BLOCK_SIZE;
  }
  assert(deviceInfo->blockSize > 0);
  maxBufferBlockCount = bufferSize/deviceInfo->blockSize;

  // open device
  error = Device_open(&deviceHandle,deviceName,DEVICE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_ACCESS_DENIED,
                 "Open device failed '%s' (error: %s)",
                 String_cString(deviceName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,deviceName,NULL)
      {
        createInfo->statusInfo.error.count++;
        createInfo->statusInfo.error.size += (uint64)deviceInfo->size;
      }

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot open device '%s' (error: %s)",
                 String_cString(deviceName),
                 Error_getText(error)
                );

      return error;
    }
  }

  // check if device contain a known file system or a raw image should be stored
  memClear(&fileSystemHandle,sizeof(FileSystemHandle));
  if (!createInfo->jobOptions->rawImagesFlag)
  {
    fileSystemFlag = (FileSystem_init(&fileSystemHandle,&deviceHandle) == ERROR_NONE);
  }
  else
  {
    fileSystemHandle.type = FILE_SYSTEM_TYPE_NONE;
    fileSystemFlag = FALSE;
  }

  // init fragment
  fragmentInit(createInfo,deviceName,deviceInfo->size,fragmentCount);

  if (!createInfo->storageFlags.noStorage)
  {
    blockOffset  = 0LL;
    blockCount   = 0LL;
    archiveFlags = ARCHIVE_FLAG_NONE;

    // check if file data should be delta compressed
    if (   (deviceInfo->size > globalOptions.compressMinFileSize)
        && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.delta)
       )
    {
       archiveFlags |= ARCHIVE_FLAG_TRY_DELTA_COMPRESS;
    }

    // check if file data should be byte compressed
    if (   (deviceInfo->size > globalOptions.compressMinFileSize)
        && !PatternList_match(&createInfo->jobOptions->compressExcludePatternList,deviceName,PATTERN_MATCH_MODE_EXACT)
       )
    {
       archiveFlags |= ARCHIVE_FLAG_TRY_BYTE_COMPRESS;
    }

    // create new archive image entry
    error = Archive_newImageEntry(&archiveEntryInfo,
                                  &createInfo->archiveHandle,
                                  createInfo->jobOptions->compressAlgorithms.delta,
                                  createInfo->jobOptions->compressAlgorithms.byte,
                                  deviceName,
                                  deviceInfo,
                                  fileSystemHandle.type,
                                  fragmentOffset/(uint64)deviceInfo->blockSize,
                                  (fragmentSize+(uint64)deviceInfo->blockSize-1)/(uint64)deviceInfo->blockSize,
                                  archiveFlags
                                 );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive image entry '%s' (error: %s)",
                 String_cString(deviceName),
                 Error_getText(error)
                );
      if (fileSystemFlag) FileSystem_done(&fileSystemHandle);
      Device_close(&deviceHandle);
      fragmentDone(createInfo,deviceName);
      return error;
    }

    // write device content to archive
    blockOffset = fragmentOffset/(uint64)deviceInfo->blockSize;
    blockCount  = (fragmentSize+(uint64)deviceInfo->blockSize-1)/(uint64)deviceInfo->blockSize;
    error       = ERROR_NONE;
    while (   (createInfo->failError == ERROR_NONE)
           && !isAborted(createInfo)
           && (error == ERROR_NONE)
           && (blockCount > 0LL)
          )
    {
      // pause
      Storage_pause(&createInfo->storageInfo);

      // read blocks from device
      bufferBlockCount = 0;
      while (   (blockCount > 0LL)
             && (bufferBlockCount < maxBufferBlockCount)
            )
      {
        if (   !fileSystemFlag
            || FileSystem_blockIsUsed(&fileSystemHandle,(blockOffset+(uint64)bufferBlockCount)*(uint64)deviceInfo->blockSize)
           )
        {
          // seek to block
          error = Device_seek(&deviceHandle,(blockOffset+(uint64)bufferBlockCount)*(uint64)deviceInfo->blockSize);
          if (error != ERROR_NONE) break;

          // read block
          error = Device_read(&deviceHandle,buffer+bufferBlockCount*deviceInfo->blockSize,deviceInfo->blockSize,NULL);
          if (error != ERROR_NONE) break;
        }
        else
        {
          // block not used -> store as "0"-block
          memClear(buffer+bufferBlockCount*deviceInfo->blockSize,deviceInfo->blockSize);
        }
        bufferBlockCount++;
        blockCount--;
      }
      if (error != ERROR_NONE) break;

      // write data to archive
      if (bufferBlockCount > 0)
      {
        error = Archive_writeData(&archiveEntryInfo,buffer,bufferBlockCount*deviceInfo->blockSize,deviceInfo->blockSize);
        if (error == ERROR_NONE)
        {
          // get current archive size
          archiveSize = Archive_getSize(&createInfo->archiveHandle);

          // update status info
          STATUS_INFO_UPDATE(createInfo,deviceName,&fragmentNode)
          {
            if (fragmentNode != NULL)
            {
              // add fragment
              FragmentList_addRange(fragmentNode,blockOffset*(uint64)deviceInfo->blockSize,(uint64)bufferBlockCount*(uint64)deviceInfo->blockSize);

              // update status info
              if (fragmentNode == createInfo->statusInfoCurrentFragmentNode)
              {
                createInfo->statusInfo.entry.doneSize   = FragmentList_getSize(createInfo->statusInfoCurrentFragmentNode);
                createInfo->statusInfo.entry.totalSize  = FragmentList_getTotalSize(createInfo->statusInfoCurrentFragmentNode);
              }
            }
            createInfo->statusInfo.done.size        = createInfo->statusInfo.done.size+(uint64)bufferBlockCount*(uint64)deviceInfo->blockSize;
            createInfo->statusInfo.archiveSize      = archiveSize+createInfo->statusInfo.storage.totalSize;
            createInfo->statusInfo.compressionRatio = (!createInfo->storageFlags.dryRun && (createInfo->statusInfo.done.size > 0))
                                                        ? 100.0-(archiveSize*100.0)/createInfo->statusInfo.done.size
                                                        : 0.0;
          }
          blockOffset += (uint64)bufferBlockCount;
        }
        else
        {
          logMessage(createInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "Write archive failed (error: %s)",
                     Error_getText(error)
                    );
        }

        if (isPrintInfo(2))
        {
          percentageDone = 0;
          STATUS_INFO_GET(createInfo,deviceName)
          {
            percentageDone = (createInfo->statusInfo.entry.totalSize > 0LL)
                               ? (uint)((createInfo->statusInfo.entry.doneSize*100LL)/createInfo->statusInfo.entry.totalSize)
                               : 100;
          }
          printInfo(2,"%3d%%\b\b\b\b",percentageDone);
        }
      }
      else
      {
        logMessage(createInfo->logHandle,
                   LOG_TYPE_ERROR,
                   "Read device failed '%s' (error: %s)",
                   String_cString(deviceName),
                   Error_getText(error)
                  );
      }

      // wait for temporary file space
      waitForTemporaryFileSpace(createInfo);
    }
    if (isAborted(createInfo))
    {
      printInfo(1,"ABORTED\n");
      (void)Archive_closeEntry(&archiveEntryInfo);
      if (fileSystemFlag) FileSystem_done(&fileSystemHandle);
      Device_close(&deviceHandle);
      fragmentDone(createInfo,deviceName);
      return error;
    }
    if (error != ERROR_NONE)
    {
      if (createInfo->jobOptions->skipUnreadableFlag)
      {
        printInfo(1,"skipped (reason: %s)\n",Error_getText(error));

        STATUS_INFO_UPDATE(createInfo,deviceName,NULL)
        {
          createInfo->statusInfo.error.count++;
          createInfo->statusInfo.error.size += fragmentSize;
        }

        (void)Archive_closeEntry(&archiveEntryInfo);
        if (fileSystemFlag) FileSystem_done(&fileSystemHandle);
        Device_close(&deviceHandle);
        fragmentDone(createInfo,deviceName);

        return ERROR_NONE;
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError("Cannot store image entry (error: %s)!",
                   Error_getText(error)
                  );

        (void)Archive_closeEntry(&archiveEntryInfo);
        if (fileSystemFlag) FileSystem_done(&fileSystemHandle);
        Device_close(&deviceHandle);
        fragmentDone(createInfo,deviceName);

        return error;
      }
    }
    printInfo(2,"    \b\b\b\b");

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive image entry (error: %s)!",
                 Error_getText(error)
                );
      if (fileSystemFlag) FileSystem_done(&fileSystemHandle);
      Device_close(&deviceHandle);
      fragmentDone(createInfo,deviceName);
      return error;
    }

    // done file system
    if (fileSystemFlag)
    {
      FileSystem_done(&fileSystemHandle);
    }

    // get final compression ratio
    if (archiveEntryInfo.image.chunkImageData.blockCount > 0)
    {
      compressionRatio = 100.0-archiveEntryInfo.image.chunkImageData.info.size*100.0/(archiveEntryInfo.image.chunkImageData.blockCount*(uint64)deviceInfo->blockSize);
    }
    else
    {
      compressionRatio = 0.0;
    }

    // get fragment info
    stringClear(s1);
    if (fragmentSize < deviceInfo->size)
    {
      stringFormat(t1,sizeof(t1),"%u",fragmentCount);
      stringFormat(t2,sizeof(t2),"%u",fragmentNumber+1);
      stringAppend(s1,sizeof(s1),", fragment #");
      stringFill(s1,sizeof(s1),stringLength(t1)-stringLength(t2),' ');
      stringFormatAppend(s1,sizeof(s1),"%s/%s",t2,t1);
    }

    // get ratio info
    stringClear(s2);
    if (   ((archiveFlags & ARCHIVE_FLAG_TRY_DELTA_COMPRESS) && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.delta))
        || ((archiveFlags & ARCHIVE_FLAG_TRY_BYTE_COMPRESS ) && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.byte ))
       )
    {
      stringFormat(s2,sizeof(s2),", ratio %5.1f%%",compressionRatio);
    }

    // output result
    if (!createInfo->storageFlags.dryRun)
    {
      printInfo(1,"OK (%s, %llu bytes%s%s)\n",
                (fileSystemFlag && (fileSystemHandle.type != FILE_SYSTEM_TYPE_UNKNOWN)) ? FileSystem_fileSystemTypeToString(fileSystemHandle.type,NULL) : "raw",
                fragmentSize,
                s1,
                s2
               );
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_OK,
                 "Added image '%s' (%s, %llu bytes%s%s)",
                 String_cString(deviceName),
                 (fileSystemFlag && (fileSystemHandle.type != FILE_SYSTEM_TYPE_UNKNOWN)) ? FileSystem_fileSystemTypeToString(fileSystemHandle.type,NULL) : "raw",
                 fragmentSize,
                 s1,
                 s2
                );
    }
    else
    {
      printInfo(1,"OK (%s, %llu bytes%s%s, dry-run)\n",
                fileSystemFlag ? FileSystem_fileSystemTypeToString(fileSystemHandle.type,NULL) : "raw",
                fragmentSize,
                s1,
                s2
               );
    }
  }
  else
  {
    STATUS_INFO_UPDATE(createInfo,deviceName,&fragmentNode)
    {
      if (fragmentNode != NULL)
      {
        // add fragment
        FragmentList_addRange(fragmentNode,0,deviceInfo->size);
      }
    }

    printInfo(1,"OK (%s, %llu bytes, not stored)\n",
              fileSystemFlag ? FileSystem_fileSystemTypeToString(fileSystemHandle.type,NULL) : "raw",
              fragmentSize
             );
  }

  // update status info
  STATUS_INFO_UPDATE(createInfo,deviceName,&fragmentNode)
  {
    if (fragmentNode != NULL)
    {
      if (fragmentNode == createInfo->statusInfoCurrentFragmentNode)
      {
        createInfo->statusInfo.entry.doneSize   = FragmentList_getSize(createInfo->statusInfoCurrentFragmentNode);
        createInfo->statusInfo.entry.totalSize  = FragmentList_getTotalSize(createInfo->statusInfoCurrentFragmentNode);
      }

      if (FragmentList_isComplete(fragmentNode))
      {
        createInfo->statusInfo.done.count++;
      }
    }
  }

  // close device
  Device_close(&deviceHandle);

  // free resources
  fragmentDone(createInfo,deviceName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeDirectoryEntry
* Purpose: store a directory entry into archive
* Input  : createInfo    - create info structure
*          directoryName - directory name to store
*          fileInfo      - file info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeDirectoryEntry(CreateInfo     *createInfo,
                                 ConstString    directoryName,
                                 const FileInfo *fileInfo
                                )
{
  Errors                    error;
  FileExtendedAttributeList fileExtendedAttributeList;
  ArchiveEntryInfo          archiveEntryInfo;

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);
  assert(directoryName != NULL);
  assert(fileInfo != NULL);

  printInfo(1,"Add directory '%s'...",String_cString(directoryName));

  // get file extended attributes
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,directoryName);
  if (error != ERROR_NONE)
  {
//TODO
    if      (createInfo->jobOptions->noStopOnAttributeErrorFlag)
    {
      printInfo(2,"Cannot not get extended attributes for '%s' - continue (error: %s)!",
                String_cString(directoryName),
                Error_getText(error)
               );
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_INCOMPLETE,
                 "Cannot get extended attributes for '%s' - continue (error: %s)",
                 String_cString(directoryName),
                 Error_getText(error)
                );
    }
    else if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_ACCESS_DENIED,
                 "Access denied '%s' (error: %s)",
                 String_cString(directoryName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,directoryName,NULL)
      {
        createInfo->statusInfo.error.count++;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get extended attributes for '%s' (error: %s)",
                 String_cString(directoryName),
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }
  }

  if (!createInfo->storageFlags.noStorage)
  {
    // new directory
    error = Archive_newDirectoryEntry(&archiveEntryInfo,
                                      &createInfo->archiveHandle,
                                      directoryName,
                                      fileInfo,
                                      &fileExtendedAttributeList
                                     );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive directory entry '%s' (error: %s)",
                 String_cString(directoryName),
                 Error_getText(error)
                );
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_ACCESS_DENIED,
                 "Open failed '%s' (error: %s)",
                 String_cString(directoryName),
                 Error_getText(error)
                );

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive directory entry (error: %s)!",
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }

    // output result
    if (!createInfo->storageFlags.dryRun)
    {
      printInfo(1,"OK\n");
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_OK,
                 "Added directory '%s'",
                 String_cString(directoryName)
                );
    }
    else
    {
      printInfo(1,"OK (dry-run)\n");
    }
  }
  else
  {
    printInfo(1,"OK (not stored)\n");
  }

  // update status info
  STATUS_INFO_UPDATE(createInfo,directoryName,NULL)
  {
    createInfo->statusInfo.done.count++;
  }

  // free resources
  File_doneExtendedAttributes(&fileExtendedAttributeList);

  // add to incremental list
  if (createInfo->storeIncrementalFileInfoFlag)
  {
    addIncrementalList(&createInfo->namesDictionary,directoryName,fileInfo);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeLinkEntry
* Purpose: store a link entry into archive
* Input  : createInfo  - create info structure
*          linkName    - link name to store
*          fileInfo    - file info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeLinkEntry(CreateInfo     *createInfo,
                            ConstString    linkName,
                            const FileInfo *fileInfo
                           )
{
  Errors                    error;
  FileExtendedAttributeList fileExtendedAttributeList;
  String                    fileName;
  ArchiveEntryInfo          archiveEntryInfo;

  assert(createInfo != NULL);
  assert(linkName != NULL);
  assert(fileInfo != NULL);

  printInfo(1,"Add link      '%s'...",String_cString(linkName));

  // get file extended attributes
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,linkName);
  if (error != ERROR_NONE)
  {
//TODO
    if      (createInfo->jobOptions->noStopOnAttributeErrorFlag)
    {
      printInfo(2,"Cannot not get extended attributes for '%s' - continue (error: %s)!",
                String_cString(linkName),
                Error_getText(error)
               );
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_INCOMPLETE,
                 "Cannot get extended attributes for '%s' - continue (error: %s)",
                 String_cString(linkName),
                 Error_getText(error)
                );
    }
    else if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_ACCESS_DENIED,
                 "Access denied '%s' (error: %s)",
                 String_cString(linkName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,linkName,NULL)
      {
        createInfo->statusInfo.error.count++;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get extended attributes for '%s' (error: %s)",
                 String_cString(linkName),
                 Error_getText(error)
                );

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  if (!createInfo->storageFlags.noStorage)
  {
    // read link
    fileName = String_new();
    error = File_readLink(fileName,linkName,FALSE);
    if (error != ERROR_NONE)
    {
      if (createInfo->jobOptions->skipUnreadableFlag)
      {
        printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
        logMessage(createInfo->logHandle,
                   LOG_TYPE_ENTRY_ACCESS_DENIED,
                   "Open failed '%s' (error: %s)",
                   String_cString(linkName),
                   Error_getText(error)
                  );

        STATUS_INFO_UPDATE(createInfo,linkName,NULL)
        {
          createInfo->statusInfo.error.count++;
          createInfo->statusInfo.error.size += (uint64)fileInfo->size;
        }

        String_delete(fileName);
        File_doneExtendedAttributes(&fileExtendedAttributeList);

        return ERROR_NONE;
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError("Cannot read link '%s' (error: %s)",
                   String_cString(linkName),
                   Error_getText(error)
                  );

        String_delete(fileName);
        File_doneExtendedAttributes(&fileExtendedAttributeList);

        return error;
      }
    }

    // new linke
    error = Archive_newLinkEntry(&archiveEntryInfo,
                                 &createInfo->archiveHandle,
                                 linkName,
                                 fileName,
                                 fileInfo,
                                 &fileExtendedAttributeList
                                );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive link entry '%s' (error: %s)",
                 String_cString(linkName),
                 Error_getText(error)
                );
      String_delete(fileName);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }

    // update status info
    STATUS_INFO_UPDATE(createInfo,linkName,NULL)
    {
      // no additional values
    }

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive link entry (error: %s)!",
                 Error_getText(error)
                );
      String_delete(fileName);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }

    // output result
    if (!createInfo->storageFlags.dryRun)
    {
      printInfo(1,"OK\n");
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_OK,
                 "Added link '%s'\n",
                 String_cString(linkName)
                );
    }
    else
    {
      printInfo(1,"OK (dry-run)\n");
    }

    // free resources
    String_delete(fileName);
  }
  else
  {
    printInfo(1,"OK (not stored)\n");
  }

  // update status info
  STATUS_INFO_UPDATE(createInfo,linkName,NULL)
  {
    createInfo->statusInfo.done.count++;
  }

  // free resources
  File_doneExtendedAttributes(&fileExtendedAttributeList);

  // add to incremental list
  if (createInfo->storeIncrementalFileInfoFlag)
  {
    addIncrementalList(&createInfo->namesDictionary,linkName,fileInfo);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeHardLinkEntry
* Purpose: store a hard link entry into archive
* Input  : createInfo     - create info structure
*          fileNameList   - hard link filename list to store
*          fileInfo       - file info
*          fragmentNumber - fragment number [0..n-1]
*          fragmentCount  - fragment count
*          fragmentOffset - fragment offset [bytes]
*          fragmentSize   - fragment size [bytes]
*          buffer         - buffer for temporary data
*          bufferSize     - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeHardLinkEntry(CreateInfo       *createInfo,
                                const StringList *fileNameList,
                                const FileInfo   *fileInfo,
                                uint             fragmentNumber,
                                uint             fragmentCount,
                                uint64           fragmentOffset,
                                uint64           fragmentSize,
                                byte             *buffer,
                                uint             bufferSize
                               )
{
  Errors                    error;
  FileExtendedAttributeList fileExtendedAttributeList;
  FileHandle                fileHandle;
  ArchiveFlags              archiveFlags;
  ArchiveEntryInfo          archiveEntryInfo;
  uint64                    offset;
  uint64                    size;
  ulong                     bufferLength;
  FragmentNode              *fragmentNode;
  uint64                    archiveSize;
  uint                      percentageDone;
  double                    compressionRatio;
  char                      t1[16],t2[16];
  char                      s1[256],s2[256];
  const StringNode          *stringNode;
  String                    fileName;

  assert(createInfo != NULL);
  assert(fileNameList != NULL);
  assert(!StringList_isEmpty(fileNameList));
  assert(fileInfo != NULL);
  assert((fragmentCount == 0) || (fragmentNumber < fragmentCount));
  assert(buffer != NULL);

  printInfo(1,"Add hardlink  '%s'...",String_cString(StringList_first(fileNameList,NULL)));

  // get file extended attributes
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,StringList_first(fileNameList,NULL));
  if (error != ERROR_NONE)
  {
//TODO
    if      (createInfo->jobOptions->noStopOnAttributeErrorFlag)
    {
      printWarning("Cannot not get extended attributes for '%s' - continue (error: %s)!",
                   String_cString(StringList_first(fileNameList,NULL)),
                   Error_getText(error)
                  );
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_INCOMPLETE,
                 "Cannot get extended attributes for '%s' - continue (error: %s)",
                 String_cString(StringList_first(fileNameList,NULL)),
                 Error_getText(error)
                );
    }
    else if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_ACCESS_DENIED,
                 "Access denied '%s' (error: %s)",
                 String_cString(StringList_first(fileNameList,NULL)),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),NULL)
      {
        createInfo->statusInfo.error.count++;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get extended attributes for '%s' (error: %s)",
                 String_cString(StringList_first(fileNameList,NULL)),
                 Error_getText(error)
                );

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  // open file
  error = File_open(&fileHandle,StringList_first(fileNameList,NULL),FILE_OPEN_READ|FILE_OPEN_NO_ATIME|FILE_OPEN_NO_CACHE);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_ACCESS_DENIED,
                 "Open file failed '%s' (error: %s)",
                 String_cString(StringList_first(fileNameList,NULL)),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),NULL)
      {
        createInfo->statusInfo.error.count += StringList_count(fileNameList);
        createInfo->statusInfo.error.size += (uint64)StringList_count(fileNameList)*(uint64)fileInfo->size;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot open file '%s' (error: %s)",
                 String_cString(StringList_first(fileNameList,NULL)),
                 Error_getText(error)
                );

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  // init fragment
  fragmentInit(createInfo,StringList_first(fileNameList,NULL),fileInfo->size,fragmentCount);

  if (!createInfo->storageFlags.noStorage)
  {
    archiveFlags = ARCHIVE_FLAG_NONE;

    // check if file data should be delta compressed
    if (   (fileInfo->size > globalOptions.compressMinFileSize)
        && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.delta)
       )
    {
       archiveFlags |= ARCHIVE_FLAG_TRY_DELTA_COMPRESS;
    }

    // check if file data should be byte compressed
    if (   (fileInfo->size > globalOptions.compressMinFileSize)
        && !PatternList_matchStringList(&createInfo->jobOptions->compressExcludePatternList,fileNameList,PATTERN_MATCH_MODE_EXACT)
       )
    {
       archiveFlags |= ARCHIVE_FLAG_TRY_BYTE_COMPRESS;
    }

    // create new archive hard link entry
    error = Archive_newHardLinkEntry(&archiveEntryInfo,
                                     &createInfo->archiveHandle,
                                     createInfo->jobOptions->compressAlgorithms.delta,
                                     createInfo->jobOptions->compressAlgorithms.byte,
                                     fileNameList,
                                     fileInfo,
                                     &fileExtendedAttributeList,
                                     fragmentOffset,
                                     fragmentSize,
                                     archiveFlags
                                    );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive hardlink entry '%s' (error: %s)",
                 String_cString(StringList_first(fileNameList,NULL)),
                 Error_getText(error)
                );
      (void)File_close(&fileHandle);
      fragmentDone(createInfo,StringList_first(fileNameList,NULL));
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }

    // seek to start offset
    error = File_seek(&fileHandle,fragmentOffset);
    if (error == ERROR_NONE)
    {
      // write hard link content to archive
      offset = fragmentOffset;
      size   = fragmentSize;
      error  = ERROR_NONE;
      while (   (createInfo->failError == ERROR_NONE)
             && !isAborted(createInfo)
             && (error == ERROR_NONE)
             && (size > 0LL)
            )
      {
//fprintf(stderr,"%s, %d: fragmentOffset=%llu size=%llu\n",__FILE__,__LINE__,fragmentOffset,size);
        // pause create
        pauseCreate(createInfo);

        // read file data
        error = File_read(&fileHandle,buffer,MIN(size,bufferSize),&bufferLength);
        if (error == ERROR_NONE)
        {
          // write data to archive
          if (bufferLength > 0L)
          {
            error = Archive_writeData(&archiveEntryInfo,buffer,bufferLength,1);
            if (error == ERROR_NONE)
            {
              // get current archive size
              archiveSize = Archive_getSize(&createInfo->archiveHandle);

              // update status info
              STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),&fragmentNode)
              {
                if (fragmentNode != NULL)
                {
                  // add fragment
                  FragmentList_addRange(fragmentNode,offset,bufferLength);

                  // update status info
                  if (fragmentNode == createInfo->statusInfoCurrentFragmentNode)
                  {
                    createInfo->statusInfo.entry.doneSize   = FragmentList_getSize(createInfo->statusInfoCurrentFragmentNode);
                    createInfo->statusInfo.entry.totalSize  = FragmentList_getTotalSize(createInfo->statusInfoCurrentFragmentNode);
                  }
                }
                createInfo->statusInfo.done.size        = createInfo->statusInfo.done.size+(uint64)bufferLength;
                createInfo->statusInfo.archiveSize      = archiveSize+createInfo->statusInfo.storage.totalSize;
                createInfo->statusInfo.compressionRatio = (!createInfo->storageFlags.dryRun && (createInfo->statusInfo.done.size > 0))
                                                            ? 100.0-(createInfo->statusInfo.archiveSize *100.0)/createInfo->statusInfo.done.size
                                                            : 0.0;
              }
              offset += bufferLength;
            }
            else
            {
              logMessage(createInfo->logHandle,
                         LOG_TYPE_ERROR,
                         "Write archive failed (error: %s)",
                         Error_getText(error)
                        );
            }

            if (isPrintInfo(2))
            {
              percentageDone = 0;
              STATUS_INFO_GET(createInfo,StringList_first(fileNameList,NULL))
              {
                percentageDone = (createInfo->statusInfo.entry.totalSize > 0LL)
                                   ? (uint)((createInfo->statusInfo.entry.doneSize*100LL)/createInfo->statusInfo.entry.totalSize)
                                   : 100;
              }
              printInfo(2,"%3d%%\b\b\b\b",percentageDone);
            }

            assert(size >= bufferLength);
            size -= bufferLength;
          }
          else
          {
            // read nohting -> file size changed -> done
            size = 0;
          }
        }
        else
        {
          logMessage(createInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "Read hardlink failed '%s' (error: %s)",
                     String_cString(StringList_first(fileNameList,NULL)),
                     Error_getText(error)
                    );
        }

        // wait for temporary file space
        waitForTemporaryFileSpace(createInfo);
      }
      if (isAborted(createInfo))
      {
        printInfo(1,"ABORTED\n");
        (void)Archive_closeEntry(&archiveEntryInfo);
        (void)File_close(&fileHandle);
        fragmentDone(createInfo,StringList_first(fileNameList,NULL));
        File_doneExtendedAttributes(&fileExtendedAttributeList);
        return error;
      }
    }
    if (error != ERROR_NONE)
    {
      if (createInfo->jobOptions->skipUnreadableFlag)
      {
        printInfo(1,"skipped (reason: %s)\n",Error_getText(error));

        STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),NULL)
        {
          createInfo->statusInfo.error.count++;
          createInfo->statusInfo.error.size += fragmentSize;
        }

        (void)Archive_closeEntry(&archiveEntryInfo);
        (void)File_close(&fileHandle);
        fragmentDone(createInfo,StringList_first(fileNameList,NULL));
        File_doneExtendedAttributes(&fileExtendedAttributeList);

        return ERROR_NONE;
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError("Cannot store hardlink entry (error: %s)!",
                   Error_getText(error)
                  );

        (void)Archive_closeEntry(&archiveEntryInfo);
        (void)File_close(&fileHandle);
        fragmentDone(createInfo,StringList_first(fileNameList,NULL));
        File_doneExtendedAttributes(&fileExtendedAttributeList);
        return error;
      }
    }
    printInfo(2,"    \b\b\b\b");

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive hardlink entry (error: %s)!",
                 Error_getText(error)
                );
      (void)File_close(&fileHandle);
      fragmentDone(createInfo,StringList_first(fileNameList,NULL));
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }

    // get final compression ratio
    if (archiveEntryInfo.hardLink.chunkHardLinkData.fragmentSize > 0LL)
    {
      compressionRatio = 100.0-archiveEntryInfo.hardLink.chunkHardLinkData.info.size*100.0/archiveEntryInfo.hardLink.chunkHardLinkData.fragmentSize;
    }
    else
    {
      compressionRatio = 0.0;
    }

    // get fragment info
    stringClear(s1);
    if (fragmentSize < fileInfo->size)
    {
      stringFormat(t1,sizeof(t1),"%u",fragmentCount);
      stringFormat(t2,sizeof(t2),"%u",fragmentNumber+1);
      stringAppend(s1,sizeof(s1),", fragment #");
      stringFill(s1,sizeof(s1),stringLength(t1)-stringLength(t2),' ');
      stringFormatAppend(s1,sizeof(s1),"%s/%s",t2,t1);
    }

    // get ratio info
    stringClear(s2);
    if (   ((archiveFlags & ARCHIVE_FLAG_TRY_DELTA_COMPRESS) && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.delta))
        || ((archiveFlags & ARCHIVE_FLAG_TRY_BYTE_COMPRESS ) && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.byte ))
       )
    {
      stringFormat(s2,sizeof(s2),", ratio %5.1f%%",compressionRatio);
    }

    // output result
    if (!createInfo->storageFlags.dryRun)
    {
      printInfo(1,"OK (%llu bytes%s%s)\n",
                fragmentSize,
                s1,
                s2
               );
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_OK,
                 "Added hardlink '%s' (%llu bytes%s%s)",
                 String_cString(StringList_first(fileNameList,NULL)),
                 fragmentSize,
                 s1,
                 s2
                );
    }
    else
    {
      printInfo(1,"OK (%llu bytes%s%s, dry-run)\n",
                fragmentSize,
                s1,
                s2
               );
    }
  }
  else
  {
    STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),&fragmentNode)
    {
      if (fragmentNode != NULL)
      {
        // add fragment
        FragmentList_addRange(fragmentNode,0,fileInfo->size);
      }
    }

    printInfo(1,"OK (%llu bytes, not stored)\n",
              fragmentSize
             );
  }

  // update status info
  STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),&fragmentNode)
  {
    if (fragmentNode != NULL)
    {
      if (fragmentNode == createInfo->statusInfoCurrentFragmentNode)
      {
        createInfo->statusInfo.entry.doneSize   = FragmentList_getSize(createInfo->statusInfoCurrentFragmentNode);
        createInfo->statusInfo.entry.totalSize  = FragmentList_getTotalSize(createInfo->statusInfoCurrentFragmentNode);
      }

      if (FragmentList_isComplete(fragmentNode))
      {
        createInfo->statusInfo.done.count += StringList_count(fileNameList);
      }
    }
  }

  // close file
  (void)File_close(&fileHandle);

  // free resources
  fragmentDone(createInfo,StringList_first(fileNameList,NULL));
  File_doneExtendedAttributes(&fileExtendedAttributeList);

  // add to incremental list
  if (createInfo->storeIncrementalFileInfoFlag)
  {
    STRINGLIST_ITERATE(fileNameList,stringNode,fileName)
    {
      addIncrementalList(&createInfo->namesDictionary,fileName,fileInfo);
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeSpecialEntry
* Purpose: store a special entry into archive
* Input  : createInfo  - create info structure
*          fileName    - file name to store
*          fileInfo    - file info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeSpecialEntry(CreateInfo     *createInfo,
                               ConstString    fileName,
                               const FileInfo *fileInfo
                              )
{
  Errors                    error;
  FileExtendedAttributeList fileExtendedAttributeList;
  ArchiveEntryInfo          archiveEntryInfo;

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);
  assert(fileName != NULL);
  assert(fileInfo != NULL);

  printInfo(1,"Add special   '%s'...",String_cString(fileName));

  // get file extended attributes
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,fileName);
  if (error != ERROR_NONE)
  {
    if      (createInfo->jobOptions->noStopOnAttributeErrorFlag)
    {
      printInfo(2,"Cannot not get extended attributes for '%s' - continue (error: %s)!",
                String_cString(fileName),
                Error_getText(error)
               );
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_INCOMPLETE,
                 "Cannot get extended attributes for '%s' - continue (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );
    }
    else if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_ACCESS_DENIED,
                 "Access denied '%s' (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
      {
        createInfo->statusInfo.error.count++;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get extended attributes for '%s' (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  if (!createInfo->storageFlags.noStorage)
  {
    // new special
    error = Archive_newSpecialEntry(&archiveEntryInfo,
                                    &createInfo->archiveHandle,
                                    fileName,
                                    fileInfo,
                                    &fileExtendedAttributeList
                                   );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive special entry '%s' (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive special entry (error: %s)!",
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }

    // output result
    if (!createInfo->storageFlags.dryRun)
    {
      printInfo(1,"OK\n");
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_OK,
                 "Added special '%s'",
                 String_cString(fileName)
                );
    }
    else
    {
      printInfo(1,"OK (dry-run)\n");
    }
  }
  else
  {
    printInfo(1,"OK (not stored)\n");
  }

  // update status info
  STATUS_INFO_UPDATE(createInfo,fileName,NULL)
  {
    createInfo->statusInfo.done.count++;
  }

  // free resources
  File_doneExtendedAttributes(&fileExtendedAttributeList);

  // add to incremental list
  if (createInfo->storeIncrementalFileInfoFlag)
  {
    addIncrementalList(&createInfo->namesDictionary,fileName,fileInfo);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : createThreadCode
* Purpose: create worker thread
* Input  : createInfo - create info structure
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void createThreadCode(CreateInfo *createInfo)
{
  byte             *buffer;
  EntryMsg         entryMsg;
  bool             ownFileFlag;
  const StringNode *stringNode;
  String           name;
  Errors           error;

  assert(createInfo != NULL);

  // allocate buffer
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // store entries
  while (   (createInfo->failError == ERROR_NONE)
         && !isAborted(createInfo)
         && MsgQueue_get(&createInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER)
        )
  {
    // check if own file (in temporary directory or storage file)
    ownFileFlag =    String_startsWith(entryMsg.name,tmpDirectory)
                  || StringList_contains(&createInfo->storageFileList,entryMsg.name);
    if (!ownFileFlag)
    {
      STRINGLIST_ITERATE(&entryMsg.nameList,stringNode,name)
      {
        ownFileFlag =    String_startsWith(name,tmpDirectory)
                      || StringList_contains(&createInfo->storageFileList,name);
        if (ownFileFlag) break;
      }
    }

    if (!ownFileFlag)
    {
      // pause create
      pauseCreate(createInfo);

      switch (entryMsg.fileType)
      {
        case FILE_TYPE_FILE:
          switch (entryMsg.entryType)
          {
            case ENTRY_TYPE_FILE:
              error = storeFileEntry(createInfo,
                                     entryMsg.name,
                                     &entryMsg.fileInfo,
                                     entryMsg.fragmentNumber,
                                     entryMsg.fragmentCount,
                                     entryMsg.fragmentOffset,
                                     entryMsg.fragmentSize,
                                     buffer,
                                     BUFFER_SIZE
                                    );
              if (error != ERROR_NONE) createInfo->failError = error;
              break;
            case ENTRY_TYPE_IMAGE:
              // nothing to do
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }
          break;
        case FILE_TYPE_DIRECTORY:
          switch (entryMsg.entryType)
          {
            case ENTRY_TYPE_FILE:
              error = storeDirectoryEntry(createInfo,
                                          entryMsg.name,
                                          &entryMsg.fileInfo
                                         );
              if (error != ERROR_NONE) createInfo->failError = error;
              break;
            case ENTRY_TYPE_IMAGE:
              // nothing to do
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }
          break;
        case FILE_TYPE_LINK:
          switch (entryMsg.entryType)
          {
            case ENTRY_TYPE_FILE:
              error = storeLinkEntry(createInfo,
                                     entryMsg.name,
                                     &entryMsg.fileInfo
                                    );
              if (error != ERROR_NONE) createInfo->failError = error;
              break;
            case ENTRY_TYPE_IMAGE:
              // nothing to do
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }
          break;
        case FILE_TYPE_HARDLINK:
          switch (entryMsg.entryType)
          {
            case ENTRY_TYPE_FILE:
              error = storeHardLinkEntry(createInfo,
                                         &entryMsg.nameList,
                                         &entryMsg.fileInfo,
                                         entryMsg.fragmentNumber,
                                         entryMsg.fragmentCount,
                                         entryMsg.fragmentOffset,
                                         entryMsg.fragmentSize,
                                         buffer,
                                         BUFFER_SIZE
                                        );
              if (error != ERROR_NONE) createInfo->failError = error;
              break;
            case ENTRY_TYPE_IMAGE:
              // nothing to do
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }
          break;
        case FILE_TYPE_SPECIAL:
          switch (entryMsg.entryType)
          {
            case ENTRY_TYPE_FILE:
              error = storeSpecialEntry(createInfo,
                                        entryMsg.name,
                                        &entryMsg.fileInfo
                                       );
              if (error != ERROR_NONE) createInfo->failError = error;
              break;
            case ENTRY_TYPE_IMAGE:
              error = storeImageEntry(createInfo,
                                      entryMsg.name,
                                      &entryMsg.deviceInfo,
                                      entryMsg.fragmentNumber,
                                      entryMsg.fragmentCount,
                                      entryMsg.fragmentOffset,
                                      entryMsg.fragmentSize,
                                      buffer,
                                      BUFFER_SIZE
                                     );
              if (error != ERROR_NONE) createInfo->failError = error;
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break; /* not reached */
      }
    }
    else
    {
      printInfo(1,"Add '%s'...skipped (reason: own created file)\n",String_cString(entryMsg.name));

      STATUS_INFO_UPDATE(createInfo,entryMsg.name,NULL)
      {
        createInfo->statusInfo.skipped.count++;
      }
    }

    // update status info and check if aborted
    SEMAPHORE_LOCKED_DO(&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      updateStatusInfo(createInfo,FALSE);
    }

    // free entry message
    freeEntryMsg(&entryMsg,NULL);

// NYI: is this really useful? (avoid that sum-collector-thread is slower than file-collector-thread)
    // slow down if too fast
    while (   !createInfo->collectorSumThreadExitedFlag
           && (createInfo->statusInfo.done.count >= createInfo->statusInfo.total.count)
          )
    {
      Misc_udelay(1000LL*US_PER_MS);
    }
  }

  // on an error or abort terminated the entry queue
  if (isAborted(createInfo) || (createInfo->failError != ERROR_NONE))
  {
    MsgQueue_setEndOfMsg(&createInfo->entryMsgQueue);
  }

  // free resources
  free(buffer);
}

/*---------------------------------------------------------------------*/

Errors Command_create(ServerIO                     *masterIO,
                      ConstString                  jobUUID,
                      ConstString                  scheduleUUID,
                      ConstString                  scheduleTitle,
                      ConstString                  scheduleCustomText,
                      ConstString                  storageName,
                      const EntryList              *includeEntryList,
                      const PatternList            *excludePatternList,
                      JobOptions                   *jobOptions,
                      ArchiveTypes                 archiveType,
                      uint64                       createdDateTime,
                      StorageFlags                 storageFlags,
                      GetNamePasswordFunction      getNamePasswordFunction,
                      void                         *getNamePasswordUserData,
                      StatusInfoFunction           statusInfoFunction,
                      void                         *statusInfoUserData,
                      StorageRequestVolumeFunction storageRequestVolumeFunction,
                      void                         *storageRequestVolumeUserData,
                      IsPauseFunction              isPauseCreateFunction,
                      void                         *isPauseCreateUserData,
                      IsPauseFunction              isPauseStorageFunction,
                      void                         *isPauseStorageUserData,
                      IsAbortedFunction            isAbortedFunction,
                      void                         *isAbortedUserData,
                      LogHandle                    *logHandle
                     )
{
  AutoFreeList     autoFreeList;
  String           printableStorageName;
  String           incrementalListFileName;
  bool             useIncrementalFileInfoFlag;
  bool             incrementalFileInfoExistFlag;
  IndexHandle      *indexHandle;
  CreateInfo       createInfo;
  Errors           error;
  StorageSpecifier storageSpecifier;
  IndexId          uuidId;
  String           hostName,userName;
  IndexId          entityId;
  Thread           collectorSumThread;                 // files collector sum thread
  Thread           collectorThread;                    // files collector thread
  Thread           storageThread;                      // storage thread
  Thread           *createThreads;
  uint             createThreadCount;
  uint             i;
  String           fileName;

  assert(storageName != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);

  // init variables
  AutoFree_init(&autoFreeList);
  printableStorageName         = String_new();
  incrementalListFileName      = NULL;
  useIncrementalFileInfoFlag   = FALSE;
  incrementalFileInfoExistFlag = FALSE;
  hostName                     = String_new();
  userName                     = String_new();
  AUTOFREE_ADD(&autoFreeList,printableStorageName,{ String_delete(printableStorageName); });
  AUTOFREE_ADD(&autoFreeList,hostName,{ String_delete(hostName); });
  AUTOFREE_ADD(&autoFreeList,userName,{ String_delete(userName); });

  // check if storage name given
  if (String_isEmpty(storageName))
  {
    printError("No storage name given!");
    AutoFree_cleanup(&autoFreeList);
    return ERROR_NO_STORAGE_NAME;
  }

  // parse storage name
  Storage_initSpecifier(&storageSpecifier);
  error = Storage_parseName(&storageSpecifier,storageName);
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize storage '%s' (error: %s)",
               String_cString(storageName),
               Error_getText(error)
              );
    Storage_doneSpecifier(&storageSpecifier);
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Storage_doneSpecifier(&storageSpecifier); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&storageSpecifier,{ Storage_doneSpecifier(&storageSpecifier); });

  // open index
//TODO: each thread need his own handle!
  indexHandle = Index_open(masterIO,INDEX_TIMEOUT);
  AUTOFREE_ADD(&autoFreeList,indexHandle,{ Index_close(indexHandle); });

  // init create info
  initCreateInfo(&createInfo,
                 indexHandle,
                 jobUUID,
                 scheduleUUID,
                 scheduleTitle,
                 scheduleCustomText,
                 includeEntryList,
                 excludePatternList,
                 jobOptions,
                 archiveType,
                 createdDateTime,
                 storageFlags,
                 CALLBACK_(isPauseCreateFunction,isPauseCreateUserData),
                 CALLBACK_(statusInfoFunction,statusInfoUserData),
                 CALLBACK_(isAbortedFunction,isAbortedUserData),
                 logHandle
                );
  AUTOFREE_ADD(&autoFreeList,&createInfo,{ doneCreateInfo(&createInfo); });

  // mount devices
  error = mountAll(&jobOptions->mountList);
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&jobOptions->mountList,{ unmountAll(&jobOptions->mountList); });

  // get printable storage name
  Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);

  // get hostname, username
  Network_getHostName(hostName);
  Misc_getCurrentUserName(userName);

  // init storage
  error = Storage_init(&createInfo.storageInfo,
                       masterIO,
                       &storageSpecifier,
                       jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       storageFlags,
                       CALLBACK_(updateStorageStatusInfo,&createInfo),
                       CALLBACK_(getNamePasswordFunction,getNamePasswordUserData),
                       CALLBACK_(storageRequestVolumeFunction,storageRequestVolumeUserData),
                       CALLBACK_(isPauseStorageFunction,isPauseStorageUserData),
                       CALLBACK_(isAbortedFunction,isAbortedUserData),
                       logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize storage '%s' (error: %s)",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Storage_done(&createInfo.storageInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&createInfo.storageInfo,{ Storage_done(&createInfo.storageInfo); });

#if 0
//TODO: useful?
  // check if write access
  directoryName = File_getDirectoryName(String_new(),storageSpecifier.archiveName);
  if (!Storage_isWritable(&createInfo.storageInfo,directoryName))
  {
    error = ERRORX_(WRITE_FILE,0,"%s",String_cString(storageSpecifier.archiveName));
    printError("Cannot write storage (error: no write access for '%s')!",
               String_cString(storageSpecifier.archiveName)
              );
    String_delete(directoryName);
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  String_delete(directoryName);
#endif

  if (   (createInfo.archiveType == ARCHIVE_TYPE_FULL)
      || (createInfo.archiveType == ARCHIVE_TYPE_INCREMENTAL)
      || (createInfo.archiveType == ARCHIVE_TYPE_DIFFERENTIAL)
      || !String_isEmpty(jobOptions->incrementalListFileName)
     )
  {
    // init names dictionary
    Dictionary_init(&createInfo.namesDictionary,DICTIONARY_BYTE_COPY,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
    AUTOFREE_ADD(&autoFreeList,&createInfo.namesDictionary,{ Dictionary_done(&createInfo.namesDictionary); });

    // get increment list file name
    incrementalListFileName = String_new();
    if (!String_isEmpty(jobOptions->incrementalListFileName))
    {
      String_set(incrementalListFileName,jobOptions->incrementalListFileName);
      incrementalFileInfoExistFlag = File_exists(incrementalListFileName);
    }
    else
    {
      incrementalFileInfoExistFlag = FALSE;

      if (!incrementalFileInfoExistFlag)
      {
        // get incremental file name from storage name
        getIncrementalFileNameFromStorage(incrementalListFileName,
                                          &createInfo.storageInfo.storageSpecifier
                                         );
        incrementalFileInfoExistFlag = File_exists(incrementalListFileName);
      }
      if (!incrementalFileInfoExistFlag && (jobUUID != NULL))
      {
        // get incremental file name from job UUID
        getIncrementalFileNameFromJobUUID(incrementalListFileName,jobUUID);
        incrementalFileInfoExistFlag = File_exists(incrementalListFileName);
      }
    }
    AUTOFREE_ADD(&autoFreeList,incrementalListFileName,{ String_delete(incrementalListFileName); });

    if (   incrementalFileInfoExistFlag
        && (   (createInfo.archiveType == ARCHIVE_TYPE_INCREMENTAL )
            || (createInfo.archiveType == ARCHIVE_TYPE_DIFFERENTIAL)
           )
       )
    {
      // read incremental list
      printInfo(1,"Read incremental list '%s'...",String_cString(incrementalListFileName));
      error = readIncrementalList(&createInfo,
                                  incrementalListFileName,
                                  &createInfo.namesDictionary
                                 );
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("Cannot read incremental list file '%s' (error: %s)",
                   String_cString(incrementalListFileName),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
      printInfo(1,
                "OK (%lu entries)\n",
                Dictionary_count(&createInfo.namesDictionary)
               );
    }

    useIncrementalFileInfoFlag              = TRUE;
    createInfo.storeIncrementalFileInfoFlag =    (createInfo.archiveType == ARCHIVE_TYPE_FULL)
                                              || (createInfo.archiveType == ARCHIVE_TYPE_INCREMENTAL);
  }

  entityId = INDEX_ID_NONE;
  if (indexHandle != NULL)
  {
    // get/create index job UUID
    if (!Index_findUUID(indexHandle,
                        jobUUID,
                        NULL,  // scheduleUUID
                        &uuidId,
                        NULL,  // executionCountNormal,
                        NULL,  // executionCountFull,
                        NULL,  // executionCountIncremental,
                        NULL,  // executionCountDifferential,
                        NULL,  // executionCountContinuous,
                        NULL,  // averageDurationNormal,
                        NULL,  // averageDurationFull,
                        NULL,  // averageDurationIncremental,
                        NULL,  // averageDurationDifferential,
                        NULL,  // averageDurationContinuous,
                        NULL,  // totalEntityCount,
                        NULL,  // totalStorageCount,
                        NULL,  // totalStorageSize,
                        NULL,  // totalEntryCount,
                        NULL  // totalEntrySize
                       )
       )
    {
      error = Index_newUUID(indexHandle,jobUUID,&uuidId);
      if (error != ERROR_NONE)
      {
        printError("Cannot create index for '%s' (error: %s)!",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }

    // create new index entity
    error = Index_newEntity(indexHandle,
                            jobUUID,
                            scheduleUUID,
                            hostName,
                            userName,
                            archiveType,
                            createdDateTime,
                            TRUE,  // locked
                            &entityId
                           );
    if (error != ERROR_NONE)
    {
      printError("Cannot create index for '%s' (error: %s)!",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    assert(!INDEX_ID_IS_NONE(entityId));
    DEBUG_TESTCODE() { (void)deleteEntity(indexHandle,entityId); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
    AUTOFREE_ADD(&autoFreeList,&entityId,{ (void)deleteEntity(indexHandle,entityId); });

//TODO
    // purge expired entities

  }

  // create new archive
  error = Archive_create(&createInfo.archiveHandle,
                         hostName,
                         userName,
                         &createInfo.storageInfo,
                         NULL,  // archiveName
                         uuidId,
                         entityId,
                         jobUUID,
                         scheduleUUID,
                         &createInfo.jobOptions->deltaSourceList,
                         archiveType,
                         createdDateTime,
                         TRUE,  // createMeta
                         NULL,  // cryptPassword
                         storageFlags,
                         CALLBACK_(NULL,NULL),  // archiveInitFunction
                         CALLBACK_(NULL,NULL),  // archiveDoneFunction
                         CALLBACK_(archiveGetSize,&createInfo),
                         CALLBACK_(archiveStore,&createInfo),
                         CALLBACK_(getNamePasswordFunction,getNamePasswordUserData),
                         logHandle
                        );
  if (error != ERROR_NONE)
  {
    printError("Cannot create archive file '%s' (error: %s)",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Archive_close(&createInfo.archiveHandle); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&createInfo.archiveHandle,{ Archive_close(&createInfo.archiveHandle); });

  // start collectors and storage thread
  if (!Thread_init(&collectorSumThread,"BAR collector sum",globalOptions.niceLevel,collectorSumThreadCode,&createInfo))
  {
    HALT_FATAL_ERROR("Cannot initialize collector sum thread!");
  }
  if (!Thread_init(&collectorThread,"BAR collector",globalOptions.niceLevel,collectorThreadCode,&createInfo))
  {
    HALT_FATAL_ERROR("Cannot initialize collector thread!");
  }
  if (!Thread_init(&storageThread,"BAR storage",globalOptions.niceLevel,storageThreadCode,&createInfo))
  {
    HALT_FATAL_ERROR("Cannot initialize storage thread!");
  }
  AUTOFREE_ADD(&autoFreeList,&collectorSumThread,{ MsgQueue_setEndOfMsg(&createInfo.entryMsgQueue); Thread_join(&collectorSumThread); Thread_done(&collectorSumThread); });
  AUTOFREE_ADD(&autoFreeList,&collectorThread,{ MsgQueue_setEndOfMsg(&createInfo.entryMsgQueue); Thread_join(&collectorThread); Thread_done(&collectorThread); });
  AUTOFREE_ADD(&autoFreeList,&storageThread,{ MsgQueue_setEndOfMsg(&createInfo.storageMsgQueue); Thread_join(&storageThread); Thread_done(&storageThread); });

  // start create threads
  createThreadCount = (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores();
  createThreads     = (Thread*)malloc(createThreadCount*sizeof(Thread));
  if (createThreads == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,createThreads,{ free(createThreads); });
  for (i = 0; i < createThreadCount; i++)
  {
    if (!Thread_init(&createThreads[i],"BAR create",globalOptions.niceLevel,createThreadCode,&createInfo))
    {
      HALT_FATAL_ERROR("Cannot initialize create thread #%d!",i);
    }
  }

  // wait for collector threads
  AUTOFREE_REMOVE(&autoFreeList,&collectorSumThread);
  if (!Thread_join(&collectorSumThread))
  {
    HALT_INTERNAL_ERROR("Cannot stop collector sum thread!");
  }
  Thread_done(&collectorSumThread);
  AUTOFREE_REMOVE(&autoFreeList,&collectorThread);
  if (!Thread_join(&collectorThread))
  {
    HALT_INTERNAL_ERROR("Cannot stop collector thread!");
  }
  Thread_done(&collectorThread);

  // wait for and done create threads
  AUTOFREE_REMOVE(&autoFreeList,createThreads);
  MsgQueue_setEndOfMsg(&createInfo.entryMsgQueue);
  for (i = 0; i < createThreadCount; i++)
  {
    if (!Thread_join(&createThreads[i]))
    {
      HALT_FATAL_ERROR("Cannot stop create thread #%d!",i);
    }
    Thread_done(&createThreads[i]);
  }
  free(createThreads);

  // close archive
  AUTOFREE_REMOVE(&autoFreeList,&createInfo.archiveHandle);
  error = Archive_close(&createInfo.archiveHandle);
  if (error != ERROR_NONE)
  {
    printError("Cannot close archive '%s' (error: %s)",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  // wait for storage thread
  AUTOFREE_REMOVE(&autoFreeList,&storageThread);
  MsgQueue_setEndOfMsg(&createInfo.storageMsgQueue);
  if (!Thread_join(&storageThread))
  {
    HALT_INTERNAL_ERROR("Cannot stop storage thread!");
  }
  Thread_done(&storageThread);

  // final update of status info
  SEMAPHORE_LOCKED_DO(&createInfo.statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,2000)
  {
    (void)updateStatusInfo(&createInfo,TRUE);
  }

  // update index
  if (indexHandle != NULL)
  {
    assert(!INDEX_ID_IS_NONE(entityId));

    // update entity, uuid info (aggregated values)
    error = Index_updateEntityInfos(indexHandle,
                                    entityId
                                   );
    if (error != ERROR_NONE)
    {
      printError("Cannot create index for '%s' (error: %s)!",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    error = Index_updateUUIDInfos(indexHandle,
                                  uuidId
                                 );
    if (error != ERROR_NONE)
    {
      printError("Cannot create index for '%s' (error: %s)!",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // unlock entity
    (void)Index_unlockEntity(indexHandle,entityId);

    if (   (createInfo.failError == ERROR_NONE)
        && !createInfo.storageFlags.dryRun
        && !isAborted(&createInfo)
       )
    {
      // delete entity if nothing created
      error = Index_pruneEntity(indexHandle,entityId);
      if (error != ERROR_NONE)
      {
        printError("Cannot create index for '%s' (error: %s)!",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }
    else
    {
      // delete entity on error/abort
      (void)deleteEntity(indexHandle,entityId);
    }

    AUTOFREE_REMOVE(&autoFreeList,&entityId);
  }

  // write incremental list
  if (   (createInfo.failError == ERROR_NONE)
      && !createInfo.storageFlags.dryRun
      && !isAborted(&createInfo)
      && createInfo.storeIncrementalFileInfoFlag
     )
  {
    // get new increment list file name, delete old incremental list file
    if (String_isEmpty(jobOptions->incrementalListFileName))
    {
      fileName = String_new();

      // get new name
      if (jobUUID != NULL)
      {
        // get incremental file name from job UUID
        getIncrementalFileNameFromJobUUID(fileName,jobUUID);
      }
      else
      {
        // get incremental file name from storage name
        getIncrementalFileNameFromStorage(fileName,
                                          &createInfo.storageInfo.storageSpecifier
                                         );
      }

      // use new name if different
      if (!String_equals(incrementalListFileName,fileName))
      {
        File_delete(incrementalListFileName,FALSE);
        String_set(incrementalListFileName,fileName);
      }

      String_delete(fileName);
    }

    // write incremental list
    printInfo(1,"Write incremental list '%s'...",String_cString(incrementalListFileName));
    error = writeIncrementalList(&createInfo,
                                 incrementalListFileName,
                                 &createInfo.namesDictionary
                                );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot write incremental list file '%s' (error: %s)",
                 String_cString(incrementalListFileName),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
    printInfo(1,
              "OK (%lu entries)\n",
              Dictionary_count(&createInfo.namesDictionary)
             );
    logMessage(logHandle,LOG_TYPE_ALWAYS,"Updated incremental file '%s'",String_cString(incrementalListFileName));
  }

  // done storage
  AUTOFREE_REMOVE(&autoFreeList,&createInfo.storageInfo);
  Storage_done(&createInfo.storageInfo);

  // unmount devices
  error = unmountAll(&jobOptions->mountList);
  if (error != ERROR_NONE)
  {
    printWarning("Cannot unmount devices (error: %s)",
                 Error_getText(error)
                );
  }

  // output statics
  if (createInfo.failError == ERROR_NONE)
  {
    printInfo(1,
              "%lu entries/%.1lf%s (%llu bytes) included\n",
              createInfo.statusInfo.done.count,
              BYTES_SHORT(createInfo.statusInfo.done.size),
              BYTES_UNIT(createInfo.statusInfo.done.size),
              createInfo.statusInfo.done.size
             );
    printInfo(2,
              "%lu entries/%.1lf%s (%llu bytes) skipped\n",
              createInfo.statusInfo.skipped.count,
              BYTES_SHORT(createInfo.statusInfo.skipped.size),
              BYTES_UNIT(createInfo.statusInfo.skipped.size),
              createInfo.statusInfo.skipped.size
             );
    printInfo(2,
              "%lu entries/%.1lf%s (%llu bytes) with errors\n",
              createInfo.statusInfo.error.count,
              BYTES_SHORT(createInfo.statusInfo.error.size),
              BYTES_UNIT(createInfo.statusInfo.error.size),
              createInfo.statusInfo.error.size
             );
    logMessage(logHandle,
               LOG_TYPE_ALWAYS,
               "%lu entries/%.1lf%s (%llu bytes) included, %lu entries skipped, %lu entries with errors",
               createInfo.statusInfo.done.count,
               BYTES_SHORT(createInfo.statusInfo.done.size),
               BYTES_UNIT(createInfo.statusInfo.done.size),
               createInfo.statusInfo.done.size,
               createInfo.statusInfo.skipped.count,
               createInfo.statusInfo.error.count
              );
  }

  // get error code
  if (!isAborted(&createInfo))
  {
    error = createInfo.failError;
  }
  else
  {
    error = ERROR_ABORTED;
  }

  // free resources
  if (useIncrementalFileInfoFlag)
  {
    String_delete(incrementalListFileName);
    Dictionary_done(&createInfo.namesDictionary);
  }
  doneCreateInfo(&createInfo);
  Index_close(indexHandle);
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(userName);
  String_delete(hostName);
  String_delete(printableStorageName);
  AutoFree_done(&autoFreeList);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
