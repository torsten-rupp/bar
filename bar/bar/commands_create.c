/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/commands_create.c,v $
* $Revision: 1.11 $
* $Author: torsten $
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
#include "patternlists.h"
#include "files.h"
#include "archive.h"
#include "crypt.h"
#include "storage.h"
#include "misc.h"

#include "commands_create.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define MAX_FILE_MSG_QUEUE_ENTRIES    256
#define MAX_STORAGE_MSG_QUEUE_ENTRIES 256

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

typedef struct
{
  String                      storageName;
  PatternList                 *includePatternList;
  PatternList                 *excludePatternList;
  const JobOptions            *jobOptions;
  ArchiveTypes                archiveType;                        // archive type to create
  bool                        *pauseFlag;                         // TRUE for pause
  bool                        *requestedAbortFlag;                // TRUE to abort create

  Dictionary                  filesDictionary;                    // dictionary with files (used for incremental backup)
  StorageFileHandle           storageFileHandle;                  // storage handle
  String                      archiveFileName;                    // archive file name
  time_t                      startTime;                          // start time [ms] (unix time)

  MsgQueue                    fileMsgQueue;                       // queue with files to store

  Thread                      collectorSumThread;                 // files collector sum thread id
  bool                        collectorSumThreadExitFlag;
  bool                        collectorSumThreadExitedFlag;
  Thread                      collectorThread;                    // files collector thread id
  bool                        collectorThreadExitFlag;

  MsgQueue                    storageMsgQueue;                    // queue with waiting storage files
  Semaphore                   storageSemaphore;
  uint                        storageCount;                       // number of current storage files
  uint64                      storageBytes;                       // number of bytes in current storage files
  Thread                      storageThread;                      // storage thread id
  bool                        storageThreadExitFlag;
  StringList                  storageFileList;                    // list with stored storage files

  Errors                      failError;

  CreateStatusInfoFunction    statusInfoFunction;
  void                        *statusInfoUserData;
  CreateStatusInfo            statusInfo;                         // status info
} CreateInfo;

typedef struct
{
  String    fileName;
  FileTypes fileType;
} FileMsg;

typedef struct
{
  String fileName;
  uint64 fileSize;
  bool   completeFlag;
  String destinationFileName;
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
* Purpose: read incremental list file
* Input  : fileName        - file name
*          filesDictionary - files dictionary
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

  /* init variables */
  Dictionary_clear(filesDictionary,NULL,NULL);

  /* open file */
  error = File_open(&fileHandle,fileName,FILE_OPENMODE_READ);
  if (error != ERROR_NONE)
  {
    free(keyData);
    return error;
  }

  /* read and check header */
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

  /* read entries */
  while (!File_eof(&fileHandle))
  {
    /* read entry */
    incrementalListInfo.state = INCREMENTAL_FILE_STATE_UNKNOWN;
    error = File_read(&fileHandle,&incrementalListInfo.cast,sizeof(incrementalListInfo.cast),NULL);
    if (error != ERROR_NONE) break;
    error = File_read(&fileHandle,&keyLength,sizeof(keyLength),NULL);
    if (error != ERROR_NONE) break;
    error = File_read(&fileHandle,keyData,keyLength,NULL);
    if (error != ERROR_NONE) break;

    /* store in dictionary */
    Dictionary_add(filesDictionary,
                   keyData,
                   keyLength,
                   &incrementalListInfo,
                   sizeof(incrementalListInfo)
                  );
  }

  /* close file */
  File_close(&fileHandle);

  /* free resources */
  free(keyData);

  return error;
}

/***********************************************************************\
* Name   : writeIncrementalList
* Purpose: write incremental list file
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

  String                    directory;
  String                    tmpFileName;
  Errors                    error;
  FileHandle                fileHandle;
  char                      id[32];
  uint16                    version;
  DictionaryIterator        dictionaryIterator;
  const void                *keyData;
  ulong                     keyLength;
  const void                *data;
  ulong                     length;
  uint16                    n;
  const IncrementalListInfo *incrementalListInfo;
  String                    directoryName;

  assert(fileName != NULL);
  assert(filesDictionary != NULL);

  /* get temporary name */
  directory = File_getFilePathName(File_newFileName(),fileName);
  tmpFileName = File_newFileName();
  error = File_getTmpFileName(tmpFileName,NULL,directory);
  if (error != ERROR_NONE)
  {
    File_deleteFileName(tmpFileName);
    File_deleteFileName(directory);
    return error;
  }

  /* open file */
  error = File_open(&fileHandle,tmpFileName,FILE_OPENMODE_CREATE);
  if (error != ERROR_NONE)
  {
    File_deleteFileName(tmpFileName);
    File_deleteFileName(directory);
    return error;
  }

  /* write header */
  memset(id,0,sizeof(id));
  strncpy(id,INCREMENTAL_LIST_FILE_ID,sizeof(id)-1);
  error = File_write(&fileHandle,id,sizeof(id));
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    File_deleteFileName(tmpFileName);
    File_deleteFileName(directory);
    return error;
  }
  version = INCREMENTAL_LIST_FILE_VERSION;
  error = File_write(&fileHandle,&version,sizeof(version));
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    File_deleteFileName(tmpFileName);
    File_deleteFileName(directory);
    return error;
  }

  /* write entries */
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
    error = File_write(&fileHandle,keyData,keyLength);
    if (error != ERROR_NONE) break;
  }
  Dictionary_doneIterator(&dictionaryIterator);

  /* close file */
  File_close(&fileHandle);
  if (error != ERROR_NONE)
  {
    File_delete(tmpFileName,FALSE);
    File_deleteFileName(tmpFileName);
    File_deleteFileName(directory);
    return error;
  }

  /* create directory if not existing */
  directoryName = File_getFilePathName(String_new(),fileName);
  if (!String_empty(directoryName) && !File_exists(directoryName))
  {
    error = File_makeDirectory(directoryName,FILE_DEFAULT_USER_ID,FILE_DEFAULT_GROUP_ID,FILE_DEFAULT_PERMISSION);
    if (error != ERROR_NONE)
    {
      String_delete(directoryName);
      File_delete(tmpFileName,FALSE);
      File_deleteFileName(tmpFileName);
      File_deleteFileName(directory);
      return error;
    }
  }
  String_delete(directoryName);

  /* rename files */
  error = File_rename(tmpFileName,fileName);
  if (error != ERROR_NONE)
  {
    File_delete(tmpFileName,FALSE);
    File_deleteFileName(tmpFileName);
    File_deleteFileName(directory);
    return error;
  }

  /* free resources */
  File_deleteFileName(tmpFileName);
  File_deleteFileName(directory);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : checkFileChanged
* Purpose: check if file changed
* Input  : filesDictionary - files dictionary
*          fileName        - file name
*          fileInfo        - file info
* Output : -
* Return : TRUE iff file changed, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool checkFileChanged(Dictionary     *filesDictionary,
                            const String   fileName,
                            const FileInfo *fileInfo
                           )
{
  void                *data;
  ulong               length;
  IncrementalListInfo *incrementalListInfo;

  assert(filesDictionary != NULL);
  assert(fileName != NULL);
  assert(fileInfo != NULL);

  /* check if exists */
  if (!Dictionary_find(filesDictionary,
                       String_cString(fileName),
                       String_length(fileName),
                       &data,
                       &length
                      )
     )
  {
    return TRUE;
  }
  assert(length == sizeof(IncrementalListInfo));

  /* check if modified */
  incrementalListInfo = (IncrementalListInfo*)data;
  if (memcmp(incrementalListInfo->cast,&fileInfo->cast,sizeof(FileCast)) != 0)
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
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateStatusInfo(const CreateInfo *createInfo)
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
  assert(createInfo != NULL);
  assert(storageStatusInfo != NULL);

  createInfo->statusInfo.volumeNumber   = storageStatusInfo->volumeNumber;
  createInfo->statusInfo.volumeProgress = storageStatusInfo->volumeProgress;

  updateStatusInfo(createInfo);
}

