/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver file functions
* Systems: all
*
\***********************************************************************/

#define __FILES_IMPLEMENATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#ifdef HAVE_SYS_IOCTL_H
  #include <sys/ioctl.h>
#endif
#include <utime.h>
#ifdef HAVE_SYS_STATVFS_H
  #include <sys/statvfs.h>
#endif
#ifdef HAVE_PWD_H
  #include <pwd.h>
#endif
#ifdef HAVE_GRP_H
  #include <grp.h>
#endif
#ifdef HAVE_SYS_XATTR_H
  #include <sys/xattr.h>
#endif
#include <errno.h>
#ifdef HAVE_BACKTRACE
  #include <execinfo.h>
#endif
#include <assert.h>

#if   defined(PLATFORM_LINUX)
  #include <linux/fs.h>
#elif defined(PLATFORM_WINDOWS)
  #include <windows.h>
#endif /* PLATFORM_... */

#include "global.h"
#include "strings.h"
#include "stringlists.h"
#include "devices.h"

#ifndef NDEBUG
  #include <pthread.h>
  #include "lists.h"
#endif /* not NDEBUG */

#include "files.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
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

#ifdef HAVE__FSEEKI64
  #define FSEEK(handle,offset,mode) _fseeki64(handle,offset,mode)
#elif HAVE_FSEEKO
  #define FSEEK(handle,offset,mode) fseeko(handle,offset,mode)
#else
  #define FSEEK(handle,offset,mode) fseek(handle,offset,mode)
#endif

#ifdef HAVE__FTELLI64
  #define FTELL(handle) _ftelli64(handle)
#elif HAVE_FTELLO
  #define FTELL(handle) ftello(handle)
#else
  #define FTELL(handle) ftell(handle)
#endif

#ifdef HAVE_STAT64
  #define STAT(fileName,fileState)  stat64(fileName,fileState)
  #define LSTAT(fileName,fileState) lstat64(fileName,fileState)
  typedef struct stat64 FileStat;
#elif HAVE___STAT64
  #define STAT(fileName,fileState)  stat64(fileName,fileState)
  #define LSTAT(fileName,fileState) lstat64(fileName,fileState)
  typedef struct __stat64 FileStat;
#elif HAVE__STATI64
  #define STAT(fileName,fileState)  _stati64(fileName,fileState)
  #define LSTAT(fileName,fileState) _stati64(fileName,fileState)
  typedef struct _stati64 FileStat;
#elif HAVE_STAT
  #define STAT(fileName,fileState)  stat(fileName,fileState)
  #define LSTAT(fileName,fileState) lstat(fileName,fileState)
  typedef struct stat FileStat;
#else
  #error No struct stat64 nor struct __stat64
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
  LOCAL pthread_once_t  debugFileInitFlag = PTHREAD_ONCE_INIT;
  LOCAL pthread_mutex_t debugFileLock;
  LOCAL DebugFileList   debugOpenFileList;
  LOCAL DebugFileList   debugClosedFileList;
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
LOCAL void debugFileInit(void)
{
  pthread_mutex_init(&debugFileLock,NULL);
  List_init(&debugOpenFileList);
  List_init(&debugClosedFileList);
}
#endif /* NDEBUG */

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
        debugDumpStackTrace(stderr,0,debugFileNode->closeStackTrace,debugFileNode->closeStackTraceSize,0);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR_AT(fileName,
                             lineNb,
                             "File 0x%08lx was closed at %s, line %lu",
                             (ulong)fileHandle,
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
        debugDumpCurrentStackTrace(stderr,0,0);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR("File 0x%08lx is not open",
                          (ulong)fileHandle
                         );
    }
  }
  pthread_mutex_unlock(&debugFileLock);

  // Note: real file index may be different, because of buffer in stream object
  // assert(((fileHandle->mode & FILE_STREAM) == FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
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
LOCAL Errors initFileHandle(FileHandle  *fileHandle,
                            int         fileDescriptor,
                            const char  *fileName,
                            FileModes   fileMode
                           )
