/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/archive.c,v $
* $Revision: 1.5 $
* $Author: torsten $
* Contents: 
* Systems :
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "lists.h"

#include "bar.h"
#include "archive_format.h"
#include "chunks.h"
#include "files.h"
#include "compress.h"
#include "crypt.h"

#include "archive.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool endOfFile(void *userData)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;

  assert(archiveInfo != NULL);

  return Files_eof(&archiveInfo->fileHandle);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool readFile(void *userData, void *buffer, ulong bufferLength)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;
  ulong       readBytes;

  assert(archiveInfo != NULL);

  if (Files_read(&archiveInfo->fileHandle,buffer,bufferLength,&readBytes) != ERROR_NONE)
  {
    return FALSE;
  }

  return (readBytes == bufferLength);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool writeFile(void *userData, const void *buffer, ulong bufferLength)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;

  assert(archiveInfo != NULL);

  return (Files_write(&archiveInfo->fileHandle,buffer,bufferLength) == ERROR_NONE);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool tellFile(void *userData, uint64 *offset)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;

  assert(archiveInfo != NULL);
  assert(offset != NULL);

  return (Files_tell(&archiveInfo->fileHandle,offset) == ERROR_NONE);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool seekFile(void *userData, uint64 offset)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;

  assert(archiveInfo != NULL);

  return (Files_seek(&archiveInfo->fileHandle,offset) == ERROR_NONE);
}

/*---------------------------------------------------------------------*/

