/***********************************************************************\
*
* Contents: XFS file system
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/

#include <stdlib.h>
#include <semaphore.h>
#include <assert.h>

// Note: libxfs define macro "_" and have some warnings
#ifdef HAVE_XFS
#undef _
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wpointer-arith"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "xfs/libxfs.h"
#pragma GCC pop_options
#endif // HAVE_XFS

#if 0
#warning debug only
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif /* 0 */

#include "common/global.h"
#include "common/filesystems.h"

#include "filesystems_xfs.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

#ifdef HAVE_XFS
typedef enum
{
  XFS_DATA_TYPE_NONE,

  XFS_DATA_TYPE_AGF,
  XFS_DATA_TYPE_AGFL,
  XFS_DATA_TYPE_AGI,
  XFS_DATA_TYPE_ATTR,
  XFS_DATA_TYPE_BMAPBTA,
  XFS_DATA_TYPE_BMAPBTD,
  XFS_DATA_TYPE_BNOBT,
  XFS_DATA_TYPE_CNTBT,
  XFS_DATA_TYPE_DATA,
  XFS_DATA_TYPE_DIR2,
  XFS_DATA_TYPE_DQBLK,
  XFS_DATA_TYPE_INOBT,
  XFS_DATA_TYPE_INODATA,
  XFS_DATA_TYPE_INODE,
  XFS_DATA_TYPE_LOG,
  XFS_DATA_TYPE_RTBITMAP,
  XFS_DATA_TYPE_RTSUMMARY,
  XFS_DATA_TYPE_SB,
  XFS_DATA_TYPE_SYMLINK,
  XFS_DATA_TYPE_TEXT
} XFSDataType;

typedef bool(*ScanBTreeFunction)(XFSHandle                    *xfsHandle,
                                 xfs_mount_t                  *xfsMount,
                                 const struct xfs_btree_block *block,
                                 XFSDataType                  xfsDataType,
                                 size_t                       level,
                                 const xfs_agf_t              *agf
                                );
#endif // HAVE_XFS

/***************************** Variables *******************************/
#ifdef HAVE_XFS
LOCAL sem_t              libXFSInitLock;
LOCAL struct libxfs_init libXFSInit;
#endif // HAVE_XFS

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

#ifdef HAVE_XFS
LOCAL bool scanBTree(XFSHandle         *xfsHandle,
                     xfs_mount_t       *xfsMount,
                     const xfs_agf_t   *agf,
                     xfs_agblock_t     root,
                     XFSDataType       xfsDataType,
                     size_t            level,
                     ScanBTreeFunction scanBTreeFunction
                    );

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : xfsReadSuperBlock
* Purpose: read super block
* Input  : deviceHandle  - device handle
*          xfsSuperBlock - super block variable
*          offset        - block offset [byte]
*          size          - block size [byte]
* Output : -
* Return : TRUE iff super block read
* Notes  : -
\***********************************************************************/

LOCAL bool xfsReadSuperBlock(DeviceHandle *deviceHandle, xfs_sb_t *xfsSuperBlock, xfs_off_t offset, size_t size)
{
  struct xfs_dsb *xfsDiskSuperBlock = aligned_alloc(libxfs_device_alignment(), size);
  if (xfsDiskSuperBlock == NULL)
  {
    return FALSE;
  }
  memClear(xfsDiskSuperBlock,size);

  if (Device_seek(deviceHandle,offset) != ERROR_NONE)
  {
    free(xfsDiskSuperBlock);
    return FALSE;
  }
  if (Device_read(deviceHandle,xfsDiskSuperBlock,size,NULL) != ERROR_NONE)
  {
    free(xfsDiskSuperBlock);
    return FALSE;
  }
//debugDumpMemory(xfsDiskSuperBlock,size,0);

  memClear(xfsSuperBlock,sizeof(xfs_sb_t));
  libxfs_sb_from_disk(xfsSuperBlock,xfsDiskSuperBlock);

  free(xfsDiskSuperBlock);

  return TRUE;
}

/***********************************************************************\
* Name   : xfsReadBuffer
* Purpose: read XFS data into buffer
* Input  : target  - target
*          address - address
*          length  - bytes to length
*          flags   - flags for libxfs_readbufr()
* Output : -
* Return : libxfs buffer or NULL if read fail
* Notes  : -
\***********************************************************************/

