/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/chunks.c,v $
* $Revision: 1.9 $
* $Author: torsten $
* Contents: Backup ARchiver file chunks functions
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
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
  bool(*endOfFile)(void *userData);
  bool(*readFile)(void *userData, void *buffer, ulong length);
  bool(*writeFile)(void *userData, const void *buffer, ulong length);
  bool(*tellFile)(void *userData, uint64 *offset);
  bool(*seekFile)(void *userData, uint64 offset);
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
* Return : TRUE if read, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool readDefinition(void      *userData,
                          const int *definition,
                          ulong     definitionSize,
                          uint      alignment,
                          CryptInfo *cryptInfo,
                          void      *data,
                          ulong     *bytesRead
                         )
{
  ulong bufferLength;
  byte  *buffer;
  byte  *p;
  int   z;

  assert(bytesRead != NULL);

  /* allocate buffer */
  bufferLength = ALIGN(definitionSize,alignment);
  buffer = (byte*)malloc(bufferLength);
  if (buffer == NULL)
  {
    return FALSE;
  }

  /* read data */
  if (!IO.readFile(userData,buffer,bufferLength))
  {
    free(buffer);
    return FALSE;
  }
  (*bytesRead) = bufferLength;

  /* decrypt */
//cryptInfo=NULL;
  if (cryptInfo != NULL)
  {
// NYI ???: seed value?
    Crypt_reset(cryptInfo,0);
    if (Crypt_decrypt(cryptInfo,buffer,bufferLength) != ERROR_NONE)
    {
      return FALSE;
    }
  }

  /* read definition */
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
            n = (*(uint8*)p); p+=1;

            (*((uint8*)data)) = n; data = ((char*)data)+1+PADDING_INT8;
          }
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          {
            uint16 n;

            assert(p+2 <= buffer+bufferLength);
            n = ntohs(*((uint16*)p)); p+=2;

            (*((uint16*)data)) = n; data = ((char*)data)+2+PADDING_INT16;
          }
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          {
            uint32 n;

            assert(p+4 <= buffer+bufferLength);
            n = ntohl(*((uint32*)p)); p+=4;

            (*((uint32*)data)) = n; data = ((char*)data)+4+PADDING_INT32;
          }
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          {
            uint64 n;
            uint32 l[2];

            assert(p+8 <= buffer+bufferLength);
            l[0] = ntohl(*((uint32*)p)); p+=4;
            l[1] = ntohl(*((uint32*)p)); p+=4;
            n = (((uint64)l[0]) << 32) | (((uint64)l[1] << 0));

            (*((uint64*)data)) = n; data = ((char*)data)+8+PADDING_INT64;
          }
          break;
        case CHUNK_DATATYPE_NAME:
          {
            uint16 length;
            String s;

            assert(p+2 <= buffer+bufferLength);
            length = ntohs(*((uint16*)p)); p += 2;
            assert(p+length <= buffer+bufferLength);
            s = String_newBuffer(p,length); p += length;

//pointer size???
            (*((String*)data)) = s; data = ((char*)data)+4+PADDING_ADDRESS;
          }
          break;
        case CHUNK_DATATYPE_DATA:
          break;
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      }
    }
  }

#if 0
  /* read padding for alignment */
  if ((alignment > 0) && (((*bytesRead)%alignment) > 0))
  {
    padSize = alignment-(*bytesRead)%alignment;
    while (padSize > 0)
    {
      length = MIN(padSize,sizeof(padBuffer));
      if (!IO.readFile(userData,padBuffer,length))
      {
        return FALSE;
      }
      (*bytesRead) += length;

      padSize -= length;
    }
  }
