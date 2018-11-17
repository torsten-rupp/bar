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

#include "common/global.h"
#include "common/autofree.h"
#include "strings.h"
#include "common/stringlists.h"

#include "errors.h"
#include "common/patterns.h"
#include "common/patternlists.h"
#include "deltasourcelists.h"
#include "common/files.h"
#include "archive.h"
#include "common/fragmentlists.h"
#include "common/misc.h"

#include "commands_restore.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// file data buffer size
#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/

// restore information
typedef struct
{
  StorageSpecifier                *storageSpecifier;             // storage specifier structure
  const EntryList                 *includeEntryList;             // list of included entries
  const PatternList               *excludePatternList;           // list of exclude patterns
  DeltaSourceList                 *deltaSourceList;              // delta sources
  const JobOptions                *jobOptions;
  bool                            dryRun;                        //

  RestoreUpdateStatusInfoFunction updateStatusInfoFunction;      // update status info call-back
  void                            *updateStatusInfoUserData;     // user data for update status info call-back
  RestoreHandleErrorFunction      handleErrorFunction;           // handle error call-back
  void                            *handleErrorUserData;          // user data for handle error call-back
  GetNamePasswordFunction         getNamePasswordFunction;       // get name/password call-back
  void                            *getNamePasswordUserData;      // user data for get password call-back
  IsPauseFunction                 isPauseFunction;               // check for pause call-back
  void                            *isPauseUserData;              // user data for check for pause call-back
  IsAbortedFunction               isAbortedFunction;             // check for aborted call-back
  void                            *isAbortedUserData;            // user data for check for aborted call-back
  LogHandle                       *logHandle;                    // log handle

  FragmentList                    fragmentList;                  // entry fragments

  Errors                          failError;                     // restore error
  StatusInfo                      statusInfo;                    // status info
  Semaphore                       statusInfoLock;                // status info lock
} RestoreInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : initRestoreInfo
* Purpose: initialize restore info
* Input  : restoreInfo                - restore info variable
*          storageSpecifier           - storage specifier structure
*          includeEntryList           - include entry list
*          excludePatternList         - exclude pattern list
*          compressExcludePatternList - exclude compression pattern list
*          deltaSourceList            - delta source list
*          jobOptions                 - job options
*          dryRun                     -
*          storageNameCustomText      - storage name custome text or NULL
*          updateStatusInfoFunction   - status info function call-back
*                                       (can be NULL)
*          updateStatusInfoUserData   - user data for status info
*                                       function
*          handleErrorFunction        - get password call-back
*          handleErrorUserData        - user data for get password
*          getNamePasswordFunction    - get name/password call-back
*          getNamePasswordUserData    - user data for get password
*          pauseRestoreFlag           - pause restore flag (can be
*                                       NULL)
*          requestedAbortFlag         - request abort flag (can be NULL)
*          logHandle                  - log handle (can be NULL)
* Output : restoreInfo - initialized restore info variable
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initRestoreInfo(RestoreInfo                     *restoreInfo,
                           StorageSpecifier                *storageSpecifier,
                           const EntryList                 *includeEntryList,
                           const PatternList               *excludePatternList,
                           DeltaSourceList                 *deltaSourceList,
                           JobOptions                      *jobOptions,
                           bool                            dryRun,
                           RestoreUpdateStatusInfoFunction updateStatusInfoFunction,
                           void                            *updateStatusInfoUserData,
                           RestoreHandleErrorFunction      handleErrorFunction,
                           void                            *handleErrorUserData,
                           GetNamePasswordFunction         getNamePasswordFunction,
                           void                            *getNamePasswordUserData,
                           IsPauseFunction                 isPauseFunction,
                           void                            *isPauseUserData,
                           IsAbortedFunction               isAbortedFunction,
                           void                            *isAbortedUserData,
                           LogHandle                       *logHandle
                          )
{
  assert(restoreInfo != NULL);

  // init variables
  restoreInfo->storageSpecifier               = storageSpecifier;
  restoreInfo->includeEntryList               = includeEntryList;
  restoreInfo->excludePatternList             = excludePatternList;
  restoreInfo->deltaSourceList                = deltaSourceList;
  restoreInfo->jobOptions                     = jobOptions;
  restoreInfo->dryRun                         = dryRun;
  restoreInfo->logHandle                      = logHandle;
  FragmentList_init(&restoreInfo->fragmentList);
  restoreInfo->failError                      = ERROR_NONE;
  restoreInfo->updateStatusInfoFunction       = updateStatusInfoFunction;
  restoreInfo->updateStatusInfoUserData       = updateStatusInfoUserData;
  restoreInfo->handleErrorFunction            = handleErrorFunction;
  restoreInfo->handleErrorUserData            = handleErrorUserData;
  restoreInfo->getNamePasswordFunction        = getNamePasswordFunction;
  restoreInfo->getNamePasswordUserData        = getNamePasswordUserData;
  restoreInfo->isPauseFunction                = isPauseFunction;
  restoreInfo->isPauseUserData                = isPauseUserData;
  restoreInfo->isAbortedFunction              = isAbortedFunction;
  restoreInfo->isAbortedUserData              = isAbortedUserData;
  initStatusInfo(&restoreInfo->statusInfo);

  // init locks
  if (!Semaphore_init(&restoreInfo->statusInfoLock,SEMAPHORE_TYPE_BINARY))
  {
    HALT_FATAL_ERROR("Cannot initialize status info semaphore!");
  }

  DEBUG_ADD_RESOURCE_TRACE(restoreInfo,sizeof(restoreInfo));
}

/***********************************************************************\
* Name   : doneRestoreInfo
* Purpose: deinitialize restore info
* Input  : restoreInfo - restore info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneRestoreInfo(RestoreInfo *restoreInfo)
{
  assert(restoreInfo != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(restoreInfo,sizeof(restoreInfo));

  doneStatusInfo(&restoreInfo->statusInfo);
  Semaphore_done(&restoreInfo->statusInfoLock);

  FragmentList_done(&restoreInfo->fragmentList);
}

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

LOCAL String getDestinationFileName(String      destinationFileName,
                                    String      fileName,
                                    ConstString destination,
                                    int         directoryStripCount
                                   )
{
  String          pathName,baseName;
  StringTokenizer fileNameTokenizer;
  ConstString     token;
  int             i;

  assert(destinationFileName != NULL);
  assert(fileName != NULL);

  // get destination base directory
  if (!String_isEmpty(destination))
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

LOCAL String getDestinationDeviceName(String      destinationDeviceName,
                                      String      imageName,
                                      ConstString destination
                                     )
{
  assert(destinationDeviceName != NULL);
  assert(imageName != NULL);

  if (!String_isEmpty(destination))
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
* Purpose: update restore status info
* Input  : restoreInfo - restore info
*          forceUpdate - true to force update
* Output : -
* Return : -
* Notes  : Update only every 500ms or if forced
\***********************************************************************/

LOCAL void updateStatusInfo(RestoreInfo *restoreInfo, bool forceUpdate)
{
  static uint64 lastTimestamp = 0LL;
  uint64        timestamp;

  assert(restoreInfo != NULL);

  if (restoreInfo->updateStatusInfoFunction != NULL)
  {
    timestamp = Misc_getTimestamp();
    if (forceUpdate || (timestamp > (lastTimestamp+500LL*US_PER_MS)))
    {
      restoreInfo->updateStatusInfoFunction(&restoreInfo->statusInfo,
                                            restoreInfo->updateStatusInfoUserData
                                           );
      lastTimestamp = timestamp;
    }
  }
}

/***********************************************************************\
* Name   : handleError
* Purpose: handle restore error
* Input  : restoreInfo - restore info
*          error       - error code
* Input  : -
* Output : -
* Return : new error code
* Notes  : -
\***********************************************************************/

LOCAL Errors handleError(RestoreInfo *restoreInfo, Errors error)
{
  assert(restoreInfo != NULL);

  if (restoreInfo->handleErrorFunction != NULL)
  {
    error = restoreInfo->handleErrorFunction(error,
                                             &restoreInfo->statusInfo,
                                             restoreInfo->handleErrorUserData
                                            );
  }

  return error;
}

