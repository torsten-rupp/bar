/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/archive.c,v $
* $Revision: 1.29 $
* $Author: torsten $
* Contents: Backup ARchiver archive functions
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

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
#include "storage.h"
#include "compress.h"
#include "passwords.h"
#include "crypt.h"

#include "archive.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define MAX_BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/

typedef enum
{
  BLOCK_MODE_WRITE,
  BLOCK_MODE_FLUSH,
} BlockModes;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : getNextChunkHeader
* Purpose: read next chunk header
* Input  : archiveInfo - archive info block
* Output : chunkHeader - read chunk header
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors getNextChunkHeader(ArchiveInfo *archiveInfo, ChunkHeader *chunkHeader)
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(chunkHeader != NULL);

  if (!archiveInfo->nextChunkHeaderReadFlag)
  {
    if (Chunk_eof(&archiveInfo->storageFileHandle))
    {
      return ERROR_END_OF_ARCHIVE;
    }

    error = Chunk_next(&archiveInfo->storageFileHandle,chunkHeader);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  else
  {
    (*chunkHeader) = archiveInfo->nextChunkHeader;
    archiveInfo->nextChunkHeaderReadFlag = FALSE;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : ungetNextChunkHeader
* Purpose: restore chunk header for next read
* Input  : archiveInfo - archive info block
*          chunkHeader - read chunk header
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void ungetNextChunkHeader(ArchiveInfo *archiveInfo, ChunkHeader *chunkHeader)
{
  assert(archiveInfo != NULL);
  assert(chunkHeader != NULL);

  archiveInfo->nextChunkHeaderReadFlag = TRUE;
  archiveInfo->nextChunkHeader         = (*chunkHeader);
}

/***********************************************************************\
* Name   : checkNewPartNeeded
* Purpose: check if new file part should be created
* Input  : archiveInfo - archive info block
*          headerLength      - length of header data to write
*          headerWrittenFlag - TRUE iff header already written
*          minBytes          - additional space needed in archive file
* Output : -
* Return : TRUE if new file part should be created, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool checkNewPartNeeded(ArchiveInfo *archiveInfo,
                              ulong       headerLength,
                              bool        headerWrittenFlag,
                              ulong       minBytes
                             )
{
  bool   newPartFlag;
  uint64 fileSize;

  newPartFlag = FALSE;
  if (archiveInfo->partSize > 0)
  {
    if (archiveInfo->fileOpenFlag)
    {
      /* get file size */
      fileSize = Storage_getSize(&archiveInfo->storageFileHandle);

      if      (   !headerWrittenFlag
               && (fileSize+headerLength >= archiveInfo->partSize)
              )
      {
        /* file header cannot be written without fragmentation -> new part */
        newPartFlag = TRUE;
      }
      else if ((fileSize+minBytes) >= archiveInfo->partSize)
      {
        /* less than min. number of bytes left in part -> new part */
        newPartFlag = TRUE;
      }
    }
  }

  return newPartFlag;
}

/***********************************************************************\
* Name   : openArchiveFile
* Purpose: create and open new archive file
* Input  : archiveInfo - archive info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors openArchiveFile(ArchiveInfo *archiveInfo)
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(!archiveInfo->fileOpenFlag);

  /* get output filename */
  archiveInfo->fileName = String_new();
  if (!File_getTmpFileName(archiveInfo->tmpDirectory,archiveInfo->fileName))
  {
    return ERROR_IO_ERROR;
  }

  /* create file */
  error = Storage_create(&archiveInfo->storageFileHandle,archiveInfo->fileName,0,archiveInfo->options);
  if (error != ERROR_NONE)
  {
    String_delete(archiveInfo->fileName);
    return error;
  }
 
  archiveInfo->fileOpenFlag = TRUE;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : closeArchiveFile
* Purpose: close archive file
* Input  : archiveInfo - archive info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors closeArchiveFile(ArchiveInfo *archiveInfo)
{
  uint64 size;
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveInfo->fileOpenFlag);

  /* close file */
  size = Storage_getSize(&archiveInfo->storageFileHandle);
  Storage_close(&archiveInfo->storageFileHandle);

  if (archiveInfo->archiveNewFileFunction != NULL)
  {
    /* call back */
    error = archiveInfo->archiveNewFileFunction(archiveInfo->fileName,
                                                size,
                                                TRUE,
                                                (archiveInfo->partSize > 0)?archiveInfo->partNumber:-1,
                                                archiveInfo->archiveNewFileUserData
                                               );
    if (error != ERROR_NONE)
    {
      String_delete(archiveInfo->fileName);
      archiveInfo->fileOpenFlag = FALSE;
      return error;
    }
  }
  if (archiveInfo->partSize > 0)
  {
    archiveInfo->partNumber++;
  }

  /* free resources */
  String_delete(archiveInfo->fileName);

  archiveInfo->fileOpenFlag = FALSE;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : ensureArchiveSpace
