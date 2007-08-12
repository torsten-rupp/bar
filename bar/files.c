/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/files.c,v $
* $Revision: 1.2 $
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

LOCAL bool endOfFile(void *userData)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;
  off64_t     n;

  assert(archiveInfo != NULL);

  n = lseek64(archiveInfo->handle,0,SEEK_CUR);
  if (n == (off64_t)-1)
  {
    return TRUE;
  }

  return (n >= archiveInfo->size);
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

  if (!chunks_initF(endOfFile,
                   readFile,
                   writeFile,
                   tellFile,
                   seekFile
                  )
     )
  {
    return ERROR_INIT;
  }

  archiveInfo->fileName             = String_newCString(archiveFileName); 
  archiveInfo->partSize             = partSize;                           
  archiveInfo->partNumber           = 0;                                  
  archiveInfo->handle               = -1;                                 
//  archiveInfo->index                = 0;                                  
//  archiveInfo->size                 = 0;                                  

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

  if (!chunks_initF(endOfFile,
                   readFile,
                   writeFile,
                   tellFile,
                   seekFile
                  )
     )
  {
    return ERROR_INIT;
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
  archiveInfo->fileName            = String_newCString(archiveFileName);
  archiveInfo->partSize            = 0;
  archiveInfo->partNumber          = 0;
  archiveInfo->handle              = handle;
//  archiveInfo->index               = 0;
  archiveInfo->size                = size;

  return ERROR_NONE;
}

bool files_eof(ArchiveInfo *archiveInfo)
{
  uint64 offset;

  assert(archiveInfo != NULL);
  assert(archiveInfo->handle >= 0);

  if (!tellFile(archiveInfo,&offset))
  {
    return TRUE;
  }

  return offset >= archiveInfo->size;
}

/*
Errors files_next(ArchiveInfo *archiveInfo,
                  ChunkId     *chunkId
                 )
{
  assert(archiveInfo != NULL);
  assert(chunkId != NULL);

  if (!chunks_net(&archiveInfo->chunkInfoEntry,NULL,chunkId))
  {
    return ERROR_IO_ERROR;
  }
//  archiveInfo->index += CHUNK_HEADER_SIZE;

  return ERROR_NONE;
}
*/

Errors files_done(ArchiveInfo *archiveInfo)
{
  assert(archiveInfo != NULL);
  assert(archiveInfo->fileName != NULL);

//  chunks_done(&archiveInfo->chunkInfoData);
//  chunks_done(&archiveInfo->chunkInfoEntry);

  String_delete(archiveInfo->fileName);

  chunks_doneF();

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

  /* init file-chunk */
  if (!chunks_init(&fileInfo->chunkInfoFile,
                   NULL,
                   archiveInfo
                  )
     )
  {
    return ERROR_IO_ERROR;
  }
  if (!chunks_init(&fileInfo->chunkInfoFileEntry,
                   &fileInfo->chunkInfoFile,
                   archiveInfo
                  )
     )
  {
    return ERROR_IO_ERROR;
  }
  if (!chunks_init(&fileInfo->chunkInfoFileData,
                   &fileInfo->chunkInfoFile,
                   archiveInfo
                  )
     )
  {
    return ERROR_IO_ERROR;
  }

  fileInfo->archiveInfo                    = archiveInfo;
  fileInfo->mode                           = FILE_MODE_WRITE;

  fileInfo->chunkFileEntry.fileType        = 0;
  fileInfo->chunkFileEntry.size            = size;
  fileInfo->chunkFileEntry.timeLastAccess  = timeLastAccess;
  fileInfo->chunkFileEntry.timeModified    = timeModified;
  fileInfo->chunkFileEntry.timeLastChanged = timeLastChanged;
  fileInfo->chunkFileEntry.userId          = userId;
  fileInfo->chunkFileEntry.groupId         = groupId;
  fileInfo->chunkFileEntry.permission      = permission;
  fileInfo->chunkFileEntry.name            = fileName;

  fileInfo->chunkFileData.partOffset       = 0;
  fileInfo->chunkFileData.partSize         = 0;

  fileInfo->headerWrittenFlag              = FALSE;

  return ERROR_NONE;
}

