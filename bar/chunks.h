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
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"

#include "crypt.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/* chunk layout:

 0        4        12                        12+L1                   12+L1+L2
---------------------------------------------------------------------------+
 |   id   |  size  | fixed data      | pad   | data/sub-chunks | pad | crc |
 +-------------------------------------------------------------------------+
                    <-- chunkSize -->
                    <-- *1 -----------------> <-- *1 --------------->
  <-- 12 bytes ---> <-- size --------------------------------------------->

  *1 = multiple of crypt block size if encrypted, otherwise multiple of 4

*/

// size of chunk header
#define CHUNK_HEADER_SIZE (4L+8L)

// chunk ids
#define CHUNK_ID_NONE 0

// chunk data types
#define CHUNK_DATATYPE_NONE       0
#define CHUNK_DATATYPE_BYTE       1
#define CHUNK_DATATYPE_UINT8      2
#define CHUNK_DATATYPE_UINT16     3
#define CHUNK_DATATYPE_UINT32     4
#define CHUNK_DATATYPE_UINT64     5
#define CHUNK_DATATYPE_INT8       6
#define CHUNK_DATATYPE_INT16      7
#define CHUNK_DATATYPE_INT32      8
#define CHUNK_DATATYPE_INT64      9
#define CHUNK_DATATYPE_STRING    10
#define CHUNK_DATATYPE_DATA      11
#define CHUNK_DATATYPE_CRC32     12

#define CHUNK_DATATYPE_ARRAY   0x40
#define CHUNK_DATATYPE_FIXED   0x00
#define CHUNK_DATATYPE_DYNAMIC 0x80

// chunk alignment
#define CHUNK_ALIGN             127

// chunk i/o modes
typedef enum
{
  CHUNK_MODE_READ,
  CHUNK_MODE_WRITE,

  CHUNK_MODE_UNKNOWN
} ChunkModes;

// indicate to use data from parent chunk
#define CHUNK_USE_PARENT NULL

// chunk flags
#define CHUNK_FLAG_DATA (1 << 0)

/***************************** Datatypes *******************************/

// i/o functions
typedef struct
{
  /***********************************************************************\
  * Name   : eof
  * Purpose: check end of data
  * Input  : userData - userdata
  * Output : -
  * Return : TRUE iff end of data
  * Notes  : -
  \***********************************************************************/

  bool(*eof)(void *userData);

  /***********************************************************************\
  * Name   : read
  * Purpose: read data
  * Input  : length   - buffer length [bytes]
  *          userData - userdata
  * Output : buffer    - buffer
  *          bytesRead - read bytes
  * Return : ERROR_NONE or error code
  * Notes  : -
  \***********************************************************************/

  Errors(*read)(void *userData, void *buffer, ulong length, ulong *bytesRead);

  /***********************************************************************\
  * Name   : write
  * Purpose: write data
  * Input  : buffer   - buffer
  *          length   - length of data [bytes]
  *          userData - userdata
  * Output : -
  * Return : ERROR_NONE or error code
  * Notes  : -
  \***********************************************************************/

  Errors(*write)(void *userData, const void *buffer, ulong length);

  /***********************************************************************\
  * Name   : tell
  * Purpose: tell position
  * Input  : userData - userdata
  * Output : offset - offset [0..n-1]
  * Return : ERROR_NONE or error code
  * Notes  : -
  \***********************************************************************/

  Errors(*tell)(void *userData, uint64 *offset);

  /***********************************************************************\
  * Name   : seek
  * Purpose: seek to position
  * Input  : offset   - offset [0..n-1]
  *          userData - userdata
  * Output : -
  * Return : ERROR_NONE or error code
  * Notes  : -
  \***********************************************************************/

  Errors(*seek)(void *userData, uint64 offset);
//  Errors(*seek)(uint64 offset, void *userData);

  /***********************************************************************\
  * Name   : getSize
  * Purpose: get size of transfer
  * Input  : userData - userdata
  * Output : -
  * Return : size or -1 if unknown
  * Notes  : -
  \***********************************************************************/

  int64(*getSize)(void *userData);
} ChunkIO;

// chunk id: 4 characters
typedef uint32 ChunkId;

