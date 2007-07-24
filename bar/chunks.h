/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/chunks.h,v $
* $Revision: 1.1.1.1 $
* $Author: torsten $
* Contents: Backup ARchiver file chunk functions
* Systems : all
*
\***********************************************************************/

#ifndef __CHUNKS_H__
#define __CHUNKS_H__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define CHUNK_DATATYPE_UINT8    1
#define CHUNK_DATATYPE_UINT16   2
#define CHUNK_DATATYPE_UINT32   3
#define CHUNK_DATATYPE_UINT64   4
#define CHUNK_DATATYPE_INT8     5
#define CHUNK_DATATYPE_INT16    6
#define CHUNK_DATATYPE_INT32    7
#define CHUNK_DATATYPE_INT64    8
#define CHUNK_DATATYPE_NAME     9
#define CHUNK_DATATYPE_DATA    10

/***************************** Datatypes *******************************/

typedef struct
{
  uint32 id;
  uint64 size;
} ChunkHeader;

typedef struct ChunkInfoBlock
{
  struct ChunkInfoBlock *containerChunkInfoBlock;

  enum
  {
    CHUNK_MODE_WRITE,
    CHUNK_MODE_READ,
  } mode;
  bool(*readFile)(void *userData, void *buffer, ulong length);
  bool(*writeFile)(void *userData, const void *buffer, ulong length);
  bool(*tellFile)(void *userData, uint64 *offset);
  bool(*seekFile)(void *userData, uint64 offset);
  void *userData;

  uint64                offset;    // start of chunk (header)
  uint32                id;
  uint64                size;      // size of chunk
  uint64                index;     // current position in chunk
} ChunkInfoBlock;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

bool chunks_init(ChunkInfoBlock *chunkInfoBlock,
                 bool(*readFile)(void *userData, void *buffer, ulong length),
                 bool(*writeFile)(void *userData, const void *buffer, ulong length),
                 bool(*tellFile)(void *userData, uint64 *offset),
                 bool(*seekFile)(void *userData, uint64 offset),
                 void *userData
                );
void chunks_done(ChunkInfoBlock *chunkInfoBlock);

bool chunks_get(ChunkInfoBlock *chunkInfoBlock);
bool chunks_getSub(ChunkInfoBlock *chunkInfoBlock, ChunkInfoBlock *containerChunkInfoBlock);
bool chunks_new(ChunkInfoBlock *chunkInfoBlock, uint32 id);
bool chunks_newSub(ChunkInfoBlock *chunkInfoBlock, ChunkInfoBlock *containerChunkInfoBlock, uint32 id);
bool chunks_close(ChunkInfoBlock *chunkInfoBlock);

bool chunks_skip(ChunkInfoBlock *chunkInfoBlock);
bool chunks_eof(ChunkInfoBlock *chunkInfoBlock);

ulong chunks_getSize(const void *data, int definition[]);

bool chunks_read(ChunkInfoBlock *chunkInfoBlock, void *data, int definition[]);
bool chunks_write(ChunkInfoBlock *chunkInfoBlock, const void *data, int definition[]);

bool chunks_readData(ChunkInfoBlock *chunkInfoBlock, void *data, ulong size);
bool chunks_writeData(ChunkInfoBlock *chunkInfoBlock, const void *data, ulong size);
bool chunks_skipData(ChunkInfoBlock *chunkInfoBlock, ulong size);

#ifdef __cplusplus
  }
#endif

#endif /* __CHUNKS_H__ */

/* end of file */
