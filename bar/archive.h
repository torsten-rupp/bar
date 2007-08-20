/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/archive.h,v $
* $Revision: 1.5 $
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

#include "bar.h"
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
  String             fileName;
  uint64             partSize;
  CompressAlgorithms compressAlgorithm;
  CryptAlgorithms    cryptAlgorithm;
  const char         *password;

  int                partNumber;
  FileHandle         fileHandle;
} ArchiveInfo;

typedef struct
{
  ArchiveInfo    *archiveInfo;

  uint           blockLength;              /* block length for file entry/file
                                              data (depend on used crypt
                                              algorithm)
                                            */

  CompressInfo   compressInfo;             // file data compress info  

  CryptInfo      cryptInfoFileEntry;       // file entry cryption info
  CryptInfo      cryptInfoFileData;        // file data cryption info

  enum
  {
    FILE_MODE_READ,
    FILE_MODE_WRITE,
  } mode;

  ChunkInfo      chunkInfoFile;            // chunk info block for file
  ChunkFile      chunkFile;                // file
  ChunkInfo      chunkInfoFileEntry;       // chunk info block for file entry
  ChunkFileEntry chunkFileEntry;           // file entry
  ChunkInfo      chunkInfoFileData;        // chunk info block for file data
  ChunkFileData  chunkFileData;            // file data

  uint           headerLength;             // length of header
  bool           headerWrittenFlag;        // TRUE iff header written

  byte           *buffer;
  ulong          bufferLength;
} ArchiveFileInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Archive_create
* Purpose: create archive
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Archive_create(ArchiveInfo        *archiveInfo,
                      const char         *archiveFileName,
                      uint64             partSize,
                      CompressAlgorithms compressAlgorithm,
                      CryptAlgorithms    cryptAlgorithm,
                      const char         *password
                     );

/***********************************************************************\
* Name   : Archive_open
* Purpose: open archive
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Archive_open(ArchiveInfo *archiveInfo,
                    const char  *archiveFileName,
                    const char  *password
                   );

/***********************************************************************\
* Name   : Archive_done
* Purpose: close archive
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Archive_done(ArchiveInfo *archiveInfo);

/***********************************************************************\
* Name   : Archive_eof
* Purpose: check if end-of-archive file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Archive_eof(ArchiveInfo *archiveInfo);

/***********************************************************************\
* Name   : Archive_newFile
* Purpose: add new file to archive
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Archive_newFile(ArchiveInfo     *archiveInfo,
                       ArchiveFileInfo *archiveFileInfo,
                       const String    fileName,
                       const FileInfo  *fileInfo
                      );

/***********************************************************************\
* Name   : Archive_readFile
* Purpose: read file info from archive
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Archive_readFile(ArchiveInfo     *archiveInfo,
                        ArchiveFileInfo *archiveFileInfo,
                        String          fileName,
                        FileInfo        *fileInfo,
                        uint64          *partOffset,
                        uint64          *partSize
                       );

/***********************************************************************\
* Name   : Archive_closeFile
* Purpose: clsoe file in archive
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Archive_closeFile(ArchiveFileInfo *archiveFileInfo);

/***********************************************************************\
* Name   : Archive_writeFileData
* Purpose: write data to file in archive
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Archive_writeFileData(ArchiveFileInfo *archiveFileInfo,
                             const void      *buffer,
                             ulong           bufferLength
                            );

/***********************************************************************\
* Name   : Archive_readFileData
* Purpose: read data from file in archive
* Input  : -
* Output : -
* Return : -
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
