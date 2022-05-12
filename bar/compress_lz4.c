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

#define _LZ4_DEBUG
#define _LZ4_DEBUG_DISABLE_COMPRESS

/***************************** Constants *******************************/

#define LZ4_BLOCK_SIZE             (64*KB)
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

#ifdef LZ4_DEBUG
LOCAL uint lz4BlockCount = 0;
#endif /* LZ4_DEBUG */

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
    UNUSED_VARIABLE(compressInfo);

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

//uint ii=0;
LOCAL Errors CompressLZ4_compressData(CompressInfo *compressInfo)
{
  ulong  maxCompressBytes,maxDataBytes;
  uint   n;
  int    lz4Result;
  uint   compressLength;
  uint   length;
  uint32 compressLengthFlags;
  uint32 lengthFlags;

  assert(compressInfo != NULL);
  assert(compressInfo->lz4.inputBuffer != NULL);
  assert(compressInfo->lz4.outputBuffer != NULL);

  if (!RingBuffer_isFull(&compressInfo->compressRingBuffer))                  // space in compress buffer
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
      #ifdef LZ4_DEBUG
        fprintf(stderr,"%s, %d: compress: move output=%u\n",__FILE__,__LINE__,n);
      #endif /* LZ4_DEBUG */
    }
  }

  if (!RingBuffer_isFull(&compressInfo->compressRingBuffer))                // space in compress buffer
  {
    // assert output buffer must be empty
    assert(compressInfo->lz4.outputBufferIndex >= compressInfo->lz4.outputBufferLength);

    // compress available data
    if (!RingBuffer_isEmpty(&compressInfo->dataRingBuffer))                 // unprocessed data available
    {
      // get max. number of data bytes
      maxDataBytes = RingBuffer_getAvailable(&compressInfo->dataRingBuffer);

      // move data buffer -> LZ4 input buffer
      n = MIN(MIN(compressInfo->lz4.inputBufferSize-compressInfo->lz4.inputBufferLength,LZ4_BLOCK_SIZE),maxDataBytes);
      if (n > 0)
      {
        RingBuffer_get(&compressInfo->dataRingBuffer,
                       &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferLength],
                       n
                      );
        compressInfo->lz4.inputBufferLength += n;
        compressInfo->lz4.totalInputLength  += (uint64)n;
        #ifdef LZ4_DEBUG
          fprintf(stderr,"%s, %d: compress: move input=%u\n",__FILE__,__LINE__,n);
        #endif /* LZ4_DEBUG */
      }
      assert((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) <= compressInfo->lz4.inputBufferSize);

      if ((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) >= LZ4_BLOCK_SIZE)
      {
//        assert((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) == LZ4_BLOCK_SIZE);

        // compress: LZ4 input buffer -> LZ4 output buffer (spare 4 bytes for length+flags in output buffer)
        #ifndef LZ4_DEBUG_DISABLE_COMPRESS
          lz4Result = lz4CompressBlock(compressInfo,
                                       &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex],
                                       compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex,
                                       &compressInfo->lz4.outputBuffer[4],
                                       compressInfo->lz4.outputBufferSize-4,
                                       &compressLength
                                      );
        #else /* LZ4_DEBUG_DISABLE_COMPRESS */
          compressLength = 0;
          lz4Result      = LZ4_FAIL;
        #endif /* not LZ4_DEBUG_DISABLE_COMPRESS */
        if (lz4Result == LZ4_OK)
        {
          // store compressed data
          assert(compressLength <= compressInfo->lz4.outputBufferSize-4);

          // put length of compress data+flags into output buffer before data
          compressLengthFlags = ((uint32)compressLength & LZ4_LENGTH_MASK) | LZ4_COMPRESSED_FLAG;
          #ifdef LZ4_STREAM
            compressLengthFlags |= LZ4_STREAM_FLAG;
          #endif /* LZ4_STREAM */
          putUINT32(&compressInfo->lz4.outputBuffer[0],compressLengthFlags);
          #ifdef LZ4_DEBUG
            fprintf(stderr,"%s, %d: compress: length=%u -> compressLength=%u, compressLengthFlags=%08x\n",__FILE__,__LINE__,compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex,compressLength,compressLengthFlags);
          #endif /* LZ4_DEBUG */

          // reset LZ4 input bfufer
          compressInfo->lz4.inputBufferIndex  = 0;
          compressInfo->lz4.inputBufferLength = 0;

          // init LZ4 output buffer
          compressInfo->lz4.outputBufferIndex  = 0;
          compressInfo->lz4.outputBufferLength = 4+compressLength;
        }
        else
        {
          // cannot compress => store original data
          length = MIN(compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex,compressInfo->lz4.outputBufferSize-4);

          // move input buffer -> compress buffer (spare 4 bytes for length+flags in output buffer)
          memcpy(&compressInfo->lz4.outputBuffer[4],
                 &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex],
                 length
                );

          // put length of data+flags into output buffer before data
          lengthFlags = (uint32)length & LZ4_LENGTH_MASK;
          putUINT32(&compressInfo->lz4.outputBuffer[0],lengthFlags);
          #ifdef LZ4_DEBUG
            fprintf(stderr,"%s, %d: compress: lengthFlags=%08x length=%d\n",__FILE__,__LINE__,lengthFlags,compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex);
          #endif /* LZ4_DEBUG */

          // shift LZ4 input buffer
          memmove(compressInfo->lz4.inputBuffer,
                  &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex+length],
                  compressInfo->lz4.inputBufferSize-length
                 );
          compressInfo->lz4.inputBufferLength -= length;
          compressInfo->lz4.inputBufferIndex  =  0;

          // init LZ4 output buffer
          compressInfo->lz4.outputBufferIndex  = 0;
          compressInfo->lz4.outputBufferLength = 4+length;
        }
        assert((compressInfo->lz4.outputBufferLength-compressInfo->lz4.outputBufferIndex) <= compressInfo->lz4.outputBufferSize);

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
          #ifdef LZ4_DEBUG
            fprintf(stderr,"%s, %d: compress: move output=%u\n",__FILE__,__LINE__,n);
          #endif /* LZ4_DEBUG */
        }
      }

      // update compress state
      compressInfo->compressState = COMPRESS_STATE_RUNNING;
    }
  }

  if (!RingBuffer_isFull(&compressInfo->compressRingBuffer))                // space in compress buffer
  {
    // assert output buffer must be empty
    assert(compressInfo->lz4.outputBufferIndex >= compressInfo->lz4.outputBufferLength);

    // finish compress, flush internal compress buffers
    if (   compressInfo->flushFlag                                          // flush data requested
        && (compressInfo->compressState == COMPRESS_STATE_RUNNING)          // compressor is running -> data available in internal buffers
       )
    {
      assert(RingBuffer_isEmpty(&compressInfo->dataRingBuffer));

      if ((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) > 0)
      {
        assert((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) <= LZ4_BLOCK_SIZE);

        // compress: LZ4 input buffer -> LZ4 output buffer (spare 4 bytes for length+flags in output buffer)
        #ifndef LZ4_DEBUG_DISABLE_COMPRESS
          lz4Result = lz4CompressBlock(compressInfo,
                                       &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex],
                                       compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex,
                                       &compressInfo->lz4.outputBuffer[4],
                                       compressInfo->lz4.outputBufferSize-4,
                                       &compressLength
                                     );
        #else /* LZ4_DEBUG_DISABLE_COMPRESS */
          compressLength = 0;
          lz4Result      = LZ4_FAIL;
        #endif /* not LZ4_DEBUG_DISABLE_COMPRESS */
        if (lz4Result == LZ4_OK)
        {
          // store compressed data
          assert(compressLength <= compressInfo->lz4.outputBufferSize-4);

          // put length of compress data+flags into output buffer before compressed data
          compressLengthFlags = ((uint32)compressLength & LZ4_LENGTH_MASK) | LZ4_COMPRESSED_FLAG | LZ4_END_OF_DATA_FLAG;
          #ifdef LZ4_STREAM
            compressLengthFlags |= LZ4_STREAM_FLAG;
          #endif /* LZ4_STREAM */
          putUINT32(&compressInfo->lz4.outputBuffer[0],compressLengthFlags);
          #ifdef LZ4_DEBUG
            fprintf(stderr,"%s, %d: compress: final length=%u -> compressLength=%u, compressLengthFlags=%08x\n",__FILE__,__LINE__,compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex,compressLength,compressLengthFlags);
          #endif /* LZ4_DEBUG */

          // reset LZ4 input buffer
          compressInfo->lz4.inputBufferIndex  = 0;
          compressInfo->lz4.inputBufferLength = 0;

          // init LZ4 output buffer
          compressInfo->lz4.outputBufferIndex  = 0;
          compressInfo->lz4.outputBufferLength = 4+compressLength;
        }
        else
        {
          // cannot compress => store original data
          length = MIN(compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex,compressInfo->lz4.outputBufferSize-4);

          // transfer: input buffer -> compress buffer
          memcpy(&compressInfo->lz4.outputBuffer[4],
                 &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex],
                 length
                );

          // put length of data+end-of-data flag into output buffer before data
          lengthFlags = ((uint32)length & LZ4_LENGTH_MASK) | LZ4_END_OF_DATA_FLAG;
          putUINT32(&compressInfo->lz4.outputBuffer[0],lengthFlags);
          #ifdef LZ4_DEBUG
            fprintf(stderr,"%s, %d: compress: final length=%u, lengthFlags=%08x\n",__FILE__,__LINE__,compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex,lengthFlags);
          #endif /* LZ4_DEBUG */

          // shift LZ4 input buffer
          memmove(compressInfo->lz4.inputBuffer,
                  &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex+length],
                  compressInfo->lz4.inputBufferSize-length
                 );
          compressInfo->lz4.inputBufferLength -= length;
          compressInfo->lz4.inputBufferIndex  =  0;

          // init LZ4 output buffer
          compressInfo->lz4.outputBufferIndex  = 0;
          compressInfo->lz4.outputBufferLength = 4+length;
        }
        assert((compressInfo->lz4.outputBufferLength-compressInfo->lz4.outputBufferIndex) <= compressInfo->lz4.outputBufferSize);

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
          #ifdef LZ4_DEBUG
            fprintf(stderr,"%s, %d: compress: move output=%u\n",__FILE__,__LINE__,n);
          #endif /* LZ4_DEBUG */
        }
      }
      else
      {
        #ifdef LZ4_DEBUG
          fprintf(stderr,"%s, %d: compress: end of data\n",__FILE__,__LINE__);
        #endif /* LZ4_DEBUG */
        compressInfo->endOfDataFlag = TRUE;
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

  if (!RingBuffer_isFull(&compressInfo->dataRingBuffer))                      // space in compress buffer
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
      #ifdef LZ4_DEBUG
        fprintf(stderr,"%s, %d: decompress: move input=%u\n",__FILE__,__LINE__,n);
      #endif /* LZ4_DEBUG */
    }
  }

  if (!RingBuffer_isFull(&compressInfo->dataRingBuffer))                      // space in data buffer
  {
    if (!compressInfo->lz4.lastBlockFlag)                                     // not end-of-data
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
          RingBuffer_get(&compressInfo->compressRingBuffer,
                         &compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferLength],
                         n
                        );
          compressInfo->lz4.inputBufferLength += n;
          compressInfo->lz4.totalInputLength  += (uint64)n;
          #ifdef LZ4_DEBUG
            fprintf(stderr,"%s, %d: decompress: length=%u\n",__FILE__,__LINE__,n);
          #endif /* LZ4_DEBUG */
        }
        assert((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) <= compressInfo->lz4.inputBufferSize);
      }

      if ((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) >= 4)
      {
        // decompress: get compress length of data and flags
        compressLengthFlags = getUINT32(&compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex]);
        #ifdef LZ4_DEBUG
          fprintf(stderr,"%s, %d: compressLengthFlags=%08x\n",__FILE__,__LINE__,compressLengthFlags);
        #endif /* LZ4_DEBUG */
        if ((compressLengthFlags & LZ4_COMPRESSED_FLAG) == LZ4_COMPRESSED_FLAG)
        {
          // compressed data block -> decompress
          compressLength = (uint)(compressLengthFlags & LZ4_LENGTH_MASK);
          #ifdef LZ4_DEBUG
            fprintf(stderr,"%s, %d: compressLengthFlags=%08x\n",__FILE__,__LINE__,compressLengthFlags);
          #endif /* LZ4_DEBUG */
          if ((compressLength <= 0) || ((4+compressLength) >= compressInfo->lz4.inputBufferSize))
          {
            return ERRORX_(INFLATE,0,"invalid data size");
          }

          if ((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) >= (4+compressLength)) // enough compress data available
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
              return ERROR_(INFLATE,lz4Result);
            }
            assert(length <= compressInfo->lz4.outputBufferSize);

            #ifdef LZ4_DEBUG
              fprintf(stderr,"%s, %d: decompress: compressLengthFlags=%08x compressLength=%u -> length=%u\n",__FILE__,__LINE__,compressLengthFlags,compressLength,length);
            #endif /* LZ4_DEBUG */

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

            // check if last block
            compressInfo->lz4.lastBlockFlag = ((compressLengthFlags & LZ4_END_OF_DATA_FLAG) == LZ4_END_OF_DATA_FLAG);
          }
        }
        else
        {
          // not compressed data block -> copy
          length = (uint)(compressLengthFlags & LZ4_LENGTH_MASK);
          if ((length <= 0) || ((4+length) > compressInfo->lz4.inputBufferSize))
          {
            return ERRORX_(INFLATE,0,"invalid data size");
          }

          if ((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) >= (4+length)) // enough data available
          {
            #ifdef LZ4_DEBUG
              fprintf(stderr,"%s, %d: decompress: compressLengthFlags=%08x decompressed length=%u\n",__FILE__,__LINE__,compressLengthFlags,length);
            #endif /* LZ4_DEBUG */

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

            // check if last block
            compressInfo->lz4.lastBlockFlag = ((compressLengthFlags & LZ4_END_OF_DATA_FLAG) == LZ4_END_OF_DATA_FLAG);
          }
        }
        assert((compressInfo->lz4.outputBufferLength-compressInfo->lz4.outputBufferIndex) <= compressInfo->lz4.outputBufferSize);

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
          #ifdef LZ4_DEBUG
            fprintf(stderr,"%s, %d: compress: move output=%u\n",__FILE__,__LINE__,n);
          #endif /* LZ4_DEBUG */
        }
        assert((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) <= compressInfo->lz4.inputBufferSize);

        // update compress state
        compressInfo->compressState = COMPRESS_STATE_RUNNING;
      }
    }