* Purpose: ensure space is available in archive for not fragmented
*          writting
* Input  : minBytes - minimal number of bytes
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors ensureArchiveSpace(ArchiveInfo *archiveInfo,
                                ulong       minBytes
                               )
{
  Errors error;

  /* check if split necessary */
  if (checkNewPartNeeded(archiveInfo,
                         0,
                         FALSE,
                         minBytes
                        )
     )
  {
    /* split needed -> close file */
    closeArchiveFile(archiveInfo);
  }

  /* open file if needed */
  if (!archiveInfo->fileOpenFlag)
  {
    /* create file */
    error = openArchiveFile(archiveInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeDataBlock
* Purpose: write data block to file: encrypt and split file
* Input  : archiveFileInfo - archive file info block
*          blockModes      - block write mode; see BlockModes
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors writeDataBlock(ArchiveFileInfo *archiveFileInfo, BlockModes blockModes)
{
  ulong  length;
  bool   newPartFlag;
//  ulong  n;
//  ulong  freeBlocks;
  Errors error;

  assert(archiveFileInfo != NULL);

  /* get compressed block */
  Compress_getBlock(&archiveFileInfo->file.compressInfoData,
                    archiveFileInfo->file.buffer,
                    &length
                   );         

  /* check if split necessary */
  newPartFlag = checkNewPartNeeded(archiveFileInfo->archiveInfo,
                                   archiveFileInfo->file.headerLength,
                                   archiveFileInfo->file.headerWrittenFlag,
                                   ((length > 0) || (blockModes == BLOCK_MODE_WRITE))?archiveFileInfo->archiveInfo->blockLength:0
                                  );

  /* split */
  if (newPartFlag)
  {
    /* create chunk-headers */
    if (!archiveFileInfo->file.headerWrittenFlag && (!archiveFileInfo->file.createdFlag || (length > 0)))
    {
      error = Chunk_create(&archiveFileInfo->file.chunkInfoFile,
                           CHUNK_ID_FILE,
                           CHUNK_DEFINITION_FILE,
                           Chunk_getSize(CHUNK_DEFINITION_FILE,0,&archiveFileInfo->file.chunkFile),
                           &archiveFileInfo->file.chunkFile
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = Chunk_create(&archiveFileInfo->file.chunkInfoFileEntry,
                           CHUNK_ID_FILE_ENTRY,
                           CHUNK_DEFINITION_FILE_ENTRY,
                           Chunk_getSize(CHUNK_DEFINITION_FILE_ENTRY,archiveFileInfo->blockLength,&archiveFileInfo->file.chunkFileEntry),
                           &archiveFileInfo->file.chunkFileEntry
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = Chunk_create(&archiveFileInfo->file.chunkInfoFileData,
                           CHUNK_ID_FILE_DATA,
                           CHUNK_DEFINITION_FILE_DATA,
                           Chunk_getSize(CHUNK_DEFINITION_FILE_DATA,archiveFileInfo->blockLength,&archiveFileInfo->file.chunkFileData),
                           &archiveFileInfo->file.chunkFileData
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      archiveFileInfo->file.createdFlag       = TRUE;
      archiveFileInfo->file.headerWrittenFlag = TRUE;
    }

    if (length > 0)
    {
      /* encrypt block */
      error = Crypt_encrypt(&archiveFileInfo->file.cryptInfoData,
                            archiveFileInfo->file.buffer,
                            archiveFileInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      /* write block */
      error = Chunk_writeData(&archiveFileInfo->file.chunkInfoFileData,
                              archiveFileInfo->file.buffer,
                              archiveFileInfo->blockLength
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    /* flush compress buffer */
    Compress_flush(&archiveFileInfo->file.compressInfoData);
    while (!Compress_checkBlockIsEmpty(&archiveFileInfo->file.compressInfoData))
    {
      /* get compressed block */
      Compress_getBlock(&archiveFileInfo->file.compressInfoData,
                        archiveFileInfo->file.buffer,
                        &length
                       );         

      /* encrypt block */
      error = Crypt_encrypt(&archiveFileInfo->file.cryptInfoData,
                            archiveFileInfo->file.buffer,
                            archiveFileInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      /* write */
      error = Chunk_writeData(&archiveFileInfo->file.chunkInfoFileData,
                              archiveFileInfo->file.buffer,
                              archiveFileInfo->blockLength
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    /* update part size */
    archiveFileInfo->file.chunkFileData.fragmentSize = Compress_getInputLength(&archiveFileInfo->file.compressInfoData);
    error = Chunk_update(&archiveFileInfo->file.chunkInfoFileData,
                         &archiveFileInfo->file.chunkFileData
                        );
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* close chunks */
    error = Chunk_close(&archiveFileInfo->file.chunkInfoFileData);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Chunk_close(&archiveFileInfo->file.chunkInfoFileEntry);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Chunk_close(&archiveFileInfo->file.chunkInfoFile);
    if (error != ERROR_NONE)
    {
      return error;
    }
    archiveFileInfo->file.headerWrittenFlag = FALSE;

    /* close file */
    closeArchiveFile(archiveFileInfo->archiveInfo);

    /* reset compress (do it here because data if buffered and can be processed before a new file is opened) */
    Compress_reset(&archiveFileInfo->file.compressInfoData);
  }
  else
  {
    /* get size of space to reserve for chunk-header */
    if (!archiveFileInfo->file.headerWrittenFlag || newPartFlag)
    {
//      n = archiveFileInfo->headerLength;
    }
    else
    {
//      n = 0;
    }

    /* calculate max. length of data which can be written into this part */
//        freeBlocks = (archiveFileInfo->archiveInfo->partSize-(size+n))/archiveFileInfo->blockLength;

    if (!archiveFileInfo->file.createdFlag || (length > 0))
    {
      /* open file if needed */
      if (!archiveFileInfo->archiveInfo->fileOpenFlag)
      {
        /* create file */
        error = openArchiveFile(archiveFileInfo->archiveInfo);
        if (error != ERROR_NONE)
        {
          return error;
        }

        /* initialise variables */
        archiveFileInfo->file.headerWrittenFlag = FALSE;

        archiveFileInfo->file.chunkFileData.fragmentOffset = archiveFileInfo->file.chunkFileData.fragmentOffset+archiveFileInfo->file.chunkFileData.fragmentSize;
        archiveFileInfo->file.chunkFileData.fragmentSize   = 0;

        /* reset data crypt */
        Crypt_reset(&archiveFileInfo->file.cryptInfoData,0);
      }

      /* create chunk-headers */
      if (!archiveFileInfo->file.headerWrittenFlag)
      {
        error = Chunk_create(&archiveFileInfo->file.chunkInfoFile,
                             CHUNK_ID_FILE,
                             CHUNK_DEFINITION_FILE,
                             Chunk_getSize(CHUNK_DEFINITION_FILE,0,&archiveFileInfo->file.chunkFile),
                             &archiveFileInfo->file.chunkFile
                            );
        if (error != ERROR_NONE)
        {
          return error;
        }

        error = Chunk_create(&archiveFileInfo->file.chunkInfoFileEntry,
                             CHUNK_ID_FILE_ENTRY,
                             CHUNK_DEFINITION_FILE_ENTRY,
                             Chunk_getSize(CHUNK_DEFINITION_FILE_ENTRY,archiveFileInfo->blockLength,&archiveFileInfo->file.chunkFileEntry),
                             &archiveFileInfo->file.chunkFileEntry
                            );
        if (error != ERROR_NONE)
        {
          return error;
        }

        error = Chunk_create(&archiveFileInfo->file.chunkInfoFileData,
                             CHUNK_ID_FILE_DATA,
                             CHUNK_DEFINITION_FILE_DATA,
                             Chunk_getSize(CHUNK_DEFINITION_FILE_DATA,archiveFileInfo->blockLength,&archiveFileInfo->file.chunkFileData),
                             &archiveFileInfo->file.chunkFileData
                            );
        if (error != ERROR_NONE)
        {
          return error;
        }

        archiveFileInfo->file.createdFlag       = TRUE;
        archiveFileInfo->file.headerWrittenFlag = TRUE;
      }

      if (length > 0)
      {
        /* encrypt block */
        error = Crypt_encrypt(&archiveFileInfo->file.cryptInfoData,
                              archiveFileInfo->file.buffer,
                              archiveFileInfo->blockLength
                             );
        if (error != ERROR_NONE)
        {
          return error;
        }

        /* write block */
        error = Chunk_writeData(&archiveFileInfo->file.chunkInfoFileData,
                                archiveFileInfo->file.buffer,
                                archiveFileInfo->blockLength
                               );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : readDataBlock
* Purpose: read data block: decrypt
* Input  : archiveFileInfo - archive file info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors readDataBlock(ArchiveFileInfo *archiveFileInfo)
{
  Errors error;

  assert(archiveFileInfo != NULL);

  if (!Chunk_eofSub(&archiveFileInfo->file.chunkInfoFileData))
  {
    /* read */
    error = Chunk_readData(&archiveFileInfo->file.chunkInfoFileData,
                           archiveFileInfo->file.buffer,
                           archiveFileInfo->blockLength
                          );
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* decrypt */
    error = Crypt_decrypt(&archiveFileInfo->file.cryptInfoData,
                          archiveFileInfo->file.buffer,
                          archiveFileInfo->blockLength
                         );
    if (error != ERROR_NONE)
    {
      return error;
    }

    Compress_putBlock(&archiveFileInfo->file.compressInfoData,
                      archiveFileInfo->file.buffer,
                      archiveFileInfo->blockLength
                     );
  }
  else
  {
    Compress_flush(&archiveFileInfo->file.compressInfoData);
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Archive_init(void)
{
  Errors error;

  /* init chunks*/
  error = Chunk_init((bool(*)(void*))Storage_eof,
                     (Errors(*)(void*,void*,ulong,ulong*))Storage_read,
                     (Errors(*)(void*,const void*,ulong))Storage_write,
                     (Errors(*)(void*,uint64*))Storage_tell,
                     (Errors(*)(void*,uint64))Storage_seek
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

void Archive_done(void)
{
}

Errors Archive_create(ArchiveInfo            *archiveInfo,
                      ArchiveNewFileFunction archiveNewFileFunction,
                      void                   *archiveNewFileUserData,
const Options *options,
                      const char             *tmpDirectory,
                      uint64                 partSize,
                      CompressAlgorithms     compressAlgorithm,
                      ulong                  compressMinFileSize,
                      CryptAlgorithms        cryptAlgorithm,
                      Password               *password
                     )
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveNewFileFunction != NULL);

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(cryptAlgorithm,&archiveInfo->blockLength);
  if (error != ERROR_NONE)
  {
    return error;
  }
  assert(archiveInfo->blockLength > 0);
  if (archiveInfo->blockLength > MAX_BUFFER_SIZE)
  {
    return ERROR_UNSUPPORTED_BLOCK_SIZE;
  }

  archiveInfo->archiveNewFileFunction  = archiveNewFileFunction;  
  archiveInfo->archiveNewFileUserData  = archiveNewFileUserData;  
archiveInfo->options = options;
  archiveInfo->tmpDirectory            = tmpDirectory; 
  archiveInfo->partSize                = partSize;                           
  archiveInfo->compressAlgorithm       = compressAlgorithm;
  archiveInfo->compressMinFileSize     = compressMinFileSize;
  archiveInfo->cryptAlgorithm          = cryptAlgorithm;                           
  archiveInfo->password                = password;  

  archiveInfo->partNumber              = 0;
  archiveInfo->fileOpenFlag            = FALSE;
  archiveInfo->fileName                = NULL;

  archiveInfo->nextChunkHeaderReadFlag = FALSE;

  return ERROR_NONE;
}

Errors Archive_open(ArchiveInfo   *archiveInfo,
                    const String  archiveFileName,
                    const Options *options,
                    Password      *password
                   )
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveFileName != NULL);

  /* init */
  archiveInfo->archiveNewFileFunction  = NULL;  
  archiveInfo->archiveNewFileUserData  = NULL;  
  archiveInfo->tmpDirectory            = NULL;
  archiveInfo->partSize                = 0;
  archiveInfo->compressAlgorithm       = 0;
  archiveInfo->compressMinFileSize     = 0;
  archiveInfo->cryptAlgorithm          = CRYPT_ALGORITHM_UNKNOWN;
  archiveInfo->password                = password;

  archiveInfo->partNumber              = 0;
  archiveInfo->fileOpenFlag            = TRUE;
  archiveInfo->fileName                = String_copy(archiveFileName);

  archiveInfo->nextChunkHeaderReadFlag = FALSE;

  /* open storage */
  error = Storage_open(&archiveInfo->storageFileHandle,archiveInfo->fileName,options);
  if (error != ERROR_NONE)
  {
    String_delete(archiveInfo->fileName);
    return error;
  }

  return ERROR_NONE;
}

bool Archive_eof(ArchiveInfo *archiveInfo)
{
  bool        chunkHeaderFoundFlag;
  Errors      error;
  ChunkHeader chunkHeader;

  assert(archiveInfo != NULL);

  /* find next file, directory, link chunk */
  chunkHeaderFoundFlag = FALSE;
  while (!Storage_eof(&archiveInfo->storageFileHandle) && !chunkHeaderFoundFlag)
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      return error;
    }

    chunkHeaderFoundFlag = (   (chunkHeader.id == CHUNK_ID_FILE)
                            || (chunkHeader.id == CHUNK_ID_DIRECTORY)
                            || (chunkHeader.id == CHUNK_ID_LINK)
                           );

    if (!chunkHeaderFoundFlag)
    {
      error = Chunk_skip(archiveInfo,&chunkHeader);
      if (error != ERROR_NONE)
      {
        return error;
      }
      warning("Skipped unexpected chunk '%s'\n",Chunk_idToString(chunkHeader.id));
    }
  }

  if (chunkHeaderFoundFlag)
  {
    /* store chunk header for read */
    ungetNextChunkHeader(archiveInfo,&chunkHeader);
  }

  return !chunkHeaderFoundFlag;
}

Errors Archive_close(ArchiveInfo *archiveInfo)
{
  assert(archiveInfo != NULL);

  if (archiveInfo->fileOpenFlag)
  {
    closeArchiveFile(archiveInfo);
  }

  return ERROR_NONE;
}

Errors Archive_newFileEntry(ArchiveInfo     *archiveInfo,
                            ArchiveFileInfo *archiveFileInfo,
                            const String    name,
                            const FileInfo  *fileInfo
                           )
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo                         = archiveInfo;
  archiveFileInfo->mode                                = FILE_MODE_WRITE;

  archiveFileInfo->cryptAlgorithm                      = archiveInfo->cryptAlgorithm;
  archiveFileInfo->blockLength                         = archiveInfo->blockLength;

  archiveFileInfo->fileType                            = FILETYPE_FILE;

  archiveFileInfo->file.compressAlgorithm              = (fileInfo->size > archiveFileInfo->archiveInfo->compressMinFileSize)?archiveInfo->compressAlgorithm:COMPRESS_ALGORITHM_NONE;

  archiveFileInfo->file.chunkFile.compressAlgorithm    = archiveFileInfo->file.compressAlgorithm;
  archiveFileInfo->file.chunkFile.cryptAlgorithm       = archiveInfo->cryptAlgorithm;

  archiveFileInfo->file.chunkFileEntry.size            = fileInfo->size;
  archiveFileInfo->file.chunkFileEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveFileInfo->file.chunkFileEntry.timeModified    = fileInfo->timeModified;
  archiveFileInfo->file.chunkFileEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveFileInfo->file.chunkFileEntry.userId          = fileInfo->userId;
  archiveFileInfo->file.chunkFileEntry.groupId         = fileInfo->groupId;
  archiveFileInfo->file.chunkFileEntry.permission      = fileInfo->permission;
  archiveFileInfo->file.chunkFileEntry.name            = String_copy(name);

  archiveFileInfo->file.chunkFileData.fragmentOffset   = 0;
  archiveFileInfo->file.chunkFileData.fragmentSize     = 0;

  archiveFileInfo->file.createdFlag                    = FALSE;
  archiveFileInfo->file.headerLength                   = 0;
  archiveFileInfo->file.headerWrittenFlag              = FALSE;

  archiveFileInfo->file.buffer                         = NULL;
  archiveFileInfo->file.bufferLength                   = 0;

  /* init file-chunk */
  error = Chunk_new(&archiveFileInfo->file.chunkInfoFile,
                    NULL,
                    &archiveInfo->storageFileHandle,
                    0,
                    NULL
                   );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* allocate buffer */
  archiveFileInfo->file.bufferLength = (MAX_BUFFER_SIZE/archiveFileInfo->blockLength)*archiveFileInfo->blockLength;
  archiveFileInfo->file.buffer = (byte*)malloc(archiveFileInfo->file.bufferLength);
  if (archiveFileInfo->file.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init crypt */
  error = Crypt_new(&archiveFileInfo->file.cryptInfoFileEntry,
                    archiveInfo->cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    free(archiveFileInfo->file.buffer);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }
  error = Crypt_new(&archiveFileInfo->file.cryptInfoFileData,
                    archiveInfo->cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }
  error = Crypt_new(&archiveFileInfo->file.cryptInfoData,
                    archiveInfo->cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }

  /* init compress */
  error = Compress_new(&archiveFileInfo->file.compressInfoData,
                       COMPRESS_MODE_DEFLATE,
                       archiveFileInfo->file.compressAlgorithm,
                       archiveFileInfo->blockLength
                      );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->file.cryptInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return ERROR_COMPRESS_ERROR;
  }

  /* init entry/dat file-chunks */
  error = Chunk_new(&archiveFileInfo->file.chunkInfoFileEntry,
                    &archiveFileInfo->file.chunkInfoFile,
                    &archiveInfo->storageFileHandle,
                    archiveFileInfo->blockLength,
                    &archiveFileInfo->file.cryptInfoFileEntry
                   );
  if (error != ERROR_NONE)
  {
    Compress_delete(&archiveFileInfo->file.compressInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }
  error = Chunk_new(&archiveFileInfo->file.chunkInfoFileData,
                    &archiveFileInfo->file.chunkInfoFile,
                    &archiveInfo->storageFileHandle,
                    archiveFileInfo->blockLength,
                    &archiveFileInfo->file.cryptInfoFileData
                   );
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->file.chunkInfoFileEntry);
    Compress_delete(&archiveFileInfo->file.compressInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }

  /* calculate header length */
  archiveFileInfo->file.headerLength = Chunk_getSize(CHUNK_DEFINITION_FILE,      0,                           &archiveFileInfo->file.chunkFile     )+
                                       Chunk_getSize(CHUNK_DEFINITION_FILE_ENTRY,archiveFileInfo->blockLength,&archiveFileInfo->file.chunkFileEntry)+
                                       Chunk_getSize(CHUNK_DEFINITION_FILE_DATA, archiveFileInfo->blockLength,&archiveFileInfo->file.chunkFileData );

  return ERROR_NONE;
}

Errors Archive_newDirectoryEntry(ArchiveInfo     *archiveInfo,
                                 ArchiveFileInfo *archiveFileInfo,
                                 const String    name,
                                 FileInfo        *fileInfo
                                )
{
  Errors error;
  ulong  length;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo                                   = archiveInfo;
  archiveFileInfo->mode                                          = FILE_MODE_WRITE;

  archiveFileInfo->cryptAlgorithm                                = archiveInfo->cryptAlgorithm;
  archiveFileInfo->blockLength                                   = archiveInfo->blockLength;

  archiveFileInfo->fileType                                      = FILETYPE_DIRECTORY;

  archiveFileInfo->directory.chunkDirectory.cryptAlgorithm       = archiveInfo->cryptAlgorithm;

  archiveFileInfo->directory.chunkDirectoryEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveFileInfo->directory.chunkDirectoryEntry.timeModified    = fileInfo->timeModified;
  archiveFileInfo->directory.chunkDirectoryEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveFileInfo->directory.chunkDirectoryEntry.userId          = fileInfo->userId;
  archiveFileInfo->directory.chunkDirectoryEntry.groupId         = fileInfo->groupId;
  archiveFileInfo->directory.chunkDirectoryEntry.permission      = fileInfo->permission;
  archiveFileInfo->directory.chunkDirectoryEntry.name            = String_copy(name);

  /* init directory chunk */
  error = Chunk_new(&archiveFileInfo->directory.chunkInfoDirectory,
                    NULL,
                    &archiveInfo->storageFileHandle,
                    0,
                    NULL
                   );
  if (error != ERROR_NONE)
  {
    String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
    return error;
  }

  /* init crypt */
  error = Crypt_new(&archiveFileInfo->directory.cryptInfoDirectoryEntry,
                    archiveInfo->cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
    String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
    return error;
  }

  /* init directory entry chunk */
  error = Chunk_new(&archiveFileInfo->directory.chunkInfoDirectoryEntry,
                    &archiveFileInfo->directory.chunkInfoDirectory,
                    &archiveInfo->storageFileHandle,
                    archiveFileInfo->blockLength,
                    &archiveFileInfo->directory.cryptInfoDirectoryEntry
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
    String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
    return error;
  }

  /* calculate header length */
  length = Chunk_getSize(CHUNK_DEFINITION_DIRECTORY,      0,                           &archiveFileInfo->directory.chunkDirectory     )+
           Chunk_getSize(CHUNK_DEFINITION_DIRECTORY_ENTRY,archiveFileInfo->blockLength,&archiveFileInfo->directory.chunkDirectoryEntry);

  /* ensure space in archive */
  error = ensureArchiveSpace(archiveFileInfo->archiveInfo,
                             length
                            );
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
    String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
    return error;
  }

  /* write directory chunk */
  error = Chunk_create(&archiveFileInfo->directory.chunkInfoDirectory,
                       CHUNK_ID_DIRECTORY,
                       CHUNK_DEFINITION_DIRECTORY,
                       Chunk_getSize(CHUNK_DEFINITION_DIRECTORY,0,&archiveFileInfo->directory.chunkDirectory),
                       &archiveFileInfo->directory.chunkDirectory
                      );
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
    String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
    return error;
  }
  error = Chunk_create(&archiveFileInfo->directory.chunkInfoDirectoryEntry,
                       CHUNK_ID_DIRECTORY_ENTRY,
                       CHUNK_DEFINITION_DIRECTORY_ENTRY,
                       Chunk_getSize(CHUNK_DEFINITION_DIRECTORY_ENTRY,archiveFileInfo->blockLength,&archiveFileInfo->directory.chunkDirectoryEntry),
                       &archiveFileInfo->directory.chunkDirectoryEntry
                      );
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
    String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
    return error;
  }

  /* close chunks */
  error = Chunk_close(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
    String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
    return error;
  }
  error = Chunk_close(&archiveFileInfo->directory.chunkInfoDirectory);
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
    String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
    return error;
  }

  return ERROR_NONE;
}

Errors Archive_newLinkEntry(ArchiveInfo     *archiveInfo,
                            ArchiveFileInfo *archiveFileInfo,
                            const String    name,
                            const String    fileName,
                            FileInfo        *fileInfo
                           )
{
  Errors error;
  ulong  length;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo                         = archiveInfo;
  archiveFileInfo->mode                                = FILE_MODE_WRITE;

  archiveFileInfo->cryptAlgorithm                      = archiveInfo->cryptAlgorithm;
  archiveFileInfo->blockLength                         = archiveInfo->blockLength;

  archiveFileInfo->fileType                            = FILETYPE_LINK;

  archiveFileInfo->link.chunkLink.cryptAlgorithm       = archiveInfo->cryptAlgorithm;

  archiveFileInfo->link.chunkLinkEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveFileInfo->link.chunkLinkEntry.timeModified    = fileInfo->timeModified;
  archiveFileInfo->link.chunkLinkEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveFileInfo->link.chunkLinkEntry.userId          = fileInfo->userId;
  archiveFileInfo->link.chunkLinkEntry.groupId         = fileInfo->groupId;
  archiveFileInfo->link.chunkLinkEntry.permission      = fileInfo->permission;
  archiveFileInfo->link.chunkLinkEntry.name            = String_copy(name);
  archiveFileInfo->link.chunkLinkEntry.fileName        = String_copy(fileName);

  /* init link-chunk */
  error = Chunk_new(&archiveFileInfo->link.chunkInfoLink,
                    NULL,
                    &archiveInfo->storageFileHandle,
                    0,
                    NULL
                   );
  if (error != ERROR_NONE)
  {
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.fileName);
    return error;
  }

  /* init crypt */
  error = Crypt_new(&archiveFileInfo->link.cryptInfoLinkEntry,
                    archiveInfo->cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.fileName);
    return error;
  }

  /* init link entry chunk */
  error = Chunk_new(&archiveFileInfo->link.chunkInfoLinkEntry,
                    &archiveFileInfo->link.chunkInfoLink,
                    &archiveInfo->storageFileHandle,
                    archiveFileInfo->blockLength,
                    &archiveFileInfo->link.cryptInfoLinkEntry
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.fileName);
    return error;
  }

  /* calculate length */
  length = Chunk_getSize(CHUNK_DEFINITION_LINK,      0,                           &archiveFileInfo->link.chunkLink     )+
           Chunk_getSize(CHUNK_DEFINITION_LINK_ENTRY,archiveFileInfo->blockLength,&archiveFileInfo->link.chunkLinkEntry);

  /* ensure space in archive */
  error = ensureArchiveSpace(archiveFileInfo->archiveInfo,
                             length
                            );
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->link.chunkInfoLinkEntry);
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.fileName);
    return error;
  }

  /* write link chunks */
  error = Chunk_create(&archiveFileInfo->link.chunkInfoLink,
                       CHUNK_ID_LINK,
                       CHUNK_DEFINITION_LINK,
                       Chunk_getSize(CHUNK_DEFINITION_LINK,0,&archiveFileInfo->link.chunkLink),
                       &archiveFileInfo->link.chunkLink
                      );
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->link.chunkInfoLinkEntry);
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.fileName);
    return error;
  }
  error = Chunk_create(&archiveFileInfo->link.chunkInfoLinkEntry,
                       CHUNK_ID_LINK_ENTRY,
                       CHUNK_DEFINITION_LINK_ENTRY,
                       Chunk_getSize(CHUNK_DEFINITION_LINK_ENTRY,archiveFileInfo->blockLength,&archiveFileInfo->link.chunkLinkEntry),
                       &archiveFileInfo->link.chunkLinkEntry
                      );
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->link.chunkInfoLinkEntry);
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.fileName);
    return error;
  }

  /* close chunks */
  error = Chunk_close(&archiveFileInfo->link.chunkInfoLinkEntry);
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->link.chunkInfoLinkEntry);
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.fileName);
    return error;
  }
  error = Chunk_close(&archiveFileInfo->link.chunkInfoLink);
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->link.chunkInfoLinkEntry);
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.fileName);
    return error;
  }

  return ERROR_NONE;
}

