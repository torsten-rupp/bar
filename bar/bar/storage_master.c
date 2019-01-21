/***********************************************************************\
*
* $Revision: 4036 $
* $Date: 2015-05-30 01:48:57 +0200 (Sat, 30 May 2015) $
* $Author: torsten $
* Contents: storage master functions
* Systems: all
*
\***********************************************************************/

#define __STORAGE_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>

#include "common/global.h"
#include "common/autofree.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/files.h"

#include "errors.h"
//#include "crypt.h"
//#include "passwords.h"
#include "common/misc.h"
//#include "archive.h"
//#include "server.h"
//#include "bar_global.h"
#include "bar.h"
#include "server_io.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define MASTER_DEBUG_LEVEL      1
#define MASTER_DEBUG_LEVEL_DATA 2
#define MASTER_COMMAND_TIMEOUT  (30LL*MS_PER_SECOND)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

LOCAL Errors StorageMaster_initAll(void)
{
  return ERROR_NONE;
}

LOCAL void StorageMaster_doneAll(void)
{
}

//TODO: required?
#if 0
LOCAL bool StorageMaster_equalSpecifiers(const StorageSpecifier *storageSpecifier1,
                                         ConstString            archiveName1,
                                         const StorageSpecifier *storageSpecifier2,
                                         ConstString            archiveName2
                                        )
{
  assert(storageSpecifier1 != NULL);
  assert(storageSpecifier1->type == STORAGE_TYPE_MASTER);
  assert(storageSpecifier2 != NULL);
  assert(storageSpecifier2->type == STORAGE_TYPE_MASTER);

  if (archiveName1 == NULL) archiveName1 = storageSpecifier1->archiveName;
  if (archiveName2 == NULL) archiveName2 = storageSpecifier2->archiveName;

  return String_equals(archiveName1,archiveName2);
}

LOCAL String StorageMaster_getName(String                 string,
                                   const StorageSpecifier *storageSpecifier,
                                   ConstString            archiveName
                                  )
{
  ConstString storageFileName;

  assert(storageSpecifier != NULL);

  // get file to use
  if      (!String_isEmpty(archiveName))
  {
    storageFileName = archiveName;
  }
  else if (storageSpecifier->archivePatternString != NULL)
  {
    storageFileName = storageSpecifier->archivePatternString;
  }
  else
  {
    storageFileName = storageSpecifier->archiveName;
  }

  String_clear(string);
  if (!String_isEmpty(storageFileName))
  {
    String_append(string,storageFileName);
  }

  return string;
}

LOCAL void StorageMaster_getPrintableName(String                 string,
                                          const StorageSpecifier *storageSpecifier,
                                          ConstString            archiveName
                                         )
{
  ConstString storageFileName;

  assert(string != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_MASTER);

  // get file to use
  if      (!String_isEmpty(archiveName))
  {
    storageFileName = archiveName;
  }
  else if (!String_isEmpty(storageSpecifier->archivePatternString))
  {
    storageFileName = storageSpecifier->archivePatternString;
  }
  else
  {
    storageFileName = storageSpecifier->archiveName;
  }

  if (!String_isEmpty(storageFileName))
  {
    String_append(string,storageFileName);
  }
}
#endif

LOCAL Errors StorageMaster_init(StorageInfo            *storageInfo,
                                const StorageSpecifier *storageSpecifier,
                                const JobOptions       *jobOptions
                               )
{
  assert(storageInfo != NULL);
  assert(storageSpecifier != NULL);

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(jobOptions);

  return ERROR_NONE;
}

//TODO: required?
#if 0
LOCAL Errors StorageMaster_done(StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_MASTER);

  UNUSED_VARIABLE(storageInfo);

  return ERROR_NONE;
}

LOCAL bool StorageMaster_isServerAllocationPending(const StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_MASTER);

  UNUSED_VARIABLE(storageInfo);

  return FALSE;
}
#endif

