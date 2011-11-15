/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/compress.c,v $
* $Revision: 1.5 $
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
#ifdef HAVE_XDELTA3
  #include "xdelta3.h"
#endif /* HAVE_XDELTA */
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "lists.h"
#include "files.h"

#include "errors.h"
#include "entrylists.h"
#include "patternlists.h"
#include "compress.h"
#include "crypt.h"
#include "storage.h"
#include "bar.h"

#include "compress.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

LOCAL const struct { const char *name; CompressAlgorithms compressAlgorithm; } COMPRESS_ALGORITHMS[] =
{
  { "none",    COMPRESS_ALGORITHM_NONE     },

  { "zip0",    COMPRESS_ALGORITHM_ZIP_0    },
  { "zip1",    COMPRESS_ALGORITHM_ZIP_1    },
  { "zip2",    COMPRESS_ALGORITHM_ZIP_2    },
  { "zip3",    COMPRESS_ALGORITHM_ZIP_3    },
  { "zip4",    COMPRESS_ALGORITHM_ZIP_4    },
  { "zip5",    COMPRESS_ALGORITHM_ZIP_5    },
  { "zip6",    COMPRESS_ALGORITHM_ZIP_6    },
  { "zip7",    COMPRESS_ALGORITHM_ZIP_7    },
  { "zip8",    COMPRESS_ALGORITHM_ZIP_8    },
  { "zip9",    COMPRESS_ALGORITHM_ZIP_9    },

  { "bzip1",   COMPRESS_ALGORITHM_BZIP2_1  },
  { "bzip2",   COMPRESS_ALGORITHM_BZIP2_2  },
  { "bzip3",   COMPRESS_ALGORITHM_BZIP2_3  },
  { "bzip4",   COMPRESS_ALGORITHM_BZIP2_4  },
  { "bzip5",   COMPRESS_ALGORITHM_BZIP2_5  },
  { "bzip6",   COMPRESS_ALGORITHM_BZIP2_6  },
  { "bzip7",   COMPRESS_ALGORITHM_BZIP2_7  },
  { "bzip8",   COMPRESS_ALGORITHM_BZIP2_8  },
  { "bzip9",   COMPRESS_ALGORITHM_BZIP2_9  },

  { "lzma1",   COMPRESS_ALGORITHM_LZMA_1   },
  { "lzma2",   COMPRESS_ALGORITHM_LZMA_2   },
  { "lzma3",   COMPRESS_ALGORITHM_LZMA_3   },
  { "lzma4",   COMPRESS_ALGORITHM_LZMA_4   },
  { "lzma5",   COMPRESS_ALGORITHM_LZMA_5   },
  { "lzma6",   COMPRESS_ALGORITHM_LZMA_6   },
  { "lzma7",   COMPRESS_ALGORITHM_LZMA_7   },
  { "lzma8",   COMPRESS_ALGORITHM_LZMA_8   },
  { "lzma9",   COMPRESS_ALGORITHM_LZMA_9   },

  { "xdelta1", COMPRESS_ALGORITHM_XDELTA_1 },
  { "xdelta2", COMPRESS_ALGORITHM_XDELTA_2 },
  { "xdelta3", COMPRESS_ALGORITHM_XDELTA_3 },
  { "xdelta4", COMPRESS_ALGORITHM_XDELTA_4 },
  { "xdelta5", COMPRESS_ALGORITHM_XDELTA_5 },
  { "xdelta6", COMPRESS_ALGORITHM_XDELTA_6 },
  { "xdelta7", COMPRESS_ALGORITHM_XDELTA_7 },
  { "xdelta8", COMPRESS_ALGORITHM_XDELTA_8 },
  { "xdelta9", COMPRESS_ALGORITHM_XDELTA_9 },
};

#ifdef HAVE_XDELTA3
 #define XDELTA_BUFFER_SIZE XD3_ALLOCSIZE
           //XD3_DEFAULT_WINSIZE;
#endif /* HAVE_XDELTA3 */

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

