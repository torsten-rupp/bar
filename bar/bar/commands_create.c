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
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "autofree.h"
#include "strings.h"
#include "stringmaps.h"
#include "lists.h"
#include "stringlists.h"
#include "threads.h"
#include "msgqueues.h"
#include "semaphores.h"
#include "dictionaries.h"
#include "misc.h"

#include "errors.h"
#include "patterns.h"
#include "entrylists.h"
#include "patternlists.h"
#include "files.h"
#include "devices.h"
#include "filesystems.h"
#include "archive.h"
#include "deltasources.h"
#include "crypt.h"
#include "storage.h"
#include "database.h"
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
  StorageSpecifier            *storageSpecifier;                  // storage specifier structure
  IndexHandle                 *indexHandle;
  ConstString                 jobUUID;                            // unique job id to store or NULL
  ConstString                 scheduleUUID;                       // unique schedule id to store or NULL
  const EntryList             *includeEntryList;                  // list of included entries
  const PatternList           *excludePatternList;                // list of exclude patterns
  const PatternList           *compressExcludePatternList;        // exclude compression pattern list
  const DeltaSourceList       *deltaSourceList;                   // delta sources
  const JobOptions            *jobOptions;
  ArchiveTypes                archiveType;                        // archive type to create
  ConstString                 scheduleTitle;                      // schedule title or NULL
  ConstString                 scheduleCustomText;                 // schedule custom text or NULL
  bool                        *pauseCreateFlag;                   // TRUE for pause creation
  bool                        *pauseStorageFlag;                  // TRUE for pause storage
  bool                        *requestedAbortFlag;                // TRUE to abort create
  LogHandle                   *logHandle;                         // log handle

  bool                        partialFlag;                        // TRUE for create incremental/differential archive
  Dictionary                  namesDictionary;                    // dictionary with files (used for incremental/differental backup)
  bool                        storeIncrementalFileInfoFlag;       // TRUE to store incremental file data
  Storage                     storage;                      // storage handle
  time_t                      startTime;                          // start time [ms] (unix time)

  MsgQueue                    entryMsgQueue;                      // queue with entries to store

  ArchiveInfo                 archiveInfo;

  bool                        collectorSumThreadExitedFlag;       // TRUE iff collector sum thread exited

  MsgQueue                    storageMsgQueue;                    // queue with waiting storage files
  Semaphore                   storageInfoLock;                    // lock semaphore for storage info
  struct
  {
    uint                      count;                              // number of current storage files
    uint64                    bytes;                              // number of bytes in current storage files
  }                           storageInfo;
  bool                        storageThreadExitFlag;
  StringList                  storageFileList;                    // list with stored storage files

  Errors                      failError;                          // failure error

  CreateStatusInfoFunction    createStatusInfoFunction;           // status info call back
  void                        *createStatusInfoUserData;          // user data for status info call back
  CreateStatusInfo            statusInfo;                         // status info
  Semaphore                   statusInfoLock;                     // status info lock
  Semaphore                   statusInfoNameLock;                 // status info name lock
} CreateInfo;

// hard link info
typedef struct
{
  uint       count;                                               // number of hard links
  StringList nameList;                                            // list of hard linked names
} HardLinkInfo;

// entry message, send from collector thread -> main
typedef struct
{
  EntryTypes entryType;
  FileTypes  fileType;
  String     name;                                                // file/image/directory/link/special name
  StringList nameList;                                            // list of hard link names
} EntryMsg;

// storage message, send from main -> storage thread
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
//  String_delete(storageMsg->scheduleUUID);
//  String_delete(storageMsg->jobUUID);
}

/***********************************************************************\
* Name   : initStatusInfo
* Purpose: initialize status info
* Input  : statusInfo - status info variable
* Output : statusInfo - initialized create status variable
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initStatusInfo(CreateStatusInfo *statusInfo)
{
  assert(statusInfo != NULL);

  statusInfo->doneCount           = 0L;
  statusInfo->doneSize            = 0LL;
  statusInfo->totalEntryCount     = 0L;
  statusInfo->totalEntrySize      = 0LL;
  statusInfo->collectTotalSumDone = FALSE;
  statusInfo->skippedEntryCount   = 0L;
  statusInfo->skippedEntrySize    = 0LL;
  statusInfo->errorEntryCount     = 0L;
  statusInfo->errorEntrySize      = 0LL;
  statusInfo->archiveSize         = 0LL;
  statusInfo->compressionRatio    = 0.0;
  statusInfo->entryName           = String_new();
  statusInfo->entryDoneSize       = 0LL;
  statusInfo->entryTotalSize      = 0LL;
  statusInfo->storageName         = String_new();
  statusInfo->storageDoneSize     = 0LL;
  statusInfo->storageTotalSize    = 0LL;
  statusInfo->volumeNumber        = 0;
  statusInfo->volumeProgress      = 0.0;
}

/***********************************************************************\
* Name   : initCreateInfo
* Purpose: initialize create info
* Input  : createInfo                 - create info variable
*          storageSpecifier           - storage specifier structure
*          jobUUID                    - unique job id to store or NULL
*          scheduleUUID               - unique schedule id to store or NULL
*          includeEntryList           - include entry list
*          excludePatternList         - exclude pattern list
*          compressExcludePatternList - exclude compression pattern list
*          deltaSourceList            - delta source list
*          jobOptions                 - job options
*          archiveType                - archive type; see ArchiveTypes
*                                       (normal/full/incremental)
*          storageNameCustomText      - storage name custome text or NULL
*          createStatusInfoFunction   - status info call back function
*                                       (can be NULL)
*          createStatusInfoUserData   - user data for status info
*                                       function
*          pauseCreateFlag            - pause creation flag (can be
*                                       NULL)
*          pauseStorageFlag           - pause storage flag (can be NULL)
*          requestedAbortFlag         - request abort flag (can be NULL)
*          logHandle                  - log handle (can be NULL)
* Output : createInfo - initialized create info variable
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initCreateInfo(CreateInfo               *createInfo,
                          StorageSpecifier         *storageSpecifier,
                          IndexHandle              *indexHandle,
                          ConstString              jobUUID,
                          ConstString              scheduleUUID,
                          const EntryList          *includeEntryList,
                          const PatternList        *excludePatternList,
                          const PatternList        *compressExcludePatternList,
                          const DeltaSourceList    *deltaSourceList,
                          const JobOptions         *jobOptions,
                          ArchiveTypes             archiveType,
                          ConstString              scheduleTitle,
                          ConstString              scheduleCustomText,
                          CreateStatusInfoFunction createStatusInfoFunction,
                          void                     *createStatusInfoUserData,
                          bool                     *pauseCreateFlag,
                          bool                     *pauseStorageFlag,
                          bool                     *requestedAbortFlag,
                          LogHandle                *logHandle
                         )
{
  assert(createInfo != NULL);

  // init variables
  createInfo->storageSpecifier               = storageSpecifier;
  createInfo->indexHandle                    = indexHandle;
  createInfo->jobUUID                        = jobUUID;
  createInfo->scheduleUUID                   = scheduleUUID;
  createInfo->includeEntryList               = includeEntryList;
  createInfo->excludePatternList             = excludePatternList;
  createInfo->compressExcludePatternList     = compressExcludePatternList;
  createInfo->deltaSourceList                = deltaSourceList;
  createInfo->jobOptions                     = jobOptions;
  createInfo->scheduleTitle                  = scheduleTitle;
  createInfo->scheduleCustomText             = scheduleCustomText;
  createInfo->pauseCreateFlag                = pauseCreateFlag;
  createInfo->pauseStorageFlag               = pauseStorageFlag;
  createInfo->requestedAbortFlag             = requestedAbortFlag;
  createInfo->logHandle                      = logHandle;
  createInfo->storeIncrementalFileInfoFlag   = FALSE;
  createInfo->startTime                      = time(NULL);
  createInfo->collectorSumThreadExitedFlag   = FALSE;
  createInfo->storageInfo.count              = 0;
  createInfo->storageInfo.bytes              = 0LL;
  createInfo->storageThreadExitFlag          = FALSE;
  StringList_init(&createInfo->storageFileList);
  createInfo->failError                      = ERROR_NONE;
  createInfo->createStatusInfoFunction       = createStatusInfoFunction;
  createInfo->createStatusInfoUserData       = createStatusInfoUserData;
  initStatusInfo(&createInfo->statusInfo);

  if (   (archiveType == ARCHIVE_TYPE_FULL)
      || (archiveType == ARCHIVE_TYPE_INCREMENTAL)
      || (archiveType == ARCHIVE_TYPE_DIFFERENTIAL)
      || (archiveType == ARCHIVE_TYPE_CONTINUOUS)
     )
  {
    createInfo->archiveType = archiveType;
    createInfo->partialFlag =    (archiveType == ARCHIVE_TYPE_INCREMENTAL)
                              || (archiveType == ARCHIVE_TYPE_DIFFERENTIAL);
//TODO
//                              || (archiveType == ARCHIVE_TYPE_CONTINUOUS);
  }
  else
  {
    createInfo->archiveType = jobOptions->archiveType;
    createInfo->partialFlag =    (jobOptions->archiveType == ARCHIVE_TYPE_INCREMENTAL)
                              || (jobOptions->archiveType == ARCHIVE_TYPE_DIFFERENTIAL);
//TODO
//                              || (jobOptions->archiveType == ARCHIVE_TYPE_CONTINUOUS);
  }

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
  if (!Semaphore_init(&createInfo->storageInfoLock))
  {
    HALT_FATAL_ERROR("Cannot initialize storage semaphore!");
  }
  if (!Semaphore_init(&createInfo->statusInfoLock))
  {
    HALT_FATAL_ERROR("Cannot initialize status info semaphore!");
  }
  if (!Semaphore_init(&createInfo->statusInfoNameLock))
  {
    HALT_FATAL_ERROR("Cannot initialize status info name semaphore!");
  }

  DEBUG_ADD_RESOURCE_TRACE(createInfo,sizeof(CreateInfo));
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

  DEBUG_REMOVE_RESOURCE_TRACE(createInfo,sizeof(CreateInfo));

  Semaphore_done(&createInfo->statusInfoNameLock);
  Semaphore_done(&createInfo->statusInfoLock);
  Semaphore_done(&createInfo->storageInfoLock);

  MsgQueue_done(&createInfo->storageMsgQueue,(MsgQueueMsgFreeFunction)freeStorageMsg,NULL);
  MsgQueue_done(&createInfo->entryMsgQueue,(MsgQueueMsgFreeFunction)freeEntryMsg,NULL);

  String_delete(createInfo->statusInfo.storageName);
  String_delete(createInfo->statusInfo.entryName);
  StringList_done(&createInfo->storageFileList);
}

/***********************************************************************\
* Name   : readIncrementalList
* Purpose: read data of incremental list from file
* Input  : fileName        - file name
*          namesDictionary - names dictionary variable
* Output : -
* Return : ERROR_NONE if incremental list read in files dictionary,
*          error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors readIncrementalList(ConstString fileName,
                                 Dictionary  *namesDictionary
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
  if (strcmp(id,INCREMENTAL_LIST_FILE_ID) != 0)
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
  while (!File_eof(&fileHandle))
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
* Input  : fileName        - file name
*          namesDictionary - names dictionary
* Output : -
* Return : ERROR_NONE if incremental list file written, error code
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors writeIncrementalList(ConstString fileName,
                                  Dictionary  *namesDictionary
                                 )
{
  String                    directory;
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

  assert(fileName != NULL);
  assert(namesDictionary != NULL);

  // get directory of .bid file
  directory = File_getFilePathName(String_new(),fileName);

  // create directory if not existing
  if (!String_isEmpty(directory))
  {
    if      (!File_exists(directory))
    {
      error = File_makeDirectory(directory,FILE_DEFAULT_USER_ID,FILE_DEFAULT_GROUP_ID,FILE_DEFAULT_PERMISSION);
      if (error != ERROR_NONE)
      {
        String_delete(directory);
        return error;
      }
    }
    else if (!File_isDirectory(directory))
    {
      error = ERRORX_(NOT_A_DIRECTORY,0,"%s",String_cString(directory));
      String_delete(directory);
      return error;
    }
  }

  // get temporary name for new .bid file
  tmpFileName = String_new();
  error = File_getTmpFileName(tmpFileName,"bid",directory);
  if (error != ERROR_NONE)
  {
    String_delete(tmpFileName);
    String_delete(directory);
    return error;
  }

  // open file new .bid file
  error = File_open(&fileHandle,tmpFileName,FILE_OPEN_CREATE);
  if (error != ERROR_NONE)
  {
    File_delete(tmpFileName,FALSE);
    String_delete(tmpFileName);
    String_delete(directory);
    return error;
  }

  // write header
  memset(id,0,sizeof(id));
  strncpy(id,INCREMENTAL_LIST_FILE_ID,sizeof(id)-1);
  error = File_write(&fileHandle,id,sizeof(id));
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    File_delete(tmpFileName,FALSE);
    String_delete(tmpFileName);
    String_delete(directory);
    return error;
  }
  version = INCREMENTAL_LIST_FILE_VERSION;
  error = File_write(&fileHandle,&version,sizeof(version));
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    File_delete(tmpFileName,FALSE);
    String_delete(tmpFileName);
    String_delete(directory);
    return error;
  }

  // write entries
  Dictionary_initIterator(&dictionaryIterator,namesDictionary);
  while (Dictionary_getNext(&dictionaryIterator,
                            &keyData,
                            &keyLength,
                            &data,
                            &length
                           )
        )
  {
    assert(keyData != NULL);
    assert(keyLength <= 65535);
    assert(data != NULL);
    assert(length == sizeof(IncrementalListInfo));

    incrementalListInfo = (IncrementalListInfo*)data;
#if 0
{
char s[1024];

memcpy(s,keyData,keyLength);s[keyLength]=0;
fprintf(stderr,"%s,%d: %s %d\n",__FILE__,__LINE__,s,incrementalFileInfo->state);
}
#endif /* 0 */

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
    String_delete(directory);
    return error;
  }

  // rename files
  error = File_rename(tmpFileName,fileName,NULL);
  if (error != ERROR_NONE)
  {
    File_delete(tmpFileName,FALSE);
    String_delete(tmpFileName);
    String_delete(directory);
    return error;
  }

  // free resources
  String_delete(tmpFileName);
  String_delete(directory);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : isAborted
* Purpose: check if job is aborted
* Input  : createInfo - create info
* Output : -
* Return : TRUE iff aborted
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isAborted(const CreateInfo *createInfo)
{
  assert(createInfo != NULL);

  return (createInfo->requestedAbortFlag != NULL) && (*createInfo->requestedAbortFlag);
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

  while (   ((createInfo->pauseCreateFlag != NULL) && (*createInfo->pauseCreateFlag))
         && !isAborted(createInfo)
        )
  {
    Misc_udelay(500LL*US_PER_MS);
  }
}

