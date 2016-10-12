/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive test functions
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
#include "strings.h"
#include "stringlists.h"
#include "msgqueues.h"

#include "errors.h"
#include "patterns.h"
#include "entrylists.h"
#include "patternlists.h"
#include "deltasourcelists.h"
#include "files.h"
#include "archive.h"
#include "fragmentlists.h"

#include "commands_test.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// file data buffer size
#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/
// test info
typedef struct
{
  StorageSpecifier            *storageSpecifier;                  // storage specifier structure
  ConstString                 jobUUID;                            // unique job id to store or NULL
  ConstString                 scheduleUUID;                       // unique schedule id to store or NULL
  const EntryList             *includeEntryList;                  // list of included entries
  const PatternList           *excludePatternList;                // list of exclude patterns
  const PatternList           *compressExcludePatternList;        // exclude compression pattern list
  const DeltaSourceList       *deltaSourceList;                   // delta sources
  const JobOptions            *jobOptions;
  ArchiveTypes                archiveType;                        // archive type to create
  ConstString                 scheduleTitle;                      // schedule title or NULL
  ConstString                 scheduleCustomText;                 // schedule custom text or NULL
  bool                        *pauseCreateFlag;                   // TRUE for pause creation
  bool                        *pauseStorageFlag;                  // TRUE for pause storage
  bool                        *requestedAbortFlag;                // TRUE to abort create
  LogHandle                   *logHandle;                         // log handle

  bool                        partialFlag;                        // TRUE for create incremental/differential archive
  bool                        storeIncrementalFileInfoFlag;       // TRUE to store incremental file data
  StorageHandle               storageHandle;                      // storage handle
  time_t                      startTime;                          // start time [ms] (unix time)

  MsgQueue                    entryMsgQueue;                      // queue with entries to store

  ArchiveInfo                 archiveInfo;

  bool                        collectorSumThreadExitedFlag;       // TRUE iff collector sum thread exited

  MsgQueue                    storageMsgQueue;                    // queue with waiting storage files
  Semaphore                   storageInfoLock;                    // lock semaphore for storage info
  struct
  {
    uint                      count;                              // number of current storage files
    uint64                    bytes;                              // number of bytes in current storage files
  }                           storageInfo;
  bool                        storageThreadExitFlag;
  StringList                  storageFileList;                    // list with stored storage files

  Errors                      failError;                          // failure error

//  CreateStatusInfoFunction    statusInfoFunction;                 // status info call back
//  void                        *statusInfoUserData;                // user data for status info call back
//  CreateStatusInfo            statusInfo;                         // status info
  Semaphore                   statusInfoLock;                     // status info lock
  Semaphore                   statusInfoNameLock;                 // status info name lock
} TestInfo;

// entry message, send from collector thread -> main
typedef struct
{
  EntryTypes entryType;
  FileTypes  fileType;
  String     name;                                                // file/image/directory/link/special name
  StringList nameList;                                            // list of hard link names
} EntryMsg;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

