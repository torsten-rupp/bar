/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/chunks.c,v $
* $Revision: 1.1.1.1 $
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

#define PADDING_INT8  1
#define PADDING_INT16 0
#define PADDING_INT32 0
#define PADDING_INT64 0

LOCAL int CHUNK_HEADER_DEFINITION[] = {CHUNK_DATATYPE_UINT32,CHUNK_DATATYPE_UINT64,0};

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_init(ChunkInfoBlock *chunkInfoBlock,
                 bool(*readFile)(void *userData, void *buffer, ulong length),
                 bool(*writeFile)(void *userData, const void *buffer, ulong length),
                 bool(*tellFile)(void *userData, uint64 *offset),
                 bool(*seekFile)(void *userData, uint64 offset),
                 void *userData
                )
{
  assert(chunkInfoBlock != NULL);

  chunkInfoBlock->readFile  = readFile;
  chunkInfoBlock->writeFile = writeFile;
  chunkInfoBlock->tellFile  = tellFile;
  chunkInfoBlock->seekFile  = seekFile;
  chunkInfoBlock->userData  = userData;

  return TRUE;
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void chunks_done(ChunkInfoBlock *chunkInfoBlock)
{
  assert(chunkInfoBlock != NULL);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_get(ChunkInfoBlock *chunkInfoBlock)
{
  uint64      offset;
  ChunkHeader chunkHeader;

  assert(chunkInfoBlock != NULL);

  /* get current offset */
  if (!chunkInfoBlock->tellFile(chunkInfoBlock->userData,&offset))
  {
    return FALSE;
  }

  /* read chunk header */
  if (!chunks_read(chunkInfoBlock,&chunkHeader,CHUNK_HEADER_DEFINITION))
  {
    return FALSE;
  }

  /* init */
  chunkInfoBlock->containerChunkInfoBlock = NULL;
  chunkInfoBlock->mode                    = CHUNK_MODE_READ;
  chunkInfoBlock->offset                  = offset;
  chunkInfoBlock->id                      = chunkHeader.id;
  chunkInfoBlock->size                    = chunkHeader.size;
  chunkInfoBlock->index                   = 0;

  return TRUE;
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_getSub(ChunkInfoBlock *chunkInfoBlock, ChunkInfoBlock *containerChunkInfoBlock)
{
  uint64      offset;
  ChunkHeader chunkHeader;

  assert(chunkInfoBlock != NULL);
  assert(containerChunkInfoBlock != NULL);
  assert(containerChunkInfoBlock->mode == CHUNK_MODE_READ);

  if ((containerChunkInfoBlock->index + sizeof(ChunkHeader)) <= containerChunkInfoBlock->size)
  {
    /* get current offset */
    if (!chunkInfoBlock->tellFile(chunkInfoBlock->userData,&offset))
    {
      return FALSE;
    }

    /* read chunk header */
    if (!chunks_read(chunkInfoBlock,&chunkHeader,CHUNK_HEADER_DEFINITION))
    {
      return FALSE;
    }
    containerChunkInfoBlock->index += sizeof(ChunkHeader);

    /* valida chunk */
    if (chunkHeader.size > (containerChunkInfoBlock->size-containerChunkInfoBlock->index))
    {
      return FALSE;
    }

    /* init */
    chunkInfoBlock->containerChunkInfoBlock = containerChunkInfoBlock;
    chunkInfoBlock->mode                    = CHUNK_MODE_READ;
    chunkInfoBlock->offset                  = offset;
    chunkInfoBlock->id                      = chunkHeader.id;
    chunkInfoBlock->size                    = chunkHeader.size;
    chunkInfoBlock->index                   = 0;

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_new(ChunkInfoBlock *chunkInfoBlock, uint32 id)
{
  uint64      offset;
  ChunkHeader chunkHeader;

  assert(chunkInfoBlock != NULL);

  /* get current offset */
  if (!chunkInfoBlock->tellFile(chunkInfoBlock->userData,&offset))
  {
    return FALSE;
  }

  /* write chunk header */
  chunkHeader.id   = 0;
  chunkHeader.size = 0;
  if (!chunks_write(chunkInfoBlock,&chunkHeader,CHUNK_HEADER_DEFINITION))
  {
    return FALSE;
  }

  /* init */
  chunkInfoBlock->containerChunkInfoBlock = NULL;
  chunkInfoBlock->mode                    = CHUNK_MODE_WRITE;
  chunkInfoBlock->offset                  = offset;
  chunkInfoBlock->id                      = id;
  chunkInfoBlock->size                    = 0;
  chunkInfoBlock->index                   = 0;

  return TRUE;
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_newSub(ChunkInfoBlock *chunkInfoBlock, ChunkInfoBlock *containerChunkInfoBlock, uint32 id)
{
  uint64      offset;
  ChunkHeader chunkHeader;

  assert(chunkInfoBlock != NULL);
  assert(containerChunkInfoBlock != NULL);
  assert(containerChunkInfoBlock->mode == CHUNK_MODE_READ);

  /* get current offset */
  if (!chunkInfoBlock->tellFile(chunkInfoBlock->userData,&offset))
  {
    return FALSE;
  }

  /* write chunk header */
  chunkHeader.id   = 0;
  chunkHeader.size = 0;
  if (!chunks_write(chunkInfoBlock,&chunkHeader,CHUNK_HEADER_DEFINITION))
  {
    return FALSE;
  }
  containerChunkInfoBlock->size  += sizeof(ChunkHeader);
  containerChunkInfoBlock->index += sizeof(ChunkHeader);

  /* init */
  chunkInfoBlock->containerChunkInfoBlock = containerChunkInfoBlock;
  chunkInfoBlock->mode                    = CHUNK_MODE_WRITE;
  chunkInfoBlock->offset                  = offset;
  chunkInfoBlock->id                      = id;
  chunkInfoBlock->size                    = 0;
  chunkInfoBlock->index                   = 0;

  return TRUE;
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_close(ChunkInfoBlock *chunkInfoBlock)
{
  uint64      oldOffset;
  ChunkHeader chunkHeader;

  assert(chunkInfoBlock != NULL);

  /* write size to chunk-header */
  if (!chunkInfoBlock->tellFile(chunkInfoBlock->userData,&oldOffset))
  {
    return FALSE;
  }
  if (!chunkInfoBlock->seekFile(chunkInfoBlock->userData,chunkInfoBlock->offset))
  {
    chunkInfoBlock->seekFile(chunkInfoBlock->userData,oldOffset);
    return FALSE;
  }
  chunkHeader.id   = chunkInfoBlock->id;
  chunkHeader.size = chunkInfoBlock->size;
  if (!chunks_write(chunkInfoBlock,&chunkHeader,CHUNK_HEADER_DEFINITION))
  {
    chunkInfoBlock->seekFile(chunkInfoBlock->userData,oldOffset);
    return FALSE;
  }
  if (!chunkInfoBlock->seekFile(chunkInfoBlock->userData,oldOffset))
  {
    return FALSE;
  }

  /* set size in container chunk */
  if (chunkInfoBlock->containerChunkInfoBlock != NULL)
  {
    switch (chunkInfoBlock->mode)
    {
      case CHUNK_MODE_WRITE:
        chunkInfoBlock->containerChunkInfoBlock->size  += chunkInfoBlock->size;
        chunkInfoBlock->containerChunkInfoBlock->index += chunkInfoBlock->size;
        break;
      case CHUNK_MODE_READ:
        chunkInfoBlock->containerChunkInfoBlock->index += chunkInfoBlock->size;
        break;
    }
  }

  return TRUE; 
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_skip(ChunkInfoBlock *chunkInfoBlock)
{
  assert(chunkInfoBlock != NULL);

  return chunks_skipData(chunkInfoBlock,chunkInfoBlock->size-chunkInfoBlock->index);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_eof(ChunkInfoBlock *chunkInfoBlock)
{
  assert(chunkInfoBlock != NULL);

  return (chunkInfoBlock->index > chunkInfoBlock->size);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

ulong chunks_getSize(const void *data, int definition[])
{
  int   z;
  ulong size;

  assert(data != NULL);
  assert(definition != NULL);

  size = 0;
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

  return size;
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_read(ChunkInfoBlock *chunkInfoBlock, void *data, int definition[])
{
  int z;

  assert(chunkInfoBlock != NULL);
  assert(data != NULL);
  assert(definition != NULL);

  for (z = 0; definition[z] != 0; z++)
  {
    switch (definition[z])
    {
      case CHUNK_DATATYPE_UINT8:
      case CHUNK_DATATYPE_INT8:
        {
          uint8 n;

          if (!chunkInfoBlock->readFile(chunkInfoBlock->userData,&n,1))
          {
            return FALSE;
          }

          (*((uint8*)data)) = n;
          chunkInfoBlock->index += 1;
          data = ((char*)data) + 4;
        }
        break;
      case CHUNK_DATATYPE_UINT16:
      case CHUNK_DATATYPE_INT16:
        {
          uint16 n;

          if (!chunkInfoBlock->readFile(chunkInfoBlock->userData,&n,2))
          {
            return FALSE;
          }
          n = ntohs(n);

          (*((uint16*)data)) = n;
          chunkInfoBlock->index += 2;
          data = ((char*)data) + 4;
        }
        break;
      case CHUNK_DATATYPE_UINT32:
      case CHUNK_DATATYPE_INT32:
        {
          uint32 n;

          if (!chunkInfoBlock->readFile(chunkInfoBlock->userData,&n,4))
          {
            return FALSE;
          }
          n = ntohl(n);

          (*((uint32*)data)) = n;
          chunkInfoBlock->index += 4;
          data = ((char*)data) + 4;
        }
        break;
      case CHUNK_DATATYPE_UINT64:
      case CHUNK_DATATYPE_INT64:
        {
          uint64 n;
          uint32 l[2];

          if (!chunkInfoBlock->readFile(chunkInfoBlock->userData,l,8))
          {
            return FALSE;
          }
          n = (((uint64)ntohl(l[0])) << 32) | (((uint64)ntohl(l[1]) << 0));

          (*((uint64*)data)) = n;
          chunkInfoBlock->index += 8;
          data = ((char*)data) + 8;
        }
        break;
      case CHUNK_DATATYPE_NAME:
        {
          uint16 n;
          ulong  length;
          void   *buffer;
          String s;

          if (!chunkInfoBlock->readFile(chunkInfoBlock->userData,&n,2))
          {
            return FALSE;
          }
          length = htons(n);
          buffer = malloc(length);
          if (buffer == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          if (!chunkInfoBlock->readFile(chunkInfoBlock->userData,buffer,length))
          {
            return FALSE;
          }
          s = String_new();
          String_setBuffer(s,buffer,length);
          free(buffer);

          (*((String*)data)) = s;
          chunkInfoBlock->index += 2+length;
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

  return TRUE;
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_write(ChunkInfoBlock *chunkInfoBlock, const void *data, int definition[])
{
  int z;

  assert(chunkInfoBlock != NULL);
  assert(data != NULL);
  assert(definition != NULL);

  for (z = 0; definition[z] != 0; z++)
  {
    switch (definition[z])
    {
      case CHUNK_DATATYPE_UINT8:
      case CHUNK_DATATYPE_INT8:
        {
          uint8 n;

          n = (*((uint8*)data));
          if (!chunkInfoBlock->writeFile(chunkInfoBlock->userData,&n,1))
          {
            return FALSE;
          }
          chunkInfoBlock->size  += 1;
          chunkInfoBlock->index += 1;
          data = ((char*)data) + 4;
        }
        break;
      case CHUNK_DATATYPE_UINT16:
      case CHUNK_DATATYPE_INT16:
        {
          uint16 n;

          n = htons(*((uint16*)data));
          if (!chunkInfoBlock->writeFile(chunkInfoBlock->userData,&n,2))
          {
            return FALSE;
          }
          chunkInfoBlock->size  += 2;
          chunkInfoBlock->index += 2;
          data = ((char*)data) + 4;
        }
        break;
      case CHUNK_DATATYPE_UINT32:
      case CHUNK_DATATYPE_INT32:
        {
          uint32 n;

          n = htonl(*((uint32*)data));
          if (!chunkInfoBlock->writeFile(chunkInfoBlock->userData,&n,4))
          {
            return FALSE;
          }
          chunkInfoBlock->size  += 4;
          chunkInfoBlock->index += 4;
          data = ((char*)data) + 4;
        }
        break;
      case CHUNK_DATATYPE_UINT64:
      case CHUNK_DATATYPE_INT64:
        {
          uint64 n;
          uint32 l[2];

          n = (*((uint64*)data));
          l[0] = htonl((n & 0xFFFFffff00000000LL) >> 32);
          l[1] = htonl((n & 0x00000000FFFFffffLL) >>  0);
          if (!chunkInfoBlock->writeFile(chunkInfoBlock->userData,l,8))
          {
            return FALSE;
          }
          chunkInfoBlock->size  += 8;
          chunkInfoBlock->index += 8;
          data = ((char*)data) + 8;
        }
        break;
      case CHUNK_DATATYPE_NAME:
        {
          String s;
          ulong  length;
          uint16 n;

          s = (*((String*)data));
          assert(s != NULL);
          length = String_length(s);
          n = htons(length);
          if (!chunkInfoBlock->writeFile(chunkInfoBlock->userData,&n,2))
          {
            return FALSE;
          }
          if (!chunkInfoBlock->writeFile(chunkInfoBlock->userData,String_cString(s),length))
          {
            return FALSE;
          }
          chunkInfoBlock->size  += 2+length;
          chunkInfoBlock->index += 2+length;
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

  return TRUE;
}

#if 0
/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_readChunkString(ChunkInfoBlock *chunkInfoBlock, String string, ulong length)
{
  void *buffer;
  ssize_t n;

  assert(chunkInfoBlock != NULL);

  buffer = malloc(length);
  if (buffer == NULL)
  {
    return FALSE;
  }

  n = read(chunkInfoBlock->fileHandle,buffer,length);
  if (n < 0)
  {
    free(buffer);
    return FALSE;
  }
  chunkInfoBlock->index += n;

  if (n != length)
  {
    free(buffer);
    return FALSE;
  }

  String_setBuffer(string,buffer,length);

  free(buffer);

  return TRUE;
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_writeChunkString(ChunkInfoBlock *chunkInfoBlock, String string)
{
  assert(chunkInfoBlock != NULL);
}
#endif /* 0 */

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_readData(ChunkInfoBlock *chunkInfoBlock, void *data, ulong size)
{
  assert(chunkInfoBlock != NULL);

  if (size > (chunkInfoBlock->size - chunkInfoBlock->index))
  {
    size = chunkInfoBlock->size - chunkInfoBlock->index;
  }

  if (!chunkInfoBlock->readFile(chunkInfoBlock->userData,data,size))
  {
    return FALSE;
  }
  chunkInfoBlock->index += size;

  return TRUE;
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_writeData(ChunkInfoBlock *chunkInfoBlock, const void *data, ulong size)
{
  assert(chunkInfoBlock != NULL);

  if (!chunkInfoBlock->writeFile(chunkInfoBlock->userData,data,size))
  {
    return FALSE;
  }
  chunkInfoBlock->size  += size;
  chunkInfoBlock->index += size;

  return TRUE;
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_skipData(ChunkInfoBlock *chunkInfoBlock, ulong size)
{
  uint64 offset;

  assert(chunkInfoBlock != NULL);

  if (!chunkInfoBlock->tellFile(chunkInfoBlock->userData,&offset))
  {
    return FALSE;
  }
  offset += size;
  if (!chunkInfoBlock->seekFile(chunkInfoBlock->userData,offset))
  {
    return FALSE;
  }
  chunkInfoBlock->index += size;

  /* set size in container chunk */
  if (chunkInfoBlock->containerChunkInfoBlock != NULL)
  {
    chunkInfoBlock->containerChunkInfoBlock->index += size;
  }

  return TRUE;
}


#ifdef __cplusplus
  }
#endif

/* end of file */
