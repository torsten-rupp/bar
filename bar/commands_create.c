/***********************************************************************\
*
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
#include "common/cstrings.h"
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
#include "archives.h"
#include "deltasources.h"
#include "crypt.h"
#include "par2.h"
#include "storage.h"
#include "continuous.h"
#include "index/index_storages.h"
#include "index/index_entities.h"
#include "index/index_uuids.h"
#include "index/index_assign.h"

#include "commands_create.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define MAX_ENTRY_MSG_QUEUE 256

// file data buffer size
#define BUFFER_SIZE                   (64*1024)

#define INCREMENTAL_LIST_FILE_ID      "BAR incremental list"
#define INCREMENTAL_LIST_FILE_VERSION 1

/***************************** Datatypes *******************************/

// collector types
typedef enum
{
  COLLECTOR_TYPE_SUM,
  COLLECTOR_TYPE_ENTRIES
} CollectorTypes;

// incremental file state
typedef enum
{
  INCREMENTAL_FILE_STATE_UNKNOWN,
  INCREMENTAL_FILE_STATE_OK,
  INCREMENTAL_FILE_STATE_ADDED,
} IncrementalFileStates;

// incremental data info prefix
typedef struct
{
  IncrementalFileStates state;
  FileCast              cast;
} IncrementalListInfo;

// create info
typedef struct
{
  StorageInfo                 storageInfo;                           // storage info
  IndexHandle                 *indexHandle;
  const char                  *jobUUID;                              // job UUID to store or NULL
  const char                  *scheduleUUID;                         // schedule UUID or NULL
  const char                  *scheduleTitle;                        // schedule title or NULL
  const char                  *entityUUID;                           // entity UUID to store or NULL
  ArchiveTypes                archiveType;                           // archive type to create
  const char                  *customText;                           // custom text or NULL
  const EntryList             *includeEntryList;                     // list of included entries
  const PatternList           *excludePatternList;                   // list of exclude patterns
// TODO: already in storageInfo
  JobOptions                  *jobOptions;
  uint64                      createdDateTime;                       // date/time of created [s]

  LogHandle                   *logHandle;                            // log handle

  bool                        partialFlag;                           // TRUE for create incremental/differential archive
  Dictionary                  namesDictionary;                       // dictionary with files (used for incremental/differental backup)
  bool                        storeIncrementalFileInfoFlag;          // TRUE to store incremental file data

  MsgQueue                    entryMsgQueue;                         // queue with entries to store

  ArchiveHandle               archiveHandle;

  bool                        collectorTotalSumDone;                 // TRUE iff collector sum done

  MsgQueue                    storageMsgQueue;                       // queue with waiting storage files
  Semaphore                   storageInfoLock;                       // lock semaphore for storage info
  struct
  {
    uint                      count;                                 // number of current storage files
    uint64                    bytes;                                 // number of bytes in current storage files
  }                           storage;
  bool                        storageThreadExitFlag;
  StringList                  storageFileList;                       // list with stored storage files

  Errors                      failError;                             // failure error

  IsPauseFunction             isPauseCreateFunction;                 // pause create check callback (can be NULL)
  void                        *isPauseCreateUserData;                // user data for pause create check

  IsAbortedFunction           isAbortedFunction;                     // abort create check callback (can be NULL)
  void                        *isAbortedUserData;                    // user data for abort create check

  FragmentList                runningInfoFragmentList;               // running info fragment list

  RunningInfoFunction         runningInfoFunction;                   // running info callback
  void                        *runningInfoUserData;                  // user data for running info call back
  Semaphore                   runningInfoLock;                       // running info lock
  RunningInfo                 runningInfo;                           // running info
  const FragmentNode          *runningInfoCurrentFragmentNode;       // current fragment node in running info
  uint64                      runningInfoCurrentLastUpdateTimestamp; // timestamp of last update current fragment node
} CreateInfo;

// hard link info
typedef struct
{
  uint       count;                                                  // number of hard links
  StringList nameList;                                               // list of hard linked names
  FileInfo   fileInfo;
} HardLinkInfo;

// format modes
typedef enum
{
  FORMAT_MODE_ARCHIVE_FILE_NAME,
  FORMAT_MODE_PATTERN,
} FormatModes;

// supported file system types
const FileSystemTypes SUPPORTED_FILE_SYSTEM_TYPES[] =
{
  FILE_SYSTEM_TYPE_FAT12,
  FILE_SYSTEM_TYPE_FAT16,
  FILE_SYSTEM_TYPE_FAT32,
  FILE_SYSTEM_TYPE_EXT2,
  FILE_SYSTEM_TYPE_EXT3,
  FILE_SYSTEM_TYPE_EXT4,
  FILE_SYSTEM_TYPE_REISERFS3_5,
  FILE_SYSTEM_TYPE_REISERFS3_6,
  FILE_SYSTEM_TYPE_REISERFS4,
  FILE_SYSTEM_TYPE_EXFAT,
  FILE_SYSTEM_TYPE_XFS
};

// entry types
typedef enum
{
  ENTRY_TYPE_FILE,
  ENTRY_TYPE_IMAGE,
  ENTRY_TYPE_DIRECTORY,
  ENTRY_TYPE_LINK,
  ENTRY_TYPE_HARDLINK,
  ENTRY_TYPE_SPECIAL
} EntryTypes;

// entry message, send from collector thread -> main
typedef struct
{
  EntryTypes type;
  union
  {
    struct
    {
      String     name;                                               // file/image/directory/link/special name
      FileInfo   fileInfo;
      uint       fragmentNumber;                                     // fragment number [0..n-1]
      uint       fragmentCount;                                      // fragment count
      uint64     fragmentOffset;
      uint64     fragmentSize;
    } file;
    struct
    {
      String     name;                                               // file/image/directory/link/special name
      DeviceInfo       deviceInfo;
      FileSystemHandle *fileSystemHandle;                            // file system (NULL for raw images)
      uint             fragmentNumber;                               // fragment number [0..n-1]
      uint             fragmentCount;                                // fragment count
      uint64           fragmentOffset;
      uint64           fragmentSize;
    } image;
    struct
    {
      String     name;                                               // file/image/directory/link/special name
      FileInfo   fileInfo;
    } directory;
    struct
    {
      String     name;                                               // file/image/directory/link/special name
      FileInfo   fileInfo;
    } link;
    struct
    {
      StringList nameList;                                           // list of hard link names
      FileInfo   fileInfo;
      uint       fragmentNumber;                                     // fragment number [0..n-1]
      uint       fragmentCount;                                      // fragment count
      uint64     fragmentOffset;
      uint64     fragmentSize;
    } hardLink;
    struct
    {
      String     name;                                               // file/image/directory/link/special name
      FileInfo   fileInfo;
      union
      {
        struct
        {
          DeviceInfo deviceInfo;
          uint       fragmentNumber;                                 // fragment number [0..n-1]
          uint       fragmentCount;                                  // fragment count
          uint64     fragmentOffset;
          uint64     fragmentSize;
        };
      };
    } special;
  };
} EntryMsg;