#else /* not NDEBUG */
LOCAL Errors initFileHandle(const char  *__fileName__,
                            uint        __lineNb__,
                            FileHandle  *fileHandle,
                            int         fileDescriptor,
                            const char  *fileName,
                            FileModes   fileMode
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
        return ERROR_(CREATE_FILE,errno);
      }

      // truncate and seek to start
      if ((fileMode & FILE_STREAM) != FILE_STREAM)
      {
        if (FTRUNCATE(fileDescriptor,0) != 0)
        {
          return ERROR_(CREATE_FILE,errno);
        }
        if (FSEEK(fileHandle->file,0,SEEK_SET) != 0)
        {
          return ERROR_(CREATE_FILE,errno);
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
        return ERROR_(OPEN_FILE,errno);
      }

      // get file size
      if ((fileMode & FILE_STREAM) != FILE_STREAM)
      {
        if (FSEEK(fileHandle->file,0,SEEK_END) != 0)
        {
          error = ERRORX_(IO_ERROR,errno,NULL);
          fclose(fileHandle->file);
          return error;
        }
        n = (int64_t)FTELL(fileHandle->file);
        if (n == (-1LL))
        {
          error = ERRORX_(IO_ERROR,errno,NULL);
          fclose(fileHandle->file);
          return error;
        }
        if (FSEEK(fileHandle->file,0,SEEK_SET) != 0)
        {
          error = ERRORX_(IO_ERROR,errno,NULL);
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
        return ERROR_(OPEN_FILE,errno);
      }

      // seek to start
      if ((fileMode & FILE_STREAM) != FILE_STREAM)
      {
        if (FSEEK(fileHandle->file,0,SEEK_SET) != 0)
        {
          return ERROR_(CREATE_FILE,errno);
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
        return ERROR_(OPEN_FILE,errno);
      }

      // get file size
      if ((fileMode & FILE_STREAM) != FILE_STREAM)
      {
        if (FSEEK(fileHandle->file,0,SEEK_END) != 0)
        {
          return ERROR_(CREATE_FILE,errno);
        }
        n = (int64_t)FTELL(fileHandle->file);
        if (n == (-1LL))
        {
          error = ERROR_(IO_ERROR,errno);
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
  fileHandle->name = String_newCString(fileName);;
  fileHandle->mode = fileMode;
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
          debugDumpStackTrace(stderr,0,debugFileNode->stackTrace,debugFileNode->stackTraceSize,0);
        #endif /* HAVE_BACKTRACE */
        if (debugFileNode->fileHandle->name != NULL)
        {
          HALT_INTERNAL_ERROR("File '%s' at %s, line %lu opened again at %s, line %u",
                              String_cString(debugFileNode->fileHandle->name),
                              debugFileNode->fileName,
                              debugFileNode->lineNb,
                              __fileName__,
                              __lineNb__
                             );
        }
        else
        {
          HALT_INTERNAL_ERROR("File %p at %s, line %lu opened again at %s, line %u",
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
        debugFileNode->stackTraceSize      = backtrace((void*)debugFileNode->stackTrace,SIZE_OF_ARRAY(debugFileNode->stackTrace));
      #endif /* HAVE_BACKTRACE */
      debugFileNode->closeFileName         = NULL;
      debugFileNode->closeLineNb           = 0;
      #ifdef HAVE_BACKTRACE
        debugFileNode->closeStackTraceSize = 0;
      #endif /* HAVE_BACKTRACE */
      debugFileNode->fileHandle            = fileHandle;

      // add string to open-list
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
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL void doneFileHandle(FileHandle *fileHandle)
#else /* not NDEBUG */
LOCAL void doneFileHandle(const char  *__fileName__,
                          uint        __lineNb__,
                          FileHandle  *fileHandle
                         )
#endif /* NDEBUG */
{
  #ifndef NDEBUG
    DebugFileNode *debugFileNode;
  #endif /* not NDEBUG */

  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);

  // close file
  (void)fclose(fileHandle->file);

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
        debugFileNode->closeFileName = __fileName__;
        debugFileNode->closeLineNb   = __lineNb__;
        #ifdef HAVE_BACKTRACE
          debugFileNode->closeStackTraceSize = backtrace((void*)debugFileNode->closeStackTrace,SIZE_OF_ARRAY(debugFileNode->closeStackTrace));
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
          debugDumpCurrentStackTrace(stderr,0,0);
        #endif /* HAVE_BACKTRACE */
        HALT_INTERNAL_ERROR("File '%p' not found in debug list at %s, line %u",
                            fileHandle->file,
                            __fileName__,
                            __lineNb__
                           );
      }
    }
    pthread_mutex_unlock(&debugFileLock);
  #endif /* not NDEBUG */
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
  struct timespec times[2];

  times[0].tv_sec  = ts->tv_sec;
  times[0].tv_nsec = ts->tv_nsec;
  times[1].tv_sec  = 0;
  times[1].tv_nsec = UTIME_OMIT;

  return futimens(fileDescriptor,times) == 0;
}
#endif /* not HAVE_O_NOATIME */

/***********************************************************************\
* Name   : getAttributes
* Purpose: get file attributes
* Input  : fileName - file name
* Output : attributes - file attributes
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getAttributes(ConstString fileName, FileAttributes *fileAttributes)
{
  long   attributes;
  #ifdef FS_IOC_GETFLAGS
    int    handle;
    Errors error;
  #endif /* FS_IOC_GETFLAGS */
  #ifndef HAVE_O_NOATIME
    struct stat stat;
    bool   atimeFlag;
    struct timespec atime;
  #endif /* not HAVE_O_NOATIME */

  assert(fileName != NULL);
  assert(fileAttributes != NULL);

  attributes = 0LL;
  #ifdef FS_IOC_GETFLAGS
    // open file (first try with O_NOATIME)
    #ifdef HAVE_O_NOATIME
      handle = open(String_cString(fileName),O_RDONLY|O_NONBLOCK|O_NOATIME);
      if (handle == -1)
      {
        handle = open(String_cString(fileName),O_RDONLY|O_NONBLOCK);
      }
    #else /* not HAVE_O_NOATIME */
      handle = open(String_cString(fileName),O_RDONLY|O_NONBLOCK);
    #endif /* HAVE_O_NOATIME */
    if (handle == -1)
    {
      return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
    }

    #ifndef HAVE_O_NOATIME
      // store atime
      if (fstat(handle,&stat) == 0)
      {
        atime.tv_sec    = stat.st_atime;
        #ifdef HAVE_STAT_ATIM_TV_NSEC
          atime.tv_nsec = stat.st_atim.tv_nsec;
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
      error = ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
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
  if ((attributes & FILE_ATTRIBUTE_NO_DUMP    ) != 0LL) (*fileAttributes) |= FILE_ATTRIBUTE_NO_DUMP;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : setAttributes
* Purpose: set file attributes
* Input  : fileName - file name
* Output : attributes - file attributes
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors setAttributes(ConstString fileName, FileAttributes fileAttributes)
{
  long   attributes;
  #ifdef FS_IOC_GETFLAGS
    int    handle;
    Errors error;
  #endif /* FS_IOC_GETFLAGS */
  #ifndef HAVE_O_NOATIME
    struct stat stat;
    bool   atimeFlag;
    struct timespec atime;
  #endif /* not HAVE_O_NOATIME */

  assert(fileName != NULL);

  #if defined(FS_IOC_GETFLAGS) && defined(FS_IOC_SETFLAGS)
    // open file (first try with O_NOATIME)
    #ifdef HAVE_O_NOATIME
      handle = open(String_cString(fileName),O_RDONLY|O_NONBLOCK|O_NOATIME);
      if (handle == -1)
      {
        handle = open(String_cString(fileName),O_RDONLY|O_NONBLOCK);
      }
    #else /* not HAVE_O_NOATIME */
      handle = open(String_cString(fileName),O_RDONLY|O_NONBLOCK);
    #endif /* HAVE_O_NOATIME */
    if (handle == -1)
    {
      return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
    }

    // set attributes
    if (ioctl(handle,FS_IOC_GETFLAGS,&attributes) != 0)
    {
      error = ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
      close(handle);
      return error;
    }
    attributes &= ~(FILE_ATTRIBUTE_COMPRESS|FILE_ATTRIBUTE_NO_COMPRESS|FILE_ATTRIBUTE_NO_DUMP);
    if ((fileAttributes & FILE_ATTRIBUTE_COMPRESS) != 0LL) attributes |= FILE_ATTRIBUTE_COMPRESS;
    if ((fileAttributes & FILE_ATTRIBUTE_NO_COMPRESS) != 0LL) attributes |= FILE_ATTRIBUTE_NO_COMPRESS;
    if ((fileAttributes & FILE_ATTRIBUTE_NO_DUMP) != 0LL) attributes |= FILE_ATTRIBUTE_NO_DUMP;
    if (ioctl(handle,FS_IOC_SETFLAGS,&attributes) != 0)
    {
      error = ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
      close(handle);
      return error;
    }

    // close file
    close(handle);
  #else /* not FS_IOC_GETFLAGS && FS_IOC_SETFLAGS */
    UNUSED_VARIABLE(fileName);
  #endif /* FS_IOC_GETFLAGS && FS_IOC_SETFLAGS */


  return ERROR_NONE;
}
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
* Name   : parseRootEntry
* Purpose: parse root entry
* Input  : rootListHandle - root list handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void parseRootEntry(RootListHandle *rootListHandle)
{
  char       *tokenizer;
  const char *s;
  StringNode *iteratorVariable;
  String     variable;

  rootListHandle->parseFlag = FALSE;

  // parse
  s = strtok_r(rootListHandle->line," ",&tokenizer);
  if (s == NULL)
  {
    return;
  }

  // get name
  s = strtok_r(NULL," ",&tokenizer);
  if (s == NULL)
  {
    return;
  }
  strncpy(rootListHandle->name,s,sizeof(rootListHandle->name));

  // check if known file system
  s = strtok_r(NULL," ",&tokenizer);
  if (s == NULL)
  {
    return;
  }
  STRINGLIST_ITERATE(&rootListHandle->fileSystemNames,iteratorVariable,variable)
  {
    if (String_equalsCString(variable,s))
    {
      rootListHandle->parseFlag = TRUE;
      break;
    }
  }
}

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
    if (   !String_endsWithChar(fileName,FILES_PATHNAME_SEPARATOR_CHAR)
        && !String_startsWithChar(name,FILES_PATHNAME_SEPARATOR_CHAR)
       )
    {
      String_appendChar(fileName,FILES_PATHNAME_SEPARATOR_CHAR);
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
    if (   !String_endsWithChar(fileName,FILES_PATHNAME_SEPARATOR_CHAR)
        && (name[0] != FILES_PATHNAME_SEPARATOR_CHAR)
       )
    {
      String_appendChar(fileName,FILES_PATHNAME_SEPARATOR_CHAR);
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
    if (   !String_endsWithChar(fileName,FILES_PATHNAME_SEPARATOR_CHAR)
        && (ch != FILES_PATHNAME_SEPARATOR_CHAR)
       )
    {
      String_appendChar(fileName,FILES_PATHNAME_SEPARATOR_CHAR);
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
    if (   !String_endsWithChar(fileName,FILES_PATHNAME_SEPARATOR_CHAR)
        && ((bufferLength == 0) || (buffer[0] != FILES_PATHNAME_SEPARATOR_CHAR))
       )
    {
      String_appendChar(fileName,FILES_PATHNAME_SEPARATOR_CHAR);
    }
  }
  String_appendBuffer(fileName,buffer,bufferLength);

  return fileName;
}

String File_getFilePathName(String pathName, ConstString fileName)
{
  assert(pathName != NULL);

  return File_getFilePathNameCString(pathName,String_cString(fileName));
}

String File_getFilePathNameCString(String pathName, const char *fileName)
{
  const char *lastPathSeparator;

  assert(pathName != NULL);

  if (fileName != NULL)
  {
    // find last path separator
    lastPathSeparator = strrchr(fileName,FILES_PATHNAME_SEPARATOR_CHAR);

    // get path
    if (lastPathSeparator != NULL)
    {
      String_setBuffer(pathName,fileName,lastPathSeparator-fileName);
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

String File_getFileBaseName(String baseName, ConstString fileName)
{
  assert(baseName != NULL);

  return File_getFileBaseNameCString(baseName,String_cString(fileName));
}

String File_getFileBaseNameCString(String baseName, const char *fileName)
{
  const char *lastPathSeparator;

  assert(baseName != NULL);

  if (fileName != NULL)
  {
    // find last path separator
    lastPathSeparator = strrchr(fileName,FILES_PATHNAME_SEPARATOR_CHAR);

    // get path
    if (lastPathSeparator != NULL)
    {
      String_setCString(baseName,lastPathSeparator+1);
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

String File_getRootFileName(String rootName, ConstString fileName)
{
  assert(rootName != NULL);

  return File_getRootFileNameCString(rootName,String_cString(fileName));
}

String File_getRootFileNameCString(String rootName, const char *fileName)
{
  size_t n;

  assert(rootName != NULL);

  String_clear(rootName);
  if (fileName != NULL)
  {
    n = strlen(fileName);
    #if   defined(PLATFORM_LINUX)
      if ((n >= 1) && (fileName[0] == FILES_PATHNAME_SEPARATOR_CHAR))
      {
        String_setChar(rootName,FILES_PATHNAME_SEPARATOR_CHAR);
      }
    #elif defined(PLATFORM_WINDOWS)
      if      (   (n >= 2)
               && (toupper(fileName[0]) >= 'A') && (toupper(fileName[0]) <= 'Z')
               && (fileName[1] == ':')
              )
      {
        String_setChar(rootName,toupper(fileName[0]);
        String_appendChar(rootName,':');
      }
      else if (   (n >= 2)
               && (strncmp(fileName,"\\\\",2) == 0)
              )
      {
        String_clear(rootName);
      }
      if ((n >= 3) && (String_index(fileName,2) == FILES_PATHNAME_SEPARATOR_CHAR))
      {
        String_appendChar(rootName,FILES_PATHNAME_SEPARATOR_CHAR);
      }
    #endif /* PLATFORM_... */
  }
  else
  {
    String_clear(rootName);
  }

  return rootName;
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
    free(buffer);
  #endif /* PLATFORM_... */

  return absoluteFileName;
}

void File_splitFileName(ConstString fileName, String *pathName, String *baseName)
{
  assert(fileName != NULL);
  assert(pathName != NULL);
  assert(baseName != NULL);

  (*pathName) = File_getFilePathName(File_newFileName(),fileName);
  (*baseName) = File_getFileBaseName(File_newFileName(),fileName);
}

void File_initSplitFileName(StringTokenizer *stringTokenizer, ConstString fileName)
{
  assert(stringTokenizer != NULL);
  assert(fileName != NULL);

  String_initTokenizer(stringTokenizer,fileName,STRING_BEGIN,FILES_PATHNAME_SEPARATOR_CHARS,NULL,FALSE);
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

bool File_isAbsoluteFileName(ConstString fileName)
{
  assert(fileName != NULL);

  return File_isAbsoluteFileNameCString(String_cString(fileName));
}

bool File_isAbsoluteFileNameCString(const char *fileName)
{
  size_t n;

  assert(fileName != NULL);

  n = strlen(fileName);
  #if   defined(PLATFORM_LINUX)
    return ((n >= 1) && (fileName[0] == FILES_PATHNAME_SEPARATOR_CHAR));
  #elif defined(PLATFORM_WINDOWS)
    return    ((n >= 2) && ((toupper(fileName[0]) >= 'A') && (toupper(fileName[0]) <= 'Z') && (fileName[1] == ':'))
           || ((n >= 2) && (strncmp(fileName,"\\\\",2) == 0));
  #endif /* PLATFORM_... */
}

/*---------------------------------------------------------------------*/

const char *File_getSystemTmpDirectory()
{
  const char *tmpDirectory;
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    static bool bufferInit = FALSE;
    static char buffer[MAX_PATH+1];
    char        *s;
    DWORD       n;
  #endif /* PLATFORM_... */

  #if   defined(PLATFORM_LINUX)
    tmpDirectory = getenv("TMPDIR");
    if (tmpDirectory == NULL) tmpDirectory = getenv("TMP");
    if (tmpDirectory == NULL)
    {
      tmpDirectory = "/tmp";
    }
  #elif defined(PLATFORM_WINDOWS)
    if (!bufferInit)
    {
      n = GetTempPath(sizeof(buffer),buffer);
      if ((n > 0) && (n <= MAX_PATH))
      {
        // remove trailing \\ if Windows added it (Note: Windows should not try to be smart - it cannot...)
        s = &buffer[strlen(buffer)-1];
        while ((s >= buffer) && ((*s) == '\\'))
        {
          (*s) = '\0';
          s--;
        }
      }
      else
      {
        strncpy(buffer,"c:\\tmp",sizeof(buffer)-1);
        buffer[sizeof(buffer)-1] = '\0';
      }

      bufferInit = TRUE;
    }
    tmpDirectory = buffer;
  #endif /* PLATFORM_... */
  assert(tmpDirectory != NULL);

  return tmpDirectory;
}

#ifdef NDEBUG
Errors File_getTmpFile(FileHandle  *fileHandle,
                       ConstString pattern,
                       ConstString directory
                      )
#else /* not NDEBUG */
Errors __File_getTmpFile(const char  *__fileName__,
                         uint        __lineNb__,
                         FileHandle  *fileHandle,
                         ConstString pattern,
                         ConstString directory
                        )
#endif /* NDEBUG */
{
  #ifdef NDEBUG
    return File_getTmpFileCString(fileHandle,String_cString(pattern),directory);
  #else /* not NDEBUG */
    return __File_getTmpFileCString(__fileName__,__lineNb__,fileHandle,String_cString(pattern),directory);
  #endif /* NDEBUG */
}

#ifdef NDEBUG
Errors File_getTmpFileCString(FileHandle  *fileHandle,
                              char const  *pattern,
                              ConstString directory
                             )
#else /* not NDEBUG */
Errors __File_getTmpFileCString(const char  *__fileName__,
                                uint        __lineNb__,
                                FileHandle  *fileHandle,
                                char const  *pattern,
                                ConstString directory
                               )
#endif /* NDEBUG */
{
  const char *tmpDirectory;
  char       *s;
  int        handle;
  Errors     error;
  #ifndef NDEBUG
    DebugFileNode *debugFileNode;
  #endif /* not NDEBUG */

  assert(fileHandle != NULL);

  if (pattern == NULL) pattern = "tmp-XXXXXX";

  // create directory
  if (!String_isEmpty(directory))
  {
    s = (char*)malloc(String_length(directory)+strlen(FILE_SEPARATOR_STRING)+strlen(pattern)+1);
    if (s == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    strcpy(s,String_cString(directory));
    strcat(s,FILE_SEPARATOR_STRING);
  }
  else
  {
    tmpDirectory = getenv("TMP");
    s = (char*)malloc(((tmpDirectory != NULL) ? strlen(tmpDirectory)+1 : 0)+strlen(pattern)+1);
    if (s == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    if (tmpDirectory != NULL)
    {
      strcpy(s,tmpDirectory);
      strcat(s,FILE_SEPARATOR_STRING);
    }
    else
    {
      s[0] = '\0';
    }
  }
  strcat(s,pattern);

  // create temporary file
  #ifdef HAVE_MKSTEMP
    handle = mkstemp(s);
    if (handle == -1)
    {
      error = ERRORX_(IO_ERROR,errno,"%s",s);
      free(s);
      return error;
    }
    fileHandle->file = fdopen(handle,"w+b");
    if (fileHandle->file == NULL)
    {
      error = ERRORX_(CREATE_FILE,errno,"%s",s);
      free(s);
      return error;
    }
  #elif HAVE_MKTEMP
    // Note: there is a race-condition when mktemp() and open() is used!
    if (strcmp(mktemp(s),"") == 0)
    {
      error = ERRORX_(IO_ERROR,errno,"%s",s);
      free(s);
      return error;
    }
    fileHandle->file = FOPEN(s,"w+b");
      if (fileHandle->file == NULL)
    {
      error = ERRORX_(CREATE_FILE,errno,"%s",s);
      free(s);
      return error;
    }
  #else /* not HAVE_MKSTEMP || HAVE_MKTEMP */
    #error mkstemp() nor mktemp() available
  #endif /* HAVE_MKSTEMP || HAVE_MKTEMP */

  // remove file from directory (finally deleted on close)
  #ifdef NDEBUG
    if (unlink(s) != 0)
    {
      error = ERRORX_(IO_ERROR,errno,"%s",s);
      free(s);
      return error;
    }
    fileHandle->name = NULL;
  #else /* not NDEBUG */
    fileHandle->name              = String_newCString(s);
    fileHandle->deleteOnCloseFlag = TRUE;
  #endif /* NDEBUG */

  fileHandle->index = 0LL;
  fileHandle->size  = 0LL;
  fileHandle->mode  = 0;
  StringList_init(&fileHandle->lineBufferList);

  free(s);

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
          debugDumpStackTrace(stderr,0,debugFileNode->stackTrace,debugFileNode->stackTraceSize,0);
        #endif /* HAVE_BACKTRACE */
        if (debugFileNode->fileHandle->name != NULL)
        {
          HALT_INTERNAL_ERROR("File '%s' at %s, line %lu opened multiple times at %s, line %u",
                              String_cString(debugFileNode->fileHandle->name),
                              debugFileNode->fileName,
                              debugFileNode->lineNb,
                              __fileName__,
                              __lineNb__
                             );
        }
        else
        {
          HALT_INTERNAL_ERROR("File %p at %s, line %lu opened multiple times at %s, line %u",
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
        debugFileNode->stackTraceSize      = backtrace((void*)debugFileNode->stackTrace,SIZE_OF_ARRAY(debugFileNode->stackTrace));
      #endif /* HAVE_BACKTRACE */
      debugFileNode->closeFileName         = NULL;
      debugFileNode->closeLineNb           = 0;
      #ifdef HAVE_BACKTRACE
        debugFileNode->closeStackTraceSize = 0;
      #endif /* HAVE_BACKTRACE */
      debugFileNode->fileHandle            = fileHandle;

      // add string to open-list
      List_append(&debugOpenFileList,debugFileNode);
    }
    pthread_mutex_unlock(&debugFileLock);
  #endif /* not NDEBUG */

  return ERROR_NONE;
}

Errors File_getTmpFileName(String fileName, const char *prefix, ConstString directory)
{
  return File_getTmpFileNameCString(fileName,prefix,String_cString(directory));
}

Errors File_getTmpFileNameCString(String fileName, const char *prefix, const char *directory)
{
  char   *s;
  int    handle;
  Errors error;

  assert(fileName != NULL);

  if (prefix == NULL) prefix = "tmp";
  if (directory == NULL) directory = File_getSystemTmpDirectory();

  if (!stringIsEmpty(directory))
  {
    s = (char*)malloc(strlen(directory)+strlen(FILE_SEPARATOR_STRING)+strlen(prefix)+7+1);
    if (s == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    strcpy(s,directory);
    strcat(s,FILE_SEPARATOR_STRING);
  }
  else
  {
    s = (char*)malloc(strlen(prefix)+7+1);
    if (s == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    s[0] = '\0';
  }
  strcat(s,prefix);
  strcat(s,"-XXXXXX");

  #ifdef HAVE_MKSTEMP
    handle = mkstemp(s);
    if (handle == -1)
    {
      error = ERRORX_(IO_ERROR,errno,"%s",s);
      free(s);
      return error;
    }
    close(handle);
  #elif HAVE_MKTEMP
    // Note: there is a race-condition when mktemp() and open() is used!
    if (strcmp(mktemp(s),"") == 0)
    {
      error = ERRORX_(IO_ERROR,errno,"%s",s);
      free(s);
      return error;
    }
    handle = open(s,O_CREAT|O_EXCL);
    if (handle == -1)
    {
      error = ERRORX_(IO_ERROR,errno,"%s",s);
      free(s);
      return error;
    }
    close(handle);
  #else /* not HAVE_MKSTEMP || HAVE_MKTEMP */
    #error mkstemp() nor mktemp() available
  #endif /* HAVE_MKSTEMP || HAVE_MKTEMP */

  String_setBuffer(fileName,s,strlen(s));

  free(s);

  return ERROR_NONE;
}

Errors File_getTmpDirectoryName(String directoryName, ConstString pattern, ConstString directory)
{
  return File_getTmpDirectoryNameCString(directoryName,String_cString(pattern),directory);
}

Errors File_getTmpDirectoryNameCString(String directoryName, char const *pattern, ConstString directory)
{
  char   *s;
  #ifdef HAVE_MKDTEMP
  #elif HAVE_MKTEMP
    #if (MKDIR_ARGUMENTS_COUNT == 2)
      mode_t currentCreationMask;
    #endif /* MKDIR_ARGUMENTS_COUNT == 2 */
  #endif /* HAVE_MKSTEMP || HAVE_MKTEMP */
  Errors error;

  assert(directoryName != NULL);

  if (pattern == NULL) pattern = "tmp-XXXXXX";

  if (!String_isEmpty(directory))
  {
    String_set(directoryName,directory);
    File_appendFileNameCString(directoryName,pattern);
  }
  else
  {
    String_setCString(directoryName,pattern);
  }
  s = String_toCString(directoryName);
  if (s == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  #ifdef HAVE_MKDTEMP
    if (mkdtemp(s) == NULL)
    {
      error = ERRORX_(IO_ERROR,errno,"%s",s);
      free(s);
      return error;
    }
  #elif HAVE_MKTEMP
    // Note: there is a race-condition when mktemp() and mkdir() is used!
    if (strcmp(mktemp(s),"") == 0)
    {
      error = ERRORX_(IO_ERROR,errno,"%s",s);
      free(s);
      return error;
    }

    #if   (MKDIR_ARGUMENTS_COUNT == 1)
      // create directory
      if (mkdir(s) != 0)
      {
        error = ERRORX_(IO_ERROR,errno,"%s",s);
        free(s);
        return error;
      }
    #elif (MKDIR_ARGUMENTS_COUNT == 2)
      // get current umask (get and restore current value)
      currentCreationMask = umask(0);
      umask(currentCreationMask);

      // create directory
      if (mkdir(s,0777 & ~currentCreationMask) != 0)
      {
        error = ERRORX_(IO_ERROR,errno,"%s",s);
        free(s);
        return error;
      }
    #endif /* MKDIR_ARGUMENTS_COUNT == ... */
  #else /* not HAVE_MKSTEMP || HAVE_MKTEMP */
    #error mkstemp() nor mktemp() available
  #endif /* HAVE_MKSTEMP || HAVE_MKTEMP */

  String_setBuffer(directoryName,s,strlen(s));

  free(s);

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
Errors File_open(FileHandle   *fileHandle,
                 ConstString  fileName,
                 FileModes    fileMode
                )
#else /* not NDEBUG */
Errors __File_open(const char  *__fileName__,
                   uint        __lineNb__,
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
                          uint       __lineNb__,
                          FileHandle *fileHandle,
                          const char *fileName,
                          FileModes  fileMode
                         )
#endif /* NDEBUG */
{
  int     fileDescriptor;
  Errors  error;
  #ifdef HAVE_O_NOATIME
  #else /* not HAVE_O_NOATIME */
    struct stat stat;
  #endif /* HAVE_O_NOATIME */
  String  pathName;

  assert(fileHandle != NULL);
  assert(fileName != NULL);

  switch (fileMode & FILE_OPEN_MASK_MODE)
  {
    case FILE_OPEN_CREATE:
      // create file
      fileDescriptor = open(fileName,O_RDWR|O_CREAT|O_TRUNC|O_LARGEFILE,0666);
      if (fileDescriptor == -1)
      {
        return ERRORX_(CREATE_FILE,errno,"%s",fileName);
      }

      // init stream
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
      #ifdef HAVE_O_NOATIME
        if ((fileMode & FILE_OPEN_NO_ATIME) != 0)
        {
          // first try with O_NOATIME, then without O_NOATIME
          fileDescriptor = open(fileName,O_RDONLY|O_LARGEFILE|O_NOATIME,0);
          if (fileDescriptor == -1)
          {
            fileDescriptor = open(fileName,O_RDONLY|O_LARGEFILE,0);
          }
        }
        else
        {
          fileDescriptor = open(fileName,O_RDONLY|O_LARGEFILE,0);
        }
        if (fileDescriptor == -1)
        {
          return ERRORX_(OPEN_FILE,errno,"%s",fileName);
        }
      #else /* not HAVE_O_NOATIME */
        fileDescriptor = open(fileName,O_RDONLY|O_LARGEFILE,0);
        if (fileDescriptor == -1)
        {
          return ERRORX_(OPEN_FILE,errno,"%s",fileName);
        }

        // store atime
        if ((fileMode & FILE_OPEN_NO_ATIME) != 0)
        {
          if (fstat(fileDescriptor,&stat) == 0)
          {
            fileHandle->atime.tv_sec  = stat.st_atime;
            #ifdef HAVE_STAT_ATIM_TV_NSEC
              fileHandle->atime.tv_nsec = stat.st_atim.tv_nsec;
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
      #endif /* HAVE_O_NOATIME */

      // init stream
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
          if ((fileHandle->mode & FILE_OPEN_NO_ATIME) != 0)
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
      pathName = File_getFilePathNameCString(File_newFileName(),fileName);
      if (!String_isEmpty(pathName) && !File_exists(pathName))
      {
        error = File_makeDirectory(pathName,
                                   FILE_DEFAULT_USER_ID,
                                   FILE_DEFAULT_GROUP_ID,
                                   FILE_DEFAULT_PERMISSION
                                  );
        if (error != ERROR_NONE)
        {
          File_deleteFileName(pathName);
          return error;
        }
      }
      File_deleteFileName(pathName);

      // open file for writing
      fileDescriptor = open(fileName,O_RDWR|O_CREAT|O_LARGEFILE,0666);
      if (fileDescriptor == -1)
      {
        return ERRORX_(OPEN_FILE,errno,"%s",fileName);
      }

      // init stream
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
      pathName = File_getFilePathNameCString(File_newFileName(),fileName);
      if (!String_isEmpty(pathName) && !File_exists(pathName))
      {
        error = File_makeDirectory(pathName,
                                   FILE_DEFAULT_USER_ID,
                                   FILE_DEFAULT_GROUP_ID,
                                   FILE_DEFAULT_PERMISSION
                                  );
        if (error != ERROR_NONE)
        {
          File_deleteFileName(pathName);
          return error;
        }
      }
      File_deleteFileName(pathName);

      // open file for append
      fileDescriptor = open(fileName,O_RDWR|O_CREAT|O_APPEND|O_LARGEFILE,0666);
      if (fileDescriptor == -1)
      {
        return ERRORX_(IO_ERROR,errno,"%s",fileName);
      }

      // init stream
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
                             uint       __lineNb__,
                             FileHandle *fileHandle,
                             int        fileDescriptor,
                             FileModes  fileMode
                            )
#endif /* NDEBUG */
{
  assert(fileHandle != NULL);
  assert(fileDescriptor != -1);

  #ifdef NDEBUG
    return initFileHandle(fileHandle,
                          fileDescriptor,
                          NULL,  // fileName
                          fileMode
                         );
  #else /* not NDEBUG */
    return initFileHandle(__fileName__,
                          __lineNb__,
                          fileHandle,
                          fileDescriptor,
                          NULL,  // fileName
                          fileMode
                         );
  #endif /* NDEBUG */
}

#ifdef NDEBUG
Errors File_close(FileHandle *fileHandle)
#else /* not NDEBUG */
Errors __File_close(const char *__fileName__,
                    uint       __lineNb__,
                    FileHandle *fileHandle
                   )
#endif /* NDEBUG */
{
  Errors error;

  FILE_CHECK_VALID(fileHandle);

  error = ERROR_NONE;

  #ifndef NDEBUG
    if (fileHandle->deleteOnCloseFlag && (fileHandle->name != NULL))
    {
      if (unlink(String_cString(fileHandle->name)) != 0)
      {
        if (error == ERROR_NONE) error = ERRORX_(IO_ERROR,errno,"%s",String_cString(fileHandle->name));
      }
    }
  #endif /* not NDEBUG */

  // free caches if requested
  if ((fileHandle->mode & FILE_OPEN_NO_CACHE) != 0)
  {
    File_dropCaches(fileHandle,0LL,0LL,FALSE);
  }

  #ifndef HAVE_O_NOATIME
    if ((fileHandle->mode & FILE_OPEN_NO_ATIME) != 0)
    {
      if (!setAccessTime(fileHandle->handle, &fileHandle->atime))
      {
        if (error == ERROR_NONE) error = ERRORX_(IO_ERROR,errno,"%s",String_cString(fileHandle->name));
      }
    }
  #endif /* not HAVE_O_NOATIME */

  // done stream
  #ifdef NDEBUG
    doneFileHandle(fileHandle);
  #else /* not NDEBUG */
    doneFileHandle(__fileName__,
                   __lineNb__,
                   fileHandle
                  );
  #endif /* NDEBUG */

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
                 ulong      bufferLength,
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
//    assert(((fileHandle->mode & FILE_STREAM) == FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
    n = fread(buffer,1,bufferLength,fileHandle->file);
    if ((n <= 0) && (ferror(fileHandle->file) != 0))
    {
      return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileHandle->name));
    }
    fileHandle->index += (uint64)n;
//TODO: not valid when file changed in the meantime
//    assert(((fileHandle->mode & FILE_STREAM) == FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
    (*bytesRead) = n;
  }
  else
  {
    // read all requested data
    errno=0;
    while (bufferLength > 0L)
    {
      n = fread(buffer,1,bufferLength,fileHandle->file);
      if (n <= 0)
      {
        if (ferror(fileHandle->file) != 0)
        {
          return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileHandle->name));
        }
        else
        {
          return ERROR_END_OF_FILE;
        }
      }
      buffer = (byte*)buffer+n;
      bufferLength -= (ulong)n;
      fileHandle->index += (uint64)n;
//TODO: not valid when file changed in the meantime
//      assert(((fileHandle->mode & FILE_STREAM) == FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
    }
  }

  // free caches if requested
  if ((fileHandle->mode & FILE_OPEN_NO_CACHE) != 0)
  {
    File_dropCaches(fileHandle,0LL,fileHandle->index,FALSE);
  }

  return ERROR_NONE;
}

Errors File_write(FileHandle *fileHandle,
                  const void *buffer,
                  ulong      bufferLength
                 )
{
  ssize_t n;

  FILE_CHECK_VALID(fileHandle);
  assert(buffer != NULL);

  n = fwrite(buffer,1,bufferLength,fileHandle->file);
  if (n > 0)
  {
    fileHandle->index += (uint64)n;
    // Note: real file index may be different, because of buffer in stream object
    // assert(((fileHandle->mode & FILE_STREAM) == FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
  }
  if (fileHandle->index > fileHandle->size) fileHandle->size = fileHandle->index;
  if (n != (ssize_t)bufferLength)
  {
    return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileHandle->name));
  }

  // free caches if requested
  if ((fileHandle->mode & FILE_OPEN_NO_CACHE) != 0)
  {
    File_dropCaches(fileHandle,0LL,fileHandle->index,TRUE);
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
//        assert(((fileHandle->mode & FILE_STREAM) == FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
        if (((char)ch != '\n') && ((char)ch != '\r'))
        {
          String_appendChar(line,ch);
        }
      }
      else
      {
        if (!feof(fileHandle->file))
        {
          return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileHandle->name));
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
//        assert(((fileHandle->mode & FILE_STREAM) == FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
        if (ch != '\n')
        {
          fileHandle->index -= 1LL;
//TODO: not valid when file changed in the meantime
//          assert(((fileHandle->mode & FILE_STREAM) == FILE_STREAM) || (fileHandle->index == (uint64)FTELL(fileHandle->file)));
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

Errors File_transfer(FileHandle *sourceFileHandle,
                     FileHandle *destinationFileHandle,
                     uint64     length,
                     uint64     *bytesTransfered
                    )
{
  #define BUFFER_SIZE (1024*1024)

  void    *buffer;
  ulong   bufferLength;
  ssize_t n;
  Errors  error;

  // allocate transfer buffer
  buffer = (char*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // transfer data
  if (bytesTransfered != NULL) (*bytesTransfered) = 0LL;
  while (length > 0LL)
  {
    bufferLength = MIN(length,BUFFER_SIZE);

    n = fread(buffer,1,bufferLength,sourceFileHandle->file);
    if (n != (ssize_t)bufferLength)
    {
      error = ERRORX_(IO_ERROR,errno,"%s",String_cString(sourceFileHandle->name));
      free(buffer);
      return error;
    }

    n = fwrite(buffer,1,bufferLength,destinationFileHandle->file);
    if (n != (ssize_t)bufferLength)
    {
      error = ERRORX_(IO_ERROR,errno,"%s",String_cString(destinationFileHandle->name));
      free(buffer);
      return error;
    }

    length -= bufferLength;
    if (bytesTransfered != NULL) (*bytesTransfered) += bufferLength;
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
    return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileHandle->name));
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
  while (!readFlag)
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
    if (!String_isEmpty(line))
    {
      readFlag = (commentChars == NULL) || (strchr(commentChars,(int)String_index(line,STRING_BEGIN)) == NULL);
    }
  }

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
    return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileHandle->name));
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
    return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileHandle->name));
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

  if (size < fileHandle->size)
  {
    (void)fflush(fileHandle->file);
    if (ftruncate(fileno(fileHandle->file),(off_t)size) != 0)
    {
      return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileHandle->name));
    }
    if (fileHandle->index > size)
    {
      if (FSEEK(fileHandle->file,(off_t)size,SEEK_SET) == -1)
      {
        return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileHandle->name));
      }
      fileHandle->index = size;
//TODO: not valid when file changed in the meantime
//      assert(fileHandle->index == (uint64)FTELL(fileHandle->file));
    }
    fileHandle->size = size;
  }

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
      return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileHandle->name));
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

  handle = open(String_cString(fileName),O_WRONLY|O_CREAT|O_NOCTTY|O_NONBLOCK,0666);
  if (handle == -1)
  {
    return ERROR_(CREATE_FILE,errno);
  }
  close(handle);

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors File_openRootList(RootListHandle *rootListHandle)
{
  #define FILESYSMTES_FILENAME "/proc/filesystems"
  #define MOUNTS_FILENAME      "/proc/mounts"

  FILE *handle;
  char line[1024];
  char *s,*t;

  assert(rootListHandle != NULL);

  rootListHandle->parseFlag = FALSE;

  // get file system names
  handle = fopen(FILESYSMTES_FILENAME,"r");
  if (handle == NULL)
  {
    return ERRORX_(OPEN_FILE,errno,"%s",FILESYSMTES_FILENAME);
  }
  StringList_init(&rootListHandle->fileSystemNames);
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
      (*t) = '\0';
      StringList_appendCString(&rootListHandle->fileSystemNames,s);
    }
  }
  (void)fclose(handle);

  // open mount list
  rootListHandle->mounts = fopen(MOUNTS_FILENAME,"r");
  if (rootListHandle->mounts == NULL)
  {
    StringList_done(&rootListHandle->fileSystemNames);
    return ERRORX_(OPEN_FILE,errno,"%s",MOUNTS_FILENAME);
  }

  return ERROR_NONE;
}

void File_closeRootList(RootListHandle *rootListHandle)
{
  assert(rootListHandle != NULL);

  fclose(rootListHandle->mounts);
  StringList_done(&rootListHandle->fileSystemNames);
}

bool File_endOfRootList(RootListHandle *rootListHandle)
{
  assert(rootListHandle != NULL);

  while (   !rootListHandle->parseFlag
         && (fgets(rootListHandle->line,sizeof(rootListHandle->line),rootListHandle->mounts) != NULL)
        )
  {
    parseRootEntry(rootListHandle);
  }

  return !rootListHandle->parseFlag;
}

Errors File_readRootList(RootListHandle *rootListHandle,
                         String         name
                        )
{
  assert(rootListHandle != NULL);

  while (   !rootListHandle->parseFlag
         && (fgets(rootListHandle->line,sizeof(rootListHandle->line),rootListHandle->mounts) != NULL)
        )
  {
    parseRootEntry(rootListHandle);
  }
  if (!rootListHandle->parseFlag)
  {
    return ERROR_END_OF_FILE;
  }

  String_setCString(name,rootListHandle->name);

  rootListHandle->parseFlag = FALSE;

  return ERROR_NONE;
}

Errors File_openDirectoryList(DirectoryListHandle *directoryListHandle,
                              ConstString         pathName
                             )
{
  assert(directoryListHandle != NULL);
  assert(pathName != NULL);

  return File_openDirectoryListCString(directoryListHandle,String_cString(pathName));
}

Errors File_openDirectoryListCString(DirectoryListHandle *directoryListHandle,
                                     const char          *pathName
                                    )
{
  #ifdef HAVE_O_NOATIME
    int    handle;
  #else /* not HAVE_O_NOATIME */
    struct stat stat;
  #endif /* HAVE_O_NOATIME */

  assert(directoryListHandle != NULL);
  assert(pathName != NULL);

  #if defined(HAVE_FDOPENDIR) && defined(HAVE_O_DIRECTORY)
    #ifdef HAVE_O_NOATIME
      // open directory (try first with O_NOATIME)
      handle = open(pathName,O_RDONLY|O_NOCTTY|O_DIRECTORY|O_NOATIME,0);
      if (handle == -1)
      {
        handle = open(pathName,O_RDONLY|O_NOCTTY|O_DIRECTORY,0);
      }
      if (handle == -1)
      {
        return ERRORX_(OPEN_DIRECTORY,errno,"%s",pathName);
      }

      // create directory handle
      directoryListHandle->dir = fdopendir(handle);
    #else /* not HAVE_O_NOATIME */
      // open directory
      directoryListHandle->handle = open(pathName,O_RDONLY|O_NOCTTY|O_DIRECTORY,0);
      if (directoryListHandle->handle == -1)
      {
        return ERRORX_(OPEN_DIRECTORY,errno,"%s",pathName);
      }

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
    #endif /* HAVE_O_NOATIME */
  #else /* not HAVE_FDOPENDIR && HAVE_O_DIRECTORY */
    directoryListHandle->dir = opendir(pathName);
  #endif /* HAVE_FDOPENDIR && HAVE_O_DIRECTORY */
  if (directoryListHandle->dir == NULL)
  {
    #if defined(HAVE_FDOPENDIR) && defined(HAVE_O_DIRECTORY)
      #ifndef HAVE_O_NOATIME
        (void)setAccessTime(directoryListHandle->handle,&directoryListHandle->atime);
      #endif /* not HAVE_O_NOATIME */
    #endif /* HAVE_FDOPENDIR && HAVE_O_DIRECTORY */
    return ERRORX_(OPEN_DIRECTORY,errno,"%s",pathName);
  }

  directoryListHandle->name  = String_newCString(pathName);
  directoryListHandle->entry = NULL;

  return ERROR_NONE;
}

void File_closeDirectoryList(DirectoryListHandle *directoryListHandle)
{
  assert(directoryListHandle != NULL);
  assert(directoryListHandle->name != NULL);
  assert(directoryListHandle->dir != NULL);

  #if defined(HAVE_FDOPENDIR) && defined(HAVE_O_DIRECTORY)
    #ifndef HAVE_O_NOATIME
      (void)setAccessTime(directoryListHandle->handle,&directoryListHandle->atime);
    #endif /* not HAVE_O_NOATIME */
  #endif /* HAVE_FDOPENDIR && HAVE_O_DIRECTORY */

  closedir(directoryListHandle->dir);
  File_deleteFileName(directoryListHandle->name);
}

bool File_endOfDirectoryList(DirectoryListHandle *directoryListHandle)
{
  assert(directoryListHandle != NULL);
  assert(directoryListHandle->name != NULL);
  assert(directoryListHandle->dir != NULL);

  // read entry iff not read
  if (directoryListHandle->entry == NULL)
  {
    directoryListHandle->entry = readdir(directoryListHandle->dir);
  }

  // skip "." and ".." entries
  while (   (directoryListHandle->entry != NULL)
         && (   (strcmp(directoryListHandle->entry->d_name,"." ) == 0)
             || (strcmp(directoryListHandle->entry->d_name,"..") == 0)
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
  assert(directoryListHandle->name != NULL);
  assert(directoryListHandle->dir != NULL);
  assert(fileName != NULL);

  // read entry iff not read
  if (directoryListHandle->entry == NULL)
  {
    directoryListHandle->entry = readdir(directoryListHandle->dir);
  }

  // skip "." and ".." entries
  while (   (directoryListHandle->entry != NULL)
         && (   (strcmp(directoryListHandle->entry->d_name,"." ) == 0)
             || (strcmp(directoryListHandle->entry->d_name,"..") == 0)
            )
        )
  {
    directoryListHandle->entry = readdir(directoryListHandle->dir);
  }
  if (directoryListHandle->entry == NULL)
  {
    return ERRORX_(IO_ERROR,errno,"%s",String_cString(directoryListHandle->name));
  }

  // get entry name
  String_set(fileName,directoryListHandle->name);
  File_appendFileNameCString(fileName,directoryListHandle->entry->d_name);

  // mark entry read
  directoryListHandle->entry = NULL;

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

uint32 File_userNameToUserId(const char *name)
{
  #define BUFFER_DELTA_SIZE 1024
  #define MAX_BUFFER_SIZE   (64*1024)

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETPWNAM_R)
    long          bufferSize;
    char          *buffer,*newBuffer;
    struct passwd passwordEntry;
    struct passwd *result;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETPWNAM_R) */
  uint32        userId;

  assert(name != NULL);

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETPWNAM_R)
    // allocate buffer
    bufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufferSize == -1L)
    {
      return FILE_DEFAULT_USER_ID;
    }
    buffer = (char*)malloc(bufferSize);
    if (buffer == NULL)
    {
      return FILE_DEFAULT_USER_ID;
    }

    // get user passwd entry
    while (getpwnam_r(name,&passwordEntry,buffer,bufferSize,&result) != 0)
    {
      if ((errno != ERANGE) || ((bufferSize+BUFFER_DELTA_SIZE) >= MAX_BUFFER_SIZE))
      {
        free(buffer);
        return FILE_DEFAULT_USER_ID;
      }
      else
      {
        // Note: returned size may not be enough. Increase buffer size.
        newBuffer = (char*)realloc(buffer,bufferSize+BUFFER_DELTA_SIZE);
        if (newBuffer == NULL)
        {
          free(buffer);
          return FILE_DEFAULT_USER_ID;
        }
        buffer     =  newBuffer;
        bufferSize += BUFFER_DELTA_SIZE;
      }
    }

    // get user id
    userId = (result != NULL) ? result->pw_uid : FILE_DEFAULT_USER_ID;

    // free resources
    free(buffer);
  #else /* not defined(HAVE_SYSCONF) && defined(HAVE_GETPWNAM_R) */
    UNUSED_VARIABLE(name);

    userId = FILE_DEFAULT_USER_ID;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETPWNAM_R) */

  return userId;

  #undef BUFFER_DELTA_SIZE
  #undef MAX_BUFFER_SIZE
}

const char *File_userIdToUserName(char *name, uint nameSize, uint32 userId)
{
  #define BUFFER_DELTA_SIZE 1024
  #define MAX_BUFFER_SIZE   (64*1024)

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R)
    long          bufferSize;
    char          *buffer,*newBuffer;
    struct passwd groupEntry;
    struct passwd *result;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R) */

  assert(name != NULL);
  assert(nameSize > 0);

  name[0] = '\0';

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R)
    // allocate buffer
    bufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufferSize == -1L)
    {
      return NULL;
    }
    buffer = (char*)malloc(bufferSize);
    if (buffer == NULL)
    {
      return NULL;
    }

    // get user passwd entry
    while (getpwuid_r((uid_t)userId,&groupEntry,buffer,bufferSize,&result) != 0)
    {
      if ((errno != ERANGE) || ((bufferSize+BUFFER_DELTA_SIZE) >= MAX_BUFFER_SIZE))
      {
        free(buffer);
        return NULL;
      }
      else
      {
        // Note: returned size may not be enough. Increase buffer size.
        newBuffer = (char*)realloc(buffer,bufferSize+BUFFER_DELTA_SIZE);
        if (newBuffer == NULL)
        {
          free(buffer);
          return NULL;
        }
        buffer     =  newBuffer;
        bufferSize += BUFFER_DELTA_SIZE;
      }
    }

    // get user name
    if (result != NULL)
    {
      strncpy(name,result->pw_name,nameSize);
    }
    else
    {
      strncpy(name,"NONE",nameSize);
    }
    name[nameSize-1] = '\0';

    // free resources
    free(buffer);
  #else /* not defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R) */
    UNUSED_VARIABLE(userId);

    strncpy(name,"NONE",nameSize);
    name[nameSize-1] = '\0';
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R) */

  return name;

  #undef BUFFER_DELTA_SIZE
  #undef MAX_BUFFER_SIZE
}

uint32 File_groupNameToGroupId(const char *name)
{
  #define BUFFER_DELTA_SIZE 1024
  #define MAX_BUFFER_SIZE   (64*1024)

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETGRNAM_R)
    long         bufferSize;
    char         *buffer,*newBuffer;
    struct group groupEntry;
    struct group *result;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R) */
  uint32       groupId;

  assert(name != NULL);

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETGRNAM_R)
    // allocate buffer
    bufferSize = sysconf(_SC_GETGR_R_SIZE_MAX);
    if (bufferSize == -1L)
    {
      return FILE_DEFAULT_GROUP_ID;
    }
    buffer = (char*)malloc(bufferSize);
    if (buffer == NULL)
    {
      return FILE_DEFAULT_GROUP_ID;
    }

    // get user passwd entry
    while (getgrnam_r(name,&groupEntry,buffer,bufferSize,&result) != 0)
    {
      if ((errno != ERANGE) || ((bufferSize+BUFFER_DELTA_SIZE) >= MAX_BUFFER_SIZE))
      {
        free(buffer);
        return FILE_DEFAULT_GROUP_ID;
      }
      else
      {
        // Note: returned size may not be enough. Increase buffer size.
        newBuffer = (char*)realloc(buffer,bufferSize+BUFFER_DELTA_SIZE);
        if (newBuffer == NULL)
        {
          free(buffer);
          return FILE_DEFAULT_GROUP_ID;
        }
        buffer     =  newBuffer;
        bufferSize += BUFFER_DELTA_SIZE;
      }
    }

    // get group id
    groupId = (result != NULL) ? result->gr_gid : FILE_DEFAULT_GROUP_ID;

    // free resources
    free(buffer);
  #else /* not defined(HAVE_SYSCONF) && defined(HAVE_GETGRNAM_R) */
    UNUSED_VARIABLE(name);

    groupId = FILE_DEFAULT_GROUP_ID;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETGRNAM_R) */

  return groupId;

  #undef BUFFER_DELTA_SIZE
  #undef MAX_BUFFER_SIZE
}