Errors Archive_getNextFileType(ArchiveInfo     *archiveInfo,
                               ArchiveFileInfo *archiveFileInfo,
                               FileTypes       *fileType
                              )
{
  Errors      error;
  ChunkHeader chunkHeader;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileType != NULL);

  UNUSED_VARIABLE(archiveFileInfo);

  /* find next file, directory, link chunk */
  do
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      return error;
    }

    if (   (chunkHeader.id != CHUNK_ID_FILE)
        && (chunkHeader.id != CHUNK_ID_DIRECTORY)
        && (chunkHeader.id != CHUNK_ID_LINK)
       )
    {
      error = Chunk_skip(archiveInfo,&chunkHeader);
      if (error != ERROR_NONE)
      {
        return error;
      }
      continue;
    }
  }
  while (   (chunkHeader.id != CHUNK_ID_FILE)
         && (chunkHeader.id != CHUNK_ID_DIRECTORY)
         && (chunkHeader.id != CHUNK_ID_LINK)
        );

  /* get file type */
  switch (chunkHeader.id)
  {
    case CHUNK_ID_FILE:      (*fileType) = FILETYPE_FILE;      break;
    case CHUNK_ID_DIRECTORY: (*fileType) = FILETYPE_DIRECTORY; break;
    case CHUNK_ID_LINK:      (*fileType) = FILETYPE_LINK;      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  /* store chunk header for read */
  ungetNextChunkHeader(archiveInfo,&chunkHeader);

  return ERROR_NONE;
}

