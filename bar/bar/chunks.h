/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver file chunk functions
* Systems: all
*
\***********************************************************************/

#ifndef __CHUNKS__
#define __CHUNKS__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"

#include "crypt.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// size of chunk header
#define CHUNK_HEADER_SIZE (4+8)

// chunk ids
#define CHUNK_ID_NONE 0

// chunk data types
#define CHUNK_DATATYPE_UINT8    1
#define CHUNK_DATATYPE_UINT16   2
#define CHUNK_DATATYPE_UINT32   3
#define CHUNK_DATATYPE_UINT64   4
#define CHUNK_DATATYPE_INT8     5
#define CHUNK_DATATYPE_INT16    6
#define CHUNK_DATATYPE_INT32    7
#define CHUNK_DATATYPE_INT64    8
#define CHUNK_DATATYPE_STRING   9
#define CHUNK_DATATYPE_DATA    10
#define CHUNK_DATATYPE_CRC32   11

typedef enum
{
  CHUNK_MODE_UNKNOWN,
  CHUNK_MODE_WRITE,
  CHUNK_MODE_READ,
} ChunkModes;

/***************************** Datatypes *******************************/

// i/o functions
typedef struct
{
  // check end of data
  bool(*eof)(void *userData);
  // read data
  Errors(*read)(void *userData, void *buffer, ulong length, ulong *bytesRead);
  // write data
  Errors(*write)(void *userData, const void *buffer, ulong length);
  // tell position
  Errors(*tell)(void *userData, uint64 *offset);
  // seek to position
  Errors(*seek)(void *userData, uint64 offset);
  // get size
  uint64(*getSize)(void *userData);
} ChunkIO;

// chunk id: 4 characters
typedef uint32 ChunkId;

// chunk header (Note: only id+size is stored in file!)
typedef struct
{
  union
  {
    ChunkId id;
    char    idChars[4];
  };                                  // chunk id
  uint64 size;                        // size of chunk (without chunk header)
  uint64 offset;                      // start of chunk in file (offset of header)
} ChunkHeader;

// chunk information
typedef struct ChunkInfo
{
  struct ChunkInfo *parentChunkInfo;
  const ChunkIO    *io;               // i/o functions
  void             *ioUserData;       // user data for i/o

  ChunkModes       mode;
  uint             alignment;         // alignment for chunk
  CryptInfo        *cryptInfo;        // encryption

  union
  {
    ChunkId id;
    char    idChars[4];
  };                                  // chunk id
  const int        *definition;       // chunk definition
  ulong            chunkSize;         // size of fixed chunk data (without chunk header+data elements)
  uint64           size;              // total size of chunk (without chunk header)
  uint64           offset;            // start of chunk in file (offset of header)
  uint64           index;             // current position in chunk (from begin=end of header)

  void             *data;             // chunk data
} ChunkInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Chunk_initAll
* Purpose: init chunks
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_initAll(void);

/***********************************************************************\
* Name   : Chunk_doneAll
* Purpose: done chunks
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Chunk_doneAll(void);

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
* Name   : Chunk_init
* Purpose: init chunk info block
* Input  : chunkInfo       - chunk info block to initialize
*          parentChunkInfo - parent chunk info block
*          chunkIO         - i/o functions
*          chunkIOUserData - user data for i/o
*          alignment       - alignment to use
*          cryptInfo       - crypt info
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_init(ChunkInfo     *chunkInfo,
                  ChunkInfo     *parentChunkInfo,
                  const ChunkIO *chunkIO,
                  void          *chunkIOUserData,
                  ChunkId       chunkId,
                  const int     *definition,
                  uint          alignment,
                  CryptInfo     *cryptInfo,
                  void          *data
                 );

/***********************************************************************\
* Name   : Chunk_done
* Purpose: deinit chunk info block
* Input  : chunkInfo - chunk info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Chunk_done(ChunkInfo *chunkInfo);

/***********************************************************************\
* Name   : Chunk_next
* Purpose: get next chunk
* Input  : chunkIO         - i/o functions
*          chunkIOUserData - user data for file i/o
* Output : chunkHeader - chunk header
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_next(const ChunkIO *chunkIO,
                  void          *chunkIOUserData,
                  ChunkHeader   *chunkHeader
                 );

/***********************************************************************\
* Name   : Chunk_skip
* Purpose: skip chink
* Input  : chunkIO         - i/o functions
*          chunkIOUserData - user data for file i/o
*          chunkHeader     - chunk header
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_skip(const ChunkIO     *chunkIO,
                  void              *chunkIOUserData,
                  const ChunkHeader *chunkHeader
                 );

/***********************************************************************\
* Name   : Chunk_eof
* Purpose: check of end of chunks (file)
* Input  : chunkIO         - i/o functions
*          chunkIOUserData - user data for file i/o
* Output : -
* Return : TRUE if end of chunks, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Chunk_eof(const ChunkIO *chunkIO,
               void          *chunkIOUserData
              );

/***********************************************************************\
* Name   : Chunk_tell
* Purpose: get current index position in chunk
* Input  : chunkInfo - chunk info block
* Output : index - index
* Return : -
* Notes  : -
\***********************************************************************/

void Chunk_tell(ChunkInfo *chunkInfo, uint64 *index);

/***********************************************************************\
* Name   : Chunk_seek
* Purpose: restore current index position in chunk
* Input  : chunkInfo - chunk info block
*          index     - index
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Chunk_seek(ChunkInfo *chunkInfo, uint64 index);

/***********************************************************************\
* Name   : Chunk_open
* Purpose: open chunk
* Input  : chunkInfo   - chunk info block
*          chunkHeader - chunk header
*          dataSize    - data size of chunk
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_open(ChunkInfo         *chunkInfo,
                  const ChunkHeader *chunkHeader,
                  ulong             dataSize
                 );

/***********************************************************************\
* Name   : Chunk_create
* Purpose: create new chunk
* Input  : chunkInfo - chunk info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_create(ChunkInfo *chunkInfo);

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
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_update(ChunkInfo *chunkInfo);

/***********************************************************************\
* Name   : Chunk_readData
* Purpose: read data from chunk
* Input  : chunkInfo   - chunk info block
*          data        - buffer for data
*          size        - max. number of bytes to read
* Output : bytesRead - number of bytes read (can be NULL)
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_readData(ChunkInfo *chunkInfo,
                      void      *data,
                      ulong     size,
                      ulong     *bytesRead
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
