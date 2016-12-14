/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive compare functions
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
#include "msgqueues.h"

#include "errors.h"
#include "patterns.h"
#include "entrylists.h"
#include "patternlists.h"
#include "deltasourcelists.h"
#include "files.h"
#include "filesystems.h"
#include "archive.h"
#include "fragmentlists.h"
#include "deltasources.h"

#include "commands_compare.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// data buffer size
#define BUFFER_SIZE (64*1024)

// max. number of entry messages
#define MAX_ENTRY_MSG_QUEUE 256

/***************************** Datatypes *******************************/

// compare info
typedef struct
{
  StorageSpecifier    *storageSpecifier;                  // storage specifier structure
  FragmentList        *fragmentList;
  const EntryList     *includeEntryList;                  // list of included entries
  const PatternList   *excludePatternList;                // list of exclude patterns
  DeltaSourceList     *deltaSourceList;                   // delta sources
  const JobOptions    *jobOptions;
  GetPasswordFunction getPasswordFunction;
  void                *getPasswordUserData;
  LogHandle           *logHandle;                         // log handle

  bool                *pauseTestFlag;                     // TRUE for pause creation
  bool                *requestedAbortFlag;                // TRUE to abort create

  MsgQueue            entryMsgQueue;                      // queue with entries to store

  Errors              failError;                          // failure error
} CompareInfo;