/***********************************************************************\
* Name   : restoreFileEntry
* Purpose: restore file entry
* Input  : restoreInfo          - restore info
*          archiveHandle        - archive handle
*          printableStorageName - printable storage name
*          buffer               - buffer for temporary data
*          bufferSize           - size of data buffer
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors restoreFileEntry(RestoreInfo   *restoreInfo,
                              ArchiveHandle *archiveHandle,
                              ConstString   printableStorageName,
                              byte          *buffer,
                              uint          bufferSize
                             )
{
  AutoFreeList              autoFreeList;
  String                    fileName;
  FileExtendedAttributeList fileExtendedAttributeList;
  Errors                    error;
  ArchiveEntryInfo          archiveEntryInfo;
  FileInfo                  fileInfo;
  uint64                    fragmentOffset,fragmentSize;
  String                    destinationFileName;
  long                      index;
  String                    prefixFileName,postfixFileName;
  uint                      n;
  FragmentNode              *fragmentNode;
  String                    parentDirectoryName;
//            FileInfo                      localFileInfo;
  FileHandle                fileHandle;
  uint64                    length;
  ulong                     bufferLength;
  char                      s[256];

  assert(restoreInfo != NULL);
  assert(restoreInfo->jobOptions != NULL);
  assert(archiveHandle != NULL);

  // init variables
  AutoFree_init(&autoFreeList);
  fileName = String_new();
  File_initExtendedAttributes(&fileExtendedAttributeList);
  AUTOFREE_ADD(&autoFreeList,fileName,{ String_delete(fileName); });
  AUTOFREE_ADD(&autoFreeList,&fileExtendedAttributeList,{ File_doneExtendedAttributes(&fileExtendedAttributeList); });

  // read file entry
  error = Archive_readFileEntry(&archiveEntryInfo,
                                archiveHandle,
                                NULL,  // deltaCompressAlgorithm
                                NULL,  // byteCompressAlgorithm
                                NULL,  // cryptType
                                NULL,  // cryptAlgorithm
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
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo,{ Archive_closeEntry(&archiveEntryInfo); });

  if (   (List_isEmpty(restoreInfo->includeEntryList) || EntryList_match(restoreInfo->includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
      && ((restoreInfo->excludePatternList == NULL) || !PatternList_match(restoreInfo->excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
     )
  {
    // get destination filename
    destinationFileName = getDestinationFileName(String_new(),
                                                 fileName,
                                                 restoreInfo->jobOptions->destination,
                                                 restoreInfo->jobOptions->directoryStripCount
                                                );
    AUTOFREE_ADD(&autoFreeList,destinationFileName,{ String_delete(destinationFileName); });

    // update status info
    String_set(restoreInfo->statusInfo.entryName,destinationFileName);
    restoreInfo->statusInfo.entryDoneSize  = 0LL;
    restoreInfo->statusInfo.entryTotalSize = fragmentSize;
    updateStatusInfo(restoreInfo,TRUE);

    // check if file fragment already exists, file already exists
    if (!restoreInfo->jobOptions->noFragmentsCheckFlag)
    {
      // check if fragment already exist -> get/create file fragment node
      fragmentNode = FragmentList_find(&restoreInfo->fragmentList,destinationFileName);
      if (fragmentNode != NULL)
      {
        if (FragmentList_entryExists(fragmentNode,fragmentOffset,fragmentSize))
        {
          switch (restoreInfo->jobOptions->restoreEntryMode)
          {
            case RESTORE_ENTRY_MODE_STOP:
              // stop
              printInfo(1,
                        "  Restore file '%s'...skipped (file part %llu..%llu exists)\n",
                        String_cString(destinationFileName),
                        fragmentOffset,
                        (fragmentSize > 0LL) ? fragmentOffset+fragmentSize-1 : fragmentOffset
                       );
              AutoFree_cleanup(&autoFreeList);
              return !restoreInfo->jobOptions->noStopOnErrorFlag ? ERROR_FILE_EXISTS_ : ERROR_NONE;
              break;
            case RESTORE_ENTRY_MODE_RENAME:
              // rename new entry
              prefixFileName  = String_new();
              postfixFileName = String_new();
              index = String_findLastChar(destinationFileName,STRING_END,'.');
              if (index >= 0)
              {
                String_sub(prefixFileName,destinationFileName,STRING_BEGIN,index);
                String_sub(postfixFileName,destinationFileName,index,STRING_END);
              }
              else
              {
                String_set(prefixFileName,destinationFileName);
              }
              String_set(destinationFileName,prefixFileName);
              Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
              String_append(destinationFileName,postfixFileName);
              if (File_exists(destinationFileName))
              {
                n = 0;
                do
                {
                  String_set(destinationFileName,prefixFileName);
                  Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
                  String_appendFormat(destinationFileName,"-%04u",n);
                  String_append(destinationFileName,postfixFileName);
                  n++;
                }
                while (File_exists(destinationFileName));
              }
              String_delete(postfixFileName);
              String_delete(prefixFileName);
              break;
            case RESTORE_ENTRY_MODE_OVERWRITE:
              // nothing to do
              break;
          }
        }
      }
      else
      {
        // check if file already exists
        if (File_exists(destinationFileName))
        {
          switch (restoreInfo->jobOptions->restoreEntryMode)
          {
            case RESTORE_ENTRY_MODE_STOP:
              // stop
              printInfo(1,"  Restore file '%s'...skipped (file exists)\n",String_cString(destinationFileName));
              AutoFree_cleanup(&autoFreeList);
              return !restoreInfo->jobOptions->noStopOnErrorFlag ? ERROR_FILE_EXISTS_ : ERROR_NONE;
              break;
            case RESTORE_ENTRY_MODE_RENAME:
              // rename new entry
              prefixFileName  = String_new();
              postfixFileName = String_new();
              index = String_findLastChar(destinationFileName,STRING_END,'.');
              if (index >= 0)
              {
                String_sub(prefixFileName,destinationFileName,STRING_BEGIN,index);
                String_sub(postfixFileName,destinationFileName,index,STRING_END);
              }
              else
              {
                String_set(prefixFileName,destinationFileName);
              }
              String_set(destinationFileName,prefixFileName);
              Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
              String_append(destinationFileName,postfixFileName);
              if (File_exists(destinationFileName))
              {
                n = 0;
                do
                {
                  String_set(destinationFileName,prefixFileName);
                  Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
                  String_appendFormat(destinationFileName,"-%04u",n);
                  String_append(destinationFileName,postfixFileName);
                  n++;
                }
                while (File_exists(destinationFileName));
              }
              String_delete(postfixFileName);
              String_delete(prefixFileName);
              break;
            case RESTORE_ENTRY_MODE_OVERWRITE:
              // nothing to do
              break;
          }
        }
        fragmentNode = FragmentList_add(&restoreInfo->fragmentList,destinationFileName,fileInfo.size,&fileInfo,sizeof(FileInfo));
      }
      assert(fragmentNode != NULL);
    }
    else
    {
      fragmentNode = NULL;
    }

    printInfo(1,"  Restore file '%s'...",String_cString(destinationFileName));

    // create parent directories if not existing
    if (!restoreInfo->dryRun)
    {
      parentDirectoryName = File_getDirectoryName(String_new(),destinationFileName);
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
          AutoFree_cleanup(&autoFreeList);
          return error;
        }

        // set directory owner ship
        error = File_setOwner(parentDirectoryName,
                              (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : fileInfo.userId,
                              (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : fileInfo.groupId
                             );
        if (error != ERROR_NONE)
        {
          if (!restoreInfo->jobOptions->noStopOnErrorFlag)
          {
            printInfo(1,"FAIL!\n");
            printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                       String_cString(parentDirectoryName),
                       Error_getText(error)
                      );
            String_delete(parentDirectoryName);
            AutoFree_cleanup(&autoFreeList);
            return error;
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

    if (!restoreInfo->dryRun)
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
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      AUTOFREE_ADD(&autoFreeList,&fileHandle,{ (void)File_close(&fileHandle); });

      // seek to fragment position
      error = File_seek(&fileHandle,fragmentOffset);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("Cannot write content of file '%s' (error: %s)\n",
                   String_cString(destinationFileName),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }

    // write file data
    error  = ERROR_NONE;
    length = 0LL;
    while (   ((restoreInfo->isAbortedFunction == NULL) || !restoreInfo->isAbortedFunction(restoreInfo->isAbortedUserData))
           && (length < fragmentSize)
          )
    {
      // pause
      while ((restoreInfo->isPauseFunction != NULL) && restoreInfo->isPauseFunction(restoreInfo->isPauseUserData))
      {
        Misc_udelay(500L*US_PER_MS);
      }

      bufferLength = (ulong)MIN(fragmentSize-length,bufferSize);

      error = Archive_readData(&archiveEntryInfo,buffer,bufferLength);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("Cannot read content of file '%s' (error: %s)!\n",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
        break;
      }
      if (!restoreInfo->dryRun)
      {
        error = File_write(&fileHandle,buffer,bufferLength);
        if (error != ERROR_NONE)
        {
          printInfo(1,"FAIL!\n");
          printError("Cannot write content of file '%s' (error: %s)\n",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          break;
        }
      }

      // update status info
      restoreInfo->statusInfo.entryDoneSize += (uint64)bufferLength;
      updateStatusInfo(restoreInfo,FALSE);

      length += (uint64)bufferLength;

      printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
    }
    if      (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    else if ((restoreInfo->isAbortedFunction != NULL) && restoreInfo->isAbortedFunction(restoreInfo->isAbortedUserData))
    {
      printInfo(1,"ABORTED\n");
      AutoFree_cleanup(&autoFreeList);
      return ERROR_ABORTED;
    }
    printInfo(2,"    \b\b\b\b");

    // set file size
#ifndef WERROR
#warning required? wrong?
#endif
    if (!restoreInfo->dryRun)
    {
      if (File_getSize(&fileHandle) > fileInfo.size)
      {
        File_truncate(&fileHandle,fileInfo.size);
      }
    }

    // close file
    if (!restoreInfo->dryRun)
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
      if (!restoreInfo->dryRun)
      {
        if (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = restoreInfo->jobOptions->owner.userId;
        if (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = restoreInfo->jobOptions->owner.groupId;
        error = File_setInfo(&fileInfo,destinationFileName);
        if (error != ERROR_NONE)
        {
          if (   !restoreInfo->jobOptions->noStopOnErrorFlag
              && !File_isNetworkFileSystem(destinationFileName)
             )
          {
            printInfo(1,"FAIL!\n");
            printError("Cannot set file info of '%s' (error: %s)\n",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
            AutoFree_cleanup(&autoFreeList);
            return error;
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
        FragmentList_discard(&restoreInfo->fragmentList,fragmentNode);
      }
    }

    // get fragment info
    stringClear(s);
    if (fragmentSize < fileInfo.size)
    {
      stringFormat(s,sizeof(s),", fragment %15llu..%15llu",fragmentOffset,fragmentOffset+fragmentSize-1LL);
    }

    // output result
    if (!restoreInfo->dryRun)
    {
      printInfo(1,"OK (%llu bytes%s)\n",
                fragmentSize,
                s
               );
    }
    else
    {
      printInfo(1,"OK (%llu bytes%s, dry-run)\n",
                fragmentSize,
                s
               );
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
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : restoreImageEntry
* Purpose: restore image entry
* Input  : restoreInfo          - restore info
*          archiveHandle        - archive handle
*          storageSpecifier     - storage specifier
*          archiveName          - archive name
*          printableStorageName - printable storage name
*          buffer               - buffer for temporary data
*          bufferSize           - size of data buffer
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors restoreImageEntry(RestoreInfo   *restoreInfo,
                               ArchiveHandle *archiveHandle,
                               ConstString   printableStorageName,
                               byte          *buffer,
                               uint          bufferSize
                              )
{
  AutoFreeList     autoFreeList;
  String           deviceName;
  Errors           error;
  ArchiveEntryInfo archiveEntryInfo;
  DeviceInfo       deviceInfo;
  uint64           blockOffset,blockCount;
  String           destinationDeviceName;
  long             index;
  String           prefixFileName,postfixFileName;
  uint             n;
  FragmentNode     *fragmentNode;
  String           parentDirectoryName;
  enum
  {
    DEVICE,
    FILE,
    UNKNOWN
  }                type;
  DeviceHandle     deviceHandle;
  FileHandle       fileHandle;
  uint64           block;
  ulong            bufferBlockCount;
  char             s[256];

  // init variables
  AutoFree_init(&autoFreeList);
  deviceName = String_new();
  AUTOFREE_ADD(&autoFreeList,deviceName,{ String_delete(deviceName); });

  // read image entry
  error = Archive_readImageEntry(&archiveEntryInfo,
                                 archiveHandle,
                                 NULL,  // deltaCompressAlgorithm
                                 NULL,  // byteCompressAlgorithm
                                 NULL,  // cryptType
                                 NULL,  // cryptAlgorithm
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
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo,{ (void)Archive_closeEntry(&archiveEntryInfo); });
  if (deviceInfo.blockSize > bufferSize)
  {
    printError("Device block size %llu on '%s' is too big (max: %llu)\n",
               deviceInfo.blockSize,
               String_cString(deviceName),
               bufferSize
              );
    AutoFree_cleanup(&autoFreeList);
    return ERROR_INVALID_DEVICE_BLOCK_SIZE;
  }
  assert(deviceInfo.blockSize > 0);

  if (   (List_isEmpty(restoreInfo->includeEntryList) || EntryList_match(restoreInfo->includeEntryList,deviceName,PATTERN_MATCH_MODE_EXACT))
      && ((restoreInfo->excludePatternList == NULL) || !PatternList_match(restoreInfo->excludePatternList,deviceName,PATTERN_MATCH_MODE_EXACT))
     )
  {
    // get destination filename
    destinationDeviceName = getDestinationDeviceName(String_new(),
                                                     deviceName,
                                                     restoreInfo->jobOptions->destination
                                                    );
    AUTOFREE_ADD(&autoFreeList,destinationDeviceName,{ String_delete(destinationDeviceName); });

    // update status info
    String_set(restoreInfo->statusInfo.entryName,destinationDeviceName);
    restoreInfo->statusInfo.entryDoneSize  = 0LL;
    restoreInfo->statusInfo.entryTotalSize = blockCount;
    updateStatusInfo(restoreInfo,TRUE);

    if (!restoreInfo->jobOptions->noFragmentsCheckFlag)
    {
      // check if image fragment already exists

      // get/create image fragment node
      fragmentNode = FragmentList_find(&restoreInfo->fragmentList,deviceName);
      if (fragmentNode != NULL)
      {
        if (FragmentList_entryExists(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize))
        {
          switch (restoreInfo->jobOptions->restoreEntryMode)
          {
            case RESTORE_ENTRY_MODE_STOP:
              // stop
              printInfo(1,
                        "  Restore image '%s'...skipped (image part %llu..%llu exists)\n",
                        String_cString(destinationDeviceName),
                        blockOffset*(uint64)deviceInfo.blockSize,
                        ((blockCount > 0) ? blockOffset+blockCount-1:blockOffset)*(uint64)deviceInfo.blockSize
                       );
              AutoFree_cleanup(&autoFreeList);
              return !restoreInfo->jobOptions->noStopOnErrorFlag ? ERROR_FILE_EXISTS_ : ERROR_NONE;
              break;
            case RESTORE_ENTRY_MODE_RENAME:
              // rename new entry
              prefixFileName  = String_new();
              postfixFileName = String_new();
              index = String_findLastChar(destinationDeviceName,STRING_END,'.');
              if (index >= 0)
              {
                String_sub(prefixFileName,destinationDeviceName,STRING_BEGIN,index);
                String_sub(postfixFileName,destinationDeviceName,index,STRING_END);
              }
              else
              {
                String_set(prefixFileName,destinationDeviceName);
              }
              n = 0;
              do
              {
                String_set(destinationDeviceName,prefixFileName);
                String_appendFormat(destinationDeviceName,"-%04u",n);
                String_append(destinationDeviceName,postfixFileName);
                n++;
              }
              while (File_exists(destinationDeviceName));
              String_delete(postfixFileName);
              String_delete(prefixFileName);
              break;
            case RESTORE_ENTRY_MODE_OVERWRITE:
              // nothing to do
              break;
          }
        }
      }
      else
      {
        fragmentNode = FragmentList_add(&restoreInfo->fragmentList,deviceName,deviceInfo.size,NULL,0);
      }
      assert(fragmentNode != NULL);
    }
    else
    {
      fragmentNode = NULL;
    }

    printInfo(1,"  Restore image '%s'...",String_cString(destinationDeviceName));

    // create parent directories if not existing
    if (!restoreInfo->dryRun)
    {
      parentDirectoryName = File_getDirectoryName(String_new(),destinationDeviceName);
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
          AutoFree_cleanup(&autoFreeList);
          return error;
        }

        // set directory owner ship
        error = File_setOwner(parentDirectoryName,
                              (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : deviceInfo.userId,
                              (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : deviceInfo.groupId
                             );
        if (error != ERROR_NONE)
        {
          if (!restoreInfo->jobOptions->noStopOnErrorFlag)
          {
            printInfo(1,"FAIL!\n");
            printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                       String_cString(parentDirectoryName),
                       Error_getText(error)
                      );
            String_delete(parentDirectoryName);
            AutoFree_cleanup(&autoFreeList);
            return error;
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
    if (!restoreInfo->dryRun)
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
        AutoFree_cleanup(&autoFreeList);
        return error;
      }

      // seek to fragment position
      switch (type)
      {
        case DEVICE:
          error = Device_seek(&deviceHandle,blockOffset*(uint64)deviceInfo.blockSize);
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL!\n");
            printError("Cannot write content to device '%s' (error: %s)\n",
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
            printError("Cannot write content of file '%s' (error: %s)\n",
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
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }

    // write image data
    error = ERROR_NONE;
    block = 0LL;
    while (   ((restoreInfo->isAbortedFunction == NULL) || !restoreInfo->isAbortedFunction(restoreInfo->isAbortedUserData))
           && (block < blockCount)
          )
    {
      // pause
      while ((restoreInfo->isPauseFunction != NULL) && restoreInfo->isPauseFunction(restoreInfo->isPauseUserData))
      {
        Misc_udelay(500L*1000L);
      }

      bufferBlockCount = MIN(blockCount-block,bufferSize/deviceInfo.blockSize);

      // read data from archive
      error = Archive_readData(&archiveEntryInfo,buffer,bufferBlockCount*deviceInfo.blockSize);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("Cannot read content of image '%s' (error: %s)!\n",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
        break;
      }

      if (!restoreInfo->dryRun)
      {
        // write data to device
        switch (type)
        {
          case DEVICE:
            error = Device_write(&deviceHandle,buffer,bufferBlockCount*deviceInfo.blockSize);
            if (error != ERROR_NONE)
            {
              printInfo(1,"FAIL!\n");
              printError("Cannot write content to device '%s' (error: %s)\n",
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
              printError("Cannot write content of file '%s' (error: %s)\n",
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
          break;
        }
      }

      // update status info
      restoreInfo->statusInfo.entryDoneSize += bufferBlockCount*deviceInfo.blockSize;
      updateStatusInfo(restoreInfo,FALSE);

      block += (uint64)bufferBlockCount;

      printInfo(2,"%3d%%\b\b\b\b",(uint)((block*100LL)/blockCount));
    }
    if      (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    else if ((restoreInfo->isAbortedFunction != NULL) && restoreInfo->isAbortedFunction(restoreInfo->isAbortedUserData))
    {
      printInfo(1,"ABORTED\n");
      AutoFree_cleanup(&autoFreeList);
      return ERROR_ABORTED;
    }
    printInfo(2,"    \b\b\b\b");

    // close device/file
    if (!restoreInfo->dryRun)
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
        FragmentList_discard(&restoreInfo->fragmentList,fragmentNode);
      }
    }

    // get fragment info
    stringClear(s);
    if ((blockCount*deviceInfo.blockSize) < deviceInfo.size)
    {
      stringFormat(s,sizeof(s),", fragment %15llu..%15llu",blockOffset*deviceInfo.blockSize,blockOffset*deviceInfo.blockSize+(blockCount*deviceInfo.blockSize)-1LL);
    }

    // output result
    if (!restoreInfo->dryRun)
    {
      printInfo(1,"OK (%llu bytes%s)\n",
                blockCount*deviceInfo.blockSize,
                s
               );
    }
    else
    {
      printInfo(1,"OK (%llu bytes%s, dry-run)\n",
                blockCount*deviceInfo.blockSize,
                s
               );
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
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : restoreDirectoryEntry
* Purpose: restore directory entry
* Input  : restoreInfo          - restore info
*          archiveHandle        - archive handle
*          printableStorageName - printable storage name
*          buffer               - buffer for temporary data
*          bufferSize           - size of data buffer
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors restoreDirectoryEntry(RestoreInfo   *restoreInfo,
                                   ArchiveHandle *archiveHandle,
                                   ConstString   printableStorageName,
                                   byte          *buffer,
                                   uint          bufferSize
                                  )
{
  AutoFreeList              autoFreeList;
  String                    directoryName;
  FileExtendedAttributeList fileExtendedAttributeList;
  Errors                    error;
  ArchiveEntryInfo          archiveEntryInfo;
  FileInfo                  fileInfo;
  String                    destinationFileName;
  long                      index;
  String                    prefixFileName,postfixFileName;
  uint                      n;
//            FileInfo localFileInfo;

  UNUSED_VARIABLE(buffer);
  UNUSED_VARIABLE(bufferSize);

  // init variables
  AutoFree_init(&autoFreeList);
  directoryName = String_new();
  File_initExtendedAttributes(&fileExtendedAttributeList);
  AUTOFREE_ADD(&autoFreeList,directoryName,{ String_delete(directoryName); });
  AUTOFREE_ADD(&autoFreeList,&fileExtendedAttributeList,{ File_doneExtendedAttributes(&fileExtendedAttributeList); });

  // read directory entry
  error = Archive_readDirectoryEntry(&archiveEntryInfo,
                                     archiveHandle,
                                     NULL,  // cryptType
                                     NULL,  // cryptAlgorithm
                                     directoryName,
                                     &fileInfo,
                                     &fileExtendedAttributeList
                                    );
  if (error != ERROR_NONE)
  {
    printError("Cannot read 'directory' content of archive '%s' (error: %s)!\n",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo,{ (void)Archive_closeEntry(&archiveEntryInfo); });

  if (   (List_isEmpty(restoreInfo->includeEntryList) || EntryList_match(restoreInfo->includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
      && ((restoreInfo->excludePatternList == NULL) || !PatternList_match(restoreInfo->excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT))
     )
  {
    // get destination filename
    destinationFileName = getDestinationFileName(String_new(),
                                                 directoryName,
                                                 restoreInfo->jobOptions->destination,
                                                 restoreInfo->jobOptions->directoryStripCount
                                                );
    AUTOFREE_ADD(&autoFreeList,destinationFileName,{ String_delete(destinationFileName); });

    // update status info
    String_set(restoreInfo->statusInfo.entryName,destinationFileName);
    restoreInfo->statusInfo.entryDoneSize  = 0LL;
    restoreInfo->statusInfo.entryTotalSize = 0LL;
    updateStatusInfo(restoreInfo,TRUE);

    // check if directory already exists
    if (File_exists(destinationFileName))
    {
      switch (restoreInfo->jobOptions->restoreEntryMode)
      {
        case RESTORE_ENTRY_MODE_STOP:
          // stop
          printInfo(1,
                    "  Restore directory '%s'...skipped (file exists)\n",
                    String_cString(destinationFileName)
                   );
          AutoFree_cleanup(&autoFreeList);
          return error;
          break;
        case RESTORE_ENTRY_MODE_RENAME:
          // rename new entry
          prefixFileName  = String_new();
          postfixFileName = String_new();
          index = String_findLastChar(destinationFileName,STRING_END,'.');
          if (index >= 0)
          {
            String_sub(prefixFileName,destinationFileName,STRING_BEGIN,index);
            String_sub(postfixFileName,destinationFileName,index,STRING_END);
          }
          else
          {
            String_set(prefixFileName,destinationFileName);
          }
          String_set(destinationFileName,prefixFileName);
          Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
          String_append(destinationFileName,postfixFileName);
          if (File_exists(destinationFileName))
          {
            n = 0;
            do
            {
              String_set(destinationFileName,prefixFileName);
              Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
              String_appendFormat(destinationFileName,"-%04u",n);
              String_append(destinationFileName,postfixFileName);
              n++;
            }
            while (File_exists(destinationFileName));
          }
          String_delete(postfixFileName);
          String_delete(prefixFileName);
          break;
        case RESTORE_ENTRY_MODE_OVERWRITE:
          // nothing to do
          break;
      }
    }

    printInfo(1,"  Restore directory '%s'...",String_cString(destinationFileName));

    // create directory
    if (!restoreInfo->dryRun)
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
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }

    // set file time, file owner/group
    if (!restoreInfo->dryRun)
    {
      if (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = restoreInfo->jobOptions->owner.userId;
      if (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = restoreInfo->jobOptions->owner.groupId;
      error = File_setInfo(&fileInfo,destinationFileName);
      if (error != ERROR_NONE)
      {
        if (   !restoreInfo->jobOptions->noStopOnErrorFlag
            && !File_isNetworkFileSystem(destinationFileName)
           )
        {
          printInfo(1,"FAIL!\n");
          printError("Cannot set directory info of '%s' (error: %s)\n",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
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

    // output result
    if (!restoreInfo->dryRun)
    {
      printInfo(1,"OK\n");
    }
    else
    {
      printInfo(1,"OK (dry-run)\n");
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
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : restoreLinkEntry
* Purpose: restore link entry
* Input  : restoreInfo          - restore info
*          archiveHandle        - archive handle
*          printableStorageName - printable storage name
*          buffer               - buffer for temporary data
*          bufferSize           - size of data buffer
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors restoreLinkEntry(RestoreInfo   *restoreInfo,
                              ArchiveHandle *archiveHandle,
                              ConstString   printableStorageName,
                              byte          *buffer,
                              uint          bufferSize
                             )
{
  AutoFreeList              autoFreeList;
  String                    linkName;
  String                    fileName;
  FileExtendedAttributeList fileExtendedAttributeList;
  Errors                    error;
  ArchiveEntryInfo          archiveEntryInfo;
  FileInfo                  fileInfo;
  String                    destinationFileName;
  long                      index;
  String                    prefixFileName,postfixFileName;
  uint                      n;
  String                    parentDirectoryName;
//            FileInfo localFileInfo;

  UNUSED_VARIABLE(buffer);
  UNUSED_VARIABLE(bufferSize);

  // init variables
  AutoFree_init(&autoFreeList);
  linkName = String_new();
  fileName = String_new();
  File_initExtendedAttributes(&fileExtendedAttributeList);
  AUTOFREE_ADD(&autoFreeList,linkName,{ String_delete(linkName); });
  AUTOFREE_ADD(&autoFreeList,fileName,{ String_delete(fileName); });
  AUTOFREE_ADD(&autoFreeList,&fileExtendedAttributeList,{ File_doneExtendedAttributes(&fileExtendedAttributeList); });

  // read link entry
  error = Archive_readLinkEntry(&archiveEntryInfo,
                                archiveHandle,
                                NULL,  // cryptType
                                NULL,  // cryptAlgorithm
                                linkName,
                                fileName,
                                &fileInfo,
                                &fileExtendedAttributeList
                               );
  if (error != ERROR_NONE)
  {
    printError("Cannot read 'link' content of archive '%s' (error: %s)!\n",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo,{ Archive_closeEntry(&archiveEntryInfo); });

  if (   (List_isEmpty(restoreInfo->includeEntryList) || EntryList_match(restoreInfo->includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
      && ((restoreInfo->excludePatternList == NULL) || !PatternList_match(restoreInfo->excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT))
     )
  {
    // get destination filename
    destinationFileName = getDestinationFileName(String_new(),
                                                 linkName,
                                                 restoreInfo->jobOptions->destination,
                                                 restoreInfo->jobOptions->directoryStripCount
                                                );
    AUTOFREE_ADD(&autoFreeList,destinationFileName,{ String_delete(destinationFileName); });

    // update status info
    String_set(restoreInfo->statusInfo.entryName,destinationFileName);
    restoreInfo->statusInfo.entryDoneSize  = 0LL;
    restoreInfo->statusInfo.entryTotalSize = 0LL;
    updateStatusInfo(restoreInfo,TRUE);

    // create parent directories if not existing
    if (!restoreInfo->dryRun)
    {
      parentDirectoryName = File_getDirectoryName(String_new(),destinationFileName);
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
          AutoFree_cleanup(&autoFreeList);
          return error;
        }

        // set directory owner ship
        error = File_setOwner(parentDirectoryName,
                              (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : fileInfo.userId,
                              (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : fileInfo.groupId
                             );
        if (error != ERROR_NONE)
        {
          if (!restoreInfo->jobOptions->noStopOnErrorFlag)
          {
            printInfo(1,"FAIL!\n");
            printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                       String_cString(parentDirectoryName),
                       Error_getText(error)
                      );
            String_delete(parentDirectoryName);
            AutoFree_cleanup(&autoFreeList);
            return error;
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
    if (File_exists(destinationFileName))
    {
      switch (restoreInfo->jobOptions->restoreEntryMode)
      {
        case RESTORE_ENTRY_MODE_STOP:
          // stop
          printInfo(1,
                    "  Restore link '%s'...skipped (file exists)\n",
                    String_cString(destinationFileName)
                   );
          if (!restoreInfo->jobOptions->noStopOnErrorFlag)
          {
            error = ERRORX_(FILE_EXISTS_,0,"%s",String_cString(destinationFileName));
          }
          AutoFree_cleanup(&autoFreeList);
          return error;
        case RESTORE_ENTRY_MODE_RENAME:
          // rename new entry
          prefixFileName  = String_new();
          postfixFileName = String_new();
          index = String_findLastChar(destinationFileName,STRING_END,'.');
          if (index >= 0)
          {
            String_sub(prefixFileName,destinationFileName,STRING_BEGIN,index);
            String_sub(postfixFileName,destinationFileName,index,STRING_END);
          }
          else
          {
            String_set(prefixFileName,destinationFileName);
          }
          String_set(destinationFileName,prefixFileName);
          Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
          String_append(destinationFileName,postfixFileName);
          if (File_exists(destinationFileName))
          {
            n = 0;
            do
            {
              String_set(destinationFileName,prefixFileName);
              Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
              String_appendFormat(destinationFileName,"-%04u",n);
              String_append(destinationFileName,postfixFileName);
              n++;
            }
            while (File_exists(destinationFileName));
          }
          String_delete(postfixFileName);
          String_delete(prefixFileName);
          break;
        case RESTORE_ENTRY_MODE_OVERWRITE:
          // nothing to do
          break;
      }
    }

    printInfo(1,"  Restore link '%s'...",String_cString(destinationFileName));

    // create link
    if (!restoreInfo->dryRun)
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
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }

    // set file time, file owner/group
    if (!restoreInfo->dryRun)
    {
      if (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = restoreInfo->jobOptions->owner.userId;
      if (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = restoreInfo->jobOptions->owner.groupId;
      error = File_setInfo(&fileInfo,destinationFileName);
      if (error != ERROR_NONE)
      {
        if (   !restoreInfo->jobOptions->noStopOnErrorFlag
            && !File_isNetworkFileSystem(destinationFileName)
           )
        {
          printInfo(1,"FAIL!\n");
          printError("Cannot set file info of '%s' (error: %s)\n",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
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

    // output result
    if (!restoreInfo->dryRun)
    {
      printInfo(1,"OK\n");
    }
    else
    {
      printInfo(1,"OK (dry-run)\n");
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
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : restoreHardLinkEntry
* Purpose: restore hardlink entry
* Input  : restoreInfo          - restore info
*          archiveHandle        - archive handle
*          printableStorageName - printable storage name
*          buffer               - buffer for temporary data
*          bufferSize           - size of data buffer
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors restoreHardLinkEntry(RestoreInfo   *restoreInfo,
                                  ArchiveHandle *archiveHandle,
                                  ConstString   printableStorageName,
                                  byte          *buffer,
                                  uint          bufferSize
                                 )
{
  AutoFreeList              autoFreeList;
  StringList                fileNameList;
  FileExtendedAttributeList fileExtendedAttributeList;
  Errors                    error;
  ArchiveEntryInfo          archiveEntryInfo;
  FileInfo                  fileInfo;
  uint64                    fragmentOffset,fragmentSize;
  String                    hardLinkFileName;
  String                    destinationFileName;
  bool                      restoredDataFlag;
  void                      *autoFreeSavePoint;
  long                      index;
  String                    prefixFileName,postfixFileName;
  uint                      n;
  FragmentNode              *fragmentNode;
  const StringNode          *stringNode;
  String                    fileName;
  String                    parentDirectoryName;
//            FileInfo                  localFileInfo;
  FileHandle                fileHandle;
  uint64                    length;
  ulong                     bufferLength;
  char                      s[256];

  // init variables
  AutoFree_init(&autoFreeList);
  StringList_init(&fileNameList);
  File_initExtendedAttributes(&fileExtendedAttributeList);
  AUTOFREE_ADD(&autoFreeList,&fileNameList,{ StringList_done(&fileNameList); });
  AUTOFREE_ADD(&autoFreeList,&fileExtendedAttributeList,{ File_doneExtendedAttributes(&fileExtendedAttributeList); });

  // read hard link entry
  error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                    archiveHandle,
                                    NULL,  // deltaCompressAlgorithm
                                    NULL,  // byteCompressAlgorithm
                                    NULL,  // cryptType
                                    NULL,  // cryptAlgorithm
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
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo,{ Archive_closeEntry(&archiveEntryInfo); });

  hardLinkFileName    = String_new();
  destinationFileName = String_new();
  AUTOFREE_ADD(&autoFreeList,hardLinkFileName,{ String_delete(hardLinkFileName); });
  AUTOFREE_ADD(&autoFreeList,destinationFileName,{ String_delete(destinationFileName); });
  restoredDataFlag    = FALSE;
  autoFreeSavePoint   = AutoFree_save(&autoFreeList);
  STRINGLIST_ITERATEX(&fileNameList,stringNode,fileName,error == ERROR_NONE)
  {
    if (   (List_isEmpty(restoreInfo->includeEntryList) || EntryList_match(restoreInfo->includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
        && ((restoreInfo->excludePatternList == NULL) || !PatternList_match(restoreInfo->excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
       )
    {
      // get destination filename
      getDestinationFileName(destinationFileName,
                             fileName,
                             restoreInfo->jobOptions->destination,
                             restoreInfo->jobOptions->directoryStripCount
                            );

      // update status info
      String_set(restoreInfo->statusInfo.entryName,destinationFileName);
      restoreInfo->statusInfo.entryDoneSize  = 0LL;
      restoreInfo->statusInfo.entryTotalSize = fragmentSize;
      updateStatusInfo(restoreInfo,TRUE);

      printInfo(1,"  Restore hard link '%s'...",String_cString(destinationFileName));

      // create parent directories if not existing
      if (!restoreInfo->dryRun)
      {
        parentDirectoryName = File_getDirectoryName(String_new(),destinationFileName);
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
            AutoFree_cleanup(&autoFreeList);
            return error;
          }

          // set directory owner ship
          error = File_setOwner(parentDirectoryName,
                                (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : fileInfo.userId,
                                (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : fileInfo.groupId
                               );
          if (error != ERROR_NONE)
          {
            if (!restoreInfo->jobOptions->noStopOnErrorFlag)
            {
              printInfo(1,"FAIL!\n");
              printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                         String_cString(parentDirectoryName),
                         Error_getText(error)
                        );
              String_delete(parentDirectoryName);
              AutoFree_cleanup(&autoFreeList);
              return error;
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
        // check if file fragment already eixsts, file already exists
        if (!restoreInfo->jobOptions->noFragmentsCheckFlag)
        {
          // check if fragment already exist -> get/create file fragment node
          fragmentNode = FragmentList_find(&restoreInfo->fragmentList,fileName);
          if (fragmentNode != NULL)
          {
            if (FragmentList_entryExists(fragmentNode,fragmentOffset,fragmentSize))
            {
              switch (restoreInfo->jobOptions->restoreEntryMode)
              {
                case RESTORE_ENTRY_MODE_STOP:
                  // stop
                  printInfo(1,"skipped (file part %llu..%llu exists)\n",
                            String_cString(destinationFileName),
                            fragmentOffset,
                            (fragmentSize > 0LL) ? fragmentOffset+fragmentSize-1:fragmentOffset
                           );
                  AutoFree_cleanup(&autoFreeList);
                  return !restoreInfo->jobOptions->noStopOnErrorFlag ? ERROR_FILE_EXISTS_ : ERROR_NONE;
                  break;
                case RESTORE_ENTRY_MODE_RENAME:
                  // rename new entry
                  prefixFileName  = String_new();
                  postfixFileName = String_new();
                  index = String_findLastChar(destinationFileName,STRING_END,'.');
                  if (index >= 0)
                  {
                    String_sub(prefixFileName,destinationFileName,STRING_BEGIN,index);
                    String_sub(postfixFileName,destinationFileName,index,STRING_END);
                  }
                  else
                  {
                    String_set(prefixFileName,destinationFileName);
                  }
                  String_set(destinationFileName,prefixFileName);
                  Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
                  String_append(destinationFileName,postfixFileName);
                  if (File_exists(destinationFileName))
                  {
                    n = 0;
                    do
                    {
                      String_set(destinationFileName,prefixFileName);
                      Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
                      String_appendFormat(destinationFileName,"-%04u",n);
                      String_append(destinationFileName,postfixFileName);
                      n++;
                    }
                    while (File_exists(destinationFileName));
                  }
                  String_delete(postfixFileName);
                  String_delete(prefixFileName);
                  break;
                case RESTORE_ENTRY_MODE_OVERWRITE:
                  // nothing to do
                  break;
              }
            }
          }
          else
          {
            // check if file already exists
            if (File_exists(destinationFileName))
            {
              switch (restoreInfo->jobOptions->restoreEntryMode)
              {
                case RESTORE_ENTRY_MODE_STOP:
                  // stop
                  printInfo(1,"skipped (file exists)\n",String_cString(destinationFileName));
                  AutoFree_cleanup(&autoFreeList);
                  return !restoreInfo->jobOptions->noStopOnErrorFlag ? ERROR_FILE_EXISTS_ : ERROR_NONE;
                  break;
                case RESTORE_ENTRY_MODE_RENAME:
                  // rename new entry
                  prefixFileName  = String_new();
                  postfixFileName = String_new();
                  index = String_findLastChar(destinationFileName,STRING_END,'.');
                  if (index >= 0)
                  {
                    String_sub(prefixFileName,destinationFileName,STRING_BEGIN,index);
                    String_sub(postfixFileName,destinationFileName,index,STRING_END);
                  }
                  else
                  {
                    String_set(prefixFileName,destinationFileName);
                  }
                  String_set(destinationFileName,prefixFileName);
                  Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
                  String_append(destinationFileName,postfixFileName);
                  if (File_exists(destinationFileName))
                  {
                    n = 0;
                    do
                    {
                      String_set(destinationFileName,prefixFileName);
                      Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
                      String_appendFormat(destinationFileName,"-%04u",n);
                      String_append(destinationFileName,postfixFileName);
                      n++;
                    }
                    while (File_exists(destinationFileName));
                  }
                  String_delete(postfixFileName);
                  String_delete(prefixFileName);
                  break;
                case RESTORE_ENTRY_MODE_OVERWRITE:
                  // nothing to do
                  break;
              }
            }
            fragmentNode = FragmentList_add(&restoreInfo->fragmentList,fileName,fileInfo.size,&fileInfo,sizeof(FileInfo));
          }
          assert(fragmentNode != NULL);
        }
        else
        {
          fragmentNode = NULL;
        }

        if (!restoreInfo->dryRun)
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
            AutoFree_cleanup(&autoFreeList);
            return error;
          }
          AUTOFREE_ADD(&autoFreeList,&fileHandle,{ (void)File_close(&fileHandle); });

          // seek to fragment position
          error = File_seek(&fileHandle,fragmentOffset);
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL!\n");
            printError("Cannot write content of hard link '%s' (error: %s)\n",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
            File_close(&fileHandle);
            AutoFree_cleanup(&autoFreeList);
            return error;
          }
          String_set(hardLinkFileName,destinationFileName);
        }

        // write file data
        error  = ERROR_NONE;
        length = 0LL;
        while (   ((restoreInfo->isAbortedFunction == NULL) || !restoreInfo->isAbortedFunction(restoreInfo->isAbortedUserData))
               && (length < fragmentSize)
              )
        {
          // pause
          while ((restoreInfo->isPauseFunction != NULL) && restoreInfo->isPauseFunction(restoreInfo->isPauseUserData))
          {
            Misc_udelay(500L*US_PER_MS);
          }

          bufferLength = (ulong)MIN(fragmentSize-length,bufferSize);

          error = Archive_readData(&archiveEntryInfo,buffer,bufferLength);
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL!\n");
            printError("Cannot read content of hard link '%s' (error: %s)!\n",
                       String_cString(printableStorageName),
                       Error_getText(error)
                      );
            break;
          }
          if (!restoreInfo->dryRun)
          {
            error = File_write(&fileHandle,buffer,bufferLength);
            if (error != ERROR_NONE)
            {
              printInfo(1,"FAIL!\n");
              printError("Cannot write content of hard link '%s' (error: %s)\n",
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
              break;
            }
          }

          // update status info
          restoreInfo->statusInfo.entryDoneSize += (uint64)bufferLength;
          updateStatusInfo(restoreInfo,FALSE);

          length += (uint64)bufferLength;

          printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
        }
        if      (error != ERROR_NONE)
        {
          if (!restoreInfo->dryRun)
          {
            (void)File_close(&fileHandle);
          }
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        else if ((restoreInfo->isAbortedFunction != NULL) && restoreInfo->isAbortedFunction(restoreInfo->isAbortedUserData))
        {
          printInfo(1,"ABORTED\n");
          if (!restoreInfo->dryRun)
          {
            (void)File_close(&fileHandle);
          }
          AutoFree_cleanup(&autoFreeList);
          return ERROR_ABORTED;
        }
        printInfo(2,"    \b\b\b\b");

        // set file size
#ifndef WERROR
#warning required? wrong?
#endif
        if (!restoreInfo->dryRun)
        {
          if (File_getSize(&fileHandle) > fileInfo.size)
          {
            File_truncate(&fileHandle,fileInfo.size);
          }
        }

        // close file
        if (!restoreInfo->dryRun)
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
          if (!restoreInfo->dryRun)
          {
            if (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = restoreInfo->jobOptions->owner.userId;
            if (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = restoreInfo->jobOptions->owner.groupId;
            error = File_setInfo(&fileInfo,destinationFileName);
            if (error != ERROR_NONE)
            {
              if (   !restoreInfo->jobOptions->noStopOnErrorFlag
                  && !File_isNetworkFileSystem(destinationFileName)
                 )
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot set file info of '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Error_getText(error)
                          );
                AutoFree_cleanup(&autoFreeList);
                return error;
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
            FragmentList_discard(&restoreInfo->fragmentList,fragmentNode);
          }
        }

        // get fragment info
        stringClear(s);
        if (fragmentSize < fileInfo.size)
        {
          stringFormat(s,sizeof(s),", fragment %15llu..%15llu",fragmentOffset,fragmentOffset+fragmentSize-1LL);
        }

        // output result
        if (!restoreInfo->dryRun)
        {
          printInfo(1,"OK (%llu bytes%s)\n",
                    fragmentSize,
                    s
                   );
        }
        else
        {
          printInfo(1,"OK (%llu bytes%s, dry-run)\n",
                    fragmentSize,
                    s
                   );
        }

        restoredDataFlag = TRUE;
      }
      else
      {
        // check file if exists
        if (File_exists(destinationFileName))
        {
          switch (restoreInfo->jobOptions->restoreEntryMode)
          {
            case RESTORE_ENTRY_MODE_STOP:
              // stop
              printInfo(1,"skipped (file exists)\n",String_cString(destinationFileName));
              AutoFree_restore(&autoFreeList,autoFreeSavePoint,TRUE);
              break;
              break;
            case RESTORE_ENTRY_MODE_RENAME:
              // rename new entry
              prefixFileName  = String_new();
              postfixFileName = String_new();
              index = String_findLastChar(destinationFileName,STRING_END,'.');
              if (index >= 0)
              {
                String_sub(prefixFileName,destinationFileName,STRING_BEGIN,index);
                String_sub(postfixFileName,destinationFileName,index,STRING_END);
              }
              else
              {
                String_set(prefixFileName,destinationFileName);
              }
              String_set(destinationFileName,prefixFileName);
              Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
              String_append(destinationFileName,postfixFileName);
              if (File_exists(destinationFileName))
              {
                n = 0;
                do
                {
                  String_set(destinationFileName,prefixFileName);
                  Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
                  String_appendFormat(destinationFileName,"-%04u",n);
                  String_append(destinationFileName,postfixFileName);
                  n++;
                }
                while (File_exists(destinationFileName));
              }
              String_delete(postfixFileName);
              String_delete(prefixFileName);
              break;
            case RESTORE_ENTRY_MODE_OVERWRITE:
              break;
          }
        }

        // create hard link
        if (!restoreInfo->dryRun)
        {
          error = File_makeHardLink(destinationFileName,hardLinkFileName);
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL!\n");
            printError("Cannot create/write to file '%s' (error: %s)\n",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
            AutoFree_cleanup(&autoFreeList);
            return error;
          }
        }

        // output result
        if (!restoreInfo->dryRun)
        {
          printInfo(1,"OK\n");
        }
        else
        {
          printInfo(1,"OK (dry-run)\n");
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
  AutoFree_restore(&autoFreeList,autoFreeSavePoint,FALSE);
  String_delete(destinationFileName);
  String_delete(hardLinkFileName);
  AUTOFREE_REMOVE(&autoFreeList,destinationFileName);
  AUTOFREE_REMOVE(&autoFreeList,hardLinkFileName);
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
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
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : restoreSpecialEntry
* Purpose: restore special entry
* Input  : restoreInfo          - restore info
*          archiveHandle        - archive handle
*          printableStorageName - printable storage name
*          buffer               - buffer for temporary data
*          bufferSize           - size of data buffer
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors restoreSpecialEntry(RestoreInfo   *restoreInfo,
                                 ArchiveHandle *archiveHandle,
                                 ConstString   printableStorageName,
                                 byte          *buffer,
                                 uint          bufferSize
                                )
{
  AutoFreeList              autoFreeList;
  String                    fileName;
  FileExtendedAttributeList fileExtendedAttributeList;
  Errors                    error;
  ArchiveEntryInfo          archiveEntryInfo;
  FileInfo                  fileInfo;
  String                    destinationFileName;
  long                      index;
  String                    prefixFileName,postfixFileName;
  uint                      n;
  String                    parentDirectoryName;
//            FileInfo localFileInfo;

  UNUSED_VARIABLE(buffer);
  UNUSED_VARIABLE(bufferSize);

  // init variables
  AutoFree_init(&autoFreeList);
  fileName = String_new();
  File_initExtendedAttributes(&fileExtendedAttributeList);
  AUTOFREE_ADD(&autoFreeList,fileName,{ String_delete(fileName); });
  AUTOFREE_ADD(&autoFreeList,&fileExtendedAttributeList,{ File_doneExtendedAttributes(&fileExtendedAttributeList); });

  // read special device entry
  error = Archive_readSpecialEntry(&archiveEntryInfo,
                                   archiveHandle,
                                   NULL,  // cryptType
                                   NULL,  // cryptAlgorithm
                                   fileName,
                                   &fileInfo,
                                   &fileExtendedAttributeList
                                  );
  if (error != ERROR_NONE)
  {
    printError("Cannot read 'special' content of archive '%s' (error: %s)!\n",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo,{ Archive_closeEntry(&archiveEntryInfo); });

  if (   (List_isEmpty(restoreInfo->includeEntryList) || EntryList_match(restoreInfo->includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
      && ((restoreInfo->excludePatternList == NULL) || !PatternList_match(restoreInfo->excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
     )
  {
    // get destination filename
    destinationFileName = getDestinationFileName(String_new(),
                                                 fileName,
                                                 restoreInfo->jobOptions->destination,
                                                 restoreInfo->jobOptions->directoryStripCount
                                                );
    AUTOFREE_ADD(&autoFreeList,destinationFileName,{ String_delete(destinationFileName); });

    // update status info
    String_set(restoreInfo->statusInfo.entryName,destinationFileName);
    restoreInfo->statusInfo.entryDoneSize  = 0LL;
    restoreInfo->statusInfo.entryTotalSize = 0LL;
    updateStatusInfo(restoreInfo,TRUE);

    // create parent directories if not existing
    if (!restoreInfo->dryRun)
    {
      parentDirectoryName = File_getDirectoryName(String_new(),destinationFileName);
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
          AutoFree_cleanup(&autoFreeList);
          return error;
        }

        // set directory owner ship
        error = File_setOwner(parentDirectoryName,
                              (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : fileInfo.userId,
                              (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : fileInfo.groupId
                             );
        if (error != ERROR_NONE)
        {
          if (!restoreInfo->jobOptions->noStopOnErrorFlag)
          {
            printInfo(1,"FAIL!\n");
            printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                       String_cString(parentDirectoryName),
                       Error_getText(error)
                      );
            String_delete(parentDirectoryName);
            AutoFree_cleanup(&autoFreeList);
            return error;
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
    if (File_exists(destinationFileName))
    {
      switch (restoreInfo->jobOptions->restoreEntryMode)
      {
        case RESTORE_ENTRY_MODE_STOP:
          // stop
          printInfo(1,
                    "  Restore special device '%s'...skipped (file exists)\n",
                    String_cString(destinationFileName)
                   );
          if (!restoreInfo->jobOptions->noStopOnErrorFlag)
          {
            error = ERRORX_(FILE_EXISTS_,0,"%s",String_cString(destinationFileName));
          }
          AutoFree_cleanup(&autoFreeList);
          return error;
          break;
        case RESTORE_ENTRY_MODE_RENAME:
          // rename new entry
          prefixFileName  = String_new();
          postfixFileName = String_new();
          index = String_findLastChar(destinationFileName,STRING_END,'.');
          if (index >= 0)
          {
            String_sub(prefixFileName,destinationFileName,STRING_BEGIN,index);
            String_sub(postfixFileName,destinationFileName,index,STRING_END);
          }
          else
          {
            String_set(prefixFileName,destinationFileName);
          }
          String_set(destinationFileName,prefixFileName);
          Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
          String_append(destinationFileName,postfixFileName);
          if (File_exists(destinationFileName))
          {
            n = 0;
            do
            {
              String_set(destinationFileName,prefixFileName);
              Misc_formatDateTime(destinationFileName,fileInfo.timeModified,"-%H:%M:%S");
              String_appendFormat(destinationFileName,"-%04u",n);
              String_append(destinationFileName,postfixFileName);
              n++;
            }
            while (File_exists(destinationFileName));
          }
          String_delete(postfixFileName);
          String_delete(prefixFileName);
          break;
        case RESTORE_ENTRY_MODE_OVERWRITE:
          // nothing to do
          break;
      }
    }

    printInfo(1,"  Restore special device '%s'...",String_cString(destinationFileName));

    // create special device
    if (!restoreInfo->dryRun)
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
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }

    // set file time, file owner/group
    if (!restoreInfo->dryRun)
    {
      if (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = restoreInfo->jobOptions->owner.userId;
      if (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = restoreInfo->jobOptions->owner.groupId;
      error = File_setInfo(&fileInfo,destinationFileName);
      if (error != ERROR_NONE)
      {
        if (   !restoreInfo->jobOptions->noStopOnErrorFlag
            && !File_isNetworkFileSystem(destinationFileName)
           )
        {
          printInfo(1,"FAIL!\n");
          printError("Cannot set file info of '%s' (error: %s)\n",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
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

    // output result
    if (!restoreInfo->dryRun)
    {
      printInfo(1,"OK\n");
    }
    else
    {
      printInfo(1,"OK (dry-run)\n");
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
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : restoreArchiveContent
* Purpose: restore archive content
* Input  : restoreInfo      - restore info
*          storageSpecifier - storage to restore from
*          archiveName      - archive to restore from or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors restoreArchiveContent(RestoreInfo      *restoreInfo,
                                   StorageSpecifier *storageSpecifier,
                                   ConstString      archiveName
                                  )
{
  AutoFreeList         autoFreeList;
  String               printableStorageName;
  byte                 *buffer;
  StorageInfo          storageInfo;
  Errors               error;
  CryptSignatureStates allCryptSignatureState;
  ArchiveHandle        archiveHandle;
  Errors               failError;
  ArchiveEntryTypes    archiveEntryType;

  assert(restoreInfo != NULL);
  assert(restoreInfo->includeEntryList != NULL);
  assert(restoreInfo->jobOptions != NULL);
  assert(storageSpecifier != NULL);

  // init variables
  AutoFree_init(&autoFreeList);
  printableStorageName = String_new();
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,printableStorageName,{ String_delete(printableStorageName); });
  AUTOFREE_ADD(&autoFreeList,buffer,{ free(buffer); });

  // get printable storage name
  Storage_getPrintableName(printableStorageName,storageSpecifier,archiveName);

  // init status info
  String_set(restoreInfo->statusInfo.storageName,printableStorageName);
  restoreInfo->statusInfo.storageDoneSize  = 0LL;
  restoreInfo->statusInfo.storageTotalSize = 0LL;
  updateStatusInfo(restoreInfo,TRUE);

  // init storage
  error = Storage_init(&storageInfo,
NULL, // masterIO
                       storageSpecifier,
                       restoreInfo->jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK(NULL,NULL),  // updateStatusInfo
                       CALLBACK(restoreInfo->getNamePasswordFunction,restoreInfo->getNamePasswordUserData),
                       CALLBACK(NULL,NULL)  // requestVolume
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize storage '%s' (error: %s)!\n",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    error = handleError(restoreInfo,error);
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&storageInfo,{ Storage_done(&storageInfo); });

  // check if storage exists
  if (!Storage_exists(&storageInfo,archiveName))
  {
    printError("Archive not found '%s'!\n",
               String_cString(printableStorageName)
              );
    AutoFree_cleanup(&autoFreeList);
    return ERROR_ARCHIVE_NOT_FOUND;
  }

  // open archive
  error = Archive_open(&archiveHandle,
                       &storageInfo,
                       archiveName,
                       restoreInfo->deltaSourceList,
                       CALLBACK(restoreInfo->getNamePasswordFunction,restoreInfo->getNamePasswordUserData),
                       restoreInfo->logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot open storage '%s' (error: %s)!\n",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    error = handleError(restoreInfo,error);
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveHandle,{ Archive_close(&archiveHandle); });

  // check signatures
  if (!restoreInfo->jobOptions->skipVerifySignaturesFlag)
  {
    error = Archive_verifySignatures(&archiveHandle,
                                     &allCryptSignatureState
                                    );
    if (error != ERROR_NONE)
    {
      error = handleError(restoreInfo,error);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    if (!Crypt_isValidSignatureState(allCryptSignatureState))
    {
      printError("Invalid signature in '%s'!\n",
                 String_cString(printableStorageName)
                );
      error = handleError(restoreInfo,error);
      AutoFree_cleanup(&autoFreeList);
      return ERROR_INVALID_SIGNATURE;
    }
  }

  // update status info
  restoreInfo->statusInfo.storageTotalSize = Archive_getSize(&archiveHandle);
  updateStatusInfo(restoreInfo,TRUE);

  // read archive entries
  printInfo(0,
            "Restore from '%s'%s",
            String_cString(printableStorageName),
            !isPrintInfo(1) ? "..." : ":\n"
           );
  failError = ERROR_NONE;
  while (   ((failError == ERROR_NONE) || restoreInfo->jobOptions->noStopOnErrorFlag)
         && ((restoreInfo->isAbortedFunction == NULL) || !restoreInfo->isAbortedFunction(restoreInfo->isAbortedUserData))
         && !Archive_eof(&archiveHandle,ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS|(isPrintInfo(3) ? ARCHIVE_FLAG_PRINT_UNKNOWN_CHUNKS : ARCHIVE_FLAG_NONE))
        )
  {
    // pause
    while ((restoreInfo->isPauseFunction != NULL) && restoreInfo->isPauseFunction(restoreInfo->isPauseUserData))
    {
      Misc_udelay(500L*US_PER_MS);
    }

    // get next archive entry type
    error = Archive_getNextArchiveEntry(&archiveHandle,
                                        &archiveEntryType,
                                        NULL,  // archiveCryptInfo
                                        NULL,  // offset
                                        ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS
                                       );
    if (error != ERROR_NONE)
    {
      printError("Cannot read next entry in archive '%s' (error: %s)!\n",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      if (failError == ERROR_NONE) failError = handleError(restoreInfo,error);
      break;
    }

    // update storage status
    restoreInfo->statusInfo.storageDoneSize = Archive_tell(&archiveHandle);
    updateStatusInfo(restoreInfo,TRUE);

    // restore entry
    switch (archiveEntryType)
    {
      case ARCHIVE_ENTRY_TYPE_NONE:
        #ifndef NDEBUG      
          HALT_INTERNAL_ERROR_UNREACHABLE();                          
        #endif /* NDEBUG */                    
        break; /* not reached */
      case ARCHIVE_ENTRY_TYPE_FILE:
        error = restoreFileEntry(restoreInfo,
                                 &archiveHandle,
                                 printableStorageName,
                                 buffer,
                                 BUFFER_SIZE
                                );
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        error = restoreImageEntry(restoreInfo,
                                  &archiveHandle,
                                  printableStorageName,
                                  buffer,
                                  BUFFER_SIZE
                                 );
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        error = restoreDirectoryEntry(restoreInfo,
                                      &archiveHandle,
                                      printableStorageName,
                                      buffer,
                                      BUFFER_SIZE
                                     );
        break;
      case ARCHIVE_ENTRY_TYPE_LINK:
        error = restoreLinkEntry(restoreInfo,
                                 &archiveHandle,
                                 printableStorageName,
                                 buffer,
                                 BUFFER_SIZE
                                );
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        error = restoreHardLinkEntry(restoreInfo,
                                     &archiveHandle,
                                     printableStorageName,
                                     buffer,
                                     BUFFER_SIZE
                                    );
        break;
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
        error = restoreSpecialEntry(restoreInfo,
                                    &archiveHandle,
                                    printableStorageName,
                                    buffer,
                                    BUFFER_SIZE
                                   );
        break;
      case ARCHIVE_ENTRY_TYPE_META:
        error = Archive_skipNextEntry(&archiveHandle);
        break;
      case ARCHIVE_ENTRY_TYPE_SIGNATURE:
        error = Archive_skipNextEntry(&archiveHandle);
        break;
      case ARCHIVE_ENTRY_TYPE_UNKNOWN:
        #ifndef NDEBUG      
          HALT_INTERNAL_ERROR_UNREACHABLE();                          
        #endif /* NDEBUG */                    
        break; /* not reached */
    }
    if (error != ERROR_NONE)
    {
      // handle error
      if (Error_getCode(error) != ERROR_NO_CRYPT_PASSWORD)
      {
        error = handleError(restoreInfo,error);
      }

      // set fail error
      if (error != ERROR_NONE)
      {
        if (failError == ERROR_NONE) failError = error;
      }
    }

    // update storage status
    restoreInfo->statusInfo.storageDoneSize = Archive_tell(&archiveHandle);
    updateStatusInfo(restoreInfo,TRUE);
  }
  if (!isPrintInfo(1)) printInfo(0,"%s",(failError == ERROR_NONE) ? "OK\n" : "FAIL!\n");

  // close archive
  Archive_close(&archiveHandle);

  // done storage
  (void)Storage_done(&storageInfo);

  // free resources
  free(buffer);
  String_delete(printableStorageName);
  AutoFree_done(&autoFreeList);

  return failError;
}

