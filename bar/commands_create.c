/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_create.c,v $
* $Revision: 1.6 $
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

#include "errors.h"
#include "patterns.h"
#include "files.h"
#include "archive.h"
#include "crypt.h"

#include "command_create.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL pthread_mutex_t    fileNameListLock;
LOCAL pthread_cond_t     fileNameListNew;
LOCAL FileNameList       fileNameList;

LOCAL PatternList        *collectorThreadIncludePatternList;
LOCAL PatternList        *collectorThreadExcludePatternList;
LOCAL bool               collectorThreadExitFlag;
LOCAL pthread_t          collectorThread;

LOCAL struct
{
  ulong includedCount;
  ulong excludedCount;
  ulong skippedCount;
  ulong errorCount;
} statistics;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : lockFileNameList
* Purpose: lock filename list
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void lockFileNameList(void)
{
  pthread_mutex_lock(&fileNameListLock);
}

/***********************************************************************\
* Name   : unlockFileNameList
* Purpose: unlock filename list
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void unlockFileNameList(void)
{
  pthread_mutex_unlock(&fileNameListLock);
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

  return Patterns_match(includePatternNode,fileName);
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

  return Patterns_matchList(excludePatternList,fileName);
}

/***********************************************************************\
* Name   : appendFileNameToList
* Purpose: append a filename to a filename list
* Input  : fileNameList - filename list
*          fileName     - filename to add (will be copied!)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendFileNameToList(FileNameList *fileNameList, String fileName)
{
  FileNameNode *fileNameNode;

  assert(fileNameList != NULL);
  assert(fileName != NULL);

  /* allocate node */
  fileNameNode = (FileNameNode*)malloc(sizeof(FileNameNode));
  if (fileNameNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  fileNameNode->fileName = String_copy(fileName);

  /* add */
  lockFileNameList();
  Lists_add(fileNameList,fileNameNode);
  unlockFileNameList();

  /* send signal to waiting threads */
  pthread_cond_broadcast(&fileNameListNew);
}

/***********************************************************************\
* Name   : freeFileNameNode
* Purpose: free filename node
* Input  : fileNameNode - filename node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeFileNameNode(FileNameNode *fileNameNode, void *userData)
{
  assert(fileNameNode != NULL);

  String_delete(fileNameNode->fileName);
}

/***********************************************************************\
* Name   : getNextFile
* Purpose: get next file from list of files to pack
* Input  : fileName - filename
* Output : -
* Return : filename or NULL of no more file available
* Notes  : -
\***********************************************************************/

