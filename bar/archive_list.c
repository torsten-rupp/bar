/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/archive_list.c,v $
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
#include "chunks.h"
#include "archive_format.h"
#include "files.h"

#include "archive.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/


/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

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

/*---------------------------------------------------------------------*/

LOCAL void printFileInfo(const FileInfo *fileInfo)
{
  assert(fileInfo != NULL);

printf("%10llu %10llu..%10llu %s\n",
fileInfo->chunkFileEntry.size,
fileInfo->chunkFileData.partOffset,
fileInfo->chunkFileData.partOffset+fileInfo->chunkFileData.partSize,
String_cString(fileInfo->chunkFileEntry.name)
);
}

bool archive_list(FileNameList *fileNameList, PatternList *includeList, PatternList *excludeList)
{
  Errors       error;
  FileNameNode *fileNameNode;
  ArchiveInfo  archiveInfo;
  FileInfo     fileInfo;
  ChunkId      chunkId;

  fileNameNode = fileNameList->head;
  while (fileNameNode != NULL)
  {
    /* open archive */
    error = files_open(&archiveInfo,
                       String_cString(fileNameNode->fileName)
                      );
    if (error != ERROR_NONE)
    {
      printError("Cannot open file '%s' (error: %s)!\n",String_cString(fileNameNode->fileName),strerror(errno));
      return FALSE;
HALT_INTERNAL_ERROR("x");
    }

    /* list contents */
    while (!files_eof(&archiveInfo))
    {
      error = files_readFile(&archiveInfo,&fileInfo);
      if (error != ERROR_NONE)
      {
HALT_INTERNAL_ERROR("x");
      }
      printFileInfo(&fileInfo);
      files_closeFile(&fileInfo);
    }

    /* close archive */
    files_done(&archiveInfo);

    fileNameNode = fileNameNode->next;
  }

  return TRUE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
