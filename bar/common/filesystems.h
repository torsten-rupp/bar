/***********************************************************************\
*
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

// file system types
typedef enum
{
  FILE_SYSTEM_TYPE_NONE,

  FILE_SYSTEM_TYPE_EXT,
  FILE_SYSTEM_TYPE_EXT2,
  FILE_SYSTEM_TYPE_EXT3,
  FILE_SYSTEM_TYPE_EXT4,
  FILE_SYSTEM_TYPE_BTRFS,
  FILE_SYSTEM_TYPE_HPFS,
  FILE_SYSTEM_TYPE_ISOFS,
  FILE_SYSTEM_TYPE_XFS,
  FILE_SYSTEM_TYPE_UDF,

  FILE_SYSTEM_TYPE_REISERFS,
  FILE_SYSTEM_TYPE_REISERFS3_5,
  FILE_SYSTEM_TYPE_REISERFS3_6,
  FILE_SYSTEM_TYPE_REISERFS4,

  FILE_SYSTEM_TYPE_MINIX,
  FILE_SYSTEM_TYPE_MINIX1,
  FILE_SYSTEM_TYPE_MINIX2,
  FILE_SYSTEM_TYPE_MINIX3,

  FILE_SYSTEM_TYPE_FAT,
  FILE_SYSTEM_TYPE_FAT12,
  FILE_SYSTEM_TYPE_FAT16,
  FILE_SYSTEM_TYPE_FAT32,
  FILE_SYSTEM_TYPE_EXFAT,

  FILE_SYSTEM_TYPE_AFS,
  FILE_SYSTEM_TYPE_CODA,
  FILE_SYSTEM_TYPE_NFS,
  FILE_SYSTEM_TYPE_SMB1,
  FILE_SYSTEM_TYPE_SMB2,

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
* Name   : FileSystem_typeToString
* Purpose: get name of file system
* Input  : fileSystemType - file system type
*          defaultValue   - default value for not supported file system
*                           types
* Output : -
* Return : file system name
* Notes  : -
\***********************************************************************/

const char *FileSystem_typeToString(FileSystemTypes fileSystemType, const char *defaultValue);

/***********************************************************************\
* Name   : FileSystem_parseType
* Purpose: parse file system type
* Input  : deviceName - device name
* Output : fileSystemType - file system type
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool FileSystem_parseType(const char *deviceName, FileSystemTypes *fileSystemType);

/***********************************************************************\
* Name   : FileSystem_getType
* Purpose: get file system type
* Input  : deviceName - device name
* Output : -
* Return : file system type
* Notes  : -
\***********************************************************************/

FileSystemTypes FileSystem_getType(const char *deviceName);

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
