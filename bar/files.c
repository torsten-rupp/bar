/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/files.c,v $
* $Revision: 1.9 $
* $Author: torsten $
* Contents: Backup ARchiver file functions
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

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

String Files_setFileName(String fileName, String name)
{
  assert(fileName != NULL);

  return String_set(fileName,name);
}

String Files_setFileNameCString(String fileName, const char *name)
{
  assert(fileName != NULL);

  return String_setCString(fileName,name);
}

String Files_setFileNameChar(String fileName, char ch)
{
  assert(fileName != NULL);

  return String_setChar(fileName,ch);
}

String Files_appendFileName(String fileName, String name)
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

String Files_appendFileNameCString(String fileName, const char *name)
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

String Files_appendFileNameChar(String fileName, char ch)
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

String Files_getFilePathName(String fileName, String pathName)
{
  long lastPathSeparatorIndex;

  assert(fileName != NULL);
  assert(pathName != NULL);

  /* find last path separator */
  lastPathSeparatorIndex = String_findLastChar(fileName,STRING_END,FILES_PATHNAME_SEPARATOR_CHAR);

  /* get path */
  if (lastPathSeparatorIndex >= 0)
  {
    String_sub(pathName,fileName,0,lastPathSeparatorIndex);
  }
  else
  {
    String_clear(pathName);
  }

  return pathName;
}

String Files_getFileBaseName(String fileName, String baseName)
{
  long lastPathSeparatorIndex;

  assert(fileName != NULL);
  assert(baseName != NULL);

  /* find last path separator */
  lastPathSeparatorIndex = String_findLastChar(fileName,STRING_END,FILES_PATHNAME_SEPARATOR_CHAR);

  /* get path */
  if (lastPathSeparatorIndex >= 0)
  {
    String_sub(baseName,fileName,lastPathSeparatorIndex+1,STRING_END);
  }
  else
  {
    String_clear(baseName);
  }

  return baseName;
}

void Files_splitFileName(String fileName, String *pathName, String *baseName)
{
  assert(fileName != NULL);
  assert(pathName != NULL);
  assert(baseName != NULL);

  (*pathName) = Files_getFilePathName(fileName,String_new());
  (*baseName) = Files_getFileBaseName(fileName,String_new());
}

void Files_initSplitFileName(StringTokenizer *stringTokenizer, String fileName)
{
  assert(stringTokenizer != NULL);
  assert(fileName != NULL);

  String_initTokenizer(stringTokenizer,fileName,FILES_PATHNAME_SEPARATOR_CHARS,NULL,FALSE);
}

void Files_doneSplitFileName(StringTokenizer *stringTokenizer)
{
  assert(stringTokenizer != NULL);
  String_doneTokenizer(stringTokenizer);
}

bool Files_getNextSplitFileName(StringTokenizer *stringTokenizer, String *const name)
{
  assert(stringTokenizer != NULL);
  assert(name != NULL);

  return String_getNextToken(stringTokenizer,name,NULL);
}

/*---------------------------------------------------------------------*/

