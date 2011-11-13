/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/compress.h,v $
* $Revision: 1.3 $
* $Author: torsten $
* Contents: Backup ARchiver source functions
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <zlib.h>
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

/* file data buffer size */
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
//    if (sourceNode->tmpLocalStorageFlag)
//    {
      File_delete(sourceNode->localStorageName,FALSE);
//    }
    String_delete(sourceNode->localStorageName);
  }
  String_delete(sourceNode->storageName);
}

LOCAL Errors restoreFile(const String                    archiveName,
                         const String                    name,
                         const JobOptions                *jobOptions,
                         const String                    destinationFileName,
                         ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                         void                            *archiveGetCryptPasswordUserData,
                         bool                            *pauseFlag,
                         bool                            *requestedAbortFlag
                        )
{
  byte              *buffer;
//  String            printableArchiveName;
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

  /* initialize variables */

  /* allocate resources */
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
//  printableArchiveName = String_new();

  /* open archive */
  error = Archive_open(&archiveInfo,
                       archiveName,
                       jobOptions,
                       archiveGetCryptPasswordFunction,
                       archiveGetCryptPasswordUserData
                      );
  if (error != ERROR_NONE)
  {
#if 0
    printError("Cannot open archive file '%s' (error: %s)!\n",
               String_cString(printableArchiveName),
               Errors_getText(error)
              );
#endif /* 0 */
    return error;
  }
//  abortFlag = !updateStatusInfo(&restoreInfo);

  /* read archive entries */
  failError = ERROR_NONE;
  while (   TRUE//((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
         && !Archive_eof(&archiveInfo,TRUE)
         && (failError == ERROR_NONE)
        )
  {
#if 0
    /* pause */
    while ((restoreInfo.pauseFlag != NULL) && (*restoreInfo.pauseFlag))
    {
      Misc_udelay(500*1000);
    }
#endif /* 0 */

    /* get next archive entry type */
    error = Archive_getNextArchiveEntryType(&archiveInfo,
                                            &archiveEntryType,
                                            TRUE
                                           );
    if (error != ERROR_NONE)
    {
#if 0
      printError("Cannot read next entry in archive '%s' (error: %s)!\n",
                 String_cString(printableArchiveName),
                 Errors_getText(error)
                );
#endif /* 0 */
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
          ulong      n;

          /* read file */
          fileName = String_new();
          error = Archive_readFileEntry(&archiveInfo,
                                        &archiveEntryInfo,
//???
NULL,
NULL,
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
#if 0
            printError("Cannot read 'file' content of archive '%s' (error: %s)!\n",
                       String_cString(printableArchiveName),
                       Errors_getText(error)
                      );
#endif /* 0 */
            String_delete(fileName);
            if (failError == ERROR_NONE) failError = error;
            continue;
          }

          if (String_equals(name,fileName))
          {
//            abortFlag = !updateStatusInfo(&restoreInfo);

            if (!jobOptions->dryRunFlag)
            {
              /* open file */
              error = File_open(&fileHandle,destinationFileName,FILE_OPEN_WRITE);
              if (error != ERROR_NONE)
              {
                printError("Cannot create/write to file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Errors_getText(error)
                          );
                String_delete(destinationFileName);
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag)
                {
                  failError = error;
                }
                continue;
              }

              /* seek to fragment position */
              error = File_seek(&fileHandle,fragmentOffset);
              if (error != ERROR_NONE)
              {
                printError("Cannot write file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Errors_getText(error)
                          );
                File_close(&fileHandle);
                String_delete(destinationFileName);
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag)
                {
                  failError = error;
                }
                continue;
              }
            }

            /* write file data */
            length = 0;
            while (   TRUE//((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
                   && (length < fragmentSize)
                  )
            {
#if 0
              /* pause */
              while ((restoreInfo.pauseFlag != NULL) && (*restoreInfo.pauseFlag))
              {
                Misc_udelay(500*1000);
              }
#endif /* 0 */

              n = MIN(fragmentSize-length,BUFFER_SIZE);

              error = Archive_readData(&archiveEntryInfo,buffer,n);
              if (error != ERROR_NONE)
              {
#if 0
                printError("Cannot read content of archive '%s' (error: %s)!\n",
                           String_cString(printableArchiveName),
                           Errors_getText(error)
                          );
#endif /* 0 */
                if (failError == ERROR_NONE) failError = error;
                break;
              }
              if (!jobOptions->dryRunFlag)
              {
                error = File_write(&fileHandle,buffer,n);
                if (error != ERROR_NONE)
                {
                  printError("Cannot write file '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Errors_getText(error)
                            );
                  if (jobOptions->stopOnErrorFlag)
                  {
                    failError = error;
                  }
                  break;
                }
              }
//              abortFlag = !updateStatusInfo(&restoreInfo);

              length += n;
            }
            if      (failError != ERROR_NONE)
            {
              if (!jobOptions->dryRunFlag)
              {
                File_close(&fileHandle);
              }
              String_delete(destinationFileName);
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              continue;
            }
#if 0
            else if ((restoreInfo.requestedAbortFlag != NULL) && (*restoreInfo.requestedAbortFlag))
            {
              printInfo(2,"ABORTED\n");
              if (!jobOptions->dryRunFlag)
              {
                File_close(&fileHandle);
              }
              String_delete(destinationFileName);
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              continue;
            }
#endif /* 0 */

            /* close file */
            if (!jobOptions->dryRunFlag)
            {
              File_close(&fileHandle);
            }
          }

          /* close archive file, free resources */
          (void)Archive_closeEntry(&archiveEntryInfo);

          /* free resources */
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
          ulong      n;

          /* read image */
          imageName = String_new();
          error = Archive_readImageEntry(&archiveInfo,
                                         &archiveEntryInfo,
//???
NULL,
NULL,
                                         NULL,
                                         NULL,
                                         NULL,
                                         NULL,
                                         imageName,
                                         NULL,
                                         &blockOffset,
                                         &blockCount
                                        );
          if (error != ERROR_NONE)
          {
#if 0
            printError("Cannot read 'image' content of archive '%s' (error: %s)!\n",
                       String_cString(printableArchiveName),
                       Errors_getText(error)
                      );
#endif /* 0 */
            String_delete(imageName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          if (String_equals(name,imageName))
          {
//            abortFlag = !updateStatusInfo(&restoreInfo);

            /* open file */
            if (!jobOptions->dryRunFlag)
            {
              error = File_open(&fileHandle,destinationFileName,FILE_OPEN_WRITE);
              if (error != ERROR_NONE)
              {
                printError("Cannot create/write to file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Errors_getText(error)
                          );
                String_delete(destinationFileName);
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(imageName);
                if (jobOptions->stopOnErrorFlag)
                {
                  failError = error;
                }
                continue;
              }

              /* seek to fragment position */
              error = File_seek(&fileHandle,blockOffset);
              if (error != ERROR_NONE)
              {
                printError("Cannot write file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Errors_getText(error)
                          );
                File_close(&fileHandle);
                String_delete(destinationFileName);
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(imageName);
                if (jobOptions->stopOnErrorFlag)
                {
                  failError = error;
                }
                continue;
              }
            }

            /* write image data to file */
            block = 0;
            while (   TRUE//((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
                   && (block < blockCount)
                  )
            {
#if 0
              /* pause */
              while ((restoreInfo.pauseFlag != NULL) && (*restoreInfo.pauseFlag))
              {
                Misc_udelay(500*1000);
              }
#endif

//              assert(deviceInfo.blockSize > 0);
//              bufferBlockCount = MIN(blockCount-block,BUFFER_SIZE/deviceInfo.blockSize);
//???
bufferBlockCount = 0;

              error = Archive_readData(&archiveEntryInfo,buffer,0/*???bufferBlockCount*deviceInfo.blockSize*/);
              if (error != ERROR_NONE)
              {
#if 0
                printError("Cannot read content of archive '%s' (error: %s)!\n",
                           String_cString(printableArchiveName),
                           Errors_getText(error)
                          );
#endif /* 0 */
                if (failError == ERROR_NONE) failError = error;
                break;
              }
              if (!jobOptions->dryRunFlag)
              {
                error = File_write(&fileHandle,buffer,n);
                if (error != ERROR_NONE)
                {
                  printError("Cannot write file '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Errors_getText(error)
                            );
                  if (jobOptions->stopOnErrorFlag)
                  {
                    failError = error;
                  }
                  break;
                }
              }
//              abortFlag = !updateStatusInfo(&restoreInfo);

              block += (uint64)bufferBlockCount;
            }

            /* close file */
            if (!jobOptions->dryRunFlag)
            {
              File_close(&fileHandle);
            }
          }

          /* close archive file, free resources */
          (void)Archive_closeEntry(&archiveEntryInfo);

          /* free resources */
          String_delete(imageName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        break;
      case ARCHIVE_ENTRY_TYPE_LINK:
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        {
          StringList       fileNameList;
          uint64           fragmentOffset,fragmentSize;
          const StringNode *stringNode;
          String           fileName;
          FileHandle       fileHandle;
          uint64           length;
          ulong            n;

          /* read hard link */
          StringList_init(&fileNameList);
          error = Archive_readHardLinkEntry(&archiveInfo,
                                            &archiveEntryInfo,
//???
NULL,
NULL,
                                            NULL,
                                            NULL,
                                            NULL,
                                            NULL,
                                            &fileNameList,
                                            NULL,
                                            &fragmentOffset,
                                            &fragmentSize
                                           );
          if (error != ERROR_NONE)
          {
#if 0
            printError("Cannot read 'hard link' content of archive '%s' (error: %s)!\n",
                       String_cString(printableArchiveName),
                       Errors_getText(error)
                      );
#endif /* 0 */
            StringList_done(&fileNameList);
            if (failError == ERROR_NONE) failError = error;
            continue;
          }

          if (StringList_contain(&fileNameList,name))
          {
//              abortFlag = !updateStatusInfo(&restoreInfo);

            if (!jobOptions->dryRunFlag)
            {
              /* open file */
              error = File_open(&fileHandle,destinationFileName,FILE_OPEN_WRITE);
              if (error != ERROR_NONE)
              {
                printError("Cannot create/write to file '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Errors_getText(error)
                          );
                (void)Archive_closeEntry(&archiveEntryInfo);
                StringList_done(&fileNameList);
                if (jobOptions->stopOnErrorFlag)
                {
                  failError = error;
                }
                continue;
              }

              /* seek to fragment position */
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
                if (jobOptions->stopOnErrorFlag)
                {
                  failError = error;
                }
                continue;
              }
            }

            /* write file data */
            length = 0;
            while (   TRUE//((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
                   && (length < fragmentSize)
                  )
            {
#if 0
              /* pause */
              while ((restoreInfo.pauseFlag != NULL) && (*restoreInfo.pauseFlag))
              {
                Misc_udelay(500*1000);
              }
#endif /* 0 */

              n = MIN(fragmentSize-length,BUFFER_SIZE);

              error = Archive_readData(&archiveEntryInfo,buffer,n);
              if (error != ERROR_NONE)
              {
#if 0
                printError("Cannot read content of archive '%s' (error: %s)!\n",
                           String_cString(printableArchiveName),
                           Errors_getText(error)
                          );
#endif /* 0 */
                failError = error;
                break;
              }
              if (!jobOptions->dryRunFlag)
              {
                error = File_write(&fileHandle,buffer,n);
                if (error != ERROR_NONE)
                {
                  printError("Cannot write file '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Errors_getText(error)
                            );
                  if (jobOptions->stopOnErrorFlag)
                  {
                    failError = error;
                  }
                  break;
                }
              }
//                  abortFlag = !updateStatusInfo(&restoreInfo);

              length += n;
            }
            if      (failError != ERROR_NONE)
            {
              if (!jobOptions->dryRunFlag)
              {
                File_close(&fileHandle);
                break;
              }
            }
#if 0
            else if ((restoreInfo.requestedAbortFlag != NULL) && (*restoreInfo.requestedAbortFlag))
            {
              printInfo(2,"ABORTED\n");
              File_close(&fileHandle);
              break;
            }
#endif /* 0 */

            /* close file */
            if (!jobOptions->dryRunFlag)
            {
              File_close(&fileHandle);
            }
          }
          if (failError != ERROR_NONE)
          {
            (void)Archive_closeEntry(&archiveEntryInfo);
            StringList_done(&fileNameList);
            continue;
          }

          /* close archive file, free resources */
          (void)Archive_closeEntry(&archiveEntryInfo);

          /* free resources */
          StringList_done(&fileNameList);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
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

  /* free resources */
//  String_delete(printableArchiveName);
  free(buffer);

#if 0
  if ((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
  {
    return restoreInfo.failError;
  }
  else
  {
    return ERROR_ABORTED;
  }
#endif /* 0 */
  return failError;
}

/*---------------------------------------------------------------------*/

Errors Source_init(SourceInfo        *sourceInfo,
                   const PatternList *sourcePatternList,
                   JobOptions        *jobOptions
                  )
{
  String      fileName;
  PatternNode *patternNode;
  Errors      error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  SourceNode *sourceNode;

  assert(sourceInfo != NULL);
  assert(sourcePatternList != NULL);

  List_init(&sourceInfo->sourceList);
  sourceInfo->jobOptions = jobOptions;

  fileName = String_new();
  PATTERNLIST_ITERATE(sourcePatternList,patternNode)
  {
    error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                      patternNode->string,
                                      jobOptions
                                     );
    if (error == ERROR_NONE)
    {
      // read directory
      while (!Storage_endOfDirectoryList(&storageDirectoryListHandle))
      {
        error = Storage_readDirectoryList(&storageDirectoryListHandle,
                                          fileName,
                                          NULL
                                         );
        if (   (error == ERROR_NONE)
            && Pattern_match(&patternNode->pattern,fileName,PATTERN_MATCH_MODE_EXACT)
           )
        {
          sourceNode = LIST_NEW_NODE(SourceNode);
          if (sourceNode == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          sourceNode->storageName         = String_duplicate(fileName);
          sourceNode->localStorageName    = NULL;
//          sourceNode->tmpLocalStorageFlag = FALSE;
          List_append(&sourceInfo->sourceList,sourceNode);
        }
      }
      Storage_closeDirectoryList(&storageDirectoryListHandle);
    }
    else
    {
      sourceNode = LIST_NEW_NODE(SourceNode);
      if (sourceNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      sourceNode->storageName         = String_duplicate(patternNode->string);
      sourceNode->localStorageName    = NULL;
//      sourceNode->tmpLocalStorageFlag = FALSE;
      List_append(&sourceInfo->sourceList,sourceNode);
    }
  }
  String_delete(fileName);

  return ERROR_NONE;
}

void Source_done(SourceInfo *sourceInfo)
{
  assert(sourceInfo != NULL);

  List_done(&sourceInfo->sourceList,(ListNodeFreeFunction)freeSourceNode,NULL);
}

Errors Source_openEntry(SourceEntryInfo *sourceEntryInfo,
                        SourceInfo      *sourceInfo,
                        const String    name
                       )
{
  Errors     error;
  SourceNode *sourceNode;
  StringNode *stringNode;
  String     localStorageName;

  assert(sourceEntryInfo != NULL);
  assert(sourceInfo != NULL);
  assert(name != NULL);

  // create temporary source file
  sourceEntryInfo->tmpFileName = String_new();
  error = File_getTmpFileName(sourceEntryInfo->tmpFileName,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    String_delete(sourceEntryInfo->tmpFileName);
    return error;
  }

  // restore into temporary file
  LIST_ITERATE(&sourceInfo->sourceList,sourceNode)
  {
    // create local copy of storage archive
    if (sourceNode->localStorageName == NULL)
    {
      // create temporary file
      localStorageName = String_new();
      error = File_getTmpFileName(localStorageName,NULL,tmpDirectory);
      if (error != ERROR_NONE)
      {
        String_delete(localStorageName);
        String_delete(sourceEntryInfo->tmpFileName);
        return error;
      }

      // copy storage to local file
      error = Storage_copy(sourceNode->storageName,
                           sourceInfo->jobOptions,
                           NULL,//StorageRequestVolumeFunction storageRequestVolumeFunction,
                           NULL,//void                         *storageRequestVolumeUserData,
                           NULL,//StorageStatusInfoFunction    storageStatusInfoFunction,
                           NULL,//void                         *storageStatusInfoUserData,
                           localStorageName
                          );
      if (error != ERROR_NONE)
      {
        File_delete(localStorageName,FALSE);
        String_delete(localStorageName);
        String_delete(sourceEntryInfo->tmpFileName);
        return error;
      }

      // store local storage name
      sourceNode->localStorageName = localStorageName;
    }

    // restore to temporary file
    assert(sourceNode->localStorageName != NULL);
    error = restoreFile(sourceNode->localStorageName,
                        name,
                        sourceInfo->jobOptions,
                        sourceEntryInfo->tmpFileName,
                        inputCryptPassword,
                        NULL,
                        NULL,//bool                            *pauseFlag,
                        NULL//bool                            *requestedAbortFlag
                       );
    if (error != ERROR_NONE)
    {
      String_delete(sourceEntryInfo->tmpFileName);
      return error;
    }

    // store source node
    sourceEntryInfo->sourceNode = sourceNode;
  }

  // open temporary restored file
  error = File_open(&sourceEntryInfo->tmpFileHandle,sourceEntryInfo->tmpFileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    String_delete(sourceEntryInfo->tmpFileName);
    return error;
  }

  return ERROR_NONE;
}

void Source_closeEntry(SourceEntryInfo *sourceEntryInfo)
{
  assert(sourceEntryInfo != NULL);
  assert(sourceEntryInfo->tmpFileName != NULL);

  File_close(&sourceEntryInfo->tmpFileHandle);
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
//  File_delete(sourceEntryInfo->tmpFileName,FALSE);
  String_delete(sourceEntryInfo->tmpFileName);
}

Errors Source_getEntryDataBlock(SourceEntryInfo *sourceEntryInfo,
                                void            *buffer,
                                uint64          offset,
                                ulong           length
                               )
{
  assert(sourceEntryInfo != NULL);
  assert(buffer != NULL);
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);

  return ERROR_STILL_NOT_IMPLEMENTED;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
