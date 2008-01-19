/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/compress.c,v $
* $Revision: 1.14 $
* $Author: torsten $
* Contents: Backup ARchiver compress functions
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_Z
  #include <zlib.h>
#endif /* HAVE_Z */
#ifdef HAVE_BZ2
  #include <bzlib.h>
#endif /* HAVE_BZ2 */
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "lists.h"

#include "bar.h"

#include "compress.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

LOCAL const struct { const char *name; CompressAlgorithms compressAlgorithm; } COMPRESS_ALGORITHMS[] =
{
  { "none",  COMPRESS_ALGORITHM_NONE    },

  { "zip0",  COMPRESS_ALGORITHM_ZIP_0   },
  { "zip1",  COMPRESS_ALGORITHM_ZIP_1   },
  { "zip2",  COMPRESS_ALGORITHM_ZIP_2   },
  { "zip3",  COMPRESS_ALGORITHM_ZIP_3   },
  { "zip4",  COMPRESS_ALGORITHM_ZIP_4   },
  { "zip5",  COMPRESS_ALGORITHM_ZIP_5   },
  { "zip6",  COMPRESS_ALGORITHM_ZIP_6   },
  { "zip7",  COMPRESS_ALGORITHM_ZIP_7   },
  { "zip8",  COMPRESS_ALGORITHM_ZIP_8   },
  { "zip9",  COMPRESS_ALGORITHM_ZIP_9   },

  { "bzip1", COMPRESS_ALGORITHM_BZIP2_1 },
  { "bzip2", COMPRESS_ALGORITHM_BZIP2_2 },
  { "bzip3", COMPRESS_ALGORITHM_BZIP2_3 },
  { "bzip4", COMPRESS_ALGORITHM_BZIP2_4 },
  { "bzip5", COMPRESS_ALGORITHM_BZIP2_5 },
  { "bzip6", COMPRESS_ALGORITHM_BZIP2_6 },
  { "bzip7", COMPRESS_ALGORITHM_BZIP2_7 },
  { "bzip8", COMPRESS_ALGORITHM_BZIP2_8 },
  { "bzip9", COMPRESS_ALGORITHM_BZIP2_9 },
};

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : compressData
* Purpose: compress data if possible
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors compressData(CompressInfo *compressInfo)
{
  ulong maxCompressBytes,maxDataBytes;
  ulong n;

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
      if (compressInfo->compressBufferLength < compressInfo->compressBufferSize)  // space in compress buffer
      {
        if (compressInfo->dataBufferLength > 0)  // data available
        {
          /* copy from data buffer -> compress buffer */
          assert(compressInfo->dataBufferIndex == 0);
          maxDataBytes     = compressInfo->dataBufferLength;
          maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;
          n = MIN(maxDataBytes,maxCompressBytes);
          memcpy(compressInfo->compressBuffer+compressInfo->compressBufferLength,
                 compressInfo->dataBuffer,
                 n
                );
          compressInfo->compressState = COMPRESS_STATE_RUNNING;
          compressInfo->compressBufferLength += n;

          /* shift data buffer */
          memmove(compressInfo->dataBuffer,compressInfo->dataBuffer+n,compressInfo->dataBufferLength-n);
          compressInfo->dataBufferLength -= n;

          compressInfo->none.length += n;
        }
      }
      break;
    case COMPRESS_ALGORITHM_ZIP_0:
    case COMPRESS_ALGORITHM_ZIP_1:
    case COMPRESS_ALGORITHM_ZIP_2:
    case COMPRESS_ALGORITHM_ZIP_3:
    case COMPRESS_ALGORITHM_ZIP_4:
    case COMPRESS_ALGORITHM_ZIP_5:
    case COMPRESS_ALGORITHM_ZIP_6:
    case COMPRESS_ALGORITHM_ZIP_7:
    case COMPRESS_ALGORITHM_ZIP_8:
    case COMPRESS_ALGORITHM_ZIP_9:
      #ifdef HAVE_Z
        {
          int zlibError;

          if (   (compressInfo->compressBufferLength < compressInfo->compressBufferSize)  // space in compress buffer
              && !compressInfo->endOfDataFlag                                             // not end-of-data
             )
          {
            /* compress available data */
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
              if (    (zlibError != Z_OK)
                   && (zlibError != Z_BUF_ERROR)
                 )
              {
                return ERROR_COMPRESS_ERROR;
              }
              compressInfo->compressState = COMPRESS_STATE_RUNNING;
              compressInfo->compressBufferLength += maxCompressBytes-compressInfo->zlib.stream.avail_out;

              /* shift data buffer */
              n = maxDataBytes-compressInfo->zlib.stream.avail_in;
              memmove(compressInfo->dataBuffer,compressInfo->dataBuffer+n,compressInfo->dataBufferLength-n);
              compressInfo->dataBufferLength -= n;
            }
          }
          if (   (compressInfo->compressBufferLength < compressInfo->compressBufferSize)  // space in compress buffer
              && !compressInfo->endOfDataFlag                                             // not end-of-data
             )
          {
            /* finish compress, flush internal compress buffers */
            if (   (compressInfo->flushFlag)                                // flush data requested
                && (compressInfo->compressState == COMPRESS_STATE_RUNNING)  // compressor is running -> data available in internal buffers
               )
            {
              /* compress with flush */
              maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;
              compressInfo->zlib.stream.next_in   = NULL;
              compressInfo->zlib.stream.avail_in  = 0;
              compressInfo->zlib.stream.next_out  = compressInfo->compressBuffer+compressInfo->compressBufferLength;
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
                return ERROR_COMPRESS_ERROR;
              }
              compressInfo->compressBufferLength += maxCompressBytes-compressInfo->zlib.stream.avail_out;
            }
          }
        }
      #else /* not HAVE_Z */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_Z */
      break;
    case COMPRESS_ALGORITHM_BZIP2_1:
    case COMPRESS_ALGORITHM_BZIP2_2:
    case COMPRESS_ALGORITHM_BZIP2_3:
    case COMPRESS_ALGORITHM_BZIP2_4:
    case COMPRESS_ALGORITHM_BZIP2_5:
    case COMPRESS_ALGORITHM_BZIP2_6:
    case COMPRESS_ALGORITHM_BZIP2_7:
    case COMPRESS_ALGORITHM_BZIP2_8:
    case COMPRESS_ALGORITHM_BZIP2_9:
      #ifdef HAVE_BZ2
        {
          int bzlibError;

          if (   (compressInfo->compressBufferLength < compressInfo->compressBufferSize)  // space in compress buffer
              && !compressInfo->endOfDataFlag                                             // not end-of-data
             )
          {
            /* compress available data */
            if      (compressInfo->dataBufferLength > 0)  // data available
            {
              /* compress */
              maxDataBytes     = compressInfo->dataBufferLength;
              maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;
              compressInfo->bzlib.stream.next_in   = (char*)(compressInfo->dataBuffer+compressInfo->dataBufferIndex);
              compressInfo->bzlib.stream.avail_in  = maxDataBytes;
              compressInfo->bzlib.stream.next_out  = (char*)(compressInfo->compressBuffer+compressInfo->compressBufferLength);
              compressInfo->bzlib.stream.avail_out = maxCompressBytes;
              bzlibError = BZ2_bzCompress(&compressInfo->bzlib.stream,BZ_RUN);
              if (bzlibError != BZ_RUN_OK)
              {
                return ERROR_COMPRESS_ERROR;
              }
              compressInfo->compressState = COMPRESS_STATE_RUNNING;
              compressInfo->compressBufferLength += maxCompressBytes-compressInfo->bzlib.stream.avail_out;

              /* shift data buffer */
              n = maxDataBytes-compressInfo->bzlib.stream.avail_in;
              memmove(compressInfo->dataBuffer,compressInfo->dataBuffer+n,compressInfo->dataBufferLength-n);
              compressInfo->dataBufferLength -= n;
            }
          }
          if (   (compressInfo->compressBufferLength < compressInfo->compressBufferSize)  // space in compress buffer
              && !compressInfo->endOfDataFlag                                             // not end-of-data
             )
          {
            /* finish compress, flush internal compress buffers */
            if (   (compressInfo->flushFlag)                                // flush data requested
                && (compressInfo->compressState == COMPRESS_STATE_RUNNING)  // compressor is running -> data available in internal buffers
               )
            {
              /* compress with flush */
              maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;
              compressInfo->bzlib.stream.next_in   = NULL;
              compressInfo->bzlib.stream.avail_in  = 0;
              compressInfo->bzlib.stream.next_out  = (char*)(compressInfo->compressBuffer+compressInfo->compressBufferLength);
              compressInfo->bzlib.stream.avail_out = maxCompressBytes;
              bzlibError = BZ2_bzCompress(&compressInfo->bzlib.stream,BZ_FINISH);
              if      (bzlibError == BZ_STREAM_END)
              {
                compressInfo->endOfDataFlag = TRUE;
              }
              else if (bzlibError != BZ_FINISH_OK)
              {
                return ERROR_COMPRESS_ERROR;
              }
              compressInfo->compressBufferLength += maxCompressBytes-compressInfo->bzlib.stream.avail_out;
            }
          }
        }
      #else /* not HAVE_BZ2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_BZ2 */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : decompressData
