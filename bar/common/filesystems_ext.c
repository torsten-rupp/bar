/***********************************************************************\
*
* Contents: Backup ARchiver EXT2/3/4 file system plug in
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/

#if 0
#warning debug only
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif /* 0 */

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// Note: use defines here, because headers files may not be available

// ext2
#define EXT2_FIRST_SUPER_BLOCK_OFFSET     1024
#define EXT2_SUPER_MAGIC                  0xEF53

#define EXT2_MAX_BLOCK_SIZE               (64*1024)

#define EXT2_REVISION_OLD                 0
#define EXT2_REVISION_DYNAMIC             1

#define EXT2_FEATURE_COMPAT_DIR_PREALLOC  0x0001
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES 0x0002
#define EXT2_FEATURE_COMPAT_EXT_ATTR      0x0008
#define EXT2_FEATURE_COMPAT_RESIZE_INO    0x0010
#define EXT2_FEATURE_COMPAT_DIR_INDEX     0x0020

#define EXT2_FEATURE_INCOMPAT_FILETYPE    0x0002
#define EXT2_FEATURE_INCOMPAT_META_BG     0x0010

#define EXT2_FEATURE_COMPAT_SUPP          (  EXT2_FEATURE_COMPAT_DIR_PREALLOC \
                                           | EXT2_FEATURE_COMPAT_IMAGIC_INODES \
                                           | EXT2_FEATURE_COMPAT_EXT_ATTR \
                                           | EXT2_FEATURE_COMPAT_RESIZE_INO \
                                           | EXT2_FEATURE_COMPAT_DIR_INDEX \
                                          )
#define EXT2_FEATURE_INCOMPAT_SUPP        (  EXT2_FEATURE_INCOMPAT_FILETYPE \
                                           | EXT2_FEATURE_INCOMPAT_META_BG \
                                          )

// ext3
#define EXT3_FEATURE_COMPAT_DIR_PREALLOC  0x0001
#define EXT3_FEATURE_COMPAT_IMAGIC_INODES 0x0002
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL   0x0004
#define EXT3_FEATURE_COMPAT_EXT_ATTR      0x0008
#define EXT3_FEATURE_COMPAT_RESIZE_INODE  0x0010
#define EXT3_FEATURE_COMPAT_DIR_INDEX     0x0020

#define EXT3_FEATURE_INCOMPAT_FILETYPE    0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER     0x0004
#define EXT3_FEATURE_INCOMPAT_META_BG     0x0010

#define EXT3_FEATURE_COMPAT_SUPP          (  EXT3_FEATURE_COMPAT_DIR_PREALLOC \
                                           | EXT3_FEATURE_COMPAT_IMAGIC_INODES \
                                           | EXT3_FEATURE_COMPAT_HAS_JOURNAL \
                                           | EXT3_FEATURE_COMPAT_EXT_ATTR \
                                           | EXT3_FEATURE_COMPAT_RESIZE_INODE \
                                           | EXT2_FEATURE_COMPAT_EXT_ATTR \
                                           | EXT3_FEATURE_COMPAT_DIR_INDEX \
                                          )
#define EXT3_FEATURE_INCOMPAT_SUPP        (  EXT3_FEATURE_INCOMPAT_FILETYPE \
                                           | EXT3_FEATURE_INCOMPAT_RECOVER \
                                           | EXT3_FEATURE_INCOMPAT_META_BG \
                                          )

// ext4
#define EXT4_MAX_GROUP_DESCRIPTOR_SIZE    1024

#define EXT4_FEATURE_COMPAT_DIR_PREALLOC  0x0001
#define EXT4_FEATURE_COMPAT_IMAGIC_INODES 0x0002
#define EXT4_FEATURE_COMPAT_HAS_JOURNAL   0x0004
#define EXT4_FEATURE_COMPAT_EXT_ATTR      0x0008
#define EXT4_FEATURE_COMPAT_RESIZE_INODE  0x0010
#define EXT4_FEATURE_COMPAT_DIR_INDEX     0x0020

#define EXT4_FEATURE_INCOMPAT_FILETYPE    0x0002
#define EXT4_FEATURE_INCOMPAT_RECOVER     0x0004
#define EXT4_FEATURE_INCOMPAT_META_BG     0x0010
#define EXT4_FEATURE_INCOMPAT_EXTENTS     0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT       0x0080
#define EXT4_FEATURE_INCOMPAT_MMP         0x0100
#define EXT4_FEATURE_INCOMPAT_FLEX_BG     0x0200

