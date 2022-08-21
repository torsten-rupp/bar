/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver file functions
* Systems: all
*
\***********************************************************************/

#define __FILES_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_SYSMACROS_H
  #include <sys/sysmacros.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#ifdef HAVE_SYS_IOCTL_H
  #include <sys/ioctl.h>
#endif
#include <utime.h>
#ifdef HAVE_SYS_STATFS_H
  #include <sys/statfs.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
  #include <sys/statvfs.h>
#endif
#ifdef HAVE_SYS_XATTR_H
  #include <sys/xattr.h>
#endif
#ifdef HAVE_SYS_VFS_H
  #include <sys/vfs.h>
#endif
#include <errno.h>
#ifdef HAVE_EXECINFO_H
  #include <execinfo.h>
#endif
#include <assert.h>

#if   defined(PLATFORM_LINUX)
  #include <linux/fs.h>
  #include <linux/magic.h>
#elif defined(PLATFORM_WINDOWS)
  #include <fileapi.h>
  #include <winsock2.h>
  #include <windows.h>
  #include <shlobj.h>
  #include <combaseapi.h>
  #include <knownfolders.h>
#endif /* PLATFORM_... */

#include "common/global.h"
#include "common/lists.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/devices.h"
#include "errors.h"

#ifndef NDEBUG
  #include <pthread.h>
  #include "common/lists.h"
#endif /* not NDEBUG */

#include "files.h"

/****************** Conditional compilation switches *******************/

#ifndef NDEBUG
  #define getLastError(...) __getLastError(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Constants *******************************/

// file types
LOCAL const struct
{
  const char *name;
  FileTypes  fileType;
} FILE_TYPES[] =
{
  {"NONE",     FILE_TYPE_NONE     },
  {"FILE",     FILE_TYPE_FILE     },
  {"DIRECTORY",FILE_TYPE_DIRECTORY},
  {"LINK",     FILE_TYPE_LINK     },
  {"HARDLINK", FILE_TYPE_HARDLINK },
  {"SPECIAL",  FILE_TYPE_SPECIAL  }
};

// file special types
LOCAL const struct
{
  const char       *name;
  FileSpecialTypes fileSpecialType;
} FILE_SPECIAL_TYPES[] =
{
  {"CHARACTER_DEVICE",FILE_SPECIAL_TYPE_CHARACTER_DEVICE},
  {"BLOCK_DEVICE",    FILE_SPECIAL_TYPE_BLOCK_DEVICE    },
  {"FIFO",            FILE_SPECIAL_TYPE_FIFO            },
  {"SOCKET",          FILE_SPECIAL_TYPE_SOCKET          },
  {"OTHER",           FILE_SPECIAL_TYPE_OTHER           }
};

#if   defined(PLATFORM_LINUX)
  #define O_BINARY 0
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

#if   defined(PLATFORM_LINUX)
  const char *FILESYSMTES_FILENAME = "/proc/filesystems";
  const char *MOUNTS_FILENAME      = "/proc/mounts";
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

#define DEBUG_MAX_CLOSED_LIST 100

/***************************** Datatypes *******************************/
#ifdef HAVE_LSEEK64
  #define SEEK(handle,offset,mode) lseek64(handle,offset,mode)
  #define TELL(handle) lseek64(handle,0,SEEK_CUR)
#else
  #define SEEK(handle,offset,mode) lseek(handle,offset,mode)
  #define TELL(handle) lseek(handle,0,SEEK_CUR)
#endif

#ifdef HAVE_FTRUNCATE64
  #define FTRUNCATE(handle,size) ftruncate64(handle,size)
#else
  #define FTRUNCATE(handle,size) ftruncate(handle,size)
#endif

#ifdef HAVE_FOPEN64
  #define FOPEN(fileName,mode) fopen64(fileName,mode)
#else
  #define FOPEN(fileName,mode) fopen(fileName,mode)
#endif

#ifdef HAVE_FSEEKO64
  #define FSEEK(handle,offset,mode) fseeko64(handle,offset,mode)
#elif HAVE_FSEEKO
  #define FSEEK(handle,offset,mode) fseeko(handle,offset,mode)
#else
  #define FSEEK(handle,offset,mode) fseek(handle,offset,mode)
#endif

#ifdef HAVE_FTELLO64
  #define FTELL(handle) ftello64(handle)
#elif HAVE_FTELLO
  #define FTELL(handle) ftello(handle)
#else
  #define FTELL(handle) ftell(handle)
#endif

#if defined(HAVE_STAT64) && defined(HAVE_LSTAT64) && defined(HAVE_STRUCT_STAT64)
  #define STAT(fileName,fileState)  stat64(fileName,fileState)
  #define LSTAT(fileName,fileState) lstat64(fileName,fileState)
  typedef struct stat64 FileStat;
#elif defined(HAVE___STAT64) && defined(HAVE___LSTAT64) && defined(HAVE_STRUCT___STAT64)
  #define STAT(fileName,fileState)  __stat64(fileName,fileState)
  #define LSTAT(fileName,fileState) __lstat64(fileName,fileState)
  typedef struct __stat64 FileStat;
#elif defined(HAVE_STAT) && defined(HAVE_LSTAT) && defined(HAVE_STRUCT_STAT)
  #define STAT(fileName,fileState)  stat(fileName,fileState)
  #define LSTAT(fileName,fileState) lstat(fileName,fileState)
  typedef struct stat FileStat;
#elif defined(HAVE__STATI64) && defined(HAVE_STRUCT__STATI64)
  #define STAT(fileName,fileState)  _stati64(fileName,fileState)
  #define LSTAT(fileName,fileState) _stati64(fileName,fileState)
  typedef struct _stati64 FileStat;
#else
  #error No struct stat, lstat, or struct stat64
#endif

#ifndef NDEBUG
  typedef struct DebugFileNode
  {
    LIST_NODE_HEADER(struct DebugFileNode);

    const char *fileName;
    ulong      lineNb;
    #ifdef HAVE_BACKTRACE
      void const *stackTrace[16];
      int        stackTraceSize;
    #endif /* HAVE_BACKTRACE */
    const char *closeFileName;
    ulong      closeLineNb;
    #ifdef HAVE_BACKTRACE
      void const *closeStackTrace[16];
      int        closeStackTraceSize;
    #endif /* HAVE_BACKTRACE */
    FileHandle *fileHandle;
  } DebugFileNode;

  typedef struct
  {
    LIST_HEADER(DebugFileNode);
  } DebugFileList;
#endif /* not NDEBUG */

/***************************** Variables *******************************/
#ifndef NDEBUG
  LOCAL pthread_once_t      debugFileInitFlag = PTHREAD_ONCE_INIT;
  LOCAL pthread_mutexattr_t debugFileLockAttribute;
  LOCAL pthread_mutex_t     debugFileLock;
  LOCAL DebugFileList       debugOpenFileList;
  LOCAL DebugFileList       debugClosedFileList;
#endif /* not NDEBUG */

/****************************** Macros *********************************/
#ifndef NDEBUG
  #define FILE_CHECK_VALID(fileHandle) \
    do \
    { \
      fileCheckValid(__FILE__,__LINE__,fileHandle); \
    } \
    while (0)
#else /* NDEBUG */
  #define FILE_CHECK_VALID(fileHandle) \
    do \
    { \
    } \
    while (0)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifndef NDEBUG
/***********************************************************************\
* Name   : debugFileInit
* Purpose: debug init
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugFileInit(void)
{
  if (pthread_mutexattr_init(&debugFileLockAttribute) != 0)
  {
    HALT_INTERNAL_ERROR("Cannot initialize file debug lock!");
  }
  pthread_mutexattr_settype(&debugFileLockAttribute,PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&debugFileLock,&debugFileLockAttribute);
  List_init(&debugOpenFileList,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  List_init(&debugClosedFileList,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
}
#endif /* NDEBUG */

/***********************************************************************\
* Name   : getLastError
* Purpose: get last file error
* Input  : fileName - file name
* Output : -
* Return : ERROR_NONE or last error
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL Errors getLastError(ErrorCodes errorCode,
                          const char *fileName
                         )
#else /* not NDEBUG */
LOCAL Errors __getLastError(const char *__fileName__,
                            ulong      __lineNb__,
                            ErrorCodes errorCode,
                            const char *fileName
                           )
#endif /* NDEBUG */
{
  int    n;
  Errors error;

  n = errno;

  switch (n)
  {
    case ENOSPC:
      {
        String deviceName;

        deviceName = File_getDeviceNameCString(String_new(),fileName);
        #ifdef NDEBUG
          error = Errorx_(ERROR_CODE_IO,ENOSPC,"no space left on device '%s'",String_cString(deviceName));
        #else /* not NDEBUG */
          error = Errorx_(__fileName__,__lineNb__,ERROR_CODE_IO,ENOSPC,"no space left on device '%s'",String_cString(deviceName));
        #endif /* NDEBUG */
        String_delete(deviceName);
      }
      break;
    default:
      #ifdef NDEBUG
        error = Errorx_(errorCode,n,"%E: %s",n,fileName);
      #else /* not NDEBUG */
        error = Errorx_(__fileName__,__lineNb__,errorCode,n,"%E: %s",n,fileName);
      #endif /* NDEBUG */
      break;
  }

  return error;
}

/***********************************************************************\
* Name   : getFileType
* Purpose: get file type from file state
* Input  : fileStat - file state
* Output : file type; see FileTypes
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL FileTypes getFileType(const FileStat *fileStat)
{
  assert(fileStat != NULL);

  if      (S_ISREG(fileStat->st_mode))  return (fileStat->st_nlink > 1) ? FILE_TYPE_HARDLINK : FILE_TYPE_FILE;
  else if (S_ISDIR(fileStat->st_mode))  return FILE_TYPE_DIRECTORY;
  #ifdef S_ISLNK
  else if (S_ISLNK(fileStat->st_mode))  return FILE_TYPE_LINK;
  #endif /* S_ISLNK */
  else if (S_ISCHR(fileStat->st_mode))  return FILE_TYPE_SPECIAL;
  else if (S_ISBLK(fileStat->st_mode))  return FILE_TYPE_SPECIAL;
  else if (S_ISFIFO(fileStat->st_mode)) return FILE_TYPE_SPECIAL;
  #ifdef S_ISSOCK
  else if (S_ISSOCK(fileStat->st_mode)) return FILE_TYPE_SPECIAL;
  #endif /* S_ISSOCK */
  else                                  return FILE_TYPE_UNKNOWN;
}

#ifndef NDEBUG
/***********************************************************************\
* Name   : fileCheckValid
* Purpose: check if file handle is valid
* Input  : fileName   - file name
*          lineNb     - line number
*          fileHandle - file handle to checke
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void fileCheckValid(const char       *fileName,
                          ulong            lineNb,
                          const FileHandle *fileHandle
                         )
{
  DebugFileNode *debugFileNode;

  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);

  pthread_once(&debugFileInitFlag,debugFileInit);

  pthread_mutex_lock(&debugFileLock);
  {
    // check if file was closed
    debugFileNode = debugClosedFileList.head;
    while ((debugFileNode != NULL) && (debugFileNode->fileHandle != fileHandle))
    {
      debugFileNode = debugFileNode->next;
    }
    if (debugFileNode != NULL)
    {
      #ifdef HAVE_BACKTRACE
        debugDumpStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugFileNode->closeStackTrace,debugFileNode->closeStackTraceSize,0);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR_AT(fileName,
                             lineNb,
                             "File %p was closed at %s, line %lu",
                             fileHandle,
                             debugFileNode->closeFileName,
                             debugFileNode->closeLineNb
                            );
    }

    // check if file is open
    debugFileNode = debugOpenFileList.head;
    while ((debugFileNode != NULL) && (debugFileNode->fileHandle != fileHandle))
    {
      debugFileNode = debugFileNode->next;
    }
    if (debugFileNode == NULL)
    {
      #ifdef HAVE_BACKTRACE
        debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR("File %p is not open",
                          fileHandle
                         );
    }
  }
  pthread_mutex_unlock(&debugFileLock);

  // Note: real file index may be different, because of buffer in stream object
  // assert(IS_SET(fileHandle->mode,FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
}
#endif /* NDEBUG */

/***********************************************************************\
* Name   : initFileHandle
* Purpose: initialize file handle
* Input  : fileHandle     - file handle variable
*          fileDescriptor - open file descriptor
*          fileName       - file name
*          fileMode       - file mode
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL Errors initFileHandle(FileHandle *fileHandle,
                            int        fileDescriptor,
                            const char *fileName,
                            FileModes  fileMode
                           )
#else /* not NDEBUG */
LOCAL Errors initFileHandle(const char *__fileName__,
                            ulong      __lineNb__,
                            FileHandle *fileHandle,
                            int        fileDescriptor,
                            const char *fileName,
                            FileModes  fileMode
                           )
#endif /* NDEBUG */
{
  int64_t n;
  Errors  error;
  #ifndef NDEBUG
    DebugFileNode *debugFileNode;
  #endif /* not NDEBUG */

  assert(fileHandle != NULL);
  assert(fileDescriptor >= 0);

  switch (fileMode & FILE_OPEN_MASK_MODE)
  {
    case FILE_OPEN_CREATE:
      // open file from descriptor
      fileHandle->file = fdopen(fileDescriptor,"w+b");
      if (fileHandle->file == NULL)
      {
        return getLastError(ERROR_CODE_CREATE_FILE,fileName);
      }

      // seek to start and truncate
      if ((fileMode & FILE_STREAM) != FILE_STREAM)
      {
        if (FSEEK(fileHandle->file,0,SEEK_SET) != 0)
        {
          return getLastError(ERROR_CODE_CREATE_FILE,fileName);
        }
        if (FTRUNCATE(fileDescriptor,0) != 0)
        {
          return getLastError(ERROR_CODE_CREATE_FILE,fileName);
        }
      }

      fileHandle->index = 0LL;
      fileHandle->size  = 0LL;
      break;
    case FILE_OPEN_READ:
      // open file from descriptor
      fileHandle->file = fdopen(fileDescriptor,"rb");
      if (fileHandle->file == NULL)
      {
        return getLastError(ERROR_CODE_CREATE_FILE,fileName);
      }

      // get file size
      if ((fileMode & FILE_STREAM) != FILE_STREAM)
      {
        if (FSEEK(fileHandle->file,0,SEEK_END) != 0)
        {
          error = getLastError(ERROR_CODE_IO,fileName);
          fclose(fileHandle->file);
          return error;
        }
        n = (int64_t)FTELL(fileHandle->file);
        if (n == (-1LL))
        {
          error = getLastError(ERROR_CODE_IO,fileName);
          fclose(fileHandle->file);
          return error;
        }
        if (FSEEK(fileHandle->file,0,SEEK_SET) != 0)
        {
          error = getLastError(ERROR_CODE_IO,fileName);
          fclose(fileHandle->file);
          return error;
        }
      }
      else
      {
        n = 0LL;
      }

      fileHandle->index = 0LL;
      fileHandle->size  = (uint64_t)n;
      break;
    case FILE_OPEN_WRITE:
      // open file from descriptor
      fileHandle->file = fdopen(fileDescriptor,"w+b");
      if (fileHandle->file == NULL)
      {
        return getLastError(ERROR_CODE_OPEN_FILE,fileName);
      }

      // seek to start
      if ((fileMode & FILE_STREAM) != FILE_STREAM)
      {
        if (FSEEK(fileHandle->file,0,SEEK_SET) != 0)
        {
          return getLastError(ERROR_CODE_CREATE_FILE,fileName);
        }
      }

      fileHandle->index = 0LL;
      fileHandle->size  = 0LL;
      break;
    case FILE_OPEN_APPEND:
      // open file from descriptor
      fileHandle->file = fdopen(fileDescriptor,"ab");
      if (fileHandle->file == NULL)
      {
        return getLastError(ERROR_CODE_OPEN_FILE,fileName);
      }

      // get file size
      if ((fileMode & FILE_STREAM) != FILE_STREAM)
      {
        if (FSEEK(fileHandle->file,0,SEEK_END) != 0)
        {
          return getLastError(ERROR_CODE_CREATE_FILE,fileName);
        }
        n = (int64_t)FTELL(fileHandle->file);
        if (n == (-1LL))
        {
          error = getLastError(ERROR_CODE_IO,fileName);
          fclose(fileHandle->file);
          return error;
        }
      }
      else
      {
        n = 0LL;
      }

      fileHandle->index = (uint64)n;
      fileHandle->size  = (uint64)n;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  fileHandle->name        = String_newCString(fileName);;
  fileHandle->mode        = fileMode;
  #ifndef NDEBUG
    fileHandle->deleteOnCloseFlag = FALSE;
  #endif /* not NDEBUG */
  StringList_init(&fileHandle->lineBufferList);

  #ifndef NDEBUG
    pthread_once(&debugFileInitFlag,debugFileInit);

    pthread_mutex_lock(&debugFileLock);
    {
      // check if file is already in open-list
      debugFileNode = debugOpenFileList.head;
      while ((debugFileNode != NULL) && (debugFileNode->fileHandle != fileHandle))
      {
        debugFileNode = debugFileNode->next;
      }
      if (debugFileNode != NULL)
      {
        #ifdef HAVE_BACKTRACE
          debugDumpStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugFileNode->stackTrace,debugFileNode->stackTraceSize,0);
        #endif /* HAVE_BACKTRACE */
        if (debugFileNode->fileHandle->name != NULL)
        {
          HALT_INTERNAL_ERROR("File '%s' at %s, line %lu opened again at %s, line %lu",
                              String_cString(debugFileNode->fileHandle->name),
                              debugFileNode->fileName,
                              debugFileNode->lineNb,
                              __fileName__,
                              __lineNb__
                             );
        }
        else
        {
          HALT_INTERNAL_ERROR("File %p at %s, line %lu opened again at %s, line %lu",
                              debugFileNode->fileHandle,
                              debugFileNode->fileName,
                              debugFileNode->lineNb,
                              __fileName__,
                              __lineNb__
                             );
        }
      }

      // find file in closed-list; reuse or allocate new debug node
      debugFileNode = debugClosedFileList.head;
      while ((debugFileNode != NULL) && (debugFileNode->fileHandle != fileHandle))
      {
        debugFileNode = debugFileNode->next;
      }
      if (debugFileNode != NULL)
      {
        List_remove(&debugClosedFileList,debugFileNode);
      }
      else
      {
        debugFileNode = LIST_NEW_NODE(DebugFileNode);
        if (debugFileNode == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }
      }

      // init file node
      debugFileNode->fileName              = __fileName__;
      debugFileNode->lineNb                = __lineNb__;
      #ifdef HAVE_BACKTRACE
        debugFileNode->stackTraceSize      = getStackTrace(debugFileNode->stackTrace,SIZE_OF_ARRAY(debugFileNode->stackTrace));
      #endif /* HAVE_BACKTRACE */
      debugFileNode->closeFileName         = NULL;
      debugFileNode->closeLineNb           = 0;
      #ifdef HAVE_BACKTRACE
        debugFileNode->closeStackTraceSize = 0;
      #endif /* HAVE_BACKTRACE */
      debugFileNode->fileHandle            = fileHandle;

      // add file to open-list
      List_append(&debugOpenFileList,debugFileNode);
    }
    pthread_mutex_unlock(&debugFileLock);
  #endif /* not NDEBUG */

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : doneFileHandle
* Purpose: deinitialize file handle
* Input  : fileHandle     - file handle variable
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL Errors doneFileHandle(FileHandle *fileHandle)
#else /* not NDEBUG */
LOCAL Errors doneFileHandle(const char  *__fileName__,
                            ulong       __lineNb__,
                            FileHandle  *fileHandle
                           )
#endif /* NDEBUG */
{
  Errors error;

  #ifndef NDEBUG
    DebugFileNode *debugFileNode;
  #endif /* not NDEBUG */

  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);

  error = ERROR_NONE;

  // close file
  (void)fclose(fileHandle->file);

  // delete on close
  #ifndef NDEBUG
    if (fileHandle->deleteOnCloseFlag && (fileHandle->name != NULL))
    {
      if (unlink(String_cString(fileHandle->name)) != 0)
      {
        if (error == ERROR_NONE) error = getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
      }
    }
  #endif /* not NDEBUG */

  // free resources
  StringList_done(&fileHandle->lineBufferList);
  if (fileHandle->name != NULL) String_delete(fileHandle->name);

  #ifndef NDEBUG
    pthread_once(&debugFileInitFlag,debugFileInit);

    pthread_mutex_lock(&debugFileLock);
    {
      // find file in open-list
      debugFileNode = debugOpenFileList.head;
      while ((debugFileNode != NULL) && (debugFileNode->fileHandle != fileHandle))
      {
        debugFileNode = debugFileNode->next;
      }
      if (debugFileNode != NULL)
      {
        // remove from open list
        List_remove(&debugOpenFileList,debugFileNode);

        // add to closed list
        debugFileNode->closeFileName         = __fileName__;
        debugFileNode->closeLineNb           = __lineNb__;
        #ifdef HAVE_BACKTRACE
          debugFileNode->closeStackTraceSize = getStackTrace(debugFileNode->closeStackTrace,SIZE_OF_ARRAY(debugFileNode->closeStackTrace));
        #endif /* HAVE_BACKTRACE */
        List_append(&debugClosedFileList,debugFileNode);

        // shorten closed list
        while (debugClosedFileList.count > DEBUG_MAX_CLOSED_LIST)
        {
          debugFileNode = (DebugFileNode*)List_removeFirst(&debugClosedFileList);
          LIST_DELETE_NODE(debugFileNode);
        }
      }
      else
      {
        #ifdef HAVE_BACKTRACE
          debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
        #endif /* HAVE_BACKTRACE */
        HALT_INTERNAL_ERROR("File '%p' not found in debug list at %s, line %lu",
                            fileHandle->file,
                            __fileName__,
                            __lineNb__
                           );
      }
    }
    pthread_mutex_unlock(&debugFileLock);
  #endif /* not NDEBUG */

  return error;
}

