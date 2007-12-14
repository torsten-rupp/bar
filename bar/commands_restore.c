/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_restore.c,v $
* $Revision: 1.28 $
* $Author: torsten $
* Contents: Backup ARchiver archive restore function
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

#include "commands_restore.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/
typedef struct
{
  PatternList               *includePatternList;
  PatternList               *excludePatternList;
  const Options             *options;

  time_t                    startTime;

  Errors                    error;

  RestoreStatusInfoFunction statusInfoFunction;
  void                      *statusInfoUserData;
  RestoreStatusInfo         statusInfo;              // status info
} RestoreInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : getDestinationFileName
* Purpose: get destination file name by stripping directory levels and
*          add destination directory
* Input  : destinationFileName - destination file name variable
*          fileName            - original file name
*          directory           - destination directory
*          directoryStripCount - number of directories to strip from
*                                original file name
* Output : -
* Return : file name
* Notes  : -
\***********************************************************************/

LOCAL String getDestinationFileName(String       destinationFileName,
                                    String       fileName,
                                    const String directory,
                                    uint         directoryStripCount
                                   )
{
  String          pathName,baseName,name;
  StringTokenizer fileNameTokenizer;
  int             z;

  if (directory != NULL)
  {
    File_setFileName(destinationFileName,directory);
  }
  else
  {
    String_clear(destinationFileName);
  }
  File_splitFileName(fileName,&pathName,&baseName);
  File_initSplitFileName(&fileNameTokenizer,pathName);
  z = 0;
  while ((z< directoryStripCount) && File_getNextSplitFileName(&fileNameTokenizer,&name))
  {
    z++;
  }
  while (File_getNextSplitFileName(&fileNameTokenizer,&name))
  {
    File_appendFileName(destinationFileName,name);
  }     
  File_doneSplitFileName(&fileNameTokenizer);
  File_appendFileName(destinationFileName,baseName);
  String_delete(pathName);
  String_delete(baseName);

  return destinationFileName;
}

/***********************************************************************\
* Name   : updateStatusInfo
* Purpose: update status info
* Input  : createInfo - create info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateStatusInfo(const RestoreInfo *restoreInfo)
{
  assert(restoreInfo != NULL);

  if (restoreInfo->statusInfoFunction != NULL)
  {
    restoreInfo->statusInfoFunction(restoreInfo->error,&restoreInfo->statusInfo,restoreInfo->statusInfoUserData);
  }
}

/*---------------------------------------------------------------------*/

