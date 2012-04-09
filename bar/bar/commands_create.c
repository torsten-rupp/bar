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
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "lists.h"
#include "stringlists.h"
#include "threads.h"
#include "msgqueues.h"
#include "semaphores.h"
#include "dictionaries.h"

#include "errors.h"
#include "patterns.h"
#include "entrylists.h"
#include "patternlists.h"
#include "files.h"
#include "devices.h"
#include "filesystems.h"
#include "archive.h"
#include "sources.h"
#include "crypt.h"
#include "storage.h"
#include "misc.h"
#include "database.h"

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

typedef struct
{
  IncrementalFileStates state;
  FileCast              cast;
} IncrementalListInfo;

// create info
typedef struct
{
  String                      storageName;                        //
  const EntryList             *includeEntryList;                  // list of included entries
  const PatternList           *excludePatternList;                // list of exclude patterns
  const JobOptions            *jobOptions;
  ArchiveTypes                archiveType;                        // archive type to create
  bool                        *pauseCreateFlag;                   // TRUE for pause creation
  bool                        *pauseStorageFlag;                  // TRUE for pause storage
  bool                        *requestedAbortFlag;                // TRUE to abort create

  bool                        partialFlag;                        // TRUE for create incremental/differential archive
  Dictionary                  filesDictionary;                    // dictionary with files (used for incremental/differental backup)
  StorageFileHandle           storageFileHandle;                  // storage handle
  String                      archiveFileName;                    // archive file name
  time_t                      startTime;                          // start time [ms] (unix time)

  MsgQueue                    entryMsgQueue;                      // queue with entries to store

  Thread                      collectorSumThread;                 // files collector sum thread id
  bool                        collectorSumThreadExitFlag;
  bool                        collectorSumThreadExitedFlag;
  Thread                      collectorThread;                    // files collector thread id
  bool                        collectorThreadExitFlag;            // TRUE iff collector thread exited

  MsgQueue                    storageMsgQueue;                    // queue with waiting storage files
  Semaphore                   storageInfoLock;                    // lock semaphore for storage info
  struct
  {
    uint                      count;                              // number of current storage files
    uint64                    bytes;                              // number of bytes in current storage files
  }                           storageInfo;
  Thread                      storageThread;                      // storage thread id
  bool                        storageThreadExitFlag;
  StringList                  storageFileList;                    // list with stored storage files

  Errors                      failError;                          // failure error

  CreateStatusInfoFunction    statusInfoFunction;                 // status info call back
  void                        *statusInfoUserData;                // user data for status info call back
  CreateStatusInfo            statusInfo;                         // status info
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
  DatabaseHandle *databaseHandle;
  int64          storageId;                                       // database storage id
  String         fileName;                                        // temporary archive name
  uint64         fileSize;                                        // archive size
  String         destinationFileName;                             // destination archive name
} StorageMsg;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : readIncrementalList
* Purpose: read data of incremental list from file
* Input  : fileName        - file name
*          filesDictionary - files dictionary variable
* Output : -
* Return : ERROR_NONE if incremental list read in files dictionary,
*          error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors readIncrementalList(const String fileName,
                                 Dictionary   *filesDictionary
                                )
{
  void                *keyData;
  Errors              error;
  FileHandle          fileHandle;
  char                id[32];
  uint16              version;
  IncrementalListInfo incrementalListInfo;
  uint16              keyLength;

  assert(fileName != NULL);
  assert(filesDictionary != NULL);

  keyData = malloc(64*1024);
  if (keyData == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init variables
  Dictionary_clear(filesDictionary,NULL,NULL);

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
    Dictionary_add(filesDictionary,
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
*          filesDictionary - files dictionary
* Output : -
* Return : ERROR_NONE if incremental list file written, error code
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors writeIncrementalList(const String     fileName,
                                  const Dictionary *filesDictionary
                                 )
{
  assert(fileName != NULL);
  assert(filesDictionary != NULL);

  String                    directoryName;
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
  assert(filesDictionary != NULL);

  // create directory if not existing
  directoryName = File_getFilePathName(String_new(),fileName);
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
      error = ERRORX(NOT_A_DIRECTORY,0,String_cString(directoryName));
      String_delete(directoryName);
      return error;
    }
  }
  String_delete(directoryName);

  // get temporary name
  directory = File_getFilePathName(File_newFileName(),fileName);
  tmpFileName = File_newFileName();
  error = File_getTmpFileName(tmpFileName,NULL,directory);
  if (error != ERROR_NONE)
  {
    File_deleteFileName(tmpFileName);
    File_deleteFileName(directory);
    return error;
  }

  // open file
  error = File_open(&fileHandle,tmpFileName,FILE_OPEN_CREATE);
  if (error != ERROR_NONE)
  {
    File_delete(tmpFileName,FALSE);
    File_deleteFileName(tmpFileName);
    File_deleteFileName(directory);
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
    File_deleteFileName(tmpFileName);
    File_deleteFileName(directory);
    return error;
  }
  version = INCREMENTAL_LIST_FILE_VERSION;
  error = File_write(&fileHandle,&version,sizeof(version));
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    File_delete(tmpFileName,FALSE);
    File_deleteFileName(tmpFileName);
    File_deleteFileName(directory);
    return error;
  }

  // write entries
  Dictionary_initIterator(&dictionaryIterator,filesDictionary);
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

    error = File_write(&fileHandle,incrementalListInfo->cast,sizeof(incrementalListInfo->cast));
    if (error != ERROR_NONE) break;
    n = (uint16)keyLength;
    error = File_write(&fileHandle,&n,sizeof(n));
    if (error != ERROR_NONE) break;
    error = File_write(&fileHandle,keyData,n);
    if (error != ERROR_NONE) break;
  }
  Dictionary_doneIterator(&dictionaryIterator);

  // close file
  File_close(&fileHandle);
  if (error != ERROR_NONE)
  {
    File_delete(tmpFileName,FALSE);
    File_delete(tmpFileName,FALSE);
    File_deleteFileName(tmpFileName);
    File_deleteFileName(directory);
    return error;
  }

  // rename files
  error = File_rename(tmpFileName,fileName);
  if (error != ERROR_NONE)
  {
    File_delete(tmpFileName,FALSE);
    File_deleteFileName(tmpFileName);
    File_deleteFileName(directory);
    return error;
  }

  // free resources
  File_deleteFileName(tmpFileName);
  File_deleteFileName(directory);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : checkFileChanged
