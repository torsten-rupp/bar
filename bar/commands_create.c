/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_create.c,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: Backup ARchiver archive functions
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

#include "bar.h"
#include "files.h"
#include "archive.h"

#include "command_create.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL bool            exitFlag;
LOCAL const char      *archiveFileName;
LOCAL PatternList     *includePatternList;
LOCAL PatternList     *excludePatternList;
LOCAL ulong           partSize;

LOCAL pthread_mutex_t fileNameListLock;
LOCAL pthread_cond_t  fileNameListNew;
LOCAL FileNameList    fileNameList;

LOCAL bool            collectorDone;
LOCAL pthread_t       threadCollector;
LOCAL pthread_t       threadPacker;

LOCAL struct
{
  ulong includedCount;
  ulong excludedCount;
  ulong skippedCount;
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
* Name   : checkIsPattern
* Purpose: check is string a pattern
* Input  : s - string
* Output : -
* Return : TRUE is s is a pattern, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool checkIsPattern(String s)
{
// ???
return FALSE;
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool checkIsIncluded(PatternNode *includePatternNode,
                           String      fileName
                          )
{
  assert(includePatternNode != NULL);
  assert(fileName != NULL);

return TRUE;
}

/***********************************************************************\
* Name   : checkIsExcluded
* Purpose: check if filename is excluded
* Input  : s - filename
* Output : -
* Return : TRUE if excluded, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool checkIsExcluded(PatternList *excludePatternList,
                           String      fileName
                          )
{
  bool        excludeFlag;
  PatternNode *excludePatternNode;

  assert(excludePatternList != NULL);
  assert(fileName != NULL);

  excludeFlag = FALSE;
  excludePatternNode = excludePatternList->head;
  while (!exitFlag && (excludePatternNode != NULL) && !excludeFlag)
  {
    /* match with exclude pattern */

    excludePatternNode = excludePatternNode->next;
  }

  return excludeFlag;
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
  List_add(fileNameList,fileNameNode);
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
  fileNameNode = (FileNameNode*)List_getFirst(&fileNameList);
  while ((fileNameNode == NULL) && !collectorDone)
  {
    pthread_cond_wait(&fileNameListNew,&fileNameListLock);
    fileNameNode = (FileNameNode*)List_getFirst(&fileNameList);
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

  assert(includePatternList != NULL);
  assert(excludePatternList != NULL);

  includePatternNode = includePatternList->head;
  while (!exitFlag && (includePatternNode != NULL))
  {
    /* find base path */
    basePath = String_new();
    String_initTokenizer(&fileNameTokenizer,includePatternNode->pattern,FILES_PATHNAME_SEPARATOR_CHARS,NULL);
    s = String_new();
    while (String_getNextToken(&fileNameTokenizer,&s,NULL) && !checkIsPattern(s))
    {
      if (String_length(basePath) > 0) String_appendChar(basePath,FILES_PATHNAME_SEPARATOR_CHAR);
      String_append(basePath,s);
    }
    String_delete(s);

    /* find files */
    List_init(&directoryList);
    appendFileNameToList(&directoryList,basePath);
    while (!List_empty(&directoryList))
    {
      /* get next directory to process */
      fileNameNode = (FileNameNode*)List_getFirst(&directoryList);
      if (checkIsIncluded(includePatternNode,fileNameNode->fileName))
      {
        switch (files_getType(fileNameNode->fileName))
        {
          case FILETYPE_FILE:
            /* add to file list */
            appendFileNameToList(&fileNameList,fileNameNode->fileName);
            statistics.includedCount++;
//fprintf(stderr,"%s,%d: collect %s\n",__FILE__,__LINE__,String_cString(fileName));
            break;
          case FILETYPE_DIRECTORY:
            /* read directory contents */
            error = files_openDirectory(&directoryHandle,fileNameNode->fileName);
            if (error == ERROR_NONE)
            {
              fileName = String_new();
              while (!files_endOfDirectory(&directoryHandle))
              {
                error = files_readDirectory(&directoryHandle,fileName);
                if (error != ERROR_NONE)
                {
        //??? log
HALT_INTERNAL_ERROR("x");
                }

                /* filter excludes */
                if (!checkIsExcluded(excludePatternList,fileName))
                {
                  /* detect file type */
                  switch (files_getType(fileName))
                  {
                    case FILETYPE_FILE:
                      /* add to file list */
                      appendFileNameToList(&fileNameList,fileName);
                      statistics.includedCount++;
    //fprintf(stderr,"%s,%d: collect %s\n",__FILE__,__LINE__,String_cString(fileName));
                      break;
                    case FILETYPE_DIRECTORY:
                      /* add to directory list */
                      appendFileNameToList(&directoryList,fileName);
                      break;
                    case FILETYPE_LINK:
        // ???
                      break;
                    default:
                      // ??? log
                      break;
                  }
                }
                else
                {
                  statistics.excludedCount++;
                }

              }
              String_delete(fileName);

              files_closeDirectory(&directoryHandle);
            }
            else
            {
        //??? log
            }
            break;
          case FILETYPE_LINK:
// ???
            break;
          default:
            // ??? log
            break;
        }
      }

      /* free resources */
      freeFileNameNode(fileNameNode,NULL);
      free(fileNameNode);
    }
    List_done(&directoryList,NULL,NULL);

    String_delete(basePath);

    /* next include pattern */
    includePatternNode = includePatternNode->next;
  }
  collectorDone = TRUE;
}

/***********************************************************************\
* Name   : packer
* Purpose: file packer thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void packer(void)
{
  ArchiveInfo     archiveInfo;
  Errors          error;
  void            *buffer;
  String          fileName;
  FileInfo        fileInfo;
  ArchiveFileInfo archiveFileInfo;
  int             inputHandle;
  ssize_t         n;

  /* allocate resources */
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  fileName = String_new();

  /* create new archive */
  error = archive_create(&archiveInfo,
                         archiveFileName,
                         partSize
                        );
  if (error != ERROR_NONE)
  {
    free(buffer);
HALT(1,"x");
  }

  fileInfo.name = String_new();
  while (!exitFlag && (getNextFile(fileName) != NULL))
  {
fprintf(stderr,"%s,%d: pack %s\n",__FILE__,__LINE__,String_cString(fileName));
    /* get file info */
    error = files_getInfo(fileName,&fileInfo);
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
  // log ???
      continue;
    }

    /* new file */
    error = archive_newFile(&archiveInfo,
                            &archiveFileInfo,
                            &fileInfo
                           );

    /* write file content */  
    inputHandle = open(String_cString(fileName),O_RDONLY);
    if (inputHandle == -1)
    {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
  // log ???
      continue;
    }
    error = ERROR_NONE;
    do
    {
      n = read(inputHandle,buffer,BUFFER_SIZE);
      if (n > 0)
      {
        error = archive_writeFileData(&archiveFileInfo,buffer,n);
      }
    }
    while ((error == ERROR_NONE) && (n > 0));
    close(inputHandle);
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
  // log ???
    }

    archive_closeFile(&archiveFileInfo);
  }
  String_delete(fileInfo.name);

  /* close archive */
  archive_done(&archiveInfo);

  /* free resources */
  String_delete(fileName);
  free(buffer);
}

