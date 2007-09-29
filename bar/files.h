/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/files.h,v $
* $Revision: 1.14 $
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
* Name   : File_setFileName
* Purpose: set name to file name
* Input  : fileName - file name
*          name     - name to set
* Output : -
* Return : file name
* Notes  : -
\***********************************************************************/

String File_setFileName(String fileName, const String name);
String File_setFileNameCString(String fileName, const char *name);
String File_setFileNameChar(String fileName, char ch);

/***********************************************************************\
* Name   : File_fileNameAppend
* Purpose: append name to file name
* Input  : fileName - file name
*          name     - name to append
* Output : -
* Return : file name
* Notes  : -
\***********************************************************************/

String File_appendFileName(String fileName, const String name);
String File_appendFileNameCString(String fileName, const char *name);
String File_appendFileNameChar(String fileName, char ch);

/***********************************************************************\
* Name   : File_getFilePathName
* Purpose: get path of filename
* Input  : fileName - file name
*          path     - path variable
* Output : -
* Return : path
* Notes  : -
\***********************************************************************/

String File_getFilePathName(const String fileName, String path);

/***********************************************************************\
* Name   : File_getFileBaseName
* Purpose: get basename of file
* Input  : fileName - file name
*          baseName - basename variable
* Output : -
* Return : basename
* Notes  : -
\***********************************************************************/

String File_getFileBaseName(const String fileName, String baseName);

/***********************************************************************\
* Name   : File_splitFileName
* Purpose: split file name into path name and base name
* Input  : fileName - file name
* Output : pathName - path name (allocated string)
*          baseName - base name (allocated string)
* Return : -
* Notes  : -
\***********************************************************************/

void File_splitFileName(const String fileName, String *pathName, String *baseName);

/***********************************************************************\
* Name   : File_initSplitFileName, File_doneSplitFileName
* Purpose: init/done file name splitter
* Input  : stringTokenizer - string tokenizer (see strings.h)
*          fileName        - file name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void File_initSplitFileName(StringTokenizer *stringTokenizer, const String fileName);
void File_doneSplitFileName(StringTokenizer *stringTokenizer);

/***********************************************************************\
* Name   : File_getNextFileName
* Purpose: get next part of file name 
* Input  : stringTokenizer - string tokenizer (see strings.h)
* Output : name - next name (internal reference; do not delete!)
* Return : TRUE if file name found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool File_getNextSplitFileName(StringTokenizer *stringTokenizer, String *const name);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : File_getTmpFileName
* Purpose: create and get a temporary file name
* Input  : directory - directory to create temporary file
*          fileName  - variable for temporary file name
* Output : -
* Return : TRUE iff temporary file created, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool File_getTmpFileName(const char *directory, String fileName);

/***********************************************************************\
* Name   : File_getTmpDirectoryName
* Purpose: create and get a temporary directory name
* Input  : directory - directory to create temporary file
*          fileName  - variable for temporary file name
* Output : -
* Return : TRUE iff temporary directory created, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool File_getTmpDirectoryName(const char *directory, String fileName);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : File_open
* Purpose: create new file
* Input  : fileHandle - file handle
*          fileName   - file name
* Output : fileHandle - file handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_open(FileHandle    *fileHandle,
                  const String  fileName,
                  FileOpenModes fileOpenMode
                 );

/***********************************************************************\
* Name   : File_close
* Purpose: close file
* Input  : fileHandle - file handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_close(FileHandle *fileHandle);

/***********************************************************************\
* Name   : File_eof
* Purpose: check if end-of-file
* Input  : fileHandle - file handle
* Output : -
* Return : TRUE if end-of-file, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool File_eof(FileHandle *fileHandle);

/***********************************************************************\
* Name   : File_read
* Purpose: read data from file
* Input  : fileHandle   - file handle
*          buffer       - buffer for data to read
*          bufferLength - length of data to read
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_read(FileHandle *fileHandle,
                 void       *buffer,
                 ulong      bufferLength,
                 ulong      *readBytes
                );

/***********************************************************************\
* Name   : File_write
* Purpose: write data into file
* Input  : fileHandle   - file handle
*          buffer       - buffer for data to write
*          bufferLength - length of data to write
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_write(FileHandle *fileHandle,
                  const void *buffer,
                  ulong      bufferLength
                 );

/***********************************************************************\
* Name   : File_size
* Purpose: get file size
* Input  : fileHandle - file handle
* Output : -
* Return : size of file (in bytes)
* Notes  : -
\***********************************************************************/

