/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive compare function
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
#include "entrylists.h"
#include "patternlists.h"
#include "files.h"
#include "filesystems.h"
#include "archive.h"
#include "fragmentlists.h"
#include "sources.h"

#include "commands_compare.h"

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
* Name   : compare
* Purpose: compare memory
* Input  : p0,p1  - memory to compare
*          length - size of memory blocks (in bytes)
* Output : -
* Return : number of equal bytes or length if memory blocks are equal
* Notes  : -
\***********************************************************************/

LOCAL_INLINE ulong compare(const void *p0, const void *p1, ulong length)
{
  const byte *b0,*b1;
  ulong      i;

  b0 = (const byte*)p0;
  b1 = (const byte*)p1;
  i = 0L;
  while (   (i < length)
         && ((*b0) == (*b1))
        )
  {
    b0++;
    b1++;
    i++;
  }

  return i;
}

/*---------------------------------------------------------------------*/

Errors Command_compare(const StringList                *storageNameList,
                       const EntryList                 *includeEntryList,
                       const PatternList               *excludePatternList,
                       JobOptions                      *jobOptions,
                       ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                       void                            *archiveGetCryptPasswordUserData
                      )
{
  AutoFreeList      autoFreeList;
  byte              *archiveBuffer,*buffer;
  FragmentList      fragmentList;
  StorageSpecifier  storageSpecifier;
  String            storageFileName;
  String            printableStorageName;
  StringNode        *stringNode;
  String            storageName;
  Errors            failError;
  Errors            error;
  ArchiveInfo       archiveInfo;
  ArchiveEntryInfo  archiveEntryInfo;
  ArchiveEntryTypes archiveEntryType;
  FragmentNode      *fragmentNode;

  assert(storageNameList != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(jobOptions != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  // allocate resources
  archiveBuffer = (byte*)malloc(BUFFER_SIZE);
  if (archiveBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    free(archiveBuffer);
    HALT_INSUFFICIENT_MEMORY();
  }
  FragmentList_init(&fragmentList);
  Storage_initSpecifier(&storageSpecifier);
  storageFileName      = String_new();
  printableStorageName = String_new();
  AUTOFREE_ADD(&autoFreeList,printableStorageName,{ String_delete(printableStorageName); });
  AUTOFREE_ADD(&autoFreeList,storageFileName,{ String_delete(storageFileName); });
  AUTOFREE_ADD(&autoFreeList,&storageSpecifier,{ Storage_doneSpecifier(&storageSpecifier); });
  AUTOFREE_ADD(&autoFreeList,&fragmentList,{ FragmentList_done(&fragmentList); });
  AUTOFREE_ADD(&autoFreeList,archiveBuffer,{ free(archiveBuffer); });
  AUTOFREE_ADD(&autoFreeList,buffer,{ free(buffer); });

  failError = ERROR_NONE;
  STRINGLIST_ITERATE(storageNameList,stringNode,storageName)
  {
    // parse storage name, get printable name
    error = Storage_parseName(storageName,&storageSpecifier,storageFileName);
    if (error != ERROR_NONE)
    {
      printError("Invalid storage '%s' (error: %s)!\n",
                 String_cString(storageName),
                 Errors_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      continue;
    }
    DEBUG_TESTCODE("Command_compare1") { failError = DEBUG_TESTCODE_ERROR(); break; }
    Storage_getPrintableName(printableStorageName,&storageSpecifier,storageFileName);

    printInfo(1,"Comparing archive '%s':\n",String_cString(printableStorageName));

    // open archive
    error = Archive_open(&archiveInfo,
                         &storageSpecifier,
                         storageFileName,
                         jobOptions,
                         &globalOptions.maxBandWidthList,
                         SERVER_CONNECTION_PRIORITY_HIGH,
                         archiveGetCryptPasswordFunction,
                         archiveGetCryptPasswordUserData
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot open archive file '%s' (error: %s)!\n",
                 String_cString(printableStorageName),
                 Errors_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      continue;
    }
    DEBUG_TESTCODE("Command_compare2") { (void)Archive_close(&archiveInfo); failError = DEBUG_TESTCODE_ERROR(); break; }

    // read files
    while (   !Archive_eof(&archiveInfo,TRUE)
           && (failError == ERROR_NONE)
          )
    {
      // get next archive entry type
      error = Archive_getNextArchiveEntryType(&archiveInfo,
                                              &archiveEntryType,
                                              TRUE
                                             );
      if (error != ERROR_NONE)
      {
        printError("Cannot read next entry in archive '%s' (error: %s)!\n",
                   String_cString(printableStorageName),
                   Errors_getText(error)
                  );
        if (failError == ERROR_NONE) failError = error;
        break;
      }
      DEBUG_TESTCODE("Command_compare3") { failError = DEBUG_TESTCODE_ERROR(); break; }

      switch (archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          {
            CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
            String             fileName;
            FileInfo           fileInfo;
            uint64             fragmentOffset,fragmentSize;
            FragmentNode       *fragmentNode;
//            FileInfo         localFileInfo;
            FileHandle         fileHandle;
            bool               equalFlag;
            uint64             length;
            ulong              bufferLength;
            ulong              diffIndex;

            // read file
            fileName = String_new();
            error = Archive_readFileEntry(&archiveEntryInfo,
                                          &archiveInfo,
                                          &deltaCompressAlgorithm,
                                          &byteCompressAlgorithm,
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
                         String_cString(printableStorageName),
                         Errors_getText(error)
                        );
              String_delete(fileName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }
            DEBUG_TESTCODE("Command_compare100") { String_delete(fileName); Archive_closeEntry(&archiveEntryInfo); failError = DEBUG_TESTCODE_ERROR(); break; }

            if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(1,"  Compare file '%s'...",String_cString(fileName));

              // check if file exists and check file type
              if (!File_exists(fileName))
              {
                printInfo(1,"FAIL!\n");
                printError("File '%s' not found!\n",String_cString(fileName));
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag) failError = ERROR_FILE_NOT_FOUND_;
                break;
              }
              if (File_getType(fileName) != FILE_TYPE_FILE)
              {
                printInfo(1,"FAIL!\n");
                printError("'%s' is not a file!\n",String_cString(fileName));
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag) failError = ERROR_WRONG_ENTRY_TYPE;
                break;
              }

              if (!jobOptions->noFragmentsCheckFlag)
              {
                // get file fragment node
                fragmentNode = FragmentList_find(&fragmentList,fileName);
                if (fragmentNode == NULL)
                {
                  fragmentNode = FragmentList_add(&fragmentList,fileName,fileInfo.size,NULL,0);
                }
                assert(fragmentNode != NULL);
//FragmentList_print(fragmentNode,String_cString(fileName));
              }
              else
              {
                fragmentNode = NULL;
              }

              // open file
              error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
              if (error != ERROR_NONE)
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot open file '%s' (error: %s)\n",
                           String_cString(fileName),
                           Errors_getText(error)
                          );
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag) failError = error;
                continue;
              }
              DEBUG_TESTCODE("Command_compare101") { (void)File_close(&fileHandle); String_delete(fileName); Archive_closeEntry(&archiveEntryInfo); failError = DEBUG_TESTCODE_ERROR(); break; }

              // check file size
              if (fileInfo.size != (long)File_getSize(&fileHandle))
              {
                printInfo(1,"FAIL!\n");
                printError("'%s' differ in size: expected %lld bytes, found %lld bytes\n",
                           String_cString(fileName),
                           fileInfo.size,
                           File_getSize(&fileHandle)
                          );
                File_close(&fileHandle);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag) failError = ERROR_ENTRIES_DIFFER;
                continue;
              }

              // seek to fragment position
              error = File_seek(&fileHandle,fragmentOffset);
              if (error != ERROR_NONE)
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot read file '%s' (error: %s)\n",
                           String_cString(fileName),
                           Errors_getText(error)
                          );
                File_close(&fileHandle);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag) failError = error;
                continue;
              }
              DEBUG_TESTCODE("Command_compare102") { (void)File_close(&fileHandle); String_delete(fileName); Archive_closeEntry(&archiveEntryInfo); failError = DEBUG_TESTCODE_ERROR(); continue; }

              // compare archive and file content
              length    = 0LL;
              equalFlag = TRUE;
              diffIndex = 0L;
              while (   (length < fragmentSize)
                     && equalFlag
                    )
              {
                bufferLength = MIN(fragmentSize-length,BUFFER_SIZE);

                // read archive, file
                error = Archive_readData(&archiveEntryInfo,archiveBuffer,bufferLength);
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot read content of archive '%s' (error: %s)!\n",
                             String_cString(printableStorageName),
                             Errors_getText(error)
                            );
                  if (failError == ERROR_NONE) failError = error;
                  break;
                }
                DEBUG_TESTCODE("Command_compare103") { failError = DEBUG_TESTCODE_ERROR(); break; }
                error = File_read(&fileHandle,buffer,bufferLength,NULL);
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot read file '%s' (error: %s)\n",
                             String_cString(fileName),
                             Errors_getText(error)
                            );
                  if (jobOptions->stopOnErrorFlag) failError = error;
                  break;
                }
                DEBUG_TESTCODE("Command_compare104") { failError = DEBUG_TESTCODE_ERROR(); break; }

                // compare
                diffIndex = compare(archiveBuffer,buffer,bufferLength);
                equalFlag = (diffIndex >= bufferLength);
                if (!equalFlag)
                {
                  error = ERROR_ENTRIES_DIFFER;

                  printInfo(1,"FAIL!\n");
                  printError("'%s' differ at offset %llu\n",
                             String_cString(fileName),
                             fragmentOffset+length+(uint64)diffIndex
                            );
                  failError = error;
                  break;
                }

                length += (uint64)bufferLength;

                printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
              }
              if (error != ERROR_NONE)
              {
                File_close(&fileHandle);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                continue;
              }
              if (failError != ERROR_NONE)
              {
                File_close(&fileHandle);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                break;
              }
              DEBUG_TESTCODE("Command_compare105") { File_close(&fileHandle); Archive_closeEntry(&archiveEntryInfo); String_delete(fileName); failError = DEBUG_TESTCODE_ERROR(); break; }

              printInfo(2,"    \b\b\b\b");

              // close file
              File_close(&fileHandle);

              if (fragmentNode != NULL)
              {
                // add fragment to file fragment list
                FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);

                // discard fragment list if file is complete
                if (FragmentList_isEntryComplete(fragmentNode))
                {
                  FragmentList_discard(&fragmentList,fragmentNode);
                }
              }

