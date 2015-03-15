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
#include <lz4.h>
#include <lz4hc.h>
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

#define LZ4_BLOCK_SIZE             (64*KB)
//#define LZ4_DECOMPRESS_PREFIX (64*KB)
//#define LZ4_CACHELINE_SIZE    64
#define LZ4_DICTIONARY_BUFFER_SIZE (64*KB)

#define LZ4_MAX_COMPRESS_LENGTH 0x00FFFFFF
#define LZ4_LENGTH_MASK         0x00FFFFFF
#define LZ4_END_OF_DATA_FLAG    0x80000000   // set iff last block
#define LZ4_COMPRESSED_FLAG     0x40000000   // set iff block is compressed
#define LZ4_STREAM_FLAG         0x20000000   // set iff streaming is used, otherwise single block compression

#define LZ4_OK   0
#define LZ4_FAIL -1

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** ForwardDICTIONARY_BUFFER_SIZEs ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : isHC
* Purpose: check if HC compression
* Input  : compressionLevel - compression level
* Output : -
* Return : TRUE iff HC compression
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isHC(uint compressionLevel)
{
  return (compressionLevel > 0);
}

#ifdef LZ4_STREAM
/***********************************************************************\
* Name   : lz4InitStream
* Purpose: init LZ4 stream
* Input  : compressionLevel - compression level
* Output : -
* Return : stream handle or NULL
* Notes  : -
\***********************************************************************/

LOCAL void *lz4InitStream(uint compressionLevel)
{
  void *handle;

  if (isHC(compressionLevel))
  {
    handle = LZ4_createStreamHC();
    if (handle != NULL)
    {
      LZ4_resetStreamHC((LZ4_streamHC_t*)handle,compressionLevel);
    }
  }
  else
  {
    handle = LZ4_createStream();
    if (handle != NULL)
    {
      LZ4_resetStream((LZ4_stream_t*)handle);
    }
  }

  return handle;
}

/***********************************************************************\
* Name   : lz4DoneStream
* Purpose: done LZ4 stream
* Input  : handle           - stream handle
*          compressionLevel - compression level
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void lz4DoneStream(void *handle, uint compressionLevel)
{
  assert(handle != NULL);

  if (isHC(compressionLevel))
  {
    LZ4_freeStreamHC((LZ4_streamHC_t*)handle);
  }
  else
  {
    LZ4_freeStream((LZ4_stream_t*)handle);
  }
}
#endif /* LZ4_STREAM */

/***********************************************************************\
* Name   : lz4CompressBlock
* Purpose: LZ4 compress block
* Input  : handle            - LZ4 handle
*          inputBuffer       - input buffer
*          inputBufferLength - input buffer lenght
*          outputBuffer      - output buffer
*          outputBufferSize  - output buffer size
*          compressionLevel  - compression level
* Output : outputBufferLength - number of compressed bytes
* Return : LZ4_OK if compressed, LZ4_FAIL otherwise
* Notes  : -
\***********************************************************************/

LOCAL int lz4CompressBlock(CompressInfo *compressInfo,
                           const byte   *inputBuffer,
                           uint         inputBufferLength,
                           byte         *outputBuffer,
                           uint         outputBufferSize,
                           uint         *outputBufferLength
                          )
{
  int result;

  assert(compressInfo != NULL);
  assert(inputBuffer != NULL);
  assert(outputBuffer != NULL);
  assert(outputBufferLength != NULL);

  if (isHC(compressInfo->lz4.compressionLevel))
  {
    #ifdef LZ4_STREAM
      result = LZ4_compressHC_limitedOutput_continue(compressInfo->stream.compressHC,
                                                     (const char*)inputBuffer,
                                                     (char*)outputBuffer,
                                                     (int)inputBufferLength,
                                                     (int)outputBufferSize
                                                    );
    if (result <= 0)
    {
      return LZ4_FAIL;
    }

    #else /* not LZ4_STREAM */
      result = LZ4_compressHC_limitedOutput((const char*)inputBuffer,
                                            (char*)outputBuffer,
                                            (int)inputBufferLength,
                                            (int)outputBufferSize
                                           );
      if (result <= 0)
      {
       return LZ4_FAIL;
      }
    #endif /* LZ4_STREAM */
  }
  else
  {
    #ifdef LZ4_STREAM
      result = LZ4_compress_limitedOutput_continue(compressInfo->stream.compress,
                                                   (const char*)inputBuffer,
                                                   (char*)outputBuffer,
                                                   (int)inputBufferLength,
                                                   (int)outputBufferSize
                                                  );
      if (result <= 0)
      {
       return LZ4_FAIL;
      }


      /* Note: bug in LZ4? If the dictionary buffer is not saved decompression
         will fail.
      */
      LZ4_saveDict((LZ4_stream_t*)handle,(char*)dictionaryBuffer,LZ4_DICTIONARY_BUFFER_SIZE);
    #else /* not LZ4_STREAM */
      result = LZ4_compress_limitedOutput((const char*)inputBuffer,
                                          (char*)outputBuffer,
                                          (int)inputBufferLength,
                                          (int)outputBufferSize
                                         );
      if (result <= 0)
      {
       return LZ4_FAIL;
      }
    #endif /* LZ4_STREAM */
  }
  assert((uint)result <= outputBufferSize);
  (*outputBufferLength) = (uint)result;

  return LZ4_OK;
}

