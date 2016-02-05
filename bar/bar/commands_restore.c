/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive restore functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

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
#include "autofree.h"
#include "strings.h"
#include "stringlists.h"

#include "errors.h"
#include "patterns.h"
#include "patternlists.h"
#include "deltasourcelists.h"
#include "files.h"
#include "archive.h"
#include "fragmentlists.h"
#include "misc.h"

#include "commands_restore.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// file data buffer size
#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/

// restore information
typedef struct
{
  RestoreStatusInfoFunction statusInfoFunction;      // status info call back
  void                      *statusInfoUserData;     // user data for status info call back

  Errors                    failError;               // restore error
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
*          name                - original name
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
                                    int          directoryStripCount
                                   )
{
  String          pathName,baseName;
  StringTokenizer fileNameTokenizer;
  ConstString     token;
  int             i;

  assert(destinationFileName != NULL);
  assert(fileName != NULL);

  // get destination base directory
  if (destination != NULL)
  {
    File_setFileName(destinationFileName,destination);
  }
  else
  {
    String_clear(destinationFileName);
  }

  // split original name
  File_splitFileName(fileName,&pathName,&baseName);

  // strip directory
  if (directoryStripCount != DIRECTORY_STRIP_NONE)
  {
    File_initSplitFileName(&fileNameTokenizer,pathName);
    i = 0;
    while (   ((directoryStripCount == DIRECTORY_STRIP_ANY) || (i < directoryStripCount))
           && File_getNextSplitFileName(&fileNameTokenizer,&token)
          )
    {
      i++;
    }
    while (File_getNextSplitFileName(&fileNameTokenizer,&token))
    {
      File_appendFileName(destinationFileName,token);
    }
  }
  else
  {
    File_appendFileName(destinationFileName,pathName);
  }

  // append file name
  File_appendFileName(destinationFileName,baseName);

  // free resources
  String_delete(pathName);
  String_delete(baseName);

  return destinationFileName;
}

