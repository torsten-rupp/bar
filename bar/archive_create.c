/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/archive_create.c,v $
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
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "bar.h"
#include "files.h"

#include "archive.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define PATHNAME_SEPARATOR_CHAR '/'
#define PATHNAME_SEPARATOR_CHARS "/"

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

LOCAL uint            partNumber;
LOCAL int             outputHandle;
LOCAL ulong           outputSize;

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

#if 0
/***********************************************************************\
* Name   : ensureArchiveFileSpace
* Purpose: create and ensure archive file space
* Input  : fileInfoBlock - file-info block
*          minSize       - min. size
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool ensureArchiveFileSpace(FileInfoBlock *fileInfoBlock, ulong minSize)
{
  off64_t n;
  String  fileName;

  assert(fileInfoBlock != NULL);


  /* check part size, prepare for next part if needed */
  if (fileInfoBlock->handle >= 0)
  {
    n = lseek64(fileInfoBlock->handle,0,SEEK_CUR);
    if (n == (off_t)-1)
    {
      return FALSE;
    }

    if ((fileInfoBlock->partSize > 0) && ((uint64)n + minSize + 1 >= fileInfoBlock->partSize))
    {
      close(fileInfoBlock->handle);
      fileInfoBlock->handle = -1;
    }
  }

  /* open next part */
  if (fileInfoBlock->handle < 0)
  {
    /* get output filename */
    if (fileInfoBlock->partSize > 0)
    {
      fileName = String_format(String_new(),"%s.%06d",archiveFileName,fileInfoBlock->partNumber);
      fileInfoBlock->partNumber++;
    }
    else
    {
      fileName = String_newCString(archiveFileName);
    }

    /* create file */
    fileInfoBlock->handle = open(String_cString(fileName),O_CREAT|O_RDWR|O_TRUNC,0644);
    if (fileInfoBlock->handle == -1)
    {
  //??? log
      return FALSE;
    }

    String_delete(fileName);
  }

  return TRUE;
}

LOCAL bool nextArchiveFile(FileInfoBlock *fileInfoBlock)
{
}

/***********************************************************************\
* Name   : closeArchiveFile
* Purpose: close archive file
* Input  : fileInfoBlock - file-info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void closeArchiveFile(FileInfoBlock *fileInfoBlock)
{
  assert(fileInfoBlock != NULL);

  if (fileInfoBlock->handle >= 0)
  {
    close(fileInfoBlock->handle);
    fileInfoBlock->handle = -1;
  }
}

LOCAL bool nextFile(void *userData)
{
  FileInfoBlock *fileInfoBlock = (FileInfoBlock*)userData;

  assert(fileInfoBlock != NULL);

  /* create new entry */
  if (!chunks_new(&chunkInfoBlock,BAR_CHUNK_ID_FILE))
  {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
// log ???
    return FALSE;
  }

  /* write file info */
  if (!chunks_write(&chunkInfoBlock,&fileInfoBlock.chunkFile,BAR_CHUNK_DEFINITION_FILE))
  {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
// log ???
    return FALSE;
  }

  return TRUE;
}

LOCAL bool closeFile(void *userData)
{
}

LOCAL bool readFile(void *userData, void *buffer, ulong length)
{
  FileInfoBlock *fileInfoBlock = (FileInfoBlock*)userData;

  assert(fileInfoBlock != NULL);

  return (read(fileInfoBlock->handle,buffer,length) == length);
}

LOCAL bool writeFile(void *userData, const void *buffer, ulong length)
{
  ulong n;

  FileInfoBlock *fileInfoBlock = (FileInfoBlock*)userData;

  assert(fileInfoBlock != NULL);

  while (length > 0)
  {
    n = fileInfoBlock->
  }

  return (write(fileInfoBlock->handle,buffer,length) == length);
}

LOCAL bool tellFile(void *userData, uint64 *offset)
{
  FileInfoBlock *fileInfoBlock = (FileInfoBlock*)userData;
  off64_t       n;

  assert(fileInfoBlock != NULL);
  assert(offset != NULL);

  n = lseek64(fileInfoBlock->handle,0,SEEK_CUR);
  if (n == (off_t)-1)
  {
    return FALSE;
  }
  (*offset) = (uint64)n;

  return TRUE;
}

