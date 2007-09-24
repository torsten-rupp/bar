/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_create.c,v $
* $Revision: 1.14 $
* $Author: torsten $
* Contents: Backup ARchiver archive create function
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
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
#include "msgqueues.h"

#include "errors.h"
#include "patterns.h"
#include "files.h"
#include "archive.h"
#include "crypt.h"

#include "command_create.h"

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
  uint64 length;
  bool   completeFlag;
} StorageMsg;

/***************************** Variables *******************************/
LOCAL MsgQueue    fileMsgQueue;

LOCAL PatternList *collectorThreadIncludePatternList;
LOCAL PatternList *collectorThreadExcludePatternList;
LOCAL bool        collectorThreadExitFlag;
LOCAL pthread_t   collectorThread;

LOCAL MsgQueue    storageMsgQueue;
LOCAL bool        storageThreadExitFlag;
LOCAL pthread_t   storageThread;

LOCAL struct
{
  ulong  includedCount;
  uint64 includedByteSum;
  ulong  excludedCount;
  ulong  skippedCount;
  ulong  errorCount;
} statistics;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : lockFileList
* Purpose: lock file list
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void lockFileList(void)
{
//  pthread_mutex_lock(&fileListLock);
}

/***********************************************************************\
* Name   : unlockFileList
* Purpose: unlock filename list
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void unlockFileList(void)
{
//  pthread_mutex_unlock(&fileListLock);
}

/***********************************************************************\
* Name   : waitFileListModified
* Purpose: wait until file name list is modified
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void waitFileListModified(void)
{
//  pthread_cond_wait(&fileListModified,&fileListLock);
}

/***********************************************************************\
* Name   : signalFileListModified
* Purpose: signal file name list modified
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void signalFileListModified(void)
{
//  pthread_cond_broadcast(&fileListModified);
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

  return Patterns_match(includePatternNode,fileName,PATTERN_MATCH_MODE_BEGIN);
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

  return Patterns_matchList(excludePatternList,fileName,PATTERN_MATCH_MODE_BEGIN);
}

/***********************************************************************\
* Name   : appendFileNameToList
* Purpose: append a filename to a filename list
* Input  : fileMsgQueue - file message queue
*          fileName     - file name (will be copied!)
*          fileType     - file type
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendFileToList(MsgQueue  *fileMsgQueue,
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
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

/***********************************************************************\
* Name   : collector
* Purpose: file collector thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void collector(void)
{
  StringList      nameList;
  String          name;
  PatternNode     *includePatternNode;
  StringTokenizer fileNameTokenizer;
  String          basePath;
  String          s;
  Errors          error;
  String          fileName;
  DirectoryHandle directoryHandle;

  assert(collectorThreadIncludePatternList != NULL);
  assert(collectorThreadExcludePatternList != NULL);

  StringList_init(&nameList);
  name = String_new();

  includePatternNode = collectorThreadIncludePatternList->head;
  while (!collectorThreadExitFlag && (includePatternNode != NULL))
  {
    /* find base path */
    basePath = String_new();
    Files_initSplitFileName(&fileNameTokenizer,includePatternNode->pattern);
    if (Files_getNextSplitFileName(&fileNameTokenizer,&s) && !Patterns_checkIsPattern(name))
    {
      if (String_length(s) > 0)
      {
        Files_setFileName(basePath,s);
      }
      else
      {
        Files_setFileNameChar(basePath,FILES_PATHNAME_SEPARATOR_CHAR);
      }
    }
    while (Files_getNextSplitFileName(&fileNameTokenizer,&s) && !Patterns_checkIsPattern(name))
    {
      Files_appendFileName(basePath,s);
    }
    Files_doneSplitFileName(&fileNameTokenizer);

    /* find files */
    StringList_append(&nameList,basePath);
    while (!collectorThreadExitFlag && !StringList_empty(&nameList))
    {
      /* get next directory to process */
      name = StringList_getFirst(&nameList,name);
      if (   checkIsIncluded(includePatternNode,name)
          && !checkIsExcluded(collectorThreadExcludePatternList,name)
         )
      {
        switch (Files_getType(name))
        {
          case FILETYPE_FILE:
            /* add to file list */
            appendFileToList(&fileMsgQueue,name,FILETYPE_FILE);
            break;
          case FILETYPE_DIRECTORY:
            /* add to file list */
            appendFileToList(&fileMsgQueue,name,FILETYPE_DIRECTORY);

            /* open directory contents */
            error = Files_openDirectory(&directoryHandle,name);
            if (error == ERROR_NONE)
            {
              /* read directory contents */
              fileName = String_new();
              while (!Files_endOfDirectory(&directoryHandle))
              {
                /* read next directory entry */
                error = Files_readDirectory(&directoryHandle,fileName);
                if (error != ERROR_NONE)
                {
                  printError("Cannot read directory '%s' (error: %s)\n",
                             String_cString(name),
                             getErrorText(error)
                            );
                  statistics.errorCount++;
                  continue;
                }

                if (   checkIsIncluded(includePatternNode,fileName)
                    && !checkIsExcluded(collectorThreadExcludePatternList,fileName)
                   )
                {
                  /* detect file type */
                  switch (Files_getType(fileName))
                  {
                    case FILETYPE_FILE:
                      /* add to file list */
                      appendFileToList(&fileMsgQueue,fileName,FILETYPE_FILE);
                      break;
                    case FILETYPE_DIRECTORY:
                      /* add to name list */
                      StringList_append(&nameList,fileName);
                      break;
                    case FILETYPE_LINK:
                      /* add to file list */
                      appendFileToList(&fileMsgQueue,fileName,FILETYPE_LINK);
                      break;
                    default:
                      info(2,"Unknown type of file '%s' - skipped\n",String_cString(fileName));
                      statistics.skippedCount++;
                      break;
                  }
                }
                else
                {
                  statistics.excludedCount++;
                }
              }

              /* close directory, free resources */
              String_delete(fileName);
              Files_closeDirectory(&directoryHandle);
            }
            else
            {
              printError("Cannot open directory '%s' (error: %s)\n",
                         String_cString(name),
                         getErrorText(error)
                        );
              statistics.errorCount++;
            }
            break;
          case FILETYPE_LINK:
            /* add to file list */
            appendFileToList(&fileMsgQueue,name,FILETYPE_LINK);
            break;
          default:
            info(2,"Unknown type of file '%s' - skipped\n",String_cString(name));
            statistics.skippedCount++;
            break;
        }
      }
      else
      {
        statistics.excludedCount++;
      }
    }

    /* free resources */
    String_delete(basePath);

    /* next include pattern */
    includePatternNode = includePatternNode->next;
  }
  MsgQueue_setEndOfMsg(&fileMsgQueue);
  collectorThreadExitFlag = TRUE;

  /* send signal to waiting threads */
  signalFileListModified();

  /* free resoures */
  String_delete(name);
  StringList_done(&nameList,NULL);
}