/***********************************************************************\
* Name   : pauseStorage
* Purpose: pause storage
* Input  : createInfo - create info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void pauseStorage(const CreateInfo *createInfo)
{
  assert(createInfo != NULL);

  while (   ((createInfo->pauseStorageFlag != NULL) && (*createInfo->pauseStorageFlag))
         && !isAborted(createInfo)
        )
  {
    Misc_udelay(500LL*US_PER_MS);
  }
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

  if (createInfo->createStatusInfoFunction != NULL)
  {
    timestamp = Misc_getTimestamp();
    if (forceUpdate || (timestamp > (lastTimestamp+500LL*US_PER_MS)))
    {
      createInfo->createStatusInfoFunction(createInfo->failError,
                                           &createInfo->statusInfo,
                                           createInfo->createStatusInfoUserData
                                          );
      lastTimestamp = timestamp;
    }
  }
}

/***********************************************************************\
* Name   : updateStorageStatusInfo
* Purpose: update storage info data
* Input  : userData          - user data: create info
*          storageStatusInfo - storage status info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateStorageStatusInfo(const StorageStatusInfo *storageStatusInfo,
                                   void                    *userData
                                  )
{
  CreateInfo    *createInfo = (CreateInfo*)userData;
  SemaphoreLock semaphoreLock;

  assert(createInfo != NULL);
  assert(storageStatusInfo != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,2000)
  {
    createInfo->statusInfo.volumeNumber   = storageStatusInfo->volumeNumber;
    createInfo->statusInfo.volumeProgress = storageStatusInfo->volumeProgress;
    updateStatusInfo(createInfo,TRUE);
  }
}

/***********************************************************************\
* Name   : appendFileToEntryList
* Purpose: append file to entry list
* Input  : entryMsgQueue - entry message queue
*          entryType     - entry type
*          name          - name (will be copied!)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendFileToEntryList(MsgQueue    *entryMsgQueue,
                                 EntryTypes  entryType,
                                 ConstString name
                                )
{
  EntryMsg entryMsg;

  assert(entryMsgQueue != NULL);
  assert(name != NULL);

  // init
  entryMsg.entryType = entryType;
  entryMsg.fileType  = FILE_TYPE_FILE;
  entryMsg.name      = String_duplicate(name);
  StringList_init(&entryMsg.nameList);

  // put into message queue
  if (!MsgQueue_put(entryMsgQueue,&entryMsg,sizeof(entryMsg)))
  {
    freeEntryMsg(&entryMsg,NULL);
  }
}

/***********************************************************************\
* Name   : appendDirectoryToEntryList
* Purpose: append directory to entry list
* Input  : entryMsgQueue - entry message queue
*          entryType     - entry type
*          name          - name (will be copied!)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendDirectoryToEntryList(MsgQueue    *entryMsgQueue,
                                      EntryTypes  entryType,
                                      ConstString name
                                     )
{
  EntryMsg entryMsg;

  assert(entryMsgQueue != NULL);
  assert(name != NULL);

  // init
  entryMsg.entryType = entryType;
  entryMsg.fileType  = FILE_TYPE_DIRECTORY;
  entryMsg.name      = String_duplicate(name);
  StringList_init(&entryMsg.nameList);

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
*          fileType      - file type
*          name          - name (will be copied!)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendLinkToEntryList(MsgQueue    *entryMsgQueue,
                                 EntryTypes  entryType,
                                 ConstString name
                                )
{
  EntryMsg entryMsg;

  assert(entryMsgQueue != NULL);
  assert(name != NULL);

  // init
  entryMsg.entryType = entryType;
  entryMsg.fileType  = FILE_TYPE_LINK;
  entryMsg.name      = String_duplicate(name);
  StringList_init(&entryMsg.nameList);

  // put into message queue
  if (!MsgQueue_put(entryMsgQueue,&entryMsg,sizeof(entryMsg)))
  {
    freeEntryMsg(&entryMsg,NULL);
  }
}

/***********************************************************************\
* Name   : appendHardLinkToEntryList
* Purpose: append hard link to entry list
* Input  : entryMsgQueue - entry message queue
*          entryType     - entry type
*          nameList      - name list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendHardLinkToEntryList(MsgQueue   *entryMsgQueue,
                                     EntryTypes entryType,
                                     StringList *nameList
                                    )
{
  EntryMsg entryMsg;

  assert(entryMsgQueue != NULL);

  // init
  entryMsg.entryType = entryType;
  entryMsg.fileType  = FILE_TYPE_HARDLINK;
  entryMsg.name      = NULL;
  StringList_init(&entryMsg.nameList);
  StringList_move(nameList,&entryMsg.nameList);

  // put into message queue
  if (!MsgQueue_put(entryMsgQueue,&entryMsg,sizeof(entryMsg)))
  {
    freeEntryMsg(&entryMsg,NULL);
  }
}

/***********************************************************************\
* Name   : appendSpecialToEntryList
* Purpose: append special to entry list
* Input  : entryMsgQueue - entry message queue
*          entryType     - entry type
*          name          - name (will be copied!)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendSpecialToEntryList(MsgQueue    *entryMsgQueue,
                                    EntryTypes  entryType,
                                    ConstString name
                                   )
{
  EntryMsg entryMsg;

  assert(entryMsgQueue != NULL);
  assert(name != NULL);

  // init
  entryMsg.entryType = entryType;
  entryMsg.fileType  = FILE_TYPE_SPECIAL;
  entryMsg.name      = String_duplicate(name);
  StringList_init(&entryMsg.nameList);

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
*          time               - time
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
                                   time_t           time,
                                   int              partNumber
                                  )
{
  TextMacro textMacros[5];

  StaticString   (uuid,MISC_UUID_STRING_LENGTH);
  bool           partNumberFlag;
  TemplateHandle templateHandle;
  ulong          i,j;
  char           buffer[256];
  ulong          divisor;
  ulong          n;
  uint           z;
  int            d;

  // init variables
  Misc_getUUID(uuid);

  // init template
  templateInit(&templateHandle,
               String_cString(templateFileName),
               expandMacroMode,
               time,
               FALSE
              );

  // expand template
  TEXT_MACRO_N_CSTRING(textMacros[0],"%type", getArchiveTypeName(archiveType),TEXT_MACRO_PATTERN_CSTRING);
  TEXT_MACRO_N_CSTRING(textMacros[1],"%T",    getArchiveTypeShortName(archiveType),".");
  TEXT_MACRO_N_CSTRING(textMacros[2],"%uuid", String_cString(uuid),TEXT_MACRO_PATTERN_CSTRING);
  TEXT_MACRO_N_CSTRING(textMacros[3],"%title",(scheduleTitle != NULL) ? String_cString(scheduleTitle) : "",TEXT_MACRO_PATTERN_CSTRING);
  TEXT_MACRO_N_CSTRING(textMacros[4],"%text", (scheduleCustomText != NULL) ? String_cString(scheduleCustomText) : "",TEXT_MACRO_PATTERN_CSTRING);
  templateMacros(&templateHandle,
                 textMacros,
                 SIZE_OF_ARRAY(textMacros)
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
        String_format(fileName,".%06d",partNumber);
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
* Name   : formatIncrementalFileName
* Purpose: format incremental file name
* Input  : fileName         - file name variable
*          storageSpecifier - storage specifier
* Output : -
* Return : file name
* Notes  : -
\***********************************************************************/

