/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/chunks.c,v $
* $Revision: 1.3 $
* $Author: torsten $
* Contents: Backup ARchiver file chunks functions
* Systems: all
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
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "bar.h"
#include "archive_format.h"

#include "chunks.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/* chunk header definition */
LOCAL int CHUNK_DEFINITION[] = {
                                 CHUNK_DATATYPE_UINT32,offsetof(ChunkHeader,id),
                                 CHUNK_DATATYPE_UINT64,offsetof(ChunkHeader,size),
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

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : readDefinition
* Purpose: read chunk definition
* Input  : io             - i/o functions
*          ioUserData     - user data for i/o
*          definition     - chunk definition
*          definitionSize - chunk definition size (in bytes)
*          alignment      - chunk alignment
*          cryptInfo      - crypt info
* Output : data      - read data
*          bytesRead - number of bytes read
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors readDefinition(const ChunkIO *io,
                            void          *ioUserData,
                            const int     *definition,
                            ulong         definitionSize,
                            uint          alignment,
                            CryptInfo     *cryptInfo,
                            void          *data,
                            ulong         *bytesRead
                           )
{
  Errors error;
  ulong  bufferLength;
  byte   *buffer;
  uint64 offset;
  byte   *p;
  uint32 crc;
  int    z;

  assert(io != NULL);
  assert(bytesRead != NULL);

  /* allocate buffer */
  bufferLength = ALIGN(definitionSize,alignment);
  buffer = (byte*)calloc(bufferLength,1);
  if (buffer == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }

  /* read data */
  io->tell(ioUserData,&offset);
  error = io->read(ioUserData,buffer,bufferLength,bytesRead);
  if (error != ERROR_NONE)
  {
    io->seek(ioUserData,offset);
    free(buffer);
    return error;
  }

  /* decrypt */
  if (cryptInfo != NULL)
  {
// NYI ???: seed value?
    Crypt_reset(cryptInfo,0);
    if (Crypt_decrypt(cryptInfo,buffer,bufferLength) != ERROR_NONE)
    {
      io->seek(ioUserData,offset);
      free(buffer);
      return ERROR_DECRYPT_FAIL;
    }
  }

  /* read definition */
  crc = crc32(0,Z_NULL,0);
  p = buffer;
  if (definition != NULL)
  {
    for (z = 0; definition[z+0] != 0; z+=2)
    {
      switch (definition[z+0])
      {
        case CHUNK_DATATYPE_UINT8:
        case CHUNK_DATATYPE_INT8:
          {
            uint8 n;

            if (p+1 > buffer+bufferLength)
            {
              io->seek(ioUserData,offset);
              free(buffer);
              return ERROR_CORRUPT_DATA;
            }
            crc = crc32(crc,p,1);
            n = (*((uint8*)p));
            p += 1;

            (*((uint8*)((char*)data+definition[z+1]))) = n;
          }
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          {
            uint16 n;

            if (p+2 > buffer+bufferLength)
            {
              io->seek(ioUserData,offset);
              free(buffer);
              return ERROR_CORRUPT_DATA;
            }
            crc = crc32(crc,p,2);
            n = ntohs(*((uint16*)p));
            p += 2;

            (*((uint16*)((char*)data+definition[z+1]))) = n;
          }
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          {
            uint32 n;

            if (p+4 > buffer+bufferLength)
            {
              io->seek(ioUserData,offset);
              free(buffer);
              return ERROR_CORRUPT_DATA;
            }
            crc = crc32(crc,p,4);
            n = ntohl(*((uint32*)p));
            p += 4;

            (*((uint32*)((char*)data+definition[z+1]))) = n;
          }
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          {
            uint64 n;
            uint32 h,l;

            if (p+8 > buffer+bufferLength)
            {
              io->seek(ioUserData,offset);
              free(buffer);
              return ERROR_CORRUPT_DATA;
            }
            crc = crc32(crc,p,4);
            h = ntohl(*((uint32*)p));
            p += 4;
            crc = crc32(crc,p,4);
            l = ntohl(*((uint32*)p));
            p += 4;
            n = (((uint64)h) << 32) | (((uint64)l << 0));

            (*((uint64*)((char*)data+definition[z+1]))) = n;
          }
          break;
        case CHUNK_DATATYPE_STRING:
          {
            uint16 length;
            String s;

            if (p+2 > buffer+bufferLength)
            {
              io->seek(ioUserData,offset);
              return ERROR_CORRUPT_DATA;
            }
            crc = crc32(crc,p,2);
            length = ntohs(*((uint16*)p));
            p += 2;
            if (p+length > buffer+bufferLength)
            {
              io->seek(ioUserData,offset);
              free(buffer);
              return ERROR_CORRUPT_DATA;
            }
            crc = crc32(crc,p,length);
            s = String_newBuffer(p,length); p += length;

            (*((String*)((char*)data+definition[z+1]))) = s;
          }
          break;
        case CHUNK_DATATYPE_DATA:
          break;
          break;
        case CHUNK_DATATYPE_CRC32:
          {
            uint32 n;

            if (p+4 > buffer+bufferLength)
            {
              io->seek(ioUserData,offset);
              free(buffer);
              return ERROR_CORRUPT_DATA;
            }
            n = ntohl(*(uint32*)p);
            p += 4;

//fprintf(stderr,"%s,%d: n=%x crc=%x\n",__FILE__,__LINE__,n,crc);
            if (n != crc)
            {
              io->seek(ioUserData,offset);
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
* Input  : io             - i/o functions
*          ioUserData     - user data for i/o
*          definition     - chunk definition
*          definitionSize - chunk definition size (in bytes)
*          alignment      - chunk alignment
*          cryptInfo      - crypt info
*          data           - chunk data
* Output : bytesWritten - number of bytes written
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors writeDefinition(const ChunkIO *io,
                             void          *ioUserData,
                             const int     *definition,
                             ulong         definitionSize,
                             uint          alignment,
                             CryptInfo     *cryptInfo,
                             const void    *data,
                             ulong         *bytesWritten
                            )
{
  ulong  bufferLength;
  byte   *buffer;
  byte   *p;
  uint32 crc;
  int    z;
  Errors error;

  assert(io != NULL);
  assert(bytesWritten != NULL);

  /* allocate buffer */
  bufferLength = ALIGN(definitionSize,alignment);
  buffer = (byte*)calloc(bufferLength,1);
  if (buffer == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }

  /* write definition */
  crc = crc32(0,Z_NULL,0);
  p = buffer;
  if (definition != NULL)
  {
    for (z = 0; definition[z+0] != 0; z+=2)
    {
      switch (definition[z+0])
      {
        case CHUNK_DATATYPE_UINT8:
        case CHUNK_DATATYPE_INT8:
          {
            uint8 n;

            n = (*((uint8*)((char*)data+definition[z+1])));

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

            n = (*((uint16*)((char*)data+definition[z+1])));

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

            n = (*((uint32*)((char*)data+definition[z+1])));

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

            n = (*((uint64*)((char*)data+definition[z+1])));

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

            s = (*((String*)((char*)data+definition[z+1])));
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
  error = io->write(ioUserData,buffer,bufferLength);
  if (error != ERROR_NONE)
  {
    free(buffer);
    return error;
  }
  (*bytesWritten) = bufferLength;

  /* free resources */
  free(buffer);

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

  definitionSize = 0;
  for (z = 0; definition[z+0] != 0; z+=2)
  {
    switch (definition[z+0])
    {
      case CHUNK_DATATYPE_UINT8:
      case CHUNK_DATATYPE_INT8:
        definitionSize += 1;
        break;
      case CHUNK_DATATYPE_UINT16:
      case CHUNK_DATATYPE_INT16:
        definitionSize += 2;
        break;
      case CHUNK_DATATYPE_UINT32:
      case CHUNK_DATATYPE_INT32:
      case CHUNK_DATATYPE_CRC32:
        definitionSize += 4;
        break;
      case CHUNK_DATATYPE_UINT64:
      case CHUNK_DATATYPE_INT64:
        definitionSize += 8;
        break;
      case CHUNK_DATATYPE_STRING:
        {
          String s;

          assert(data != NULL);

          s = (*((String*)((char*)data+definition[z+1])));
          assert(s != NULL);
          definitionSize += 2+String_length(s);
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

Errors Chunk_init(ChunkInfo     *chunkInfo,
                  ChunkInfo     *parentChunkInfo,
                  const ChunkIO *io,
                  void          *ioUserData,
                  uint          alignment,
                  CryptInfo     *cryptInfo
                 )
{
  assert(chunkInfo != NULL);

  chunkInfo->parentChunkInfo = parentChunkInfo;
  chunkInfo->io              = (parentChunkInfo != NULL)?parentChunkInfo->io:io;
  chunkInfo->ioUserData      = (parentChunkInfo != NULL)?parentChunkInfo->ioUserData:ioUserData;

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

Errors Chunk_next(const ChunkIO *io,
                  void          *ioUserData,
                  ChunkHeader   *chunkHeader
                 )
{
  Errors error;
  uint64 offset;
  Chunk  chunk;
  ulong  bytesRead;

  assert(io != NULL);
  assert(chunkHeader != NULL);

  /* get current offset */
  error = io->tell(ioUserData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkHeader->offset = offset;

  /* read chunk header */
  error = readDefinition(io,
                         ioUserData,
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

Errors Chunk_skip(const ChunkIO     *io,
                  void              *ioUserData,
                  const ChunkHeader *chunkHeader
                 )
{
  uint64 size;
  uint64 offset;
  Errors error;

  assert(io != NULL);
  assert(chunkHeader != NULL);

  /* fseeko in File_seek() cause an SigSegV if "offset" is completely wrong;
     thus use a valid offset only
  */
  size = io->getSize(ioUserData);
  offset = (chunkHeader->offset+CHUNK_HEADER_SIZE+chunkHeader->size < size)?chunkHeader->offset+CHUNK_HEADER_SIZE+chunkHeader->size:size;
  error = io->seek(ioUserData,offset);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

bool Chunk_eof(const ChunkIO *io,
               void          *ioUserData
              )
{
  assert(io != NULL);

  return io->eof(ioUserData);
}

void Chunk_tell(ChunkInfo *chunkInfo, uint64 *index)
{
  #ifndef NDEBUG
    Errors error;
    uint64 n;
  #endif /* NDEBUG */

  assert(chunkInfo != NULL);
  assert(chunkInfo->io != NULL);
  assert(index != NULL);

  #ifndef NDEBUG
    error = chunkInfo->io->tell(chunkInfo->ioUserData,&n);
    assert(error == ERROR_NONE);
    assert(n == chunkInfo->offset + CHUNK_HEADER_SIZE + chunkInfo->index);
  #endif /* NDEBUG */

  (*index) = chunkInfo->index;
}

Errors Chunk_seek(ChunkInfo *chunkInfo, uint64 index)
{
  assert(chunkInfo != NULL);
  assert(chunkInfo->io != NULL);

  chunkInfo->index = index;
  return chunkInfo->io->seek(chunkInfo->ioUserData,chunkInfo->offset + CHUNK_HEADER_SIZE + index);
}

Errors Chunk_open(ChunkInfo         *chunkInfo,
                  const ChunkHeader *chunkHeader,
                  ChunkId           chunkId,
                  const int         *definition,
                  ulong             definitionSize,
                  void              *data
                 )
{
  Errors error;
  ulong  bytesRead;

  assert(chunkInfo != NULL);
  assert(chunkInfo->io != NULL);
  assert(chunkId == chunkHeader->id);

  /* init */
  chunkInfo->id             = chunkId;
  chunkInfo->definition     = definition;
  chunkInfo->definitionSize = definitionSize;
  chunkInfo->size           = chunkHeader->size;
  chunkInfo->offset         = chunkHeader->offset;
  chunkInfo->mode           = CHUNK_MODE_READ;
  chunkInfo->index          = 0;

  error = readDefinition(chunkInfo->io,
                         chunkInfo->ioUserData,
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
  assert(chunkInfo->io != NULL);
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
  error = chunkInfo->io->tell(chunkInfo->ioUserData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkInfo->offset = offset;

  /* write chunk header id */
  chunkHeader.id   = 0;
  chunkHeader.size = 0;
  error = writeDefinition(chunkInfo->io,
                          chunkInfo->ioUserData,
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
    error = writeDefinition(chunkInfo->io,
                            chunkInfo->ioUserData,
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
  assert(chunkInfo->io != NULL);

  switch (chunkInfo->mode)
  {
    case CHUNK_MODE_UNKNOWN:
      break;
    case CHUNK_MODE_WRITE:
      /* write size to chunk-header */
      error = chunkInfo->io->tell(chunkInfo->ioUserData,&offset);
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = chunkInfo->io->seek(chunkInfo->ioUserData,chunkInfo->offset);
      if (error != ERROR_NONE)
      {
        return error;
      }
      chunkHeader.id   = chunkInfo->id;
      chunkHeader.size = chunkInfo->size;
      error = writeDefinition(chunkInfo->io,
                              chunkInfo->ioUserData,
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

      error = chunkInfo->io->seek(chunkInfo->ioUserData,offset);
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    case CHUNK_MODE_READ:
      /* seek to end of chunk */
      error = chunkInfo->io->seek(chunkInfo->ioUserData,chunkInfo->offset+CHUNK_HEADER_SIZE+chunkInfo->size);
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
  assert(chunkInfo->io != NULL);
  assert(chunkHeader != NULL);

  if ((chunkInfo->index + CHUNK_HEADER_SIZE) > chunkInfo->size)
  {
    return ERROR_END_OF_DATA;
  }

  /* get current offset */
  error = chunkInfo->io->tell(chunkInfo->ioUserData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkHeader->offset = offset;

  /* read chunk header */
  error = readDefinition(chunkInfo->io,
                         chunkInfo->ioUserData,
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
  assert(chunkInfo != NULL);
  assert(chunkInfo->io != NULL);

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

  assert(chunkInfo != NULL);
  assert(chunkInfo->io != NULL);

  /* get current offset */
  error = chunkInfo->io->tell(chunkInfo->ioUserData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* update */
  error = chunkInfo->io->seek(chunkInfo->ioUserData,chunkInfo->offset+CHUNK_HEADER_SIZE);
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = writeDefinition(chunkInfo->io,
                          chunkInfo->ioUserData,
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
  error = chunkInfo->io->seek(chunkInfo->ioUserData,offset);
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
  assert(chunkInfo->io != NULL);

  if (size > (chunkInfo->size - chunkInfo->index))
  {
    size = chunkInfo->size - chunkInfo->index;
  }

  error = chunkInfo->io->read(chunkInfo->ioUserData,data,size,&bytesRead);
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (size != bytesRead)
  {
    return ERROR(IO_ERROR,errno);
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
  assert(chunkInfo->io != NULL);

  error = chunkInfo->io->write(chunkInfo->ioUserData,data,size);
  if (error != ERROR_NONE)
  {
    return ERROR(IO_ERROR,errno);
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
  assert(chunkInfo->io != NULL);

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