Errors Archive_readFileEntry(ArchiveInfo        *archiveInfo,
                             ArchiveFileInfo    *archiveFileInfo,
                             CompressAlgorithms *compressAlgorithm,
                             CryptAlgorithms    *cryptAlgorithm,
                             String             name,
                             FileInfo           *fileInfo,
                             uint64             *fragmentOffset,
                             uint64             *fragmentSize
                            )
{
  Errors      error;
  ChunkHeader chunkHeader;
  bool        foundFileEntry,foundFileData;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo       = archiveInfo;
  archiveFileInfo->mode              = FILE_MODE_READ;
  archiveFileInfo->fileType          = FILETYPE_FILE;

  archiveFileInfo->file.buffer       = NULL;
  archiveFileInfo->file.bufferLength = 0;

  /* init file chunk */
  error = Chunk_new(&archiveFileInfo->file.chunkInfoFile,
                    NULL,
                    &archiveInfo->storageFileHandle,
                    0,
                    NULL
                   );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* find next file chunk */
  do
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_FILE)
    {
      error = Chunk_skip(archiveInfo,&chunkHeader);
      if (error != ERROR_NONE)
      {
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_FILE);

  /* read file chunk, find file data */
  error = Chunk_open(&archiveFileInfo->file.chunkInfoFile,
                     &chunkHeader,
                     CHUNK_ID_FILE,
                     CHUNK_DEFINITION_FILE,
                     Chunk_getSize(CHUNK_DEFINITION_FILE,0,NULL),
                     &archiveFileInfo->file.chunkFile
                    );
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }
  archiveFileInfo->cryptAlgorithm         = archiveFileInfo->file.chunkFile.cryptAlgorithm;
  archiveFileInfo->file.compressAlgorithm = archiveFileInfo->file.chunkFile.compressAlgorithm;

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }
  assert(archiveFileInfo->blockLength > 0);

  /* allocate buffer */
  archiveFileInfo->file.bufferLength = (MAX_BUFFER_SIZE/archiveFileInfo->blockLength)*archiveFileInfo->blockLength;
  archiveFileInfo->file.buffer = (byte*)malloc(archiveFileInfo->file.bufferLength);
  if (archiveFileInfo->file.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init crypt */
  error = Crypt_new(&archiveFileInfo->file.cryptInfoFileEntry,
                    archiveFileInfo->cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    free(archiveFileInfo->file.buffer);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }
  error = Crypt_new(&archiveFileInfo->file.cryptInfoFileData,
                    archiveFileInfo->cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }
  error = Crypt_new(&archiveFileInfo->file.cryptInfoData,
                    archiveFileInfo->cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }

  /* init compress */
  error = Compress_new(&archiveFileInfo->file.compressInfoData,
                       COMPRESS_MODE_INFLATE,
                       archiveFileInfo->file.compressAlgorithm,
                       archiveFileInfo->blockLength
                      );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->file.cryptInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return ERROR_COMPRESS_ERROR;
  }

  /* init file entry/data chunks */
  error = Chunk_new(&archiveFileInfo->file.chunkInfoFileEntry,
                    &archiveFileInfo->file.chunkInfoFile,
                    &archiveInfo->storageFileHandle,
                    archiveFileInfo->blockLength,
                    &archiveFileInfo->file.cryptInfoFileEntry
                   );
  if (error != ERROR_NONE)
  {
    Compress_delete(&archiveFileInfo->file.compressInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }
  error = Chunk_new(&archiveFileInfo->file.chunkInfoFileData,
                    &archiveFileInfo->file.chunkInfoFile,
                    &archiveInfo->storageFileHandle,
                    archiveFileInfo->blockLength,
                    &archiveFileInfo->file.cryptInfoFileData
                   );
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->file.chunkInfoFileEntry);
    Compress_delete(&archiveFileInfo->file.compressInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }

  /* read file entry/data chunks */
  foundFileEntry = FALSE;
  foundFileData  = FALSE;
  while (   !Chunk_eofSub(&archiveFileInfo->file.chunkInfoFile)
         && (!foundFileEntry || !foundFileData)         
        )
  {
    error = Chunk_nextSub(&archiveFileInfo->file.chunkInfoFile,&chunkHeader);
    if (error != ERROR_NONE)
    {
      Chunk_delete(&archiveFileInfo->file.chunkInfoFileData);
      Chunk_delete(&archiveFileInfo->file.chunkInfoFileEntry);
      Compress_delete(&archiveFileInfo->file.compressInfoData);
      Crypt_delete(&archiveFileInfo->file.cryptInfoData);
      Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
      Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
      free(archiveFileInfo->file.buffer);
      Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
      return error;
    }

    switch (chunkHeader.id)
    {
      case CHUNK_ID_FILE_ENTRY:
        error = Chunk_open(&archiveFileInfo->file.chunkInfoFileEntry,
                           &chunkHeader,
                           CHUNK_ID_FILE_ENTRY,
                           CHUNK_DEFINITION_FILE_ENTRY,
                           chunkHeader.size,
                           &archiveFileInfo->file.chunkFileEntry
                          );
        if (error != ERROR_NONE)
        {
          Chunk_delete(&archiveFileInfo->file.chunkInfoFileData);
          Chunk_delete(&archiveFileInfo->file.chunkInfoFileEntry);
          Compress_delete(&archiveFileInfo->file.compressInfoData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
          free(archiveFileInfo->file.buffer);
          Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
          return error;
        }

        String_set(name,archiveFileInfo->file.chunkFileEntry.name);
        fileInfo->size            = archiveFileInfo->file.chunkFileEntry.size;
        fileInfo->timeLastAccess  = archiveFileInfo->file.chunkFileEntry.timeLastAccess;
        fileInfo->timeModified    = archiveFileInfo->file.chunkFileEntry.timeModified;
        fileInfo->timeLastChanged = archiveFileInfo->file.chunkFileEntry.timeLastChanged;
        fileInfo->userId          = archiveFileInfo->file.chunkFileEntry.userId;
        fileInfo->groupId         = archiveFileInfo->file.chunkFileEntry.groupId;
        fileInfo->permission      = archiveFileInfo->file.chunkFileEntry.permission;

        foundFileEntry = TRUE;
        break;
      case CHUNK_ID_FILE_DATA:
        error = Chunk_open(&archiveFileInfo->file.chunkInfoFileData,
                           &chunkHeader,
                           CHUNK_ID_FILE_DATA,
                           CHUNK_DEFINITION_FILE_DATA,
                           Chunk_getSize(CHUNK_DEFINITION_FILE_DATA,archiveFileInfo->blockLength,NULL),
                           &archiveFileInfo->file.chunkFileData
                          );
        if (error != ERROR_NONE)
        {
          if (foundFileEntry) String_delete(archiveFileInfo->file.chunkFileEntry.name);
          Chunk_delete(&archiveFileInfo->file.chunkInfoFileData);
          Chunk_delete(&archiveFileInfo->file.chunkInfoFileEntry);
          Compress_delete(&archiveFileInfo->file.compressInfoData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
          free(archiveFileInfo->file.buffer);
          Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
          return error;
        }

        if (fragmentOffset != NULL) (*fragmentOffset) = archiveFileInfo->file.chunkFileData.fragmentOffset;
        if (fragmentSize   != NULL) (*fragmentSize)   = archiveFileInfo->file.chunkFileData.fragmentSize;

        foundFileData = TRUE;
        break;
      default:
        error = Chunk_skipSub(&archiveFileInfo->file.chunkInfoFile,&chunkHeader);
        if (error != ERROR_NONE)
        {
          if (foundFileEntry) String_delete(archiveFileInfo->file.chunkFileEntry.name);
          Chunk_delete(&archiveFileInfo->file.chunkInfoFileData);
          Chunk_delete(&archiveFileInfo->file.chunkInfoFileEntry);
          Compress_delete(&archiveFileInfo->file.compressInfoData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
          free(archiveFileInfo->file.buffer);
          Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
        }
        break;
    }
  }
  if (!foundFileEntry)
  {
    Chunk_delete(&archiveFileInfo->file.chunkInfoFileData);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFileEntry);
    Compress_delete(&archiveFileInfo->file.compressInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return ERROR_NO_FILE_ENTRY;
  }
  if (!foundFileData)
  {
    if (foundFileEntry) String_delete(archiveFileInfo->file.chunkFileEntry.name);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFileData);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFileEntry);
    Compress_delete(&archiveFileInfo->file.compressInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
    return ERROR_NO_FILE_DATA;
  }

  if (compressAlgorithm != NULL) (*compressAlgorithm) = archiveFileInfo->file.compressAlgorithm;
  if (cryptAlgorithm    != NULL) (*cryptAlgorithm)    = archiveFileInfo->cryptAlgorithm;

  /* reset compress, crypt */
  Compress_reset(&archiveFileInfo->file.compressInfoData);
  Crypt_reset(&archiveFileInfo->file.cryptInfoFileData,0);

  return ERROR_NONE;
}

Errors Archive_readDirectoryEntry(ArchiveInfo     *archiveInfo,
                                  ArchiveFileInfo *archiveFileInfo,
                                  CryptAlgorithms *cryptAlgorithm,
                                  String          name,
                                  FileInfo        *fileInfo
                                 )
{
  Errors      error;
  ChunkHeader chunkHeader;
  bool        foundDirectoryEntry;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo = archiveInfo;
  archiveFileInfo->mode        = FILE_MODE_READ;
  archiveFileInfo->fileType    = FILETYPE_DIRECTORY;

  /* init file chunk */
  error = Chunk_new(&archiveFileInfo->directory.chunkInfoDirectory,
                    NULL,
                    &archiveInfo->storageFileHandle,
                    0,
                    NULL
                   );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* find next directory chunk */
  do
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_DIRECTORY)
    {
      error = Chunk_skip(archiveInfo,&chunkHeader);
      if (error != ERROR_NONE)
      {
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_DIRECTORY);

  /* read directory chunk */
  error = Chunk_open(&archiveFileInfo->directory.chunkInfoDirectory,
                     &chunkHeader,
                     CHUNK_ID_DIRECTORY,
                     CHUNK_DEFINITION_DIRECTORY,
                     Chunk_getSize(CHUNK_DEFINITION_DIRECTORY,0,NULL),
                     &archiveFileInfo->directory.chunkDirectory
                    );
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
    return error;
  }
  archiveFileInfo->cryptAlgorithm = archiveFileInfo->directory.chunkDirectory.cryptAlgorithm;

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
    return error;
  }
  assert(archiveFileInfo->blockLength > 0);

  /* init crypt */
  error = Crypt_new(&archiveFileInfo->directory.cryptInfoDirectoryEntry,
                    archiveFileInfo->cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
    return error;
  }

  /* init directory entry */
  error = Chunk_new(&archiveFileInfo->directory.chunkInfoDirectoryEntry,
                    &archiveFileInfo->directory.chunkInfoDirectory,
                    &archiveInfo->storageFileHandle,
                    archiveFileInfo->blockLength,
                    &archiveFileInfo->directory.cryptInfoDirectoryEntry
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
    return error;
  }

  /* read directory entry */
  foundDirectoryEntry = FALSE;
  while (   !Chunk_eofSub(&archiveFileInfo->directory.chunkInfoDirectory)
         && !foundDirectoryEntry         
        )
  {
    error = Chunk_nextSub(&archiveFileInfo->directory.chunkInfoDirectory,&chunkHeader);
    if (error != ERROR_NONE)
    {
      Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
      Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
      Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
      return error;
    }

    switch (chunkHeader.id)
    {
      case CHUNK_ID_DIRECTORY_ENTRY:
        error = Chunk_open(&archiveFileInfo->directory.chunkInfoDirectoryEntry,
                           &chunkHeader,
                           CHUNK_ID_DIRECTORY_ENTRY,
                           CHUNK_DEFINITION_DIRECTORY_ENTRY,
                           chunkHeader.size,
                           &archiveFileInfo->directory.chunkDirectoryEntry
                          );
        if (error != ERROR_NONE)
        {
          Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
          Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
          Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
          return error;
        }

        String_set(name,archiveFileInfo->directory.chunkDirectoryEntry.name);
        fileInfo->timeLastAccess  = archiveFileInfo->directory.chunkDirectoryEntry.timeLastAccess;
        fileInfo->timeModified    = archiveFileInfo->directory.chunkDirectoryEntry.timeModified;
        fileInfo->timeLastChanged = archiveFileInfo->directory.chunkDirectoryEntry.timeLastChanged;
        fileInfo->userId          = archiveFileInfo->directory.chunkDirectoryEntry.userId;
        fileInfo->groupId         = archiveFileInfo->directory.chunkDirectoryEntry.groupId;
        fileInfo->permission      = archiveFileInfo->directory.chunkDirectoryEntry.permission;

        foundDirectoryEntry = TRUE;
        break;
      default:
        error = Chunk_skipSub(&archiveFileInfo->directory.chunkInfoDirectory,&chunkHeader);
        if (error != ERROR_NONE)
        {
          Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
          Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
          Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
          return error;
        }
        break;
    }
  }
  if (!foundDirectoryEntry)
  {
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_delete(&archiveFileInfo->directory.chunkInfoDirectory);
    return ERROR_NO_FILE_ENTRY;
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveFileInfo->cryptAlgorithm;

  return ERROR_NONE;
}

