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
#ifdef HAVE_FSEEKO
  #define FSEEK fseeko
#elif HAVE__FSEEKI64
  #define FSEEK _fseeki64
#else
  #define FSEEK fseek
#endif

#ifdef HAVE_FTELLO
  #define FTELL ftello
#elif HAVE__FTELLI64
  #define FTELL _ftelli64
#else
  #define FTELL ftell
#endif

#ifdef HAVE_STAT64
  #define STAT  stat64
  #define LSTAT lstat64
  typedef struct stat64 FileStat;
#elif HAVE___STAT64
  #define STAT  stat64
  #define LSTAT lstat64
  typedef struct __stat64 FileStat;
#elif HAVE__STATI64
  #define STAT  _stati64
  #define LSTAT _stati64
  typedef struct _stati64 FileStat;
#elif HAVE_STAT
  #define STAT  stat
  #define LSTAT lstat
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
LOCAL void fileCheckValid(const char *fileName,
                          ulong      lineNb,
                          FileHandle *fileHandle
                         )
{
  DebugFileNode *debugFileNode;

  assert(fileHandle != NULL);

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
        debugDumpCurrentStackTrace(stderr,"",0);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR_AT(fileName,
                             lineNb,
                             "File 0x%08x was closed at %s, line %lu",
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
        debugDumpCurrentStackTrace(stderr,"",0);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR("File 0x%08x is not open",
                          fileHandle
                         );
    }
  }
  pthread_mutex_unlock(&debugFileLock);
}
#endif /* NDEBUG */

/***********************************************************************\
* Name   : getAttributes
* Purpose: get file attributes
* Input  : fileName - file name
* Output : attributes - file attributes
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getAttributes(const String fileName, FileAttributes *fileAttributes)
{
  long   attributes;
  #ifdef FS_IOC_GETFLAGS
    int    handle;
    Errors error;
  #endif /* FS_IOC_GETFLAGS */

  assert(fileName != NULL);
  assert(fileAttributes != NULL);

  attributes = 0LL;
  #ifdef FS_IOC_GETFLAGS
    // get file attributes
    handle = open(String_cString(fileName),O_RDONLY|O_NONBLOCK);
    if (handle == -1)
    {
      return ERRORX_(IO_ERROR,errno,String_cString(fileName));
    }
    if (ioctl(handle,FS_IOC_GETFLAGS,&attributes) != 0)
    {
      error = ERRORX_(IO_ERROR,errno,String_cString(fileName));
      close(handle);
      return error;
    }
    close(handle);
  #else /* FS_IOC_GETFLAGS */
    UNUSED_VARIABLE(fileName);
  #endif /* FS_IOC_GETFLAGS */

  (*fileAttributes) = 0LL;
  #ifdef HAVE_FS_COMPR_FL
    if ((attributes & FILE_ATTRIBUTE_COMPRESS   ) != 0LL) (*fileAttributes) |= FILE_ATTRIBUTE_COMPRESS;
  #endif
  #ifdef HAVE_FS_NOCOMP_FL
    if ((attributes & FILE_ATTRIBUTE_NO_COMPRESS) != 0LL) (*fileAttributes) |= FILE_ATTRIBUTE_NO_COMPRESS;
  #endif
  #ifdef HAVE_FS_NODUMP_FL
    if ((attributes & FILE_ATTRIBUTE_NO_DUMP    ) != 0LL) (*fileAttributes) |= FILE_ATTRIBUTE_NO_DUMP;
  #endif

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

/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/

String File_newFileName(void)
{
  return String_new();
}

String File_duplicateFileName(const String fromFileName)
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

String File_setFileName(String fileName, const String name)
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

String File_appendFileName(String fileName, const String name)
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

String File_getFilePathName(String pathName, const String fileName)
{
  assert(fileName != NULL);
  assert(pathName != NULL);

  return File_getFilePathNameCString(pathName,String_cString(fileName));
}

String File_getFilePathNameCString(String pathName, const char *fileName)
{
  const char *lastPathSeparator;

  assert(fileName != NULL);
  assert(pathName != NULL);

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

  return pathName;
}

String File_getFileBaseName(String baseName, const String fileName)
{
  assert(fileName != NULL);
  assert(baseName != NULL);

  return File_getFileBaseNameCString(baseName,String_cString(fileName));
}

String File_getFileBaseNameCString(String baseName, const char *fileName)
{
  const char *lastPathSeparator;

  assert(fileName != NULL);
  assert(baseName != NULL);

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

  return baseName;
}

void File_splitFileName(const String fileName, String *pathName, String *baseName)
{
  assert(fileName != NULL);
  assert(pathName != NULL);
  assert(baseName != NULL);

  (*pathName) = File_getFilePathName(File_newFileName(),fileName);
  (*baseName) = File_getFileBaseName(File_newFileName(),fileName);
}

void File_initSplitFileName(StringTokenizer *stringTokenizer, String fileName)
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

