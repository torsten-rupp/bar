/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/files.h,v $
* $Revision: 1.10 $
* $Author: torsten $
* Contents: Backup ARchiver files functions
* Systems: all
*
\***********************************************************************/

#ifndef __FILES__
#define __FILES__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#ifdef FILE_SEPARAPTOR_CHAR
  #define _FILES_PATHNAME_SEPARATOR_CHARS_STRING1(z) _FILES_PATHNAME_SEPARATOR_CHARS_STRING2(z) 
  #define _FILES_PATHNAME_SEPARATOR_CHARS_STRING2(z) #z

  #define FILES_PATHNAME_SEPARATOR_CHAR FILE_SEPARAPTOR_CHAR
  #define FILES_PATHNAME_SEPARATOR_CHARS _FILES_PATHNAME_SEPARATOR_CHARS_STRING1(FILE_SEPARAPTOR_CHAR)
#else
  #define FILES_PATHNAME_SEPARATOR_CHAR '/'
  #define FILES_PATHNAME_SEPARATOR_CHARS "/"
#endif

#define FILE_CAST_SIZE (sizeof(time_t)+sizeof(time_t))

/* file types */
typedef enum
{
  FILE_TYPE_NONE,

  FILE_TYPE_FILE,
  FILE_TYPE_DIRECTORY,
  FILE_TYPE_LINK,
  FILE_TYPE_HARDLINK,
  FILE_TYPE_SPECIAL,

  FILE_TYPE_UNKNOWN
} FileTypes;

/* file open mask */
#define FILE_OPEN_MASK_MODE  0x0000000F
#define FILE_OPEN_MASK_FLAGS 0xFFFF0000

/* file open modes */
typedef enum
{
  FILE_OPEN_CREATE = 0,
  FILE_OPEN_READ   = 1,
  FILE_OPEN_WRITE  = 2,
  FILE_OPEN_APPEND = 3
} FileModes;

/* additional file open flags */
#define FILE_OPEN_NO_CACHE (1 << 16)

/* special file types */
typedef enum
{
  FILE_SPECIAL_TYPE_CHARACTER_DEVICE,
  FILE_SPECIAL_TYPE_BLOCK_DEVICE,
  FILE_SPECIAL_TYPE_FIFO,
  FILE_SPECIAL_TYPE_SOCKET
} FileSpecialTypes;

/* permission flags */
#define FILE_PERMISSION_USER_READ    S_IRUSR
#define FILE_PERMISSION_USER_WRITE   S_IWUSR
#define FILE_PERMISSION_USER_EXECUTE S_IXUSR
#define FILE_PERMISSION_USER_ACCESS  S_IXUSR

#define FILE_PERMISSION_GROUP_READ    S_IRGRP
#define FILE_PERMISSION_GROUP_WRITE   S_IWGRP
#define FILE_PERMISSION_GROUP_EXECUTE S_IXGRP
#define FILE_PERMISSION_GROUP_ACCESS  S_IXGRP

#define FILE_PERMISSION_OTHER_READ    S_IROTH
#define FILE_PERMISSION_OTHER_WRITE   S_IWOTH
#define FILE_PERMISSION_OTHER_EXECUTE S_IXOTH
#define FILE_PERMISSION_OTHER_ACCESS  S_IXOTH

#define FILE_PERMISSION_READ    (FILE_PERMISSION_USER_READ|FILE_PERMISSION_GROUP_READ|FILE_PERMISSION_OTHER_READ)
#define FILE_PERMISSION_WRITE   (FILE_PERMISSION_USER_WRITE|FILE_PERMISSION_GROUP_WRITE|FILE_PERMISSION_OTHER_WRITE)
#define FILE_PERMISSION_EXECUTE (FILE_PERMISSION_USER_EXECUTE|FILE_PERMISSION_GROUP_EXECUTE|FILE_PERMISSION_OTHER_EXECUTE)
#define FILE_PERMISSION_MASK    (FILE_PERMISSION_READ|FILE_PERMISSION_WRITE|FILE_PERMISSION_EXECUTE)

/* default user, group ids, permission */
#define FILE_DEFAULT_USER_ID    0xFFFFFFFF
#define FILE_DEFAULT_GROUP_ID   0xFFFFFFFF
#define FILE_DEFAULT_PERMISSION 0xFFFFFFFF

/***************************** Datatypes *******************************/

/* file i/o handle */
typedef struct
{
  String name;
  ulong  mode;
  FILE   *file;
  uint64 index;
  uint64 size;
} FileHandle;