/***********************************************************************\
* Name   : getDestinationDeviceName
* Purpose: get destination device name
* Input  : destinationDeviceName - destination device name variable
*          imageName             - original file name
*          destination           - destination device or NULL
* Output : -
* Return : device name
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
    if (File_isDirectory(destination))
    {
      File_setFileName(destinationDeviceName,destination);
      File_appendFileName(destinationDeviceName,imageName);
    }
    else
    {
      File_setFileName(destinationDeviceName,destination);
    }
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
* Input  : restoreInfo - restore info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateStatusInfo(RestoreInfo *restoreInfo)
{
  assert(restoreInfo != NULL);

  if (restoreInfo->statusInfoFunction != NULL)
  {
    restoreInfo->statusInfoFunction(restoreInfo->statusInfoUserData,restoreInfo->failError,&restoreInfo->statusInfo);
  }
}

/***********************************************************************\
* Name   : restoreArchiveContent
* Purpose: restore archive content
* Input  : storageSpecifier                 - storage specifier
*          archiveName                      - archive name
*          includeEntryList                 - include entry list
*          excludePatternList               - exclude pattern list
*          deltaSourceList                  - delta source list
*          jobOptions                       - job options
*          archiveGetCryptPasswordFunction  - get password call back
*          archiveGetCryptPasswordUserData  - user data for get password
*          pauseRestoreFlag                 - pause restore flag (can be
*                                             NULL)
*          requestedAbortFlag               - request abort flag (can be
*                                             NULL)
*          fragmentList                     - fragment list
*          restoreInfo                      - restore info
*          logHandle                        - log handle (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors restoreArchiveContent(StorageSpecifier                *storageSpecifier,
                                   ConstString                     archiveName,
                                   const EntryList                 *includeEntryList,
                                   const PatternList               *excludePatternList,
                                   DeltaSourceList                 *deltaSourceList,
                                   JobOptions                      *jobOptions,
                                   ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                                   void                            *archiveGetCryptPasswordUserData,
                                   bool                            *pauseRestoreFlag,
                                   bool                            *requestedAbortFlag,
                                   FragmentList                    *fragmentList,
                                   RestoreInfo                     *restoreInfo,
                                   LogHandle                       *logHandle
                                  )
{
  AutoFreeList      autoFreeList;
  byte              *buffer;
  StorageHandle     storageHandle;
  Errors            error;
  ArchiveInfo       archiveInfo;
  void              *autoFreeSavePoint1,*autoFreeSavePoint2;
  ArchiveEntryInfo  archiveEntryInfo;
  ArchiveEntryTypes archiveEntryType;
  FragmentNode      *fragmentNode;

  assert(storageSpecifier != NULL);
  assert(includeEntryList != NULL);
  assert(jobOptions != NULL);
  assert(fragmentList != NULL);
  assert(restoreInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,buffer,{ free(buffer); });

  // init storage
  error = Storage_init(&storageHandle,
                       storageSpecifier,
                       jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK(NULL,NULL),
                       CALLBACK(NULL,NULL)
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize storage '%s' (error: %s)!\n",
               Storage_getPrintableNameCString(storageSpecifier,archiveName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&storageHandle,{ Storage_done(&storageHandle); });

  // open archive
  error = Archive_open(&archiveInfo,
                       &storageHandle,
                       storageSpecifier,
                       archiveName,
                       deltaSourceList,
                       jobOptions,
                       archiveGetCryptPasswordFunction,
                       archiveGetCryptPasswordUserData,
                       logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot open storage '%s' (error: %s)!\n",
               Storage_getPrintableNameCString(storageSpecifier,archiveName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveInfo,{ Archive_close(&archiveInfo); });
  String_set(restoreInfo->statusInfo.storageName,Storage_getPrintableName(storageSpecifier,archiveName));
  restoreInfo->statusInfo.storageDoneBytes  = 0LL;
  restoreInfo->statusInfo.storageTotalBytes = Archive_getSize(&archiveInfo);
  updateStatusInfo(restoreInfo);

  // read archive entries
  printInfo(0,"Restore from archive '%s':\n",Storage_getPrintableNameCString(storageSpecifier,archiveName));
  while (   ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
         && !Archive_eof(&archiveInfo,TRUE)
         && (restoreInfo->failError == ERROR_NONE)
        )
  {
    // pause
    while ((pauseRestoreFlag != NULL) && (*pauseRestoreFlag))
    {
      Misc_udelay(500L*1000L);
    }

    // get next archive entry type
    error = Archive_getNextArchiveEntryType(&archiveInfo,
                                            &archiveEntryType,
                                            TRUE
                                           );
    if (error != ERROR_NONE)
    {
      printError("Cannot read next entry in archive '%s' (error: %s)!\n",
                 Storage_getPrintableNameCString(storageSpecifier,archiveName),
                 Error_getText(error)
                );
      if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
      break;
    }

    // update storage status
    restoreInfo->statusInfo.storageDoneBytes = Archive_tell(&archiveInfo);
    updateStatusInfo(restoreInfo);

    switch (archiveEntryType)
    {
      case ARCHIVE_ENTRY_TYPE_FILE:
        {
          String                    fileName;
          FileExtendedAttributeList fileExtendedAttributeList;
          FileInfo                  fileInfo;
          uint64                    fragmentOffset,fragmentSize;
          String                    destinationFileName;
          String                    parentDirectoryName;
//            FileInfo                      localFileInfo;
          FileHandle                fileHandle;
          uint64                    length;
          ulong                     n;

          autoFreeSavePoint1 = AutoFree_save(&autoFreeList);

          // read file
          fileName = String_new();
          File_initExtendedAttributes(&fileExtendedAttributeList);
          error = Archive_readFileEntry(&archiveEntryInfo,
                                        &archiveInfo,
                                        NULL,  // deltaCompressAlgorithm
                                        NULL,  // byteCompressAlgorithm
                                        NULL,  // cryptAlgorithm
                                        NULL,  // cryptType
                                        fileName,
                                        &fileInfo,
                                        &fileExtendedAttributeList,
                                        NULL,  // deltaSourceName
                                        NULL,  // deltaSourceSize
                                        &fragmentOffset,
                                        &fragmentSize
                                       );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'file' content of archive '%s' (error: %s)!\n",
                       Storage_getPrintableNameCString(storageSpecifier,archiveName),
                       Error_getText(error)
                      );
            File_doneExtendedAttributes(&fileExtendedAttributeList);
            String_delete(fileName);
            if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
            AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
            continue;
          }
          AUTOFREE_ADD(&autoFreeList,fileName,{ String_delete(fileName); });
          AUTOFREE_ADD(&autoFreeList,&fileExtendedAttributeList,{ File_doneExtendedAttributes(&fileExtendedAttributeList); });
          AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo,{ Archive_closeEntry(&archiveEntryInfo); });

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
              && ((excludePatternList == NULL) || !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
             )
          {
            String_set(restoreInfo->statusInfo.entryName,fileName);
            restoreInfo->statusInfo.entryDoneBytes  = 0LL;
            restoreInfo->statusInfo.entryTotalBytes = fragmentSize;
            updateStatusInfo(restoreInfo);

            // get destination filename
            destinationFileName = getDestinationFileName(String_new(),
                                                         fileName,
                                                         jobOptions->destination,
                                                         jobOptions->directoryStripCount
                                                        );
            AUTOFREE_ADD(&autoFreeList,destinationFileName,{ String_delete(destinationFileName); });

            // check if file fragment already exists, file already exists
            if (!jobOptions->noFragmentsCheckFlag)
            {
              // get/create file fragment node
              fragmentNode = FragmentList_find(fragmentList,destinationFileName);
              if (fragmentNode != NULL)
              {
                if (!jobOptions->overwriteFilesFlag && FragmentList_entryExists(fragmentNode,fragmentOffset,fragmentSize))
                {
                  printInfo(1,
                            "  Restore file '%s'...skipped (file part %llu..%llu exists)\n",
                            String_cString(destinationFileName),
                            fragmentOffset,
                            (fragmentSize > 0LL) ? fragmentOffset+fragmentSize-1 : fragmentOffset
                           );
                  AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                  continue;
                }
              }
              else
              {
                if (!jobOptions->overwriteFilesFlag && File_exists(destinationFileName))
                {
                  printInfo(1,"  Restore file '%s'...skipped (file exists)\n",String_cString(destinationFileName));
                  AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                  continue;
                }
                fragmentNode = FragmentList_add(fragmentList,destinationFileName,fileInfo.size,&fileInfo,sizeof(FileInfo));
              }
              assert(fragmentNode != NULL);
            }
            else
            {
              fragmentNode = NULL;
            }

            printInfo(1,"  Restore file '%s'...",String_cString(destinationFileName));

            // create parent directories if not existing
            if (!jobOptions->dryRunFlag)
            {
              parentDirectoryName = File_getFilePathName(String_new(),destinationFileName);
              if (!String_isEmpty(parentDirectoryName) && !File_exists(parentDirectoryName))
              {
                // create directory
                error = File_makeDirectory(parentDirectoryName,
                                           FILE_DEFAULT_USER_ID,
                                           FILE_DEFAULT_GROUP_ID,
                                           FILE_DEFAULT_PERMISSION
                                          );
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot create directory '%s' (error: %s)\n",
                             String_cString(parentDirectoryName),
                             Error_getText(error)
                            );
                  String_delete(parentDirectoryName);
                  if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
                  AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                  continue;
                }

                // set directory owner ship
                error = File_setOwner(parentDirectoryName,
                                      (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? jobOptions->owner.userId  : fileInfo.userId,
                                      (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? jobOptions->owner.groupId : fileInfo.groupId
                                     );
                if (error != ERROR_NONE)
                {
                  if (jobOptions->stopOnErrorFlag)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                               String_cString(parentDirectoryName),
                               Error_getText(error)
                              );
                    String_delete(parentDirectoryName);
                    if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
                    AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                    continue;
                  }
                  else
                  {
                    printWarning("Cannot set owner ship of directory '%s' (error: %s)\n",
                                 String_cString(parentDirectoryName),
                                 Error_getText(error)
                                );
                  }
                }
              }
              String_delete(parentDirectoryName);
            }

            if (!jobOptions->dryRunFlag)
            {
              // temporary change owern+permission for writing (ignore errors)
              (void)File_setPermission(destinationFileName,FILE_PERMISSION_USER_READ|FILE_PERMISSION_USER_WRITE);
              (void)File_setOwner(destinationFileName,FILE_OWN_USER_ID,FILE_OWN_GROUP_ID);

              // open file
              error = File_open(&fileHandle,destinationFileName,FILE_OPEN_WRITE);
              if (error != ERROR_NONE)
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot create/write to file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Error_getText(error)
                          );
                if (jobOptions->stopOnErrorFlag) restoreInfo->failError = error;
                AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                continue;
              }
              AUTOFREE_ADD(&autoFreeList,&fileHandle,{ (void)File_close(&fileHandle); });

              // seek to fragment position
              error = File_seek(&fileHandle,fragmentOffset);
              if (error != ERROR_NONE)
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot write file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Error_getText(error)
                          );
                if (jobOptions->stopOnErrorFlag) restoreInfo->failError = error;
                AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                continue;
              }
            }

            // write file data
            length = 0LL;
            while (   ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
                   && (length < fragmentSize)
                  )
            {
              // pause
              while ((pauseRestoreFlag != NULL) && (*pauseRestoreFlag))
              {
                Misc_udelay(500L*1000L);
              }

              n = (ulong)MIN(fragmentSize-length,BUFFER_SIZE);

              error = Archive_readData(&archiveEntryInfo,buffer,n);
              if (error != ERROR_NONE)
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot read content of archive '%s' (error: %s)!\n",
                           Storage_getPrintableNameCString(storageSpecifier,archiveName),
                           Error_getText(error)
                          );
                if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
                break;
              }
              if (!jobOptions->dryRunFlag)
              {
                error = File_write(&fileHandle,buffer,n);
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot write file '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Error_getText(error)
                            );
                  if (jobOptions->stopOnErrorFlag) restoreInfo->failError = error;
                  break;
                }
              }
              restoreInfo->statusInfo.entryDoneBytes += (uint64)n;
              updateStatusInfo(restoreInfo);

              length += (uint64)n;

              printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
            }
            if      (restoreInfo->failError != ERROR_NONE)
            {
              AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
              continue;
            }
            else if ((requestedAbortFlag != NULL) && (*requestedAbortFlag))
            {
              printInfo(1,"ABORTED\n");
              AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
              continue;
            }
            printInfo(2,"    \b\b\b\b");

            // set file size
#ifndef WERROR
#warning required? wrong?
#endif
            if (!jobOptions->dryRunFlag)
            {
              if (File_getSize(&fileHandle) > fileInfo.size)
              {
                File_truncate(&fileHandle,fileInfo.size);
              }
            }

            // close file
            if (!jobOptions->dryRunFlag)
            {
              (void)File_close(&fileHandle);
              AUTOFREE_REMOVE(&autoFreeList,&fileHandle);
            }

            if (fragmentNode != NULL)
            {
              // add fragment to file fragment list
              FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);
//FragmentList_debugPrintInfo(fragmentNode,String_cString(fileName));
            }

            if ((fragmentNode == NULL) || FragmentList_isEntryComplete(fragmentNode))
            {
              // set file time, file owner/group, file permission
              if (!jobOptions->dryRunFlag)
              {
                if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
                if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
                error = File_setFileInfo(destinationFileName,&fileInfo);
                if (error != ERROR_NONE)
                {
                  if (jobOptions->stopOnErrorFlag)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot set file info of '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Error_getText(error)
                              );
                    restoreInfo->failError = error;
                    AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                    continue;
                  }
                  else
                  {
                    printWarning("Cannot set file info of '%s' (error: %s)\n",
                                 String_cString(destinationFileName),
                                 Error_getText(error)
                                );
                  }
                }
              }
            }

            if (fragmentNode != NULL)
            {
              if (FragmentList_isEntryComplete(fragmentNode))
              {
                // discard fragment list
                FragmentList_discard(fragmentList,fragmentNode);
              }
            }

            if (!jobOptions->dryRunFlag)
            {
              printInfo(1,"ok\n");
            }
            else
            {
              printInfo(1,"ok (dry-run)\n");
            }

            /* check if all data read.
               Note: it is not possible to check if all data is read when
               compression is used. The decompressor may not be at the end
               of a compressed data chunk even compressed data is _not_
               corrupt.
            */
            if (   !Compress_isCompressed(archiveEntryInfo.file.deltaCompressAlgorithm)
                && !Compress_isCompressed(archiveEntryInfo.file.byteCompressAlgorithm)
                && !Archive_eofData(&archiveEntryInfo))
            {
              printWarning("unexpected data at end of file entry '%S'.\n",fileName);
            }

            // free resources
            String_delete(destinationFileName);
          }
          else
          {
            // skip
            printInfo(2,"  Restore '%s'...skipped\n",String_cString(fileName));
          }

          // close archive file, free resources
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printWarning("close 'file' entry fail (error: %s)\n",Error_getText(error));
          }

          // free resources
          File_doneExtendedAttributes(&fileExtendedAttributeList);
          String_delete(fileName);
          AutoFree_restore(&autoFreeList,autoFreeSavePoint1,FALSE);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        {
          String       deviceName;
          DeviceInfo   deviceInfo;
          uint64       blockOffset,blockCount;
          String       destinationDeviceName;
          String       parentDirectoryName;
          enum
          {
            DEVICE,
            FILE,
            UNKNOWN
          }            type;
          DeviceHandle deviceHandle;
          FileHandle   fileHandle;
          uint64       block;
          ulong        bufferBlockCount;

          autoFreeSavePoint1 = AutoFree_save(&autoFreeList);

          // read image
          deviceName = String_new();
          error = Archive_readImageEntry(&archiveEntryInfo,
                                         &archiveInfo,
                                         NULL,  // deltaCompressAlgorithm
                                         NULL,  // byteCompressAlgorithm
                                         NULL,  // cryptAlgorithm
                                         NULL,  // cryptType
                                         deviceName,
                                         &deviceInfo,
                                         NULL,  // fileSystemType
                                         NULL,  // deltaSourceName
                                         NULL,  // deltaSourceSize
                                         &blockOffset,
                                         &blockCount
                                        );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'image' content of archive '%s' (error: %s)!\n",
                       Storage_getPrintableNameCString(storageSpecifier,archiveName),
                       Error_getText(error)
                      );
            String_delete(deviceName);
            (void)Archive_closeEntry(&archiveEntryInfo);
            if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
            break;
          }
          if (deviceInfo.blockSize > BUFFER_SIZE)
          {
            printError("Device block size %llu on '%s' is too big (max: %llu)\n",
                       deviceInfo.blockSize,
                       String_cString(deviceName),
                       BUFFER_SIZE
                      );
            String_delete(deviceName);
            (void)Archive_closeEntry(&archiveEntryInfo);
            if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = ERROR_INVALID_DEVICE_BLOCK_SIZE;
            break;
          }
          assert(deviceInfo.blockSize > 0);
          AUTOFREE_ADD(&autoFreeList,deviceName,{ String_delete(deviceName); });
          AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo,{ (void)Archive_closeEntry(&archiveEntryInfo); });

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,deviceName,PATTERN_MATCH_MODE_EXACT))
              && ((excludePatternList == NULL) || !PatternList_match(excludePatternList,deviceName,PATTERN_MATCH_MODE_EXACT))
             )
          {
            String_set(restoreInfo->statusInfo.entryName,deviceName);
            restoreInfo->statusInfo.entryDoneBytes  = 0LL;
            restoreInfo->statusInfo.entryTotalBytes = blockCount;
            updateStatusInfo(restoreInfo);

            // get destination filename
            destinationDeviceName = getDestinationDeviceName(String_new(),
                                                             deviceName,
                                                             jobOptions->destination
                                                            );
            AUTOFREE_ADD(&autoFreeList,destinationDeviceName,{ String_delete(destinationDeviceName); });


            if (!jobOptions->noFragmentsCheckFlag)
            {
              // get/create image fragment node
              fragmentNode = FragmentList_find(fragmentList,deviceName);
              if (fragmentNode != NULL)
              {
                if (!jobOptions->overwriteFilesFlag && FragmentList_entryExists(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize))
                {
                  printInfo(1,
                            "  Restore image '%s'...skipped (image part %llu..%llu exists)\n",
                            String_cString(destinationDeviceName),
                            blockOffset*(uint64)deviceInfo.blockSize,
                            ((blockCount > 0) ? blockOffset+blockCount-1:blockOffset)*(uint64)deviceInfo.blockSize
                           );
                  AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                  continue;
                }
              }
              else
              {
                fragmentNode = FragmentList_add(fragmentList,deviceName,deviceInfo.size,NULL,0);
              }
              assert(fragmentNode != NULL);
            }
            else
            {
              fragmentNode = NULL;
            }

            printInfo(1,"  Restore image '%s'...",String_cString(destinationDeviceName));

            // create parent directories if not existing
            if (!jobOptions->dryRunFlag)
            {
              parentDirectoryName = File_getFilePathName(String_new(),destinationDeviceName);
              if (!String_isEmpty(parentDirectoryName) && !File_exists(parentDirectoryName))
              {
                // create directory
                error = File_makeDirectory(parentDirectoryName,
                                           FILE_DEFAULT_USER_ID,
                                           FILE_DEFAULT_GROUP_ID,
                                           FILE_DEFAULT_PERMISSION
                                          );
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot create directory '%s' (error: %s)\n",
                             String_cString(parentDirectoryName),
                             Error_getText(error)
                            );
                  String_delete(parentDirectoryName);
                  if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
                  AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                  continue;
                }

                // set directory owner ship
                error = File_setOwner(parentDirectoryName,
                                      (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? jobOptions->owner.userId  : deviceInfo.userId,
                                      (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? jobOptions->owner.groupId : deviceInfo.groupId
                                     );
                if (error != ERROR_NONE)
                {
                  if (jobOptions->stopOnErrorFlag)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                               String_cString(parentDirectoryName),
                               Error_getText(error)
                              );
                    String_delete(parentDirectoryName);
                    if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
                    AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                    continue;
                  }
                  else
                  {
                    printWarning("Cannot set owner ship of directory '%s' (error: %s)\n",
                                 String_cString(parentDirectoryName),
                                 Error_getText(error)
                                );
                  }
                }
              }
              String_delete(parentDirectoryName);
            }

            type = UNKNOWN;
            if (!jobOptions->dryRunFlag)
            {
              if (File_isDevice(destinationDeviceName))
              {
                // open device
                error = Device_open(&deviceHandle,destinationDeviceName,DEVICE_OPEN_WRITE);
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot open to device '%s' (error: %s)\n",
                             String_cString(destinationDeviceName),
                             Error_getText(error)
                            );
                }
                type = DEVICE;
                AUTOFREE_ADD(&autoFreeList,&deviceHandle,{ (void)Device_close(&deviceHandle); });
              }
              else
              {
                // temporary change owern+permission for writing (ignore errors)
                (void)File_setPermission(destinationDeviceName,FILE_PERMISSION_USER_READ|FILE_PERMISSION_USER_WRITE);
                (void)File_setOwner(destinationDeviceName,FILE_OWN_USER_ID,FILE_OWN_GROUP_ID);

                // open file
                error = File_open(&fileHandle,destinationDeviceName,FILE_OPEN_WRITE);
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot open to file '%s' (error: %s)\n",
                             String_cString(destinationDeviceName),
                             Error_getText(error)
                            );
                }
                type = FILE;
                AUTOFREE_ADD(&autoFreeList,&fileHandle,{ (void)File_close(&fileHandle); });
              }
              if (error != ERROR_NONE)
              {
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo->failError = error;
                }
                AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                continue;
              }

              // seek to fragment position
              switch (type)
              {
                case DEVICE:
                  error = Device_seek(&deviceHandle,blockOffset*(uint64)deviceInfo.blockSize);
                  if (error != ERROR_NONE)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot write to device '%s' (error: %s)\n",
                               String_cString(destinationDeviceName),
                               Error_getText(error)
                              );
                  }
                  break;
                case FILE:
                  error = File_seek(&fileHandle,blockOffset*(uint64)deviceInfo.blockSize);
                  if (error != ERROR_NONE)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot write to file '%s' (error: %s)\n",
                               String_cString(destinationDeviceName),
                               Error_getText(error)
                              );
                  }
                  break;
                default:
                  #ifndef NDEBUG
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                  #endif /* NDEBUG */
                  break;
              }
              if (error != ERROR_NONE)
              {
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo->failError = error;
                }
                AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                continue;
              }
            }

            // write image data
            block = 0LL;
            while (   ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
                   && (block < blockCount)
                  )
            {
              // pause
              while ((pauseRestoreFlag != NULL) && (*pauseRestoreFlag))
              {
                Misc_udelay(500L*1000L);
              }

              bufferBlockCount = MIN(blockCount-block,BUFFER_SIZE/deviceInfo.blockSize);

              // read data from archive
              error = Archive_readData(&archiveEntryInfo,buffer,bufferBlockCount*deviceInfo.blockSize);
              if (error != ERROR_NONE)
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot read content of archive '%s' (error: %s)!\n",
                           Storage_getPrintableNameCString(storageSpecifier,archiveName),
                           Error_getText(error)
                          );
                if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
                break;
              }

              if (!jobOptions->dryRunFlag)
              {
                // write data to device
                switch (type)
                {
                  case DEVICE:
                    error = Device_write(&deviceHandle,buffer,bufferBlockCount*deviceInfo.blockSize);
                    if (error != ERROR_NONE)
                    {
                      printInfo(1,"FAIL!\n");
                      printError("Cannot write to device '%s' (error: %s)\n",
                                 String_cString(destinationDeviceName),
                                 Error_getText(error)
                                );
                    }
                    break;
                  case FILE:
                    error = File_write(&fileHandle,buffer,bufferBlockCount*deviceInfo.blockSize);
                    if (error != ERROR_NONE)
                    {
                      printInfo(1,"FAIL!\n");
                      printError("Cannot write to file '%s' (error: %s)\n",
                                 String_cString(destinationDeviceName),
                                 Error_getText(error)
                                );
                    }
                    break;
                  default:
                    #ifndef NDEBUG
                      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                    #endif /* NDEBUG */
                    break;
                }
                if (error != ERROR_NONE)
                {
                  if (jobOptions->stopOnErrorFlag)
                  {
                    restoreInfo->failError = error;
                  }
                  break;
                }
              }
              restoreInfo->statusInfo.entryDoneBytes += bufferBlockCount*deviceInfo.blockSize;
              updateStatusInfo(restoreInfo);

              block += (uint64)bufferBlockCount;

              printInfo(2,"%3d%%\b\b\b\b",(uint)((block*100LL)/blockCount));
            }
            if      (restoreInfo->failError != ERROR_NONE)
            {
              AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
              continue;
            }
            else if ((requestedAbortFlag != NULL) && (*requestedAbortFlag))
            {
              printInfo(1,"ABORTED\n");
              AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
              continue;
            }
            printInfo(2,"    \b\b\b\b");

            // close device/file
            if (!jobOptions->dryRunFlag)
            {
              switch (type)
              {
                case DEVICE:
                  (void)Device_close(&deviceHandle);
                  AUTOFREE_REMOVE(&autoFreeList,&deviceHandle);
                  break;
                case FILE:
                  (void)File_close(&fileHandle);
                  AUTOFREE_REMOVE(&autoFreeList,&fileHandle);
                  break;
                default:
                  #ifndef NDEBUG
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                  #endif /* NDEBUG */
                  break;
              }
            }

            if (fragmentNode != NULL)
            {
              // add fragment to file fragment list
              FragmentList_addEntry(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize);
//FragmentList_debugPrintInfo(fragmentNode,String_cString(fileName));

              // discard fragment list if file is complete
              if (FragmentList_isEntryComplete(fragmentNode))
              {
                FragmentList_discard(fragmentList,fragmentNode);
              }
            }

            if (!jobOptions->dryRunFlag)
            {
              printInfo(1,"ok\n");
            }
            else
            {
              printInfo(1,"ok (dry-run)\n");
            }

            /* check if all data read.
               Note: it is not possible to check if all data is read when
               compression is used. The decompressor may not be at the end
               of a compressed data chunk even compressed data is _not_
               corrupt.
            */
            if (   !Compress_isCompressed(archiveEntryInfo.image.deltaCompressAlgorithm)
                && !Compress_isCompressed(archiveEntryInfo.image.byteCompressAlgorithm)
                && !Archive_eofData(&archiveEntryInfo))
            {
              printWarning("unexpected data at end of image entry '%S'.\n",deviceName);
            }

            // free resources
            String_delete(destinationDeviceName);
          }
          else
          {
            // skip
            printInfo(2,"  Restore '%s'...skipped\n",String_cString(deviceName));
          }

          // close archive file
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printWarning("close 'image' entry fail (error: %s)\n",Error_getText(error));
          }

          // free resources
          String_delete(deviceName);
          AutoFree_restore(&autoFreeList,autoFreeSavePoint1,FALSE);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        {
          String                    directoryName;
          FileExtendedAttributeList fileExtendedAttributeList;
          FileInfo                  fileInfo;
          String                    destinationFileName;
//            FileInfo localFileInfo;

          autoFreeSavePoint1 = AutoFree_save(&autoFreeList);

          // read directory
          directoryName = String_new();
          File_initExtendedAttributes(&fileExtendedAttributeList);
          error = Archive_readDirectoryEntry(&archiveEntryInfo,
                                             &archiveInfo,
                                             NULL,  // cryptAlgorithm
                                             NULL,  // cryptType
                                             directoryName,
                                             &fileInfo,
                                             &fileExtendedAttributeList
                                            );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'directory' content of archive '%s' (error: %s)!\n",
                       Storage_getPrintableNameCString(storageSpecifier,archiveName),
                       Error_getText(error)
                      );
            String_delete(directoryName);
            if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
            break;
          }
          AUTOFREE_ADD(&autoFreeList,directoryName,{ String_delete(directoryName); });
          AUTOFREE_ADD(&autoFreeList,&fileExtendedAttributeList,{ File_doneExtendedAttributes(&fileExtendedAttributeList); });
          AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo,{ (void)Archive_closeEntry(&archiveEntryInfo); });

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
              && ((excludePatternList == NULL) || !PatternList_match(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT))
             )
          {
            String_set(restoreInfo->statusInfo.entryName,directoryName);
            restoreInfo->statusInfo.entryDoneBytes  = 0LL;
            restoreInfo->statusInfo.entryTotalBytes = 0LL;
            updateStatusInfo(restoreInfo);

            // get destination filename
            destinationFileName = getDestinationFileName(String_new(),
                                                         directoryName,
                                                         jobOptions->destination,
                                                         jobOptions->directoryStripCount
                                                        );
            AUTOFREE_ADD(&autoFreeList,destinationFileName,{ String_delete(destinationFileName); });


            // check if directory already exists
            if (!jobOptions->overwriteFilesFlag && File_exists(destinationFileName))
            {
              printInfo(1,
                        "  Restore directory '%s'...skipped (file exists)\n",
                        String_cString(destinationFileName)
                       );
              AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
              continue;
            }

            printInfo(1,"  Restore directory '%s'...",String_cString(destinationFileName));

            // create directory
            if (!jobOptions->dryRunFlag)
            {
              error = File_makeDirectory(destinationFileName,
                                         FILE_DEFAULT_USER_ID,
                                         FILE_DEFAULT_GROUP_ID,
                                         fileInfo.permission
                                        );
              if (error != ERROR_NONE)
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot create directory '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Error_getText(error)
                          );
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo->failError = error;
                }
                AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                continue;
              }
            }

            // set file time, file owner/group
            if (!jobOptions->dryRunFlag)
            {
              if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
              if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
              error = File_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                if (jobOptions->stopOnErrorFlag)
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot set directory info of '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Error_getText(error)
                            );
                  if (jobOptions->stopOnErrorFlag)
                  {
                    restoreInfo->failError = error;
                  }
                  AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                  continue;
                }
                else
                {
                  printWarning("Cannot set directory info of '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Error_getText(error)
                              );
                }
              }
            }

            if (!jobOptions->dryRunFlag)
            {
              printInfo(1,"ok\n");
            }
            else
            {
              printInfo(1,"ok (dry-run)\n");
            }

            // check if all data read
            if (!Archive_eofData(&archiveEntryInfo))
            {
              printWarning("unexpected data at end of directory entry '%S'.\n",directoryName);
            }

            // free resources
            String_delete(destinationFileName);
          }
          else
          {
            // skip
            printInfo(2,"  Restore '%s'...skipped\n",String_cString(directoryName));
          }

          // close archive file
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printWarning("close 'directory' entry fail (error: %s)\n",Error_getText(error));
          }

          // free resources
          File_doneExtendedAttributes(&fileExtendedAttributeList);
          String_delete(directoryName);
          AutoFree_restore(&autoFreeList,autoFreeSavePoint1,FALSE);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_LINK:
        {
          String                    linkName;
          String                    fileName;
          FileExtendedAttributeList fileExtendedAttributeList;
          FileInfo                  fileInfo;
          String                    destinationFileName;
          String                    parentDirectoryName;
//            FileInfo localFileInfo;

          autoFreeSavePoint1 = AutoFree_save(&autoFreeList);

          // read link
          linkName = String_new();
          fileName = String_new();
          File_initExtendedAttributes(&fileExtendedAttributeList);
          error = Archive_readLinkEntry(&archiveEntryInfo,
                                        &archiveInfo,
                                        NULL,  // cryptAlgorithm
                                        NULL,  // cryptType
                                        linkName,
                                        fileName,
                                        &fileInfo,
                                        &fileExtendedAttributeList
                                       );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'link' content of archive '%s' (error: %s)!\n",
                       Storage_getPrintableNameCString(storageSpecifier,archiveName),
                       Error_getText(error)
                      );
            File_doneExtendedAttributes(&fileExtendedAttributeList);
            String_delete(fileName);
            String_delete(linkName);
            if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
            break;
          }
          AUTOFREE_ADD(&autoFreeList,linkName,{ String_delete(linkName); });
          AUTOFREE_ADD(&autoFreeList,fileName,{ String_delete(fileName); });
          AUTOFREE_ADD(&autoFreeList,&fileExtendedAttributeList,{ File_doneExtendedAttributes(&fileExtendedAttributeList); });
          AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo,{ Archive_closeEntry(&archiveEntryInfo); });

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
              && ((excludePatternList == NULL) || !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT))
             )
          {
            String_set(restoreInfo->statusInfo.entryName,linkName);
            restoreInfo->statusInfo.entryDoneBytes  = 0LL;
            restoreInfo->statusInfo.entryTotalBytes = 0LL;
            updateStatusInfo(restoreInfo);

            // get destination filename
            destinationFileName = getDestinationFileName(String_new(),
                                                         linkName,
                                                         jobOptions->destination,
                                                         jobOptions->directoryStripCount
                                                        );
            AUTOFREE_ADD(&autoFreeList,destinationFileName,{ String_delete(destinationFileName); });

            // create parent directories if not existing
            if (!jobOptions->dryRunFlag)
            {
              parentDirectoryName = File_getFilePathName(String_new(),destinationFileName);
              if (!String_isEmpty(parentDirectoryName) && !File_exists(parentDirectoryName))
              {
                // create directory
                error = File_makeDirectory(parentDirectoryName,
                                           FILE_DEFAULT_USER_ID,
                                           FILE_DEFAULT_GROUP_ID,
                                           FILE_DEFAULT_PERMISSION
                                          );
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot create directory '%s' (error: %s)\n",
                             String_cString(parentDirectoryName),
                             Error_getText(error)
                            );
                  String_delete(parentDirectoryName);
                  if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
                  AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                  continue;
                }

                // set directory owner ship
                error = File_setOwner(parentDirectoryName,
                                      (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? jobOptions->owner.userId  : fileInfo.userId,
                                      (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? jobOptions->owner.groupId : fileInfo.groupId
                                     );
                if (error != ERROR_NONE)
                {
                  if (jobOptions->stopOnErrorFlag)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                               String_cString(parentDirectoryName),
                               Error_getText(error)
                              );
                    String_delete(parentDirectoryName);
                    if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
                    AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                    continue;
                  }
                  else
                  {
                    printWarning("Cannot set owner ship of directory '%s' (error: %s)\n",
                                 String_cString(parentDirectoryName),
                                 Error_getText(error)
                                );
                  }
                }
              }
              String_delete(parentDirectoryName);
            }

            // check if link areadly exists
            if (!jobOptions->overwriteFilesFlag && File_exists(destinationFileName))
            {
              printInfo(1,
                        "  Restore link '%s'...skipped (file exists)\n",
                        String_cString(destinationFileName)
                       );
              if (jobOptions->stopOnErrorFlag)
              {
                restoreInfo->failError = ERRORX_(FILE_EXISTS_,0,"%s",String_cString(destinationFileName));
              }
              AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
              continue;
            }

            printInfo(1,"  Restore link '%s'...",String_cString(destinationFileName));

            // create link
            if (!jobOptions->dryRunFlag)
            {
              error = File_makeLink(destinationFileName,fileName);
              if (error != ERROR_NONE)
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot create link '%s' -> '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           String_cString(fileName),
                           Error_getText(error)
                          );
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo->failError = error;
                }
                AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                continue;
              }
            }

            // set file time, file owner/group
            if (!jobOptions->dryRunFlag)
            {
              if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
              if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
              error = File_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                if (jobOptions->stopOnErrorFlag)
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot set file info of '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Error_getText(error)
                            );
                  if (jobOptions->stopOnErrorFlag)
                  {
                    restoreInfo->failError = error;
                  }
                  AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                  continue;
                }
                else
                {
                  printWarning("Cannot set file info of '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Error_getText(error)
                              );
                }
              }
            }

            if (!jobOptions->dryRunFlag)
            {
              printInfo(1,"ok\n");
            }
            else
            {
              printInfo(1,"ok (dry-run)\n");
            }

            // check if all data read
            if (!Archive_eofData(&archiveEntryInfo))
            {
              printWarning("unexpected data at end of link entry '%S'.\n",linkName);
            }

            // free resources
            String_delete(destinationFileName);
          }
          else
          {
            // skip
            printInfo(2,"  Restore '%s'...skipped\n",String_cString(linkName));
          }

          // close archive file
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printWarning("close 'link' entry fail (error: %s)\n",Error_getText(error));
          }

          // free resources
          File_doneExtendedAttributes(&fileExtendedAttributeList);
          String_delete(fileName);
          String_delete(linkName);
          AutoFree_restore(&autoFreeList,autoFreeSavePoint1,FALSE);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        {
          StringList                fileNameList;
          FileExtendedAttributeList fileExtendedAttributeList;
          FileInfo                  fileInfo;
          uint64                    fragmentOffset,fragmentSize;
          String                    hardLinkFileName;
          String                    destinationFileName;
          bool                      restoredDataFlag;
          const StringNode          *stringNode;
          String                    fileName;
          String                    parentDirectoryName;
//            FileInfo                  localFileInfo;
          FileHandle                fileHandle;
          uint64                    length;
          ulong                     n;

          autoFreeSavePoint1 = AutoFree_save(&autoFreeList);

          // read hard link
          StringList_init(&fileNameList);
          File_initExtendedAttributes(&fileExtendedAttributeList);
          error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                            &archiveInfo,
                                            NULL,  // deltaCompressAlgorithm
                                            NULL,  // byteCompressAlgorithm
                                            NULL,  // cryptAlgorithm
                                            NULL,  // cryptType
                                            &fileNameList,
                                            &fileInfo,
                                            &fileExtendedAttributeList,
                                            NULL,  // deltaSourceName
                                            NULL,  // deltaSourceSize
                                            &fragmentOffset,
                                            &fragmentSize
                                           );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'hard link' content of archive '%s' (error: %s)!\n",
                       Storage_getPrintableNameCString(storageSpecifier,archiveName),
                       Error_getText(error)
                      );
            StringList_done(&fileNameList);
            if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
            AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
            continue;
          }
          AUTOFREE_ADD(&autoFreeList,&fileNameList,{ StringList_done(&fileNameList); });
          AUTOFREE_ADD(&autoFreeList,&fileExtendedAttributeList,{ File_doneExtendedAttributes(&fileExtendedAttributeList); });
          AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo,{ Archive_closeEntry(&archiveEntryInfo); });

          hardLinkFileName    = String_new();
          destinationFileName = String_new();
          AUTOFREE_ADD(&autoFreeList,hardLinkFileName,{ String_delete(hardLinkFileName); });
          AUTOFREE_ADD(&autoFreeList,destinationFileName,{ String_delete(destinationFileName); });
          restoredDataFlag    = FALSE;
          autoFreeSavePoint2  = AutoFree_save(&autoFreeList);
          STRINGLIST_ITERATE(&fileNameList,stringNode,fileName)
          {
            if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                && ((excludePatternList == NULL) || !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
               )
            {
              String_set(restoreInfo->statusInfo.entryName,fileName);
              restoreInfo->statusInfo.entryDoneBytes  = 0LL;
              restoreInfo->statusInfo.entryTotalBytes = fragmentSize;
              updateStatusInfo(restoreInfo);

              // get destination filename
              getDestinationFileName(destinationFileName,
                                     fileName,
                                     jobOptions->destination,
                                     jobOptions->directoryStripCount
                                    );

              printInfo(1,"  Restore hard link '%s'...",String_cString(destinationFileName));

              // create parent directories if not existing
              if (!jobOptions->dryRunFlag)
              {
                parentDirectoryName = File_getFilePathName(String_new(),destinationFileName);
                if (!String_isEmpty(parentDirectoryName) && !File_exists(parentDirectoryName))
                {
                  // create directory
                  error = File_makeDirectory(parentDirectoryName,
                                             FILE_DEFAULT_USER_ID,
                                             FILE_DEFAULT_GROUP_ID,
                                             FILE_DEFAULT_PERMISSION
                                            );
                  if (error != ERROR_NONE)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot create directory '%s' (error: %s)\n",
                               String_cString(parentDirectoryName),
                               Error_getText(error)
                              );
                    String_delete(parentDirectoryName);
                    if (jobOptions->stopOnErrorFlag)
                    {
                      restoreInfo->failError = error;
                      break;
                    }
                    else
                    {
                      AutoFree_restore(&autoFreeList,autoFreeSavePoint2,TRUE);
                      continue;
                    }
                  }

                  // set directory owner ship
                  error = File_setOwner(parentDirectoryName,
                                        (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? jobOptions->owner.userId  : fileInfo.userId,
                                        (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? jobOptions->owner.groupId : fileInfo.groupId
                                       );
                  if (error != ERROR_NONE)
                  {
                    if (jobOptions->stopOnErrorFlag)
                    {
                      printInfo(1,"FAIL!\n");
                      printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                                 String_cString(parentDirectoryName),
                                 Error_getText(error)
                                );
                      String_delete(parentDirectoryName);
                      restoreInfo->failError = error;
                      break;
                    }
                    else
                    {
                      printWarning("Cannot set owner ship of directory '%s' (error: %s)\n",
                                   String_cString(parentDirectoryName),
                                   Error_getText(error)
                                  );
                    }
                  }
                }
                String_delete(parentDirectoryName);
              }

              if (!restoredDataFlag)
              {
                if (!jobOptions->noFragmentsCheckFlag)
                {
                  // check if file fragment already eixsts, file already exists
                  fragmentNode = FragmentList_find(fragmentList,fileName);
                  if (fragmentNode != NULL)
                  {
                    if (!jobOptions->overwriteFilesFlag && FragmentList_entryExists(fragmentNode,fragmentOffset,fragmentSize))
                    {
                      printInfo(1,"skipped (file part %llu..%llu exists)\n",
                                String_cString(destinationFileName),
                                fragmentOffset,
                                (fragmentSize > 0LL) ? fragmentOffset+fragmentSize-1:fragmentOffset
                               );
                      AutoFree_restore(&autoFreeList,autoFreeSavePoint2,TRUE);
                      continue;
                    }
                  }
                  else
                  {
                    if (!jobOptions->overwriteFilesFlag && File_exists(destinationFileName))
                    {
                      printInfo(1,"skipped (file exists)\n",String_cString(destinationFileName));
                      AutoFree_restore(&autoFreeList,autoFreeSavePoint2,TRUE);
                      continue;
                    }
                    fragmentNode = FragmentList_add(fragmentList,fileName,fileInfo.size,&fileInfo,sizeof(FileInfo));
                  }
                  assert(fragmentNode != NULL);
                }
                else
                {
                  fragmentNode = NULL;
                }

                if (!jobOptions->dryRunFlag)
                {
                  // temporary change owern+permission for writing (ignore errors)
                  (void)File_setPermission(destinationFileName,FILE_PERMISSION_USER_READ|FILE_PERMISSION_USER_WRITE);
                  (void)File_setOwner(destinationFileName,FILE_OWN_USER_ID,FILE_OWN_GROUP_ID);

                  // open file
                  error = File_open(&fileHandle,destinationFileName,FILE_OPEN_WRITE);
                  if (error != ERROR_NONE)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot create/write to file '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Error_getText(error)
                              );
                    if (jobOptions->stopOnErrorFlag)
                    {
                      restoreInfo->failError = error;
                      break;
                    }
                    else
                    {
                      AutoFree_restore(&autoFreeList,autoFreeSavePoint2,TRUE);
                      continue;
                    }
                  }
                  AUTOFREE_ADD(&autoFreeList,&fileHandle,{ (void)File_close(&fileHandle); });

                  // seek to fragment position
                  error = File_seek(&fileHandle,fragmentOffset);
                  if (error != ERROR_NONE)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot write file '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Error_getText(error)
                              );
                    File_close(&fileHandle);
                    if (jobOptions->stopOnErrorFlag)
                    {
                      restoreInfo->failError = error;
                      break;
                    }
                    else
                    {
                      AutoFree_restore(&autoFreeList,autoFreeSavePoint2,TRUE);
                      continue;
                    }
                  }
                  String_set(hardLinkFileName,destinationFileName);
                }

                // write file data
                length = 0LL;
                while (   ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
                       && (length < fragmentSize)
                      )
                {
                  // pause
                  while ((pauseRestoreFlag != NULL) && (*pauseRestoreFlag))
                  {
                    Misc_udelay(500L*1000L);
                  }

                  n = (ulong)MIN(fragmentSize-length,BUFFER_SIZE);

                  error = Archive_readData(&archiveEntryInfo,buffer,n);
                  if (error != ERROR_NONE)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot read content of archive '%s' (error: %s)!\n",
                               Storage_getPrintableNameCString(storageSpecifier,archiveName),
                               Error_getText(error)
                              );
                    restoreInfo->failError = error;
                    break;
                  }
                  if (!jobOptions->dryRunFlag)
                  {
                    error = File_write(&fileHandle,buffer,n);
                    if (error != ERROR_NONE)
                    {
                      printInfo(1,"FAIL!\n");
                      printError("Cannot write file '%s' (error: %s)\n",
                                 String_cString(destinationFileName),
                                 Error_getText(error)
                                );
                      if (jobOptions->stopOnErrorFlag)
                      {
                        restoreInfo->failError = error;
                      }
                      break;
                    }
                  }
                  restoreInfo->statusInfo.entryDoneBytes += (uint64)n;
                  updateStatusInfo(restoreInfo);

                  length += (uint64)n;

                  printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
                }
                if      (restoreInfo->failError != ERROR_NONE)
                {
                  if (!jobOptions->dryRunFlag)
                  {
                    (void)File_close(&fileHandle);
                  }
                  break;
                }
                else if ((requestedAbortFlag != NULL) && (*requestedAbortFlag))
                {
                  printInfo(1,"ABORTED\n");
                  if (!jobOptions->dryRunFlag)
                  {
                    (void)File_close(&fileHandle);
                  }
                  break;
                }
                printInfo(2,"    \b\b\b\b");

                // set file size
