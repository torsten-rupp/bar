/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/compress.c,v $
* $Revision: 1.4 $
* $Author: torsten $
* Contents: Backup ARchiver compress functions
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "lists.h"

#include "bar.h"

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

LOCAL Errors compressData(CompressInfo *compressInfo)
{
  ulong maxCompressBytes,maxDataBytes;
  ulong n;
  int   zlibError;

  assert(compressInfo != NULL);
  assert(compressInfo->dataBuffer != NULL);
  assert(compressInfo->compressBuffer != NULL);
  assert(compressInfo->dataBufferIndex <= compressInfo->dataBufferLength);
  assert(compressInfo->dataBufferLength <= compressInfo->dataBufferSize);
  assert(compressInfo->compressBufferIndex <= compressInfo->compressBufferLength);
  assert(compressInfo->compressBufferLength <= compressInfo->compressBufferSize);

  /* compress if possible */
  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      if (   (compressInfo->compressBufferLength < compressInfo->compressBufferSize)  // space in compress buffer
          && (compressInfo->dataBufferLength > 0)                                     // data available
         )
      {
        /* copy from data buffer -> compress buffer */
        maxDataBytes     = compressInfo->dataBufferLength;
        maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;
        n = MIN(maxDataBytes,maxCompressBytes);
        memcpy(compressInfo->compressBuffer+compressInfo->compressBufferLength,
               compressInfo->dataBuffer+compressInfo->dataBufferIndex,
               n
              );
        compressInfo->compressBufferLength += n;

        /* shift data buffer */
        memmove(compressInfo->dataBuffer,compressInfo->dataBuffer+n,compressInfo->dataBufferLength-n);
        compressInfo->dataBufferLength -= n;
      }
      break;
    case COMPRESS_ALGORITHM_ZIP0:
    case COMPRESS_ALGORITHM_ZIP1:
    case COMPRESS_ALGORITHM_ZIP2:
    case COMPRESS_ALGORITHM_ZIP3:
    case COMPRESS_ALGORITHM_ZIP4:
    case COMPRESS_ALGORITHM_ZIP5:
    case COMPRESS_ALGORITHM_ZIP6:
    case COMPRESS_ALGORITHM_ZIP7:
    case COMPRESS_ALGORITHM_ZIP8:
    case COMPRESS_ALGORITHM_ZIP9:
      if (compressInfo->compressBufferLength < compressInfo->compressBufferSize)  // space in compress buffer
      {
        if      (compressInfo->dataBufferLength > 0)  // data available
        {
          /* compress */
          maxDataBytes     = compressInfo->dataBufferLength;
          maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;
          compressInfo->zlib.stream.next_in   = compressInfo->dataBuffer+compressInfo->dataBufferIndex;
          compressInfo->zlib.stream.avail_in  = maxDataBytes;
          compressInfo->zlib.stream.next_out  = compressInfo->compressBuffer+compressInfo->compressBufferLength;
          compressInfo->zlib.stream.avail_out = maxCompressBytes;
          zlibError = deflate(&compressInfo->zlib.stream,Z_NO_FLUSH);
          if ((zlibError != Z_OK) && (zlibError != Z_BUF_ERROR))
          {
            return ERROR_COMPRESS_ERROR;
          }
          compressInfo->compressBufferLength += maxCompressBytes-compressInfo->zlib.stream.avail_out;

          /* shift data buffer */
          n = maxDataBytes-compressInfo->zlib.stream.avail_in;
          memmove(compressInfo->dataBuffer,compressInfo->dataBuffer+n,n);
          compressInfo->dataBufferLength -= n;
        }
        else if (compressInfo->flushFlag) // flush data
        {
          /* compress with flush */
          maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;
          compressInfo->zlib.stream.next_in   = NULL;
          compressInfo->zlib.stream.avail_in  = 0;
          compressInfo->zlib.stream.next_out  = compressInfo->compressBuffer+compressInfo->compressBufferLength;
          compressInfo->zlib.stream.avail_out = maxCompressBytes;
          zlibError = deflate(&compressInfo->zlib.stream,Z_FINISH);
          if ((zlibError != Z_OK) && (zlibError != Z_STREAM_END) && (zlibError != Z_BUF_ERROR))
          {
            return ERROR_COMPRESS_ERROR;
          }
          compressInfo->compressBufferLength += maxCompressBytes-compressInfo->zlib.stream.avail_out;
        }
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

LOCAL Errors decompressData(CompressInfo *compressInfo)
{
  ulong maxCompressBytes,maxDataBytes;
  ulong n;
  int   zlibError;

  assert(compressInfo != NULL);
  assert(compressInfo->dataBuffer != NULL);
  assert(compressInfo->compressBuffer != NULL);
  assert(compressInfo->dataBufferIndex <= compressInfo->dataBufferLength);
  assert(compressInfo->dataBufferLength <= compressInfo->dataBufferSize);
  assert(compressInfo->compressBufferIndex <= compressInfo->compressBufferLength);
  assert(compressInfo->compressBufferLength <= compressInfo->compressBufferSize);

  /* decompress if possible */
  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      if (   (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)         // no data buffer
          && (compressInfo->compressBufferIndex < compressInfo->compressBufferLength)  // unprocessed compressed data available
         )
      {
        /* copy from compress buffer -> data buffer */
        maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
        maxDataBytes     = compressInfo->dataBufferLength;
        n = MIN(maxCompressBytes,maxDataBytes);
        memcpy(compressInfo->dataBuffer,
               compressInfo->compressBuffer+compressInfo->compressBufferIndex,
               n
              );
        compressInfo->compressBufferLength += n;

        compressInfo->dataBufferIndex  = 0;
        compressInfo->dataBufferLength = n;
      }
      break;
    case COMPRESS_ALGORITHM_ZIP0:
    case COMPRESS_ALGORITHM_ZIP1:
    case COMPRESS_ALGORITHM_ZIP2:
    case COMPRESS_ALGORITHM_ZIP3:
    case COMPRESS_ALGORITHM_ZIP4:
    case COMPRESS_ALGORITHM_ZIP5:
    case COMPRESS_ALGORITHM_ZIP6:
    case COMPRESS_ALGORITHM_ZIP7:
    case COMPRESS_ALGORITHM_ZIP8:
    case COMPRESS_ALGORITHM_ZIP9:
      if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)  // no data in buffer
      {
        compressInfo->dataBufferIndex  = 0;
        compressInfo->dataBufferLength = 0;

        if      (compressInfo->compressBufferIndex < compressInfo->compressBufferLength)  // unprocessed compressed data available
        {
          /* decompress */
          maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
          maxDataBytes     = compressInfo->dataBufferSize-compressInfo->dataBufferLength;
          compressInfo->zlib.stream.next_in   = compressInfo->compressBuffer+compressInfo->compressBufferIndex;
          compressInfo->zlib.stream.avail_in  = maxCompressBytes;
          compressInfo->zlib.stream.next_out  = compressInfo->dataBuffer+compressInfo->dataBufferLength;
          compressInfo->zlib.stream.avail_out = maxDataBytes;
          zlibError = inflate(&compressInfo->zlib.stream,Z_NO_FLUSH);
          if ((zlibError != Z_OK) && (zlibError != Z_STREAM_END) && (zlibError != Z_BUF_ERROR))
          {
            return ERROR_COMPRESS_ERROR;
          }
          compressInfo->compressBufferIndex += maxCompressBytes-compressInfo->zlib.stream.avail_in;
          compressInfo->dataBufferLength += maxDataBytes-compressInfo->zlib.stream.avail_out;
        }
        else if (compressInfo->flushFlag) // flush data
        {
          /* decompress with flush */
          maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
          maxDataBytes     = compressInfo->dataBufferSize-compressInfo->dataBufferLength;
          compressInfo->zlib.stream.next_in   = NULL;
          compressInfo->zlib.stream.avail_in  = 0;
          compressInfo->zlib.stream.next_out  = compressInfo->dataBuffer+compressInfo->dataBufferLength;
          compressInfo->zlib.stream.avail_out = maxDataBytes;
          zlibError = inflate(&compressInfo->zlib.stream,Z_FINISH);
          if ((zlibError != Z_OK) && (zlibError != Z_STREAM_END) && (zlibError != Z_BUF_ERROR))
          {
            return ERROR_COMPRESS_ERROR;
          }
          compressInfo->dataBufferLength += maxDataBytes-compressInfo->zlib.stream.avail_out;
        }
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

/*---------------------------------------------------------------------*/

Errors Compress_init(void)
{
  return ERROR_NONE;
}

void Compress_done(void)
{
}

Errors Compress_new(CompressInfo       *compressInfo,
                    CompressModes      compressMode,
                    CompressAlgorithms compressAlgorithm,
                    ulong              blockLength
                   )
{
  int compressionLevel;

  assert(compressInfo != NULL);
//compressAlgorithm=COMPRESS_ALGORITHM_NONE;

  /* init variables */
  compressInfo->compressMode         = compressMode; 
  compressInfo->compressAlgorithm    = compressAlgorithm; 
  compressInfo->blockLength          = blockLength;
  compressInfo->flushFlag            = FALSE;
  compressInfo->dataBufferIndex      = 0;
  compressInfo->dataBufferLength     = 0;
  compressInfo->dataBufferSize       = blockLength;
  compressInfo->compressBufferIndex  = 0;
  compressInfo->compressBufferLength = 0;
  compressInfo->compressBufferSize   = blockLength;

  /* allocate buffers */
  compressInfo->dataBuffer = malloc(blockLength);
  if (compressInfo->dataBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  compressInfo->compressBuffer = malloc(blockLength);
  if (compressInfo->compressBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  switch (compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      compressInfo->none.length = 0;
      break;
    case COMPRESS_ALGORITHM_ZIP0:
    case COMPRESS_ALGORITHM_ZIP1:
    case COMPRESS_ALGORITHM_ZIP2:
    case COMPRESS_ALGORITHM_ZIP3:
    case COMPRESS_ALGORITHM_ZIP4:
    case COMPRESS_ALGORITHM_ZIP5:
    case COMPRESS_ALGORITHM_ZIP6:
    case COMPRESS_ALGORITHM_ZIP7:
    case COMPRESS_ALGORITHM_ZIP8:
    case COMPRESS_ALGORITHM_ZIP9:
      switch (compressAlgorithm)
      {
        case COMPRESS_ALGORITHM_ZIP0: compressionLevel = 0; break;
        case COMPRESS_ALGORITHM_ZIP1: compressionLevel = 1; break;
        case COMPRESS_ALGORITHM_ZIP2: compressionLevel = 2; break;
        case COMPRESS_ALGORITHM_ZIP3: compressionLevel = 3; break;
        case COMPRESS_ALGORITHM_ZIP4: compressionLevel = 4; break;
        case COMPRESS_ALGORITHM_ZIP5: compressionLevel = 5; break;
        case COMPRESS_ALGORITHM_ZIP6: compressionLevel = 6; break;
        case COMPRESS_ALGORITHM_ZIP7: compressionLevel = 7; break;
        case COMPRESS_ALGORITHM_ZIP8: compressionLevel = 8; break;
        case COMPRESS_ALGORITHM_ZIP9: compressionLevel = 9; break;
        default:
          break;
      }
      compressInfo->zlib.stream.zalloc = Z_NULL;
      compressInfo->zlib.stream.zfree  = Z_NULL;
      compressInfo->zlib.stream.opaque = Z_NULL;
      switch (compressMode)
      {
        case COMPRESS_MODE_DEFLATE:
          if (deflateInit(&compressInfo->zlib.stream,compressionLevel) != Z_OK)
          {
            free(compressInfo->compressBuffer);
            free(compressInfo->dataBuffer);
            return ERROR_INIT_COMPRESS;
          }
          break;
        case COMPRESS_MODE_INFLATE:
          if (inflateInit(&compressInfo->zlib.stream) != Z_OK)
          {
            free(compressInfo->compressBuffer);
            free(compressInfo->dataBuffer);
            return ERROR_INIT_COMPRESS;
          }
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
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

void Compress_delete(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);
  assert(compressInfo->dataBuffer != NULL);
  assert(compressInfo->compressBuffer != NULL);

  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      break;
    case COMPRESS_ALGORITHM_ZIP0:
    case COMPRESS_ALGORITHM_ZIP1:
    case COMPRESS_ALGORITHM_ZIP2:
    case COMPRESS_ALGORITHM_ZIP3:
    case COMPRESS_ALGORITHM_ZIP4:
    case COMPRESS_ALGORITHM_ZIP5:
    case COMPRESS_ALGORITHM_ZIP6:
    case COMPRESS_ALGORITHM_ZIP7:
    case COMPRESS_ALGORITHM_ZIP8:
    case COMPRESS_ALGORITHM_ZIP9:
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
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  free(compressInfo->compressBuffer);
  free(compressInfo->dataBuffer);
}

Errors Compress_reset(CompressInfo *compressInfo)
{
  int zlibError;

  assert(compressInfo != NULL);

  /* init variables */
  compressInfo->flushFlag            = FALSE;
  compressInfo->dataBufferIndex      = 0;
  compressInfo->dataBufferLength     = 0;
  compressInfo->compressBufferIndex  = 0;
  compressInfo->compressBufferLength = 0;

  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      compressInfo->none.length = 0;
      break;
    case COMPRESS_ALGORITHM_ZIP0:
    case COMPRESS_ALGORITHM_ZIP1:
    case COMPRESS_ALGORITHM_ZIP2:
    case COMPRESS_ALGORITHM_ZIP3:
    case COMPRESS_ALGORITHM_ZIP4:
    case COMPRESS_ALGORITHM_ZIP5:
    case COMPRESS_ALGORITHM_ZIP6:
    case COMPRESS_ALGORITHM_ZIP7:
    case COMPRESS_ALGORITHM_ZIP8:
    case COMPRESS_ALGORITHM_ZIP9:
      switch (compressInfo->compressMode)
      {
        case COMPRESS_MODE_DEFLATE:
          zlibError = deflateReset(&compressInfo->zlib.stream);
          break;
        case COMPRESS_MODE_INFLATE:
          zlibError = inflateReset(&compressInfo->zlib.stream);
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
      if ((zlibError != Z_OK) && (zlibError != Z_STREAM_END))
      {
        return ERROR_COMPRESS_ERROR;
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

Errors Compress_deflate(CompressInfo *compressInfo,
                        byte         data
                       )
{
  ulong maxCompressBytes,maxDataBytes;
  ulong n;
  Errors error;

  assert(compressInfo != NULL);
  assert(compressInfo->compressMode == COMPRESS_MODE_DEFLATE);
  assert(compressInfo->dataBuffer != NULL);
  assert(compressInfo->compressBuffer != NULL);
  assert(compressInfo->dataBufferIndex <= compressInfo->dataBufferLength);
  assert(compressInfo->dataBufferLength <= compressInfo->dataBufferSize);
  assert(compressInfo->compressBufferIndex <= compressInfo->compressBufferLength);
  assert(compressInfo->compressBufferLength <= compressInfo->compressBufferSize);

  if (compressInfo->dataBufferLength >= compressInfo->dataBufferSize)  // no more space in data buffer
  {
    error = compressData(compressInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  assert(compressInfo->dataBufferLength < compressInfo->dataBufferSize);
  (*(compressInfo->dataBuffer+compressInfo->dataBufferLength)) = data;
  compressInfo->dataBufferLength++;
#if 0
  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      assert(compressInfo->compressBufferLength < compressInfo->compressBufferSize);
      (*(compressInfo->compressBuffer+compressInfo->compressBufferIndex)) = data;
      compressInfo->compressBufferIndex++;
      compressInfo->compressBufferLength++;

      compressInfo->none.length++;
      break;
    case COMPRESS_ALGORITHM_ZIP0:
    case COMPRESS_ALGORITHM_ZIP1:
    case COMPRESS_ALGORITHM_ZIP2:
    case COMPRESS_ALGORITHM_ZIP3:
    case COMPRESS_ALGORITHM_ZIP4:
    case COMPRESS_ALGORITHM_ZIP5:
    case COMPRESS_ALGORITHM_ZIP6:
    case COMPRESS_ALGORITHM_ZIP7:
    case COMPRESS_ALGORITHM_ZIP8:
    case COMPRESS_ALGORITHM_ZIP9:
      if (compressInfo->dataBufferLength >= compressInfo->dataBufferSize)  // no more space in data buffer
      {
        /* compress */
        maxDataBytes     = compressInfo->dataBufferLength;
        maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;
        compressInfo->zlib.stream.next_in   = compressInfo->dataBuffer;
        compressInfo->zlib.stream.avail_in  = maxDataBytes;
        compressInfo->zlib.stream.next_out  = compressInfo->compressBuffer+compressInfo->compressBufferLength;
        compressInfo->zlib.stream.avail_out = maxCompressBytes;
        if (deflate(&compressInfo->zlib.stream,Z_NO_FLUSH) != Z_OK)
        {
          return ERROR_COMPRESS_ERROR;
        }
        compressInfo->compressBufferLength += maxCompressBytes-compressInfo->zlib.stream.avail_out;

        /* shift data buffer */
        n = maxDataBytes-compressInfo->zlib.stream.avail_in;
        memmove(compressInfo->dataBuffer,compressInfo->dataBuffer+n,n);
        compressInfo->dataBufferLength -= n;
      }

      /* store in data buffer */
      assert(compressInfo->dataBufferLength < compressInfo->compressBufferSize);
      (*(compressInfo->dataBuffer+compressInfo->dataBufferLength)) = data;
      compressInfo->dataBufferLength++;

      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
#endif /* 0 */

  return ERROR_NONE;
}

Errors Compress_inflate(CompressInfo *compressInfo,
                        byte         *data
                       )
{
  ulong maxCompressBytes,maxDataBytes;
  int  zlibError;
  Errors error;

  assert(compressInfo != NULL);
  assert(compressInfo->compressMode == COMPRESS_MODE_INFLATE);
  assert(compressInfo->dataBuffer != NULL);
  assert(compressInfo->compressBuffer != NULL);
  assert(compressInfo->dataBufferIndex <= compressInfo->dataBufferLength);
  assert(compressInfo->dataBufferLength <= compressInfo->dataBufferSize);
  assert(compressInfo->compressBufferIndex <= compressInfo->compressBufferLength);
  assert(compressInfo->compressBufferLength <= compressInfo->compressBufferSize);
  assert(data != NULL);

  if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)
  {
    error = decompressData(compressInfo);
    if (error != ERROR_NONE)
    {
      return;
    }
  }
  assert(compressInfo->dataBufferIndex < compressInfo->dataBufferLength);
  (*data) = (*(compressInfo->dataBuffer+compressInfo->dataBufferIndex));
  compressInfo->dataBufferIndex++;

#if 0
  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      assert(compressInfo->compressBufferIndex < compressInfo->compressBufferLength);
      (*data) = (*(compressInfo->compressBuffer+compressInfo->compressBufferIndex));
      compressInfo->compressBufferIndex++;

      compressInfo->none.length++;
      break;
    case COMPRESS_ALGORITHM_ZIP0:
    case COMPRESS_ALGORITHM_ZIP1:
    case COMPRESS_ALGORITHM_ZIP2:
    case COMPRESS_ALGORITHM_ZIP3:
    case COMPRESS_ALGORITHM_ZIP4:
    case COMPRESS_ALGORITHM_ZIP5:
    case COMPRESS_ALGORITHM_ZIP6:
    case COMPRESS_ALGORITHM_ZIP7:
    case COMPRESS_ALGORITHM_ZIP8:
    case COMPRESS_ALGORITHM_ZIP9:
      if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)  // no more data
      {
        compressInfo->dataBufferIndex  = 0;
        compressInfo->dataBufferLength = 0;

        /* decompress */
        maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
        maxDataBytes     = compressInfo->dataBufferSize-compressInfo->dataBufferLength;
        compressInfo->zlib.stream.next_in   = compressInfo->compressBuffer+compressInfo->compressBufferIndex;
        compressInfo->zlib.stream.avail_in  = maxCompressBytes;
        compressInfo->zlib.stream.next_out  = compressInfo->dataBuffer+compressInfo->dataBufferLength;
        compressInfo->zlib.stream.avail_out = maxDataBytes;
        zlibError = inflate(&compressInfo->zlib.stream,Z_NO_FLUSH);
        if ((zlibError != Z_OK) && (zlibError != Z_BUF_ERROR))
        {
          return ERROR_COMPRESS_ERROR;
        }
        compressInfo->compressBufferIndex += maxCompressBytes-compressInfo->zlib.stream.avail_in;
        compressInfo->dataBufferLength += maxDataBytes-compressInfo->zlib.stream.avail_out;
      }

      /* get from data buffer */
      assert(compressInfo->dataBufferIndex < compressInfo->dataBufferLength);
      (*data) = (*(compressInfo->dataBuffer+compressInfo->dataBufferIndex));
      compressInfo->dataBufferIndex++;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
#endif /* 0 */

  return ERROR_NONE;
}

Errors Compress_flush(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  compressInfo->flushFlag = TRUE;

  return ERROR_NONE;
}

uint64 Compress_getInputLength(CompressInfo *compressInfo)
{
  uint64 length;

  assert(compressInfo != NULL);

  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      length = compressInfo->none.length;
      break;
    case COMPRESS_ALGORITHM_ZIP0:
    case COMPRESS_ALGORITHM_ZIP1:
    case COMPRESS_ALGORITHM_ZIP2:
    case COMPRESS_ALGORITHM_ZIP3:
    case COMPRESS_ALGORITHM_ZIP4:
    case COMPRESS_ALGORITHM_ZIP5:
    case COMPRESS_ALGORITHM_ZIP6:
    case COMPRESS_ALGORITHM_ZIP7:
    case COMPRESS_ALGORITHM_ZIP8:
    case COMPRESS_ALGORITHM_ZIP9:
      length = (uint64)compressInfo->zlib.stream.total_in;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return length;
}

uint64 Compress_getOutputLength(CompressInfo *compressInfo)
{
  uint64 length;

  assert(compressInfo != NULL);

  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      length = compressInfo->none.length;
      break;
    case COMPRESS_ALGORITHM_ZIP0:
    case COMPRESS_ALGORITHM_ZIP1:
    case COMPRESS_ALGORITHM_ZIP2:
    case COMPRESS_ALGORITHM_ZIP3:
    case COMPRESS_ALGORITHM_ZIP4:
    case COMPRESS_ALGORITHM_ZIP5:
    case COMPRESS_ALGORITHM_ZIP6:
    case COMPRESS_ALGORITHM_ZIP7:
    case COMPRESS_ALGORITHM_ZIP8:
    case COMPRESS_ALGORITHM_ZIP9:
      length = (uint64)compressInfo->zlib.stream.total_out;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return length;
}

Errors Compress_available(CompressInfo *compressInfo, ulong *availableBytes)
{
  Errors error;

  assert(compressInfo != NULL);

  switch (compressInfo->compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      error = compressData(compressInfo);
      if (error != ERROR_NONE) return FALSE;
      break;
    case COMPRESS_MODE_INFLATE:
      error = decompressData(compressInfo);
      if (error != ERROR_NONE) return FALSE;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  /* data is avaialbe iff bufferIndex < bufferLength */
  (*availableBytes) = compressInfo->dataBufferLength-compressInfo->dataBufferIndex;

  return ERROR_NONE;
}

bool Compress_checkBlockIsFull(CompressInfo *compressInfo)
{
  Errors error;

  assert(compressInfo != NULL);

  switch (compressInfo->compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      error = compressData(compressInfo);
      if (error != ERROR_NONE) return FALSE;
      break;
    case COMPRESS_MODE_INFLATE:
      error = decompressData(compressInfo);
      if (error != ERROR_NONE) return FALSE;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
    
  /* compress buffer is full iff compressBufferLength >= blockLength */
  return (compressInfo->compressBufferLength >= compressInfo->blockLength);
}

bool Compress_checkBlockIsEmpty(CompressInfo *compressInfo)
{
  Errors error;

  assert(compressInfo != NULL);

  switch (compressInfo->compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      error = compressData(compressInfo);
      if (error != ERROR_NONE) return FALSE;
      break;
    case COMPRESS_MODE_INFLATE:
      error = decompressData(compressInfo);
      if (error != ERROR_NONE) return FALSE;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  /* compress buffer is empty iff compressBufferIndex >= compressBufferLength */
  return (compressInfo->compressBufferIndex >= compressInfo->compressBufferLength);
}

#if 0
bool Compress_checkEndOfBlock(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);
  assert(compressInfo->dataBuffer != NULL);
  assert(compressInfo->compressBuffer != NULL);
  assert(compressInfo->dataBufferIndex <= compressInfo->dataBufferLength);
  assert(compressInfo->dataBufferLength <= compressInfo->dataBufferSize);
  assert(compressInfo->compressBufferIndex <= compressInfo->compressBufferLength);
  assert(compressInfo->compressBufferLength <= compressInfo->compressBufferSize);

  /* end of block iff bufferIndex >= bufferLength */
  return (compressInfo->bufferIndex >= compressInfo->bufferLength);
}
#endif /* 0 */

void Compress_getBlock(CompressInfo *compressInfo,
                       void         *buffer,
                       ulong        *bufferLength
                      )
{
  assert(compressInfo != NULL);
  assert(compressInfo->compressBuffer != NULL);
// ??? blocklength
  assert(compressInfo->compressBufferLength <= compressInfo->blockLength);
  assert(buffer != NULL);
  assert(bufferLength != NULL);

  memcpy(buffer,compressInfo->compressBuffer,compressInfo->compressBufferLength);
  memset(buffer+compressInfo->compressBufferLength,
         0,
         compressInfo->blockLength-compressInfo->compressBufferIndex
        );
  (*bufferLength) = compressInfo->blockLength;

  compressInfo->compressBufferIndex  = 0;
  compressInfo->compressBufferLength = 0;
}

void Compress_putBlock(CompressInfo *compressInfo,
                       void         *buffer,
                       ulong        bufferLength
                      )
{
  assert(compressInfo != NULL);
  assert(compressInfo->compressBuffer != NULL);
  assert(buffer != NULL);
  assert(bufferLength <= compressInfo->blockLength);

  memcpy(compressInfo->compressBuffer,buffer,bufferLength);
  compressInfo->compressBufferIndex  = 0;
  compressInfo->compressBufferLength = bufferLength;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