/***********************************************************************\
* Name   : storage
* Purpose: archive storage thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void storage(void)
{
  Errors          error;

return;
  while (!storageThreadExitFlag)
  {
    /* store archive files */
    while (!collectorThreadExitFlag )
    {
    }
  }
  storageThreadExitFlag = TRUE;

  /* send signal to waiting threads */
  signalFileListModified();

  /* free resoures */
}

/*---------------------------------------------------------------------*/

bool command_create(const char      *archiveFileName,
                    PatternList     *includePatternList,
                    PatternList     *excludePatternList,
                    const char      *tmpDirectory,
                    ulong           partSize,
                    uint            compressAlgorithm,
                    ulong           compressMinFileSize,
                    CryptAlgorithms cryptAlgorithm,
                    const char      *password
                   )
{
  bool            failFlag;
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
  statistics.includedCount   = 0;
  statistics.includedByteSum = 0;
  statistics.excludedCount   = 0;
  statistics.skippedCount    = 0;

  /* allocate resources */
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init file name list, list locka and list signal */
  if (!MsgQueue_init(&fileMsgQueue,MAX_FILE_MSG_QUEUE_ENTRIES))
  {
    HALT_FATAL_ERROR("Cannot initialise file message queue!");
  }
  if (!MsgQueue_init(&storageMsgQueue,MAX_STORAGE_MSG_QUEUE_ENTRIES))
  {
    HALT_FATAL_ERROR("Cannot initialise storage message queue!");
  }

  /* start threads */
  collectorThreadIncludePatternList = includePatternList;
  collectorThreadExcludePatternList = excludePatternList;
  collectorThreadExitFlag           = FALSE;
  if (pthread_create(&collectorThread,NULL,(void*(*)(void*))collector,NULL) != 0)
  {
    HALT_FATAL_ERROR("Cannot initialise collector thread!");
  }
  if (pthread_create(&storageThread,NULL,(void*(*)(void*))storage,NULL) != 0)
  {
    HALT_FATAL_ERROR("Cannot initialise storage thread!");
  }

  /* create new archive */
  error = Archive_create(&archiveInfo,
                         archiveFileName,
                         partSize,
                         compressAlgorithm,
                         compressMinFileSize,
                         cryptAlgorithm,
                         password
                        );
  if (error != ERROR_NONE)
  {
    printError("Cannot create archive file '%s' (error: %s)\n",
               archiveFileName,
               getErrorText(error)
              );

    /* stop threads */
    collectorThreadExitFlag = TRUE;
    storageThreadExitFlag = TRUE;
    pthread_join(collectorThread,NULL);
    pthread_join(storageThread,NULL);

    /* free resources */
    MsgQueue_done(&storageMsgQueue,NULL,NULL);
    MsgQueue_done(&fileMsgQueue,NULL,NULL);
    free(buffer);

    return FALSE;
  }

  /* store files */
  fileName = String_new();
  failFlag = FALSE;
  while (getNextFile(&fileMsgQueue,fileName,&fileType))
  {
    info(0,"Store '%s'...",String_cString(fileName));

    switch (fileType)
    {
      case FILETYPE_FILE:
        {
          FileInfo   fileInfo;
          FileHandle fileHandle;
          ulong      n;
          double     ratio;

          /* get file info */
          error = Files_getFileInfo(fileName,&fileInfo);
          if (error != ERROR_NONE)
          {
            info(0,"fail\n");
            printError("Cannot get info for file '%s' (error: %s)\n",
                       String_cString(fileName),
                       getErrorText(error)
                      );
            failFlag = TRUE;
            continue;
          }

          /* new file */
          error = Archive_newFileEntry(&archiveInfo,
                                       &archiveFileInfo,
                                       fileName,
                                       &fileInfo
                                      );
          if (error != ERROR_NONE)
          {
            info(0,"fail\n");
            printError("Cannot create new archive entry '%s' (error: %s)\n",
                       String_cString(fileName),
                       getErrorText(error)
                      );
            failFlag = TRUE;
            break;
          }

          /* write file content into archive */  
          error = Files_open(&fileHandle,fileName,FILE_OPENMODE_READ);
          if (error != ERROR_NONE)
          {
            info(0,"fail\n");
            printError("Cannot open file '%s' (error: %s)\n",
                       String_cString(fileName),
                       getErrorText(error)
                      );
            failFlag = TRUE;
            continue;
          }
          error = ERROR_NONE;
          do
          {
            Files_read(&fileHandle,buffer,BUFFER_SIZE,&n);
            if (n > 0)
            {
              error = Archive_writeFileData(&archiveFileInfo,buffer,n);
            }
          }
          while ((n > 0) && (error == ERROR_NONE));
          Files_close(&fileHandle);
          if (error != ERROR_NONE)
          {
            info(0,"fail\n");
            printError("Cannot create archive file!\n");
            Archive_closeEntry(&archiveFileInfo);
            failFlag = TRUE;
            break;
          }

          /* close archive entry */
          Archive_closeEntry(&archiveFileInfo);

          /* update statistics */
          statistics.includedCount++;
          statistics.includedByteSum += fileInfo.size;

          if ((archiveFileInfo.file.compressAlgorithm != COMPRESS_ALGORITHM_NONE) && (archiveFileInfo.file.chunkFileData.fragmentSize > 0))
          {
            ratio = 100.0-archiveFileInfo.file.chunkInfoFileData.size*100.0/archiveFileInfo.file.chunkFileData.fragmentSize;
          }
          else
          {
            ratio = 0;
          }
          info(0,"ok (ratio %.1f%%)\n",ratio);
        }
        break;
      case FILETYPE_DIRECTORY:
        {
          FileInfo fileInfo;

          /* get directory info */
          error = Files_getFileInfo(fileName,&fileInfo);
          if (error != ERROR_NONE)
          {
            info(0,"fail\n");
            printError("Cannot get info for directory '%s' (error: %s)\n",
                       String_cString(fileName),
                       getErrorText(error)
                      );
            failFlag = TRUE;
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
            info(0,"fail\n");
            printError("Cannot create new archive entry '%s' (error: %s)\n",
                       String_cString(fileName),
                       getErrorText(error)
                      );
            failFlag = TRUE;
            break;
          }

          /* close archive entry */
          Archive_closeEntry(&archiveFileInfo);

          /* free resources */

          /* update statistics */
          statistics.includedCount++;

          info(0,"ok\n");
        }
        break;
      case FILETYPE_LINK:
        {
          FileInfo fileInfo;
          String   name;

          /* get file info */
          error = Files_getFileInfo(fileName,&fileInfo);
          if (error != ERROR_NONE)
          {
            info(0,"fail\n");
            printError("Cannot get info for file '%s' (error: %s)\n",
                       String_cString(fileName),
                       getErrorText(error)
                      );
            failFlag = TRUE;
            continue;
          }

          /* read link */
          name = String_new();
          error = Files_readLink(fileName,name);
          if (error != ERROR_NONE)
          {
            info(0,"fail\n");
            printError("Cannot read link '%s' (error: %s)\n",
                       String_cString(fileName),
                       getErrorText(error)
                      );
            String_delete(name);
            failFlag = TRUE;
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
            info(0,"fail\n");
            printError("Cannot create new archive entry '%s' (error: %s)\n",
                       String_cString(fileName),
                       getErrorText(error)
                      );
            String_delete(name);
            failFlag = TRUE;
            break;
          }

          /* close archive entry */
          Archive_closeEntry(&archiveFileInfo);

          /* free resources */
          String_delete(name);

          /* update statistics */
          statistics.includedCount++;

          info(0,"ok\n");
        }
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
  }
  collectorThreadExitFlag = TRUE;
  String_delete(fileName);

  /* close archive */
  Archive_close(&archiveInfo);

  /* wait for threads */
  pthread_join(collectorThread,NULL);
  pthread_join(storageThread,NULL);

  /* free resources */
  MsgQueue_done(&storageMsgQueue,NULL,NULL);
  MsgQueue_done(&fileMsgQueue,NULL,NULL);
  free(buffer);

  /* output statics */
  info(0,"%lu file(s)/%llu bytes(s) included\n",statistics.includedCount,statistics.includedByteSum);
  info(1,"%lu file(s) excluded\n",statistics.excludedCount);
  info(1,"%lu file(s) skipped\n",statistics.skippedCount);
  info(1,"%lu error(s)\n",statistics.errorCount);

  return !failFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