static int xr = 0;
static int xw = 0;
LOCAL Errors compressData(CompressInfo *compressInfo)
{
  ulong maxCompressBytes,maxDataBytes;
  ulong compressBytes,dataBytes;

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
        if (compressInfo->dataBufferLength > 0L)  // data available
        {
          /* get max. number of data and compressed bytes */
          maxDataBytes     = compressInfo->dataBufferLength-compressInfo->dataBufferIndex;
          maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;

          /* copy from data buffer -> compress buffer */
          compressBytes = MIN(maxDataBytes,maxCompressBytes);
          dataBytes     = compressBytes;
          memcpy(compressInfo->compressBuffer+compressInfo->compressBufferLength,
                 compressInfo->dataBuffer+compressInfo->dataBufferIndex,
                 compressBytes
                );

          /* update compress state, compress length */
          compressInfo->compressState = COMPRESS_STATE_RUNNING;
          compressInfo->compressBufferLength += compressBytes;

          /* shift data buffer */
          memmove(compressInfo->dataBuffer,
                  compressInfo->dataBuffer+compressInfo->dataBufferIndex+dataBytes,
                  compressInfo->dataBufferLength-compressInfo->dataBufferIndex-dataBytes
                 );
// ??? compressInfo->dataBufferIndex obsolete?
          compressInfo->dataBufferIndex = 0L;
          compressInfo->dataBufferLength -= dataBytes;

          /* store number of bytes "compressed" */
          compressInfo->none.length += compressBytes;
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
              /* get max. number of data and compressed bytes */
              maxDataBytes     = compressInfo->dataBufferLength-compressInfo->dataBufferIndex;
              maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;

              /* compress: transfer data buffer -> compress buffer */
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
              dataBytes     = maxDataBytes-compressInfo->zlib.stream.avail_in;
              compressBytes = maxCompressBytes-compressInfo->zlib.stream.avail_out;

              /* update compress state, compress length */
              compressInfo->compressState = COMPRESS_STATE_RUNNING;
              compressInfo->compressBufferLength += compressBytes;

              /* shift data buffer */
//              memmove(compressInfo->dataBuffer+compressInfo->dataBufferIndex,
              memmove(compressInfo->dataBuffer,
                      compressInfo->dataBuffer+compressInfo->dataBufferIndex+dataBytes,
                      compressInfo->dataBufferLength-compressInfo->dataBufferIndex-dataBytes
                     );
              compressInfo->dataBufferIndex = 0L;
              compressInfo->dataBufferLength -= dataBytes;
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
              /* get max. number of compressed bytes */
              maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;

              /* compress with flush: transfer to compress buffer */
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
              compressBytes = maxCompressBytes-compressInfo->zlib.stream.avail_out;

              /* update compress length */
              compressInfo->compressBufferLength += compressBytes;
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
              /* get max. number of data and compressed bytes */
              maxDataBytes     = compressInfo->dataBufferLength-compressInfo->dataBufferIndex;
              maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;

              /* compress: transfer data buffer -> compress buffer */
              compressInfo->bzlib.stream.next_in   = (char*)(compressInfo->dataBuffer+compressInfo->dataBufferIndex);
              compressInfo->bzlib.stream.avail_in  = maxDataBytes;
              compressInfo->bzlib.stream.next_out  = (char*)(compressInfo->compressBuffer+compressInfo->compressBufferLength);
              compressInfo->bzlib.stream.avail_out = maxCompressBytes;
              bzlibError = BZ2_bzCompress(&compressInfo->bzlib.stream,BZ_RUN);
              if (bzlibError != BZ_RUN_OK)
              {
                return ERROR_COMPRESS_ERROR;
              }
              dataBytes     = maxDataBytes-compressInfo->bzlib.stream.avail_in;
              compressBytes = maxCompressBytes-compressInfo->bzlib.stream.avail_out;

              /* update compress state, compress length */
              compressInfo->compressState = COMPRESS_STATE_RUNNING;
              compressInfo->compressBufferLength += compressBytes;

              /* shift data buffer */
//              memmove(compressInfo->dataBuffer+compressInfo->dataBufferIndex,
              memmove(compressInfo->dataBuffer,
                      compressInfo->dataBuffer+compressInfo->dataBufferIndex+dataBytes,
                      compressInfo->dataBufferLength-compressInfo->dataBufferIndex-dataBytes
                     );
              compressInfo->dataBufferIndex = 0L;
              compressInfo->dataBufferLength -= dataBytes;
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
              /* get max. number of compressed bytes */
              maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;

              /* compress with flush: transfer to compress buffer */
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
              compressBytes = maxCompressBytes-compressInfo->bzlib.stream.avail_out;

              /* update compress length */
              compressInfo->compressBufferLength += compressBytes;
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
              /* get max. number of data and compressed bytes */
              maxDataBytes     = compressInfo->dataBufferLength-compressInfo->dataBufferIndex;
              maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;

              /* compress: transfer data buffer -> compress buffer */
              compressInfo->lzmalib.stream.next_in   = (uint8_t*)(compressInfo->dataBuffer+compressInfo->dataBufferIndex);
              compressInfo->lzmalib.stream.avail_in  = maxDataBytes;
              compressInfo->lzmalib.stream.next_out  = (uint8_t*)(compressInfo->compressBuffer+compressInfo->compressBufferLength);
              compressInfo->lzmalib.stream.avail_out = maxCompressBytes;
              lzmaResult = lzma_code(&compressInfo->lzmalib.stream,LZMA_RUN);
              if (lzmaResult != LZMA_OK)
              {
                return ERROR(COMPRESS_ERROR,lzmaResult);
              }
              dataBytes     = maxDataBytes-compressInfo->lzmalib.stream.avail_in;
              compressBytes = maxCompressBytes-compressInfo->lzmalib.stream.avail_out;

              /* update compress state, compress length */
              compressInfo->compressState = COMPRESS_STATE_RUNNING;
              compressInfo->compressBufferLength += compressBytes;

              /* shift data buffer */
//              memmove(compressInfo->dataBuffer+compressInfo->dataBufferIndex,
              memmove(compressInfo->dataBuffer,
                      compressInfo->dataBuffer+compressInfo->dataBufferIndex+dataBytes,
                      compressInfo->dataBufferLength-compressInfo->dataBufferIndex-dataBytes
                     );
              compressInfo->dataBufferIndex = 0L;
              compressInfo->dataBufferLength -= dataBytes;
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
              /* get max. number of compressed bytes */
              maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;

              /* compress with flush: transfer to compress buffer */
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
              compressBytes = maxCompressBytes-compressInfo->lzmalib.stream.avail_out;

              /* update compress length */
              compressInfo->compressBufferLength += compressBytes;
            }
          }
        }
      #else /* not HAVE_LZMA */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_LZMA */
      break;
    case COMPRESS_ALGORITHM_XDELTA_1:
    case COMPRESS_ALGORITHM_XDELTA_2:
    case COMPRESS_ALGORITHM_XDELTA_3:
    case COMPRESS_ALGORITHM_XDELTA_4:
    case COMPRESS_ALGORITHM_XDELTA_5:
    case COMPRESS_ALGORITHM_XDELTA_6:
    case COMPRESS_ALGORITHM_XDELTA_7:
    case COMPRESS_ALGORITHM_XDELTA_8:
    case COMPRESS_ALGORITHM_XDELTA_9:
      #ifdef HAVE_XDELTA3
        {
          ulong  dataBytes;
          int    xdeltaResult;
          Errors error;
          ulong  bytesRead;
          byte   *outputBuffer;
          ulong  outputBufferSize;

          if (   (compressInfo->compressBufferLength < compressInfo->compressBufferSize)  // space in compress buffer
              && !compressInfo->endOfDataFlag                                             // not end-of-data
             )
          {
            /* compress available data */
            if      (compressInfo->dataBufferLength > 0)  // data available
            {
              /* get max. number of data and compressed bytes */
              maxDataBytes     = compressInfo->dataBufferLength-compressInfo->dataBufferIndex;
              maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;

              /* compress: transfer data buffer -> output buffer */
              dataBytes = 0L;
              do
              {
                xdeltaResult = xd3_encode_input(&compressInfo->xdelta.stream);
                switch (xdeltaResult)
                {
                  case XD3_INPUT:
                    if (dataBytes < maxDataBytes)
                    {
//??? dataBufferIndex is always 0?
                      compressInfo->xdelta.inputBuffer[0] = *(compressInfo->dataBuffer+compressInfo->dataBufferIndex+dataBytes);
                      xd3_avail_input(&compressInfo->xdelta.stream,
                                      compressInfo->xdelta.inputBuffer,
                                      1
                                     );
xr++;
//fprintf(stderr,"%s,%d: XD3_INPUT 1 - %lu %c\n",__FILE__,__LINE__,xr,*(compressInfo->dataBuffer+compressInfo->dataBufferIndex+dataBytes));
                      dataBytes++;
                    }
                    break;
                  case XD3_OUTPUT:
                    /* allocate/resize output buffer */
                    if (   (compressInfo->xdelta.outputBuffer == NULL)
                        || (compressInfo->xdelta.outputBufferLength+(ulong)compressInfo->xdelta.stream.avail_out > compressInfo->xdelta.outputBufferSize)
                       )
                    {
                      outputBufferSize = CEIL(compressInfo->xdelta.outputBufferLength+(ulong)compressInfo->xdelta.stream.avail_out,256);
                      outputBuffer = realloc(compressInfo->xdelta.outputBuffer,outputBufferSize);
                      if (outputBuffer == NULL)
                      {
                        HALT_INSUFFICIENT_MEMORY();
                      }
                      compressInfo->xdelta.outputBuffer     = outputBuffer;
                      compressInfo->xdelta.outputBufferSize = outputBufferSize;
                    }

                    /* copy data to output buffer */
                    memcpy(compressInfo->xdelta.outputBuffer+compressInfo->xdelta.outputBufferLength,
                           compressInfo->xdelta.stream.next_out,
                           compressInfo->xdelta.stream.avail_out
                          );
                    compressInfo->xdelta.outputBufferLength += (ulong)compressInfo->xdelta.stream.avail_out;
xw += compressInfo->xdelta.stream.avail_out;

                    /* done data */
                    xd3_consume_output(&compressInfo->xdelta.stream);
fprintf(stderr,"%s,%d: XD3_OUTPUT %d\n",__FILE__,__LINE__,xw);
//fprintf(stderr,"%s,%d: s=%ld l=%ld\n",__FILE__,__LINE__,compressInfo->xdelta.outputBufferSize,compressInfo->xdelta.outputBufferLength);
                    break;
                  case XD3_GETSRCBLK:
                    assert(compressInfo->xdelta.sourceGetEntryDataBlock != NULL);
                    error = compressInfo->xdelta.sourceGetEntryDataBlock(compressInfo->xdelta.sourceGetEntryDataBlockUserData,
                                                                         (void*)compressInfo->xdelta.source.curblk,
                                                                         (uint64)compressInfo->xdelta.source.blksize*compressInfo->xdelta.source.getblkno,
                                                                         compressInfo->xdelta.source.blksize,
                                                                         &bytesRead
                                                                        );
                    if (error != ERROR_NONE)
                    {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Errors_getText(error));
                      return error;
                    }
                    compressInfo->xdelta.source.onblk    = bytesRead;
                    compressInfo->xdelta.source.curblkno = compressInfo->xdelta.source.getblkno;
fprintf(stderr,"%s,%d: XD3_GETSRCBLK %d %d %d: %02x %02x %02x %02x %02x %02x\n",__FILE__,__LINE__,compressInfo->xdelta.source.onblk,compressInfo->xdelta.source.curblkno,compressInfo->xdelta.source.blksize,
compressInfo->xdelta.source.curblk[0],
compressInfo->xdelta.source.curblk[1],
compressInfo->xdelta.source.curblk[2],
compressInfo->xdelta.source.curblk[3],
compressInfo->xdelta.source.curblk[4],
compressInfo->xdelta.source.curblk[5]
);
                    break;
                  case XD3_GOTHEADER:
fprintf(stderr,"%s,%d: XD3_GOTHEADER\n",__FILE__,__LINE__);
                    break;
                  case XD3_WINSTART:
fprintf(stderr,"%s,%d: XD3_WINSTART\n",__FILE__,__LINE__);
                    break;
                  case XD3_WINFINISH:
fprintf(stderr,"%s,%d: XD3_WINFINISH\n",__FILE__,__LINE__);
                    break;
                  default:
                    return ERRORX(COMPRESS_ERROR,xdeltaResult,xd3_errstring(&compressInfo->xdelta.stream));
                    break;
                }
              }
              while (   (dataBytes < maxDataBytes)
                     && (xdeltaResult != XD3_OUTPUT)
                    );

              /* update compress state, copy compress data, update compress length */
              compressInfo->compressState = COMPRESS_STATE_RUNNING;
              if (compressInfo->xdelta.outputBufferLength > 0L)
              {
                /* copy from output buffer -> compress buffer */
                compressBytes = MIN(compressInfo->xdelta.outputBufferLength,maxCompressBytes);
                memcpy(compressInfo->compressBuffer+compressInfo->compressBufferLength,
                       compressInfo->xdelta.outputBuffer,
                       compressBytes
                      );
                compressInfo->compressBufferLength += compressBytes;
assert(compressInfo->compressBufferLength <= compressInfo->compressBufferSize);

                /* shift output buffer */
                memmove(compressInfo->xdelta.outputBuffer,
                        compressInfo->xdelta.outputBuffer+compressBytes,
                        compressInfo->xdelta.outputBufferLength-compressBytes
                       );
                compressInfo->xdelta.outputBufferLength -= compressBytes;
              }

              /* shift data buffer */
//              memmove(compressInfo->dataBuffer+compressInfo->dataBufferIndex,
              memmove(compressInfo->dataBuffer,
                      compressInfo->dataBuffer+compressInfo->dataBufferIndex+dataBytes,
                      compressInfo->dataBufferLength-compressInfo->dataBufferIndex-dataBytes
                     );
              compressInfo->dataBufferIndex = 0L;
              compressInfo->dataBufferLength -= dataBytes;
            }
          }

          if (   (compressInfo->compressBufferLength < compressInfo->compressBufferSize)  // space in compress buffer
              && !compressInfo->endOfDataFlag                                             // not end-of-data
             )
          {
            // finish compress, flush internal compress buffers
            if (   compressInfo->flushFlag                                  // flush data requested
                && (compressInfo->compressState == COMPRESS_STATE_RUNNING)  // compressor is running -> data available in internal buffers
               )
            {
//fprintf(stderr,"%s,%d: %lu %lu ----------------------\n",__FILE__,__LINE__,(long)compressInfo->xdelta.stream.total_in,(long)compressInfo->xdelta.stream.total_out);
              /* get max. number of data and compressed bytes */
              maxDataBytes     = compressInfo->dataBufferLength;
              maxCompressBytes = compressInfo->compressBufferSize-compressInfo->compressBufferLength;

              dataBytes = 0L;
              do
              {
                do
                {
                  xdeltaResult = xd3_encode_input(&compressInfo->xdelta.stream);
                  switch (xdeltaResult)
                  {
                    case XD3_INPUT:
                      if (dataBytes < maxDataBytes)
                      {
                        compressInfo->xdelta.inputBuffer[0] = *(compressInfo->dataBuffer+compressInfo->dataBufferIndex+dataBytes);
                        xd3_avail_input(&compressInfo->xdelta.stream,
                                        compressInfo->xdelta.inputBuffer,
                                        1
                                       );
xr++;
//fprintf(stderr,"%s,%d: XD3_INPUT 1 - %lu %c\n",__FILE__,__LINE__,xr,*(compressInfo->dataBuffer+compressInfo->dataBufferIndex+dataBytes));
                        dataBytes++;
                      }
                      else
                      {
fprintf(stderr,"%s, %d: --- FLUSH ---\n",__FILE__,__LINE__);
                        xd3_set_flags(&compressInfo->xdelta.stream,compressInfo->xdelta.stream.flags|XD3_FLUSH);
                        xd3_avail_input(&compressInfo->xdelta.stream,compressInfo->xdelta.inputBuffer,0);

                        compressInfo->xdelta.flushFlag = TRUE;
                      }
                      break;
                    case XD3_OUTPUT:
                      /* allocate/resize output buffer */
                      if (   (compressInfo->xdelta.outputBuffer == NULL)
                          || (compressInfo->xdelta.outputBufferLength+(ulong)compressInfo->xdelta.stream.avail_out > compressInfo->xdelta.outputBufferSize)
                         )
                      {
                        outputBufferSize = CEIL(compressInfo->xdelta.outputBufferLength+(ulong)compressInfo->xdelta.stream.avail_out,256);
                        outputBuffer = realloc(compressInfo->xdelta.outputBuffer,outputBufferSize);
                        if (outputBuffer == NULL)
                        {
                          HALT_INSUFFICIENT_MEMORY();
                        }
                        compressInfo->xdelta.outputBuffer     = outputBuffer;
                        compressInfo->xdelta.outputBufferSize = outputBufferSize;
                      }

                      /* copy data to output buffer */
                      memcpy(compressInfo->xdelta.outputBuffer,
                             compressInfo->xdelta.stream.next_out,
                             compressInfo->xdelta.stream.avail_out
                            );
                      compressInfo->xdelta.outputBufferLength += (ulong)compressInfo->xdelta.stream.avail_out;
  xw += compressInfo->xdelta.stream.avail_out;

                      /* done data */
                      xd3_consume_output(&compressInfo->xdelta.stream);
  fprintf(stderr,"%s,%d: XD3_OUTPUT %d\n",__FILE__,__LINE__,xw);
  //fprintf(stderr,"%s,%d: outputBufferSize=%ld outputBufferLength=%ld\n",__FILE__,__LINE__,compressInfo->xdelta.outputBufferSize,compressInfo->xdelta.outputBufferLength);
                      break;
                    case XD3_GETSRCBLK:
  fprintf(stderr,"%s,%d: XD3_GETSRCBLK %d\n",__FILE__,__LINE__,compressInfo->xdelta.source.getblkno);
                      assert(compressInfo->xdelta.sourceGetEntryDataBlock != NULL);
                      error = compressInfo->xdelta.sourceGetEntryDataBlock(compressInfo->xdelta.sourceGetEntryDataBlockUserData,
                                                                           (void*)compressInfo->xdelta.source.curblk,
                                                                           (uint64)compressInfo->xdelta.source.blksize*compressInfo->xdelta.source.getblkno,
                                                                           compressInfo->xdelta.source.blksize,
                                                                           &bytesRead
                                                                          );
                      if (error != ERROR_NONE)
                      {
  fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Errors_getText(error));
                        return error;
                      }
                      compressInfo->xdelta.source.onblk    = bytesRead;
                      compressInfo->xdelta.source.curblkno = compressInfo->xdelta.source.getblkno;
#if 0
  fprintf(stderr,"%s,%d: XD3_GETSRCBLK %d %d %d: %02x %02x %02x %02x %02x %02x\n",__FILE__,__LINE__,compressInfo->xdelta.source.onblk,compressInfo->xdelta.source.curblkno,compressInfo->xdelta.source.blksize,
  compressInfo->xdelta.source.curblk[0],
  compressInfo->xdelta.source.curblk[1],
  compressInfo->xdelta.source.curblk[2],
  compressInfo->xdelta.source.curblk[3],
  compressInfo->xdelta.source.curblk[4],
  compressInfo->xdelta.source.curblk[5]
  );
#endif /* 0 */
                      break;
                    case XD3_GOTHEADER:
//fprintf(stderr,"%s,%d: XD3_GOTHEADER\n",__FILE__,__LINE__);
                      break;
                    case XD3_WINSTART:
//fprintf(stderr,"%s,%d: XD3_WINSTART\n",__FILE__,__LINE__);
                      break;
                    case XD3_WINFINISH:
                      /* Note: WINFINSIH after flush mean: end of compression */
//fprintf(stderr,"%s,%d: XD3_WINFINISH\n",__FILE__,__LINE__);
                      if (compressInfo->xdelta.flushFlag) compressInfo->endOfDataFlag = TRUE;
                      break;
                    default:
                      return ERRORX(COMPRESS_ERROR,xdeltaResult,xd3_errstring(&compressInfo->xdelta.stream));
                      break;
                  }
                }
                while (   !compressInfo->endOfDataFlag
                       && (xdeltaResult != XD3_OUTPUT)
                      );

                /* copy compress data, update compress length */
                if ((maxCompressBytes > 0) && (compressInfo->xdelta.outputBufferLength > 0L))
                {
                  /* copy from output buffer -> compress buffer */
                  compressBytes = MIN(compressInfo->xdelta.outputBufferLength,maxCompressBytes);
                  memcpy(compressInfo->compressBuffer+compressInfo->compressBufferLength,
                         compressInfo->xdelta.outputBuffer,
                         compressBytes
                        );
                  compressInfo->compressBufferLength += compressBytes;
                  maxCompressBytes -= compressBytes;

                  /* shift output buffer */
                  memmove(compressInfo->xdelta.outputBuffer,
                          compressInfo->xdelta.outputBuffer+compressBytes,
                          compressInfo->xdelta.outputBufferLength-compressBytes
                         );
                  compressInfo->xdelta.outputBufferLength -= compressBytes;
                }
              }
              while (!compressInfo->endOfDataFlag);
            }
          }
        }
      #else /* not HAVE_XDELTA3 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_XDELTA3 */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  assert(compressInfo->compressBufferLength <= compressInfo->compressBufferSize);

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
  ulong compressBytes,dataBytes;

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
        compressInfo->dataBufferIndex  = 0L;
        compressInfo->dataBufferLength = 0L;

        if (compressInfo->compressBufferIndex < compressInfo->compressBufferLength)  // unprocessed compressed data available
        {
          /* get max. number of compressed and data bytes */
          maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
          maxDataBytes     = compressInfo->dataBufferSize-compressInfo->dataBufferIndex;

          /* copy from compress buffer -> data buffer */
          dataBytes     = MIN(maxCompressBytes,maxDataBytes);
          compressBytes = dataBytes;
          memcpy(compressInfo->dataBuffer+compressInfo->dataBufferIndex,
                 compressInfo->compressBuffer+compressInfo->compressBufferIndex,
                 dataBytes
                );

          /* update compress state, compress index, data length */
          compressInfo->compressState = COMPRESS_STATE_RUNNING;
          compressInfo->compressBufferIndex += compressBytes;
          compressInfo->dataBufferLength += dataBytes;

          /* store number of data bytes */
          compressInfo->none.length += dataBytes;
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

          if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)  // no data in data buffer
          {
            compressInfo->dataBufferIndex  = 0L;
            compressInfo->dataBufferLength = 0L;

            if (!compressInfo->endOfDataFlag)
            {
              /* decompress available data */
              if      (compressInfo->compressBufferIndex < compressInfo->compressBufferLength)  // unprocessed compressed data available
              {
                /* get max. number of compressed and data bytes */
                maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
                maxDataBytes     = compressInfo->dataBufferSize;

                /* decompress: transfer compress buffer -> data buffer */
                compressInfo->zlib.stream.next_in   = compressInfo->compressBuffer+compressInfo->compressBufferIndex;
                compressInfo->zlib.stream.avail_in  = maxCompressBytes;
                compressInfo->zlib.stream.next_out  = compressInfo->dataBuffer;
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
                compressBytes = maxCompressBytes-compressInfo->zlib.stream.avail_in;
                dataBytes     = maxDataBytes-compressInfo->zlib.stream.avail_out;

                /* update compress state, compress index, data length */
                compressInfo->compressState = COMPRESS_STATE_RUNNING;
                compressInfo->compressBufferIndex += compressBytes;
                compressInfo->dataBufferLength = dataBytes;
              }
            }
          }
          if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)  // no data in data buffer
          {
            compressInfo->dataBufferIndex  = 0L;
            compressInfo->dataBufferLength = 0L;

            if (!compressInfo->endOfDataFlag)
            {
              /* finish decompress, flush internal decompress buffers */
              if (   (compressInfo->flushFlag)                                // flush data requested
                  && (compressInfo->compressState == COMPRESS_STATE_RUNNING)  // compressor is running -> data available in internal buffers
                 )
              {
                /* get max. number of compressed and data bytes */
                maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
                maxDataBytes     = compressInfo->dataBufferSize;

                /* decompress with flush: transfer to data buffer */
                compressInfo->zlib.stream.next_in   = NULL;
                compressInfo->zlib.stream.avail_in  = 0;
                compressInfo->zlib.stream.next_out  = compressInfo->dataBuffer;
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
                dataBytes = maxDataBytes-compressInfo->zlib.stream.avail_out;

                /* update data length */
                compressInfo->dataBufferLength = dataBytes;
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

          if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)  // no data in data buffer
          {
            compressInfo->dataBufferIndex  = 0L;
            compressInfo->dataBufferLength = 0L;

            if (!compressInfo->endOfDataFlag)
            {
              /* decompress available data */
              if      (compressInfo->compressBufferIndex < compressInfo->compressBufferLength)  // unprocessed compressed data available
              {
                /* get max. number of compressed and data bytes */
                maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
                maxDataBytes     = compressInfo->dataBufferSize;

                /* decompress: transfer compress buffer -> data buffer */
                compressInfo->bzlib.stream.next_in   = (char*)(compressInfo->compressBuffer+compressInfo->compressBufferIndex);
                compressInfo->bzlib.stream.avail_in  = maxCompressBytes;
                compressInfo->bzlib.stream.next_out  = (char*)compressInfo->dataBuffer;
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
                compressBytes = maxCompressBytes-compressInfo->bzlib.stream.avail_in;
                dataBytes     = maxDataBytes-compressInfo->bzlib.stream.avail_out;

                /* update compress state, compress index, data length */
                compressInfo->compressState = COMPRESS_STATE_RUNNING;
                compressInfo->compressBufferIndex += compressBytes;
                compressInfo->dataBufferLength = dataBytes;
              }
            }
          }
          if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)  // no data in data buffer
          {
            compressInfo->dataBufferIndex  = 0L;
            compressInfo->dataBufferLength = 0L;

            if (!compressInfo->endOfDataFlag)
            {
              /* finish decompress, flush internal decompress buffers */
              if (   (compressInfo->flushFlag)                                // flush data requested
                  && (compressInfo->compressState == COMPRESS_STATE_RUNNING)  // compressor is running -> data available in internal buffers
                 )
              {
                /* get max. number of compressed and data bytes */
                maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
                maxDataBytes     = compressInfo->dataBufferSize;

                /* decompress with flush: transfer to data buffer */
                compressInfo->bzlib.stream.next_in   = NULL;
                compressInfo->bzlib.stream.avail_in  = 0;
                compressInfo->bzlib.stream.next_out  = (char*)compressInfo->dataBuffer;
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
                dataBytes = maxDataBytes-compressInfo->bzlib.stream.avail_out;

                /* update data length */
                compressInfo->dataBufferLength = dataBytes;
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

          if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)  // no data in data buffer
          {
            compressInfo->dataBufferIndex  = 0L;
            compressInfo->dataBufferLength = 0L;

            if (!compressInfo->endOfDataFlag)
            {
              /* decompress available data */
              if      (compressInfo->compressBufferIndex < compressInfo->compressBufferLength)  // unprocessed compressed data available
              {
                /* get max. number of compressed and data bytes */
                maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
                maxDataBytes     = compressInfo->dataBufferSize;

                /* decompress: transfer compress buffer -> data buffer */
                compressInfo->lzmalib.stream.next_in   = (uint8_t*)(compressInfo->compressBuffer+compressInfo->compressBufferIndex);
                compressInfo->lzmalib.stream.avail_in  = maxCompressBytes;
                compressInfo->lzmalib.stream.next_out  = (uint8_t*)compressInfo->dataBuffer;
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
                compressBytes = maxCompressBytes-compressInfo->lzmalib.stream.avail_in;
                dataBytes     = maxDataBytes-compressInfo->lzmalib.stream.avail_out;

                /* update compress state, compress index, data length */
                compressInfo->compressState = COMPRESS_STATE_RUNNING;
                compressInfo->compressBufferIndex += compressBytes;
                compressInfo->dataBufferLength = dataBytes;
              }
            }
          }
          if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)  // no data in data buffer
          {
            compressInfo->dataBufferIndex  = 0L;
            compressInfo->dataBufferLength = 0L;

            if (!compressInfo->endOfDataFlag)
            {
              /* finish decompress, flush internal decompress buffers */
              if (   (compressInfo->flushFlag)                                // flush data requested
                  && (compressInfo->compressState == COMPRESS_STATE_RUNNING)  // compressor is running -> data available in internal buffers
                 )
              {
                /* get max. number of compressed and data bytes */
                maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
                maxDataBytes     = compressInfo->dataBufferSize;

                /* decompress with flush: transfer to data buffer */
                compressInfo->lzmalib.stream.next_in   = NULL;
                compressInfo->lzmalib.stream.avail_in  = 0;
                compressInfo->lzmalib.stream.next_out  = (uint8_t*)compressInfo->dataBuffer;
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
                dataBytes = maxDataBytes-compressInfo->lzmalib.stream.avail_out;

                /* update data length */
                compressInfo->dataBufferLength = dataBytes;
              }
            }
          }
        }
      #else /* not HAVE_LZMA */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_LZMA */
      break;
    case COMPRESS_ALGORITHM_XDELTA_1:
    case COMPRESS_ALGORITHM_XDELTA_2:
    case COMPRESS_ALGORITHM_XDELTA_3:
    case COMPRESS_ALGORITHM_XDELTA_4:
    case COMPRESS_ALGORITHM_XDELTA_5:
    case COMPRESS_ALGORITHM_XDELTA_6:
    case COMPRESS_ALGORITHM_XDELTA_7:
    case COMPRESS_ALGORITHM_XDELTA_8:
    case COMPRESS_ALGORITHM_XDELTA_9:
      #ifdef HAVE_XDELTA3
        {
          ulong compressBytes;
          int   xdeltaResult;
          ulong outputBufferSize;
          byte  *outputBuffer;

          if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)  // no data in data buffer
          {
            compressInfo->dataBufferIndex  = 0L;
            compressInfo->dataBufferLength = 0L;

            if (!compressInfo->endOfDataFlag)
            {
              /* decompress available data */
              if      (compressInfo->compressBufferIndex < compressInfo->compressBufferLength)  // unprocessed compressed data available
              {
                /* get max. number of compressed and data bytes */
                maxCompressBytes = compressInfo->compressBufferLength-compressInfo->compressBufferIndex;
                maxDataBytes     = compressInfo->dataBufferSize;

                /* decompress: transfer compress buffer -> output buffer */
                compressBytes = 0L;
                do
                {
                  xdeltaResult = xd3_decode_input(&compressInfo->xdelta.stream);
                  switch (xdeltaResult)
                  {
                    case XD3_INPUT:
//fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
                      compressInfo->xdelta.inputBuffer[0] = *(compressInfo->compressBuffer+compressInfo->compressBufferIndex+compressBytes);
                      xd3_avail_input(&compressInfo->xdelta.stream,
                                      compressInfo->xdelta.inputBuffer,
                                      1
                                     );
                      compressBytes++;
                      break;
                    case XD3_OUTPUT:
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
                      /* allocate/resize output buffer */
                      if (   (compressInfo->xdelta.outputBuffer == NULL)
                          || (compressInfo->xdelta.outputBufferLength+(ulong)compressInfo->xdelta.stream.avail_out > compressInfo->xdelta.outputBufferSize)
                         )
                      {
                        outputBufferSize = CEIL(compressInfo->xdelta.outputBufferLength+(ulong)compressInfo->xdelta.stream.avail_out,256);
                        outputBuffer = realloc(compressInfo->xdelta.outputBuffer,outputBufferSize);
                        if (outputBuffer == NULL)
                        {
                          HALT_INSUFFICIENT_MEMORY();
                        }
                        compressInfo->xdelta.outputBuffer     = outputBuffer;
                        compressInfo->xdelta.outputBufferSize = outputBufferSize;
                      }

                      /* copy data to output buffer */
                      memcpy(compressInfo->xdelta.outputBuffer+compressInfo->xdelta.outputBufferLength,
                             compressInfo->xdelta.stream.next_out,
                             compressInfo->xdelta.stream.avail_out
                            );
                      compressInfo->xdelta.outputBufferLength += (ulong)compressInfo->xdelta.stream.avail_out;
                      xd3_consume_output(&compressInfo->xdelta.stream);

  fprintf(stderr,"%s,%d: s=%ld l=%ld\n",__FILE__,__LINE__,compressInfo->xdelta.outputBufferSize,compressInfo->xdelta.outputBufferLength);
                      break;
                    case XD3_GOTHEADER:
  fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
                      break;
                    case XD3_WINSTART:
  fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
                      break;
                    case XD3_WINFINISH:
  fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
                      break;
                    default:
                      return ERRORX(COMPRESS_ERROR,xdeltaResult,xd3_errstring(&compressInfo->xdelta.stream));
                      break;
                  }
                }
                while (   (compressBytes < maxCompressBytes)
                       && (xdeltaResult != XD3_OUTPUT)
                      );

                /* update compress state, compress index, data length */
                compressInfo->compressState = COMPRESS_STATE_RUNNING;
                compressInfo->compressBufferIndex += compressBytes;
                if (compressInfo->xdelta.outputBufferLength > 0L)
                {
                  /* copy from output buffer -> data buffer */
                  dataBytes = MIN(compressInfo->xdelta.outputBufferLength,maxDataBytes);
                  memcpy(compressInfo->dataBuffer,
                         compressInfo->xdelta.outputBuffer,
                         dataBytes
                        );
                  compressInfo->dataBufferLength = dataBytes;

                  /* shift output buffer */
                  memmove(compressInfo->xdelta.outputBuffer,
                          compressInfo->xdelta.outputBuffer+dataBytes,
                          compressInfo->xdelta.outputBufferLength-dataBytes
                         );
                  compressInfo->xdelta.outputBufferLength -= dataBytes;
                }
              }
            }
          }
          if (compressInfo->dataBufferIndex >= compressInfo->dataBufferLength)  // no data in buffer
          {
            compressInfo->dataBufferIndex  = 0L;
            compressInfo->dataBufferLength = 0L;

            if (!compressInfo->endOfDataFlag)
            {
              /* finish decompress, flush internal decompress buffers */
              if (   (compressInfo->compressBufferIndex >= compressInfo->compressBufferLength)  // all compressed data processed
                  && (compressInfo->flushFlag)                                                  // flush data requested
                  && (compressInfo->compressState == COMPRESS_STATE_RUNNING)                    // compressor is running -> data available in internal buffers
                 )
              {
                /* get max. number of data bytes */
                maxDataBytes = compressInfo->dataBufferSize-compressInfo->dataBufferLength;

                /* decompress with flush: transfer data buffer -> output buffer */
                xd3_set_flags(&compressInfo->xdelta.stream,compressInfo->xdelta.stream.flags|XD3_FLUSH);
                xd3_avail_input(&compressInfo->xdelta.stream,compressInfo->xdelta.inputBuffer,0);
                do
                {
                  xdeltaResult = xd3_decode_input(&compressInfo->xdelta.stream);
                  switch (xdeltaResult)
                  {
                    case XD3_INPUT:
//fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
                      xd3_avail_input(&compressInfo->xdelta.stream,compressInfo->xdelta.inputBuffer,0);
                      break;
                    case XD3_OUTPUT:
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
                      /* allocate/resize output buffer */
                      if (   (compressInfo->xdelta.outputBuffer == NULL)
                          || (compressInfo->xdelta.outputBufferLength+(ulong)compressInfo->xdelta.stream.avail_out > compressInfo->xdelta.outputBufferSize)
                         )
                      {
                        outputBufferSize = CEIL(compressInfo->xdelta.outputBufferLength+(ulong)compressInfo->xdelta.stream.avail_out,256);
                        outputBuffer = realloc(compressInfo->xdelta.outputBuffer,outputBufferSize);
                        if (outputBuffer == NULL)
                        {
                          HALT_INSUFFICIENT_MEMORY();
                        }
                        compressInfo->xdelta.outputBuffer     = outputBuffer;
                        compressInfo->xdelta.outputBufferSize = outputBufferSize;
                      }

                      /* copy data to output buffer */
                      memcpy(compressInfo->xdelta.outputBuffer+compressInfo->xdelta.outputBufferLength,
                             compressInfo->xdelta.stream.next_out,
                             compressInfo->xdelta.stream.avail_out
                            );
                      compressInfo->xdelta.outputBufferLength += (ulong)compressInfo->xdelta.stream.avail_out;
                      xd3_consume_output(&compressInfo->xdelta.stream);

  fprintf(stderr,"%s,%d: s=%ld l=%ld\n",__FILE__,__LINE__,compressInfo->xdelta.outputBufferSize,compressInfo->xdelta.outputBufferLength);
                      break;
                    case XD3_GOTHEADER:
  fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
                      break;
                    case XD3_WINSTART:
  fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
                      break;
                    case XD3_WINFINISH:
  fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
                      break;
                    default:
                      return ERRORX(COMPRESS_ERROR,xdeltaResult,xd3_errstring(&compressInfo->xdelta.stream));
                      break;
                  }
                }
                while (xdeltaResult != XD3_OUTPUT);

                /* update data length */
                if (compressInfo->xdelta.outputBufferLength > 0L)
                {
                  /* copy from output buffer -> data buffer */
                  dataBytes = MIN(compressInfo->xdelta.outputBufferLength,maxDataBytes);
                  memcpy(compressInfo->dataBuffer,
                         compressInfo->xdelta.outputBuffer,
                         dataBytes
                        );
                  compressInfo->dataBufferLength = dataBytes;

                  /* shift output buffer */
                  memmove(compressInfo->xdelta.outputBuffer,
                          compressInfo->xdelta.outputBuffer+dataBytes,
                          compressInfo->xdelta.outputBufferLength-dataBytes
                         );
                  compressInfo->xdelta.outputBufferLength -= dataBytes;
                }
                else
                {
                  /* no more data */
                  compressInfo->endOfDataFlag = TRUE;
                }
              }
            }
          }
        }
      #else /* not HAVE_XDELTA3 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_XDELTA3 */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return ERROR_NONE;
}

