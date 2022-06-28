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
#include <inttypes.h>
#include <errno.h>
#include <assert.h>

#include "common/autofree.h"
#include "common/dictionaries.h"
#include "common/fragmentlists.h"
#include "common/global.h"
#include "common/misc.h"
#include "common/msgqueues.h"
#include "common/patternlists.h"
#include "common/patterns.h"
#include "common/stringlists.h"
#include "common/strings.h"

#include "bar.h"
#include "errors.h"
#include "deltasourcelists.h"
#include "common/files.h"
#include "archive.h"

#include "commands_restore.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// file data buffer size
#define BUFFER_SIZE (64*1024)

// max. number of entry messages
#define MAX_ENTRY_MSG_QUEUE 256

/***************************** Datatypes *******************************/

// restore information
typedef struct
{
  StorageSpecifier                *storageSpecifier;                    // storage specifier structure
  const EntryList                 *includeEntryList;                    // list of included entries
  const PatternList               *excludePatternList;                  // list of exclude patterns
  JobOptions                      *jobOptions;

  LogHandle                       *logHandle;                           // log handle

  Semaphore                       namesDictionaryLock;
  Dictionary                      namesDictionary;                      // dictionary with files (used for detecting overwrite existing files)

  RestoreUpdateStatusInfoFunction updateStatusInfoFunction;             // update status info call-back
  void                            *updateStatusInfoUserData;            // user data for update status info call-back
  RestoreHandleErrorFunction      handleErrorFunction;                  // handle error call-back
  void                            *handleErrorUserData;                 // user data for handle error call-back
  GetNamePasswordFunction         getNamePasswordFunction;              // get name/password call-back
  void                            *getNamePasswordUserData;             // user data for get password call-back
  IsPauseFunction                 isPauseFunction;                      // check for pause call-back
  void                            *isPauseUserData;                     // user data for check for pause call-back
  IsAbortedFunction               isAbortedFunction;                    // check for aborted call-back
  void                            *isAbortedUserData;                   // user data for check for aborted call-back

  MsgQueue                        entryMsgQueue;                        // queue with entries to store

  Errors                          failError;                            // failure error

  Semaphore                       fragmentListLock;
  FragmentList                    fragmentList;                         // entry fragments

  Semaphore                       statusInfoLock;
  StatusInfo                      statusInfo;                           // status info
  const FragmentNode              *statusInfoCurrentFragmentNode;       // current fragment node in status info
  uint64                          statusInfoCurrentLastUpdateTimestamp; // timestamp of last update current fragment node
} RestoreInfo;

// entry message send to restore threads
typedef struct
{
  uint                   archiveIndex;
  const ArchiveHandle    *archiveHandle;
  ArchiveEntryTypes      archiveEntryType;
  const ArchiveCryptInfo *archiveCryptInfo;
  uint64                 offset;
} EntryMsg;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : freeEntryMsg
* Purpose: free file entry message call back
* Input  : entryMsg - entry message
*          userData - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeEntryMsg(EntryMsg *entryMsg, void *userData)
{
  assert(entryMsg != NULL);

  UNUSED_VARIABLE(entryMsg);
  UNUSED_VARIABLE(userData);
}

/***********************************************************************\
* Name   : initRestoreInfo
* Purpose: initialize restore info
* Input  : restoreInfo                - restore info variable
*          storageSpecifier           - storage specifier structure
*          includeEntryList           - include entry list
*          excludePatternList         - exclude pattern list
*          compressExcludePatternList - exclude compression pattern list
*          jobOptions                 - job options
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
                           JobOptions                      *jobOptions,
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
  restoreInfo->storageSpecifier                     = storageSpecifier;
  restoreInfo->includeEntryList                     = includeEntryList;
  restoreInfo->excludePatternList                   = excludePatternList;
  restoreInfo->jobOptions                           = jobOptions;

  restoreInfo->logHandle                            = logHandle;

  restoreInfo->failError                            = ERROR_NONE;

  restoreInfo->updateStatusInfoFunction             = updateStatusInfoFunction;
  restoreInfo->updateStatusInfoUserData             = updateStatusInfoUserData;
  restoreInfo->handleErrorFunction                  = handleErrorFunction;
  restoreInfo->handleErrorUserData                  = handleErrorUserData;
  restoreInfo->getNamePasswordFunction              = getNamePasswordFunction;
  restoreInfo->getNamePasswordUserData              = getNamePasswordUserData;
  restoreInfo->isPauseFunction                      = isPauseFunction;
  restoreInfo->isPauseUserData                      = isPauseUserData;
  restoreInfo->isAbortedFunction                    = isAbortedFunction;
  restoreInfo->isAbortedUserData                    = isAbortedUserData;

  if (!Semaphore_init(&restoreInfo->namesDictionaryLock,SEMAPHORE_TYPE_BINARY))
  {
    HALT_FATAL_ERROR("Cannot initialize name dictionary semaphore!");
  }
  Dictionary_init(&restoreInfo->namesDictionary,DICTIONARY_BYTE_COPY,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));

  if (!Semaphore_init(&restoreInfo->fragmentListLock,SEMAPHORE_TYPE_BINARY))
  {
    HALT_FATAL_ERROR("Cannot initialize fragment list semaphore!");
  }
  FragmentList_init(&restoreInfo->fragmentList);

  if (!Semaphore_init(&restoreInfo->statusInfoLock,SEMAPHORE_TYPE_BINARY))
  {
    HALT_FATAL_ERROR("Cannot initialize status info semaphore!");
  }
  initStatusInfo(&restoreInfo->statusInfo);
  restoreInfo->statusInfoCurrentFragmentNode        = NULL;
  restoreInfo->statusInfoCurrentLastUpdateTimestamp = 0LL;

  // init entry name queue, storage queue
  if (!MsgQueue_init(&restoreInfo->entryMsgQueue,
                     MAX_ENTRY_MSG_QUEUE,
                     CALLBACK_((MsgQueueMsgFreeFunction)freeEntryMsg,NULL)
                    )
     )
  {
    HALT_FATAL_ERROR("Cannot initialize entry message queue!");
  }

  DEBUG_ADD_RESOURCE_TRACE(restoreInfo,RestoreInfo);
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

  DEBUG_REMOVE_RESOURCE_TRACE(restoreInfo,RestoreInfo);

  Semaphore_done(&restoreInfo->statusInfoLock);
  MsgQueue_done(&restoreInfo->entryMsgQueue);

  doneStatusInfo(&restoreInfo->statusInfo);

  FragmentList_done(&restoreInfo->fragmentList);
  Semaphore_done(&restoreInfo->fragmentListLock);
  Dictionary_done(&restoreInfo->namesDictionary);
  Semaphore_done(&restoreInfo->namesDictionaryLock);
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

#if 0
//TODO: remove?
/***********************************************************************\
* Name   : statusInfoUpdateLock
* Purpose: lock status info update
* Input  : createInfo   - create info structure
*          fragmentNode - fragment node (can be NULL)
* Output : -
* Return : always TRUE
* Notes  : -
\***********************************************************************/

