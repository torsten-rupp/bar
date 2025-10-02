/***********************************************************************\
*
* Contents: EXFAT file systems
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/

#include <stdlib.h>
#include <stdint.h>

#include "common/global.h"
#include "common/filesystems.h"

#include "filesystems_exfat.h"

/* links
https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification
https://elm-chan.org/docs/exfat_e.html
*/

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define EXFAT_MAGIC 0xAA55

#define ENTRY_TYPE_NONE                                   0x00
#define ENTRY_TYPE_BITMAP                                 0x01
#define ENTRY_TYPE_UPCASE                                 0x02
#define ENTRY_TYPE_VOLUME_LABEL                           0x04
#define ENTRY_TYPE_FILE_DIRECTORY                         0x05
#define ENTRY_TYPE_STREAM_EXTENSION                       0x40
#define ENTRY_TYPE_FILE_NAME                              0x41
#define ENTRY_TYPE_WINDOWS_CE_ACCESS_CONTROL_LIST         0x42
#define ENTRY_TYPE_VOLUME_GUID                            0x20
#define ENTRY_TYPE_TEX_FAT_PADDING                        0xA1
#define ENTRY_TYPE_WINDWOS_CE_ACCESS_CONTROL_TABLE        0xA2

#define ENTRY_TYPE_MASK                                   0x1F
#define ENTRY_TYPE_IN_USE                                 0x80
#define ENTRY_CATEGORY_SECONDARY                          0x40
#define ENTRY_IMPORTANCE_BENIGN                           0x20

#define ENTRY_FILE_ATTRIBUTE_READ_ONLY                    0x01
#define ENTRY_FILE_ATTRIBUTE_HIDDEN                       0x02
#define ENTRY_FILE_ATTRIBUTE_SYSTEM                       0x04
#define ENTRY_FILE_ATTRIBUTE_DIRECTORY                    0x10
#define ENTRY_FILE_ATTRIBUTE_ARCHIVE                      0x20

#define ENTRY_GENERAL_SECONDARY_FLAGS_ALLOCATION_POSSIBLE 0x01
#define ENTRY_GENERAL_SECONDARY_FLAGS_NO_FAT_CHAIN        0x02

#define CLUSTER_BASE_INDEX                                2

/***************************** Datatypes *******************************/
// EXTFAT super block
typedef struct
{
  uint8_t  jumpCode[3];
  uint8_t  oemName[8];
  uint8_t  reserved1[53];
  uint64_t partitionOffset;
  uint64_t partitionLength;                  // number of total sectors
  uint32_t fatOffset;
  uint32_t fatLength;
  uint32_t clusterHeapOffset;                // [sector]
  uint32_t clusterCount;
  uint32_t rootDirectoryCluster;             // [cluster]
  uint32_t volumeSerial;
  struct
  {
    uint8_t minor;
    uint8_t major;
  }      fileSystemReversion;
  uint16_t volumeFlags;
  uint8_t  bytesPerSectorShift;
  uint8_t  sectorsPerClusterShift;
  uint8_t  fatCount;
  uint8_t  driveSelect;
  uint8_t  percentInUse;
  uint8_t  reserved2[7];
  uint8_t  bootCode[390];
  uint8_t  bootSignature[2];
  uint8_t  excessSpace[512];
} ATTRIBUTE_PACKED EXFATBootSector;
static_assert(sizeof(EXFATBootSector) == 1024);

