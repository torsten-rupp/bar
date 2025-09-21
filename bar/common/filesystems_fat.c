/***********************************************************************\
*
* Contents: FAT file systems
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/

#include "common/global.h"
#include "common/filesystems.h"

#include "filesystems_fat.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define MAX_FAT_CACHE_SIZE 32

/* Cluster bitmap format:

   depending on the FAT type the entry in the cluster bitmap are
     - 12 bit (2x 12bit in 3 bytes, little endian)
     - 16 bit (little endian)
     - 32 bit (little endian)

   cluster entry 0 and 1 are reservered

   +-----------------------+
   | 0 | 1 | 2 | ... | n-1 |
   +-----------------------+
             ^ first cluster of data sectors
     ^^^^^ reserved
*/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

// read int8/int16/int32 from arbitary memory position
#define FAT_READ_INT8(data,offset)  (*((uint8*)(data)+offset))
#define FAT_READ_INT16(data,offset) (LE16_TO_HOST(  (((uint16)(*((uint8*)(data)+(offset)+0)))<< 0) \
                                                  | (((uint16)(*((uint8*)(data)+(offset)+1)))<< 8) \
                                                 ) & 0xFFFF \
                                    )
#define FAT_READ_INT24(data,offset) LE32_TO_HOST(  (((uint32)(*((uint8*)(data)+(offset)+0)))<< 0) \
                                                 | (((uint32)(*((uint8*)(data)+(offset)+1)))<< 8) \
                                                 | (((uint32)(*((uint8*)(data)+(offset)+2)))<<16) \
                                                )
#define FAT_READ_INT32(data,offset) LE32_TO_HOST(  (((uint32)(*((uint8*)(data)+(offset)+0)))<< 0) \
                                                 | (((uint32)(*((uint8*)(data)+(offset)+1)))<< 8) \
                                                 | (((uint32)(*((uint8*)(data)+(offset)+2)))<<16) \
                                                 | (((uint32)(*((uint8*)(data)+(offset)+3)))<<24) \
                                                )

// convert from little endian to host system format
#if __BYTE_ORDER == __LITTLE_ENDIAN
  #define LE16_TO_HOST(x) (x)
  #define LE32_TO_HOST(x) (x)
#else /* not __BYTE_ORDER == __LITTLE_ENDIAN */
  #define LE16_TO_HOST(x) swapBytes16(x)
  #define LE32_TO_HOST(x) swapBytes32(x)