/***********************************************************************\
* Name   : lz4DecompressBlock
* Purpose: LZ4 compress block
* Input  : inputBuffer       - input buffer
*          inputBufferLength - input buffer lenght
*          outputBuffer      - output buffer
*          outputBufferSize  - output buffer size
* Output : outputBufferLength - length of data in output buffer
* Return : LZ4_OK if decompressed, LZ4_FAIL otherwise
* Notes  : -
\***********************************************************************/

LOCAL_INLINE int lz4DecompressBlock(CompressInfo *compressInfo,
                                    const byte   *inputBuffer,
                                    uint         inputBufferLength,
                                    byte         *outputBuffer,
                                    uint         outputBufferSize,
                                    uint         *outputBufferLength
                                   )
{
  int result;

  assert(compressInfo != NULL);
  assert(inputBuffer != NULL);
  assert(outputBuffer != NULL);
  assert(outputBufferLength != NULL);

  #ifdef LZ4_STREAM
    result = LZ4_decompress_safe_continue(compressInfo->stream.decompress,
                                          (const char*)inputBuffer,
                                          (char*)outputBuffer,
                                          (int)inputBufferLength,
                                          (int)outputBufferSize
                                         );
    if (result < 0)
    {
      return LZ4_FAIL;
    }

    /* Note: The output buffer must be saved to keep the dictionary
       for stream decompression. See also note on lz4CompressBlock
       for stream compression.
    */
    memcpy(dictionaryBuffer,outputBuffer,MIN(result,LZ4_DICTIONARY_BUFFER_SIZE));
    LZ4_setStreamDecode(handle,(char*)dictionaryBuffer,MIN(result,LZ4_DICTIONARY_BUFFER_SIZE));
  #else /* not LZ4_STREAM */
    result = LZ4_decompress_safe((const char*)inputBuffer,
                                 (char*)outputBuffer,
                                 (int)inputBufferLength,
                                 (int)outputBufferSize
                                );
    if (result < 0)
    {
      return LZ4_FAIL;
    }
  #endif /* LZ4_STREAM */
  assert((uint)result <= outputBufferSize);
  (*outputBufferLength) = (uint)result;

  return LZ4_OK;
}