Errors Files_open(FileHandle    *fileHandle,
                  const String  fileName,
                  FileOpenModes fileOpenMode
                 )
{
  off64_t n;
  Errors  error;
  String  pathName;

  assert(fileHandle != NULL);
  assert(fileName != NULL);

  switch (fileOpenMode)
  {
    case FILE_OPENMODE_CREATE:
      /* create file */
      fileHandle->handle = open(String_cString(fileName),O_RDWR|O_CREAT|O_TRUNC|O_LARGEFILE,0600);
      if (fileHandle->handle == -1)
      {
        return ERROR_CREATE_FILE;
      }

      fileHandle->index = 0;
      fileHandle->size  = 0;
      break;
    case FILE_OPENMODE_READ:
      /* open file for reading */
      fileHandle->handle = open(String_cString(fileName),O_RDONLY|O_LARGEFILE,0600);
      if (fileHandle->handle == -1)
      {
        return ERROR_OPEN_FILE;
      }

      /* get file size */
      if (lseek64(fileHandle->handle,(off64_t)0,SEEK_END) == (off64_t)-1)
      {
        close(fileHandle->handle);
        return ERROR_IO_ERROR;
      }
      n = lseek64(fileHandle->handle,0,SEEK_CUR);
      if (n == (off64_t)-1)
      {
        close(fileHandle->handle);
        return ERROR_IO_ERROR;
      }
      fileHandle->size = (uint64)n;
      if (lseek64(fileHandle->handle,(off64_t)0,SEEK_SET) == (off64_t)-1)
      {
        close(fileHandle->handle);
        return ERROR_IO_ERROR;
      }

      fileHandle->index = 0;
      break;
    case FILE_OPENMODE_WRITE:
      /* create directory if needed */
      pathName = Files_getFilePathName(fileName,String_new());
      if (!Files_exist(pathName))
      {
        error = Files_makeDirectory(pathName);
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      String_delete(pathName);

      /* open file wrote writing */
      fileHandle->handle = open(String_cString(fileName),O_WRONLY|O_CREAT|O_LARGEFILE,0600);
      if (fileHandle->handle == -1)
      {
        return ERROR_OPEN_FILE;
      }

      fileHandle->index = 0;
      fileHandle->size  = 0;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

Errors Files_close(FileHandle *fileHandle)
{
  assert(fileHandle != NULL);
  assert(fileHandle->handle >= 0);

  close(fileHandle->handle);
  fileHandle->handle = -1;

  return ERROR_NONE;
}

bool Files_eof(FileHandle *fileHandle)
{
  off64_t n;

  assert(fileHandle != NULL);

  n = lseek64(fileHandle->handle,0,SEEK_CUR);
  if (n == (off64_t)-1)
  {
    return TRUE;
  }

  return (n >= fileHandle->size);
}

Errors Files_read(FileHandle *fileHandle,
                  void       *buffer,
                  ulong      bufferLength,
                  ulong      *readBytes
                 )
{
  ssize_t n;

  assert(fileHandle != NULL);
  assert(readBytes != NULL);

  n = read(fileHandle->handle,buffer,bufferLength);
  if (n > 0) fileHandle->index += n;
  if (n < 0)
  {
    return ERROR_IO_ERROR;
  }

  (*readBytes) = n;

  return ERROR_NONE;
}

Errors Files_write(FileHandle *fileHandle,
                   const void *buffer,
                   ulong      bufferLength
                  )
{
  ssize_t n;

  assert(fileHandle != NULL);

  n = write(fileHandle->handle,buffer,bufferLength);
  if (n > 0) fileHandle->index += n;
  if (fileHandle->index > fileHandle->size) fileHandle->size = fileHandle->index;
  if (n != bufferLength)
  {
    return ERROR_IO_ERROR;
  }

  return ERROR_NONE;
}

uint64 Files_getSize(FileHandle *fileHandle)
{
  assert(fileHandle != NULL);

  return fileHandle->size;
}

Errors Files_tell(FileHandle *fileHandle, uint64 *offset)
{
  off64_t n;

  assert(fileHandle != NULL);
  assert(offset != NULL);

  n = lseek64(fileHandle->handle,0,SEEK_CUR);
  if (n == (off64_t)-1)
  {
    return ERROR_IO_ERROR;
  }
assert(n == (off64_t)fileHandle->index);

  (*offset) = fileHandle->index;

  return ERROR_NONE;
}

Errors Files_seek(FileHandle *fileHandle, uint64 offset)
{
  assert(fileHandle != NULL);

  if (lseek64(fileHandle->handle,(off64_t)offset,SEEK_SET) == (off64_t)-1)
  {
    return ERROR_IO_ERROR;
  }
  fileHandle->index = offset;

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Files_makeDirectory(const String pathName)
{
  StringTokenizer pathNameTokenizer;
  String          directoryName;
  String          name;

  assert(pathName != NULL);

  directoryName = String_new();
  Files_initSplitFileName(&pathNameTokenizer,pathName);
  if (Files_getNextSplitFileName(&pathNameTokenizer,&name))
  {
    if (String_length(name) > 0)
    {
      Files_setFileName(directoryName,name);
    }
    else
    {
      Files_setFileNameChar(directoryName,FILES_PATHNAME_SEPARATOR_CHAR);
    }
  }
  while (Files_getNextSplitFileName(&pathNameTokenizer,&name))
  {
    if (String_length(name) > 0)
    {     
      Files_appendFileName(directoryName,name);

      if (!Files_exist(directoryName))
      {
        if (mkdir(String_cString(directoryName),0700) != 0)
        {
          Files_doneSplitFileName(&pathNameTokenizer);
          String_delete(directoryName);
          return ERROR_IO_ERROR;
        }
      }
    }
  }
  Files_doneSplitFileName(&pathNameTokenizer);
  String_delete(directoryName);

  return ERROR_NONE;
}

Errors Files_openDirectory(DirectoryHandle *directoryHandle,
                           const String    pathName)
{
  assert(directoryHandle != NULL);
  assert(pathName != NULL);

  directoryHandle->handle = opendir(String_cString(pathName));
  if (directoryHandle->handle == NULL)
  {
    return ERROR_OPEN_DIRECTORY;
  }

  directoryHandle->name  = String_copy(pathName);
  directoryHandle->entry = NULL;

  return ERROR_NONE;
}

void Files_closeDirectory(DirectoryHandle *directoryHandle)
{
  assert(directoryHandle != NULL);

  closedir(directoryHandle->handle);
  String_delete(directoryHandle->name);
}

bool Files_endOfDirectory(DirectoryHandle *directoryHandle)
{
  assert(directoryHandle != NULL);

  /* read entry iff not read */
  if (directoryHandle->entry == NULL)
  {
    directoryHandle->entry = readdir(directoryHandle->handle);
  }

  /* skip "." and ".." entries */
  while (   (directoryHandle->entry != NULL)
         && (   (strcmp(directoryHandle->entry->d_name,"." ) == 0)
             || (strcmp(directoryHandle->entry->d_name,"..") == 0)
            )
        )
  {
    directoryHandle->entry = readdir(directoryHandle->handle);
  }

  return directoryHandle->entry == NULL;
}

Errors Files_readDirectory(DirectoryHandle *directoryHandle,
                           String          fileName
                          )
{
  assert(directoryHandle != NULL);
  assert(fileName != NULL);

  /* read entry iff not read */
  if (directoryHandle->entry == NULL)
  {
    directoryHandle->entry = readdir(directoryHandle->handle);
  }

  /* skip "." and ".." entries */
  while (   (directoryHandle->entry != NULL)
         && (   (strcmp(directoryHandle->entry->d_name,"." ) == 0)
             || (strcmp(directoryHandle->entry->d_name,"..") == 0)
            )
        )
  {
    directoryHandle->entry = readdir(directoryHandle->handle);
  }
  if (directoryHandle->entry == NULL)
  {
    return ERROR_IO_ERROR;
  }

  String_set(fileName,directoryHandle->name);
  String_appendChar(fileName,FILES_PATHNAME_SEPARATOR_CHAR);
  String_appendCString(fileName,directoryHandle->entry->d_name);

  directoryHandle->entry = NULL;

  return ERROR_NONE;
}

FileTypes Files_getType(String fileName)
{
  struct stat fileStat;

  assert(fileName != NULL);

  if (lstat(String_cString(fileName),&fileStat) == 0)
  {
    if      (S_ISREG(fileStat.st_mode)) return FILETYPE_FILE;
    else if (S_ISDIR(fileStat.st_mode)) return FILETYPE_DIRECTORY;
    else if (S_ISLNK(fileStat.st_mode)) return FILETYPE_LINK;
    else                                return FILETYPE_UNKNOWN;
  }
  else
  {
    return FILETYPE_UNKNOWN;
  }
}

bool Files_exist(String fileName)
{
  struct stat fileStat;

  assert(fileName != NULL);

  return (stat(String_cString(fileName),&fileStat) == 0);
}

Errors Files_getFileInfo(String   fileName,
                         FileInfo *fileInfo
                        )
{
  struct stat fileStat;

  assert(fileName != NULL);
  assert(fileInfo != NULL);

  if (lstat(String_cString(fileName),&fileStat) != 0)
  {
    return ERROR_IO_ERROR;
  }

  fileInfo->size            = fileStat.st_size;
  fileInfo->timeLastAccess  = fileStat.st_atime;
  fileInfo->timeModified    = fileStat.st_mtime;
  fileInfo->timeLastChanged = fileStat.st_ctime;
  fileInfo->userId          = fileStat.st_uid;
  fileInfo->groupId         = fileStat.st_gid;
  fileInfo->permission      = fileStat.st_mode;

  return ERROR_NONE;
}

Errors Files_setFileInfo(String   fileName,
                         FileInfo *fileInfo
                        )
{
  struct utimbuf utimeBuffer;

  assert(fileName != NULL);
  assert(fileInfo != NULL);

  utimeBuffer.actime  = fileInfo->timeLastAccess;
  utimeBuffer.modtime = fileInfo->timeModified;
  if (utime(String_cString(fileName),&utimeBuffer) != 0)
  {
    return ERROR_IO_ERROR;
  }
  if (chown(String_cString(fileName),fileInfo->userId,fileInfo->groupId) != 0)
  {
    return ERROR_IO_ERROR;
  }
  if (chmod(String_cString(fileName),fileInfo->permission) != 0)
  {
    return ERROR_IO_ERROR;
  }

  return ERROR_NONE;
}

Errors Files_readLink(String linkName,
                      String fileName
                     )
{
  #define BUFFER_SIZE  256
  #define BUFFER_DELTA 128

  char *buffer;
  uint bufferSize;
  int  result;

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
    free(buffer);
    return ERROR_IO_ERROR;
  }
}

Errors Files_link(String linkName,
                  String fileName
                 )
{
  assert(linkName != NULL);
  assert(fileName != NULL);

  unlink(String_cString(linkName));
  if (symlink(String_cString(fileName),String_cString(linkName)) != 0)
  {
    return ERROR_IO_ERROR;
  }

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
