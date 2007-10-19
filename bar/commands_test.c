/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_test.c,v $
* $Revision: 1.18 $
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
#include "filefragmentlists.h"

#include "commands_test.h"

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

Errors Command_test(StringList    *archiveFileNameList,
                    PatternList   *includePatternList,
                    PatternList   *excludePatternList,
                    const Options *options
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
  while (!StringList_empty(archiveFileNameList))
  {
    StringList_getFirst(archiveFileNameList,archiveFileName);

    /* open archive */
    error = Archive_open(&archiveInfo,
                         archiveFileName,
                         options,
                         options->cryptPassword
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
    while (!Archive_eof(&archiveInfo) && (failError == ERROR_NONE))
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
        if (failError == ERROR_NONE) failError = error;
        break;
      }

      switch (fileType)
      {
        case FILETYPE_FILE:
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
            ulong            readBytes;
            ulong            diffIndex;

            /* readt file */
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
              printError("Cannot not read content of archive '%s' (error: %s)!\n",
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
              info(0,"Test file '%s'...",String_cString(fileName));

              /* check file */
              if (!File_exists(fileName))
              {
                info(0,"File '%s' does not exists!\n",
                     String_cString(fileName)
                    );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (failError == ERROR_NONE) failError = ERROR_FILE_NOT_FOUND;
                break;
              }
              if (File_getType(fileName) != FILETYPE_FILE)
              {
                info(0,"Not a file '%s'!\n",
                     String_cString(fileName)
                    );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (failError == ERROR_NONE) failError = ERROR_WRONG_FILE_TYPE;
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
                info(0,"fail\n");
                printError("Cannot open file '%s' (error: %s)\n",
                           String_cString(fileName),
                           getErrorText(error)
                          );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (failError == ERROR_NONE) failError = error;
                continue;
              }

              /* check file size */
              if (fileInfo.size != File_getSize(&fileHandle))
              {
                info(0,"differ in size: expected %lld bytes, found %lld bytes\n",
                     fileInfo.size,
                     File_getSize(&fileHandle)
                    );
                File_close(&fileHandle);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (failError == ERROR_NONE) failError = ERROR_FILES_DIFFER;
                continue;
              }

              /* check file content */
              error = File_seek(&fileHandle,fragmentOffset);
              if (error != ERROR_NONE)
              {
                info(0,"fail\n");
                printError("Cannot read file '%s' (error: %s)\n",
                           String_cString(fileName),
                           getErrorText(error)
                          );
                File_close(&fileHandle);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (failError == ERROR_NONE) failError = error;
                continue;
              }
              length    = 0;
              equalFlag = TRUE;
              while ((length < fragmentSize) && equalFlag)
              {
                n = MIN(fragmentSize-length,BUFFER_SIZE);

                /* read archive, file */
                error = Archive_readFileData(&archiveFileInfo,archiveBuffer,n);
                if (error != ERROR_NONE)
                {
                  info(0,"fail\n");
                  printError("Cannot not read content of archive '%s' (error: %s)!\n",
                             String_cString(archiveFileName),
                             getErrorText(error)
                            );
                  if (failError == ERROR_NONE) failError = error;
                  break;
                }
                error = File_read(&fileHandle,fileBuffer,n,&readBytes);
                if (error != ERROR_NONE)
                {
                  info(0,"fail\n");
                  printError("Cannot read file '%s' (error: %s)\n",
                             String_cString(fileName),
                             getErrorText(error)
                            );
                  if (failError == ERROR_NONE) failError = error;
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
              if (equalFlag)
              {
                info(0,"ok\n",
                     String_cString(fileName)
                    );
              }
              else
              {
                info(0,"differ at offset %llu\n",
                     fragmentOffset+length+diffIndex
                    );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (failError == ERROR_NONE) failError = ERROR_FILES_DIFFER;
                continue;
              }

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
              info(1,"Test '%s'...skipped\n",String_cString(fileName));
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(fileName);
          }
          break;
        case FILETYPE_DIRECTORY:
          {
            String   directoryName;
            FileInfo fileInfo;
//            String   localFileName;
//            FileInfo localFileInfo;

            /* read link */
            directoryName = String_new();
            error = Archive_readDirectoryEntry(&archiveInfo,
                                               &archiveFileInfo,
                                               NULL,
                                               directoryName,
                                               &fileInfo
                                              );
            if (error != ERROR_NONE)
            {
              printError("Cannot not read content of archive '%s' (error: %s)!\n",
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
              info(0,"Test directory '%s'...",String_cString(directoryName));

              /* check directory */
              if (!File_exists(directoryName))
              {
                info(0,"Directory '%s' does not exists!\n",
                     String_cString(directoryName)
                    );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(directoryName);
                if (failError == ERROR_NONE) failError = ERROR_FILE_NOT_FOUND;
                break;
              }
              if (File_getType(directoryName) != FILETYPE_DIRECTORY)
              {
                info(0,"Not a directory '%s'!\n",
                     String_cString(directoryName)
                    );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(directoryName);
                if (failError == ERROR_NONE) failError = ERROR_WRONG_FILE_TYPE;
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
              info(0,"ok\n",
                   String_cString(directoryName)
                  );

              /* free resources */
            }
            else
            {
              /* skip */
              info(1,"Restore '%s'...skipped\n",String_cString(directoryName));
            }

            /* close archive file */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(directoryName);
          }
          break;
        case FILETYPE_LINK:
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
              printError("Cannot not read content of archive '%s' (error: %s)!\n",
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
              info(0,"Test link '%s'...",String_cString(linkName));

              /* check link */
              if (!File_exists(linkName))
              {
                info(0,"Link '%s' does not exists!\n",
                     String_cString(linkName)
                    );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (failError == ERROR_NONE) failError = ERROR_FILE_NOT_FOUND;
                break;
              }
              if (File_getType(linkName) != FILETYPE_LINK)
              {
                info(0,"Not a link '%s'!\n",
                     String_cString(linkName)
                    );
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (failError == ERROR_NONE) failError = ERROR_WRONG_FILE_TYPE;
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
                if (failError == ERROR_NONE) failError = error;
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
                if (failError == ERROR_NONE) failError = ERROR_FILES_DIFFER;
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
      info(0,"Warning: incomplete file '%s'\n",String_cString(fileFragmentNode->fileName));
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
