/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/chunks.c,v $
* $Revision: 1.19 $
* $Author: torsten $
* Contents: Backup ARchiver file chunks functions
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <zlib.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "bar.h"
#include "archive_format.h"

#include "chunks.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/* number of padding bytes in C-structs */
#define PADDING_INT8    3
#define PADDING_INT16   2
#define PADDING_INT32   0
#define PADDING_INT64   0
#define PADDING_ADDRESS 0

/* chunk header definition */
LOCAL int CHUNK_DEFINITION[] = {
                                 CHUNK_DATATYPE_UINT32,
                                 CHUNK_DATATYPE_UINT64,
                                 0
                               };
#define CHUNK_DEFINITION_SIZE (4+8)

/***************************** Datatypes *******************************/

/* chunk header */
typedef struct
{
  ChunkId id;
  uint64  size;
} Chunk;

/***************************** Variables *******************************/
LOCAL struct
{
  bool(*eof)(void *userData);
  Errors(*read)(void *userData, void *buffer, ulong length, ulong *bytesRead);
  Errors(*write)(void *userData, const void *buffer, ulong length);
  Errors(*tell)(void *userData, uint64 *offset);
  Errors(*seek)(void *userData, uint64 offset);
  uint64(*getSize)(void *userData);
} IO;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : readDefinition
* Purpose: read chunk definition
* Input  : userData       - user data
*          definition     - chunk definition
*          definitionSize - chunk definition size (in bytes)
*          alignment      - chunk alignment
*          cryptInfo      - crypt info
* Output : data      - read data
*          bytesRead - number of bytes read
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors readDefinition(void      *userData,
                            const int *definition,
                            ulong     definitionSize,
                            uint      alignment,
                            CryptInfo *cryptInfo,
                            void      *data,
                            ulong     *bytesRead
                           )
{
  Errors error;
  ulong  bufferLength;
  byte   *buffer;
  byte   *p;
  uint32 crc;
  int    z;

  assert(bytesRead != NULL);

  /* allocate buffer */
  bufferLength = ALIGN(definitionSize,alignment);
  buffer = (byte*)malloc(bufferLength);
  if (buffer == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }

  /* read data */
  error = IO.read(userData,buffer,bufferLength,bytesRead);
  if (error != ERROR_NONE)
  {
    free(buffer);
    return error;
  }

  /* decrypt */
//cryptInfo=NULL;
  if (cryptInfo != NULL)
  {
// NYI ???: seed value?
    Crypt_reset(cryptInfo,0);
    if (Crypt_decrypt(cryptInfo,buffer,bufferLength) != ERROR_NONE)
    {
      free(buffer);
      return ERROR_DECRYPT_FAIL;
    }
  }

  /* read definition */
  crc = crc32(0,Z_NULL,0);
  p = buffer;
  if (definition != NULL)
  {
    for (z = 0; definition[z] != 0; z++)
    {
      switch (definition[z])
      {
        case CHUNK_DATATYPE_UINT8:
        case CHUNK_DATATYPE_INT8:
          {
            uint8 n;

            assert(p+1 <= buffer+bufferLength);
            crc = crc32(crc,p,1);
            n = (*((uint8*)p));
            p += 1;

            (*((uint8*)data)) = n; data = ((char*)data)+1+PADDING_INT8;
          }
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          {
            uint16 n;

            assert(p+2 <= buffer+bufferLength);
            crc = crc32(crc,p,2);
            n = ntohs(*((uint16*)p));
            p += 2;

            (*((uint16*)data)) = n; data = ((char*)data)+2+PADDING_INT16;
          }
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          {
            uint32 n;

            assert(p+4 <= buffer+bufferLength);
            crc = crc32(crc,p,4);
            n = ntohl(*((uint32*)p));
            p += 4;

            (*((uint32*)data)) = n; data = ((char*)data)+4+PADDING_INT32;
          }
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          {
            uint64 n;
            uint32 h,l;

            assert(p+8 <= buffer+bufferLength);
            crc = crc32(crc,p,4);
            h = ntohl(*((uint32*)p));
            p += 4;
            crc = crc32(crc,p,4);
            l = ntohl(*((uint32*)p));
            p += 4;
            n = (((uint64)h) << 32) | (((uint64)l << 0));

            (*((uint64*)data)) = n; data = ((char*)data)+8+PADDING_INT64;
          }
          break;
        case CHUNK_DATATYPE_STRING:
          {
            uint16 length;
            String s;

            assert(p+2 <= buffer+bufferLength);
            crc = crc32(crc,p,2);
            length = ntohs(*((uint16*)p));
            p += 2;
            assert(p+length <= buffer+bufferLength);
            crc = crc32(crc,p,length);
            s = String_newBuffer(p,length); p += length;

//pointer size???
            (*((String*)data)) = s; data = ((char*)data)+4+PADDING_ADDRESS;
          }
          break;
        case CHUNK_DATATYPE_DATA:
          break;
          break;
        case CHUNK_DATATYPE_CRC32:
          {
            uint32 n;

            assert(p+4 <= buffer+bufferLength);
            n = ntohl(*(uint32*)p);
            p += 4;

//fprintf(stderr,"%s,%d: n=%x crc=%x\n",__FILE__,__LINE__,n,crc);
            if (n != crc)
            {
              free(buffer);
              return ERROR_CRC_ERROR;
            }

            crc = crc32(0,Z_NULL,0);
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

  /* free resources */
  free(buffer);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeDefinition
* Purpose: write chunk definition
* Input  : userData       - user data
*          definition     - chunk definition
*          definitionSize - chunk definition size (in bytes)
*          alignment      - chunk alignment
*          cryptInfo      - crypt info
*          data           - chunk data
* Output : bytesWritten - number of bytes written
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors writeDefinition(void       *userData,
                             const int  *definition,
                             ulong      definitionSize,
                             uint       alignment,
                             CryptInfo  *cryptInfo,
                             const void *data,
                             ulong      *bytesWritten
                            )
{
  ulong  bufferLength;
  byte   *buffer;
  byte   *p;
  uint32 crc;
  int    z;
  Errors error;

  assert(bytesWritten != NULL);

  /* allocate buffer */
  bufferLength = ALIGN(definitionSize,alignment);
  buffer = (byte*)malloc(bufferLength);
  if (buffer == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }

  /* write definition */
  crc = crc32(0,Z_NULL,0);
  p = buffer;
  if (definition != NULL)
  {
    for (z = 0; definition[z] != 0; z++)
    {
      switch (definition[z])
      {
        case CHUNK_DATATYPE_UINT8:
        case CHUNK_DATATYPE_INT8:
          {
            uint8 n;

            n = (*((uint8*)data)); data = ((char*)data)+1+PADDING_INT8;

            assert(p+1 <= buffer+bufferLength);
            (*((uint8*)p)) = n;
            crc = crc32(crc,p,1);
            p += 1;
          }
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          {
            uint16 n;

            n = (*((uint16*)data)); data = ((char*)data)+2+PADDING_INT16;

            assert(p+2 <= buffer+bufferLength);
            (*((uint16*)p)) = htons(n);
            crc = crc32(crc,p,2);
            p += 2;
          }
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          {
            uint32 n;

            n = (*((uint32*)data)); data = ((char*)data)+4+PADDING_INT32;

            assert(p+4 <= buffer+bufferLength);
            (*((uint32*)p)) = htonl(n);
            crc = crc32(crc,p,4);
            p += 4;
          }
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          {
            uint64 n;
            uint32 h,l;

            n = (*((uint64*)data)); data = ((char*)data)+8+PADDING_INT64;

            assert(p+8 <= buffer+bufferLength);
            h = (n & 0xFFFFffff00000000LL) >> 32;
            l = (n & 0x00000000FFFFffffLL) >>  0;
            (*((uint32*)p)) = htonl(h);
            crc = crc32(crc,p,4);
            p += 4;
            (*((uint32*)p)) = htonl(l);
            crc = crc32(crc,p,4);
            p += 4;
          }
          break;
        case CHUNK_DATATYPE_STRING:
          {
            uint16 length;
            String s;

//pointer size???    
            s = (*((String*)data)); data = ((char*)data)+4+PADDING_ADDRESS;
            length = (uint16)String_length(s);

            assert(p+2 <= buffer+bufferLength);
            (*((uint16*)p)) = htons(length); \
            crc = crc32(crc,p,2);
            p += 2;
            assert(p+length <= buffer+bufferLength);
            memcpy(p,String_cString(s),length);
            crc = crc32(crc,p,length);
            p += length;
          }
          break;
        case CHUNK_DATATYPE_DATA:
          break;
        case CHUNK_DATATYPE_CRC32:
          {
            assert(p+4 <= buffer+bufferLength);
//fprintf(stderr,"%s,%d: crc=%lu\n",__FILE__,__LINE__,crc);
//crc=0xAAAAAAAA;
            (*((uint32*)p)) = htonl(crc);
            p += 4;

            crc = crc32(0,Z_NULL,0);
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

  /* encrypt */
//cryptInfo=NULL;
  if (cryptInfo != NULL)
  {
// NYI ???: seed value?
    Crypt_reset(cryptInfo,0);
    if (Crypt_encrypt(cryptInfo,buffer,bufferLength) != ERROR_NONE)
    {
      free(buffer);
      return ERROR_ENCRYPT_FAIL;
    }
  }

  /* write data */
  error = IO.write(userData,buffer,bufferLength);
  if (error != ERROR_NONE)
  {
    free(buffer);
    return error;
  }
  (*bytesWritten) = bufferLength;

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Chunk_initAll(bool(*eof)(void *userData),
                      Errors(*read)(void *userData, void *buffer, ulong length, ulong *bytesRead),
                      Errors(*write)(void *userData, const void *buffer, ulong length),
                      Errors(*tell)(void *userData, uint64 *offset),
                      Errors(*seek)(void *userData, uint64 offset),
                      uint64(*getSize)(void *userData)
                     )
{
  assert(eof != NULL);
  assert(read != NULL);
  assert(write != NULL);
  assert(tell != NULL);
  assert(seek != NULL);
  assert(getSize != NULL);

  IO.eof     = eof;
  IO.read    = read;
  IO.write   = write;
  IO.tell    = tell;
  IO.seek    = seek;
  IO.getSize = getSize;

  return ERROR_NONE;
}

void Chunk_doneAll(void)
{
}

const char *Chunk_idToString(ChunkId chunkId)
{
  static char s[5];
  char   ch;

  ch = (char)((chunkId & 0xFF000000) >> 24); s[0] = (isprint(ch))?ch:'.';
  ch = (char)((chunkId & 0x00FF0000) >> 16); s[1] = (isprint(ch))?ch:'.';
  ch = (char)((chunkId & 0x0000FF00) >>  8); s[2] = (isprint(ch))?ch:'.';
  ch = (char)((chunkId & 0x000000FF) >>  0); s[3] = (isprint(ch))?ch:'.';
  s[4] = '\0';

  return s;
}

ulong Chunk_getSize(const int  *definition,
                     ulong      alignment,
                     const void *data
                    )
{
  int   z;
  ulong definitionSize;

  assert(definition != NULL);

//  return getDefinitionSize(definition,alignment,data);

  definitionSize = 0;
  for (z = 0; definition[z] != 0; z++)
  {
    switch (definition[z])
    {
      case CHUNK_DATATYPE_UINT8:
      case CHUNK_DATATYPE_INT8:
        definitionSize += 1;
        if (data != NULL) data = ((char*)data) + 4;
        break;
      case CHUNK_DATATYPE_UINT16:
      case CHUNK_DATATYPE_INT16:
        definitionSize += 2;
        if (data != NULL) data = ((char*)data) + 4;
        break;
      case CHUNK_DATATYPE_UINT32:
      case CHUNK_DATATYPE_INT32:
      case CHUNK_DATATYPE_CRC32:
        definitionSize += 4;
        if (data != NULL) data = ((char*)data) + 4;
        break;
      case CHUNK_DATATYPE_UINT64:
      case CHUNK_DATATYPE_INT64:
        definitionSize += 8;
        if (data != NULL) data = ((char*)data) + 8;
        break;
      case CHUNK_DATATYPE_STRING:
        {
          String s;

          assert(data != NULL);

          s = (*((String*)data));
          assert(s != NULL);
          definitionSize += 2 + String_length(s);
//pointer size???
          data = ((char*)data) + 4;
        }
        break;
      case CHUNK_DATATYPE_DATA:
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
  }

  /* add padding for alignment */
  definitionSize = ALIGN(definitionSize,alignment);

  return definitionSize;
}

Errors Chunk_init(ChunkInfo *chunkInfo,
                  ChunkInfo *parentChunkInfo,
                  void      *userData,
                  uint      alignment,
                  CryptInfo *cryptInfo
                 )
{
  assert(chunkInfo != NULL);

  chunkInfo->parentChunkInfo = parentChunkInfo;
  chunkInfo->userData        = userData;

  chunkInfo->mode            = CHUNK_MODE_UNKNOWN;
  chunkInfo->alignment       = alignment;
  chunkInfo->cryptInfo       = cryptInfo;

  chunkInfo->id              = CHUNK_ID_NONE;
  chunkInfo->definition      = NULL;
  chunkInfo->definitionSize  = 0;
  chunkInfo->size            = 0;
  chunkInfo->offset          = 0;
  chunkInfo->index           = 0;

  return ERROR_NONE;
}

void Chunk_done(ChunkInfo *chunkInfo)
{
  assert(chunkInfo != NULL);

  UNUSED_VARIABLE(chunkInfo);
}

Errors Chunk_next(void        *userData,
                  ChunkHeader *chunkHeader
                 )
{
  Errors error;
  uint64 offset;
  Chunk  chunk;
  ulong  bytesRead;

  assert(chunkHeader != NULL);

  /* get current offset */
  error = IO.tell(userData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkHeader->offset = offset;

  /* read chunk header */
  error = readDefinition(userData,
                         CHUNK_DEFINITION,
                         CHUNK_DEFINITION_SIZE,
                         0,
                         NULL,
                         &chunk,
                         &bytesRead
                        );
  if (error != ERROR_NONE)
  {
    return FALSE;
  }
  chunkHeader->id   = chunk.id;
  chunkHeader->size = chunk.size;

  return ERROR_NONE;
}

Errors Chunk_skip(void        *userData,
                  ChunkHeader *chunkHeader
                 )
{
  uint64 size;
  uint64 offset;
  Errors error;

  assert(chunkHeader != NULL);

  /* fseeko in File_seek() cause an SigSegV if "offset" is completely wrong;
     thus use a valid offset only
  */
  size = IO.getSize(userData);
  offset = (chunkHeader->offset+CHUNK_HEADER_SIZE+chunkHeader->size < size)?chunkHeader->offset+CHUNK_HEADER_SIZE+chunkHeader->size:size;
  error = IO.seek(userData,offset);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

bool Chunk_eof(void *userData)
{
  return IO.eof(userData);
}

Errors Chunk_open(ChunkInfo   *chunkInfo,
                  ChunkHeader *chunkHeader,
                  ChunkId     chunkId,
                  const int   *definition,
                  ulong       definitionSize,
                  void        *data
                 )
{
  Errors error;
  ulong  bytesRead;

  assert(chunkInfo != NULL);
  assert(chunkId == chunkHeader->id);

  /* init */
  chunkInfo->id             = chunkId;
  chunkInfo->definition     = definition;
  chunkInfo->definitionSize = definitionSize;
  chunkInfo->size           = chunkHeader->size;
  chunkInfo->offset         = chunkHeader->offset;
  chunkInfo->mode           = CHUNK_MODE_READ;
  chunkInfo->index          = 0;

  error = readDefinition(chunkInfo->userData,
                         chunkInfo->definition,
                         chunkInfo->definitionSize,
                         chunkInfo->alignment,
                         chunkInfo->cryptInfo,
                         data,
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

Errors Chunk_create(ChunkInfo  *chunkInfo,
                    ChunkId    chunkId,
                    const int  *definition,
                    ulong      definitionSize,
                    const void *data
                   )
{
  Errors      error;
  uint64      offset;
  ChunkHeader chunkHeader;
  ulong       bytesWritten;

  assert(chunkInfo != NULL);
  assert(chunkId != CHUNK_ID_NONE);

  /* init */
  chunkInfo->id             = chunkId;
  chunkInfo->definition     = definition;
  chunkInfo->definitionSize = definitionSize;
  chunkInfo->size           = 0;
  chunkInfo->offset         = 0;
  chunkInfo->mode           = CHUNK_MODE_WRITE;
  chunkInfo->index          = 0;

  /* get size of chunk (without data elements) */
  chunkInfo->definitionSize = Chunk_getSize(definition,chunkInfo->alignment,data);

  /* get current offset */
  error = IO.tell(chunkInfo->userData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkInfo->offset = offset;

  /* write chunk header id */
  chunkHeader.id   = 0;
  chunkHeader.size = 0;
  error = writeDefinition(chunkInfo->userData,
                          CHUNK_DEFINITION,
                          CHUNK_DEFINITION_SIZE,
                          0,
                          NULL,
                          &chunkHeader,
                          &bytesWritten
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkInfo->index = 0;
  chunkInfo->size  = 0;
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += bytesWritten;
    chunkInfo->parentChunkInfo->size  += bytesWritten;
  }

  /* write chunk data */
  if (chunkInfo->definition != NULL)
  {
    error = writeDefinition(chunkInfo->userData,
                            chunkInfo->definition,
                            chunkInfo->definitionSize,
                            chunkInfo->alignment,
                            chunkInfo->cryptInfo,
                            data,
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

  switch (chunkInfo->mode)
  {
    case CHUNK_MODE_UNKNOWN:
      break;
    case CHUNK_MODE_WRITE:
      /* write size to chunk-header */
      error = IO.tell(chunkInfo->userData,&offset);
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = IO.seek(chunkInfo->userData,chunkInfo->offset);
      if (error != ERROR_NONE)
      {
        return error;
      }
      chunkHeader.id   = chunkInfo->id;
      chunkHeader.size = chunkInfo->size;
      error = writeDefinition(chunkInfo->userData,
                              CHUNK_DEFINITION,
                              CHUNK_DEFINITION_SIZE,
                              0,
                              NULL,
                              &chunkHeader,
                              &bytesWritten
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = IO.seek(chunkInfo->userData,offset);
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    case CHUNK_MODE_READ:
      /* seek to end of chunk */
      error = IO.seek(chunkInfo->userData,chunkInfo->offset+CHUNK_HEADER_SIZE+chunkInfo->size);
      if (error != ERROR_NONE)
      {
        return error;
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
  assert(chunkHeader != NULL);

  if ((chunkInfo->index + CHUNK_HEADER_SIZE) > chunkInfo->size)
  {
    return ERROR_END_OF_DATA;
  }

  /* get current offset */
  error = IO.tell(chunkInfo->userData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkHeader->offset = offset;

  /* read chunk header */
  error = readDefinition(chunkInfo->userData,
                         CHUNK_DEFINITION,
                         CHUNK_DEFINITION_SIZE,
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

  /* validate chunk */
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
  UNUSED_VARIABLE(chunkInfo);
  UNUSED_VARIABLE(chunkHeader);

  HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();

  return ERROR_NONE; 
}

bool Chunk_eofSub(ChunkInfo *chunkInfo)
{
  return chunkInfo->index >= chunkInfo->size;
}

Errors Chunk_update(ChunkInfo  *chunkInfo,
                    const void *data
                   )
{
  Errors error;
  uint64 offset;
  ulong  bytesWritten;

  /* get current offset */
  error = IO.tell(chunkInfo->userData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* update */
  error = IO.seek(chunkInfo->userData,chunkInfo->offset+CHUNK_HEADER_SIZE);
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = writeDefinition(chunkInfo->userData,
                          chunkInfo->definition,
                          chunkInfo->definitionSize,
                          chunkInfo->alignment,
                          chunkInfo->cryptInfo,
                          data,
                          &bytesWritten
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* restore offset */
  error = IO.seek(chunkInfo->userData,offset);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Chunk_readData(ChunkInfo *chunkInfo,
                      void      *data,
                      ulong     size
                     )
{
  Errors error;
  ulong  bytesRead;

  assert(chunkInfo != NULL);

  if (size > (chunkInfo->size - chunkInfo->index))
  {
    size = chunkInfo->size - chunkInfo->index;
  }

  error = IO.read(chunkInfo->userData,data,size,&bytesRead);
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (size != bytesRead)
  {
    return ERROR_IO_ERROR;
  }
  chunkInfo->index += size;
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += size;
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

  error = IO.write(chunkInfo->userData,data,size);
  if (error != ERROR_NONE)
  {
    return ERROR_IO_ERROR;
  }
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

  error = IO.tell(chunkInfo->userData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }
  offset += size;
  error = IO.seek(chunkInfo->userData,offset);
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkInfo->index += size;

  /* set size in container chunk */
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