//TODO WIP
#if 0
/***********************************************************************\
* Name   : testThreadCode
* Purpose: test worker thread
* Input  : testInfo - test info structure
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void testThreadCode(TestInfo *testInfo)
{
  byte              *buffer;
  EntryMsg entryMsg;
  StorageHandle     storageHandle;
  Errors            failError;
  Errors            error;
  ArchiveInfo       archiveInfo;
  ArchiveEntryInfo  archiveEntryInfo;
  ArchiveEntryTypes archiveEntryType;

//  assert(storageSpecifier != NULL);
//  assert(includeEntryList != NULL);
//  assert(excludePatternList != NULL);
//  assert(jobOptions != NULL);
//  assert(fragmentList != NULL);

  // init variables
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // test entries
  while (   (testInfo->failError == ERROR_NONE)
//         && !isAborted(testInfo)
         && MsgQueue_get(&testInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER)
        )
  {
  }

#if 0
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
    free(buffer);
    return error;
  }

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
    (void)Storage_done(&storageHandle);
    free(buffer);
    return error;
  }

  // read archive entries
  printInfo(0,
            "Test storage '%s'%s",
            Storage_getPrintableNameCString(storageSpecifier,archiveName),
            !isPrintInfo(1) ? "..." : ":\n"
           );
  failError = ERROR_NONE;
  while (!Archive_eof(&archiveInfo,FALSE))
  {
    // get next archive entry type
    error = Archive_getNextArchiveEntryType(&archiveInfo,
                                            &archiveEntryType,
                                            FALSE
                                           );
    if (error != ERROR_NONE)
    {
      printError("Cannot read next entry in archive '%s' (error: %s)!\n",
                 Storage_getPrintableNameCString(storageSpecifier,archiveName),
                 Error_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      break;
    }

    switch (archiveEntryType)
    {
      case ARCHIVE_ENTRY_TYPE_FILE:
        {
          String       fileName;
          FileInfo     fileInfo;
          uint64       fragmentOffset,fragmentSize;
          FragmentNode *fragmentNode;
          uint64       length;
          ulong        n;

          // read file
          fileName = String_new();
          error = Archive_readFileEntry(&archiveEntryInfo,
                                        &archiveInfo,
                                        NULL,  // deltaCompressAlgorithm
                                        NULL,  // byteCompressAlgorithm
                                        NULL,  // cryptAlgorithm
                                        NULL,  // cryptType
                                        fileName,
                                        &fileInfo,
                                        NULL,  // fileExtendedAttributeList
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
            String_delete(fileName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
              && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
             )
          {
            printInfo(1,"  Test file '%s'...",String_cString(fileName));

            if (!jobOptions->noFragmentsCheckFlag)
            {
              // get file fragment list
              fragmentNode = FragmentList_find(fragmentList,fileName);
              if (fragmentNode == NULL)
              {
                fragmentNode = FragmentList_add(fragmentList,fileName,fileInfo.size,NULL,0);
              }
              assert(fragmentNode != NULL);
            }
            else
            {
              fragmentNode = NULL;
            }
//FragmentList_print(fragmentNode,String_cString(fileName));

            // read file content
            length = 0LL;
            while (length < fragmentSize)
            {
              n = (ulong)MIN(fragmentSize-length,BUFFER_SIZE);

              // read archive file
              error = Archive_readData(&archiveEntryInfo,buffer,n);
              if (error != ERROR_NONE)
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot read content of archive '%s' (error: %s)!\n",
                           Storage_getPrintableNameCString(storageSpecifier,archiveName),
                           Error_getText(error)
                          );
                break;
              }

              length += (uint64)n;

              printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
            }
            if (error != ERROR_NONE)
            {
              if (failError == ERROR_NONE) failError = error;
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              continue;
            }
            printInfo(2,"    \b\b\b\b");

            if (fragmentNode != NULL)
            {
              // add fragment to file fragment list
              FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);

              // discard fragment list if file is complete
              if (FragmentList_isEntryComplete(fragmentNode))
              {
                FragmentList_discard(fragmentList,fragmentNode);
              }
            }

            /* check if all data read.
               Note: it is not possible to check if all data is read when
               compression is used. The decompressor may not be at the end
               of a compressed data chunk even compressed data is _not_
               corrupt.
            */
            if (   !Compress_isCompressed(archiveEntryInfo.file.deltaCompressAlgorithm)
                && !Compress_isCompressed(archiveEntryInfo.file.byteCompressAlgorithm)
                && !Archive_eofData(&archiveEntryInfo)
               )
            {
              printInfo(1,"FAIL!\n");
              printError("unexpected data at end of file entry '%S'!\n",fileName);
              if (failError == ERROR_NONE)
              {
                failError = ERRORX_(CORRUPT_DATA,0,String_cString(fileName));
              }
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              break;
            }

            printInfo(1,"OK\n");
          }
          else
          {
            // skip
            printInfo(2,"  Test '%s'...skipped\n",String_cString(fileName));
          }

          // close archive file, free resources
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printError("closing 'file' entry fail (error: %s)!\n",
                       Error_getText(error)
                      );
            String_delete(fileName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          // free resources
          String_delete(fileName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        {
          String       deviceName;
          DeviceInfo   deviceInfo;
          uint64       blockOffset,blockCount;
          FragmentNode *fragmentNode;
          uint64       block;
          ulong        bufferBlockCount;

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
            if (failError == ERROR_NONE) failError = error;
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
            if (failError == ERROR_NONE)
            {
              failError = ERROR_INVALID_DEVICE_BLOCK_SIZE;
            }
            break;
          }
          assert(deviceInfo.blockSize > 0);

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,deviceName,PATTERN_MATCH_MODE_EXACT))
              && !PatternList_match(excludePatternList,deviceName,PATTERN_MATCH_MODE_EXACT)
             )
          {
            printInfo(1,"  Test image '%s'...",String_cString(deviceName));

            if (!jobOptions->noFragmentsCheckFlag)
            {
              // get file fragment node
              fragmentNode = FragmentList_find(fragmentList,deviceName);
              if (fragmentNode == NULL)
              {
                fragmentNode = FragmentList_add(fragmentList,deviceName,deviceInfo.size,NULL,0);
              }
//FragmentList_print(fragmentNode,String_cString(deviceName));
              assert(fragmentNode != NULL);
            }
            else
            {
              fragmentNode = NULL;
            }

            // read image content
            block = 0LL;
            while (block < blockCount)
            {
              bufferBlockCount = MIN(blockCount-block,BUFFER_SIZE/deviceInfo.blockSize);

              // read archive file
              error = Archive_readData(&archiveEntryInfo,buffer,bufferBlockCount*deviceInfo.blockSize);
              if (error != ERROR_NONE)
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot read content of archive '%s' (error: %s)!\n",
                           Storage_getPrintableNameCString(storageSpecifier,archiveName),
                           Error_getText(error)
                          );
                break;
              }

              block += (uint64)bufferBlockCount;

              printInfo(2,"%3d%%\b\b\b\b",(uint)((block*100LL)/blockCount));
            }
            if (error != ERROR_NONE)
            {
              if (failError == ERROR_NONE) failError = error;
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(deviceName);
              continue;
            }
            printInfo(2,"    \b\b\b\b");

            if (fragmentNode != NULL)
            {
              // add fragment to file fragment list
              FragmentList_addEntry(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize);

              // discard fragment list if file is complete
              if (FragmentList_isEntryComplete(fragmentNode))
              {
                FragmentList_discard(fragmentList,fragmentNode);
              }
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
              printInfo(1,"FAIL!\n");
              printError("unexpected data at end of image entry '%S'!\n",deviceName);
              if (failError == ERROR_NONE)
              {
                failError = ERRORX_(CORRUPT_DATA,0,String_cString(deviceName));
              }
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(deviceName);
              break;
            }

            printInfo(1,"OK\n");
          }
          else
          {
            // skip
            printInfo(2,"  Test '%s'...skipped\n",String_cString(deviceName));
          }

          // close archive file, free resources
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printError("closing 'image' entry fail (error: %s)!\n",
                       Error_getText(error)
                      );
            String_delete(deviceName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          // free resources
          String_delete(deviceName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        {
          String   directoryName;
          FileInfo fileInfo;

          // read directory
          directoryName = String_new();
          error = Archive_readDirectoryEntry(&archiveEntryInfo,
                                             &archiveInfo,
                                             NULL,  // cryptAlgorithm
                                             NULL,  // cryptType
                                             directoryName,
                                             &fileInfo,
                                             NULL   // fileExtendedAttributeList
                                            );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'directory' content of archive '%s' (error: %s)!\n",
                       Storage_getPrintableNameCString(storageSpecifier,archiveName),
                       Error_getText(error)
                      );
            String_delete(directoryName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
              && !PatternList_match(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
             )
          {
            printInfo(1,"  Test directory '%s'...",String_cString(directoryName));

            // check if all data read
            if (!Archive_eofData(&archiveEntryInfo))
            {
              printInfo(1,"FAIL!\n");
              printError("unexpected data at end of directory entry '%S'!\n",directoryName);
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(directoryName);
              if (failError == ERROR_NONE)
              {
                failError = ERRORX_(CORRUPT_DATA,0,String_cString(directoryName));
              }
              break;
            }

            printInfo(1,"OK\n");

            // free resources
          }
          else
          {
            // skip
            printInfo(2,"Test '%s'...skipped\n",String_cString(directoryName));
          }

          // close archive file
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printError("closing 'directory' entry fail (error: %s)!\n",
                       Error_getText(error)
                      );
            String_delete(directoryName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          // free resources
          String_delete(directoryName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_LINK:
        {
          String   linkName;
          String   fileName;
          FileInfo fileInfo;

          // read link
          linkName = String_new();
          fileName = String_new();
          error = Archive_readLinkEntry(&archiveEntryInfo,
                                        &archiveInfo,
                                        NULL,  // cryptAlgorithm
                                        NULL,  // cryptType
                                        linkName,
                                        fileName,
                                        &fileInfo,
                                        NULL   // fileExtendedAttributeList
                                       );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'link' content of archive '%s' (error: %s)!\n",
                       Storage_getPrintableNameCString(storageSpecifier,archiveName),
                       Error_getText(error)
                      );
            String_delete(fileName);
            String_delete(linkName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
              && !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
             )
          {
            printInfo(1,"  Test link '%s'...",String_cString(linkName));

            // check if all data read
            if (!Archive_eofData(&archiveEntryInfo))
            {
              printInfo(1,"FAIL!\n");
              printError("unexpected data at end of link entry '%S'!\n",linkName);
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              String_delete(linkName);
              if (failError == ERROR_NONE)
              {
                failError = ERRORX_(CORRUPT_DATA,0,String_cString(linkName));
              }
              break;
            }

            printInfo(1,"OK\n");

            // free resources
          }
          else
          {
            // skip
            printInfo(2,"  Test '%s'...skipped\n",String_cString(linkName));
          }

          // close archive file
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printError("closing 'link' entry fail (error: %s)!\n",
                       Error_getText(error)
                      );
            String_delete(fileName);
            String_delete(linkName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          // free resources
          String_delete(fileName);
          String_delete(linkName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        {
          StringList       fileNameList;
          FileInfo         fileInfo;
          uint64           fragmentOffset,fragmentSize;
          bool             testedDataFlag;
          const StringNode *stringNode;
          String           fileName;
          FragmentNode     *fragmentNode;
          uint64           length;
          ulong            n;

          // read hard linke
          StringList_init(&fileNameList);
          error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                            &archiveInfo,
                                            NULL,  // deltaCompressAlgorithm
                                            NULL,  // byteCompressAlgorithm
                                            NULL,  // cryptAlgorithm
                                            NULL,  // cryptType
                                            &fileNameList,
                                            &fileInfo,
                                            NULL,  // fileExtendedAttributeList
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
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          testedDataFlag = FALSE;
          STRINGLIST_ITERATE(&fileNameList,stringNode,fileName)
          {
            if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(1,"  Test hard link '%s'...",String_cString(fileName));

              if (!testedDataFlag && (failError == ERROR_NONE))
              {
                // read hard link data

                if (!jobOptions->noFragmentsCheckFlag)
                {
                  // get file fragment list
                  fragmentNode = FragmentList_find(fragmentList,fileName);
                  if (fragmentNode == NULL)
                  {
                    fragmentNode = FragmentList_add(fragmentList,fileName,fileInfo.size,NULL,0);
                  }
                  assert(fragmentNode != NULL);
//FragmentList_print(fragmentNode,String_cString(fileName));
                }
                else
                {
                  fragmentNode = NULL;
                }

                // read hard link content
                length = 0LL;
                while (length < fragmentSize)
                {
                  n = (ulong)MIN(fragmentSize-length,BUFFER_SIZE);

                  // read archive file
                  error = Archive_readData(&archiveEntryInfo,buffer,n);
                  if (error != ERROR_NONE)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot read content of archive '%s' (error: %s)!\n",
                               Storage_getPrintableNameCString(storageSpecifier,archiveName),
                               Error_getText(error)
                              );
                    break;
                  }

                  length += (uint64)n;

                  printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
                }
                if (error != ERROR_NONE)
                {
                  if (failError == ERROR_NONE) failError = error;
                  break;
                }
                printInfo(2,"    \b\b\b\b");

                if (fragmentNode != NULL)
                {
                  // add fragment to file fragment list
                  FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);

                  // discard fragment list if file is complete
                  if (FragmentList_isEntryComplete(fragmentNode))
                  {
                    FragmentList_discard(fragmentList,fragmentNode);
                  }
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
                  printError("unexpected data at end of hard link entry '%S'!\n",fileName);
                  if (failError == ERROR_NONE)
                  {
                    failError = ERRORX_(CORRUPT_DATA,0,String_cString(fileName));
                  }
                  break;
                }

                printInfo(1,"OK\n");

                testedDataFlag = TRUE;
              }
              else
              {
                if (failError == ERROR_NONE)
                {
                  printInfo(1,"OK\n");
                }
                else
                {
                  printInfo(1,"FAIL!\n");
                }
              }
            }
            else
            {
              // skip
              printInfo(2,"  Test '%s'...skipped\n",String_cString(fileName));
            }
          }
          if (failError != ERROR_NONE)
          {
            (void)Archive_closeEntry(&archiveEntryInfo);
            StringList_done(&fileNameList);
            break;
          }

          // close archive file, free resources
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printError("closing 'hard link' entry fail (error: %s)!\n",
                       Error_getText(error)
                      );
            StringList_done(&fileNameList);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          // free resources
          StringList_done(&fileNameList);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
        {
          String   fileName;
          FileInfo fileInfo;

          // read special
          fileName = String_new();
          error = Archive_readSpecialEntry(&archiveEntryInfo,
                                           &archiveInfo,
                                           NULL,  // cryptAlgorithm
                                           NULL,  // cryptType
                                           fileName,
                                           &fileInfo,
                                           NULL   // fileExtendedAttributeList
                                          );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'special' content of archive '%s' (error: %s)!\n",
                       Storage_getPrintableNameCString(storageSpecifier,archiveName),
                       Error_getText(error)
                      );
            String_delete(fileName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
              && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
             )
          {
            printInfo(1,"  Test special device '%s'...",String_cString(fileName));

            // check if all data read
            if (!Archive_eofData(&archiveEntryInfo))
            {
              printInfo(1,"FAIL!\n");
              printError("unexpected data at end of special entry '%S'!\n",fileName);
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              if (failError == ERROR_NONE)
              {
                failError = ERRORX_(CORRUPT_DATA,0,String_cString(fileName));
              }
              break;
            }

            printInfo(1,"OK\n");

            // free resources
          }
          else
          {
            // skip
            printInfo(2,"  Test '%s'...skipped\n",String_cString(fileName));
          }

          // close archive file
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printError("closing 'special' entry fail (error: %s)!\n",
                       Error_getText(error)
                      );
            String_delete(fileName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          // free resources
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
  if (!isPrintInfo(1)) printInfo(0,"%s",(failError == ERROR_NONE) ? "OK\n" : "FAIL!\n");

  // close archive
  Archive_close(&archiveInfo);

  // done storage
  (void)Storage_done(&storageHandle);
#endif

  // free resources
  free(buffer);
}
#endif

/***********************************************************************\
* Name   : testArchiveContent
* Purpose: test archive content
* Input  : storageSpecifier    - storage specifier
*          archiveName         - archive name
*          includeEntryList    - include entry list
*          excludePatternList  - exclude pattern list
*          deltaSourceList     - delta source list
*          jobOptions          - job options
*          getPasswordFunction - get password call back
*          getPasswordUserData - user data for get password
*          fragmentList        - fragment list
*          logHandle           - log handle (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors testArchiveContent(StorageSpecifier    *storageSpecifier,
                                ConstString         archiveName,
                                const EntryList     *includeEntryList,
                                const PatternList   *excludePatternList,
                                DeltaSourceList     *deltaSourceList,
                                JobOptions          *jobOptions,
                                GetPasswordFunction getPasswordFunction,
                                void                *getPasswordUserData,
                                FragmentList        *fragmentList,
                                LogHandle           *logHandle
                               )
{
  byte              *buffer;
  StorageHandle     storageHandle;
  Errors            failError;
  Errors            error;
  ArchiveInfo       archiveInfo;
  ArchiveEntryInfo  archiveEntryInfo;
  ArchiveEntryTypes archiveEntryType;

  assert(storageSpecifier != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(jobOptions != NULL);
  assert(fragmentList != NULL);

  // init variables
  buffer = (byte*)malloc(BUFFER_SIZE);
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
               Storage_getPrintableNameCString(storageSpecifier,archiveName),
               Error_getText(error)
              );
    free(buffer);
    return error;
  }

  // open archive
  error = Archive_open(&archiveInfo,
                       &storageHandle,
                       storageSpecifier,
                       archiveName,
                       deltaSourceList,
                       jobOptions,
                       getPasswordFunction,
                       getPasswordUserData,
                       logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot open storage '%s' (error: %s)!\n",
               Storage_getPrintableNameCString(storageSpecifier,archiveName),
               Error_getText(error)
              );
    (void)Storage_done(&storageHandle);
    free(buffer);
    return error;
  }

  // read archive entries
  printInfo(0,
            "Test storage '%s'%s",
            Storage_getPrintableNameCString(storageSpecifier,archiveName),
            !isPrintInfo(1) ? "..." : ":\n"
           );
  failError = ERROR_NONE;
  while (!Archive_eof(&archiveInfo,FALSE))
  {
    // get next archive entry type
    error = Archive_getNextArchiveEntryType(&archiveInfo,
                                            &archiveEntryType,
                                            FALSE
                                           );
    if (error != ERROR_NONE)
    {
      printError("Cannot read next entry in archive '%s' (error: %s)!\n",
                 Storage_getPrintableNameCString(storageSpecifier,archiveName),
                 Error_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      break;
    }

    switch (archiveEntryType)
    {
      case ARCHIVE_ENTRY_TYPE_FILE:
        {
          String       fileName;
          FileInfo     fileInfo;
          uint64       fragmentOffset,fragmentSize;
          FragmentNode *fragmentNode;
          uint64       length;
          ulong        n;

          // read file
          fileName = String_new();
          error = Archive_readFileEntry(&archiveEntryInfo,
                                        &archiveInfo,
                                        NULL,  // deltaCompressAlgorithm
                                        NULL,  // byteCompressAlgorithm
                                        NULL,  // cryptAlgorithm
                                        NULL,  // cryptType
                                        fileName,
                                        &fileInfo,
                                        NULL,  // fileExtendedAttributeList
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
            String_delete(fileName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
              && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
             )
          {
            printInfo(1,"  Test file '%s'...",String_cString(fileName));

            if (!jobOptions->noFragmentsCheckFlag)
            {
              // get file fragment list
              fragmentNode = FragmentList_find(fragmentList,fileName);
              if (fragmentNode == NULL)
              {
                fragmentNode = FragmentList_add(fragmentList,fileName,fileInfo.size,NULL,0);
              }
              assert(fragmentNode != NULL);
            }
            else
            {
              fragmentNode = NULL;
            }
//FragmentList_print(fragmentNode,String_cString(fileName));

            // read file content
            length = 0LL;
            while (length < fragmentSize)
            {
              n = (ulong)MIN(fragmentSize-length,BUFFER_SIZE);

              // read archive file
              error = Archive_readData(&archiveEntryInfo,buffer,n);
              if (error != ERROR_NONE)
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot read content of archive '%s' (error: %s)!\n",
                           Storage_getPrintableNameCString(storageSpecifier,archiveName),
                           Error_getText(error)
                          );
                break;
              }

              length += (uint64)n;

              printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
            }
            if (error != ERROR_NONE)
            {
              if (failError == ERROR_NONE) failError = error;
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              continue;
            }
            printInfo(2,"    \b\b\b\b");

            if (fragmentNode != NULL)
            {
              // add fragment to file fragment list
              FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);

              // discard fragment list if file is complete
              if (FragmentList_isEntryComplete(fragmentNode))
              {
                FragmentList_discard(fragmentList,fragmentNode);
              }
            }

            /* check if all data read.
               Note: it is not possible to check if all data is read when
               compression is used. The decompressor may not be at the end
               of a compressed data chunk even compressed data is _not_
               corrupt.
            */
            if (   !Compress_isCompressed(archiveEntryInfo.file.deltaCompressAlgorithm)
                && !Compress_isCompressed(archiveEntryInfo.file.byteCompressAlgorithm)
                && !Archive_eofData(&archiveEntryInfo)
               )
            {
              printInfo(1,"FAIL!\n");
              printError("unexpected data at end of file entry '%S'!\n",fileName);
              if (failError == ERROR_NONE)
              {
                failError = ERRORX_(CORRUPT_DATA,0,"%s",String_cString(fileName));
              }
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              break;
            }

            printInfo(1,"OK\n");
          }
          else
          {
            // skip
            printInfo(2,"  Test '%s'...skipped\n",String_cString(fileName));
          }

          // close archive file, free resources
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printError("closing 'file' entry fail (error: %s)!\n",
                       Error_getText(error)
                      );
            String_delete(fileName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          // free resources
          String_delete(fileName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        {
          String       deviceName;
          DeviceInfo   deviceInfo;
          uint64       blockOffset,blockCount;
          FragmentNode *fragmentNode;
          uint64       block;
          ulong        bufferBlockCount;

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
            if (failError == ERROR_NONE) failError = error;
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
            if (failError == ERROR_NONE)
            {
              failError = ERROR_INVALID_DEVICE_BLOCK_SIZE;
            }
            break;
          }
          assert(deviceInfo.blockSize > 0);

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,deviceName,PATTERN_MATCH_MODE_EXACT))
              && !PatternList_match(excludePatternList,deviceName,PATTERN_MATCH_MODE_EXACT)
             )
          {
            printInfo(1,"  Test image '%s'...",String_cString(deviceName));

            if (!jobOptions->noFragmentsCheckFlag)
            {
              // get file fragment node
              fragmentNode = FragmentList_find(fragmentList,deviceName);
              if (fragmentNode == NULL)
              {
                fragmentNode = FragmentList_add(fragmentList,deviceName,deviceInfo.size,NULL,0);
              }
//FragmentList_print(fragmentNode,String_cString(deviceName));
              assert(fragmentNode != NULL);
            }
            else
            {
              fragmentNode = NULL;
            }

            // read image content
            block = 0LL;
            while (block < blockCount)
            {
              bufferBlockCount = MIN(blockCount-block,BUFFER_SIZE/deviceInfo.blockSize);

              // read archive file
              error = Archive_readData(&archiveEntryInfo,buffer,bufferBlockCount*deviceInfo.blockSize);
              if (error != ERROR_NONE)
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot read content of archive '%s' (error: %s)!\n",
                           Storage_getPrintableNameCString(storageSpecifier,archiveName),
                           Error_getText(error)
                          );
                break;
              }

              block += (uint64)bufferBlockCount;

              printInfo(2,"%3d%%\b\b\b\b",(uint)((block*100LL)/blockCount));
            }
            if (error != ERROR_NONE)
            {
              if (failError == ERROR_NONE) failError = error;
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(deviceName);
              continue;
            }
            printInfo(2,"    \b\b\b\b");

            if (fragmentNode != NULL)
            {
              // add fragment to file fragment list
              FragmentList_addEntry(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize);

              // discard fragment list if file is complete
              if (FragmentList_isEntryComplete(fragmentNode))
              {
                FragmentList_discard(fragmentList,fragmentNode);
              }
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
              printInfo(1,"FAIL!\n");
              printError("unexpected data at end of image entry '%S'!\n",deviceName);
              if (failError == ERROR_NONE)
              {
                failError = ERRORX_(CORRUPT_DATA,0,"%s",String_cString(deviceName));
              }
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(deviceName);
              break;
            }

            printInfo(1,"OK\n");
          }
          else
          {
            // skip
            printInfo(2,"  Test '%s'...skipped\n",String_cString(deviceName));
          }

          // close archive file, free resources
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printError("closing 'image' entry fail (error: %s)!\n",
                       Error_getText(error)
                      );
            String_delete(deviceName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          // free resources
          String_delete(deviceName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        {
          String   directoryName;
          FileInfo fileInfo;

          // read directory
          directoryName = String_new();
          error = Archive_readDirectoryEntry(&archiveEntryInfo,
                                             &archiveInfo,
                                             NULL,  // cryptAlgorithm
                                             NULL,  // cryptType
                                             directoryName,
                                             &fileInfo,
                                             NULL   // fileExtendedAttributeList
                                            );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'directory' content of archive '%s' (error: %s)!\n",
                       Storage_getPrintableNameCString(storageSpecifier,archiveName),
                       Error_getText(error)
                      );
            String_delete(directoryName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
              && !PatternList_match(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
             )
          {
            printInfo(1,"  Test directory '%s'...",String_cString(directoryName));

            // check if all data read
            if (!Archive_eofData(&archiveEntryInfo))
            {
              printInfo(1,"FAIL!\n");
              printError("unexpected data at end of directory entry '%S'!\n",directoryName);
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(directoryName);
              if (failError == ERROR_NONE)
              {
                failError = ERRORX_(CORRUPT_DATA,0,"%s",String_cString(directoryName));
              }
              break;
            }

            printInfo(1,"OK\n");

            // free resources
          }
          else
          {
            // skip
            printInfo(2,"Test '%s'...skipped\n",String_cString(directoryName));
          }

          // close archive file
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printError("closing 'directory' entry fail (error: %s)!\n",
                       Error_getText(error)
                      );
            String_delete(directoryName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          // free resources
          String_delete(directoryName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_LINK:
        {
          String   linkName;
          String   fileName;
          FileInfo fileInfo;

          // read link
          linkName = String_new();
          fileName = String_new();
          error = Archive_readLinkEntry(&archiveEntryInfo,
                                        &archiveInfo,
                                        NULL,  // cryptAlgorithm
                                        NULL,  // cryptType
                                        linkName,
                                        fileName,
                                        &fileInfo,
                                        NULL   // fileExtendedAttributeList
                                       );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'link' content of archive '%s' (error: %s)!\n",
                       Storage_getPrintableNameCString(storageSpecifier,archiveName),
                       Error_getText(error)
                      );
            String_delete(fileName);
            String_delete(linkName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
              && !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
             )
          {
            printInfo(1,"  Test link '%s'...",String_cString(linkName));

            // check if all data read
            if (!Archive_eofData(&archiveEntryInfo))
            {
              printInfo(1,"FAIL!\n");
              printError("unexpected data at end of link entry '%S'!\n",linkName);
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              String_delete(linkName);
              if (failError == ERROR_NONE)
              {
                failError = ERRORX_(CORRUPT_DATA,0,"%s",String_cString(linkName));
              }
              break;
            }

            printInfo(1,"OK\n");

            // free resources
          }
          else
          {
            // skip
            printInfo(2,"  Test '%s'...skipped\n",String_cString(linkName));
          }

          // close archive file
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printError("closing 'link' entry fail (error: %s)!\n",
                       Error_getText(error)
                      );
            String_delete(fileName);
            String_delete(linkName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          // free resources
          String_delete(fileName);
          String_delete(linkName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        {
          StringList       fileNameList;
          FileInfo         fileInfo;
          uint64           fragmentOffset,fragmentSize;
          bool             testedDataFlag;
          const StringNode *stringNode;
          String           fileName;
          FragmentNode     *fragmentNode;
          uint64           length;
          ulong            n;

          // read hard linke
          StringList_init(&fileNameList);
          error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                            &archiveInfo,
                                            NULL,  // deltaCompressAlgorithm
                                            NULL,  // byteCompressAlgorithm
                                            NULL,  // cryptAlgorithm
                                            NULL,  // cryptType
                                            &fileNameList,
                                            &fileInfo,
                                            NULL,  // fileExtendedAttributeList
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
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          testedDataFlag = FALSE;
          STRINGLIST_ITERATE(&fileNameList,stringNode,fileName)
          {
            if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(1,"  Test hard link '%s'...",String_cString(fileName));

              if (!testedDataFlag && (failError == ERROR_NONE))
              {
                // read hard link data

                if (!jobOptions->noFragmentsCheckFlag)
                {
                  // get file fragment list
                  fragmentNode = FragmentList_find(fragmentList,fileName);
                  if (fragmentNode == NULL)
                  {
                    fragmentNode = FragmentList_add(fragmentList,fileName,fileInfo.size,NULL,0);
                  }
                  assert(fragmentNode != NULL);
//FragmentList_print(fragmentNode,String_cString(fileName));
                }
                else
                {
                  fragmentNode = NULL;
                }

                // read hard link content
                length = 0LL;
                while (length < fragmentSize)
                {
                  n = (ulong)MIN(fragmentSize-length,BUFFER_SIZE);

                  // read archive file
                  error = Archive_readData(&archiveEntryInfo,buffer,n);
                  if (error != ERROR_NONE)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot read content of archive '%s' (error: %s)!\n",
                               Storage_getPrintableNameCString(storageSpecifier,archiveName),
                               Error_getText(error)
                              );
                    break;
                  }

                  length += (uint64)n;

                  printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
                }
                if (error != ERROR_NONE)
                {
                  if (failError == ERROR_NONE) failError = error;
                  break;
                }
                printInfo(2,"    \b\b\b\b");

                if (fragmentNode != NULL)
                {
                  // add fragment to file fragment list
                  FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);

                  // discard fragment list if file is complete
                  if (FragmentList_isEntryComplete(fragmentNode))
                  {
                    FragmentList_discard(fragmentList,fragmentNode);
                  }
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
                  printError("unexpected data at end of hard link entry '%S'!\n",fileName);
                  if (failError == ERROR_NONE)
                  {
                    failError = ERRORX_(CORRUPT_DATA,0,"%s",String_cString(fileName));
                  }
                  break;
                }

                printInfo(1,"OK\n");

                testedDataFlag = TRUE;
              }
              else
              {
                if (failError == ERROR_NONE)
                {
                  printInfo(1,"OK\n");
                }
                else
                {
                  printInfo(1,"FAIL!\n");
                }
              }
            }
            else
            {
              // skip
              printInfo(2,"  Test '%s'...skipped\n",String_cString(fileName));
            }
          }
          if (failError != ERROR_NONE)
          {
            (void)Archive_closeEntry(&archiveEntryInfo);
            StringList_done(&fileNameList);
            break;
          }

          // close archive file, free resources
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printError("closing 'hard link' entry fail (error: %s)!\n",
                       Error_getText(error)
                      );
            StringList_done(&fileNameList);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          // free resources
          StringList_done(&fileNameList);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
        {
          String   fileName;
          FileInfo fileInfo;

          // read special
          fileName = String_new();
          error = Archive_readSpecialEntry(&archiveEntryInfo,
                                           &archiveInfo,
                                           NULL,  // cryptAlgorithm
                                           NULL,  // cryptType
                                           fileName,
                                           &fileInfo,
                                           NULL   // fileExtendedAttributeList
                                          );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'special' content of archive '%s' (error: %s)!\n",
                       Storage_getPrintableNameCString(storageSpecifier,archiveName),
                       Error_getText(error)
                      );
            String_delete(fileName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
              && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
             )
          {
            printInfo(1,"  Test special device '%s'...",String_cString(fileName));

            // check if all data read
            if (!Archive_eofData(&archiveEntryInfo))
            {
              printInfo(1,"FAIL!\n");
              printError("unexpected data at end of special entry '%S'!\n",fileName);
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              if (failError == ERROR_NONE)
              {
                failError = ERRORX_(CORRUPT_DATA,0,"%s",String_cString(fileName));
              }
              break;
            }

            printInfo(1,"OK\n");

            // free resources
          }
          else
          {
            // skip
            printInfo(2,"  Test '%s'...skipped\n",String_cString(fileName));
          }

          // close archive file
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printError("closing 'special' entry fail (error: %s)!\n",
                       Error_getText(error)
                      );
            String_delete(fileName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }

          // free resources
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
  if (!isPrintInfo(1)) printInfo(0,"%s",(failError == ERROR_NONE) ? "OK\n" : "FAIL!\n");

  // close archive
  Archive_close(&archiveInfo);

  // done storage
  (void)Storage_done(&storageHandle);

  // free resources
  free(buffer);

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Command_test(const StringList    *storageNameList,
                    const EntryList     *includeEntryList,
                    const PatternList   *excludePatternList,
                    DeltaSourceList     *deltaSourceList,
                    JobOptions          *jobOptions,
                    GetPasswordFunction getPasswordFunction,
                    void                *getPasswordUserData,
                    LogHandle           *logHandle
                   )
{
  FragmentList               fragmentList;
  StorageSpecifier           storageSpecifier;
  StringNode                 *stringNode;
  String                     storageName;
  Errors                     failError;
  Errors                     error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  Pattern                    pattern;
  String                     fileName;
  FragmentNode               *fragmentNode;

  assert(storageNameList != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(jobOptions != NULL);

  // allocate resources
  FragmentList_init(&fragmentList);
  Storage_initSpecifier(&storageSpecifier);

  failError = ERROR_NONE;
  STRINGLIST_ITERATE(storageNameList,stringNode,storageName)
  {
    // parse storage name
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error != ERROR_NONE)
    {
      printError("Invalid storage '%s' (error: %s)!\n",
                 String_cString(storageName),
                 Error_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      continue;
    }

    error = ERROR_UNKNOWN;

    if (error != ERROR_NONE)
    {
      if (String_isEmpty(storageSpecifier.archivePatternString))
      {
        // test archive content
        error = testArchiveContent(&storageSpecifier,
                                   NULL,
                                   includeEntryList,
                                   excludePatternList,
                                   deltaSourceList,
                                   jobOptions,
                                   getPasswordFunction,
                                   getPasswordUserData,
                                   &fragmentList,
                                   logHandle
                                  );
      }
    }
    if (error != ERROR_NONE)
    {
      error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                        &storageSpecifier,
                                        NULL,  // archiveName
                                        jobOptions,
                                        SERVER_CONNECTION_PRIORITY_HIGH
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

            // test archive content
            error = testArchiveContent(&storageSpecifier,
                                       fileName,
                                       includeEntryList,
                                       excludePatternList,
                                       deltaSourceList,
                                       jobOptions,
                                       getPasswordFunction,
                                       getPasswordUserData,
                                       &fragmentList,
                                       logHandle
                                      );
          }
          String_delete(fileName);
          Pattern_done(&pattern);
        }
        else
        {
          printError("Cannot open storage '%s' (error: %s)!\n",
                     String_cString(storageName),
                     Error_getText(error)
                    );
        }
        Storage_closeDirectoryList(&storageDirectoryListHandle);
      }
    }
    if (error != ERROR_NONE)
    {
      if (failError == ERROR_NONE) failError = error;
      continue;
    }
  }

  if (   (failError == ERROR_NONE)
      && !jobOptions->noFragmentsCheckFlag
     )
  {
    // check fragment lists
    FRAGMENTLIST_ITERATE(&fragmentList,fragmentNode)
    {
      if (!FragmentList_isEntryComplete(fragmentNode))
      {
        printInfo(0,"Warning: incomplete file '%s'\n",String_cString(fragmentNode->name));
        if (isPrintInfo(2))
        {
          printInfo(2,"  Fragments:\n");
          FragmentList_print(stdout,4,fragmentNode);
        }
        if (failError == ERROR_NONE) failError = ERROR_ENTRY_INCOMPLETE;
      }
    }
  }

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  FragmentList_done(&fragmentList);

  return failError;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