LOCAL struct xfs_buf *xfsReadBuffer(struct xfs_buftarg *target,
                                    xfs_daddr_t        address,
                                    size_t             length,
                                    int                flags
                                   )
{
  struct xfs_buf *xfsBuffer;

  int error;

#if 1
  // allocate libxfs buffer
#if 1
  error = libxfs_buf_get(target,
                         address,
                         length,
                         &xfsBuffer
                        );
#else
  error = libxfs_buf_get_uncached(target,
                         length,
                         &xfsBuffer
                        );
#endif
  if (error != 0)
  {
    return NULL;
  }

  // read data
  error = libxfs_readbufr(target,
                          address,
                          xfsBuffer,
                          length,
                          flags
                         );
  if (error != 0)
  {
    libxfs_buf_relse(xfsBuffer);
    return NULL;
  }
#else
  error = libxfs_buf_read_uncached(target,
                          address,
                          length,
                          flags,
                          &xfsBuffer,
                          NULL
                         );
  if (error != 0)
  {
    return NULL;
  }
#endif

  return xfsBuffer;
}

/***********************************************************************\
* Name   : xfsFreeBuffer
* Purpose: free libxfs buffer
* Input  : xfsFreeBuffer = libxfs buffer
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void xfsFreeBuffer(struct xfs_buf *xfsBuffer)
{
  assert(xfsBuffer != NULL);

  libxfs_buf_relse(xfsBuffer);
}

/***********************************************************************\
* Name   : markUnused
* Purpose: mark blocks as unused
* Input  : xfsHandle - XFS handle
*          xfsMount  - XFS mount information
*          agno      - allocation group sequence number
*          agbno     - allocation group block number
*          length    - number of blocks
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void markUnused(XFSHandle         *xfsHandle,
                      const xfs_mount_t *xfsMount,
                      xfs_agnumber_t    agno,
                      xfs_agblock_t     agbno,
                      xfs_extlen_t      length
                     )
{
  assert(xfsHandle != NULL);

  uint64_t startBlock = (agno*xfsMount->m_sb.sb_agblocks)+agbno;
//fprintf(stderr,"%s:%d: clear %d..%d %d\n",__FILE__,__LINE__,startBlock,startBlock+length-1,length);
  BitSet_clear(&xfsHandle->blocksBitSet,startBlock,length);
}

/***********************************************************************\
* Name   : scanFreeList
* Purpose: scan list of free blocks
* Input  : deviceHandle - device handle
*          xfsHandle    - XFS handle
*          xfsMount     - XFS mount information
*          agf          - allocation group free space
* Output : -
* Return : TRUE iff scanned
* Notes  : -
\***********************************************************************/

LOCAL bool scanFreeList(XFSHandle   *xfsHandle,
                        xfs_mount_t *xfsMount,
                        xfs_agf_t   *agf
                       )
{
  assert(xfsHandle != NULL);
  assert(xfsMount != NULL);
  assert(agf != NULL);

  // check if free list is empty
  if (be32_to_cpu(agf->agf_flcount) == 0)
  {
    return TRUE;
  }

  // verify allocation group free values
  if (   (be32_to_cpu(agf->agf_flfirst) >= xfs_agfl_size(xfsMount))
      || (be32_to_cpu(agf->agf_fllast) >= xfs_agfl_size(xfsMount))
     )
  {
    return FALSE;
  }

  // read allocation group free space block
  struct xfs_buf *xfsBuffer = xfsReadBuffer(xfsMount->m_dev,
                                            XFS_AG_DADDR(xfsMount,be32_to_cpu(agf->agf_seqno),XFS_AGFL_DADDR(xfsMount)),
                                            XFS_FSS_TO_BB(xfsMount,1),
                                            0  // flags
                                           );
  if (xfsBuffer == NULL)
  {
    return FALSE;
  }

  // traverse all allocation group free space blocks
  xfs_agfl_walk(xfsMount,
                agf,
                xfsBuffer,
                LAMBDA(int,(struct xfs_mount *xfsMount, xfs_agblock_t bno, void *userData),
                {
                  markUnused(xfsHandle,
                             xfsMount,
                             be32_to_cpu(agf->agf_seqno),
                             bno,
                             1  // length
                            );
                  return 0;
                }
                ),
                NULL
               );

  // free resources
  xfsFreeBuffer(xfsBuffer);

  return TRUE;
}