// EXTFAT entry
typedef struct EXFATEntry
{
  uint8_t type;
  union
  {
    struct
    {
      uint8_t  flags;
      uint8_t  reserved[18];
      uint32_t startCluster;
      uint64_t size;
    } ATTRIBUTE_PACKED bitmap;
    struct
    {
      uint8_t  reserved1[3];
      uint32_t tableCheckSum;
      uint8_t  reserved2[12];
      uint32_t startCluster;
      uint64_t size;
    } ATTRIBUTE_PACKED upcase;
    struct
    {
      uint8_t  characterCount;
      uint16_t name[11];  // UTF16-LE
      uint8_t  reserved[8];
    } ATTRIBUTE_PACKED volumeLabel;
    struct
    {
      uint8_t  secondaryCount;
      uint16_t setCheckSum;
      uint16_t fileAttribute;
      uint8_t  reserved1[2];
      uint32_t createTimeStamp;
      uint32_t lastModifiedTimeStamp;
      uint32_t lastAccessedTimeStamp;
      uint8_t  create10msIncrement;
      uint8_t  lastModified10msIncrement;
      uint8_t  createTZOffset;
      uint8_t  lastModifiedTZOffset;
      uint8_t  lastAccessedTZOffset;
      uint8_t  reserved2[7];
    } ATTRIBUTE_PACKED fileDirectory;
    struct
    {
      uint8_t  generalSecondaryFlags;
      uint8_t  reserved1[1];
      uint16_t nameHash;
      uint8_t  reserved2[2];
      uint64_t validDataLength;
      uint8_t  reserved3[4];
      uint32_t startCluster;
      uint64_t size;
    } ATTRIBUTE_PACKED streamExtension;
    struct
    {
      uint8_t  generalSecondaryFlags;
      uint16_t fileName[15];  // UTF16-LE
    } ATTRIBUTE_PACKED fileName;
    struct
    {
    } ATTRIBUTE_PACKED windowsCEAccessControlList;
    struct
    {
    } ATTRIBUTE_PACKED volumeGUID;
    struct
    {
    } ATTRIBUTE_PACKED texFatPadding;
    struct
    {
    } ATTRIBUTE_PACKED windowsCEAccessControlTable;
  };
} ATTRIBUTE_PACKED EXFATEntry;
static_assert(sizeof(EXFATEntry) == 32);

/***************************** Variables *******************************/

/****************************** Macros *********************************/

// read int8/int16/int32 from arbitary memory position
#define EXFAT_READ_UINT8(data)  (*((uint8*)(&data)))
#define EXFAT_READ_UINT16(data) (LE16_TO_HOST(  (((uint16_t)(*((uint8_t*)(&data)+0))) << 0) \
                                              | (((uint16_t)(*((uint8_t*)(&data)+1))) << 8) \
                                             ) & 0xFFFF \
                               )
#define EXFAT_READ_UINT24(data) LE32_TO_HOST(  (((uint32_t)(*((uint8_t*)(&data)+0))) <<  0) \
                                             | (((uint32_t)(*((uint8_t*)(&data)+1))) <<  8) \
                                             | (((uint32_t)(*((uint8_t*)(&data)+2))) << 16) \
                                            )
#define EXFAT_READ_UINT32(data) LE32_TO_HOST(  (((uint32_t)(*((uint8_t*)(&data)+0))) <<  0) \
                                             | (((uint32_t)(*((uint8_t*)(&data)+1))) <<  8) \
                                             | (((uint32_t)(*((uint8_t*)(&data)+2))) << 16) \
                                             | (((uint32_t)(*((uint8_t*)(&data)+3))) << 24) \
                                            )
#define EXFAT_READ_UINT64(data) LE64_TO_HOST(  (((uint64_t)(*((uint8_t*)(&data)+0))) <<  0) \
                                             | (((uint64_t)(*((uint8_t*)(&data)+1))) <<  8) \
                                             | (((uint64_t)(*((uint8_t*)(&data)+2))) << 16) \
                                             | (((uint64_t)(*((uint8_t*)(&data)+3))) << 24) \
                                             | (((uint64_t)(*((uint8_t*)(&data)+4))) << 32) \
                                             | (((uint64_t)(*((uint8_t*)(&data)+5))) << 40) \
                                             | (((uint64_t)(*((uint8_t*)(&data)+6))) << 48) \
                                             | (((uint64_t)(*((uint8_t*)(&data)+7))) << 56) \
                                            )

// convert from little endian to host system format
#if __BYTE_ORDER == __LITTLE_ENDIAN
  #define LE16_TO_HOST(x) (x)
  #define LE32_TO_HOST(x) (x)
  #define LE64_TO_HOST(x) (x)