/***********************************************************************\
* Name   : setAccessTime
* Purpose: set atime
* Input  : fileDescriptor - file descriptor
*          ts             - time
* Output : -
* Return : TRUE if set, FALSE otherwise
* Notes  : -
\***********************************************************************/

#ifndef HAVE_O_NOATIME
LOCAL bool setAccessTime(int fileDescriptor, const struct timespec *ts)
{
  #if   defined(PLATFORM_LINUX)
    struct timespec times[2];
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  #if   defined(PLATFORM_LINUX)
    times[0].tv_sec  = ts->tv_sec;
    times[0].tv_nsec = ts->tv_nsec;
    times[1].tv_sec  = 0;
    times[1].tv_nsec = UTIME_OMIT;

    return futimens(fileDescriptor,times) == 0;
  #elif defined(PLATFORM_WINDOWS)
    UNUSED_VARIABLE(fileDescriptor);
    UNUSED_VARIABLE(ts);
//TODO: NYI

    return TRUE;
  #endif /* PLATFORM_... */
}
#endif /* not HAVE_O_NOATIME */

/***********************************************************************\
* Name   : freeExtendedAttributeNode
* Purpose: free allocated extended attribute node
* Input  : fileExtendedAttributeNode - extended attribute node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeExtendedAttributeNode(FileExtendedAttributeNode *fileExtendedAttributeNode, void *userData)
{
  assert(fileExtendedAttributeNode != NULL);
  assert(fileExtendedAttributeNode->name != NULL);
  assert(fileExtendedAttributeNode->data != NULL);

  UNUSED_VARIABLE(userData);

  free(fileExtendedAttributeNode->data);
  String_delete(fileExtendedAttributeNode->name);
}

/***********************************************************************\
* Name   : parseMountListEntry
* Purpose: parse mount list entry
* Input  : mountPointName        - mount point name variable
*          maxMountPointNameSize - max. size of mount point name
*          line                  - line
*          fileSystemNames       - file system names (can be NULL)
* Output : -
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

#if defined(PLATFORM_LINUX)
//LOCAL void parseMountListEntry(RootListHandle *rootListHandle)
LOCAL bool parseMountListEntry(char *mountPointName, uint maxMountPointNameSize, char *line, const StringList *fileSystemNames)
{
  char       *tokenizer;
  const char *s;
  bool       parseFlag;
  StringNode *iteratorVariable;
  String     variable;

  // parse
  s = strtok_r(line," ",&tokenizer);
  if (s == NULL)
  {
    return FALSE;
  }

  // get name
  s = strtok_r(NULL," ",&tokenizer);
  if (s == NULL)
  {
    return FALSE;
  }
  stringSet(mountPointName,maxMountPointNameSize,s);

  // check if known file system
  parseFlag = FALSE;
  if (fileSystemNames != NULL)
  {
    s = strtok_r(NULL," ",&tokenizer);
    if (s == NULL)
    {
      return FALSE;
    }
    STRINGLIST_ITERATE(fileSystemNames,iteratorVariable,variable)
    {
      if (String_equalsCString(variable,s))
      {
        parseFlag = TRUE;
        break;
      }
    }
  }
  else
  {
    parseFlag = TRUE;
  }

  return parseFlag;
}
#endif /* PLATFORM_... */

/*---------------------------------------------------------------------*/

String File_newFileName(void)
{
  return String_new();
}

String File_duplicateFileName(ConstString fromFileName)
{
  return String_duplicate(fromFileName);
}

void File_deleteFileName(String fileName)
{
  String_delete(fileName);
}

String File_clearFileName(String fileName)
{
  assert(fileName != NULL);

  return String_clear(fileName);
}

String File_setFileName(String fileName, ConstString name)
{
  assert(fileName != NULL);

  return String_set(fileName,name);
}

String File_setFileNameCString(String fileName, const char *name)
{
  assert(fileName != NULL);

  return String_setCString(fileName,name);
}

String File_setFileNameChar(String fileName, char ch)
{
  assert(fileName != NULL);

  return String_setChar(fileName,ch);
}

String File_appendFileName(String fileName, ConstString name)
{
  assert(fileName != NULL);
  assert(name != NULL);

  if (!String_isEmpty(fileName))
  {
    if (   !String_endsWithChar(fileName,FILE_PATH_SEPARATOR_CHAR)
        && !String_startsWithChar(name,FILE_PATH_SEPARATOR_CHAR)
       )
    {
      String_appendChar(fileName,FILE_PATH_SEPARATOR_CHAR);
    }
  }
  String_append(fileName,name);

  return fileName;
}

String File_appendFileNameCString(String fileName, const char *name)
{
  assert(fileName != NULL);
  assert(name != NULL);

  if (!String_isEmpty(fileName))
  {
    if (   !String_endsWithChar(fileName,FILE_PATH_SEPARATOR_CHAR)
        && (name[0] != FILE_PATH_SEPARATOR_CHAR)
       )
    {
      String_appendChar(fileName,FILE_PATH_SEPARATOR_CHAR);
    }
  }
  String_appendCString(fileName,name);

  return fileName;
}

String File_appendFileNameChar(String fileName, char ch)
{
  assert(fileName != NULL);

  if (!String_isEmpty(fileName))
  {
    if (   !String_endsWithChar(fileName,FILE_PATH_SEPARATOR_CHAR)
        && (ch != FILE_PATH_SEPARATOR_CHAR)
       )
    {
      String_appendChar(fileName,FILE_PATH_SEPARATOR_CHAR);
    }
  }
  String_appendChar(fileName,ch);

  return fileName;
}

String File_appendFileNameBuffer(String fileName, const char *buffer, ulong bufferLength)
{
  assert(fileName != NULL);

  if (!String_isEmpty(fileName))
  {
    if (   !String_endsWithChar(fileName,FILE_PATH_SEPARATOR_CHAR)
        && ((bufferLength == 0) || (buffer[0] != FILE_PATH_SEPARATOR_CHAR))
       )
    {
      String_appendChar(fileName,FILE_PATH_SEPARATOR_CHAR);
    }
  }
  String_appendBuffer(fileName,buffer,bufferLength);

  return fileName;
}

String File_getDirectoryName(String pathName, ConstString fileName)
{
  long n;

  assert(pathName != NULL);

  if (fileName != NULL)
  {
    n = String_findLastChar(fileName,STRING_END,FILE_PATH_SEPARATOR_CHAR);
    if (n >= 0)
    {
      String_sub(pathName,fileName,STRING_BEGIN,n);
    }
    else
    {
      String_clear(pathName);
    }
  }
  else
  {
    String_clear(pathName);
  }

  return pathName;
}

String File_getDirectoryNameCString(String pathName, const char *fileName)
{
  long i;

  assert(pathName != NULL);

  if (fileName != NULL)
  {
    // find last path separator
    i = stringFindReverseChar(fileName,FILE_PATH_SEPARATOR_CHAR);

    // get path
    if (i >= 0L)
    {
      String_setBuffer(pathName,fileName,i);
    }
    else
    {
      String_clear(pathName);
    }
  }
  else
  {
    String_clear(pathName);
  }

  return pathName;
}

String File_getBaseName(String baseName, ConstString fileName)
{
  assert(baseName != NULL);

  return File_getBaseNameCString(baseName,String_cString(fileName));
}

String File_getBaseNameCString(String baseName, const char *fileName)
{
  long i;

  assert(baseName != NULL);

  if (fileName != NULL)
  {
    // find last path separator
    i = stringFindReverseChar(fileName,FILE_PATH_SEPARATOR_CHAR);

    // get path
    if (i >= 0L)
    {
      String_setCString(baseName,fileName+i+1);
    }
    else
    {
      String_setCString(baseName,fileName);
    }
  }
  else
  {
    String_clear(baseName);
  }

  return baseName;
}

String File_getRootName(String rootName, ConstString fileName)
{
  assert(rootName != NULL);

  return File_getRootNameCString(rootName,String_cString(fileName));
}

String File_getRootNameCString(String rootName, const char *fileName)
{
  size_t n;

  assert(rootName != NULL);

  String_clear(rootName);
  if (fileName != NULL)
  {
    n = stringLength(fileName);
    #if   defined(PLATFORM_LINUX)
      if ((n >= 1) && (fileName[0] == FILE_PATH_SEPARATOR_CHAR))
      {
        String_setChar(rootName,FILE_PATH_SEPARATOR_CHAR);
      }
    #elif defined(PLATFORM_WINDOWS)
      if      (   (n >= 2)
               && (toupper(fileName[0]) >= 'A') && (toupper(fileName[0]) <= 'Z')
               && (fileName[1] == ':')
              )
      {
        String_setChar(rootName,toupper(fileName[0]));
        String_appendChar(rootName,':');
      }
      else if (   (n >= 2)
               && (stringEqualsPrefix(fileName,"\\\\",2) == 0)
              )
      {
        String_clear(rootName);
      }
      if ((n >= 3) && (fileName[2] == FILE_PATH_SEPARATOR_CHAR))
      {
        String_appendChar(rootName,FILE_PATH_SEPARATOR_CHAR);
      }
    #endif /* PLATFORM_... */
  }
  else
  {
    String_clear(rootName);
  }

  return rootName;
}

String File_getDeviceName(String deviceName, ConstString fileName)
{
  assert(deviceName != NULL);

  return File_getDeviceNameCString(deviceName,String_cString(fileName));
}

String File_getDeviceNameCString(String deviceName, const char *fileName)
{
  #if   defined(PLATFORM_LINUX)
    FILE *handle;
    char line[FILE_MAX_PATH_LENGTH+256];
    char name[FILE_MAX_PATH_LENGTH];
    uint n0,n1;
  #elif defined(PLATFORM_WINDOWS)
    uint n;
  #endif /* PLATFORM_... */

  assert(deviceName != NULL);

  String_clear(deviceName);
  if (fileName != NULL)
  {
    #if   defined(PLATFORM_LINUX)
      handle = fopen(MOUNTS_FILENAME,"r");
      if (handle != NULL)
      {
        n0 = stringLength(fileName);
        while (fgets(line,sizeof(line),handle) != NULL)
        {
          if (parseMountListEntry(name,sizeof(name),line,NULL))
          {
            n1 = stringLength(name);
            if (   (n0 > n1)
                && stringEqualsPrefix(fileName,name,n1)
                && (fileName[n1] == FILE_PATH_SEPARATOR_CHAR)
               )
            {
              String_setCString(deviceName,name);
              break;
            }
          }
        }
        fclose(handle);

        if (stringStartsWith(fileName,"/"))
        {
          String_setCString(deviceName,"/");
        }
      }
    #elif defined(PLATFORM_WINDOWS)
      n = stringLength(fileName);
      if      (   (n >= 2)
               && (toupper(fileName[0]) >= 'A') && (toupper(fileName[0]) <= 'Z')
               && (fileName[1] == ':')
              )
      {
        String_setChar(deviceName,toupper(fileName[0]));
        String_appendChar(deviceName,':');
      }
      else if (   (n >= 2)
               && (stringEqualsPrefix(fileName,"\\\\",2) == 0)
              )
      {
        String_clear(deviceName);
      }
      if ((n >= 3) && (fileName[2] == FILE_PATH_SEPARATOR_CHAR))
      {
        String_appendChar(deviceName,FILE_PATH_SEPARATOR_CHAR);
      }
    #endif /* PLATFORM_... */
  }

  return deviceName;
}

bool File_isAbsoluteFileName(ConstString fileName)
{
  assert(fileName != NULL);

  return File_isAbsoluteFileNameCString(String_cString(fileName));
}

bool File_isAbsoluteFileNameCString(const char *fileName)
{
  size_t n;

  assert(fileName != NULL);

  n = stringLength(fileName);
  #if   defined(PLATFORM_LINUX)
    return ((n >= 1) && (fileName[0] == FILE_PATH_SEPARATOR_CHAR));
  #elif defined(PLATFORM_WINDOWS)
    return    ((n >= 2) && ((toupper(fileName[0]) >= 'A') && (toupper(fileName[0]) <= 'Z') && (fileName[1] == ':')))
           || ((n >= 2) && (stringEqualsPrefix(fileName,"\\\\",2) == 0));
  #endif /* PLATFORM_... */
}

String File_getAbsoluteFileName(String absoluteFileName, ConstString fileName)
{
  assert(absoluteFileName != NULL);
  assert(fileName != NULL);

  return File_getAbsoluteFileNameCString(absoluteFileName,String_cString(fileName));
}

String File_getAbsoluteFileNameCString(String absoluteFileName, const char *fileName)
{
  #if   defined(PLATFORM_LINUX)
    char *buffer;
  #elif defined(PLATFORM_WINDOWS)
    char *buffer;
  #endif /* PLATFORM_... */

  assert(absoluteFileName != NULL);
  assert(fileName != NULL);

  #if   defined(PLATFORM_LINUX)
    buffer = realpath(fileName,NULL);
    String_setCString(absoluteFileName,buffer);
    free(buffer);
  #elif defined(PLATFORM_WINDOWS)
    buffer = _fullpath(NULL,fileName,0);
    String_setCString(absoluteFileName,buffer);
    // replace brain dead '\'
    String_replaceAllChar(absoluteFileName,STRING_BEGIN,'\\',FILE_PATH_SEPARATOR_CHAR);
    free(buffer);
  #endif /* PLATFORM_... */

  return absoluteFileName;
}

void File_splitFileName(ConstString fileName, String *pathName, String *baseName)
{
  assert(fileName != NULL);
  assert(pathName != NULL);
  assert(baseName != NULL);

  (*pathName) = File_getDirectoryName(File_newFileName(),fileName);
  (*baseName) = File_getBaseName(File_newFileName(),fileName);
}

void File_initSplitFileName(StringTokenizer *stringTokenizer, ConstString fileName)
{
  assert(stringTokenizer != NULL);
  assert(fileName != NULL);

  String_initTokenizer(stringTokenizer,fileName,STRING_BEGIN,FILE_PATH_SEPARATOR_CHARS,NULL,FALSE);
}

void File_doneSplitFileName(StringTokenizer *stringTokenizer)
{
  assert(stringTokenizer != NULL);

  String_doneTokenizer(stringTokenizer);
}

bool File_getNextSplitFileName(StringTokenizer *stringTokenizer, ConstString *name)
{
  assert(stringTokenizer != NULL);
  assert(name != NULL);

  return String_getNextToken(stringTokenizer,name,NULL);
}

/*---------------------------------------------------------------------*/

String File_getSystemDirectory(String path, FileSystemPathTypes fileSystemPathType, ConstString subDirectory)
{
  return File_getSystemDirectoryCString(path,fileSystemPathType,String_cString(subDirectory));
}

String File_getSystemDirectoryCString(String path, FileSystemPathTypes fileSystemPathType, const char *subDirectory)
{
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    WCHAR       *data;
    static char buffer[4*MAX_PATH+1];
    int         bufferLength;
  #endif /* PLATFORM_... */

  assert(path != NULL);

  #if   defined(PLATFORM_LINUX)
    switch (fileSystemPathType)
    {
      case FILE_SYSTEM_PATH_ROOT:
        String_setCString(path,"/");
        break;
      case FILE_SYSTEM_PATH_TMP:
        String_setCString(path,getenv("TMPDIR"));
        if (String_isEmpty(path)) String_setCString(path,getenv("TMP"));
        if (String_isEmpty(path)) String_setCString(path,"/tmp");
        break;
      case FILE_SYSTEM_PATH_CONFIGURATION:
        String_setCString(path,CONFIG_DIR);
        break;
      case FILE_SYSTEM_PATH_RUNTIME:
        String_setCString(path,RUNTIME_DIR);
        break;
      case FILE_SYSTEM_PATH_TLS:
        String_setCString(path,TLS_DIR);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; /* not reached */
    }
  #elif defined(PLATFORM_WINDOWS)
    bufferLength = 0;
    switch (fileSystemPathType)
    {
      case FILE_SYSTEM_PATH_ROOT:
// TODO: current drive?
        String_setCString(path,"C:/");
        break;
      case FILE_SYSTEM_PATH_TMP:
        bufferLength = GetTempPath(sizeof(buffer),buffer);
        if (bufferLength == 0)
        {
          String_setCString(path,"C:\\tmp");
        }
        break;
      case FILE_SYSTEM_PATH_CONFIGURATION:
        SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &data);
        bufferLength = WideCharToMultiByte(CP_UTF8,0,data,lstrlenW(data),buffer,sizeof(buffer),NULL,NULL);
        CoTaskMemFree(data);
        break;
      case FILE_SYSTEM_PATH_RUNTIME:
        SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &data);
        bufferLength = WideCharToMultiByte(CP_UTF8,0,data,lstrlenW(data),buffer,sizeof(buffer),NULL,NULL);
        CoTaskMemFree(data);
        break;
      case FILE_SYSTEM_PATH_TLS:
        SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &data);
        bufferLength = WideCharToMultiByte(CP_UTF8,0,data,lstrlenW(data),buffer,sizeof(buffer),NULL,NULL);
        CoTaskMemFree(data);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; /* not reached */
    }

    // skip trailing \\ if Windows added it (Note: Windows should not try to be smart - it cannot...)
    while ((bufferLength > 0) && buffer[bufferLength-1] == '\\')
    {
      bufferLength--;
    }

    String_setBuffer(path,buffer,bufferLength);
  #endif /* PLATFORM_... */
  if (subDirectory != NULL) File_appendFileNameCString(path,subDirectory);

  return path;
}

