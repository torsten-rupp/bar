/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver compress functions
* Systems: all
*
\***********************************************************************/

#define __COMPRESS_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "common/global.h"
#include "common/ringbuffers.h"
#include "common/lists.h"
#include "common/files.h"

#include "errors.h"
#include "entrylists.h"
#include "common/patternlists.h"
#include "crypt.h"
#include "storage.h"
#include "bar.h"

#include "compress.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

LOCAL const struct
{
  const char         *name;
  CompressAlgorithms compressAlgorithm;
}
COMPRESS_ALGORITHMS[] =
{
  { "none",     COMPRESS_ALGORITHM_NONE      },

  { "zip0",     COMPRESS_ALGORITHM_ZIP_0     },
  { "zip1",     COMPRESS_ALGORITHM_ZIP_1     },
  { "zip2",     COMPRESS_ALGORITHM_ZIP_2     },
  { "zip3",     COMPRESS_ALGORITHM_ZIP_3     },
  { "zip4",     COMPRESS_ALGORITHM_ZIP_4     },
  { "zip5",     COMPRESS_ALGORITHM_ZIP_5     },
  { "zip6",     COMPRESS_ALGORITHM_ZIP_6     },
  { "zip7",     COMPRESS_ALGORITHM_ZIP_7     },
  { "zip8",     COMPRESS_ALGORITHM_ZIP_8     },
  { "zip9",     COMPRESS_ALGORITHM_ZIP_9     },

  { "bzip1",    COMPRESS_ALGORITHM_BZIP2_1   },
  { "bzip2",    COMPRESS_ALGORITHM_BZIP2_2   },
  { "bzip3",    COMPRESS_ALGORITHM_BZIP2_3   },
  { "bzip4",    COMPRESS_ALGORITHM_BZIP2_4   },
  { "bzip5",    COMPRESS_ALGORITHM_BZIP2_5   },
  { "bzip6",    COMPRESS_ALGORITHM_BZIP2_6   },
  { "bzip7",    COMPRESS_ALGORITHM_BZIP2_7   },
  { "bzip8",    COMPRESS_ALGORITHM_BZIP2_8   },
  { "bzip9",    COMPRESS_ALGORITHM_BZIP2_9   },

  { "lzma1",    COMPRESS_ALGORITHM_LZMA_1    },
  { "lzma2",    COMPRESS_ALGORITHM_LZMA_2    },
  { "lzma3",    COMPRESS_ALGORITHM_LZMA_3    },
  { "lzma4",    COMPRESS_ALGORITHM_LZMA_4    },
  { "lzma5",    COMPRESS_ALGORITHM_LZMA_5    },
  { "lzma6",    COMPRESS_ALGORITHM_LZMA_6    },
  { "lzma7",    COMPRESS_ALGORITHM_LZMA_7    },
  { "lzma8",    COMPRESS_ALGORITHM_LZMA_8    },
  { "lzma9",    COMPRESS_ALGORITHM_LZMA_9    },

  { "lzo1",     COMPRESS_ALGORITHM_LZO_1     },
  { "lzo2",     COMPRESS_ALGORITHM_LZO_2     },
  { "lzo3",     COMPRESS_ALGORITHM_LZO_3     },
  { "lzo4",     COMPRESS_ALGORITHM_LZO_4     },
  { "lzo5",     COMPRESS_ALGORITHM_LZO_5     },

  { "lz4-0",    COMPRESS_ALGORITHM_LZ4_0     },
  { "lz4-1",    COMPRESS_ALGORITHM_LZ4_1     },
  { "lz4-2",    COMPRESS_ALGORITHM_LZ4_2     },
  { "lz4-3",    COMPRESS_ALGORITHM_LZ4_3     },
  { "lz4-4",    COMPRESS_ALGORITHM_LZ4_4     },
  { "lz4-5",    COMPRESS_ALGORITHM_LZ4_5     },
  { "lz4-6",    COMPRESS_ALGORITHM_LZ4_6     },
  { "lz4-7",    COMPRESS_ALGORITHM_LZ4_7     },
  { "lz4-8",    COMPRESS_ALGORITHM_LZ4_8     },
  { "lz4-9",    COMPRESS_ALGORITHM_LZ4_9     },
  { "lz4-10",   COMPRESS_ALGORITHM_LZ4_10    },
  { "lz4-11",   COMPRESS_ALGORITHM_LZ4_11    },
  { "lz4-12",   COMPRESS_ALGORITHM_LZ4_12    },
  { "lz4-13",   COMPRESS_ALGORITHM_LZ4_13    },
  { "lz4-14",   COMPRESS_ALGORITHM_LZ4_14    },
  { "lz4-15",   COMPRESS_ALGORITHM_LZ4_15    },
  { "lz4-16",   COMPRESS_ALGORITHM_LZ4_16    },

  { "zstd0",    COMPRESS_ALGORITHM_ZSTD_0    },
  { "zstd1",    COMPRESS_ALGORITHM_ZSTD_1    },
  { "zstd2",    COMPRESS_ALGORITHM_ZSTD_2    },
  { "zstd3",    COMPRESS_ALGORITHM_ZSTD_3    },
  { "zstd4",    COMPRESS_ALGORITHM_ZSTD_4    },
  { "zstd5",    COMPRESS_ALGORITHM_ZSTD_5    },
  { "zstd6",    COMPRESS_ALGORITHM_ZSTD_6    },
  { "zstd7",    COMPRESS_ALGORITHM_ZSTD_7    },
  { "zstd8",    COMPRESS_ALGORITHM_ZSTD_8    },
  { "zstd9",    COMPRESS_ALGORITHM_ZSTD_9    },
  { "zstd10",   COMPRESS_ALGORITHM_ZSTD_10   },
  { "zstd11",   COMPRESS_ALGORITHM_ZSTD_11   },
  { "zstd12",   COMPRESS_ALGORITHM_ZSTD_12   },
  { "zstd13",   COMPRESS_ALGORITHM_ZSTD_13   },
  { "zstd14",   COMPRESS_ALGORITHM_ZSTD_14   },
  { "zstd15",   COMPRESS_ALGORITHM_ZSTD_15   },
  { "zstd16",   COMPRESS_ALGORITHM_ZSTD_16   },
  { "zstd17",   COMPRESS_ALGORITHM_ZSTD_17   },
  { "zstd18",   COMPRESS_ALGORITHM_ZSTD_18   },
  { "zstd19",   COMPRESS_ALGORITHM_ZSTD_19   },

  { "xdelta1",  COMPRESS_ALGORITHM_XDELTA_1  },
  { "xdelta2",  COMPRESS_ALGORITHM_XDELTA_2  },
  { "xdelta3",  COMPRESS_ALGORITHM_XDELTA_3  },
  { "xdelta4",  COMPRESS_ALGORITHM_XDELTA_4  },
  { "xdelta5",  COMPRESS_ALGORITHM_XDELTA_5  },
  { "xdelta6",  COMPRESS_ALGORITHM_XDELTA_6  },
  { "xdelta7",  COMPRESS_ALGORITHM_XDELTA_7  },
  { "xdelta8",  COMPRESS_ALGORITHM_XDELTA_8  },
  { "xdelta9",  COMPRESS_ALGORITHM_XDELTA_9  },
};