#ifdef HAVE_XDELTA
/***********************************************************************\
* Name   : xdeltaGetBlock
* Purpose:
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL int xdelta3GetBlock(xd3_stream *xd3Stream,
                          xd3_source *xd3Source,
                          xoff_t      blkno
                         )
{
  CompressInfo *compressInfo;
  Errors       error;
  ulong        bytesRead;

  assert(xd3Stream != NULL);
  assert(xd3Source != NULL);

  compressInfo = (CompressInfo*)xd3Source->ioh;

  #ifdef HAVE_XDELTA3
    assert(compressInfo->xdelta.sourceGetEntryDataBlock != NULL);
    error = compressInfo->xdelta.sourceGetEntryDataBlock(compressInfo->xdelta.sourceGetEntryDataBlockUserData,
                                                         (void*)xd3Source->curblk,
                                                         (uint64)xd3Source->blksize*blkno,
                                                         xd3Source->blksize,
                                                         &bytesRead
                                                        );
    if (error != ERROR_NONE)
    {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Errors_getText(error));
      return error;
    }
    xd3Source->onblk    = bytesRead;
    xd3Source->curblkno = blkno;
  #else /* not HAVE_XDELTA3 */
    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_XDELTA3 */

  return 0;
}
#endif /* HAVE_XDELTA */
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