LOCAL String getNextFile(String fileName)
{
  FileNameNode *fileNameNode;

  lockFileNameList();
  fileNameNode = (FileNameNode*)Lists_getFirst(&fileNameList);
  while ((fileNameNode == NULL) && !collectorThreadExitFlag)
  {
    pthread_cond_wait(&fileNameListNew,&fileNameListLock);
    fileNameNode = (FileNameNode*)Lists_getFirst(&fileNameList);
  }
  unlockFileNameList();
  if (fileNameNode != NULL)
  {
    String_set(fileName,fileNameNode->fileName);
    freeFileNameNode(fileNameNode,NULL);
    free(fileNameNode);
  }
  else
  {
    fileName = NULL;
  }

  return fileName;
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
  PatternNode     *includePatternNode;
  StringTokenizer fileNameTokenizer;
  String          s;
  String          basePath;
  FileNameList    directoryList;
  FileNameNode    *fileNameNode;
  Errors          error;
  String          fileName;
  DirectoryHandle directoryHandle;

  assert(collectorThreadIncludePatternList != NULL);
  assert(collectorThreadExcludePatternList != NULL);

  includePatternNode = collectorThreadIncludePatternList->head;
  while (!collectorThreadExitFlag && (includePatternNode != NULL))
  {
    /* find base path */
    basePath = String_new();
    String_initTokenizer(&fileNameTokenizer,includePatternNode->pattern,FILES_PATHNAME_SEPARATOR_CHARS,NULL);
    while (String_getNextToken(&fileNameTokenizer,&s,NULL) && !Patterns_checkIsPattern(s))
    {
      Files_appendFileName(basePath,s);
    }

    /* find files */
    Lists_init(&directoryList);
    appendFileNameToList(&directoryList,basePath);
    while (!collectorThreadExitFlag && !Lists_empty(&directoryList))
    {
      /* get next directory to process */
      fileNameNode = (FileNameNode*)Lists_getFirst(&directoryList);
      switch (Files_getType(fileNameNode->fileName))
      {
        case FILETYPE_FILE:
          if (   checkIsIncluded(includePatternNode,fileNameNode->fileName)
              && !checkIsExcluded(collectorThreadExcludePatternList,fileNameNode->fileName)
             )
          {
            /* add to file list */
            appendFileNameToList(&fileNameList,fileNameNode->fileName);
            statistics.includedCount++;
//fprintf(stderr,"%s,%d: collect %s\n",__FILE__,__LINE__,String_cString(fileName));
          }
          else
          {
            statistics.excludedCount++;
          }
          break;
        case FILETYPE_DIRECTORY:
          /* read directory contents */
          error = Files_openDirectory(&directoryHandle,fileNameNode->fileName);
          if (error == ERROR_NONE)
          {
            fileName = String_new();
            while (!Files_endOfDirectory(&directoryHandle))
            {
              /* read next directory entry */
              error = Files_readDirectory(&directoryHandle,fileName);
              if (error != ERROR_NONE)
              {
                printError("Cannot read directory '%s' (error: %s)\n",String_cString(fileName),getErrorText(error));
                statistics.errorCount++;
                continue;
              }

              /* detect file type */
              switch (Files_getType(fileName))
              {
                case FILETYPE_FILE:
                  if (   checkIsIncluded(includePatternNode,fileName)
                      && !checkIsExcluded(collectorThreadExcludePatternList,fileName)
                     )
                  {
                    /* add to file list */
                    appendFileNameToList(&fileNameList,fileName);
                    statistics.includedCount++;
                  }
                  else
                  {
                    statistics.excludedCount++;
                  }
                  break;
                case FILETYPE_DIRECTORY:
                  /* add to directory list */
                  appendFileNameToList(&directoryList,fileName);
                  break;
                case FILETYPE_LINK:
                  if (   checkIsIncluded(includePatternNode,fileName)
                      && !checkIsExcluded(collectorThreadExcludePatternList,fileName)
                     )
                  {
                    /* add to file list */
                    appendFileNameToList(&fileNameList,fileName);
                    statistics.includedCount++;
                  }
                  else
                  {
                    statistics.excludedCount++;
                  }
                  break;
                default:
                  info(2,"Unknown type of file '%s' - skipped\n",String_cString(fileName));
                  statistics.skippedCount++;
                  break;
              }

            }
            String_delete(fileName);

            Files_closeDirectory(&directoryHandle);
          }
          else
          {
            printError("Cannot open directory '%s' (error: %s)\n",String_cString(fileNameNode->fileName),getErrorText(error));
            statistics.errorCount++;
          }
          break;
        case FILETYPE_LINK:
          if (   checkIsIncluded(includePatternNode,fileNameNode->fileName)
              && !checkIsExcluded(collectorThreadExcludePatternList,fileNameNode->fileName)
             )
          {
            /* add to file list */
            appendFileNameToList(&fileNameList,fileName);
            statistics.includedCount++;
          }
          else
          {
            statistics.excludedCount++;
          }
          break;
        default:
          info(2,"Unknown type of file '%s' - skipped\n",String_cString(fileNameNode->fileName));
          statistics.skippedCount++;
          break;
      }

      /* free resources */
      freeFileNameNode(fileNameNode,NULL);
      free(fileNameNode);
    }
    Lists_done(&directoryList,NULL,NULL);

    /* free resources */
    String_delete(basePath);

    /* next include pattern */
    includePatternNode = includePatternNode->next;
  }
  collectorThreadExitFlag = TRUE;

  /* send signal to waiting threads */
  pthread_cond_broadcast(&fileNameListNew);
}

/*---------------------------------------------------------------------*/

