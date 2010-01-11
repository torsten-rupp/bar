/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/filesystems_ext.c,v $
* $Revision: 1.4 $
* $Author: torsten $
* Contents: Backup ARchiver EXT2/3 file system plug in
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define EXT_FIRST_SUPER_BLOCK_OFFSET 1024
#define EXT_SUPER_MAGIC              0xEF53
#define EXT_MAX_BLOCK_SIZE           4096

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
  uchar  bitmapData[EXT_MAX_BLOCK_SIZE];    // bitmap block data
} EXTHandle;

/* ext2/ext3 super block */
typedef struct
{
  uint32 inodeCount;
  uint32 blocksCount;
  uint32 reserverBlocksCount;
  uint32 freeBlocksCount;
  uint32 freeInodesCount;
  uint32 firstDataBlock;
  uint32 logBlockSize;
  int32  logFragmentSize;
  uint32 blocksPerGroup;
  uint32 fragmentsPerGroup;
  uint32 inodesPerGroup;
  uint32 mountedTimestamp;
  uint32 writeTimestamp;
  uint16 mountedCount;
  int16  maxMountedCount;
  uint16 magic;
  uint16 state;
  uint16 errors;
  uint16 minorRevisionLevel;
  uint32 lastCheckTimestamp;
  uint32 checkInterval;
  uint32 creatorOS;
  uint32 revisionLevel;
  uint16 defaultUIDReservedBlocks;
  uint16 defaultGIDReservedBlocks;

  /* only when revision level EXT2_DYNAMIC_REV is set */
  uint32 firstInode;
  uint16 inodeSize;
  uint16 blockGroupNumber;
  uint32 featureCompatibility;
  uint32 featureInCompatibility;
  uint32 featureReadOnlyCompatibility;
  uint8  uuid[16];
  char   volumeName[16];
  char   lastMounted[64];
  uint32 algorithmUsageBitmap;

  /* only when EXT2_COMPAT_PREALLOC is set */
  uint8  preallocBlocks;
  uint8  preallocDirectoryBlocks;
  uint16 padding0;

  /* only when EXT3_FEATURE_COMPAT_HAS_JOURNAL is set */
  uint8  journalUUID[16];
  uint32 journalInodeNumber;
  uint32 journalDevice;
  uint32 lastOrphan;
  uint32 hashSeed[4];
  uint8  defaultHashVersion;
  uint8  padding1;
  uint16 padding2;
  uint32 defaultMountOptions;
  uint32 firstMetaBlockGroup;
  uint32 reserved[190];
} EXTSuperBlock ATTRIBUTE_PACKED;

/* ext2/ext3 group descriptor */
typedef struct
{
  uint32 blockBitmap;
  uint32 inodeBitmap;
  uint32 inodeTable;
  uint16 freeBlocksCount;
  uint16 freeInodesCount;
  uint16 usedDirectoriesCount;
  uint16 padding0;
  uint32 reserver0[3];
} EXTGroupDescriptor ATTRIBUTE_PACKED;

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
  EXTSuperBlock      extSuperBlock;
  EXTGroupDescriptor extGroupDescriptor;
  uint32             z;

  assert(deviceHandle != NULL);
  assert(extHandle != NULL);
  assert(sizeof(extSuperBlock) == 1024);
  assert(sizeof(extGroupDescriptor) == 32);

  /* read first super-block */
  if (Device_seek(deviceHandle,EXT_FIRST_SUPER_BLOCK_OFFSET) != ERROR_NONE)
  {
    return FALSE;
  }
  if (Device_read(deviceHandle,&extSuperBlock,sizeof(extSuperBlock),NULL) != ERROR_NONE)
  {
    return FALSE;
  }

  /* check if this a super block */
  if ((uint16)(LE16_TO_HOST(extSuperBlock.magic)) != EXT_SUPER_MAGIC)
  {
    return FALSE;
  }

  /* get block size */
  switch (LE32_TO_HOST(extSuperBlock.logBlockSize))
  {
    case 0: extHandle->blockSize = 1024; break;
    case 1: extHandle->blockSize = 2048; break;
    case 2: extHandle->blockSize = 4096; break;
    default:
      return FALSE;
      break;
  }

  /* get file system block info */
  extHandle->totalBlocks    = LE32_TO_HOST(extSuperBlock.blocksCount);
  extHandle->firstDataBlock = LE32_TO_HOST(extSuperBlock.firstDataBlock);
  extHandle->blocksPerGroup = LE32_TO_HOST(extSuperBlock.blocksPerGroup);
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
  for (z = 0; z < extHandle->bitmapBlocksCount; z++)
  {
    if (Device_read(deviceHandle,&extGroupDescriptor,sizeof(extGroupDescriptor),NULL) != ERROR_NONE)
    {
      free(extHandle->bitmapBlocks);
      return FALSE;
    }
    extHandle->bitmapBlocks[z] = LE32_TO_HOST(extGroupDescriptor.blockBitmap);
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
