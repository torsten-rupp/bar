/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/archive.c,v $
* $Revision: 1.11 $
* $Author: torsten $
* Contents: Backup ARchiver archive functions
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "lists.h"

#include "errors.h"
#include "archive_format.h"
#include "chunks.h"
#include "files.h"
#include "compress.h"
#include "crypt.h"

#include "archive.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define MAX_BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : endOfFile
* Purpose: check if end-of-file
* Input  : userData - archive-info
* Output : -
* Return : TRUE if end-of-file, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool endOfFile(void *userData)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;

  assert(archiveInfo != NULL);

  return Files_eof(&archiveInfo->fileHandle);
}

/***********************************************************************\
* Name   : readFile
* Purpose: read data from file
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
* Name   : writeFile
* Purpose: write data to file
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
* Name   : tellFile
* Purpose: get position in file
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
* Name   : seekFile
* Purpose: seek in file
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

/***********************************************************************\
* Name   : writeBlock
* Purpose: write block to file: encrypt and split file
* Input  : archiveFileInfo - archive file info
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors writeBlock(ArchiveFileInfo *archiveFileInfo)
{
  ulong  length;
  uint64 fileSize;
  bool   newPartFlag;
//  ulong  n;
//  ulong  freeBlocks;
  String fileName;
  Errors error;

  assert(archiveFileInfo != NULL);

  /* check if split necessary */
  newPartFlag = FALSE;
  if (archiveFileInfo->archiveInfo->partSize > 0)
  {
    /* check if new file-part is needed */
    if (archiveFileInfo->archiveInfo->fileHandle.handle >= 0)
    {
      /* get total file size */
      fileSize = archiveFileInfo->chunkInfoFileData.offset+CHUNK_HEADER_SIZE+archiveFileInfo->chunkInfoFileData.size;

      if      (   !archiveFileInfo->headerWrittenFlag
               && (fileSize+archiveFileInfo->headerLength >= archiveFileInfo->archiveInfo->partSize)
              )
      {
        /* file header cannot be written without fragmentation -> new part */
        newPartFlag = TRUE;
      }
      else if ((fileSize+archiveFileInfo->blockLength) >= archiveFileInfo->archiveInfo->partSize)
      {
        /* less than one block left in part -> new part */
        newPartFlag = TRUE;
      }
    }
  }

  /* split */
  if (newPartFlag)
  {
    /* create chunk-headers */
    if (!archiveFileInfo->headerWrittenFlag)
    {
      if (!Chunks_create(&archiveFileInfo->chunkInfoFile,
                         CHUNK_ID_FILE,
                         CHUNK_DEFINITION_FILE,
                         Chunks_getSize(CHUNK_DEFINITION_FILE,0,&archiveFileInfo->chunkFile),
                         &archiveFileInfo->chunkFile
                        )
         )
      {
        return ERROR_IO_ERROR;
      }

      if (!Chunks_create(&archiveFileInfo->chunkInfoFileEntry,
                         CHUNK_ID_FILE_ENTRY,
                         CHUNK_DEFINITION_FILE_ENTRY,
                         Chunks_getSize(CHUNK_DEFINITION_FILE_ENTRY,archiveFileInfo->blockLength,&archiveFileInfo->chunkFileEntry),
                         &archiveFileInfo->chunkFileEntry
                        )
         )
      {
        return ERROR_IO_ERROR;
      }

      if (!Chunks_create(&archiveFileInfo->chunkInfoFileData,
                         CHUNK_ID_FILE_DATA,
                         CHUNK_DEFINITION_FILE_DATA,
                         Chunks_getSize(CHUNK_DEFINITION_FILE_DATA,archiveFileInfo->blockLength,&archiveFileInfo->chunkFileData),
                         &archiveFileInfo->chunkFileData
                        )
         )
      {
        return ERROR_IO_ERROR;
      }

      archiveFileInfo->headerWrittenFlag = TRUE;
    }

    /* get compressed block */
    Compress_getBlock(&archiveFileInfo->compressInfoData,
                      archiveFileInfo->buffer,
                      &length
                     );         

    /* encrypt block */
    error = Crypt_encrypt(&archiveFileInfo->cryptInfoData,
                          archiveFileInfo->buffer,
                          archiveFileInfo->blockLength
                         );
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* write block */
    if (!Chunks_writeData(&archiveFileInfo->chunkInfoFileData,
                          archiveFileInfo->buffer,
                          archiveFileInfo->blockLength
                         )
       )
    {
      return ERROR_IO_ERROR;
    }

    /* flush compress buffer */
    Compress_flush(&archiveFileInfo->compressInfoData);
    while (!Compress_checkBlockIsEmpty(&archiveFileInfo->compressInfoData))
    {
      /* get compressed block */
      Compress_getBlock(&archiveFileInfo->compressInfoData,
                        archiveFileInfo->buffer,
                        &length
                       );         

      /* encrypt block */
      error = Crypt_encrypt(&archiveFileInfo->cryptInfoData,
                            archiveFileInfo->buffer,
                            archiveFileInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      /* write */
      if (!Chunks_writeData(&archiveFileInfo->chunkInfoFileData,
                            archiveFileInfo->buffer,
                            archiveFileInfo->blockLength
                           )
         )
      {
        return ERROR_IO_ERROR;
      }
    }

    /* update part size */
    archiveFileInfo->chunkFileData.partSize = Compress_getInputLength(&archiveFileInfo->compressInfoData);
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

    /* close file */
    Files_close(&archiveFileInfo->archiveInfo->fileHandle);
    archiveFileInfo->archiveInfo->fileHandle.handle = -1;

    /* reset compress (do it here because data if buffered and can be processed before a new file is opened) */
    Compress_reset(&archiveFileInfo->compressInfoData);
  }
  else
  {
    /* get size of space to reserve for chunk-header */
    if (!archiveFileInfo->headerWrittenFlag || newPartFlag)
    {
//      n = archiveFileInfo->headerLength;
    }
    else
    {
//      n = 0;
    }

    /* calculate max. length of data which can be written into this part */
//        freeBlocks = (archiveFileInfo->archiveInfo->partSize-(size+n))/archiveFileInfo->blockLength;

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

      /* free resources */
      String_delete(fileName);

      /* initialise variables */
      archiveFileInfo->headerWrittenFlag = FALSE;

      archiveFileInfo->chunkFileData.partOffset = archiveFileInfo->chunkFileData.partOffset+archiveFileInfo->chunkFileData.partSize;
      archiveFileInfo->chunkFileData.partSize   = 0;
  
      /* reset data crypt */
      Crypt_reset(&archiveFileInfo->cryptInfoData,0);
    }

    /* create chunk-headers */
    if (!archiveFileInfo->headerWrittenFlag)
    {
      if (!Chunks_create(&archiveFileInfo->chunkInfoFile,
                         CHUNK_ID_FILE,
                         CHUNK_DEFINITION_FILE,
                         Chunks_getSize(CHUNK_DEFINITION_FILE,0,&archiveFileInfo->chunkFile),
                         &archiveFileInfo->chunkFile
                        )
         )
      {
        return ERROR_IO_ERROR;
      }

      if (!Chunks_create(&archiveFileInfo->chunkInfoFileEntry,
                         CHUNK_ID_FILE_ENTRY,
                         CHUNK_DEFINITION_FILE_ENTRY,
                         Chunks_getSize(CHUNK_DEFINITION_FILE_ENTRY,archiveFileInfo->blockLength,&archiveFileInfo->chunkFileEntry),
                         &archiveFileInfo->chunkFileEntry
                        )
         )
      {
        return ERROR_IO_ERROR;
      }

      if (!Chunks_create(&archiveFileInfo->chunkInfoFileData,
                         CHUNK_ID_FILE_DATA,
                         CHUNK_DEFINITION_FILE_DATA,
                         Chunks_getSize(CHUNK_DEFINITION_FILE_DATA,archiveFileInfo->blockLength,&archiveFileInfo->chunkFileData),
                         &archiveFileInfo->chunkFileData
                        )
         )
      {
        return ERROR_IO_ERROR;
      }

      archiveFileInfo->headerWrittenFlag = TRUE;
    }

    /* get compressed block */
    Compress_getBlock(&archiveFileInfo->compressInfoData,
                      archiveFileInfo->buffer,
                      &length
                     );         

    /* encrypt block */
    error = Crypt_encrypt(&archiveFileInfo->cryptInfoData,
                          archiveFileInfo->buffer,
                          archiveFileInfo->blockLength
                         );
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* write block */
    if (!Chunks_writeData(&archiveFileInfo->chunkInfoFileData,
                          archiveFileInfo->buffer,
                          archiveFileInfo->blockLength
                         )
       )
    {
      return ERROR_IO_ERROR;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : readBlock
* Purpose: read block: decrypt
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors readBlock(ArchiveFileInfo *archiveFileInfo)
{
  Errors error;

  assert(archiveFileInfo != NULL);

  if (!Chunks_eofSub(&archiveFileInfo->chunkInfoFileData))
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
    error = Crypt_decrypt(&archiveFileInfo->cryptInfoData,
                          archiveFileInfo->buffer,
                          archiveFileInfo->blockLength
                         );
    if (error != ERROR_NONE)
    {
      return error;
    }

    Compress_putBlock(&archiveFileInfo->compressInfoData,
                      archiveFileInfo->buffer,
                      archiveFileInfo->blockLength
                     );
  }
  else
  {
    Compress_flush(&archiveFileInfo->compressInfoData);
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Archive_create(ArchiveInfo        *archiveInfo,
                      const char         *archiveFileName,
                      uint64             partSize,
                      CompressAlgorithms compressAlgorithm,
                      CryptAlgorithms    cryptAlgorithm,
                      const char         *password
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
  archiveInfo->compressAlgorithm = compressAlgorithm;
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
  archiveInfo->compressAlgorithm = 0;
  archiveInfo->cryptAlgorithm    = CRYPT_ALGORITHM_UNKNOWN;
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

  archiveFileInfo->chunkFile.compressAlgorithm    = archiveInfo->compressAlgorithm;
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

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    return error;
  }
  assert(archiveFileInfo->blockLength > 0);

  /* init file-chunk */
  if (!Chunks_new(&archiveFileInfo->chunkInfoFile,
                  NULL,
                  archiveInfo,
                  0,
                  NULL
                 )
     )
  {
    return ERROR_IO_ERROR;
  }

  /* allocate buffer */
  if (archiveFileInfo->blockLength > MAX_BUFFER_SIZE)
  {
    return ERROR_UNSUPPORTED_BLOCK_SIZE;
  }
  archiveFileInfo->bufferLength = (MAX_BUFFER_SIZE/archiveFileInfo->blockLength)*archiveFileInfo->blockLength;
  archiveFileInfo->buffer = (byte*)malloc(archiveFileInfo->bufferLength);
  if (archiveFileInfo->buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init crypt */
  error = Crypt_new(&archiveFileInfo->cryptInfoFileEntry,
                    archiveInfo->cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    free(archiveFileInfo->buffer);
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
    return error;
  }
  error = Crypt_new(&archiveFileInfo->cryptInfoFileData,
                    archiveInfo->cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    free(archiveFileInfo->buffer);
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
    return error;
  }
  error = Crypt_new(&archiveFileInfo->cryptInfoData,
                    archiveInfo->cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    free(archiveFileInfo->buffer);
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
    return error;
  }

  /* init compress info */
  error = Compress_new(&archiveFileInfo->compressInfoData,
                       COMPRESS_MODE_DEFLATE,
                       archiveFileInfo->archiveInfo->compressAlgorithm,
                       archiveFileInfo->blockLength
                      );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->cryptInfoData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    free(archiveFileInfo->buffer);
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
    return ERROR_COMPRESS_ERROR;
  }

  /* init entry/dat file-chunks */
  if (!Chunks_new(&archiveFileInfo->chunkInfoFileEntry,
                  &archiveFileInfo->chunkInfoFile,
                  archiveInfo,
                  archiveFileInfo->blockLength,
                  &archiveFileInfo->cryptInfoFileEntry
                 )
     )
  {
    Compress_delete(&archiveFileInfo->compressInfoData);
    Crypt_delete(&archiveFileInfo->cryptInfoData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    free(archiveFileInfo->buffer);
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
    return ERROR_IO_ERROR;
  }
  if (!Chunks_new(&archiveFileInfo->chunkInfoFileData,
                  &archiveFileInfo->chunkInfoFile,
                  archiveInfo,
                  archiveFileInfo->blockLength,
                  &archiveFileInfo->cryptInfoFileData
                 )
     )
  {
    Chunks_delete(&archiveFileInfo->chunkInfoFileEntry);
    Compress_delete(&archiveFileInfo->compressInfoData);
    Crypt_delete(&archiveFileInfo->cryptInfoData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    free(archiveFileInfo->buffer);
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
    return ERROR_IO_ERROR;
  }

  /* calculate header length */
  archiveFileInfo->headerLength = Chunks_getSize(CHUNK_DEFINITION_FILE,      0,                           &archiveFileInfo->chunkFile     )+
                                  Chunks_getSize(CHUNK_DEFINITION_FILE_ENTRY,archiveFileInfo->blockLength,&archiveFileInfo->chunkFileEntry)+
                                  Chunks_getSize(CHUNK_DEFINITION_FILE_DATA, archiveFileInfo->blockLength,&archiveFileInfo->chunkFileData );

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

  /* init file chunk */
  if (!Chunks_new(&archiveFileInfo->chunkInfoFile,
                  NULL,
                  archiveInfo,
                  0,
                  NULL
                 )
     )
  {
    return ERROR_IO_ERROR;
  }

  /* find file chunk */
  do
  {
    if (Chunks_eof(archiveInfo))
    {
      Chunks_delete(&archiveFileInfo->chunkInfoFile);
      return ERROR_END_OF_ARCHIVE;
    }

    if (!Chunks_next(archiveInfo,&chunkHeader))
    {
      Chunks_delete(&archiveFileInfo->chunkInfoFile);
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
                   CHUNK_ID_FILE,
                   CHUNK_DEFINITION_FILE,
                   Chunks_getSize(CHUNK_DEFINITION_FILE,0,NULL),
                   &archiveFileInfo->chunkFile
                  )
     )
  {
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
    return ERROR_IO_ERROR;
  }
  fileInfo->compressAlgorithm = archiveFileInfo->chunkFile.compressAlgorithm;
  fileInfo->cryptAlgorithm    = archiveFileInfo->chunkFile.cryptAlgorithm;

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->chunkFile.cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
    return error;
  }
  assert(archiveFileInfo->blockLength > 0);

  /* allocate buffer */
  archiveFileInfo->bufferLength = (MAX_BUFFER_SIZE/archiveFileInfo->blockLength)*archiveFileInfo->blockLength;
  archiveFileInfo->buffer = (byte*)malloc(archiveFileInfo->bufferLength);
  if (archiveFileInfo->buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init crypt */
  error = Crypt_new(&archiveFileInfo->cryptInfoFileEntry,
                    archiveFileInfo->chunkFile.cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    free(archiveFileInfo->buffer);
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
    return error;
  }
  error = Crypt_new(&archiveFileInfo->cryptInfoFileData,
                    archiveFileInfo->chunkFile.cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    free(archiveFileInfo->buffer);
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
    return error;
  }
  error = Crypt_new(&archiveFileInfo->cryptInfoData,
                    archiveFileInfo->chunkFile.cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    free(archiveFileInfo->buffer);
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
    return error;
  }

  /* init compress info */
  error = Compress_new(&archiveFileInfo->compressInfoData,
                       COMPRESS_MODE_INFLATE,
                       archiveFileInfo->chunkFile.compressAlgorithm,
                       archiveFileInfo->blockLength
                      );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->cryptInfoData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    free(archiveFileInfo->buffer);
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
    return ERROR_COMPRESS_ERROR;
  }

  /* init file entry/data chunks */
  if (!Chunks_new(&archiveFileInfo->chunkInfoFileEntry,
                  &archiveFileInfo->chunkInfoFile,
                  archiveInfo,
                  archiveFileInfo->blockLength,
                  &archiveFileInfo->cryptInfoFileEntry
                 )
     )
  {
    Compress_delete(&archiveFileInfo->compressInfoData);
    Crypt_delete(&archiveFileInfo->cryptInfoData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    free(archiveFileInfo->buffer);
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
    return ERROR_IO_ERROR;
  }
  if (!Chunks_new(&archiveFileInfo->chunkInfoFileData,
                  &archiveFileInfo->chunkInfoFile,
                  archiveInfo,
                  archiveFileInfo->blockLength,
                  &archiveFileInfo->cryptInfoFileData
                 )
     )
  {
    Chunks_delete(&archiveFileInfo->chunkInfoFileEntry);
    Compress_delete(&archiveFileInfo->compressInfoData);
    Crypt_delete(&archiveFileInfo->cryptInfoData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    free(archiveFileInfo->buffer);
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
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
      Chunks_delete(&archiveFileInfo->chunkInfoFileData);
      Chunks_delete(&archiveFileInfo->chunkInfoFileEntry);
      Compress_delete(&archiveFileInfo->compressInfoData);
      Crypt_delete(&archiveFileInfo->cryptInfoData);
      Crypt_delete(&archiveFileInfo->cryptInfoFileData);
      Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
      free(archiveFileInfo->buffer);
      Chunks_delete(&archiveFileInfo->chunkInfoFile);
      return ERROR_IO_ERROR;
    }

    switch (chunkHeader.id)
    {
      case CHUNK_ID_FILE_ENTRY:
        if (!Chunks_open(&archiveFileInfo->chunkInfoFileEntry,
                         &chunkHeader,
                         CHUNK_ID_FILE_ENTRY,
                         CHUNK_DEFINITION_FILE_ENTRY,
                         chunkHeader.size,
                         &archiveFileInfo->chunkFileEntry
                        )
           )
        {
          Chunks_delete(&archiveFileInfo->chunkInfoFileData);
          Chunks_delete(&archiveFileInfo->chunkInfoFileEntry);
          Compress_delete(&archiveFileInfo->compressInfoData);
          Crypt_delete(&archiveFileInfo->cryptInfoData);
          Crypt_delete(&archiveFileInfo->cryptInfoFileData);
          Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
          free(archiveFileInfo->buffer);
          Chunks_delete(&archiveFileInfo->chunkInfoFile);
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
                         CHUNK_ID_FILE_DATA,
                         CHUNK_DEFINITION_FILE_DATA,
                         Chunks_getSize(CHUNK_DEFINITION_FILE_DATA,archiveFileInfo->blockLength,NULL),
                         &archiveFileInfo->chunkFileData
                        )
           )
        {
          if (foundFileEntry) String_delete(archiveFileInfo->chunkFileEntry.name);
          Chunks_delete(&archiveFileInfo->chunkInfoFileData);
          Chunks_delete(&archiveFileInfo->chunkInfoFileEntry);
          Compress_delete(&archiveFileInfo->compressInfoData);
          Crypt_delete(&archiveFileInfo->cryptInfoData);
          Crypt_delete(&archiveFileInfo->cryptInfoFileData);
          Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
          free(archiveFileInfo->buffer);
          Chunks_delete(&archiveFileInfo->chunkInfoFile);
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
    Chunks_delete(&archiveFileInfo->chunkInfoFileData);
    Chunks_delete(&archiveFileInfo->chunkInfoFileEntry);
    Compress_delete(&archiveFileInfo->compressInfoData);
    Crypt_delete(&archiveFileInfo->cryptInfoData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    free(archiveFileInfo->buffer);
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
    return ERROR_NO_FILE_ENTRY;
  }
  if (!foundFileData)
  {
    if (foundFileEntry) String_delete(archiveFileInfo->chunkFileEntry.name);
    Chunks_delete(&archiveFileInfo->chunkInfoFileData);
    Chunks_delete(&archiveFileInfo->chunkInfoFileEntry);
    Compress_delete(&archiveFileInfo->compressInfoData);
    Crypt_delete(&archiveFileInfo->cryptInfoData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
    free(archiveFileInfo->buffer);
    Chunks_delete(&archiveFileInfo->chunkInfoFile);
    return ERROR_NO_FILE_DATA;
  }

      /* reset compress, crypt */
      Compress_reset(&archiveFileInfo->compressInfoData);
      Crypt_reset(&archiveFileInfo->cryptInfoFileData,0);

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
        /* flush last blocks */
        Compress_flush(&archiveFileInfo->compressInfoData);
        while (!Compress_checkBlockIsEmpty(&archiveFileInfo->compressInfoData))
        {
          error = writeBlock(archiveFileInfo);
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        /* update part size */
        archiveFileInfo->chunkFileData.partSize = Compress_getInputLength(&archiveFileInfo->compressInfoData);
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
  archiveFileInfo->headerWrittenFlag = FALSE;

  /* free resources */
  Chunks_delete(&archiveFileInfo->chunkInfoFileData);
  Chunks_delete(&archiveFileInfo->chunkInfoFileEntry);
  Compress_delete(&archiveFileInfo->compressInfoData);
  Crypt_delete(&archiveFileInfo->cryptInfoData);
  Crypt_delete(&archiveFileInfo->cryptInfoFileData);
  Crypt_delete(&archiveFileInfo->cryptInfoFileEntry);
  Chunks_delete(&archiveFileInfo->chunkInfoFile);
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
  byte   *p;
  ulong  length;
  Errors error;

  assert(archiveFileInfo != NULL);
  assert(archiveFileInfo->archiveInfo != NULL);
  assert((archiveFileInfo->bufferLength%archiveFileInfo->blockLength) == 0);

  p      = (byte*)buffer;
  length = 0;
  while (length < bufferLength)
  {
    /* compress */
    Compress_deflate(&archiveFileInfo->compressInfoData,*p);

    /* check if block can be encrypted and written */
    while (Compress_checkBlockIsFull(&archiveFileInfo->compressInfoData))
    {
      error = writeBlock(archiveFileInfo);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
    assert(!Compress_checkBlockIsFull(&archiveFileInfo->compressInfoData));

    /* next byte */
    p++;
    length++;
  }

  archiveFileInfo->bufferLength = 0;

  return ERROR_NONE;
}

Errors Archive_readFileData(ArchiveFileInfo *archiveFileInfo, void *buffer, ulong bufferLength)
{
  byte   *p;
  ulong  length;
  Errors error;
  ulong  n;

  assert(archiveFileInfo != NULL);

  p      = (byte*)buffer;
  length = 0;
  while (length < bufferLength)
  {
    /* fill decompressor with compressed data blocks */
    do
    {
      error = Compress_available(&archiveFileInfo->compressInfoData,&n);
      if (error != ERROR_NONE)
      {
        return error;
      }

      if (n <= 0)
      {
        error = readBlock(archiveFileInfo);
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
    }
    while (n <= 0);

    /* decompress data */
    Compress_inflate(&archiveFileInfo->compressInfoData,p);

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
