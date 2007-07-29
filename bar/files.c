/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/files.c,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: 
* Systems :
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "lists.h"

#include "bar.h"
#include "archive_format.h"
#include "chunks.h"

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

LOCAL bool nextFile(void *userData)
{
#if 0
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;

  assert(archiveInfo != NULL);

  /* create new entry */
  if (!chunks_new(&chunkInfoBlock,BAR_CHUNK_ID_FILE))
  {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
// log ???
    return FALSE;
  }

  /* write file info */
//  if (!chunks_write(&chunkInfoBlock,&fileInfoBlock.chunkFile,BAR_CHUNK_DEFINITION_FILE))
//  {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
// log ???
    return FALSE;
//  }

  return TRUE;
#endif /* 0 */
    return FALSE;
}

LOCAL bool closeFile(void *userData)
{
    return FALSE;
}

LOCAL bool readFile(void *userData, void *buffer, ulong bufferLength)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;

  assert(archiveInfo != NULL);

  return (read(archiveInfo->handle,buffer,bufferLength) == bufferLength);
}

LOCAL bool writeFile(void *userData, const void *buffer, ulong bufferLength)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;

  assert(archiveInfo != NULL);

  return (write(archiveInfo->handle,buffer,bufferLength) == bufferLength);
}

LOCAL bool tellFile(void *userData, uint64 *offset)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;
  off64_t     n;

  assert(archiveInfo != NULL);
  assert(offset != NULL);

  n = lseek64(archiveInfo->handle,0,SEEK_CUR);
  if (n == (off64_t)-1)
  {
    return FALSE;
  }
  (*offset) = (uint64)n;

  return TRUE;
}

LOCAL bool seekFile(void *userData, uint64 offset)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;

  assert(archiveInfo != NULL);

  if (lseek64(archiveInfo->handle,(off64_t)offset,SEEK_SET) == (off64_t)-1)
  {
    return FALSE;
  }

  return TRUE;
}

/*---------------------------------------------------------------------*/


Errors files_create(ArchiveInfo *archiveInfo,
                    const char  *archiveFileName,
                    uint64      partSize
                   )
{
  assert(archiveInfo != NULL);
  assert(archiveFileName != NULL);

  archiveInfo->fileName   = String_newCString(archiveFileName);
  archiveInfo->partSize   = partSize;
  archiveInfo->partNumber = 0;
  archiveInfo->handle     = -1;
  archiveInfo->index      = 0;
  archiveInfo->size       = 0;
  archiveInfo->chunkFlag  = FALSE;

  /* init file-chunk */
  if (!chunks_init(&archiveInfo->chunkInfo,
                   readFile,
                   writeFile,
                   tellFile,
                   seekFile,
                   archiveInfo
                  )
     )
  {
    return ERROR_IO_ERROR;
  }

  return ERROR_NONE;
}

Errors files_open(ArchiveInfo *archiveInfo,
                  const char  *archiveFileName
                 )
{
  int     handle;
  off64_t n;
  uint64  size;

  assert(archiveInfo != NULL);
  assert(archiveFileName != NULL);

  /* init file-chunk */
  if (!chunks_init(&archiveInfo->chunkInfo,
                   readFile,
                   writeFile,
                   tellFile,
                   seekFile,
                   archiveInfo
                  )
     )
  {
    return ERROR_IO_ERROR;
  }

  /* open file */
  handle = open(archiveFileName,O_RDONLY,0);
  if (handle == -1)
  {
    return ERROR_IO_ERROR;
  }

  /* get file size */
  if (lseek64(handle,(off64_t)0,SEEK_END) == (off64_t)-1)
  {
    close(handle);
    return ERROR_IO_ERROR;
  }
  n = lseek64(handle,0,SEEK_CUR);
  if (n == (off64_t)-1)
  {
    close(handle);
    return ERROR_IO_ERROR;
  }
  size = (uint64)n;
  if (lseek64(handle,(off64_t)0,SEEK_SET) == (off64_t)-1)
  {
    close(handle);
    return ERROR_IO_ERROR;
  }

  /* init */
  archiveInfo->fileName   = String_newCString(archiveFileName);
  archiveInfo->partSize   = 0;
  archiveInfo->partNumber = 0;
  archiveInfo->handle     = handle;
  archiveInfo->index      = 0;
  archiveInfo->size       = size;
  archiveInfo->chunkFlag  = FALSE;

  return ERROR_NONE;
}

bool files_eof(ArchiveInfo *archiveInfo)
{
  assert(archiveInfo != NULL);
  assert(archiveInfo->handle >= 0);

  return archiveInfo->index >= archiveInfo->size;
}

