/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver compress functions
* Systems: all
*
\***********************************************************************/

#define __COMPRESS_IMPLEMENATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

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

// size of compress buffers
#define MAX_BUFFER_SIZE (4*1024)

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
  ulong compressBytes;

  assert(compressInfo != NULL);

  // compress if possible
  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      // Note: compress with identity compressor

      if (!compressInfo->endOfDataFlag)                                               // not end-of-data
      {
        if (!RingBuffer_isFull(&compressInfo->compressRingBuffer))                    // space in compress buffer
        {
          if (!RingBuffer_isEmpty(&compressInfo->dataRingBuffer))                     // data available
          {
            // get max. number of data and max. number of "compressed" bytes
            maxDataBytes     = RingBuffer_getAvailable(&compressInfo->dataRingBuffer);
            maxCompressBytes = RingBuffer_getFree(&compressInfo->compressRingBuffer);

            // copy from data buffer -> compress buffer
            compressBytes = MIN(maxDataBytes,maxCompressBytes);
            RingBuffer_move(&compressInfo->dataRingBuffer,
                            &compressInfo->compressRingBuffer,
                            compressBytes
                           );

            // update compress state, compress length
            compressInfo->compressState = COMPRESS_STATE_RUNNING;

            // store number of bytes "compressed"
            compressInfo->none.totalBytes += compressBytes;
          }
        }

        if (RingBuffer_isEmpty(&compressInfo->compressRingBuffer))                    // no data in "compress" buffer
        {
          // finish "compress"
          if (   compressInfo->flushFlag                                              // flush data requested
              && (compressInfo->compressState == COMPRESS_STATE_RUNNING)              // compressor is running -> data available in internal buffers
             )
          {
            compressInfo->endOfDataFlag = TRUE;
          }
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
      // compress with zlib
      #ifdef HAVE_Z
        {
          int zlibError;

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
                compressInfo->zlib.stream.next_in   = (Bytef*)RingBuffer_cArrayOut(&compressInfo->dataRingBuffer);
                compressInfo->zlib.stream.avail_in  = maxDataBytes;
                compressInfo->zlib.stream.next_out  = (Bytef*)RingBuffer_cArrayIn(&compressInfo->compressRingBuffer);
                compressInfo->zlib.stream.avail_out = maxCompressBytes;
                zlibError = deflate(&compressInfo->zlib.stream,Z_NO_FLUSH);
                if (    (zlibError != Z_OK)
                     && (zlibError != Z_BUF_ERROR)
                   )
                {
                  return ERROR_(DEFLATE_FAIL,zlibError);
                }
                RingBuffer_decrement(&compressInfo->dataRingBuffer,
                                     maxDataBytes-compressInfo->zlib.stream.avail_in
                                    );
                RingBuffer_increment(&compressInfo->compressRingBuffer,
                                     maxCompressBytes-compressInfo->zlib.stream.avail_out
                                    );

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
                compressInfo->zlib.stream.next_in   = NULL;
                compressInfo->zlib.stream.avail_in  = 0;
                compressInfo->zlib.stream.next_out  = (Bytef*)RingBuffer_cArrayIn(&compressInfo->compressRingBuffer);
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
                  return ERROR_(DEFLATE_FAIL,zlibError);
                }
                RingBuffer_increment(&compressInfo->compressRingBuffer,
                                     maxCompressBytes-compressInfo->zlib.stream.avail_out
                                    );
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
      // compress with bzip2
      #ifdef HAVE_BZ2
        {
          int bzlibError;

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

                // compress: transfer data buffer -> compress buffer
                compressInfo->bzlib.stream.next_in   = (char*)RingBuffer_cArrayOut(&compressInfo->dataRingBuffer);
                compressInfo->bzlib.stream.avail_in  = maxDataBytes;
                compressInfo->bzlib.stream.next_out  = (char*)RingBuffer_cArrayIn(&compressInfo->compressRingBuffer);
                compressInfo->bzlib.stream.avail_out = maxCompressBytes;
                bzlibError = BZ2_bzCompress(&compressInfo->bzlib.stream,BZ_RUN);
                if (bzlibError != BZ_RUN_OK)
                {
                  return ERROR_(DEFLATE_FAIL,bzlibError);
                }
                RingBuffer_decrement(&compressInfo->dataRingBuffer,
                                     maxDataBytes-compressInfo->bzlib.stream.avail_in
                                    );
                RingBuffer_increment(&compressInfo->compressRingBuffer,
                                     maxCompressBytes-compressInfo->bzlib.stream.avail_out
                                    );

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
                compressInfo->bzlib.stream.next_in   = NULL;
                compressInfo->bzlib.stream.avail_in  = 0;
                compressInfo->bzlib.stream.next_out  = (char*)RingBuffer_cArrayIn(&compressInfo->compressRingBuffer);
                compressInfo->bzlib.stream.avail_out = maxCompressBytes;
                bzlibError = BZ2_bzCompress(&compressInfo->bzlib.stream,BZ_FINISH);
                if      (bzlibError == BZ_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (bzlibError != BZ_FINISH_OK)
                {
                  return ERROR_(DEFLATE_FAIL,bzlibError);
                }
                RingBuffer_increment(&compressInfo->compressRingBuffer,
                                     maxCompressBytes-compressInfo->bzlib.stream.avail_out
                                    );
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
      // compress with lzma
      #ifdef HAVE_LZMA
        {
          lzma_ret lzmaResult;

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

                // compress: transfer data buffer -> compress buffer
                compressInfo->lzmalib.stream.next_in   = (uint8_t*)RingBuffer_cArrayOut(&compressInfo->dataRingBuffer);
                compressInfo->lzmalib.stream.avail_in  = maxDataBytes;
                compressInfo->lzmalib.stream.next_out  = (uint8_t*)RingBuffer_cArrayIn(&compressInfo->compressRingBuffer);
                compressInfo->lzmalib.stream.avail_out = maxCompressBytes;
                lzmaResult = lzma_code(&compressInfo->lzmalib.stream,LZMA_RUN);
                if (lzmaResult != LZMA_OK)
                {
                  return ERROR_(DEFLATE_FAIL,lzmaResult);
                }
                RingBuffer_decrement(&compressInfo->dataRingBuffer,
                                     maxDataBytes-compressInfo->lzmalib.stream.avail_in
                                    );
                RingBuffer_increment(&compressInfo->compressRingBuffer,
                                     maxCompressBytes-compressInfo->lzmalib.stream.avail_out
                                    );

                // update compress state, compress length
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
                compressInfo->lzmalib.stream.next_in   = NULL;
                compressInfo->lzmalib.stream.avail_in  = 0;
                compressInfo->lzmalib.stream.next_out  = (uint8_t*)RingBuffer_cArrayIn(&compressInfo->compressRingBuffer);
                compressInfo->lzmalib.stream.avail_out = maxCompressBytes;
                lzmaResult = lzma_code(&compressInfo->lzmalib.stream,LZMA_FINISH);
                if      (lzmaResult == LZMA_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (lzmaResult != LZMA_OK)
                {
                  return ERROR_(DEFLATE_FAIL,lzmaResult);
                }
                RingBuffer_increment(&compressInfo->compressRingBuffer,
                                     maxCompressBytes-compressInfo->lzmalib.stream.avail_out
                                    );
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
      // compress with xdelta
      #ifdef HAVE_XDELTA3
        {
          ulong  dataBytes;
          bool   doneFlag;
          int    xdeltaResult;
          ulong  n;
          Errors error;
          ulong  requiredBytes;
          ulong  bytesRead;
//static int xr=0;
//static int xw=0;

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

                // compress: transfer data buffer -> output buffer
                dataBytes = 0L;
                doneFlag  = FALSE;
                do
                {
                  xdeltaResult = xd3_encode_input(&compressInfo->xdelta.stream);
                  switch (xdeltaResult)
                  {
                    case XD3_INPUT:
                      if (dataBytes < maxDataBytes)
                      {
                        n = MIN(maxDataBytes-dataBytes,sizeof(compressInfo->xdelta.inputBuffer));
                        assert(n > 0);
                        RingBuffer_get(&compressInfo->dataRingBuffer,
                                       compressInfo->xdelta.inputBuffer,
                                       n
                                      );
#if 0
{
int h = open("xd3.data",O_CREAT|O_WRONLY|O_APPEND,0664);
write(h,compressInfo->xdelta.inputBuffer,n);
close(h);
}
#endif /* 0 */
                        xd3_avail_input(&compressInfo->xdelta.stream,
                                        compressInfo->xdelta.inputBuffer,
                                        (usize_t)n
                                       );
//xw+=n;
//fprintf(stderr,"XD3_INPUT 1 - n=%d xw=%d compressInfo->xdelta.stream.total_in=%d\n",n,xw,compressInfo->xdelta.stream.total_in);
//dumpMemory(FALSE,compressInfo->xdelta.inputBuffer,n);
                        dataBytes += n;
                      }
                      else
                      {
                        xd3_avail_input(&compressInfo->xdelta.stream,
                                        compressInfo->xdelta.inputBuffer,
                                        0
                                       );

                        doneFlag = TRUE;
                      }
                      break;
                    case XD3_OUTPUT:
                      // increase output buffer size if required
                      if (RingBuffer_getFree(&compressInfo->xdelta.outputRingBuffer) < (ulong)compressInfo->xdelta.stream.avail_out)
                      {
                        requiredBytes = RingBuffer_getSize(&compressInfo->xdelta.outputRingBuffer)+
                                        (ulong)compressInfo->xdelta.stream.avail_out-RingBuffer_getFree(&compressInfo->xdelta.outputRingBuffer);
                        if (!RingBuffer_resize(&compressInfo->xdelta.outputRingBuffer,requiredBytes))
                        {
                          HALT_INSUFFICIENT_MEMORY();
                        }
                      }

                      // append compressed data to output buffer
                      RingBuffer_put(&compressInfo->xdelta.outputRingBuffer,
                                     compressInfo->xdelta.stream.next_out,
                                     compressInfo->xdelta.stream.avail_out
                                    );

                      // done compressed data
                      xd3_consume_output(&compressInfo->xdelta.stream);

                      doneFlag = TRUE;
                      break;
                    case XD3_GETSRCBLK:
                      error = Source_getEntryDataBlock(compressInfo->xdelta.sourceHandle,
                                                       (void*)compressInfo->xdelta.source.curblk,
                                                       (uint64)compressInfo->xdelta.source.blksize*compressInfo->xdelta.source.getblkno,
                                                       compressInfo->xdelta.source.blksize,
                                                       &bytesRead
                                                      );
                      if (error != ERROR_NONE)
                      {
                        return error;
                      }
                      compressInfo->xdelta.source.onblk    = bytesRead;
                      compressInfo->xdelta.source.curblkno = compressInfo->xdelta.source.getblkno;
                      break;
                    case XD3_GOTHEADER:
//fprintf(stderr,"%s,%d: XD3_GOTHEADER\n",__FILE__,__LINE__);
                      break;
                    case XD3_WINSTART:
//fprintf(stderr,"%s,%d: XD3_WINSTART\n",__FILE__,__LINE__);
                      break;
                    case XD3_WINFINISH:
//fprintf(stderr,"%s,%d: XD3_WINFINISH\n",__FILE__,__LINE__);
                      break;
                    default:
                      return ERRORX_(DEFLATE_FAIL,xdeltaResult,xd3_errstring(&compressInfo->xdelta.stream));
                      break;
                  }
                }
                while (!doneFlag);

                // get compressed data from output buffer
                compressBytes = MIN(RingBuffer_getAvailable(&compressInfo->xdelta.outputRingBuffer),
                                    maxCompressBytes
                                   );
                if (compressBytes > 0L)
                {
                  // copy from output buffer -> compress buffer
                  RingBuffer_move(&compressInfo->xdelta.outputRingBuffer,
                                  &compressInfo->compressRingBuffer,
                                  compressBytes
                                 );
                }

                // update compress state
                compressInfo->compressState = COMPRESS_STATE_RUNNING;
              }
            }
          }

          if (!compressInfo->endOfDataFlag)                                           // not end-of-data
          {
            if (!RingBuffer_isFull(&compressInfo->compressRingBuffer))                // space in compress buffer
            {
//fprintf(stderr,"%s, %d: compressInfo->flushFlag=%d (compressInfo->compressState=%d\n",__FILE__,__LINE__,compressInfo->flushFlag,compressInfo->compressState);
              // finish compress, flush internal compress buffers
              if (   compressInfo->flushFlag                                          // flush data requested
                  && (compressInfo->compressState == COMPRESS_STATE_RUNNING)          // compressor is running -> data available in internal buffers
                 )
              {
//fprintf(stderr,"%s,%d: %lu %lu ----------------------\n",__FILE__,__LINE__,(long)compressInfo->xdelta.stream.total_in,(long)compressInfo->xdelta.stream.total_out);
                // get max. number of data bytes and max. number of compressed bytes
                maxDataBytes     = RingBuffer_getAvailable(&compressInfo->dataRingBuffer);
                maxCompressBytes = RingBuffer_getFree(&compressInfo->compressRingBuffer);

                // compress: transfer data buffer -> output buffer
                dataBytes = 0L;
                do
                {
                  xdeltaResult = xd3_encode_input(&compressInfo->xdelta.stream);
                  switch (xdeltaResult)
                  {
                    case XD3_INPUT:
                      if (dataBytes < maxDataBytes)
                      {
                        n = MIN(maxDataBytes-dataBytes,sizeof(compressInfo->xdelta.inputBuffer));
                        assert(n > 0);
//??? dataBufferIndex is always 0?
                        RingBuffer_get(&compressInfo->dataRingBuffer,
                                       compressInfo->xdelta.inputBuffer,
                                       n
                                      );
                        xd3_avail_input(&compressInfo->xdelta.stream,
                                        compressInfo->xdelta.inputBuffer,
                                        (usize_t)n
                                       );
//xw+=n;
//fprintf(stderr,"XD3_INPUT 1 - n=%d xw=%d compressInfo->xdelta.stream.total_in=%d\n",n,xw,compressInfo->xdelta.stream.total_in);
//dumpMemory(FALSE,compressInfo->xdelta.inputBuffer,n);
                        dataBytes += n;
                      }
                      else if (!compressInfo->xdelta.flushFlag)
                      {
                        xd3_set_flags(&compressInfo->xdelta.stream,compressInfo->xdelta.stream.flags|XD3_FLUSH);
                        xd3_avail_input(&compressInfo->xdelta.stream,compressInfo->xdelta.inputBuffer,0);

                        compressInfo->xdelta.flushFlag = TRUE;
                      }
                      else
                      {
                        compressInfo->endOfDataFlag = TRUE;
                      }
                      break;
                    case XD3_OUTPUT:
                      // increase output buffer size if required
                      if (RingBuffer_getFree(&compressInfo->xdelta.outputRingBuffer) < (ulong)compressInfo->xdelta.stream.avail_out)
                      {
                        requiredBytes = RingBuffer_getSize(&compressInfo->xdelta.outputRingBuffer)+
                                        (ulong)compressInfo->xdelta.stream.avail_out-RingBuffer_getFree(&compressInfo->xdelta.outputRingBuffer);
                        if (!RingBuffer_resize(&compressInfo->xdelta.outputRingBuffer,requiredBytes))
                        {
                          HALT_INSUFFICIENT_MEMORY();
                        }
                      }

                      // append compressed data to output buffer
                      RingBuffer_put(&compressInfo->xdelta.outputRingBuffer,
                                     compressInfo->xdelta.stream.next_out,
                                     compressInfo->xdelta.stream.avail_out
                                    );

                      // done compressed data
                      xd3_consume_output(&compressInfo->xdelta.stream);
//fprintf(stderr,"%s,%d: outputBufferSize=%ld outputBufferLength=%ld\n",__FILE__,__LINE__,compressInfo->xdelta.outputBufferSize,compressInfo->xdelta.outputBufferLength);
                      break;
                    case XD3_GETSRCBLK:
                      error = Source_getEntryDataBlock(compressInfo->xdelta.sourceHandle,
                                                       (void*)compressInfo->xdelta.source.curblk,
                                                       (uint64)compressInfo->xdelta.source.blksize*compressInfo->xdelta.source.getblkno,
                                                       compressInfo->xdelta.source.blksize,
                                                       &bytesRead
                                                      );
                      if (error != ERROR_NONE)
                      {
                        return error;
                      }
                      compressInfo->xdelta.source.onblk    = bytesRead;
                      compressInfo->xdelta.source.curblkno = compressInfo->xdelta.source.getblkno;
                      break;
                    case XD3_GOTHEADER:
//fprintf(stderr,"%s,%d: XD3_GOTHEADER\n",__FILE__,__LINE__);
                      break;
                    case XD3_WINSTART:
//fprintf(stderr,"%s,%d: XD3_WINSTART\n",__FILE__,__LINE__);
                      break;
                    case XD3_WINFINISH:
//fprintf(stderr,"%s,%d: XD3_WINFINISH\n",__FILE__,__LINE__);
                      break;
                    default:
                      return ERRORX_(DEFLATE_FAIL,xdeltaResult,xd3_errstring(&compressInfo->xdelta.stream));
                      break;
                  }
                }
                while (   !compressInfo->endOfDataFlag
                       && (xdeltaResult != XD3_OUTPUT)
                      );

                // get compressed data from output buffer
                compressBytes = MIN(RingBuffer_getAvailable(&compressInfo->xdelta.outputRingBuffer),
                                    maxCompressBytes
                                   );
                if (compressBytes > 0L)
                {
                  // copy from output buffer -> compress buffer
                  RingBuffer_move(&compressInfo->xdelta.outputRingBuffer,
                                  &compressInfo->compressRingBuffer,
                                  compressBytes
                                 );
                }
              }
            }
          }

          // copy from output buffer -> compress buffer
          compressBytes = MIN(RingBuffer_getAvailable(&compressInfo->xdelta.outputRingBuffer),
                              RingBuffer_getFree(&compressInfo->compressRingBuffer)
                             );
          if (compressBytes > 0L)
          {
            RingBuffer_move(&compressInfo->xdelta.outputRingBuffer,
                            &compressInfo->compressRingBuffer,
                            compressBytes
                           );
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
  ulong dataBytes;

  assert(compressInfo != NULL);

  // decompress if possible
  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      // Note: decompress with identity compressor

      if (!compressInfo->endOfDataFlag)                                               // not end-of-data
      {
        if (!RingBuffer_isFull(&compressInfo->dataRingBuffer))                        // space in data buffer
        {
          // "decompress" available data
          if (!RingBuffer_isEmpty(&compressInfo->compressRingBuffer))                 // unprocessed "compressed" data available
          {
            // get max. number of "compressed" bytes and max. number of data bytes
            maxCompressBytes = RingBuffer_getAvailable(&compressInfo->compressRingBuffer);
            maxDataBytes     = RingBuffer_getFree(&compressInfo->dataRingBuffer);

            // copy from compress buffer -> data buffer
            dataBytes = MIN(maxCompressBytes,maxDataBytes);
            RingBuffer_move(&compressInfo->compressRingBuffer,
                            &compressInfo->dataRingBuffer,
                            dataBytes
                           );

            // update compress state, compress index, data length
            compressInfo->compressState = COMPRESS_STATE_RUNNING;

            // store number of "decompressed" data bytes
            compressInfo->none.totalBytes += dataBytes;
          }
        }

        if (RingBuffer_isEmpty(&compressInfo->dataRingBuffer))                        // no data in data buffer
        {
          // finish "decompress"
          if (   compressInfo->flushFlag                                              // flush data requested
              && (compressInfo->compressState == COMPRESS_STATE_RUNNING)              // compressor is running -> data available in internal buffers
             )
          {
            compressInfo->endOfDataFlag = TRUE;
          }
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
      // decompress with zlib
      #ifdef HAVE_Z
        {
          int zlibResult;

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
                compressInfo->zlib.stream.next_in   = (Bytef*)RingBuffer_cArrayOut(&compressInfo->compressRingBuffer);
                compressInfo->zlib.stream.avail_in  = maxCompressBytes;
                compressInfo->zlib.stream.next_out  = (Bytef*)RingBuffer_cArrayIn(&compressInfo->dataRingBuffer);
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
                  return ERROR_(INFLATE_FAIL,zlibResult);
                }
                RingBuffer_decrement(&compressInfo->compressRingBuffer,
                                     maxCompressBytes-compressInfo->zlib.stream.avail_in
                                    );
                RingBuffer_increment(&compressInfo->dataRingBuffer,
                                     maxDataBytes-compressInfo->zlib.stream.avail_out
                                    );

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
                compressInfo->zlib.stream.next_in   = NULL;
                compressInfo->zlib.stream.avail_in  = 0;
                compressInfo->zlib.stream.next_out  = (Bytef*)RingBuffer_cArrayIn(&compressInfo->dataRingBuffer);
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
                  return ERROR_(INFLATE_FAIL,zlibResult);
                }
                RingBuffer_increment(&compressInfo->dataRingBuffer,
                                     maxDataBytes-compressInfo->zlib.stream.avail_out
                                    );
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
      // decompress with bzip2
      #ifdef HAVE_BZ2
        {
          int bzlibResult;

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
                compressInfo->bzlib.stream.next_in   = (char*)RingBuffer_cArrayOut(&compressInfo->compressRingBuffer);
                compressInfo->bzlib.stream.avail_in  = maxCompressBytes;
                compressInfo->bzlib.stream.next_out  = (char*)RingBuffer_cArrayIn(&compressInfo->dataRingBuffer);
                compressInfo->bzlib.stream.avail_out = maxDataBytes;
                bzlibResult = BZ2_bzDecompress(&compressInfo->bzlib.stream);
                if      (bzlibResult == BZ_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (bzlibResult != BZ_OK)
                {
                  return ERROR_(INFLATE_FAIL,bzlibResult);
                }
                RingBuffer_decrement(&compressInfo->compressRingBuffer,
                                     maxCompressBytes-compressInfo->bzlib.stream.avail_in
                                    );
                RingBuffer_increment(&compressInfo->dataRingBuffer,
                                     maxDataBytes-compressInfo->bzlib.stream.avail_out
                                    );

                // update compress state
                compressInfo->compressState = COMPRESS_STATE_RUNNING;
              }
            }
          }

          if (!compressInfo->endOfDataFlag)                                           // not end-of-data
          {
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
                compressInfo->bzlib.stream.next_in   = NULL;
                compressInfo->bzlib.stream.avail_in  = 0;
                compressInfo->bzlib.stream.next_out  = (char*)RingBuffer_cArrayIn(&compressInfo->dataRingBuffer);
                compressInfo->bzlib.stream.avail_out = maxDataBytes;
                bzlibResult = BZ2_bzDecompress(&compressInfo->bzlib.stream);
                if      (bzlibResult == BZ_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (bzlibResult != BZ_RUN_OK)
                {
                  return ERROR_(INFLATE_FAIL,bzlibResult);
                }
                RingBuffer_increment(&compressInfo->dataRingBuffer,
                                     maxDataBytes-compressInfo->bzlib.stream.avail_out
                                    );
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
      // decompress with lzma
      #ifdef HAVE_LZMA
        {
          lzma_ret lzmaResult;

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
                compressInfo->lzmalib.stream.next_in   = (uint8_t*)RingBuffer_cArrayOut(&compressInfo->compressRingBuffer);
                compressInfo->lzmalib.stream.avail_in  = maxCompressBytes;
                compressInfo->lzmalib.stream.next_out  = (uint8_t*)RingBuffer_cArrayIn(&compressInfo->dataRingBuffer);
                compressInfo->lzmalib.stream.avail_out = maxDataBytes;
                lzmaResult = lzma_code(&compressInfo->lzmalib.stream,LZMA_RUN);
                if      (lzmaResult == LZMA_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (lzmaResult != LZMA_OK)
                {
                  return ERROR_(INFLATE_FAIL,lzmaResult);
                }
                RingBuffer_decrement(&compressInfo->compressRingBuffer,
                                     maxCompressBytes-compressInfo->lzmalib.stream.avail_in
                                    );
                RingBuffer_increment(&compressInfo->dataRingBuffer,
                                     maxDataBytes-compressInfo->lzmalib.stream.avail_out
                                    );

                // update compress state
                compressInfo->compressState = COMPRESS_STATE_RUNNING;
              }
            }
          }

          if (!compressInfo->endOfDataFlag)                                           // not end-of-data
          {
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
                compressInfo->lzmalib.stream.next_in   = NULL;
                compressInfo->lzmalib.stream.avail_in  = 0;
                compressInfo->lzmalib.stream.next_out  = (uint8_t*)RingBuffer_cArrayIn(&compressInfo->dataRingBuffer);
                compressInfo->lzmalib.stream.avail_out = maxDataBytes;
                lzmaResult = lzma_code(&compressInfo->lzmalib.stream,LZMA_FINISH);
                if      (lzmaResult == LZMA_STREAM_END)
                {
                  compressInfo->endOfDataFlag = TRUE;
                }
                else if (lzmaResult != LZMA_OK)
                {
                  return ERROR_(INFLATE_FAIL,lzmaResult);
                }
                RingBuffer_increment(&compressInfo->dataRingBuffer,
                                     maxDataBytes-compressInfo->lzmalib.stream.avail_out
                                    );
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
      // decompress with xdelta
      #ifdef HAVE_XDELTA3
        {
          ulong  compressBytes;
          bool   doneFlag;
          int    xdeltaResult;
          ulong  n;
          Errors error;
          ulong  requiredBytes;
          ulong  bytesRead;
//static int xr=0;
//static int xw=0;

          if (RingBuffer_isEmpty(&compressInfo->dataRingBuffer))                      // no data in data buffer
          {
            // get decompressed data from output buffer
            if (!RingBuffer_isEmpty(&compressInfo->xdelta.outputRingBuffer))
            {
              dataBytes = MIN(RingBuffer_getAvailable(&compressInfo->xdelta.outputRingBuffer),
                              RingBuffer_getFree(&compressInfo->dataRingBuffer)
                             );
              RingBuffer_move(&compressInfo->xdelta.outputRingBuffer,
                              &compressInfo->dataRingBuffer,
                              dataBytes
                             );
            }
          }

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
//fprintf(stderr,"%s, %d: maxCompressBytes=%d compressBufferSize=%d compressBufferLength=%d\n",__FILE__,__LINE__,maxCompressBytes,compressInfo->compressBufferSize,compressInfo->compressBufferLength);

                // decompress: transfer compress buffer -> output buffer
                compressBytes = 0L;
                doneFlag      = FALSE;
                do
                {
                  xdeltaResult = xd3_decode_input(&compressInfo->xdelta.stream);
                  switch (xdeltaResult)
                  {
                    case XD3_INPUT:
                      if (compressBytes < maxCompressBytes)
                      {
                        n = MIN(maxCompressBytes-compressBytes,sizeof(compressInfo->xdelta.inputBuffer));
                        assert(n > 0);
//??? dataBufferIndex is always 0?
                        RingBuffer_get(&compressInfo->compressRingBuffer,
                                       compressInfo->xdelta.inputBuffer,
                                       n
                                      );
                        xd3_avail_input(&compressInfo->xdelta.stream,
                                        compressInfo->xdelta.inputBuffer,
                                        (usize_t)n
                                       );
//xw+=n;
//fprintf(stderr,"XD3_INPUT 1 - n=%d xw=%d compressInfo->xdelta.stream.total_in=%d\n",n,xw,compressInfo->xdelta.stream.total_in);
//dumpMemory(FALSE,compressInfo->xdelta.inputBuffer,n);
                        compressBytes += n;
                      }
                      else
                      {
                        xd3_avail_input(&compressInfo->xdelta.stream,compressInfo->xdelta.inputBuffer,0);

                        doneFlag = TRUE;
                      }
                      break;
                    case XD3_OUTPUT:
                      // increase output buffer size if required
                      if (RingBuffer_getFree(&compressInfo->xdelta.outputRingBuffer) < (ulong)compressInfo->xdelta.stream.avail_out)
                      {
                        requiredBytes = RingBuffer_getSize(&compressInfo->xdelta.outputRingBuffer)+
                                        (ulong)compressInfo->xdelta.stream.avail_out-RingBuffer_getFree(&compressInfo->xdelta.outputRingBuffer);
                        if (!RingBuffer_resize(&compressInfo->xdelta.outputRingBuffer,requiredBytes))
                        {
                          HALT_INSUFFICIENT_MEMORY();
                        }
                      }

                      // append compressed data to output buffer
                      RingBuffer_put(&compressInfo->xdelta.outputRingBuffer,
                                     compressInfo->xdelta.stream.next_out,
                                     compressInfo->xdelta.stream.avail_out
                                    );

                      // done decompressed data
                      xd3_consume_output(&compressInfo->xdelta.stream);

                      doneFlag = TRUE;
//fprintf(stderr,"%s,%d: outputBufferSize=%ld outputBufferLength=%ld\n",__FILE__,__LINE__,compressInfo->xdelta.outputBufferSize,compressInfo->xdelta.outputBufferLength);
                      break;
                    case XD3_GETSRCBLK:
                      error = Source_getEntryDataBlock(compressInfo->xdelta.sourceHandle,
                                                       (void*)compressInfo->xdelta.source.curblk,
                                                       (uint64)compressInfo->xdelta.source.blksize*compressInfo->xdelta.source.getblkno,
                                                       compressInfo->xdelta.source.blksize,
                                                       &bytesRead
                                                      );
                      if (error != ERROR_NONE)
                      {
                        return error;
                      }
                      compressInfo->xdelta.source.onblk    = bytesRead;
                      compressInfo->xdelta.source.curblkno = compressInfo->xdelta.source.getblkno;
                      break;
                    case XD3_GOTHEADER:
//fprintf(stderr,"%s,%d: XD3_GOTHEADER\n",__FILE__,__LINE__);
                      break;
                    case XD3_WINSTART:
//fprintf(stderr,"%s,%d: XD3_WINSTART\n",__FILE__,__LINE__);
                      break;
                    case XD3_WINFINISH:
//fprintf(stderr,"%s,%d: XD3_WINFINISH\n",__FILE__,__LINE__);
                      break;
                    default:
                      return ERRORX_(INFLATE_FAIL,xdeltaResult,xd3_errstring(&compressInfo->xdelta.stream));
                      break;
                  }
                }
                while (!doneFlag);

                // get decompressed data from output buffer
                dataBytes = MIN(RingBuffer_getAvailable(&compressInfo->xdelta.outputRingBuffer),
                                maxDataBytes
                               );
                if (dataBytes > 0L)
                {
                  // copy from output buffer -> data buffer
                  RingBuffer_move(&compressInfo->xdelta.outputRingBuffer,
                                  &compressInfo->dataRingBuffer,
                                  dataBytes
                                 );
                }

                // update compress state
                compressInfo->compressState = COMPRESS_STATE_RUNNING;
              }
            }

            /* Note: do not try to decompress more data here, because
               end-of-data is not recognized by xdelta-decompressor.
               Instead decompress only minimum, then return. Caller
               check if all data is decompressed and call
               xdelta-decompressor again if not all data is decompressed.
            */
            if (RingBuffer_isEmpty(&compressInfo->dataRingBuffer))                    // no data in data buffer
            {
//fprintf(stderr,"%s, %d: %d %d\n",__FILE__,__LINE__,compressInfo->compressBufferIndex,compressInfo->compressBufferLength);
//fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,compressInfo->flushFlag);
//fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,compressInfo->compressState == COMPRESS_STATE_RUNNING);
              // finish decompress, flush internal decompress buffers
              if (   RingBuffer_isEmpty(&compressInfo->compressRingBuffer)            // all compressed data processed
                  && compressInfo->flushFlag                                          // flush data requested
                  && (compressInfo->compressState == COMPRESS_STATE_RUNNING)          // compressor is running -> data available in internal buffers
                 )
              {
                // get max. number of data bytes
                maxDataBytes = RingBuffer_getFree(&compressInfo->dataRingBuffer);

                // decompress with flush: transfer rest of internal data -> output buffer
                xd3_set_flags(&compressInfo->xdelta.stream,compressInfo->xdelta.stream.flags|XD3_FLUSH);
                doneFlag = FALSE;
                do
                {
                  xdeltaResult = xd3_decode_input(&compressInfo->xdelta.stream);
                  switch (xdeltaResult)
                  {
                    case XD3_INPUT:
                      xd3_avail_input(&compressInfo->xdelta.stream,compressInfo->xdelta.inputBuffer,0);

                      doneFlag = TRUE;
                      break;
                    case XD3_OUTPUT:
                      // increase output buffer size if required
                      if (RingBuffer_getFree(&compressInfo->xdelta.outputRingBuffer) < (ulong)compressInfo->xdelta.stream.avail_out)
                      {
                        requiredBytes = RingBuffer_getSize(&compressInfo->xdelta.outputRingBuffer)+
                                        (ulong)compressInfo->xdelta.stream.avail_out-RingBuffer_getFree(&compressInfo->xdelta.outputRingBuffer);
                        if (!RingBuffer_resize(&compressInfo->xdelta.outputRingBuffer,requiredBytes))
                        {
                          HALT_INSUFFICIENT_MEMORY();
                        }
                      }

                      // append compressed data to output buffer
                      RingBuffer_put(&compressInfo->xdelta.outputRingBuffer,
                                     compressInfo->xdelta.stream.next_out,
                                     compressInfo->xdelta.stream.avail_out
                                    );

                      // done decompressed data
                      xd3_consume_output(&compressInfo->xdelta.stream);

                      doneFlag = TRUE;
//fprintf(stderr,"%s,%d: outputBufferSize=%ld outputBufferLength=%ld\n",__FILE__,__LINE__,compressInfo->xdelta.outputBufferSize,compressInfo->xdelta.outputBufferLength);
                      break;
                    case XD3_GETSRCBLK:
                      error = Source_getEntryDataBlock(compressInfo->xdelta.sourceHandle,
                                                       (void*)compressInfo->xdelta.source.curblk,
                                                       (uint64)compressInfo->xdelta.source.blksize*compressInfo->xdelta.source.getblkno,
                                                       compressInfo->xdelta.source.blksize,
                                                       &bytesRead
                                                      );
                      if (error != ERROR_NONE)
                      {
                        return error;
                      }
                      compressInfo->xdelta.source.onblk    = bytesRead;
                      compressInfo->xdelta.source.curblkno = compressInfo->xdelta.source.getblkno;
//dumpMemory(FALSE,compressInfo->xdelta.source.curblk,bytesRead);
                      break;
                    case XD3_GOTHEADER:
//fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
                      break;
                    case XD3_WINSTART:
//fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
                      break;
                    case XD3_WINFINISH:
//fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
                      break;
                    default:
                      return ERRORX_(INFLATE_FAIL,xdeltaResult,xd3_errstring(&compressInfo->xdelta.stream));
                      break;
                  }
                }
                while (!doneFlag);

                // get decompressed data from output buffer
                if (!RingBuffer_isEmpty(&compressInfo->xdelta.outputRingBuffer))
                {
                  // copy from output buffer -> data buffer
                  dataBytes = MIN(RingBuffer_getAvailable(&compressInfo->xdelta.outputRingBuffer),
                                  RingBuffer_getFree(&compressInfo->dataRingBuffer)
                                 );
                  RingBuffer_move(&compressInfo->xdelta.outputRingBuffer,
                                  &compressInfo->dataRingBuffer,
                                  dataBytes
                                 );
                }
                else
                {
                  // no more data
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

/*---------------------------------------------------------------------*/

Errors Compress_initAll(void)
{
  return ERROR_NONE;
}

void Compress_doneAll(void)
{
}

const char *Compress_getAlgorithmName(CompressAlgorithms compressAlgorithm)
{
  uint       z;
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
  uint               z;
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

bool Compress_isValidAlgorithm(uint16 n)
{
  uint z;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS))
         && (COMPRESS_ALGORITHMS[z].compressAlgorithm != COMPRESS_CONSTANT_TO_ALGORITHM(n))
        )
  {
    z++;
  }

  return (z < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS));
}

Errors Compress_init(CompressInfo       *compressInfo,
                     CompressModes      compressMode,
                     CompressAlgorithms compressAlgorithm,
                     ulong              blockLength,
                     SourceHandle       *sourceHandle
                    )
{
  assert(compressInfo != NULL);

  #ifndef HAVE_XDELTA3
    UNUSED_VARIABLE(sourceHandle);
  #endif

  // init variables
  compressInfo->compressMode         = compressMode;
  compressInfo->compressAlgorithm    = compressAlgorithm;
  compressInfo->blockLength          = blockLength;
  compressInfo->compressState        = COMPRESS_STATE_INIT;
  compressInfo->endOfDataFlag        = FALSE;
  compressInfo->flushFlag            = FALSE;
  // allocate buffers
  if (!RingBuffer_init(&compressInfo->dataRingBuffer,1,FLOOR(MAX_BUFFER_SIZE,blockLength)))
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  if (!RingBuffer_init(&compressInfo->compressRingBuffer,1,FLOOR(MAX_BUFFER_SIZE,blockLength)))
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  switch (compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      compressInfo->none.totalBytes = 0LL;
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
                RingBuffer_done(&compressInfo->compressRingBuffer,NULL,NULL);
                RingBuffer_done(&compressInfo->dataRingBuffer,NULL,NULL);
                return ERROR_INIT_COMPRESS;
              }
              break;
            case COMPRESS_MODE_INFLATE:
              if (inflateInit(&compressInfo->zlib.stream) != Z_OK)
              {
                RingBuffer_done(&compressInfo->compressRingBuffer,NULL,NULL);
                RingBuffer_done(&compressInfo->dataRingBuffer,NULL,NULL);
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
                RingBuffer_done(&compressInfo->compressRingBuffer,NULL,NULL);
                RingBuffer_done(&compressInfo->dataRingBuffer,NULL,NULL);
                return ERROR_INIT_COMPRESS;
              }
              break;
            case COMPRESS_MODE_INFLATE:
              if (BZ2_bzDecompressInit(&compressInfo->bzlib.stream,0,0) != BZ_OK)
              {
                RingBuffer_done(&compressInfo->compressRingBuffer,NULL,NULL);
                RingBuffer_done(&compressInfo->dataRingBuffer,NULL,NULL);
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
                RingBuffer_done(&compressInfo->compressRingBuffer,NULL,NULL);
                RingBuffer_done(&compressInfo->dataRingBuffer,NULL,NULL);
                return ERROR_INIT_COMPRESS;
              }
              break;
            case COMPRESS_MODE_INFLATE:
              compressInfo->lzmalib.stream = streamInit;
              if (lzma_auto_decoder(&compressInfo->lzmalib.stream,0xFFFffffFFFFffffLL,0) != LZMA_OK)
              {
                RingBuffer_done(&compressInfo->compressRingBuffer,NULL,NULL);
                RingBuffer_done(&compressInfo->dataRingBuffer,NULL,NULL);
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
          compressInfo->xdelta.sourceHandle = sourceHandle;
          if (!RingBuffer_init(&compressInfo->xdelta.outputRingBuffer,1,XDELTA_BUFFER_SIZE))
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          compressInfo->xdelta.flags        = 0;
          compressInfo->xdelta.flushFlag    = FALSE;

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

          // init xdelta configuration (Note: clear memory is required by xdelta library!)
          memset(&xd3Config,0,sizeof(xd3Config));
          xd3_init_config(&xd3Config,compressInfo->xdelta.flags);
          xd3Config.winsize = XDELTA_BUFFER_SIZE;

          // init xdelta stream (Note: clear memory is required by xdelta library!)
          memset(&compressInfo->xdelta.stream,0,sizeof(compressInfo->xdelta.stream));
          compressInfo->xdelta.stream.getblk = NULL;
          if (xd3_config_stream(&compressInfo->xdelta.stream,&xd3Config) != 0)
          {
            RingBuffer_done(&compressInfo->compressRingBuffer,NULL,NULL);
            RingBuffer_done(&compressInfo->dataRingBuffer,NULL,NULL);
            return ERROR_INIT_COMPRESS;
          }

          // allocate source data buffer
          compressInfo->xdelta.sourceBuffer = malloc(XDELTA_BUFFER_SIZE);
          if (compressInfo->xdelta.sourceBuffer == NULL)
          {
            xd3_free_stream(&compressInfo->xdelta.stream);
            RingBuffer_done(&compressInfo->compressRingBuffer,NULL,NULL);
            RingBuffer_done(&compressInfo->dataRingBuffer,NULL,NULL);
            return ERROR_INIT_COMPRESS;
          }

          // init xdelta source variables (Note: clear memory is required by xdelta library!)
          memset(&compressInfo->xdelta.source,0,sizeof(compressInfo->xdelta.source));
          compressInfo->xdelta.source.ioh      = NULL;
          compressInfo->xdelta.source.blksize  = XDELTA_BUFFER_SIZE;
          compressInfo->xdelta.source.onblk    = (usize_t)0;
          compressInfo->xdelta.source.curblkno = (xoff_t)(-1);
          compressInfo->xdelta.source.curblk   = compressInfo->xdelta.sourceBuffer;
          if (xd3_set_source(&compressInfo->xdelta.stream,&compressInfo->xdelta.source) != 0)
          {
            xd3_free_stream(&compressInfo->xdelta.stream);
            free(compressInfo->xdelta.sourceBuffer);
            RingBuffer_done(&compressInfo->compressRingBuffer,NULL,NULL);
            RingBuffer_done(&compressInfo->dataRingBuffer,NULL,NULL);
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

void Compress_done(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

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
        RingBuffer_done(&compressInfo->xdelta.outputRingBuffer,NULL,NULL);
        free(compressInfo->xdelta.sourceBuffer);
      #else /* not HAVE_XDELTA3 */
        return;
      #endif /* HAVE_XDELTA3 */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  RingBuffer_done(&compressInfo->compressRingBuffer,NULL,NULL);
  RingBuffer_done(&compressInfo->dataRingBuffer,NULL,NULL);
}

Errors Compress_reset(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  // reset variables, buffers
  compressInfo->compressState = COMPRESS_STATE_INIT;
  compressInfo->endOfDataFlag = FALSE;
  compressInfo->flushFlag     = FALSE;
  RingBuffer_clear(&compressInfo->dataRingBuffer,NULL,NULL);
  RingBuffer_clear(&compressInfo->compressRingBuffer,NULL,NULL);

  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      compressInfo->none.totalBytes = 0LL;
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
              if ((zlibResult != Z_OK) && (zlibResult != Z_STREAM_END))
              {
                return ERROR_(DEFLATE_FAIL,zlibResult);
              }
              break;
            case COMPRESS_MODE_INFLATE:
              zlibResult = inflateReset(&compressInfo->zlib.stream);
              if ((zlibResult != Z_OK) && (zlibResult != Z_STREAM_END))
              {
                return ERROR_(INFLATE_FAIL,zlibResult);
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
          int bzlibResult;

          bzlibResult = BZ_PARAM_ERROR;
          switch (compressInfo->compressMode)
          {
            case COMPRESS_MODE_DEFLATE:
              BZ2_bzCompressEnd(&compressInfo->bzlib.stream);
              bzlibResult = BZ2_bzCompressInit(&compressInfo->bzlib.stream,compressInfo->bzlib.compressionLevel,0,0);
              if (bzlibResult != BZ_OK)
              {
                return ERROR_(DEFLATE_FAIL,bzlibResult);
              }
              break;
            case COMPRESS_MODE_INFLATE:
              BZ2_bzDecompressEnd(&compressInfo->bzlib.stream);
              bzlibResult = BZ2_bzDecompressInit(&compressInfo->bzlib.stream,0,0);
              if (bzlibResult != BZ_OK)
              {
                return ERROR_(INFLATE_FAIL,bzlibResult);
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
          int         lzmalibResult;

          lzmalibResult = LZMA_PROG_ERROR;
          switch (compressInfo->compressMode)
          {
            case COMPRESS_MODE_DEFLATE:
              lzma_end(&compressInfo->lzmalib.stream);
              compressInfo->lzmalib.stream = streamInit;
              lzmalibResult = lzma_easy_encoder(&compressInfo->lzmalib.stream,compressInfo->lzmalib.compressionLevel,LZMA_CHECK_NONE);
              if (lzmalibResult != LZMA_OK)
              {
                return ERROR_(DEFLATE_FAIL,lzmalibResult);;
              }
              break;
            case COMPRESS_MODE_INFLATE:
              lzma_end(&compressInfo->lzmalib.stream);
              compressInfo->lzmalib.stream = streamInit;
              lzmalibResult = lzma_auto_decoder(&compressInfo->lzmalib.stream,0xFFFffffFFFFffffLL,0);
              if (lzmalibResult != LZMA_OK)
              {
                return ERROR_(INFLATE_FAIL,lzmalibResult);
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

          // close xdelta stream
          xd3_close_stream(&compressInfo->xdelta.stream);
          xd3_free_stream(&compressInfo->xdelta.stream);

          // re-initialize variables
          RingBuffer_clear(&compressInfo->xdelta.outputRingBuffer,NULL,NULL);
          compressInfo->xdelta.flushFlag = FALSE;

          // re-init xdelta configuration (Note: clear memory is required by xdelta library!)
          memset(&xd3Config,0,sizeof(xd3Config));
          xd3_init_config(&xd3Config,compressInfo->xdelta.flags);
          xd3Config.winsize = XDELTA_BUFFER_SIZE;

          // re-init xdelta stream (Note: clear memory is required by xdelta library!)
          memset(&compressInfo->xdelta.stream,0,sizeof(compressInfo->xdelta.stream));
          compressInfo->xdelta.stream.getblk = NULL;
          if (xd3_config_stream(&compressInfo->xdelta.stream,&xd3Config) != 0)
          {
            switch (compressInfo->compressMode)
            {
              case COMPRESS_MODE_DEFLATE:
                return ERRORX_(DEFLATE_FAIL,0,xd3_errstring(&compressInfo->xdelta.stream));
                break;
              case COMPRESS_MODE_INFLATE:
                return ERRORX_(INFLATE_FAIL,0,xd3_errstring(&compressInfo->xdelta.stream));
                break;
              #ifndef NDEBUG
                default:
                  HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                  break; /* not reached */
              #endif /* NDEBUG */
            }
          }

          // init xdelta source variables (Note: clear memory is required by xdelta library!)
          memset(&compressInfo->xdelta.source,0,sizeof(compressInfo->xdelta.source));
          compressInfo->xdelta.source.blksize  = XDELTA_BUFFER_SIZE;
          compressInfo->xdelta.source.onblk    = (usize_t)0;
          compressInfo->xdelta.source.curblkno = (xoff_t)(-1);
          compressInfo->xdelta.source.curblk   = compressInfo->xdelta.sourceBuffer;
          if (xd3_set_source(&compressInfo->xdelta.stream,&compressInfo->xdelta.source) != 0)
          {
            switch (compressInfo->compressMode)
            {
              case COMPRESS_MODE_DEFLATE:
                return ERRORX_(DEFLATE_FAIL,0,xd3_errstring(&compressInfo->xdelta.stream));
                break;
              case COMPRESS_MODE_INFLATE:
                return ERRORX_(INFLATE_FAIL,0,xd3_errstring(&compressInfo->xdelta.stream));
                break;
              #ifndef NDEBUG
                default:
                  HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                  break; /* not reached */
              #endif /* NDEBUG */
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

Errors Compress_deflate(CompressInfo *compressInfo,
                        const byte   *buffer,
                        ulong        bufferLength,
                        ulong        *deflatedBytes
                       )
{
  Errors error;
  ulong  n;

  assert(compressInfo != NULL);
  assert(compressInfo->compressMode == COMPRESS_MODE_DEFLATE);
  assert(buffer != NULL);

  if (deflatedBytes != NULL) (*deflatedBytes) = 0L;
  do
  {
    // check if data buffer is full, compress data buffer
    if (RingBuffer_isFull(&compressInfo->dataRingBuffer))
    {
      error = compressData(compressInfo);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    // get number of bytes which can be copied to data buffer
    n = MIN(RingBuffer_getFree(&compressInfo->dataRingBuffer),bufferLength);

    // copy uncompressed data into data buffer
    RingBuffer_put(&compressInfo->dataRingBuffer,buffer,n);
    buffer += n;
    bufferLength -= n;

    if (deflatedBytes != NULL) (*deflatedBytes) += n;
  }
  while (   (n > 0L)
         && (bufferLength > 0L)
         && !RingBuffer_isFull(&compressInfo->compressRingBuffer)
        );

  return ERROR_NONE;
}

Errors Compress_inflate(CompressInfo *compressInfo,
                        byte         *buffer,
                        ulong        bufferSize,
                        ulong        *inflatedBytes
                       )
{
  Errors error;
  ulong  n;

  assert(compressInfo != NULL);
  assert(compressInfo->compressMode == COMPRESS_MODE_INFLATE);
  assert(buffer != NULL);
  assert(inflatedBytes != NULL);

  (*inflatedBytes) = 0L;
  do
  {
    // check if data buffer is empty, decompress and fill data buffer
    if (RingBuffer_isEmpty(&compressInfo->dataRingBuffer))
    {
      error = decompressData(compressInfo);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    // get number of bytes which can be copied from data buffer
    n = MIN(RingBuffer_getAvailable(&compressInfo->dataRingBuffer),bufferSize);

    // copy decompressed data into buffer
    RingBuffer_get(&compressInfo->dataRingBuffer,buffer,n);
    buffer += n;
    bufferSize -= n;

    (*inflatedBytes) += n;
  }
  while (   (n > 0L)
         && (bufferSize > 0L)
        );

  return ERROR_NONE;
}

Errors Compress_flush(CompressInfo *compressInfo)
{
  Errors error;

  assert(compressInfo != NULL);

  // mark compress flush
  compressInfo->flushFlag = TRUE;

#if 0
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
#endif
error=ERROR_NONE;

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
      length = compressInfo->none.totalBytes;
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
        length = 0LL;
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
      length = compressInfo->none.totalBytes;
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
  assert(compressInfo->compressMode == COMPRESS_MODE_INFLATE);
  assert(bytes != NULL);

  if (RingBuffer_isEmpty(&compressInfo->dataRingBuffer))
  {
    // decompress data
    error = decompressData(compressInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // decompressed data is available iff bufferIndex < bufferLength
  (*bytes) = RingBuffer_getAvailable(&compressInfo->dataRingBuffer);

  return ERROR_NONE;
}

Errors Compress_getAvailableCompressedBlocks(CompressInfo       *compressInfo,
                                             CompressBlockTypes blockType,
                                             uint               *blockCount
                                            )
{
  Errors error;

  assert(compressInfo != NULL);
  assert(compressInfo->compressMode == COMPRESS_MODE_DEFLATE);
  assert(blockCount != NULL);

  // compress data
  error = compressData(compressInfo);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get number of compressed blocks
  (*blockCount) = 0;
  switch (blockType)
  {
    case COMPRESS_BLOCK_TYPE_ANY:
      // block is available iff compressBufferLength >= 0
      (*blockCount) = (uint)((RingBuffer_getAvailable(&compressInfo->compressRingBuffer)+compressInfo->blockLength-1)/compressInfo->blockLength);
      break;
    case COMPRESS_BLOCK_TYPE_FULL:
      // block is full iff compressBufferLength >= blockLength
      (*blockCount) = (uint)(RingBuffer_getAvailable(&compressInfo->compressRingBuffer)/compressInfo->blockLength);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

void Compress_getCompressedData(CompressInfo *compressInfo,
                                byte         *buffer,
                                ulong        bufferSize,
                                ulong        *bufferLength
                               )
{
  ulong n;

  assert(compressInfo != NULL);
  assert(compressInfo->compressMode == COMPRESS_MODE_DEFLATE);
  assert(buffer != NULL);
  assert(bufferSize >= compressInfo->blockLength);
  assert(bufferLength != NULL);

  // compress data (ignore error here)
  compressData(compressInfo);

  // copy compressed data into buffer
  n = MIN(RingBuffer_getAvailable(&compressInfo->compressRingBuffer),FLOOR(bufferSize,compressInfo->blockLength));
  RingBuffer_get(&compressInfo->compressRingBuffer,buffer,n);

  // set rest in last block to 0
  memset(buffer+n,
         0,
         CEIL(n,compressInfo->blockLength)-n
        );

  // set length
  (*bufferLength) = CEIL(n,compressInfo->blockLength);
  assert((*bufferLength) <= bufferSize);
  assert((*bufferLength)%compressInfo->blockLength == 0);
}

void Compress_putCompressedData(CompressInfo *compressInfo,
                                const void   *buffer,
                                ulong        bufferLength
                               )
{
  assert(compressInfo != NULL);
  assert(compressInfo->compressMode == COMPRESS_MODE_INFLATE);
  assert(buffer != NULL);

  // decompress data (ignore error here)
  decompressData(compressInfo);

  // copy data into compressed buffer
  assert(RingBuffer_getFree(&compressInfo->compressRingBuffer) >= bufferLength);
  RingBuffer_put(&compressInfo->compressRingBuffer,buffer,bufferLength);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