/***********************************************************************\
* Name   : CompressLZ4_compressData
* Purpose: compress data with LZ4
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

#define _XXX
uint ii=0;
LOCAL Errors CompressLZ4_compressData(CompressInfo *compressInfo)
{
  ulong  maxCompressBytes,maxDataBytes;
  uint   n;
  int    lz4Result;
  uint   compressLength;
  uint32 compressLengthFlags;
  uint32 lengthFlags;

  assert(compressInfo != NULL);
  assert(compressInfo->lz4.inputBuffer != NULL);
  assert(compressInfo->lz4.outputBuffer != NULL);

  if (!compressInfo->endOfDataFlag)                                           // not end-of-data
  {
    if (!RingBuffer_isFull(&compressInfo->compressRingBuffer))                // space in compress buffer
    {
      // get max. number of compressed bytes
      maxCompressBytes = RingBuffer_getFree(&compressInfo->compressRingBuffer);

      // move LZ4 output buffer -> compress buffer
      n = MIN(compressInfo->lz4.outputBufferLength-compressInfo->lz4.outputBufferIndex,maxCompressBytes);
      if (n > 0)
      {
        RingBuffer_put(&compressInfo->compressRingBuffer,
                       &compressInfo->lz4.outputBuffer[compressInfo->lz4.outputBufferIndex],
                       n
                      );
        compressInfo->lz4.outputBufferIndex += n;
        compressInfo->lz4.totalOutputLength += (uint64)n;
      }
    }

    if (!RingBuffer_isFull(&compressInfo->compressRingBuffer))                // space in compress buffer
    {
      assert(compressInfo->lz4.outputBufferIndex >= compressInfo->lz4.outputBufferLength);

      // compress available data
      if (!RingBuffer_isEmpty(&compressInfo->dataRingBuffer))                 // unprocessed data available
      {
        // get max. number of data bytes
        maxDataBytes = RingBuffer_getAvailable(&compressInfo->dataRingBuffer);

        // move data buffer -> LZ4 input buffer
        n = MIN(compressInfo->lz4.inputBufferSize-compressInfo->lz4.inputBufferLength,maxDataBytes);
        if (n > 0)
        {
          RingBuffer_get(&compressInfo->dataRingBuffer,
                         &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferLength],
                         n
                        );
          compressInfo->lz4.inputBufferLength += n;
          compressInfo->lz4.totalInputLength  += (uint64)n;
        }

        if ((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) >= LZ4_BLOCK_SIZE)
        {
          // compress: LZ4 input buffer -> LZ4 output buffer (spare 4 bytes for length+flags in output buffer)
          lz4Result = lz4CompressBlock(compressInfo,
                                       &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex],
                                       compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex,
                                       &compressInfo->lz4.outputBuffer[4],
                                       compressInfo->lz4.outputBufferSize-4,
                                       &compressLength
                                      );
          if (lz4Result == LZ4_OK)
          {
ii++;
fprintf(stderr,"%s, %d: compress %u: compressLength=%d\n",__FILE__,__LINE__,ii,compressLength);
            // store compressed data
            assert(compressLength < LZ4_MAX_COMPRESS_LENGTH);

            // put compress length of data+compress flags into output buffer before data
            compressLengthFlags = ((uint32)compressLength & LZ4_LENGTH_MASK) | LZ4_COMPRESSED_FLAG;
            #ifdef LZ4_STREAM
              compressLengthFlags |= LZ4_STREAM_FLAG;
            #endif /* LZ4_STREAM */
            putUINT32(&compressInfo->lz4.outputBuffer[0],compressLengthFlags);

            // init LZ4 output buffer
            compressInfo->lz4.outputBufferIndex  = 0;
            compressInfo->lz4.outputBufferLength = 4+(uint)compressLength;
          }
          else
          {
ii++;
fprintf(stderr,"%s, %d: compress %u: length=%d\n",__FILE__,__LINE__,ii,compressInfo->lz4.inputBufferLength);
            // cannot compress => store original data

            // move input buffer -> compress buffer (spare 4 bytes for length+flags in output buffer)
            memcpy(&compressInfo->lz4.outputBuffer[4],
                   &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex],
                   compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex
                  );

            // put length of data into output buffer before data
            lengthFlags = ((uint32)(compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) & LZ4_LENGTH_MASK);
            putUINT32(&compressInfo->lz4.outputBuffer[0],lengthFlags);

            // init LZ4 output buffer
            compressInfo->lz4.outputBufferIndex  = 0;
            compressInfo->lz4.outputBufferLength = 4+compressInfo->lz4.inputBufferLength;
          }
          compressInfo->lz4.inputBufferIndex  = 0;
          compressInfo->lz4.inputBufferLength = 0;

          // get max. number of compressed bytes
          maxCompressBytes = RingBuffer_getFree(&compressInfo->compressRingBuffer);

          // move LZ4 output buffer -> compress buffer
          n = MIN(compressInfo->lz4.outputBufferLength-compressInfo->lz4.outputBufferIndex,maxCompressBytes);
          if (n > 0)
          {
            RingBuffer_put(&compressInfo->compressRingBuffer,
                           &compressInfo->lz4.outputBuffer[compressInfo->lz4.outputBufferIndex],
                           n
                          );
            compressInfo->lz4.outputBufferIndex += n;
            compressInfo->lz4.totalOutputLength += (uint64)n;
          }
        }

        // update compress state
        compressInfo->compressState = COMPRESS_STATE_RUNNING;
      }
    }

    if (!RingBuffer_isFull(&compressInfo->compressRingBuffer))                // space in compress buffer
    {
      assert(compressInfo->lz4.outputBufferIndex >= compressInfo->lz4.outputBufferLength);

      // finish compress, flush internal compress buffers
      if (   compressInfo->flushFlag                                          // flush data requested
          && (compressInfo->compressState == COMPRESS_STATE_RUNNING)          // compressor is running -> data available in internal buffers
         )
      {
        if ((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) > 0)
        {
          n = MIN(compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex,LZ4_BLOCK_SIZE);

          // compress: LZ4 input buffer -> LZ4 output buffer (spare 4 bytes for length+flags in output buffer)
          lz4Result = lz4CompressBlock(compressInfo,
                                       &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex],
                                       compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex,
                                       &compressInfo->lz4.outputBuffer[4],
                                       compressInfo->lz4.outputBufferSize-4,
                                       &compressLength
                                      );
          if (lz4Result == LZ4_OK)
          {
ii++;
fprintf(stderr,"%s, %d: compress %u: final compressLength=%d\n",__FILE__,__LINE__,ii,compressLength);
            // store compressed data
            assert(compressLength < LZ4_MAX_COMPRESS_LENGTH);


            // put compress length of data+compress flags into output buffer before compressed data, set compressed flag
            compressLengthFlags = ((uint32)compressLength & LZ4_LENGTH_MASK) | LZ4_COMPRESSED_FLAG | LZ4_END_OF_DATA_FLAG;
            #ifdef LZ4_STREAM
              compressLengthFlags |= LZ4_STREAM_FLAG;
            #endif /* LZ4_STREAM */
            putUINT32(&compressInfo->lz4.outputBuffer[0],compressLengthFlags);

            // init LZ4 output buffer
            compressInfo->lz4.outputBufferIndex  = 0;
            compressInfo->lz4.outputBufferLength = 4+(uint)compressLength;
          }
          else
          {
ii++;
fprintf(stderr,"%s, %d: compress %u: final length=%d\n",__FILE__,__LINE__,ii,compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex);
            // cannot compress => store original data

            // transfer: input buffer -> compress buffer
            memcpy(&compressInfo->lz4.outputBuffer[4],
                   &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex],
                   compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex
                  );
            // put length of data into output buffer before data
            lengthFlags = ((uint32)(compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) & LZ4_LENGTH_MASK) | LZ4_END_OF_DATA_FLAG;
            putUINT32(&compressInfo->lz4.outputBuffer[0],lengthFlags);

            // init LZ4 output buffer
            compressInfo->lz4.outputBufferIndex  = 0;
            compressInfo->lz4.outputBufferLength = 4+(uint)compressInfo->lz4.inputBufferLength;
          }
          compressInfo->endOfDataFlag = TRUE;

          compressInfo->lz4.inputBufferIndex  = 0;
          compressInfo->lz4.inputBufferLength = 0;

          // get max. number of compressed bytes
          maxCompressBytes = RingBuffer_getFree(&compressInfo->compressRingBuffer);

          // move LZ4 output buffer -> compress buffer
          n = MIN(compressInfo->lz4.outputBufferLength-compressInfo->lz4.outputBufferIndex,maxCompressBytes);
          if (n > 0)
          {
            RingBuffer_put(&compressInfo->compressRingBuffer,
                           &compressInfo->lz4.outputBuffer[compressInfo->lz4.outputBufferIndex],
                           n
                          );
            compressInfo->lz4.outputBufferIndex += n;
            compressInfo->lz4.totalOutputLength += (uint64)n;
          }
        }
      }
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : CompressLZ4_decompressData
* Purpose: decompress data with LZ4
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
LOCAL Errors CompressLZ4_decompressData(CompressInfo *compressInfo)
{
  ulong  maxCompressBytes,maxDataBytes;
  uint   n;
  uint32 compressLengthFlags;
  uint   compressLength;
  int    lz4Result;
  uint   length;

  assert(compressInfo != NULL);
  assert(compressInfo->lz4.inputBuffer != NULL);
  assert(compressInfo->lz4.outputBuffer != NULL);

  if (!compressInfo->endOfDataFlag)                                           // not end-of-data
  {
    if (!RingBuffer_isFull(&compressInfo->dataRingBuffer))                    // space in compress buffer
    {
      // get max. number of compressed bytes
      maxCompressBytes = RingBuffer_getAvailable(&compressInfo->compressRingBuffer);

      // move LZ4 output buffer -> data buffer
      n = MIN(compressInfo->lz4.outputBufferLength-compressInfo->lz4.outputBufferIndex,maxCompressBytes);
      if (n > 0)
      {
        RingBuffer_put(&compressInfo->dataRingBuffer,
                       &compressInfo->lz4.outputBuffer[compressInfo->lz4.outputBufferIndex],
                       n
                      );
        compressInfo->lz4.outputBufferIndex += n;
        compressInfo->lz4.totalOutputLength += (uint64)n;
      }
    }

    if (!RingBuffer_isFull(&compressInfo->dataRingBuffer))                    // space in data buffer
    {
      assert(compressInfo->lz4.outputBufferIndex >= compressInfo->lz4.outputBufferLength);

      // decompress data available
      if (!RingBuffer_isEmpty(&compressInfo->compressRingBuffer))             // unprocessed compressed data available
      {
        // get max. number of compressed bytes
        maxCompressBytes = RingBuffer_getAvailable(&compressInfo->compressRingBuffer);

        // move compress buffer -> LZ4 input buffer
        n = MIN(compressInfo->lz4.inputBufferSize-compressInfo->lz4.inputBufferLength,maxCompressBytes);
        if (n > 0)
        {
//fprintf(stderr,"%s, %d: decompress: shift into lz4 inpout %d\n",__FILE__,__LINE__,n);
          RingBuffer_get(&compressInfo->compressRingBuffer,
                         &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferLength],
                         n
                        );
          compressInfo->lz4.inputBufferLength += n;
          compressInfo->lz4.totalInputLength  += (uint64)n;
        }
      }

      if ((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) >= 4) // enough compress data available
      {
        // decompress: get compress length of data and flags
        compressLengthFlags = getUINT32(&compressInfo->lz4.inputBuffer[0]);
        if ((compressLengthFlags & LZ4_COMPRESSED_FLAG) == LZ4_COMPRESSED_FLAG)
        {
          // compressed data block
          compressLength = (uint)(compressLengthFlags & LZ4_LENGTH_MASK);
          if ((compressLength <= 0) || ((4+compressLength) >= compressInfo->lz4.inputBufferSize))
          {
            return ERRORX_(INFLATE_FAIL,0,"invalid data size");
          }

          if ((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) >= (4+compressLength))
          {
            // decompress: LZ4 input buffer -> LZ4 output buffer (byte 0..3 are length+flags)
            lz4Result = lz4DecompressBlock(compressInfo,
                                           &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex+4],
                                           compressLength,
                                           compressInfo->lz4.outputBuffer,
                                           compressInfo->lz4.outputBufferSize,
                                           &length
                                          );
            if (lz4Result != LZ4_OK)
            {
              return ERROR_INFLATE_FAIL;
            }
ii++;
fprintf(stderr,"%s, %d: decompress %u: n=%08x compressLength=%d -> %d\n",__FILE__,__LINE__,ii,n,compressLength,length);

            // shift LZ4 input buffer
            memmove(compressInfo->lz4.inputBuffer,
                    &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex+4+compressLength],
                    compressInfo->lz4.inputBufferLength-(compressInfo->lz4.inputBufferIndex+4+compressLength)
                   );
            compressInfo->lz4.inputBufferLength -= compressInfo->lz4.inputBufferIndex+4+compressLength;
            compressInfo->lz4.inputBufferIndex  =  0;

            // init LZ4 output buffer
            compressInfo->lz4.outputBufferIndex  = 0;
            compressInfo->lz4.outputBufferLength = length;
          }
        }
        else
        {
          // not compressed data block
          length = (uint)(compressLengthFlags & LZ4_LENGTH_MASK);
          if ((length <= 0) || ((4+length) > compressInfo->lz4.inputBufferSize))
          {
            return ERRORX_(INFLATE_FAIL,0,"invalid data size");
          }

          if ((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) >= (4+length))
          {
ii++;
fprintf(stderr,"%s, %d: decompress %u: n=%08x uncompressed length=%d\n",__FILE__,__LINE__,ii,n,length);
            // transfer: LZ4 input buffer -> LZ4 output buffer (byte 0..3 are length+flags)
            memcpy(compressInfo->lz4.outputBuffer,
                   &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex+4],
                   length
                  );

            // shift LZ4 input buffer
            memmove(compressInfo->lz4.inputBuffer,
                    &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex+4+length],
                    compressInfo->lz4.inputBufferLength-(compressInfo->lz4.inputBufferIndex+4+length)
                   );
            compressInfo->lz4.inputBufferLength -= compressInfo->lz4.inputBufferIndex+4+length;
            compressInfo->lz4.inputBufferIndex  =  0;

            // init LZ4 output buffer
            compressInfo->lz4.outputBufferIndex  = 0;
            compressInfo->lz4.outputBufferLength = length;
          }
        }

        // get max. number of bytes free in data buffer
        maxDataBytes = RingBuffer_getFree(&compressInfo->dataRingBuffer);

        // move LZ4 output buffer -> data buffer
        n = MIN(compressInfo->lz4.outputBufferLength-compressInfo->lz4.outputBufferIndex,maxDataBytes);
        if (n > 0)
        {
          RingBuffer_put(&compressInfo->dataRingBuffer,
                         &compressInfo->lz4.outputBuffer[compressInfo->lz4.outputBufferIndex],
                         n
                        );
          compressInfo->lz4.outputBufferIndex += n;
          compressInfo->lz4.totalOutputLength += (uint64)n;
        }

        // update compress state
        compressInfo->compressState = COMPRESS_STATE_RUNNING;
      }
    }

    if (RingBuffer_isEmpty(&compressInfo->dataRingBuffer))                    // no data in data buffer
    {
      assert(compressInfo->lz4.outputBufferIndex >= compressInfo->lz4.outputBufferLength);

      // finish decompress, flush internal decompress buffers
      if (   compressInfo->flushFlag                                          // flush data requested
          && (compressInfo->compressState == COMPRESS_STATE_RUNNING)          // compressor is running -> data available in internal buffers
         )
      {
fprintf(stderr,"%s, %d: decompress: finish\n",__FILE__,__LINE__);
        // get max. number of compressed bytes
        maxCompressBytes = RingBuffer_getAvailable(&compressInfo->compressRingBuffer);

        // move compress buffer -> LZ4 input buffer
        n = MIN(compressInfo->lz4.inputBufferSize-compressInfo->lz4.inputBufferLength,maxCompressBytes);
        if (n > 0)
        {
          RingBuffer_get(&compressInfo->compressRingBuffer,
                         &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferLength],
                         n
                        );
          compressInfo->lz4.inputBufferLength += n;
          compressInfo->lz4.totalInputLength  += (uint64)n;
        }

        if ((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) >= 4)
        {
          // decompress: get compress length of data and flags
          compressLengthFlags = getUINT32(&compressInfo->lz4.inputBuffer[0]);
          if ((compressLengthFlags & LZ4_COMPRESSED_FLAG) == LZ4_COMPRESSED_FLAG)
          {
            // compressed data block
            compressLength = (uint)(compressLengthFlags & LZ4_LENGTH_MASK);
            if ((compressLength <= 0) || ((4+compressLength) >= compressInfo->lz4.inputBufferSize))
            {
              return ERRORX_(INFLATE_FAIL,0,"invalid data size");
            }

            if ((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) >= (4+compressLength))
            {
              // decompress: LZ4 input buffer -> LZ4 output buffer (byte 0..3 are length+flags)
              lz4Result = lz4DecompressBlock(compressInfo,
                                             &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex+4],
                                             compressLength,
                                             compressInfo->lz4.outputBuffer,
                                             compressInfo->lz4.outputBufferSize,
                                             &length
                                            );
              if (lz4Result != LZ4_OK)
              {
                return ERROR_DEFLATE_FAIL;
              }
ii++;
fprintf(stderr,"%s, %d: decompress %u: n=%08x compressLength=%d -> %d\n",__FILE__,__LINE__,ii,n,compressLength,length);

              // shift LZ4 input buffer
              memmove(compressInfo->lz4.inputBuffer,
                      &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex+4+compressLength],
                      compressInfo->lz4.inputBufferLength-(compressInfo->lz4.inputBufferIndex+4+compressLength)
                     );
              compressInfo->lz4.inputBufferLength -= compressInfo->lz4.inputBufferIndex+4+compressLength;
              compressInfo->lz4.inputBufferIndex  =  0;

              // init LZ4 output buffer
              compressInfo->lz4.outputBufferIndex  = 0;
              compressInfo->lz4.outputBufferLength = length;
            }
          }
          else
          {
            // not compressed data block
            length = (uint)(compressLengthFlags & LZ4_LENGTH_MASK);
            if ((length <= 0) || ((4+length) > compressInfo->lz4.inputBufferSize))
            {
              return ERRORX_(INFLATE_FAIL,0,"invalid data size");
            }

            if ((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) >= (4+length))
            {
              // transfer: LZ4 input buffer -> LZ4 output buffer (byte 0..3 are length+flags)
              memcpy(compressInfo->lz4.outputBuffer,
                     &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex+4],
                     length
                    );
ii++;
fprintf(stderr,"%s, %d: decompress %u: final uncompressed length=%d\n",__FILE__,__LINE__,ii,length);

              // shift LZ4 input buffer
              memmove(compressInfo->lz4.inputBuffer,
                      &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex+4+length],
                      compressInfo->lz4.inputBufferLength-(compressInfo->lz4.inputBufferIndex+4+length)
                     );
              compressInfo->lz4.inputBufferLength -= compressInfo->lz4.inputBufferIndex+4+length;
              compressInfo->lz4.inputBufferIndex  =  0;

              // init LZ4 output buffer
              compressInfo->lz4.outputBufferIndex  = 0;
              compressInfo->lz4.outputBufferLength = length;
            }
          }
          if ((n & LZ4_END_OF_DATA_FLAG) == LZ4_END_OF_DATA_FLAG)
          {
            compressInfo->endOfDataFlag = TRUE;
          }

          // get max. number of bytes free in data buffer
          maxDataBytes = RingBuffer_getFree(&compressInfo->dataRingBuffer);

          // move LZ4 output buffer -> data buffer
          n = MIN(compressInfo->lz4.outputBufferLength-compressInfo->lz4.outputBufferIndex,maxDataBytes);
          if (n > 0)
          {
            RingBuffer_put(&compressInfo->dataRingBuffer,
                           &compressInfo->lz4.outputBuffer[compressInfo->lz4.outputBufferIndex],
                           n
                          );
            compressInfo->lz4.outputBufferIndex += n;
            compressInfo->lz4.totalOutputLength += (uint64)n;
          }

          // update compress state
          compressInfo->compressState = COMPRESS_STATE_RUNNING;
        }
      }
    }
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

