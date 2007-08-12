/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/archive.h,v $
* $Revision: 1.3 $
* $Author: torsten $
* Contents: archive functions
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
#include "archive_format.h"
#include "files.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef struct
{
  String     fileName;
  uint64     partSize;

  int        partNumber;
  FileHandle fileHandle;
} ArchiveInfo;

typedef struct
{
  ArchiveInfo    *archiveInfo;

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
} ArchiveFileInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : archive_create
* Purpose: create archive
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors archive_create(ArchiveInfo *archiveInfo,
                      const char  *archiveFileName,
                      uint64      partSize
                     );

/***********************************************************************\
* Name   : archive_open
* Purpose: open archive
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors archive_open(ArchiveInfo *archiveInfo,
                    const char  *archiveFileName
                   );

/***********************************************************************\
* Name   : archive_done
* Purpose: close archive
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors archive_done(ArchiveInfo *archiveInfo);

/***********************************************************************\
* Name   : archive_eof
* Purpose: check if end-of-archive file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool archive_eof(ArchiveInfo *archiveInfo);

/***********************************************************************\
* Name   : archive_newFile
* Purpose: add new file to archive
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors archive_newFile(ArchiveInfo     *archiveInfo,
                       ArchiveFileInfo *archiveFileInfo,
                       const FileInfo  *fileInfo
                      );

/***********************************************************************\
* Name   : archive_readFile
* Purpose: read file info from archive
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors archive_readFile(ArchiveInfo     *archiveInfo,
                        ArchiveFileInfo *archiveFileInfo,
                        FileInfo        *fileInfo,
                        uint64          *partOffset,
                        uint64          *partSize
                       );

/***********************************************************************\
* Name   : archive_closeFile
* Purpose: clsoe file in archive
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors archive_closeFile(ArchiveFileInfo *archiveFileInfo);

/***********************************************************************\
* Name   : archive_writeFileData
* Purpose: write data to file in archive
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors archive_writeFileData(ArchiveFileInfo *archiveFileInfo,
                             const void      *buffer,
                             ulong           bufferLength);

/***********************************************************************\
* Name   : archive_readFileData
* Purpose: read data from file in archive
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors archive_readFileData(ArchiveFileInfo *archiveFileInfo,
                            void            *buffer,
                            ulong           bufferLength
                           );

#ifdef __cplusplus
  }
#endif

#endif /* __ARCHIVE__ */

/* end of file */
