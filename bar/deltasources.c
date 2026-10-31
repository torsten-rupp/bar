/***********************************************************************\
*
* Contents: Backup ARchiver source functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"
#include "common/strings.h"
#include "common/patternlists.h"
#include "common/files.h"
#include "common/fragmentlists.h"

// TODO: remove bar.h
#include "bar.h"
#include "bar_common.h"
#include "errors.h"
#include "storage.h"
#include "archives.h"

#include "deltasources.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// file data buffer size
#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/

typedef struct
{
  String    filePath;     // file path to open delta source file
  String    tmpFilePath;  // file path of temporary delta source file
} DeltaSourceInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : createLocalStorageArchive
* Purpose: create local copy of storage file
* Input  : localStorageSpecifier - local storage specifier
*          storageSpecifier      - storage specifier
*          jobOptions            - job options
* Output : -
* Return : ERROR_NONE if local copy created, otherwise error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createLocalStorageArchive(StorageSpecifier       *localStorageSpecifier,
                                       const StorageSpecifier *storageSpecifier,
                                       const JobOptions       *jobOptions
                                      )
{
  Errors error;

  assert(localStorageSpecifier != NULL);
  assert(storageSpecifier != NULL);

  // init variables
  localStorageSpecifier->type = STORAGE_TYPE_FILESYSTEM;

  // create temporary file
  error = File_getTmpFileName(localStorageSpecifier->archiveName,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // copy storage to local file
  error = Storage_copyToLocal(storageSpecifier,
                              localStorageSpecifier->archiveName,
                              jobOptions,
                              &globalOptions.maxBandWidthList,
                              CALLBACK_(NULL,NULL),  // StorageUpdateProgressFunction
                              CALLBACK_(NULL,NULL)  // StorageVolumeRequestFunction
                             );
  if (error != ERROR_NONE)
  {
    File_delete(localStorageSpecifier->archiveName,FALSE);
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : doneLocalStorageArchive
* Purpose: done local copy of storage file
* Input  : localStorageSpecifier - local storage specifier
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneLocalStorageArchive(StorageSpecifier *localStorageSpecifier)
{
  assert(localStorageSpecifier != NULL);

  File_delete(localStorageSpecifier->archiveName,FALSE);
}

#if 0
// NYI
/***********************************************************************\
* Name   :
* Purpose:
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deleteLocalStorageArchive(String localStorageName)
{
  File_delete(localStorageName,FALSE);
  String_delete(localStorageName);
}
#endif /* 0 */

/***********************************************************************\
* Name   : freeDeltaSourceInfo
* Purpose: free delta source info
* Input  : data     - delta source info
*          size     - size of delta source info (not used)
*          userData - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeDeltaSourceInfo(void *data, size_t size, void *userData)
{
  DeltaSourceInfo *deltaSourceInfo = (DeltaSourceInfo*)data;
  assert(deltaSourceInfo != NULL);

  UNUSED_VARIABLE(size);
  UNUSED_VARIABLE(userData);

  if (deltaSourceInfo->tmpFilePath != NULL)
  {
    (void)File_delete(deltaSourceInfo->tmpFilePath,FALSE);
    String_delete(deltaSourceInfo->tmpFilePath);
  }
}

/***********************************************************************\
* Name   : restoreFile
* Purpose: restore file from archive
* Input  : archiveName             - archive file name
*          name                    - name of entry to restore
*          deltaSourceList         - delta source list
*          jobOptions              - job options
*          destinationFileName     - destination file name
*          fragmentNode            - fragment node (can be NULL)
*          getNamePasswordFunction - get password call back
*          getNamePasswordUserData - user data for get password call back
*          pauseFlag               - pause flag (can be NULL)
*          requestedAbortFlag      - request abort flag (can be NULL)
*          logHandle               - log handle (can be NULL)
* Output : -
* Return : ERROR_NONE if file restored, otherwise error code
* Notes  : -
\***********************************************************************/

