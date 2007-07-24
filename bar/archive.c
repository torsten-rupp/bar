/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/archive.c,v $
* $Revision: 1.1.1.1 $
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
#include "chunks.h"
#include "archive_format.h"

#include "archive.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define PATHNAME_SEPARATOR_CHAR '/'
#define PATHNAME_SEPARATOR_CHARS "/"

#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/

typedef enum
{
  FILETYPE_NONE,

  FILETYPE_FILE,
  FILETYPE_LINK,
  FILETYPE_DIRECTORY,

  FILETYPE_UNKNOWN
} FileTypes;

/***************************** Variables *******************************/
LOCAL bool       exitFlag;
LOCAL const char   *archiveFileName;
LOCAL PatternList  *includePatternList;
LOCAL PatternList  *excludePatternList;
LOCAL ulong        partSize;

LOCAL sem_t        fileNameListLock;
LOCAL FileNameList fileNameList;

LOCAL bool         collectorDone;
LOCAL pthread_t    threadCollector;
LOCAL pthread_t    threadPacker;

LOCAL uint       partNumber;
LOCAL int        outputHandle;
LOCAL ulong      outputSize;

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
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool createFile(void)
{
  String fileName;
  char   s[7];

  /* get output filename */
  if (partSize > 0)
  {
    fileName = String_newCString(archiveFileName);
    snprintf(s,6,"%06d",partNumber);
    String_appendChar(fileName,'.');
    String_appendCString(fileName,s);
  }
  else
  {
    fileName = String_newCString(archiveFileName);
  }

  /* create file */
  outputHandle = open(String_cString(fileName),O_CREAT|O_RDWR|O_TRUNC,0644);
  if (outputHandle == -1)
  {
//??? log
    return FALSE;
  }
  outputSize = 0;

  String_delete(fileName);

  return TRUE;
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void closeFile(void)
{
  if (outputHandle >= 0)
  {
    close(outputHandle);
    outputHandle = -1;
  }
}

/***********************************************************************\
* Name   : ensureFileSpace
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool ensureFileSpace(ulong size)
{
  /* check part size, prepare for next part if needed */
  if (outputHandle >= 0)
  {
    if ((partSize > 0) && (outputSize + size + 1 >= partSize))
    {
      closeFile();
      partNumber++;
    }
  }

  /* open next part */
  if (outputHandle < 0)
  {
    if (!createFile())
    {
      return FALSE;
    }
  }

  return TRUE;
}

LOCAL bool readFile(void *userData, void *buffer, ulong length)
{
  return (read((int)userData,buffer,length) == length);
}

LOCAL bool writeFile(void *userData, const void *buffer, ulong length)
{
  return (write((int)userData,buffer,length) == length);
}

LOCAL bool tellFile(void *userData, uint64 *offset)
{
  off64_t n;

  assert(offset != NULL);

  n = lseek64((int)userData,0,SEEK_CUR);
  if (n == (off_t)-1)
  {
    return FALSE;
  }
  (*offset) = (uint64)n;

  return TRUE;
}

