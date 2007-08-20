/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/files.h,v $
* $Revision: 1.6 $
* $Author: torsten $
* Contents: Backup ARchiver files functions
* Systems : all
*
\***********************************************************************/

#ifndef __FILES__
#define __FILES__

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

typedef enum
{
  FILE_OPENMODE_CREATE,
  FILE_OPENMODE_READ,
  FILE_OPENMODE_WRITE,
} FileOpenModes;

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
* Name   : Files_getFilePath
* Purpose: get path of filename
* Input  : fileName - file name
*          path     - path variable
* Output : -
* Return : path
* Notes  : -
\***********************************************************************/

String Files_getFilePath(String fileName, String path);

/***********************************************************************\
* Name   : Files_getFileBaseName
* Purpose: get basename of file
* Input  : fileName - file name
*          baseName - basename variable
* Output : -
* Return : basename
* Notes  : -
\***********************************************************************/

String Files_getFileBaseName(String fileName, String baseName);

/***********************************************************************\
* Name   : Files_fileNameAppend
* Purpose: append name to file name
* Input  : fileName - file name
*          name     - name to append
* Output : -
* Return : file name
* Notes  : -
\***********************************************************************/

String Files_appendFileName(String fileName, String name);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Files_openCreate
* Purpose: create new file
* Input  : fileHandle - file handle
*          fileName   - file name
* Output : fileHandle - file handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Files_open(FileHandle    *fileHandle,
                  const String  fileName,
                  FileOpenModes fileOpenMode
                 );

/***********************************************************************\
* Name   : Files_close
* Purpose: close file
* Input  : fileHandle - file handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Files_close(FileHandle *fileHandle);

/***********************************************************************\
* Name   : Files_eof
* Purpose: check if end-of-file
* Input  : fileHandle - file handle
* Output : -
* Return : TRUE if end-of-file, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Files_eof(FileHandle *fileHandle);

/***********************************************************************\
* Name   : Files_read
* Purpose: read data from file
* Input  : fileHandle   - file handle
*          buffer       - buffer for data to read
*          bufferLength - length of data to read
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Files_read(FileHandle *fileHandle,
                  void       *buffer,
                  ulong      bufferLength,
                  ulong      *readBytes
                 );

/***********************************************************************\
* Name   : Files_write
* Purpose: write data into file
* Input  : fileHandle   - file handle
*          buffer       - buffer for data to write
*          bufferLength - length of data to write
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Files_write(FileHandle *fileHandle,
                   const void *buffer,
                   ulong      bufferLength
                  );

/***********************************************************************\
* Name   : Files_size
* Purpose: get file size
* Input  : fileHandle - file handle
* Output : -
* Return : size of file (in bytes)
* Notes  : -
\***********************************************************************/

uint64 Files_size(FileHandle *fileHandle);

/***********************************************************************\
* Name   : Files_tell
* Purpose: get current position in file
* Input  : fileHandle - file handle
* Output : offset - offset (0..n-1)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Files_tell(FileHandle *fileHandle,
                  uint64     *offset
                 );

/***********************************************************************\
* Name   : Files_seek
* Purpose: seek in file
* Input  : fileHandle - file handle
*          offset     - offset (0..n-1)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Files_seek(FileHandle *fileHandle, uint64 offset);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Files_makeDirectory(const String pathName);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Files_openDirectory(DirectoryHandle *directoryHandle,
                           const String    pathName);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Files_closeDirectory(DirectoryHandle *directoryHandle);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Files_endOfDirectory(DirectoryHandle *directoryHandle);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Files_readDirectory(DirectoryHandle *directoryHandle,
                           String          fileName
                          );

/***********************************************************************\
* Name   : Files_getType
* Purpose: get file type
* Input  : fileName - file name
* Output : -
* Return : file type; see FileTypes
* Notes  : -
\***********************************************************************/

FileTypes Files_getType(String fileName);

/***********************************************************************\
* Name   : Files_exist
* Purpose: check if file exist
* Input  : fileName - file name
* Output : -
* Return : TRUE if file exists, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool Files_exist(String fileName);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Files_getInfo(String   fileName,
                     FileInfo *fileInfo
                    );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Files_setInfo(String   fileName,
                     FileInfo *fileInfo
                    );

#ifdef __cplusplus
  }
#endif

#endif /* __FILES__ */

/* end of file */