LOCAL Errors CompressLZ4_init(CompressInfo       *compressInfo,
                              CompressModes      compressMode,
                              CompressAlgorithms compressAlgorithm
                             )
{
  assert(compressInfo != NULL);

  compressInfo->lz4.compressMode       = compressMode;
  compressInfo->lz4.inputBufferIndex   = 0;
  compressInfo->lz4.inputBufferLength  = 0;
  compressInfo->lz4.inputBufferSize    = LZ4_compressBound(LZ4_BLOCK_SIZE)+4;
  compressInfo->lz4.outputBufferIndex  = 0;
  compressInfo->lz4.outputBufferLength = 0;
  compressInfo->lz4.outputBufferSize   = LZ4_compressBound(LZ4_BLOCK_SIZE)+4;
  compressInfo->lz4.totalInputLength   = 0LL;
  compressInfo->lz4.totalOutputLength  = 0LL;

  switch (compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_LZ4_0 : compressInfo->lz4.compressionLevel =  0; break;
    case COMPRESS_ALGORITHM_LZ4_1 : compressInfo->lz4.compressionLevel =  1; break;
    case COMPRESS_ALGORITHM_LZ4_2 : compressInfo->lz4.compressionLevel =  2; break;
    case COMPRESS_ALGORITHM_LZ4_3 : compressInfo->lz4.compressionLevel =  3; break;
    case COMPRESS_ALGORITHM_LZ4_4 : compressInfo->lz4.compressionLevel =  4; break;
    case COMPRESS_ALGORITHM_LZ4_5 : compressInfo->lz4.compressionLevel =  5; break;
    case COMPRESS_ALGORITHM_LZ4_6 : compressInfo->lz4.compressionLevel =  6; break;
    case COMPRESS_ALGORITHM_LZ4_7 : compressInfo->lz4.compressionLevel =  7; break;
    case COMPRESS_ALGORITHM_LZ4_8 : compressInfo->lz4.compressionLevel =  8; break;
    case COMPRESS_ALGORITHM_LZ4_9 : compressInfo->lz4.compressionLevel =  9; break;
    case COMPRESS_ALGORITHM_LZ4_10: compressInfo->lz4.compressionLevel = 10; break;
    case COMPRESS_ALGORITHM_LZ4_11: compressInfo->lz4.compressionLevel = 11; break;
    case COMPRESS_ALGORITHM_LZ4_12: compressInfo->lz4.compressionLevel = 12; break;
    case COMPRESS_ALGORITHM_LZ4_13: compressInfo->lz4.compressionLevel = 13; break;
    case COMPRESS_ALGORITHM_LZ4_14: compressInfo->lz4.compressionLevel = 14; break;
    case COMPRESS_ALGORITHM_LZ4_15: compressInfo->lz4.compressionLevel = 15; break;
    case COMPRESS_ALGORITHM_LZ4_16: compressInfo->lz4.compressionLevel = 16; break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  compressInfo->lz4.inputBuffer = (byte*)malloc(compressInfo->lz4.inputBufferSize);
  if (compressInfo->lz4.inputBuffer == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }
  compressInfo->lz4.outputBuffer = (byte*)malloc(compressInfo->lz4.outputBufferSize);
  if (compressInfo->lz4.outputBuffer == NULL)
  {
    free(compressInfo->lz4.inputBuffer);
    return ERROR_INSUFFICIENT_MEMORY;
  }
  #ifdef LZ4_STREAM
    compressInfo->lz4.dictionaryBuffer = (byte*)malloc(LZ4_DICTIONARY_BUFFER_SIZE);
    if (compressInfo->lz4.dictionaryBuffer == NULL)
    {
      free(compressInfo->lz4.outputBuffer);
      free(compressInfo->lz4.inputBuffer);
      return ERROR_INSUFFICIENT_MEMORY;
    }
  #endif /* LZ4_STREAM */
  switch (compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      #ifdef LZ4_STREAM
        compressInfo->lz4.stream.compress = lz4InitStream(compressInfo->lz4.compressionLevel);
        if (compressInfo->lz4.stream.compress == NULL)
        {
          free(compressInfo->lz4.dictionaryBuffer);
          free(compressInfo->lz4.outputBuffer);
          free(compressInfo->lz4.inputBuffer);
          return ERROR_INIT_COMPRESS;
        }
      #endif /* LZ4_STREAM */
      break;
    case COMPRESS_MODE_INFLATE:
      #ifdef LZ4_STREAM
        compressInfo->lz4.stream.decompress = LZ4_createStreamDecode();
        if (compressInfo->lz4.stream.decompress == NULL)
        {
          free(compressInfo->lz4.dictionaryBuffer);
          free(compressInfo->lz4.outputBuffer);
          free(compressInfo->lz4.inputBuffer);
          return ERROR_INIT_COMPRESS;
        }
      #endif /* LZ4_STREAM */
      break;
  }

  return ERROR_NONE;
}

LOCAL void CompressLZ4_done(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  switch (compressInfo->lz4.compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      #ifdef LZ4_STREAM
        lz4DoneStream(compressInfo->lz4.stream.compress,compressInfo->lz4.compressionLevel);
      #endif /* LZ4_STREAM */
      break;
    case COMPRESS_MODE_INFLATE:
      #ifdef LZ4_STREAM
        LZ4_freeStreamDecode(compressInfo->lz4.stream.decompress);
      #endif /* LZ4_STREAM */
      break;
  }
  free(compressInfo->lz4.outputBuffer);
  free(compressInfo->lz4.inputBuffer);
  #ifdef LZ4_STREAM
    free(compressInfo->lz4.dictionaryBuffer);
  #endif /* LZ4_STREAM */
}

LOCAL Errors CompressLZ4_reset(CompressInfo *compressInfo)
{

  assert(compressInfo != NULL);

  UNUSED_VARIABLE(compressInfo);

  return ERROR_NONE;
}

LOCAL uint64 CompressLZ4_getInputLength(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return compressInfo->lz4.totalInputLength;
}

LOCAL uint64 CompressLZ4_getOutputLength(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return compressInfo->lz4.totalOutputLength;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