uint64 File_getSize(FileHandle *fileHandle);

/***********************************************************************\
* Name   : File_tell
* Purpose: get current position in file
* Input  : fileHandle - file handle
* Output : offset - offset (0..n-1)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_tell(FileHandle *fileHandle,
                 uint64     *offset
                );

/***********************************************************************\
* Name   : File_seek
* Purpose: seek in file
* Input  : fileHandle - file handle
*          offset     - offset (0..n-1)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_seek(FileHandle *fileHandle, uint64 offset);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : File_makeDirectory
* Purpose: create directory including intermedate directories
* Input  : pathName - path name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_makeDirectory(const String pathName);

/***********************************************************************\
* Name   : File_openDirectory
* Purpose: open directory for reading
* Input  : directoryHandle - directory handle
*          pathName        - path name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_openDirectory(DirectoryHandle *directoryHandle,
                          const String    pathName
                         );

/***********************************************************************\
* Name   : File_closeDirectory
* Purpose: close directory
* Input  : directoryHandle - directory handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void File_closeDirectory(DirectoryHandle *directoryHandle);

/***********************************************************************\
* Name   : File_endOfDirectory
* Purpose: check if end of directory reached
* Input  : directoryHandle - directory handle
* Output : -
* Return : TRUE if not more diretory entries to read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool File_endOfDirectory(DirectoryHandle *directoryHandle);

/***********************************************************************\
* Name   : File_readDirectory
* Purpose: read next directory entry
* Input  : directoryHandle - directory handle
*          fileName        - file name variable
* Output : fileName - next file name
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_readDirectory(DirectoryHandle *directoryHandle,
                          String          fileName
                         );

/***********************************************************************\
* Name   : File_getType
* Purpose: get file type
* Input  : fileName - file name
* Output : -
* Return : file type; see FileTypes
* Notes  : -
\***********************************************************************/

FileTypes File_getType(String fileName);

/***********************************************************************\
* Name   : File_delete
* Purpose: delete file/directory/link
* Input  : fileName - file name
* Output : -
* Return : TRUE if file/directory/link deleted, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool File_delete(String fileName);

/***********************************************************************\
* Name   : File_rename
* Purpose: rename file/directory/link
* Input  : oldFileName - old file name
*          newFileName - new file name
* Output : -
* Return : TRUE if file/directory/link renamed, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool File_rename(String oldFileName,
                 String newFileName
                );

/***********************************************************************\
* Name   : File_copy
* Purpose: copy files
* Input  : sourceFileName      - source file name
*          destinationFileName - destination file name
* Output : -
* Return : TRUE if file/directory/link copied, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool File_copy(String sourceFileName,
               String destinationFileName
              );

/***********************************************************************\
* Name   : File_exists
* Purpose: check if file exists
* Input  : fileName - file name
* Output : -
* Return : TRUE if file exists, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool File_exists(String fileName);

/***********************************************************************\
* Name   : File_getInfo
* Purpose: get file info
* Input  : fileName - file name
* Output : fileInfo - file info
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors File_getFileInfo(String   fileName,
                        FileInfo *fileInfo
                       );

/***********************************************************************\
* Name   : File_setInfo
* Purpose: set file info
* Input  : fileName - file name
*          fileInfo - file info
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors File_setFileInfo(String   fileName,
                        FileInfo *fileInfo
                       );

/***********************************************************************\
* Name   : File_readLink
* Purpose: read link
* Input  : linkName - link name
* Output : fileName - file name link references
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors File_readLink(String linkName,
                     String fileName
                    );

/***********************************************************************\
* Name   : File_link
* Purpose: create link
* Input  : linkName - link name
*          fileName - file name
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors File_link(String linkName,
                 String fileName
                );

#ifdef __cplusplus
  }
#endif

#endif /* __FILES__ */

/* end of file */
