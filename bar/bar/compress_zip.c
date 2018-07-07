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
#include <zlib.h>
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
* Name   : CompressZIP_compressData
* Purpose: compress data with ZIP
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors CompressZIP_compressData(CompressInfo *compressInfo)
{
  ulong maxCompressBytes,maxDataBytes;
  int   zlibError;

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

        // compress: data buffer -> compress buffer
        compressInfo->zlib.stream.next_in   = (Bytef*)RingBuffer_cArrayOut(&compressInfo->dataRingBuffer);
        compressInfo->zlib.stream.avail_in  = maxDataBytes;
        compressInfo->zlib.stream.next_out  = (Bytef*)RingBuffer_cArrayIn(&compressInfo->compressRingBuffer);
        compressInfo->zlib.stream.avail_out = maxCompressBytes;
        zlibError = deflate(&compressInfo->zlib.stream,Z_NO_FLUSH);
        if (    (zlibError != Z_OK)
             && (zlibError != Z_BUF_ERROR)
           )
        {
          return ERROR_(DEFLATE_FAIL,zlibError);
        }
        RingBuffer_decrement(&compressInfo->dataRingBuffer,
                             maxDataBytes-compressInfo->zlib.stream.avail_in
                            );
        RingBuffer_increment(&compressInfo->compressRingBuffer,
                             maxCompressBytes-compressInfo->zlib.stream.avail_out
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
        compressInfo->zlib.stream.next_in   = NULL;
        compressInfo->zlib.stream.avail_in  = 0;
        compressInfo->zlib.stream.next_out  = (Bytef*)RingBuffer_cArrayIn(&compressInfo->compressRingBuffer);
        compressInfo->zlib.stream.avail_out = maxCompressBytes;
        zlibError = deflate(&compressInfo->zlib.stream,Z_FINISH);
        if      (zlibError == Z_STREAM_END)
        {
          compressInfo->endOfDataFlag = TRUE;
        }
        else if (   (zlibError != Z_OK)
                 && (zlibError != Z_BUF_ERROR)
                )
        {
          return ERROR_(DEFLATE_FAIL,zlibError);
        }
        RingBuffer_increment(&compressInfo->compressRingBuffer,
                             maxCompressBytes-compressInfo->zlib.stream.avail_out
                            );
      }
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : CompressZIP_decompressData
* Purpose: decompress data with ZIP
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors CompressZIP_decompressData(CompressInfo *compressInfo)
{
  int zlibResult;

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
        compressInfo->zlib.stream.next_in   = (Bytef*)RingBuffer_cArrayOut(&compressInfo->compressRingBuffer);
        compressInfo->zlib.stream.avail_in  = maxCompressBytes;
        compressInfo->zlib.stream.next_out  = (Bytef*)RingBuffer_cArrayIn(&compressInfo->dataRingBuffer);
        compressInfo->zlib.stream.avail_out = maxDataBytes;
        zlibResult = inflate(&compressInfo->zlib.stream,Z_NO_FLUSH);
        if      (zlibResult == Z_STREAM_END)
        {
          compressInfo->endOfDataFlag = TRUE;
        }
        else if (   (zlibResult != Z_OK)
                 && (zlibResult != Z_BUF_ERROR)
                )
        {
          return ERROR_(INFLATE_FAIL,zlibResult);
        }
        RingBuffer_decrement(&compressInfo->compressRingBuffer,
                             maxCompressBytes-compressInfo->zlib.stream.avail_in
                            );
        RingBuffer_increment(&compressInfo->dataRingBuffer,
                             maxDataBytes-compressInfo->zlib.stream.avail_out
                            );

        // update compress state
        compressInfo->compressState = COMPRESS_STATE_RUNNING;
      }
    }

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
        compressInfo->zlib.stream.next_in   = NULL;
        compressInfo->zlib.stream.avail_in  = 0;
        compressInfo->zlib.stream.next_out  = (Bytef*)RingBuffer_cArrayIn(&compressInfo->dataRingBuffer);
        compressInfo->zlib.stream.avail_out = maxDataBytes;
        zlibResult = inflate(&compressInfo->zlib.stream,Z_FINISH);
        if      (zlibResult == Z_STREAM_END)
        {
          compressInfo->endOfDataFlag = TRUE;
        }
        else if (   (zlibResult != Z_OK)
                 && (zlibResult != Z_BUF_ERROR)
                )
        {
          return ERROR_(INFLATE_FAIL,zlibResult);
        }
        RingBuffer_increment(&compressInfo->dataRingBuffer,
                             maxDataBytes-compressInfo->zlib.stream.avail_out
                            );
      }
    }
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

