/***********************************************************************\
*
* $Revision: 4036 $
* $Date: 2015-05-30 01:48:57 +0200 (Sat, 30 May 2015) $
* $Author: torsten $
* Contents: storage file functions
* Systems: all
*
\***********************************************************************/

#define __STORAGE_IMPLEMENATION__

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
#include "crypt.h"
#include "passwords.h"
#include "misc.h"
#include "archive.h"
#include "bar_global.h"
#include "bar.h"

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

LOCAL Errors StorageFile_initAll(void)
{
  return ERROR_NONE;
}

LOCAL void StorageFile_doneAll(void)
{
}

LOCAL bool StorageFile_equalSpecifiers(const StorageSpecifier *storageSpecifier1,
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

LOCAL String StorageFile_getName(StorageSpecifier *storageSpecifier,
                                 ConstString      archiveName
                                )
{
  ConstString storageFileName;

  assert(storageSpecifier != NULL);

  // get file to use
  if      (archiveName != NULL)
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

LOCAL ConstString StorageFile_getPrintableName(StorageSpecifier *storageSpecifier,
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
  else if (!String_isEmpty(storageSpecifier->archivePatternString))
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

LOCAL Errors StorageFile_init(Storage                *storage,
                              const StorageSpecifier *storageSpecifier,
                              const JobOptions       *jobOptions
                             )
{
  assert(storage != NULL);
  assert(storageSpecifier != NULL);

  UNUSED_VARIABLE(storage);
  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(jobOptions);

  return ERROR_NONE;
}

LOCAL Errors StorageFile_done(Storage *storage)
{
  assert(storage != NULL);
  assert(storage->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  UNUSED_VARIABLE(storage);

  return ERROR_NONE;
}

LOCAL bool StorageFile_isServerAllocationPending(Storage *storage)
{
  assert(storage != NULL);
  assert(storage->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  UNUSED_VARIABLE(storage);

  return FALSE;
}

LOCAL Errors StorageFile_preProcess(Storage     *storage,
                                    ConstString archiveName,
                                    time_t      time,
                                    bool        initialFlag
                                   )
{
  TextMacro textMacros[2];
  String    script;
  Errors    error;

  assert(storage != NULL);
  assert(storage->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  error = ERROR_NONE;

  if ((storage->jobOptions == NULL) || !storage->jobOptions->dryRunFlag)
  {
    if (!initialFlag)
    {
      // init macros
      TEXT_MACRO_N_STRING (textMacros[0],"%file",  archiveName,                NULL);
      TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storage->volumeNumber,NULL);

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

LOCAL Errors StorageFile_postProcess(Storage     *storage,
                                     ConstString archiveName,
                                     time_t      time,
                                     bool        finalFlag
                                    )
{
  TextMacro textMacros[2];
  String    script;
  Errors    error;

  assert(storage != NULL);
  assert(storage->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  error = ERROR_NONE;

  if ((storage->jobOptions == NULL) || !storage->jobOptions->dryRunFlag)
  {
    if (!finalFlag)
    {
      // init macros
      TEXT_MACRO_N_STRING (textMacros[0],"%file",  archiveName,                NULL);
      TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storage->volumeNumber,NULL);

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

LOCAL bool StorageFile_exists(Storage *storage, ConstString archiveName)
{
  assert(storage != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storage);

  return File_exists(archiveName);
}

LOCAL Errors StorageFile_create(StorageHandle *storageHandle,
                                ConstString   archiveName,
                                uint64        archiveSize
                               )
{
  Errors error;
  String directoryName;

  assert(storageHandle != NULL);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(archiveSize);

  // check if archive file exists
  if (   (storageHandle->storage->jobOptions != NULL)
      && (storageHandle->storage->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_APPEND)
      && (storageHandle->storage->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_OVERWRITE)
      && !storageHandle->storage->jobOptions->archiveFileModeOverwriteFlag
      && File_exists(archiveName)
     )
  {
    return ERRORX_(FILE_EXISTS_,0,"%s",String_cString(archiveName));
  }

  if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
  {
    // create directory if not existing
    directoryName = File_getFilePathName(String_new(),archiveName);
    if (!String_isEmpty(directoryName) && !File_exists(directoryName))
    {
      error = File_makeDirectory(directoryName,
                                 FILE_DEFAULT_USER_ID,
                                 FILE_DEFAULT_GROUP_ID,
                                 FILE_DEFAULT_PERMISSION
                                );
      if (error != ERROR_NONE)
      {
        String_delete(directoryName);
        return error;
      }
    }
    String_delete(directoryName);

    // create/append file
    error = File_open(&storageHandle->fileSystem.fileHandle,
                      archiveName,
                      (   (storageHandle->storage->jobOptions->archiveFileMode == ARCHIVE_FILE_MODE_APPEND)
                       && !storageHandle->storage->jobOptions->archiveFileModeOverwriteFlag
                      )
                        ? FILE_OPEN_APPEND
                        : FILE_OPEN_CREATE
                     );
    if (error != ERROR_NONE)
    {
      return error;
    }

    DEBUG_ADD_RESOURCE_TRACE(&storageHandle->fileSystem,sizeof(storageHandle->fileSystem));
  }

  return ERROR_NONE;
}

LOCAL Errors StorageFile_open(StorageHandle *storageHandle,
                              ConstString  archiveName
                             )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(!String_isEmpty(archiveName));

  // init variables
  storageHandle->mode = STORAGE_MODE_READ;

  // check if file exists
  if (!File_exists(archiveName))
  {
    return ERRORX_(FILE_NOT_FOUND_,0,"%s",String_cString(archiveName));
  }

  // open file
  error = File_open(&storageHandle->fileSystem.fileHandle,
                    archiveName,
                    FILE_OPEN_READ
                   );
  if (error != ERROR_NONE)
  {
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(&storageHandle->fileSystem,sizeof(storageHandle->fileSystem));

  return ERROR_NONE;
}

LOCAL void StorageFile_close(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->fileSystem);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->fileSystem,sizeof(storageHandle->fileSystem));

  switch (storageHandle->mode)
  {
    case STORAGE_MODE_WRITE:
      if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
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

LOCAL bool StorageFile_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->fileSystem);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
  {
    return File_eof(&storageHandle->fileSystem.fileHandle);
  }
  else
  {
    return TRUE;
  }
}

LOCAL Errors StorageFile_read(StorageHandle *storageHandle,
                              void          *buffer,
                              ulong         size,
                              ulong         *bytesRead
                             )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->fileSystem);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(buffer != NULL);

  error = ERROR_NONE;
  if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
  {
    error = File_read(&storageHandle->fileSystem.fileHandle,buffer,size,bytesRead);
  }

  return error;
}

LOCAL Errors StorageFile_write(StorageHandle *storageHandle,
                               const void    *buffer,
                               ulong         size
                              )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->fileSystem);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(buffer != NULL);

  error = ERROR_NONE;
  if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
  {
    error = File_write(&storageHandle->fileSystem.fileHandle,buffer,size);
  }

  return error;
}

LOCAL Errors StorageFile_tell(StorageHandle *storageHandle,
                              uint64        *offset
                             )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->fileSystem);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
  {
    error = File_tell(&storageHandle->fileSystem.fileHandle,offset);
  }

  return error;
}

LOCAL Errors StorageFile_seek(StorageHandle *storageHandle,
                              uint64        offset
                             )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->fileSystem);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  error = ERROR_NONE;
  if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
  {
    error = File_seek(&storageHandle->fileSystem.fileHandle,offset);
  }

  return error;
}

LOCAL uint64 StorageFile_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->fileSystem);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  size = 0LL;
  if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
  {
    size = File_getSize(&storageHandle->fileSystem.fileHandle);
  }

  return size;
}

