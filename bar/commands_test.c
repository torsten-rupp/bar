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
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/msgqueues.h"
#include "common/patterns.h"
#include "common/patternlists.h"
#include "common/files.h"
#include "common/fragmentlists.h"

#include "bar.h"
#include "errors.h"
#include "entrylists.h"
#include "deltasourcelists.h"
#include "archive.h"

#include "commands_test.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// data buffer size
#define BUFFER_SIZE (64*1024)

// max. number of entry messages
#define MAX_ENTRY_MSG_QUEUE 256

/***************************** Datatypes *******************************/

// test info
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
} TestInfo;

// entry message send to test threads
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
* Name   : initTestInfo
* Purpose: initialize test info
* Input  : testInfo                - test info variable
*          includeEntryList        - include entry list
*          excludePatternList      - exclude pattern list
*          jobOptions              - job options
*          pauseTestFlag           - pause creation flag (can be NULL)
*          requestedAbortFlag      - request abort flag (can be NULL)
*          getNamePasswordFunction - get password call back
*          getNamePasswordUserData - user data for get password
*          fragmentList            - fragment list
*          logHandle               - log handle (can be NULL)
* Output : createInfo - initialized test info variable
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initTestInfo(TestInfo                *testInfo,
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
  assert(testInfo != NULL);

  // init variables
  testInfo->includeEntryList        = includeEntryList;
  testInfo->excludePatternList      = excludePatternList;
  testInfo->jobOptions              = jobOptions;
  testInfo->pauseTestFlag           = pauseTestFlag;
  testInfo->requestedAbortFlag      = requestedAbortFlag;
  testInfo->getNamePasswordFunction = getNamePasswordFunction;
  testInfo->getNamePasswordUserData = getNamePasswordUserData;
  testInfo->fragmentList            = fragmentList;
  testInfo->logHandle               = logHandle;
  testInfo->failError               = ERROR_NONE;

  // init entry name queue, storage queue
  if (!MsgQueue_init(&testInfo->entryMsgQueue,
                     MAX_ENTRY_MSG_QUEUE,
                     CALLBACK_((MsgQueueMsgFreeFunction)freeEntryMsg,NULL)
                    )
     )
  {
    HALT_FATAL_ERROR("Cannot initialize entry message queue!");
  }

  // init locks
  if (!Semaphore_init(&testInfo->fragmentListLock,SEMAPHORE_TYPE_BINARY))
  {
    HALT_FATAL_ERROR("Cannot initialize fragment list semaphore!");
  }

  DEBUG_ADD_RESOURCE_TRACE(testInfo,TestInfo);
}

