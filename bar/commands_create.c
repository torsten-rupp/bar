/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_create.c,v $
* $Revision: 1.44 $
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
  PatternList                 *includePatternList;
  PatternList                 *excludePatternList;
  const Options               *options;
  bool                        *abortRequestFlag;                  // TRUE if abort requested

  Dictionary                  filesDictionary;                    // dictionary with files (used for incremental backup)
  StorageFileHandle           storageFileHandle;                  // storage handle
  String                      archiveFileName;                    // archive file name
  time_t                      startTime;                          // start time [ms] (unix time)

  MsgQueue                    fileMsgQueue;                       // queue with files to backup

  Thread                      collectorSumThread;                 // files collector sum thread id
  bool                        collectorSumThreadExitFlag;
  bool                        collectorSumThreadExitedFlag;
  Thread                      collectorThread;                    // files collector thread id
  bool                        collectorThreadExitFlag;

  MsgQueue                    storageMsgQueue;                    // queue with storage files
  Semaphore                   storageSemaphore;
  uint                        storageCount;                       // number of current storage files
  uint64                      storageBytes;                       // number of bytes in current storage files
  Thread                      storageThread;                      // storage thread id
  bool                        storageThreadExitFlag;

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
  tmpFileName = String_new();
  File_getTmpFileName(tmpFileName,NULL);

  /* open file */
  error = File_open(&fileHandle,tmpFileName,FILE_OPENMODE_CREATE);
  if (error != ERROR_NONE)
  {
    String_delete(tmpFileName);
    return error;
  }

  /* write header */
  memset(id,0,sizeof(id));
  strncpy(id,INCREMENTAL_LIST_FILE_ID,sizeof(id)-1);
  error = File_write(&fileHandle,id,sizeof(id));
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    String_delete(tmpFileName);
    return error;
  }
  version = INCREMENTAL_LIST_FILE_VERSION;
  error = File_write(&fileHandle,&version,sizeof(version));
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    String_delete(tmpFileName);
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
    String_delete(tmpFileName);
    return error;
  }

  /* create directory if not existing */
  directoryName = File_getFilePathName(String_new(),fileName);
  if (!File_exists(directoryName))
  {
    error = File_makeDirectory(directoryName);
    if (error != ERROR_NONE)
    {
      String_delete(directoryName);
      File_delete(tmpFileName,FALSE);
      String_delete(tmpFileName);
      return error;
    }
  }
  String_delete(directoryName);

  /* rename files */
  error = File_rename(tmpFileName,fileName);
  if (error != ERROR_NONE)
  {
    File_delete(tmpFileName,FALSE);
    String_delete(tmpFileName);
    return error;
  }

  /* free resources */
  String_delete(tmpFileName);

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

  return Pattern_match(includePatternNode,fileName,PATTERN_MATCH_MODE_BEGIN);
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

  return Pattern_matchList(excludePatternList,fileName,PATTERN_MATCH_MODE_BEGIN);
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
* Input  : archiveType      - archive type
*          fileName         - file name variable
*          templateFileName - template file name
*          partNumber       - part number
*          time             - time
* Output : -
* Return : formated file name
* Notes  : -
\***********************************************************************/