// entry message send to compare threads
typedef struct
{
  const StorageInfo *storageInfo;
  byte              cryptSalt[CRYPT_SALT_LENGTH];
  uint              cryptMode;
  ArchiveEntryTypes archiveEntryType;
  uint64            offset;
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
* Name   : initCompareInfo
* Purpose: initialize compare info
* Input  : compareInfo                - compare info variable
*          storageSpecifier           - storage specifier structure
*          includeEntryList           - include entry list
*          excludePatternList         - exclude pattern list
*          deltaSourceList            - delta source list
*          jobOptions                 - job options
*          pauseTestFlag              - pause creation flag (can be
*                                       NULL)
*          requestedAbortFlag         - request abort flag (can be NULL)
*          logHandle                  - log handle (can be NULL)
* Output : createInfo - initialized compare info variable
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initCompareInfo(CompareInfo       *compareInfo,
                           FragmentList      *fragmentList,
                           StorageSpecifier  *storageSpecifier,
                           const EntryList   *includeEntryList,
                           const PatternList *excludePatternList,
                           DeltaSourceList   *deltaSourceList,
                           const JobOptions  *jobOptions,
                           bool              *pauseTestFlag,
                           bool              *requestedAbortFlag,
                           LogHandle         *logHandle
                          )
{
  assert(compareInfo != NULL);

  // init variables
  compareInfo->fragmentList       = fragmentList;
  compareInfo->storageSpecifier   = storageSpecifier;
  compareInfo->includeEntryList   = includeEntryList;
  compareInfo->excludePatternList = excludePatternList;
  compareInfo->deltaSourceList    = deltaSourceList;
  compareInfo->jobOptions         = jobOptions;
  compareInfo->pauseTestFlag      = pauseTestFlag;
  compareInfo->requestedAbortFlag = requestedAbortFlag;
  compareInfo->logHandle          = logHandle;
  compareInfo->failError          = ERROR_NONE;

  // init entry name queue, storage queue
  if (!MsgQueue_init(&compareInfo->entryMsgQueue,MAX_ENTRY_MSG_QUEUE))
  {
    HALT_FATAL_ERROR("Cannot initialize entry message queue!");
  }

#if 0
  // init locks
  if (!Semaphore_init(&compareInfo->storageInfoLock))
  {
    HALT_FATAL_ERROR("Cannot initialize storage semaphore!");
  }
#endif

  DEBUG_ADD_RESOURCE_TRACE(compareInfo,sizeof(CompareInfo));
}

/***********************************************************************\
* Name   : doneCompareInfo
* Purpose: deinitialize compare info
* Input  : compareInfo - compare info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneCompareInfo(CompareInfo *compareInfo)
{
  assert(compareInfo != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(compareInfo,sizeof(CompareInfo));

  MsgQueue_done(&compareInfo->entryMsgQueue,(MsgQueueMsgFreeFunction)freeEntryMsg,NULL);
}

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

/***********************************************************************\
* Name   : compareFileEntry
* Purpose: compare a file entry in archive
* Input  : archiveHandle        - archive handle
*          offset               - offset
*          includeEntryList     - include entry list
*          excludePatternList   - exclude pattern list
*          printableStorageName - printable storage name
*          jobOptions           - job options
*          fragmentList         - fragment list
*          buffer0,buffer1      - buffers for temporary data
*          bufferSize           - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors compareFileEntry(ArchiveHandle     *archiveHandle,
                              const EntryList   *includeEntryList,
                              const PatternList *excludePatternList,
                              const char        *printableStorageName,
                              const JobOptions  *jobOptions,
                              FragmentList      *fragmentList,
                              byte              *buffer0,
                              byte              *buffer1,
                              uint              bufferSize
                             )
{
  Errors             error;
  ArchiveEntryInfo   archiveEntryInfo;
  CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
  String             fileName;
  FileInfo           fileInfo;
  uint64             fragmentOffset,fragmentSize;
//            FileInfo         localFileInfo;
  FileHandle         fileHandle;
  bool               equalFlag;
  uint64             length;
  ulong              bufferLength;
  ulong              diffIndex;
  SemaphoreLock      semaphoreLock;
  FragmentNode       *fragmentNode;

  // read file
  fileName = String_new();
  error = Archive_readFileEntry(&archiveEntryInfo,
                                archiveHandle,
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
               printableStorageName,
               Error_getText(error)
              );
    String_delete(fileName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }

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
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return ERROR_FILE_NOT_FOUND_;
    }
    if (File_getType(fileName) != FILE_TYPE_FILE)
    {
      printInfo(1,"FAIL!\n");
      printError("'%s' is not a file!\n",String_cString(fileName));
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return ERROR_WRONG_ENTRY_TYPE;
    }

    // open file
    error = File_open(&fileHandle,fileName,FILE_OPEN_READ|FILE_OPEN_NO_ATIME|FILE_OPEN_NO_CACHE);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL!\n");
      printError("Cannot open file '%s' (error: %s)\n",
                 String_cString(fileName),
                 Error_getText(error)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return error;
    }
    DEBUG_TESTCODE() { (void)File_close(&fileHandle); Archive_closeEntry(&archiveEntryInfo); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }

    // check file size
    if (fileInfo.size != File_getSize(&fileHandle))
    {
      printInfo(1,"FAIL!\n");
      printError("'%s' differ in size: expected %lld bytes, found %lld bytes\n",
                 String_cString(fileName),
                 fileInfo.size,
                 File_getSize(&fileHandle)
                );
      File_close(&fileHandle);
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return ERROR_ENTRIES_DIFFER;
    }

    // seek to fragment position
    error = File_seek(&fileHandle,fragmentOffset);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL!\n");
      printError("Cannot read file '%s' (error: %s)\n",
                 String_cString(fileName),
                 Error_getText(error)
                );
      File_close(&fileHandle);
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return error;
    }
    DEBUG_TESTCODE() { (void)File_close(&fileHandle); Archive_closeEntry(&archiveEntryInfo); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }

    // compare archive and file content
    length    = 0LL;
    equalFlag = TRUE;
    diffIndex = 0L;
    while (   (length < fragmentSize)
           && equalFlag
          )
    {
      bufferLength = (ulong)MIN(fragmentSize-length,bufferSize);

      // read archive, file
      error = Archive_readData(&archiveEntryInfo,buffer0,bufferLength);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("Cannot read content of archive '%s' (error: %s)!\n",
                   printableStorageName,
                   Error_getText(error)
                  );
        break;
      }
      DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }
      error = File_read(&fileHandle,buffer1,bufferLength,NULL);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("Cannot read file '%s' (error: %s)\n",
                   String_cString(fileName),
                   Error_getText(error)
                  );
        break;
      }
      DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

      // compare
      diffIndex = compare(buffer0,buffer1,bufferLength);
      equalFlag = (diffIndex >= bufferLength);
      if (!equalFlag)
      {
        error = ERROR_ENTRIES_DIFFER;

        printInfo(1,"FAIL!\n");
        printError("'%s' differ at offset %llu\n",
                   String_cString(fileName),
                   fragmentOffset+length+(uint64)diffIndex
                  );
        break;
      }

      length += (uint64)bufferLength;

      printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
    }
    if (error != ERROR_NONE)
    {
      File_close(&fileHandle);
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return error;
    }
    DEBUG_TESTCODE() { File_close(&fileHandle); Archive_closeEntry(&archiveEntryInfo); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }

    printInfo(2,"    \b\b\b\b");

    // close file
    File_close(&fileHandle);

    if (!jobOptions->noFragmentsCheckFlag)
    {
      SEMAPHORE_LOCKED_DO(semaphoreLock,&fragmentList->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get file fragment node
        fragmentNode = FragmentList_find(fragmentList,fileName);
        if (fragmentNode == NULL)
        {
          fragmentNode = FragmentList_add(fragmentList,fileName,fileInfo.size,NULL,0);
        }
        assert(fragmentNode != NULL);
//FragmentList_print(fragmentNode,String_cString(fileName));

        // add fragment to file fragment list
        FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);

        // discard fragment list if file is complete
        if (FragmentList_isEntryComplete(fragmentNode))
        {
          FragmentList_discard(fragmentList,fragmentNode);
        }
      }
    }

#if 0
    // get local file info
    // check file time, permissions, file owner/group
#endif /* 0 */
    printInfo(1,"OK\n");

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
    printWarning("close 'file' entry fail (error: %s)\n",Error_getText(error));
  }

  // free resources
  String_delete(fileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : compareImageEntry
* Purpose: compare a image entry in archive
* Input  : archiveHandle        - archive handle
*          offset               - offset
*          printableStorageName - printable storage name
*          jobOptions           - job options
*          fragmentList         - fragment list
*          buffer0,buffer1      - buffers for temporary data
*          bufferSize           - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors compareImageEntry(ArchiveHandle     *archiveHandle,
                               const EntryList   *includeEntryList,
                               const PatternList *excludePatternList,
                               const char        *printableStorageName,
                               const JobOptions  *jobOptions,
                               FragmentList      *fragmentList,
                               byte              *buffer0,
                               byte              *buffer1,
                               uint              bufferSize
                              )
{
  Errors             error;
  ArchiveEntryInfo   archiveEntryInfo;
  CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
  String             deviceName;
  DeviceInfo         deviceInfo;
  uint64             blockOffset,blockCount;
  DeviceHandle       deviceHandle;
  bool               fileSystemFlag;
  FileSystemHandle   fileSystemHandle;
  bool               equalFlag;
  uint64             block;
  ulong              diffIndex;
  SemaphoreLock      semaphoreLock;
  FragmentNode       *fragmentNode;

  // read image
  deviceName = String_new();
  error = Archive_readImageEntry(&archiveEntryInfo,
                                 archiveHandle,
                                 &deltaCompressAlgorithm,
                                 &byteCompressAlgorithm,
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
               printableStorageName,
               Error_getText(error)
              );
    String_delete(deviceName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }
  if (deviceInfo.blockSize > bufferSize)
  {
    printError("Device block size %llu on '%s' is too big (max: %llu)\n",
               deviceInfo.blockSize,
               String_cString(deviceName),
               bufferSize
              );
    String_delete(deviceName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }
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
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(deviceName);
      return error;
    }

    // get device info
    error = Device_getDeviceInfo(&deviceInfo,deviceName);
    if (error != ERROR_NONE)
    {
      (void)Archive_closeEntry(&archiveEntryInfo);
      if (jobOptions->skipUnreadableFlag)
      {
        printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
        String_delete(deviceName);
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError("Cannot open device '%s' (error: %s)\n",
                   String_cString(deviceName),
                   Error_getText(error)
                  );
        String_delete(deviceName);
        return error;
      }
    }
    DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }

    // check device block size, get max. blocks in buffer
    if (deviceInfo.blockSize > bufferSize)
    {
      printInfo(1,"FAIL\n");
      printError("Device block size %llu on '%s' is too big (max: %llu)\n",
                 deviceInfo.blockSize,
                 String_cString(deviceName),
                 bufferSize
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(deviceName);
      return ERROR_INVALID_DEVICE_BLOCK_SIZE;
    }
    assert(deviceInfo.blockSize > 0);

    // open device
    error = Device_open(&deviceHandle,deviceName,DEVICE_OPEN_READ);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL!\n");
      printError("Cannot open file '%s' (error: %s)\n",
                 String_cString(deviceName),
                 Error_getText(error)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(deviceName);
      return error;
    }
    DEBUG_TESTCODE() { Device_close(&deviceHandle); Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }

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
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(deviceName);
      return ERROR_ENTRIES_DIFFER;
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
                 Error_getText(error)
                );
      Device_close(&deviceHandle);
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(deviceName);
      return error;
    }
    DEBUG_TESTCODE() { Device_close(&deviceHandle); Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }

    // compare archive and device/image content
    block     = 0LL;
    equalFlag = TRUE;
    diffIndex = 0L;
    while (   (block < blockCount)
           && equalFlag
          )
    {
      // read data from archive (only single block)
      error = Archive_readData(&archiveEntryInfo,buffer0,deviceInfo.blockSize);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("Cannot read content of archive '%s' (error: %s)!\n",
                   printableStorageName,
                   Error_getText(error)
                  );
        break;
      }
      DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

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
                     Error_getText(error)
                    );
          break;
        }
        DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

        // read data from device/image
        error = Device_read(&deviceHandle,buffer1,deviceInfo.blockSize,NULL);
        if (error != ERROR_NONE)
        {
          printInfo(1,"FAIL!\n");
          printError("Cannot read device '%s' (error: %s)\n",
                     String_cString(deviceName),
                     Error_getText(error)
                    );
          break;
        }
        DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

        // compare
        diffIndex = compare(buffer0,buffer1,deviceInfo.blockSize);
        equalFlag = (diffIndex >= deviceInfo.blockSize);
        if (!equalFlag)
        {
          error = ERROR_ENTRIES_DIFFER;

          printInfo(1,"FAIL!\n");
          printError("'%s' differ at offset %llu\n",
                     String_cString(deviceName),
                     blockOffset*(uint64)deviceInfo.blockSize+block*(uint64)deviceInfo.blockSize+(uint64)diffIndex
                    );
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
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(deviceName);
      return error;
    }
    DEBUG_TESTCODE() { if (fileSystemFlag) { FileSystem_done(&fileSystemHandle); } Device_close(&deviceHandle); Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }
    printInfo(2,"    \b\b\b\b");

    if (!jobOptions->noFragmentsCheckFlag)
    {
      SEMAPHORE_LOCKED_DO(semaphoreLock,&fragmentList->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get image fragment node
        fragmentNode = FragmentList_find(fragmentList,deviceName);
        if (fragmentNode == NULL)
        {
          fragmentNode = FragmentList_add(fragmentList,deviceName,deviceInfo.size,NULL,0);
        }
        assert(fragmentNode != NULL);

        // add fragment to file fragment list
        FragmentList_addEntry(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize);

        // discard fragment list if file is complete
        if (FragmentList_isEntryComplete(fragmentNode))
        {
          FragmentList_discard(fragmentList,fragmentNode);
        }
      }
    }

    printInfo(1,"OK\n",
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
    printWarning("close 'image' entry fail (error: %s)\n",Error_getText(error));
    // ignore error
  }

  // free resources
  String_delete(deviceName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : compareDirectoryEntry
* Purpose: compare a directory entry in archive
* Input  : archiveHandle        - archive handle
*          offset               - offset
*          printableStorageName - printable storage name
*          jobOptions           - job options
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors compareDirectoryEntry(ArchiveHandle     *archiveHandle,
                                   const EntryList   *includeEntryList,
                                   const PatternList *excludePatternList,
                                   const char        *printableStorageName,
                                   const JobOptions  *jobOptions
                                  )
{
  Errors           error;
  ArchiveEntryInfo archiveEntryInfo;
  String           directoryName;
  FileInfo         fileInfo;
//            String   localFileName;
//            FileInfo localFileInfo;

  UNUSED_VARIABLE(jobOptions);

  // read directory
  directoryName = String_new();
  error = Archive_readDirectoryEntry(&archiveEntryInfo,
                                     archiveHandle,
                                     NULL,  // cryptAlgorithm
                                     NULL,  // cryptType
                                     directoryName,
                                     &fileInfo,
                                     NULL   // fileExtendedAttributeList
                                    );
  if (error != ERROR_NONE)
  {
    printError("Cannot read 'directory' content of archive '%s' (error: %s)!\n",
               printableStorageName,
               Error_getText(error)
              );
    String_delete(directoryName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); String_delete(directoryName); return DEBUG_TESTCODE_ERROR(); }

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
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(directoryName);
      return error;
    }
    if (File_getType(directoryName) != FILE_TYPE_DIRECTORY)
    {
      printInfo(1,"FAIL!\n");
      printError("'%s' is not a directory!\n",
                 String_cString(directoryName)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(directoryName);
      return error;
    }

#if 0
    // get local file info
    error = File_getFileInfo(directoryName,&localFileInfo);
    if (error != ERROR_NONE)
    {
      printError("Cannot read local directory '%s' (error: %s)!\n",
                 String_cString(directoryName),
                 Error_getText(error)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(directoryName);
      return error;
    }
    D EBUG_TESTCODE("Command_compare301") { Archive_closeEntry(&archiveEntryInfo); String_delete(directoryName); return DEBUG_TESTCODE_ERROR(); }

    // check file time, permissions, file owner/group
#endif /* 0 */
    printInfo(1,"OK\n");

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
    printWarning("close 'directory' entry fail (error: %s)\n",Error_getText(error));
  }

  // free resources
  String_delete(directoryName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : compareLinkEntry
* Purpose: compare a link entry in archive
* Input  : archiveHandle        - archive handle
*          offset               - offset
*          printableStorageName - printable storage name
*          jobOptions           - job options
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors compareLinkEntry(ArchiveHandle     *archiveHandle,
                              const EntryList   *includeEntryList,
                              const PatternList *excludePatternList,
                              const char        *printableStorageName,
                              const JobOptions  *jobOptions
                             )
{
  Errors           error;
  ArchiveEntryInfo archiveEntryInfo;
  String           linkName;
  String           fileName;
  FileInfo         fileInfo;
  String           localFileName;
//                    FileInfo localFileInfo;

  UNUSED_VARIABLE(jobOptions);

  // read link
  linkName = String_new();
  fileName = String_new();
  error = Archive_readLinkEntry(&archiveEntryInfo,
                                archiveHandle,
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
               printableStorageName,
               Error_getText(error)
              );
    String_delete(fileName);
    String_delete(linkName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); String_delete(fileName); String_delete(linkName); return DEBUG_TESTCODE_ERROR(); }

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
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      String_delete(linkName);
      return error;
    }
    if (File_getType(linkName) != FILE_TYPE_LINK)
    {
      printInfo(1,"FAIL!\n");
      printError("'%s' is not a link!\n",
                 String_cString(linkName)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      String_delete(linkName);
      return error;
    }

    // check link name
    localFileName = String_new();
    error = File_readLink(localFileName,linkName);
    if (error != ERROR_NONE)
    {
      printError("Cannot read local file '%s' (error: %s)!\n",
                 String_cString(linkName),
                 Error_getText(error)
                );
      String_delete(localFileName);
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      String_delete(linkName);
      return error;
    }
    DEBUG_TESTCODE() { String_delete(localFileName); Archive_closeEntry(&archiveEntryInfo); String_delete(fileName);  String_delete(linkName);return DEBUG_TESTCODE_ERROR(); }
    if (!String_equals(fileName,localFileName))
    {
      printInfo(1,"FAIL!\n");
      printError("Link '%s' does not contain file '%s'!\n",
                 String_cString(linkName),
                 String_cString(fileName)
                );
      String_delete(localFileName);
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      String_delete(linkName);
      return error;
    }
    String_delete(localFileName);

#if 0
    // get local file info
    error = File_getFileInfo(linkName,&localFileInfo);
    if (error != ERROR_NONE)
    {
      printError("Cannot read local file '%s' (error: %s)!\n",
                 String_cString(linkName),
                 Error_getText(error)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      String_delete(linkName);
      return error;
    }
    D EBUG_TESTCODE("Command_compare403") { Archive_closeEntry(&archiveEntryInfo); String_delete(fileName); String_delete(linkName); return DEBUG_TESTCODE_ERROR(); }

    // check file time, permissions, file owner/group
#endif /* 0 */
    printInfo(1,"OK\n");

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
    printWarning("close 'link' entry fail (error: %s)\n",Error_getText(error));
  }

  // free resources
  String_delete(fileName);
  String_delete(linkName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : compareHardLinkEntry
* Purpose: compare a hardlink entry in archive
* Input  : archiveHandle        - archive handle
*          offset               - offset
*          printableStorageName - printable storage name
*          jobOptions           - job options
*          fragmentList         - fragment list
*          buffer0,buffer1      - buffers for temporary data
*          bufferSize           - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors compareHardLinkEntry(ArchiveHandle     *archiveHandle,
                                  const EntryList   *includeEntryList,
                                  const PatternList *excludePatternList,
                                  const char        *printableStorageName,
                                  const JobOptions  *jobOptions,
                                  FragmentList      *fragmentList,
                                  byte              *buffer0,
                                  byte              *buffer1,
                                  uint              bufferSize
                                 )
{
  Errors             error;
  ArchiveEntryInfo   archiveEntryInfo;
  CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
  StringList         fileNameList;
  FileInfo           fileInfo;
  uint64             fragmentOffset,fragmentSize;
  bool               comparedDataFlag;
  const StringNode   *stringNode;
  String             fileName;
//            FileInfo         localFileInfo;
  FileHandle         fileHandle;
  bool               equalFlag;
  uint64             length;
  ulong              bufferLength;
  ulong              diffIndex;
  SemaphoreLock      semaphoreLock;
  FragmentNode       *fragmentNode;

  // read hard link
  StringList_init(&fileNameList);
  error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                    archiveHandle,
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
               printableStorageName,
               Error_getText(error)
              );
    StringList_done(&fileNameList);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); StringList_done(&fileNameList); return DEBUG_TESTCODE_ERROR(); }

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
        if (!jobOptions->noStopOnErrorFlag)
        {
          error = ERROR_FILE_NOT_FOUND_;
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
        if (!jobOptions->noStopOnErrorFlag)
        {
          error = ERROR_WRONG_ENTRY_TYPE;
          break;
        }
        else
        {
          continue;
        }
      }

      if (!comparedDataFlag && (error == ERROR_NONE))
      {
        // compare hard link data

        // open file
        error = File_open(&fileHandle,fileName,FILE_OPEN_READ|FILE_OPEN_NO_ATIME|FILE_OPEN_NO_CACHE);
        if (error != ERROR_NONE)
        {
          printInfo(1,"FAIL!\n");
          printError("Cannot open file '%s' (error: %s)\n",
                     String_cString(fileName),
                     Error_getText(error)
                    );
          if (!jobOptions->noStopOnErrorFlag)
          {
            break;
          }
          else
          {
            continue;
          }
        }
        DEBUG_TESTCODE() { (void)File_close(&fileHandle); error = DEBUG_TESTCODE_ERROR(); break; }

        // check file size
        if (fileInfo.size != File_getSize(&fileHandle))
        {
          printInfo(1,"FAIL!\n");
          printError("'%s' differ in size: expected %lld bytes, found %lld bytes\n",
                     String_cString(fileName),
                     fileInfo.size,
                     File_getSize(&fileHandle)
                    );
          File_close(&fileHandle);
          if (!jobOptions->noStopOnErrorFlag)
          {
            error = ERROR_ENTRIES_DIFFER;
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
                     Error_getText(error)
                    );
          File_close(&fileHandle);
          if (!jobOptions->noStopOnErrorFlag)
          {
            break;
          }
          else
          {
            continue;
          }
        }
        DEBUG_TESTCODE() { (void)File_close(&fileHandle); error = DEBUG_TESTCODE_ERROR(); break; }

        // compare archive and hard link content
        length    = 0LL;
        equalFlag = TRUE;
        diffIndex = 0L;
        while (   (length < fragmentSize)
               && equalFlag
              )
        {
          bufferLength = (ulong)MIN(fragmentSize-length,bufferSize);

          // read archive, file
          error = Archive_readData(&archiveEntryInfo,buffer0,bufferLength);
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL!\n");
            printError("Cannot read content of archive '%s' (error: %s)!\n",
                       printableStorageName,
                       Error_getText(error)
                      );
            break;
          }
          DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }
          error = File_read(&fileHandle,buffer1,bufferLength,NULL);
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL!\n");
            printError("Cannot read file '%s' (error: %s)\n",
                       String_cString(fileName),
                       Error_getText(error)
                      );
            break;
          }
          DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }

          // compare
          diffIndex = compare(buffer0,buffer1,bufferLength);
          equalFlag = (diffIndex >= bufferLength);
          if (!equalFlag)
          {
            error = ERROR_ENTRIES_DIFFER;

            printInfo(1,"FAIL!\n");
            printError("'%s' differ at offset %llu\n",
                       String_cString(fileName),
                       fragmentOffset+length+(uint64)diffIndex
                      );
            break;
          }

          length += (uint64)bufferLength;

          printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
        }
        if (error != ERROR_NONE)
        {
          (void)File_close(&fileHandle);
          return error;
        }
        printInfo(2,"    \b\b\b\b");

        // close file
        (void)File_close(&fileHandle);

        if (!jobOptions->noFragmentsCheckFlag)
        {
          SEMAPHORE_LOCKED_DO(semaphoreLock,&fragmentList->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
          {
            // get file fragment list
            fragmentNode = FragmentList_find(fragmentList,fileName);
            if (fragmentNode == NULL)
            {
              fragmentNode = FragmentList_add(fragmentList,fileName,fileInfo.size,NULL,0);
            }
            assert(fragmentNode != NULL);

            // add fragment to file fragment list
            FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);

            // discard fragment list if file is complete
            if (FragmentList_isEntryComplete(fragmentNode))
            {
              FragmentList_discard(fragmentList,fragmentNode);
            }
          }
        }