#endif /* 0 */

  /* free resources */
  free(buffer);

  return TRUE;
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
* Return : TRUE if written, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool writeDefinition(void       *userData,
                           const int  *definition,
                           ulong      definitionSize,
                           uint       alignment,
                           CryptInfo  *cryptInfo,
                           const void *data,
                           ulong      *bytesWritten
                          )
{
  ulong bufferLength;
  byte  *buffer;
  byte  *p;
  int   z;

  assert(bytesWritten != NULL);

  /* allocate buffer */
  bufferLength = ALIGN(definitionSize,alignment);
  buffer = (byte*)malloc(bufferLength);
  if (buffer == NULL)
  {
    return FALSE;
  }

  /* write definition */
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
            (*((uint8*)p)) = n; p += 1;
          }
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          {
            uint16 n;

            n = htons(*((uint16*)data)); data = ((char*)data)+2+PADDING_INT16;

            assert(p+2 <= buffer+bufferLength);
            (*((uint16*)p)) = n; p += 2;
          }
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          {
            uint32 n;

            n = htonl(*((uint32*)data)); data = ((char*)data)+4+PADDING_INT32;

            assert(p+4 <= buffer+bufferLength);
            (*((uint32*)p)) = n; p += 4;       
          }
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          {
            uint64 n;
            uint32 l[2];

            n = (*((uint64*)data)); data = ((char*)data)+8+PADDING_INT64;

            assert(p+8 <= buffer+bufferLength);
            l[0] = htonl((n & 0xFFFFffff00000000LL) >> 32);
            l[1] = htonl((n & 0x00000000FFFFffffLL) >>  0);
            (*((uint32*)p)) = l[0]; p += 4;
            (*((uint32*)p)) = l[1]; p += 4;
          }
          break;
        case CHUNK_DATATYPE_NAME:
          {
            uint   length;
            String s;
            uint16 n;

//pointer size???    
            s = (*((String*)data)); data = ((char*)data)+4+PADDING_ADDRESS;
            length = String_length(s);

            assert(p+2 <= buffer+bufferLength);
            n = htons(length);
            (*((uint16*)p)) = n; p += 2;
            assert(p+length <= buffer+bufferLength);
            memcpy(p,String_cString(s),length); p += length;
          }
          break;
        case CHUNK_DATATYPE_DATA:
          break;
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      }
    }
  }

#if 0
  /* write padding for alignment */
  if ((alignment > 0) && (((*bytesWritten)%alignment) > 0))
  {
    memset(padBuffer,0,sizeof(padBuffer));
    padSize = alignment-(*bytesWritten)%alignment;
    while (padSize > 0)
    {
      length = MIN(padSize,sizeof(padBuffer));
      if (!IO.writeFile(userData,padBuffer,length))
      {
        return FALSE;
      }
      (*bytesWritten) += length;

      padSize -= length;
    }
  }
#endif /* 0 */

  /* encrypt */
//cryptInfo=NULL;
  if (cryptInfo != NULL)
  {
// NYI ???: seed value?
    Crypt_reset(cryptInfo,0);
    if (Crypt_encrypt(cryptInfo,buffer,bufferLength) != ERROR_NONE)
    {
      return FALSE;
    }
  }

  /* write data */
  if (!IO.writeFile(userData,buffer,bufferLength))
  {
    free(buffer);
    return FALSE;
  }
  (*bytesWritten) = bufferLength;

  return TRUE;
}

/*---------------------------------------------------------------------*/

Errors Chunks_init(bool(*endOfFile)(void *userData),
                   bool(*readFile)(void *userData, void *buffer, ulong length),
                   bool(*writeFile)(void *userData, const void *buffer, ulong length),
                   bool(*tellFile)(void *userData, uint64 *offset),
                   bool(*seekFile)(void *userData, uint64 offset)
                 )
{
  assert(endOfFile != NULL);
  assert(readFile != NULL);
  assert(writeFile != NULL);
  assert(tellFile != NULL);
  assert(seekFile != NULL);

  IO.endOfFile = endOfFile;
  IO.readFile  = readFile;
  IO.writeFile = writeFile;
  IO.tellFile  = tellFile;
  IO.seekFile  = seekFile;

  return ERROR_NONE;
}

void Chunks_doneF(void)
{
}

ulong Chunks_getSize(const int  *definition,
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
        definitionSize += 4;
        if (data != NULL) data = ((char*)data) + 4;
        break;
      case CHUNK_DATATYPE_UINT64:
      case CHUNK_DATATYPE_INT64:
        definitionSize += 8;
        if (data != NULL) data = ((char*)data) + 8;
        break;
      case CHUNK_DATATYPE_NAME:
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
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    }
  }

  /* add padding for alignment */
  definitionSize = ALIGN(definitionSize,alignment);

  return definitionSize;
}

bool Chunks_new(ChunkInfo *chunkInfo,
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

  return TRUE;
}

void Chunks_delete(ChunkInfo *chunkInfo)
{
  assert(chunkInfo != NULL);
}

