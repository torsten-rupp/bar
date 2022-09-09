/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver ReiserFS file system plug in
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/

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
  uint   blockSize;                             // block size (1024, 2048, 4096, 8192)
  ulong  firstDataBlock;                        // first data block
  uint32 totalBlocks;                           // total number of blocks
  int    bitmapIndex;                           // index of currently read bitmap
  uchar  bitmapData[REISERFS_MAX_BLOCK_SIZE];   // bitmap block data
} REISERFSHandle;

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

/***************************** Variables *******************************/

/****************************** Macros *********************************/
#define REISERFS_BLOCK_TO_OFFSET(reiserFSHandle,block) ((block)*reiserFSHandle->blockSize)

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : REISERFS_init
* Purpose: initialize ReiserFS handle
* Input  : deviceHandle   - device handle
*          reiserFSHandle - ReiserFS handle variable
* Output : reiserFSHandle - ReiserFS variable
* Return : file system type or FILE_SYSTEN_UNKNOWN;
* Notes  : -
\***********************************************************************/

LOCAL FileSystemTypes REISERFS_init(DeviceHandle *deviceHandle, REISERFSHandle *reiserFSHandle)
{
  ReiserSuperBlock reiserSuperBlock;
  FileSystemTypes  fileSystemType;

  assert(deviceHandle != NULL);
  assert(reiserFSHandle != NULL);

  // read super-block
  if (Device_seek(deviceHandle,REISERFS_SUPER_BLOCK_OFFSET) != ERROR_NONE)
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }
  if (Device_read(deviceHandle,&reiserSuperBlock,sizeof(reiserSuperBlock),NULL) != ERROR_NONE)
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }

  // check if this a ReiserFS super block, detect file system type
  if      (stringStartsWith(reiserSuperBlock.magicString,REISERFS_SUPER_MAGIC_STRING_V1))
  {
    fileSystemType = FILE_SYSTEM_TYPE_REISERFS1;
  }
  else if (   stringStartsWith(reiserSuperBlock.magicString,REISERFS_SUPER_MAGIC_STRING_V2)
           || stringStartsWith(reiserSuperBlock.magicString,REISERFS_SUPER_MAGIC_STRING_V3)
          )
  {
    fileSystemType = FILE_SYSTEM_TYPE_REISERFS3;
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
  reiserFSHandle->totalBlocks = LE32_TO_HOST(reiserSuperBlock.blockCount);
  reiserFSHandle->blockSize   = LE32_TO_HOST(reiserSuperBlock.blockSize);
  reiserFSHandle->bitmapIndex = -1;

  // validate data
  if (   !(reiserFSHandle->blockSize >= 512)
      || !((reiserFSHandle->blockSize % 512) == 0)
      || !(reiserFSHandle->totalBlocks > 0)
     )
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }

  return fileSystemType;
}

/***********************************************************************\
* Name   : REISERFS_done
* Purpose: deinitialize ReiserFS handle
* Input  : deviceHandle   - device handle
*          reiserFSHandle - ReiserFS handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void REISERFS_done(DeviceHandle *deviceHandle, REISERFSHandle *reiserFSHandle)
{
  assert(deviceHandle != NULL);
  assert(reiserFSHandle != NULL);

  UNUSED_VARIABLE(deviceHandle);
  UNUSED_VARIABLE(reiserFSHandle);
}

/***********************************************************************\
* Name   : REISERFS_blockIsUsed
* Purpose: check if block is used
* Input  : deviceHandle   - device handle
*          reiserFSHandle - ReiserFS handle
*          offset         - offset in image
* Output : -
* Return : TRUE iff block at offset is used, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool REISERFS_blockIsUsed(DeviceHandle *deviceHandle, REISERFSHandle *reiserFSHandle, uint64 offset)
{
  uint32 block;
  uint   bitmapIndex;
  uint32 bitmapBlock;
  uint   index;

  assert(deviceHandle != NULL);
  assert(reiserFSHandle != NULL);
  assert(reiserFSHandle->blockSize != 0);

  // calculate block
  block = offset/reiserFSHandle->blockSize;

  if (block >= 17)
  {
    // calculate bitmap index
    assert(reiserFSHandle->blockSize != 0);
    bitmapIndex = block/(reiserFSHandle->blockSize*8);

    // read correct block bitmap if needed
    if (reiserFSHandle->bitmapIndex != (int)bitmapIndex)
    {
      bitmapBlock = ((bitmapIndex > 0)
                      ? (uint32)bitmapIndex*(uint32)reiserFSHandle->blockSize*8
                      : REISERFS_SUPER_BLOCK_OFFSET/reiserFSHandle->blockSize+1
                    );//*(uint32)reiserFSHandle->blockSize;
      if (Device_seek(deviceHandle,REISERFS_BLOCK_TO_OFFSET(reiserFSHandle,bitmapBlock)) != ERROR_NONE)
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
    index = block-bitmapIndex*reiserFSHandle->blockSize*8;
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
