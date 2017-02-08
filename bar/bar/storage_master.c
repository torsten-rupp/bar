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
  TextMacro textMacros[2];
  String    script;
  Errors    error;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_MASTER);

  error = ERROR_NONE;

  if ((storageInfo->jobOptions == NULL) || !storageInfo->jobOptions->dryRunFlag)
  {
    if (!initialFlag)
    {
      // init macros
      TEXT_MACRO_N_STRING (textMacros[0],"%file",  archiveName,                NULL);
      TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageInfo->volumeNumber,NULL);

      if (globalOptions.file.writePreProcessCommand != NULL)
      {
        // write pre-processing
        printInfo(1,"Write pre-processing...");

        // get script
        script = expandTemplate(String_cString(globalOptions.file.writePreProcessCommand),
                                EXPAND_MACRO_MODE_STRING,
                                time,
                                initialFlag,
                                textMacros,
                                SIZE_OF_ARRAY(textMacros)
                               );
        if (script != NULL)
        {
          // execute script
          error = Misc_executeScript(String_cString(script),
                                     CALLBACK(executeIOOutput,NULL),
                                     CALLBACK(executeIOOutput,NULL)
                                    );
          String_delete(script);
        }
        else
        {
          error = ERROR_EXPAND_TEMPLATE;
        }

        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }
    }
  }

  return error;
}

LOCAL Errors StorageMaster_postProcess(StorageInfo *storageInfo,
                                       ConstString archiveName,
                                       time_t      time,
                                       bool        finalFlag
                                      )
{
  TextMacro textMacros[2];
  String    script;
  Errors    error;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_MASTER);

  error = ERROR_NONE;

  if ((storageInfo->jobOptions == NULL) || !storageInfo->jobOptions->dryRunFlag)
  {
    if (!finalFlag)
    {
      // init macros
      TEXT_MACRO_N_STRING (textMacros[0],"%file",  archiveName,              NULL);
      TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageInfo->volumeNumber,NULL);

      if (globalOptions.file.writePostProcessCommand != NULL)
      {
        // write post-process
        printInfo(1,"Write post-processing...");

        // get script
        script = expandTemplate(String_cString(globalOptions.file.writePostProcessCommand),
                                EXPAND_MACRO_MODE_STRING,
                                time,
                                finalFlag,
                                textMacros,
                                SIZE_OF_ARRAY(textMacros)
                               );
        if (script != NULL)
        {
          // execute script
          error = Misc_executeScript(String_cString(script),
                                     CALLBACK(executeIOOutput,NULL),
                                     CALLBACK(executeIOOutput,NULL)
                                    );
          String_delete(script);
        }
        else
        {
          error = ERROR_EXPAND_TEMPLATE;
        }

        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }
    }
  }

  return error;
}

LOCAL bool StorageMaster_exists(StorageInfo *storageInfo, ConstString fileName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(fileName));

  UNUSED_VARIABLE(storageInfo);

  return File_exists(fileName);
}

LOCAL Errors StorageMaster_create(StorageHandle *storageHandle,
                                  ConstString   fileName,
                                  uint64        fileSize
                                 )
{
  Errors error;
  String directoryName;
ServerIOResultList serverResultList;
  uint id;
  StringMap resultMap;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);
  assert(!String_isEmpty(fileName));

  UNUSED_VARIABLE(fileSize);

  // init variables
  resultMap = StringMap_new();

fprintf(stderr,"%s, %d: StorageMaster_create\n",__FILE__,__LINE__);
  error = ServerIO_sendCommand(storageHandle->storageInfo->master.io,
                               &id,
                               "STORAGE_CREATE name=%S size=%llu",
                               fileName,
                               fileSize
                              );
  if (error != ERROR_NONE)
  {
fprintf(stderr,"%s, %d: EEE %s\n",__FILE__,__LINE__,Error_getText(error));
    StringMap_delete(resultMap);
    return error;
  }

