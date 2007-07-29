/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/chunks.h,v $
* $Revision: 1.2 $
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

#define CHUNK_HEADER_SIZE (4+8)

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

typedef uint32 ChunkId;

typedef struct
{
  ChunkId id;
  uint64  size;
} ChunkHeader;

typedef struct ChunkInfo
{
  struct ChunkInfo *containerChunkInfo;

  enum
  {
    CHUNK_MODE_WRITE,
    CHUNK_MODE_READ,
  } mode;
  bool(*nextFile)(void *userData);
  bool(*closeFile)(void *userData);
  bool(*readFile)(void *userData, void *buffer, ulong length);
  bool(*writeFile)(void *userData, const void *buffer, ulong length);
  bool(*tellFile)(void *userData, uint64 *offset);
  bool(*seekFile)(void *userData, uint64 offset);
  void *userData;

  uint64                offset;    // start of chunk (header)
  uint32                id;
  uint64                size;      // size of chunk
  uint64                index;     // current position in chunk
} ChunkInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

bool chunks_init(ChunkInfo *chunkInfo,
                 bool(*readFile)(void *userData, void *buffer, ulong length),
                 bool(*writeFile)(void *userData, const void *buffer, ulong length),
                 bool(*tellFile)(void *userData, uint64 *offset),
                 bool(*seekFile)(void *userData, uint64 offset),
                 void *userData
                );
void chunks_done(ChunkInfo *chunkInfo);

bool chunks_get(ChunkInfo *chunkInfo);
bool chunks_getSub(ChunkInfo *chunkInfo, ChunkInfo *containerChunkInfo);
bool chunks_new(ChunkInfo *chunkInfo, ChunkId chunkId);
bool chunks_newSub(ChunkInfo *chunkInfo, ChunkInfo *containerChunkInfo, ChunkId chunkId);
bool chunks_close(ChunkInfo *chunkInfo);

bool chunks_skip(ChunkInfo *chunkInfo);
bool chunks_eof(ChunkInfo *chunkInfo);

ulong chunks_getSize(const void *data, int definition[]);

bool chunks_read(ChunkInfo *chunkInfo, void *data, int definition[]);
bool chunks_write(ChunkInfo *chunkInfo, const void *data, int definition[]);

bool chunks_readData(ChunkInfo *chunkInfo, void *data, ulong size);
bool chunks_writeData(ChunkInfo *chunkInfo, const void *data, ulong size);
bool chunks_skipData(ChunkInfo *chunkInfo, ulong size);

#ifdef __cplusplus
  }
#endif

#endif /* __CHUNKS_H__ */

/* end of file */