#if 0
/***********************************************************************\
* Name   : flushBuffer
* Purpose: flush output buffer: compress and encrypt
* Input  : archiveFileInfo - file info
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors flushBuffer(ArchiveFileInfo *archiveFileInfo)
{
  ulong      length;
  uint64     size;
  bool       newPartFlag;
  ulong      restLength;
  ulong      partLength;
  ulong      n;
  String     fileName;
  Errors     error;
//  ulong      compressLength;
  ulong      cryptLength;
  void       *dataBuffer;
  ulong      dataLength;

  assert(archiveFileInfo != NULL);
  assert(archiveFileInfo->archiveInfo != NULL);
  assert((archiveFileInfo->bufferLength%archiveFileInfo->blockLength) == 0);

  length = 0;
  while (length < archiveFileInfo->bufferLength)
  {
    dataBuffer = (char*)archiveFileInfo->buffer+length;
    dataLength = archiveFileInfo->bufferLength-length;

    /* split, calculate rest-length */
    if (archiveFileInfo->archiveInfo->partSize > 0)
    {
      /* check if new file-part is needed */
      newPartFlag = FALSE;
      if (archiveFileInfo->archiveInfo->fileHandle.handle == -1)
      {
        /* not open -> new part */
        newPartFlag = TRUE;
      }
      else
      {
        /* get current size */
        if (!tellFile(archiveFileInfo->archiveInfo,&size))
        {
          return ERROR_IO_ERROR;
        }

        if      (   !archiveFileInfo->headerWrittenFlag
                 && (size + archiveFileInfo->headerLength >= archiveFileInfo->archiveInfo->partSize)
                )
        {
          /* file header cannot be written without fragmentation -> new part */
          newPartFlag = TRUE;
        }
        else if (size >= archiveFileInfo->archiveInfo->partSize)
        {
          /* part is full -> new part */
          newPartFlag = TRUE;
        }
      }
      if (newPartFlag)
      {
        if (archiveFileInfo->archiveInfo->fileHandle.handle >= 0)
        {
          /* close file, prepare for next part */
          if (archiveFileInfo->headerWrittenFlag)
          {
            /* update part offset, part size */
            if (!Chunks_update(&archiveFileInfo->chunkInfoFileData,
                               &archiveFileInfo->chunkFileData
                              )
               )
            {
              return ERROR_IO_ERROR;
            }

            /* close chunks */
            if (!Chunks_close(&archiveFileInfo->chunkInfoFileData))
            {
              return ERROR_IO_ERROR;
            }
            if (!Chunks_close(&archiveFileInfo->chunkInfoFileEntry))
            {
              return ERROR_IO_ERROR;
            }
            if (!Chunks_close(&archiveFileInfo->chunkInfoFile))
            {
              return ERROR_IO_ERROR;
            }
            archiveFileInfo->headerWrittenFlag = FALSE;
          }

          Files_close(&archiveFileInfo->archiveInfo->fileHandle);
          archiveFileInfo->archiveInfo->fileHandle.handle = -1;
        }
      }

      /* get size of space to reserve for chunk-header */
      if (!archiveFileInfo->headerWrittenFlag || newPartFlag)
      {
        n = archiveFileInfo->headerLength;
      }
      else
      {
        n = 0;
      }

      /* calculate max. length of data which can be written into this part */
      restLength = archiveFileInfo->archiveInfo->partSize-(size+n);

      /* calculate length of data to write */
      partLength = MIN(restLength,dataLength);
    }
    else
    {
      partLength = dataLength;
    }

    /* compress */
    if (1)
    {
    }

    /* encrypt */
    error = Crypt_encrypt(&archiveFileInfo->cryptInfoFileData,
                          dataBuffer,
                          dataLength,
                          archiveFileInfo->cryptBuffer,
                          &cryptLength,
                          BUFFER_SIZE
                         );
    if (error != ERROR_NONE)
    {
      return error;
    }
    dataBuffer=archiveFileInfo->cryptBuffer;
    dataLength=cryptLength;

    /* open file */
    if (archiveFileInfo->archiveInfo->fileHandle.handle == -1)
    {
      /* get output filename */
      if (archiveFileInfo->archiveInfo->partSize > 0)
      {
        fileName = String_format(String_new(),"%S.%06d",archiveFileInfo->archiveInfo->fileName,archiveFileInfo->archiveInfo->partNumber);
        archiveFileInfo->archiveInfo->partNumber++;
      }
      else
      {
        fileName = String_copy(archiveFileInfo->archiveInfo->fileName);
      }

      /* create file */
      error = Files_open(&archiveFileInfo->archiveInfo->fileHandle,fileName,FILE_OPENMODE_CREATE);
      if (error != ERROR_NONE)
      {
        return error;
      }

  //      archiveFileInfo->archiveInfo->index               = 0;
  //      archiveFileInfo->archiveInfo->size                = 0;
      archiveFileInfo->headerWrittenFlag = FALSE;

      String_delete(fileName);
    }

    /* write chunk-header */
    if (!archiveFileInfo->headerWrittenFlag)
    {
      if (!Chunks_new(&archiveFileInfo->chunkInfoFile,
                      &archiveFileInfo->chunkFile
                     )
         )
      {
        return ERROR_IO_ERROR;
      }
  //      archiveFileInfo->archiveInfo->size += CHUNK_HEADER_SIZE;

      if (!Chunks_new(&archiveFileInfo->chunkInfoFileEntry,
                      &archiveFileInfo->chunkFileEntry
                     )
         )
      {
        return ERROR_IO_ERROR;
      }
  //      archiveFileInfo->archiveInfo->size += Chunks_getSize(&archiveFileInfo->chunkInfoFileEntry,&chunkFileEntry);

      if (!Chunks_new(&archiveFileInfo->chunkInfoFileData,
                      &archiveFileInfo->chunkFileData
                     )
         )
      {
        return ERROR_IO_ERROR;
      }
  //      archiveFileInfo->archiveInfo->size += Chunks_getSize(&archiveFileInfo->chunkInfoFileData,&chunkFileData);

      archiveFileInfo->chunkFileData.partOffset = archiveFileInfo->chunkFileData.partOffset+archiveFileInfo->chunkFileData.partSize;
      archiveFileInfo->chunkFileData.partSize   = 0;

      archiveFileInfo->headerWrittenFlag = TRUE;
    }

    /* write */
    if (!Chunks_writeData(&archiveFileInfo->chunkInfoFileData,dataBuffer,dataLength))
    {
      return ERROR_IO_ERROR;
    }
  //    archiveFileInfo->archiveInfo->size += dataLength;
    archiveFileInfo->chunkFileData.partSize += partLength;

    length += partLength;
  }

  archiveFileInfo->bufferLength = 0;

  return ERROR_NONE;
}
#endif /* 0 */

