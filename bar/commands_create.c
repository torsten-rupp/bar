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

#include "common/arrays.h"
#include "common/autofree.h"
#include "common/database.h"
#include "common/devices.h"
#include "common/dictionaries.h"
#include "common/files.h"
#include "common/filesystems.h"
#include "common/fragmentlists.h"
#include "common/global.h"
#include "common/lists.h"
#include "common/misc.h"
#include "common/msgqueues.h"
#include "common/patternlists.h"
#include "common/patterns.h"
#include "common/semaphores.h"
#include "common/stringlists.h"
#include "common/stringmaps.h"
#include "common/strings.h"
#include "common/threads.h"

#include "bar.h"
#include "bar_common.h"
#include "errors.h"
#include "jobs.h"
#include "entrylists.h"
#include "archive.h"
#include "deltasources.h"
#include "crypt.h"
#include "storage.h"
#include "continuous.h"
#include "index/index_storages.h"

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
  const char                  *jobUUID;                             // job UUID to store or NULL
  const char                  *scheduleUUID;                        // schedule UUID or NULL
  const char                  *scheduleTitle;                       // schedule title or NULL
  const char                  *entityUUID;                          // entity UUID to store or NULL
  ArchiveTypes                archiveType;                          // archive type to create
  const char                  *customText;                          // custom text or NULL
  const EntryList             *includeEntryList;                    // list of included entries
  const PatternList           *excludePatternList;                  // list of exclude patterns
  JobOptions                  *jobOptions;
  uint64                      createdDateTime;                      // date/time of created [s]

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
//  ArchiveTypes archiveType;
  IndexId      storageId;
  String       intermediateFileName;                              // intermediate archive file name
  uint64       intermediateFileSize;                              // intermediate archive size [bytes]
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
  String_delete(storageMsg->intermediateFileName);
}

