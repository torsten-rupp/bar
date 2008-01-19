/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/compress.h,v $
* $Revision: 1.11 $
* $Author: torsten $
* Contents: Backup ARchiver compress functions
* Systems : all
*
\***********************************************************************/

#ifndef __COMPRESS__
#define __COMPRESS__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <zlib.h>
#ifdef HAVE_BZ2
  #include <bzlib.h>
#endif /* HAVE_BZ2 */
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

typedef enum
{
  COMPRESS_MODE_DEFLATE,
  COMPRESS_MODE_INFLATE,
} CompressModes;

typedef enum
{
  COMPRESS_STATE_INIT,      // initialized, but no data in compress/decompress
  COMPRESS_STATE_DONE,      // deinitialized
  COMPRESS_STATE_RUNNING    // running, data available in internal compress/decompress buffers
} CompressStates;

typedef enum
{
  COMPRESS_ALGORITHM_NONE,

  COMPRESS_ALGORITHM_ZIP_0,
  COMPRESS_ALGORITHM_ZIP_1,
  COMPRESS_ALGORITHM_ZIP_2,
  COMPRESS_ALGORITHM_ZIP_3,
  COMPRESS_ALGORITHM_ZIP_4,
  COMPRESS_ALGORITHM_ZIP_5,
  COMPRESS_ALGORITHM_ZIP_6,
  COMPRESS_ALGORITHM_ZIP_7,
  COMPRESS_ALGORITHM_ZIP_8,
  COMPRESS_ALGORITHM_ZIP_9,

  COMPRESS_ALGORITHM_BZIP2_1,
  COMPRESS_ALGORITHM_BZIP2_2,
  COMPRESS_ALGORITHM_BZIP2_3,
  COMPRESS_ALGORITHM_BZIP2_4,
  COMPRESS_ALGORITHM_BZIP2_5,
  COMPRESS_ALGORITHM_BZIP2_6,
  COMPRESS_ALGORITHM_BZIP2_7,
  COMPRESS_ALGORITHM_BZIP2_8,
  COMPRESS_ALGORITHM_BZIP2_9,

  COMPRESS_ALGORITHM_UNKNOWN=0xFFFF,
} CompressAlgorithms;

/***************************** Datatypes *******************************/

/* compress info block */
typedef struct
{
  CompressModes      compressMode;
  CompressAlgorithms compressAlgorithm;
  ulong              blockLength;

  CompressStates     compressState;             // compress/decompress state
  bool               endOfDataFlag;             // TRUE if end-of-data detected
  bool               flushFlag;                 // TRUE for flushing all buffers

  union
  {
    struct
    {
      uint64 length;
    } none;
    struct
    {
      z_stream stream;
    } zlib;
    #ifdef HAVE_BZ2
      struct
      {
        uint      compressionLevel;
        bz_stream stream;
      } bzlib;
    #endif /* HAVE_BZ2 */
  };

  byte               *dataBuffer;               // buffer for uncompressed data
  ulong              dataBufferIndex;           // position of next byte in uncompressed data buffer
  ulong              dataBufferLength;          // length of data in uncompressed data buffer
  ulong              dataBufferSize;            // length of data in uncompressed data buffer

  byte               *compressBuffer;           // buffer for compressed data
  ulong              compressBufferIndex;       // position of next byte in compressed data buffer
  ulong              compressBufferLength;      // length of data in compressed data buffer
  ulong              compressBufferSize;        // length of data in compressed data buffer

} CompressInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Compress_init
* Purpose: initialize compress functions
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_init(void);

/***********************************************************************\
* Name   : Compress_done
* Purpose: deinitialize compress functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Compress_done(void);

/***********************************************************************\
* Name   : Compress_getAlgorithmName
* Purpose: get name of compress algorithm
* Input  : compressAlgorithm - compress algorithm
* Output : -
* Return : compress algorithm name
* Notes  : -
\***********************************************************************/