#if 0
              // get local file info
              // check file time, permissions, file owner/group
#endif /* 0 */
              printInfo(1,"ok\n");

              /* check if all data read.
                 Note: it is not possible to check if all data is read when
                 compression is used. The decompressor may not be at the end
                 of a compressed data chunk even compressed data is _not_
                 corrupt.
              */
              if (   !Compress_isCompressed(deltaCompressAlgorithm)
                  && !Compress_isCompressed(byteCompressAlgorithm)
                  && !Archive_eofData(&archiveEntryInfo))
              {
                printWarning("unexpected data at end of file entry '%S'.\n",fileName);
              }

              // free resources
            }
            else
            {
              // skip
              printInfo(2,"  Compare '%s'...skipped\n",String_cString(fileName));
            }

            // close archive file
            error = Archive_closeEntry(&archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              printWarning("close 'file' entry fail (error: %s)\n",Errors_getText(error));
            }

            // free resources
            String_delete(fileName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
            String             deviceName;
            DeviceInfo         deviceInfo;
            uint64             blockOffset,blockCount;
            FragmentNode       *fragmentNode;
            DeviceHandle       deviceHandle;
            bool               fileSystemFlag;
            FileSystemHandle   fileSystemHandle;
            bool               equalFlag;
            uint64             block;
            ulong              diffIndex;

            // read image
            deviceName = String_new();
            error = Archive_readImageEntry(&archiveEntryInfo,
                                           &archiveInfo,
                                           &deltaCompressAlgorithm,
                                           &byteCompressAlgorithm,
                                           NULL,  // cryptAlgorithm
                                           NULL,  // cryptType
                                           deviceName,
                                           &deviceInfo,
                                           NULL,  // deltaSourceName
                                           NULL,  // deltaSourceSize
                                           &blockOffset,
                                           &blockCount
                                          );
            if (error != ERROR_NONE)
            {
              printError("Cannot read 'image' content of archive '%s' (error: %s)!\n",
                         String_cString(printableStorageName),
                         Errors_getText(error)
                        );
              String_delete(deviceName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }
            DEBUG_TESTCODE("Command_compare200") { Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); failError = DEBUG_TESTCODE_ERROR(); break; }
            if (deviceInfo.blockSize > BUFFER_SIZE)
            {
              printError("Device block size %llu on '%s' is too big (max: %llu)\n",
                         deviceInfo.blockSize,
                         String_cString(deviceName),
                         BUFFER_SIZE
                        );
              String_delete(deviceName);
              if (jobOptions->stopOnErrorFlag) failError = ERROR_INVALID_DEVICE_BLOCK_SIZE;
              break;
            }
            DEBUG_TESTCODE("Command_compare201") { Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); failError = DEBUG_TESTCODE_ERROR(); break; }
            assert(deviceInfo.blockSize > 0);

            if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,deviceName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,deviceName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(1,"  Compare image '%s'...",String_cString(deviceName));

              // check if device/image exists
              if (!File_exists(deviceName))
              {
                printInfo(1,"FAIL!\n");
                printError("Device '%s' not found!\n",String_cString(deviceName));
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(deviceName);
                if (jobOptions->stopOnErrorFlag) failError = ERROR_FILE_NOT_FOUND_;
                break;
              }

              if (!jobOptions->noFragmentsCheckFlag)
              {
                // get image fragment list
                fragmentNode = FragmentList_find(&fragmentList,deviceName);
                if (fragmentNode == NULL)
                {
                  fragmentNode = FragmentList_add(&fragmentList,deviceName,deviceInfo.size,NULL,0);
                }
                assert(fragmentNode != NULL);
              }
              else
              {
                fragmentNode = NULL;
              }

              // get device info
              error = Device_getDeviceInfo(&deviceInfo,deviceName);
              if (error != ERROR_NONE)
              {
                if (jobOptions->skipUnreadableFlag)
                {
                  printInfo(1,"skipped (reason: %s)\n",Errors_getText(error));
                }
                else
                {
                  printInfo(1,"FAIL\n");
                  printError("Cannot open device '%s' (error: %s)\n",
                             String_cString(deviceName),
                             Errors_getText(error)
                            );
                  if (jobOptions->stopOnErrorFlag) failError = error;
                }
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(deviceName);
                continue;
              }
              DEBUG_TESTCODE("Command_compare202") { Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); failError = DEBUG_TESTCODE_ERROR(); break; }

              // check device block size, get max. blocks in buffer
              if (deviceInfo.blockSize > BUFFER_SIZE)
              {
                printInfo(1,"FAIL\n");
                printError("Device block size %llu on '%s' is too big (max: %llu)\n",
                           deviceInfo.blockSize,
                           String_cString(deviceName),
                           BUFFER_SIZE
                          );
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(deviceName);
                if (jobOptions->stopOnErrorFlag) failError = ERROR_INVALID_DEVICE_BLOCK_SIZE;
                continue;
              }
              assert(deviceInfo.blockSize > 0);

              // open device
              error = Device_open(&deviceHandle,deviceName,DEVICE_OPEN_READ);
              if (error != ERROR_NONE)
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot open file '%s' (error: %s)\n",
                           String_cString(deviceName),
                           Errors_getText(error)
                          );
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(deviceName);
                if (jobOptions->stopOnErrorFlag) failError = error;
                continue;
              }
              DEBUG_TESTCODE("Command_compare203") { Device_close(&deviceHandle); Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); failError = DEBUG_TESTCODE_ERROR(); break; }

              // check image size
              if (deviceInfo.size != Device_getSize(&deviceHandle))
              {
                printInfo(1,"FAIL!\n");
                printError("'%s' differ in size: expected %lld bytes, found %lld bytes\n",
                           String_cString(deviceName),
                           deviceInfo.size,
                           Device_getSize(&deviceHandle)
                          );
                Device_close(&deviceHandle);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(deviceName);
                if (jobOptions->stopOnErrorFlag) failError = ERROR_ENTRIES_DIFFER;
                continue;
              }

              // check if device contain a known file system or a raw image should be compared
              if (!jobOptions->rawImagesFlag)
              {
                fileSystemFlag = (FileSystem_init(&fileSystemHandle,&deviceHandle) == ERROR_NONE);
              }
              else
              {
                fileSystemFlag = FALSE;
              }

              // seek to fragment position
              error = Device_seek(&deviceHandle,blockOffset*(uint64)deviceInfo.blockSize);
              if (error != ERROR_NONE)
              {
                printInfo(1,"FAIL!\n");
                printError("Cannot write to device '%s' (error: %s)\n",
                           String_cString(deviceName),
                           Errors_getText(error)
                          );
                Device_close(&deviceHandle);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(deviceName);
                if (jobOptions->stopOnErrorFlag) failError = error;
                continue;
              }
              DEBUG_TESTCODE("Command_compare204") { Device_close(&deviceHandle); Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); failError = DEBUG_TESTCODE_ERROR(); break; }

              // compare archive and device/image content
              block     = 0LL;
              equalFlag = TRUE;
              diffIndex = 0L;
              while (   (block < blockCount)
                     && equalFlag
                    )
              {
                // read data from archive (only single block)
                error = Archive_readData(&archiveEntryInfo,archiveBuffer,deviceInfo.blockSize);
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot read content of archive '%s' (error: %s)!\n",
                             String_cString(printableStorageName),
                             Errors_getText(error)
                            );
                  if (failError == ERROR_NONE) failError = error;
                  break;
                }
                DEBUG_TESTCODE("Command_compare205") { failError = DEBUG_TESTCODE_ERROR(); break; }

                if (   !fileSystemFlag
                    || FileSystem_blockIsUsed(&fileSystemHandle,(blockOffset+block)*(uint64)deviceInfo.blockSize)
                   )
                {
                  // seek to device/image position
                  error = Device_seek(&deviceHandle,(blockOffset+block)*(uint64)deviceInfo.blockSize);
                  if (error != ERROR_NONE)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot seek device '%s' (error: %s)\n",
                               String_cString(deviceName),
                               Errors_getText(error)
                              );
                    if (jobOptions->stopOnErrorFlag) failError = error;
                    break;
                  }
                  DEBUG_TESTCODE("Command_compare206") { failError = DEBUG_TESTCODE_ERROR(); break; }

                  // read data from device/image
                  error = Device_read(&deviceHandle,buffer,deviceInfo.blockSize,NULL);
                  if (error != ERROR_NONE)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot read device '%s' (error: %s)\n",
                               String_cString(deviceName),
                               Errors_getText(error)
                              );
                    if (jobOptions->stopOnErrorFlag) failError = error;
                    break;
                  }
                  DEBUG_TESTCODE("Command_compare207") { failError = DEBUG_TESTCODE_ERROR(); break; }

                  // compare
                  diffIndex = compare(archiveBuffer,buffer,deviceInfo.blockSize);
                  equalFlag = (diffIndex >= deviceInfo.blockSize);
                  if (!equalFlag)
                  {
                    error = ERROR_ENTRIES_DIFFER;

                    printInfo(1,"FAIL!\n");
                    printError("'%s' differ at offset %llu\n",
                               String_cString(deviceName),
                               blockOffset*(uint64)deviceInfo.blockSize+block*(uint64)deviceInfo.blockSize+(uint64)diffIndex
                              );
                    failError = error;
                    break;
                  }
                }

                block += 1LL;

                printInfo(2,"%3d%%\b\b\b\b",(uint)((block*100LL)/blockCount));
              }
              if (error != ERROR_NONE)
              {
                if (fileSystemFlag) FileSystem_done(&fileSystemHandle);
                Device_close(&deviceHandle);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(deviceName);
                continue;
              }
              if (failError != ERROR_NONE)
              {
                if (fileSystemFlag) FileSystem_done(&fileSystemHandle);
                Device_close(&deviceHandle);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(deviceName);
                break;
              }
              DEBUG_TESTCODE("Command_compare208") { failError = DEBUG_TESTCODE_ERROR(); break; }
              printInfo(2,"    \b\b\b\b");

              if (fragmentNode != NULL)
              {
                // add fragment to file fragment list
                FragmentList_addEntry(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize);

                // discard fragment list if file is complete
                if (FragmentList_isEntryComplete(fragmentNode))
                {
                  FragmentList_discard(&fragmentList,fragmentNode);
                }
              }

              printInfo(1,"ok\n",
                        fileSystemFlag?FileSystem_getName(fileSystemHandle.type):"raw"
                       );

              /* check if all data read.
                 Note: it is not possible to check if all data is read when
                 compression is used. The decompressor may not be at the end
                 of a compressed data chunk even compressed data is _not_
                 corrupt.
              */
              if (   !Compress_isCompressed(deltaCompressAlgorithm)
                  && !Compress_isCompressed(byteCompressAlgorithm)
                  && !Archive_eofData(&archiveEntryInfo))
              {
                printWarning("unexpected data at end of image entry '%S'.\n",deviceName);
              }

              // done file system
              if (fileSystemFlag)
              {
                FileSystem_done(&fileSystemHandle);
              }

              // close device
              Device_close(&deviceHandle);
            }
            else
            {
              // skip
              printInfo(2,"  Compare '%s'...skipped\n",String_cString(deviceName));
            }

            // close archive file, free resources
            error = Archive_closeEntry(&archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              printWarning("close 'image' entry fail (error: %s)\n",Errors_getText(error));
            }

            // free resources
            String_delete(deviceName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          {
            String   directoryName;
            FileInfo fileInfo;
//            String   localFileName;
//            FileInfo localFileInfo;

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
                         String_cString(printableStorageName),
                         Errors_getText(error)
                        );
              String_delete(directoryName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }
            DEBUG_TESTCODE("Command_compare300") { Archive_closeEntry(&archiveEntryInfo); String_delete(directoryName); failError = DEBUG_TESTCODE_ERROR(); break; }

            if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(1,"  Compare directory '%s'...",String_cString(directoryName));

              // check if file exists and file type
              if (!File_exists(directoryName))
              {
                printInfo(1,"FAIL!\n");
                printError("Directory '%s' does not exists!\n",String_cString(directoryName));
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(directoryName);
                if (jobOptions->stopOnErrorFlag) failError = ERROR_FILE_NOT_FOUND_;
                break;
              }
              if (File_getType(directoryName) != FILE_TYPE_DIRECTORY)
              {
                printInfo(1,"FAIL!\n");
                printError("'%s' is not a directory!\n",
                           String_cString(directoryName)
                          );
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(directoryName);
                if (jobOptions->stopOnErrorFlag) failError = ERROR_WRONG_ENTRY_TYPE;
                break;
              }

#if 0
              // get local file info
              error = File_getFileInfo(&localFileInfo,directoryName);
              if (error != ERROR_NONE)
              {
                printError("Cannot read local directory '%s' (error: %s)!\n",
                           String_cString(directoryName),
                           Errors_getText(error)
                          );
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(directoryName);
                if (failError == ERROR_NONE) failError = error;
                break;
              }
              DEBUG_TESTCODE("Command_compare301") { Archive_closeEntry(&archiveEntryInfo); String_delete(directoryName); failError = DEBUG_TESTCODE_ERROR(); break; }

              // check file time, permissions, file owner/group
#endif /* 0 */
              printInfo(1,"ok\n");

              // check if all data read
              if (!Archive_eofData(&archiveEntryInfo))
              {
                printWarning("unexpected data at end of directory entry '%S'.\n",directoryName);
              }

              // free resources
            }
            else
            {
              // skip
              printInfo(2,"  Compare '%s'...skipped\n",String_cString(directoryName));
            }

            // close archive file
            error = Archive_closeEntry(&archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              printWarning("close 'directory' entry fail (error: %s)\n",Errors_getText(error));
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
            String   localFileName;
//            FileInfo localFileInfo;

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
                         String_cString(printableStorageName),
                         Errors_getText(error)
                        );
              String_delete(fileName);
              String_delete(linkName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }
            DEBUG_TESTCODE("Command_compare400") { Archive_closeEntry(&archiveEntryInfo); String_delete(linkName); String_delete(fileName); failError = DEBUG_TESTCODE_ERROR(); break; }

            if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(1,"  Compare link '%s'...",String_cString(linkName));

              // check if file exists and file type
              if (!File_exists(linkName))
              {
                printInfo(1,"FAIL!\n");
                printError("Link '%s' -> '%s' does not exists!\n",
                           String_cString(linkName),
                           String_cString(fileName)
                          );
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (jobOptions->stopOnErrorFlag) failError = ERROR_FILE_NOT_FOUND_;
                break;
              }
              if (File_getType(linkName) != FILE_TYPE_LINK)
              {
                printInfo(1,"FAIL!\n");
                printError("'%s' is not a link!\n",
                           String_cString(linkName)
                          );
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (jobOptions->stopOnErrorFlag) failError = ERROR_WRONG_ENTRY_TYPE;
                break;
              }

              // check link name
              localFileName = String_new();
              error = File_readLink(localFileName,linkName);
              if (error != ERROR_NONE)
              {
                printError("Cannot read local file '%s' (error: %s)!\n",
                           String_cString(linkName),
                           Errors_getText(error)
                          );
                String_delete(localFileName);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (jobOptions->stopOnErrorFlag) failError = error;
                break;
              }
              DEBUG_TESTCODE("Command_compare401") { String_delete(localFileName); Archive_closeEntry(&archiveEntryInfo); String_delete(linkName); String_delete(fileName); failError = DEBUG_TESTCODE_ERROR(); break; }
              if (!String_equals(fileName,localFileName))
              {
                printInfo(1,"FAIL!\n");
                printError("Link '%s' does not contain file '%s'!\n",
                           String_cString(linkName),
                           String_cString(fileName)
                          );
                String_delete(localFileName);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (jobOptions->stopOnErrorFlag) failError = ERROR_ENTRIES_DIFFER;
                break;
              }
              String_delete(localFileName);