LOCAL Errors StorageMaster_preProcess(const StorageInfo *storageInfo,
                                      ConstString       archiveName,
                                      time_t            time,
                                      bool              initialFlag
                                     )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_MASTER);

  // init variables

  // pre-process command
  error = ServerIO_executeCommand(storageInfo->master.io,
                                  MASTER_DEBUG_LEVEL,
                                  MASTER_COMMAND_TIMEOUT,
                                  NULL,  // resultMap
                                  "PREPROCESS archiveName=%S time=%llu initialFlag=%y",
//TODO: if empty/NULL?
                                  archiveName,
                                  time,
                                  initialFlag
                                 );
  if (error != ERROR_NONE)
  {
fprintf(stderr,"%s, %d: EEE %s\n",__FILE__,__LINE__,Error_getText(error));
    return error;
  }

  // free resources

  return ERROR_NONE;
}

LOCAL Errors StorageMaster_postProcess(const StorageInfo *storageInfo,
                                       ConstString       archiveName,
                                       time_t            time,
                                       bool              finalFlag
                                      )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_MASTER);

  // init variables

  // post-process command
  error = ServerIO_executeCommand(storageInfo->master.io,
                                  MASTER_DEBUG_LEVEL,
                                  MASTER_COMMAND_TIMEOUT,
                                  NULL,  // resultMap
                                  "POSTPROCESS archiveName=%S time=%llu finalFlag=%y",
//TODO: if empty/NULL?
                                  archiveName,
                                  time,
                                  finalFlag
                                 );
  if (error != ERROR_NONE)
  {
fprintf(stderr,"%s, %d: EEE %s\n",__FILE__,__LINE__,Error_getText(error));
    return error;
  }

  // free resources

  return ERROR_NONE;
}

//TODO: required?
#if 0
LOCAL bool StorageMaster_exists(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);

HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
//TODO
return TRUE;
}

LOCAL bool StorageMaster_isFile(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);

  return File_exists(archiveName);
}

LOCAL bool StorageMaster_isDirectory(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);

  return File_exists(archiveName);
}

LOCAL bool StorageMaster_isReadable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);

  return File_exists(archiveName);
}

LOCAL bool StorageMaster_isWriteable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);

  return File_exists(archiveName);
}
#endif

LOCAL Errors StorageMaster_getTmpName(String archiveName, const StorageInfo *storageInfo)
{
  assert(archiveName != NULL);
  assert(!String_isEmpty(archiveName));
  assert(storageInfo != NULL);

  UNUSED_VARIABLE(storageInfo);

return ERROR_STILL_NOT_IMPLEMENTED;
  return File_getTmpFileName(archiveName,String_cString(archiveName),NULL);
}

LOCAL Errors StorageMaster_create(StorageHandle *storageHandle,
                                  ConstString   fileName,
                                  uint64        fileSize
                                 )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);
  assert(!String_isEmpty(fileName));

  UNUSED_VARIABLE(fileSize);

  // init variables
  storageHandle->master.buffer = malloc(BUFFER_SIZE);
  if (storageHandle->master.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  storageHandle->master.index = 0LL;
  storageHandle->master.size  = 0LL;

  error = ServerIO_executeCommand(storageHandle->storageInfo->master.io,
                                  MASTER_DEBUG_LEVEL,
                                  MASTER_COMMAND_TIMEOUT,
                                  NULL,  // resultMap
                                  "STORAGE_CREATE archiveName=%'S archiveSize=%llu",
                                  fileName,
                                  fileSize
                                 );
  if (error != ERROR_NONE)
  {
fprintf(stderr,"%s, %d: EEE %s\n",__FILE__,__LINE__,Error_getText(error));
    return error;
  }

  // free resources

  DEBUG_ADD_RESOURCE_TRACE(&storageHandle->master,StorageHandleMaster);

  return ERROR_NONE;
}