LOCAL Errors writeBlock(ArchiveFileInfo *archiveFileInfo)
{
  ulong  length;
  uint64 size;
  bool   newPartFlag;
  ulong  n;
  String fileName;
  Errors error;

  assert(archiveFileInfo != NULL);
  assert(!Compress_checkBlockIsEmpty(&archiveFileInfo->compressInfo));

  /* get block */
  Compress_getBlock(&archiveFileInfo->compressInfo,
                    archiveFileInfo->buffer,
                    &length
                   );

  /* pad block */
  memset((byte*)archiveFileInfo->buffer+length,
         0,
         archiveFileInfo->blockLength-length
        );
         

  /* encrypt block */
  error = Crypt_encrypt(&archiveFileInfo->cryptInfoFileData,
                        archiveFileInfo->buffer,
                        archiveFileInfo->blockLength
                       );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* split, calculate rest-length */
  if (archiveFileInfo->archiveInfo->partSize > 0)
  {
    /* check if new file-part is needed */
    newPartFlag = FALSE;
    if (archiveFileInfo->archiveInfo->fileHandle.handle == -1)
    {
      /* not open -> new part */
      newPartFlag = TRUE;
    }
    else
    {
      /* get current size */
      if (!tellFile(archiveFileInfo->archiveInfo,&size))
      {
        return ERROR_IO_ERROR;
      }

      if      (   !archiveFileInfo->headerWrittenFlag
               && (size + archiveFileInfo->headerLength >= archiveFileInfo->archiveInfo->partSize)
              )
      {
        /* file header cannot be written without fragmentation -> new part */
        newPartFlag = TRUE;
      }
      else if ((size+archiveFileInfo->blockLength) > archiveFileInfo->archiveInfo->partSize)
      {
        /* less than one block left in part -> new part */
        newPartFlag = TRUE;
      }
    }
    if (newPartFlag)
    {
      if (archiveFileInfo->archiveInfo->fileHandle.handle >= 0)
      {
        /* close file, prepare for next part */
        if (archiveFileInfo->headerWrittenFlag)
        {
          /* update part offset, part size */
          if (!Chunks_update(&archiveFileInfo->chunkInfoFileData,
                             &archiveFileInfo->chunkFileData
                            )
             )
          {
            return ERROR_IO_ERROR;
          }

          /* close chunks */
          if (!Chunks_close(&archiveFileInfo->chunkInfoFileData))
          {
            return ERROR_IO_ERROR;
          }
          if (!Chunks_close(&archiveFileInfo->chunkInfoFileEntry))
          {
            return ERROR_IO_ERROR;
          }
          if (!Chunks_close(&archiveFileInfo->chunkInfoFile))
          {
            return ERROR_IO_ERROR;
          }
          archiveFileInfo->headerWrittenFlag = FALSE;
        }

        Files_close(&archiveFileInfo->archiveInfo->fileHandle);
        archiveFileInfo->archiveInfo->fileHandle.handle = -1;
      }
    }

    /* get size of space to reserve for chunk-header */
    if (!archiveFileInfo->headerWrittenFlag || newPartFlag)
    {
      n = archiveFileInfo->headerLength;
    }
    else
    {
      n = 0;
    }

    /* calculate max. length of data which can be written into this part */
//        freeBlocks = (archiveFileInfo->archiveInfo->partSize-(size+n))/archiveFileInfo->blockLength;

    /* calculate length of data to write */
//        partLength = MIN(restLength,dataLength);
  }
  else
  {
//        partLength = dataLength;
  }

  /* open file */
  if (archiveFileInfo->archiveInfo->fileHandle.handle == -1)
  {
    /* get output filename */
    if (archiveFileInfo->archiveInfo->partSize > 0)
    {
      fileName = String_format(String_new(),"%S.%06d",archiveFileInfo->archiveInfo->fileName,archiveFileInfo->archiveInfo->partNumber);
      archiveFileInfo->archiveInfo->partNumber++;
    }
    else
    {
      fileName = String_copy(archiveFileInfo->archiveInfo->fileName);
    }

    /* create file */
    error = Files_open(&archiveFileInfo->archiveInfo->fileHandle,fileName,FILE_OPENMODE_CREATE);
    if (error != ERROR_NONE)
    {
      return error;
    }

//      archiveFileInfo->archiveInfo->index               = 0;
//      archiveFileInfo->archiveInfo->size                = 0;
    archiveFileInfo->headerWrittenFlag = FALSE;

    String_delete(fileName);
  }

  /* write chunk-header */
  if (!archiveFileInfo->headerWrittenFlag)
  {
    if (!Chunks_new(&archiveFileInfo->chunkInfoFile,
                    &archiveFileInfo->chunkFile
                   )
       )
    {
      return ERROR_IO_ERROR;
    }
//      archiveFileInfo->archiveInfo->size += CHUNK_HEADER_SIZE;

    if (!Chunks_new(&archiveFileInfo->chunkInfoFileEntry,
                    &archiveFileInfo->chunkFileEntry
                   )
       )
    {
      return ERROR_IO_ERROR;
    }
//      archiveFileInfo->archiveInfo->size += Chunks_getSize(&archiveFileInfo->chunkInfoFileEntry,&chunkFileEntry);

    if (!Chunks_new(&archiveFileInfo->chunkInfoFileData,
                    &archiveFileInfo->chunkFileData
                   )
       )
    {
      return ERROR_IO_ERROR;
    }
//      archiveFileInfo->archiveInfo->size += Chunks_getSize(&archiveFileInfo->chunkInfoFileData,&chunkFileData);

    archiveFileInfo->chunkFileData.partOffset = archiveFileInfo->chunkFileData.partOffset+archiveFileInfo->chunkFileData.partSize;
    archiveFileInfo->chunkFileData.partSize   = 0;

    archiveFileInfo->headerWrittenFlag = TRUE;
  }

  /* write */
  if (!Chunks_writeData(&archiveFileInfo->chunkInfoFileData,archiveFileInfo->buffer,archiveFileInfo->blockLength))
  {
    return ERROR_IO_ERROR;
  }
//    archiveFileInfo->archiveInfo->size += dataLength;
  archiveFileInfo->chunkFileData.partSize += length;

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Archive_create(ArchiveInfo     *archiveInfo,
                      const char      *archiveFileName,
                      uint64          partSize,
                      CryptAlgorithms cryptAlgorithm,
                      const char      *password
                     )
{
  assert(archiveInfo != NULL);
  assert(archiveFileName != NULL);

  if (!Chunks_initF(endOfFile,
                   readFile,
                   writeFile,
                   tellFile,
                   seekFile
                  )
     )
  {
    return ERROR_INIT;
  }

  archiveInfo->fileName          = String_newCString(archiveFileName); 
  archiveInfo->partSize          = partSize;                           
  archiveInfo->cryptAlgorithm    = cryptAlgorithm;                           
  archiveInfo->password          = password;  

  archiveInfo->partNumber        = 0;
  archiveInfo->fileHandle.handle = -1;
  archiveInfo->fileHandle.size   = 0;

  return ERROR_NONE;
}

Errors Archive_open(ArchiveInfo *archiveInfo,
                    const char  *archiveFileName,
                    const char  *password
                   )
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveFileName != NULL);

  if (!Chunks_initF(endOfFile,
                   readFile,
                   writeFile,
                   tellFile,
                   seekFile
                  )
     )
  {
    return ERROR_INIT;
  }

  /* init */
  archiveInfo->fileName          = String_newCString(archiveFileName);
  archiveInfo->partSize          = 0;
  archiveInfo->password          = password;

  archiveInfo->partNumber        = 0;
  archiveInfo->fileHandle.handle = -1;
  archiveInfo->fileHandle.size   = 0;

  /* open file */
  error = Files_open(&archiveInfo->fileHandle,archiveInfo->fileName,FILE_OPENMODE_READ);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