#if 0
              // get local file info
              error = File_getFileInfo(&localFileInfo,linkName);
              if (error != ERROR_NONE)
              {
                printError("Cannot read local file '%s' (error: %s)!\n",
                           String_cString(linkName),
                           Errors_getText(error)
                          );
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (failError == ERROR_NONE) failError = error;
                break;
              }
              DEBUG_TESTCODE("Command_compare403") { Archive_closeEntry(&archiveEntryInfo); String_delete(linkName); String_delete(fileName); failError = DEBUG_TESTCODE_ERROR(); break; }

              // check file time, permissions, file owner/group
#endif /* 0 */
              printInfo(1,"ok\n");

              // check if all data read
              if (!Archive_eofData(&archiveEntryInfo))
              {
                printWarning("unexpected data at end of link entry '%S'.\n",linkName);
              }

              // free resources
            }
            else
            {
              // skip
              printInfo(2,"  Compare '%s'...skipped\n",String_cString(linkName));
            }

            // close archive file
            error = Archive_closeEntry(&archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              printWarning("close 'link' entry fail (error: %s)\n",Errors_getText(error));
            }

            // free resources
            String_delete(fileName);
            String_delete(linkName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          {
            CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
            StringList         fileNameList;
            FileInfo           fileInfo;
            uint64             fragmentOffset,fragmentSize;
            bool               comparedDataFlag;
            const StringNode   *stringNode;
            String             fileName;
            FragmentNode       *fragmentNode;
//            FileInfo         localFileInfo;
            FileHandle         fileHandle;
            bool               equalFlag;
            uint64             length;
            ulong              bufferLength;
            ulong              diffIndex;

            // read hard link
            StringList_init(&fileNameList);
            error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                              &archiveInfo,
                                              &deltaCompressAlgorithm,
                                              &byteCompressAlgorithm,
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
                         String_cString(printableStorageName),
                         Errors_getText(error)
                        );
              StringList_done(&fileNameList);
              if (failError == ERROR_NONE) failError = error;
              break;
            }
            DEBUG_TESTCODE("Command_compare500") { Archive_closeEntry(&archiveEntryInfo); StringList_done(&fileNameList); failError = DEBUG_TESTCODE_ERROR(); break; }

            comparedDataFlag = FALSE;
            STRINGLIST_ITERATE(&fileNameList,stringNode,fileName)
            {
              if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                  && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                 )
              {
                printInfo(1,"  Compare hard link '%s'...",String_cString(fileName));

                // check file if exists and file type
                if (!File_exists(fileName))
                {
                  printInfo(1,"FAIL!\n");
                  printError("File '%s' not found!\n",String_cString(fileName));
                  if (jobOptions->stopOnErrorFlag)
                  {
                    failError = ERROR_FILE_NOT_FOUND_;
                    break;
                  }
                  else
                  {
                    continue;
                  }
                }
                if (File_getType(fileName) != FILE_TYPE_HARDLINK)
                {
                  printInfo(1,"FAIL!\n");
                  printError("'%s' is not a hard link!\n",String_cString(fileName));
                  if (jobOptions->stopOnErrorFlag)
                  {
                    failError = ERROR_WRONG_ENTRY_TYPE;
                    break;
                  }
                  else
                  {
                    continue;
                  }
                }

                if (!comparedDataFlag && (failError == ERROR_NONE))
                {
                  // compare hard link data

                  if (!jobOptions->noFragmentsCheckFlag)
                  {
                    // get file fragment list
                    fragmentNode = FragmentList_find(&fragmentList,fileName);
                    if (fragmentNode == NULL)
                    {
                      fragmentNode = FragmentList_add(&fragmentList,fileName,fileInfo.size,NULL,0);
                    }
                    assert(fragmentNode != NULL);
                  }
                  else
                  {
                    fragmentNode = NULL;
                  }

                  // open file
                  error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
                  if (error != ERROR_NONE)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot open file '%s' (error: %s)\n",
                               String_cString(fileName),
                               Errors_getText(error)
                              );
                    if (jobOptions->stopOnErrorFlag)
                    {
                      failError = error;
                      break;
                    }
                    else
                    {
                      continue;
                    }
                  }
                  DEBUG_TESTCODE("Command_compare501") { (void)File_close(&fileHandle); failError = DEBUG_TESTCODE_ERROR(); break; }

                  // check file size
                  if (fileInfo.size != (long)File_getSize(&fileHandle))
                  {
                    printInfo(1,"FAIL!\n");
                    printError("'%s' differ in size: expected %lld bytes, found %lld bytes\n",
                               String_cString(fileName),
                               fileInfo.size,
                               File_getSize(&fileHandle)
                              );
                    File_close(&fileHandle);
                    if (jobOptions->stopOnErrorFlag)
                    {
                      failError = ERROR_ENTRIES_DIFFER;
                      break;
                    }
                    else
                    {
                      continue;
                    }
                  }

                  // seek to fragment position
                  error = File_seek(&fileHandle,fragmentOffset);
                  if (error != ERROR_NONE)
                  {
                    printInfo(1,"FAIL!\n");
                    printError("Cannot read file '%s' (error: %s)\n",
                               String_cString(fileName),
                               Errors_getText(error)
                              );
                    File_close(&fileHandle);
                    if (jobOptions->stopOnErrorFlag)
                    {
                      failError = error;
                      break;
                    }
                    else
                    {
                      continue;
                    }
                  }
                  DEBUG_TESTCODE("Command_compare502") { (void)File_close(&fileHandle); failError = DEBUG_TESTCODE_ERROR(); break; }

                  // compare archive and hard link content
                  length    = 0LL;
                  equalFlag = TRUE;
                  diffIndex = 0L;
                  while (   (length < fragmentSize)
                         && equalFlag
                        )
                  {
                    bufferLength = MIN(fragmentSize-length,BUFFER_SIZE);

                    // read archive, file
                    error = Archive_readData(&archiveEntryInfo,archiveBuffer,bufferLength);
                    if (error != ERROR_NONE)
                    {
                      printInfo(1,"FAIL!\n");
                      printError("Cannot read content of archive '%s' (error: %s)!\n",
                                 String_cString(printableStorageName),
                                 Errors_getText(error)
                                );
                      if (failError == ERROR_NONE) failError = error;
                      break;
                    }
                    DEBUG_TESTCODE("Command_compare503") { failError = DEBUG_TESTCODE_ERROR(); break; }
                    error = File_read(&fileHandle,buffer,bufferLength,NULL);
                    if (error != ERROR_NONE)
                    {
                      printInfo(1,"FAIL!\n");
                      printError("Cannot read file '%s' (error: %s)\n",
                                 String_cString(fileName),
                                 Errors_getText(error)
                                );
                      if (jobOptions->stopOnErrorFlag) failError = error;
                      break;
                    }
                    DEBUG_TESTCODE("Command_compare504") { failError = DEBUG_TESTCODE_ERROR(); break; }

                    // compare
                    diffIndex = compare(archiveBuffer,buffer,bufferLength);
                    equalFlag = (diffIndex >= bufferLength);
                    if (!equalFlag)
                    {
                      error = ERROR_ENTRIES_DIFFER;

                      printInfo(1,"FAIL!\n");
                      printError("'%s' differ at offset %llu\n",
                                 String_cString(fileName),
                                 fragmentOffset+length+(uint64)diffIndex
                                );
                      if (jobOptions->stopOnErrorFlag) failError = error;
                      break;
                    }

                    length += (uint64)bufferLength;

                    printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
                  }
                  if (error != ERROR_NONE)
                  {
                    File_close(&fileHandle);
                    break;
                  }
                  printInfo(2,"    \b\b\b\b");

                  // close file
                  File_close(&fileHandle);

                  if (fragmentNode != NULL)
                  {
                    // add fragment to file fragment list
                    FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);

                    // discard fragment list if file is complete
                    if (FragmentList_isEntryComplete(fragmentNode))
                    {
                      FragmentList_discard(&fragmentList,fragmentNode);
                    }
                  }
