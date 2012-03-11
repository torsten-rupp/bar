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
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "patternlists.h"
#include "files.h"

#include "errors.h"
#include "archive.h"

#include "sources.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// file data buffer size
#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL SourceList sourceList;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

  // create local copy of storage file
LOCAL Errors createLocalStorageArchive(String       localStorageName,
                                       const String storageName,
                                       JobOptions   *jobOptions
                                      )
{
  Errors error;

  assert(localStorageName != NULL);
  assert(storageName != NULL);

  error = File_getTmpFileName(localStorageName,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    return error;
  }

  error = Storage_copy(storageName,
                       jobOptions,
                       NULL,//StorageRequestVolumeFunction storageRequestVolumeFunction,
                       NULL,//void                         *storageRequestVolumeUserData,
                       NULL,//StorageStatusInfoFunction    storageStatusInfoFunction,
                       NULL,//void                         *storageStatusInfoUserData,
                       localStorageName
                      );
  if (error != ERROR_NONE)
  {
    File_delete(localStorageName,FALSE);
    return error;
  }

  return ERROR_NONE;
}

LOCAL void deleteLocalStorageArchive(String localStorageName)
{
  File_delete(localStorageName,FALSE);
  String_delete(localStorageName);
}

LOCAL Errors restoreFile(const String                    archiveName,
                         const String                    name,
                         const PatternList               *sourcePatternList,
                         JobOptions                      *jobOptions,
                         const String                    destinationFileName,
                         ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                         void                            *archiveGetCryptPasswordUserData,
                         bool                            *pauseFlag,
                         bool                            *requestedAbortFlag
                        )
{
  byte              *buffer;
//  bool              abortFlag;
  Errors            error;
  ArchiveInfo       archiveInfo;
  Errors            failError;
  ArchiveEntryInfo  archiveEntryInfo;
  ArchiveEntryTypes archiveEntryType;

  assert(archiveName != NULL);
  assert(name != NULL);
  assert(jobOptions != NULL);
  assert(destinationFileName != NULL);

  // initialize variables

  // allocate resources
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // open archive
  error = Archive_open(&archiveInfo,
                       archiveName,
                       jobOptions,
                       archiveGetCryptPasswordFunction,
                       archiveGetCryptPasswordUserData
                      );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // read archive entries
  failError = ERROR_NONE;
  while (   ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
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
          error = Archive_readFileEntry(&archiveInfo,
                                        &archiveEntryInfo,
//???
//NULL,
//NULL,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL,
                                        fileName,
                                        NULL,
                                        NULL,
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
                         Errors_getText(error)
                        );
              (void)Archive_closeEntry(&archiveEntryInfo);
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
                         Errors_getText(error)
                        );
              File_close(&fileHandle);
              (void)Archive_closeEntry(&archiveEntryInfo);
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

              error = Archive_readData(&archiveEntryInfo,buffer,bufferLength);
              if (error != ERROR_NONE)
              {
                if (failError == ERROR_NONE) failError = error;
                break;
              }
              error = File_write(&fileHandle,buffer,bufferLength);
              if (error != ERROR_NONE)
              {
                printError("Cannot write file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Errors_getText(error)
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
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              continue;
            }
            else if ((requestedAbortFlag != NULL) && (*requestedAbortFlag))
            {
              File_close(&fileHandle);
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              continue;
            }

            // close file
            File_close(&fileHandle);
          }

          // close archive file, free resources
          (void)Archive_closeEntry(&archiveEntryInfo);

          // free resources
          String_delete(fileName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        {
          String     imageName;
          uint64     blockOffset,blockCount;
          FileHandle fileHandle;
          uint64     block;
          ulong      bufferBlockCount;
          ulong      bufferLength;

          // read image
          imageName = String_new();
          error = Archive_readImageEntry(&archiveInfo,
                                         &archiveEntryInfo,
//???
//NULL,
//NULL,
                                         NULL,
                                         NULL,
                                         NULL,
                                         NULL,
                                         imageName,
                                         NULL,
                                         NULL,
                                         &blockOffset,
                                         &blockCount
                                        );
          if (error != ERROR_NONE)
          {
            String_delete(imageName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          if (String_equals(name,imageName))
          {
//            abortFlag = !updateStatusInfo(&restoreInfo);

            // open file
            error = File_open(&fileHandle,destinationFileName,FILE_OPEN_WRITE);
            if (error != ERROR_NONE)
            {
              printError("Cannot create/write to file '%s' (error: %s)\n",
                         String_cString(destinationFileName),
                         Errors_getText(error)
                        );
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(imageName);
              failError = error;
              continue;
            }

            // seek to fragment position
            error = File_seek(&fileHandle,blockOffset);
            if (error != ERROR_NONE)
            {
              printError("Cannot write file '%s' (error: %s)\n",
                         String_cString(destinationFileName),
                         Errors_getText(error)
                        );
              File_close(&fileHandle);
              (void)Archive_closeEntry(&archiveEntryInfo);
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

//              assert(deviceInfo.blockSize > 0);
//              bufferBlockCount = MIN(blockCount-block,BUFFER_SIZE/deviceInfo.blockSize);
//???
bufferBlockCount = 0;

              error = Archive_readData(&archiveEntryInfo,buffer,0/*bufferLength ???bufferBlockCount*deviceInfo.blockSize*/);
              if (error != ERROR_NONE)
              {
                if (failError == ERROR_NONE) failError = error;
                break;
              }
              error = File_write(&fileHandle,buffer,bufferLength);
              if (error != ERROR_NONE)
              {
                printError("Cannot write file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Errors_getText(error)
                          );
                failError = error;
                break;
              }
//              abortFlag = !updateStatusInfo(&restoreInfo);

              block += (uint64)bufferBlockCount;
            }

            // close file
            File_close(&fileHandle);
          }

          // close archive file, free resources
          (void)Archive_closeEntry(&archiveEntryInfo);

          // free resources
          String_delete(imageName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        {
          StringList       fileNameList;
          uint64           fragmentOffset,fragmentSize;
          const StringNode *stringNode;
          String           fileName;
          FileHandle       fileHandle;
          uint64           length;
          ulong            bufferLength;

          // read hard link
          StringList_init(&fileNameList);
          error = Archive_readHardLinkEntry(&archiveInfo,
                                            &archiveEntryInfo,
//???
//NULL,
//NULL,
                                            NULL,
                                            NULL,
                                            NULL,
                                            NULL,
                                            &fileNameList,
                                            NULL,
                                            NULL,
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
                         Errors_getText(error)
                        );
              (void)Archive_closeEntry(&archiveEntryInfo);
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
                         Errors_getText(error)
                        );
              File_close(&fileHandle);
              (void)Archive_closeEntry(&archiveEntryInfo);
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

              error = Archive_readData(&archiveEntryInfo,buffer,bufferLength);
              if (error != ERROR_NONE)
              {
                failError = error;
                break;
              }
              error = File_write(&fileHandle,buffer,bufferLength);
              if (error != ERROR_NONE)
              {
                printError("Cannot write file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Errors_getText(error)
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
          }
          if (failError != ERROR_NONE)
          {
            (void)Archive_closeEntry(&archiveEntryInfo);
            StringList_done(&fileNameList);
            continue;
          }

          // close archive file, free resources
          (void)Archive_closeEntry(&archiveEntryInfo);

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
  (void)Archive_close(&archiveInfo);

  // free resources
  free(buffer);

  return failError;
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

  if (sourceNode->localStorageName != NULL)
  {
    deleteLocalStorageArchive(sourceNode->localStorageName);
  }
  String_delete(sourceNode->storageName);
}

/*---------------------------------------------------------------------*/

Errors Source_initAll(void)
{
  List_init(&sourceList);

  return ERROR_NONE;
}

void Source_doneAll(void)
{
  List_done(&sourceList,(ListNodeFreeFunction)freeSourceNode,NULL);
}

void Source_addSource(const String sourcePattern)
{
  JobOptions                 jobOptions;
  Errors                     error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  String                     fileName;
  SourceNode                 *sourceNode;

  // init options
  initJobOptions(&jobOptions);

  // open directory list
  error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                    sourcePattern,
                                    &jobOptions
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
        sourceNode = LIST_NEW_NODE(SourceNode);
        if (sourceNode == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }
        sourceNode->storageName         = String_duplicate(fileName);
        sourceNode->localStorageName    = NULL;
//          sourceNode->tmpLocalStorageFlag = FALSE;
        List_append(&sourceList,sourceNode);
      }
    }
    String_delete(fileName);
    Storage_closeDirectoryList(&storageDirectoryListHandle);
  }
  else
  {
    sourceNode = LIST_NEW_NODE(SourceNode);
    if (sourceNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    sourceNode->storageName         = String_duplicate(sourcePattern);
    sourceNode->localStorageName    = NULL;
//      sourceNode->tmpLocalStorageFlag = FALSE;
    List_append(&sourceList,sourceNode);
  }

  // free resources
  freeJobOptions(&jobOptions);
}

Errors Source_addSourceList(const PatternList *sourcePatternList)
{
  String      fileName;
  PatternNode *patternNode;
  Errors      error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  SourceNode *sourceNode;

  assert(sourcePatternList != NULL);

  fileName = String_new();
  PATTERNLIST_ITERATE(sourcePatternList,patternNode)
  {
    Source_addSource(patternNode->string);//,jobOptions);
  }
  String_delete(fileName);

  return ERROR_NONE;
}

Errors Source_openEntry(SourceHandle *sourceHandle,
                        const String sourceStorageName,
                        const String name
                       )
{
  JobOptions jobOptions;
  Errors     error;
  bool       restoredFlag;
  String     localStorageName;
  SourceNode *sourceNode;

  assert(sourceHandle != NULL);
  assert(name != NULL);

  // init options
  initJobOptions(&jobOptions);

  // create temporary restore file as delta source
  sourceHandle->tmpFileName = String_new();
  error = File_getTmpFileName(sourceHandle->tmpFileName,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    String_delete(sourceHandle->tmpFileName);
    freeJobOptions(&jobOptions);
    return error;
  }

  restoredFlag = FALSE;

  if (!restoredFlag)
  {
    // restore from given storage name
    if (sourceStorageName != NULL)
    {
      localStorageName = String_new();
      error = createLocalStorageArchive(localStorageName,
                                        sourceStorageName,
                                        &jobOptions
                                       );
      if (error != ERROR_NONE)
      {
        String_delete(localStorageName);
        String_delete(sourceHandle->tmpFileName);
        freeJobOptions(&jobOptions);
        return error;
      }

      // restore to temporary file
      error = restoreFile(localStorageName,
                          name,
                          NULL,
                          &jobOptions,
                          sourceHandle->tmpFileName,
                          inputCryptPassword,
                          NULL,
                          NULL,//bool                            *pauseFlag,
                          NULL//bool                            *requestedAbortFlag
                         );
      if (error == ERROR_NONE)
      {
        restoredFlag = TRUE;
      }
      else
      {
        File_delete(localStorageName,FALSE);
        String_delete(localStorageName);
        String_delete(sourceHandle->tmpFileName);
        freeJobOptions(&jobOptions);
        return error;
      }

      // delete local storage file
      File_delete(localStorageName,FALSE);
      String_delete(localStorageName);
    }
  }

  if (!restoredFlag)
  {
    // restore from source pattern list
    LIST_ITERATE(&sourceList,sourceNode)
    {
      if (sourceNode->localStorageName == NULL)
      {
        // create local copy of storage file
        localStorageName = String_new();
        error = createLocalStorageArchive(localStorageName,
                                          sourceNode->storageName,
                                          &jobOptions
                                        );
        if (error != ERROR_NONE)
        {
          String_delete(localStorageName);
          String_delete(sourceHandle->tmpFileName);
          freeJobOptions(&jobOptions);
          return error;
        }
        sourceNode->localStorageName = localStorageName;
      }
      assert(sourceNode->localStorageName != NULL);

      // restore to temporary file
      error = restoreFile(sourceNode->localStorageName,
                          name,
                          NULL,
                          &jobOptions,
                          sourceHandle->tmpFileName,
                          inputCryptPassword,
                          NULL,
                          NULL,//bool                            *pauseFlag,
                          NULL//bool                            *requestedAbortFlag
                         );
      if (error == ERROR_NONE)
      {
        sourceHandle->storageName = sourceNode->storageName;
        restoredFlag = TRUE;
        break;
      }
    }
  }

  if (!restoredFlag)
  {
    String_delete(sourceHandle->tmpFileName);
    freeJobOptions(&jobOptions);
    return ERRORX(DELTA_SOURCE_NOT_FOUND,0,String_cString(sourceStorageName));
  }

  // open temporary restored file
  error = File_open(&sourceHandle->tmpFileHandle,sourceHandle->tmpFileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    String_delete(sourceHandle->tmpFileName);
    freeJobOptions(&jobOptions);
    return error;
  }

  // free resources
  freeJobOptions(&jobOptions);

  return ERROR_NONE;
}

void Source_closeEntry(SourceHandle *sourceHandle)
{
  assert(sourceHandle != NULL);
  assert(sourceHandle->tmpFileName != NULL);

  File_close(&sourceHandle->tmpFileHandle);
  File_delete(sourceHandle->tmpFileName,FALSE);
  String_delete(sourceHandle->tmpFileName);
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

  error = File_seek(&sourceHandle->tmpFileHandle,offset);
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
