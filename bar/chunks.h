/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/chunks.h,v $
* $Revision: 1.3 $
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

#define CHUNK_ID_NONE 0

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
  uint32 id;            // chunk id
  uint64 size;          // size of chunk (without chunk header)
  uint64 offset;        // start of chunk in file (header) 
} ChunkHeader;

typedef enum
{
  CHUNK_MODE_UNKNOWN,
  CHUNK_MODE_WRITE,
  CHUNK_MODE_READ,
} ChunkModes;

typedef struct ChunkInfo
{
  struct ChunkInfo *parentChunkInfo;
  void             *userData;

  ChunkModes       mode;
  const int        *definition;   // chunk definition

  uint32           id;            // chunk id
  uint64           size;          // size of chunk (without chunk header)
  uint64           offset;        // start of chunk in file (header) 
  uint64           index;         // current position in chunk
} ChunkInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

bool chunks_initF(bool(*endOfFile)(void *userData),
                  bool(*readFile)(void *userData, void *buffer, ulong length),
                  bool(*writeFile)(void *userData, const void *buffer, ulong length),
                  bool(*tellFile)(void *userData, uint64 *offset),
                  bool(*seekFile)(void *userData, uint64 offset)
                );
void chunks_doneF(void);

bool chunks_init(ChunkInfo *chunkInfo,
                 ChunkInfo *parentChunkInfo,
                 void      *userData
                );
void chunks_done(ChunkInfo *chunkInfo);

bool chunks_next(void        *userData,
                 ChunkHeader *chunkHeader
                );

bool chunks_skip(void        *userData,
                 ChunkHeader *chunkHeader
                );

bool chunks_eof(void *userData);

bool chunks_open(ChunkInfo   *chunkInfo,
                 ChunkHeader *chunkHeader,
                 int         *definition,
                 void        *data
                );
bool chunks_new(ChunkInfo  *chunkInfo,
                ChunkId    chunkId,
                int        *definition,
                const void *data
               );
bool chunks_close(ChunkInfo *chunkInfo);

bool chunks_nextSub(ChunkInfo   *chunkInfo,
                    ChunkHeader *chunkHeader
                   );
bool chunks_skipSub(ChunkInfo   *chunkInfo,
                    ChunkHeader *chunkHeader
                   );
bool chunks_eofSub(ChunkInfo *chunkInfo);

ulong chunks_getSize(ChunkInfo *chunkInfo, const void *data);

//bool chunks_read(ChunkInfo *chunkInfo, void *data);
//bool chunks_write(ChunkInfo *chunkInfo, const void *data);
bool chunks_update(ChunkInfo *chunkInfo, const void *data);

bool chunks_readData(ChunkInfo *chunkInfo, void *data, ulong size);
bool chunks_writeData(ChunkInfo *chunkInfo, const void *data, ulong size);
bool chunks_skipData(ChunkInfo *chunkInfo, ulong size);

#ifdef __cplusplus
  }
#endif

#endif /* __CHUNKS_H__ */

/* end of file */
