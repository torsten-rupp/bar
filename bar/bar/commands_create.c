/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive create function
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

#include "commands_create.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define MAX_FILE_MSG_QUEUE_ENTRIES    256
#define MAX_STORAGE_MSG_QUEUE_ENTRIES 256

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
  StorageHandle               storageHandle;                      // storage handle
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

  CreateStatusInfoFunction    statusInfoFunction;                 // status info call back
  void                        *statusInfoUserData;                // user data for status info call back
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
  DatabaseId  entityId;                                           // database entity id
  DatabaseId  storageId;                                          // database storage id
  String      fileName;                                           // intermediate archive filename
  uint64      entries;                                            // number of entries in archive
  uint64      size;                                               // archive size [bytes]
  String      archiveName;                                        // destination archive name
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

  statusInfo->doneEntries         = 0L;
  statusInfo->doneBytes           = 0LL;
  statusInfo->totalEntries        = 0L;
  statusInfo->totalBytes          = 0LL;
  statusInfo->collectTotalSumDone = FALSE;
  statusInfo->skippedEntries      = 0L;
  statusInfo->skippedBytes        = 0LL;
  statusInfo->errorEntries        = 0L;
  statusInfo->errorBytes          = 0LL;
  statusInfo->archiveBytes        = 0LL;
  statusInfo->compressionRatio    = 0.0;
  statusInfo->name                = String_new();
  statusInfo->entryDoneBytes      = 0LL;
  statusInfo->entryTotalBytes     = 0LL;
  statusInfo->storageName         = String_new();
  statusInfo->storageDoneBytes    = 0LL;
  statusInfo->storageTotalBytes   = 0LL;
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
                          ConstString              jobUUID,
                          ConstString              scheduleUUID,
                          const EntryList          *includeEntryList,
                          const PatternList        *excludePatternList,
                          const PatternList        *compressExcludePatternList,
                          const DeltaSourceList    *deltaSourceList,
                          JobOptions               *jobOptions,
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
  createInfo->statusInfoFunction             = createStatusInfoFunction;
  createInfo->statusInfoUserData             = createStatusInfoUserData;
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
  if (!MsgQueue_init(&createInfo->entryMsgQueue,MAX_FILE_MSG_QUEUE_ENTRIES))
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

  DEBUG_REMOVE_RESOURCE_TRACE(createInfo,sizeof(createInfo));

  Semaphore_done(&createInfo->statusInfoNameLock);
  Semaphore_done(&createInfo->statusInfoLock);
  Semaphore_done(&createInfo->storageInfoLock);

  MsgQueue_done(&createInfo->storageMsgQueue,(MsgQueueMsgFreeFunction)freeStorageMsg,NULL);
  MsgQueue_done(&createInfo->entryMsgQueue,(MsgQueueMsgFreeFunction)freeEntryMsg,NULL);

  String_delete(createInfo->statusInfo.storageName);
  String_delete(createInfo->statusInfo.name);
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
                   sizeof(incrementalListInfo),
                   DICTIONARY_BYTE_COPY
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
      error = ERRORX_(NOT_A_DIRECTORY,0,String_cString(directory));
      String_delete(directory);
      return error;
    }
  }

  // get temporary name for new .bid file
  tmpFileName = String_new();
  error = File_getTmpFileName(tmpFileName,NULL,directory);
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
    Misc_udelay(500LL*MISC_US_PER_MS);
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
    Misc_udelay(500LL*MISC_US_PER_MS);
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
                 sizeof(incrementalListInfo),
                 DICTIONARY_BYTE_COPY
                );
}