LOCAL bool seekFile(void *userData, uint64 offset)
{
  FileInfoBlock *fileInfoBlock = (FileInfoBlock*)userData;

  assert(fileInfoBlock != NULL);

  if (lseek64(fileInfoBlock->handle,(off64_t)offset,SEEK_SET) == (off_t)-1)
  {
    return FALSE;
  }

  return TRUE;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

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
* Name   : getFileType
* Purpose: get file type
* Input  : fileName - filename
* Output : -
* Return : file type; see FILETYPES_*
* Notes  : -
\***********************************************************************/

LOCAL FileTypes getFileType(String fileName)
{
  struct stat fileInfo;

  if (lstat(String_cString(fileName),&fileInfo) == 0)
  {
    if      (S_ISREG(fileInfo.st_mode)) return FILETYPE_FILE;
    else if (S_ISDIR(fileInfo.st_mode)) return FILETYPE_DIRECTORY;
    else if (S_ISLNK(fileInfo.st_mode)) return FILETYPE_LINK;
    else                                return FILETYPE_UNKNOWN;
  }
  else
  {
    return FILETYPE_UNKNOWN;
  }
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
  DIR             *directoryHandle;
  struct dirent   *directoryEntry;
  FileNameNode    *fileNameNode;
  String          fileName;
  bool            excludeFlag;

  assert(includePatternList != NULL);
  assert(excludePatternList != NULL);

  includePatternNode = includePatternList->head;
  while (!exitFlag && (includePatternNode != NULL))
  {
    /* find base path */
    basePath = String_new();
    String_initTokenizer(&fileNameTokenizer,includePatternNode->pattern,PATHNAME_SEPARATOR_CHARS,NULL);
    s = String_new();
    while (String_getNextToken(&fileNameTokenizer,&s,NULL) && !checkIsPattern(s))
    {
      if (String_length(basePath) > 0) String_appendChar(basePath,PATHNAME_SEPARATOR_CHAR);
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
        switch (getFileType(fileNameNode->fileName))
        {
          case FILETYPE_FILE:
            /* add to file list */
            appendFileNameToList(&fileNameList,fileNameNode->fileName);
            statistics.includedCount++;
//fprintf(stderr,"%s,%d: collect %s\n",__FILE__,__LINE__,String_cString(fileName));
            break;
          case FILETYPE_DIRECTORY:
            /* read directory contents */
            directoryHandle = opendir(String_cString(fileNameNode->fileName));
            if (directoryHandle != NULL)
            {
              while ((directoryEntry = readdir(directoryHandle)) != NULL)
              {
                if ((strcmp(directoryEntry->d_name,".") != 0) && (strcmp(directoryEntry->d_name,"..") != 0))
                {
                  /* get filename */
                  fileName = String_copy(fileNameNode->fileName);
                  String_appendChar(fileName,PATHNAME_SEPARATOR_CHAR);
                  String_appendCString(fileName,directoryEntry->d_name);

                  /* filter excludes */
                  if (!checkIsExcluded(excludePatternList,s))
                  {
                    /* detect file type */
                    switch (getFileType(fileName))
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

                  String_delete(fileName);
                }
              }
              if (errno != 0)
              {
        //??? log
              }
              closedir(directoryHandle);
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
  ArchiveInfo    archiveInfo;
  Errors         error;
  void           *buffer;
  String         fileName;
  struct stat    fileStat;
FileInfo fileInfo;
//  ChunkInfoBlock chunkInfoBlock;
  int            inputHandle;
  ssize_t        n;

  /* initialise new archive */
  error = files_create(&archiveInfo,
                       archiveFileName,
                       partSize
                      );
  if (error != ERROR_NONE)
  {
HALT(1,"x");
  }

  /* allocate file data buffer */
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  fileName = String_new();
  while (!exitFlag && (getNextFile(fileName) != NULL))
  {
fprintf(stderr,"%s,%d: pack %s\n",__FILE__,__LINE__,String_cString(fileName));
    /* get file info */
    if (lstat(String_cString(fileName),&fileStat) != 0)
    {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
  // log ???
      continue;
    }

    /* new file */
    error = files_newFile(&archiveInfo,
                          &fileInfo,
                          fileName,
                          fileStat.st_size,
                          fileStat.st_atime,
                          fileStat.st_mtime,
                          fileStat.st_ctime,
                          fileStat.st_uid,
                          fileStat.st_gid,
                          fileStat.st_mode
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
        error = files_writeFileData(&fileInfo,buffer,n);
      }
    }
    while ((error == ERROR_NONE) && (n > 0));
    close(inputHandle);
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
  // log ???
    }

    files_closeFile(&fileInfo);
  }

  String_delete(fileName);
  files_done(&archiveInfo);
  free(buffer);
}

/*---------------------------------------------------------------------*/

bool archive_create(const char *fileName, PatternList *includeList, PatternList *excludeList, const char *tmpDirectory, ulong size)
{
  /* initialise variables */
  archiveFileName    = fileName;
  includePatternList = includeList;
  excludePatternList = excludeList;
  partSize           = size;
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