* Purpose: decompress data if possible
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors decompressData(CompressInfo *compressInfo)
{
  ulong maxCompressBytes,maxDataBytes;
  ulong n;

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
      if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)  // no data in buffer
      {
        compressInfo->dataBufferIndex  = 0;
        compressInfo->dataBufferLength = 0;

        if (compressInfo->compressBufferIndex < compressInfo->compressBufferLength)  // unprocessed compressed data available
        {
          /* copy from compress buffer -> data buffer */
          maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
          maxDataBytes     = compressInfo->dataBufferSize;
          n = MIN(maxCompressBytes,maxDataBytes);
          memcpy(compressInfo->dataBuffer,
                 compressInfo->compressBuffer+compressInfo->compressBufferIndex,
                 n
                );
          compressInfo->compressState = COMPRESS_STATE_RUNNING;
          compressInfo->compressBufferIndex += n;

          compressInfo->dataBufferLength = n;

          compressInfo->none.length += n;
        }
      }
      break;
    case COMPRESS_ALGORITHM_ZIP_0:
    case COMPRESS_ALGORITHM_ZIP_1:
    case COMPRESS_ALGORITHM_ZIP_2:
    case COMPRESS_ALGORITHM_ZIP_3:
    case COMPRESS_ALGORITHM_ZIP_4:
    case COMPRESS_ALGORITHM_ZIP_5:
    case COMPRESS_ALGORITHM_ZIP_6:
    case COMPRESS_ALGORITHM_ZIP_7:
    case COMPRESS_ALGORITHM_ZIP_8:
    case COMPRESS_ALGORITHM_ZIP_9:
      #ifdef HAVE_Z
        {
          int zlibError;

          if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)  // no data in buffer
          {
            compressInfo->dataBufferIndex  = 0;
            compressInfo->dataBufferLength = 0;

            if (!compressInfo->endOfDataFlag)
            {
              /* decompress available data */
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
                if      (zlibError == Z_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (   (zlibError != Z_OK)
                         && (zlibError != Z_BUF_ERROR)
                        )
                {
                  return ERROR_COMPRESS_ERROR;
                }
                compressInfo->compressState = COMPRESS_STATE_RUNNING;
                compressInfo->compressBufferIndex += maxCompressBytes-compressInfo->zlib.stream.avail_in;
                compressInfo->dataBufferLength += maxDataBytes-compressInfo->zlib.stream.avail_out;
              }
            }
          }
          if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)  // no data in buffer
          {
            compressInfo->dataBufferIndex  = 0;
            compressInfo->dataBufferLength = 0;

            if (!compressInfo->endOfDataFlag)
            {
              /* finish decompress, flush internal decompress buffers */
              if (   (compressInfo->flushFlag)                                // flush data requested
                  && (compressInfo->compressState == COMPRESS_STATE_RUNNING)  // compressor is running -> data available in internal buffers
                 )
              {
                /* decompress with flush */
                maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
                maxDataBytes     = compressInfo->dataBufferSize-compressInfo->dataBufferLength;
                compressInfo->zlib.stream.next_in   = NULL;
                compressInfo->zlib.stream.avail_in  = 0;
                compressInfo->zlib.stream.next_out  = compressInfo->dataBuffer+compressInfo->dataBufferLength;
                compressInfo->zlib.stream.avail_out = maxDataBytes;
                zlibError = inflate(&compressInfo->zlib.stream,Z_FINISH);
                if      (zlibError == Z_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (   (zlibError != Z_OK)
                         && (zlibError != Z_BUF_ERROR)
                        )
                {
                  return ERROR_COMPRESS_ERROR;
                }
                compressInfo->dataBufferLength += maxDataBytes-compressInfo->zlib.stream.avail_out;
              }
            }
          }
        }
      #else /* not HAVE_Z */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_Z */
      break;
    case COMPRESS_ALGORITHM_BZIP2_1:
    case COMPRESS_ALGORITHM_BZIP2_2:
    case COMPRESS_ALGORITHM_BZIP2_3:
    case COMPRESS_ALGORITHM_BZIP2_4:
    case COMPRESS_ALGORITHM_BZIP2_5:
    case COMPRESS_ALGORITHM_BZIP2_6:
    case COMPRESS_ALGORITHM_BZIP2_7:
    case COMPRESS_ALGORITHM_BZIP2_8:
    case COMPRESS_ALGORITHM_BZIP2_9:
      #ifdef HAVE_BZ2
        {
          int bzlibError;

          if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)  // no data in buffer
          {
            compressInfo->dataBufferIndex  = 0;
            compressInfo->dataBufferLength = 0;

            if (!compressInfo->endOfDataFlag)
            {
              /* decompress available data */
              if      (compressInfo->compressBufferIndex < compressInfo->compressBufferLength)  // unprocessed compressed data available
              {
                /* decompress */
                maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
                maxDataBytes     = compressInfo->dataBufferSize-compressInfo->dataBufferLength;
                compressInfo->bzlib.stream.next_in   = (char*)(compressInfo->compressBuffer+compressInfo->compressBufferIndex);
                compressInfo->bzlib.stream.avail_in  = maxCompressBytes;
                compressInfo->bzlib.stream.next_out  = (char*)(compressInfo->dataBuffer+compressInfo->dataBufferLength);
                compressInfo->bzlib.stream.avail_out = maxDataBytes;
                bzlibError = BZ2_bzDecompress(&compressInfo->bzlib.stream);
                if      (bzlibError == BZ_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (bzlibError != BZ_OK)
                {
                  return ERROR_COMPRESS_ERROR;
                }
                compressInfo->compressState = COMPRESS_STATE_RUNNING;
                compressInfo->compressBufferIndex += maxCompressBytes-compressInfo->bzlib.stream.avail_in;
                compressInfo->dataBufferLength += maxDataBytes-compressInfo->bzlib.stream.avail_out;
              }
            }
          }
          if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)  // no data in buffer
          {
            compressInfo->dataBufferIndex  = 0;
            compressInfo->dataBufferLength = 0;

            if (!compressInfo->endOfDataFlag)
            {
              /* finish decompress, flush internal decompress buffers */
              if (   (compressInfo->flushFlag)                                // flush data requested
                  && (compressInfo->compressState == COMPRESS_STATE_RUNNING)  // compressor is running -> data available in internal buffers
                 )
              {
                /* decompress with flush */
                maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
                maxDataBytes     = compressInfo->dataBufferSize-compressInfo->dataBufferLength;
                compressInfo->bzlib.stream.next_in   = NULL;
                compressInfo->bzlib.stream.avail_in  = 0;
                compressInfo->bzlib.stream.next_out  = (char*)(compressInfo->dataBuffer+compressInfo->dataBufferLength);
                compressInfo->bzlib.stream.avail_out = maxDataBytes;
                bzlibError = BZ2_bzDecompress(&compressInfo->bzlib.stream);
                if      (bzlibError == BZ_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (bzlibError != BZ_RUN_OK)
                {
                  return ERROR_COMPRESS_ERROR;
                }
                compressInfo->dataBufferLength += maxDataBytes-compressInfo->bzlib.stream.avail_out;
              }
            }
          }
        }
      #else /* not HAVE_BZ2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_BZ2 */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
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