Errors Command_restore(StringList                *archiveFileNameList,
                       PatternList               *includePatternList,
                       PatternList               *excludePatternList,
                       Options                   *options,
                       RestoreStatusInfoFunction restoreStatusInfoFunction,
                       void                      *restoreStatusInfoUserData,
                       bool                      *abortRequestFlag
                      )
{
  RestoreInfo       restoreInfo;
  byte              *buffer;
  FileFragmentList  fileFragmentList;
  String            archiveFileName;
  Errors            error;
  ArchiveInfo       archiveInfo;
  ArchiveFileInfo   archiveFileInfo;
  FileTypes         fileType;
  FileFragmentNode  *fileFragmentNode;

  assert(archiveFileNameList != NULL);
  assert(includePatternList != NULL);
  assert(excludePatternList != NULL);
  assert(options != NULL);

  /* initialize variables */
  restoreInfo.includePatternList           = includePatternList;
  restoreInfo.excludePatternList           = excludePatternList;
  restoreInfo.options                      = options;
  restoreInfo.startTime                    = time(NULL);
  restoreInfo.error                        = ERROR_NONE;
  restoreInfo.statusInfoFunction           = restoreStatusInfoFunction;
  restoreInfo.statusInfoUserData           = restoreStatusInfoUserData;
  restoreInfo.statusInfo.doneFiles         = 0L;
  restoreInfo.statusInfo.doneBytes         = 0LL;
  restoreInfo.statusInfo.skippedFiles      = 0L;
  restoreInfo.statusInfo.skippedBytes      = 0LL;
  restoreInfo.statusInfo.errorFiles        = 0L;
  restoreInfo.statusInfo.errorBytes        = 0LL;
  restoreInfo.statusInfo.fileName          = String_new();
  restoreInfo.statusInfo.fileDoneBytes     = 0LL;
  restoreInfo.statusInfo.fileTotalBytes    = 0LL;
  restoreInfo.statusInfo.storageName       = String_new();
  restoreInfo.statusInfo.storageDoneBytes  = 0LL;
  restoreInfo.statusInfo.storageTotalBytes = 0LL;

  /* allocate resources */
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  FileFragmentList_init(&fileFragmentList);
  archiveFileName = String_new();

  while (   ((abortRequestFlag == NULL) || !(*abortRequestFlag))
         && !StringList_empty(archiveFileNameList)
        )
  {
    StringList_getFirst(archiveFileNameList,archiveFileName);
    printInfo(0,"Restore archive '%s':\n",String_cString(archiveFileName));

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
      if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
      continue;
    }
    String_set(restoreInfo.statusInfo.storageName,archiveFileName);
    updateStatusInfo(&restoreInfo);

    /* read files */
    while (   ((abortRequestFlag == NULL) || !(*abortRequestFlag))
           && !Archive_eof(&archiveInfo))
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
        if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
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
            String           destinationFileName;
            String           directoryName;
//            FileInfo         localFileInfo;
            FileHandle       fileHandle;
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
              if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
              break;
            }

            if (   (List_empty(includePatternList) || Pattern_matchList(includePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !Pattern_matchList(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              String_set(restoreInfo.statusInfo.fileName,fileName);
              restoreInfo.statusInfo.fileDoneBytes = 0LL;
              restoreInfo.statusInfo.fileTotalBytes = fragmentSize;
              updateStatusInfo(&restoreInfo);

              /* get destination filename */
              destinationFileName = getDestinationFileName(String_new(),
                                                           fileName,
                                                           options->directory,
                                                           options->directoryStripCount
                                                          );


              /* check if file fragment exists */
              fileFragmentNode = FileFragmentList_findFile(&fileFragmentList,fileName);
              if (fileFragmentNode != NULL)
              {
                if (!options->overwriteFilesFlag && FileFragmentList_checkExists(fileFragmentNode,fragmentOffset,fragmentSize))
                {
                  printInfo(1,"  Restore file '%s'...skipped (file part %ll..%ll exists)\n",
                            String_cString(destinationFileName),
                            fragmentOffset,
                            (fragmentSize > 0)?fragmentOffset+fragmentSize-1:fragmentOffset
                           );
                  String_delete(destinationFileName);
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(fileName);
//                  if (restoreInfo.error == ERROR_NONE) restoreInfo.error = ERROR_FILE_EXITS;
                  continue;
                }
              }
              else
              {
                if (!options->overwriteFilesFlag && File_exists(destinationFileName))
                {
                  printInfo(1,"  Restore file '%s'...skipped (file exists)\n",String_cString(destinationFileName));
                  String_delete(destinationFileName);
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(fileName);
//                  if (restoreInfo.error == ERROR_NONE) restoreInfo.error = ERROR_FILE_EXITS;
                  continue;
                }
                fileFragmentNode = FileFragmentList_addFile(&fileFragmentList,fileName,fileInfo.size);
              }

              printInfo(2,"  Restore file '%s'...",String_cString(destinationFileName));

              /* create directory if not existing */
              directoryName = File_getFilePathName(destinationFileName,String_new());
              if (!File_exists(directoryName))
              {
                error = File_makeDirectory(directoryName);
                if (error != ERROR_NONE)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot create directory '%s' (error: %s)\n",
                             String_cString(directoryName),
                             getErrorText(error)
                            );
                  String_delete(directoryName);
                  String_delete(destinationFileName);
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(fileName);
                  if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
                  continue;
                }
              }
              String_delete(directoryName);

              /* write file data */
//if (fileFragmentNode == NULL) File_delete(destinationFileName,TRUE);
              error = File_open(&fileHandle,destinationFileName,FILE_OPENMODE_CREATE);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot open file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           getErrorText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
                continue;
              }
              error = File_seek(&fileHandle,fragmentOffset);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot write file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           getErrorText(error)
                          );
                File_close(&fileHandle);
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
                continue;
              }

              length = 0;
              while (   ((abortRequestFlag == NULL) || !(*abortRequestFlag))
                     && (length < fragmentSize)
                    )
              {
                n = MIN(fragmentSize-length,BUFFER_SIZE);

                error = Archive_readFileData(&archiveFileInfo,buffer,n);
                if (error != ERROR_NONE)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot not read content of archive '%s' (error: %s)!\n",
                             String_cString(archiveFileName),
                             getErrorText(error)
                            );
                  if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
                  break;
                }
                error = File_write(&fileHandle,buffer,n);
                if (error != ERROR_NONE)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot write file '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             getErrorText(error)
                            );
                  if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
                  break;
                }
                restoreInfo.statusInfo.fileDoneBytes += n;
                updateStatusInfo(&restoreInfo);

                length += n;
              }
              File_close(&fileHandle);
              if ((abortRequestFlag != NULL) && (*abortRequestFlag))
              {
                printInfo(2,"ABORTED\n");
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                continue;
              }
#if 0
              if (restoreInfo.error != ERROR_NONE)
              {
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                continue;
              }
#endif /* 0 */

              /* add fragment to file fragment list */
              FileFragmentList_add(fileFragmentNode,fragmentOffset,fragmentSize);