#define EXT4_FEATURE_COMPAT_SUPP          (  EXT4_FEATURE_COMPAT_DIR_PREALLOC \
                                           | EXT4_FEATURE_COMPAT_IMAGIC_INODES \
                                           | EXT4_FEATURE_COMPAT_HAS_JOURNAL \
                                           | EXT4_FEATURE_COMPAT_EXT_ATTR \
                                           | EXT4_FEATURE_COMPAT_RESIZE_INODE \
                                           | EXT4_FEATURE_COMPAT_DIR_INDEX \
                                          )
#define EXT4_FEATURE_INCOMPAT_SUPP        (  EXT4_FEATURE_INCOMPAT_FILETYPE \
                                           | EXT4_FEATURE_INCOMPAT_RECOVER \
                                           | EXT4_FEATURE_INCOMPAT_META_BG \
                                           | EXT4_FEATURE_INCOMPAT_EXTENTS \
                                           | EXT4_FEATURE_INCOMPAT_64BIT \
                                           | EXT4_FEATURE_INCOMPAT_MMP \
                                           | EXT4_FEATURE_INCOMPAT_FLEX_BG \
                                          )

/***************************** Datatypes *******************************/
typedef struct
{
  FileSystemTypes type;                              // EXT type: FILE_SYSTEM_TYPE_EXT2/FILE_SYSTEM_TYPE_EXT3/FILE_SYSTEM_TYPE_EXT4
  uint            blockSize;                         // block size [bytes]
  uint            groupDescriptorSize;               // group descriptor size [bytes]
  uint32          blocksPerGroup;                    // number of blocks in block group
  uint64          firstDataBlock;                    // first data block (0..n-1)
  uint64          totalBlocks;                       // total number of blocks
  uint64          *bitmapBlocks;                     // bitmap block numbers
  uint32          bitmapBlocksCount;                 // number of bitmap block numbers
  int             bitmapIndex;                       // index of currently read bitmap (0..n-1)
  uchar           bitmapData[EXT2_MAX_BLOCK_SIZE];   // bitmap block data
} EXTHandle;

// ext2/ext3 super block
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

  // only when revision level EXT2_DYNAMIC_REV is set
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

  // only when EXT2_COMPAT_PREALLOC is set
  uint8  preallocBlocks;
  uint8  preallocDirectoryBlocks;
  uint16 reservedGroupTableBlocks;

  // only when EXT3_FEATURE_COMPAT_HAS_JOURNAL/EXT4_FEATURE_COMPAT_HAS_JOURNAL is set
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

  // only when EXT2_DYNAMIC_REV & EXT4_FEATURE_INCOMPAT_64BIT is set
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
} ATTRIBUTE_PACKED EXTSuperBlock;

// ext2/ext3 group descriptor
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
} ATTRIBUTE_PACKED EXT23GroupDescriptor;

// ext4 group descriptor
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
} ATTRIBUTE_PACKED EXT4GroupDescriptor;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : LOW_HIGH_TO_UINT64
* Purpose: convert low/high 32bit value into 64bit value
* Input  : low, high - low/high 32bit value
* Output : -
* Return : 64bit valule
* Notes  : -
\***********************************************************************/

#define LOW_HIGH_TO_UINT64(low,high) ((((uint64)(high)) << 32) | (((uint64)(low)) << 0))

/***********************************************************************\
* Name   : EXT_BLOCK_TO_OFFSET
* Purpose: get byte offset for block number
* Input  : extHandle - ext-handle
*          block     - block number (0..n-1)
* Output : -
* Return : byte offset
* Notes  : -
\***********************************************************************/

#define EXT_BLOCK_TO_OFFSET(extHandle,block) ((block)*extHandle->blockSize)

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : EXT_init
* Purpose: initialize Ext handle
* Input  : deviceHandle - device handle
*          extHandle    - Ext handle variable
* Output : extHandle - Ext variable
* Return : file system type or FILE_SYSTEN_UNKNOWN;
* Notes  : -
\***********************************************************************/

