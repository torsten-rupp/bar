/***********************************************************************\
*
* Contents: EXT2/3/4 file system
* Systems: all
*
\***********************************************************************/

#ifndef __FILESYSTEMS_EXT__
#define __FILESYSTEMS_EXT__

/****************************** Includes *******************************/

#include "common/global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define EXT2_MAX_BLOCK_SIZE (64*1024)

/***************************** Datatypes *******************************/
typedef struct
{
  uint   blockSize;                         // block size [bytes]
  uint   groupDescriptorSize;               // group descriptor size [bytes]
  uint32_t blocksPerGroup;                    // number of blocks in block group
  uint64 firstDataBlock;                    // first data block (0..n-1)
  uint64 totalBlocks;                       // total number of blocks
  uint64 *bitmapBlocks;                     // bitmap block numbers
  uint32 bitmapBlocksCount;                 // number of bitmap block numbers
  int    bitmapIndex;                       // index of currently read bitmap (0..n-1)
  uchar  bitmapData[EXT2_MAX_BLOCK_SIZE];   // bitmap block data
} EXTHandle;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifdef __cplusplus
  }
#endif

#endif /* __FILESYSTEMS_EXT__ */

/* end of file */
