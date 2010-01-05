/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/filesystems_ext2.c,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Backup ARchiver EXT2 file system plug in
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <linux/ext2_fs.h>
#include <utils.h>

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
typedef struct
{
  uint   blockSize;                      // block size (1024, 2048, 4096)
  ulong  firstDataBlock;                 // first data block
  ulong  blocksPerGroup;                 // number of blocks in block group
  uint64 totalBlocks;                    // total number of blocks
  uint32 *bitmapBlocks;                 
  uint32 bitmapBlocksCount;
  int64  block;                          // currently read block
  uchar  blockData[EXT2_MAX_BLOCK_SIZE]; // block data
} EXT2Handle;

/***************************** Variables *******************************/

/****************************** Macros *********************************/
#define BLOCK_TO_OFFSET(ext2Handle,block) ((block)*ext2Handle->blockSize)

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

LOCAL bool EXT2_init(DeviceHandle *deviceHandle, EXT2Handle *ext2Handle)
{
  struct ext2_super_block superBlock;
  struct ext2_group_desc  groupDescriptor;
  uint32                  z;

  assert(deviceHandle != NULL);
  assert(ext2Handle != NULL);

  /* read first super-block */
  if (Device_seek(deviceHandle,1024) != ERROR_NONE)
  {
    return FALSE;
  }
  if (Device_read(deviceHandle,&superBlock,1024,NULL) != ERROR_NONE)
  {
    return FALSE;
  }

  /* check if this a super block */
  if ((__u16)(le16_to_cpu(superBlock.s_magic)) != EXT2_SUPER_MAGIC) return FALSE;

  /* get block size */
  switch (le32_to_cpu(superBlock.s_log_block_size))
  {
    case 0: ext2Handle->blockSize = 1024; break;
    case 1: ext2Handle->blockSize = 2048; break;
    case 2: ext2Handle->blockSize = 4096; break;
    default:
      return FALSE;
      break;
  }

  /* get file system block info */
  ext2Handle->totalBlocks    = le32_to_cpu(superBlock.s_blocks_count);
  ext2Handle->firstDataBlock = le32_to_cpu(superBlock.s_first_data_block);
  ext2Handle->blocksPerGroup = le32_to_cpu(superBlock.s_blocks_per_group);
  ext2Handle->block          = -1;

  /* read bitmap blocks */
  ext2Handle->bitmapBlocksCount = (ext2Handle->totalBlocks+ext2Handle->blocksPerGroup-1)/ext2Handle->blocksPerGroup;;
  ext2Handle->bitmapBlocks = (uint32*)malloc(ext2Handle->bitmapBlocksCount*sizeof(uint32));
  if (ext2Handle->bitmapBlocks == NULL)
  {
    return FALSE;
  }
  if (Device_seek(deviceHandle,BLOCK_TO_OFFSET(ext2Handle,2)) != ERROR_NONE)
  {
    free(ext2Handle->bitmapBlocks);
    return FALSE;
  }
  for (z = 0; z < ext2Handle->bitmapBlocksCount; z++)
  {
    if (Device_read(deviceHandle,&groupDescriptor,sizeof(struct ext2_group_desc),NULL) != ERROR_NONE)
    {
      free(ext2Handle->bitmapBlocks);
      return FALSE;
    }
    ext2Handle->bitmapBlocks[z] = le32_to_cpu(groupDescriptor.bg_block_bitmap);
  }

  return TRUE;
}

LOCAL void EXT2_done(DeviceHandle *deviceHandle, EXT2Handle *ext2Handle)
{
  assert(deviceHandle != NULL);
  assert(ext2Handle != NULL);

  UNUSED_VARIABLE(deviceHandle);

  free(ext2Handle->bitmapBlocks);
}

LOCAL bool EXT2_blockIsUsed(DeviceHandle *deviceHandle, EXT2Handle *ext2Handle, uint64 blockOffset)
{
  uint32 block;
  uint32 bitmapBlock;

  assert(deviceHandle != NULL);
  assert(ext2Handle != NULL);

  /* calculate block */
  block = blockOffset/ext2Handle->blockSize;

  if (block >= 1)
  {
    /* calculate bitmap block */
    bitmapBlock = (block-1)/(ext2Handle->blockSize*8);

    /* read correct block bitmap if needed */
    if (ext2Handle->block != bitmapBlock)
    {
      if (Device_seek(deviceHandle,BLOCK_TO_OFFSET(ext2Handle,ext2Handle->bitmapBlocks[bitmapBlock])) != ERROR_NONE)
      {
        return TRUE;
      }
      if (Device_read(deviceHandle,ext2Handle->blockData,ext2Handle->blockSize,NULL) != ERROR_NONE)
      {
        return TRUE;
      }
      ext2Handle->block = bitmapBlock;
    }

    /* check if block used */
    return ((ext2Handle->blockData[(block-1)/8] & (1 << (block-1)%8)) != 0);
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