Errors Compress_new(CompressInfo                    *compressInfo,
                    CompressModes                   compressMode,
                    CompressAlgorithms              compressAlgorithm,
                    ulong                           blockLength,
                    CompressSourceGetEntryDataBlock sourceGetEntryDataBlock,
                    void                            *sourceGetEntryDataBlockUserData
                   )
{
  assert(compressInfo != NULL);

  /* init variables */
  compressInfo->compressMode         = compressMode;
  compressInfo->compressAlgorithm    = compressAlgorithm;
  compressInfo->blockLength          = blockLength;
  compressInfo->compressState        = COMPRESS_STATE_INIT;
  compressInfo->endOfDataFlag        = FALSE;
  compressInfo->flushFlag            = FALSE;
  compressInfo->dataBufferIndex      = 0L;
  compressInfo->dataBufferLength     = 0L;
  compressInfo->dataBufferSize       = blockLength;
  compressInfo->compressBufferIndex  = 0L;
  compressInfo->compressBufferLength = 0L;
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
    case COMPRESS_ALGORITHM_XDELTA_1:
    case COMPRESS_ALGORITHM_XDELTA_2:
    case COMPRESS_ALGORITHM_XDELTA_3:
    case COMPRESS_ALGORITHM_XDELTA_4:
    case COMPRESS_ALGORITHM_XDELTA_5:
    case COMPRESS_ALGORITHM_XDELTA_6:
    case COMPRESS_ALGORITHM_XDELTA_7:
    case COMPRESS_ALGORITHM_XDELTA_8:
    case COMPRESS_ALGORITHM_XDELTA_9:
      #ifdef HAVE_XDELTA3
        {
          xd3_config xd3Config;

          // initialize variables
          compressInfo->xdelta.sourceGetEntryDataBlock         = sourceGetEntryDataBlock;
          compressInfo->xdelta.sourceGetEntryDataBlockUserData = sourceGetEntryDataBlockUserData;
          compressInfo->xdelta.outputBuffer                    = NULL;
          compressInfo->xdelta.outputBufferLength              = 0L;
          compressInfo->xdelta.outputBufferSize                = 0L;
          compressInfo->xdelta.flags                           = 0;
          compressInfo->xdelta.flushFlag                       = FALSE;

          // initialize xdelta flags
          switch (compressAlgorithm)
          {
            case COMPRESS_ALGORITHM_XDELTA_1: compressInfo->xdelta.flags |= XD3_COMPLEVEL_1; break;
            case COMPRESS_ALGORITHM_XDELTA_2: compressInfo->xdelta.flags |= XD3_COMPLEVEL_2; break;
            case COMPRESS_ALGORITHM_XDELTA_3: compressInfo->xdelta.flags |= XD3_COMPLEVEL_3; break;
            case COMPRESS_ALGORITHM_XDELTA_4: compressInfo->xdelta.flags |= XD3_COMPLEVEL_3; break;
            case COMPRESS_ALGORITHM_XDELTA_5: compressInfo->xdelta.flags |= XD3_COMPLEVEL_3; break;
            case COMPRESS_ALGORITHM_XDELTA_6: compressInfo->xdelta.flags |= XD3_COMPLEVEL_6; break;
            case COMPRESS_ALGORITHM_XDELTA_7: compressInfo->xdelta.flags |= XD3_COMPLEVEL_6; break;
            case COMPRESS_ALGORITHM_XDELTA_8: compressInfo->xdelta.flags |= XD3_COMPLEVEL_6; break;
            case COMPRESS_ALGORITHM_XDELTA_9: compressInfo->xdelta.flags |= XD3_COMPLEVEL_9; break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break;
          }
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

          // init xdelta configuration
          memset(&xd3Config,0,sizeof(xd3Config));
          xd3_init_config(&xd3Config,compressInfo->xdelta.flags);
//          xd3Config.getblk  = NULL; //xdelta3GetBlock;
          xd3Config.winsize = XDELTA_BUFFER_SIZE;

          // init xdelta stream
          memset(&compressInfo->xdelta.stream,0,sizeof(compressInfo->xdelta.stream));
          if (xd3_config_stream(&compressInfo->xdelta.stream,&xd3Config) != 0)
          {
            free(compressInfo->compressBuffer);
            free(compressInfo->dataBuffer);
            return ERROR_INIT_COMPRESS;
          }

          // init xdelta source
          compressInfo->xdelta.sourceBuffer = malloc(XDELTA_BUFFER_SIZE);
          if (compressInfo->xdelta.sourceBuffer == NULL)
          {
            xd3_free_stream(&compressInfo->xdelta.stream);
            free(compressInfo->compressBuffer);
            free(compressInfo->dataBuffer);
            return ERROR_INIT_COMPRESS;
          }
          memset(&compressInfo->xdelta.source,0,sizeof(compressInfo->xdelta.source));
//          compressInfo->xdelta.source.ioh      = compressInfo;
          compressInfo->xdelta.source.blksize  = XDELTA_BUFFER_SIZE;
          compressInfo->xdelta.source.onblk    = (usize_t)0;
          compressInfo->xdelta.source.curblkno = (xoff_t)(-1);
          compressInfo->xdelta.source.curblk   = compressInfo->xdelta.sourceBuffer;
          if (xd3_set_source(&compressInfo->xdelta.stream,&compressInfo->xdelta.source) != 0)
          {
            xd3_free_stream(&compressInfo->xdelta.stream);
            free(compressInfo->xdelta.sourceBuffer);
            free(compressInfo->compressBuffer);
            free(compressInfo->dataBuffer);
            return ERROR_INIT_COMPRESS;
          }
        }
      #else /* not HAVE_XDELTA3 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_XDELTA3 */
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
    case COMPRESS_ALGORITHM_XDELTA_1:
    case COMPRESS_ALGORITHM_XDELTA_2:
    case COMPRESS_ALGORITHM_XDELTA_3:
    case COMPRESS_ALGORITHM_XDELTA_4:
    case COMPRESS_ALGORITHM_XDELTA_5:
    case COMPRESS_ALGORITHM_XDELTA_6:
    case COMPRESS_ALGORITHM_XDELTA_7:
    case COMPRESS_ALGORITHM_XDELTA_8:
    case COMPRESS_ALGORITHM_XDELTA_9:
      #ifdef HAVE_XDELTA3
        xd3_close_stream(&compressInfo->xdelta.stream);
        xd3_free_stream(&compressInfo->xdelta.stream);
        if (compressInfo->xdelta.outputBuffer != NULL) free(compressInfo->xdelta.outputBuffer);
        free(compressInfo->xdelta.sourceBuffer);
      #else /* not HAVE_XDELTA3 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_XDELTA3 */
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
  compressInfo->dataBufferIndex      = 0L;
  compressInfo->dataBufferLength     = 0L;
  compressInfo->compressBufferIndex  = 0L;
  compressInfo->compressBufferLength = 0L;

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
    case COMPRESS_ALGORITHM_XDELTA_1:
    case COMPRESS_ALGORITHM_XDELTA_2:
    case COMPRESS_ALGORITHM_XDELTA_3:
    case COMPRESS_ALGORITHM_XDELTA_4:
    case COMPRESS_ALGORITHM_XDELTA_5:
    case COMPRESS_ALGORITHM_XDELTA_6:
    case COMPRESS_ALGORITHM_XDELTA_7:
    case COMPRESS_ALGORITHM_XDELTA_8:
    case COMPRESS_ALGORITHM_XDELTA_9:
      #ifdef HAVE_XDELTA3
        {
          xd3_config xd3Config;

          // close xdelta stream
          xd3_close_stream(&compressInfo->xdelta.stream);
          xd3_free_stream(&compressInfo->xdelta.stream);

          // re-initialize variables
          compressInfo->xdelta.outputBufferLength = 0L;
          compressInfo->xdelta.flushFlag          = FALSE;

          // re-init xdelta configuration
          memset(&xd3Config,0,sizeof(xd3Config));
          xd3_init_config(&xd3Config,compressInfo->xdelta.flags);
          xd3Config.getblk  = //xdelta3GetBlock;
          xd3Config.winsize = XDELTA_BUFFER_SIZE;

          // re-init xdelta stream
          if (xd3_config_stream(&compressInfo->xdelta.stream,&xd3Config) != 0)
          {
            return ERROR_COMPRESS_ERROR;
          }

          // re-init xdelta source
          compressInfo->xdelta.source.onblk    = (usize_t)0;
          compressInfo->xdelta.source.curblkno = (xoff_t)(-1);
          if (xd3_set_source(&compressInfo->xdelta.stream,&compressInfo->xdelta.source) != 0)
          {
            return ERROR_COMPRESS_ERROR;
          }
        }
      #else /* not HAVE_XDELTA3 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_XDELTA3 */
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
                        ulong        *deflatedBytes
                       )
{
  Errors error;
  ulong  n;

  assert(compressInfo != NULL);
  assert(compressInfo->compressMode == COMPRESS_MODE_DEFLATE);
  assert(data != NULL);

  if (deflatedBytes != NULL) (*deflatedBytes) = 0L;
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

    /* get available space in buffer */
    n = MIN(length,compressInfo->dataBufferSize-compressInfo->dataBufferLength);

    /* copy new data to data buffer */
    memcpy(compressInfo->dataBuffer+compressInfo->dataBufferLength,data,n);
    compressInfo->dataBufferLength +=n;
    data += n;
    length -= n;

    if (deflatedBytes != NULL) (*deflatedBytes) += n;
  }
  while (   (n > 0L)
         && (length > 0L)
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

  if (inflatedBytes != NULL) (*inflatedBytes) = 0L;
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

    /* get number of bytes to read */
    n = MIN(length,compressInfo->dataBufferLength-compressInfo->dataBufferIndex);

    /* copy from data buffer */
    memcpy(data,compressInfo->dataBuffer+compressInfo->dataBufferIndex,n);
    compressInfo->dataBufferIndex += n;
    data += n;
    length -= n;

    if (inflatedBytes != NULL) (*inflatedBytes) += n;
  }
  while (   (n > 0L)
         && (length > 0L)
         && (compressInfo->compressBufferIndex < compressInfo->compressBufferLength)
        );

  return ERROR_NONE;
}