#ifndef WERROR
#warning required? wrong?
#endif
                if (!jobOptions->dryRunFlag)
                {
                  if (File_getSize(&fileHandle) > fileInfo.size)
                  {
                    File_truncate(&fileHandle,fileInfo.size);
                  }
                }

                // close file
                if (!jobOptions->dryRunFlag)
                {
                  (void)File_close(&fileHandle);
                  AUTOFREE_REMOVE(&autoFreeList,&fileHandle);
                }

                if (fragmentNode != NULL)
                {
                  // add fragment to file fragment list
                  FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);
//FragmentList_debugPrintInfo(fragmentNode,String_cString(fileName));
                }

                if ((fragmentNode == NULL) || FragmentList_isEntryComplete(fragmentNode))
                {
                  // set file time, file owner/group
                  if (!jobOptions->dryRunFlag)
                  {
                    if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
                    if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
                    error = File_setFileInfo(destinationFileName,&fileInfo);
                    if (error != ERROR_NONE)
                    {
                      if (jobOptions->stopOnErrorFlag)
                      {
                        printInfo(1,"FAIL!\n");
                        printError("Cannot set file info of '%s' (error: %s)\n",
                                   String_cString(destinationFileName),
                                   Error_getText(error)
                                  );
                        restoreInfo->failError = error;
                        break;
                      }
                      else
                      {
                        printWarning("Cannot set file info of '%s' (error: %s)\n",
                                     String_cString(destinationFileName),
                                     Error_getText(error)
                                    );
                      }
                    }
                  }
                }

                if (fragmentNode != NULL)
                {
                  // discard fragment list if file is complete
                  if (FragmentList_isEntryComplete(fragmentNode))
                  {
                    FragmentList_discard(fragmentList,fragmentNode);
                  }
                }

                if (!jobOptions->dryRunFlag)
                {
                  printInfo(1,"ok\n");
                }
                else
                {
                  printInfo(1,"ok (dry-run)\n");
                }

                restoredDataFlag = TRUE;
              }
              else
              {
                // check file if exists
                if (!jobOptions->overwriteFilesFlag && File_exists(destinationFileName))
                {
                  printInfo(1,"skipped (file exists)\n",String_cString(destinationFileName));
                  AutoFree_restore(&autoFreeList,autoFreeSavePoint2,TRUE);
                  continue;
                }

                // create hard link
                if (!jobOptions->dryRunFlag)
                {
                  error = File_makeHardLink(destinationFileName,hardLinkFileName);
                  if (error != ERROR_NONE)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot create/write to file '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Error_getText(error)
                              );
                    if (jobOptions->stopOnErrorFlag)
                    {
                      restoreInfo->failError = error;
                      break;
                    }
                    else
                    {
                      AutoFree_restore(&autoFreeList,autoFreeSavePoint2,TRUE);
                      continue;
                    }
                  }
                }

                if (!jobOptions->dryRunFlag)
                {
                  printInfo(1,"ok\n");
                }
                else
                {
                  printInfo(1,"ok (dry-run)\n");
                }

                /* check if all data read.
                   Note: it is not possible to check if all data is read when
                   compression is used. The decompressor may not be at the end
                   of a compressed data chunk even compressed data is _not_
                   corrupt.
                */
                if (   !Compress_isCompressed(archiveEntryInfo.hardLink.deltaCompressAlgorithm)
                    && !Compress_isCompressed(archiveEntryInfo.hardLink.byteCompressAlgorithm)
                    && !Archive_eofData(&archiveEntryInfo))
                {
                  printWarning("unexpected data at end of hard link entry '%S'.\n",fileName);
                }
              }
            }
            else
            {
              // skip
              printInfo(2,"  Restore '%s'...skipped\n",String_cString(fileName));
            }
          }
          AutoFree_restore(&autoFreeList,autoFreeSavePoint2,FALSE);
          String_delete(destinationFileName);
          String_delete(hardLinkFileName);
          AUTOFREE_REMOVE(&autoFreeList,destinationFileName);
          AUTOFREE_REMOVE(&autoFreeList,hardLinkFileName);
          if (restoreInfo->failError != ERROR_NONE)
          {
            AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
            continue;
          }

          // close archive file
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printWarning("close 'hard link' entry fail (error: %s)\n",Error_getText(error));
          }

          // free resources
          File_doneExtendedAttributes(&fileExtendedAttributeList);
          StringList_done(&fileNameList);
          AutoFree_restore(&autoFreeList,autoFreeSavePoint1,FALSE);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
        {
          String                    fileName;
          FileExtendedAttributeList fileExtendedAttributeList;
          FileInfo                  fileInfo;
          String                    destinationFileName;
          String                    parentDirectoryName;
//            FileInfo localFileInfo;

          autoFreeSavePoint1 = AutoFree_save(&autoFreeList);

          // read special device
          fileName = String_new();
          File_initExtendedAttributes(&fileExtendedAttributeList);
          error = Archive_readSpecialEntry(&archiveEntryInfo,
                                           &archiveInfo,
                                           NULL,  // cryptAlgorithm
                                           NULL,  // cryptType
                                           fileName,
                                           &fileInfo,
                                           &fileExtendedAttributeList
                                          );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'special' content of archive '%s' (error: %s)!\n",
                       Storage_getPrintableNameCString(storageSpecifier,archiveName),
                       Error_getText(error)
                      );
            String_delete(fileName);
            if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
            break;
          }
          AUTOFREE_ADD(&autoFreeList,fileName,{ String_delete(fileName); });
          AUTOFREE_ADD(&autoFreeList,&fileExtendedAttributeList,{ File_doneExtendedAttributes(&fileExtendedAttributeList); });
          AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo,{ Archive_closeEntry(&archiveEntryInfo); });

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
              && ((excludePatternList == NULL) || !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
             )
          {
            String_set(restoreInfo->statusInfo.entryName,fileName);
            restoreInfo->statusInfo.entryDoneBytes  = 0LL;
            restoreInfo->statusInfo.entryTotalBytes = 0LL;
            updateStatusInfo(restoreInfo);

            // get destination filename
            destinationFileName = getDestinationFileName(String_new(),
                                                         fileName,
                                                         jobOptions->destination,
                                                         jobOptions->directoryStripCount
                                                        );
            AUTOFREE_ADD(&autoFreeList,destinationFileName,{ String_delete(destinationFileName); });

            // create parent directories if not existing
            if (!jobOptions->dryRunFlag)
            {
              parentDirectoryName = File_getFilePathName(String_new(),destinationFileName);
              if (!String_isEmpty(parentDirectoryName) && !File_exists(parentDirectoryName))
              {
                // create directory
                error = File_makeDirectory(parentDirectoryName,
                                           FILE_DEFAULT_USER_ID,
                                           FILE_DEFAULT_GROUP_ID,
                                           FILE_DEFAULT_PERMISSION
                                          );
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot create directory '%s' (error: %s)\n",
                             String_cString(parentDirectoryName),
                             Error_getText(error)
                            );
                  String_delete(parentDirectoryName);
                  if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
                  AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                  continue;
                }

                // set directory owner ship
                error = File_setOwner(parentDirectoryName,
                                      (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? jobOptions->owner.userId  : fileInfo.userId,
                                      (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? jobOptions->owner.groupId : fileInfo.groupId
                                     );
                if (error != ERROR_NONE)
                {
                  if (jobOptions->stopOnErrorFlag)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                               String_cString(parentDirectoryName),
                               Error_getText(error)
                              );
                    String_delete(parentDirectoryName);
                    if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
                    AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                    continue;
                  }
                  else
                  {
                    printWarning("Cannot set owner ship of directory '%s' (error: %s)\n",
                                 String_cString(parentDirectoryName),
                                 Error_getText(error)
                                );
                  }
                }
              }
              String_delete(parentDirectoryName);
            }

            // check if special file already exists
            if (!jobOptions->overwriteFilesFlag && File_exists(destinationFileName))
            {
              printInfo(1,
                        "  Restore special device '%s'...skipped (file exists)\n",
                        String_cString(destinationFileName)
                       );
              if (jobOptions->stopOnErrorFlag)
              {
                restoreInfo->failError = ERRORX_(FILE_EXISTS_,0,"%s",String_cString(destinationFileName));
              }
              AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
              continue;
            }

            printInfo(1,"  Restore special device '%s'...",String_cString(destinationFileName));

            // create special device
            if (!jobOptions->dryRunFlag)
            {
              error = File_makeSpecial(destinationFileName,
                                       fileInfo.specialType,
                                       fileInfo.major,
                                       fileInfo.minor
                                      );
              if (error != ERROR_NONE)
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot create special device '%s' (error: %s)\n",
                           String_cString(fileName),
                           Error_getText(error)
                          );
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo->failError = error;
                }
                AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                continue;
              }
            }

            // set file time, file owner/group
            if (!jobOptions->dryRunFlag)
            {
              if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
              if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
              error = File_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                if (jobOptions->stopOnErrorFlag)
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot set file info of '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Error_getText(error)
                            );
                  if (jobOptions->stopOnErrorFlag)
                  {
                    restoreInfo->failError = error;
                  }
                  AutoFree_restore(&autoFreeList,autoFreeSavePoint1,TRUE);
                  continue;
                }
                else
                {
                  printWarning("Cannot set file info of '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Error_getText(error)
                              );
                }
              }
            }

            if (!jobOptions->dryRunFlag)
            {
              printInfo(1,"ok\n");
            }
            else
            {
              printInfo(1,"ok (dry-run)\n");
            }

            // check if all data read
            if (!Archive_eofData(&archiveEntryInfo))
            {
              printWarning("unexpected data at end of special entry '%S'.\n",fileName);
            }

            // free resources
            String_delete(destinationFileName);
          }
          else
          {
            // skip
            printInfo(2,"  Restore '%s'...skipped\n",String_cString(fileName));
          }

          // close archive file
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printWarning("close 'special' entry fail (error: %s)\n",Error_getText(error));
          }

          // free resources
          File_doneExtendedAttributes(&fileExtendedAttributeList);
          String_delete(fileName);
          AutoFree_restore(&autoFreeList,autoFreeSavePoint1,FALSE);
        }
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; /* not reached */
    }

    // update storage status
    restoreInfo->statusInfo.storageDoneBytes = Archive_tell(&archiveInfo);
    updateStatusInfo(restoreInfo);
  }

  // close archive
  Archive_close(&archiveInfo);

  // done storage
  (void)Storage_done(&storageHandle);

  // free resources
  free(buffer);
  AutoFree_done(&autoFreeList);

  return error;
}