LOCAL FileSystemTypes EXT_init(DeviceHandle *deviceHandle, EXTHandle *extHandle)
{
  EXTSuperBlock        extSuperBlock;
  EXT23GroupDescriptor ext23GroupDescriptor;
  EXT4GroupDescriptor  ext4GroupDescriptor;
  uint32               z;

  assert(deviceHandle != NULL);
  assert(extHandle != NULL);
  assert(sizeof(extSuperBlock) == 1024);
  assert(sizeof(ext23GroupDescriptor) == 32);
  assert(sizeof(ext4GroupDescriptor) == 64);

  // read first super-block
  if (Device_seek(deviceHandle,EXT2_FIRST_SUPER_BLOCK_OFFSET) != ERROR_NONE)
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }
  if (Device_read(deviceHandle,&extSuperBlock,sizeof(extSuperBlock),NULL) != ERROR_NONE)
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }

  // check if this a super block
  if ((uint16)(LE16_TO_HOST(extSuperBlock.magic)) != EXT2_SUPER_MAGIC)
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }

  // get block size
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
      return FILE_SYSTEM_TYPE_UNKNOWN;
      break;
  }

  // get ext type: ext2/3/4
#if 0
#warning debug only
fprintf(stderr,"%s, %d: revisionLevel = %d\n",__FILE__,__LINE__,LE32_TO_HOST(extSuperBlock.revisionLevel));
fprintf(stderr,"%s, %d: featureCompatible = 0x%x\n",__FILE__,__LINE__,LE32_TO_HOST(extSuperBlock.featureCompatible));
fprintf(stderr,"%s, %d: featureInCompatible = 0x%x\n",__FILE__,__LINE__,LE32_TO_HOST(extSuperBlock.featureInCompatible));
fprintf(stderr,"%s, %d: featureCompatible & ~EXT2_FEATURE_COMPAT_SUPP = 0x%x\n",__FILE__,__LINE__,LE32_TO_HOST(extSuperBlock.featureCompatible)& ~EXT2_FEATURE_COMPAT_SUPP);
fprintf(stderr,"%s, %d: featureCompatible & ~EXT3_FEATURE_COMPAT_SUPP = 0x%x\n",__FILE__,__LINE__,LE32_TO_HOST(extSuperBlock.featureCompatible)& ~EXT3_FEATURE_COMPAT_SUPP);
fprintf(stderr,"%s, %d: featureCompatible & ~EXT4_FEATURE_COMPAT_SUPP = 0x%x\n",__FILE__,__LINE__,LE32_TO_HOST(extSuperBlock.featureCompatible)& ~EXT4_FEATURE_COMPAT_SUPP);
fprintf(stderr,"%s, %d: featureInCompatible & ~EXT2_FEATURE_INCOMPAT_SUPP = 0x%x\n",__FILE__,__LINE__,LE32_TO_HOST(extSuperBlock.featureInCompatible)& ~EXT2_FEATURE_INCOMPAT_SUPP);
fprintf(stderr,"%s, %d: featureInCompatible & ~EXT3_FEATURE_INCOMPAT_SUPP = 0x%x\n",__FILE__,__LINE__,LE32_TO_HOST(extSuperBlock.featureInCompatible)& ~EXT3_FEATURE_INCOMPAT_SUPP);
fprintf(stderr,"%s, %d: featureInCompatible & ~EXT4_FEATURE_INCOMPAT_SUPP = 0x%x\n",__FILE__,__LINE__,LE32_TO_HOST(extSuperBlock.featureInCompatible)& ~EXT4_FEATURE_INCOMPAT_SUPP);
#endif /* 0 */
  if      (   ((LE32_TO_HOST(extSuperBlock.featureCompatible  ) & ~EXT2_FEATURE_COMPAT_SUPP  ) == 0)
           && ((LE32_TO_HOST(extSuperBlock.featureInCompatible) & ~EXT2_FEATURE_INCOMPAT_SUPP) == 0)
          )
  {
    // ext2
    extHandle->type = FILE_SYSTEM_TYPE_EXT2;
  }
  else if (   (LE32_TO_HOST(extSuperBlock.revisionLevel) == EXT2_REVISION_DYNAMIC)
           && ((LE32_TO_HOST(extSuperBlock.featureCompatible  ) & ~EXT3_FEATURE_COMPAT_SUPP  ) == 0)
           && ((LE32_TO_HOST(extSuperBlock.featureInCompatible) & ~EXT3_FEATURE_INCOMPAT_SUPP) == 0)
          )
  {
    // ext3
    extHandle->type = FILE_SYSTEM_TYPE_EXT3;
  }
  else if (   (LE32_TO_HOST(extSuperBlock.revisionLevel) == EXT2_REVISION_DYNAMIC)
           && ((LE32_TO_HOST(extSuperBlock.featureCompatible  ) & ~EXT4_FEATURE_COMPAT_SUPP  ) == 0)
           && ((LE32_TO_HOST(extSuperBlock.featureInCompatible) & ~EXT4_FEATURE_INCOMPAT_SUPP) == 0)
          )
  {
    // ext4
    extHandle->type = FILE_SYSTEM_TYPE_EXT4;
  }
  else
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }

  // get file system block info, init data
  switch (extHandle->type)
  {
    case FILE_SYSTEM_TYPE_EXT2:
    case FILE_SYSTEM_TYPE_EXT3:
      extHandle->groupDescriptorSize = sizeof(EXT23GroupDescriptor);
      extHandle->blocksPerGroup      = LE32_TO_HOST(extSuperBlock.blocksPerGroup);
      extHandle->firstDataBlock      = (uint64)LE32_TO_HOST(extSuperBlock.firstDataBlock);
      extHandle->totalBlocks         = (uint64)LE32_TO_HOST(extSuperBlock.blocksCount);
      break;
    case FILE_SYSTEM_TYPE_EXT4:
      extHandle->groupDescriptorSize = //((LE32_TO_HOST(extSuperBlock.featureCompatible  ) & EXT4_FEATURE_COMPAT_HAS_JOURNAL) != 0)
                                       ((LE32_TO_HOST(extSuperBlock.featureInCompatible) & EXT4_FEATURE_INCOMPAT_64BIT) != 0)
                                         ? (uint)LE16_TO_HOST(extSuperBlock.groupDescriptorSize)
                                         : sizeof(EXT23GroupDescriptor);
      extHandle->blocksPerGroup      = LE32_TO_HOST(extSuperBlock.blocksPerGroup);
      extHandle->firstDataBlock      = (uint64)LE32_TO_HOST(extSuperBlock.firstDataBlock);
      extHandle->totalBlocks         = LOW_HIGH_TO_UINT64(LE32_TO_HOST(extSuperBlock.blocksCount),
                                                          LE32_TO_HOST(extSuperBlock.blocksCountHigh)
                                                         );
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  extHandle->bitmapIndex = -1;

  // validate data
  if (   !((extHandle->groupDescriptorSize > 0) && (extHandle->groupDescriptorSize <= EXT4_MAX_GROUP_DESCRIPTOR_SIZE))
      || !(extHandle->blocksPerGroup > 0)
      || !(extHandle->totalBlocks > 0)
      || !(   ((extHandle->blockSize <= 1024) && (extHandle->firstDataBlock == 1))
           || ((extHandle->blockSize >  1024) && (extHandle->firstDataBlock == 0))
          )
     )
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }

  // read group descriptors and detect bitmap block numbers
  extHandle->bitmapBlocksCount = (extHandle->totalBlocks+extHandle->blocksPerGroup-1)/extHandle->blocksPerGroup;;
  extHandle->bitmapBlocks = (uint64*)malloc(extHandle->bitmapBlocksCount*sizeof(uint64));
  if (extHandle->bitmapBlocks == NULL)
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }
  for (z = 0; z < extHandle->bitmapBlocksCount; z++)
  {
    if (Device_seek(deviceHandle,EXT_BLOCK_TO_OFFSET(extHandle,extHandle->firstDataBlock+1)+(uint64)z*(uint64)extHandle->groupDescriptorSize) != ERROR_NONE)
    {
      free(extHandle->bitmapBlocks);
      return FILE_SYSTEM_TYPE_UNKNOWN;
    }
    switch (extHandle->type)
    {
      case FILE_SYSTEM_TYPE_EXT2:
      case FILE_SYSTEM_TYPE_EXT3:
        if (Device_read(deviceHandle,&ext23GroupDescriptor,sizeof(ext23GroupDescriptor),NULL) != ERROR_NONE)
        {
          free(extHandle->bitmapBlocks);
          return FILE_SYSTEM_TYPE_UNKNOWN;
        }
        extHandle->bitmapBlocks[z] = (uint64)LE32_TO_HOST(ext23GroupDescriptor.blockBitmap);
        break;
      case FILE_SYSTEM_TYPE_EXT4:
        if (Device_read(deviceHandle,&ext4GroupDescriptor,sizeof(ext4GroupDescriptor),NULL) != ERROR_NONE)
        {
          free(extHandle->bitmapBlocks);
          return FILE_SYSTEM_TYPE_UNKNOWN;
        }
        extHandle->bitmapBlocks[z] = LOW_HIGH_TO_UINT64(LE32_TO_HOST(ext4GroupDescriptor.blockBitmap),
                                                        LE32_TO_HOST(ext4GroupDescriptor.blockBitmapHigh)
                                                       );
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; /* not reached */
    }
  }

