/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/files.h,v $
* $Revision: 1.4 $
* $Author: torsten $
* Contents: files functions
* Systems : all
*
\***********************************************************************/

#ifndef __FILES_H__
#define __FILES_H__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "bar.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define FILES_PATHNAME_SEPARATOR_CHAR '/'
#define FILES_PATHNAME_SEPARATOR_CHARS "/"

typedef enum
{
  FILETYPE_NONE,

  FILETYPE_FILE,
  FILETYPE_LINK,
  FILETYPE_DIRECTORY,

  FILETYPE_UNKNOWN
} FileTypes;

/***************************** Datatypes *******************************/
typedef struct
{
  int    handle;
  uint64 size;
} FileHandle;

typedef struct
{
  String        name;
  DIR           *handle;
  struct dirent *entry;
} DirectoryHandle;

typedef struct
{
  String name;
  uint64 size;
  uint64 timeLastAccess;
  uint64 timeModified;
  uint64 timeLastChanged;
  uint32 userId;
  uint32 groupId;
  uint32 permission;
} FileInfo;

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

Errors files_create(FileHandle   *fileHandle,
                    const String fileName
                   );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors files_open(FileHandle   *fileHandle,
                  const String fileName
                 );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors files_close(FileHandle *fileHandle);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool files_eof(FileHandle *fileHandle);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors files_read(FileHandle *fileHandle,
                  void       *buffer,
                  ulong      bufferLength
                 );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors files_write(FileHandle *fileHandle,
                   const void *buffer,
                   ulong      bufferLength
                  );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors files_tell(FileHandle *fileHandle,
                  uint64     *offset
                 );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors files_seek(FileHandle *fileHandle, uint64 offset);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors files_openDirectory(DirectoryHandle *directoryHandle,
                           const String    pathName);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void files_closeDirectory(DirectoryHandle *directoryHandle);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool files_endOfDirectory(DirectoryHandle *directoryHandle);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors files_readDirectory(DirectoryHandle *directoryHandle,
                           String          fileName
                          );

/***********************************************************************\
* Name   : files_getType
* Purpose: get file type
* Input  : fileName - file name
* Output : -
* Return : file type; see FileTypes
* Notes  : -
\***********************************************************************/

FileTypes files_getType(String fileName);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors files_getInfo(String   fileName,
                     FileInfo *fileInfo
                    );

#ifdef __cplusplus
  }
#endif

#endif /* __FILES_H__ */

/* end of file */