// chunk definition
typedef const uint ChunkDefinition;

// chunk transformation info
typedef Errors(*ChunkTransformFunction)(void *oldChunk, void *newChunk, void *userData);
typedef struct
{
  struct
  {
    ChunkId         id;
    uint            allocSize;
    uint            fixedSize;
    ChunkDefinition *definition;
  } old,new;
  ChunkTransformFunction transformFunction;
} ChunkTransformInfo;

// chunk header (Note: only id+size is stored in file!)
typedef struct
{
  // header data
  union
  {
    ChunkId                id;
    char                   idChars[4];
  };                                  // chunk id
  uint64                   size;      // size of chunk (without chunk header) [bytes]

  // additional data (not stored in file)
  uint64                   offset;      // start of chunk in file (offset of header)
  const ChunkTransformInfo *transformInfo;
} ChunkHeader;

// chunk information
typedef struct ChunkInfo
{
  struct ChunkInfo *parentChunkInfo;
  const ChunkIO    *io;               // i/o functions
  void             *ioUserData;       // user data for i/o

  ChunkModes       mode;              // mode read/write
  uint             alignment;         // alignment for chunk
  CryptInfo        *cryptInfo;        // encryption

  union
  {
    ChunkId id;
    char    idChars[4];
  };                                  // chunk id
  ChunkDefinition  *definition;       // chunk definition
  ulong            chunkSize;         // size of chunk (without chunk header+data elements) [bytes]
  uint64           size;              // size of chunk with sub-chunks (without chunk header) [bytes]
  uint64           offset;            // start of chunk in file (offset of header)
  uint64           index;             // current position inside chunk 0..size

  void             *data;             // chunk data
} ChunkInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define Chunk_init(...)   __Chunk_init(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Chunk_done(...)   __Chunk_done(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Chunk_open(...)   __Chunk_open(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Chunk_create(...) __Chunk_create(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

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
* Input  : chunkInfo  - chunk info block
*          chunkData  - chunk data
*          dataLength - length of data
* Output : -
* Return : size of chunk
* Notes  : -
\***********************************************************************/

ulong Chunk_getSize(const ChunkInfo *chunkInfo,
                    const void      *chunkData,
                    ulong           dataLength
                   );

/***********************************************************************\
* Name   : Chunk_init
* Purpose: init chunk info block
* Input  : chunkInfo       - chunk info block to initialize
*          parentChunkInfo - parent chunk info block
*          chunkIO         - i/o functions or NULL
*          chunkIOUserData - user data for i/o or NULL
*          alignment       - alignment to use
*          cryptInfo       - crypt info
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Errors Chunk_init(ChunkInfo       *chunkInfo,
                  ChunkInfo       *parentChunkInfo,
                  const ChunkIO   *chunkIO,
                  void            *chunkIOUserData,
                  ChunkId         chunkId,
                  ChunkDefinition *definition,
                  uint            alignment,
                  CryptInfo       *cryptInfo,
                  void            *data
                 );
#else /* not NDEBUG */
Errors __Chunk_init(const char      *__fileName__,
                    ulong           __lineNb__,
                    ChunkInfo       *chunkInfo,
                    ChunkInfo       *parentChunkInfo,
                    const ChunkIO   *chunkIO,
                    void            *chunkIOUserData,
                    ChunkId         chunkId,
                    ChunkDefinition *definition,
                    uint            alignment,
                    CryptInfo       *cryptInfo,
                    void            *data
                   );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Chunk_done
* Purpose: deinit chunk info block
* Input  : chunkInfo - chunk info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void Chunk_done(ChunkInfo *chunkInfo);
#else /* not NDEBUG */
void __Chunk_done(const char *__fileName__,
                  ulong      __lineNb__,
                  ChunkInfo  *chunkInfo
                 );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Chunk_next
* Purpose: get next chunk header
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
* Purpose: skip chunk
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

INLINE bool Chunk_eof(const ChunkIO *chunkIO,
                      void          *chunkIOUserData
                     );
#if defined(NDEBUG) || defined(__CHUNKS_IMPLEMENTATION__)
INLINE bool Chunk_eof(const ChunkIO *chunkIO,
                      void          *chunkIOUserData
                     )
{
  assert(chunkIO != NULL);
  assert(chunkIO->eof != NULL);

  return chunkIO->eof(chunkIOUserData);
}
#endif /* NDEBUG || __CHUNKS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Chunk_startOffset
* Purpose: get absolute chunk start offset
* Input  : chunkInfo - chunk info block
* Output : -
* Return : start offset
* Notes  : -
\***********************************************************************/

INLINE uint64 Chunk_startOffset(const ChunkInfo *chunkInfo);
#if defined(NDEBUG) || defined(__CHUNKS_IMPLEMENTATION__)
INLINE uint64 Chunk_startOffset(const ChunkInfo *chunkInfo)
{
  assert(chunkInfo != NULL);
  assert(chunkInfo->offset >= CHUNK_HEADER_SIZE);

  return chunkInfo->offset-CHUNK_HEADER_SIZE;
}
#endif /* NDEBUG || __CHUNKS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Chunk_endOffset
* Purpose: get absolute chunk end offset
* Input  : chunkInfo - chunk info block
* Output : -
* Return : start offset
* Notes  : -
\***********************************************************************/

INLINE uint64 Chunk_endOffset(const ChunkInfo *chunkInfo);
#if defined(NDEBUG) || defined(__CHUNKS_IMPLEMENTATION__)
INLINE uint64 Chunk_endOffset(const ChunkInfo *chunkInfo)
{
  assert(chunkInfo != NULL);
  assert(chunkInfo->offset >= CHUNK_HEADER_SIZE);

  return CHUNK_HEADER_SIZE+chunkInfo->offset+chunkInfo->size;
}
#endif /* NDEBUG || __CHUNKS_IMPLEMENTATION__ */

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
* Name   : Chunk_transfer
* Purpose: transfer chunk
* Input  : chunkHeader         - chunk header
*          fromChunkIO         - from chunk i/o
*          fromChunkIOUserData - from chunk i/o userdata
*          toChunkIO           - to chunk i/o
*          toChunkIOUserData   - to chunk i/o userdata
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Chunk_transfer(const ChunkHeader *chunkHeader,
                      const ChunkIO     *fromChunkIO,
                      void              *fromChunkIOUserData,
                      const ChunkIO     *toChunkIO,
                      void              *toChunkIOUserData
                     );

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

#ifdef NDEBUG
Errors Chunk_open(ChunkInfo         *chunkInfo,
                  const ChunkHeader *chunkHeader,
                  ulong             dataSize,
                  void              *transformUserData
                 );
#else /* not NDEBUG */
Errors __Chunk_open(const char        *__fileName__,
                    ulong             __lineNb__,
                    ChunkInfo         *chunkInfo,
                    const ChunkHeader *chunkHeader,
                    ulong             dataSize,
                    void              *transformUserData
                   );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Chunk_create
* Purpose: create new chunk
* Input  : chunkInfo - chunk info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Errors Chunk_create(ChunkInfo *chunkInfo);
#else /* not NDEBUG */
Errors __Chunk_create(const char *__fileName__,
                      ulong      __lineNb__,
                      ChunkInfo  *chunkInfo
                     );
#endif /* NDEBUG */

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

Errors Chunk_skipSub(ChunkInfo         *chunkInfo,
                     const ChunkHeader *chunkHeader
                    );

/***********************************************************************\
* Name   : Chunk_eofSub
* Purpose: check if end of sub-chunks
* Input  : chunkInfo - chunk info block
* Output : -
* Return : TRUE if end of sub-chunks, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool Chunk_eofSub(ChunkInfo *chunkInfo);
#if defined(NDEBUG) || defined(__CHUNKS_IMPLEMENTATION__)
INLINE bool Chunk_eofSub(ChunkInfo *chunkInfo)
{
  assert(chunkInfo != NULL);

  return chunkInfo->index >= chunkInfo->size;
}
#endif /* NDEBUG || __CHUNKS_IMPLEMENTATION__ */

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
* Input  : chunkInfo - chunk info block
*          data      - buffer for data
*          size      - max. number of bytes to read
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
* Input  : chunkInfo - chunk info block
*          data      - buffer with data
*          size      - number of bytes to write
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
