/***********************************************************************\
*
* Contents: file system functions
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
#include "common/filesystems_ext.h"
#include "common/filesystems_fat.h"
#include "common/filesystems_reiserfs.h"
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

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#include "filesystems_ext.c"
#include "filesystems_fat.c"
#include "filesystems_reiserfs.c"

/***********************************************************************\
* Name   : getFileSystemType
* Purpose: detect file system type
* Input  : deviceHandle - device handle
* Output : -
* Return : file system type or FILE_SYSTEM_TYPE_NONE
* Notes  : -
\***********************************************************************/

LOCAL FileSystemTypes getFileSystemType(DeviceHandle *deviceHandle)
{
  assert(deviceHandle != NULL);

  FileSystemTypes fileSystemType = FILE_SYSTEM_TYPE_NONE;

  // detect file system on device
  if      (EXT_init(deviceHandle,&fileSystemType,NULL))
  {
  }
  else if (FAT_init(deviceHandle,&fileSystemType,NULL))
  {
  }
  else if (ReiserFS_init(deviceHandle,&fileSystemType,NULL))
  {
  }
  else
  {
    // use libblkid to detect file system on device
    #if defined(HAVE_BLKID_NEW_PROBE_FROM_FILENAME) && defined(HAVE_BLKID_DO_PROBE) && defined(HAVE_BLKID_PROBE_LOOKUP_VALUE)
    //    blkid_probe blkidProbe = blkid_new_probe_from_filename(deviceName);
      blkid_probe blkidProbe = blkid_new_probe();
      if (blkidProbe != NULL)
      {
        if (blkid_probe_set_device(blkidProbe,deviceHandle->handle,0,0) == 0)
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
        }
        blkid_free_probe(blkidProbe);
      }
    #else
      UNUSED_VARIABLE(deviceHandle);
    #endif
  }

  return fileSystemType;
}

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

FileSystemTypes FileSystem_getType(ConstString deviceName)
{
  assert(deviceName != NULL);

  return FileSystem_getTypeCString(String_cString(deviceName));
}

FileSystemTypes FileSystem_getTypeCString(const char *deviceName)
{
  assert(deviceName != NULL);

  FileSystemTypes fileSystemType;

  DeviceHandle deviceHandle;
  if (Device_openCString(&deviceHandle,deviceName,DEVICE_OPEN_READ) == ERROR_NONE)
  {
    fileSystemType = getFileSystemType(&deviceHandle);
    Device_close(&deviceHandle);
  }
  else
  {
    fileSystemType = FILE_SYSTEM_TYPE_UNKNOWN;
  }

  return fileSystemType;
}

Errors FileSystem_init(FileSystemHandle *fileSystemHandle,
                       DeviceHandle     *deviceHandle
                      )
{
  assert(fileSystemHandle != NULL);
  assert(deviceHandle != NULL);

  // initialize variables
  fileSystemHandle->deviceHandle = deviceHandle;
  fileSystemHandle->type         = FILE_SYSTEM_TYPE_UNKNOWN;
  if      (EXT_init(deviceHandle,&fileSystemHandle->type,&fileSystemHandle->extHandle))
  {
  }
  else if (FAT_init(deviceHandle,&fileSystemHandle->type,&fileSystemHandle->fatHandle))
  {
  }
  else if (ReiserFS_init(deviceHandle,&fileSystemHandle->type,&fileSystemHandle->reiserFSHandle))
  {
  }
  else
  {
    fileSystemHandle->type = getFileSystemType(deviceHandle);
  }
  assert(fileSystemHandle->type != FILE_SYSTEM_TYPE_UNKNOWN);

  return ERROR_NONE;
}

Errors FileSystem_done(FileSystemHandle *fileSystemHandle)
{
  assert(fileSystemHandle != NULL);

  switch (fileSystemHandle->type)
  {
    case FILE_SYSTEM_TYPE_EXT2:
    case FILE_SYSTEM_TYPE_EXT3:
    case FILE_SYSTEM_TYPE_EXT4:
      EXT_done(&fileSystemHandle->extHandle);
      break;
    case FILE_SYSTEM_TYPE_FAT12:
    case FILE_SYSTEM_TYPE_FAT16:
    case FILE_SYSTEM_TYPE_FAT32:
      FAT_done(&fileSystemHandle->fatHandle);
      break;
    case FILE_SYSTEM_TYPE_REISERFS3_5:
    case FILE_SYSTEM_TYPE_REISERFS3_6:
      ReiserFS_done(&fileSystemHandle->reiserFSHandle);
      break;
    default:
      break;
  }

  return ERROR_NONE;
}

bool FileSystem_blockIsUsed(FileSystemHandle *fileSystemHandle, uint64 offset)
{
  assert(fileSystemHandle != NULL);

  bool blockIsUsed;
  switch (fileSystemHandle->type)
  {
    case FILE_SYSTEM_TYPE_EXT2:
    case FILE_SYSTEM_TYPE_EXT3:
    case FILE_SYSTEM_TYPE_EXT4:
      blockIsUsed = EXT_blockIsUsed(fileSystemHandle->deviceHandle,fileSystemHandle->type,&fileSystemHandle->extHandle,offset);
      break;
    case FILE_SYSTEM_TYPE_FAT12:
    case FILE_SYSTEM_TYPE_FAT16:
    case FILE_SYSTEM_TYPE_FAT32:
      blockIsUsed = FAT_blockIsUsed(fileSystemHandle->deviceHandle,fileSystemHandle->type,&fileSystemHandle->fatHandle,offset);
      break;
    case FILE_SYSTEM_TYPE_REISERFS3_5:
    case FILE_SYSTEM_TYPE_REISERFS3_6:
      blockIsUsed = ReiserFS_blockIsUsed(fileSystemHandle->deviceHandle,fileSystemHandle->type,&fileSystemHandle->reiserFSHandle,offset);
      break;
    default:
      blockIsUsed = TRUE;
      break;
  }

  return blockIsUsed;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