bool command_create(const char      *archiveFileName,
                    PatternList     *includePatternList,
                    PatternList     *excludePatternList,
                    const char      *tmpDirectory,
                    ulong           partSize,
                    uint            compressAlgorithm,
                    CryptAlgorithms cryptAlgorithm,
                    const char      *password
                   )
{
  bool            failFlag;
  ArchiveInfo     archiveInfo;
  Errors          error;
  byte            *buffer;
  String          fileName;
  FileInfo        fileInfo;
  ArchiveFileInfo archiveFileInfo;
  FileHandle      fileHandle;
  ulong           n;
ulong xx;

  assert(archiveFileName != NULL);
  assert(includePatternList != NULL);
  assert(excludePatternList != NULL);

  /* initialise variables */
  statistics.includedCount = 0;
  statistics.excludedCount = 0;
  statistics.skippedCount  = 0;

  /* allocate resources */
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  fileName = String_new();

  /* init file name list, list locka and list signal */
  Lists_init(&fileNameList);
  if (pthread_mutex_init(&fileNameListLock,NULL) != 0)
  {
    HALT_FATAL_ERROR("Cannot initialise filename list lock semaphore!");
  }
  if (pthread_cond_init(&fileNameListNew,NULL) != 0)
  {
    HALT_FATAL_ERROR("Cannot initialise filename list new event!");
  }

  /* start file collector thread */
  collectorThreadIncludePatternList = includePatternList;
  collectorThreadExcludePatternList = excludePatternList;
  collectorThreadExitFlag           = FALSE;
  if (pthread_create(&collectorThread,NULL,(void*(*)(void*))collector,NULL) != 0)
  {
    HALT_FATAL_ERROR("Cannot initialise collector thread!");
  }

  /* create new archive */
  error = Archive_create(&archiveInfo,
                         archiveFileName,
                         partSize,
                         compressAlgorithm,
                         cryptAlgorithm,
                         password
                        );
  if (error != ERROR_NONE)
  {
    printError("Cannot create archive file '%s' (error: %s)\n",archiveFileName,getErrorText(error));

    /* stop collector thread */
    collectorThreadExitFlag = TRUE;
    pthread_join(collectorThread,NULL);

    /* free resources */
    Lists_done(&fileNameList,(NodeFreeFunction)freeFileNameNode,NULL);
    pthread_cond_destroy(&fileNameListNew);
    pthread_mutex_destroy(&fileNameListLock);
    String_delete(fileName);
    free(buffer);

    return FALSE;
  }

  /* store files */
  failFlag = FALSE;
  while (getNextFile(fileName) != NULL)
  {
    info(0,"Store '%s'...",String_cString(fileName));

    /* get file info */
    error = Files_getInfo(fileName,&fileInfo);
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
  // log ???
      continue;
    }

    /* new file */
    error = Archive_newFile(&archiveInfo,
                            &archiveFileInfo,
                            fileName,
                            &fileInfo
                           );
    if (error != ERROR_NONE)
    {
      info(0,"fail\n");
      printError("Cannot create new archive file '%s' (error: %s)\n",String_cString(fileName),getErrorText(error));
      failFlag = TRUE;
      continue;
    }

    /* write file content into archive */  
    error = Files_open(&fileHandle,fileName,FILE_OPENMODE_READ);
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
  // log ???
      failFlag = TRUE;
      continue;
    }
    error = ERROR_NONE;
xx=0;
    do
    {
      Files_read(&fileHandle,buffer,BUFFER_SIZE,&n);
      if (n > 0)
      {
        error = Archive_writeFileData(&archiveFileInfo,buffer,n);
xx+=n;
      }
    }
    while ((n > 0) && (error == ERROR_NONE));
    Files_close(&fileHandle);
    if (error != ERROR_NONE)
    {
// log ???
      Archive_closeFile(&archiveFileInfo);
      info(0,"fail\n");
      printError("Cannot create archive file!\n");
      failFlag = TRUE;
      break;
    }

    /* close archive file */
    Archive_closeFile(&archiveFileInfo);

    info(0,"ok\n");
  }

  /* close archive */
  Archive_done(&archiveInfo);

  /* wait for collector thread */
  pthread_join(collectorThread,NULL);

  /* free resources */
  Lists_done(&fileNameList,(NodeFreeFunction)freeFileNameNode,NULL);
  pthread_cond_destroy(&fileNameListNew);
  pthread_mutex_destroy(&fileNameListLock);
  String_delete(fileName);
  free(buffer);

  /* output statics */
  info(0,"%lu file(s) included\n",statistics.includedCount);
  info(1,"%lu file(s) excluded\n",statistics.excludedCount);
  info(1,"%lu file(s) skipped\n",statistics.skippedCount);
  info(1,"%lu error(s)\n",statistics.errorCount);

  return !failFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
