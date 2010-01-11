/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/filesystems_fat.c,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: Backup ARchiver FAT file systems plug in
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define CLUSTER_BITMAP_SIZE 4096

#define MAX_FAT_CACHE_SIZE 32

/***************************** Datatypes *******************************/

typedef struct
{
  enum
  {
    FAT12,
    FAT16,
    FAT32
  } type;
  uint   bytesPerSector;
  uint   sectorsPerCluster;
  uint   reservedSectors;
  uint   fatCount;
  uint32 totalSectorsCount;
  uint   maxRootEntries;
  uint   sectorsPerFAT;
  uint32 dataSectorsCount;
  uint32 clustersCount;

  uint   bitsPerFATEntry;
  uint32 firstDataSector;

//  int    fatSector;
//  uint   fatSectorsCount;

  int      clusterBaseIndex;
  byte     clusterBitmap[CLUSTER_BITMAP_SIZE/8];
} FATHandle;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/* read int8/int16/int32 from arbitary memory position */
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

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

LOCAL bool readClusterBitmap(DeviceHandle *deviceHandle, FATHandle *fatHandle, uint32 cluster)
{
  uint   fatSectorsCount;
  byte   *buffer;
  uint32 fatStartSector;
  uint32 clustersCount;
  uint   index;
  bool   clusterIsUsed;
uint x;

  /* calculate max. number of FAT sectors to read */
  fatSectorsCount = MIN(((CLUSTER_BITMAP_SIZE*fatHandle->bitsPerFATEntry)/8)/fatHandle->bytesPerSector,fatHandle->sectorsPerFAT);
  assert(fatSectorsCount > 0);

  /* allocate sectors buffer */
  buffer = (byte*)malloc(fatSectorsCount*fatHandle->bytesPerSector);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* read FAT sectors */
  fatStartSector = ((cluster*fatHandle->bitsPerFATEntry)/8)/fatHandle->bytesPerSector;
  if (Device_seek(deviceHandle,(uint32)(fatHandle->reservedSectors+fatStartSector)*(uint32)fatHandle->bytesPerSector) != ERROR_NONE)
  {
    return FALSE;
  }
  if (Device_read(deviceHandle,buffer,fatSectorsCount*(uint32)fatHandle->bytesPerSector,NULL) != ERROR_NONE)
  {
    return FALSE;
  }

  /* calculate cluster base index, number of clusters */
  assert(fatHandle->bitsPerFATEntry != 0);
  fatHandle->clusterBaseIndex = (fatStartSector*fatHandle->bytesPerSector*8)/fatHandle->bitsPerFATEntry;
  clustersCount = (fatSectorsCount*fatHandle->bytesPerSector*8)/fatHandle->bitsPerFATEntry;
  if ((fatHandle->clusterBaseIndex+clustersCount) > fatHandle->clustersCount) clustersCount = fatHandle->clustersCount-fatHandle->clusterBaseIndex;

  /* init cluster bitmap from FAT entries */
  memset(fatHandle->clusterBitmap,0,sizeof(fatHandle->clusterBitmap));
  switch (fatHandle->type)
  {
    case FAT12:
      for (index = 0; index < clustersCount; index++)
      {
x=FAT_READ_INT24(buffer,((index*12)/8) & ~0x1);
        if (index & 0x1)
        {
          clusterIsUsed = (((FAT_READ_INT24(buffer,((index*12)/8) & ~0x1) & 0xFFF000) >> 12) != 0x000);
        }
        else
        {
          clusterIsUsed = (((FAT_READ_INT24(buffer,((index*12)/8) & ~0x1) & 0x000FFF) >>  0) != 0x000);
        }
        if (clusterIsUsed)
        {
          BITSET_SET(fatHandle->clusterBitmap,index);
        }
      }
      break;
    case FAT16:
      for (index = 0; index < clustersCount; index++)
      {
        if (LE16_TO_HOST(*((uint16*)buffer+index)) != 0x0000) BITSET_SET(fatHandle->clusterBitmap,index);
      }
      break;
    case FAT32:
      for (index = 0; index < clustersCount; index++)
      {
        if (LE32_TO_HOST(*((uint32*)buffer+index)) != 0x00000000) BITSET_SET(fatHandle->clusterBitmap,index);
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  /* free resources */
  free(buffer);

  return TRUE;
}

LOCAL bool FAT_init(DeviceHandle *deviceHandle, FATHandle *fatHandle)
{
  char bootSector[512];

  assert(deviceHandle != NULL);
  assert(fatHandle != NULL);

  /* read boot sector */
  if (Device_seek(deviceHandle,0) != ERROR_NONE)
  {
    return FALSE;
  }
  if (Device_read(deviceHandle,&bootSector,512,NULL) != ERROR_NONE)
  {
    return FALSE;
  }

  /* check if valid boot sector */
  if ((uint16)FAT_READ_INT16(bootSector,0x1FE) != 0xAA55) return FALSE;

  /* int file system info */
  fatHandle->bytesPerSector    = FAT_READ_INT16(bootSector,0xB);
  fatHandle->sectorsPerCluster = FAT_READ_INT8(bootSector,0xD);
  fatHandle->reservedSectors   = FAT_READ_INT16(bootSector,0xE);
  fatHandle->fatCount          = FAT_READ_INT8(bootSector,0x10);
  fatHandle->totalSectorsCount = (uint32)((FAT_READ_INT16(bootSector,0x13) != 0)?FAT_READ_INT16(bootSector,0x13):FAT_READ_INT32(bootSector,0x20));
  fatHandle->maxRootEntries    = FAT_READ_INT16(bootSector,0x11);
  fatHandle->sectorsPerFAT     = (FAT_READ_INT16(bootSector,0x16) != 0)?FAT_READ_INT16(bootSector,0x16):FAT_READ_INT32(bootSector,0x24);
  fatHandle->bitsPerFATEntry   = 0;
  fatHandle->clusterBaseIndex  = -1;

  /* validate data */
  if (   !(fatHandle->bytesPerSector >= 512)
      || !((fatHandle->bytesPerSector % 512) == 0)
      || !(fatHandle->sectorsPerCluster > 0)
      || !(fatHandle->reservedSectors > 0)
      || !(fatHandle->fatCount > 0)
      || !(fatHandle->totalSectorsCount < 0x0FFFFFFF)
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
  fatHandle->dataSectorsCount = fatHandle->totalSectorsCount
                                -fatHandle->reservedSectors
                                -fatHandle->fatCount*fatHandle->sectorsPerFAT
                                -(fatHandle->maxRootEntries*32+fatHandle->bytesPerSector-1)/fatHandle->bytesPerSector
                                ;

  /* calculate total number of clusters, detect FAT type, get number of bits per FAT entry */
  assert(fatHandle->sectorsPerCluster != 0);
  fatHandle->clustersCount = fatHandle->dataSectorsCount/fatHandle->sectorsPerCluster;
  if      (fatHandle->clustersCount <  4085)
  {
    fatHandle->type            = FAT12;
    fatHandle->bitsPerFATEntry = 12;
  }
  else if (fatHandle->clustersCount < 65525)
  {
    fatHandle->type            = FAT16;
    fatHandle->bitsPerFATEntry = 16;
  }
  else
  {
    fatHandle->type            = FAT32;
    fatHandle->bitsPerFATEntry = 32;
  }

  /* calculate first data sector:
       reserved sectors
     + sectors for FATs (FAT12/FAT16 only, 0 for FAT32)
     + sectors for root directory entries (FAT12/FAT16 only, 0 for FAT32)
  */
  fatHandle->firstDataSector =  fatHandle->reservedSectors
                               +fatHandle->fatCount*fatHandle->sectorsPerFAT
                               +(fatHandle->maxRootEntries*32+fatHandle->bytesPerSector-1)/fatHandle->bytesPerSector
                               ;

  return TRUE;
}

LOCAL void FAT_done(DeviceHandle *deviceHandle, FATHandle *fatHandle)
{
  assert(deviceHandle != NULL);
  assert(fatHandle != NULL);

  UNUSED_VARIABLE(deviceHandle);
  UNUSED_VARIABLE(fatHandle);
}

LOCAL bool FAT_blockIsUsed(DeviceHandle *deviceHandle, FATHandle *fatHandle, uint64 offset)
{
  bool   blockIsUsed;
  uint32 sector;
  uint32 cluster;
  uint   index;

  assert(deviceHandle != NULL);
  assert(fatHandle != NULL);

  blockIsUsed = FALSE;

  /* calculate sector */
  assert(fatHandle->bytesPerSector != 0);
  sector = offset/fatHandle->bytesPerSector;

  if (sector >= fatHandle->firstDataSector)
  {
    /* calculate cluster of sector */
    assert(fatHandle->sectorsPerCluster != 0);
    cluster = (sector-fatHandle->firstDataSector)/fatHandle->sectorsPerCluster+2;

    /* read correct cluster bitmap if needed */
    if (   (fatHandle->clusterBaseIndex < 0)
        || (cluster < fatHandle->clusterBaseIndex)
        || (cluster >= fatHandle->clusterBaseIndex+CLUSTER_BITMAP_SIZE))
    {
      if (!readClusterBitmap(deviceHandle,fatHandle,cluster))
      {
        return FALSE;
      }
    }

    /* check if sector is used */
    assert((cluster >= fatHandle->clusterBaseIndex) && (cluster < fatHandle->clusterBaseIndex+CLUSTER_BITMAP_SIZE));
    index = cluster-fatHandle->clusterBaseIndex;
    blockIsUsed = BITSET_IS_SET(fatHandle->clusterBitmap,index);
  }
  else
  {
    blockIsUsed = TRUE;
  }

  return blockIsUsed;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
