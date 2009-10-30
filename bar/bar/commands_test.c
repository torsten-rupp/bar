/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/commands_test.c,v $
* $Revision: 1.6 $
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
#include "patternlists.h"
#include "files.h"
#include "archive.h"
#include "fragmentlists.h"

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

Errors Command_test(StringList                      *archiveFileNameList,
                    PatternList                     *includePatternList,
                    PatternList                     *excludePatternList,
                    JobOptions                      *jobOptions,
                    ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                    void                            *archiveGetCryptPasswordUserData
                   )
{
  byte              *archiveBuffer,*fileBuffer;
  FragmentList      fragmentList;
  String            archiveFileName;
  Errors            failError;
  Errors            error;
  ArchiveInfo       archiveInfo;
  ArchiveFileInfo   archiveFileInfo;
  ArchiveEntryTypes archiveEntryType;
  FragmentNode      *fragmentNode;

  assert(archiveFileNameList != NULL);
  assert(includePatternList != NULL);
  assert(excludePatternList != NULL);
  assert(jobOptions != NULL);

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
  FragmentList_init(&fragmentList);
  archiveFileName = String_new();

  failError = ERROR_NONE;
  while (!StringList_empty(archiveFileNameList))
  {
    StringList_getFirst(archiveFileNameList,archiveFileName);
    printInfo(0,"Testing archive '%s':\n",String_cString(archiveFileName));

    /* open archive */
    error = Archive_open(&archiveInfo,
                         archiveFileName,
                         jobOptions,
                         archiveGetCryptPasswordFunction,
                         archiveGetCryptPasswordUserData
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot open archive file '%s' (error: %s)!\n",
                 String_cString(archiveFileName),
                 Errors_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      continue;
    }

    /* read files */
    while (!Archive_eof(&archiveInfo) && (failError == ERROR_NONE))
    {
      /* get next archive entry type */
      error = Archive_getNextArchiveEntryType(&archiveInfo,
                                              &archiveFileInfo,
                                              &archiveEntryType
                                             );
      if (error != ERROR_NONE)
      {
        printError("Cannot not read next entry in archive '%s' (error: %s)!\n",
                   String_cString(archiveFileName),
                   Errors_getText(error)
                  );
        if (failError == ERROR_NONE) failError = error;
        break;
      }

      switch (archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          {
            String       fileName;
            FileInfo     fileInfo;
            uint64       fragmentOffset,fragmentSize;
            FragmentNode *fragmentNode;
            uint64       length;
            ulong        n;

            /* read file */
            fileName = String_new();
            error = Archive_readFileEntry(&archiveInfo,
                                          &archiveFileInfo,
                                          NULL,
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
                         Errors_getText(error)
                        );
              String_delete(fileName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            if (   (List_empty(includePatternList) || PatternList_match(includePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(2,"  Test file '%s'...",String_cString(fileName));

              /* get file fragment list */
              fragmentNode = FragmentList_find(&fragmentList,fileName);
              if (fragmentNode == NULL)
              {
                fragmentNode = FragmentList_add(&fragmentList,fileName,fileInfo.size);
              }
//FragmentList_print(fragmentNode,String_cString(fileName));

              /* test read file content */
              length = 0;
              while (length < fragmentSize)
              {
                n = MIN(fragmentSize-length,BUFFER_SIZE);

                /* read archive file */
                error = Archive_readData(&archiveFileInfo,archiveBuffer,n);
                if (error != ERROR_NONE)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot not read content of archive '%s' (error: %s)!\n",
                             String_cString(archiveFileName),
                             Errors_getText(error)
                            );
                  if (failError == ERROR_NONE) failError = error;
                  break;
                }

                length += (uint64)n;
              }
              if (failError != ERROR_NONE)
              {
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                continue;
              }

              printInfo(2,"ok\n");

              /* add fragment to file fragment list */
              FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);

              /* discard fragment list if file is complete */
              if (FragmentList_checkEntryComplete(fragmentNode))
              {
                FragmentList_remove(&fragmentList,fragmentNode);
              }

              /* free resources */
            }
            else
            {
              /* skip */
              printInfo(3,"  Test '%s'...skipped\n",String_cString(fileName));
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(fileName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            String       imageName;
            DeviceInfo   deviceInfo;
            uint64       blockOffset,blockCount;
            FragmentNode *fragmentNode;
            uint64       length;
            ulong        n;

            /* read image */
            imageName = String_new();
            error = Archive_readImageEntry(&archiveInfo,
                                           &archiveFileInfo,
                                           NULL,
                                           NULL,
                                           NULL,
                                           imageName,
                                           &deviceInfo,
                                           &blockOffset,
                                           &blockCount
                                          );
            if (error != ERROR_NONE)
            {
              printError("Cannot not read 'image' content of archive '%s' (error: %s)!\n",
                         String_cString(archiveFileName),
                         Errors_getText(error)
                        );
              String_delete(imageName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            if (   (List_empty(includePatternList) || PatternList_match(includePatternList,imageName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,imageName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(2,"  Test image '%s'...",String_cString(imageName));

              /* get file fragment list */
              fragmentNode = FragmentList_find(&fragmentList,imageName);
              if (fragmentNode == NULL)
              {
                fragmentNode = FragmentList_add(&fragmentList,imageName,deviceInfo.size);
              }
//FragmentList_print(fragmentNode,String_cString(imageName));

              /* test read image content */
              length = 0;
              while (length < blockCount)
              {
                assert(deviceInfo.blockSize > 0);
                n = MIN(blockCount-length,BUFFER_SIZE/deviceInfo.blockSize);

                /* read archive file */
                error = Archive_readData(&archiveFileInfo,archiveBuffer,n*deviceInfo.blockSize);
                if (error != ERROR_NONE)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot not read content of archive '%s' (error: %s)!\n",
                             String_cString(archiveFileName),
                             Errors_getText(error)
                            );
                  if (failError == ERROR_NONE) failError = error;
                  break;
                }

                length += (uint64)n;
              }
              if (failError != ERROR_NONE)
              {
                Archive_closeEntry(&archiveFileInfo);
                String_delete(imageName);
                continue;
              }

              printInfo(2,"ok\n");

              /* add fragment to file fragment list */
              FragmentList_addEntry(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize);

              /* discard fragment list if file is complete */
              if (FragmentList_checkEntryComplete(fragmentNode))
              {
                FragmentList_remove(&fragmentList,fragmentNode);
              }

              /* free resources */
            }
            else
            {
              /* skip */
              printInfo(3,"  Test '%s'...skipped\n",String_cString(imageName));
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(imageName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          {
            String   directoryName;
            FileInfo fileInfo;

            /* read directory */
            directoryName = String_new();
            error = Archive_readDirectoryEntry(&archiveInfo,
                                               &archiveFileInfo,
                                               NULL,
                                               NULL,
                                               directoryName,
                                               &fileInfo
                                              );
            if (error != ERROR_NONE)
            {
              printError("Cannot not read 'directory' content of archive '%s' (error: %s)!\n",
                         String_cString(archiveFileName),
                         Errors_getText(error)
                        );
              String_delete(directoryName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            if (   (List_empty(includePatternList) || PatternList_match(includePatternList,directoryName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(2,"  Test directory '%s'...",String_cString(directoryName));

              printInfo(2,"ok\n");

              /* free resources */
            }
            else
            {
              /* skip */
              printInfo(3,"Test '%s'...skipped\n",String_cString(directoryName));
            }

            /* close archive file */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(directoryName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
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
                                          NULL,
                                          linkName,
                                          fileName,
                                          &fileInfo
                                         );
            if (error != ERROR_NONE)
            {
              printError("Cannot not read 'link' content of archive '%s' (error: %s)!\n",
                         String_cString(archiveFileName),
                         Errors_getText(error)
                        );
              String_delete(fileName);
              String_delete(linkName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            if (   (List_empty(includePatternList) || PatternList_match(includePatternList,linkName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(2,"  Test link '%s'...",String_cString(linkName));

              printInfo(2,"ok\n");

              /* free resources */
            }
            else
            {
              /* skip */
              printInfo(3,"  Test '%s'...skipped\n",String_cString(linkName));
            }

            /* close archive file */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(fileName);
            String_delete(linkName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          {
            String   fileName;
            FileInfo fileInfo;

            /* read special */
            fileName = String_new();
            error = Archive_readSpecialEntry(&archiveInfo,
                                             &archiveFileInfo,
                                             NULL,
                                             NULL,
                                             fileName,
                                             &fileInfo
                                            );
            if (error != ERROR_NONE)
            {
              printError("Cannot not read 'special' content of archive '%s' (error: %s)!\n",
                         String_cString(archiveFileName),
                         Errors_getText(error)
                        );
              String_delete(fileName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            if (   (List_empty(includePatternList) || PatternList_match(includePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(2,"  Test special device '%s'...",String_cString(fileName));

              printInfo(2,"ok\n");

              /* free resources */
            }
            else
            {
              /* skip */
              printInfo(3,"  Test '%s'...skipped\n",String_cString(fileName));
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
  for (fragmentNode = fragmentList.head; fragmentNode != NULL; fragmentNode = fragmentNode->next)
  {
    if (!FragmentList_checkEntryComplete(fragmentNode))
    {
      printInfo(0,"Warning: incomplete file '%s'\n",String_cString(fragmentNode->name));
      if (failError == ERROR_NONE) failError = ERROR_FILE_INCOMPLETE;
    }
  }

  /* free resources */
  String_delete(archiveFileName);
  FragmentList_done(&fragmentList);
  free(fileBuffer);
  free(archiveBuffer);

  return failError;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