LOCAL Errors StorageMaster_open(StorageHandle *storageHandle,
                                ConstString   fileName
                               )
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);
  assert(!String_isEmpty(fileName));

  // init variables
  storageHandle->master.buffer = malloc(BUFFER_SIZE);
  if (storageHandle->master.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  storageHandle->master.index = 0LL;
UNUSED_VARIABLE(fileName);

return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL void StorageMaster_close(StorageHandle *storageHandle)
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->master);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);

  DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->master,StorageHandleMaster);

  error = ServerIO_executeCommand(storageHandle->storageInfo->master.io,
                                  MASTER_DEBUG_LEVEL,
                                  MASTER_COMMAND_TIMEOUT,
                                  NULL,  // resultMap
                                  "STORAGE_CLOSE"
                                 );
  if (error != ERROR_NONE)
  {
//TODO
fprintf(stderr,"%s, %d: EEE %s\n",__FILE__,__LINE__,Error_getText(error));
  }

  // free resources
  free(storageHandle->master.buffer);
}

//TODO: required?
#if 0
LOCAL bool StorageMaster_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->master);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);

  return storageHandle->master.index >= storageHandle->master.size;
}

LOCAL Errors StorageMaster_read(StorageHandle *storageHandle,
                                void          *buffer,
                                ulong         bufferSize,
                                ulong         *bytesRead
                               )
{
//  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->master);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);
  assert(buffer != NULL);

UNUSED_VARIABLE(buffer);
UNUSED_VARIABLE(bufferSize);
UNUSED_VARIABLE(bytesRead);
return ERROR_STILL_NOT_IMPLEMENTED;
}
#endif

LOCAL Errors StorageMaster_write(StorageHandle *storageHandle,
                                 const void    *buffer,
                                 ulong         bufferLength
                                )
{
  const uint MAX_BLOCK_SIZE = 32*1024;
  const uint MAX_BLOCKS     = 4;

  String     encodedData;
  const byte *p;
  uint       ids[MAX_BLOCKS];
  uint       idCount;
  ulong      writtenBytes;
  ulong      n;
  Errors     error;
  uint       i;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->master);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);
  assert(buffer != NULL);

  // init variables
  encodedData = String_new();

  p            = (const byte*)buffer;
  writtenBytes = 0L;
#if 0
//TODO: remove single block transmit
  while (writtenBytes < bufferLength)
  {
    // encode data
    n = MIN(bufferLength-writtenBytes,MAX_BLOCK_SIZE);
    Misc_base64Encode(String_clear(encodedData),p,n);

    // send data
    error = ServerIO_executeCommand(storageHandle->storageInfo->master.io,
                                    MASTER_DEBUG_LEVEL_DATA,
                                    MASTER_COMMAND_TIMEOUT,
                                    NULL,  // resultMap
                                    "STORAGE_WRITE offset=%llu length=%u data=%s",
//TODO
                                    storageHandle->master.index,
                                    n,
                                    String_cString(encodedData)
                                   );
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s, %d: EEE %s\n",__FILE__,__LINE__,Error_getText(error));
      String_delete(encodedData);
      return error;
    }

    // next part
    p += n;
    writtenBytes += n;
    storageHandle->master.index += (uint64)n;
  }