Errors files_readFile(ArchiveInfo *archiveInfo,
                      FileInfo    *fileInfo
                     )
{
  ChunkHeader    chunkHeader;
  ChunkFileEntry chunkFileEntry;
  ChunkFileData  chunkFileData;
  bool           foundFileEntry,foundFileData;

  assert(archiveInfo != NULL);
  assert(fileInfo != NULL);

  /* init file-chunk */
  if (!chunks_init(&fileInfo->chunkInfoFile,
                   NULL,
                   archiveInfo
                  )
     )
  {
    return ERROR_IO_ERROR;
  }
  if (!chunks_init(&fileInfo->chunkInfoFileEntry,
                   &fileInfo->chunkInfoFile,
                   archiveInfo
                  )
     )
  {
    return ERROR_IO_ERROR;
  }
  if (!chunks_init(&fileInfo->chunkInfoFileData,
                   &fileInfo->chunkInfoFile,
                   archiveInfo
                  )
     )
  {
    return ERROR_IO_ERROR;
  }

  /* find file chunk */
  do
  {
    if (chunks_eof(archiveInfo))
    {
      return ERROR_END_OF_ARCHIVE;
    }

    if (!chunks_next(archiveInfo,&chunkHeader))
    {
      return ERROR_IO_ERROR;
    }

    if (chunkHeader.id != CHUNK_ID_FILE)
    {
      chunks_skip(archiveInfo,&chunkHeader);
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_FILE);

  /* read file chunk, find file data */
  if (!chunks_open(&fileInfo->chunkInfoFile,
                   &chunkHeader,
                   CHUNK_DEFINITION_FILE,
                   &fileInfo->chunkFile
                  )
     )
  {
    return ERROR_IO_ERROR;
  }
  foundFileEntry = FALSE;
  foundFileData  = FALSE;
  while (   !chunks_eofSub(&fileInfo->chunkInfoFile)
         && (!foundFileEntry || !foundFileData)         
        )
  {
    if (!chunks_nextSub(&fileInfo->chunkInfoFile,&chunkHeader))
    {
      return ERROR_IO_ERROR;
    }

    switch (chunkHeader.id)
    {
      case CHUNK_ID_FILE_ENTRY:
        if (!chunks_open(&fileInfo->chunkInfoFileEntry,
                         &chunkHeader,
                         CHUNK_DEFINITION_FILE_ENTRY,
                         &fileInfo->chunkFileEntry
                        )
           )
        {
          return ERROR_IO_ERROR;
        }
        foundFileEntry = TRUE;
        break;
      case CHUNK_ID_FILE_DATA:
        if (!chunks_open(&fileInfo->chunkInfoFileData,
                         &chunkHeader,
                         CHUNK_DEFINITION_FILE_DATA,
                         &fileInfo->chunkFileData
                        )
           )
        {
          return ERROR_IO_ERROR;
        }
        foundFileData = TRUE;
        break;
      default:
        chunks_skipSub(&fileInfo->chunkInfoFile,&chunkHeader);
        break;
    }
  }
  if (!foundFileEntry)
  {
    return ERROR_NO_FILE_ENTRY;
  }
  if (!foundFileData)
  {
    return ERROR_NO_FILE_DATA;
  }

  fileInfo->archiveInfo = archiveInfo;
  fileInfo->mode        = FILE_MODE_READ;

  return ERROR_NONE;
}

Errors files_closeFile(FileInfo *fileInfo)
{
  assert(fileInfo != NULL);
  assert(fileInfo->archiveInfo != NULL);

  if (fileInfo->mode == FILE_MODE_WRITE)
  {
  // offset, partsize
    if (!chunks_update(&fileInfo->chunkInfoFileData,
                       &fileInfo->chunkFileData
                      )
       )
    {
      return ERROR_IO_ERROR;
    }
    if (!chunks_update(&fileInfo->chunkInfoFile,
                       &fileInfo->chunkFile
                      )
       )
    {
      return ERROR_IO_ERROR;
    }
  }

  if (!chunks_close(&fileInfo->chunkInfoFileData))
  {
    return ERROR_IO_ERROR;
  }
  if (!chunks_close(&fileInfo->chunkInfoFileEntry))
  {
    return ERROR_IO_ERROR;
  }
  if (!chunks_close(&fileInfo->chunkInfoFile))
  {
    return ERROR_IO_ERROR;
  }
//  fileInfo->archiveInfo->index               = fileInfo->archiveInfo->chunkInfoData.index;
  fileInfo->headerWrittenFlag = FALSE;

  return ERROR_NONE;
}

Errors files_writeFileData(FileInfo *fileInfo, const void *buffer, ulong bufferLength)
{
  const char     *p;
  ulong          length;
  uint64         size;
  bool           newPartFlag;
  ulong          restLength;
  ulong          partLength;
  ulong          n;
  String         fileName;
  void           *writeBuffer;
  ulong          writeLength;

  assert(fileInfo != NULL);
  assert(fileInfo->archiveInfo != NULL);

  p      = (char*)buffer;
  length = 0;
  while (length < bufferLength)
  {
    /* split, calculate rest-length */
    if (fileInfo->archiveInfo->partSize > 0)
    {
      if (!tellFile(fileInfo->archiveInfo,&size))
      {
        return ERROR_IO_ERROR;
      }

      /* check if file-header can be written */
      newPartFlag = FALSE;
      if      (   !fileInfo->headerWrittenFlag
               && (size + fileInfo->headerLength >= fileInfo->archiveInfo->partSize)
              )
      {
        newPartFlag = TRUE;
      }
      else if (size >= fileInfo->archiveInfo->partSize)
      {
        newPartFlag = TRUE;
      }
      if (newPartFlag)
      {
        if (fileInfo->archiveInfo->handle >= 0)
        {
          /* close file, prepare for next part */
          if (fileInfo->headerWrittenFlag)
          {
            if (!chunks_close(&fileInfo->chunkInfoFileData))
            {
              return ERROR_IO_ERROR;
            }
            if (!chunks_close(&fileInfo->chunkInfoFileEntry))
            {
              return ERROR_IO_ERROR;
            }
            if (!chunks_close(&fileInfo->chunkInfoFile))
            {
              return ERROR_IO_ERROR;
            }
            fileInfo->headerWrittenFlag = FALSE;
          }

          close(fileInfo->archiveInfo->handle);
          fileInfo->archiveInfo->handle = -1;
//          fileInfo->archiveInfo->index  = 0;
//          fileInfo->archiveInfo->size   = 0;
        }
      }

      /* get size of space to reserve for chunk-header */
      if (!fileInfo->headerWrittenFlag || newPartFlag)
      {
        n = fileInfo->headerLength;
      }
      else
      {
        n = 0;
      }

      /* calculate max. length of data to write */
      restLength = fileInfo->archiveInfo->partSize-(size+n);

      /* calculate length of data to write */
      partLength = (restLength < (bufferLength-length))?restLength:bufferLength-length;
    }
    else
    {
      partLength = bufferLength - length;
    }

    /* compress */
writeBuffer=(char*)buffer;
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

//      fileInfo->archiveInfo->index               = 0;
//      fileInfo->archiveInfo->size                = 0;
      fileInfo->headerWrittenFlag = FALSE;

      String_delete(fileName);
    }

    /* write chunk-header */
    if (!fileInfo->headerWrittenFlag)
    {
      if (!chunks_new(&fileInfo->chunkInfoFile,
                      CHUNK_ID_FILE,
                      NULL,
                      NULL
                     )
         )
      {
        return ERROR_IO_ERROR;
      }
//      fileInfo->archiveInfo->size += CHUNK_HEADER_SIZE;

      if (!chunks_new(&fileInfo->chunkInfoFileEntry,
                      CHUNK_ID_FILE_ENTRY,
                      CHUNK_DEFINITION_FILE_ENTRY,
                      &fileInfo->chunkFileEntry
                     )
         )
      {
        return ERROR_IO_ERROR;
      }
//      fileInfo->archiveInfo->size += chunks_getSize(&fileInfo->chunkInfoFileEntry,&chunkFileEntry);

      if (!chunks_new(&fileInfo->chunkInfoFileData,
                      CHUNK_ID_FILE_DATA,
                      CHUNK_DEFINITION_FILE_DATA,
                      &fileInfo->chunkFileData
                     )
         )
      {
        return ERROR_IO_ERROR;
      }
//      fileInfo->archiveInfo->size += chunks_getSize(&fileInfo->chunkInfoFileData,&chunkFileData);

      fileInfo->chunkFileData.partOffset = fileInfo->chunkFileData.partOffset+fileInfo->chunkFileData.partSize;
      fileInfo->chunkFileData.partSize   = 0;

      fileInfo->headerLength      = chunks_getSize(&fileInfo->chunkInfoFile,     &fileInfo->chunkFile)+
                                    chunks_getSize(&fileInfo->chunkInfoFileEntry,&fileInfo->chunkFileEntry)+
                                    chunks_getSize(&fileInfo->chunkInfoFileData, &fileInfo->chunkFileData);
      fileInfo->headerWrittenFlag = TRUE;
    }

    /* write */
    if (!chunks_writeData(&fileInfo->chunkInfoFileData,writeBuffer,writeLength))
    {
      return ERROR_IO_ERROR;
    }
//    fileInfo->archiveInfo->size += writeLength;
    fileInfo->chunkFileData.partSize += partLength;

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