const char *File_groupIdToGroupName(char *name, uint nameSize, uint32 groupId)
{
  #define BUFFER_DELTA_SIZE 1024
  #define MAX_BUFFER_SIZE   (64*1024)

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R)
    long         bufferSize;
    char         *buffer,*newBuffer;
    struct group groupEntry;
    struct group *result;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETGRGID_R) */

  assert(name != NULL);
  assert(nameSize > 0);

  name[0] = '\0';

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETGRGID_R)
    // allocate buffer
    bufferSize = sysconf(_SC_GETGR_R_SIZE_MAX);
    if (bufferSize == -1L)
    {
      return NULL;
    }
    buffer = (char*)malloc(bufferSize);
    if (buffer == NULL)
    {
      return NULL;
    }

    // get user passwd entry
    while (getgrgid_r((gid_t)groupId,&groupEntry,buffer,bufferSize,&result) != 0)
    {
      if ((errno != ERANGE) || ((bufferSize+BUFFER_DELTA_SIZE) >= MAX_BUFFER_SIZE))
      {
        free(buffer);
        return NULL;
      }
      else
      {
        // Note: returned size may not be enough. Increase buffer size.
        newBuffer = (char*)realloc(buffer,bufferSize+BUFFER_DELTA_SIZE);
        if (newBuffer == NULL)
        {
          free(buffer);
          return NULL;
        }
        buffer     =  newBuffer;
        bufferSize += BUFFER_DELTA_SIZE;
      }
    }

    // get group name
    if (result != NULL)
    {
      strncpy(name,result->gr_name,nameSize);
    }
    else
    {
      strncpy(name,"NONE",nameSize);
    }
    name[nameSize-1] = '\0';

    // free resources
    free(buffer);
  #else /* not defined(HAVE_SYSCONF) && defined(HAVE_GETGRGID_R) */
    UNUSED_VARIABLE(groupId);

    strncpy(name,"NONE",nameSize);
    name[nameSize-1] = '\0';
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETGRGID_R) */

  return name;

  #undef BUFFER_DELTA_SIZE
  #undef MAX_BUFFER_SIZE
}

