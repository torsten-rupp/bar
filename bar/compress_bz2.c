/***********************************************************************\
*
* $Revision: 3071 $
* $Date: 2014-02-22 20:20:41 +0100 (Sat, 22 Feb 2014) $
* $Author: trupp $
* Contents: Backup ARchiver compress functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <bzlib.h>
#include <assert.h>

#include "common/global.h"
#include "common/ringbuffers.h"
#include "common/lists.h"
#include "common/files.h"

#include "errors.h"
#include "entrylists.h"
#include "common/patternlists.h"
#include "storage.h"

#include "compress.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : CompressBZ2_compressData
* Purpose: compress data with bzip2
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors CompressBZ2_compressData(CompressInfo *compressInfo)
{
  ulong maxCompressBytes,maxDataBytes;
  int   bzlibResult;

  assert(compressInfo != NULL);

  if (!compressInfo->endOfDataFlag)                                           // not end-of-data
  {
    if (!RingBuffer_isFull(&compressInfo->compressRingBuffer))                // space in compress buffer
    {
      // compress available data
      if (!RingBuffer_isEmpty(&compressInfo->dataRingBuffer))                 // unprocessed data available
      {
        // get max. number of data and max. number of compressed bytes
        maxDataBytes     = RingBuffer_getAvailable(&compressInfo->dataRingBuffer);
        maxCompressBytes = RingBuffer_getFree(&compressInfo->compressRingBuffer);

        // compress: transfer data buffer -> compress buffer
        compressInfo->bzlib.stream.next_in   = (char*)RingBuffer_cArrayOut(&compressInfo->dataRingBuffer);
        compressInfo->bzlib.stream.avail_in  = maxDataBytes;
        compressInfo->bzlib.stream.next_out  = (char*)RingBuffer_cArrayIn(&compressInfo->compressRingBuffer);
        compressInfo->bzlib.stream.avail_out = maxCompressBytes;
        bzlibResult = BZ2_bzCompress(&compressInfo->bzlib.stream,BZ_RUN);
//fprintf(stderr,"%s, %d: bzlibResult=%d input=%u,%u output=%u,%u\n",__FILE__,__LINE__,bzlibResult,maxDataBytes-compressInfo->bzlib.stream.avail_in,maxDataBytes,maxCompressBytes-compressInfo->bzlib.stream.avail_out,maxCompressBytes);
        if (bzlibResult != BZ_RUN_OK)
        {
          return ERROR_(DEFLATE_FAIL,bzlibResult);
        }
        RingBuffer_decrement(&compressInfo->dataRingBuffer,
                             maxDataBytes-compressInfo->bzlib.stream.avail_in
                            );
        RingBuffer_increment(&compressInfo->compressRingBuffer,
                             maxCompressBytes-compressInfo->bzlib.stream.avail_out
                            );

        // update compress state
        compressInfo->compressState = COMPRESS_STATE_RUNNING;
      }
    }

    if (!RingBuffer_isFull(&compressInfo->compressRingBuffer))                // space in compress buffer
    {
      // finish compress, flush internal compress buffers
      if (   compressInfo->flushFlag                                          // flush data requested
          && (compressInfo->compressState == COMPRESS_STATE_RUNNING)          // compressor is running -> data available in internal buffers
         )
      {
        // get max. number of compressed bytes
        maxCompressBytes = RingBuffer_getFree(&compressInfo->compressRingBuffer);

        // compress with flush: transfer to compress buffer
        compressInfo->bzlib.stream.next_in   = NULL;
        compressInfo->bzlib.stream.avail_in  = 0;
        compressInfo->bzlib.stream.next_out  = (char*)RingBuffer_cArrayIn(&compressInfo->compressRingBuffer);
        compressInfo->bzlib.stream.avail_out = maxCompressBytes;
        bzlibResult = BZ2_bzCompress(&compressInfo->bzlib.stream,BZ_FINISH);
//fprintf(stderr,"%s, %d: bzlibResult=%d output=%u,%u\n",__FILE__,__LINE__,bzlibResult,maxCompressBytes-compressInfo->bzlib.stream.avail_out,maxCompressBytes);
        if      (bzlibResult == BZ_STREAM_END)
        {
          compressInfo->endOfDataFlag = TRUE;
        }
        else if (bzlibResult != BZ_FINISH_OK)
        {
          return ERROR_(DEFLATE_FAIL,bzlibResult);
        }
        RingBuffer_increment(&compressInfo->compressRingBuffer,
                             maxCompressBytes-compressInfo->bzlib.stream.avail_out
                            );
      }
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : CompressBZ2_decompressData
* Purpose: decompress data with bzip2
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors CompressBZ2_decompressData(CompressInfo *compressInfo)
{
  int bzlibResult;

  ulong maxCompressBytes,maxDataBytes;

  assert(compressInfo != NULL);

  if (!compressInfo->endOfDataFlag)                                           // not end-of-data
  {
    if (!RingBuffer_isFull(&compressInfo->dataRingBuffer))                    // space in data buffer
    {
      // decompress available data
      if (!RingBuffer_isEmpty(&compressInfo->compressRingBuffer))             // unprocessed compressed data available
      {
        // get max. number of compressed and max. number of data bytes
        maxCompressBytes = RingBuffer_getAvailable(&compressInfo->compressRingBuffer);
        maxDataBytes     = RingBuffer_getFree(&compressInfo->dataRingBuffer);

        // decompress: transfer compress buffer -> data buffer
        compressInfo->bzlib.stream.next_in   = (char*)RingBuffer_cArrayOut(&compressInfo->compressRingBuffer);
        compressInfo->bzlib.stream.avail_in  = maxCompressBytes;
        compressInfo->bzlib.stream.next_out  = (char*)RingBuffer_cArrayIn(&compressInfo->dataRingBuffer);
        compressInfo->bzlib.stream.avail_out = maxDataBytes;
        bzlibResult = BZ2_bzDecompress(&compressInfo->bzlib.stream);
//fprintf(stderr,"%s, %d: bzlibResult=%d input=%u,%u output=%u,%u\n",__FILE__,__LINE__,bzlibResult,maxCompressBytes-compressInfo->bzlib.stream.avail_in,maxCompressBytes,maxDataBytes-compressInfo->bzlib.stream.avail_out,maxDataBytes);
        if      (bzlibResult == BZ_STREAM_END)
        {
          compressInfo->endOfDataFlag = TRUE;
        }
        else if (bzlibResult != BZ_OK)
        {
          return ERROR_(INFLATE_FAIL,bzlibResult);
        }
        RingBuffer_decrement(&compressInfo->compressRingBuffer,
                             maxCompressBytes-compressInfo->bzlib.stream.avail_in
                            );
        RingBuffer_increment(&compressInfo->dataRingBuffer,
                             maxDataBytes-compressInfo->bzlib.stream.avail_out
                            );

        // update compress state
        compressInfo->compressState = COMPRESS_STATE_RUNNING;
      }
    }
  }

  if (!compressInfo->endOfDataFlag)                                           // not end-of-data
  {
    if (RingBuffer_isEmpty(&compressInfo->dataRingBuffer))                    // no data in data buffer
    {
      // finish decompress, flush internal decompress buffers
      if (   compressInfo->flushFlag                                          // flush data requested
          && (compressInfo->compressState == COMPRESS_STATE_RUNNING)          // compressor is running -> data available in internal buffers
         )
      {
        // get max. number of data bytes
        maxDataBytes = RingBuffer_getFree(&compressInfo->dataRingBuffer);

        // decompress with flush: transfer rest of internal data -> data buffer
        compressInfo->bzlib.stream.next_in   = NULL;
        compressInfo->bzlib.stream.avail_in  = 0;
        compressInfo->bzlib.stream.next_out  = (char*)RingBuffer_cArrayIn(&compressInfo->dataRingBuffer);
        compressInfo->bzlib.stream.avail_out = maxDataBytes;
        bzlibResult = BZ2_bzDecompress(&compressInfo->bzlib.stream);
//fprintf(stderr,"%s, %d: bzlibResult=%d output=%u,%u\n",__FILE__,__LINE__,bzlibResult,maxDataBytes-compressInfo->bzlib.stream.avail_out,maxDataBytes);
        if      (bzlibResult == BZ_STREAM_END)
        {
          compressInfo->endOfDataFlag = TRUE;
        }
        else if (bzlibResult != BZ_RUN_OK)
        {
          return ERROR_(INFLATE_FAIL,bzlibResult);
        }
        RingBuffer_increment(&compressInfo->dataRingBuffer,
                             maxDataBytes-compressInfo->bzlib.stream.avail_out
                            );
      }
    }
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

LOCAL Errors CompressBZ2_init(CompressInfo       *compressInfo,
                              CompressModes      compressMode,
                              CompressAlgorithms compressAlgorithm
                             )
{
  int bz2Result;

  assert(compressInfo != NULL);

  compressInfo->bzlib.compressionLevel = 0;
  switch (compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_BZIP2_1: compressInfo->bzlib.compressionLevel = 1; break;
    case COMPRESS_ALGORITHM_BZIP2_2: compressInfo->bzlib.compressionLevel = 2; break;
    case COMPRESS_ALGORITHM_BZIP2_3: compressInfo->bzlib.compressionLevel = 3; break;
    case COMPRESS_ALGORITHM_BZIP2_4: compressInfo->bzlib.compressionLevel = 4; break;
    case COMPRESS_ALGORITHM_BZIP2_5: compressInfo->bzlib.compressionLevel = 5; break;
    case COMPRESS_ALGORITHM_BZIP2_6: compressInfo->bzlib.compressionLevel = 6; break;
    case COMPRESS_ALGORITHM_BZIP2_7: compressInfo->bzlib.compressionLevel = 7; break;
    case COMPRESS_ALGORITHM_BZIP2_8: compressInfo->bzlib.compressionLevel = 8; break;
    case COMPRESS_ALGORITHM_BZIP2_9: compressInfo->bzlib.compressionLevel = 9; break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  compressInfo->bzlib.stream.bzalloc = NULL;
  compressInfo->bzlib.stream.bzfree  = NULL;
  compressInfo->bzlib.stream.opaque  = NULL;
  switch (compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      bz2Result = BZ2_bzCompressInit(&compressInfo->bzlib.stream,compressInfo->bzlib.compressionLevel,0,0);
      if (bz2Result != BZ_OK)
      {
        return ERRORX_(INIT_COMPRESS,bz2Result,NULL);
      }
      break;
    case COMPRESS_MODE_INFLATE:
      bz2Result = BZ2_bzDecompressInit(&compressInfo->bzlib.stream,0,0);
      if (bz2Result != BZ_OK)
      {
        return ERRORX_(INIT_DECOMPRESS,bz2Result,NULL);
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

LOCAL void CompressBZ2_done(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  switch (compressInfo->compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      BZ2_bzCompressEnd(&compressInfo->bzlib.stream);
      break;
    case COMPRESS_MODE_INFLATE:
      BZ2_bzDecompressEnd(&compressInfo->bzlib.stream);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
}

LOCAL Errors CompressBZ2_reset(CompressInfo *compressInfo)
{
  int bzlibResult;

  assert(compressInfo != NULL);

  bzlibResult = BZ_PARAM_ERROR;
  switch (compressInfo->compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      BZ2_bzCompressEnd(&compressInfo->bzlib.stream);
      bzlibResult = BZ2_bzCompressInit(&compressInfo->bzlib.stream,compressInfo->bzlib.compressionLevel,0,0);
      if (bzlibResult != BZ_OK)
      {
        return ERROR_(DEFLATE_FAIL,bzlibResult);
      }
      break;
    case COMPRESS_MODE_INFLATE:
      BZ2_bzDecompressEnd(&compressInfo->bzlib.stream);
      bzlibResult = BZ2_bzDecompressInit(&compressInfo->bzlib.stream,0,0);
      if (bzlibResult != BZ_OK)
      {
        return ERROR_(INFLATE_FAIL,bzlibResult);
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

LOCAL uint64 CompressBZ2_getInputLength(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return ((uint64)compressInfo->bzlib.stream.total_in_hi32 << 32) | ((uint64)compressInfo->bzlib.stream.total_in_lo32 << 0);
}

LOCAL uint64 CompressBZ2_getOutputLength(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return ((uint64)compressInfo->bzlib.stream.total_out_hi32) | ((uint64)compressInfo->bzlib.stream.total_out_lo32 << 0);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