LOCAL String formatArchiveFileName(String        fileName,
                                   const String  templateFileName,
                                   int           partNumber,
                                   bool          lastPartFlag,
                                   time_t        time,
                                   const Options *options
                                  )
{
  TextMacro  textMacros[2];

  struct tm  tmStruct;
  long       i;
  char       format[4];
  char       buffer[256];
  size_t     length;
  long       i0,i1;
  ulong      divisor;           
  ulong      n;
  int        d;

  /* expand macros */
  switch (options->archiveType)
  {
    case ARCHIVE_TYPE_NORMAL:      TEXT_MACRO_CSTRING(textMacros[0],"%type","normal");      break;
    case ARCHIVE_TYPE_FULL:        TEXT_MACRO_CSTRING(textMacros[0],"%type","full");        break;
    case ARCHIVE_TYPE_INCREMENTAL: TEXT_MACRO_CSTRING(textMacros[0],"%type","incremental"); break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
      #endif /* NDEBUG */
  }
  TEXT_MACRO_CSTRING(textMacros[1],"%last",lastPartFlag?"-last":"");
  Misc_expandMacros(fileName,templateFileName,textMacros,SIZE_OF_ARRAY(textMacros));

  /* expand time macros */
  localtime_r(&time,&tmStruct);
  i = 0;
  while ((i = String_findChar(fileName,i,'%')) >= 0)
  {
    if ((i+1) < String_length(fileName))
    {
      switch (String_index(fileName,i+1))
      {
        case 'E':
        case 'O':
          format[0] = '%';
          format[1] = String_index(fileName,i+1);
          format[2] = String_index(fileName,i+2);
          format[3] = '\0';

          length = strftime(buffer,sizeof(buffer)-1,format,&tmStruct);

          String_remove(fileName,i,3);
          String_insertBuffer(fileName,i,buffer,length);
          i += length;
          break;
        case '%':
          String_remove(fileName,i,1);
          i += 1;
          break;
        default:
          format[0] = '%';
          format[1] = String_index(fileName,i+1);
          format[2] = '\0';

          length = strftime(buffer,sizeof(buffer)-1,format,&tmStruct);

          String_remove(fileName,i,2);
          String_insertBuffer(fileName,i,buffer,length);
          i += length;
          break;
      }
    }
    else
    {
     i += 1;
    }      
  }

  /* expand part number */
  if (partNumber >= 0)
  {
    i0 = String_findChar(fileName,STRING_BEGIN,'#');
    if (i0 >= 0)
    {
      /* find #...# and get max. divisor for part number */
      divisor = 1;
      i1 = i0+1;
      while ((i1 < String_length(fileName) && String_index(fileName,i1) == '#'))
      {
        i1++;
        if (divisor < 1000000000) divisor*=10;
      }

      /* replace #...# by part number */     
      n = partNumber;
      i = 0;
      while (divisor > 0)
      {
        d = n/divisor; n = n%divisor; divisor = divisor/10;
        assert(i < sizeof(buffer));
        buffer[i] = '0'+d; i++;
      }
      assert(i < sizeof(buffer));
      buffer[i] = '\0';
      String_replaceCString(fileName,i0,i1-i0,buffer);
    }
    else
    {
      /* append to end of file name */
      String_format(fileName,".%06d",partNumber);
    }
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
  String          s;
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
         && ((createInfo->abortRequestFlag == NULL) || !(*createInfo->abortRequestFlag))
         && (includePatternNode != NULL)
        )
  {
    /* find base path */
    basePath = String_new();
    File_initSplitFileName(&fileNameTokenizer,includePatternNode->pattern);
    if (File_getNextSplitFileName(&fileNameTokenizer,&s) && !Pattern_checkIsPattern(name))
    {
      if (String_length(s) > 0)
      {
        File_setFileName(basePath,s);
      }
      else
      {
        File_setFileNameChar(basePath,FILES_PATHNAME_SEPARATOR_CHAR);
      }
    }
    while (File_getNextSplitFileName(&fileNameTokenizer,&s) && !Pattern_checkIsPattern(name))
    {
      File_appendFileName(basePath,s);
    }
    File_doneSplitFileName(&fileNameTokenizer);

    /* find files */
    StringList_append(&nameList,basePath);
    while (   !createInfo->collectorSumThreadExitFlag
           && (createInfo->failError == ERROR_NONE)
           && ((createInfo->abortRequestFlag == NULL) || !(*createInfo->abortRequestFlag))
           && !StringList_empty(&nameList)
          )
    {
      /* get next directory to process */
      name = StringList_getLast(&nameList,name);
      if (   checkIsIncluded(includePatternNode,name)
          && !checkIsExcluded(createInfo->excludePatternList,name)
         )
      {
        /* read file info */
        error = File_getFileInfo(name,&fileInfo);
        if (error != ERROR_NONE)
        {
          continue;
        }

        switch (fileInfo.type)
        {
          case FILE_TYPE_FILE:
            if ((createInfo->options->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
            {
              createInfo->statusInfo.totalFiles++;
              createInfo->statusInfo.totalBytes += fileInfo.size;
              updateStatusInfo(createInfo);
            }
            break;
          case FILE_TYPE_DIRECTORY:
            if ((createInfo->options->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
            {
              createInfo->statusInfo.totalFiles++;
              updateStatusInfo(createInfo);
            }

            /* open directory contents */
            error = File_openDirectory(&directoryHandle,name);
            if (error == ERROR_NONE)
            {
              /* read directory contents */
              fileName = String_new();
              while (   !createInfo->collectorSumThreadExitFlag
                     && (createInfo->failError == ERROR_NONE)
                     && ((createInfo->abortRequestFlag == NULL) || !(*createInfo->abortRequestFlag))
                     && !File_endOfDirectory(&directoryHandle)
                    )
              {
/*
xx++;
if ((xx%1000)==0)
{
// String_debug();
fprintf(stderr,"%s,%d: %lu xx=%lu\n",__FILE__,__LINE__,StringList_count(&nameList),xx);
}
*/
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
                      if ((createInfo->options->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo))
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
                      if ((createInfo->options->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo))
                      {
                        createInfo->statusInfo.totalFiles++;
                        updateStatusInfo(createInfo);
                      }
                      break;
                    case FILE_TYPE_SPECIAL:
                      if ((createInfo->options->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo))
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
            if ((createInfo->options->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
            {
              createInfo->statusInfo.totalFiles++;
              updateStatusInfo(createInfo);
            }
            break;
          case FILE_TYPE_SPECIAL:
            if ((createInfo->options->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
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
  String          s;
  Errors          error;
  String          fileName;
  FileInfo        fileInfo;
  DirectoryHandle directoryHandle;

  assert(createInfo != NULL);
  assert(createInfo->includePatternList != NULL);
  assert(createInfo->excludePatternList != NULL);

  StringList_init(&nameList);
  name = String_new();

  includePatternNode = createInfo->includePatternList->head;
  while (   (createInfo->failError == ERROR_NONE)
         && ((createInfo->abortRequestFlag == NULL) || !(*createInfo->abortRequestFlag))
         && (includePatternNode != NULL)
        )
  {
#if 1
    /* find base path */
    basePath = String_new();
    File_initSplitFileName(&fileNameTokenizer,includePatternNode->pattern);
    if (File_getNextSplitFileName(&fileNameTokenizer,&s) && !Pattern_checkIsPattern(name))
    {
      if (String_length(s) > 0)
      {
        File_setFileName(basePath,s);
      }
      else
      {
        File_setFileNameChar(basePath,FILES_PATHNAME_SEPARATOR_CHAR);
      }
    }
    while (File_getNextSplitFileName(&fileNameTokenizer,&s) && !Pattern_checkIsPattern(name))
    {
      File_appendFileName(basePath,s);
    }
    File_doneSplitFileName(&fileNameTokenizer);

    /* find files */
    StringList_append(&nameList,basePath);
    while (   (createInfo->failError == ERROR_NONE)
           && ((createInfo->abortRequestFlag == NULL) || !(*createInfo->abortRequestFlag))
           && !StringList_empty(&nameList)
          )
    {
      /* get next directory to process */
      name = StringList_getLast(&nameList,name);
      if (   checkIsIncluded(includePatternNode,name)
          && !checkIsExcluded(createInfo->excludePatternList,name)
         )
      {
        /* read file info */
        error = File_getFileInfo(name,&fileInfo);
        if (error != ERROR_NONE)
        {
          logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(name));
          printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(name),getErrorText(error));
          createInfo->statusInfo.errorFiles++;
          updateStatusInfo(createInfo);
          continue;
        }

        switch (fileInfo.type)
        {
          case FILE_TYPE_FILE:
            if ((createInfo->options->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
            {
              /* add to file list */
              appendToFileList(&createInfo->fileMsgQueue,name,FILE_TYPE_FILE);
            }
            break;
          case FILE_TYPE_DIRECTORY:
            if ((createInfo->options->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
            {
              /* add to file list */
              appendToFileList(&createInfo->fileMsgQueue,name,FILE_TYPE_DIRECTORY);
            }

            /* open directory contents */
            error = File_openDirectory(&directoryHandle,name);
            if (error == ERROR_NONE)
            {
              /* read directory contents */
              fileName = String_new();
              while (   (createInfo->failError == ERROR_NONE)
                     && ((createInfo->abortRequestFlag == NULL) || !(*createInfo->abortRequestFlag))
                     && !File_endOfDirectory(&directoryHandle)
                    )
              {
                /* read next directory entry */
                error = File_readDirectory(&directoryHandle,fileName);
                if (error != ERROR_NONE)
                {
                  logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(name));
                  printInfo(2,"Cannot read directory '%s' (error: %s) - skipped\n",String_cString(name),getErrorText(error));
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
                    printInfo(2,"Cannot access '%s' (error: %s) - skipped\n",String_cString(fileName),getErrorText(error));
                    createInfo->statusInfo.errorFiles++;
                    updateStatusInfo(createInfo);
                    continue;
                  }

                  /* detect file type */
                  switch (fileInfo.type)
                  {
                    case FILE_TYPE_FILE:
                      if ((createInfo->options->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo))
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
                      if ((createInfo->options->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo))
                      {
                        /* add to file list */
                        appendToFileList(&createInfo->fileMsgQueue,fileName,FILE_TYPE_LINK);
                      }
                      break;
                    case FILE_TYPE_SPECIAL:
                      if ((createInfo->options->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,fileName,&fileInfo))
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
              String_delete(fileName);

              /* close directory */
              File_closeDirectory(&directoryHandle);
            }
            else
            {
              logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(name));
              printInfo(2,"Cannot open directory '%s' (error: %s) - skipped\n",String_cString(name),getErrorText(error));
              createInfo->statusInfo.errorFiles++;
              updateStatusInfo(createInfo);
            }
            break;
          case FILE_TYPE_LINK:
            if ((createInfo->options->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
            {
              /* add to file list */
              appendToFileList(&createInfo->fileMsgQueue,name,FILE_TYPE_LINK);
            }
            break;
          case FILE_TYPE_SPECIAL:
            if ((createInfo->options->archiveType != ARCHIVE_TYPE_INCREMENTAL) || checkFileChanged(&createInfo->filesDictionary,name,&fileInfo))
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
#else
sleep(1);
#endif /* 0 */
  }
  MsgQueue_setEndOfMsg(&createInfo->fileMsgQueue);

  /* free resoures */
  String_delete(name);
  StringList_done(&nameList);

  createInfo->collectorThreadExitFlag = TRUE;
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : appendToStorageList
* Purpose: append a filename to a filename list
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

  String_delete(storageMsg->fileName);
  String_delete(storageMsg->destinationFileName);
}

/***********************************************************************\
* Name   : storeArchiveFile
* Purpose: storage archive call back
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors storeArchiveFile(String fileName,
                              uint64 fileSize,
                              int    partNumber,
                              bool   lastPartFlag,
                              void   *userData
                             )
{
  CreateInfo *createInfo = (CreateInfo*)userData;
  String     destinationName; 

  assert(createInfo != NULL);

  /* get destination file name */
  destinationName = formatArchiveFileName(String_new(),
                                          createInfo->archiveFileName,
                                          partNumber,
                                          lastPartFlag,
                                          createInfo->startTime,
                                          createInfo->options
                                         );

  /* send to storage controller */
  Semaphore_lock(&createInfo->storageSemaphore);
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
  if (createInfo->options->maxTmpSize > 0)
  {
    Semaphore_lock(&createInfo->storageSemaphore);
    while ((createInfo->storageCount > 2) && (createInfo->storageBytes > createInfo->options->maxTmpSize))
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
* Name   : storage
* Purpose: archive storage thread
* Input  : createInfo - create info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void storageThread(CreateInfo *createInfo)
{
  byte       *buffer;
  StorageMsg storageMsg;
  Errors     error;
  FileHandle fileHandle;
  ulong      n;

  assert(createInfo != NULL);

  /* allocate resources */
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* initial pre-processing */
  if (createInfo->failError == ERROR_NONE)
  {
    /* initial pre-process */
    error = Storage_preProcess(&createInfo->storageFileHandle);
    if (error != ERROR_NONE)
    {
      printError("Cannot pre-process storage (error: %s)!\n",
                 getErrorText(error)
                );
      createInfo->failError = error;
    }
  }

  /* store data */
  while (   (createInfo->failError == ERROR_NONE)
         && ((createInfo->abortRequestFlag == NULL) || !(*createInfo->abortRequestFlag))
         && MsgQueue_get(&createInfo->storageMsgQueue,&storageMsg,NULL,sizeof(storageMsg))
        )
  {
    if (createInfo->failError == ERROR_NONE)
    {
      /* pre-process */
      error = Storage_preProcess(&createInfo->storageFileHandle);
      if (error != ERROR_NONE)
      {
        printError("Cannot pre-process file '%s' (error: %s)!\n",
                   String_cString(storageMsg.fileName),
                   getErrorText(error)
                  );
        createInfo->failError = error;
        continue;
      }

      printInfo(0,"Store '%s' to '%s'...",String_cString(storageMsg.fileName),String_cString(storageMsg.destinationFileName));

      /* create storage file */
      error = Storage_create(&createInfo->storageFileHandle,
                             storageMsg.destinationFileName,
                             storageMsg.fileSize,
                             createInfo->options
                            );
      if (error != ERROR_NONE)
      {
        printInfo(0,"FAIL!\n");
        printError("Cannot store file '%s' (error: %s)\n",
                   String_cString(storageMsg.destinationFileName),
                   getErrorText(error)
                  );
        File_delete(storageMsg.fileName,FALSE);
        String_delete(storageMsg.fileName);
        String_delete(storageMsg.destinationFileName);
        createInfo->failError = error;
        continue;
      }
      String_set(createInfo->statusInfo.storageName,storageMsg.destinationFileName);

      /* store data */
      error = File_open(&fileHandle,storageMsg.fileName,FILE_OPENMODE_READ);
      if (error != ERROR_NONE)
      {
        printInfo(0,"FAIL!\n");
        printError("Cannot open file '%s' (error: %s)!\n",
                   String_cString(storageMsg.fileName),
                   getErrorText(error)
                  );
        File_delete(storageMsg.fileName,FALSE);
        String_delete(storageMsg.fileName);
        String_delete(storageMsg.destinationFileName);
        createInfo->failError = error;
        continue;
      }
      do
      {
        error = File_read(&fileHandle,buffer,BUFFER_SIZE,&n);
        if (error != ERROR_NONE)
        {
          printInfo(0,"FAIL!\n");
          printError("Cannot read file '%s' (error: %s)!\n",
                     String_cString(storageMsg.fileName),
                     getErrorText(error)
                    );
          createInfo->failError = error;
          break;
        }
        error = Storage_write(&createInfo->storageFileHandle,buffer,n);
        if (error != ERROR_NONE)
        {
          printInfo(0,"FAIL!\n");
          printError("Cannot write file '%s' (error: %s)!\n",
                     String_cString(storageMsg.destinationFileName),
                     getErrorText(error)
                    );
          createInfo->failError = error;
          break;
        }
        createInfo->statusInfo.storageDoneBytes += n;
        updateStatusInfo(createInfo);
      }
      while (   (createInfo->failError == ERROR_NONE)
             && ((createInfo->abortRequestFlag == NULL) || !(*createInfo->abortRequestFlag))
             && !File_eof(&fileHandle)
            );
      File_close(&fileHandle);

      /* close storage file */
      Storage_close(&createInfo->storageFileHandle);

      if (error == ERROR_NONE)
      {
        logMessage(LOG_TYPE_STORAGE,"stored '%s'",String_cString(storageMsg.destinationFileName));
        printInfo(0,"ok\n");
      }

      /* post-process */
      if (createInfo->failError == ERROR_NONE)
      {
        error = Storage_postProcess(&createInfo->storageFileHandle,FALSE);
        if (error != ERROR_NONE)
        {
          printError("Cannot post-process storage file '%s' (error: %s)!\n",
                     String_cString(storageMsg.fileName),
                     getErrorText(error)
                    );
          createInfo->failError = error;
        }
      }
    }
else
{
fprintf(stderr,"%s,%d: FAIL - only delete files? %s \n",__FILE__,__LINE__,getErrorText(createInfo->failError));
}
    /* delete source file */
    error = File_delete(storageMsg.fileName,FALSE);
    if (error != ERROR_NONE)
    {
      printWarning("Cannot delete file '%s' (error: %s)!\n",
                   String_cString(storageMsg.fileName),
                   getErrorText(error)
                  );
    }

    /* update storage info */
    Semaphore_lock(&createInfo->storageSemaphore);
    assert(createInfo->storageCount > 0);
    assert(createInfo->storageBytes>= storageMsg.fileSize);
    createInfo->storageCount -= 1;
    createInfo->storageBytes -= storageMsg.fileSize;
    Semaphore_unlock(&createInfo->storageSemaphore);

    /* free resources */
    String_delete(storageMsg.fileName);
    String_delete(storageMsg.destinationFileName);
  }

  /* final post-processing */
  if (createInfo->failError == ERROR_NONE)
  {
    error = Storage_postProcess(&createInfo->storageFileHandle,TRUE);
    if (error != ERROR_NONE)
    {
      printError("Cannot post-process storage (error: %s)!\n",
                 getErrorText(error)
                );
      createInfo->failError = error;
    }
  }

  /* free resoures */
  free(buffer);

  createInfo->storageThreadExitFlag = TRUE;
}

/*---------------------------------------------------------------------*/

Errors Command_create(const char                   *archiveFileName,
                      PatternList                  *includePatternList,
                      PatternList                  *excludePatternList,
                      Options                      *options,
                      CreateStatusInfoFunction     createStatusInfoFunction,
                      void                         *createStatusInfoUserData,
                      StorageRequestVolumeFunction storageRequestVolumeFunction,
                      void                         *storageRequestVolumeUserData,
                      bool                         *abortRequestFlag
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

  assert(archiveFileName != NULL);
  assert(includePatternList != NULL);
  assert(excludePatternList != NULL);

  /* initialise variables */
  createInfo.includePatternList           = includePatternList;
  createInfo.excludePatternList           = excludePatternList;
  createInfo.options                      = options;
  createInfo.abortRequestFlag             = abortRequestFlag;
  createInfo.archiveFileName              = String_newCString(archiveFileName);
  createInfo.startTime                    = time(NULL);
  createInfo.collectorSumThreadExitFlag   = FALSE;
  createInfo.collectorSumThreadExitedFlag = FALSE;
  createInfo.collectorThreadExitFlag      = FALSE;
  createInfo.storageCount                 = 0;
  createInfo.storageBytes                 = 0LL;
  createInfo.storageThreadExitFlag        = FALSE;
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
  fileName = String_new();

  /* init file name list, list lock and list signal */
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

  /* init storage */
  error = Storage_init(&createInfo.storageFileHandle,
                       String_setCString(fileName,archiveFileName),
                       createInfo.options,
                       storageRequestVolumeFunction,
                       storageRequestVolumeUserData,
                       (StorageStatusInfoFunction)updateStorageStatusInfo,
                       &createInfo,
                       createInfo.archiveFileName
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot create storage '%s' (error: %s)\n",
               archiveFileName,
               getErrorText(error)
              );
    Semaphore_done(&createInfo.storageSemaphore);
    MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
    MsgQueue_done(&createInfo.fileMsgQueue,NULL,NULL);
    String_delete(fileName);
    free(buffer);
    String_delete(createInfo.statusInfo.storageName);
    String_delete(createInfo.statusInfo.fileName);
    String_delete(createInfo.archiveFileName);

    return error;
  }

  if (   (options->archiveType == ARCHIVE_TYPE_FULL)
      || (options->archiveType == ARCHIVE_TYPE_INCREMENTAL)
      || !String_empty(options->incrementalListFileName)
     )
  {
    /* get increment list file name */
    incrementalListFileName = String_new();
    if (!String_empty(options->incrementalListFileName))
    {
      String_set(incrementalListFileName,options->incrementalListFileName);
    }
    else
    {
      formatArchiveFileName(incrementalListFileName,
                            createInfo.archiveFileName,
                            -1,
                            createInfo.startTime,
                            FALSE,
                            options
                           );
      String_appendCString(incrementalListFileName,".bid");
    }
    Dictionary_init(&createInfo.filesDictionary,NULL,NULL);
    storeIncrementalFileInfoFlag = TRUE;

    /* read incremental list */
    if ((options->archiveType == ARCHIVE_TYPE_INCREMENTAL) && File_exists(incrementalListFileName))
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
                   getErrorText(error)
                  );
        String_delete(incrementalListFileName);
        Semaphore_done(&createInfo.storageSemaphore);
        MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
        MsgQueue_done(&createInfo.fileMsgQueue,NULL,NULL);
        String_delete(fileName);
        free(buffer);
        Dictionary_done(&createInfo.filesDictionary,NULL,NULL);
        String_delete(createInfo.statusInfo.storageName);
        String_delete(createInfo.statusInfo.fileName);
        String_delete(createInfo.archiveFileName);

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
                         storeArchiveFile,
                         &createInfo,
                         options
                        );
  if (error != ERROR_NONE)
  {
    printError("Cannot create archive file '%s' (error: %s)\n",
               archiveFileName,
               getErrorText(error)
              );
    if (storeIncrementalFileInfoFlag)
    {
      Dictionary_done(&createInfo.filesDictionary,NULL,NULL);
      String_delete(incrementalListFileName);
    }
    Semaphore_done(&createInfo.storageSemaphore);
    MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
    MsgQueue_done(&createInfo.fileMsgQueue,NULL,NULL);
    String_delete(fileName);
    free(buffer);
    String_delete(createInfo.statusInfo.storageName);
    String_delete(createInfo.statusInfo.fileName);
    String_delete(createInfo.archiveFileName);

    return error;
  }

  /* start threads */
  if (!Thread_init(&createInfo.collectorSumThread,options->niceLevel,collectorSumThread,&createInfo))
  {
    HALT_FATAL_ERROR("Cannot initialise collector sum thread!");
  }
  if (!Thread_init(&createInfo.collectorThread,options->niceLevel,collectorThread,&createInfo))
  {
    HALT_FATAL_ERROR("Cannot initialise collector thread!");
  }
  if (!Thread_init(&createInfo.storageThread,options->niceLevel,storageThread,&createInfo))
  {
    HALT_FATAL_ERROR("Cannot initialise storage thread!");
  }

  /* store files */
  while (   ((createInfo.abortRequestFlag == NULL) || !(*createInfo.abortRequestFlag))
         && getNextFile(&createInfo.fileMsgQueue,fileName,&fileType)
        )
  {
    if (createInfo.failError == ERROR_NONE)
    {
      printInfo(1,"Add '%s'...",String_cString(fileName));

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
              if (options->skipUnreadableFlag)
              {
                printInfo(1,"skipped (reason: %s)\n",getErrorText(error));
                logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(fileName));
              }
              else
              {
                printInfo(1,"FAIL\n");
                printError("Cannot get info for file '%s' (error: %s)\n",
                           String_cString(fileName),
                           getErrorText(error)
                          );
                createInfo.failError = error;
              }
              continue;
            }

            if (!options->noStorageFlag)
            {
              /* open file */
              error = File_open(&fileHandle,fileName,FILE_OPENMODE_READ);
              if (error != ERROR_NONE)
              {
                if (options->skipUnreadableFlag)
                {
                  printInfo(1,"skipped (reason: %s)\n",getErrorText(error));
                  logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"open failed '%s'",String_cString(fileName));
                }
                else
                {
                  printInfo(1,"FAIL\n");
                  printError("Cannot open file '%s' (error: %s)\n",
                             String_cString(fileName),
                             getErrorText(error)
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
                           getErrorText(error)
                          );
                createInfo.failError = error;
                break;
              }
              String_set(createInfo.statusInfo.fileName,fileName);
              createInfo.statusInfo.fileDoneBytes = 0LL;
              createInfo.statusInfo.fileTotalBytes = fileInfo.size;
              updateStatusInfo(&createInfo);

              /* write file content into archive */
              error = ERROR_NONE;
              do
              {
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
              while (   ((createInfo.abortRequestFlag == NULL) || !(*createInfo.abortRequestFlag))
                     && (n > 0)
                     && (createInfo.failError == ERROR_NONE)
                     && (error == ERROR_NONE)
                    );
              if ((createInfo.abortRequestFlag != NULL) && (*createInfo.abortRequestFlag))
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
                           getErrorText(error)
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
                           getErrorText(error)
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
              if (options->skipUnreadableFlag)
              {
                printInfo(1,"skipped (reason: %s)\n",getErrorText(error));
                logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(fileName));
              }
              else
              {
                printInfo(1,"FAIL\n");
                printError("Cannot get info for directory '%s' (error: %s)\n",
                           String_cString(fileName),
                           getErrorText(error)
                          );
                createInfo.failError = error;
              }
              continue;
            }

            if (!options->noStorageFlag)
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
                           getErrorText(error)
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
                           getErrorText(error)
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
              if (options->skipUnreadableFlag)
              {
                printInfo(1,"skipped (reason: %s)\n",getErrorText(error));
                logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(fileName));
              }
              else
              {
                printInfo(1,"FAIL\n");
                printError("Cannot get info for link '%s' (error: %s)\n",
                           String_cString(fileName),
                           getErrorText(error)
                          );
                createInfo.failError = error;
              }
              continue;
            }

            if (!options->noStorageFlag)
            {
              /* read link */
              name = String_new();
              error = File_readLink(fileName,name);
              if (error != ERROR_NONE)
              {
                if (options->skipUnreadableFlag)
                {
                  printInfo(1,"skipped (reason: %s)\n",getErrorText(error));
                  logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"open failed '%s'",String_cString(fileName));
                }
                else
                {
                  printInfo(1,"FAIL\n");
                  printError("Cannot read link '%s' (error: %s)\n",
                             String_cString(fileName),
                             getErrorText(error)
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
                           getErrorText(error)
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
                           getErrorText(error)
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
              if (options->skipUnreadableFlag)
              {
                printInfo(1,"skipped (reason: %s)\n",getErrorText(error));
                logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(fileName));
              }
              else
              {
                printInfo(1,"FAIL\n");
                printError("Cannot get info for link '%s' (error: %s)\n",
                           String_cString(fileName),
                           getErrorText(error)
                          );
                createInfo.failError = error;
              }
              continue;
            }

            if (!options->noStorageFlag)
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
                           getErrorText(error)
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
                           getErrorText(error)
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

  /* close archive */
  Archive_close(&archiveInfo);
  createInfo.collectorSumThreadExitFlag = TRUE;
  MsgQueue_setEndOfMsg(&createInfo.fileMsgQueue);
  MsgQueue_setEndOfMsg(&createInfo.storageMsgQueue);
  updateStatusInfo(&createInfo);

  /* wait for threads */
  Thread_join(&createInfo.storageThread);
  Thread_join(&createInfo.collectorThread);
  Thread_join(&createInfo.collectorSumThread);

  /* close storage */
  Storage_done(&createInfo.storageFileHandle);

  /* write incremental list */
  if ((createInfo.failError == ERROR_NONE) && storeIncrementalFileInfoFlag)
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
                 getErrorText(error)
                );
      Semaphore_done(&createInfo.storageSemaphore);
      MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
      MsgQueue_done(&createInfo.fileMsgQueue,NULL,NULL);
      String_delete(fileName);
      free(buffer);
      Dictionary_done(&createInfo.filesDictionary,NULL,NULL);
      String_delete(createInfo.statusInfo.storageName);
      String_delete(createInfo.statusInfo.fileName);
      String_delete(createInfo.archiveFileName);

      return error;
    }
    printInfo(1,"ok\n");

    logMessage(LOG_TYPE_ALWAYS,"create incremental file '%s'",String_cString(incrementalListFileName));
  }

  /* output statics */
  printInfo(0,"%lu file(s)/%llu bytes(s) included\n",createInfo.statusInfo.doneFiles,createInfo.statusInfo.doneBytes);
  printInfo(2,"%lu file(s) skipped\n",createInfo.statusInfo.skippedFiles);
  printInfo(2,"%lu file(s) with errors\n",createInfo.statusInfo.errorFiles);

  /* free resources */
  if (storeIncrementalFileInfoFlag)
  {
    Dictionary_done(&createInfo.filesDictionary,NULL,NULL);
    String_delete(incrementalListFileName);
  }
  Semaphore_done(&createInfo.storageSemaphore);
  MsgQueue_done(&createInfo.storageMsgQueue,(MsgQueueMsgFreeFunction)freeStorageMsg,NULL);
  MsgQueue_done(&createInfo.fileMsgQueue,(MsgQueueMsgFreeFunction)freeFileMsg,NULL);
  String_delete(fileName);
  free(buffer);
  String_delete(createInfo.statusInfo.storageName);
  String_delete(createInfo.statusInfo.fileName);
  String_delete(createInfo.archiveFileName);

  if ((createInfo.abortRequestFlag == NULL) || !(*createInfo.abortRequestFlag))
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
