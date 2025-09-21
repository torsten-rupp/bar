/***********************************************************************\
*
* Contents: Backup ARchiver file system functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_BLKID_BLKID_H
  #include <blkid/blkid.h>
#endif
#include <errno.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
  #include <linux/fs.h>
  #include <linux/magic.h>
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

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
  {"none",        FILE_SYSTEM_TYPE_NONE       },

  {"EXT",         FILE_SYSTEM_TYPE_EXT        },
  {"EXT2",        FILE_SYSTEM_TYPE_EXT2       },
  {"EXT3",        FILE_SYSTEM_TYPE_EXT3       },
  {"EXT4",        FILE_SYSTEM_TYPE_EXT4       },
  {"BTRFS",       FILE_SYSTEM_TYPE_BTRFS      },
  {"ISOFS",       FILE_SYSTEM_TYPE_ISOFS      },
  {"XFS",         FILE_SYSTEM_TYPE_XFS        },
  {"UDF",         FILE_SYSTEM_TYPE_UDF        },

  {"ReiserFS",    FILE_SYSTEM_TYPE_REISERFS   },
  {"ReiserFS 3.5",FILE_SYSTEM_TYPE_REISERFS3_5},
  {"ReiserFS 3.6",FILE_SYSTEM_TYPE_REISERFS3_6},
  {"ReiserFS 4",  FILE_SYSTEM_TYPE_REISERFS4  },

  {"Minix",       FILE_SYSTEM_TYPE_MINIX      },
  {"Minix 1",     FILE_SYSTEM_TYPE_MINIX1     },
  {"Minix 2",     FILE_SYSTEM_TYPE_MINIX2     },
  {"Minix 3",     FILE_SYSTEM_TYPE_MINIX3     },

  {"FAT",         FILE_SYSTEM_TYPE_FAT        },
  {"FAT12",       FILE_SYSTEM_TYPE_FAT12      },
  {"FAT16",       FILE_SYSTEM_TYPE_FAT16      },
  {"FAT32",       FILE_SYSTEM_TYPE_FAT32      },
  {"EXFAT",       FILE_SYSTEM_TYPE_EXFAT      },

  {"AFS",         FILE_SYSTEM_TYPE_AFS        },
  {"CODA",        FILE_SYSTEM_TYPE_CODA       },
  {"NFS",         FILE_SYSTEM_TYPE_NFS        },
  {"SMB1",        FILE_SYSTEM_TYPE_SMB1       },
  {"SMB2",        FILE_SYSTEM_TYPE_SMB2       },
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


const char *FileSystem_typeToString(FileSystemTypes fileSystemType, const char *defaultValue)
{
  size_t i = 0;
  while (   (i < SIZE_OF_ARRAY(FILESYTEM_TYPES))
         && (FILESYTEM_TYPES[i].fileSystemType != fileSystemType)
        )
  {
    i++;
  }
  const char *name;
  if (i < SIZE_OF_ARRAY(FILESYTEM_TYPES))
  {
    name = FILESYTEM_TYPES[i].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool FileSystem_parseType(const char *deviceName, FileSystemTypes *fileSystemType)
{
  assert(deviceName != NULL);
  assert(fileSystemType != NULL);

  size_t i = 0;
  while (   (i < SIZE_OF_ARRAY(FILESYTEM_TYPES))
         && !stringEqualsIgnoreCase(FILESYTEM_TYPES[i].name,deviceName)
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

FileSystemTypes FileSystem_getType(const char *deviceName)
{
  assert(deviceName != NULL);

  FileSystemTypes fileSystemType = FILE_SYSTEM_TYPE_NONE;

// TODO: use detector from init if possible, raiser version is not detected by libblkid
  #if defined(HAVE_BLKID_NEW_PROBE_FROM_FILENAME) && defined(HAVE_BLKID_DO_PROBE) && defined(HAVE_BLKID_PROBE_LOOKUP_VALUE)
    blkid_probe blkidProbe = blkid_new_probe_from_filename(deviceName);
    if (blkidProbe != NULL)
    {
      if (blkid_do_probe(blkidProbe) == 0)
      {
        const char *type;
        if (blkid_probe_lookup_value(blkidProbe, "TYPE", &type, NULL) == 0)
        {
//          int version;
//          blkid_probe_lookup_value(blkidProbe, "VERSION", &version, NULL);
//fprintf(stderr,"%s:%d: deviceName=%s type=%s\n",__FILE__,__LINE__,deviceName,type);
          const struct
          {
            const char      *name;
            FileSystemTypes fileSystemType;
          }
          FILESYTEM_TYPES[] =
          {
            { "ext2",    FILE_SYSTEM_TYPE_EXT2     },
            { "ext3",    FILE_SYSTEM_TYPE_EXT3     },
            { "ext4",    FILE_SYSTEM_TYPE_EXT4     },
            { "btrfs",   FILE_SYSTEM_TYPE_BTRFS    },
            { "hpfs",    FILE_SYSTEM_TYPE_HPFS     },
            { "isofs",   FILE_SYSTEM_TYPE_ISOFS    },
            { "xfs",     FILE_SYSTEM_TYPE_XFS      },
            { "udf",     FILE_SYSTEM_TYPE_UDF      },

            { "raiser",  FILE_SYSTEM_TYPE_REISERFS },

            { "minix",   FILE_SYSTEM_TYPE_MINIX    },

            { "fat",     FILE_SYSTEM_TYPE_FAT      },
            { "fat12",   FILE_SYSTEM_TYPE_FAT12    },
            { "fat16",   FILE_SYSTEM_TYPE_FAT16    },
            { "fat32",   FILE_SYSTEM_TYPE_FAT32    },
            { "vfat",    FILE_SYSTEM_TYPE_FAT      },
            { "exfat",   FILE_SYSTEM_TYPE_EXFAT    },

            { "afs",     FILE_SYSTEM_TYPE_AFS      },
            { "coda",    FILE_SYSTEM_TYPE_CODA     },
            { "nfs",     FILE_SYSTEM_TYPE_NFS      },
            { "smb1",    FILE_SYSTEM_TYPE_SMB1     },
            { "smb2",    FILE_SYSTEM_TYPE_SMB2     },
          };

          size_t i = ARRAY_FIND(FILESYTEM_TYPES,
                                SIZE_OF_ARRAY(FILESYTEM_TYPES),
                                i,
                                stringEqualsIgnoreCase(FILESYTEM_TYPES[i].name,type)
                               );
          if (i < SIZE_OF_ARRAY(FILESYTEM_TYPES)) fileSystemType = FILESYTEM_TYPES[i].fileSystemType;
        }
      }
      blkid_free_probe(blkidProbe);
    }
  #else
    UNUSED_VARIABLE(deviceName);
  #endif

  return fileSystemType;
}

Errors FileSystem_init(FileSystemHandle *fileSystemHandle,
                       DeviceHandle     *deviceHandle
                      )
{
  assert(fileSystemHandle != NULL);
  assert(deviceHandle != NULL);

  // initialize variables
  fileSystemHandle->deviceHandle        = deviceHandle;
  fileSystemHandle->type                = FILE_SYSTEM_TYPE_UNKNOWN;
  fileSystemHandle->handle              = NULL;
  fileSystemHandle->doneFunction        = NULL;
  fileSystemHandle->blockIsUsedFunction = NULL;

  // detect file system on device
  size_t i = 0;
  while ((i < SIZE_OF_ARRAY(FILE_SYSTEMS)) && (fileSystemHandle->type == FILE_SYSTEM_TYPE_UNKNOWN))
  {
    void *handle = malloc(FILE_SYSTEMS[i].sizeOfHandle);
    if (handle == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    fileSystemHandle->type = FILE_SYSTEMS[i].initFunction(deviceHandle,handle);
    if (fileSystemHandle->type != FILE_SYSTEM_TYPE_UNKNOWN)
    {
      fileSystemHandle->handle              = handle;
      fileSystemHandle->doneFunction        = FILE_SYSTEMS[i].doneFunction;
      fileSystemHandle->blockIsUsedFunction = FILE_SYSTEMS[i].blockIsUsedFunction;
    }
    else
    {
      free(handle);
    }
    i++;
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