/***********************************************************************\
* Name   : checkIsIncluded
* Purpose: check if filename is included
* Input  : includePatternNode - include pattern node
*          fileName           - file name
* Output : -
* Return : TRUE if excluded, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool checkIsIncluded(PatternNode *includePatternNode,
                           String      fileName
                          )
{
  assert(includePatternNode != NULL);
  assert(fileName != NULL);

  return Pattern_match(&includePatternNode->pattern,fileName,PATTERN_MATCH_MODE_BEGIN);
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

LOCAL bool checkIsExcluded(PatternList *excludePatternList,
                           String      fileName
                          )
{
  assert(excludePatternList != NULL);
  assert(fileName != NULL);

  return PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT);
}

/***********************************************************************\
* Name   : appendToFileList
* Purpose: append a filename to filename list
* Input  : fileMsgQueue - file message queue
*          fileName     - file name (will be copied!)
*          fileType     - file type
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendToFileList(MsgQueue  *fileMsgQueue,
                            String    fileName,
                            FileTypes fileType
                           )
{
  FileMsg fileMsg;

  assert(fileMsgQueue != NULL);
  assert(fileName != NULL);

  /* init */
  fileMsg.fileName = String_duplicate(fileName);
  fileMsg.fileType = fileType;

  /* put into message queue */
  MsgQueue_put(fileMsgQueue,&fileMsg,sizeof(fileMsg));
}

