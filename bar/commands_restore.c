/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_restore.c,v $
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

#include "command_restore.h"

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

bool command_restore(FileNameList *fileNameList,
                     PatternList  *includeList,
                     PatternList  *excludeList,
                     const char   *directory
                    )
{
  Errors          error;
  FileNameNode    *fileNameNode;
  ArchiveInfo     archiveInfo;
  ArchiveFileInfo archiveFileInfo;
  FileInfo        fileInfo;
  uint64          partOffset,partSize;
  ulong           length;

  assert(fileNameList != NULL);
  assert(includeList != NULL);
  assert(excludeList != NULL);

  /* initialise variables */
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

    /* read files */
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

      length = 0;
      while (length < partSize)
      {
      }

      archive_closeFile(&archiveFileInfo);
    }

    /* close archive */
    archive_done(&archiveInfo);

    /* next file */
    fileNameNode = fileNameNode->next;
  }

  /* free resources */
  String_delete(fileInfo.name);

  return TRUE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
