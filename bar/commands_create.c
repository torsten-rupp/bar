/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_create.c,v $
* $Revision: 1.38 $
* $Author: torsten $
* Contents: Backup ARchiver archive create function
* Systems : all
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

#define BUFFER_SIZE               (64*1024)

/***************************** Datatypes *******************************/

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

typedef struct
{
  PatternList                 *includePatternList;
  PatternList                 *excludePatternList;
  const Options               *options;
  bool                        *abortRequestFlag;                  // TRUE if abort requested

  StorageFileHandle           storageFileHandle;
  String                      fileName;                           // archive file name
  time_t                      startTime;                          // start time [ms] (unix time)

  MsgQueue                    fileMsgQueue;                       // queue with files to backup

  Thread                      collectorSumThread;                 // files collector sum thread id
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

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

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
  fileMsg.fileName = String_copy(fileName);
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
  while (   !createInfo->collectorThreadExitFlag
         && ((createInfo->abortRequestFlag == NULL) || !(*createInfo->abortRequestFlag))
         && (createInfo->failError == ERROR_NONE)
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
    while (   !createInfo->collectorThreadExitFlag
           && ((createInfo->abortRequestFlag == NULL) || !(*createInfo->abortRequestFlag))
           && (createInfo->failError == ERROR_NONE)
           && !StringList_empty(&nameList)
          )
    {
      /* get next directory to process */
      name = StringList_getLast(&nameList,name);
      if (   checkIsIncluded(includePatternNode,name)
          && !checkIsExcluded(createInfo->excludePatternList,name)
         )
      {
        switch (File_getType(name))
        {
          case FILE_TYPE_FILE:
            error = File_getFileInfo(name,&fileInfo);
            if (error == ERROR_NONE)
            {
              createInfo->statusInfo.totalFiles++;
              createInfo->statusInfo.totalBytes += fileInfo.size;
              updateStatusInfo(createInfo);
            }
            break;
          case FILE_TYPE_DIRECTORY:
            createInfo->statusInfo.totalFiles++;
            updateStatusInfo(createInfo);

            /* open directory contents */
            error = File_openDirectory(&directoryHandle,name);
            if (error == ERROR_NONE)
            {
              /* read directory contents */
              fileName = String_new();
              while (   !createInfo->collectorThreadExitFlag
                     && (createInfo->failError == ERROR_NONE)
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
                  /* detect file type */
                  switch (File_getType(fileName))
                  {
                    case FILE_TYPE_FILE:
                      error = File_getFileInfo(fileName,&fileInfo);
                      if (error == ERROR_NONE)
                      {
                        createInfo->statusInfo.totalFiles++;
                        createInfo->statusInfo.totalBytes += fileInfo.size;
                        updateStatusInfo(createInfo);
                      }
                      break;
                    case FILE_TYPE_DIRECTORY:
                      /* add to name list */
//fprintf(stderr,"%s,%d: fileName=%s\n",__FILE__,__LINE__,String_cString(fileName));
                      StringList_append(&nameList,fileName);
                      break;
                    case FILE_TYPE_LINK:
                      createInfo->statusInfo.totalFiles++;
                      updateStatusInfo(createInfo);
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
            createInfo->statusInfo.totalFiles++;
            updateStatusInfo(createInfo);
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
  while (   !createInfo->collectorThreadExitFlag
         && ((createInfo->abortRequestFlag == NULL) || !(*createInfo->abortRequestFlag))
         && (createInfo->failError == ERROR_NONE)
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
    while (   !createInfo->collectorThreadExitFlag
           && ((createInfo->abortRequestFlag == NULL) || !(*createInfo->abortRequestFlag))
           && (createInfo->failError == ERROR_NONE)
           && !StringList_empty(&nameList)
          )
    {
      /* get next directory to process */
      name = StringList_getLast(&nameList,name);
      if (   checkIsIncluded(includePatternNode,name)
          && !checkIsExcluded(createInfo->excludePatternList,name)
         )
      {
        switch (File_getType(name))
        {
          case FILE_TYPE_FILE:
            error = File_getFileInfo(name,&fileInfo);
            if (error == ERROR_NONE)
            {
              /* add to file list */
              appendToFileList(&createInfo->fileMsgQueue,name,FILE_TYPE_FILE);
            }
            else
            {
              logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(name));
              printInfo(2,"Cannot read directory '%s' (error: %s) - skipped\n",String_cString(name),getErrorText(error));
              createInfo->statusInfo.errorFiles++;
              updateStatusInfo(createInfo);
            }
            break;
          case FILE_TYPE_DIRECTORY:
            /* add to file list */
            appendToFileList(&createInfo->fileMsgQueue,name,FILE_TYPE_DIRECTORY);

            /* open directory contents */
            error = File_openDirectory(&directoryHandle,name);
            if (error == ERROR_NONE)
            {
              /* read directory contents */
              fileName = String_new();
              while (   !createInfo->collectorThreadExitFlag
                     && (createInfo->failError == ERROR_NONE)
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
                  /* detect file type */
                  switch (File_getType(fileName))
                  {
                    case FILE_TYPE_FILE:
                      error = File_getFileInfo(fileName,&fileInfo);
                      if (error == ERROR_NONE)
                      {
                        /* add to file list */
                        appendToFileList(&createInfo->fileMsgQueue,fileName,FILE_TYPE_FILE);
                      }
                      else
                      {
                        logMessage(LOG_TYPE_FILE_ACCESS_DENIED,"access denied '%s'",String_cString(fileName));
                        printInfo(2,"Cannot access file '%s' (error: %s) - skipped\n",String_cString(fileName),getErrorText(error));
                        createInfo->statusInfo.errorFiles++;
                        updateStatusInfo(createInfo);
                      }
                      break;
                    case FILE_TYPE_DIRECTORY:
                      /* add to name list */
                      StringList_append(&nameList,fileName);
                      break;
                    case FILE_TYPE_LINK:
                      /* add to file list */
                      appendToFileList(&createInfo->fileMsgQueue,fileName,FILE_TYPE_LINK);
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
            /* add to file list */
            appendToFileList(&createInfo->fileMsgQueue,name,FILE_TYPE_LINK);
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
  storageMsg.fileName            = String_copy(fileName);
  storageMsg.fileSize            = fileSize;
  storageMsg.destinationFileName = String_copy(destinationFileName);
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
                              bool   completeFlag,
                              int    partNumber,
                              void   *userData
                             )
{
  CreateInfo *createInfo = (CreateInfo*)userData;
  long       i0,i1;
  ulong      divisor;           
  ulong      n;
  int        d;
  String     destinationName; 
  struct tm  tmStruct;
  long       i;
  char       format[4];
  char       buffer[256];
  size_t     length;

  assert(createInfo != NULL);

  if (completeFlag)
  {
    /* get destination file name */
    destinationName = String_new();
    if (partNumber >= 0)
    {
      i0 = String_findChar(createInfo->fileName,STRING_BEGIN,'#');
      if (i0 >= 0)
      {
        /* find #...# and get max. divisor for part number */
        divisor = 1;
        i1 = i0+1;
        while ((i1 < String_length(createInfo->fileName) && String_index(createInfo->fileName,i1) == '#'))
        {
          i1++;
          if (divisor < 1000000000) divisor*=10;
        }

        /* format destination file name */
        String_sub(destinationName,createInfo->fileName,0,i0);
        n = partNumber;
        while (divisor > 0)
        {
          d = n/divisor; n = n%divisor; divisor = divisor/10;
          String_appendChar(destinationName,'0'+d);
        }
        String_appendSub(destinationName,createInfo->fileName,i1,STRING_END);
      }
      else
      {
        /* format destination file name */
        String_format(destinationName,"%S.%06d",createInfo->fileName,partNumber);
      }
    }
    else
    {
      String_set(destinationName,createInfo->fileName);
    }
    localtime_r(&createInfo->startTime,&tmStruct);
    i = 0;
    while ((i = String_findChar(destinationName,i,'%')) >= 0)
    {
      if ((i+1) < String_length(destinationName))
      {
        switch (String_index(destinationName,i+1))
        {
          case 'E':
          case 'O':
            format[0] = '%';
            format[1] = String_index(destinationName,i+1);
            format[2] = String_index(destinationName,i+2);
            format[3] = '\0';

            length = strftime(buffer,sizeof(buffer)-1,format,&tmStruct);

            String_remove(destinationName,i,3);
            String_insertBuffer(destinationName,i,buffer,length);
            i += length;
            break;
          case '%':
            String_remove(destinationName,i,1);
            i += 1;
            break;
          default:
            format[0] = '%';
            format[1] = String_index(destinationName,i+1);
            format[2] = '\0';

            length = strftime(buffer,sizeof(buffer)-1,format,&tmStruct);

            String_remove(destinationName,i,2);
            String_insertBuffer(destinationName,i,buffer,length);
            i += length;
            break;
        }
      }
      else
      {
       i += 1;
      }      
    }

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
  }

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

  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

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

  while (   !createInfo->storageThreadExitFlag
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
      while (   !createInfo->storageThreadExitFlag 
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

  if (createInfo->failError == ERROR_NONE)
  {
    /* final post-process */
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
  createInfo.fileName                     = String_new();
  createInfo.startTime                    = time(NULL);
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
                       createInfo.fileName
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
    String_delete(createInfo.statusInfo.fileName);
    String_delete(createInfo.statusInfo.storageName);

    return error;
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
    Semaphore_done(&createInfo.storageSemaphore);
    MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
    MsgQueue_done(&createInfo.fileMsgQueue,NULL,NULL);
    String_delete(fileName);
    free(buffer);
    String_delete(createInfo.statusInfo.fileName);
    String_delete(createInfo.statusInfo.storageName);

    return error;
  }

  /* start threads */
  if (!Thread_init(&createInfo.collectorSumThread,0,collectorSumThread,&createInfo))
  {
    HALT_FATAL_ERROR("Cannot initialise collector sum thread!");
  }
  if (!Thread_init(&createInfo.collectorThread,0,collectorThread,&createInfo))
  {
    HALT_FATAL_ERROR("Cannot initialise collector thread!");
  }
  if (!Thread_init(&createInfo.storageThread,0,storageThread,&createInfo))
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

            /* free resources */
            String_delete(name);

            printInfo(1,"ok\n");

            logMessage(LOG_TYPE_FILE_OK,"added '%s'",String_cString(fileName));
            createInfo.statusInfo.doneFiles++;
            updateStatusInfo(&createInfo);
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

  /* close archive */
  Archive_close(&archiveInfo);
  MsgQueue_setEndOfMsg(&createInfo.fileMsgQueue);
  MsgQueue_setEndOfMsg(&createInfo.storageMsgQueue);
  updateStatusInfo(&createInfo);

  /* wait for threads */
  if ((createInfo.abortRequestFlag == NULL) || !(*createInfo.abortRequestFlag))
  {
    createInfo.collectorThreadExitFlag = TRUE;
    createInfo.storageThreadExitFlag   = TRUE;
  }
  Thread_join(&createInfo.storageThread);
  Thread_join(&createInfo.collectorThread);
  Thread_join(&createInfo.collectorSumThread);

  /* output statics */
  printInfo(0,"%lu file(s)/%llu bytes(s) included\n",createInfo.statusInfo.doneFiles,createInfo.statusInfo.doneBytes);
  printInfo(2,"%lu file(s) skipped\n",createInfo.statusInfo.skippedFiles);
  printInfo(2,"%lu file(s) with errors\n",createInfo.statusInfo.errorFiles);

  /* free resources */
  Storage_done(&createInfo.storageFileHandle);
  Semaphore_done(&createInfo.storageSemaphore);
  MsgQueue_done(&createInfo.storageMsgQueue,(MsgQueueMsgFreeFunction)freeStorageMsg,NULL);
  MsgQueue_done(&createInfo.fileMsgQueue,(MsgQueueMsgFreeFunction)freeFileMsg,NULL);
  String_delete(fileName);
  free(buffer);
  String_delete(createInfo.statusInfo.fileName);
  String_delete(createInfo.statusInfo.storageName);
  String_delete(createInfo.fileName);

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