/* directory list handle */
typedef struct
{
  String        name;
  DIR           *dir;
  struct dirent *entry;
} DirectoryListHandle;

/* file permission */
typedef uint32 FilePermission;

/* file cast: change if file is modified in some way */
typedef byte FileCast[FILE_CAST_SIZE];

/* file info data */
typedef struct
{
  FileTypes        type;                     // file type; see FileTypes
  int64            size;                     // size of file [bytes]
  uint64           timeLastAccess;           // timestamp of last access
  uint64           timeModified;             // timestamp of last modification
  uint64           timeLastChanged;          // timestamp of last changed
  uint32           userId;                   // user id
  uint32           groupId;                  // group id
  FilePermission   permission;               // permission flags
  FileSpecialTypes specialType;              // special type; see FileSpecialTypes
  uint32           major,minor;              // special type major/minor number

  uint64           id;                       // unique id (e. g. inode number)
  uint             linkCount;                // number of hard links
  FileCast         cast;                     // cast value for checking if file was changed
} FileInfo;

/* file system info data */
typedef struct
{
  ulong  blockSize;                          // size of block [bytes]
  uint64 freeBytes;
  uint64 totalBytes;
  uint   maxFileNameLength;
} FileSystemInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define File_open(fileHandle,fileName,fileMode)                 __File_open(__FILE__,__LINE__,fileHandle,fileName,fileMode)
  #define File_openCString(fileHandle,fileName,fileMode)          __File_openCString(__FILE__,__LINE__,fileHandle,fileName,fileMode)
  #define File_openDescriptor(fileHandle,fileDescriptor,fileMode) __File_openDescriptor(__FILE__,__LINE__,fileHandle,fileDescriptor,fileMode)
  #define File_close(fileHandle)                                  __File_close(__FILE__,__LINE__,fileHandle)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : File_newFileName
* Purpose: create new file name variable
* Input  : -
* Output : -
* Return : file name variable (empty)
* Notes  : -
\***********************************************************************/

String File_newFileName(void);

/***********************************************************************\
* Name   : File_duplicateFileName
* Purpose: duplicate file name
* Input  : fromFileName - file name
* Output : -
* Return : new file name
* Notes  : -
\***********************************************************************/

String File_duplicateFileName(const String fromFileName);

/***********************************************************************\
* Name   : File_deleteFileName
* Purpose: delete file name variable
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void File_deleteFileName(String fileName);

/***********************************************************************\
* Name   : File_clearFileName
* Purpose: clear file name variable
* Input  : fileName - file name variable
* Output : -
* Return : file name variable
* Notes  : -
\***********************************************************************/

String File_clearFileName(String fileName);

/***********************************************************************\
* Name   : File_setFileName
* Purpose: set file name
* Input  : fileName - file name variable
*          name     - name to set
* Output : -
* Return : file name variable
* Notes  : -
\***********************************************************************/

String File_setFileName(String fileName, const String name);
String File_setFileNameCString(String fileName, const char *name);
String File_setFileNameChar(String fileName, char ch);

/***********************************************************************\
* Name   : File_fileNameAppend
* Purpose: append name to file name
* Input  : fileName - file name variable
*          name     - name to append
* Output : -
* Return : file name variable
* Notes  : -
\***********************************************************************/

String File_appendFileName(String fileName, const String name);
String File_appendFileNameCString(String fileName, const char *name);
String File_appendFileNameChar(String fileName, char ch);
String File_appendFileNameBuffer(String fileName, const char *buffer, ulong bufferLength);

/***********************************************************************\
* Name   : File_getFilePathName, File_getFilePathNameCString
* Purpose: get path of filename
* Input  : path     - path variable
*          fileName - file name
* Output : -
* Return : path variable
* Notes  : -
\***********************************************************************/

String File_getFilePathName(String path, const String fileName);
String File_getFilePathNameCString(String path, const char *fileName);

/***********************************************************************\
* Name   : File_getFileBaseName, File_getFileBaseNameCString
* Purpose: get basename of file
* Input  : baseName - basename variable
*          fileName - file name
* Output : -
* Return : basename variable
* Notes  : -
\***********************************************************************/

String File_getFileBaseName(String baseName, const String fileName);
String File_getFileBaseNameCString(String baseName, const char *fileName);

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
* Input  : fileName  - variable for temporary file name
*          pattern   - pattern with XXXXXX or NULL
*          directory - directory to create temporary file (can be NULL)
* Output : fileName - temporary file name
* Return : TRUE iff temporary file created, FALSE otherwise
* Notes  : -
\***********************************************************************/