FilePermission File_stringToPermission(const char *string)
{
  FilePermission permission;
  uint           stringLength;

  assert(string != NULL);

  permission = FILE_PERMISSION_NONE;

  stringLength = strlen(string);
  if ((stringLength >= 1) && (toupper(string[0]) == 'R')) permission |= FILE_PERMISSION_USER_READ;
  if ((stringLength >= 2) && (toupper(string[1]) == 'W')) permission |= FILE_PERMISSION_USER_WRITE;
  if ((stringLength >= 3) && (toupper(string[2]) == 'X')) permission |= FILE_PERMISSION_USER_EXECUTE;
  if ((stringLength >= 3) && (toupper(string[2]) == 'S')) permission |= FILE_PERMISSION_USER_SET_ID;
  if ((stringLength >= 4) && (toupper(string[3]) == 'R')) permission |= FILE_PERMISSION_GROUP_READ;
  if ((stringLength >= 5) && (toupper(string[4]) == 'W')) permission |= FILE_PERMISSION_GROUP_WRITE;
  if ((stringLength >= 6) && (toupper(string[5]) == 'X')) permission |= FILE_PERMISSION_GROUP_EXECUTE;
  if ((stringLength >= 6) && (toupper(string[5]) == 'S')) permission |= FILE_PERMISSION_GROUP_SET_ID;
  if ((stringLength >= 7) && (toupper(string[6]) == 'R')) permission |= FILE_PERMISSION_OTHER_READ;
  if ((stringLength >= 8) && (toupper(string[7]) == 'W')) permission |= FILE_PERMISSION_OTHER_WRITE;
  if ((stringLength >= 9) && (toupper(string[8]) == 'X')) permission |= FILE_PERMISSION_OTHER_EXECUTE;
  if ((stringLength >= 9) && (toupper(string[8]) == 'T')) permission |= FILE_PERMISSION_STICKY_BIT;

  return permission;
}

