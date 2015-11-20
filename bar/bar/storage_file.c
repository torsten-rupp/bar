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

LOCAL bool StorageFile_parseSpecifier(ConstString fileSpecifier,
                                      String      fileName
                                     )
{
  assert(fileSpecifier != NULL);
  assert(fileName != NULL);

  if (fileName != NULL) String_set(fileName,fileSpecifier);

  return TRUE;
}

LOCAL bool StorageFile_equalNames(const StorageSpecifier *storageSpecifier1,
                                  const StorageSpecifier *storageSpecifier2
                                 )
{
  assert(storageSpecifier1 != NULL);
  assert(storageSpecifier1->type == STORAGE_TYPE_FILESYSTEM);
  assert(storageSpecifier2 != NULL);
  assert(storageSpecifier2->type == STORAGE_TYPE_FILESYSTEM);

  return String_equals(storageSpecifier1->archiveName,storageSpecifier2->archiveName);
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

LOCAL Errors StorageFile_init(StorageHandle          *storageHandle,
                              const StorageSpecifier *storageSpecifier,
                              const JobOptions       *jobOptions
                             )
{
  assert(storageHandle != NULL);
  assert(storageSpecifier != NULL);

  UNUSED_VARIABLE(storageHandle);
  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(jobOptions);

  return ERROR_NONE;
}

LOCAL Errors StorageFile_done(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  UNUSED_VARIABLE(storageHandle);

  return ERROR_NONE;
}

LOCAL bool StorageFile_isServerAllocationPending(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  UNUSED_VARIABLE(storageHandle);

  return FALSE;
}

LOCAL Errors StorageFile_preProcess(StorageHandle *storageHandle,
                                    bool          initialFlag
                                   )
{
  TextMacro textMacros[2];
  Errors    error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  error = ERROR_NONE;

  if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
  {
    if (!initialFlag)
    {
      // init macros
      TEXT_MACRO_N_STRING (textMacros[0],"%file",  storageHandle->fileSystem.fileHandle.name);
      TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageHandle->volumeNumber              );

      if (globalOptions.file.writePreProcessCommand != NULL)
      {
        // write pre-processing
        if (error == ERROR_NONE)
        {
          printInfo(0,"Write pre-processing...");
          error = Misc_executeCommand(String_cString(globalOptions.file.writePreProcessCommand),
                                      textMacros,
                                      SIZE_OF_ARRAY(textMacros),
                                      CALLBACK(executeIOOutput,NULL),
                                      CALLBACK(executeIOOutput,NULL)
                                     );
          printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
        }
      }
    }
  }

  return error;
}

LOCAL Errors StorageFile_postProcess(StorageHandle *storageHandle,
                                     bool          finalFlag
                                    )
{
  TextMacro textMacros[2];
  Errors    error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  error = ERROR_NONE;

  if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
  {
    if (!finalFlag)
    {
      // init macros
      TEXT_MACRO_N_STRING (textMacros[0],"%file",  storageHandle->fileSystem.fileHandle.name);
      TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageHandle->volumeNumber              );

      if (globalOptions.file.writePostProcessCommand != NULL)
      {
        // write post-process
        if (error == ERROR_NONE)
        {
          printInfo(0,"Write post-processing...");
          error = Misc_executeCommand(String_cString(globalOptions.file.writePostProcessCommand),
                                      textMacros,
                                      SIZE_OF_ARRAY(textMacros),
                                      CALLBACK(executeIOOutput,NULL),
                                      CALLBACK(executeIOOutput,NULL)
                                     );
          printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
        }
      }
    }
  }

  return error;
}

LOCAL Errors StorageFile_create(StorageHandle *storageHandle,
                                ConstString   archiveName,
                                uint64        archiveSize
                               )
{
  Errors error;
  String directoryName;

  assert(storageHandle != NULL);
  assert(archiveName != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  UNUSED_VARIABLE(archiveSize);

  // check if archive file exists
  if (   (storageHandle->jobOptions != NULL)
      && (storageHandle->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_APPEND)
      && (storageHandle->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_OVERWRITE)
      && !storageHandle->jobOptions->archiveFileModeOverwriteFlag
      && File_exists(archiveName))
  {
    return ERRORX_(FILE_EXISTS_,0,String_cString(archiveName));
  }

  if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
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

    // open file
    error = File_open(&storageHandle->fileSystem.fileHandle,
                      archiveName,
                      (   (storageHandle->jobOptions->archiveFileMode == ARCHIVE_FILE_MODE_APPEND)
                       && !storageHandle->jobOptions->archiveFileModeOverwriteFlag
                      )
                        ? FILE_OPEN_APPEND
                        : FILE_OPEN_CREATE
                     );
    if (error != ERROR_NONE)
    {
      return error;
    }

    DEBUG_ADD_RESOURCE_TRACE("storage create file",&storageHandle->fileSystem);
  }

  return error;
}

LOCAL Errors StorageFile_open(StorageHandle *storageHandle, ConstString archiveName)
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  // init variables
  storageHandle->mode = STORAGE_MODE_READ;

  // get file name
  if (archiveName == NULL) archiveName = storageHandle->storageSpecifier.archiveName;
  if (String_isEmpty(archiveName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  // check if file exists
  if (!File_exists(archiveName))
  {
    return ERRORX_(FILE_NOT_FOUND_,0,String_cString(archiveName));
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

  DEBUG_ADD_RESOURCE_TRACE("storage open file",&storageHandle->fileSystem);

  return ERROR_NONE;
}

LOCAL void StorageFile_close(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->fileSystem);

  switch (storageHandle->mode)
  {
    case STORAGE_MODE_UNKNOWN:
      break;
    case STORAGE_MODE_WRITE:
      if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
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
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
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
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(buffer != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  error = ERROR_NONE;
  if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
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
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(buffer != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  error = ERROR_NONE;
  if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
  {
    error = File_write(&storageHandle->fileSystem.fileHandle,buffer,size);
  }

  return error;
}

LOCAL uint64 StorageFile_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  size = 0LL;
  if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
  {
    size = File_getSize(&storageHandle->fileSystem.fileHandle);
  }

  return size;
}

LOCAL Errors StorageFile_tell(StorageHandle *storageHandle,
                              uint64        *offset
                             )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(offset != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  (*offset) = 0LL;

  error = ERROR_NONE;
  if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
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
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  error = ERROR_NONE;
  if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
  {
    error = File_seek(&storageHandle->fileSystem.fileHandle,offset);
  }

  return error;
}

LOCAL Errors StorageFile_delete(StorageHandle *storageHandle,
                                ConstString   archiveName
                               )
{
  ConstString storageFileName;
  Errors      error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(archiveName != NULL);

  error = ERROR_NONE;
  if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
  {
    error = File_delete(archiveName,FALSE);
  }

  return error;
}

#if 0
still not complete
LOCAL Errors StorageFile_getFileInfo(StorageHandle *storageHandle,
                                     ConstString   fileName,
                                     FileInfo      *fileInfo
                                    )
{
  String infoFileName;
  Errors error;

  assert(storageHandle != NULL);
  assert(fileInfo != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  error = File_getFileInfo(infoFileName,fileInfo);

  return error;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageFile_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                           const StorageSpecifier     *storageSpecifier,
                                           const JobOptions           *jobOptions,
                                           ServerConnectionPriorities serverConnectionPriority,
                                           ConstString                archiveName
                                          )
{
  Errors error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_FILESYSTEM);
  assert(archiveName != NULL);

  UNUSED_VARIABLE(jobOptions);

  // initialize variables

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
