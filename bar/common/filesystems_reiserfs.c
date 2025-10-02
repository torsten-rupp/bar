/***********************************************************************\
*
* Contents: ReiserFS file system
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/

#include "common/global.h"
#include "common/filesystems.h"

#include "filesystems_reiserfs.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define REISERFS_SUPER_BLOCK_OFFSET (64*1024)
#define REISERFS_SUPER_MAGIC_STRING_V1 "ReIsErFs"
#define REISERFS_SUPER_MAGIC_STRING_V2 "ReIsEr2Fs"
#define REISERFS_SUPER_MAGIC_STRING_V3 "ReIsEr3Fs"
#define REISERFS_SUPER_MAGIC_STRING_V4 "ReIsEr4"

#define REISERFS_MAX_BLOCK_SIZE 8192

/***************************** Datatypes *******************************/

typedef struct
{
  uint32 blockCount;
  uint32 freeBlocks;
  uint32 rootBlock;
  uint32 journal[8];
  uint16 blockSize;
  uint16 oidMaxSize;
  uint16 oidCurrentSize;
  uint16 state;
  char   magicString[12];
  uint32 hashFunctionCode;
  uint16 treeHeight;
  uint16 bitmapNumber;
  uint16 version;
  uint16 reserver0;
  uint32 inodeGeneration;
  uint32 flags;
  uchar  uuid[16];
  uchar  label[16];
  byte   unused0[88];
} ATTRIBUTE_PACKED ReiserSuperBlock;
static_assert(sizeof(ReiserSuperBlock) == 204);

/***************************** Variables *******************************/

/****************************** Macros *********************************/
// convert from little endian to host system format
#if __BYTE_ORDER == __LITTLE_ENDIAN
  #define LE16_TO_HOST(x) (x)
  #define LE32_TO_HOST(x) (x)
#else /* not __BYTE_ORDER == __LITTLE_ENDIAN */
  #define LE16_TO_HOST(x) swapBytes16(x)
  #define LE32_TO_HOST(x) swapBytes32(x)
#endif /* __BYTE_ORDER == __LITTLE_ENDIAN */

#define REISERFS_BLOCK_TO_OFFSET(fileSystemHandle,block) ((block)*reiserFSHandle->blockSize)

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : REISERFS_getType
* Purpose: get ReiserFS file system type
* Input  : deviceHandle   - device handle
* Output : -
* Return : TRUE iff file system initialized
* Notes  : -
\***********************************************************************/

LOCAL bool ReiserFS_getType(DeviceHandle *deviceHandle)
{
  assert(deviceHandle != NULL);

  // read super-block
  ReiserSuperBlock reiserSuperBlock;
  if (Device_seek(deviceHandle,REISERFS_SUPER_BLOCK_OFFSET) != ERROR_NONE)
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }
  if (Device_read(deviceHandle,&reiserSuperBlock,sizeof(reiserSuperBlock),NULL) != ERROR_NONE)
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }

  // check if this a ReiserFS super block, detect file system type
  FileSystemTypes fileSystemType;
  if      (stringStartsWith(reiserSuperBlock.magicString,REISERFS_SUPER_MAGIC_STRING_V1))
  {
    fileSystemType = FILE_SYSTEM_TYPE_REISERFS3_5;
  }
  else if (   stringStartsWith(reiserSuperBlock.magicString,REISERFS_SUPER_MAGIC_STRING_V2)
           || stringStartsWith(reiserSuperBlock.magicString,REISERFS_SUPER_MAGIC_STRING_V3)
          )
  {
    fileSystemType = FILE_SYSTEM_TYPE_REISERFS3_6;
  }
  else if (stringStartsWith(reiserSuperBlock.magicString,REISERFS_SUPER_MAGIC_STRING_V4))
  {
    fileSystemType = FILE_SYSTEM_TYPE_REISERFS4;
  }
  else
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }

  // get file system block info
  uint64 totalBlocks = LE32_TO_HOST(reiserSuperBlock.blockCount);
  uint   blockSize   = LE32_TO_HOST(reiserSuperBlock.blockSize);

  // validate data
  if (   !(blockSize >= 512)
      || !((blockSize % 512) == 0)
      || !(totalBlocks > 0)
     )
  {
    fileSystemType = FILE_SYSTEM_TYPE_UNKNOWN;
  }

  return fileSystemType;
}

/***********************************************************************\
* Name   : ReiserFS_init
* Purpose: initialize ReiserFS handle
* Input  : deviceHandle - device handle
* Output : fileSystemType - file system type
*          reiserFSHandle - ReiserFS handle (can be NULL)
* Return : TRUE iff file system initialized
* Notes  : -
\***********************************************************************/

