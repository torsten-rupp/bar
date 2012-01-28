/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
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
#ifdef HAVE_LZMA
  #include <lzma.h>
#endif /* HAVE_LZMA */
#ifdef HAVE_XDELTA3
  #include "xdelta3.h"
#endif /* HAVE_XDELTA */
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "archive_format_const.h"
#include "errors.h"
#include "sources.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

typedef enum
{
  COMPRESS_MODE_DEFLATE,    // compress
  COMPRESS_MODE_INFLATE,    // decompress
} CompressModes;

typedef enum
{
  COMPRESS_STATE_INIT,      // initialized, but no data in compress/decompress
  COMPRESS_STATE_DONE,      // deinitialized
  COMPRESS_STATE_RUNNING    // running, data available in internal compress/decompress buffers
} CompressStates;

typedef enum
{
  COMPRESS_ALGORITHM_NONE      = CHUNK_CONST_COMPRESS_ALGORITHM_NONE,

  COMPRESS_ALGORITHM_ZIP_0     = CHUNK_CONST_COMPRESS_ALGORITHM_ZIP_0,
  COMPRESS_ALGORITHM_ZIP_1     = CHUNK_CONST_COMPRESS_ALGORITHM_ZIP_1,
  COMPRESS_ALGORITHM_ZIP_2     = CHUNK_CONST_COMPRESS_ALGORITHM_ZIP_2,
  COMPRESS_ALGORITHM_ZIP_3     = CHUNK_CONST_COMPRESS_ALGORITHM_ZIP_3,
  COMPRESS_ALGORITHM_ZIP_4     = CHUNK_CONST_COMPRESS_ALGORITHM_ZIP_4,
  COMPRESS_ALGORITHM_ZIP_5     = CHUNK_CONST_COMPRESS_ALGORITHM_ZIP_5,
  COMPRESS_ALGORITHM_ZIP_6     = CHUNK_CONST_COMPRESS_ALGORITHM_ZIP_6,
  COMPRESS_ALGORITHM_ZIP_7     = CHUNK_CONST_COMPRESS_ALGORITHM_ZIP_7,
  COMPRESS_ALGORITHM_ZIP_8     = CHUNK_CONST_COMPRESS_ALGORITHM_ZIP_8,
  COMPRESS_ALGORITHM_ZIP_9     = CHUNK_CONST_COMPRESS_ALGORITHM_ZIP_9,

  COMPRESS_ALGORITHM_BZIP2_1   = CHUNK_CONST_COMPRESS_ALGORITHM_BZIP2_1,
  COMPRESS_ALGORITHM_BZIP2_2   = CHUNK_CONST_COMPRESS_ALGORITHM_BZIP2_2,
  COMPRESS_ALGORITHM_BZIP2_3   = CHUNK_CONST_COMPRESS_ALGORITHM_BZIP2_3,
  COMPRESS_ALGORITHM_BZIP2_4   = CHUNK_CONST_COMPRESS_ALGORITHM_BZIP2_4,
  COMPRESS_ALGORITHM_BZIP2_5   = CHUNK_CONST_COMPRESS_ALGORITHM_BZIP2_5,
  COMPRESS_ALGORITHM_BZIP2_6   = CHUNK_CONST_COMPRESS_ALGORITHM_BZIP2_6,
  COMPRESS_ALGORITHM_BZIP2_7   = CHUNK_CONST_COMPRESS_ALGORITHM_BZIP2_7,
  COMPRESS_ALGORITHM_BZIP2_8   = CHUNK_CONST_COMPRESS_ALGORITHM_BZIP2_8,
  COMPRESS_ALGORITHM_BZIP2_9   = CHUNK_CONST_COMPRESS_ALGORITHM_BZIP2_9,

  COMPRESS_ALGORITHM_LZMA_1    = CHUNK_CONST_COMPRESS_ALGORITHM_LZMA_1,
  COMPRESS_ALGORITHM_LZMA_2    = CHUNK_CONST_COMPRESS_ALGORITHM_LZMA_2,
  COMPRESS_ALGORITHM_LZMA_3    = CHUNK_CONST_COMPRESS_ALGORITHM_LZMA_3,
  COMPRESS_ALGORITHM_LZMA_4    = CHUNK_CONST_COMPRESS_ALGORITHM_LZMA_4,
  COMPRESS_ALGORITHM_LZMA_5    = CHUNK_CONST_COMPRESS_ALGORITHM_LZMA_5,
  COMPRESS_ALGORITHM_LZMA_6    = CHUNK_CONST_COMPRESS_ALGORITHM_LZMA_6,
  COMPRESS_ALGORITHM_LZMA_7    = CHUNK_CONST_COMPRESS_ALGORITHM_LZMA_7,
  COMPRESS_ALGORITHM_LZMA_8    = CHUNK_CONST_COMPRESS_ALGORITHM_LZMA_8,
  COMPRESS_ALGORITHM_LZMA_9    = CHUNK_CONST_COMPRESS_ALGORITHM_LZMA_9,

  COMPRESS_ALGORITHM_XDELTA_1  = CHUNK_CONST_COMPRESS_ALGORITHM_XDELTA_1,
  COMPRESS_ALGORITHM_XDELTA_2  = CHUNK_CONST_COMPRESS_ALGORITHM_XDELTA_2,
  COMPRESS_ALGORITHM_XDELTA_3  = CHUNK_CONST_COMPRESS_ALGORITHM_XDELTA_3,
  COMPRESS_ALGORITHM_XDELTA_4  = CHUNK_CONST_COMPRESS_ALGORITHM_XDELTA_4,
  COMPRESS_ALGORITHM_XDELTA_5  = CHUNK_CONST_COMPRESS_ALGORITHM_XDELTA_5,
  COMPRESS_ALGORITHM_XDELTA_6  = CHUNK_CONST_COMPRESS_ALGORITHM_XDELTA_6,
  COMPRESS_ALGORITHM_XDELTA_7  = CHUNK_CONST_COMPRESS_ALGORITHM_XDELTA_7,
  COMPRESS_ALGORITHM_XDELTA_8  = CHUNK_CONST_COMPRESS_ALGORITHM_XDELTA_8,
  COMPRESS_ALGORITHM_XDELTA_9  = CHUNK_CONST_COMPRESS_ALGORITHM_XDELTA_9,

  COMPRESS_ALGORITHM_UNKNOWN = 0xFFFF,
} CompressAlgorithms;