/*---------------------------------------------------------------------*/

Errors Command_restore(const StringList                *storageNameList,
                       const EntryList                 *includeEntryList,
                       const PatternList               *excludePatternList,
                       DeltaSourceList                 *deltaSourceList,
                       JobOptions                      *jobOptions,
                       ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                       void                            *archiveGetCryptPasswordUserData,
                       RestoreStatusInfoFunction       restoreStatusInfoFunction,
                       void                            *restoreStatusInfoUserData,
                       bool                            *pauseRestoreFlag,
                       bool                            *requestedAbortFlag,
                       LogHandle                       *logHandle
                      )
{
  RestoreInfo                restoreInfo;
  FragmentList               fragmentList;
  StorageSpecifier           storageSpecifier;
  StringNode                 *stringNode;
  String                     storageName;
  bool                       abortFlag;
  Errors                     error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  Pattern                    pattern;
  String                     fileName;
  FragmentNode               *fragmentNode;

  assert(storageNameList != NULL);
  assert(includeEntryList != NULL);
  assert(jobOptions != NULL);

  // init variables
  restoreInfo.failError                     = ERROR_NONE;
  restoreInfo.statusInfoFunction            = restoreStatusInfoFunction;
  restoreInfo.statusInfoUserData            = restoreStatusInfoUserData;
  restoreInfo.statusInfo.storageName        = String_new();
  restoreInfo.statusInfo.storageDoneBytes   = 0LL;
  restoreInfo.statusInfo.storageTotalBytes  = 0LL;
  restoreInfo.statusInfo.doneEntries        = 0L;
  restoreInfo.statusInfo.doneBytes          = 0LL;
  restoreInfo.statusInfo.skippedEntries     = 0L;
  restoreInfo.statusInfo.skippedBytes       = 0LL;
  restoreInfo.statusInfo.errorEntries       = 0L;
  restoreInfo.statusInfo.errorBytes         = 0LL;
  restoreInfo.statusInfo.entryName          = String_new();
  restoreInfo.statusInfo.entryDoneBytes     = 0LL;
  restoreInfo.statusInfo.entryTotalBytes    = 0LL;
  FragmentList_init(&fragmentList);
  Storage_initSpecifier(&storageSpecifier);

  error     = ERROR_NONE;
  abortFlag = FALSE;
  STRINGLIST_ITERATE(storageNameList,stringNode,storageName)
  {
    // pause
    while ((pauseRestoreFlag != NULL) && (*pauseRestoreFlag))
    {
      Misc_udelay(500L*1000L);
    }

    // parse storage name
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error != ERROR_NONE)
    {
      printError("Invalid storage '%s' (error: %s)!\n",
                 String_cString(storageName),
                 Error_getText(error)
                );
      if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
      continue;
    }

    if (String_isEmpty(storageSpecifier.archivePatternString))
    {
      // restore archive content
      error = restoreArchiveContent(&storageSpecifier,
                                    NULL,  // archiveName
                                    includeEntryList,
                                    excludePatternList,
                                    deltaSourceList,
                                    jobOptions,
                                    archiveGetCryptPasswordFunction,
                                    archiveGetCryptPasswordUserData,
                                    pauseRestoreFlag,
                                    requestedAbortFlag,
                                    &fragmentList,
                                    &restoreInfo,
                                    logHandle
                                   );
      if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
    }
    else
    {
      // restore all matching archives content
      error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                        &storageSpecifier,
                                        jobOptions,
                                        SERVER_CONNECTION_PRIORITY_HIGH,
                                        NULL  // archiveName
                                       );
      if (error == ERROR_NONE)
      {
        error = Pattern_init(&pattern,storageSpecifier.archivePatternString,
                             jobOptions->patternType,
                             PATTERN_FLAG_NONE
                            );
        if (error == ERROR_NONE)
        {
          fileName = String_new();
          while (!Storage_endOfDirectoryList(&storageDirectoryListHandle) && (error == ERROR_NONE))
          {
            // read next directory entry
            error = Storage_readDirectoryList(&storageDirectoryListHandle,fileName,NULL);
            if (error != ERROR_NONE)
            {
              continue;
            }

            // match pattern
            if (!Pattern_match(&pattern,fileName,PATTERN_MATCH_MODE_EXACT))
            {
              continue;
            }

            // restore archive content
            error = restoreArchiveContent(&storageSpecifier,
                                          fileName,
                                          includeEntryList,
                                          excludePatternList,
                                          deltaSourceList,
                                          jobOptions,
                                          archiveGetCryptPasswordFunction,
                                          archiveGetCryptPasswordUserData,
                                          pauseRestoreFlag,
                                          requestedAbortFlag,
                                          &fragmentList,
                                          &restoreInfo,
                                          logHandle
                                         );
            if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
          }
          String_delete(fileName);
          Pattern_done(&pattern);
        }

        Storage_closeDirectoryList(&storageDirectoryListHandle);
      }
    }

    if (   abortFlag
        || ((requestedAbortFlag != NULL) && (*requestedAbortFlag))
        || (restoreInfo.failError != ERROR_NONE)
       )
    {
      break;
    }
  }

  if (   (error == ERROR_NONE)
      && (restoreInfo.failError == ERROR_NONE)
      && !jobOptions->noFragmentsCheckFlag
     )
  {
    // check fragment lists, set file info also for incomplete entries
    if ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
    {
      FRAGMENTLIST_ITERATE(&fragmentList,fragmentNode)
      {
        if (!FragmentList_isEntryComplete(fragmentNode))
        {
          printInfo(0,"Warning: incomplete entry '%s'\n",String_cString(fragmentNode->name));
          if (isPrintInfo(2))
          {
            printInfo(2,"  Fragments:\n");
            FragmentList_print(stdout,4,fragmentNode);
          }
          if (restoreInfo.failError == ERROR_NONE)
          {
            restoreInfo.failError = ERRORX_(ENTRY_INCOMPLETE,0,"%s",String_cString(fragmentNode->name));
          }
        }

        if (fragmentNode->userData != NULL)
        {
          // set file time, file owner/group, file permission
          if (!jobOptions->dryRunFlag)
          {
            if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ((FileInfo*)fragmentNode->userData)->userId  = jobOptions->owner.userId;
            if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ((FileInfo*)fragmentNode->userData)->groupId = jobOptions->owner.groupId;
            error = File_setFileInfo(fragmentNode->name,(FileInfo*)fragmentNode->userData);
            if (error != ERROR_NONE)
            {
              if (jobOptions->stopOnErrorFlag)
              {
                printError("Cannot set file info of '%s' (error: %s)\n",
                           String_cString(fragmentNode->name),
                           Error_getText(error)
                          );
                restoreInfo.failError = error;
              }
              else
              {
                printWarning("Cannot set file info of '%s' (error: %s)\n",
                             String_cString(fragmentNode->name),
                             Error_getText(error)
                            );
              }
            }
          }
        }
      }
    }
  }

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  FragmentList_done(&fragmentList);
  String_delete(restoreInfo.statusInfo.entryName);
  String_delete(restoreInfo.statusInfo.storageName);

  if ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
  {
    return restoreInfo.failError;
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