/***********************************************************************\
* Name   : scanBTree
* Purpose: scan B+ tree
* Input  : deviceHandle      - device handle
*          xfsHandle         - XFS handle
*          xfsMount          - XFS mount information
*          agf               - allocation group free space
*          root              - root
*          level             - level
*          scanBTreeFunction - scan function call back
* Output : -
* Return : TRUE iff scanned
* Notes  : -
\***********************************************************************/

LOCAL bool scanBTree(XFSHandle         *xfsHandle,
                     xfs_mount_t       *xfsMount,
                     const xfs_agf_t   *agf,
                     xfs_agblock_t     root,
                     XFSDataType       xfsDataType,
                     size_t            level,
                     ScanBTreeFunction scanBTreeFunction
                    )
{
  assert(xfsHandle != NULL);
  assert(xfsMount != NULL);
  assert(agf != NULL);
  assert(level > 0);
  assert(scanBTreeFunction != NULL);

  struct xfs_buf *xfsBuffer = xfsReadBuffer(xfsMount->m_dev,
                                            XFS_AGB_TO_DADDR(xfsMount,be32_to_cpu(agf->agf_seqno),root),
                                            XFS_FSB_TO_BB(xfsMount,1),
                                            0  // flags
                                           );
  if (xfsBuffer == NULL)
  {
    return FALSE;
  }

  void *data = xfsBuffer->b_addr;
  if (data == NULL)
  {
    xfsFreeBuffer(xfsBuffer);
    return FALSE;
  }
  scanBTreeFunction(xfsHandle,xfsMount,data,xfsDataType,level-1,agf);

  // free resources
  xfsFreeBuffer(xfsBuffer);

  return TRUE;
}

/***********************************************************************\
* Name   : allocationGroupScanFunction
* Purpose: allocation group scan callback function
* Input  : deviceHandle - device handle
*          xfsHandle    - XFS handle
*          xfsMount     - XFS mount information
*          block        - block
*          xfsDataType  - data type
*          level        - level
*          agf          - allocation group free space
* Output : -
* Return : TRUE iff scanned
* Notes  : -
\***********************************************************************/

LOCAL bool allocationGroupScanFunction(XFSHandle                    *xfsHandle,
                                       xfs_mount_t                  *xfsMount,
                                       const struct xfs_btree_block *block,
                                       XFSDataType                  xfsDataType,
                                       size_t                       level,
                                       const xfs_agf_t              *agf
                                      )
{
  // check for magic numbers in block
  if (   (be32_to_cpu(block->bb_magic) != XFS_ABTB_MAGIC)
      && (be32_to_cpu(block->bb_magic) != XFS_ABTB_CRC_MAGIC)
     )
  {
    return FALSE;
  }

  bool result = TRUE;

  if (level == 0)
  {
    const xfs_alloc_rec_t *allocationRecord = XFS_ALLOC_REC_ADDR(xfsMount, block, 1);
    for (size_t i = 0; i < be16_to_cpu(block->bb_numrecs); i++)
    {
      markUnused(xfsHandle,
                 xfsMount,
                 be32_to_cpu(agf->agf_seqno),
                 be32_to_cpu(allocationRecord[i].ar_startblock),
                 be32_to_cpu(allocationRecord[i].ar_blockcount)
                );
    }
  }
  else
  {
    const xfs_alloc_ptr_t *allocationPointer = XFS_ALLOC_PTR_ADDR(xfsMount, block, 1, xfsMount->m_alloc_mxr[1]);
    size_t i = 0;
    while ((i < be16_to_cpu(block->bb_numrecs)) && result)
    {
      result = scanBTree(xfsHandle,
                         xfsMount,
                         agf,
                         be32_to_cpu(allocationPointer[i]),
                         xfsDataType,
                         level,
                         allocationGroupScanFunction
                        );
      i++;
    }
  }

  return result;
}