LOCAL SemaphoreLock statusInfoUpdateLock(RestoreInfo *restoreInfo, ConstString name, FragmentNode **foundFragmentNode)
{
  assert(restoreInfo != NULL);

  // lock
  Semaphore_lock(&restoreInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);

  if (foundFragmentNode != NULL)
  {
    // lock
    Semaphore_lock(&restoreInfo->fragmentListLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);

    // find fragment node
    (*foundFragmentNode) = FragmentList_find(&restoreInfo->fragmentList,name);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : statusInfoUpdateUnlock
* Purpose: status info update unlock
* Input  : createInfo   - create info structure
*          name         - name of entry
*          fragmentNode - fragment node (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void statusInfoUpdateUnlock(RestoreInfo *restoreInfo, ConstString name, const FragmentNode *fragmentNode)
{
  const FragmentNode *fragmentNode;

  assert(restoreInfo != NULL);

  if (name != NULL)
  {
    // update current status info if not set or timeout
    if (   (restoreInfo->statusInfoCurrentFragmentNode == NULL)
        || ((Misc_getTimestamp()-restoreInfo->statusInfoCurrentLastUpdateTimestamp) >= 10*US_PER_S)
       )
    {
      // set new current status info
      String_set(restoreInfo->statusInfo.entry.name,name);
      if (fragmentNode != NULL)
      {
        restoreInfo->statusInfo.entry.doneSize  = FragmentList_getSize(fragmentNode);
        restoreInfo->statusInfo.entry.totalSize = FragmentList_getTotalSize(fragmentNode);

        restoreInfo->statusInfoCurrentFragmentNode = !FragmentList_isComplete(fragmentNode) ? fragmentNode : NULL;
      }
      else
      {
        restoreInfo->statusInfoCurrentFragmentNode = NULL;
      }

      // save last update time
      restoreInfo->statusInfoCurrentLastUpdateTimestamp = Misc_getTimestamp();
    }
  }

  // update status info
  updateStatusInfo(restoreInfo,TRUE);

  // unlock
  if (fragmentNode != NULL)
  {
    Semaphore_unlock(&restoreInfo->fragmentListLock);
  }
  Semaphore_unlock(&restoreInfo->statusInfoLock);
}

/***********************************************************************\
* Name   : STATUS_INFO_UPDATE
* Purpose: update status info
* Input  : restoreInfo  - restore info structure
*          name         - name of entry
*          fragmentNode - fragment node variable (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define STATUS_INFO_UPDATE(restoreInfo,name,fragmentNode) \
  for (SemaphoreLock semaphoreLock = statusInfoUpdateLock(restoreInfo,name,fragmentNode); \
       semaphoreLock; \
       statusInfoUpdateUnlock(restoreInfo,name), semaphoreLock = FALSE \
      )
#endif

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
* Name   : createParentDirectories
* Purpose: create parent directories for file if it does not exists
* Input  : restoreInfo    - restore info
*          fileName       - file name
*          userId,groupId - user/group id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createParentDirectories(RestoreInfo *restoreInfo,
                                     ConstString fileName,
                                     uint32      userId,
                                     uint32      groupId
                                    )
{
  String parentDirectoryName;
  String directoryName;
  Errors error;

  assert(fileName != NULL);

  parentDirectoryName = File_getDirectoryName(String_new(),fileName);
  if (!String_isEmpty(parentDirectoryName))
  {
    SEMAPHORE_LOCKED_DO(&restoreInfo->namesDictionaryLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      // add all directories to known names
      directoryName = String_duplicate(parentDirectoryName);
      while (!String_isEmpty(directoryName))
      {
        if (!Dictionary_contains(&restoreInfo->namesDictionary,
                                 String_cString(directoryName),
                                 String_length(directoryName)
                                )
           )
        {
          Dictionary_add(&restoreInfo->namesDictionary,
                         String_cString(directoryName),
                         String_length(directoryName),
                         NULL,
                         0
                        );
        }

        File_getDirectoryName(directoryName,directoryName);
      }
      String_delete(directoryName);
    }

    if (!File_exists(parentDirectoryName))
    {
      // create parent directories (ignore error if it already exists now)
      error = File_makeDirectory(parentDirectoryName,
                                 FILE_DEFAULT_USER_ID,
                                 FILE_DEFAULT_GROUP_ID,
                                 FILE_DEFAULT_PERMISSIONS,
                                 TRUE
                                );
      if (error != ERROR_NONE)
      {
        String_delete(parentDirectoryName);
        Semaphore_unlock(&restoreInfo->namesDictionaryLock);
        return error;
      }

      // set parent directory owner/group
      error = File_setOwner(parentDirectoryName,
                            (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : userId,
                            (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : groupId
                           );
      if (error != ERROR_NONE)
      {
        String_delete(parentDirectoryName);
        Semaphore_unlock(&restoreInfo->namesDictionaryLock);
        return error;
      }
    }

  }

  // free resources
  String_delete(parentDirectoryName);

  return ERROR_NONE;
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
//            FileInfo                      localFileInfo;
  FileModes                 fileMode;
  FileHandle                fileHandle;
  uint64                    length;
  ulong                     bufferLength;
  bool                      isComplete;
  char                      sizeString[32];
  char                      fragmentString[256];

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
    printError("cannot read 'file' content of archive '%s' (error: %s)",
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
    SEMAPHORE_LOCKED_DO(&restoreInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      String_set(restoreInfo->statusInfo.entry.name,destinationFileName);
      restoreInfo->statusInfo.entry.doneSize  = 0LL;
      restoreInfo->statusInfo.entry.totalSize = fragmentSize;
      updateStatusInfo(restoreInfo,TRUE);
    }

    // restore file
    printInfo(1,"  Restore file      '%s'...",String_cString(destinationFileName));

    // check if file already exists
    SEMAPHORE_LOCKED_DO(&restoreInfo->namesDictionaryLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      if (!Dictionary_contains(&restoreInfo->namesDictionary,
                               String_cString(destinationFileName),
                               String_length(destinationFileName)
                              )
         )
      {
        if (File_exists(destinationFileName))
        {
          switch (restoreInfo->jobOptions->restoreEntryMode)
          {
            case RESTORE_ENTRY_MODE_STOP:
              // stop
              printInfo(1,"stopped (file exists)\n");
              error = !restoreInfo->jobOptions->noStopOnErrorFlag
                        ? ERRORX_(FILE_EXISTS_,0,"%s",String_cString(destinationFileName))
                        : ERROR_NONE;
              Semaphore_unlock(&restoreInfo->namesDictionaryLock);
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
              String_append(destinationFileName,postfixFileName);
              if (File_exists(destinationFileName))
              {
                n = 0;
                do
                {
                  String_set(destinationFileName,prefixFileName);
                  String_appendFormat(destinationFileName,"-%u",n);
                  String_append(destinationFileName,postfixFileName);
                  n++;
                }
                while (File_exists(destinationFileName));
              }
              String_delete(postfixFileName);
              String_delete(prefixFileName);
              break;
            case RESTORE_ENTRY_MODE_OVERWRITE:
              // truncate to 0-file
              error = File_open(&fileHandle,destinationFileName,FILE_OPEN_CREATE);
              if (error != ERROR_NONE)
              {
                Semaphore_unlock(&restoreInfo->namesDictionaryLock);
                AutoFree_cleanup(&autoFreeList);
                return error;
              }
              (void)File_close(&fileHandle);
              break;
            case RESTORE_ENTRY_MODE_SKIP_EXISTING:
              // skip
              printInfo(1,"skipped (file exists)\n");
              Semaphore_unlock(&restoreInfo->namesDictionaryLock);
              AutoFree_cleanup(&autoFreeList);
              return ERROR_NONE;
          }
        }

        Dictionary_add(&restoreInfo->namesDictionary,
                       String_cString(destinationFileName),
                       String_length(destinationFileName),
                       NULL,
                       0
                      );
      }
    }

    // check if file fragment already exists
    if (!restoreInfo->jobOptions->noFragmentsCheckFlag)
    {
      // check if fragment already exist -> get/create file fragment node
      SEMAPHORE_LOCKED_DO(&restoreInfo->fragmentListLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        FragmentNode *fragmentNode;

        fragmentNode = FragmentList_find(&restoreInfo->fragmentList,fileName);
        if (fragmentNode != NULL)
        {
          if (FragmentList_rangeExists(fragmentNode,fragmentOffset,fragmentSize))
          {
            switch (restoreInfo->jobOptions->restoreEntryMode)
            {
              case RESTORE_ENTRY_MODE_STOP:
                // stop
                printInfo(1,
                          "stopped (file part %llu..%llu exists)\n",
                          fragmentOffset,
                          (fragmentSize > 0LL) ? fragmentOffset+fragmentSize-1 : fragmentOffset
                         );
                Semaphore_unlock(&restoreInfo->fragmentListLock);
                AutoFree_cleanup(&autoFreeList);
                return !restoreInfo->jobOptions->noStopOnErrorFlag ? ERROR_FILE_EXISTS_ : ERROR_NONE;
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
                String_append(destinationFileName,postfixFileName);
                if (File_exists(destinationFileName))
                {
                  n = 0;
                  do
                  {
                    String_set(destinationFileName,prefixFileName);
                    String_appendFormat(destinationFileName,"-%u",n);
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
              case RESTORE_ENTRY_MODE_SKIP_EXISTING:
                // skip
                printInfo(1,
                          "skipped (file part %llu..%llu exists)\n",
                          fragmentOffset,
                          (fragmentSize > 0LL) ? fragmentOffset+fragmentSize-1 : fragmentOffset
                         );
                Semaphore_unlock(&restoreInfo->fragmentListLock);
                AutoFree_cleanup(&autoFreeList);
                return ERROR_NONE;
                break;
            }
          }
        }
        else
        {
          fragmentNode = FragmentList_add(&restoreInfo->fragmentList,
                                          fileName,
                                          fileInfo.size,
                                          &fileInfo,sizeof(FileInfo),
                                          0
                                         );
        }
        assert(fragmentNode != NULL);
      }
    }

    // create parent directories if not existing
    if (!restoreInfo->jobOptions->dryRun)
    {
      error = createParentDirectories(restoreInfo,destinationFileName,
                                      (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : fileInfo.userId,
                                      (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : fileInfo.groupId
                                     );
      if (error != ERROR_NONE)
      {
        if (!restoreInfo->jobOptions->noStopOnErrorFlag)
        {
          printInfo(1,"FAIL!\n");
          printError("cannot set owner/group of parent directory for '%s' (error: %s)",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        else
        {
          printWarning("cannot set owner/group of parent directory for '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
        }
      }
    }

    if (!restoreInfo->jobOptions->dryRun)
    {
      // temporary change owner+permissions for writing (ignore errors)
      (void)File_setPermission(destinationFileName,FILE_PERMISSION_USER_READ|FILE_PERMISSION_USER_WRITE);
      (void)File_setOwner(destinationFileName,FILE_OWN_USER_ID,FILE_OWN_GROUP_ID);

      // open file
      fileMode = FILE_OPEN_WRITE;
      if (restoreInfo->jobOptions->sparseFilesFlag) fileMode |= FILE_SPARSE;
      error = File_open(&fileHandle,destinationFileName,fileMode);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("cannot create/write to file '%s' (error: %s)",
                   String_cString(destinationFileName),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      AUTOFREE_ADD(&autoFreeList,&fileHandle,{ (void)File_close(&fileHandle); });

      // set file length for sparse files
      if (restoreInfo->jobOptions->sparseFilesFlag)
      {
        error = File_truncate(&fileHandle,fileInfo.size);
        if (error != ERROR_NONE)
        {
          printInfo(1,"FAIL!\n");
          printError("cannot create/write to file '%s' (error: %s)",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
      }

      // seek to fragment position
      error = File_seek(&fileHandle,fragmentOffset);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("cannot write content of file '%s' (error: %s)",
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
        printError("cannot read content of file '%s' (error: %s)!",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
        break;
      }
      if (!restoreInfo->jobOptions->dryRun)
      {
        error = File_write(&fileHandle,buffer,bufferLength);
        if (error != ERROR_NONE)
        {
          printInfo(1,"FAIL!\n");
          printError("cannot write content of file '%s' (error: %s)",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          break;
        }
      }

      // update status info
      restoreInfo->statusInfo.entry.doneSize += (uint64)bufferLength;
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
    if (!restoreInfo->jobOptions->dryRun)
    {
      if (File_getSize(&fileHandle) > fileInfo.size)
      {
        File_truncate(&fileHandle,fileInfo.size);
      }
    }

    // close file
    if (!restoreInfo->jobOptions->dryRun)
    {
      AUTOFREE_REMOVE(&autoFreeList,&fileHandle);
      (void)File_close(&fileHandle);
    }

    // add fragment to file fragment list
    isComplete = FALSE;
    SEMAPHORE_LOCKED_DO(&restoreInfo->fragmentListLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      FragmentNode *fragmentNode;

      fragmentNode = FragmentList_find(&restoreInfo->fragmentList,fileName);
      if (fragmentNode != NULL)
      {
        FragmentList_addRange(fragmentNode,fragmentOffset,fragmentSize);
//FragmentList_debugPrintInfo(fragmentNode,String_cString(fileName));

        if (FragmentList_isComplete(fragmentNode))
        {
          FragmentList_discard(&restoreInfo->fragmentList,fragmentNode);
          isComplete = TRUE;
        }
      }
      else
      {
        isComplete = TRUE;
      }
    }

    if (!restoreInfo->jobOptions->dryRun)
    {
      if (isComplete)
      {
        // set file time, file permission
        if (globalOptions.permissions != FILE_DEFAULT_PERMISSIONS)
        {
          fileInfo.permissions = globalOptions.permissions;
        }
        error = File_setInfo(&fileInfo,destinationFileName);
        if (error != ERROR_NONE)
        {
          if (   !restoreInfo->jobOptions->noStopOnErrorFlag
              && !File_isNetworkFileSystem(destinationFileName)
             )
          {
            printInfo(1,"FAIL!\n");
            printError("cannot set file info of '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
            AutoFree_cleanup(&autoFreeList);
            return error;
          }
          else
          {
            printWarning("cannot set file info of '%s' (error: %s)",
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
          }
        }

        // set file owner/group
        error = File_setOwner(destinationFileName,
                              (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : fileInfo.userId,
                              (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : fileInfo.groupId
                             );
        if (error != ERROR_NONE)
        {
          if (   !restoreInfo->jobOptions->noStopOnOwnerErrorFlag
              && !File_isNetworkFileSystem(destinationFileName)
             )
          {
            printInfo(1,"FAIL!\n");
            printError("cannot set owner/group of file '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
            AutoFree_cleanup(&autoFreeList);
            return error;
          }
          else
          {
            printWarning("cannot set owner/group of file '%s' (error: %s)",
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
          }
        }

        // set attributes
        error = File_setAttributes(fileInfo.attributes,destinationFileName);
        if (error != ERROR_NONE)
        {
          if (   !restoreInfo->jobOptions->noStopOnAttributeErrorFlag
              && !File_isNetworkFileSystem(destinationFileName)
             )
          {
            printInfo(1,"FAIL!\n");
            printError("cannot set file attributes of '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
            AutoFree_cleanup(&autoFreeList);
            return error;
          }
          else
          {
            printWarning("cannot set file attributes of '%s' (error: %s)",
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
          }
        }
      }
    }

    // get size/fragment info
    if (globalOptions.humanFormatFlag)
    {
      getHumanSizeString(sizeString,sizeof(sizeString),fileInfo.size);
    }
    else
    {
      stringFormat(sizeString,sizeof(sizeString),"%*"PRIu64,stringInt64Length(globalOptions.fragmentSize),fileInfo.size);
    }
    stringClear(fragmentString);
    if (fragmentSize < fileInfo.size)
    {
      stringFormat(fragmentString,sizeof(fragmentString),
                   ", fragment %*"PRIu64"..%*"PRIu64,
                   stringInt64Length(fileInfo.size),fragmentOffset,
                   stringInt64Length(fileInfo.size),fragmentOffset+fragmentSize-1LL
                  );
    }

    // output result
    if (!restoreInfo->jobOptions->dryRun)
    {
      printInfo(1,"OK (%s bytes%s)\n",sizeString,fragmentString);
    }
    else
    {
      printInfo(1,"OK (%s bytes%s, dry-run)\n",sizeString,fragmentString);
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
      printWarning("unexpected data at end of file entry '%S'",fileName);
    }

    // free resources
    String_delete(destinationFileName);
  }
  else
  {
    // skip
    printInfo(2,"  Restore file      '%s'...skipped\n",String_cString(fileName));

    restoreInfo->statusInfo.skipped.count++;
    restoreInfo->statusInfo.skipped.size += (uint64)fileInfo.size;
    updateStatusInfo(restoreInfo,FALSE);
  }

  // close archive file, free resources
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'file' entry fail (error: %s)",Error_getText(error));
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
  enum
  {
    DEVICE,
    FILE,
    UNKNOWN
  }                type;
  DeviceHandle     deviceHandle;
  FileModes        fileMode;
  FileHandle       fileHandle;
  uint64           block;
  ulong            bufferBlockCount;
  char             sizeString[32];
  char             fragmentString[256];

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
    printError("cannot read 'image' content of archive '%s' (error: %s)!",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo,{ (void)Archive_closeEntry(&archiveEntryInfo); });
  if (deviceInfo.blockSize > bufferSize)
  {
    printError("device block size %llu on '%s' is too big (max: %llu)",
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
    SEMAPHORE_LOCKED_DO(&restoreInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      String_set(restoreInfo->statusInfo.entry.name,destinationDeviceName);
      restoreInfo->statusInfo.entry.doneSize  = 0LL;
      restoreInfo->statusInfo.entry.totalSize = blockCount;
      updateStatusInfo(restoreInfo,TRUE);
    }

    // restore image
    printInfo(1,"  Restore image     '%s'...",String_cString(destinationDeviceName));

    if (!restoreInfo->jobOptions->noFragmentsCheckFlag)
    {
      // check if image fragment already exists
      SEMAPHORE_LOCKED_DO(&restoreInfo->fragmentListLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        FragmentNode *fragmentNode;

        fragmentNode = FragmentList_find(&restoreInfo->fragmentList,deviceName);
        if (fragmentNode != NULL)
        {
          if (FragmentList_rangeExists(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize))
          {
            switch (restoreInfo->jobOptions->restoreEntryMode)
            {
              case RESTORE_ENTRY_MODE_STOP:
                // stop
                printInfo(1,
                          "stopped (image part %llu..%llu exists)\n",
                          String_cString(destinationDeviceName),
                          blockOffset*(uint64)deviceInfo.blockSize,
                          ((blockCount > 0) ? blockOffset+blockCount-1:blockOffset)*(uint64)deviceInfo.blockSize
                         );
                Semaphore_unlock(&restoreInfo->fragmentListLock);
                AutoFree_cleanup(&autoFreeList);
                return !restoreInfo->jobOptions->noStopOnErrorFlag ? ERROR_FILE_EXISTS_ : ERROR_NONE;
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
                if (!File_isDevice(destinationDeviceName))
                {
                  // truncate to 0-file
                  error = File_open(&fileHandle,destinationDeviceName,FILE_OPEN_CREATE);
                  if (error != ERROR_NONE)
                  {
                    Semaphore_unlock(&restoreInfo->namesDictionaryLock);
                    AutoFree_cleanup(&autoFreeList);
                    return error;
                  }
                  (void)File_close(&fileHandle);
                }
                break;
              case RESTORE_ENTRY_MODE_SKIP_EXISTING:
                // skip
                printInfo(1,
                          "skipped (image part %llu..%llu exists)\n",
                          blockOffset*(uint64)deviceInfo.blockSize,
                          ((blockCount > 0) ? blockOffset+blockCount-1:blockOffset)*(uint64)deviceInfo.blockSize
                         );
                Semaphore_unlock(&restoreInfo->fragmentListLock);
                AutoFree_cleanup(&autoFreeList);
                return ERROR_NONE;
            }
          }
        }
        else
        {
          fragmentNode = FragmentList_add(&restoreInfo->fragmentList,
                                          deviceName,
                                          deviceInfo.size,
                                          NULL,0,
                                          0
                                         );
        }
        assert(fragmentNode != NULL);
      }
    }

    // create parent directories if not existing
    if (!restoreInfo->jobOptions->dryRun)
    {
      error = createParentDirectories(restoreInfo,destinationDeviceName,
                                      (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : deviceInfo.userId,
                                      (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : deviceInfo.groupId
                                     );
      if (error != ERROR_NONE)
      {
        if (!restoreInfo->jobOptions->noStopOnErrorFlag)
        {
          printInfo(1,"FAIL!\n");
          printError("cannot set owner/group of parent directory for '%s' (error: %s)",
                     String_cString(destinationDeviceName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        else
        {
          printWarning("cannot set owner/group of parent directory for '%s' (error: %s)",
                       String_cString(destinationDeviceName),
                       Error_getText(error)
                      );
        }
      }
    }

    type = UNKNOWN;
    if (!restoreInfo->jobOptions->dryRun)
    {
      if (File_isDevice(destinationDeviceName))
      {
        // open device
        error = Device_open(&deviceHandle,destinationDeviceName,DEVICE_OPEN_WRITE);
        if (error != ERROR_NONE)
        {
          printInfo(1,"FAIL!\n");
          printError("cannot open to device '%s' (error: %s)",
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
        fileMode = FILE_OPEN_WRITE;
        if (restoreInfo->jobOptions->sparseFilesFlag) fileMode |= FILE_SPARSE;
        error = File_open(&fileHandle,destinationDeviceName,fileMode);
        if (error != ERROR_NONE)
        {
          printInfo(1,"FAIL!\n");
          printError("cannot create/write to file '%s' (error: %s)",
                     String_cString(destinationDeviceName),
                     Error_getText(error)
                    );
        }
        type = FILE;
        AUTOFREE_ADD(&autoFreeList,&fileHandle,{ (void)File_close(&fileHandle); });

        // set file length for sparse files
        if (restoreInfo->jobOptions->sparseFilesFlag)
        {
          error = File_truncate(&fileHandle,deviceInfo.size);
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL!\n");
            printError("cannot create/write to file '%s' (error: %s)",
                       String_cString(destinationDeviceName),
                       Error_getText(error)
                      );
            AutoFree_cleanup(&autoFreeList);
            return error;
          }
        }
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
            printError("cannot write content to device '%s' (error: %s)",
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
            printError("cannot write content of file '%s' (error: %s)",
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
        printError("cannot read content of image '%s' (error: %s)!",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
        break;
      }

      if (!restoreInfo->jobOptions->dryRun)
      {
        // write data to device
        switch (type)
        {
          case DEVICE:
            error = Device_write(&deviceHandle,buffer,bufferBlockCount*deviceInfo.blockSize);
            if (error != ERROR_NONE)
            {
              printInfo(1,"FAIL!\n");
              printError("cannot write content to device '%s' (error: %s)",
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
              printError("cannot write content of file '%s' (error: %s)",
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
      SEMAPHORE_LOCKED_DO(&restoreInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        restoreInfo->statusInfo.entry.doneSize += bufferBlockCount*deviceInfo.blockSize;
        updateStatusInfo(restoreInfo,FALSE);
      }

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
    if (!restoreInfo->jobOptions->dryRun)
    {
      switch (type)
      {
        case DEVICE:
          AUTOFREE_REMOVE(&autoFreeList,&deviceHandle);
          (void)Device_close(&deviceHandle);
          break;
        case FILE:
          AUTOFREE_REMOVE(&autoFreeList,&fileHandle);
          (void)File_close(&fileHandle);
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break;
      }
    }

    // add fragment to file fragment list
    SEMAPHORE_LOCKED_DO(&restoreInfo->fragmentListLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      FragmentNode *fragmentNode;

      fragmentNode = FragmentList_find(&restoreInfo->fragmentList,deviceName);
      if (fragmentNode != NULL)
      {
        FragmentList_addRange(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize);
  //FragmentList_debugPrintInfo(fragmentNode,String_cString(fileName));

        // discard fragment list if file is complete
        if (FragmentList_isComplete(fragmentNode))
        {
          FragmentList_discard(&restoreInfo->fragmentList,fragmentNode);
        }
      }
    }

    // get size/fragment info
    if (globalOptions.humanFormatFlag)
    {
      getHumanSizeString(sizeString,sizeof(sizeString),blockCount*deviceInfo.blockSize);
    }
    else
    {
      stringFormat(sizeString,sizeof(sizeString),"%*"PRIu64,stringInt64Length(globalOptions.fragmentSize),blockCount*deviceInfo.blockSize);
    }
    stringClear(fragmentString);
    if ((blockCount*deviceInfo.blockSize) < deviceInfo.size)
    {
      stringFormat(fragmentString,sizeof(fragmentString),
                   ", fragment %*"PRIu64"..%*"PRIu64,
                   stringInt64Length(deviceInfo.size),blockOffset*deviceInfo.blockSize,
                   stringInt64Length(deviceInfo.size),blockOffset*deviceInfo.blockSize+(blockCount*deviceInfo.blockSize)-1LL
                  );
    }

    // output result
    if (!restoreInfo->jobOptions->dryRun)
    {
      printInfo(1,"OK (%s bytes%s)\n",sizeString,fragmentString);
    }
    else
    {
      printInfo(1,"OK (%s bytes%s, dry-run)\n",sizeString,fragmentString);
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
      printWarning("unexpected data at end of image entry '%S'",deviceName);
    }

    // free resources
    String_delete(destinationDeviceName);
  }
  else
  {
    // skip
    printInfo(2,"  Restore image     '%s'...skipped\n",String_cString(deviceName));
  }

  // close archive file
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'image' entry fail (error: %s)",Error_getText(error));
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
                                   ConstString   printableStorageName
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
    printError("cannot read 'directory' content of archive '%s' (error: %s)!",
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
    SEMAPHORE_LOCKED_DO(&restoreInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      String_set(restoreInfo->statusInfo.entry.name,destinationFileName);
      restoreInfo->statusInfo.entry.doneSize  = 0LL;
      restoreInfo->statusInfo.entry.totalSize = 0LL;
      updateStatusInfo(restoreInfo,TRUE);
    }

    // restore directory
    printInfo(1,"  Restore directory '%s'...",String_cString(destinationFileName));

    // check if directory already exists
    SEMAPHORE_LOCKED_DO(&restoreInfo->namesDictionaryLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      if (!Dictionary_contains(&restoreInfo->namesDictionary,
                               String_cString(destinationFileName),
                               String_length(destinationFileName)
                              )
         )
      {
        if (File_exists(destinationFileName))
        {
          switch (restoreInfo->jobOptions->restoreEntryMode)
          {
            case RESTORE_ENTRY_MODE_STOP:
              // stop
              printInfo(1,"stopped (directory exists)\n");
              error = !restoreInfo->jobOptions->noStopOnErrorFlag
                        ? ERRORX_(FILE_EXISTS_,0,"%s",String_cString(destinationFileName))
                        : ERROR_NONE;
              Semaphore_unlock(&restoreInfo->namesDictionaryLock);
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
              String_append(destinationFileName,postfixFileName);
              if (File_exists(destinationFileName))
              {
                n = 0;
                do
                {
                  String_set(destinationFileName,prefixFileName);
                  String_appendFormat(destinationFileName,"-%u",n);
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
            case RESTORE_ENTRY_MODE_SKIP_EXISTING:
              // skip
              printInfo(1,"skipped (directory exists)\n");
              Semaphore_unlock(&restoreInfo->namesDictionaryLock);
              AutoFree_cleanup(&autoFreeList);
              return ERROR_NONE;
          }
        }

        Dictionary_add(&restoreInfo->namesDictionary,
                       String_cString(destinationFileName),
                       String_length(destinationFileName),
                       NULL,
                       0
                      );
      }
    }

    // create parent directories if not existing
    if (!restoreInfo->jobOptions->dryRun)
    {
      error = createParentDirectories(restoreInfo,destinationFileName,
                                      (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : fileInfo.userId,
                                      (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : fileInfo.groupId
                                     );
      if (error != ERROR_NONE)
      {
        if (!restoreInfo->jobOptions->noStopOnErrorFlag)
        {
          printInfo(1,"FAIL!\n");
          printError("cannot set owner/group of parent directory for '%s' (error: %s)",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        else
        {
          printWarning("cannot set owner/group of parent directory for '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
        }
      }
    }

    // create directory
    if (!restoreInfo->jobOptions->dryRun)
    {
      error = File_makeDirectory(destinationFileName,
                                 FILE_DEFAULT_USER_ID,
                                 FILE_DEFAULT_GROUP_ID,
                                 fileInfo.permissions,
                                 FALSE
                                );
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("cannot create directory '%s' (error: %s)",
                   String_cString(destinationFileName),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }

    if (!restoreInfo->jobOptions->dryRun)
    {
      // set file time, file permission
      if (globalOptions.permissions != FILE_DEFAULT_PERMISSIONS)
      {
        fileInfo.permissions = globalOptions.permissions;
      }
      error = File_setInfo(&fileInfo,destinationFileName);
      if (error != ERROR_NONE)
      {
        if (   !restoreInfo->jobOptions->noStopOnErrorFlag
            && !File_isNetworkFileSystem(destinationFileName)
           )
        {
          printInfo(1,"FAIL!\n");
          printError("cannot set directory info of '%s' (error: %s)",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        else
        {
          printWarning("cannot set directory info of '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
        }
      }

      // set directory owner/group
      error = File_setOwner(destinationFileName,
                            (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : fileInfo.userId,
                            (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : fileInfo.groupId
                           );
      if (error != ERROR_NONE)
      {
        if (   !restoreInfo->jobOptions->noStopOnOwnerErrorFlag
            && !File_isNetworkFileSystem(destinationFileName)
           )
        {
          printInfo(1,"FAIL!\n");
          printError("cannot set owner/group of directory '%s' (error: %s)",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        else
        {
          printWarning("cannot set owner/group of directory '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
        }
      }

      // set attributes
      error = File_setAttributes(fileInfo.attributes,destinationFileName);
      if (error != ERROR_NONE)
      {
        if (   !restoreInfo->jobOptions->noStopOnAttributeErrorFlag
            && !File_isNetworkFileSystem(destinationFileName)
           )
        {
          printInfo(1,"FAIL!\n");
          printError("cannot set directory attributes of '%s' (error: %s)",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        else
        {
          printWarning("cannot set directory attributes of '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
        }
      }
    }

    // output result
    if (!restoreInfo->jobOptions->dryRun)
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
      printWarning("unexpected data at end of directory entry '%S'",directoryName);
    }

    // free resources
    String_delete(destinationFileName);
  }
  else
  {
    // skip
    printInfo(2,"  Restore directory '%s'...skipped\n",String_cString(directoryName));
  }

  // close archive file
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'directory' entry fail (error: %s)",Error_getText(error));
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
                              ConstString   printableStorageName
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
//            FileInfo localFileInfo;

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
    printError("cannot read 'link' content of archive '%s' (error: %s)!",
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
    SEMAPHORE_LOCKED_DO(&restoreInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      String_set(restoreInfo->statusInfo.entry.name,destinationFileName);
      restoreInfo->statusInfo.entry.doneSize  = 0LL;
      restoreInfo->statusInfo.entry.totalSize = 0LL;
      updateStatusInfo(restoreInfo,TRUE);
    }

    // restore link
    printInfo(1,"  Restore link      '%s'...",String_cString(destinationFileName));

    // check if link areadly exists
    SEMAPHORE_LOCKED_DO(&restoreInfo->namesDictionaryLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      if (!Dictionary_contains(&restoreInfo->namesDictionary,
                               String_cString(destinationFileName),
                               String_length(destinationFileName)
                              )
         )
      {
        if (File_exists(destinationFileName))
        {
          switch (restoreInfo->jobOptions->restoreEntryMode)
          {
            case RESTORE_ENTRY_MODE_STOP:
              // stop
              printInfo(1,
                        "stopped (link exists)\n",
                        String_cString(destinationFileName)
                       );
              error = !restoreInfo->jobOptions->noStopOnErrorFlag
                        ? ERRORX_(FILE_EXISTS_,0,"%s",String_cString(destinationFileName))
                        : ERROR_NONE;
              Semaphore_unlock(&restoreInfo->namesDictionaryLock);
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
              String_append(destinationFileName,postfixFileName);
              if (File_exists(destinationFileName))
              {
                n = 0;
                do
                {
                  String_set(destinationFileName,prefixFileName);
                  String_appendFormat(destinationFileName,"-%u",n);
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
            case RESTORE_ENTRY_MODE_SKIP_EXISTING:
              // skip
              printInfo(1,"skipped (link exists)\n");
              Semaphore_unlock(&restoreInfo->namesDictionaryLock);
              AutoFree_cleanup(&autoFreeList);
              return ERROR_NONE;
          }
        }

        Dictionary_add(&restoreInfo->namesDictionary,
                       String_cString(destinationFileName),
                       String_length(destinationFileName),
                       NULL,
                       0
                      );
      }
    }

    // create parent directories if not existing
    if (!restoreInfo->jobOptions->dryRun)
    {
      error = createParentDirectories(restoreInfo,destinationFileName,
                                      (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : fileInfo.userId,
                                      (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : fileInfo.groupId
                                     );
      if (error != ERROR_NONE)
      {
        if (!restoreInfo->jobOptions->noStopOnErrorFlag)
        {
          printInfo(1,"FAIL!\n");
          printError("cannot set owner/group of parent directory for '%s' (error: %s)",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        else
        {
          printWarning("cannot set owner/group of parent directory for '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
        }
      }
    }

    // create link
    if (!restoreInfo->jobOptions->dryRun)
    {
      error = File_makeLink(destinationFileName,fileName);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("cannot create link '%s' -> '%s' (error: %s)",
                   String_cString(destinationFileName),
                   String_cString(fileName),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }

    if (!restoreInfo->jobOptions->dryRun)
    {
      // set file time, file permissions
      if (globalOptions.permissions != FILE_DEFAULT_PERMISSIONS)
      {
        fileInfo.permissions = globalOptions.permissions;
      }
      error = File_setInfo(&fileInfo,destinationFileName);
      if (error != ERROR_NONE)
      {
        if (   !restoreInfo->jobOptions->noStopOnErrorFlag
            && !File_isNetworkFileSystem(destinationFileName)
           )
        {
          printInfo(1,"FAIL!\n");
          printError("cannot set file info of '%s' (error: %s)",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        else
        {
          printWarning("cannot set file info of '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
        }
      }

      // set link owner/group
      error = File_setOwner(destinationFileName,
                            (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : fileInfo.userId,
                            (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : fileInfo.groupId
                           );
      if (error != ERROR_NONE)
      {
        if (   !restoreInfo->jobOptions->noStopOnOwnerErrorFlag
            && !File_isNetworkFileSystem(destinationFileName)
           )
        {
          printInfo(1,"FAIL!\n");
          printError("cannot set owner/group of link '%s' (error: %s)",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        else
        {
          printWarning("cannot set owner/group of link '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
        }
      }
    }

    // output result
    if (!restoreInfo->jobOptions->dryRun)
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
      printWarning("unexpected data at end of link entry '%S'",linkName);
    }

    // free resources
    String_delete(destinationFileName);
  }
  else
  {
    // skip
    printInfo(2,"  Restore link      '%s'...skipped\n",String_cString(linkName));
  }

  // close archive file
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'link' entry fail (error: %s)",Error_getText(error));
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
  const StringNode          *stringNode;
  String                    fileName;
  FileModes                 fileMode;
  FileHandle                fileHandle;
  uint64                    length;
  ulong                     bufferLength;
  bool                      isComplete;
  char                      sizeString[32];
  char                      fragmentString[256];

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
    printError("cannot read 'hard link' content of archive '%s' (error: %s)!",
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
      SEMAPHORE_LOCKED_DO(&restoreInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        String_set(restoreInfo->statusInfo.entry.name,destinationFileName);
        restoreInfo->statusInfo.entry.doneSize  = 0LL;
        restoreInfo->statusInfo.entry.totalSize = fragmentSize;
        updateStatusInfo(restoreInfo,TRUE);
      }

      // restore hardlink
      printInfo(1,"  Restore hard link '%s'...",String_cString(destinationFileName));

      // check if hardlink already exists
      SEMAPHORE_LOCKED_DO(&restoreInfo->namesDictionaryLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        if (!Dictionary_contains(&restoreInfo->namesDictionary,
                                 String_cString(destinationFileName),
                                 String_length(destinationFileName)
                                )
           )
        {
          if (File_exists(destinationFileName))
          {
            switch (restoreInfo->jobOptions->restoreEntryMode)
            {
              case RESTORE_ENTRY_MODE_STOP:
                // stop
                printInfo(1,"stopped (hardlink exists)\n");
                error = !restoreInfo->jobOptions->noStopOnErrorFlag
                          ? ERRORX_(FILE_EXISTS_,0,"%s",String_cString(destinationFileName))
                          : ERROR_NONE;
                Semaphore_unlock(&restoreInfo->namesDictionaryLock);
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
                String_append(destinationFileName,postfixFileName);
                if (File_exists(destinationFileName))
                {
                  n = 0;
                  do
                  {
                    String_set(destinationFileName,prefixFileName);
                    String_appendFormat(destinationFileName,"-%u",n);
                    String_append(destinationFileName,postfixFileName);
                    n++;
                  }
                  while (File_exists(destinationFileName));
                }
                String_delete(postfixFileName);
                String_delete(prefixFileName);
                break;
              case RESTORE_ENTRY_MODE_OVERWRITE:
                // truncate to 0-file
                error = File_open(&fileHandle,destinationFileName,FILE_OPEN_CREATE);
                if (error != ERROR_NONE)
                {
                  Semaphore_unlock(&restoreInfo->namesDictionaryLock);
                  AutoFree_cleanup(&autoFreeList);
                  return error;
                }
                (void)File_close(&fileHandle);
                break;
              case RESTORE_ENTRY_MODE_SKIP_EXISTING:
                // skip
                printInfo(1,"skipped (hardlink exists)\n");
                Semaphore_unlock(&restoreInfo->namesDictionaryLock);
                AutoFree_cleanup(&autoFreeList);
                return ERROR_NONE;
            }
          }

          Dictionary_add(&restoreInfo->namesDictionary,
                         String_cString(destinationFileName),
                         String_length(destinationFileName),
                         NULL,
                         0
                        );
        }
      }

      // check if hardlink fragment already eixsts
      if (   !restoreInfo->jobOptions->noFragmentsCheckFlag
          && !restoredDataFlag
         )
      {
        // check if fragment already exist -> get/create file fragment node
        SEMAPHORE_LOCKED_DO(&restoreInfo->fragmentListLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          FragmentNode *fragmentNode;

          fragmentNode = FragmentList_find(&restoreInfo->fragmentList,fileName);
          if (fragmentNode != NULL)
          {
            if (FragmentList_rangeExists(fragmentNode,fragmentOffset,fragmentSize))
            {
              switch (restoreInfo->jobOptions->restoreEntryMode)
              {
                case RESTORE_ENTRY_MODE_STOP:
                  // stop
                  printInfo(1,"skipped (hardlink part %llu..%llu exists)\n",
                            fragmentOffset,
                            (fragmentSize > 0LL) ? fragmentOffset+fragmentSize-1:fragmentOffset
                           );
                  Semaphore_unlock(&restoreInfo->fragmentListLock);
                  AutoFree_cleanup(&autoFreeList);
                  return !restoreInfo->jobOptions->noStopOnErrorFlag ? ERROR_FILE_EXISTS_ : ERROR_NONE;
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
                  String_append(destinationFileName,postfixFileName);
                  if (File_exists(destinationFileName))
                  {
                    n = 0;
                    do
                    {
                      String_set(destinationFileName,prefixFileName);
                      String_appendFormat(destinationFileName,"-%u",n);
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
                case RESTORE_ENTRY_MODE_SKIP_EXISTING:
                  // skip
                  printInfo(1,"skipped (hardlink part %llu..%llu exists)\n",
                            fragmentOffset,
                            (fragmentSize > 0LL) ? fragmentOffset+fragmentSize-1:fragmentOffset
                           );
                  Semaphore_unlock(&restoreInfo->fragmentListLock);
                  AutoFree_cleanup(&autoFreeList);
                  return ERROR_NONE;
              }
            }
          }
          else
          {
            fragmentNode = FragmentList_add(&restoreInfo->fragmentList,
                                            fileName,
                                            fileInfo.size,
                                            &fileInfo,sizeof(FileInfo),
                                            0
                                           );
          }
          assert(fragmentNode != NULL);
        }
      }

      // create parent directories if not existing
      if (!restoreInfo->jobOptions->dryRun)
      {
        error = createParentDirectories(restoreInfo,destinationFileName,
                                        (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : fileInfo.userId,
                                        (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : fileInfo.groupId
                                       );
        if (error != ERROR_NONE)
        {
          if (!restoreInfo->jobOptions->noStopOnErrorFlag)
          {
            printInfo(1,"FAIL!\n");
            printError("cannot set owner/group of parent directory for '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
            AutoFree_cleanup(&autoFreeList);
            return error;
          }
          else
          {
            printWarning("cannot set owner/group of parent directory for '%s' (error: %s)",
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
          }
        }
      }

      // create hardlink
      if (!restoredDataFlag)
      {
        // create file
        if (!restoreInfo->jobOptions->dryRun)
        {
          // temporary change owner+permissions for writing (ignore errors)
          (void)File_setPermission(destinationFileName,FILE_PERMISSION_USER_READ|FILE_PERMISSION_USER_WRITE);
          (void)File_setOwner(destinationFileName,FILE_OWN_USER_ID,FILE_OWN_GROUP_ID);

          // open file
          fileMode = FILE_OPEN_WRITE;
          if (restoreInfo->jobOptions->sparseFilesFlag) fileMode |= FILE_SPARSE;
          error = File_open(&fileHandle,destinationFileName,fileMode);
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL!\n");
            printError("cannot create/write to hard link '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
            AutoFree_cleanup(&autoFreeList);
            return error;
          }
          AUTOFREE_ADD(&autoFreeList,&fileHandle,{ (void)File_close(&fileHandle); });

          // set file length for sparse files
          if (restoreInfo->jobOptions->sparseFilesFlag)
          {
            error = File_truncate(&fileHandle,fileInfo.size);
            if (error != ERROR_NONE)
            {
              printInfo(1,"FAIL!\n");
              printError("cannot create/write to hard link '%s' (error: %s)",
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
              AutoFree_cleanup(&autoFreeList);
              return error;
            }
          }

          // seek to fragment position
          error = File_seek(&fileHandle,fragmentOffset);
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL!\n");
            printError("cannot write content of hard link '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
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
            printError("cannot read content of hard link '%s' (error: %s)!",
                       String_cString(printableStorageName),
                       Error_getText(error)
                      );
            break;
          }
          if (!restoreInfo->jobOptions->dryRun)
          {
            error = File_write(&fileHandle,buffer,bufferLength);
            if (error != ERROR_NONE)
            {
              printInfo(1,"FAIL!\n");
              printError("cannot write content of hard link '%s' (error: %s)",
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
              break;
            }
          }

          // update status info
          SEMAPHORE_LOCKED_DO(&restoreInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
          {
            restoreInfo->statusInfo.entry.doneSize += (uint64)bufferLength;
            updateStatusInfo(restoreInfo,FALSE);
          }

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
        if (!restoreInfo->jobOptions->dryRun)
        {
          if (File_getSize(&fileHandle) > fileInfo.size)
          {
            File_truncate(&fileHandle,fileInfo.size);
          }
        }

        // close file
        if (!restoreInfo->jobOptions->dryRun)
        {
          AUTOFREE_REMOVE(&autoFreeList,&fileHandle);
          (void)File_close(&fileHandle);
        }

        // add fragment to hardlink fragment list
        isComplete = FALSE;
        SEMAPHORE_LOCKED_DO(&restoreInfo->fragmentListLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          FragmentNode *fragmentNode;

          fragmentNode = FragmentList_find(&restoreInfo->fragmentList,fileName);
          if (fragmentNode != NULL)
          {
            FragmentList_addRange(fragmentNode,fragmentOffset,fragmentSize);
//FragmentList_debugPrintInfo(fragmentNode,String_cString(fileName));

            if (FragmentList_isComplete(fragmentNode))
            {
              FragmentList_discard(&restoreInfo->fragmentList,fragmentNode);
              isComplete = TRUE;
            }
          }
          else
          {
            isComplete = TRUE;
          }
        }

        if (!restoreInfo->jobOptions->dryRun)
        {
          if (isComplete)
          {
            // set hardlink time, file permissions
            if (globalOptions.permissions != FILE_DEFAULT_PERMISSIONS)
            {
              fileInfo.permissions = globalOptions.permissions;
            }
            error = File_setInfo(&fileInfo,destinationFileName);
            if (error != ERROR_NONE)
            {
              if (   !restoreInfo->jobOptions->noStopOnErrorFlag
                  && !File_isNetworkFileSystem(destinationFileName)
                 )
              {
                printInfo(1,"FAIL!\n");
                printError("cannot set hard link info of '%s' (error: %s)",
                           String_cString(destinationFileName),
                           Error_getText(error)
                          );
                AutoFree_cleanup(&autoFreeList);
                return error;
              }
              else
              {
                printWarning("cannot set hard link info of '%s' (error: %s)",
                             String_cString(destinationFileName),
                             Error_getText(error)
                            );
              }
            }

            // set hardlink owner/group
            error = File_setOwner(destinationFileName,
                                  (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : fileInfo.userId,
                                  (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : fileInfo.groupId
                                 );
            if (error != ERROR_NONE)
            {
              if (   !restoreInfo->jobOptions->noStopOnOwnerErrorFlag
                  && !File_isNetworkFileSystem(destinationFileName)
                 )
              {
                printInfo(1,"FAIL!\n");
                printError("cannot set owner/group of hard link '%s' (error: %s)",
                           String_cString(destinationFileName),
                           Error_getText(error)
                          );
                AutoFree_cleanup(&autoFreeList);
                return error;
              }
              else
              {
                printWarning("cannot set owner/group of hard link '%s' (error: %s)",
                             String_cString(destinationFileName),
                             Error_getText(error)
                            );
              }
            }

            // set attributes
            error = File_setAttributes(fileInfo.attributes,destinationFileName);
            if (error != ERROR_NONE)
            {
              if (   !restoreInfo->jobOptions->noStopOnAttributeErrorFlag
                  && !File_isNetworkFileSystem(destinationFileName)
                 )
              {
                printInfo(1,"FAIL!\n");
                printError("cannot set hard link attributes of '%s' (error: %s)",
                           String_cString(destinationFileName),
                           Error_getText(error)
                          );
                AutoFree_cleanup(&autoFreeList);
                return error;
              }
              else
              {
                printWarning("cannot set hard link attributes of '%s' (error: %s)",
                             String_cString(destinationFileName),
                             Error_getText(error)
                            );
              }
            }
          }
        }

        // get size/fragment info
        if (globalOptions.humanFormatFlag)
        {
          getHumanSizeString(sizeString,sizeof(sizeString),fileInfo.size);
        }
        else
        {
          stringFormat(sizeString,sizeof(sizeString),"%*"PRIu64,stringInt64Length(globalOptions.fragmentSize),fileInfo.size);
        }
        stringClear(fragmentString);
        if (fragmentSize < fileInfo.size)
        {
          stringFormat(fragmentString,sizeof(fragmentString),
                       ", fragment %*"PRIu64"..%*"PRIu64,
                       stringInt64Length(fileInfo.size),fragmentOffset,
                       stringInt64Length(fileInfo.size),fragmentOffset+fragmentSize-1LL
                      );
        }

        // output result
        if (!restoreInfo->jobOptions->dryRun)
        {
          printInfo(1,"OK (%s bytes%s)\n",sizeString,fragmentString);
        }
        else
        {
          printInfo(1,"OK (%s bytes%s, dry-run)\n",sizeString,fragmentString);
        }

        restoredDataFlag = TRUE;
      }
      else
      {
        // create hard link
        if (!restoreInfo->jobOptions->dryRun)
        {
          error = File_makeHardLink(destinationFileName,hardLinkFileName);
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL!\n");
            printError("cannot create hard link '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
            AutoFree_cleanup(&autoFreeList);
            return error;
          }
        }

        // output result
        if (!restoreInfo->jobOptions->dryRun)
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
          printWarning("unexpected data at end of hard link entry '%S'",fileName);
        }
      }
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
    printWarning("close 'hard link' entry fail (error: %s)",Error_getText(error));
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
                                 ConstString   printableStorageName
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
//            FileInfo localFileInfo;

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
    printError("cannot read 'special' content of archive '%s' (error: %s)!",
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
    SEMAPHORE_LOCKED_DO(&restoreInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      String_set(restoreInfo->statusInfo.entry.name,destinationFileName);
      restoreInfo->statusInfo.entry.doneSize  = 0LL;
      restoreInfo->statusInfo.entry.totalSize = 0LL;
      updateStatusInfo(restoreInfo,TRUE);
    }

    // restore special entry
    printInfo(1,"  Restore special   '%s'...",String_cString(destinationFileName));

    // check if special entry already exists
    SEMAPHORE_LOCKED_DO(&restoreInfo->namesDictionaryLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      if (File_exists(destinationFileName))
      {
        switch (restoreInfo->jobOptions->restoreEntryMode)
        {
          case RESTORE_ENTRY_MODE_STOP:
            // stop
            printInfo(1,"stopped (special exists)\n");
            error = !restoreInfo->jobOptions->noStopOnErrorFlag
                      ? ERRORX_(FILE_EXISTS_,0,"%s",String_cString(destinationFileName))
                      : ERROR_NONE;
            Semaphore_unlock(&restoreInfo->namesDictionaryLock);
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
            String_append(destinationFileName,postfixFileName);
            if (File_exists(destinationFileName))
            {
              n = 0;
              do
              {
                String_set(destinationFileName,prefixFileName);
                String_appendFormat(destinationFileName,"-%u",n);
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
          case RESTORE_ENTRY_MODE_SKIP_EXISTING:
            // skip
            printInfo(1,"skipped (special exists)\n");
            Semaphore_unlock(&restoreInfo->namesDictionaryLock);
            AutoFree_cleanup(&autoFreeList);
            return ERROR_NONE;
        }

        Dictionary_add(&restoreInfo->namesDictionary,
                       String_cString(destinationFileName),
                       String_length(destinationFileName),
                       NULL,
                       0
                      );
      }
    }

    // create parent directories if not existing
    if (!restoreInfo->jobOptions->dryRun)
    {
      error = createParentDirectories(restoreInfo,destinationFileName,
                                      (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : fileInfo.userId,
                                      (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : fileInfo.groupId
                                     );
      if (error != ERROR_NONE)
      {
        if (!restoreInfo->jobOptions->noStopOnErrorFlag)
        {
          printInfo(1,"FAIL!\n");
          printError("cannot set owner/group of parent directory for '%s' (error: %s)",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        else
        {
          printWarning("cannot set owner/group of parent directory for '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
        }
      }
    }

    // create special file
    if (!restoreInfo->jobOptions->dryRun)
    {
      error = File_makeSpecial(destinationFileName,
                               fileInfo.specialType,
                               fileInfo.major,
                               fileInfo.minor
                              );
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("cannot create special device '%s' (error: %s)",
                   String_cString(fileName),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }

    if (!restoreInfo->jobOptions->dryRun)
    {
      // set file time, file permissions
      if (globalOptions.permissions != FILE_DEFAULT_PERMISSIONS)
      {
        fileInfo.permissions = globalOptions.permissions;
      }
      error = File_setInfo(&fileInfo,destinationFileName);
      if (error != ERROR_NONE)
      {
        if (   !restoreInfo->jobOptions->noStopOnErrorFlag
            && !File_isNetworkFileSystem(destinationFileName)
           )
        {
          printInfo(1,"FAIL!\n");
          printError("cannot set file info of '%s' (error: %s)",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        else
        {
          printWarning("cannot set file info of '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
        }
      }

      // set special entry owner/group
      error = File_setOwner(destinationFileName,
                            (restoreInfo->jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) ? restoreInfo->jobOptions->owner.userId  : fileInfo.userId,
                            (restoreInfo->jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) ? restoreInfo->jobOptions->owner.groupId : fileInfo.groupId
                           );
      if (error != ERROR_NONE)
      {
        if (   !restoreInfo->jobOptions->noStopOnOwnerErrorFlag
            && !File_isNetworkFileSystem(destinationFileName)
           )
        {
          printInfo(1,"FAIL!\n");
          printError("cannot set owner/group of file '%s' (error: %s)",
                     String_cString(destinationFileName),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        else
        {
          printWarning("cannot set owner/group of file '%s' (error: %s)",
                       String_cString(destinationFileName),
                       Error_getText(error)
                      );
        }
      }
    }

    // output result
    if (!restoreInfo->jobOptions->dryRun)
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
      printWarning("unexpected data at end of special entry '%S'",fileName);
    }

    // free resources
    String_delete(destinationFileName);
  }
  else
  {
    // skip
    printInfo(2,"  Restore special   '%s'...skipped\n",String_cString(fileName));
  }

  // close archive file
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'special' entry fail (error: %s)",Error_getText(error));
  }

  // free resources
  File_doneExtendedAttributes(&fileExtendedAttributeList);
  String_delete(fileName);
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : restoreThreadCode
* Purpose: restore worker thread
* Input  : restoreInfo - restore info structure
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void restoreThreadCode(RestoreInfo *restoreInfo)
{
  byte          *buffer;
  uint          archiveIndex;
  ArchiveHandle archiveHandle;
  EntryMsg      entryMsg;
  Errors        error;

  assert(restoreInfo != NULL);
  assert(restoreInfo->jobOptions != NULL);

  // init variables
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  archiveIndex = 0;

  // restore entries
  while (MsgQueue_get(&restoreInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER))
  {
    if (   ((restoreInfo->failError == ERROR_NONE) || restoreInfo->jobOptions->noStopOnErrorFlag)
// TODO:
//        && !isAborted(restoreInfo)
       )
    {
      // open archive (only if new archive)
      if (archiveIndex != entryMsg.archiveIndex)
      {
        // close previous archive
        if (archiveIndex != 0)
        {
          Archive_close(&archiveHandle,FALSE);
          archiveIndex = 0;
        }

        // open new archive
        error = Archive_openHandle(&archiveHandle,
                                   entryMsg.archiveHandle
                                  );
        if (error != ERROR_NONE)
        {
          printError("cannot open archive '%s' (error: %s)!",
                     String_cString(entryMsg.archiveHandle->printableStorageName),
                     Error_getText(error)
                    );
          if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
          freeEntryMsg(&entryMsg,NULL);
          break;
        }

        // store current archive index
        archiveIndex = entryMsg.archiveIndex;
      }

      // set archive crypt info
      Archive_setCryptInfo(&archiveHandle,entryMsg.archiveCryptInfo);

      // seek to start of entry
      error = Archive_seek(&archiveHandle,entryMsg.offset);
      if (error != ERROR_NONE)
      {
        printError("cannot read storage '%s' (error: %s)!",
                   String_cString(entryMsg.archiveHandle->printableStorageName),
                   Error_getText(error)
                  );
        if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
        freeEntryMsg(&entryMsg,NULL);
        break;
      }

      switch (entryMsg.archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_NONE:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNREACHABLE();
          #endif /* NDEBUG */
          break; /* not reached */
        case ARCHIVE_ENTRY_TYPE_FILE:
          error = restoreFileEntry(restoreInfo,
                                   &archiveHandle,
                                   entryMsg.archiveHandle->printableStorageName,
                                   buffer,
                                   BUFFER_SIZE
                                  );
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          error = restoreImageEntry(restoreInfo,
                                    &archiveHandle,
                                    entryMsg.archiveHandle->printableStorageName,
                                    buffer,
                                    BUFFER_SIZE
                                   );
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          error = restoreDirectoryEntry(restoreInfo,
                                        &archiveHandle,
                                        entryMsg.archiveHandle->printableStorageName
                                       );

          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          error = restoreLinkEntry(restoreInfo,
                                   &archiveHandle,
                                   entryMsg.archiveHandle->printableStorageName
                                  );
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          error = restoreHardLinkEntry(restoreInfo,
                                       &archiveHandle,
                                       entryMsg.archiveHandle->printableStorageName,
                                       buffer,
                                       BUFFER_SIZE
                                      );
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          error = restoreSpecialEntry(restoreInfo,
                                      &archiveHandle,
                                      entryMsg.archiveHandle->printableStorageName
                                     );
          break;
        case ARCHIVE_ENTRY_TYPE_META:
          error = Archive_skipNextEntry(&archiveHandle);
          break;
        case ARCHIVE_ENTRY_TYPE_SIGNATURE:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNREACHABLE();
          #else
            error = Archive_skipNextEntry(&archiveHandle);
          #endif /* NDEBUG */
          break;
        case ARCHIVE_ENTRY_TYPE_UNKNOWN:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNREACHABLE();
          #endif /* NDEBUG */
          break; /* not reached */
      }
      if (error != ERROR_NONE)
      {
        if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
        freeEntryMsg(&entryMsg,NULL);
        break;
      }
    }

    // free resources
    freeEntryMsg(&entryMsg,NULL);
  }

  // close archive
  if (archiveIndex != 0)
  {
    Archive_close(&archiveHandle,FALSE);
  }

  // discard processing all other entries
  while (MsgQueue_get(&restoreInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER))
  {
    freeEntryMsg(&entryMsg,NULL);
  }

  // free resources
  free(buffer);
}

