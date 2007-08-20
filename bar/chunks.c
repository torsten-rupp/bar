/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/chunks.c,v $
* $Revision: 1.6 $
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
#define PADDING_INT8  1
#define PADDING_INT16 0
#define PADDING_INT32 0
#define PADDING_INT64 0

/* chunk header definition */
LOCAL int CHUNK_DEFINITION[] = {
                                 CHUNK_DATATYPE_UINT32,
                                 CHUNK_DATATYPE_UINT64,
                                 0
                               };

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
* Name   : getDefinitionSize
* Purpose: get size of chunk definition
* Input  : data       - chunk data
*          definition - chunk definition
*          alignment  - chunk alignment
* Output : -
* Return : size of definition (in bytes)
* Notes  : -
\***********************************************************************/

LOCAL ulong getDefinitionSize(const void *data,
                              const int  *definition,
                              uint       alignment
                             )
{
  int   z;
  ulong size;

  assert(data != NULL);

  size = 0;
  if (definition != NULL)
  {
    for (z = 0; definition[z] != 0; z++)
    {
      switch (definition[z])
      {
        case CHUNK_DATATYPE_UINT8:
        case CHUNK_DATATYPE_INT8:
          size += 1;
          data = ((char*)data) + 4;
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          size += 2;
          data = ((char*)data) + 4;
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          size += 4;
          data = ((char*)data) + 4;
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          size += 8;
          data = ((char*)data) + 8;
          break;
        case CHUNK_DATATYPE_NAME:
          {
            String s;

            s = (*((String*)data));
            assert(s != NULL);
            size += 2 + String_length(s);
  //pointer size???
            data = ((char*)data) + 4;
          }
          break;
        case CHUNK_DATATYPE_DATA:
          break;
        default:
          HALT_INTERAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      }
    }
  }

  /* add padding for alignment */
  if ((alignment > 0) && (size%alignment > 0))
  {
    size += alignment-size%alignment;
  }

  return size;
}

/***********************************************************************\
* Name   : readDefinition
* Purpose: read chunk definition
* Input  : userData   - user data
*          definition - chunk definition
*          alignment  - chunk alignment
* Output : data      - read data
*          bytesRead - number of bytes read
* Return : TRUE if read, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool readDefinition(void      *userData,
                          void      *data,
                          const int *definition,
                          uint      alignment,
                          ulong     *bytesRead
                         )
{
  int   z;
  ulong length;
  uint  padSize;
  char  padBuffer[16];

  assert(bytesRead != NULL);

  (*bytesRead) = 0;

  /* read data */
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

            assert(data != NULL);

            if (!IO.readFile(userData,&n,1))
            {
              return FALSE;
            }

            (*((uint8*)data)) = n;
            (*bytesRead) += 1;
            data = ((char*)data) + 4;
          }
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          {
            uint16 n;

            assert(data != NULL);

            if (!IO.readFile(userData,&n,2))
            {
              return FALSE;
            }
            n = ntohs(n);

            (*((uint16*)data)) = n;
            (*bytesRead) += 2;
            data = ((char*)data) + 4;
          }
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          {
            uint32 n;

            assert(data != NULL);

            if (!IO.readFile(userData,&n,4))
            {
              return FALSE;
            }
            n = ntohl(n);

            (*((uint32*)data)) = n;
            (*bytesRead) += 4;
            data = ((char*)data) + 4;
          }
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          {
            uint64 n;
            uint32 l[2];

            assert(data != NULL);

            if (!IO.readFile(userData,l,8))
            {
              return FALSE;
            }
            n = (((uint64)ntohl(l[0])) << 32) | (((uint64)ntohl(l[1]) << 0));

            (*((uint64*)data)) = n;
            (*bytesRead) += 8;
            data = ((char*)data) + 8;
          }
          break;
        case CHUNK_DATATYPE_NAME:
          {
            uint16 n;
            void   *buffer;
            String s;

            assert(data != NULL);

            if (!IO.readFile(userData,&n,2))
            {
              return FALSE;
            }
            length = htons(n);
            buffer = malloc(length);
            if (buffer == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }
            if (!IO.readFile(userData,buffer,length))
            {
              return FALSE;
            }
            s = String_new();
            String_setBuffer(s,buffer,length);
            free(buffer);

            (*((String*)data)) = s;
            (*bytesRead) += 2+length;
  //pointer size???
            data = ((char*)data) + 4;
          }
          break;
        case CHUNK_DATATYPE_DATA:
          break;
        default:
          HALT_INTERAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      }
    }
  }

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

  return TRUE;
}

