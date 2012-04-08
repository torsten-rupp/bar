/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver EXT2/3/4 file system plug in
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define EXT2_FIRST_SUPER_BLOCK_OFFSET  1024
#define EXT2_SUPER_MAGIC               0xEF53

#define EXT2_MAX_BLOCK_SIZE            (64*1024)

#define EXT2_REVISION_OLD              0
#define EXT2_REVISION_DYNAMIC          1

#define EXT4_MAX_GROUP_DESCRIPTOR_SIZE 1024
#define EXT4_FEATURE_INCOMPAT_64BIT    0x0080

/***************************** Datatypes *******************************/
typedef struct
{
  enum
  {
    EXT2_OR_3,
    EXT4
  }      type;                              // EXT type (ext2/3/4)
  uint   blockSize;                         // block size [bytes]
  uint   groupDescriptorSize;               // group descriptor size [bytes]
  uint32 blocksPerGroup;                    // number of blocks in block group
  uint64 firstDataBlock;                    // first data block (0..n-1)
  uint64 totalBlocks;                       // total number of blocks
  uint64 *bitmapBlocks;
  uint32 bitmapBlocksCount;
  int    bitmapIndex;                       // index of currently read bitmap (0..n-1)
  uchar  bitmapData[EXT2_MAX_BLOCK_SIZE];   // bitmap block data
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
  uint32 featureCompatible;
  uint32 featureInCompatible;
  uint32 featureReadOnlyCompatible;
  uint8  uuid[16];
  char   volumeName[16];
  char   lastMounted[64];
  uint32 algorithmUsageBitmap;

  /* only when EXT2_COMPAT_PREALLOC is set */
  uint8  preallocBlocks;
  uint8  preallocDirectoryBlocks;
  uint16 reservedGroupTableBlocks;

  /* only when EXT3_FEATURE_COMPAT_HAS_JOURNAL is set */
  uint8  journalUUID[16];
  uint32 journalInodeNumber;
  uint32 journalDevice;
  uint32 lastOrphan;
  uint32 hashSeed[4];
  uint8  defaultHashVersion;
  uint8  defaultJournalBackupType;
  uint16 groupDescriptorSize;
  uint32 defaultMountOptions;
  uint32 firstMetaBlockGroup;
  uint32 mkfsTimestamp;
  uint32 journalBlocks[17];

  /* only when EXT2_DYNAMIC_REV & EXT4_FEATURE_INCOMPAT_64BIT is set */
  uint32 blocksCountHigh;
  uint32 reserverBlocksCountHigh;
  uint32 freeBlocksCountHigh;
  uint16 minExtraInodeSize;
  uint16 wantExtraInodeSize;
  uint32 flags;
  uint16 raidStride;
  uint16 mmpInternal;
  uint64 mmpBlock;
  uint32 raidStrideWidth;
  uint8  logGroupsPerFlex;
  uint8  pad0;
  uint16 pad1;
  uint64 kbytesWritten;

  uint32 reserved[160];
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
  uint16 flags;
  uint32 reserver0[2];
  uint16 inodeTableUnused;
  uint16 checksum;
} EXT2GroupDescriptor ATTRIBUTE_PACKED;

/* ext4 group descriptor */
typedef struct
{
  uint32 blockBitmap;
  uint32 inodeBitmap;
  uint32 inodeTable;
  uint16 freeBlocksCount;
  uint16 freeInodesCount;
  uint16 usedDirectoriesCount;
  uint16 flags;
  uint32 reserver0[2];
  uint16 inodeTableUnused;
  uint16 checksum;
  uint32 blockBitmapHigh;
  uint32 inodeBitmapHigh;
  uint32 inodeTableHigh;
  uint16 freeBlocksCountHigh;
  uint16 freeInodesCountHigh;
  uint16 usedDirectoriesCountHigh;
  uint16 pad0;
  uint32 reserver1[3];
} EXT4GroupDescriptor ATTRIBUTE_PACKED;

/***************************** Variables *******************************/

/****************************** Macros *********************************/
#define LOW_HIGH_TO_UINT64(low,high) ((((uint64)(high)) << 32) | (((uint64)(low)) << 0))

#define EXT_BLOCK_TO_OFFSET(extHandle,block) ((block)*extHandle->blockSize)

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