/*---------------------------------------------------------------------*/

bool command_create(const char  *fileName,
                    PatternList *includeList,
                    PatternList *excludeList,
                    const char  *tmpDirectory,
                    ulong       _partSize
                   )
{
  assert(fileName != NULL);
  assert(includeList != NULL);
  assert(excludeList != NULL);

  /* initialise variables */
  archiveFileName    = fileName;
  includePatternList = includeList;
  excludePatternList = excludeList;
  partSize           = _partSize;
  if (pthread_mutex_init(&fileNameListLock,NULL) != 0)
  {
    HALT(EXITCODE_FATAL_ERROR,"Cannot initialise filename list lock semaphore!");
  }
  if (pthread_cond_init(&fileNameListNew,NULL) != 0)
  {
    HALT(EXITCODE_FATAL_ERROR,"Cannot initialise filename list new event!");
  }
  List_init(&fileNameList);
  statistics.includedCount = 0;
  statistics.excludedCount = 0;
  statistics.skippedCount  = 0;

  /* start threads */
  collectorDone = FALSE;
  if (pthread_create(&threadCollector,NULL,(void*(*)(void*))collector,NULL) != 0)
  {
    HALT(EXITCODE_FATAL_ERROR,"Cannot initialise collector thread!");
  }
  if (pthread_create(&threadPacker,NULL,(void*(*)(void*))packer,NULL) != 0)
  {
    HALT(EXITCODE_FATAL_ERROR,"Cannot initialise collector thread!");
  }

  /* wait for threads */
  pthread_join(threadCollector,NULL);
  pthread_join(threadPacker,NULL); 

  List_done(&fileNameList,(NodeFreeFunction)freeFileNameNode,NULL);
  pthread_cond_destroy(&fileNameListNew);
  pthread_mutex_destroy(&fileNameListLock);

fprintf(stderr,"%s,%d: included=%ld excluded=%ld\n",__FILE__,__LINE__,statistics.includedCount,statistics.excludedCount);

  return TRUE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