#else /* not __BYTE_ORDER == __LITTLE_ENDIAN */
  #define LE16_TO_HOST(x) swapBytes16(x)
  #define LE32_TO_HOST(x) swapBytes32(x)
  #define LE64_TO_HOST(x) swapBytes64(x)
#endif /* __BYTE_ORDER == __LITTLE_ENDIAN */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : sectorToOffset
* Purpose: convert sector index to byte offset
* Input  : exfatHandle - EXFAT handle
*          sector      - sector index
* Output : -
* Return : bytes offset
* Notes  : -
\***********************************************************************/

LOCAL_INLINE uint64_t sectorToOffset(const EXFATHandle *exfatHandle, uint32_t sector)
{
  assert(exfatHandle != NULL);

  return sector*exfatHandle->bytesPerSector;
}

/***********************************************************************\
* Name   : offsetToSector
* Purpose: convert byte offset to sector index
* Input  : exfatHandle - EXFAT handle
*          offset      - byte offset
* Output : -
* Return : sector index
* Notes  : -
\***********************************************************************/

LOCAL_INLINE uint64_t offsetToSector(const EXFATHandle *exfatHandle, uint64_t offset)
{
  assert(exfatHandle != NULL);
  assert(exfatHandle->bytesPerSector != 0);

  return offset/(uint64_t)exfatHandle->bytesPerSector;
}

/***********************************************************************\
* Name   : clusterToSector
* Purpose: convert cluster index to sector index
* Input  : exfatHandle - EXFAT handle
*          clusters    - cluster index
* Output : -
* Return : sector index
* Notes  : -
\***********************************************************************/

LOCAL_INLINE uint64_t clusterToSector(const EXFATHandle *exfatHandle, uint32_t cluster)
{
  assert(exfatHandle != NULL);

  return cluster*exfatHandle->sectorsPerCluster;
}

/***********************************************************************\
* Name   : clusterToSector
* Purpose: convert sector index to cluster index
* Input  : exfatHandle - EXFAT handle
*          sector      - sector index
* Output : -
* Return : cluster index
* Notes  : -
\***********************************************************************/

LOCAL_INLINE uint32_t sectorToCluster(const EXFATHandle *exfatHandle, uint64_t sector)
{
  assert(exfatHandle != NULL);
  assert(exfatHandle->sectorsPerCluster != 0);

  return (uint32_t)(sector/(uint64_t)exfatHandle->sectorsPerCluster);
}

/***********************************************************************\
* Name   : clustersToOffset
* Purpose: convert cluster index to byte offset
* Input  : exfatHandle - EXFAT handle
*          cluster     - cluster index
* Output : -
* Return : bytes offset
* Notes  : -
\***********************************************************************/

LOCAL_INLINE uint64_t clusterToOffset(const EXFATHandle *exfatHandle, uint32_t cluster)
{
  assert(exfatHandle != NULL);

  return sectorToOffset(exfatHandle,clusterToSector(exfatHandle,cluster));
}

/***********************************************************************\
* Name   : offsetToSector
* Purpose: convert byte offset to cluster index
* Input  : exfatHandle - EXFAT handle
*          offset      - bytes offset
* Output : -
* Return : cluster index
* Notes  : -
\***********************************************************************/

LOCAL_INLINE uint32_t offsetToCluster(const EXFATHandle *exfatHandle, uint64_t offset)
{
  assert(exfatHandle != NULL);

  return sectorToCluster(exfatHandle,offsetToSector(exfatHandle,offset));
}