LOCAL bool seekFile(void *userData, uint64 offset)
{
  if (lseek64((int)userData,(off64_t)offset,SEEK_SET) == (off_t)-1)
  {
    return FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool readData(ChunkInfoBlock *chunkInfoBlock, void *data, ulong size)
{
  /* read data */
  if (!chunks_readData(chunkInfoBlock,data,size))
  {
    return FALSE;
  }

  /* encrypt */

  /* compress */

  return TRUE;
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool writeData(ChunkInfoBlock *chunkInfoBlock, const void *data, ulong size)
{
  /* compress */

  /* encrypt */

  /* write data */
  if (!chunks_writeData(chunkInfoBlock,data,size))
  {
    return FALSE;
  }

  return TRUE;
}

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
  sem_wait(&fileNameListLock);
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
  sem_post(&fileNameListLock);
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

  fileNameNode = (FileNameNode*)malloc(sizeof(FileNameNode));
  if (fileNameNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  fileNameNode->fileName = String_copy(fileName);

  List_add(fileNameList,fileNameNode);
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
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL String getNextFile(String fileName)
{
  FileNameNode *fileNameNode;

  do
  {
    lockFileNameList();
    fileNameNode = (FileNameNode*)List_getFirst(&fileNameList);
    unlockFileNameList();
  }
  while ((fileNameNode == NULL) && !collectorDone);
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
  PatternNode     *excludePatternNode;
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
      String_appendChar(basePath,PATHNAME_SEPARATOR_CHAR);
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
            excludeFlag = FALSE;
            excludePatternNode = excludePatternList->head;
            while (!exitFlag && (excludePatternNode != NULL) && !excludeFlag)
            {
              /* match with exclude pattern */

              excludePatternNode = excludePatternNode->next;
            }

            if (!excludeFlag)
            {
              /* detect file type */
              switch (getFileType(fileName))
              {
                case FILETYPE_FILE:
                  /* store in file list */
                  lockFileNameList();
                  appendFileNameToList(&fileNameList,fileName);
                  unlockFileNameList();
                  statistics.includedCount++;
//fprintf(stderr,"%s,%d: collect %s\n",__FILE__,__LINE__,String_cString(fileName));
                  break;
                case FILETYPE_DIRECTORY:
                  /* store in directory list */
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
  void           *buffer;
  String         fileName;
  struct stat    fileInfo;
  int            inputHandle;
  ChunkInfoBlock chunkInfoBlock;
  BARChunk_File  chunkFile;
  ssize_t        n;

  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  fileName = String_new();

  /* create archive file */
  if (!createFile())
  {
  // log ???
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
    exitFlag = TRUE;
    return;
  }

  while (!exitFlag && (getNextFile(fileName) != NULL))
  {
fprintf(stderr,"%s,%d: pack %s\n",__FILE__,__LINE__,String_cString(fileName));

    /* get file info */
    if (lstat(String_cString(fileName),&fileInfo) != 0)
    {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
  // log ???
      continue;
    }
    chunkFile.fileType        = 0;
    chunkFile.size            = fileInfo.st_size;
    chunkFile.timeLastAccess  = fileInfo.st_atime;
    chunkFile.timeModified    = fileInfo.st_mtime;
    chunkFile.timeLastChanged = fileInfo.st_ctime;
    chunkFile.userId          = fileInfo.st_uid;
    chunkFile.groupId         = fileInfo.st_gid;
    chunkFile.permission      = fileInfo.st_mode;
    chunkFile.name            = fileName;

    /* init */
    chunks_init(&chunkInfoBlock,readFile,writeFile,tellFile,seekFile,(void*)outputHandle);

    /* make sure chunk can be written in single file */
    if (!ensureFileSpace(sizeof(ChunkHeader) + chunks_getSize(&chunkFile,BAR_CHUNK_DEFINITION_FILE)))
    {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
  // log ???
      continue;
    }

    /* create new entry */
    if (!chunks_new(&chunkInfoBlock,BAR_CHUNK_ID_FILE))
    {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
  // log ???
      continue;
    }

    /* write file info */
    if (!chunks_write(&chunkInfoBlock,&chunkFile,BAR_CHUNK_DEFINITION_FILE))
    {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
  // log ???
      continue;
    }

    /* write file content */
    inputHandle = open(String_cString(fileName),O_RDONLY);
    if (inputHandle == -1)
    {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
  // log ???
      continue;
    }
    do
    {
      n = read(inputHandle,buffer,BUFFER_SIZE);
      if (n > 0)
      {
        if (!chunks_writeData(&chunkInfoBlock,buffer,n))
        {
          n = -1;
        }
      }
    }
    while (n > 0);
    close(inputHandle);
    if (n == -1)
    {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
  // log ???
    }

    /* close entry */
    if (!chunks_close(&chunkInfoBlock))
    {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
  // log ???
    }
  }

  /* close archive file */
  closeFile();

  String_delete(fileName);
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
  partNumber         = 0;
  outputHandle       = -1;
  if (sem_init(&fileNameListLock,0,1) != 0)
  {
    HALT(EXITCODE_FATAL_ERROR,"Cannot initialise filename list lock semaphore!");
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
  sem_destroy(&fileNameListLock);

fprintf(stderr,"%s,%d: included=%ld excluded=%ld\n",__FILE__,__LINE__,statistics.includedCount,statistics.excludedCount);

  return TRUE;
}

/*---------------------------------------------------------------------*/

LOCAL void printFileInfo(const BARChunk_File *chunkFile)
{
}

LOCAL bool listFileChunks(ChunkInfoBlock *containerChunkInfoBlock)
{
  ChunkInfoBlock chunkInfoBlock;

  while (chunks_getSub(&chunkInfoBlock,containerChunkInfoBlock))
  {
    switch (chunkInfoBlock.id)
    {
      case BAR_CHUNK_ID_COMPRESSION:
chunks_skip(&chunkInfoBlock);
        break;
      case BAR_CHUNK_ID_ENCRYPTION:
chunks_skip(&chunkInfoBlock);
        break;
      case BAR_CHUNK_ID_FILE:
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
        break;
      default:
fprintf(stderr,"%s,%d: skip unknown chunk 0x%ux size %llu\n",__FILE__,__LINE__,chunkInfoBlock.id,chunkInfoBlock.size);
        chunks_skip(&chunkInfoBlock);
        break;
    }
  }

  return TRUE;
}

bool archive_list(FileNameList *fileNameList, PatternList *includeList, PatternList *excludeList)
{
  FileNameNode   *fileNameNode;
  int            fileHandle;
  ChunkInfoBlock chunkInfoBlock;
  BARChunk_File  chunkFile;

  fileNameNode = fileNameList->head;
  while (fileNameNode != NULL)
  {
    /* open archive */
    fileHandle = open(String_cString(fileNameNode->fileName),O_RDONLY);
    if (fileHandle == -1)
    {
      printError("Cannot open file '%s' (error: %s)!\n",String_cString(fileNameNode->fileName),strerror(errno));
      return FALSE;
    }
//fprintf(stderr,"%s,%d: 0x%lx 0x%lx\n",__FILE__,__LINE__,
//BAR_CHUNK_ID_BAR,
//BAR_CHUNK_ID_FILE
//);

    /* list contents */
    chunks_init(&chunkInfoBlock,readFile,writeFile,tellFile,seekFile,(void*)fileHandle);
    while (chunks_get(&chunkInfoBlock))
    {
      switch (chunkInfoBlock.id)
      {
        case BAR_CHUNK_ID_BAR:
          listFileChunks(&chunkInfoBlock);
          break;
        case BAR_CHUNK_ID_COMPRESSION:
chunks_skip(&chunkInfoBlock);
          break;
        case BAR_CHUNK_ID_ENCRYPTION:
chunks_skip(&chunkInfoBlock);
          break;
        case BAR_CHUNK_ID_FILE:
          if (chunks_read(&chunkInfoBlock,&chunkFile,BAR_CHUNK_DEFINITION_FILE))
          {
            printf("%10llu %s\n",chunkFile.size,String_cString(chunkFile.name));
            String_delete(chunkFile.name);
          }
          chunks_skip(&chunkInfoBlock);
          break;
        default:
fprintf(stderr,"%s,%d: skip unknown chunk 0x%ux size %lld\n",__FILE__,__LINE__,chunkInfoBlock.id,chunkInfoBlock.size);
          chunks_skip(&chunkInfoBlock);
          break;
      }
    }

    /* close */
    close(fileHandle);

    fileNameNode = fileNameNode->next;
  }

  return TRUE;
}

/*---------------------------------------------------------------------*/

bool archive_test(FileNameList *fileNameList, PatternList *includeList, PatternList *excludeList)
{
  return TRUE;
}

/*---------------------------------------------------------------------*/

bool archive_restore(FileNameList *fileNameList, PatternList *includeList, PatternList *excludeList, const char *directory)
{
  return TRUE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