#endif /* __BYTE_ORDER == __LITTLE_ENDIAN */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : readClusterBitmap
* Purpose: read cluster bitmap
* Input  : deviceHandle   - device handle
*          fileSystemType - file system type
*          fatHandle      - FAT handle
* Output : -
* Return : TRUE iff cluster bitmap is read, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool readClusterBitmap(DeviceHandle *deviceHandle, FileSystemTypes fileSystemType, FATHandle *fatHandle, uint32 cluster)
{
  assert(deviceHandle != NULL);
  assert(   (fileSystemType == FILE_SYSTEM_TYPE_FAT12)
         || (fileSystemType == FILE_SYSTEM_TYPE_FAT16)
         || (fileSystemType == FILE_SYSTEM_TYPE_FAT32)
        );
  assert(fatHandle != NULL);

  // calculate max. number of sectors to read for cluster bitmap
  uint fatSectorsCount = MIN(((CLUSTER_BITMAP_SIZE*fatHandle->bitsPerFATEntry)/8)/fatHandle->bytesPerSector,
                             fatHandle->sectorsPerFAT
                            );
  assert(fatSectorsCount > 0);

  // allocate sectors buffer
  byte *buffer = (byte*)malloc(fatSectorsCount*fatHandle->bytesPerSector);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // read sectors of cluster bitmap from FAT1
  uint32 fatStartSector = ((cluster*fatHandle->bitsPerFATEntry)/8)/fatHandle->bytesPerSector;
  if (Device_seek(deviceHandle,(uint32)(fatHandle->reservedSectors+fatStartSector)*(uint32)fatHandle->bytesPerSector) != ERROR_NONE)
  {
    return FALSE;
  }
  if (Device_read(deviceHandle,buffer,fatSectorsCount*(uint32)fatHandle->bytesPerSector,NULL) != ERROR_NONE)
  {
    return FALSE;
  }

  // calculate cluster base index, number of clusters
  assert(fatHandle->bitsPerFATEntry != 0);
  fatHandle->clusterBaseIndex = (fatStartSector*fatHandle->bytesPerSector*8)/fatHandle->bitsPerFATEntry;
  uint32 clustersCount = (fatSectorsCount*fatHandle->bytesPerSector*8)/fatHandle->bitsPerFATEntry;
  if ((fatHandle->clusterBaseIndex+clustersCount) > fatHandle->clustersCount)
  {
    clustersCount = fatHandle->clustersCount-fatHandle->clusterBaseIndex;
  }
  assert(clustersCount <= CLUSTER_BITMAP_SIZE);

  // init cluster bitmap from FAT entries
  memClear(fatHandle->clusterBitmap,sizeof(fatHandle->clusterBitmap));
  switch (fileSystemType)
  {
    case FILE_SYSTEM_TYPE_FAT12:
      for (uint index = 0; index < clustersCount; index++)
      {
        bool clusterIsFree;
        if (index & 0x1)
        {
          clusterIsFree = (((FAT_READ_INT24(buffer,((index*12)/8) & ~0x1) & 0xFFF000) >> 12) == 0x000);
        }
        else
        {
          clusterIsFree = (((FAT_READ_INT24(buffer,((index*12)/8) & ~0x1) & 0x000FFF) >>  0) == 0x000);
        }
        if (!clusterIsFree)
        {
          BITSET_SET(fatHandle->clusterBitmap,index);
        }
      }
      break;
    case FILE_SYSTEM_TYPE_FAT16:
      for (uint index = 0; index < clustersCount; index++)
      {
        bool clusterIsFree;
        clusterIsFree = (LE16_TO_HOST(((uint16*)buffer)[index]) == 0x0000);
        if (!clusterIsFree)
        {
//fprintf(stderr,"%s, %d: %d - %d\n",__FILE__,__LINE__,index,LE16_TO_HOST(((uint16*)buffer)[index]));
          BITSET_SET(fatHandle->clusterBitmap,index);
        }
      }
      break;
    case FILE_SYSTEM_TYPE_FAT32:
      for (uint index = 0; index < clustersCount; index++)
      {
        bool clusterIsFree;
        clusterIsFree = (LE32_TO_HOST(((uint32*)buffer)[index]) == 0x00000000);
        if (!clusterIsFree)
        {
          BITSET_SET(fatHandle->clusterBitmap,index);
        }
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  // free resources
  free(buffer);

#if 0
{
  int i;
  int i0,i1;

  i = (fatStartSector > 0) ? 0 : 2;
  while (i < clustersCount)
  {
    while ((i < clustersCount) && !BITSET_IS_SET(fatHandle->clusterBitmap,i))
    {
      i++;
    }
    i0 = i;
    while ((i < clustersCount) && BITSET_IS_SET(fatHandle->clusterBitmap,i))
    {
      i++;
    }
    i1 = i;

    if (i1 > i0)
    {
      fprintf(stderr,"%s, %d: used %d - %d\n",__FILE__,__LINE__,
      fatHandle->firstDataSector+(fatHandle->clusterBaseIndex+i0-2)*fatHandle->sectorsPerCluster,
      fatHandle->firstDataSector+(fatHandle->clusterBaseIndex+i1-2)*fatHandle->sectorsPerCluster-1
      );
    }
  }
}
#endif /* 0 */

  return TRUE;
}

/***********************************************************************\
* Name   : FAT_getType
* Purpose: get FAT file system type
* Input  : deviceHandle - device handle
* Output : -
* Return : file system type or FILE_SYSTEN_UNKNOWN;
* Notes  : -
\***********************************************************************/

LOCAL FileSystemTypes FAT_getType(DeviceHandle *deviceHandle)
{
  assert(deviceHandle != NULL);

  // read boot sector
  uint8_t bootSector[512];
  if (Device_seek(deviceHandle,0) != ERROR_NONE)
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }
  if (Device_read(deviceHandle,bootSector,512,NULL) != ERROR_NONE)
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }

  // check if valid boot sector
  if ((uint16)FAT_READ_INT16(bootSector,0x1FE) != 0xAA55)
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }

  // int file system info
  int16  bytesPerSector    = FAT_READ_INT16(bootSector,0x0B);
  int8   sectorsPerCluster = FAT_READ_INT8 (bootSector,0x0D);
  int16  reservedSectors   = FAT_READ_INT16(bootSector,0x0E);
  int8   fatCount          = FAT_READ_INT8 (bootSector,0x10);
  uint32 totalSectorsCount = (FAT_READ_INT16(bootSector,0x13) != 0)
                               ? (uint32)FAT_READ_INT16(bootSector,0x13)
                               : (uint32)FAT_READ_INT32(bootSector,0x20);
  int16  maxRootEntries    = FAT_READ_INT16(bootSector,0x11);
  int16  sectorsPerFAT     = (FAT_READ_INT16(bootSector,0x16) != 0)?FAT_READ_INT16(bootSector,0x16):FAT_READ_INT32(bootSector,0x24);
  assert(sectorsPerCluster != 0);

  // validate data
  if (   !(bytesPerSector >= 512)
      || !((bytesPerSector % 512) == 0)
      || !(sectorsPerCluster > 0)
      || !(reservedSectors > 0)
      || !(fatCount > 0)
      || !(totalSectorsCount < 0x0FFFFFFF)
     )
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }

  /* calculate number of data sectors
     total sectors
     - reserved sectors
     - sectors for FATs (FAT12/FAT16 only, 0 for FAT32)
     - sectors for root directory entries
  */
  uint32 dataSectorsCount = totalSectorsCount
                            -reservedSectors
                            -fatCount*sectorsPerFAT
                            -(maxRootEntries*32+bytesPerSector-1)/bytesPerSector
                            ;
