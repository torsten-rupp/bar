/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/commands_restore.c,v $
* $Revision: 1.7 $
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
#include "patternlists.h"
#include "files.h"
#include "archive.h"
#include "fragmentlists.h"
#include "misc.h"

#include "commands_restore.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/
typedef struct
{
  EntryList                 *includeEntryList;
  PatternList               *excludePatternList;
  const JobOptions          *jobOptions;
  bool                      *pauseFlag;              // pause flag (can be NULL)
  bool                      *requestedAbortFlag;     // request abort flag (can be NULL)

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
*          destination         - destination directory or NULL
*          directoryStripCount - number of directories to strip from
*                                original file name
* Output : -
* Return : file name
* Notes  : -
\***********************************************************************/

LOCAL String getDestinationFileName(String       destinationFileName,
                                    String       fileName,
                                    const String destination,
                                    uint         directoryStripCount
                                   )
{
  String          pathName,baseName,name;
  StringTokenizer fileNameTokenizer;
  int             z;

  assert(destinationFileName != NULL);
  assert(fileName != NULL);

  /* get destination base directory */
  if (destination != NULL)
  {
    File_setFileName(destinationFileName,destination);
  }
  else
  {
    File_clearFileName(destinationFileName);
  }

  /* split file name */
  File_splitFileName(fileName,&pathName,&baseName);

  /* strip directory, create destination directory */
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

  /* create destination file name */
  File_appendFileName(destinationFileName,baseName);

  /* free resources  */
  String_delete(pathName);
  String_delete(baseName);

  return destinationFileName;
}

/***********************************************************************\
* Name   : getDestinationFileName
* Purpose: get destination file name by stripping directory levels and
*          add destination directory
* Input  : destinationDeviceName - destination device name variable
*          imageName             - original file name
*          destination           - destination device or NULL
* Output : -
* Return : file name
* Notes  : -
\***********************************************************************/

LOCAL String getDestinationDeviceName(String       destinationDeviceName,
                                      String       imageName,
                                      const String destination
                                     )
{
  assert(destinationDeviceName != NULL);
  assert(imageName != NULL);

  if (destination != NULL)
  {
    File_setFileName(destinationDeviceName,destination);
  }
  else
  {
    File_setFileName(destinationDeviceName,imageName);
  }

  return destinationDeviceName;
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

Errors Command_restore(StringList                      *archiveFileNameList,
                       EntryList                       *includeEntryList,
                       PatternList                     *excludePatternList,
                       JobOptions                      *jobOptions,
                       ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                       void                            *archiveGetCryptPasswordUserData,
                       RestoreStatusInfoFunction       restoreStatusInfoFunction,
                       void                            *restoreStatusInfoUserData,
                       bool                            *pauseFlag,
                       bool                            *requestedAbortFlag
                      )
{
  RestoreInfo       restoreInfo;
  byte              *buffer;
  FragmentList      fragmentList;
  String            archiveFileName;
  Errors            error;
  ArchiveInfo       archiveInfo;
  ArchiveFileInfo   archiveFileInfo;
  ArchiveEntryTypes archiveEntryType;
  FragmentNode      *fragmentNode;

  assert(archiveFileNameList != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(jobOptions != NULL);

  /* initialize variables */
  restoreInfo.includeEntryList             = includeEntryList;
  restoreInfo.excludePatternList           = excludePatternList;
  restoreInfo.jobOptions                   = jobOptions;
  restoreInfo.pauseFlag                    = pauseFlag;
  restoreInfo.requestedAbortFlag           = requestedAbortFlag;
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
  FragmentList_init(&fragmentList);
  archiveFileName = String_new();

  while (   ((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
         && !StringList_empty(archiveFileNameList)
         && (restoreInfo.error == ERROR_NONE)
        )
  {
    /* pause */
    while ((restoreInfo.pauseFlag != NULL) && (*restoreInfo.pauseFlag))
    {
      Misc_udelay(500*1000);
    }

    StringList_getFirst(archiveFileNameList,archiveFileName);
    printInfo(0,"Restore archive '%s':\n",String_cString(archiveFileName));

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
      if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
      continue;
    }
    String_set(restoreInfo.statusInfo.storageName,archiveFileName);
    updateStatusInfo(&restoreInfo);

    /* read files */
    while (   ((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
           && !Archive_eof(&archiveInfo)
           && (restoreInfo.error == ERROR_NONE)
          )
    {
      /* pause */
      while ((restoreInfo.pauseFlag != NULL) && (*restoreInfo.pauseFlag))
      {
        Misc_udelay(500*1000);
      }

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
        if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
        break;
      }

      switch (archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          {
            String       fileName;
            FileInfo     fileInfo;
            uint64       fragmentOffset,fragmentSize;
            String       destinationFileName;
            FragmentNode *fragmentNode;
            String       directoryName;
//            FileInfo         localFileInfo;
            FileHandle   fileHandle;
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
              if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
              continue;
            }

            if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              String_set(restoreInfo.statusInfo.fileName,fileName);
              restoreInfo.statusInfo.fileDoneBytes = 0LL;
              restoreInfo.statusInfo.fileTotalBytes = fragmentSize;
              updateStatusInfo(&restoreInfo);

              /* get destination filename */
              destinationFileName = getDestinationFileName(String_new(),
                                                           fileName,
                                                           jobOptions->destination,
                                                           jobOptions->directoryStripCount
                                                          );


              /* check if file fragment exists */
              fragmentNode = FragmentList_find(&fragmentList,fileName);
              if (fragmentNode != NULL)
              {
                if (!jobOptions->overwriteFilesFlag && FragmentList_checkEntryExists(fragmentNode,fragmentOffset,fragmentSize))
                {
                  printInfo(1,"  Restore file '%s'...skipped (file part %llu..%llu exists)\n",
                            String_cString(destinationFileName),
                            fragmentOffset,
                            (fragmentSize > 0)?fragmentOffset+fragmentSize-1:fragmentOffset
                           );
                  String_delete(destinationFileName);
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(fileName);
                  continue;
                }
              }
              else
              {
                if (!jobOptions->overwriteFilesFlag && File_exists(destinationFileName))
                {
                  printInfo(1,"  Restore file '%s'...skipped (file exists)\n",String_cString(destinationFileName));
                  String_delete(destinationFileName);
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(fileName);
                  continue;
                }
                fragmentNode = FragmentList_add(&fragmentList,fileName,fileInfo.size);
              }

              printInfo(2,"  Restore file '%s'...",String_cString(destinationFileName));

              /* create directory if not existing */
              directoryName = File_getFilePathName(String_new(),destinationFileName);
              if (!File_exists(directoryName))
              {
                /* create directory */
                error = File_makeDirectory(directoryName,
                                           FILE_DEFAULT_USER_ID,
                                           FILE_DEFAULT_GROUP_ID,
                                           fileInfo.permission
                                          );
                if (error != ERROR_NONE)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot create directory '%s' (error: %s)\n",
                             String_cString(directoryName),
                             Errors_getText(error)
                            );
                  String_delete(directoryName);
                  String_delete(destinationFileName);
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(fileName);
                  if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
                  continue;
                }

                /* set owner ship */
                error = File_setOwner(directoryName,
                                      (jobOptions->owner.userId != FILE_DEFAULT_USER_ID)?jobOptions->owner.userId:fileInfo.userId,
                                      (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID)?jobOptions->owner.groupId:fileInfo.groupId
                                     );
                if (error != ERROR_NONE)
                {
                  if (jobOptions->stopOnErrorFlag)
                  {
                    printInfo(2,"FAIL!\n");
                    printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                               String_cString(directoryName),
                               Errors_getText(error)
                              );
                    String_delete(directoryName);
                    String_delete(destinationFileName);
                    Archive_closeEntry(&archiveFileInfo);
                    String_delete(fileName);
                    if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
                    continue;
                  }
                  else
                  {
                    printWarning("Cannot set owner ship of directory '%s' (error: %s)\n",
                                 String_cString(directoryName),
                                 Errors_getText(error)
                                );
                  }
                }
              }
              String_delete(directoryName);

              /* write file data */
//if (fragmentNode == NULL) File_delete(destinationFileName,TRUE);
              error = File_open(&fileHandle,destinationFileName,FILE_OPENMODE_WRITE);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot create/write to file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Errors_getText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo.error = error;
                }
                continue;
              }
              error = File_seek(&fileHandle,fragmentOffset);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot write file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Errors_getText(error)
                          );
                File_close(&fileHandle);
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo.error = error;
                }
                continue;
              }

              length = 0;
              while (   ((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
                     && (length < fragmentSize)
                    )
              {
                /* pause */
                while ((restoreInfo.pauseFlag != NULL) && (*restoreInfo.pauseFlag))
                {
                  Misc_udelay(500*1000);
                }

                n = MIN(fragmentSize-length,BUFFER_SIZE);

                error = Archive_readData(&archiveFileInfo,buffer,n);
                if (error != ERROR_NONE)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot not read content of archive '%s' (error: %s)!\n",
                             String_cString(archiveFileName),
                             Errors_getText(error)
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
                             Errors_getText(error)
                            );
                  if (jobOptions->stopOnErrorFlag)
                  {
                    restoreInfo.error = error;
                  }
                  break;
                }
                restoreInfo.statusInfo.fileDoneBytes += n;
                updateStatusInfo(&restoreInfo);

                length += n;
              }
              if (File_getSize(&fileHandle) > fileInfo.size)
              {
                File_truncate(&fileHandle,fileInfo.size);
              }
              File_close(&fileHandle);
              if ((restoreInfo.requestedAbortFlag != NULL) && (*restoreInfo.requestedAbortFlag))
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

              /* set file time, file owner/group */
              if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
              if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
              error = File_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                if (jobOptions->stopOnErrorFlag)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot set file info of '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Errors_getText(error)
                            );
                  String_delete(destinationFileName);
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(fileName);
                  restoreInfo.error = error;
                  continue;
                }
                else
                {
                  printWarning("Cannot set file info of '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Errors_getText(error)
                              );
                }
              }

              /* add fragment to file fragment list */
              FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);
