/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver files functions
* Systems: all
*
\***********************************************************************/

#ifndef __FILES__
#define __FILES__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
  #include <linux/fs.h>
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

#include "global.h"
#include "strings.h"
#include "lists.h"
#include "stringlists.h"
#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// max. length of a path
#define FILE_MAX_PATH_MAX_LENGTH PATH_MAX

// temporary directory
#define FILE_TMP_DIRECTORY File_getSystemTmpDirectory()

#if defined(FILE_SEPARATOR_CHAR) && defined(FILE_SEPARATOR_STRING)
  #define FILES_PATHNAME_SEPARATOR_CHAR FILE_SEPARATOR_CHAR
  #define FILES_PATHNAME_SEPARATOR_CHARS FILE_SEPARATOR_STRING
#else
  #define FILES_PATHNAME_SEPARATOR_CHAR '/'
  #define FILES_PATHNAME_SEPARATOR_CHARS "/"
#endif

#define FILE_CAST_SIZE (sizeof(time_t)+sizeof(time_t))

// file types
// work-around Windows:
#ifdef FILE_TYPE_UNKNOWN
  #undef FILE_TYPE_UNKNOWN
#endif
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

// file open mask
#define FILE_OPEN_MASK_MODE  0x0000000F
#define FILE_OPEN_MASK_FLAGS 0xFFFF0000

// file open modes
typedef enum
{
  FILE_OPEN_CREATE = 0,
  FILE_OPEN_READ   = 1,
  FILE_OPEN_WRITE  = 2,
  FILE_OPEN_APPEND = 3
} FileModes;

// additional file open flags
#define FILE_STREAM        (1 << 16)
#define FILE_OPEN_NO_CACHE (1 << 17)
#define FILE_OPEN_NO_ATIME (1 << 18)

// special file types
typedef enum
{
  FILE_SPECIAL_TYPE_CHARACTER_DEVICE,
  FILE_SPECIAL_TYPE_BLOCK_DEVICE,
  FILE_SPECIAL_TYPE_FIFO,
  FILE_SPECIAL_TYPE_SOCKET,
  FILE_SPECIAL_TYPE_OTHER
} FileSpecialTypes;

// permission flags
#ifdef HAVE_S_IRUSR
  #define FILE_PERMISSION_USER_READ     S_IRUSR
#else
  #define FILE_PERMISSION_USER_READ     0
#endif
#ifdef HAVE_S_IWUSR
  #define FILE_PERMISSION_USER_WRITE    S_IWUSR
#else
  #define FILE_PERMISSION_USER_WRITE    0
#endif
#ifdef HAVE_S_IXUSR
  #define FILE_PERMISSION_USER_EXECUTE  S_IXUSR
#else
  #define FILE_PERMISSION_USER_EXECUTE  0
#endif
#ifdef HAVE_S_IXUSR
  #define FILE_PERMISSION_USER_ACCESS   S_IXUSR
#else
  #define FILE_PERMISSION_USER_ACCESS   0
#endif

#ifdef HAVE_S_IRGRP
  #define FILE_PERMISSION_GROUP_READ    S_IRGRP
#else
  #define FILE_PERMISSION_GROUP_READ    0
#endif
#ifdef HAVE_S_IWGRP
  #define FILE_PERMISSION_GROUP_WRITE   S_IWGRP
#else
  #define FILE_PERMISSION_GROUP_WRITE   0
#endif
#ifdef HAVE_S_IXGRP
  #define FILE_PERMISSION_GROUP_EXECUTE S_IXGRP
#else
  #define FILE_PERMISSION_GROUP_EXECUTE 0
#endif
#ifdef HAVE_S_IXGRP
  #define FILE_PERMISSION_GROUP_ACCESS  S_IXGRP
#else
  #define FILE_PERMISSION_GROUP_ACCESS  0
#endif

#ifdef HAVE_S_IROTH
  #define FILE_PERMISSION_OTHER_READ    S_IROTH
#else
  #define FILE_PERMISSION_OTHER_READ    0
#endif
#ifdef HAVE_S_IWOTH
  #define FILE_PERMISSION_OTHER_WRITE   S_IWOTH
#else
  #define FILE_PERMISSION_OTHER_WRITE   0
#endif
#ifdef HAVE_S_IXOTH
  #define FILE_PERMISSION_OTHER_EXECUTE S_IXOTH
#else
  #define FILE_PERMISSION_OTHER_EXECUTE 0
#endif
#ifdef HAVE_S_IXOTH
  #define FILE_PERMISSION_OTHER_ACCESS  S_IXOTH
#else
  #define FILE_PERMISSION_OTHER_ACCESS  0
#endif


#ifdef HAVE_S_ISUID
  #define FILE_PERMISSION_USER_SET_ID   S_ISUID
#else
  #define FILE_PERMISSION_USER_SET_ID   0
#endif
#ifdef HAVE_S_ISGID
  #define FILE_PERMISSION_GROUP_SET_ID  S_ISGID