//fprintf(stderr,"%s, %d: fatHandle->totalSectorsCount=%d \n",__FILE__,__LINE__,fatHandle->totalSectorsCount);
//fprintf(stderr,"%s, %d: fatHandle->reservedSectors=%d \n",__FILE__,__LINE__,fatHandle->reservedSectors);
//fprintf(stderr,"%s, %d: fatHandle->fatCount*fatHandle->sectorsPerFAT=%d \n",__FILE__,__LINE__,fatHandle->fatCount*fatHandle->sectorsPerFAT);
//fprintf(stderr,"%s, %d: fatHandle->maxRootEntries=%d \n",__FILE__,__LINE__,fatHandle->maxRootEntries);
//fprintf(stderr,"%s, %d: (fatHandle->maxRootEntries*32+fatHandle->bytesPerSector-1)/fatHandle->bytesPerSector=%d \n",__FILE__,__LINE__,(fatHandle->maxRootEntries*32+fatHandle->bytesPerSector-1)/fatHandle->bytesPerSector);
//fprintf(stderr,"%s, %d: fatHandle->dataSectorsCount=%d \n",__FILE__,__LINE__,fatHandle->dataSectorsCount);

  // detect FAT type
  FileSystemTypes fileSystemType;
  uint32_t clustersCount = 2+dataSectorsCount/sectorsPerCluster;
  if      (clustersCount <  4087)
  {
    fileSystemType = FILE_SYSTEM_TYPE_FAT12;
  }
  else if (clustersCount < 65527)
  {
    fileSystemType = FILE_SYSTEM_TYPE_FAT16;
  }
  else
  {
    fileSystemType = FILE_SYSTEM_TYPE_FAT32;
  }

  return fileSystemType;
}

/***********************************************************************\
* Name   : FAT_init
* Purpose: initialize FAT handle
* Input  : deviceHandle - device handle
* Output : fileSystemType - file system type
*          fatHandle      - FAT handle (can be NULL)
* Output : -
* Return : TRUE iff file system initialized
* Notes  : -
\***********************************************************************/