/***********************************************************************\
* Name   : writeDefinition
* Purpose: write chunk definition
* Input  : userData   - user data
*          data       - chunk data
*          definition - chunk definition
*          alignment  - chunk alignment
* Output : bytesWritten - number of bytes written
* Return : TRUE if written, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool writeDefinition(void       *userData,
                           const void *data,
                           const int  *definition,
                           uint       alignment,
                           ulong      *bytesWritten
                          )
{
  int   z;
  ulong length;
  uint  padSize;
  char  padBuffer[16];

  assert(bytesWritten != NULL);

  (*bytesWritten) = 0;

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

            assert(data != NULL);

            n = (*((uint8*)data));
            if (!IO.writeFile(userData,&n,1))
            {
              return FALSE;
            }
            (*bytesWritten) += 1;
            data = ((char*)data) + 4;
          }
          break;
        case CHUNK_DATATYPE_UINT16:
        case CHUNK_DATATYPE_INT16:
          {
            uint16 n;

            assert(data != NULL);

            n = htons(*((uint16*)data));
            if (!IO.writeFile(userData,&n,2))
            {
              return FALSE;
            }
            (*bytesWritten) += 2;
            data = ((char*)data) + 4;
          }
          break;
        case CHUNK_DATATYPE_UINT32:
        case CHUNK_DATATYPE_INT32:
          {
            uint32 n;

            assert(data != NULL);

            n = htonl(*((uint32*)data));
            if (!IO.writeFile(userData,&n,4))
            {
              return FALSE;
            }
            (*bytesWritten) += 4;
            data = ((char*)data) + 4;
          }
          break;
        case CHUNK_DATATYPE_UINT64:
        case CHUNK_DATATYPE_INT64:
          {
            uint64 n;
            uint32 l[2];

            assert(data != NULL);

            n = (*((uint64*)data));
            l[0] = htonl((n & 0xFFFFffff00000000LL) >> 32);
            l[1] = htonl((n & 0x00000000FFFFffffLL) >>  0);
            if (!IO.writeFile(userData,l,8))
            {
              return FALSE;
            }
            (*bytesWritten) += 8;
            data = ((char*)data) + 8;
          }
          break;
        case CHUNK_DATATYPE_NAME:
          {
            String s;
            uint16 n;

            assert(data != NULL);

            s = (*((String*)data));
            assert(s != NULL);
            length = String_length(s);
            n = htons(length);
            if (!IO.writeFile(userData,&n,2))
            {
              return FALSE;
            }
            if (!IO.writeFile(userData,String_cString(s),length))
            {
              return FALSE;
            }
            (*bytesWritten) += 2+length;
  //pointer size???
            data = ((char*)data) + 4;
          }
          break;
        case CHUNK_DATATYPE_DATA:
          break;
        default:
          HALT_INTERAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      }
    }
  }

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

  return TRUE;
}

/*---------------------------------------------------------------------*/

bool Chunks_initF(bool(*endOfFile)(void *userData),
                  bool(*readFile)(void *userData, void *buffer, ulong length),
                  bool(*writeFile)(void *userData, const void *buffer, ulong length),
                  bool(*tellFile)(void *userData, uint64 *offset),
                  bool(*seekFile)(void *userData, uint64 offset)
                )
{
  IO.endOfFile = endOfFile;
  IO.readFile  = readFile;
  IO.writeFile = writeFile;
  IO.tellFile  = tellFile;
  IO.seekFile  = seekFile;

  return TRUE;
}

void Chunks_doneF(void)
{
}

bool Chunks_new(ChunkInfo *chunkInfo,
                ChunkInfo *parentChunkInfo,
                void      *userData,
                ChunkId   chunkId,
                int       *definition,
                uint      alignment
               )
{
  assert(chunkInfo != NULL);

  chunkInfo->parentChunkInfo = parentChunkInfo;
  chunkInfo->userData        = userData;

  chunkInfo->mode            = CHUNK_MODE_UNKNOWN;
  chunkInfo->definition      = definition;
  chunkInfo->alignment       = alignment;

  chunkInfo->id              = chunkId;
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
  if (!readDefinition(userData,&chunk,CHUNK_DEFINITION,0,&bytesRead))
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
                 void        *data
                )
{
  ulong bytesRead;

  assert(chunkInfo != NULL);
  assert(chunkInfo->id == chunkHeader->id);

  /* init */
  chunkInfo->size   = chunkHeader->size;
  chunkInfo->offset = chunkHeader->offset;
  chunkInfo->mode   = CHUNK_MODE_READ;
  chunkInfo->index  = 0;

  if (!readDefinition(chunkInfo->userData,data,chunkInfo->definition,chunkInfo->alignment,&bytesRead))
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
                   const void *data
                  )
{
  uint64      offset;
  ChunkHeader chunkHeader;
  ulong       bytesWritten;

  assert(chunkInfo != NULL);
  assert(chunkInfo->id != CHUNK_ID_NONE);

  /* init */
  chunkInfo->size   = 0;
  chunkInfo->offset = 0;
  chunkInfo->mode   = CHUNK_MODE_WRITE;
  chunkInfo->index  = 0;

  /* get current offset */
  if (!IO.tellFile(chunkInfo->userData,&offset))
  {
    return FALSE;
  }
  chunkInfo->offset = offset;

  /* write chunk header id */
  chunkHeader.id   = 0;
  chunkHeader.size = 0;
  if (!writeDefinition(chunkInfo->userData,&chunkHeader,CHUNK_DEFINITION,0,&bytesWritten))
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
      if (!writeDefinition(chunkInfo->userData,&chunkHeader,CHUNK_DEFINITION,0,&bytesWritten))
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
  if (!readDefinition(chunkInfo->userData,&chunk,CHUNK_DEFINITION,0,&bytesRead))
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

ulong Chunks_getSize(ChunkInfo  *chunkInfo,
                     const void *data
                    )
{
  assert(chunkInfo != NULL);
  assert(data != NULL);

  return CHUNK_HEADER_SIZE+getDefinitionSize(data,chunkInfo->definition,chunkInfo->alignment);
}

bool Chunks_read(ChunkInfo *chunkInfo, void *data)
{
  ulong bytesRead;

  assert(chunkInfo != NULL);
  assert(data != NULL);

  if (!readDefinition(chunkInfo->userData,data,chunkInfo->definition,chunkInfo->alignment,&bytesRead))
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
  if (!writeDefinition(chunkInfo->userData,data,chunkInfo->definition,chunkInfo->alignment,&bytesWritten))
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