const char *Compress_getAlgorithmName(CompressAlgorithms compressAlgorithm)
{
  int        z;
  const char *s;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS))
         && (COMPRESS_ALGORITHMS[z].compressAlgorithm != compressAlgorithm)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS))
  {
    s = COMPRESS_ALGORITHMS[z].name;
  }
  else
  {
    s = "unknown";
  }

  return s;
}

CompressAlgorithms Compress_getAlgorithm(const char *name)
{
  int                z;
  CompressAlgorithms compressAlgorithm;

  assert(name != NULL);

  z = 0;
  while (   (z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS))
         && (strcmp(COMPRESS_ALGORITHMS[z].name,name) != 0)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS))
  {
    compressAlgorithm = COMPRESS_ALGORITHMS[z].compressAlgorithm;
  }
  else
  {
    compressAlgorithm = COMPRESS_ALGORITHM_UNKNOWN;
  }

  return compressAlgorithm;
}

Errors Compress_new(CompressInfo       *compressInfo,
                    CompressModes      compressMode,
                    CompressAlgorithms compressAlgorithm,
                    ulong              blockLength
                   )
{
  assert(compressInfo != NULL);
//compressAlgorithm = COMPRESS_ALGORITHM_NONE;

  /* init variables */
  compressInfo->compressMode         = compressMode; 
  compressInfo->compressAlgorithm    = compressAlgorithm; 
  compressInfo->blockLength          = blockLength;
  compressInfo->compressState        = COMPRESS_STATE_INIT;
  compressInfo->endOfDataFlag        = FALSE;
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
    case COMPRESS_ALGORITHM_ZIP_0:
    case COMPRESS_ALGORITHM_ZIP_1:
    case COMPRESS_ALGORITHM_ZIP_2:
    case COMPRESS_ALGORITHM_ZIP_3:
    case COMPRESS_ALGORITHM_ZIP_4:
    case COMPRESS_ALGORITHM_ZIP_5:
    case COMPRESS_ALGORITHM_ZIP_6:
    case COMPRESS_ALGORITHM_ZIP_7:
    case COMPRESS_ALGORITHM_ZIP_8:
    case COMPRESS_ALGORITHM_ZIP_9:
      #ifdef HAVE_Z
        {
          int compressionLevel;

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
        }
      #else /* not HAVE_Z */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_Z */
      break;
    case COMPRESS_ALGORITHM_BZIP2_1:
    case COMPRESS_ALGORITHM_BZIP2_2:
    case COMPRESS_ALGORITHM_BZIP2_3:
    case COMPRESS_ALGORITHM_BZIP2_4:
    case COMPRESS_ALGORITHM_BZIP2_5:
    case COMPRESS_ALGORITHM_BZIP2_6:
    case COMPRESS_ALGORITHM_BZIP2_7:
    case COMPRESS_ALGORITHM_BZIP2_8:
    case COMPRESS_ALGORITHM_BZIP2_9:
      #ifdef HAVE_BZ2
        {
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
              if (BZ2_bzCompressInit(&compressInfo->bzlib.stream,compressInfo->bzlib.compressionLevel,0,0) != BZ_OK)
              {
                free(compressInfo->compressBuffer);
                free(compressInfo->dataBuffer);
                return ERROR_INIT_COMPRESS;
              }
              break;
            case COMPRESS_MODE_INFLATE:
              if (BZ2_bzDecompressInit(&compressInfo->bzlib.stream,0,0) != BZ_OK)
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
        }
      #else /* not HAVE_BZ2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_BZ2 */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
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
    case COMPRESS_ALGORITHM_ZIP_0:
    case COMPRESS_ALGORITHM_ZIP_1:
    case COMPRESS_ALGORITHM_ZIP_2:
    case COMPRESS_ALGORITHM_ZIP_3:
    case COMPRESS_ALGORITHM_ZIP_4:
    case COMPRESS_ALGORITHM_ZIP_5:
    case COMPRESS_ALGORITHM_ZIP_6:
    case COMPRESS_ALGORITHM_ZIP_7:
    case COMPRESS_ALGORITHM_ZIP_8:
    case COMPRESS_ALGORITHM_ZIP_9:
      #ifdef HAVE_Z
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
      #else /* not HAVE_Z */
      #endif /* HAVE_Z */
      break;
    case COMPRESS_ALGORITHM_BZIP2_1:
    case COMPRESS_ALGORITHM_BZIP2_2:
    case COMPRESS_ALGORITHM_BZIP2_3:
    case COMPRESS_ALGORITHM_BZIP2_4:
    case COMPRESS_ALGORITHM_BZIP2_5:
    case COMPRESS_ALGORITHM_BZIP2_6:
    case COMPRESS_ALGORITHM_BZIP2_7:
    case COMPRESS_ALGORITHM_BZIP2_8:
    case COMPRESS_ALGORITHM_BZIP2_9:
      #ifdef HAVE_BZ2
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
      #else /* not HAVE_BZ2 */
      #endif /* HAVE_BZ2 */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  free(compressInfo->compressBuffer);
  free(compressInfo->dataBuffer);
}