bool Archive_eof(ArchiveInfo *archiveInfo)
{
  assert(archiveInfo != NULL);

  return Files_eof(&archiveInfo->fileHandle);
}

Errors Archive_done(ArchiveInfo *archiveInfo)
{
  assert(archiveInfo != NULL);
  assert(archiveInfo->fileName != NULL);

  String_delete(archiveInfo->fileName);

  Chunks_doneF();

  return ERROR_NONE;
}

Errors Archive_newFile(ArchiveInfo     *archiveInfo,
                       ArchiveFileInfo *archiveFileInfo,
                       const String    fileName,
                       const FileInfo  *fileInfo
                      )
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo                    = archiveInfo;
  archiveFileInfo->mode                           = FILE_MODE_WRITE;

  archiveFileInfo->chunkFile.compressAlgorithm    = 0;
  archiveFileInfo->chunkFile.cryptAlgorithm       = archiveInfo->cryptAlgorithm;

  archiveFileInfo->chunkFileEntry.fileType        = 0;
  archiveFileInfo->chunkFileEntry.size            = fileInfo->size;
  archiveFileInfo->chunkFileEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveFileInfo->chunkFileEntry.timeModified    = fileInfo->timeModified;
  archiveFileInfo->chunkFileEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveFileInfo->chunkFileEntry.userId          = fileInfo->userId;
  archiveFileInfo->chunkFileEntry.groupId         = fileInfo->groupId;
  archiveFileInfo->chunkFileEntry.permission      = fileInfo->permission;
  archiveFileInfo->chunkFileEntry.name            = String_copy(fileName);

  archiveFileInfo->chunkFileData.partOffset       = 0;
  archiveFileInfo->chunkFileData.partSize         = 0;

  archiveFileInfo->headerLength                   = 0;
  archiveFileInfo->headerWrittenFlag              = FALSE;

  archiveFileInfo->bufferLength                   = 0;

  /* allocate buffers */
  archiveFileInfo->buffer = malloc(BUFFER_SIZE*2);
  if (archiveFileInfo->buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  archiveFileInfo->compressBuffer = malloc(BUFFER_SIZE*2);
  if (archiveFileInfo->compressBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  archiveFileInfo->cryptBuffer = malloc(BUFFER_SIZE*2);
  if (archiveFileInfo->cryptBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return error;
  }

  /* init compress info */
  error = Compress_new(&archiveFileInfo->compressInfo,
                       archiveFileInfo->blockLength
                      );
  if (error != ERROR_NONE)
  {
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return ERROR_COMPRESS_ERROR;
  }

  /* init crypt */
  error = Crypt_new(&archiveFileInfo->cryptInfoFileEntry,
                    archiveInfo->cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Compress_delete(&archiveFileInfo->compressInfo);
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return error;
  }
  error = Crypt_new(&archiveFileInfo->cryptInfoFileData,
                    archiveInfo->cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    Compress_delete(&archiveFileInfo->compressInfo);
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return error;
  }

  /* init file-chunks */
  if (!Chunks_init(&archiveFileInfo->chunkInfoFile,
                   NULL,
                   archiveInfo,
                   CHUNK_ID_FILE,
                   CHUNK_DEFINITION_FILE,
                   0
                  )
     )
  {
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    Compress_delete(&archiveFileInfo->compressInfo);
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return ERROR_IO_ERROR;
  }
  if (!Chunks_init(&archiveFileInfo->chunkInfoFileEntry,
                   &archiveFileInfo->chunkInfoFile,
                   archiveInfo,
                   CHUNK_ID_FILE_ENTRY,
                   CHUNK_DEFINITION_FILE_ENTRY,
                   archiveFileInfo->blockLength
                  )
     )
  {
    Chunks_done(&archiveFileInfo->chunkInfoFile);
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    Compress_delete(&archiveFileInfo->compressInfo);
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return ERROR_IO_ERROR;
  }
  if (!Chunks_init(&archiveFileInfo->chunkInfoFileData,
                   &archiveFileInfo->chunkInfoFile,
                   archiveInfo,
                   CHUNK_ID_FILE_DATA,
                   CHUNK_DEFINITION_FILE_DATA,
                   archiveFileInfo->blockLength
                  )
     )
  {
    Chunks_done(&archiveFileInfo->chunkInfoFileEntry);
    Chunks_done(&archiveFileInfo->chunkInfoFile);
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    Compress_delete(&archiveFileInfo->compressInfo);
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return ERROR_IO_ERROR;
  }

  /* calculate header length */
  archiveFileInfo->headerLength = Chunks_getSize(&archiveFileInfo->chunkInfoFile,     &archiveFileInfo->chunkFile)+
                                  Chunks_getSize(&archiveFileInfo->chunkInfoFileEntry,&archiveFileInfo->chunkFileEntry)+
                                  Chunks_getSize(&archiveFileInfo->chunkInfoFileData, &archiveFileInfo->chunkFileData);

  return ERROR_NONE;
}

Errors Archive_readFile(ArchiveInfo     *archiveInfo,
                        ArchiveFileInfo *archiveFileInfo,
                        String          fileName,
                        FileInfo        *fileInfo,
                        uint64          *partOffset,
                        uint64          *partSize
                       )
{
  Errors      error;
  ChunkHeader chunkHeader;
  bool        foundFileEntry,foundFileData;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo   = archiveInfo;
  archiveFileInfo->mode          = FILE_MODE_READ;

  archiveFileInfo->bufferLength  = 0;

  /* allocate buffers */
  archiveFileInfo->buffer = malloc(BUFFER_SIZE*2);
  if (archiveFileInfo->buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  archiveFileInfo->compressBuffer = malloc(BUFFER_SIZE*2);
  if (archiveFileInfo->compressBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  archiveFileInfo->cryptBuffer = malloc(BUFFER_SIZE*2);
  if (archiveFileInfo->cryptBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init file chunk */
  if (!Chunks_init(&archiveFileInfo->chunkInfoFile,
                   NULL,
                   archiveInfo,
                   CHUNK_ID_FILE,
                   CHUNK_DEFINITION_FILE,
                   0
                  )
     )
  {
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return ERROR_IO_ERROR;
  }

  /* find file chunk */
  do
  {
    if (Chunks_eof(archiveInfo))
    {
      Chunks_done(&archiveFileInfo->chunkInfoFile);
      free(archiveFileInfo->cryptBuffer);
      free(archiveFileInfo->compressBuffer);
      free(archiveFileInfo->buffer);
      return ERROR_END_OF_ARCHIVE;
    }

    if (!Chunks_next(archiveInfo,&chunkHeader))
    {
      Chunks_done(&archiveFileInfo->chunkInfoFile);
      free(archiveFileInfo->cryptBuffer);
      free(archiveFileInfo->compressBuffer);
      free(archiveFileInfo->buffer);
      return ERROR_IO_ERROR;
    }

    if (chunkHeader.id != CHUNK_ID_FILE)
    {
      Chunks_skip(archiveInfo,&chunkHeader);
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_FILE);

  /* read file chunk, find file data */
  if (!Chunks_open(&archiveFileInfo->chunkInfoFile,
                   &chunkHeader,
                   &archiveFileInfo->chunkFile
                  )
     )
  {
    Chunks_done(&archiveFileInfo->chunkInfoFile);
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return ERROR_IO_ERROR;
  }

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->chunkFile.cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunks_done(&archiveFileInfo->chunkInfoFile);
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return error;
  }

  /* init compress info */
  error = Compress_new(&archiveFileInfo->compressInfo,
                       archiveFileInfo->blockLength
                      );
  if (error != ERROR_NONE)
  {
    Chunks_done(&archiveFileInfo->chunkInfoFile);
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return ERROR_COMPRESS_ERROR;
  }

  /* init crypt */
  error = Crypt_new(&archiveFileInfo->cryptInfoFileEntry,
                    archiveFileInfo->chunkFile.cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Compress_delete(&archiveFileInfo->compressInfo);
    Chunks_done(&archiveFileInfo->chunkInfoFile);
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return error;
  }
  error = Crypt_new(&archiveFileInfo->cryptInfoFileData,
                    archiveFileInfo->chunkFile.cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    Compress_delete(&archiveFileInfo->compressInfo);
    Chunks_done(&archiveFileInfo->chunkInfoFile);
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return error;
  }

  /* init file entry/data chunks */
  if (!Chunks_init(&archiveFileInfo->chunkInfoFileEntry,
                   &archiveFileInfo->chunkInfoFile,
                   archiveInfo,
                   CHUNK_ID_FILE_ENTRY,
                   CHUNK_DEFINITION_FILE_ENTRY,
                   archiveFileInfo->blockLength
                  )
     )
  {
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    Compress_delete(&archiveFileInfo->compressInfo);
    Chunks_done(&archiveFileInfo->chunkInfoFile);
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return ERROR_IO_ERROR;
  }
  if (!Chunks_init(&archiveFileInfo->chunkInfoFileData,
                   &archiveFileInfo->chunkInfoFile,
                   archiveInfo,
                   CHUNK_ID_FILE_DATA,
                   CHUNK_DEFINITION_FILE_DATA,
                   archiveFileInfo->blockLength
                  )
     )
  {
    Chunks_done(&archiveFileInfo->chunkInfoFileEntry);
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    Compress_delete(&archiveFileInfo->compressInfo);
    Chunks_done(&archiveFileInfo->chunkInfoFile);
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return ERROR_IO_ERROR;
  }

  /* read file entry/data chunks */
  foundFileEntry = FALSE;
  foundFileData  = FALSE;
  while (   !Chunks_eofSub(&archiveFileInfo->chunkInfoFile)
         && (!foundFileEntry || !foundFileData)         
        )
  {
    if (!Chunks_nextSub(&archiveFileInfo->chunkInfoFile,&chunkHeader))
    {
      Chunks_done(&archiveFileInfo->chunkInfoFileData);
      Chunks_done(&archiveFileInfo->chunkInfoFileEntry);
      Crypt_delete(&archiveFileInfo->cryptInfoFileData);
      Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
      Compress_delete(&archiveFileInfo->compressInfo);
      Chunks_done(&archiveFileInfo->chunkInfoFile);
      free(archiveFileInfo->cryptBuffer);
      free(archiveFileInfo->compressBuffer);
      free(archiveFileInfo->buffer);
      return ERROR_IO_ERROR;
    }

    switch (chunkHeader.id)
    {
      case CHUNK_ID_FILE_ENTRY:
        if (!Chunks_open(&archiveFileInfo->chunkInfoFileEntry,
                         &chunkHeader,
                         &archiveFileInfo->chunkFileEntry
                        )
           )
        {
          Chunks_done(&archiveFileInfo->chunkInfoFileData);
          Chunks_done(&archiveFileInfo->chunkInfoFileEntry);
          Crypt_delete(&archiveFileInfo->cryptInfoFileData);
          Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
          Compress_delete(&archiveFileInfo->compressInfo);
          Chunks_done(&archiveFileInfo->chunkInfoFile);
          free(archiveFileInfo->cryptBuffer);
          free(archiveFileInfo->compressBuffer);
          free(archiveFileInfo->buffer);
          return ERROR_IO_ERROR;
        }

        String_set(fileName,archiveFileInfo->chunkFileEntry.name);
        fileInfo->size            = archiveFileInfo->chunkFileEntry.size;
        fileInfo->timeLastAccess  = archiveFileInfo->chunkFileEntry.timeLastAccess;
        fileInfo->timeModified    = archiveFileInfo->chunkFileEntry.timeModified;
        fileInfo->timeLastChanged = archiveFileInfo->chunkFileEntry.timeLastChanged;
        fileInfo->userId          = archiveFileInfo->chunkFileEntry.userId;
        fileInfo->groupId         = archiveFileInfo->chunkFileEntry.groupId;
        fileInfo->permission      = archiveFileInfo->chunkFileEntry.permission;

        foundFileEntry = TRUE;
        break;
      case CHUNK_ID_FILE_DATA:
        if (!Chunks_open(&archiveFileInfo->chunkInfoFileData,
                         &chunkHeader,
                         &archiveFileInfo->chunkFileData
                        )
           )
        {
          if (foundFileEntry) String_delete(archiveFileInfo->chunkFileEntry.name);
          Chunks_done(&archiveFileInfo->chunkInfoFileData);
          Chunks_done(&archiveFileInfo->chunkInfoFileEntry);
          Crypt_delete(&archiveFileInfo->cryptInfoFileData);
          Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
          Compress_delete(&archiveFileInfo->compressInfo);
          Chunks_done(&archiveFileInfo->chunkInfoFile);
          free(archiveFileInfo->cryptBuffer);
          free(archiveFileInfo->compressBuffer);
          free(archiveFileInfo->buffer);
          return ERROR_IO_ERROR;
        }

        if (partOffset != NULL) (*partOffset) = archiveFileInfo->chunkFileData.partOffset;
        if (partSize   != NULL) (*partSize)   = archiveFileInfo->chunkFileData.partSize;

        foundFileData = TRUE;
        break;
      default:
        Chunks_skipSub(&archiveFileInfo->chunkInfoFile,&chunkHeader);
        break;
    }
  }
  if (!foundFileEntry)
  {
    if (foundFileEntry) String_delete(archiveFileInfo->chunkFileEntry.name);
    Chunks_done(&archiveFileInfo->chunkInfoFileData);
    Chunks_done(&archiveFileInfo->chunkInfoFileEntry);
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    Compress_delete(&archiveFileInfo->compressInfo);
    Chunks_done(&archiveFileInfo->chunkInfoFile);
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return ERROR_NO_FILE_ENTRY;
  }
  if (!foundFileData)
  {
    if (foundFileEntry) String_delete(archiveFileInfo->chunkFileEntry.name);
    Chunks_done(&archiveFileInfo->chunkInfoFileData);
    Chunks_done(&archiveFileInfo->chunkInfoFileEntry);
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    Compress_delete(&archiveFileInfo->compressInfo);
    Chunks_done(&archiveFileInfo->chunkInfoFile);
    free(archiveFileInfo->cryptBuffer);
    free(archiveFileInfo->compressBuffer);
    free(archiveFileInfo->buffer);
    return ERROR_NO_FILE_DATA;
  }

  return ERROR_NONE;
}

Errors Archive_closeFile(ArchiveFileInfo *archiveFileInfo)
{
  Errors error;

  assert(archiveFileInfo != NULL);
  assert(archiveFileInfo->archiveInfo != NULL);

  switch (archiveFileInfo->mode)
  {
    case FILE_MODE_WRITE:
      {
        /* flush last block */
        if (!Compress_checkBlockIsEmpty(&archiveFileInfo->compressInfo))
        {
          error = writeBlock(archiveFileInfo);
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        /* update part offset, part size */
        if (!Chunks_update(&archiveFileInfo->chunkInfoFileData,
                           &archiveFileInfo->chunkFileData
                          )
           )
        {
          return ERROR_IO_ERROR;
        }
      }
      break;
    case FILE_MODE_READ:
      break;
  }

  /* close chunks */
  if (!Chunks_close(&archiveFileInfo->chunkInfoFileData))
  {
    return ERROR_IO_ERROR;
  }
  if (!Chunks_close(&archiveFileInfo->chunkInfoFileEntry))
  {
    return ERROR_IO_ERROR;
  }
  if (!Chunks_close(&archiveFileInfo->chunkInfoFile))
  {
    return ERROR_IO_ERROR;
  }
//  archiveFileInfo->archiveInfo->index               = archiveFileInfo->archiveInfo->chunkInfoData.index;
  archiveFileInfo->headerWrittenFlag = FALSE;

  /* free resources */
  Chunks_done(&archiveFileInfo->chunkInfoFileData);
  Chunks_done(&archiveFileInfo->chunkInfoFileEntry);
  Chunks_done(&archiveFileInfo->chunkInfoFile);
  Crypt_delete(&archiveFileInfo->cryptInfoFileData);
  Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
  Compress_delete(&archiveFileInfo->compressInfo);
  free(archiveFileInfo->compressBuffer);
  free(archiveFileInfo->cryptBuffer);
  free(archiveFileInfo->buffer);
  switch (archiveFileInfo->mode)
  {
    case FILE_MODE_WRITE:
      String_delete(archiveFileInfo->chunkFileEntry.name);
      break;
    case FILE_MODE_READ:
      break;
  }

  return ERROR_NONE;
}

Errors Archive_writeFileData(ArchiveFileInfo *archiveFileInfo, const void *buffer, ulong bufferLength)
{
  byte       *p;
  ulong      length;
  uint64     size;
  bool       newPartFlag;
//  ulong      freeBlocks;
//  ulong      partLength;
  ulong      n;
  String     fileName;
  Errors     error;
//  ulong      compressLength;
  ulong      cryptLength;
  void       *dataBuffer;
  ulong      dataLength;

  assert(archiveFileInfo != NULL);
  assert(archiveFileInfo->archiveInfo != NULL);
  assert((archiveFileInfo->bufferLength%archiveFileInfo->blockLength) == 0);

  p      = (byte*)buffer;
  length = 0;
  while (length < bufferLength)
  {
    /* compress */
    Compress_deflate(&archiveFileInfo->compressInfo,*p);

    /* check if block can be encrypted and written */
    if (Compress_checkBlockIsFull(&archiveFileInfo->compressInfo))
    {
      error = writeBlock(archiveFileInfo);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
    assert(!Compress_checkBlockIsFull(&archiveFileInfo->compressInfo));

    /* next byte */
    p++;
    length++;
  }

  archiveFileInfo->bufferLength = 0;

  return ERROR_NONE;

#if 0
  ulong  length;
  ulong  n;
  Errors error;

  assert(archiveFileInfo != NULL);

  length = 0;
  while (length < bufferLength)
  {
    /* copy data to output buffer */
    n=MIN(bufferLength-length,BUFFER_SIZE-archiveFileInfo->bufferLength);
    memcpy((char*)archiveFileInfo->buffer+archiveFileInfo->bufferLength,
           (char*)buffer+length,
           n
          );
    archiveFileInfo->bufferLength += n;

    /* flush buffer if full */
    if (archiveFileInfo->bufferLength >= BUFFER_SIZE)
    {
      error = flushBuffer(archiveFileInfo);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    length += n;
  }

  return ERROR_NONE;
#endif /* 0 */
}

Errors Archive_readFileData(ArchiveFileInfo *archiveFileInfo, void *buffer, ulong bufferLength)
{
  byte   *p;
  ulong  length;
  Errors error;

  assert(archiveFileInfo != NULL);

  p      = (byte*)buffer;
  length = 0;
  while (length < bufferLength)
  {
    if (Compress_checkEndOfBlock(&archiveFileInfo->compressInfo))
    {
      /* read */
      if (!Chunks_readData(&archiveFileInfo->chunkInfoFileData,
                           archiveFileInfo->buffer,
                           archiveFileInfo->blockLength
                          )
         )
      {
        return ERROR_IO_ERROR;
      }

      /* decrypt */
  memset(archiveFileInfo->compressBuffer,0,BUFFER_SIZE);
      error = Crypt_decrypt(&archiveFileInfo->cryptInfoFileData,
                            archiveFileInfo->buffer,
                            archiveFileInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      Compress_putBlock(&archiveFileInfo->compressInfo,
                        archiveFileInfo->buffer,
                        archiveFileInfo->blockLength
                       );
    }
    assert(!Compress_checkEndOfBlock(&archiveFileInfo->compressInfo));

    /* decompress data */
    Compress_inflate(&archiveFileInfo->compressInfo,p);

    /* next byte */
    p++;
    length++;
  }

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