Errors File_getTmpFileName(String fileName, const String pattern, const String directory);
Errors File_getTmpFileNameCString(String fileName, char const *pattern, const String directory);

/***********************************************************************\
* Name   : File_getTmpDirectoryName
* Purpose: create and get a temporary directory name
* Input  : directoryName - variable for temporary directory name
*          pattern       - pattern with XXXXXX or NULL
*          directory     - directory to create temporary file (can be
*                          NULL)
* Output : directoryName - temporary directory name
* Return : TRUE iff temporary directory created, FALSE otherwise
* Notes  : -
\***********************************************************************/

Errors File_getTmpDirectoryName(String directoryName, const String pattern, const String directory);
Errors File_getTmpDirectoryNameCString(String directoryName, char const *pattern, const String directory);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : File_open, File_openCString
* Purpose: open file
* Input  : fileHandle - file handle
*          fileName   - file name
*          fileMode   - file modes; see FILE_OPEN_*
* Output : fileHandle - file handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Errors File_open(FileHandle    *fileHandle,
                 const String  fileName,
                 FileModes     fileMode
                );
Errors File_openCString(FileHandle *fileHandle,
                        const char *fileName,
                        FileModes  fileMode
                       );
#else /* not NDEBUG */
Errors __File_open(const char   *__fileName__,
                   ulong        __lineNb__,
                   FileHandle   *fileHandle,
                   const String fileName,
                   FileModes    fileMode
                  );
Errors __File_openCString(const char *__fileName__,
                          ulong      __lineNb__,
                          FileHandle *fileHandle,
                          const char *fileName,
                          FileModes  fileMode
                         );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : File_openDescriptor
* Purpose: opeen file by descriptor
* Input  : fileHandle     - file handle
*          fileDescriptor - file descriptor
*          fileMode       - file open mode; see FILE_OPEN_*
* Output : fileHandle - file handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Errors File_openDescriptor(FileHandle *fileHandle,
                           int        fileDescriptor,
                           FileModes  fileMode
                          );
#else /* not NDEBUG */
Errors __File_openDescriptor(const char *__fileName__,
                             ulong      __lineNb__,
                             FileHandle *fileHandle,
                             int        fileDescriptor,
                             FileModes  fileMode
                            );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : File_close
