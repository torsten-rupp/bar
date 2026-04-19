/***********************************************************************\
*
* Contents: EXFAT file systems
* Systems: all
*
\***********************************************************************/

#ifndef __FILESYSTEMS_EXFAT__
#define __FILESYSTEMS_EXFAT__

/****************************** Includes *******************************/

#include "common/global.h"
#include "common/bitsets.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef struct
{
  uint16_t bytesPerSector;
  uint16_t sectorsPerCluster;
  uint64_t totalSectorsCount;
  uint8_t  fatCount;
  uint32_t clusterHeapOffset;
  uint32_t clusterCount;
  uint32_t rootDirectoryCluster;

  BitSet   clusterBitmap;
} EXFATHandle;

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

#endif /* __FILESYSTEMS_EXFAT__ */

/* end of file */