* Purpose: check if file changed
* Input  : filesDictionary - files dictionary
*          fileName        - file name
*          fileInfo        - file info with file cast data
* Output : -
* Return : TRUE iff file changed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool checkFileChanged(Dictionary     *filesDictionary,
                            const String   fileName,
                            const FileInfo *fileInfo
                           )
{
  union
  {
    void                *value;
    IncrementalListInfo *incrementalListInfo;
  } data;
  ulong length;

  assert(filesDictionary != NULL);
  assert(fileName != NULL);
  assert(fileInfo != NULL);

  // check if exists
  if (!Dictionary_find(filesDictionary,
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
  if (memcmp(data.incrementalListInfo->cast,&fileInfo->cast,sizeof(FileCast)) != 0)
  {
    return TRUE;
  }

  return FALSE;
}

/***********************************************************************\
* Name   : addIncrementalList
* Purpose: add file to incremental list
* Input  : filesDictionary - files dictionary
*          fileName        - file name
*          fileInfo        - file info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addIncrementalList(Dictionary     *filesDictionary,
                              const String   fileName,
                              const FileInfo *fileInfo
                             )
{
  IncrementalListInfo incrementalListInfo;

  assert(filesDictionary != NULL);
  assert(fileName != NULL);
  assert(fileInfo != NULL);

  incrementalListInfo.state = INCREMENTAL_FILE_STATE_ADDED;
  memcpy(incrementalListInfo.cast,fileInfo->cast,sizeof(FileCast));

  Dictionary_add(filesDictionary,
                 String_cString(fileName),
                 String_length(fileName),
                 &incrementalListInfo,
                 sizeof(incrementalListInfo)
                );
}

/***********************************************************************\
* Name   : updateStatusInfo
* Purpose: update status info
* Input  : createInfo - create info
* Output : -
* Return : TRUE to continue, FALSE if user aborted
* Notes  : -
\***********************************************************************/

LOCAL bool updateStatusInfo(const CreateInfo *createInfo)
{
  assert(createInfo != NULL);

  if (createInfo->statusInfoFunction != NULL)
  {
    return createInfo->statusInfoFunction(createInfo->statusInfoUserData,
                                          createInfo->failError,
                                          &createInfo->statusInfo
                                         );
  }
  else
  {
    return TRUE;
  }
}

/***********************************************************************\
* Name   : updateStorageStatusInfo
* Purpose: update storage info data
* Input  : createInfo        - create info
*          storageStatusInfo - storage status info
* Output : -
* Return : TRUE to continue, FALSE if user aborted
* Notes  : -
\***********************************************************************/

LOCAL bool updateStorageStatusInfo(CreateInfo              *createInfo,
                                   const StorageStatusInfo *storageStatusInfo
                                  )
{
  assert(createInfo != NULL);
  assert(storageStatusInfo != NULL);

  createInfo->statusInfo.volumeNumber   = storageStatusInfo->volumeNumber;
  createInfo->statusInfo.volumeProgress = storageStatusInfo->volumeProgress;

  return updateStatusInfo(createInfo);
}

/***********************************************************************\
* Name   : checkIsIncluded
* Purpose: check if filename is included
* Input  : includeEntryNode - include entry node
*          fileName         - file name
* Output : -
* Return : TRUE if excluded, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool checkIsIncluded(const EntryNode *includeEntryNode,
                           const String    fileName
                          )
{
  assert(includeEntryNode != NULL);
  assert(fileName != NULL);

  return Pattern_match(&includeEntryNode->pattern,fileName,PATTERN_MATCH_MODE_BEGIN);
}

/***********************************************************************\
* Name   : checkIsExcluded
* Purpose: check if filename is excluded
* Input  : excludePatternList - exclude pattern list
*          fileName           - file name
* Output : -
* Return : TRUE if excluded, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool checkIsExcluded(const PatternList *excludePatternList,
                           const String      fileName
                          )
{
  assert(excludePatternList != NULL);
  assert(fileName != NULL);

  return PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT);
}

/***********************************************************************\
* Name   : checkNoBackup
* Purpose: check if file .nobackup/.NOBACKUP exists in sub-directory
* Input  : pathName - path name
* Output : -
* Return : TRUE if .nobackup/.NOBACKUP exists and option
*          ignoreNoBackupFile is not set, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool checkNoBackup(const String pathName)
{
  String fileName;
  bool   haveNoBackupFlag;

  assert(pathName != NULL);

  haveNoBackupFlag = FALSE;
  if (!globalOptions.ignoreNoBackupFileFlag)
  {
    fileName = File_newFileName();
    haveNoBackupFlag |= File_exists(File_appendFileNameCString(File_setFileName(fileName,pathName),".nobackup"));
    haveNoBackupFlag |= File_exists(File_appendFileNameCString(File_setFileName(fileName,pathName),".NOBACKUP"));
    File_deleteFileName(fileName);
  }

  return haveNoBackupFlag;
}

/***********************************************************************\
* Name   : checkNoDumpAttribute
* Purpose: check if file attribute 'no dump' is set
* Input  : fileInfo   - file info
*          jobOptions - job options
* Output : -
* Return : TRUE if 'no dump' attribute is set and option ignoreNoDump is
*          not set, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool checkNoDumpAttribute(const FileInfo *fileInfo, const JobOptions *jobOptions)
{
  assert(fileInfo != NULL);
  assert(jobOptions != NULL);

  return !jobOptions->ignoreNoDumpAttributeFlag && ((fileInfo->attributes & FILE_ATTRIBUTE_NO_DUMP) != 0);
}

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
* Name   : appendFileToEntryList
* Purpose: append file to entry list
* Input  : entryMsgQueue - entry message queue
*          entryType     - entry type
*          name          - name (will be copied!)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendFileToEntryList(MsgQueue     *entryMsgQueue,
                                 EntryTypes   entryType,
                                 const String name
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

LOCAL void appendDirectoryToEntryList(MsgQueue     *entryMsgQueue,
                                      EntryTypes   entryType,
                                      const String name
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

LOCAL void appendLinkToEntryList(MsgQueue     *entryMsgQueue,
                                 EntryTypes   entryType,
                                 const String name
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

LOCAL void appendSpecialToEntryList(MsgQueue     *entryMsgQueue,
                                    EntryTypes   entryType,
                                    const String name
                                   )
{
  EntryMsg entryMsg;

  assert(entryMsgQueue != NULL);
  assert(name != NULL);

  // init
  entryMsg.entryType = entryType;
  entryMsg.fileType  = FILE_TYPE_SPECIAL;
  entryMsg.name      = String_duplicate(name);
//  StringList_init(&entryMsg.nameList);

  // put into message queue
  if (!MsgQueue_put(entryMsgQueue,&entryMsg,sizeof(entryMsg)))
  {
    freeEntryMsg(&entryMsg,NULL);
  }
}

/***********************************************************************\
* Name   : formatArchiveFileName
* Purpose: get archive file name
* Input  : fileName         - file name variable
*          formatMode       - format mode; see FORMAT_MODE_*
*          archiveType      - archive type; see ARCHIVE_TYPE_*
*          templateFileName - template file name
*          time             - time
*          partNumber       - part number (>=0 for parts, -1 for single
*                             archive)
*          lastPartFlag     - TRUE iff last part
* Output : -
* Return : formated file name
* Notes  : -
\***********************************************************************/

LOCAL String formatArchiveFileName(String       fileName,
                                   FormatModes  formatMode,
                                   ArchiveTypes archiveType,
                                   const String templateFileName,
                                   time_t       time,
                                   int          partNumber,
                                   bool         lastPartFlag
                                  )
{
  TextMacro textMacros[2];

  bool      partNumberFlag;
  struct tm tmStruct;
  ulong     i,j;
  char      format[4];
  char      buffer[256];
  size_t    length;
  ulong     divisor;
  ulong     n;
  uint      z;
  int       d;

  // expand named macros
  switch (archiveType)
  {
    case ARCHIVE_TYPE_NORMAL:       TEXT_MACRO_N_CSTRING(textMacros[0],"%type","normal");       break;
    case ARCHIVE_TYPE_FULL:         TEXT_MACRO_N_CSTRING(textMacros[0],"%type","full");         break;
    case ARCHIVE_TYPE_INCREMENTAL:  TEXT_MACRO_N_CSTRING(textMacros[0],"%type","incremental");  break;
    case ARCHIVE_TYPE_DIFFERENTIAL: TEXT_MACRO_N_CSTRING(textMacros[0],"%type","differential"); break;
    case ARCHIVE_TYPE_UNKNOWN:      TEXT_MACRO_N_CSTRING(textMacros[0],"%type","unknown");      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  switch (formatMode)
  {
    case FORMAT_MODE_ARCHIVE_FILE_NAME:
      TEXT_MACRO_N_CSTRING(textMacros[1],"%last",lastPartFlag?"-last":"");
      Misc_expandMacros(fileName,String_cString(templateFileName),textMacros,SIZE_OF_ARRAY(textMacros));
      break;
    case FORMAT_MODE_PATTERN:
      TEXT_MACRO_N_CSTRING(textMacros[1],"%last","(-last){0,1}");
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
      #endif /* NDEBUG */
  }

  // expand time macros, part number
  localtime_r(&time,&tmStruct);
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
              // %%
              String_remove(fileName,i,1);
              i += 1L;
              break;
            case '#':
              // %#
              String_remove(fileName,i,1);
              i += 1L;
              break;
            default:
              // format time part
              switch (String_index(fileName,i+1))
              {
                case 'E':
                case 'O':
                  // %Ex, %Ox
                  format[0] = '%';
                  format[1] = String_index(fileName,i+1);
                  format[2] = String_index(fileName,i+2);
                  format[3] = '\0';

                  String_remove(fileName,i,3);
                  break;
                default:
                  // %x
                  format[0] = '%';
                  format[1] = String_index(fileName,i+1);
                  format[2] = '\0';

                  String_remove(fileName,i,2);
                  break;
              }
              length = strftime(buffer,sizeof(buffer)-1,format,&tmStruct);

              // insert in string
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
        // #...
        switch (formatMode)
        {
          case FORMAT_MODE_ARCHIVE_FILE_NAME:
            if (partNumber != ARCHIVE_PART_NUMBER_NONE)
            {
              // find #...# and get max. divisor for part number
              divisor = 1;
              j = i+1L;
              while ((j < String_length(fileName) && String_index(fileName,j) == '#'))
              {
                j++;
                if (divisor < 1000000000) divisor*=10;
              }

              // replace #...# by part number
              n = partNumber;
              z = 0;
              while (divisor > 0)
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

  return fileName;
}

/***********************************************************************\
* Name   : formatIncrementalFileName
* Purpose: format incremental file name
* Input  : fileName         - file name variable
*          templateFileName - template file name
* Output : -
* Return : file name
* Notes  : -
\***********************************************************************/

LOCAL String formatIncrementalFileName(String       fileName,
                                       const String templateFileName
                                      )
{
  #define SEPARATOR_CHARS "-_"

  ulong i;
  char  ch;

  // remove all macros and leading and tailing separator characters
  String_clear(fileName);
  i = 0L;
  while (i < String_length(templateFileName))
  {
    ch = String_index(templateFileName,i);
    switch (ch)
    {
      case '%':
        i += 1L;
        if (i < String_length(templateFileName))
        {
          // removed previous separator characters
          String_trimRight(fileName,SEPARATOR_CHARS);

          ch = String_index(templateFileName,i);
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
                while (   (i < String_length(templateFileName))
                       && isalpha(ch)
                      )
                {
                  i += 1L;
                  ch = String_index(templateFileName,i);
                }
              }

              // discard following separator characters
              if (strchr(SEPARATOR_CHARS,ch) != NULL)
              {
                while (   (i < String_length(templateFileName))
                       && (strchr(SEPARATOR_CHARS,ch) != NULL)
                      )
                {
                  i += 1L;
                  ch = String_index(templateFileName,i);
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
  StringList          nameList;
  String              name;
  bool                abortFlag;
  EntryNode           *includeEntryNode;
  StringTokenizer     fileNameTokenizer;
  String              basePath;
  String              string;
  Errors              error;
  String              fileName;
  FileInfo            fileInfo;
  DirectoryListHandle directoryListHandle;
  DeviceInfo          deviceInfo;

  assert(createInfo != NULL);
  assert(createInfo->includeEntryList != NULL);
  assert(createInfo->excludePatternList != NULL);
  assert(createInfo->jobOptions != NULL);

  StringList_init(&nameList);
  name = String_new();

  abortFlag = FALSE;
  includeEntryNode = createInfo->includeEntryList->head;
  while (   !createInfo->collectorSumThreadExitFlag
         && !abortFlag
         && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
         && (createInfo->failError == ERROR_NONE)
         && (includeEntryNode != NULL)
        )
  {
    // pause
    while ((createInfo->pauseCreateFlag != NULL) && (*createInfo->pauseCreateFlag))
    {
      Misc_udelay(500*1000);
    }

    // find base path
    basePath = String_new();
    File_initSplitFileName(&fileNameTokenizer,includeEntryNode->string);
    if (File_getNextSplitFileName(&fileNameTokenizer,&string) && !Pattern_checkIsPattern(string))
    {
      if (String_length(string) > 0L)
      {
        File_setFileName(basePath,string);
      }
      else
      {
        File_setFileNameChar(basePath,FILES_PATHNAME_SEPARATOR_CHAR);
      }
    }
    while (File_getNextSplitFileName(&fileNameTokenizer,&string) && !Pattern_checkIsPattern(string))
    {
      File_appendFileName(basePath,string);
    }
    File_doneSplitFileName(&fileNameTokenizer);

    // find files
    StringList_append(&nameList,basePath);
    while (   !createInfo->collectorSumThreadExitFlag
           && (createInfo->failError == ERROR_NONE)
           && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
           && !StringList_isEmpty(&nameList)
          )
    {
      // pause
      while ((createInfo->pauseCreateFlag != NULL) && (*createInfo->pauseCreateFlag))
      {
        Misc_udelay(500*1000);
      }

      // get next directory to process
      name = StringList_getLast(&nameList,name);

      if (   checkIsIncluded(includeEntryNode,name)
          && !checkIsExcluded(createInfo->excludePatternList,name)
         )
      {

        // read file info
        error = File_getFileInfo(&fileInfo,name);
        if (error != ERROR_NONE)
        {
          continue;
        }

        if (   (fileInfo.type == FILE_TYPE_DIRECTORY)
            || !checkNoDumpAttribute(&fileInfo,createInfo->jobOptions)
           )
        {
          switch (fileInfo.type)
          {
            case FILE_TYPE_FILE:
              switch (includeEntryNode->type)
              {
                case ENTRY_TYPE_FILE:
                  if (   !createInfo->partialFlag
                      || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo)
                     )
                  {
                    createInfo->statusInfo.totalEntries++;
                    createInfo->statusInfo.totalBytes += fileInfo.size;
                    abortFlag |= !updateStatusInfo(createInfo);
                  }
                  break;
                case ENTRY_TYPE_IMAGE:
                  break;
              }
              break;
            case FILE_TYPE_DIRECTORY:
              if (   !checkNoBackup(name)
                  && !checkNoDumpAttribute(&fileInfo,createInfo->jobOptions)
                 )
              {
                switch (includeEntryNode->type)
                {
                  case ENTRY_TYPE_FILE:
                    if (   !createInfo->partialFlag
                        || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo)
                       )
                    {
                      createInfo->statusInfo.totalEntries++;
                      abortFlag |= !updateStatusInfo(createInfo);
                    }
                    break;
                  case ENTRY_TYPE_IMAGE:
                    break;
                }

                // open directory contents
                error = File_openDirectoryList(&directoryListHandle,name);
                if (error == ERROR_NONE)
                {
                  // read directory contents
                  fileName = String_new();
                  while (   !createInfo->collectorSumThreadExitFlag
                         && (createInfo->failError == ERROR_NONE)
                         && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
                         && !File_endOfDirectoryList(&directoryListHandle)
                        )
                  {
                    // pause
                    while ((createInfo->pauseCreateFlag != NULL) && (*createInfo->pauseCreateFlag))
                    {
                      Misc_udelay(500*1000);
                    }

                    // read next directory entry
                    error = File_readDirectoryList(&directoryListHandle,fileName);
                    if (error != ERROR_NONE)
                    {
                      continue;
                    }

                    if (   checkIsIncluded(includeEntryNode,fileName)
                        && !checkIsExcluded(createInfo->excludePatternList,fileName)
                       )
                    {
                      // read file info
                      error = File_getFileInfo(&fileInfo,fileName);
                      if (error != ERROR_NONE)
                      {
                        continue;
                      }

                      if (   (fileInfo.type == FILE_TYPE_DIRECTORY)
                          || !checkNoDumpAttribute(&fileInfo,createInfo->jobOptions)
                         )
                      {
                        switch (fileInfo.type)
                        {
                          case FILE_TYPE_FILE:
                            switch (includeEntryNode->type)
                            {
                              case ENTRY_TYPE_FILE:
                                if (   !createInfo->partialFlag
                                    || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo)
                                   )
                                {
                                  createInfo->statusInfo.totalEntries++;
                                  createInfo->statusInfo.totalBytes += fileInfo.size;
                                  abortFlag |= !updateStatusInfo(createInfo);
                                }
                                break;
                              case ENTRY_TYPE_IMAGE:
                                break;
                            }
                            break;
                          case FILE_TYPE_DIRECTORY:
                            // add to name list
                            StringList_append(&nameList,fileName);
                            break;
                          case FILE_TYPE_LINK:
                            switch (includeEntryNode->type)
                            {
                              case ENTRY_TYPE_FILE:
                                if (   !createInfo->partialFlag
                                    || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo)
                                   )
                                {
                                  createInfo->statusInfo.totalEntries++;
                                  abortFlag |= !updateStatusInfo(createInfo);
                                }
                                break;
                              case ENTRY_TYPE_IMAGE:
                                break;
                            }
                            break;
                          case FILE_TYPE_HARDLINK:
                            switch (includeEntryNode->type)
                            {
                              case ENTRY_TYPE_FILE:
                                if (  !createInfo->partialFlag
                                    || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo)
                                   )
                                {
                                  createInfo->statusInfo.totalEntries++;
                                  createInfo->statusInfo.totalBytes += fileInfo.size;
                                  abortFlag |= !updateStatusInfo(createInfo);
                                }
                                break;
                              case ENTRY_TYPE_IMAGE:
                                break;
                            }
                            break;
                          case FILE_TYPE_SPECIAL:
                            switch (includeEntryNode->type)
                            {
                              case ENTRY_TYPE_FILE:
                                if (  !createInfo->partialFlag
                                    || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo)
                                   )
                                {
                                  createInfo->statusInfo.totalEntries++;
                                  if (   (includeEntryNode->type == ENTRY_TYPE_IMAGE)
                                      && (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                                      && (fileInfo.size >= 0L)
                                     )
                                  {
                                    createInfo->statusInfo.totalBytes += fileInfo.size;
                                  }
                                  abortFlag |= !updateStatusInfo(createInfo);
                                }
                                break;
                              case ENTRY_TYPE_IMAGE:
                                if (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                                {
                                  createInfo->statusInfo.totalEntries++;
                                  if (fileInfo.size >= 0L) createInfo->statusInfo.totalBytes += fileInfo.size;
                                  abortFlag |= !updateStatusInfo(createInfo);
                                }
                                break;
                            }
                            break;
                          default:
                            break;
                        }
                      }
                    }
                  }
                  String_delete(fileName);

                  // close directory
                  File_closeDirectoryList(&directoryListHandle);
                }
              }
              break;
            case FILE_TYPE_LINK:
              switch (includeEntryNode->type)
              {
                case ENTRY_TYPE_FILE:
                  if (   !createInfo->partialFlag
                      || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo)
                     )
                  {
                    createInfo->statusInfo.totalEntries++;
                    abortFlag |= !updateStatusInfo(createInfo);
                  }
                  break;
                case ENTRY_TYPE_IMAGE:
                  break;
              }
              break;
            case FILE_TYPE_HARDLINK:
              switch (includeEntryNode->type)
              {
                case ENTRY_TYPE_FILE:
                  if (   !createInfo->partialFlag
                      || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo)
                     )
                  {
                    createInfo->statusInfo.totalEntries++;
                    createInfo->statusInfo.totalBytes += fileInfo.size;
                    abortFlag |= !updateStatusInfo(createInfo);
                  }
                  break;
                case ENTRY_TYPE_IMAGE:
                  break;
              }
              break;
            case FILE_TYPE_SPECIAL:
              switch (includeEntryNode->type)
              {
                case ENTRY_TYPE_FILE:
                  if (   !createInfo->partialFlag
                      || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo)
                     )
                  {
                    createInfo->statusInfo.totalEntries++;
                    abortFlag |= !updateStatusInfo(createInfo);
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

                    createInfo->statusInfo.totalEntries++;
                    if (fileInfo.size >= 0L) createInfo->statusInfo.totalBytes += fileInfo.size;
                    abortFlag |= !updateStatusInfo(createInfo);
                  }
                  break;
              }
              break;
            default:
              break;
          }
        }
      }
    }

    // free resources
    String_delete(basePath);

    // next include entry
    includeEntryNode = includeEntryNode->next;
  }

  // free resoures
  String_delete(name);
  StringList_done(&nameList);

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
  StringList          nameList;
  String              name;
  String              fileName;
  Dictionary          hardLinkDictionary;
  bool                abortFlag;
  EntryNode           *includeEntryNode;
  StringTokenizer     fileNameTokenizer;
  String              basePath;
  String              string;
  Errors              error;
  FileInfo            fileInfo;
  DirectoryListHandle directoryListHandle;
  DeviceInfo          deviceInfo;
  DictionaryIterator  dictionaryIterator;

  assert(createInfo != NULL);
  assert(createInfo->includeEntryList != NULL);
  assert(createInfo->excludePatternList != NULL);
  assert(createInfo->jobOptions != NULL);

  StringList_init(&nameList);
  name     = String_new();
  fileName = String_new();
  if (!Dictionary_init(&hardLinkDictionary,NULL,NULL))
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  abortFlag = FALSE;
  includeEntryNode = createInfo->includeEntryList->head;
  while (   !abortFlag
         && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
         && (createInfo->failError == ERROR_NONE)
         && (includeEntryNode != NULL)
        )
  {
    // pause
    while ((createInfo->pauseCreateFlag != NULL) && (*createInfo->pauseCreateFlag))
    {
      Misc_udelay(500*1000);
    }

    // find base path
    basePath = String_new();
    File_initSplitFileName(&fileNameTokenizer,includeEntryNode->string);
    if (File_getNextSplitFileName(&fileNameTokenizer,&string) && !Pattern_checkIsPattern(string))
    {
      if (String_length(string) > 0L)
      {
        File_setFileName(basePath,string);
      }
      else
      {
        File_setFileNameChar(basePath,FILES_PATHNAME_SEPARATOR_CHAR);
      }
    }
    while (File_getNextSplitFileName(&fileNameTokenizer,&string) && !Pattern_checkIsPattern(string))
    {
      File_appendFileName(basePath,string);
    }
    File_doneSplitFileName(&fileNameTokenizer);

    // find files
    StringList_append(&nameList,basePath);
    while (   !abortFlag
           && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
           && (createInfo->failError == ERROR_NONE)
           && !StringList_isEmpty(&nameList)
          )
    {
      // pause
      while ((createInfo->pauseCreateFlag != NULL) && (*createInfo->pauseCreateFlag))
      {
        Misc_udelay(500*1000);
      }

      // get next directory to process
      name = StringList_getLast(&nameList,name);

      if (   checkIsIncluded(includeEntryNode,name)
          && !checkIsExcluded(createInfo->excludePatternList,name)
         )
      {
        // read file info
        error = File_getFileInfo(&fileInfo,name);
        if (error != ERROR_NONE)
        {
          printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(name),Errors_getText(error));
          logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"access denied '%s'\n",String_cString(name));
          createInfo->statusInfo.errorEntries++;
          abortFlag |= !updateStatusInfo(createInfo);
          continue;
        }

        if (   (fileInfo.type == FILE_TYPE_DIRECTORY)
            || !checkNoDumpAttribute(&fileInfo,createInfo->jobOptions)
           )
        {
          switch (fileInfo.type)
          {
            case FILE_TYPE_FILE:
              switch (includeEntryNode->type)
              {
                case ENTRY_TYPE_FILE:
                  if (   !createInfo->partialFlag
                      || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo)
                     )
                  {
                    // add to store list
                    appendFileToEntryList(&createInfo->entryMsgQueue,
                                          ENTRY_TYPE_FILE,
                                          name
                                         );
                  }
                  break;
                case ENTRY_TYPE_IMAGE:
                  break;
              }
              break;
            case FILE_TYPE_DIRECTORY:
              if (!checkNoBackup(name))
              {
                switch (includeEntryNode->type)
                {
                  case ENTRY_TYPE_FILE:
                    if (   !createInfo->partialFlag
                        || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo)
                       )
                    {
                      // add to store list
                      appendDirectoryToEntryList(&createInfo->entryMsgQueue,
                                                 ENTRY_TYPE_FILE,
                                                 name
                                                );
                    }
                    break;
                  case ENTRY_TYPE_IMAGE:
                    break;
                }

                // open directory contents
                error = File_openDirectoryList(&directoryListHandle,name);
                if (error == ERROR_NONE)
                {
                  // read directory contents
                  while (   (createInfo->failError == ERROR_NONE)
                         && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
                         && !File_endOfDirectoryList(&directoryListHandle)
                        )
                  {
                    // pause
                    while ((createInfo->pauseCreateFlag != NULL) && (*createInfo->pauseCreateFlag))
                    {
                      Misc_udelay(500*1000);
                    }

                    // read next directory entry
                    error = File_readDirectoryList(&directoryListHandle,fileName);
                    if (error != ERROR_NONE)
                    {
                      printInfo(2,"Cannot read directory '%s' (error: %s) - skipped\n",String_cString(name),Errors_getText(error));
                      logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"access denied '%s'\n",String_cString(name));
                      createInfo->statusInfo.errorEntries++;
                      createInfo->statusInfo.errorBytes += (uint64)fileInfo.size;
                      abortFlag |= !updateStatusInfo(createInfo);
                      continue;
                    }

                    if (   checkIsIncluded(includeEntryNode,fileName)
                        && !checkIsExcluded(createInfo->excludePatternList,fileName)
                       )
                    {
                      // read file info
                      error = File_getFileInfo(&fileInfo,fileName);
                      if (error != ERROR_NONE)
                      {
                        printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(fileName),Errors_getText(error));
                        logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"access denied '%s'\n",String_cString(fileName));
                        createInfo->statusInfo.errorEntries++;
                        abortFlag |= !updateStatusInfo(createInfo);
                        continue;
                      }

                      if (   (fileInfo.type == FILE_TYPE_DIRECTORY)
                          || !checkNoDumpAttribute(&fileInfo,createInfo->jobOptions)
                         )
                      {
                        // detect file type
                        switch (fileInfo.type)
                        {
                          case FILE_TYPE_FILE:
                            switch (includeEntryNode->type)
                            {
                              case ENTRY_TYPE_FILE:
                                if (   !createInfo->partialFlag
                                    || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo)
                                   )
                                {
                                  // add to store list
                                  appendFileToEntryList(&createInfo->entryMsgQueue,
                                                        ENTRY_TYPE_FILE,
                                                        fileName
                                                       );
                                }
                                break;
                              case ENTRY_TYPE_IMAGE:
                                break;
                            }
                            break;
                          case FILE_TYPE_DIRECTORY:
                            // add to directory search list
                            StringList_append(&nameList,fileName);
                            break;
                          case FILE_TYPE_LINK:
                            switch (includeEntryNode->type)
                            {
                              case ENTRY_TYPE_FILE:
                                if (   !createInfo->partialFlag
                                    || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo)
                                   )
                                {
                                  // add to store list
                                  appendLinkToEntryList(&createInfo->entryMsgQueue,
                                                        ENTRY_TYPE_FILE,
                                                        fileName
                                                       );
                                }
                                break;
                              case ENTRY_TYPE_IMAGE:
                                break;
                            }
                            break;
                          case FILE_TYPE_HARDLINK:
                            switch (includeEntryNode->type)
                            {
                              case ENTRY_TYPE_FILE:
                                {
                                  union { void *value; HardLinkInfo *hardLinkInfo; } data;
                                  HardLinkInfo hardLinkInfo;

                                  if (   !createInfo->partialFlag
                                      || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo)
                                     )
                                  {
                                    if (Dictionary_find(&hardLinkDictionary,
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
                                        // found last hardlink -> add to store list
                                        appendHardLinkToEntryList(&createInfo->entryMsgQueue,
                                                                  ENTRY_TYPE_FILE,
                                                                  &data.hardLinkInfo->nameList
                                                                 );

                                        // clear entry
                                        Dictionary_rem(&hardLinkDictionary,
                                                       &fileInfo.id,
                                                       sizeof(fileInfo.id),
                                                       NULL,
                                                       NULL
                                                      );
                                      }
                                    }
                                    else
                                    {
                                      // create hard link name list
                                      hardLinkInfo.count = fileInfo.linkCount;
                                      StringList_init(&hardLinkInfo.nameList);
                                      StringList_append(&hardLinkInfo.nameList,fileName);

                                      if (!Dictionary_add(&hardLinkDictionary,
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
                            }
                            break;
                          case FILE_TYPE_SPECIAL:
                            switch (includeEntryNode->type)
                            {
                              case ENTRY_TYPE_FILE:
                                if (   !createInfo->partialFlag
                                    || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo)
                                   )
                                {
                                  // add to store list
                                  appendSpecialToEntryList(&createInfo->entryMsgQueue,
                                                           ENTRY_TYPE_FILE,
                                                           fileName
                                                          );
                                }
                                break;
                              case ENTRY_TYPE_IMAGE:
                                if (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                                {
                                  // add to store list
                                  appendSpecialToEntryList(&createInfo->entryMsgQueue,
                                                           ENTRY_TYPE_IMAGE,
                                                           fileName
                                                          );
                                }
                                break;
                            }
                            break;
                          default:
                            printInfo(2,"Unknown type of file '%s' - skipped\n",String_cString(fileName));
                            logMessage(LOG_TYPE_ENTRY_TYPE_UNKNOWN,"unknown type '%s'\n",String_cString(fileName));
                            createInfo->statusInfo.errorEntries++;
                            createInfo->statusInfo.errorBytes += (uint64)fileInfo.size;
                            abortFlag |= !updateStatusInfo(createInfo);
                            break;
                        }
                      }
                      else
                      {
                        if (checkNoDumpAttribute(&fileInfo,createInfo->jobOptions))
                        {
                          logMessage(LOG_TYPE_ENTRY_EXCLUDED,"excluded '%s' (no dump attribute)\n",String_cString(fileName));
                        }
                        else
                        {
                          logMessage(LOG_TYPE_ENTRY_EXCLUDED,"excluded '%s'\n",String_cString(fileName));
                        }
                        createInfo->statusInfo.skippedEntries++;
                        createInfo->statusInfo.skippedBytes += fileInfo.size;
                        abortFlag |= !updateStatusInfo(createInfo);
                      }
                    }
                    else
                    {
                      logMessage(LOG_TYPE_ENTRY_EXCLUDED,"excluded '%s'\n",String_cString(fileName));
                      createInfo->statusInfo.skippedEntries++;
                      createInfo->statusInfo.skippedBytes += fileInfo.size;
                      abortFlag |= !updateStatusInfo(createInfo);
                    }
                  }

                  // close directory
                  File_closeDirectoryList(&directoryListHandle);
                }
                else
                {
                  printInfo(2,"Cannot open directory '%s' (error: %s) - skipped\n",String_cString(name),Errors_getText(error));
                  logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"access denied '%s'\n",String_cString(name));
                  createInfo->statusInfo.errorEntries++;
                  abortFlag |= !updateStatusInfo(createInfo);
                }
              }
              else
              {
                logMessage(LOG_TYPE_ENTRY_EXCLUDED,"excluded '%s' (.nobackup file)\n",String_cString(name));
              }
              break;
            case FILE_TYPE_LINK:
              switch (includeEntryNode->type)
              {
                case ENTRY_TYPE_FILE:
                  if (  !createInfo->partialFlag
                      || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo)
                    )
                  {
                    // add to store list
                    appendLinkToEntryList(&createInfo->entryMsgQueue,
                                          ENTRY_TYPE_FILE,
                                          name
                                         );
                  }
                  break;
                case ENTRY_TYPE_IMAGE:
                  break;
              }
              break;
            case FILE_TYPE_HARDLINK:
              switch (includeEntryNode->type)
              {
                case ENTRY_TYPE_FILE:
                  if (   !createInfo->partialFlag
                      || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo)
                     )
                  {
                    union { void *value; HardLinkInfo *hardLinkInfo; } data;
                    HardLinkInfo hardLinkInfo;

                    if (   !createInfo->partialFlag
                        || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo)
                       )
                    {
                      if (Dictionary_find(&hardLinkDictionary,
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
                          // found last hardlink -> add to store list
                          appendHardLinkToEntryList(&createInfo->entryMsgQueue,
                                                    ENTRY_TYPE_FILE,
                                                    &data.hardLinkInfo->nameList
                                                   );

                          // clear entry
                          Dictionary_rem(&hardLinkDictionary,
                                         &fileInfo.id,
                                         sizeof(fileInfo.id),
                                         NULL,
                                         NULL
                                        );
                        }
                      }
                      else
                      {
                        // create hard link name list
                        hardLinkInfo.count = fileInfo.linkCount;
                        StringList_init(&hardLinkInfo.nameList);
                        StringList_append(&hardLinkInfo.nameList,name);

                        if (!Dictionary_add(&hardLinkDictionary,
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
              }
              break;
            case FILE_TYPE_SPECIAL:
              switch (includeEntryNode->type)
              {
                case ENTRY_TYPE_FILE:
                  if (   !createInfo->partialFlag
                      || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo)
                     )
                  {
                    // add to store list
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
                      printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(name),Errors_getText(error));
                      logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"access denied '%s'\n",String_cString(name));
                      createInfo->statusInfo.errorEntries++;
                      abortFlag |= !updateStatusInfo(createInfo);
                      continue;
                    }
                    UNUSED_VARIABLE(deviceInfo);

                    // add to store list
                    appendSpecialToEntryList(&createInfo->entryMsgQueue,
                                             ENTRY_TYPE_IMAGE,
                                             name
                                            );
                  }
                  break;
              }
              break;
            default:
              printInfo(2,"Unknown type of file '%s' - skipped\n",String_cString(name));
              logMessage(LOG_TYPE_ENTRY_TYPE_UNKNOWN,"unknown type '%s'\n",String_cString(name));
              createInfo->statusInfo.errorEntries++;
              createInfo->statusInfo.errorBytes += (uint64)fileInfo.size;
              abortFlag |= !updateStatusInfo(createInfo);
              break;
          }
        }
        else
        {
          if (checkNoDumpAttribute(&fileInfo,createInfo->jobOptions))
          {
            logMessage(LOG_TYPE_ENTRY_EXCLUDED,"excluded '%s' (no dump attribute)\n",String_cString(name));
          }
          else
          {
            logMessage(LOG_TYPE_ENTRY_EXCLUDED,"excluded '%s'\n",String_cString(name));
          }
          createInfo->statusInfo.skippedEntries++;
          createInfo->statusInfo.skippedBytes += fileInfo.size;
          abortFlag |= !updateStatusInfo(createInfo);
        }
      }
      else
      {
        logMessage(LOG_TYPE_ENTRY_EXCLUDED,"excluded '%s'\n",String_cString(name));
        createInfo->statusInfo.skippedEntries++;
        createInfo->statusInfo.skippedBytes += fileInfo.size;
        abortFlag |= !updateStatusInfo(createInfo);
      }
    }

    // free resources
    String_delete(basePath);

    // next include entry
    includeEntryNode = includeEntryNode->next;
  }

  // add incomplete hard link entries to file list (not all hard links found)
