/***********************************************************************\
*
* Contents: Backup ARchiver compress functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "xdelta3.h"

#include "common/global.h"
#include "common/ringbuffers.h"
#include "common/lists.h"
#include "common/files.h"

#include "errors.h"
#include "entrylists.h"
#include "common/patternlists.h"
#include "deltasources.h"
#include "storage.h"

#include "compress.h"

/****************** Conditional compilation switches *******************/

#define _XDELTA_DEBUG

/***************************** Constants *******************************/

#define XDELTA_BUFFER_SIZE XD3_ALLOCSIZE

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : CompressXD3_compressData
* Purpose: compress data with XDelta3
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors CompressXD3_compressData(CompressInfo *compressInfo)
{
  ulong  maxCompressBytes,maxDataBytes;
  ulong  compressBytes;
  ulong  dataBytes;
  bool   doneFlag;
  int    xdeltaResult;
  ulong  n;
  Errors error;
  ulong  requiredBytes;
  ulong  bytesRead;
//static int xr=0;
//static int xw=0;

  assert(compressInfo != NULL);

  // compress with xdelta
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
                #ifdef XDELTA_DEBUG
                  fprintf(stderr,"%s, %d: compress: move input=%lu\n",__FILE__,__LINE__,n);
                #endif /* XDELTA_DEBUG */
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
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: compress: move output=%u\n",__FILE__,__LINE__,compressInfo->xdelta.stream.avail_out);
              #endif /* XDELTA_DEBUG */

              // done compressed data
              xd3_consume_output(&compressInfo->xdelta.stream);

              doneFlag = TRUE;
              break;
            case XD3_GETSRCBLK:
              error = DeltaSource_getEntryDataBlock(compressInfo->xdelta.deltaSourceHandle,
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
              #ifdef XDELTA_DEBUG
xxx                fprintf(stderr,"%s, %d: compress: get block=%u: %lu\n",__FILE__,__LINE__,(uint)compressInfo->xdelta.source.getblkno,bytesRead);
              #endif /* XDELTA_DEBUG */
              break;
            case XD3_GOTHEADER:
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: compress: get block=%u: %lu\n",__FILE__,__LINE__,(uint)compressInfo->xdelta.source.getblkno,bytesRead);
              #endif /* XDELTA_DEBUG */
//fprintf(stderr,"%s,%d: XD3_GOTHEADER\n",__FILE__,__LINE__);
              break;
            case XD3_WINSTART:
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: compress: get block=%u: %lu\n",__FILE__,__LINE__,(uint)compressInfo->xdelta.source.getblkno,bytesRead);
              #endif /* XDELTA_DEBUG */
//fprintf(stderr,"%s,%d: XD3_WINSTART\n",__FILE__,__LINE__);
              break;
            case XD3_WINFINISH:
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: compress: get block=%u: %lu\n",__FILE__,__LINE__,(uint)compressInfo->xdelta.source.getblkno,bytesRead);
              #endif /* XDELTA_DEBUG */
//fprintf(stderr,"%s,%d: XD3_WINFINISH\n",__FILE__,__LINE__);
              break;
            default:
              return ERRORX_(DEFLATE,xdeltaResult,"%s",xd3_errstring(&compressInfo->xdelta.stream));
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
      // finish compress, flush internal compress buffers
      if (   compressInfo->flushFlag                                          // flush data requested
          && (compressInfo->compressState == COMPRESS_STATE_RUNNING)          // compressor is running -> data available in internal buffers
         )
      {
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
                #ifdef XDELTA_DEBUG
                  fprintf(stderr,"%s, %d: compress: move input=%lu\n",__FILE__,__LINE__,n);
                #endif /* XDELTA_DEBUG */

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
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: compress: move output=%u\n",__FILE__,__LINE__,compressInfo->xdelta.stream.avail_out);
              #endif /* XDELTA_DEBUG */

              // done compressed data
              xd3_consume_output(&compressInfo->xdelta.stream);
//fprintf(stderr,"%s,%d: outputBufferSize=%ld outputBufferLength=%ld\n",__FILE__,__LINE__,compressInfo->xdelta.outputBufferSize,compressInfo->xdelta.outputBufferLength);
              break;
            case XD3_GETSRCBLK:
              error = DeltaSource_getEntryDataBlock(compressInfo->xdelta.deltaSourceHandle,
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
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: compress: get block=%u: %lu\n",__FILE__,__LINE__,(uint)compressInfo->xdelta.source.getblkno,bytesRead);
              #endif /* XDELTA_DEBUG */
              break;
            case XD3_GOTHEADER:
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: compress: get block=%u: %lu\n",__FILE__,__LINE__,(uint)compressInfo->xdelta.source.getblkno,bytesRead);
              #endif /* XDELTA_DEBUG */
//fprintf(stderr,"%s,%d: XD3_GOTHEADER\n",__FILE__,__LINE__);
              break;
            case XD3_WINSTART:
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: compress: get block=%u: %lu\n",__FILE__,__LINE__,(uint)compressInfo->xdelta.source.getblkno,bytesRead);
              #endif /* XDELTA_DEBUG */
//fprintf(stderr,"%s,%d: XD3_WINSTART\n",__FILE__,__LINE__);
              break;
            case XD3_WINFINISH:
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: compress: get block=%u: %lu\n",__FILE__,__LINE__,(uint)compressInfo->xdelta.source.getblkno,bytesRead);
              #endif /* XDELTA_DEBUG */
//fprintf(stderr,"%s,%d: XD3_WINFINISH\n",__FILE__,__LINE__);
              break;
            default:
              return ERRORX_(DEFLATE,xdeltaResult,"%s",xd3_errstring(&compressInfo->xdelta.stream));
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

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : CompressXD3_decompressData
* Purpose: decompress data with Xdelta3
* Input  : compressInfo - compress info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors CompressXD3_decompressData(CompressInfo *compressInfo)
{
  ulong maxCompressBytes,maxDataBytes;
  ulong dataBytes;
  ulong  compressBytes;
  bool   doneFlag;
  int    xdeltaResult;
  ulong  n;
  Errors error;
  ulong  requiredBytes;
  ulong  bytesRead;
//static int xr=0;
//static int xw=0;

  assert(compressInfo != NULL);

  if (!RingBuffer_isFull(&compressInfo->dataRingBuffer))                      // space in data buffer
  {
    // move xdelta decompressed data -> data buffer
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

  if (!RingBuffer_isFull(&compressInfo->dataRingBuffer))                      // space in data buffer
  {
    if (!compressInfo->endOfDataFlag)                                         // not end-of-data
    {
      // decompress available data
      if (!RingBuffer_isEmpty(&compressInfo->compressRingBuffer))             // unprocessed compressed data available
      {
        // get max. number of compressed and max. number of data bytes
        maxCompressBytes = RingBuffer_getAvailable(&compressInfo->compressRingBuffer);
        maxDataBytes     = RingBuffer_getFree(&compressInfo->dataRingBuffer);

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
                #ifdef XDELTA_DEBUG
                  fprintf(stderr,"%s, %d: decompress: move input=%u\n",__FILE__,__LINE__,compressInfo->xdelta.stream.avail_out);
                #endif /* XDELTA_DEBUG */
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

              // append uncompressed data to output buffer
              RingBuffer_put(&compressInfo->xdelta.outputRingBuffer,
                             compressInfo->xdelta.stream.next_out,
                             compressInfo->xdelta.stream.avail_out
                            );
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: decompress: move output=%u\n",__FILE__,__LINE__,compressInfo->xdelta.stream.avail_out);
              #endif /* XDELTA_DEBUG */

              // done undecompressed data
              xd3_consume_output(&compressInfo->xdelta.stream);

              doneFlag = TRUE;
//fprintf(stderr,"%s,%d: outputBufferSize=%ld outputBufferLength=%ld\n",__FILE__,__LINE__,compressInfo->xdelta.outputBufferSize,compressInfo->xdelta.outputBufferLength);
              break;
            case XD3_GETSRCBLK:
              error = DeltaSource_getEntryDataBlock(compressInfo->xdelta.deltaSourceHandle,
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
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: decompress: get block=%u: %lu\n",__FILE__,__LINE__,(uint)compressInfo->xdelta.source.getblkno,bytesRead);
              #endif /* XDELTA_DEBUG */
              break;
            case XD3_GOTHEADER:
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: decompress: XD3_GOTHEADER\n",__FILE__,__LINE__);
              #endif /* XDELTA_DEBUG */
              break;
            case XD3_WINSTART:
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: decompress: XD3_WINSTART\n",__FILE__,__LINE__);
              #endif /* XDELTA_DEBUG */
              break;
            case XD3_WINFINISH:
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: decompress: XD3_WINFINISH\n",__FILE__,__LINE__);
              #endif /* XDELTA_DEBUG */
              break;
            default:
              return ERRORX_(INFLATE,xdeltaResult,"%s",xd3_errstring(&compressInfo->xdelta.stream));
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

    /* Note: do not try to decompress more data here than minimal
       required, because end-of-data is not recognized by
       xdelta-decompressor. Instead decompress only minimum, then
       return. The caller check if all data is decompressed and
       call xdelta-decompressor again if not all data is decompressed.
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
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: decompress: length=%u\n",__FILE__,__LINE__,compressInfo->xdelta.stream.avail_out);
              #endif /* XDELTA_DEBUG */

              // done decompressed data
              xd3_consume_output(&compressInfo->xdelta.stream);

              doneFlag = TRUE;
//fprintf(stderr,"%s,%d: outputBufferSize=%ld outputBufferLength=%ld\n",__FILE__,__LINE__,compressInfo->xdelta.outputBufferSize,compressInfo->xdelta.outputBufferLength);
              break;
            case XD3_GETSRCBLK:
              error = DeltaSource_getEntryDataBlock(compressInfo->xdelta.deltaSourceHandle,
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
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: decompress: get block=%u: %lu\n",__FILE__,__LINE__,(uint)compressInfo->xdelta.source.getblkno,bytesRead);
              #endif /* XDELTA_DEBUG */
              break;
            case XD3_GOTHEADER:
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: decompress: XD3_GOTHEADER\n",__FILE__,__LINE__);
              #endif /* XDELTA_DEBUG */
              break;
            case XD3_WINSTART:
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: decompress: XD3_WINSTART\n",__FILE__,__LINE__);
              #endif /* XDELTA_DEBUG */
              break;
            case XD3_WINFINISH:
              #ifdef XDELTA_DEBUG
                fprintf(stderr,"%s, %d: decompress: XD3_WINFINISH\n",__FILE__,__LINE__);
              #endif /* XDELTA_DEBUG */
              break;
            default:
              return ERRORX_(INFLATE,xdeltaResult,"%s",xd3_errstring(&compressInfo->xdelta.stream));
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

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

LOCAL Errors CompressXD3_init(CompressInfo       *compressInfo,
                              CompressAlgorithms compressAlgorithm,
                              DeltaSourceHandle  *deltaSourceHandle
                             )
{
  xd3_config xd3Config;
  int        xd3Result;

  assert(compressInfo != NULL);

  // initialize variables
  compressInfo->xdelta.deltaSourceHandle = deltaSourceHandle;
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
  memClear(&xd3Config,sizeof(xd3Config));
  xd3_init_config(&xd3Config,compressInfo->xdelta.flags);
  xd3Config.winsize = XDELTA_BUFFER_SIZE;

  // init xdelta stream (Note: clear memory is required by xdelta library!)
  memClear(&compressInfo->xdelta.stream,sizeof(compressInfo->xdelta.stream));
  compressInfo->xdelta.stream.getblk = NULL;
  xd3Result = xd3_config_stream(&compressInfo->xdelta.stream,&xd3Config);
  if (xd3Result != 0)
  {
    RingBuffer_done(&compressInfo->xdelta.outputRingBuffer,NULL,NULL);
    return ERRORX_(INIT_COMPRESS,xd3Result,"%s",xd3_strerror(xd3Result));
  }

  // allocate source data buffer
  compressInfo->xdelta.sourceBuffer = malloc(XDELTA_BUFFER_SIZE);
  if (compressInfo->xdelta.sourceBuffer == NULL)
  {
    xd3_free_stream(&compressInfo->xdelta.stream);
    RingBuffer_done(&compressInfo->xdelta.outputRingBuffer,NULL,NULL);
    return ERRORX_(INIT_COMPRESS,0,"insufficient memory");
  }

  // init xdelta source variables (Note: clear memory is required by xdelta library!)
  memClear(&compressInfo->xdelta.source,sizeof(compressInfo->xdelta.source));
  compressInfo->xdelta.source.ioh      = NULL;
  compressInfo->xdelta.source.blksize  = XDELTA_BUFFER_SIZE;
  compressInfo->xdelta.source.onblk    = (usize_t)0;
  compressInfo->xdelta.source.curblkno = (xoff_t)(-1);
  compressInfo->xdelta.source.curblk   = compressInfo->xdelta.sourceBuffer;
  xd3Result = xd3_set_source(&compressInfo->xdelta.stream,&compressInfo->xdelta.source);
  if (xd3Result != 0)
  {
    free(compressInfo->xdelta.sourceBuffer);
    xd3_free_stream(&compressInfo->xdelta.stream);
    RingBuffer_done(&compressInfo->xdelta.outputRingBuffer,NULL,NULL);
    return ERRORX_(INIT_COMPRESS,xd3Result,"%s",xd3_strerror(xd3Result));
  }

  return ERROR_NONE;
}

LOCAL void CompressXD3_done(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  xd3_close_stream(&compressInfo->xdelta.stream);
  free(compressInfo->xdelta.sourceBuffer);
  xd3_free_stream(&compressInfo->xdelta.stream);
  RingBuffer_done(&compressInfo->xdelta.outputRingBuffer,NULL,NULL);
}

LOCAL Errors CompressXD3_reset(CompressInfo *compressInfo)
{
  xd3_config xd3Config;

  assert(compressInfo != NULL);

  // close xdelta stream
  xd3_close_stream(&compressInfo->xdelta.stream);
  xd3_free_stream(&compressInfo->xdelta.stream);

  // re-initialize variables
  RingBuffer_clear(&compressInfo->xdelta.outputRingBuffer,NULL,NULL);
  compressInfo->xdelta.flushFlag = FALSE;

  // re-init xdelta configuration (Note: clear memory is required by xdelta library!)
  memClear(&xd3Config,sizeof(xd3Config));
  xd3_init_config(&xd3Config,compressInfo->xdelta.flags);
  xd3Config.winsize = XDELTA_BUFFER_SIZE;

  // re-init xdelta stream (Note: clear memory is required by xdelta library!)
  memClear(&compressInfo->xdelta.stream,sizeof(compressInfo->xdelta.stream));
  compressInfo->xdelta.stream.getblk = NULL;
  if (xd3_config_stream(&compressInfo->xdelta.stream,&xd3Config) != 0)
  {
    switch (compressInfo->compressMode)
    {
      case COMPRESS_MODE_DEFLATE:
        return ERRORX_(DEFLATE,0,"%s",xd3_errstring(&compressInfo->xdelta.stream));
        break;
      case COMPRESS_MODE_INFLATE:
        return ERRORX_(INFLATE,0,"%s",xd3_errstring(&compressInfo->xdelta.stream));
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
  }

  // init xdelta source variables (Note: clear memory is required by xdelta library!)
  memClear(&compressInfo->xdelta.source,sizeof(compressInfo->xdelta.source));
  compressInfo->xdelta.source.blksize  = XDELTA_BUFFER_SIZE;
  compressInfo->xdelta.source.onblk    = (usize_t)0;
  compressInfo->xdelta.source.curblkno = (xoff_t)(-1);
  compressInfo->xdelta.source.curblk   = compressInfo->xdelta.sourceBuffer;
  if (xd3_set_source(&compressInfo->xdelta.stream,&compressInfo->xdelta.source) != 0)
  {
    switch (compressInfo->compressMode)
    {
      case COMPRESS_MODE_DEFLATE:
        return ERRORX_(DEFLATE,0,"%s",xd3_errstring(&compressInfo->xdelta.stream));
        break;
      case COMPRESS_MODE_INFLATE:
        return ERRORX_(INFLATE,0,"%s",xd3_errstring(&compressInfo->xdelta.stream));
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
  }

  return ERROR_NONE;
}

LOCAL uint64 CompressXD3_getInputLength(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return (uint64)compressInfo->xdelta.stream.total_in;
}

LOCAL uint64 CompressXD3_getOutputLength(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return (uint64)compressInfo->xdelta.stream.total_out;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
