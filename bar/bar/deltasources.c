/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver source functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "patternlists.h"
#include "files.h"
#include "fragmentlists.h"

#include "errors.h"
#include "storage.h"
#include "archive.h"
#include "bar_global.h"
#include "bar.h"

#include "deltasources.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// file data buffer size
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
  error = Storage_copy(storageSpecifier,
                       jobOptions,
                       &globalOptions.maxBandWidthList,
                       NULL,//StorageRequestVolumeFunction storageRequestVolumeFunction,
                       NULL,//void                         *storageRequestVolumeUserData,
                       NULL,//StorageStatusInfoFunction    storageStatusInfoFunction,
                       NULL,//void                         *storageStatusInfoUserData,
                       localStorageSpecifier->archiveName
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
* Name   : restoreFile
* Purpose: restore file from archive
* Input  : archiveName         - archive file name
*          name                - name of entry to restore
*          deltaSourceList     - delta source list
*          jobOptions          - job options
*          destinationFileName - destination file name
*          fragmentNode        - fragment node (can be NULL)
*          getPasswordFunction - get password call back
*          getPasswordUserData - user data for get password call back
*          pauseFlag           - pause flag (can be NULL)
*          requestedAbortFlag  - request abort flag (can be NULL)
*          logHandle           - log handle (can be NULL)
* Output : -
* Return : ERROR_NONE if file restored, otherwise error code
* Notes  : -
\***********************************************************************/

LOCAL Errors restoreFile(StorageSpecifier    *storageSpecifier,
                         ConstString         name,
                         DeltaSourceList     *deltaSourceList,
                         const JobOptions    *jobOptions,
                         ConstString         destinationFileName,
                         FragmentNode        *fragmentNode,
                         GetPasswordFunction getPasswordFunction,
                         void                *getPasswordUserData,
                         bool                *pauseFlag,
                         bool                *requestedAbortFlag,
                         LogHandle           *logHandle
                        )
{
  bool              restoredFlag;
  StorageHandle     storageHandle;
  byte              *buffer;
//  bool              abortFlag;
  Errors            error;
  ArchiveInfo       archiveInfo;
  Errors            failError;
  ArchiveEntryInfo  archiveEntryInfo;
  ArchiveEntryTypes archiveEntryType;

  assert(storageSpecifier != NULL);
  assert(name != NULL);
  assert(jobOptions != NULL);
  assert(destinationFileName != NULL);

  // initialize variables
  restoredFlag = FALSE;

  // allocate resources
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init storage
  error = Storage_init(&storageHandle,
                       storageSpecifier,
                       jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK(NULL,NULL),  // updateStatusInfo
                       CALLBACK(NULL,NULL),  // getPassword
                       CALLBACK(NULL,NULL)  // requestVolume
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize storage '%s' (error: %s)!\n",
               String_cString(Storage_getPrintableName(storageSpecifier,NULL)),
               Error_getText(error)
              );
    return error;
  }

  // open archive
  error = Archive_open(&archiveInfo,
                       &storageHandle,
                       storageSpecifier,
                       NULL,  // archive name
                       deltaSourceList,
                       jobOptions,
                       CALLBACK(getPasswordFunction,getPasswordUserData),
                       logHandle
                      );
  if (error != ERROR_NONE)
  {
    (void)Storage_done(&storageHandle);
    return error;
  }

  // read archive entries
  failError = ERROR_NONE;
  while (   !restoredFlag
         && ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
         && !Archive_eof(&archiveInfo,TRUE)
         && (failError == ERROR_NONE)
        )
  {
    // pause
    while ((pauseFlag != NULL) && (*pauseFlag))
    {
      Misc_udelay(500*1000);
    }

    // get next archive entry type
    error = Archive_getNextArchiveEntryType(&archiveInfo,
                                            &archiveEntryType,
                                            TRUE
                                           );
    if (error != ERROR_NONE)
    {
      if (failError == ERROR_NONE) failError = error;
      break;
    }

    switch (archiveEntryType)
    {
      case ARCHIVE_ENTRY_TYPE_FILE:
        {
          String     fileName;
          uint64     fragmentOffset,fragmentSize;
          FileHandle fileHandle;
          uint64     length;
          ulong      bufferLength;

          // read file
          fileName = String_new();
          error = Archive_readFileEntry(&archiveEntryInfo,
                                        &archiveInfo,
                                        NULL,  // deltaCompressAlgorithm
                                        NULL,  // byteCompressAlgorithm
                                        NULL,  // cryptAlgorithm
                                        NULL,  // cryptType
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
            continue;
          }

          if (String_equals(name,fileName))
          {
//            abortFlag = !updateStatusInfo(&restoreInfo);

            // open file
            error = File_open(&fileHandle,destinationFileName,FILE_OPEN_WRITE);
            if (error != ERROR_NONE)
            {
              printError("Cannot create/write to file '%s' (error: %s)\n",
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              failError = error;
              continue;
            }

            // seek to fragment position
            error = File_seek(&fileHandle,fragmentOffset);
            if (error != ERROR_NONE)
            {
              printError("Cannot write file '%s' (error: %s)\n",
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
              File_close(&fileHandle);
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              failError = error;
              continue;
            }

            // write file data
            length = 0;
            while (   ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
                   && (length < fragmentSize)
                  )
            {
              // pause
              while ((pauseFlag != NULL) && (*pauseFlag))
              {
                Misc_udelay(500*1000);
              }

              bufferLength = MIN(fragmentSize-length,BUFFER_SIZE);

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
                printError("Cannot write file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Error_getText(error)
                          );
                failError = error;
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
              continue;
            }
            else if ((requestedAbortFlag != NULL) && (*requestedAbortFlag))
            {
              File_close(&fileHandle);
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              continue;
            }

            // close file
            File_close(&fileHandle);

            if (fragmentNode != NULL)
            {
              // add fragment to file fragment list
              FragmentList_addEntry(fragmentNode,
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
          String     imageName;
          DeviceInfo deviceInfo;
          uint64     blockOffset,blockCount;
          FileHandle fileHandle;
          uint64     block;
          ulong      bufferBlockCount;

          // read image
          imageName = String_new();
          error = Archive_readImageEntry(&archiveEntryInfo,
                                         &archiveInfo,
                                         NULL,  // deltaCompressAlgorithm
                                         NULL,  // byteCompressAlgorithm
                                         NULL,  // cryptAlgorithm
                                         NULL,  // cryptType
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
            error = File_open(&fileHandle,destinationFileName,FILE_OPEN_WRITE);
            if (error != ERROR_NONE)
            {
              printError("Cannot create/write to file '%s' (error: %s)\n",
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(imageName);
              failError = error;
              continue;
            }

            // seek to fragment position
            error = File_seek(&fileHandle,blockOffset*(uint64)deviceInfo.blockSize);
            if (error != ERROR_NONE)
            {
              printError("Cannot write file '%s' (error: %s)\n",
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
              File_close(&fileHandle);
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(imageName);
              failError = error;
              continue;
            }

            // write image data to file
            block = 0;
            while (   ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
                   && (block < blockCount)
                  )
            {
              // pause
              while ((pauseFlag != NULL) && (*pauseFlag))
              {
                Misc_udelay(500*1000);
              }

              bufferBlockCount = MIN(blockCount-block,BUFFER_SIZE/deviceInfo.blockSize);

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
                printError("Cannot write file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Error_getText(error)
                          );
                failError = error;
                break;
              }
//              abortFlag = !updateStatusInfo(&restoreInfo);

              block += (uint64)bufferBlockCount;
            }

            // close file
            File_close(&fileHandle);

            if (fragmentNode != NULL)
            {
              // add fragment to file fragment list
              FragmentList_addEntry(fragmentNode,
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
          StringList       fileNameList;
          uint64           fragmentOffset,fragmentSize;
//          const StringNode *stringNode;
//          String           fileName;
          FileHandle       fileHandle;
          uint64           length;
          ulong            bufferLength;

          // read hard link
          StringList_init(&fileNameList);
          error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                            &archiveInfo,
                                            NULL,  // deltaCompressAlgorithm
                                            NULL,  // byteCompressAlgorithm
                                            NULL,  // cryptAlgorithm
                                            NULL,  // cryptType
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
            continue;
          }

          if (StringList_contain(&fileNameList,name))
          {
//              abortFlag = !updateStatusInfo(&restoreInfo);

            // open file
            error = File_open(&fileHandle,destinationFileName,FILE_OPEN_WRITE);
            if (error != ERROR_NONE)
            {
              printError("Cannot create/write to file '%s' (error: %s)\n",
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
              Archive_closeEntry(&archiveEntryInfo);
              StringList_done(&fileNameList);
              failError = error;
              continue;
            }

            // seek to fragment position
            error = File_seek(&fileHandle,fragmentOffset);
            if (error != ERROR_NONE)
            {
              printError("Cannot write file '%s' (error: %s)\n",
                         String_cString(destinationFileName),
                         Error_getText(error)
                        );
              File_close(&fileHandle);
              Archive_closeEntry(&archiveEntryInfo);
              StringList_done(&fileNameList);
              failError = error;
              continue;
            }

            // write file data
            length = 0;
            while (   ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
                   && (length < fragmentSize)
                  )
            {
              // pause
              while ((pauseFlag != NULL) && (*pauseFlag))
              {
                Misc_udelay(500*1000);
              }

              bufferLength = MIN(fragmentSize-length,BUFFER_SIZE);

              // read data from archive
              error = Archive_readData(&archiveEntryInfo,buffer,bufferLength);
              if (error != ERROR_NONE)
              {
                failError = error;
                break;
              }

              // write data to file
              error = File_write(&fileHandle,buffer,bufferLength);
              if (error != ERROR_NONE)
              {
                printError("Cannot write file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Error_getText(error)
                          );
                failError = error;
                break;
              }
//                  abortFlag = !updateStatusInfo(&restoreInfo);

              length += bufferLength;
            }
            if      (failError != ERROR_NONE)
            {
              File_close(&fileHandle);
              break;
            }
            else if ((requestedAbortFlag != NULL) && (*requestedAbortFlag))
            {
              File_close(&fileHandle);
              break;
            }

            // close file
            File_close(&fileHandle);

            if (fragmentNode != NULL)
            {
              // add fragment to file fragment list
              FragmentList_addEntry(fragmentNode,
                                    fragmentOffset,
                                    fragmentSize
                                   );
            }

            // entry restored
            restoredFlag = TRUE;
          }
          if (failError != ERROR_NONE)
          {
            Archive_closeEntry(&archiveEntryInfo);
            StringList_done(&fileNameList);
            continue;
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
        error = Archive_skipNextEntry(&archiveInfo);
        if (error != ERROR_NONE)
        {
          if (failError == ERROR_NONE) failError = error;
        }
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; /* not reached */
    }
  }

  // close archive
  Archive_close(&archiveInfo);

  // done storage
  (void)Storage_done(&storageHandle);

  // free resources
  free(buffer);

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
  bool             restoredFlag;
  Errors           error;
  FragmentNode     fragmentNode;
  bool             semaphoreLock;
  DeltaSourceNode  *deltaSourceNode;
  String           tmpFileName;
  StorageSpecifier storageSpecifier,localStorageSpecifier;
//  String           localStorageName;

  assert(deltaSourceHandle != NULL);
  assert(name != NULL);
  assert(jobOptions != NULL);

  // init variables
  deltaSourceHandle->name        = NULL;
  deltaSourceHandle->size        = 0LL;
  deltaSourceHandle->tmpFileName = NULL;
  deltaSourceHandle->baseOffset  = 0LL;
  Storage_initSpecifier(&storageSpecifier);
  Storage_initSpecifier(&localStorageSpecifier);

  restoredFlag = FALSE;
  error        = ERROR_UNKNOWN;

//fprintf(stderr,"%s, %d: name=%s storage=%s\n",__FILE__,__LINE__,String_cString(name),String_cString(sourceStorageName));
//if (deltaSourceList!= NULL) { DeltaSourceNode *n=deltaSourceList->head; fprintf(stderr,"%s, %d: count=%d\n",__FILE__,__LINE__,deltaSourceList->count);while (n != NULL) { fprintf(stderr,"%s, %d: n=%p: %s\n",__FILE__,__LINE__,n,String_cString(n->storageName)); n=n->next;} }
  // check if source can be restored from local files given by command option --delta-source
  if (!restoredFlag)
  {
    if (deltaSourceList != NULL)
    {
      SEMAPHORE_LOCKED_DO(semaphoreLock,&deltaSourceList->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        LIST_ITERATE(deltaSourceList,deltaSourceNode)
        {
//fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,String_cString(deltaSourceNode->storageName));
//Errors b =Storage_parseName(&storageSpecifier,deltaSourceNode->storageName);
//fprintf(stderr,"%s, %d: %d %d %d: %s\n",__FILE__,__LINE__,b,Storage_isLocalArchive(&storageSpecifier),Archive_isArchiveFile(storageSpecifier.archiveName),String_cString(storageSpecifier.archiveName));
          // check if available in file system, but not an archive file
          if (   (Storage_parseName(&storageSpecifier,deltaSourceNode->storageName) == ERROR_NONE)
              && Storage_isInFileSystem(&storageSpecifier)
              && !Archive_isArchiveFile(storageSpecifier.archiveName)
             )
          {
            // open local file as source
            error = File_open(&deltaSourceHandle->tmpFileHandle,storageSpecifier.archiveName,FILE_OPEN_READ);
            if (error == ERROR_NONE)
            {
              deltaSourceHandle->name = deltaSourceNode->storageName;
              restoredFlag = TRUE;
            }
          }

          // stop if restored
          if (restoredFlag) break;
        }
      }
    }
  }

  // check if source can be restored from local archives given by command option --delta-source
  if (!restoredFlag)
  {
    if (deltaSourceList != NULL)
    {
      // create temporary file as delta source
      tmpFileName = String_new();
      error = File_getTmpFileName(tmpFileName,NULL,tmpDirectory);
      if (error == ERROR_NONE)
      {
        // init variables
        FragmentList_initNode(&fragmentNode,name,size,NULL,0);

        SEMAPHORE_LOCKED_DO(semaphoreLock,&deltaSourceList->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
        {
          LIST_ITERATE(deltaSourceList,deltaSourceNode)
          {
            // check if restore in progress (avoid infinite loops)
            if (!deltaSourceNode->locked)
            {
              // check if available in file system and an archive file
              if (   (Storage_parseName(&storageSpecifier,deltaSourceNode->storageName) == ERROR_NONE)
                  && Storage_isInFileSystem(&storageSpecifier)
                  && Archive_isArchiveFile(storageSpecifier.archiveName)
                 )
              {
                // set restore flag for source node and restore to temporary file
                BLOCK_DO(deltaSourceNode->locked = TRUE,
                         deltaSourceNode->locked = FALSE,
                {
                  error = restoreFile(&storageSpecifier,
                                      name,
                                      deltaSourceList,
                                      jobOptions,
                                      tmpFileName,
                                      &fragmentNode,
                                      CALLBACK(getPasswordConsole,NULL),
                                      NULL,  // pauseFlag
                                      NULL,  // requestedAbortFlag,
                                      NULL   // logHandle
                                     );
                  if (error == ERROR_NONE)
                  {
                    deltaSourceHandle->name = deltaSourceNode->storageName;
                  }
                });
              }
            }

            // stop if complete
            if (   (size != SOURCE_SIZE_UNKNOWN)
                && FragmentList_isEntryComplete(&fragmentNode)
               )
            {
              break;
            }
          }
        }

        if (   (deltaSourceHandle->name != NULL)
            && (   (size == SOURCE_SIZE_UNKNOWN)
                || FragmentList_isEntryComplete(&fragmentNode)
               )
           )
        {
          // open temporary restored file
          error = File_open(&deltaSourceHandle->tmpFileHandle,tmpFileName,FILE_OPEN_READ);
          if (error == ERROR_NONE)
          {
            deltaSourceHandle->tmpFileName = tmpFileName;
            restoredFlag = TRUE;
          }
        }

        // free resources
        FragmentList_doneNode(&fragmentNode);
        if (!restoredFlag)
        {
          File_delete(tmpFileName,FALSE);
          String_delete(tmpFileName);
        }
      }
      else
      {
        String_delete(tmpFileName);
      }
    }
  }

  // check if source can be restored from (non-file system) storage names given by command option --delta-source
  if (!restoredFlag)
  {
    if (deltaSourceList != NULL)
    {
      // create temporary file as delta source
      tmpFileName = String_new();
      error = File_getTmpFileName(tmpFileName,NULL,tmpDirectory);
      if (error == ERROR_NONE)
      {
        // init variables
        FragmentList_initNode(&fragmentNode,name,size,NULL,0);

        SEMAPHORE_LOCKED_DO(semaphoreLock,&deltaSourceList->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
        {
          LIST_ITERATE(deltaSourceList,deltaSourceNode)
          {
            // check if restore in progress (avoid infinite loops)
            if (!deltaSourceNode->locked)
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
                  // set restore flag for source node and restore to temporary file
                  BLOCK_DO(deltaSourceNode->locked = TRUE,
                           deltaSourceNode->locked = FALSE,
                  {
                    error = restoreFile(&localStorageSpecifier,
                                        name,
                                        deltaSourceList,
                                        jobOptions,
                                        tmpFileName,
                                        &fragmentNode,
                                        CALLBACK(getPasswordConsole,NULL),
                                        NULL,  // pauseFlag
                                        NULL,  // requestedAbortFlag
                                        NULL   // logHandle
                                       );
                    if (error == ERROR_NONE)
                    {
                      deltaSourceHandle->name = deltaSourceNode->storageName;
                    }
                  });

                  // delete local copy of storage file
                  doneLocalStorageArchive(&localStorageSpecifier);
                }
              }
            }

            // stop if complete
            if (   (size != SOURCE_SIZE_UNKNOWN)
                && FragmentList_isEntryComplete(&fragmentNode)
               )
            {
              break;
            }
          }
        }

        if (   (deltaSourceHandle->name != NULL)
            && (   (size == SOURCE_SIZE_UNKNOWN)
                || FragmentList_isEntryComplete(&fragmentNode)
               )
           )
        {
          // open temporary restored file
          error = File_open(&deltaSourceHandle->tmpFileHandle,tmpFileName,FILE_OPEN_READ);
          if (error == ERROR_NONE)
          {
            deltaSourceHandle->tmpFileName = tmpFileName;
            restoredFlag = TRUE;
          }
        }

        // free resources
        FragmentList_doneNode(&fragmentNode);
        if (!restoredFlag)
        {
          File_delete(tmpFileName,FALSE);
          String_delete(tmpFileName);
        }
      }
      else
      {
        String_delete(tmpFileName);
      }
    }
  }

  // check if source can be restored from orginal archive in file system
  if (!restoredFlag)
  {
    if (   !String_isEmpty(sourceStorageName)
        && (Storage_parseName(&storageSpecifier,sourceStorageName) == ERROR_NONE)
        && Storage_isInFileSystem(&storageSpecifier)
       )
    {
      if (Archive_isArchiveFile(storageSpecifier.archiveName))
      {
        // create temporary file as delta source
        tmpFileName = String_new();
        error = File_getTmpFileName(tmpFileName,NULL,tmpDirectory);
        if (error == ERROR_NONE)
        {
          // restore to temporary file
          error = restoreFile(&storageSpecifier,
                              name,
                              deltaSourceList,
                              jobOptions,
                              tmpFileName,
                              NULL,  // fragmentNode
                              CALLBACK(getPasswordConsole,NULL),
                              NULL,  // pauseFlag
                              NULL,  // requestedAbortFlag
                              NULL   // logHandle
                             );
          if (error == ERROR_NONE)
          {
            // open temporary restored file
            error = File_open(&deltaSourceHandle->tmpFileHandle,tmpFileName,FILE_OPEN_READ);
            if (error == ERROR_NONE)
            {
              deltaSourceHandle->name        = sourceStorageName;
              deltaSourceHandle->tmpFileName = tmpFileName;
              restoredFlag = TRUE;
            }
          }

          // free resources
          if (!restoredFlag)
          {
            File_delete(tmpFileName,FALSE);
            String_delete(tmpFileName);
          }
        }
        else
        {
          String_delete(tmpFileName);
        }
      }
      else
      {
        // open local file as delta source
        error = File_open(&deltaSourceHandle->tmpFileHandle,storageSpecifier.archiveName,FILE_OPEN_READ);
        if (error == ERROR_NONE)
        {
          deltaSourceHandle->name = sourceStorageName;
          restoredFlag = TRUE;
        }
      }
    }
  }

  // check if source can be restored from original storage name
  if (!restoredFlag)
  {
    if (   !String_isEmpty(sourceStorageName)
        && (Storage_parseName(&storageSpecifier,sourceStorageName) == ERROR_NONE)
       )
    {
      // create temporary restore file as delta source
      tmpFileName = String_new();
      error = File_getTmpFileName(tmpFileName,NULL,tmpDirectory);
      if (error == ERROR_NONE)
      {
        // create local copy of storage file
        error = createLocalStorageArchive(&localStorageSpecifier,
                                          &storageSpecifier,
                                          jobOptions
                                         );
        if (error == ERROR_NONE)
        {
          // restore to temporary file
          error = restoreFile(&localStorageSpecifier,
                              name,
                              deltaSourceList,
                              jobOptions,
                              tmpFileName,
                              NULL,  // fragmentNode
                              CALLBACK(getPasswordConsole,NULL),
                              NULL,  // pauseFlag
                              NULL,  // requestedAbortFlag
                              NULL   // logHandle
                             );
          if (error == ERROR_NONE)
          {
            // open temporary restored file
            error = File_open(&deltaSourceHandle->tmpFileHandle,tmpFileName,FILE_OPEN_READ);
            if (error == ERROR_NONE)
            {
              deltaSourceHandle->name        = sourceStorageName;
              deltaSourceHandle->tmpFileName = tmpFileName;
              restoredFlag = TRUE;
            }
          }

          // delete local copy of storage file
          doneLocalStorageArchive(&localStorageSpecifier);
        }

        // free resources
        if (!restoredFlag)
        {
          File_delete(tmpFileName,FALSE);
          String_delete(tmpFileName);
        }
      }
      else
      {
        String_delete(tmpFileName);
      }
    }
  }

  // free resources
  Storage_doneSpecifier(&localStorageSpecifier);
  Storage_doneSpecifier(&storageSpecifier);

  if (restoredFlag)
  {
    return ERROR_NONE;
  }
  else if (error != ERROR_NONE)
  {
    return error;
  }
  else
  {
    return (sourceStorageName != NULL) ? ERRORX_(DELTA_SOURCE_NOT_FOUND,0,"%s",sourceStorageName) : ERROR_DELTA_SOURCE_NOT_FOUND;
  }
}

void DeltaSource_closeEntry(DeltaSourceHandle *deltaSourceHandle)
{
  assert(deltaSourceHandle != NULL);

  // close source file
  File_close(&deltaSourceHandle->tmpFileHandle);

  // delete temporary source file
  if (deltaSourceHandle->tmpFileName != NULL)
  {
    File_delete(deltaSourceHandle->tmpFileName,FALSE);
    String_delete(deltaSourceHandle->tmpFileName);
  }
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