Errors Archive_readLinkEntry(ArchiveInfo     *archiveInfo,
                             ArchiveFileInfo *archiveFileInfo,
                             CryptAlgorithms *cryptAlgorithm,
                             String          name,
                             String          fileName,
                             FileInfo        *fileInfo
                            )
{
  Errors      error;
  ChunkHeader chunkHeader;
  bool        foundLinkEntry;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo = archiveInfo;
  archiveFileInfo->mode        = FILE_MODE_READ;
  archiveFileInfo->fileType    = FILETYPE_LINK;

  /* init file chunk */
  error = Chunk_new(&archiveFileInfo->link.chunkInfoLink,
                    NULL,
                    &archiveInfo->storageFileHandle,
                    0,
                    NULL
                   );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* find next link chunk */
  do
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_LINK)
    {
      Chunk_skip(archiveInfo,&chunkHeader);
      if (error != ERROR_NONE)
      {
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_LINK);

  /* read link chunk */
  error = Chunk_open(&archiveFileInfo->link.chunkInfoLink,
                     &chunkHeader,
                     CHUNK_ID_LINK,
                     CHUNK_DEFINITION_LINK,
                     Chunk_getSize(CHUNK_DEFINITION_LINK,0,NULL),
                     &archiveFileInfo->link.chunkLink
                    );
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
    return error;
  }
  archiveFileInfo->cryptAlgorithm = archiveFileInfo->link.chunkLink.cryptAlgorithm;

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
    return error;
  }
  assert(archiveFileInfo->blockLength > 0);

  /* init crypt */
  error = Crypt_new(&archiveFileInfo->link.cryptInfoLinkEntry,
                    archiveFileInfo->cryptAlgorithm,
                    archiveInfo->password
                   );
  if (error != ERROR_NONE)
  {
    Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
    return error;
  }

  /* init link entry */
  error = Chunk_new(&archiveFileInfo->link.chunkInfoLinkEntry,
                    &archiveFileInfo->link.chunkInfoLink,
                    &archiveInfo->storageFileHandle,
                    archiveFileInfo->blockLength,
                    &archiveFileInfo->link.cryptInfoLinkEntry
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
    return error;
  }

  /* read link entry */
  foundLinkEntry = FALSE;
  while (   !Chunk_eofSub(&archiveFileInfo->link.chunkInfoLink)
         && !foundLinkEntry         
        )
  {
    error = Chunk_nextSub(&archiveFileInfo->link.chunkInfoLink,&chunkHeader);
    if (error != ERROR_NONE)
    {
      Chunk_delete(&archiveFileInfo->link.chunkInfoLinkEntry);
      Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
      Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
      return error;
    }

    switch (chunkHeader.id)
    {
      case CHUNK_ID_LINK_ENTRY:
        error = Chunk_open(&archiveFileInfo->link.chunkInfoLinkEntry,
                           &chunkHeader,
                           CHUNK_ID_LINK_ENTRY,
                           CHUNK_DEFINITION_LINK_ENTRY,
                           chunkHeader.size,
                           &archiveFileInfo->link.chunkLinkEntry
                          );
        if (error != ERROR_NONE)
        {
          Chunk_delete(&archiveFileInfo->link.chunkInfoLinkEntry);
          Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
          Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
          return error;
        }

        String_set(name,archiveFileInfo->link.chunkLinkEntry.name);
        String_set(fileName,archiveFileInfo->link.chunkLinkEntry.fileName);
        fileInfo->timeLastAccess  = archiveFileInfo->link.chunkLinkEntry.timeLastAccess;
        fileInfo->timeModified    = archiveFileInfo->link.chunkLinkEntry.timeModified;
        fileInfo->timeLastChanged = archiveFileInfo->link.chunkLinkEntry.timeLastChanged;
        fileInfo->userId          = archiveFileInfo->link.chunkLinkEntry.userId;
        fileInfo->groupId         = archiveFileInfo->link.chunkLinkEntry.groupId;
        fileInfo->permission      = archiveFileInfo->link.chunkLinkEntry.permission;

        foundLinkEntry = TRUE;
        break;
      default:
        error = Chunk_skipSub(&archiveFileInfo->link.chunkInfoLink,&chunkHeader);
        if (error != ERROR_NONE)
        {
          Chunk_delete(&archiveFileInfo->link.chunkInfoLinkEntry);
          Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
          Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
          return error;
        }
        break;
    }
  }
  if (!foundLinkEntry)
  {
    Chunk_delete(&archiveFileInfo->link.chunkInfoLinkEntry);
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
    return ERROR_NO_FILE_ENTRY;
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveFileInfo->cryptAlgorithm;

  return ERROR_NONE;
}

