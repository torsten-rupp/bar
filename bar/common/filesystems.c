/***********************************************************************\
*
* Contents: Backup ARchiver file system functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "common/global.h"
#include "common/strings.h"
#include "common/devices.h"

#include "errors.h"

#include "filesystems.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// file system types
LOCAL const struct
{
  const char      *name;
  FileSystemTypes fileSystemType;
}
FILESYTEM_TYPES[] =
{
  {"none",       FILE_SYSTEM_TYPE_NONE     },

  {"Ext2",       FILE_SYSTEM_TYPE_EXT2     },
  {"Ext3",       FILE_SYSTEM_TYPE_EXT3     },
  {"Ext4",       FILE_SYSTEM_TYPE_EXT4     },
  {"FAT12",      FILE_SYSTEM_TYPE_FAT12    },
  {"FAT16",      FILE_SYSTEM_TYPE_FAT16    },
  {"FAT32",      FILE_SYSTEM_TYPE_FAT32    },
  {"ReiserFS 1", FILE_SYSTEM_TYPE_REISERFS1},
  {"ReiserFS 3", FILE_SYSTEM_TYPE_REISERFS3},
  {"ReiserFS 4", FILE_SYSTEM_TYPE_REISERFS4},
};


/***************************** Datatypes *******************************/
// file system definition
typedef struct
{
  uint                          sizeOfHandle;
  FileSystemInitFunction        initFunction;
  FileSystemDoneFunction        doneFunction;
  FileSystemBlockIsUsedFunction blockIsUsedFunction;
} FileSystem;

/***************************** Variables *******************************/

// define file system
#define DEFINE_FILE_SYSTEM(name) \
  { \
    sizeof(name ## Handle), \
    (FileSystemInitFunction)name ## _init, \
    (FileSystemDoneFunction)name ## _done, \
    (FileSystemBlockIsUsedFunction)name ## _blockIsUsed, \
  }

/****************************** Macros *********************************/

// convert from little endian to host system format
#if __BYTE_ORDER == __LITTLE_ENDIAN
  #define LE16_TO_HOST(x) (x)
  #define LE32_TO_HOST(x) (x)
#else /* not __BYTE_ORDER == __LITTLE_ENDIAN */
  #define LE16_TO_HOST(x) swap16(x)
  #define LE32_TO_HOST(x) swap32(x)
#endif /* __BYTE_ORDER == __LITTLE_ENDIAN */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
#else /* not __BYTE_ORDER == __LITTLE_ENDIAN */
/***********************************************************************\
* Name   : swap16
* Purpose: swap 16bit value
* Input  : n - value
* Output : -
* Return : swapped value
* Notes  : -
\***********************************************************************/

LOCAL_INLINE uint16 swap16(uint16 n)
{
  return   ((n & 0xFF00U >> 8) << 0)
         | ((n & 0x00FFU >> 0) << 8)
         ;
}

/***********************************************************************\
* Name   : swap32
* Purpose: swap 32bit value
* Input  : n - value
* Output : -
* Return : swapped value
* Notes  : -
\***********************************************************************/

LOCAL_INLINE uint32 swap32(uint32 n)
{
  return   ((n & 0xFF000000U >> 24) <<  0)
         | ((n & 0x00FF0000U >> 16) <<  8)
         | ((n & 0x0000FF00U >>  8) << 16)
         | ((n & 0x000000FFU >>  0) << 24)
         ;
}
#endif /* __BYTE_ORDER == __LITTLE_ENDIAN */

#include "filesystems_ext.c"
#include "filesystems_fat.c"
#include "filesystems_reiserfs.c"

// support file systems
LOCAL FileSystem FILE_SYSTEMS[] =
{
  DEFINE_FILE_SYSTEM(EXT),
  DEFINE_FILE_SYSTEM(FAT),
  DEFINE_FILE_SYSTEM(REISERFS),
};

/*---------------------------------------------------------------------*/

Errors FileSystem_init(FileSystemHandle *fileSystemHandle,
                       DeviceHandle     *deviceHandle
                      )
{
  void *handle;
  uint z;

  assert(fileSystemHandle != NULL);
  assert(deviceHandle != NULL);

  // initialize variables
  fileSystemHandle->deviceHandle        = deviceHandle;
  fileSystemHandle->type                = FILE_SYSTEM_TYPE_UNKNOWN;
  fileSystemHandle->handle              = NULL;
  fileSystemHandle->doneFunction        = NULL;
  fileSystemHandle->blockIsUsedFunction = NULL;

  // detect file system on device
  z = 0;
  while ((z < SIZE_OF_ARRAY(FILE_SYSTEMS)) && (fileSystemHandle->type == FILE_SYSTEM_TYPE_UNKNOWN))
  {
    handle = malloc(FILE_SYSTEMS[z].sizeOfHandle);
    if (handle == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    fileSystemHandle->type = FILE_SYSTEMS[z].initFunction(deviceHandle,handle);
    if (fileSystemHandle->type != FILE_SYSTEM_TYPE_UNKNOWN)
    {
      fileSystemHandle->handle              = handle;
      fileSystemHandle->doneFunction        = FILE_SYSTEMS[z].doneFunction;
      fileSystemHandle->blockIsUsedFunction = FILE_SYSTEMS[z].blockIsUsedFunction;
    }
    else
    {
      free(handle);
    }
    z++;
  }

  return ERROR_NONE;
}

Errors FileSystem_done(FileSystemHandle *fileSystemHandle)
{
  assert(fileSystemHandle != NULL);

  if (fileSystemHandle->doneFunction != NULL)
  {
    fileSystemHandle->doneFunction(fileSystemHandle->deviceHandle,fileSystemHandle->handle);
  }
  if (fileSystemHandle->handle != NULL)
  {
    free(fileSystemHandle->handle);
  }

  return ERROR_NONE;
}

const char *FileSystem_fileSystemTypeToString(FileSystemTypes fileSystemType, const char *defaultValue)
{
  return (   (ARRAY_FIRST(FILESYTEM_TYPES).fileSystemType <= fileSystemType)
          && (fileSystemType <= ARRAY_LAST(FILESYTEM_TYPES).fileSystemType)
         )
           ? FILESYTEM_TYPES[fileSystemType-ARRAY_FIRST(FILESYTEM_TYPES).fileSystemType].name
           : defaultValue;
}

bool FileSystem_parseFileSystemType(const char *name, FileSystemTypes *fileSystemType)
{
  uint i;

  assert(name != NULL);
  assert(fileSystemType != NULL);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(FILESYTEM_TYPES))
         && !stringEqualsIgnoreCase(FILESYTEM_TYPES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(FILESYTEM_TYPES))
  {
    (*fileSystemType) = FILESYTEM_TYPES[i].fileSystemType;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool FileSystem_blockIsUsed(FileSystemHandle *fileSystemHandle, uint64 offset)
{
  assert(fileSystemHandle != NULL);

  if (fileSystemHandle->blockIsUsedFunction != NULL)
  {
    return fileSystemHandle->blockIsUsedFunction(fileSystemHandle->deviceHandle,fileSystemHandle->handle,offset);
  }
  else
  {
    return TRUE;
  }
}

#ifdef __cplusplus
  }
#endif

/* end of file */