#else
  #define FILE_PERMISSION_GROUP_SET_ID  0
#endif
#ifdef HAVE_S_ISVTX
  #define FILE_PERMISSION_STICKY_BIT    S_ISVTX
#else
  #define FILE_PERMISSION_STICKY_BIT    0
#endif

#define FILE_PERMISSION_NONE      0
#define FILE_PERMISSION_READ      (FILE_PERMISSION_USER_READ|FILE_PERMISSION_GROUP_READ|FILE_PERMISSION_OTHER_READ)
#define FILE_PERMISSION_WRITE     (FILE_PERMISSION_USER_WRITE|FILE_PERMISSION_GROUP_WRITE|FILE_PERMISSION_OTHER_WRITE)
#define FILE_PERMISSION_EXECUTE   (FILE_PERMISSION_USER_EXECUTE|FILE_PERMISSION_GROUP_EXECUTE|FILE_PERMISSION_OTHER_EXECUTE)
#define FILE_PERMISSION_MASK      (FILE_PERMISSION_READ|FILE_PERMISSION_WRITE|FILE_PERMISSION_EXECUTE)
#define FILE_PERMISSION_ALL       (FILE_PERMISSION_READ|FILE_PERMISSION_WRITE|FILE_PERMISSION_EXECUTE|FILE_PERMISSION_USER_SET_ID|FILE_PERMISSION_GROUP_SET_ID|FILE_PERMISSION_STICKY_BIT)

// own user, group ids, own default permission
#define FILE_OWN_USER_ID        0
#define FILE_OWN_GROUP_ID       0

// default user, group ids, permission
#define FILE_DEFAULT_USER_ID    0xFFFFFFFF
#define FILE_DEFAULT_GROUP_ID   0xFFFFFFFF
#define FILE_DEFAULT_PERMISSION 0xFFFFFFFF

// attributes
#define FILE_ATTRIBUTE_NONE 0LL
#ifdef HAVE_FS_COMPR_FL
  #define FILE_ATTRIBUTE_COMPRESS    FS_COMPR_FL
#else
  #define FILE_ATTRIBUTE_COMPRESS    0LL
#endif
#ifdef HAVE_FS_NOCOMP_FL
  #define FILE_ATTRIBUTE_NO_COMPRESS FS_NOCOMP_FL
#else
  #define FILE_ATTRIBUTE_NO_COMPRESS 0LL
#endif
#ifdef HAVE_FS_IMMUTABLE_FL
  #define FILE_ATTRIBUTE_IMMUTABLE   FS_IMMUTABLE_FL
#else
  #define FILE_ATTRIBUTE_IMMUTABLE   0LL
#endif
#ifdef HAVE_FS_APPEND_FL
  #define FILE_ATTRIBUTE_APPEND      FS_APPEND_FL
#else
  #define FILE_ATTRIBUTE_APPEND      0LL
#endif
#ifdef HAVE_FS_NODUMP_FL
  #define FILE_ATTRIBUTE_NO_DUMP     FS_NODUMP_FL
#else
  #define FILE_ATTRIBUTE_NO_DUMP     0LL
#endif

/***************************** Datatypes *******************************/

// file i/o handle
typedef struct
{
  String     name;
  FileModes  mode;
  FILE       *file;
  uint64     index;
  uint64     size;
  #ifndef NDEBUG
    bool deleteOnCloseFlag;
  #endif /* not NDEBUG */
  StringList lineBufferList;
  #ifndef HAVE_O_NOATIME
    int             handle;
    struct timespec atime;
  #endif /* not HAVE_O_NOATIME */
} FileHandle;

// root list handle
typedef struct
{
  StringList fileSystemNames;
  FILE       *mounts;
  char       line[1024];
  bool       parseFlag;
  char       name[256];
} RootListHandle;

// directory list handle
typedef struct
{
  String        name;
  DIR           *dir;
  struct dirent *entry;
  #if defined(HAVE_FDOPENDIR) && defined(HAVE_O_DIRECTORY)
    #ifndef HAVE_O_NOATIME
      int             handle;
      struct timespec atime;
    #endif /* not HAVE_O_NOATIME */
  #endif /* HAVE_FDOPENDIR && HAVE_O_DIRECTORY */
} DirectoryListHandle;

// file permission
typedef uint32 FilePermission;

// file attributes
typedef uint64 FileAttributes;

// file extended attributes
typedef struct FileExtendedAttributeNode
{
  LIST_NODE_HEADER(struct FileExtendedAttributeNode);

  String name;
  void   *data;
  uint   dataLength;
} FileExtendedAttributeNode;

typedef struct
{
  LIST_HEADER(FileExtendedAttributeNode);
} FileExtendedAttributeList;

// file cast: change if file is modified in some way
//typedef byte FileCast[FILE_CAST_SIZE];
typedef struct
{
  time_t mtime;
  time_t ctime;
} FileCast;