/***********************************************************************\
* Name   : scanAllocationGroup
* Purpose: scan allocation group
* Input  : deviceHandle          - device handle
*          xfsHandle             - XFS handle
*          xfsMount              - XFS mount information
*          allocationGroupNumber - allocation group number
* Output : -
* Return : TRUE iff scanned
* Notes  : -
\***********************************************************************/
LOCAL bool scanAllocationGroup(XFSHandle      *xfsHandle,
                               xfs_mount_t    *xfsMount,
                               xfs_agnumber_t allocationGroupNumber
                              )
{
  struct xfs_buf *xfsBuffer = xfsReadBuffer(xfsMount->m_dev,
                                            XFS_AG_DADDR(xfsMount,allocationGroupNumber,XFS_AGF_DADDR(xfsMount)),
                                            XFS_FSS_TO_BB(xfsMount,1),
                                            0  // flags
                                           );
  if (xfsBuffer == NULL)
  {
    return FALSE;
  }

  xfs_agf_t *agf = xfsBuffer->b_addr;
  if (!scanFreeList(xfsHandle,xfsMount,agf))
  {
    xfsFreeBuffer(xfsBuffer);
    return FALSE;
  }
  if (!scanBTree(xfsHandle,
                 xfsMount,
                 agf,
                 be32_to_cpu(agf->agf_bno_root),
                 XFS_DATA_TYPE_BNOBT,
                 be32_to_cpu(agf->agf_bno_level),
                 allocationGroupScanFunction
                )
     )
  {
    xfsFreeBuffer(xfsBuffer);
    return FALSE;
  }

  // free resources
  xfsFreeBuffer(xfsBuffer);

  return TRUE;
}

/***********************************************************************\
* Name   : readBitmap
* Purpose: read used blocks bitset
* Input  : deviceHandle - device handle
*          xfsHandle    - XFS handle
*          xfsMount     - XFS mount information
* Output : -
* Return : TRUE iff bitset read
* Notes  : -
\***********************************************************************/

LOCAL bool readBitmap(DeviceHandle *deviceHandle, XFSHandle *xfsHandle, xfs_mount_t *xfsMount)
{
  assert(deviceHandle != NULL);
  assert(xfsHandle != NULL);
  assert(xfsMount != NULL);

  UNUSED_VARIABLE(deviceHandle);

  // init with all blocks used
  BitSet_setAll(&xfsHandle->blocksBitSet);

  // scan allocation groups
  for (xfs_agnumber_t allocationGroupNumber = 0; allocationGroupNumber < xfsMount->m_sb.sb_agcount; allocationGroupNumber++)
  {
    if (!scanAllocationGroup(xfsHandle,xfsMount,allocationGroupNumber))
    {
      return FALSE;
    }
  }

#if 0
  // debug only
  uint64_t blockUsed  = 0;
  uint64_t blocksFree = 0;
  for (uint64_t i = 0; i < xfsHandle->totalBlocks; i++)
  {
    if (BITSET_IS_SET(xfsHandle->bitmapBlocks,xfsHandle->totalBlocks,i))
      blockUsed++;
    else
      blocksFree++;
  }
  fprintf(stderr,"%s:%d: block used=%lu blocks free=%lu\n",__FILE__,__LINE__,blockUsed,blocksFree);
#endif

  return TRUE;
}
#endif // HAVE_XFS

/***********************************************************************\
* Name   : XFS_initAll
* Purpose: init XFS file system
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors XFS_initAll(void)
{
#ifdef HAVE_XFS
  if (sem_init(&libXFSInitLock,0,1) != 0)
  {
    return ERROR_INIT;
  }
  memClear(&libXFSInit,sizeof(libXFSInit));
  libXFSInit.flags=LIBXFS_DIRECT|LIBXFS_ISREADONLY;
  if (libxfs_init(&libXFSInit) != 1)
  {
    return ERROR_INIT;
  }
#endif // HAVE_XFS

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : XFS_doneAll
* Purpose: deinitialize XFS file system
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL void XFS_doneAll(void)
{
#ifdef HAVE_XFS
  libxfs_destroy(&libXFSInit);
  sem_destroy(&libXFSInitLock);
#endif // HAVE_XFS
}

/***********************************************************************\
* Name   : XFS_init
* Purpose: initialize XFS file system
* Input  : deviceHandle - device handle
* Output : fileSystemType - file system type
*          xfsHandle      - XFS handle (can be NULL)
* Output : -
* Return : TRUE iff file system initialized
* Notes  : -
\***********************************************************************/

