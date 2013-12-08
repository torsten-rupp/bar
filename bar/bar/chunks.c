/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver file chunks functions
* Systems: all
*
\***********************************************************************/

#define __CHUNK_IMPLEMENATION__

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
  #include <windows.h>
  #include <winsock2.h>
#endif
#include <zlib.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "bar.h"
#include "archive_format.h"

#include "chunks.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// chunk header definition
LOCAL int CHUNK_HEADER_DEFINITION[] = {
                                       CHUNK_DATATYPE_UINT32,offsetof(ChunkHeader,id),
                                       CHUNK_DATATYPE_UINT64,offsetof(ChunkHeader,size),
                                       0
                                      };

#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/

// chunk header
typedef struct
{
  ChunkId id;
  uint64  size;
} Chunk;

// chunk read/write buffer
typedef struct
{
  const ChunkIO *chunkIO;
  void          *chunkIOUserData;
  union
  {
    ulong       bytesRead;
    ulong       bytesWritten;
  };

  const int     *definition;
  ulong         chunkSize;
  uint          definitionIndex;
  uint          alignment;
  CryptInfo     *cryptInfo;

  byte          *buffer;
  ulong         bufferLength;
  ulong         bufferSize;
  ulong         bufferIndex;
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

LOCAL Errors initChunkBuffer(ChunkBuffer   *chunkBuffer,
                             ChunkModes    chunkMode,
                             const ChunkIO *chunkIO,
                             void          *chunkIOUserData,
                             const int     *definition,
                             ulong         chunkSize,
                             uint          alignment,
                             CryptInfo     *cryptInfo
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

  chunkBuffer->chunkIO         = chunkIO;
  chunkBuffer->chunkIOUserData = chunkIOUserData;
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

  switch (chunkMode)
  {
    case CHUNK_MODE_READ:
#if 0
// does not work: decryption in parts is not possible, thus data must be read and decrypted as a single block
      // get aligned max. data length which can be read initialy
      n        = 0;
      i        = 0;
      doneFlag = FALSE;
      while (   (definition[i] != 0)
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

          case CHUNK_DATATYPE_BYTE|CHUNK_DATATYPE_ARRAY:
          case CHUNK_DATATYPE_UINT8|CHUNK_DATATYPE_ARRAY:
          case CHUNK_DATATYPE_INT8|CHUNK_DATATYPE_ARRAY:
          case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY:
          case CHUNK_DATATYPE_INT16|CHUNK_DATATYPE_ARRAY:
          case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY:
          case CHUNK_DATATYPE_INT32|CHUNK_DATATYPE_ARRAY:
          case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY:
          case CHUNK_DATATYPE_INT64|CHUNK_DATATYPE_ARRAY:
            n += 2L;
            doneFlag = TRUE;
            i += 3;
            break;
          case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY:
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
//fprintf(stderr,"%s, %d: read:\n",__FILE__,__LINE__); debugDumpMemory(FALSE,chunkBuffer->buffer,n);

      // decrypt initial data
      if (cryptInfo != NULL)
      {
// NYI ???: seed value?
        Crypt_reset(cryptInfo,0);
        error = Crypt_decrypt(cryptInfo,chunkBuffer->buffer,n);
        if (error != ERROR_NONE)
        {
          free(chunkBuffer->buffer);
          return error;
        }
      }
      chunkBuffer->bufferLength += n;
//fprintf(stderr,"%s, %d: read decrypted:\n",__FILE__,__LINE__); debugDumpMemory(FALSE,chunkBuffer->buffer,n);
      break;
    case CHUNK_MODE_WRITE:
      // allocate initial buffer size
      chunkBuffer->bufferSize = 1024;
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

  DEBUG_ADD_RESOURCE_TRACE("chunk buffer",chunkBuffer);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : doneChunkBuffer
* Purpose: done chunk buffer
* Input  : chunkBuffer - chunk buffer handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneChunkBuffer(ChunkBuffer *chunkBuffer)
{
  assert(chunkBuffer != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(chunkBuffer);

  free(chunkBuffer->buffer);
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
      chunkBuffer->bufferSize = ALIGN(chunkBuffer->bufferLength+n,1024);
      chunkBuffer->buffer = realloc(chunkBuffer->buffer,chunkBuffer->bufferSize);
      if (chunkBuffer->buffer == NULL)
      {
        return ERROR_INSUFFICIENT_MEMORY;
      }
      memset(&chunkBuffer->buffer[chunkBuffer->bufferLength],0,chunkBuffer->bufferSize-chunkBuffer->bufferLength);
    }

    // read data
    error = chunkBuffer->chunkIO->read(chunkBuffer->chunkIOUserData,
                                       &chunkBuffer->buffer[chunkBuffer->bufferLength],
                                       n,
                                       NULL
                                      );
    if (error != ERROR_NONE)
    {
      free(chunkBuffer->buffer);
      return error;
    }
    chunkBuffer->bytesRead += n;
//fprintf(stderr,"%s, %d: read:\n",__FILE__,__LINE__); debugDumpMemory(FALSE,chunkBuffer->buffer,chunkBuffer->bufferLength+n);

    // decrypt data
    if (chunkBuffer->cryptInfo != NULL)
    {
      // Note: we need to decrypt from start, because it is not possible to decrypt partial data blocks
// NYI ???: seed value?
      Crypt_reset(chunkBuffer->cryptInfo,0);
      error = Crypt_decrypt(chunkBuffer->cryptInfo,chunkBuffer->buffer,chunkBuffer->bufferLength+n);
      if (error != ERROR_NONE)
      {
        free(chunkBuffer->buffer);
        return error;
      }
    }
//fprintf(stderr,"%s, %d: read decrypted:\n",__FILE__,__LINE__); debugDumpMemory(FALSE,chunkBuffer->buffer,chunkBuffer->bufferLength+n);

    chunkBuffer->bufferLength += n;
  }

  // get data
  (*p) = &chunkBuffer->buffer[chunkBuffer->bufferIndex];
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
  assert(chunkBuffer->chunkIO != NULL);
  assert(chunkBuffer->chunkIO->read != NULL);

  // increase buffer size if required
  if ((chunkBuffer->bufferLength+size) > chunkBuffer->bufferSize)
  {
    chunkBuffer->bufferSize = ALIGN(ALIGN(chunkBuffer->bufferLength+size,chunkBuffer->alignment),1024);
    chunkBuffer->buffer = realloc(chunkBuffer->buffer,chunkBuffer->bufferSize);
    if (chunkBuffer->buffer == NULL)
    {
      return ERROR_INSUFFICIENT_MEMORY;
    }
    memset(&chunkBuffer->buffer[chunkBuffer->bufferLength],0,chunkBuffer->bufferSize-chunkBuffer->bufferLength);
  }

  // put data
  memcpy(&chunkBuffer->buffer[chunkBuffer->bufferLength],p,size);
  chunkBuffer->bufferLength += size;

  return ERROR_NONE;
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
  memset(&chunkBuffer->buffer[chunkBuffer->bufferLength],0,n-chunkBuffer->bufferLength);

  // encrypt data
//fprintf(stderr,"%s, %d: write:\n",__FILE__,__LINE__); debugDumpMemory(FALSE,chunkBuffer->buffer,ALIGN(chunkBuffer->bufferLength,chunkBuffer->alignment));
  if (chunkBuffer->cryptInfo != NULL)
  {
// NYI ???: seed value?
    Crypt_reset(chunkBuffer->cryptInfo,0);
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
//fprintf(stderr,"%s, %d: write encrypted:\n",__FILE__,__LINE__); debugDumpMemory(FALSE,chunkBuffer->buffer,ALIGN(chunkBuffer->bufferLength,chunkBuffer->alignment));
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
* Name   : initDefinition
* Purpose: init chunk definition
* Input  : definition - chunk definition
*          data       - chunk data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initDefinition(const int *definition,
                          void      *data
                         )
{
  uint i;

  if (definition != NULL)
  {
    i = 0;
    while (definition[i+0] != 0)
    {
      switch (definition[i+0])
      {
        case CHUNK_DATATYPE_BYTE:
        case CHUNK_DATATYPE_UINT8:
        case CHUNK_DATATYPE_INT8:
          // init 8bite value
          (*((uint8*)((byte*)data+definition[i+1]))) = 0;

          i += 2;
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          // init 16bite value
          (*((uint16*)((byte*)data+definition[i+1]))) = 0;

          i += 2;
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          // init 32bite value
          (*((uint32*)((byte*)data+definition[i+1]))) = 0L;

          i += 2;
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          // init 64bite value
          (*((uint64*)((byte*)data+definition[i+1]))) = 0LL;

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
            (*((String*)((byte*)data+definition[i+1]))) = string;

            i += 2;
          }
          break;

        case CHUNK_DATATYPE_BYTE|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_UINT8|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT8|CHUNK_DATATYPE_ARRAY:
          (*((uint8*)((byte*)data+definition[i+1]))) = 0;

          i += 3;
          break;
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT16|CHUNK_DATATYPE_ARRAY:
          (*((uint16*)((byte*)data+definition[i+1]))) = 0;

          i += 3;
          break;
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT32|CHUNK_DATATYPE_ARRAY:
          (*((uint32*)((byte*)data+definition[i+1]))) = 0L;

          i += 3;
          break;
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT64|CHUNK_DATATYPE_ARRAY:
          (*((uint64*)((byte*)data+definition[i+1]))) = 0LL;

          i += 3;
          break;
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY:
          {
            String string;

            string = String_new();
            if (string == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }

            (*((String*)((byte*)data+definition[i+1]))) = string;

            i += 3;
          }
          break;

        case CHUNK_DATATYPE_CRC32:
          i += 2;
          break;

        case CHUNK_DATATYPE_DATA:
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

  DEBUG_ADD_RESOURCE_TRACE("definition data",data);
}

/***********************************************************************\
* Name   : doneDefinition
* Purpose: done chunk definition
* Input  : definition     - chunk definition
*          data           - chunk data
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors doneDefinition(const int  *definition,
                            const void *data
                           )
{
  uint i;

  DEBUG_REMOVE_RESOURCE_TRACE(data);

  if (definition != NULL)
  {
    i = 0;
    while (definition[i+0] != 0)
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
          String_delete(*((String*)((byte*)data+definition[i+1])));
          i += 2;
          break;

        case CHUNK_DATATYPE_BYTE|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_UINT8|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT8|CHUNK_DATATYPE_ARRAY:
          i += 3;
          break;
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT16|CHUNK_DATATYPE_ARRAY:
          i += 3;
          break;
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT32|CHUNK_DATATYPE_ARRAY:
          i += 3;
          break;
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT64|CHUNK_DATATYPE_ARRAY:
          i += 3;
          break;
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY:
          String_delete(*((String*)((byte*)data+definition[i+1])));
          i += 3;
          break;

        case CHUNK_DATATYPE_CRC32:
          i += 2;
          break;

        case CHUNK_DATATYPE_DATA:
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
* Name   : readDefinition
* Purpose: read chunk definition
* Input  : chunkIO         - i/o functions
*          chunkIOUserData - user data for i/o
*          definition      - chunk definition
*          chunkSize       - size of chunk (in bytes)
*          alignment       - chunk alignment
*          cryptInfo       - crypt info
* Output : chunkData - chunk data
*          bytesRead - number of bytes read
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors readDefinition(const ChunkIO *chunkIO,
                            void          *chunkIOUserData,
                            const int     *definition,
                            ulong         chunkSize,
                            uint          alignment,
                            CryptInfo     *cryptInfo,
                            void          *chunkData,
                            ulong         *bytesRead
                           )
{
  Errors      error;
  uint64      offset;
  ChunkBuffer chunkBuffer;
  uint32      crc;
  uint        i;
  void        *p;
  char        errorText[64];

  assert(chunkIO != NULL);
  assert(chunkIO->tell != NULL);
  assert(chunkIO->seek != NULL);
  assert(chunkIO->read != NULL);

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
    while (   (definition[i+0] != 0)
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
            n = (*((uint8*)p));

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
            n = ntohs(*((uint16*)p));

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
            n = ntohl(*((uint32*)p));

            (*((uint32*)((byte*)chunkData+definition[i+1]))) = n;

            i += 2;
          }
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          {
            uint64 n;
            uint32 h,l;

            // get 64bit value
            error = getChunkBuffer(&chunkBuffer,&p,8L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p,8);
            h = ntohl(*((uint32*)p+0));
            l = ntohl(*((uint32*)p+1));
            n = (((uint64)h) << 32) | (((uint64)l << 0));

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
            stringLength = ntohs(*((uint16*)p));

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

        case CHUNK_DATATYPE_BYTE|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_UINT8|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT8|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT16|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT32|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT64|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY:
          {
            uint16 arrayLength;
            void   *arrayData;

            // get array length (16bit value)
            error = getChunkBuffer(&chunkBuffer,&p,2L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p,2);
            arrayLength = ntohl(*((uint16*)p));

            switch (definition[i+0])
            {
              case CHUNK_DATATYPE_BYTE|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_UINT8|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_INT8|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_INT16|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_INT32|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_INT64|CHUNK_DATATYPE_ARRAY:
                {
                  ulong size;

                  switch (definition[i+0])
                  {
                    case CHUNK_DATATYPE_BYTE|CHUNK_DATATYPE_ARRAY  :
                    case CHUNK_DATATYPE_UINT8|CHUNK_DATATYPE_ARRAY :
                    case CHUNK_DATATYPE_INT8|CHUNK_DATATYPE_ARRAY  : size = 1; break;
                    case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY:
                    case CHUNK_DATATYPE_INT16|CHUNK_DATATYPE_ARRAY : size = 2; break;
                    case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY:
                    case CHUNK_DATATYPE_INT32|CHUNK_DATATYPE_ARRAY : size = 4; break;
                    case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY:
                    case CHUNK_DATATYPE_INT64|CHUNK_DATATYPE_ARRAY : size = 8; break;
                  }

                  // get array data
                  error = getChunkBuffer(&chunkBuffer,&p,(ulong)arrayLength*size);
                  if (error != ERROR_NONE) break;
                  crc = crc32(crc,p,(ulong)arrayLength*size);

                  arrayData = malloc((ulong)arrayLength*size);
                  if (arrayData == NULL)
                  {
                    snprintf(errorText,sizeof(errorText),"insufficient memory: %lubytes",(ulong)arrayLength*size);
                    error = ERRORX_(CORRUPT_DATA,0,errorText);
                    break;
                  }
                  memcpy(arrayData,p,(ulong)arrayLength*size);

                  (*((uint* )((byte*)chunkData+definition[i+1]))) = (uint)arrayLength;
                  (*((void**)((byte*)chunkData+definition[i+2]))) = arrayData;
                }
                break;
              case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY:
                {
                  String *strings;
                  uint   z;
                  uint16 stringLength;

                  // allocate string array
                  strings = calloc((ulong)arrayLength,sizeof(String));
                  if (strings == NULL)
                  {
                    snprintf(errorText,sizeof(errorText),"insufficient memory: %lubytes",(ulong)arrayLength*sizeof(String));
                    error = ERRORX_(CORRUPT_DATA,0,errorText);
                    break;
                  }

                  // get array data
                  for (z = 0; z < arrayLength; z++)
                  {
                    // get string length (16bit value)
                    error = getChunkBuffer(&chunkBuffer,&p,2L);
                    if (error != ERROR_NONE) break;
                    crc = crc32(crc,p,2);
                    stringLength = ntohs(*((uint16*)p));

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
            n = ntohl(*(uint32*)p);

            // check crc
            if (n != crc)
            {
              snprintf(errorText,sizeof(errorText),"at offset %llu",offset);
              error = ERRORX_(CRC_,0,errorText);
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

LOCAL Errors writeDefinition(const ChunkIO *chunkIO,
                             void          *chunkIOUserData,
                             const int     *definition,
                             uint          chunkSize,
                             uint          alignment,
                             CryptInfo     *cryptInfo,
                             const void    *chunkData,
                             ulong         *bytesWritten
                            )
{
  ChunkBuffer chunkBuffer;
  uint32      crc;
  int         i;
  byte        p[8];
  Errors      error;

  assert(chunkIO != NULL);
  assert(chunkIO->write != NULL);
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
    while (definition[i+0] != 0)
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
            (*((uint8*)p)) = n;
            error = putChunkBuffer(&chunkBuffer,p,1L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p,1);

            i += 2;
          }
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          {
            uint16 n;

            n = (*((uint16*)((byte*)chunkData+definition[i+1])));

            // put 16bit value
            (*((uint16*)p)) = htons(n);
            error = putChunkBuffer(&chunkBuffer,p,2L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p,2);

            i += 2;
          }
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          {
            uint32 n;

            n = (*((uint32*)((byte*)chunkData+definition[i+1])));

            // put 32bit value
            (*((uint32*)p)) = htonl(n);
            error = putChunkBuffer(&chunkBuffer,p,4L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p,4);

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
            (*((uint32*)p+0)) = htonl(h);
            (*((uint32*)p+1)) = htonl(l);
            error = putChunkBuffer(&chunkBuffer,p,8L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p,8);

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
            (*((uint16*)p)) = htons(stringLength);
            error = putChunkBuffer(&chunkBuffer,p,2L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p,2);

            // put string data
            error = putChunkBuffer(&chunkBuffer,stringData,(ulong)stringLength);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,stringData,stringLength);

            i += 2;
          }
          break;

        case CHUNK_DATATYPE_BYTE|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_UINT8|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT8|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT16|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT32|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_INT64|CHUNK_DATATYPE_ARRAY:
        case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY:
          {
            uint16     arrayLength;
            const void *arrayData;
            const void *stringData;
            uint16     stringLength;

            arrayLength = (*((uint* )((byte*)chunkData+definition[i+1])));
            arrayData   = (*((void**)((byte*)chunkData+definition[i+2])));

            // put array length (16bit value)
            (*((uint16*)p)) = htons(arrayLength);
            error = putChunkBuffer(&chunkBuffer,p,2L);
            if (error != ERROR_NONE) break;
            crc = crc32(crc,p,2);

            switch (definition[i+0])
            {
              case CHUNK_DATATYPE_BYTE|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_UINT8|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_INT8|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_INT16|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_INT32|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY:
              case CHUNK_DATATYPE_INT64|CHUNK_DATATYPE_ARRAY:
                {
                  ulong size;

                  switch (definition[i+0])
                  {
                    case CHUNK_DATATYPE_BYTE|CHUNK_DATATYPE_ARRAY  :
                    case CHUNK_DATATYPE_UINT8|CHUNK_DATATYPE_ARRAY :
                    case CHUNK_DATATYPE_INT8|CHUNK_DATATYPE_ARRAY  : size = 1; break;
                    case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY:
                    case CHUNK_DATATYPE_INT16|CHUNK_DATATYPE_ARRAY : size = 2; break;
                    case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY:
                    case CHUNK_DATATYPE_INT32|CHUNK_DATATYPE_ARRAY : size = 4; break;
                    case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY:
                    case CHUNK_DATATYPE_INT64|CHUNK_DATATYPE_ARRAY : size = 8; break;
                  }

                  // put array data
                  error = putChunkBuffer(&chunkBuffer,arrayData,(ulong)arrayLength*size);
                  if (error != ERROR_NONE) break;
                  crc = crc32(crc,arrayData,arrayLength*size);
                }
                break;
              case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY:
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
                    (*((uint16*)p)) = htons(stringLength);
                    error = putChunkBuffer(&chunkBuffer,&p,2L);
                    if (error != ERROR_NONE) break;
                    crc = crc32(crc,p,2);

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
            (*((uint32*)p)) = htonl(crc);
            error = putChunkBuffer(&chunkBuffer,p,4);
            if (error != ERROR_NONE) break;

            crc = crc32(0,Z_NULL,0);

            i += 2;
          }
          break;

        case CHUNK_DATATYPE_DATA:
          i += 2;
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

ulong Chunk_getSize(const int  *definition,
                    ulong      alignment,
                    const void *chunkData,
                    ulong      dataLength
                   )
{
  ulong size;
  int   z;

  assert(definition != NULL);

  size = 0;
  z    = 0;
  while (definition[z+0] != CHUNK_DATATYPE_NONE)
  {
    switch (definition[z+0])
    {
      case CHUNK_DATATYPE_BYTE:
      case CHUNK_DATATYPE_UINT8:
      case CHUNK_DATATYPE_INT8:
        size += 1;

        z += 2;
        break;
      case CHUNK_DATATYPE_UINT16:
      case CHUNK_DATATYPE_INT16:
        size += 2;

        z += 2;
        break;
      case CHUNK_DATATYPE_UINT32:
      case CHUNK_DATATYPE_INT32:
        size += 4;

        z += 2;
        break;
      case CHUNK_DATATYPE_UINT64:
      case CHUNK_DATATYPE_INT64:
        size += 8;

        z += 2;
        break;
      case CHUNK_DATATYPE_STRING:
        {
          String s;

          assert(chunkData != NULL);

          s = (*((String*)((byte*)chunkData+definition[z+1])));
          assert(s != NULL);
#warning alignment string?
          size += 2+String_length(s);

          z += 2;
        }
        break;

      case CHUNK_DATATYPE_BYTE|CHUNK_DATATYPE_ARRAY:
      case CHUNK_DATATYPE_UINT8|CHUNK_DATATYPE_ARRAY:
      case CHUNK_DATATYPE_INT8|CHUNK_DATATYPE_ARRAY:
        {
          uint length;

          assert(chunkData != NULL);

          length = (*((uint*)((byte*)chunkData+definition[z+1])));
          size += 2+ALIGN(length*1,4);

          z += 3;
        }
        break;
      case CHUNK_DATATYPE_UINT16|CHUNK_DATATYPE_ARRAY:
      case CHUNK_DATATYPE_INT16|CHUNK_DATATYPE_ARRAY:
        {
          uint length;

          assert(chunkData != NULL);

          length = (*((uint*)((byte*)chunkData+definition[z+1])));
          size += 2+ALIGN(length*2,4);

          z += 3;
        }
        break;
      case CHUNK_DATATYPE_UINT32|CHUNK_DATATYPE_ARRAY:
      case CHUNK_DATATYPE_INT32|CHUNK_DATATYPE_ARRAY:
        {
          uint length;

          assert(chunkData != NULL);

          length = (*((uint*)((byte*)chunkData+definition[z+1])));
          size += 2+ALIGN(length*4,4);

          z += 3;
        }
        break;
      case CHUNK_DATATYPE_UINT64|CHUNK_DATATYPE_ARRAY:
      case CHUNK_DATATYPE_INT64|CHUNK_DATATYPE_ARRAY:
        {
          uint length;

          assert(chunkData != NULL);

          length = (*((uint*)((byte*)chunkData+definition[z+1])));
          size += 2+ALIGN(length*8,4);

          z += 3;
        }
        break;
      case CHUNK_DATATYPE_STRING|CHUNK_DATATYPE_ARRAY:
        {
          uint   length;
          String s;

          assert(chunkData != NULL);

          length = (*((uint*)((byte*)chunkData+definition[z+1])));
          while (length > 0)
          {
            s = (*((String*)((byte*)chunkData+definition[z+1])));
            assert(s != NULL);
#warning alignment string?
            size += 2+String_length(s);
          }

          z += 3;
        }
        break;

      case CHUNK_DATATYPE_CRC32:
        size += 4;

        z += 2;
        break;

      case CHUNK_DATATYPE_DATA:
        size += dataLength;

        z += 2;
        break;

      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
  }

  // align size
  size = ALIGN(size,alignment);

  return size;
}

#ifdef NDEBUG
Errors Chunk_init(ChunkInfo     *chunkInfo,
                  ChunkInfo     *parentChunkInfo,
                  const ChunkIO *chunkIO,
                  void          *chunkIOUserData,
                  ChunkId       chunkId,
                  const int     *definition,
                  uint          alignment,
                  CryptInfo     *cryptInfo,
                  void          *data
                 )
#else /* not NDEBUG */
Errors __Chunk_init(const char    *__fileName__,
                    ulong         __lineNb__,
                    ChunkInfo     *chunkInfo,
                    ChunkInfo     *parentChunkInfo,
                    const ChunkIO *chunkIO,
                    void          *chunkIOUserData,
                    ChunkId       chunkId,
                    const int     *definition,
                    uint          alignment,
                    CryptInfo     *cryptInfo,
                    void          *data
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

  chunkInfo->id              = chunkId;
  chunkInfo->definition      = definition;
  chunkInfo->chunkSize       = 0L;
  chunkInfo->size            = 0LL;
  chunkInfo->offset          = 0LL;
  chunkInfo->index           = 0LL;

  chunkInfo->data            = data;

  initDefinition(definition,data);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE("chunk",chunkInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,"chunk",chunkInfo);
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
    DEBUG_REMOVE_RESOURCE_TRACE(chunkInfo);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,chunkInfo);
  #endif /* NDEBUG */

  doneDefinition(chunkInfo->definition,chunkInfo->data);
}

Errors Chunk_next(const ChunkIO *chunkIO,
                  void          *chunkIOUserData,
                  ChunkHeader   *chunkHeader
                 )
{
  Errors error;
  uint64 offset;
  Chunk  chunk;

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
                         &chunk,
                         NULL
                        );
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkHeader->id   = chunk.id;
  chunkHeader->size = chunk.size;

  return ERROR_NONE;
}

Errors Chunk_skip(const ChunkIO     *chunkIO,
                  void              *chunkIOUserData,
                  const ChunkHeader *chunkHeader
                 )
{
  uint64 size;
  uint64 offset;
  Errors error;

  assert(chunkIO != NULL);
  assert(chunkIO->getSize != NULL);
  assert(chunkIO->seek != NULL);
  assert(chunkHeader != NULL);

  size = chunkIO->getSize(chunkIOUserData);
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
  error = chunkIO->seek(chunkIOUserData,offset);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

bool Chunk_eof(const ChunkIO *chunkIO,
               void          *chunkIOUserData
              )
{
  assert(chunkIO != NULL);
  assert(chunkIO->eof != NULL);

  return chunkIO->eof(chunkIOUserData);
}

void Chunk_tell(ChunkInfo *chunkInfo, uint64 *index)
{
  #ifndef NDEBUG
    Errors error;
    uint64 n;
  #endif /* NDEBUG */

  assert(chunkInfo != NULL);
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
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->io->seek != NULL);

  error = chunkInfo->io->seek(chunkInfo->ioUserData,chunkInfo->offset+CHUNK_HEADER_SIZE+index);
  if (error == ERROR_NONE)
  {
    chunkInfo->index = index;
  }

  return error;
}

Errors Chunk_open(ChunkInfo         *chunkInfo,
                  const ChunkHeader *chunkHeader,
                  ulong             dataSize
                 )
{
  Errors error;
  ulong  bytesRead;

  assert(chunkInfo != NULL);
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->id == chunkHeader->id);
  assert(chunkInfo->data != NULL);

  // init
  chunkInfo->chunkSize = dataSize;
  chunkInfo->size      = chunkHeader->size;
  chunkInfo->offset    = chunkHeader->offset;
  chunkInfo->mode      = CHUNK_MODE_READ;
  chunkInfo->index     = 0LL;

  // read chunk
  error = readDefinition(chunkInfo->io,
                         chunkInfo->ioUserData,
                         chunkInfo->definition,
                         chunkInfo->chunkSize,
                         chunkInfo->alignment,
                         chunkInfo->cryptInfo,
                         chunkInfo->data,
                         &bytesRead
                        );
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkInfo->index += bytesRead;
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += bytesRead;
  }

  return ERROR_NONE;
}

Errors Chunk_create(ChunkInfo *chunkInfo)
{
  Errors      error;
  uint64      offset;
  ChunkHeader chunkHeader;
  ulong       bytesWritten;

  assert(chunkInfo != NULL);
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->id != CHUNK_ID_NONE);
  assert(chunkInfo->data != NULL);

  // init
  chunkInfo->size   = 0LL;
  chunkInfo->offset = 0LL;
  chunkInfo->mode   = CHUNK_MODE_WRITE;
  chunkInfo->index  = 0LL;

  // get size of chunk (without data elements)
  chunkInfo->chunkSize = Chunk_getSize(chunkInfo->definition,chunkInfo->alignment,chunkInfo->data,0);

  // get current offset
  error = chunkInfo->io->tell(chunkInfo->ioUserData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkInfo->offset = offset;

  // write chunk header id
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
    chunkInfo->parentChunkInfo->index += bytesWritten;
    chunkInfo->parentChunkInfo->size  += bytesWritten;
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
    chunkInfo->index += bytesWritten;
    chunkInfo->size  += bytesWritten;
    if (chunkInfo->parentChunkInfo != NULL)
    {
      chunkInfo->parentChunkInfo->index += bytesWritten;
      chunkInfo->parentChunkInfo->size  += bytesWritten;
    }
  }

  return ERROR_NONE;
}

Errors Chunk_close(ChunkInfo *chunkInfo)
{
  Errors      error;
  uint64      offset;
  ChunkHeader chunkHeader;
  ulong       bytesWritten;

  assert(chunkInfo != NULL);
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
      if (chunkInfo->offset+CHUNK_HEADER_SIZE+chunkInfo->size > chunkInfo->io->getSize(chunkInfo->ioUserData))
      {
        return ERROR_INVALID_CHUNK_SIZE;
      }

      if (chunkInfo->offset+CHUNK_HEADER_SIZE+chunkInfo->size > offset)
      {
        // seek to end of chunk
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
  Chunk  chunk;
  ulong  bytesRead;

  assert(chunkInfo != NULL);
  assert(chunkInfo->io != NULL);
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
                         &chunk,
                         &bytesRead
                        );
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkInfo->index += bytesRead;
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += bytesRead;
  }
  chunkHeader->id   = chunk.id;
  chunkHeader->size = chunk.size;

  // validate chunk
  if (chunk.size > (chunkInfo->size-chunkInfo->index))
  {
    return ERROR_END_OF_DATA;
  }

  return ERROR_NONE;
}

Errors Chunk_skipSub(ChunkInfo   *chunkInfo,
                     ChunkHeader *chunkHeader
                    )
{
  uint64 size;
  uint64 offset;
  Errors error;

  assert(chunkInfo != NULL);
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
  error = chunkInfo->io->seek(chunkInfo->ioUserData,offset);
  if (error != ERROR_NONE)
  {
    return error;
  }

  chunkInfo->index = offset;

  return ERROR_NONE;
}

bool Chunk_eofSub(ChunkInfo *chunkInfo)
{
  return chunkInfo->index >= chunkInfo->size;
}

Errors Chunk_update(ChunkInfo *chunkInfo)
{
  Errors error;
  uint64 offset;
  ulong  bytesWritten;

  assert(chunkInfo != NULL);
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->io->tell != NULL);
  assert(chunkInfo->io->seek != NULL);
  assert(chunkInfo->data != NULL);
  assert(chunkInfo->chunkSize == Chunk_getSize(chunkInfo->definition,chunkInfo->alignment,chunkInfo->data,0));

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
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->io->read != NULL);
  assert(data != NULL);

  // limit size to read to rest
  if (size > (chunkInfo->size-chunkInfo->index))
  {
    size = chunkInfo->size-chunkInfo->index;
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
      return ERROR_(IO_ERROR,errno);
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
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->io->write != NULL);

  // write data
  error = chunkInfo->io->write(chunkInfo->ioUserData,data,size);
  if (error != ERROR_NONE)
  {
    return ERROR_(IO_ERROR,errno);
  }

  // increment indizes and increase sizes
  chunkInfo->size  += size;
  chunkInfo->index += size;
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += size;
    chunkInfo->parentChunkInfo->size  += size;
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
  assert(chunkInfo->io != NULL);
  assert(chunkInfo->io->tell != NULL);
  assert(chunkInfo->io->seek != NULL);

  // skip data
  error = chunkInfo->io->tell(chunkInfo->ioUserData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }
  offset += size;
  error = chunkInfo->io->seek(chunkInfo->ioUserData,offset);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // increment size
  chunkInfo->index += size;

  // set size in container chunk
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += size;
    chunkInfo->parentChunkInfo->size  += size;
  }

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