LOCAL String formatIncrementalFileName(String                 fileName,
                                       const StorageSpecifier *storageSpecifier
                                      )
{
  #define SEPARATOR_CHARS "-_"

  ulong i;
  char  ch;

  // remove all macros and leading and tailing separator characters
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
          String_trimRight(fileName,SEPARATOR_CHARS);

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
  Dictionary    duplicateNamesDictionary;
  String        name;
  Errors        error;
  FileInfo      fileInfo;
  SemaphoreLock semaphoreLock;
  DeviceInfo    deviceInfo;

  assert(createInfo != NULL);
  assert(createInfo->includeEntryList != NULL);
  assert(createInfo->excludePatternList != NULL);
  assert(createInfo->jobOptions != NULL);

  // initialize variables
  if (!Dictionary_init(&duplicateNamesDictionary,CALLBACK_NULL,CALLBACK_NULL,CALLBACK_NULL))
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  name = String_new();

  if (createInfo->archiveType == ARCHIVE_TYPE_CONTINUOUS)
  {
    DatabaseQueryHandle databaseQueryHandle;
    DatabaseId          databaseId;

    // process entries from continous database
    error = Continuous_initList(&databaseQueryHandle,createInfo->jobUUID,createInfo->scheduleUUID);
    if (error == ERROR_NONE)
    {
      while (Continuous_getNext(&databaseQueryHandle,&databaseId,name))
      {
//fprintf(stderr,"%s, %d: jobUUID=%s name='%s'\n",__FILE__,__LINE__,String_cString(jobUUID),String_cString(name));
        // pause
        pauseCreate(createInfo);

        // check if file still exists
        if (!File_exists(name))
        {
          continue;
        }

        // read file info
        error = File_getFileInfo(name,&fileInfo);
        if (error != ERROR_NONE)
        {
          continue;
        }

        if (!isNoDumpAttribute(&fileInfo,createInfo->jobOptions))
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

                  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                  {
                    createInfo->statusInfo.totalEntryCount++;
                    createInfo->statusInfo.totalEntrySize += fileInfo.size;
                    updateStatusInfo(createInfo,FALSE);
                  }
                }
              }
              break;
            case FILE_TYPE_DIRECTORY:
              // add to known names history
              Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

              SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
              {
                createInfo->statusInfo.totalEntryCount++;
                updateStatusInfo(createInfo,FALSE);
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

                  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                  {
                    createInfo->statusInfo.totalEntryCount++;
                    updateStatusInfo(createInfo,FALSE);
                  }
                }
              }
              break;
            case FILE_TYPE_HARDLINK:
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

                  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                  {
                    createInfo->statusInfo.totalEntryCount++;
                    createInfo->statusInfo.totalEntrySize += fileInfo.size;
                    updateStatusInfo(createInfo,FALSE);
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

                  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                  {
                    createInfo->statusInfo.totalEntryCount++;
                    updateStatusInfo(createInfo,FALSE);
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
    while (   (includeEntryNode != NULL)
           && (createInfo->failError == ERROR_NONE)
           && !isAborted(createInfo)
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
          File_setFileNameChar(basePath,FILES_PATHNAME_SEPARATOR_CHAR);
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
        // pause
        pauseCreate(createInfo);

        // get next file/directory to process
        StringList_removeLast(&nameList,name);

        // read file info
        error = File_getFileInfo(name,&fileInfo);
        if (error != ERROR_NONE)
        {
          continue;
        }

        if (!isNoDumpAttribute(&fileInfo,createInfo->jobOptions))
        {
          switch (fileInfo.type)
          {
            case FILE_TYPE_FILE:
              if (   isIncluded(includeEntryNode,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                  && !Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name))
                 )
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
                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                      {
                        createInfo->statusInfo.totalEntryCount++;
                        createInfo->statusInfo.totalEntrySize += fileInfo.size;
                        updateStatusInfo(createInfo,FALSE);
                      }
                    }
                    break;
                  case ENTRY_TYPE_IMAGE:
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
              // add to known names history
              Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

              if (!isNoBackup(name))
              {
                if (   isIncluded(includeEntryNode,name)
                    && !isInExcludedList(createInfo->excludePatternList,name)
                   )
                {
                  switch (includeEntryNode->type)
                  {
                    case ENTRY_TYPE_FILE:
                      if (   !createInfo->partialFlag
                          || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                         )
                      {
                        SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                        {
                          createInfo->statusInfo.totalEntryCount++;
                          updateStatusInfo(createInfo,FALSE);
                        }
                      }
                      break;
                    case ENTRY_TYPE_IMAGE:
                      break;
                    default:
                      #ifndef NDEBUG
                        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                      #endif /* NDEBUG */
                      break; /* not reached */
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

                    if (   isIncluded(includeEntryNode,fileName)
                        && !isInExcludedList(createInfo->excludePatternList,fileName)
                       )
                    {
                      // read file info
                      error = File_getFileInfo(fileName,&fileInfo);
                      if (error != ERROR_NONE)
                      {
                        continue;
                      }

                      if (!isNoDumpAttribute(&fileInfo,createInfo->jobOptions))
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
                                    SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                                    {
                                      createInfo->statusInfo.totalEntryCount++;
                                      createInfo->statusInfo.totalEntrySize += fileInfo.size;
                                      updateStatusInfo(createInfo,FALSE);
                                    }
                                  }
                                  break;
                                case ENTRY_TYPE_IMAGE:
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
                                    SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                                    {
                                      createInfo->statusInfo.totalEntryCount++;
                                      updateStatusInfo(createInfo,FALSE);
                                    }
                                  }
                                  break;
                                case ENTRY_TYPE_IMAGE:
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
                                  if (  !createInfo->partialFlag
                                      || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                      )
                                  {
                                    SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                                    {
                                      createInfo->statusInfo.totalEntryCount++;
                                      createInfo->statusInfo.totalEntrySize += fileInfo.size;
                                      updateStatusInfo(createInfo,FALSE);
                                    }
                                  }
                                  break;
                                case ENTRY_TYPE_IMAGE:
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
                                    SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                                    {
                                      createInfo->statusInfo.totalEntryCount++;
                                      if (   (includeEntryNode->type == ENTRY_TYPE_IMAGE)
                                          && (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                                          )
                                      {
                                        createInfo->statusInfo.totalEntrySize += fileInfo.size;
                                      }
                                      updateStatusInfo(createInfo,FALSE);
                                    }
                                  }
                                  break;
                                case ENTRY_TYPE_IMAGE:
                                  if (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                                  {
                                    SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                                    {
                                      createInfo->statusInfo.totalEntryCount++;
                                      createInfo->statusInfo.totalEntrySize += fileInfo.size;
                                      updateStatusInfo(createInfo,FALSE);
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

                  // close directory
                  File_closeDirectoryList(&directoryListHandle);
                }
              }
              break;
            case FILE_TYPE_LINK:
              if (   isIncluded(includeEntryNode,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                  && !Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name))
                 )
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
                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                      {
                        createInfo->statusInfo.totalEntryCount++;
                        updateStatusInfo(createInfo,FALSE);
                      }
                    }
                    break;
                  case ENTRY_TYPE_IMAGE:
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
              if (   isIncluded(includeEntryNode,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                  && !Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name))
                 )
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
                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                      {
                        createInfo->statusInfo.totalEntryCount++;
                        createInfo->statusInfo.totalEntrySize += fileInfo.size;
                        updateStatusInfo(createInfo,FALSE);
                      }
                    }
                    break;
                  case ENTRY_TYPE_IMAGE:
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
              if (   isIncluded(includeEntryNode,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                  && !Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name))
                 )
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
                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                      {
                        createInfo->statusInfo.totalEntryCount++;
                        updateStatusInfo(createInfo,FALSE);
                      }
                    }
                    break;
                  case ENTRY_TYPE_IMAGE:
                    if (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                    {
                      // get device info
                      error = Device_getDeviceInfo(&deviceInfo,name);
                      if (error != ERROR_NONE)
                      {
                        continue;
                      }
                      UNUSED_VARIABLE(deviceInfo);

                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                      {
                        createInfo->statusInfo.totalEntryCount++;
                        createInfo->statusInfo.totalEntrySize += fileInfo.size;
                        updateStatusInfo(createInfo,FALSE);
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
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    createInfo->statusInfo.collectTotalSumDone = TRUE;
    updateStatusInfo(createInfo,TRUE);
  }

  // free resoures
  String_delete(name);
  Dictionary_done(&duplicateNamesDictionary);

  // terminate
  createInfo->collectorSumThreadExitedFlag = TRUE;
}

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
  AutoFreeList       autoFreeList;
  Dictionary         duplicateNamesDictionary;
  StringList         nameList;
  ulong              n;
  String             basePath;
  String             name;
  FileInfo           fileInfo;
  SemaphoreLock      semaphoreLock;
  String             fileName;
  Dictionary         hardLinksDictionary;
  Errors             error;
  DeviceInfo         deviceInfo;
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
  if (!Dictionary_init(&duplicateNamesDictionary,CALLBACK_NULL,CALLBACK_NULL,CALLBACK_NULL))
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  name = String_new();
  if (!Dictionary_init(&hardLinksDictionary,DICTIONARY_BYTE_COPY,CALLBACK_NULL,CALLBACK_NULL))
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,&duplicateNamesDictionary,{ Dictionary_done(&duplicateNamesDictionary); });
  AUTOFREE_ADD(&autoFreeList,name,{ String_delete(name); });
  AUTOFREE_ADD(&autoFreeList,&hardLinksDictionary,{ Dictionary_done(&hardLinksDictionary); });

  if (createInfo->archiveType == ARCHIVE_TYPE_CONTINUOUS)
  {
    // process entries from continous database
    while (Continuous_removeNext(createInfo->jobUUID,createInfo->scheduleUUID,name))
    {
      // pause
      pauseCreate(createInfo);

      // check if file still exists
      if (!File_exists(name))
      {
        continue;
      }

      // read file info
      error = File_getFileInfo(name,&fileInfo);
      if (error != ERROR_NONE)
      {
        printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(name),Error_getText(error));
        logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(name),Error_getText(error));

        SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          createInfo->statusInfo.doneCount++;
          createInfo->statusInfo.errorEntryCount++;
          updateStatusInfo(createInfo,FALSE);
        }
        continue;
      }

      if (!isNoDumpAttribute(&fileInfo,createInfo->jobOptions))
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
                                        name
                                       );
                }
                else
                {
                  logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Size exceeded limit '%s'\n",String_cString(name));

                  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                  {
                    createInfo->statusInfo.skippedEntryCount++;
                    createInfo->statusInfo.skippedEntrySize += fileInfo.size;
                    updateStatusInfo(createInfo,FALSE);
                  }
                }
              }
            }
            else
            {
              logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'\n",String_cString(name));

              SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
              {
                createInfo->statusInfo.skippedEntryCount++;
                createInfo->statusInfo.skippedEntrySize += fileInfo.size;
                updateStatusInfo(createInfo,FALSE);
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
                                       name
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
                                      name
                                     );
              }
            }
            else
            {
              logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'\n",String_cString(name));

              SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
              {
                createInfo->statusInfo.skippedEntryCount++;
                createInfo->statusInfo.skippedEntrySize += fileInfo.size;
                updateStatusInfo(createInfo,FALSE);
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
                HardLinkInfo hardLinkInfo;

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
                                                &data.hardLinkInfo->nameList
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
                  logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Size exceeded limit '%s'\n",String_cString(name));

                  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                  {
                    createInfo->statusInfo.skippedEntryCount++;
                    createInfo->statusInfo.skippedEntrySize += fileInfo.size;
                    updateStatusInfo(createInfo,FALSE);
                  }
                }
              }
            }
            else
            {
              logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'\n",String_cString(name));

              SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
              {
                createInfo->statusInfo.skippedEntryCount++;
                createInfo->statusInfo.skippedEntrySize += fileInfo.size;
                updateStatusInfo(createInfo,FALSE);
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
                                         name
                                        );
              }
            }
            else
            {
              logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'\n",String_cString(name));

              SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
              {
                createInfo->statusInfo.skippedEntryCount++;
                createInfo->statusInfo.skippedEntrySize += fileInfo.size;
                updateStatusInfo(createInfo,FALSE);
              }
            }
            break;
          default:
            printInfo(2,"Unknown type of file '%s' - skipped\n",String_cString(fileName));
            logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_TYPE_UNKNOWN,"Unknown type '%s'\n",String_cString(fileName));

            SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
            {
              createInfo->statusInfo.doneCount++;
              createInfo->statusInfo.doneSize += (uint64)fileInfo.size;
              createInfo->statusInfo.errorEntryCount++;
              createInfo->statusInfo.errorEntrySize += (uint64)fileInfo.size;
              updateStatusInfo(createInfo,FALSE);
            }
            break;
        }
      }

      // free resources
    }
  }
  else
  {
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
    AUTOFREE_ADD(&autoFreeList,fileName,{ String_delete(fileName); });

    // process include entries
    includeEntryNode = createInfo->includeEntryList->head;
    while (   (includeEntryNode != NULL)
           && (createInfo->failError == ERROR_NONE)
           && !isAborted(createInfo)
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
          File_setFileNameChar(basePath,FILES_PATHNAME_SEPARATOR_CHAR);
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
      while (   !StringList_isEmpty(&nameList)
             && (createInfo->failError == ERROR_NONE)
             && !isAborted(createInfo)
            )
      {
        // pause
        pauseCreate(createInfo);

        // get next entry to process
        StringList_removeLast(&nameList,name);

        // read file info
        error = File_getFileInfo(name,&fileInfo);
        if (error != ERROR_NONE)
        {
          printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(name),Error_getText(error));
          logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(name),Error_getText(error));

          SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
          {
            createInfo->statusInfo.doneCount++;
            createInfo->statusInfo.errorEntryCount++;
            updateStatusInfo(createInfo,FALSE);
          }
          continue;
        }

        if (!isNoDumpAttribute(&fileInfo,createInfo->jobOptions))
        {
          switch (fileInfo.type)
          {
            case FILE_TYPE_FILE:
              if (   isIncluded(includeEntryNode,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                 )
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
                                              name
                                             );
                      }
                      break;
                    case ENTRY_TYPE_IMAGE:
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
                logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'\n",String_cString(name));

                SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                {
                  createInfo->statusInfo.skippedEntryCount++;
                  createInfo->statusInfo.skippedEntrySize += fileInfo.size;
                  updateStatusInfo(createInfo,FALSE);
                }
              }
              break;
            case FILE_TYPE_DIRECTORY:
              // add to known names history
              Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

              if (!isNoBackup(name))
              {
                if (   isIncluded(includeEntryNode,name)
                    && !isInExcludedList(createInfo->excludePatternList,name)
                   )
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
                                                   name
                                                  );
                      }
                      break;
                    case ENTRY_TYPE_IMAGE:
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
                  logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'\n",String_cString(name));

                  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                  {
                    createInfo->statusInfo.skippedEntryCount++;
                    createInfo->statusInfo.skippedEntrySize += fileInfo.size;
                    updateStatusInfo(createInfo,FALSE);
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
                      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(name),Error_getText(error));

                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                      {
                        createInfo->statusInfo.doneCount++;
                        createInfo->statusInfo.doneSize += (uint64)fileInfo.size;
                        createInfo->statusInfo.errorEntryCount++;
                        createInfo->statusInfo.errorEntrySize += (uint64)fileInfo.size;
                        updateStatusInfo(createInfo,FALSE);
                      }
                      continue;
                    }

                    // read file info
                    error = File_getFileInfo(fileName,&fileInfo);
                    if (error != ERROR_NONE)
                    {
                      printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(fileName),Error_getText(error));
                      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(fileName),Error_getText(error));

                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                      {
                        createInfo->statusInfo.doneCount++;
                        createInfo->statusInfo.errorEntryCount++;
                        updateStatusInfo(createInfo,FALSE);
                      }
                      continue;
                    }

                    if (   isIncluded(includeEntryNode,fileName)
                        && !isInExcludedList(createInfo->excludePatternList,fileName)
                       )
                    {
                      if (!isNoDumpAttribute(&fileInfo,createInfo->jobOptions))
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
                                                          fileName
                                                         );
                                  }
                                  break;
                                case ENTRY_TYPE_IMAGE:
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
                                                          fileName
                                                         );
                                  }
                                  break;
                                case ENTRY_TYPE_IMAGE:
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
                                    HardLinkInfo hardLinkInfo;

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
                                                                    &data.hardLinkInfo->nameList
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
                                                             fileName
                                                            );
                                  }
                                  break;
                                case ENTRY_TYPE_IMAGE:
                                  if (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                                  {
                                    // add to entry list
                                    appendSpecialToEntryList(&createInfo->entryMsgQueue,
                                                              ENTRY_TYPE_IMAGE,
                                                              fileName
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
                            logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_TYPE_UNKNOWN,"Unknown type '%s'\n",String_cString(fileName));

                            SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                            {
                              createInfo->statusInfo.doneCount++;
                              createInfo->statusInfo.doneSize += (uint64)fileInfo.size;
                              createInfo->statusInfo.errorEntryCount++;
                              createInfo->statusInfo.errorEntrySize += (uint64)fileInfo.size;
                              updateStatusInfo(createInfo,FALSE);
                            }
                            break;
                        }
                      }
                      else
                      {
                        logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s' (no dump attribute)\n",String_cString(fileName));

                        SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                        {
                          createInfo->statusInfo.skippedEntryCount++;
                          createInfo->statusInfo.skippedEntrySize += fileInfo.size;
                          updateStatusInfo(createInfo,FALSE);
                        }
                      }
                    }
                    else
                    {
                      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'\n",String_cString(fileName));

                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                      {
                        createInfo->statusInfo.skippedEntryCount++;
                        createInfo->statusInfo.skippedEntrySize += fileInfo.size;
                        updateStatusInfo(createInfo,FALSE);
                      }
                    }
                  }

                  // close directory
                  File_closeDirectoryList(&directoryListHandle);
                }
                else
                {
                  printInfo(2,"Cannot open directory '%s' (error: %s) - skipped\n",String_cString(name),Error_getText(error));
                  logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(name),Error_getText(error));

                  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                  {
                    createInfo->statusInfo.doneCount++;
                    createInfo->statusInfo.errorEntryCount++;
                    updateStatusInfo(createInfo,FALSE);
                  }
                }
              }
              else
              {
                logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s' (.nobackup file)\n",String_cString(name));
              }
              break;
            case FILE_TYPE_LINK:
              if (   isIncluded(includeEntryNode,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                 )
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
                                              name
                                             );
                      }
                      break;
                    case ENTRY_TYPE_IMAGE:
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
                logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'\n",String_cString(name));

                SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                {
                  createInfo->statusInfo.skippedEntryCount++;
                  createInfo->statusInfo.skippedEntrySize += fileInfo.size;
                  updateStatusInfo(createInfo,FALSE);
                }
              }
              break;
            case FILE_TYPE_HARDLINK:
              if (   isIncluded(includeEntryNode,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                 )
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
                        HardLinkInfo hardLinkInfo;

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
                            StringList_append(&data.hardLinkInfo->nameList,name);

                            if (StringList_count(&data.hardLinkInfo->nameList) >= data.hardLinkInfo->count)
                            {
                              // found last hardlink -> add to entry list
                              appendHardLinkToEntryList(&createInfo->entryMsgQueue,
                                                        ENTRY_TYPE_FILE,
                                                        &data.hardLinkInfo->nameList
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
                logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'\n",String_cString(name));

                SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                {
                  createInfo->statusInfo.skippedEntryCount++;
                  createInfo->statusInfo.skippedEntrySize += fileInfo.size;
                  updateStatusInfo(createInfo,FALSE);
                }
              }
              break;
            case FILE_TYPE_SPECIAL:
              if (   isIncluded(includeEntryNode,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                 )
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
                                                 name
                                                );
                      }
                      break;
                    case ENTRY_TYPE_IMAGE:
                      if (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                      {
                        // get device info
                        error = Device_getDeviceInfo(&deviceInfo,name);
                        if (error != ERROR_NONE)
                        {
                          printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(name),Error_getText(error));
                          logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(name),Error_getText(error));

                          SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                          {
                            createInfo->statusInfo.doneCount++;
                            createInfo->statusInfo.errorEntryCount++;
                            updateStatusInfo(createInfo,FALSE);
                          }
                          continue;
                        }
                        UNUSED_VARIABLE(deviceInfo);

                        // add to entry list
                        appendSpecialToEntryList(&createInfo->entryMsgQueue,
                                                  ENTRY_TYPE_IMAGE,
                                                  name
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
              }
              else
              {
                logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'\n",String_cString(name));

                SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
                {
                  createInfo->statusInfo.skippedEntryCount++;
                  createInfo->statusInfo.skippedEntrySize += fileInfo.size;
                  updateStatusInfo(createInfo,FALSE);
                }
              }
              break;
            default:
              printInfo(2,"Unknown type of file '%s' - skipped\n",String_cString(name));
              logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_TYPE_UNKNOWN,"Unknown type '%s'\n",String_cString(name));

              SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
              {
                createInfo->statusInfo.doneCount++;
                createInfo->statusInfo.doneSize += (uint64)fileInfo.size;
                createInfo->statusInfo.errorEntryCount++;
                createInfo->statusInfo.errorEntrySize += (uint64)fileInfo.size;
                updateStatusInfo(createInfo,FALSE);
              }
              break;
          }
        }
        else
        {
          logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s' (no dump attribute)\n",String_cString(name));

          SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
          {
            createInfo->statusInfo.skippedEntryCount++;
            createInfo->statusInfo.skippedEntrySize += fileInfo.size;
            updateStatusInfo(createInfo,FALSE);
          }
        }

        // increment number of possible found files
        n++;

        // free resources
      }
      if (n <= 0)
      {
        logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_MISSING,"No matching entry found for '%s'\n",String_cString(includeEntryNode->string));

        SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          createInfo->statusInfo.skippedEntryCount++;
          updateStatusInfo(createInfo,FALSE);
        }

        if (createInfo->jobOptions->skipUnreadableFlag)
        {
          printWarning("No matching entry found for '%s' - skipped\n",
                       String_cString(includeEntryNode->string)
                      );
        }
        else
        {
          printError("No matching entry found for '%s'!\n",
                     String_cString(includeEntryNode->string)
                    );
          createInfo->failError = ERRORX_(FILE_NOT_FOUND_,0,"%s",String_cString(includeEntryNode->string));
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
                              &data.hardLinkInfo->nameList
                             );
  }
  Dictionary_doneIterator(&dictionaryIterator);

  // done
  MsgQueue_setEndOfMsg(&createInfo->entryMsgQueue);

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
  SemaphoreLock semaphoreLock;

  assert(createInfo != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->storageInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    createInfo->storageInfo.count += 1;
    createInfo->storageInfo.bytes += size;
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
  SemaphoreLock semaphoreLock;

  assert(createInfo != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->storageInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    assert(createInfo->storageInfo.count > 0);
    assert(createInfo->storageInfo.bytes >= size);

    createInfo->storageInfo.count -= 1;
    createInfo->storageInfo.bytes -= size;
  }
}

