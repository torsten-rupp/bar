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
#include <lzma.h>
#include <assert.h>

#include "global.h"
#include "ringbuffers.h"
#include "lists.h"
#include "files.h"

#include "errors.h"
#include "entrylists.h"
#include "patternlists.h"
#include "sources.h"
#include "crypt.h"
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
* Name   : lzmaErrorText
* Purpose: get error text for LZMA error code
* Input  : lzmaResult - LZMA result
* Output : -
* Return : error text or NULL
* Notes  : -
\***********************************************************************/

LOCAL const char *lzmaErrorText(lzma_ret lzmaResult)
{
  const char *errorText;

  switch (lzmaResult)
  {
      case LZMA_OK:                errorText = NULL; break;
      case LZMA_STREAM_END:        errorText = "end of stream"; break;
      case LZMA_NO_CHECK:          errorText = "no integrity check"; break;
      case LZMA_UNSUPPORTED_CHECK: errorText = "unsupported integrity check"; break;
      case LZMA_GET_CHECK:         errorText = "integrity check id found"; break;
      case LZMA_MEM_ERROR:         errorText = "insufficient memory"; break;
      case LZMA_MEMLIMIT_ERROR:    errorText = "memory limit reached"; break;
      case LZMA_FORMAT_ERROR:      errorText = "data format not recognized"; break;
      case LZMA_OPTIONS_ERROR:     errorText = "invalid or unsupported option"; break;
      case LZMA_DATA_ERROR:        errorText = "corrupt data"; break;
      case LZMA_BUF_ERROR:         errorText = "no input/output data"; break;
      case LZMA_PROG_ERROR:        errorText = "programming error: invalid arguments or corrupt decoder"; break;
      default:                     errorText = "unknown"; break;
  }

  return errorText;
}

