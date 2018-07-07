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

#define _LZO_DEBUG
#define _LZO_DEBUG_DISABLE_COMPRESS

/***************************** Constants *******************************/

#define LZO_BLOCK_SIZE (64*KB)

#define LZO_MAX_COMPRESS_LENGTH 0x00FFFFFF
#define LZO_LENGTH_MASK         0x00FFFFFF
#define LZO_END_OF_DATA_FLAG    0x80000000
#define LZO_COMPRESSED_FLAG     0x40000000

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

#ifdef LZO_DEBUG
LOCAL uint lzoblockCount = 0;
#endif /* LZO_DEBUG */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : getLZOErrorText
* Purpose: get error text for LZO error code
* Input  : errorCode - LZO error code
* Output : -
* Return : error text
* Notes  : -
\***********************************************************************/

LOCAL const char *getLZOErrorText(int errorCode)
{
  const char *errorText;

  switch (errorCode)
  {
    case LZO_E_OK                   : errorText = "none";                  break;
    case LZO_E_ERROR                : errorText = "error";                 break;
    case LZO_E_OUT_OF_MEMORY        : errorText = "out of memory";         break;
    case LZO_E_NOT_COMPRESSIBLE     : errorText = "not compressible";      break;
    case LZO_E_INPUT_OVERRUN        : errorText = "input overrun";         break;
    case LZO_E_OUTPUT_OVERRUN       : errorText = "output overrun";        break;
    case LZO_E_LOOKBEHIND_OVERRUN   : errorText = "look behind overrun";   break;
    case LZO_E_EOF_NOT_FOUND        : errorText = "end of data not found"; break;
    case LZO_E_INPUT_NOT_CONSUMED   : errorText = "input not consumed";    break;
    case LZO_E_NOT_YET_IMPLEMENTED  : errorText = "not yet implemented";   break;
    case LZO_E_INVALID_ARGUMENT     : errorText = "invalid argument";      break;
    #ifdef LZO_E_INVALID_ALIGNMENT
      case LZO_E_INVALID_ALIGNMENT  : errorText = "invalid alignment";     break;
    #endif
    #ifdef LZO_E_OUTPUT_NOT_CONSUMED
      case LZO_E_OUTPUT_NOT_CONSUMED: errorText = "output not consumed";   break;
    #endif
    #ifdef LZO_E_INTERNAL_ERROR
      case LZO_E_INTERNAL_ERROR     : errorText = "internal error";        break;
    #endif
    default:                          errorText = "unknown";               break;
  }

  return errorText;
}

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
  uint     n;
  lzo_uint compressLength;
  uint     length;
  int      lzoError;
  uint32   compressLengthFlags;
  uint32   lengthFlags;

  assert(compressInfo != NULL);
  assert(compressInfo->lzo.compressFunction != NULL);
  assert(compressInfo->lzo.outputBuffer != NULL);

  if (!RingBuffer_isFull(&compressInfo->compressRingBuffer))                  // space in compress buffer
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
      compressInfo->lzo.outputBufferIndex += n;
      compressInfo->lzo.totalOutputLength += (uint64)n;
      #ifdef LZO_DEBUG
        fprintf(stderr,"%s, %d: compress: move output=%u\n",__FILE__,__LINE__,n);
      #endif /* LZO_DEBUG */
    }
  }

  if (!RingBuffer_isFull(&compressInfo->compressRingBuffer))                // space in compress buffer
  {
    // assert output buffer must be empty
    assert(compressInfo->lzo.outputBufferIndex >= compressInfo->lzo.outputBufferLength);

    // compress available data
    if (!RingBuffer_isEmpty(&compressInfo->dataRingBuffer))                 // unprocessed data available
    {
      // get max. number of data bytes
      maxDataBytes = RingBuffer_getAvailable(&compressInfo->dataRingBuffer);

      // move data buffer -> LZO input buffer
      n = MIN(LZO_BLOCK_SIZE-compressInfo->lzo.inputBufferLength,maxDataBytes);
      if (n > 0)
      {
        RingBuffer_get(&compressInfo->dataRingBuffer,
                       &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferLength],
                       n
                      );
        compressInfo->lzo.inputBufferLength += n;
        compressInfo->lzo.totalInputLength  += (uint64)n;
        #ifdef LZO_DEBUG
          fprintf(stderr,"%s, %d: compress: move input=%u\n",__FILE__,__LINE__,n);
        #endif /* LZO_DEBUG */
      }

      if ((compressInfo->lzo.inputBufferLength-compressInfo->lzo.inputBufferIndex) >= LZO_BLOCK_SIZE)
      {
        assert((compressInfo->lzo.inputBufferLength-compressInfo->lzo.inputBufferIndex) == LZO_BLOCK_SIZE);

        // compress: LZO input buffer -> LZO output buffer (spare 4 bytes for length in output buffer and compressed flag)
        #ifndef LZO_DEBUG_DISABLE_COMPRESS
          lzoError = compressInfo->lzo.compressFunction((lzo_bytep)&compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex],
                                                        LZO_BLOCK_SIZE,
                                                        &compressInfo->lzo.outputBuffer[4],
                                                        &compressLength,
                                                        compressInfo->lzo.workingMemory
                                                       );
          if (lzoError != LZO_E_OK)
          {
            return ERRORX_(DEFLATE_FAIL,lzoError,"%s",getLZOErrorText(lzoError));
          }
        #else /* LZO_DEBUG_DISABLE_COMPRESS */
          compressLength = 0;
        #endif /* not LZO_DEBUG_DISABLE_COMPRESS */
        assert(compressLength <= compressInfo->lzo.bufferSize-4);
        if ((compressLength > 0) && (compressLength < LZO_BLOCK_SIZE))
        {
          // store compressed data
          assert(compressLength < LZO_MAX_COMPRESS_LENGTH);

          // put length of compress data+flag into output buffer before data
          compressLengthFlags = ((uint32)compressLength & LZO_LENGTH_MASK) | LZO_COMPRESSED_FLAG;
          putUINT32(&compressInfo->lzo.outputBuffer[0],compressLengthFlags);
          #ifdef LZO_DEBUG
            fprintf(stderr,"%s, %d: compress: length=%u -> compressLength=%u, compressLengthFlags=%08x\n",__FILE__,__LINE__,LZO_BLOCK_SIZE,(uint)compressLength,compressLengthFlags);
          #endif /* LZO_DEBUG */

          // reset LZO input bfufer
          compressInfo->lzo.inputBufferIndex  = 0;
          compressInfo->lzo.inputBufferLength = 0;

          // init LZO output buffer
          compressInfo->lzo.outputBufferIndex  = 0;
          compressInfo->lzo.outputBufferLength = 4+(uint)compressLength;
        }
        else
        {
          // cannot compress => store original data
          length = MIN(compressInfo->lzo.inputBufferLength-compressInfo->lzo.inputBufferIndex,compressInfo->lzo.bufferSize-4);

          // move input buffer -> compress buffer (spare 4 bytes for length+flags in output buffer)
          memcpy(&compressInfo->lzo.outputBuffer[4],
                 &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex],
                 length
                );

          // put length of data+flag into output buffer before data
          lengthFlags = (uint32)length & LZO_LENGTH_MASK;
          putUINT32(&compressInfo->lzo.outputBuffer[0],lengthFlags);
          #ifdef LZO_DEBUG
            fprintf(stderr,"%s, %d: compress: length=%u, lengthFlags=%08x\n",__FILE__,__LINE__,compressInfo->lzo.inputBufferLength,lengthFlags);
          #endif /* LZO_DEBUG */

          // shift LZO input buffer
          memmove(compressInfo->lzo.inputBuffer,
                  &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex+length],
                  compressInfo->lzo.bufferSize-length
                 );
          compressInfo->lzo.inputBufferLength -= length;
          compressInfo->lzo.inputBufferIndex  =  0;

          // init LZO output buffer
          compressInfo->lzo.outputBufferIndex  = 0;
          compressInfo->lzo.outputBufferLength = 4+length;
        }

        // get max. number of data and max. number of compressed bytes
        maxCompressBytes = RingBuffer_getFree(&compressInfo->compressRingBuffer);

        // move LZO output buffer -> compress buffer
        n = MIN(compressInfo->lzo.outputBufferLength-compressInfo->lzo.outputBufferIndex,maxCompressBytes);
        if (n > 0)
        {
          RingBuffer_put(&compressInfo->compressRingBuffer,
                         &compressInfo->lzo.outputBuffer[compressInfo->lzo.outputBufferIndex],
                         n
                        );
          compressInfo->lzo.outputBufferIndex += n;
          compressInfo->lzo.totalOutputLength += (uint64)n;
        }
      }

      // update compress state
      compressInfo->compressState = COMPRESS_STATE_RUNNING;
    }
  }

  if (!RingBuffer_isFull(&compressInfo->compressRingBuffer))                // space in compress buffer
  {
    // assert output buffer must be empty
    assert(compressInfo->lzo.outputBufferIndex >= compressInfo->lzo.outputBufferLength);

    // finish compress, flush internal compress buffers
    if (   compressInfo->flushFlag                                          // flush data requested
        && (compressInfo->compressState == COMPRESS_STATE_RUNNING)          // compressor is running -> data available in internal buffers
       )
    {
      if ((compressInfo->lzo.inputBufferLength-compressInfo->lzo.inputBufferIndex) > 0)
      {
        // compress: LZO input buffer -> LZO output buffer (spare 4 bytes for length+flags in output buffer)
        #ifndef LZO_DEBUG_DISABLE_COMPRESS
          lzoError = compressInfo->lzo.compressFunction((lzo_bytep)&compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex],
                                                        compressInfo->lzo.inputBufferLength-compressInfo->lzo.inputBufferIndex,
                                                        &compressInfo->lzo.outputBuffer[4],
                                                        &compressLength,
                                                        compressInfo->lzo.workingMemory
                                                       );
          if (lzoError != LZO_E_OK)
          {
            return ERRORX_(DEFLATE_FAIL,lzoError,"%s",getLZOErrorText(lzoError));
          }
        #else /* LZO_DEBUG_DISABLE_COMPRESS */
          compressLength = 0;
        #endif /* not LZO_DEBUG_DISABLE_COMPRESS */
        assert(compressLength <= compressInfo->lzo.bufferSize-4);
        if ((compressLength > 0) && (compressLength < (compressInfo->lzo.inputBufferLength-compressInfo->lzo.inputBufferIndex)))
        {
          // store compressed data
          assert(compressLength < LZO_MAX_COMPRESS_LENGTH);

          // put length of compressed data+flags into output buffer before data
          compressLengthFlags = ((uint32)compressLength & LZO_LENGTH_MASK) | LZO_COMPRESSED_FLAG | LZO_END_OF_DATA_FLAG;
          putUINT32(&compressInfo->lzo.outputBuffer[0],compressLengthFlags);
          #ifdef LZO_DEBUG
            fprintf(stderr,"%s, %d: compress: final length=%u -> compressLength=%u, compressLengthFlags=%08x\n",__FILE__,__LINE__,compressInfo->lzo.inputBufferLength-compressInfo->lzo.inputBufferIndex,(uint)compressLength,compressLengthFlags);
          #endif /* LZO_DEBUG */

          // reset LZO input bfufer
          compressInfo->lzo.inputBufferIndex  = 0;
          compressInfo->lzo.inputBufferLength = 0;

          // init LZO output buffer
          compressInfo->lzo.outputBufferIndex  = 0;
          compressInfo->lzo.outputBufferLength = 4+(uint)compressLength;
        }
        else
        {
          // cannot compress => store original data
          length = MIN(compressInfo->lzo.inputBufferLength-compressInfo->lzo.inputBufferIndex,compressInfo->lzo.bufferSize-4);

          // transfer: input buffer -> compress buffer
          memcpy(&compressInfo->lzo.outputBuffer[4],
                 &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex],
                 length
                );

          // put length of data+end-of-data flag into output buffer before data
          lengthFlags = ((uint32)length & LZO_LENGTH_MASK) | LZO_END_OF_DATA_FLAG;
          putUINT32(&compressInfo->lzo.outputBuffer[0],lengthFlags);
          #ifdef LZO_DEBUG
            fprintf(stderr,"%s, %d: compress: final length=%d, lengthFlags=%08x\n",__FILE__,__LINE__,compressInfo->lzo.inputBufferLength-compressInfo->lzo.inputBufferIndex,lengthFlags);
          #endif /* LZO_DEBUG */

          // shift LZO input buffer
          memmove(compressInfo->lzo.inputBuffer,
                  &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex+length],
                  compressInfo->lzo.bufferSize-length
                 );
          compressInfo->lzo.inputBufferLength -= length;
          compressInfo->lzo.inputBufferIndex  =  0;

          // init LZO output buffer
          compressInfo->lzo.outputBufferIndex  = 0;
          compressInfo->lzo.outputBufferLength = 4+length;
        }

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
          compressInfo->lzo.outputBufferIndex += n;
          compressInfo->lzo.totalOutputLength += (uint64)n;
          #ifdef LZO_DEBUG
            fprintf(stderr,"%s, %d: compress: move output=%u\n",__FILE__,__LINE__,n);
          #endif /* LZO_DEBUG */
        }
      }
      else
      {
        #ifdef LZO_DEBUG
          fprintf(stderr,"%s, %d: compress: end of data\n",__FILE__,__LINE__);
        #endif /* LZO_DEBUG */
        compressInfo->endOfDataFlag = TRUE;
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
  uint     n;
  uint32   compressLengthFlags;
  lzo_uint compressLength;
  int      lzoError;
  lzo_uint length;

  assert(compressInfo != NULL);
  assert(compressInfo->lzo.compressFunction != NULL);
  assert(compressInfo->lzo.inputBuffer != NULL);
  assert(compressInfo->lzo.outputBuffer != NULL);

  if (!RingBuffer_isFull(&compressInfo->dataRingBuffer))                      // space in data buffer
  {
    // get max. number of data bytes
    maxCompressBytes = RingBuffer_getAvailable(&compressInfo->compressRingBuffer);

    // move LZO output buffer -> data buffer
    n = MIN(compressInfo->lzo.outputBufferLength-compressInfo->lzo.outputBufferIndex,maxCompressBytes);
    if (n > 0)
    {
      RingBuffer_put(&compressInfo->dataRingBuffer,
                     &compressInfo->lzo.outputBuffer[compressInfo->lzo.outputBufferIndex],
                     n
                    );
      compressInfo->lzo.outputBufferIndex += n;
      compressInfo->lzo.totalOutputLength += (uint64)n;
      #ifdef LZO_DEBUG
        fprintf(stderr,"%s, %d: decompress: move output=%u\n",__FILE__,__LINE__,n);
      #endif /* LZO_DEBUG */
    }
  }

  if (!RingBuffer_isFull(&compressInfo->dataRingBuffer))                      // space in data buffer
  {
    if (!compressInfo->lzo.lastBlockFlag)                                     // not end-of-data
    {
      assert(compressInfo->lzo.outputBufferIndex >= compressInfo->lzo.outputBufferLength);

      // decompress available data
      if (!RingBuffer_isEmpty(&compressInfo->compressRingBuffer))             // unprocessed compressed data available
      {
        // get max. number of compressed bytes
        maxCompressBytes = RingBuffer_getAvailable(&compressInfo->compressRingBuffer);

        // move compress buffer -> LZO input buffer
        n = MIN(compressInfo->lzo.bufferSize-compressInfo->lzo.inputBufferLength,maxCompressBytes);
        if (n > 0)
        {
          RingBuffer_get(&compressInfo->compressRingBuffer,
                         &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferLength],
                         n
                        );
          compressInfo->lzo.inputBufferLength += n;
          compressInfo->lzo.totalInputLength  += (uint64)n;
          #ifdef LZO_DEBUG
            fprintf(stderr,"%s, %d: decompress: move input=%u\n",__FILE__,__LINE__,n);
          #endif /* LZO_DEBUG */
        }
      }

      if ((compressInfo->lzo.inputBufferLength-compressInfo->lzo.inputBufferIndex) >= 4)
      {
        // decompress: get length of compressed data and flags
        compressLengthFlags = getUINT32(&compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex]);
        #ifdef LZO_DEBUG
          fprintf(stderr,"%s, %d: compressLengthFlags=%08x\n",__FILE__,__LINE__,compressLengthFlags);
        #endif /* LZO_DEBUG */
        if ((compressLengthFlags & LZO_COMPRESSED_FLAG) == LZO_COMPRESSED_FLAG)
        {
          // compressed data block
          compressLength = (lzo_uint)(compressLengthFlags & LZO_LENGTH_MASK);
          if ((compressLength <= 0) || ((uint)(4+compressLength) >= compressInfo->lzo.bufferSize))
          {
            return ERRORX_(INFLATE_FAIL,0,"invalid compressed data size");
          }

          if ((compressInfo->lzo.inputBufferLength-compressInfo->lzo.inputBufferIndex) >= (uint)(4+compressLength)) // enough compress data available
          {
            // decompress: LZO input buffer -> LZO output buffer (byte 0..3 are length+flag)
            length = (lzo_uint)compressInfo->lzo.bufferSize;
            lzoError = compressInfo->lzo.decompressFunction((lzo_bytep)&compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex+4],
                                                            compressLength,
                                                            compressInfo->lzo.outputBuffer,
                                                            &length,
                                                            compressInfo->lzo.workingMemory
                                                           );
            if (lzoError != LZO_E_OK)
            {
              return ERRORX_(INFLATE_FAIL,lzoError,"%s",getLZOErrorText(lzoError));
            }
            assert(length <= compressInfo->lzo.bufferSize);

            #ifdef LZO_DEBUG
              fprintf(stderr,"%s, %d: decompress: compressLengthFlags=%08x, compressLength=%u -> length=%u\n",__FILE__,__LINE__,compressLengthFlags,(uint)compressLength,(uint)length);
            #endif /* LZO_DEBUG */

            // shift LZO input buffer
            memmove(compressInfo->lzo.inputBuffer,
                    &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex+4+compressLength],
                    compressInfo->lzo.inputBufferLength-(compressInfo->lzo.inputBufferIndex+4+compressLength)
                   );
            compressInfo->lzo.inputBufferLength -= compressInfo->lzo.inputBufferIndex+4+compressLength;
            compressInfo->lzo.inputBufferIndex  =  0;

            // init LZO output buffer
            compressInfo->lzo.outputBufferIndex  = 0;
            compressInfo->lzo.outputBufferLength = (uint)length;

            // check if last block
            compressInfo->lzo.lastBlockFlag = ((compressLengthFlags & LZO_END_OF_DATA_FLAG) == LZO_END_OF_DATA_FLAG);
          }
        }
        else
        {
          // not compressed data block
          length = (lzo_uint)(compressLengthFlags & LZO_LENGTH_MASK);
          if ((length <= 0) || ((uint)(4+length) >= compressInfo->lzo.bufferSize))
          {
            return ERRORX_(INFLATE_FAIL,0,"invalid decompressed data size");
          }

          if ((compressInfo->lzo.inputBufferLength-compressInfo->lzo.inputBufferIndex) >= (uint)(4+length)) // enough data available
          {
            #ifdef LZO_DEBUG
              fprintf(stderr,"%s, %d: decompress: compressLengthFlags=%08x, length=%u\n",__FILE__,__LINE__,compressLengthFlags,(uint)length);
            #endif /* LZO_DEBUG */

            // transfer: LZO input buffer -> LZO output buffer (byte 0..3 are length+flags)
            memcpy(compressInfo->lzo.outputBuffer,
                   &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex+4],
                   length
                  );

            // shift LZO input buffer
            memmove(compressInfo->lzo.inputBuffer,
                    &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex+4+length],
                    compressInfo->lzo.inputBufferLength-(compressInfo->lzo.inputBufferIndex+4+length)
                   );
            compressInfo->lzo.inputBufferLength -= compressInfo->lzo.inputBufferIndex+4+length;
            compressInfo->lzo.inputBufferIndex  =  0;

            // init LZO output buffer
            compressInfo->lzo.outputBufferIndex  = 0;
            compressInfo->lzo.outputBufferLength = (uint)length;

            // check if last block
            compressInfo->lzo.lastBlockFlag = ((compressLengthFlags & LZO_END_OF_DATA_FLAG) == LZO_END_OF_DATA_FLAG);
          }
        }

        // get max. number of bytes free in data buffer
        maxDataBytes = RingBuffer_getFree(&compressInfo->dataRingBuffer);

        // move LZO output buffer -> data buffer
        n = MIN(compressInfo->lzo.outputBufferLength-compressInfo->lzo.outputBufferIndex,maxDataBytes);
        if (n > 0)
        {
          RingBuffer_put(&compressInfo->dataRingBuffer,
                         &compressInfo->lzo.outputBuffer[compressInfo->lzo.outputBufferIndex],
                         n
                        );
          compressInfo->lzo.outputBufferIndex += n;
          compressInfo->lzo.totalOutputLength += (uint64)n;
          #ifdef LZO_DEBUG
            fprintf(stderr,"%s, %d: decompress: move output=%u\n",__FILE__,__LINE__,n);
          #endif /* LZO_DEBUG */
        }

        // update compress state
        compressInfo->compressState = COMPRESS_STATE_RUNNING;
      }
    }

    if (!RingBuffer_isFull(&compressInfo->dataRingBuffer))                    // space in data buffer
    {
      assert(compressInfo->lzo.outputBufferIndex >= compressInfo->lzo.outputBufferLength);
      // finish decompress, flush internal decompress buffers
      if (   compressInfo->flushFlag                                          // flush data requested
          && (compressInfo->compressState == COMPRESS_STATE_RUNNING)          // compressor is running -> data available in internal buffers
         )
      {
        // get max. number of compressed bytes
        maxCompressBytes = RingBuffer_getAvailable(&compressInfo->compressRingBuffer);

        // move compress buffer -> LZO input buffer
        n = MIN(compressInfo->lzo.bufferSize-compressInfo->lzo.inputBufferLength,maxCompressBytes);
        if (n > 0)
        {
          RingBuffer_get(&compressInfo->compressRingBuffer,
                         &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferLength],
                         n
                        );
          compressInfo->lzo.inputBufferLength += n;
          compressInfo->lzo.totalInputLength  += (uint64)n;
          #ifdef LZO_DEBUG
            fprintf(stderr,"%s, %d: decompress: move input=%u\n",__FILE__,__LINE__,n);
          #endif /* LZO_DEBUG */
        }

        if (!compressInfo->lzo.lastBlockFlag)
        {
          if ((compressInfo->lzo.inputBufferLength-compressInfo->lzo.inputBufferIndex) >= 4)
          {
            // decompress: get length of compressed data and flags from LZO input buffer
            compressLengthFlags = getUINT32(&compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex]);
            if ((compressLengthFlags & LZO_COMPRESSED_FLAG) == LZO_COMPRESSED_FLAG)
            {
              // compressed data block -> decompress
              compressLength = (lzo_uint)(compressLengthFlags & LZO_LENGTH_MASK);
              if ((compressLength <= 0) || ((uint)(4+compressLength) >= compressInfo->lzo.bufferSize))
              {
                return ERRORX_(INFLATE_FAIL,0,"invalid compressed data size");
              }

              if ((compressInfo->lzo.inputBufferLength-compressInfo->lzo.inputBufferIndex) >= (uint)(4+compressLength))
              {
                // decompress: LZO input buffer -> LZO output buffer (byte 0..3 are length+flags)
                length = (lzo_uint)compressInfo->lzo.bufferSize;
                lzoError = compressInfo->lzo.decompressFunction((lzo_bytep)&compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex+4],
                                                                compressLength,
                                                                compressInfo->lzo.outputBuffer,
                                                                &length,
                                                                compressInfo->lzo.workingMemory
                                                               );
                if (lzoError != LZO_E_OK)
                {
                  return ERRORX_(INFLATE_FAIL,lzoError,"%s",getLZOErrorText(lzoError));
                }
                assert(length <= compressInfo->lzo.bufferSize);

                #ifdef LZO_DEBUG
                  fprintf(stderr,"%s, %d: decompress: compressLengthFlags=%08x compressLength=%u -> %u\n",__FILE__,__LINE__,compressLengthFlags,(uint)compressLength,(uint)length);
                #endif /* LZO_DEBUG */

                // shift LZO input buffer
                memmove(compressInfo->lzo.inputBuffer,
                        &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex+4+compressLength],
                        compressInfo->lzo.inputBufferLength-(compressInfo->lzo.inputBufferIndex+4+compressLength)
                       );
                compressInfo->lzo.inputBufferLength -= compressInfo->lzo.inputBufferIndex+4+compressLength;
                compressInfo->lzo.inputBufferIndex  =  0;

                // init LZO output buffer
                compressInfo->lzo.outputBufferIndex  = 0;
                compressInfo->lzo.outputBufferLength = (uint)length;

                // check if last block
                compressInfo->lzo.lastBlockFlag = ((compressLengthFlags & LZO_END_OF_DATA_FLAG) == LZO_END_OF_DATA_FLAG);
              }
            }
            else
            {
              // not compressed data block
              length = (lzo_uint)(compressLengthFlags & LZO_LENGTH_MASK);
              if ((length <= 0) || ((uint)(4+length) >= compressInfo->lzo.bufferSize))
              {
                return ERRORX_(INFLATE_FAIL,0,"invalid decompressed data size");
              }

              if ((compressInfo->lzo.inputBufferLength-compressInfo->lzo.inputBufferIndex) >= (uint)(4+length))
              {
                #ifdef LZO_DEBUG
                  fprintf(stderr,"%s, %d: decompress: final decompressed length=%u\n",__FILE__,__LINE__,(uint)length);
                #endif /* LZO_DEBUG */

                // transfer: LZO input buffer -> LZO output buffer (byte 0..3 are length+flags)
                assert(compressInfo->lzo.outputBufferIndex >= compressInfo->lzo.outputBufferLength);
                memcpy(compressInfo->lzo.outputBuffer,
                       &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex+4],
                       length
                      );

                // shift LZO input buffer
                memmove(compressInfo->lzo.inputBuffer,
                        &compressInfo->lzo.inputBuffer[compressInfo->lzo.inputBufferIndex+4+length],
                        compressInfo->lzo.inputBufferLength-(compressInfo->lzo.inputBufferIndex+4+length)
                       );
                compressInfo->lzo.inputBufferLength -= compressInfo->lzo.inputBufferIndex+4+length;
                compressInfo->lzo.inputBufferIndex  =  0;

                // init LZO output buffer
                compressInfo->lzo.outputBufferIndex  = 0;
                compressInfo->lzo.outputBufferLength = (uint)length;

                // check if last block
                compressInfo->lzo.lastBlockFlag = ((compressLengthFlags & LZO_END_OF_DATA_FLAG) == LZO_END_OF_DATA_FLAG);
              }
            }

            // get max. number of bytes free in data buffer
            maxDataBytes  = RingBuffer_getFree(&compressInfo->dataRingBuffer);

            // move LZO output buffer -> data buffer
            n = MIN(compressInfo->lzo.outputBufferLength-compressInfo->lzo.outputBufferIndex,maxDataBytes);
            if (n > 0)
            {
              RingBuffer_put(&compressInfo->dataRingBuffer,
                             &compressInfo->lzo.outputBuffer[compressInfo->lzo.outputBufferIndex],
                             n
                            );
              compressInfo->lzo.outputBufferIndex += n;
              compressInfo->lzo.totalOutputLength += (uint64)n;
            }
          }
        }
        else
        {
          #ifdef LZO_DEBUG
            fprintf(stderr,"%s, %d: decompress: end of data\n",__FILE__,__LINE__);
          #endif /* LZO_DEBUG */
          compressInfo->endOfDataFlag = TRUE;
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
  ulong workingMemorySize;

  assert(compressInfo != NULL);

  UNUSED_VARIABLE(compressMode);

  compressInfo->lzo.inputBufferIndex   = 0;
  compressInfo->lzo.inputBufferLength  = 0;
  compressInfo->lzo.outputBufferIndex  = 0;
  compressInfo->lzo.outputBufferLength = 0;
  compressInfo->lzo.lastBlockFlag      = FALSE;
  compressInfo->lzo.totalInputLength   = 0LL;
  compressInfo->lzo.totalOutputLength  = 0LL;

  switch (compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_LZO_1:
      compressInfo->lzo.compressFunction   = lzo1x_1_11_compress;
      compressInfo->lzo.decompressFunction = lzo1x_decompress_safe;
      compressInfo->lzo.bufferSize         = 4+LZO_BLOCK_SIZE+(LZO_BLOCK_SIZE/16)+64+3; // 4 bytes lenght+max. LZO buffer
      workingMemorySize                    = LZO1X_1_11_MEM_COMPRESS;
      break;
    case COMPRESS_ALGORITHM_LZO_2:
      compressInfo->lzo.compressFunction   = lzo1x_1_12_compress;
      compressInfo->lzo.decompressFunction = lzo1x_decompress_safe;
      compressInfo->lzo.bufferSize         = 4+LZO_BLOCK_SIZE+(LZO_BLOCK_SIZE/16)+64+3; // 4 bytes lenght+max. LZO buffer
      workingMemorySize                    = LZO1X_1_12_MEM_COMPRESS;
      break;
    case COMPRESS_ALGORITHM_LZO_3:
      compressInfo->lzo.compressFunction   = lzo1x_1_15_compress;
      compressInfo->lzo.decompressFunction = lzo1x_decompress_safe;
      compressInfo->lzo.bufferSize         = 4+LZO_BLOCK_SIZE+(LZO_BLOCK_SIZE/16)+64+3; // 4 bytes lenght+max. LZO buffer
      workingMemorySize                    = LZO1X_1_15_MEM_COMPRESS;
      break;
    case COMPRESS_ALGORITHM_LZO_4:
      compressInfo->lzo.compressFunction   = lzo1x_1_compress;
      compressInfo->lzo.decompressFunction = lzo1x_decompress_safe;
      compressInfo->lzo.bufferSize         = 4+LZO_BLOCK_SIZE+(LZO_BLOCK_SIZE/16)+64+3; // 4 bytes lenght+max. LZO buffer
      workingMemorySize                    = LZO1X_1_MEM_COMPRESS;
      break;
    case COMPRESS_ALGORITHM_LZO_5:
      compressInfo->lzo.compressFunction   = lzo1x_999_compress;
      compressInfo->lzo.decompressFunction = lzo1x_decompress_safe;
      compressInfo->lzo.bufferSize         = 4+LZO_BLOCK_SIZE+(LZO_BLOCK_SIZE/16)+64+3; // 4 bytes lenght+max. LZO buffer
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