/***********************************************************************\
* Name   : initCreateInfo
* Purpose: initialize create info
* Input  : createInfo                 - create info variable
*          indexHandle                - index handle
*          jobUUID                    - job UUID to store or NULL
*          scheduleUUID               - schedule UUID to store or NULL
*          scheduleTitle              - schedule title or NULL
*          entityUUID                 - entity UUID to store or NULL
*          archiveType                - archive type; see ArchiveTypes
*                                       (normal/full/incremental)
*          includeEntryList           - include entry list
*          excludePatternList         - exclude pattern list
*          customText                 - custome text or NULL
*          jobOptions                 - job options
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
                          const char         *jobUUID,
                          const char         *scheduleUUID,
                          const char         *scheduleTitle,
                          const char         *entityUUID,
                          ArchiveTypes       archiveType,
                          const EntryList    *includeEntryList,
                          const PatternList  *excludePatternList,
                          const char         *customText,
                          JobOptions         *jobOptions,
                          uint64             createdDateTime,
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
// TODO: needed?
  createInfo->scheduleTitle                        = scheduleTitle;
  createInfo->entityUUID                           = entityUUID;
  createInfo->includeEntryList                     = includeEntryList;
  createInfo->excludePatternList                   = excludePatternList;
  createInfo->customText                           = customText;
  createInfo->jobOptions                           = jobOptions;
  createInfo->createdDateTime                      = createdDateTime;

  createInfo->logHandle                            = logHandle;

  Dictionary_init(&createInfo->namesDictionary,DICTIONARY_BYTE_COPY,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

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
  if (!MsgQueue_init(&createInfo->entryMsgQueue,
                     MAX_ENTRY_MSG_QUEUE,
                     CALLBACK_((MsgQueueMsgFreeFunction)freeEntryMsg,NULL)
                    )
     )
  {
    HALT_FATAL_ERROR("Cannot initialize entry message queue!");
  }
  if (!MsgQueue_init(&createInfo->storageMsgQueue,0,CALLBACK_((MsgQueueMsgFreeFunction)freeStorageMsg,NULL)))
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

  MsgQueue_done(&createInfo->storageMsgQueue);
  MsgQueue_done(&createInfo->entryMsgQueue);

  doneStatusInfo(&createInfo->statusInfo);
  FragmentList_done(&createInfo->statusInfoFragmentList);
  StringList_done(&createInfo->storageFileList);

  Dictionary_done(&createInfo->namesDictionary);
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
      error = File_makeDirectory(directoryName,
                                 FILE_DEFAULT_USER_ID,
                                 FILE_DEFAULT_GROUP_ID,
                                 FILE_DEFAULT_PERMISSIONS,
                                 FALSE
                                );
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

LOCAL String getIncrementalFileNameFromJobUUID(String fileName, const char *jobUUID)
{
  assert(fileName != NULL);
  assert(jobUUID != NULL);

  File_setFileName(fileName,globalOptions.incrementalDataDirectory);
  File_appendFileNameCString(fileName,jobUUID);
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

  Dictionary duplicateNamesDictionary;
  String     name;
  Dictionary hardLinksDictionary;
  Errors     error;

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

  if (createInfo->archiveType == ARCHIVE_TYPE_CONTINUOUS)
  {
    DatabaseHandle          continuousDatabaseHandle;
    DatabaseStatementHandle databaseStatementHandle;
    DatabaseId              databaseId;

    // process entries from continuous database
    if (Continuous_isAvailable())
    {
      // open continuous database
      error = Continuous_open(&continuousDatabaseHandle);
      if (error == ERROR_NONE)
      {
        error = Continuous_initList(&databaseStatementHandle,
                                    &continuousDatabaseHandle,
                                    createInfo->jobUUID,
                                    createInfo->scheduleUUID
                                   );
        if (error == ERROR_NONE)
        {
          while (Continuous_getNext(&databaseStatementHandle,&databaseId,name))
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
          Continuous_doneList(&databaseStatementHandle);
        }

        // close continuous database
        Continuous_close(&continuousDatabaseHandle);
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
          File_setFileNameChar(basePath,FILE_PATH_SEPARATOR_CHAR);
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

  if (createInfo->archiveType == ARCHIVE_TYPE_CONTINUOUS)
  {
    DatabaseHandle continuousDatabaseHandle;

    if (Continuous_isAvailable())
    {
      // open continuous database
      error = Continuous_open(&continuousDatabaseHandle);
      if (error != ERROR_NONE)
      {
        printError("cannot initialise continuous database (error: %s)!",
                   Error_getText(error)
                  );
        AutoFree_freeAll(&autoFreeList);
        return;
      }
      AUTOFREE_ADD(&autoFreeList,&continuousDatabaseHandle,{ Continuous_close(&continuousDatabaseHandle); });

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
            printWarning("Cannot get info for '%s' (error: %s) - skipped",String_cString(name),Error_getText(error));
            logMessage(createInfo->logHandle,
                       LOG_TYPE_ENTRY_ACCESS_DENIED,
                       "Access denied '%s' (error: %s)",
                       String_cString(name),
                       Error_getText(error)
                      );
          }
          else
          {
            printError("cannot get info for '%s' (error: %s)",
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
                                          !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
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
                                                  !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
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

      // close continuous database
      Continuous_close(&continuousDatabaseHandle);
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
          File_getSystemDirectory(basePath,FILE_SYSTEM_PATH_ROOT,NULL);
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
            printWarning("Cannot get info for '%s' (error: %s) - skipped",String_cString(name),Error_getText(error));
            logMessage(createInfo->logHandle,
                       LOG_TYPE_ENTRY_ACCESS_DENIED,
                       "Access denied '%s' (error: %s)",
                       String_cString(name),
                       Error_getText(error)
                      );
          }
          else
          {
            printError("cannot get info for '%s' (error: %s)",
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
                                                !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
                                               );
                        }
                        break;
                      case ENTRY_TYPE_IMAGE:
                        printWarning("'%s' is not a device",String_cString(name));
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
                        printWarning("'%s' is not a device",String_cString(name));
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
                      printError("Cannot read directory '%s' (error: %s) - skipped",String_cString(name),Error_getText(error));
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
                      printError("Cannot access '%s' (error: %s) - skipped",String_cString(fileName),Error_getText(error));
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
                                                            !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
                                                           );
                                    }
                                    break;
                                  case ENTRY_TYPE_IMAGE:
                                    printWarning("'%s' is not a device",String_cString(fileName));
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
                                          if (createInfo->jobOptions->skipUnreadableFlag)
                                          {
                                            printWarning("Cannot get info for '%s' (error: %s) - skipped",String_cString(fileName),Error_getText(error));
                                            logMessage(createInfo->logHandle,
                                                       LOG_TYPE_ENTRY_ACCESS_DENIED,
                                                       "Access denied '%s' (error: %s)",
                                                       String_cString(fileName),
                                                       Error_getText(error)
                                                      );
                                          }
                                          else
                                          {
                                            printError("cannot get info for '%s' (error: %s)",
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

                                        // add to entry list
                                        appendImageToEntryList(&createInfo->entryMsgQueue,
                                                               ENTRY_TYPE_IMAGE,
                                                               name,
                                                               &deviceInfo,
                                                               !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
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
                                                                      !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
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
                                        printError("Cannot access '%s' (error: %s) - skipped",String_cString(name),Error_getText(error));
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
                                                             !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
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
                                                   !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
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
                                                        !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
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
                              if (createInfo->jobOptions->skipUnreadableFlag)
                              {
                                printWarning("Cannot get info for '%s' (error: %s) - skipped",String_cString(name),Error_getText(error));
                                logMessage(createInfo->logHandle,
                                           LOG_TYPE_ENTRY_ACCESS_DENIED,
                                           "Access denied '%s' (error: %s)",
                                           String_cString(name),
                                           Error_getText(error)
                                          );
                              }
                              else
                              {
                                printError("cannot get info for '%s' (error: %s)",
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

                            // add to entry list
                            appendImageToEntryList(&createInfo->entryMsgQueue,
                                                   ENTRY_TYPE_IMAGE,
                                                   name,
                                                   &deviceInfo,
                                                   !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
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
        if (!createInfo->jobOptions->skipUnreadableFlag)
        {
          printError("no matching entry found for '%s'!",
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
                              !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
                             );
  }
  Dictionary_doneIterator(&dictionaryIterator);

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

  if (globalOptions.maxTmpSize > 0)
  {
    SEMAPHORE_LOCKED_DO(&createInfo->storageInfoLock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
    {
      if (   (createInfo->storage.count > 2)                           // more than 2 archives are waiting
          && (createInfo->storage.bytes > globalOptions.maxTmpSize)    // temporary space limit exceeded
          && (createInfo->failError == ERROR_NONE)
          && !isAborted(createInfo)
         )
      {
        STATUS_INFO_UPDATE(createInfo,NULL,NULL)
        {
          String_setCString(createInfo->statusInfo.message,"wait for temporary space");
        }

        do
        {
          Semaphore_waitModified(&createInfo->storageInfoLock,30*MS_PER_SECOND);
        }
        while (   (createInfo->storage.count > 2)
               && (createInfo->storage.bytes > globalOptions.maxTmpSize)
               && (createInfo->failError == ERROR_NONE)
               && !isAborted(createInfo)
              );

        STATUS_INFO_UPDATE(createInfo,NULL,NULL)
        {
          String_clear(createInfo->statusInfo.message);
        }
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

  // get archive file name (expand macros)
  archiveName = String_new();
  error = Archive_formatName(archiveName,
                             storageInfo->storageSpecifier.archiveName,
                             EXPAND_MACRO_MODE_STRING,
                             createInfo->archiveType,
                             createInfo->scheduleTitle,
                             createInfo->customText,
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
* Name   : simpleTestArchive
* Purpose: simple test archive
* Input  : storageInfo - storage info
*          archiveName - archive name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors simpleTestArchive(StorageInfo *storageInfo,
                               ConstString  archiveName
                              )
{
  Errors            error;
  ArchiveHandle     archiveHandle;
  ArchiveEntryTypes archiveEntryType;
  ArchiveEntryInfo  archiveEntryInfo;

  // open archive
  error = Archive_open(&archiveHandle,
                       storageInfo,
                       archiveName,
                       NULL,  // deltaSourceList,
                       ARCHIVE_FLAG_NONE,
                       CALLBACK_(NULL,NULL),  // getNamePasswordFunction
                       NULL // logHandle
                      );
  if (error != ERROR_NONE)
  {
// TODO: race condition in scp/sftp without a short delay?
Misc_udelay(1000*1000);
    error = Archive_open(&archiveHandle,
                         storageInfo,
                         archiveName,
                         NULL,  // deltaSourceList,
                         ARCHIVE_FLAG_NONE,
                         CALLBACK_(NULL,NULL),  // getNamePasswordFunction
                         NULL // logHandle
                        );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // simple test: read and skip content of archive entries
  error = ERROR_NONE;
  while (   (error == ERROR_NONE)
         && !Archive_eof(&archiveHandle)
        )
  {
    // get next archive entry type
    error = Archive_getNextArchiveEntry(&archiveHandle,
                                        &archiveEntryType,
                                        NULL,  // archiveCryptInfo,
                                        NULL,  // offset,
                                        NULL  // size
                                       );
    if (error == ERROR_NONE)
    {
//fprintf(stderr,"%s:%d: archiveEntryType=%d\n",__FILE__,__LINE__,archiveEntryType);
      switch (archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_NONE:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNREACHABLE();
          #endif /* NDEBUG */
          break; /* not reached */
        case ARCHIVE_ENTRY_TYPE_FILE:
          error = Archive_readFileEntry(&archiveEntryInfo,
                                        &archiveHandle,
                                        NULL,  // deltaCompressAlgorithm
                                        NULL,  // byteCompressAlgorithm
                                        NULL,  // cryptType
                                        NULL,  // cryptAlgorithm
                                        NULL,  // cryptSalt
                                        NULL,  // cryptKey
                                        NULL,  // fileName,
                                        NULL,  // fileInfo,
                                        NULL,  // fileExtendedAttributeList
                                        NULL,  // deltaSourceName
                                        NULL,  // deltaSourceSize
                                        NULL,  // fragmentOffset,
                                        NULL  // fragmentSize
                                       );
          if (error == ERROR_NONE)
          {
            error = Archive_closeEntry(&archiveEntryInfo);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          error = Archive_readImageEntry(&archiveEntryInfo,
                                         &archiveHandle,
                                         NULL,  // deltaCompressAlgorithm
                                         NULL,  // byteCompressAlgorithm
                                         NULL,  // cryptType
                                         NULL,  // cryptAlgorithm
                                         NULL,  // cryptSalt
                                         NULL,  // cryptKey
                                         NULL,  // deviceName,
                                         NULL,  // deviceInfo,
                                         NULL,  // fileSystemType
                                         NULL,  // deltaSourceName
                                         NULL,  // deltaSourceSize
                                         NULL,  // blockOffset,
                                         NULL  // blockCount
                                        );
          if (error == ERROR_NONE)
          {
            error = Archive_closeEntry(&archiveEntryInfo);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          error = Archive_readDirectoryEntry(&archiveEntryInfo,
                                             &archiveHandle,
                                             NULL,  // cryptType
                                             NULL,  // cryptAlgorithm
                                             NULL,  // cryptSalt
                                             NULL,  // cryptKey
                                             NULL,  // directoryName,
                                             NULL,  // fileInfo,
                                             NULL   // fileExtendedAttributeList
                                            );
          if (error == ERROR_NONE)
          {
            error = Archive_closeEntry(&archiveEntryInfo);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          error = Archive_readLinkEntry(&archiveEntryInfo,
                                        &archiveHandle,
                                        NULL,  // cryptType
                                        NULL,  // cryptAlgorithm
                                        NULL,  // cryptSalt
                                        NULL,  // cryptKey
                                        NULL,  // linkName,
                                        NULL,  // fileName,
                                        NULL,  // fileInfo,
                                        NULL   // fileExtendedAttributeList
                                       );
          if (error == ERROR_NONE)
          {
            error = Archive_closeEntry(&archiveEntryInfo);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                            &archiveHandle,
                                            NULL,  // deltaCompressAlgorithm
                                            NULL,  // byteCompressAlgorithm
                                            NULL,  // cryptType
                                            NULL,  // cryptAlgorithm
                                            NULL,  // cryptSalt
                                            NULL,  // cryptKey
                                            NULL,  // fileNameList,
                                            NULL,  // fileInfo,
                                            NULL,  // fileExtendedAttributeList
                                            NULL,  // deltaSourceName
                                            NULL,  // deltaSourceSize
                                            NULL,  // fragmentOffset,
                                            NULL  // fragmentSize
                                           );
          if (error == ERROR_NONE)
          {
            error = Archive_closeEntry(&archiveEntryInfo);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          error = Archive_readSpecialEntry(&archiveEntryInfo,
                                           &archiveHandle,
                                           NULL,  // cryptType
                                           NULL,  // cryptAlgorithm
                                           NULL,  // cryptSalt
                                           NULL,  // cryptKey
                                           NULL,  // fileName,
                                           NULL,  // fileInfo,
                                           NULL   // fileExtendedAttributeList
                                          );
          if (error == ERROR_NONE)
          {
            error = Archive_closeEntry(&archiveEntryInfo);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_META:
          error = Archive_skipNextEntry(&archiveHandle);
          break;
        case ARCHIVE_ENTRY_TYPE_SALT:
        case ARCHIVE_ENTRY_TYPE_KEY:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNREACHABLE();
          #else
            error = Archive_skipNextEntry(&archiveHandle);
          #endif /* NDEBUG */
          break;
        case ARCHIVE_ENTRY_TYPE_SIGNATURE:
          error = Archive_skipNextEntry(&archiveHandle);
          break;
        case ARCHIVE_ENTRY_TYPE_UNKNOWN:
          error = ERROR_UNKNOWN_CHUNK;
          break; /* not reached */
      }
    }
  }

  // close archive
  (void)Archive_close(&archiveHandle,FALSE);

  return error;
}

/***********************************************************************\
* Name   : archiveStore
* Purpose: call back to store archive file
* Input  : storageInfo          - storage info
*          uuidId               - index UUID id
*          jobUUID              - job UUID
*          entityUUID           - entity UUID
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
                          ConstString  entityUUID,
                          IndexId      entityId,
                          IndexId      storageId,
                          int          partNumber,
                          ConstString  intermediateFileName,
                          uint64       intermediateFileSize,
                          void         *userData
                         )
{
  CreateInfo *createInfo = (CreateInfo*)userData;
  Errors     error;
  String     archiveName;
  StorageMsg storageMsg;

  assert(storageInfo != NULL);
  assert(!String_isEmpty(intermediateFileName));
  assert(createInfo != NULL);

  UNUSED_VARIABLE(jobUUID);
  UNUSED_VARIABLE(entityUUID);

  // get archive file name (expand macros)
  archiveName = String_new();
  error = Archive_formatName(archiveName,
                             storageInfo->storageSpecifier.archiveName,
                             EXPAND_MACRO_MODE_STRING,
                             createInfo->archiveType,
                             createInfo->scheduleTitle,
                             createInfo->customText,
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
  storageMsg.uuidId               = uuidId;
  storageMsg.entityId             = entityId;
  storageMsg.storageId            = storageId;
  storageMsg.intermediateFileName = String_duplicate(intermediateFileName);
  storageMsg.intermediateFileSize = intermediateFileSize;
  storageMsg.archiveName          = archiveName;
  storageInfoIncrement(createInfo,intermediateFileSize);
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
    createInfo->statusInfo.storage.totalSize += intermediateFileSize;
  }

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
                                 const char  *jobUUID,
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
                                     NULL,  // entityUUID
                                     NULL,  // indexIds
                                     0,   // indexIdCount
                                     INDEX_TYPESET_ALL,
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
                   jobUUID,
                   Error_getText(error)
                  );
        break;
      }
      while (Index_getNextStorage(&indexQueryHandle,
                                  &uuidId,
                                  NULL,  // jobUUID
                                  &entityId,
                                  NULL,  // entityUUID
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
//fprintf(stderr,"%s, %d: %"PRIu64" %s: createdDateTime=%"PRIu64" size=%"PRIu64"\n",__FILE__,__LINE__,storageId,String_cString(storageName),createdDateTime,size);
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

//fprintf(stderr,"%s, %d: totalStorageSize=%"PRIu64" limit=%"PRIu64" oldestStorageId=%"PRIu64"\n",__FILE__,__LINE__,totalStorageSize,limit,oldestStorageId);
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
                       "Purging storage '%s', %.1f%s (%"PRIu64" bytes) fail (error: %s)",
                       String_cString(oldestStorageName),
                       BYTES_SHORT(oldestSize),
                       BYTES_UNIT(oldestSize),
                       oldestSize,
                       Error_getText(error)
                      );
          }
          Storage_done(&storageInfo);
        }

        // purge index of storage
        error = IndexStorage_purge(indexHandle,
                                   oldestStorageId,
                                   NULL  // progressInfo
                                  );
        if (error != ERROR_NONE)
        {
          logMessage(logHandle,
                     LOG_TYPE_STORAGE,
                     "Purging storage index #%"PRIu64" fail (error: %s)",
                     oldestStorageId,
                     Error_getText(error)
                    );
          break;
        }
        (void)Index_pruneEntity(indexHandle,oldestEntityId);
        (void)Index_pruneUUID(indexHandle,oldestUUIDId);

        // log
        Misc_formatDateTime(String_clear(dateTime),oldestCreatedDateTime,FALSE,NULL);
        logMessage(logHandle,
                   LOG_TYPE_STORAGE,
                   "Job size limit exceeded (max %.1f%s): purged storage '%s', created at %s, %"PRIu64" bytes",
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
                                     NULL,  // jobUUID
                                     NULL,  // entityUUID
                                     NULL,  // indexIds
                                     0,   // indexIdCount
                                     INDEX_TYPESET_ALL,
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
                                  NULL,  // entityUUID
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
//fprintf(stderr,"%s, %d: %"PRIu64" %s: %"PRIu64"\n",__FILE__,__LINE__,storageId,String_cString(storageName),createdDateTime);
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
                       "Purging storage '%s', %.1f%s (%"PRIu64" bytes) fail (error: %s)",
                       String_cString(oldestStorageName),
                       BYTES_SHORT(oldestSize),
                       BYTES_UNIT(oldestSize),
                       oldestSize,
                       Error_getText(error)
                      );
          }
          Storage_done(&storageInfo);
        }

        // purge index of storage
        error = IndexStorage_purge(indexHandle,
                                   oldestStorageId,
                                   NULL  // progressInfo
                                  );
        if (error != ERROR_NONE)
        {
          logMessage(logHandle,
                     LOG_TYPE_STORAGE,
                     "Purging storage index #%"PRIu64" fail (error: %s)",
                     oldestStorageId,
                     Error_getText(error)
                    );
          break;
        }
        (void)Index_pruneEntity(indexHandle,oldestEntityId);
        (void)Index_pruneUUID(indexHandle,oldestUUIDId);

        // log
        Misc_formatDateTime(dateTime,oldestCreatedDateTime,FALSE,NULL);
        logMessage(logHandle,
                   LOG_TYPE_STORAGE,
                   "Server size limit exceeded (max %.1f%s): purged storage '%s', created at %s, %"PRIu64" bytes",
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
  String           directoryPath;
  FileInfo         fileInfo;
  Server           server;
  FileHandle       fileHandle;
  uint             retryCount;
  uint64           storageSize;
  bool             appendFlag;
  StorageHandle    storageHandle;
  String           pattern;

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
  directoryPath         = String_new();
  existingStorageName   = String_new();
  existingDirectoryName = String_new();
  Storage_initSpecifier(&existingStorageSpecifier);
  AUTOFREE_ADD(&autoFreeList,buffer,{ free(buffer); });
  AUTOFREE_ADD(&autoFreeList,printableStorageName,{ String_delete(printableStorageName); });
  AUTOFREE_ADD(&autoFreeList,directoryPath,{ String_delete(directoryPath); });
  AUTOFREE_ADD(&autoFreeList,existingStorageName,{ String_delete(existingStorageName); });
  AUTOFREE_ADD(&autoFreeList,existingDirectoryName,{ String_delete(existingDirectoryName); });
  AUTOFREE_ADD(&autoFreeList,&existingStorageSpecifier,{ Storage_doneSpecifier(&existingStorageSpecifier); });

  // initial storage pre-processing
  if (   (createInfo->failError == ERROR_NONE)
      && !createInfo->jobOptions->dryRun
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
        printError("cannot pre-process storage (error: %s)!",
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
                   storageInfoDecrement(createInfo,storageMsg.intermediateFileSize);
                   File_delete(storageMsg.intermediateFileName,FALSE);
                   freeStorageMsg(&storageMsg,NULL);
                 }
                );
    if ((createInfo->failError != ERROR_NONE) || isAborted(createInfo))
    {
      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      break;
    }

    if (!createInfo->jobOptions->dryRun)
    {
      // get file info
      error = File_getInfo(&fileInfo,storageMsg.intermediateFileName);
      if (error != ERROR_NONE)
      {
        if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

        printError("cannot get info for file '%s' (error: %s)",
                   String_cString(storageMsg.intermediateFileName),
                   Error_getText(error)
                  );

        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        break;
      }
      DEBUG_TESTCODE() { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); break; }

      // check if exist, auto-rename if requested
      if (   (createInfo->storageInfo.jobOptions != NULL)
          && (createInfo->storageInfo.jobOptions->archiveFileMode == ARCHIVE_FILE_MODE_RENAME)
          && Storage_exists(&createInfo->storageInfo,storageMsg.archiveName)
         )
      {
        String directoryPath,baseName;
        String prefixFileName,postfixFileName;
        long   index;
        uint   n;

        // rename new archive
        directoryPath   = String_new();
        baseName        = String_new();
        prefixFileName  = String_new();
        postfixFileName = String_new();
        File_splitFileName(storageMsg.archiveName,directoryPath,baseName);
        index = String_findLastChar(baseName,STRING_END,'.');
        if (index >= 0)
        {
          String_sub(prefixFileName,baseName,STRING_BEGIN,index);
          String_sub(postfixFileName,baseName,index,STRING_END);
        }
        else
        {
          String_set(prefixFileName,baseName);
        }
        n = 0;
        do
        {
          String_set(storageMsg.archiveName,directoryPath);
          File_appendFileName(storageMsg.archiveName,prefixFileName);
          String_appendFormat(storageMsg.archiveName,"-%u",n);
          String_append(storageMsg.archiveName,postfixFileName);
          n++;
        }
        while (Storage_exists(&createInfo->storageInfo,storageMsg.archiveName));
        String_delete(baseName);
        String_delete(directoryPath);
        String_delete(postfixFileName);
        String_delete(prefixFileName);
      }

      // get printable storage name
      Storage_getPrintableName(printableStorageName,&createInfo->storageInfo.storageSpecifier,storageMsg.archiveName);

      // pre-process
      error = Storage_preProcess(&createInfo->storageInfo,
                                 storageMsg.
                                 archiveName,
                                 createInfo->createdDateTime,
                                 FALSE  // initialFlag
                                );
      if (error != ERROR_NONE)
      {
        if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

        printError("cannot pre-process file '%s' (error: %s)!",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );

        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        break;
      }
      DEBUG_TESTCODE() { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); break; }

      // check storage size, purge old archives
      if (!createInfo->jobOptions->dryRun && (createInfo->jobOptions->maxStorageSize > 0LL))
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
        printInfo(1,"Store '%s' to '%s'...",String_cString(storageMsg.intermediateFileName),String_cString(printableStorageName));
      #else /* not NDEBUG */
        printInfo(1,"Store '%s'...",String_cString(printableStorageName));
      #endif /* NDEBUG */
      error = File_open(&fileHandle,storageMsg.intermediateFileName,FILE_OPEN_READ);
      if (error != ERROR_NONE)
      {
        if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

        printInfo(0,"FAIL!\n");
        printError("cannot open file '%s' (error: %s)!",
                   String_cString(storageMsg.intermediateFileName),
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
            // create fail -> abort
            break;
          }
        }
        DEBUG_TESTCODE() { Storage_close(&storageHandle); error = DEBUG_TESTCODE_ERROR(); break; }
        AUTOFREE_ADD(&autoFreeList,&storageMsg,
                     {
                       if (!appendFlag && !INDEX_ID_IS_NONE(storageMsg.storageId))
                       {
                         IndexStorage_purge(createInfo->indexHandle,
                                            storageMsg.storageId,
                                            NULL  // progressInfo
                                           );
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

        // transfer file data to storage
        error = Storage_transferFromFile(&fileHandle,
                                         &storageHandle,
                                         CALLBACK_(NULL,NULL),  // storageTransferInfo
                                         CALLBACK_(NULL,NULL)  // isAborted
                                        );
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
        printError("cannot store '%s' (error: %s)!",
                   String_cString(printableStorageName),
                   Error_getText(createInfo->failError)
                  );
        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        break;
      }
      printInfo(1,"OK (%"PRIu64" bytes)\n",storageSize);

      if (createInfo->jobOptions->testCreatedArchivesFlag)
      {
        printInfo(1,"Test '%s'...",String_cString(printableStorageName));
        error = simpleTestArchive(&createInfo->storageInfo,storageMsg.archiveName);
        if (error != ERROR_NONE)
        {
          if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

          printInfo(0,"FAIL!\n");
          printError("cannot test '%s' (error: %s)!",
                     String_cString(printableStorageName),
                     Error_getText(createInfo->failError)
                    );
          AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
          break;
        }
        printInfo(1,"OK (%"PRIu64" bytes)\n",fileInfo.size);
      }

      // update index database and set state
      if (   (createInfo->indexHandle != NULL)
          && !INDEX_ID_IS_NONE(storageMsg.storageId)
         )
      {
        assert(!INDEX_ID_IS_NONE(storageMsg.entityId));

        // check if append and storage exists => assign to existing storage index
        if (   appendFlag
            && (Index_findStorageByName(createInfo->indexHandle,
                                        &createInfo->storageInfo.storageSpecifier,
                                        storageMsg.archiveName,
                                        NULL,  // uuidId
                                        NULL,  // entityId
                                        NULL,  // jobUUID
                                        NULL,  // entityUUID
                                        &storageId,
                                        NULL,  // createdDateTime
                                        NULL,  // size
                                        NULL,  // indexMode
                                        NULL,  // indexState
                                        NULL,  // lastCheckedDateTime
                                        NULL,  // errorMessage
                                        NULL,  // totalEntryCount
                                        NULL  // totalEntrySize
                                       ) == ERROR_NONE
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
            printError("cannot update index for storage '%s' (error: %s)!",
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
//fprintf(stderr,"%s, %d: append to storage %"PRIu64"\n",__FILE__,__LINE__,storageId);
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
            printError("cannot update index for storage '%s' (error: %s)!",
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
          // replace index: keep new storage
          storageId = storageMsg.storageId;
          AUTOFREE_ADD(&autoFreeList,&storageMsg.storageId,
          {
            // nothing to do
          });

          // delete old indizes for same storage file
          error = Index_purgeAllStoragesByName(createInfo->indexHandle,
                                               &createInfo->storageInfo.storageSpecifier,
                                               storageMsg.archiveName,
                                               storageMsg.storageId
                                              );
          if (error != ERROR_NONE)
          {
            if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

            printInfo(1,"FAIL\n");
            printError("cannot delete old index for storage '%s' (error: %s)!",
                       String_cString(printableStorageName),
                       Error_getText(error)
                      );

            AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
            break;
          }

          // append storage to existing entity which has the same storage directory
          if (createInfo->storageInfo.jobOptions->archiveFileMode == ARCHIVE_FILE_MODE_APPEND)
          {
            Storage_getPrintableName(printableStorageName,&createInfo->storageInfo.storageSpecifier,storageMsg.archiveName);

            // find matching entity and assign storage to entity
            File_getDirectoryName(directoryPath,storageMsg.archiveName);
            error = Index_initListStorages(&indexQueryHandle,
                                           createInfo->indexHandle,
                                           storageMsg.uuidId,
                                           INDEX_ID_ANY, // entityId
                                           NULL,  // jobUUID
                                           NULL,  // entityUUID
                                           NULL,  // indexIds
                                           0,  // indexIdCount
                                           INDEX_TYPESET_ALL,
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
                                        NULL,  // jobUUID
                                        &existingEntityId,
                                        NULL,  // entityUUID
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
                  && Storage_equalSpecifiers(&existingStorageSpecifier,directoryPath,&existingStorageSpecifier,existingDirectoryName)
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
              printError("cannot delete old index for storage '%s' (error: %s)!",
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
          printError("cannot update index for storage '%s' (error: %s)!",
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
      logMessage(createInfo->logHandle,
                 LOG_TYPE_STORAGE,
                 "%s '%s' (%"PRIu64" bytes)",
                 appendFlag ? "Appended to" : "Stored",
                 String_cString(printableStorageName),
                 storageSize
                );

      // post-process
      error = Storage_postProcess(&createInfo->storageInfo,storageMsg.archiveName,createInfo->createdDateTime,FALSE);
      if (error != ERROR_NONE)
      {
        printError("cannot post-process storage file '%s' (error: %s)!",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
        if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        break;
      }
      DEBUG_TESTCODE() { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); continue; }

      // delete temporary storage file
      error = File_delete(storageMsg.intermediateFileName,FALSE);
      if (error != ERROR_NONE)
      {
        printWarning("cannot delete file '%s' (error: %s)!",
                     String_cString(storageMsg.intermediateFileName),
                     Error_getText(error)
                    );
      }

      // add to list of stored archive files
      StringList_append(&createInfo->storageFileList,storageMsg.archiveName);

      // update storage info
      storageInfoDecrement(createInfo,storageMsg.intermediateFileSize);
    }

    // free resources
    freeStorageMsg(&storageMsg,NULL);

    AutoFree_restore(&autoFreeList,autoFreeSavePoint,FALSE);
  }

  // discard unprocessed archives
  while (MsgQueue_get(&createInfo->storageMsgQueue,&storageMsg,NULL,sizeof(storageMsg),NO_WAIT))
  {
    // discard index
    if (!INDEX_ID_IS_NONE(storageMsg.storageId))
    {
      (void)IndexStorage_purge(createInfo->indexHandle,
                               storageMsg.storageId,
                               NULL  // progressInfo
                              );
    }

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

  // final storage post-processing
  if (   (createInfo->failError == ERROR_NONE)
      && !createInfo->jobOptions->dryRun
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
        printError("cannot post-process storage (error: %s)!",
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
      // get archive name pattern (expand macros)
      pattern = String_new();
      error = Archive_formatName(pattern,
                                 createInfo->storageInfo.storageSpecifier.archiveName,
                                 EXPAND_MACRO_MODE_PATTERN,
                                 createInfo->archiveType,
                                 createInfo->scheduleTitle,
                                 createInfo->customText,
                                 createInfo->createdDateTime,
                                 ARCHIVE_PART_NUMBER_NONE
                                );
      if (error == ERROR_NONE)
      {
        // delete all matching storage files which are unknown
        (void)Storage_forAll(&createInfo->storageInfo.storageSpecifier,
                             NULL,  // directory
                             String_cString(pattern),
                             TRUE,  // skipUnreadableFlag
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
  String_delete(directoryPath);
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
* Name   : getArchiveEntryName
* Purpose: transform archive entry name
* Input  : archiveEntryName - archive entry name variable
*          name             - name to transform
* Output : -
* Return : archive entry name
* Notes  : -
\***********************************************************************/

LOCAL String getArchiveEntryName(String archiveEntryName, ConstString name)
{
  Pattern pattern;
  ulong   index;
  ulong   matchIndex,matchLength;

  assert(archiveEntryName != NULL);
  assert(name != NULL);

  if (!String_isEmpty(globalOptions.transform.patternString))
  {
    // transform name with matching pattern
    String_clear(archiveEntryName);
    Pattern_init(&pattern,globalOptions.transform.patternString,globalOptions.transform.patternType,PATTERN_FLAG_NONE);
    index = STRING_BEGIN;
    while (Pattern_match(&pattern,name,index,PATTERN_MATCH_MODE_ANY,&matchIndex,&matchLength))
    {
      String_appendBuffer(archiveEntryName,String_cString(name)+index,matchIndex-index);
      String_append(archiveEntryName,globalOptions.transform.replace);
      index = matchIndex+matchLength;
    }
    Pattern_done(&pattern);
    String_appendCString(archiveEntryName,String_cString(name)+index);
  }
  else
  {
    // keep name
    String_set(archiveEntryName,name);
  }

  return archiveEntryName;
}

/***********************************************************************\
* Name   : getArchiveEntryNameList
* Purpose: transform archive entry name list
* Input  : archiveEntryNameList - archive entry name list variable
*          nameList             - name list to transform
* Output : archiveEntryNameList - archive entry name list
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void getArchiveEntryNameList(StringList *archiveEntryNameList, const StringList *nameList)
{
  String     archiveEntryName;
  StringNode *iterator;
  String     name;

  assert(archiveEntryNameList != NULL);

  archiveEntryName = String_new();
  STRINGLIST_ITERATE(nameList,iterator,name)
  {
    StringList_append(archiveEntryNameList,getArchiveEntryName(archiveEntryName,name));
  }
  String_delete(archiveEntryName);
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
  String                    archiveEntryName;
  ArchiveEntryInfo          archiveEntryInfo;
  uint64                    offset;
  uint64                    size;
  ulong                     bufferLength;
  FragmentNode              *fragmentNode;
  uint64                    archiveSize;
  uint                      percentageDone;
  double                    compressionRatio;
  double                    d;
  char                      fragmentInfoString[256],compressionRatioString[256];

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
        createInfo->statusInfo.skipped.count++;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("cannot get extended attributes for '%s' (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
      {
        createInfo->statusInfo.error.count++;
      }

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
                 "Cannot open file '%s' - skipped (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
      {
        createInfo->statusInfo.skipped.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->statusInfo.skipped.size  += fragmentSize;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("cannot open file '%s' (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
      {
        createInfo->statusInfo.error.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->statusInfo.error.size  += (uint64)fileInfo->size;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  // init fragment
  fragmentInit(createInfo,fileName,fileInfo->size,fragmentCount);

  if (!createInfo->jobOptions->noStorage)
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
    archiveEntryName = getArchiveEntryName(String_new(),fileName);
    error = Archive_newFileEntry(&archiveEntryInfo,
                                 &createInfo->archiveHandle,
                                 createInfo->jobOptions->compressAlgorithms.delta,
                                 createInfo->jobOptions->compressAlgorithms.byte,
                                 createInfo->jobOptions->cryptAlgorithms[0],
                                 NULL,  // cryptSalt
                                 NULL,  // cryptKey
                                 archiveEntryName,
                                 fileInfo,
                                 &fileExtendedAttributeList,
                                 fragmentOffset,
                                 fragmentSize,
                                 archiveFlags
                                );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("cannot create new archive file entry '%s' (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );
      String_delete(archiveEntryName);
      (void)File_close(&fileHandle);
      fragmentDone(createInfo,fileName);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }
    String_delete(archiveEntryName);

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
                createInfo->statusInfo.compressionRatio = (!createInfo->jobOptions->dryRun && (createInfo->statusInfo.done.size > 0))
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
        printError("cannot store file entry (error: %s)!",
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
      printError("cannot close archive file entry (error: %s)!",
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
    stringClear(fragmentInfoString);
    if (fragmentSize < fileInfo->size)
    {
      d = (fragmentCount > 0) ? ceil(log10((double)fragmentCount)) : 1.00;
      stringAppend(fragmentInfoString,sizeof(fragmentInfoString),", fragment #");
      stringFormatAppend(fragmentInfoString,sizeof(fragmentInfoString),"%*u/%u",(int)d,1+fragmentNumber,fragmentCount);
    }

    // ratio info
    stringClear(compressionRatioString);
    if (   ((archiveFlags & ARCHIVE_FLAG_TRY_DELTA_COMPRESS) && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.delta))
        || ((archiveFlags & ARCHIVE_FLAG_TRY_BYTE_COMPRESS ) && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.byte ))
       )
    {
      stringFormat(compressionRatioString,sizeof(compressionRatioString),", ratio %5.1f%%",compressionRatio);
    }

    if (!createInfo->jobOptions->dryRun)
    {
      printInfo(1,"OK (%"PRIu64" bytes%s%s)\n",
                fragmentSize,
                fragmentInfoString,
                compressionRatioString
               );

      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_OK,
                 "Added file '%s' (%"PRIu64" bytes%s%s)",
                 String_cString(fileName),
                 fragmentSize,
                 fragmentInfoString,
                 compressionRatioString
                );
    }
    else
    {
      printInfo(1,"OK (%"PRIu64" bytes%s%s, dry-run)\n",
                fragmentSize,
                fragmentInfoString,
                compressionRatioString
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

    printInfo(1,"OK (%"PRIu64" bytes, not stored)\n",
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
  String           archiveEntryName;
  ArchiveEntryInfo archiveEntryInfo;
  uint64           blockOffset;
  uint64           blockCount;
  uint             bufferBlockCount;
  FragmentNode     *fragmentNode;
  uint64           archiveSize;
  uint             percentageDone;
  double           compressionRatio;
  double           d;
  char             fragmentInfoString[256],compressionRatioString[256];

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
    printError("device block size %"PRIu64" on '%s' is too big (max: %"PRIu64")",
               deviceInfo->blockSize,
               String_cString(deviceName),
               bufferSize
              );
    return ERROR_INVALID_DEVICE_BLOCK_SIZE;
  }
  if (deviceInfo->blockSize <= 0)
  {
    printInfo(1,"FAIL\n");
    printError("invalid device block size for '%s'",
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
                 "Cannot open device '%s' - skipped (error: %s)",
                 String_cString(deviceName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,deviceName,NULL)
      {
        createInfo->statusInfo.skipped.count++;
        createInfo->statusInfo.skipped.size += fragmentSize;
      }

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("cannot open device '%s' (error: %s)",
                 String_cString(deviceName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,deviceName,NULL)
      {
        createInfo->statusInfo.error.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->statusInfo.error.size  += fragmentSize;
      }

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

  if (!createInfo->jobOptions->noStorage)
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
    archiveEntryName = getArchiveEntryName(String_new(),deviceName);
    error = Archive_newImageEntry(&archiveEntryInfo,
                                  &createInfo->archiveHandle,
                                  createInfo->jobOptions->compressAlgorithms.delta,
                                  createInfo->jobOptions->compressAlgorithms.byte,
                                  createInfo->jobOptions->cryptAlgorithms[0],
                                  NULL,  // cryptSalt
                                  NULL,  // cryptKey
                                  archiveEntryName,
                                  deviceInfo,
                                  fileSystemHandle.type,
                                  fragmentOffset/(uint64)deviceInfo->blockSize,
                                  (fragmentSize+(uint64)deviceInfo->blockSize-1)/(uint64)deviceInfo->blockSize,
                                  archiveFlags
                                 );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("cannot create new archive image entry '%s' (error: %s)",
                 String_cString(deviceName),
                 Error_getText(error)
                );
      String_delete(archiveEntryName);
      if (fileSystemFlag) FileSystem_done(&fileSystemHandle);
      Device_close(&deviceHandle);
      fragmentDone(createInfo,deviceName);
      return error;
    }
    String_delete(archiveEntryName);

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
            createInfo->statusInfo.compressionRatio = (!createInfo->jobOptions->dryRun && (createInfo->statusInfo.done.size > 0))
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
        printError("cannot store image entry (error: %s)!",
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
      printError("cannot close archive image entry (error: %s)!",
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
    stringClear(fragmentInfoString);
    if (fragmentSize < deviceInfo->size)
    {
      d = (fragmentCount > 0) ? ceil(log10((double)fragmentCount)) : 1.0;
      stringAppend(fragmentInfoString,sizeof(fragmentInfoString),", fragment #");
      stringFormatAppend(fragmentInfoString,sizeof(fragmentInfoString),"%*u/%u",(int)d,1+fragmentNumber,fragmentCount);
    }

    // get ratio info
    stringClear(compressionRatioString);
    if (   ((archiveFlags & ARCHIVE_FLAG_TRY_DELTA_COMPRESS) && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.delta))
        || ((archiveFlags & ARCHIVE_FLAG_TRY_BYTE_COMPRESS ) && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.byte ))
       )
    {
      stringFormat(compressionRatioString,sizeof(compressionRatioString),", ratio %5.1f%%",compressionRatio);
    }

    // output result
    if (!createInfo->jobOptions->dryRun)
    {
      printInfo(1,"OK (%s, %"PRIu64" bytes%s%s)\n",
                (fileSystemFlag && (fileSystemHandle.type != FILE_SYSTEM_TYPE_UNKNOWN)) ? FileSystem_fileSystemTypeToString(fileSystemHandle.type,NULL) : "raw",
                fragmentSize,
                fragmentInfoString,
                compressionRatioString
               );
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_OK,
                 "Added image '%s' (%s, %"PRIu64" bytes%s%s)",
                 String_cString(deviceName),
                 (fileSystemFlag && (fileSystemHandle.type != FILE_SYSTEM_TYPE_UNKNOWN)) ? FileSystem_fileSystemTypeToString(fileSystemHandle.type,NULL) : "raw",
                 fragmentSize,
                 fragmentInfoString,
                 compressionRatioString
                );
    }
    else
    {
      printInfo(1,"OK (%s, %"PRIu64" bytes%s%s, dry-run)\n",
                fileSystemFlag ? FileSystem_fileSystemTypeToString(fileSystemHandle.type,NULL) : "raw",
                fragmentSize,
                fragmentInfoString,
                compressionRatioString
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

    d = (globalOptions.fragmentSize > 0LL) ? ceil(log10((double)globalOptions.fragmentSize)) : 1.0;
    printInfo(1,"OK (%s, %/"PRIu64" bytes, not stored)\n",
              fileSystemFlag ? FileSystem_fileSystemTypeToString(fileSystemHandle.type,NULL) : "raw",
              (int)d,
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
  String                    archiveEntryName;
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
      printError("cannot get extended attributes for '%s' (error: %s)",
                 String_cString(directoryName),
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }
  }

  if (!createInfo->jobOptions->noStorage)
  {
    // new directory
    archiveEntryName = getArchiveEntryName(String_new(),directoryName);
    error = Archive_newDirectoryEntry(&archiveEntryInfo,
                                      &createInfo->archiveHandle,
                                      createInfo->jobOptions->cryptAlgorithms[0],
                                      NULL,  // cryptSalt
                                      NULL,  // cryptKey
                                      archiveEntryName,
                                      fileInfo,
                                      &fileExtendedAttributeList
                                     );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("cannot create new archive directory entry '%s' (error: %s)",
                 String_cString(directoryName),
                 Error_getText(error)
                );
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_ACCESS_DENIED,
                 "Open failed '%s' (error: %s)",
                 String_cString(directoryName),
                 Error_getText(error)
                );
      String_delete(archiveEntryName);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }
    String_delete(archiveEntryName);

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("cannot close archive directory entry (error: %s)!",
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }

    // output result
    if (!createInfo->jobOptions->dryRun)
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
  String                    archiveEntryName;
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
      printError("cannot get extended attributes for '%s' (error: %s)",
                 String_cString(linkName),
                 Error_getText(error)
                );

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  if (!createInfo->jobOptions->noStorage)
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
        printError("cannot read link '%s' (error: %s)",
                   String_cString(linkName),
                   Error_getText(error)
                  );

        String_delete(fileName);
        File_doneExtendedAttributes(&fileExtendedAttributeList);

        return error;
      }
    }

    // new linke
    archiveEntryName = getArchiveEntryName(String_new(),linkName);
    error = Archive_newLinkEntry(&archiveEntryInfo,
                                 &createInfo->archiveHandle,
                                 createInfo->jobOptions->cryptAlgorithms[0],
                                 NULL,  // cryptSalt
                                 NULL,  // cryptKey
                                 archiveEntryName,
                                 fileName,
                                 fileInfo,
                                 &fileExtendedAttributeList
                                );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("cannot create new archive link entry '%s' (error: %s)",
                 String_cString(linkName),
                 Error_getText(error)
                );
      String_delete(archiveEntryName);
      String_delete(fileName);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }
    String_delete(archiveEntryName);

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
      printError("cannot close archive link entry (error: %s)!",
                 Error_getText(error)
                );
      String_delete(fileName);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }

    // output result
    if (!createInfo->jobOptions->dryRun)
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
  StringList                archiveEntryNameList;
  ArchiveEntryInfo          archiveEntryInfo;
  uint64                    offset;
  uint64                    size;
  ulong                     bufferLength;
  FragmentNode              *fragmentNode;
  uint64                    archiveSize;
  uint                      percentageDone;
  double                    compressionRatio;
  double                    d;
  char                      fragmentInfoString[256],compressionRatioString[256];
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
    if      (createInfo->jobOptions->noStopOnAttributeErrorFlag)
    {
      printWarning("cannot not get extended attributes for '%s' - continue (error: %s)!",
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
        createInfo->statusInfo.skipped.count++;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("cannot get extended attributes for '%s' (error: %s)",
                 String_cString(StringList_first(fileNameList,NULL)),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),NULL)
      {
        createInfo->statusInfo.error.count++;
      }

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
                 "Cannot open hardlink '%s' - skipped (error: %s)",
                 String_cString(StringList_first(fileNameList,NULL)),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),NULL)
      {
        createInfo->statusInfo.skipped.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->statusInfo.skipped.size  += fragmentSize;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("cannot open hardlink '%s' (error: %s)",
                 String_cString(StringList_first(fileNameList,NULL)),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),NULL)
      {
        createInfo->statusInfo.error.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->statusInfo.error.size  += fragmentSize;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  // init fragment
  fragmentInit(createInfo,StringList_first(fileNameList,NULL),fileInfo->size,fragmentCount);

  if (!createInfo->jobOptions->noStorage)
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
    StringList_init(&archiveEntryNameList);
    getArchiveEntryNameList(&archiveEntryNameList,fileNameList);
    error = Archive_newHardLinkEntry(&archiveEntryInfo,
                                     &createInfo->archiveHandle,
                                     createInfo->jobOptions->compressAlgorithms.delta,
                                     createInfo->jobOptions->compressAlgorithms.byte,
                                     createInfo->jobOptions->cryptAlgorithms[0],
                                     NULL,  // cryptSalt
                                     NULL,  // cryptKey
                                     &archiveEntryNameList,
                                     fileInfo,
                                     &fileExtendedAttributeList,
                                     fragmentOffset,
                                     fragmentSize,
                                     archiveFlags
                                    );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("cannot create new archive hardlink entry '%s' (error: %s)",
                 String_cString(StringList_first(fileNameList,NULL)),
                 Error_getText(error)
                );
      StringList_done(&archiveEntryNameList);
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
                createInfo->statusInfo.compressionRatio = (!createInfo->jobOptions->dryRun && (createInfo->statusInfo.done.size > 0))
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
        StringList_done(&archiveEntryNameList);
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
        StringList_done(&archiveEntryNameList);
        (void)File_close(&fileHandle);
        fragmentDone(createInfo,StringList_first(fileNameList,NULL));
        File_doneExtendedAttributes(&fileExtendedAttributeList);

        return ERROR_NONE;
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError("cannot store hardlink entry (error: %s)!",
                   Error_getText(error)
                  );

        (void)Archive_closeEntry(&archiveEntryInfo);
        StringList_done(&archiveEntryNameList);
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
      printError("cannot close archive hardlink entry (error: %s)!",
                 Error_getText(error)
                );
      StringList_done(&archiveEntryNameList);
      (void)File_close(&fileHandle);
      fragmentDone(createInfo,StringList_first(fileNameList,NULL));
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }
    StringList_done(&archiveEntryNameList);

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
    stringClear(fragmentInfoString);
    if (fragmentSize < fileInfo->size)
    {
      d = (fragmentCount > 0) ? ceil(log10((double)fragmentCount)) : 1.0;
      stringAppend(fragmentInfoString,sizeof(fragmentInfoString),", fragment #");
      stringFormatAppend(fragmentInfoString,sizeof(fragmentInfoString),"%*u/%u",(int)d,1+fragmentNumber,fragmentCount);
    }

    // get ratio info
    stringClear(compressionRatioString);
    if (   ((archiveFlags & ARCHIVE_FLAG_TRY_DELTA_COMPRESS) && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.delta))
        || ((archiveFlags & ARCHIVE_FLAG_TRY_BYTE_COMPRESS ) && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.byte ))
       )
    {
      stringFormat(compressionRatioString,sizeof(compressionRatioString),", ratio %5.1f%%",compressionRatio);
    }

    // output result
    if (!createInfo->jobOptions->dryRun)
    {
      printInfo(1,"OK (%"PRIu64" bytes%s%s)\n",
                fragmentSize,
                fragmentInfoString,
                compressionRatioString
               );
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_OK,
                 "Added hardlink '%s' (%"PRIu64" bytes%s%s)",
                 String_cString(StringList_first(fileNameList,NULL)),
                 fragmentSize,
                 fragmentInfoString,
                 compressionRatioString
                );
    }
    else
    {
      printInfo(1,"OK (%"PRIu64" bytes%s%s, dry-run)\n",
                fragmentSize,
                fragmentInfoString,
                compressionRatioString
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

    printInfo(1,"OK (%"PRIu64" bytes, not stored)\n",
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
  String                    archiveEntryName;
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
      printError("cannot get extended attributes for '%s' (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  if (!createInfo->jobOptions->noStorage)
  {
    // new special
    archiveEntryName = getArchiveEntryName(String_new(),fileName);
    error = Archive_newSpecialEntry(&archiveEntryInfo,
                                    &createInfo->archiveHandle,
                                    createInfo->jobOptions->cryptAlgorithms[0],
                                    NULL,  // cryptSalt
                                    NULL,  // cryptKey
                                    archiveEntryName,
                                    fileInfo,
                                    &fileExtendedAttributeList
                                   );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("cannot create new archive special entry '%s' (error: %s)",
                 String_cString(fileName),
                 Error_getText(error)
                );
      String_delete(archiveEntryName);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }
    String_delete(archiveEntryName);

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("cannot close archive special entry (error: %s)!",
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      return error;
    }

    // output result
    if (!createInfo->jobOptions->dryRun)
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
                      const char                   *jobUUID,
                      const char                   *scheduleUUID,
                      const char                   *scheduleTitle,
                      const char                   *entityUUID,
                      ArchiveTypes                 archiveType,
                      ConstString                  storageName,
                      const EntryList              *includeEntryList,
                      const PatternList            *excludePatternList,
                      const char                   *customText,
                      JobOptions                   *jobOptions,
                      uint64                       createdDateTime,
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
  IndexHandle      indexHandle;
  CreateInfo       createInfo;
  Errors           error;
  StorageSpecifier storageSpecifier;
  IndexId          uuidId;
  String           hostName,userName;
  IndexId          entityId;
  ThreadPoolNode   *collectorSumThreadNode,*collectorThreadNode,*collectorStorageThreadNode;
  uint             createThreadCount;
  ThreadPoolSet    createThreadSet;
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
    printError("no storage name given!");
    AutoFree_cleanup(&autoFreeList);
    return ERROR_NO_STORAGE_NAME;
  }

  // parse storage name
  Storage_initSpecifier(&storageSpecifier);
  error = Storage_parseName(&storageSpecifier,storageName);
  if (error != ERROR_NONE)
  {
    printError("cannot initialize storage '%s' (error: %s)",
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
  if (Index_isAvailable())
  {
    error = Index_open(&indexHandle,masterIO,INDEX_TIMEOUT);
    if (error != ERROR_NONE)
    {
      printError("cannot open index (error: %s)",
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&indexHandle,{ Index_close(&indexHandle); });
  }

  // init create info
  initCreateInfo(&createInfo,
                 Index_isAvailable() ? &indexHandle : NULL,
                 jobUUID,
                 scheduleUUID,
                 scheduleTitle,
                 entityUUID,
                 archiveType,
                 includeEntryList,
                 excludePatternList,
                 customText,
                 jobOptions,
                 createdDateTime,
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
                       CALLBACK_(updateStorageStatusInfo,&createInfo),
                       CALLBACK_(getNamePasswordFunction,getNamePasswordUserData),
                       CALLBACK_(storageRequestVolumeFunction,storageRequestVolumeUserData),
                       CALLBACK_(isPauseStorageFunction,isPauseStorageUserData),
                       CALLBACK_(isAbortedFunction,isAbortedUserData),
                       logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError("cannot initialize storage '%s' (error: %s)",
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
    printError("cannot write storage (error: no write access for '%s')!",
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
        printError("cannot read incremental list file '%s' (error: %s)",
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
  if (Index_isAvailable())
  {
    // get/create index job UUID
    error = Index_findUUID(&indexHandle,
                           jobUUID,
                           NULL,  // entityUUID
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
                          );
    if (Error_getCode(error) == ERROR_CODE_DATABASE_ENTRY_NOT_FOUND)
    {
      error = Index_newUUID(&indexHandle,jobUUID,&uuidId);
    }
    if (error != ERROR_NONE)
    {
      printError("cannot create index for '%s' (error: %s)!",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // create new index entity
    error = Index_newEntity(&indexHandle,
                            jobUUID,
                            entityUUID,
                            String_cString(hostName),
                            String_cString(userName),
                            archiveType,
                            createdDateTime,
                            TRUE,  // locked
                            &entityId
                           );
    if (error != ERROR_NONE)
    {
      printError("cannot create index for '%s' (error: %s)!",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    assert(!INDEX_ID_IS_NONE(entityId));
    DEBUG_TESTCODE() { (void)Index_purgeEntity(&indexHandle,entityId); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
    AUTOFREE_ADD(&autoFreeList,&entityId,{ (void)Index_unlockEntity(&indexHandle,entityId); (void)Index_purgeEntity(&indexHandle,entityId); });
  }

  // create new archive
  error = Archive_create(&createInfo.archiveHandle,
                         String_cString(hostName),
                         String_cString(userName),
                         &createInfo.storageInfo,
                         NULL,  // archiveName
                         uuidId,
                         entityId,
                         jobUUID,
                         entityUUID,
                         &jobOptions->deltaSourceList,
                         archiveType,
                         jobOptions->dryRun,
                         createdDateTime,
                         NULL,  // cryptPassword
                           ARCHIVE_FLAG_CREATE_SALT
                         | ARCHIVE_FLAG_CREATE_KEY
                         | ARCHIVE_FLAG_CREATE_META,
                         CALLBACK_(NULL,NULL),  // archiveInitFunction
                         CALLBACK_(NULL,NULL),  // archiveDoneFunction
                         CALLBACK_(archiveGetSize,&createInfo),
                         CALLBACK_(NULL,NULL),  // archiveTest
                         CALLBACK_(archiveStore,&createInfo),
                         CALLBACK_(getNamePasswordFunction,getNamePasswordUserData),
                         logHandle
                        );
  if (error != ERROR_NONE)
  {
    printError("cannot create archive file '%s' (error: %s)",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Archive_close(&createInfo.archiveHandle,FALSE); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&createInfo.archiveHandle,{ Archive_close(&createInfo.archiveHandle,FALSE); });

  // start collectors and storage thread
  collectorSumThreadNode = ThreadPool_run(&workerThreadPool,collectorSumThreadCode,&createInfo);
  assert(collectorSumThreadNode != NULL);
  collectorThreadNode = ThreadPool_run(&workerThreadPool,collectorThreadCode,&createInfo);
  assert(collectorThreadNode != NULL);
  collectorStorageThreadNode = ThreadPool_run(&workerThreadPool,storageThreadCode,&createInfo);
  assert(collectorStorageThreadNode != NULL);
  AUTOFREE_ADD(&autoFreeList,collectorSumThreadNode,{ ThreadPool_join(&workerThreadPool,collectorSumThreadNode); });
  AUTOFREE_ADD(&autoFreeList,collectorThreadNode,{ ThreadPool_join(&workerThreadPool,collectorThreadNode); });
  AUTOFREE_ADD(&autoFreeList,collectorStorageThreadNode,{ MsgQueue_setEndOfMsg(&createInfo.storageMsgQueue); ThreadPool_join(&workerThreadPool,collectorStorageThreadNode); });

  // start create threads
  createThreadCount = (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores();
  ThreadPool_initSet(&createThreadSet,&workerThreadPool);
  for (i = 0; i < createThreadCount; i++)
  {
    ThreadPool_setAdd(&createThreadSet,
                      ThreadPool_run(&workerThreadPool,createThreadCode,&createInfo)
                     );
  }
  AUTOFREE_ADD(&autoFreeList,&createThreadSet,{ ThreadPool_joinSet(&createThreadSet); ThreadPool_doneSet(&createThreadSet); });

  // wait for collector threads
  ThreadPool_join(&workerThreadPool,collectorSumThreadNode);
  ThreadPool_join(&workerThreadPool,collectorThreadNode);
  AUTOFREE_REMOVE(&autoFreeList,collectorSumThreadNode);
  AUTOFREE_REMOVE(&autoFreeList,collectorThreadNode);

  // wait for and done create threads
  MsgQueue_setEndOfMsg(&createInfo.entryMsgQueue);
  ThreadPool_joinSet(&createThreadSet);
  AUTOFREE_REMOVE(&autoFreeList,&createThreadSet);
  ThreadPool_doneSet(&createThreadSet);

  // close archive
  AUTOFREE_REMOVE(&autoFreeList,&createInfo.archiveHandle);
  error = Archive_close(&createInfo.archiveHandle,TRUE);
  if (error != ERROR_NONE)
  {
    printError("cannot close archive '%s' (error: %s)",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  // wait for storage thread
  MsgQueue_setEndOfMsg(&createInfo.storageMsgQueue);
  ThreadPool_join(&workerThreadPool,collectorStorageThreadNode);
  AUTOFREE_REMOVE(&autoFreeList,collectorStorageThreadNode);

  // final update of status info
  SEMAPHORE_LOCKED_DO(&createInfo.statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,2000)
  {
    (void)updateStatusInfo(&createInfo,TRUE);
  }

  // update index
  if (Index_isAvailable())
  {
    assert(!INDEX_ID_IS_NONE(entityId));

    // update entity, uuid info (aggregated values)
    error = Index_updateEntityInfos(&indexHandle,
                                    entityId
                                   );
    if (error != ERROR_NONE)
    {
      printError("cannot create index for '%s' (error: %s)!",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    error = Index_updateUUIDInfos(&indexHandle,
                                  uuidId
                                 );
    if (error != ERROR_NONE)
    {
      printError("cannot create index for '%s' (error: %s)!",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // unlock entity
    AUTOFREE_REMOVE(&autoFreeList,&entityId);
    (void)Index_unlockEntity(&indexHandle,entityId);

    if (   (createInfo.failError == ERROR_NONE)
        && !createInfo.jobOptions->dryRun
        && !isAborted(&createInfo)
       )
    {
      // delete entity if nothing created
      error = Index_pruneEntity(&indexHandle,entityId);
      if (error != ERROR_NONE)
      {
        printError("cannot delete empty entity for '%s' (error: %s)!",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }
    else
    {
      // delete entity on error/dry-run/abort
      error = Index_deleteEntity(&indexHandle,entityId);
      if (error != ERROR_NONE)
      {
        printWarning("cannot delete entity for '%s' (error: %s)!",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
      }
    }
  }

  // output statics
  if (createInfo.failError == ERROR_NONE)
  {
    printInfo(1,
              "%lu entries/%.1lf%s (%"PRIu64" bytes) included\n",
              createInfo.statusInfo.done.count,
              BYTES_SHORT(createInfo.statusInfo.done.size),
              BYTES_UNIT(createInfo.statusInfo.done.size),
              createInfo.statusInfo.done.size
             );
    printInfo(2,
              "%lu entries/%.1lf%s (%"PRIu64" bytes) skipped\n",
              createInfo.statusInfo.skipped.count,
              BYTES_SHORT(createInfo.statusInfo.skipped.size),
              BYTES_UNIT(createInfo.statusInfo.skipped.size),
              createInfo.statusInfo.skipped.size
             );
    printInfo(2,
              "%lu entries/%.1lf%s (%"PRIu64" bytes) with errors\n",
              createInfo.statusInfo.error.count,
              BYTES_SHORT(createInfo.statusInfo.error.size),
              BYTES_UNIT(createInfo.statusInfo.error.size),
              createInfo.statusInfo.error.size
             );
    logMessage(logHandle,
               LOG_TYPE_ALWAYS,
               "%lu entries/%.1lf%s (%"PRIu64" bytes) included, %lu entries skipped, %lu entries with errors",
               createInfo.statusInfo.done.count,
               BYTES_SHORT(createInfo.statusInfo.done.size),
               BYTES_UNIT(createInfo.statusInfo.done.size),
               createInfo.statusInfo.done.size,
               createInfo.statusInfo.skipped.count,
               createInfo.statusInfo.error.count
              );
  }

  // write incremental list
  if (   (createInfo.failError == ERROR_NONE)
      && !createInfo.jobOptions->dryRun
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
      printError("cannot write incremental list file '%s' (error: %s)",
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
  AUTOFREE_REMOVE(&autoFreeList,&jobOptions->mountList);
  error = unmountAll(&jobOptions->mountList);
  if (error != ERROR_NONE)
  {
    printWarning("cannot unmount devices (error: %s)",
                 Error_getText(error)
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
  }
  doneCreateInfo(&createInfo);
  if (Index_isAvailable())
  {
    Index_close(&indexHandle);
  }
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
