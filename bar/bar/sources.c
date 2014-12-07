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

#include "forward.h"         /* required for JobOptions. Do not include
                                bar.h, because of circular dependency
                                in JobOptions
                             */

#include "global.h"
#include "strings.h"
#include "patternlists.h"
#include "files.h"
#include "fragmentlists.h"

#include "errors.h"
#include "storage.h"
#include "archive.h"

#include "sources.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// file data buffer size
#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/
// source node
typedef struct SourceNode
{
  LIST_NODE_HEADER(struct SourceNode);

  String           storageName;          // storage archive name
  StorageSpecifier storageSpecifier;     // storage specifier
} SourceNode;

typedef struct
{
  LIST_HEADER(SourceNode);
  Semaphore passwordLock;
} SourceList;

/***************************** Variables *******************************/
LOCAL SourceList sourceList;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : addSourceNodes
* Purpose: add source nodes (matching names or name)
* Input  : storageName    - storage name
*          storagePattern - storage pattern
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors addSourceNodes(const String storageName, const Pattern *storagePattern)
{
  JobOptions                 jobOptions;
  StorageSpecifier           storageSpecifier;
  Errors                     error;
  StorageSpecifier           storageDirectorySpecifier;
  StorageDirectoryListHandle storageDirectoryListHandle;
  String                     fileName;
  SourceNode                 *sourceNode;

  // init variables
  initJobOptions(&jobOptions);

  // parse storage name
  Storage_initSpecifier(&storageSpecifier);
  error = Storage_parseName(&storageSpecifier,storageName);
  if (error != ERROR_NONE)
  {
    Storage_doneSpecifier(&storageSpecifier);
    doneJobOptions(&jobOptions);
    return error;
  }

  // get path storage specifier
  Storage_duplicateSpecifier(&storageDirectorySpecifier,&storageSpecifier);
  File_getFilePathName(storageDirectorySpecifier.fileName,storageSpecifier.fileName);

  // open directory list
  sourceNode = NULL;
  error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                    &storageDirectorySpecifier,
                                    &jobOptions,
                                    SERVER_CONNECTION_PRIORITY_LOW
                                   );
  if (error == ERROR_NONE)
  {
    // read directory
    fileName = String_new();
    while (!Storage_endOfDirectoryList(&storageDirectoryListHandle))
    {
      error = Storage_readDirectoryList(&storageDirectoryListHandle,
                                        fileName,
                                        NULL
                                       );
      if (error == ERROR_NONE)
      {
        // check pattern
        if (Pattern_match(storagePattern,fileName,PATTERN_MATCH_MODE_BEGIN))
        {
          // add file entry
          sourceNode = LIST_NEW_NODE(SourceNode);
          if (sourceNode == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          sourceNode->storageName = String_duplicate(fileName);
          Storage_duplicateSpecifier(&sourceNode->storageSpecifier,&storageSpecifier);
          String_set(sourceNode->storageSpecifier.fileName,fileName);
          List_append(&sourceList,sourceNode);
        }
      }
    }
    String_delete(fileName);
    Storage_closeDirectoryList(&storageDirectoryListHandle);
  }

  // add file entry directly if no matching entry found in directory
  if (sourceNode == NULL)
  {
    printWarning("No matching entry for delta source '%s' found",String_cString(Storage_getPrintableName(&storageSpecifier,NULL)));
    sourceNode = LIST_NEW_NODE(SourceNode);
    if (sourceNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    sourceNode->storageName = String_duplicate(storageName);
    Storage_duplicateSpecifier(&sourceNode->storageSpecifier,&storageSpecifier);
    List_append(&sourceList,sourceNode);
  }

  // free resources
  Storage_doneSpecifier(&storageDirectorySpecifier);
  Storage_doneSpecifier(&storageSpecifier);
  doneJobOptions(&jobOptions);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : freeSourceNode
* Purpose: free source node
* Input  : sourceNode - source node
*          userData   - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeSourceNode(SourceNode *sourceNode, void *userData)
{
  assert(sourceNode != NULL);

  UNUSED_VARIABLE(userData);

  Storage_doneSpecifier(&sourceNode->storageSpecifier);
  String_delete(sourceNode->storageName);
}

/***********************************************************************\
* Name   : createLocalStorageArchive
* Purpose: create local copy of storage file
* Input  : localFileName - local file name
*          storageName   - storage name
*          jobOptions    - job options
* Output : -
* Return : ERROR_NONE if local copy created, otherwise error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createLocalStorageArchive(String                 localFileName,
                                       const StorageSpecifier *storageSpecifier,
                                       const JobOptions       *jobOptions
                                      )
{
//  StorageSpecifier storageSpecifier;
  Errors           error;

  assert(localFileName != NULL);
  assert(storageSpecifier != NULL);

  // create temporary file
  error = File_getTmpFileName(localFileName,NULL,tmpDirectory);
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
                       localFileName
                      );
  if (error != ERROR_NONE)
  {
    File_delete(localFileName,FALSE);
    return error;
  }

  return ERROR_NONE;
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
* Input  : archiveName                      - archive file name
*          name                             - name of entry to restore
*          jobOptions                       - job options
*          destinationFileName              - destination file name
*          fragmentNode                     - fragment node (can be NULL)
*          archiveGetCryptPasswordFunction  - get password call back
*          archiveGetCryptPasswordUserData  - user data for get password
*                                             call back
*          pauseFlag                        - pause flag (can be NULL)
*          requestedAbortFlag               - request abort flag (can be
*                                             NULL)
* Output : -
* Return : ERROR_NONE if file restored, otherwise error code
* Notes  : -
\***********************************************************************/

LOCAL Errors restoreFile(const String                    storageName,
                         const String                    name,
                         const JobOptions                *jobOptions,
                         const String                    destinationFileName,
                         FragmentNode                    *fragmentNode,
                         ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                         void                            *archiveGetCryptPasswordUserData,
                         bool                            *pauseFlag,
                         bool                            *requestedAbortFlag
                        )
{
  bool              restoredFlag;
  StorageSpecifier  storageSpecifier;
  StorageHandle     storageHandle;
  byte              *buffer;
//  bool              abortFlag;
  Errors            error;
  ArchiveInfo       archiveInfo;
  Errors            failError;
  ArchiveEntryInfo  archiveEntryInfo;
  ArchiveEntryTypes archiveEntryType;

  assert(storageName != NULL);
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

  // parse storage name
  Storage_initSpecifier(&storageSpecifier);
  error = Storage_parseName(&storageSpecifier,storageName);
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize storage '%s' (error: %s)\n",
               String_cString(storageName),
               Error_getText(error)
              );
    Storage_doneSpecifier(&storageSpecifier);
    return error;
  }

  // init storage
  error = Storage_init(&storageHandle,
                       &storageSpecifier,
                       jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK(NULL,NULL),
                       CALLBACK(NULL,NULL)
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize storage '%s' (error: %s)!\n",
               String_cString(storageName),
               Error_getText(error)
              );
    Storage_doneSpecifier(&storageSpecifier);
    return error;
  }

  // open archive
  error = Archive_open(&archiveInfo,
                       &storageHandle,
                       &storageSpecifier,
                       jobOptions,
                       CALLBACK(archiveGetCryptPasswordFunction,archiveGetCryptPasswordUserData)
                      );
  if (error != ERROR_NONE)
  {
    (void)Storage_done(&storageHandle);
    Storage_doneSpecifier(&storageSpecifier);
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
                                        NULL,  // deltaSourceName
                                        NULL,  // deltaSourceSize
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
                                         NULL,  // deltaSourceName
                                         NULL,  // deltaSourceSize
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
                                            NULL,  // deltaSourceName
                                            NULL,  // deltaSourceSize
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
  Storage_doneSpecifier(&storageSpecifier);
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

Errors Source_initAll(void)
{
  List_init(&sourceList);
  Semaphore_init(&sourceList.passwordLock);

  return ERROR_NONE;
}

void Source_doneAll(void)
{
  Semaphore_done(&sourceList.passwordLock);
  List_done(&sourceList,(ListNodeFreeFunction)freeSourceNode,NULL);
}

Errors Source_addSource(const String storageName)
{
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    String    string;
  #endif /* PLATFORM_... */
  Pattern storagePattern;
  Errors  error;

  // init pattern
  #if   defined(PLATFORM_LINUX)
    error = Pattern_init(&storagePattern,
                         storageName,
                         PATTERN_TYPE_GLOB,
                         PATTERN_FLAG_NONE
                        );
  #elif defined(PLATFORM_WINDOWS)
    // escape all '\' by '\\'
    string = String_duplicate(storageName);
    String_replaceAllCString(string,STRING_BEGIN,"\\","\\\\");

    error = Pattern_init(&storagePattern,
                         string,
                         PATTERN_TYPE_GLOB,
                         PATTERN_FLAG_IGNORE_CASE
                        );

    // free resources
    String_delete(string);
  #endif /* PLATFORM_... */
  if (error != ERROR_NONE)
  {
    return error;
  }

  addSourceNodes(storageName,&storagePattern);

  // free resources
  Pattern_done(&storagePattern);

  return ERROR_NONE;
}

Errors Source_addSourceList(const PatternList *sourcePatternList)
{
  PatternNode *patternNode;
  Errors      error;

  assert(sourcePatternList != NULL);

  PATTERNLIST_ITERATE(sourcePatternList,patternNode)
  {
    error = addSourceNodes(patternNode->string,&patternNode->pattern);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  return ERROR_NONE;
}

Errors Source_openEntry(SourceHandle     *sourceHandle,
                        const String     sourceStorageName,
                        const String     name,
                        int64            size,
                        const JobOptions *jobOptions
                       )
{
  bool             restoredFlag;
  Errors           error;
  FragmentNode     fragmentNode;
  bool             semaphoreLock;
  SourceNode       *sourceNode;
  String           tmpFileName;
  StorageSpecifier storageSpecifier;
  String           localStorageName;

  assert(sourceHandle != NULL);
  assert(name != NULL);
  assert(jobOptions != NULL);

  // init variables
  sourceHandle->name        = NULL;
  sourceHandle->size        = 0LL;
  sourceHandle->tmpFileName = NULL;
  sourceHandle->baseOffset  = 0LL;

  restoredFlag = FALSE;
  error        = ERROR_UNKNOWN;

  // check if source can be restored from local files given by command option --delta-source
  if (!restoredFlag)
  {
    LIST_ITERATE(&sourceList,sourceNode)
    {
      if (sourceNode->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM)
      {
        if (!Archive_isArchiveFile(sourceNode->storageSpecifier.fileName))
        {
          // open local file as source
          error = File_open(&sourceHandle->tmpFileHandle,sourceNode->storageSpecifier.fileName,FILE_OPEN_READ);
          if (error == ERROR_NONE)
          {
            sourceHandle->name = sourceNode->storageName;
            restoredFlag = TRUE;
          }
        }
      }

      // stop if restored
      if (restoredFlag) break;
    }
  }

  // check if source can be restored from local archives given by command option --delta-source
  if (!restoredFlag)
  {
    // create temporary restore file as delta source
    tmpFileName = String_new();
    error = File_getTmpFileName(tmpFileName,NULL,tmpDirectory);
    if (error == ERROR_NONE)
    {
      // init variables
      FragmentList_initNode(&fragmentNode,name,size,NULL,0);

      SEMAPHORE_LOCKED_DO(semaphoreLock,&sourceList.passwordLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        // restore from local source list
        LIST_ITERATE(&sourceList,sourceNode)
        {
          if (sourceNode->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM)
          {
            if (Archive_isArchiveFile(sourceNode->storageSpecifier.fileName))
            {
              // restore to temporary file
              error = restoreFile(sourceNode->storageSpecifier.fileName,
                                  name,
                                  jobOptions,
                                  tmpFileName,
                                  &fragmentNode,
                                  inputCryptPassword,
                                  NULL,  // archiveGetCryptPasswordUserData
                                  NULL,  // pauseFlag
                                  NULL   // requestedAbortFlag
                                 );
              if (error == ERROR_NONE)
              {
                sourceHandle->name = sourceNode->storageName;
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

      if (   (sourceHandle->name != NULL)
          && (   (size == SOURCE_SIZE_UNKNOWN)
              || FragmentList_isEntryComplete(&fragmentNode)
             )
         )
      {
        // open temporary restored file
        error = File_open(&sourceHandle->tmpFileHandle,tmpFileName,FILE_OPEN_READ);
        if (error == ERROR_NONE)
        {
          sourceHandle->tmpFileName = tmpFileName;
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

  // check if source can be restored from storage names given by command option --delta-source
  if (!restoredFlag)
  {
    // create temporary restore file as delta source
    tmpFileName = String_new();
    error = File_getTmpFileName(tmpFileName,NULL,tmpDirectory);
    if (error == ERROR_NONE)
    {
      // init variables
      FragmentList_initNode(&fragmentNode,name,size,NULL,0);

      SEMAPHORE_LOCKED_DO(semaphoreLock,&sourceList.passwordLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        // restore from remove source list
        LIST_ITERATE(&sourceList,sourceNode)
        {
          if (sourceNode->storageSpecifier.type != STORAGE_TYPE_FILESYSTEM)
          {
            // create local copy of storage file
            localStorageName = String_new();
            error = createLocalStorageArchive(localStorageName,
                                              &sourceNode->storageSpecifier,
                                              jobOptions
                                            );
            if (error == ERROR_NONE)
            {
              // restore to temporary file
              error = restoreFile(localStorageName,
                                  name,
                                  jobOptions,
                                  tmpFileName,
                                  &fragmentNode,
                                  inputCryptPassword,
                                  NULL,  // archiveGetCryptPasswordUserData
                                  NULL,  // pauseFlag
                                  NULL   // requestedAbortFlag
                                 );
              if (error == ERROR_NONE)
              {
                sourceHandle->name = sourceNode->storageName;
              }

              // delete local storage file
              File_delete(localStorageName,FALSE);
            }

            // free resources
            String_delete(localStorageName);
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

      if (   (sourceHandle->name != NULL)
          && (   (size == SOURCE_SIZE_UNKNOWN)
              || FragmentList_isEntryComplete(&fragmentNode)
             )
         )
      {
        // open temporary restored file
        error = File_open(&sourceHandle->tmpFileHandle,tmpFileName,FILE_OPEN_READ);
        if (error == ERROR_NONE)
        {
          sourceHandle->tmpFileName = tmpFileName;
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

  // check if source can be restored from orginal archive in local file system
  if (!restoredFlag)
  {
    if (   (sourceStorageName != NULL)
        && File_isFile(sourceStorageName)
       )
    {
      if (Archive_isArchiveFile(sourceStorageName))
      {
        // create temporary restore file as delta source
        tmpFileName = String_new();
        error = File_getTmpFileName(tmpFileName,NULL,tmpDirectory);
        if (error == ERROR_NONE)
        {
          SEMAPHORE_LOCKED_DO(semaphoreLock,&sourceList.passwordLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
          {
            // restore to temporary file
            error = restoreFile(sourceStorageName,
                                name,
                                jobOptions,
                                tmpFileName,
                                NULL,  // fragmentNode
                                inputCryptPassword,
                                NULL,  // archiveGetCryptPasswordUserData
                                NULL,  // pauseFlag
                                NULL   // requestedAbortFlag
                               );
            if (error == ERROR_NONE)
            {
              // open temporary restored file
              error = File_open(&sourceHandle->tmpFileHandle,tmpFileName,FILE_OPEN_READ);
              if (error == ERROR_NONE)
              {
                sourceHandle->name        = sourceStorageName;
                sourceHandle->tmpFileName = tmpFileName;
                restoredFlag = TRUE;
              }
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
        error = File_open(&sourceHandle->tmpFileHandle,sourceStorageName,FILE_OPEN_READ);
        if (error == ERROR_NONE)
        {
          sourceHandle->name = sourceStorageName;
          restoredFlag = TRUE;
        }
      }
    }
  }

  // check if source can be restored from original storage name
  if (!restoredFlag)
  {
    if (sourceStorageName != NULL)
    {
      // parse storage name
      Storage_initSpecifier(&storageSpecifier);
      error = Storage_parseName(&storageSpecifier,sourceStorageName);
      if (error == ERROR_NONE)
      {
        // create local copy of storage file
        localStorageName = String_new();
        error = createLocalStorageArchive(localStorageName,
                                          &storageSpecifier,
                                          jobOptions
                                         );
        if (error == ERROR_NONE)
        {
          // create temporary restore file as delta source
          tmpFileName = String_new();
          error = File_getTmpFileName(tmpFileName,NULL,tmpDirectory);
          if (error == ERROR_NONE)
          {
            SEMAPHORE_LOCKED_DO(semaphoreLock,&sourceList.passwordLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
            {
              // restore to temporary file
              error = restoreFile(localStorageName,
                                  name,
                                  jobOptions,
                                  tmpFileName,
                                  NULL,  // fragmentNode
                                  inputCryptPassword,
                                  NULL,  // archiveGetCryptPasswordUserData
                                  NULL,  // pauseFlag
                                  NULL   // requestedAbortFlag
                                 );
              if (error == ERROR_NONE)
              {
                // open temporary restored file
                error = File_open(&sourceHandle->tmpFileHandle,tmpFileName,FILE_OPEN_READ);
                if (error == ERROR_NONE)
                {
                  sourceHandle->name        = sourceStorageName;
                  sourceHandle->tmpFileName = tmpFileName;
                  restoredFlag = TRUE;
                }
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

          // delete local storage file
          File_delete(localStorageName,FALSE);
        }
        String_delete(localStorageName);
      }
      Storage_doneSpecifier(&storageSpecifier);
    }
  }

  if (restoredFlag)
  {
    return ERROR_NONE;
  }
  else
  {
    return (error != ERROR_NONE) ? error : ERRORX_(DELTA_SOURCE_NOT_FOUND,0,String_cString(sourceStorageName));
  }
}

void Source_closeEntry(SourceHandle *sourceHandle)
{
  assert(sourceHandle != NULL);

  // close source file
  File_close(&sourceHandle->tmpFileHandle);

  // delete temporary source file
  if (sourceHandle->tmpFileName != NULL)
  {
    File_delete(sourceHandle->tmpFileName,FALSE);
    String_delete(sourceHandle->tmpFileName);
  }
}

String Source_getName(SourceHandle *sourceHandle)
{
  assert(sourceHandle != NULL);

  return sourceHandle->name;
}

uint64 Source_getSize(SourceHandle *sourceHandle)
{
  assert(sourceHandle != NULL);

  return File_getSize(&sourceHandle->tmpFileHandle);
}

void Source_setBaseOffset(SourceHandle *sourceHandle, uint64 offset)
{
  assert(sourceHandle != NULL);

  sourceHandle->baseOffset = offset;
}

Errors Source_getEntryDataBlock(SourceHandle *sourceHandle,
                                void         *buffer,
                                uint64       offset,
                                ulong        length,
                                ulong        *bytesRead
                               )
{
  Errors          error;

  assert(sourceHandle != NULL);
  assert(buffer != NULL);
  assert(bytesRead != NULL);
//fprintf(stderr,"%s, %d: Source_getEntryDataBlock %d\n",__FILE__,__LINE__,offset);

  error = File_seek(&sourceHandle->tmpFileHandle,sourceHandle->baseOffset+offset);
  if (error != ERROR_NONE)
  {
    return error;
  }

  error = File_read(&sourceHandle->tmpFileHandle,buffer,length,bytesRead);
  if (error != ERROR_NONE)
  {
    return error;
  }
//fprintf(stderr,"%s,%d: read source %d\n",__FILE__,__LINE__,*bytesRead);

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