#ifdef NDEBUG
Errors File_getTmpFile(FileHandle  *fileHandle,
                       ConstString prefix,
                       ConstString directory
                      )
#else /* not NDEBUG */
Errors __File_getTmpFile(const char  *__fileName__,
                         ulong       __lineNb__,
                         FileHandle  *fileHandle,
                         ConstString prefix,
                         ConstString directory
                        )
#endif /* NDEBUG */
{
  #ifdef NDEBUG
    return File_getTmpFileCString(fileHandle,String_cString(prefix),String_cString(directory));
  #else /* not NDEBUG */
    return __File_getTmpFileCString(__fileName__,__lineNb__,fileHandle,String_cString(prefix),String_cString(directory));
  #endif /* NDEBUG */
}

#ifdef NDEBUG
Errors File_getTmpFileCString(FileHandle *fileHandle,
                              char const *prefix,
                              const char *directory
                             )
#else /* not NDEBUG */
Errors __File_getTmpFileCString(const char *__fileName__,
                                ulong      __lineNb__,
                                FileHandle *fileHandle,
                                char const *prefix,
                                const char *directory
                               )
#endif /* NDEBUG */
{
  String  name;
  int     handle;
  Errors  error;
  #ifndef NDEBUG
    DebugFileNode *debugFileNode;
  #endif /* not NDEBUG */

  assert(fileHandle != NULL);

  if (prefix == NULL) prefix = "tmp";

  // get directory
  if (!stringIsEmpty(directory))
  {
    name = String_newCString(directory);
  }
  else
  {
    name = File_getSystemDirectory(String_new(),FILE_SYSTEM_PATH_TMP,NULL);
  }
  if (!File_exists(name))
  {
    error = ERRORX_(DIRECTORY_NOT_FOUND_,0,"%s",String_cString(name));
    String_delete(name);
    return error;
  }
  if (!String_isEmpty(name))
  {
    String_appendCString(name,FILE_PATH_SEPARATOR_STRING);
  }
  String_appendCString(name,prefix);
  String_appendCString(name,"-XXXXXX");

  // create temporary file
  #ifdef HAVE_MKSTEMP
    handle = mkstemp((char*)String_cString(name));
    if (handle == -1)
    {
      error = getLastError(ERROR_CODE_IO,String_cString(name));
      String_delete(name);
      return error;
    }
    fileHandle->file = fdopen(handle,"w+b");
    if (fileHandle->file == NULL)
    {
      error = getLastError(ERROR_CODE_CREATE_FILE,String_cString(name));
      close(handle);
      (void)unlink(String_cString(name));
      String_delete(name);
      return error;
    }
  #elif HAVE_MKTEMP
    // Note: there is a race-condition when mktemp() and open() is used!
    if (stringIsEmpty(mktemp(String_cString(name))))
    {
      error = getLastError(ERROR_CODE_IO,String_cString(name));
      String_delete(name);
      return error;
    }
    fileHandle->file = FOPEN(s,"w+b");
    if (fileHandle->file == NULL)
    {
      error = getLastError(ERROR_CODE_CREATE_FILE,String_cString(name));
      (void)unlink(String_cString(s))
      String_delete(name);
      return error;
    }
  #else /* not HAVE_MKSTEMP || HAVE_MKTEMP */
    #error mkstemp() nor mktemp() available
  #endif /* HAVE_MKSTEMP || HAVE_MKTEMP */

  // remove file from directory (finally deleted on close)
  #ifdef NDEBUG
    if (unlink(String_cString(name)) != 0)
    {
      error = getLastError(ERROR_CODE_IO,String_cString(name));
      String_delete(name);
      return error;
    }
  #else /* not NDEBUG */
    fileHandle->deleteOnCloseFlag = TRUE;
  #endif /* NDEBUG */

  fileHandle->name  = name;
  fileHandle->mode  = 0;
  fileHandle->index = 0LL;
  fileHandle->size  = 0LL;
  StringList_init(&fileHandle->lineBufferList);

  #ifndef NDEBUG
    pthread_once(&debugFileInitFlag,debugFileInit);

    pthread_mutex_lock(&debugFileLock);
    {
      // check if file is already in open-list
      debugFileNode = debugOpenFileList.head;
      while ((debugFileNode != NULL) && (debugFileNode->fileHandle != fileHandle))
      {
        debugFileNode = debugFileNode->next;
      }
      if (debugFileNode != NULL)
      {
        #ifdef HAVE_BACKTRACE
          debugDumpStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugFileNode->stackTrace,debugFileNode->stackTraceSize,0);
        #endif /* HAVE_BACKTRACE */
        if (debugFileNode->fileHandle->name != NULL)
        {
          HALT_INTERNAL_ERROR("File '%s' at %s, line %lu opened multiple times at %s, line %lu",
                              String_cString(debugFileNode->fileHandle->name),
                              debugFileNode->fileName,
                              debugFileNode->lineNb,
                              __fileName__,
                              __lineNb__
                             );
        }
        else
        {
          HALT_INTERNAL_ERROR("File %p at %s, line %lu opened multiple times at %s, line %lu",
                              debugFileNode->fileHandle,
                              debugFileNode->fileName,
                              debugFileNode->lineNb,
                              __fileName__,
                              __lineNb__
                             );
        }
      }

      // find file in closed-list; reuse or allocate new debug node
      debugFileNode = debugClosedFileList.head;
      while ((debugFileNode != NULL) && (debugFileNode->fileHandle != fileHandle))
      {
        debugFileNode = debugFileNode->next;
      }
      if (debugFileNode != NULL)
      {
        List_remove(&debugClosedFileList,debugFileNode);
      }
      else
      {
        debugFileNode = LIST_NEW_NODE(DebugFileNode);
        if (debugFileNode == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }
      }

      // init file node
      debugFileNode->fileName              = __fileName__;
      debugFileNode->lineNb                = __lineNb__;
      #ifdef HAVE_BACKTRACE
        debugFileNode->stackTraceSize      = getStackTrace(debugFileNode->stackTrace,SIZE_OF_ARRAY(debugFileNode->stackTrace));
      #endif /* HAVE_BACKTRACE */
      debugFileNode->closeFileName         = NULL;
      debugFileNode->closeLineNb           = 0;
      #ifdef HAVE_BACKTRACE
        debugFileNode->closeStackTraceSize = 0;
      #endif /* HAVE_BACKTRACE */
      debugFileNode->fileHandle            = fileHandle;

      // add file to open-list
      List_append(&debugOpenFileList,debugFileNode);
    }
    pthread_mutex_unlock(&debugFileLock);
  #endif /* not NDEBUG */

  return ERROR_NONE;
}

Errors File_getTmpFileName(String      fileName,
                           const char  *prefix,
                           ConstString directory
                          )
{
  return File_getTmpFileNameCString(fileName,prefix,String_cString(directory));
}

