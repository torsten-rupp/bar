/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/compress.h,v $
* $Revision: 1.3 $
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
#ifdef HAVE_LZMA
  #include <lzma.h>
#endif /* HAVE_LZMA */
#ifdef HAVE_XDELTA3
  #include "xdelta3.h"
#endif /* HAVE_XDELTA */
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "errors.h"

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
  COMPRESS_ALGORITHM_NONE,

  COMPRESS_ALGORITHM_ZIP_0     = 1,
  COMPRESS_ALGORITHM_ZIP_1     = 2,
  COMPRESS_ALGORITHM_ZIP_2     = 3,
  COMPRESS_ALGORITHM_ZIP_3     = 4,
  COMPRESS_ALGORITHM_ZIP_4     = 5,
  COMPRESS_ALGORITHM_ZIP_5     = 6,
  COMPRESS_ALGORITHM_ZIP_6     = 7,
  COMPRESS_ALGORITHM_ZIP_7     = 8,
  COMPRESS_ALGORITHM_ZIP_8     = 9,
  COMPRESS_ALGORITHM_ZIP_9     = 10,

  COMPRESS_ALGORITHM_BZIP2_1   = 11,
  COMPRESS_ALGORITHM_BZIP2_2   = 12,
  COMPRESS_ALGORITHM_BZIP2_3   = 13,
  COMPRESS_ALGORITHM_BZIP2_4   = 14,
  COMPRESS_ALGORITHM_BZIP2_5   = 15,
  COMPRESS_ALGORITHM_BZIP2_6   = 16,
  COMPRESS_ALGORITHM_BZIP2_7   = 18,
  COMPRESS_ALGORITHM_BZIP2_8   = 19,
  COMPRESS_ALGORITHM_BZIP2_9   = 20,

  COMPRESS_ALGORITHM_LZMA_1    = 21,
  COMPRESS_ALGORITHM_LZMA_2    = 22,
  COMPRESS_ALGORITHM_LZMA_3    = 23,
  COMPRESS_ALGORITHM_LZMA_4    = 24,
  COMPRESS_ALGORITHM_LZMA_5    = 25,
  COMPRESS_ALGORITHM_LZMA_6    = 26,
  COMPRESS_ALGORITHM_LZMA_7    = 27,
  COMPRESS_ALGORITHM_LZMA_8    = 28,
  COMPRESS_ALGORITHM_LZMA_9    = 29,

  COMPRESS_ALGORITHM_XDELTA_1  = 30,
  COMPRESS_ALGORITHM_XDELTA_2  = 31,
  COMPRESS_ALGORITHM_XDELTA_3  = 32,
  COMPRESS_ALGORITHM_XDELTA_4  = 33,
  COMPRESS_ALGORITHM_XDELTA_5  = 34,
  COMPRESS_ALGORITHM_XDELTA_6  = 35,
  COMPRESS_ALGORITHM_XDELTA_7  = 36,
  COMPRESS_ALGORITHM_XDELTA_8  = 37,
  COMPRESS_ALGORITHM_XDELTA_9  = 38,

  COMPRESS_ALGORITHM_UNKNOWN = 0xFFFF,
} CompressAlgorithms;

typedef enum
{
  COMPRESS_BLOCK_TYPE_ANY,                      // any blocks
  COMPRESS_BLOCK_TYPE_FULL                      // full blocks (=block length)
} CompressBlockTypes;

/***************************** Datatypes *******************************/

typedef Errors(*CompressSourceGetEntryDataBlock)(void   *userData,
                                                 void   *buffer,
                                                 uint64 offset,
                                                 ulong  length,
                                                 ulong  *bytesRead
                                                );