/***********************************************************************\
* Name   : readClusterBitmap
* Purpose: read cluster bitmap
* Input  : deviceHandle - device handle
*          exfatHandle  - EXFAT handle
* Output : -
* Return : TRUE iff cluster bitmap is read, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool EXFAT_blockIsUsed(DeviceHandle *deviceHandle, EXFATHandle *exfatHandle, uint64_t offset);

LOCAL bool EXFAT_readClusterBitmap(DeviceHandle *deviceHandle, EXFATHandle *exfatHandle)
{
  assert(deviceHandle != NULL);
  assert(exfatHandle != NULL);
  assert(exfatHandle->clusterCount > 0);

  Errors error;

  // allocate cluster allocation bitmap
  exfatHandle->clusterBitmap = (byte*)malloc((exfatHandle->clusterCount+7)/8);
  if (exfatHandle->clusterBitmap == NULL)
  {
    return FALSE;
  }

  // find and read cluster allocation bitmap
  bool bitmapReadFlag = FALSE;
  uint64_t offset = sectorToOffset( exfatHandle,exfatHandle->clusterHeapOffset
                                   +clusterToSector(exfatHandle,(exfatHandle->rootDirectoryCluster-CLUSTER_BASE_INDEX))
                                  );
  error = Device_seek(deviceHandle,offset);
  if (error == ERROR_NONE)
  {
    EXFATEntry exfatEntry;
    uint8      entryType = ENTRY_TYPE_NONE;
    do
    {
      error = Device_read(deviceHandle,&exfatEntry,sizeof(exfatEntry),NULL);
      if (error == ERROR_NONE)
      {
        entryType = EXFAT_READ_UINT8(exfatEntry.type) & ENTRY_TYPE_MASK;
        switch (entryType)
        {
          case ENTRY_TYPE_BITMAP:
            {
              uint64_t offset = sectorToOffset( exfatHandle,exfatHandle->clusterHeapOffset
                                               +clusterToSector(exfatHandle,(EXFAT_READ_UINT32(exfatEntry.bitmap.startCluster)-CLUSTER_BASE_INDEX))
                                              );
              uint64_t size   = EXFAT_READ_UINT64(exfatEntry.bitmap.size);
              error = Device_seek(deviceHandle,offset);
              if (error == ERROR_NONE)
              {
                error = Device_read(deviceHandle,exfatHandle->clusterBitmap,size,NULL);
                if (error == ERROR_NONE)
                {
                  bitmapReadFlag = TRUE;
                }
              }
            }
            break;
          case ENTRY_TYPE_UPCASE:
            break;
          case ENTRY_TYPE_VOLUME_LABEL:
            break;
          case ENTRY_TYPE_FILE_DIRECTORY:
            break;
          case ENTRY_TYPE_STREAM_EXTENSION:
            break;
          case ENTRY_TYPE_FILE_NAME:
            break;
          case ENTRY_TYPE_WINDOWS_CE_ACCESS_CONTROL_LIST:
            break;
          case ENTRY_TYPE_VOLUME_GUID:
            break;
          case ENTRY_TYPE_TEX_FAT_PADDING:
            break;
          case ENTRY_TYPE_WINDWOS_CE_ACCESS_CONTROL_TABLE:
            break;
          default:
            // ignore unknown
            break;
        }
      }
    }
    while ((error == ERROR_NONE) && (exfatEntry.type != 0));
  }
  if ((error != ERROR_NONE) || !bitmapReadFlag)
  {
    free(exfatHandle->clusterBitmap);
    return FALSE;
  }
//debugDumpMemory(exfatHandle->clusterBitmap,exfatHandle->clusterCount/8,0); abort();

#if 0
  {
    uint64_t offset = 0;
    size_t   n      = 0;
    while (offset < sectorToOffset(exfatHandle,exfatHandle->totalSectorsCount))
    {
      if ((n%16) == 0) printf("0x%08x 0x%08x %16llu:",n,offset,offset);
      uint8_t m = 0;
      for (uint64_t i = 0; i < 8; i++)
      {
        if (EXFAT_blockIsUsed(deviceHandle,exfatHandle,offset))
        {
          m |= 1 << i;
        }
        offset += 512;
      }
      printf(" %02x",m);
      n++;
      if ((n%16) == 0) printf("\n");
    }
    printf("\n");
  }
#endif

  return TRUE;
}

/***********************************************************************\
* Name   : EXFAT_getType
* Purpose: get EXFAT file system type
* Input  : deviceHandle - device handle
* Output : -
* Return : file system type or FILE_SYSTEN_UNKNOWN;
* Notes  : -
\***********************************************************************/

