/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_list.c,v $
* $Revision: 1.4 $
* $Author: torsten $
* Contents: Backup ARchiver archive list function
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
#include "patterns.h"
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

LOCAL void printFileInfo(const String fileName, const FileInfo *fileInfo, uint64 partOffset, uint64 partSize)
{
  assert(fileInfo != NULL);

printf("%10llu %10llu..%10llu %s\n",
fileInfo->size,
partOffset,
(partSize > 0)?partOffset+partSize-1:partOffset,
String_cString(fileName)
);
}

/*---------------------------------------------------------------------*/

bool command_list(FileNameList *archiveFileNameList,
                  PatternList  *includePatternList,
                  PatternList  *excludePatternList,
                  const char   *password
                 )
{
  Errors          error;
  String          fileName;
  FileNameNode    *archiveFileNameNode;
  ArchiveInfo     archiveInfo;
  ArchiveFileInfo archiveFileInfo;
  FileInfo        fileInfo;
  uint64          partOffset,partSize;

  assert(archiveFileNameList != NULL);
  assert(includePatternList != NULL);
  assert(excludePatternList != NULL);

  fileName = String_new();
 
  archiveFileNameNode = archiveFileNameList->head;
  while (archiveFileNameNode != NULL)
  {
    /* open archive */
    error = Archive_open(&archiveInfo,
                         String_cString(archiveFileNameNode->fileName),
                         password
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot open file '%s' (error: %s)!\n",String_cString(archiveFileNameNode->fileName),strerror(errno));
      return FALSE;
HALT_INTERNAL_ERROR("x");
    }

    /* list contents */
    while (!Archive_eof(&archiveInfo))
    {
      /* open archive file */
      error = Archive_readFile(&archiveInfo,
                               &archiveFileInfo,
                               fileName,
                               &fileInfo,
                               &partOffset,
                               &partSize
                              );
      if (error != ERROR_NONE)
      {
HALT_INTERNAL_ERROR("x");
      }

      if (   (Lists_empty(includePatternList) || Patterns_matchList(includePatternList,fileName))
          && !Patterns_matchList(excludePatternList,fileName)
         )
      {
        /* output file info */
        printFileInfo(fileName,&fileInfo,partOffset,partSize);
      }

      /* close archive file */
      Archive_closeFile(&archiveFileInfo);
    }

    /* close archive */
    Archive_done(&archiveInfo);

    /* next file */
    archiveFileNameNode = archiveFileNameNode->next;
  }

  /* free resources */
  String_delete(fileName);

  return TRUE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