Errors files_getNext(ArchiveInfo *archiveInfo,
                     ChunkId     *chunkId
                    )
{
  ChunkHeader chunkHeader;

  assert(archiveInfo != NULL);
  assert(chunkId != NULL);

  if (archiveInfo->chunkFlag)
  {
    if (!chunks_close(&archiveInfo->chunkInfo))
    {
      return ERROR_IO_ERROR;
    }
    archiveInfo->chunkFlag = FALSE;
  }

  if (!chunks_get(&archiveInfo->chunkInfo))
  {
    return ERROR_IO_ERROR;
  }
  archiveInfo->index += CHUNK_HEADER_SIZE;

  (*chunkId) = archiveInfo->chunkInfo.id;

  return ERROR_NONE;
}

Errors files_done(ArchiveInfo *archiveInfo)
{
  assert(archiveInfo != NULL);
  assert(archiveInfo->fileName != NULL);

  chunks_done(&archiveInfo->chunkInfo);

  String_delete(archiveInfo->fileName);

  return ERROR_NONE;
}

Errors files_newFile(ArchiveInfo *archiveInfo,
                     FileInfo    *fileInfo,
                     String      fileName,
                     uint64      size,
                     uint64      timeLastAccess,
                     uint64      timeModified,
                     uint64      timeLastChanged,
                     uint32      userId,
                     uint32      groupId,
                     uint32      permission
                    )
{
  assert(archiveInfo != NULL);
  assert(fileInfo != NULL);

  fileInfo->archiveInfo     = archiveInfo;

  fileInfo->name            = fileName;
  fileInfo->size            = size;
  fileInfo->timeLastAccess  = timeLastAccess;
  fileInfo->timeModified    = timeModified;
  fileInfo->timeLastChanged = timeLastChanged;
  fileInfo->userId          = userId;
  fileInfo->groupId         = groupId;
  fileInfo->permission      = permission;

  fileInfo->partOffset      = 0;
  fileInfo->partSize        = 0;

  return ERROR_NONE;
}

Errors files_readFile(ArchiveInfo *archiveInfo,
                      FileInfo    *fileInfo
                     )
{
  BARChunk_File chunkFile;

  assert(archiveInfo != NULL);
  assert(fileInfo != NULL);

  if (!chunks_read(&archiveInfo->chunkInfo,&chunkFile,BAR_CHUNK_DEFINITION_FILE))
  {
    return ERROR_IO_ERROR;
  }
  archiveInfo->index += chunks_getSize(&chunkFile,BAR_CHUNK_DEFINITION_FILE);

  fileInfo->archiveInfo     = archiveInfo;

  fileInfo->fileType        = chunkFile.fileType;
  fileInfo->size            = chunkFile.size;
  fileInfo->timeLastAccess  = chunkFile.timeLastAccess;
  fileInfo->timeModified    = chunkFile.timeModified;
  fileInfo->timeLastChanged = chunkFile.timeLastChanged;
  fileInfo->userId          = chunkFile.userId;
  fileInfo->groupId         = chunkFile.groupId;
  fileInfo->permission      = chunkFile.permission;
  fileInfo->name            = chunkFile.name;

  return ERROR_NONE;
}

Errors files_closeFile(FileInfo *fileInfo)
{
  off64_t n;

  assert(fileInfo != NULL);
  assert(fileInfo->archiveInfo != NULL);

  if (!chunks_close(&fileInfo->archiveInfo->chunkInfo))
  {
    return ERROR_IO_ERROR;
  }
  fileInfo->archiveInfo->index = fileInfo->archiveInfo->chunkInfo.index;
  fileInfo->archiveInfo->chunkFlag = FALSE;

  return ERROR_NONE;
}