// file info data
typedef struct
{
  FileTypes        type;              // file type; see FileTypes
  uint64           size;              // size of file [bytes]
  uint64           timeLastAccess;    // timestamp of last access
  uint64           timeModified;      // timestamp of last modification (changed content)
  uint64           timeLastChanged;   // timestamp of last changed (changed meta-data or content)
  uint32           userId;            // user id
  uint32           groupId;           // group id
  FilePermission   permission;        // permission flags
  FileSpecialTypes specialType;       // special type; see FileSpecialTypes
  uint32           major,minor;       // special type major/minor number
  FileAttributes   attributes;        // attributes

  uint64           id;                // unique id (e. g. inode number)
  uint             linkCount;         // number of hard links
  FileCast         cast;              // cast value for checking if file was changed
} FileInfo;

// file system info data
typedef struct
{
  ulong  blockSize;                   // size of block [bytes]
  uint64 freeBytes;
  uint64 totalBytes;
  uint   maxFileNameLength;
} FileSystemInfo;

#ifndef NDEBUG
/***********************************************************************\
* Name   : FileDumpInfoFunction
* Purpose: string dump info call-back function
* Input  : fileHandle - file handle
*          fileName   - file name
*          lineNb     - line number
*          n          - string number [0..count-1]
*          count      - total string count
*          userData   - user data
* Output : -
* Return : TRUE for continue, FALSE for abort
* Notes  : -
\***********************************************************************/

typedef bool(*FileDumpInfoFunction)(const FileHandle *fileHandle,
                                    const char       *fileName,
                                    ulong            lineNb,
                                    ulong            n,
                                    ulong            count,
                                    void             *userData
                                   );