bool Chunks_next(void        *userData,
                 ChunkHeader *chunkHeader)
{
  uint64 offset;
  Chunk  chunk;
  ulong  bytesRead;

  assert(chunkHeader != NULL);

  /* get current offset */
  if (!IO.tellFile(userData,&offset))
  {
    return FALSE;
  }
  chunkHeader->offset = offset;

  /* read chunk header */
  if (!readDefinition(userData,
                      CHUNK_DEFINITION,
                      CHUNK_DEFINITION_SIZE,
                      0,
                      NULL,
                      &chunk,
                      &bytesRead
                     )
     )
  {
    return FALSE;
  }
  chunkHeader->id   = chunk.id;
  chunkHeader->size = chunk.size;

  return TRUE;
}

bool Chunks_skip(void        *userData,
                 ChunkHeader *chunkHeader
                )
{
  assert(chunkHeader != NULL);

  if (!IO.seekFile(userData,chunkHeader->offset+CHUNK_HEADER_SIZE+chunkHeader->size))
  {
    return FALSE;
  }

  return TRUE;
}

bool Chunks_eof(void *userData)
{
  return IO.endOfFile(userData);
}

bool Chunks_open(ChunkInfo   *chunkInfo,
                 ChunkHeader *chunkHeader,
                 ChunkId     chunkId,
                 const int   *definition,
                 ulong       definitionSize,
                 void        *data
                )
{
  ulong bytesRead;

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

  if (!readDefinition(chunkInfo->userData,
                      chunkInfo->definition,
                      chunkInfo->definitionSize,
                      chunkInfo->alignment,
                      chunkInfo->cryptInfo,
                      data,
                      &bytesRead
                     )
     )
  {
    return FALSE;
  }
  chunkInfo->index += bytesRead;
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += bytesRead;
  }

  return TRUE;
}

bool Chunks_create(ChunkInfo  *chunkInfo,
                   ChunkId    chunkId,
                   const int  *definition,
                   ulong      definitionSize,
                   const void *data
                  )
{
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
  chunkInfo->definitionSize = Chunks_getSize(definition,chunkInfo->alignment,data);

  /* get current offset */
  if (!IO.tellFile(chunkInfo->userData,&offset))
  {
    return FALSE;
  }
  chunkInfo->offset = offset;

  /* write chunk header id */
  chunkHeader.id   = 0;
  chunkHeader.size = 0;
  if (!writeDefinition(chunkInfo->userData,
                       CHUNK_DEFINITION,
                       CHUNK_DEFINITION_SIZE,
                       0,
                       NULL,
                       &chunkHeader,
                       &bytesWritten
                      )
     )
  {
    return FALSE;
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
    if (!writeDefinition(chunkInfo->userData,
                         chunkInfo->definition,
                         chunkInfo->definitionSize,
                         chunkInfo->alignment,
                         chunkInfo->cryptInfo,
                         data,
                         &bytesWritten
                        )
       )
    {
      return FALSE;
    }
    chunkInfo->index += bytesWritten;
    chunkInfo->size  += bytesWritten;
    if (chunkInfo->parentChunkInfo != NULL)
    {
      chunkInfo->parentChunkInfo->index += bytesWritten;
      chunkInfo->parentChunkInfo->size  += bytesWritten;
    }
  }

  return TRUE;
}

bool Chunks_close(ChunkInfo *chunkInfo)
{
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
      if (!IO.tellFile(chunkInfo->userData,&offset))
      {
        return FALSE;
      }

      if (!IO.seekFile(chunkInfo->userData,chunkInfo->offset))
      {
        return FALSE;
      }
      chunkHeader.id   = chunkInfo->id;
      chunkHeader.size = chunkInfo->size;
      if (!writeDefinition(chunkInfo->userData,
                           CHUNK_DEFINITION,
                           CHUNK_DEFINITION_SIZE,
                           0,
                           NULL,
                           &chunkHeader,
                           &bytesWritten
                          )
         )
      {
        return FALSE;
      }

      if (!IO.seekFile(chunkInfo->userData,offset))
      {
        return FALSE;
      }
    case CHUNK_MODE_READ:
      if (!IO.seekFile(chunkInfo->userData,chunkInfo->offset+CHUNK_HEADER_SIZE+chunkInfo->size))
      {
        return FALSE;
      }
      break;
  }

  return TRUE; 
}

bool Chunks_nextSub(ChunkInfo   *chunkInfo,
                    ChunkHeader *chunkHeader
                   )
{
  uint64 offset;
  Chunk  chunk;
  ulong  bytesRead;

  assert(chunkInfo != NULL);
  assert(chunkHeader != NULL);

  if ((chunkInfo->index + CHUNK_HEADER_SIZE) > chunkInfo->size)
  {
    return FALSE;
  }

  /* get current offset */
  if (!IO.tellFile(chunkInfo->userData,&offset))
  {
    return FALSE;
  }
  chunkHeader->offset = offset;

  /* read chunk header */
  if (!readDefinition(chunkInfo->userData,
                      CHUNK_DEFINITION,
                      CHUNK_DEFINITION_SIZE,
                      0,
                      NULL,
                      &chunk,
                      &bytesRead
                     )
     )
  {
    return FALSE;
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
    return FALSE;
  }

  return TRUE; 
}

