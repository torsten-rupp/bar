/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver compress functions
* Systems: all
*
\***********************************************************************/

#ifndef __COMPRESS__
#define __COMPRESS__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

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
#include "ringbuffers.h"

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
          SourceHandle *sourceHandle;           // delta source handle
          byte         *sourceBuffer;           // buffer for delta source data
          RingBuffer   outputRingBuffer;
          int          flags;                   // XDELTA flags
          xd3_stream   stream;                  // XDELTA stream
          xd3_source   source;                  // XDELTA source
          byte         inputBuffer[1024];       /* buffer for next xdelta input data bytes (Note: do
                                                   not use pointer to data/compress ring buffers
                                                   because input/output is not processed immediately
                                                   and must be available until next input data is
                                                   requested
                                                */
          bool         flushFlag;               // TRUE iff flush send to xdelta compressor
        #endif /* HAVE_XDELTA3 */
      } xdelta;
    #endif /* HAVE_XDELTA */
  };

  RingBuffer         dataRingBuffer;            // buffer for uncompressed data
  RingBuffer         compressRingBuffer;        // buffer for compressed data
} CompressInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : COMPRESS_CONSTANT_TO_ALGORITHM
* Purpose: convert archive definition constant to algorithm enum value
* Input  : n - number
* Output : -
* Return : compress algorithm
* Notes  : -
\***********************************************************************/

#define COMPRESS_CONSTANT_TO_ALGORITHM(n) \
  ((CompressAlgorithms)(n))

/***********************************************************************\
* Name   : COMPRESS_ALGORITHM_TO_CONSTANT
* Purpose: convert algorithm enum value to archive definition constant
* Input  : compressAlgorithm - compress algorithm
* Output : -
* Return : number
* Notes  : -
\***********************************************************************/

#define COMPRESS_ALGORITHM_TO_CONSTANT(compressAlgorithm) \
  ((uint16)(compressAlgorithm))

#ifndef NDEBUG
  #define Compress_init(...) __Compress_init(__FILE__,__LINE__,__VA_ARGS__)
  #define Compress_done(...) __Compress_done(__FILE__,__LINE__,__VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Compress_initAll
* Purpose: initialize compress functions
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_initAll(void);

/***********************************************************************\
* Name   : Compress_doneAll
* Purpose: deinitialize compress functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Compress_doneAll(void);

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
* Name   : Compress_isValidAlgorithm
* Purpose: check if valid compress algoritm
* Input  : n - compress algorithm constant
* Output : -
* Return : TRUE iff valid, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Compress_isValidAlgorithm(uint16 n);

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
* Name   : Compress_init
* Purpose: init compress handle
* Input  : compressInfo     - compress info block
*          compressionLevel - compression level (0..9)
*          blockLength      - block length
*          sourceHandle     - source handle (can be NULL if no
*                             delta-compression is used)
* Output : compressInfo - initialized compress info block
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Compress_init(CompressInfo       *compressInfo,
                       CompressModes      compressMode,
                       CompressAlgorithms compressAlgorithm,
                       ulong              blockLength,
                       SourceHandle       *sourceHandle
                      );
#else /* not NDEBUG */
  Errors __Compress_init(const char         *__fileName__,
                         ulong              __lineNb__,
                         CompressInfo       *compressInfo,
                         CompressModes      compressMode,
                         CompressAlgorithms compressAlgorithm,
                         ulong              blockLength,
                         SourceHandle       *sourceHandle
                        );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Compress_done
* Purpose: done compress handle
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Compress_done(CompressInfo *compressInfo);
#else /* not NDEBUG */
  void __Compress_done(const char   *__fileName__,
                       ulong        __lineNb__,
                       CompressInfo *compressInfo
                      );
