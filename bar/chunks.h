/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/chunks.h,v $
* $Revision: 1.11 $
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

#include "crypt.h"

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
#define CHUNK_DATATYPE_CRC32   11

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
  uint32 id;                        // chunk id                              
  uint64 size;                      // size of chunk (without chunk header)  
  uint64 offset;                    // start of chunk in file (header)       
} ChunkHeader;

typedef struct ChunkInfo
{
  struct ChunkInfo *parentChunkInfo;
  void             *userData;

  ChunkModes       mode;
  uint             alignment;       // alignment for chunk
  CryptInfo        *cryptInfo;      // encryption

  ChunkId          id;              // chunk id
  const int        *definition;     // chunk definition
  ulong            definitionSize;  // chunk definition size (without data elements)
  uint64           size;            // total size of chunk (without chunk header)
  uint64           offset;          // start of chunk in file (header) 
  uint64           index;           // current position in chunk
} ChunkInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Chunk_init
* Purpose: init chunks functions
* Input  : eof   - call back check end of data
*          read  - call back read data
*          write - call back write data
*          tell  - call back tell position
*          seek  - call back seek to position
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_init(bool(*eof)(void *userData),
                  Errors(*read)(void *userData, void *buffer, ulong length, ulong *bytesRead),
                  Errors(*write)(void *userData, const void *buffer, ulong length),
                  Errors(*tell)(void *userData, uint64 *offset),
                  Errors(*seek)(void *userData, uint64 offset)
                 );

/***********************************************************************\
* Name   : Chunk_done
* Purpose: done chunks functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Chunk_done(void);

/***********************************************************************\
* Name   : Chunk_idToString
* Purpose: convert chunk id to string
* Input  : chunkId - chunk id
* Output : -
* Return : string
* Notes  : non-reentrant function!
\***********************************************************************/

const char *Chunk_idToString(ChunkId chunkId);

/***********************************************************************\
* Name   : Chunk_getSize
* Purpose: get size of chunk in bytes (without header and data elements)
* Input  : definition - chunk definition
*          alignment  - alignment to use
*          data       - chunk data
* Output : -
* Return : size of chunk
* Notes  : -
\***********************************************************************/

ulong Chunk_getSize(const int  *definition,
                    ulong      alignment,
                    const void *data
                   );

/***********************************************************************\
* Name   : Chunk_new
* Purpose: create new chunk handle
* Input  : chunkInfo       - chunk info block
*          parentChunkInfo - parent chunk info block
*          userData        - user data for i/o
*          alignment       - alignment to use
*          cryptInfo       - crypt info
*          dataCryptInfo   - crypt info for data elements
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_new(ChunkInfo *chunkInfo,
                 ChunkInfo *parentChunkInfo,
                 void      *userData,
                 uint      alignment,
                 CryptInfo *cryptInfo
                );

/***********************************************************************\
* Name   : Chunk_delete
* Purpose: delete chunk handle
* Input  : chunkInfo - chunk info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Chunk_delete(ChunkInfo *chunkInfo);

/***********************************************************************\
* Name   : Chunk_next
* Purpose: get next chunk
* Input  : userData - user data for file i/o
* Output : chunkHeader - chunk header
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_next(void        *userData,
                  ChunkHeader *chunkHeader
                 );

/***********************************************************************\
* Name   : Chunk_skip
* Purpose: skip chink
* Input  : userData    - user data for file i/o
*          chunkHeader - chunk header
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_skip(void        *userData,
                  ChunkHeader *chunkHeader
                 );

/***********************************************************************\
* Name   : Chunk_eof
* Purpose: check of end of chunks (file)
* Input  : userData - user data for file i/o
* Output : -
* Return : TRUE if end of chunks, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunk_eof(void *userData);

/***********************************************************************\
* Name   : Chunk_open
* Purpose: open chunk
* Input  : chunkInfo      - chunk info block
*          chunkHeader    - chunk header
*          chunkId        - chunk id
*          definition     - chunk definition
*          definitionSize - size of chunk (without data elements)
* Output : data - chunk data
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_open(ChunkInfo   *chunkInfo,
                  ChunkHeader *chunkHeader,
                  ChunkId     chunkId,
                  const int   *definition,
                  ulong       definitionSize,
                  void        *data
                 );

/***********************************************************************\
* Name   : Chunk_create
* Purpose: create new chunk
* Input  : chunkInfo      - chunk info block
*          chunkId        - chunk id
*          definition     - chunk definition
*          definitionSize - size of chunk (without data elements)
*          data           - chunk data
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_create(ChunkInfo  *chunkInfo,
                    ChunkId    chunkId,
                    const int  *definition,
                    ulong      definitionSize,
                    const void *data
                   );

/***********************************************************************\
* Name   : Chunk_close
* Purpose: update chunk header and close chunk
* Input  : chunkInfo - chunk info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_close(ChunkInfo *chunkInfo);

/***********************************************************************\
* Name   : Chunk_nextSub
* Purpose: get next sub-chunk
* Input  : chunkInfo   - chunk info block
*          chunkHeader - chunk header
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_nextSub(ChunkInfo   *chunkInfo,
                     ChunkHeader *chunkHeader
                    );

/***********************************************************************\
* Name   : Chunk_skipSub
* Purpose: skip sub-chunk
* Input  : chunkInfo   - chunk info block
*          chunkHeader - chunk header
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_skipSub(ChunkInfo   *chunkInfo,
                     ChunkHeader *chunkHeader
                    );

/***********************************************************************\
* Name   : Chunk_eofSub
* Purpose: check if end of sub-chunks
* Input  : chunkInfo - chunk info block
* Output : -
* Return : TRUE if end of sub-chunks, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunk_eofSub(ChunkInfo *chunkInfo);

/***********************************************************************\
* Name   : Chunk_update
* Purpose: update chunk data
* Input  : chunkInfo - chunk info block
*          data      - chunk data
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_update(ChunkInfo  *chunkInfo,
                    const void *data
                   );

/***********************************************************************\
* Name   : Chunk_readData
* Purpose: read data from chunk
* Input  : chunkInfo   - chunk info block
*          data        - buffer for data
*          size        - number of bytes to read
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_readData(ChunkInfo *chunkInfo,
                      void      *data,
                      ulong     size
                     );

/***********************************************************************\
* Name   : Chunk_writeData
* Purpose: write data into chunk
* Input  : chunkInfo   - chunk info block
*          data        - buffer with data
*          size        - number of bytes to write
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_writeData(ChunkInfo  *chunkInfo,
                       const void *data,
                       ulong      size
                      );

/***********************************************************************\
* Name   : Chunk_skipData
* Purpose: skip chunk data
* Input  : chunkInfo - chunk info block
*          size      - number of bytes to skip
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_skipData(ChunkInfo *chunkInfo,
                      ulong     size
                     );

#ifdef __cplusplus
  }
#endif

#endif /* __CHUNKS__ */

/* end of file */