#if 0
        // get local file info
        // check file time, permissions, file owner/group
#endif /* 0 */
        printInfo(1,"OK\n");

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
        if (error == ERROR_NONE)
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
      printInfo(2,"  Compare '%s'...skipped\n",String_cString(fileName));
    }
  }

  // close archive file, free resources
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'hard link' entry fail (error: %s)\n",Error_getText(error));
  }

  // free resources
  StringList_done(&fileNameList);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : compareSpecialEntry
* Purpose: compare a special entry in archive
* Input  : archiveHandle        - archive handle
*          offset               - offset
*          printableStorageName - printable storage name
*          jobOptions           - job options
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors compareSpecialEntry(ArchiveHandle     *archiveHandle,
                                 const EntryList   *includeEntryList,
                                 const PatternList *excludePatternList,
                                 const char        *printableStorageName,
                                 const JobOptions  *jobOptions
                                )
{
  Errors           error;
  ArchiveEntryInfo archiveEntryInfo;
  String           fileName;
  FileInfo         fileInfo;
  FileInfo         localFileInfo;

  UNUSED_VARIABLE(jobOptions);

  // read special
  fileName = String_new();
  error = Archive_readSpecialEntry(&archiveEntryInfo,
                                   archiveHandle,
                                   NULL,  // cryptAlgorithm
                                   NULL,  // cryptType
                                   fileName,
                                   &fileInfo,
                                   NULL   // fileExtendedAttributeList
                                  );
  if (error != ERROR_NONE)
  {
    printError("Cannot read 'special' content of archive '%s' (error: %s)!\n",
               printableStorageName,
               Error_getText(error)
              );
    String_delete(fileName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }

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
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return error;
    }
    if (File_getType(fileName) != FILE_TYPE_SPECIAL)
    {
      printInfo(1,"FAIL!\n");
      printError("'%s' is not a special device!\n",
                 String_cString(fileName)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return error;
    }

    // check special settings
    error = File_getFileInfo(fileName,&localFileInfo);
    if (error != ERROR_NONE)
    {
      printError("Cannot read local file '%s' (error: %s)!\n",
                 String_cString(fileName),
                 Error_getText(error)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return error;
    }
    DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); String_delete(fileName); return DEBUG_TESTCODE_ERROR(); }
    if (fileInfo.specialType != localFileInfo.specialType)
    {
      printError("Different types of special device '%s'!\n",
                 String_cString(fileName)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return error;
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
        (void)Archive_closeEntry(&archiveEntryInfo);
        String_delete(fileName);
        return error;
      }
      if (fileInfo.minor != localFileInfo.minor)
      {
        printError("Different minor numbers of special device '%s'!\n",
                   String_cString(fileName)
                  );
        (void)Archive_closeEntry(&archiveEntryInfo);
        String_delete(fileName);
        return error;
      }
    }

#if 0

    // check file time, permissions, file owner/group
#endif /* 0 */

    printInfo(1,"OK\n");

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
    printWarning("close 'special' entry fail (error: %s)\n",Error_getText(error));
  }

  // free resources
  String_delete(fileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : compareThreadCode
* Purpose: compare worker thread
* Input  : compareInfo - compare info structure
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void compareThreadCode(CompareInfo *compareInfo)
{
  String        printableStorageName;
  byte          *buffer0,*buffer1;
  ArchiveHandle archiveHandle;
  EntryMsg      entryMsg;
  Errors        error;

  assert(compareInfo != NULL);

  // init variables
  printableStorageName = String_new();
  buffer0 = (byte*)malloc(BUFFER_SIZE);
  if (buffer0 == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  buffer1 = (byte*)malloc(BUFFER_SIZE);
  if (buffer1 == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // get printable storage name
  Storage_getPrintableName(printableStorageName,compareInfo->storageSpecifier,NULL);

  // compare entries
  while (   (compareInfo->failError == ERROR_NONE)
//TODO
//         && !isAborted(compareInfo)
         && MsgQueue_get(&compareInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER)
        )
  {
//fprintf(stderr,"%s, %d: %p %d %llu\n",__FILE__,__LINE__,pthread_self(),entryMsg.archiveEntryType,entryMsg.offset);
//TODO: open only when changed
    // open archive
    error = Archive_open(&archiveHandle,
                         entryMsg.storageInfo,
                         NULL,  // fileName,
                         compareInfo->deltaSourceList,
                         compareInfo->jobOptions,
                         compareInfo->getPasswordFunction,
                         compareInfo->getPasswordUserData,
                         compareInfo->logHandle
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot open storage '%s' (error: %s)!\n",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      if (compareInfo->failError == ERROR_NONE) compareInfo->failError = error;
      break;
    }

    // set crypt salt and crypt mode
    Archive_setCryptSalt(&archiveHandle,entryMsg.cryptSalt,sizeof(entryMsg.cryptSalt));
    Archive_setCryptMode(&archiveHandle,entryMsg.cryptMode);

    // seek to start of entry
    error = Archive_seek(&archiveHandle,entryMsg.offset);
    if (error != ERROR_NONE)
    {
      printError("Cannot read storage '%s' (error: %s)!\n",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      if (compareInfo->failError == ERROR_NONE) compareInfo->failError = error;
      break;
    }

    switch (entryMsg.archiveEntryType)
    {
      case ARCHIVE_ENTRY_TYPE_FILE:
        error = compareFileEntry(&archiveHandle,
                                 compareInfo->includeEntryList,
                                 compareInfo->excludePatternList,
                                 String_cString(printableStorageName),
                                 compareInfo->jobOptions,
                                 compareInfo->fragmentList,
                                 buffer0,
                                 buffer1,
                                 BUFFER_SIZE
                                );
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        error = compareImageEntry(&archiveHandle,
                                  compareInfo->includeEntryList,
                                  compareInfo->excludePatternList,
                                  String_cString(printableStorageName),
                                  compareInfo->jobOptions,
                                  compareInfo->fragmentList,
                                  buffer0,
                                  buffer1,
                                  BUFFER_SIZE
                                 );
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        error = compareDirectoryEntry(&archiveHandle,
                                      compareInfo->includeEntryList,
                                      compareInfo->excludePatternList,
                                      String_cString(printableStorageName),
                                      compareInfo->jobOptions
                                     );
        break;
      case ARCHIVE_ENTRY_TYPE_LINK:
        error = compareLinkEntry(&archiveHandle,
                                 compareInfo->includeEntryList,
                                 compareInfo->excludePatternList,
                                 String_cString(printableStorageName),
                                 compareInfo->jobOptions
                                );
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        error = compareHardLinkEntry(&archiveHandle,
                                     compareInfo->includeEntryList,
                                     compareInfo->excludePatternList,
                                     String_cString(printableStorageName),
                                     compareInfo->jobOptions,
                                     compareInfo->fragmentList,
                                     buffer0,
                                     buffer1,
                                     BUFFER_SIZE
                                    );
        break;
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
        error = compareSpecialEntry(&archiveHandle,
                                    compareInfo->includeEntryList,
                                    compareInfo->excludePatternList,
                                    String_cString(printableStorageName),
                                    compareInfo->jobOptions
                                   );
        break;
      case ARCHIVE_ENTRY_TYPE_META:
        error = Archive_skipNextEntry(&archiveHandle);
        break;
      case ARCHIVE_ENTRY_TYPE_SIGNATURE:
        error = Archive_skipNextEntry(&archiveHandle);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; /* not reached */
    }
    if (error != ERROR_NONE)
    {
      if (!compareInfo->jobOptions->noStopOnErrorFlag)
      {
        if (compareInfo->failError == ERROR_NONE) compareInfo->failError = error;
      }
    }

    // close archive
    Archive_close(&archiveHandle);

    freeEntryMsg(&entryMsg,NULL);
  }
  if (!isPrintInfo(1)) printInfo(0,"%s",(compareInfo->failError == ERROR_NONE) ? "OK\n" : "FAIL!\n");

  // free resources
  free(buffer1);
  free(buffer0);
  String_delete(printableStorageName);
}

/***********************************************************************\
* Name   : compareArchiveContent
* Purpose: compare archive content
* Input  : storageSpecifier    - storage specifier
*          fileName            - file name
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

LOCAL Errors compareArchiveContent(StorageSpecifier    *storageSpecifier,
                                   ConstString         fileName,
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
  String               printableStorageName;
  Thread               *compareThreads;
  uint                 compareThreadCount;
  StorageInfo          storageInfo;
  Errors               error;
  CryptSignatureStates allCryptSignatureState;
  CompareInfo          compareInfo;
  uint                 i;
  Errors               failError;
  ArchiveHandle        archiveHandle;
  ArchiveEntryTypes    archiveEntryType;
  uint64               offset;
  EntryMsg             entryMsg;

  assert(storageSpecifier != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(jobOptions != NULL);
  assert(fragmentList != NULL);

  // init variables
  printableStorageName = String_new();
  compareThreadCount   = (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores();
  compareThreads = (Thread*)malloc(compareThreadCount*sizeof(Thread));
  if (compareThreads == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // get printable storage name
  Storage_getPrintableName(printableStorageName,storageSpecifier,fileName);

  // init storage
  error = Storage_init(&storageInfo,
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
               String_cString(printableStorageName),
               Error_getText(error)
              );
    free(compareThreads);
    String_delete(printableStorageName);
    return error;
  }

  // check signatures
  if (!jobOptions->skipVerifySignaturesFlag)
  {
    error = Archive_verifySignatures(&storageInfo,
                                     fileName,
                                     jobOptions,
                                     &allCryptSignatureState
                                    );
    if (error != ERROR_NONE)
    {
      (void)Storage_done(&storageInfo);
      free(compareThreads);
      String_delete(printableStorageName);
      return error;
    }
    if (!Crypt_isValidSignatureState(allCryptSignatureState))
    {
      printError("Invalid signature in '%s'!\n",
                 String_cString(printableStorageName)
                );
      (void)Storage_done(&storageInfo);
      free(compareThreads);
      String_delete(printableStorageName);
      return ERROR_INVALID_SIGNATURE;
    }
  }

  // init compare info
  initCompareInfo(&compareInfo,
                  fragmentList,
                  storageSpecifier,
                  includeEntryList,
                  excludePatternList,
                  deltaSourceList,
                  jobOptions,
//TODO
NULL,  //               pauseTestFlag,
NULL,  //               requestedAbortFlag,
                  logHandle
                 );

  // start compare threads
  for (i = 0; i < compareThreadCount; i++)
  {
    if (!Thread_init(&compareThreads[i],"BAR compare",globalOptions.niceLevel,compareThreadCode,&compareInfo))
    {
      HALT_FATAL_ERROR("Cannot initialize comparethread #%d!",i);
    }
  }

  // open archive
  error = Archive_open(&archiveHandle,
                       &storageInfo,
                       fileName,
                       deltaSourceList,
                       jobOptions,
                       getPasswordFunction,
                       getPasswordUserData,
                       logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot open storage '%s' (error: %s)!\n",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    (void)Storage_done(&storageInfo);
    doneCompareInfo(&compareInfo);
    free(compareThreads);
    String_delete(printableStorageName);
    return error;
  }
  DEBUG_TESTCODE() { (void)Archive_close(&archiveHandle); (void)Storage_done(&storageInfo); return DEBUG_TESTCODE_ERROR(); }

  // read archive entries
  printInfo(0,
            "Compare storage '%s'%s",
            String_cString(printableStorageName),
            !isPrintInfo(1) ? "..." : ":\n"
           );
  failError = ERROR_NONE;
  while (   !Archive_eof(&archiveHandle,TRUE,isPrintInfo(3))
         && (failError == ERROR_NONE)
        )
  {
    // get next archive entry type
    error = Archive_getNextArchiveEntry(&archiveHandle,
                                        &archiveEntryType,
                                        &offset,
                                        TRUE,
                                        isPrintInfo(3)
                                       );
    if (error != ERROR_NONE)
    {
      printError("Cannot read next entry in archive '%s' (error: %s)!\n",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      break;
    }
    DEBUG_TESTCODE() { failError = DEBUG_TESTCODE_ERROR(); break; }

    // send entry to test threads
    entryMsg.storageInfo      = &storageInfo;
    memCopyFast(entryMsg.cryptSalt,sizeof(entryMsg.cryptSalt),archiveHandle.cryptSalt,sizeof(archiveHandle.cryptSalt));
    entryMsg.cryptMode        = archiveHandle.cryptMode;
    entryMsg.archiveEntryType = archiveEntryType;
    entryMsg.offset           = offset;
    if (!MsgQueue_put(&compareInfo.entryMsgQueue,&entryMsg,sizeof(entryMsg)))
    {
      HALT_INTERNAL_ERROR("Send message to test threads!");
    }

    // next entry
    error = Archive_skipNextEntry(&archiveHandle);
    if (error != ERROR_NONE)
    {
      if (failError == ERROR_NONE) failError = error;
      break;
    }
  }
  if (!isPrintInfo(1)) printInfo(0,"%s",(failError == ERROR_NONE) ? "OK\n" : "FAIL!\n");

  // close archive
  Archive_close(&archiveHandle);

  // wait for compare threads
  MsgQueue_setEndOfMsg(&compareInfo.entryMsgQueue);
  for (i = 0; i < compareThreadCount; i++)
  {
    if (!Thread_join(&compareThreads[i]))
    {
      HALT_INTERNAL_ERROR("Cannot stop compare thread #%d!",i);
    }
  }

  // done storage
  (void)Storage_done(&storageInfo);

  // done compare info
  doneCompareInfo(&compareInfo);

  // free resources
  free(compareThreads);
  String_delete(printableStorageName);

  return error;
}

/*---------------------------------------------------------------------*/

Errors Command_compare(const StringList    *storageNameList,
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

  // init variables
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
    DEBUG_TESTCODE() { failError = DEBUG_TESTCODE_ERROR(); break; }

    if (String_isEmpty(storageSpecifier.archivePatternString))
    {
      // compare archive content
      error = compareArchiveContent(&storageSpecifier,
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
    else
    {
      error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                        &storageSpecifier,
                                        NULL,  // pathName
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

            // compare archive content
            error = compareArchiveContent(&storageSpecifier,
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

        Storage_closeDirectoryList(&storageDirectoryListHandle);
      }
    }
    if (error != ERROR_NONE)
    {
      if (failError == ERROR_NONE) failError = error;
      continue;
    }

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
        if (failError == ERROR_NONE)
        {
          failError = ERRORX_(ENTRY_INCOMPLETE,0,"%s",String_cString(fragmentNode->name));;
        }
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