//FileFragmentList_print(fileFragmentNode,String_cString(fileName));

              /* set file time, permissions, file owner/group */
              error = File_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot set file info of '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           getErrorText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
                continue;
              }

              /* discard fragment list if file is complete */
              if (FileFragmentList_checkComplete(fileFragmentNode))
              {
                FileFragmentList_removeFile(&fileFragmentList,fileFragmentNode);
              }

              /* free resources */
              String_delete(destinationFileName);

              printInfo(2,"ok\n");
            }
            else
            {
              /* skip */
              printInfo(3,"  Restore '%s'...skipped\n",String_cString(fileName));
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
            String   destinationFileName;
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
              printError("Cannot not read content of archive '%s' (error: %s)!\n",
                         String_cString(archiveFileName),
                         getErrorText(error)
                        );
              String_delete(directoryName);
              if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
              break;
            }

            if (   (List_empty(includePatternList) || Pattern_matchList(includePatternList,directoryName,PATTERN_MATCH_MODE_EXACT))
                && !Pattern_matchList(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              String_set(restoreInfo.statusInfo.fileName,directoryName);
              restoreInfo.statusInfo.fileDoneBytes = 0LL;
              restoreInfo.statusInfo.fileTotalBytes = 00L;
              updateStatusInfo(&restoreInfo);

              /* get destination filename */
              destinationFileName = getDestinationFileName(String_new(),
                                                           directoryName,
                                                           options->directory,
                                                           options->directoryStripCount
                                                          );


              /* create directory */
//File_delete(destinationFileName,TRUE);
              if (!options->overwriteFilesFlag && File_exists(destinationFileName))
              {
                printInfo(1,
                          "  Restore directory '%s'...skipped (file exists)\n",
                          String_cString(destinationFileName)
                         );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(directoryName);
//                if (restoreInfo.error == ERROR_NONE) restoreInfo.error = ERROR_FILE_EXITS;
                continue;
              }

              printInfo(2,"  Restore directory '%s'...",String_cString(destinationFileName));

              error = File_makeDirectory(destinationFileName);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot create directory '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           getErrorText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(directoryName);
                if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
                continue;
              }

              /* set file time, permissions, file owner/group */
              error = File_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot set directory info of '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           getErrorText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(directoryName);
                if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
                continue;
              }

              /* free resources */
              String_delete(destinationFileName);

              printInfo(2,"ok\n");
            }
            else
            {
              /* skip */
              printInfo(3,"  Restore '%s'...skipped\n",String_cString(directoryName));
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
            String   destinationFileName;
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
              if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
              break;
            }

            if (   (List_empty(includePatternList) || Pattern_matchList(includePatternList,linkName,PATTERN_MATCH_MODE_EXACT))
                && !Pattern_matchList(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              String_set(restoreInfo.statusInfo.fileName,linkName);
              restoreInfo.statusInfo.fileDoneBytes = 0LL;
              restoreInfo.statusInfo.fileTotalBytes = 00L;
              updateStatusInfo(&restoreInfo);

              /* get destination filename */
              destinationFileName = getDestinationFileName(String_new(),
                                                           linkName,
                                                           options->directory,
                                                           options->directoryStripCount
                                                          );


              /* create link */
//File_delete(destinationFileName,TRUE);
              if (!options->overwriteFilesFlag && File_exists(destinationFileName))
              {
                printInfo(1,
                          "  Restore link '%s'...skipped (file exists)\n",
                          String_cString(destinationFileName)
                         );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
//                if (restoreInfo.error == ERROR_NONE) restoreInfo.error = ERROR_FILE_EXITS;
                continue;
              }

              printInfo(2,"  Restore link '%s'...",String_cString(destinationFileName));

              error = File_link(destinationFileName,fileName);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot create link '%s' -> '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           String_cString(fileName),
                           getErrorText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
                continue;
              }

              /* set file time, permissions, file owner/group */
              error = File_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot set file info of '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           getErrorText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
                continue;
              }

              /* free resources */
              String_delete(destinationFileName);

              printInfo(2,"ok\n");
            }
            else
            {
              /* skip */
              printInfo(3,"  Restore '%s'...skipped\n",String_cString(linkName));
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
  if ((abortRequestFlag != NULL) && (*abortRequestFlag))
  {
    restoreInfo.error = ERROR_ABORTED;
  }

  if ((abortRequestFlag == NULL) || !(*abortRequestFlag))
  {
    /* check fragment lists */
    for (fileFragmentNode = fileFragmentList.head; fileFragmentNode != NULL; fileFragmentNode = fileFragmentNode->next)
    {
      if (!FileFragmentList_checkComplete(fileFragmentNode))
      {
        printInfo(0,"Warning: incomplete file '%s'\n",String_cString(fileFragmentNode->fileName));
        if (restoreInfo.error == ERROR_NONE) restoreInfo.error = ERROR_FILE_INCOMPLETE;
      }
    }
  }

  /* free resources */
  String_delete(archiveFileName);
  FileFragmentList_done(&fileFragmentList);
  free(buffer);
  String_delete(restoreInfo.statusInfo.fileName);
  String_delete(restoreInfo.statusInfo.storageName);

  return restoreInfo.error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