#else
  idCount = 0;
  while (writtenBytes < bufferLength)
  {
    // encode data
    n = MIN(bufferLength-writtenBytes,MAX_BLOCK_SIZE);
    Misc_base64Encode(String_clear(encodedData),p,n);

    // send data
    error = ServerIO_sendCommand(storageHandle->storageInfo->master.io,
                                 MASTER_DEBUG_LEVEL_DATA,
                                 &ids[idCount],
                                 "STORAGE_WRITE offset=%llu length=%u data=%s",
//TODO
                                 storageHandle->master.index,
                                 n,
                                 String_cString(encodedData)
                                );
    if (error != ERROR_NONE)
    {
      String_delete(encodedData);
      return error;
    }
    idCount++;

    // wait for result
    if (idCount >= MAX_BLOCKS)
    {
      error = ServerIO_waitResults(storageHandle->storageInfo->master.io,
                                   MASTER_COMMAND_TIMEOUT,
                                   ids,
                                   idCount,
                                   &i,
                                   NULL, // &completedFlag,
                                   NULL  // resultMap
                                  );
      if (error != ERROR_NONE)
      {
        String_delete(encodedData);
        return error;
      }
      ids[i] = ids[idCount-1];
      idCount--;
    }

    // next part
    p += n;
    writtenBytes += n;
    storageHandle->master.index += (uint64)n;
  }
  while (idCount > 0)
  {
    error = ServerIO_waitResults(storageHandle->storageInfo->master.io,
                                 MASTER_COMMAND_TIMEOUT,
                                 ids,
                                 idCount,
                                 &i,
                                 NULL, // &completedFlag,
                                 NULL  // resultMap
                                );
    if (error != ERROR_NONE)
    {
      String_delete(encodedData);
      return error;
    }
    ids[i] = ids[idCount-1];
    idCount--;
  }
#endif

  if (storageHandle->master.index > storageHandle->master.size)
  {
    storageHandle->master.size = storageHandle->master.index;
  }

  // free resources
  String_delete(encodedData);

  return error;
}

LOCAL Errors StorageMaster_transfer(StorageHandle *storageHandle,
                                    FileHandle    *fileHandle
                                   )
{
  const uint MAX_BLOCKS = 16;  // max. number of pending transfer blocks

  String encodedData;
  uint64 size;
  ulong  transferedBytes;
  uint   ids[MAX_BLOCKS];
  uint   idCount;
  ulong  n;
  Errors error;
  uint   i;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->master);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);

  // init variables
  encodedData = String_new();

  // seek to begin of file
  error = File_seek(fileHandle,0LL);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get total size
  size = File_getSize(fileHandle);

  transferedBytes = 0L;
  idCount         = 0;
  while (transferedBytes < size)
  {
    n = (ulong)MIN(size-transferedBytes,BUFFER_SIZE);

    // read data
    error = File_read(fileHandle,storageHandle->master.buffer,n,NULL);
    if (error != ERROR_NONE)
    {
      String_delete(encodedData);
      return error;
    }

    // encode data
    Misc_base64Encode(String_clear(encodedData),storageHandle->master.buffer,n);

    // send data
    error = ServerIO_sendCommand(storageHandle->storageInfo->master.io,
                                 MASTER_DEBUG_LEVEL_DATA,
                                 &ids[idCount],
                                 "STORAGE_WRITE offset=%llu length=%u data=%s",
//TODO
                                 storageHandle->master.index,
                                 n,
                                 String_cString(encodedData)
                                );
    if (error != ERROR_NONE)
    {
      String_delete(encodedData);
      return error;
    }
//fprintf(stderr,"%s, %d: sent %d\n",__FILE__,__LINE__,ids[idCount]);
    idCount++;

    // wait for result
    if (idCount >= MAX_BLOCKS)
    {
      error = ServerIO_waitResults(storageHandle->storageInfo->master.io,
                                   MASTER_COMMAND_TIMEOUT,
                                   ids,
                                   idCount,
                                   &i,
                                   NULL, // &completedFlag,
                                   NULL  // resultMap
                                  );
      if (error != ERROR_NONE)
      {
        String_delete(encodedData);
        return error;
      }
//fprintf(stderr,"%s, %d: ack %d\n",__FILE__,__LINE__,ids[i]);
      ids[i] = ids[idCount-1];
      idCount--;
    }

    // next part
    transferedBytes += (uint64)n;
    storageHandle->master.index += (uint64)n;

    // update status info
    storageHandle->storageInfo->runningInfo.storageDoneBytes = (uint64)n;
    if (!updateStorageStatusInfo(storageHandle->storageInfo))
    {
      String_delete(encodedData);
      return ERROR_ABORTED;
    }
  }
  while (idCount > 0)
  {
    error = ServerIO_waitResults(storageHandle->storageInfo->master.io,
                                 MASTER_COMMAND_TIMEOUT,
                                 ids,
                                 idCount,
                                 &i,
                                 NULL, // &completedFlag,
                                 NULL  // resultMap
                                );
    if (error != ERROR_NONE)
    {
      String_delete(encodedData);
      return error;
    }
//fprintf(stderr,"%s, %d: ack %d\n",__FILE__,__LINE__,ids[i]);
    ids[i] = ids[idCount-1];
    idCount--;
  }

  if (storageHandle->master.index > storageHandle->master.size)
  {
    storageHandle->master.size = storageHandle->master.index;
  }

  // free resources
  String_delete(encodedData);

  return ERROR_NONE;
}