const char *File_permissionToString(char *string, uint stringSize, FilePermission permission)
{
  assert(string != NULL);
  assert(stringSize > 0);

  memset(string,'-',stringSize-1);
  if ((stringSize >= 1) && ((permission & FILE_PERMISSION_USER_READ    ) != 0)) string[0] = 'r';
  if ((stringSize >= 2) && ((permission & FILE_PERMISSION_USER_WRITE   ) != 0)) string[1] = 'w';
  if ((stringSize >= 3) && ((permission & FILE_PERMISSION_USER_EXECUTE ) != 0)) string[2] = 'x';
  if ((stringSize >= 3) && ((permission & FILE_PERMISSION_USER_SET_ID  ) != 0)) string[2] = 's';
  if ((stringSize >= 4) && ((permission & FILE_PERMISSION_GROUP_READ   ) != 0)) string[3] = 'r';
  if ((stringSize >= 5) && ((permission & FILE_PERMISSION_GROUP_WRITE  ) != 0)) string[4] = 'w';
  if ((stringSize >= 6) && ((permission & FILE_PERMISSION_GROUP_EXECUTE) != 0)) string[5] = 'x';
  if ((stringSize >= 6) && ((permission & FILE_PERMISSION_GROUP_SET_ID ) != 0)) string[5] = 's';
  if ((stringSize >= 7) && ((permission & FILE_PERMISSION_OTHER_READ   ) != 0)) string[6] = 'r';
  if ((stringSize >= 8) && ((permission & FILE_PERMISSION_OTHER_WRITE  ) != 0)) string[7] = 'w';
  if ((stringSize >= 9) && ((permission & FILE_PERMISSION_OTHER_EXECUTE) != 0)) string[8] = 'x';
  if ((stringSize >= 9) && ((permission & FILE_PERMISSION_STICKY_BIT   ) != 0)) string[8] = 't';
  string[stringSize-1] = '\0';

  return string;
}