LOCAL Errors restoreFile(StorageSpecifier        *storageSpecifier,
                         ConstString             name,
                         DeltaSourceList         *deltaSourceList,
                         const JobOptions        *jobOptions,
                         ConstString             destinationFileName,
                         FragmentNode            *fragmentNode,
                         GetNamePasswordFunction getNamePasswordFunction,
                         void                    *getNamePasswordUserData,
                         bool                    *pauseFlag,
                         bool                    *requestedAbortFlag,
                         LogHandle               *logHandle
                        )
{
  Errors error;

  assert(storageSpecifier != NULL);
  assert(name != NULL);
  assert(jobOptions != NULL);
  assert(destinationFileName != NULL);

  // initialize variables
  byte *buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // get printable storage name
  String printableStorageName = Storage_getPrintableName(String_new(),storageSpecifier,NULL);

  // init storage
  StorageInfo storageInfo;
  error = Storage_init(&storageInfo,
NULL, // masterIO
                       storageSpecifier,
                       jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK_(NULL,NULL),  // storageUpdateProgress
                       CALLBACK_(NULL,NULL),  // getPassword
                       CALLBACK_(NULL,NULL),  // requestVolume
                       CALLBACK_(NULL,NULL),  // isPause
                       CALLBACK_(NULL,NULL),  // isAborted
                       NULL  // logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError(_("cannot initialize storage '%s' (error: %s)!"),
               String_cString(printableStorageName),
               Error_getText(error)
              );
    String_delete(printableStorageName);
    free(buffer);
    return error;
  }

  // open archive
  ArchiveHandle archiveHandle;
  error = Archive_open(&archiveHandle,
                       &storageInfo,
                       NULL,  // archive name
                       deltaSourceList,
                       ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS|(isPrintInfo(3) ? ARCHIVE_FLAG_PRINT_UNKNOWN_CHUNKS : ARCHIVE_FLAG_NONE),
                       CALLBACK_(getNamePasswordFunction,getNamePasswordUserData),
                       logHandle
                      );
  if (error != ERROR_NONE)
  {
    (void)Storage_done(&storageInfo);
    String_delete(printableStorageName);
    free(buffer);
    return error;
  }

  // read archive entries
  bool   restoredFlag = FALSE;
  Errors failError    = ERROR_NONE;
  while (   !restoredFlag
         && (failError == ERROR_NONE)
         && ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
         && !Archive_eof(&archiveHandle)
        )
  {
    // pause
    while ((pauseFlag != NULL) && (*pauseFlag))
    {
      Misc_udelay(500LL*US_PER_MS);
    }

    // get next archive entry type
    ArchiveEntryTypes archiveEntryType;
    error = Archive_getNextArchiveEntry(&archiveHandle,
                                        &archiveEntryType,
                                        NULL,  // archiveCryptInfo
                                        NULL,  // offset
                                        NULL  // size
                                       );
    if (error != ERROR_NONE)
    {
      if (failError == ERROR_NONE) failError = error;
      break;
    }
    assert(archiveEntryType != ARCHIVE_ENTRY_TYPE_NONE);

    switch (archiveEntryType)
    {
      case ARCHIVE_ENTRY_TYPE_NONE:
        break;
      case ARCHIVE_ENTRY_TYPE_FILE:
        {
          // read file
          String           fileName = String_new();
          ArchiveEntryInfo archiveEntryInfo;
          uint64           fragmentOffset,fragmentSize;
          error = Archive_readFileEntry(&archiveEntryInfo,
                                        &archiveHandle,
                                        NULL,  // deltaCompressAlgorithm
                                        NULL,  // byteCompressAlgorithm
                                        NULL,  // cryptType
                                        NULL,  // cryptAlgorithm
                                        NULL,  // cryptSalt
                                        NULL,  // cryptKey
                                        fileName,
                                        NULL,  // fileInfo
                                        NULL,  // fileExtendedAttributeList
                                        NULL,  // deltaSourceHandleName
                                        NULL,  // deltaSourceHandleSize
                                        &fragmentOffset,
                                        &fragmentSize
                                       );
          if (error != ERROR_NONE)
          {
            String_delete(fileName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          if (String_equals(name,fileName))
          {
//            abortFlag = !updateStatusInfo(&restoreInfo);

            // open file
            FileHandle fileHandle;
            error = File_open(&fileHandle,destinationFileName,FILE_OPEN_WRITE);
            if (error != ERROR_NONE)
            {
              printError(_("cannot create/write to file '%s' (error: %s)"),
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            // seek to fragment position
            error = File_seek(&fileHandle,fragmentOffset);
            if (error != ERROR_NONE)
            {
              printError(_("cannot write file '%s' (error: %s)"),
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
              File_close(&fileHandle);
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            // write file data
            uint64 length = 0;
            while (   ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
                   && (length < fragmentSize)
                  )
            {
              // pause
              while ((pauseFlag != NULL) && (*pauseFlag))
              {
                Misc_udelay(500LL*US_PER_MS);
              }

              ulong bufferLength = MIN(fragmentSize-length,BUFFER_SIZE);

              // read data from archive
              error = Archive_readData(&archiveEntryInfo,buffer,bufferLength);
              if (error != ERROR_NONE)
              {
                if (failError == ERROR_NONE) failError = error;
                break;
              }

              // write data to file
              error = File_write(&fileHandle,buffer,bufferLength);
              if (error != ERROR_NONE)
              {
                printError(_("cannot write file '%s' (error: %s)"),
                           String_cString(destinationFileName),
                           Error_getText(error)
                          );
                if (failError == ERROR_NONE) failError = error;
                break;
              }
//              abortFlag = !updateStatusInfo(&restoreInfo);

              length += bufferLength;
            }
            if      (failError != ERROR_NONE)
            {
              File_close(&fileHandle);
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              break;
            }
            else if ((requestedAbortFlag != NULL) && (*requestedAbortFlag))
            {
              File_close(&fileHandle);
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              break;
            }

            // close file
            File_close(&fileHandle);

            if (fragmentNode != NULL)
            {
              // add fragment to file fragment list
              FragmentList_addRange(fragmentNode,
                                    fragmentOffset,
                                    fragmentSize
                                   );
            }

            // entry restored
            restoredFlag = TRUE;
          }

          // close archive file, free resources
          Archive_closeEntry(&archiveEntryInfo);

          // free resources
          String_delete(fileName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        {
          // read image
          ArchiveEntryInfo archiveEntryInfo;
          String           imageName = String_new();
          DeviceInfo       deviceInfo;
          uint64           blockOffset,blockCount;
          error = Archive_readImageEntry(&archiveEntryInfo,
                                         &archiveHandle,
                                         NULL,  // deltaCompressAlgorithm
                                         NULL,  // byteCompressAlgorithm
                                         NULL,  // cryptType
                                         NULL,  // cryptAlgorithm
                                         NULL,  // cryptSalt
                                         NULL,  // cryptKey
                                         imageName,
                                         &deviceInfo,
                                         NULL,  // fileSystemType
                                         NULL,  // deltaSourceHandleName
                                         NULL,  // deltaSourceHandleSize
                                         &blockOffset,
                                         &blockCount
                                        );
          if (error != ERROR_NONE)
          {
            String_delete(imageName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }
          if (deviceInfo.blockSize > BUFFER_SIZE)
          {
            String_delete(imageName);
            if (failError == ERROR_NONE) failError = ERROR_INVALID_DEVICE_BLOCK_SIZE;
            break;
          }
          assert(deviceInfo.blockSize > 0);

          if (String_equals(name,imageName))
          {
//            abortFlag = !updateStatusInfo(&restoreInfo);

            // open file
            FileHandle fileHandle;
            error = File_open(&fileHandle,destinationFileName,FILE_OPEN_WRITE);
            if (error != ERROR_NONE)
            {
              printError(_("cannot create/write to file '%s' (error: %s)"),
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(imageName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            // seek to fragment position
            error = File_seek(&fileHandle,blockOffset*(uint64)deviceInfo.blockSize);
            if (error != ERROR_NONE)
            {
              printError(_("cannot write file '%s' (error: %s)"),
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
              File_close(&fileHandle);
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(imageName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            // write image data to file
            uint64 block = 0;
            while (   ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
                   && (block < blockCount)
                  )
            {
              // pause
              while ((pauseFlag != NULL) && (*pauseFlag))
              {
                Misc_udelay(500LL*US_PER_MS);
              }

              ulong bufferBlockCount = MIN(blockCount-block,BUFFER_SIZE/deviceInfo.blockSize);

              // read data from archive
              error = Archive_readData(&archiveEntryInfo,buffer,bufferBlockCount*(uint64)deviceInfo.blockSize);
              if (error != ERROR_NONE)
              {
                if (failError == ERROR_NONE) failError = error;
                break;
              }

              // write data to file
              error = File_write(&fileHandle,buffer,bufferBlockCount*(uint64)deviceInfo.blockSize);
              if (error != ERROR_NONE)
              {
                printError(_("cannot write file '%s' (error: %s)"),
                           String_cString(destinationFileName),
                           Error_getText(error)
                          );
                if (failError == ERROR_NONE) failError = error;
                break;
              }
//              abortFlag = !updateStatusInfo(&restoreInfo);

              block += (uint64)bufferBlockCount;
            }
            if      (failError != ERROR_NONE)
            {
              File_close(&fileHandle);
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(imageName);
              break;
            }
            else if ((requestedAbortFlag != NULL) && (*requestedAbortFlag))
            {
              File_close(&fileHandle);
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(imageName);
              break;
            }

            // close file
            File_close(&fileHandle);

            if (fragmentNode != NULL)
            {
              // add fragment to file fragment list
              FragmentList_addRange(fragmentNode,
                                    blockOffset*(uint64)deviceInfo.blockSize,
                                    blockCount*(uint64)deviceInfo.blockSize
                                   );
            }

            // entry restored
            restoredFlag = TRUE;
          }

          // close archive file, free resources
          Archive_closeEntry(&archiveEntryInfo);

          // free resources
          String_delete(imageName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        {
          // read hard link
          ArchiveEntryInfo archiveEntryInfo;
          StringList       fileNameList;
          StringList_init(&fileNameList);
          uint64           fragmentOffset,fragmentSize;
          error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                            &archiveHandle,
                                            NULL,  // deltaCompressAlgorithm
                                            NULL,  // byteCompressAlgorithm
                                            NULL,  // cryptType
                                            NULL,  // cryptAlgorithm
                                            NULL,  // cryptSalt
                                            NULL,  // cryptKey
                                            &fileNameList,
                                            NULL,  // fileInfo
                                            NULL,  // fileExtendedAttributeList
                                            NULL,  // deltaSourceHandleName
                                            NULL,  // deltaSourceHandleSize
                                            &fragmentOffset,
                                            &fragmentSize
                                           );
          if (error != ERROR_NONE)
          {
            StringList_done(&fileNameList);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          if (StringList_contains(&fileNameList,name))
          {
//              abortFlag = !updateStatusInfo(&restoreInfo);

            // open file
            FileHandle fileHandle;
            error = File_open(&fileHandle,destinationFileName,FILE_OPEN_WRITE);
            if (error != ERROR_NONE)
            {
              printError(_("cannot create/write to file '%s' (error: %s)"),
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
              Archive_closeEntry(&archiveEntryInfo);
              StringList_done(&fileNameList);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            // seek to fragment position
            error = File_seek(&fileHandle,fragmentOffset);
            if (error != ERROR_NONE)
            {
              printError(_("cannot write file '%s' (error: %s)"),
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
              File_close(&fileHandle);
              Archive_closeEntry(&archiveEntryInfo);
              StringList_done(&fileNameList);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            // write file data
            uint64 length = 0;
            while (   ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
                   && (length < fragmentSize)
                  )
            {
              // pause
              while ((pauseFlag != NULL) && (*pauseFlag))
              {
                Misc_udelay(500LL*US_PER_MS);
              }

              ulong bufferLength = MIN(fragmentSize-length,BUFFER_SIZE);

              // read data from archive
              error = Archive_readData(&archiveEntryInfo,buffer,bufferLength);
              if (error != ERROR_NONE)
              {
                if (failError == ERROR_NONE) failError = error;
                break;
              }

              // write data to file
              error = File_write(&fileHandle,buffer,bufferLength);
              if (error != ERROR_NONE)
              {
                printError(_("cannot write file '%s' (error: %s)"),
                           String_cString(destinationFileName),
                           Error_getText(error)
                          );
                if (failError == ERROR_NONE) failError = error;
                break;
              }
//                  abortFlag = !updateStatusInfo(&restoreInfo);

              length += bufferLength;
            }
            if      (failError != ERROR_NONE)
            {
              File_close(&fileHandle);
              Archive_closeEntry(&archiveEntryInfo);
              StringList_done(&fileNameList);
              break;
            }
            else if ((requestedAbortFlag != NULL) && (*requestedAbortFlag))
            {
              File_close(&fileHandle);
              Archive_closeEntry(&archiveEntryInfo);
              StringList_done(&fileNameList);
              break;
            }

            // close file
            File_close(&fileHandle);

            if (fragmentNode != NULL)
            {
              // add fragment to file fragment list
              FragmentList_addRange(fragmentNode,
                                    fragmentOffset,
                                    fragmentSize
                                   );
            }

            // entry restored
            restoredFlag = TRUE;
          }

          // close archive file, free resources
          Archive_closeEntry(&archiveEntryInfo);

          // free resources
          StringList_done(&fileNameList);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
      case ARCHIVE_ENTRY_TYPE_LINK:
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
      case ARCHIVE_ENTRY_TYPE_META:
        error = Archive_skipNextEntry(&archiveHandle);
        if (error != ERROR_NONE)
        {
          if (failError == ERROR_NONE) failError = error;
          break;
        }
        break;
      case ARCHIVE_ENTRY_TYPE_SALT:
      case ARCHIVE_ENTRY_TYPE_KEY:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNREACHABLE();
        #else
          error = Archive_skipNextEntry(&archiveHandle);
          if (error != ERROR_NONE)
          {
            if (failError == ERROR_NONE) failError = error;
            break;
          }
        #endif /* NDEBUG */
        break;
      case ARCHIVE_ENTRY_TYPE_SIGNATURE:
        error = Archive_skipNextEntry(&archiveHandle);
        if (error != ERROR_NONE)
        {
          if (failError == ERROR_NONE) failError = error;
          break;
        }
        break;
      case ARCHIVE_ENTRY_TYPE_UNKNOWN:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNREACHABLE();
        #endif /* NDEBUG */
        break;
    }
  }

  // close archive
  Archive_close(&archiveHandle,FALSE);

  // done storage
  (void)Storage_done(&storageInfo);

  // free resources
  free(buffer);
  String_delete(printableStorageName);

  if      (failError != ERROR_NONE)
  {
    return failError;
  }
  else if (!restoredFlag)
  {
    return ERROR_ENTRY_NOT_FOUND;
  }
  else
  {
    return ERROR_NONE;
  }
}

/*---------------------------------------------------------------------*/

Errors DeltaSource_initAll(void)
{
  return ERROR_NONE;
}

void DeltaSource_doneAll(void)
{
}

Errors DeltaSource_openEntry(DeltaSourceHandle *deltaSourceHandle,
                             DeltaSourceList   *deltaSourceList,
                             ConstString       sourceStorageName,
                             ConstString       name,
                             int64             size,
                             const JobOptions  *jobOptions
                            )
{
  assert(deltaSourceHandle != NULL);
  assert(deltaSourceList != NULL);
  assert(name != NULL);
  assert(jobOptions != NULL);

  // init variables
  deltaSourceHandle->name        = NULL;
  deltaSourceHandle->baseOffset  = 0LL;
  StorageSpecifier storageSpecifier;
  Storage_initSpecifier(&storageSpecifier);
  StorageSpecifier localStorageSpecifier;
  Storage_initSpecifier(&localStorageSpecifier);

  Errors error = ERROR_UNKNOWN;

  SEMAPHORE_LOCKED_DO(&deltaSourceList->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // find/add temporary
    FragmentNode *fragmentNode = FragmentList_find(&deltaSourceList->temporaryList,name);
    if (fragmentNode == NULL)
    {
      DeltaSourceInfo deltaSourceInfo;
      deltaSourceInfo.filePath    = NULL;
      deltaSourceInfo.tmpFilePath = NULL;
      fragmentNode = FragmentList_add(&deltaSourceList->temporaryList,
                                      name,
                                      (size != SOURCE_SIZE_UNKNOWN) ? (uint64)size : 0,
                                      &deltaSourceInfo,
                                      sizeof(deltaSourceInfo),
                                      0,
                                      CALLBACK_(freeDeltaSourceInfo,NULL)
                                     );
    }
    assert(fragmentNode != NULL);

    // check if source can be restored from some local file given by command options --delta-source
    if ((size == SOURCE_SIZE_UNKNOWN) || !FragmentList_isComplete(fragmentNode))
    {
      const DeltaSourceNode *deltaSourceNode;
      LIST_ITERATEX(deltaSourceList,
                    deltaSourceNode,
                    (
                        (size == SOURCE_SIZE_UNKNOWN)
                     || !FragmentList_isComplete(fragmentNode)
                    )
                   )
      {
        // check if available in file system, but is not an archive file
        if (   (Storage_parseName(&storageSpecifier,deltaSourceNode->storageName) == ERROR_NONE)
            && Storage_isInFileSystem(&storageSpecifier)
            && !Archive_isArchiveFile(storageSpecifier.archiveName)
           )
        {
          DeltaSourceInfo *deltaSourceInfo = (DeltaSourceInfo*)fragmentNode->data;
          assert(deltaSourceInfo != NULL);

          // complete file size as fragment size
          FileInfo fileInfo;
          if (File_getInfo(&fileInfo, storageSpecifier.archiveName))
          {
            FragmentList_addRange(fragmentNode,0,fileInfo.size);
          }

          // use local file as source
          deltaSourceInfo->filePath = deltaSourceNode->storageName;
          deltaSourceHandle->name   = String_duplicate(storageSpecifier.archiveName);
        }
      }
    }

    // check if source can be restored from local archives given by command option --delta-source
    if ((size == SOURCE_SIZE_UNKNOWN) || !FragmentList_isComplete(fragmentNode))
    {
      DeltaSourceNode *deltaSourceNode;
      LIST_ITERATEX(deltaSourceList,
                    deltaSourceNode,
                    (   (size == SOURCE_SIZE_UNKNOWN)
                     || FragmentList_isComplete(fragmentNode)
                    )
                   )
      {
        DeltaSourceInfo *deltaSourceInfo = (DeltaSourceInfo*)fragmentNode->data;
        assert(deltaSourceInfo != NULL);
        if (deltaSourceInfo->filePath == NULL)
        {
          // check if available in file system and is an archive file
          if (   (Storage_parseName(&storageSpecifier,deltaSourceNode->storageName) == ERROR_NONE)
              && Storage_isInFileSystem(&storageSpecifier)
              && Archive_isArchiveFile(storageSpecifier.archiveName)
             )
          {
            // create temporary file as delta source
            if (deltaSourceInfo->tmpFilePath == NULL)
            {
              String tmpFilePath = String_new();
              error = File_getTmpFileName(tmpFilePath,NULL,tmpDirectory);
              if (error == ERROR_NONE)
              {
                deltaSourceInfo->tmpFilePath = tmpFilePath;
              }
              else
              {
                String_delete(tmpFilePath);
              }
            }

            // restore into temporary file
            if (error == ERROR_NONE)
            {
              error = restoreFile(&storageSpecifier,
                                  name,
                                  deltaSourceList,
                                  jobOptions,
                                  deltaSourceInfo->tmpFilePath,
                                  fragmentNode,
                                  CALLBACK_(getPasswordFromConsole,NULL),
                                  NULL,  // pauseFlag
                                  NULL,  // requestedAbortFlag,
                                  NULL   // logHandle
                                 );
              if (error == ERROR_NONE)
              {
                // use archive entry as delta source
                deltaSourceInfo->filePath = deltaSourceInfo->tmpFilePath;
                deltaSourceHandle->name   = String_duplicate(deltaSourceNode->storageName);
              }
            }
          }
        }
        else
        {
          // use archive entry as delta source
          deltaSourceHandle->name = String_duplicate(deltaSourceNode->storageName);
        }
      }
    }

    // check if source can be restored from (non-file system) storage names given by command option --delta-source
    if ((size == SOURCE_SIZE_UNKNOWN) || !FragmentList_isComplete(fragmentNode))
    {
      // restore file into temporary file
      DeltaSourceNode *deltaSourceNode;
      LIST_ITERATEX(deltaSourceList,
                    deltaSourceNode,
                    (   (size == SOURCE_SIZE_UNKNOWN)
                     || FragmentList_isComplete(fragmentNode)
                    )
                   )
      {
        DeltaSourceInfo *deltaSourceInfo = (DeltaSourceInfo*)fragmentNode->data;
        assert(deltaSourceInfo != NULL);
        if (deltaSourceInfo->filePath == NULL)
        {
          // check if not available in file system
          if (   (Storage_parseName(&storageSpecifier,deltaSourceNode->storageName) == ERROR_NONE)
              && !Storage_isInFileSystem(&storageSpecifier)
             )
          {
            // create local copy of storage file
            error = createLocalStorageArchive(&localStorageSpecifier,
                                              &storageSpecifier,
                                              jobOptions
                                             );
            if (error == ERROR_NONE)
            {
              // create temporary file as delta source
              if (deltaSourceInfo->tmpFilePath == NULL)
              {
                String tmpFilePath = String_new();
                error = File_getTmpFileName(tmpFilePath,NULL,tmpDirectory);
                if (error == ERROR_NONE)
                {
                  deltaSourceInfo->tmpFilePath = tmpFilePath;
                }
                else
                {
                  String_delete(tmpFilePath);
                }
              }

              if (error == ERROR_NONE)
              {
                // set restore flag for source node and restore to temporary file
                error = restoreFile(&localStorageSpecifier,
                                    name,
                                    deltaSourceList,
                                    jobOptions,
                                    deltaSourceInfo->tmpFilePath,
                                    fragmentNode,
                                    CALLBACK_(getPasswordFromConsole,NULL),
                                    NULL,  // pauseFlag
                                    NULL,  // requestedAbortFlag
                                    NULL   // logHandle
                                   );
                if (error == ERROR_NONE)
                {
                  // use archive entry as delta source
                  deltaSourceInfo->filePath = deltaSourceInfo->tmpFilePath;
                  deltaSourceHandle->name   = String_duplicate(deltaSourceNode->storageName);
                }
              }

              // delete local copy of storage file
              doneLocalStorageArchive(&localStorageSpecifier);
            }
          }
        }
        else
        {
          // use archive entry as delta source
          deltaSourceHandle->name = String_duplicate(deltaSourceNode->storageName);
        }
      }
    }

    // check if source can be restored from orginal archive in file system
    if ((size == SOURCE_SIZE_UNKNOWN) || !FragmentList_isComplete(fragmentNode))
    {
      if (   !String_isEmpty(sourceStorageName)
          && (Storage_parseName(&storageSpecifier,sourceStorageName) == ERROR_NONE)
          && Storage_isInFileSystem(&storageSpecifier)
         )
      {
        DeltaSourceInfo *deltaSourceInfo = (DeltaSourceInfo*)fragmentNode->data;
        assert(deltaSourceInfo != NULL);
        if (deltaSourceInfo->filePath == NULL)
        {
          if (Archive_isArchiveFile(storageSpecifier.archiveName))
          {
            // create temporary file as delta source
            if (deltaSourceInfo->tmpFilePath == NULL)
            {
              String tmpFilePath = String_new();
              error = File_getTmpFileName(tmpFilePath,NULL,tmpDirectory);
              if (error == ERROR_NONE)
              {
                deltaSourceInfo->tmpFilePath = tmpFilePath;
              }
              else
              {
                String_delete(tmpFilePath);
              }
            }

            // restore to temporary file
            if (error == ERROR_NONE)
            {
              error = restoreFile(&storageSpecifier,
                                  name,
                                  deltaSourceList,
                                  jobOptions,
                                  deltaSourceInfo->tmpFilePath,
                                  fragmentNode,
                                  CALLBACK_(getPasswordFromConsole,NULL),
                                  NULL,  // pauseFlag
                                  NULL,  // requestedAbortFlag
                                  NULL   // logHandle
                                 );
              if (error == ERROR_NONE)
              {
                // use orginal archive as delta source
                deltaSourceInfo->filePath = deltaSourceInfo->tmpFilePath;
                deltaSourceHandle->name   = String_duplicate(storageSpecifier.archiveName);
              }
            }
          }
        }
        else
        {
          // use original archive as delta source
          deltaSourceHandle->name = String_duplicate(storageSpecifier.archiveName);
        }
      }
    }

    // check if source can be restored from original storage name
    if ((size == SOURCE_SIZE_UNKNOWN) || !FragmentList_isComplete(fragmentNode))
    {
      if (   !String_isEmpty(sourceStorageName)
          && (Storage_parseName(&storageSpecifier,sourceStorageName) == ERROR_NONE)
         )
      {
        // create local copy of storage file
        error = createLocalStorageArchive(&localStorageSpecifier,
                                          &storageSpecifier,
                                          jobOptions
                                         );
        if (error == ERROR_NONE)
        {
          DeltaSourceInfo *deltaSourceInfo = (DeltaSourceInfo*)fragmentNode->data;
          assert(deltaSourceInfo != NULL);

          // create temporary file as delta source
          if (deltaSourceInfo->tmpFilePath == NULL)
          {
            String tmpFilePath = String_new();
            error = File_getTmpFileName(tmpFilePath,NULL,tmpDirectory);
            if (error == ERROR_NONE)
            {
              deltaSourceInfo->tmpFilePath = tmpFilePath;
            }
            else
            {
              String_delete(tmpFilePath);
            }
          }

          // restore into temporary file
          if (error == ERROR_NONE)
          {
            error = restoreFile(&localStorageSpecifier,
                                name,
                                deltaSourceList,
                                jobOptions,
                                deltaSourceInfo->tmpFilePath,
                                fragmentNode,
                                CALLBACK_(getPasswordFromConsole,NULL),
                                NULL,  // pauseFlag
                                NULL,  // requestedAbortFlag
                                NULL   // logHandle
                               );
            if (error == ERROR_NONE)
            {
              // use archive entry as delta source
              deltaSourceHandle->name   = String_duplicate(sourceStorageName);
              deltaSourceInfo->filePath = deltaSourceInfo->tmpFilePath;
            }

            // delete local copy of storage file
            doneLocalStorageArchive(&localStorageSpecifier);
          }
        }
      }
    }

    // open restored file
    DeltaSourceInfo *deltaSourceInfo = (DeltaSourceInfo*)fragmentNode->data;
    assert(deltaSourceInfo != NULL);
    if (deltaSourceInfo->filePath != NULL)
    {
      error = File_open(&deltaSourceHandle->tmpFileHandle,deltaSourceInfo->filePath,FILE_OPEN_READ);
    }
    else
    {
      error = (sourceStorageName != NULL)
        ? ERRORX_(DELTA_SOURCE_NOT_FOUND,0,"%s",sourceStorageName)
        : ERROR_DELTA_SOURCE_NOT_FOUND;
    }
  }
  assert(error != ERROR_UNKNOWN);

  // free resources
  Storage_doneSpecifier(&localStorageSpecifier);
  Storage_doneSpecifier(&storageSpecifier);

  return error;
}

void DeltaSource_closeEntry(DeltaSourceHandle *deltaSourceHandle)
{
  assert(deltaSourceHandle != NULL);

  // close source file
  File_close(&deltaSourceHandle->tmpFileHandle);

  // free resources
  String_delete(deltaSourceHandle->name);
}

ConstString DeltaSource_getName(const DeltaSourceHandle *deltaSourceHandle)
{
  assert(deltaSourceHandle != NULL);

  return deltaSourceHandle->name;
}

uint64 DeltaSource_getSize(const DeltaSourceHandle *deltaSourceHandle)
{
  assert(deltaSourceHandle != NULL);

  return File_getSize(&deltaSourceHandle->tmpFileHandle);
}

void DeltaSource_setBaseOffset(DeltaSourceHandle *deltaSourceHandle, uint64 offset)
{
  assert(deltaSourceHandle != NULL);

  deltaSourceHandle->baseOffset = offset;
}

Errors DeltaSource_getEntryDataBlock(DeltaSourceHandle *deltaSourceHandle,
                                     void              *buffer,
                                     uint64            offset,
                                     ulong             length,
                                     ulong             *bytesRead
                                    )
{
  Errors error;

  assert(deltaSourceHandle != NULL);
  assert(buffer != NULL);
  assert(bytesRead != NULL);

  error = File_seek(&deltaSourceHandle->tmpFileHandle,deltaSourceHandle->baseOffset+offset);
  if (error != ERROR_NONE)
  {
    return error;
  }

  error = File_read(&deltaSourceHandle->tmpFileHandle,buffer,length,bytesRead);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
