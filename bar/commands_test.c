/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_test.c,v $
* $Revision: 1.8 $
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
#include "stringlists.h"

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

bool command_test(StringList  *archiveFileNameList,
                  PatternList *includePatternList,
                  PatternList *excludePatternList,
                  const char  *password
                 )
{
  byte            *archiveBuffer,*fileBuffer;
  String          archiveFileName;
  bool            failFlag;
  Errors          error;
  ArchiveInfo     archiveInfo;
  ArchiveFileInfo archiveFileInfo;
  FileTypes       fileType;

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
  archiveFileName = String_new();

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
      printError("Cannot open archive file '%s' (error: %s)!\n",
                 String_cString(archiveFileName),
                 getErrorText(error)
                );
      failFlag = TRUE;
      continue;
    }

    /* read files */
    while (!Archive_eof(&archiveInfo) && !failFlag)
    {
      /* get next file type */
      error = Archive_getNextFileType(&archiveInfo,
                                      &archiveFileInfo,
                                      &fileType
                                     );
      if (error != ERROR_NONE)
      {
        printError("Cannot not read content of archive '%s' (error: %s)!\n",
                   String_cString(archiveFileName),
                   getErrorText(error)
                  );
        failFlag = TRUE;
        break;
      }

      switch (fileType)
      {
        case FILETYPE_FILE:
          {
            String     fileName;
            FileInfo   fileInfo;
            uint64     partOffset,partSize;
//            FileInfo   localFileInfo;
            FileHandle fileHandle;
            bool       equalFlag;
            uint64     length;
            ulong      n;
            ulong      readBytes;
            ulong      diffIndex;

            /* get next file from archive */
            fileName = String_new();
            error = Archive_readFileEntry(&archiveInfo,
                                          &archiveFileInfo,
                                          NULL,
                                          NULL,
                                          fileName,
                                          &fileInfo,
                                          &partOffset,
                                          &partSize
                                         );
            if (error != ERROR_NONE)
            {
              printError("Cannot not read content of archive '%s' (error: %s)!\n",
                         String_cString(archiveFileName),
                         getErrorText(error)
                        );
              String_delete(fileName);
              failFlag = TRUE;
              break;
            }

            if (   (Lists_empty(includePatternList) || Patterns_matchList(includePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !Patterns_matchList(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              info(0,"Test '%s'...",String_cString(fileName));

              /* open file */
              error = Files_open(&fileHandle,fileName,FILE_OPENMODE_READ);
              if (error != ERROR_NONE)
              {
                info(0,"fail\n");
                printError("Cannot open file '%s' (error: %s)\n",
                           String_cString(fileName),
                           getErrorText(error)
                          );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                failFlag = TRUE;
                continue;
              }

              /* compare file size */
              if (fileInfo.size != Files_getSize(&fileHandle))
              {
                info(0,"differ in size: expected %lld bytes, found %lld bytes\n",
                     fileInfo.size,
                     Files_getSize(&fileHandle)
                    );
                Files_close(&fileHandle);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                failFlag = TRUE;
                continue;
              }

              /* compare file content */
              error = Files_seek(&fileHandle,partOffset);
              if (error != ERROR_NONE)
              {
                info(0,"fail\n");
                printError("Cannot read file '%s' (error: %s)\n",
                           String_cString(fileName),
                           getErrorText(error)
                          );
                Files_close(&fileHandle);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                failFlag = TRUE;
                continue;
              }
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
                  printError("Cannot not read content of archive '%s' (error: %s)!\n",
                             String_cString(archiveFileName),
                             getErrorText(error)
                            );
                  failFlag = TRUE;
                  break;
                }
                error = Files_read(&fileHandle,fileBuffer,n,&readBytes);
                if (error != ERROR_NONE)
                {
                  info(0,"fail\n");
                  printError("Cannot read file '%s' (error: %s)\n",
                             String_cString(fileName),
                             getErrorText(error)
                            );
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
              Files_close(&fileHandle);
              if (failFlag)
              {
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                continue;
              }

#if 0
              /* get local file info */
              /* check file time, permissions, file owner/group */
#endif /* 0 */
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

              /* free resources */
            }
            else
            {
              /* skip */
              info(1,"Test '%s'...skipped\n",String_cString(fileName));
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(fileName);
          }
          break;
        case FILETYPE_DIRECTORY:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
          break;
        case FILETYPE_LINK:
          {
            String   linkName;
            String   fileName;
            FileInfo fileInfo;
            String   localFileName;
//            FileInfo localFileInfo;

            /* open archive link */
            linkName = String_new();
            fileName = String_new();
            error = Archive_readLinkEntry(&archiveInfo,
                                          &archiveFileInfo,
                                          NULL,
                                          linkName,
                                          fileName,
                                          &fileInfo
                                         );
            if (error != ERROR_NONE)
            {
              printError("Cannot not read content of archive '%s' (error: %s)!\n",
                         String_cString(archiveFileName),
                         getErrorText(error)
                        );
              String_delete(fileName);
              String_delete(linkName);
              failFlag = TRUE;
              break;
            }

            if (   (Lists_empty(includePatternList) || Patterns_matchList(includePatternList,linkName,PATTERN_MATCH_MODE_EXACT))
                && !Patterns_matchList(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              info(0,"Test '%s'...",String_cString(linkName));

              /* check link */
              if (!Files_exist(linkName))
              {
                info(0,"Link '%s' does not exists!\n",
                     String_cString(linkName)
                    );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                failFlag = TRUE;
                break;
              }
              if (Files_getType(linkName) != FILETYPE_LINK)
              {
                info(0,"File is not a link '%s'!\n",
                     String_cString(linkName)
                    );
                failFlag = TRUE;
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                failFlag = TRUE;
                break;
              }

              /* check link content */
              localFileName = String_new();
              error = Files_readLink(linkName,localFileName);
              if (error != ERROR_NONE)
              {
                printError("Cannot not read local file '%s' (error: %s)!\n",
                           String_cString(linkName),
                           getErrorText(error)
                          );
                String_delete(localFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                failFlag = TRUE;
                break;
              }
              if (!String_equals(fileName,localFileName))
              {
                info(0,"Link '%s' does not contain file '%s'!\n",
                     String_cString(linkName),
                     String_cString(fileName)
                    );
                String_delete(localFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                failFlag = TRUE;
                break;
              }
              String_delete(localFileName);

#if 0
              /* get local file info */
              error = Files_getFileInfo(linkName,&localFileInfo);
              if (error != ERROR_NONE)
              {
                printError("Cannot not read local file '%s' (error: %s)!\n",
                           String_cString(linkName),
                           getErrorText(error)
                          );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                failFlag = TRUE;
                break;
              }

              /* check file time, permissions, file owner/group */
#endif /* 0 */
              info(0,"ok\n",
                   String_cString(linkName)
                  );

              /* free resources */
            }
            else
            {
              /* skip */
              info(1,"Restore '%s'...skipped\n",String_cString(linkName));
            }

            /* close archive file */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(fileName);
            String_delete(linkName);
          }
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
    }

    /* close archive */
    Archive_close(&archiveInfo);
  }

  /* free resources */
  String_delete(archiveFileName);
  free(fileBuffer);
  free(archiveBuffer);

  return !failFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