// size of compress buffers
#define MAX_BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#if defined(HAVE_LZO) || defined(HAVE_LZ4)
/***********************************************************************\
* Name   : putUINT32
* Purpose: put uint32 into buffer (big endian)
* Input  : buffer - buffer
*          n      - value
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void putUINT32(byte *buffer, uint32 n)
{
  buffer[0] = (n & 0xFF000000) >> 24;
  buffer[1] = (n & 0x00FF0000) >> 16;
  buffer[2] = (n & 0x0000FF00) >>  8;
  buffer[3] = (n & 0x000000FF) >>  0;
}

/***********************************************************************\
* Name   : getUINT32
* Purpose: get uint32 from buffer (big endian)
* Input  : buffer - buffer
* Output : -
* Return : value
* Notes  : -
\***********************************************************************/

LOCAL_INLINE uint32 getUINT32(byte *buffer)
{
  assert(buffer != NULL);

  return   ((uint32)buffer[0] << 24)
         | ((uint32)buffer[1] << 16)
         | ((uint32)buffer[2] <<  8)
         | ((uint32)buffer[3] <<  0);
}
#endif /* defined(HAVE_LZO) || defined(HAVE_LZ4) */

#ifdef HAVE_Z
  #include "compress_zip.c"
#endif /* HAVE_Z */
#ifdef HAVE_BZ2
  #include "compress_bz2.c"
#endif /* HAVE_BZ2 */
#ifdef HAVE_LZMA
  #include "compress_lzma.c"
#endif /* HAVE_LZMA */
#ifdef HAVE_LZO
  #include "compress_lzo.c"
#endif /* HAVE_LZO */
#ifdef HAVE_LZ4
  #include "compress_lz4.c"
#endif /* HAVE_LZ4 */
#ifdef HAVE_ZSTD
  #include "compress_zstd.c"
#endif /* HAVE_ZSTD */
#ifdef HAVE_XDELTA3
  #include "compress_xd3.c"
