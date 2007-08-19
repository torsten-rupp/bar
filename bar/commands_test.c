/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_test.c,v $
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

bool command_test(FileNameList *archiveFileNameList,
                  PatternList  *includePatternList,
                  PatternList  *excludePatternList,
                  const char   *password
                 )
{
  void            *archiveBuffer,*fileBuffer;
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
  archiveBuffer = malloc(BUFFER_SIZE);
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
  archiveFileNameNode = archiveFileNameList->head;
  while ((archiveFileNameNode != NULL) && !failFlag)
  {
    /* open archive */
    error = Archive_open(&archiveInfo,
                         String_cString(archiveFileNameNode->fileName),
                         password
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot open archive file '%s' (error: %s)!\n",String_cString(archiveFileNameNode->fileName),strerror(errno));
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
                               &partOffset,
                               &partSize
                              );
      if (error != ERROR_NONE)
      {
        printError("Cannot read archive file '%s' (error: %s)!\n",String_cString(archiveFileNameNode->fileName),strerror(errno));
        Archive_closeFile(&archiveFileInfo);
        failFlag = TRUE;
        break;
      }

      if (   (Lists_empty(includePatternList) || Patterns_matchList(includePatternList,fileName))
          && !Patterns_matchList(excludePatternList,fileName)
         )
      {
        /* compare file content */
        error = Files_open(&fileHandle,fileName,FILE_OPENMODE_READ);
        if (error != ERROR_NONE)
        {
          printf("Cannot open file '%s' (error: %s)\n",String_cString(fileName),strerror(errno));
          Archive_closeFile(&archiveFileInfo);
          failFlag = TRUE;
          continue;
        }
        error = Files_seek(&fileHandle,partOffset);
        if (error != ERROR_NONE)
        {
          printf("Cannot read file '%s' (error: %s)\n",String_cString(fileName),strerror(errno));
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
              printf("Cannot archive read file '%s' (error: %s)\n",String_cString(archiveFileNameNode->fileName),strerror(errno));
              break;
            }
            error = Files_read(&fileHandle,fileBuffer,n,&readBytes);
            if (error != ERROR_NONE)
            {
              printf("Cannot read file '%s' (error: %s)\n",String_cString(fileName),strerror(errno));
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
          if (error != ERROR_NONE)
          {
            Archive_closeFile(&archiveFileInfo);
            Files_close(&fileHandle);
            failFlag = TRUE;
            continue;
          }

          if (equalFlag)
          {
            printf("File '%s' ok\n",
                   String_cString(fileName)
                  );
          }
          else
          {
            printf("File '%s' differ at offset %lld\n",
                   String_cString(fileName),
                   partOffset+length+diffIndex
                  );
            failFlag = TRUE;
          }
        }
        else
        {
          printf("File '%s' differ in size: expected %lld bytes, found %lld bytes\n",
                 String_cString(fileName),
                 fileInfo.size,
                 Files_size(&fileHandle)
                );
          failFlag = TRUE;
        }
        Files_close(&fileHandle);
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
  free(fileBuffer);
  free(archiveBuffer);

  return !failFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