//???
union { const void *value; const uint64 *id; } keyData;
union { void *value; HardLinkInfo *hardLinkInfo; } data;
  Dictionary_initIterator(&dictionaryIterator,&hardLinkDictionary);
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

  MsgQueue_setEndOfMsg(&createInfo->entryMsgQueue);

  // free resoures
  Dictionary_done(&hardLinkDictionary,NULL,NULL); //???(DictionaryFreeFunction)freeHardLinkEntry,NULL);
  String_delete(fileName);
  String_delete(name);
  StringList_done(&nameList);

  createInfo->collectorThreadExitFlag = TRUE;
}

/*---------------------------------------------------------------------*/

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

  String_delete(storageMsg->destinationFileName);
  String_delete(storageMsg->fileName);
}

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
* Name   : storeArchiveFile
* Purpose: call back to store archive
* Input  : userData       - user data
*          databaseHandle - database handle or NULL if no database
*          storageId      - database id of storage
*          fileName       - archive file name
*          partNumber     - part number or -1 if no parts
*          lastPartFlag   - TRUE iff last archive part, FALSE otherwise
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeArchiveFile(void           *userData,
                              DatabaseHandle *databaseHandle,
                              int64          storageId,
                              String         fileName,
                              int            partNumber,
                              bool           lastPartFlag
                             )
{
  CreateInfo    *createInfo = (CreateInfo*)userData;
  Errors        error;
  FileInfo      fileInfo;
  String        destinationFileName;
  StorageMsg    storageMsg;
  SemaphoreLock semaphoreLock;

  assert(createInfo != NULL);
  assert(fileName != NULL);

  // get file info
  error = File_getFileInfo(&fileInfo,fileName);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get destination file name
  destinationFileName = formatArchiveFileName(String_new(),
                                              FORMAT_MODE_ARCHIVE_FILE_NAME,
                                              createInfo->archiveType,
                                              createInfo->archiveFileName,
                                              createInfo->startTime,
                                              partNumber,
                                              lastPartFlag
                                             );

  // send to storage controller
  storageMsg.databaseHandle      = databaseHandle;
  storageMsg.storageId           = storageId;
  storageMsg.fileName            = String_duplicate(fileName);
  storageMsg.fileSize            = fileInfo.size;
  storageMsg.destinationFileName = destinationFileName;
  storageInfoIncrement(createInfo,fileInfo.size);
  if (!MsgQueue_put(&createInfo->storageMsgQueue,&storageMsg,sizeof(storageMsg)))
  {
    freeStorageMsg(&storageMsg,NULL);
    return ERROR_NONE;
  }

  // update info
  createInfo->statusInfo.archiveTotalBytes += fileInfo.size;
  updateStatusInfo(createInfo);

  // wait for space in temporary directory
  if (globalOptions.maxTmpSize > 0)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&createInfo->storageInfoLock,SEMAPHORE_LOCK_TYPE_READ)
    {
      while (   (createInfo->storageInfo.count > 2)                           // more than 2 archives are waiting
             && (createInfo->storageInfo.bytes > globalOptions.maxTmpSize))   // temporary space above limit is used
      {
        Semaphore_waitModified(&createInfo->storageInfoLock);
      }
    }
  }

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

