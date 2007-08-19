/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/chunks.h,v $
* $Revision: 1.5 $
* $Author: torsten $
* Contents: Backup ARchiver file chunk functions
* Systems : all
*
\***********************************************************************/

#ifndef __CHUNKS__
#define __CHUNKS__

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
  uint             alignment;     // alignment for chunk

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

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Chunks_initF(bool(*endOfFile)(void *userData),
                  bool(*readFile)(void *userData, void *buffer, ulong length),
                  bool(*writeFile)(void *userData, const void *buffer, ulong length),
                  bool(*tellFile)(void *userData, uint64 *offset),
                  bool(*seekFile)(void *userData, uint64 offset)
                );
void Chunks_doneF(void);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Chunks_init(ChunkInfo *chunkInfo,
                 ChunkInfo *parentChunkInfo,
                 void      *userData,
                 ChunkId   chunkId,
                 int       *definition,
                 uint      alignment
                );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Chunks_done(ChunkInfo *chunkInfo);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Chunks_next(void        *userData,
                 ChunkHeader *chunkHeader
                );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Chunks_skip(void        *userData,
                 ChunkHeader *chunkHeader
                );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Chunks_eof(void *userData);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Chunks_open(ChunkInfo   *chunkInfo,
                 ChunkHeader *chunkHeader,
                 void        *data
                );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Chunks_new(ChunkInfo  *chunkInfo,
                const void *data
               );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Chunks_close(ChunkInfo *chunkInfo);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Chunks_nextSub(ChunkInfo   *chunkInfo,
                    ChunkHeader *chunkHeader
                   );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Chunks_skipSub(ChunkInfo   *chunkInfo,
                    ChunkHeader *chunkHeader
                   );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Chunks_eofSub(ChunkInfo *chunkInfo);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

ulong Chunks_getSize(ChunkInfo  *chunkInfo,
                     const void *data
                    );

//bool Chunks_read(ChunkInfo *chunkInfo, void *data);
//bool Chunks_write(ChunkInfo *chunkInfo, const void *data);
bool Chunks_update(ChunkInfo *chunkInfo, const void *data);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Chunks_readData(ChunkInfo *chunkInfo, void *data, ulong size);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Chunks_writeData(ChunkInfo *chunkInfo, const void *data, ulong size);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Chunks_skipData(ChunkInfo *chunkInfo, ulong size);

#ifdef __cplusplus
  }
#endif

#endif /* __CHUNKS__ */

/* end of file */
