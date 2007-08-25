/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/compress.c,v $
* $Revision: 1.3 $
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
                    uint               blockLength
                   )
{
  int compressionLevel;

  assert(compressInfo != NULL);

  /* init variables */
  compressInfo->compressMode      = compressMode; 
  compressInfo->compressAlgorithm = compressAlgorithm; 
  compressInfo->blockLength       = blockLength;

  /* allocate buffer */
  compressInfo->buffer = malloc(blockLength);
  if (compressInfo->buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  compressInfo->bufferIndex  = 0;
  compressInfo->bufferLength = 0;

  /* init zlib stream */
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
            free(compressInfo->buffer);
            return ERROR_INIT_COMPRESS;
          }
          break;
        case COMPRESS_MODE_INFLATE:
          if (inflateInit(&compressInfo->zlib.stream) != Z_OK)
          {
            free(compressInfo->buffer);
            return ERROR_INIT_COMPRESS;
          }
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
  assert(compressInfo->buffer != NULL);

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
  free(compressInfo->buffer);
}

Errors Compress_reset(CompressInfo *compressInfo)
{
  int zlibError;

  assert(compressInfo != NULL);

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
  compressInfo->bufferIndex  = 0;
  compressInfo->bufferLength = 0;

  return ERROR_NONE;
}

Errors Compress_deflate(CompressInfo *compressInfo,
                        byte         data
                       )
{
  uint n;

  assert(compressInfo != NULL);
  assert(compressInfo->compressMode == COMPRESS_MODE_DEFLATE);
  assert(compressInfo->buffer != NULL);
  assert(compressInfo->bufferIndex < compressInfo->blockLength);
  assert(compressInfo->compressMode == COMPRESS_MODE_DEFLATE);

  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      (*(compressInfo->buffer+compressInfo->bufferIndex)) = data;
      compressInfo->bufferIndex++;

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
      n = compressInfo->blockLength-compressInfo->bufferIndex;
      compressInfo->zlib.stream.next_in   = &data;
      compressInfo->zlib.stream.avail_in  = 1;
      compressInfo->zlib.stream.next_out  = compressInfo->buffer+compressInfo->bufferIndex;
      compressInfo->zlib.stream.avail_out = n;
      if (deflate(&compressInfo->zlib.stream,Z_NO_FLUSH) != Z_OK)
      {
        return ERROR_COMPRESS_ERROR;
      }
      compressInfo->bufferIndex  += n-compressInfo->zlib.stream.avail_out;
      compressInfo->bufferLength += n-compressInfo->zlib.stream.avail_out;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

Errors Compress_inflate(CompressInfo *compressInfo,
                        byte         *data
                       )
{
  uint n;

  assert(compressInfo != NULL);
  assert(compressInfo->compressMode == COMPRESS_MODE_INFLATE);
  assert(compressInfo->buffer != NULL);
  assert(compressInfo->bufferIndex < compressInfo->bufferLength);
  assert(data != NULL);

  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      (*data) = (*(compressInfo->buffer+compressInfo->bufferIndex));
      compressInfo->bufferIndex++;

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
      n = compressInfo->blockLength-compressInfo->bufferIndex;
      compressInfo->zlib.stream.next_in   = compressInfo->buffer+compressInfo->bufferIndex;
      compressInfo->zlib.stream.avail_in  = n;
      compressInfo->zlib.stream.next_out  = data;
      compressInfo->zlib.stream.avail_out = 1;
      if (inflate(&compressInfo->zlib.stream,Z_NO_FLUSH) != Z_OK)
      {
        return ERROR_COMPRESS_ERROR;
      }
      compressInfo->bufferIndex += n-compressInfo->zlib.stream.avail_in;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

Errors Compress_flush(CompressInfo *compressInfo)
{
  uint n;
  int  zlibError;

  assert(compressInfo != NULL);
  assert(compressInfo->buffer != NULL);
  assert(compressInfo->bufferIndex <= compressInfo->blockLength);

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
      n = compressInfo->blockLength-compressInfo->bufferIndex;
      compressInfo->zlib.stream.next_in   = NULL;
      compressInfo->zlib.stream.avail_in  = 0;
      compressInfo->zlib.stream.next_out  = compressInfo->buffer+compressInfo->bufferIndex;
      compressInfo->zlib.stream.avail_out = n;
      switch (compressInfo->compressMode)
      {
        case COMPRESS_MODE_DEFLATE:
          zlibError = deflate(&compressInfo->zlib.stream,Z_FINISH);
          break;
        case COMPRESS_MODE_INFLATE:
          zlibError = inflate(&compressInfo->zlib.stream,Z_FINISH);
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
      compressInfo->bufferIndex  += n-compressInfo->zlib.stream.avail_out;
      compressInfo->bufferLength += n-compressInfo->zlib.stream.avail_out;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

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

bool Compress_checkBlockIsFull(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return (compressInfo->bufferIndex >= compressInfo->blockLength);
}

bool Compress_checkBlockIsEmpty(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return (compressInfo->bufferIndex == 0);
}

bool Compress_checkEndOfBlock(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return (compressInfo->bufferIndex >= compressInfo->bufferLength);
}

void Compress_getBlock(CompressInfo *compressInfo,
                       void         *buffer,
                       ulong        *bufferLength
                      )
{
  assert(compressInfo != NULL);
  assert(compressInfo->buffer != NULL);
  assert(compressInfo->bufferLength <= compressInfo->blockLength);
  assert(buffer != NULL);
  assert(bufferLength != NULL);

  memcpy(buffer,compressInfo->buffer,compressInfo->bufferIndex);
  memset(buffer+compressInfo->bufferIndex,
         0,
         compressInfo->blockLength-compressInfo->bufferIndex
        );
  (*bufferLength) = compressInfo->bufferLength;

  compressInfo->bufferIndex  = 0;
  compressInfo->bufferLength = 0;
}

void Compress_putBlock(CompressInfo *compressInfo,
                       void         *buffer,
                       ulong        bufferLength
                      )
{
  assert(compressInfo != NULL);
  assert(compressInfo->buffer != NULL);
  assert(buffer != NULL);
  assert(bufferLength <= compressInfo->blockLength);

  memcpy(compressInfo->buffer,buffer,bufferLength);
  compressInfo->bufferIndex  = 0;
  compressInfo->bufferLength = bufferLength;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
