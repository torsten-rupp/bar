/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/compress.c,v $
* $Revision: 1.4 $
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
#ifdef HAVE_LZMA
  #include <lzma.h>
#endif /* HAVE_LZMA */
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

  { "lzma1", COMPRESS_ALGORITHM_LZMA_1 },
  { "lzma2", COMPRESS_ALGORITHM_LZMA_2 },
  { "lzma3", COMPRESS_ALGORITHM_LZMA_3 },
  { "lzma4", COMPRESS_ALGORITHM_LZMA_4 },
  { "lzma5", COMPRESS_ALGORITHM_LZMA_5 },
  { "lzma6", COMPRESS_ALGORITHM_LZMA_6 },
  { "lzma7", COMPRESS_ALGORITHM_LZMA_7 },
  { "lzma8", COMPRESS_ALGORITHM_LZMA_8 },
  { "lzma9", COMPRESS_ALGORITHM_LZMA_9 },
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
    case COMPRESS_ALGORITHM_LZMA_1:
    case COMPRESS_ALGORITHM_LZMA_2:
    case COMPRESS_ALGORITHM_LZMA_3:
    case COMPRESS_ALGORITHM_LZMA_4:
    case COMPRESS_ALGORITHM_LZMA_5:
    case COMPRESS_ALGORITHM_LZMA_6:
    case COMPRESS_ALGORITHM_LZMA_7:
    case COMPRESS_ALGORITHM_LZMA_8:
    case COMPRESS_ALGORITHM_LZMA_9:
      #ifdef HAVE_LZMA
        {
          lzma_ret lzmaResult;

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
              compressInfo->lzmalib.stream.next_in   = (uint8_t*)(compressInfo->dataBuffer+compressInfo->dataBufferIndex);
              compressInfo->lzmalib.stream.avail_in  = maxDataBytes;
              compressInfo->lzmalib.stream.next_out  = (uint8_t*)(compressInfo->compressBuffer+compressInfo->compressBufferLength);
              compressInfo->lzmalib.stream.avail_out = maxCompressBytes;
              lzmaResult = lzma_code(&compressInfo->lzmalib.stream,LZMA_RUN);
              if (lzmaResult != LZMA_OK)
              {
                return ERROR(COMPRESS_ERROR,lzmaResult);
              }
              compressInfo->compressState = COMPRESS_STATE_RUNNING;
              compressInfo->compressBufferLength += maxCompressBytes-compressInfo->lzmalib.stream.avail_out;

              /* shift data buffer */
              n = maxDataBytes-compressInfo->lzmalib.stream.avail_in;
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
              compressInfo->lzmalib.stream.next_in   = NULL;
              compressInfo->lzmalib.stream.avail_in  = 0;
              compressInfo->lzmalib.stream.next_out  = (uint8_t*)(compressInfo->compressBuffer+compressInfo->compressBufferLength);
              compressInfo->lzmalib.stream.avail_out = maxCompressBytes;
              lzmaResult = lzma_code(&compressInfo->lzmalib.stream,LZMA_FINISH);
              if      (lzmaResult == LZMA_STREAM_END)
              {
                compressInfo->endOfDataFlag = TRUE;
              }
              else if (lzmaResult != LZMA_OK)
              {
                return ERROR(COMPRESS_ERROR,lzmaResult);
              }
              compressInfo->compressBufferLength += maxCompressBytes-compressInfo->lzmalib.stream.avail_out;
            }
          }
        }
      #else /* not HAVE_LZMA */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_LZMA */
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
          int zlibResult;

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
                zlibResult = inflate(&compressInfo->zlib.stream,Z_NO_FLUSH);
                if      (zlibResult == Z_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (   (zlibResult != Z_OK)
                         && (zlibResult != Z_BUF_ERROR)
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
                zlibResult = inflate(&compressInfo->zlib.stream,Z_FINISH);
                if      (zlibResult == Z_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (   (zlibResult != Z_OK)
                         && (zlibResult != Z_BUF_ERROR)
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
          int bzlibResult;

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
                bzlibResult = BZ2_bzDecompress(&compressInfo->bzlib.stream);
                if      (bzlibResult == BZ_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (bzlibResult != BZ_OK)
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
                bzlibResult = BZ2_bzDecompress(&compressInfo->bzlib.stream);
                if      (bzlibResult == BZ_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (bzlibResult != BZ_RUN_OK)
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
    case COMPRESS_ALGORITHM_LZMA_1:
    case COMPRESS_ALGORITHM_LZMA_2:
    case COMPRESS_ALGORITHM_LZMA_3:
    case COMPRESS_ALGORITHM_LZMA_4:
    case COMPRESS_ALGORITHM_LZMA_5:
    case COMPRESS_ALGORITHM_LZMA_6:
    case COMPRESS_ALGORITHM_LZMA_7:
    case COMPRESS_ALGORITHM_LZMA_8:
    case COMPRESS_ALGORITHM_LZMA_9:
      #ifdef HAVE_LZMA
        {
          lzma_ret lzmaResult;

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
                compressInfo->lzmalib.stream.next_in   = (uint8_t*)(compressInfo->compressBuffer+compressInfo->compressBufferIndex);
                compressInfo->lzmalib.stream.avail_in  = maxCompressBytes;
                compressInfo->lzmalib.stream.next_out  = (uint8_t*)(compressInfo->dataBuffer+compressInfo->dataBufferLength);
                compressInfo->lzmalib.stream.avail_out = maxDataBytes;
                lzmaResult = lzma_code(&compressInfo->lzmalib.stream,LZMA_RUN);
                if      (lzmaResult == LZMA_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (lzmaResult != LZMA_OK)
                {
                  return ERROR_COMPRESS_ERROR;
                }
                compressInfo->compressState = COMPRESS_STATE_RUNNING;
                compressInfo->compressBufferIndex += maxCompressBytes-compressInfo->lzmalib.stream.avail_in;
                compressInfo->dataBufferLength += maxDataBytes-compressInfo->lzmalib.stream.avail_out;
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
                compressInfo->lzmalib.stream.next_in   = NULL;
                compressInfo->lzmalib.stream.avail_in  = 0;
                compressInfo->lzmalib.stream.next_out  = (uint8_t*)(compressInfo->dataBuffer+compressInfo->dataBufferLength);
                compressInfo->lzmalib.stream.avail_out = maxDataBytes;
                lzmaResult = lzma_code(&compressInfo->lzmalib.stream,LZMA_FINISH);
                if      (lzmaResult == LZMA_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (lzmaResult != LZMA_OK)
                {
                  return ERROR_COMPRESS_ERROR;
                }
                compressInfo->dataBufferLength += maxDataBytes-compressInfo->lzmalib.stream.avail_out;
              }
            }
          }
        }
      #else /* not HAVE_LZMA */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_LZMA */
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
  compressInfo->dataBuffer = malloc(compressInfo->dataBufferSize);
  if (compressInfo->dataBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  compressInfo->compressBuffer = malloc(compressInfo->compressBufferSize);
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
    case COMPRESS_ALGORITHM_LZMA_1:
    case COMPRESS_ALGORITHM_LZMA_2:
    case COMPRESS_ALGORITHM_LZMA_3:
    case COMPRESS_ALGORITHM_LZMA_4:
    case COMPRESS_ALGORITHM_LZMA_5:
    case COMPRESS_ALGORITHM_LZMA_6:
    case COMPRESS_ALGORITHM_LZMA_7:
    case COMPRESS_ALGORITHM_LZMA_8:
    case COMPRESS_ALGORITHM_LZMA_9:
      #ifdef HAVE_LZMA
        {
          lzma_stream streamInit = LZMA_STREAM_INIT;

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
          compressInfo->lzmalib.stream.allocator = NULL;
          switch (compressMode)
          {
            case COMPRESS_MODE_DEFLATE:
              compressInfo->lzmalib.stream = streamInit;
              if (lzma_easy_encoder(&compressInfo->lzmalib.stream,compressInfo->lzmalib.compressionLevel,LZMA_CHECK_NONE) != LZMA_OK)
              {
                free(compressInfo->compressBuffer);
                free(compressInfo->dataBuffer);
                return ERROR_INIT_COMPRESS;
              }
              break;
            case COMPRESS_MODE_INFLATE:
              compressInfo->lzmalib.stream = streamInit;
              if (lzma_auto_decoder(&compressInfo->lzmalib.stream,0xFFFffffFFFFffffLL,0) != LZMA_OK)
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
      #else /* not HAVE_LZMA */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_LZMA */
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
    case COMPRESS_ALGORITHM_LZMA_1:
    case COMPRESS_ALGORITHM_LZMA_2:
    case COMPRESS_ALGORITHM_LZMA_3:
    case COMPRESS_ALGORITHM_LZMA_4:
    case COMPRESS_ALGORITHM_LZMA_5:
    case COMPRESS_ALGORITHM_LZMA_6:
    case COMPRESS_ALGORITHM_LZMA_7:
    case COMPRESS_ALGORITHM_LZMA_8:
    case COMPRESS_ALGORITHM_LZMA_9:
      #ifdef HAVE_LZMA
        lzma_end(&compressInfo->lzmalib.stream);
      #else /* not HAVE_LZMA */
      #endif /* HAVE_LZMA */
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
          int zlibResult;

          zlibResult = Z_ERRNO;
          switch (compressInfo->compressMode)
          {
            case COMPRESS_MODE_DEFLATE:
              zlibResult = deflateReset(&compressInfo->zlib.stream);
              break;
            case COMPRESS_MODE_INFLATE:
              zlibResult = inflateReset(&compressInfo->zlib.stream);
              break;
            #ifndef NDEBUG
              default:
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                break; /* not reached */
            #endif /* NDEBUG */
          }
          if ((zlibResult != Z_OK) && (zlibResult != Z_STREAM_END))
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
          int bzlibResult;

          bzlibResult = BZ_PARAM_ERROR;
          switch (compressInfo->compressMode)
          {
            case COMPRESS_MODE_DEFLATE:
              BZ2_bzCompressEnd(&compressInfo->bzlib.stream);
              bzlibResult = BZ2_bzCompressInit(&compressInfo->bzlib.stream,compressInfo->bzlib.compressionLevel,0,0);
              break;
            case COMPRESS_MODE_INFLATE:
              BZ2_bzDecompressEnd(&compressInfo->bzlib.stream);
              bzlibResult = BZ2_bzDecompressInit(&compressInfo->bzlib.stream,0,0);
              break;
            #ifndef NDEBUG
              default:
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                break; /* not reached */
            #endif /* NDEBUG */
          }
          if (bzlibResult != BZ_OK)
          {
            return ERROR_COMPRESS_ERROR;
          }
        }
      #else /* not HAVE_BZ2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_BZ2 */
      break;
    case COMPRESS_ALGORITHM_LZMA_1:
    case COMPRESS_ALGORITHM_LZMA_2:
    case COMPRESS_ALGORITHM_LZMA_3:
    case COMPRESS_ALGORITHM_LZMA_4:
    case COMPRESS_ALGORITHM_LZMA_5:
    case COMPRESS_ALGORITHM_LZMA_6:
    case COMPRESS_ALGORITHM_LZMA_7:
    case COMPRESS_ALGORITHM_LZMA_8:
    case COMPRESS_ALGORITHM_LZMA_9:
      #ifdef HAVE_LZMA
        {
          lzma_stream streamInit = LZMA_STREAM_INIT;
          int         lzmalibResult;

          lzmalibResult = LZMA_PROG_ERROR;
          switch (compressInfo->compressMode)
          {
            case COMPRESS_MODE_DEFLATE:
              lzma_end(&compressInfo->lzmalib.stream);
              compressInfo->lzmalib.stream = streamInit;
              lzmalibResult = lzma_easy_encoder(&compressInfo->lzmalib.stream,compressInfo->lzmalib.compressionLevel,LZMA_CHECK_NONE);
              break;
            case COMPRESS_MODE_INFLATE:
              lzma_end(&compressInfo->lzmalib.stream);
              compressInfo->lzmalib.stream = streamInit;
              lzmalibResult = lzma_auto_decoder(&compressInfo->lzmalib.stream,0xFFFffffFFFFffffLL,0);
              break;
            #ifndef NDEBUG
              default:
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                break; /* not reached */
            #endif /* NDEBUG */
          }
          if (lzmalibResult != LZMA_OK)
          {
            return ERROR_COMPRESS_ERROR;
          }
        }
      #else /* not HAVE_LZMA */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_LZMA */
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
                        const byte   *data,
                        ulong        length,
                        ulong        *deflatedBytess
                       )
{
  Errors error;
  ulong  n;

  assert(compressInfo != NULL);
  assert(compressInfo->compressMode == COMPRESS_MODE_DEFLATE);
  assert(data != NULL);
  assert(deflatedBytess != NULL);

  (*deflatedBytess) = 0;
  do
  {
    /* check if data buffer is full, compress data buffer */
    if (compressInfo->dataBufferLength >= compressInfo->dataBufferSize)
    {
      error = compressData(compressInfo);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
//    assert(compressInfo->dataBufferLength < compressInfo->dataBufferSize);

    /* get available space in buffer */
    n = MIN(length,compressInfo->dataBufferSize-compressInfo->dataBufferLength);

    /* copy data to data buffer */
    memcpy(compressInfo->dataBuffer+compressInfo->dataBufferLength,data,n);
    compressInfo->dataBufferLength +=n;
    data += n;
    length -= n;

    (*deflatedBytess) += n;
  }
  while (   (n > 0)
         && (length > 0)
         && (compressInfo->compressBufferLength < compressInfo->compressBufferSize)
        );

  return ERROR_NONE;
}

Errors Compress_inflate(CompressInfo *compressInfo,
                        byte         *data,
                        ulong        length,
                        ulong        *inflatedBytes
                       )
{
  Errors error;
  ulong  n;

  assert(compressInfo != NULL);
  assert(compressInfo->compressMode == COMPRESS_MODE_INFLATE);
  assert(data != NULL);
  assert(inflatedBytes != NULL);

  (*inflatedBytes) = 0;
  do
  {
    /* check if buffer is empty, decompress data */
    if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)
    {
      error = decompressData(compressInfo);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
//    assert(compressInfo->dataBufferIndex < compressInfo->dataBufferLength);

    /* get number of bytes to read */
    n = MIN(length,compressInfo->dataBufferLength-compressInfo->dataBufferIndex);

    /* copy from data buffer */
    memcpy(data,compressInfo->dataBuffer+compressInfo->dataBufferIndex,n);
    compressInfo->dataBufferIndex += n;
    data += n;
    length -= n;

    (*inflatedBytes) += n;
  }
  while (   (n > 0)
         && (length > 0)
         && (compressInfo->compressBufferIndex < compressInfo->compressBufferLength)
        );

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
    case COMPRESS_ALGORITHM_LZMA_1:
    case COMPRESS_ALGORITHM_LZMA_2:
    case COMPRESS_ALGORITHM_LZMA_3:
    case COMPRESS_ALGORITHM_LZMA_4:
    case COMPRESS_ALGORITHM_LZMA_5:
    case COMPRESS_ALGORITHM_LZMA_6:
    case COMPRESS_ALGORITHM_LZMA_7:
    case COMPRESS_ALGORITHM_LZMA_8:
    case COMPRESS_ALGORITHM_LZMA_9:
      #ifdef HAVE_LZMA
        length = (uint64)compressInfo->lzmalib.stream.total_in;
      #else /* not HAVE_LZMA */
        length = 0LL;
      #endif /* HAVE_LZMA */
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
    case COMPRESS_ALGORITHM_LZMA_1:
    case COMPRESS_ALGORITHM_LZMA_2:
    case COMPRESS_ALGORITHM_LZMA_3:
    case COMPRESS_ALGORITHM_LZMA_4:
    case COMPRESS_ALGORITHM_LZMA_5:
    case COMPRESS_ALGORITHM_LZMA_6:
    case COMPRESS_ALGORITHM_LZMA_7:
    case COMPRESS_ALGORITHM_LZMA_8:
    case COMPRESS_ALGORITHM_LZMA_9:
      #ifdef HAVE_LZMA
        length = (uint64)compressInfo->lzmalib.stream.total_out;
      #else /* not HAVE_LZMA */
        length = 0LL;
      #endif /* HAVE_LZMA */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return length;
}

ulong Compress_getAvailableBytes(CompressInfo *compressInfo)
{
  Errors error;

  assert(compressInfo != NULL);

  switch (compressInfo->compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      error = compressData(compressInfo);
      if (error != ERROR_NONE) return 0;
      break;
    case COMPRESS_MODE_INFLATE:
      error = decompressData(compressInfo);
      if (error != ERROR_NONE) return 0;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  /* data is available iff bufferIndex < bufferLength */
  return compressInfo->dataBufferLength-compressInfo->dataBufferIndex;
}

uint Compress_getAvailableBlocks(CompressInfo *compressInfo, CompressBlockTypes blockType)
{
  Errors error;
  uint   blockCount;

  assert(compressInfo != NULL);

  switch (compressInfo->compressMode)
  {
    case COMPRESS_MODE_DEFLATE:
      error = compressData(compressInfo);
      if (error != ERROR_NONE) return 0;
      break;
    case COMPRESS_MODE_INFLATE:
      error = decompressData(compressInfo);
      if (error != ERROR_NONE) return 0;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  blockCount = 0;
  switch (blockType)
  {
    case COMPRESS_BLOCK_TYPE_ANY:
      /* block is available iff compressBufferLength >= 0 */
      blockCount = (uint)((compressInfo->compressBufferLength-compressInfo->compressBufferIndex+compressInfo->blockLength-1)/compressInfo->blockLength);
      break;
    case COMPRESS_BLOCK_TYPE_FULL:
      /* block is full iff compressBufferLength >= blockLength */
      blockCount = (uint)((compressInfo->compressBufferLength-compressInfo->compressBufferIndex)/compressInfo->blockLength);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return blockCount;
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
  assert(compressInfo->compressBufferLength <= compressInfo->compressBufferSize);
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
  assert(bufferLength <= compressInfo->compressBufferSize);

  memcpy(compressInfo->compressBuffer,buffer,bufferLength);
  compressInfo->compressBufferIndex  = 0;
  compressInfo->compressBufferLength = bufferLength;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