Errors Compress_reset(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  /* init variables */
  compressInfo->compressState        = COMPRESS_STATE_INIT;
  compressInfo->endOfDataFlag        = FALSE;
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
    case COMPRESS_ALGORITHM_ZIP_0:
    case COMPRESS_ALGORITHM_ZIP_1:
    case COMPRESS_ALGORITHM_ZIP_2:
    case COMPRESS_ALGORITHM_ZIP_3:
    case COMPRESS_ALGORITHM_ZIP_4:
    case COMPRESS_ALGORITHM_ZIP_5:
    case COMPRESS_ALGORITHM_ZIP_6:
    case COMPRESS_ALGORITHM_ZIP_7:
    case COMPRESS_ALGORITHM_ZIP_8:
    case COMPRESS_ALGORITHM_ZIP_9:
      #ifdef HAVE_Z
        {
          int zlibError;

          zlibError = Z_ERRNO;
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
        }
      #else /* not HAVE_Z */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_Z */
      break;
    case COMPRESS_ALGORITHM_BZIP2_1:
    case COMPRESS_ALGORITHM_BZIP2_2:
    case COMPRESS_ALGORITHM_BZIP2_3:
    case COMPRESS_ALGORITHM_BZIP2_4:
    case COMPRESS_ALGORITHM_BZIP2_5:
    case COMPRESS_ALGORITHM_BZIP2_6:
    case COMPRESS_ALGORITHM_BZIP2_7:
    case COMPRESS_ALGORITHM_BZIP2_8:
    case COMPRESS_ALGORITHM_BZIP2_9:
      #ifdef HAVE_BZ2
        {
          int bzlibError;

          bzlibError = BZ_PARAM_ERROR;
          switch (compressInfo->compressMode)
          {
            case COMPRESS_MODE_DEFLATE:
              BZ2_bzCompressEnd(&compressInfo->bzlib.stream);
              bzlibError = BZ2_bzCompressInit(&compressInfo->bzlib.stream,compressInfo->bzlib.compressionLevel,0,0);
              break;
            case COMPRESS_MODE_INFLATE:
              BZ2_bzDecompressEnd(&compressInfo->bzlib.stream);
              bzlibError = BZ2_bzDecompressInit(&compressInfo->bzlib.stream,0,0);
              break;
            #ifndef NDEBUG
              default:
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                break; /* not reached */
            #endif /* NDEBUG */
          }
          if (bzlibError != BZ_OK)
          {
            return ERROR_COMPRESS_ERROR;
          }
        }
      #else /* not HAVE_BZ2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_BZ2 */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return ERROR_NONE;
}

