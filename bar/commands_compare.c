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
#include <inttypes.h>
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
#include "common/cstrings.h"
#include "common/autofree.h"
#include "common/files.h"
#include "common/filesystems.h"
#include "common/fragmentlists.h"
#include "common/global.h"
#include "common/msgqueues.h"
#include "common/patternlists.h"
#include "common/patterns.h"
#include "common/stringlists.h"
#include "common/strings.h"

#include "bar.h"
#include "errors.h"
#include "entrylists.h"
#include "deltasourcelists.h"
#include "archive.h"
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
  const EntryList         *includeEntryList;                  // list of included entries
  const PatternList       *excludePatternList;                // list of exclude patterns
  JobOptions              *jobOptions;
  LogHandle               *logHandle;                         // log handle

  bool                    *pauseTestFlag;                     // TRUE for pause creation
  bool                    *requestedAbortFlag;                // TRUE to abort create

  GetNamePasswordFunction getNamePasswordFunction;
  void                    *getNamePasswordUserData;

  Semaphore               fragmentListLock;
  FragmentList            *fragmentList;

  MsgQueue                entryMsgQueue;                      // queue with entries to store

  Errors                  failError;                          // failure error
} CompareInfo;

// entry message send to compare threads
typedef struct
{
  uint                archiveIndex;
  const ArchiveHandle *archiveHandle;
  ArchiveEntryTypes   archiveEntryType;
  ArchiveCryptInfo    *archiveCryptInfo;
  uint64              offset;
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
* Input  : compareInfo             - compare info variable
*          includeEntryList        - include entry list
*          excludePatternList      - exclude pattern list
*          archiveHandle           - archive handle
*          pauseTestFlag           - pause creation flag (can be NULL)
*          requestedAbortFlag      - request abort flag (can be NULL)
*          getNamePasswordFunction - get password call back
*          getNamePasswordUserData - user data for get password
*          fragmentList            - fragment list
*          logHandle               - log handle (can be NULL)
* Output : createInfo - initialized compare info variable
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initCompareInfo(CompareInfo             *compareInfo,
                           const EntryList         *includeEntryList,
                           const PatternList       *excludePatternList,
                           JobOptions              *jobOptions,
                           bool                    *pauseTestFlag,
                           bool                    *requestedAbortFlag,
                           GetNamePasswordFunction getNamePasswordFunction,
                           void                    *getNamePasswordUserData,
                           FragmentList            *fragmentList,
                           LogHandle               *logHandle
                          )
{
  assert(compareInfo != NULL);

  // init variables
  compareInfo->includeEntryList        = includeEntryList;
  compareInfo->excludePatternList      = excludePatternList;
  compareInfo->jobOptions              = jobOptions;
  compareInfo->pauseTestFlag           = pauseTestFlag;
  compareInfo->requestedAbortFlag      = requestedAbortFlag;
  compareInfo->getNamePasswordFunction = getNamePasswordFunction;
  compareInfo->getNamePasswordUserData = getNamePasswordUserData;
  compareInfo->fragmentList            = fragmentList;
  compareInfo->logHandle               = logHandle;
  compareInfo->failError               = ERROR_NONE;

  // init entry name queue, storage queue
  if (!MsgQueue_init(&compareInfo->entryMsgQueue,
                     MAX_ENTRY_MSG_QUEUE,
                     CALLBACK_((MsgQueueMsgFreeFunction)freeEntryMsg,NULL)
                    )
     )
  {
    HALT_FATAL_ERROR("Cannot initialize entry message queue!");
  }

  // init locks
  if (!Semaphore_init(&compareInfo->fragmentListLock,SEMAPHORE_TYPE_BINARY))
  {
    HALT_FATAL_ERROR("Cannot initialize fragment list semaphore!");
  }

  DEBUG_ADD_RESOURCE_TRACE(compareInfo,CompareInfo);
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

  DEBUG_REMOVE_RESOURCE_TRACE(compareInfo,CompareInfo);

  Semaphore_done(&compareInfo->fragmentListLock);

  MsgQueue_done(&compareInfo->entryMsgQueue);
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
*          includeEntryList     - include entry list
*          excludePatternList   - exclude pattern list
*          printableStorageName - printable storage name
*          fragmentListLock     - fragment list lock
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
                              Semaphore         *fragmentListLock,
                              FragmentList      *fragmentList,
                              byte              *buffer0,
                              byte              *buffer1,
                              uint              bufferSize
                             )
{
  Errors             error;
  String             fileName;
  ArchiveEntryInfo   archiveEntryInfo;
  CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
  FileInfo           fileInfo;
  uint64             fragmentOffset,fragmentSize;
//            FileInfo         localFileInfo;
  FileHandle         fileHandle;
  bool               equalFlag;
  uint64             length;
  ulong              bufferLength;
  ulong              diffIndex;
  FragmentNode       *fragmentNode;
  char               sizeString[32];
  char               fragmentString[256];

  assert(archiveHandle != NULL);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(fragmentListLock != NULL);
  assert(fragmentList != NULL);
  assert(buffer0 != NULL);
  assert(buffer1 != NULL);

  // read file
  fileName = String_new();
  error = Archive_readFileEntry(&archiveEntryInfo,
                                archiveHandle,
                                &deltaCompressAlgorithm,
                                &byteCompressAlgorithm,
                                NULL,  // cryptType
                                NULL,  // cryptAlgorithm
                                NULL,  // cryptSalt
                                NULL,  // cryptKey
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
    printError("cannot read 'file' entry from storage '%s' (error: %s)!",
               String_cString(archiveHandle->printableStorageName),
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
    printInfo(1,"  Compare file      '%s'...",String_cString(fileName));

    // check if file exists and check file type
    if (!File_exists(fileName))
    {
      printInfo(1,"FAIL!\n");
      printError("file '%s' not found!",String_cString(fileName));
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return ERROR_FILE_NOT_FOUND_;
    }
    if (File_getType(fileName) != FILE_TYPE_FILE)
    {
      printInfo(1,"FAIL!\n");
      printError("'%s' is not a file!",String_cString(fileName));
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return ERROR_WRONG_ENTRY_TYPE;
    }

    // open file
    error = File_open(&fileHandle,fileName,FILE_OPEN_READ|FILE_OPEN_NO_ATIME|FILE_OPEN_NO_CACHE);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL!\n");
      printError("cannot open file '%s' (error: %s)",
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
      printError("'%s' differ in size: expected %"PRIu64" bytes, found %"PRIu64" bytes",
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
      printError("cannot read content of 'file' entry '%s' (error: %s)",
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
        printError("cannot read content of 'file' entry '%s' (error: %s)!",
                   String_cString(fileName),
                   Error_getText(error)
                  );
        break;
      }
      DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }
      error = File_read(&fileHandle,buffer1,bufferLength,NULL);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("cannot read file '%s' (error: %s)",
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
        printError("'%s' differ at offset %"PRIu64,
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

    if (!archiveHandle->storageInfo->jobOptions->noFragmentsCheckFlag)
    {
      SEMAPHORE_LOCKED_DO(fragmentListLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get file fragment node
        fragmentNode = FragmentList_find(fragmentList,fileName);
        if (fragmentNode == NULL)
        {
          fragmentNode = FragmentList_add(fragmentList,fileName,fileInfo.size,NULL,0,0);
        }
        assert(fragmentNode != NULL);
//FragmentList_print(fragmentNode,String_cString(fileName),FALSE);

        // add fragment to file fragment list
        FragmentList_addRange(fragmentNode,fragmentOffset,fragmentSize);

        // discard fragment list if file is complete
        if (FragmentList_isComplete(fragmentNode))
        {
          FragmentList_discard(fragmentList,fragmentNode);
        }
      }
    }

#if 0
    // get local file info
    // check file time, permissions, file owner/group
#endif /* 0 */

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
      error = ERRORX_(CORRUPT_DATA,0,"%s",String_cString(fileName));
      printInfo(1,"FAIL!\n");
      printError("unexpected data at end of file entry '%s'!",String_cString(fileName));
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return error;
    }

    // get size/fragment info
    if (globalOptions.humanFormatFlag)
    {
      getHumanSizeString(sizeString,sizeof(sizeString),fragmentSize);
    }
    else
    {
      stringFormat(sizeString,sizeof(sizeString),"%"PRIu64,fragmentSize);
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

    // output
    printInfo(1,"OK (%s bytes%s)\n",sizeString,fragmentString);

    // free resources
  }
  else
  {
    // skip
    printInfo(2,"  Compare file      '%s'...skipped\n",String_cString(fileName));
  }

  // close archive file
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'file' entry fail (error: %s)",Error_getText(error));
  }

  // free resources
  String_delete(fileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : compareImageEntry
* Purpose: compare a image entry in archive
* Input  : archiveHandle        - archive handle
*          includeEntryList     - include entry list
*          excludePatternList   - exclude pattern list
*          printableStorageName - printable storage name
*          fragmentListLock     - fragment list lock
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
                               Semaphore         *fragmentListLock,
                               FragmentList      *fragmentList,
                               byte              *buffer0,
                               byte              *buffer1,
                               uint              bufferSize
                              )
{
  Errors             error;
  String             deviceName;
  ArchiveEntryInfo   archiveEntryInfo;
  CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
  DeviceInfo         deviceInfo;
  uint64             blockOffset,blockCount;
  DeviceHandle       deviceHandle;
  bool               fileSystemFlag;
  FileSystemHandle   fileSystemHandle;
  bool               equalFlag;
  uint64             block;
  ulong              diffIndex;
  FragmentNode       *fragmentNode;
  char               sizeString[32];
  char               fragmentString[256];

  assert(archiveHandle != NULL);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(fragmentListLock != NULL);
  assert(fragmentList != NULL);
  assert(buffer0 != NULL);
  assert(buffer1 != NULL);

  // read image
  deviceName = String_new();
  error = Archive_readImageEntry(&archiveEntryInfo,
                                 archiveHandle,
                                 &deltaCompressAlgorithm,
                                 &byteCompressAlgorithm,
                                 NULL,  // cryptType
                                 NULL,  // cryptAlgorithm
                                 NULL,  // cryptSalt
                                 NULL,  // cryptKey
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
    printError("cannot read 'image' content from storage '%s' (error: %s)!",
               String_cString(archiveHandle->printableStorageName),
               Error_getText(error)
              );
    String_delete(deviceName);
    return error;
  }
  DEBUG_TESTCODE() { Archive_closeEntry(&archiveEntryInfo); String_delete(deviceName); return DEBUG_TESTCODE_ERROR(); }
  if (deviceInfo.blockSize > bufferSize)
  {
    printError("device block size %"PRIu64" on '%s' is too big (max: %"PRIu64")",
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
    printInfo(1,"  Compare image     '%s'...",String_cString(deviceName));

    // check if device/image exists
    if (!Device_exists(deviceName))
    {
      printInfo(1,"FAIL!\n");
      printError("device '%s' not found!",String_cString(deviceName));
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(deviceName);
      return error;
    }

    // get device info
    error = Device_getInfo(&deviceInfo,deviceName,TRUE);
    if (error != ERROR_NONE)
    {
      (void)Archive_closeEntry(&archiveEntryInfo);
      if (archiveHandle->storageInfo->jobOptions->skipUnreadableFlag)
      {
        printInfo(1,"skipped (reason: %s)\n",Error_getText(error));
        String_delete(deviceName);
        return ERROR_NONE;
      }
      else
      {
        printInfo(1,"FAIL\n");
        printError("cannot open device '%s' (error: %s)",
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
      printError("device block size %"PRIu64" on '%s' is too big (max: %"PRIu64")",
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
      printError("cannot open file '%s' (error: %s)",
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
      printError("'%s' differ in size: expected %"PRIu64" bytes, found %"PRIu64" bytes",
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
    if (!archiveHandle->storageInfo->jobOptions->rawImagesFlag)
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
      printError("cannot read device '%s' (error: %s)",
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
        printError("cannot read content from 'image' entry '%s' (error: %s)!",
                   String_cString(deviceName),
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
          printError("cannot read device '%s' (error: %s)",
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
          printError("cannot read device '%s' (error: %s)",
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
          printError("'%s' differ at offset %"PRIu64,
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

    if (!archiveHandle->storageInfo->jobOptions->noFragmentsCheckFlag)
    {
      SEMAPHORE_LOCKED_DO(fragmentListLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get image fragment node
        fragmentNode = FragmentList_find(fragmentList,deviceName);
        if (fragmentNode == NULL)
        {
          fragmentNode = FragmentList_add(fragmentList,deviceName,deviceInfo.size,NULL,0,0);
        }
        assert(fragmentNode != NULL);

        // add fragment to file fragment list
        FragmentList_addRange(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize);

        // discard fragment list if file is complete
        if (FragmentList_isComplete(fragmentNode))
        {
          FragmentList_discard(fragmentList,fragmentNode);
        }
      }
    }

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
      error = ERRORX_(CORRUPT_DATA,0,"%s",String_cString(deviceName));
      printInfo(1,"FAIL!\n");
      printWarning("unexpected data at end of image entry '%s'",String_cString(deviceName));
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(deviceName);
      return error;
    }

    // get size/fragment info
    if (globalOptions.humanFormatFlag)
    {
      getHumanSizeString(sizeString,sizeof(sizeString),blockCount*(uint64)deviceInfo.blockSize);
    }
    else
    {
      stringFormat(sizeString,sizeof(sizeString),"%"PRIu64,blockCount*(uint64)deviceInfo.blockSize);
    }
    stringClear(fragmentString);
    if ((blockCount*(uint64)deviceInfo.blockSize) < deviceInfo.size)
    {
      stringFormat(fragmentString,sizeof(fragmentString),
                   ", fragment %*"PRIu64"..%*"PRIu64,
                   stringInt64Length(deviceInfo.size),blockOffset*deviceInfo.blockSize,
                   stringInt64Length(deviceInfo.size),blockOffset*deviceInfo.blockSize+(blockCount*deviceInfo.blockSize)-1LL
                  );
    }

    // output
    printInfo(1,"OK (%s bytes%s)\n",sizeString,fragmentString);

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
    printInfo(2,"  Compare image     '%s'...skipped\n",String_cString(deviceName));
  }

  // close archive file, free resources
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'image' entry fail (error: %s)",Error_getText(error));
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
*          includeEntryList     - include entry list
*          excludePatternList   - exclude pattern list
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors compareDirectoryEntry(ArchiveHandle     *archiveHandle,
                                   const EntryList   *includeEntryList,
                                   const PatternList *excludePatternList
                                  )
{
  Errors           error;
  String           directoryName;
  ArchiveEntryInfo archiveEntryInfo;
  FileInfo         fileInfo;
//            String   localFileName;
//            FileInfo localFileInfo;

  assert(archiveHandle != NULL);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);

  // read directory
  directoryName = String_new();
  error = Archive_readDirectoryEntry(&archiveEntryInfo,
                                     archiveHandle,
                                     NULL,  // cryptType
                                     NULL,  // cryptAlgorithm
                                     NULL,  // cryptSalt
                                     NULL,  // cryptKey
                                     directoryName,
                                     &fileInfo,
                                     NULL   // fileExtendedAttributeList
                                    );
  if (error != ERROR_NONE)
  {
    printError("cannot read 'directory' entry from storage '%s' (error: %s)!",
               String_cString(archiveHandle->printableStorageName),
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
      printError("directory '%s' does not exists!",String_cString(directoryName));
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(directoryName);
      return error;
    }
    if (File_getType(directoryName) != FILE_TYPE_DIRECTORY)
    {
      printInfo(1,"FAIL!\n");
      printError("'%s' is not a directory!",
                 String_cString(directoryName)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(directoryName);
      return error;
    }

#if 0
    // get local file info
    error = File_getInfo(&localFileInfo,directoryName);
    if (error != ERROR_NONE)
    {
      printError("cannot read local directory '%s' (error: %s)!",
                 String_cString(directoryName),
                 Error_getText(error)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(directoryName);
      return error;
    }
    DEBUG_TESTCODE("Command_compare301") { Archive_closeEntry(&archiveEntryInfo); String_delete(directoryName); return DEBUG_TESTCODE_ERROR(); }

    // check file time, permissions, file owner/group
#endif /* 0 */
    printInfo(1,"OK\n");

    // check if all data read
    if (!Archive_eofData(&archiveEntryInfo))
    {
      printWarning("unexpected data at end of directory entry '%s'",String_cString(directoryName));
    }

    // free resources
  }
  else
  {
    // skip
    printInfo(2,"  Compare directory '%s'...skipped\n",String_cString(directoryName));
  }

  // close archive file
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'directory' entry fail (error: %s)",Error_getText(error));
  }

  // free resources
  String_delete(directoryName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : compareLinkEntry
* Purpose: compare a link entry in archive
* Input  : archiveHandle        - archive handle
*          includeEntryList     - include entry list
*          excludePatternList   - exclude pattern list
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors compareLinkEntry(ArchiveHandle     *archiveHandle,
                              const EntryList   *includeEntryList,
                              const PatternList *excludePatternList
                             )
{
  Errors           error;
  String           linkName;
  String           fileName;
  ArchiveEntryInfo archiveEntryInfo;
  FileInfo         fileInfo;
  String           localFileName;
//                    FileInfo localFileInfo;

  assert(archiveHandle != NULL);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);

  // read link
  linkName = String_new();
  fileName = String_new();
  error = Archive_readLinkEntry(&archiveEntryInfo,
                                archiveHandle,
                                NULL,  // cryptType
                                NULL,  // cryptAlgorithm
                                NULL,  // cryptSalt
                                NULL,  // cryptKey
                                linkName,
                                fileName,
                                &fileInfo,
                                NULL   // fileExtendedAttributeList
                               );
  if (error != ERROR_NONE)
  {
    printError("cannot read 'link' entry from storage '%s' (error: %s)!",
               String_cString(archiveHandle->printableStorageName),
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
    printInfo(1,"  Compare link      '%s'...",String_cString(linkName));

    // check if file exists and file type
    if (!File_exists(linkName))
    {
      printInfo(1,"FAIL!\n");
      printError("link '%s' -> '%s' does not exists!",
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
      printError("'%s' is not a link!",
                 String_cString(linkName)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      String_delete(linkName);
      return error;
    }

    // check link
    localFileName = String_new();
    error = File_readLink(localFileName,linkName,FALSE);
    if (error != ERROR_NONE)
    {
      printError("cannot read local file '%s' (error: %s)!",
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
      printError("link '%s' does not contain file '%s'!",
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
    error = File_getInfo(&localFileInfo,linkName);
    if (error != ERROR_NONE)
    {
      printError("cannot read local file '%s' (error: %s)!",
                 String_cString(linkName),
                 Error_getText(error)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      String_delete(linkName);
      return error;
    }
    DEBUG_TESTCODE("Command_compare403") { Archive_closeEntry(&archiveEntryInfo); String_delete(fileName); String_delete(linkName); return DEBUG_TESTCODE_ERROR(); }

    // check file time, permissions, file owner/group
#endif /* 0 */
    printInfo(1,"OK\n");

    // check if all data read
    if (!Archive_eofData(&archiveEntryInfo))
    {
      printWarning("unexpected data at end of link entry '%s'",String_cString(linkName));
    }

    // free resources
  }
  else
  {
    // skip
    printInfo(2,"  Compare link      '%s'...skipped\n",String_cString(linkName));
  }

  // close archive file
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'link' entry fail (error: %s)",Error_getText(error));
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
*          includeEntryList     - include entry list
*          excludePatternList   - exclude pattern list
*          fragmentListLock     - fragment list lock
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
                                  Semaphore         *fragmentListLock,
                                  FragmentList      *fragmentList,
                                  byte              *buffer0,
                                  byte              *buffer1,
                                  uint              bufferSize
                                 )
{
  Errors             error;
  StringList         fileNameList;
  ArchiveEntryInfo   archiveEntryInfo;
  CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
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
  FragmentNode       *fragmentNode;
  char               sizeString[32];
  char               fragmentString[256];

  assert(archiveHandle != NULL);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(fragmentListLock != NULL);
  assert(fragmentList != NULL);
  assert(buffer0 != NULL);
  assert(buffer1 != NULL);

  // read hard link
  StringList_init(&fileNameList);
  error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                    archiveHandle,
                                    &deltaCompressAlgorithm,
                                    &byteCompressAlgorithm,
                                    NULL,  // cryptType
                                    NULL,  // cryptAlgorithm
                                    NULL,  // cryptSalt
                                    NULL,  // cryptKey
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
    printError("cannot read 'hard link' entry from storage '%s' (error: %s)!",
               String_cString(archiveHandle->printableStorageName),
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
        printError("file '%s' not found!",String_cString(fileName));
        if (!archiveHandle->storageInfo->jobOptions->noStopOnErrorFlag)
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
        printError("'%s' is not a hard link!",String_cString(fileName));
        if (!archiveHandle->storageInfo->jobOptions->noStopOnErrorFlag)
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
          printError("cannot open file '%s' (error: %s)",
                     String_cString(fileName),
                     Error_getText(error)
                    );
          if (!archiveHandle->storageInfo->jobOptions->noStopOnErrorFlag)
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
          printError("'%s' differ in size: expected %"PRIu64" bytes, found %"PRIu64" bytes",
                     String_cString(fileName),
                     fileInfo.size,
                     File_getSize(&fileHandle)
                    );
          File_close(&fileHandle);
          if (!archiveHandle->storageInfo->jobOptions->noStopOnErrorFlag)
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
          printError("cannot read file '%s' (error: %s)",
                     String_cString(fileName),
                     Error_getText(error)
                    );
          File_close(&fileHandle);
          if (!archiveHandle->storageInfo->jobOptions->noStopOnErrorFlag)
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
            printError("cannot read content of 'hard link' entry '%s' (error: %s)!",
                       String_cString(StringList_first(&fileNameList,NULL)),
                       Error_getText(error)
                      );
            break;
          }
          DEBUG_TESTCODE() { error = DEBUG_TESTCODE_ERROR(); break; }
          error = File_read(&fileHandle,buffer1,bufferLength,NULL);
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL!\n");
            printError("cannot read file '%s' (error: %s)",
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
            printError("'%s' differ at offset %"PRIu64,
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
          if (!archiveHandle->storageInfo->jobOptions->noStopOnErrorFlag)
          {
            break;
          }
          else
          {
            continue;
          }
        }
        printInfo(2,"    \b\b\b\b");

        // close file
        (void)File_close(&fileHandle);

        if (!archiveHandle->storageInfo->jobOptions->noFragmentsCheckFlag)
        {
          SEMAPHORE_LOCKED_DO(fragmentListLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
          {
            // get file fragment list
            fragmentNode = FragmentList_find(fragmentList,fileName);
            if (fragmentNode == NULL)
            {
              fragmentNode = FragmentList_add(fragmentList,fileName,fileInfo.size,NULL,0,0);
            }
            assert(fragmentNode != NULL);

            // add fragment to file fragment list
            FragmentList_addRange(fragmentNode,fragmentOffset,fragmentSize);

            // discard fragment list if file is complete
            if (FragmentList_isComplete(fragmentNode))
            {
              FragmentList_discard(fragmentList,fragmentNode);
            }
          }
        }
#if 0
        // get local file info
        // check file time, permissions, file owner/group
#endif /* 0 */
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
          error = ERRORX_(CORRUPT_DATA,0,"%s",String_cString(fileName));
          printInfo(1,"FAIL!\n");
          printError("unexpected data at end of file entry '%s'!",String_cString(fileName));
          (void)Archive_closeEntry(&archiveEntryInfo);
          String_delete(fileName);
          return error;
        }

        // get size/fragment info
        if (globalOptions.humanFormatFlag)
        {
          getHumanSizeString(sizeString,sizeof(sizeString),fragmentSize);
        }
        else
        {
          stringFormat(sizeString,sizeof(sizeString),"%"PRIu64,fragmentSize);
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

        printInfo(1,"OK (%s bytes%s)\n",sizeString,fragmentString);

        comparedDataFlag = TRUE;
      }
      else
      {
        // compare hard link data already done

        // get size/fragment info
        if (globalOptions.humanFormatFlag)
        {
          getHumanSizeString(sizeString,sizeof(sizeString),fileInfo.size);
        }
        else
        {
          stringFormat(sizeString,sizeof(sizeString),"%"PRIu64,fileInfo.size);
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

        if (error == ERROR_NONE)
        {
          printInfo(1,"OK (%s bytes%s)\n",sizeString,fragmentString);
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
      printInfo(2,"  Compare hard link '%s'...skipped\n",String_cString(fileName));
    }
  }

  // close archive file, free resources
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'hard link' entry fail (error: %s)",Error_getText(error));
  }

  // free resources
  StringList_done(&fileNameList);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : compareSpecialEntry
* Purpose: compare a special entry in archive
* Input  : archiveHandle        - archive handle
*          includeEntryList     - include entry list
*          excludePatternList   - exclude pattern list
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors compareSpecialEntry(ArchiveHandle     *archiveHandle,
                                 const EntryList   *includeEntryList,
                                 const PatternList *excludePatternList
                                )
{
  Errors           error;
  String           fileName;
  ArchiveEntryInfo archiveEntryInfo;
  FileInfo         fileInfo;
  FileInfo         localFileInfo;

  assert(archiveHandle != NULL);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);

  // read special
  fileName = String_new();
  error = Archive_readSpecialEntry(&archiveEntryInfo,
                                   archiveHandle,
                                   NULL,  // cryptType
                                   NULL,  // cryptAlgorithm
                                   NULL,  // cryptSalt
                                   NULL,  // cryptKey
                                   fileName,
                                   &fileInfo,
                                   NULL   // fileExtendedAttributeList
                                  );
  if (error != ERROR_NONE)
  {
    printError("cannot read 'special' entry from storage '%s' (error: %s)!",
               String_cString(archiveHandle->printableStorageName),
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
    printInfo(1,"  Compare special   '%s'...",String_cString(fileName));

    // check if file exists and file type
    if (!File_exists(fileName))
    {
      printInfo(1,"FAIL!\n");
      printError("special device '%s' does not exists!",
                 String_cString(fileName)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return error;
    }
    if (File_getType(fileName) != FILE_TYPE_SPECIAL)
    {
      printInfo(1,"FAIL!\n");
      printError("'%s' is not a special device!",
                 String_cString(fileName)
                );
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return error;
    }

    // check special settings
    error = File_getInfo(&localFileInfo,fileName);
    if (error != ERROR_NONE)
    {
      printError("cannot read local file '%s' (error: %s)!",
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
      printError("different types of special device '%s'!",
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
        printError("different major numbers of special device '%s'!",
                   String_cString(fileName)
                  );
        (void)Archive_closeEntry(&archiveEntryInfo);
        String_delete(fileName);
        return error;
      }
      if (fileInfo.minor != localFileInfo.minor)
      {
        printError("different minor numbers of special device '%s'!",
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
      printWarning("unexpected data at end of special entry '%s'",String_cString(fileName));
    }

    // free resources
  }
  else
  {
    // skip
    printInfo(2,"  Compare special   '%s'...skipped\n",String_cString(fileName));
  }

  // close archive file
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printWarning("close 'special' entry fail (error: %s)",Error_getText(error));
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
  byte          *buffer0,*buffer1;
  uint          archiveIndex;
  ArchiveHandle archiveHandle;
  EntryMsg      entryMsg;
  Errors        error;

  assert(compareInfo != NULL);
  assert(compareInfo->jobOptions != NULL);

  // init variables
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
  archiveIndex = 0;

  // compare entries
  while (MsgQueue_get(&compareInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER))
  {
    assert(entryMsg.archiveHandle != NULL);
    assert(entryMsg.archiveCryptInfo != NULL);

    if (   ((compareInfo->failError == ERROR_NONE) || !compareInfo->jobOptions->noStopOnErrorFlag)
//TODO
//         && !isAborted(compareInfo)
       )
    {
      // open archive (only if new archive)
      if (archiveIndex < entryMsg.archiveIndex)
      {
        // close previous archive
        if (archiveIndex != 0)
        {
          Archive_close(&archiveHandle,FALSE);
        }

        // open new archive
        error = Archive_openHandle(&archiveHandle,
                                   entryMsg.archiveHandle
                                  );
        if (error != ERROR_NONE)
        {
          printError("cannot open storage '%s' (error: %s)!",
                     String_cString(entryMsg.archiveHandle->printableStorageName),
                     Error_getText(error)
                    );
          if (compareInfo->failError == ERROR_NONE) compareInfo->failError = error;
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
        if (compareInfo->failError == ERROR_NONE) compareInfo->failError = error;
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
          error = compareFileEntry(&archiveHandle,
                                   compareInfo->includeEntryList,
                                   compareInfo->excludePatternList,
                                   &compareInfo->fragmentListLock,
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
                                    &compareInfo->fragmentListLock,
                                    compareInfo->fragmentList,
                                    buffer0,
                                    buffer1,
                                    BUFFER_SIZE
                                   );
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          error = compareDirectoryEntry(&archiveHandle,
                                        compareInfo->includeEntryList,
                                        compareInfo->excludePatternList
                                       );
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          error = compareLinkEntry(&archiveHandle,
                                   compareInfo->includeEntryList,
                                   compareInfo->excludePatternList
                                  );
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          error = compareHardLinkEntry(&archiveHandle,
                                       compareInfo->includeEntryList,
                                       compareInfo->excludePatternList,
                                       &compareInfo->fragmentListLock,
                                       compareInfo->fragmentList,
                                       buffer0,
                                       buffer1,
                                       BUFFER_SIZE
                                      );
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          error = compareSpecialEntry(&archiveHandle,
                                      compareInfo->includeEntryList,
                                      compareInfo->excludePatternList
                                     );
          break;
        case ARCHIVE_ENTRY_TYPE_META:
          error = Archive_skipNextEntry(&archiveHandle);
          break;
        case ARCHIVE_ENTRY_TYPE_SALT:
        case ARCHIVE_ENTRY_TYPE_KEY:
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
        if (compareInfo->failError == ERROR_NONE) compareInfo->failError = error;
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
  while (MsgQueue_get(&compareInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER))
  {
    assert(entryMsg.archiveHandle != NULL);
    assert(entryMsg.archiveCryptInfo != NULL);

    freeEntryMsg(&entryMsg,NULL);
  }

  // free resources
  free(buffer1);
  free(buffer0);
}

/***********************************************************************\
* Name   : compareArchive
* Purpose: compare archive content
* Input  : storageSpecifier        - storage specifier
*          archiveName             - archive name (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors compareArchive(CompareInfo      *compareInfo,
                            StorageSpecifier *storageSpecifier,
                            ConstString      archiveName
                           )
{
  AutoFreeList         autoFreeList;
  String               printableStorageName;
  uint                 compareThreadCount;
  StorageInfo          storageInfo;
  Errors               error;
  uint                 i;
  ArchiveHandle        archiveHandle;
  CryptSignatureStates allCryptSignatureState;
  uint64               lastSignatureOffset;
  ArchiveEntryTypes    archiveEntryType;
  ArchiveCryptInfo     *archiveCryptInfo;
  uint64               offset;
  EntryMsg             entryMsg;

  assert(compareInfo != NULL);
  assert(storageSpecifier != NULL);

  // init variables
  AutoFree_init(&autoFreeList);
  printableStorageName = String_new();
  AUTOFREE_ADD(&autoFreeList,printableStorageName,{ String_delete(printableStorageName); });

  // get printable storage name
  Storage_getPrintableName(printableStorageName,storageSpecifier,archiveName);

  // init storage
  error = Storage_init(&storageInfo,
NULL, // masterSocketHandle
                       storageSpecifier,
                       compareInfo->jobOptions,
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
    printError("cannot initialize storage '%s' (error: %s)!",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&storageInfo,{ (void)Storage_done(&storageInfo); });

  // check if storage exists
  if (!Storage_exists(&storageInfo,archiveName))
  {
    printError("storage not found '%s'!",
               String_cString(printableStorageName)
              );
    AutoFree_cleanup(&autoFreeList);
    return ERROR_ARCHIVE_NOT_FOUND;
  }

  // open archive
  error = Archive_open(&archiveHandle,
                       &storageInfo,
                       archiveName,
                       &compareInfo->jobOptions->deltaSourceList,
                       ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS|(isPrintInfo(3) ? ARCHIVE_FLAG_PRINT_UNKNOWN_CHUNKS : ARCHIVE_FLAG_NONE),
                       CALLBACK_(compareInfo->getNamePasswordFunction,compareInfo->getNamePasswordUserData),
                       compareInfo->logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError("cannot open storage '%s' (error: %s)!",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { (void)Archive_close(&archiveHandle,FALSE); (void)Storage_done(&storageInfo); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveHandle,{ (void)Archive_close(&archiveHandle,FALSE); });

  // check signatures
  if (!compareInfo->jobOptions->skipVerifySignaturesFlag)
  {
    error = Archive_verifySignatures(&archiveHandle,
                                     &allCryptSignatureState
                                    );
    if (error != ERROR_NONE)
    {
      if (!compareInfo->jobOptions->forceVerifySignaturesFlag && (Error_getCode(error) == ERROR_CODE_NO_PUBLIC_SIGNATURE_KEY))
      {
        allCryptSignatureState = CRYPT_SIGNATURE_STATE_SKIPPED;
      }
      else
      {
        // signature error
        printError("cannot verify signatures of '%s' (error: %s)!",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }
    if (!Crypt_isValidSignatureState(allCryptSignatureState))
    {
      if (compareInfo->jobOptions->forceVerifySignaturesFlag)
      {
        // signature error
        printError("invalid signature in '%s'!",
                   String_cString(printableStorageName)
                  );
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

  // start compare threads
  MsgQueue_reset(&compareInfo->entryMsgQueue);
  compareThreadCount = (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores();
  for (i = 0; i < compareThreadCount; i++)
  {
    ThreadPool_run(&workerThreadPool,compareThreadCode,compareInfo);
  }

  // output info
  printInfo(0,
            "Compare storage '%s'%s",
            String_cString(printableStorageName),
            !isPrintInfo(1) ? "..." : ":\n"
           );

  // read archive entries
  allCryptSignatureState = CRYPT_SIGNATURE_STATE_NONE;
  error                  = ERROR_NONE;
  lastSignatureOffset    = Archive_tell(&archiveHandle);
  while (   (compareInfo->jobOptions->skipVerifySignaturesFlag || Crypt_isValidSignatureState(allCryptSignatureState))
         && ((compareInfo->failError == ERROR_NONE) || !compareInfo->jobOptions->noStopOnErrorFlag)
         && !Archive_eof(&archiveHandle)
        )
  {
    // get next archive entry type
    error = Archive_getNextArchiveEntry(&archiveHandle,
                                        &archiveEntryType,
                                        &archiveCryptInfo,
                                        &offset,
                                        NULL  // size
                                       );
    if (error != ERROR_NONE)
    {
      printError("cannot read next entry from storage '%s' (error: %s)!",
                 String_cString(printableStorageName),
                 Error_getText(error)
                );
      if (compareInfo->failError == ERROR_NONE) compareInfo->failError = error;
      break;
    }
    DEBUG_TESTCODE() { compareInfo->failError = DEBUG_TESTCODE_ERROR(); break; }

    if (archiveEntryType != ARCHIVE_ENTRY_TYPE_SIGNATURE)
    {
      // send entry to test threads
//TODO: increment on multiple archives and when threads are not restarted each time
      entryMsg.archiveIndex     = 1;
      entryMsg.archiveHandle    = &archiveHandle;
      entryMsg.archiveEntryType = archiveEntryType;
      entryMsg.archiveCryptInfo = archiveCryptInfo;
      entryMsg.offset           = offset;
      if (!MsgQueue_put(&compareInfo->entryMsgQueue,&entryMsg,sizeof(entryMsg)))
      {
        HALT_INTERNAL_ERROR("Send message to compare threads fail!");
      }

      // next entry
      error = Archive_skipNextEntry(&archiveHandle);
      if (error != ERROR_NONE)
      {
        if (compareInfo->failError == ERROR_NONE) compareInfo->failError = error;
        break;
      }
    }
    else
    {
      if (!compareInfo->jobOptions->skipVerifySignaturesFlag)
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
        if (compareInfo->failError == ERROR_NONE) compareInfo->failError = error;
        break;
      }
      lastSignatureOffset = Archive_tell(&archiveHandle);
    }
  }

  // wait for compare threads
  MsgQueue_setEndOfMsg(&compareInfo->entryMsgQueue);
  ThreadPool_joinAll(&workerThreadPool);

  // close archive
  Archive_close(&archiveHandle,FALSE);

  // done storage
  (void)Storage_done(&storageInfo);

  // output info
  if (!isPrintInfo(1)) printInfo(0,
                                 "%s",
                                    (compareInfo->failError == ERROR_NONE)
                                 && (   compareInfo->jobOptions->skipVerifySignaturesFlag
                                     || Crypt_isValidSignatureState(allCryptSignatureState)
                                    )
                                   ? "OK\n"
                                   : "FAIL!\n"
                                );

  // output signature error/warning
  if (!Crypt_isValidSignatureState(allCryptSignatureState))
  {
    if (compareInfo->jobOptions->forceVerifySignaturesFlag)
    {
      printError("invalid signature in '%s'!",
                 String_cString(printableStorageName)
                );
      if (compareInfo->failError == ERROR_NONE) compareInfo->failError = ERROR_INVALID_SIGNATURE;
    }
    else
    {
      printWarning("invalid signature in '%s'!",
                   String_cString(printableStorageName)
                  );
    }
  }

  // free resources
  String_delete(printableStorageName);
  AutoFree_done(&autoFreeList);

  return compareInfo->failError;
}

/*---------------------------------------------------------------------*/

Errors Command_compare(const StringList        *storageNameList,
                       const EntryList         *includeEntryList,
                       const PatternList       *excludePatternList,
                       JobOptions              *jobOptions,
                       GetNamePasswordFunction getNamePasswordFunction,
                       void                    *getNamePasswordUserData,
                       LogHandle               *logHandle
                      )
{
  FragmentList               fragmentList;
  StorageSpecifier           storageSpecifier;
  CompareInfo                compareInfo;
  StringNode                 *stringNode;
  String                     storageName;
  bool                       someStorageFound;
  Errors                     error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  String                     fileName;
  FileInfo                   fileInfo;
  FragmentNode               *fragmentNode;

  assert(storageNameList != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(jobOptions != NULL);

  // init variables
  FragmentList_init(&fragmentList);
  Storage_initSpecifier(&storageSpecifier);

  // init compare info
  initCompareInfo(&compareInfo,
                  includeEntryList,
                  excludePatternList,
                  jobOptions,
//TODO
NULL,  //               pauseTestFlag,
NULL,  //               requestedAbortFlag,
                  CALLBACK_(getNamePasswordFunction,getNamePasswordUserData),
                  &fragmentList,
                  logHandle
                 );

  someStorageFound = FALSE;
  STRINGLIST_ITERATE(storageNameList,stringNode,storageName)
  {
    // parse storage name
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error != ERROR_NONE)
    {
      printError("invalid storage '%s' (error: %s)!",
                 String_cString(storageName),
                 Error_getText(error)
                );
      if (compareInfo.failError == ERROR_NONE) compareInfo.failError = error;
      continue;
    }
    DEBUG_TESTCODE() { compareInfo.failError = DEBUG_TESTCODE_ERROR(); break; }

    error = ERROR_UNKNOWN;

    // try compare archive content
    if (error != ERROR_NONE)
    {
      if (String_isEmpty(storageSpecifier.archivePatternString))
      {
        // compare archive content
        error = compareArchive(&compareInfo,
                                      &storageSpecifier,
                                      NULL
                                     );
        if (error == ERROR_NONE)
        {
          someStorageFound = TRUE;
        }
      }
    }

    // try compare directory content
    if (error != ERROR_NONE)
    {
      error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                        &storageSpecifier,
                                        NULL,  // pathName
                                        jobOptions,
                                        SERVER_CONNECTION_PRIORITY_HIGH
                                       );
      if (error == ERROR_NONE)
      {
        fileName = String_new();
        while (!Storage_endOfDirectoryList(&storageDirectoryListHandle))
        {
          // read next directory entry
          error = Storage_readDirectoryList(&storageDirectoryListHandle,fileName,&fileInfo);
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

          // compare archive content
          if (   (fileInfo.type == FILE_TYPE_FILE)
              || (fileInfo.type == FILE_TYPE_LINK)
              || (fileInfo.type == FILE_TYPE_HARDLINK)
             )
          {
            error = compareArchive(&compareInfo,
                                          &storageSpecifier,
                                          fileName
                                         );
            if (error != ERROR_NONE)
            {
              if (compareInfo.failError == ERROR_NONE) compareInfo.failError = error;
            }
          }
          someStorageFound = TRUE;
        }
        String_delete(fileName);

        Storage_closeDirectoryList(&storageDirectoryListHandle);
      }
    }

    if (error != ERROR_NONE)
    {
      if (compareInfo.failError == ERROR_NONE) compareInfo.failError = error;
      continue;
    }

    if (compareInfo.failError != ERROR_NONE) break;
  }
  if ((compareInfo.failError == ERROR_NONE) && !StringList_isEmpty(storageNameList) && !someStorageFound)
  {
    printError("no matching storage files found!");
    compareInfo.failError = ERROR_FILE_NOT_FOUND_;
  }

  if (   (compareInfo.failError == ERROR_NONE)
      && !jobOptions->noFragmentsCheckFlag
     )
  {
    // check fragment lists
    FRAGMENTLIST_ITERATE(&fragmentList,fragmentNode)
    {
      if (!FragmentList_isComplete(fragmentNode))
      {
        printInfo(0,"Warning: incomplete entry '%s'\n",String_cString(fragmentNode->name));
        if (isPrintInfo(1))
        {
          printInfo(1,"  Fragments:\n");
          FragmentList_print(stdout,4,fragmentNode,TRUE);
        }
        if (compareInfo.failError == ERROR_NONE)
        {
          compareInfo.failError = ERRORX_(ENTRY_INCOMPLETE,0,"%s",String_cString(fragmentNode->name));;
        }
      }
    }
  }

  // get error
// TODO:
#if 0
  if ((isAbortedFunction == NULL) || !isAbortedFunction(isAbortedUserData))
  {
    error = compareInfo.failError;
  }
  else
  {
    error = ERROR_ABORTED;
  }
#else
error = compareInfo.failError;
#endif

  // done compare info
  doneCompareInfo(&compareInfo);

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  FragmentList_done(&fragmentList);

  // output info
  if (error != ERROR_NONE)
  {
    printInfo(1,tr("Compare fail: %s\n"),Error_getText(error));
  }

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
