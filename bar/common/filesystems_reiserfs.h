/***********************************************************************\
*
* Contents: ReiserFS file system
* Systems: all
*
\***********************************************************************/

#ifndef __FILESYSTEMS_REISERFS__
#define __FILESYSTEMS_REISERFS__

/****************************** Includes *******************************/

#include "common/global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define REISERFS_MAX_BLOCK_SIZE 8192

/***************************** Datatypes *******************************/
typedef struct
{
  uint     blockSize;                             // block size (1024, 2048, 4096, 8192)
  ulong    firstDataBlock;                        // first data block
  uint32_t totalBlocks;                           // total number of blocks
  int      bitmapIndex;                           // index of currently read bitmap
  uchar    bitmapData[REISERFS_MAX_BLOCK_SIZE];   // bitmap block data
} ReiserFSHandle;

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

#endif /* __FILESYSTEMS_REISERFS__ */

/* end of file */
