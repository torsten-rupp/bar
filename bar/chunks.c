/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver file chunks functions
* Systems: all
*
\***********************************************************************/

#define __CHUNKS_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef HAVE_NETINET_IN_H
  #include <netinet/in.h>
#endif
#ifdef WIN32
  #include <winsock2.h>
  #include <windows.h>
#endif
#include <zlib.h>
#include <errno.h>
#include <assert.h>

#include "common/global.h"
#include "common/strings.h"

#include "bar.h"
#include "archive_format.h"

#include "chunks.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// chunk header definition
LOCAL ChunkDefinition CHUNK_HEADER_DEFINITION[] = {
                                                   CHUNK_DATATYPE_UINT32,offsetof(ChunkHeader,id),
                                                   CHUNK_DATATYPE_UINT64,offsetof(ChunkHeader,size),
                                                   0
                                                  };

#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/

// chunk read/write buffer
typedef struct
{
  ChunkModes      chunkMode;
  const ChunkIO   *chunkIO;
  void            *chunkIOUserData;
  uint64          offset;
  union
  {
    ulong         bytesRead;
    ulong         bytesWritten;
  };

  ChunkDefinition *definition;
  ulong           chunkSize;
  uint            definitionIndex;
  uint            alignment;
  CryptInfo       *cryptInfo;

  byte            *buffer;
  ulong           bufferLength;
  ulong           bufferSize;
  ulong           bufferIndex;
} ChunkBuffer;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : initChunkBuffer
* Purpose: init chunk buffer
* Input  : chunkIO         - i/o functions
*          chunkIOUserData - user data for i/o
*          definition      - chunk definition
*          chunkSize       - chunk size (in bytes)
*          alignment       - chunk alignment
*          cryptInfo       - crypt info
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors initChunkBuffer(ChunkBuffer     *chunkBuffer,
                             ChunkModes      chunkMode,
                             const ChunkIO   *chunkIO,
                             void            *chunkIOUserData,
                             ChunkDefinition *definition,
                             ulong           chunkSize,
                             uint            alignment,
                             CryptInfo       *cryptInfo
                            )
{
  ulong  n;
//  uint   i;
//  bool   doneFlag;
  Errors error;

  assert(chunkBuffer != NULL);
  assert(chunkIO != NULL);
  assert(chunkIO->read != NULL);
  assert(definition != NULL);

  chunkBuffer->chunkMode       = chunkMode;
  chunkBuffer->chunkIO         = chunkIO;
  chunkBuffer->chunkIOUserData = chunkIOUserData;
  chunkBuffer->offset          = 0LL;
  chunkBuffer->bytesRead       = 0L;
  chunkBuffer->bytesWritten    = 0L;

  chunkBuffer->definition      = definition;
  chunkBuffer->chunkSize       = chunkSize;
  chunkBuffer->definitionIndex = 0;
  chunkBuffer->alignment       = alignment;
  chunkBuffer->cryptInfo       = cryptInfo;

  chunkBuffer->bufferLength    = 0L;
  chunkBuffer->bufferSize      = 0L;
  chunkBuffer->bufferIndex     = 0L;

  error = chunkIO->tell(chunkIOUserData,
                        &chunkBuffer->offset
                       );
  if (error != ERROR_NONE)
  {
    return error;
  }

  switch (chunkMode)
  {
    case CHUNK_MODE_READ:
#if 0
// does not work: decryption in parts is not possible because of CTS, thus data must be read and decrypted as a single block
      // get aligned max. data length which can be read initialy
      n        = 0;
      i        = 0;
      doneFlag = FALSE;
      while (   (definition[i] != CHUNK_DATATYPE_NONE)
             && !doneFlag
            )
      {
        switch (definition[i+0])
        {
          case CHUNK_DATATYPE_BYTE:
          case CHUNK_DATATYPE_UINT8:
          case CHUNK_DATATYPE_INT8:
            n += 1L;
            i += 2;
            break;
          case CHUNK_DATATYPE_UINT16:
          case CHUNK_DATATYPE_INT16:
            n += 2L;
            i += 2;
            break;
          case CHUNK_DATATYPE_UINT32:
          case CHUNK_DATATYPE_INT32:
            n += 4L;
            i += 2;
            break;
          case CHUNK_DATATYPE_UINT64:
          case CHUNK_DATATYPE_INT64:
            n += 8L;
            i += 2;
            break;
          case CHUNK_DATATYPE_STRING:
            n += 2L;
            doneFlag = TRUE;
            i += 2;
            break;

          case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
            n += 2L;
            doneFlag = TRUE;
            i += 3;
            break;

          case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
            n += 2L;
            doneFlag = TRUE;
            i += 3;
            break;
          case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
            n += 2L+2L;
            doneFlag = TRUE;
            i += 3;
            break;

          case CHUNK_DATATYPE_CRC32:
            n += 4L;
            i += 2;
            break;

          case CHUNK_DATATYPE_DATA:
            doneFlag = TRUE;
            i += 2;
            break;

          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
          #endif /* NDEBUG */
        }
      }
      n = ALIGN(n,alignment);
#else
      n = ALIGN(chunkSize,alignment);
#endif
      if ((chunkBuffer->bytesRead+n) > chunkSize)
      {
        return ERROR_INVALID_CHUNK_SIZE;
      }

      // allocate initial buffer size
      chunkBuffer->bufferSize = ALIGN(n,1024);
      chunkBuffer->buffer = malloc(chunkBuffer->bufferSize);
      if (chunkBuffer->buffer == NULL)
      {
        return ERROR_INSUFFICIENT_MEMORY;
      }

      // read initial data
      error = chunkIO->read(chunkIOUserData,
                            chunkBuffer->buffer,
                            n,
                            NULL
                           );
      if (error != ERROR_NONE)
      {
        free(chunkBuffer->buffer);
        return error;
      }
      chunkBuffer->bytesRead += n;
//fprintf(stderr,"%s, %d: read raw:\n",__FILE__,__LINE__); debugDumpMemory(chunkBuffer->buffer,n,FALSE);

      // decrypt initial data
      if (cryptInfo != NULL)
      {
        Crypt_reset(cryptInfo);
        error = Crypt_decrypt(cryptInfo,chunkBuffer->buffer,n);
        if (error != ERROR_NONE)
        {
          free(chunkBuffer->buffer);
          return error;
        }
      }
      chunkBuffer->bufferLength += n;
//fprintf(stderr,"%s, %d: read decrypted:\n",__FILE__,__LINE__); debugDumpMemory(chunkBuffer->buffer,n,FALSE);
      break;
    case CHUNK_MODE_WRITE:
      n = ALIGN(1024,alignment);

      // allocate initial buffer size
      chunkBuffer->bufferSize = ALIGN(n,1024);
      chunkBuffer->buffer = malloc(chunkBuffer->bufferSize);
      if (chunkBuffer->buffer == NULL)
      {
        return ERROR_INSUFFICIENT_MEMORY;
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  DEBUG_ADD_RESOURCE_TRACE(chunkBuffer,ChunkBuffer);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : doneChunkBuffer
* Purpose: done chunk buffer
* Input  : chunkBuffer - chunk buffer handle
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors doneChunkBuffer(ChunkBuffer *chunkBuffer)
{
  assert(chunkBuffer != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(chunkBuffer,ChunkBuffer);

  free(chunkBuffer->buffer);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : getChunkBuffer
* Purpose: get chunk data pointer
* Input  : chunkBuffer - chunk buffer handle
*          p           - address of pointer variable
*          size        - size of data
* Output : p - pointer to chunk data
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors getChunkBuffer(ChunkBuffer *chunkBuffer, void **p, ulong size)
{
  ulong  bufferSize;
  void   *buffer;
  ulong  n;
  Errors error;

  assert(chunkBuffer != NULL);
  assert(chunkBuffer->chunkIO != NULL);
  assert(chunkBuffer->chunkIO->read != NULL);

  // read and append data
  if ((chunkBuffer->bufferIndex+size) > chunkBuffer->bufferLength)
  {
    // calculate number of data bytes to append
    n = ALIGN((chunkBuffer->bufferIndex+size)-chunkBuffer->bufferLength,chunkBuffer->alignment);
    if ((chunkBuffer->bytesRead+n) > chunkBuffer->chunkSize)
    {
      return ERROR_INVALID_CHUNK_SIZE;
    }

    // increase buffer size if required
    if ((chunkBuffer->bufferLength+n) > chunkBuffer->bufferSize)
    {
      bufferSize = ALIGN(chunkBuffer->bufferLength+n,1024);
      buffer = realloc(chunkBuffer->buffer,bufferSize);
      if (buffer == NULL)
      {
        return ERROR_INSUFFICIENT_MEMORY;
      }
      chunkBuffer->buffer     = buffer;
      chunkBuffer->bufferSize = bufferSize;
      memClear(&chunkBuffer->buffer[chunkBuffer->bufferLength],chunkBuffer->bufferSize-chunkBuffer->bufferLength);
    }

    // read data
    error = chunkBuffer->chunkIO->read(chunkBuffer->chunkIOUserData,
                                       &chunkBuffer->buffer[chunkBuffer->bufferLength],
                                       n,
                                       NULL
                                      );
    if (error != ERROR_NONE)
    {
      return error;
    }
    chunkBuffer->bytesRead += n;

//fprintf(stderr,"%s, %d: read:\n",__FILE__,__LINE__); debugDumpMemory(FALSE,chunkBuffer->buffer,chunkBuffer->bufferLength+n);

    // decrypt data
    if (chunkBuffer->cryptInfo != NULL)
    {
      // Note: we need to decrypt from start, because it is not possible to decrypt partial data blocks with CTS enabled
      Crypt_reset(chunkBuffer->cryptInfo);
      error = Crypt_decrypt(chunkBuffer->cryptInfo,chunkBuffer->buffer,chunkBuffer->bufferLength+n);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
//fprintf(stderr,"%s, %d: read decrypted:\n",__FILE__,__LINE__); debugDumpMemory(chunkBuffer->buffer,chunkBuffer->bufferLength+n,FALSE);

    chunkBuffer->bufferLength += n;
  }

  // get data
  if (p != NULL) (*p) = &chunkBuffer->buffer[chunkBuffer->bufferIndex];
  chunkBuffer->bufferIndex += size;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : putChunkBuffer
* Purpose: put data into chunk buffer
* Input  : chunkBuffer - chunk buffer handle
*          p           - data pointer
*          size        - size of data
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors putChunkBuffer(ChunkBuffer *chunkBuffer, const void *p, ulong size)
{
  assert(chunkBuffer != NULL);

  // increase buffer size if required
  if ((chunkBuffer->bufferLength+size) > chunkBuffer->bufferSize)
  {
    chunkBuffer->bufferSize = ALIGN(ALIGN(chunkBuffer->bufferLength+size,chunkBuffer->alignment),1024);
    chunkBuffer->buffer = realloc(chunkBuffer->buffer,chunkBuffer->bufferSize);
    if (chunkBuffer->buffer == NULL)
    {
      return ERROR_INSUFFICIENT_MEMORY;
    }
    memClear(&chunkBuffer->buffer[chunkBuffer->bufferLength],chunkBuffer->bufferSize-chunkBuffer->bufferLength);
  }

  // put data
  memcpy(&chunkBuffer->buffer[chunkBuffer->bufferLength],p,size);
  chunkBuffer->bufferLength += size;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : alignChunkBuffer
* Purpose: align chunk data
* Input  : chunkBuffer - chunk buffer handle
*          alignment   - alignment
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors alignChunkBuffer(ChunkBuffer *chunkBuffer, uint alignment)
{
  // max. padding is 16-1 = 15 bytes
  const byte PADDING[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

  Errors error;

  assert(chunkBuffer != NULL);

  error = ERROR_UNKNOWN;
  switch (chunkBuffer->chunkMode)
  {
    case CHUNK_MODE_READ:
      error = getChunkBuffer(chunkBuffer,
                             NULL,
                             ALIGN(chunkBuffer->bufferIndex,alignment)-chunkBuffer->bufferIndex
                            );
      break;
    case CHUNK_MODE_WRITE:
      assert(ALIGN(chunkBuffer->bufferLength,alignment)-chunkBuffer->bufferLength < sizeof(PADDING));
      error = putChunkBuffer(chunkBuffer,
                             PADDING,
                             ALIGN(chunkBuffer->bufferLength,alignment)-chunkBuffer->bufferLength
                            );
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return error;
}

/***********************************************************************\
* Name   : flushChunkBuffer
* Purpose: flush and write chunk buffer
* Input  : chunkBuffer - chunk buffer handle
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors flushChunkBuffer(ChunkBuffer *chunkBuffer)
{
  ulong  n;
  Errors error;

  assert(chunkBuffer != NULL);
  assert(chunkBuffer->chunkIO != NULL);
  assert(chunkBuffer->chunkIO->write != NULL);

  // calculate data bytes
  n = ALIGN(chunkBuffer->bufferLength,chunkBuffer->alignment);
  memClear(&chunkBuffer->buffer[chunkBuffer->bufferLength],n-chunkBuffer->bufferLength);

  // encrypt data
//fprintf(stderr,"%s, %d: write unencrypted:\n",__FILE__,__LINE__); debugDumpMemory(chunkBuffer->buffer,n,FALSE);
  if (chunkBuffer->cryptInfo != NULL)
  {
    Crypt_reset(chunkBuffer->cryptInfo);
    error = Crypt_encrypt(chunkBuffer->cryptInfo,
                          chunkBuffer->buffer,
                          n
                         );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // write data
//fprintf(stderr,"%s, %d: write raw:\n",__FILE__,__LINE__); debugDumpMemory(chunkBuffer->buffer,n,FALSE);
  error = chunkBuffer->chunkIO->write(chunkBuffer->chunkIOUserData,
                                      chunkBuffer->buffer,
                                      n
                                     );
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkBuffer->bytesWritten += n;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : getUint8
* Purpose: get uint8 value
* Input  : p - memory address
* Output : -
* Return : uint8 value
* Notes  : required because p may be not aligned
\***********************************************************************/

LOCAL_INLINE uint8 getUint8(void *p)
{
  uint8 n;

  n = (*((uint8*)p));

  return n;
}

/***********************************************************************\
* Name   : getUint16
* Purpose: get uint16 value
* Input  : p - memory address
* Output : -
* Return : uint16 value
* Notes  : required because p may be not aligned
\***********************************************************************/

LOCAL_INLINE uint16 getUint16(void *p)
{
  uint16 n;

//  n = ntohs(*((uint16*)p));
  n =   ((uint16)(*((byte*)p+0)) <<  0)
      | ((uint16)(*((byte*)p+1)) <<  8);

  return ntohs(n);
}

/***********************************************************************\
* Name   : getUint32
* Purpose: get uint32 value
* Input  : p - memory address
* Output : -
* Return : uint32 value
* Notes  : required because p may be not aligned
\***********************************************************************/

LOCAL_INLINE uint32 getUint32(void *p)
{
  uint32 n;

//  n = ntohl(*(uint32*)p);
  n =   ((uint32)(*((byte*)p+0)) <<  0)
      | ((uint32)(*((byte*)p+1)) <<  8)
      | ((uint32)(*((byte*)p+2)) << 16)
      | ((uint32)(*((byte*)p+3)) << 24);

  return ntohl(n);
}

/***********************************************************************\
* Name   : getUint64
* Purpose: get uint64 value
* Input  : p - memory address
* Output : -
* Return : uint64 value
* Notes  : required because p may be not aligned
\***********************************************************************/

LOCAL_INLINE uint64 getUint64(void *p)
{
  uint32 h,l;

//            h = ntohl(*((uint32*)p+0));
//            l = ntohl(*((uint32*)p+1));
//            n = (((uint64)h) << 32) | (((uint64)l << 0));
  h = getUint32((byte*)p+0);
  l = getUint32((byte*)p+4);

  return (((uint64)h) << 32) | (((uint64)l) << 0);
}

/***********************************************************************\
* Name   : initDefinition
* Purpose: init chunk definition
* Input  : definition - chunk definition
*          chunkData  - chunk data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL void initDefinition(ChunkDefinition *definition,
                          void            *chunkData
                         )
#else /* not NDEBUG */
LOCAL void initDefinition(const char      *__fileName__,
                          ulong           __lineNb__,
                          ChunkDefinition *definition,
                          void            *chunkData
                         )
#endif /* NDEBUG */
{
  uint i;

  if (definition != NULL)
  {
    i = 0;
    while (definition[i+0] != CHUNK_DATATYPE_NONE)
    {
      switch (definition[i+0])
      {
        case CHUNK_DATATYPE_BYTE:
        case CHUNK_DATATYPE_UINT8:
        case CHUNK_DATATYPE_INT8:
          // init 8bit value
          (*((uint8*)((byte*)chunkData+definition[i+1]))) = 0;
          i += 2;
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          // init 16bit value
          (*((uint16*)((byte*)chunkData+definition[i+1]))) = 0;
          i += 2;
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          // init 32bit value
          (*((uint32*)((byte*)chunkData+definition[i+1]))) = 0L;
          i += 2;
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          // init 64bit value
          (*((uint64*)((byte*)chunkData+definition[i+1]))) = 0LL;
          i += 2;
          break;
        case CHUNK_DATATYPE_STRING:
          {
            String string;

            string = String_new();
            if (string == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }

            // init string value
            (*((String*)((byte*)chunkData+definition[i+1]))) = string;

            i += 2;
          }
          break;
        case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          memClear((byte*)chunkData+definition[i+2],definition[i+1]*sizeof(byte));
          i += 3;
          break;
        case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          memClear((byte*)chunkData+definition[i+2],definition[i+1]*sizeof(uint8));
          i += 3;
          break;
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          memClear((byte*)chunkData+definition[i+2],definition[i+1]*sizeof(uint16));
          i += 3;
          break;
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          memClear((byte*)chunkData+definition[i+2],definition[i+1]*sizeof(uint32));
          i += 3;
          break;
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          memClear((byte*)chunkData+definition[i+2],definition[i+1]*sizeof(uint64));
          i += 3;
          break;
        case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          (*((uint* )((byte*)chunkData+definition[i+1]))) = 0;
          (*((void**)((byte*)chunkData+definition[i+2]))) = NULL;
          i += 3;
          break;
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          memClear((byte*)chunkData+definition[i+2],definition[i+1]*sizeof(String*));
          i += 3;
          break;
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          (*((uint*   )((byte*)chunkData+definition[i+1]))) = 0;
          (*((String**)((byte*)chunkData+definition[i+2]))) = NULL;
          i += 3;
          break;
        case CHUNK_DATATYPE_CRC32:
          i += 2;
          break;
        case CHUNK_DATATYPE_DATA:
          i += 2;
          break;
        case CHUNK_ALIGN:
          i += 2;
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
    }
  }

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(chunkData,ChunkData);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,chunkData,ChunkData);
  #endif /* NDEBUG */
}

/***********************************************************************\
* Name   : doneDefinition
* Purpose: done chunk definition
* Input  : definition - chunk definition
*          chunkData  - chunk data
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL Errors doneDefinition(ChunkDefinition *definition,
                            const void      *chunkData
                           )
#else /* not NDEBUG */
LOCAL Errors doneDefinition(const char      *__fileName__,
                            ulong           __lineNb__,
                            ChunkDefinition *definition,
                            const void      *chunkData
                           )
#endif /* NDEBUG */
{
  uint i;

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(chunkData,ChunkData);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,chunkData,ChunkData);
  #endif /* NDEBUG */

  if (definition != NULL)
  {
    i = 0;
    while (definition[i+0] != CHUNK_DATATYPE_NONE)
    {
      switch (definition[i+0])
      {
        case CHUNK_DATATYPE_BYTE:
        case CHUNK_DATATYPE_UINT8:
        case CHUNK_DATATYPE_INT8:
          i += 2;
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          i += 2;
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          i += 2;
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          i += 2;
          break;
        case CHUNK_DATATYPE_STRING:
          String_delete(*((String*)((byte*)chunkData+definition[i+1])));
          i += 2;
          break;
        case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          i += 3;
          break;
        case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          i += 3;
          break;
        case CHUNK_DATATYPE_CRC32:
          i += 2;
          break;
        case CHUNK_DATATYPE_DATA:
          i += 2;
          break;
        case CHUNK_ALIGN:
          i += 2;
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : getDefinitionSize
* Purpose: get chunk size
* Input  : definition - chunk definition
*          alignment  - chunk alignment
*          chunkData  - chunk data (can be NULL to get fixed size only)
*          dataLength - length of data or 0
* Output : -
* Return : chunk size [bytes]
* Notes  : -
\***********************************************************************/

LOCAL ulong getDefinitionSize(ChunkDefinition *definition,
                              uint            alignment,
                              const void      *chunkData,
                              ulong           dataLength
                             )
{
  ulong size;
  int   i;

  assert(definition != NULL);

  size = 0L;

  if (definition != NULL)
  {
    i = 0;
    while (definition[i+0] != CHUNK_DATATYPE_NONE)
    {
      switch (definition[i+0])
      {
        case CHUNK_DATATYPE_BYTE:
        case CHUNK_DATATYPE_UINT8:
        case CHUNK_DATATYPE_INT8:
          size += sizeof(uint8);

          i += 2;
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          size += sizeof(uint16);

          i += 2;
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          size += sizeof(uint32);

          i += 2;
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          size += sizeof(uint64);

          i += 2;
          break;
        case CHUNK_DATATYPE_STRING:
          {
            String s;

            assert(chunkData != NULL);

            s = (*((String*)((byte*)chunkData+definition[i+1])));
            assert(s != NULL);
            size += sizeof(uint16)+String_length(s);

            i += 2;
          }
          break;

        case CHUNK_DATATYPE_BYTE |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT8|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          {
            uint length;

            length = (uint)definition[i+1];
            size += ALIGN(length*sizeof(uint8),sizeof(uint32));

            i += 3;
          }
          break;
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          {
            uint length;

            length = (uint)definition[i+1];
            size += ALIGN(length*sizeof(uint16),sizeof(uint32));

            i += 3;
          }
          break;
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          {
            uint length;

            length = (uint)definition[i+1];
            size += ALIGN(length*sizeof(uint32),sizeof(uint32));

            i += 3;
          }
          break;
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          {
            uint length;

            length = (uint)definition[i+1];
            size += ALIGN(length*sizeof(uint64),sizeof(uint32));

            i += 3;
          }
          break;
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          {
            uint   length;
            String s;

            assert(chunkData != NULL);

            length = (uint)definition[i+1];
            while (length > 0)
            {
              s = (*((String*)((byte*)chunkData+definition[i+1])));
              assert(s != NULL);
              size += sizeof(uint16)+String_length(s);
            }

            i += 3;
          }
          break;
        case CHUNK_DATATYPE_BYTE |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT8|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          {
            uint length;

            assert(chunkData != NULL);

            length = (*((uint*)((byte*)chunkData+definition[i+1])));
            size += sizeof(uint16)+ALIGN(length*sizeof(uint8),sizeof(uint32));

            i += 3;
          }
          break;
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          {
            uint length;

            assert(chunkData != NULL);

            length = (*((uint*)((byte*)chunkData+definition[i+1])));
            size += sizeof(uint16)+ALIGN(length*sizeof(uint16),sizeof(uint32));

            i += 3;
          }
          break;
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          {
            uint length;

            assert(chunkData != NULL);

            length = (*((uint*)((byte*)chunkData+definition[i+1])));
            size += sizeof(uint16)+ALIGN(length*sizeof(uint32),sizeof(uint32));

            i += 3;
          }
          break;
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          {
            uint length;

            assert(chunkData != NULL);

            length = (*((uint*)((byte*)chunkData+definition[i+1])));
            size += sizeof(uint16)+ALIGN(length*sizeof(uint64),sizeof(uint32));

            i += 3;
          }
          break;
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          {
            uint   length;
            String s;

            assert(chunkData != NULL);

            length = (*((uint*)((byte*)chunkData+definition[i+1])));
            size += sizeof(uint16);
            while (length > 0)
            {
              s = (*((String*)((byte*)chunkData+definition[i+1])));
              assert(s != NULL);
              size += sizeof(uint16)+String_length(s);
            }

            i += 3;
          }
          break;

        case CHUNK_DATATYPE_CRC32:
          size += sizeof(uint32);

          i += 2;
          break;

        case CHUNK_DATATYPE_DATA:
          size += dataLength;

          i += 2;
          break;

        case CHUNK_ALIGN:
          {
            uint   alignment;

            assert(chunkData != NULL);

            alignment = (*((uint*)((byte*)chunkData+definition[i+1])));
            size = ALIGN(size,alignment);

            i += 2;
          }
          break;

        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
    }
  }

  // align size
  size = ALIGN(size,alignment);

  return size;
}

/***********************************************************************\
* Name   : resetDefinition
* Purpose: reset resources of chunk definition
* Input  : definition - chunk definition
*          chunkData  - chunk data
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL void resetDefinition(ChunkDefinition *definition,
                           void            *chunkData
                          )
{
  uint i;

  if (definition != NULL)
  {
    i = 0;
    while (definition[i+0] != CHUNK_DATATYPE_NONE)
    {
      switch (definition[i+0])
      {
        case CHUNK_DATATYPE_BYTE:
        case CHUNK_DATATYPE_UINT8:
        case CHUNK_DATATYPE_INT8:
          // init 8bit value
          (*((uint8*)((byte*)chunkData+definition[i+1]))) = 0;
          i += 2;
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          // init 16bit value
          (*((uint16*)((byte*)chunkData+definition[i+1]))) = 0;
          i += 2;
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          // init 32bit value
          (*((uint32*)((byte*)chunkData+definition[i+1]))) = 0L;
          i += 2;
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          // init 64bit value
          (*((uint64*)((byte*)chunkData+definition[i+1]))) = 0LL;
          i += 2;
          break;
        case CHUNK_DATATYPE_STRING:
          String_clear(*((String*)((byte*)chunkData+definition[i+1])));
          i += 2;
          break;
        case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          {
//TODO
#if 0
            uint arrayLength;
            void *arrayData;

            arrayLength = (*((uint* )((byte*)chunkData+definition[i+1])));
            arrayData   = (*((void**)((byte*)chunkData+definition[i+2])));
            if (arrayLength > 0)
            {
              assert(arrayData != NULL);
              free(arrayData);
            }
#endif
//            (*((uint* )((byte*)chunkData+definition[i+1]))) = 0;
//            (*((void**)((byte*)chunkData+definition[i+2]))) = NULL;
            i += 3;
          }
          break;
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          {
            uint   arrayLength;
            String *strings;
            uint   z;

            arrayLength = (uint)definition[i+1];
            strings     = (*((String**)((byte*)chunkData+definition[i+2])));
            assert(strings != NULL);
            for (z = 0; z < arrayLength; z++)
            {
              String_delete(strings[z]);
              strings[z] = NULL;
            }
            i += 3;
          }
          break;
        case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          {
            uint arrayLength;
            void *arrayData;

            arrayLength = (*((uint* )((byte*)chunkData+definition[i+1])));
            arrayData   = (*((void**)((byte*)chunkData+definition[i+2])));
            if (arrayLength > 0)
            {
              assert(arrayData != NULL);
              free(arrayData);
            }
            (*((uint* )((byte*)chunkData+definition[i+1]))) = 0;
            (*((void**)((byte*)chunkData+definition[i+2]))) = NULL;
            i += 3;
          }
          break;
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          {
            uint   arrayLength;
            String *strings;
            uint   z;

            arrayLength = (*((uint*   )((byte*)chunkData+definition[i+1])));
            strings     = (*((String**)((byte*)chunkData+definition[i+2])));
            if (arrayLength > 0)
            {
              assert(strings != NULL);
              for (z = 0; z < arrayLength; z++)
              {
                String_delete(strings[z]);
              }
              free(strings);
            }
            (*((uint* )((byte*)chunkData+definition[i+1]))) = 0;
            (*((void**)((byte*)chunkData+definition[i+2]))) = NULL;
            i += 3;
          }
          break;
        case CHUNK_DATATYPE_CRC32:
          (*((uint32*)((byte*)chunkData+definition[i+1]))) = 0L;
          i += 2;
          break;
        case CHUNK_DATATYPE_DATA:
          i += 2;
          break;
        case CHUNK_ALIGN:
          i += 2;
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
    }
  }
}

/***********************************************************************\
* Name   : readDefinition
* Purpose: read chunk definition
* Input  : chunkIO         - i/o functions
*          chunkIOUserData - user data for i/o
*          definition      - chunk definition
*          chunkSize       - size of chunk (in bytes)
*          alignment       - chunk alignment
*          cryptInfo       - crypt info
*          chunkData       - chunk data buffer
* Output : chunkData - chunk data
*          bytesRead - number of bytes read
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors readDefinition(const ChunkIO   *chunkIO,
                            void            *chunkIOUserData,
                            ChunkDefinition *definition,
                            ulong           chunkSize,
                            uint            alignment,
                            CryptInfo       *cryptInfo,
                            void            *chunkData,
                            ulong           *bytesRead
                           )
{
  Errors      error;
  uint64      offset;
  ChunkBuffer chunkBuffer;
  uLong       crc;
  uint        i;
  void        *p;

  assert(chunkIO != NULL);
  assert(chunkIO->tell != NULL);
  assert(chunkIO->seek != NULL);

  // initialize variables
  if (bytesRead != NULL) (*bytesRead) = 0L;

  if (definition != NULL)
  {
    // get current offet
    error = chunkIO->tell(chunkIOUserData,&offset);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // init chunk buffer
    error = initChunkBuffer(&chunkBuffer,
                            CHUNK_MODE_READ,
                            chunkIO,
                            chunkIOUserData,
                            definition,
                            chunkSize,
                            alignment,
                            cryptInfo
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // read definition
    crc = crc32(0,Z_NULL,0);
    i   = 0;
    while (   (definition[i+0] != CHUNK_DATATYPE_NONE)
           && (error == ERROR_NONE)
          )
    {
      switch (definition[i+0])
      {
        case CHUNK_DATATYPE_BYTE:
        case CHUNK_DATATYPE_UINT8:
        case CHUNK_DATATYPE_INT8:
          {
            uint8 n;

            // get 8bit value
            error = getChunkBuffer(&chunkBuffer,&p,1L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p,1);
            n = getUint8(p);

            (*((uint8*)((byte*)chunkData+definition[i+1]))) = n;

            i += 2;
          }
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          {
            uint16 n;

            // get 16bit value
            error = getChunkBuffer(&chunkBuffer,&p,2L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p,2);
            n = getUint16(p);

            (*((uint16*)((byte*)chunkData+definition[i+1]))) = n;

            i += 2;
          }
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          {
            uint32 n;

            // get 32bit value
            error = getChunkBuffer(&chunkBuffer,&p,4L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p,4);
            n = getUint32(p);

            (*((uint32*)((byte*)chunkData+definition[i+1]))) = n;

            i += 2;
          }
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          {
            uint64 n;

            // get 64bit value
            error = getChunkBuffer(&chunkBuffer,&p,8L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p,8);
            n = getUint64(p);

            (*((uint64*)((byte*)chunkData+definition[i+1]))) = n;

            i += 2;
          }
          break;
        case CHUNK_DATATYPE_STRING:
          {
            uint16 stringLength;
            String string;

            // get string length (16bit value)
            error = getChunkBuffer(&chunkBuffer,&p,2L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p,2);
            stringLength = getUint16(p);

            // get string data
            error = getChunkBuffer(&chunkBuffer,&p,(ulong)stringLength);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p,stringLength);

            string = (*((String*)((byte*)chunkData+definition[i+1])));
            assert(string != NULL);
            String_setBuffer(string,p,stringLength);

            i += 2;
          }
          break;

        case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          {
            uint arrayLength;
            void *arrayData;

            // get array length
            arrayLength = (uint)definition[i+1];

            switch (definition[i+0])
            {
              case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
                {
                  ulong size;

                  size = 0L;
                  switch (definition[i+0])
                  {
                    case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
                    case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
                    case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED: size = 1L; break;
                    case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
                    case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED: size = 2L; break;
                    case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
                    case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED: size = 4L; break;
                    case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
                    case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED: size = 8L; break;
                  }

                  // get array data
                  error = getChunkBuffer(&chunkBuffer,&p,(ulong)arrayLength*size);
                  if (error != ERROR_NONE) break;
                  crc = crc32(crc,p,(ulong)arrayLength*size);

                  arrayData = (void*)((byte*)chunkData+definition[i+2]);
                  memcpy(arrayData,p,(ulong)arrayLength*size);
                }
                break;
              case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
                {
                  String *strings;
                  uint   z;
                  uint16 stringLength;

                  // get string data
                  strings = (String*)((byte*)chunkData+definition[i+2]);
                  for (z = 0; z < arrayLength; z++)
                  {
                    // get string length (16bit value)
                    error = getChunkBuffer(&chunkBuffer,&p,2L);
                    if (error != ERROR_NONE) break;
                    crc = crc32(crc,p,2);
                    stringLength = getUint16(p);

                    // get string data
                    error = getChunkBuffer(&chunkBuffer,&p,(ulong)stringLength);
                    if (error != ERROR_NONE) break;
                    crc = crc32(crc,p,stringLength);
                    strings[z] = String_newBuffer(p,stringLength);
                  }

                  (*((void**)((byte*)chunkData+definition[i+2]))) = strings;
                }
                break;
            }
            if (error != ERROR_NONE) break;

            i += 3;
          }
          break;

        case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          {
            uint16 arrayLength;
            void   *arrayData;

            // get array length (16bit value)
            error = getChunkBuffer(&chunkBuffer,&p,2L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p,2);
            arrayLength = getUint16(p);

            switch (definition[i+0])
            {
              case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
                {
                  ulong size;

                  size = 0L;
                  switch (definition[i+0])
                  {
                    case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
                    case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
                    case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC: size = 1L; break;
                    case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
                    case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC: size = 2L; break;
                    case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
                    case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC: size = 4L; break;
                    case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
                    case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC: size = 8L; break;
                  }

                  // get array data
                  error = getChunkBuffer(&chunkBuffer,&p,(ulong)arrayLength*size);
                  if (error != ERROR_NONE) break;
                  crc = crc32(crc,p,(ulong)arrayLength*size);

                  arrayData = (*((void**)((byte*)chunkData+definition[i+2])));
                  arrayData = realloc(arrayData,(ulong)arrayLength*size);
                  if (arrayData == NULL)
                  {
                    error = ERRORX_(CORRUPT_DATA,0,"insufficient memory: %lubytes",(ulong)arrayLength*size);
                    break;
                  }
                  memcpy(arrayData,p,(ulong)arrayLength*size);

                  (*((uint* )((byte*)chunkData+definition[i+1]))) = (uint)arrayLength;
                  (*((void**)((byte*)chunkData+definition[i+2]))) = arrayData;
                }
                break;
              case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
                {
                  String *strings;
                  uint   z;
                  uint16 stringLength;

                  // allocate string array
                  strings = (*((String**)((byte*)chunkData+definition[i+2])));
                  strings = realloc(strings,(ulong)arrayLength*sizeof(String));
                  if (strings == NULL)
                  {
                    error = ERRORX_(CORRUPT_DATA,0,"insufficient memory: %lubytes",(ulong)arrayLength*sizeof(String));
                    break;
                  }

                  // get array data
                  for (z = 0; z < arrayLength; z++)
                  {
                    // get string length (16bit value)
                    error = getChunkBuffer(&chunkBuffer,&p,2L);
                    if (error != ERROR_NONE) break;
                    crc = crc32(crc,p,2);
                    stringLength = getUint16(p);

                    // get string data
                    error = getChunkBuffer(&chunkBuffer,&p,(ulong)stringLength);
                    if (error != ERROR_NONE) break;
                    crc = crc32(crc,p,stringLength);
                    strings[z] = String_newBuffer(p,stringLength);
                  }

                  (*((uint* )((byte*)chunkData+definition[i+1]))) = (uint)arrayLength;
                  (*((void**)((byte*)chunkData+definition[i+2]))) = strings;
                }
                break;
            }
            if (error != ERROR_NONE) break;

            i += 3;
          }
          break;

        case CHUNK_DATATYPE_CRC32:
          {
            uint32 n;

            // get 32bit value
            error = getChunkBuffer(&chunkBuffer,&p,4L);
            if (error != ERROR_NONE) break;
            n = getUint32(p);

            // check crc
            if (n != crc)
            {
              error = ERRORX_(CRC_,0,"%"PRIu64,offset);
              break;
            }

            // reset crc
            crc = crc32(0,Z_NULL,0);

            i += 2;
          }
          break;

        case CHUNK_DATATYPE_DATA:
          i += 2;
          break;

        case CHUNK_ALIGN:
          {
            error = alignChunkBuffer(&chunkBuffer,definition[i+1]);
            if (error != ERROR_NONE) break;

            i += 2;
          }
          break;

        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
    }
    if (error != ERROR_NONE)
    {
      doneChunkBuffer(&chunkBuffer);
      chunkIO->seek(chunkIOUserData,offset);
      return error;
    }

    // store number of bytes read
    if (bytesRead != NULL) (*bytesRead) = chunkBuffer.bytesRead;

    // free resources
    doneChunkBuffer(&chunkBuffer);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeDefinition
* Purpose: write chunk definition
* Input  : chunkIO         - i/o functions
*          chunkIOUserData - user data for i/o
*          definition      - chunk definition
*          chunkSize       - chunk size (in bytes)
*          alignment       - chunk alignment
*          cryptInfo       - crypt info
*          chunkData       - chunk data
* Output : bytesWritten - number of bytes written
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors writeDefinition(const ChunkIO   *chunkIO,
                             void            *chunkIOUserData,
                             ChunkDefinition *definition,
                             uint            chunkSize,
                             uint            alignment,
                             CryptInfo       *cryptInfo,
                             const void      *chunkData,
                             ulong           *bytesWritten
                            )
{
  ChunkBuffer chunkBuffer;
  uLong       crc;
  int         i;
  union
  {
    uint8  u8;
    uint16 u16;
    uint32 u32;
    uint32 u64[2];
    byte   data[8];
  } p;
  Errors      error;

  assert(chunkIO != NULL);
  assert(definition != NULL);

  // initialize variables
  if (bytesWritten != NULL) (*bytesWritten) = 0L;

  if (definition != NULL)
  {
    // init chunk buffer
    error = initChunkBuffer(&chunkBuffer,
                            CHUNK_MODE_WRITE,
                            chunkIO,
                            chunkIOUserData,
                            definition,
                            chunkSize,
                            alignment,
                            cryptInfo
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // write definition
    crc = crc32(0,Z_NULL,0);
    i   = 0;
    while (definition[i+0] != CHUNK_DATATYPE_NONE)
    {
      switch (definition[i+0])
      {
        case CHUNK_DATATYPE_BYTE:
        case CHUNK_DATATYPE_UINT8:
        case CHUNK_DATATYPE_INT8:
          {
            uint8 n;

            n = (*((uint8*)((byte*)chunkData+definition[i+1])));

            // put 8bit value
            p.u8 = n;
            error = putChunkBuffer(&chunkBuffer,p.data,1L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p.data,1);

            i += 2;
          }
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          {
            uint16 n;

            n = (*((uint16*)((byte*)chunkData+definition[i+1])));
//Note: for some reason Valgrind says here n is undefined, but it is not.

            // put 16bit value
            p.u16 = htons(n);
            error = putChunkBuffer(&chunkBuffer,p.data,2L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p.data,2);

            i += 2;
          }
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          {
            uint32 n;

            n = (*((uint32*)((byte*)chunkData+definition[i+1])));

            // put 32bit value
            p.u32 = htonl(n);
            error = putChunkBuffer(&chunkBuffer,p.data,4L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p.data,4);

            i += 2;
          }
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          {
            uint64 n;
            uint32 h,l;

            n = (*((uint64*)((byte*)chunkData+definition[i+1])));

            // put 64bit value
            h = (n & 0xFFFFffff00000000LL) >> 32;
            l = (n & 0x00000000FFFFffffLL) >>  0;
            p.u64[0] = htonl(h);
            p.u64[1] = htonl(l);
            error = putChunkBuffer(&chunkBuffer,p.data,8L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p.data,8);

            i += 2;
          }
          break;
        case CHUNK_DATATYPE_STRING:
          {
            String     string;
            const void *stringData;
            uint16     stringLength;

            string       = (*((String*)((byte*)chunkData+definition[i+1])));
            stringData   = String_cString(string);
            stringLength = (uint16)String_length(string);

            // put string length (16bit value)
            p.u16 = htons(stringLength);
            error = putChunkBuffer(&chunkBuffer,p.data,2L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p.data,2);

            // put string data
            error = putChunkBuffer(&chunkBuffer,stringData,(ulong)stringLength);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,stringData,stringLength);

            i += 2;
          }
          break;

        case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
          {
            uint       arrayLength;
            const void *arrayData;
            const void *stringData;
            uint16     stringLength;

            arrayLength = (uint)definition[i+1];
            arrayData   = (void*)((byte*)chunkData+definition[i+2]);

            switch (definition[i+0])
            {
              case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
              case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
                {
                  ulong size;

                  size = 0L;
                  switch (definition[i+0])
                  {
                    case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
                    case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
                    case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED: size = 1L; break;
                    case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
                    case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED: size = 2L; break;
                    case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
                    case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED: size = 4L; break;
                    case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
                    case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED: size = 8L; break;
                  }

                  // put array data
                  error = putChunkBuffer(&chunkBuffer,arrayData,(ulong)arrayLength*size);
                  if (error != ERROR_NONE) break;
                  crc = crc32(crc,arrayData,arrayLength*size);
                }
                break;
              case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_FIXED:
                {
                  String *strings;
                  uint   z;

                  // put string data
                  strings = (String*)arrayData;
                  for (z = 0; z < arrayLength; z++)
                  {
                    stringLength = (uint16)String_length(strings[z]);
                    stringData   = String_cString(strings[z]);

                    // put string length (16bit value)
                    p.u16 = htons(stringLength);
                    error = putChunkBuffer(&chunkBuffer,p.data,2L);
                    if (error != ERROR_NONE) break;
                    crc = crc32(crc,p.data,2);

                    // put string data
                    error = putChunkBuffer(&chunkBuffer,stringData,(ulong)stringLength);
                    if (error != ERROR_NONE) break;
                    crc = crc32(crc,stringData,stringLength);
                  }
                }
                break;
            }
            if (error != ERROR_NONE) break;

            i += 3;
          }
          break;

        case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
          {
            uint16     arrayLength;
            const void *arrayData;
            const void *stringData;
            uint16     stringLength;

            arrayLength = (*((uint* )((byte*)chunkData+definition[i+1])));
            arrayData   = (*((void**)((byte*)chunkData+definition[i+2])));

            // put array length (16bit value)
            p.u16 = htons(arrayLength);
            error = putChunkBuffer(&chunkBuffer,p.data,2L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p.data,2);

            switch (definition[i+0])
            {
              case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
              case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
                {
                  ulong size;

                  size = 0L;
                  switch (definition[i+0])
                  {
                    case CHUNK_DATATYPE_BYTE  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
                    case CHUNK_DATATYPE_UINT8 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
                    case CHUNK_DATATYPE_INT8  |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC: size = 1L; break;
                    case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
                    case CHUNK_DATATYPE_INT16 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC: size = 2L; break;
                    case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
                    case CHUNK_DATATYPE_INT32 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC: size = 4L; break;
                    case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
                    case CHUNK_DATATYPE_INT64 |CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC: size = 8L; break;
                  }

                  // put array data
                  error = putChunkBuffer(&chunkBuffer,arrayData,(ulong)arrayLength*size);
                  if (error != ERROR_NONE) break;
                  crc = crc32(crc,arrayData,arrayLength*size);
                }
                break;
              case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY|CHUNK_DATATYPE_DYNAMIC:
                {
                  String *strings;
                  uint   z;

                  // put string data
                  strings = (String*)arrayData;
                  for (z = 0; z < arrayLength; z++)
                  {
                    stringLength = (uint16)String_length(strings[z]);
                    stringData   = String_cString(strings[z]);

                    // put string length (16bit value)
                    p.u16 = htons(stringLength);
                    error = putChunkBuffer(&chunkBuffer,p.data,2L);
                    if (error != ERROR_NONE) break;
                    crc = crc32(crc,p.data,2);

                    // put string data
                    error = putChunkBuffer(&chunkBuffer,stringData,(ulong)stringLength);
                    if (error != ERROR_NONE) break;
                    crc = crc32(crc,stringData,stringLength);
                  }
                }
                break;
            }
            if (error != ERROR_NONE) break;

            i += 3;
          }
          break;

        case CHUNK_DATATYPE_CRC32:
          {
            // put crc (32bit value)
            p.u32 = htonl(crc);
            error = putChunkBuffer(&chunkBuffer,p.data,4);
            if (error != ERROR_NONE) break;

            crc = crc32(0,Z_NULL,0);

            i += 2;
          }
          break;

        case CHUNK_DATATYPE_DATA:
          i += 2;
          break;

        case CHUNK_ALIGN:
          {
            error = alignChunkBuffer(&chunkBuffer,definition[i+1]);
            if (error != ERROR_NONE) break;

            i += 2;
          }
          break;

        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
    }

    // write data
    error = flushChunkBuffer(&chunkBuffer);
    if (error != ERROR_NONE)
    {
      doneChunkBuffer(&chunkBuffer);
      return error;
    }
    if (bytesWritten != NULL) (*bytesWritten) = chunkBuffer.bytesWritten;

    // free resources
    doneChunkBuffer(&chunkBuffer);
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Chunk_initAll(void)
{
  return ERROR_NONE;
}

void Chunk_doneAll(void)
{
}

const char *Chunk_idToString(ChunkId chunkId)
{
  static char s[5];
  char   ch;

  ch = (char)((chunkId & 0xFF000000) >> 24); s[0] = (isprint(ch)) ? ch : '.';
  ch = (char)((chunkId & 0x00FF0000) >> 16); s[1] = (isprint(ch)) ? ch : '.';
  ch = (char)((chunkId & 0x0000FF00) >>  8); s[2] = (isprint(ch)) ? ch : '.';
  ch = (char)((chunkId & 0x000000FF) >>  0); s[3] = (isprint(ch)) ? ch : '.';
  s[4] = '\0';

  return s;
}

ulong Chunk_getSize(const ChunkInfo *chunkInfo,
                    const void      *chunkData,
                    ulong           dataLength
                   )
{
  assert(chunkInfo != NULL);

  return getDefinitionSize(chunkInfo->definition,
                           chunkInfo->alignment,
                           chunkData,
                           dataLength
                          );
}

#ifdef NDEBUG
Errors Chunk_init(ChunkInfo       *chunkInfo,
                  ChunkInfo       *parentChunkInfo,
                  const ChunkIO   *chunkIO,
                  void            *chunkIOUserData,
                  ChunkId         chunkId,
                  ChunkDefinition *definition,
                  uint            alignment,
                  CryptInfo       *cryptInfo,
                  void            *data
                 )
#else /* not NDEBUG */
Errors __Chunk_init(const char      *__fileName__,
                    ulong           __lineNb__,
                    ChunkInfo       *chunkInfo,
                    ChunkInfo       *parentChunkInfo,
                    const ChunkIO   *chunkIO,
                    void            *chunkIOUserData,
                    ChunkId         chunkId,
                    ChunkDefinition *definition,
                    uint            alignment,
                    CryptInfo       *cryptInfo,
                    void            *data
                   )
#endif /* NDEBUG */
{
  assert(chunkInfo != NULL);

  chunkInfo->parentChunkInfo = parentChunkInfo;
  chunkInfo->io              = (parentChunkInfo != NULL) ? parentChunkInfo->io         : chunkIO;
  chunkInfo->ioUserData      = (parentChunkInfo != NULL) ? parentChunkInfo->ioUserData : chunkIOUserData;

  chunkInfo->mode            = CHUNK_MODE_UNKNOWN;
  chunkInfo->alignment       = alignment;
  chunkInfo->cryptInfo       = cryptInfo;

  #ifndef NDEBUG
    chunkInfo->id            = (globalOptions.debug.createArchiveErrors > 0) ? CHUNK_ID_BROKEN : chunkId;
    if (globalOptions.debug.createArchiveErrors > 0) globalOptions.debug.createArchiveErrors--;
  #else
    chunkInfo->id            = chunkId;
  #endif

  chunkInfo->definition      = definition;
  chunkInfo->chunkSize       = 0L;
  chunkInfo->size            = 0LL;
  chunkInfo->offset          = 0LL;
  chunkInfo->index           = 0LL;

  chunkInfo->data            = data;

  #ifdef NDEBUG
    initDefinition(chunkInfo->definition,chunkInfo->data);
  #else /* not NDEBUG */
    initDefinition(__fileName__,__lineNb__,chunkInfo->definition,chunkInfo->data);
  #endif /* NDEBUG */

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(chunkInfo,ChunkInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,chunkInfo,ChunkInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
void Chunk_done(ChunkInfo *chunkInfo)
#else /* not NDEBUG */
void __Chunk_done(const char *__fileName__,
                  ulong      __lineNb__,
                  ChunkInfo  *chunkInfo
                 )
#endif /* NDEBUG */
{
  assert(chunkInfo != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(chunkInfo,ChunkInfo);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,chunkInfo,ChunkInfo);
  #endif /* NDEBUG */

  if (chunkInfo->mode == CHUNK_MODE_READ)
  {
    resetDefinition(chunkInfo->definition,chunkInfo->data);
  }
  #ifdef NDEBUG
    doneDefinition(chunkInfo->definition,chunkInfo->data);
  #else /* not NDEBUG */
    doneDefinition(__fileName__,__lineNb__,chunkInfo->definition,chunkInfo->data);
  #endif /* NDEBUG */
}

Errors Chunk_next(const ChunkIO *chunkIO,
                  void          *chunkIOUserData,
                  ChunkHeader   *chunkHeader
                 )
{
  Errors                   error;
  uint64                   offset;
  const ChunkTransformInfo *chunkTransformInfo;

  assert(chunkIO != NULL);
  assert(chunkIO->tell != NULL);
  assert(chunkHeader != NULL);

  // get current offset
  error = chunkIO->tell(chunkIOUserData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkHeader->offset = offset;

  // read chunk header
  error = readDefinition(chunkIO,
                         chunkIOUserData,
                         CHUNK_HEADER_DEFINITION,
                         CHUNK_HEADER_SIZE,
                         0,
                         NULL,
                         chunkHeader,
                         NULL
                        );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // find transform chunk function (if any)
  chunkHeader->transformInfo = NULL;
  chunkTransformInfo = &CHUNK_TRANSFORM_INFOS[0];
  while (chunkTransformInfo->old.id != CHUNK_ID_NONE)
  {
    if (chunkTransformInfo->old.id == chunkHeader->id)
    {
      chunkHeader->id            = chunkTransformInfo->new.id;
      chunkHeader->transformInfo = chunkTransformInfo;
//fprintf(stderr,"%s, %d: --- transform function %x %x\n",__FILE__,__LINE__,chunkHeader->id,chunkHeader->transformInfo);
      break;
    }
    chunkTransformInfo++;
  }

  return ERROR_NONE;
}

Errors Chunk_skip(const ChunkIO     *chunkIO,
                  void              *chunkIOUserData,
                  const ChunkHeader *chunkHeader
                 )
{
  int64  size;
  uint64 offset;
  Errors error;

  assert(chunkIO != NULL);
  assert(chunkIO->getSize != NULL);
  assert(chunkIO->seek != NULL);
  assert(chunkHeader != NULL);

  size = chunkIO->getSize(chunkIOUserData);
  if ((size >= 0) && (chunkHeader->offset+CHUNK_HEADER_SIZE+chunkHeader->size > (uint64)size))
  {
    return ERROR_INVALID_CHUNK_SIZE;
  }

  /* Note: fseeko in File_seek() cause an SigSegV if "offset" is
     completely wrong; thus never call it with an invalid offset
  */
  offset = ((size < 0LL) || (chunkHeader->offset+CHUNK_HEADER_SIZE+chunkHeader->size <= (uint64)size))
             ? chunkHeader->offset+CHUNK_HEADER_SIZE+chunkHeader->size
             : (uint64)size+1;
  error = chunkIO->seek(chunkIOUserData,offset);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

void Chunk_tell(ChunkInfo *chunkInfo, uint64 *index)
{
  #ifndef NDEBUG
    Errors error;
    uint64 n;
  #endif /* NDEBUG */

  assert(chunkInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(chunkInfo);
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->io->tell != NULL);
  assert(index != NULL);

  #ifndef NDEBUG
    error = chunkInfo->io->tell(chunkInfo->ioUserData,&n);
    assert(error == ERROR_NONE);
    assert(n == chunkInfo->offset+CHUNK_HEADER_SIZE+chunkInfo->index);
  #endif /* NDEBUG */

  (*index) = chunkInfo->index;
}

Errors Chunk_seek(ChunkInfo *chunkInfo, uint64 index)
{
  Errors error;

  assert(chunkInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(chunkInfo);
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->io->seek != NULL);

  error = chunkInfo->io->seek(chunkInfo->ioUserData,chunkInfo->offset+CHUNK_HEADER_SIZE+index);
  if (error == ERROR_NONE)
  {
    chunkInfo->index = index;
  }

  return error;
}

Errors Chunk_transfer(const ChunkHeader *chunkHeader,
                      const ChunkIO     *fromChunkIO,
                      void              *fromChunkIOUserData,
                      const ChunkIO     *toChunkIO,
                      void              *toChunkIOUserData
                     )
{
  #define TRANSFER_BUFFER_SIZE (1024*1024)

  void   *buffer;
  Errors error;
  uint64 transferedBytes;
  ulong  n;

  // init variables
  buffer = malloc(TRANSFER_BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // transfer header
  error = writeDefinition(toChunkIO,
                          toChunkIOUserData,
                          CHUNK_HEADER_DEFINITION,
                          CHUNK_HEADER_SIZE,
                          0,
                          NULL,
                          chunkHeader,
                          NULL
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // transfer data
  transferedBytes = 0;
  while (transferedBytes < chunkHeader->size)
  {
    // get block size
    n = (ulong)MIN(chunkHeader->size-transferedBytes,TRANSFER_BUFFER_SIZE);

    // read data
    error = fromChunkIO->read(fromChunkIOUserData,buffer,n,NULL);
    if (error != ERROR_NONE)
    {
      free(buffer);
      return error;
    }

    // transfer to storage
    error = toChunkIO->write(toChunkIOUserData,buffer,n);
    if (error != ERROR_NONE)
    {
      free(buffer);
      return error;
    }

    // next part
    transferedBytes += (uint64)n;
  }

  // free resources
  free(buffer);

  return ERROR_NONE;
}

#ifdef NDEBUG
Errors Chunk_open(ChunkInfo         *chunkInfo,
                  const ChunkHeader *chunkHeader,
                  ulong             dataSize,
                  void              *transformUserData
                 )
#else /* not NDEBUG */
Errors __Chunk_open(const char        *__fileName__,
                    ulong             __lineNb__,
                    ChunkInfo         *chunkInfo,
                    const ChunkHeader *chunkHeader,
                    ulong             dataSize,
                    void              *transformUserData
                  )
#endif /* NDEBUG */
{
  Errors error;
  void   *deprecatedChunkData;
  ulong  bytesRead;

  assert(chunkInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(chunkInfo);
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->id == chunkHeader->id);
  assert(chunkInfo->data != NULL);
  assert(chunkHeader != NULL);

  #ifndef NDEBUG
    UNUSED_VARIABLE(__fileName__);
    UNUSED_VARIABLE(__lineNb__);
  #endif /* not NDEBUG */

  // init
  chunkInfo->chunkSize = 0L;
  chunkInfo->size      = chunkHeader->size;
  chunkInfo->offset    = chunkHeader->offset;
  chunkInfo->mode      = CHUNK_MODE_READ;
  chunkInfo->index     = 0LL;

chunkInfo->chunkSize = ALIGN(dataSize,chunkInfo->alignment);
//chunkInfo->chunkSize = getDefinitionSize(chunkInfo->definition,chunkInfo->alignment,NULL,0);
//chunkInfo->chunkSize = chunkHeader->size;

  if (chunkHeader->transformInfo != NULL)
  {
//fprintf(stderr,"%s, %d: do transform %x -> %x, size %d %d!\n",__FILE__,__LINE__,chunkHeader->transformInfo->old.id,chunkHeader->transformInfo->new.id,ALIGN(chunkHeader->transformInfo->old.fixedSize,chunkInfo->alignment),getDefinitionSize(chunkHeader->transformInfo->old.definition,chunkInfo->alignment,NULL,0),getDefinitionSize(chunkHeader->transformInfo->new.definition,chunkInfo->alignment,NULL,0));
    // allocate memory for data of deprecated chunk
    deprecatedChunkData = malloc(chunkHeader->transformInfo->old.allocSize);
    if (deprecatedChunkData == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    // read deprecated chunk
    error = readDefinition(chunkInfo->io,
                           chunkInfo->ioUserData,
                           chunkHeader->transformInfo->old.definition,
//                           chunkInfo->chunkSize,
                           ALIGN(chunkHeader->transformInfo->old.fixedSize,chunkInfo->alignment),
                           chunkInfo->alignment,
                           chunkInfo->cryptInfo,
                           deprecatedChunkData,
                           &bytesRead
                          );

    // transform chunk data
    if (error == ERROR_NONE)
    {
      error = chunkHeader->transformInfo->transformFunction(deprecatedChunkData,
                                                            chunkInfo->data,
                                                            transformUserData
                                                           );
    }

    // free resources
    free(deprecatedChunkData);
  }
  else
  {
    // read chunk
    resetDefinition(chunkInfo->definition,chunkInfo->data);
    error = readDefinition(chunkInfo->io,
                           chunkInfo->ioUserData,
                           chunkInfo->definition,
                           chunkInfo->chunkSize,
//getDefinitionSize(chunkInfo->definition,chunkInfo->alignment,NULL,0),
                           chunkInfo->alignment,
                           chunkInfo->cryptInfo,
                           chunkInfo->data,
                           &bytesRead
                          );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkInfo->index += (uint64)bytesRead;
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += (uint64)bytesRead;
  }

  // transform

  return ERROR_NONE;
}

#ifdef NDEBUG
Errors Chunk_create(ChunkInfo *chunkInfo)
#else /* not NDEBUG */
Errors __Chunk_create(const char *__fileName__,
                      ulong      __lineNb__,
                      ChunkInfo  *chunkInfo
                     )
#endif /* NDEBUG */
{
  Errors      error;
  uint64      offset;
  ChunkHeader chunkHeader;
  ulong       bytesWritten;

  assert(chunkInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(chunkInfo);
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->io->tell != NULL);
  assert(chunkInfo->id != CHUNK_ID_NONE);
  assert(chunkInfo->data != NULL);

  #ifndef NDEBUG
    UNUSED_VARIABLE(__fileName__);
    UNUSED_VARIABLE(__lineNb__);
  #endif /* not NDEBUG */

  // init
  chunkInfo->size   = 0LL;
  chunkInfo->offset = 0LL;
  chunkInfo->mode   = CHUNK_MODE_WRITE;
  chunkInfo->index  = 0LL;

  // get size of chunk (without data elements)
  chunkInfo->chunkSize = Chunk_getSize(chunkInfo,chunkInfo->data,0);

  // get current offset
  error = chunkInfo->io->tell(chunkInfo->ioUserData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkInfo->offset = offset;

  // write chunk header
  chunkHeader.id   = 0;
  chunkHeader.size = 0;
  error = writeDefinition(chunkInfo->io,
                          chunkInfo->ioUserData,
                          CHUNK_HEADER_DEFINITION,
                          CHUNK_HEADER_SIZE,
                          0,
                          NULL,
                          &chunkHeader,
                          &bytesWritten
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkInfo->index = 0LL;
  chunkInfo->size  = 0LL;
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += (uint64)bytesWritten;
    chunkInfo->parentChunkInfo->size  += (uint64)bytesWritten;
  }

  // write chunk data
  if (chunkInfo->definition != NULL)
  {
    error = writeDefinition(chunkInfo->io,
                            chunkInfo->ioUserData,
                            chunkInfo->definition,
                            chunkInfo->chunkSize,
                            chunkInfo->alignment,
                            chunkInfo->cryptInfo,
                            chunkInfo->data,
                            &bytesWritten
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }
    chunkInfo->index += (uint64)bytesWritten;
    chunkInfo->size  += (uint64)bytesWritten;
    if (chunkInfo->parentChunkInfo != NULL)
    {
      chunkInfo->parentChunkInfo->index += (uint64)bytesWritten;
      chunkInfo->parentChunkInfo->size  += (uint64)bytesWritten;
    }
  }

  return ERROR_NONE;
}

Errors Chunk_close(ChunkInfo *chunkInfo)
{
  Errors      error;
  uint64      offset;
  int64       size;
  ChunkHeader chunkHeader;
  ulong       bytesWritten;

  assert(chunkInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(chunkInfo);
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->io->tell != NULL);
  assert(chunkInfo->io->seek != NULL);
  assert(chunkInfo->io->getSize != NULL);

  switch (chunkInfo->mode)
  {
    case CHUNK_MODE_UNKNOWN:
      break;
    case CHUNK_MODE_WRITE:
      // save offset
      error = chunkInfo->io->tell(chunkInfo->ioUserData,&offset);
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update id and size in chunk-header
      error = chunkInfo->io->seek(chunkInfo->ioUserData,chunkInfo->offset);
      if (error != ERROR_NONE)
      {
        return error;
      }
      chunkHeader.id   = chunkInfo->id;
      chunkHeader.size = chunkInfo->size;
      error = writeDefinition(chunkInfo->io,
                              chunkInfo->ioUserData,
                              CHUNK_HEADER_DEFINITION,
                              CHUNK_HEADER_SIZE,
                              0,
                              NULL,
                              &chunkHeader,
                              &bytesWritten
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // restore offset
      error = chunkInfo->io->seek(chunkInfo->ioUserData,offset);
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    case CHUNK_MODE_READ:
      // check chunk size value
      error = chunkInfo->io->tell(chunkInfo->ioUserData,&offset);
      if (error != ERROR_NONE)
      {
        return error;
      }
      size = chunkInfo->io->getSize(chunkInfo->ioUserData);
      if ((size >= 0LL) && (chunkInfo->offset+CHUNK_HEADER_SIZE+chunkInfo->size > (uint64)size))
      {
       return ERROR_INVALID_CHUNK_SIZE;
      }

      // seek to end of chunk
      if (chunkInfo->offset+CHUNK_HEADER_SIZE+chunkInfo->size > offset)
      {
        error = chunkInfo->io->seek(chunkInfo->ioUserData,chunkInfo->offset+CHUNK_HEADER_SIZE+chunkInfo->size);
        if (error != ERROR_NONE)
        {
          return error;
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

Errors Chunk_nextSub(ChunkInfo   *chunkInfo,
                     ChunkHeader *chunkHeader
                    )
{
  Errors error;
  uint64 offset;
  ulong  bytesRead;
  const ChunkTransformInfo *chunkTransformInfo;

  assert(chunkInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(chunkInfo);
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->io->tell != NULL);
  assert(chunkHeader != NULL);

  if ((chunkInfo->index+CHUNK_HEADER_SIZE) > (chunkInfo->offset+chunkInfo->size))
  {
    return ERROR_END_OF_DATA;
  }

  // get current offset
  error = chunkInfo->io->tell(chunkInfo->ioUserData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkHeader->offset = offset;

  // read chunk header
  error = readDefinition(chunkInfo->io,
                         chunkInfo->ioUserData,
                         CHUNK_HEADER_DEFINITION,
                         CHUNK_HEADER_SIZE,
                         0,
                         NULL,
                         chunkHeader,
                         &bytesRead
                        );
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkInfo->index += (uint64)bytesRead;
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += (uint64)bytesRead;
  }

  // validate chunk size
  if ((chunkInfo->index+chunkHeader->size) > chunkInfo->offset+CHUNK_HEADER_SIZE+chunkInfo->size)
  {
    return ERROR_END_OF_DATA;
  }

  // find transform chunk function (if any)
  chunkHeader->transformInfo = NULL;
  chunkTransformInfo = CHUNK_TRANSFORM_INFOS;
  while (chunkTransformInfo->old.id != CHUNK_ID_NONE)
  {
    if (chunkTransformInfo->old.id == chunkHeader->id)
    {
      chunkHeader->id            = chunkTransformInfo->new.id;
      chunkHeader->transformInfo = chunkTransformInfo;
//fprintf(stderr,"%s, %d: --- transform function %x %p\n",__FILE__,__LINE__,chunkHeader->id,chunkHeader->transformInfo);
      break;
    }
    chunkTransformInfo++;
  }

  return ERROR_NONE;
}

Errors Chunk_skipSub(ChunkInfo         *chunkInfo,
                     const ChunkHeader *chunkHeader
                    )
{
  uint64 size;
  uint64 offset;
  Errors error;

  assert(chunkInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(chunkInfo);
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->io->getSize != NULL);
  assert(chunkInfo->io->seek != NULL);
  assert(chunkHeader != NULL);

  size = chunkInfo->io->getSize(chunkInfo->ioUserData);
  if (chunkHeader->offset+CHUNK_HEADER_SIZE+chunkHeader->size > size)
  {
    return ERROR_INVALID_CHUNK_SIZE;
  }

  /* Note: fseeko in File_seek() cause an SigSegV if "offset" is
     completely wrong; thus never call it with an invalid offset
  */
  offset = (chunkHeader->offset+CHUNK_HEADER_SIZE+chunkHeader->size <= size)
             ? chunkHeader->offset+CHUNK_HEADER_SIZE+chunkHeader->size
             : size+1;
  assert(offset > chunkInfo->index);
  error = chunkInfo->io->seek(chunkInfo->ioUserData,offset);
  if (error != ERROR_NONE)
  {
    return error;
  }

  chunkInfo->index += chunkHeader->size;

  return ERROR_NONE;
}

Errors Chunk_update(ChunkInfo *chunkInfo)
{
  Errors error;
  uint64 offset;
  ulong  bytesWritten;

  assert(chunkInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(chunkInfo);
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->io->tell != NULL);
  assert(chunkInfo->io->seek != NULL);
  assert(chunkInfo->data != NULL);
  assert(chunkInfo->chunkSize == Chunk_getSize(chunkInfo,chunkInfo->data,0));

  // get current offset
  error = chunkInfo->io->tell(chunkInfo->ioUserData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // update
  error = chunkInfo->io->seek(chunkInfo->ioUserData,chunkInfo->offset+CHUNK_HEADER_SIZE);
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = writeDefinition(chunkInfo->io,
                          chunkInfo->ioUserData,
                          chunkInfo->definition,
                          chunkInfo->chunkSize,
                          chunkInfo->alignment,
                          chunkInfo->cryptInfo,
                          chunkInfo->data,
                          &bytesWritten
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // restore offset
  error = chunkInfo->io->seek(chunkInfo->ioUserData,offset);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Chunk_readData(ChunkInfo *chunkInfo,
                      void      *data,
                      ulong     size,
                      ulong     *bytesRead
                     )
{
  Errors error;
  ulong  n;

  assert(chunkInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(chunkInfo);
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->io->read != NULL);
  assert(data != NULL);

  // limit size to read to rest
  if ((uint64)size > (chunkInfo->size-chunkInfo->index))
  {
    size = (ulong)(chunkInfo->size-chunkInfo->index);
  }

  // read data
  error = chunkInfo->io->read(chunkInfo->ioUserData,data,size,&n);
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (bytesRead != NULL)
  {
    // return number of read bytes
    (*bytesRead) = n;
  }
  else
  {
    // check if read all requested data
    if (size != n)
    {
      return ERROR_(IO,errno);
    }
  }

  // increment indizes
  chunkInfo->index += (uint64)n;
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += (uint64)n;
  }

  return ERROR_NONE;
}

Errors Chunk_writeData(ChunkInfo  *chunkInfo,
                       const void *data,
                       ulong      size
                      )
{
  Errors error;

  assert(chunkInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(chunkInfo);
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->io->write != NULL);

  // write data
  error = chunkInfo->io->write(chunkInfo->ioUserData,data,size);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // increment indizes and increase sizes
  chunkInfo->size  += (uint64)size;
  chunkInfo->index += (uint64)size;
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += (uint64)size;
    chunkInfo->parentChunkInfo->size  += (uint64)size;
  }

  return ERROR_NONE;
}

Errors Chunk_skipData(ChunkInfo *chunkInfo,
                      ulong     size
                     )
{
  Errors error;
  uint64 offset;

  assert(chunkInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(chunkInfo);
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->io->tell != NULL);
  assert(chunkInfo->io->seek != NULL);

  // skip data
  error = chunkInfo->io->tell(chunkInfo->ioUserData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }
  offset += (uint64)size;
  error = chunkInfo->io->seek(chunkInfo->ioUserData,offset);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // increment size
  chunkInfo->index += (uint64)size;

  // set size in container chunk
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += (uint64)size;
    chunkInfo->parentChunkInfo->size  += (uint64)size;
  }

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
