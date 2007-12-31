/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_compare.c,v $
* $Revision: 1.7 $
* $Author: torsten $
* Contents: Backup ARchiver archive compare function
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

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
#include "filefragmentlists.h"

#include "commands_compare.h"

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

Errors Command_compare(StringList  *archiveFileNameList,
                       PatternList *includePatternList,
                       PatternList *excludePatternList,
                       Options     *options
                      )
{
  byte             *archiveBuffer,*fileBuffer;
  FileFragmentList fileFragmentList;
  String           archiveFileName;
  Errors           failError;
  Errors           error;
  ArchiveInfo      archiveInfo;
  ArchiveFileInfo  archiveFileInfo;
  FileTypes        fileType;
  FileFragmentNode *fileFragmentNode;

  assert(archiveFileNameList != NULL);
  assert(includePatternList != NULL);
  assert(excludePatternList != NULL);
  assert(options != NULL);

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
  FileFragmentList_init(&fileFragmentList);
  archiveFileName = String_new();

  failError = ERROR_NONE;
  while (   !StringList_empty(archiveFileNameList)
         && (failError == ERROR_NONE)
        )
  {
    StringList_getFirst(archiveFileNameList,archiveFileName);
    printInfo(1,"Comparing archive '%s':\n",String_cString(archiveFileName));

    /* open archive */
    error = Archive_open(&archiveInfo,
                         archiveFileName,
                         options
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot open archive file '%s' (error: %s)!\n",
                 String_cString(archiveFileName),
                 getErrorText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      continue;
    }

    /* read files */
    while (   !Archive_eof(&archiveInfo)
           && (failError == ERROR_NONE)
          )
    {
      /* get next file type */
      error = Archive_getNextFileType(&archiveInfo,
                                      &archiveFileInfo,
                                      &fileType
                                     );
      if (error != ERROR_NONE)
      {
        printError("Cannot not read next entry in archive '%s' (error: %s)!\n",
                   String_cString(archiveFileName),
                   getErrorText(error)
                  );
        if (failError == ERROR_NONE) failError = error;
        break;
      }

      switch (fileType)
      {
        case FILE_TYPE_FILE:
          {
            String           fileName;
            FileInfo         fileInfo;
            uint64           fragmentOffset,fragmentSize;
            FileFragmentNode *fileFragmentNode;
//            FileInfo   localFileInfo;
            FileHandle       fileHandle;
            bool             equalFlag;
            uint64           length;
            ulong            n;
            ulong            diffIndex;

            /* read file */
            fileName = String_new();
            error = Archive_readFileEntry(&archiveInfo,
                                          &archiveFileInfo,
                                          NULL,
                                          NULL,
                                          fileName,
                                          &fileInfo,
                                          &fragmentOffset,
                                          &fragmentSize
                                         );
            if (error != ERROR_NONE)
            {
              printError("Cannot not read 'file' content of archive '%s' (error: %s)!\n",
                         String_cString(archiveFileName),
                         getErrorText(error)
                        );
              String_delete(fileName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            if (   (List_empty(includePatternList) || Pattern_matchList(includePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !Pattern_matchList(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(2,"  Compare file '%s'...",String_cString(fileName));

              /* check file */
              if (!File_exists(fileName))
              {
                printInfo(2,"FAIL!\n");
                printError("File '%s' not found!\n",String_cString(fileName));
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (options->stopOnErrorFlag)
                {
                  failError = ERROR_FILE_NOT_FOUND;
                }
                break;
              }
              if (File_getType(fileName) != FILE_TYPE_FILE)
              {
                printInfo(2,"FAIL!\n");
                printError("'%s' is not a file!\n",String_cString(fileName));
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (options->stopOnErrorFlag)
                {
                  failError = ERROR_WRONG_FILE_TYPE;
                }
                break;
              }

              /* get file fragment list */
              fileFragmentNode = FileFragmentList_findFile(&fileFragmentList,fileName);
              if (fileFragmentNode == NULL)
              {
                fileFragmentNode = FileFragmentList_addFile(&fileFragmentList,fileName,fileInfo.size);
              }
//FileFragmentList_print(fileFragmentNode,String_cString(fileName));

              /* open file */
              error = File_open(&fileHandle,fileName,FILE_OPENMODE_READ);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot open file '%s' (error: %s)\n",
                           String_cString(fileName),
                           getErrorText(error)
                          );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (options->stopOnErrorFlag)
                {
                  failError = error;
                }
                continue;
              }

              /* check file size */
              if (fileInfo.size != File_getSize(&fileHandle))
              {
                printInfo(2,"FAIL!\n");
                printError("'%s' differ in size: expected %lld bytes, found %lld bytes\n",
                           String_cString(fileName),
                           fileInfo.size,
                           File_getSize(&fileHandle)
                          );
                File_close(&fileHandle);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (options->stopOnErrorFlag)
                {
                  failError = ERROR_FILES_DIFFER;
                }
                continue;
              }

              /* check file content */
              error = File_seek(&fileHandle,fragmentOffset);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot read file '%s' (error: %s)\n",
                           String_cString(fileName),
                           getErrorText(error)
                          );
                File_close(&fileHandle);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (options->stopOnErrorFlag)
                {
                  failError = error;
                }
                continue;
              }
              length    = 0;
              equalFlag = TRUE;
              diffIndex = 0;
              while ((length < fragmentSize) && equalFlag)
              {
                n = MIN(fragmentSize-length,BUFFER_SIZE);

                /* read archive, file */
                error = Archive_readFileData(&archiveFileInfo,archiveBuffer,n);
                if (error != ERROR_NONE)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot not read content of archive '%s' (error: %s)!\n",
                             String_cString(archiveFileName),
                             getErrorText(error)
                            );
                  if (failError == ERROR_NONE) failError = error;
                  break;
                }
                error = File_read(&fileHandle,fileBuffer,n,NULL);
                if (error != ERROR_NONE)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot read file '%s' (error: %s)\n",
                             String_cString(fileName),
                             getErrorText(error)
                            );
                  if (options->stopOnErrorFlag)
                  {
                    failError = error;
                  }
                  break;
                }

                /* compare */
                diffIndex = compare(archiveBuffer,fileBuffer,n);
                equalFlag = (diffIndex >= n);
                if (!equalFlag)
                {
                  printInfo(2,"FAIL!\n");
                  printError("'%s' differ at offset %llu\n",
                             String_cString(fileName),
                             fragmentOffset+length+diffIndex
                            );
                  if (options->stopOnErrorFlag)
                  {
                    failError = ERROR_FILES_DIFFER;
                  }
                  break;
                }

                length += n;
              }
              File_close(&fileHandle);
              if (failError != ERROR_NONE)
              {
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                continue;
              }

#if 0
              /* get local file info */
              /* check file time, permissions, file owner/group */
#endif /* 0 */
              printInfo(2,"ok\n");

              /* add fragment to file fragment list */
              FileFragmentList_add(fileFragmentNode,fragmentOffset,fragmentSize);

              /* discard fragment list if file is complete */
              if (FileFragmentList_checkComplete(fileFragmentNode))
              {
                FileFragmentList_removeFile(&fileFragmentList,fileFragmentNode);
              }

              /* free resources */
            }
            else
            {
              /* skip */
              printInfo(3,"  Compare '%s'...skipped\n",String_cString(fileName));
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(fileName);
          }
          break;
        case FILE_TYPE_DIRECTORY:
          {
            String   directoryName;
            FileInfo fileInfo;
//            String   localFileName;
//            FileInfo localFileInfo;

            /* read directory */
            directoryName = String_new();
            error = Archive_readDirectoryEntry(&archiveInfo,
                                               &archiveFileInfo,
                                               NULL,
                                               directoryName,
                                               &fileInfo
                                              );
            if (error != ERROR_NONE)
            {
              printError("Cannot not read 'directory' content of archive '%s' (error: %s)!\n",
                         String_cString(archiveFileName),
                         getErrorText(error)
                        );
              String_delete(directoryName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            if (   (List_empty(includePatternList) || Pattern_matchList(includePatternList,directoryName,PATTERN_MATCH_MODE_EXACT))
                && !Pattern_matchList(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(2,"  Compare directory '%s'...",String_cString(directoryName));

              /* check directory */
              if (!File_exists(directoryName))
              {
                printInfo(2,"FAIL!\n");
                printError("Directory '%s' does not exists!\n",String_cString(directoryName));
                Archive_closeEntry(&archiveFileInfo);
                String_delete(directoryName);
                if (options->stopOnErrorFlag)
                {
                  failError = ERROR_FILE_NOT_FOUND;
                }
                break;
              }
              if (File_getType(directoryName) != FILE_TYPE_DIRECTORY)
              {
                printInfo(2,"FAIL!\n");
                printError("'%s' is not a directory!\n",
                           String_cString(directoryName)
                          );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(directoryName);
                if (options->stopOnErrorFlag)
                {
                  failError = ERROR_WRONG_FILE_TYPE;
                }
                break;
              }

#if 0
              /* get local file info */
              error = File_getFileInfo(directoryName,&localFileInfo);
              if (error != ERROR_NONE)
              {
                printError("Cannot not read local directory '%s' (error: %s)!\n",
                           String_cString(directoryName),
                           getErrorText(error)
                          );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(directoryName);
                if (failError == ERROR_NONE) failError = error;
                break;
              }

              /* check file time, permissions, file owner/group */
#endif /* 0 */
              printInfo(2,"ok\n");

              /* free resources */
            }
            else
            {
              /* skip */
              printInfo(3,"  Compare '%s'...skipped\n",String_cString(directoryName));
            }

            /* close archive file */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(directoryName);
          }
          break;
        case FILE_TYPE_LINK:
          {
            String   linkName;
            String   fileName;
            FileInfo fileInfo;
            String   localFileName;
//            FileInfo localFileInfo;

            /* read link */
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
              printError("Cannot not read 'link' content of archive '%s' (error: %s)!\n",
                         String_cString(archiveFileName),
                         getErrorText(error)
                        );
              String_delete(fileName);
              String_delete(linkName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            if (   (List_empty(includePatternList) || Pattern_matchList(includePatternList,linkName,PATTERN_MATCH_MODE_EXACT))
                && !Pattern_matchList(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(2,"  Compare link '%s'...",String_cString(linkName));

              /* check link */
              if (!File_exists(linkName))
              {
                printInfo(2,"FAIL!\n");
                printError("Link '%s' -> '%s' does not exists!\n",
                           String_cString(linkName),
                           String_cString(fileName)
                          );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (options->stopOnErrorFlag)
                {
                  failError = ERROR_FILE_NOT_FOUND;
                }
                break;
              }
              if (File_getType(linkName) != FILE_TYPE_LINK)
              {
                printInfo(2,"FAIL!\n");
                printError("'%s' is not a link!\n",
                           String_cString(linkName)
                          );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (options->stopOnErrorFlag)
                {
                  failError = ERROR_WRONG_FILE_TYPE;
                }
                break;
              }

              /* check link content */
              localFileName = String_new();
              error = File_readLink(linkName,localFileName);
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
                if (options->stopOnErrorFlag)
                {
                  failError = error;
                }
                break;
              }
              if (!String_equals(fileName,localFileName))
              {
                printInfo(2,"FAIL!\n");
                printError("Link '%s' does not contain file '%s'!\n",
                           String_cString(linkName),
                           String_cString(fileName)
                          );
                String_delete(localFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (options->stopOnErrorFlag)
                {
                  failError = ERROR_FILES_DIFFER;
                }
                break;
              }
              String_delete(localFileName);

#if 0
              /* get local file info */
              error = File_getFileInfo(linkName,&localFileInfo);
              if (error != ERROR_NONE)
              {
                printError("Cannot not read local file '%s' (error: %s)!\n",
                           String_cString(linkName),
                           getErrorText(error)
                          );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (failError == ERROR_NONE) failError = error;
                break;
              }

              /* check file time, permissions, file owner/group */
#endif /* 0 */
              printInfo(2,"ok\n");

              /* free resources */
            }
            else
            {
              /* skip */
              printInfo(3,"  Compare '%s'...skipped\n",String_cString(linkName));
            }

            /* close archive file */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(fileName);
            String_delete(linkName);
          }
          break;
        case FILE_TYPE_SPECIAL:
          {
            String   fileName;
            FileInfo fileInfo;
            FileInfo localFileInfo;

            /* read special */
            fileName = String_new();
            error = Archive_readSpecialEntry(&archiveInfo,
                                             &archiveFileInfo,
                                             NULL,
                                             fileName,
                                             &fileInfo
                                            );
            if (error != ERROR_NONE)
            {
              printError("Cannot not read 'special' content of archive '%s' (error: %s)!\n",
                         String_cString(archiveFileName),
                         getErrorText(error)
                        );
              String_delete(fileName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            if (   (List_empty(includePatternList) || Pattern_matchList(includePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !Pattern_matchList(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(2,"  Compare special device '%s'...",String_cString(fileName));

              /* check special device */
              if (!File_exists(fileName))
              {
                printInfo(2,"FAIL!\n");
                printError("Special device '%s' does not exists!\n",
                           String_cString(fileName)
                          );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (options->stopOnErrorFlag)
                {
                  failError = ERROR_FILE_NOT_FOUND;
                }
                break;
              }
              if (File_getType(fileName) != FILE_TYPE_SPECIAL)
              {
                printInfo(2,"FAIL!\n");
                printError("'%s' is not a special device!\n",
                           String_cString(fileName)
                          );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (options->stopOnErrorFlag)
                {
                  failError = ERROR_WRONG_FILE_TYPE;
                }
                break;
              }

              /* check special settings */
              error = File_getFileInfo(fileName,&localFileInfo);
              if (error != ERROR_NONE)
              {
                printError("Cannot not read local file '%s' (error: %s)!\n",
                           String_cString(fileName),
                           getErrorText(error)
                          );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (options->stopOnErrorFlag)
                {
                  failError = error;
                }
                break;
              }
              if (fileInfo.specialType != localFileInfo.specialType)
              {
                printError("Different types of special device '%s'!\n",
                           String_cString(fileName)
                          );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (options->stopOnErrorFlag)
                {
                  failError = error;
                }
                break;
              }
              if (   (fileInfo.specialType == FILE_SPECIAL_TYPE_CHARACTER_DEVICE)
                  || (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                 )
              {
                if (fileInfo.major != localFileInfo.major)
                {
                  printError("Different major numbers of special device '%s'!\n",
                             String_cString(fileName)
                            );
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(fileName);
                  if (options->stopOnErrorFlag)
                  {
                    failError = error;
                  }
                  break;
                }
                if (fileInfo.minor != localFileInfo.minor)
                {
                  printError("Different minor numbers of special device '%s'!\n",
                             String_cString(fileName)
                            );
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(fileName);
                  if (options->stopOnErrorFlag)
                  {
                    failError = error;
                  }
                  break;
                }
              }

#if 0

              /* check file time, permissions, file owner/group */
#endif /* 0 */

              printInfo(2,"ok\n");

              /* free resources */
            }
            else
            {
              /* skip */
              printInfo(3,"  Compare '%s'...skipped\n",String_cString(fileName));
            }

            /* close archive file */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(fileName);
          }
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break; /* not reached */
      }
    }

    /* close archive */
    Archive_close(&archiveInfo);
  }

  /* check fragment lists */
  for (fileFragmentNode = fileFragmentList.head; fileFragmentNode != NULL; fileFragmentNode = fileFragmentNode->next)
  {
    if (!FileFragmentList_checkComplete(fileFragmentNode))
    {
      printInfo(0,"Warning: incomplete file '%s'\n",String_cString(fileFragmentNode->fileName));
      if (failError == ERROR_NONE) failError = ERROR_FILE_INCOMPLETE;
    }
  }

  /* free resources */
  String_delete(archiveFileName);
  FileFragmentList_done(&fileFragmentList);
  free(fileBuffer);
  free(archiveBuffer);

  return failError;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
