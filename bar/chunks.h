/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/chunks.h,v $
* $Revision: 1.6 $
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

/* size of chunk header */
#define CHUNK_HEADER_SIZE (4+8)

/* chunk ids */
#define CHUNK_ID_NONE 0

/* chunk data types */
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

typedef enum
{
  CHUNK_MODE_UNKNOWN,
  CHUNK_MODE_WRITE,
  CHUNK_MODE_READ,
} ChunkModes;

/***************************** Datatypes *******************************/

typedef uint32 ChunkId;

typedef struct
{
  uint32 id;                      // chunk id                              
  uint64 size;                    // size of chunk (without chunk header)  
  uint64 offset;                  // start of chunk in file (header)       
} ChunkHeader;

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
* Name   : Chunks_new
* Purpose: create new chunk handle
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Chunks_new(ChunkInfo *chunkInfo,
                ChunkInfo *parentChunkInfo,
                void      *userData,
                ChunkId   chunkId,
                int       *definition,
                uint      alignment
               );

/***********************************************************************\
* Name   : Chunks_delete
* Purpose: delete chunk handle
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Chunks_delete(ChunkInfo *chunkInfo);

/***********************************************************************\
* Name   : Chunks_next
* Purpose: get next chunk
* Input  : userData - user data for file i/o
* Output : chunkHeader - chunk header
* Return : TRUE if chunk header read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunks_next(void        *userData,
                 ChunkHeader *chunkHeader
                );

/***********************************************************************\
* Name   : Chunks_skip
* Purpose: skip chink
* Input  : userData    - user data for file i/o
*          chunkHeader - chunk header
* Output : -
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunks_skip(void        *userData,
                 ChunkHeader *chunkHeader
                );

/***********************************************************************\
* Name   : Chunks_eof
* Purpose: check of end of chunks (file)
* Input  : userData - user data for file i/o
* Output : -
* Return : TRUE if end of chunks, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunks_eof(void *userData);

/***********************************************************************\
* Name   : Chunks_open
* Purpose: open chunk
* Input  : chunkInfo   - chunk info block
*          chunkHeader - chunk header
* Output : data - chunk data
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunks_open(ChunkInfo   *chunkInfo,
                 ChunkHeader *chunkHeader,
                 void        *data
                );

/***********************************************************************\
* Name   : Chunks_create
* Purpose: create new chunk
* Input  : chunkInfo - chunk info block
*          data      - chunk data
* Output : -
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunks_create(ChunkInfo  *chunkInfo,
                   const void *data
                  );

/***********************************************************************\
* Name   : Chunks_close
* Purpose: update chunk header and close chunk
* Input  : chunkInfo - chunk info block
* Output : -
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunks_close(ChunkInfo *chunkInfo);

/***********************************************************************\
* Name   : Chunks_nextSub
* Purpose: get next sub-chunk
* Input  : chunkInfo   - chunk info block
*          chunkHeader - chunk header
* Output : -
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunks_nextSub(ChunkInfo   *chunkInfo,
                    ChunkHeader *chunkHeader
                   );

/***********************************************************************\
* Name   : Chunks_skipSub
* Purpose: skip sub-chunk
* Input  : chunkInfo   - chunk info block
*          chunkHeader - chunk header
* Output : -
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunks_skipSub(ChunkInfo   *chunkInfo,
                    ChunkHeader *chunkHeader
                   );

/***********************************************************************\
* Name   : Chunks_eofSub
* Purpose: check if end of sub-chunks
* Input  : chunkInfo - chunk info block
* Output : -
* Return : TRUE if end of sub-chunks, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunks_eofSub(ChunkInfo *chunkInfo);

/***********************************************************************\
* Name   : Chunks_getSize
* Purpose: get size of chunk in bytes (without data elements)
* Input  : chunkInfo - chunk info block
*          data      - chunk data
* Output : -
* Return : size of chunk
* Notes  : -
\***********************************************************************/

ulong Chunks_getSize(ChunkInfo  *chunkInfo,
                     const void *data
                    );

/***********************************************************************\
* Name   : Chunks_update
* Purpose: update chunk data
* Input  : chunkInfo - chunk info block
*          data      - chunk data
* Output : -
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunks_update(ChunkInfo  *chunkInfo,
                   const void *data
                  );

/***********************************************************************\
* Name   : Chunks_readData
* Purpose: read data from chunk
* Input  : chunkInfo   - chunk info block
*          data        - buffer for data
*          size        - number of bytes to read
* Output : -
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunks_readData(ChunkInfo *chunkInfo,
                     void      *data,
                     ulong     size
                    );

/***********************************************************************\
* Name   : Chunks_writeData
* Purpose: write data into chunk
* Input  : chunkInfo   - chunk info block
*          data        - buffer with data
*          size        - number of bytes to write
* Output : -
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunks_writeData(ChunkInfo  *chunkInfo,
                      const void *data,
                      ulong      size
                     );

/***********************************************************************\
* Name   : Chunks_skipData
* Purpose: skip chunk data
* Input  : chunkInfo - chunk info block
*          size      - number of bytes to skip
* Output : -
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunks_skipData(ChunkInfo *chunkInfo,
                     ulong     size
                    );

#ifdef __cplusplus
  }
#endif

#endif /* __CHUNKS__ */

/* end of file */