// storage message, send from create threads -> storage thread
typedef struct
{
  IndexId      uuidId;
  IndexId      entityId;
//  ArchiveTypes archiveType;
  IndexId      storageId;
  String       intermediateFileName;                                 // intermediate archive file name
  uint64       intermediateFileSize;                                 // intermediate archive size [bytes]
  String       archiveName;                                          // destination archive name
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

  switch (entryMsg->type)
  {
    case ENTRY_TYPE_FILE:
      String_delete(entryMsg->file.name);
      break;
    case ENTRY_TYPE_IMAGE:
      String_delete(entryMsg->image.name);
      break;
    case ENTRY_TYPE_DIRECTORY:
      String_delete(entryMsg->directory.name);
      break;
    case ENTRY_TYPE_LINK:
      String_delete(entryMsg->link.name);
      break;
    case ENTRY_TYPE_HARDLINK:
      StringList_done(&entryMsg->hardLink.nameList);
      break;
    case ENTRY_TYPE_SPECIAL:
      String_delete(entryMsg->special.name);
      break;
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
*          runningInfoFunction        - running info call back function
*                                       (can be NULL)
*          runningInfoUserData        - user data for running info
*                                       function
*          isAbortedFunction          - is abort check callback (can be
*                                       NULL)
*          isAbortedUserData          - user data for is aborted check
*          logHandle                  - log handle (can be NULL)
* Output : createInfo - initialized create info variable
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initCreateInfo(CreateInfo          *createInfo,
                          IndexHandle         *indexHandle,
                          const char          *jobUUID,
                          const char          *scheduleUUID,
                          const char          *scheduleTitle,
                          const char          *entityUUID,
                          ArchiveTypes        archiveType,
                          const EntryList     *includeEntryList,
                          const PatternList   *excludePatternList,
                          const char          *customText,
                          JobOptions          *jobOptions,
                          uint64              createdDateTime,
                          IsPauseFunction     isPauseCreateFunction,
                          void                *isPauseCreateUserData,
                          RunningInfoFunction runningInfoFunction,
                          void                *runningInfoUserData,
                          IsAbortedFunction   isAbortedFunction,
                          void                *isAbortedUserData,
                          LogHandle           *logHandle
                         )
{
  assert(createInfo != NULL);

  // init variables
  createInfo->indexHandle                           = indexHandle;
  createInfo->jobUUID                               = jobUUID;
  createInfo->scheduleUUID                          = scheduleUUID;
// TODO: needed?
  createInfo->scheduleTitle                         = scheduleTitle;
  createInfo->entityUUID                            = entityUUID;
  createInfo->includeEntryList                      = includeEntryList;
  createInfo->excludePatternList                    = excludePatternList;
  createInfo->customText                            = customText;
  createInfo->jobOptions                            = jobOptions;
  createInfo->createdDateTime                       = createdDateTime;

  createInfo->logHandle                             = logHandle;

  Dictionary_init(&createInfo->namesDictionary,DICTIONARY_BYTE_INIT_ENTRY,DICTIONARY_BYTE_DONE_ENTRY,DICTIONARY_BYTE_COMPARE_ENTRY);

  createInfo->storeIncrementalFileInfoFlag          = FALSE;

  createInfo->collectorTotalSumDone                 = FALSE;

  createInfo->storage.count                         = 0;
  createInfo->storage.bytes                         = 0LL;
  createInfo->storageThreadExitFlag                 = FALSE;
  StringList_init(&createInfo->storageFileList);

  createInfo->failError                             = ERROR_NONE;

  createInfo->runningInfoFunction                   = runningInfoFunction;
  createInfo->runningInfoUserData                   = runningInfoUserData;
  FragmentList_init(&createInfo->runningInfoFragmentList);
  createInfo->runningInfoCurrentFragmentNode        = NULL;
  createInfo->runningInfoCurrentLastUpdateTimestamp = 0LL;

  createInfo->isPauseCreateFunction                 = isPauseCreateFunction;
  createInfo->isPauseCreateUserData                 = isPauseCreateUserData;

  createInfo->isAbortedFunction                     = isAbortedFunction;
  createInfo->isAbortedUserData                     = isAbortedUserData;

  initRunningInfo(&createInfo->runningInfo);

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
  if (!Semaphore_init(&createInfo->runningInfoLock,SEMAPHORE_TYPE_BINARY))
  {
    HALT_FATAL_ERROR("Cannot initialize running info semaphore!");
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

  Semaphore_done(&createInfo->runningInfoLock);
  Semaphore_done(&createInfo->storageInfoLock);

  MsgQueue_done(&createInfo->storageMsgQueue);
  MsgQueue_done(&createInfo->entryMsgQueue);

  doneRunningInfo(&createInfo->runningInfo);
  FragmentList_done(&createInfo->runningInfoFragmentList);
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

  Errors error;

  assert(createInfo != NULL);
  assert(fileName != NULL);
  assert(namesDictionary != NULL);

  // open file
  FileHandle fileHandle;
  error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // read and check header
  char id[32];
  error = File_read(&fileHandle,id,sizeof(id),NULL);
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    return error;
  }
  if (!stringEquals(id,INCREMENTAL_LIST_FILE_ID))
  {
    File_close(&fileHandle);
    return ERROR_NOT_AN_INCREMENTAL_FILE;
  }
  uint16 version;
  error = File_read(&fileHandle,&version,sizeof(version),NULL);
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    return error;
  }
  if (version != INCREMENTAL_LIST_FILE_VERSION)
  {
    File_close(&fileHandle);
    return ERROR_WRONG_INCREMENTAL_FILE_VERSION;
  }

  // read entries
  void *keyData = malloc(MAX_KEY_DATA);
  if (keyData == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  Dictionary_clear(namesDictionary);
  while (!File_eof(&fileHandle) && !isAborted(createInfo))
  {
    // read entry
    IncrementalListInfo incrementalListInfo;
    incrementalListInfo.state = INCREMENTAL_FILE_STATE_UNKNOWN;
    error = File_read(&fileHandle,&incrementalListInfo.cast,sizeof(incrementalListInfo.cast),NULL);
    if (error != ERROR_NONE) break;
    uint16 keyLength;
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
  free(keyData);

  // close file
  File_close(&fileHandle);

  // free resources

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
  Errors error;

  assert(createInfo != NULL);
  assert(fileName != NULL);
  assert(namesDictionary != NULL);

  // get directory of .bid file
  String directoryName = File_getDirectoryName(String_new(),fileName);

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
  String tmpFileName = String_new();
  error = File_getTmpFileName(tmpFileName,"bid",directoryName);
  if (error != ERROR_NONE)
  {
    String_delete(tmpFileName);
    String_delete(directoryName);
    return error;
  }

  // open file new .bid file
  FileHandle fileHandle;
  error = File_open(&fileHandle,tmpFileName,FILE_OPEN_CREATE);
  if (error != ERROR_NONE)
  {
    File_delete(tmpFileName,FALSE);
    String_delete(tmpFileName);
    String_delete(directoryName);
    return error;
  }

  // write header
  char id[32];
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
  uint16 version = INCREMENTAL_LIST_FILE_VERSION;
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
  DictionaryIterator dictionaryIterator;
  Dictionary_initIterator(&dictionaryIterator,namesDictionary);
  const void         *keyData;
  ulong              keyLength;
  void               *data;
  ulong              length;
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

    const IncrementalListInfo *incrementalListInfo = (IncrementalListInfo*)data;
    error = File_write(&fileHandle,&incrementalListInfo->cast,sizeof(incrementalListInfo->cast));
    if (error != ERROR_NONE) break;
    uint16 n = (uint16)keyLength;
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
  assert(namesDictionary != NULL);
  assert(fileName != NULL);
  assert(fileInfo != NULL);

  // check if exists
  union
  {
    void                *value;
    IncrementalListInfo *incrementalListInfo;
  } data;
  ulong length;
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
  String s = String_new();
  printInfo(2,"Include '%s':\n",String_cString(name));
  printInfo(2,"  new: %s\n",String_cString(File_castToString(String_clear(s),fileCast)));
  union
  {
    void                *value;
    IncrementalListInfo *incrementalListInfo;
  } data;
  ulong length;
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
  assert(namesDictionary != NULL);
  assert(fileName != NULL);
  assert(fileInfo != NULL);

  IncrementalListInfo incrementalListInfo;
  incrementalListInfo.state = INCREMENTAL_FILE_STATE_ADDED;
  memCopyFast(&incrementalListInfo.cast,sizeof(incrementalListInfo.cast),&fileInfo->cast,sizeof(FileCast));

  Dictionary_add(namesDictionary,
                 String_cString(fileName),
                 String_length(fileName),
                 &incrementalListInfo,
                 sizeof(incrementalListInfo)
                );
}

/***********************************************************************\
* Name   : updateRunningInfo
* Purpose: update running info
* Input  : createInfo  - create info
*          forceUpdate - true to force update
* Output : -
* Return : -
* Notes  : Update only every 500ms or if forced
\***********************************************************************/

LOCAL void updateRunningInfo(CreateInfo *createInfo, bool forceUpdate)
{
  static uint64 lastTimestamp = 0LL;

  assert(createInfo != NULL);
  assert(Semaphore_isLocked(&createInfo->runningInfoLock));

  if (createInfo->runningInfoFunction != NULL)
  {
    uint64 timestamp = Misc_getTimestamp();
    if (forceUpdate || (timestamp > (lastTimestamp+500LL*US_PER_MS)))
    {
      createInfo->runningInfoFunction(createInfo->failError,
                                      &createInfo->runningInfo,
                                      createInfo->runningInfoUserData
                                     );
      lastTimestamp = timestamp;
    }
  }
}

/***********************************************************************\
* Name   : runningInfoUpdateLock
* Purpose: lock running info update
* Input  : createInfo   - create info structure
*          fragmentNode - fragment node (can be NULL)
* Output : -
* Return : always TRUE
* Notes  : -
\***********************************************************************/

LOCAL SemaphoreLock runningInfoUpdateLock(CreateInfo *createInfo, ConstString name, FragmentNode **foundFragmentNode)
{
  assert(createInfo != NULL);

  // lock
  Semaphore_lock(&createInfo->runningInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);

  if (foundFragmentNode != NULL)
  {
    // find fragment node
    (*foundFragmentNode) = FragmentList_find(&createInfo->runningInfoFragmentList,name);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : runningInfoUpdateUnlock
* Purpose: running info update unlock
* Input  : createInfo - create info structure
*          name       - name of entry
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void runningInfoUpdateUnlock(CreateInfo *createInfo, ConstString name)
{
  assert(createInfo != NULL);

  if (name != NULL)
  {
    // update current running info if not set or on timeout
    if (   (createInfo->runningInfoCurrentFragmentNode == NULL)
        || ((Misc_getTimestamp()-createInfo->runningInfoCurrentLastUpdateTimestamp) >= 10*US_PER_S)
       )
    {
      // find fragment node
      const FragmentNode *fragmentNode = FragmentList_find(&createInfo->runningInfoFragmentList,name);

      // set new current running info
      String_set(createInfo->runningInfo.progress.entry.name,name);
      if (fragmentNode != NULL)
      {
        createInfo->runningInfo.progress.entry.doneSize  = FragmentList_getSize(fragmentNode);
        createInfo->runningInfo.progress.entry.totalSize = FragmentList_getTotalSize(fragmentNode);

        createInfo->runningInfoCurrentFragmentNode = !FragmentList_isComplete(fragmentNode) ? fragmentNode : NULL;
      }
      else
      {
        createInfo->runningInfoCurrentFragmentNode = NULL;
      }

      // save last update time
      createInfo->runningInfoCurrentLastUpdateTimestamp = Misc_getTimestamp();
    }
  }

  // update running info
  updateRunningInfo(createInfo,TRUE);

  // unlock
  Semaphore_unlock(&createInfo->runningInfoLock);
}

//TODO: comment
#define STATUS_INFO_GET(createInfo,name) \
  for (SemaphoreLock semaphoreLock = Semaphore_lock(&createInfo->runningInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER); \
       semaphoreLock; \
       Semaphore_unlock(&createInfo->runningInfoLock), semaphoreLock = FALSE \
      )

/***********************************************************************\
* Name   : STATUS_INFO_UPDATE
* Purpose: update running info
* Input  : createInfo   - create info structure
*          name         - name of entry
*          fragmentNode - fragment node variable (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define STATUS_INFO_UPDATE(createInfo,name,fragmentNode) \
  for (SemaphoreLock semaphoreLock = runningInfoUpdateLock(createInfo,name,fragmentNode); \
       semaphoreLock; \
       runningInfoUpdateUnlock(createInfo,name), semaphoreLock = FALSE \
      )

/***********************************************************************\
* Name   : updateStorageProgress
* Purpose: update storage progress data
* Input  : doneSize     - done size [bytes]
*          volumeNumber - volume number [1..n]
*          volumeDone   - volume done [0..100%]
*          messageCode  - message code; see MESSAGE_CODE_...
*          messageText  - message text
*          userData     - user data
* Output : -
* Return : TRUE to continue, FALSE to abort
* Notes  : -
\***********************************************************************/

LOCAL bool updateStorageProgress(uint64       doneSize,
                                 uint         volumeNumber,
                                 double       volumeDone,
                                 MessageCodes messageCode,
                                 ConstString  messageText,
                                 void         *userData
                                )
{
  CreateInfo *createInfo = (CreateInfo*)userData;
  assert(createInfo != NULL);
  STATUS_INFO_UPDATE(createInfo,NULL,NULL)
  {
    createInfo->runningInfo.progress.storage.doneSize = doneSize;
    createInfo->runningInfo.progress.volume.number    = volumeNumber;
    createInfo->runningInfo.progress.volume.done      = volumeDone;
    messageSet(&createInfo->runningInfo.message,messageCode,messageText);
  }

  return !isAborted(createInfo);
}

/***********************************************************************\
* Name   : appendFileToEntryList
* Purpose: append file to entry list
* Input  : entryMsgQueue    - entry message queue
*          name             - name (will be copied!)
*          fileInfo         - file info
*          maxFragmentSize  - max. fragment size or 0
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendFileToEntryList(CreateInfo     *createInfo,
                                 ConstString    name,
                                 const FileInfo *fileInfo,
                                 uint64         maxFragmentSize
                                )
{
  assert(createInfo != NULL);
  assert(name != NULL);
  assert(fileInfo != NULL);

  uint   fragmentCount  = (maxFragmentSize > 0LL)
                            ? (fileInfo->size+maxFragmentSize-1)/maxFragmentSize
                            : 1;
  uint   fragmentNumber = 0;
  uint64 fragmentOffset = 0LL;
  do
  {
    // calculate fragment size
    uint64 fragmentSize = ((maxFragmentSize > 0LL) && ((fileInfo->size-fragmentOffset) > maxFragmentSize))
                            ? maxFragmentSize
                            : fileInfo->size-fragmentOffset;

    // init
    EntryMsg entryMsg;
    entryMsg.type                = ENTRY_TYPE_FILE;
    entryMsg.file.name           = String_duplicate(name);
    memCopyFast(&entryMsg.file.fileInfo,sizeof(entryMsg.file.fileInfo),fileInfo,sizeof(FileInfo));
    entryMsg.file.fragmentNumber = fragmentNumber;
    entryMsg.file.fragmentCount  = fragmentCount;
    entryMsg.file.fragmentOffset = fragmentOffset;
    entryMsg.file.fragmentSize   = fragmentSize;

    // put into message queue
    if (!MsgQueue_put(&createInfo->entryMsgQueue,&entryMsg,sizeof(entryMsg)))
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
*          name             - name (will be copied!)
*          deviceInfo       - device info
*          maxFragmentSize  - max. fragment size or 0
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendImageToEntryList(CreateInfo       *createInfo,
                                  ConstString      name,
                                  const DeviceInfo *deviceInfo,
                                  uint64           maxFragmentSize
                                 )
{

  assert(createInfo != NULL);
  assert(name != NULL);
  assert(deviceInfo != NULL);

  uint   fragmentCount  = (maxFragmentSize > 0LL)
                            ? (deviceInfo->size+maxFragmentSize-1)/maxFragmentSize
                            : 1;
  uint   fragmentNumber = 0;
  uint64 fragmentOffset = 0LL;
  do
  {
    // calculate fragment size
    uint64 fragmentSize = ((maxFragmentSize > 0LL) && ((deviceInfo->size-fragmentOffset) > maxFragmentSize))
                            ? maxFragmentSize
                            : deviceInfo->size-fragmentOffset;

    // init
    EntryMsg entryMsg;
    entryMsg.type                   = ENTRY_TYPE_IMAGE;
    entryMsg.image.name             = String_duplicate(name);
    memCopyFast(&entryMsg.image.deviceInfo,sizeof(entryMsg.image.deviceInfo),deviceInfo,sizeof(DeviceInfo));
    entryMsg.image.fragmentNumber   = fragmentNumber;
    entryMsg.image.fragmentCount    = fragmentCount;
    entryMsg.image.fragmentOffset   = fragmentOffset;
    entryMsg.image.fragmentSize     = fragmentSize;

    // put into message queue
    if (!MsgQueue_put(&createInfo->entryMsgQueue,&entryMsg,sizeof(entryMsg)))
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
*          name          - name (will be copied!)
*          fileInfo      - file info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendDirectoryToEntryList(CreateInfo     *createInfo,
                                      ConstString    name,
                                      const FileInfo *fileInfo
                                     )
{
  assert(createInfo != NULL);
  assert(name != NULL);
  assert(fileInfo != NULL);

  // init
  EntryMsg entryMsg;
  entryMsg.type           = ENTRY_TYPE_DIRECTORY;
  entryMsg.directory.name = String_duplicate(name);
  memCopyFast(&entryMsg.directory.fileInfo,sizeof(entryMsg.directory.fileInfo),fileInfo,sizeof(FileInfo));

  // put into message queue
  if (!MsgQueue_put(&createInfo->entryMsgQueue,&entryMsg,sizeof(entryMsg)))
  {
    freeEntryMsg(&entryMsg,NULL);
  }
}

/***********************************************************************\
* Name   : appendLinkToEntryList
* Purpose: append link to entry list
* Input  : entryMsgQueue - entry message queue
*          name          - name (will be copied!)
*          fileInfo      - file info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendLinkToEntryList(CreateInfo     *createInfo,
                                 ConstString    name,
                                 const FileInfo *fileInfo
                                )
{
  assert(createInfo != NULL);
  assert(name != NULL);
  assert(fileInfo != NULL);

  // init
  EntryMsg entryMsg;
  entryMsg.type      = ENTRY_TYPE_LINK;
  entryMsg.link.name = String_duplicate(name);
  memCopyFast(&entryMsg.link.fileInfo,sizeof(entryMsg.link.fileInfo),fileInfo,sizeof(FileInfo));

  // put into message queue
  if (!MsgQueue_put(&createInfo->entryMsgQueue,&entryMsg,sizeof(entryMsg)))
  {
    freeEntryMsg(&entryMsg,NULL);
  }
}

/***********************************************************************\
* Name   : appendHardLinkToEntryList
* Purpose: append hard link to entry list
* Input  : entryMsgQueue   - entry message queue
*          nameList        - name list
*          fileInfo        - file info
*          maxFragmentSize - max. fragment size or 0
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendHardLinkToEntryList(CreateInfo     *createInfo,
                                     StringList     *nameList,
                                     const FileInfo *fileInfo,
                                     uint64         maxFragmentSize
                                    )
{
  assert(createInfo != NULL);
  assert(nameList != NULL);
  assert(!StringList_isEmpty(nameList));
  assert(fileInfo != NULL);

  uint   fragmentCount     = (maxFragmentSize > 0LL)
                               ? (fileInfo->size+maxFragmentSize-1)/maxFragmentSize
                               : 1;
  uint   fragmentNumber    = 0;
  uint64 fragmentOffset    = 0LL;
  do
  {
    // calculate fragment size
    uint64 fragmentSize = ((maxFragmentSize > 0LL) && ((fileInfo->size-fragmentOffset) > maxFragmentSize))
                            ? maxFragmentSize
                            : fileInfo->size-fragmentOffset;

    // init
    EntryMsg entryMsg;
    entryMsg.type                    = ENTRY_TYPE_HARDLINK;
    StringList_initDuplicate(&entryMsg.hardLink.nameList,nameList);
    memCopyFast(&entryMsg.hardLink.fileInfo,sizeof(entryMsg.hardLink.fileInfo),fileInfo,sizeof(FileInfo));
    entryMsg.hardLink.fragmentNumber = fragmentNumber;
    entryMsg.hardLink.fragmentCount  = fragmentCount;
    entryMsg.hardLink.fragmentOffset = fragmentOffset;
    entryMsg.hardLink.fragmentSize   = fragmentSize;

    // put into message queue
    if (!MsgQueue_put(&createInfo->entryMsgQueue,&entryMsg,sizeof(entryMsg)))
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
*          name          - name (will be copied!)
*          fileInfo      - file info or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendSpecialToEntryList(CreateInfo       *createInfo,
                                    ConstString      name,
                                    const FileInfo   *fileInfo
                                   )
{
  assert(createInfo != NULL);
  assert(name != NULL);
  assert(fileInfo != NULL);

  // init
  EntryMsg entryMsg;
  entryMsg.type         = ENTRY_TYPE_SPECIAL;
  entryMsg.special.name = String_duplicate(name);
  memCopyFast(&entryMsg.special.fileInfo,sizeof(entryMsg.special.fileInfo),fileInfo,sizeof(FileInfo));

  // put into message queue
  if (!MsgQueue_put(&createInfo->entryMsgQueue,&entryMsg,sizeof(entryMsg)))
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

  assert(fileName != NULL);
  assert(storageSpecifier != NULL);

  // remove all macros and leading and trailing separator characters
  String_clear(fileName);
  ulong i = 0L;
  while (i < String_length(storageSpecifier->archiveName))
  {
    char ch = String_index(storageSpecifier->archiveName,i);
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
* Name   : collector
* Purpose: file collector
* Input  : createInfo    - create info block
*          collectorType - collector type
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void collector(CreateInfo     *createInfo,
                     CollectorTypes collectorType
                    )
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

  auto void freeHardlinkInfo(void *data, ulong length, void *userData);
  void freeHardlinkInfo(void *data, ulong length, void *userData)
  {
    HardLinkInfo *hardLinkInfo = (HardLinkInfo*)data;

    UNUSED_VARIABLE(length);
    UNUSED_VARIABLE(userData);

    StringList_done(&hardLinkInfo->nameList);
  }

  assert(createInfo != NULL);
  assert(createInfo->includeEntryList != NULL);
  assert(createInfo->excludePatternList != NULL);
  assert(createInfo->jobOptions != NULL);

  // initialize variables
  Dictionary         duplicateNamesDictionary;
  if (!Dictionary_init(&duplicateNamesDictionary,DICTIONARY_BYTE_INIT_ENTRY,DICTIONARY_BYTE_DONE_ENTRY,DICTIONARY_BYTE_COMPARE_ENTRY))
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  Dictionary         hardLinksDictionary;
  if (!Dictionary_init(&hardLinksDictionary,DICTIONARY_BYTE_INIT_ENTRY,CALLBACK_(freeHardlinkInfo,NULL),DICTIONARY_BYTE_COMPARE_ENTRY))
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  if (createInfo->archiveType == ARCHIVE_TYPE_CONTINUOUS)
  {
    if (Continuous_isAvailable())
    {
      Errors error;
      // open continuous database
      DatabaseHandle continuousDatabaseHandle;
      error = Continuous_open(&continuousDatabaseHandle);
      if (error == ERROR_NONE)
      {
        // process entries from continuous database
        DatabaseStatementHandle databaseStatementHandle;
        error = Continuous_initList(&databaseStatementHandle,
                                    &continuousDatabaseHandle,
                                    createInfo->jobUUID,
                                    createInfo->scheduleUUID
                                   );
        if (error == ERROR_NONE)
        {
          DatabaseId databaseId;
          String     name = String_new();
          while (   (createInfo->failError == ERROR_NONE)
                 && !isAborted(createInfo)
                 && Continuous_getNext(&databaseStatementHandle,&databaseId,name)
                )
          {
            // pause
            pauseCreate(createInfo);

            // check if file still exists
            if (!File_exists(name))
            {
              continue;
            }

            // read file info
            FileInfo fileInfo;
            error = File_getInfo(&fileInfo,name);
            if (error != ERROR_NONE)
            {
              if (collectorType == COLLECTOR_TYPE_ENTRIES)
              {
                if (createInfo->jobOptions->skipUnreadableFlag)
                {
                  printWarning(_("XXXcannot get info for '%s' (error: %s) - skipped"),String_cString(name),Error_getText(error));
                }
                else
                {
                  printError(_("cannot get info for '%s' (error: %s)"),
                             String_cString(name),
                             Error_getText(error)
                            );
                }
                logMessage(createInfo->logHandle,
                           LOG_TYPE_ENTRY_ACCESS_DENIED,
                           "Access denied '%s' (error: %s)",
                           String_cString(name),
                           Error_getText(error)
                          );

                STATUS_INFO_UPDATE(createInfo,name,NULL)
                {
                  createInfo->runningInfo.progress.error.count++;
                }
              }
              continue;
            }

            // collect entry for storage
            if (createInfo->jobOptions->ignoreNoDumpAttributeFlag || !File_hasAttributeNoDump(&fileInfo))
            {
              switch (fileInfo.type)
              {
                case FILE_TYPE_FILE:
                  if (isInIncludedList(createInfo->includeEntryList,name))
                  {
                    if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                    {
                      // add to known names history
                      Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                      if (   ((globalOptions.continuousMaxSize == 0LL) || fileInfo.size <= globalOptions.continuousMaxSize)
                          && !isInExcludedList(createInfo->excludePatternList,name)
                         )
                      {
                        switch (collectorType)
                        {
                          case COLLECTOR_TYPE_ENTRIES:
                            if ((globalOptions.continuousMaxSize == 0LL) || fileInfo.size <= globalOptions.continuousMaxSize)
                            {
                              // add to entry list
                              if (isPrintInfo(2))
                              {
                                printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
                              }
                              appendFileToEntryList(createInfo,
                                                    name,
                                                    &fileInfo,
                                                    !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
                                                   );
                            }
                            else
                            {
                              logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Size exceeded limit '%s'",String_cString(name));

                              STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                              {
                                createInfo->runningInfo.progress.skipped.count++;
                                createInfo->runningInfo.progress.skipped.size += fileInfo.size;
                              }
                            }
                            break;
                          case COLLECTOR_TYPE_SUM:
                            STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                            {
                              createInfo->runningInfo.progress.total.count++;
                              createInfo->runningInfo.progress.total.size += fileInfo.size;
                            }
                            break;
                        }
                      }
                      else
                      {
                        if (collectorType == COLLECTOR_TYPE_ENTRIES)
                        {
                          logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                          STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                          {
                            createInfo->runningInfo.progress.skipped.count++;
                            createInfo->runningInfo.progress.skipped.size += fileInfo.size;
                          }
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
                    if (isInIncludedList(createInfo->includeEntryList,name))
                    {
                      if (!isInExcludedList(createInfo->excludePatternList,name))
                      {
                        switch (collectorType)
                        {
                          case COLLECTOR_TYPE_ENTRIES:
                            // add to entry list
                            if (isPrintInfo(2))
                            {
                              printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
                            }
                            appendDirectoryToEntryList(createInfo,
                                                       name,
                                                       &fileInfo
                                                      );
                            break;
                          case COLLECTOR_TYPE_SUM:
                            STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                            {
                              createInfo->runningInfo.progress.total.count++;
                            }
                            break;
                        }
                      }
                      else
                      {
                        if (collectorType == COLLECTOR_TYPE_ENTRIES)
                        {
                          logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                          STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                          {
                            createInfo->runningInfo.progress.skipped.count++;
                            createInfo->runningInfo.progress.skipped.size += fileInfo.size;
                          }
                        }
                      }
                    }
                  }
                  break;
                case FILE_TYPE_LINK:
                  if (isInIncludedList(createInfo->includeEntryList,name))
                  {
                    if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                    {
                      if (!isInExcludedList(createInfo->excludePatternList,name))
                      {
                        // add to known names history
                        Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                        switch (collectorType)
                        {
                          case COLLECTOR_TYPE_ENTRIES:
                            // add to entry list
                            if (isPrintInfo(2))
                            {
                              printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
                            }
                            appendLinkToEntryList(createInfo,
                                                  name,
                                                  &fileInfo
                                                 );
                            break;
                          case COLLECTOR_TYPE_SUM:
                            STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                            {
                              createInfo->runningInfo.progress.total.count++;
                            }
                            break;
                        }
                      }
                      else
                      {
                        if (collectorType == COLLECTOR_TYPE_ENTRIES)
                        {
                          logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                          STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                          {
                            createInfo->runningInfo.progress.skipped.count++;
                            createInfo->runningInfo.progress.skipped.size += fileInfo.size;
                          }
                        }
                      }
                    }
                  }
                  break;
                case FILE_TYPE_HARDLINK:
                  if (   ((globalOptions.continuousMaxSize == 0LL) || fileInfo.size <= globalOptions.continuousMaxSize)
                      && isInIncludedList(createInfo->includeEntryList,name)
                     )
                  {
                    if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                    {
                      // add to known names history
                      Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                      if (!isInExcludedList(createInfo->excludePatternList,name))
                      {
                        if (collectorType == COLLECTOR_TYPE_ENTRIES)
                        {
                          if ((globalOptions.continuousMaxSize == 0LL) || fileInfo.size <= globalOptions.continuousMaxSize)
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
                                // found last hardlink

                                switch (collectorType)
                                {
                                  case COLLECTOR_TYPE_ENTRIES:
                                    // add to entry list
                                    appendHardLinkToEntryList(createInfo,
                                                              &data.hardLinkInfo->nameList,
                                                              &data.hardLinkInfo->fileInfo,
                                                              !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
                                                             );
                                    break;
                                  case COLLECTOR_TYPE_SUM:
                                    // update status
                                    STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                                    {
                                      createInfo->runningInfo.progress.total.count++;
                                      createInfo->runningInfo.progress.total.size += fileInfo.size;
                                    }
                                }

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
                              if (isPrintInfo(2))
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

                            STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                            {
                              createInfo->runningInfo.progress.skipped.count++;
                              createInfo->runningInfo.progress.skipped.size += fileInfo.size;
                            }
                          }
                        }
                      }
                      else
                      {
                        if (collectorType == COLLECTOR_TYPE_ENTRIES)
                        {
                          logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                          STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                          {
                            createInfo->runningInfo.progress.skipped.count++;
                            createInfo->runningInfo.progress.skipped.size += fileInfo.size;
                          }
                        }
                      }
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

                      switch (collectorType)
                      {
                        case COLLECTOR_TYPE_ENTRIES:
                          // add to entry list
                          if (isPrintInfo(2))
                          {
                            printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
                          }
                          appendSpecialToEntryList(createInfo,
                                                   name,
                                                   &fileInfo
                                                  );
                          break;
                        case COLLECTOR_TYPE_SUM:
                          STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                          {
                            createInfo->runningInfo.progress.total.count++;
                          }
                          break;
                      }
                    }
                    else
                    {
                      if (collectorType == COLLECTOR_TYPE_ENTRIES)
                      {
                        logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                        STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                        {
                          createInfo->runningInfo.progress.skipped.count++;
                          createInfo->runningInfo.progress.skipped.size += fileInfo.size;
                        }
                      }
                    }
                  }
                  break;
                default:
                  if (collectorType == COLLECTOR_TYPE_ENTRIES)
                  {
                    printInfo(2,"Unknown type of file '%s' - skipped\n",String_cString(name));
                    logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_TYPE_UNKNOWN,"Unknown type '%s'",String_cString(name));

                    STATUS_INFO_UPDATE(createInfo,name,NULL)
                    {
                      createInfo->runningInfo.progress.error.count++;
                      createInfo->runningInfo.progress.error.size += (uint64)fileInfo.size;
                    }
                  }
                  break;
              }
            }

            // mark as stored
            if (collectorType == COLLECTOR_TYPE_ENTRIES)
            {
              error = Continuous_markEntryStored(&continuousDatabaseHandle,databaseId);
              if (error != ERROR_NONE)
              {
                printError(_("cannot mark continuous entry '%s' as stored (error: %s)"),
                           String_cString(name),
                           Error_getText(error)
                          );
                logMessage(createInfo->logHandle,
                           LOG_TYPE_ENTRY_ACCESS_DENIED,
                           "Cannot mark continuous entry '%s' as stored (error: %s)",
                           String_cString(name),
                           Error_getText(error)
                          );

                STATUS_INFO_UPDATE(createInfo,name,NULL)
                {
                  createInfo->runningInfo.progress.error.count++;
                }
                continue;
              }
            }

            // free resources
          }
          String_delete(name);

          Continuous_doneList(&databaseStatementHandle);
        }

        // close continuous database
        Continuous_close(&continuousDatabaseHandle);
      }
      else
      {
        if (collectorType == COLLECTOR_TYPE_ENTRIES)
        {
          printError(_("cannot initialize continuous database (error: %s)!"),
                     Error_getText(error)
                    );
        }
      }
    }
  }
  else
  {

    // process include entries
    StringList      nameList;
    StringList_init(&nameList);
    String path     = String_new();
    String fileName = String_new();
    EntryNode *includeEntryNode = LIST_HEAD(createInfo->includeEntryList);
    while (   (createInfo->failError == ERROR_NONE)
           && !isAborted(createInfo)
           && (includeEntryNode != NULL)
          )
    {
      Errors error;

      // pause
      pauseCreate(createInfo);

      // find base path
      StringTokenizer fileNameTokenizer;
      ConstString     token;
      File_initSplitFileName(&fileNameTokenizer,includeEntryNode->string);
      if (File_getNextSplitFileName(&fileNameTokenizer,&token) && !Pattern_checkIsPattern(token))
      {
        if (!String_isEmpty(token))
        {
          File_setFileName(path,token);
        }
        else
        {
          File_getSystemDirectory(path,FILE_SYSTEM_PATH_ROOT,NULL);
        }
      }
      else
      {
        String_clear(path);
      }
      while (File_getNextSplitFileName(&fileNameTokenizer,&token) && !Pattern_checkIsPattern(token))
      {
        File_appendFileName(path,token);
      }
      File_doneSplitFileName(&fileNameTokenizer);

      // find files starting from base path
      String name = String_new();
      StringList_append(&nameList,path);
      ulong n = 0;
      while (   (createInfo->failError == ERROR_NONE)
             && !isAborted(createInfo)
             && !StringList_isEmpty(&nameList)
            )
      {
        // pause
        pauseCreate(createInfo);

        // get next entry to process
        StringList_removeLast(&nameList,name);

        // read file info
        FileInfo fileInfo;
        if (!String_isEmpty(name))
        {
          String_set(path,name);
        }
        else
        {
          File_getCurrentDirectory(path);
        }
        error = File_getInfo(&fileInfo,path);
        if (error != ERROR_NONE)
        {
          if (collectorType == COLLECTOR_TYPE_ENTRIES)
          {
            if (createInfo->jobOptions->skipUnreadableFlag)
            {
              printWarning(_("cannot get info for '%s' (error: %s) - skipped"),String_cString(path),Error_getText(error));
            }
            else
            {
              printError(_("cannot get info for '%s' (error: %s)"),
                         String_cString(path),
                         Error_getText(error)
                        );
            }
            logMessage(createInfo->logHandle,
                       LOG_TYPE_ENTRY_ACCESS_DENIED,
                       "Access denied '%s' (error: %s)",
                       String_cString(path),
                       Error_getText(error)
                      );

            STATUS_INFO_UPDATE(createInfo,name,NULL)
            {
              createInfo->runningInfo.progress.error.count++;
            }
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
                if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                {
                  // add to known names history
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                  if (!isInExcludedList(createInfo->excludePatternList,name))
                  {
                    switch (includeEntryNode->storeType)
                    {
                      case ENTRY_STORE_TYPE_FILE:
                        if (   !createInfo->partialFlag
                            || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                           )
                        {
                          switch (collectorType)
                          {
                            case COLLECTOR_TYPE_ENTRIES:
                              // add to entry list
                              if (createInfo->partialFlag && isPrintInfo(2))
                              {
                                printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
                              }
                              appendFileToEntryList(createInfo,
                                                    name,
                                                    &fileInfo,
                                                    !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
                                                   );
                              break;
                            case COLLECTOR_TYPE_SUM:
                              STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                              {
                                createInfo->runningInfo.progress.total.count++;
                                createInfo->runningInfo.progress.total.size += fileInfo.size;
                              }
                              break;
                          }
                        }
                        break;
                      case ENTRY_STORE_TYPE_IMAGE:
                        if (collectorType == COLLECTOR_TYPE_ENTRIES)
                        {
                          printWarning(_("'%s' is not a device"),String_cString(name));
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
                    logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                    STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                    {
                      createInfo->runningInfo.progress.skipped.count++;
                      createInfo->runningInfo.progress.skipped.size += fileInfo.size;
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
                    switch (includeEntryNode->storeType)
                    {
                      case ENTRY_STORE_TYPE_FILE:
                        if (   !createInfo->partialFlag
                            || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                           )
                        {
                          switch (collectorType)
                          {
                            case COLLECTOR_TYPE_ENTRIES:
                              // add to entry list
                              if (createInfo->partialFlag && isPrintInfo(2))
                              {
                                printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
                              }
                              appendDirectoryToEntryList(createInfo,
                                                         name,
                                                         &fileInfo
                                                        );
                              break;
                            case COLLECTOR_TYPE_SUM:
                              STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                              {
                                createInfo->runningInfo.progress.total.count++;
                              }
                              break;
                          }
                        }
                        break;
                      case ENTRY_STORE_TYPE_IMAGE:
                        if (collectorType == COLLECTOR_TYPE_ENTRIES)
                        {
                          printWarning(_("'%s' is not a device"),String_cString(name));
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
                    if (collectorType == COLLECTOR_TYPE_ENTRIES)
                    {
                      logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                      STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                      {
                        createInfo->runningInfo.progress.skipped.count++;
                        createInfo->runningInfo.progress.skipped.size += fileInfo.size;
                      }
                    }
                  }
                }

                // open directory contents
                DirectoryListHandle directoryListHandle;
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
                    FileInfo fileInfo;
                    error = File_readDirectoryList(&directoryListHandle,fileName,&fileInfo);
                    if (error != ERROR_NONE)
                    {
                      if (collectorType == COLLECTOR_TYPE_ENTRIES)
                      {
                        printError(_("cannot read directory '%s' (error: %s) - skipped"),String_cString(name),Error_getText(error));
                        logMessage(createInfo->logHandle,
                                   LOG_TYPE_ENTRY_ACCESS_DENIED,
                                   "Access denied '%s' (error: %s)",
                                   String_cString(name),
                                   Error_getText(error)
                                  );

                        STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                        {
                          createInfo->runningInfo.progress.error.count++;
                          createInfo->runningInfo.progress.error.size += fileInfo.size;
                        }
                      }
                      continue;
                    }

                    // add sub-directories to directory search list
                    if (fileInfo.type == FILE_TYPE_DIRECTORY)
                    {
                      StringList_append(&nameList,fileName);
                    }

                    if (isIncluded(includeEntryNode,fileName))
                    {
                      if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName)))
                      {
                        // add to known names history
                        Dictionary_add(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName),NULL,0);

                        if (!isInExcludedList(createInfo->excludePatternList,fileName))
                        {
                          if (createInfo->jobOptions->ignoreNoDumpAttributeFlag || !File_hasAttributeNoDump(&fileInfo))
                          {
                            switch (fileInfo.type)
                            {
                              case FILE_TYPE_FILE:
                                switch (includeEntryNode->storeType)
                                {
                                  case ENTRY_STORE_TYPE_FILE:
                                    if (   !createInfo->partialFlag
                                        || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                       )
                                    {
                                      switch (collectorType)
                                      {
                                        case COLLECTOR_TYPE_ENTRIES:
                                          // add to entry list
                                          if (createInfo->partialFlag && isPrintInfo(2))
                                          {
                                            printIncrementalInfo(&createInfo->namesDictionary,fileName,&fileInfo.cast);
                                          }
                                          appendFileToEntryList(createInfo,
                                                                fileName,
                                                                &fileInfo,
                                                                !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
                                                               );
                                          break;
                                        case COLLECTOR_TYPE_SUM:
                                          STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                                          {
                                            createInfo->runningInfo.progress.total.count++;
                                            createInfo->runningInfo.progress.total.size += fileInfo.size;
                                          }
                                          break;
                                      }
                                    }
                                    break;
                                  case ENTRY_STORE_TYPE_IMAGE:
                                    if (collectorType == COLLECTOR_TYPE_ENTRIES)
                                    {
                                      printWarning(_("'%s' is not a device"),String_cString(fileName));
                                    }
                                    break;
                                  default:
                                    #ifndef NDEBUG
                                      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                                    #endif /* NDEBUG */
                                    break; /* not reached */
                                }
                                break;
                              case FILE_TYPE_DIRECTORY:
                                // nothing to do: directory is appended to include list
                                break;
                              case FILE_TYPE_LINK:
                                switch (includeEntryNode->storeType)
                                {
                                  case ENTRY_STORE_TYPE_FILE:
                                    if (   !createInfo->partialFlag
                                        || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                       )
                                    {
                                      switch (collectorType)
                                      {
                                        case COLLECTOR_TYPE_ENTRIES:
                                          // add to entry list
                                          if (createInfo->partialFlag && isPrintInfo(2))
                                          {
                                            printIncrementalInfo(&createInfo->namesDictionary,fileName,&fileInfo.cast);
                                          }
                                          appendLinkToEntryList(createInfo,
                                                                fileName,
                                                                &fileInfo
                                                               );
                                          break;
                                        case COLLECTOR_TYPE_SUM:
                                          STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                                          {
                                            createInfo->runningInfo.progress.total.count++;
                                          }
                                          break;
                                      }
                                    }
                                    break;
                                  case ENTRY_STORE_TYPE_IMAGE:
                                    if (File_readLink(fileName,name,TRUE) == ERROR_NONE)
                                    {
                                      // get device info
                                      DeviceInfo deviceInfo;
                                      error = Device_getInfo(&deviceInfo,fileName,TRUE);
                                      if (error != ERROR_NONE)
                                      {
                                        if (collectorType == COLLECTOR_TYPE_ENTRIES)
                                        {
                                          if (createInfo->jobOptions->skipUnreadableFlag)
                                          {
                                            printWarning(_("cannot get info for '%s' (error: %s) - skipped"),String_cString(fileName),Error_getText(error));
                                          }
                                          else
                                          {
                                            printError(_("cannot get info for '%s' (error: %s)"),
                                                       String_cString(fileName),
                                                       Error_getText(error)
                                                      );
                                          }
                                          logMessage(createInfo->logHandle,
                                                     LOG_TYPE_ENTRY_ACCESS_DENIED,
                                                     "Access denied '%s' (error: %s)",
                                                     String_cString(fileName),
                                                     Error_getText(error)
                                                    );

                                          STATUS_INFO_UPDATE(createInfo,name,NULL)
                                          {
                                            createInfo->runningInfo.progress.error.count++;
                                          }
                                        }
                                        continue;
                                      }

                                      switch (collectorType)
                                      {
                                        case COLLECTOR_TYPE_ENTRIES:
                                          // add to entry list
                                          appendImageToEntryList(createInfo,
                                                                 name,
                                                                 &deviceInfo,
                                                                 !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
                                                                );
                                          break;
                                        case COLLECTOR_TYPE_SUM:
                                          if (deviceInfo.type == DEVICE_TYPE_BLOCK)
                                          {
                                            STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                                            {
                                              createInfo->runningInfo.progress.total.count++;
                                              createInfo->runningInfo.progress.total.size += deviceInfo.size;
                                            }
                                          }
                                          break;
                                      }
                                    }
                                    break;
                                  default:
                                    #ifndef NDEBUG
                                      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                                    #endif /* NDEBUG */
                                    break; /* not reached */
                                }
                                break;
                              case FILE_TYPE_HARDLINK:
                                switch (includeEntryNode->storeType)
                                {
                                  case ENTRY_STORE_TYPE_FILE:
                                    {
                                      if (   !createInfo->partialFlag
                                          || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
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
                                          StringList_append(&data.hardLinkInfo->nameList,fileName);

                                          if (StringList_count(&data.hardLinkInfo->nameList) >= data.hardLinkInfo->count)
                                          {
                                            // found last hardlink

                                            switch (collectorType)
                                            {
                                              case COLLECTOR_TYPE_ENTRIES:
                                                // add to entry list
                                                appendHardLinkToEntryList(createInfo,
                                                                          &data.hardLinkInfo->nameList,
                                                                          &data.hardLinkInfo->fileInfo,
                                                                          !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
                                                                         );
                                                break;
                                              case COLLECTOR_TYPE_SUM:
                                                // update status
                                                STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                                                {
                                                  createInfo->runningInfo.progress.total.count++;
                                                  createInfo->runningInfo.progress.total.size += fileInfo.size;
                                                }
                                                break;
                                            }

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
                                  case ENTRY_STORE_TYPE_IMAGE:
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
                                switch (includeEntryNode->storeType)
                                {
                                  case ENTRY_STORE_TYPE_FILE:
                                    if (   !createInfo->partialFlag
                                        || isFileChanged(&createInfo->namesDictionary,fileName,&fileInfo)
                                       )
                                    {
                                      switch (collectorType)
                                      {
                                        case COLLECTOR_TYPE_ENTRIES:
                                          // add to entry list
                                          if (createInfo->partialFlag && isPrintInfo(2))
                                          {
                                            printIncrementalInfo(&createInfo->namesDictionary,fileName,&fileInfo.cast);
                                          }
                                          appendSpecialToEntryList(createInfo,
                                                                   fileName,
                                                                   &fileInfo
                                                                  );
                                          break;
                                        case COLLECTOR_TYPE_SUM:
                                          STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                                          {
                                            createInfo->runningInfo.progress.total.count++;
                                            if (   (includeEntryNode->storeType == ENTRY_STORE_TYPE_IMAGE)
                                                && (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                                               )
                                            {
                                              createInfo->runningInfo.progress.total.size += fileInfo.size;
                                            }
                                          }
                                          break;
                                      }
                                    }
                                    break;
                                  case ENTRY_STORE_TYPE_IMAGE:
                                    if (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                                    {
                                      // get device info
                                      DeviceInfo deviceInfo;
                                      error = Device_getInfo(&deviceInfo,fileName,TRUE);
                                      if (error != ERROR_NONE)
                                      {
                                        if (collectorType == COLLECTOR_TYPE_ENTRIES)
                                        {
                                          printError(_("cannot access '%s' (error: %s) - skipped"),String_cString(name),Error_getText(error));
                                          logMessage(createInfo->logHandle,
                                                     LOG_TYPE_ENTRY_ACCESS_DENIED,
                                                     "Access denied '%s' (error: %s)",
                                                     String_cString(name),
                                                     Error_getText(error)
                                                    );

                                          STATUS_INFO_UPDATE(createInfo,name,NULL)
                                          {
                                            createInfo->runningInfo.progress.error.count++;
                                          }
                                        }
                                        continue;
                                      }

                                      switch (collectorType)
                                      {
                                        case COLLECTOR_TYPE_ENTRIES:
                                          // add to entry list
                                          appendImageToEntryList(createInfo,
                                                                 fileName,
                                                                 &deviceInfo,
                                                                 !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
                                                                );
                                          break;
                                        case COLLECTOR_TYPE_SUM:
                                          STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                                          {
                                            createInfo->runningInfo.progress.total.count++;
                                            createInfo->runningInfo.progress.total.size += fileInfo.size;
                                          }
                                          break;
                                      }
                                    }
                                    break;
                                  default:
                                    #ifndef NDEBUG
                                      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                                    #endif /* NDEBUG */
                                    break; /* not reached */
                                }
                                break;
                              default:
                                if (collectorType == COLLECTOR_TYPE_ENTRIES)
                                {
                                  printInfo(2,"Unknown type of file '%s' - skipped\n",String_cString(fileName));
                                  logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_TYPE_UNKNOWN,"Unknown type '%s'",String_cString(fileName));

                                  STATUS_INFO_UPDATE(createInfo,fileName,NULL)
                                  {
                                    createInfo->runningInfo.progress.error.count++;
                                    createInfo->runningInfo.progress.error.size += fileInfo.size;
                                  }
                                }
                                break;
                            }
                          }
                          else
                          {
                            if (collectorType == COLLECTOR_TYPE_ENTRIES)
                            {
                              logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s' (no dump attribute)",String_cString(fileName));

                              STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                              {
                                createInfo->runningInfo.progress.skipped.count++;
                                createInfo->runningInfo.progress.skipped.size += fileInfo.size;
                              }
                            }
                          }
                        }
                        else
                        {
                          if (collectorType == COLLECTOR_TYPE_ENTRIES)
                          {
                            logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(fileName));

                            STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                            {
                              createInfo->runningInfo.progress.skipped.count++;
                              createInfo->runningInfo.progress.skipped.size += fileInfo.size;
                            }
                          }
                        }
                      }
                    }
                  }

                  // close directory
                  File_closeDirectoryList(&directoryListHandle);
                }
                else
                {
                  if (collectorType == COLLECTOR_TYPE_ENTRIES)
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
                      createInfo->runningInfo.progress.error.count++;
                    }
                  }
                }
              }
              else
              {
                if (collectorType == COLLECTOR_TYPE_ENTRIES)
                {
                  logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s' (.nobackup file)",String_cString(name));
                }
              }
              break;
            case FILE_TYPE_LINK:
              if (isIncluded(includeEntryNode,name))
              {
                if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                {
                  // add to known names history
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                  if (!isInExcludedList(createInfo->excludePatternList,name))
                  {
                    switch (includeEntryNode->storeType)
                    {
                      case ENTRY_STORE_TYPE_FILE:
                        if (  !createInfo->partialFlag
                            || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                           )
                        {
                          switch (collectorType)
                          {
                            case COLLECTOR_TYPE_ENTRIES:
                              // add to entry list
                              if (createInfo->partialFlag && isPrintInfo(2))
                              {
                                printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
                              }
                              appendLinkToEntryList(createInfo,
                                                    name,
                                                    &fileInfo
                                                   );
                              break;
                            case COLLECTOR_TYPE_SUM:
                              STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                              {
                                createInfo->runningInfo.progress.total.count++;
                              }
                              break;
                          }
                        }
                        break;
                      case ENTRY_STORE_TYPE_IMAGE:
                        {
                          error = File_readLink(fileName,name,TRUE);
                          if (error != ERROR_NONE)
                          {
                            if (collectorType == COLLECTOR_TYPE_ENTRIES)
                            {
                              if (createInfo->jobOptions->skipUnreadableFlag)
                              {
                                printWarning(_("cannot get info for '%s' (error: %s) - skipped"),String_cString(path),Error_getText(error));
                              }
                              else
                              {
                                printError(_("cannot get info for '%s' (error: %s)"),
                                           String_cString(path),
                                           Error_getText(error)
                                          );
                              }
                              logMessage(createInfo->logHandle,
                                         LOG_TYPE_ENTRY_ACCESS_DENIED,
                                         "Access denied '%s' (error: %s)",
                                         String_cString(path),
                                         Error_getText(error)
                                        );

                              STATUS_INFO_UPDATE(createInfo,name,NULL)
                              {
                                createInfo->runningInfo.progress.error.count++;
                              }
                            }
                            continue;
                          }

                          // get device info
                          DeviceInfo deviceInfo;
                          error = Device_getInfo(&deviceInfo,fileName,TRUE);
                          if (error != ERROR_NONE)
                          {
                            if (collectorType == COLLECTOR_TYPE_ENTRIES)
                            {
                              printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(name),Error_getText(error));
                              logMessage(createInfo->logHandle,
                                         LOG_TYPE_ENTRY_ACCESS_DENIED,
                                         "Access denied '%s' (error: %s)",
                                         String_cString(name),
                                         Error_getText(error)
                                        );
                            }

                            STATUS_INFO_UPDATE(createInfo,name,NULL)
                            {
                              createInfo->runningInfo.progress.error.count++;
                            }
                            continue;
                          }

                          switch (collectorType)
                          {
                            case COLLECTOR_TYPE_ENTRIES:
                              // add to entry list
                              appendImageToEntryList(createInfo,
                                                     name,
                                                     &deviceInfo,
                                                     !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
                                                    );
                              break;
                            case COLLECTOR_TYPE_SUM:
                              if (deviceInfo.type == DEVICE_TYPE_BLOCK)
                              {
                                STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                                {
                                  createInfo->runningInfo.progress.total.count++;
                                  createInfo->runningInfo.progress.total.size += deviceInfo.size;
                                }
                              }
                              break;
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
                  else
                  {
                    logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                    STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                    {
                      createInfo->runningInfo.progress.skipped.count++;
                      createInfo->runningInfo.progress.skipped.size += fileInfo.size;
                    }
                  }
                }
              }
              break;
            case FILE_TYPE_HARDLINK:
              if (isIncluded(includeEntryNode,name))
              {
                if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                {
                  // add to known names history
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                  if (!isInExcludedList(createInfo->excludePatternList,name))
                  {
                    switch (includeEntryNode->storeType)
                    {
                      case ENTRY_STORE_TYPE_FILE:
                        if (   !createInfo->partialFlag
                            || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                            )
                        {
                          union { void *value; HardLinkInfo *hardLinkInfo; } data;
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
                              // found last hardlink
                              switch (collectorType)
                              {
                                case COLLECTOR_TYPE_ENTRIES:
                                  // add to entry list
                                  appendHardLinkToEntryList(createInfo,
                                                            &data.hardLinkInfo->nameList,
                                                            &data.hardLinkInfo->fileInfo,
                                                            !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
                                                           );
                                  break;
                                case COLLECTOR_TYPE_SUM:
                                  // update status
                                  STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                                  {
                                    createInfo->runningInfo.progress.total.count++;
                                    createInfo->runningInfo.progress.total.size += fileInfo.size;
                                  }
                                  break;
                              }

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

                            HardLinkInfo hardLinkInfo;
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
                      case ENTRY_STORE_TYPE_IMAGE:
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

                    STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                    {
                      createInfo->runningInfo.progress.skipped.count++;
                      createInfo->runningInfo.progress.skipped.size += fileInfo.size;
                    }
                  }
                }
              }
              break;
            case FILE_TYPE_SPECIAL:
              if (isIncluded(includeEntryNode,name))
              {
                if (!Dictionary_contains(&duplicateNamesDictionary,String_cString(name),String_length(name)))
                {
                  // add to known names history
                  Dictionary_add(&duplicateNamesDictionary,String_cString(name),String_length(name),NULL,0);

                  if (!isInExcludedList(createInfo->excludePatternList,name))
                  {
                    switch (includeEntryNode->storeType)
                    {
                      case ENTRY_STORE_TYPE_FILE:
                        if (   !createInfo->partialFlag
                            || isFileChanged(&createInfo->namesDictionary,name,&fileInfo)
                           )
                        {
                          switch (collectorType)
                          {
                            case COLLECTOR_TYPE_ENTRIES:
                              // add to entry list
                              if (createInfo->partialFlag && isPrintInfo(2))
                              {
                                printIncrementalInfo(&createInfo->namesDictionary,name,&fileInfo.cast);
                              }
                              appendSpecialToEntryList(createInfo,
                                                       name,
                                                       &fileInfo
                                                      );
                              break;
                            case COLLECTOR_TYPE_SUM:
                              STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                              {
                                createInfo->runningInfo.progress.total.count++;
                              }
                              break;
                          }
                        }
                        break;
                      case ENTRY_STORE_TYPE_IMAGE:
                        if (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                        {
                          // get device info
                          DeviceInfo deviceInfo;
                          error = Device_getInfo(&deviceInfo,name,TRUE);
                          if (error != ERROR_NONE)
                          {
                            if (collectorType == COLLECTOR_TYPE_ENTRIES)
                            {
                              if (createInfo->jobOptions->skipUnreadableFlag)
                              {
                                printWarning(_("cannot get info for '%s' (error: %s) - skipped"),String_cString(name),Error_getText(error));
                              }
                              else
                              {
                                printError(_("cannot get info for '%s' (error: %s)"),
                                           String_cString(name),
                                           Error_getText(error)
                                          );
                              }
                              logMessage(createInfo->logHandle,
                                         LOG_TYPE_ENTRY_ACCESS_DENIED,
                                         "Access denied '%s' (error: %s)",
                                         String_cString(name),
                                         Error_getText(error)
                                        );

                              STATUS_INFO_UPDATE(createInfo,name,NULL)
                              {
                                createInfo->runningInfo.progress.error.count++;
                              }
                            }
                            continue;
                          }

                          switch (collectorType)
                          {
                            case COLLECTOR_TYPE_ENTRIES:
                              // add to entry list
                              appendImageToEntryList(createInfo,
                                                     name,
                                                     &deviceInfo,
                                                     !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
                                                    );
                              break;
                            case COLLECTOR_TYPE_SUM:
                              STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                              {
                                createInfo->runningInfo.progress.total.count++;
                                createInfo->runningInfo.progress.total.size += deviceInfo.size;
                              }
                              break;
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
                  else
                  {
                    logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s'",String_cString(name));

                    STATUS_INFO_UPDATE(createInfo,NULL,NULL)
                    {
                      createInfo->runningInfo.progress.skipped.count++;
                      createInfo->runningInfo.progress.skipped.size += fileInfo.size;
                    }
                  }
                }
              }
              break;
            default:
              if (collectorType == COLLECTOR_TYPE_ENTRIES)
              {
                printInfo(2,"Unknown type of file '%s' - skipped\n",String_cString(name));
                logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_TYPE_UNKNOWN,"Unknown type '%s'",String_cString(name));

                STATUS_INFO_UPDATE(createInfo,name,NULL)
                {
                  createInfo->runningInfo.progress.error.count++;
                  createInfo->runningInfo.progress.error.size += fileInfo.size;
                }
              }
              break;
          }
        }
        else
        {
          if (collectorType == COLLECTOR_TYPE_ENTRIES)
          {
            logMessage(createInfo->logHandle,LOG_TYPE_ENTRY_EXCLUDED,"Excluded '%s' (no dump attribute)",String_cString(name));

            STATUS_INFO_UPDATE(createInfo,NULL,NULL)
            {
              createInfo->runningInfo.progress.skipped.count++;
              createInfo->runningInfo.progress.skipped.size += fileInfo.size;
            }
          }
        }

        // free resources
      }
      String_delete(name);
      if (collectorType == COLLECTOR_TYPE_ENTRIES)
      {
        if (n <= 0)
        {
          if (!createInfo->jobOptions->skipUnreadableFlag)
          {
            if (collectorType == COLLECTOR_TYPE_ENTRIES)
            {
              printError(_("no matching entry found for '%s'!"),
                         String_cString(includeEntryNode->string)
                        );
              logMessage(createInfo->logHandle,
                         LOG_TYPE_ENTRY_MISSING,
                         "No matching entry found for '%s'",
                         String_cString(includeEntryNode->string)
                        );
            }
            createInfo->failError = ERRORX_(FILE_NOT_FOUND_,0,"%s",String_cString(includeEntryNode->string));
          }

          STATUS_INFO_UPDATE(createInfo,NULL,NULL)
          {
            createInfo->runningInfo.progress.error.count++;
          }
        }
      }

      // next include entry
      includeEntryNode = includeEntryNode->next;
    }

    // free resoures
    String_delete(fileName);
    String_delete(path);
    StringList_done(&nameList);
  }

  if (collectorType == COLLECTOR_TYPE_ENTRIES)
  {
    // add incomplete hard link entries (not all hard links found) to entry list
    DictionaryIterator dictionaryIterator;
    Dictionary_initIterator(&dictionaryIterator,&hardLinksDictionary);
//???
union { const void *value; const uint64 *id; } keyData;
union { void *value; HardLinkInfo *hardLinkInfo; } data;
    while (Dictionary_getNext(&dictionaryIterator,
                              &keyData.value,
                              NULL,
                              &data.value,
                              NULL
                             )
          )
    {
      appendHardLinkToEntryList(createInfo,
                                &data.hardLinkInfo->nameList,
                                &data.hardLinkInfo->fileInfo,
                                !createInfo->jobOptions->noStorage ? globalOptions.fragmentSize : 0LL
                               );
    }
    Dictionary_doneIterator(&dictionaryIterator);
  }

  // free resoures
  Dictionary_done(&hardLinksDictionary);
  Dictionary_done(&duplicateNamesDictionary);
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
  collector(createInfo,COLLECTOR_TYPE_SUM);

  // set collector done
  STATUS_INFO_UPDATE(createInfo,NULL,NULL)
  {
    createInfo->runningInfo.progress.collectTotalSumDone = TRUE;
  }

  createInfo->collectorTotalSumDone = TRUE;
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
  collector(createInfo,COLLECTOR_TYPE_ENTRIES);
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
          messageSet(&createInfo->runningInfo.message,MESSAGE_CODE_WAIT_FOR_TEMPORARY_SPACE,NULL);
          String_clear(createInfo->runningInfo.message.text);
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
          messageClear(&createInfo->runningInfo.message);
          String_clear(createInfo->runningInfo.message.text);
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
  assert(storageInfo != NULL);

  UNUSED_VARIABLE(storageId);

  uint64 archiveSize = 0LL;

  Errors error;

  // get archive file name (expand macros)
  String     archiveName = String_new();
  CreateInfo *createInfo = (CreateInfo*)userData;
  assert(createInfo != NULL);
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
  StorageHandle storageHandle;
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
  Errors error;

  // open archive
  ArchiveHandle archiveHandle;
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
    ArchiveEntryTypes archiveEntryType;
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
          {
            ArchiveEntryInfo archiveEntryInfo;
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
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            ArchiveEntryInfo archiveEntryInfo;
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
          }
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          {
            ArchiveEntryInfo archiveEntryInfo;
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
          }
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          {
            ArchiveEntryInfo archiveEntryInfo;
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
          }
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          {
            ArchiveEntryInfo archiveEntryInfo;
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
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          {
            ArchiveEntryInfo archiveEntryInfo;
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
          }
          break;
        case ARCHIVE_ENTRY_TYPE_META:
          {
// TODO: read meta
            error = Archive_skipNextEntry(&archiveHandle);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SALT:
// TODO: read salt
          {
          }
          break;
        case ARCHIVE_ENTRY_TYPE_KEY:
// TODO: read key
          {
            #ifndef NDEBUG
              HALT_INTERNAL_ERROR_UNREACHABLE();
            #else
              error = Archive_skipNextEntry(&archiveHandle);
            #endif /* NDEBUG */
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SIGNATURE:
// TODO: read signature
          {
            error = Archive_skipNextEntry(&archiveHandle);
          }
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
  Errors error;

  assert(storageInfo != NULL);
  assert(!String_isEmpty(intermediateFileName));

  UNUSED_VARIABLE(jobUUID);
  UNUSED_VARIABLE(entityUUID);

  // get archive file name (expand macros)
  String     archiveName = String_new();
  CreateInfo *createInfo = (CreateInfo*)userData;
  assert(createInfo != NULL);
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
  StorageMsg storageMsg;
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

  // update running info
  STATUS_INFO_UPDATE(createInfo,NULL,NULL)
  {
    createInfo->runningInfo.progress.storage.totalSize += intermediateFileSize;
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
  assert(jobUUID != NULL);
  assert(maxStorageSize > 0LL);

  if (indexHandle != NULL)
  {
    String           storageName       = String_new();
    String           oldestStorageName = String_new();
    StorageSpecifier storageSpecifier;
    Storage_initSpecifier(&storageSpecifier);
    String           dateTime          = String_new();
    uint64           totalStorageSize  = 0LL;
    IndexId          oldestStorageId;
    do
    {
      oldestStorageId  = INDEX_ID_NONE;

      Errors error;

      // get total storage size, find oldest storage entry
      IndexId          oldestUUIDId          = INDEX_ID_NONE;
      IndexId          oldestEntityId        = INDEX_ID_NONE;
      String_clear(oldestStorageName);
      uint64           oldestCreatedDateTime = MAX_UINT64;
      uint64           oldestSize            = 0LL;
      IndexQueryHandle indexQueryHandle;
      error = IndexStorage_initList(&indexQueryHandle,
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
      IndexId uuidId;
      IndexId entityId;
      IndexId storageId;
      uint64  createdDateTime;
      uint64  size;
      while (IndexStorage_getNext(&indexQueryHandle,
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
      if (!INDEX_ID_IS_NONE((oldestStorageId)) && (totalStorageSize > limit))
      {
        // delete oldest storage entry
        error = Storage_parseName(&storageSpecifier,oldestStorageName);
        if (error == ERROR_NONE)
        {
          StorageInfo storageInfo;
          error = Storage_init(&storageInfo,
//TODO
NULL, // masterIO
                               &storageSpecifier,
                               NULL,  // jobOptions
                               &globalOptions.indexDatabaseMaxBandWidthList,
                               SERVER_CONNECTION_PRIORITY_HIGH,
                               CALLBACK_(NULL,NULL),  // storageUpdateProgress
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

            Storage_done(&storageInfo);
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
        if (!INDEX_ID_IS_NONE(oldestEntityId))
        {
          (void)IndexEntity_prune(indexHandle,NULL,NULL,oldestEntityId);
        }
        if (!INDEX_ID_IS_NONE(oldestUUIDId))
        {
          (void)IndexUUID_prune(indexHandle,NULL,NULL,oldestUUIDId);
        }

        // log
        Misc_formatDateTime(String_clear(dateTime),oldestCreatedDateTime,TIME_TYPE_LOCAL,NULL);
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
    while (   !INDEX_ID_IS_NONE(oldestStorageId)
           && (totalStorageSize > limit)
          );

    // free resources
    String_delete(dateTime);
    Storage_doneSpecifier(&storageSpecifier);
    String_delete(oldestStorageName);
    String_delete(storageName);
  }
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
  assert(server != NULL);
  assert(maxStorageSize > 0LL);

  if (indexHandle != NULL)
  {
    StorageSpecifier storageSpecifier;
    Storage_initSpecifier(&storageSpecifier);
    String           storageName       = String_new();
    String           oldestStorageName = String_new();
    String           dateTime          = String_new();
    uint64           totalStorageSize  = 0LL;
    IndexId          oldestStorageId   = INDEX_ID_NONE;
    do
    {
      Errors error;

      // get total storage size, find oldest storage entry
      IndexId          oldestUUIDId          = INDEX_ID_NONE;
      IndexId          oldestEntityId        = INDEX_ID_NONE;
      String_clear(oldestStorageName);
      uint64           oldestCreatedDateTime = MAX_UINT64;
      uint64           oldestSize            = 0LL;
      IndexQueryHandle indexQueryHandle;
      error = IndexStorage_initList(&indexQueryHandle,
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
      IndexId uuidId;
      IndexId entityId;
      IndexId storageId;
      uint64  createdDateTime;
      uint64  size;
      while (IndexStorage_getNext(&indexQueryHandle,
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
          StorageInfo storageInfo;
          error = Storage_init(&storageInfo,
//TODO
NULL, // masterIO
                               &storageSpecifier,
                               NULL,  // jobOptions
                               &globalOptions.indexDatabaseMaxBandWidthList,
                               SERVER_CONNECTION_PRIORITY_HIGH,
                               CALLBACK_(NULL,NULL),  // storageUpdateProgress
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
        (void)IndexEntity_prune(indexHandle,NULL,NULL,oldestEntityId);
        (void)IndexUUID_prune(indexHandle,NULL,NULL,oldestUUIDId);

        // log
        Misc_formatDateTime(dateTime,oldestCreatedDateTime,TIME_TYPE_LOCAL,NULL);
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

    // free resources
    String_delete(dateTime);
    Storage_doneSpecifier(&storageSpecifier);
    String_delete(oldestStorageName);
    String_delete(storageName);
  }
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

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);

  // init variables
  AutoFreeList autoFreeList;
  AutoFree_init(&autoFreeList);

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
      Errors error = Storage_preProcess(&createInfo->storageInfo,NULL,createInfo->createdDateTime,TRUE);
      if (error != ERROR_NONE)
      {
        printError(_("cannot pre-process storage (error: %s)!"),
                   Error_getText(error)
                  );
        createInfo->failError = error;
        AutoFree_cleanup(&autoFreeList);
        return;
      }
    }
  }

  // store archives
  String printableStorageName  = String_new();
  AUTOFREE_ADD(&autoFreeList,printableStorageName,{ String_delete(printableStorageName); });
  String directoryPath         = String_new();
  AUTOFREE_ADD(&autoFreeList,directoryPath,{ String_delete(directoryPath); });
  String existingStorageName   = String_new();
  AUTOFREE_ADD(&autoFreeList,existingStorageName,{ String_delete(existingStorageName); });
  String existingDirectoryName = String_new();
  AUTOFREE_ADD(&autoFreeList,existingDirectoryName,{ String_delete(existingDirectoryName); });
  StorageSpecifier existingStorageSpecifier;
  Storage_initSpecifier(&existingStorageSpecifier);
  AUTOFREE_ADD(&autoFreeList,&existingStorageSpecifier,{ Storage_doneSpecifier(&existingStorageSpecifier); });
  byte *buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,buffer,{ free(buffer); });
  while (   (createInfo->failError == ERROR_NONE)
         && !isAborted(createInfo)
        )
  {
    void *autoFreeSavePoint = AutoFree_save(&autoFreeList);

    // pause, check abort
    Storage_pause(&createInfo->storageInfo);
    if ((createInfo->failError != ERROR_NONE) || isAborted(createInfo))
    {
      AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
      break;
    }

    // get next archive to store
    StorageMsg storageMsg;
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
      Errors error;

      // get file info
      FileInfo fileInfo;
      error = File_getInfo(&fileInfo,storageMsg.intermediateFileName);
      if (error != ERROR_NONE)
      {
        if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

        printError(_("cannot get info for file '%s' (error: %s)"),
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
        String directoryPath,baseName,extension;
        String prefixFileName,postfixFileName;
        uint   n;

        // rename new archive
        directoryPath   = String_new();
        baseName        = String_new();
        extension       = String_new();
        prefixFileName  = String_new();
        postfixFileName = String_new();
        File_splitFileName(storageMsg.archiveName,directoryPath,baseName,extension);
        String_set(prefixFileName,baseName);
        String_set(postfixFileName,extension);
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
        String_delete(postfixFileName);
        String_delete(prefixFileName);
        String_delete(extension);
        String_delete(baseName);
        String_delete(directoryPath);
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

        printError(_("cannot pre-process file '%s' (error: %s)!"),
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
        Server server;
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
      FileHandle fileHandle;
      error = File_open(&fileHandle,storageMsg.intermediateFileName,FILE_OPEN_READ);
      if (error != ERROR_NONE)
      {
        if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

        printInfo(1,"FAIL!\n");
        printError(_("cannot open file '%s' (error: %s)!"),
                   String_cString(storageMsg.intermediateFileName),
                   Error_getText(error)
                  );

        AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
        break;
      }
      AUTOFREE_ADD(&autoFreeList,&fileHandle,{ File_close(&fileHandle); });
      DEBUG_TESTCODE() { createInfo->failError = DEBUG_TESTCODE_ERROR(); AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE); continue; }

      // create storage
      uint   retryCount  = 0;
      bool   appendFlag  = FALSE;
      uint64 storageSize = 0LL;
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
        StorageHandle storageHandle;
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

        // update running info
        STATUS_INFO_UPDATE(createInfo,NULL,NULL)
        {
          String_set(createInfo->runningInfo.progress.storage.name,printableStorageName);
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
        printInfo(1,"FAIL!\n");
        printError(_("cannot store '%s' (error: %s)!"),
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

          printInfo(1,"FAIL!\n");
          printError(_("cannot test '%s' (error: %s)!"),
                     String_cString(printableStorageName),
                     Error_getText(createInfo->failError)
                    );
          AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
          break;
        }
        printInfo(1,"OK (%"PRIu64" bytes)\n",fileInfo.size);
      }

#ifdef HAVE_PAR2
      if (   !stringIsEmpty(globalOptions.par2Directory)
          || !String_isEmpty(createInfo->jobOptions->par2Directory)
         )
      {
        printInfo(1,"Create PAR2 files for '%s'...",String_cString(printableStorageName));
        error = PAR2_create(storageMsg.intermediateFileName,
                            fileInfo.size,
                            storageMsg.archiveName,
                            !String_isEmpty(createInfo->jobOptions->par2Directory)
                              ? String_cString(createInfo->jobOptions->par2Directory)
                              : globalOptions.par2Directory,
                            createInfo->storageInfo.jobOptions->archiveFileMode
                           );
        if (error != ERROR_NONE)
        {
          if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

          printInfo(1,"FAIL!\n");
          printError(_("cannot create PAR2 files for '%s' (error: %s)!"),
                     String_cString(printableStorageName),
                     Error_getText(createInfo->failError)
                    );
          AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
          break;
        }
        printInfo(1,"OK\n");
      }
#endif // HAVE_PAR2

      // update index database and set state
      if (   (createInfo->indexHandle != NULL)
          && !INDEX_ID_IS_NONE(storageMsg.storageId)
         )
      {
        assert(!INDEX_ID_IS_NONE(storageMsg.entityId));

        // check if append and storage exists => assign to existing storage index
        IndexId storageId;
        if (   appendFlag
            && (IndexStorage_findByName(createInfo->indexHandle,
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
                                       )
               )
           )
        {
          // set index database state
          error = IndexStorage_setState(createInfo->indexHandle,
                                        storageId,
                                        INDEX_STATE_CREATE,
                                        0LL,  // lastCheckedDateTime
                                        NULL // errorMessage
                                       );
          if (error != ERROR_NONE)
          {
            if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

            printInfo(1,"FAIL\n");
            printError(_("cannot update index for storage '%s' (error: %s)!"),
                       String_cString(printableStorageName),
                       Error_getText(error)
                      );

            AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
            break;
          }
          AUTOFREE_ADD(&autoFreeList,&storageMsg.storageId,
          {
            (void)IndexStorage_setState(createInfo->indexHandle,
                                        storageId,
                                        INDEX_STATE_ERROR,
                                        0LL,  // lastCheckedDateTime
                                        NULL // errorMessage
                                       );
          });

          // append index: assign storage index entries to existing storage index
//fprintf(stderr,"%s, %d: append to storage %"PRIu64"\n",__FILE__,__LINE__,storageId);
          error = IndexAssign_to(createInfo->indexHandle,
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
            printError(_("cannot update index for storage '%s' (error: %s)!"),
                       String_cString(printableStorageName),
                       Error_getText(error)
                      );

            AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
            break;
          }

          // prune storage (maybe empty now)
          (void)IndexStorage_prune(createInfo->indexHandle,NULL,NULL,storageMsg.storageId);

          // prune entity (maybe empty now)
          (void)IndexEntity_prune(createInfo->indexHandle,NULL,NULL,storageMsg.entityId);
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
          error = IndexStorage_purgeAllByName(createInfo->indexHandle,
                                              &createInfo->storageInfo.storageSpecifier,
                                              storageMsg.archiveName,
                                              storageMsg.storageId,
                                              NULL  // progressInfo
                                             );
          if (error != ERROR_NONE)
          {
            if (createInfo->failError == ERROR_NONE) createInfo->failError = error;

            printInfo(1,"FAIL\n");
            printError(_("cannot delete old index for storage '%s' (error: %s)!"),
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
            IndexQueryHandle indexQueryHandle;
            error = IndexStorage_initList(&indexQueryHandle,
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
            IndexId existingEntityId;
            IndexId existingStorageId;
            while (IndexStorage_getNext(&indexQueryHandle,
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
                error = IndexAssign_to(createInfo->indexHandle,
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
              printError(_("cannot delete old index for storage '%s' (error: %s)!"),
                         String_cString(printableStorageName),
                         Error_getText(error)
                        );

              AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
              break;
            }

            // prune entity (maybe empty now)
            (void)IndexEntity_prune(createInfo->indexHandle,NULL,NULL,storageMsg.entityId);
          }
        }

        // update index storage name+size+newest entries
        if (error == ERROR_NONE)
        {
          error = IndexStorage_update(createInfo->indexHandle,
                                      storageId,
                                      NULL,  // hostName
                                      NULL,  // userName
                                      printableStorageName,
                                      0,  // createDateTime
                                      storageSize,
                                      NULL,  // comment
                                      TRUE  // updateNewest
                                     );
        }

        // update storages info (aggregated values)
        if (error == ERROR_NONE)
        {
          error = IndexStorage_updateInfos(createInfo->indexHandle,
                                           storageId
                                          );
        }

        // set index database state and last check time stamp
        if (error == ERROR_NONE)
        {
          error = IndexStorage_setState(createInfo->indexHandle,
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
          printError(_("cannot update index for storage '%s' (error: %s)!"),
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
        printError(_("cannot post-process storage file '%s' (error: %s)!"),
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
        printWarning(_("cannot delete file '%s' (error: %s)!"),
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
  StorageMsg storageMsg;
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
    Errors error = File_delete(storageMsg.intermediateFileName,FALSE);
    if (error != ERROR_NONE)
    {
      printWarning(_("cannot delete file '%s' (error: %s)!"),
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
      Errors error = Storage_postProcess(&createInfo->storageInfo,NULL,createInfo->createdDateTime,TRUE);
      if (error != ERROR_NONE)
      {
        printError(_("cannot post-process storage (error: %s)!"),
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
      String pattern = String_new();
      Errors error = Archive_formatName(pattern,
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
                               UNUSED_VARIABLE(fileInfo);
                               UNUSED_VARIABLE(userData);

                               // init variables
                               StorageSpecifier storageSpecifier;
                               Storage_initSpecifier(&storageSpecifier);

                               Errors error = Storage_parseName(&storageSpecifier,storageName);
                               if (error == ERROR_NONE)
                               {
                                 // find in storage list
                                 if (StringList_find(&createInfo->storageFileList,storageSpecifier.archiveName) == NULL)
                                 {
                                   (void)Storage_delete(&createInfo->storageInfo,storageName);
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
  assert(createInfo != NULL);
  assert(name != NULL);

  STATUS_INFO_UPDATE(createInfo,name,NULL)
  {
    // get/create fragment node
    FragmentNode *fragmentNode = FragmentList_find(&createInfo->runningInfoFragmentList,name);
    if (fragmentNode == NULL)
    {
      fragmentNode = FragmentList_add(&createInfo->runningInfoFragmentList,name,size,NULL,0,fragmentCount);
      if (fragmentNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
    }
    assert(fragmentNode != NULL);

    // status update
    if (   (createInfo->runningInfoCurrentFragmentNode == NULL)
        || ((Misc_getTimestamp()-createInfo->runningInfoCurrentLastUpdateTimestamp) >= 10*US_PER_S)
       )
    {
      createInfo->runningInfoCurrentFragmentNode        = fragmentNode;
      createInfo->runningInfoCurrentLastUpdateTimestamp = Misc_getTimestamp();

      String_set(createInfo->runningInfo.progress.entry.name,name);
      createInfo->runningInfo.progress.entry.doneSize  = FragmentList_getSize(fragmentNode);
      createInfo->runningInfo.progress.entry.totalSize = FragmentList_getTotalSize(fragmentNode);
    }
    assert(createInfo->runningInfoCurrentFragmentNode != NULL);
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
  assert(createInfo != NULL);
  assert(name != NULL);

  SEMAPHORE_LOCKED_DO(&createInfo->runningInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // get fragment node
    FragmentNode *fragmentNode = FragmentList_find(&createInfo->runningInfoFragmentList,name);
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
        if (fragmentNode == createInfo->runningInfoCurrentFragmentNode)
        {
          createInfo->runningInfoCurrentFragmentNode = NULL;
        }

        FragmentList_discard(&createInfo->runningInfoFragmentList,fragmentNode);
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
  assert(archiveEntryName != NULL);
  assert(name != NULL);

  if (!String_isEmpty(globalOptions.transform.patternString))
  {
    // transform name with matching pattern
    String_clear(archiveEntryName);
    Pattern pattern;
    Pattern_init(&pattern,globalOptions.transform.patternString,globalOptions.transform.patternType,PATTERN_FLAG_NONE);
    ulong   index = STRING_BEGIN;
    ulong   matchIndex,matchLength;
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
  assert(archiveEntryNameList != NULL);

  String archiveEntryName = String_new();
  ConstString name;
  STRINGLIST_ITERATE(nameList,name)
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
  Errors error;

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);
  assert(fileName != NULL);
  assert(fileInfo != NULL);
  assert((fragmentCount == 0) || (fragmentNumber < fragmentCount));
  assert(buffer != NULL);

  printInfo(1,"Add file      '%s'...",String_cString(fileName));

  // get file extended attributes
  FileExtendedAttributeList fileExtendedAttributeList;
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
        createInfo->runningInfo.progress.skipped.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.skipped.size  += fragmentSize;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.done.size += fragmentSize;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError(_("cannot get extended attributes for '%s' (error: %s)"),
                 String_cString(fileName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
      {
        createInfo->runningInfo.progress.error.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.error.size += fragmentSize;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.done.size += fragmentSize;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  // open file
  FileHandle fileHandle;
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

      STATUS_INFO_UPDATE(createInfo,NULL,NULL)
      {
        createInfo->runningInfo.progress.skipped.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.skipped.size  += fragmentSize;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.done.size  += fragmentSize;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError(_("cannot open file '%s' (error: %s)"),
                 String_cString(fileName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
      {
        createInfo->runningInfo.progress.error.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.error.size  += (uint64)fileInfo->size;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.done.size  += (uint64)fileInfo->size;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  // init fragment
  fragmentInit(createInfo,fileName,fileInfo->size,fragmentCount);

  if (!createInfo->jobOptions->noStorage)
  {
    uint64       offset       = 0LL;
    uint64       size         = 0LL;
    ArchiveFlags archiveFlags = ARCHIVE_FLAG_NONE;

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
    String           archiveEntryName = getArchiveEntryName(String_new(),fileName);
    ArchiveEntryInfo archiveEntryInfo;
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
      printError(_("cannot create new archive file entry '%s' (error: %s)"),
                 String_cString(fileName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
      {
        createInfo->runningInfo.progress.error.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.error.size  += (uint64)fileInfo->size;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.done.size  += (uint64)fileInfo->size;
      }

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
        ulong bufferLength;
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
              uint64 archiveSize = Archive_getSize(&createInfo->archiveHandle);

              FragmentNode *fragmentNode;
              STATUS_INFO_UPDATE(createInfo,fileName,&fragmentNode)
              {
                if (fragmentNode != NULL)
                {
                  // add fragment
                  FragmentList_addRange(fragmentNode,offset,bufferLength);

                  // update running info
                  if (fragmentNode == createInfo->runningInfoCurrentFragmentNode)
                  {
                    createInfo->runningInfo.progress.entry.doneSize   = FragmentList_getSize(createInfo->runningInfoCurrentFragmentNode);
                    createInfo->runningInfo.progress.entry.totalSize  = FragmentList_getTotalSize(createInfo->runningInfoCurrentFragmentNode);
                  }
                }
                createInfo->runningInfo.progress.done.size        = createInfo->runningInfo.progress.done.size+(uint64)bufferLength;
                createInfo->runningInfo.progress.archiveSize      = archiveSize+createInfo->runningInfo.progress.storage.totalSize;
                createInfo->runningInfo.progress.compressionRatio = (!createInfo->jobOptions->dryRun && (createInfo->runningInfo.progress.done.size > 0))
                                                                      ? 100.0-(archiveSize*100.0)/createInfo->runningInfo.progress.done.size
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
              uint percentageDone = 0;
              STATUS_INFO_GET(createInfo,fileName)
              {
                percentageDone = (createInfo->runningInfo.progress.entry.totalSize > 0LL)
                                   ? (uint)((createInfo->runningInfo.progress.entry.doneSize*100LL)/createInfo->runningInfo.progress.entry.totalSize)
                                   : 100;
              }
              printInfo(2,"%3d%%\b\b\b\b",percentageDone);
            }

            assert(size >= bufferLength);
            size -= bufferLength;
          }
          else
          {
            // read nothing -> file size changed -> done
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
          createInfo->runningInfo.progress.error.count += (fragmentOffset == 0LL) ? 1 : 0;;
          createInfo->runningInfo.progress.error.size += fragmentSize;
          createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;;
          createInfo->runningInfo.progress.done.size += fragmentSize;
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
        printError(_("cannot store file entry (error: %s)!"),
                   Error_getText(error)
                  );

        STATUS_INFO_UPDATE(createInfo,fileName,NULL)
        {
          createInfo->runningInfo.progress.error.count += (fragmentOffset == 0LL) ? 1 : 0;;
          createInfo->runningInfo.progress.error.size += fragmentSize;
          createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;;
          createInfo->runningInfo.progress.done.size += fragmentSize;
        }

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
      printError(_("cannot close archive file entry (error: %s)!"),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
      {
        createInfo->runningInfo.progress.error.count += (fragmentOffset == 0LL) ? 1 : 0;;
        createInfo->runningInfo.progress.error.size += fragmentSize;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;;
        createInfo->runningInfo.progress.done.size += fragmentSize;
      }

      (void)File_close(&fileHandle);
      fragmentDone(createInfo,fileName);
      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }

    // get final compression ratio
    double compressionRatio;
    if (archiveEntryInfo.file.chunkFileData.fragmentSize > 0LL)
    {
      compressionRatio = 100.0-archiveEntryInfo.file.chunkFileData.info.size*100.0/archiveEntryInfo.file.chunkFileData.fragmentSize;
    }
    else
    {
      compressionRatio = 0.0;
    }

    // get fragment info
    char fragmentInfoString[256];
    stringClear(fragmentInfoString);
    if (fragmentSize < fileInfo->size)
    {
      double d = (fragmentCount > 0) ? ceil(log10((double)fragmentCount)) : 1.00;
      stringAppend(fragmentInfoString,sizeof(fragmentInfoString),", fragment #");
      stringAppendFormat(fragmentInfoString,sizeof(fragmentInfoString),"%*u/%u",(int)d,1+fragmentNumber,fragmentCount);
    }

    // ratio info
    char compressionRatioString[256];
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
    FragmentNode *fragmentNode;
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

  // update running info
  FragmentNode *fragmentNode;
  STATUS_INFO_UPDATE(createInfo,fileName,&fragmentNode)
  {
    if (fragmentNode != NULL)
    {
      if (fragmentNode == createInfo->runningInfoCurrentFragmentNode)
      {
        createInfo->runningInfo.progress.entry.doneSize   = FragmentList_getSize(createInfo->runningInfoCurrentFragmentNode);
        createInfo->runningInfo.progress.entry.totalSize  = FragmentList_getTotalSize(createInfo->runningInfoCurrentFragmentNode);
      }
      if (FragmentList_isComplete(fragmentNode))
      {
        createInfo->runningInfo.progress.done.count++;
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
  Errors error;

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
    printError(_("device block size %"PRIu64" on '%s' is too big (max: %"PRIu64")"),
               deviceInfo->blockSize,
               String_cString(deviceName),
               bufferSize
              );

    STATUS_INFO_UPDATE(createInfo,deviceName,NULL)
    {
      createInfo->runningInfo.progress.skipped.count += (fragmentOffset == 0LL) ? 1 : 0;
      createInfo->runningInfo.progress.skipped.size  += fragmentSize;
      createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
      createInfo->runningInfo.progress.done.size  += fragmentSize;
    }

    return ERROR_INVALID_DEVICE_BLOCK_SIZE;
  }
  if (deviceInfo->blockSize <= 0)
  {
    printInfo(1,"FAIL\n");
    printError(_("invalid device block size for '%s'"),
               String_cString(deviceName)
              );

    STATUS_INFO_UPDATE(createInfo,deviceName,NULL)
    {
      createInfo->runningInfo.progress.skipped.count += (fragmentOffset == 0LL) ? 1 : 0;
      createInfo->runningInfo.progress.skipped.size  += fragmentSize;
      createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
      createInfo->runningInfo.progress.done.size  += fragmentSize;
    }

    return ERROR_INVALID_DEVICE_BLOCK_SIZE;
  }
  assert(deviceInfo->blockSize > 0);
  uint maxBufferBlockCount = bufferSize/deviceInfo->blockSize;

  // open device
  DeviceHandle deviceHandle;
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
        createInfo->runningInfo.progress.skipped.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.skipped.size  += fragmentSize;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.done.size  += fragmentSize;
      }

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError(_("cannot open device '%s' (error: %s)"),
                 String_cString(deviceName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,deviceName,NULL)
      {
        createInfo->runningInfo.progress.error.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.error.size  += fragmentSize;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.done.size  += fragmentSize;
      }

      return error;
    }
  }

  // init file system
  bool isSupportedFileSystem = FALSE;
  FileSystemHandle fileSystemHandle;
  if (!createInfo->jobOptions->rawImagesFlag)
  {
    if (FileSystem_init(&fileSystemHandle,deviceName) == ERROR_NONE)
    {
      if (ARRAY_FIND(SUPPORTED_FILE_SYSTEM_TYPES,
                     SIZE_OF_ARRAY(SUPPORTED_FILE_SYSTEM_TYPES),
                     i,
                     fileSystemHandle.type == SUPPORTED_FILE_SYSTEM_TYPES[i]
                    )
         )
      {
        isSupportedFileSystem = TRUE;
      }
      else
      {
        FileSystem_done(&fileSystemHandle);
      }
    }
  }

  // init fragment
  fragmentInit(createInfo,deviceName,deviceInfo->size,fragmentCount);

  if (!createInfo->jobOptions->noStorage)
  {
    uint64       blockOffset  = 0LL;
    uint64       blockCount   = 0LL;
    ArchiveFlags archiveFlags = ARCHIVE_FLAG_NONE;

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
    String           archiveEntryName = getArchiveEntryName(String_new(),deviceName);
    ArchiveEntryInfo archiveEntryInfo;
    error = Archive_newImageEntry(&archiveEntryInfo,
                                  &createInfo->archiveHandle,
                                  createInfo->jobOptions->compressAlgorithms.delta,
                                  createInfo->jobOptions->compressAlgorithms.byte,
                                  createInfo->jobOptions->cryptAlgorithms[0],
                                  NULL,  // cryptSalt
                                  NULL,  // cryptKey
                                  archiveEntryName,
                                  deviceInfo,
                                  (!createInfo->jobOptions->rawImagesFlag && isSupportedFileSystem)
                                    ? fileSystemHandle.type
                                    : FILE_SYSTEM_TYPE_NONE,
                                  fragmentOffset/(uint64)deviceInfo->blockSize,
                                  (fragmentSize+(uint64)deviceInfo->blockSize-1)/(uint64)deviceInfo->blockSize,
                                  archiveFlags
                                 );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError(_("cannot create new archive image entry '%s' (error: %s)"),
                 String_cString(deviceName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,deviceName,NULL)
      {
        createInfo->runningInfo.progress.skipped.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.skipped.size  += fragmentSize;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.done.size  += fragmentSize;
      }

      String_delete(archiveEntryName);
      if (isSupportedFileSystem) FileSystem_done(&fileSystemHandle);
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
      uint bufferBlockCount = 0;
      while (   (blockCount > 0LL)
             && (bufferBlockCount < maxBufferBlockCount)
            )
      {
        if (   createInfo->jobOptions->rawImagesFlag
            || !isSupportedFileSystem
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
          uint64 archiveSize = Archive_getSize(&createInfo->archiveHandle);

          // update running info
          FragmentNode *fragmentNode;
          STATUS_INFO_UPDATE(createInfo,deviceName,&fragmentNode)
          {
            if (fragmentNode != NULL)
            {
              // add fragment
              FragmentList_addRange(fragmentNode,blockOffset*(uint64)deviceInfo->blockSize,(uint64)bufferBlockCount*(uint64)deviceInfo->blockSize);

              // update running info
              if (fragmentNode == createInfo->runningInfoCurrentFragmentNode)
              {
                createInfo->runningInfo.progress.entry.doneSize   = FragmentList_getSize(createInfo->runningInfoCurrentFragmentNode);
                createInfo->runningInfo.progress.entry.totalSize  = FragmentList_getTotalSize(createInfo->runningInfoCurrentFragmentNode);
              }
            }
            createInfo->runningInfo.progress.done.size        = createInfo->runningInfo.progress.done.size+(uint64)bufferBlockCount*(uint64)deviceInfo->blockSize;
            createInfo->runningInfo.progress.archiveSize      = archiveSize+createInfo->runningInfo.progress.storage.totalSize;
            createInfo->runningInfo.progress.compressionRatio = (!createInfo->jobOptions->dryRun && (createInfo->runningInfo.progress.done.size > 0))
                                                                  ? 100.0-(archiveSize*100.0)/createInfo->runningInfo.progress.done.size
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
          uint percentageDone = 0;
          STATUS_INFO_GET(createInfo,deviceName)
          {
            percentageDone = (createInfo->runningInfo.progress.entry.totalSize > 0LL)
                               ? (uint)((createInfo->runningInfo.progress.entry.doneSize*100LL)/createInfo->runningInfo.progress.entry.totalSize)
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
      if (isSupportedFileSystem) FileSystem_done(&fileSystemHandle);
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
          createInfo->runningInfo.progress.error.count += (fragmentOffset == 0LL) ? 1 : 0;;
          createInfo->runningInfo.progress.error.size += fragmentSize;
          createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;;
          createInfo->runningInfo.progress.done.size += fragmentSize;
        }

        (void)Archive_closeEntry(&archiveEntryInfo);
        if (isSupportedFileSystem) FileSystem_done(&fileSystemHandle);
        Device_close(&deviceHandle);
        fragmentDone(createInfo,deviceName);

        return ERROR_NONE;
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError(_("cannot store image entry (error: %s)!"),
                   Error_getText(error)
                  );

        STATUS_INFO_UPDATE(createInfo,deviceName,NULL)
        {
          createInfo->runningInfo.progress.error.count += (fragmentOffset == 0LL) ? 1 : 0;;
          createInfo->runningInfo.progress.error.size += fragmentSize;
          createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;;
          createInfo->runningInfo.progress.done.size += fragmentSize;
        }

        (void)Archive_closeEntry(&archiveEntryInfo);
        if (isSupportedFileSystem) FileSystem_done(&fileSystemHandle);
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
      printError(_("cannot close archive image entry (error: %s)!"),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,deviceName,NULL)
      {
        createInfo->runningInfo.progress.error.count += (fragmentOffset == 0LL) ? 1 : 0;;
        createInfo->runningInfo.progress.error.size += fragmentSize;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;;
        createInfo->runningInfo.progress.done.size += fragmentSize;
      }

      if (isSupportedFileSystem) FileSystem_done(&fileSystemHandle);
      Device_close(&deviceHandle);
      fragmentDone(createInfo,deviceName);

      return error;
    }

    // get final compression ratio
    double compressionRatio;
    if (archiveEntryInfo.image.chunkImageData.blockCount > 0)
    {
      compressionRatio = 100.0-archiveEntryInfo.image.chunkImageData.info.size*100.0/(archiveEntryInfo.image.chunkImageData.blockCount*(uint64)deviceInfo->blockSize);
    }
    else
    {
      compressionRatio = 0.0;
    }

    // get size/fragment info
    char sizeString[32];
    if (globalOptions.humanFormatFlag)
    {
      getHumanSizeString(sizeString,sizeof(sizeString),fragmentSize);
    }
    else
    {
      stringFormat(sizeString,sizeof(sizeString),"%*"PRIu64,stringInt64Length(globalOptions.fragmentSize),fragmentSize);
    }
    char fragmentInfoString[256];
    stringClear(fragmentInfoString);
    if (fragmentSize < deviceInfo->size)
    {
      double d = (fragmentCount > 0) ? ceil(log10((double)fragmentCount)) : 1.0;
      stringAppend(fragmentInfoString,sizeof(fragmentInfoString),", fragment #");
      stringAppendFormat(fragmentInfoString,sizeof(fragmentInfoString),"%*u/%u",(int)d,1+fragmentNumber,fragmentCount);
    }

    // get ratio info
    char compressionRatioString[256];
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
      printInfo(1,"OK (%s, %s bytes%s%s)\n",
                (!createInfo->jobOptions->rawImagesFlag && isSupportedFileSystem)
                  ? FileSystem_typeToString(fileSystemHandle.type,NULL)
                  : "raw",
                sizeString,
                fragmentInfoString,
                compressionRatioString
               );
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_OK,
                 "Added image '%s' (%s, %"PRIu64" bytes%s%s)",
                 String_cString(deviceName),
                 (!createInfo->jobOptions->rawImagesFlag && isSupportedFileSystem)
                   ? FileSystem_typeToString(fileSystemHandle.type,NULL)
                   : "raw",
                 fragmentSize,
                 fragmentInfoString,
                 compressionRatioString
                );
    }
    else
    {
      printInfo(1,"OK (%s, %s bytes%s%s, dry-run)\n",
                (!createInfo->jobOptions->rawImagesFlag && isSupportedFileSystem)
                  ? FileSystem_typeToString(fileSystemHandle.type,NULL)
                  : "raw",
                sizeString,
                fragmentInfoString,
                compressionRatioString
               );
    }
  }
  else
  {
    FragmentNode *fragmentNode;
    STATUS_INFO_UPDATE(createInfo,deviceName,&fragmentNode)
    {
      if (fragmentNode != NULL)
      {
        // add fragment
        FragmentList_addRange(fragmentNode,0,deviceInfo->size);
      }
    }

    double d = (globalOptions.fragmentSize > 0LL) ? ceil(log10((double)globalOptions.fragmentSize)) : 1.0;
    printInfo(1,"OK (%s, %/"PRIu64" bytes, not stored)\n",
              (!createInfo->jobOptions->rawImagesFlag && isSupportedFileSystem)
                ? FileSystem_typeToString(fileSystemHandle.type,NULL)
                : "raw",
              (int)d,
              fragmentSize
             );
  }

  // update running info
  FragmentNode *fragmentNode;
  STATUS_INFO_UPDATE(createInfo,deviceName,&fragmentNode)
  {
    if (fragmentNode != NULL)
    {
      if (fragmentNode == createInfo->runningInfoCurrentFragmentNode)
      {
        createInfo->runningInfo.progress.entry.doneSize   = FragmentList_getSize(createInfo->runningInfoCurrentFragmentNode);
        createInfo->runningInfo.progress.entry.totalSize  = FragmentList_getTotalSize(createInfo->runningInfoCurrentFragmentNode);
      }

      if (FragmentList_isComplete(fragmentNode))
      {
        createInfo->runningInfo.progress.done.count++;
      }
    }
  }

  // free resources
  if (isSupportedFileSystem)
  {
    FileSystem_done(&fileSystemHandle);
  }
  Device_close(&deviceHandle);
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
  Errors error;

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);
  assert(directoryName != NULL);
  assert(fileInfo != NULL);

  printInfo(1,"Add directory '%s'...",String_cString(directoryName));

  // get file extended attributes
  FileExtendedAttributeList fileExtendedAttributeList;
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
        createInfo->runningInfo.progress.error.count++;
        createInfo->runningInfo.progress.done.count++;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError(_("cannot get extended attributes for '%s' (error: %s)"),
                 String_cString(directoryName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,directoryName,NULL)
      {
        createInfo->runningInfo.progress.error.count++;
        createInfo->runningInfo.progress.done.count++;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  if (!createInfo->jobOptions->noStorage)
  {
    // new directory
    String           archiveEntryName = getArchiveEntryName(String_new(),directoryName);
    ArchiveEntryInfo archiveEntryInfo;
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
      printError(_("cannot create new archive directory entry '%s' (error: %s)"),
                 String_cString(directoryName),
                 Error_getText(error)
                );
      logMessage(createInfo->logHandle,
                 LOG_TYPE_ENTRY_ACCESS_DENIED,
                 "Open failed '%s' (error: %s)",
                 String_cString(directoryName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,directoryName,NULL)
      {
        createInfo->runningInfo.progress.error.count++;
        createInfo->runningInfo.progress.done.count++;
      }

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
      printError(_("cannot close archive directory entry (error: %s)!"),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,directoryName,NULL)
      {
        createInfo->runningInfo.progress.error.count++;
        createInfo->runningInfo.progress.done.count++;
      }

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

  // update running info
  STATUS_INFO_UPDATE(createInfo,directoryName,NULL)
  {
    createInfo->runningInfo.progress.done.count++;
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
  Errors error;

  assert(createInfo != NULL);
  assert(linkName != NULL);
  assert(fileInfo != NULL);

  printInfo(1,"Add link      '%s'...",String_cString(linkName));

  // get file extended attributes
  FileExtendedAttributeList fileExtendedAttributeList;
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
        createInfo->runningInfo.progress.error.count++;
        createInfo->runningInfo.progress.done.count++;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError(_("cannot get extended attributes for '%s' (error: %s)"),
                 String_cString(linkName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,linkName,NULL)
      {
        createInfo->runningInfo.progress.error.count++;
        createInfo->runningInfo.progress.done.count++;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  if (!createInfo->jobOptions->noStorage)
  {
    // read link
    String fileName = String_new();
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
          createInfo->runningInfo.progress.error.count++;
          createInfo->runningInfo.progress.done.count++;
        }

        String_delete(fileName);
        File_doneExtendedAttributes(&fileExtendedAttributeList);

        return ERROR_NONE;
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError(_("cannot read link '%s' (error: %s)"),
                   String_cString(linkName),
                   Error_getText(error)
                  );

        STATUS_INFO_UPDATE(createInfo,linkName,NULL)
        {
          createInfo->runningInfo.progress.error.count++;
          createInfo->runningInfo.progress.done.count++;
        }

        String_delete(fileName);
        File_doneExtendedAttributes(&fileExtendedAttributeList);

        return error;
      }
    }

    // new link
    String            archiveEntryName = getArchiveEntryName(String_new(),linkName);
    ArchiveEntryInfo  archiveEntryInfo;
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
      printError(_("cannot create new archive link entry '%s' (error: %s)"),
                 String_cString(linkName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,linkName,NULL)
      {
        createInfo->runningInfo.progress.error.count++;
        createInfo->runningInfo.progress.done.count++;
      }

      String_delete(archiveEntryName);
      String_delete(fileName);
      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
    String_delete(archiveEntryName);

    // update running info
    STATUS_INFO_UPDATE(createInfo,linkName,NULL)
    {
      // no additional values
    }

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError(_("cannot close archive link entry (error: %s)!"),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,linkName,NULL)
      {
        createInfo->runningInfo.progress.error.count++;
        createInfo->runningInfo.progress.done.count++;
      }

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
                 "Added link '%s'",
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

  // update running info
  STATUS_INFO_UPDATE(createInfo,linkName,NULL)
  {
    createInfo->runningInfo.progress.done.count++;
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
  Errors error;

  assert(createInfo != NULL);
  assert(fileNameList != NULL);
  assert(!StringList_isEmpty(fileNameList));
  assert(fileInfo != NULL);
  assert((fragmentCount == 0) || (fragmentNumber < fragmentCount));
  assert(buffer != NULL);

  printInfo(1,"Add hardlink  '%s'...",String_cString(StringList_first(fileNameList,NULL)));

  // get file extended attributes
  FileExtendedAttributeList fileExtendedAttributeList;
  File_initExtendedAttributes(&fileExtendedAttributeList);
  error = File_getExtendedAttributes(&fileExtendedAttributeList,StringList_first(fileNameList,NULL));
  if (error != ERROR_NONE)
  {
    if      (createInfo->jobOptions->noStopOnAttributeErrorFlag)
    {
      printWarning(_("cannot not get extended attributes for '%s' - continue (error: %s)!"),
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
        createInfo->runningInfo.progress.skipped.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.skipped.size  += fragmentSize;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.done.size += fragmentSize;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError(_("cannot get extended attributes for '%s' (error: %s)"),
                 String_cString(StringList_first(fileNameList,NULL)),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),NULL)
      {
        createInfo->runningInfo.progress.error.count++;
        createInfo->runningInfo.progress.done.count++;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.done.size += fragmentSize;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  // open file
  FileHandle fileHandle;
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
        createInfo->runningInfo.progress.skipped.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.skipped.size  += fragmentSize;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.done.size += fragmentSize;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError(_("cannot open hardlink '%s' (error: %s)"),
                 String_cString(StringList_first(fileNameList,NULL)),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),NULL)
      {
        createInfo->runningInfo.progress.error.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.error.size  += fragmentSize;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.done.size += fragmentSize;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  // init fragment
  fragmentInit(createInfo,StringList_first(fileNameList,NULL),fileInfo->size,fragmentCount);

  if (!createInfo->jobOptions->noStorage)
  {
    ArchiveFlags archiveFlags = ARCHIVE_FLAG_NONE;

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
    StringList archiveEntryNameList;
    StringList_init(&archiveEntryNameList);
    getArchiveEntryNameList(&archiveEntryNameList,fileNameList);
    ArchiveEntryInfo archiveEntryInfo;
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
      printError(_("cannot create new archive hardlink entry '%s' (error: %s)"),
                 String_cString(StringList_first(fileNameList,NULL)),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),NULL)
      {
        createInfo->runningInfo.progress.error.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.error.size  += fragmentSize;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.done.size += fragmentSize;
      }

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
      uint64 offset = fragmentOffset;
      uint64 size   = fragmentSize;
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
        ulong bufferLength;
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
              uint64 archiveSize = Archive_getSize(&createInfo->archiveHandle);

              // update running info
              FragmentNode *fragmentNode;
              STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),&fragmentNode)
              {
                if (fragmentNode != NULL)
                {
                  // add fragment
                  FragmentList_addRange(fragmentNode,offset,bufferLength);

                  // update running info
                  if (fragmentNode == createInfo->runningInfoCurrentFragmentNode)
                  {
                    createInfo->runningInfo.progress.entry.doneSize   = FragmentList_getSize(createInfo->runningInfoCurrentFragmentNode);
                    createInfo->runningInfo.progress.entry.totalSize  = FragmentList_getTotalSize(createInfo->runningInfoCurrentFragmentNode);
                  }
                }
                createInfo->runningInfo.progress.done.size        = createInfo->runningInfo.progress.done.size+(uint64)bufferLength;
                createInfo->runningInfo.progress.archiveSize      = archiveSize+createInfo->runningInfo.progress.storage.totalSize;
                createInfo->runningInfo.progress.compressionRatio = (!createInfo->jobOptions->dryRun && (createInfo->runningInfo.progress.done.size > 0))
                                                                      ? 100.0-(createInfo->runningInfo.progress.archiveSize *100.0)/createInfo->runningInfo.progress.done.size
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
              uint percentageDone = 0;
              STATUS_INFO_GET(createInfo,StringList_first(fileNameList,NULL))
              {
                percentageDone = (createInfo->runningInfo.progress.entry.totalSize > 0LL)
                                   ? (uint)((createInfo->runningInfo.progress.entry.doneSize*100LL)/createInfo->runningInfo.progress.entry.totalSize)
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
          createInfo->runningInfo.progress.error.count += (fragmentOffset == 0LL) ? 1 : 0;
          createInfo->runningInfo.progress.error.size  += fragmentSize;
          createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
          createInfo->runningInfo.progress.done.size += fragmentSize;
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
        printError(_("cannot store hardlink entry (error: %s)!"),
                   Error_getText(error)
                  );

        STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),NULL)
        {
          createInfo->runningInfo.progress.error.count += (fragmentOffset == 0LL) ? 1 : 0;
          createInfo->runningInfo.progress.error.size  += fragmentSize;
          createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
          createInfo->runningInfo.progress.done.size += fragmentSize;
        }

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
      printError(_("cannot close archive hardlink entry (error: %s)!"),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),NULL)
      {
        createInfo->runningInfo.progress.error.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.error.size  += fragmentSize;
        createInfo->runningInfo.progress.done.count += (fragmentOffset == 0LL) ? 1 : 0;
        createInfo->runningInfo.progress.done.size += fragmentSize;
      }

      StringList_done(&archiveEntryNameList);
      (void)File_close(&fileHandle);
      fragmentDone(createInfo,StringList_first(fileNameList,NULL));
      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
    StringList_done(&archiveEntryNameList);

    // get final compression ratio
    double compressionRatio;
    if (archiveEntryInfo.hardLink.chunkHardLinkData.fragmentSize > 0LL)
    {
      compressionRatio = 100.0-archiveEntryInfo.hardLink.chunkHardLinkData.info.size*100.0/archiveEntryInfo.hardLink.chunkHardLinkData.fragmentSize;
    }
    else
    {
      compressionRatio = 0.0;
    }

    // get fragment info
    char fragmentInfoString[256];
    stringClear(fragmentInfoString);
    if (fragmentSize < fileInfo->size)
    {
      double d = (fragmentCount > 0) ? ceil(log10((double)fragmentCount)) : 1.0;
      stringAppend(fragmentInfoString,sizeof(fragmentInfoString),", fragment #");
      stringAppendFormat(fragmentInfoString,sizeof(fragmentInfoString),"%*u/%u",(int)d,1+fragmentNumber,fragmentCount);
    }

    // get ratio info
    char compressionRatioString[256];
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
    char compressionRatioString[256];
    stringClear(compressionRatioString);
    FragmentNode *fragmentNode;
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

  // update running info
  FragmentNode *fragmentNode;
  STATUS_INFO_UPDATE(createInfo,StringList_first(fileNameList,NULL),&fragmentNode)
  {
    if (fragmentNode != NULL)
    {
      if (fragmentNode == createInfo->runningInfoCurrentFragmentNode)
      {
        createInfo->runningInfo.progress.entry.doneSize   = FragmentList_getSize(createInfo->runningInfoCurrentFragmentNode);
        createInfo->runningInfo.progress.entry.totalSize  = FragmentList_getTotalSize(createInfo->runningInfoCurrentFragmentNode);
      }

      if (FragmentList_isComplete(fragmentNode))
      {
        createInfo->runningInfo.progress.done.count += StringList_count(fileNameList);
      }
    }
  }

  // close file
  (void)File_close(&fileHandle);

  const StringNode *stringNode = fileNameList->head->next;
  while (stringNode != NULL)
  {
    printInfo(1,"Add hardlink  '%s'...OK\n",String_cString(stringNode->string));
    stringNode = stringNode->next;
  }

  // free resources
  fragmentDone(createInfo,StringList_first(fileNameList,NULL));
  File_doneExtendedAttributes(&fileExtendedAttributeList);

  // add to incremental list
  if (createInfo->storeIncrementalFileInfoFlag)
  {
    ConstString fileName;
    STRINGLIST_ITERATE(fileNameList,fileName)
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
  Errors error;

  assert(createInfo != NULL);
  assert(createInfo->jobOptions != NULL);
  assert(fileName != NULL);
  assert(fileInfo != NULL);

  printInfo(1,"Add special   '%s'...",String_cString(fileName));

  // get file extended attributes
  FileExtendedAttributeList fileExtendedAttributeList;
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
        createInfo->runningInfo.progress.error.count++;
        createInfo->runningInfo.progress.done.count++;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError(_("cannot get extended attributes for '%s' (error: %s)"),
                 String_cString(fileName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
      {
        createInfo->runningInfo.progress.error.count++;
        createInfo->runningInfo.progress.done.count++;
      }

      File_doneExtendedAttributes(&fileExtendedAttributeList);

      return error;
    }
  }

  if (!createInfo->jobOptions->noStorage)
  {
    // new special
    String           archiveEntryName = getArchiveEntryName(String_new(),fileName);
    ArchiveEntryInfo archiveEntryInfo;
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
      printError(_("cannot create new archive special entry '%s' (error: %s)"),
                 String_cString(fileName),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
      {
        createInfo->runningInfo.progress.error.count++;
        createInfo->runningInfo.progress.done.count++;
      }

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
      printError(_("cannot close archive special entry (error: %s)!"),
                 Error_getText(error)
                );

      STATUS_INFO_UPDATE(createInfo,fileName,NULL)
      {
        createInfo->runningInfo.progress.error.count++;
        createInfo->runningInfo.progress.done.count++;
      }

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

  // update running info
  STATUS_INFO_UPDATE(createInfo,fileName,NULL)
  {
    createInfo->runningInfo.progress.done.count++;
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

  assert(createInfo != NULL);

  // store entries
  EntryMsg entryMsg;
  byte     *buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  while (   (createInfo->failError == ERROR_NONE)
         && !isAborted(createInfo)
         && MsgQueue_get(&createInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER)
        )
  {
    // get name, check if own file (in temporary directory or storage file)
    ConstString name;
    bool        ownFileFlag = FALSE;
    switch (entryMsg.type)
    {
      case ENTRY_TYPE_FILE:
        name        = entryMsg.file.name;
        ownFileFlag =    String_startsWith(entryMsg.file.name,tmpDirectory)
                      || StringList_contains(&createInfo->storageFileList,entryMsg.file.name);
        break;
      case ENTRY_TYPE_IMAGE:
        name        = entryMsg.image.name;
        ownFileFlag =    String_startsWith(entryMsg.image.name,tmpDirectory)
                      || StringList_contains(&createInfo->storageFileList,entryMsg.image.name);
        break;
      case ENTRY_TYPE_DIRECTORY:
        name        = entryMsg.directory.name;
        ownFileFlag =    String_startsWith(entryMsg.directory.name,tmpDirectory)
                      || StringList_contains(&createInfo->storageFileList,entryMsg.directory.name);
        break;
      case ENTRY_TYPE_LINK:
        name        = entryMsg.link.name;
        ownFileFlag =    String_startsWith(entryMsg.link.name,tmpDirectory)
                      || StringList_contains(&createInfo->storageFileList,entryMsg.link.name);
        break;
      case ENTRY_TYPE_HARDLINK:
        {
          name = StringList_first(&entryMsg.hardLink.nameList,NULL);

          ConstString hardLinkName;
          STRINGLIST_ITERATEX(&entryMsg.hardLink.nameList,hardLinkName,!ownFileFlag)
          {
            ownFileFlag =    String_startsWith(hardLinkName,tmpDirectory)
                          || StringList_contains(&createInfo->storageFileList,hardLinkName);
          }
        }
        break;
      case ENTRY_TYPE_SPECIAL:
        name        = entryMsg.special.name;
        ownFileFlag =    String_startsWith(entryMsg.special.name,tmpDirectory)
                      || StringList_contains(&createInfo->storageFileList,entryMsg.special.name);
        break;
    }

    if (!ownFileFlag)
    {
      // pause create
      pauseCreate(createInfo);

      Errors error;

      switch (entryMsg.type)
      {
        case ENTRY_TYPE_FILE:
          error = storeFileEntry(createInfo,
                                 entryMsg.file.name,
                                 &entryMsg.file.fileInfo,
                                 entryMsg.file.fragmentNumber,
                                 entryMsg.file.fragmentCount,
                                 entryMsg.file.fragmentOffset,
                                 entryMsg.file.fragmentSize,
                                 buffer,
                                 BUFFER_SIZE
                                );
          if (error != ERROR_NONE) createInfo->failError = error;
          break;
        case ENTRY_TYPE_IMAGE:
          error = storeImageEntry(createInfo,
                                  entryMsg.image.name,
                                  &entryMsg.image.deviceInfo,
                                  entryMsg.image.fragmentNumber,
                                  entryMsg.image.fragmentCount,
                                  entryMsg.image.fragmentOffset,
                                  entryMsg.image.fragmentSize,
                                  buffer,
                                  BUFFER_SIZE
                                 );
          if (error != ERROR_NONE) createInfo->failError = error;
          break;
        case ENTRY_TYPE_DIRECTORY:
          error = storeDirectoryEntry(createInfo,
                                      entryMsg.directory.name,
                                      &entryMsg.directory.fileInfo
                                     );
          if (error != ERROR_NONE) createInfo->failError = error;
          break;
        case ENTRY_TYPE_LINK:
          error = storeLinkEntry(createInfo,
                                 entryMsg.link.name,
                                 &entryMsg.link.fileInfo
                                );
          if (error != ERROR_NONE) createInfo->failError = error;
          break;
        case ENTRY_TYPE_HARDLINK:
          error = storeHardLinkEntry(createInfo,
                                     &entryMsg.hardLink.nameList,
                                     &entryMsg.hardLink.fileInfo,
                                     entryMsg.hardLink.fragmentNumber,
                                     entryMsg.hardLink.fragmentCount,
                                     entryMsg.hardLink.fragmentOffset,
                                     entryMsg.hardLink.fragmentSize,
                                     buffer,
                                     BUFFER_SIZE
                                    );
          if (error != ERROR_NONE) createInfo->failError = error;
          break;
        case ENTRY_TYPE_SPECIAL:
          error = storeSpecialEntry(createInfo,
                                    entryMsg.special.name,
                                    &entryMsg.special.fileInfo
                                   );
          if (error != ERROR_NONE) createInfo->failError = error;
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
      printInfo(1,"Add '%s'...skipped (reason: own created file)\n",String_cString(name));

      STATUS_INFO_UPDATE(createInfo,name,NULL)
      {
        createInfo->runningInfo.progress.skipped.count++;
      }
    }

    // update running info and check if aborted
    SEMAPHORE_LOCKED_DO(&createInfo->runningInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      updateRunningInfo(createInfo,FALSE);
    }

    // free entry message
    freeEntryMsg(&entryMsg,NULL);

// NYI: is this really useful? (avoid that sum-collector-thread is slower than file-collector-thread)
    // slow down if too fast
    while (   !createInfo->collectorTotalSumDone
           && (createInfo->runningInfo.progress.done.count >= createInfo->runningInfo.progress.total.count)
          )
    {
      Misc_udelay(1000LL*US_PER_MS);
    }
  }

  // if error or abort terminated the entry queue
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
                      RunningInfoFunction          runningInfoFunction,
                      void                         *runningInfoUserData,
                      StorageVolumeRequestFunction storageVolumeRequestFunction,
                      void                         *storageVolumeRequestUserData,
                      IsPauseFunction              isPauseCreateFunction,
                      void                         *isPauseCreateUserData,
                      IsPauseFunction              isPauseStorageFunction,
                      void                         *isPauseStorageUserData,
                      IsAbortedFunction            isAbortedFunction,
                      void                         *isAbortedUserData,
                      LogHandle                    *logHandle
                     )
{

  assert(storageName != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);

  // init variables
  AutoFreeList autoFreeList;
  AutoFree_init(&autoFreeList);

  // check if storage name given
  if (String_isEmpty(storageName))
  {
    printError(_("no storage name given!"));
    AutoFree_cleanup(&autoFreeList);
    return ERROR_NO_STORAGE_NAME;
  }

  Errors error;

  // parse storage name
  StorageSpecifier storageSpecifier;
  Storage_initSpecifier(&storageSpecifier);
  error = Storage_parseName(&storageSpecifier,storageName);
  if (error != ERROR_NONE)
  {
    printError(_("cannot initialize storage '%s' (error: %s)"),
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
  IndexHandle indexHandle;
  if (Index_isAvailable())
  {
    error = Index_open(&indexHandle,masterIO,INDEX_TIMEOUT);
    if (error != ERROR_NONE)
    {
      printError(_("cannot open index (error: %s)"),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&indexHandle,{ Index_close(&indexHandle); });
  }

  // init create info
  CreateInfo createInfo;
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
                 CALLBACK_(runningInfoFunction,runningInfoUserData),
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
  String printableStorageName = Storage_getPrintableName(String_new(),&storageSpecifier,NULL);
  AUTOFREE_ADD(&autoFreeList,printableStorageName,{ String_delete(printableStorageName); });

  // get hostname, username
  String hostName = String_new();
  Network_getHostName(hostName);
  AUTOFREE_ADD(&autoFreeList,hostName,{ String_delete(hostName); });
  String userName = String_new();
  Misc_getCurrentUserName(userName);
  AUTOFREE_ADD(&autoFreeList,userName,{ String_delete(userName); });

  // init storage
  error = Storage_init(&createInfo.storageInfo,
                       masterIO,
                       &storageSpecifier,
                       jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK_(updateStorageProgress,&createInfo),
                       CALLBACK_(getNamePasswordFunction,getNamePasswordUserData),
                       CALLBACK_(storageVolumeRequestFunction,storageVolumeRequestUserData),
                       CALLBACK_(isPauseStorageFunction,isPauseStorageUserData),
                       CALLBACK_(isAbortedFunction,isAbortedUserData),
                       logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError(_("cannot initialize storage '%s' (error: %s)"),
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
  // check for write access
  directoryName = File_getDirectoryName(String_new(),storageSpecifier.archiveName);
  if (!Storage_isWritable(&createInfo.storageInfo,directoryName))
  {
    error = ERRORX_(WRITE_FILE,0,"%s",String_cString(storageSpecifier.archiveName));
    printError(_("cannot write storage (error: no write access for '%s')!"),
               String_cString(storageSpecifier.archiveName)
              );
    String_delete(directoryName);
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  String_delete(directoryName);
#endif

  String incrementalListFileName = String_new();
  if (   (createInfo.archiveType == ARCHIVE_TYPE_FULL)
      || (createInfo.archiveType == ARCHIVE_TYPE_INCREMENTAL)
      || (createInfo.archiveType == ARCHIVE_TYPE_DIFFERENTIAL)
      || !String_isEmpty(jobOptions->incrementalListFileName)
     )
  {
    // get increment list file name
    bool incrementalFileInfoExistFlag = FALSE;
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
        printError(_("cannot read incremental list file '%s' (error: %s)"),
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

    createInfo.storeIncrementalFileInfoFlag =    (createInfo.archiveType == ARCHIVE_TYPE_FULL)
                                              || (createInfo.archiveType == ARCHIVE_TYPE_INCREMENTAL);
  }
  AUTOFREE_ADD(&autoFreeList,incrementalListFileName,{ String_delete(incrementalListFileName); });

  IndexId entityId = INDEX_ID_NONE;
  if (Index_isAvailable())
  {
    // get/create index job UUID
    IndexId uuidId;
    if (!IndexUUID_find(&indexHandle,
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
                       )
       )
    {
      error = IndexUUID_new(&indexHandle,jobUUID,&uuidId);
      if (error != ERROR_NONE)
      {
        printError(_("cannot create index for '%s' (error: %s)!"),
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }

    // create new index entity
    error = IndexEntity_new(&indexHandle,
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
      printError(_("cannot create index for '%s' (error: %s)!"),
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    assert(!INDEX_ID_IS_NONE(entityId));
    DEBUG_TESTCODE() { (void)IndexEntity_purge(&indexHandle,entityId); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
    AUTOFREE_ADD(&autoFreeList,&entityId,{ (void)IndexEntity_unlock(&indexHandle,entityId); (void)IndexEntity_purge(&indexHandle,entityId); });
  }

  // create new archive
  IndexId uuidId;
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
    printError(_("cannot create archive file '%s' (error: %s)"),
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Archive_close(&createInfo.archiveHandle,FALSE); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&createInfo.archiveHandle,{ Archive_close(&createInfo.archiveHandle,FALSE); });

  // start collectors and storage thread
  ThreadPoolNode *collectorSumThreadNode = ThreadPool_run(&workerThreadPool,collectorSumThreadCode,&createInfo);
  assert(collectorSumThreadNode != NULL);
  ThreadPoolNode *collectorThreadNode = ThreadPool_run(&workerThreadPool,collectorThreadCode,&createInfo);
  assert(collectorThreadNode != NULL);
  ThreadPoolNode *collectorStorageThreadNode = ThreadPool_run(&workerThreadPool,storageThreadCode,&createInfo);
  assert(collectorStorageThreadNode != NULL);
  AUTOFREE_ADD(&autoFreeList,collectorSumThreadNode,{ ThreadPool_join(&workerThreadPool,collectorSumThreadNode); });
  AUTOFREE_ADD(&autoFreeList,collectorThreadNode,{ ThreadPool_join(&workerThreadPool,collectorThreadNode); });
  AUTOFREE_ADD(&autoFreeList,collectorStorageThreadNode,{ MsgQueue_setEndOfMsg(&createInfo.storageMsgQueue); ThreadPool_join(&workerThreadPool,collectorStorageThreadNode); });

  // start create threads
  uint createThreadCount = (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores();
  ThreadPoolSet createThreadSet;
  ThreadPool_initSet(&createThreadSet,&workerThreadPool);
  for (uint i = 0; i < createThreadCount; i++)
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
  if (createInfo.failError != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return createInfo.failError;
  }

  // close archive
  AUTOFREE_REMOVE(&autoFreeList,&createInfo.archiveHandle);
  error = Archive_close(&createInfo.archiveHandle,TRUE);
  if (error != ERROR_NONE)
  {
    printError(_("cannot close archive '%s' (error: %s)"),
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

  // final update of running info
  SEMAPHORE_LOCKED_DO(&createInfo.runningInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,2000)
  {
    updateRunningInfo(&createInfo,TRUE);
  }

  // update index
  if (Index_isAvailable())
  {
    assert(!INDEX_ID_IS_NONE(entityId));

    // update entity, uuid info (aggregated values)
    error = IndexEntity_updateInfos(&indexHandle,
                                    entityId
                                   );
    if (error != ERROR_NONE)
    {
      printError(_("cannot create index for '%s' (error: %s)!"),
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    error = IndexUUID_updateInfos(&indexHandle,
                                  uuidId
                                 );
    if (error != ERROR_NONE)
    {
      printError(_("cannot create index for '%s' (error: %s)!"),
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // unlock entity
    AUTOFREE_REMOVE(&autoFreeList,&entityId);
    (void)IndexEntity_unlock(&indexHandle,entityId);

    if (   (createInfo.failError == ERROR_NONE)
        && !createInfo.jobOptions->dryRun
        && !isAborted(&createInfo)
       )
    {
      // delete entity if nothing created
      error = IndexEntity_prune(&indexHandle,NULL,NULL,entityId);
      if (error != ERROR_NONE)
      {
        printError(_("cannot delete empty entity for '%s' (error: %s)!"),
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
      error = IndexEntity_delete(&indexHandle,NULL,NULL,entityId);
      if (error != ERROR_NONE)
      {
        printWarning(_("cannot delete entity for '%s' (error: %s)!"),
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
              createInfo.runningInfo.progress.done.count,
              BYTES_SHORT(createInfo.runningInfo.progress.done.size),
              BYTES_UNIT(createInfo.runningInfo.progress.done.size),
              createInfo.runningInfo.progress.done.size
             );
    printInfo(2,
              "%lu entries/%.1lf%s (%"PRIu64" bytes) skipped\n",
              createInfo.runningInfo.progress.skipped.count,
              BYTES_SHORT(createInfo.runningInfo.progress.skipped.size),
              BYTES_UNIT(createInfo.runningInfo.progress.skipped.size),
              createInfo.runningInfo.progress.skipped.size
             );
    printInfo(2,
              "%lu entries/%.1lf%s (%"PRIu64" bytes) with errors\n",
              createInfo.runningInfo.progress.error.count,
              BYTES_SHORT(createInfo.runningInfo.progress.error.size),
              BYTES_UNIT(createInfo.runningInfo.progress.error.size),
              createInfo.runningInfo.progress.error.size
             );
    logMessage(logHandle,
               LOG_TYPE_ALWAYS,
               "%lu entries/%.1lf%s (%"PRIu64" bytes) included, %lu entries skipped, %lu entries with errors",
               createInfo.runningInfo.progress.done.count,
               BYTES_SHORT(createInfo.runningInfo.progress.done.size),
               BYTES_UNIT(createInfo.runningInfo.progress.done.size),
               createInfo.runningInfo.progress.done.size,
               createInfo.runningInfo.progress.skipped.count,
               createInfo.runningInfo.progress.error.count
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
      String fileName = String_new();

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
      printError(_("cannot write incremental list file '%s' (error: %s)"),
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
  // Note: no AUTOFREE_REMOVE(&autoFreeList,&createInfo.storageInfo), it is identical with &createInfo
  Storage_done(&createInfo.storageInfo);

  // unmount devices
  AUTOFREE_REMOVE(&autoFreeList,&jobOptions->mountList);
  error = unmountAll(&jobOptions->mountList);
  if (error != ERROR_NONE)
  {
    printWarning(_("cannot unmount devices (error: %s)"),
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
  AUTOFREE_REMOVE(&autoFreeList,incrementalListFileName);
  String_delete(incrementalListFileName);
  AUTOFREE_REMOVE(&autoFreeList,&createInfo);
  doneCreateInfo(&createInfo);
  if (Index_isAvailable())
  {
    Index_close(&indexHandle);
  }
  AUTOFREE_REMOVE(&autoFreeList,&storageSpecifier);
  Storage_doneSpecifier(&storageSpecifier);
  AUTOFREE_REMOVE(&autoFreeList,userName);
  String_delete(userName);
  AUTOFREE_REMOVE(&autoFreeList,hostName);
  String_delete(hostName);
  AUTOFREE_REMOVE(&autoFreeList,printableStorageName);
  String_delete(printableStorageName);
  AutoFree_done(&autoFreeList);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