//    if (RingBuffer_isEmpty(&compressInfo->dataRingBuffer))                    // no data in data buffer
    if (!RingBuffer_isFull(&compressInfo->dataRingBuffer))                    // space in data buffer
    {
      assert(compressInfo->lz4.outputBufferIndex >= compressInfo->lz4.outputBufferLength);

      // finish decompress, flush internal decompress buffers
      if (   compressInfo->flushFlag                                          // flush data requested
          && (compressInfo->compressState == COMPRESS_STATE_RUNNING)          // compressor is running -> data available in internal buffers
         )
      {
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
          #ifdef LZ4_DEBUG
            fprintf(stderr,"%s, %d: compress: length=%u\n",__FILE__,__LINE__,n);
          #endif /* LZ4_DEBUG */
        }
        assert((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) <= compressInfo->lz4.inputBufferSize);

        if (!compressInfo->lz4.lastBlockFlag)                                     // not end-of-data
        {
          if ((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) >= 4)
          {
            // decompress: get length of compressed data and flags from LZ4 input buffer
            compressLengthFlags = getUINT32(&compressInfo->lz4.inputBuffer[compressInfo->lz4.inputBufferIndex]);
            if ((compressLengthFlags & LZ4_COMPRESSED_FLAG) == LZ4_COMPRESSED_FLAG)
            {
              // compressed data block -> decompress
              compressLength = (uint)(compressLengthFlags & LZ4_LENGTH_MASK);
              if ((compressLength <= 0) || ((4+compressLength) >= compressInfo->lz4.inputBufferSize))
              {
                return ERRORX_(INFLATE,0,"invalid data size");
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
                  return ERROR_DEFLATE;
                }
                assert(length <= compressInfo->lz4.outputBufferSize);

                #ifdef LZ4_DEBUG
                  fprintf(stderr,"%s, %d: decompress: compressLengthFlags=%08x compressLength=%u -> %u\n",__FILE__,__LINE__,compressLengthFlags,compressLength);
                #endif /* LZ4_DEBUG */

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

                // check if last block
                compressInfo->lz4.lastBlockFlag = ((compressLengthFlags & LZ4_END_OF_DATA_FLAG) == LZ4_END_OF_DATA_FLAG);
              }
            }
            else
            {
              // not compressed data block -> copy
              length = (uint)(compressLengthFlags & LZ4_LENGTH_MASK);
              if ((length <= 0) || ((4+length) > compressInfo->lz4.inputBufferSize))
              {
                return ERRORX_(INFLATE,0,"invalid data size");
              }

              if ((compressInfo->lz4.inputBufferLength-compressInfo->lz4.inputBufferIndex) >= (4+length))
              {
                #ifdef LZ4_DEBUG
                  fprintf(stderr,"%s, %d: decompress: final decompressed length=%u\n",__FILE__,__LINE__,(uint)length);
                #endif /* LZ4_DEBUG */

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

                // check if last block
                compressInfo->lz4.lastBlockFlag = ((compressLengthFlags & LZ4_END_OF_DATA_FLAG) == LZ4_END_OF_DATA_FLAG);
              }
            }
            assert((compressInfo->lz4.outputBufferLength-compressInfo->lz4.outputBufferIndex) <= compressInfo->lz4.outputBufferSize);

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
              #ifdef LZ4_DEBUG
                fprintf(stderr,"%s, %d: decompress: move output=%u\n",__FILE__,__LINE__,n);
              #endif /* LZ4_DEBUG */
            }
          }
        }
        else
        {
          #ifdef LZ4_DEBUG
            fprintf(stderr,"%s, %d: decompress: end of data\n",__FILE__,__LINE__);
          #endif /* LZ4_DEBUG */
          compressInfo->endOfDataFlag = TRUE;
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

  compressInfo->lz4.inputBufferIndex   = 0;
  compressInfo->lz4.inputBufferLength  = 0;
  compressInfo->lz4.inputBufferSize    = 4+LZ4_compressBound(LZ4_BLOCK_SIZE);  // length/flags prefix+max. size of a LZ4 data block
  compressInfo->lz4.outputBufferIndex  = 0;
  compressInfo->lz4.outputBufferLength = 0;
  compressInfo->lz4.outputBufferSize   = 4+LZ4_compressBound(LZ4_BLOCK_SIZE);  // length/flags prefix+max. size of a LZ4 data block
  compressInfo->lz4.lastBlockFlag      = FALSE;
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

  switch (compressInfo->compressMode)
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