/***********************************************************************\
* Name   : restoreArchive
* Purpose: restore archive content
* Input  : restoreInfo      - restore info
*          storageSpecifier - storage to restore from
*          archiveName      - archive name to restore from or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors restoreArchive(RestoreInfo      *restoreInfo,
                            StorageSpecifier *storageSpecifier,
                            ConstString       archiveName
                           )
{
  AutoFreeList           autoFreeList;
  String                 printableStorageName;
  byte                   *buffer;
  Errors                 error;
  StorageInfo            storageInfo;
  uint                   restoreThreadCount;
  uint                   i;
  ArchiveHandle          archiveHandle;
  CryptSignatureStates   allCryptSignatureState;
  uint64                 lastSignatureOffset;
  ArchiveEntryTypes      archiveEntryType;
  const ArchiveCryptInfo *archiveCryptInfo;
  uint64                 offset;
  EntryMsg               entryMsg;

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
  SEMAPHORE_LOCKED_DO(&restoreInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    String_set(restoreInfo->statusInfo.storage.name,printableStorageName);
    restoreInfo->statusInfo.storage.doneSize  = 0LL;
    restoreInfo->statusInfo.storage.totalSize = 0LL;
    updateStatusInfo(restoreInfo,TRUE);
  }

  // init storage
  error = Storage_init(&storageInfo,
NULL, // masterIO
                       storageSpecifier,
                       restoreInfo->jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK_(NULL,NULL),  // updateStatusInfo
                       CALLBACK_(restoreInfo->getNamePasswordFunction,restoreInfo->getNamePasswordUserData),
                       CALLBACK_(NULL,NULL), // requestVolume
//TODO
                       CALLBACK_(NULL,NULL),  // isPause
                       CALLBACK_(NULL,NULL),  // isAborted
                       restoreInfo->logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError("cannot initialize storage '%s' (error: %s)!",
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
    printError("archive not found '%s'!",
               String_cString(printableStorageName)
              );
    error = handleError(restoreInfo,ERRORX_(ARCHIVE_NOT_FOUND,0,"%s",String_cString(printableStorageName)));
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // open archive
  error = Archive_open(&archiveHandle,
                       &storageInfo,
                       archiveName,
                       &restoreInfo->jobOptions->deltaSourceList,
                       CALLBACK_(restoreInfo->getNamePasswordFunction,restoreInfo->getNamePasswordUserData),
                       restoreInfo->logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError("cannot open storage '%s' (error: %s)!",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    error = handleError(restoreInfo,error);
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveHandle,{ Archive_close(&archiveHandle,FALSE); });

  // check signatures
  if (!restoreInfo->jobOptions->skipVerifySignaturesFlag)
  {
    error = Archive_verifySignatures(&archiveHandle,
                                     &allCryptSignatureState
                                    );
    if (error != ERROR_NONE)
    {
      if (!restoreInfo->jobOptions->forceVerifySignaturesFlag && (Error_getCode(error) == ERROR_CODE_NO_PUBLIC_SIGNATURE_KEY))
      {
        allCryptSignatureState = CRYPT_SIGNATURE_STATE_SKIPPED;
      }
      else
      {
        // signature error
        error = handleError(restoreInfo,error);
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }
    if (!Crypt_isValidSignatureState(allCryptSignatureState))
    {
      if (restoreInfo->jobOptions->forceVerifySignaturesFlag)
      {
        // signature error
        printError("invalid signature in '%s'!",
                   String_cString(printableStorageName)
                  );
        (void)handleError(restoreInfo,error);
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_SIGNATURE;
      }
      else
      {
        // print signature warning
        printWarning("invalid signature in '%s'!",
                     String_cString(printableStorageName)
                    );
      }
    }
  }

  // update status info
  SEMAPHORE_LOCKED_DO(&restoreInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    restoreInfo->statusInfo.storage.totalSize = Archive_getSize(&archiveHandle);
    updateStatusInfo(restoreInfo,TRUE);
  }

  // output info
  printInfo(0,
            "Restore storage '%s'%s",
            String_cString(printableStorageName),
            !isPrintInfo(1) ? "..." : ":\n"
           );

  // start restore threads
  MsgQueue_reset(&restoreInfo->entryMsgQueue);
  restoreThreadCount = (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores();
  for (i = 0; i < restoreThreadCount; i++)
  {
    ThreadPool_run(&workerThreadPool,restoreThreadCode,restoreInfo);
  }

  // read archive entries
  allCryptSignatureState = CRYPT_SIGNATURE_STATE_NONE;
  error                  = ERROR_NONE;
  lastSignatureOffset    = Archive_tell(&archiveHandle);
  while (   ((restoreInfo->failError == ERROR_NONE) || restoreInfo->jobOptions->noStopOnErrorFlag)
         && ((restoreInfo->isAbortedFunction == NULL) || !restoreInfo->isAbortedFunction(restoreInfo->isAbortedUserData))
         && (restoreInfo->jobOptions->skipVerifySignaturesFlag || Crypt_isValidSignatureState(allCryptSignatureState))
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
                                        &archiveCryptInfo,
                                        &offset,
                                        isPrintInfo(3) ? ARCHIVE_FLAG_PRINT_UNKNOWN_CHUNKS : ARCHIVE_FLAG_NONE
                                       );
    if (error != ERROR_NONE)
    {
      printError("cannot read next entry in archive '%s' (error: %s)!",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = handleError(restoreInfo,error);
      break;
    }

    // update storage status
    SEMAPHORE_LOCKED_DO(&restoreInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      restoreInfo->statusInfo.storage.doneSize = Archive_tell(&archiveHandle);
      updateStatusInfo(restoreInfo,TRUE);
    }

    // restore entries
    if (archiveEntryType != ARCHIVE_ENTRY_TYPE_SIGNATURE)
    {
      // send entry to restore threads
//TODO: increment on multiple archives and when threads are not restarted for each archive (multi-threaded restore over multiple archives)
      entryMsg.archiveIndex     = 1;
      entryMsg.archiveHandle    = &archiveHandle;
      entryMsg.archiveEntryType = archiveEntryType;
      entryMsg.archiveCryptInfo = archiveCryptInfo;
      entryMsg.offset           = offset;
      if (!MsgQueue_put(&restoreInfo->entryMsgQueue,&entryMsg,sizeof(entryMsg)))
      {
        HALT_INTERNAL_ERROR("Send message to restore threads fail!");
      }

      // skip entry
      error = Archive_skipNextEntry(&archiveHandle);
      if (error != ERROR_NONE)
      {
        if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
        break;
      }
    }
    else
    {
      if (!restoreInfo->jobOptions->skipVerifySignaturesFlag)
      {
        // check signature
        error = Archive_verifySignatureEntry(&archiveHandle,lastSignatureOffset,&allCryptSignatureState);
      }
      else
      {
         // skip signature
         error = Archive_skipNextEntry(&archiveHandle);
      }
      if (error != ERROR_NONE)
      {
        if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = error;
        break;
      }
      lastSignatureOffset = Archive_tell(&archiveHandle);
    }

    // update storage status
    SEMAPHORE_LOCKED_DO(&restoreInfo->statusInfoLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      restoreInfo->statusInfo.storage.doneSize = Archive_tell(&archiveHandle);
      updateStatusInfo(restoreInfo,TRUE);
    }
  }

  // wait for restore threads
  MsgQueue_setEndOfMsg(&restoreInfo->entryMsgQueue);
  ThreadPool_joinAll(&workerThreadPool);

  // close archive
  Archive_close(&archiveHandle,FALSE);

  // done storage
  (void)Storage_done(&storageInfo);

  // output info
  if (!isPrintInfo(1)) printInfo(0,
                                 "%s",
                                    (restoreInfo->failError == ERROR_NONE)
                                 && (   restoreInfo->jobOptions->skipVerifySignaturesFlag
                                     || Crypt_isValidSignatureState(allCryptSignatureState)
                                    )
                                   ? "OK\n"
                                   : "FAIL!\n"
                                );

  // output signature error/warning
  if (!Crypt_isValidSignatureState(allCryptSignatureState))
  {
    if (restoreInfo->jobOptions->forceVerifySignaturesFlag)
    {
      printError("invalid signature in '%s'!",
                 String_cString(printableStorageName)
                );
      if (restoreInfo->failError == ERROR_NONE) restoreInfo->failError = ERROR_INVALID_SIGNATURE;
    }
    else
    {
      printWarning("invalid signature in '%s'!",
                   String_cString(printableStorageName)
                  );
    }
  }

  // free resources
  free(buffer);
  String_delete(printableStorageName);
  AutoFree_done(&autoFreeList);

  return restoreInfo->failError;
}

/*---------------------------------------------------------------------*/