Errors Archive_closeEntry(ArchiveFileInfo *archiveFileInfo)
{
  Errors error;

  assert(archiveFileInfo != NULL);
  assert(archiveFileInfo->archiveInfo != NULL);

  switch (archiveFileInfo->mode)
  {
    case FILE_MODE_WRITE:
      switch (archiveFileInfo->fileType)
      {
        case FILETYPE_FILE:
          /* flush last blocks */
          Compress_flush(&archiveFileInfo->file.compressInfoData);
          if (!archiveFileInfo->file.createdFlag || !Compress_checkBlockIsEmpty(&archiveFileInfo->file.compressInfoData))
          {
            while (!Compress_checkBlockIsEmpty(&archiveFileInfo->file.compressInfoData))
            {
              error = writeDataBlock(archiveFileInfo,BLOCK_MODE_WRITE);
              if (error != ERROR_NONE)
              {
                return error;
              }
            }
            error = writeDataBlock(archiveFileInfo,BLOCK_MODE_FLUSH);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }

          /* update file and chunks if header is written */
          if (archiveFileInfo->file.headerWrittenFlag)
          {
            /* update part size */
            archiveFileInfo->file.chunkFileData.fragmentSize = Compress_getInputLength(&archiveFileInfo->file.compressInfoData);
            error = Chunk_update(&archiveFileInfo->file.chunkInfoFileData,
                                 &archiveFileInfo->file.chunkFileData
                                );
            if (error != ERROR_NONE)
            {
              return error;
            }

            /* close chunks */
            error = Chunk_close(&archiveFileInfo->file.chunkInfoFileData);
            if (error != ERROR_NONE)
            {
              return error;
            }
            error = Chunk_close(&archiveFileInfo->file.chunkInfoFileEntry);
            if (error != ERROR_NONE)
            {
              return error;
            }
            error = Chunk_close(&archiveFileInfo->file.chunkInfoFile);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }

          /* free resources */
          Chunk_delete(&archiveFileInfo->file.chunkInfoFileData);
          Chunk_delete(&archiveFileInfo->file.chunkInfoFileEntry);
          Compress_delete(&archiveFileInfo->file.compressInfoData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
          Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
          free(archiveFileInfo->file.buffer);
          String_delete(archiveFileInfo->file.chunkFileEntry.name);
          break;
        case FILETYPE_DIRECTORY:
          String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
          break;
        case FILETYPE_LINK:
          /* close chunks */
          error = Chunk_close(&archiveFileInfo->link.chunkInfoLinkEntry);
          if (error != ERROR_NONE)
          {
            return error;
          }
          error = Chunk_close(&archiveFileInfo->link.chunkInfoLink);
          if (error != ERROR_NONE)
          {
            return error;
          }

          /* free resources */
          Chunk_delete(&archiveFileInfo->link.chunkInfoLinkEntry);
          Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
          Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
          String_delete(archiveFileInfo->link.chunkLinkEntry.name);
          String_delete(archiveFileInfo->link.chunkLinkEntry.fileName);
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break; /* not reached */
      }
      break;
    case FILE_MODE_READ:
      switch (archiveFileInfo->fileType)
      {
        case FILETYPE_FILE:
          /* close chunks */
          error = Chunk_close(&archiveFileInfo->file.chunkInfoFileData);
          if (error != ERROR_NONE)
          {
            return error;
          }
          error = Chunk_close(&archiveFileInfo->file.chunkInfoFileEntry);
          if (error != ERROR_NONE)
          {
            return error;
          }
          error = Chunk_close(&archiveFileInfo->file.chunkInfoFile);
          if (error != ERROR_NONE)
          {
            return error;
          }

          /* free resources */
          Chunk_delete(&archiveFileInfo->file.chunkInfoFileData);
          Chunk_delete(&archiveFileInfo->file.chunkInfoFileEntry);
          Compress_delete(&archiveFileInfo->file.compressInfoData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
          Chunk_delete(&archiveFileInfo->file.chunkInfoFile);
          free(archiveFileInfo->file.buffer);
          String_delete(archiveFileInfo->file.chunkFileEntry.name);
          break;
        case FILETYPE_DIRECTORY:
          String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
          break;
        case FILETYPE_LINK:
          /* close chunks */
          error = Chunk_close(&archiveFileInfo->link.chunkInfoLinkEntry);
          if (error != ERROR_NONE)
          {
            return error;
          }
          error = Chunk_close(&archiveFileInfo->link.chunkInfoLink);
          if (error != ERROR_NONE)
          {
            return error;
          }

          /* free resources */
          Chunk_delete(&archiveFileInfo->link.chunkInfoLinkEntry);
          Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
          Chunk_delete(&archiveFileInfo->link.chunkInfoLink);
          String_delete(archiveFileInfo->link.chunkLinkEntry.name);
          String_delete(archiveFileInfo->link.chunkLinkEntry.fileName);
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break; /* not reached */
      }
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
  assert((archiveFileInfo->file.bufferLength%archiveFileInfo->blockLength) == 0);

  p      = (byte*)buffer;
  length = 0;
  while (length < bufferLength)
  {
    /* compress */
    error = Compress_deflate(&archiveFileInfo->file.compressInfoData,*p);
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* check if block can be encrypted and written */
    while (Compress_checkBlockIsFull(&archiveFileInfo->file.compressInfoData))
    {
      error = writeDataBlock(archiveFileInfo,BLOCK_MODE_WRITE);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
    assert(!Compress_checkBlockIsFull(&archiveFileInfo->file.compressInfoData));

    /* next byte */
    p++;
    length++;
  }

  archiveFileInfo->file.bufferLength = 0;

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
      error = Compress_available(&archiveFileInfo->file.compressInfoData,&n);
      if (error != ERROR_NONE)
      {
        return error;
      }

      if (n <= 0)
      {
        error = readDataBlock(archiveFileInfo);
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
    }
    while (n <= 0);

    /* decompress data */
    error = Compress_inflate(&archiveFileInfo->file.compressInfoData,p);
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* next byte */
    p++;
    length++;
  }

  return ERROR_NONE;
}

uint64 Archive_getSize(ArchiveFileInfo *archiveFileInfo)
{
  assert(archiveFileInfo != NULL);

  return Storage_getSize(&archiveFileInfo->archiveInfo->storageFileHandle);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