LOCAL Errors StorageFile_delete(Storage     *storage,
                                ConstString archiveName
                               )
{
  Errors error;

  assert(storage != NULL);
  assert(storage->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(!String_isEmpty(archiveName));

  error = ERROR_NONE;
  if ((storage->jobOptions == NULL) || !storage->jobOptions->dryRunFlag)
  {
    error = File_delete(archiveName,FALSE);
  }

  return error;
}

#if 0
still not complete
LOCAL Errors StorageFile_getFileInfo(Storage     *storage,
                                     ConstString fileName,
                                     FileInfo    *fileInfo
                                    )
{
  String infoFileName;
  Errors error;

  assert(storage != NULL);
  assert(fileInfo != NULL);
  assert(storage->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  error = File_getFileInfo(infoFileName,fileInfo);

  return error;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageFile_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                           const StorageSpecifier     *storageSpecifier,
                                           ConstString                archiveName,
                                           const JobOptions           *jobOptions,
                                           ServerConnectionPriorities serverConnectionPriority
                                          )
{
  Errors error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_FILESYSTEM);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(jobOptions);
  UNUSED_VARIABLE(serverConnectionPriority);

  // init variables
  storageDirectoryListHandle->type = STORAGE_TYPE_FILESYSTEM;

  // open directory
  error = File_openDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle,
                                 archiveName
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

LOCAL void StorageFile_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  File_closeDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle);
}

LOCAL bool StorageFile_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  return File_endOfDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle);
}

LOCAL Errors StorageFile_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
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