#endif /* HAVE_XDELTA3 */

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
  Errors error;
  ulong  maxCompressBytes,maxDataBytes;
  ulong  compressBytes;

  assert(compressInfo != NULL);

  // compress if possible
  error = ERROR_UNKNOWN;
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

      error = ERROR_NONE;
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
        error = CompressZIP_compressData(compressInfo);
      #else /* not HAVE_Z */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
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
        error = CompressBZ2_compressData(compressInfo);
      #else /* not HAVE_BZ2 */
        return ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
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
        error = CompressLZMA_compressData(compressInfo);
      #else /* not HAVE_LZMA */
        return ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_LZMA */
      break;
    case COMPRESS_ALGORITHM_LZO_1:
    case COMPRESS_ALGORITHM_LZO_2:
    case COMPRESS_ALGORITHM_LZO_3:
    case COMPRESS_ALGORITHM_LZO_4:
    case COMPRESS_ALGORITHM_LZO_5:
      // compress with lzo
      #ifdef HAVE_LZO
        error = CompressLZO_compressData(compressInfo);
      #else /* not HAVE_LZO */
        return ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_LZO */
      break;
    case COMPRESS_ALGORITHM_LZ4_0:
    case COMPRESS_ALGORITHM_LZ4_1:
    case COMPRESS_ALGORITHM_LZ4_2:
    case COMPRESS_ALGORITHM_LZ4_3:
    case COMPRESS_ALGORITHM_LZ4_4:
    case COMPRESS_ALGORITHM_LZ4_5:
    case COMPRESS_ALGORITHM_LZ4_6:
    case COMPRESS_ALGORITHM_LZ4_7:
    case COMPRESS_ALGORITHM_LZ4_8:
    case COMPRESS_ALGORITHM_LZ4_9:
    case COMPRESS_ALGORITHM_LZ4_10:
    case COMPRESS_ALGORITHM_LZ4_11:
    case COMPRESS_ALGORITHM_LZ4_12:
    case COMPRESS_ALGORITHM_LZ4_13:
    case COMPRESS_ALGORITHM_LZ4_14:
    case COMPRESS_ALGORITHM_LZ4_15:
    case COMPRESS_ALGORITHM_LZ4_16:
      // compress with lz4
      #ifdef HAVE_LZ4
        error = CompressLZ4_compressData(compressInfo);
      #else /* not HAVE_LZ4 */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_LZ4 */
      break;
    case COMPRESS_ALGORITHM_ZSTD_0:
    case COMPRESS_ALGORITHM_ZSTD_1:
    case COMPRESS_ALGORITHM_ZSTD_2:
    case COMPRESS_ALGORITHM_ZSTD_3:
    case COMPRESS_ALGORITHM_ZSTD_4:
    case COMPRESS_ALGORITHM_ZSTD_5:
    case COMPRESS_ALGORITHM_ZSTD_6:
    case COMPRESS_ALGORITHM_ZSTD_7:
    case COMPRESS_ALGORITHM_ZSTD_8:
    case COMPRESS_ALGORITHM_ZSTD_9:
    case COMPRESS_ALGORITHM_ZSTD_10:
    case COMPRESS_ALGORITHM_ZSTD_11:
    case COMPRESS_ALGORITHM_ZSTD_12:
    case COMPRESS_ALGORITHM_ZSTD_13:
    case COMPRESS_ALGORITHM_ZSTD_14:
    case COMPRESS_ALGORITHM_ZSTD_15:
    case COMPRESS_ALGORITHM_ZSTD_16:
    case COMPRESS_ALGORITHM_ZSTD_17:
    case COMPRESS_ALGORITHM_ZSTD_18:
    case COMPRESS_ALGORITHM_ZSTD_19:
      // compress with zstd
      #ifdef HAVE_ZSTD
        error = CompressZStd_compressData(compressInfo);
      #else /* not HAVE_ZSTD */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_ZSTD */
      break;
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
        error = CompressXD3_compressData(compressInfo);
      #else /* not HAVE_XDELTA3 */
        return ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_XDELTA3 */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  assert(error != ERROR_UNKNOWN);

  return error;
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
  Errors error;
  ulong maxCompressBytes,maxDataBytes;
  ulong dataBytes;

  assert(compressInfo != NULL);

  // decompress if possible
  error = ERROR_UNKNOWN;
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

      error = ERROR_NONE;
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
        error = CompressZIP_decompressData(compressInfo);
      #else /* not HAVE_Z */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
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
        error = CompressBZ2_decompressData(compressInfo);
      #else /* not HAVE_BZ2 */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
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
        error = CompressLZMA_decompressData(compressInfo);
      #else /* not HAVE_LZMA */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_LZMA */
      break;
    case COMPRESS_ALGORITHM_LZO_1:
    case COMPRESS_ALGORITHM_LZO_2:
    case COMPRESS_ALGORITHM_LZO_3:
    case COMPRESS_ALGORITHM_LZO_4:
    case COMPRESS_ALGORITHM_LZO_5:
      // decompress with lzo
      #ifdef HAVE_LZO
        error = CompressLZO_decompressData(compressInfo);
      #else /* not HAVE_LZO */
        return ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_LZO */
      break;
    case COMPRESS_ALGORITHM_LZ4_0:
    case COMPRESS_ALGORITHM_LZ4_1:
    case COMPRESS_ALGORITHM_LZ4_2:
    case COMPRESS_ALGORITHM_LZ4_3:
    case COMPRESS_ALGORITHM_LZ4_4:
    case COMPRESS_ALGORITHM_LZ4_5:
    case COMPRESS_ALGORITHM_LZ4_6:
    case COMPRESS_ALGORITHM_LZ4_7:
    case COMPRESS_ALGORITHM_LZ4_8:
    case COMPRESS_ALGORITHM_LZ4_9:
    case COMPRESS_ALGORITHM_LZ4_10:
    case COMPRESS_ALGORITHM_LZ4_11:
    case COMPRESS_ALGORITHM_LZ4_12:
    case COMPRESS_ALGORITHM_LZ4_13:
    case COMPRESS_ALGORITHM_LZ4_14:
    case COMPRESS_ALGORITHM_LZ4_15:
    case COMPRESS_ALGORITHM_LZ4_16:
      // decompress with lz4
      #ifdef HAVE_LZ4
        error = CompressLZ4_decompressData(compressInfo);
      #else /* not HAVE_LZ4 */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_LZ4 */
      break;
    case COMPRESS_ALGORITHM_ZSTD_0:
    case COMPRESS_ALGORITHM_ZSTD_1:
    case COMPRESS_ALGORITHM_ZSTD_2:
    case COMPRESS_ALGORITHM_ZSTD_3:
    case COMPRESS_ALGORITHM_ZSTD_4:
    case COMPRESS_ALGORITHM_ZSTD_5:
    case COMPRESS_ALGORITHM_ZSTD_6:
    case COMPRESS_ALGORITHM_ZSTD_7:
    case COMPRESS_ALGORITHM_ZSTD_8:
    case COMPRESS_ALGORITHM_ZSTD_9:
    case COMPRESS_ALGORITHM_ZSTD_10:
    case COMPRESS_ALGORITHM_ZSTD_11:
    case COMPRESS_ALGORITHM_ZSTD_12:
    case COMPRESS_ALGORITHM_ZSTD_13:
    case COMPRESS_ALGORITHM_ZSTD_14:
    case COMPRESS_ALGORITHM_ZSTD_15:
    case COMPRESS_ALGORITHM_ZSTD_16:
    case COMPRESS_ALGORITHM_ZSTD_17:
    case COMPRESS_ALGORITHM_ZSTD_18:
    case COMPRESS_ALGORITHM_ZSTD_19:
      // decompress with zstd
      #ifdef HAVE_ZSTD
        error = CompressZStd_decompressData(compressInfo);
      #else /* not HAVE_ZSTD */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_ZSTD */
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
        error = CompressXD3_decompressData(compressInfo);
      #else /* not HAVE_XDELTA3 */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_XDELTA3 */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

