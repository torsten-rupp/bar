/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/filesystems_ext.c,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Backup ARchiver EXT2 file system plug in
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <linux/ext2_fs.h>

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
typedef struct
{
  uint   blockSize;                         // block size (1024, 2048, 4096)
  ulong  firstDataBlock;                    // first data block
  ulong  blocksPerGroup;                    // number of blocks in block group
  uint64 totalBlocks;                       // total number of blocks
  uint32 *bitmapBlocks;                 
  uint32 bitmapBlocksCount;
  int    bitmapIndex;                       // index of currently read bitmap
  uchar  bitmapData[EXT2_MAX_BLOCK_SIZE];   // bitmap block data
} EXTHandle;

/***************************** Variables *******************************/

/****************************** Macros *********************************/
#define EXT_BLOCK_TO_OFFSET(extHandle,block) ((block)*extHandle->blockSize)

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

LOCAL bool EXT_init(DeviceHandle *deviceHandle, EXTHandle *extHandle)
{
  struct ext2_super_block superBlock;
  struct ext2_group_desc  groupDescriptor;
  uint32                  z;

  assert(deviceHandle != NULL);
  assert(extHandle != NULL);

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
    case 0: extHandle->blockSize = 1024; break;
    case 1: extHandle->blockSize = 2048; break;
    case 2: extHandle->blockSize = 4096; break;
    case 3: extHandle->blockSize = 8192; break;
    default:
      return FALSE;
      break;
  }

  /* get file system block info */
  extHandle->totalBlocks    = le32_to_cpu(superBlock.s_blocks_count);
  extHandle->firstDataBlock = le32_to_cpu(superBlock.s_first_data_block);
  extHandle->blocksPerGroup = le32_to_cpu(superBlock.s_blocks_per_group);
  extHandle->bitmapIndex    = -1;

  /* validate data */
  if (   !(extHandle->totalBlocks > 0)
      || !(extHandle->firstDataBlock >= 1)
      || !(extHandle->blocksPerGroup > 0)
     )
  {
    return FALSE;
  }

  /* read group descriptors and detect bitmap block numbers */
  extHandle->bitmapBlocksCount = (extHandle->totalBlocks+extHandle->blocksPerGroup-1)/extHandle->blocksPerGroup;;
  extHandle->bitmapBlocks = (uint32*)malloc(extHandle->bitmapBlocksCount*sizeof(uint32));
  if (extHandle->bitmapBlocks == NULL)
  {
    return FALSE;
  }
  if (Device_seek(deviceHandle,EXT_BLOCK_TO_OFFSET(extHandle,extHandle->firstDataBlock+1)) != ERROR_NONE)
  {
    free(extHandle->bitmapBlocks);
    return FALSE;
  }
  assert(sizeof(struct ext2_group_desc) == 32);
  for (z = 0; z < extHandle->bitmapBlocksCount; z++)
  {
    if (Device_read(deviceHandle,&groupDescriptor,sizeof(struct ext2_group_desc),NULL) != ERROR_NONE)
    {
      free(extHandle->bitmapBlocks);
      return FALSE;
    }
    extHandle->bitmapBlocks[z] = le32_to_cpu(groupDescriptor.bg_block_bitmap);
  }

#if 0
fprintf(stderr,"\n");
for (z = 0; z < extHandle->bitmapBlocksCount; z++)
{
fprintf(stderr,"%s,%d: z=%d block=%ld used=%d\n",__FILE__,__LINE__,z,extHandle->bitmapBlocks[z],EXT_blockIsUsed(deviceHandle,extHandle,extHandle->bitmapBlocks[z]));
}
#endif /* 0 */

  return TRUE;
}

LOCAL void EXT_done(DeviceHandle *deviceHandle, EXTHandle *extHandle)
{
  assert(deviceHandle != NULL);
  assert(extHandle != NULL);

  UNUSED_VARIABLE(deviceHandle);

  free(extHandle->bitmapBlocks);
}

LOCAL bool EXT_blockIsUsed(DeviceHandle *deviceHandle, EXTHandle *extHandle, uint64 offset)
{
  uint64 block;
  uint   bitmapIndex;
  uint   index;

  assert(deviceHandle != NULL);
  assert(extHandle != NULL);

  /* calculate block */
  block = offset/extHandle->blockSize;

  if (block >= 1)
  {
    /* calculate bitmap index */
    assert(extHandle->blocksPerGroup != 0);
    bitmapIndex = (block-1)/extHandle->blocksPerGroup;
    assert(bitmapIndex < extHandle->bitmapBlocksCount);

    /* read correct block bitmap if needed */
    if (extHandle->bitmapIndex != bitmapIndex)
    {
      if (Device_seek(deviceHandle,EXT_BLOCK_TO_OFFSET(extHandle,extHandle->bitmapBlocks[bitmapIndex])) != ERROR_NONE)
      {
        return TRUE;
      }
      if (Device_read(deviceHandle,extHandle->bitmapData,extHandle->blockSize,NULL) != ERROR_NONE)
      {
        return TRUE;
      }
      extHandle->bitmapIndex = bitmapIndex;
    }

    /* check if block is used */
    assert((block-1) >= bitmapIndex*extHandle->blocksPerGroup);
    index = (block-1)-bitmapIndex*extHandle->blocksPerGroup;
    return ((extHandle->bitmapData[index/8] & (1 << index%8)) != 0);
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