Errors Compress_flush(CompressInfo *compressInfo)
{
  Errors error;

  assert(compressInfo != NULL);

  // mark compress flush
  compressInfo->flushFlag = TRUE;

  // try to compress/decompress data
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

  return error;
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
    case COMPRESS_ALGORITHM_XDELTA_1:
    case COMPRESS_ALGORITHM_XDELTA_2:
    case COMPRESS_ALGORITHM_XDELTA_3:
    case COMPRESS_ALGORITHM_XDELTA_4:
    case COMPRESS_ALGORITHM_XDELTA_5:
    case COMPRESS_ALGORITHM_XDELTA_6:
    case COMPRESS_ALGORITHM_XDELTA_7:
    case COMPRESS_ALGORITHM_XDELTA_8:
    case COMPRESS_ALGORITHM_XDELTA_9:
      #ifdef HAVE_XDELTA3
        length = (uint64)compressInfo->xdelta.stream.total_in;
      #else /* not HAVE_XDELTA3 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_XDELTA3 */
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
    case COMPRESS_ALGORITHM_XDELTA_1:
    case COMPRESS_ALGORITHM_XDELTA_2:
    case COMPRESS_ALGORITHM_XDELTA_3:
    case COMPRESS_ALGORITHM_XDELTA_4:
    case COMPRESS_ALGORITHM_XDELTA_5:
    case COMPRESS_ALGORITHM_XDELTA_6:
    case COMPRESS_ALGORITHM_XDELTA_7:
    case COMPRESS_ALGORITHM_XDELTA_8:
    case COMPRESS_ALGORITHM_XDELTA_9:
      #ifdef HAVE_XDELTA3
        length = (uint64)compressInfo->xdelta.stream.total_out;
      #else /* not HAVE_XDELTA3 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_XDELTA3 */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return length;
}