/***********************************************************************\
* Name   : freeFileMsg
* Purpose: free file msg
* Input  : fileMsg - file message
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeFileMsg(FileMsg *fileMsg, void *userData)
{
  assert(fileMsg != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(fileMsg->fileName);
}

/***********************************************************************\
* Name   : getNextFile
* Purpose: get next file from list of files to pack
* Input  : fileMsgQueue - file message queue
           fileName - file name variable
* Output : fileName - file name
*          fileType - file type
* Return : TRUE if file available, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool getNextFile(MsgQueue  *fileMsgQueue,
                       String    fileName,
                       FileTypes *fileType
                      )
{
  FileMsg fileMsg;

  assert(fileName != NULL);
  assert(fileType != NULL);

  if (MsgQueue_get(fileMsgQueue,&fileMsg,NULL,sizeof(fileMsg)))
  {
    String_set(fileName,fileMsg.fileName);
    (*fileType) = fileMsg.fileType;

    String_delete(fileMsg.fileName);

    return TRUE;
  }
  else
  {
    return FALSE;
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
  long      i,j;
  char      format[4];
  char      buffer[256];
  size_t    length;
  ulong     divisor;
  ulong     n;
  int       z;
  int       d;

  /* expand named macros */
  switch (archiveType)
  {
    case ARCHIVE_TYPE_NORMAL:      TEXT_MACRO_N_CSTRING(textMacros[0],"%type","normal");      break;
    case ARCHIVE_TYPE_FULL:        TEXT_MACRO_N_CSTRING(textMacros[0],"%type","full");        break;
    case ARCHIVE_TYPE_INCREMENTAL: TEXT_MACRO_N_CSTRING(textMacros[0],"%type","incremental"); break;
    case ARCHIVE_TYPE_UNKNOWN:     TEXT_MACRO_N_CSTRING(textMacros[0],"%type","unknown");     break;
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

  /* expand time macros, part number */
  localtime_r(&time,&tmStruct);
  partNumberFlag = FALSE;
  i = 0;
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
              /* %% */
              String_remove(fileName,i,1);
              i += 1;
              break;
            case '#':
              /* %# */
              String_remove(fileName,i,1);
              i += 1;
              break;
            default:
              /* format time part */
              switch (String_index(fileName,i+1))
              {
                case 'E':
                case 'O':
                  /* %Ex, %Ox */
                  format[0] = '%';
                  format[1] = String_index(fileName,i+1);
                  format[2] = String_index(fileName,i+2);
                  format[3] = '\0';

                  String_remove(fileName,i,3);
                  break;
                default:
                  /* %x */
                  format[0] = '%';
                  format[1] = String_index(fileName,i+1);
                  format[2] = '\0';

                  String_remove(fileName,i,2);
                  break;
              }
              length = strftime(buffer,sizeof(buffer)-1,format,&tmStruct);

              /* insert in string */
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
                      i += 1;
                    }
                    String_insertChar(fileName,i,buffer[z]);
                    i += 1;
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
          /* % at end of string */
          i += 1;
        }      
        break;
      case '#':
        /* #... */
        switch (formatMode)
        {
          case FORMAT_MODE_ARCHIVE_FILE_NAME:
            if (partNumber != ARCHIVE_PART_NUMBER_NONE)
            {
              /* find #...# and get max. divisor for part number */
              divisor = 1;
              j = i+1;
              while ((j < String_length(fileName) && String_index(fileName,j) == '#'))
              {
                j++;
                if (divisor < 1000000000) divisor*=10;
              }

              /* replace #...# by part number */     
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
              i += 1;
            }       
            break;
          case FORMAT_MODE_PATTERN:
            /* replace by "." */
            String_replaceChar(fileName,i,1,'.');
            i += 1;
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
            #endif /* NDEBUG */
        }
        break;
      default:
        i += 1;
        break;
    }
  }

  /* append part number if multipart mode and there is no part number in format string */
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

  long i;
  char ch;

  /* remove all macros and leading and tailing separator characters */
  String_clear(fileName);
  i = 0;
  while (i < String_length(templateFileName))
  {
    ch = String_index(templateFileName,i);
    switch (ch)
    {
      case '%':
        i++;
        if (i < String_length(templateFileName))
        {
          /* removed previous separator characters */
          String_trimRight(fileName,SEPARATOR_CHARS);

          ch = String_index(templateFileName,i);
          switch (ch)
          {
            case '%':
              /* %% */
              String_appendChar(fileName,'%');
              i++;
              break;
            case '#':
              /* %# */
              String_appendChar(fileName,'#');
              i++;
              break;
            default:
              /* discard %xyz */
              if (isalpha(ch))
              {
                while (   (i < String_length(templateFileName))
                       && isalpha(ch)
                      )
                {
                  i++;
                  ch = String_index(templateFileName,i);
                }
              }

              /* discard following separator characters */
              if (strchr(SEPARATOR_CHARS,ch) != NULL)
              {
                while (   (i < String_length(templateFileName))
                       && (strchr(SEPARATOR_CHARS,ch) != NULL)
                      )
                {
                  i++;
                  ch = String_index(templateFileName,i);
                }
              }
              break;
          }
        }
        break;
      case '#':
        i++;
        break;
      default:
        String_appendChar(fileName,ch);
        i++;
        break;
    }
  }

  /* replace or add file name extension */
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
* Name   : collectorSumThread
* Purpose: file collector sum thread: only collect files and update
*          total files/bytes values
* Input  : createInfo - create info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void collectorSumThread(CreateInfo *createInfo)
{
  StringList      nameList;
  String          name;
  PatternNode     *includePatternNode;
  StringTokenizer fileNameTokenizer;
  String          basePath;
  String          string;
  Errors          error;
  String          fileName;
  FileInfo        fileInfo;
  DirectoryHandle directoryHandle;
//ulong xx;

  assert(createInfo != NULL);
  assert(createInfo->includePatternList != NULL);
  assert(createInfo->excludePatternList != NULL);

  StringList_init(&nameList);
  name = String_new();

  includePatternNode = createInfo->includePatternList->head;
  while (   !createInfo->collectorSumThreadExitFlag
         && (createInfo->failError == ERROR_NONE)
         && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
         && (includePatternNode != NULL)
        )
  {
    /* pause */
    while ((createInfo->pauseFlag != NULL) && (*createInfo->pauseFlag))
    {
      Misc_udelay(500*1000);
    }

    /* find base path */
    basePath = String_new();
    File_initSplitFileName(&fileNameTokenizer,includePatternNode->string);
    if (File_getNextSplitFileName(&fileNameTokenizer,&string) && !Pattern_checkIsPattern(string))
    {
      if (String_length(string) > 0)
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

    /* find files */
    StringList_append(&nameList,basePath);
    while (   !createInfo->collectorSumThreadExitFlag
           && (createInfo->failError == ERROR_NONE)
           && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
           && !StringList_empty(&nameList)
          )
    {
      /* pause */
      while ((createInfo->pauseFlag != NULL) && (*createInfo->pauseFlag))
      {
        Misc_udelay(500*1000);
      }

      /* get next directory to process */
      name = StringList_getLast(&nameList,name);

      /* read file info */
      error = File_getFileInfo(name,&fileInfo);
      if (error != ERROR_NONE)
      {
        continue;
      }

      if (   (fileInfo.type == FILE_TYPE_DIRECTORY)
          || (   checkIsIncluded(includePatternNode,name)
              && !checkIsExcluded(createInfo->excludePatternList,name)
             )
         )
      {
        switch (fileInfo.type)
        {
          case FILE_TYPE_FILE:
            if ((createInfo->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
            {
              createInfo->statusInfo.totalFiles++;
              createInfo->statusInfo.totalBytes += fileInfo.size;
              updateStatusInfo(createInfo);
            }
            break;
          case FILE_TYPE_DIRECTORY:
            if (   checkIsIncluded(includePatternNode,name)
                && !checkIsExcluded(createInfo->excludePatternList,name)
               )
            {
              if ((createInfo->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
              {
                createInfo->statusInfo.totalFiles++;
                updateStatusInfo(createInfo);
              }
            }

            /* open directory contents */
            error = File_openDirectory(&directoryHandle,name);
            if (error == ERROR_NONE)
            {
              /* read directory contents */
              fileName = String_new();
              while (   !createInfo->collectorSumThreadExitFlag
                     && (createInfo->failError == ERROR_NONE)
                     && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
                     && !File_endOfDirectory(&directoryHandle)
                    )
              {
                /* pause */
                while ((createInfo->pauseFlag != NULL) && (*createInfo->pauseFlag))
                {
                  Misc_udelay(500*1000);
                }

                /* read next directory entry */
                error = File_readDirectory(&directoryHandle,fileName);
                if (error != ERROR_NONE)
                {
                  continue;
                }

                if (   checkIsIncluded(includePatternNode,fileName)
                    && !checkIsExcluded(createInfo->excludePatternList,fileName)
                   )
                {
                  /* read file info */
                  error = File_getFileInfo(fileName,&fileInfo);
                  if (error != ERROR_NONE)
                  {
                    continue;
                  }

                  switch (fileInfo.type)
                  {
                    case FILE_TYPE_FILE:
                      if ((createInfo->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo))
                      {
                        createInfo->statusInfo.totalFiles++;
                        createInfo->statusInfo.totalBytes += fileInfo.size;
                        updateStatusInfo(createInfo);
                      }
                      break;
                    case FILE_TYPE_DIRECTORY:
                      /* add to name list */
                      StringList_append(&nameList,fileName);
                      break;
                    case FILE_TYPE_LINK:
                      if ((createInfo->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo))
                      {
                        createInfo->statusInfo.totalFiles++;
                        updateStatusInfo(createInfo);
                      }
                      break;
                    case FILE_TYPE_SPECIAL:
                      if ((createInfo->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo))
                      {
                        createInfo->statusInfo.totalFiles++;
                        updateStatusInfo(createInfo);
                      }
                      break;
                    default:
                      break;
                  }
                }
              }
              String_delete(fileName);

              /* close directory */
              File_closeDirectory(&directoryHandle);
            }
            break;
          case FILE_TYPE_LINK:
            if ((createInfo->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
            {
              createInfo->statusInfo.totalFiles++;
              updateStatusInfo(createInfo);
            }
            break;
          case FILE_TYPE_SPECIAL:
            if ((createInfo->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
            {
              createInfo->statusInfo.totalFiles++;
              updateStatusInfo(createInfo);
            }
            break;
          default:
            break;
        }
      }
    }

    /* free resources */
    String_delete(basePath);

    /* next include pattern */
    includePatternNode = includePatternNode->next;
  }

  /* free resoures */
  String_delete(name);
  StringList_done(&nameList);

  /* terminate */
  createInfo->collectorSumThreadExitedFlag = TRUE;
}

/***********************************************************************\
* Name   : collectorThread
* Purpose: file collector thread
* Input  : createInfo - create info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void collectorThread(CreateInfo *createInfo)
{
  StringList      nameList;
  String          name;
  PatternNode     *includePatternNode;
  StringTokenizer fileNameTokenizer;
  String          basePath;
  String          string;
  Errors          error;
  String          fileName;
  FileInfo        fileInfo;
  DirectoryHandle directoryHandle;

  assert(createInfo != NULL);
  assert(createInfo->includePatternList != NULL);
  assert(createInfo->excludePatternList != NULL);

  StringList_init(&nameList);
  name     = String_new();
  fileName = String_new();

  includePatternNode = createInfo->includePatternList->head;
  while (   (createInfo->failError == ERROR_NONE)
         && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
         && (includePatternNode != NULL)
        )
  {
    /* pause */
    while ((createInfo->pauseFlag != NULL) && (*createInfo->pauseFlag))
    {
      Misc_udelay(500*1000);
    }

    /* find base path */
    basePath = String_new();
    File_initSplitFileName(&fileNameTokenizer,includePatternNode->string);
    if (File_getNextSplitFileName(&fileNameTokenizer,&string) && !Pattern_checkIsPattern(string))
    {
      if (String_length(string) > 0)
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

    /* find files */
    StringList_append(&nameList,basePath);
    while (   (createInfo->failError == ERROR_NONE)
           && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
           && !StringList_empty(&nameList)
          )
    {
      /* pause */
      while ((createInfo->pauseFlag != NULL) && (*createInfo->pauseFlag))
      {
        Misc_udelay(500*1000);
      }

      /* get next directory to process */
      name = StringList_getLast(&nameList,name);

      /* read file info */
      error = File_getFileInfo(name,&fileInfo);
      if (error != ERROR_NONE)
      {
        logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(name));
        printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(name),Errors_getText(error));
        createInfo->statusInfo.errorFiles++;
        updateStatusInfo(createInfo);
        continue;
      }

      if (   (fileInfo.type == FILE_TYPE_DIRECTORY)
          || (   checkIsIncluded(includePatternNode,name)
              && !checkIsExcluded(createInfo->excludePatternList,name)
             )
         )
      {
        switch (fileInfo.type)
        {
          case FILE_TYPE_FILE:
            if ((createInfo->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
            {
              /* add to file list */
              appendToFileList(&createInfo->fileMsgQueue,name,FILE_TYPE_FILE);
            }
            break;
          case FILE_TYPE_DIRECTORY:
            if (   checkIsIncluded(includePatternNode,name)
                && !checkIsExcluded(createInfo->excludePatternList,name)
               )
            {
              if ((createInfo->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
              {
                /* add to file list */
                appendToFileList(&createInfo->fileMsgQueue,name,FILE_TYPE_DIRECTORY);
              }
            }

            /* open directory contents */
            error = File_openDirectory(&directoryHandle,name);
            if (error == ERROR_NONE)
            {
              /* read directory contents */
              while (   (createInfo->failError == ERROR_NONE)
                     && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
                     && !File_endOfDirectory(&directoryHandle)
                    )
              {
                /* pause */
                while ((createInfo->pauseFlag != NULL) && (*createInfo->pauseFlag))
                {
                  Misc_udelay(500*1000);
                }

                /* read next directory entry */
                error = File_readDirectory(&directoryHandle,fileName);
                if (error != ERROR_NONE)
                {
                  logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(name));
                  printInfo(2,"Cannot read directory '%s' (error: %s) - skipped\n",String_cString(name),Errors_getText(error));
                  createInfo->statusInfo.errorFiles++;
                  createInfo->statusInfo.errorBytes += fileInfo.size;
                  updateStatusInfo(createInfo);
                  continue;
                }

                if (   checkIsIncluded(includePatternNode,fileName)
                    && !checkIsExcluded(createInfo->excludePatternList,fileName)
                   )
                {
                  /* read file info */
                  error = File_getFileInfo(fileName,&fileInfo);
                  if (error != ERROR_NONE)
                  {
                    logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(fileName));
                    printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(fileName),Errors_getText(error));
                    createInfo->statusInfo.errorFiles++;
                    updateStatusInfo(createInfo);
                    continue;
                  }

                  /* detect file type */
                  switch (fileInfo.type)
                  {
                    case FILE_TYPE_FILE:
                      if ((createInfo->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo))
                      {
                        /* add to file list */
                        appendToFileList(&createInfo->fileMsgQueue,fileName,FILE_TYPE_FILE);
                      }
                      break;
                    case FILE_TYPE_DIRECTORY:
                      /* add to name list */
                      StringList_append(&nameList,fileName);
                      break;
                    case FILE_TYPE_LINK:
                      if ((createInfo->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo))
                      {
                        /* add to file list */
                        appendToFileList(&createInfo->fileMsgQueue,fileName,FILE_TYPE_LINK);
                      }
                      break;
                    case FILE_TYPE_SPECIAL:
                      if ((createInfo->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo))
                      {
                        /* add to file list */
                        appendToFileList(&createInfo->fileMsgQueue,fileName,FILE_TYPE_SPECIAL);
                      }
                      break;
                    default:
                      logMessage(LOG_TYPE_FILE_TYPE_UNKNOWN,"unknown type '%s'",String_cString(fileName));
                      printInfo(2,"Unknown type of file '%s' - skipped\n",String_cString(fileName));
                      createInfo->statusInfo.errorFiles++;
                      createInfo->statusInfo.errorBytes += fileInfo.size;
                      updateStatusInfo(createInfo);
                      break;
                  }
                }
                else
                {
                  createInfo->statusInfo.skippedFiles++;
                  createInfo->statusInfo.skippedBytes += fileInfo.size;
                  updateStatusInfo(createInfo);
                }
              }

              /* close directory */
              File_closeDirectory(&directoryHandle);
            }
            else
            {
              logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(name));
              printInfo(2,"Cannot open directory '%s' (error: %s) - skipped\n",String_cString(name),Errors_getText(error));
              createInfo->statusInfo.errorFiles++;
              updateStatusInfo(createInfo);
            }
            break;
          case FILE_TYPE_LINK:
            if ((createInfo->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
            {
              /* add to file list */
              appendToFileList(&createInfo->fileMsgQueue,name,FILE_TYPE_LINK);
            }
            break;
          case FILE_TYPE_SPECIAL:
            if ((createInfo->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
            {
              /* add to file list */
              appendToFileList(&createInfo->fileMsgQueue,name,FILE_TYPE_SPECIAL);
            }
            break;
          default:
            logMessage(LOG_TYPE_FILE_TYPE_UNKNOWN,"unknown type '%s'",String_cString(name));
            printInfo(2,"Unknown type of file '%s' - skipped\n",String_cString(name));
            createInfo->statusInfo.errorFiles++;
            createInfo->statusInfo.errorBytes += fileInfo.size;
            updateStatusInfo(createInfo);
            break;
        }
      }
      else
      {
        logMessage(LOG_TYPE_FILE_EXCLUDED,"excluded '%s'",String_cString(name));
        createInfo->statusInfo.skippedFiles++;
        createInfo->statusInfo.skippedBytes += fileInfo.size;
        updateStatusInfo(createInfo);
      }
    }

    /* free resources */
    String_delete(basePath);

    /* next include pattern */
    includePatternNode = includePatternNode->next;
  }
  MsgQueue_setEndOfMsg(&createInfo->fileMsgQueue);

  /* free resoures */
  String_delete(fileName);
  String_delete(name);
  StringList_done(&nameList);

  createInfo->collectorThreadExitFlag = TRUE;
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : appendToStorageList
* Purpose: put file into storage queue
* Input  : storageMsgQueue     - storage message queue
*          fileName            - file name (will be copied!)
*          fileSize            - file size (in bytes)
*          destinationFileName - destination file name (will be copied!)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendToStorageList(MsgQueue  *storageMsgQueue,
                               String    fileName,
                               uint64    fileSize,
                               String    destinationFileName
                              )
{
  StorageMsg storageMsg;

  assert(storageMsgQueue != NULL);
  assert(fileName != NULL);
  assert(destinationFileName != NULL);

  /* init */
  storageMsg.fileName            = String_duplicate(fileName);
  storageMsg.fileSize            = fileSize;
  storageMsg.destinationFileName = String_duplicate(destinationFileName);
  storageMsg.completeFlag        = TRUE;

  /* put into message queue */
  MsgQueue_put(storageMsgQueue,&storageMsg,sizeof(storageMsg));
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

  String_delete(storageMsg->destinationFileName);
  String_delete(storageMsg->fileName);
}

/***********************************************************************\
* Name   : storeArchiveFile
* Purpose: storage archive call back
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors storeArchiveFile(void   *userData,
                              String fileName,
                              uint64 fileSize,
                              int    partNumber,
                              bool   lastPartFlag
                             )
{
  CreateInfo *createInfo = (CreateInfo*)userData;
  String     destinationName; 

  assert(createInfo != NULL);

  /* get destination file name */
  destinationName = formatArchiveFileName(String_new(),
                                          FORMAT_MODE_ARCHIVE_FILE_NAME,
                                          createInfo->archiveType,
                                          createInfo->archiveFileName,
                                          createInfo->startTime,
                                          partNumber,
                                          lastPartFlag
                                         );

  /* send to storage controller */
  Semaphore_lock(&createInfo->storageSemaphore,SEMAPHORE_LOCK_TYPE_READ_WRITE);
  createInfo->storageCount += 1;
  createInfo->storageBytes += fileSize;
  appendToStorageList(&createInfo->storageMsgQueue,
                      fileName,
                      fileSize,
                      destinationName
                     );
  Semaphore_unlock(&createInfo->storageSemaphore);
  createInfo->statusInfo.storageTotalBytes += fileSize;
  updateStatusInfo(createInfo);

  /* wait for space in temporary directory */
  if (globalOptions.maxTmpSize > 0)
  {
    Semaphore_lock(&createInfo->storageSemaphore,SEMAPHORE_LOCK_TYPE_READ_WRITE);
    while ((createInfo->storageCount > 2) && (createInfo->storageBytes > globalOptions.maxTmpSize))
    {
      Semaphore_waitModified(&createInfo->storageSemaphore);
    }
    Semaphore_unlock(&createInfo->storageSemaphore);
  }

  /* free resources */
  String_delete(destinationName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storageThread
* Purpose: archive storage thread
* Input  : createInfo - create info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void storageThread(CreateInfo *createInfo)
{
  #define MAX_RETRIES 3

  byte                   *buffer;
  String                 storageName;
  StorageMsg             storageMsg;
  Errors                 error;
  FileHandle             fileHandle;
  uint                   retryCount;
  ulong                  n;
  String                 pattern;
  String                 storagePath;
  String                 fileName;
  StorageDirectoryHandle storageDirectoryHandle;

  assert(createInfo != NULL);

  /* allocate resources */
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  storageName = String_new();

  /* initial pre-processing */
  if ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
  {
    if (createInfo->failError == ERROR_NONE)
    {
      /* pause */
      while ((createInfo->pauseFlag != NULL) && (*createInfo->pauseFlag))
      {
        Misc_udelay(500*1000);
      }

      /* initial pre-process */
      error = Storage_preProcess(&createInfo->storageFileHandle);
      if (error != ERROR_NONE)
      {
        printError("Cannot pre-process storage (error: %s)!\n",
                   Errors_getText(error)
                  );
        createInfo->failError = error;
      }
    }
  }

  /* store data */
  while (MsgQueue_get(&createInfo->storageMsgQueue,&storageMsg,NULL,sizeof(storageMsg)))
  {
    if ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
    {
      if (createInfo->failError == ERROR_NONE)
      {
        /* pause */
        while ((createInfo->pauseFlag != NULL) && (*createInfo->pauseFlag))
        {
          Misc_udelay(500*1000);
        }

        /* pre-process */
        error = Storage_preProcess(&createInfo->storageFileHandle);
        if (error != ERROR_NONE)
        {
          printError("Cannot pre-process file '%s' (error: %s)!\n",
                     String_cString(storageMsg.fileName),
                     Errors_getText(error)
                    );
          createInfo->failError = error;
          continue;
        }

        /* get storage name */
        Storage_getName(&createInfo->storageFileHandle,
                        storageName,
                        storageMsg.destinationFileName
                       );

        /* open file to store */
        printInfo(0,"Store '%s' to '%s'...",String_cString(storageMsg.fileName),String_cString(storageName));
        error = File_open(&fileHandle,storageMsg.fileName,FILE_OPENMODE_READ);
        if (error != ERROR_NONE)
        {
          printInfo(0,"FAIL!\n");
          printError("Cannot open file '%s' (error: %s)!\n",
                     String_cString(storageMsg.fileName),
                     Errors_getText(error)
                    );
          File_delete(storageMsg.fileName,FALSE);
          String_delete(storageMsg.fileName);
          String_delete(storageMsg.destinationFileName);
          createInfo->failError = error;
          continue;
        }

        retryCount = 0;
        do
        {
          /* pause */
          while ((createInfo->pauseFlag != NULL) && (*createInfo->pauseFlag))
          {
            Misc_udelay(500*1000);
          }

          /* next try */
          retryCount++;
          if (retryCount > MAX_RETRIES) break;

#if 1
          /* create storage file */
          error = Storage_create(&createInfo->storageFileHandle,
                                 storageMsg.destinationFileName,
                                 storageMsg.fileSize
                                );
          if (error != ERROR_NONE)
          {
            if (retryCount >= MAX_RETRIES)
            {
              printInfo(0,"FAIL!\n");
              printError("Cannot store file '%s' (error: %s)\n",
                         String_cString(storageName),
                         Errors_getText(error)
                        );
              createInfo->failError = error;
            }
            continue;
          }
          String_set(createInfo->statusInfo.storageName,storageMsg.destinationFileName);

          /* store data */
          File_seek(&fileHandle,0);
          do
          {
            /* pause */
            while ((createInfo->pauseFlag != NULL) && (*createInfo->pauseFlag))
            {
              Misc_udelay(500*1000);
            }

            error = File_read(&fileHandle,buffer,BUFFER_SIZE,&n);
            if (error != ERROR_NONE)
            {
              printInfo(0,"FAIL!\n");
              printError("Cannot read file '%s' (error: %s)!\n",
                         String_cString(storageName),
                         Errors_getText(error)
                        );
              createInfo->failError = error;
              break;
            }
            error = Storage_write(&createInfo->storageFileHandle,buffer,n);
            if (error != ERROR_NONE)
            {
              if (retryCount >= MAX_RETRIES)
              {
                printInfo(0,"FAIL!\n");
                printError("Cannot write file '%s' (error: %s)!\n",
                           String_cString(storageName),
                           Errors_getText(error)
                          );
                createInfo->failError = error;
              }
              break;
            }
            createInfo->statusInfo.storageDoneBytes += n;
            updateStatusInfo(createInfo);
          }
          while (   (createInfo->failError == ERROR_NONE)
                 && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
                 && !File_eof(&fileHandle)
                );

          /* close storage file */
          Storage_close(&createInfo->storageFileHandle);
#else
  error = ERROR_NONE;
#endif /* 0 */

          if (error == ERROR_NONE)
          {
            logMessage(LOG_TYPE_STORAGE,"stored '%s'",String_cString(storageName));
            printInfo(0,"ok\n");
          }
        }
        while (   (error != ERROR_NONE)
               && (createInfo->failError == ERROR_NONE)
               && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
              );

        /* close file to store */
        File_close(&fileHandle);

        /* check for error */
        if (   (error != ERROR_NONE)
            && ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
           )
        {
          File_delete(storageMsg.fileName,FALSE);
          String_delete(storageMsg.fileName);
          String_delete(storageMsg.destinationFileName);
          continue;
        }

        /* add to list of stored storage files */
        StringList_append(&createInfo->storageFileList,storageMsg.destinationFileName);

        /* post-process */
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
      }
    }

    /* delete source file */
    error = File_delete(storageMsg.fileName,FALSE);
    if (error != ERROR_NONE)
    {
      printWarning("Cannot delete file '%s' (error: %s)!\n",
                   String_cString(storageMsg.fileName),
                   Errors_getText(error)
                  );
    }

    /* update storage info */
    Semaphore_lock(&createInfo->storageSemaphore,SEMAPHORE_LOCK_TYPE_READ_WRITE);
    assert(createInfo->storageCount > 0);
    assert(createInfo->storageBytes >= storageMsg.fileSize);
    createInfo->storageCount -= 1;
    createInfo->storageBytes -= storageMsg.fileSize;
    Semaphore_unlock(&createInfo->storageSemaphore);

    /* free resources */
    freeStorageMsg(&storageMsg,NULL);
  }

  /* final post-processing */
  if ((createInfo->requestedAbortFlag == NULL) || !(*createInfo->requestedAbortFlag))
  {
    if (createInfo->failError == ERROR_NONE)
    {
      /* pause */
      while ((createInfo->pauseFlag != NULL) && (*createInfo->pauseFlag))
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

  /* delete old storage files */
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
//fprintf(stderr,"%s,%d: pa=%s\n",__FILE__,__LINE__,String_cString(pattern));

        storagePath = File_getFilePathName(String_new(),createInfo->storageName);
        error = Storage_openDirectory(&storageDirectoryHandle,
                                      storagePath,
                                      createInfo->jobOptions
                                     );
        if (error == ERROR_NONE)
        {
          fileName = String_new();
          while (   !Storage_endOfDirectory(&storageDirectoryHandle)
                 && (Storage_readDirectory(&storageDirectoryHandle,fileName,NULL) == ERROR_NONE)
                )
          {
            /* find in storage list */
            if (String_match(fileName,STRING_BEGIN,pattern,NULL,NULL))
            {
              if (StringList_find(&createInfo->storageFileList,fileName) == NULL)
              {
//fprintf(stderr,"%s,%d: deletete  %s    \n",__FILE__,__LINE__,String_cString(fileName));
                Storage_delete(&createInfo->storageFileHandle,fileName);
              }
            }
          }
          String_delete(fileName);

          Storage_closeDirectory(&storageDirectoryHandle);
        }
        String_delete(storagePath);
        String_delete(pattern);
      }
    }
  }

  /* free resoures */
  String_delete(storageName);
  free(buffer);

  createInfo->storageThreadExitFlag = TRUE;
}

/*---------------------------------------------------------------------*/

Errors Command_create(const char                      *storageName,
                      PatternList                     *includePatternList,
                      PatternList                     *excludePatternList,
                      JobOptions                      *jobOptions,
                      ArchiveTypes                    archiveType,
                      ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                      void                            *archiveGetCryptPasswordUserData,
                      CreateStatusInfoFunction        createStatusInfoFunction,
                      void                            *createStatusInfoUserData,
                      StorageRequestVolumeFunction    storageRequestVolumeFunction,
                      void                            *storageRequestVolumeUserData,
                      bool                            *pauseFlag,
                      bool                            *requestedAbortFlag
                     )
{
  CreateInfo      createInfo;
  ArchiveInfo     archiveInfo;
  byte            *buffer;
  Errors          error;
  String          incrementalListFileName;
  bool            storeIncrementalFileInfoFlag;
  String          fileName;
  FileTypes       fileType;
  ArchiveFileInfo archiveFileInfo;

  assert(storageName != NULL);
  assert(includePatternList != NULL);
  assert(excludePatternList != NULL);

  /* initialise variables */
  createInfo.storageName                  = String_newCString(storageName);
  createInfo.includePatternList           = includePatternList;
  createInfo.excludePatternList           = excludePatternList;
  createInfo.jobOptions                   = jobOptions;
  createInfo.archiveType                  = ((archiveType == ARCHIVE_TYPE_FULL) || (archiveType == ARCHIVE_TYPE_INCREMENTAL))?archiveType:jobOptions->archiveType;
  createInfo.pauseFlag                    = pauseFlag;
  createInfo.requestedAbortFlag           = requestedAbortFlag;
  createInfo.archiveFileName              = String_new();
  createInfo.startTime                    = time(NULL);
  createInfo.collectorSumThreadExitFlag   = FALSE;
  createInfo.collectorSumThreadExitedFlag = FALSE;
  createInfo.collectorThreadExitFlag      = FALSE;
  createInfo.storageCount                 = 0;
  createInfo.storageBytes                 = 0LL;
  createInfo.storageThreadExitFlag        = FALSE;
  StringList_init(&createInfo.storageFileList);
  createInfo.failError                    = ERROR_NONE;
  createInfo.statusInfoFunction           = createStatusInfoFunction;
  createInfo.statusInfoUserData           = createStatusInfoUserData;
  createInfo.statusInfo.doneFiles         = 0L;
  createInfo.statusInfo.doneBytes         = 0LL;
  createInfo.statusInfo.totalFiles        = 0L;
  createInfo.statusInfo.totalBytes        = 0LL;
  createInfo.statusInfo.skippedFiles      = 0L;
  createInfo.statusInfo.skippedBytes      = 0LL;
  createInfo.statusInfo.errorFiles        = 0L;
  createInfo.statusInfo.errorBytes        = 0LL;
  createInfo.statusInfo.archiveBytes      = 0LL;
  createInfo.statusInfo.compressionRatio  = 0.0;
  createInfo.statusInfo.fileName          = String_new();
  createInfo.statusInfo.fileDoneBytes     = 0LL;
  createInfo.statusInfo.fileTotalBytes    = 0LL;
  createInfo.statusInfo.storageName       = String_new();
  createInfo.statusInfo.storageDoneBytes  = 0LL;
  createInfo.statusInfo.storageTotalBytes = 0LL;
  createInfo.statusInfo.volumeNumber      = 0;
  createInfo.statusInfo.volumeProgress    = 0.0;

  incrementalListFileName      = NULL;
  storeIncrementalFileInfoFlag = FALSE;

  /* allocate resources */
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
//  fileName = String_new();

#if 1
  /* init file name queue, storage queue and list lock */
  if (!MsgQueue_init(&createInfo.fileMsgQueue,MAX_FILE_MSG_QUEUE_ENTRIES))
  {
    HALT_FATAL_ERROR("Cannot initialise file message queue!");
  }
  if (!MsgQueue_init(&createInfo.storageMsgQueue,0))
  {
    HALT_FATAL_ERROR("Cannot initialise storage message queue!");
  }
  if (!Semaphore_init(&createInfo.storageSemaphore))
  {
    HALT_FATAL_ERROR("Cannot initialise storage semaphore!");
  }
#endif /* 0 */

  /* init storage */
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
    Semaphore_done(&createInfo.storageSemaphore);
    MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
    MsgQueue_done(&createInfo.fileMsgQueue,NULL,NULL);
    free(buffer);
    String_delete(createInfo.statusInfo.storageName);
    String_delete(createInfo.statusInfo.fileName);
    String_delete(createInfo.archiveFileName);
    StringList_done(&createInfo.storageFileList);
    String_delete(createInfo.storageName);

    return error;
  }

#if 1
  if (   (createInfo.archiveType == ARCHIVE_TYPE_FULL)
      || (createInfo.archiveType == ARCHIVE_TYPE_INCREMENTAL)
      || !String_empty(jobOptions->incrementalListFileName)
     )
  {
    /* get increment list file name */
    incrementalListFileName = String_new();
    if (!String_empty(jobOptions->incrementalListFileName))
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
    storeIncrementalFileInfoFlag = TRUE;

    /* read incremental list */
    if ((createInfo.archiveType == ARCHIVE_TYPE_INCREMENTAL) && File_exists(incrementalListFileName))
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
        Semaphore_done(&createInfo.storageSemaphore);
        MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
        MsgQueue_done(&createInfo.fileMsgQueue,NULL,NULL);
        free(buffer);
        Dictionary_done(&createInfo.filesDictionary,NULL,NULL);
        String_delete(createInfo.statusInfo.storageName);
        String_delete(createInfo.statusInfo.fileName);
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
  }

  /* create new archive */
  error = Archive_create(&archiveInfo,
                         jobOptions,
                         storeArchiveFile,
                         &createInfo,
                         archiveGetCryptPasswordFunction,
                         archiveGetCryptPasswordUserData
                        );
  if (error != ERROR_NONE)
  {
    printError("Cannot create archive file '%s' (error: %s)\n",
               String_cString(createInfo.storageName),
               Errors_getText(error)
              );
    if (storeIncrementalFileInfoFlag)
    {
      Dictionary_done(&createInfo.filesDictionary,NULL,NULL);
      String_delete(incrementalListFileName);
    }
    Semaphore_done(&createInfo.storageSemaphore);
    MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
    MsgQueue_done(&createInfo.fileMsgQueue,NULL,NULL);
    free(buffer);
    String_delete(createInfo.statusInfo.storageName);
    String_delete(createInfo.statusInfo.fileName);
    String_delete(createInfo.archiveFileName);
    StringList_done(&createInfo.storageFileList);
    String_delete(createInfo.storageName);

    return error;
  }

  /* start threads */
  if (!Thread_init(&createInfo.collectorSumThread,"BAR collector sum",globalOptions.niceLevel,collectorSumThread,&createInfo))
  {
    HALT_FATAL_ERROR("Cannot initialise collector sum thread!");
  }
  if (!Thread_init(&createInfo.collectorThread,"BAR collector",globalOptions.niceLevel,collectorThread,&createInfo))
  {
    HALT_FATAL_ERROR("Cannot initialise collector thread!");
  }
  if (!Thread_init(&createInfo.storageThread,"BAR storage",globalOptions.niceLevel,storageThread,&createInfo))
  {
    HALT_FATAL_ERROR("Cannot initialise storage thread!");
  }

  /* store files */
  fileName = String_new();
  while (   ((createInfo.requestedAbortFlag == NULL) || !(*createInfo.requestedAbortFlag))
         && getNextFile(&createInfo.fileMsgQueue,fileName,&fileType)
        )
  {
    if (createInfo.failError == ERROR_NONE)
    {
      /* pause */
      while ((createInfo.pauseFlag != NULL) && (*createInfo.pauseFlag))
      {
        Misc_udelay(500*1000);
      }

      printInfo(1,"Add '%s'...",String_cString(fileName));

      if (   !String_subEquals(fileName,tmpDirectory,STRING_BEGIN,String_length(tmpDirectory))
          && (StringList_find(&createInfo.storageFileList,fileName) == NULL)
         )
      {
        switch (fileType)
        {
          case FILE_TYPE_FILE:
            {
              FileInfo   fileInfo;
              FileHandle fileHandle;
              ulong      n;
              double     ratio;

              /* get file info */
              error = File_getFileInfo(fileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                if (jobOptions->skipUnreadableFlag)
                {
                  printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
                  logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(fileName));
                }
                else
                {
                  printInfo(1,"FAIL\n");
                  printError("Cannot get info for file '%s' (error: %s)\n",
                             String_cString(fileName),
                             Errors_getText(error)
                            );
                  createInfo.failError = error;
                }
                continue;
              }

              if (!jobOptions->noStorageFlag)
              {
                /* open file */
                error = File_open(&fileHandle,fileName,FILE_OPENMODE_READ);
                if (error != ERROR_NONE)
                {
                  if (jobOptions->skipUnreadableFlag)
                  {
                    printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
                    logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"open failed '%s'",String_cString(fileName));
                  }
                  else
                  {
                    printInfo(1,"FAIL\n");
                    printError("Cannot open file '%s' (error: %s)\n",
                               String_cString(fileName),
                               Errors_getText(error)
                              );
                    createInfo.failError = error;
                  }
                  continue;
                }

                /* create new archive file entry */
                error = Archive_newFileEntry(&archiveInfo,
                                             &archiveFileInfo,
                                             fileName,
                                             &fileInfo
                                            );
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL\n");
                  printError("Cannot create new archive file entry '%s' (error: %s)\n",
                             String_cString(fileName),
                             Errors_getText(error)
                            );
                  createInfo.failError = error;
                  break;
                }
                String_set(createInfo.statusInfo.fileName,fileName);
                createInfo.statusInfo.fileDoneBytes = 0LL;
                createInfo.statusInfo.fileTotalBytes = fileInfo.size;
                updateStatusInfo(&createInfo);

                /* write file content to archive */
                error = ERROR_NONE;
                do
                {
                  /* pause */
                  while ((createInfo.pauseFlag != NULL) && (*createInfo.pauseFlag))
                  {
                    Misc_udelay(500*1000);
                  }

                  File_read(&fileHandle,buffer,BUFFER_SIZE,&n);
                  if (n > 0)
                  {
                    error = Archive_writeFileData(&archiveFileInfo,buffer,n);
                    createInfo.statusInfo.doneBytes += n;
                    createInfo.statusInfo.fileDoneBytes += n;
                    createInfo.statusInfo.archiveBytes = createInfo.statusInfo.storageTotalBytes+Archive_getSize(&archiveFileInfo);
                    createInfo.statusInfo.compressionRatio = 100.0-(createInfo.statusInfo.storageTotalBytes+Archive_getSize(&archiveFileInfo))*100.0/createInfo.statusInfo.doneBytes;
    //printf(stderr,"%s,%d: storage=%llu done=%llu\n",__FILE__,__LINE__,createInfo.statusInfo.storageTotalBytes+Archive_getSize(&archiveFileInfo),createInfo.statusInfo.doneBytes);
                    updateStatusInfo(&createInfo);
                  }
                }
                while (   ((createInfo.requestedAbortFlag == NULL) || !(*createInfo.requestedAbortFlag))
                       && (n > 0)
                       && (createInfo.failError == ERROR_NONE)
                       && (error == ERROR_NONE)
                      );
                if ((createInfo.requestedAbortFlag != NULL) && (*createInfo.requestedAbortFlag))
                {
                  printInfo(1,"ABORTED\n");
                  File_close(&fileHandle);
                  Archive_closeEntry(&archiveFileInfo);
                  break;
                }
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL\n");
                  printError("Cannot store archive file (error: %s)!\n",
                             Errors_getText(error)
                            );
                  File_close(&fileHandle);
                  Archive_closeEntry(&archiveFileInfo);
                  createInfo.failError = error;
                  break;
                }

                /* close file */
                File_close(&fileHandle);

                /* close archive entry */
                error = Archive_closeEntry(&archiveFileInfo);
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL\n");
                  printError("Cannot close archive file entry (error: %s)!\n",
                             Errors_getText(error)
                            );
                  createInfo.failError = error;
                  break;
                }

                if ((archiveFileInfo.file.compressAlgorithm != COMPRESS_ALGORITHM_NONE) && (archiveFileInfo.file.chunkFileData.fragmentSize > 0))
                {
                  ratio = 100.0-archiveFileInfo.file.chunkInfoFileData.size*100.0/archiveFileInfo.file.chunkFileData.fragmentSize;
                }
                else
                {
                  ratio = 0;
                }
                printInfo(1,"ok (%llu bytes, ratio %.1f%%)\n",fileInfo.size,ratio);

                logMessage(LOG_TYPE_FILE_OK,"added '%s'",String_cString(fileName));
                createInfo.statusInfo.doneFiles++;
                updateStatusInfo(&createInfo);
              }
              else
              {
                printInfo(1,"ok (not stored)\n");
              }

              /* add to incremental list */
              if (storeIncrementalFileInfoFlag)
              {
                addIncrementalList(&createInfo.filesDictionary,fileName,&fileInfo);
              }
            }
            break;
          case FILE_TYPE_DIRECTORY:
            {
              FileInfo fileInfo;

              /* get directory info */
              error = File_getFileInfo(fileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                if (jobOptions->skipUnreadableFlag)
                {
                  printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
                  logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(fileName));
                }
                else
                {
                  printInfo(1,"FAIL\n");
                  printError("Cannot get info for directory '%s' (error: %s)\n",
                             String_cString(fileName),
                             Errors_getText(error)
                            );
                  createInfo.failError = error;
                }
                continue;
              }

              if (!jobOptions->noStorageFlag)
              {
                /* new directory */
                error = Archive_newDirectoryEntry(&archiveInfo,
                                                  &archiveFileInfo,
                                                  fileName,
                                                  &fileInfo
                                                 );
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL\n");
                  printError("Cannot create new archive directory entry '%s' (error: %s)\n",
                             String_cString(fileName),
                             Errors_getText(error)
                            );
                  logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"open failed '%s'",String_cString(fileName));
                  createInfo.failError = error;
                  break;
                }

                /* close archive entry */
                error = Archive_closeEntry(&archiveFileInfo);
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL\n");
                  printError("Cannot close archive directory entry (error: %s)!\n",
                             Errors_getText(error)
                            );
                  createInfo.failError = error;
                  break;
                }

                printInfo(1,"ok\n");

                logMessage(LOG_TYPE_FILE_OK,"added '%s'",String_cString(fileName));
                createInfo.statusInfo.doneFiles++;
                updateStatusInfo(&createInfo);
              }
              else
              {
                printInfo(1,"ok (not stored)\n");
              }

              /* add to incremental list */
              if (storeIncrementalFileInfoFlag)
              {
                addIncrementalList(&createInfo.filesDictionary,fileName,&fileInfo);
              }
            }
            break;
          case FILE_TYPE_LINK:
            {
              FileInfo fileInfo;
              String   name;

              /* get file info */
              error = File_getFileInfo(fileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                if (jobOptions->skipUnreadableFlag)
                {
                  printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
                  logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(fileName));
                }
                else
                {
                  printInfo(1,"FAIL\n");
                  printError("Cannot get info for link '%s' (error: %s)\n",
                             String_cString(fileName),
                             Errors_getText(error)
                            );
                  createInfo.failError = error;
                }
                continue;
              }

              if (!jobOptions->noStorageFlag)
              {
                /* read link */
                name = String_new();
                error = File_readLink(fileName,name);
                if (error != ERROR_NONE)
                {
                  if (jobOptions->skipUnreadableFlag)
                  {
                    printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
                    logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"open failed '%s'",String_cString(fileName));
                  }
                  else
                  {
                    printInfo(1,"FAIL\n");
                    printError("Cannot read link '%s' (error: %s)\n",
                               String_cString(fileName),
                               Errors_getText(error)
                              );
                    String_delete(name);
                    createInfo.failError = error;
                  }
                  continue;
                }

                /* new link */
                error = Archive_newLinkEntry(&archiveInfo,
                                             &archiveFileInfo,
                                             fileName,
                                             name,
                                             &fileInfo
                                            );
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL\n");
                  printError("Cannot create new archive link entry '%s' (error: %s)\n",
                             String_cString(fileName),
                             Errors_getText(error)
                            );
                  String_delete(name);
                  createInfo.failError = error;
                  break;
                }

                /* close archive entry */
                error = Archive_closeEntry(&archiveFileInfo);
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL\n");
                  printError("Cannot close archive link entry (error: %s)!\n",
                             Errors_getText(error)
                            );
                  createInfo.failError = error;
                  break;
                }

                printInfo(1,"ok\n");

                logMessage(LOG_TYPE_FILE_OK,"added '%s'",String_cString(fileName));
                createInfo.statusInfo.doneFiles++;
                updateStatusInfo(&createInfo);

                /* free resources */
                String_delete(name);
              }
              else
              {
                printInfo(1,"ok (not stored)\n");
              }

              /* add to incremental list */
              if (storeIncrementalFileInfoFlag)
              {
                addIncrementalList(&createInfo.filesDictionary,fileName,&fileInfo);
              }
            }
            break;
          case FILE_TYPE_SPECIAL:
            {
              FileInfo fileInfo;

              /* get file info */
              error = File_getFileInfo(fileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                if (jobOptions->skipUnreadableFlag)
                {
                  printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
                  logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(fileName));
                }
                else
                {
                  printInfo(1,"FAIL\n");
                  printError("Cannot get info for link '%s' (error: %s)\n",
                             String_cString(fileName),
                             Errors_getText(error)
                            );
                  createInfo.failError = error;
                }
                continue;
              }

              if (!jobOptions->noStorageFlag)
              {
                /* new special */
                error = Archive_newSpecialEntry(&archiveInfo,
                                                &archiveFileInfo,
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
                  createInfo.failError = error;
                  break;
                }

                /* close archive entry */
                error = Archive_closeEntry(&archiveFileInfo);
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL\n");
                  printError("Cannot close archive special entry (error: %s)!\n",
                             Errors_getText(error)
                            );
                  createInfo.failError = error;
                  break;
                }

                printInfo(1,"ok\n");

                logMessage(LOG_TYPE_FILE_OK,"added '%s'",String_cString(fileName));
                createInfo.statusInfo.doneFiles++;
                updateStatusInfo(&createInfo);

                /* free resources */
              }
              else
              {
                printInfo(1,"ok (not stored)\n");
              }

              /* add to incremental list */
              if (storeIncrementalFileInfoFlag)
              {
                addIncrementalList(&createInfo.filesDictionary,fileName,&fileInfo);
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
        printInfo(1,"skipped (reason: own created file)\n");
        logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"skipped '%s'",String_cString(fileName));
      }

