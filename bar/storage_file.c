/***********************************************************************\
*
* Contents: storage file functions
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
#include "common/passwords.h"
#include "common/misc.h"

#include "bar.h"
#include "bar_common.h"
#include "errors.h"
#include "crypt.h"
#include "archive.h"

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

LOCAL String StorageFile_getName(String                 string,
                                 const StorageSpecifier *storageSpecifier,
                                 ConstString            archiveName
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

  if (!String_isEmpty(storageFileName))
  {
    String_append(string,storageFileName);
  }

  return string;
}

/***********************************************************************\
* Name   : StorageFile_getPrintableName
* Purpose: get printable storage name (without password)
* Input  : string           - name variable (can be NULL)
*          storageSpecifier - storage specifier string
*          archiveName      - archive name (can be NULL)
* Output : -
* Return : printable storage name
* Notes  : if archiveName is NULL file name from storageSpecifier is used
\***********************************************************************/

LOCAL void StorageFile_getPrintableName(String                 string,
                                        const StorageSpecifier *storageSpecifier,
                                        ConstString            archiveName
                                       )
{
  ConstString storageFileName;

  assert(string != NULL);
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
    String_append(string,storageFileName);
  }
}

/***********************************************************************\
* Name   : StorageFile_init
* Purpose: init new storage
* Input  : storageInfo                     - storage info variable
*          jobOptions                      - job options or NULL
*          maxBandWidthList                - list with max. band width
*                                            to use [bits/s] or NULL
*          serverConnectionPriority        - server connection priority
* Output : storageInfo - initialized storage info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageFile_init(StorageInfo      *storageInfo,
                              const JobOptions *jobOptions
                             )
{
  assert(storageInfo != NULL);

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(jobOptions);

  return ERROR_NONE;
}

LOCAL Errors StorageFile_done(StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  UNUSED_VARIABLE(storageInfo);

  return ERROR_NONE;
}

LOCAL bool StorageFile_isServerAllocationPending(const StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  UNUSED_VARIABLE(storageInfo);

  return FALSE;
}

LOCAL Errors StorageFile_preProcess(const StorageInfo *storageInfo,
                                    ConstString       archiveName,
                                    time_t            time,
                                    bool              initialFlag
                                   )
{
  TextMacros (textMacros,3);
  Errors     error;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  error = ERROR_NONE;

  if (!initialFlag)
  {
    // init variables
    String directory = String_new();

    // init macros
    TEXT_MACROS_INIT(textMacros)
    {
      TEXT_MACRO_X_STRING("%directory",File_getDirectoryName(directory,archiveName),NULL);
      TEXT_MACRO_X_STRING("%file",     archiveName,                                 NULL);
      TEXT_MACRO_X_INT   ("%number",   storageInfo->volumeNumber,                   NULL);
    }

    if (!String_isEmpty(globalOptions.file.writePreProcessCommand))
    {
      printInfo(1,"Write pre-processing...");
      error = executeTemplate(String_cString(globalOptions.file.writePreProcessCommand),
                              time,
                              textMacros.data,
                              textMacros.count,
                              CALLBACK_(executeIOOutput,NULL)
                             );
      printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
    }

    // free resources
    String_delete(directory);
  }

  return error;
}

LOCAL Errors StorageFile_postProcess(const StorageInfo *storageInfo,
                                     ConstString       archiveName,
                                     time_t            time,
                                     bool              finalFlag
                                    )
{
  TextMacros (textMacros,3);
  Errors     error;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  error = ERROR_NONE;

  if (!finalFlag)
  {
    // init variables
    String directory = String_new();

    // init macros
    TEXT_MACROS_INIT(textMacros)
    {
      TEXT_MACRO_X_STRING("%directory",File_getDirectoryName(directory,archiveName),NULL);
      TEXT_MACRO_X_STRING("%file",     archiveName,                                 NULL);
      TEXT_MACRO_X_INT   ("%number",   storageInfo->volumeNumber,                   NULL);
    }

    if (!String_isEmpty(globalOptions.file.writePostProcessCommand))
    {
      printInfo(1,"Write post-processing...");
      error = executeTemplate(String_cString(globalOptions.file.writePostProcessCommand),
                              time,
                              textMacros.data,
                              textMacros.count,
                              CALLBACK_(executeIOOutput,NULL)
                             );
      printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
    }

    // free resources
    String_delete(directory);
  }

  return error;
}

LOCAL bool StorageFile_exists(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);

  return File_exists(archiveName);
}

LOCAL bool StorageFile_isFile(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);

  return File_isFile(archiveName);
}

LOCAL bool StorageFile_isDirectory(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);

  return File_isDirectory(archiveName);
}

LOCAL bool StorageFile_isReadable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);

  return File_isReadable(archiveName);
}

LOCAL bool StorageFile_isWritable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);

  return File_isWritable(archiveName);
}

LOCAL Errors StorageFile_getTmpName(String archiveName, const StorageInfo *storageInfo)
{
  String directoryPath,baseName;
  Errors result;

  assert(archiveName != NULL);
  assert(!String_isEmpty(archiveName));
  assert(storageInfo != NULL);

  UNUSED_VARIABLE(storageInfo);

  directoryPath = String_new();
  baseName      = String_new();
  File_splitFileName(archiveName,directoryPath,baseName,NULL);
  result = File_getTmpFileName(archiveName,
                               String_cString(baseName),
                               !String_isEmpty(directoryPath) ? directoryPath : tmpDirectory
                              );
  String_delete(baseName);
  String_delete(directoryPath);

  return result;
}

LOCAL Errors StorageFile_create(StorageHandle *storageHandle,
                                ConstString   fileName,
                                uint64        fileSize,
                                bool          forceFlag
                               )
{
  Errors error;
  String directoryName;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(!String_isEmpty(fileName));

  UNUSED_VARIABLE(fileSize);

  // check if file exists
  if (   !forceFlag
      && (storageHandle->storageInfo->jobOptions != NULL)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_APPEND)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_OVERWRITE)
      && StorageFile_exists(storageHandle->storageInfo,fileName)
     )
  {
    return ERRORX_(FILE_EXISTS_,0,"%s",String_cString(fileName));
  }

  // create directory if not existing
  directoryName = File_getDirectoryName(String_new(),fileName);
  if (!String_isEmpty(directoryName))
  {
    if (File_exists(directoryName))
    {
      // check if directory
      if (!File_isDirectory(directoryName))
      {
        error = ERRORX_(NOT_A_DIRECTORY,0,"%s",String_cString(directoryName));
        String_delete(directoryName);
        return error;
      }
    }
    else
    {
      // create directory
      error = File_makeDirectory(directoryName,
                                 FILE_DEFAULT_USER_ID,
                                 FILE_DEFAULT_GROUP_ID,
                                 FILE_DEFAULT_PERMISSIONS,
                                 FALSE
                                );
      if (error != ERROR_NONE)
      {
        String_delete(directoryName);
        return error;
      }
    }
  }
  String_delete(directoryName);

  // create/append file
  error = File_open(&storageHandle->fileSystem.fileHandle,
                    fileName,
                    (storageHandle->storageInfo->jobOptions->archiveFileMode == ARCHIVE_FILE_MODE_APPEND)
                      ? FILE_OPEN_APPEND
                      : FILE_OPEN_CREATE
                   );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

LOCAL Errors StorageFile_open(StorageHandle *storageHandle,
                              ConstString   fileName
                             )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(!String_isEmpty(fileName));

  // check if file exists
  if (!File_exists(fileName))
  {
    return ERRORX_(FILE_NOT_FOUND_,0,"%s",String_cString(fileName));
  }

  // init variables
  storageHandle->mode = STORAGE_MODE_READ;

  // open file
  error = File_open(&storageHandle->fileSystem.fileHandle,
                    fileName,
                    FILE_OPEN_READ
                   );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

LOCAL void StorageFile_close(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  switch (storageHandle->mode)
  {
    case STORAGE_MODE_WRITE:
      File_close(&storageHandle->fileSystem.fileHandle);
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
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  return File_eof(&storageHandle->fileSystem.fileHandle);
}

LOCAL Errors StorageFile_read(StorageHandle *storageHandle,
                              void          *buffer,
                              ulong         bufferSize,
                              ulong         *bytesRead
                             )
{
  assert(storageHandle != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(buffer != NULL);

  return File_read(&storageHandle->fileSystem.fileHandle,buffer,bufferSize,bytesRead);
}

LOCAL Errors StorageFile_write(StorageHandle *storageHandle,
                               const void    *buffer,
                               ulong         bufferLength
                              )
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(buffer != NULL);

  return File_write(&storageHandle->fileSystem.fileHandle,buffer,bufferLength);
}

LOCAL Errors StorageFile_tell(StorageHandle *storageHandle,
                              uint64        *offset
                             )
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(offset != NULL);

  (*offset) = 0LL;

  return File_tell(&storageHandle->fileSystem.fileHandle,offset);
}

LOCAL Errors StorageFile_seek(StorageHandle *storageHandle,
                              uint64        offset
                             )
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  return File_seek(&storageHandle->fileSystem.fileHandle,offset);
}

LOCAL uint64 StorageFile_getSize(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  return File_getSize(&storageHandle->fileSystem.fileHandle);
}

LOCAL Errors StorageFile_rename(const StorageInfo *storageInfo,
                                ConstString       oldArchiveName,
                                ConstString       newArchiveName
                               )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  UNUSED_VARIABLE(storageInfo);

  error = File_rename(oldArchiveName,newArchiveName,NULL);

  return error;
}

LOCAL Errors StorageFile_makeDirectory(const StorageInfo *storageInfo,
                                       ConstString       directoryName
                                      )
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(!String_isEmpty(directoryName));

  UNUSED_VARIABLE(storageInfo);

  return File_makeDirectory(directoryName,
                            FILE_DEFAULT_USER_ID,
                            FILE_DEFAULT_GROUP_ID,
                            FILE_DEFAULT_PERMISSIONS,
                            FALSE
                           );
}

LOCAL Errors StorageFile_delete(const StorageInfo *storageInfo,
                                ConstString       fileName
                               )
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(!String_isEmpty(fileName));

  UNUSED_VARIABLE(storageInfo);

  return File_delete(fileName,FALSE);
}

#if 0
still not complete
LOCAL Errors StorageFile_getInfo(const StorageInfo *storageInfo,
                                 ConstString       fileName,
                                 FileInfo          *fileInfo
                                )
{
  String infoFileName;
  Errors error;

  assert(storageInfo != NULL);
  assert(fileInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  error = File_getInfo(fileInfo,infoFileName);

  return error;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageFile_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
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
  assert(pathName != NULL);

  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(jobOptions);
  UNUSED_VARIABLE(serverConnectionPriority);

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
      (void)File_getInfo(fileInfo,fileName);
    }
  }

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