LOCAL Errors CompressZIP_init(CompressInfo       *compressInfo,
                              CompressModes      compressMode,
                              CompressAlgorithms compressAlgorithm
                             )
{
  int compressionLevel;
  int zlibResult;

  assert(compressInfo != NULL);

  compressionLevel = 0;
  switch (compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_ZIP_0: compressionLevel = 0; break;
    case COMPRESS_ALGORITHM_ZIP_1: compressionLevel = 1; break;
    case COMPRESS_ALGORITHM_ZIP_2: compressionLevel = 2; break;
    case COMPRESS_ALGORITHM_ZIP_3: compressionLevel = 3; break;
    case COMPRESS_ALGORITHM_ZIP_4: compressionLevel = 4; break;
    case COMPRESS_ALGORITHM_ZIP_5: compressionLevel = 5; break;
    case COMPRESS_ALGORITHM_ZIP_6: compressionLevel = 6; break;
    case COMPRESS_ALGORITHM_ZIP_7: compressionLevel = 7; break;
    case COMPRESS_ALGORITHM_ZIP_8: compressionLevel = 8; break;
    case COMPRESS_ALGORITHM_ZIP_9: compressionLevel = 9; break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  compressInfo->zlib.stream.zalloc = Z_NULL;
  compressInfo->zlib.stream.zfree  = Z_NULL;
  compressInfo->zlib.stream.opaque = Z_NULL;
  switch (compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      zlibResult = deflateInit(&compressInfo->zlib.stream,compressionLevel);
      if (zlibResult != Z_OK)
      {
        return ERRORX_(INIT_COMPRESS,zlibResult,"%s",zError(zlibResult));
      }
      break;
    case COMPRESS_MODE_INFLATE:
      zlibResult = inflateInit(&compressInfo->zlib.stream);
      if (zlibResult != Z_OK)
      {
        return ERRORX_(INIT_DECOMPRESS,zlibResult,"%s",zError(zlibResult));
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

LOCAL void CompressZIP_done(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  switch (compressInfo->compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      deflateEnd(&compressInfo->zlib.stream);
      break;
    case COMPRESS_MODE_INFLATE:
      inflateEnd(&compressInfo->zlib.stream);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
}

LOCAL Errors CompressZIP_reset(CompressInfo *compressInfo)
{
  int zlibResult;

  assert(compressInfo != NULL);

  zlibResult = Z_ERRNO;
  switch (compressInfo->compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      zlibResult = deflateReset(&compressInfo->zlib.stream);
      if ((zlibResult != Z_OK) && (zlibResult != Z_STREAM_END))
      {
        return ERROR_(DEFLATE_FAIL,zlibResult);
      }
      break;
    case COMPRESS_MODE_INFLATE:
      zlibResult = inflateReset(&compressInfo->zlib.stream);
      if ((zlibResult != Z_OK) && (zlibResult != Z_STREAM_END))
      {
        return ERROR_(INFLATE_FAIL,zlibResult);
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

LOCAL uint64 CompressZIP_getInputLength(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return (uint64)compressInfo->zlib.stream.total_in;
}

LOCAL uint64 CompressZIP_getOutputLength(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return (uint64)compressInfo->zlib.stream.total_out;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