/* compress info block */
typedef struct
{
  CompressModes      compressMode;              // mode: compress/decompress
  CompressAlgorithms compressAlgorithm;         // compression algorithm to use
  ulong              blockLength;               // block length to use [bytes]

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
          CompressSourceGetEntryDataBlock sourceGetEntryDataBlock;
          void                            *sourceGetEntryDataBlockUserData;
          bool                            done;               // TRUE iff compression is done
          byte                            *sourceBuffer;      // buffer for source
          byte                            *outputBuffer;      // buffer for output (allocated if NULL)
          ulong                           outputBufferLength; // number of bytes in output buffer
          ulong                           outputBufferSize;   // size of output buffer (reallocated if 0 or to small)
          int                             flags;              // XDELTA flags
          xd3_stream                      stream;             // XDELTA stream
          xd3_source                      source;             // XDELTA source
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
* Name   : COMPRESS_IS_ZIP_ALGORITHM
* Purpose: check if ZIP algorithm
* Input  : algorithm - algorithm
* Output : -
* Return : TRUE iff ZIP algorithm, FALSE otherwise
* Notes  : -
\***********************************************************************/

#define COMPRESS_IS_ZIP_ALGORITHM(algorithm) \
  ((COMPRESS_ALGORITHM_ZIP_0 <= (algorithm)) && ((algorithm) <= COMPRESS_ALGORITHM_ZIP_9))

/***********************************************************************\
* Name   : COMPRESS_IS_BZIP2_ALGORITHM
* Purpose: check if BZIP2 algorithm
* Input  : algorithm - algorithm
* Output : -
* Return : TRUE iff BZIP2 algorithm, FALSE otherwise
* Notes  : -
\***********************************************************************/

#define COMPRESS_IS_BZIP2_ALGORITHM(algorithm) \
  ((COMPRESS_ALGORITHM_BZIP2_1 <= (algorithm)) && ((algorithm) <= COMPRESS_ALGORITHM_BZIP2_9))

/***********************************************************************\
* Name   : COMPRESS_IS_LZMA_ALGORITHM
* Purpose: check if LZMA algorithm
* Input  : algorithm - algorithm
* Output : -
* Return : TRUE iff LZMA algorithm, FALSE otherwise
* Notes  : -
\***********************************************************************/

#define COMPRESS_IS_LZMA_ALGORITHM(algorithm) \
  ((COMPRESS_ALGORITHM_LZMA_1 <= (algorithm)) && ((algorithm) <= COMPRESS_ALGORITHM_LZMA_9))

/***********************************************************************\
* Name   : COMPRESS_IS_XDELTA_ALGORITHM
* Purpose: check if XDELTA algorithm
* Input  : algorithm - algorithm
* Output : -
* Return : TRUE iff XDELTA algorithm, FALSE otherwise
* Notes  : -
\***********************************************************************/

#define COMPRESS_IS_XDELTA_ALGORITHM(algorithm) \
  ((COMPRESS_ALGORITHM_XDELTA_1 <= (algorithm)) && ((algorithm) <= COMPRESS_ALGORITHM_XDELTA_9))

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
*          sourceEntryInfo  - source entry info (can be NULL, used for
*                             delta-compression)
* Output : compressInfo - initialized compress info block
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_new(CompressInfo                    *compressInfo,
                    CompressModes                   compressMode,
                    CompressAlgorithms              compressAlgorithm,
                    ulong                           blockLength,
                    CompressSourceGetEntryDataBlock sourceGetEntryDataBlock,
                    void                            *sourceGetEntryDataBlockUserData
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
* Output : deflatedBytes - number of processed data bytes
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
*          inflatedBytes - number of data bytes
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
* Name   : Compress_getAvailableBytes
* Purpose: get number of available bytes (decompressed bytes)
* Input  : compressInfo - compress info block
* Output :
* Return : number of available bytes
* Notes  : -
\***********************************************************************/

ulong Compress_getAvailableBytes(CompressInfo *compressInfo);

/***********************************************************************\
* Name   : Compress_availableBlocks
* Purpose: get number of available (full) blocks (compressed blocks)
* Input  : compressInfo - compress info block
* Output : -
* Return : number of available (full) blocks
* Notes  : -
\***********************************************************************/

uint Compress_getAvailableBlocks(CompressInfo *compressInfo, CompressBlockTypes blockType);

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

#endif /* __COMPRESS__ */

/* end of file */