//TODO: required?
#if 0
LOCAL Errors StorageMaster_tell(StorageHandle *storageHandle,
                                uint64        *offset
                               )
{
//  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->master);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);
  assert(offset != NULL);

  (*offset) = storageHandle->master.index;

  return ERROR_NONE;
}

LOCAL Errors StorageMaster_seek(StorageHandle *storageHandle,
                                uint64        offset
                               )
{
//  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->master);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);

  storageHandle->master.index = offset;

  return ERROR_NONE;
}
#endif

LOCAL uint64 StorageMaster_getSize(StorageHandle *storageHandle)
{
//  uint64 size;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->master);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);

  return storageHandle->master.size;
}

//TODO: required?
#if 0
LOCAL Errors StorageMaster_rename(const StorageInfo *storageInfo,
                                  ConstString       fromArchiveName,
                                  ConstString       toArchiveName
                                 )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_MASTER);

UNUSED_VARIABLE(storageInfo);
UNUSED_VARIABLE(fromArchiveName);
UNUSED_VARIABLE(toArchiveName);
error = ERROR_STILL_NOT_IMPLEMENTED;

  return error;
}

LOCAL Errors StorageMaster_delete(const StorageInfo *storageInfo,
                                  ConstString       fileName
                                 )
{
//  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_MASTER);
  assert(!String_isEmpty(fileName));
x

return ERROR_(STILL_NOT_IMPLEMENTED,0,"");
}
#endif

#if 0
still not complete
LOCAL Errors StorageMaster_getInfo(const StorageInfo *storageInfo,
                                   ConstString       fileName,
                                   FileInfo          *fileInfo
                                  )
{
  String infoFileName;
  Errors error;

  assert(storageInfo != NULL);
  assert(fileInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_MASTER);

  error = File_getInfo(fileInfo,infoFileName);

  return error;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

//TODO: required?
#if 0
LOCAL Errors StorageMaster_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                             const StorageSpecifier     *storageSpecifier,
                                             ConstString                pathName,
                                             const JobOptions           *jobOptions,
                                             ServerConnectionPriorities serverConnectionPriority
                                            )
{
//  Errors error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_MASTER);
  assert(!String_isEmpty(pathName));

  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(jobOptions);
  UNUSED_VARIABLE(serverConnectionPriority);

  // init variables
  storageDirectoryListHandle->type = STORAGE_TYPE_MASTER;

return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL void StorageMaster_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_MASTER);
}

LOCAL bool StorageMaster_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_MASTER);

return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL Errors StorageMaster_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                             String                     fileName,
                                             FileInfo                   *fileInfo
                                            )
{
//  Errors error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_MASTER);

UNUSED_VARIABLE(fileName);
UNUSED_VARIABLE(fileInfo);
return ERROR_STILL_NOT_IMPLEMENTED;
//  return error;
}
#endif

#ifdef __cplusplus
  }
#endif

/* end of file */