Errors Compress_deflate(CompressInfo *compressInfo,
                        byte         data
                       )
{
  Errors error;

  assert(compressInfo != NULL);
  assert(compressInfo->compressMode == COMPRESS_MODE_DEFLATE);

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

  return ERROR_NONE;
}

Errors Compress_inflate(CompressInfo *compressInfo,
                        byte         *data
                       )
{
  Errors error;

  assert(compressInfo != NULL);
  assert(compressInfo->compressMode == COMPRESS_MODE_INFLATE);
  assert(data != NULL);

  if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)
  {
    error = decompressData(compressInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  assert(compressInfo->dataBufferIndex < compressInfo->dataBufferLength);
  (*data) = (*(compressInfo->dataBuffer+compressInfo->dataBufferIndex));
  compressInfo->dataBufferIndex++;

  return ERROR_NONE;
}

Errors Compress_flush(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  compressInfo->flushFlag = TRUE;
//fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);

  return ERROR_NONE;
}

uint64 Compress_getInputLength(CompressInfo *compressInfo)
{
  uint64 length;

  assert(compressInfo != NULL);

  length = 0LL;
  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      length = compressInfo->none.length;
      break;
    case COMPRESS_ALGORITHM_ZIP_0:
    case COMPRESS_ALGORITHM_ZIP_1:
    case COMPRESS_ALGORITHM_ZIP_2:
    case COMPRESS_ALGORITHM_ZIP_3:
    case COMPRESS_ALGORITHM_ZIP_4:
    case COMPRESS_ALGORITHM_ZIP_5:
    case COMPRESS_ALGORITHM_ZIP_6:
    case COMPRESS_ALGORITHM_ZIP_7:
    case COMPRESS_ALGORITHM_ZIP_8:
    case COMPRESS_ALGORITHM_ZIP_9:
      #ifdef HAVE_Z
        length = (uint64)compressInfo->zlib.stream.total_in;
      #else /* not HAVE_Z */
        length = 0LL;
      #endif /* HAVE_Z */
      break;
    case COMPRESS_ALGORITHM_BZIP2_1:
    case COMPRESS_ALGORITHM_BZIP2_2:
    case COMPRESS_ALGORITHM_BZIP2_3:
    case COMPRESS_ALGORITHM_BZIP2_4:
    case COMPRESS_ALGORITHM_BZIP2_5:
    case COMPRESS_ALGORITHM_BZIP2_6:
    case COMPRESS_ALGORITHM_BZIP2_7:
    case COMPRESS_ALGORITHM_BZIP2_8:
    case COMPRESS_ALGORITHM_BZIP2_9:
      #ifdef HAVE_BZ2
        length = ((uint64)compressInfo->bzlib.stream.total_in_hi32 << 32) | ((uint64)compressInfo->bzlib.stream.total_in_lo32 << 0);
      #else /* not HAVE_BZ2 */
        length = 0LL;
      #endif /* HAVE_BZ2 */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return length;
}

uint64 Compress_getOutputLength(CompressInfo *compressInfo)
{
  uint64 length;

  assert(compressInfo != NULL);

  length = 0LL;
  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      length = compressInfo->none.length;
      break;
    case COMPRESS_ALGORITHM_ZIP_0:
    case COMPRESS_ALGORITHM_ZIP_1:
    case COMPRESS_ALGORITHM_ZIP_2:
    case COMPRESS_ALGORITHM_ZIP_3:
    case COMPRESS_ALGORITHM_ZIP_4:
    case COMPRESS_ALGORITHM_ZIP_5:
    case COMPRESS_ALGORITHM_ZIP_6:
    case COMPRESS_ALGORITHM_ZIP_7:
    case COMPRESS_ALGORITHM_ZIP_8:
    case COMPRESS_ALGORITHM_ZIP_9:
      #ifdef HAVE_Z
        length = (uint64)compressInfo->zlib.stream.total_out;
      #else /* not HAVE_Z */
        length = 0LL;
      #endif /* HAVE_Z */
      break;
    case COMPRESS_ALGORITHM_BZIP2_1:
    case COMPRESS_ALGORITHM_BZIP2_2:
    case COMPRESS_ALGORITHM_BZIP2_3:
    case COMPRESS_ALGORITHM_BZIP2_4:
    case COMPRESS_ALGORITHM_BZIP2_5:
    case COMPRESS_ALGORITHM_BZIP2_6:
    case COMPRESS_ALGORITHM_BZIP2_7:
    case COMPRESS_ALGORITHM_BZIP2_8:
    case COMPRESS_ALGORITHM_BZIP2_9:
      #ifdef HAVE_BZ2
        length = ((uint64)compressInfo->bzlib.stream.total_out_hi32) | ((uint64)compressInfo->bzlib.stream.total_out_lo32 << 0);
      #else /* not HAVE_BZ2 */
        length = 0LL;
      #endif /* HAVE_BZ2 */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
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
      if (error != ERROR_NONE) return error;
      break;
    case COMPRESS_MODE_INFLATE:
      error = decompressData(compressInfo);
      if (error != ERROR_NONE) return error;
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
      if (error != ERROR_NONE) return TRUE;
      break;
    case COMPRESS_MODE_INFLATE:
      error = decompressData(compressInfo);
      if (error != ERROR_NONE) return TRUE;
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
                       byte         *buffer,
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
  (*bufferLength) = compressInfo->compressBufferLength;

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
