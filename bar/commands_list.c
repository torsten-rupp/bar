/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_list.c,v $
* $Revision: 1.1 $
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

#include "command_list.h"

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

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : printFileInfo
* Purpose: print file information
* Input  : archiveFileInfo - file info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printFileInfo(const ArchiveFileInfo *archiveFileInfo)
{
  assert(archiveFileInfo != NULL);

printf("%10llu %10llu..%10llu %s\n",
archiveFileInfo->chunkFileEntry.size,
archiveFileInfo->chunkFileData.partOffset,
archiveFileInfo->chunkFileData.partOffset+archiveFileInfo->chunkFileData.partSize-1,
String_cString(archiveFileInfo->chunkFileEntry.name)
);
}

/*---------------------------------------------------------------------*/

bool command_list(FileNameList *fileNameList,
                  PatternList  *includeList,
                  PatternList  *excludeList)
{
  Errors          error;
  FileNameNode    *fileNameNode;
  ArchiveInfo     archiveInfo;
  ArchiveFileInfo archiveFileInfo;
  FileInfo        fileInfo;
  uint64          partOffset,partSize;

  assert(fileNameList != NULL);
  assert(includeList != NULL);
  assert(excludeList != NULL);

  fileInfo.name = String_new();
 
  fileNameNode = fileNameList->head;
  while (fileNameNode != NULL)
  {
    /* open archive */
    error = archive_open(&archiveInfo,
                         String_cString(fileNameNode->fileName)
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot open file '%s' (error: %s)!\n",String_cString(fileNameNode->fileName),strerror(errno));
      return FALSE;
HALT_INTERNAL_ERROR("x");
    }

    /* list contents */
    while (!archive_eof(&archiveInfo))
    {
      error = archive_readFile(&archiveInfo,
                               &archiveFileInfo,
                               &fileInfo,
                               &partOffset,
                               &partSize
                              );
      if (error != ERROR_NONE)
      {
HALT_INTERNAL_ERROR("x");
      }
      printFileInfo(&archiveFileInfo);
      archive_closeFile(&archiveFileInfo);
    }

    /* close archive */
    archive_done(&archiveInfo);

    fileNameNode = fileNameNode->next;
  }

  String_delete(fileInfo.name);

  return TRUE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