LOCAL bool EXT_init(DeviceHandle *deviceHandle, EXTHandle *extHandle)
{
  EXTSuperBlock       extSuperBlock;
  EXT2GroupDescriptor ext2GroupDescriptor;
  EXT4GroupDescriptor ext4GroupDescriptor;
  uint32              z;

  assert(deviceHandle != NULL);
  assert(extHandle != NULL);
  assert(sizeof(extSuperBlock) == 1024);
  assert(sizeof(ext2GroupDescriptor) == 32);
  assert(sizeof(ext4GroupDescriptor) == 64);

  /* read first super-block */
  if (Device_seek(deviceHandle,EXT2_FIRST_SUPER_BLOCK_OFFSET) != ERROR_NONE)
  {
    return FALSE;
  }
  if (Device_read(deviceHandle,&extSuperBlock,sizeof(extSuperBlock),NULL) != ERROR_NONE)
  {
    return FALSE;
  }

  /* check if this a super block */
  if ((uint16)(LE16_TO_HOST(extSuperBlock.magic)) != EXT2_SUPER_MAGIC)
  {
    return FALSE;
  }

  /* get block size */
  switch (LE32_TO_HOST(extSuperBlock.logBlockSize))
  {
    case 0: extHandle->blockSize =  1*1024; break;
    case 1: extHandle->blockSize =  2*1024; break;
    case 2: extHandle->blockSize =  4*1024; break;
    case 3: extHandle->blockSize =  8*1024; break;
    case 4: extHandle->blockSize = 16*1024; break;
    case 5: extHandle->blockSize = 32*1024; break;
    case 6: extHandle->blockSize = 64*1024; break;
    default:
      return FALSE;
      break;
  }

  /* get ext type: ext2/3/4/ */
  if (   (LE32_TO_HOST(extSuperBlock.revisionLevel) == EXT2_REVISION_DYNAMIC)
      && ((LE32_TO_HOST(extSuperBlock.featureCompatible) & EXT4_FEATURE_INCOMPAT_64BIT) == EXT4_FEATURE_INCOMPAT_64BIT)
     )
  {
    /* ext4 */
    extHandle->type = EXT4;
  }
  else
  {
    /* ext2/ext3 */
    extHandle->type = EXT2_OR_3;
  }

  /* get file system block info, init data */
  switch (extHandle->type)
  {
    case EXT2_OR_3:
      extHandle->groupDescriptorSize = sizeof(EXT2GroupDescriptor);
      extHandle->blocksPerGroup      = LE32_TO_HOST(extSuperBlock.blocksPerGroup);
      extHandle->firstDataBlock      = (uint64)LE32_TO_HOST(extSuperBlock.firstDataBlock);
      extHandle->totalBlocks         = (uint64)LE32_TO_HOST(extSuperBlock.blocksCount);
      break;
    case EXT4:
      extHandle->groupDescriptorSize = (uint)LE16_TO_HOST(extSuperBlock.groupDescriptorSize);
      extHandle->blocksPerGroup      = LE32_TO_HOST(extSuperBlock.blocksPerGroup);
      extHandle->firstDataBlock      = (uint64)LE32_TO_HOST(extSuperBlock.firstDataBlock);
      extHandle->totalBlocks         = LOW_HIGH_TO_UINT64(LE32_TO_HOST(extSuperBlock.blocksCount),
                                                          LE32_TO_HOST(extSuperBlock.blocksCountHigh)
                                                         );
      break;
  }
  extHandle->bitmapIndex = -1;

  /* validate data */
  if (   !((extHandle->groupDescriptorSize > 0) && (extHandle->groupDescriptorSize <= EXT4_MAX_GROUP_DESCRIPTOR_SIZE))
      || !(extHandle->blocksPerGroup > 0)
      || !(extHandle->totalBlocks > 0)
      || !(extHandle->firstDataBlock >= 1)
     )
  {
    return FALSE;
  }

  /* read group descriptors and detect bitmap block numbers */
  extHandle->bitmapBlocksCount = (extHandle->totalBlocks+extHandle->blocksPerGroup-1)/extHandle->blocksPerGroup;;
  extHandle->bitmapBlocks = (uint64*)malloc(extHandle->bitmapBlocksCount*sizeof(uint64));
  if (extHandle->bitmapBlocks == NULL)
  {
    return FALSE;
  }
  for (z = 0; z < extHandle->bitmapBlocksCount; z++)
  {
    if (Device_seek(deviceHandle,EXT_BLOCK_TO_OFFSET(extHandle,extHandle->firstDataBlock+1)+(uint64)z*(uint64)extHandle->groupDescriptorSize) != ERROR_NONE)
    {
      free(extHandle->bitmapBlocks);
      return FALSE;
    }
    switch (extHandle->type)
    {
      case EXT2_OR_3:
        if (Device_read(deviceHandle,&ext2GroupDescriptor,sizeof(ext2GroupDescriptor),NULL) != ERROR_NONE)
        {
          free(extHandle->bitmapBlocks);
          return FALSE;
        }
        extHandle->bitmapBlocks[z] = (uint64)LE32_TO_HOST(ext2GroupDescriptor.blockBitmap);
        break;
      case EXT4:
        if (Device_read(deviceHandle,&ext4GroupDescriptor,sizeof(ext4GroupDescriptor),NULL) != ERROR_NONE)
        {
          free(extHandle->bitmapBlocks);
          return FALSE;
        }
        extHandle->bitmapBlocks[z] = LOW_HIGH_TO_UINT64(LE32_TO_HOST(ext4GroupDescriptor.blockBitmap),
                                                        LE32_TO_HOST(ext4GroupDescriptor.blockBitmapHigh)
                                                       );
        break;
    }
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
    if (extHandle->bitmapIndex != (int)bitmapIndex)
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
