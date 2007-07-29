/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/chunks.c,v $
* $Revision: 1.2 $
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

LOCAL ulong getDefinitionSize(const void *data, int definition[])
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

LOCAL bool readDefinition(ChunkInfo *chunkInfo, void *data, int definition[])
{
  int z;

  assert(chunkInfo != NULL);
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

          if (!chunkInfo->readFile(chunkInfo->userData,&n,1))
          {
            return FALSE;
          }

          (*((uint8*)data)) = n;
          chunkInfo->index += 1;
          data = ((char*)data) + 4;
        }
        break;
      case CHUNK_DATATYPE_UINT16:
      case CHUNK_DATATYPE_INT16:
        {
          uint16 n;

          if (!chunkInfo->readFile(chunkInfo->userData,&n,2))
          {
            return FALSE;
          }
          n = ntohs(n);

          (*((uint16*)data)) = n;
          chunkInfo->index += 2;
          data = ((char*)data) + 4;
        }
        break;
      case CHUNK_DATATYPE_UINT32:
      case CHUNK_DATATYPE_INT32:
        {
          uint32 n;

          if (!chunkInfo->readFile(chunkInfo->userData,&n,4))
          {
            return FALSE;
          }
          n = ntohl(n);

          (*((uint32*)data)) = n;
          chunkInfo->index += 4;
          data = ((char*)data) + 4;
        }
        break;
      case CHUNK_DATATYPE_UINT64:
      case CHUNK_DATATYPE_INT64:
        {
          uint64 n;
          uint32 l[2];

          if (!chunkInfo->readFile(chunkInfo->userData,l,8))
          {
            return FALSE;
          }
          n = (((uint64)ntohl(l[0])) << 32) | (((uint64)ntohl(l[1]) << 0));

          (*((uint64*)data)) = n;
          chunkInfo->index += 8;
          data = ((char*)data) + 8;
        }
        break;
      case CHUNK_DATATYPE_NAME:
        {
          uint16 n;
          ulong  length;
          void   *buffer;
          String s;

          if (!chunkInfo->readFile(chunkInfo->userData,&n,2))
          {
            return FALSE;
          }
          length = htons(n);
          buffer = malloc(length);
          if (buffer == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          if (!chunkInfo->readFile(chunkInfo->userData,buffer,length))
          {
            return FALSE;
          }
          s = String_new();
          String_setBuffer(s,buffer,length);
          free(buffer);

          (*((String*)data)) = s;
          chunkInfo->index += 2+length;
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

bool writeDefinition(ChunkInfo *chunkInfo, const void *data, int definition[], bool updateSize)
{
  int z;

  assert(chunkInfo != NULL);
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
          if (!chunkInfo->writeFile(chunkInfo->userData,&n,1))
          {
            return FALSE;
          }
          if (updateSize) chunkInfo->size  += 1;
          chunkInfo->index += 1;
          data = ((char*)data) + 4;
        }
        break;
      case CHUNK_DATATYPE_UINT16:
      case CHUNK_DATATYPE_INT16:
        {
          uint16 n;

          n = htons(*((uint16*)data));
          if (!chunkInfo->writeFile(chunkInfo->userData,&n,2))
          {
            return FALSE;
          }
          if (updateSize) chunkInfo->size  += 2;
          chunkInfo->index += 2;
          data = ((char*)data) + 4;
        }
        break;
      case CHUNK_DATATYPE_UINT32:
      case CHUNK_DATATYPE_INT32:
        {
          uint32 n;

          n = htonl(*((uint32*)data));
          if (!chunkInfo->writeFile(chunkInfo->userData,&n,4))
          {
            return FALSE;
          }
          if (updateSize) chunkInfo->size  += 4;
          chunkInfo->index += 4;
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
          if (!chunkInfo->writeFile(chunkInfo->userData,l,8))
          {
            return FALSE;
          }
          if (updateSize) chunkInfo->size  += 8;
          chunkInfo->index += 8;
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
          if (!chunkInfo->writeFile(chunkInfo->userData,&n,2))
          {
            return FALSE;
          }
          if (!chunkInfo->writeFile(chunkInfo->userData,String_cString(s),length))
          {
            return FALSE;
          }
          if (updateSize) chunkInfo->size  += 2+length;
          chunkInfo->index += 2+length;
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

/*---------------------------------------------------------------------*/


/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_init(ChunkInfo *chunkInfo,
                 bool(*readFile)(void *userData, void *buffer, ulong length),
                 bool(*writeFile)(void *userData, const void *buffer, ulong length),
                 bool(*tellFile)(void *userData, uint64 *offset),
                 bool(*seekFile)(void *userData, uint64 offset),
                 void *userData
                )
{
  assert(chunkInfo != NULL);

  chunkInfo->readFile  = readFile;
  chunkInfo->writeFile = writeFile;
  chunkInfo->tellFile  = tellFile;
  chunkInfo->seekFile  = seekFile;
  chunkInfo->userData  = userData;

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

void chunks_done(ChunkInfo *chunkInfo)
{
  assert(chunkInfo != NULL);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_get(ChunkInfo *chunkInfo)
{
  uint64      offset;
  ChunkHeader chunkHeader;

  assert(chunkInfo != NULL);

  /* get current offset */
  if (!chunkInfo->tellFile(chunkInfo->userData,&offset))
  {
    return FALSE;
  }

  /* read chunk header */
  if (!readDefinition(chunkInfo,&chunkHeader,CHUNK_HEADER_DEFINITION))
  {
    return FALSE;
  }

  /* init */
  chunkInfo->containerChunkInfo = NULL;
  chunkInfo->mode               = CHUNK_MODE_READ;
  chunkInfo->offset             = offset;
  chunkInfo->id                 = chunkHeader.id;
  chunkInfo->size               = chunkHeader.size;
  chunkInfo->index              = 0;

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

bool chunks_getSub(ChunkInfo *chunkInfo, ChunkInfo *containerChunkInfo)
{
  uint64      offset;
  ChunkHeader chunkHeader;

  assert(chunkInfo != NULL);
  assert(containerChunkInfo != NULL);
  assert(containerChunkInfo->mode == CHUNK_MODE_READ);

  if ((containerChunkInfo->index + CHUNK_HEADER_SIZE) <= containerChunkInfo->size)
  {
    /* get current offset */
    if (!chunkInfo->tellFile(chunkInfo->userData,&offset))
    {
      return FALSE;
    }

    /* read chunk header */
    if (!readDefinition(chunkInfo,&chunkHeader,CHUNK_HEADER_DEFINITION))
    {
      return FALSE;
    }
    containerChunkInfo->index += CHUNK_HEADER_SIZE;

    /* valida chunk */
    if (chunkHeader.size > (containerChunkInfo->size-containerChunkInfo->index))
    {
      return FALSE;
    }

    /* init */
    chunkInfo->containerChunkInfo = containerChunkInfo;
    chunkInfo->mode               = CHUNK_MODE_READ;
    chunkInfo->offset             = offset;
    chunkInfo->id                 = chunkHeader.id;
    chunkInfo->size               = chunkHeader.size;
    chunkInfo->index              = 0;

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

bool chunks_new(ChunkInfo *chunkInfo, ChunkId chunkId)
{
  uint64      offset;
  ChunkHeader chunkHeader;

  assert(chunkInfo != NULL);

  /* get current offset */
  if (!chunkInfo->tellFile(chunkInfo->userData,&offset))
  {
    return FALSE;
  }

  /* write chunk header */
  chunkHeader.id   = 0;
  chunkHeader.size = 0;
  if (!writeDefinition(chunkInfo,&chunkHeader,CHUNK_HEADER_DEFINITION,TRUE))
  {
    return FALSE;
  }

  /* init */
  chunkInfo->containerChunkInfo = NULL;
  chunkInfo->mode               = CHUNK_MODE_WRITE;
  chunkInfo->offset             = offset;
  chunkInfo->id                 = chunkId;
  chunkInfo->size               = 0;
  chunkInfo->index              = 0;

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

bool chunks_newSub(ChunkInfo *chunkInfo, ChunkInfo *containerChunkInfo, ChunkId chunkId)
{
  uint64      offset;
  ChunkHeader chunkHeader;

  assert(chunkInfo != NULL);
  assert(containerChunkInfo != NULL);
  assert(containerChunkInfo->mode == CHUNK_MODE_READ);

  /* get current offset */
  if (!chunkInfo->tellFile(chunkInfo->userData,&offset))
  {
    return FALSE;
  }

  /* write chunk header */
  chunkHeader.id   = 0;
  chunkHeader.size = 0;
  if (!writeDefinition(chunkInfo,&chunkHeader,CHUNK_HEADER_DEFINITION,TRUE))
  {
    return FALSE;
  }
  containerChunkInfo->size  += CHUNK_HEADER_SIZE;
  containerChunkInfo->index += CHUNK_HEADER_SIZE;

  /* init */
  chunkInfo->containerChunkInfo = containerChunkInfo;
  chunkInfo->mode               = CHUNK_MODE_WRITE;
  chunkInfo->offset             = offset;
  chunkInfo->id                 = chunkId;
  chunkInfo->size               = 0;
  chunkInfo->index              = 0;

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

bool chunks_close(ChunkInfo *chunkInfo)
{
  ChunkHeader chunkHeader;

  assert(chunkInfo != NULL);

  switch (chunkInfo->mode)
  {
    case CHUNK_MODE_WRITE:
      /* write size to chunk-header */
      if (!chunkInfo->seekFile(chunkInfo->userData,chunkInfo->offset))
      {
        return FALSE;
      }
      chunkHeader.id   = chunkInfo->id;
      chunkHeader.size = chunkInfo->size;
      if (!writeDefinition(chunkInfo,&chunkHeader,CHUNK_HEADER_DEFINITION,FALSE))
      {
        return FALSE;
      }
    case CHUNK_MODE_READ:
      break;
  }

  /* seek end to of chunk */
  if (!chunkInfo->seekFile(chunkInfo->userData,chunkInfo->offset+CHUNK_HEADER_SIZE+chunkInfo->size))
  {
    return FALSE;
  }
  chunkInfo->index = chunkInfo->offset+CHUNK_HEADER_SIZE+chunkInfo->size;

  /* set size in container chunk */
  if (chunkInfo->containerChunkInfo != NULL)
  {
    switch (chunkInfo->mode)
    {
      case CHUNK_MODE_WRITE:
        chunkInfo->containerChunkInfo->size  += CHUNK_HEADER_SIZE+chunkInfo->size;
        chunkInfo->containerChunkInfo->index += CHUNK_HEADER_SIZE+chunkInfo->size;
        break;
      case CHUNK_MODE_READ:
        chunkInfo->containerChunkInfo->index += CHUNK_HEADER_SIZE+chunkInfo->size;
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

bool chunks_skip(ChunkInfo *chunkInfo)
{
  assert(chunkInfo != NULL);

  return chunks_skipData(chunkInfo,chunkInfo->size-chunkInfo->index);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_eof(ChunkInfo *chunkInfo)
{
  assert(chunkInfo != NULL);

  return (chunkInfo->index >= chunkInfo->size);
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

  return getDefinitionSize(data,definition);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_read(ChunkInfo *chunkInfo, void *data, int definition[])
{
  assert(chunkInfo != NULL);
  assert(data != NULL);
  assert(definition != NULL);

  return readDefinition(chunkInfo,data,definition);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool chunks_write(ChunkInfo *chunkInfo, const void *data, int definition[])
{
  assert(chunkInfo != NULL);
  assert(data != NULL);
  assert(definition != NULL);

  return writeDefinition(chunkInfo,data,definition,TRUE);
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

bool chunks_readChunkString(ChunkInfo *chunkInfo, String string, ulong length)
{
  void *buffer;
  ssize_t n;

  assert(chunkInfo != NULL);

  buffer = malloc(length);
  if (buffer == NULL)
  {
    return FALSE;
  }

  n = read(chunkInfo->fileHandle,buffer,length);
  if (n < 0)
  {
    free(buffer);
    return FALSE;
  }
  chunkInfo->index += n;

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

bool chunks_writeChunkString(ChunkInfo *chunkInfo, String string)
{
  assert(chunkInfo != NULL);
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

bool chunks_readData(ChunkInfo *chunkInfo, void *data, ulong size)
{
  assert(chunkInfo != NULL);

  if (size > (chunkInfo->size - chunkInfo->index))
  {
    size = chunkInfo->size - chunkInfo->index;
  }

  if (!chunkInfo->readFile(chunkInfo->userData,data,size))
  {
    return FALSE;
  }
  chunkInfo->index += size;

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

bool chunks_writeData(ChunkInfo *chunkInfo, const void *data, ulong size)
{
  assert(chunkInfo != NULL);

  if (!chunkInfo->writeFile(chunkInfo->userData,data,size))
  {
    return FALSE;
  }
  chunkInfo->size  += size;
  chunkInfo->index += size;

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

bool chunks_skipData(ChunkInfo *chunkInfo, ulong size)
{
  uint64 offset;

  assert(chunkInfo != NULL);

  if (!chunkInfo->tellFile(chunkInfo->userData,&offset))
  {
    return FALSE;
  }
  offset += size;
  if (!chunkInfo->seekFile(chunkInfo->userData,offset))
  {
    return FALSE;
  }
  chunkInfo->index += size;

  /* set size in container chunk */
  if (chunkInfo->containerChunkInfo != NULL)
  {
    chunkInfo->containerChunkInfo->index += size;
  }

  return TRUE;
}


#ifdef __cplusplus
  }
#endif

/* end of file */
