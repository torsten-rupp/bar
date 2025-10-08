/***********************************************************************\
*
* Contents: FAT file systems
* Systems: all
*
\***********************************************************************/

#ifndef __FILESYSTEMS_FAT__
#define __FILESYSTEMS_FAT__

/****************************** Includes *******************************/

#include "common/global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define CLUSTER_BITMAP_SIZE 4096

/***************************** Datatypes *******************************/

typedef struct
{
  uint     bytesPerSector;
  uint     sectorsPerCluster;
  uint     reservedSectors;
  uint     fatCount;
  uint32_t totalSectorsCount;
  uint     maxRootEntries;
  uint     sectorsPerFAT;
  uint32_t dataSectorsCount;
  uint32_t clustersCount;

  uint   bitsPerFATEntry;
  uint32 firstDataSector;

  int    clusterBaseIndex;
  byte   clusterBitmap[CLUSTER_BITMAP_SIZE/8];
} FATHandle;

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

#endif /* __FILESYSTEMS_FAT__ */

/* end of file */