#endif /* not NDEBUG */

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define File_getTmpFile(...)             __File_getTmpFile(__FILE__,__LINE__, ## __VA_ARGS__)
  #define File_getTmpFileCString(...)      __File_getTmpFileCString(__FILE__,__LINE__, ## __VA_ARGS__)
  #define File_open(...)                   __File_open(__FILE__,__LINE__, ## __VA_ARGS__)
  #define File_openCString(...)            __File_openCString(__FILE__,__LINE__, ## __VA_ARGS__)
  #define File_openDescriptor(...)         __File_openDescriptor(__FILE__,__LINE__, ## __VA_ARGS__)
  #define File_close(...)                  __File_close(__FILE__,__LINE__, ## __VA_ARGS__)
  #define File_initExtendedAttributes(...) __File_initExtendedAttributes(__FILE__,__LINE__, ## __VA_ARGS__)
  #define File_doneExtendedAttributes(...) __File_doneExtendedAttributes(__FILE__,__LINE__, ## __VA_ARGS__)
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

String File_duplicateFileName(ConstString fromFileName);

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
* Name   : File_setFileName, File_setFileNameCString,
*          File_setFileNameChar
* Purpose: set file name
* Input  : fileName - file name variable
*          name     - name to set
* Output : -
* Return : file name variable
* Notes  : -
\***********************************************************************/

String File_setFileName(String fileName, ConstString name);
String File_setFileNameCString(String fileName, const char *name);
String File_setFileNameChar(String fileName, char ch);

/***********************************************************************\
* Name   : File_appendFileName, File_appendFileNameCString,
*          File_appendFileNameChar, File_appendFileNameBuffer
* Purpose: append name to file name
* Input  : fileName - file name variable
*          name     - name to append
* Output : -
* Return : file name variable
* Notes  : -
\***********************************************************************/

String File_appendFileName(String fileName, ConstString name);
String File_appendFileNameCString(String fileName, const char *name);
String File_appendFileNameChar(String fileName, char ch);
String File_appendFileNameBuffer(String fileName, const char *buffer, ulong bufferLength);

/***********************************************************************\
* Name   : File_getDirectoryName, File_getDirectoryNameCString
* Purpose: get path of filename
* Input  : path     - path variable
*          fileName - file name
* Output : -
* Return : path variable
* Notes  : -
\***********************************************************************/

String File_getDirectoryName(String path, ConstString fileName);
String File_getDirectoryNameCString(String path, const char *fileName);

/***********************************************************************\
* Name   : File_getBaseName, File_getBaseNameCString
* Purpose: get basename of file (name without directory)
* Input  : baseName - basename variable
*          fileName - file name
* Output : -
* Return : basename variable
* Notes  : -
\***********************************************************************/

String File_getBaseName(String baseName, ConstString fileName);
String File_getBaseNameCString(String baseName, const char *fileName);

/***********************************************************************\
* Name   : File_getRootName, File_getRootNameCString
* Purpose: get root of file
* Input  : rootName - rootname variable
*          fileName - file name
* Output : -
* Return : rootName variable
* Notes  : -
\***********************************************************************/

String File_getRootName(String rootName, ConstString fileName);
String File_getRootNameCString(String rootName, const char *fileName);

/***********************************************************************\
* Name   : File_getAbsoluteFileName, File_getAbsoluteFileNameCString
* Purpose: get absolute name of file
* Input  : absoluteFileName - absolute file name variable
*          fileName         - file name
* Output : -
* Return : absoluteFileName variable
* Notes  : -
\***********************************************************************/

String File_getAbsoluteFileName(String absoluteFileName, ConstString fileName);
String File_getAbsoluteFileNameCString(String absoluteFileName, const char *fileName);

/***********************************************************************\
* Name   : File_splitFileName
* Purpose: split file name into path name and base name
* Input  : fileName - file name
* Output : directoryName - directory name (allocated string)
*          baseName      - base name (allocated string)
* Return : -
* Notes  : -
\***********************************************************************/

void File_splitFileName(ConstString fileName, String *directoryName, String *baseName);

/***********************************************************************\
* Name   : File_initSplitFileName, File_doneSplitFileName
* Purpose: init/done file name splitter
* Input  : stringTokenizer - string tokenizer (see strings.h)
*          fileName        - file name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void File_initSplitFileName(StringTokenizer *stringTokenizer, ConstString fileName);
void File_doneSplitFileName(StringTokenizer *stringTokenizer);

/***********************************************************************\
* Name   : File_getNextFileName
* Purpose: get next part of file name
* Input  : stringTokenizer - string tokenizer (see strings.h)
* Output : name - next name (internal reference; do not delete!)
* Return : TRUE if file name found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool File_getNextSplitFileName(StringTokenizer *stringTokenizer, ConstString *name);

/***********************************************************************\
* Name   : File_isAbsoluteFileName, File_isAbsoluteFileNameCString
* Purpose: check if file name is absolute
* Input  : fileName - file name
* Output : -
* Return : TRUE if file name is absolute, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool File_isAbsoluteFileName(ConstString fileName);
bool File_isAbsoluteFileNameCString(const char *fileName);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : File_getSystemTmpDirectory
* Purpose: get system temporary directory name
* Input  : -
* Output : -
* Return : temporary directory name
* Notes  : -
\***********************************************************************/

const char *File_getSystemTmpDirectory(void);

/***********************************************************************\
* Name   : File_getTmpFile, File_getTmpFileCString
* Purpose: create and open a temporary file
* Input  : fileHandle - variable for temporary file handle
*          pattern    - pattern with XXXXXX or NULL
*          directory  - directory to create temporary file (can be NULL)
* Output : fileHandle - temporary file handle
* Return : TRUE iff temporary file created and opened, FALSE otherwise
* Notes  : File is deleted when closed
\***********************************************************************/

#ifdef NDEBUG
  Errors File_getTmpFile(FileHandle  *fileHandle,
                         ConstString pattern,
                         ConstString directory
                        );
  Errors File_getTmpFileCString(FileHandle *fileHandle,
                                const char *pattern,
                                const char *directory
                               );
#else /* not NDEBUG */
  Errors __File_getTmpFile(const char  *__fileName__,
                           ulong       __lineNb__,
                           FileHandle  *fileHandle,
                           ConstString pattern,
                           ConstString directory
                          );
  Errors __File_getTmpFileCString(const char *__fileName__,
                                  ulong      __lineNb__,
                                  FileHandle *fileHandle,
                                  const char *pattern,
                                  const char *directory
                                 );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : File_getTmpFileName, File_getTmpFileNameCString
* Purpose: create and get a temporary file name
* Input  : fileName  - variable for temporary file name
*          prefix    - prefix or NULL (default "tmp")
*          directory - directory to create temporary file (can be NULL)
* Output : fileName - temporary file name
* Return : TRUE iff temporary file created, FALSE otherwise
* Notes  : if directory is NULL "tmp" is used
*          if directory is NULL system temporary directory is used
\***********************************************************************/

Errors File_getTmpFileName(String      fileName,
                           const char  *prefix,
                           ConstString directory
                          );
Errors File_getTmpFileNameCString(String     fileName,
                                  const char *prefix,
                                  const char *directory
                                 );

/***********************************************************************\
* Name   : File_getTmpDirectoryName, File_getTmpDirectoryNameCString
* Purpose: create and get a temporary directory name
* Input  : directoryName - variable for temporary directory name
*          prefix        - prefix (can be NULL)
*          directory     - directory to create temporary file (can be
*                          NULL)
* Output : directoryName - temporary directory name
* Return : TRUE iff temporary directory created, FALSE otherwise
* Notes  : if directory is NULL "tmp" is used
*          if directory is NULL system temporary directory is used
\***********************************************************************/

Errors File_getTmpDirectoryName(String      directoryName,
                                const char  *prefix,
                                ConstString directory
                               );
Errors File_getTmpDirectoryNameCString(String     directoryName,
                                       const char *prefix,
                                       const char *directory
                                      );

void File_registerTmpFile(FileHandle *fileHandle);
void File_unregisterTmpFile(FileHandle *fileHandle);
uint64 File_getRegisterTmpFileSize(void);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : File_fileTypeToString
* Purpose: get name of file type
* Input  : fileType     - file type
*          defaultValue - default value
* Output : -
* Return : file type string
* Notes  : -
\***********************************************************************/

const char *File_fileTypeToString(FileTypes fileType, const char *defaultValue);

/***********************************************************************\
* Name   : File_parseFileType
* Purpose: parse file type algorithm
* Input  : name - name of crypt algorithm
* Output : fileType - archive type
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool File_parseFileType(const char *name, FileTypes *fileType);

/***********************************************************************\
* Name   : File_fileSpecialTypeToString
* Purpose: get name of file special type
* Input  : fileSpecialType - file special type
*          defaultValue    - default value
* Output : -
* Return : file type string
* Notes  : -
\***********************************************************************/

const char *File_fileSpecialTypeToString(FileSpecialTypes fileSpecialType, const char *defaultValue);

/***********************************************************************\
* Name   : File_parseSpecialFileType
* Purpose: parse file special type
* Input  : name - name of file special type
* Output : fileSpecialType - file special type
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool File_parseFileSpecialType(const char *name, FileSpecialTypes *fileSpecialType);

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
Errors File_open(FileHandle   *fileHandle,
                 ConstString  fileName,
                 FileModes    fileMode
                );
Errors File_openCString(FileHandle *fileHandle,
                        const char *fileName,
                        FileModes  fileMode
                       );
#else /* not NDEBUG */
Errors __File_open(const char  *__fileName__,
                   ulong       __lineNb__,
                   FileHandle   *fileHandle,
                   ConstString fileName,
                   FileModes   fileMode
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
Errors __File_close(const char *__fileName__,
                    ulong      __lineNb__,
                    FileHandle *fileHandle
                   );
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
*          bufferSize   - max. length of data to read [bytes]
* Output : bytesRead - bytes read (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : if bytesRead is not given (NULL) reading less than
*          bufferLength bytes result in an i/o error
\***********************************************************************/

Errors File_read(FileHandle *fileHandle,
                 void       *buffer,
                 ulong      bufferSize,
                 ulong      *bytesRead
                );

/***********************************************************************\
* Name   : File_write
* Purpose: write data into file
* Input  : fileHandle   - file handle
*          buffer       - buffer for data to write
*          bufferLength - length of data to write [bytes]
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

Errors File_writeLine(FileHandle  *fileHandle,
                      ConstString line
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
* Name   : File_transfer
* Purpose: transfer data from file to file
* Input  : sourceFileHandle      - file handle
*          destinationFileHandle - file handle
*          length                - number of bytes to transfer
* Output : bytesTransfered - bytes transfered (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_transfer(FileHandle *sourceFileHandle,
                     FileHandle *destinationFileHandle,
                     uint64     length,
                     uint64     *bytesTransfered
                    );

/***********************************************************************\
* Name   : File_flush
* Purpose: flush pending output
* Input  : fileHandle - file handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_flush(FileHandle *fileHandle);

/***********************************************************************\
* Name   : File_getLine
* Purpose: get next non-empty/non-comment line
* Input  : fileHandle - file handle
*          line       - string variable
*          lineNb     - line number variable or NULL
* Output : line   - read line
*          lineNb - line number
* Return : TRUE iff line read
* Notes  : -
\***********************************************************************/

bool File_getLine(FileHandle *fileHandle,
                  String     line,
                  uint       *lineNb,
                  const char *commentChars
                 );

/***********************************************************************\
* Name   : File_ungetLine
* Purpose: unget line
* Input  : fileHandle - file handle
*          line       - string variable
*          lineNb     - line number variable or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void File_ungetLine(FileHandle  *fileHandle,
                    ConstString line,
                    uint        *lineNb
                   );

/***********************************************************************\
* Name   : File_getSize
* Purpose: get file size
* Input  : fileHandle - file handle
* Output : -
* Return : size of file (in bytes)
* Notes  : -
\***********************************************************************/

uint64 File_getSize(const FileHandle *fileHandle);

/***********************************************************************\
* Name   : File_tell
* Purpose: get current position in file
* Input  : fileHandle - file handle
* Output : offset - offset (0..n-1)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_tell(const FileHandle *fileHandle,
                 uint64           *offset
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

/***********************************************************************\
* Name   : File_touch
* Purpose: touch file
* Input  : fileName - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_touch(ConstString fileName);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : File_openRootList
* Purpose: open root list for reading
* Input  : rootListHandle - root list handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_openRootList(RootListHandle *rootListHandle);

/***********************************************************************\
* Name   : File_closeRootList
* Purpose: close root list
* Input  : rootListHandle - root list handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void File_closeRootList(RootListHandle *rootListHandle);

/***********************************************************************\
* Name   : File_endOfRootList
* Purpose: check if end of root list reached
* Input  : rootListHandle - root list handle
* Output : -
* Return : TRUE if not more root entries to read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool File_endOfRootList(RootListHandle *rootListHandle);

/***********************************************************************\
* Name   : File_readRootList
* Purpose: read next directory list entry
* Input  : rootListHandle - root list handle
*          name           - name variable
* Output : name - next name
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_readRootList(RootListHandle *rootListHandle,
                         String              name
                        );

/***********************************************************************\
* Name   : File_openDirectoryList
* Purpose: open directory for reading
* Input  : directoryListHandle - directory list handle
*          directoryName       - directory name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_openDirectoryList(DirectoryListHandle *directoryListHandle,
                              ConstString         directoryName
                             );
Errors File_openDirectoryListCString(DirectoryListHandle *directoryListHandle,
                                     const char          *directoryName
                                    );

/***********************************************************************\
* Name   : File_closeDirectoryList
* Purpose: close directory list
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
* Return : TRUE if not more directory entries to read, FALSE otherwise
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
* Name   : File_userNameToUserId
* Purpose: convert user name to user id
* Input  : name     - name variable
*          nameSize - max. size of name
*          userId   - user id
* Output : name - user name
* Return : user name or "NONE" if user not found
* Notes  : -
\***********************************************************************/

const char *File_userIdToUserName(char *name, uint nameSize, uint32 userId);

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
* Name   : File_groupNameToGroupId
* Purpose: convert group name to group id
* Input  : name     - name variable
*          nameSize - max. size of name
*          groupId  - group id
* Output : name - group name
* Return : group name or "NONE" if user not found
* Notes  : -
\***********************************************************************/

const char *File_groupIdToGroupName(char *name, uint nameSize, uint32 groupId);

/***********************************************************************\
* Name   : File_stringToPermission
* Purpose: convert string to file permission
* Input  : string - string
* Output : -
* Return : file permission
* Notes  : -
\***********************************************************************/

FilePermission File_stringToPermission(const char *string);

/***********************************************************************\
* Name   : File_permissionToString
* Purpose: convert file permission to string
* Input  : string     - string variable
*          stringSize - max. size of string
*          permission - file permission
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

const char *File_permissionToString(char *string, uint stringSize, FilePermission permission);

/***********************************************************************\
* Name   : File_getType
* Purpose: get file type
* Input  : fileName - file name
* Output : -
* Return : file type; see FileTypes
* Notes  : -
\***********************************************************************/

FileTypes File_getType(ConstString fileName);

/***********************************************************************\
* Name   : File_getData
* Purpose: read file content into buffer
* Input  : fileName - file name
* Output : data - data
*          size - size of data
* Return : ERROR_NONE or error code
* Notes  : data must be freed with free() after usage!
\***********************************************************************/

Errors File_getData(ConstString fileName,
                    void        **data,
                    ulong       *size
                   );
Errors File_getDataCString(const char *fileName,
                           void       **data,
                           ulong      *size
                          );

/***********************************************************************\
* Name   : File_delete, File_deleteCString
* Purpose: delete file/directory/link
* Input  : fileName - file name
* Output : -
* Return : TRUE if file/directory/link deleted, FALSE otherwise
* Notes  : -
\***********************************************************************/

Errors File_delete(ConstString fileName, bool recursiveFlag);
Errors File_deleteCString(const char *fileName, bool recursiveFlag);

/***********************************************************************\
* Name   : File_rename, File_renameCString
* Purpose: rename file/directory/link
* Input  : oldFileName       - old file name
*          newFileName       - new file name
*          newBackupFileName - new backup file name (can be NULL)
* Output : -
* Return : TRUE if renamed, FALSE otherwise
* Notes  : - renamed/copy new -> new backup (if backup name given)
*            rename/copy old -> new
*          - if files are not on the same logical device the file is
*            copied
\***********************************************************************/

Errors File_rename(ConstString oldFileName,
                   ConstString newFileName,
                   ConstString newBackupFileName
                  );
Errors File_renameCString(const char *oldFileName,
                          const char *newFileName,
                          const char *newBackupFileName
                         );

/***********************************************************************\
* Name   : File_copy, File_copyCString
* Purpose: copy files
* Input  : sourceFileName      - source file name
*          destinationFileName - destination file name
* Output : -
* Return : TRUE if file/directory/link copied, FALSE otherwise
* Notes  : -
\***********************************************************************/

Errors File_copy(ConstString sourceFileName,
                 ConstString destinationFileName
                );
Errors File_copyCString(const char *sourceFileName,
                        const char *destinationFileName
                       );

/***********************************************************************\
* Name   : File_exists, File_existsCString
* Purpose: check if file/directory/link/device exists
* Input  : fileName - file name
* Output : -
* Return : TRUE if file/directory exists, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool File_exists(ConstString fileName);
bool File_existsCString(const char *fileName);

/***********************************************************************\
* Name   : File_isFile, File_isFileCString
* Purpose: check if file
* Input  : fileName - file name
* Output : -
* Return : TRUE if file, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool File_isFile(ConstString fileName);
bool File_isFileCString(const char *fileName);

/***********************************************************************\
* Name   : File_isDirectory, File_isDirectoryCString
* Purpose: check if directory
* Input  : fileName - file name
* Output : -
* Return : TRUE if directory, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool File_isDirectory(ConstString fileName);
bool File_isDirectoryCString(const char *fileName);

/***********************************************************************\
* Name   : File_isDevice, File_isDeviceCString
* Purpose: check if device (block, character)
* Input  : fileName - file name
* Output : -
* Return : TRUE if device, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool File_isDevice(ConstString fileName);
bool File_isDeviceCString(const char *fileName);

/***********************************************************************\
* Name   : File_isReadable, File_isReadableCString
* Purpose: check if file exists and is readable
* Input  : fileName - file name
* Output : -
* Return : TRUE if file/directory exists and is readable, FALSE
*          otherweise
* Notes  : -
\***********************************************************************/

bool File_isReadable(ConstString fileName);
bool File_isReadableCString(const char *fileName);

/***********************************************************************\
* Name   : File_isWriteable, File_isWriteableCString
* Purpose: check if file or directory exists and is writable
* Input  : fileName - file name
* Output : -
* Return : TRUE if file/directory exists and is writable, FALSE
*          otherweise
* Notes  : -
\***********************************************************************/

bool File_isWriteable(ConstString fileName);
bool File_isWriteableCString(const char *fileName);

/***********************************************************************\
* Name   : File_isNetworkFileSystem, File_isNetworkFileSystemCString
* Purpose: check if file or directory is on a network filesystem
* Input  : fileName - file name
* Output : -
* Return : TRUE if file/directory is on a network filesystem
* Notes  : -
\***********************************************************************/

bool File_isNetworkFileSystem(ConstString fileName);
bool File_isNetworkFileSystemCString(const char *fileName);

/***********************************************************************\
* Name   : File_getInfo, File_getInfoCString
* Purpose: get file info
* Input  : fileInfo - file info variable
*          fileName - file name
* Output : fileInfo - file info
* Return : ERROR_NONE or error code
* Notes  : fileInfo must _not_ be initialized
\***********************************************************************/

Errors File_getInfo(FileInfo    *fileInfo,
                    ConstString fileName
                   );
Errors File_getInfoCString(FileInfo   *fileInfo,
                           const char *fileName
                          );

/***********************************************************************\
* Name   : File_setFileInfo, File_setInfoCString
* Purpose: set file info (time, owner, permission)
* Input  : fileInfo - file info
*          fileName - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_setInfo(const FileInfo *fileInfo,
                    ConstString    fileName
                   );
Errors File_setInfoCString(const FileInfo *fileInfo,
                           const char     *fileName
                          );

/***********************************************************************\
* Name   : File_haveAttributeCompress, File_haveAttributeNoCompress,
*          File_haveAttributeNoDump
* Purpose: check if compress/no-compress/no-dump attribute is set
* Input  : fileInfo - file info variable
* Output : -
* Return : TRUE if attribute is set, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool File_haveAttributeCompress(const FileInfo *fileInfo);
#if defined(NDEBUG) || defined(__FILES_IMPLEMENTATION__)
INLINE bool File_haveAttributeCompress(const FileInfo *fileInfo)
{
  assert(fileInfo != NULL);

  #ifdef HAVE_FS_COMPR_FL
    return (fileInfo->attributes & FILE_ATTRIBUTE_COMPRESS) != 0;
  #else
    UNUSED_VARIABLE(fileInfo);

    return FALSE;
  #endif
}
#endif /* NDEBUG || __FILES_IMPLEMENTATION__ */

INLINE bool File_haveAttributeNoCompress(const FileInfo *fileInfo);
#if defined(NDEBUG) || defined(__FILES_IMPLEMENTATION__)
INLINE bool File_haveAttributeNoCompress(const FileInfo *fileInfo)
{
  assert(fileInfo != NULL);

  #ifdef HAVE_FS_COMPR_FL
    return (fileInfo->attributes & FILE_ATTRIBUTE_NO_COMPRESS) != 0;
  #else
    UNUSED_VARIABLE(fileInfo);

    return FALSE;
  #endif
}
#endif /* NDEBUG || __FILES_IMPLEMENTATION__ */

INLINE bool File_haveAttributeNoDump(const FileInfo *fileInfo);
#if defined(NDEBUG) || defined(__FILES_IMPLEMENTATION__)
INLINE bool File_haveAttributeNoDump(const FileInfo *fileInfo)
{
  assert(fileInfo != NULL);

  #ifdef HAVE_FS_COMPR_FL
    return (fileInfo->attributes & FILE_ATTRIBUTE_NO_DUMP) != 0;
  #else
    UNUSED_VARIABLE(fileInfo);

    return FALSE;
  #endif
}
#endif /* NDEBUG || __FILES_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : File_initExtendedAttributes
* Purpose: initialize extended attributes list
* Input  : fileExtendedAttributeList - extended attributes list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void File_initExtendedAttributes(FileExtendedAttributeList *fileExtendedAttributeList);
#else /* not NDEBUG */
void __File_initExtendedAttributes(const char                *__fileName__,
                                   ulong                     __lineNb__,
                                   FileExtendedAttributeList *fileExtendedAttributeList
                                  );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : File_doneExtendedAttributes
* Purpose: deinitialize extended attributes list
* Input  : fileExtendedAttributeList - extended attributes list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void File_doneExtendedAttributes(FileExtendedAttributeList *fileExtendedAttributeList);
#else /* not NDEBUG */
void __File_doneExtendedAttributes(const char                *__fileName__,
                                   ulong                     __lineNb__,
                                   FileExtendedAttributeList *fileExtendedAttributeList
                                  );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : File_addExtendedAttribute, File_addExtendedAttributeCString
* Purpose: add file extended attribute to list
* Input  : name       - name of attribute
*          data       - data
*          dataLength - length of data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void File_addExtendedAttribute(FileExtendedAttributeList *fileExtendedAttributeList,
                               ConstString               name,
                               const void                *data,
                               uint                      dataLength
                              );
void File_addExtendedAttributeCString(FileExtendedAttributeList *fileExtendedAttributeList,
                                      const char                *name,
                                      const void                *data,
                                      uint                      dataLength
                                     );

/***********************************************************************\
* Name   : File_getExtendedAttributes
* Purpose: get extended attributes of file
* Input  : fileExtendedAttributeList - extended attributes list
*          fileName                  - file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : fileExtendedAttributeList must _not_ be initialized
\***********************************************************************/

Errors File_getExtendedAttributes(FileExtendedAttributeList *fileExtendedAttributeList,
                                  ConstString               fileName
                                 );

/***********************************************************************\
* Name   : File_setExtendedAttributes
* Purpose: set extended attributes of file
* Input  : fileName                  - file name
*          fileExtendedAttributeList - extended attributes list
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_setExtendedAttributes(ConstString                     fileName,
                                  const FileExtendedAttributeList *fileExtendedAttributeList
                                 );

/***********************************************************************\
* Name   : File_getFileTimeModified
* Purpose: get file modified time
* Input  : fileName - file name
* Output : -
* Return : time modified or 0
* Notes  : -
\***********************************************************************/

uint64 File_getFileTimeModified(ConstString fileName);

/***********************************************************************\
* Name   : File_setPermission
* Purpose: set file permission
* Input  : fileName   - file name
*          permission - file permission
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_setPermission(ConstString    fileName,
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

Errors File_setOwner(ConstString fileName,
                     uint32      userId,
                     uint32      groupId
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

Errors File_makeDirectory(ConstString    pathName,
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
* Name   : File_changeDirectory, File_changeDirectoryCString
* Purpose: change current directory
* Input  : pathName - path name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_changeDirectory(ConstString pathName);
Errors File_changeDirectoryCString(const char *pathName);

/***********************************************************************\
* Name   : File_readLink
* Purpose: read link
* Input  : linkName - link name
* Output : fileName - file name link references
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors File_readLink(String      fileName,
                     ConstString linkName
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

Errors File_makeLink(ConstString linkName,
                     ConstString fileName
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

Errors File_makeHardLink(ConstString linkName,
                         ConstString fileName
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

Errors File_makeSpecial(ConstString      name,
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
                              ConstString    pathName
                             );


INLINE bool File_isEqualsCast(const FileCast *fileCast0, const FileCast *fileCast1);
#if defined(NDEBUG) || defined(__FILES_IMPLEMENTATION__)
INLINE bool File_isEqualsCast(const FileCast *fileCast0, const FileCast *fileCast1)
{
  return memcmp(fileCast0,fileCast1,sizeof(FileCast)) == 0;
}
#endif /* NDEBUG || __FILES_IMPLEMENTATION__ */

String File_castToString(String string, const FileCast *fileCast);

/***********************************************************************\
* Name   : File_getDescriptor
* Purpose: get file descriptor
* Input  : file - file
* Output : -
* Return : file descriptor
* Notes  : -
\***********************************************************************/

INLINE int File_getDescriptor(FILE *file);
#if defined(NDEBUG) || defined(__FILES_IMPLEMENTATION__)
INLINE int File_getDescriptor(FILE *file)
{
  assert(file != NULL);

  return fileno(file);
}
#endif /* NDEBUG || __FILES_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : File_isTerminal
* Purpose: check if file handle is connected to a terminal
* Input  : file - file
* Output : -
* Return : TRUE if file is connected to a terminal, FALSE otherweise
* Notes  : -
\***********************************************************************/

bool File_isTerminal(FILE *file);

#ifndef NDEBUG
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
* Input  : handle               - output channel
*          fileDumpInfoFunction - file dump info call-back or NULL
*          fileDumpInfoUserData - file dump info user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void File_debugDumpInfo(FILE                 *handle,
                        FileDumpInfoFunction fileDumpInfoFunction,
                        void                 *fileDumpInfoUserData
                       );
void File_debugPrintInfo(FileDumpInfoFunction fileDumpInfoFunction,
                         void                 *fileDumpInfoUserData
                        );

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
* Name   : File_debugCheck
* Purpose: file debug function: output open files and statistics, check
*          for lost resources
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void File_debugCheck(void);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __FILES__ */

/* end of file */