Errors Command_restore(const StringList                *storageNameList,
                       const EntryList                 *includeEntryList,
                       const PatternList               *excludePatternList,
                       JobOptions                      *jobOptions,
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
  StorageSpecifier           storageSpecifier;
  RestoreInfo                restoreInfo;
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
                  jobOptions,
                  CALLBACK_(updateStatusInfoFunction,updateStatusInfoUserData),
                  CALLBACK_(handleErrorFunction,handleErrorUserData),
                  CALLBACK_(getNamePasswordFunction,getNamePasswordUserData),
                  CALLBACK_(isPauseFunction,isPauseUserData),
                  CALLBACK_(isAbortedFunction,isAbortedUserData),
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
      printError("invalid storage '%s' (error: %s)",
                 String_cString(storageName),
                 Error_getText(error)
                );
      if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
      continue;
    }

    if (String_isEmpty(storageSpecifier.archivePatternString))
    {
      // restore archive content
      error = restoreArchive(&restoreInfo,
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
            if (!Pattern_match(&storageSpecifier.archivePattern,fileName,STRING_BEGIN,PATTERN_MATCH_MODE_EXACT,NULL,NULL))
            {
              continue;
            }
          }

          // restore archive content
          error = restoreArchive(&restoreInfo,
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
    printError("no matching storage files found!");
    restoreInfo.failError = ERROR_FILE_NOT_FOUND_;
  }

  if (   (restoreInfo.failError == ERROR_NONE)
      && !jobOptions->noFragmentsCheckFlag
     )
  {
    // check fragment lists, set file info/owner/attributes also for incomplete entries
    if ((isAbortedFunction == NULL) || !isAbortedFunction(isAbortedUserData))
    {
      FRAGMENTLIST_ITERATE(&restoreInfo.fragmentList,fragmentNode)
      {
        if (!FragmentList_isComplete(fragmentNode))
        {
          printInfo(0,"Warning: incomplete entry '%s'\n",String_cString(fragmentNode->name));
          if (isPrintInfo(1))
          {
            printInfo(1,"  Fragments:\n");
            FragmentList_print(stdout,4,fragmentNode,TRUE);
          }
          error = ERRORX_(ENTRY_INCOMPLETE,0,"%s",String_cString(fragmentNode->name));
          if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = handleError(&restoreInfo,error);
        }

        if (fragmentNode->userData != NULL)
        {
          if (!jobOptions->dryRun)
          {
            // set file time, file permission of incomplete entries
            if (globalOptions.permissions != FILE_DEFAULT_PERMISSIONS)
            {
              ((FileInfo*)fragmentNode->userData)->permissions = globalOptions.permissions;
            }
            error = File_setInfo((FileInfo*)fragmentNode->userData,fragmentNode->name);
            if (error != ERROR_NONE)
            {
              if (   !jobOptions->noStopOnErrorFlag
                  && !File_isNetworkFileSystem(fragmentNode->name)
                 )
              {
                printError("cannot set file info of '%s' (error: %s)",
                           String_cString(fragmentNode->name),
                           Error_getText(error)
                          );
                if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
              }
              else
              {
                printWarning("cannot set file info of '%s' (error: %s)",
                             String_cString(fragmentNode->name),
                             Error_getText(error)
                            );
              }
            }

            // set file owner/group of incomplete entries
            error = File_setOwner(fragmentNode->name,
                                  ((FileInfo*)fragmentNode->userData)->userId,
                                  ((FileInfo*)fragmentNode->userData)->groupId
                                 );
            if (error != ERROR_NONE)
            {
              if (   !jobOptions->noStopOnOwnerErrorFlag
                  && !File_isNetworkFileSystem(fragmentNode->name)
                 )
              {
                printInfo(1,"FAIL!\n");
                printError("cannot set owner/group of '%s' (error: %s)",
                           String_cString(fragmentNode->name),
                           Error_getText(error)
                          );
                return error;
              }
              else
              {
                printWarning("cannot set owner/group of '%s' (error: %s)",
                             String_cString(fragmentNode->name),
                             Error_getText(error)
                            );
              }
            }

            // set attributes of incomplete entries
            error = File_setAttributes(((FileInfo*)fragmentNode->userData)->attributes,fragmentNode->name);
            if (error != ERROR_NONE)
            {
              if (   !jobOptions->noStopOnAttributeErrorFlag
                  && !File_isNetworkFileSystem(fragmentNode->name)
                 )
              {
                printError("cannot set file attributes of '%s' (error: %s)",
                           String_cString(fragmentNode->name),
                           Error_getText(error)
                          );
                if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
              }
              else
              {
                printWarning("cannot set file attributes of '%s' (error: %s)",
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

  // done restore info
  doneRestoreInfo(&restoreInfo);

  // free resources
  Storage_doneSpecifier(&storageSpecifier);

  // output info
  if (error != ERROR_NONE)
  {
    printInfo(1,tr("Restore fail: %s\n"),Error_getText(error));
  }

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
