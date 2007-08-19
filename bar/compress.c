/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/compress.c,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: backup archiver compress functions
* Systems :
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "lists.h"

#include "bar.h"

#include "compress.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

Errors Compress_init(void)
{
  return ERROR_NONE;
}

void Compress_done(void)
{
}

Errors Compress_new(CompressInfo *compressInfo,
                    uint         blockLength
                   )
{
  assert(compressInfo != NULL);

  compressInfo->blockLength = blockLength;

  compressInfo->buffer = malloc(blockLength);
  if (compressInfo->buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  compressInfo->bufferIndex  = 0;
  compressInfo->bufferLength = 0;

  return ERROR_NONE;
}

void Compress_delete(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);
  assert(compressInfo->buffer != NULL);

  free(compressInfo->buffer);
}

Errors Compress_deflate(CompressInfo *compressInfo,
                        byte         data
                       )
{
  assert(compressInfo != NULL);
  assert(compressInfo->buffer != NULL);
  assert(compressInfo->bufferIndex < compressInfo->blockLength);

  *((byte*)compressInfo->buffer+compressInfo->bufferIndex) = data;
  compressInfo->bufferIndex++;
  compressInfo->bufferLength++;

  return ERROR_NONE;
}

Errors Compress_inflate(CompressInfo *compressInfo,
                        byte         *data
                       )
{
  assert(compressInfo != NULL);
  assert(compressInfo->buffer != NULL);
  assert(compressInfo->bufferIndex < compressInfo->bufferLength);
  assert(data != NULL);

  (*data) = *((byte*)compressInfo->buffer+compressInfo->bufferIndex);
  compressInfo->bufferIndex++;

  return ERROR_NONE;
}

bool Compress_checkBlockIsFull(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return (compressInfo->bufferIndex >= compressInfo->blockLength);
}

bool Compress_checkBlockIsEmpty(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return (compressInfo->bufferIndex == 0);
}

bool Compress_checkEndOfBlock(CompressInfo *compressInfo)
{
  assert(compressInfo != NULL);

  return (compressInfo->bufferIndex >= compressInfo->bufferLength);
}

void Compress_getBlock(CompressInfo *compressInfo,
                       void         *buffer,
                       ulong        *bufferLength
                      )
{
  assert(compressInfo != NULL);
  assert(compressInfo->buffer != NULL);
  assert(compressInfo->bufferLength != compressInfo->blockLength);
  assert(buffer != NULL);
  assert(bufferLength != NULL);

  memcpy(buffer,compressInfo->buffer,compressInfo->bufferIndex);
  memset((byte*)buffer+compressInfo->bufferIndex,
         0,
         compressInfo->blockLength-compressInfo->bufferIndex
        );
  (*bufferLength) = compressInfo->bufferLength;

  compressInfo->bufferIndex  = 0;
  compressInfo->bufferLength = 0;
}

void Compress_putBlock(CompressInfo *compressInfo,
                       void         *buffer,
                       ulong        bufferLength
                      )
{
  assert(compressInfo != NULL);
  assert(compressInfo->buffer != NULL);
  assert(buffer != NULL);
  assert(bufferLength <= compressInfo->blockLength);

  memcpy(compressInfo->buffer,buffer,bufferLength);
  compressInfo->bufferIndex  = 0;
  compressInfo->bufferLength = bufferLength;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