bool File_getNextSplitFileName(StringTokenizer *stringTokenizer, String *const name)
{
  assert(stringTokenizer != NULL);
  assert(name != NULL);

  return String_getNextToken(stringTokenizer,name,NULL);
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
Errors File_getTmpFile(FileHandle   *fileHandle,
                       const String pattern,
                       const String directory
                      )
#else /* not NDEBUG */
Errors __File_getTmpFile(const char   *__fileName__,
                         uint         __lineNb__,
                         FileHandle   *fileHandle,
                         const String pattern,
                         const String directory
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
Errors File_getTmpFileCString(FileHandle   *fileHandle,
                              char const   *pattern,
                              const String directory
                             )
#else /* not NDEBUG */
Errors __File_getTmpFileCString(const char   *__fileName__,
                                uint         __lineNb__,
                                FileHandle   *fileHandle,
                                char const   *pattern,
                                const String directory
                               )
#endif /* NDEBUG */
{
  char   *s;
  int    handle;
  Errors error;
  #ifndef NDEBUG
    DebugFileNode *debugFileNode;
  #endif /* not NDEBUG */

  assert(fileHandle != NULL);

  if (pattern == NULL) pattern = "tmp-XXXXXX";

  if (!String_isEmpty(directory))
  {
    s = (char*)malloc(String_length(directory)+strlen(FILE_SEPARATOR_STRING)+strlen(pattern)+1);
    if (s == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    strcpy(s,String_cString(directory));
    strcat(s,FILE_SEPARATOR_STRING);
    strcat(s,pattern);
  }
  else
  {
    s = (char*)malloc(strlen(pattern)+1);
    if (s == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    strcpy(s,pattern);
  }

  #ifdef HAVE_MKSTEMP
    handle = mkstemp(s);
    if (handle == -1)
    {
      error = ERRORX_(IO_ERROR,errno,s);
      free(s);
      return error;
    }
    fileHandle->file = fdopen(handle,"w+b");
    if (fileHandle->file == NULL)
    {
      error = ERRORX_(CREATE_FILE,errno,s);
      free(s);
      return error;
    }
  #elif HAVE_MKTEMP
    // Note: there is a race-condition when mktemp() and open() is used!
    if (strcmp(mktemp(s),"") == 0)
    {
      error = ERRORX_(IO_ERROR,errno,s);
      free(s);
      return error;
    }
    fileHandle->file = fopen(s,"w+b");
      if (fileHandle->file == NULL)
    {
      error = ERRORX_(CREATE_FILE,errno,s);
      free(s);
      return error;
    }
  #else /* not HAVE_MKSTEMP || HAVE_MKTEMP */
    #error mkstemp() nor mktemp() available
  #endif /* HAVE_MKSTEMP || HAVE_MKTEMP */
  #ifdef NDEBUG
    if (unlink(s) != 0)
    {
      error = ERRORX_(IO_ERROR,errno,s);
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
        fprintf(stderr,"DEBUG WARNING: file '%s' at %s, line %lu opened again at %s, line %u\n",
                String_cString(debugFileNode->fileHandle->name),
                debugFileNode->fileName,
                debugFileNode->lineNb,
                __fileName__,
                __lineNb__
               );
        #ifdef HAVE_BACKTRACE
          debugDumpCurrentStackTrace(stderr,"",0);
        #endif /* HAVE_BACKTRACE */
        HALT_INTERNAL_ERROR("");
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

Errors File_getTmpFileName(String fileName, const String pattern, const String directory)
{
  return File_getTmpFileNameCString(fileName,String_cString(pattern),directory);
}

Errors File_getTmpFileNameCString(String fileName, char const *pattern, const String directory)
{
  char   *s;
  int    handle;
  Errors error;

  assert(fileName != NULL);

  if (pattern == NULL) pattern = "tmp-XXXXXX";

  if (!String_isEmpty(directory))
  {
    s = (char*)malloc(String_length(directory)+strlen(FILE_SEPARATOR_STRING)+strlen(pattern)+1);
    if (s == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    strcpy(s,String_cString(directory));
    strcat(s,FILE_SEPARATOR_STRING);
    strcat(s,pattern);
  }
  else
  {
    s = (char*)malloc(strlen(pattern)+1);
    if (s == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    strcpy(s,pattern);
  }

  #ifdef HAVE_MKSTEMP
    handle = mkstemp(s);
    if (handle == -1)
    {
      error = ERRORX_(IO_ERROR,errno,s);
      free(s);
      return error;
    }
    close(handle);
  #elif HAVE_MKTEMP
    // Note: there is a race-condition when mktemp() and open() is used!
    if (strcmp(mktemp(s),"") == 0)
    {
      error = ERRORX_(IO_ERROR,errno,s);
      free(s);
      return error;
    }
    handle = open(s,O_CREAT|O_EXCL);
    if (handle == -1)
    {
      error = ERRORX_(IO_ERROR,errno,s);
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

Errors File_getTmpDirectoryName(String directoryName, const String pattern, const String directory)
{
  return File_getTmpDirectoryNameCString(directoryName,String_cString(pattern),directory);
}

Errors File_getTmpDirectoryNameCString(String directoryName, char const *pattern, const String directory)
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
      error = ERRORX_(IO_ERROR,errno,s);
      free(s);
      return error;
    }
  #elif HAVE_MKTEMP
    // Note: there is a race-condition when mktemp() and mkdir() is used!
    if (strcmp(mktemp(s),"") == 0)
    {
      error = ERRORX_(IO_ERROR,errno,s);
      free(s);
      return error;
    }

    #if   (MKDIR_ARGUMENTS_COUNT == 1)
      // create directory
      if (mkdir(s) != 0)
      {
        error = ERRORX_(IO_ERROR,errno,s);
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
        error = ERRORX_(IO_ERROR,errno,s);
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
Errors File_open(FileHandle    *fileHandle,
                 const String  fileName,
                 FileModes     fileMode
                )
#else /* not NDEBUG */
Errors __File_open(const char   *__fileName__,
                   uint         __lineNb__,
                   FileHandle   *fileHandle,
                   const String fileName,
                   FileModes    fileMode
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
  off_t  n;
  Errors error;
  String pathName;
  #ifndef NDEBUG
    DebugFileNode *debugFileNode;
  #endif /* not NDEBUG */

  assert(fileHandle != NULL);
  assert(fileName != NULL);

  switch (fileMode & FILE_OPEN_MASK_MODE)
  {
    case FILE_OPEN_CREATE:
      // create file
      fileHandle->file = fopen(fileName,"w+b");
      if (fileHandle->file == NULL)
      {
        return ERRORX_(CREATE_FILE,errno,fileName);
      }

      fileHandle->name  = String_newCString(fileName);
      fileHandle->index = 0LL;
      fileHandle->size  = 0LL;
      #ifndef NDEBUG
        fileHandle->deleteOnCloseFlag = FALSE;
      #endif /* not NDEBUG */
      break;
    case FILE_OPEN_READ:
      // open file for reading
      fileHandle->file = fopen(fileName,"rb");
      if (fileHandle->file == NULL)
      {
        return ERRORX_(OPEN_FILE,errno,fileName);
      }

      // get file size
      if (FSEEK(fileHandle->file,(off_t)0,SEEK_END) == -1)
      {
        error = ERRORX_(IO_ERROR,errno,fileName);
        fclose(fileHandle->file);
        return error;
      }
      n = FTELL(fileHandle->file);
      if (n == (off_t)(-1))
      {
        error = ERRORX_(IO_ERROR,errno,fileName);
        fclose(fileHandle->file);
        return error;
      }
      if (FSEEK(fileHandle->file,(off_t)0,SEEK_SET) == -1)
      {
        error = ERRORX_(IO_ERROR,errno,fileName);
        fclose(fileHandle->file);
        return error;
      }

      fileHandle->name  = String_newCString(fileName);
      fileHandle->index = 0LL;
      fileHandle->size  = (uint64)n;
      #ifndef NDEBUG
        fileHandle->deleteOnCloseFlag = FALSE;
      #endif /* not NDEBUG */
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

      // open existing file for writing
      fileHandle->file = fopen(fileName,"r+b");
      if (fileHandle->file == NULL)
      {
        if (errno == ENOENT)
        {
          fileHandle->file = fopen(fileName,"wb");
          if (fileHandle->file == NULL)
          {
            return ERRORX_(OPEN_FILE,errno,fileName);
          }
        }
        else
        {
          return ERRORX_(OPEN_FILE,errno,fileName);
        }
      }

      fileHandle->name  = String_newCString(fileName);
      fileHandle->index = 0LL;
      fileHandle->size  = 0LL;
      #ifndef NDEBUG
        fileHandle->deleteOnCloseFlag = FALSE;
      #endif /* not NDEBUG */
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

      // open existing file for writing
      fileHandle->file = fopen(fileName,"ab");
      if (fileHandle->file == NULL)
      {
        return ERRORX_(OPEN_FILE,errno,fileName);
      }

      // get file size
      n = FTELL(fileHandle->file);
      if (n == (off_t)(-1))
      {
        error = ERRORX_(IO_ERROR,errno,fileName);
        fclose(fileHandle->file);
        return error;
      }

      fileHandle->name  = String_newCString(fileName);
      fileHandle->index = (uint64)n;
      fileHandle->size  = (uint64)n;
      #ifndef NDEBUG
        fileHandle->deleteOnCloseFlag = FALSE;
      #endif /* not NDEBUG */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  fileHandle->mode = fileMode;
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
          debugDumpCurrentStackTrace(stderr,"",0);
        #endif /* HAVE_BACKTRACE */
        HALT_INTERNAL_ERROR("File '%s' at %s, line %lu opened again at %s, line %u",
                            String_cString(debugFileNode->fileHandle->name),
                            debugFileNode->fileName,
                            debugFileNode->lineNb,
                            __fileName__,
                            __lineNb__
                           );
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
  off_t  n;
  Errors error;
  #ifndef NDEBUG
    DebugFileNode *debugFileNode;
  #endif /* not NDEBUG */

  assert(fileHandle != NULL);
  assert(fileDescriptor >= 0);

  switch (fileMode & FILE_OPEN_MASK_MODE)
  {
    case FILE_OPEN_CREATE:
      // create file
      fileHandle->file = fdopen(fileDescriptor,"wb");
      if (fileHandle->file == NULL)
      {
        return ERROR_(CREATE_FILE,errno);
      }

      fileHandle->name  = NULL;
      fileHandle->index = 0LL;
      fileHandle->size  = 0LL;
      break;
    case FILE_OPEN_READ:
      // open file for reading
      fileHandle->file = fdopen(fileDescriptor,"rb");
      if (fileHandle->file == NULL)
      {
        return ERROR_(OPEN_FILE,errno);
      }

      fileHandle->name  = NULL;
      fileHandle->index = 0LL;
      fileHandle->size  = 0LL;
      break;
    case FILE_OPEN_WRITE:
      // open file for writing
      fileHandle->file = fdopen(fileDescriptor,"wb");
      if (fileHandle->file == NULL)
      {
        return ERROR_(OPEN_FILE,errno);
      }

      fileHandle->name  = NULL;
      fileHandle->index = 0LL;
      fileHandle->size  = 0LL;
      break;
    case FILE_OPEN_APPEND:
      // open file for writing
      fileHandle->file = fdopen(fileDescriptor,"ab");
      if (fileHandle->file == NULL)
      {
        return ERROR_(OPEN_FILE,errno);
      }

      // get file size
      n = FTELL(fileHandle->file);
      if (n == (off_t)(-1))
      {
        error = ERROR_(IO_ERROR,errno);
        fclose(fileHandle->file);
        return error;
      }

      fileHandle->name  = NULL;
      fileHandle->index = (uint64)n;
      fileHandle->size  = (uint64)n;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  fileHandle->mode = fileMode;
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
          debugDumpCurrentStackTrace(stderr,"",0);
        #endif /* HAVE_BACKTRACE */
        HALT_INTERNAL_ERROR("File '%s' at %s, line %lu opened again at %s, line %lu",
                            String_cString(debugFileNode->fileHandle->name),
                            debugFileNode->fileName,
                            debugFileNode->lineNb,
                            __fileName__,
                            __lineNb__
                           );
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

#ifdef NDEBUG
Errors File_close(FileHandle *fileHandle)
#else /* not NDEBUG */
Errors __File_close(const char *__fileName__,
                    uint       __lineNb__,
                    FileHandle *fileHandle
                   )
#endif /* NDEBUG */
{
  #ifndef NDEBUG
    DebugFileNode *debugFileNode;
  #endif /* not NDEBUG */

  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
  FILE_CHECK_VALID(fileHandle);

  #ifndef NDEBUG
    if (fileHandle->deleteOnCloseFlag)
    {
      if (unlink(String_cString(fileHandle->name)) != 0)
      {
        return ERRORX_(IO_ERROR,errno,String_cString(fileHandle->name));
      }
    }
  #endif /* not NDEBUG */

  // free caches if requested
  if ((fileHandle->mode & FILE_OPEN_NO_CACHE) != 0)
  {
    File_dropCaches(fileHandle,0LL,0LL,FALSE);
  }

  // close file
  fclose(fileHandle->file);

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
          debugFileNode = (DebugFileNode*)List_getFirst(&debugClosedFileList);
          LIST_DELETE_NODE(debugFileNode);
        }
      }
      else
      {
        #ifdef HAVE_BACKTRACE
          debugDumpCurrentStackTrace(stderr,"",0);
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

  return ERROR_NONE;
}

bool File_eof(FileHandle *fileHandle)
{
  int  ch;
  bool eofFlag;

  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
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

  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
  assert(buffer != NULL);
  FILE_CHECK_VALID(fileHandle);

  if (bytesRead != NULL)
  {
    // read as much data as possible
    n = fread(buffer,1,bufferLength,fileHandle->file);
//fprintf(stderr,"%s, %d: much %x bufferLength=%lu n=%ld %d\n",__FILE__,__LINE__,fileHandle->file,bufferLength,n,ferror(fileHandle->file));
    if ((n <= 0) && ferror(fileHandle->file))
    {
      return ERRORX_(IO_ERROR,errno,String_cString(fileHandle->name));
    }
    fileHandle->index += (uint64)n;
    (*bytesRead) = n;
  }
  else
  {
    // read all requested data
    while (bufferLength > 0L)
    {
      n = fread(buffer,1,bufferLength,fileHandle->file);
//fprintf(stderr,"%s, %d: all bufferLength=%lu n=%ld %d\n",__FILE__,__LINE__,bufferLength,n,ferror(fileHandle->file));
      if ((n <= 0) && ferror(fileHandle->file))
      {
        return ERRORX_(IO_ERROR,errno,String_cString(fileHandle->name));
      }
      buffer = (byte*)buffer+n;
      bufferLength -= (ulong)n;
      fileHandle->index += (uint64)n;
    }
  }

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

  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
  assert(buffer != NULL);
  FILE_CHECK_VALID(fileHandle);

  n = fwrite(buffer,1,bufferLength,fileHandle->file);
  if (n > 0) fileHandle->index += n;
//fprintf(stderr,"%s, %d: write %x bufferLength=%lu n=%ld %d fileHandle->index=%llu\n",__FILE__,__LINE__,fileHandle->file,bufferLength,n,ferror(fileHandle->file),fileHandle->index);
  if (fileHandle->index > fileHandle->size) fileHandle->size = fileHandle->index;
  if (n != (ssize_t)bufferLength)
  {
    return ERRORX_(IO_ERROR,errno,String_cString(fileHandle->name));
  }

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

  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
  assert(line != NULL);
  FILE_CHECK_VALID(fileHandle);

  String_clear(line);
  if (StringList_isEmpty(&fileHandle->lineBufferList))
  {
    do
    {
      ch = getc(fileHandle->file);
      if (ch >= 0) fileHandle->index += 1;
      if (ch < 0)
      {
        return ERRORX_(IO_ERROR,errno,String_cString(fileHandle->name));
      }
      if (((char)ch != '\n') && ((char)ch != '\r'))
      {
        String_appendChar(line,ch);
      }
    }
    while (((char)ch != '\r') && ((char)ch != '\n'));
    if      ((char)ch == '\r')
    {
      ch = getc(fileHandle->file);
      if (ch >= 0) fileHandle->index += 1;
      if (ch != '\n')
      {
        fileHandle->index -= 1;
        ungetc(ch,fileHandle->file);
      }
    }
  }
  else
  {
    StringList_getLast(&fileHandle->lineBufferList,line);
  }

  return ERROR_NONE;
}

Errors File_writeLine(FileHandle   *fileHandle,
                      const String line
                     )
{
  Errors error;

  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
  assert(line != NULL);
  FILE_CHECK_VALID(fileHandle);

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

  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
  assert(format != NULL);
  FILE_CHECK_VALID(fileHandle);

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
      error = ERRORX_(IO_ERROR,errno,String_cString(sourceFileHandle->name));
      free(buffer);
      return error;
    }

    n = fwrite(buffer,1,bufferLength,destinationFileHandle->file);
    if (n != (ssize_t)bufferLength)
    {
      error = ERRORX_(IO_ERROR,errno,String_cString(destinationFileHandle->name));
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
  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
  FILE_CHECK_VALID(fileHandle);

  if (fflush(fileHandle->file) != 0)
  {
    return ERRORX_(IO_ERROR,errno,String_cString(fileHandle->name));
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

  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
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
      StringList_getLast(&fileHandle->lineBufferList,line);
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

void File_ungetLine(FileHandle   *fileHandle,
                    const String line,
                    uint         *lineNb
                   )
{
  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
  FILE_CHECK_VALID(fileHandle);

  StringList_append(&fileHandle->lineBufferList,line);
  if (lineNb != NULL) (*lineNb)--;
}

uint64 File_getSize(FileHandle *fileHandle)
{
  assert(fileHandle != NULL);
  FILE_CHECK_VALID(fileHandle);

  return fileHandle->size;
}

Errors File_tell(FileHandle *fileHandle, uint64 *offset)
{
  off_t n;

  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
  assert(offset != NULL);
  FILE_CHECK_VALID(fileHandle);

  n = FTELL(fileHandle->file);
  if (n == (off_t)(-1))
  {
    return ERRORX_(IO_ERROR,errno,String_cString(fileHandle->name));
  }
  assert((uint64)n == fileHandle->index);

  (*offset) = fileHandle->index;

  return ERROR_NONE;
}

Errors File_seek(FileHandle *fileHandle,
                 uint64     offset
                )
{
  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
  FILE_CHECK_VALID(fileHandle);

  if (FSEEK(fileHandle->file,(off_t)offset,SEEK_SET) == -1)
  {
    return ERRORX_(IO_ERROR,errno,String_cString(fileHandle->name));
  }
  fileHandle->index = offset;

  return ERROR_NONE;
}

Errors File_truncate(FileHandle *fileHandle,
                     uint64     size
                    )
{
  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
  FILE_CHECK_VALID(fileHandle);

  if (size < fileHandle->size)
  {
    (void)fflush(fileHandle->file);
    if (ftruncate(fileno(fileHandle->file),(off_t)size) != 0)
    {
      return ERRORX_(IO_ERROR,errno,String_cString(fileHandle->name));
    }
    if (fileHandle->index > size)
    {
      if (FSEEK(fileHandle->file,(off_t)size,SEEK_SET) == -1)
      {
        return ERRORX_(IO_ERROR,errno,String_cString(fileHandle->name));
      }
      fileHandle->index = size;
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

  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
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

  #ifdef HAVE_POSIX_FADVISE
    if (posix_fadvise(handle,offset,length,POSIX_FADV_DONTNEED) != 0)
    {
      return ERRORX_(IO_ERROR,errno,String_cString(fileHandle->name));
    }
  #else
    UNUSED_VARIABLE(offset);
    UNUSED_VARIABLE(length);
  #endif /* HAVE_POSIX_FADVISE */

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors File_openDirectoryList(DirectoryListHandle *directoryListHandle,
                              const String        pathName
                             )
{
  assert(directoryListHandle != NULL);
  assert(pathName != NULL);

  directoryListHandle->dir = opendir(String_cString(pathName));
  if (directoryListHandle->dir == NULL)
  {
    return ERRORX_(OPEN_DIRECTORY,errno,String_cString(pathName));
  }

  directoryListHandle->name  = String_duplicate(pathName);
  directoryListHandle->entry = NULL;

  return ERROR_NONE;
}

Errors File_openDirectoryListCString(DirectoryListHandle *directoryListHandle,
                                     const char          *pathName
                                    )
{
  assert(directoryListHandle != NULL);
  assert(pathName != NULL);

  directoryListHandle->dir = opendir(pathName);
  if (directoryListHandle->dir == NULL)
  {
    return ERRORX_(OPEN_DIRECTORY,errno,pathName);
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
    return ERRORX_(IO_ERROR,errno,String_cString(directoryListHandle->name));
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
  #if defined(HAVE_SYSCONF) && defined(HAVE_GETPWNAM_R)
    long          bufferSize;
    char          *buffer;
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
    if (getpwnam_r(name,&passwordEntry,buffer,bufferSize,&result) != 0)
    {
      free(buffer);
      return FILE_DEFAULT_USER_ID;
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
}

const char *File_userIdToUserName(char *name, uint nameSize, uint32 userId)
{
  #if defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R)
    long          bufferSize;
    char          *buffer;
    struct passwd groupEntry;
    struct passwd *result;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R) */

  assert(name != NULL);
  assert(nameSize > 0);

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
    if (getpwuid_r((uid_t)userId,&groupEntry,buffer,bufferSize,&result) != 0)
    {
      free(buffer);
      return NULL;
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
}

uint32 File_groupNameToGroupId(const char *name)
{
  #if defined(HAVE_SYSCONF) && defined(HAVE_GETGRNAM_R)
    long         bufferSize;
    char         *buffer;
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
    if (getgrnam_r(name,&groupEntry,buffer,bufferSize,&result) != 0)
    {
      free(buffer);
      return FILE_DEFAULT_GROUP_ID;
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
}

const char *File_groupIdToGroupName(char *name, uint nameSize, uint32 groupId)
{
  #if defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R)
    long         bufferSize;
    char         *buffer;
    struct group groupEntry;
    struct group *result;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETGRGID_R) */

  assert(name != NULL);
  assert(nameSize > 0);

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
    if (getgrgid_r((gid_t)groupId,&groupEntry,buffer,bufferSize,&result) != 0)
    {
      free(buffer);
      return NULL;
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
}

FileTypes File_getType(const String fileName)
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

Errors File_getData(const String fileName,
                    void         **data,
                    ulong        *size
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

Errors File_delete(const String fileName, bool recursiveFlag)
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

  if (LSTAT(String_cString(fileName),&fileStat) != 0)
  {
    return ERRORX_(IO_ERROR,errno,String_cString(fileName));
  }

  if      (   S_ISREG(fileStat.st_mode)
           #ifdef S_ISLNK
           || S_ISLNK(fileStat.st_mode)
           #endif /* S_ISLNK */
          )
  {
    if (unlink(String_cString(fileName)) != 0)
    {
      return ERRORX_(IO_ERROR,errno,String_cString(fileName));
    }
  }
  else if (S_ISDIR(fileStat.st_mode))
  {
    error = ERROR_NONE;
    if (recursiveFlag)
    {
      // delete entries in directory
      StringList_init(&directoryList);
      StringList_append(&directoryList,fileName);
      directoryName = File_newFileName();
      name = File_newFileName();
      while (!StringList_isEmpty(&directoryList) && (error == ERROR_NONE))
      {
        StringList_getFirst(&directoryList,directoryName);

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
                    error = ERRORX_(IO_ERROR,errno,String_cString(name));
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
              error = ERRORX_(IO_ERROR,errno,String_cString(directoryName));
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
      if (rmdir(String_cString(fileName)) != 0)
      {
        error = ERRORX_(IO_ERROR,errno,String_cString(fileName));
      }
    }

    return error;
  }

  return ERROR_NONE;
}

Errors File_rename(const String oldFileName,
                   const String newFileName
                  )
{
  Errors error;

  assert(oldFileName != NULL);
  assert(newFileName != NULL);

  // try rename
  if (rename(String_cString(oldFileName),String_cString(newFileName)) != 0)
  {
    // copy to new file
    error = File_copy(oldFileName,newFileName);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // delete old file
    if (unlink(String_cString(oldFileName)) != 0)
    {
      return ERRORX_(IO_ERROR,errno,String_cString(oldFileName));
    }
  }

  return ERROR_NONE;
}

Errors File_copy(const String sourceFileName,
                 const String destinationFileName
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
  sourceFile = fopen(String_cString(sourceFileName),"r");
  if (sourceFile == NULL)
  {
    error = ERRORX_(OPEN_FILE,errno,String_cString(sourceFileName));
    free(buffer);
    return error;
  }
  destinationFile = fopen(String_cString(destinationFileName),"w");
  if (destinationFile == NULL)
  {
    error = ERRORX_(OPEN_FILE,errno,String_cString(destinationFileName));
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
        error = ERRORX_(IO_ERROR,errno,String_cString(destinationFileName));
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
        error = ERRORX_(IO_ERROR,errno,String_cString(sourceFileName));
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
  if (LSTAT(String_cString(sourceFileName),&fileStat) != 0)
  {
    return ERRORX_(IO_ERROR,errno,String_cString(sourceFileName));
  }
  #ifdef HAVE_CHOWN
    if (chown(String_cString(destinationFileName),fileStat.st_uid,fileStat.st_gid) != 0)
    {
      return ERRORX_(IO_ERROR,errno,String_cString(destinationFileName));
    }
  #endif /* HAVE_CHOWN */
  #ifdef HAVE_CHMOD
    if (chmod(String_cString(destinationFileName),fileStat.st_mode) != 0)
    {
      return ERRORX_(IO_ERROR,errno,String_cString(destinationFileName));
    }
  #endif /* HAVE_CHMOD */

  return ERROR_NONE;

  #undef BUFFER_SIZE
}

bool File_exists(const String fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  return (LSTAT(String_cString(fileName),&fileStat) == 0);
}

bool File_existsCString(const char *fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  return (LSTAT((strlen(fileName) > 0) ? fileName : "",&fileStat) == 0);
}

bool File_isFile(const String fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  return (   (STAT(String_cString(fileName),&fileStat) == 0)
          && S_ISREG(fileStat.st_mode)
         );
}

bool File_isFileCString(const char *fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  return (   (STAT(fileName,&fileStat) == 0)
          && S_ISREG(fileStat.st_mode)
         );
}

bool File_isDirectory(const String fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  return (   (STAT(String_cString(fileName),&fileStat) == 0)
          && S_ISDIR(fileStat.st_mode)
         );
}

bool File_isDirectoryCString(const char *fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  return (   (STAT(fileName,&fileStat) == 0)
          && S_ISDIR(fileStat.st_mode)
         );
}

bool File_isDevice(const String fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  return (   (STAT(String_cString(fileName),&fileStat) == 0)
          && (S_ISCHR(fileStat.st_mode) || S_ISBLK(fileStat.st_mode))
         );
}

bool File_isDeviceCString(const char *fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  return (   (STAT(fileName,&fileStat) == 0)
          && (S_ISCHR(fileStat.st_mode) || S_ISBLK(fileStat.st_mode))
         );
}

bool File_isReadable(const String fileName)
{
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

bool File_isWriteable(const String fileName)
{
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

Errors File_getFileInfo(FileInfo     *fileInfo,
                        const String fileName
                       )
{
  FileStat   fileStat;
  DeviceInfo deviceInfo;
  struct
  {
    time_t d0;
    time_t d1;
  } cast;
  Errors     error;

  assert(fileName != NULL);
  assert(fileInfo != NULL);

  // get file meta data
  if (LSTAT(String_cString(fileName),&fileStat) != 0)
  {
    return ERRORX_(IO_ERROR,errno,String_cString(fileName));
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
  cast.d0 = fileStat.st_mtime;
  cast.d1 = fileStat.st_ctime;
  memcpy(fileInfo->cast,&cast,sizeof(FileCast));

  // store specific meta data
  if      (S_ISREG(fileStat.st_mode))
  {
    fileInfo->type = (fileStat.st_nlink > 1) ? FILE_TYPE_HARDLINK : FILE_TYPE_FILE;
    fileInfo->size = fileStat.st_size;

    // get file attributes
    error = getAttributes(fileName,&fileInfo->attributes);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  else if (S_ISDIR(fileStat.st_mode))
  {
    fileInfo->type = FILE_TYPE_DIRECTORY;
    fileInfo->size = 0LL;

    // get file attributes
    error = getAttributes(fileName,&fileInfo->attributes);
    if (error != ERROR_NONE)
    {
      return error;
    }
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
    fileInfo->size        = -1LL;
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

#if 0
  // get extended attributes
  error = File_getExtendedAttributes(&fileInfo->extendedAttributeList,fileName);
  if (error != ERROR_NONE)
  {
    return error;
  }
  #endif

  return ERROR_NONE;
}

Errors File_setFileInfo(const String fileName,
                        FileInfo     *fileInfo
                       )
{
  struct utimbuf utimeBuffer;
//  Errors         error;

  assert(fileName != NULL);
  assert(fileInfo != NULL);

  // set meta data
  switch (fileInfo->type)
  {
    case FILE_TYPE_FILE:
    case FILE_TYPE_DIRECTORY:
    case FILE_TYPE_HARDLINK:
    case FILE_TYPE_SPECIAL:
      utimeBuffer.actime  = fileInfo->timeLastAccess;
      utimeBuffer.modtime = fileInfo->timeModified;
      if (utime(String_cString(fileName),&utimeBuffer) != 0)
      {
        return ERRORX_(IO_ERROR,errno,String_cString(fileName));
      }
      #ifdef HAVE_CHOWN
        if (chown(String_cString(fileName),fileInfo->userId,fileInfo->groupId) != 0)
        {
          return ERRORX_(IO_ERROR,errno,String_cString(fileName));
        }
      #endif /* HAVE_CHOWN */
      #ifdef HAVE_CHMOD
        if (chmod(String_cString(fileName),(mode_t)fileInfo->permission) != 0)
        {
          return ERRORX_(IO_ERROR,errno,String_cString(fileName));
        }
      #endif /* HAVE_CHMOD */
      break;
    case FILE_TYPE_LINK:
      #ifdef HAVE_LCHMOD
        if (lchown(String_cString(fileName),fileInfo->userId,fileInfo->groupId) != 0)
        {
          return ERRORX_(IO_ERROR,errno,String_cString(fileName));
        }
      #endif /* HAVE_LCHMOD */
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

void File_initExtendedAttributes(FileExtendedAttributeList *fileExtendedAttributeList)
{
  assert(fileExtendedAttributeList != NULL);

  List_init(fileExtendedAttributeList);
}

void File_doneExtendedAttributes(FileExtendedAttributeList *fileExtendedAttributeList)
{
  assert(fileExtendedAttributeList != NULL);

  List_done(fileExtendedAttributeList,(ListNodeFreeFunction)freeExtendedAttributeNode,NULL);
}

void File_addExtendedAttribute(FileExtendedAttributeList *fileExtendedAttributeList,
                               const String              name,
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
                                  const String              fileName
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
    return ERRORX_(IO_ERROR,errno,String_cString(fileName));
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
    return ERRORX_(IO_ERROR,errno,String_cString(fileName));
  }

  // get attributes
  name = names;
  while ((name-names) < namesLength)
  {
    // allocate buffer for data
    n = lgetxattr(String_cString(fileName),name,NULL,0);
    if (n < 0)
    {
      free(names);
      return ERRORX_(IO_ERROR,errno,String_cString(fileName));
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
      return ERRORX_(IO_ERROR,errno,String_cString(fileName));
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

Errors File_setExtendedAttributes(const String                    fileName,
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
      return ERRORX_(IO_ERROR,errno,String_cString(fileName));
    }
  }

  return ERROR_NONE;
}

uint64 File_getFileTimeModified(const String fileName)
{
  FileStat fileStat;

  assert(fileName != NULL);

  if (LSTAT(String_cString(fileName),&fileStat) != 0)
  {
    return 0LL;
  }

  return (uint64)fileStat.st_mtime;
}

Errors File_setPermission(const String   fileName,
                          FilePermission permission
                         )
{
  assert(fileName != NULL);

  if (chmod(String_cString(fileName),(mode_t)permission) != 0)
  {
    return ERRORX_(IO_ERROR,errno,String_cString(fileName));
  }

  return ERROR_NONE;
}

Errors File_setOwner(const String fileName,
                     uint32       userId,
                     uint32       groupId
                    )
{
  assert(fileName != NULL);

  #ifdef HAVE_CHOWN
    if (chown(String_cString(fileName),
              (userId  != FILE_DEFAULT_USER_ID ) ? (uid_t)userId  : (uid_t)(-1),
              (groupId != FILE_DEFAULT_GROUP_ID) ? (gid_t)groupId : (gid_t)(-1)
             ) != 0
       )
    {
      return ERRORX_(IO_ERROR,errno,String_cString(fileName));
    }

    return ERROR_NONE;
  #else /* not HAVE_CHOWN */
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(userId);
    UNUSED_VARIABLE(groupId);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CHOWN */
}

Errors File_makeDirectory(const String   pathName,
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
  FileStat        fileStat;
  #ifdef HAVE_CHOWN
    uid_t           uid;
    gid_t           gid;
  #endif /* HAVE_CHOWN */
  Errors          error;
  String          name;

  assert(pathName != NULL);

  // initialize variables
  directoryName = File_newFileName();
  parentDirectoryName = File_newFileName();

  // get current umask (get and restore current value)
  currentCreationMask = umask(0);
  umask(currentCreationMask);

  // create directory including parent directories
  File_initSplitFileName(&pathNameTokenizer,pathName);
  if (File_getNextSplitFileName(&pathNameTokenizer,&name))
  {
    if (!String_isEmpty(name))
    {
      File_setFileName(directoryName,name);
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
        error = ERRORX_(IO_ERROR,errno,String_cString(directoryName));
        File_doneSplitFileName(&pathNameTokenizer);
        File_deleteFileName(parentDirectoryName);
        File_deleteFileName(directoryName);
        return error;
      }
    #elif (MKDIR_ARGUMENTS_COUNT == 2)
      if (mkdir(String_cString(directoryName),0777 & ~currentCreationMask) != 0)
      {
        error = ERRORX_(IO_ERROR,errno,String_cString(directoryName));
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
        uid = (userId  != FILE_DEFAULT_USER_ID ) ? (uid_t)userId  : (uid_t)(-1);
        gid = (groupId != FILE_DEFAULT_GROUP_ID) ? (gid_t)groupId : (gid_t)(-1);
        if (chown(String_cString(directoryName),uid,gid) != 0)
        {
          error = ERRORX_(IO_ERROR,errno,String_cString(directoryName));
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
    error = ERRORX_(NOT_A_DIRECTORY,0,String_cString(directoryName));
    File_doneSplitFileName(&pathNameTokenizer);
    File_deleteFileName(parentDirectoryName);
    File_deleteFileName(directoryName);
    return error;
  }

  while (File_getNextSplitFileName(&pathNameTokenizer,&name))
  {
    if (!String_isEmpty(name))
    {
      // get new parent directory
      File_setFileName(parentDirectoryName,directoryName);

      // get sub-directory
      File_appendFileName(directoryName,name);

      if      (!File_exists(directoryName))
      {
        // set read/write/execute-access in parent directory
        if (LSTAT(String_cString(parentDirectoryName),&fileStat) != 0)
        {
          error = ERRORX_(IO_ERROR,errno,String_cString(parentDirectoryName));
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
          error = ERRORX_(IO_ERROR,errno,String_cString(parentDirectoryName));
          File_doneSplitFileName(&pathNameTokenizer);
          File_deleteFileName(parentDirectoryName);
          File_deleteFileName(directoryName);
          return error;
        }

        // create sub-directory
        #if   (MKDIR_ARGUMENTS_COUNT == 1)
          if (mkdir(String_cString(directoryName)) != 0)
          {
            error = ERRORX_(IO_ERROR,errno,String_cString(directoryName));
            File_doneSplitFileName(&pathNameTokenizer);
            File_deleteFileName(parentDirectoryName);
            File_deleteFileName(directoryName);
            return error;
          }
        #elif (MKDIR_ARGUMENTS_COUNT == 2)
          if (mkdir(String_cString(directoryName),0777 & ~currentCreationMask) != 0)
          {
            error = ERRORX_(IO_ERROR,errno,String_cString(directoryName));
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
            uid = (userId  != FILE_DEFAULT_USER_ID ) ? (uid_t)userId  : (uid_t)(-1);
            gid = (groupId != FILE_DEFAULT_GROUP_ID) ? (gid_t)groupId : (gid_t)(-1);
            if (chown(String_cString(directoryName),uid,gid) != 0)
            {
              error = ERRORX_(IO_ERROR,errno,String_cString(directoryName));
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
              error = ERRORX_(IO_ERROR,errno,String_cString(directoryName));
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
        error = ERRORX_(NOT_A_DIRECTORY,0,String_cString(directoryName));
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

Errors File_readLink(String       fileName,
                     const String linkName
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
      error = ERRORX_(IO_ERROR,errno,String_cString(linkName));
      free(buffer);
      return error;
    }
  #else /* not HAVE_READLINK */
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(linkName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_READLINK */
}

Errors File_makeLink(const String linkName,
                     const String fileName
                    )
{
  assert(linkName != NULL);
  assert(fileName != NULL);

  #ifdef HAVE_SYMLINK
    unlink(String_cString(linkName));
    if (symlink(String_cString(fileName),String_cString(linkName)) != 0)
    {
      return ERRORX_(IO_ERROR,errno,String_cString(linkName));
    }

    return ERROR_NONE;
  #else /* not HAVE_SYMLINK */
    UNUSED_VARIABLE(linkName);
    UNUSED_VARIABLE(fileName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SYMLINK */
}

Errors File_makeHardLink(const String linkName,
                         const String fileName
                        )
{
  assert(linkName != NULL);
  assert(fileName != NULL);

  #ifdef HAVE_LINK
    unlink(String_cString(linkName));
    if (link(String_cString(fileName),String_cString(linkName)) != 0)
    {
      return ERRORX_(IO_ERROR,errno,String_cString(linkName));
    }

    return ERROR_NONE;
  #else /* not HAVE_LINK */
    UNUSED_VARIABLE(linkName);
    UNUSED_VARIABLE(fileName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_LINK */
}

Errors File_makeSpecial(const String     name,
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
          return ERRORX_(IO_ERROR,errno,String_cString(name));
        }
        break;
      case FILE_SPECIAL_TYPE_BLOCK_DEVICE:
        if (mknod(String_cString(name),S_IFBLK|0600,makedev(major,minor)) != 0)
        {
          return ERRORX_(IO_ERROR,errno,String_cString(name));
        }
        break;
      case FILE_SPECIAL_TYPE_FIFO:
        if (mknod(String_cString(name),S_IFIFO|0666,0) != 0)
        {
          return ERRORX_(IO_ERROR,errno,String_cString(name));
        }
        break;
      case FILE_SPECIAL_TYPE_SOCKET:
        if (mknod(String_cString(name),S_IFSOCK|0600,0) != 0)
        {
          return ERRORX_(IO_ERROR,errno,String_cString(name));
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
                              const String   pathName
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
      return ERRORX_(IO_ERROR,errno,String_cString(pathName));
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

void File_debugDumpInfo(FILE *handle)
{
  DebugFileNode *debugFileNode;

  pthread_once(&debugFileInitFlag,debugFileInit);

  pthread_mutex_lock(&debugFileLock);
  {
    LIST_ITERATE(&debugOpenFileList,debugFileNode)
    {
      assert(debugFileNode->fileHandle != NULL);
      assert(debugFileNode->fileHandle->name != NULL);

      fprintf(handle,"DEBUG: file '%s' opened at %s, line %lu\n",
              String_cString(debugFileNode->fileHandle->name),
              debugFileNode->fileName,
              debugFileNode->lineNb
             );
    }
  }
  pthread_mutex_unlock(&debugFileLock);
}

void File_debugPrintInfo()
{
  File_debugDumpInfo(stderr);
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

  File_debugPrintInfo();
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