/***********************************************************************\
* Name   : doneTestInfo
* Purpose: deinitialize test info
* Input  : testInfo - test info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneTestInfo(TestInfo *testInfo)
{
  assert(testInfo != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(testInfo,TestInfo);

  Semaphore_done(&testInfo->fragmentListLock);

  MsgQueue_done(&testInfo->entryMsgQueue);
}

/***********************************************************************\
* Name   : testFileEntry
* Purpose: test a file entry in archive
* Input  : archiveHandle        - archive handle
*          includeEntryList     - include entry list
*          excludePatternList   - exclude pattern list
*          fragmentListLock     - fragment list lock
*          fragmentList         - fragment list
*          buffer               - buffer for temporary data
*          bufferSize           - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors testFileEntry(ArchiveHandle     *archiveHandle,
                           const EntryList   *includeEntryList,
                           const PatternList *excludePatternList,
                           Semaphore         *fragmentListLock,
                           FragmentList      *fragmentList,
                           byte              *buffer,
                           uint              bufferSize
                          )
{
  Errors           error;
  String           fileName;
  ArchiveEntryInfo archiveEntryInfo;
  FileInfo         fileInfo;
  uint64           fragmentOffset,fragmentSize;
  uint64           length;
  ulong            n;
  FragmentNode     *fragmentNode;
  char             sizeString[32];
  char             fragmentString[256];

  assert(archiveHandle != NULL);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(fragmentListLock != NULL);
  assert(fragmentList != NULL);
  assert(buffer != NULL);

  // open archive entry
  fileName = String_new();
  error = Archive_readFileEntry(&archiveEntryInfo,
                                archiveHandle,
                                NULL,  // deltaCompressAlgorithm
                                NULL,  // byteCompressAlgorithm
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

  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
      && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
     )
  {
    printInfo(1,"  Test file      '%s'...",String_cString(fileName));

    // read file content
    length = 0LL;
    while (length < fragmentSize)
    {
      n = (ulong)MIN(fragmentSize-length,bufferSize);

      // read archive file
      error = Archive_readData(&archiveEntryInfo,buffer,n);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("cannot read content of archive '%s' (error: %s)!",
                   String_cString(archiveHandle->printableStorageName),
                   Error_getText(error)
                  );
        break;
      }

      length += (uint64)n;

      printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
    }
    if (error != ERROR_NONE)
    {
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return error;
    }
    printInfo(2,"    \b\b\b\b");

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

        // add fragment to file fragment list
        FragmentList_addRange(fragmentNode,fragmentOffset,fragmentSize);

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
    if (   !Compress_isCompressed(archiveEntryInfo.file.deltaCompressAlgorithm)
        && !Compress_isCompressed(archiveEntryInfo.file.byteCompressAlgorithm)
        && !Archive_eofData(&archiveEntryInfo)
       )
    {
      error = ERRORX_(CORRUPT_DATA,0,"%s",String_cString(fileName));
      printInfo(1,"FAIL!\n");
      printError("unexpected data at end of file entry '%S'!",fileName);
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return error;
    }

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

    // output
    printInfo(1,"OK (%s bytes%s)\n",sizeString,fragmentString);
  }
  else
  {
    // skip
    printInfo(2,"  Test file      '%s'...skipped\n",String_cString(fileName));
  }

  // close archive entry
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printError("closing 'file' entry fail (error: %s)!",
               Error_getText(error)
              );
    String_delete(fileName);
    return error;
  }

  // free resources
  String_delete(fileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : testImageEntry
* Purpose: test a image entry in archive
* Input  : archiveHandle        - archive handle
*          includeEntryList     - include entry list
*          excludePatternList   - exclude pattern list
*          fragmentListLock     - fragment list lock
*          fragmentList         - fragment list
*          buffer               - buffer for temporary data
*          bufferSize           - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors testImageEntry(ArchiveHandle     *archiveHandle,
                            const EntryList   *includeEntryList,
                            const PatternList *excludePatternList,
                            Semaphore         *fragmentListLock,
                            FragmentList      *fragmentList,
                            byte              *buffer,
                            uint              bufferSize
                           )
{
  Errors           error;
  String           deviceName;
  ArchiveEntryInfo archiveEntryInfo;
  DeviceInfo       deviceInfo;
  uint64           blockOffset,blockCount;
  uint64           block;
  ulong            bufferBlockCount;
  FragmentNode     *fragmentNode;
  char             sizeString[32];
  char             fragmentString[256];

  assert(archiveHandle != NULL);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(fragmentListLock != NULL);
  assert(fragmentList != NULL);
  assert(buffer != NULL);

  // open archive entry
  deviceName = String_new();
  error = Archive_readImageEntry(&archiveEntryInfo,
                                 archiveHandle,
                                 NULL,  // deltaCompressAlgorithm
                                 NULL,  // byteCompressAlgorithm
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
    printError("cannot read 'image' entry from storage '%s' (error: %s)!",
               String_cString(archiveHandle->printableStorageName),
               Error_getText(error)
              );
    String_delete(deviceName);
    return error;
  }
  if (deviceInfo.blockSize > bufferSize)
  {
    printError("device block size %"PRIu64" on '%s' is too big (max: %"PRIu64")",
               deviceInfo.blockSize,
               String_cString(deviceName),
               BUFFER_SIZE
              );
    String_delete(deviceName);
    return ERROR_INVALID_DEVICE_BLOCK_SIZE;
  }
  assert(deviceInfo.blockSize > 0);

  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,deviceName,PATTERN_MATCH_MODE_EXACT))
      && !PatternList_match(excludePatternList,deviceName,PATTERN_MATCH_MODE_EXACT)
     )
  {
    printInfo(1,"  Test image     '%s'...",String_cString(deviceName));

    // read image content
    block = 0LL;
    while (block < blockCount)
    {
      bufferBlockCount = MIN(blockCount-block,bufferSize/deviceInfo.blockSize);

      // read archive file
      error = Archive_readData(&archiveEntryInfo,buffer,bufferBlockCount*deviceInfo.blockSize);
      if (error != ERROR_NONE)
      {
        printInfo(1,"FAIL!\n");
        printError("cannot read content of archive '%s' (error: %s)!",
                   String_cString(archiveHandle->printableStorageName),
                   Error_getText(error)
                  );
        break;
      }

      block += (uint64)bufferBlockCount;

      printInfo(2,"%3d%%\b\b\b\b",(uint)((block*100LL)/blockCount));
    }
    if (error != ERROR_NONE)
    {
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(deviceName);
      return error;
    }
    printInfo(2,"    \b\b\b\b");

    if (!archiveHandle->storageInfo->jobOptions->noFragmentsCheckFlag)
    {
      SEMAPHORE_LOCKED_DO(fragmentListLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get file fragment node
        fragmentNode = FragmentList_find(fragmentList,deviceName);
        if (fragmentNode == NULL)
        {
          fragmentNode = FragmentList_add(fragmentList,deviceName,deviceInfo.size,NULL,0,0);
        }
//FragmentList_print(fragmentNode,String_cString(deviceName),FALSE);
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
    if (   !Compress_isCompressed(archiveEntryInfo.image.deltaCompressAlgorithm)
        && !Compress_isCompressed(archiveEntryInfo.image.byteCompressAlgorithm)
        && !Archive_eofData(&archiveEntryInfo))
    {
      error = ERRORX_(CORRUPT_DATA,0,"%s",String_cString(deviceName));
      printInfo(1,"FAIL!\n");
      printError("unexpected data at end of image entry '%S'!",deviceName);
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
  }
  else
  {
    // skip
    printInfo(2,"  Test image     '%s'...skipped\n",String_cString(deviceName));
  }

  // close archive entry
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printError("closing 'image' entry fail (error: %s)!",
               Error_getText(error)
              );
    String_delete(deviceName);
    return error;
  }

  // free resources
  String_delete(deviceName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : testDirectoryEntry
* Purpose: test a directory entry in archive
* Input  : archiveHandle      - archive handle
*          includeEntryList   - include entry list
*          excludePatternList - exclude pattern list
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors testDirectoryEntry(ArchiveHandle     *archiveHandle,
                                const EntryList   *includeEntryList,
                                const PatternList *excludePatternList
                               )
{
  Errors           error;
  String           directoryName;
  ArchiveEntryInfo archiveEntryInfo;
  FileInfo         fileInfo;

  assert(archiveHandle != NULL);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);

  // open archive entry
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

  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
      && !PatternList_match(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
     )
  {
    printInfo(1,"  Test directory '%s'...",String_cString(directoryName));

    // check if all data read
    if (!Archive_eofData(&archiveEntryInfo))
    {
      printInfo(1,"FAIL!\n");
      printError("unexpected data at end of directory entry '%S'!",directoryName);
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(directoryName);
      return ERRORX_(CORRUPT_DATA,0,"%s",String_cString(directoryName));
    }

    printInfo(1,"OK\n");

    // free resources
  }
  else
  {
    // skip
    printInfo(2,"  Test directory '%s'...skipped\n",String_cString(directoryName));
  }

  // close archive entry
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printError("closing 'directory' entry fail (error: %s)!",
               Error_getText(error)
              );
    String_delete(directoryName);
    return error;
  }

  // free resources
  String_delete(directoryName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : testLinkEntry
* Purpose: test a link entry in archive
* Input  : archiveHandle      - archive handle
*          includeEntryList   - include entry list
*          excludePatternList - exclude pattern list
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors testLinkEntry(ArchiveHandle     *archiveHandle,
                           const EntryList   *includeEntryList,
                           const PatternList *excludePatternList
                          )
{
  Errors           error;
  String           linkName;
  String           fileName;
  ArchiveEntryInfo archiveEntryInfo;
  FileInfo         fileInfo;

  assert(archiveHandle != NULL);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);

  // open archive entry
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
    printError("cannot read 'link' content of archive '%s' (error: %s)!",
               String_cString(archiveHandle->printableStorageName),
               Error_getText(error)
              );
    String_delete(fileName);
    String_delete(linkName);
    return error;
  }

  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
      && !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
     )
  {
    printInfo(1,"  Test link      '%s'...",String_cString(linkName));

    // check if all data read
    if (!Archive_eofData(&archiveEntryInfo))
    {
      printInfo(1,"FAIL!\n");
      printError("unexpected data at end of link entry '%S'!",linkName);
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      String_delete(linkName);
      return ERRORX_(CORRUPT_DATA,0,"%s",String_cString(linkName));
    }

    printInfo(1,"OK\n");

    // free resources
  }
  else
  {
    // skip
    printInfo(2,"  Test link      '%s'...skipped\n",String_cString(linkName));
  }

  // close archive file
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printError("closing 'link' entry fail (error: %s)!",
               Error_getText(error)
              );
    String_delete(fileName);
    String_delete(linkName);
    return error;
  }

  // free resources
  String_delete(fileName);
  String_delete(linkName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : testHardLinkEntry
* Purpose: test a hardlink entry in archive
* Input  : archiveHandle      - archive handle
*          includeEntryList   - include entry list
*          excludePatternList - exclude pattern list
*          fragmentListLock     - fragment list lock
*          fragmentList       - fragment list
*          buffer             - buffer for temporary data
*          bufferSize         - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors testHardLinkEntry(ArchiveHandle     *archiveHandle,
                               const EntryList   *includeEntryList,
                               const PatternList *excludePatternList,
                               Semaphore         *fragmentListLock,
                               FragmentList      *fragmentList,
                               byte              *buffer,
                               uint              bufferSize
                              )
{
  Errors           error;
  StringList       fileNameList;
  ArchiveEntryInfo archiveEntryInfo;
  FileInfo         fileInfo;
  uint64           fragmentOffset,fragmentSize;
  bool             testedDataFlag;
  const StringNode *stringNode;
  String           fileName;
  uint64           length;
  ulong            n;
  FragmentNode     *fragmentNode;
  char             sizeString[32];
  char             fragmentString[256];

  assert(archiveHandle != NULL);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(fragmentListLock != NULL);
  assert(fragmentList != NULL);
  assert(buffer != NULL);

  // open archive entry
  StringList_init(&fileNameList);
  error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                    archiveHandle,
                                    NULL,  // deltaCompressAlgorithm
                                    NULL,  // byteCompressAlgorithm
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

  testedDataFlag = FALSE;
  STRINGLIST_ITERATE(&fileNameList,stringNode,fileName)
  {
    if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
        && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
       )
    {
      printInfo(1,"  Test hard link '%s'...",String_cString(fileName));

      if (!testedDataFlag && (error == ERROR_NONE))
      {
        // read hard link content
        length = 0LL;
        while (length < fragmentSize)
        {
          n = (ulong)MIN(fragmentSize-length,bufferSize);

          // read archive file
          error = Archive_readData(&archiveEntryInfo,buffer,n);
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL!\n");
            printError("cannot read content of archive '%s' (error: %s)!",
                       String_cString(archiveHandle->printableStorageName),
                       Error_getText(error)
                      );
            break;
          }

          length += (uint64)n;

          printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
        }
        if (error != ERROR_NONE)
        {
          break;
        }
        printInfo(2,"    \b\b\b\b");

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

            // add range to file fragment list
            FragmentList_addRange(fragmentNode,fragmentOffset,fragmentSize);

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
        if (   !Compress_isCompressed(archiveEntryInfo.hardLink.deltaCompressAlgorithm)
            && !Compress_isCompressed(archiveEntryInfo.hardLink.byteCompressAlgorithm)
            && !Archive_eofData(&archiveEntryInfo))
        {
          printError("unexpected data at end of hard link entry '%S'!",fileName);
          error = ERRORX_(CORRUPT_DATA,0,"%s",String_cString(fileName));
          break;
        }

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

        printInfo(1,"OK (%s bytes%s)\n",sizeString,fragmentString);

        testedDataFlag = TRUE;
      }
      else
      {
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
      printInfo(2,"  Test hard link '%s'...skipped\n",String_cString(fileName));
    }
  }
  if (error != ERROR_NONE)
  {
    (void)Archive_closeEntry(&archiveEntryInfo);
    StringList_done(&fileNameList);
    return error;
  }

  // close archive entry
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printError("closing 'hard link' entry fail (error: %s)!",
               Error_getText(error)
              );
    StringList_done(&fileNameList);
    return error;
  }

  // free resources
  StringList_done(&fileNameList);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : testSpecialEntry
* Purpose: test a special entry in archive
* Input  : archiveHandle      - archive handle
*          includeEntryList   - include entry list
*          excludePatternList - exclude pattern list
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors testSpecialEntry(ArchiveHandle     *archiveHandle,
                              const EntryList   *includeEntryList,
                              const PatternList *excludePatternList
                             )
{
  Errors           error;
  String           fileName;
  ArchiveEntryInfo archiveEntryInfo;
  FileInfo         fileInfo;

  assert(archiveHandle != NULL);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);

  // open archive entry
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

  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
      && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
     )
  {
    printInfo(1,"  Test special   '%s'...",String_cString(fileName));

    // check if all data read
    if (!Archive_eofData(&archiveEntryInfo))
    {
      printInfo(1,"FAIL!\n");
      printError("unexpected data at end of special entry '%S'!",fileName);
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return ERRORX_(CORRUPT_DATA,0,"%s",String_cString(fileName));
    }

    printInfo(1,"OK\n");

    // free resources
  }
  else
  {
    // skip
    printInfo(2,"  Test special   '%s'...skipped\n",String_cString(fileName));
  }

  // close archive entry
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printError("closing 'special' entry fail (error: %s)!",
               Error_getText(error)
              );
    String_delete(fileName);
    return error;
  }

  // free resources
  String_delete(fileName);

  return ERROR_NONE;
}

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
  byte          *buffer;
  uint          archiveIndex;
  ArchiveHandle archiveHandle;
  EntryMsg      entryMsg;
  Errors        error;

  assert(testInfo != NULL);
  assert(testInfo->jobOptions != NULL);

  // init variables
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  archiveIndex = 0;

  // test entries
  while (MsgQueue_get(&testInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER))
  {
    assert(entryMsg.archiveHandle != NULL);
    assert(entryMsg.archiveCryptInfo != NULL);

    if (   ((testInfo->failError == ERROR_NONE) || !testInfo->jobOptions->noStopOnErrorFlag)
// TODO:
//        && !isAborted(testInfo)
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
          printError("cannot open archive '%s' (error: %s)!",
                     String_cString(entryMsg.archiveHandle->printableStorageName),
                     Error_getText(error)
                    );
          if (testInfo->failError == ERROR_NONE) testInfo->failError = error;
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
        if (testInfo->failError == ERROR_NONE) testInfo->failError = error;
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
          error = testFileEntry(&archiveHandle,
                                testInfo->includeEntryList,
                                testInfo->excludePatternList,
                                &testInfo->fragmentListLock,
                                testInfo->fragmentList,
                                buffer,
                                BUFFER_SIZE
                               );
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          error = testImageEntry(&archiveHandle,
                                 testInfo->includeEntryList,
                                 testInfo->excludePatternList,
                                 &testInfo->fragmentListLock,
                                 testInfo->fragmentList,
                                 buffer,
                                 BUFFER_SIZE
                                );
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          error = testDirectoryEntry(&archiveHandle,
                                     testInfo->includeEntryList,
                                     testInfo->excludePatternList
                                    );
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          error = testLinkEntry(&archiveHandle,
                                testInfo->includeEntryList,
                                testInfo->excludePatternList
                               );
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          error = testHardLinkEntry(&archiveHandle,
                                    testInfo->includeEntryList,
                                    testInfo->excludePatternList,
                                    &testInfo->fragmentListLock,
                                    testInfo->fragmentList,
                                    buffer,
                                    BUFFER_SIZE
                                   );
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          error = testSpecialEntry(&archiveHandle,
                                   testInfo->includeEntryList,
                                   testInfo->excludePatternList
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
        if (testInfo->failError == ERROR_NONE) testInfo->failError = error;
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
  while (MsgQueue_get(&testInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER))
  {
    assert(entryMsg.archiveHandle != NULL);
    assert(entryMsg.archiveCryptInfo != NULL);

    freeEntryMsg(&entryMsg,NULL);
  }

  // free resources
  free(buffer);
}

/***********************************************************************\
* Name   : testArchive
* Purpose: test archive content
* Input  : storageSpecifier        - storage specifier
*          archiveName             - archive name (can be NULL)
*          includeEntryList        - include entry list
*          excludePatternList      - exclude pattern list
*          jobOptions              - job options
*          getNamePasswordFunction - get password call back
*          getNamePasswordUserData - user data for get password
*          fragmentList            - fragment list
*          logHandle               - log handle (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors testArchive(TestInfo         *testInfo,
                         StorageSpecifier *storageSpecifier,
                         ConstString      archiveName
                        )
{
  AutoFreeList         autoFreeList;
  String               printableStorageName;
  StorageInfo          storageInfo;
  Errors               error;
  uint                 testThreadCount;
  uint                 i;
  ArchiveHandle        archiveHandle;
  CryptSignatureStates allCryptSignatureState;
  uint64               lastSignatureOffset;
  ArchiveEntryTypes    archiveEntryType;
  ArchiveCryptInfo     *archiveCryptInfo;
  uint64               offset;
  EntryMsg             entryMsg;

  assert(testInfo != NULL);
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
                       testInfo->jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK_(NULL,NULL),  // updateStatusInfo
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
                       &testInfo->jobOptions->deltaSourceList,
                       isPrintInfo(3) ? ARCHIVE_FLAG_PRINT_UNKNOWN_CHUNKS : ARCHIVE_FLAG_NONE,
                       CALLBACK_(testInfo->getNamePasswordFunction,testInfo->getNamePasswordUserData),
                       testInfo->logHandle
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
  if (!testInfo->jobOptions->skipVerifySignaturesFlag)
  {
    error = Archive_verifySignatures(&archiveHandle,
                                     &allCryptSignatureState
                                    );
    if (error != ERROR_NONE)
    {
      if (!testInfo->jobOptions->forceVerifySignaturesFlag && (Error_getCode(error) == ERROR_CODE_NO_PUBLIC_SIGNATURE_KEY))
      {
        allCryptSignatureState = CRYPT_SIGNATURE_STATE_SKIPPED;
      }
      else
      {
        // signature error
        printError("cannot verify signatures '%s' (error: %s)!",
                   String_cString(printableStorageName),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }
    if (!Crypt_isValidSignatureState(allCryptSignatureState))
    {
      if (testInfo->jobOptions->forceVerifySignaturesFlag)
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
        printWarning("invalid signature in '%s'",
                     String_cString(printableStorageName)
                    );
      }
    }
  }

  // output info
  printInfo(0,
            "Test storage '%s'%s",
            String_cString(printableStorageName),
            !isPrintInfo(1) ? "..." : ":\n"
           );

  // start test threads
  MsgQueue_reset(&testInfo->entryMsgQueue);
  testThreadCount = (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores();
  for (i = 0; i < testThreadCount; i++)
  {
    ThreadPool_run(&workerThreadPool,testThreadCode,testInfo);
  }

  // read archive entries
  allCryptSignatureState = CRYPT_SIGNATURE_STATE_NONE;
  error                  = ERROR_NONE;
  lastSignatureOffset    = Archive_tell(&archiveHandle);
  while (   ((testInfo->failError == ERROR_NONE) || !testInfo->jobOptions->noStopOnErrorFlag)
         && (testInfo->jobOptions->skipVerifySignaturesFlag || Crypt_isValidSignatureState(allCryptSignatureState))
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
      if (testInfo->failError == ERROR_NONE) testInfo->failError = error;
      break;
    }
    DEBUG_TESTCODE() { testInfo->failError = DEBUG_TESTCODE_ERROR(); break; }

    if (archiveEntryType != ARCHIVE_ENTRY_TYPE_SIGNATURE)
    {
      // send entry to test threads
//TODO: increment on multiple archives and when threads are not restarted each time
      entryMsg.archiveIndex     = 1;
      entryMsg.archiveHandle    = &archiveHandle;
      entryMsg.archiveEntryType = archiveEntryType;
      entryMsg.archiveCryptInfo = archiveCryptInfo;
      entryMsg.offset           = offset;
      if (!MsgQueue_put(&testInfo->entryMsgQueue,&entryMsg,sizeof(entryMsg)))
      {
        HALT_INTERNAL_ERROR("Send message to test threads fail!");
      }

      // skip entry
      error = Archive_skipNextEntry(&archiveHandle);
      if (error != ERROR_NONE)
      {
        if (testInfo->failError == ERROR_NONE) testInfo->failError = error;
        break;
      }
    }
    else
    {
      if (!testInfo->jobOptions->skipVerifySignaturesFlag)
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
        if (testInfo->failError == ERROR_NONE) testInfo->failError = error;
        break;
      }
      lastSignatureOffset = Archive_tell(&archiveHandle);
    }
  }

  // wait for test threads
  MsgQueue_setEndOfMsg(&testInfo->entryMsgQueue);
  ThreadPool_joinAll(&workerThreadPool);

  // close archive
  (void)Archive_close(&archiveHandle,FALSE);

  // done storage
  (void)Storage_done(&storageInfo);

  // output info
  if (!isPrintInfo(1)) printInfo(0,
                                 "%s",
                                    (testInfo->failError == ERROR_NONE)
                                 && (   testInfo->jobOptions->skipVerifySignaturesFlag
                                     || Crypt_isValidSignatureState(allCryptSignatureState)
                                    )
                                   ? "OK\n"
                                   : "FAIL!\n"
                                );

  // output signature error/warning
  if (!Crypt_isValidSignatureState(allCryptSignatureState))
  {
    if (testInfo->jobOptions->forceVerifySignaturesFlag)
    {
      printError("invalid signature in '%s'!",
                 String_cString(printableStorageName)
                );
      if (testInfo->failError == ERROR_NONE) testInfo->failError = ERROR_INVALID_SIGNATURE;
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

  return testInfo->failError;
}

/*---------------------------------------------------------------------*/

