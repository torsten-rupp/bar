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

// data buffer size
#define BUFFER_SIZE (64*1024)

// max. number of entry messages
#define MAX_ENTRY_MSG_QUEUE 256

/***************************** Datatypes *******************************/

// test info
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
} TestInfo;

// entry message send to test threads
typedef struct
{
  StorageInfo       *storageInfo;
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

  UNUSED_VARIABLE(userData);

//      StringList_done(&entryMsg->nameList);
//      String_delete(entryMsg->name);
}

/***********************************************************************\
* Name   : initTestInfo
* Purpose: initialize test info
* Input  : testInfo                   - test info variable
*          storageSpecifier           - storage specifier structure
*          includeEntryList           - include entry list
*          excludePatternList         - exclude pattern list
*          deltaSourceList            - delta source list
*          jobOptions                 - job options
*          pauseTestFlag              - pause creation flag (can be
*                                       NULL)
*          requestedAbortFlag         - request abort flag (can be NULL)
*          logHandle                  - log handle (can be NULL)
* Output : createInfo - initialized test info variable
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initTestInfo(TestInfo          *testInfo,
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
  assert(testInfo != NULL);

  // init variables
  testInfo->fragmentList       = fragmentList;
  testInfo->storageSpecifier   = storageSpecifier;
  testInfo->includeEntryList   = includeEntryList;
  testInfo->excludePatternList = excludePatternList;
  testInfo->deltaSourceList    = deltaSourceList;
  testInfo->jobOptions         = jobOptions;
  testInfo->pauseTestFlag      = pauseTestFlag;
  testInfo->requestedAbortFlag = requestedAbortFlag;
  testInfo->logHandle          = logHandle;
  testInfo->failError          = ERROR_NONE;

  // init entry name queue, storage queue
  if (!MsgQueue_init(&testInfo->entryMsgQueue,MAX_ENTRY_MSG_QUEUE))
  {
    HALT_FATAL_ERROR("Cannot initialize entry message queue!");
  }

#if 0
  // init locks
  if (!Semaphore_init(&testInfo->storageInfoLock))
  {
    HALT_FATAL_ERROR("Cannot initialize storage semaphore!");
  }
#endif

  DEBUG_ADD_RESOURCE_TRACE(testInfo,sizeof(TestInfo));
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

  DEBUG_REMOVE_RESOURCE_TRACE(testInfo,sizeof(TestInfo));

  MsgQueue_done(&testInfo->entryMsgQueue,(MsgQueueMsgFreeFunction)freeEntryMsg,NULL);
}

/***********************************************************************\
* Name   : testFileEntry
* Purpose: test a file entry in archive
* Input  : archiveHandle        - archive handle
*          offset               - offset
*          includeEntryList     - include entry list
*          excludePatternList   - exclude pattern list
*          printableStorageName - printable storage name
*          jobOptions           - job options
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
                           const char        *printableStorageName,
                           const JobOptions  *jobOptions,
                           FragmentList      *fragmentList,
                           byte              *buffer,
                           uint              bufferSize
                          )
{
  Errors           error;
  ArchiveEntryInfo archiveEntryInfo;
  String           fileName;
  FileInfo         fileInfo;
  uint64           fragmentOffset,fragmentSize;
  uint64           length;
  ulong            n;
  SemaphoreLock    semaphoreLock;
  FragmentNode     *fragmentNode;
  char             s[256];

  // open archive entry
  fileName = String_new();
  error = Archive_readFileEntry(&archiveEntryInfo,
                                archiveHandle,
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
               printableStorageName,
               Error_getText(error)
              );
    String_delete(fileName);
    return error;
  }

  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
      && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
     )
  {
    printInfo(1,"  Test file '%s'...",String_cString(fileName));

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
        printError("Cannot read content of archive '%s' (error: %s)!\n",
                   printableStorageName,
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

        // add fragment to file fragment list
        FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);

        // discard fragment list if file is complete
        if (FragmentList_isEntryComplete(fragmentNode))
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
      printInfo(1,"FAIL!\n");
      printError("unexpected data at end of file entry '%S'!\n",fileName);
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(fileName);
      return ERRORX_(CORRUPT_DATA,0,"%s",String_cString(fileName));
    }

    // get fragment info
    if (fragmentSize < fileInfo.size)
    {
      stringFormat(s,sizeof(s),", fragment %12llu..%12llu",fragmentOffset,fragmentOffset+fragmentSize-1LL);
    }
    else
    {
      stringClear(s);
    }

    // output
    printInfo(1,"OK (%llu bytes%s)\n",fragmentSize,s);
  }
  else
  {
    // skip
    printInfo(2,"Test '%s'...skipped\n",String_cString(fileName));
  }

  // close archive entry
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printError("closing 'file' entry fail (error: %s)!\n",
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
*          offset               - offset
*          printableStorageName - printable storage name
*          jobOptions           - job options
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
                            const char        *printableStorageName,
                            const JobOptions  *jobOptions,
                            FragmentList      *fragmentList,
                            byte              *buffer,
                            uint              bufferSize
                           )
{
  Errors           error;
  ArchiveEntryInfo archiveEntryInfo;
  String           deviceName;
  DeviceInfo       deviceInfo;
  uint64           blockOffset,blockCount;
  uint64           block;
  ulong            bufferBlockCount;
  SemaphoreLock    semaphoreLock;
  FragmentNode     *fragmentNode;

  // open archive entry
  deviceName = String_new();
  error = Archive_readImageEntry(&archiveEntryInfo,
                                 archiveHandle,
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
               printableStorageName,
               Error_getText(error)
              );
    String_delete(deviceName);
    return error;
  }
  if (deviceInfo.blockSize > bufferSize)
  {
    printError("Device block size %llu on '%s' is too big (max: %llu)\n",
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
    printInfo(1,"  Test image '%s'...",String_cString(deviceName));

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
        printError("Cannot read content of archive '%s' (error: %s)!\n",
                   printableStorageName,
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

    if (!jobOptions->noFragmentsCheckFlag)
    {
      SEMAPHORE_LOCKED_DO(semaphoreLock,&fragmentList->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        // get file fragment node
        fragmentNode = FragmentList_find(fragmentList,deviceName);
        if (fragmentNode == NULL)
        {
          fragmentNode = FragmentList_add(fragmentList,deviceName,deviceInfo.size,NULL,0);
        }
  //FragmentList_print(fragmentNode,String_cString(deviceName));
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
      (void)Archive_closeEntry(&archiveEntryInfo);
      String_delete(deviceName);
      return ERRORX_(CORRUPT_DATA,0,"%s",String_cString(deviceName));
    }

    printInfo(1,"OK\n");
  }
  else
  {
    // skip
    printInfo(2,"  Test '%s'...skipped\n",String_cString(deviceName));
  }

  // close archive entry
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printError("closing 'image' entry fail (error: %s)!\n",
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
* Input  : archiveHandle        - archive handle
*          offset               - offset
*          printableStorageName - printable storage name
*          jobOptions           - job options
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors testDirectoryEntry(ArchiveHandle     *archiveHandle,
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

  UNUSED_VARIABLE(jobOptions);

  // open archive entry
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
      return ERRORX_(CORRUPT_DATA,0,"%s",String_cString(directoryName));
    }

    printInfo(1,"OK\n");

    // free resources
  }
  else
  {
    // skip
    printInfo(2,"Test '%s'...skipped\n",String_cString(directoryName));
  }

  // close archive entry
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printError("closing 'directory' entry fail (error: %s)!\n",
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
* Input  : archiveHandle        - archive handle
*          offset               - offset
*          printableStorageName - printable storage name
*          jobOptions           - job options
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors testLinkEntry(ArchiveHandle     *archiveHandle,
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

  UNUSED_VARIABLE(jobOptions);

  // open archive entry
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
      return ERRORX_(CORRUPT_DATA,0,"%s",String_cString(linkName));
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
* Input  : archiveHandle        - archive handle
*          offset               - offset
*          printableStorageName - printable storage name
*          jobOptions           - job options
*          fragmentList         - fragment list
*          buffer               - buffer for temporary data
*          bufferSize           - size of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors testHardLinkEntry(ArchiveHandle     *archiveHandle,
                               const EntryList   *includeEntryList,
                               const PatternList *excludePatternList,
                               const char        *printableStorageName,
                               const JobOptions  *jobOptions,
                               FragmentList      *fragmentList,
                               byte              *buffer,
                               uint              bufferSize
                              )
{
  Errors           error;
  ArchiveEntryInfo archiveEntryInfo;
  StringList       fileNameList;
  FileInfo         fileInfo;
  uint64           fragmentOffset,fragmentSize;
  bool             testedDataFlag;
  const StringNode *stringNode;
  String           fileName;
  uint64           length;
  ulong            n;
  SemaphoreLock    semaphoreLock;
  FragmentNode     *fragmentNode;

  // open archive entry
  StringList_init(&fileNameList);
  error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                    archiveHandle,
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
               printableStorageName,
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
        // read hard link data

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
            printError("Cannot read content of archive '%s' (error: %s)!\n",
                       printableStorageName,
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
          error = ERRORX_(CORRUPT_DATA,0,"%s",String_cString(fileName));
          break;
        }

        printInfo(1,"OK\n");

        testedDataFlag = TRUE;
      }
      else
      {
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
      printInfo(2,"  Test '%s'...skipped\n",String_cString(fileName));
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
    printError("closing 'hard link' entry fail (error: %s)!\n",
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
* Input  : archiveHandle        - archive handle
*          offset               - offset
*          printableStorageName - printable storage name
*          jobOptions           - job options
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors testSpecialEntry(ArchiveHandle     *archiveHandle,
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

  UNUSED_VARIABLE(jobOptions);

  // open archive entry
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
      return ERRORX_(CORRUPT_DATA,0,"%s",String_cString(fileName));
    }

    printInfo(1,"OK\n");

    // free resources
  }
  else
  {
    // skip
    printInfo(2,"  Test '%s'...skipped\n",String_cString(fileName));
  }

  // close archive entry
  error = Archive_closeEntry(&archiveEntryInfo);
  if (error != ERROR_NONE)
  {
    printError("closing 'special' entry fail (error: %s)!\n",
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
  byte              *buffer;

  ArchiveHandle     archiveHandle;
  EntryMsg          entryMsg;
  Errors            error;

  assert(testInfo != NULL);

  // init variables
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // test entries
  while (   (testInfo->failError == ERROR_NONE)
//TODO
//         && !isAborted(testInfo)
         && MsgQueue_get(&testInfo->entryMsgQueue,&entryMsg,NULL,sizeof(entryMsg),WAIT_FOREVER)
        )
  {
//TODO: open only when changed
    // open archive
    error = Archive_open(&archiveHandle,
                         entryMsg.storageInfo,
                         NULL,  // fileName,
                         testInfo->deltaSourceList,
                         testInfo->jobOptions,
                         testInfo->getPasswordFunction,
                         testInfo->getPasswordUserData,
                         testInfo->logHandle
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot open storage '%s' (error: %s)!\n",
                 Storage_getPrintableNameCString(testInfo->storageSpecifier,NULL),
                 Error_getText(error)
                );
      free(buffer);
      if (testInfo->failError == ERROR_NONE) testInfo->failError = error;
      break;
    }

    // seek to start of entry
    error = Archive_seek(&archiveHandle,entryMsg.offset);
    if (error != ERROR_NONE)
    {
      printError("Cannot read storage '%s' (error: %s)!\n",
                 Storage_getPrintableNameCString(testInfo->storageSpecifier,NULL),
                 Error_getText(error)
                );
      if (testInfo->failError == ERROR_NONE) testInfo->failError = error;
      break;
    }

    switch (entryMsg.archiveEntryType)
    {
      case ARCHIVE_ENTRY_TYPE_FILE:
        error = testFileEntry(&archiveHandle,
                              testInfo->includeEntryList,
                              testInfo->excludePatternList,
                              Storage_getPrintableNameCString(testInfo->storageSpecifier,NULL),
                              testInfo->jobOptions,
                              testInfo->fragmentList,
                              buffer,
                              BUFFER_SIZE
                             );
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        error = testImageEntry(&archiveHandle,
                               testInfo->includeEntryList,
                               testInfo->excludePatternList,
                               Storage_getPrintableNameCString(testInfo->storageSpecifier,NULL),
                               testInfo->jobOptions,
                               testInfo->fragmentList,
                               buffer,
                               BUFFER_SIZE
                              );
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        error = testDirectoryEntry(&archiveHandle,
                                   testInfo->includeEntryList,
                                   testInfo->excludePatternList,
                                   Storage_getPrintableNameCString(testInfo->storageSpecifier,NULL),
                                   testInfo->jobOptions
                                  );
        break;
      case ARCHIVE_ENTRY_TYPE_LINK:
        error = testLinkEntry(&archiveHandle,
                              testInfo->includeEntryList,
                              testInfo->excludePatternList,
                              Storage_getPrintableNameCString(testInfo->storageSpecifier,NULL),
                              testInfo->jobOptions
                             );
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        error = testHardLinkEntry(&archiveHandle,
                                  testInfo->includeEntryList,
                                  testInfo->excludePatternList,
                                  Storage_getPrintableNameCString(testInfo->storageSpecifier,NULL),
                                  testInfo->jobOptions,
                                  testInfo->fragmentList,
                                  buffer,
                                  BUFFER_SIZE
                                 );
        break;
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
        error = testSpecialEntry(&archiveHandle,
                                 testInfo->includeEntryList,
                                 testInfo->excludePatternList,
                                 Storage_getPrintableNameCString(testInfo->storageSpecifier,NULL),
                                 testInfo->jobOptions
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
      if (testInfo->failError == ERROR_NONE) testInfo->failError = error;
    }

    // close archive
    Archive_close(&archiveHandle);

    freeEntryMsg(&entryMsg,NULL);
  }
  if (!isPrintInfo(1)) printInfo(0,"%s",(testInfo->failError == ERROR_NONE) ? "OK\n" : "FAIL!\n");

  // free resources
  free(buffer);
}

/***********************************************************************\
* Name   : testArchiveContent
* Purpose: test archive content
* Input  : storageSpecifier    - storage specifier
*          fileName            - file name (can be NULL)
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
                                ConstString         fileName,
                                const EntryList     *includeEntryList,
                                const PatternList   *excludePatternList,
                                DeltaSourceList     *deltaSourceList,
                                const JobOptions    *jobOptions,
                                GetPasswordFunction getPasswordFunction,
                                void                *getPasswordUserData,
                                FragmentList        *fragmentList,
                                LogHandle           *logHandle
                               )
{
  TestInfo             testInfo;
  StorageInfo          storageInfo;
  Errors               error;
  CryptSignatureStates allCryptSignatureState;
  Thread               *testThreads;
  uint                 testThreadCount;
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
  testThreadCount = (globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores();
  testThreads = (Thread*)malloc(testThreadCount*sizeof(Thread));
  if (testThreads == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init test info
  initTestInfo(&testInfo,
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
               Storage_getPrintableNameCString(storageSpecifier,NULL),
               Error_getText(error)
              );
    doneTestInfo(&testInfo);
    free(testThreads);
    return error;
  }

  // check signatures
  if (!jobOptions->skipVerifySignaturesFlag)
  {
    error = Archive_verifySignatures(&storageInfo,
                                     fileName,
                                     jobOptions,
                                     logHandle,
                                     &allCryptSignatureState
                                    );
    if (error != ERROR_NONE)
    {
      (void)Storage_done(&storageInfo);
      doneTestInfo(&testInfo);
      free(testThreads);
      return error;
    }
    if (   (allCryptSignatureState != CRYPT_SIGNATURE_STATE_NONE)
        && (allCryptSignatureState != CRYPT_SIGNATURE_STATE_OK)
       )
    {
      printError("Invalid signature in '%s'!\n",
                 Storage_getPrintableNameCString(storageSpecifier,fileName)
                );
      (void)Storage_done(&storageInfo);
      doneTestInfo(&testInfo);
      free(testThreads);
      return ERROR_INVALID_SIGNATURE;
    }
  }

  // start test threads
  for (i = 0; i < testThreadCount; i++)
  {
    if (!Thread_init(&testThreads[i],"BAR test",globalOptions.niceLevel,testThreadCode,&testInfo))
    {
      HALT_FATAL_ERROR("Cannot initialize test thread #%d!",i);
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
               Storage_getPrintableNameCString(storageSpecifier,NULL),
               Error_getText(error)
              );
    (void)Storage_done(&storageInfo);
    doneTestInfo(&testInfo);
    free(testThreads);
    return error;
  }

  // read archive entries
  printInfo(0,
            "Test storage '%s'%s",
            Storage_getPrintableNameCString(storageSpecifier,NULL),
            !isPrintInfo(1) ? "..." : ":\n"
           );
  failError = ERROR_NONE;
  while (!Archive_eof(&archiveHandle,FALSE,isPrintInfo(3)))
  {
    // get next archive entry type
    error = Archive_getNextArchiveEntry(&archiveHandle,
                                        &archiveEntryType,
                                        &offset,
                                        FALSE,
                                        isPrintInfo(3)
                                       );
    if (error != ERROR_NONE)
    {
      printError("Cannot read next entry in archive '%s' (error: %s)!\n",
                 Storage_getPrintableNameCString(storageSpecifier,NULL),
                 Error_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      break;
    }

    // send entry to test threads
    entryMsg.storageInfo      = &storageInfo;
    entryMsg.archiveEntryType = archiveEntryType;
    entryMsg.offset           = offset;
    if (!MsgQueue_put(&testInfo.entryMsgQueue,&entryMsg,sizeof(entryMsg)))
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

  // wait for test threads
  MsgQueue_setEndOfMsg(&testInfo.entryMsgQueue);
  for (i = 0; i < testThreadCount; i++)
  {
    if (!Thread_join(&testThreads[i]))
    {
      HALT_INTERNAL_ERROR("Cannot stop test thread #%d!",i);
    }
  }

  // done storage
  (void)Storage_done(&storageInfo);

  // done test info
  doneTestInfo(&testInfo);

  // free resources
  free(testThreads);

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
                                   NULL,  // fileName
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
                                        NULL,  // fileName
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