LOCAL bool ReiserFS_init(DeviceHandle *deviceHandle, FileSystemTypes *fileSystemType, ReiserFSHandle *reiserFSHandle)
{
  assert(deviceHandle != NULL);
  assert(fileSystemType != NULL);

  // read super-block
  ReiserSuperBlock reiserSuperBlock;
  if (Device_seek(deviceHandle,REISERFS_SUPER_BLOCK_OFFSET) != ERROR_NONE)
  {
    return FALSE;
  }
  if (Device_read(deviceHandle,&reiserSuperBlock,sizeof(reiserSuperBlock),NULL) != ERROR_NONE)
  {
    return FALSE;
  }

  // check if this is a ReiserFS super block, detect file system type
  if      (stringStartsWith(reiserSuperBlock.magicString,REISERFS_SUPER_MAGIC_STRING_V1))
  {
    (*fileSystemType) = FILE_SYSTEM_TYPE_REISERFS3_5;
  }
  else if (   stringStartsWith(reiserSuperBlock.magicString,REISERFS_SUPER_MAGIC_STRING_V2)
           || stringStartsWith(reiserSuperBlock.magicString,REISERFS_SUPER_MAGIC_STRING_V3)
          )
  {
    (*fileSystemType) = FILE_SYSTEM_TYPE_REISERFS3_6;
  }
  else if (stringStartsWith(reiserSuperBlock.magicString,REISERFS_SUPER_MAGIC_STRING_V4))
  {
    (*fileSystemType) = FILE_SYSTEM_TYPE_REISERFS4;
  }
  else
  {
    return FALSE;
  }

  if (reiserFSHandle != NULL)
  {
    // get file system block info
    reiserFSHandle->totalBlocks = LE32_TO_HOST(reiserSuperBlock.blockCount);
    reiserFSHandle->blockSize   = LE32_TO_HOST(reiserSuperBlock.blockSize);
    reiserFSHandle->bitmapIndex = -1;

    // validate data
    if (   !(reiserFSHandle->blockSize >= 512)
        || !((reiserFSHandle->blockSize % 512) == 0)
        || !(reiserFSHandle->totalBlocks > 0)
       )
    {
      return FALSE;
    }
  }

  return TRUE;
}

/***********************************************************************\
* Name   : ReiserFS_done
* Purpose: deinitialize ReiserFS handle
* Input  : reiserFSandle - ReiserFS handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void ReiserFS_done(ReiserFSHandle *reiserFSandle)
{
  assert(reiserFSandle != NULL);

  UNUSED_VARIABLE(reiserFSandle);
}

/***********************************************************************\
* Name   : ReiserFS_blockIsUsed
* Purpose: check if block is used
* Input  : deviceHandle   - device handle
*          fileSystemType - file system type
*          reiserFSHandle - ReiserFS handle
*          offset         - offset in image
* Output : -
* Return : TRUE iff block at offset is used, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool ReiserFS_blockIsUsed(DeviceHandle *deviceHandle, FileSystemTypes fileSystemType, ReiserFSHandle *reiserFSHandle, uint64 offset)
{
  assert(deviceHandle != NULL);
  assert(   (fileSystemType == FILE_SYSTEM_TYPE_REISERFS3_5)
         || (fileSystemType == FILE_SYSTEM_TYPE_REISERFS3_6)
        );
  assert(reiserFSHandle != NULL);
  assert(reiserFSHandle->blockSize != 0);

  // calculate block
  uint32 block = offset/reiserFSHandle->blockSize;

  if (block >= 17)
  {
    // calculate bitmap index
    uint bitmapIndex = block/(reiserFSHandle->blockSize*8);

    // read correct block bitmap if needed
    if (reiserFSHandle->bitmapIndex != (int)bitmapIndex)
    {
      uint32 bitmapBlock = (bitmapIndex > 0)
                            ? (uint32)bitmapIndex*(uint32)reiserFSHandle->blockSize*8
                            : REISERFS_SUPER_BLOCK_OFFSET/reiserFSHandle->blockSize+1;
      if (Device_seek(deviceHandle,REISERFS_BLOCK_TO_OFFSET(fileSystemHandle,bitmapBlock)) != ERROR_NONE)
      {
        return TRUE;
      }
      if (Device_read(deviceHandle,reiserFSHandle->bitmapData,reiserFSHandle->blockSize,NULL) != ERROR_NONE)
      {
        return TRUE;
      }
      reiserFSHandle->bitmapIndex = bitmapIndex;
    }

    // check if block is used
    uint index = block-bitmapIndex*reiserFSHandle->blockSize*8;
    return ((reiserFSHandle->bitmapData[index/8] & (1 << index%8)) != 0);
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