typedef enum
{
  COMPRESS_BLOCK_TYPE_ANY,                      // any blocks
  COMPRESS_BLOCK_TYPE_FULL                      // full blocks (=block length)
} CompressBlockTypes;

/***************************** Datatypes *******************************/

// compress info block
typedef struct
{
  CompressModes      compressMode;              // mode: deflate (compress)/inflate (decompress)
  CompressAlgorithms compressAlgorithm;         // compression algorithm to use
  ulong              blockLength;               // block length to use [bytes]

  CompressStates     compressState;             // compress/decompress state
  bool               endOfDataFlag;             // TRUE if end-of-data detected
  bool               flushFlag;                 // TRUE for flushing all buffers

  union
  {
    struct
    {
      uint64 totalBytes;                        // total number of processed bytes
    } none;
    struct
    {
      z_stream stream;
    } zlib;
    #ifdef HAVE_BZ2
      struct
      {
        uint      compressionLevel;             // used compression level (needed for reset)
        bz_stream stream;                       // BZIP2 stream
      } bzlib;
    #endif /* HAVE_BZ2 */
    #ifdef HAVE_LZMA
      struct
      {
        uint        compressionLevel;           // used compression level (needed for reset)
        lzma_stream stream;                     // LZMA stream
      } lzmalib;
    #endif /* HAVE_LZMA */
    #ifdef HAVE_XDELTA
      struct
      {
        #ifdef HAVE_XDELTA3
          void       *sourceHandle;             // source handle
          byte       *sourceBuffer;             // buffer for source
          byte       *outputBuffer;             // buffer for output (allocated if NULL)
          ulong      outputBufferLength;        // number of bytes in output buffer
          ulong      outputBufferSize;          // size of output buffer (reallocated if 0 or to small)
          int        flags;                     // XDELTA flags
          xd3_stream stream;                    // XDELTA stream
          xd3_source source;                    // XDELTA source
          byte       inputBuffer[1];            /* buffer for next input byte (Note: do not use
                                                   pointer to dataBuffer/compressBuffer because
                                                   input/output is not processed immediately and must
                                                   be available until next input byte is requested
                                                */
          bool       flushFlag;                 // TRUE iff flush send to xdelta compressor
        #endif /* HAVE_XDELTA3 */
      } xdelta;
    #endif /* HAVE_XDELTA */
  };

  byte               *dataBuffer;               // buffer for uncompressed data
  ulong              dataBufferIndex;           // position of next byte in uncompressed data buffer
  ulong              dataBufferLength;          // length of data in uncompressed data buffer
  ulong              dataBufferSize;            // size of uncompressed data buffer

  byte               *compressBuffer;           // buffer for compressed data
  ulong              compressBufferIndex;       // position of next byte in compressed data buffer
  ulong              compressBufferLength;      // length of data in compressed data buffer
  ulong              compressBufferSize;        // size of compressed data buffer
} CompressInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : COMPRESS_NUMBER_TO_ALGORITHM
* Purpose: convert archive definition constant to algorithm enum value
* Input  : n - number
* Output : -
* Return : compress algorithm
* Notes  : -
\***********************************************************************/