LOCAL void storageThreadCode(CreateInfo *createInfo)
{
  #define MAX_RETRIES 3

  byte                       *buffer;
  String                     storageName;
  String                     printableStorageName;
  bool                       abortFlag;
  StorageMsg                 storageMsg;
  Errors                     error;
  FileInfo                   fileInfo;
  FileHandle                 fileHandle;
  uint                       retryCount;
  ulong                      bufferLength;
  String                     pattern;
  String                     storagePath;
  String                     fileName;
  int64                      oldStorageId;
  StorageDirectoryListHandle storageDirectoryListHandle;

  assert(createInfo != NULL);

  // allocate resources
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  storageName          = String_new();
  printableStorageName = String_new();

  // initial pre-processing
  if ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
  {
    if (createInfo->failError == ERROR_NONE)
    {
      // pause
      while ((createInfo->pauseStorageFlag != NULL) && (*createInfo->pauseStorageFlag))
      {
        Misc_udelay(500*1000);
      }

      // initial pre-process
      error = Storage_preProcess(&createInfo->storageFileHandle,TRUE);
      if (error != ERROR_NONE)
      {
        printError("Cannot pre-process storage (error: %s)!\n",
                   Errors_getText(error)
                  );
        createInfo->failError = error;
      }
    }
  }

  // store data
  abortFlag = FALSE;
  while (MsgQueue_get(&createInfo->storageMsgQueue,&storageMsg,NULL,sizeof(storageMsg)))
  {
    if (   !abortFlag
        && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
       )
    {
      if (createInfo->failError == ERROR_NONE)
      {
        // pause
        while ((createInfo->pauseStorageFlag != NULL) && (*createInfo->pauseStorageFlag))
        {
          Misc_udelay(500*1000);
        }

        // pre-process
        error = Storage_preProcess(&createInfo->storageFileHandle,FALSE);
        if (error != ERROR_NONE)
        {
          printError("Cannot pre-process file '%s' (error: %s)!\n",
                     String_cString(storageMsg.fileName),
                     Errors_getText(error)
                    );
          createInfo->failError = error;

          // delete database index
          if (indexDatabaseHandle != NULL)
          {
            Index_delete(indexDatabaseHandle,storageMsg.storageId);
          }

          // free resources
          File_delete(storageMsg.fileName,FALSE);
          storageInfoDecrement(createInfo,storageMsg.fileSize);
          freeStorageMsg(&storageMsg,NULL);
          continue;
        }

        // get file info
        error = File_getFileInfo(&fileInfo,storageMsg.fileName);
        if (error != ERROR_NONE)
        {
          printError("Cannot get information for file '%s' (error: %s)!\n",
                     String_cString(storageMsg.fileName),
                     Errors_getText(error)
                    );
          createInfo->failError = error;

          // delete database index
          if (indexDatabaseHandle != NULL)
          {
            Index_delete(indexDatabaseHandle,storageMsg.storageId);
          }

          // free resources
          File_delete(storageMsg.fileName,FALSE);
          storageInfoDecrement(createInfo,storageMsg.fileSize);
          freeStorageMsg(&storageMsg,NULL);
          continue;
        }

        // get storage name
        Storage_getHandleName(storageName,
                              &createInfo->storageFileHandle,
                              storageMsg.destinationFileName
                             );
        Storage_getPrintableName(printableStorageName,storageName);

        // open file to store
        #ifndef NDEBUG
          printInfo(0,"Store '%s' to '%s'...",String_cString(storageMsg.fileName),String_cString(printableStorageName));
        #else /* not NDEBUG */
          printInfo(0,"Store archive '%s'...",String_cString(printableStorageName));
        #endif /* NDEBUG */
        error = File_open(&fileHandle,storageMsg.fileName,FILE_OPEN_READ);
        if (error != ERROR_NONE)
        {
          printInfo(0,"FAIL!\n");
          printError("Cannot open file '%s' (error: %s)!\n",
                     String_cString(storageMsg.fileName),
                     Errors_getText(error)
                    );
          createInfo->failError = error;

          // delete database index
          if (indexDatabaseHandle != NULL)
          {
            Index_delete(indexDatabaseHandle,storageMsg.storageId);
          }

          // free resources
          File_delete(storageMsg.fileName,FALSE);
          storageInfoDecrement(createInfo,storageMsg.fileSize);
          freeStorageMsg(&storageMsg,NULL);
          continue;
        }

        // store file
        retryCount = 0;
        do
        {
          // pause
          while ((createInfo->pauseStorageFlag != NULL) && (*createInfo->pauseStorageFlag))
          {
            Misc_udelay(500*1000);
          }

          // next try
          if (retryCount > MAX_RETRIES) break;
          retryCount++;

          // create storage file
          error = Storage_create(&createInfo->storageFileHandle,
                                 storageMsg.destinationFileName,
                                 fileInfo.size
                                );
          if (error != ERROR_NONE)
          {
            // output error message, store error
            if (retryCount > MAX_RETRIES)
            {
              // output error message, store error
              printInfo(0,"FAIL!\n");
              printError("Cannot store file '%s' (error: %s)\n",
                         String_cString(printableStorageName),
                         Errors_getText(error)
                        );

              // fatal error -> stop
              createInfo->failError = error;
              break;
            }
            else
            {
              // error -> retry
              continue;
            }
          }

          // update status info, check for abort
          String_set(createInfo->statusInfo.storageName,printableStorageName);
          abortFlag |= !updateStatusInfo(createInfo);

          // store data
          File_seek(&fileHandle,0);
          do
          {
            // pause
            while ((createInfo->pauseStorageFlag != NULL) && (*createInfo->pauseStorageFlag))
            {
              Misc_udelay(500*1000);
            }

            error = File_read(&fileHandle,buffer,BUFFER_SIZE,&bufferLength);
            if (error != ERROR_NONE)
            {
              // output error message, store error
              printInfo(0,"FAIL!\n");
              printError("Cannot read file '%s' (error: %s)!\n",
                         String_cString(printableStorageName),
                         Errors_getText(error)
                        );

              // fatal error -> stop
              createInfo->failError = error;
              break;
            }
            error = Storage_write(&createInfo->storageFileHandle,buffer,bufferLength);
            if (error != ERROR_NONE)
            {
fprintf(stderr,"%s, %d: error=%x\n",__FILE__,__LINE__,error);
              if (retryCount > MAX_RETRIES)
              {
                // output error message, store error
                printInfo(0,"FAIL!\n");
                printError("Cannot write file '%s' (error: %s)!\n",
                           String_cString(printableStorageName),
                           Errors_getText(error)
                          );

                // fatal error -> stop
                createInfo->failError = error;
                break;
              }
              else
              {
                // error -> retry
                break;
              }
            }

            // update status info, check for abort
            createInfo->statusInfo.archiveDoneBytes += (uint64)bufferLength;
            abortFlag |= !updateStatusInfo(createInfo);

            // on fatal error/abort -> stop
            if (   (createInfo->failError != ERROR_NONE)
                || ((createInfo->requestedAbortFlag != NULL) && (*createInfo->requestedAbortFlag))
               )
            {
              // fatal error/abort -> stop
              break;
            }
          }
          while (!File_eof(&fileHandle));

          // close storage file
          Storage_close(&createInfo->storageFileHandle);

          // on fatal error/abort -> stop
          if (   (createInfo->failError != ERROR_NONE)
              || ((createInfo->requestedAbortFlag != NULL) && (*createInfo->requestedAbortFlag))
             )
          {
            // fatal error/abort -> stop
            break;
          }
        }
        while (error != ERROR_NONE);

        // close file to store
        File_close(&fileHandle);

        if (createInfo->failError == ERROR_NONE)
        {
          printInfo(0,"ok\n");
          logMessage(LOG_TYPE_STORAGE,"stored '%s'\n",String_cString(printableStorageName));
        }

        // update database index and set state
        if (   (createInfo->failError == ERROR_NONE)
            && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
           )
        {
          if (indexDatabaseHandle != NULL)
          {
            // delete old indizes for same storage file
            if (createInfo->failError == ERROR_NONE)
            {
              while (Index_findByName(indexDatabaseHandle,
                                      printableStorageName,
                                      &oldStorageId,
                                      NULL,
                                      NULL
                                     )
                    )
              {
                Index_delete(indexDatabaseHandle,oldStorageId);
              }
              while ((error == ERROR_NONE) && (oldStorageId != DATABASE_ID_NONE));
              if (error != ERROR_NONE)
              {
                printError("Cannot update index for storage '%s' (error: %s)!\n",
                           String_cString(printableStorageName),
                           Errors_getText(error)
                          );
                createInfo->failError = error;
              }
            }

            // set database storage name, size
            if (createInfo->failError == ERROR_NONE)
            {
              error = Index_update(indexDatabaseHandle,
                                   storageMsg.storageId,
                                   printableStorageName,
                                   fileInfo.size
                                  );
              if (error != ERROR_NONE)
              {
                printError("Cannot update index for storage '%s' (error: %s)!\n",
                           String_cString(printableStorageName),
                           Errors_getText(error)
                          );
                createInfo->failError = error;
              }
            }

            // set database state
            if (createInfo->failError == ERROR_NONE)
            {
              error = Index_setState(indexDatabaseHandle,
                                     storageMsg.storageId,
                                     INDEX_STATE_OK,
                                     Misc_getCurrentDateTime(),
                                     NULL
                                    );
              if (error != ERROR_NONE)
              {
                printError("Cannot update index for storage '%s' (error: %s)!\n",
                           String_cString(printableStorageName),
                           Errors_getText(error)
                          );
                createInfo->failError = error;
              }
            }
          }
        }

        // post-process
        if (   (createInfo->failError == ERROR_NONE)
            && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
           )
        {
          error = Storage_postProcess(&createInfo->storageFileHandle,FALSE);
          if (error != ERROR_NONE)
          {
            printError("Cannot post-process storage file '%s' (error: %s)!\n",
                       String_cString(storageMsg.fileName),
                       Errors_getText(error)
                      );
            createInfo->failError = error;
          }
        }

        // check error/aborted
        if (   (createInfo->failError != ERROR_NONE)
            || ((createInfo->requestedAbortFlag != NULL) && (*createInfo->requestedAbortFlag))
           )
        {
          if (createInfo->failError != ERROR_NONE)
          {
            // error -> set database state
            if (indexDatabaseHandle != NULL)
            {
              Index_setState(indexDatabaseHandle,
                             storageMsg.storageId,
                             INDEX_STATE_ERROR,
                             0LL,
                             "%s (error code: %d)",
                             Errors_getText(error),
                             error
                            );
            }
          }
          else if ((createInfo->requestedAbortFlag != NULL) && (*createInfo->requestedAbortFlag))
          {
            // aborted -> delete database index
            if (indexDatabaseHandle != NULL)
            {
              Index_delete(indexDatabaseHandle,storageMsg.storageId);
            }
          }

          // free resources
          File_delete(storageMsg.fileName,FALSE);
          freeStorageMsg(&storageMsg,NULL);
          storageInfoDecrement(createInfo,storageMsg.fileSize);
          continue;
        }

        // add to list of stored storage files
        StringList_append(&createInfo->storageFileList,storageMsg.destinationFileName);
      }
    }

    // delete source file
    error = File_delete(storageMsg.fileName,FALSE);
    if (error != ERROR_NONE)
    {
      printWarning("Cannot delete file '%s' (error: %s)!\n",
                   String_cString(storageMsg.fileName),
                   Errors_getText(error)
                  );
    }

    // update storage info
    storageInfoDecrement(createInfo,storageMsg.fileSize);

    // free resources
    freeStorageMsg(&storageMsg,NULL);
  }

  // final post-processing
  if ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
  {
    if (createInfo->failError == ERROR_NONE)
    {
      // pause
      while ((createInfo->pauseStorageFlag != NULL) && (*createInfo->pauseStorageFlag))
      {
        Misc_udelay(500*1000);
      }

      error = Storage_postProcess(&createInfo->storageFileHandle,TRUE);
      if (error != ERROR_NONE)
      {
        printError("Cannot post-process storage (error: %s)!\n",
                   Errors_getText(error)
                  );
        createInfo->failError = error;
      }
    }
  }

  // delete old storage files
  if ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
  {
    if (createInfo->failError == ERROR_NONE)
    {
      if (globalOptions.deleteOldArchiveFilesFlag)
      {
        pattern = formatArchiveFileName(String_new(),
                                        FORMAT_MODE_PATTERN,
                                        createInfo->archiveType,
                                        createInfo->archiveFileName,
                                        createInfo->startTime,
                                        ARCHIVE_PART_NUMBER_NONE,
                                        FALSE
                                       );

        storagePath = File_getFilePathName(String_new(),createInfo->storageName);
        error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                          storagePath,
                                          createInfo->jobOptions
                                         );
        if (error == ERROR_NONE)
        {
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
                Storage_delete(&createInfo->storageFileHandle,fileName);
              }
            }
          }
          String_delete(fileName);

          Storage_closeDirectoryList(&storageDirectoryListHandle);
        }
        String_delete(storagePath);
        String_delete(pattern);
      }
    }
  }

  // free resoures
  String_delete(printableStorageName);
  String_delete(storageName);
  free(buffer);

  createInfo->storageThreadExitFlag = TRUE;
}