#if 0
#warning debug only
fprintf(stderr,"\n");
for (z = 0; z < extHandle->bitmapBlocksCount; z++)
{
fprintf(stderr,"%s,%d: z=%d block=%ld used=%d\n",__FILE__,__LINE__,z,extHandle->bitmapBlocks[z],EXT_blockIsUsed(deviceHandle,extHandle,extHandle->bitmapBlocks[z]));
}
#endif /* 0 */

  return extHandle->type;
}

/***********************************************************************\
* Name   : EXT_done
* Purpose: deinitialize Ext handle
* Input  : deviceHandle - device handle
*          extHandle    - Ext handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/
LOCAL void EXT_done(DeviceHandle *deviceHandle, EXTHandle *extHandle)
{
  assert(deviceHandle != NULL);
  assert(extHandle != NULL);
  assert(extHandle->bitmapBlocks != NULL);

  UNUSED_VARIABLE(deviceHandle);

  free(extHandle->bitmapBlocks);
}

/***********************************************************************\
* Name   : EXT_blockIsUsed
* Purpose: check if block is used
* Input  : deviceHandle - device handle
*          extHandle    - Ext handle
*          offset       - offset in image
* Output : -
* Return : TRUE iff block at offset is used, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool EXT_blockIsUsed(DeviceHandle *deviceHandle, EXTHandle *extHandle, uint64 offset)
{
  uint64 block;
  uint64 blockOffset;
  uint   bitmapIndex;
  uint   index;

  assert(deviceHandle != NULL);
  assert(extHandle != NULL);
  assert(extHandle->bitmapBlocks != NULL);

  // calculate block
  block = offset/extHandle->blockSize;

  if (block >= 1)
  {
//fprintf(stderr,"%s, %d: extHandle->firstDataBlock=%d extHandle->blockSize=%d\n",__FILE__,__LINE__,extHandle->firstDataBlock,extHandle->blockSize);
assert((extHandle->firstDataBlock ==1) || (extHandle->blockSize > 1024));
assert((extHandle->firstDataBlock ==0) || (extHandle->blockSize <= 1024));
    blockOffset = block-extHandle->firstDataBlock;

    // calculate used block bitmap index
    assert(extHandle->blocksPerGroup != 0);
    bitmapIndex = blockOffset/extHandle->blocksPerGroup;
    assert(bitmapIndex < extHandle->bitmapBlocksCount);

    // read correct used block bitmap if not already read
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
#if 0
#warning debug only
fprintf(stderr,"%s, %d: bitmapIndex=%d\n",__FILE__,__LINE__,bitmapIndex);
{
  long b;
  long b0,b1;
  bool f;
  uint i;
  for (b = bitmapIndex*extHandle->blocksPerGroup; b < bitmapIndex*extHandle->blocksPerGroup+extHandle->blockSize*8; b++)
  {
    i = (b)-bitmapIndex*extHandle->blocksPerGroup;
    f = ((extHandle->bitmapData[i/8] & (1 << i%8)) != 0);
    if (!f)
    {
      b0 = b;
      do
      {
        b++;
        i = (b)-bitmapIndex*extHandle->blocksPerGroup;
        f = ((extHandle->bitmapData[i/8] & (1 << i%8)) != 0);
      }
      while ((b < bitmapIndex*extHandle->blocksPerGroup+extHandle->blockSize*8) && !f);
      b1 = b-1;
      fprintf(stderr,"%s, %d: free=%d-%d\n",__FILE__,__LINE__,b0+1,b1+1);
    }
  }
}
#endif /* 0 */
    }

    // check if block is used
    assert(blockOffset >= bitmapIndex*extHandle->blocksPerGroup);
    index = blockOffset-bitmapIndex*extHandle->blocksPerGroup;
#if 0
#warning debug only
if ((extHandle->bitmapData[index/8] & (1 << index%8)) == 0)
{
int h = open("ext_freeblocks.txt",O_CREAT|O_WRONLY|O_APPEND,0664);
char s[256];
sprintf(s,"%"PRIu64" %d\n",offset,block);
write(h,s,strlen(s));
close(h);
}
#endif /* 0 */
    return ((extHandle->bitmapData[index/8] & (1 << index%8)) != 0);
  }
  else
  {
    // first block is always "used"
    return TRUE;
  }
}

#ifdef __cplusplus
  }
#endif

/* end of file */
