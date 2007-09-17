/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/archive.h,v $
* $Revision: 1.10 $
* $Author: torsten $
* Contents: Backup ARchiver archive functions
* Systems : all
*
\***********************************************************************/

#ifndef __ARCHIVE__
#define __ARCHIVE__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "errors.h"
#include "chunks.h"
#include "compress.h"
#include "crypt.h"
#include "archive_format.h"
#include "files.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef struct
{
  String             fileName;                       // archive basename
  uint64             partSize;                       // approximated part size
  CompressAlgorithms compressAlgorithm;              // default compression algorithm
  ulong              compressMinFileSize;            // min. file size to use compression
  CryptAlgorithms    cryptAlgorithm;                 // default crypt algorithm
  const char         *password;                      // password

  uint               blockLength;                    /* block length for file entry/file
                                                        data (depend on used crypt
                                                        algorithm)
                                                     */

  int                partNumber;                     // file part number
  bool               fileOpenFlag;                   // TRUE iff file is open
  FileHandle         fileHandle;                     // file handle

  bool               nextChunkHeaderReadFlag;        // TRUE iff next chunk header read
  ChunkHeader        nextChunkHeader;                // next file, directory, link chunk header
} ArchiveInfo;

typedef struct
{
  ArchiveInfo        *archiveInfo;

  enum
  {
    FILE_MODE_READ,
    FILE_MODE_WRITE,
  } mode;

  CryptAlgorithms    cryptAlgorithm;                 // crypt algorithm for file entry
  uint               blockLength;                    /* block length for file entry/file
                                                        data (depend on used crypt
                                                        algorithm)
                                                     */

  FileTypes          fileType;
  union
  {
    struct
    {
      CompressAlgorithms  compressAlgorithm;         // compression algorithm for file entry

      ChunkInfo           chunkInfoFile;             // chunk info block for file
      ChunkFile           chunkFile;                 // file

      ChunkInfo           chunkInfoFileEntry;        // chunk info block for file entry
      ChunkFileEntry      chunkFileEntry;            // file entry
      CryptInfo           cryptInfoFileEntry;        // file entry cryption info (without data elements)

      ChunkInfo           chunkInfoFileData;         // chunk info block for file data
      ChunkFileData       chunkFileData;             // file data
      CryptInfo           cryptInfoFileData;         // file data cryption info (without data elements)

      CompressInfo        compressInfoData;          // data compress info
      CryptInfo           cryptInfoData;             // data cryption info

      byte                *buffer;
      ulong               bufferLength;
    } file;
    struct
    {
      ChunkInfo           chunkInfoDirectory;        // chunk info block for directory
      ChunkDirectory      chunkDirectory;            // directory

      ChunkInfo           chunkInfoDirectoryEntry;   // chunk info block for directory entry
      ChunkDirectoryEntry chunkDirectoryEntry;       // directory entry
      CryptInfo           cryptInfoDirectoryEntry;   // directory entry cryption info
    } directory;
    struct
    {
      ChunkInfo           chunkInfoLink;             // chunk info block for link
      ChunkLink           chunkLink;                 // link

      ChunkInfo           chunkInfoLinkEntry;        // chunk info block for link entry
      ChunkLinkEntry      chunkLinkEntry;            // link entry
      CryptInfo           cryptInfoLinkEntry;        // link entry
    } link;

  };
  uint                headerLength;                  // length of header
  bool                headerWrittenFlag;             // TRUE iff header written
} ArchiveFileInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Archive_init
* Purpose: init archive functions
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Archive_init(void);

