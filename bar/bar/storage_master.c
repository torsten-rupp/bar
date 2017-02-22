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

#include "global.h"
#include "autofree.h"
#include "strings.h"
#include "stringlists.h"
#include "files.h"
#include "errors.h"

#include "errors.h"
//#include "crypt.h"
//#include "passwords.h"
#include "misc.h"
//#include "archive.h"
//#include "server.h"
//#include "bar_global.h"
#include "bar.h"
#include "server_io.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

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

LOCAL String StorageMaster_getName(StorageSpecifier *storageSpecifier,
                                   ConstString      archiveName
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

  String_clear(storageSpecifier->storageName);
  if (!String_isEmpty(storageFileName))
  {
    String_append(storageSpecifier->storageName,storageFileName);
  }

  return storageSpecifier->storageName;
}

LOCAL void StorageMaster_getPrintableName(String                 printableStorageName,
                                          const StorageSpecifier *storageSpecifier,
                                          ConstString            archiveName
                                         )
{
  ConstString storageFileName;

  assert(printableStorageName != NULL);
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
    String_append(printableStorageName,storageFileName);
  }
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
  assert(storageInfo->type == STORAGE_TYPE_MASTER);

  UNUSED_VARIABLE(storageInfo);

  return ERROR_NONE;
}

LOCAL bool StorageMaster_isServerAllocationPending(StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_MASTER);

  UNUSED_VARIABLE(storageInfo);

  return FALSE;
}

LOCAL Errors StorageMaster_preProcess(StorageInfo *storageInfo,
                                      ConstString archiveName,
                                      time_t      time,
                                      bool        initialFlag
                                     )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_MASTER);

  // init variables

  error = ServerIO_executeCommand(storageInfo->master.io,
                                  5LL*MS_PER_SECOND,
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

LOCAL Errors StorageMaster_postProcess(StorageInfo *storageInfo,
                                       ConstString archiveName,
                                       time_t      time,
                                       bool        finalFlag
                                      )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_MASTER);

  // init variables

  error = ServerIO_executeCommand(storageInfo->master.io,
                                  5LL*MS_PER_SECOND,
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

LOCAL bool StorageMaster_exists(StorageInfo *storageInfo, ConstString fileName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(fileName));

  UNUSED_VARIABLE(storageInfo);

HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
//TODO
return TRUE;
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
  storageHandle->master.index = 0LL;
  storageHandle->master.size  = 0LL;

  error = ServerIO_executeCommand(storageHandle->storageInfo->master.io,
                                  5LL*MS_PER_SECOND,
                                  NULL,  // resultMap
                                  "STORAGE_CREATE archiveName=%S archiveSize=%llu",
                                  fileName,
                                  fileSize
                                 );
  if (error != ERROR_NONE)
  {
fprintf(stderr,"%s, %d: EEE %s\n",__FILE__,__LINE__,Error_getText(error));
    return error;
  }

  // free resources

  DEBUG_ADD_RESOURCE_TRACE(&storageHandle->master,sizeof(storageHandle->master));

  return ERROR_NONE;
}

LOCAL Errors StorageMaster_open(StorageHandle *storageHandle,
                                ConstString   fileName
                               )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);
  assert(!String_isEmpty(fileName));

  // init variables
  storageHandle->mode = STORAGE_MODE_READ;
  storageHandle->master.index = 0LL;
//TODO:
storageHandle->master.size = 0LL;

return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL void StorageMaster_close(StorageHandle *storageHandle)
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->master);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);

  DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->master,sizeof(storageHandle->master));

  error = ServerIO_executeCommand(storageHandle->storageInfo->master.io,
                                  30LL*MS_PER_SECOND,
                                  NULL,  // resultMap
                                  "STORAGE_CLOSE"
                                 );
  if (error != ERROR_NONE)
  {
//TODO
fprintf(stderr,"%s, %d: EEE %s\n",__FILE__,__LINE__,Error_getText(error));
  }
}

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
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->master);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);
  assert(buffer != NULL);

return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL Errors StorageMaster_write(StorageHandle *storageHandle,
                                 const void    *buffer,
                                 ulong         bufferLength
                                )
{
  String     encodedData;
  const byte *p;
  ulong      writtenBytes;
  ulong      n;
  Errors     error;

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
  while (writtenBytes < bufferLength)
  {
    // encode data (max. 4096bytes in single write)
    n = MIN(bufferLength-writtenBytes,4096);
    Misc_base64Encode(encodedData,p,n);

    // send data
    error = ServerIO_executeCommand(storageHandle->storageInfo->master.io,
                                    30LL*MS_PER_SECOND,
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

  if (storageHandle->master.index > storageHandle->master.size)
  {
    storageHandle->master.size = storageHandle->master.index;
  }

  // free resources
  String_delete(encodedData);

  return error;
}

LOCAL Errors StorageMaster_tell(StorageHandle *storageHandle,
                                uint64        *offset
                               )
{
  Errors error;

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
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->master);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);

  storageHandle->master.index = offset;

  return ERROR_NONE;
}

LOCAL uint64 StorageMaster_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->master);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);

  return storageHandle->master.size;
}

LOCAL Errors StorageMaster_delete(StorageInfo *storageInfo,
                                  ConstString fileName
                                 )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_MASTER);
  assert(!String_isEmpty(fileName));

return ERROR_STILL_NOT_IMPLEMENTED;
}

#if 0
still not complete
LOCAL Errors StorageMaster_getFileInfo(StorageInfo *storageInfo,
                                       ConstString fileName,
                                       FileInfo    *fileInfo
                                      )
{
  String infoFileName;
  Errors error;

  assert(storageInfo != NULL);
  assert(fileInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_MASTER);

  error = File_getFileInfo(infoFileName,fileInfo);

  return error;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageMaster_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                             const StorageSpecifier     *storageSpecifier,
                                             ConstString                pathName,
                                             const JobOptions           *jobOptions,
                                             ServerConnectionPriorities serverConnectionPriority
                                            )
{
  Errors error;

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
  Errors error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_MASTER);

return ERROR_STILL_NOT_IMPLEMENTED;

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