#define COMPRESS_CONSTANT_TO_ALGORITHM(n) \
  ((CompressAlgorithms)(n))

/***********************************************************************\
* Name   : COMPRESS_ALGORITHM_TO_NUMBER
* Purpose: convert algorithm enum value to archive definition constant
* Input  : compressAlgorithm - compress algorithm
* Output : -
* Return : number
* Notes  : -
\***********************************************************************/

#define COMPRESS_ALGORITHM_TO_CONSTANT(compressAlgorithm) \
  ((uint16)(compressAlgorithm))

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
* Name   : Compress_isCompressed
* Purpose: check if compressed
* Input  : compressAlgorithm - compress algorithm
* Output : -
* Return : TRUE iff compressed, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool Compress_isCompressed(CompressAlgorithms compressAlgorithm);
#if defined(NDEBUG) || defined(__COMPRESS_IMPLEMENATION__)
INLINE bool Compress_isCompressed(CompressAlgorithms compressAlgorithm)
{
  return compressAlgorithm != COMPRESS_ALGORITHM_NONE;
}
#endif /* NDEBUG || __COMPRESS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Compress_isZIPCompressed
* Purpose: check if ZIP algorithm
* Input  : compressAlgorithm - compress algorithm
* Output : -
* Return : TRUE iff ZIP compress algorithm, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool Compress_isZIPCompressed(CompressAlgorithms compressAlgorithm);
#if defined(NDEBUG) || defined(__COMPRESS_IMPLEMENATION__)
INLINE bool Compress_isZIPCompressed(CompressAlgorithms compressAlgorithm)
{
  return (COMPRESS_ALGORITHM_ZIP_0 <= compressAlgorithm) && (compressAlgorithm <= COMPRESS_ALGORITHM_ZIP_9);
}
#endif /* NDEBUG || __COMPRESS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Compress_isBZIP2Compressed
* Purpose: check if BZIP2 algorithm
* Input  : compressAlgorithm - compress algorithm
* Output : -
* Return : TRUE iff BZIP2 compress algorithm, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool Compress_isBZIP2Compressed(CompressAlgorithms compressAlgorithm);
#if defined(NDEBUG) || defined(__COMPRESS_IMPLEMENATION__)
INLINE bool Compress_isBZIP2Compressed(CompressAlgorithms compressAlgorithm)
{
  return (COMPRESS_ALGORITHM_BZIP2_1 <= compressAlgorithm) && (compressAlgorithm <= COMPRESS_ALGORITHM_BZIP2_9);
}
#endif /* NDEBUG || __COMPRESS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Compress_isLZMACompressed
* Purpose: check if LZMA algorithm
* Input  : compressAlgorithm - compress algorithm
* Output : -
* Return : TRUE iff LZMA compress algorithm, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool Compress_isLZMACompressed(CompressAlgorithms compressAlgorithm);
#if defined(NDEBUG) || defined(__COMPRESS_IMPLEMENATION__)
INLINE bool Compress_isLZMACompressed(CompressAlgorithms compressAlgorithm)
{
  return (COMPRESS_ALGORITHM_LZMA_1 <= compressAlgorithm) && (compressAlgorithm <= COMPRESS_ALGORITHM_LZMA_9);
}
#endif /* NDEBUG || __COMPRESS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Compress_isXDeltaCompressed
* Purpose: check if XDELTA algorithm
* Input  : compressAlgorithm - compress algorithm
* Output : -
* Return : TRUE iff XDELTA compress algorithm, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool Compress_isXDeltaCompressed(CompressAlgorithms compressAlgorithm);
#if defined(NDEBUG) || defined(__COMPRESS_IMPLEMENATION__)
INLINE bool Compress_isXDeltaCompressed(CompressAlgorithms compressAlgorithm)
{
  return (COMPRESS_ALGORITHM_XDELTA_1 <= compressAlgorithm) && (compressAlgorithm <= COMPRESS_ALGORITHM_XDELTA_9);
}
#endif /* NDEBUG || __COMPRESS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Compress_new
* Purpose: create new compress handle
* Input  : compressInfo     - compress info block
*          compressionLevel - compression level (0..9)
*          blockLength      - block length
*          sourceHandle     - source handle (can be NULL if no
*                             delta-compression is used)
* Output : compressInfo - initialized compress info block
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_new(CompressInfo       *compressInfo,
                    CompressModes      compressMode,
                    CompressAlgorithms compressAlgorithm,
                    ulong              blockLength,
                    SourceHandle       *sourceHandle
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
* Purpose: deflate (compress) data
* Input  : compressInfo - compress info block
*          data         - data byte to compress
* Output : deflatedBytes - number of processed data bytes (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_deflate(CompressInfo *compressInfo,
                        const byte   *data,
                        ulong        length,
                        ulong        *deflatedBytes
                       );

/***********************************************************************\
* Name   : Compress_inflate
* Purpose: inflate (decompress) data
* Input  : compressInfo - compress info block
* Output : data          - decompressed data
*          inflatedBytes - number of data bytes (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_inflate(CompressInfo *compressInfo,
                        byte         *data,
                        ulong        length,
                        ulong        *inflatedBytes
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
* Name   : Compress_isFlush
* Purpose: check if flush compress data is set
* Input  : compressInfo - compress info block
* Output : -
* Return : TRUE iff flush, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool Compress_isFlush(CompressInfo *compressInfo);
#if defined(NDEBUG) || defined(__COMPRESS_IMPLEMENATION__)
INLINE bool Compress_isFlush(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return compressInfo->flushFlag;
}
#endif /* defined(NDEBUG) || defined(__COMPRESS_IMPLEMENATION__) */

/***********************************************************************\
* Name   : Compress_isEndOfData
* Purpose: check if end of compress data
* Input  : compressInfo - compress info block
* Output : -
* Return : TRUE iff end of compress data, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool Compress_isEndOfData(CompressInfo *compressInfo);
#if defined(NDEBUG) || defined(__COMPRESS_IMPLEMENATION__)
INLINE bool Compress_isEndOfData(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return compressInfo->endOfDataFlag;
}
#endif /* defined(NDEBUG) || defined(__COMPRESS_IMPLEMENATION__) */

/***********************************************************************\
* Name   : Compress_getInputLength
* Purpose: get number of input bytes
* Input  : compressInfo - compress info block
* Output : -
* Return : number of input bytes
* Notes  : -
\***********************************************************************/

uint64 Compress_getInputLength(CompressInfo *compressInfo);

/***********************************************************************\
* Name   : Compress_getOutputLength
* Purpose: get number of output bytes
* Input  : compressInfo - compress info block
* Output : -
* Return : number of output bytes
* Notes  : -
\***********************************************************************/

uint64 Compress_getOutputLength(CompressInfo *compressInfo);

/***********************************************************************\
* Name   : Compress_getAvailableDecompressedBytes
* Purpose: decompress data and get number of available bytes in
*          decompressor
* Input  : compressInfo - compress info block
* Output : bytes - number of available bytes
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_getAvailableDecompressedBytes(CompressInfo *compressInfo,
                                              ulong        *bytes
                                             );

/***********************************************************************\
* Name   : Compress_getAvailableCompressedBlocks
* Purpose: compress data and get number of available compressed blocks
*          in compressor
* Input  : compressInfo - compress info block
* Output : blockCount - number of available (full) blocks
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_getAvailableCompressedBlocks(CompressInfo       *compressInfo,
                                             CompressBlockTypes blockType,
                                             uint               *blockCount
                                            );

#if 0
/***********************************************************************\
* Name   : Compress_checkEndOfBlock
* Purpose: check end of block reached
* Input  : compressInfo - compress info block
* Output : -
* Return : TRUE at end of block, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Compress_checkEndOfBlock(CompressInfo *compressInfo);
#endif /* 0 */

#if 0
obsolete
//????
/***********************************************************************\
* Name   : Compress_getCompressedByte
* Purpose: get next compressed byte
* Input  : compressInfo - compress info block
* Output : buffer - buffer with compressed byte
* Return : TRUE iff compressed is read, FALSE otherwise
* Notes  : -
\***********************************************************************/

// replace by Compress_getCompressedBlock? compressed data is always organized in blocks?
bool Compress_getCompressedByte(CompressInfo *compressInfo,
                                byte         *buffer
                               );
#endif /* 0 */

/***********************************************************************\
* Name   : Compress_getCompressedBlock
* Purpose: compress data and get next compressed data block from
*          compressor
* Input  : compressInfo - compress info block
* Output : buffer       - data
*          bufferLength - number of bytes in block
* Return : -
* Notes  : buffer size have to be at least blockLength!
\***********************************************************************/

void Compress_getCompressedBlock(CompressInfo *compressInfo,
                                 byte         *buffer,
                                 ulong        *bufferLength
                                );

/***********************************************************************\
* Name   : Compress_putCompressedBlock
* Purpose: put compressed block data into decompressor
* Input  : compressInfo - compress info block
*          buffer       - data
*          bufferLength - length of data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Compress_putCompressedBlock(CompressInfo *compressInfo,
                                 void         *buffer,
                                 ulong        bufferLength
                                );

#ifdef __cplusplus
  }
#endif

#endif /* __COMPRESS__ */

/* end of file */