/***********************************************************************\
* Name   : updateStatusInfo
* Purpose: update status info
* Input  : createInfo - create info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateStatusInfo(CreateInfo *createInfo)
{
  assert(createInfo != NULL);

  if (createInfo->statusInfoFunction != NULL)
  {
    createInfo->statusInfoFunction(createInfo->statusInfoUserData,
                                   createInfo->failError,
                                   &createInfo->statusInfo
                                  );
  }
}

/***********************************************************************\
* Name   : updateStorageStatusInfo
* Purpose: update storage info data
* Input  : createInfo        - create info
*          storageStatusInfo - storage status info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateStorageStatusInfo(CreateInfo              *createInfo,
                                   const StorageStatusInfo *storageStatusInfo
                                  )
{
  SemaphoreLock semaphoreLock;

  assert(createInfo != NULL);
  assert(storageStatusInfo != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    createInfo->statusInfo.volumeNumber   = storageStatusInfo->volumeNumber;
    createInfo->statusInfo.volumeProgress = storageStatusInfo->volumeProgress;
    updateStatusInfo(createInfo);
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
* Input  : fileName              - file name variable
*          formatMode            - format mode; see FORMAT_MODE_*
*          templateFileName      - template file name
*          archiveType           - archive type; see ARCHIVE_TYPE_*
*          scheduleTitle         - schedule title or NULL
*          scheduleCustomText    - schedule custom text or NULL
*          time                  - time
*          partNumber            - part number (>=0 for parts,
*                                  ARCHIVE_PART_NUMBER_NONE for single
*                                  part archive)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors formatArchiveFileName(String       fileName,
                                   FormatModes  formatMode,
                                   ConstString  templateFileName,
                                   ArchiveTypes archiveType,
                                   ConstString  scheduleTitle,
                                   ConstString  scheduleCustomText,
                                   time_t       time,
                                   int          partNumber
                                  )
{
  TextMacro textMacros[10];

  StaticString (uuid,MISC_UUID_STRING_LENGTH);
  bool         partNumberFlag;
  #ifdef HAVE_LOCALTIME_R
    struct tm tmBuffer;
  #endif /* HAVE_LOCALTIME_R */
  struct tm    *tm;
  uint         weekNumberU,weekNumberW;
  ulong        i,j;
  char         format[4];
  char         buffer[256];
  size_t       length;
  ulong        divisor;
  ulong        n;
  uint         z;
  int          d;

  // init variables
  Misc_getUUID(uuid);
  #ifdef HAVE_LOCALTIME_R
    tm = localtime_r(&time,&tmBuffer);
  #else /* not HAVE_LOCALTIME_R */
    tm = localtime(&time);
  #endif /* HAVE_LOCALTIME_R */
  assert(tm != NULL);
  strftime(buffer,sizeof(buffer)-1,"%U",tm); buffer[sizeof(buffer)-1] = '\0';
  weekNumberU = (uint)atoi(buffer);
  strftime(buffer,sizeof(buffer)-1,"%W",tm); buffer[sizeof(buffer)-1] = '\0';
  weekNumberW = (uint)atoi(buffer);

  // expand named macros
  switch (formatMode)
  {
    case FORMAT_MODE_ARCHIVE_FILE_NAME:
      TEXT_MACRO_N_CSTRING(textMacros[0],"%type", getArchiveTypeName(archiveType)     );
      TEXT_MACRO_N_CSTRING(textMacros[1],"%T",    getArchiveTypeShortName(archiveType));
      TEXT_MACRO_N_CSTRING(textMacros[3],"%uuid", String_cString(uuid));
      TEXT_MACRO_N_CSTRING(textMacros[4],"%title",(scheduleTitle != NULL) ? String_cString(scheduleTitle) : "");
      TEXT_MACRO_N_CSTRING(textMacros[5],"%text", (scheduleCustomText != NULL) ? String_cString(scheduleCustomText) : "");
      TEXT_MACRO_N_INTEGER(textMacros[6],"%U2",   (weekNumberU%2)+1);
      TEXT_MACRO_N_INTEGER(textMacros[7],"%U4",   (weekNumberU%4)+1);
      TEXT_MACRO_N_INTEGER(textMacros[8],"%W2",   (weekNumberW%2)+1);
      TEXT_MACRO_N_INTEGER(textMacros[9],"%W4",   (weekNumberW%4)+1);
      Misc_expandMacros(fileName,String_cString(templateFileName),textMacros,SIZE_OF_ARRAY(textMacros));
      break;
    case FORMAT_MODE_PATTERN:
      TEXT_MACRO_N_CSTRING(textMacros[0],"%type", "\\S+");
      TEXT_MACRO_N_CSTRING(textMacros[1],"%T",    ".");
      TEXT_MACRO_N_CSTRING(textMacros[3],"%uuid", "[-0-9a-fA-F]+");
      TEXT_MACRO_N_CSTRING(textMacros[4],"%title","\\S+");
      TEXT_MACRO_N_CSTRING(textMacros[5],"%text", "\\S+");
      TEXT_MACRO_N_CSTRING(textMacros[6],"%U2",   "[12]");
      TEXT_MACRO_N_CSTRING(textMacros[7],"%U4",   "[1234]");
      TEXT_MACRO_N_CSTRING(textMacros[8],"%W2",   "[12]");
      TEXT_MACRO_N_CSTRING(textMacros[9],"%W4",   "[1234]");
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
      #endif /* NDEBUG */
  }
  Misc_expandMacros(fileName,String_cString(templateFileName),textMacros,SIZE_OF_ARRAY(textMacros));

  // expand date/time macros, part number
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
              // %% -> %
              String_remove(fileName,i,1);
              i += 1L;
              break;
            case '#':
              // %# -> #
              String_remove(fileName,i,1);
              i += 1L;
              break;
            default:
              // format date/time part
              switch (String_index(fileName,i+1))
              {
                case 'E':
                case 'O':
                  // %Ex, %Ox: extended date/time macros
                  format[0] = '%';
                  format[1] = String_index(fileName,i+1);
                  format[2] = String_index(fileName,i+2);
                  format[3] = '\0';

                  String_remove(fileName,i,3);
                  break;
                default:
                  // %x: date/time macros
                  format[0] = '%';
                  format[1] = String_index(fileName,i+1);
                  format[2] = '\0';

                  String_remove(fileName,i,2);
                  break;
              }
              length = strftime(buffer,sizeof(buffer)-1,format,tm); buffer[sizeof(buffer)-1] = '\0';

              // insert into string
              switch (formatMode)
              {
                case FORMAT_MODE_ARCHIVE_FILE_NAME:
                  String_insertBuffer(fileName,i,buffer,length);
                  i += length;
                  break;
                case FORMAT_MODE_PATTERN:
                  for (z = 0 ; z < length; z++)
                  {
                    if (strchr("*+?{}():[].^$|",buffer[z]) != NULL)
                    {
                      String_insertChar(fileName,i,'\\');
                      i += 1L;
                    }
                    String_insertChar(fileName,i,buffer[z]);
                    i += 1L;
                  }
                  break;
                #ifndef NDEBUG
                  default:
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                    break; /* not reached */
                  #endif /* NDEBUG */
              }
              break;
          }
        }
        else
        {
          // % at end of string
          i += 1L;
        }
        break;
      case '#':
        // #...#
        switch (formatMode)
        {
          case FORMAT_MODE_ARCHIVE_FILE_NAME:
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
          case FORMAT_MODE_PATTERN:
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
    switch (formatMode)
    {
      case FORMAT_MODE_ARCHIVE_FILE_NAME:
        String_format(fileName,".%06d",partNumber);
        break;
      case FORMAT_MODE_PATTERN:
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
  Dictionary          duplicateNamesDictionary;
  String              name;
  Errors              error;
  FileInfo            fileInfo;
  SemaphoreLock       semaphoreLock;
  DeviceInfo          deviceInfo;

  assert(createInfo != NULL);
  assert(createInfo->includeEntryList != NULL);
  assert(createInfo->excludePatternList != NULL);
  assert(createInfo->jobOptions != NULL);

  // initialize variables
  if (!Dictionary_init(&duplicateNamesDictionary,CALLBACK_NULL,CALLBACK_NULL))
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
              if (   isInIncludedList(createInfo->includeEntryList,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                  && !Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name))
                 )
              {
                if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                {
                  // add to known names history
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

                  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                  {
                    createInfo->statusInfo.totalEntries++;
                    createInfo->statusInfo.totalBytes += fileInfo.size;
                    updateStatusInfo(createInfo);
                  }
                }
              }
              break;
            case FILE_TYPE_DIRECTORY:
              if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
              {
                // add to known names history
                Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

                SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                {
                  createInfo->statusInfo.totalEntries++;
                  updateStatusInfo(createInfo);
                }
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
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

                  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                  {
                    createInfo->statusInfo.totalEntries++;
                    updateStatusInfo(createInfo);
                  }
                }
              }
              break;
            case FILE_TYPE_HARDLINK:
              if (   isInIncludedList(createInfo->includeEntryList,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                  && !Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name))
                  )
              {
                if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                {
                  // add to known names history
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

                  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                  {
                    createInfo->statusInfo.totalEntries++;
                    createInfo->statusInfo.totalBytes += fileInfo.size;
                    updateStatusInfo(createInfo);
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
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

                  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                  {
                    createInfo->statusInfo.totalEntries++;
                    updateStatusInfo(createInfo);
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
        StringList_getLast(&nameList,name);

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
                Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

                switch (includeEntryNode->type)
                {
                  case ENTRY_TYPE_FILE:
                    if (   !createInfo->partialFlag
                        || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                       )
                    {
                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                      {
                        createInfo->statusInfo.totalEntries++;
                        createInfo->statusInfo.totalBytes += fileInfo.size;
                        updateStatusInfo(createInfo);
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
              if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
              {
                // add to known names history
                Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

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
                          SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                          {
                            createInfo->statusInfo.totalEntries++;
                            updateStatusInfo(createInfo);
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
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0,CALLBACK_NULL);

                                switch (includeEntryNode->type)
                                {
                                  case ENTRY_TYPE_FILE:
                                    if (   !createInfo->partialFlag
                                        || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                        )
                                    {
                                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                                      {
                                        createInfo->statusInfo.totalEntries++;
                                        createInfo->statusInfo.totalBytes += fileInfo.size;
                                        updateStatusInfo(createInfo);
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
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0,CALLBACK_NULL);

                                switch (includeEntryNode->type)
                                {
                                  case ENTRY_TYPE_FILE:
                                    if (   !createInfo->partialFlag
                                        || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                       )
                                    {
                                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                                      {
                                        createInfo->statusInfo.totalEntries++;
                                        updateStatusInfo(createInfo);
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
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0,CALLBACK_NULL);

                                switch (includeEntryNode->type)
                                {
                                  case ENTRY_TYPE_FILE:
                                    if (  !createInfo->partialFlag
                                        || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                        )
                                    {
                                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                                      {
                                        createInfo->statusInfo.totalEntries++;
                                        createInfo->statusInfo.totalBytes += fileInfo.size;
                                        updateStatusInfo(createInfo);
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
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0,CALLBACK_NULL);

                                switch (includeEntryNode->type)
                                {
                                  case ENTRY_TYPE_FILE:
                                    if (  !createInfo->partialFlag
                                        || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                        )
                                    {
                                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                                      {
                                        createInfo->statusInfo.totalEntries++;
                                        if (   (includeEntryNode->type == ENTRY_TYPE_IMAGE)
                                            && (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                                            )
                                        {
                                          createInfo->statusInfo.totalBytes += fileInfo.size;
                                        }
                                        updateStatusInfo(createInfo);
                                      }
                                    }
                                    break;
                                  case ENTRY_TYPE_IMAGE:
                                    if (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                                    {
                                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                                      {
                                        createInfo->statusInfo.totalEntries++;
                                        createInfo->statusInfo.totalBytes += fileInfo.size;
                                        updateStatusInfo(createInfo);
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
              }
              break;
            case FILE_TYPE_LINK:
              if (   isIncluded(includeEntryNode,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                  && !Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name))
                 )
              {
                // add to known names history
                Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

                switch (includeEntryNode->type)
                {
                  case ENTRY_TYPE_FILE:
                    if (   !createInfo->partialFlag
                        || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                        )
                    {
                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                      {
                        createInfo->statusInfo.totalEntries++;
                        updateStatusInfo(createInfo);
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
                Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

                switch (includeEntryNode->type)
                {
                  case ENTRY_TYPE_FILE:
                    if (   !createInfo->partialFlag
                        || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                        )
                    {
                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                      {
                        createInfo->statusInfo.totalEntries++;
                        createInfo->statusInfo.totalBytes += fileInfo.size;
                        updateStatusInfo(createInfo);
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
                Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

                switch (includeEntryNode->type)
                {
                  case ENTRY_TYPE_FILE:
                    if (   !createInfo->partialFlag
                        || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                        )
                    {
                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                      {
                        createInfo->statusInfo.totalEntries++;
                        updateStatusInfo(createInfo);
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

                      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                      {
                        createInfo->statusInfo.totalEntries++;
                        createInfo->statusInfo.totalBytes += fileInfo.size;
                        updateStatusInfo(createInfo);
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
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    createInfo->statusInfo.collectTotalSumDone = TRUE;
    updateStatusInfo(createInfo);
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
  AutoFreeList        autoFreeList;
  Dictionary          duplicateNamesDictionary;
  StringList          nameList;
  String              basePath;
  String              name;
  FileInfo            fileInfo;
  SemaphoreLock       semaphoreLock;
  String              fileName;
  Dictionary          hardLinksDictionary;
  Errors              error;
  DeviceInfo          deviceInfo;
  DictionaryIterator  dictionaryIterator;
//???
union { const void *value; const uint64 *id; } keyData;
union { void *value; HardLinkInfo *hardLinkInfo; } data;

  assert(createInfo != NULL);
  assert(createInfo->includeEntryList != NULL);
  assert(createInfo->excludePatternList != NULL);
  assert(createInfo->jobOptions != NULL);

  // initialize variables
  AutoFree_init(&autoFreeList);
  if (!Dictionary_init(&duplicateNamesDictionary,CALLBACK_NULL,CALLBACK_NULL))
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  name     = String_new();
  if (!Dictionary_init(&hardLinksDictionary,DICTIONARY_BYTE_FREE,CALLBACK_NULL))
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,&duplicateNamesDictionary,{ Dictionary_done(&duplicateNamesDictionary); });
  AUTOFREE_ADD(&autoFreeList,name,{ String_delete(name); });
  AUTOFREE_ADD(&autoFreeList,&hardLinksDictionary,{ Dictionary_done(&hardLinksDictionary); });

  if (createInfo->archiveType == ARCHIVE_TYPE_CONTINUOUS)
  {
    DatabaseQueryHandle databaseQueryHandle;
    StaticString        (jobUUID,MISC_UUID_STRING_LENGTH);
    DatabaseId          databaseId;

fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
    // process entries from continous database
    error = Continuous_initList(&databaseQueryHandle,createInfo->jobUUID,createInfo->scheduleUUID);
    if (error == ERROR_NONE)
    {
      AUTOFREE_ADD(&autoFreeList,&databaseQueryHandle,{ Continuous_doneList(&databaseQueryHandle); });

      while (Continuous_getNext(&databaseQueryHandle,&databaseId,name))
      {
fprintf(stderr,"%s, %d: jobUUID=%s name='%s'\n",__FILE__,__LINE__,String_cString(jobUUID),String_cString(name));
        // pause
        pauseCreate(createInfo);

        // remove continuous entry
        Continuous_remove(databaseId);

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

          SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
          {
            createInfo->statusInfo.errorEntries++;
            updateStatusInfo(createInfo);
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
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

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
              }
              else
              {
                logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'\n",String_cString(name));

                SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                {
                  createInfo->statusInfo.skippedEntries++;
                  createInfo->statusInfo.skippedBytes += fileInfo.size;
                  updateStatusInfo(createInfo);
                }
              }
              break;
            case FILE_TYPE_DIRECTORY:
              if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
              {
                // add to known names history
                Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

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
            case FILE_TYPE_LINK:
              if (   isInIncludedList(createInfo->includeEntryList,name)
                  && !isInExcludedList(createInfo->excludePatternList,name)
                 )
              {
                if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                {
                  // add to known names history
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

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

                SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                {
                  createInfo->statusInfo.skippedEntries++;
                  createInfo->statusInfo.skippedBytes += fileInfo.size;
                  updateStatusInfo(createInfo);
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
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

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
                                        sizeof(hardLinkInfo),
                                        DICTIONARY_BYTE_COPY
                                       )
                        )
                    {
                      HALT_INSUFFICIENT_MEMORY();
                    }
                  }
                }
              }
              else
              {
                logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'\n",String_cString(name));

                SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                {
                  createInfo->statusInfo.skippedEntries++;
                  createInfo->statusInfo.skippedBytes += fileInfo.size;
                  updateStatusInfo(createInfo);
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
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

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

                SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                {
                  createInfo->statusInfo.skippedEntries++;
                  createInfo->statusInfo.skippedBytes += fileInfo.size;
                  updateStatusInfo(createInfo);
                }
              }
              break;
            default:
              printInfo(2,"Unknown type of file '%s' - skipped\n",String_cString(fileName));
              logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_TYPE_UNKNOWN,"Unknown type '%s'\n",String_cString(fileName));

              SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
              {
                createInfo->statusInfo.errorEntries++;
                createInfo->statusInfo.errorBytes += (uint64)fileInfo.size;
                updateStatusInfo(createInfo);
              }
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
      StringList_append(&nameList,basePath);
      while (   !StringList_isEmpty(&nameList)
             && (createInfo->failError == ERROR_NONE)
             && !isAborted(createInfo)
            )
      {
        // pause
        pauseCreate(createInfo);

        // get next entry to process
        StringList_getLast(&nameList,name);
//fprintf(stderr,"%s, %d: ----------------------\n",__FILE__,__LINE__);
//fprintf(stderr,"%s, %d: %s included=%d excluded=%d dictionary=%d\n",__FILE__,__LINE__,String_cString(name),isIncluded(includeEntryNode,name),isInExcludedList(createInfo->excludePatternList,name),Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)));

        // read file info
        error = File_getFileInfo(name,&fileInfo);
        if (error != ERROR_NONE)
        {
          printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(name),Error_getText(error));
          logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(name),Error_getText(error));

          SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
          {
            createInfo->statusInfo.errorEntries++;
            updateStatusInfo(createInfo);
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
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

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

                SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                {
                  createInfo->statusInfo.skippedEntries++;
                  createInfo->statusInfo.skippedBytes += fileInfo.size;
                  updateStatusInfo(createInfo);
                }
              }
              break;
            case FILE_TYPE_DIRECTORY:
              if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
              {
                // add to known names history
                Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

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

                    SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                    {
                      createInfo->statusInfo.skippedEntries++;
                      createInfo->statusInfo.skippedBytes += fileInfo.size;
                      updateStatusInfo(createInfo);
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

                        SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                        {
                          createInfo->statusInfo.errorEntries++;
                          createInfo->statusInfo.errorBytes += (uint64)fileInfo.size;
                          updateStatusInfo(createInfo);
                        }
                        continue;
                      }
  //fprintf(stderr,"%s, %d: %s included=%d excluded=%d dictionary=%d\n",__FILE__,__LINE__,String_cString(fileName),isIncluded(includeEntryNode,fileName),isInExcludedList(createInfo->excludePatternList,fileName),Dictionary_contains(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName)));

                      if (   isIncluded(includeEntryNode,fileName)
                          && !isInExcludedList(createInfo->excludePatternList,fileName)
                         )
                      {
                        // read file info
                        error = File_getFileInfo(fileName,&fileInfo);
                        if (error != ERROR_NONE)
                        {
                          printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(fileName),Error_getText(error));
                          logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(fileName),Error_getText(error));

                          SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                          {
                            createInfo->statusInfo.errorEntries++;
                            updateStatusInfo(createInfo);
                          }
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
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0,CALLBACK_NULL);

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
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0,CALLBACK_NULL);

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
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0,CALLBACK_NULL);

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
                                                              sizeof(hardLinkInfo),
                                                              DICTIONARY_BYTE_COPY
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
                                Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0,CALLBACK_NULL);

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

                              SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                              {
                                createInfo->statusInfo.errorEntries++;
                                createInfo->statusInfo.errorBytes += (uint64)fileInfo.size;
                                updateStatusInfo(createInfo);
                              }
                              break;
                          }
                        }
                        else
                        {
                          logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s' (no dump attribute)\n",String_cString(fileName));

                          SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                          {
                            createInfo->statusInfo.skippedEntries++;
                            createInfo->statusInfo.skippedBytes += fileInfo.size;
                            updateStatusInfo(createInfo);
                          }
                        }
                      }
                      else
                      {
                        logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'\n",String_cString(fileName));

                        SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                        {
                          createInfo->statusInfo.skippedEntries++;
                          createInfo->statusInfo.skippedBytes += fileInfo.size;
                          updateStatusInfo(createInfo);
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

                    SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                    {
                      createInfo->statusInfo.errorEntries++;
                      updateStatusInfo(createInfo);
                    }
                  }
                }
                else
                {
                  logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s' (.nobackup file)\n",String_cString(name));
                }
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
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

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

                SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                {
                  createInfo->statusInfo.skippedEntries++;
                  createInfo->statusInfo.skippedBytes += fileInfo.size;
                  updateStatusInfo(createInfo);
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
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

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
                                                sizeof(hardLinkInfo),
                                                DICTIONARY_BYTE_COPY
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

                SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                {
                  createInfo->statusInfo.skippedEntries++;
                  createInfo->statusInfo.skippedBytes += fileInfo.size;
                  updateStatusInfo(createInfo);
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
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0,CALLBACK_NULL);

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

                          SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                          {
                            createInfo->statusInfo.errorEntries++;
                            updateStatusInfo(createInfo);
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

                SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
                {
                  createInfo->statusInfo.skippedEntries++;
                  createInfo->statusInfo.skippedBytes += fileInfo.size;
                  updateStatusInfo(createInfo);
                }
              }
              break;
            default:
              printInfo(2,"Unknown type of file '%s' - skipped\n",String_cString(name));
              logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_TYPE_UNKNOWN,"Unknown type '%s'\n",String_cString(name));

              SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
              {
                createInfo->statusInfo.errorEntries++;
                createInfo->statusInfo.errorBytes += (uint64)fileInfo.size;
                updateStatusInfo(createInfo);
              }
              break;
          }
        }
        else
        {
          logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s' (no dump attribute)\n",String_cString(name));

          SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
          {
            createInfo->statusInfo.skippedEntries++;
            createInfo->statusInfo.skippedBytes += fileInfo.size;
            updateStatusInfo(createInfo);
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

  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->storageInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
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

  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->storageInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    assert(createInfo->storageInfo.count > 0);
    assert(createInfo->storageInfo.bytes >= size);

    createInfo->storageInfo.count -= 1;
    createInfo->storageInfo.bytes -= size;
  }
}

/***********************************************************************\
* Name   : getArchiveSize
* Purpose: call back for init archive file
* Input  : userData    - user data
*          indexHandle - index handle or NULL if no index
*          storageId   - database id of storage
*          partNumber   - part number or ARCHIVE_PART_NUMBER_NONE for
*                         single part
* Output : -
* Return : size or 0
* Notes  : -
\***********************************************************************/

LOCAL Errors getArchiveSize(void        *userData,
                            IndexHandle *indexHandle,
                            DatabaseId  entityId,
                            DatabaseId  storageId,
                            int         partNumber
                           )
{
  CreateInfo *createInfo = (CreateInfo*)userData;
  String     archiveName;
  Errors     error;
  StorageArchiveHandle storageArchiveHandle;
  uint64     storageSize;

  assert(createInfo != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(entityId);
  UNUSED_VARIABLE(createInfo);

  storageSize = 0LL;

  // get archive file name
  archiveName = String_new();
  error = formatArchiveFileName(archiveName,
                                FORMAT_MODE_ARCHIVE_FILE_NAME,
                                createInfo->storageSpecifier->archiveName,
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
  DEBUG_TESTCODE("getArchiveSize1") { String_delete(archiveName); return DEBUG_TESTCODE_ERROR(); }

  // get storage size
  error = Storage_open(&storageArchiveHandle,&createInfo->storageHandle,archiveName);
  if (error != ERROR_NONE)
  {
    String_delete(archiveName);
    return 0LL;
  }
  storageSize = Storage_getSize(&storageArchiveHandle);
  Storage_close(&storageArchiveHandle);

  // free resources
  String_delete(archiveName);

  return storageSize;
}

/***********************************************************************\
* Name   : storeArchiveFile
* Purpose: call back to store archive file
* Input  : userData     - user data
*          indexHandle  - index handle or NULL if no index
*          entityId     - database id of entity
*          partNumber   - part number or ARCHIVE_PART_NUMBER_NONE for
*                         single part
*          storageId    - database id of storage
*          tmpFileName  - temporary archive file name
*          entries      - number of entries
*          size         - size of archive
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeArchiveFile(void        *userData,
                              IndexHandle *indexHandle,
                              DatabaseId  entityId,
                              DatabaseId  storageId,
                              int         partNumber,
                              String      tmpFileName,
                              uint64      entries,
                              uint64      size
                             )
{
  CreateInfo    *createInfo = (CreateInfo*)userData;
  Errors        error;
  FileInfo      fileInfo;
  String        archiveName;
  ConstString   printableStorageName;
  StorageMsg    storageMsg;
  SemaphoreLock semaphoreLock;

  assert(createInfo != NULL);
  assert(createInfo->storageSpecifier != NULL);
  assert(tmpFileName != NULL);
  assert(!String_isEmpty(tmpFileName));

  // get file info
// TODO replace by getFileSize()
  error = File_getFileInfo(tmpFileName,&fileInfo);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get archive file name
  archiveName = String_new();
  error = formatArchiveFileName(archiveName,
                                FORMAT_MODE_ARCHIVE_FILE_NAME,
                                createInfo->storageSpecifier->archiveName,
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
  DEBUG_TESTCODE("storeArchiveFile1") { String_delete(archiveName); return DEBUG_TESTCODE_ERROR(); }

  if (storageId != DATABASE_ID_NONE)
  {
    // set database storage name
    printableStorageName = Storage_getPrintableName(createInfo->storageSpecifier,archiveName);
    error = Index_storageUpdate(indexHandle,
                                storageId,
                                printableStorageName,
                                0LL,               // entries
                                0LL                // size
                               );
    if (error != ERROR_NONE)
    {
      printError("Cannot update index for storage '%s' (error: %s)!\n",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      return error;
    }
  }

  // send to storage thread
  storageMsg.entityId    = entityId;
  storageMsg.storageId   = storageId;
  storageMsg.fileName    = String_duplicate(tmpFileName);
  storageMsg.entries     = entries;
  storageMsg.size        = size;
  storageMsg.archiveName = archiveName;
  storageInfoIncrement(createInfo,fileInfo.size);
  if (!MsgQueue_put(&createInfo->storageMsgQueue,&storageMsg,sizeof(storageMsg)))
  {
    freeStorageMsg(&storageMsg,NULL);
    return ERROR_NONE;
  }
  DEBUG_TESTCODE("storeArchiveFile2") { return DEBUG_TESTCODE_ERROR(); }

  // update status info
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    createInfo->statusInfo.storageTotalBytes += fileInfo.size;
    updateStatusInfo(createInfo);
  }

  // wait for space in temporary directory
  if (globalOptions.maxTmpSize > 0)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->storageInfoLock,SEMAPHORE_LOCK_TYPE_READ)
    {
      while (   (createInfo->storageInfo.count > 2)                           // more than 2 archives are waiting
             && (createInfo->storageInfo.bytes > globalOptions.maxTmpSize)    // temporary space above limit is used
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
* Name   : purgeStorage
* Purpose: purge storage
* Input  : jobUUID        - job UUID
*          maxStorageSize - max. storage size [bytes]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void purgeStorage(ConstString jobUUID, uint64 maxStorageSize, LogHandle *logHandle)
{
  String           storageName;
  StorageSpecifier storageSpecifier;
  Errors           error;
  uint64           storageSize;
  DatabaseId       oldestStorageId;
  String           oldestStorageName;
  uint64           oldestCreatedDateTime;
  uint64           oldestSize;
  IndexQueryHandle indexQueryHandle;
  DatabaseId       storageId;
  uint64           createdDateTime;
  uint64           size;
  StorageHandle    storageHandle;
  String           dateTime;

fprintf(stderr,"%s, %d: purgeStorage\n",__FILE__,__LINE__);
  // init variables
  storageName       = String_new();
  oldestStorageName = String_new();
  Storage_initSpecifier(&storageSpecifier);
  dateTime          = String_new();

  do
  {
    // get storage size, find oldest entry
    storageSize           = 0LL;
    oldestStorageId       = DATABASE_ID_NONE;
    String_clear(oldestStorageName);
    oldestCreatedDateTime = MAX_UINT64;
    oldestSize            = 0LL;
    error = Index_initListStorage(&indexQueryHandle,
                                  indexHandle,
                                  jobUUID,
                                  DATABASE_ID_ANY,  // entityId,
                                  STORAGE_TYPE_ANY,  // storageType,
                                  NULL,  // storageName,
                                  NULL,  // hostName,
                                  NULL,  // loginName,
                                  NULL,  // deviceName,
                                  NULL,  // fileName,
                                  INDEX_STATE_SET(INDEX_STATE_OK)
                                 );
    if (error != ERROR_NONE)
    {
      break;
    }
    while (Index_getNextStorage(&indexQueryHandle,
                                &storageId,
                                NULL,  //DatabaseId       *entityId,
                                NULL,  //String           jobUUID,
                                NULL,  //String           scheduleUUID,
                                NULL,  //ArchiveTypes     *archiveType,
                                storageName,
                                &createdDateTime,
                                NULL,  //uint64           *entries,
                                &size,
                                NULL,  //IndexStates      *indexState,
                                NULL,  //IndexModes       *indexMode,
                                NULL,  //uint64           *lastCheckedDateTime,
                                NULL   //String           errorMessage
                               )
          )
    {
      if (createdDateTime < oldestCreatedDateTime)
      {
        oldestStorageId       = storageId;
        String_set(oldestStorageName,storageName);
        oldestCreatedDateTime = createdDateTime;
        oldestSize            = size;
      }
      storageSize += size;
    }
    Index_doneList(&indexQueryHandle);

    if ((storageSize > maxStorageSize) && (oldestStorageId != DATABASE_ID_NONE))
    {
fprintf(stderr,"%s, %d: purge sotrage %lld\n",__FILE__,__LINE__,oldestStorageId);
      // delete oldest storage
      error = Storage_parseName(&storageSpecifier,oldestStorageName);
      if (error != ERROR_NONE)
      {
        break;
      }
      error = Storage_init(&storageHandle,
                           &storageSpecifier,
                           NULL,  // jobOptions
                           &globalOptions.indexDatabaseMaxBandWidthList,
                           SERVER_CONNECTION_PRIORITY_HIGH,
                           CALLBACK(NULL,NULL),
                           CALLBACK(NULL,NULL)
                          );
      if (error != ERROR_NONE)
      {
        break;
      }
      error = Storage_delete(&storageHandle,
                             NULL  // archiveName
                            );
      if (error != ERROR_NONE)
      {
        Storage_done(&storageHandle);
        break;
      }
#warning TODO
      (void)Storage_pruneDirectories(&storageHandle);
      Storage_done(&storageHandle);

      // delete database entry
      error = Index_deleteStorage(indexHandle,oldestStorageId);
      if (error != ERROR_NONE)
      {
        break;
      }

      Misc_formatDateTime(dateTime,oldestCreatedDateTime,NULL);
      logMessage(logHandle,
                 LOG_TYPE_STORAGE,
                 "Purged storage %s, created at %s, %llu bytes\n",
                 String_cString(oldestStorageName),
                 String_cString(dateTime),
                 oldestSize
                );
    }
  }
  while (   (storageSize > maxStorageSize)
         && (oldestStorageId != DATABASE_ID_NONE)
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

  AutoFreeList               autoFreeList;
  byte                       *buffer;
  String                     storageName;
  void                       *autoFreeSavePoint;
  StorageMsg                 storageMsg;
  Errors                     error;
  FileInfo                   fileInfo;
  FileHandle                 fileHandle;
  uint                       retryCount;
  StorageArchiveHandle       storageArchiveHandle;
  ulong                      bufferLength;
  SemaphoreLock              semaphoreLock;
  String                     pattern;
  StorageSpecifier           storageDirectorySpecifier;
  IndexQueryHandle           indexQueryHandle;
  DatabaseId                 oldStorageId;
  String                     oldStorageName;
  StorageDirectoryListHandle storageDirectoryListHandle;
  String                     fileName;

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
  AUTOFREE_ADD(&autoFreeList,buffer,{ free(buffer); });

  // initial pre-processing
  if (!isAborted(createInfo))
  {
    if (createInfo->failError == ERROR_NONE)
    {
      // pause
      pauseStorage(createInfo);

      // initial pre-process
      error = Storage_preProcess(&createInfo->storageHandle,NULL,TRUE);
      if (error != ERROR_NONE)
      {
        printError("Cannot pre-process storage (error: %s)!\n",
                   Error_getText(error)
                  );
        createInfo->failError = error;
      }
    }
  }

  // store data
  storageName       = String_new();
  autoFreeSavePoint = AutoFree_save(&autoFreeList);
  while (   (createInfo->failError == ERROR_NONE)
         && !isAborted(createInfo)
         && MsgQueue_get(&createInfo->storageMsgQueue,&storageMsg,NULL,sizeof(storageMsg),WAIT_FOREVER)
        )
  {
    AUTOFREE_ADD(&autoFreeList,&storageMsg,
                 {
                   storageInfoDecrement(createInfo,storageMsg.size);
                   File_delete(storageMsg.fileName,FALSE);
                   if (storageMsg.storageId != DATABASE_ID_NONE) Index_deleteStorage(indexHandle,storageMsg.storageId);
                   freeStorageMsg(&storageMsg,NULL);
                 }
                );

    // pause
    pauseStorage(createInfo);

    // pre-process
    error = Storage_preProcess(&createInfo->storageHandle,storageMsg.archiveName,FALSE);
    if (error != ERROR_NONE)
    {
      printError("Cannot pre-process file '%s' (error: %s)!\n",
                 String_cString(storageMsg.fileName),
                 Error_getText(error)
                );
      createInfo->failError = error;

      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }
    DEBUG_TESTCODE("storageThreadCode1") { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); break; }

    // get printable storage name
    String_set(storageName,Storage_getPrintableName(createInfo->storageSpecifier,storageMsg.archiveName));

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
    DEBUG_TESTCODE("storageThreadCode2") { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); break; }

    // check storage size, purge old archives
    if (createInfo->jobOptions->maxStorageSize > 0LL)
    {
      // purge by job storage size
      purgeStorage(createInfo->jobUUID,
                   (createInfo->jobOptions->maxStorageSize > fileInfo.size) ? createInfo->jobOptions->maxStorageSize-fileInfo.size : 0LL,
                   createInfo->logHandle
                  );
    }
//TODO
      // clean by server storage size

    // open file to store
    #ifndef NDEBUG
      printInfo(1,"Store '%s' to '%s'...",String_cString(storageMsg.fileName),String_cString(storageName));
    #else /* not NDEBUG */
      printInfo(1,"Store archive '%s'...",String_cString(storageName));
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
    DEBUG_TESTCODE("storageThreadCode4") { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); continue; }

    // write data to store file
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
      pauseStorage(createInfo);

      // create storage file
      error = Storage_create(&storageArchiveHandle,
                             &createInfo->storageHandle,
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
          printError("Cannot store file '%s' (error: %s)\n",
                     String_cString(storageName),
                     Error_getText(error)
                    );
          break;
        }
      }
      DEBUG_TESTCODE("storageThreadCode5") { Storage_close(&createInfo->storageHandle); error = DEBUG_TESTCODE_ERROR(); break; }

      // update status info, check for abort
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        String_set(createInfo->statusInfo.storageName,storageName);
        updateStatusInfo(createInfo);
      }

      // store data
      File_seek(&fileHandle,0);
      do
      {
        // pause
        pauseStorage(createInfo);

        // read data from local file
        error = File_read(&fileHandle,buffer,BUFFER_SIZE,&bufferLength);
        if (error != ERROR_NONE)
        {
          printInfo(0,"FAIL!\n");
          printError("Cannot read file '%s' (error: %s)!\n",
                     String_cString(storageName),
                     Error_getText(error)
                    );
          break;
        }
        DEBUG_TESTCODE("storageThreadCode6") { error = DEBUG_TESTCODE_ERROR(); break; }

        // store data
        error = Storage_write(&storageArchiveHandle,buffer,bufferLength);
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
                       String_cString(storageName),
                       Error_getText(error)
                      );
            break;
          }
        }
        DEBUG_TESTCODE("storageThreadCode7") { error = DEBUG_TESTCODE_ERROR(); break; }

        // update status info, check for abort
        SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
        {
          createInfo->statusInfo.storageDoneBytes += (uint64)bufferLength;
          updateStatusInfo(createInfo);
        }
      }
      while (   !File_eof(&fileHandle)
             && !isAborted(createInfo)
            );

      // close storage
      Storage_close(&storageArchiveHandle);
    }
    while (   (error != ERROR_NONE)
           && (retryCount <= MAX_RETRIES)
           && !isAborted(createInfo)
          );
    if (error != ERROR_NONE)
    {
      createInfo->failError = error;

      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }

    // close file to store
    File_close(&fileHandle);
    AUTOFREE_REMOVE(&autoFreeList,&fileHandle);

    if (!isAborted(createInfo))
    {
      printInfo(1,"ok\n");
      logMessage(createInfo->logHandle,LOG_TYPE_STORAGE,"Stored '%s'\n",String_cString(storageName));
    }
    else
    {
      printInfo(1,"ABORTED\n");
    }

    // update index database and set state
    if (storageMsg.storageId != DATABASE_ID_NONE)
    {
#warning TODO
#if 0
      // delete old indizes for same storage file
      oldStorageName = String_new();
      error = Index_initListStorage(&indexQueryHandle,
                                    indexHandle,
                                    NULL, // uuid,
                                    DATABASE_ID_ANY, // jobId
                                    STORAGE_TYPE_ANY,
                                    NULL, // storageName
                                    createInfo->storageSpecifier->hostName,
                                    createInfo->storageSpecifier->loginName,
                                    createInfo->storageSpecifier->deviceName,
                                    storageMsg.destinationFileName,
                                    INDEX_STATE_SET_ALL
                                   );
      while (Index_getNextStorage(&indexQueryHandle,
                                  &oldStorageId,
                                  NULL, // entity id
                                  NULL, // job UUID
                                  NULL, // schedule UUID
                                  NULL, // archive type
                                  oldStorageName,
                                  NULL, // createdDateTime
                                  NULL, // entries
                                  NULL, // size
                                  NULL, // indexState,
                                  NULL, // indexMode,
                                  NULL, // lastCheckedDateTime,
                                  NULL  // errorMessage
                                 )
            )
      {
        if (oldStorageId != storageMsg.storageId)
        {
          error = Index_deleteStorage(indexHandle,oldStorageId);
          if (error != ERROR_NONE)
          {
            printError("Cannot delete old index for storage '%s' (error: %s)!\n",
                       String_cString(oldStorageName),
                       Error_getText(error)
                      );
            createInfo->failError = error;
            break;
          }
          DEBUG_TESTCODE("storageThreadCode8") { createInfo->failError = DEBUG_TESTCODE_ERROR(); break; }
        }
      }
      Index_doneList(&indexQueryHandle);
      String_delete(oldStorageName);
      if (createInfo->failError != ERROR_NONE)
      {
        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        continue;
      }
#endif

      // set database storage entries and size
      error = Index_storageUpdate(indexHandle,
                                  storageMsg.storageId,
                                  NULL,              // storageName
                                  storageMsg.entries,
                                  fileInfo.size
                                 );
      if (error != ERROR_NONE)
      {
        printError("Cannot update index for storage '%s' (error: %s)!\n",
                   String_cString(storageName),
                   Error_getText(error)
                  );
        createInfo->failError = error;

        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        continue;
      }
      DEBUG_TESTCODE("storageThreadCode9") { createInfo->failError = DEBUG_TESTCODE_ERROR(); }

      // set database state and time stamp
      error = Index_setState(indexHandle,
                             storageMsg.storageId,
                             ((createInfo->failError == ERROR_NONE) && !isAborted(createInfo))
                               ? INDEX_STATE_OK
                               : INDEX_STATE_ERROR,
                             Misc_getCurrentDateTime(),
                             NULL // errorMessage
                            );
      if (error != ERROR_NONE)
      {
        printError("Cannot update index for storage '%s' (error: %s)!\n",
                   String_cString(storageName),
                   Error_getText(error)
                  );
        createInfo->failError = error;

        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        continue;
      }
      DEBUG_TESTCODE("storageThreadCode10") { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); continue; }
    }

    // post-process
    error = Storage_postProcess(&createInfo->storageHandle,storageMsg.archiveName,FALSE);
    if (error != ERROR_NONE)
    {
      printError("Cannot post-process storage file '%s' (error: %s)!\n",
                 String_cString(storageName),
                 Error_getText(error)
                );
      createInfo->failError = error;

      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }
    DEBUG_TESTCODE("storageThreadCode11") { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); continue; }

    // check if aborted
    if (isAborted(createInfo))
    {
      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      continue;
    }

    // add to list of stored archive files
#warning TODO
//    StringList_append(&createInfo->storageFileList,storageMsg.destinationFileName);

    // delete temporary storage file
    error = File_delete(storageMsg.fileName,FALSE);
    if (error != ERROR_NONE)
    {
      printWarning("Cannot delete file '%s' (error: %s)!\n",
                   String_cString(storageMsg.fileName),
                   Error_getText(error)
                  );
    }

    // update storage info
    storageInfoDecrement(createInfo,storageMsg.size);

    // free resources
    freeStorageMsg(&storageMsg,NULL);
    AutoFree_restore(&autoFreeList,autoFreeSavePoint,FALSE);
  }
  String_delete(storageName);

  // final post-processing
  if (   !isAborted(createInfo)
      && (createInfo->failError == ERROR_NONE)
     )
  {
    // pause
    pauseStorage(createInfo);

    error = Storage_postProcess(&createInfo->storageHandle,NULL,TRUE);
    if (error != ERROR_NONE)
    {
      printError("Cannot post-process storage (error: %s)!\n",
                 Error_getText(error)
                );
      createInfo->failError = error;
    }
  }

  // delete old storage files
  if (   !isAborted(createInfo)
      && (createInfo->failError == ERROR_NONE)
     )
  {
    if (globalOptions.deleteOldArchiveFilesFlag)
    {
      // get archive name pattern
      pattern = String_new();
      error = formatArchiveFileName(pattern,
                                    FORMAT_MODE_PATTERN,
                                    createInfo->storageSpecifier->archiveName,
                                    createInfo->archiveType,
                                    createInfo->scheduleTitle,
                                    createInfo->scheduleCustomText,
                                    createInfo->startTime,
                                    ARCHIVE_PART_NUMBER_NONE
                                   );
      if (error == ERROR_NONE)
      {
        // open directory
        Storage_duplicateSpecifier(&storageDirectorySpecifier,createInfo->storageSpecifier);
        File_getFilePathName(storageDirectorySpecifier.archiveName,createInfo->storageSpecifier->archiveName);
        error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                          &storageDirectorySpecifier,
                                          createInfo->jobOptions,
                                          SERVER_CONNECTION_PRIORITY_HIGH,
                                          NULL  // archiveName
                                         );
        if (error == ERROR_NONE)
        {
          // read directory
          fileName = String_new();
          while (   !Storage_endOfDirectoryList(&storageDirectoryListHandle)
                 && (Storage_readDirectoryList(&storageDirectoryListHandle,fileName,NULL) == ERROR_NONE)
                )
          {
            // find in storage list
            if (String_match(fileName,STRING_BEGIN,pattern,NULL,NULL))
            {
              if (StringList_find(&createInfo->storageFileList,fileName) == NULL)
              {
#warning XXXX
//                Storage_delete(&createInfo->storageHandle,fileName);
              }
            }
          }
          String_delete(fileName);

          // close directory
          Storage_closeDirectoryList(&storageDirectoryListHandle);
        }
        Storage_doneSpecifier(&storageDirectorySpecifier);
      }
      String_delete(pattern);
    }
  }

  // free resoures
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

  locked = Semaphore_lock(&createInfo->statusInfoNameLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,SEMAPHORE_NO_WAIT);
  if (locked)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
    {
      String_set(createInfo->statusInfo.name,name);
      createInfo->statusInfo.entryDoneBytes  = 0LL;
      createInfo->statusInfo.entryTotalBytes = size;
      updateStatusInfo(createInfo);
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
* Input  : createInfo - create info structure
*          fileName   - file name to store
*          buffer     - buffer for temporary data
*          bufferSize - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeFileEntry(CreateInfo  *createInfo,
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
  uint64                    entryDoneBytes;
  ulong                     bufferLength;
  uint64                    archiveSize;
  uint64                    doneBytes,archiveBytes;
  double                    compressionRatio;
  uint                      percentageDone;

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);
  assert(fileName != NULL);
  assert(buffer != NULL);

  printInfo(1,"Add '%s'...",String_cString(fileName));

  // try to set status done entry info
  statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,fileName,fileInfo.size);

  // get file info, file extended attributes
  error = File_getFileInfo(fileName,&fileInfo);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(fileName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        createInfo->statusInfo.errorEntries++;
      }
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get info for '%s' (error: %s)\n",
                 String_cString(fileName),
                 Error_getText(error)
                );
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
  }
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,fileName);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(fileName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        createInfo->statusInfo.errorEntries++;
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

//fprintf(stderr,"%s, %d: ----------------\n",__FILE__,__LINE__);
//FileExtendedAttributeNode *fileExtendedAttributeNode; LIST_ITERATE(&fileExtendedAttributeList,fileExtendedAttributeNode) { fprintf(stderr,"%s, %d: fileExtendedAttributeNode=%s\n",__FILE__,__LINE__,String_cString(fileExtendedAttributeNode->name)); }

  // open file
  error = File_open(&fileHandle,fileName,FILE_OPEN_READ|FILE_OPEN_NO_ATIME|FILE_OPEN_NO_CACHE);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Open file failed '%s'\n",String_cString(fileName));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        createInfo->statusInfo.errorEntries++;
        createInfo->statusInfo.errorBytes += (uint64)fileInfo.size;
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
    error          = ERROR_NONE;
    entryDoneBytes = 0LL;
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
            entryDoneBytes += (uint64)bufferLength;
            archiveSize    = Archive_getSize(&createInfo->archiveInfo);

            // try to set status done entry info
            if (!statusEntryDoneLocked)
            {
              statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,fileName,fileInfo.size);
            }

            // update status info
            SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
            {
              doneBytes        = createInfo->statusInfo.doneBytes+(uint64)bufferLength;
              archiveBytes     = createInfo->statusInfo.storageTotalBytes+archiveSize;
              compressionRatio = (!createInfo->jobOptions->dryRunFlag && (doneBytes > 0))
                                   ? 100.0-(archiveBytes*100.0)/doneBytes
                                   : 0.0;

              if (statusEntryDoneLocked)
              {
                String_set(createInfo->statusInfo.name,fileName);
                createInfo->statusInfo.entryDoneBytes  = entryDoneBytes;
                createInfo->statusInfo.entryTotalBytes = fileInfo.size;
              }
              createInfo->statusInfo.doneBytes        = doneBytes;
              createInfo->statusInfo.archiveBytes     = archiveBytes;
              createInfo->statusInfo.compressionRatio = compressionRatio;
              updateStatusInfo(createInfo);
            }
          }

          if (isPrintInfo(2))
          {
            percentageDone = 0;
            SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ)
            {
              percentageDone = (createInfo->statusInfo.entryTotalBytes > 0LL)
                                 ? (uint)((createInfo->statusInfo.entryDoneBytes*100LL)/createInfo->statusInfo.entryTotalBytes)
                                 : 100;
            }
            printInfo(2,"%3d%%\b\b\b\b",percentageDone);
          }
        }
      }
    }
    while (   !isAborted(createInfo)
           && (bufferLength > 0L)
           && (createInfo->failError == ERROR_NONE)
           && (error == ERROR_NONE)
          );
    if (isAborted(createInfo))
    {
      printInfo(1,"ABORTED\n");
      Archive_closeEntry(&archiveEntryInfo);
      (void)File_close(&fileHandle);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return FALSE;
    }
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot store archive file (error: %s)!\n",
                 Error_getText(error)
                );
      Archive_closeEntry(&archiveEntryInfo);
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
      printInfo(1,"ok (%llu bytes, ratio %.1f%%)\n",fileInfo.size,compressionRatio);
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_OK,"Added '%s'\n",String_cString(fileName));
    }
    else
    {
      printInfo(1,"ok (%llu bytes, dry-run)\n",fileInfo.size);
    }
  }
  else
  {
    printInfo(1,"ok (%llu bytes, not stored)\n",fileInfo.size);
  }

  // update done entries
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    createInfo->statusInfo.doneEntries++;
    updateStatusInfo(createInfo);
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
* Input  : createInfo - create info structure
*          deviceName - device name
*          buffer     - buffer for temporary data
*          bufferSize - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeImageEntry(CreateInfo  *createInfo,
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
  uint64           entryDoneBytes;
  uint64           block;
  uint64           blockCount;
  uint             bufferBlockCount;
  uint64           archiveSize;
  uint64           doneBytes,archiveBytes;
  double           compressionRatio;
  uint             percentageDone;
  ArchiveEntryInfo archiveEntryInfo;

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);
  assert(deviceName != NULL);
  assert(buffer != NULL);

  printInfo(1,"Add '%s'...",String_cString(deviceName));

  // try to set status done entry info
  statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,deviceName,deviceInfo.size);

  // get device info
  error = Device_getDeviceInfo(&deviceInfo,deviceName);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(deviceName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        createInfo->statusInfo.errorEntries++;
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
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        createInfo->statusInfo.errorEntries++;
        createInfo->statusInfo.errorBytes += (uint64)deviceInfo.size;
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
    block          = 0LL;
    blockCount     = deviceInfo.size/(uint64)deviceInfo.blockSize;
    error          = ERROR_NONE;
    entryDoneBytes = 0LL;
    while (   (block < blockCount)
           && !isAborted(createInfo)
           && (createInfo->failError == ERROR_NONE)
           && (error == ERROR_NONE)
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
          entryDoneBytes += (uint64)bufferBlockCount*(uint64)deviceInfo.blockSize;
          archiveSize    = Archive_getSize(&createInfo->archiveInfo);

          // try to set status done entry info
          if (!statusEntryDoneLocked)
          {
            statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,deviceName,deviceInfo.size);
          }

          // update status info
          SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
          {
            doneBytes        = createInfo->statusInfo.doneBytes+(uint64)bufferBlockCount*(uint64)deviceInfo.blockSize;
            archiveBytes     = createInfo->statusInfo.storageTotalBytes+archiveSize;
            compressionRatio = (!createInfo->jobOptions->dryRunFlag && (doneBytes > 0))
                                 ? 100.0-(archiveBytes*100.0)/doneBytes
                                 : 0.0;

            createInfo->statusInfo.doneBytes += (uint64)bufferBlockCount*(uint64)deviceInfo.blockSize;
            if (statusEntryDoneLocked)
            {
              String_set(createInfo->statusInfo.name,deviceName);
              createInfo->statusInfo.entryDoneBytes  = entryDoneBytes;
              createInfo->statusInfo.entryTotalBytes = deviceInfo.size;
            }
            createInfo->statusInfo.doneBytes        = doneBytes;
            createInfo->statusInfo.archiveBytes     = archiveBytes;
            createInfo->statusInfo.compressionRatio = compressionRatio;
            updateStatusInfo(createInfo);
          }
        }

        if (isPrintInfo(2))
        {
          percentageDone = 0;
          SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ)
          {
            percentageDone = (createInfo->statusInfo.entryTotalBytes > 0LL)
                               ? (uint)((createInfo->statusInfo.entryDoneBytes*100LL)/createInfo->statusInfo.entryTotalBytes)
                               : 100;
          }
          printInfo(2,"%3d%%\b\b\b\b",percentageDone);
        }
      }
    }
    if (isAborted(createInfo))
    {
      printInfo(1,"ABORTED\n");
      Archive_closeEntry(&archiveEntryInfo);
      if (fileSystemFlag) FileSystem_done(&fileSystemHandle);
      Device_close(&deviceHandle);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot store archive file (error: %s)!\n",
                 Error_getText(error)
                );
      Archive_closeEntry(&archiveEntryInfo);
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
      printInfo(1,"ok (%s, %llu bytes, ratio %.1f%%)\n",
                fileSystemFlag ? FileSystem_getName(fileSystemHandle.type) : "raw",
                deviceInfo.size,
                compressionRatio
               );
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_OK,"Added '%s'\n",String_cString(deviceName));
    }
    else
    {
      printInfo(1,"ok (%s, %llu bytes, dry-run)\n",
                fileSystemFlag ? FileSystem_getName(fileSystemHandle.type) : "raw",
                deviceInfo.size
               );
    }

  }
  else
  {
    printInfo(1,"ok (%s, %llu bytes, not stored)\n",
              fileSystemFlag ? FileSystem_getName(fileSystemHandle.type) : "raw",
              deviceInfo.size
             );
  }

  // update done entries
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    createInfo->statusInfo.doneEntries++;
    updateStatusInfo(createInfo);
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
*          directoryName - directory name to store
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeDirectoryEntry(CreateInfo  *createInfo,
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

  // try to set status done entry info
  statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,directoryName,0LL);

  // get file info, file extended attributes
  error = File_getFileInfo(directoryName,&fileInfo);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(directoryName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        createInfo->statusInfo.errorEntries++;
      }
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get info for '%s' (error: %s)\n",
                 String_cString(directoryName),
                 Error_getText(error)
                );
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
  }
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,directoryName);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(directoryName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        createInfo->statusInfo.errorEntries++;
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
      printInfo(1,"ok\n");
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_OK,"Added '%s'\n",String_cString(directoryName));
    }
    else
    {
      printInfo(1,"ok (dry-run)\n");
    }
  }
  else
  {
    printInfo(1,"ok (not stored)\n");
  }

  // update done entries
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    createInfo->statusInfo.doneEntries++;
    updateStatusInfo(createInfo);
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
* Input  : createInfo - create info structure
*          linkName   - link name to store
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeLinkEntry(CreateInfo  *createInfo,
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

  // try to set status done entry info
  statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,linkName,0LL);

  // get file info, file extended attributes
  error = File_getFileInfo(linkName,&fileInfo);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Alccess denied '%s' (error: %s)\n",String_cString(linkName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        createInfo->statusInfo.errorEntries++;
      }
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get info for '%s' (error: %s)\n",
                 String_cString(linkName),
                 Error_getText(error)
                );
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
  }
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,linkName);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(linkName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        createInfo->statusInfo.errorEntries++;
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
        SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
        {
          createInfo->statusInfo.errorEntries++;
          createInfo->statusInfo.errorBytes += (uint64)fileInfo.size;
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
      printInfo(1,"ok\n");
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_OK,"Added '%s'\n",String_cString(linkName));
    }
    else
    {
      printInfo(1,"ok (dry-run)\n");
    }

    // free resources
    String_delete(fileName);
  }
  else
  {
    printInfo(1,"ok (not stored)\n");
  }

  // update done entries
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    createInfo->statusInfo.doneEntries++;
    updateStatusInfo(createInfo);
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
*          nameList    - hard link name list to store
*          buffer      - buffer for temporary data
*          bufferSize  - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeHardLinkEntry(CreateInfo       *createInfo,
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
  uint64                    entryDoneBytes;
  ulong                     bufferLength;
  uint64                    archiveSize;
  uint64                    doneBytes,archiveBytes;
  double                    compressionRatio;
  uint                      percentageDone;
  const StringNode          *stringNode;
  String                    name;

  assert(createInfo != NULL);
  assert(nameList != NULL);
  assert(!StringList_isEmpty(nameList));
  assert(buffer != NULL);

  printInfo(1,"Add '%s'...",String_cString(StringList_first(nameList,NULL)));

  // try to set status done entry info
  statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,StringList_first(nameList,NULL),fileInfo.size);

  // get file info, file extended attributes
  error = File_getFileInfo(StringList_first(nameList,NULL),&fileInfo);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(StringList_first(nameList,NULL)),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        createInfo->statusInfo.errorEntries += StringList_count(nameList);
      }
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get info for '%s' (error: %s)\n",
                 String_cString(StringList_first(nameList,NULL)),
                 Error_getText(error)
                );
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
  }
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,StringList_first(nameList,NULL));
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(StringList_first(nameList,NULL)),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        createInfo->statusInfo.errorEntries++;
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
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Open file failed '%s'\n",String_cString(StringList_first(nameList,NULL)));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        createInfo->statusInfo.errorEntries += StringList_count(nameList);
        createInfo->statusInfo.errorBytes += (uint64)StringList_count(nameList)*(uint64)fileInfo.size;
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
    error          = ERROR_NONE;
    entryDoneBytes = 0LL;
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
            entryDoneBytes += (uint64)StringList_count(nameList)*(uint64)bufferLength;
            archiveSize = Archive_getSize(&createInfo->archiveInfo);

            // try to set status done entry info
            if (!statusEntryDoneLocked)
            {
              statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,StringList_first(nameList,NULL),fileInfo.size);
            }

            SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
            {
              doneBytes        = createInfo->statusInfo.doneBytes+(uint64)bufferLength;
              archiveBytes     = createInfo->statusInfo.storageTotalBytes+archiveSize;
              compressionRatio = (!createInfo->jobOptions->dryRunFlag && (doneBytes > 0))
                                   ? 100.0-(archiveBytes*100.0)/doneBytes
                                   : 0.0;

              createInfo->statusInfo.doneBytes += (uint64)StringList_count(nameList)*(uint64)bufferLength;
              if (statusEntryDoneLocked)
              {
                String_set(createInfo->statusInfo.name,StringList_first(nameList,NULL));
                createInfo->statusInfo.entryDoneBytes  = entryDoneBytes;
                createInfo->statusInfo.entryTotalBytes = fileInfo.size;
              }
              createInfo->statusInfo.doneBytes        = doneBytes;
              createInfo->statusInfo.archiveBytes     = archiveBytes;
              createInfo->statusInfo.compressionRatio = compressionRatio;
              updateStatusInfo(createInfo);
            }
          }

          if (isPrintInfo(2))
          {
            percentageDone = 0;
            SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ)
            {
              percentageDone = (createInfo->statusInfo.entryTotalBytes > 0LL)
                                 ? (uint)((createInfo->statusInfo.entryDoneBytes*100LL)/createInfo->statusInfo.entryTotalBytes)
                                 : 100;
            }
            printInfo(2,"%3d%%\b\b\b\b",percentageDone);
          }
        }
      }
    }
    while (   !isAborted(createInfo)
           && (bufferLength > 0L)
           && (createInfo->failError == ERROR_NONE)
           && (error == ERROR_NONE)
          );
    if (isAborted(createInfo))
    {
      printInfo(1,"ABORTED\n");
      Archive_closeEntry(&archiveEntryInfo);
      (void)File_close(&fileHandle);
      File_doneExtendedAttributes(&fileExtendedAttributeList);
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot store archive file (error: %s)!\n",
                 Error_getText(error)
                );
      Archive_closeEntry(&archiveEntryInfo);
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
      printInfo(1,"ok (%llu bytes, ratio %.1f%%)\n",
                fileInfo.size,
                compressionRatio
               );
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_OK,"Added '%s'\n",String_cString(StringList_first(nameList,NULL)));
    }
    else
    {
      printInfo(1,"ok (%llu bytes, dry-run)\n",fileInfo.size);
    }
  }
  else
  {
    printInfo(1,"ok (%llu bytes, not stored)\n",fileInfo.size);
  }

  // update done entries
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    createInfo->statusInfo.doneEntries += StringList_count(nameList);
    updateStatusInfo(createInfo);
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
* Input  : createInfo - create info structure
*          fileName   - file name to store
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeSpecialEntry(CreateInfo  *createInfo,
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

  // try to set status done entry info
  statusEntryDoneLocked = setStatusEntryDoneInfo(createInfo,fileName,0LL);

  // get file info, file extended attributes
  error = File_getFileInfo(fileName,&fileInfo);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(fileName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        createInfo->statusInfo.errorEntries++;
      }
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get info for '%s' (error: %s)\n",
                 String_cString(fileName),
                 Error_getText(error)
                );
      clearStatusEntryDoneInfo(createInfo,statusEntryDoneLocked);
      return error;
    }
  }
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,fileName);
  if (error != ERROR_NONE)
  {
    if (createInfo->jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_ACCESS_DENIED,"Access denied '%s' (error: %s)\n",String_cString(fileName),Error_getText(error));
      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        createInfo->statusInfo.errorEntries++;
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
      printInfo(1,"ok\n");
      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_OK,"Added '%s'\n",String_cString(fileName));
    }
    else
    {
      printInfo(1,"ok (dry-run)\n");
    }
  }
  else
  {
    printInfo(1,"ok (not stored)\n");
  }

  // update done entries
  SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    createInfo->statusInfo.doneEntries++;
    updateStatusInfo(createInfo);
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

  // store files
  while (   (createInfo->failError == ERROR_NONE)
         && !isAborted(createInfo)
         && MsgQueue_get(&createInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER)
        )
  {
    // pause
    pauseCreate(createInfo);

    // check if own file (in temporary directory or storage file)
    ownFileFlag =    String_startsWith(entryMsg.name,tmpDirectory)
                  || StringList_contain(&createInfo->storageFileList,entryMsg.name);
    if (!ownFileFlag)
    {
      STRINGLIST_ITERATE(&entryMsg.nameList,stringNode,name)
      {
        ownFileFlag =    String_startsWith(name,tmpDirectory)
                      || StringList_contain(&createInfo->storageFileList,name);
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
                                        entryMsg.name
                                       );
              if (error != ERROR_NONE) createInfo->failError = error;
              break;
            case ENTRY_TYPE_IMAGE:
              error = storeImageEntry(createInfo,
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

      SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        createInfo->statusInfo.skippedEntries++;
      }
    }

    // update status info and check if aborted
    SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
    {
      updateStatusInfo(createInfo);
    }

    // free entry message
    freeEntryMsg(&entryMsg,NULL);

// NYI: is this really useful? (avoid that sum-collector-thread is slower than file-collector-thread)
    // slow down if too fast
    while (   !createInfo->collectorSumThreadExitedFlag
           && (createInfo->statusInfo.doneEntries >= createInfo->statusInfo.totalEntries)
          )
    {
      Misc_udelay(1000*1000);
    }
  }

  // free resources
  free(buffer);
}