FileTypes File_getType(ConstString fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  if (LSTAT(String_cString(fileName),&fileStat) == 0)
  {
    if      (S_ISREG(fileStat.st_mode))  return (fileStat.st_nlink > 1) ? FILE_TYPE_HARDLINK : FILE_TYPE_FILE;
    else if (S_ISDIR(fileStat.st_mode))  return FILE_TYPE_DIRECTORY;
    #ifdef S_ISLNK
    else if (S_ISLNK(fileStat.st_mode))  return FILE_TYPE_LINK;
    #endif /* S_ISLNK */
    else if (S_ISCHR(fileStat.st_mode))  return FILE_TYPE_SPECIAL;
    else if (S_ISBLK(fileStat.st_mode))  return FILE_TYPE_SPECIAL;
    else if (S_ISFIFO(fileStat.st_mode)) return FILE_TYPE_SPECIAL;
    #ifdef S_ISSOCK
    else if (S_ISSOCK(fileStat.st_mode)) return FILE_TYPE_SPECIAL;
    #endif /* S_ISSOCK */
    else                                 return FILE_TYPE_UNKNOWN;
  }
  else
  {
    return FILE_TYPE_UNKNOWN;
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
    return ERROR_(IO_ERROR,errno);
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
    return ERRORX_(IO_ERROR,errno,"%s",fileName);
  }

  if      (   S_ISREG(fileStat.st_mode)
           #ifdef S_ISLNK
           || S_ISLNK(fileStat.st_mode)
           #endif /* S_ISLNK */
          )
  {
    if (unlink(fileName) != 0)
    {
      return ERRORX_(IO_ERROR,errno,"%s",fileName);
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
            if (   (strcmp(entry->d_name,"." ) != 0)
                && (strcmp(entry->d_name,"..") != 0)
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
                    error = ERRORX_(IO_ERROR,errno,"%s",String_cString(name));
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
              error = ERRORX_(IO_ERROR,errno,"%s",String_cString(directoryName));
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
        error = ERRORX_(IO_ERROR,errno,"%s",fileName);
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
  Errors error;

  assert(oldFileName != NULL);
  assert(newFileName != NULL);

  if (newBackupFileName != NULL)
  {
    // try rename new -> backup
    if (rename(newFileName,newBackupFileName) != 0)
    {
      // copy to backup file
      error = File_copyCString(newFileName,newBackupFileName);
      if (error != ERROR_NONE)
      {
        return error;
      }

      // delete new file
      if (unlink(newFileName) != 0)
      {
        return ERRORX_(IO_ERROR,errno,"%s",newFileName);
      }
    }
  }

  // try rename old -> new
  if (rename(oldFileName,newFileName) != 0)
  {
    // copy to new file
    error = File_copyCString(oldFileName,newFileName);
    if (error != ERROR_NONE)
    {
      if (newBackupFileName != NULL)
      {
        (void)File_copyCString(newBackupFileName,newFileName);
        (void)unlink(newBackupFileName);
      }
      return error;
    }

    // delete old file
    if (unlink(oldFileName) != 0)
    {
      if (newBackupFileName != NULL)
      {
        (void)File_copyCString(newBackupFileName,newFileName);
        (void)unlink(newBackupFileName);
      }
      return ERRORX_(IO_ERROR,errno,"%s",oldFileName);
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
  sourceFile = FOPEN(sourceFileName,"r");
  if (sourceFile == NULL)
  {
    error = ERRORX_(OPEN_FILE,errno,"%s",sourceFileName);
    free(buffer);
    return error;
  }
  destinationFile = FOPEN(destinationFileName,"w");
  if (destinationFile == NULL)
  {
    error = ERRORX_(OPEN_FILE,errno,"%s",destinationFileName);
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
        error = ERRORX_(IO_ERROR,errno,"%s",destinationFileName);
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
        error = ERRORX_(IO_ERROR,errno,"%s",sourceFileName);
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
    return ERRORX_(IO_ERROR,errno,"%s",sourceFileName);
  }
  #ifdef HAVE_CHOWN
    if (chown(destinationFileName,fileStat.st_uid,fileStat.st_gid) != 0)
    {
      return ERRORX_(IO_ERROR,errno,"%s",destinationFileName);
    }
  #endif /* HAVE_CHOWN */
  #ifdef HAVE_CHMOD
    if (chmod(destinationFileName,fileStat.st_mode) != 0)
    {
      return ERRORX_(IO_ERROR,errno,"%s",destinationFileName);
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

  return (LSTAT((strlen(fileName) > 0) ? fileName : "",&fileStat) == 0);
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
    DWORD fileAttributes;
  #endif /* PLATFORM_... */

  assert(fileName != NULL);

  #if   defined(PLATFORM_LINUX)
    return access(fileName,F_OK|R_OK) == 0;
  #elif defined(PLATFORM_WINDOWS)
    // Note: access() does not return correct values on MinGW

    fileAttributes = GetFileAttributes(fileName);
    return (fileAttributes & (FILE_ATTRIBUTE_NORMAL|FILE_ATTRIBUTE_READONLY)) == FILE_ATTRIBUTE_NORMAL;
  #endif /* PLATFORM_... */
}

bool File_isWriteable(ConstString fileName)
{
  assert(fileName != NULL);

  return File_isWriteableCString(String_cString(fileName));
}

bool File_isWriteableCString(const char *fileName)
{
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    DWORD fileAttributes;
  #endif /* PLATFORM_... */

  assert(fileName != NULL);

  #if   defined(PLATFORM_LINUX)
    return access((strlen(fileName) > 0) ? fileName : ".",W_OK) == 0;
  #elif defined(PLATFORM_WINDOWS)
    // Note: access() does not return correct values on MinGW

    fileAttributes = GetFileAttributes((strlen(fileName) > 0) ? fileName : ".");
    return    ((fileAttributes & (FILE_ATTRIBUTE_NORMAL|FILE_ATTRIBUTE_READONLY)) == FILE_ATTRIBUTE_NORMAL)
           || ((fileAttributes & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_READONLY)) == FILE_ATTRIBUTE_DIRECTORY);
  #endif /* PLATFORM_... */
}

Errors File_getFileInfo(ConstString fileName,
                        FileInfo    *fileInfo
                       )
{
  FileStat   fileStat;
  DeviceInfo deviceInfo;

  assert(fileName != NULL);
  assert(fileInfo != NULL);

  // get file meta data
  if (LSTAT(String_cString(fileName),&fileStat) != 0)
  {
    return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
  }
  fileInfo->timeLastAccess  = fileStat.st_atime;
  fileInfo->timeModified    = fileStat.st_mtime;
  fileInfo->timeLastChanged = fileStat.st_ctime;
  fileInfo->userId          = fileStat.st_uid;
  fileInfo->groupId         = fileStat.st_gid;
  fileInfo->permission      = (FilePermission)fileStat.st_mode;
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
    (void)getAttributes(fileName,&fileInfo->attributes);
  }
  else if (S_ISDIR(fileStat.st_mode))
  {
    fileInfo->type = FILE_TYPE_DIRECTORY;
    fileInfo->size = 0LL;

    // get file attributes
    (void)getAttributes(fileName,&fileInfo->attributes);
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
    if (Device_getDeviceInfo(&deviceInfo,fileName) == ERROR_NONE)
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

Errors File_setFileInfo(ConstString    fileName,
                        const FileInfo *fileInfo
                       )
{
  struct utimbuf utimeBuffer;
  Errors         error;

  assert(fileName != NULL);
  assert(fileInfo != NULL);

  // set meta data
  switch (fileInfo->type)
  {
    case FILE_TYPE_FILE:
    case FILE_TYPE_DIRECTORY:
    case FILE_TYPE_HARDLINK:
      // set last access, time modified, user/group id, permissions
      utimeBuffer.actime  = fileInfo->timeLastAccess;
      utimeBuffer.modtime = fileInfo->timeModified;
      if (utime(String_cString(fileName),&utimeBuffer) != 0)
      {
        return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
      }
      #ifdef HAVE_CHOWN
        if (chown(String_cString(fileName),fileInfo->userId,fileInfo->groupId) != 0)
        {
          return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
        }
      #endif /* HAVE_CHOWN */
      #ifdef HAVE_CHMOD
        if (chmod(String_cString(fileName),(mode_t)fileInfo->permission) != 0)
        {
          return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
        }
      #endif /* HAVE_CHMOD */

      // set attributes
      error = setAttributes(fileName,fileInfo->attributes);
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    case FILE_TYPE_LINK:
      // set user/group id
      #ifdef HAVE_LCHMOD
        if (lchown(String_cString(fileName),fileInfo->userId,fileInfo->groupId) != 0)
        {
          return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
        }
      #endif /* HAVE_LCHMOD */
      break;
    case FILE_TYPE_SPECIAL:
      // set last access, time modified, user/group id, permissions
      utimeBuffer.actime  = fileInfo->timeLastAccess;
      utimeBuffer.modtime = fileInfo->timeModified;
      if (utime(String_cString(fileName),&utimeBuffer) != 0)
      {
        return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
      }
      #ifdef HAVE_CHOWN
        if (chown(String_cString(fileName),fileInfo->userId,fileInfo->groupId) != 0)
        {
          return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
        }
      #endif /* HAVE_CHOWN */
      #ifdef HAVE_CHMOD
        if (chmod(String_cString(fileName),(mode_t)fileInfo->permission) != 0)
        {
          return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
        }
      #endif /* HAVE_CHMOD */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

#if 0
  // set extended attributes
  error = File_setExtendedAttributes(fileName,&fileInfo->extendedAttributeList);
  if (error != ERROR_NONE)
  {
    return error;
  }
#endif

  return ERROR_NONE;
}

#ifdef NDEBUG
void File_initExtendedAttributes(FileExtendedAttributeList *fileExtendedAttributeList)
#else /* not NDEBUG */
void __File_initExtendedAttributes(const char                *__fileName__,
                                   uint                      __lineNb__,
                                   FileExtendedAttributeList *fileExtendedAttributeList
                                  )
#endif /* NDEBUG */
{
  assert(fileExtendedAttributeList != NULL);

  List_init(fileExtendedAttributeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(fileExtendedAttributeList,sizeof(FileExtendedAttributeList));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,fileExtendedAttributeList,sizeof(FileExtendedAttributeList));
  #endif /* NDEBUG */
}

#ifdef NDEBUG
void File_doneExtendedAttributes(FileExtendedAttributeList *fileExtendedAttributeList)
#else /* not NDEBUG */
void __File_doneExtendedAttributes(const char                *__fileName__,
                                   uint                      __lineNb__,
                                   FileExtendedAttributeList *fileExtendedAttributeList
                                  )
#endif /* NDEBUG */
{
  assert(fileExtendedAttributeList != NULL);

  List_done(fileExtendedAttributeList,(ListNodeFreeFunction)freeExtendedAttributeNode,NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(fileExtendedAttributeList,sizeof(FileExtendedAttributeList));
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,fileExtendedAttributeList,sizeof(FileExtendedAttributeList));
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
  int                       n;
  char                      *names;
  uint                      namesLength;
  const char                *name;
  void                      *data;
  uint                      dataLength;
  FileExtendedAttributeNode *fileExtendedAttributeNode;

  assert(fileExtendedAttributeList != NULL);
  assert(fileName != NULL);

  // init variables
  List_init(fileExtendedAttributeList);

  // allocate buffer for attribute names
  n = llistxattr(String_cString(fileName),NULL,0);
  if (n < 0)
  {
    return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
  }
  namesLength = (uint)n;
  names = (char*)malloc(namesLength);
  if (names == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }

  // get attribute names
  if (llistxattr(String_cString(fileName),names,namesLength) < 0)
  {
    free(names);
    return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
  }

  // get attributes
  name = names;
  while ((uint)(name-names) < namesLength)
  {
    // allocate buffer for data
    n = lgetxattr(String_cString(fileName),name,NULL,0);
    if (n < 0)
    {
      free(names);
      return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
    }
    dataLength = (uint)n;
    data = malloc(dataLength);
    if (data == NULL)
    {
      free(names);
      return ERROR_INSUFFICIENT_MEMORY;
    }

    // get extended attribute
    n = lgetxattr(String_cString(fileName),name,data,dataLength);
    if (n < 0)
    {
      free(data);
      free(names);
      return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
    }

    // store in attribute list
    fileExtendedAttributeNode = LIST_NEW_NODE(FileExtendedAttributeNode);
    if (fileExtendedAttributeNode == NULL)
    {
      free(data);
      free(names);
      return ERROR_INSUFFICIENT_MEMORY;
    }
    fileExtendedAttributeNode->name       = String_newCString(name);
    fileExtendedAttributeNode->data       = data;
    fileExtendedAttributeNode->dataLength = dataLength;
    List_append(fileExtendedAttributeList,fileExtendedAttributeNode);

    // next attribute
    name += strlen(name)+1;
  }

  // free resources
  free(names);

  return ERROR_NONE;
}

Errors File_setExtendedAttributes(ConstString                     fileName,
                                  const FileExtendedAttributeList *fileExtendedAttributeList
                                 )
{
  FileExtendedAttributeNode *fileExtendedAttributeNode;

  assert(fileName != NULL);
  assert(fileExtendedAttributeList != NULL);

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
      return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
    }
  }

  return ERROR_NONE;
}

uint64 File_getFileTimeModified(ConstString fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  if (LSTAT(String_cString(fileName),&fileStat) != 0)
  {
    return 0LL;
  }

  return (uint64)fileStat.st_mtime;
}

Errors File_setPermission(ConstString    fileName,
                          FilePermission permission
                         )
{
  assert(fileName != NULL);

  if (chmod(String_cString(fileName),(mode_t)permission) != 0)
  {
    return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
  }

  return ERROR_NONE;
}

Errors File_setOwner(ConstString fileName,
                     uint32      userId,
                     uint32      groupId
                    )
{
  #ifdef HAVE_CHOWN
    uid_t uid;
    gid_t gid;
  #endif /* HAVE_CHOWN */

  assert(fileName != NULL);

  #ifdef HAVE_CHOWN
    if      (userId == FILE_OWN_USER_ID     ) uid = getuid();
    else if (userId == FILE_DEFAULT_USER_ID ) uid = -1;
    else                                      uid = (uid_t)userId;
    if      (userId == FILE_OWN_GROUP_ID    ) gid = getgid();
    else if (userId == FILE_DEFAULT_GROUP_ID) gid = -1;
    else                                      gid = (uid_t)groupId;

    if (chown(String_cString(fileName),uid,gid) != 0)
    {
      return ERRORX_(IO_ERROR,errno,"%s",String_cString(fileName));
    }

    return ERROR_NONE;
  #else /* not HAVE_CHOWN */
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(userId);
    UNUSED_VARIABLE(groupId);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CHOWN */
}

Errors File_makeDirectory(ConstString    pathName,
                          uint32         userId,
                          uint32         groupId,
                          FilePermission permission
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

  // initialize variables
  directoryName = File_newFileName();
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
      File_setFileNameChar(directoryName,FILES_PATHNAME_SEPARATOR_CHAR);
    }
  }
  if      (!File_exists(directoryName))
  {
    // create root-directory
    #if   (MKDIR_ARGUMENTS_COUNT == 1)
      if (mkdir(String_cString(directoryName)) != 0)
      {
        error = ERRORX_(IO_ERROR,errno,"%s",String_cString(directoryName));
        File_doneSplitFileName(&pathNameTokenizer);
        File_deleteFileName(parentDirectoryName);
        File_deleteFileName(directoryName);
        return error;
      }
    #elif (MKDIR_ARGUMENTS_COUNT == 2)
      if (mkdir(String_cString(directoryName),0777 & ~currentCreationMask) != 0)
      {
        error = ERRORX_(IO_ERROR,errno,"%s",String_cString(directoryName));
        File_doneSplitFileName(&pathNameTokenizer);
        File_deleteFileName(parentDirectoryName);
        File_deleteFileName(directoryName);
        return error;
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
          error = ERRORX_(IO_ERROR,errno,"%s",String_cString(directoryName));
          File_doneSplitFileName(&pathNameTokenizer);
          File_deleteFileName(parentDirectoryName);
          File_deleteFileName(directoryName);
          return error;
        }
      #endif /* HAVE_CHOWN */
    }

    // set permission
    if (permission != FILE_DEFAULT_PERMISSION)
    {
      #ifdef HAVE_CHMOD
        if (chmod(String_cString(directoryName),
                  ((mode_t)permission|PERMISSION_DIRECTORY) & ~currentCreationMask
                 ) != 0
           )
        {
          error = ERROR_(IO_ERROR,errno);
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
    error = ERRORX_(NOT_A_DIRECTORY,0,"%s",String_cString(directoryName));
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
          error = ERRORX_(IO_ERROR,errno,"%s",String_cString(parentDirectoryName));
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
          error = ERRORX_(IO_ERROR,errno,"%s",String_cString(parentDirectoryName));
          File_doneSplitFileName(&pathNameTokenizer);
          File_deleteFileName(parentDirectoryName);
          File_deleteFileName(directoryName);
          return error;
        }

        // create sub-directory
        #if   (MKDIR_ARGUMENTS_COUNT == 1)
          if (mkdir(String_cString(directoryName)) != 0)
          {
            error = ERRORX_(IO_ERROR,errno,"%s",String_cString(directoryName));
            File_doneSplitFileName(&pathNameTokenizer);
            File_deleteFileName(parentDirectoryName);
            File_deleteFileName(directoryName);
            return error;
          }
        #elif (MKDIR_ARGUMENTS_COUNT == 2)
          if (mkdir(String_cString(directoryName),0777 & ~currentCreationMask) != 0)
          {
            error = ERRORX_(IO_ERROR,errno,"%s",String_cString(directoryName));
            File_doneSplitFileName(&pathNameTokenizer);
            File_deleteFileName(parentDirectoryName);
            File_deleteFileName(directoryName);
            return error;
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
              error = ERRORX_(IO_ERROR,errno,"%s",String_cString(directoryName));
              File_doneSplitFileName(&pathNameTokenizer);
              File_deleteFileName(parentDirectoryName);
              File_deleteFileName(directoryName);
              return error;
            }
          #endif /* HAVE_CHOWN */
        }

        // set permission
        if (permission != FILE_DEFAULT_PERMISSION)
        {
          // set permission
          #ifdef HAVE_CHMOD
            if (chmod(String_cString(directoryName),
                      ((mode_t)permission|PERMISSION_DIRECTORY) & ~currentCreationMask
                     ) != 0
               )
            {
              error = ERRORX_(IO_ERROR,errno,"%s",String_cString(directoryName));
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
        error = ERRORX_(NOT_A_DIRECTORY,0,"%s",String_cString(directoryName));
        File_doneSplitFileName(&pathNameTokenizer);
        File_deleteFileName(parentDirectoryName);
        File_deleteFileName(directoryName);
        return error;
      }
    }
  }

  // free resources
  File_doneSplitFileName(&pathNameTokenizer);
  File_deleteFileName(parentDirectoryName);
  File_deleteFileName(directoryName);

  return ERROR_NONE;

  #undef PERMISSION_DIRECTORY
}

Errors File_makeDirectoryCString(const char     *pathName,
                                 uint32         userId,
                                 uint32         groupId,
                                 FilePermission permission
                                )
{
  String string;
  Errors error;

  string = File_setFileNameCString(File_newFileName(),pathName);
  error = File_makeDirectory(string,userId,groupId,permission);
  File_deleteFileName(string);

  return error;
}

Errors File_readLink(String      fileName,
                     ConstString linkName
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

    if (result != -1)
    {
      String_setBuffer(fileName,buffer,result);
      free(buffer);
      return ERROR_NONE;
    }
    else
    {
      error = ERRORX_(IO_ERROR,errno,"%s",String_cString(linkName));
      free(buffer);
      return error;
    }
  #else /* not HAVE_READLINK */
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(linkName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_READLINK */
}

Errors File_makeLink(ConstString linkName,
                     ConstString fileName
                    )
{
  assert(linkName != NULL);
  assert(fileName != NULL);

  #ifdef HAVE_SYMLINK
    unlink(String_cString(linkName));
    if (symlink(String_cString(fileName),String_cString(linkName)) != 0)
    {
      return ERRORX_(IO_ERROR,errno,"%s",String_cString(linkName));
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
      return ERRORX_(IO_ERROR,errno,"%s",String_cString(linkName));
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

  #ifdef HAVE_MKNOD
    unlink(String_cString(name));
    switch (type)
    {
      case FILE_SPECIAL_TYPE_CHARACTER_DEVICE:
        if (mknod(String_cString(name),S_IFCHR|0600,makedev(major,minor)) != 0)
        {
          return ERRORX_(IO_ERROR,errno,"%s",String_cString(name));
        }
        break;
      case FILE_SPECIAL_TYPE_BLOCK_DEVICE:
        if (mknod(String_cString(name),S_IFBLK|0600,makedev(major,minor)) != 0)
        {
          return ERRORX_(IO_ERROR,errno,"%s",String_cString(name));
        }
        break;
      case FILE_SPECIAL_TYPE_FIFO:
        if (mknod(String_cString(name),S_IFIFO|0666,0) != 0)
        {
          return ERRORX_(IO_ERROR,errno,"%s",String_cString(name));
        }
        break;
      case FILE_SPECIAL_TYPE_SOCKET:
        if (mknod(String_cString(name),S_IFSOCK|0600,0) != 0)
        {
          return ERRORX_(IO_ERROR,errno,"%s",String_cString(name));
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
  assert(fileSystemInfo != NULL);

  #ifdef HAVE_STATVFS
    if (statvfs(String_cString(pathName),&fileSystemStat) != 0)
    {
      return ERRORX_(IO_ERROR,errno,"%s",String_cString(pathName));
    }

    fileSystemInfo->blockSize         = fileSystemStat.f_bsize;
    fileSystemInfo->freeBytes         = (uint64)fileSystemStat.f_bavail*(uint64)fileSystemStat.f_bsize;
    fileSystemInfo->totalBytes        = (uint64)fileSystemStat.f_blocks*(uint64)fileSystemStat.f_frsize;
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
  char s[64];
  struct tm tm;

  assert(s != NULL);
  assert(fileCast != NULL);

  String_clear(string);

  localtime_r(&fileCast->mtime,&tm);
  strftime(s,sizeof(s),"%F %T",&tm);
  String_format(string,"mtime=%s",s);

  String_appendChar(string,' ');

  localtime_r(&fileCast->ctime,&tm);
  strftime(s,sizeof(s),"%F %T",&tm);
  String_format(string,"ctime=%s",s);

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
    List_done(&debugClosedFileList,NULL,NULL);
    List_done(&debugOpenFileList,NULL,NULL);
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

  File_debugPrintInfo(CALLBACK_NULL);
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