LOCAL bool XFS_init(DeviceHandle *deviceHandle, FileSystemTypes *fileSystemType, XFSHandle *xfsHandle)
{
  assert(deviceHandle != NULL);
  assert(fileSystemType != NULL);

#ifdef HAVE_XFS
  // lock
  sem_wait(&libXFSInitLock);

  // set device handles
  libXFSInit.data.fd = Device_getDescriptor(deviceHandle);
  libXFSInit.log.fd  = Device_getDescriptor(deviceHandle);
  libXFSInit.rt.fd   = Device_getDescriptor(deviceHandle);

  // init XFS mount information
  xfs_mount_t xfsMount;
  memClear(&xfsMount,sizeof(xfsMount));

  // read super block
  if (!xfsReadSuperBlock(deviceHandle,&xfsMount.m_sb,0,XFS_MAX_SECTORSIZE))
  {
    sem_post(&libXFSInitLock);
    return FALSE;
  }

  // check if this a super block and valid
  if (   (xfsMount.m_sb.sb_magicnum != XFS_SB_MAGIC)
      || xfsMount.m_sb.sb_inprogress
      || (xfsMount.m_sb.sb_logstart == 0)
      || (xfsMount.m_sb.sb_rextents != 0)
     )
  {
    sem_post(&libXFSInitLock);
    return FALSE;
  }

  (*fileSystemType) = FILE_SYSTEM_TYPE_XFS;

  if (xfsHandle != NULL)
  {
    xfsHandle->blockSize   = xfsMount.m_sb.sb_blocksize;
    xfsHandle->totalBlocks = xfsMount.m_sb.sb_dblocks;
//fprintf(stderr,"%s:%d: xfsHandle->blockSize=%d\n",__FILE__,__LINE__,xfsHandle->blockSize);
//fprintf(stderr,"%s:%d: xfsHandle->totalBlocks=%lu\n",__FILE__,__LINE__,xfsHandle->totalBlocks);

    // get file system block info, init data
    if (libxfs_mount(&xfsMount,&xfsMount.m_sb,&libXFSInit,1) == NULL)
    {
      sem_post(&libXFSInitLock);
      return FALSE;
    }

    // read allocation bit set
    if (!BitSet_init(&xfsHandle->blocksBitSet,xfsHandle->totalBlocks))
    {
      libxfs_umount(&xfsMount);
      sem_post(&libXFSInitLock);
      return FALSE;
    }
    if (!readBitmap(deviceHandle,xfsHandle,&xfsMount))
    {
      BitSet_done(&xfsHandle->blocksBitSet);
      libxfs_umount(&xfsMount);
      sem_post(&libXFSInitLock);
      return FALSE;
    }

    // free resources
    libxfs_umount(&xfsMount);

#if 0
#warning debug only
fprintf(stderr,"\n");
for (size_t i = 0; i < xfsHandle->bitmapBlocksCount; i++)
{
fprintf(stderr,"%s,%d: z=%d block=%ld used=%d\n",__FILE__,__LINE__,z,xfsHandle->bitmapBlocks[i],EXT_blockIsUsed(fileSystemHandle->deviceHandle,xfsHandle,xfsHandle->bitmapBlocks[z]));
}
#endif /* 0 */
  }

  // unlock
  sem_post(&libXFSInitLock);

  return TRUE;
#else // not HAVE_XFS
  UNUSED_VARIABLE(deviceHandle);
  UNUSED_VARIABLE(fileSystemType);
  UNUSED_VARIABLE(xfsHandle);

  return FALSE;
#endif // HAVE_XFS
}

/***********************************************************************\
* Name   : XFS_done
* Purpose: deinitialize XFS handle
* Input  : fileSystemHandle - file system handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/
LOCAL void XFS_done(XFSHandle *xfsHandle)
{
#ifdef HAVE_XFS
  assert(xfsHandle != NULL);

  BitSet_done(&xfsHandle->blocksBitSet);
#else // not HAVE_XFS
  UNUSED_VARIABLE(xfsHandle);
#endif // HAVE_XFS
}

/***********************************************************************\
* Name   : XFS_blockIsUsed
* Purpose: check if block is used
* Input  : deviceHandle   - device handle
*          fileSystemType - file system type
*          xfsHandle      - EXT handle
*          offset         - offset in image
* Output : -
* Return : TRUE iff block at offset is used, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool XFS_blockIsUsed(DeviceHandle *deviceHandle, const XFSHandle *xfsHandle, uint64 offset)
{
  assert(xfsHandle != NULL);

  UNUSED_VARIABLE(deviceHandle);

#ifdef HAVE_XFS
  uint64 block = offset/xfsHandle->blockSize;
  return BitSet_isSet(&xfsHandle->blocksBitSet,block);
#else // not HAVE_XFS
  UNUSED_VARIABLE(deviceHandle);
  UNUSED_VARIABLE(xfsHandle);
  UNUSED_VARIABLE(offset);

  return TRUE;
#endif // HAVE_XFS
}

#ifdef __cplusplus
  }
#endif

/* end of file */