/***********************************************************************\
* Name   : storeFileEntry
* Purpose: store a file entry into archive
* Input  : archiveInfo                  - archive info
*          compressExcludePatternList   - exclude compression pattern
*                                         list
*          jobOptions                   - job options; see JobOptions
*          createInfo                   - create info structure
*          storeIncrementalFileInfoFlag - TRUE to store incremental
*                                         file data
*          fileName                     - file name to store
*          buffer                       - buffer for temporary data
*          bufferSize                   - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeFileEntry(ArchiveInfo       *archiveInfo,
                            const PatternList *compressExcludePatternList,
                            JobOptions        *jobOptions,
                            CreateInfo        *createInfo,
                            bool              storeIncrementalFileInfoFlag,
                            const String      fileName,
                            byte              *buffer,
                            uint              bufferSize
                           )
{
  Errors           error;
  FileInfo         fileInfo;
  FileHandle       fileHandle;
  bool             byteCompressFlag;
  bool             deltaCompressFlag;
  SourceHandle     sourceHandle;
  ArchiveEntryInfo archiveEntryInfo;
  ulong            bufferLength;
  uint             percentageDone;
  double           ratio;

  assert(archiveInfo != NULL);
  assert(jobOptions != NULL);
  assert(createInfo != NULL);
  assert(buffer != NULL);
  assert(fileName != NULL);

  printInfo(1,"Add '%s'...",String_cString(fileName));

  // get file info
  error = File_getFileInfo(&fileInfo,fileName);
  if (error != ERROR_NONE)
  {
    if (jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
      logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"access denied '%s'\n",String_cString(fileName));
      createInfo->statusInfo.errorEntries++;
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get info for '%s' (error: %s)\n",
                 String_cString(fileName),
                 Errors_getText(error)
                );
      return error;
    }
  }

  if (!jobOptions->noStorageFlag)
  {
    // open file
    error = File_open(&fileHandle,fileName,FILE_OPEN_READ|FILE_OPEN_NO_CACHE);
    if (error != ERROR_NONE)
    {
      if (jobOptions->skipUnreadableFlag)
      {
        printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
        logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"open file failed '%s'\n",String_cString(fileName));
        createInfo->statusInfo.errorEntries++;
        createInfo->statusInfo.errorBytes += (uint64)fileInfo.size;
        return ERROR_NONE;
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError("Cannot open file '%s' (error: %s)\n",
                   String_cString(fileName),
                   Errors_getText(error)
                  );
        return error;
      }
    }

    // check if file data should be compressed
    byteCompressFlag =    (fileInfo.size > (int64)globalOptions.compressMinFileSize)
                       && !PatternList_match(compressExcludePatternList,fileName,PATTERN_MATCH_MODE_EXACT);

    // check if file data should be delta compressed
    deltaCompressFlag = FALSE;
    if (byteCompressFlag && Compress_isCompressed(archiveInfo->jobOptions->compressAlgorithm.delta))
    {
      // get source for delta-compression
      error = Source_openEntry(&sourceHandle,
                               NULL,
                               fileName,
                               jobOptions
                              );
      if      (error == ERROR_NONE)
      {
        deltaCompressFlag = TRUE;
      }
      else if (jobOptions->forceDeltaCompressionFlag)
      {
        printInfo(1,"FAIL\n");
        printError("Cannot open source file for '%s' (error: %s)\n",
                   String_cString(fileName),
                   Errors_getText(error)
                  );
        File_close(&fileHandle);
        return error;
      }
      else
      {
        printWarning("File '%s' not delta compressed (no source file found)\n",String_cString(fileName));
        logMessage(LOG_TYPE_WARNING,
                   "File '%s' not delta compressed (no source file found)\n",
                   String_cString(fileName)
                  );
      }
    }

    // create new archive file entry
    error = Archive_newFileEntry(archiveInfo,
                                 &archiveEntryInfo,
                                 deltaCompressFlag?&sourceHandle:NULL,
                                 fileName,
                                 &fileInfo,
                                 deltaCompressFlag?Source_getName(&sourceHandle):NULL,
                                 deltaCompressFlag,
                                 byteCompressFlag
                                );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive file entry '%s' (error: %s)\n",
                 String_cString(fileName),
                 Errors_getText(error)
                );
      if (deltaCompressFlag)
      {
        Source_closeEntry(&sourceHandle);
      }
      File_close(&fileHandle);
      return error;
    }
    String_set(createInfo->statusInfo.name,fileName);
    createInfo->statusInfo.entryDoneBytes  = 0LL;
    createInfo->statusInfo.entryTotalBytes = fileInfo.size;
    updateStatusInfo(createInfo);

    // write file content to archive
    error = ERROR_NONE;
    do
    {
      // pause
      while ((createInfo->pauseCreateFlag != NULL) && (*createInfo->pauseCreateFlag))
      {
        Misc_udelay(500*1000);
      }

      File_read(&fileHandle,buffer,bufferSize,&bufferLength);
      if (bufferLength > 0)
      {
        error = Archive_writeData(&archiveEntryInfo,buffer,bufferLength,1);
        createInfo->statusInfo.doneBytes += (uint64)bufferLength;
        createInfo->statusInfo.entryDoneBytes += (uint64)bufferLength;
        createInfo->statusInfo.archiveBytes = createInfo->statusInfo.archiveTotalBytes+Archive_getSize(archiveInfo);
        createInfo->statusInfo.compressionRatio = 100.0-(createInfo->statusInfo.archiveTotalBytes+Archive_getSize(archiveInfo))*100.0/createInfo->statusInfo.doneBytes;
        updateStatusInfo(createInfo);

        percentageDone = (uint)((createInfo->statusInfo.entryDoneBytes*100LL)/createInfo->statusInfo.entryTotalBytes);
        printInfo(2,"%3d%%\b\b\b\b",percentageDone);
      }
    }
    while (   ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
           && (bufferLength > 0)
           && (createInfo->failError == ERROR_NONE)
           && (error == ERROR_NONE)
          );
    if ((createInfo->requestedAbortFlag != NULL) && (*createInfo->requestedAbortFlag))
    {
      printInfo(1,"ABORTED\n");
      File_close(&fileHandle);
      Archive_closeEntry(&archiveEntryInfo);
      return FALSE;
    }
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot store archive file (error: %s)!\n",
                 Errors_getText(error)
                );
      File_close(&fileHandle);
      Archive_closeEntry(&archiveEntryInfo);
      return error;
    }
    printInfo(2,"    \b\b\b\b");

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive file entry (error: %s)!\n",
                 Errors_getText(error)
                );
      File_close(&fileHandle);
      return error;
    }

    // free resources
    if (deltaCompressFlag)
    {
      Source_closeEntry(&sourceHandle);
    }

    // close file
    File_close(&fileHandle);

    // get compression ratio
    if (   Compress_isCompressed(archiveEntryInfo.file.byteCompressAlgorithm)
        && (archiveEntryInfo.file.chunkFileData.fragmentSize > 0LL)
       )
    {
      ratio = 100.0-archiveEntryInfo.file.chunkFileData.info.size*100.0/archiveEntryInfo.file.chunkFileData.fragmentSize;
    }
    else
    {
      ratio = 0.0;
    }

    if (!jobOptions->dryRunFlag)
    {
      printInfo(1,"ok (%llu bytes, ratio %.1f%%)\n",fileInfo.size,ratio);
      logMessage(LOG_TYPE_ENTRY_OK,"added '%s'\n",String_cString(fileName));
    }
    else
    {
      printInfo(1,"ok (%llu bytes, dry-run)\n",fileInfo.size);
    }
    createInfo->statusInfo.doneEntries++;
    updateStatusInfo(createInfo);
  }
  else
  {
    printInfo(1,"ok (%llu bytes, not stored)\n",fileInfo.size);
  }

  // add to incremental list
  if (storeIncrementalFileInfoFlag)
  {
    addIncrementalList(&createInfo->filesDictionary,fileName,&fileInfo);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeImageEntry
* Purpose: store an image entry into archive
* Input  : archiveInfo                  - archive info
*          compressExcludePatternList   - exclude compression pattern
*                                         list
*          jobOptions                   - job options; see JobOptions
*          createInfo                   - create info structure
*          storeIncrementalFileInfoFlag - not used
*          deviceName                   - device name
*          buffer                       - buffer for temporary data
*          bufferSize                   - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeImageEntry(ArchiveInfo       *archiveInfo,
                             const PatternList *compressExcludePatternList,
                             JobOptions        *jobOptions,
                             CreateInfo        *createInfo,
                             const String      deviceName,
                             byte              *buffer,
                             uint              bufferSize
                            )
{
  Errors           error;
  DeviceInfo       deviceInfo;
  DeviceHandle     deviceHandle;
  bool             fileSystemFlag;
  FileSystemHandle fileSystemHandle;
  bool             byteCompressFlag;
  bool             deltaCompressFlag;
  SourceHandle     sourceHandle;
  uint64           block;
  uint             bufferBlockCount;
  uint             maxBufferBlockCount;
  uint             percentageDone;
  double           ratio;
  ArchiveEntryInfo archiveEntryInfo;

  assert(archiveInfo != NULL);
  assert(jobOptions != NULL);
  assert(createInfo != NULL);
  assert(buffer != NULL);
  assert(deviceName != NULL);

  printInfo(1,"Add '%s'...",String_cString(deviceName));

  if (!jobOptions->noStorageFlag)
  {
    // get device info
    error = Device_getDeviceInfo(&deviceInfo,deviceName);
    if (error != ERROR_NONE)
    {
      if (jobOptions->skipUnreadableFlag)
      {
        printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
        logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"access denied '%s'\n",String_cString(deviceName));
        createInfo->statusInfo.errorEntries++;
        return ERROR_NONE;
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError("Cannot open device '%s' (error: %s)\n",
                   String_cString(deviceName),
                   Errors_getText(error)
                  );
        return error;
      }
    }

    // check device block size, get max. blocks in buffer
    if (deviceInfo.blockSize > bufferSize)
    {
      printInfo(1,"FAIL\n");
      printError("Device block size %llu on '%s' is to big (max: %llu)\n",
                 deviceInfo.blockSize,
                 String_cString(deviceName),
                 bufferSize
                );
      return ERROR_INVALID_DEVICE_BLOCK_SIZE;
    }
    assert(deviceInfo.blockSize != 0);
    maxBufferBlockCount = bufferSize/deviceInfo.blockSize;

    // open device
    error = Device_open(&deviceHandle,deviceName,DEVICE_OPENMODE_READ);
    if (error != ERROR_NONE)
    {
      if (jobOptions->skipUnreadableFlag)
      {
        printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
        logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"open device failed '%s'\n",String_cString(deviceName));
        createInfo->statusInfo.errorEntries++;
        createInfo->statusInfo.errorBytes += (uint64)deviceInfo.size;
        return ERROR_NONE;
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError("Cannot open device '%s' (error: %s)\n",
                   String_cString(deviceName),
                   Errors_getText(error)
                  );
        return error;
      }
    }
    String_set(createInfo->statusInfo.name,deviceName);
    createInfo->statusInfo.entryDoneBytes  = 0LL;
    createInfo->statusInfo.entryTotalBytes = deviceInfo.size;
    updateStatusInfo(createInfo);

    // check if device contain a known file system or raw image should be stored
    if (!jobOptions->rawImagesFlag)
    {
      fileSystemFlag = (FileSystem_init(&fileSystemHandle,&deviceHandle) == ERROR_NONE);
    }
    else
    {
      fileSystemFlag = FALSE;
    }

    // check if image data should be compressed
    byteCompressFlag =    (deviceInfo.size > (int64)globalOptions.compressMinFileSize)
                       && !PatternList_match(compressExcludePatternList,deviceName,PATTERN_MATCH_MODE_EXACT);

    // check if file data should be delta compressed
    deltaCompressFlag = FALSE;
    if (byteCompressFlag && Compress_isCompressed(archiveInfo->jobOptions->compressAlgorithm.delta))
    {
      // get source for delta-compression
      error = Source_openEntry(&sourceHandle,
                               NULL,
                               deviceName,
                               jobOptions
                              );
      if (error == ERROR_NONE)
      {
        deltaCompressFlag = TRUE;
      }
      else
      {
        if (jobOptions->forceDeltaCompressionFlag)
        {
          printInfo(1,"FAIL\n");
          printError("Cannot open source file for '%s' (error: %s)\n",
                     String_cString(deviceName),
                     Errors_getText(error)
                    );
          Device_close(&deviceHandle);
          return error;
        }
        else
        {
          logMessage(LOG_TYPE_WARNING,
                     "Image of device '%s' not delta compressed (no source file found)\n",
                     String_cString(deviceName)
                    );
        }
      }
    }

    // create new archive image entry
    error = Archive_newImageEntry(archiveInfo,
                                  &archiveEntryInfo,
                                  deltaCompressFlag?&sourceHandle:NULL,
                                  deviceName,
                                  &deviceInfo,
                                  deltaCompressFlag?Source_getName(&sourceHandle):NULL,
                                  deltaCompressFlag,
                                  byteCompressFlag
                                 );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive image entry '%s' (error: %s)\n",
                 String_cString(deviceName),
                 Errors_getText(error)
                );
      if (deltaCompressFlag)
      {
        Source_closeEntry(&sourceHandle);
      }
      Device_close(&deviceHandle);
      return error;
    }

    // write device content to archive
    error = ERROR_NONE;
    block = 0LL;
    while (   ((int64)(block*(uint64)deviceInfo.blockSize) < deviceInfo.size)
           && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
           && (createInfo->failError == ERROR_NONE)
           && (error == ERROR_NONE)
          )
    {
      // pause
      while ((createInfo->pauseCreateFlag != NULL) && (*createInfo->pauseCreateFlag))
      {
        Misc_udelay(500*1000);
      }

      // read blocks info buffer
      bufferBlockCount = 0;
      while (   ((int64)(block*(uint64)deviceInfo.blockSize) < deviceInfo.size)
             && (bufferBlockCount < maxBufferBlockCount)
            )
      {
        if (!fileSystemFlag || FileSystem_blockIsUsed(&fileSystemHandle,block*(uint64)deviceInfo.blockSize))
        {
          // read single block
          error = Device_seek(&deviceHandle,block*(uint64)deviceInfo.blockSize);
          if (error != ERROR_NONE) break;
          error = Device_read(&deviceHandle,buffer+bufferBlockCount*deviceInfo.blockSize,deviceInfo.blockSize,NULL);
          if (error != ERROR_NONE) break;
        }
        else
        {
          // not used -> store as "0"-block
          memset(buffer+bufferBlockCount*deviceInfo.blockSize,0,deviceInfo.blockSize);
        }
        block++;
        bufferBlockCount++;
      }

      // write blocks content to archive
      if (bufferBlockCount > 0)
      {
        error = Archive_writeData(&archiveEntryInfo,buffer,bufferBlockCount*deviceInfo.blockSize,deviceInfo.blockSize);
        createInfo->statusInfo.doneBytes += (uint64)bufferBlockCount*(uint64)deviceInfo.blockSize;
        createInfo->statusInfo.entryDoneBytes += (uint64)bufferBlockCount*(uint64)deviceInfo.blockSize;
        createInfo->statusInfo.archiveBytes = createInfo->statusInfo.archiveTotalBytes+Archive_getSize(archiveInfo);
        createInfo->statusInfo.compressionRatio = 100.0-(createInfo->statusInfo.archiveTotalBytes+Archive_getSize(archiveInfo))*100.0/createInfo->statusInfo.doneBytes;
        updateStatusInfo(createInfo);

        percentageDone = (uint)((createInfo->statusInfo.entryDoneBytes*100LL)/createInfo->statusInfo.entryTotalBytes);
        printInfo(2,"%3d%%\b\b\b\b",percentageDone);
      }
    }
    if ((createInfo->requestedAbortFlag != NULL) && (*createInfo->requestedAbortFlag))
    {
      printInfo(1,"ABORTED\n");
      Device_close(&deviceHandle);
      Archive_closeEntry(&archiveEntryInfo);
      return error;
    }
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot store archive file (error: %s)!\n",
                 Errors_getText(error)
                );
      Device_close(&deviceHandle);
      Archive_closeEntry(&archiveEntryInfo);
      return error;
    }
    printInfo(2,"    \b\b\b\b");

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive image entry (error: %s)!\n",
                 Errors_getText(error)
                );
      Device_close(&deviceHandle);
      return error;
    }

    // done file system
    if (fileSystemFlag)
    {
      FileSystem_done(&fileSystemHandle);
    }

    // free resources
    if (deltaCompressFlag)
    {
      Source_closeEntry(&sourceHandle);
    }

    // close device
    Device_close(&deviceHandle);

    // get compression ratio
    if (   (   Compress_isCompressed(archiveEntryInfo.image.deltaCompressAlgorithm)
            || Compress_isCompressed(archiveEntryInfo.image.byteCompressAlgorithm)
           )
        && (archiveEntryInfo.image.chunkImageData.blockCount > 0)
       )
    {
      ratio = 100.0-archiveEntryInfo.image.chunkImageData.info.size*100.0/(archiveEntryInfo.image.chunkImageData.blockCount*(uint64)deviceInfo.blockSize);
    }
    else
    {
      ratio = 0.0;
    }

    if (!jobOptions->dryRunFlag)
    {
      printInfo(1,"ok (%llu bytes, ratio %.1f%%)\n",deviceInfo.size,ratio);
      logMessage(LOG_TYPE_ENTRY_OK,"added '%s'\n",String_cString(deviceName));
    }
    else
    {
      printInfo(1,"ok (%llu bytes, dry-run)\n",deviceInfo.size);
    }
    createInfo->statusInfo.doneEntries++;
    updateStatusInfo(createInfo);
  }
  else
  {
    printInfo(1,"ok (%llu bytes, not stored)\n",deviceInfo.size);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeDirectoryEntry
* Purpose: store a directory entry into archive
* Input  : archiveInfo                  - archive info
*          jobOptions                   - job options; see JobOptions
*          createInfo                   - create info structure
*          storeIncrementalFileInfoFlag - TRUE to store incremental
*                                         file data
*          directoryName                - directory name to store
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeDirectoryEntry(ArchiveInfo  *archiveInfo,
                                 JobOptions   *jobOptions,
                                 CreateInfo   *createInfo,
                                 bool         storeIncrementalFileInfoFlag,
                                 const String directoryName
                                )
{
  Errors           error;
  FileInfo         fileInfo;
  ArchiveEntryInfo archiveEntryInfo;

  assert(archiveInfo != NULL);
  assert(jobOptions != NULL);
  assert(createInfo != NULL);
  assert(directoryName != NULL);

  printInfo(1,"Add '%s'...",String_cString(directoryName));

  // get file info
  error = File_getFileInfo(&fileInfo,directoryName);
  if (error != ERROR_NONE)
  {
    if (jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
      logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"access denied '%s'\n",String_cString(directoryName));
      createInfo->statusInfo.errorEntries++;
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get info for '%s' (error: %s)\n",
                 String_cString(directoryName),
                 Errors_getText(error)
                );
      return error;
    }
  }

  if (!jobOptions->noStorageFlag)
  {
    // new directory
    error = Archive_newDirectoryEntry(archiveInfo,
                                      &archiveEntryInfo,
                                      directoryName,
                                      &fileInfo
                                     );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive directory entry '%s' (error: %s)\n",
                 String_cString(directoryName),
                 Errors_getText(error)
                );
      logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"open failed '%s'\n",String_cString(directoryName));
      return error;
    }

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive directory entry (error: %s)!\n",
                 Errors_getText(error)
                );
      return error;
    }

    if (!jobOptions->dryRunFlag)
    {
      printInfo(1,"ok\n");
      logMessage(LOG_TYPE_ENTRY_OK,"added '%s'\n",String_cString(directoryName));
    }
    else
    {
      printInfo(1,"ok (dry-run)\n");
    }
    createInfo->statusInfo.doneEntries++;
    updateStatusInfo(createInfo);
  }
  else
  {
    printInfo(1,"ok (not stored)\n");
  }

  // add to incremental list
  if (storeIncrementalFileInfoFlag)
  {
    addIncrementalList(&createInfo->filesDictionary,directoryName,&fileInfo);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeLinkEntry
* Purpose: store a link entry into archive
* Input  : archiveInfo                  - archive info
*          jobOptions                   - job options; see JobOptions
*          createInfo                   - create info structure
*          storeIncrementalFileInfoFlag - TRUE to store incremental
*                                         file data
*          linkName                     - link name to store
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeLinkEntry(ArchiveInfo  *archiveInfo,
                            JobOptions   *jobOptions,
                            CreateInfo   *createInfo,
                            bool         storeIncrementalFileInfoFlag,
                            const String linkName
                           )
{
  Errors           error;
  FileInfo         fileInfo;
  String           fileName;
  ArchiveEntryInfo archiveEntryInfo;

  assert(archiveInfo != NULL);
  assert(jobOptions != NULL);
  assert(createInfo != NULL);
  assert(linkName != NULL);

  printInfo(1,"Add '%s'...",String_cString(linkName));

  // get file info
  error = File_getFileInfo(&fileInfo,linkName);
  if (error != ERROR_NONE)
  {
    if (jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
      logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"access denied '%s'\n",String_cString(linkName));
      createInfo->statusInfo.errorEntries++;
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get info for '%s' (error: %s)\n",
                 String_cString(linkName),
                 Errors_getText(error)
                );
      return error;
    }
  }

  if (!jobOptions->noStorageFlag)
  {
    // read link
    fileName = String_new();
    error = File_readLink(fileName,linkName);
    if (error != ERROR_NONE)
    {
      if (jobOptions->skipUnreadableFlag)
      {
        printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
        logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"open failed '%s'\n",String_cString(linkName));
        createInfo->statusInfo.errorEntries++;
        createInfo->statusInfo.errorBytes += (uint64)fileInfo.size;
        String_delete(fileName);
        return ERROR_NONE;
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError("Cannot read link '%s' (error: %s)\n",
                   String_cString(linkName),
                   Errors_getText(error)
                  );
        String_delete(fileName);
        return error;
      }
    }

    // new link
    error = Archive_newLinkEntry(archiveInfo,
                                 &archiveEntryInfo,
                                 linkName,
                                 fileName,
                                 &fileInfo
                                );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive link entry '%s' (error: %s)\n",
                 String_cString(linkName),
                 Errors_getText(error)
                );
      String_delete(fileName);
      return error;
    }

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive link entry (error: %s)!\n",
                 Errors_getText(error)
                );
      String_delete(fileName);
      return error;
    }

    if (!jobOptions->dryRunFlag)
    {
      printInfo(1,"ok\n");
      logMessage(LOG_TYPE_ENTRY_OK,"added '%s'\n",String_cString(linkName));
    }
    else
    {
      printInfo(1,"ok (dry-run)\n");
    }
    createInfo->statusInfo.doneEntries++;
    updateStatusInfo(createInfo);

    // free resources
    String_delete(fileName);
  }
  else
  {
    printInfo(1,"ok (not stored)\n");
  }

  // add to incremental list
  if (storeIncrementalFileInfoFlag)
  {
    addIncrementalList(&createInfo->filesDictionary,linkName,&fileInfo);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeHardLinkEntry
* Purpose: store a hard link entry into archive
* Input  : archiveInfo                  - archive info
*          compressExcludePatternList   - exclude compression pattern
*                                         list
*          jobOptions                   - job options; see JobOptions
*          createInfo                   - create info structure
*          storeIncrementalFileInfoFlag - TRUE to store incremental
*                                         file data
*          nameList                     - hard link name list to store
*          buffer                       - buffer for temporary data
*          bufferSize                   - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeHardLinkEntry(ArchiveInfo       *archiveInfo,
                                const PatternList *compressExcludePatternList,
                                JobOptions        *jobOptions,
                                CreateInfo        *createInfo,
                                bool              storeIncrementalFileInfoFlag,
                                const StringList  *nameList,
                                byte              *buffer,
                                uint              bufferSize
                               )
{
  Errors           error;
  FileInfo         fileInfo;
  FileHandle       fileHandle;
  bool             byteCompressFlag;
  bool             deltaCompressFlag;
  SourceHandle     sourceHandle;
  ArchiveEntryInfo archiveEntryInfo;
  ulong            bufferLength;
  uint             percentageDone;
  double           ratio;
  const StringNode *stringNode;
  String           name;

  assert(archiveInfo != NULL);
  assert(jobOptions != NULL);
  assert(createInfo != NULL);
  assert(nameList != NULL);
  assert(!StringList_isEmpty(nameList));
  assert(buffer != NULL);

  printInfo(1,"Add '%s'...",String_cString(nameList->head->string));

  // get file info
  error = File_getFileInfo(&fileInfo,nameList->head->string);
  if (error != ERROR_NONE)
  {
    if (jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
      logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"access denied '%s'\n",String_cString(nameList->head->string));
      createInfo->statusInfo.errorEntries += StringList_count(nameList);
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get info for '%s' (error: %s)\n",
                 String_cString(nameList->head->string),
                 Errors_getText(error)
                );
      return error;
    }
  }

  if (!jobOptions->noStorageFlag)
  {
    // open file
    error = File_open(&fileHandle,nameList->head->string,FILE_OPEN_READ|FILE_OPEN_NO_CACHE);
    if (error != ERROR_NONE)
    {
      if (jobOptions->skipUnreadableFlag)
      {
        printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
        logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"open file failed '%s'\n",String_cString(nameList->head->string));
        createInfo->statusInfo.errorEntries += StringList_count(nameList);
        createInfo->statusInfo.errorBytes += (uint64)StringList_count(nameList)*(uint64)fileInfo.size;
        return ERROR_NONE;
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError("Cannot open file '%s' (error: %s)\n",
                   String_cString(nameList->head->string),
                   Errors_getText(error)
                  );
        return error;
      }
    }

    // check if file data should be compressed
    byteCompressFlag =    (fileInfo.size > (int64)globalOptions.compressMinFileSize)
                       && !PatternList_matchStringList(compressExcludePatternList,nameList,PATTERN_MATCH_MODE_EXACT);

    // check if file data should be delta compressed
    deltaCompressFlag = FALSE;
    if (byteCompressFlag && Compress_isCompressed(archiveInfo->jobOptions->compressAlgorithm.delta))
    {
      // get source for delta-compression
      STRINGLIST_ITERATE(nameList,stringNode,name)
      {
        error = Source_openEntry(&sourceHandle,
                                 NULL,
                                 name,
                                 jobOptions
                                );
        if (error == ERROR_NONE) break;
      }
      if (error == ERROR_NONE)
      {
        deltaCompressFlag = TRUE;
      }
      else
      {
        if (jobOptions->forceDeltaCompressionFlag)
        {
          printInfo(1,"FAIL\n");
          printError("Cannot open source file for '%s' (error: %s)\n",
                     String_cString(nameList->head->string),
                     Errors_getText(error)
                    );
          File_close(&fileHandle);
          return error;
        }
        else
        {
          logMessage(LOG_TYPE_WARNING,
                     "File '%s' not delta compressed (no source file found)\n",
                     String_cString(nameList->head->string)
                    );
        }
      }
    }

    // create new archive hard link entry
    error = Archive_newHardLinkEntry(archiveInfo,
                                     &archiveEntryInfo,
                                     deltaCompressFlag?&sourceHandle:NULL,
                                     nameList,
                                     &fileInfo,
                                     deltaCompressFlag?Source_getName(&sourceHandle):NULL,
                                     deltaCompressFlag,
                                     byteCompressFlag
                                    );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive file entry '%s' (error: %s)\n",
                 String_cString(nameList->head->string),
                 Errors_getText(error)
                );
      File_close(&fileHandle);
      return error;
    }
    String_set(createInfo->statusInfo.name,nameList->head->string);
    createInfo->statusInfo.entryDoneBytes  = 0LL;
    createInfo->statusInfo.entryTotalBytes = fileInfo.size;
    updateStatusInfo(createInfo);

    // write hard link content to archive
    error = ERROR_NONE;
    do
    {
      // pause
      while ((createInfo->pauseCreateFlag != NULL) && (*createInfo->pauseCreateFlag))
      {
        Misc_udelay(500*1000);
      }

      File_read(&fileHandle,buffer,bufferSize,&bufferLength);
      if (bufferLength > 0)
      {
        error = Archive_writeData(&archiveEntryInfo,buffer,bufferLength,1);
        createInfo->statusInfo.doneBytes += (uint64)StringList_count(nameList)*(uint64)bufferLength;
        createInfo->statusInfo.entryDoneBytes += (uint64)StringList_count(nameList)*(uint64)bufferLength;
        createInfo->statusInfo.archiveBytes = createInfo->statusInfo.archiveTotalBytes+Archive_getSize(archiveInfo);
        createInfo->statusInfo.compressionRatio = 100.0-(createInfo->statusInfo.archiveTotalBytes+Archive_getSize(archiveInfo))*100.0/createInfo->statusInfo.doneBytes;
        updateStatusInfo(createInfo);

        percentageDone = (uint)((createInfo->statusInfo.entryDoneBytes*100LL)/createInfo->statusInfo.entryTotalBytes);
        printInfo(2,"%3d%%\b\b\b\b",percentageDone);
      }
    }
    while (   ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
           && (bufferLength > 0)
           && (createInfo->failError == ERROR_NONE)
           && (error == ERROR_NONE)
          );
    if ((createInfo->requestedAbortFlag != NULL) && (*createInfo->requestedAbortFlag))
    {
      printInfo(1,"ABORTED\n");
      File_close(&fileHandle);
      Archive_closeEntry(&archiveEntryInfo);
      return error;
    }
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot store archive file (error: %s)!\n",
                 Errors_getText(error)
                );
      File_close(&fileHandle);
      Archive_closeEntry(&archiveEntryInfo);
      return error;
    }
    printInfo(2,"    \b\b\b\b");

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive file entry (error: %s)!\n",
                 Errors_getText(error)
                );
      File_close(&fileHandle);
      return error;
    }

    // free resources
    if (deltaCompressFlag)
    {
      Source_closeEntry(&sourceHandle);
    }

    // close file
    File_close(&fileHandle);

    // get compression ratio
    if (   (   Compress_isCompressed(archiveEntryInfo.hardLink.deltaCompressAlgorithm)
            || Compress_isCompressed(archiveEntryInfo.hardLink.byteCompressAlgorithm)
           )
        && (archiveEntryInfo.hardLink.chunkHardLinkData.fragmentSize > 0LL)
       )
    {
      ratio = 100.0-archiveEntryInfo.hardLink.chunkHardLinkData.info.size*100.0/archiveEntryInfo.hardLink.chunkHardLinkData.fragmentSize;
    }
    else
    {
      ratio = 0.0;
    }

    if (!jobOptions->dryRunFlag)
    {
      printInfo(1,"ok (%llu bytes, ratio %.1f%%)\n",fileInfo.size,ratio);
      logMessage(LOG_TYPE_ENTRY_OK,"added '%s'\n",String_cString(nameList->head->string));
    }
    else
    {
      printInfo(1,"ok (%llu bytes, dry-run)\n",fileInfo.size);
    }
    createInfo->statusInfo.doneEntries += StringList_count(nameList);
    updateStatusInfo(createInfo);
  }
  else
  {
    printInfo(1,"ok (%llu bytes, not stored)\n",fileInfo.size);
  }

  // add to incremental list
  if (storeIncrementalFileInfoFlag)
  {
    STRINGLIST_ITERATE(nameList,stringNode,name)
    {
      addIncrementalList(&createInfo->filesDictionary,name,&fileInfo);
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeSpecialEntry
* Purpose: store a special entry into archive
* Input  : archiveInfo                  - archive info
*          jobOptions                   - job options; see JobOptions
*          createInfo                   - create info structure
*          storeIncrementalFileInfoFlag - TRUE to store incremental
*                                         file data
*          fileName                     - file name to store
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeSpecialEntry(ArchiveInfo  *archiveInfo,
                               JobOptions   *jobOptions,
                               CreateInfo   *createInfo,
                               bool         storeIncrementalFileInfoFlag,
                               const String fileName
                              )
{
  Errors           error;
  FileInfo         fileInfo;
  ArchiveEntryInfo archiveEntryInfo;

  assert(archiveInfo != NULL);
  assert(jobOptions != NULL);
  assert(createInfo != NULL);
  assert(fileName != NULL);

  printInfo(1,"Add '%s'...",String_cString(fileName));

  // get file info
  error = File_getFileInfo(&fileInfo,fileName);
  if (error != ERROR_NONE)
  {
    if (jobOptions->skipUnreadableFlag)
    {
      printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
      logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"access denied '%s'\n",String_cString(fileName));
      createInfo->statusInfo.errorEntries++;
      return ERROR_NONE;
    }
    else
    {
      printInfo(1,"FAIL\n");
      printError("Cannot get info for '%s' (error: %s)\n",
                 String_cString(fileName),
                 Errors_getText(error)
                );
      return error;
    }
  }

  if (!jobOptions->noStorageFlag)
  {
    // new special
    error = Archive_newSpecialEntry(archiveInfo,
                                    &archiveEntryInfo,
                                    fileName,
                                    &fileInfo
                                   );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot create new archive special entry '%s' (error: %s)\n",
                 String_cString(fileName),
                 Errors_getText(error)
                );
      return error;
    }

    // close archive entry
    error = Archive_closeEntry(&archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot close archive special entry (error: %s)!\n",
                 Errors_getText(error)
                );
      return error;
    }

    if (!jobOptions->dryRunFlag)
    {
      printInfo(1,"ok\n");
      logMessage(LOG_TYPE_ENTRY_OK,"added '%s'\n",String_cString(fileName));
    }
    else
    {
      printInfo(1,"ok (dry-run)\n");
    }
    createInfo->statusInfo.doneEntries++;
    updateStatusInfo(createInfo);
  }
  else
  {
    printInfo(1,"ok (not stored)\n");
  }

  // add to incremental list
  if (storeIncrementalFileInfoFlag)
  {
    addIncrementalList(&createInfo->filesDictionary,fileName,&fileInfo);
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Command_create(const char                      *storageName,
                      const EntryList                 *includeEntryList,
                      const PatternList               *excludePatternList,
                      const PatternList               *compressExcludePatternList,
                      JobOptions                      *jobOptions,
                      ArchiveTypes                    archiveType,
                      ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                      void                            *archiveGetCryptPasswordUserData,
                      CreateStatusInfoFunction        createStatusInfoFunction,
                      void                            *createStatusInfoUserData,
                      StorageRequestVolumeFunction    storageRequestVolumeFunction,
                      void                            *storageRequestVolumeUserData,
                      bool                            *pauseCreateFlag,
                      bool                            *pauseStorageFlag,
                      bool                            *requestedAbortFlag
                     )
{
  String           printableStorageName;
  CreateInfo       createInfo;
  ArchiveInfo      archiveInfo;
  byte             *buffer;
  Errors           error;
  String           incrementalListFileName;
  bool             useIncrementalFileInfoFlag;
  bool             storeIncrementalFileInfoFlag;
  bool             incrementalFileInfoExistFlag;
  bool             abortFlag;
  EntryMsg         entryMsg;
  bool             ownFileFlag;
  const StringNode *stringNode;
  String           name;

  assert(storageName != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);

  // initialise variables
  createInfo.storageName                  = String_newCString(storageName);
  createInfo.includeEntryList             = includeEntryList;
  createInfo.excludePatternList           = excludePatternList;
  createInfo.jobOptions                   = jobOptions;
  createInfo.pauseCreateFlag              = pauseCreateFlag;
  createInfo.pauseStorageFlag             = pauseStorageFlag;
  createInfo.requestedAbortFlag           = requestedAbortFlag;
  createInfo.archiveFileName              = String_new();
  createInfo.startTime                    = time(NULL);
  createInfo.collectorSumThreadExitFlag   = FALSE;
  createInfo.collectorSumThreadExitedFlag = FALSE;
  createInfo.collectorThreadExitFlag      = FALSE;
  createInfo.storageInfo.count            = 0;
  createInfo.storageInfo.bytes            = 0LL;
  createInfo.storageThreadExitFlag        = FALSE;
  StringList_init(&createInfo.storageFileList);
  createInfo.failError                    = ERROR_NONE;
  createInfo.statusInfoFunction           = createStatusInfoFunction;
  createInfo.statusInfoUserData           = createStatusInfoUserData;
  createInfo.statusInfo.doneEntries       = 0L;
  createInfo.statusInfo.doneBytes         = 0LL;
  createInfo.statusInfo.totalEntries      = 0L;
  createInfo.statusInfo.totalBytes        = 0LL;
  createInfo.statusInfo.skippedEntries    = 0L;
  createInfo.statusInfo.skippedBytes      = 0LL;
  createInfo.statusInfo.errorEntries      = 0L;
  createInfo.statusInfo.errorBytes        = 0LL;
  createInfo.statusInfo.archiveBytes      = 0LL;
  createInfo.statusInfo.compressionRatio  = 0.0;
  createInfo.statusInfo.name              = String_new();
  createInfo.statusInfo.entryDoneBytes    = 0LL;
  createInfo.statusInfo.entryTotalBytes   = 0LL;
  createInfo.statusInfo.storageName       = String_new();
  createInfo.statusInfo.archiveDoneBytes  = 0LL;
  createInfo.statusInfo.archiveTotalBytes = 0LL;
  createInfo.statusInfo.volumeNumber      = 0;
  createInfo.statusInfo.volumeProgress    = 0.0;

  if (   (archiveType == ARCHIVE_TYPE_FULL)
      || (archiveType == ARCHIVE_TYPE_INCREMENTAL)
      || (archiveType == ARCHIVE_TYPE_DIFFERENTIAL)
     )
  {
    createInfo.archiveType = archiveType;
    createInfo.partialFlag =    (archiveType == ARCHIVE_TYPE_INCREMENTAL)
                             || (archiveType == ARCHIVE_TYPE_DIFFERENTIAL);
  }
  else
  {
    createInfo.archiveType = jobOptions->archiveType;
    createInfo.partialFlag =    (jobOptions->archiveType == ARCHIVE_TYPE_INCREMENTAL)
                             || (jobOptions->archiveType == ARCHIVE_TYPE_DIFFERENTIAL);
  }

  incrementalListFileName      = NULL;
  useIncrementalFileInfoFlag   = FALSE;
  storeIncrementalFileInfoFlag = FALSE;
  incrementalFileInfoExistFlag = FALSE;

  // allocate buffer
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init file name queue, storage queue and list lock
  if (!MsgQueue_init(&createInfo.entryMsgQueue,MAX_FILE_MSG_QUEUE_ENTRIES))
  {
    HALT_FATAL_ERROR("Cannot initialise file message queue!");
  }
  if (!MsgQueue_init(&createInfo.storageMsgQueue,0))
  {
    HALT_FATAL_ERROR("Cannot initialise storage message queue!");
  }
  if (!Semaphore_init(&createInfo.storageInfoLock))
  {
    HALT_FATAL_ERROR("Cannot initialise storage semaphore!");
  }

  // get printable storage name
  printableStorageName = Storage_getPrintableName(String_new(),createInfo.storageName);

  // init storage
  error = Storage_init(&createInfo.storageFileHandle,
                       createInfo.storageName,
                       createInfo.jobOptions,
                       storageRequestVolumeFunction,
                       storageRequestVolumeUserData,
                       (StorageStatusInfoFunction)updateStorageStatusInfo,
                       &createInfo,
                       createInfo.archiveFileName
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize storage '%s' (error: %s)\n",
               String_cString(printableStorageName),
               Errors_getText(error)
              );
    String_delete(printableStorageName);
    Semaphore_done(&createInfo.storageInfoLock);
    MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
    MsgQueue_done(&createInfo.entryMsgQueue,(MsgQueueMsgFreeFunction)freeEntryMsg,NULL);
    free(buffer);
    String_delete(createInfo.statusInfo.storageName);
    String_delete(createInfo.statusInfo.name);
    String_delete(createInfo.archiveFileName);
    StringList_done(&createInfo.storageFileList);
    String_delete(createInfo.storageName);

    return error;
  }

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
                                createInfo.archiveFileName
                               );
    }
    Dictionary_init(&createInfo.filesDictionary,NULL,NULL);

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
                                  &createInfo.filesDictionary
                                 );
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("Cannot read incremental list file '%s' (error: %s)\n",
                   String_cString(incrementalListFileName),
                   Errors_getText(error)
                  );
        String_delete(incrementalListFileName);
#if 0
// NYI: must index be deleted on error?
        if (indexDatabaseHandle != NULL)
        {
          Storage_indexDiscard(&createInfo.storageIndexHandle);
        }
#endif /* 0 */
        Storage_done(&createInfo.storageFileHandle);
        String_delete(printableStorageName);
        Semaphore_done(&createInfo.storageInfoLock);
        MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
        MsgQueue_done(&createInfo.entryMsgQueue,(MsgQueueMsgFreeFunction)freeEntryMsg,NULL);
        free(buffer);
        Dictionary_done(&createInfo.filesDictionary,NULL,NULL);
        String_delete(createInfo.statusInfo.storageName);
        String_delete(createInfo.statusInfo.name);
        String_delete(createInfo.archiveFileName);
        StringList_done(&createInfo.storageFileList);
        String_delete(createInfo.storageName);

        return error;
      }
      printInfo(1,
                "ok (%lu entries)\n",
                Dictionary_count(&createInfo.filesDictionary)
               );
    }

    useIncrementalFileInfoFlag   = TRUE;
    storeIncrementalFileInfoFlag =    (createInfo.archiveType == ARCHIVE_TYPE_FULL)
                                   || (createInfo.archiveType == ARCHIVE_TYPE_INCREMENTAL);
  }

  // create new archive
  error = Archive_create(&archiveInfo,
                         jobOptions,
                         storeArchiveFile,
                         &createInfo,
                         archiveGetCryptPasswordFunction,
                         archiveGetCryptPasswordUserData,
                         indexDatabaseHandle
                        );
  if (error != ERROR_NONE)
  {
    printError("Cannot create archive file '%s' (error: %s)\n",
               String_cString(printableStorageName),
               Errors_getText(error)
              );
    if (useIncrementalFileInfoFlag)
    {
      Dictionary_done(&createInfo.filesDictionary,NULL,NULL);
      if (!incrementalFileInfoExistFlag) File_delete(incrementalListFileName,FALSE);
      String_delete(incrementalListFileName);
    }
#if 0
// NYI: must index be deleted on error?
    if (indexDatabaseHandle != NULL)
    {
      Storage_closeIndex(&createInfo.storageIndexHandle);
    }
#endif /* 0 */
    Storage_done(&createInfo.storageFileHandle);
    String_delete(printableStorageName);
    Semaphore_done(&createInfo.storageInfoLock);
    MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
    MsgQueue_done(&createInfo.entryMsgQueue,(MsgQueueMsgFreeFunction)freeEntryMsg,NULL);
    free(buffer);
    String_delete(createInfo.statusInfo.storageName);
    String_delete(createInfo.statusInfo.name);
    String_delete(createInfo.archiveFileName);
    StringList_done(&createInfo.storageFileList);
    String_delete(createInfo.storageName);

    return error;
  }

  // start threads
  if (!Thread_init(&createInfo.collectorSumThread,"BAR collector sum",globalOptions.niceLevel,collectorSumThreadCode,&createInfo))
  {
    HALT_FATAL_ERROR("Cannot initialise collector sum thread!");
  }
  if (!Thread_init(&createInfo.collectorThread,"BAR collector",globalOptions.niceLevel,collectorThreadCode,&createInfo))
  {
    HALT_FATAL_ERROR("Cannot initialise collector thread!");
  }
  if (!Thread_init(&createInfo.storageThread,"BAR storage",globalOptions.niceLevel,storageThreadCode,&createInfo))
  {
    HALT_FATAL_ERROR("Cannot initialise storage thread!");
  }

  // store files
  abortFlag = FALSE;
  while (   (createInfo.failError == ERROR_NONE)
         && !abortFlag
         && ((createInfo.requestedAbortFlag == NULL) || !(*createInfo.requestedAbortFlag))
         && MsgQueue_get(&createInfo.entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg))

        )
  {
    // pause
    while ((createInfo.pauseCreateFlag != NULL) && (*createInfo.pauseCreateFlag))
    {
      Misc_udelay(500*1000);
    }

    // check if own file (in temporary directory or storage file)
    ownFileFlag =    String_startsWith(entryMsg.name,tmpDirectory)
                  || StringList_contain(&createInfo.storageFileList,entryMsg.name);
    if (!ownFileFlag)
    {
      STRINGLIST_ITERATE(&entryMsg.nameList,stringNode,name)
      {
        ownFileFlag =    String_startsWith(name,tmpDirectory)
                      || StringList_contain(&createInfo.storageFileList,name);
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
              error = storeFileEntry(&archiveInfo,
                                     compressExcludePatternList,
                                     jobOptions,
                                     &createInfo,
                                     storeIncrementalFileInfoFlag,
                                     entryMsg.name,
                                     buffer,
                                     BUFFER_SIZE
                                    );
              if (error != ERROR_NONE) createInfo.failError = error;
              abortFlag |= !updateStatusInfo(&createInfo);
              break;
            case ENTRY_TYPE_IMAGE:
              break;
          }
          break;
        case FILE_TYPE_DIRECTORY:
          switch (entryMsg.entryType)
          {
            case ENTRY_TYPE_FILE:
              error = storeDirectoryEntry(&archiveInfo,
                                          jobOptions,
                                          &createInfo,
                                          storeIncrementalFileInfoFlag,
                                          entryMsg.name
                                         );
              if (error != ERROR_NONE) createInfo.failError = error;
              abortFlag |= !updateStatusInfo(&createInfo);
              break;
            case ENTRY_TYPE_IMAGE:
              break;
          }
          break;
        case FILE_TYPE_LINK:
          switch (entryMsg.entryType)
          {
            case ENTRY_TYPE_FILE:
              error = storeLinkEntry(&archiveInfo,
                                     jobOptions,
                                     &createInfo,
                                     storeIncrementalFileInfoFlag,
                                     entryMsg.name
                                    );
              if (error != ERROR_NONE) createInfo.failError = error;
              abortFlag |= !updateStatusInfo(&createInfo);
              break;
            case ENTRY_TYPE_IMAGE:
              break;
          }
          break;
        case FILE_TYPE_HARDLINK:
          switch (entryMsg.entryType)
          {
            case ENTRY_TYPE_FILE:
              error = storeHardLinkEntry(&archiveInfo,
                                         compressExcludePatternList,
                                         jobOptions,
                                         &createInfo,
                                         storeIncrementalFileInfoFlag,
                                         &entryMsg.nameList,
                                         buffer,
                                         BUFFER_SIZE
                                        );
              if (error != ERROR_NONE) createInfo.failError = error;
              abortFlag |= !updateStatusInfo(&createInfo);
              break;
            case ENTRY_TYPE_IMAGE:
              break;
          }
          break;
        case FILE_TYPE_SPECIAL:
          switch (entryMsg.entryType)
          {
            case ENTRY_TYPE_FILE:
              error = storeSpecialEntry(&archiveInfo,
                                        jobOptions,
                                        &createInfo,
                                        storeIncrementalFileInfoFlag,
                                        entryMsg.name
                                       );
              if (error != ERROR_NONE) createInfo.failError = error;
              abortFlag |= !updateStatusInfo(&createInfo);
              break;
            case ENTRY_TYPE_IMAGE:
              error = storeImageEntry(&archiveInfo,
                                      compressExcludePatternList,
                                      jobOptions,
                                      &createInfo,
                                      entryMsg.name,
                                      buffer,
                                      BUFFER_SIZE
                                     );
              if (error != ERROR_NONE) createInfo.failError = error;
              abortFlag |= !updateStatusInfo(&createInfo);
              break;
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
      printInfo(1,"skipped (reason: own created file)\n");
      logMessage(LOG_TYPE_ENTRY_ACCESS_DENIED,"skipped '%s'\n",String_cString(entryMsg.name));
      createInfo.statusInfo.skippedEntries++;
      abortFlag |= !updateStatusInfo(&createInfo);
    }

    // free entry message
    freeEntryMsg(&entryMsg,NULL);

// NYI: is this really useful? (avoid that sum-collector-thread is slower than file-collector-thread)
    // slow down if too fast
    while (   !createInfo.collectorSumThreadExitedFlag
           && (createInfo.statusInfo.doneEntries >= createInfo.statusInfo.totalEntries)
          )
    {
      Misc_udelay(1000*1000);
    }
  }

  // close archive
  Archive_close(&archiveInfo);

  // signal end of data
  createInfo.collectorSumThreadExitFlag = TRUE;
  MsgQueue_setEndOfMsg(&createInfo.entryMsgQueue);
  MsgQueue_setEndOfMsg(&createInfo.storageMsgQueue);
  abortFlag |= !updateStatusInfo(&createInfo);

  // wait for threads
  Thread_join(&createInfo.storageThread);
  Thread_join(&createInfo.collectorThread);
  Thread_join(&createInfo.collectorSumThread);

  // done storage
  Storage_done(&createInfo.storageFileHandle);

  // write incremental list
  if (   storeIncrementalFileInfoFlag
      && (createInfo.failError == ERROR_NONE)
      && ((createInfo.requestedAbortFlag == NULL) || !(*createInfo.requestedAbortFlag))
      && !jobOptions->dryRunFlag
     )
  {
    printInfo(1,"Write incremental list '%s'...",String_cString(incrementalListFileName));
    error = writeIncrementalList(incrementalListFileName,
                                 &createInfo.filesDictionary
                                );
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL\n");
      printError("Cannot write incremental list file '%s' (error: %s)\n",
                 String_cString(incrementalListFileName),
                 Errors_getText(error)
                );
      if (!incrementalFileInfoExistFlag) File_delete(incrementalListFileName,FALSE);
      String_delete(incrementalListFileName);
      String_delete(printableStorageName);
      Semaphore_done(&createInfo.storageInfoLock);
      MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
      MsgQueue_done(&createInfo.entryMsgQueue,(MsgQueueMsgFreeFunction)freeEntryMsg,NULL);
      free(buffer);
      Dictionary_done(&createInfo.filesDictionary,NULL,NULL);
      String_delete(createInfo.statusInfo.storageName);
      String_delete(createInfo.statusInfo.name);
      String_delete(createInfo.archiveFileName);
      StringList_done(&createInfo.storageFileList);
      String_delete(createInfo.storageName);

      return error;
    }

    printInfo(1,"ok\n");
    logMessage(LOG_TYPE_ALWAYS,"create incremental file '%s'\n",String_cString(incrementalListFileName));
  }

  // output statics
  if (createInfo.failError == ERROR_NONE)
  {
    printInfo(0,"%lu file/image(s)/%llu bytes(s) included\n",createInfo.statusInfo.doneEntries,createInfo.statusInfo.doneBytes);
    printInfo(2,"%lu file/image(s) skipped\n",createInfo.statusInfo.skippedEntries);
    printInfo(2,"%lu file/image(s) with errors\n",createInfo.statusInfo.errorEntries);
  }

  // free resources
  if (useIncrementalFileInfoFlag)
  {
    Dictionary_done(&createInfo.filesDictionary,NULL,NULL);
    String_delete(incrementalListFileName);
  }
  Thread_done(&createInfo.collectorSumThread);
  Thread_done(&createInfo.collectorThread);
  Thread_done(&createInfo.storageThread);
  String_delete(printableStorageName);
  Semaphore_done(&createInfo.storageInfoLock);
  MsgQueue_done(&createInfo.storageMsgQueue,(MsgQueueMsgFreeFunction)freeStorageMsg,NULL);
  MsgQueue_done(&createInfo.entryMsgQueue,(MsgQueueMsgFreeFunction)freeEntryMsg,NULL);
  free(buffer);
  String_delete(createInfo.statusInfo.storageName);
  String_delete(createInfo.statusInfo.name);
  String_delete(createInfo.archiveFileName);
  StringList_done(&createInfo.storageFileList);
  String_delete(createInfo.storageName);

  if ((createInfo.requestedAbortFlag == NULL) || !(*createInfo.requestedAbortFlag))
  {
    return createInfo.failError;
  }
  else
  {
    return ERROR_ABORTED;
  }
}

#ifdef __cplusplus
  }
#endif

/* end of file */
