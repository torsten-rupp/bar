/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/files.c,v $
* $Revision: 1.9 $
* $Author: torsten $
* Contents: Backup ARchiver file functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <sys/statvfs.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "stringlists.h"
#include "devices.h"

#include "files.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

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

  if (String_length(fileName) > 0)
  {
    if (String_index(fileName,String_length(fileName)-1) != FILES_PATHNAME_SEPARATOR_CHAR)
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

  if (String_length(fileName) > 0)
  {
    if (String_index(fileName,String_length(fileName)-1) != FILES_PATHNAME_SEPARATOR_CHAR)
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

  if (String_length(fileName) > 0)
  {
    if (String_index(fileName,String_length(fileName)-1) != FILES_PATHNAME_SEPARATOR_CHAR)
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

  if (String_length(fileName) > 0)
  {
    if (String_index(fileName,String_length(fileName)-1) != FILES_PATHNAME_SEPARATOR_CHAR)
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

  /* find last path separator */
  lastPathSeparator = strrchr(fileName,FILES_PATHNAME_SEPARATOR_CHAR);

  /* get path */
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

  /* find last path separator */
  lastPathSeparator = strrchr(fileName,FILES_PATHNAME_SEPARATOR_CHAR);

  /* get path */
  if (lastPathSeparator != NULL)
  {
    String_setCString(baseName,lastPathSeparator+1);
  }
  else
  {
    String_clear(baseName);
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

  if (directory != NULL)
  {
    String_set(fileName,directory);
    File_appendFileNameCString(fileName,pattern);
  }
  else
  {
    String_setCString(fileName,pattern);
  }
  s = String_toCString(fileName);
  if (s == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  handle = mkstemp(s);
  if (handle == -1)
  {
    error = ERRORX(IO_ERROR,errno,s); 
    free(s);
    return error;
  }
  close(handle);

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
  Errors error;

  assert(directoryName != NULL);

  if (pattern == NULL) pattern = "tmp-XXXXXX";

  if (directory != NULL)
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

  if (mkdtemp(s) == NULL)
  {
    error = ERRORX(IO_ERROR,errno,s);
    free(s);
    return error;
  }

  String_setBuffer(directoryName,s,strlen(s));

  free(s);

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors File_open(FileHandle    *fileHandle,
                 const String  fileName,
                 FileOpenModes fileOpenMode
                )
{
  return File_openCString(fileHandle,
                          String_cString(fileName),
                          fileOpenMode
                         );
}

Errors File_openCString(FileHandle    *fileHandle,
                        const char    *fileName,
                        FileOpenModes fileOpenMode
                       )
{
  off_t  n;
  Errors error;
  String pathName;

  assert(fileHandle != NULL);
  assert(fileName != NULL);

  switch (fileOpenMode)
  {
    case FILE_OPENMODE_CREATE:
      /* create file */
      fileHandle->file = fopen(fileName,"wb");
      if (fileHandle->file == NULL)
      {
        return ERRORX(CREATE_FILE,errno,fileName);
      }

      fileHandle->name  = String_newCString(fileName);
      fileHandle->index = 0LL;
      fileHandle->size  = 0LL;
      break;
    case FILE_OPENMODE_READ:
      /* open file for reading */
      fileHandle->file = fopen(fileName,"rb");
      if (fileHandle->file == NULL)
      {
        return ERRORX(OPEN_FILE,errno,fileName);
      }

      /* get file size */
      if (fseeko(fileHandle->file,(off_t)0,SEEK_END) == -1)
      {
        error = ERRORX(IO_ERROR,errno,fileName);
        fclose(fileHandle->file);
        return error;
      }
      n = ftello(fileHandle->file);
      if (n == (off_t)(-1))
      {
        error = ERRORX(IO_ERROR,errno,fileName);
        fclose(fileHandle->file);
        return error;
      }
      if (fseeko(fileHandle->file,(off_t)0,SEEK_SET) == -1)
      {
        error = ERRORX(IO_ERROR,errno,fileName);
        fclose(fileHandle->file);
        return error;
      }

      fileHandle->name  = String_newCString(fileName);
      fileHandle->index = 0LL;
      fileHandle->size  = (uint64)n;
      break;
    case FILE_OPENMODE_WRITE:
      /* create directory if needed */
      pathName = File_getFilePathNameCString(File_newFileName(),fileName);
      if (!File_exists(pathName))
      {
        error = File_makeDirectory(pathName,
                                   FILE_DEFAULT_USER_ID,
                                   FILE_DEFAULT_GROUP_ID,
                                   FILE_DEFAULT_PERMISSION
                                  );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      File_deleteFileName(pathName);

      /* open existing file for writing */
      fileHandle->file = fopen(fileName,"r+b");
      if (fileHandle->file == NULL)
      {
        if (errno == ENOENT)
        {
          fileHandle->file = fopen(fileName,"wb");
          if (fileHandle->file == NULL)
          {
            return ERRORX(OPEN_FILE,errno,fileName);
          }
        }
        else
        {
          return ERRORX(OPEN_FILE,errno,fileName);
        }
      }

      fileHandle->name  = String_newCString(fileName);
      fileHandle->index = 0LL;
      fileHandle->size  = 0LL;
      break;
    case FILE_OPENMODE_APPEND:
      /* create directory if needed */
      pathName = File_getFilePathNameCString(File_newFileName(),fileName);
      if (!File_exists(pathName))
      {
        error = File_makeDirectory(pathName,
                                   FILE_DEFAULT_USER_ID,
                                   FILE_DEFAULT_GROUP_ID,
                                   FILE_DEFAULT_PERMISSION
                                  );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      File_deleteFileName(pathName);

      /* open existing file for writing */
      fileHandle->file = fopen(fileName,"ab");
      if (fileHandle->file == NULL)
      {
        return ERRORX(OPEN_FILE,errno,fileName);
      }

      /* get file size */
      n = ftello(fileHandle->file);
      if (n == (off_t)(-1))
      {
        error = ERRORX(IO_ERROR,errno,fileName);
        fclose(fileHandle->file);
        return error;
      }

      fileHandle->name  = String_newCString(fileName);
      fileHandle->index = (uint64)n;
      fileHandle->size  = (uint64)n;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

Errors File_openDescriptor(FileHandle    *fileHandle,
                           int           fileDescriptor,
                           FileOpenModes fileOpenMode
                          )
{
  off_t  n;
  Errors error;

  assert(fileHandle != NULL);
  assert(fileDescriptor >= 0);

  switch (fileOpenMode)
  {
    case FILE_OPENMODE_CREATE:
      /* create file */
      fileHandle->file = fdopen(fileDescriptor,"wb");
      if (fileHandle->file == NULL)
      {
        return ERROR(CREATE_FILE,errno);
      }

      fileHandle->name  = NULL;
      fileHandle->index = 0LL;
      fileHandle->size  = 0LL;
      break;
    case FILE_OPENMODE_READ:
      /* open file for reading */
      fileHandle->file = fdopen(fileDescriptor,"rb");
      if (fileHandle->file == NULL)
      {
        return ERROR(OPEN_FILE,errno);
      }

      fileHandle->name  = NULL;
      fileHandle->index = 0LL;
      fileHandle->size  = 0LL;
      break;
    case FILE_OPENMODE_WRITE:
      /* open file for writing */
      fileHandle->file = fdopen(fileDescriptor,"r+b");
      if (fileHandle->file == NULL)
      {
        return ERROR(OPEN_FILE,errno);
      }

      fileHandle->name  = NULL;
      fileHandle->index = 0LL;
      fileHandle->size  = 0LL;
      break;
    case FILE_OPENMODE_APPEND:
      /* open file for writing */
      fileHandle->file = fdopen(fileDescriptor,"ab");
      if (fileHandle->file == NULL)
      {
        return ERROR(OPEN_FILE,errno);
      }

      /* get file size */
      n = ftello(fileHandle->file);
      if (n == (off_t)(-1))
      {
        error = ERROR(IO_ERROR,errno);
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

  return ERROR_NONE;
}

Errors File_close(FileHandle *fileHandle)
{
  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
  assert(fileHandle->name != NULL);
 
  fclose(fileHandle->file);
  fileHandle->file = NULL;
  String_delete(fileHandle->name);

  return ERROR_NONE;
}

bool File_eof(FileHandle *fileHandle)
{
  int  ch;
  bool eofFlag;

  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);

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

  n = fread(buffer,1,bufferLength,fileHandle->file);
  if (   ((n <= 0) && ferror(fileHandle->file))
      || ((n < bufferLength) && (bytesRead == NULL))
     )
  {
    return ERRORX(IO_ERROR,errno,String_cString(fileHandle->name));
  }
  fileHandle->index += n;

  if (bytesRead != NULL) (*bytesRead) = n;

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

  n = fwrite(buffer,1,bufferLength,fileHandle->file);
  if (n > 0) fileHandle->index += n;
  if (fileHandle->index > fileHandle->size) fileHandle->size = fileHandle->index;
  if (n != bufferLength)
  {
    return ERRORX(IO_ERROR,errno,String_cString(fileHandle->name));
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

  String_clear(line);
  do
  {
    ch = getc(fileHandle->file);
    if (ch >= 0) fileHandle->index += 1;
    if (ch < 0)
    {
      return ERRORX(IO_ERROR,errno,String_cString(fileHandle->name));
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

  /* initialise variables */
  line = String_new();

  /* format line */
  va_start(arguments,format);
  String_vformat(line,format,arguments);
  va_end(arguments);

  /* write line */
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

  /* free resources */
  String_delete(line);

  return ERROR_NONE;
}

uint64 File_getSize(FileHandle *fileHandle)
{
  assert(fileHandle != NULL);
//??? geth niht
  assert(fileHandle->file != NULL);

  return fileHandle->size;
}

Errors File_tell(FileHandle *fileHandle, uint64 *offset)
{
  off_t n;

  assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);
  assert(offset != NULL);

  n = ftello(fileHandle->file);
  if (n == (off_t)(-1))
  {
    return ERRORX(IO_ERROR,errno,String_cString(fileHandle->name));
  }
// NYI
//assert(sizeof(off_t)==8);
assert(n == (off_t)fileHandle->index);

  (*offset) = fileHandle->index;

  return ERROR_NONE;
}

Errors File_seek(FileHandle *fileHandle,
                 uint64     offset
                )
{
   assert(fileHandle != NULL);
  assert(fileHandle->file != NULL);

  if (fseeko(fileHandle->file,(off_t)offset,SEEK_SET) == -1)
  {
    return ERRORX(IO_ERROR,errno,String_cString(fileHandle->name));
  }
  fileHandle->index = offset;

  return ERROR_NONE;
}

Errors File_truncate(FileHandle *fileHandle,
                     uint64     size
                    )
{
  if (size < fileHandle->size)
  {
    if (ftruncate(fileno(fileHandle->file),(off_t)size) != 0)
    {
      return ERRORX(IO_ERROR,errno,String_cString(fileHandle->name));
    }
    fileHandle->size = size;
  }

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
    return ERRORX(OPEN_DIRECTORY,errno,String_cString(pathName));
  }

  directoryListHandle->name  = String_duplicate(pathName);
  directoryListHandle->entry = NULL;

  return ERROR_NONE;
}

Errors File_openDirectoryListCString(DirectoryListHandle *directoryListHandle,
                                     const char      *pathName
                                    )
{
  assert(directoryListHandle != NULL);
  assert(pathName != NULL);

  directoryListHandle->dir = opendir(pathName);
  if (directoryListHandle->dir == NULL)
  {
    return ERRORX(OPEN_DIRECTORY,errno,pathName);
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

  /* read entry iff not read */
  if (directoryListHandle->entry == NULL)
  {
    directoryListHandle->entry = readdir(directoryListHandle->dir);
  }

  /* skip "." and ".." entries */
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

  /* read entry iff not read */
  if (directoryListHandle->entry == NULL)
  {
    directoryListHandle->entry = readdir(directoryListHandle->dir);
  }

  /* skip "." and ".." entries */
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
    return ERRORX(IO_ERROR,errno,String_cString(directoryListHandle->name));
  }

  /* get entry name */
  String_set(fileName,directoryListHandle->name);
  File_appendFileNameCString(fileName,directoryListHandle->entry->d_name);

  /* mark entry read */
  directoryListHandle->entry = NULL;

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

uint32 File_userNameToUserId(const char *name)
{
  struct passwd *passwordEntry;

  passwordEntry = getpwnam(name);
  return (passwordEntry != NULL)?passwordEntry->pw_uid:FILE_DEFAULT_USER_ID;
}

uint32 File_groupNameToGroupId(const char *name)
{
  struct group *groupEntry;

  groupEntry = getgrnam(name);
  return (groupEntry != NULL)?groupEntry->gr_gid:FILE_DEFAULT_GROUP_ID;
}

FileTypes File_getType(const String fileName)
{
  struct stat64 fileStat;

  assert(fileName != NULL);

  if (lstat64(String_cString(fileName),&fileStat) == 0)
  {
    if      (S_ISREG(fileStat.st_mode))  return FILE_TYPE_FILE;
    else if (S_ISDIR(fileStat.st_mode))  return FILE_TYPE_DIRECTORY;
    else if (S_ISLNK(fileStat.st_mode))  return FILE_TYPE_LINK;
    else if (S_ISCHR(fileStat.st_mode))  return FILE_TYPE_SPECIAL;
    else if (S_ISBLK(fileStat.st_mode))  return FILE_TYPE_SPECIAL;
    else if (S_ISFIFO(fileStat.st_mode)) return FILE_TYPE_SPECIAL;
    else if (S_ISSOCK(fileStat.st_mode)) return FILE_TYPE_SPECIAL;
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

  /* open file */
  error = File_openCString(&fileHandle,fileName,FILE_OPENMODE_READ);
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* get file size */
  (*size) = (ulong)File_getSize(&fileHandle);

  /* allocate memory for data */
  (*data) = malloc(*size);
  if ((*data) == NULL)
  {
    File_close(&fileHandle);
    return ERROR_INSUFFICIENT_MEMORY;
  }

  /* read data */
  error = File_read(&fileHandle,*data,*size,&bytesRead);
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    return error;
  }
  if (bytesRead != (*size))
  {
    File_close(&fileHandle);
    return ERROR(IO_ERROR,errno);
  }

  /* close file */
  File_close(&fileHandle);

  return ERROR_NONE;
}

Errors File_delete(const String fileName, bool recursiveFlag)
{
  struct stat64 fileStat;

  assert(fileName != NULL);

  if (lstat64(String_cString(fileName),&fileStat) != 0)
  {
    return ERRORX(IO_ERROR,errno,String_cString(fileName));
  }

  if      (   S_ISREG(fileStat.st_mode)
           || S_ISLNK(fileStat.st_mode)
          )
  {
    if (unlink(String_cString(fileName)) != 0)
    {
      return ERRORX(IO_ERROR,errno,String_cString(fileName));
    }
  }
  else if (S_ISDIR(fileStat.st_mode))
  {
    Errors        error;
    StringList    directoryList;
    DIR           *dir;
    struct dirent *entry;
    String        directoryName;
    bool          emptyFlag;
    String        name;

    error = ERROR_NONE;
    if (recursiveFlag)
    {
      /* delete entries in directory */
      StringList_init(&directoryList);
      StringList_append(&directoryList,fileName);
      directoryName = File_newFileName();
      name = File_newFileName();
      while (!StringList_empty(&directoryList) && (error == ERROR_NONE))
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

              if (lstat64(String_cString(name),&fileStat) == 0)
              {
                if      (   S_ISREG(fileStat.st_mode)
                         || S_ISLNK(fileStat.st_mode)
                        )
                {
                  if (unlink(String_cString(name)) != 0)
                  {
                    error = ERRORX(IO_ERROR,errno,String_cString(name));
                  }
//HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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
              error = ERRORX(IO_ERROR,errno,String_cString(directoryName));
            }
//HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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
        error = ERRORX(IO_ERROR,errno,String_cString(fileName));
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

  /* try rename */
  if (rename(String_cString(oldFileName),String_cString(newFileName)) != 0)
  {
    /* copy to new file */
    error = File_copy(oldFileName,newFileName);
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* delete old file */
    if (unlink(String_cString(oldFileName)) != 0)
    {
      return ERRORX(IO_ERROR,errno,String_cString(oldFileName));
    }
  }

  return ERROR_NONE;
}

Errors File_copy(const String sourceFileName,
                 const String destinationFileName
                )
{
  #define BUFFER_SIZE (1024*1024)

  byte          *buffer;
  Errors        error;
  FILE          *sourceFile,*destinationFile;
  size_t        n;
  struct stat64 fileStat;

  assert(sourceFileName != NULL);
  assert(destinationFileName != NULL);

  /* allocate buffer */
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }

  /* open files */
  sourceFile = fopen(String_cString(sourceFileName),"r");
  if (sourceFile == NULL)
  {
    error = ERRORX(OPEN_FILE,errno,String_cString(sourceFileName));
    free(buffer);
    return error;
  }
  destinationFile = fopen(String_cString(destinationFileName),"w");
  if (destinationFile == NULL)
  {
    error = ERRORX(OPEN_FILE,errno,String_cString(destinationFileName));
    fclose(sourceFile);
    free(buffer);
    return error;
  }

  /* copy data */
  do
  {
    n = fread(buffer,1,BUFFER_SIZE,sourceFile);
    if (n > 0)
    {
      if (fwrite(buffer,1,n,destinationFile) != n)
      {
        error = ERRORX(IO_ERROR,errno,String_cString(destinationFileName));
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
        error = ERRORX(IO_ERROR,errno,String_cString(sourceFileName));
        fclose(destinationFile);
        fclose(sourceFile);
        free(buffer);
        return error;
      }
    }
  }
  while (n > 0);

  /* close files */
  fclose(destinationFile);
  fclose(sourceFile);

  /* free resources */
  free(buffer);

  /* copy permissions */
  if (lstat64(String_cString(sourceFileName),&fileStat) != 0)
  {
    return ERRORX(IO_ERROR,errno,String_cString(sourceFileName));
  }
  if (chown(String_cString(destinationFileName),fileStat.st_uid,fileStat.st_gid) != 0)
  {
    return ERRORX(IO_ERROR,errno,String_cString(destinationFileName));
  }
  if (chmod(String_cString(destinationFileName),fileStat.st_mode) != 0)
  {
    return ERRORX(IO_ERROR,errno,String_cString(destinationFileName));
  }

  return ERROR_NONE;

  #undef BUFFER_SIZE
}

bool File_exists(const String fileName)
{
  struct stat fileStat;

  assert(fileName != NULL);

  return (lstat(String_cString(fileName),&fileStat) == 0);
}

bool File_existsCString(const char *fileName)
{
  struct stat fileStat;

  assert(fileName != NULL);

  return (lstat(fileName,&fileStat) == 0);
}

bool File_isFile(const String fileName)
{
  struct stat fileStat;

  assert(fileName != NULL);

  return (   (stat(String_cString(fileName),&fileStat) == 0)
          && S_ISREG(fileStat.st_mode)
         );
}

bool File_isFileCString(const char *fileName)
{
  struct stat fileStat;

  assert(fileName != NULL);

  return (   (stat(fileName,&fileStat) == 0)
          && S_ISREG(fileStat.st_mode)
         );
}

bool File_isDirectory(const String fileName)
{
  struct stat fileStat;

  assert(fileName != NULL);

  return (   (stat(String_cString(fileName),&fileStat) == 0)
          && S_ISDIR(fileStat.st_mode)
         );
}

bool File_isDirectoryCString(const char *fileName)
{
  struct stat fileStat;

  assert(fileName != NULL);

  return (   (stat(fileName,&fileStat) == 0)
          && S_ISDIR(fileStat.st_mode)
         );
}

bool File_isReadable(const String fileName)
{
  return File_isReadableCString(String_cString(fileName));
}

bool File_isReadableCString(const char *fileName)
{
  assert(fileName != NULL);

  return access(fileName,F_OK|R_OK) == 0;
}

bool File_isWriteable(const String fileName)
{
  return File_isWriteableCString(String_cString(fileName));
}

bool File_isWriteableCString(const char *fileName)
{
  assert(fileName != NULL);

  return access(fileName,F_OK|W_OK) == 0;
}

Errors File_getFileInfo(const String fileName,
                        FileInfo     *fileInfo
                       )
{
  struct stat64 fileStat;
  DeviceInfo    deviceInfo;
  struct
  {
    time_t d0;
    time_t d1;
  } cast;

  assert(fileName != NULL);
  assert(fileInfo != NULL);

  if (lstat64(String_cString(fileName),&fileStat) != 0)
  {
    return ERRORX(IO_ERROR,errno,String_cString(fileName));
  }

  if      (S_ISREG(fileStat.st_mode))
  {
    fileInfo->type = FILE_TYPE_FILE;
    fileInfo->size = fileStat.st_size;
  }
  else if (S_ISDIR(fileStat.st_mode))
  {
    fileInfo->type = FILE_TYPE_DIRECTORY;
    fileInfo->size = 0LL;
  }
  else if (S_ISLNK(fileStat.st_mode))
  {
    fileInfo->type = FILE_TYPE_LINK;
    fileInfo->size = 0LL;
  }
  else if (S_ISCHR(fileStat.st_mode))
  {
    fileInfo->type        = FILE_TYPE_SPECIAL;
    fileInfo->size        = 0LL;
    fileInfo->specialType = FILE_SPECIAL_TYPE_CHARACTER_DEVICE;
  }
  else if (S_ISBLK(fileStat.st_mode))
  {
    fileInfo->type        = FILE_TYPE_SPECIAL;
    fileInfo->size        = -1LL;
    fileInfo->specialType = FILE_SPECIAL_TYPE_BLOCK_DEVICE;

    /* try to detect block device size */
    if (Device_getDeviceInfo(fileName,&deviceInfo) == ERROR_NONE)
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
  else if (S_ISSOCK(fileStat.st_mode))
  {
    fileInfo->type        = FILE_TYPE_SPECIAL;
    fileInfo->size        = 0LL;
    fileInfo->specialType = FILE_SPECIAL_TYPE_SOCKET;
  }
  else
  {
    fileInfo->type        = FILE_TYPE_UNKNOWN;
    fileInfo->size        = 0LL;
  }
  fileInfo->timeLastAccess  = fileStat.st_atime;
  fileInfo->timeModified    = fileStat.st_mtime;
  fileInfo->timeLastChanged = fileStat.st_ctime;
  fileInfo->userId          = fileStat.st_uid;
  fileInfo->groupId         = fileStat.st_gid;
  fileInfo->permission      = fileStat.st_mode;
  fileInfo->major           = major(fileStat.st_rdev);
  fileInfo->minor           = minor(fileStat.st_rdev);
  cast.d0 = fileStat.st_mtime;
  cast.d1 = fileStat.st_ctime;
  memcpy(fileInfo->cast,&cast,sizeof(FileCast));

  return ERROR_NONE;
}

uint64 File_getFileTimeModified(const String fileName)
{
  struct stat64 fileStat;

  assert(fileName != NULL);

  if (lstat64(String_cString(fileName),&fileStat) != 0)
  {
    return 0LL;
  }

  return (uint64)fileStat.st_mtime;
}

Errors File_setPermission(const String fileName,
                          uint32       permission
                         )
{
  assert(fileName != NULL);

  if (chmod(String_cString(fileName),permission) != 0)
  {
    return ERRORX(IO_ERROR,errno,String_cString(fileName));
  }

  return ERROR_NONE;
}

Errors File_setOwner(const String fileName,
                     uint32       userId,
                     uint32       groupId
                    )
{
  assert(fileName != NULL);

  if (chown(String_cString(fileName),
            (userId  != FILE_DEFAULT_USER_ID ) ? userId  : -1,
            (groupId != FILE_DEFAULT_GROUP_ID) ? groupId : -1
           ) != 0
     )
  {
    return ERRORX(IO_ERROR,errno,String_cString(fileName));
  }

  return ERROR_NONE;
}

Errors File_setFileInfo(const String fileName,
                        FileInfo     *fileInfo
                       )
{
  struct utimbuf utimeBuffer;

  assert(fileName != NULL);
  assert(fileInfo != NULL);

  switch (fileInfo->type)
  {
    case FILE_TYPE_FILE:
    case FILE_TYPE_DIRECTORY:
    case FILE_TYPE_SPECIAL:
      utimeBuffer.actime  = fileInfo->timeLastAccess;
      utimeBuffer.modtime = fileInfo->timeModified;
      if (utime(String_cString(fileName),&utimeBuffer) != 0)
      {
        return ERRORX(IO_ERROR,errno,String_cString(fileName));
      }
      if (chown(String_cString(fileName),fileInfo->userId,fileInfo->groupId) != 0)
      {
fprintf(stderr,"%s,%d:  %d %d\n",__FILE__,__LINE__,fileInfo->userId,fileInfo->groupId);
        return ERRORX(IO_ERROR,errno,String_cString(fileName));
      }
      if (chmod(String_cString(fileName),fileInfo->permission) != 0)
      {
        return ERRORX(IO_ERROR,errno,String_cString(fileName));
      }
      break;
    case FILE_TYPE_LINK:
      if (lchown(String_cString(fileName),fileInfo->userId,fileInfo->groupId) != 0)
      {
        return ERRORX(IO_ERROR,errno,String_cString(fileName));
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return ERROR_NONE;
}

Errors File_makeDirectory(const String pathName,
                          uint32       userId,
                          uint32       groupId,
                          uint32       permission
                         )
{
  mode_t          currentCreationMask;
  StringTokenizer pathNameTokenizer;
  String          directoryName;
  uid_t           uid;
  gid_t           gid;
  Errors          error;
  String          name;

  assert(pathName != NULL);

  /* get current umask */
  currentCreationMask = umask(0);
  umask(currentCreationMask);

  /* create directory including parent directories */
  directoryName = File_newFileName();
  File_initSplitFileName(&pathNameTokenizer,pathName);
  if (File_getNextSplitFileName(&pathNameTokenizer,&name))
  {
    if (String_length(name) > 0)
    {
      File_setFileName(directoryName,name);
    }
    else
    {
      File_setFileNameChar(directoryName,FILES_PATHNAME_SEPARATOR_CHAR);
    }
  }
  if (!File_exists(directoryName))
  {
    if (mkdir(String_cString(directoryName),0777 & ~currentCreationMask) != 0)
    {
      error = ERRORX(IO_ERROR,errno,String_cString(directoryName));
      File_doneSplitFileName(&pathNameTokenizer);
      File_deleteFileName(directoryName);
      return error;
    }
    if (   (userId  != FILE_DEFAULT_USER_ID)
        || (groupId != FILE_DEFAULT_GROUP_ID)
       )
    {
      uid = (userId  != FILE_DEFAULT_USER_ID ) ? (uid_t)userId  : -1;
      gid = (groupId != FILE_DEFAULT_GROUP_ID) ? (gid_t)groupId : -1;
      if (chown(String_cString(directoryName),uid,gid) != 0)
      {
        error = ERRORX(IO_ERROR,errno,String_cString(directoryName));
        File_doneSplitFileName(&pathNameTokenizer);
        File_deleteFileName(directoryName);
        return error;
      }
    }
    if (permission != FILE_DEFAULT_PERMISSION)
    {
      if (chmod(String_cString(directoryName),(permission|S_IXUSR|S_IXGRP|S_IXOTH) & ~currentCreationMask) != 0)
      {
        error = ERROR(IO_ERROR,errno);
        File_doneSplitFileName(&pathNameTokenizer);
        File_deleteFileName(directoryName);
        return error;
      }
    }
  }
  while (File_getNextSplitFileName(&pathNameTokenizer,&name))
  {
    if (String_length(name) > 0)
    {     
      File_appendFileName(directoryName,name);

      if (!File_exists(directoryName))
      {
        if (mkdir(String_cString(directoryName),0777 & ~currentCreationMask) != 0)
        {
          error = ERRORX(IO_ERROR,errno,String_cString(directoryName));
          File_doneSplitFileName(&pathNameTokenizer);
          File_deleteFileName(directoryName);
          return error;
        }
        if (   (userId  != FILE_DEFAULT_USER_ID)
            || (groupId != FILE_DEFAULT_GROUP_ID)
           )
        {
          uid = (userId  != FILE_DEFAULT_USER_ID ) ? (uid_t)userId  : -1;
          gid = (groupId != FILE_DEFAULT_GROUP_ID) ? (gid_t)groupId : -1;
          if (chown(String_cString(directoryName),uid,gid) != 0)
          {
            error = ERRORX(IO_ERROR,errno,String_cString(directoryName));
            File_doneSplitFileName(&pathNameTokenizer);
            File_deleteFileName(directoryName);
            return error;
          }
        }
        if (permission != FILE_DEFAULT_PERMISSION)
        {
          if (chmod(String_cString(directoryName),(permission|S_IXUSR|S_IXGRP|S_IXOTH) & ~currentCreationMask) != 0)
          {
            error = ERRORX(IO_ERROR,errno,String_cString(directoryName));
            File_doneSplitFileName(&pathNameTokenizer);
            File_deleteFileName(directoryName);
            return error;
          }
        }
      }
    }
  }
  File_doneSplitFileName(&pathNameTokenizer);
  File_deleteFileName(directoryName);

  return ERROR_NONE;
}

Errors File_readLink(const String linkName,
                     String       fileName
                    )
{
  #define BUFFER_SIZE  256
  #define BUFFER_DELTA 128

  char   *buffer;
  uint   bufferSize;
  int    result;
  Errors error;

  assert(linkName != NULL);
  assert(fileName != NULL);

  /* allocate initial buffer for name */
  buffer = (char*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }  
  bufferSize = BUFFER_SIZE;

  /* try to read link, increase buffer if needed */
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
    error = ERRORX(IO_ERROR,errno,String_cString(linkName));
    free(buffer);
    return error;
  }
}

Errors File_makeLink(const String linkName,
                     const String fileName
                    )
{
  assert(linkName != NULL);
  assert(fileName != NULL);

  unlink(String_cString(linkName));
  if (symlink(String_cString(fileName),String_cString(linkName)) != 0)
  {
    return ERRORX(IO_ERROR,errno,String_cString(linkName));
  }

  return ERROR_NONE;
}

Errors File_makeSpecial(const String     name,
                        FileSpecialTypes type,
                        ulong            major,
                        ulong            minor
                       )
{
  assert(name != NULL);

  unlink(String_cString(name));
  switch (type)
  {
    case FILE_SPECIAL_TYPE_CHARACTER_DEVICE:
      if (mknod(String_cString(name),S_IFCHR|0600,makedev(major,minor)) != 0)
      {
        return ERRORX(IO_ERROR,errno,String_cString(name));
      }
      break;
    case FILE_SPECIAL_TYPE_BLOCK_DEVICE:
      if (mknod(String_cString(name),S_IFBLK|0600,makedev(major,minor)) != 0)
      {
        return ERRORX(IO_ERROR,errno,String_cString(name));
      }
      break;
    case FILE_SPECIAL_TYPE_FIFO:
      if (mknod(String_cString(name),S_IFIFO|0666,0) != 0)
      {
        return ERRORX(IO_ERROR,errno,String_cString(name));
      }
      break;
    case FILE_SPECIAL_TYPE_SOCKET:
      if (mknod(String_cString(name),S_IFSOCK|0600,0) != 0)
      {
        return ERRORX(IO_ERROR,errno,String_cString(name));
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

Errors File_getFileSystemInfo(const          String pathName,
                              FileSystemInfo *fileSystemInfo
                             )
{
  struct statvfs fileSystemStat;

  assert(pathName != NULL);
  assert(fileSystemInfo != NULL);

  if (statvfs(String_cString(pathName),&fileSystemStat) != 0)
  {
    return ERRORX(IO_ERROR,errno,String_cString(pathName));
  }

  fileSystemInfo->blockSize         = fileSystemStat.f_bsize;
  fileSystemInfo->freeBytes         = (uint64)fileSystemStat.f_bavail*(uint64)fileSystemStat.f_bsize;
  fileSystemInfo->totalBytes        = (uint64)fileSystemStat.f_blocks*(uint64)fileSystemStat.f_frsize;
  fileSystemInfo->maxFileNameLength = (uint64)fileSystemStat.f_namemax;

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