Errors files_writeFileData(FileInfo *fileInfo, const void *buffer, ulong bufferLength)
{
  const char    *p;
  ulong         length;
  BARChunk_File chunkFile;
  ulong         fileChunkHeaderLength;
  bool          newPartFlag;
  ulong         restLength;
  ulong         partLength;
  ulong         n;
  String        fileName;
  void          *writeBuffer;
  ulong         writeLength;

  assert(fileInfo != NULL);
  assert(fileInfo->archiveInfo != NULL);

  p      = (char*)buffer;
  length = 0;
  while (length < bufferLength)
  {
    /* split, calculate rest-length */
    if (fileInfo->archiveInfo->partSize > 0)
    {
      /* get chunk-header length */
      chunkFile.name        = fileInfo->name;     
      fileChunkHeaderLength = chunks_getSize(&chunkFile,BAR_CHUNK_DEFINITION_FILE);

      /* check if file-header can be written */
      newPartFlag = FALSE;
      if      (!fileInfo->archiveInfo->chunkFlag && (fileInfo->archiveInfo->size + fileChunkHeaderLength >= fileInfo->archiveInfo->partSize))
      {
        newPartFlag = TRUE;
      }
      else if (fileInfo->archiveInfo->size >= fileInfo->archiveInfo->partSize)
      {
        newPartFlag = TRUE;
      }
      if (newPartFlag)
      {
        if (fileInfo->archiveInfo->handle >= 0)
        {
          /* close file, prepare for next part */
          if (fileInfo->archiveInfo->chunkFlag)
          {
            if (!chunks_close(&fileInfo->archiveInfo->chunkInfo))
            {
              return ERROR_IO_ERROR;
            }
            fileInfo->archiveInfo->chunkFlag = FALSE;
          }

          close(fileInfo->archiveInfo->handle);
          fileInfo->archiveInfo->handle = -1;
          fileInfo->archiveInfo->index  = 0;
          fileInfo->archiveInfo->size   = 0;
        }
      }

      /* get size of space to reserve for chunk-header */
      if (!fileInfo->archiveInfo->chunkFlag || newPartFlag)
      {
        n = CHUNK_HEADER_SIZE+fileChunkHeaderLength;
      }
      else
      {
        n = 0;
      }

      /* calculate max. length of data to write */
      restLength = fileInfo->archiveInfo->partSize-(fileInfo->archiveInfo->size+n);

      /* calculate length of data to write */
      partLength = (restLength < (bufferLength-length))?restLength:bufferLength-length;
    }
    else
    {
      partLength = bufferLength - length;
    }

    /* compress */
writeBuffer=buffer;
writeLength=partLength;
    if (1)
    {
    }

    /* encrypt */
    if (0)
    {
    }

    /* open file */
    if (fileInfo->archiveInfo->handle < 0)
    {
      /* get output filename */
      if (fileInfo->archiveInfo->partSize > 0)
      {
        fileName = String_format(String_new(),"%S.%06d",fileInfo->archiveInfo->fileName,fileInfo->archiveInfo->partNumber);
        fileInfo->archiveInfo->partNumber++;
      }
      else
      {
        fileName = String_copy(fileInfo->archiveInfo->fileName);
      }

      /* create file */
      fileInfo->archiveInfo->handle = open(String_cString(fileName),O_CREAT|O_RDWR|O_TRUNC,0644);
      if (fileInfo->archiveInfo->handle == -1)
      {
        return ERROR_IO_ERROR;
      }
      fileInfo->archiveInfo->index     = 0;
      fileInfo->archiveInfo->size      = 0;
      fileInfo->archiveInfo->chunkFlag = FALSE;

      String_delete(fileName);
    }

    /* write chunk-header */
    if (!fileInfo->archiveInfo->chunkFlag)
    {
      if (!chunks_new(&fileInfo->archiveInfo->chunkInfo,BAR_CHUNK_ID_FILE))
      {
        return ERROR_IO_ERROR;
      }
      fileInfo->archiveInfo->size += CHUNK_HEADER_SIZE;

      chunkFile.fileType        = fileInfo->fileType;
      chunkFile.size            = fileInfo->size;
      chunkFile.timeLastAccess  = fileInfo->timeLastAccess;
      chunkFile.timeModified    = fileInfo->timeModified;
      chunkFile.timeLastChanged = fileInfo->timeLastChanged;
      chunkFile.userId          = fileInfo->userId;
      chunkFile.groupId         = fileInfo->groupId;
      chunkFile.permission      = fileInfo->permission;
      chunkFile.name            = fileInfo->name;
// offset, partsize
      if (!chunks_write(&fileInfo->archiveInfo->chunkInfo,&chunkFile,BAR_CHUNK_DEFINITION_FILE))
      {
        return ERROR_IO_ERROR;
      }
      fileInfo->archiveInfo->size += chunks_getSize(&chunkFile,BAR_CHUNK_DEFINITION_FILE);

      fileInfo->archiveInfo->chunkFlag = TRUE;
    }

    /* write */
    if (!chunks_writeData(&fileInfo->archiveInfo->chunkInfo,writeBuffer,writeLength))
    {
      return ERROR_IO_ERROR;
    }
    fileInfo->archiveInfo->size += writeLength;

    length += partLength;
  }

  return ERROR_NONE;
}

Errors files_readFileData(FileInfo *fileInfo, void *buffer, ulong bufferLength)
{
  assert(fileInfo != NULL);

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
