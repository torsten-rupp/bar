/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_test.c,v $
* $Revision: 1.6 $
* $Author: torsten $
* Contents: Backup ARchiver archive test function
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

#include "command_test.h"

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

/***********************************************************************\
* Name   : compare
* Purpose: compare memory
* Input  : p0,p1  - memory to compare
*          length - size of memory blocks (in bytes)
* Output : -
* Return : number of equal bytes or length of memory blocks are equal
* Notes  : -
\***********************************************************************/

LOCAL ulong compare(const void *p0, const void *p1, ulong length)
{
  const char *b0,*b1;
  ulong      i;

  b0 = (char*)p0;
  b1 = (char*)p1;
  i = 0;
  while (   (i < length)
         && ((*b0) == (*b1))
        )
  {
    i++;
    b0++;
    b1++;
  }

  return i;
}

/*---------------------------------------------------------------------*/

bool command_test(FileNameList *archiveFileNameList,
                  PatternList  *includePatternList,
                  PatternList  *excludePatternList,
                  const char   *password
                 )
{
  byte            *archiveBuffer,*fileBuffer;
  String          fileName;
  bool            failFlag;
  FileNameNode    *archiveFileNameNode;
  Errors          error;
  ArchiveInfo     archiveInfo;
  ArchiveFileInfo archiveFileInfo;
  FileInfo        fileInfo;
  uint64          partOffset,partSize;
  FileHandle      fileHandle;
  bool            equalFlag;
  uint64          length;
  ulong           n;
  ulong           readBytes;
  ulong           diffIndex;

  assert(archiveFileNameList != NULL);
  assert(includePatternList != NULL);
  assert(excludePatternList != NULL);

  /* allocate resources */
  archiveBuffer = (byte*)malloc(BUFFER_SIZE);
  if (archiveBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  fileBuffer = malloc(BUFFER_SIZE);
  if (fileBuffer == NULL)
  {
    free(archiveBuffer);
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
    while (!Archive_eof(&archiveInfo) && !failFlag)
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
        printError("Cannot not read content of archive '%s' (error: %s)!\n",String_cString(archiveFileNameNode->fileName),getErrorText(error));
        Archive_closeFile(&archiveFileInfo);
        failFlag = TRUE;
        break;
      }

      if (   (Lists_empty(includePatternList) || Patterns_matchList(includePatternList,fileName))
          && !Patterns_matchList(excludePatternList,fileName)
         )
      {
        info(0,"Test '%s'...",String_cString(fileName));

        /* compare file content */
        error = Files_open(&fileHandle,fileName,FILE_OPENMODE_READ);
        if (error != ERROR_NONE)
        {
          info(0,"fail\n");
          printError("Cannot open file '%s' (error: %s)\n",String_cString(fileName),getErrorText(error));
          Archive_closeFile(&archiveFileInfo);
          failFlag = TRUE;
          continue;
        }
        error = Files_seek(&fileHandle,partOffset);
        if (error != ERROR_NONE)
        {
          info(0,"fail\n");
          printError("Cannot read file '%s' (error: %s)\n",String_cString(fileName),getErrorText(error));
          Archive_closeFile(&archiveFileInfo);
          Files_close(&fileHandle);
          failFlag = TRUE;
          continue;
        }

        if (fileInfo.size == Files_size(&fileHandle))
        {
          length    = 0;
          equalFlag = TRUE;
          while ((length < partSize) && equalFlag)
          {
            n = ((partSize-length) > BUFFER_SIZE)?BUFFER_SIZE:partSize-length;

            /* read archive, file */
            error = Archive_readFileData(&archiveFileInfo,archiveBuffer,n);
            if (error != ERROR_NONE)
            {
              info(0,"fail\n");
              printError("Cannot not read content of archive '%s' (error: %s)!\n",String_cString(archiveFileNameNode->fileName),getErrorText(error));
              failFlag = TRUE;
              break;
            }
            error = Files_read(&fileHandle,fileBuffer,n,&readBytes);
            if (error != ERROR_NONE)
            {
              info(0,"fail\n");
              printError("Cannot read file '%s' (error: %s)\n",String_cString(fileName),getErrorText(error));
              failFlag = TRUE;
              break;
            }
            if (n != readBytes)
            {
              equalFlag = FALSE;
              break;
            }

            /* compare */
            diffIndex = compare(archiveBuffer,fileBuffer,n);
            equalFlag = (diffIndex >= n);
            if (!equalFlag)
            {
              break;
            }

            length += n;
          }
          if (failFlag)
          {
            Archive_closeFile(&archiveFileInfo);
            Files_close(&fileHandle);
            continue;
          }

          if (equalFlag)
          {
            info(0,"ok\n",
                 String_cString(fileName)
                );
          }
          else
          {
            info(0,"differ at offset %lld\n",
                 String_cString(fileName),
                 partOffset+length+diffIndex
                );
            failFlag = TRUE;
          }
        }
        else
        {
          info(0,"differ in size: expected %lld bytes, found %lld bytes\n",
               String_cString(fileName),
               fileInfo.size,
               Files_size(&fileHandle)
              );
          failFlag = TRUE;
        }
        Files_close(&fileHandle);
      }
      else
      {
        /* skip */
        info(1,"Test '%s'...skipped\n",String_cString(fileName));
      }

      /* close archive file */
      Archive_closeFile(&archiveFileInfo);
    }

    /* close archive */
    Archive_done(&archiveInfo);
  }

  /* free resources */
  String_delete(fileName);
  free(fileBuffer);
  free(archiveBuffer);

  return !failFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
