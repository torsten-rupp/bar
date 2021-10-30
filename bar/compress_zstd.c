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
#include <zstd.h>
#include <zstd_errors.h>
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
* Name   : CompressZStd_compressData
* Purpose: compress data with zstd
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors CompressZStd_compressData(CompressInfo *compressInfo)
{
  ulong  maxCompressBytes,maxDataBytes;
  size_t zstdResult;

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
        compressInfo->zstd.inBuffer.src   = RingBuffer_cArrayOut(&compressInfo->dataRingBuffer);
        compressInfo->zstd.inBuffer.size  = maxDataBytes;
        compressInfo->zstd.inBuffer.pos   = 0;
        compressInfo->zstd.outBuffer.dst  = RingBuffer_cArrayIn(&compressInfo->compressRingBuffer);
        compressInfo->zstd.outBuffer.size = maxCompressBytes;
        compressInfo->zstd.outBuffer.pos  = 0;
        zstdResult = ZSTD_compressStream(compressInfo->zstd.cStream,&compressInfo->zstd.outBuffer,&compressInfo->zstd.inBuffer);
//fprintf(stderr,"%s, %d: zstdResult=%lu input=%lu,%lu output=%lu,%lu\n",__FILE__,__LINE__,zstdResult,compressInfo->zstd.inBuffer.pos,compressInfo->zstd.inBuffer.size,compressInfo->zstd.outBuffer.pos,compressInfo->zstd.outBuffer.size);
        if (ZSTD_isError(zstdResult))
        {
          return ERRORX_(DEFLATE_FAIL,ZSTD_getErrorCode(zstdResult),ZSTD_getErrorName(zstdResult));
        }
        RingBuffer_decrement(&compressInfo->dataRingBuffer,
                             compressInfo->zstd.inBuffer.pos
                            );
        RingBuffer_increment(&compressInfo->compressRingBuffer,
                             compressInfo->zstd.outBuffer.pos
                            );
        compressInfo->zstd.totalIn  += (uint64)compressInfo->zstd.inBuffer.pos;
        compressInfo->zstd.totalOut += (uint64)compressInfo->zstd.outBuffer.pos;
//fprintf(stderr,"%s, %d: %ld -> %ld\n",__FILE__,__LINE__,compressInfo->zstd.inBuffer.pos,compressInfo->zstd.outBuffer.pos);

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
        compressInfo->zstd.outBuffer.dst  = RingBuffer_cArrayIn(&compressInfo->compressRingBuffer);
        compressInfo->zstd.outBuffer.size = maxCompressBytes;
        compressInfo->zstd.outBuffer.pos  = 0;
        zstdResult = ZSTD_endStream(compressInfo->zstd.cStream,&compressInfo->zstd.outBuffer);
//fprintf(stderr,"%s, %d: zstdResult=%lu output=%lu,%lu\n",__FILE__,__LINE__,zstdResult,compressInfo->zstd.outBuffer.pos,compressInfo->zstd.outBuffer.size);
        if      (zstdResult == 0)
        {
          compressInfo->endOfDataFlag = TRUE;
        }
        else if (ZSTD_isError(zstdResult))
        {
          return ERRORX_(DEFLATE_FAIL,ZSTD_getErrorCode(zstdResult),ZSTD_getErrorName(zstdResult));
        }
        RingBuffer_increment(&compressInfo->compressRingBuffer,
                             compressInfo->zstd.outBuffer.pos
                            );
        compressInfo->zstd.totalOut += (uint64)compressInfo->zstd.outBuffer.pos;
//fprintf(stderr,"%s, %d: %ld -> %ld\n",__FILE__,__LINE__,compressInfo->zstd.inBuffer.pos,compressInfo->zstd.outBuffer.pos);
      }
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : CompressZStd_decompressData
* Purpose: decompress data with zstd
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors CompressZStd_decompressData(CompressInfo *compressInfo)
{
  size_t zstdResult;

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
        compressInfo->zstd.inBuffer.src   = RingBuffer_cArrayOut(&compressInfo->compressRingBuffer);
        compressInfo->zstd.inBuffer.size  = maxCompressBytes;
        compressInfo->zstd.inBuffer.pos   = 0;
        compressInfo->zstd.outBuffer.dst  = RingBuffer_cArrayIn(&compressInfo->dataRingBuffer);
        compressInfo->zstd.outBuffer.size = maxDataBytes;
        compressInfo->zstd.outBuffer.pos  = 0;
        zstdResult = ZSTD_decompressStream(compressInfo->zstd.dStream,&compressInfo->zstd.outBuffer,&compressInfo->zstd.inBuffer);
//fprintf(stderr,"%s, %d: zstdResult=%lu input=%lu,%lu output=%lu,%lu\n",__FILE__,__LINE__,zstdResult,compressInfo->zstd.inBuffer.pos,compressInfo->zstd.inBuffer.size,compressInfo->zstd.outBuffer.pos,compressInfo->zstd.outBuffer.size);
        if      (   (zstdResult == 0)
                 && ((compressInfo->zstd.totalOut+(uint64)compressInfo->zstd.outBuffer.pos) >= compressInfo->length)
                )
        {
          compressInfo->endOfDataFlag = TRUE;
        }
        else if (ZSTD_isError(zstdResult))
        {
          return ERRORX_(INFLATE_FAIL,ZSTD_getErrorCode(zstdResult),ZSTD_getErrorName(zstdResult));
        }
        RingBuffer_decrement(&compressInfo->compressRingBuffer,
                             compressInfo->zstd.inBuffer.pos
                            );
        RingBuffer_increment(&compressInfo->dataRingBuffer,
                             compressInfo->zstd.outBuffer.pos
                            );
        compressInfo->zstd.totalIn  += (uint64)compressInfo->zstd.inBuffer.pos;
        compressInfo->zstd.totalOut += (uint64)compressInfo->zstd.outBuffer.pos;

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
        compressInfo->zstd.inBuffer.src   = NULL;
        compressInfo->zstd.inBuffer.size  = 0;
        compressInfo->zstd.inBuffer.pos   = 0;
        compressInfo->zstd.outBuffer.dst  = (Bytef*)RingBuffer_cArrayIn(&compressInfo->dataRingBuffer);
        compressInfo->zstd.outBuffer.size = maxDataBytes;
        compressInfo->zstd.outBuffer.pos  = 0;
        zstdResult = ZSTD_decompressStream(compressInfo->zstd.dStream,&compressInfo->zstd.outBuffer,&compressInfo->zstd.inBuffer);
//fprintf(stderr,"%s, %d: zstdResult=%lu input=%lu,%lu output=%lu,%lu\n",__FILE__,__LINE__,zstdResult,compressInfo->zstd.inBuffer.pos,compressInfo->zstd.inBuffer.size,compressInfo->zstd.outBuffer.pos,compressInfo->zstd.outBuffer.size);
        if      (compressInfo->zstd.outBuffer.pos < compressInfo->zstd.outBuffer.size)
        {
          compressInfo->endOfDataFlag = TRUE;
        }
        else if (ZSTD_getErrorCode(zstdResult) != ZSTD_error_no_error)
        {
          return ERRORX_(INFLATE_FAIL,ZSTD_getErrorCode(zstdResult),ZSTD_getErrorName(zstdResult));
        }
        RingBuffer_increment(&compressInfo->dataRingBuffer,
                             compressInfo->zstd.outBuffer.pos
                            );
        compressInfo->zstd.totalOut += (uint64)compressInfo->zstd.outBuffer.pos;
      }
    }
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