/*---------------------------------------------------------------------*/

Errors Compress_initAll(void)
{
  return ERROR_NONE;
}

void Compress_doneAll(void)
{
}

const char *Compress_algorithmToString(CompressAlgorithms compressAlgorithm, const char *defaultValue)
{
  uint       i;
  const char *s;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS))
         && (COMPRESS_ALGORITHMS[i].compressAlgorithm != compressAlgorithm)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS))
  {
    s = COMPRESS_ALGORITHMS[i].name;
  }
  else
  {
    s = defaultValue;
  }

  return s;
}

bool Compress_parseAlgorithm(const char *name, CompressAlgorithms *compressAlgorithm)
{
  uint i;

  assert(name != NULL);
  assert(compressAlgorithm != NULL);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS))
         && !stringEqualsIgnoreCase(COMPRESS_ALGORITHMS[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS))
  {
    (*compressAlgorithm) = COMPRESS_ALGORITHMS[i].compressAlgorithm;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Compress_isValidAlgorithm(uint16 n)
{
  uint i;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS))
         && (COMPRESS_ALGORITHMS[i].compressAlgorithm != COMPRESS_CONSTANT_TO_ALGORITHM(n))
        )
  {
    i++;
  }

  return (i < SIZE_OF_ARRAY(COMPRESS_ALGORITHMS));
}

#ifdef NDEBUG
  Errors Compress_init(CompressInfo       *compressInfo,
                       CompressModes      compressMode,
                       CompressAlgorithms compressAlgorithm,
                       ulong              blockLength,
                       uint64             length,
                       DeltaSourceHandle  *deltaSourceHandle
                      )
#else /* not NDEBUG */
  Errors __Compress_init(const char         *__fileName__,
                         ulong              __lineNb__,
                         CompressInfo       *compressInfo,
                         CompressModes      compressMode,
                         CompressAlgorithms compressAlgorithm,
                         ulong              blockLength,
                         uint64             length,
                         DeltaSourceHandle  *deltaSourceHandle
                        )