LOCAL FileSystemTypes EXFAT_getType(DeviceHandle *deviceHandle)
{
  assert(deviceHandle != NULL);

  // read boot sector
  EXFATBootSector exfatBootSector;
  if (Device_seek(deviceHandle,0) != ERROR_NONE)
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }
  assert(sizeof(exfatBootSector) == 1024);
  if (Device_read(deviceHandle,&exfatBootSector,sizeof(exfatBootSector),NULL) != ERROR_NONE)
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }

  // check if valid boot sector
  if ((uint16)EXFAT_READ_UINT16(exfatBootSector.bootSignature) != EXFAT_MAGIC)
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }

  // get file system info
  uint16_t bytesPerSector       = 1 << EXFAT_READ_UINT8(exfatBootSector.bytesPerSectorShift);
  uint8_t  sectorsPerCluster    = 1 << EXFAT_READ_UINT8(exfatBootSector.sectorsPerClusterShift);
  uint64_t totalSectorsCount    = EXFAT_READ_UINT64(exfatBootSector.partitionLength);
  uint32_t clusterHeapOffset    = EXFAT_READ_UINT32(exfatBootSector.clusterHeapOffset);
  uint32_t clusterCount         = EXFAT_READ_UINT32(exfatBootSector.clusterCount);
  uint32_t rootDirectoryCluster = EXFAT_READ_UINT32(exfatBootSector.rootDirectoryCluster);
  uint8_t  fatCount             = EXFAT_READ_UINT8 (exfatBootSector.fatCount);

  // validate data
  if (   !(bytesPerSector >= 512)
      || !((bytesPerSector % 512) == 0)
      || !(sectorsPerCluster > 0)
      || !(fatCount > 0)
      || !(totalSectorsCount > ((1024*1024) / bytesPerSector))
      || !(clusterCount < 0xFFFFFFF5)
     )
  {
    return FILE_SYSTEM_TYPE_UNKNOWN;
  }

  return FILE_SYSTEM_TYPE_EXFAT;
}

/***********************************************************************\
* Name   : EXFAT_init
* Purpose: initialize EXFAT handle
* Input  : deviceHandle - device handle
* Output : fileSystemType - file system type
*          exexfatHandle    - EXFAT handle (can be NULL)
* Return : TRUE iff file system initialized
* Notes  : -
\***********************************************************************/

LOCAL bool EXFAT_init(DeviceHandle *deviceHandle, FileSystemTypes *fileSystemType, EXFATHandle *exfatHandle)
{
  assert(deviceHandle != NULL);
  assert(fileSystemType != NULL);

  // read boot sector
  EXFATBootSector exfatBootSector;
  if (Device_seek(deviceHandle,0) != ERROR_NONE)
  {
    return FALSE;
  }
  assert(sizeof(exfatBootSector) == 1024);
  if (Device_read(deviceHandle,&exfatBootSector,sizeof(exfatBootSector),NULL) != ERROR_NONE)
  {
    return FALSE;
  }

  // check if valid boot sector
  if ((uint16)EXFAT_READ_UINT16(exfatBootSector.bootSignature) != EXFAT_MAGIC)
  {
    return FALSE;
  }

  // get file system info
  uint16_t bytesPerSector       = 1 << EXFAT_READ_UINT8(exfatBootSector.bytesPerSectorShift);
  uint8_t  sectorsPerCluster    = 1 << EXFAT_READ_UINT8(exfatBootSector.sectorsPerClusterShift);
  uint64_t totalSectorsCount    = EXFAT_READ_UINT64(exfatBootSector.partitionLength);
  uint32_t clusterHeapOffset    = EXFAT_READ_UINT32(exfatBootSector.clusterHeapOffset);
  uint32_t clusterCount         = EXFAT_READ_UINT32(exfatBootSector.clusterCount);
  uint32_t rootDirectoryCluster = EXFAT_READ_UINT32(exfatBootSector.rootDirectoryCluster);
  uint8_t  fatCount             = EXFAT_READ_UINT8 (exfatBootSector.fatCount);

  // validate data
  if (   !(bytesPerSector >= 512)
      || !((bytesPerSector % 512) == 0)
      || !(sectorsPerCluster > 0)
      || !(fatCount > 0)
      || !(totalSectorsCount > ((1024*1024) / bytesPerSector))
      || !(clusterCount < 0xFFFFFFF5)
     )
  {
    return FALSE;
  }

  (*fileSystemType) = FILE_SYSTEM_TYPE_EXFAT;

  if (exfatHandle != NULL)
  {
    // init EXFAT file system info
    exfatHandle->bytesPerSector       = bytesPerSector;
    exfatHandle->sectorsPerCluster    = sectorsPerCluster;
    exfatHandle->totalSectorsCount    = totalSectorsCount;
    exfatHandle->fatCount             = fatCount;
    exfatHandle->rootDirectoryCluster = rootDirectoryCluster;
    exfatHandle->clusterHeapOffset    = clusterHeapOffset;
    exfatHandle->clusterCount         = clusterCount;

    // read cluster bitmap
    if (!EXFAT_readClusterBitmap(deviceHandle,exfatHandle))
    {
      return FALSE;
    }
  }

  return TRUE;
}

