/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_restore.c,v $
* $Revision: 1.10 $
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

LOCAL String getDestinationFileName(String     destinationFileName,
                                    String     fileName,
                                    const char *directory,
                                    uint       directoryStripCount
                                   )
{
  String          pathName,baseName,name;
  StringTokenizer fileNameTokenizer;
  int             z;

  if (directory != NULL)
  {
    Files_setFileNameCString(destinationFileName,directory);
  }
  else
  {
    String_clear(destinationFileName);
  }
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

  return destinationFileName;
}


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
  bool            failFlag;
  Errors          error;
  ArchiveInfo     archiveInfo;
  ArchiveFileInfo archiveFileInfo;
  FileTypes       fileType;

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
    while (!Archive_eof(&archiveInfo))
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
            String     destinationFileName;
            FileInfo   localFileInfo;
            FileHandle fileHandle;
            uint64     length;
            ulong      n;

            /* read file */
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
              /* get destination filename */
              destinationFileName = getDestinationFileName(String_new(),
                                                           fileName,
                                                           directory,
                                                           directoryStripCount
                                                          );

              info(0,"Restore file '%s'...",String_cString(destinationFileName));

              /* check if file exists */
              if (Files_exist(destinationFileName) && !globalOptions.overwriteFlag)
              {
                info(0,"skipped (file exists)\n");
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                failFlag = TRUE;
                continue;
              }

              /* write file data */
              error = Files_open(&fileHandle,destinationFileName,FILE_OPENMODE_WRITE);
              if (error != ERROR_NONE)
              {
                info(0,"fail\n");
                printError("Cannot open file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           getErrorText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                failFlag = TRUE;
                continue;
              }
              error = Files_seek(&fileHandle,partOffset);
              if (error != ERROR_NONE)
              {
                info(0,"fail\n");
                printError("Cannot write file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           getErrorText(error)
                          );
                Files_close(&fileHandle);
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
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
                  printError("Cannot not read content of archive '%s' (error: %s)!\n",
                             String_cString(archiveFileName),
                             getErrorText(error)
                            );
                  failFlag = TRUE;
                  break;
                }
                error = Files_write(&fileHandle,buffer,n);
                if (error != ERROR_NONE)
                {
                  info(0,"fail\n");
                  printError("Cannot write file '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             getErrorText(error)
                            );
                  failFlag = TRUE;
                  break;
                }

                length += n;
              }
              Files_close(&fileHandle);
              if (failFlag)
              {
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                continue;
              }

              /* set file time, permissions, file owner/group */
              error = Files_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                info(0,"fail\n");
                printError("Cannot set file info of '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           getErrorText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
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

            /* close archive file, free resources */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(fileName);
          }
          break;
        case FILETYPE_DIRECTORY:
          {
            String   directoryName;
            FileInfo fileInfo;
            String   destinationFileName;
            FileInfo localFileInfo;

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
              failFlag = TRUE;
              break;
            }

            if (   (Lists_empty(includePatternList) || Patterns_matchList(includePatternList,directoryName,PATTERN_MATCH_MODE_EXACT))
                && !Patterns_matchList(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              /* get destination filename */
              destinationFileName = getDestinationFileName(String_new(),
                                                           directoryName,
                                                           directory,
                                                           directoryStripCount
                                                          );

              info(0,"Restore directory '%s'...",String_cString(destinationFileName));

              /* create directory */
              error = Files_makeDirectory(destinationFileName);
              if (error != ERROR_NONE)
              {
                info(0,"fail\n");
                printError("Cannot create directory '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           getErrorText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(directoryName);
                failFlag = TRUE;
                continue;
              }

              /* set file time, permissions, file owner/group */
              error = Files_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                info(0,"fail\n");
                printError("Cannot set directory info of '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           getErrorText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(directoryName);
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
            String   destinationFileName;
            FileInfo localFileInfo;

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
              failFlag = TRUE;
              break;
            }

            if (   (Lists_empty(includePatternList) || Patterns_matchList(includePatternList,linkName,PATTERN_MATCH_MODE_EXACT))
                && !Patterns_matchList(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              /* get destination filename */
              destinationFileName = getDestinationFileName(String_new(),
                                                           linkName,
                                                           directory,
                                                           directoryStripCount
                                                          );

              info(0,"Restore link '%s'...",String_cString(destinationFileName));

              /* create link */
              error = Files_link(destinationFileName,fileName);
              if (error != ERROR_NONE)
              {
                info(0,"fail\n");
                printError("Cannot create link '%s' -> '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           String_cString(fileName),
                           getErrorText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                failFlag = TRUE;
                continue;
              }

              /* set file time, permissions, file owner/group */
              error = Files_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                info(0,"fail\n");
                printError("Cannot set file info of '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           getErrorText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
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
  free(buffer);

  return !failFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
