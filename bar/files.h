/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/files.h,v $
* $Revision: 1.12 $
* $Author: torsten $
* Contents: Backup ARchiver files functions
* Systems: all
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
#include "compress.h"
#include "crypt.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define FILES_PATHNAME_SEPARATOR_CHAR '/'
#define FILES_PATHNAME_SEPARATOR_CHARS "/"

typedef enum
{
  FILETYPE_NONE,

  FILETYPE_FILE,
  FILETYPE_DIRECTORY,
  FILETYPE_LINK,

  FILETYPE_UNKNOWN
} FileTypes;

typedef enum
{
  FILE_OPENMODE_CREATE,
  FILE_OPENMODE_READ,
  FILE_OPENMODE_WRITE,
} FileOpenModes;

/***************************** Datatypes *******************************/

/* file i/o handle */
typedef struct
{
  int    handle;
  uint64 index;
  uint64 size;
} FileHandle;

/* directory read handle */
typedef struct
{
  String        name;
  DIR           *handle;
  struct dirent *entry;
} DirectoryHandle;

/* file info data */
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
* Name   : Files_setFileName
* Purpose: set name to file name
* Input  : fileName - file name
*          name     - name to set
* Output : -
* Return : file name
* Notes  : -
\***********************************************************************/

String Files_setFileName(String fileName, String name);
String Files_setFileNameCString(String fileName, const char *name);
String Files_setFileNameChar(String fileName, char ch);

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
String Files_appendFileNameCString(String fileName, const char *name);
String Files_appendFileNameChar(String fileName, char ch);

/***********************************************************************\
* Name   : Files_getFilePathName
* Purpose: get path of filename
* Input  : fileName - file name
*          path     - path variable
* Output : -
* Return : path
* Notes  : -
\***********************************************************************/

String Files_getFilePathName(String fileName, String path);

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
* Name   : Files_splitFileName
* Purpose: split file name into path name and base name
* Input  : fileName - file name
* Output : pathName - path name (allocated string)
*          baseName - base name (allocated string)
* Return : -
* Notes  : -
\***********************************************************************/

void Files_splitFileName(String fileName, String *pathName, String *baseName);

/***********************************************************************\
* Name   : Files_initSplitFileName, Files_doneSplitFileName
* Purpose: init/done file name splitter
* Input  : stringTokenizer - string tokenizer (see strings.h)
*          fileName        - file name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Files_initSplitFileName(StringTokenizer *stringTokenizer, String fileName);
void Files_doneSplitFileName(StringTokenizer *stringTokenizer);

/***********************************************************************\
* Name   : Files_getNextFileName
* Purpose: get next part of file name 
* Input  : stringTokenizer - string tokenizer (see strings.h)
* Output : name - next name (internal reference; do not delete!)
* Return : TRUE if file name found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Files_getNextSplitFileName(StringTokenizer *stringTokenizer, String *const name);

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

uint64 Files_getSize(FileHandle *fileHandle);

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
* Name   : Files_makeDirectory
* Purpose: create directory including intermedate directories
* Input  : pathName - path name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Files_makeDirectory(const String pathName);

/***********************************************************************\
* Name   : Files_openDirectory
* Purpose: open directory for reading
* Input  : directoryHandle - directory handle
*          pathName        - path name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Files_openDirectory(DirectoryHandle *directoryHandle,
                           const String    pathName);

/***********************************************************************\
* Name   : Files_closeDirectory
* Purpose: close directory
* Input  : directoryHandle - directory handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Files_closeDirectory(DirectoryHandle *directoryHandle);

/***********************************************************************\
* Name   : Files_endOfDirectory
* Purpose: check if end of directory reached
* Input  : directoryHandle - directory handle
* Output : -
* Return : TRUE if not more diretory entries to read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Files_endOfDirectory(DirectoryHandle *directoryHandle);

/***********************************************************************\
* Name   : Files_readDirectory
* Purpose: read next directory entry
* Input  : directoryHandle - directory handle
*          fileName        - file name variable
* Output : fileName - next file name
* Return : ERROR_NONE or error code
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
* Name   : Files_exists
* Purpose: check if file exists
* Input  : fileName - file name
* Output : -
* Return : TRUE if file exists, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool Files_exists(String fileName);

/***********************************************************************\
* Name   : Files_getInfo
* Purpose: get file info
* Input  : fileName - file name
* Output : fileInfo - file info
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Files_getFileInfo(String   fileName,
                         FileInfo *fileInfo
                        );

/***********************************************************************\
* Name   : Files_setInfo
* Purpose: set file info
* Input  : fileName - file name
*          fileInfo - file info
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Files_setFileInfo(String   fileName,
                         FileInfo *fileInfo
                        );

/***********************************************************************\
* Name   : Files_readLink
* Purpose: read link
* Input  : linkName - link name
* Output : fileName - file name link references
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Files_readLink(String linkName,
                      String fileName
                     );

/***********************************************************************\
* Name   : Files_link
* Purpose: create link
* Input  : linkName - link name
*          fileName - file name
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Files_link(String linkName,
                  String fileName
                 );

#ifdef __cplusplus
  }
#endif

#endif /* __FILES__ */

/* end of file */