/***********************************************************************\
* Name   : archiveGetSize
* Purpose: call back to get archive size
* Input  : indexHandle - index handle or NULL if no index
*          storageId   - index storage id
*          partNumber  - part number or ARCHIVE_PART_NUMBER_NONE for
*                        single part
*          userData    - user data
* Output : -
* Return : archive size [bytes] or 0
* Notes  : -
\***********************************************************************/

LOCAL uint64 archiveGetSize(IndexHandle *indexHandle,
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

  assert(createInfo != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(storageId);

  archiveSize = 0LL;

  // get archive file name
  archiveName = String_new();
  error = formatArchiveFileName(archiveName,
                                createInfo->storageSpecifier->archiveName,
                                EXPAND_MACRO_MODE_STRING,
                                createInfo->archiveType,
                                createInfo->scheduleTitle,
                                createInfo->scheduleCustomText,
                                createInfo->startTime,
                                partNumber
                               );
  if (error != ERROR_NONE)
  {
    String_delete(archiveName);
    return 0LL;
  }
  DEBUG_TESTCODE() { String_delete(archiveName); return DEBUG_TESTCODE_ERROR(); }

  // get archive size
  error = Storage_open(&storageHandle,&createInfo->storage,archiveName);
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
* Input  : indexHandle          - index handle or NULL if no index
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

LOCAL Errors archiveStore(IndexHandle  *indexHandle,
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
  SemaphoreLock semaphoreLock;

  assert(createInfo != NULL);
  assert(createInfo->storageSpecifier != NULL);
  assert(!String_isEmpty(intermediateFileName));

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
  error = formatArchiveFileName(archiveName,
                                createInfo->storageSpecifier->archiveName,
                                EXPAND_MACRO_MODE_STRING,
                                createInfo->archiveType,
                                createInfo->scheduleTitle,
                                createInfo->scheduleCustomText,
                                createInfo->startTime,
                                partNumber
                               );
  if (error != ERROR_NONE)
  {
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
    return ERROR_NONE;
  }

  // update status info
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    createInfo->statusInfo.storageTotalSize += fileInfo.size;
    updateStatusInfo(createInfo,FALSE);
  }

  // wait for space in temporary directory
  if (globalOptions.maxTmpSize > 0)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->storageInfoLock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
    {
      while (   (createInfo->storageInfo.count > 2)                           // more than 2 archives are waiting
             && (createInfo->storageInfo.bytes > globalOptions.maxTmpSize)    // temporary space limit exceeded
             && (createInfo->failError == ERROR_NONE)
             && !isAborted(createInfo)
            )
      {
        Semaphore_waitModified(&createInfo->storageInfoLock,30*1000);
      }
    }
  }

  // free resources

  return ERROR_NONE;
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
  IndexId          oldUUIDId;
  IndexId          oldEntityId;
  IndexId          oldStorageId;
  String           oldStorageName;
  StorageSpecifier oldStorageSpecifier;
  Errors           error;
  IndexQueryHandle indexQueryHandle;

  // init variables
  oldStorageName = String_new();
  Storage_initSpecifier(&oldStorageSpecifier);

  // delete old indizes for same storage file
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY, // uuidId
                                 INDEX_ID_ANY, // entityId
                                 NULL, // jobUUID,
                                 NULL,  // storageIds
                                 0,  // storageIdCount
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
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
                              NULL, // job UUID
                              &oldEntityId,
                              NULL, // schedule UUID
                              NULL, // archiveType
                              &oldStorageId,
                              oldStorageName,
                              NULL, // createdDateTime
                              NULL, // size
                              NULL, // indexState
                              NULL, // indexMode
                              NULL, // lastCheckedDateTime
                              NULL, // errorMessage
                              NULL, // totalEntryCount
                              NULL // totalEntrySize
                             )
        )
  {
    if (   (oldStorageId != storageId)
        && (Storage_parseName(&oldStorageSpecifier,oldStorageName) == ERROR_NONE)
        && Storage_equalSpecifiers(storageSpecifier,archiveName,&oldStorageSpecifier,NULL)
       )
    {
      // delete old index of storage
      error = Index_deleteStorage(indexHandle,oldStorageId);
      if (error != ERROR_NONE)
      {
        break;
      }
      DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

      // delete entity index if empty and not locked
      error = Index_pruneEntity(indexHandle,oldEntityId);
      if (error != ERROR_NONE)
      {
        break;
      }
      DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

      // delete uuid index if empty
      error = Index_pruneUUID(indexHandle,oldUUIDId);
      if (error != ERROR_NONE)
      {
        break;
      }
      DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }
    }
  }
  Index_doneList(&indexQueryHandle);
  if (error != ERROR_NONE)
  {
    Storage_doneSpecifier(&oldStorageSpecifier);
    String_delete(oldStorageName);
    return error;
  }

  // free resoruces
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
  Storage          storage;
  String           dateTime;

  assert(jobUUID != NULL);
  assert(maxStorageSize > 0LL);

  // init variables
  storageName       = String_new();
  oldestStorageName = String_new();
  Storage_initSpecifier(&storageSpecifier);
  dateTime          = String_new();

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
                                   NULL,  // storageIds
                                   0,   // storageIdCount
                                     INDEX_STATE_SET(INDEX_STATE_OK)
                                   | INDEX_STATE_SET(INDEX_STATE_UPDATE_REQUESTED)
                                   | INDEX_STATE_SET(INDEX_STATE_ERROR),
                                   INDEX_MODE_SET(INDEX_MODE_AUTO),
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
                 "Purging storage for job '%s' fail (error: %s)\n",
                 String_cString(jobUUID),
                 Error_getText(error)
                );
      break;
    }
    while (Index_getNextStorage(&indexQueryHandle,
                                &uuidId,
                                NULL,  // jobUUID,
                                &entityId,
                                NULL,  // scheduleUUID,
                                NULL,  // archiveType,
                                &storageId,
                                storageName,
                                &createdDateTime,
                                &size,
                                NULL,  // indexState,
                                NULL,  // indexMode,
                                NULL,  // lastCheckedDateTime,
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
    if ((totalStorageSize > limit) && (oldestStorageId != INDEX_ID_NONE))
    {
      // delete oldest storage entry
      error = Storage_parseName(&storageSpecifier,oldestStorageName);
      if (error == ERROR_NONE)
      {
        error = Storage_init(&storage,
                             &storageSpecifier,
                             NULL,  // jobOptions
                             &globalOptions.indexDatabaseMaxBandWidthList,
                             SERVER_CONNECTION_PRIORITY_HIGH,
                             CALLBACK(NULL,NULL),  // updateStatusInfo
                             CALLBACK(NULL,NULL),  // getPassword
                             CALLBACK(NULL,NULL)  // requestVolume
                            );
        if (error == ERROR_NONE)
        {
          // delete storage
          (void)Storage_delete(&storage,
                               NULL  // archiveName
                              );

          // prune empty directories
          (void)Storage_pruneDirectories(&storage,oldestStorageName);
        }
        else
        {
          logMessage(logHandle,
                     LOG_TYPE_STORAGE,
                     "Purging storage '%s', %.1f%s (%llu bytes) fail (error: %s)\n",
                     String_cString(oldestStorageName),
                     BYTES_SHORT(oldestSize),
                     BYTES_UNIT(oldestSize),
                     oldestSize,
                     Error_getText(error)
                    );
        }
        Storage_done(&storage);
      }

      // delete index of storage
      error = Index_deleteStorage(indexHandle,oldestStorageId);
      if (error != ERROR_NONE)
      {
        logMessage(logHandle,
                   LOG_TYPE_STORAGE,
                   "Purging storage index #%llu fail (error: %s)\n",
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
                 "Job size limit exceeded (max %.1f%s): purged storage '%s', created at %s, %llu bytes\n",
                 BYTES_SHORT(maxStorageSize),
                 BYTES_UNIT(maxStorageSize),
                 String_cString(oldestStorageName),
                 String_cString(dateTime),
                 oldestSize
                );
    }
  }
  while (   (totalStorageSize > limit)
         && (oldestStorageId != INDEX_ID_NONE)
        );

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
                                uint64      limit,
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
  Storage          storage;
  String           dateTime;

  assert(server != NULL);
  assert(maxStorageSize > 0LL);

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  oldestStorageName    = String_new();
  dateTime             = String_new();

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
                                   NULL,  // storageIds
                                   0,   // storageIdCount
                                     INDEX_STATE_SET(INDEX_STATE_OK)
                                   | INDEX_STATE_SET(INDEX_STATE_UPDATE_REQUESTED)
                                   | INDEX_STATE_SET(INDEX_STATE_ERROR),
                                   INDEX_MODE_SET(INDEX_MODE_AUTO),
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
                 "Purging storage for server '%s' fail (error: %s)\n",
                 String_cString(server->name),
                 Error_getText(error)
                );
      break;
    }
    while (Index_getNextStorage(&indexQueryHandle,
                                &uuidId,
                                NULL,  // jobUUID,
                                &entityId,
                                NULL,  // scheduleUUID,
                                NULL,  // archiveType,
                                &storageId,
                                storageName,
                                &createdDateTime,
                                &size,
                                NULL,  // indexState,
                                NULL,  // indexMode,
                                NULL,  // lastCheckedDateTime,
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

    if ((totalStorageSize > limit) && (oldestStorageId != INDEX_ID_NONE))
    {
      // delete oldest storage entry
      error = Storage_parseName(&storageSpecifier,oldestStorageName);
      if (error == ERROR_NONE)
      {
        error = Storage_init(&storage,
                             &storageSpecifier,
                             NULL,  // jobOptions
                             &globalOptions.indexDatabaseMaxBandWidthList,
                             SERVER_CONNECTION_PRIORITY_HIGH,
                             CALLBACK(NULL,NULL),  // updateStatusInfo
                             CALLBACK(NULL,NULL),  // getPassword
                             CALLBACK(NULL,NULL)  // requestVolume
                            );
        if (error == ERROR_NONE)
        {
          // delete storage
          (void)Storage_delete(&storage,
                               NULL  // archiveName
                              );

          // prune empty directories
          (void)Storage_pruneDirectories(&storage,oldestStorageName);
        }
        else
        {
          logMessage(logHandle,
                     LOG_TYPE_STORAGE,
                     "Purging storage '%s', %.1f%s (%llu bytes) fail (error: %s)\n",
                     String_cString(oldestStorageName),
                     BYTES_SHORT(oldestSize),
                     BYTES_UNIT(oldestSize),
                     oldestSize,
                     Error_getText(error)
                    );
        }
        Storage_done(&storage);
      }

      // delete index of storage
      error = Index_deleteStorage(indexHandle,oldestStorageId);
      if (error != ERROR_NONE)
      {
        logMessage(logHandle,
                   LOG_TYPE_STORAGE,
                   "Purging storage index #%llu fail (error: %s)\n",
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
                 "Server size limit exceeded (max %.1f%s): purged storage '%s', created at %s, %llu bytes\n",
                 BYTES_SHORT(maxStorageSize),
                 BYTES_UNIT(maxStorageSize),
                 String_cString(oldestStorageName),
                 String_cString(dateTime),
                 oldestSize
                );
    }
  }
  while (   (totalStorageSize > limit)
         && (oldestStorageId != INDEX_ID_NONE)
        );

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
  ConstString      printableStorageName;
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

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);
  assert(createInfo->storageSpecifier != NULL);

  // init variables
  AutoFree_init(&autoFreeList);
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  pathName = String_new();
  existingStorageName = String_new();
  existingPathName = String_new();
  Storage_initSpecifier(&existingStorageSpecifier);
  AUTOFREE_ADD(&autoFreeList,buffer,{ free(buffer); });
  AUTOFREE_ADD(&autoFreeList,pathName,{ String_delete(pathName); });
  AUTOFREE_ADD(&autoFreeList,existingStorageName,{ String_delete(existingStorageName); });
  AUTOFREE_ADD(&autoFreeList,existingPathName,{ String_delete(existingPathName); });
  AUTOFREE_ADD(&autoFreeList,&existingStorageSpecifier,{ Storage_doneSpecifier(&existingStorageSpecifier); });

  // initial storage pre-processing
  if (   (createInfo->failError == ERROR_NONE)
      && !isAborted(createInfo)
     )
  {
    // pause
    pauseStorage(createInfo);

    // pre-process
    if (!isAborted(createInfo))
    {
      error = Storage_preProcess(&createInfo->storage,NULL,createInfo->startTime,TRUE);
      if (error != ERROR_NONE)
      {
        printError("Cannot pre-process storage (error: %s)!\n",
                   Error_getText(error)
                  );
        createInfo->failError = error;
      }
    }
  }

  // store archives
  while (   (createInfo->failError == ERROR_NONE)
         && !isAborted(createInfo)
        )
  {
    autoFreeSavePoint = AutoFree_save(&autoFreeList);

    // pause
    pauseStorage(createInfo);
    if (isAborted(createInfo)) break;

    // get next archive to store
    if (!MsgQueue_get(&createInfo->storageMsgQueue,&storageMsg,NULL,sizeof(storageMsg),WAIT_FOREVER))
    {
      break;
    }
    AUTOFREE_ADD(&autoFreeList,&storageMsg,
                 {
                   storageInfoDecrement(createInfo,storageMsg.fileSize);
                   File_delete(storageMsg.fileName,FALSE);
                   if (storageMsg.storageId != INDEX_ID_NONE) Index_deleteStorage(createInfo->indexHandle,storageMsg.storageId);
                   freeStorageMsg(&storageMsg,NULL);
                 }
                );

    // get printable storage name
    printableStorageName = Storage_getPrintableName(createInfo->storageSpecifier,storageMsg.archiveName);

    // pre-process
    error = Storage_preProcess(&createInfo->storage,storageMsg.archiveName,createInfo->startTime,FALSE);
    if (error != ERROR_NONE)
    {
      printError("Cannot pre-process file '%s' (error: %s)!\n",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      createInfo->failError = error;

      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }
    DEBUG_TESTCODE() { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); break; }

    // get file info
    error = File_getFileInfo(storageMsg.fileName,&fileInfo);
    if (error != ERROR_NONE)
    {
      printError("Cannot get information for file '%s' (error: %s)!\n",
                 String_cString(storageMsg.fileName),
                 Error_getText(error)
                );
      createInfo->failError = error;

      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }
    DEBUG_TESTCODE() { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); break; }

    // check storage size, purge old archives
    if (!createInfo->jobOptions->dryRunFlag && (createInfo->jobOptions->maxStorageSize > 0LL))
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
      getServerSettings(createInfo->storageSpecifier,createInfo->jobOptions,&server);
      if (server.maxStorageSize > fileInfo.size)
      {
        purgeStorageByServer(createInfo->indexHandle,
                             &server,
                             server.maxStorageSize-fileInfo.size,
                             server.maxStorageSize,
                             createInfo->logHandle
                            );
      }
      doneServer(&server);
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
      printInfo(0,"FAIL!\n");
      printError("Cannot open file '%s' (error: %s)!\n",
                 String_cString(storageMsg.fileName),
                 Error_getText(error)
                );
      createInfo->failError = error;

      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }
    AUTOFREE_ADD(&autoFreeList,&fileHandle,{ File_close(&fileHandle); });
    DEBUG_TESTCODE() { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); continue; }

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
      pauseStorage(createInfo);
      if (isAborted(createInfo)) break;

      // check if append to storage
      appendFlag =    (createInfo->storage.jobOptions != NULL)
                   && (createInfo->storage.jobOptions->archiveFileMode == ARCHIVE_FILE_MODE_APPEND)
                   && Storage_exists(&createInfo->storage,storageMsg.archiveName);

      // create/append storage file
      error = Storage_create(&storageHandle,
                             &createInfo->storage,
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

      // update status info
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,2000)
      {
        String_set(createInfo->statusInfo.storageName,printableStorageName);
        updateStatusInfo(createInfo,FALSE);
      }

      // store data
      File_seek(&fileHandle,0);
      do
      {
        // pause
        pauseStorage(createInfo);
        if (isAborted(createInfo)) break;

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

        // update status info, check for abort
        SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          createInfo->statusInfo.storageDoneSize += (uint64)bufferLength;
          updateStatusInfo(createInfo,FALSE);
        }
      }
      while (   !File_eof(&fileHandle)
             && (createInfo->failError == ERROR_NONE)
             && !isAborted(createInfo)
            );

