/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/files.c,v $
* $Revision: 1.4 $
* $Author: torsten $
* Contents: file functions
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

Errors files_create(FileHandle   *fileHandle,
                    const String fileName
                   )
{
  assert(fileName != NULL);
  assert(fileHandle != NULL);

  fileHandle->handle = open(String_cString(fileName),O_CREAT|O_RDWR|O_TRUNC,0644);
  if (fileHandle->handle == -1)
  {
    return ERROR_CREATE_FILE;
  }
  fileHandle->size = 0;

  return ERROR_NONE;
}

Errors files_open(FileHandle   *fileHandle,
                  const String fileName                 
                 )
{
  off64_t n;

  assert(fileName != NULL);
  assert(fileHandle != NULL);

  /* open file */
  fileHandle->handle = open(String_cString(fileName),O_RDONLY,0);
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

  return ERROR_NONE;
}

Errors files_close(FileHandle *fileHandle)
{
  assert(fileHandle != NULL);
  assert(fileHandle->handle >= 0);

  close(fileHandle->handle);
  fileHandle->handle = -1;

  return ERROR_NONE;
}

bool files_eof(FileHandle *fileHandle)
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

Errors files_read(FileHandle *fileHandle,
                  void       *buffer,
                  ulong      bufferLength)
{
  assert(fileHandle != NULL);

  if (read(fileHandle->handle,buffer,bufferLength) != bufferLength)
  {
    return ERROR_IO_ERROR;
  }

  return ERROR_NONE;
}

Errors files_write(FileHandle *fileHandle,
                   const void *buffer,
                   ulong      bufferLength)
{
  assert(fileHandle != NULL);

  if (write(fileHandle->handle,buffer,bufferLength) != bufferLength)
  {
    return ERROR_IO_ERROR;
  }

  return ERROR_NONE;
}

Errors files_tell(FileHandle *fileHandle, uint64 *offset)
{
  off64_t n;

  assert(fileHandle != NULL);
  assert(offset != NULL);

  n = lseek64(fileHandle->handle,0,SEEK_CUR);
  if (n == (off64_t)-1)
  {
    return ERROR_IO_ERROR;
  }
  (*offset) = (uint64)n;

  return ERROR_NONE;
}

Errors files_seek(FileHandle *fileHandle, uint64 offset)
{
  assert(fileHandle != NULL);

  if (lseek64(fileHandle->handle,(off64_t)offset,SEEK_SET) == (off64_t)-1)
  {
    return ERROR_IO_ERROR;
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors files_openDirectory(DirectoryHandle *directoryHandle,
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

void files_closeDirectory(DirectoryHandle *directoryHandle)
{
  assert(directoryHandle != NULL);

  closedir(directoryHandle->handle);
  String_delete(directoryHandle->name);
}

bool files_endOfDirectory(DirectoryHandle *directoryHandle)
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

Errors files_readDirectory(DirectoryHandle *directoryHandle,
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

FileTypes files_getType(String fileName)
{
  struct stat fileStat;

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

Errors files_getInfo(String   fileName,
                     FileInfo *fileInfo
                    )
{
  struct stat     fileStat;

  assert(fileName != NULL);
  assert(fileInfo != NULL);

  if (lstat(String_cString(fileName),&fileStat) != 0)
  {
    return ERROR_IO_ERROR;
  }

  String_set(fileInfo->name,fileName);
  fileInfo->size            = fileStat.st_size;
  fileInfo->timeLastAccess  = fileStat.st_atime;
  fileInfo->timeModified    = fileStat.st_mtime;
  fileInfo->timeLastChanged = fileStat.st_ctime;
  fileInfo->userId          = fileStat.st_uid;
  fileInfo->groupId         = fileStat.st_gid;
  fileInfo->permission      = fileStat.st_mode;

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