#endif /* NDEBUG */

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
*          buffer        - buffer with data to compress
*          bufferLength  - length of data in buffer
* Output : deflatedBytes - number of processed data bytes (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_deflate(CompressInfo *compressInfo,
                        const byte   *buffer,
                        ulong        bufferLength,
                        ulong        *deflatedBytes
                       );

/***********************************************************************\
* Name   : Compress_inflate
* Purpose: inflate (decompress) data
* Input  : compressInfo - compress info block
*          buffer       - buffer for decompressed data
*          bufferSize   - size of buffer
* Output : buffer        - buffer with decompressed data
*          inflatedBytes - number of decompressed bytes in buffer
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_inflate(CompressInfo *compressInfo,
                        byte         *buffer,
                        ulong        bufferSize,
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
* Name   : Compress_getFreeDataSpace
* Purpose: get free space in data buffer
* Input  : compressInfo - compress info block
* Output : -
* Return : number of bytes free in data buffer
* Notes  : -
\***********************************************************************/

INLINE ulong Compress_getFreeDataSpace(const CompressInfo *compressInfo);
#if defined(NDEBUG) || defined(__COMPRESS_IMPLEMENATION__)
INLINE ulong Compress_getFreeDataSpace(const CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return RingBuffer_getFree(&compressInfo->dataRingBuffer);
}
#endif /* NDEBUG || __COMPRESS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Compress_getFreeCompressSpace
* Purpose: get free space in compress buffer
* Input  : compressInfo - compress info block
* Output : -
* Return : number of bytes free in compress buffer
* Notes  : -
\***********************************************************************/

INLINE ulong Compress_getFreeCompressSpace(const CompressInfo *compressInfo);
#if defined(NDEBUG) || defined(__COMPRESS_IMPLEMENATION__)
INLINE ulong Compress_getFreeCompressSpace(const CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return RingBuffer_getFree(&compressInfo->compressRingBuffer);
}
#endif /* NDEBUG || __COMPRESS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Compress_isFreeDataSpace
* Purpose: check if there is free space in data buffer
* Input  : compressInfo - compress info block
* Output : -
* Return : TRUE iff free space in data buffer
* Notes  : -
\***********************************************************************/

INLINE bool Compress_isFreeDataSpace(const CompressInfo *compressInfo);
#if defined(NDEBUG) || defined(__COMPRESS_IMPLEMENATION__)
INLINE bool Compress_isFreeDataSpace(const CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return !RingBuffer_isFull(&compressInfo->dataRingBuffer);
}
#endif /* NDEBUG || __COMPRESS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Compress_isFreeCompressSpace
* Purpose: check if there is free space in compress buffer
* Input  : compressInfo - compress info block
* Output : -
* Return : TRUE iff free space in compress buffer
* Notes  : -
\***********************************************************************/

INLINE bool Compress_isFreeCompressSpace(const CompressInfo *compressInfo);
#if defined(NDEBUG) || defined(__COMPRESS_IMPLEMENATION__)
INLINE bool Compress_isFreeCompressSpace(const CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return !RingBuffer_isFull(&compressInfo->compressRingBuffer);
}
#endif /* NDEBUG || __COMPRESS_IMPLEMENATION__ */

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
*          blockType    - block type; see CompressBlockTypes
* Output : blockCount - number of available blocks
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_getAvailableCompressedBlocks(CompressInfo       *compressInfo,
                                             CompressBlockTypes blockType,
                                             uint               *blockCount
                                            );

/***********************************************************************\
* Name   : Compress_getCompressedData
* Purpose: compress data and get next compressed data bytes from
*          compressor
* Input  : compressInfo - compress info block
*          buffer       - buffer for data
*          bufferSize   - buffer size (must be >= block length)
* Output : buffer       - compressed data (rest in last block is filled with
*                         0)
*          bufferLength - number of bytes in buffer (always a multiple
*                         of block length)
* Return : -
* Notes  : -
\***********************************************************************/

void Compress_getCompressedData(CompressInfo *compressInfo,
                                byte         *buffer,
                                ulong        bufferSize,
                                ulong        *bufferLength
                               );

/***********************************************************************\
* Name   : Compress_putCompressedData
* Purpose: decompress data and put next compressed data bytes into
*          decompressor
* Input  : compressInfo - compress info block
*          buffer       - data
*          length       - length of data (must always fit into compress
*                         buffer!)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Compress_putCompressedData(CompressInfo *compressInfo,
                                const void   *buffer,
                                ulong        bufferLength
                               );

#ifdef __cplusplus
  }
#endif

#endif /* __COMPRESS__ */

/* end of file */