/***********************************************************************\
* Name   : Archive_done
* Purpose: done archive functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Archive_done(void);

/***********************************************************************\
* Name   : Archive_create
* Purpose: create archive
* Input  : archiveInfo         - archive info block
*          archiveFileName     - archive file name
*          partSize            - part size (in bytes)
*          compressAlgorithm   - compression algorithm to use
*          compressMinFileSize - min. size of file to use compression
*          cryptAlgorithm      - crypt algorithm to use
*          password            - crypt password
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Archive_create(ArchiveInfo        *archiveInfo,
                      const char         *archiveFileName,
                      uint64             partSize,
                      CompressAlgorithms compressAlgorithm,
                      ulong              compressMinFileSize,
                      CryptAlgorithms    cryptAlgorithm,
                      const char         *password
                     );

/***********************************************************************\
* Name   : Archive_open
* Purpose: open archive
* Input  : archiveInfo     - archive info block
*          archiveFileName - archive file name
*          password        - crypt password
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Archive_open(ArchiveInfo *archiveInfo,
                    const char  *archiveFileName,
                    const char  *password
                   );

/***********************************************************************\
* Name   : Archive_close
* Purpose: close archive
* Input  : archiveInfo - archive info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Archive_close(ArchiveInfo *archiveInfo);

/***********************************************************************\
* Name   : Archive_eof
* Purpose: check if end-of-archive file
* Input  : archiveInfo - archive info block
* Output : -
* Return : TRUE if end-of-archive, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Archive_eof(ArchiveInfo *archiveInfo);

/***********************************************************************\
* Name   : Archive_newFile
* Purpose: add new file to archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
*          fileName        - file name
*          fileInfo        - file info
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Archive_newFileEntry(ArchiveInfo     *archiveInfo,
                            ArchiveFileInfo *archiveFileInfo,
                            const String    fileName,
                            const FileInfo  *fileInfo
                           );

/***********************************************************************\
* Name   : Archive_newDirectoryEntry
* Purpose: add new directory to archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
*          directoryName   - directory name
*          fileInfo        - file info
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Archive_newDirectoryEntry(ArchiveInfo     *archiveInfo,
                                 ArchiveFileInfo *archiveFileInfo,
                                 const String    directoryName,
                                 FileInfo        *fileInfo
                                );

/***********************************************************************\
* Name   : Archive_newLinkEntry
* Purpose: add new link to archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
*          name            - link name
*          fileName        - link reference name
*          fileInfo        - file info
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Archive_newLinkEntry(ArchiveInfo     *archiveInfo,
                            ArchiveFileInfo *archiveFileInfo,
                            const String    name,
                            const String    fileName,
                            FileInfo        *fileInfo
                           );

/***********************************************************************\
* Name   : Archive_getNextFileType
* Purpose: get type of next entry in archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
* Output : FileTypes - file type
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Archive_getNextFileType(ArchiveInfo     *archiveInfo,
                               ArchiveFileInfo *archiveFileInfo,
                               FileTypes       *fileType
                              );

/***********************************************************************\
* Name   : Archive_readFile
* Purpose: read file info from archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
* Output : compressAlgorithm - used compression algorithm (can be NULL)
*          fileName          - file name
*          fileInfo          - file info
*          cryptAlgorithm    - use crypt algorithm (can be NULL)
*          partOffset        - part offset (can be NULL)
*          partSize          - part size in bytes (can be NULL)
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Archive_readFileEntry(ArchiveInfo        *archiveInfo,
                             ArchiveFileInfo    *archiveFileInfo,
                             CompressAlgorithms *compressAlgorithm,
                             CryptAlgorithms    *cryptAlgorithm,
                             String             fileName,
                             FileInfo           *fileInfo,
                             uint64             *partOffset,
                             uint64             *partSize
                            );

/***********************************************************************\
* Name   : Archive_readFile
* Purpose: read file info from archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
* Output : cryptAlgorithm    - use crypt algorithm (can be NULL)
*          directoryName     - directory name
*          fileInfo          - file info
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Archive_readDirectoryEntry(ArchiveInfo     *archiveInfo,
                                  ArchiveFileInfo *archiveFileInfo,
                                  CryptAlgorithms *cryptAlgorithm,
                                  String          directoryName,
                                  FileInfo        *fileInfo
                                 );

/***********************************************************************\
* Name   : Archive_readFile
* Purpose: read file info from archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
* Output : cryptAlgorithm - use crypt algorithm (can be NULL)
*          name           - link name
*          fileName       - link reference name
*          fileInfo       - file info
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Archive_readLinkEntry(ArchiveInfo     *archiveInfo,
                             ArchiveFileInfo *archiveFileInfo,
                             CryptAlgorithms *cryptAlgorithm,
                             String          name,
                             String          fileName,
                             FileInfo        *fileInfo
                            );

/***********************************************************************\
* Name   : Archive_closeEntry
* Purpose: clsoe file in archive
* Input  : archiveFileInfo - archive file info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Archive_closeEntry(ArchiveFileInfo *archiveFileInfo);

/***********************************************************************\
* Name   : Archive_writeFileData
* Purpose: write data to file in archive
* Input  : archiveFileInfo - archive file info block
*          buffer          - data buffer
*          bufferLength    - length of data buffer
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Archive_writeFileData(ArchiveFileInfo *archiveFileInfo,
                             const void      *buffer,
                             ulong           bufferLength
                            );

/***********************************************************************\
* Name   : Archive_readFileData
* Purpose: read data from file in archive
* Input  : archiveFileInfo - archive file info block
*          buffer          - data buffer
*          bufferLength    - length of data buffer
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Archive_readFileData(ArchiveFileInfo *archiveFileInfo,
                            void            *buffer,
                            ulong           bufferLength
                           );

#ifdef __cplusplus
  }
#endif

#endif /* __ARCHIVE__ */

/* end of file */