/*---------------------------------------------------------------------*/

Errors Command_create(ConstString                     jobUUID,
//                      ConstString                     hostName,
//                      uint                            hostPort,
                      ConstString                     scheduleUUID,
                      ConstString                     storageName,
                      const EntryList                 *includeEntryList,
                      const PatternList               *excludePatternList,
                      const PatternList               *compressExcludePatternList,
                      DeltaSourceList                 *deltaSourceList,
                      JobOptions                      *jobOptions,
                      ArchiveTypes                    archiveType,
                      ConstString                     scheduleTitle,
                      ConstString                     scheduleCustomText,
                      ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                      void                            *archiveGetCryptPasswordUserData,
                      CreateStatusInfoFunction        createStatusInfoFunction,
                      void                            *createStatusInfoUserData,
                      StorageRequestVolumeFunction    storageRequestVolumeFunction,
                      void                            *storageRequestVolumeUserData,
                      bool                            *pauseCreateFlag,
                      bool                            *pauseStorageFlag,
                      bool                            *requestedAbortFlag,
                      LogHandle                       *logHandle
                     )
{
  AutoFreeList     autoFreeList;
  StorageSpecifier storageSpecifier;
  CreateInfo       createInfo;
  Thread           collectorSumThread;                 // files collector sum thread
  Thread           collectorThread;                    // files collector thread
  Thread           storageThread;                      // storage thread
  Thread           *createThreads;
  uint             createThreadCount;
  uint             z;
  Errors           error;
  String           incrementalListFileName;
  bool             useIncrementalFileInfoFlag;
  bool             incrementalFileInfoExistFlag;

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
  DEBUG_TESTCODE("Command_create1") { Storage_doneSpecifier(&storageSpecifier); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&storageSpecifier,{ Storage_doneSpecifier(&storageSpecifier); });

  // init create info
  initCreateInfo(&createInfo,
                 &storageSpecifier,
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

  // init threads
  createThreadCount = (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores();
  createThreads = (Thread*)malloc(createThreadCount*sizeof(Thread));
  if (createThreads == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,&createInfo,{ doneCreateInfo(&createInfo); });
  AUTOFREE_ADD(&autoFreeList,createThreads,{ free(createThreads); });

  // init storage
  error = Storage_init(&createInfo.storageHandle,
                       createInfo.storageSpecifier,
                       createInfo.jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK(storageRequestVolumeFunction,storageRequestVolumeUserData),
                       CALLBACK((StorageStatusInfoFunction)updateStorageStatusInfo,&createInfo)
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
  DEBUG_TESTCODE("Command_create2") { Storage_done(&createInfo.storageHandle); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&createInfo.storageHandle,{ Storage_done(&createInfo.storageHandle); });

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
    Dictionary_init(&createInfo.namesDictionary,DICTIONARY_BYTE_FREE,CALLBACK_NULL);
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
#if 0
// NYI: must index be deleted on error?
        // wait for index init
        waitIndexInit(createInfo);

        if (   (indexHandle != NULL)
            && !archiveInfo->jobOptions->noIndexDatabaseFlag
            && !archiveInfo->jobOptions->dryRunFlag
            && !archiveInfo->jobOptions->noStorageFlag
           )
        {
          Storage_indexDiscard(&createInfo.storageIndexHandle);
        }
#endif /* 0 */
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      DEBUG_TESTCODE("Command_create3") { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
      printInfo(1,
                "ok (%lu entries)\n",
                Dictionary_count(&createInfo.namesDictionary)
               );
    }

    useIncrementalFileInfoFlag              = TRUE;
    createInfo.storeIncrementalFileInfoFlag =    (createInfo.archiveType == ARCHIVE_TYPE_FULL)
                                              || (createInfo.archiveType == ARCHIVE_TYPE_INCREMENTAL);
  }

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

  // create new archive
  error = Archive_create(&createInfo.archiveInfo,
                         deltaSourceList,
                         jobOptions,
                         indexHandle,
                         jobUUID,
                         scheduleUUID,
                         archiveType,
                         CALLBACK(NULL,NULL),  // archiveInitFunction
                         CALLBACK(NULL,NULL),  // archiveDoneFunction
                         CALLBACK(getArchiveSize,&createInfo),
                         CALLBACK(storeArchiveFile,&createInfo),
                         CALLBACK(archiveGetCryptPasswordFunction,archiveGetCryptPasswordUserData),
                         logHandle
                        );
  if (error != ERROR_NONE)
  {
    printError("Cannot create archive file '%s' (error: %s)\n",
               Storage_getPrintableNameCString(createInfo.storageSpecifier,NULL),
               Error_getText(error)
              );
#if 0
// NYI: must index be deleted on error?
    // wait for index init
    waitIndexInit(createInfo);

    if (   (indexHandle != NULL)
        && !createInfo.archiveInfo->jobOptions->noIndexDatabaseFlag
        && !createInfo.archiveInfo->jobOptions->dryRunFlag
        && !createInfo.archiveInfo->jobOptions->noStorageFlag
       )
    {
      Storage_closeIndex(&createInfo.storageIndexHandle);
    }
#endif /* 0 */
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE("command_create4") { Archive_close(&createInfo.archiveInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  // start create threads
#if 1
  for (z = 0; z < createThreadCount; z++)
  {
    if (!Thread_init(&createThreads[z],"BAR create",globalOptions.niceLevel,createThreadCode,&createInfo))
    {
      HALT_FATAL_ERROR("Cannot initialize create thread!");
    }
  }

  // wait for create threads
  for (z = 0; z < createThreadCount; z++)
  {
    if (!Thread_join(&createThreads[z]))
    {
      HALT_FATAL_ERROR("Cannot stop create thread!");
    }
    Thread_done(&createThreads[z]);
  }
#else
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
createThreadCode(&createInfo);
#endif

  // close archive
  error = Archive_close(&createInfo.archiveInfo);
  if (error != ERROR_NONE)
  {
    printError("Cannot close archive '%s' (error: %s)\n",
               Storage_getPrintableNameCString(createInfo.storageSpecifier,NULL),
               Error_getText(error)
              );
#if 0
// NYI: must index be deleted on error?
    // wait for index init
    waitIndexInit(createInfo);

    if (   (indexHandle != NULL)
        && !createInfo.archiveInfo->jobOptions->noIndexDatabaseFlag
        && !createInfo.archiveInfo->jobOptions->dryRunFlag
        && !createInfo.archiveInfo->jobOptions->noStorageFlag
       )
    {
      Storage_closeIndex(&createInfo.storageIndexHandle);
    }
#endif /* 0 */
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE("command_create5") { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  // signal end of data
  MsgQueue_setEndOfMsg(&createInfo.entryMsgQueue);
  MsgQueue_setEndOfMsg(&createInfo.storageMsgQueue);

  // wait for threads
  if (!Thread_join(&storageThread))
  {
    HALT_INTERNAL_ERROR("Cannot stop storage thread!");
  }
  if (!Thread_join(&collectorThread))
  {
    HALT_INTERNAL_ERROR("Cannot stop collector thread!");
  }
  if (!Thread_join(&collectorSumThread))
  {
    HALT_INTERNAL_ERROR("Cannot stop collector sum thread!");
  }

  // final update of status info
  (void)updateStatusInfo(&createInfo);

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
    DEBUG_TESTCODE("command_create3") { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

    printInfo(1,"ok\n");
    logMessage(logHandle,LOG_TYPE_ALWAYS,"Updated incremental file '%s'\n",String_cString(incrementalListFileName));
  }

  // output statics
  if (createInfo.failError == ERROR_NONE)
  {
    printInfo(1,
              "%lu entries/%.1lf%s (%llu bytes) included\n",
              createInfo.statusInfo.doneEntries,
              BYTES_SHORT(createInfo.statusInfo.doneBytes),
              BYTES_UNIT(createInfo.statusInfo.doneBytes),
              createInfo.statusInfo.doneBytes
             );
    printInfo(2,
              "%lu entries/%.1lf%s (%llu bytes) skipped\n",
              createInfo.statusInfo.skippedEntries,
              BYTES_SHORT(createInfo.statusInfo.skippedBytes),
              BYTES_UNIT(createInfo.statusInfo.skippedBytes),
              createInfo.statusInfo.skippedBytes
             );
    printInfo(2,
              "%lu entries/%.1lf%s (%llu bytes) with errors\n",
              createInfo.statusInfo.errorEntries,
              BYTES_SHORT(createInfo.statusInfo.errorBytes),
              BYTES_UNIT(createInfo.statusInfo.errorBytes),
              createInfo.statusInfo.errorBytes
             );
    logMessage(logHandle,
               LOG_TYPE_ALWAYS,
               "%lu entries/%.1lf%s (%llu bytes) included, %lu entries skipped, %lu entries with errors\n",
               createInfo.statusInfo.doneEntries,
               BYTES_SHORT(createInfo.statusInfo.doneBytes),
               BYTES_UNIT(createInfo.statusInfo.doneBytes),
               createInfo.statusInfo.doneBytes,
               createInfo.statusInfo.skippedEntries,
               createInfo.statusInfo.errorEntries
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
  Storage_done(&createInfo.storageHandle);
  doneCreateInfo(&createInfo);
  free(createThreads);
  Storage_doneSpecifier(&storageSpecifier);
  AutoFree_done(&autoFreeList);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
