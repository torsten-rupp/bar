/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/files.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: 
* Systems :
*
\***********************************************************************/

#ifndef __FILES__
#define __FILES__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "bar.h"
#include "chunks.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef struct
{
  String    fileName;
  uint64    partSize;

  int       partNumber;
  int       handle;
  uint64    index;
  uint64    size;

  ChunkInfo chunkInfo;
  bool      chunkFlag;
} ArchiveInfo;

typedef enum
{
  FILETYPE_NONE,

  FILETYPE_FILE,
  FILETYPE_LINK,
  FILETYPE_DIRECTORY,

  FILETYPE_UNKNOWN
} FileTypes;

typedef struct
{
  ArchiveInfo *archiveInfo;

  uint16      fileType;
  uint64      size;
  uint64      timeLastAccess;
  uint64      timeModified;
  uint64      timeLastChanged;
  uint32      userId;
  uint32      groupId;
  uint32      permission;
  String      name;

  uint64      partOffset;
  uint64      partSize;
} FileInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

Errors files_create(ArchiveInfo *archiveInfo,
                    const char  *archiveFileName,
                    uint64      partSize
                   );
Errors files_open(ArchiveInfo *archiveInfo,
                  const char  *archiveFileName
                 );
Errors files_done(ArchiveInfo *archiveInfo);

bool files_eof(ArchiveInfo *archiveInfo);

Errors files_writeData(const void *buffer, ulong bufferLength);
Errors files_readData(void *buffer, ulong bufferLength);

Errors files_getNext(ArchiveInfo *archiveInfo,
                     ChunkId     *chunkId
                    );

Errors files_newFile(ArchiveInfo *archiveInfo,
                     FileInfo    *fileInfo,
                     String      fileName,
                     uint64      size,
                     uint64      timeLastAccess,
                     uint64      timeModified,
                     uint64      timeLastChanged,
                     uint32      userId,
                     uint32      groupId,
                     uint32      permission
                    );
Errors files_readFile(ArchiveInfo *archiveInfo,
                      FileInfo    *fileInfo
                     );
Errors files_closeFile(FileInfo *fileInfo);

Errors files_writeFileData(FileInfo *fileInfo, const void *buffer, ulong bufferLength);
Errors files_readFileData(FileInfo *fileInfo, void *buffer, ulong bufferLength);

#ifdef __cplusplus
  }
#endif

#endif /* __FILES__ */

/* end of file */