/*---------------------------------------------------------------------*/

Errors Command_restore(const StringList                *storageNameList,
                       const EntryList                 *includeEntryList,
                       const PatternList               *excludePatternList,
                       DeltaSourceList                 *deltaSourceList,
                       JobOptions                      *jobOptions,
                       bool                            dryRun,
                       RestoreUpdateStatusInfoFunction updateStatusInfoFunction,
                       void                            *updateStatusInfoUserData,
                       RestoreHandleErrorFunction      handleErrorFunction,
                       void                            *handleErrorUserData,
                       GetNamePasswordFunction         getNamePasswordFunction,
                       void                            *getNamePasswordUserData,
                       IsPauseFunction                 isPauseFunction,
                       void                            *isPauseUserData,
                       IsAbortedFunction               isAbortedFunction,
                       void                            *isAbortedUserData,
                       LogHandle                       *logHandle
                      )
{
  RestoreInfo                restoreInfo;
  StorageSpecifier           storageSpecifier;
  StringNode                 *stringNode;
  String                     storageName;
  Errors                     error;
  bool                       abortFlag;
  bool                       someStorageFound;
  StorageDirectoryListHandle storageDirectoryListHandle;
  String                     fileName;
  FragmentNode               *fragmentNode;

  assert(storageNameList != NULL);
  assert(includeEntryList != NULL);
  assert(jobOptions != NULL);

  // init variables
  Storage_initSpecifier(&storageSpecifier);

  // init restore info
  initRestoreInfo(&restoreInfo,
                  &storageSpecifier,
                  includeEntryList,
                  excludePatternList,
                  deltaSourceList,
                  jobOptions,
                  dryRun,
                  CALLBACK(updateStatusInfoFunction,updateStatusInfoUserData),
                  CALLBACK(handleErrorFunction,handleErrorUserData),
                  CALLBACK(getNamePasswordFunction,getNamePasswordUserData),
                  CALLBACK(isPauseFunction,isPauseUserData),
                  CALLBACK(isAbortedFunction,isAbortedUserData),
                  logHandle
                 );

  error            = ERROR_NONE;
  abortFlag        = FALSE;
  someStorageFound = FALSE;
  STRINGLIST_ITERATE(storageNameList,stringNode,storageName)
  {
    // pause
    while ((isPauseFunction != NULL) && isPauseFunction(isPauseUserData))
    {
      Misc_udelay(500L*US_PER_MS);
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
      error = restoreArchiveContent(&restoreInfo,
                                    &storageSpecifier,
                                    NULL  // archiveName
                                   );
      if (error != ERROR_NONE)
      {
        if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = handleError(&restoreInfo,error);
      }
      someStorageFound = TRUE;
    }
    else
    {
      // restore all matching archives content
      error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                        &storageSpecifier,
                                        NULL,  // archiveName
                                        jobOptions,
                                        SERVER_CONNECTION_PRIORITY_HIGH
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
          if (!String_isEmpty(storageSpecifier.archivePatternString))
          {
            if (!Pattern_match(&storageSpecifier.archivePattern,fileName,PATTERN_MATCH_MODE_EXACT))
            {
              continue;
            }
          }

          // restore archive content
          error = restoreArchiveContent(&restoreInfo,
                                        &storageSpecifier,
                                        fileName
                                       );
          if (error != ERROR_NONE)
          {
            if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = handleError(&restoreInfo,error);
          }
          someStorageFound = TRUE;
        }
        String_delete(fileName);

        Storage_closeDirectoryList(&storageDirectoryListHandle);
      }
    }

    if (   abortFlag
        || ((isAbortedFunction != NULL) && isAbortedFunction(isAbortedUserData))
        || (restoreInfo.failError != ERROR_NONE)
       )
    {
      break;
    }
  }
  if ((restoreInfo.failError == ERROR_NONE) && !StringList_isEmpty(storageNameList) && !someStorageFound)
  {
    printError("No matching storage files found!\n");
    restoreInfo.failError = ERROR_FILE_NOT_FOUND_;
  }

  if (   (restoreInfo.failError == ERROR_NONE)
      && !jobOptions->noFragmentsCheckFlag
     )
  {
    // check fragment lists, set file info also for incomplete entries
    if ((isAbortedFunction == NULL) || !isAbortedFunction(isAbortedUserData))
    {
      FRAGMENTLIST_ITERATE(&restoreInfo.fragmentList,fragmentNode)
      {
        if (!FragmentList_isEntryComplete(fragmentNode))
        {
          printInfo(0,"Warning: incomplete entry '%s'\n",String_cString(fragmentNode->name));
          if (isPrintInfo(2))
          {
            printInfo(2,"  Fragments:\n");
            FragmentList_print(stdout,4,fragmentNode,TRUE);
          }
          error = ERRORX_(ENTRY_INCOMPLETE,0,"%s",String_cString(fragmentNode->name));
          if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = handleError(&restoreInfo,error);
        }

        if (fragmentNode->userData != NULL)
        {
          // set file time, file owner/group, file permission of incomplete entries
          if (!dryRun)
          {
            if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ((FileInfo*)fragmentNode->userData)->userId  = jobOptions->owner.userId;
            if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ((FileInfo*)fragmentNode->userData)->groupId = jobOptions->owner.groupId;
            error = File_setInfo((FileInfo*)fragmentNode->userData,fragmentNode->name);
            if (error != ERROR_NONE)
            {
              if (   !jobOptions->noStopOnErrorFlag
                  && !File_isNetworkFileSystem(fragmentNode->name)
                 )
              {
                printError("Cannot set file info of '%s' (error: %s)\n",
                           String_cString(fragmentNode->name),
                           Error_getText(error)
                          );
                if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
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

  // get error
  if ((isAbortedFunction == NULL) || !isAbortedFunction(isAbortedUserData))
  {
    error = restoreInfo.failError;
  }
  else
  {
    error = ERROR_ABORTED;
  }

  // free resources
  doneRestoreInfo(&restoreInfo);
  Storage_doneSpecifier(&storageSpecifier);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
