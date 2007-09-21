/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/chunks.h,v $
* $Revision: 1.9 $
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
* Name   : Chunks_init
* Purpose: init chunks functions
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunks_init(bool(*endOfFile)(void *userData),
                   bool(*readFile)(void *userData, void *buffer, ulong length),
                   bool(*writeFile)(void *userData, const void *buffer, ulong length),
                   bool(*tellFile)(void *userData, uint64 *offset),
                   bool(*seekFile)(void *userData, uint64 offset)
                  );

/***********************************************************************\
* Name   : Chunks_done
* Purpose: done chunks functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Chunks_done(void);

/***********************************************************************\
* Name   : Chunks_getSize
* Purpose: get size of chunk in bytes (without header and data elements)
* Input  : definition - chunk definition
*          alignment  - alignment to use
*          data       - chunk data
* Output : -
* Return : size of chunk
* Notes  : -
\***********************************************************************/

ulong Chunks_getSize(const int  *definition,
                     ulong      alignment,
                     const void *data
                    );

/***********************************************************************\
* Name   : Chunks_new
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

Errors Chunks_new(ChunkInfo *chunkInfo,
                  ChunkInfo *parentChunkInfo,
                  void      *userData,
                  uint      alignment,
                  CryptInfo *cryptInfo
                 );

/***********************************************************************\
* Name   : Chunks_delete
* Purpose: delete chunk handle
* Input  : chunkInfo - chunk info block
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
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunks_next(void        *userData,
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
* Input  : chunkInfo      - chunk info block
*          chunkHeader    - chunk header
*          chunkId        - chunk id
*          definition     - chunk definition
*          definitionSize - size of chunk (without data elements)
* Output : data - chunk data
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunks_open(ChunkInfo   *chunkInfo,
                   ChunkHeader *chunkHeader,
                   ChunkId     chunkId,
                   const int   *definition,
                   ulong       definitionSize,
                   void        *data
                  );

/***********************************************************************\
* Name   : Chunks_create
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

Errors Chunks_create(ChunkInfo  *chunkInfo,
                     ChunkId    chunkId,
                     const int  *definition,
                     ulong      definitionSize,
                     const void *data
                    );

/***********************************************************************\
* Name   : Chunks_close
* Purpose: update chunk header and close chunk
* Input  : chunkInfo - chunk info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunks_close(ChunkInfo *chunkInfo);

/***********************************************************************\
* Name   : Chunks_nextSub
* Purpose: get next sub-chunk
* Input  : chunkInfo   - chunk info block
*          chunkHeader - chunk header
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunks_nextSub(ChunkInfo   *chunkInfo,
                      ChunkHeader *chunkHeader
                     );

/***********************************************************************\
* Name   : Chunks_skipSub
* Purpose: skip sub-chunk
* Input  : chunkInfo   - chunk info block
*          chunkHeader - chunk header
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunks_skipSub(ChunkInfo   *chunkInfo,
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
* Name   : Chunks_update
* Purpose: update chunk data
* Input  : chunkInfo - chunk info block
*          data      - chunk data
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunks_update(ChunkInfo  *chunkInfo,
                     const void *data
                    );

/***********************************************************************\
* Name   : Chunks_readData
* Purpose: read data from chunk
* Input  : chunkInfo   - chunk info block
*          data        - buffer for data
*          size        - number of bytes to read
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunks_readData(ChunkInfo *chunkInfo,
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
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunks_writeData(ChunkInfo  *chunkInfo,
                        const void *data,
                        ulong      size
                       );

/***********************************************************************\
* Name   : Chunks_skipData
* Purpose: skip chunk data
* Input  : chunkInfo - chunk info block
*          size      - number of bytes to skip
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunks_skipData(ChunkInfo *chunkInfo,
                       ulong     size
                      );

#ifdef __cplusplus
  }
#endif

#endif /* __CHUNKS__ */

/* end of file */
