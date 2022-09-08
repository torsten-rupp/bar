/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver file system functions
* Systems: all
*
\***********************************************************************/

#ifndef __FILESYSTEMS__
#define __FILESYSTEMS__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"
#include "strings.h"
#include "devices.h"
#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// supported file systems
typedef enum
{
  FILE_SYSTEM_TYPE_NONE,

  FILE_SYSTEM_TYPE_EXT2,
  FILE_SYSTEM_TYPE_EXT3,
  FILE_SYSTEM_TYPE_EXT4,
  FILE_SYSTEM_TYPE_FAT12,
  FILE_SYSTEM_TYPE_FAT16,
  FILE_SYSTEM_TYPE_FAT32,
  FILE_SYSTEM_TYPE_REISERFS1,
  FILE_SYSTEM_TYPE_REISERFS3,
  FILE_SYSTEM_TYPE_REISERFS4,

  FILE_SYSTEM_TYPE_UNKNOWN,
} FileSystemTypes;

/***************************** Datatypes *******************************/

// file system functions
typedef FileSystemTypes(*FileSystemInitFunction)(DeviceHandle *deviceHandle, void *handle);
typedef void(*FileSystemDoneFunction)(DeviceHandle *deviceHandle, void *handle);
typedef bool(*FileSystemBlockIsUsedFunction)(DeviceHandle *deviceHandle, void *handle, uint64 offset);

// file system handle
typedef struct
{
  DeviceHandle                  *deviceHandle;
  FileSystemTypes               type;
  void                          *handle;
  FileSystemDoneFunction        doneFunction;
  FileSystemBlockIsUsedFunction blockIsUsedFunction;
} FileSystemHandle;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : FileSystem_init
* Purpose: init file system
* Input  : fileSystemHandle - file system handle
*          deviceHandle     - device handle
* Output : fileSystemHandle - file system handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors FileSystem_init(FileSystemHandle *fileSystemHandle,
                       DeviceHandle     *deviceHandle
                      );

/***********************************************************************\
* Name   : FileSystem_close
* Purpose: close file system
* Input  : fileSystemHandle - file system handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors FileSystem_done(FileSystemHandle *fileSystemHandle);

/***********************************************************************\
* Name   : FileSystem_fileSystemTypeToString
* Purpose: get name of file system
* Input  : fileSystemType - file system type
*          defaultValue   - default value for not supported file system
*                           types
* Output : -
* Return : file system name
* Notes  : -
\***********************************************************************/

const char *FileSystem_fileSystemTypeToString(FileSystemTypes fileSystemType, const char *defaultValue);

/***********************************************************************\
* Name   : FileSystem_parseFileSystemType
* Purpose: parse file system type
* Input  : name - name of archive type
* Output : archiveType - archive type
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool FileSystem_parseFileSystemType(const char *name, FileSystemTypes *fileSystemType);

/***********************************************************************\
* Name   : FileSystem_blockIsUsed
* Purpose: check if block is used by file system
* Input  : fileSystemHandle - file system handle
*          offset           - offset (byte position) (0..n-1)
* Output : -
* Return : TRUE if block is used, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool FileSystem_blockIsUsed(FileSystemHandle *fileSystemHandle,
                            uint64           offset
                           );

#ifdef __cplusplus
  }
#endif

#endif /* __FILESYSTEMS__ */

/* end of file */