LOCAL bool FAT_init(DeviceHandle *deviceHandle, FileSystemTypes *fileSystemType, FATHandle *fatHandle)
{
  assert(deviceHandle != NULL);
  assert(fileSystemType != NULL);

  // read boot sector
  uint8_t bootSector[512];
  if (Device_seek(deviceHandle,0) != ERROR_NONE)
  {
    return FALSE;
  }
  if (Device_read(deviceHandle,bootSector,512,NULL) != ERROR_NONE)
  {
    return FALSE;
  }

  // check if valid boot sector
  if ((uint16)FAT_READ_INT16(bootSector,0x1FE) != 0xAA55)
  {
    return FALSE;
  }

  // get file system info
  int16  bytesPerSector    = FAT_READ_INT16(bootSector,0x0B);
  int8   sectorsPerCluster = FAT_READ_INT8 (bootSector,0x0D);
  int16  reservedSectors   = FAT_READ_INT16(bootSector,0x0E);
  int8   fatCount          = FAT_READ_INT8 (bootSector,0x10);
  uint32 totalSectorsCount = (FAT_READ_INT16(bootSector,0x13) != 0)
                               ? (uint32)FAT_READ_INT16(bootSector,0x13)
                               : (uint32)FAT_READ_INT32(bootSector,0x20);
  int16  maxRootEntries    = FAT_READ_INT16(bootSector,0x11);
  int16  sectorsPerFAT     = (FAT_READ_INT16(bootSector,0x16) != 0)
                               ? FAT_READ_INT16(bootSector,0x16)
                               : FAT_READ_INT32(bootSector,0x24);

  // validate data
  if (   !(bytesPerSector >= 512)
      || !((bytesPerSector % 512) == 0)
      || !(sectorsPerCluster > 0)
      || !(reservedSectors > 0)
      || !(fatCount > 0)
      || !(totalSectorsCount < 0x0FFFFFFF)
     )
  {
    return FALSE;
  }

  /* calculate number of data sectors
     total sectors
     - reserved sectors
     - sectors for FATs (FAT12/FAT16 only, 0 for FAT32)
     - sectors for root directory entries
  */
  assert(bytesPerSector > 0);
  uint32 dataSectorsCount = totalSectorsCount
                            -(uint32)reservedSectors
                            -(uint32)fatCount*(uint32)sectorsPerFAT
                            -((uint32)maxRootEntries*32+(uint32)bytesPerSector-1)/(uint32)bytesPerSector
                            ;
//fprintf(stderr,"%s, %d: totalSectorsCount=%d \n",__FILE__,__LINE__,totalSectorsCount);
//fprintf(stderr,"%s, %d: reservedSectors=%d \n",__FILE__,__LINE__,reservedSectors);
//fprintf(stderr,"%s, %d: fatCount*fatHandle->sectorsPerFAT=%d \n",__FILE__,__LINE__,fatCount*sectorsPerFAT);
//fprintf(stderr,"%s, %d: maxRootEntries=%d \n",__FILE__,__LINE__,fatHandle->maxRootEntries);
//fprintf(stderr,"%s, %d: (maxRootEntries*32+fatHandle->bytesPerSector-1)/fatHandle->bytesPerSector=%d \n",__FILE__,__LINE__,(maxRootEntries*32+bytesPerSector-1)/bytesPerSector);
//fprintf(stderr,"%s, %d: dataSectorsCount=%d \n",__FILE__,__LINE__,dataSectorsCount);

  // calculate total number of clusters, detect FAT type, get number of bits per FAT entry
  assert(sectorsPerCluster != 0);
  uint32 clustersCount = 2+dataSectorsCount/(uint32)sectorsPerCluster;

  if      (clustersCount <  4087)
  {
    (*fileSystemType) = FILE_SYSTEM_TYPE_FAT12;
  }
  else if (clustersCount < 65527)
  {
    (*fileSystemType) = FILE_SYSTEM_TYPE_FAT16;
  }
  else
  {
    (*fileSystemType) = FILE_SYSTEM_TYPE_FAT32;
  }

  if (fatHandle != NULL)
  {
    // int FAT file system info
    fatHandle->bytesPerSector    = bytesPerSector;
    fatHandle->sectorsPerCluster = sectorsPerCluster;
    fatHandle->reservedSectors   = reservedSectors;
    fatHandle->fatCount          = fatCount;
    fatHandle->totalSectorsCount = totalSectorsCount;
    fatHandle->maxRootEntries    = maxRootEntries;
    fatHandle->sectorsPerFAT     = sectorsPerFAT;
    fatHandle->dataSectorsCount  = dataSectorsCount;
    fatHandle->clustersCount     = clustersCount;
    switch (*fileSystemType)
    {
      case FILE_SYSTEM_TYPE_FAT12: fatHandle->bitsPerFATEntry = 12; break;
      case FILE_SYSTEM_TYPE_FAT16: fatHandle->bitsPerFATEntry = 16; break;
      case FILE_SYSTEM_TYPE_FAT32: fatHandle->bitsPerFATEntry = 32; break;
      #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif
    }

    /* calculate first data sector:
         reserved sectors
       + sectors for FATs (FAT12/FAT16 only, 0 for FAT32)
       + sectors for root directory entries (FAT12/FAT16 only, 0 for FAT32)
    */
    assert(bytesPerSector > 0);
    fatHandle->firstDataSector   =  (uint32)reservedSectors
                                   +(uint32)fatCount*(uint32)sectorsPerFAT
                                   +((uint32)maxRootEntries*32+(uint32)bytesPerSector-1)/(uint32)bytesPerSector
                                   ;
    fatHandle->clusterBaseIndex  = -1;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : FAT_done
* Purpose: deinitialize FAT handle
* Input  : fatHandle - FAT handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void FAT_done(FATHandle *fatHandle)
{
  assert(fatHandle != NULL);

  UNUSED_VARIABLE(fatHandle);
}

/***********************************************************************\
* Name   : FAT_blockIsUsed
* Purpose: check if block is used
* Input  : deviceHandle   - device handle
*          fileSystemType - file system type
*          fatHandle      - FAT handle
*          offset         - offset in image
* Output : -
* Return : TRUE iff block at offset is used, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool FAT_blockIsUsed(DeviceHandle *deviceHandle, FileSystemTypes fileSystemType, FATHandle *fatHandle, uint64 offset)
{
  assert(deviceHandle != NULL);
  assert(   (fileSystemType == FILE_SYSTEM_TYPE_FAT12)
         || (fileSystemType == FILE_SYSTEM_TYPE_FAT16)
         || (fileSystemType == FILE_SYSTEM_TYPE_FAT32)
        );
  assert(fatHandle != NULL);

  bool blockIsUsed = FALSE;

  // calculate sector
  assert(fatHandle->bytesPerSector != 0);
  uint32 sector = offset/fatHandle->bytesPerSector;
//fprintf(stderr,"%s, %d: %d %d\n",__FILE__,__LINE__,sector,fatHandle->firstDataSector);

  if (sector >= fatHandle->firstDataSector)
  {
    // calculate cluster of sector (Note: FAT entries 0+1 are reserved; first cluster of data sectors start at offset 2)
    assert(fatHandle->sectorsPerCluster != 0);
    uint32 cluster = 2+(sector-fatHandle->firstDataSector)/fatHandle->sectorsPerCluster;

    // read correct cluster bitmap if needed
    if (   (fatHandle->clusterBaseIndex < 0)
        || (cluster < (uint32)fatHandle->clusterBaseIndex)
        || (cluster >= (uint32)fatHandle->clusterBaseIndex+CLUSTER_BITMAP_SIZE))
    {
      if (!readClusterBitmap(deviceHandle,fileSystemType,fatHandle,cluster))
      {
        return FALSE;
      }
    }

    // check if sector is used
    assert((cluster >= (uint32)fatHandle->clusterBaseIndex) && (cluster < (uint32)fatHandle->clusterBaseIndex+CLUSTER_BITMAP_SIZE));
    assert(cluster < fatHandle->clusterBaseIndex+2+fatHandle->clustersCount);
    uint index = cluster-fatHandle->clusterBaseIndex;
    blockIsUsed = BITSET_IS_SET(fatHandle->clusterBitmap,index);

#if 0
if (blockIsUsed)
{
  fprintf(stderr,"%s, %d: used offset=%"PRIu64" sector=%"PRIu64" cluster=%d\n",__FILE__,__LINE__,offset,offset/fatHandle->bytesPerSector,cluster);
}
else
{
//  fprintf(stderr,"%s, %d: free offset=%"PRIu64" sector=%d cluster=%d \n",__FILE__,__LINE__,offset,offset/fatHandle->bytesPerSector,cluster);
}
#endif
  }
  else
  {
    blockIsUsed = TRUE;
#if 0
if (blockIsUsed)
{
  fprintf(stderr,"%s, %d: used offset=%"PRIu64" sector=%"PRIu64"\n",__FILE__,__LINE__,offset,offset/fatHandle->bytesPerSector);
}
else
{
//  fprintf(stderr,"%s, %d: free offset=%"PRIu64" sector=%d cluster=%d \n",__FILE__,__LINE__,offset,offset/fatHandle->bytesPerSector,cluster);
}
#endif
  }

  return blockIsUsed;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