/***********************************************************************\
* Name   : EXFAT_done
* Purpose: deinitialize EXFAT handle
* Input  : exfatHandle - EXFAT handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void EXFAT_done(EXFATHandle *exfatHandle)
{
  assert(exfatHandle != NULL);
  assert(exfatHandle->clusterBitmap != NULL);

  free(exfatHandle->clusterBitmap);
}

/***********************************************************************\
* Name   : EXFAT_blockIsUsed
* Purpose: check if block is used
* Input  : deviceHandle   - device handle
*          fileSystemType - file system type
*          exfatHandle      - FAT handle
*          offset         - offset in image
* Output : -
* Return : TRUE iff block at offset is used, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool EXFAT_blockIsUsed(DeviceHandle *deviceHandle, EXFATHandle *exfatHandle, uint64_t offset)
{
  assert(deviceHandle != NULL);
  assert(exfatHandle != NULL);

  bool blockIsUsed = FALSE;

  // calculate cluster
  assert(exfatHandle->bytesPerSector != 0);
  uint64_t sector = offsetToSector(exfatHandle,offset);
//fprintf(stderr,"%s:%d: sector=%llu clusterHeapOffset=%lu\n",__FILE__,__LINE__,sector,exfatHandle->clusterHeapOffset);
  if (sector >= ((uint64_t)exfatHandle->clusterHeapOffset))
  {
    uint32_t cluster = CLUSTER_BASE_INDEX+sectorToCluster(exfatHandle,sector-exfatHandle->clusterHeapOffset);
//fprintf(stderr,"%s:%d: sector=%llu cluster=%lu clusterHeapOffset=%lu\n",__FILE__,__LINE__,sector,cluster,exfatHandle->clusterHeapOffset);
    if (cluster <= exfatHandle->clusterCount)
    {
      blockIsUsed = BITSET_IS_SET(exfatHandle->clusterBitmap,cluster-CLUSTER_BASE_INDEX);
#if 0
fprintf(stderr,"%s:%d: cluster=%u [%d]=%02x bit=%x m=%x %d blockIsUsed=%d\n",__FILE__,__LINE__,
cluster,
(cluster-CLUSTER_BASE_INDEX)/8,
exfatHandle->clusterBitmap[(cluster-CLUSTER_BASE_INDEX)/8],
(1 << (cluster-CLUSTER_BASE_INDEX)%8),
exfatHandle->clusterBitmap[(cluster-CLUSTER_BASE_INDEX)/8] & (1 << (cluster-CLUSTER_BASE_INDEX)%8),
cluster-CLUSTER_BASE_INDEX,blockIsUsed);
#endif
//if (blockIsUsed) fprintf(stderr,"%s, %d: offset=%llu sector=%llu cluster=%u clusterCount=%u byte=%u\n",__FILE__,__LINE__,offset,sector,cluster,exfatHandle->clusterCount,cluster/8);
    }
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
