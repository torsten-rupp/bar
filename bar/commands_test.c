/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_test.c,v $
* $Revision: 1.23 $
* $Author: torsten $
* Contents: Backup ARchiver archive test function
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

/*---------------------------------------------------------------------*/

Errors Command_test(StringList  *archiveFileNameList,
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
  while (!StringList_empty(archiveFileNameList))
  {
    StringList_getFirst(archiveFileNameList,archiveFileName);

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
        case FILE_TYPE_FILE:
          {
            String           fileName;
            FileInfo         fileInfo;
            uint64           fragmentOffset,fragmentSize;
            FileFragmentNode *fileFragmentNode;
            uint64           length;
            ulong            n;

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
              printInfo(0,"Test file '%s'...",String_cString(fileName));

              /* get file fragment list */
              fileFragmentNode = FileFragmentList_findFile(&fileFragmentList,fileName);
              if (fileFragmentNode == NULL)
              {
                fileFragmentNode = FileFragmentList_addFile(&fileFragmentList,fileName,fileInfo.size);
              }
//FileFragmentList_print(fileFragmentNode,String_cString(fileName));

              /* test file content */
              length = 0;
              while (length < fragmentSize)
              {
                n = MIN(fragmentSize-length,BUFFER_SIZE);

                /* read archive file */
                error = Archive_readFileData(&archiveFileInfo,archiveBuffer,n);
                if (error != ERROR_NONE)
                {
                  printInfo(0,"FAIL\n");
                  printError("Cannot not read content of archive '%s' (error: %s)!\n",
                             String_cString(archiveFileName),
                             getErrorText(error)
                            );
                  if (failError == ERROR_NONE) failError = error;
                  break;
                }

                length += n;
              }
              if (failError != ERROR_NONE)
              {
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                continue;
              }

              printInfo(0,"ok\n");

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
              printInfo(1,"Test '%s'...skipped\n",String_cString(fileName));
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
              printInfo(0,"Test directory '%s'...",String_cString(directoryName));

              printInfo(0,"ok\n");

              /* free resources */
            }
            else
            {
              /* skip */
              printInfo(1,"Test '%s'...skipped\n",String_cString(directoryName));
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
              printInfo(0,"Test link '%s'...",String_cString(linkName));

              printInfo(0,"ok\n");

              /* free resources */
            }
            else
            {
              /* skip */
              printInfo(1,"Test '%s'...skipped\n",String_cString(linkName));
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