/***********************************************************************\
* Name   : CompressLZMA_compressData
* Purpose: compress data with LZMA
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors CompressLZMA_compressData(CompressInfo *compressInfo)
{
  ulong maxCompressBytes,maxDataBytes;
  lzma_ret lzmaResult;

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
        compressInfo->lzmalib.stream.next_in   = (uint8_t*)RingBuffer_cArrayOut(&compressInfo->dataRingBuffer);
        compressInfo->lzmalib.stream.avail_in  = maxDataBytes;
        compressInfo->lzmalib.stream.next_out  = (uint8_t*)RingBuffer_cArrayIn(&compressInfo->compressRingBuffer);
        compressInfo->lzmalib.stream.avail_out = maxCompressBytes;
        lzmaResult = lzma_code(&compressInfo->lzmalib.stream,LZMA_RUN);
        if (lzmaResult != LZMA_OK)
        {
          return ERRORX_(DEFLATE_FAIL,lzmaResult,lzmaErrorText(lzmaResult));
        }
        RingBuffer_decrement(&compressInfo->dataRingBuffer,
                             maxDataBytes-compressInfo->lzmalib.stream.avail_in
                            );
        RingBuffer_increment(&compressInfo->compressRingBuffer,
                             maxCompressBytes-compressInfo->lzmalib.stream.avail_out
                            );

        // update compress state, compress length
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
        compressInfo->lzmalib.stream.next_in   = NULL;
        compressInfo->lzmalib.stream.avail_in  = 0;
        compressInfo->lzmalib.stream.next_out  = (uint8_t*)RingBuffer_cArrayIn(&compressInfo->compressRingBuffer);
        compressInfo->lzmalib.stream.avail_out = maxCompressBytes;
        lzmaResult = lzma_code(&compressInfo->lzmalib.stream,LZMA_FINISH);
        if      (lzmaResult == LZMA_STREAM_END)
        {
          compressInfo->endOfDataFlag = TRUE;
        }
        else if (lzmaResult != LZMA_OK)
        {
          return ERRORX_(DEFLATE_FAIL,lzmaResult,lzmaErrorText(lzmaResult));
        }
        RingBuffer_increment(&compressInfo->compressRingBuffer,
                             maxCompressBytes-compressInfo->lzmalib.stream.avail_out
                            );
      }
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : CompressLZMA_decompressData
* Purpose: decompress data with LZMA
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors CompressLZMA_decompressData(CompressInfo *compressInfo)
{
  lzma_ret lzmaResult;

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
        compressInfo->lzmalib.stream.next_in   = (uint8_t*)RingBuffer_cArrayOut(&compressInfo->compressRingBuffer);
        compressInfo->lzmalib.stream.avail_in  = maxCompressBytes;
        compressInfo->lzmalib.stream.next_out  = (uint8_t*)RingBuffer_cArrayIn(&compressInfo->dataRingBuffer);
        compressInfo->lzmalib.stream.avail_out = maxDataBytes;
        lzmaResult = lzma_code(&compressInfo->lzmalib.stream,LZMA_RUN);
        if      (lzmaResult == LZMA_STREAM_END)
        {
          compressInfo->endOfDataFlag = TRUE;
        }
        else if (lzmaResult != LZMA_OK)
        {
          return ERRORX_(INFLATE_FAIL,lzmaResult,lzmaErrorText(lzmaResult));
        }
        RingBuffer_decrement(&compressInfo->compressRingBuffer,
                             maxCompressBytes-compressInfo->lzmalib.stream.avail_in
                            );
        RingBuffer_increment(&compressInfo->dataRingBuffer,
                             maxDataBytes-compressInfo->lzmalib.stream.avail_out
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
        compressInfo->lzmalib.stream.next_in   = NULL;
        compressInfo->lzmalib.stream.avail_in  = 0;
        compressInfo->lzmalib.stream.next_out  = (uint8_t*)RingBuffer_cArrayIn(&compressInfo->dataRingBuffer);
        compressInfo->lzmalib.stream.avail_out = maxDataBytes;
        lzmaResult = lzma_code(&compressInfo->lzmalib.stream,LZMA_FINISH);
        if      (lzmaResult == LZMA_STREAM_END)
        {
          compressInfo->endOfDataFlag = TRUE;
        }
        else if (lzmaResult != LZMA_OK)
        {
          return ERRORX_(INFLATE_FAIL,lzmaResult,lzmaErrorText(lzmaResult));
        }
        RingBuffer_increment(&compressInfo->dataRingBuffer,
                             maxDataBytes-compressInfo->lzmalib.stream.avail_out
                            );
      }
    }
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

LOCAL Errors CompressLZMA_init(CompressInfo       *compressInfo,
                               CompressModes      compressMode,
                               CompressAlgorithms compressAlgorithm
                              )
{
  lzma_stream streamInit = LZMA_STREAM_INIT;
  lzma_ret    lzmaResult;

  assert(compressInfo != NULL);

  compressInfo->lzmalib.compressionLevel = 0;
  switch (compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_LZMA_1: compressInfo->lzmalib.compressionLevel = 1; break;
    case COMPRESS_ALGORITHM_LZMA_2: compressInfo->lzmalib.compressionLevel = 2; break;
    case COMPRESS_ALGORITHM_LZMA_3: compressInfo->lzmalib.compressionLevel = 3; break;
    case COMPRESS_ALGORITHM_LZMA_4: compressInfo->lzmalib.compressionLevel = 4; break;
    case COMPRESS_ALGORITHM_LZMA_5: compressInfo->lzmalib.compressionLevel = 5; break;
    case COMPRESS_ALGORITHM_LZMA_6: compressInfo->lzmalib.compressionLevel = 6; break;
    case COMPRESS_ALGORITHM_LZMA_7: compressInfo->lzmalib.compressionLevel = 7; break;
    case COMPRESS_ALGORITHM_LZMA_8: compressInfo->lzmalib.compressionLevel = 8; break;
    case COMPRESS_ALGORITHM_LZMA_9: compressInfo->lzmalib.compressionLevel = 9; break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  compressInfo->lzmalib.stream = streamInit;
  compressInfo->lzmalib.stream.allocator = NULL;
  switch (compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      lzmaResult = lzma_easy_encoder(&compressInfo->lzmalib.stream,compressInfo->lzmalib.compressionLevel,LZMA_CHECK_NONE);
      if (lzmaResult != LZMA_OK)
      {
        RingBuffer_done(&compressInfo->compressRingBuffer,NULL,NULL);
        RingBuffer_done(&compressInfo->dataRingBuffer,NULL,NULL);
        return ERRORX_(INIT_COMPRESS,lzmaResult,lzmaErrorText(lzmaResult));
      }
      break;
    case COMPRESS_MODE_INFLATE:
      lzmaResult = lzma_auto_decoder(&compressInfo->lzmalib.stream,0xFFFffffFFFFffffLL,0);
      if (lzmaResult != LZMA_OK)
      {
        RingBuffer_done(&compressInfo->compressRingBuffer,NULL,NULL);
        RingBuffer_done(&compressInfo->dataRingBuffer,NULL,NULL);
        return ERRORX_(INIT_DECOMPRESS,lzmaResult,lzmaErrorText(lzmaResult));
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

LOCAL void CompressLZMA_done(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  lzma_end(&compressInfo->lzmalib.stream);
}

LOCAL Errors CompressLZMA_reset(CompressInfo *compressInfo)
{
  lzma_stream streamInit = LZMA_STREAM_INIT;
  int         lzmalibResult;

  assert(compressInfo != NULL);

  lzmalibResult = LZMA_PROG_ERROR;
  switch (compressInfo->compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      lzma_end(&compressInfo->lzmalib.stream);
      compressInfo->lzmalib.stream = streamInit;
      lzmalibResult = lzma_easy_encoder(&compressInfo->lzmalib.stream,compressInfo->lzmalib.compressionLevel,LZMA_CHECK_NONE);
      if (lzmalibResult != LZMA_OK)
      {
        return ERROR_(DEFLATE_FAIL,lzmalibResult);;
      }
      break;
    case COMPRESS_MODE_INFLATE:
      lzma_end(&compressInfo->lzmalib.stream);
      compressInfo->lzmalib.stream = streamInit;
      lzmalibResult = lzma_auto_decoder(&compressInfo->lzmalib.stream,0xFFFffffFFFFffffLL,0);
      if (lzmalibResult != LZMA_OK)
      {
        return ERROR_(INFLATE_FAIL,lzmalibResult);
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

LOCAL uint64 CompressLZMA_getInputLength(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return (uint64)compressInfo->lzmalib.stream.total_in;
}

LOCAL uint64 CompressLZMA_getOutputLength(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return (uint64)compressInfo->lzmalib.stream.total_out;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