fprintf(stderr,"%s, %d: wait for %d\n",__FILE__,__LINE__,id);
  error = ServerIO_waitResult(storageHandle->storageInfo->master.io,
100*                              30LL*MS_PER_SECOND,
                              id,
                              NULL,  // error
                              NULL,  // completedFlag
                              resultMap
                             );
  if (error != ERROR_NONE)
  {
fprintf(stderr,"%s, %d: EEE %d: %s\n",__FILE__,__LINE__,id,Error_getText(error));
    StringMap_delete(resultMap);
    return error;
  }

  // free resources
  StringMap_delete(resultMap);

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

  // check if file exists
  if (!File_exists(fileName))
  {
    return ERRORX_(FILE_NOT_FOUND_,0,"%s",String_cString(fileName));
  }

  // open file
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
  error = File_open(&storageHandle->fileSystem.fileHandle,
                    fileName,
                    FILE_OPEN_READ
                   );
  if (error != ERROR_NONE)
  {
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(&storageHandle->master,sizeof(storageHandle->master));

  return ERROR_NONE;
}

LOCAL void StorageMaster_close(StorageHandle *storageHandle)
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->master);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);

fprintf(stderr,"%s, %d: StorageMaster_close\n",__FILE__,__LINE__);
  DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->master,sizeof(storageHandle->master));

fprintf(stderr,"%s, %d: StorageMaster_create\n",__FILE__,__LINE__);
  error = ServerIO_executeCommand(storageHandle->storageInfo->master.io,
                                  30LL*MS_PER_SECOND,
                                  NULL,  // resultMap
                                  "STORAGE_CLOSE"
                                 );
  if (error != ERROR_NONE)
  {
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

  if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
  {
    return File_eof(&storageHandle->fileSystem.fileHandle);
  }
  else
  {
    return TRUE;
  }
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

  error = ERROR_NONE;
  if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
  {
    error = File_read(&storageHandle->fileSystem.fileHandle,buffer,bufferSize,bytesRead);
  }

  return error;
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
    // encode data
    n = MIN(bufferLength,4096);
    Misc_base64Encode(encodedData,p,n);

    // send data
  fprintf(stderr,"%s, %d: xxxxx\n",__FILE__,__LINE__);
    error = ServerIO_executeCommand(storageHandle->storageInfo->master.io,
                                    30LL*MS_PER_SECOND,
                                    NULL,  // resultMap
                                    "STORAGE_WRITE offset=%llu data=%s",
                                    123LL+(uint64)writtenBytes,
//TODO
//                                    String_cString(encodedData)
"XXXX"
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

  (*offset) = 0LL;

  error = ERROR_NONE;
  if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
  {
    error = File_tell(&storageHandle->fileSystem.fileHandle,offset);
  }

  return error;
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

  error = ERROR_NONE;
  if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
  {
    error = File_seek(&storageHandle->fileSystem.fileHandle,offset);
  }

  return error;
}

LOCAL uint64 StorageMaster_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->master);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_MASTER);

  size = 0LL;
  if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
  {
    size = File_getSize(&storageHandle->fileSystem.fileHandle);
  }

  return size;
}

LOCAL Errors StorageMaster_delete(StorageInfo *storageInfo,
                                  ConstString fileName
                                 )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_MASTER);
  assert(!String_isEmpty(fileName));

  error = ERROR_NONE;
  if ((storageInfo->jobOptions == NULL) || !storageInfo->jobOptions->dryRunFlag)
  {
    error = File_delete(fileName,FALSE);
  }

  return error;
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

  // open directory
  error = File_openDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle,
                                 pathName
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

LOCAL void StorageMaster_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_MASTER);

  File_closeDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle);
}

LOCAL bool StorageMaster_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_MASTER);

  return File_endOfDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle);
}

LOCAL Errors StorageMaster_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                             String                     fileName,
                                             FileInfo                   *fileInfo
                                            )
{
  Errors error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_MASTER);

  error = File_readDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle,fileName);
  if (error == ERROR_NONE)
  {
    if (fileInfo != NULL)
    {
      (void)File_getFileInfo(fileName,fileInfo);
    }
  }

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