#if 0
                  // get local file info
                  // check file time, permissions, file owner/group
#endif /* 0 */
                  printInfo(1,"ok\n");

                  /* check if all data read.
                     Note: it is not possible to check if all data is read when
                     compression is used. The decompressor may not be at the end
                     of a compressed data chunk even compressed data is _not_
                     corrupt.
                  */
                  if (   !Compress_isCompressed(deltaCompressAlgorithm)
                      && !Compress_isCompressed(byteCompressAlgorithm)
                      && !Archive_eofData(&archiveEntryInfo))
                  {
                    printWarning("unexpected data at end of hard link entry '%S'.\n",fileName);
                  }

                  comparedDataFlag = TRUE;
                }
                else
                {
                  // compare hard link data already done
                  if (failError == ERROR_NONE)
                  {
                    printInfo(1,"ok\n");
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
                printInfo(2,"  Compare '%s'...skipped\n",String_cString(fileName));
              }
            }

            // close archive file, free resources
            error = Archive_closeEntry(&archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              printWarning("close 'hard link' entry fail (error: %s)\n",Errors_getText(error));
            }

            // free resources
            StringList_done(&fileNameList);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          {
            String   fileName;
            FileInfo fileInfo;
            FileInfo localFileInfo;

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
                         String_cString(printableStorageName),
                         Errors_getText(error)
                        );
              String_delete(fileName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }
            DEBUG_TESTCODE("Command_compare600") { Archive_closeEntry(&archiveEntryInfo); String_delete(fileName); failError = DEBUG_TESTCODE_ERROR(); break; }

            if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(1,"  Compare special device '%s'...",String_cString(fileName));

              // check if file exists and file type
              if (!File_exists(fileName))
              {
                printInfo(1,"FAIL!\n");
                printError("Special device '%s' does not exists!\n",
                           String_cString(fileName)
                          );
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag) failError = ERROR_FILE_NOT_FOUND_;
                break;
              }
              if (File_getType(fileName) != FILE_TYPE_SPECIAL)
              {
                printInfo(1,"FAIL!\n");
                printError("'%s' is not a special device!\n",
                           String_cString(fileName)
                          );
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag) failError = ERROR_WRONG_ENTRY_TYPE;
                break;
              }

              // check special settings
              error = File_getFileInfo(&localFileInfo,fileName);
              if (error != ERROR_NONE)
              {
                printError("Cannot read local file '%s' (error: %s)!\n",
                           String_cString(fileName),
                           Errors_getText(error)
                          );
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag) failError = error;
                break;
              }
              DEBUG_TESTCODE("Command_compare601") { Archive_closeEntry(&archiveEntryInfo); String_delete(fileName); failError = DEBUG_TESTCODE_ERROR(); break; }
              if (fileInfo.specialType != localFileInfo.specialType)
              {
                printError("Different types of special device '%s'!\n",
                           String_cString(fileName)
                          );
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag) failError = error;
                break;
              }
              if (   (fileInfo.specialType == FILE_SPECIAL_TYPE_CHARACTER_DEVICE)
                  || (fileInfo.specialType == FILE_SPECIAL_TYPE_BLOCK_DEVICE)
                 )
              {
                if (fileInfo.major != localFileInfo.major)
                {
                  printError("Different major numbers of special device '%s'!\n",
                             String_cString(fileName)
                            );
                  Archive_closeEntry(&archiveEntryInfo);
                  String_delete(fileName);
                  if (jobOptions->stopOnErrorFlag) failError = error;
                  break;
                }
                if (fileInfo.minor != localFileInfo.minor)
                {
                  printError("Different minor numbers of special device '%s'!\n",
                             String_cString(fileName)
                            );
                  Archive_closeEntry(&archiveEntryInfo);
                  String_delete(fileName);
                  if (jobOptions->stopOnErrorFlag) failError = error;
                  break;
                }
              }

#if 0

              // check file time, permissions, file owner/group
#endif /* 0 */

              printInfo(1,"ok\n");

              // check if all data read
              if (!Archive_eofData(&archiveEntryInfo))
              {
                printWarning("unexpected data at end of special entry '%S'.\n",fileName);
              }

              // free resources
            }
            else
            {
              // skip
              printInfo(2,"  Compare '%s'...skipped\n",String_cString(fileName));
            }

            // close archive file
            error = Archive_closeEntry(&archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              printWarning("close 'special' entry fail (error: %s)\n",Errors_getText(error));
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

    // close archive
    Archive_close(&archiveInfo);

    if (failError != ERROR_NONE) break;
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
        printInfo(0,"Warning: incomplete entry '%s'\n",String_cString(fragmentNode->name));
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
  String_delete(printableStorageName);
  String_delete(storageFileName);
  Storage_doneSpecifier(&storageSpecifier);
  FragmentList_done(&fragmentList);
  free(archiveBuffer);
  free(buffer);
  AutoFree_done(&autoFreeList);

  return failError;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