* Purpose: close file
* Input  : fileHandle - file handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Errors File_close(FileHandle *fileHandle);
#else /* not NDEBUG */
Errors __File_close(const char *__fileName__, ulong __lineNb__, FileHandle *fileHandle);
#endif /* NDEBUG */

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
* Output : bytesRead - bytes read (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : if bytesRead is not given (NULL) reading less than
*          bufferLength bytes result in an i/o error
\***********************************************************************/

Errors File_read(FileHandle *fileHandle,
                 void       *buffer,
                 ulong      bufferLength,
                 ulong      *bytesRead
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
* Name   : File_readLine
* Purpose: read line from file (end of line: \n or \r\n)
* Input  : fileHandle - file handle
*          line       - string variable
* Output : line - read line
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_readLine(FileHandle *fileHandle,
                     String     line
                    );

/***********************************************************************\
* Name   : File_writeLine
* Purpose: write line into file
* Input  : fileHandle - file handle
*          line       - line to write
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_writeLine(FileHandle   *fileHandle,
                      const String line
                     );

/***********************************************************************\
* Name   : File_printLine
* Purpose: print line into file
* Input  : fileHandle - file handle
*          format     - format (like printf)
*          ...        - optional arguments
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_printLine(FileHandle *fileHandle,
                      const char *format,
                      ...
                     );

/***********************************************************************\
* Name   : File_getSize
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

Errors File_seek(FileHandle *fileHandle,
                 uint64     offset
                );

/***********************************************************************\
* Name   : File_truncate
* Purpose: truncate a file
* Input  : fileHandle - file handle
*          size       - size
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_truncate(FileHandle *fileHandle,
                     uint64     size
                    );

/***********************************************************************\
* Name   : File_dropCaches
* Purpose: drop any data in file system cache when possible
* Input  : fileHandle - file handle
*          offset     - offset (0..n-1)
*          length     - length of data to drop or 0LL for all data of
*          syncFlag   - TRUE to sync data on disk
*                       file
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_dropCaches(FileHandle *fileHandle,
                       uint64     offset,
                       uint64     length,
                       bool       syncFlag
                      );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : File_openDirectoryList
* Purpose: open directory for reading
* Input  : directoryListHandle - directory list handle
*          pathName            - path name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_openDirectoryList(DirectoryListHandle *directoryListHandle,
                              const String        pathName
                             );
Errors File_openDirectoryListCString(DirectoryListHandle *directoryListHandle,
                                     const char          *pathName
                                    );

/***********************************************************************\
* Name   : File_closeDirectoryList
* Purpose: close directory .ist
* Input  : directoryListHandle - directory list handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void File_closeDirectoryList(DirectoryListHandle *directoryListHandle);

/***********************************************************************\
* Name   : File_endOfDirectoryList
* Purpose: check if end of directory list reached
* Input  : directoryHandle - directory handle
* Output : -
* Return : TRUE if not more diretory entries to read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool File_endOfDirectoryList(DirectoryListHandle *directoryListHandle);

/***********************************************************************\
* Name   : File_readDirectoryList
* Purpose: read next directory list entry
* Input  : directoryHandleList - directory list handle
*          fileName            - file name variable
* Output : fileName - next file name (including path)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_readDirectoryList(DirectoryListHandle *directoryListHandle,
                              String              fileName
                             );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : File_userNameToUserId
* Purpose: convert user name to user id
* Input  : name - user name
* Output : -
* Return : user id or FILE_DEFAULT_USER_ID if user not found
* Notes  : -
\***********************************************************************/

uint32 File_userNameToUserId(const char *name);

/***********************************************************************\
* Name   : File_groupNameToGroupId
* Purpose: convert group name to group id
* Input  : name - group name
* Output : -
* Return : user id or FILE_DEFAULT_GROUP_ID if group not found
* Notes  : -
\***********************************************************************/

uint32 File_groupNameToGroupId(const char *name);

/***********************************************************************\
* Name   : File_getType
* Purpose: get file type
* Input  : fileName - file name
* Output : -
* Return : file type; see FileTypes
* Notes  : -
\***********************************************************************/

FileTypes File_getType(const String fileName);

/***********************************************************************\
* Name   : File_getData
* Purpose: read file content into buffer
* Input  : fileName - file name
* Output : data - data
*          size - size of data
* Return : ERROR_NONE or error code
* Notes  : data must be freed with free() after usage!
\***********************************************************************/

Errors File_getData(const String fileName,
                    void         **data,
                    ulong        *size
                   );
Errors File_getDataCString(const char *fileName,
                           void       **data,
                           ulong      *size
                          );

/***********************************************************************\
* Name   : File_delete
* Purpose: delete file/directory/link
* Input  : fileName - file name
* Output : -
* Return : TRUE if file/directory/link deleted, FALSE otherwise
* Notes  : -
\***********************************************************************/

Errors File_delete(const String fileName, bool recursiveFlag);

/***********************************************************************\
* Name   : File_rename
* Purpose: rename file/directory/link
* Input  : oldFileName - old file name
*          newFileName - new file name
* Output : -
* Return : TRUE if file/directory/link renamed, FALSE otherwise
* Notes  : if files are not on the same logical device the file is
*          copied
\***********************************************************************/

Errors File_rename(const String oldFileName,
                   const String newFileName
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

Errors File_copy(const String sourceFileName,
                 const String destinationFileName
                );

/***********************************************************************\
* Name   : File_exists, File_existsCString
* Purpose: check if file exists
* Input  : fileName - file name
* Output : -
* Return : TRUE if file/directory exists, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool File_exists(const String fileName);
bool File_existsCString(const char *fileName);

/***********************************************************************\
* Name   : File_isFile, File_isFileCString
* Purpose: check if file
* Input  : fileName - file name
* Output : -
* Return : TRUE if file, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool File_isFile(const String fileName);
bool File_isFileCString(const char *fileName);

/***********************************************************************\
* Name   : File_isDirectory, File_isDirectoryCString
* Purpose: check if file directory
* Input  : fileName - file name
* Output : -
* Return : TRUE if directory, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool File_isDirectory(const String fileName);
bool File_isDirectoryCString(const char *fileName);

/***********************************************************************\
* Name   : File_isReadable, File_isReadableCString
* Purpose: check if file and is readable
* Input  : fileName - file name
* Output : -
* Return : TRUE if file/directory exists and is readable, FALSE
*          otherweise
* Notes  : -
\***********************************************************************/

bool File_isReadable(const String fileName);
bool File_isReadableCString(const char *fileName);

/***********************************************************************\
* Name   : File_isWriteable, File_isWriteableCString
* Purpose: check if file or directory and is writable
* Input  : fileName - file name
* Output : -
* Return : TRUE if file/directory exists and is writable, FALSE
*          otherweise
* Notes  : -
\***********************************************************************/

bool File_isWriteable(const String fileName);
bool File_isWriteableCString(const char *fileName);

/***********************************************************************\
* Name   : File_getInfo
* Purpose: get file info
* Input  : fileInfo - file info variable
*          fileName - file name
* Output : fileInfo - file info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_getFileInfo(FileInfo     *fileInfo,
                        const String fileName
                       );

/***********************************************************************\
* Name   : File_getFileTimeModified
* Purpose: get file modified time
* Input  : fileName - file name
* Output : -
* Return : time modified or 0
* Notes  : -
\***********************************************************************/

uint64 File_getFileTimeModified(const String fileName);

/***********************************************************************\
* Name   : File_setPermission
* Purpose: set file permission
* Input  : fileName   - file name
*          permission - file permission
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_setPermission(const String   fileName,
                          FilePermission permission
                         );

/***********************************************************************\
* Name   : File_setOwner
* Purpose: set file owner
* Input  : fileName - file name
*          userId   - user id or FILE_DEFAULT_USER_ID
*          groupId  - group id or FILE_DEFAULT_GROUP_ID
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_setOwner(const String fileName,
                     uint32       userId,
                     uint32       groupId
                    );

/***********************************************************************\
* Name   : File_setFileInfo
* Purpose: set file info (time, owner, permission)
* Input  : fileName - file name
*          fileInfo - file info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_setFileInfo(const String fileName,
                        FileInfo     *fileInfo
                       );

/***********************************************************************\
* Name   : File_makeDirectory, File_makeDirectoryCString
* Purpose: create directory including intermediate directories
* Input  : pathName   - path name
*          userId     - user id or FILE_DEFAULT_USER_ID
*          groupId    - group id or FILE_DEFAULT_GROUP_ID
*          permission - permission or FILE_DEFAULT_PERMISSION
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_makeDirectory(const String   pathName,
                          uint32         userId,
                          uint32         groupId,
                          FilePermission permission
                         );
Errors File_makeDirectoryCString(const char     *pathName,
                                 uint32         userId,
                                 uint32         groupId,
                                 FilePermission permission
                                );

/***********************************************************************\
* Name   : File_readLink
* Purpose: read link
* Input  : linkName - link name
* Output : fileName - file name link references
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_readLink(String       fileName,
                     const String linkName
                    );

/***********************************************************************\
* Name   : File_makeLink
* Purpose: create (symbolic) link linkName -> fileName
* Input  : linkName - link name
*          fileName - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_makeLink(const String linkName,
                     const String fileName
                    );

/***********************************************************************\
* Name   : File_makeHardLink
* Purpose: create hard link linkName -> fileName
* Input  : linkName - link name
*          fileName - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_makeHardLink(const String linkName,
                         const String fileName
                        );

/***********************************************************************\
* Name   : File_makeSpecial
* Purpose: make special node
* Input  : name  - name
*          type  - special type
*          major - major device number
*          minor - minor device number
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_makeSpecial(const String     name,
                        FileSpecialTypes type,
                        ulong            major,
                        ulong            minor
                       );

/***********************************************************************\
* Name   : File_getFileSystemInfo
* Purpose: get file system info
* Input  : pathName - path name
* Output : fileSystemInfo - file system info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_getFileSystemInfo(FileSystemInfo *fileSystemInfo,
                              const          String pathName
                             );

#ifndef NDEBUG
/***********************************************************************\
* Name   : File_debugInit
* Purpose: init file debug functions
* Input  : -
* Output : -
* Return : -
* Notes  : called automatically
\***********************************************************************/

void File_debugInit(void);

/***********************************************************************\
* Name   : File_debugDone
* Purpose: done file debug functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void File_debugDone(void);

/***********************************************************************\
* Name   : File_debugDumpInfo, File_debugPrintInfo
* Purpose: string file function: output open files
* Input  : handle - output channel
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void File_debugDumpInfo(FILE *handle);
void File_debugPrintInfo(void);

/***********************************************************************\
* Name   : File_debugPrintStatistics
* Purpose: file debug function: output file statistics
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void File_debugPrintStatistics(void);

/***********************************************************************\
* Name   : File_debugPrintCurrentStackTrace
* Purpose: print C stack trace
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void File_debugPrintCurrentStackTrace(void);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __FILES__ */

/* end of file */
