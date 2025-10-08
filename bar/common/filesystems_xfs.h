/***********************************************************************\
*
* Contents: XFS file system
* Systems: all
*
\***********************************************************************/

#ifndef __FILESYSTEMS_XFS__
#define __FILESYSTEMS_XFS__

/****************************** Includes *******************************/

#include "common/global.h"
#include "common/bitsets.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
//#define XFS_MAX_BLOCK_SIZE (64*1024)

/***************************** Datatypes *******************************/
typedef struct
{
  uint     blockSize;                         // block size [bytes]
  uint64_t totalBlocks;                       // total number of blocks

  BitSet   blocksBitSet;                      // bit set with used blocks
} XFSHandle;

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

#endif /* __FILESYSTEMS_XFS__ */

/* end of file */
