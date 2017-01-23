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
  assert(storageSpecifier1->type == STORAGE_TYPE_FILESYSTEM);
  assert(storageSpecifier2 != NULL);
  assert(storageSpecifier2->type == STORAGE_TYPE_FILESYSTEM);

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
  assert(storageSpecifier->type == STORAGE_TYPE_FILESYSTEM);

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
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  UNUSED_VARIABLE(storageInfo);

  return ERROR_NONE;
}

LOCAL bool StorageMaster_isServerAllocationPending(StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

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
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

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
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

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

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
//  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(!String_isEmpty(fileName));

  UNUSED_VARIABLE(fileSize);
fprintf(stderr,"%s, %d: StorageMaster_create\n",__FILE__,__LINE__);

  error = ServerIO_sendMaster(storageHandle->storageInfo->master.io,
                              &serverResultList,
                              "STORAGE_CREATE name=%S size=%llu",
                              fileName,
                              fileSize
                             );

  DEBUG_ADD_RESOURCE_TRACE(&storageHandle->fileSystem,sizeof(storageHandle->fileSystem));

  return ERROR_NONE;
}

LOCAL Errors StorageMaster_open(StorageHandle *storageHandle,
                                ConstString   fileName
                               )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(!String_isEmpty(fileName));

  // init variables
  storageHandle->mode = STORAGE_MODE_READ;

  // check if file exists
  if (!File_exists(fileName))
  {
    return ERRORX_(FILE_NOT_FOUND_,0,"%s",String_cString(fileName));
  }

  // open file
  error = File_open(&storageHandle->fileSystem.fileHandle,
                    fileName,
                    FILE_OPEN_READ
                   );
  if (error != ERROR_NONE)
  {
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(&storageHandle->fileSystem,sizeof(storageHandle->fileSystem));

  return ERROR_NONE;
}

LOCAL void StorageMaster_close(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->fileSystem);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->fileSystem,sizeof(storageHandle->fileSystem));

  switch (storageHandle->mode)
  {
    case STORAGE_MODE_WRITE:
      if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
      {
        File_close(&storageHandle->fileSystem.fileHandle);
      }
      break;
    case STORAGE_MODE_READ:
      File_close(&storageHandle->fileSystem.fileHandle);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
}

LOCAL bool StorageMaster_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->fileSystem);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

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
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->fileSystem);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
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
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->fileSystem);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(buffer != NULL);

  error = ERROR_NONE;
  if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
  {
    error = File_write(&storageHandle->fileSystem.fileHandle,buffer,bufferLength);
  }

  return error;
}

LOCAL Errors StorageMaster_tell(StorageHandle *storageHandle,
                                uint64        *offset
                               )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->fileSystem);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
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
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->fileSystem);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

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
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->fileSystem);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

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
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
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
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

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
  assert(storageSpecifier->type == STORAGE_TYPE_FILESYSTEM);
  assert(!String_isEmpty(pathName));

  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(jobOptions);
  UNUSED_VARIABLE(serverConnectionPriority);

  // init variables
  storageDirectoryListHandle->type = STORAGE_TYPE_FILESYSTEM;

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
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  File_closeDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle);
}

LOCAL bool StorageMaster_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  return File_endOfDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle);
}

LOCAL Errors StorageMaster_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                             String                     fileName,
                                             FileInfo                   *fileInfo
                                            )
{
  Errors error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

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