const char *Compress_getAlgorithmName(CompressAlgorithms compressAlgorithm);

/***********************************************************************\
* Name   : Compress_getAlgorithm
* Purpose: get compress algorithm
* Input  : name - algorithm name
* Output : -
* Return : compress algorithm
* Notes  : -
\***********************************************************************/

CompressAlgorithms Compress_getAlgorithm(const char *name);

/***********************************************************************\
* Name   : Compress_new
* Purpose: create new compress handle
* Input  : compressInfo     - compress info block
*          compressionLevel - compression level (0..9)
*          blockLength      - block length
* Output : compressInfo - initialized compress info block
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_new(CompressInfo       *compressInfo,
                    CompressModes      compressMode,
                    CompressAlgorithms compressAlgorithm,
                    ulong              blockLength
                   );

/***********************************************************************\
* Name   : Compress_delete
* Purpose: delete compress handle
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

void Compress_delete(CompressInfo *compressInfo);

/***********************************************************************\
* Name   : Compress_reset
* Purpose: reset compress handle
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_reset(CompressInfo *compressInfo);

/***********************************************************************\
* Name   : Compress_deflate
* Purpose: deflate data
* Input  : compressInfo - compress info block
*          data         - data to compress
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_deflate(CompressInfo *compressInfo,
                        byte         data
                       );

/***********************************************************************\
* Name   : Compress_inflate
* Purpose: inflate data
* Input  : compressInfo - compress info block
* Output : data - decompressed data
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_inflate(CompressInfo *compressInfo,
                        byte         *data
                       );

/***********************************************************************\
* Name   : Compress_flush
* Purpose: flush compress data
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_flush(CompressInfo *compressInfo);

/***********************************************************************\
* Name   : Compress_flush
* Purpose: flush compress data
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

uint64 Compress_getInputLength(CompressInfo *compressInfo);

/***********************************************************************\
* Name   : Compress_flush
* Purpose: flush compress data
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

uint64 Compress_getOutputLength(CompressInfo *compressInfo);

/***********************************************************************\
* Name   : Compress_available
* Purpose: get number of available bytes
* Input  : compressInfo - compress info block
* Output : availableBytes - number of available bytes
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_available(CompressInfo *compressInfo, ulong *availableBytes);

/***********************************************************************\
* Name   : Compress_checkBlockIsFull
* Purpose: check if block is full
* Input  : compressInfo - compress info block
* Output : -
* Return : TRUE if block is full, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Compress_checkBlockIsFull(CompressInfo *compressInfo);

/***********************************************************************\
* Name   : Compress_checkBlockIsEmpty
* Purpose: check if block is empty
* Input  : compressInfo - compress info block
* Output : -
* Return : TRUE if block is empty, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Compress_checkBlockIsEmpty(CompressInfo *compressInfo);

/***********************************************************************\
* Name   : Compress_checkEndOfBlock
* Purpose: check end of block reached
* Input  : compressInfo - compress info block
* Output : -
* Return : TRUE at end of block, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Compress_checkEndOfBlock(CompressInfo *compressInfo);

/***********************************************************************\
* Name   : Compress_getBlock
* Purpose: get block data
* Input  : compressInfo - compress info block
* Output : buffer       - data
*          bufferLength - number of bytes in block
* Return : -
* Notes  : buffer size have to be at least blockLength!
\***********************************************************************/

void Compress_getBlock(CompressInfo *compressInfo,
                       byte         *buffer,
                       ulong        *bufferLength
                      );

/***********************************************************************\
* Name   : Compress_putBlock
* Purpose: put block data
* Input  : compressInfo - compress info block
*          buffer       - data
*          bufferLength - length of data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Compress_putBlock(CompressInfo *compressInfo,
                       void         *buffer,
                       ulong        bufferLength
                      );

#ifdef __cplusplus
  }
#endif

#endif /* __CRYPT__ */

/* end of file */
