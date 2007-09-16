/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_restore.c,v $
* $Revision: 1.8 $
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
#include "stringlists.h"

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

bool command_restore(StringList  *archiveFileNameList,
                     PatternList *includePatternList,
                     PatternList *excludePatternList,
                     uint        directoryStripCount,
                     const char  *directory,
                     const char  *password
                    )
{
  byte            *buffer;
  String          archiveFileName;
  String          fileName;
  bool            failFlag;
  Errors          error;
  ArchiveInfo     archiveInfo;
  ArchiveFileInfo archiveFileInfo;
  FileInfo        fileInfo;
  uint64          partOffset,partSize;
  String          destinationFileName;
  StringTokenizer fileNameTokenizer;
  String          pathName,baseName,name;
  int             z;
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
  archiveFileName = String_new();
  fileName = String_new();

  failFlag = FALSE;
  while (!StringLists_empty(archiveFileNameList))
  {
    StringLists_getFirst(archiveFileNameList,archiveFileName);

    /* open archive */
    error = Archive_open(&archiveInfo,
                         String_cString(archiveFileName),
                         password
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot open archive file '%s' (error: %s)!\n",String_cString(archiveFileName),getErrorText(error));
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
                               NULL,
                               NULL,
                               &partOffset,
                               &partSize
                              );
      if (error != ERROR_NONE)
      {
        printError("Cannot not read content of archive '%s' (error: %s)!\n",String_cString(archiveFileName),getErrorText(error));
        Archive_closeFile(&archiveFileInfo);
        failFlag = TRUE;
        break;
      }

      if (   (Lists_empty(includePatternList) || Patterns_matchList(includePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
          && !Patterns_matchList(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
         )
      {
        /* get destination filename */
        destinationFileName = String_new();
        if (directory != NULL) Files_setFileNameCString(destinationFileName,directory);
        Files_splitFileName(fileName,&pathName,&baseName);
        Files_initSplitFileName(&fileNameTokenizer,pathName);
        z = 0;
        while ((z< directoryStripCount) && Files_getNextSplitFileName(&fileNameTokenizer,&name))
        {
          z++;
        }
        while (Files_getNextSplitFileName(&fileNameTokenizer,&name))
        {
          Files_appendFileName(destinationFileName,name);
        }     
        Files_doneSplitFileName(&fileNameTokenizer);
        Files_appendFileName(destinationFileName,baseName);
        String_delete(pathName);
        String_delete(baseName);

        info(0,"Restore '%s'...",String_cString(destinationFileName));

        /* write file */
        if (Files_exist(destinationFileName) && !globalOptions.overwriteFlag)
        {
          info(0,"skipped (file exists)\n");
          Archive_closeFile(&archiveFileInfo);
          String_delete(destinationFileName);
          failFlag = TRUE;
          continue;
        }
        error = Files_open(&fileHandle,destinationFileName,FILE_OPENMODE_WRITE);
        if (error != ERROR_NONE)
        {
          info(0,"fail\n");
          printError("Cannot open file '%s' (error: %s)\n",String_cString(destinationFileName),getErrorText(error));
          Archive_closeFile(&archiveFileInfo);
          String_delete(destinationFileName);
          failFlag = TRUE;
          continue;
        }
        error = Files_seek(&fileHandle,partOffset);
        if (error != ERROR_NONE)
        {
          info(0,"fail\n");
          printError("Cannot write file '%s' (error: %s)\n",String_cString(destinationFileName),getErrorText(error));
          Archive_closeFile(&archiveFileInfo);
          Files_close(&fileHandle);
          String_delete(destinationFileName);
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
            printError("Cannot not read content of archive '%s' (error: %s)!\n",String_cString(archiveFileName),getErrorText(error));
            failFlag = TRUE;
            break;
          }
          error = Files_write(&fileHandle,buffer,n);
          if (error != ERROR_NONE)
          {
            info(0,"fail\n");
            printError("Cannot write file '%s' (error: %s)\n",String_cString(destinationFileName),getErrorText(error));
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
          String_delete(destinationFileName);
          continue;
        }

        /* set file time, permissions, file owner/group */
        error = Files_setFileInfo(destinationFileName,&fileInfo);
        if (error != ERROR_NONE)
        {
          info(0,"fail\n");
          printError("Cannot set file info of '%s' (error: %s)\n",String_cString(destinationFileName),getErrorText(error));
          Archive_closeFile(&archiveFileInfo);
          Files_close(&fileHandle);
          String_delete(destinationFileName);
          failFlag = TRUE;
          continue;
        }

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
    Archive_close(&archiveInfo);
  }

  /* free resources */
  String_delete(fileName);
  String_delete(archiveFileName);
  free(buffer);

  return !failFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