Errors Command_test(const StringList        *storageNameList,
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
  TestInfo                   testInfo;
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

  // allocate resources
  FragmentList_init(&fragmentList);
  Storage_initSpecifier(&storageSpecifier);

  // init test info
  initTestInfo(&testInfo,
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
      if (testInfo.failError == ERROR_NONE) testInfo.failError = error;
      continue;
    }

    error = ERROR_UNKNOWN;

    if (error != ERROR_NONE)
    {
      if (String_isEmpty(storageSpecifier.archivePatternString))
      {
        // test archive content
        error = testArchive(&testInfo,
                                   &storageSpecifier,
                                   NULL  // fileName
                                  );
        someStorageFound = TRUE;
      }
    }
    if (error != ERROR_NONE)
    {
      error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                        &storageSpecifier,
                                        NULL,  // fileName
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

          // test archive content
          if (   (fileInfo.type == FILE_TYPE_FILE)
              || (fileInfo.type == FILE_TYPE_LINK)
              || (fileInfo.type == FILE_TYPE_HARDLINK)
             )
          {
            error = testArchive(&testInfo,
                                       &storageSpecifier,
                                       fileName
                                      );
            if (error != ERROR_NONE)
            {
              if (testInfo.failError == ERROR_NONE) testInfo.failError = error;
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
      if (testInfo.failError == ERROR_NONE) testInfo.failError = error;
      continue;
    }
  }
  if ((testInfo.failError == ERROR_NONE) && !StringList_isEmpty(storageNameList) && !someStorageFound)
  {
    printError("no matching storage files found!");
    testInfo.failError = ERROR_FILE_NOT_FOUND_;
  }

  if (   (testInfo.failError == ERROR_NONE)
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
        if (testInfo.failError == ERROR_NONE) testInfo.failError = ERROR_ENTRY_INCOMPLETE;
      }
    }
  }

  // get error
// TODO:
#if 0
  if ((isAbortedFunction == NULL) || !isAbortedFunction(isAbortedUserData))
  {
    error = testInfo.failError;
  }
  else
  {
    error = ERROR_ABORTED;
  }
#else
error = testInfo.failError;
#endif

  // done test info
  doneTestInfo(&testInfo);

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  FragmentList_done(&fragmentList);

  // output info
  if (error != ERROR_NONE)
  {
    printInfo(1,tr("Test fail: {0}\n",Error_getText(error)));
  }

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