//TODO: on error restore to original size/delete

      // get archive size
      archiveSize = Storage_getSize(&storageHandle);

      // close storage
      Storage_close(&storageHandle);
    }
    while (   ((error != ERROR_NONE) && (Error_getCode(error) != ENOSPC))      // some error amd not "no space left"
           && (retryCount <= MAX_RETRIES)                                      // still some retry left
           && (createInfo->failError == ERROR_NONE)                            // no eror
           && !isAborted(createInfo)                                           // not aborted
          );
    if (error != ERROR_NONE)
    {
      if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }

    // close file to store
    File_close(&fileHandle);
    AUTOFREE_REMOVE(&autoFreeList,&fileHandle);

    // check if aborted
    if (isAborted(createInfo))
    {
      printInfo(1,"ABORTED\n");
      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }

    // done
    printInfo(1,"OK\n");
    logMessage(createInfo->logHandle,
               LOG_TYPE_STORAGE,
               "%s '%s' (%llu bytes)\n",
               appendFlag ? "Appended to" : "Storged",
               String_cString(printableStorageName),
               archiveSize
              );

    // update index database and set state
    if (storageMsg.storageId != INDEX_ID_NONE)
    {
      assert(storageMsg.entityId != INDEX_ID_NONE);

      // check if append and storage exists => assign to existing storage index
      if (   appendFlag
          && Index_findStorageByName(createInfo->indexHandle,
                                     createInfo->storageSpecifier,
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
        error = Index_setState(createInfo->indexHandle,
                               storageId,
                               INDEX_STATE_CREATE,
                               0LL,  // lastCheckedDateTime
                               NULL // errorMessage
                              );
        if (error != ERROR_NONE)
        {
          if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

          AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
          continue;
        }
        AUTOFREE_ADD(&autoFreeList,&storageMsg.storageId,
        {
          (void)Index_setState(createInfo->indexHandle,
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
          printError("Cannot update index for storage '%s' (error: %s)!\n",
                     String_cString(printableStorageName),
                     Error_getText(error)
                    );
          if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

          AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
          continue;
        }

        // delete index of storage (have to be empty now)
        assert(Index_isEmptyStorage(createInfo->indexHandle,storageMsg.storageId));
        (void)Index_deleteStorage(createInfo->indexHandle,storageMsg.storageId);

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
                                  createInfo->storageSpecifier,
                                  storageMsg.archiveName
                                 );
        if (error != ERROR_NONE)
        {
          printError("Cannot delete old index for storage '%s' (error: %s)!\n",
                     String_cString(printableStorageName),
                     Error_getText(error)
                    );
          if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

          AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
          continue;
        }

        // append storage to existing entity which have the same storage directory
        if (createInfo->storage.jobOptions->archiveFileMode == ARCHIVE_FILE_MODE_APPEND)
        {
//fprintf(stderr,"%s, %d: append to entity of uuid %llu\n",__FILE__,__LINE__,storageMsg.uuidId);
          printableStorageName = Storage_getPrintableName(createInfo->storageSpecifier,storageMsg.archiveName);

          // find matching entity and assign storage to entity
          File_getFilePathName(pathName,storageMsg.archiveName);
          error = Index_initListStorages(&indexQueryHandle,
                                         createInfo->indexHandle,
                                         storageMsg.uuidId,
                                         INDEX_ID_ANY, // entityId
                                         NULL, // jobUUID,
                                         NULL,  // storageIds
                                         0,  // storageIdCount
                                         INDEX_STATE_SET_ALL,
                                         INDEX_MODE_SET_ALL,
                                         NULL,  // archiveName,
                                         INDEX_STORAGE_SORT_MODE_NONE,
                                         DATABASE_ORDERING_NONE,
                                         0LL,  // offset
                                         INDEX_UNLIMITED
                                        );
          if (error != ERROR_NONE)
          {
            if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

            AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
            continue;
          }
          while (Index_getNextStorage(&indexQueryHandle,
                                      NULL,  // uuidId
                                      NULL,  // job UUID
                                      &existingEntityId,  // entityId,
                                      NULL,  // schedule UUID
                                      NULL,  // archiveType
                                      &existingStorageId,
                                      existingStorageName,
                                      NULL,  // createdDateTime
                                      NULL,  // size
                                      NULL,  // indexState,
                                      NULL,  // indexMode,
                                      NULL,  // lastCheckedDateTime,
                                      NULL,  // errorMessage
                                      NULL,  // totalEntryCount
                                      NULL  // totalEntrySize
                                     )
                )
          {
            File_getFilePathName(existingPathName,existingStorageName);
            if (   (storageId != existingStorageId)
                && (Storage_parseName(&existingStorageSpecifier,existingStorageName) == ERROR_NONE)
                && Storage_equalSpecifiers(&existingStorageSpecifier,pathName,&existingStorageSpecifier,existingPathName)
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

            AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
            continue;
          }

          // prune entity (maybe empty now)
          (void)Index_pruneEntity(createInfo->indexHandle,storageMsg.entityId);
        }
      }

      // update index database archive name and size
      if (error == ERROR_NONE)
      {
        error = Index_storageUpdate(createInfo->indexHandle,
                                    storageId,
                                    printableStorageName,
                                    archiveSize
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
        error = Index_setState(createInfo->indexHandle,
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
        printError("Cannot update index for storage '%s' (error: %s)!\n",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
        if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        continue;
      }
      DEBUG_TESTCODE() { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); continue; }

      AUTOFREE_REMOVE(&autoFreeList,&storageMsg.storageId);
    }

    // post-process
    error = Storage_postProcess(&createInfo->storage,storageMsg.archiveName,createInfo->startTime,FALSE);
    if (error != ERROR_NONE)
    {
      printError("Cannot post-process storage file '%s' (error: %s)!\n",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }
    DEBUG_TESTCODE() { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); continue; }

    // delete temporary storage file
    error = File_delete(storageMsg.fileName,FALSE);
    if (error != ERROR_NONE)
    {
      printWarning("Cannot delete file '%s' (error: %s)!\n",
                   String_cString(storageMsg.fileName),
                   Error_getText(error)
                  );
    }

    // add to list of stored archive files
    StringList_append(&createInfo->storageFileList,storageMsg.archiveName);

    // update storage info
    storageInfoDecrement(createInfo,storageMsg.fileSize);

    // free resources
    freeStorageMsg(&storageMsg,NULL);
    AutoFree_restore(&autoFreeList,autoFreeSavePoint,FALSE);
  }

  // discard unprocessed archives
  while (MsgQueue_get(&createInfo->storageMsgQueue,&storageMsg,NULL,sizeof(storageMsg),NO_WAIT))
  {
    // discard index
    if (storageMsg.storageId != INDEX_ID_NONE) Index_deleteStorage(createInfo->indexHandle,storageMsg.storageId);

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

  // final storage post-processing
  if (   (createInfo->failError == ERROR_NONE)
      && !isAborted(createInfo)
     )
  {
    // pause
    pauseStorage(createInfo);

    // post-processing
    if (!isAborted(createInfo))
    {
      error = Storage_postProcess(&createInfo->storage,NULL,createInfo->startTime,TRUE);
      if (error != ERROR_NONE)
      {
        printError("Cannot post-process storage (error: %s)!\n",
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
                                    createInfo->storageSpecifier->archiveName,
                                    EXPAND_MACRO_MODE_PATTERN,
                                    createInfo->archiveType,
                                    createInfo->scheduleTitle,
                                    createInfo->scheduleCustomText,
                                    createInfo->startTime,
                                    ARCHIVE_PART_NUMBER_NONE
                                   );
      if (error == ERROR_NONE)
      {
        // delete all matching storage files which are unknown
        (void)Storage_forAll(pattern,
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
                                   Storage_delete(&createInfo->storage,storageName);
                                 }
                               }

                               // free resources
                               Storage_doneSpecifier(&storageSpecifier);

                               return error;
                             },NULL)
                            );
      }
      String_delete(pattern);
    }
  }

  // free resoures
  Storage_doneSpecifier(&existingStorageSpecifier);
  String_delete(existingPathName);
  String_delete(existingStorageName);
  String_delete(pathName);
  free(buffer);
  AutoFree_done(&autoFreeList);

  createInfo->storageThreadExitFlag = TRUE;
}

/***********************************************************************\
* Name   : setStatusEntryDoneInfo
* Purpose: set status entry done info
* Input  : createInfo - create info structure
*          name - name of entry
*          size - size of entry
* Output : -
* Return : TRUE if status entry done info locked and set, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool setStatusEntryDoneInfo(CreateInfo *createInfo, ConstString name, uint64 size)
{
  bool          locked;
  SemaphoreLock semaphoreLock;

  locked = Semaphore_lock(&createInfo->statusInfoNameLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,NO_WAIT);
  if (locked)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      String_set(createInfo->statusInfo.entryName,name);
      createInfo->statusInfo.entryDoneSize  = 0LL;
      createInfo->statusInfo.entryTotalSize = size;
      updateStatusInfo(createInfo,FALSE);
    }
  }

  return locked;
}

/***********************************************************************\
* Name   : clearStatusEntryDoneInfo
* Purpose: clear status entry done info
* Input  : createInfo - create info structure
*          locked     - TRUE iff previously locked
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearStatusEntryDoneInfo(CreateInfo *createInfo, bool locked)
{
  if (locked)
  {
    Semaphore_unlock(&createInfo->statusInfoNameLock);
  }
}

/***********************************************************************\
* Name   : storeFileEntry
* Purpose: store a file entry into archive
* Input  : createInfo  - create info structure
*          indexHandle - index handle or NULL if no index
*          fileName    - file name to store
*          buffer      - buffer for temporary data
*          bufferSize  - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeFileEntry(CreateInfo  *createInfo,
                            IndexHandle *indexHandle,
                            ConstString fileName,
                            byte        *buffer,
                            uint        bufferSize
                           )
{
  bool                      statusEntryDoneLocked;
  Errors                    error;
  FileInfo                  fileInfo;
  FileExtendedAttributeList fileExtendedAttributeList;
  FileHandle                fileHandle;
  bool                      byteCompressFlag;
  bool                      deltaCompressFlag;
  ArchiveEntryInfo          archiveEntryInfo;
  SemaphoreLock             semaphoreLock;
  uint64                    entryDoneSize;
  ulong                     bufferLength;
  uint64                    archiveSize;
  uint64                    doneSize;
  double                    compressionRatio;
  uint                      percentageDone;

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);
  assert(fileName != NULL);
  assert(buffer != NULL);

  printInfo(1,"Add '%s'...",String_cString(fileName));

  // get file info
  error = File_getFileInfo(fileName,&fileInfo);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(fileName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        createInfo->statusInfo.doneCount++;
        createInfo->statusInfo.errorEntryCount++;
      }
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get info for '%s' (error: %s)\n",
                 String_cString(fileName),
                 Error_getText(error)
                );
      return error;
    }
  }

  // try to set status done entry info
  statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,fileName,fileInfo.size);

  // get file extended attributes
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,fileName);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(fileName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        createInfo->statusInfo.doneCount++;
        createInfo->statusInfo.errorEntryCount++;
      }
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get extended attributes for '%s' (error: %s)\n",
                 String_cString(fileName),
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
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
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Open file failed '%s' (error: %s)\n",String_cString(fileName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        createInfo->statusInfo.doneCount++;
        createInfo->statusInfo.doneSize += (uint64)fileInfo.size;
        createInfo->statusInfo.errorEntryCount++;
        createInfo->statusInfo.errorEntrySize += (uint64)fileInfo.size;
      }
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot open file '%s' (error: %s)\n",
                 String_cString(fileName),
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
  }

  if (!createInfo->jobOptions->noStorageFlag)
  {
    // check if file data should be byte compressed
    byteCompressFlag =    (fileInfo.size > globalOptions.compressMinFileSize)
                       && !PatternList_match(createInfo->compressExcludePatternList,fileName,PATTERN_MATCH_MODE_EXACT);

    // check if file data should be delta compressed
    deltaCompressFlag =    (fileInfo.size > globalOptions.compressMinFileSize)
                        && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.delta);

    // create new archive file entry
    error = Archive_newFileEntry(&archiveEntryInfo,
                                 &createInfo->archiveInfo,
                                 indexHandle,
                                 fileName,
                                 &fileInfo,
                                 &fileExtendedAttributeList,
                                 deltaCompressFlag,
                                 byteCompressFlag
                                );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive file entry '%s' (error: %s)\n",
                 String_cString(fileName),
                 Error_getText(error)
                );
      (void)File_close(&fileHandle);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }

    // write file content to archive
    bufferLength  = 0L;
    error         = ERROR_NONE;
    entryDoneSize = 0LL;
    do
    {
      // pause
      pauseCreate(createInfo);

      // read file data
      error = File_read(&fileHandle,buffer,bufferSize,&bufferLength);
      if (error == ERROR_NONE)
      {
        // write data to archive
        if (bufferLength > 0L)
        {
          error = Archive_writeData(&archiveEntryInfo,buffer,bufferLength,1);
          if (error == ERROR_NONE)
          {
            entryDoneSize += (uint64)bufferLength;
            archiveSize   = Archive_getSize(&createInfo->archiveInfo);

            // try to set status done entry info
            if (!statusEntryDoneLocked)
            {
              statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,fileName,fileInfo.size);
            }

            // update status info
            SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
            {
              doneSize         = createInfo->statusInfo.doneSize+(uint64)bufferLength;
              archiveSize      = createInfo->statusInfo.storageTotalSize+archiveSize;
              compressionRatio = (!createInfo->jobOptions->dryRunFlag && (doneSize > 0))
                                   ? 100.0-(archiveSize*100.0)/doneSize
                                   : 0.0;

              if (statusEntryDoneLocked)
              {
                String_set(createInfo->statusInfo.entryName,fileName);
                createInfo->statusInfo.entryDoneSize  = entryDoneSize;
                createInfo->statusInfo.entryTotalSize = fileInfo.size;
              }
              createInfo->statusInfo.doneSize         = doneSize;
              createInfo->statusInfo.archiveSize      = archiveSize;
              createInfo->statusInfo.compressionRatio = compressionRatio;
              updateStatusInfo(createInfo,TRUE);
            }
          }

          if (isPrintInfo(2))
          {
            percentageDone = 0;
            SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
            {
              percentageDone = (createInfo->statusInfo.entryTotalSize > 0LL)
                                 ? (uint)((createInfo->statusInfo.entryDoneSize*100LL)/createInfo->statusInfo.entryTotalSize)
                                 : 100;
            }
            printInfo(2,"%3d%%\b\b\b\b",percentageDone);
          }
        }
      }
    }
    while (   (bufferLength > 0L)
           && (createInfo->failError == ERROR_NONE)
           && !isAborted(createInfo)
           && (error == ERROR_NONE)
          );
    if (isAborted(createInfo))
    {
      printInfo(1,"ABORTED\n");
      (void)Archive_closeEntry(&archiveEntryInfo);
      (void)File_close(&fileHandle);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return FALSE;
    }
    if (error != ERROR_NONE)
    {
      if (createInfo->jobOptions->skipUnreadableFlag)
      {
        printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
        logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Open file failed '%s' (error: %s)\n",String_cString(fileName),Error_getText(error));
        SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          createInfo->statusInfo.doneCount++;
          createInfo->statusInfo.doneSize += (uint64)fileInfo.size;
          createInfo->statusInfo.errorEntryCount++;
          createInfo->statusInfo.errorEntrySize += (uint64)fileInfo.size;
        }
        (void)Archive_closeEntry(&archiveEntryInfo);
        (void)File_close(&fileHandle);
        File_doneExtendedAttributes(&fileExtendedAttributeList);
        clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
        return ERROR_NONE;
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError("Cannot store file entry (error: %s)!\n",
                   Error_getText(error)
                  );
        (void)Archive_closeEntry(&archiveEntryInfo);
        (void)File_close(&fileHandle);
        File_doneExtendedAttributes(&fileExtendedAttributeList);
        clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
        return error;
      }
    }
    printInfo(2,"    \b\b\b\b");

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive file entry (error: %s)!\n",
                 Error_getText(error)
                );
      (void)File_close(&fileHandle);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
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

    if (!createInfo->jobOptions->dryRunFlag)
    {
      printInfo(1,"OK (%llu bytes, ratio %.1f%%)\n",fileInfo.size,compressionRatio);
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_OK,"Added '%s'\n",String_cString(fileName));
    }
    else
    {
      printInfo(1,"OK (%llu bytes, dry-run)\n",fileInfo.size);
    }
  }
  else
  {
    printInfo(1,"OK (%llu bytes, not stored)\n",fileInfo.size);
  }

  // update done entries
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    createInfo->statusInfo.doneCount++;
    updateStatusInfo(createInfo,FALSE);
  }

  // close file
  (void)File_close(&fileHandle);

  // free resources
  File_doneExtendedAttributes(&fileExtendedAttributeList);

  // add to incremental list
  if (createInfo->storeIncrementalFileInfoFlag)
  {
    addIncrementalList(&createInfo->namesDictionary,fileName,&fileInfo);
  }

  // unlock status entry done info
  clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeImageEntry
* Purpose: store an image entry into archive
* Input  : createInfo  - create info structure
*          indexHandle - index handle or NULL if no index
*          deviceName  - device name
*          buffer      - buffer for temporary data
*          bufferSize  - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeImageEntry(CreateInfo  *createInfo,
                             IndexHandle *indexHandle,
                             ConstString deviceName,
                             byte        *buffer,
                             uint        bufferSize
                            )
{
  bool             statusEntryDoneLocked;
  Errors           error;
  DeviceInfo       deviceInfo;
  uint             maxBufferBlockCount;
  DeviceHandle     deviceHandle;
  bool             fileSystemFlag;
  FileSystemHandle fileSystemHandle;
  bool             byteCompressFlag;
  bool             deltaCompressFlag;
  SemaphoreLock    semaphoreLock;
  uint64           entryDoneSize;
  uint64           block;
  uint64           blockCount;
  uint             bufferBlockCount;
  uint64           archiveSize;
  uint64           doneSize;
  double           compressionRatio;
  uint             percentageDone;
  ArchiveEntryInfo archiveEntryInfo;

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);
  assert(deviceName != NULL);
  assert(buffer != NULL);

  printInfo(1,"Add '%s'...",String_cString(deviceName));

  // get device info
  error = Device_getDeviceInfo(&deviceInfo,deviceName);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(deviceName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        createInfo->statusInfo.doneCount++;
        createInfo->statusInfo.errorEntryCount++;
      }
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot open device '%s' (error: %s)\n",
                 String_cString(deviceName),
                 Error_getText(error)
                );
      return error;
    }
  }

  // try to set status done entry info
  statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,deviceName,deviceInfo.size);

  // check device block size, get max. blocks in buffer
  if (deviceInfo.blockSize > bufferSize)
  {
    printInfo(1,"FAIL\n");
    printError("Device block size %llu on '%s' is too big (max: %llu)\n",
               deviceInfo.blockSize,
               String_cString(deviceName),
               bufferSize
              );
    clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
    return ERROR_INVALID_DEVICE_BLOCK_SIZE;
  }
  if (deviceInfo.blockSize <= 0)
  {
    printInfo(1,"FAIL\n");
    printError("Cannot get device block size for '%s'\n",
               String_cString(deviceName)
              );
    clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
    return ERROR_INVALID_DEVICE_BLOCK_SIZE;
  }
  assert(deviceInfo.blockSize > 0);
  maxBufferBlockCount = bufferSize/deviceInfo.blockSize;

  // open device
  error = Device_open(&deviceHandle,deviceName,DEVICE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Open device failed '%s'\n",String_cString(deviceName));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        createInfo->statusInfo.doneCount++;
        createInfo->statusInfo.doneSize += (uint64)deviceInfo.size;
        createInfo->statusInfo.errorEntryCount++;
        createInfo->statusInfo.errorEntrySize += (uint64)deviceInfo.size;
      }
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot open device '%s' (error: %s)\n",
                 String_cString(deviceName),
                 Error_getText(error)
                );
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
  }

  // check if device contain a known file system or a raw image should be stored
  if (!createInfo->jobOptions->rawImagesFlag)
  {
    fileSystemFlag = (FileSystem_init(&fileSystemHandle,&deviceHandle) == ERROR_NONE);
  }
  else
  {
    fileSystemFlag = FALSE;
  }

  if (!createInfo->jobOptions->noStorageFlag)
  {
    // check if image data should be byte compressed
    byteCompressFlag =    (deviceInfo.size > globalOptions.compressMinFileSize)
                       && !PatternList_match(createInfo->compressExcludePatternList,deviceName,PATTERN_MATCH_MODE_EXACT);

    // check if file data should be delta compressed
    deltaCompressFlag =    (deviceInfo.size > globalOptions.compressMinFileSize)
                        && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.delta);

    // create new archive image entry
    error = Archive_newImageEntry(&archiveEntryInfo,
                                  &createInfo->archiveInfo,
                                  indexHandle,
                                  deviceName,
                                  &deviceInfo,
                                  fileSystemHandle.type,
                                  deltaCompressFlag,
                                  byteCompressFlag
                                 );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive image entry '%s' (error: %s)\n",
                 String_cString(deviceName),
                 Error_getText(error)
                );
      if (fileSystemFlag) FileSystem_done(&fileSystemHandle);
      Device_close(&deviceHandle);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }

    // write device content to archive
    block         = 0LL;
    blockCount    = deviceInfo.size/(uint64)deviceInfo.blockSize;
    error         = ERROR_NONE;
    entryDoneSize = 0LL;
    while (   (block < blockCount)
           && (error == ERROR_NONE)
           && (createInfo->failError == ERROR_NONE)
           && !isAborted(createInfo)
          )
    {
      // pause
      pauseCreate(createInfo);

      // read blocks from device
      bufferBlockCount = 0;
      while (   (block < blockCount)
             && (bufferBlockCount < maxBufferBlockCount)
            )
      {
        if (   !fileSystemFlag
            || FileSystem_blockIsUsed(&fileSystemHandle,block*(uint64)deviceInfo.blockSize)
           )
        {
          // read single block
          error = Device_seek(&deviceHandle,block*(uint64)deviceInfo.blockSize);
          if (error != ERROR_NONE) break;
          error = Device_read(&deviceHandle,buffer+bufferBlockCount*deviceInfo.blockSize,deviceInfo.blockSize,NULL);
          if (error != ERROR_NONE) break;
        }
        else
        {
          // block not used -> store as "0"-block
          memset(buffer+bufferBlockCount*deviceInfo.blockSize,0,deviceInfo.blockSize);
        }
        bufferBlockCount++;
        block++;
      }
      if (error != ERROR_NONE) break;

      // write data to archive
      if (bufferBlockCount > 0)
      {
        error = Archive_writeData(&archiveEntryInfo,buffer,bufferBlockCount*deviceInfo.blockSize,deviceInfo.blockSize);
        if (error == ERROR_NONE)
        {
          entryDoneSize += (uint64)bufferBlockCount*(uint64)deviceInfo.blockSize;
          archiveSize   = Archive_getSize(&createInfo->archiveInfo);

          // try to set status done entry info
          if (!statusEntryDoneLocked)
          {
            statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,deviceName,deviceInfo.size);
          }

          // update status info
          SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
          {
            doneSize         = createInfo->statusInfo.doneSize+(uint64)bufferBlockCount*(uint64)deviceInfo.blockSize;
            archiveSize      = createInfo->statusInfo.storageTotalSize+archiveSize;
            compressionRatio = (!createInfo->jobOptions->dryRunFlag && (doneSize > 0))
                                 ? 100.0-(archiveSize*100.0)/doneSize
                                 : 0.0;

            createInfo->statusInfo.doneSize += (uint64)bufferBlockCount*(uint64)deviceInfo.blockSize;
            if (statusEntryDoneLocked)
            {
              String_set(createInfo->statusInfo.entryName,deviceName);
              createInfo->statusInfo.entryDoneSize  = entryDoneSize;
              createInfo->statusInfo.entryTotalSize = deviceInfo.size;
            }
            createInfo->statusInfo.doneSize         = doneSize;
            createInfo->statusInfo.archiveSize      = archiveSize;
            createInfo->statusInfo.compressionRatio = compressionRatio;
            updateStatusInfo(createInfo,TRUE);
          }
        }

        if (isPrintInfo(2))
        {
          percentageDone = 0;
          SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
          {
            percentageDone = (createInfo->statusInfo.entryTotalSize > 0LL)
                               ? (uint)((createInfo->statusInfo.entryDoneSize*100LL)/createInfo->statusInfo.entryTotalSize)
                               : 100;
          }
          printInfo(2,"%3d%%\b\b\b\b",percentageDone);
        }
      }
    }
    if (isAborted(createInfo))
    {
      printInfo(1,"ABORTED\n");
      (void)Archive_closeEntry(&archiveEntryInfo);
      if (fileSystemFlag) FileSystem_done(&fileSystemHandle);
      Device_close(&deviceHandle);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot store image entry (error: %s)!\n",
                 Error_getText(error)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      if (fileSystemFlag) FileSystem_done(&fileSystemHandle);
      Device_close(&deviceHandle);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
    printInfo(2,"    \b\b\b\b");

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive image entry (error: %s)!\n",
                 Error_getText(error)
                );
      if (fileSystemFlag) FileSystem_done(&fileSystemHandle);
      Device_close(&deviceHandle);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
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
      compressionRatio = 100.0-archiveEntryInfo.image.chunkImageData.info.size*100.0/(archiveEntryInfo.image.chunkImageData.blockCount*(uint64)deviceInfo.blockSize);
    }
    else
    {
      compressionRatio = 0.0;
    }

    if (!createInfo->jobOptions->dryRunFlag)
    {
      printInfo(1,"OK (%s, %llu bytes, ratio %.1f%%)\n",
                fileSystemFlag ? FileSystem_getName(fileSystemHandle.type) : "raw",
                deviceInfo.size,
                compressionRatio
               );
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_OK,"Added '%s'\n",String_cString(deviceName));
    }
    else
    {
      printInfo(1,"OK (%s, %llu bytes, dry-run)\n",
                fileSystemFlag ? FileSystem_getName(fileSystemHandle.type) : "raw",
                deviceInfo.size
               );
    }

  }
  else
  {
    printInfo(1,"OK (%s, %llu bytes, not stored)\n",
              fileSystemFlag ? FileSystem_getName(fileSystemHandle.type) : "raw",
              deviceInfo.size
             );
  }

  // update done entries
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    createInfo->statusInfo.doneCount++;
    updateStatusInfo(createInfo,FALSE);
  }

  // close device
  Device_close(&deviceHandle);

  // unlock status entry done info
  clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeDirectoryEntry
* Purpose: store a directory entry into archive
* Input  : createInfo    - create info structure
*          indexHandle - index handle or NULL if no index
*          directoryName - directory name to store
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeDirectoryEntry(CreateInfo  *createInfo,
                                 IndexHandle *indexHandle,
                                 ConstString directoryName
                                )
{
  bool                      statusEntryDoneLocked;
  Errors                    error;
  FileInfo                  fileInfo;
  FileExtendedAttributeList fileExtendedAttributeList;
  ArchiveEntryInfo          archiveEntryInfo;
  SemaphoreLock             semaphoreLock;

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);
  assert(directoryName != NULL);

  printInfo(1,"Add '%s'...",String_cString(directoryName));

  // get file info
  error = File_getFileInfo(directoryName,&fileInfo);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(directoryName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        createInfo->statusInfo.doneCount++;
        createInfo->statusInfo.errorEntryCount++;
      }
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get info for '%s' (error: %s)\n",
                 String_cString(directoryName),
                 Error_getText(error)
                );
      return error;
    }
  }

  // try to set status done entry info
  statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,directoryName,0LL);

  // get file extended attributes
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,directoryName);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(directoryName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        createInfo->statusInfo.doneCount++;
        createInfo->statusInfo.errorEntryCount++;
      }
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get extended attributes for '%s' (error: %s)\n",
                 String_cString(directoryName),
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
  }

  if (!createInfo->jobOptions->noStorageFlag)
  {
    // new directory
    error = Archive_newDirectoryEntry(&archiveEntryInfo,
                                      &createInfo->archiveInfo,
                                      indexHandle,
                                      directoryName,
                                      &fileInfo,
                                      &fileExtendedAttributeList
                                     );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive directory entry '%s' (error: %s)\n",
                 String_cString(directoryName),
                 Error_getText(error)
                );
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Open failed '%s'\n",String_cString(directoryName));
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive directory entry (error: %s)!\n",
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }

    if (!createInfo->jobOptions->dryRunFlag)
    {
      printInfo(1,"OK\n");
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_OK,"Added '%s'\n",String_cString(directoryName));
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

  // update done entries
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    createInfo->statusInfo.doneCount++;
    updateStatusInfo(createInfo,FALSE);
  }

  // free resources
  File_doneExtendedAttributes(&fileExtendedAttributeList);

  // add to incremental list
  if (createInfo->storeIncrementalFileInfoFlag)
  {
    addIncrementalList(&createInfo->namesDictionary,directoryName,&fileInfo);
  }

  // unlock status entry done info
  clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeLinkEntry
* Purpose: store a link entry into archive
* Input  : createInfo  - create info structure
*          indexHandle - index handle or NULL if no index
*          linkName    - link name to store
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeLinkEntry(CreateInfo  *createInfo,
                            IndexHandle *indexHandle,
                            ConstString linkName
                           )
{
  bool                      statusEntryDoneLocked;
  Errors                    error;
  FileInfo                  fileInfo;
  FileExtendedAttributeList fileExtendedAttributeList;
  String                    fileName;
  ArchiveEntryInfo          archiveEntryInfo;
  SemaphoreLock             semaphoreLock;

  assert(createInfo != NULL);
  assert(linkName != NULL);

  printInfo(1,"Add '%s'...",String_cString(linkName));

  // get file info
  error = File_getFileInfo(linkName,&fileInfo);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Alccess denied '%s' (error: %s)\n",String_cString(linkName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        createInfo->statusInfo.doneCount++;
        createInfo->statusInfo.errorEntryCount++;
      }
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get info for '%s' (error: %s)\n",
                 String_cString(linkName),
                 Error_getText(error)
                );
      return error;
    }
  }

  // try to set status done entry info
  statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,linkName,0LL);

  // get file extended attributes
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,linkName);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(linkName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        createInfo->statusInfo.doneCount++;
        createInfo->statusInfo.errorEntryCount++;
      }
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get extended attributes for '%s' (error: %s)\n",
                 String_cString(linkName),
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
  }

  if (!createInfo->jobOptions->noStorageFlag)
  {
    // read link
    fileName = String_new();
    error = File_readLink(fileName,linkName);
    if (error != ERROR_NONE)
    {
      if (createInfo->jobOptions->skipUnreadableFlag)
      {
        printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
        logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Open failed '%s'\n",String_cString(linkName));
        SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          createInfo->statusInfo.doneCount++;
          createInfo->statusInfo.doneSize += (uint64)fileInfo.size;
          createInfo->statusInfo.errorEntryCount++;
          createInfo->statusInfo.errorEntrySize += (uint64)fileInfo.size;
        }
        String_delete(fileName);
        File_doneExtendedAttributes(&fileExtendedAttributeList);
        clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
        return ERROR_NONE;
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError("Cannot read link '%s' (error: %s)\n",
                   String_cString(linkName),
                   Error_getText(error)
                  );
        String_delete(fileName);
        File_doneExtendedAttributes(&fileExtendedAttributeList);
        clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
        return error;
      }
    }

    // new link
    error = Archive_newLinkEntry(&archiveEntryInfo,
                                 &createInfo->archiveInfo,
                                 indexHandle,
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
      String_delete(fileName);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive link entry (error: %s)!\n",
                 Error_getText(error)
                );
      String_delete(fileName);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }

    if (!createInfo->jobOptions->dryRunFlag)
    {
      printInfo(1,"OK\n");
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_OK,"Added '%s'\n",String_cString(linkName));
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

  // update done entries
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    createInfo->statusInfo.doneCount++;
    updateStatusInfo(createInfo,FALSE);
  }

  // free resources
  File_doneExtendedAttributes(&fileExtendedAttributeList);

  // add to incremental list
  if (createInfo->storeIncrementalFileInfoFlag)
  {
    addIncrementalList(&createInfo->namesDictionary,linkName,&fileInfo);
  }

  // unlock status entry done info
  clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeHardLinkEntry
* Purpose: store a hard link entry into archive
* Input  : createInfo  - create info structure
*          indexHandle - index handle or NULL if no index
*          nameList    - hard link name list to store
*          buffer      - buffer for temporary data
*          bufferSize  - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeHardLinkEntry(CreateInfo       *createInfo,
                                IndexHandle      *indexHandle,
                                const StringList *nameList,
                                byte             *buffer,
                                uint             bufferSize
                               )
{
  bool                      statusEntryDoneLocked;
  Errors                    error;
  FileInfo                  fileInfo;
  FileExtendedAttributeList fileExtendedAttributeList;
  FileHandle                fileHandle;
  bool                      byteCompressFlag;
  bool                      deltaCompressFlag;
  ArchiveEntryInfo          archiveEntryInfo;
  SemaphoreLock             semaphoreLock;
  uint64                    entryDoneSize;
  ulong                     bufferLength;
  uint64                    archiveSize;
  uint64                    doneSize;
  double                    compressionRatio;
  uint                      percentageDone;
  const StringNode          *stringNode;
  String                    name;

  assert(createInfo != NULL);
  assert(nameList != NULL);
  assert(!StringList_isEmpty(nameList));
  assert(buffer != NULL);

  printInfo(1,"Add '%s'...",String_cString(StringList_first(nameList,NULL)));

  // get file info
  error = File_getFileInfo(StringList_first(nameList,NULL),&fileInfo);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(StringList_first(nameList,NULL)),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        createInfo->statusInfo.doneCount += StringList_count(nameList);
        createInfo->statusInfo.errorEntryCount += StringList_count(nameList);
      }
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get info for '%s' (error: %s)\n",
                 String_cString(StringList_first(nameList,NULL)),
                 Error_getText(error)
                );
      return error;
    }
  }

  // try to set status done entry info
  statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,StringList_first(nameList,NULL),fileInfo.size);

  // get file extended attributes
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,StringList_first(nameList,NULL));
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(StringList_first(nameList,NULL)),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        createInfo->statusInfo.doneCount++;
        createInfo->statusInfo.errorEntryCount++;
      }
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get extended attributes for '%s' (error: %s)\n",
                 String_cString(StringList_first(nameList,NULL)),
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
  }

  // open file
  error = File_open(&fileHandle,StringList_first(nameList,NULL),FILE_OPEN_READ|FILE_OPEN_NO_ATIME|FILE_OPEN_NO_CACHE);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Open file failed '%s' (error: %s)\n",String_cString(StringList_first(nameList,NULL)),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        createInfo->statusInfo.doneCount += StringList_count(nameList);
        createInfo->statusInfo.doneSize += (uint64)StringList_count(nameList)*(uint64)fileInfo.size;
        createInfo->statusInfo.errorEntryCount += StringList_count(nameList);
        createInfo->statusInfo.errorEntrySize += (uint64)StringList_count(nameList)*(uint64)fileInfo.size;
      }
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot open file '%s' (error: %s)\n",
                 String_cString(StringList_first(nameList,NULL)),
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
  }

  if (!createInfo->jobOptions->noStorageFlag)
  {
    // check if file data should be byte compressed
    byteCompressFlag =    (fileInfo.size > globalOptions.compressMinFileSize)
                       && !PatternList_matchStringList(createInfo->compressExcludePatternList,nameList,PATTERN_MATCH_MODE_EXACT);

    // check if file data should be delta compressed
    deltaCompressFlag =    (fileInfo.size > globalOptions.compressMinFileSize)
                        && Compress_isCompressed(createInfo->jobOptions->compressAlgorithms.delta);

    // create new archive hard link entry
    error = Archive_newHardLinkEntry(&archiveEntryInfo,
                                     &createInfo->archiveInfo,
                                     indexHandle,
                                     nameList,
                                     &fileInfo,
                                     &fileExtendedAttributeList,
                                     deltaCompressFlag,
                                     byteCompressFlag
                                    );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive hardlink entry '%s' (error: %s)\n",
                 String_cString(StringList_first(nameList,NULL)),
                 Error_getText(error)
                );
      (void)File_close(&fileHandle);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }

    // write hard link content to archive
    error         = ERROR_NONE;
    entryDoneSize = 0LL;
    do
    {
      // pause
      pauseCreate(createInfo);

      // read file data
      error = File_read(&fileHandle,buffer,bufferSize,&bufferLength);
      if (error == ERROR_NONE)
      {
        // write data to archive
        if (bufferLength > 0L)
        {
          error = Archive_writeData(&archiveEntryInfo,buffer,bufferLength,1);
          if (error == ERROR_NONE)
          {
            entryDoneSize += (uint64)StringList_count(nameList)*(uint64)bufferLength;
            archiveSize = Archive_getSize(&createInfo->archiveInfo);

            // try to set status done entry info
            if (!statusEntryDoneLocked)
            {
              statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,StringList_first(nameList,NULL),fileInfo.size);
            }

            SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
            {
              doneSize         = createInfo->statusInfo.doneSize+(uint64)bufferLength;
              archiveSize      = createInfo->statusInfo.storageTotalSize+archiveSize;
              compressionRatio = (!createInfo->jobOptions->dryRunFlag && (doneSize > 0))
                                   ? 100.0-(archiveSize*100.0)/doneSize
                                   : 0.0;

              createInfo->statusInfo.doneSize += (uint64)StringList_count(nameList)*(uint64)bufferLength;
              if (statusEntryDoneLocked)
              {
                String_set(createInfo->statusInfo.entryName,StringList_first(nameList,NULL));
                createInfo->statusInfo.entryDoneSize  = entryDoneSize;
                createInfo->statusInfo.entryTotalSize = fileInfo.size;
              }
              createInfo->statusInfo.doneSize         = doneSize;
              createInfo->statusInfo.archiveSize      = archiveSize;
              createInfo->statusInfo.compressionRatio = compressionRatio;
              updateStatusInfo(createInfo,TRUE);
            }
          }

          if (isPrintInfo(2))
          {
            percentageDone = 0;
            SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
            {
              percentageDone = (createInfo->statusInfo.entryTotalSize > 0LL)
                                 ? (uint)((createInfo->statusInfo.entryDoneSize*100LL)/createInfo->statusInfo.entryTotalSize)
                                 : 100;
            }
            printInfo(2,"%3d%%\b\b\b\b",percentageDone);
          }
        }
      }
    }
    while (   (bufferLength > 0L)
           && (error == ERROR_NONE)
           && (createInfo->failError == ERROR_NONE)
           && !isAborted(createInfo)
          );
    if (isAborted(createInfo))
    {
      printInfo(1,"ABORTED\n");
      (void)Archive_closeEntry(&archiveEntryInfo);
      (void)File_close(&fileHandle);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot store hardlink entry (error: %s)!\n",
                 Error_getText(error)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      (void)File_close(&fileHandle);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
    printInfo(2,"    \b\b\b\b");

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive hardlink entry (error: %s)!\n",
                 Error_getText(error)
                );
      (void)File_close(&fileHandle);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
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

    if (!createInfo->jobOptions->dryRunFlag)
    {
      printInfo(1,"OK (%llu bytes, ratio %.1f%%)\n",
                fileInfo.size,
                compressionRatio
               );
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_OK,"Added '%s'\n",String_cString(StringList_first(nameList,NULL)));
    }
    else
    {
      printInfo(1,"OK (%llu bytes, dry-run)\n",fileInfo.size);
    }
  }
  else
  {
    printInfo(1,"OK (%llu bytes, not stored)\n",fileInfo.size);
  }

  // update done entries
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    createInfo->statusInfo.doneCount += StringList_count(nameList);
    updateStatusInfo(createInfo,FALSE);
  }

  // close file
  (void)File_close(&fileHandle);

  // free resources
  File_doneExtendedAttributes(&fileExtendedAttributeList);

  // add to incremental list
  if (createInfo->storeIncrementalFileInfoFlag)
  {
    STRINGLIST_ITERATE(nameList,stringNode,name)
    {
      addIncrementalList(&createInfo->namesDictionary,name,&fileInfo);
    }
  }

  // unlock status entry done info
  clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeSpecialEntry
* Purpose: store a special entry into archive
* Input  : createInfo  - create info structure
*          indexHandle - index handle or NULL if no index
*          fileName    - file name to store
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeSpecialEntry(CreateInfo  *createInfo,
                               IndexHandle *indexHandle,
                               ConstString fileName
                              )
{
  bool                      statusEntryDoneLocked;
  Errors                    error;
  FileInfo                  fileInfo;
  FileExtendedAttributeList fileExtendedAttributeList;
  ArchiveEntryInfo          archiveEntryInfo;
  SemaphoreLock             semaphoreLock;

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);
  assert(fileName != NULL);

  printInfo(1,"Add '%s'...",String_cString(fileName));

  // get file info, file extended attributes
  error = File_getFileInfo(fileName,&fileInfo);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(fileName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        createInfo->statusInfo.doneCount++;
        createInfo->statusInfo.errorEntryCount++;
      }
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get info for '%s' (error: %s)\n",
                 String_cString(fileName),
                 Error_getText(error)
                );
      return error;
    }
  }

  // try to set status done entry info
  statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,fileName,0LL);

  // get file extended attributes
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,fileName);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(fileName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        createInfo->statusInfo.doneCount++;
        createInfo->statusInfo.errorEntryCount++;
      }
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get extended attributes for '%s' (error: %s)\n",
                 String_cString(fileName),
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
  }

  if (!createInfo->jobOptions->noStorageFlag)
  {
    // new special
    error = Archive_newSpecialEntry(&archiveEntryInfo,
                                    &createInfo->archiveInfo,
                                    indexHandle,
                                    fileName,
                                    &fileInfo,
                                    &fileExtendedAttributeList
                                   );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive special entry '%s' (error: %s)\n",
                 String_cString(fileName),
                 Error_getText(error)
                );
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive special entry (error: %s)!\n",
                 Error_getText(error)
                );
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }

    if (!createInfo->jobOptions->dryRunFlag)
    {
      printInfo(1,"OK\n");
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_OK,"Added '%s'\n",String_cString(fileName));
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

  // update done entries
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    createInfo->statusInfo.doneCount++;
    updateStatusInfo(createInfo,FALSE);
  }

  // free resources
  File_doneExtendedAttributes(&fileExtendedAttributeList);

  // add to incremental list
  if (createInfo->storeIncrementalFileInfoFlag)
  {
    addIncrementalList(&createInfo->namesDictionary,fileName,&fileInfo);
  }

  // unlock status entry done info
  clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);

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
  SemaphoreLock    semaphoreLock;

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
    // pause
    pauseCreate(createInfo);

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
      switch (entryMsg.fileType)
      {
        case FILE_TYPE_FILE:
          switch (entryMsg.entryType)
          {
            case ENTRY_TYPE_FILE:
              error = storeFileEntry(createInfo,
                                     createInfo->indexHandle,
                                     entryMsg.name,
                                     buffer,
                                     BUFFER_SIZE
                                    );
              if (error != ERROR_NONE) createInfo->failError = error;
              break;
            case ENTRY_TYPE_IMAGE:
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
                                          createInfo->indexHandle,
                                          entryMsg.name
                                         );
              if (error != ERROR_NONE) createInfo->failError = error;
              break;
            case ENTRY_TYPE_IMAGE:
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
                                     createInfo->indexHandle,
                                     entryMsg.name
                                    );
              if (error != ERROR_NONE) createInfo->failError = error;
              break;
            case ENTRY_TYPE_IMAGE:
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
                                         createInfo->indexHandle,
                                         &entryMsg.nameList,
                                         buffer,
                                         BUFFER_SIZE
                                        );
              if (error != ERROR_NONE) createInfo->failError = error;
              break;
            case ENTRY_TYPE_IMAGE:
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
                                        createInfo->indexHandle,
                                        entryMsg.name
                                       );
              if (error != ERROR_NONE) createInfo->failError = error;
              break;
            case ENTRY_TYPE_IMAGE:
              error = storeImageEntry(createInfo,
                                      createInfo->indexHandle,
                                      entryMsg.name,
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

      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        createInfo->statusInfo.skippedEntryCount++;
        updateStatusInfo(createInfo,FALSE);
      }
    }

    // update status info and check if aborted
    SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      updateStatusInfo(createInfo,FALSE);
    }

    // free entry message
    freeEntryMsg(&entryMsg,NULL);

// NYI: is this really useful? (avoid that sum-collector-thread is slower than file-collector-thread)
    // slow down if too fast
    while (   !createInfo->collectorSumThreadExitedFlag
           && (createInfo->statusInfo.doneCount >= createInfo->statusInfo.totalEntryCount)
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

Errors Command_create(ConstString                  jobUUID,
//                      ConstString                  hostName,
//                      uint                         hostPort,
                      ConstString                  scheduleUUID,
                      ConstString                  storageName,
                      const EntryList              *includeEntryList,
                      const PatternList            *excludePatternList,
                      MountList                    *mountList,
                      const PatternList            *compressExcludePatternList,
                      DeltaSourceList              *deltaSourceList,
                      JobOptions                   *jobOptions,
                      ArchiveTypes                 archiveType,
                      ConstString                  scheduleTitle,
                      ConstString                  scheduleCustomText,
                      GetPasswordFunction          getPasswordFunction,
                      void                         *getPasswordUserData,
                      CreateStatusInfoFunction     createStatusInfoFunction,
                      void                         *createStatusInfoUserData,
                      StorageRequestVolumeFunction storageRequestVolumeFunction,
                      void                         *storageRequestVolumeUserData,
                      bool                         *pauseCreateFlag,
                      bool                         *pauseStorageFlag,
                      bool                         *requestedAbortFlag,
                      LogHandle                    *logHandle
                     )
{
  AutoFreeList     autoFreeList;
  String           incrementalListFileName;
  bool             useIncrementalFileInfoFlag;
  bool             incrementalFileInfoExistFlag;
  IndexHandle      *indexHandle;
  StorageSpecifier storageSpecifier;
  CreateInfo       createInfo;
  IndexId          uuidId;
  IndexId          entityId;
  Thread           collectorSumThread;                 // files collector sum thread
  Thread           collectorThread;                    // files collector thread
  Thread           storageThread;                      // storage thread
  Thread           *createThreads;
  uint             createThreadCount;
  uint             i;
  Errors           error;
  MountNode        *mountNode;

  assert(storageName != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);

  // init variables
  AutoFree_init(&autoFreeList);
  incrementalListFileName      = NULL;
  useIncrementalFileInfoFlag   = FALSE;
  incrementalFileInfoExistFlag = FALSE;

  // check if storage name given
  if (String_isEmpty(storageName))
  {
    printError("No storage name given\n");
    AutoFree_cleanup(&autoFreeList);
    return ERROR_NO_STORAGE_NAME;
  }

  // parse storage name
  Storage_initSpecifier(&storageSpecifier);
  error = Storage_parseName(&storageSpecifier,storageName);
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize storage '%s' (error: %s)\n",
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
  indexHandle = Index_open(INDEX_PRIORITY_HIGH,INDEX_TIMEOUT);
  AUTOFREE_ADD(&autoFreeList,indexHandle,{ Index_close(indexHandle); });

  // init create info
  initCreateInfo(&createInfo,
                 &storageSpecifier,
                 indexHandle,
                 jobUUID,
                 scheduleUUID,
                 includeEntryList,
                 excludePatternList,
                 compressExcludePatternList,
                 deltaSourceList,
                 jobOptions,
                 archiveType,
                 scheduleTitle,
                 scheduleCustomText,
                 CALLBACK(createStatusInfoFunction,createStatusInfoUserData),
                 pauseCreateFlag,
                 pauseStorageFlag,
                 requestedAbortFlag,
                 logHandle
                );
  AUTOFREE_ADD(&autoFreeList,&createInfo,{ doneCreateInfo(&createInfo); });

  // init threads
  createThreadCount = (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores();
  createThreads = (Thread*)malloc(createThreadCount*sizeof(Thread));
  if (createThreads == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,createThreads,{ free(createThreads); });

  // mount devices
  LIST_ITERATE(mountList,mountNode)
  {
    mountNode->mounted = FALSE;
    if (!Device_isMounted(mountNode->name))
    {
      error = Device_mount(mountNode->name);
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      mountNode->mounted = TRUE;
    }
    AUTOFREE_ADDX(&autoFreeList,mountNode,(MountNode *mountNode),{ if (Device_isMounted(mountNode->name) && (mountNode->alwaysUnmount || mountNode->mounted)) Device_umount(mountNode->name); });
  }

  // init storage
  error = Storage_init(&createInfo.storage,
                       createInfo.storageSpecifier,
                       createInfo.jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK(updateStorageStatusInfo,&createInfo),
                       CALLBACK(getPasswordFunction,getPasswordUserData),
                       CALLBACK(storageRequestVolumeFunction,storageRequestVolumeUserData)
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize storage '%s' (error: %s)\n",
               Storage_getPrintableNameCString(createInfo.storageSpecifier,NULL),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Storage_done(&createInfo.storage); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&createInfo.storage,{ Storage_done(&createInfo.storage); });

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
    }
    else
    {
      formatIncrementalFileName(incrementalListFileName,
                                createInfo.storageSpecifier
                               );
    }
    Dictionary_init(&createInfo.namesDictionary,DICTIONARY_BYTE_COPY,CALLBACK_NULL,CALLBACK_NULL);
    AUTOFREE_ADD(&autoFreeList,incrementalListFileName,{ String_delete(incrementalListFileName); });
    AUTOFREE_ADD(&autoFreeList,&createInfo.namesDictionary,{ Dictionary_done(&createInfo.namesDictionary); });

    // read incremental list
    incrementalFileInfoExistFlag = File_exists(incrementalListFileName);
    if (   (   (createInfo.archiveType == ARCHIVE_TYPE_INCREMENTAL )
            || (createInfo.archiveType == ARCHIVE_TYPE_DIFFERENTIAL)
           )
        && incrementalFileInfoExistFlag
       )
    {
      printInfo(1,"Read incremental list '%s'...",String_cString(incrementalListFileName));
      error = readIncrementalList(incrementalListFileName,
                                  &createInfo.namesDictionary
                                 );
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("Cannot read incremental list file '%s' (error: %s)\n",
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
    if (!Index_findUUIDByJobUUID(indexHandle,
                                 jobUUID,
                                 NULL,  // scheduleUUID
                                 &uuidId,
                                 NULL,  // lastCreatedDateTime,
                                 NULL,  // lastErrorMessage,
                                 NULL,  // executionCount,
                                 NULL,  // averageDuration,
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
        printError("Cannot create index for '%s' (error: %s)!\n",
                   Storage_getPrintableNameCString(createInfo.storageSpecifier,NULL),
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
                            archiveType,
                            0LL, // createdDateTime
                            TRUE,  // locked
                            &entityId
                           );
    if (error != ERROR_NONE)
    {
      printError("Cannot create index for '%s' (error: %s)!\n",
                 Storage_getPrintableNameCString(createInfo.storageSpecifier,NULL),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    assert(entityId != INDEX_ID_NONE);
    DEBUG_TESTCODE() { Index_deleteEntity(indexHandle,entityId); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
    AUTOFREE_ADD(&autoFreeList,&entityId,{ Index_deleteEntity(indexHandle,entityId); });

    // start index database transaction
    error = Index_beginTransaction(indexHandle,INDEX_TIMEOUT);
    if (error != ERROR_NONE)
    {
      printError("Cannot create index for '%s' (error: %s)!\n",
                 Storage_getPrintableNameCString(createInfo.storageSpecifier,NULL),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,indexHandle,{ Index_rollbackTransaction(indexHandle); });
  }

  // create new archive
  error = Archive_create(&createInfo.archiveInfo,
                         uuidId,
                         jobUUID,
                         scheduleUUID,
                         deltaSourceList,
                         jobOptions,
                         indexHandle,
                         entityId,
                         archiveType,
                         CALLBACK(NULL,NULL),  // archiveInitFunction
                         CALLBACK(NULL,NULL),  // archiveDoneFunction
                         CALLBACK(archiveGetSize,&createInfo),
                         CALLBACK(archiveStore,&createInfo),
                         CALLBACK(getPasswordFunction,getPasswordUserData),
                         logHandle
                        );
  if (error != ERROR_NONE)
  {
    printError("Cannot create archive file '%s' (error: %s)\n",
               Storage_getPrintableNameCString(createInfo.storageSpecifier,NULL),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Archive_close(&createInfo.archiveInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&createInfo.archiveInfo,{ Archive_close(&createInfo.archiveInfo); });

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
  for (i = 0; i < createThreadCount; i++)
  {
    if (!Thread_init(&createThreads[i],"BAR create",globalOptions.niceLevel,createThreadCode,&createInfo))
    {
      HALT_FATAL_ERROR("Cannot initialize create thread!");
    }
  }

  // wait for collector threads
  if (!Thread_join(&collectorSumThread))
  {
    HALT_INTERNAL_ERROR("Cannot stop collector sum thread!");
  }
  if (!Thread_join(&collectorThread))
  {
    HALT_INTERNAL_ERROR("Cannot stop collector thread!");
  }

  // wait for and done create threads
  MsgQueue_setEndOfMsg(&createInfo.entryMsgQueue);
  for (i = 0; i < createThreadCount; i++)
  {
    if (!Thread_join(&createThreads[i]))
    {
      HALT_FATAL_ERROR("Cannot stop create thread!");
    }
    Thread_done(&createThreads[i]);
  }

  // close archive
  AUTOFREE_REMOVE(&autoFreeList,&createInfo.archiveInfo);
  error = Archive_close(&createInfo.archiveInfo);
  if (error != ERROR_NONE)
  {
    printError("Cannot close archive '%s' (error: %s)\n",
               Storage_getPrintableNameCString(createInfo.storageSpecifier,NULL),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  // wait for storage thread
  MsgQueue_setEndOfMsg(&createInfo.storageMsgQueue);
  if (!Thread_join(&storageThread))
  {
    HALT_INTERNAL_ERROR("Cannot stop storage thread!");
  }

  // final update of status info
  (void)updateStatusInfo(&createInfo,TRUE);

  // update index
  if (indexHandle != NULL)
  {
    assert(entityId != INDEX_ID_NONE);

    // unlock entity
    (void)Index_unlockEntity(indexHandle,entityId);

    // delete entity if nothing created
    error = Index_pruneEntity(indexHandle,entityId);
    if (error != ERROR_NONE)
    {
      printError("Cannot create index for '%s' (error: %s)!\n",
                 Storage_getPrintableNameCString(createInfo.storageSpecifier,NULL),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // end index database transaction
    AUTOFREE_REMOVE(&autoFreeList,indexHandle);
    error = Index_endTransaction(indexHandle);
    if (error != ERROR_NONE)
    {
      printError("Cannot create index for '%s' (error: %s)!\n",
                 Storage_getPrintableNameCString(createInfo.storageSpecifier,NULL),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
  }

  // write incremental list
  if (   createInfo.storeIncrementalFileInfoFlag
      && (createInfo.failError == ERROR_NONE)
      && !isAborted(&createInfo)
      && !jobOptions->dryRunFlag
     )
  {
    printInfo(1,"Write incremental list '%s'...",String_cString(incrementalListFileName));
    error = writeIncrementalList(incrementalListFileName,
                                 &createInfo.namesDictionary
                                );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot write incremental list file '%s' (error: %s)\n",
                 String_cString(incrementalListFileName),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

    printInfo(1,"OK\n");
    logMessage(logHandle,LOG_TYPE_ALWAYS,"Updated incremental file '%s'\n",String_cString(incrementalListFileName));
  }

  // unmount devices
  LIST_ITERATE(mountList,mountNode)
  {
    if (Device_isMounted(mountNode->name) && (mountNode->alwaysUnmount || mountNode->mounted))
    {
      error = Device_umount(mountNode->name);
      if (error != ERROR_NONE)
      {
        printWarning("Cannot unmount '%s' (error: %s)\n",String_cString(mountNode->name),Error_getText(error));
      }
    }
    AUTOFREE_REMOVE(&autoFreeList,mountNode);
  }

  // output statics
  if (createInfo.failError == ERROR_NONE)
  {
    printInfo(1,
              "%lu entries/%.1lf%s (%llu bytes) included\n",
              createInfo.statusInfo.doneCount,
              BYTES_SHORT(createInfo.statusInfo.doneSize),
              BYTES_UNIT(createInfo.statusInfo.doneSize),
              createInfo.statusInfo.doneSize
             );
    printInfo(2,
              "%lu entries/%.1lf%s (%llu bytes) skipped\n",
              createInfo.statusInfo.skippedEntryCount,
              BYTES_SHORT(createInfo.statusInfo.skippedEntrySize),
              BYTES_UNIT(createInfo.statusInfo.skippedEntrySize),
              createInfo.statusInfo.skippedEntrySize
             );
    printInfo(2,
              "%lu entries/%.1lf%s (%llu bytes) with errors\n",
              createInfo.statusInfo.errorEntryCount,
              BYTES_SHORT(createInfo.statusInfo.errorEntrySize),
              BYTES_UNIT(createInfo.statusInfo.errorEntrySize),
              createInfo.statusInfo.errorEntrySize
             );
    logMessage(logHandle,
               LOG_TYPE_ALWAYS,
               "%lu entries/%.1lf%s (%llu bytes) included, %lu entries skipped, %lu entries with errors\n",
               createInfo.statusInfo.doneCount,
               BYTES_SHORT(createInfo.statusInfo.doneSize),
               BYTES_UNIT(createInfo.statusInfo.doneSize),
               createInfo.statusInfo.doneSize,
               createInfo.statusInfo.skippedEntryCount,
               createInfo.statusInfo.errorEntryCount
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
  Thread_done(&collectorSumThread);
  Thread_done(&collectorThread);
  Thread_done(&storageThread);
  if (useIncrementalFileInfoFlag)
  {
    String_delete(incrementalListFileName);
    Dictionary_done(&createInfo.namesDictionary);
  }
  Storage_done(&createInfo.storage);
  doneCreateInfo(&createInfo);
  Index_close(indexHandle);
  free(createThreads);
  Storage_doneSpecifier(&storageSpecifier);
  AutoFree_done(&autoFreeList);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