#endif /* NDEBUG */
{
  Errors error;

  assert(compressInfo != NULL);

  #ifndef HAVE_XDELTA3
    UNUSED_VARIABLE(deltaSourceHandle);
  #endif

  // init variables
  compressInfo->compressMode      = compressMode;
  compressInfo->compressAlgorithm = compressAlgorithm;
  compressInfo->blockLength       = blockLength;
  compressInfo->length            = length;
  compressInfo->compressState     = COMPRESS_STATE_INIT;
  compressInfo->endOfDataFlag     = FALSE;
  compressInfo->flushFlag         = FALSE;

  // allocate buffers
  if (!RingBuffer_init(&compressInfo->dataRingBuffer,1,FLOOR(MAX_BUFFER_SIZE,blockLength)))
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  if (!RingBuffer_init(&compressInfo->compressRingBuffer,1,FLOOR(MAX_BUFFER_SIZE,blockLength)))
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  error = ERROR_UNKNOWN;
  switch (compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      compressInfo->none.totalBytes = 0LL;
      error = ERROR_NONE;
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
        error = CompressZIP_init(compressInfo,compressMode,compressAlgorithm);
      #else /* not HAVE_Z */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
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
        error = CompressBZ2_init(compressInfo,compressMode,compressAlgorithm);
      #else /* not HAVE_BZ2 */
        return ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
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
        error = CompressLZMA_init(compressInfo,compressMode,compressAlgorithm);
      #else /* not HAVE_LZMA */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_LZMA */
      break;
    case COMPRESS_ALGORITHM_LZO_1:
    case COMPRESS_ALGORITHM_LZO_2:
    case COMPRESS_ALGORITHM_LZO_3:
    case COMPRESS_ALGORITHM_LZO_4:
    case COMPRESS_ALGORITHM_LZO_5:
      // decompress with lzo
      #ifdef HAVE_LZO
        error = CompressLZO_init(compressInfo,compressMode,compressAlgorithm);
      #else /* not HAVE_LZO */
        return ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_LZO */
      break;
    case COMPRESS_ALGORITHM_LZ4_0:
    case COMPRESS_ALGORITHM_LZ4_1:
    case COMPRESS_ALGORITHM_LZ4_2:
    case COMPRESS_ALGORITHM_LZ4_3:
    case COMPRESS_ALGORITHM_LZ4_4:
    case COMPRESS_ALGORITHM_LZ4_5:
    case COMPRESS_ALGORITHM_LZ4_6:
    case COMPRESS_ALGORITHM_LZ4_7:
    case COMPRESS_ALGORITHM_LZ4_8:
    case COMPRESS_ALGORITHM_LZ4_9:
    case COMPRESS_ALGORITHM_LZ4_10:
    case COMPRESS_ALGORITHM_LZ4_11:
    case COMPRESS_ALGORITHM_LZ4_12:
    case COMPRESS_ALGORITHM_LZ4_13:
    case COMPRESS_ALGORITHM_LZ4_14:
    case COMPRESS_ALGORITHM_LZ4_15:
    case COMPRESS_ALGORITHM_LZ4_16:
      #ifdef HAVE_LZ4
        error = CompressLZ4_init(compressInfo,compressMode,compressAlgorithm);
      #else /* not HAVE_LZ4 */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_LZ4 */
      break;
    case COMPRESS_ALGORITHM_ZSTD_0:
    case COMPRESS_ALGORITHM_ZSTD_1:
    case COMPRESS_ALGORITHM_ZSTD_2:
    case COMPRESS_ALGORITHM_ZSTD_3:
    case COMPRESS_ALGORITHM_ZSTD_4:
    case COMPRESS_ALGORITHM_ZSTD_5:
    case COMPRESS_ALGORITHM_ZSTD_6:
    case COMPRESS_ALGORITHM_ZSTD_7:
    case COMPRESS_ALGORITHM_ZSTD_8:
    case COMPRESS_ALGORITHM_ZSTD_9:
    case COMPRESS_ALGORITHM_ZSTD_10:
    case COMPRESS_ALGORITHM_ZSTD_11:
    case COMPRESS_ALGORITHM_ZSTD_12:
    case COMPRESS_ALGORITHM_ZSTD_13:
    case COMPRESS_ALGORITHM_ZSTD_14:
    case COMPRESS_ALGORITHM_ZSTD_15:
    case COMPRESS_ALGORITHM_ZSTD_16:
    case COMPRESS_ALGORITHM_ZSTD_17:
    case COMPRESS_ALGORITHM_ZSTD_18:
    case COMPRESS_ALGORITHM_ZSTD_19:
      #ifdef HAVE_ZSTD
        error = CompressZStd_init(compressInfo,compressMode,compressAlgorithm);
      #else /* not HAVE_ZSTD */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_ZSTD */
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
        error = CompressXD3_init(compressInfo,compressAlgorithm,deltaSourceHandle);
      #else /* not HAVE_XDELTA3 */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_XDELTA3 */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  assert(error != ERROR_UNKNOWN);
  if (error != ERROR_NONE)
  {
    RingBuffer_done(&compressInfo->compressRingBuffer,CALLBACK_(NULL,NULL));
    RingBuffer_done(&compressInfo->dataRingBuffer,CALLBACK_(NULL,NULL));
    return error;
  }

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(compressInfo,CompressInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,compressInfo,CompressInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  void Compress_done(CompressInfo *compressInfo)
#else /* not NDEBUG */
  void __Compress_done(const char   *__fileName__,
                       ulong        __lineNb__,
                       CompressInfo *compressInfo
                      )
#endif /* NDEBUG */
{
  assert(compressInfo != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(compressInfo,CompressInfo);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,compressInfo,CompressInfo);
  #endif /* NDEBUG */

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
        CompressZIP_done(compressInfo);
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
        CompressBZ2_done(compressInfo);
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
        CompressLZMA_done(compressInfo);
      #else /* not HAVE_LZMA */
      #endif /* HAVE_LZMA */
      break;
    case COMPRESS_ALGORITHM_LZO_1:
    case COMPRESS_ALGORITHM_LZO_2:
    case COMPRESS_ALGORITHM_LZO_3:
    case COMPRESS_ALGORITHM_LZO_4:
    case COMPRESS_ALGORITHM_LZO_5:
      #ifdef HAVE_LZO
        CompressLZO_done(compressInfo);
      #else /* not HAVE_LZO */
      #endif /* HAVE_LZO */
      break;
    case COMPRESS_ALGORITHM_LZ4_0:
    case COMPRESS_ALGORITHM_LZ4_1:
    case COMPRESS_ALGORITHM_LZ4_2:
    case COMPRESS_ALGORITHM_LZ4_3:
    case COMPRESS_ALGORITHM_LZ4_4:
    case COMPRESS_ALGORITHM_LZ4_5:
    case COMPRESS_ALGORITHM_LZ4_6:
    case COMPRESS_ALGORITHM_LZ4_7:
    case COMPRESS_ALGORITHM_LZ4_8:
    case COMPRESS_ALGORITHM_LZ4_9:
    case COMPRESS_ALGORITHM_LZ4_10:
    case COMPRESS_ALGORITHM_LZ4_11:
    case COMPRESS_ALGORITHM_LZ4_12:
    case COMPRESS_ALGORITHM_LZ4_13:
    case COMPRESS_ALGORITHM_LZ4_14:
    case COMPRESS_ALGORITHM_LZ4_15:
    case COMPRESS_ALGORITHM_LZ4_16:
      #ifdef HAVE_LZ4
        CompressLZ4_done(compressInfo);
      #else /* not HAVE_LZ4 */
      #endif /* HAVE_LZ4 */
      break;
    case COMPRESS_ALGORITHM_ZSTD_0:
    case COMPRESS_ALGORITHM_ZSTD_1:
    case COMPRESS_ALGORITHM_ZSTD_2:
    case COMPRESS_ALGORITHM_ZSTD_3:
    case COMPRESS_ALGORITHM_ZSTD_4:
    case COMPRESS_ALGORITHM_ZSTD_5:
    case COMPRESS_ALGORITHM_ZSTD_6:
    case COMPRESS_ALGORITHM_ZSTD_7:
    case COMPRESS_ALGORITHM_ZSTD_8:
    case COMPRESS_ALGORITHM_ZSTD_9:
    case COMPRESS_ALGORITHM_ZSTD_10:
    case COMPRESS_ALGORITHM_ZSTD_11:
    case COMPRESS_ALGORITHM_ZSTD_12:
    case COMPRESS_ALGORITHM_ZSTD_13:
    case COMPRESS_ALGORITHM_ZSTD_14:
    case COMPRESS_ALGORITHM_ZSTD_15:
    case COMPRESS_ALGORITHM_ZSTD_16:
    case COMPRESS_ALGORITHM_ZSTD_17:
    case COMPRESS_ALGORITHM_ZSTD_18:
    case COMPRESS_ALGORITHM_ZSTD_19:
      #ifdef HAVE_ZSTD
        CompressZStd_done(compressInfo);
      #else /* not HAVE_ZSTD */
      #endif /* HAVE_ZSTD */
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
        CompressXD3_done(compressInfo);
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
  Errors error;

  assert(compressInfo != NULL);

  // reset variables, buffers
  compressInfo->compressState = COMPRESS_STATE_INIT;
  compressInfo->endOfDataFlag = FALSE;
  compressInfo->flushFlag     = FALSE;
  RingBuffer_clear(&compressInfo->dataRingBuffer,NULL,NULL);
  RingBuffer_clear(&compressInfo->compressRingBuffer,NULL,NULL);

  error = ERROR_UNKNOWN;
  switch (compressInfo->compressAlgorithm)
  {
    case COMPRESS_ALGORITHM_NONE:
      compressInfo->none.totalBytes = 0LL;
      error = ERROR_NONE;
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
        error = CompressZIP_reset(compressInfo);
      #else /* not HAVE_Z */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
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
        error = CompressBZ2_reset(compressInfo);
      #else /* not HAVE_BZ2 */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
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
        error = CompressLZMA_reset(compressInfo);
      #else /* not HAVE_LZMA */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_LZMA */
      break;
    case COMPRESS_ALGORITHM_LZO_1:
    case COMPRESS_ALGORITHM_LZO_2:
    case COMPRESS_ALGORITHM_LZO_3:
    case COMPRESS_ALGORITHM_LZO_4:
    case COMPRESS_ALGORITHM_LZO_5:
      #ifdef HAVE_LZO
        error = CompressLZO_reset(compressInfo);
      #else /* not HAVE_LZO */
        return ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_LZO */
      break;
    case COMPRESS_ALGORITHM_LZ4_0:
    case COMPRESS_ALGORITHM_LZ4_1:
    case COMPRESS_ALGORITHM_LZ4_2:
    case COMPRESS_ALGORITHM_LZ4_3:
    case COMPRESS_ALGORITHM_LZ4_4:
    case COMPRESS_ALGORITHM_LZ4_5:
    case COMPRESS_ALGORITHM_LZ4_6:
    case COMPRESS_ALGORITHM_LZ4_7:
    case COMPRESS_ALGORITHM_LZ4_8:
    case COMPRESS_ALGORITHM_LZ4_9:
    case COMPRESS_ALGORITHM_LZ4_10:
    case COMPRESS_ALGORITHM_LZ4_11:
    case COMPRESS_ALGORITHM_LZ4_12:
    case COMPRESS_ALGORITHM_LZ4_13:
    case COMPRESS_ALGORITHM_LZ4_14:
    case COMPRESS_ALGORITHM_LZ4_15:
    case COMPRESS_ALGORITHM_LZ4_16:
      #ifdef HAVE_LZ4
        error = CompressLZ4_reset(compressInfo);
      #else /* not HAVE_LZ4 */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_LZ4 */
      break;
    case COMPRESS_ALGORITHM_ZSTD_0:
    case COMPRESS_ALGORITHM_ZSTD_1:
    case COMPRESS_ALGORITHM_ZSTD_2:
    case COMPRESS_ALGORITHM_ZSTD_3:
    case COMPRESS_ALGORITHM_ZSTD_4:
    case COMPRESS_ALGORITHM_ZSTD_5:
    case COMPRESS_ALGORITHM_ZSTD_6:
    case COMPRESS_ALGORITHM_ZSTD_7:
    case COMPRESS_ALGORITHM_ZSTD_8:
    case COMPRESS_ALGORITHM_ZSTD_9:
    case COMPRESS_ALGORITHM_ZSTD_10:
    case COMPRESS_ALGORITHM_ZSTD_11:
    case COMPRESS_ALGORITHM_ZSTD_12:
    case COMPRESS_ALGORITHM_ZSTD_13:
    case COMPRESS_ALGORITHM_ZSTD_14:
    case COMPRESS_ALGORITHM_ZSTD_15:
    case COMPRESS_ALGORITHM_ZSTD_16:
    case COMPRESS_ALGORITHM_ZSTD_17:
    case COMPRESS_ALGORITHM_ZSTD_18:
    case COMPRESS_ALGORITHM_ZSTD_19:
      #ifdef HAVE_ZSTD
        error = CompressZStd_reset(compressInfo);
      #else /* not HAVE_ZSTD */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_ZSTD */
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
        error = CompressXD3_reset(compressInfo);
      #else /* not HAVE_XDELTA3 */
        error = ERROR_COMPRESS_ALGORITHM_NOT_SUPPORTED;
      #endif /* HAVE_XDELTA3 */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  assert(error != ERROR_UNKNOWN);

  return error;
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
  assert(compressInfo != NULL);

  // mark compress flush
  compressInfo->flushFlag = TRUE;

  return ERROR_NONE;
}

bool Compress_isEndOfData(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  // decompress data
  (void)decompressData(compressInfo);

  return compressInfo->endOfDataFlag;
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
        length = CompressZIP_getInputLength(compressInfo);
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
        length = CompressBZ2_getInputLength(compressInfo);
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
        length = CompressLZMA_getInputLength(compressInfo);
      #else /* not HAVE_LZMA */
        length = 0LL;
      #endif /* HAVE_LZMA */
      break;
    case COMPRESS_ALGORITHM_LZO_1:
    case COMPRESS_ALGORITHM_LZO_2:
    case COMPRESS_ALGORITHM_LZO_3:
    case COMPRESS_ALGORITHM_LZO_4:
    case COMPRESS_ALGORITHM_LZO_5:
      #ifdef HAVE_LZO
        length = CompressLZO_getInputLength(compressInfo);
      #else /* not HAVE_LZO */
        length = 0LL;
      #endif /* HAVE_LZO */
      break;
    case COMPRESS_ALGORITHM_LZ4_0:
    case COMPRESS_ALGORITHM_LZ4_1:
    case COMPRESS_ALGORITHM_LZ4_2:
    case COMPRESS_ALGORITHM_LZ4_3:
    case COMPRESS_ALGORITHM_LZ4_4:
    case COMPRESS_ALGORITHM_LZ4_5:
    case COMPRESS_ALGORITHM_LZ4_6:
    case COMPRESS_ALGORITHM_LZ4_7:
    case COMPRESS_ALGORITHM_LZ4_8:
    case COMPRESS_ALGORITHM_LZ4_9:
    case COMPRESS_ALGORITHM_LZ4_10:
    case COMPRESS_ALGORITHM_LZ4_11:
    case COMPRESS_ALGORITHM_LZ4_12:
    case COMPRESS_ALGORITHM_LZ4_13:
    case COMPRESS_ALGORITHM_LZ4_14:
    case COMPRESS_ALGORITHM_LZ4_15:
    case COMPRESS_ALGORITHM_LZ4_16:
      #ifdef HAVE_LZ4
        length = CompressLZ4_getInputLength(compressInfo);
      #else /* not HAVE_LZ4 */
        length = 0LL;
      #endif /* HAVE_LZ4 */
      break;
    case COMPRESS_ALGORITHM_ZSTD_0:
    case COMPRESS_ALGORITHM_ZSTD_1:
    case COMPRESS_ALGORITHM_ZSTD_2:
    case COMPRESS_ALGORITHM_ZSTD_3:
    case COMPRESS_ALGORITHM_ZSTD_4:
    case COMPRESS_ALGORITHM_ZSTD_5:
    case COMPRESS_ALGORITHM_ZSTD_6:
    case COMPRESS_ALGORITHM_ZSTD_7:
    case COMPRESS_ALGORITHM_ZSTD_8:
    case COMPRESS_ALGORITHM_ZSTD_9:
    case COMPRESS_ALGORITHM_ZSTD_10:
    case COMPRESS_ALGORITHM_ZSTD_11:
    case COMPRESS_ALGORITHM_ZSTD_12:
    case COMPRESS_ALGORITHM_ZSTD_13:
    case COMPRESS_ALGORITHM_ZSTD_14:
    case COMPRESS_ALGORITHM_ZSTD_15:
    case COMPRESS_ALGORITHM_ZSTD_16:
    case COMPRESS_ALGORITHM_ZSTD_17:
    case COMPRESS_ALGORITHM_ZSTD_18:
    case COMPRESS_ALGORITHM_ZSTD_19:
      #ifdef HAVE_ZSTD
        length = CompressZStd_getInputLength(compressInfo);
      #else /* not HAVE_ZSTD */
        length = 0LL;
      #endif /* HAVE_ZSTD */
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
        length = CompressXD3_getInputLength(compressInfo);
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
        length = CompressZIP_getOutputLength(compressInfo);
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
        length = CompressBZ2_getOutputLength(compressInfo);
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
        length = CompressLZMA_getOutputLength(compressInfo);
      #else /* not HAVE_LZMA */
        length = 0LL;
      #endif /* HAVE_LZMA */
      break;
    case COMPRESS_ALGORITHM_LZO_1:
    case COMPRESS_ALGORITHM_LZO_2:
    case COMPRESS_ALGORITHM_LZO_3:
    case COMPRESS_ALGORITHM_LZO_4:
    case COMPRESS_ALGORITHM_LZO_5:
      #ifdef HAVE_LZO
        length = CompressLZO_getOutputLength(compressInfo);
      #else /* not HAVE_LZO */
        length = 0LL;
      #endif /* HAVE_LZO */
      break;
    case COMPRESS_ALGORITHM_LZ4_0:
    case COMPRESS_ALGORITHM_LZ4_1:
    case COMPRESS_ALGORITHM_LZ4_2:
    case COMPRESS_ALGORITHM_LZ4_3:
    case COMPRESS_ALGORITHM_LZ4_4:
    case COMPRESS_ALGORITHM_LZ4_5:
    case COMPRESS_ALGORITHM_LZ4_6:
    case COMPRESS_ALGORITHM_LZ4_7:
    case COMPRESS_ALGORITHM_LZ4_8:
    case COMPRESS_ALGORITHM_LZ4_9:
    case COMPRESS_ALGORITHM_LZ4_10:
    case COMPRESS_ALGORITHM_LZ4_11:
    case COMPRESS_ALGORITHM_LZ4_12:
    case COMPRESS_ALGORITHM_LZ4_13:
    case COMPRESS_ALGORITHM_LZ4_14:
    case COMPRESS_ALGORITHM_LZ4_15:
    case COMPRESS_ALGORITHM_LZ4_16:
      #ifdef HAVE_LZ4
        length = CompressLZ4_getOutputLength(compressInfo);
      #else /* not HAVE_LZ4 */
        length = 0LL;
      #endif /* HAVE_LZ4 */
      break;
    case COMPRESS_ALGORITHM_ZSTD_0:
    case COMPRESS_ALGORITHM_ZSTD_1:
    case COMPRESS_ALGORITHM_ZSTD_2:
    case COMPRESS_ALGORITHM_ZSTD_3:
    case COMPRESS_ALGORITHM_ZSTD_4:
    case COMPRESS_ALGORITHM_ZSTD_5:
    case COMPRESS_ALGORITHM_ZSTD_6:
    case COMPRESS_ALGORITHM_ZSTD_7:
    case COMPRESS_ALGORITHM_ZSTD_8:
    case COMPRESS_ALGORITHM_ZSTD_9:
    case COMPRESS_ALGORITHM_ZSTD_10:
    case COMPRESS_ALGORITHM_ZSTD_11:
    case COMPRESS_ALGORITHM_ZSTD_12:
    case COMPRESS_ALGORITHM_ZSTD_13:
    case COMPRESS_ALGORITHM_ZSTD_14:
    case COMPRESS_ALGORITHM_ZSTD_15:
    case COMPRESS_ALGORITHM_ZSTD_16:
    case COMPRESS_ALGORITHM_ZSTD_17:
    case COMPRESS_ALGORITHM_ZSTD_18:
    case COMPRESS_ALGORITHM_ZSTD_19:
      #ifdef HAVE_ZSTD
        length = CompressZStd_getOutputLength(compressInfo);
      #else /* not HAVE_ZSTD */
        length = 0LL;
      #endif /* HAVE_ZSTD */
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
        length = CompressXD3_getOutputLength(compressInfo);
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
  (void)compressData(compressInfo);

  // copy compressed data into buffer
  n = MIN(RingBuffer_getAvailable(&compressInfo->compressRingBuffer),FLOOR(bufferSize,compressInfo->blockLength));
  RingBuffer_get(&compressInfo->compressRingBuffer,buffer,n);

  // set rest in last block to 0
  memClear(buffer+n,CEIL(n,compressInfo->blockLength)-n);

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
  (void)decompressData(compressInfo);

  // copy data into compressed buffer
  assert(RingBuffer_getFree(&compressInfo->compressRingBuffer) >= bufferLength);
  RingBuffer_put(&compressInfo->compressRingBuffer,buffer,bufferLength);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
