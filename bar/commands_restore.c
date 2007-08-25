/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_restore.c,v $
* $Revision: 1.5 $
* $Author: torsten $
* Contents: Backup ARchiver archive restore function
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

#include "errors.h"
#include "patterns.h"
#include "files.h"
#include "archive.h"

#include "command_restore.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/*---------------------------------------------------------------------*/

bool command_restore(FileNameList *archiveFileNameList,
                     PatternList  *includePatternList,
                     PatternList  *excludePatternList,
                     uint         directoryStripCount,
                     const char   *directory,
                     const char   *password
                    )
{
  void            *buffer;
  String          fileName;
  bool            failFlag;
  Errors          error;
  FileNameNode    *archiveFileNameNode;
  ArchiveInfo     archiveInfo;
  ArchiveFileInfo archiveFileInfo;
  FileInfo        fileInfo;
  uint64          partOffset,partSize;
  String          destinationFileName;
  FileHandle      fileHandle;
  uint64          length;
  ulong           n;

  assert(archiveFileNameList != NULL);
  assert(includePatternList != NULL);
  assert(excludePatternList != NULL);

  /* allocate resources */
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  fileName = String_new();

  failFlag = FALSE;
  for (archiveFileNameNode = archiveFileNameList->head; archiveFileNameNode != NULL; archiveFileNameNode = archiveFileNameNode->next)
  {
    /* open archive */
    error = Archive_open(&archiveInfo,
                         String_cString(archiveFileNameNode->fileName),
                         password
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot open archive file '%s' (error: %s)!\n",String_cString(archiveFileNameNode->fileName),getErrorText(error));
      failFlag = TRUE;
      continue;
    }

    /* read files */
    while (!Archive_eof(&archiveInfo))
    {
      /* get next file from archive */
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
        Archive_closeFile(&archiveFileInfo);
        failFlag = TRUE;
        break;
      }

      if (   (Lists_empty(includePatternList) || Patterns_matchList(includePatternList,fileName))
          && !Patterns_matchList(excludePatternList,fileName)
         )
      {
        info(0,"Restore '%s'...",String_cString(fileName));

        /* get destination filename */
        destinationFileName = String_new();
        if (directory != NULL) String_setCString(destinationFileName,directory);
        if (directoryStripCount > 0)
        {
  HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
        }
        Files_appendFileName(destinationFileName,fileName);

        /* write file */
        error = Files_open(&fileHandle,destinationFileName,FILE_OPENMODE_WRITE);
        if (error != ERROR_NONE)
        {
          info(0,"fail\n");
          printf("Cannot open file '%s' (error: %s)\n",String_cString(fileName),getErrorText(error));
          Archive_closeFile(&archiveFileInfo);
          failFlag = TRUE;
          continue;
        }
        error = Files_seek(&fileHandle,partOffset);
        if (error != ERROR_NONE)
        {
          info(0,"fail\n");
          printf("Cannot write file '%s' (error: %s)\n",String_cString(fileName),getErrorText(error));
          Archive_closeFile(&archiveFileInfo);
          Files_close(&fileHandle);
          failFlag = TRUE;
          continue;
        }

        length = 0;
        while (length < partSize)
        {
          n = ((partSize-length) > BUFFER_SIZE)?BUFFER_SIZE:partSize-length;

          error = Archive_readFileData(&archiveFileInfo,buffer,n);
          if (error != ERROR_NONE)
          {
            info(0,"fail\n");
            printError("Cannot not read content of archive '%s' (error: %s)!\n",String_cString(archiveFileNameNode->fileName),getErrorText(error));
            failFlag = TRUE;
            break;
          }
          error = Files_write(&fileHandle,buffer,n);
          if (error != ERROR_NONE)
          {
            info(0,"fail\n");
            printf("Cannot write file '%s' (error: %s)\n",String_cString(fileName),getErrorText(error));
            failFlag = TRUE;
            break;
          }

          length += n;
        }
        Files_close(&fileHandle);
        if (failFlag)
        {
          Archive_closeFile(&archiveFileInfo);
          Files_close(&fileHandle);
          continue;
        }

        /* set file permissions, file owner/group */

        /* free resources */
        String_delete(destinationFileName);

        info(0,"ok\n");
      }
      else
      {
        /* skip */
        info(1,"Restore '%s'...skipped\n",String_cString(fileName));
      }

      /* close archive file */
      Archive_closeFile(&archiveFileInfo);
    }

    /* close archive */
    Archive_done(&archiveInfo);
  }

  /* free resources */
  String_delete(fileName);
  free(buffer);

  return !failFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
