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
#include <lzo/lzo1x.h>
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
#include "bar.h"

#include "compress.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define LZO_BLOCK_SIZE (64*1024)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : CompressLZO_compressData
* Purpose: compress data with LZO
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors CompressLZO_compressData(CompressInfo *compressInfo)
{
  ulong    maxCompressBytes,maxDataBytes;
  ulong    n;
  lzo_uint compressLength;
  int      lzoError;

  assert(compressInfo != NULL);
  assert(compressInfo->lzo.compressFunction != NULL);
  assert(compressInfo->lzo.outputBuffer != NULL);

  if (!compressInfo->endOfDataFlag)                                           // not end-of-data
  {
    if (!RingBuffer_isFull(&compressInfo->compressRingBuffer))                // space in compress buffer
    {
      // get max. number of compressed bytes
      maxCompressBytes = RingBuffer_getFree(&compressInfo->compressRingBuffer);

      // move LZO output buffer -> compress buffer
      n = MIN(compressInfo->lzo.outputBufferLength-compressInfo->lzo.outputBufferIndex,maxCompressBytes);
      if (n > 0)
      {
        RingBuffer_put(&compressInfo->compressRingBuffer,
                       &compressInfo->lzo.outputBuffer[compressInfo->lzo.outputBufferIndex],
                       n
                      );
        compressInfo->lzo.outputBufferIndex += (uint)n;
        compressInfo->lzo.totalOutputLength += n;
      }
    }

    if (!RingBuffer_isFull(&compressInfo->compressRingBuffer))                // space in compress buffer
    {
      // compress available data
      if (!RingBuffer_isEmpty(&compressInfo->dataRingBuffer))                 // unprocessed data available
      {
        // get max. number of data and max. number of compressed bytes
        maxDataBytes     = RingBuffer_getAvailable(&compressInfo->dataRingBuffer);
        maxCompressBytes = RingBuffer_getFree(&compressInfo->compressRingBuffer);

        // move data buffer -> LZO input buffer
        n = MIN(LZO_BLOCK_SIZE-compressInfo->lzo.inputBufferLength,maxDataBytes);
        if (n > 0)
        {
          RingBuffer_get(&compressInfo->dataRingBuffer,
                         &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferLength],
                         n
                        );
          compressInfo->lzo.inputBufferLength += (uint)n;
          compressInfo->lzo.totalInputLength  += n;
        }

        if (compressInfo->lzo.inputBufferLength >= LZO_BLOCK_SIZE)
        {
          // compress: LZO input buffer -> LZO output buffer (spare 4 bytes for length in output buffer)
          assert(compressInfo->lzo.outputBufferIndex >= compressInfo->lzo.outputBufferLength);
          lzoError = compressInfo->lzo.compressFunction((lzo_bytep)compressInfo->lzo.inputBuffer,
                                                        compressInfo->lzo.inputBufferLength,
                                                        &compressInfo->lzo.outputBuffer[4],
                                                        &compressLength,
                                                        compressInfo->lzo.workingMemory
                                                       );
          if (lzoError != LZO_E_OK)
          {
            return ERROR_(DEFLATE_FAIL,lzoError);
          }
          compressInfo->lzo.inputBufferLength = 0;
//fprintf(stderr,"%s, %d: %ld\n",__FILE__,__LINE__,compressLength);

          // compress: put length of compress data into output buffer before compressed data
          compressInfo->lzo.outputBuffer[0] = (compressLength & 0xFF000000) >> 24;
          compressInfo->lzo.outputBuffer[1] = (compressLength & 0x00FF0000) >> 16;
          compressInfo->lzo.outputBuffer[2] = (compressLength & 0x0000FF00) >>  8;
          compressInfo->lzo.outputBuffer[3] = (compressLength & 0x000000FF) >>  0;

          // init LZO output buffer
          compressInfo->lzo.outputBufferIndex  = 0;
          compressInfo->lzo.outputBufferLength = 4+(uint)compressLength;

          // move LZO output buffer -> compress buffer
          n = MIN(compressInfo->lzo.outputBufferLength-compressInfo->lzo.outputBufferIndex,maxCompressBytes);
          if (n > 0)
          {
            RingBuffer_put(&compressInfo->compressRingBuffer,
                           &compressInfo->lzo.outputBuffer[compressInfo->lzo.outputBufferIndex],
                           n
                          );
            compressInfo->lzo.outputBufferIndex += (uint)n;
            compressInfo->lzo.totalOutputLength += n;
          }
        }

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
        if (compressInfo->lzo.inputBufferLength > 0)
        {
          // get max. number of compressed bytes
          maxCompressBytes = RingBuffer_getFree(&compressInfo->compressRingBuffer);

          // compress: LZO input buffer -> LZO output buffer (spare 4 bytes for length in output buffer)
          assert(compressInfo->lzo.outputBufferIndex >= compressInfo->lzo.outputBufferLength);
          lzoError = compressInfo->lzo.compressFunction((lzo_bytep)compressInfo->lzo.inputBuffer,
                                                        compressInfo->lzo.inputBufferLength,
                                                        &compressInfo->lzo.outputBuffer[4],
                                                        &compressLength,
                                                        compressInfo->lzo.workingMemory
                                                       );
          if (lzoError != LZO_E_OK)
          {
            return ERROR_(DEFLATE_FAIL,lzoError);
          }
          compressInfo->lzo.inputBufferLength = 0;
//fprintf(stderr,"%s, %d: %ld\n",__FILE__,__LINE__,compressLength);

          // compress: put length of compress data into output buffer before compressed data
          compressInfo->lzo.outputBuffer[0] = (compressLength & 0xFF000000) >> 24;
          compressInfo->lzo.outputBuffer[1] = (compressLength & 0x00FF0000) >> 16;
          compressInfo->lzo.outputBuffer[2] = (compressLength & 0x0000FF00) >>  8;
          compressInfo->lzo.outputBuffer[3] = (compressLength & 0x000000FF) >>  0;

          // init LZO output buffer
          compressInfo->lzo.outputBufferIndex  = 0;
          compressInfo->lzo.outputBufferLength = 4+(uint)compressLength;

          // move LZO output buffer -> compress buffer
          n = MIN(compressInfo->lzo.outputBufferLength-compressInfo->lzo.outputBufferIndex,maxCompressBytes);
          if (n > 0)
          {
            RingBuffer_put(&compressInfo->compressRingBuffer,
                           &compressInfo->lzo.outputBuffer[compressInfo->lzo.outputBufferIndex],
                           n
                          );
            compressInfo->lzo.outputBufferIndex += (uint)n;
            compressInfo->lzo.totalOutputLength += n;
          }
        }
      }
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : CompressLZO_decompressData
* Purpose: decompress data with LZO
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors CompressLZO_decompressData(CompressInfo *compressInfo)
{
  ulong    maxCompressBytes,maxDataBytes;
  ulong    n;
  lzo_uint compressLength,dataLength;
  int      lzoError;

  assert(compressInfo != NULL);
  assert(compressInfo->lzo.compressFunction != NULL);
  assert(compressInfo->lzo.inputBuffer != NULL);
  assert(compressInfo->lzo.outputBuffer != NULL);

  if (!compressInfo->endOfDataFlag)                                           // not end-of-data
  {
    if (!RingBuffer_isFull(&compressInfo->dataRingBuffer))                // space in compress buffer
    {
      // get max. number of data bytes
      maxDataBytes = RingBuffer_getFree(&compressInfo->dataRingBuffer);

      // move LZO output buffer -> data buffer
      n = MIN(compressInfo->lzo.outputBufferLength-compressInfo->lzo.outputBufferIndex,maxCompressBytes);
      if (n > 0)
      {
        RingBuffer_put(&compressInfo->compressRingBuffer,
                       &compressInfo->lzo.outputBuffer[compressInfo->lzo.outputBufferIndex],
                       n
                      );
        compressInfo->lzo.outputBufferIndex += (uint)n;
        compressInfo->lzo.totalOutputLength += n;
fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,n);
      }
    }

    if (!RingBuffer_isFull(&compressInfo->dataRingBuffer))                    // space in data buffer
    {
      // decompress available data
      if (!RingBuffer_isEmpty(&compressInfo->compressRingBuffer))             // unprocessed compressed data available
      {
        // get max. number of compressed and max. number of data bytes
        maxCompressBytes = RingBuffer_getAvailable(&compressInfo->compressRingBuffer);
        maxDataBytes     = RingBuffer_getFree(&compressInfo->dataRingBuffer);

        // move compress buffer -> LZO input buffer
        n = MIN(compressInfo->lzo.bufferSize-compressInfo->lzo.inputBufferLength,maxCompressBytes);
        if (n > 0)
        {
          RingBuffer_get(&compressInfo->compressRingBuffer,
                         &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferLength],
                         n
                        );
          compressInfo->lzo.inputBufferLength += (uint)n;
          compressInfo->lzo.totalInputLength  += n;
        }

        if (compressInfo->lzo.inputBufferLength >= 4)
        {
          // decompress: get length of compressed data from LZO input buffer
          compressLength =   ((uint32)compressInfo->lzo.inputBuffer[0] << 24)
                           | ((uint32)compressInfo->lzo.inputBuffer[1] << 16)
                           | ((uint32)compressInfo->lzo.inputBuffer[2] <<  8)
                           | ((uint32)compressInfo->lzo.inputBuffer[3] <<  0);
          if (compressLength >= compressInfo->lzo.bufferSize)
          {
//fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,compressLength);
            return ERRORX_(INFLATE_FAIL,0,"Invalid data size");
          }
          if (compressInfo->lzo.inputBufferLength >= (4+compressLength))
          {
            // decompress: transfer compress buffer -> data buffer (byte 0..3 are length)
            assert(compressInfo->lzo.outputBufferIndex >= compressInfo->lzo.outputBufferLength);
            lzoError = compressInfo->lzo.decompressFunction((lzo_bytep)&compressInfo->lzo.inputBuffer[4],
                                                            compressLength,
                                                            compressInfo->lzo.outputBuffer,
                                                            &dataLength,
                                                            compressInfo->lzo.workingMemory
                                                           );
            if (lzoError != LZO_E_OK)
            {
              return ERROR_(DEFLATE_FAIL,lzoError);
            }
            memmove(compressInfo->lzo.inputBuffer,
                    &compressInfo->lzo.inputBuffer[4+compressLength],
                    compressInfo->lzo.inputBufferLength-(4+compressLength)
                   );
            compressInfo->lzo.inputBufferIndex  =  0;
            compressInfo->lzo.inputBufferLength -= 4+compressLength;
//fprintf(stderr,"%s, %d: compressLength=%d\n",__FILE__,__LINE__,compressLength);
//fprintf(stderr,"%s, %d: dataLength=%d\n",__FILE__,__LINE__,dataLength);

            // init LZO output buffer
            compressInfo->lzo.outputBufferIndex  = 0;
            compressInfo->lzo.outputBufferLength = (uint)dataLength;

            // move LZO output buffer -> data buffer
            n = MIN(compressInfo->lzo.outputBufferLength-compressInfo->lzo.outputBufferIndex,maxDataBytes);
            if (n > 0)
            {
              RingBuffer_put(&compressInfo->dataRingBuffer,
                             &compressInfo->lzo.outputBuffer[compressInfo->lzo.outputBufferIndex],
                             n
                            );
              compressInfo->lzo.outputBufferIndex += (uint)n;
              compressInfo->lzo.totalOutputLength += n;
            }

            // update compress state
            compressInfo->compressState = COMPRESS_STATE_RUNNING;
          }
        }
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

        // move compress buffer -> LZO input buffer
        n = MIN(compressInfo->lzo.bufferSize-compressInfo->lzo.inputBufferLength,maxCompressBytes);
        if (n > 0)
        {
          RingBuffer_get(&compressInfo->compressRingBuffer,
                         &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferLength],
                         n
                        );
          compressInfo->lzo.inputBufferLength += (uint)n;
          compressInfo->lzo.totalInputLength  += n;
        }

        if (compressInfo->lzo.inputBufferLength >= 4)
        {
          // decompress: get length of compressed data from LZO input buffer
          compressLength =   ((uint32)compressInfo->lzo.inputBuffer[0] << 24)
                           | ((uint32)compressInfo->lzo.inputBuffer[1] << 16)
                           | ((uint32)compressInfo->lzo.inputBuffer[2] <<  8)
                           | ((uint32)compressInfo->lzo.inputBuffer[3] <<  0);
          if (compressLength >= compressInfo->lzo.bufferSize)
          {
            return ERRORX_(DEFLATE_FAIL,0,"Invalid data size");
          }
          if (compressInfo->lzo.inputBufferLength >= (4+compressLength))
          {
            // decompress: transfer compress buffer -> data buffer (byte 0..3 are length)
            assert(compressInfo->lzo.outputBufferIndex >= compressInfo->lzo.outputBufferLength);
            lzoError = compressInfo->lzo.decompressFunction((lzo_bytep)&compressInfo->lzo.inputBuffer[4],
                                                            compressLength,
                                                            compressInfo->lzo.outputBuffer,
                                                            &dataLength,
                                                            compressInfo->lzo.workingMemory
                                                           );
            if (lzoError != LZO_E_OK)
            {
              return ERROR_(DEFLATE_FAIL,lzoError);
            }
            memmove(compressInfo->lzo.inputBuffer,
                    &compressInfo->lzo.inputBuffer[4+compressLength],
                    compressInfo->lzo.inputBufferLength-(4+compressLength)
                   );
            compressInfo->lzo.inputBufferIndex  =  0;
            compressInfo->lzo.inputBufferLength -= 4+compressLength;
//fprintf(stderr,"%s, %d: compressLength=%d\n",__FILE__,__LINE__,compressLength);
//fprintf(stderr,"%s, %d: dataLength=%d\n",__FILE__,__LINE__,dataLength);

            // init LZO output buffer
            compressInfo->lzo.outputBufferIndex  = 0;
            compressInfo->lzo.outputBufferLength = (uint)dataLength;

            // move LZO output buffer -> data buffer
            n = MIN(compressInfo->lzo.outputBufferLength-compressInfo->lzo.outputBufferIndex,maxDataBytes);
            if (n > 0)
            {
              RingBuffer_put(&compressInfo->dataRingBuffer,
                             &compressInfo->lzo.outputBuffer[compressInfo->lzo.outputBufferIndex],
                             n
                            );
              compressInfo->lzo.outputBufferIndex += (uint)n;
              compressInfo->lzo.totalOutputLength += n;
            }

            // update compress state
            compressInfo->compressState = COMPRESS_STATE_RUNNING;
          }
        }
      }
    }
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

LOCAL Errors CompressLZO_init(CompressInfo       *compressInfo,
                              CompressModes      compressMode,
                              CompressAlgorithms compressAlgorithm
                             )
{
  ulong bufferSize;
  ulong workingMemorySize;

  assert(compressInfo != NULL);

  UNUSED_VARIABLE(compressMode);

  compressInfo->lzo.inputBufferIndex   = 0;
  compressInfo->lzo.inputBufferLength  = 0;
  compressInfo->lzo.outputBufferIndex  = 0;
  compressInfo->lzo.outputBufferLength = 0;
  compressInfo->lzo.totalInputLength   = 0LL;
  compressInfo->lzo.totalOutputLength  = 0LL;

  switch (compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_LZO_1:
      compressInfo->lzo.compressFunction   = lzo1x_1_11_compress;
      compressInfo->lzo.decompressFunction = lzo1x_decompress;
      compressInfo->lzo.bufferSize         = 4+LZO_BLOCK_SIZE+(LZO_BLOCK_SIZE/16)+64+3;
      workingMemorySize                    = LZO1X_1_11_MEM_COMPRESS;
      break;
    case COMPRESS_ALGORITHM_LZO_2:
      compressInfo->lzo.compressFunction   = lzo1x_1_12_compress;
      compressInfo->lzo.decompressFunction = lzo1x_decompress;
      compressInfo->lzo.bufferSize         = 4+LZO_BLOCK_SIZE+(LZO_BLOCK_SIZE/16)+64+3;
      workingMemorySize                    = LZO1X_1_12_MEM_COMPRESS;
      break;
    case COMPRESS_ALGORITHM_LZO_3:
      compressInfo->lzo.compressFunction   = lzo1x_1_15_compress;
      compressInfo->lzo.decompressFunction = lzo1x_decompress;
      compressInfo->lzo.bufferSize         = 4+LZO_BLOCK_SIZE+(LZO_BLOCK_SIZE/16)+64+3;
      workingMemorySize                    = LZO1X_1_15_MEM_COMPRESS;
      break;
    case COMPRESS_ALGORITHM_LZO_4:
      compressInfo->lzo.compressFunction   = lzo1x_1_compress;
      compressInfo->lzo.decompressFunction = lzo1x_decompress;
      compressInfo->lzo.bufferSize         = 4+LZO_BLOCK_SIZE+(LZO_BLOCK_SIZE/16)+64+3;
      workingMemorySize                    = LZO1X_1_MEM_COMPRESS;
      break;
    case COMPRESS_ALGORITHM_LZO_5:
      compressInfo->lzo.compressFunction   = lzo1x_999_compress;
      compressInfo->lzo.decompressFunction = lzo1x_decompress;
      compressInfo->lzo.bufferSize         = 4+LZO_BLOCK_SIZE+(LZO_BLOCK_SIZE/16)+64+3;
      workingMemorySize                    = LZO1X_999_MEM_COMPRESS;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  compressInfo->lzo.inputBuffer = (byte*)malloc(compressInfo->lzo.bufferSize);
  if (compressInfo->lzo.inputBuffer == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }
  compressInfo->lzo.outputBuffer = (byte*)malloc(compressInfo->lzo.bufferSize);
  if (compressInfo->lzo.outputBuffer == NULL)
  {
    free(compressInfo->lzo.inputBuffer);
    return ERROR_INSUFFICIENT_MEMORY;
  }
  compressInfo->lzo.workingMemory = (lzo_voidp)malloc(workingMemorySize);
  if (compressInfo->lzo.workingMemory == NULL)
  {
    free(compressInfo->lzo.outputBuffer);
    free(compressInfo->lzo.inputBuffer);
    return ERROR_INSUFFICIENT_MEMORY;
  }

  return ERROR_NONE;
}

LOCAL void CompressLZO_done(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  free(compressInfo->lzo.workingMemory);
  free(compressInfo->lzo.outputBuffer);
  free(compressInfo->lzo.inputBuffer);
}

LOCAL Errors CompressLZO_reset(CompressInfo *compressInfo)
{

  assert(compressInfo != NULL);

  UNUSED_VARIABLE(compressInfo);

  return ERROR_NONE;
}

LOCAL uint64 CompressLZO_getInputLength(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return compressInfo->lzo.totalInputLength;
}

LOCAL uint64 CompressLZO_getOutputLength(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return compressInfo->lzo.totalOutputLength;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