// NYI: is this really useful? (avoid that sum-collector-thread is slower than file-collector-thread)
      /* slow down if too fast */
      while (   !createInfo.collectorSumThreadExitedFlag
             && (createInfo.statusInfo.doneFiles >= createInfo.statusInfo.totalFiles)
            )
      {
        Misc_udelay(1000*1000);
      }
    }
  }
  String_delete(fileName);

  /* close archive */
  Archive_close(&archiveInfo);

  /* signal end of data */
  createInfo.collectorSumThreadExitFlag = TRUE;
  MsgQueue_setEndOfMsg(&createInfo.fileMsgQueue);
  MsgQueue_setEndOfMsg(&createInfo.storageMsgQueue);
  updateStatusInfo(&createInfo);

  /* wait for threads */
  Thread_join(&createInfo.storageThread);
  Thread_join(&createInfo.collectorThread);
  Thread_join(&createInfo.collectorSumThread);

  /* done storage */
  Storage_done(&createInfo.storageFileHandle);

  /* write incremental list */
  if (   storeIncrementalFileInfoFlag
      && (createInfo.failError == ERROR_NONE)
      && ((createInfo.requestedAbortFlag == NULL) || !(*createInfo.requestedAbortFlag))
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
      String_delete(incrementalListFileName);
      Semaphore_done(&createInfo.storageSemaphore);
      MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
      MsgQueue_done(&createInfo.fileMsgQueue,NULL,NULL);
      free(buffer);
      Dictionary_done(&createInfo.filesDictionary,NULL,NULL);
      String_delete(createInfo.statusInfo.storageName);
      String_delete(createInfo.statusInfo.fileName);
      String_delete(createInfo.archiveFileName);
      StringList_done(&createInfo.storageFileList);
      String_delete(createInfo.storageName);

      return error;
    }
    printInfo(1,"ok\n");

    logMessage(LOG_TYPE_ALWAYS,"create incremental file '%s'",String_cString(incrementalListFileName));
  }

  /* output statics */
  if (createInfo.failError == ERROR_NONE)
  {
    printInfo(0,"%lu file(s)/%llu bytes(s) included\n",createInfo.statusInfo.doneFiles,createInfo.statusInfo.doneBytes);
    printInfo(2,"%lu file(s) skipped\n",createInfo.statusInfo.skippedFiles);
    printInfo(2,"%lu file(s) with errors\n",createInfo.statusInfo.errorFiles);
  }

  /* free resources */
  if (storeIncrementalFileInfoFlag)
  {
    Dictionary_done(&createInfo.filesDictionary,NULL,NULL);
    String_delete(incrementalListFileName);
  }
  Thread_done(&createInfo.collectorSumThread);
  Thread_done(&createInfo.collectorThread);
  Thread_done(&createInfo.storageThread);
  Semaphore_done(&createInfo.storageSemaphore);
  MsgQueue_done(&createInfo.storageMsgQueue,(MsgQueueMsgFreeFunction)freeStorageMsg,NULL);
  MsgQueue_done(&createInfo.fileMsgQueue,(MsgQueueMsgFreeFunction)freeFileMsg,NULL);
#endif /* 0 */
  free(buffer);
  String_delete(createInfo.statusInfo.storageName);
  String_delete(createInfo.statusInfo.fileName);
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