Errors Compress_getAvailableDecompressedBytes(CompressInfo *compressInfo,
                                              ulong        *bytes
                                             )
{
  Errors error;

  assert(compressInfo != NULL);
  assert(bytes != NULL);

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

  /* data is available iff bufferIndex < bufferLength */
  (*bytes) = compressInfo->dataBufferLength-compressInfo->dataBufferIndex;

  return ERROR_NONE;
}

Errors Compress_getAvailableCompressedBlocks(CompressInfo       *compressInfo,
                                             CompressBlockTypes blockType,
                                             uint               *blockCount
                                            )
{
  Errors error;

  assert(compressInfo != NULL);
  assert(blockCount != NULL);

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

  (*blockCount) = 0;
  switch (blockType)
  {
    case COMPRESS_BLOCK_TYPE_ANY:
      /* block is available iff compressBufferLength >= 0 */
      (*blockCount) = (uint)((compressInfo->compressBufferLength-compressInfo->compressBufferIndex+compressInfo->blockLength-1)/compressInfo->blockLength);
      break;
    case COMPRESS_BLOCK_TYPE_FULL:
      /* block is full iff compressBufferLength >= blockLength */
      (*blockCount) = (uint)((compressInfo->compressBufferLength-compressInfo->compressBufferIndex)/compressInfo->blockLength);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

bool Compress_getByte(CompressInfo *compressInfo,
                      byte         *buffer
                     )
{
  assert(compressInfo != NULL);
  assert(compressInfo->compressBuffer != NULL);
// ??? blocklength
  assert(compressInfo->compressBufferLength <= compressInfo->compressBufferSize);
  assert(buffer != NULL);

  if (compressInfo->compressBufferLength > compressInfo->compressBufferIndex)
  {
    (*buffer) = (*(compressInfo->compressBuffer+compressInfo->compressBufferIndex));
    compressInfo->compressBufferIndex++;

    if (compressInfo->compressBufferIndex >= compressInfo->compressBufferLength)
    {
      compressInfo->compressBufferIndex  = 0L;
      compressInfo->compressBufferLength = 0L;
    }

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

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

  memcpy(buffer,
         compressInfo->compressBuffer+compressInfo->compressBufferIndex,
         compressInfo->compressBufferLength-compressInfo->compressBufferIndex
        );
  memset(buffer+compressInfo->compressBufferLength,
         0,
         compressInfo->blockLength-compressInfo->compressBufferIndex
        );
  (*bufferLength) = compressInfo->compressBufferLength;

  compressInfo->compressBufferIndex  = 0L;
  compressInfo->compressBufferLength = 0L;
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
  compressInfo->compressBufferIndex  = 0L;
  compressInfo->compressBufferLength = bufferLength;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