//FragmentList_print(fragmentNode,String_cString(fileName));

              /* discard fragment list if file is complete */
              if (FragmentList_checkEntryComplete(fragmentNode))
              {
                FragmentList_remove(&fragmentList,fragmentNode);
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
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            String       imageName;
            DeviceInfo   deviceInfo;
            uint64       blockOffset,blockCount;
            String       destinationDeviceName;
            FragmentNode *fragmentNode;
            DeviceHandle deviceHandle;
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
              if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
              break;
            }

            if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,imageName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,imageName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              String_set(restoreInfo.statusInfo.fileName,imageName);
              restoreInfo.statusInfo.fileDoneBytes = 0LL;
              restoreInfo.statusInfo.fileTotalBytes = blockCount;
              updateStatusInfo(&restoreInfo);

              /* get destination filename */
              destinationDeviceName = getDestinationDeviceName(String_new(),
                                                               imageName,
                                                               jobOptions->destination
                                                              );


              /* check if image fragment exists */
              fragmentNode = FragmentList_find(&fragmentList,imageName);
              if (fragmentNode != NULL)
              {
                if (!jobOptions->overwriteFilesFlag && FragmentList_checkEntryExists(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize))
                {
                  printInfo(1,"  Restore image '%s'...skipped (image part %llu..%llu exists)\n",
                            String_cString(destinationDeviceName),
                            blockOffset*(uint64)deviceInfo.blockSize,
                            ((blockCount > 0)?blockOffset+blockCount-1:blockOffset)*(uint64)deviceInfo.blockSize
                           );
                  String_delete(destinationDeviceName);
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(imageName);
                  continue;
                }
              }
              else
              {
                fragmentNode = FragmentList_add(&fragmentList,imageName,deviceInfo.size);
              }

              printInfo(2,"  Restore image '%s'...",String_cString(destinationDeviceName));

              /* write image data */
              error = Device_open(&deviceHandle,destinationDeviceName,DEVICE_OPENMODE_WRITE);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot write to device '%s' (error: %s)\n",
                           String_cString(destinationDeviceName),
                           Errors_getText(error)
                          );
                String_delete(destinationDeviceName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(imageName);
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo.error = error;
                }
                continue;
              }
              error = Device_seek(&deviceHandle,blockOffset*(uint64)deviceInfo.blockSize);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot write to device '%s' (error: %s)\n",
                           String_cString(destinationDeviceName),
                           Errors_getText(error)
                          );
                Device_close(&deviceHandle);
                String_delete(destinationDeviceName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(imageName);
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo.error = error;
                }
                continue;
              }

              length = 0;
              while (   ((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
                     && (length < blockCount)
                    )
              {
                /* pause */
                while ((restoreInfo.pauseFlag != NULL) && (*restoreInfo.pauseFlag))
                {
                  Misc_udelay(500*1000);
                }

                assert(deviceInfo.blockSize > 0);
                n = MIN(blockCount-length,BUFFER_SIZE/deviceInfo.blockSize);

                error = Archive_readData(&archiveFileInfo,buffer,n*deviceInfo.blockSize);
                if (error != ERROR_NONE)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot not read content of archive '%s' (error: %s)!\n",
                             String_cString(archiveFileName),
                             Errors_getText(error)
                            );
                  if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
                  break;
                }
                error = Device_write(&deviceHandle,buffer,n*deviceInfo.blockSize);
                if (error != ERROR_NONE)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot write to device '%s' (error: %s)\n",
                             String_cString(destinationDeviceName),
                             Errors_getText(error)
                            );
                  if (jobOptions->stopOnErrorFlag)
                  {
                    restoreInfo.error = error;
                  }
                  break;
                }
                restoreInfo.statusInfo.fileDoneBytes += n*deviceInfo.blockSize;
                updateStatusInfo(&restoreInfo);

                length += n;
              }
              Device_close(&deviceHandle);
              if ((restoreInfo.requestedAbortFlag != NULL) && (*restoreInfo.requestedAbortFlag))
              {
                printInfo(2,"ABORTED\n");
                String_delete(destinationDeviceName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(imageName);
                continue;
              }
#if 0
              if (restoreInfo.error != ERROR_NONE)
              {
                String_delete(destinationDeviceName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(imageName);
                continue;
              }
#endif /* 0 */

              /* add fragment to file fragment list */
              FragmentList_addEntry(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize);

              /* discard fragment list if file is complete */
              if (FragmentList_checkEntryComplete(fragmentNode))
              {
                FragmentList_remove(&fragmentList,fragmentNode);
              }

              /* free resources */
              String_delete(destinationDeviceName);

              printInfo(2,"ok\n");
            }
            else
            {
              /* skip */
              printInfo(3,"  Restore '%s'...skipped\n",String_cString(imageName));
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
            String   destinationFileName;
//            FileInfo localFileInfo;

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
              if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
              break;
            }

            if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              String_set(restoreInfo.statusInfo.fileName,directoryName);
              restoreInfo.statusInfo.fileDoneBytes = 0LL;
              restoreInfo.statusInfo.fileTotalBytes = 00L;
              updateStatusInfo(&restoreInfo);

              /* get destination filename */
              destinationFileName = getDestinationFileName(String_new(),
                                                           directoryName,
                                                           jobOptions->destination,
                                                           jobOptions->directoryStripCount
                                                          );


              /* check if directory already exists */
              if (!jobOptions->overwriteFilesFlag && File_exists(destinationFileName))
              {
                printInfo(1,
                          "  Restore directory '%s'...skipped (file exists)\n",
                          String_cString(destinationFileName)
                         );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(directoryName);
                continue;
              }

              printInfo(2,"  Restore directory '%s'...",String_cString(destinationFileName));

              /* create directory */
              error = File_makeDirectory(destinationFileName,
                                         FILE_DEFAULT_USER_ID,
                                         FILE_DEFAULT_GROUP_ID,
                                         fileInfo.permission
                                        );
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot create directory '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Errors_getText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(directoryName);
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo.error = error;
                }
                continue;
              }

              /* set file time, file owner/group */
              if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
              if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
              error = File_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                if (jobOptions->stopOnErrorFlag)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot set directory info of '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Errors_getText(error)
                            );
                  String_delete(destinationFileName);
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(directoryName);
                  if (jobOptions->stopOnErrorFlag)
                  {
                    restoreInfo.error = error;
                  }
                  continue;
                }
                else
                {
                  printWarning("Cannot set directory info of '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Errors_getText(error)
                              );
                }
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
        case ARCHIVE_ENTRY_TYPE_LINK:
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
              if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
              break;
            }

            if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              String_set(restoreInfo.statusInfo.fileName,linkName);
              restoreInfo.statusInfo.fileDoneBytes = 0LL;
              restoreInfo.statusInfo.fileTotalBytes = 00L;
              updateStatusInfo(&restoreInfo);

              /* get destination filename */
              destinationFileName = getDestinationFileName(String_new(),
                                                           linkName,
                                                           jobOptions->destination,
                                                           jobOptions->directoryStripCount
                                                          );


              /* create link */
              if (!jobOptions->overwriteFilesFlag && File_exists(destinationFileName))
              {
                printInfo(1,
                          "  Restore link '%s'...skipped (file exists)\n",
                          String_cString(destinationFileName)
                         );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo.error = ERROR_FILE_EXITS;
                }
                continue;
              }

              printInfo(2,"  Restore link '%s'...",String_cString(destinationFileName));

              error = File_makeLink(destinationFileName,fileName);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot create link '%s' -> '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           String_cString(fileName),
                           Errors_getText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo.error = error;
                }
                continue;
              }

              /* set file time, file owner/group */
              if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
              if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
              error = File_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                if (jobOptions->stopOnErrorFlag)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot set file info of '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Errors_getText(error)
                            );
                  String_delete(destinationFileName);
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(fileName);
                  String_delete(linkName);
                  if (jobOptions->stopOnErrorFlag)
                  {
                    restoreInfo.error = error;
                  }
                  continue;
                }
                else
                {
                  printWarning("Cannot set file info of '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Errors_getText(error)
                              );
                }
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
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          {
            String   fileName;
            FileInfo fileInfo;
            String   destinationFileName;
//            FileInfo localFileInfo;

            /* read special device */
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
              if (restoreInfo.error == ERROR_NONE) restoreInfo.error = error;
              break;
            }

            if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              String_set(restoreInfo.statusInfo.fileName,fileName);
              restoreInfo.statusInfo.fileDoneBytes = 0LL;
              restoreInfo.statusInfo.fileTotalBytes = 00L;
              updateStatusInfo(&restoreInfo);

              /* get destination filename */
              destinationFileName = getDestinationFileName(String_new(),
                                                           fileName,
                                                           jobOptions->destination,
                                                           jobOptions->directoryStripCount
                                                          );


              /* create special device */
              if (!jobOptions->overwriteFilesFlag && File_exists(destinationFileName))
              {
                printInfo(1,
                          "  Restore special device '%s'...skipped (file exists)\n",
                          String_cString(destinationFileName)
                         );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo.error = ERROR_FILE_EXITS;
                }
                continue;
              }

              printInfo(2,"  Restore special device '%s'...",String_cString(destinationFileName));

              error = File_makeSpecial(destinationFileName,
                                       fileInfo.specialType,
                                       fileInfo.major,
                                       fileInfo.minor
                                      );
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot create special device '%s' (error: %s)\n",
                           String_cString(fileName),
                           Errors_getText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveFileInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo.error = error;
                }
                continue;
              }

              /* set file time, file owner/group */
              if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
              if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
              error = File_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                if (jobOptions->stopOnErrorFlag)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot set file info of '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Errors_getText(error)
                            );
                  String_delete(destinationFileName);
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(fileName);
                  if (jobOptions->stopOnErrorFlag)
                  {
                    restoreInfo.error = error;
                  }
                  continue;
                }
                else
                {
                  printWarning("Cannot set file info of '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Errors_getText(error)
                              );
                }
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
  if ((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
  {
    for (fragmentNode = fragmentList.head; fragmentNode != NULL; fragmentNode = fragmentNode->next)
    {
      if (!FragmentList_checkEntryComplete(fragmentNode))
      {
        printInfo(0,"Warning: incomplete entry '%s'\n",String_cString(fragmentNode->name));
        if (restoreInfo.error == ERROR_NONE) restoreInfo.error = ERROR_FILE_INCOMPLETE;
      }
    }
  }

  /* free resources */
  String_delete(archiveFileName);
  FragmentList_done(&fragmentList);
  free(buffer);
  String_delete(restoreInfo.statusInfo.fileName);
  String_delete(restoreInfo.statusInfo.storageName);

  if ((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
  {
    return restoreInfo.error;
  }
  else
  {
    return ERROR_ABORTED;
  }
}

#ifdef __cplusplus
  }
#endif

/* end of file */