bool Chunks_skipSub(ChunkInfo   *chunkInfo,
                    ChunkHeader *chunkHeader
                   )
{

  return TRUE; 
}

bool Chunks_eofSub(ChunkInfo *chunkInfo)
{
  return chunkInfo->index >= chunkInfo->size;
}

#if 0
bool Chunks_read(ChunkInfo *chunkInfo, void *data)
{
  ulong bytesRead;

  assert(chunkInfo != NULL);
  assert(data != NULL);

  if (!readDefinition(chunkInfo->userData,chunkInfo->definition,0/* 0 */,chunkInfo->alignment,data,&bytesRead))
  {
    return FALSE;
  }
  chunkInfo->index += bytesRead;
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += bytesRead;
  }

  return TRUE;
}

bool Chunks_write(ChunkInfo  *chunkInfo,
                  const void *data
                 )
{
  uint64 offset;
  ulong bytesWritten;

  assert(chunkInfo != NULL);
  assert(data != NULL);

  /* get current offset */
  if (!IO.tellFile(chunkInfo->userData,&offset))
  {
    return FALSE;
  }
  assert(offset == chunkInfo->offset+CHUNK_HEADER_SIZE);

  if (!writeDefinition(chunkInfo->userData,data,chunkInfo->definition,chunkInfo->alignment,&bytesWritten))
  {
    return FALSE;
  }
  chunkInfo->index += bytesWritten;
  chunkInfo->size  += bytesWritten;
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += bytesWritten;
    chunkInfo->parentChunkInfo->size  += bytesWritten;
  }

  return TRUE;
}
#endif /* 0 */

bool Chunks_update(ChunkInfo  *chunkInfo,
                   const void *data
                  )
{
  uint64 offset;
  ulong  bytesWritten;

  /* get current offset */
  if (!IO.tellFile(chunkInfo->userData,&offset))
  {
    return FALSE;
  }

  /* update */
  if (!IO.seekFile(chunkInfo->userData,chunkInfo->offset+CHUNK_HEADER_SIZE))
  {
    return FALSE;
  }
  if (!writeDefinition(chunkInfo->userData,
                       chunkInfo->definition,
                       chunkInfo->definitionSize,
                       chunkInfo->alignment,
                       chunkInfo->cryptInfo,
                       data,
                       &bytesWritten
                      )
     )
  {
    return FALSE;
  }

  /* restore offset */
  if (!IO.seekFile(chunkInfo->userData,offset))
  {
    return FALSE;
  }

  return TRUE;
}

bool Chunks_readData(ChunkInfo *chunkInfo,
                     void      *data,
                     ulong     size
                    )
{
  assert(chunkInfo != NULL);

  if (size > (chunkInfo->size - chunkInfo->index))
  {
    size = chunkInfo->size - chunkInfo->index;
  }

  if (!IO.readFile(chunkInfo->userData,data,size))
  {
    return FALSE;
  }
  chunkInfo->index += size;
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += size;
  }

  return TRUE;
}

bool Chunks_writeData(ChunkInfo  *chunkInfo,
                      const void *data,
                      ulong      size
                     )
{
  assert(chunkInfo != NULL);

  if (!IO.writeFile(chunkInfo->userData,data,size))
  {
    return FALSE;
  }
  chunkInfo->size  += size;
  chunkInfo->index += size;
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += size;
    chunkInfo->parentChunkInfo->size  += size;
  }

  return TRUE;
}

bool Chunks_skipData(ChunkInfo *chunkInfo,
                     ulong     size
                    )
{
  uint64 offset;

  assert(chunkInfo != NULL);

  if (!IO.tellFile(chunkInfo->userData,&offset))
  {
    return FALSE;
  }
  offset += size;
  if (!IO.seekFile(chunkInfo->userData,offset))
  {
    return FALSE;
  }
  chunkInfo->index += size;

  /* set size in container chunk */
  if (chunkInfo->parentChunkInfo != NULL)
  {
    chunkInfo->parentChunkInfo->index += size;
    chunkInfo->parentChunkInfo->size  += size;
  }

  return TRUE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