LOCAL Errors CompressZStd_init(CompressInfo       *compressInfo,
                               CompressModes      compressMode,
                               CompressAlgorithms compressAlgorithm
                              )
{
  size_t zstdResult;

  assert(compressInfo != NULL);

  compressInfo->zstd.compressionLevel = 0;
  compressInfo->zstd.totalIn          = 0;
  compressInfo->zstd.totalOut         = 0;
  switch (compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_ZSTD_0:  compressInfo->zstd.compressionLevel =  0; break;
    case COMPRESS_ALGORITHM_ZSTD_1:  compressInfo->zstd.compressionLevel =  1; break;
    case COMPRESS_ALGORITHM_ZSTD_2:  compressInfo->zstd.compressionLevel =  2; break;
    case COMPRESS_ALGORITHM_ZSTD_3:  compressInfo->zstd.compressionLevel =  3; break;
    case COMPRESS_ALGORITHM_ZSTD_4:  compressInfo->zstd.compressionLevel =  4; break;
    case COMPRESS_ALGORITHM_ZSTD_5:  compressInfo->zstd.compressionLevel =  5; break;
    case COMPRESS_ALGORITHM_ZSTD_6:  compressInfo->zstd.compressionLevel =  6; break;
    case COMPRESS_ALGORITHM_ZSTD_7:  compressInfo->zstd.compressionLevel =  7; break;
    case COMPRESS_ALGORITHM_ZSTD_8:  compressInfo->zstd.compressionLevel =  8; break;
    case COMPRESS_ALGORITHM_ZSTD_9:  compressInfo->zstd.compressionLevel =  9; break;
    case COMPRESS_ALGORITHM_ZSTD_10: compressInfo->zstd.compressionLevel = 10; break;
    case COMPRESS_ALGORITHM_ZSTD_11: compressInfo->zstd.compressionLevel = 11; break;
    case COMPRESS_ALGORITHM_ZSTD_12: compressInfo->zstd.compressionLevel = 12; break;
    case COMPRESS_ALGORITHM_ZSTD_13: compressInfo->zstd.compressionLevel = 13; break;
    case COMPRESS_ALGORITHM_ZSTD_14: compressInfo->zstd.compressionLevel = 14; break;
    case COMPRESS_ALGORITHM_ZSTD_15: compressInfo->zstd.compressionLevel = 15; break;
    case COMPRESS_ALGORITHM_ZSTD_16: compressInfo->zstd.compressionLevel = 16; break;
    case COMPRESS_ALGORITHM_ZSTD_17: compressInfo->zstd.compressionLevel = 17; break;
    case COMPRESS_ALGORITHM_ZSTD_18: compressInfo->zstd.compressionLevel = 18; break;
    case COMPRESS_ALGORITHM_ZSTD_19: compressInfo->zstd.compressionLevel = 19; break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  switch (compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
//fprintf(stderr,"%s, %d: %ld %ld\n",__FILE__,__LINE__,ZSTD_CStreamInSize(),ZSTD_CStreamOutSize());
      compressInfo->zstd.cStream = ZSTD_createCStream();
      if (compressInfo->zstd.cStream == NULL)
      {
        return ERROR_INIT_COMPRESS;
      }
      zstdResult = ZSTD_initCStream(compressInfo->zstd.cStream,compressInfo->zstd.compressionLevel);
      if (ZSTD_isError(zstdResult))
      {
        ZSTD_freeCStream(compressInfo->zstd.cStream);
        return ERRORX_(INIT_COMPRESS,zstdResult,"%s",ZSTD_getErrorName(zstdResult));
      }
      break;
    case COMPRESS_MODE_INFLATE:
//fprintf(stderr,"%s, %d: %ld %ld\n",__FILE__,__LINE__,ZSTD_DStreamInSize(),ZSTD_DStreamOutSize());
      compressInfo->zstd.dStream = ZSTD_createDStream();
      if (compressInfo->zstd.dStream == NULL)
      {
        return ERROR_INIT_DECOMPRESS;
      }
      zstdResult = ZSTD_initDStream(compressInfo->zstd.dStream);
      if (ZSTD_isError(zstdResult))
      {
        ZSTD_freeDStream(compressInfo->zstd.dStream);
        return ERRORX_(INIT_DECOMPRESS,zstdResult,"%s",ZSTD_getErrorName(zstdResult));
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

LOCAL void CompressZStd_done(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  switch (compressInfo->compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      ZSTD_freeCStream(compressInfo->zstd.cStream);
      break;
    case COMPRESS_MODE_INFLATE:
      ZSTD_freeDStream(compressInfo->zstd.dStream);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
}

LOCAL Errors CompressZStd_reset(CompressInfo *compressInfo)
{
  int zstdResult;

  assert(compressInfo != NULL);

  zstdResult = Z_ERRNO;
  switch (compressInfo->compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      zstdResult = ZSTD_resetCStream(compressInfo->zstd.cStream,0);
      if (ZSTD_isError(zstdResult))
      {
        return ERROR_(DEFLATE_FAIL,ZSTD_getErrorCode(zstdResult));
      }
      break;
    case COMPRESS_MODE_INFLATE:
      zstdResult = ZSTD_resetDStream(compressInfo->zstd.dStream);
      if (ZSTD_isError(zstdResult))
      {
        return ERROR_(INFLATE_FAIL,ZSTD_getErrorCode(zstdResult));
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

LOCAL uint64 CompressZStd_getInputLength(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return compressInfo->zstd.totalIn;
}

LOCAL uint64 CompressZStd_getOutputLength(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return compressInfo->zstd.totalOut;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
