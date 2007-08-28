/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_list.c,v $
* $Revision: 1.6 $
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
#include "errors.h"
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

LOCAL void printFileInfo(const String   fileName,
                         const FileInfo *fileInfo,
                         uint64         partOffset,
                         uint64         partSize,
                         uint64         fileSize
                        )
{
  double ratio;

  assert(fileInfo != NULL);

  if (partSize > 0)
  {
    ratio = 100.0-fileSize*100.0/partSize;
  }
  else
  {
    ratio = 0;
  }

  printf("%10llu %10llu..%10llu %-10s %6.1f%% %-10s %s\n",
         fileInfo->size,
         partOffset,
         (partSize > 0)?partOffset+partSize-1:partOffset,
         Compress_getAlgorithmName(fileInfo->compressAlgorithm),
         ratio,
         Crypt_getAlgorithmName(fileInfo->cryptAlgorithm),
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
  String          fileName;
  bool            failFlag;
  ulong           fileCount;
  Errors          error;
  FileNameNode    *archiveFileNameNode;
  ArchiveInfo     archiveInfo;
  ArchiveFileInfo archiveFileInfo;
  FileInfo        fileInfo;
  uint64          partOffset,partSize;

  assert(archiveFileNameList != NULL);
  assert(includePatternList != NULL);
  assert(excludePatternList != NULL);

  fileName = String_new();

  failFlag  = FALSE;
  fileCount = 0;
  info(0,
       "%-10s %-22s %-10s %-7s %-10s %s\n",
       "Size",
       "Part",
       "Compress",
       "Ratio %",
       "Crypt",
       "Name"
      );
  info(0,"--------------------------------------------------------------------------------\n");
  for (archiveFileNameNode = archiveFileNameList->head; archiveFileNameNode != NULL; archiveFileNameNode = archiveFileNameNode->next)
  {
    /* open archive */
    error = Archive_open(&archiveInfo,
                         String_cString(archiveFileNameNode->fileName),
                         password
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot open file '%s' (error: %s)!\n",String_cString(archiveFileNameNode->fileName),getErrorText(error));
      failFlag = TRUE;
      continue;
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
        printError("Cannot not read content of archive '%s' (error: %s)!\n",String_cString(archiveFileNameNode->fileName),getErrorText(error));
        failFlag = TRUE;
        break;
      }

      if (   (Lists_empty(includePatternList) || Patterns_matchList(includePatternList,fileName))
          && !Patterns_matchList(excludePatternList,fileName)
         )
      {
        /* output file info */
        printFileInfo(fileName,
                      &fileInfo,
                      partOffset,
                      partSize,
                      archiveFileInfo.chunkInfoFileData.size
                     );
        fileCount++;
      }

      /* close archive file */
      Archive_closeFile(&archiveFileInfo);
    }

    /* close archive */
    Archive_done(&archiveInfo);
  }
  info(0,"--------------------------------------------------------------------------------\n");
  info(0,"%lu file(s)\n",fileCount);

  /* free resources */
  String_delete(fileName);

  return !failFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
