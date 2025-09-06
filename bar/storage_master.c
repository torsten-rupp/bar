/***********************************************************************\
*
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
//#include "archives.h"
//#include "server.h"
//#include "bar_global.h"
#include "bar.h"
#include "server_io.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define MASTER_DEBUG_LEVEL      1
#define MASTER_DEBUG_LEVEL_DATA 2
#define MASTER_COMMAND_TIMEOUT  (60LL*MS_PER_SECOND)

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

LOCAL Errors StorageMaster_done(StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->jobOptions->storageOnMasterFlag);

  UNUSED_VARIABLE(storageInfo);

  return ERROR_NONE;
}

LOCAL Errors StorageMaster_preProcess(const StorageInfo *storageInfo,
                                      ConstString       archiveName,
                                      time_t            time,
                                      bool              initialFlag
                                     )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->jobOptions->storageOnMasterFlag);

  error = ERROR_NONE;

  if (!initialFlag)
  {
    // init macros
    TextMacros (textMacros,2);
    TEXT_MACROS_INIT(textMacros)
    {
      TEXT_MACRO_X_STRING("%file",  archiveName,              NULL);
      TEXT_MACRO_X_INT   ("%number",storageInfo->volumeNumber,NULL);
    }

    if (!String_isEmpty(globalOptions.file.writePreProcessCommand))
    {
      printInfo(1,"Write pre-processing...");
// TODO: .file. -> master
      error = executeTemplate(String_cString(globalOptions.file.writePreProcessCommand),
                              time,
                              textMacros.data,
                              textMacros.count,
                              CALLBACK_(executeIOOutput,NULL),
                              globalOptions.commandTimeout
                             );
      printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
    }
  }

  return error;
}

LOCAL Errors StorageMaster_postProcess(const StorageInfo *storageInfo,
                                       ConstString       archiveName,
                                       time_t            time,
                                       bool              finalFlag
                                      )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->jobOptions->storageOnMasterFlag);

  error = ERROR_NONE;

  if (!finalFlag)
  {
    // init macros
    TextMacros (textMacros,2);
    TEXT_MACROS_INIT(textMacros)
    {
      TEXT_MACRO_X_STRING ("%file",  archiveName,              NULL);
      TEXT_MACRO_X_INT("%number",storageInfo->volumeNumber,NULL);
    }

    if (!String_isEmpty(globalOptions.file.writePostProcessCommand))
    {
      printInfo(1,"Write post-processing...");
      error = executeTemplate(String_cString(globalOptions.file.writePostProcessCommand),
                              time,
                              textMacros.data,
                              textMacros.count,
                              CALLBACK_(executeIOOutput,NULL),
                              globalOptions.commandTimeout
                             );
      printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
    }
  }

  return error;
}

LOCAL bool StorageMaster_exists(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  bool existsFlag = FALSE;

  Errors error = ServerIO_executeCommand(storageInfo->masterIO,
                                         MASTER_DEBUG_LEVEL,
                                         MASTER_COMMAND_TIMEOUT,
                                         CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                         {
                                           assert(resultMap != NULL);

                                           UNUSED_VARIABLE(userData);

                                           StringMap_getBool(resultMap,"existsFlag",&existsFlag,FALSE);

                                           return ERROR_NONE;
                                         },NULL),
                                         "STORAGE_EXISTS archiveName=%'S",
                                         archiveName
                                        );
  if (error != ERROR_NONE)
  {
    existsFlag = FALSE;
  }

  return existsFlag;
}

LOCAL Errors StorageMaster_getTmpName(String archiveName, const StorageInfo *storageInfo)
{
  assert(archiveName != NULL);
  assert(!String_isEmpty(archiveName));
  assert(storageInfo != NULL);

  UNUSED_VARIABLE(archiveName);
  UNUSED_VARIABLE(storageInfo);

//TODO
  return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL Errors StorageMaster_create(StorageHandle *storageHandle,
                                  ConstString   fileName,
                                  uint64        fileSize,
                                  bool          forceFlag
                                 )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->jobOptions->storageOnMasterFlag);
  assert(!String_isEmpty(fileName));

  UNUSED_VARIABLE(fileSize);
  UNUSED_VARIABLE(forceFlag);

  // init variables
  storageHandle->master.index = 0LL;
  storageHandle->master.size  = 0LL;

  error = ServerIO_executeCommand(storageHandle->storageInfo->masterIO,
                                  MASTER_DEBUG_LEVEL,
                                  MASTER_COMMAND_TIMEOUT,
                                  CALLBACK_(NULL,NULL),  // commandResultFunction
                                  "STORAGE_CREATE archiveName=%'S archiveSize=%"PRIu64,
                                  fileName,
                                  fileSize
                                 );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // free resources

  return ERROR_NONE;
}

LOCAL Errors StorageMaster_open(StorageHandle *storageHandle,
                                ConstString   fileName
                               )
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->jobOptions->storageOnMasterFlag);
  assert(!String_isEmpty(fileName));

  // init variables
  storageHandle->master.index = 0LL;
UNUSED_VARIABLE(fileName);

return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL void StorageMaster_close(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->jobOptions->storageOnMasterFlag);

  // Note: ignore error
  (void)ServerIO_executeCommand(storageHandle->storageInfo->masterIO,
                                MASTER_DEBUG_LEVEL,
                                MASTER_COMMAND_TIMEOUT,
                                CALLBACK_(NULL,NULL),  // commandResultFunction
                                "STORAGE_CLOSE"
                               );
}

//TODO: required?
#if 0
LOCAL bool StorageMaster_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->jobOptions->storageOnMaster);

  return storageHandle->master.index >= storageHandle->master.size;
}

LOCAL Errors StorageMaster_read(StorageHandle *storageHandle,
                                void          *buffer,
                                ulong         bufferSize,
                                ulong         *readBytes
                               )
{
//  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->jobOptions->storageOnMaster);
  assert(buffer != NULL);

UNUSED_VARIABLE(buffer);
UNUSED_VARIABLE(bufferSize);
UNUSED_VARIABLE(readBytes);
return ERROR_STILL_NOT_IMPLEMENTED;
}
#endif

LOCAL Errors StorageMaster_write(StorageHandle *storageHandle,
                                 const void    *buffer,
                                 ulong         bufferLength
                                )
{
  const uint MAX_BLOCK_SIZE = 32*1024;
  const uint MAX_BLOCKS     = 16;  // max. number of pending transfer blocks

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storageInfo->jobOptions->storageOnMasterFlag);
  assert(buffer != NULL);

  Errors error = ERROR_NONE;

  String     encodedData  = String_new();
  const byte *p           = (const byte*)buffer;
  ulong      writtenBytes = 0L;
  uint       ids[MAX_BLOCKS];
  uint       idCount      = 0;
  while (writtenBytes < bufferLength)
  {
    ulong length = MIN(bufferLength-writtenBytes,MAX_BLOCK_SIZE);

    // encode data
    Misc_base64Encode(String_clear(encodedData),p+writtenBytes,length);

    // send data
//fprintf(stderr,"%s, %d: n=%"PRIu64"\n",__FILE__,__LINE__,n);
    error = ServerIO_sendCommand(storageHandle->storageInfo->masterIO,
                                 MASTER_DEBUG_LEVEL_DATA,
                                 &ids[idCount],
                                 "STORAGE_WRITE offset=%"PRIu64" length=%u data=%s",
//TODO
                                 storageHandle->master.index,
                                 length,
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
      uint i;
      error = ServerIO_waitResults(storageHandle->storageInfo->masterIO,
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
    writtenBytes += length;
    storageHandle->master.index += (uint64)length;
  }
  while (idCount > 0)
  {
    uint i;
    error = ServerIO_waitResults(storageHandle->storageInfo->masterIO,
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
  const uint MAX_BLOCK_SIZE = 32*1024;
  const uint MAX_BLOCKS     = 16;  // max. number of pending transfer blocks

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storageInfo->jobOptions->storageOnMasterFlag);

  Errors error;

  // seek to begin of file
  error = File_seek(fileHandle,0LL);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get total size
  uint64 size = File_getSize(fileHandle);

  void   *buffer         = malloc(MAX_BLOCK_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  String encodedData     = String_new();
  ulong  transferedBytes = 0L;
  uint   ids[MAX_BLOCKS];
  uint   idCount         = 0;
  while (transferedBytes < size)
  {
    ulong length = (ulong)MIN(size-transferedBytes,MAX_BLOCK_SIZE);

    // read data
    error = File_read(fileHandle,buffer,length,NULL);
    if (error != ERROR_NONE)
    {
      String_delete(encodedData);
      free(buffer);
      return error;
    }

    // encode data
    Misc_base64Encode(String_clear(encodedData),buffer,length);

    // send data
    error = ServerIO_sendCommand(storageHandle->storageInfo->masterIO,
                                 MASTER_DEBUG_LEVEL_DATA,
                                 &ids[idCount],
                                 "STORAGE_WRITE offset=%"PRIu64" length=%u data=%s",
//TODO
                                 storageHandle->master.index,
                                 length,
                                 String_cString(encodedData)
                                );
    if (error != ERROR_NONE)
    {
      String_delete(encodedData);
      free(buffer);
      return error;
    }
//fprintf(stderr,"%s, %d: sent %d\n",__FILE__,__LINE__,ids[idCount]);
    idCount++;

    // wait for result
    if (idCount >= MAX_BLOCKS)
    {
      uint i;
      error = ServerIO_waitResults(storageHandle->storageInfo->masterIO,
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
        free(buffer);
        return error;
      }
//fprintf(stderr,"%s, %d: ack %d\n",__FILE__,__LINE__,ids[i]);
      ids[i] = ids[idCount-1];
      idCount--;
    }

    // next part
    transferedBytes += (uint64)length;
    storageHandle->master.index += (uint64)length;

    // update running info
    storageHandle->storageInfo->progress.storageDoneBytes += (uint64)length;
    if (!updateStorageRunningInfo(storageHandle->storageInfo))
    {
      String_delete(encodedData);
      free(buffer);
      return ERROR_ABORTED;
    }
  }
  while (idCount > 0)
  {
    uint i;
    error = ServerIO_waitResults(storageHandle->storageInfo->masterIO,
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
      free(buffer);
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
  free(buffer);

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
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->jobOptions->storageOnMaster);
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
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->jobOptions->storageOnMasterFlag);

  storageHandle->master.index = offset;

  return ERROR_NONE;
}
#endif

LOCAL uint64 StorageMaster_getSize(StorageHandle *storageHandle)
{
//  uint64 size;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->jobOptions->storageOnMasterFlag);

  return storageHandle->master.size;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