Errors File_getTmpFileNameCString(String     fileName,
                                  const char *prefix,
                                  const char *directory
                                 )
{
  uint   n;
  char   *s;
  int    handle;
  Errors error;

  assert(fileName != NULL);

  if (prefix == NULL) prefix = "tmp";

  // get directory
  if (stringIsEmpty(directory)) directory = "/tmp";
  if (!File_existsCString(directory))
  {
    return ERRORX_(DIRECTORY_NOT_FOUND_,0,"%s",directory);
  }

  // get template
  n = stringLength(directory)+stringLength(FILE_PATH_SEPARATOR_STRING)+stringLength(prefix)+7+1;
  s = (char*)malloc(n);
  if (s == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  stringSet(s,n,directory);
  stringAppend(s,n,FILE_PATH_SEPARATOR_STRING);
  stringAppend(s,n,prefix);
  stringAppend(s,n,"-XXXXXX");

  // create temporary file
  #ifdef HAVE_MKSTEMP
    handle = mkstemp(s);
    if (handle == -1)
    {
      error = getLastError(ERROR_CODE_IO,s);
      free(s);
      return error;
    }
    close(handle);
  #elif HAVE_MKTEMP
    // Note: there is a race-condition when mktemp() and open() is used!
    if (stringIsEmpty(mktemp(s)))
    {
      error = getLastError(ERROR_CODE_IO,s);
      free(s);
      return error;
    }
    handle = open(s,O_CREAT|O_EXCL|O_BINARY);
    if (handle == -1)
    {
      error = getLastError(ERROR_CODE_IO,s);
      free(s);
      return error;
    }
    close(handle);
  #else /* not HAVE_MKSTEMP || HAVE_MKTEMP */
    #error mkstemp() nor mktemp() available
  #endif /* HAVE_MKSTEMP || HAVE_MKTEMP */

  String_setBuffer(fileName,s,stringLength(s));

  free(s);

  return ERROR_NONE;
}

Errors File_getTmpDirectoryName(String      directoryName,
                                const char  *prefix,
                                ConstString directory
                               )
{
  return File_getTmpDirectoryNameCString(directoryName,prefix,String_cString(directory));
}

Errors File_getTmpDirectoryNameCString(String     directoryName,
                                       const char *prefix,
                                       const char *directory
                                      )
{
  #if   defined(PLATFORM_LINUX)
    String templateName;
    char   *name;
    #ifdef HAVE_MKDTEMP
    #elif HAVE_MKTEMP
      #if (MKDIR_ARGUMENTS_COUNT == 2)
        mode_t currentCreationMask;
      #endif /* MKDIR_ARGUMENTS_COUNT == 2 */
    #endif /* HAVE_MKSTEMP || HAVE_MKTEMP */
  #elif defined(PLATFORM_WINDOWS)
    char     name[MAX_PATH+1];
    int      n;
    FileStat fileStat;
  #endif /* PLATFORM_... */
  Errors error;

  assert(directoryName != NULL);

  if (prefix == NULL) prefix = "tmp";

  // get directory
  if (stringIsEmpty(directory)) directory = "/tmp";
  if (!File_existsCString(directory))
  {
    return ERRORX_(DIRECTORY_NOT_FOUND_,0,"%s",directory);
  }

  #if   defined(PLATFORM_LINUX)
    if (!stringIsEmpty(directory))
    {
      templateName = String_newCString(directory);
    }
    else
    {
      templateName = File_getSystemDirectory(String_new(),FILE_SYSTEM_PATH_TMP,NULL);
    }
    String_appendCString(templateName,FILE_PATH_SEPARATOR_STRING);
    String_appendCString(templateName,prefix);
    String_appendCString(templateName,"-XXXXXX");

    name = String_toCString(templateName);
    if (name == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    #ifdef HAVE_MKDTEMP
      if (mkdtemp(name) == NULL)
      {
        error = getLastError(ERROR_CODE_IO,name);
        free(name);
        return error;
      }
    #elif HAVE_MKTEMP
      // Note: there is a race-condition when mktemp() and mkdir() is used!
      if (stringIsEmpty(mktemp(name)))
      {
        error = getLastError(ERROR_CODE_IO,name);
        free(name);
        return error;
      }

      #if   (MKDIR_ARGUMENTS_COUNT == 1)
        // create directory
        if (mkdir(name) != 0)
        {
          error = getLastError(ERROR_CODE_IO,name);
          free(name);
          return error;
        }
      #elif (MKDIR_ARGUMENTS_COUNT == 2)
        // get current umask (get and restore current value)
        currentCreationMask = umask(0);
        umask(currentCreationMask);

        // create directory
        if (mkdir(name,0777 & ~currentCreationMask) != 0)
        {
          error = getLastError(ERROR_CODE_IO,name);
          free(name);
          return error;
        }
      #else
        #error unknown number of arguments for mkdir()
      #endif /* MKDIR_ARGUMENTS_COUNT == ... */
    #else /* not HAVE_MKSTEMP || HAVE_MKTEMP */
      #error mkstemp() nor mktemp() available
    #endif /* HAVE_MKSTEMP || HAVE_MKTEMP */

    String_setBuffer(directoryName,name,stringLength(name));

    free(name);
    String_delete(templateName);
  #elif defined(PLATFORM_WINDOWS)
    UNUSED_VARIABLE(directory);

    // Note: there is no Win32 function to create a temporary directory? Poor Windows...
    do
    {
      if (GetTempPath(sizeof(name),name) == 0)
      {
        return getLastError(ERROR_CODE_IO,name);
      }
      n = rand();
      stringFormatAppend(name,sizeof(name),"%s-%06d",prefix,n);
    }
    while (LSTAT(name,&fileStat) == 0);

    #if   (MKDIR_ARGUMENTS_COUNT == 1)
      // create directory
      if (mkdir(name) != 0)
      {
        error = getLastError(ERROR_CODE_IO,name);
        return error;
      }
    #elif (MKDIR_ARGUMENTS_COUNT == 2)
      // get current umask (get and restore current value)
      currentCreationMask = umask(0);
      umask(currentCreationMask);

      // create directory
      if (mkdir(name,0777 & ~currentCreationMask) != 0)
      {
        error = getLastError(ERROR_CODE_IO,name);
        return error;
      }
    #else
      #error unknown number of arguments for mkdir()
    #endif /* MKDIR_ARGUMENTS_COUNT == ... */

    String_setBuffer(directoryName,name,stringLength(name));
  #endif /* PLATFORM_... */

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

const char *File_fileTypeToString(FileTypes fileType, const char *defaultValue)
{
  return ((ARRAY_FIRST(FILE_TYPES).fileType <= fileType) && (fileType <= ARRAY_LAST(FILE_TYPES).fileType))
           ? FILE_TYPES[fileType-ARRAY_FIRST(FILE_TYPES).fileType].name
           : defaultValue;
}

bool File_parseFileType(const char *name, FileTypes *fileType)
{
  uint i;

  assert(name != NULL);
  assert(fileType != NULL);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(FILE_TYPES))
         && !stringEqualsIgnoreCase(FILE_TYPES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(FILE_TYPES))
  {
    (*fileType) = FILE_TYPES[i].fileType;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

const char *File_fileSpecialTypeToString(FileSpecialTypes fileSpecialType, const char *defaultValue)
{
  return ((ARRAY_FIRST(FILE_SPECIAL_TYPES).fileSpecialType <= fileSpecialType) && (fileSpecialType <= ARRAY_LAST(FILE_SPECIAL_TYPES).fileSpecialType))
           ? FILE_SPECIAL_TYPES[fileSpecialType-ARRAY_FIRST(FILE_SPECIAL_TYPES).fileSpecialType].name
           : defaultValue;
}

bool File_parseFileSpecialType(const char *name, FileSpecialTypes *fileSpecialType)
{
  uint i;

  assert(name != NULL);
  assert(fileSpecialType != NULL);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(FILE_SPECIAL_TYPES))
         && !stringEqualsIgnoreCase(FILE_SPECIAL_TYPES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(FILE_SPECIAL_TYPES))
  {
    (*fileSpecialType) = FILE_SPECIAL_TYPES[i].fileSpecialType;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
Errors File_open(FileHandle   *fileHandle,
                 ConstString  fileName,
                 FileModes    fileMode
                )
#else /* not NDEBUG */
Errors __File_open(const char  *__fileName__,
                   ulong       __lineNb__,
                   FileHandle  *fileHandle,
                   ConstString fileName,
                   FileModes   fileMode
                  )
#endif /* NDEBUG */
{
  #ifdef NDEBUG
    return File_openCString(fileHandle,
                            String_cString(fileName),
                            fileMode
                           );
  #else /* not NDEBUG */
    return __File_openCString(__fileName__,
                              __lineNb__,
                              fileHandle,
                              String_cString(fileName),
                              fileMode
                             );
  #endif /* NDEBUG */
}

#ifdef NDEBUG
Errors File_openCString(FileHandle *fileHandle,
                        const char *fileName,
                        FileModes  fileMode
                       )
#else /* not NDEBUG */
Errors __File_openCString(const char *__fileName__,
                          ulong      __lineNb__,
                          FileHandle *fileHandle,
                          const char *fileName,
                          FileModes  fileMode
                         )
#endif /* NDEBUG */
{
  int    fileDescriptor;
  Errors error;
  String directoryName;
  #if   defined(PLATFORM_LINUX)
    #ifndef HAVE_O_NOATIME
      struct stat fileStat;
    #endif /* not HAVE_O_NOATIME */
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  assert(fileHandle != NULL);
  assert(fileName != NULL);

  switch (fileMode & FILE_OPEN_MASK_MODE)
  {
    case FILE_OPEN_CREATE:
      // create directory if needed
      directoryName = File_getDirectoryNameCString(File_newFileName(),fileName);
      if (!String_isEmpty(directoryName) && !File_exists(directoryName))
      {
        error = File_makeDirectory(directoryName,
                                   FILE_DEFAULT_USER_ID,
                                   FILE_DEFAULT_GROUP_ID,
                                   FILE_DEFAULT_PERMISSIONS,
                                   TRUE
                                  );
        if (error != ERROR_NONE)
        {
          File_deleteFileName(directoryName);
          return error;
        }
      }
      File_deleteFileName(directoryName);

      // create file
      #ifdef HAVE_O_LARGEFILE
        #define FLAGS O_RDWR|O_CREAT|O_TRUNC|O_BINARY|O_LARGEFILE
      #else
        #define FLAGS O_RDWR|O_CREAT|O_TRUNC|O_BINARY
      #endif
      fileDescriptor = open(fileName,FLAGS,0666);
      #undef FLAGS
      if (fileDescriptor == -1)
      {
        return getLastError(ERROR_CODE_CREATE_FILE,fileName);
      }

      // init handle
      #ifdef NDEBUG
        error = initFileHandle(fileHandle,
                               fileDescriptor,
                               fileName,
                               fileMode
                              );
      #else /* not NDEBUG */
        error = initFileHandle(__fileName__,
                               __lineNb__,
                               fileHandle,
                               fileDescriptor,
                               fileName,
                               fileMode
                              );
      #endif /* NDEBUG */
      if (error != ERROR_NONE)
      {
        close(fileDescriptor);
        return error;
      }
      break;
    case FILE_OPEN_READ:
      // open file for reading with support of NO_ATIME
      #ifdef HAVE_O_LARGEFILE
        #define FLAGS O_RDONLY|O_BINARY|O_LARGEFILE
      #else
        #define FLAGS O_RDONLY|O_BINARY
      #endif
      #ifdef HAVE_O_NOATIME
        if ((fileMode & FILE_OPEN_NO_ATIME) != 0)
        {
          // first try with O_NOATIME, then without O_NOATIME
          fileDescriptor = open(fileName,FLAGS|O_NOATIME,0);
          if (fileDescriptor == -1)
          {
            fileDescriptor = open(fileName,FLAGS,0);
          }
        }
        else
        {
          fileDescriptor = open(fileName,FLAGS,0);
        }
        if (fileDescriptor == -1)
        {
          return getLastError(ERROR_CODE_OPEN_FILE,fileName);
        }
      #else /* not HAVE_O_NOATIME */
        fileDescriptor = open(fileName,FLAGS,0);
        if (fileDescriptor == -1)
        {
          return getLastError(ERROR_CODE_OPEN_FILE,fileName);
        }

        #if   defined(PLATFORM_LINUX)
          // store atime
          if ((fileMode & FILE_OPEN_NO_ATIME) != 0)
          {
            if (fstat(fileDescriptor,&fileStat) == 0)
            {
              fileHandle->atime.tv_sec  = fileStat.st_atime;
              #ifdef HAVE_STAT_ATIM_TV_NSEC
                fileHandle->atime.tv_nsec = fileStat.st_atim.tv_nsec;
              #else
                fileHandle->atime.tv_nsec = 0;
              #endif
            }
            else
            {
              fileHandle->atime.tv_sec  = 0;
              fileHandle->atime.tv_nsec = UTIME_OMIT;
            }
          }
        #elif defined(PLATFORM_WINDOWS)
        #endif /* PLATFORM_... */
      #endif /* HAVE_O_NOATIME */
      #undef FLAGS

      // init handle
      #ifdef NDEBUG
        error = initFileHandle(fileHandle,
                               fileDescriptor,
                               fileName,
                               fileMode
                              );
      #else /* not NDEBUG */
        error = initFileHandle(__fileName__,
                               __lineNb__,
                               fileHandle,
                               fileDescriptor,
                               fileName,
                               fileMode
                              );
      #endif /* NDEBUG */
      if (error != ERROR_NONE)
      {
        #ifndef HAVE_O_NOATIME
          if (IS_SET(fileHandle->mode,FILE_OPEN_NO_ATIME))
          {
            (void)setAccessTime(fileHandle->handle,&fileHandle->atime);
          }
        #endif /* not HAVE_O_NOATIME */
        close(fileDescriptor);
        return error;
      }
      break;
    case FILE_OPEN_WRITE:
      // create directory if needed
      directoryName = File_getDirectoryNameCString(File_newFileName(),fileName);
      if (!String_isEmpty(directoryName) && !File_exists(directoryName))
      {
        error = File_makeDirectory(directoryName,
                                   FILE_DEFAULT_USER_ID,
                                   FILE_DEFAULT_GROUP_ID,
                                   FILE_DEFAULT_PERMISSIONS,
                                   TRUE
                                  );
        if (error != ERROR_NONE)
        {
          File_deleteFileName(directoryName);
          return error;
        }
      }
      File_deleteFileName(directoryName);

      // open file for writing
      #ifdef HAVE_O_LARGEFILE
        #define FLAGS O_RDWR|O_CREAT|O_BINARY|O_LARGEFILE
      #else
        #define FLAGS O_RDWR|O_CREAT|O_BINARY
      #endif
      fileDescriptor = open(fileName,FLAGS,0666);
      #undef FLAGS
      if (fileDescriptor == -1)
      {
        return getLastError(ERROR_CODE_OPEN_FILE,fileName);
      }

      // init handle
      #ifdef NDEBUG
        error = initFileHandle(fileHandle,
                               fileDescriptor,
                               fileName,
                               fileMode
                              );
      #else /* not NDEBUG */
        error = initFileHandle(__fileName__,
                               __lineNb__,
                               fileHandle,
                               fileDescriptor,
                               fileName,
                               fileMode
                              );
      #endif /* NDEBUG */
      if (error != ERROR_NONE)
      {
        close(fileDescriptor);
        return error;
      }
      break;
    case FILE_OPEN_APPEND:
      // create directory if needed
      directoryName = File_getDirectoryNameCString(File_newFileName(),fileName);
      if (!String_isEmpty(directoryName) && !File_exists(directoryName))
      {
        error = File_makeDirectory(directoryName,
                                   FILE_DEFAULT_USER_ID,
                                   FILE_DEFAULT_GROUP_ID,
                                   FILE_DEFAULT_PERMISSIONS,
                                   TRUE
                                  );
        if (error != ERROR_NONE)
        {
          File_deleteFileName(directoryName);
          return error;
        }
      }
      File_deleteFileName(directoryName);

      // open file for append
      #ifdef HAVE_O_LARGEFILE
        #define FLAGS O_RDWR|O_CREAT|O_APPEND|O_BINARY|O_LARGEFILE
      #else
        #define FLAGS O_RDWR|O_CREAT|O_APPEND|O_BINARY
      #endif
      fileDescriptor = open(fileName,FLAGS,0666);
      #undef FLAGS
      if (fileDescriptor == -1)
      {
        return getLastError(ERROR_CODE_IO,fileName);
      }

      // init handle
      #ifdef NDEBUG
        error = initFileHandle(fileHandle,
                               fileDescriptor,
                               fileName,
                               fileMode
                              );
      #else /* not NDEBUG */
        error = initFileHandle(__fileName__,
                               __lineNb__,
                               fileHandle,
                               fileDescriptor,
                               fileName,
                               fileMode
                              );
      #endif /* NDEBUG */
      if (error != ERROR_NONE)
      {
        close(fileDescriptor);
        return error;
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

#ifdef NDEBUG
Errors File_openDescriptor(FileHandle *fileHandle,
                           int        fileDescriptor,
                           FileModes  fileMode
                          )
#else /* not NDEBUG */
Errors __File_openDescriptor(const char *__fileName__,
                             ulong      __lineNb__,
                             FileHandle *fileHandle,
                             int        fileDescriptor,
                             FileModes  fileMode
                            )
#endif /* NDEBUG */
{
  int newFileDescriptor;

  assert(fileHandle != NULL);
  assert(fileDescriptor != -1);

  newFileDescriptor = dup(fileDescriptor);
  if (newFileDescriptor == -1)
  {
    return getLastError(ERROR_CODE_IO,"");
  }

  #ifdef NDEBUG
    return initFileHandle(fileHandle,
                          newFileDescriptor,
                          NULL,  // fileName
                          fileMode
                         );
  #else /* not NDEBUG */
    return initFileHandle(__fileName__,
                          __lineNb__,
                          fileHandle,
                          newFileDescriptor,
                          NULL,  // fileName
                          fileMode
                         );
  #endif /* NDEBUG */
}

#ifdef NDEBUG
Errors File_close(FileHandle *fileHandle)
#else /* not NDEBUG */
Errors __File_close(const char *__fileName__,
                    ulong      __lineNb__,
                    FileHandle *fileHandle
                   )
#endif /* NDEBUG */
{
  Errors error;

  FILE_CHECK_VALID(fileHandle);

  error = ERROR_NONE;

  // free caches if requested
  if (IS_SET(fileHandle->mode,FILE_OPEN_NO_CACHE))
  {
    (void)File_dropCaches(fileHandle,0LL,0LL,FALSE);
  }

  // restore access time if no-atime is not not supported
  #ifndef HAVE_O_NOATIME
    if (IS_SET(fileHandle->mode,FILE_OPEN_NO_ATIME))
    {
      if (!setAccessTime(fileHandle->handle,&fileHandle->atime))
      {
        if (error == ERROR_NONE) error = getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
      }
    }
  #endif /* not HAVE_O_NOATIME */

  // close file
  if (error == ERROR_NONE)
  {
    #ifdef NDEBUG
      error = doneFileHandle(fileHandle);
    #else /* not NDEBUG */
      error = doneFileHandle(__fileName__,
                             __lineNb__,
                             fileHandle
                            );
    #endif /* NDEBUG */
  }

  return error;
}

bool File_eof(FileHandle *fileHandle)
{
  int  ch;
  bool eofFlag;

  FILE_CHECK_VALID(fileHandle);

  ch = getc(fileHandle->file);
  if (ch != EOF)
  {
    ungetc(ch,fileHandle->file);
    eofFlag = FALSE;
  }
  else
  {
    eofFlag = TRUE;
  }

  return eofFlag;
}

Errors File_read(FileHandle *fileHandle,
                 void       *buffer,
                 ulong      bufferSize,
                 ulong      *bytesRead
                )
{
  ssize_t n;

  FILE_CHECK_VALID(fileHandle);
  assert(buffer != NULL);

  if (bytesRead != NULL)
  {
    // read as much data as possible
//TODO: not valid
//    assert(IS_SET(fileHandle->mode,FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
    n = fread(buffer,1,bufferSize,fileHandle->file);
    if ((n <= 0) && (ferror(fileHandle->file) != 0))
    {
      return getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
    }
    fileHandle->index += (uint64)n;
//TODO: not valid when file changed in the meantime
//    assert(IS_SET(fileHandle->mode,FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
    (*bytesRead) = n;
  }
  else
  {
    // read all requested data
    errno = 0;
    while (bufferSize > 0L)
    {
      n = fread(buffer,1,bufferSize,fileHandle->file);
      if (n <= 0)
      {
        if (ferror(fileHandle->file) != 0)
        {
          return getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
        }
        else
        {
          return ERROR_END_OF_FILE;
          return getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
        }
      }
      buffer = (byte*)buffer+n;
      bufferSize -= (ulong)n;
      fileHandle->index += (uint64)n;
//TODO: not valid when file changed in the meantime
//      assert(((fileHandle->mode & FILE_STREAM) == FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
    }
  }

  // free caches if requested
  if (IS_SET(fileHandle->mode,FILE_OPEN_NO_CACHE))
  {
    (void)File_dropCaches(fileHandle,0LL,fileHandle->index,FALSE);
  }

  return ERROR_NONE;
}

Errors File_write(FileHandle *fileHandle,
                  const void *buffer,
                  ulong      bufferLength
                 )
{
  ssize_t    n;
  const byte *data;
  size_t     m;


  FILE_CHECK_VALID(fileHandle);
  assert(buffer != NULL);

  if (IS_SET(fileHandle->mode,FILE_SPARSE))
  {
    // write sparse data
    n    = 0;
    data = (const byte*)buffer;
    while (n < (ssize_t)bufferLength)
    {
      // seek over 0-bytes
      m = 0;
      while (((n+m) < (size_t)bufferLength) && (data[n+m] == 0))
      {
        m++;
      }
      if (FSEEK(fileHandle->file,(off_t)fileHandle->index+n+m,SEEK_SET) == -1)
      {
        break;
      }
      n += (ssize_t)m;

      // write non-0-bytes
      m = 0;
      while (((n+m) < (size_t)bufferLength) && (data[n+m] != 0))
      {
        m++;
      }
      if (fwrite(&data[n],1,m,fileHandle->file) != m)
      {
        break;
      }
      n += (ssize_t)m;
    }
  }
  else
  {
    // write all data
    n = fwrite(buffer,1,bufferLength,fileHandle->file);
  }
  if (n > 0)
  {
    fileHandle->index += (uint64)n;
    // Note: real file index may be different, because of buffer in stream object
    // assert(IS_SET(fileHandle->mode,FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
    if (fileHandle->index > fileHandle->size) fileHandle->size = fileHandle->index;
  }
  if (n != (ssize_t)bufferLength)
  {
    return getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));;
  }

  // free caches if requested
  if (IS_SET(fileHandle->mode,FILE_OPEN_NO_CACHE))
  {
    (void)File_dropCaches(fileHandle,0LL,fileHandle->index,TRUE);
  }

  return ERROR_NONE;
}

Errors File_readLine(FileHandle *fileHandle,
                     String     line
                    )
{
  int ch;

  FILE_CHECK_VALID(fileHandle);
  assert(line != NULL);

  String_clear(line);
  if (StringList_isEmpty(&fileHandle->lineBufferList))
  {
    do
    {
      ch = fgetc(fileHandle->file);
      if (ch != EOF)
      {
        fileHandle->index += 1LL;
//TODO: not valid when file changed in the meantime
//        assert(IS_SET(fileHandle->mode,FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
        if (((char)ch != '\n') && ((char)ch != '\r'))
        {
          String_appendChar(line,ch);
        }
      }
      else
      {
        if (!feof(fileHandle->file))
        {
          return getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
        }
      }
    }
    while ((ch != EOF) && ((char)ch != '\r') && ((char)ch != '\n'));
    if      ((char)ch == '\r')
    {
      ch = fgetc(fileHandle->file);
      if (ch != EOF)
      {
        fileHandle->index += 1LL;
//TODO: not valid when file changed in the meantime
//        assert(IS_SET(fileHandle->mode,FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
        if (ch != '\n')
        {
          fileHandle->index -= 1LL;
//TODO: not valid when file changed in the meantime
//          assert(IS_SET(fileHandle->mode,FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
          ungetc(ch,fileHandle->file);
        }
      }
    }
  }
  else
  {
    StringList_removeLast(&fileHandle->lineBufferList,line);
  }

  return ERROR_NONE;
}

Errors File_writeLine(FileHandle  *fileHandle,
                      ConstString line
                     )
{
  Errors error;

  FILE_CHECK_VALID(fileHandle);
  assert(line != NULL);

  error = File_write(fileHandle,String_cString(line),String_length(line));
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = File_write(fileHandle,"\n",1);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors File_printLine(FileHandle *fileHandle,
                      const char *format,
                      ...
                     )
{
  String  line;
  va_list arguments;
  Errors  error;

  FILE_CHECK_VALID(fileHandle);
  assert(format != NULL);

  // initialize variables
  line = String_new();

  // format line
  va_start(arguments,format);
  String_vformat(line,format,arguments);
  va_end(arguments);

  // write line
  error = File_write(fileHandle,String_cString(line),String_length(line));
  if (error != ERROR_NONE)
  {
    String_delete(line);
    return error;
  }
  error = File_write(fileHandle,"\n",1);
  if (error != ERROR_NONE)
  {
    String_delete(line);
    return error;
  }

  // free resources
  String_delete(line);

  return ERROR_NONE;
}

Errors File_transfer(FileHandle *fileHandle,
                     FileHandle *fromFileHandle,
                     int64      length,
                     uint64     *bytesTransfered
                    )
{
  #define BUFFER_SIZE (1024*1024)

  void    *buffer;
  ulong   bufferLength;
  ssize_t n;
  Errors  error;

  FILE_CHECK_VALID(fileHandle);
  FILE_CHECK_VALID(fromFileHandle);

  // allocate transfer buffer
  buffer = (char*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // get number of bytes to transfer
  if (length < 0)
  {
    length = (long)(fromFileHandle->size-fromFileHandle->index);
  }

  // transfer data
  if (bytesTransfered != NULL) (*bytesTransfered) = 0LL;
  while (length > 0LL)
  {
    bufferLength = MIN(length,BUFFER_SIZE);

    n = fread(buffer,1,bufferLength,fromFileHandle->file);
    if (n != (ssize_t)bufferLength)
    {
      error = getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
      free(buffer);
      return error;
    }

    n = fwrite(buffer,1,bufferLength,fileHandle->file);
    if (n != (ssize_t)bufferLength)
    {
      error = getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
      free(buffer);
      return error;
    }

    length -= bufferLength;
    if (bytesTransfered != NULL) (*bytesTransfered) += bufferLength;
  }

  // free caches if requested
  if (IS_SET(fromFileHandle->mode,FILE_OPEN_NO_CACHE))
  {
    (void)File_dropCaches(fromFileHandle,0LL,fromFileHandle->index,TRUE);
  }
  if (IS_SET(fileHandle->mode,FILE_OPEN_NO_CACHE))
  {
    (void)File_dropCaches(fileHandle,0LL,fileHandle->index,TRUE);
  }

  // free resources
  free(buffer);

  return ERROR_NONE;

  #undef BUFFER_SIZE
}

Errors File_flush(FileHandle *fileHandle)
{
  FILE_CHECK_VALID(fileHandle);

  if (fflush(fileHandle->file) != 0)
  {
    return getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
  }

  return ERROR_NONE;
}

bool File_getLine(FileHandle *fileHandle,
                  String     line,
                  uint       *lineNb,
                  const char *commentChars
                 )
{
  bool readFlag;

  FILE_CHECK_VALID(fileHandle);

  String_clear(line);

  readFlag = FALSE;
  do
  {
    if (StringList_isEmpty(&fileHandle->lineBufferList))
    {
      // read next line
      if (   File_eof(fileHandle)
          || (File_readLine(fileHandle,line) != ERROR_NONE)
         )
      {
        break;
      }
    }
    else
    {
      // get next line from line buffer
      StringList_removeLast(&fileHandle->lineBufferList,line);
    }
    if (lineNb != NULL) (*lineNb)++;

    String_trim(line,STRING_WHITE_SPACES);

    // check if non-empty and non-comment
    readFlag =    (commentChars == NULL)
               || (   !String_isEmpty(line)
                   && (stringFindChar(commentChars,String_index(line,STRING_BEGIN)) == -1L)
                  );
  }
  while (!readFlag);

  return readFlag;
}

void File_ungetLine(FileHandle  *fileHandle,
                    ConstString line,
                    uint        *lineNb
                   )
{
  FILE_CHECK_VALID(fileHandle);

  StringList_append(&fileHandle->lineBufferList,line);
  if (lineNb != NULL) (*lineNb)--;
}

uint64 File_getSize(const FileHandle *fileHandle)
{
  FILE_CHECK_VALID(fileHandle);

  return fileHandle->size;
}

Errors File_tell(const FileHandle *fileHandle, uint64 *offset)
{
  off_t n;

  FILE_CHECK_VALID(fileHandle);
  assert(offset != NULL);

  n = FTELL(fileHandle->file);
  if (n == (off_t)(-1))
  {
    return getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
  }
  // Note: real file index may be different, because of buffer in stream object
  // assert(fileHandle->index == (uint64)n);

  (*offset) = fileHandle->index;

  return ERROR_NONE;
}

Errors File_seek(FileHandle *fileHandle,
                 uint64     offset
                )
{
  FILE_CHECK_VALID(fileHandle);

  if (FSEEK(fileHandle->file,(off_t)offset,SEEK_SET) == -1)
  {
    return getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
  }
  fileHandle->index = offset;
//TODO: not valid when file changed in the meantime
//  assert(fileHandle->index == (uint64)FTELL(fileHandle->file));
  if (fileHandle->index > fileHandle->size) fileHandle->size = fileHandle->index;

  return ERROR_NONE;
}

Errors File_truncate(FileHandle *fileHandle,
                     uint64     size
                    )
{
  FILE_CHECK_VALID(fileHandle);

  (void)fflush(fileHandle->file);
  if (ftruncate(fileno(fileHandle->file),(off_t)size) != 0)
  {
    return getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
  }
  if (fileHandle->index > size)
  {
    if (FSEEK(fileHandle->file,(off_t)size,SEEK_SET) == -1)
    {
      return getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
    }
    fileHandle->index = size;
//TODO: not valid when file changed in the meantime
//      assert(fileHandle->index == (uint64)FTELL(fileHandle->file));
  }
  fileHandle->size = size;

  return ERROR_NONE;
}

Errors File_dropCaches(FileHandle *fileHandle,
                       uint64     offset,
                       uint64     length,
                       bool       syncFlag
                      )
{
  #if defined(HAVE_FDATASYNC) || defined(HAVE_POSIX_FADVISE)
    int handle;
  #endif /* defined(HAVE_FDATASYNC) || defined(HAVE_POSIX_FADVISE) */

  FILE_CHECK_VALID(fileHandle);

  (void)fflush(fileHandle->file);
  #if defined(HAVE_FDATASYNC) || defined(HAVE_POSIX_FADVISE)
    handle = fileno(fileHandle->file);
  #else
    UNUSED_VARIABLE(fileHandle);
  #endif /* defined(HAVE_FDATASYNC) || defined(HAVE_POSIX_FADVISE) */

  #ifdef HAVE_FDATASYNC
    if (syncFlag)
    {
      (void)fdatasync(handle);
    }
  #else
    UNUSED_VARIABLE(syncFlag);
  #endif /* HAVE_FDATASYNC */

//TODO: use mincore() and only drop pages which are not used by other processes?
  #ifdef HAVE_POSIX_FADVISE
    if (posix_fadvise(handle,offset,length,POSIX_FADV_DONTNEED) != 0)
    {
      return getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
    }
  #else
    UNUSED_VARIABLE(offset);
    UNUSED_VARIABLE(length);
  #endif /* HAVE_POSIX_FADVISE */

  return ERROR_NONE;
}

Errors File_touch(ConstString fileName)
{
  int handle;

  assert(fileName != NULL);

  #if defined(HAVE_O_NOCTTY) && defined(HAVE_O_NONBLOCK)
    #define FLAGS O_WRONLY|O_CREAT|O_BINARY|O_NOCTTY|O_NONBLOCK
  #else
    #define FLAGS O_WRONLY|O_CREAT|O_BINARY
  #endif
  handle = open(String_cString(fileName),FLAGS,0666);
  #undef FLAGS
  if (handle == -1)
  {
    return getLastError(ERROR_CODE_IO,String_cString(fileName));
  }
  close(handle);

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors File_openRootList(RootListHandle *rootListHandle, bool allMountsFlag)
{
  #if   defined(PLATFORM_LINUX)
    FILE *handle;
    char line[1024];
    char *s,*t;
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  assert(rootListHandle != NULL);

  #if   defined(PLATFORM_LINUX)
    StringList_init(&rootListHandle->fileSystemNames);
    rootListHandle->mounts    = NULL;
    rootListHandle->parseFlag = FALSE;

    if (allMountsFlag)
    {
      // get file system names
      handle = fopen(FILESYSMTES_FILENAME,"r");
      if (handle != NULL)
      {
        while (fgets(line,sizeof(line),handle) != NULL)
        {
          s = line;
          if (isspace(*s))
          {
            while (isspace(*s))
            {
              s++;
            }
            t = s;
            while (!isspace(*t))
            {
              t++;
            }
            (*t) = NUL;
            StringList_appendCString(&rootListHandle->fileSystemNames,s);
          }
        }
        (void)fclose(handle);
      }

      // open mount list
      rootListHandle->mounts = fopen(MOUNTS_FILENAME,"r");
    }
  #elif defined(PLATFORM_WINDOWS)
    UNUSED_VARIABLE(allMountsFlag);

    rootListHandle->logicalDrives = GetLogicalDrives();
    if (rootListHandle->logicalDrives == 0)
    {
      return ERROR_(OPEN_FILE,GetLastError());
    }
    rootListHandle->i = 0;
  #endif /* PLATFORM_... */

  return ERROR_NONE;
}

void File_closeRootList(RootListHandle *rootListHandle)
{
  assert(rootListHandle != NULL);

  #if   defined(PLATFORM_LINUX)
    if (rootListHandle->mounts != NULL) fclose(rootListHandle->mounts);
    StringList_done(&rootListHandle->fileSystemNames);
  #elif defined(PLATFORM_WINDOWS)
    UNUSED_VARIABLE(rootListHandle);
  #endif /* PLATFORM_... */
}

bool File_endOfRootList(RootListHandle *rootListHandle)
{
  assert(rootListHandle != NULL);

  #if   defined(PLATFORM_LINUX)
    if (rootListHandle->mounts != NULL)
    {
      while (   !rootListHandle->parseFlag
             && (fgets(rootListHandle->line,sizeof(rootListHandle->line),rootListHandle->mounts) != NULL)
            )
      {
        rootListHandle->parseFlag = parseMountListEntry(rootListHandle->name,sizeof(rootListHandle->name),rootListHandle->line,&rootListHandle->fileSystemNames);
      }

      return !rootListHandle->parseFlag;
    }
    else
    {
      return rootListHandle->parseFlag;
    }
  #elif defined(PLATFORM_WINDOWS)
    return ((rootListHandle->logicalDrives >> rootListHandle->i) <= 0);
  #endif /* PLATFORM_... */
}

Errors File_readRootList(RootListHandle *rootListHandle,
                         String         rootName
                        )
{
  assert(rootListHandle != NULL);

  #if   defined(PLATFORM_LINUX)
    if (rootListHandle->mounts != NULL)
    {
      while (   !rootListHandle->parseFlag
             && (fgets(rootListHandle->line,sizeof(rootListHandle->line),rootListHandle->mounts) != NULL)
            )
      {
        rootListHandle->parseFlag = parseMountListEntry(rootListHandle->name,sizeof(rootListHandle->name),rootListHandle->line,&rootListHandle->fileSystemNames);
      }
      if (!rootListHandle->parseFlag)
      {
        return ERROR_END_OF_FILE;
      }

      String_setCString(rootName,rootListHandle->name);

      rootListHandle->parseFlag = FALSE;
    }
    else
    {
      String_setCString(rootName,"/");

      rootListHandle->parseFlag = TRUE;
    }
  #elif defined(PLATFORM_WINDOWS)
    while (   ((rootListHandle->logicalDrives >> rootListHandle->i) > 0)
           && (((1UL << rootListHandle->i) & rootListHandle->logicalDrives) == 0)
          )
    {
      rootListHandle->i++;
    }

    if (((1UL << rootListHandle->i) & rootListHandle->logicalDrives) != 0)
    {
      String_format(rootName,"%c:",'A'+rootListHandle->i);

      rootListHandle->i++;
    }
  #endif /* PLATFORM_... */

  return ERROR_NONE;
}

Errors File_openDirectoryList(DirectoryListHandle *directoryListHandle,
                              ConstString         directoryName
                             )
{
  assert(directoryListHandle != NULL);
  assert(directoryName != NULL);

  return File_openDirectoryListCString(directoryListHandle,String_cString(directoryName));
}

Errors File_openDirectoryListCString(DirectoryListHandle *directoryListHandle,
                                     const char          *directoryName
                                    )
{
  Errors error;
  #if   defined(PLATFORM_LINUX)
    #if defined(HAVE_FDOPENDIR) && defined(HAVE_O_DIRECTORY)
      const char *s;
      #ifdef HAVE_O_NOATIME
        int    handle;
      #else /* not HAVE_O_NOATIME */
        struct stat fileStat;
      #endif /* HAVE_O_NOATIME */
    #endif /* defined(HAVE_FDOPENDIR) && defined(HAVE_O_DIRECTORY) */
  #elif defined(PLATFORM_WINDOWS)
    String s;
  #endif /* PLATFORM_... */

  assert(directoryListHandle != NULL);
  assert(directoryName != NULL);

  #if   defined(PLATFORM_LINUX)
    #if defined(HAVE_FDOPENDIR) && defined(HAVE_O_DIRECTORY)
      s = !stringIsEmpty(directoryName) ? directoryName : ".";
      #ifdef HAVE_O_NOATIME
        // open directory (try first with O_NOATIME)
        handle = open(s,O_RDONLY|O_BINARY|O_NOCTTY|O_DIRECTORY|O_NOATIME,0);
        if (handle == -1)
        {
          handle = open(s,O_RDONLY|O_BINARY|O_NOCTTY|O_DIRECTORY,0);
        }
        if (handle != -1)
        {
          // create directory handle
          directoryListHandle->dir = fdopendir(handle);
        }
        else
        {
          directoryListHandle->dir = NULL;
        }
      #else /* not HAVE_O_NOATIME */
        // open directory
        directoryListHandle->handle = open(s,O_RDONLY|O_BINARY|O_NOCTTY|O_DIRECTORY,0);
        if (directoryListHandle->handle != -1)
        {
          // store atime
          if (fstat(directoryListHandle->handle,&stat) == 0)
          {
            directoryListHandle->atime.tv_sec  = stat.st_atime;
            #ifdef HAVE_STAT_ATIM_TV_NSEC
              directoryListHandle->atime.tv_nsec = stat.st_atim.tv_nsec;
            #else
              directoryListHandle->atime.tv_nsec = 0;
            #endif
          }
          else
          {
            directoryListHandle->atime.tv_sec  = 0;
            directoryListHandle->atime.tv_nsec = UTIME_OMIT;
          }

          // create directory handle
          directoryListHandle->dir = fdopendir(directoryListHandle->handle);
        }
      #endif /* HAVE_O_NOATIME */
    #else /* not HAVE_FDOPENDIR && HAVE_O_DIRECTORY */
      directoryListHandle->dir = opendir(directoryName);
    #endif /* HAVE_FDOPENDIR && HAVE_O_DIRECTORY */
  #elif defined(PLATFORM_WINDOWS)
    // Note: on Windows <drive>: and <drive>:/ are different, but <path> and <path>/ are the same...
    s = String_newCString(directoryName);
    if (!String_endsWithChar(s,FILE_PATH_SEPARATOR_CHAR)) String_appendChar(s,FILE_PATH_SEPARATOR_CHAR);
    directoryListHandle->dir = opendir(String_cString(s));
    String_delete(s);
  #endif /* PLATFORM_... */
  if (directoryListHandle->dir == NULL)
  {
    error = getLastError(ERROR_CODE_OPEN_DIRECTORY,directoryName);

    #if defined(HAVE_FDOPENDIR) && defined(HAVE_O_DIRECTORY)
      #ifndef HAVE_O_NOATIME
        (void)setAccessTime(directoryListHandle->handle,&directoryListHandle->atime);
      #endif /* not HAVE_O_NOATIME */
    #endif /* HAVE_FDOPENDIR && HAVE_O_DIRECTORY */

    return error;
  }

  directoryListHandle->basePath = String_newCString(directoryName);
  directoryListHandle->entry    = NULL;

  return ERROR_NONE;
}

void File_closeDirectoryList(DirectoryListHandle *directoryListHandle)
{
  assert(directoryListHandle != NULL);
  assert(directoryListHandle->basePath != NULL);
  assert(directoryListHandle->dir != NULL);

  #if   defined(PLATFORM_LINUX)
    // restore access time
    #if defined(HAVE_FDOPENDIR) && defined(HAVE_O_DIRECTORY)
      #ifndef HAVE_O_NOATIME
        (void)setAccessTime(directoryListHandle->handle,&directoryListHandle->atime);
      #endif /* not HAVE_O_NOATIME */
    #endif /* HAVE_FDOPENDIR && HAVE_O_DIRECTORY */
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  // close directory
  closedir(directoryListHandle->dir);

  // free resources
  File_deleteFileName(directoryListHandle->basePath);
}

bool File_endOfDirectoryList(DirectoryListHandle *directoryListHandle)
{
  assert(directoryListHandle != NULL);
  assert(directoryListHandle->basePath != NULL);
  assert(directoryListHandle->dir != NULL);

  // read entry iff not read
  if (directoryListHandle->entry == NULL)
  {
    directoryListHandle->entry = readdir(directoryListHandle->dir);
  }

  // skip "." and ".." entries
  while (   (directoryListHandle->entry != NULL)
         && (   (stringEquals(directoryListHandle->entry->d_name,"." ))
             || (stringEquals(directoryListHandle->entry->d_name,".."))
            )
        )
  {
    directoryListHandle->entry = readdir(directoryListHandle->dir);
  }

  return directoryListHandle->entry == NULL;
}

Errors File_readDirectoryList(DirectoryListHandle *directoryListHandle,
                              String              fileName
                             )
{
  assert(directoryListHandle != NULL);
  assert(directoryListHandle->basePath != NULL);
  assert(directoryListHandle->dir != NULL);
  assert(fileName != NULL);

  // read entry iff not read
  if (directoryListHandle->entry == NULL)
  {
    directoryListHandle->entry = readdir(directoryListHandle->dir);
  }

  // skip "." and ".." entries
  while (   (directoryListHandle->entry != NULL)
         && (   (stringEquals(directoryListHandle->entry->d_name,"." ))
             || (stringEquals(directoryListHandle->entry->d_name,".."))
            )
        )
  {
    directoryListHandle->entry = readdir(directoryListHandle->dir);
  }
  if (directoryListHandle->entry == NULL)
  {
    return getLastError(ERROR_CODE_IO,String_cString(fileName));
  }

  // get entry name
  String_set(fileName,directoryListHandle->basePath);
  File_appendFileNameCString(fileName,directoryListHandle->entry->d_name);

  // mark entry read
  directoryListHandle->entry = NULL;

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

FilePermissions File_stringToPermission(const char *string)
{
  FilePermissions permissions;
  uint            n;

  assert(string != NULL);

  permissions = FILE_PERMISSION_NONE;

  n = stringLength(string);
  if ((n >= 1) && (toupper(string[0]) == 'R')) permissions |= FILE_PERMISSION_USER_READ;
  if ((n >= 2) && (toupper(string[1]) == 'W')) permissions |= FILE_PERMISSION_USER_WRITE;
  if ((n >= 3) && (toupper(string[2]) == 'X')) permissions |= FILE_PERMISSION_USER_EXECUTE;
  if ((n >= 3) && (toupper(string[2]) == 'S')) permissions |= FILE_PERMISSION_USER_SET_ID;
  if ((n >= 4) && (toupper(string[3]) == 'R')) permissions |= FILE_PERMISSION_GROUP_READ;
  if ((n >= 5) && (toupper(string[4]) == 'W')) permissions |= FILE_PERMISSION_GROUP_WRITE;
  if ((n >= 6) && (toupper(string[5]) == 'X')) permissions |= FILE_PERMISSION_GROUP_EXECUTE;
  if ((n >= 6) && (toupper(string[5]) == 'S')) permissions |= FILE_PERMISSION_GROUP_SET_ID;
  if ((n >= 7) && (toupper(string[6]) == 'R')) permissions |= FILE_PERMISSION_OTHER_READ;
  if ((n >= 8) && (toupper(string[7]) == 'W')) permissions |= FILE_PERMISSION_OTHER_WRITE;
  if ((n >= 9) && (toupper(string[8]) == 'X')) permissions |= FILE_PERMISSION_OTHER_EXECUTE;
  if ((n >= 9) && (toupper(string[8]) == 'T')) permissions |= FILE_PERMISSION_STICKY_BIT;

  return permissions;
}

const char *File_permissionToString(char *string, uint stringSize, FilePermissions permissions)
{
  assert(string != NULL);
  assert(stringSize > 0);

  memset(string,'-',stringSize-1);
  if ((stringSize >= 1) && ((permissions & FILE_PERMISSION_USER_READ    ) != 0)) string[0] = 'r';
  if ((stringSize >= 2) && ((permissions & FILE_PERMISSION_USER_WRITE   ) != 0)) string[1] = 'w';
  if ((stringSize >= 3) && ((permissions & FILE_PERMISSION_USER_EXECUTE ) != 0)) string[2] = 'x';
  if ((stringSize >= 3) && ((permissions & FILE_PERMISSION_USER_SET_ID  ) != 0)) string[2] = 's';
  if ((stringSize >= 4) && ((permissions & FILE_PERMISSION_GROUP_READ   ) != 0)) string[3] = 'r';
  if ((stringSize >= 5) && ((permissions & FILE_PERMISSION_GROUP_WRITE  ) != 0)) string[4] = 'w';
  if ((stringSize >= 6) && ((permissions & FILE_PERMISSION_GROUP_EXECUTE) != 0)) string[5] = 'x';
  if ((stringSize >= 6) && ((permissions & FILE_PERMISSION_GROUP_SET_ID ) != 0)) string[5] = 's';
  if ((stringSize >= 7) && ((permissions & FILE_PERMISSION_OTHER_READ   ) != 0)) string[6] = 'r';
  if ((stringSize >= 8) && ((permissions & FILE_PERMISSION_OTHER_WRITE  ) != 0)) string[7] = 'w';
  if ((stringSize >= 9) && ((permissions & FILE_PERMISSION_OTHER_EXECUTE) != 0)) string[8] = 'x';
  if ((stringSize >= 9) && ((permissions & FILE_PERMISSION_STICKY_BIT   ) != 0)) string[8] = 't';
  string[stringSize-1] = NUL;

  return string;
}

FileTypes File_getType(ConstString fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  if (LSTAT(String_cString(fileName),&fileStat) == 0)
  {
    return getFileType(&fileStat);
  }
  else
  {
    return FILE_TYPE_NONE;
  }
}

FileTypes File_getRealType(ConstString fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  if (STAT(String_cString(fileName),&fileStat) == 0)
  {
    return getFileType(&fileStat);
  }
  else
  {
    return FILE_TYPE_NONE;
  }
}

Errors File_getData(ConstString fileName,
                    void        **data,
                    ulong       *size
                   )
{
  assert(fileName != NULL);
  assert(data != NULL);
  assert(size != NULL);

  return File_getDataCString(String_cString(fileName),
                             data,
                             size
                            );
}

Errors File_getDataCString(const char *fileName,
                           void       **data,
                           ulong      *size
                          )
{
  Errors     error;
  FileHandle fileHandle;
  ulong      bytesRead;

  assert(fileName != NULL);
  assert(data != NULL);
  assert(size != NULL);

  // open file
  error = File_openCString(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get file size
  (*size) = (ulong)File_getSize(&fileHandle);

  // allocate memory for data
  (*data) = malloc(*size);
  if ((*data) == NULL)
  {
    (void)File_close(&fileHandle);
    return ERROR_INSUFFICIENT_MEMORY;
  }

  // read data
  error = File_read(&fileHandle,*data,*size,&bytesRead);
  if (error != ERROR_NONE)
  {
    (void)File_close(&fileHandle);
    return error;
  }
  if (bytesRead != (*size))
  {
    (void)File_close(&fileHandle);
    return ERROR_(IO,0);
  }

  // close file
  (void)File_close(&fileHandle);

  return ERROR_NONE;
}

Errors File_delete(ConstString fileName, bool recursiveFlag)
{
  assert(fileName != NULL);

  return File_deleteCString(String_cString(fileName),recursiveFlag);
}

Errors File_deleteCString(const char *fileName, bool recursiveFlag)
{
  FileStat      fileStat;
  Errors        error;
  StringList    directoryList;
  DIR           *dir;
  struct dirent *entry;
  String        directoryName;
  bool          emptyFlag;
  String        name;

  assert(fileName != NULL);

  if (LSTAT(fileName,&fileStat) != 0)
  {
    return getLastError(ERROR_CODE_IO,fileName);
  }

  if      (   S_ISREG(fileStat.st_mode)
           #ifdef S_ISLNK
           || S_ISLNK(fileStat.st_mode)
           #endif /* S_ISLNK */
          )
  {
    if (unlink(fileName) != 0)
    {
      return getLastError(ERROR_CODE_IO,fileName);
    }
  }
  else if (S_ISDIR(fileStat.st_mode))
  {
    error = ERROR_NONE;
    if (recursiveFlag)
    {
      // delete entries in directory
      StringList_init(&directoryList);
      StringList_appendCString(&directoryList,fileName);
      directoryName = File_newFileName();
      name = File_newFileName();
      while (!StringList_isEmpty(&directoryList) && (error == ERROR_NONE))
      {
        StringList_removeFirst(&directoryList,directoryName);

        dir = opendir(String_cString(directoryName));
        if (dir != NULL)
        {
          emptyFlag = TRUE;
          while (((entry = readdir(dir)) != NULL) && (error == ERROR_NONE))
          {
            if (   (stringEquals(entry->d_name,"." ))
                && (stringEquals(entry->d_name,".."))
               )
            {
              String_set(name,directoryName);
              File_appendFileNameCString(name,entry->d_name);

              if (LSTAT(String_cString(name),&fileStat) == 0)
              {
                if      (   S_ISREG(fileStat.st_mode)
                         #ifdef S_ISLNK
                         || S_ISLNK(fileStat.st_mode)
                         #endif /* S_ISLNK */
                        )
                {
                  if (unlink(String_cString(name)) != 0)
                  {
                    error = getLastError(ERROR_CODE_IO,String_cString(name));
                  }
                }
                else if (S_ISDIR(fileStat.st_mode))
                {
                  StringList_append(&directoryList,name);
                  emptyFlag = FALSE;
                }
              }
            }
          }
          closedir(dir);

          if (emptyFlag)
          {
            if (rmdir(String_cString(directoryName)) != 0)
            {
              error = getLastError(ERROR_CODE_IO,String_cString(directoryName));
            }
          }
          else
          {
            StringList_append(&directoryList,directoryName);
          }
        }
      }
      File_deleteFileName(name);
      File_deleteFileName(directoryName);
      StringList_done(&directoryList);
    }
    else
    {
      if (rmdir(fileName) != 0)
      {
        error = getLastError(ERROR_CODE_IO,fileName);
      }
    }

    return error;
  }

  return ERROR_NONE;
}

Errors File_rename(ConstString oldFileName,
                   ConstString newFileName,
                   ConstString newBackupFileName
                  )
{
  assert(oldFileName != NULL);
  assert(newFileName != NULL);

  return File_renameCString(String_cString(oldFileName),
                            String_cString(newFileName),
                            String_cString(newBackupFileName)
                           );
}

Errors File_renameCString(const char *oldFileName,
                          const char *newFileName,
                          const char *newBackupFileName
                         )
{
  char       *fileName;
  const char *tmpFileName;
  FileStat   fileStat;
  int        handle;
  Errors     error;

  assert(oldFileName != NULL);
  assert(newFileName != NULL);

  fileName = NULL;

  if (LSTAT(newFileName,&fileStat) == 0)
  {
    // get temporary filename
    if (newBackupFileName != NULL)
    {
      // use given backup file name
      tmpFileName = newBackupFileName;
    }
    else
    {
      // create temporary file
      fileName = (char*)malloc(stringLength(newFileName)+7+1);
      if (fileName == NULL)
      {
        return ERROR_INSUFFICIENT_MEMORY;
      }
      stringSet(fileName,stringLength(newFileName)+7+1,newFileName);
      stringAppend(fileName,stringLength(newFileName)+7+1,"-XXXXXX");

      #ifdef HAVE_MKSTEMP
        handle = mkstemp(fileName);
        if (handle == -1)
        {
          error = getLastError(ERROR_CODE_IO,fileName);
          free(fileName);
          return error;
        }
        close(handle);
      #elif HAVE_MKTEMP
        // Note: there is a race-condition when mktemp() and open() is used!
        if (stringIsEmpty(mktemp(fileName)))
        {
          error = getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
          free(tmpFileName);
          return error;
        }
        handle = open(fileName,O_CREAT|O_EXCL|O_BINARY);
        if (handle == -1)
        {
          error = getLastError(ERROR_CODE_IO,String_cString(fileHandle->name));
          free(tmpFileName);
          return error;
        }
        close(handle);
      #else /* not HAVE_MKSTEMP || HAVE_MKTEMP */
        #error mkstemp() nor mktemp() available
      #endif /* HAVE_MKSTEMP || HAVE_MKTEMP */

      tmpFileName = fileName;
    }
  }
  else
  {
    tmpFileName = NULL;
  }

  // try rename/copy new -> backup/temporary name
  if (tmpFileName != NULL)
  {
    if (rename(newFileName,tmpFileName) != 0)
    {
      // copy to backup file
      error = File_copyCString(newFileName,tmpFileName);
      if (error != ERROR_NONE)
      {
        (void)unlink(tmpFileName);
        if (newBackupFileName == NULL) free(fileName);
        return error;
      }

      // delete new file
      if (unlink(newFileName) != 0)
      {
        error = getLastError(ERROR_CODE_IO,newFileName);
        (void)unlink(tmpFileName);
        if (newBackupFileName == NULL) free(fileName);
        return error;
      }
    }
  }

  // try rename/copy old -> new
  if (rename(oldFileName,newFileName) != 0)
  {
    // copy to new file
    error = File_copyCString(oldFileName,newFileName);
    if (error != ERROR_NONE)
    {
      if (tmpFileName != NULL)
      {
        if (rename(tmpFileName,newFileName) != 0)
        {
          (void)File_copyCString(tmpFileName,newFileName);
        }
        (void)unlink(tmpFileName);
        if (newBackupFileName == NULL) free(fileName);
      }
      return error;
    }

    // delete old file
    if (unlink(oldFileName) != 0)
    {
      error = getLastError(ERROR_CODE_IO,oldFileName);
      if (tmpFileName != NULL)
      {
        if (rename(tmpFileName,newFileName) != 0)
        {
          (void)File_copyCString(tmpFileName,newFileName);
        }
        (void)unlink(tmpFileName);
        if (newBackupFileName == NULL) free(fileName);
      }
      return error;
    }
  }

  // free resources
  if (tmpFileName != NULL)
  {
    if (newBackupFileName == NULL)
    {
      (void)unlink(tmpFileName);
      free(fileName);
    }
  }

  return ERROR_NONE;
}

Errors File_copy(ConstString sourceFileName,
                 ConstString destinationFileName
                )
{
  assert(sourceFileName != NULL);
  assert(destinationFileName != NULL);

  return File_copyCString(String_cString(sourceFileName),String_cString(destinationFileName));
}

Errors File_copyCString(const char *sourceFileName,
                        const char *destinationFileName
                       )
{
  #define BUFFER_SIZE (1024*1024)

  byte     *buffer;
  Errors   error;
  FILE     *sourceFile,*destinationFile;
  size_t   n;
  FileStat fileStat;

  assert(sourceFileName != NULL);
  assert(destinationFileName != NULL);

  // allocate buffer
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }

  // open files
  sourceFile = FOPEN(sourceFileName,"rb");
  if (sourceFile == NULL)
  {
    error = getLastError(ERROR_CODE_IO,sourceFileName);
    free(buffer);
    return error;
  }
  destinationFile = FOPEN(destinationFileName,"wb");
  if (destinationFile == NULL)
  {
    error = getLastError(ERROR_CODE_IO,destinationFileName);
    fclose(sourceFile);
    free(buffer);
    return error;
  }

  // copy data
  do
  {
    n = fread(buffer,1,BUFFER_SIZE,sourceFile);
    if (n > 0)
    {
      if (fwrite(buffer,1,n,destinationFile) != n)
      {
        error = getLastError(ERROR_CODE_IO,destinationFileName);
        fclose(destinationFile);
        fclose(sourceFile);
        free(buffer);
        return error;
      }
    }
    else
    {
      if (ferror(sourceFile))
      {
        error = getLastError(ERROR_CODE_IO,sourceFileName);
        fclose(destinationFile);
        fclose(sourceFile);
        free(buffer);
        return error;
      }
    }
  }
  while (n > 0);

  // close files
  fclose(destinationFile);
  fclose(sourceFile);

  // free resources
  free(buffer);

  // copy permissions
  if (LSTAT(sourceFileName,&fileStat) != 0)
  {
    return getLastError(ERROR_CODE_IO,sourceFileName);
  }
  #ifdef HAVE_CHOWN
    if (chown(destinationFileName,fileStat.st_uid,fileStat.st_gid) != 0)
    {
      return getLastError(ERROR_CODE_IO,destinationFileName);
    }
  #endif /* HAVE_CHOWN */
  #ifdef HAVE_CHMOD
    if (chmod(destinationFileName,fileStat.st_mode) != 0)
    {
      return getLastError(ERROR_CODE_IO,destinationFileName);
    }
  #endif /* HAVE_CHMOD */

  return ERROR_NONE;

  #undef BUFFER_SIZE
}

bool File_exists(ConstString fileName)
{
  assert(fileName != NULL);

  return File_existsCString(String_cString(fileName));
}

bool File_existsCString(const char *fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  return !stringIsEmpty(fileName) && (LSTAT(fileName,&fileStat) == 0);
}

bool File_isFile(ConstString fileName)
{
  assert(fileName != NULL);

  return File_isFileCString(String_cString(fileName));
}

bool File_isFileCString(const char *fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  return (   (STAT(fileName,&fileStat) == 0)
          && S_ISREG(fileStat.st_mode)
         );
}

bool File_isDirectory(ConstString fileName)
{
  assert(fileName != NULL);

  return File_isDirectoryCString(String_cString(fileName));
}

bool File_isDirectoryCString(const char *fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  return (   (STAT(fileName,&fileStat) == 0)
          && S_ISDIR(fileStat.st_mode)
         );
}

bool File_isDevice(ConstString fileName)
{
  assert(fileName != NULL);

  return File_isDeviceCString(String_cString(fileName));
}

bool File_isDeviceCString(const char *fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  return (   (STAT(fileName,&fileStat) == 0)
          && (S_ISCHR(fileStat.st_mode) || S_ISBLK(fileStat.st_mode))
         );
}

bool File_isReadable(ConstString fileName)
{
  assert(fileName != NULL);

  return File_isReadableCString(String_cString(fileName));
}

bool File_isReadableCString(const char *fileName)
{
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    int fileDescriptor;
  #endif /* PLATFORM_... */

  assert(fileName != NULL);

  #if   defined(PLATFORM_LINUX)
    return access(fileName,F_OK|R_OK) == 0;
  #elif defined(PLATFORM_WINDOWS)
    // Note: access() does not return correct values on MinGW
    fileDescriptor = open(fileName,O_RDONLY,0);
    if (fileDescriptor != -1)
    {
      close(fileDescriptor);
      return TRUE;
    }
    else
    {
      return FALSE;
    }
  #endif /* PLATFORM_... */
}

bool File_isWritable(ConstString fileName)
{
  assert(fileName != NULL);

  return File_isWritableCString(String_cString(fileName));
}

bool File_isWritableCString(const char *fileName)
{
  bool   isWriteable;
  String directoryPath;
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    DWORD fileAttributes;
  #endif /* PLATFORM_... */

  assert(fileName != NULL);

  isWriteable = FALSE;

  directoryPath = String_new();
  if (!stringIsEmpty(fileName))
  {
    File_getDirectoryNameCString(directoryPath,fileName);
  }
  else
  {
    fileName = ".";
    String_setCString(directoryPath,".");
  }
  #if   defined(PLATFORM_LINUX)
    isWriteable =    (File_existsCString(fileName) && (access(fileName,W_OK) == 0))
                  || (access(String_cString(directoryPath),W_OK) == 0);
  #elif defined(PLATFORM_WINDOWS)
    // Note: access() does not return correct values on MinGW
    if (File_existsCString(fileName))
    {
      fileAttributes = GetFileAttributes(fileName);
    }
    else
    {
      fileAttributes = GetFileAttributes(String_cString(directoryPath));
    }
    isWriteable =    (fileAttributes != INVALID_FILE_ATTRIBUTES)
                  && (   (fileAttributes == FILE_ATTRIBUTE_NORMAL)
                      || ((fileAttributes & FILE_ATTRIBUTE_READONLY) == 0)
                      || ((fileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
                     );
  #endif /* PLATFORM_... */
  String_delete(directoryPath);

  return isWriteable;
}

bool File_isNetworkFileSystem(ConstString fileName)
{
  assert(fileName != NULL);

  return File_isNetworkFileSystemCString(String_cString(fileName));
}

bool File_isHidden(ConstString fileName)
{
  assert(fileName != NULL);

  return File_isHiddenCString(String_cString(fileName));
}

bool File_isHiddenCString(const char *fileName)
{
  long       i;
  const char *s;

  assert(fileName != NULL);

  i = stringFindReverseChar(fileName,FILE_PATH_SEPARATOR_CHAR);
  s = (i >= 0L)
        ? &fileName[i+1]
        : fileName;

  return    !stringEquals(s,".")
         && !stringEquals(s,"..")
         && stringStartsWith(s,".");
}

bool File_isNetworkFileSystemCString(const char *fileName)
{
  bool isNetworkFileSystem;
  #if   defined(PLATFORM_LINUX)
    struct statfs fileSystemStat;
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  assert(fileName != NULL);

  isNetworkFileSystem = FALSE;

  #if   defined(PLATFORM_LINUX)
    if (statfs(fileName,&fileSystemStat) == 0)
    {
      isNetworkFileSystem =    (fileSystemStat.f_type == AFS_SUPER_MAGIC)
                            || (fileSystemStat.f_type == CODA_SUPER_MAGIC)
                            || (fileSystemStat.f_type == NFS_SUPER_MAGIC)
                            || (fileSystemStat.f_type == SMB_SUPER_MAGIC);
    }
  #elif defined(PLATFORM_WINDOWS)
    UNUSED_VARIABLE(fileName);
  #endif /* PLATFORM_... */

  return isNetworkFileSystem;
}

Errors File_getInfo(FileInfo    *fileInfo,
                    ConstString fileName
                   )
{
  assert(fileInfo != NULL);
  assert(fileName != NULL);
  assert(!String_isEmpty(fileName));

  return File_getInfoCString(fileInfo,String_cString(fileName));
}

Errors File_getInfoCString(FileInfo   *fileInfo,
                           const char *fileName
                          )
{
  FileStat   fileStat;
  DeviceInfo deviceInfo;

  assert(fileInfo != NULL);
  assert(fileName != NULL);
  assert(!stringIsEmpty(fileName));

  // get file meta data
  if (LSTAT(fileName,&fileStat) != 0)
  {
    return getLastError(ERROR_CODE_IO,fileName);
  }
  fileInfo->timeLastAccess  = fileStat.st_atime;
  fileInfo->timeModified    = fileStat.st_mtime;
  fileInfo->timeLastChanged = fileStat.st_ctime;
  fileInfo->userId          = fileStat.st_uid;
  fileInfo->groupId         = fileStat.st_gid;
  fileInfo->permissions     = (FilePermissions)fileStat.st_mode;
  #ifdef HAVE_MAJOR
    fileInfo->major         = major(fileStat.st_rdev);
  #else
    fileInfo->major         = 0;
  #endif
  #ifdef HAVE_MINOR
    fileInfo->minor         = minor(fileStat.st_rdev);
  #else
    fileInfo->minor         = 0;
  #endif
  fileInfo->attributes      = FILE_ATTRIBUTE_NONE;
  fileInfo->id              = (uint64)fileStat.st_ino;
  fileInfo->linkCount       = (uint)fileStat.st_nlink;
  fileInfo->cast.mtime      = fileStat.st_mtime;
  fileInfo->cast.ctime      = fileStat.st_ctime;

  // store specific meta data
  if      (S_ISREG(fileStat.st_mode))
  {
    fileInfo->type = (fileStat.st_nlink > 1) ? FILE_TYPE_HARDLINK : FILE_TYPE_FILE;
    fileInfo->size = fileStat.st_size;

    // get file attributes
    (void)File_getAttributesCString(&fileInfo->attributes,fileName);
  }
  else if (S_ISDIR(fileStat.st_mode))
  {
    fileInfo->type = FILE_TYPE_DIRECTORY;
    fileInfo->size = 0LL;

    // get file attributes
    (void)File_getAttributesCString(&fileInfo->attributes,fileName);
  }
  #ifdef S_ISLNK
  else if (S_ISLNK(fileStat.st_mode))
  {
    fileInfo->type = FILE_TYPE_LINK;
    fileInfo->size = 0LL;
  }
  #endif /* S_ISLNK */
  else if (S_ISCHR(fileStat.st_mode))
  {
    fileInfo->type        = FILE_TYPE_SPECIAL;
    fileInfo->size        = 0LL;
    fileInfo->specialType = FILE_SPECIAL_TYPE_CHARACTER_DEVICE;
    fileInfo->attributes  = 0LL;
  }
  else if (S_ISBLK(fileStat.st_mode))
  {
    fileInfo->type        = FILE_TYPE_SPECIAL;
    fileInfo->size        = 0LL;
    fileInfo->specialType = FILE_SPECIAL_TYPE_BLOCK_DEVICE;
    fileInfo->attributes  = 0LL;

    // try to detect block device size
    if (Device_getInfoCString(&deviceInfo,fileName,TRUE) == ERROR_NONE)
    {
      fileInfo->size = deviceInfo.size;
    }
  }
  else if (S_ISFIFO(fileStat.st_mode))
  {
    fileInfo->type        = FILE_TYPE_SPECIAL;
    fileInfo->size        = 0LL;
    fileInfo->specialType = FILE_SPECIAL_TYPE_FIFO;
  }
  #ifdef S_ISSOCK
  else if (S_ISSOCK(fileStat.st_mode))
  {
    fileInfo->type        = FILE_TYPE_SPECIAL;
    fileInfo->size        = 0LL;
    fileInfo->specialType = FILE_SPECIAL_TYPE_SOCKET;
  }
  #endif /* S_ISSOCK */
  else
  {
    fileInfo->type        = FILE_TYPE_UNKNOWN;
    fileInfo->size        = 0LL;
    fileInfo->attributes  = 0LL;
  }

  return ERROR_NONE;
}

// TODO: swap fileInfo, fileName
Errors File_setInfo(const FileInfo *fileInfo,
                    ConstString    fileName
                   )
{
  assert(fileInfo != NULL);
  assert(fileName != NULL);
  assert(!String_isEmpty(fileName));

  return File_setInfoCString(fileInfo,String_cString(fileName));
}

Errors File_setInfoCString(const FileInfo *fileInfo,
                           const char     *fileName
                          )
{
  struct utimbuf utimeBuffer;

  assert(fileInfo != NULL);
  assert(fileName != NULL);
  assert(!stringIsEmpty(fileName));

  // set meta data
  switch (fileInfo->type)
  {
    case FILE_TYPE_FILE:
    case FILE_TYPE_DIRECTORY:
    case FILE_TYPE_HARDLINK:
      #if   defined(PLATFORM_LINUX)
        // set last access, time modified, user/group id
        utimeBuffer.actime  = fileInfo->timeLastAccess;
        utimeBuffer.modtime = fileInfo->timeModified;
        if (utime(fileName,&utimeBuffer) != 0)
        {
          return getLastError(ERROR_CODE_IO,fileName);
        }
      #elif defined(PLATFORM_WINDOWS)
//TODO: implement
      #endif /* PLATFORM_... */

      // set last permissions
      #ifdef HAVE_CHMOD
        if (chmod(fileName,(mode_t)fileInfo->permissions) != 0)
        {
          return getLastError(ERROR_CODE_IO,fileName);
        }
      #endif /* HAVE_CHMOD */
      break;
    case FILE_TYPE_LINK:
      // set user/group id
      break;
    case FILE_TYPE_SPECIAL:
      // set last access, time modified, user/group id, permissions
      utimeBuffer.actime  = fileInfo->timeLastAccess;
      utimeBuffer.modtime = fileInfo->timeModified;
      if (utime(fileName,&utimeBuffer) != 0)
      {
        return getLastError(ERROR_CODE_IO,fileName);
      }
      #ifdef HAVE_CHMOD
        if (chmod(fileName,(mode_t)fileInfo->permissions) != 0)
        {
          return getLastError(ERROR_CODE_IO,fileName);
        }
      #endif /* HAVE_CHMOD */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return ERROR_NONE;
}

Errors File_getAttributes(FileAttributes *fileAttributes,
                          ConstString    fileName
                         )
{
  assert(fileAttributes != NULL);
  assert(fileName != NULL);
  assert(!String_isEmpty(fileName));

  return File_getAttributesCString(fileAttributes,String_cString(fileName));
}

Errors File_getAttributesCString(FileAttributes *fileAttributes,
                                 const char     *fileName
                                )
{
  long   attributes;
  #ifdef FS_IOC_GETFLAGS
    int    handle;
    Errors error;
  #endif /* FS_IOC_GETFLAGS */
  #if defined(FS_IOC_GETFLAGS) && !defined(HAVE_O_NOATIME)
    struct stat     fileStat;
    bool            atimeFlag;
    struct timespec atime;
  #endif /* defined(FS_IOC_GETFLAGS) && !defined(HAVE_O_NOATIME) */

  assert(fileAttributes != NULL);
  assert(fileName != NULL);
  assert(!stringIsEmpty(fileName));

  attributes = 0LL;
  #ifdef FS_IOC_GETFLAGS
    // open file (first try with O_NOATIME)
    #ifdef HAVE_O_NOATIME
      handle = open(fileName,O_RDONLY|O_NONBLOCK|O_BINARY|O_NOATIME);
      if (handle == -1)
      {
        handle = open(fileName,O_RDONLY|O_NONBLOCK|O_BINARY);
      }
    #else /* not HAVE_O_NOATIME */
      handle = open(fileName,O_RDONLY|O_NONBLOCK|O_BINARY);
    #endif /* HAVE_O_NOATIME */
    if (handle == -1)
    {
      return getLastError(ERROR_CODE_IO,fileName);
    }

    #ifndef HAVE_O_NOATIME
      // store atime
      if (fstat(handle,&fileStat) == 0)
      {
        atime.tv_sec    = fileStat.st_atime;
        #ifdef HAVE_STAT_ATIM_TV_NSEC
          atime.tv_nsec = fileStat.st_atim.tv_nsec;
        #else
          atime.tv_nsec = 0;
        #endif
        atimeFlag = TRUE;
      }
      else
      {
        atimeFlag = FALSE;
      }
    #endif /* not HAVE_O_NOATIME */

    // get attributes
    if (ioctl(handle,FS_IOC_GETFLAGS,&attributes) != 0)
    {
      error = getLastError(ERROR_CODE_IO,fileName);
      #ifndef HAVE_O_NOATIME
        if (atimeFlag)
        {
          (void)setAccessTime(handle,&atime);
        }
      #endif /* not HAVE_O_NOATIME */
      close(handle);
      return error;
    }

    #ifndef HAVE_O_NOATIME
      // restore atime
      if (atimeFlag)
      {
        (void)setAccessTime(handle,&atime);
      }
    #endif /* not HAVE_O_NOATIME */

    // close file
    close(handle);
  #else /* not FS_IOC_GETFLAGS */
    UNUSED_VARIABLE(fileName);
  #endif /* FS_IOC_GETFLAGS */

  (*fileAttributes) = 0LL;
  if ((attributes & FILE_ATTRIBUTE_COMPRESS   ) != 0LL) (*fileAttributes) |= FILE_ATTRIBUTE_COMPRESS;
  if ((attributes & FILE_ATTRIBUTE_NO_COMPRESS) != 0LL) (*fileAttributes) |= FILE_ATTRIBUTE_NO_COMPRESS;
  if ((attributes & FILE_ATTRIBUTE_IMMUTABLE  ) != 0LL) (*fileAttributes) |= FILE_ATTRIBUTE_IMMUTABLE;
  if ((attributes & FILE_ATTRIBUTE_APPEND     ) != 0LL) (*fileAttributes) |= FILE_ATTRIBUTE_APPEND;
  if ((attributes & FILE_ATTRIBUTE_NO_DUMP    ) != 0LL) (*fileAttributes) |= FILE_ATTRIBUTE_NO_DUMP;

  return ERROR_NONE;
}

Errors File_setAttributes(FileAttributes fileAttributes,
                          ConstString    fileName
                         )
{
  assert(fileName != NULL);
  assert(!String_isEmpty(fileName));

  return File_setAttributesCString(fileAttributes,String_cString(fileName));
}

Errors File_setAttributesCString(FileAttributes fileAttributes,
                                 const char     *fileName
                                )
{
  #if defined(FS_IOC_GETFLAGS) && defined(FS_IOC_SETFLAGS)
    long   attributes;
    int    handle;
    Errors error;
  #endif /* FS_IOC_GETFLAGS && FS_IOC_SETFLAGS */
  #if defined(FS_IOC_GETFLAGS) && defined(FS_IOC_SETFLAGS)
    #ifndef HAVE_O_NOATIME
//TODO: remove
//      struct stat     fileStat;
//      bool            atimeFlag;
//      struct timespec atime;
    #endif /* not HAVE_O_NOATIME */
  #endif /* defined(FS_IOC_GETFLAGS) && defined(FS_IOC_SETFLAGS) */

  assert(fileName != NULL);
  assert(!stringIsEmpty(fileName));

  #if defined(FS_IOC_GETFLAGS) && defined(FS_IOC_SETFLAGS)
    // open file (first try with O_NOATIME)
    #ifdef HAVE_O_NOATIME
      handle = open(fileName,O_RDONLY|O_NONBLOCK|O_BINARY|O_NOATIME);
      if (handle == -1)
      {
        handle = open(fileName,O_RDONLY|O_NONBLOCK|O_BINARY);
      }
    #else /* not HAVE_O_NOATIME */
      handle = open(fileName,O_RDONLY|O_NONBLOCK|O_BINARY);
    #endif /* HAVE_O_NOATIME */
    if (handle == -1)
    {
      return getLastError(ERROR_CODE_IO,fileName);
    }

    // update attributes
    if (ioctl(handle,FS_IOC_GETFLAGS,&attributes) != 0)
    {
      error = getLastError(ERROR_CODE_IO,fileName);
      close(handle);
      return error;
    }
    attributes &= ~(FILE_ATTRIBUTE_COMPRESS|FILE_ATTRIBUTE_NO_COMPRESS|FILE_ATTRIBUTE_IMMUTABLE|FILE_ATTRIBUTE_APPEND|FILE_ATTRIBUTE_NO_DUMP);
    if ((fileAttributes & FILE_ATTRIBUTE_COMPRESS   ) != 0LL) attributes |= FILE_ATTRIBUTE_COMPRESS;
    if ((fileAttributes & FILE_ATTRIBUTE_NO_COMPRESS) != 0LL) attributes |= FILE_ATTRIBUTE_NO_COMPRESS;
    if ((fileAttributes & FILE_ATTRIBUTE_IMMUTABLE  ) != 0LL) attributes |= FILE_ATTRIBUTE_IMMUTABLE;
    if ((fileAttributes & FILE_ATTRIBUTE_APPEND     ) != 0LL) attributes |= FILE_ATTRIBUTE_APPEND;
    if ((fileAttributes & FILE_ATTRIBUTE_NO_DUMP    ) != 0LL) attributes |= FILE_ATTRIBUTE_NO_DUMP;
    if (ioctl(handle,FS_IOC_SETFLAGS,&attributes) != 0)
    {
      error = getLastError(ERROR_CODE_IO,fileName);
      close(handle);
      return error;
    }

    // close file
    close(handle);
  #else /* not FS_IOC_GETFLAGS && FS_IOC_SETFLAGS */
    UNUSED_VARIABLE(fileAttributes);
    UNUSED_VARIABLE(fileName);
//TODO: NYI
  #endif /* FS_IOC_GETFLAGS && FS_IOC_SETFLAGS */

  return ERROR_NONE;
}

#ifdef NDEBUG
void File_initExtendedAttributes(FileExtendedAttributeList *fileExtendedAttributeList)
#else /* not NDEBUG */
void __File_initExtendedAttributes(const char                *__fileName__,
                                   ulong                     __lineNb__,
                                   FileExtendedAttributeList *fileExtendedAttributeList
                                  )
#endif /* NDEBUG */
{
  assert(fileExtendedAttributeList != NULL);

  List_init(fileExtendedAttributeList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeExtendedAttributeNode,NULL));

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(fileExtendedAttributeList,FileExtendedAttributeList);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,fileExtendedAttributeList,FileExtendedAttributeList);
  #endif /* NDEBUG */
}

#ifdef NDEBUG
void File_doneExtendedAttributes(FileExtendedAttributeList *fileExtendedAttributeList)
#else /* not NDEBUG */
void __File_doneExtendedAttributes(const char                *__fileName__,
                                   ulong                     __lineNb__,
                                   FileExtendedAttributeList *fileExtendedAttributeList
                                  )
#endif /* NDEBUG */
{
  assert(fileExtendedAttributeList != NULL);

  List_done(fileExtendedAttributeList);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(fileExtendedAttributeList,FileExtendedAttributeList);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,fileExtendedAttributeList,FileExtendedAttributeList);
  #endif /* NDEBUG */
}

void File_addExtendedAttribute(FileExtendedAttributeList *fileExtendedAttributeList,
                               ConstString               name,
                               const void                *data,
                               uint                      dataLength
                              )
{
  File_addExtendedAttributeCString(fileExtendedAttributeList,String_cString(name),data,dataLength);
}

void File_addExtendedAttributeCString(FileExtendedAttributeList *fileExtendedAttributeList,
                                      const char                *name,
                                      const void                *data,
                                      uint                      dataLength
                                     )
{
  FileExtendedAttributeNode *fileExtendedAttributeNode;

  assert(fileExtendedAttributeList != NULL);
  assert(name != NULL);
  assert(data != NULL);

  // allocate file extended attribute node
  fileExtendedAttributeNode = LIST_NEW_NODE(FileExtendedAttributeNode);
  if (fileExtendedAttributeNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  fileExtendedAttributeNode->data = malloc(dataLength);
  if (fileExtendedAttributeNode->data == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // add extended attribute to list
  fileExtendedAttributeNode->name       = String_newCString(name);
  memcpy(fileExtendedAttributeNode->data,data,dataLength);
  fileExtendedAttributeNode->dataLength = dataLength;
  List_append(fileExtendedAttributeList,fileExtendedAttributeNode);
}

Errors File_getExtendedAttributes(FileExtendedAttributeList *fileExtendedAttributeList,
                                  ConstString               fileName
                                 )
{
  #ifdef HAVE_LLISTXATTR
    int                       n;
    char                      *names;
    int                       namesLength;
    Errors                    error;
    const char                *name;
    void                      *data;
    int                       dataLength;
    FileExtendedAttributeNode *fileExtendedAttributeNode;
  #endif

  assert(fileExtendedAttributeList != NULL);
  assert(fileName != NULL);
  assert(!String_isEmpty(fileName));

  // init variables
  List_init(fileExtendedAttributeList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeExtendedAttributeNode,NULL));

  #ifdef HAVE_LLISTXATTR
    // allocate buffer for attribute names (Note: it is possible a value > 0 is returned here, but later 0 is returned)
    n = llistxattr(String_cString(fileName),NULL,0);
    if (n < 0)
    {
      error = getLastError(ERROR_CODE_IO,String_cString(fileName));
      List_done(fileExtendedAttributeList);
      return error;
    }
    names = (char*)malloc(n);
    if (names == NULL)
    {
      List_done(fileExtendedAttributeList);
      return ERROR_INSUFFICIENT_MEMORY;
    }

    // get attribute names
    namesLength = llistxattr(String_cString(fileName),names,n);
    if (namesLength < 0)
    {
      error = getLastError(ERROR_CODE_IO,String_cString(fileName));
      free(names);
      List_done(fileExtendedAttributeList);
      return error;
    }

    // get attributes
    name = names;
    while ((name-names) < namesLength)
    {
      // allocate buffer for data
      n = lgetxattr(String_cString(fileName),name,NULL,0);
      if (n < 0)
      {
        error = getLastError(ERROR_CODE_IO,String_cString(fileName));
        free(names);
        List_done(fileExtendedAttributeList);
        return error;
      }
      data = malloc(n);
      if (data == NULL)
      {
        free(names);
        List_done(fileExtendedAttributeList);
        return ERROR_INSUFFICIENT_MEMORY;
      }

      // get extended attribute
      dataLength = lgetxattr(String_cString(fileName),name,data,n);
      if (dataLength < 0)
      {
        error = getLastError(ERROR_CODE_IO,String_cString(fileName));
        free(data);
        free(names);
        List_done(fileExtendedAttributeList);
        return error;
      }

      // store in attribute list
      fileExtendedAttributeNode = LIST_NEW_NODE(FileExtendedAttributeNode);
      if (fileExtendedAttributeNode == NULL)
      {
        free(data);
        free(names);
        List_done(fileExtendedAttributeList);
        return ERROR_INSUFFICIENT_MEMORY;
      }
      fileExtendedAttributeNode->name       = String_newCString(name);
      fileExtendedAttributeNode->data       = data;
      fileExtendedAttributeNode->dataLength = (uint)dataLength;
      List_append(fileExtendedAttributeList,fileExtendedAttributeNode);

      // next attribute
      name += stringLength(name)+1;
    }

    // free resources
    free(names);
  #else /* not HAVE_LLISTXATTR */
    UNUSED_VARIABLE(fileExtendedAttributeList);
    UNUSED_VARIABLE(fileName);
  #endif /* HAVE_LLISTXATTR */

  return ERROR_NONE;
}

Errors File_setExtendedAttributes(ConstString                     fileName,
                                  const FileExtendedAttributeList *fileExtendedAttributeList
                                 )
{
  #ifdef HAVE_LSETXATTR
    FileExtendedAttributeNode *fileExtendedAttributeNode;
  #endif /* HAVE_LSETXATTR */

  assert(fileName != NULL);
  assert(!String_isEmpty(fileName));
  assert(fileExtendedAttributeList != NULL);

  #ifdef HAVE_LSETXATTR
    LIST_ITERATE(fileExtendedAttributeList,fileExtendedAttributeNode)
    {
      if (lsetxattr(String_cString(fileName),
                    String_cString(fileExtendedAttributeNode->name),
                    fileExtendedAttributeNode->data,
                    fileExtendedAttributeNode->dataLength,
                    0
                   ) != 0
         )
      {
        return getLastError(ERROR_CODE_IO,String_cString(fileName));
      }
    }
  #else /* not HAVE_LSETXATTR */
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(fileExtendedAttributeList);
  #endif /* HAVE_LSETXATTR */

  return ERROR_NONE;
}

uint64 File_getFileTimeModified(ConstString fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);
  assert(!String_isEmpty(fileName));

  if (LSTAT(String_cString(fileName),&fileStat) != 0)
  {
    return 0LL;
  }

  return (uint64)fileStat.st_mtime;
}

Errors File_setPermission(ConstString     fileName,
                          FilePermissions permissions
                         )
{
  assert(fileName != NULL);
  assert(!String_isEmpty(fileName));

  if (chmod(String_cString(fileName),(mode_t)permissions) != 0)
  {
    return getLastError(ERROR_CODE_IO,String_cString(fileName));
  }

  return ERROR_NONE;
}

Errors File_setOwner(ConstString fileName,
                     uint32      userId,
                     uint32      groupId
                    )
{
  #if   defined(PLATFORM_LINUX)
    #ifdef HAVE_CHOWN
      uid_t uid;
      gid_t gid;
    #endif /* HAVE_CHOWN */
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  assert(fileName != NULL);
  assert(!String_isEmpty(fileName));

  #if   defined(PLATFORM_LINUX)
    #ifdef HAVE_CHOWN
      if      (userId == FILE_OWN_USER_ID     ) uid = getuid();
      else if (userId == FILE_DEFAULT_USER_ID ) uid = -1;
      else                                      uid = (uid_t)userId;
      if      (userId == FILE_OWN_GROUP_ID    ) gid = getgid();
      else if (userId == FILE_DEFAULT_GROUP_ID) gid = -1;
      else                                      gid = (uid_t)groupId;

      #if   defined(HAVE_LCHMOD)
        if (lchown(String_cString(fileName),uid,gid) != 0)
        {
          return getLastError(ERROR_CODE_IO,String_cString(fileName));
        }
      #elif defined(HAVE_CHOWN)
        if (chown(String_cString(fileName),uid,gid) != 0)
        {
          return getLastError(ERROR_CODE_IO,String_cString(fileName));
        }
      #endif /* HAVE_LCHMOD, HAVE_CHOWN */

      return ERROR_NONE;
    #else /* not HAVE_CHOWN */
      UNUSED_VARIABLE(fileName);
      UNUSED_VARIABLE(userId);
      UNUSED_VARIABLE(groupId);

      return ERROR_FUNCTION_NOT_SUPPORTED;
    #endif /* HAVE_CHOWN */
  #elif defined(PLATFORM_WINDOWS)
//TODO: implement
UNUSED_VARIABLE(fileName);
UNUSED_VARIABLE(userId);
UNUSED_VARIABLE(groupId);

    return ERROR_NONE;
  #endif /* PLATFORM_... */
}

Errors File_makeDirectory(ConstString     pathName,
                          uint32          userId,
                          uint32          groupId,
                          FilePermissions permissions,
                          bool            ignoreExistingFlag
                         )
{
  #define PERMISSION_DIRECTORY (FILE_PERMISSION_USER_EXECUTE|FILE_PERMISSION_GROUP_EXECUTE|FILE_PERMISSION_OTHER_EXECUTE)

  String          directoryName;
  String          parentDirectoryName;
  mode_t          currentCreationMask;
  StringTokenizer pathNameTokenizer;
  ConstString     token;
  FileStat        fileStat;
  #ifdef HAVE_CHOWN
    uid_t           uid;
    gid_t           gid;
  #endif /* HAVE_CHOWN */
  Errors          error;

  assert(pathName != NULL);
  assert(!String_isEmpty(pathName));

  // initialize variables
  directoryName       = File_newFileName();
  parentDirectoryName = File_newFileName();

  // get current umask (get and restore current value)
  currentCreationMask = umask(0);
  umask(currentCreationMask);

  // create directory including parent directories
  File_initSplitFileName(&pathNameTokenizer,pathName);
  if (File_getNextSplitFileName(&pathNameTokenizer,&token))
  {
    if (!String_isEmpty(token))
    {
      File_setFileName(directoryName,token);
    }
    else
    {
      File_getSystemDirectory(directoryName,FILE_SYSTEM_PATH_ROOT,NULL);
    }
  }
  if      (!File_exists(directoryName))
  {
    // create root-directory
    #if   (MKDIR_ARGUMENTS_COUNT == 1)
      if (mkdir(String_cString(directoryName)) != 0)
      {
        error = getLastError(ERROR_CODE_IO,String_cString(directoryName));
        if (!ignoreExistingFlag && !File_isDirectory(directoryName))
        {
          File_doneSplitFileName(&pathNameTokenizer);
          File_deleteFileName(parentDirectoryName);
          File_deleteFileName(directoryName);
          return error;
        }
      }
    #elif (MKDIR_ARGUMENTS_COUNT == 2)
      if (mkdir(String_cString(directoryName),0777 & ~currentCreationMask) != 0)
      {
        error = getLastError(ERROR_CODE_IO,String_cString(directoryName));
        if (!ignoreExistingFlag && !File_isDirectory(directoryName))
        {
          File_doneSplitFileName(&pathNameTokenizer);
          File_deleteFileName(parentDirectoryName);
          File_deleteFileName(directoryName);
          return error;
        }
      }
    #endif /* MKDIR_ARGUMENTS_COUNT == ... */

    // set owner/group
    if (   (userId  != FILE_DEFAULT_USER_ID)
        || (groupId != FILE_DEFAULT_GROUP_ID)
       )
    {
      #ifdef HAVE_CHOWN
        if      (userId == FILE_OWN_USER_ID     ) uid = getuid();
        else if (userId == FILE_DEFAULT_USER_ID ) uid = -1;
        else                                      uid = (uid_t)userId;
        if      (userId == FILE_OWN_GROUP_ID    ) gid = getgid();
        else if (userId == FILE_DEFAULT_GROUP_ID) gid = -1;
        else                                      gid = (uid_t)groupId;

        if (chown(String_cString(directoryName),uid,gid) != 0)
        {
          error = getLastError(ERROR_CODE_IO,String_cString(directoryName));
          File_doneSplitFileName(&pathNameTokenizer);
          File_deleteFileName(parentDirectoryName);
          File_deleteFileName(directoryName);
          return error;
        }
      #endif /* HAVE_CHOWN */
    }

    // set permissions
    if (permissions != FILE_DEFAULT_PERMISSIONS)
    {
      #ifdef HAVE_CHMOD
        if (chmod(String_cString(directoryName),
                  ((mode_t)permissions|PERMISSION_DIRECTORY) & ~currentCreationMask
                 ) != 0
           )
        {
          error = getLastError(ERROR_CODE_IO,String_cString(directoryName));
          File_doneSplitFileName(&pathNameTokenizer);
          File_deleteFileName(parentDirectoryName);
          File_deleteFileName(directoryName);
          return error;
        }
      #endif /* HAVE_CHOWN */
    }
  }
  else if (!File_isDirectory(directoryName))
  {
    error = ERRORX_(NOT_A_DIRECTORY,0,"not a directory");
    File_doneSplitFileName(&pathNameTokenizer);
    File_deleteFileName(parentDirectoryName);
    File_deleteFileName(directoryName);
    return error;
  }

  while (File_getNextSplitFileName(&pathNameTokenizer,&token))
  {
    if (!String_isEmpty(token))
    {
      // get new parent directory
      File_setFileName(parentDirectoryName,directoryName);

      // get sub-directory
      File_appendFileName(directoryName,token);

      if      (!File_exists(directoryName))
      {
        // set read/write/execute-access in parent directory
        if (LSTAT(String_cString(parentDirectoryName),&fileStat) != 0)
        {
          error = getLastError(ERROR_CODE_IO,String_cString(parentDirectoryName));
          File_doneSplitFileName(&pathNameTokenizer);
          File_deleteFileName(parentDirectoryName);
          File_deleteFileName(directoryName);
          return error;
        }
        if (   ((fileStat.st_mode & PERMISSION_DIRECTORY) != PERMISSION_DIRECTORY)
            && (chmod(String_cString(parentDirectoryName),
                      (fileStat.st_mode|PERMISSION_DIRECTORY) & ~currentCreationMask
                     ) != 0
               )
           )
        {
          error = getLastError(ERROR_CODE_IO,String_cString(parentDirectoryName));
          File_doneSplitFileName(&pathNameTokenizer);
          File_deleteFileName(parentDirectoryName);
          File_deleteFileName(directoryName);
          return error;
        }

        // create sub-directory
        #if   (MKDIR_ARGUMENTS_COUNT == 1)
          if (mkdir(String_cString(directoryName)) != 0)
          {
            error = getLastError(ERROR_CODE_IO,String_cString(directoryName));
            if (!ignoreExistingFlag && !File_isDirectory(directoryName))
            {
              File_doneSplitFileName(&pathNameTokenizer);
              File_deleteFileName(parentDirectoryName);
              File_deleteFileName(directoryName);
              return error;
            }
          }
        #elif (MKDIR_ARGUMENTS_COUNT == 2)
          if (mkdir(String_cString(directoryName),0777 & ~currentCreationMask) != 0)
          {
            error = getLastError(ERROR_CODE_IO,String_cString(directoryName));
            if (!ignoreExistingFlag && !File_isDirectory(directoryName))
            {
              File_doneSplitFileName(&pathNameTokenizer);
              File_deleteFileName(parentDirectoryName);
              File_deleteFileName(directoryName);
              return error;
            }
          }
        #endif /* MKDIR_ARGUMENTS_COUNT == ... */

        // set owner/group
        if (   (userId  != FILE_DEFAULT_USER_ID)
            || (groupId != FILE_DEFAULT_GROUP_ID)
           )
        {
          // set owner
          #ifdef HAVE_CHOWN
            if      (userId == FILE_OWN_USER_ID     ) uid = getuid();
            else if (userId == FILE_DEFAULT_USER_ID ) uid = -1;
            else                                      uid = (uid_t)userId;
            if      (userId == FILE_OWN_GROUP_ID    ) gid = getgid();
            else if (userId == FILE_DEFAULT_GROUP_ID) gid = -1;
            else                                      gid = (uid_t)groupId;

            if (chown(String_cString(directoryName),uid,gid) != 0)
            {
              error = getLastError(ERROR_CODE_IO,String_cString(directoryName));
              File_doneSplitFileName(&pathNameTokenizer);
              File_deleteFileName(parentDirectoryName);
              File_deleteFileName(directoryName);
              return error;
            }
          #endif /* HAVE_CHOWN */
        }

        // set permissions
        if (permissions != FILE_DEFAULT_PERMISSIONS)
        {
          // set permission
          #ifdef HAVE_CHMOD
            if (chmod(String_cString(directoryName),
                      ((mode_t)permissions|PERMISSION_DIRECTORY) & ~currentCreationMask
                     ) != 0
               )
            {
              error = getLastError(ERROR_CODE_IO,String_cString(directoryName));
              File_doneSplitFileName(&pathNameTokenizer);
              File_deleteFileName(parentDirectoryName);
              File_deleteFileName(directoryName);
              return error;
            }
          #endif /* HAVE_CHMOD */
        }
      }
      else if (!File_isDirectory(directoryName))
      {
        error = ERRORX_(NOT_A_DIRECTORY,0,"not a directory");
        File_doneSplitFileName(&pathNameTokenizer);
        File_deleteFileName(parentDirectoryName);
        File_deleteFileName(directoryName);
        return error;
      }
    }
  }
  File_doneSplitFileName(&pathNameTokenizer);

  // free resources
  File_deleteFileName(parentDirectoryName);
  File_deleteFileName(directoryName);

  return ERROR_NONE;

  #undef PERMISSION_DIRECTORY
}

Errors File_makeDirectoryCString(const char      *pathName,
                                 uint32          userId,
                                 uint32          groupId,
                                 FilePermissions permissions,
                                 bool            ignoreExistingFlag
                                )
{
  String string;
  Errors error;

  assert(pathName != NULL);
  assert(!stringIsEmpty(pathName));

// TODO: move code from File_makeDirectory and call in File_makeDirectory this function
  string = File_setFileNameCString(File_newFileName(),pathName);
  error = File_makeDirectory(string,userId,groupId,permissions,ignoreExistingFlag);
  File_deleteFileName(string);

  return error;
}

Errors File_readLink(String      fileName,
                     ConstString linkName,
                     bool        absolutePathFlag
                    )
{
  #define BUFFER_SIZE  256
  #define BUFFER_DELTA 128

  #ifdef HAVE_READLINK
    char    *buffer;
    ssize_t bufferSize;
    int     result;
    Errors  error;
  #endif /* HAVE_READLINK */

  assert(linkName != NULL);
  assert(!String_isEmpty(linkName));
  assert(fileName != NULL);

  #ifdef HAVE_READLINK
    // allocate initial buffer for name
    buffer = (char*)malloc(BUFFER_SIZE);
    if (buffer == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    bufferSize = BUFFER_SIZE;

    // try to read link, increase buffer if needed
    while ((result = readlink(String_cString(linkName),buffer,bufferSize)) == bufferSize)
    {
      bufferSize += BUFFER_DELTA;
      buffer = realloc(buffer,bufferSize);
      if (buffer == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
    }
    if (result == -1)
    {
      error = getLastError(ERROR_CODE_IO,String_cString(linkName));
      free(buffer);
      return error;
    }

    if (absolutePathFlag && !File_isAbsoluteFileName(fileName))
    {
      // absolute name
      File_getDirectoryName(fileName,linkName);
      File_appendFileNameBuffer(fileName,buffer,result);
    }
    else
    {
      // absolute or relative name
      String_setBuffer(fileName,buffer,result);
    }

    // free resources
    free(buffer);

    return ERROR_NONE;
  #else /* not HAVE_READLINK */
    UNUSED_VARIABLE(absolutePathFlag);

    String_set(fileName,linkName);

    return ERROR_NONE;
  #endif /* HAVE_READLINK */
}

String File_getCurrentDirectory(String pathName)
{
  #ifdef HAVE_GET_CURRENT_DIR_NAME
    char *currentDirectory;
  #else
    char currentDirectory[PATH_MAX];
  #endif

  assert(pathName != NULL);

  #ifdef HAVE_GET_CURRENT_DIR_NAME
    currentDirectory = get_current_dir_name();
    if (currentDirectory != NULL)
    {
      String_setBuffer(pathName,currentDirectory,stringLength(currentDirectory));
      free(currentDirectory);
    }
    else
    {
      String_clear(pathName);
    }
  #else
    if (getcwd(currentDirectory,sizeof(currentDirectory)) != NULL)
    {
      String_setBuffer(pathName,currentDirectory,stringLength(currentDirectory));
    }
    else
    {
      String_clear(pathName);
    }
  #endif

  return pathName;
}

Errors File_changeDirectory(ConstString pathName)
{
  assert(pathName != NULL);

  return File_changeDirectoryCString(String_cString(pathName));
}

Errors File_changeDirectoryCString(const char *pathName)
{
  assert(pathName != NULL);

  #if   defined(PLATFORM_LINUX)
    if (chdir(pathName) != 0)
    {
      return getLastError(ERROR_CODE_IO,pathName);
    }
  #elif defined(PLATFORM_WINDOWS)
    if (chdir(pathName) != 0)
    {
      return getLastError(ERROR_CODE_IO,pathName);
    }
  #endif /* PLATFORM_... */

  return ERROR_NONE;
}

Errors File_makeLink(ConstString linkName,
                     ConstString fileName
                    )
{
  assert(linkName != NULL);
  assert(!String_isEmpty(linkName));
  assert(fileName != NULL);
  assert(!String_isEmpty(fileName));

  #ifdef HAVE_SYMLINK
    unlink(String_cString(linkName));
    if (symlink(String_cString(fileName),String_cString(linkName)) != 0)
    {
      return getLastError(ERROR_CODE_IO,String_cString(fileName));
    }

    return ERROR_NONE;
  #else /* not HAVE_SYMLINK */
    UNUSED_VARIABLE(linkName);
    UNUSED_VARIABLE(fileName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SYMLINK */
}

Errors File_makeHardLink(ConstString linkName,
                         ConstString fileName
                        )
{
  assert(linkName != NULL);
  assert(fileName != NULL);

  #ifdef HAVE_LINK
    unlink(String_cString(linkName));
    if (link(String_cString(fileName),String_cString(linkName)) != 0)
    {
      return getLastError(ERROR_CODE_IO,String_cString(fileName));
    }

    return ERROR_NONE;
  #else /* not HAVE_LINK */
    UNUSED_VARIABLE(linkName);
    UNUSED_VARIABLE(fileName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_LINK */
}

Errors File_makeSpecial(ConstString      name,
                        FileSpecialTypes type,
                        ulong            major,
                        ulong            minor
                       )
{
  assert(name != NULL);
  assert(!String_isEmpty(name));

  #ifdef HAVE_MKNOD
    unlink(String_cString(name));
    switch (type)
    {
      case FILE_SPECIAL_TYPE_CHARACTER_DEVICE:
        if (mknod(String_cString(name),S_IFCHR|0600,makedev(major,minor)) != 0)
        {
          return getLastError(ERROR_CODE_IO,String_cString(name));
        }
        break;
      case FILE_SPECIAL_TYPE_BLOCK_DEVICE:
        if (mknod(String_cString(name),S_IFBLK|0600,makedev(major,minor)) != 0)
        {
          return getLastError(ERROR_CODE_IO,String_cString(name));
        }
        break;
      case FILE_SPECIAL_TYPE_FIFO:
        if (mknod(String_cString(name),S_IFIFO|0666,0) != 0)
        {
          return getLastError(ERROR_CODE_IO,String_cString(name));
        }
        break;
      case FILE_SPECIAL_TYPE_SOCKET:
        if (mknod(String_cString(name),S_IFSOCK|0600,0) != 0)
        {
          return getLastError(ERROR_CODE_IO,String_cString(name));
        }
        break;
      case FILE_SPECIAL_TYPE_OTHER:
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }

    return ERROR_NONE;
  #else /* not HAVE_MKNOD */
    UNUSED_VARIABLE(name);
    UNUSED_VARIABLE(type);
    UNUSED_VARIABLE(major);
    UNUSED_VARIABLE(minor);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_MKNOD */
}

Errors File_getFileSystemInfo(FileSystemInfo *fileSystemInfo,
                              ConstString    pathName
                             )
{
  #ifdef HAVE_STATVFS
    struct statvfs fileSystemStat;
  #endif /* HAVE_STATVFS */

  assert(pathName != NULL);
  assert(!String_isEmpty(pathName));
  assert(fileSystemInfo != NULL);

  #ifdef HAVE_STATVFS
    if (statvfs(String_cString(pathName),&fileSystemStat) != 0)
    {
      return getLastError(ERROR_CODE_IO,String_cString(pathName));
    }

    fileSystemInfo->blockSize         = fileSystemStat.f_bsize;
    fileSystemInfo->freeBytes         = (uint64)fileSystemStat.f_bavail*(uint64)fileSystemStat.f_bsize;
    fileSystemInfo->totalBytes        = (uint64)fileSystemStat.f_blocks*(uint64)fileSystemStat.f_frsize;
    fileSystemInfo->freeFiles         = (uint64)fileSystemStat.f_ffree;
    fileSystemInfo->totalFiles        = (uint64)fileSystemStat.f_files;
    fileSystemInfo->maxFileNameLength = (uint64)fileSystemStat.f_namemax;
  #else /* not HAVE_STATVFS */
    UNUSED_VARIABLE(fileSystemInfo);
    UNUSED_VARIABLE(pathName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_STATVFS */

  return ERROR_NONE;
}

String File_castToString(String string, const FileCast *fileCast)
{
  char      s[64];
  #ifdef HAVE_LOCALTIME_R
    struct tm tm;
  #else
    struct tm *tm;
  #endif

  assert(s != NULL);
  assert(fileCast != NULL);

  String_clear(string);

  #ifdef HAVE_LOCALTIME_R
    localtime_r(&fileCast->mtime,&tm);
    strftime(s,sizeof(s),"%F %T",&tm);
  #else
    tm = localtime(&fileCast->mtime);
    assert(tm != NULL);
    strftime(s,sizeof(s),"%F %T",tm);
  #endif
// TODO: improved text
  String_appendFormat(string,"mtime=%s",s);

  String_appendChar(string,' ');

  #ifdef HAVE_LOCALTIME_R
    localtime_r(&fileCast->ctime,&tm);
    strftime(s,sizeof(s),"%F %T",&tm);
  #else
    tm = localtime(&fileCast->ctime);
    assert(tm != NULL);
    strftime(s,sizeof(s),"%F %T",tm);
  #endif
// TODO: improved text
  String_appendFormat(string,"ctime=%s",s);

  return string;
}

bool File_isTerminal(FILE *file)
{
  assert(file != NULL);

  return isatty(fileno(file)) != 0;
}

#ifndef NDEBUG
void File_debugDone(void)
{
  pthread_once(&debugFileInitFlag,debugFileInit);

  File_debugCheck();

  pthread_mutex_lock(&debugFileLock);
  {
    List_done(&debugClosedFileList);
    List_done(&debugOpenFileList);
  }
  pthread_mutex_unlock(&debugFileLock);
}

void File_debugDumpInfo(FILE                 *handle,
                        FileDumpInfoFunction fileDumpInfoFunction,
                        void                 *fileDumpInfoUserData
                       )
{
  ulong          n;
  DebugFileNode *debugFileNode;

  pthread_once(&debugFileInitFlag,debugFileInit);

  pthread_mutex_lock(&debugFileLock);
  {
    n = 0L;
    LIST_ITERATE(&debugOpenFileList,debugFileNode)
    {
      assert(debugFileNode->fileHandle != NULL);

      if (debugFileNode->fileHandle->name != NULL)
      {
        fprintf(handle,"DEBUG: file '%s' opened at %s, line %lu\n",
                String_cString(debugFileNode->fileHandle->name),
                debugFileNode->fileName,
                debugFileNode->lineNb
               );
      }
      else
      {
        fprintf(handle,"DEBUG: file %p opened at %s, line %lu\n",
                debugFileNode->fileHandle,
                debugFileNode->fileName,
                debugFileNode->lineNb
               );
      }

      if (fileDumpInfoFunction != NULL)
      {
        if (!fileDumpInfoFunction(debugFileNode->fileHandle,
                                  debugFileNode->fileName,
                                  debugFileNode->lineNb,
                                  n,
                                  List_count(&debugOpenFileList),
                                  fileDumpInfoUserData
                                 )
           )
        {
          break;
        }
      }

      n++;
    }
  }
  pthread_mutex_unlock(&debugFileLock);
}

void File_debugPrintInfo(FileDumpInfoFunction fileDumpInfoFunction,
                         void                 *fileDumpInfoUserData
                        )
{
  File_debugDumpInfo(stderr,fileDumpInfoFunction,fileDumpInfoUserData);
}

void File_debugPrintStatistics(void)
{
  pthread_once(&debugFileInitFlag,debugFileInit);

  pthread_mutex_lock(&debugFileLock);
  {
    fprintf(stderr,"DEBUG: %lu file(s) open\n",
            List_count(&debugOpenFileList)
           );
    fprintf(stderr,"DEBUG: %lu file(s) in closed list\n",
            List_count(&debugClosedFileList)
           );
  }
  pthread_mutex_unlock(&debugFileLock);
}

void File_debugCheck(void)
{
  pthread_once(&debugFileInitFlag,debugFileInit);

  File_debugPrintInfo(CALLBACK_(NULL,NULL));
  File_debugPrintStatistics();

  pthread_mutex_lock(&debugFileLock);
  {
    if (!List_isEmpty(&debugOpenFileList))
    {
      HALT_INTERNAL_ERROR_LOST_RESOURCE();
    }
  }
  pthread_mutex_unlock(&debugFileLock);
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
