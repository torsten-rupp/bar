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

/***************************** Datatypes *******************************/

// chunk header
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
  int z;

  if (definition != NULL)
  {
    for (z = 0; definition[z+0] != 0; z+=2)
    {
      switch (definition[z+0])
      {
        case CHUNK_DATATYPE_UINT8:
        case CHUNK_DATATYPE_INT8:
          (*((uint8*)((byte*)data+definition[z+1]))) = 0;
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          (*((uint16*)((byte*)data+definition[z+1]))) = 0;
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          (*((uint32*)((byte*)data+definition[z+1]))) = 0L;
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          (*((uint64*)((byte*)data+definition[z+1]))) = 0LL;
          break;
        case CHUNK_DATATYPE_STRING:
          {
            String string;

            string = String_new();
            if (string == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }

            (*((String*)((byte*)data+definition[z+1]))) = string;
          }
          break;
        case CHUNK_DATATYPE_DATA:
          break;
        case CHUNK_DATATYPE_CRC32:
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
  int z;

  if (definition != NULL)
  {
    for (z = 0; definition[z+0] != 0; z+=2)
    {
      switch (definition[z+0])
      {
        case CHUNK_DATATYPE_UINT8:
        case CHUNK_DATATYPE_INT8:
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          break;
        case CHUNK_DATATYPE_STRING:
          String_delete(*((String*)((byte*)data+definition[z+1])));
          break;
        case CHUNK_DATATYPE_DATA:
          break;
        case CHUNK_DATATYPE_CRC32:
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
* Input  : io              - i/o functions
*          ioUserData      - user data for i/o
*          definition      - chunk definition
*          size            - chunk size (in bytes)
*          alignment       - chunk alignment
*          cryptInfo       - crypt info
* Output : data      - read data
*          bytesRead - number of bytes read
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors readDefinition(const ChunkIO *io,
                            void          *ioUserData,
                            const int     *definition,
                            uint          size,
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
  char   errorText[64];

  assert(io != NULL);
  assert(io->tell != NULL);
  assert(io->seek != NULL);
  assert(io->read != NULL);
  assert(bytesRead != NULL);

  // allocate buffer
  bufferLength = ALIGN(size,alignment);
  buffer = (byte*)calloc(bufferLength,1);
  if (buffer == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }

  // read data
  io->tell(ioUserData,&offset);
  error = io->read(ioUserData,buffer,bufferLength,bytesRead);
  if (error != ERROR_NONE)
  {
    io->seek(ioUserData,offset);
    free(buffer);
    return error;
  }

  // decrypt
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

  // read definition
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
              snprintf(errorText,sizeof(errorText),"at offset %llu",offset+(p-buffer));
              return ERRORX_(CORRUPT_DATA,0,errorText);
            }
            crc = crc32(crc,p,1);
            n = (*((uint8*)p));
            p += 1;

            (*((uint8*)((byte*)data+definition[z+1]))) = n;
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
              snprintf(errorText,sizeof(errorText),"at offset %llu",offset+(p-buffer));
              return ERRORX_(CORRUPT_DATA,0,errorText);
            }
            crc = crc32(crc,p,2);
            n = ntohs(*((uint16*)p));
            p += 2;

            (*((uint16*)((byte*)data+definition[z+1]))) = n;
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
              snprintf(errorText,sizeof(errorText),"at offset %llu",offset+(p-buffer));
              return ERRORX_(CORRUPT_DATA,0,errorText);
            }
            crc = crc32(crc,p,4);
            n = ntohl(*((uint32*)p));
            p += 4;

            (*((uint32*)((byte*)data+definition[z+1]))) = n;
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
              snprintf(errorText,sizeof(errorText),"at offset %llu",offset+(p-buffer));
              return ERRORX_(CORRUPT_DATA,0,errorText);
            }
            crc = crc32(crc,p,4);
            h = ntohl(*((uint32*)p));
            p += 4;
            crc = crc32(crc,p,4);
            l = ntohl(*((uint32*)p));
            p += 4;
            n = (((uint64)h) << 32) | (((uint64)l << 0));

            (*((uint64*)((byte*)data+definition[z+1]))) = n;
          }
          break;
        case CHUNK_DATATYPE_STRING:
          {
            uint16 length;
            String string;

            if (p+2 > buffer+bufferLength)
            {
              io->seek(ioUserData,offset);
              snprintf(errorText,sizeof(errorText),"at offset %llu",offset+(p-buffer));
              return ERRORX_(CORRUPT_DATA,0,errorText);
            }
            crc = crc32(crc,p,2);
            length = ntohs(*((uint16*)p));
            p += 2;
            if (p+length > buffer+bufferLength)
            {
              io->seek(ioUserData,offset);
              free(buffer);
              snprintf(errorText,sizeof(errorText),"at offset %llu",offset+(p-buffer));
              return ERRORX_(CORRUPT_DATA,0,errorText);
            }
            crc = crc32(crc,p,length);

            string = (*((String*)((byte*)data+definition[z+1])));
            assert(string != NULL);
            String_setBuffer(string,p,length); p += length;
          }
          break;
        case CHUNK_DATATYPE_DATA:
          break;
        case CHUNK_DATATYPE_CRC32:
          {
            uint32 n;

            if (p+4 > buffer+bufferLength)
            {
              io->seek(ioUserData,offset);
              free(buffer);
              snprintf(errorText,sizeof(errorText),"at offset %llu",offset+(p-buffer));
              return ERRORX_(CORRUPT_DATA,0,errorText);
            }
            n = ntohl(*(uint32*)p);
            p += 4;

            if (n != crc)
            {
              io->seek(ioUserData,offset);
              free(buffer);
              snprintf(errorText,sizeof(errorText),"at offset %llu",offset);
              return ERRORX_(CRC_,0,errorText);
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

  // free resources
  free(buffer);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeDefinition
* Purpose: write chunk definition
* Input  : io              - i/o functions
*          ioUserData      - user data for i/o
*          definition      - chunk definition
*          size            - chunk size (in bytes)
*          alignment       - chunk alignment
*          cryptInfo       - crypt info
*          data            - chunk data
* Output : bytesWritten - number of bytes written
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors writeDefinition(const ChunkIO *io,
                             void          *ioUserData,
                             const int     *definition,
                             uint          size,
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
  assert(io->write != NULL);
  assert(bytesWritten != NULL);

  // allocate buffer
  bufferLength = ALIGN(size,alignment);
  buffer = (byte*)calloc(bufferLength,1);
  if (buffer == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }

  // write definition
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

            n = (*((uint8*)((byte*)data+definition[z+1])));

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

            n = (*((uint16*)((byte*)data+definition[z+1])));

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

            n = (*((uint32*)((byte*)data+definition[z+1])));

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

            n = (*((uint64*)((byte*)data+definition[z+1])));

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
            String string;

            string = (*((String*)((byte*)data+definition[z+1])));
            length = (uint16)String_length(string);

            assert(p+2 <= buffer+bufferLength);
            (*((uint16*)p)) = htons(length); \
            crc = crc32(crc,p,2);
            p += 2;
            assert(p+length <= buffer+bufferLength);
            memcpy(p,String_cString(string),length);
            crc = crc32(crc,p,length);
            p += length;
          }
          break;
        case CHUNK_DATATYPE_DATA:
          break;
        case CHUNK_DATATYPE_CRC32:
          {
            assert(p+4 <= buffer+bufferLength);
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

  // encrypt
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

  // write data
  error = io->write(ioUserData,buffer,bufferLength);
  if (error != ERROR_NONE)
  {
    free(buffer);
    return error;
  }
  (*bytesWritten) = bufferLength;

  // free resources
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

          s = (*((String*)((byte*)data+definition[z+1])));
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

  // add padding for alignment
  definitionSize = ALIGN(definitionSize,alignment);

  return definitionSize;
}

Errors Chunk_init(ChunkInfo     *chunkInfo,
                  ChunkInfo     *parentChunkInfo,
                  const ChunkIO *io,
                  void          *ioUserData,
                  ChunkId       chunkId,
                  const int     *definition,
                  uint          alignment,
                  CryptInfo     *cryptInfo,
                  void          *data
                 )
{
  assert(chunkInfo != NULL);

  chunkInfo->parentChunkInfo = parentChunkInfo;
  chunkInfo->io              = (parentChunkInfo != NULL)?parentChunkInfo->io:io;
  chunkInfo->ioUserData      = (parentChunkInfo != NULL)?parentChunkInfo->ioUserData:ioUserData;

  chunkInfo->mode            = CHUNK_MODE_UNKNOWN;
  chunkInfo->alignment       = alignment;
  chunkInfo->cryptInfo       = cryptInfo;

  chunkInfo->id              = chunkId;
  chunkInfo->definition      = definition;
  chunkInfo->chunkSize       = 0;
  chunkInfo->size            = 0;
  chunkInfo->offset          = 0;
  chunkInfo->index           = 0;

  chunkInfo->data            = data;

  initDefinition(definition,data);

  return ERROR_NONE;
}

void Chunk_done(ChunkInfo *chunkInfo)
{
  assert(chunkInfo != NULL);

  doneDefinition(chunkInfo->definition,chunkInfo->data);
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
  assert(io->tell != NULL);
  assert(chunkHeader != NULL);

  // get current offset
  error = io->tell(ioUserData,&offset);
  if (error != ERROR_NONE)
  {
    return error;
  }
  chunkHeader->offset = offset;

  // read chunk header
  error = readDefinition(io,
                         ioUserData,
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
  assert(io->getSize != NULL);
  assert(io->seek != NULL);
  assert(chunkHeader != NULL);

  size = io->getSize(ioUserData);
  if (chunkHeader->offset+CHUNK_HEADER_SIZE+chunkHeader->size > size)
  {
    return ERROR_INVALID_CHUNK_SIZE;
  }

  /* Note: fseeko in File_seek() cause an SigSegV if "offset" is
     completely wrong; thus never call with an invalid offset
  */
  offset = (chunkHeader->offset+CHUNK_HEADER_SIZE+chunkHeader->size <= size)
             ? chunkHeader->offset+CHUNK_HEADER_SIZE+chunkHeader->size
             : size+1;
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
  assert(io->eof != NULL);

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
  chunkInfo->index     = 0;

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
  chunkInfo->size   = 0;
  chunkInfo->offset = 0;
  chunkInfo->mode   = CHUNK_MODE_WRITE;
  chunkInfo->index  = 0;

  // get size of chunk (without data elements)
  chunkInfo->chunkSize = Chunk_getSize(chunkInfo->definition,chunkInfo->alignment,chunkInfo->data);

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
  chunkInfo->index = 0;
  chunkInfo->size  = 0;
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

  if ((chunkInfo->index+CHUNK_HEADER_SIZE) > chunkInfo->size)
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
  assert(chunkInfo->chunkSize == Chunk_getSize(chunkInfo->definition,chunkInfo->alignment,chunkInfo->data));

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
    // check if requested all data read
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
