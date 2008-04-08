/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/archive.c,v $
* $Revision: 1.40 $
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
* Name   : initCryptPassword
* Purpose: initialize crypt password if password not set
* Input  : cryptAlgorithm   - crypt algorithm to use
* Output : cryptPassword - crypt password
* Return : TRUE if passwort initialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool initCryptPassword(CryptAlgorithms cryptAlgorithm,
                             Password        **cryptPassword
                            )
{
  assert(cryptPassword != NULL);

  if ((cryptAlgorithm != CRYPT_ALGORITHM_NONE) && ((*cryptPassword) == NULL))
  {
    return inputCryptPassword(cryptPassword);
  }
  else
  {
    return TRUE;
  }
}

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
    if (Chunk_eof(&archiveInfo->fileHandle))
    {
      return ERROR_END_OF_ARCHIVE;
    }

    error = Chunk_next(&archiveInfo->fileHandle,chunkHeader);
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
  if (archiveInfo->jobOptions->archivePartSize > 0)
  {
    if (archiveInfo->fileOpenFlag)
    {
      /* get file size */
      fileSize = File_getSize(&archiveInfo->fileHandle);

      if      (   !headerWrittenFlag
               && (fileSize+headerLength >= archiveInfo->jobOptions->archivePartSize)
              )
      {
        /* file header cannot be written without fragmentation -> new part */
        newPartFlag = TRUE;
      }
      else if ((fileSize+minBytes) >= archiveInfo->jobOptions->archivePartSize)
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
  error = File_getTmpFileName(archiveInfo->fileName,globalOptions.tmpDirectory);
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* create file */
  error = File_open(&archiveInfo->fileHandle,archiveInfo->fileName,FILE_OPENMODE_CREATE);
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
* Input  : archiveInfo  - archive info block
*          lastPartFlag - TRUE iff last archive part, FALSE otherwise
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors closeArchiveFile(ArchiveInfo *archiveInfo,
                              bool        lastPartFlag
                             )
{
  uint64 size;
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveInfo->fileOpenFlag);

  /* close file */
  size = File_getSize(&archiveInfo->fileHandle);
  File_close(&archiveInfo->fileHandle);

  if (archiveInfo->archiveNewFileFunction != NULL)
  {
    /* call back */
    error = archiveInfo->archiveNewFileFunction(archiveInfo->fileName,
                                                size,
                                                (archiveInfo->jobOptions->archivePartSize > 0)?archiveInfo->partNumber:-1,
                                                lastPartFlag,
                                                archiveInfo->archiveNewFileUserData
                                               );
    if (error != ERROR_NONE)
    {
      String_delete(archiveInfo->fileName);
      archiveInfo->fileOpenFlag = FALSE;
      return error;
    }
  }
  if (archiveInfo->jobOptions->archivePartSize > 0)
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
    closeArchiveFile(archiveInfo,FALSE);
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
    closeArchiveFile(archiveFileInfo->archiveInfo,FALSE);

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

Errors Archive_initAll(void)
{
  Errors error;

  /* init chunks*/
  error = Chunk_initAll((bool(*)(void*))File_eof,
                        (Errors(*)(void*,void*,ulong,ulong*))File_read,
                        (Errors(*)(void*,const void*,ulong))File_write,
                        (Errors(*)(void*,uint64*))File_tell,
                        (Errors(*)(void*,uint64))File_seek,
                        (uint64(*)(void*))File_getSize
                       );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

void Archive_doneAll(void)
{
}

Errors Archive_create(ArchiveInfo            *archiveInfo,
                      ArchiveNewFileFunction archiveNewFileFunction,
                      void                   *archiveNewFileUserData,
                      JobOptions             *jobOptions
                     )
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveNewFileFunction != NULL);
  assert(jobOptions != NULL);

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(jobOptions->cryptAlgorithm,&archiveInfo->blockLength);
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
  archiveInfo->jobOptions              = jobOptions;

  archiveInfo->partNumber              = 0;
  archiveInfo->fileOpenFlag            = FALSE;
  archiveInfo->fileName                = NULL;

  archiveInfo->nextChunkHeaderReadFlag = FALSE;

  return ERROR_NONE;
}

Errors Archive_open(ArchiveInfo  *archiveInfo,
                    const String archiveFileName,
                    JobOptions   *jobOptions
                   )
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveFileName != NULL);

  /* init */
  archiveInfo->archiveNewFileFunction  = NULL;  
  archiveInfo->archiveNewFileUserData  = NULL;  
  archiveInfo->jobOptions              = jobOptions;

  archiveInfo->partNumber              = 0;
  archiveInfo->fileOpenFlag            = TRUE;
  archiveInfo->fileName                = String_duplicate(archiveFileName);

  archiveInfo->nextChunkHeaderReadFlag = FALSE;

  /* open file */
  error = File_open(&archiveInfo->fileHandle,archiveInfo->fileName,FILE_OPENMODE_READ);
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

  /* find next file, directory, link, special chunk */
  chunkHeaderFoundFlag = FALSE;
  while (!File_eof(&archiveInfo->fileHandle) && !chunkHeaderFoundFlag)
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      return error;
    }

    chunkHeaderFoundFlag = (   (chunkHeader.id == CHUNK_ID_FILE)
                            || (chunkHeader.id == CHUNK_ID_DIRECTORY)
                            || (chunkHeader.id == CHUNK_ID_LINK)
                            || (chunkHeader.id == CHUNK_ID_SPECIAL)
                           );

    if (!chunkHeaderFoundFlag)
    {
      error = Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
      if (error != ERROR_NONE)
      {
        return error;
      }
      printWarning("Skipped unexpected chunk '%s'\n",Chunk_idToString(chunkHeader.id));
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
    closeArchiveFile(archiveInfo,TRUE);
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

  /* init crypt password (if needed) */
  if (!initCryptPassword(archiveInfo->jobOptions->cryptAlgorithm,
                         &archiveInfo->jobOptions->cryptPassword
                        )
     )
  {
    return ERROR_NO_PASSWORD;
  }

  /* init archive file info */
  archiveFileInfo->archiveInfo                         = archiveInfo;
  archiveFileInfo->mode                                = FILE_MODE_WRITE;

  archiveFileInfo->cryptAlgorithm                      = archiveInfo->jobOptions->cryptAlgorithm;
  archiveFileInfo->blockLength                         = archiveInfo->blockLength;

  archiveFileInfo->fileType                            = FILE_TYPE_FILE;

  archiveFileInfo->file.compressAlgorithm              = (fileInfo->size > globalOptions.compressMinFileSize)?archiveInfo->jobOptions->compressAlgorithm:COMPRESS_ALGORITHM_NONE;

  archiveFileInfo->file.chunkFile.compressAlgorithm    = archiveFileInfo->file.compressAlgorithm;
  archiveFileInfo->file.chunkFile.cryptAlgorithm       = archiveInfo->jobOptions->cryptAlgorithm;

  archiveFileInfo->file.chunkFileEntry.size            = fileInfo->size;
  archiveFileInfo->file.chunkFileEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveFileInfo->file.chunkFileEntry.timeModified    = fileInfo->timeModified;
  archiveFileInfo->file.chunkFileEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveFileInfo->file.chunkFileEntry.userId          = fileInfo->userId;
  archiveFileInfo->file.chunkFileEntry.groupId         = fileInfo->groupId;
  archiveFileInfo->file.chunkFileEntry.permission      = fileInfo->permission;
  archiveFileInfo->file.chunkFileEntry.name            = String_duplicate(name);

  archiveFileInfo->file.chunkFileData.fragmentOffset   = 0;
  archiveFileInfo->file.chunkFileData.fragmentSize     = 0;

  archiveFileInfo->file.createdFlag                    = FALSE;
  archiveFileInfo->file.headerLength                   = 0;
  archiveFileInfo->file.headerWrittenFlag              = FALSE;

  archiveFileInfo->file.buffer                         = NULL;
  archiveFileInfo->file.bufferLength                   = 0;

  /* init file-chunk */
  error = Chunk_init(&archiveFileInfo->file.chunkInfoFile,
                     NULL,
                     &archiveInfo->fileHandle,
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
                    archiveInfo->jobOptions->cryptAlgorithm,
                    archiveInfo->jobOptions->cryptPassword
                   );
  if (error != ERROR_NONE)
  {
    free(archiveFileInfo->file.buffer);
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }
  error = Crypt_new(&archiveFileInfo->file.cryptInfoFileData,
                    archiveInfo->jobOptions->cryptAlgorithm,
                    archiveInfo->jobOptions->cryptPassword
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }
  error = Crypt_new(&archiveFileInfo->file.cryptInfoData,
                    archiveInfo->jobOptions->cryptAlgorithm,
                    archiveInfo->jobOptions->cryptPassword
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
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
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    return ERROR_COMPRESS_ERROR;
  }

  /* init entry/dat file-chunks */
  error = Chunk_init(&archiveFileInfo->file.chunkInfoFileEntry,
                     &archiveFileInfo->file.chunkInfoFile,
                     &archiveInfo->fileHandle,
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
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }
  error = Chunk_init(&archiveFileInfo->file.chunkInfoFileData,
                     &archiveFileInfo->file.chunkInfoFile,
                     &archiveInfo->fileHandle,
                     archiveFileInfo->blockLength,
                     &archiveFileInfo->file.cryptInfoFileData
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->file.chunkInfoFileEntry);
    Compress_delete(&archiveFileInfo->file.compressInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
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
                                 const String    directoryName,
                                 FileInfo        *fileInfo
                                )
{
  Errors error;
  ulong  length;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init crypt password (if needed) */
  if (!initCryptPassword(archiveInfo->jobOptions->cryptAlgorithm,
                         &archiveInfo->jobOptions->cryptPassword
                        )
     )
  {
    return ERROR_NO_PASSWORD;
  }

  /* init archive file info */
  archiveFileInfo->archiveInfo                                   = archiveInfo;
  archiveFileInfo->mode                                          = FILE_MODE_WRITE;

  archiveFileInfo->cryptAlgorithm                                = archiveInfo->jobOptions->cryptAlgorithm;
  archiveFileInfo->blockLength                                   = archiveInfo->blockLength;

  archiveFileInfo->fileType                                      = FILE_TYPE_DIRECTORY;

  archiveFileInfo->directory.chunkDirectory.cryptAlgorithm       = archiveInfo->jobOptions->cryptAlgorithm;

  archiveFileInfo->directory.chunkDirectoryEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveFileInfo->directory.chunkDirectoryEntry.timeModified    = fileInfo->timeModified;
  archiveFileInfo->directory.chunkDirectoryEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveFileInfo->directory.chunkDirectoryEntry.userId          = fileInfo->userId;
  archiveFileInfo->directory.chunkDirectoryEntry.groupId         = fileInfo->groupId;
  archiveFileInfo->directory.chunkDirectoryEntry.permission      = fileInfo->permission;
  archiveFileInfo->directory.chunkDirectoryEntry.name            = String_duplicate(directoryName);

  /* init directory chunk */
  error = Chunk_init(&archiveFileInfo->directory.chunkInfoDirectory,
                     NULL,
                     &archiveInfo->fileHandle,
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
                    archiveInfo->jobOptions->cryptAlgorithm,
                    archiveInfo->jobOptions->cryptPassword
                   );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
    String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
    return error;
  }

  /* init directory entry chunk */
  error = Chunk_init(&archiveFileInfo->directory.chunkInfoDirectoryEntry,
                     &archiveFileInfo->directory.chunkInfoDirectory,
                     &archiveInfo->fileHandle,
                     archiveFileInfo->blockLength,
                     &archiveFileInfo->directory.cryptInfoDirectoryEntry
                    );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
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
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
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
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
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
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
    String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
    return error;
  }

  /* close chunks */
  error = Chunk_close(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
    String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
    return error;
  }
  error = Chunk_close(&archiveFileInfo->directory.chunkInfoDirectory);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
    String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
    return error;
  }

  return ERROR_NONE;
}

Errors Archive_newLinkEntry(ArchiveInfo     *archiveInfo,
                            ArchiveFileInfo *archiveFileInfo,
                            const String    linkName,
                            const String    destinationName,
                            FileInfo        *fileInfo
                           )
{
  Errors error;
  ulong  length;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init crypt password (if needed) */
  if (!initCryptPassword(archiveInfo->jobOptions->cryptAlgorithm,
                         &archiveInfo->jobOptions->cryptPassword
                        )
     )
  {
    return ERROR_NO_PASSWORD;
  }

  /* init archive file info */
  archiveFileInfo->archiveInfo                         = archiveInfo;
  archiveFileInfo->mode                                = FILE_MODE_WRITE;

  archiveFileInfo->cryptAlgorithm                      = archiveInfo->jobOptions->cryptAlgorithm;
  archiveFileInfo->blockLength                         = archiveInfo->blockLength;

  archiveFileInfo->fileType                            = FILE_TYPE_LINK;

  archiveFileInfo->link.chunkLink.cryptAlgorithm       = archiveInfo->jobOptions->cryptAlgorithm;

  archiveFileInfo->link.chunkLinkEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveFileInfo->link.chunkLinkEntry.timeModified    = fileInfo->timeModified;
  archiveFileInfo->link.chunkLinkEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveFileInfo->link.chunkLinkEntry.userId          = fileInfo->userId;
  archiveFileInfo->link.chunkLinkEntry.groupId         = fileInfo->groupId;
  archiveFileInfo->link.chunkLinkEntry.permission      = fileInfo->permission;
  archiveFileInfo->link.chunkLinkEntry.name            = String_duplicate(linkName);
  archiveFileInfo->link.chunkLinkEntry.destinationName = String_duplicate(destinationName);

  /* init link-chunk */
  error = Chunk_init(&archiveFileInfo->link.chunkInfoLink,
                     NULL,
                     &archiveInfo->fileHandle,
                     0,
                     NULL
                    );
  if (error != ERROR_NONE)
  {
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.destinationName);
    return error;
  }

  /* init crypt */
  error = Crypt_new(&archiveFileInfo->link.cryptInfoLinkEntry,
                    archiveInfo->jobOptions->cryptAlgorithm,
                    archiveInfo->jobOptions->cryptPassword
                   );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.destinationName);
    return error;
  }

  /* init link entry chunk */
  error = Chunk_init(&archiveFileInfo->link.chunkInfoLinkEntry,
                     &archiveFileInfo->link.chunkInfoLink,
                     &archiveInfo->fileHandle,
                     archiveFileInfo->blockLength,
                     &archiveFileInfo->link.cryptInfoLinkEntry
                    );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.destinationName);
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
    Chunk_done(&archiveFileInfo->link.chunkInfoLinkEntry);
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.destinationName);
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
    Chunk_done(&archiveFileInfo->link.chunkInfoLinkEntry);
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.destinationName);
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
    Chunk_done(&archiveFileInfo->link.chunkInfoLinkEntry);
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.destinationName);
    return error;
  }

  /* close chunks */
  error = Chunk_close(&archiveFileInfo->link.chunkInfoLinkEntry);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->link.chunkInfoLinkEntry);
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.destinationName);
    return error;
  }
  error = Chunk_close(&archiveFileInfo->link.chunkInfoLink);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->link.chunkInfoLinkEntry);
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    String_delete(archiveFileInfo->link.chunkLinkEntry.name);
    String_delete(archiveFileInfo->link.chunkLinkEntry.destinationName);
    return error;
  }

  return ERROR_NONE;
}

Errors Archive_newSpecialEntry(ArchiveInfo     *archiveInfo,
                               ArchiveFileInfo *archiveFileInfo,
                               const String    specialName,
                               FileInfo        *fileInfo
                              )
{
  Errors error;
  ulong  length;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init crypt password (if needed) */
  if (!initCryptPassword(archiveInfo->jobOptions->cryptAlgorithm,
                         &archiveInfo->jobOptions->cryptPassword
                        )
     )
  {
    return ERROR_NO_PASSWORD;
  }

  /* init archive file info */
  archiveFileInfo->archiveInfo                               = archiveInfo;
  archiveFileInfo->mode                                      = FILE_MODE_WRITE;

  archiveFileInfo->cryptAlgorithm                            = archiveInfo->jobOptions->cryptAlgorithm;
  archiveFileInfo->blockLength                               = archiveInfo->blockLength;

  archiveFileInfo->fileType                                  = FILE_TYPE_SPECIAL;

  archiveFileInfo->special.chunkSpecial.cryptAlgorithm       = archiveInfo->jobOptions->cryptAlgorithm;

  archiveFileInfo->special.chunkSpecialEntry.specialType     = fileInfo->specialType;
  archiveFileInfo->special.chunkSpecialEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveFileInfo->special.chunkSpecialEntry.timeModified    = fileInfo->timeModified;
  archiveFileInfo->special.chunkSpecialEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveFileInfo->special.chunkSpecialEntry.userId          = fileInfo->userId;
  archiveFileInfo->special.chunkSpecialEntry.groupId         = fileInfo->groupId;
  archiveFileInfo->special.chunkSpecialEntry.permission      = fileInfo->permission;
  archiveFileInfo->special.chunkSpecialEntry.major           = fileInfo->major;
  archiveFileInfo->special.chunkSpecialEntry.minor           = fileInfo->minor;
  archiveFileInfo->special.chunkSpecialEntry.name            = String_duplicate(specialName);

  /* init special-chunk */
  error = Chunk_init(&archiveFileInfo->special.chunkInfoSpecial,
                     NULL,
                     &archiveInfo->fileHandle,
                     0,
                     NULL
                    );
  if (error != ERROR_NONE)
  {
    String_delete(archiveFileInfo->special.chunkSpecialEntry.name);
    return error;
  }

  /* init crypt */
  error = Crypt_new(&archiveFileInfo->special.cryptInfoSpecialEntry,
                    archiveInfo->jobOptions->cryptAlgorithm,
                    archiveInfo->jobOptions->cryptPassword
                   );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    String_delete(archiveFileInfo->special.chunkSpecialEntry.name);
    return error;
  }

  /* init special entry chunk */
  error = Chunk_init(&archiveFileInfo->special.chunkInfoSpecialEntry,
                     &archiveFileInfo->special.chunkInfoSpecial,
                     &archiveInfo->fileHandle,
                     archiveFileInfo->blockLength,
                     &archiveFileInfo->special.cryptInfoSpecialEntry
                    );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->special.cryptInfoSpecialEntry);
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    String_delete(archiveFileInfo->special.chunkSpecialEntry.name);
    return error;
  }

  /* calculate length */
  length = Chunk_getSize(CHUNK_DEFINITION_SPECIAL,      0,                           &archiveFileInfo->special.chunkSpecial     )+
           Chunk_getSize(CHUNK_DEFINITION_SPECIAL_ENTRY,archiveFileInfo->blockLength,&archiveFileInfo->special.chunkSpecialEntry);

  /* ensure space in archive */
  error = ensureArchiveSpace(archiveFileInfo->archiveInfo,
                             length
                            );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecialEntry);
    Crypt_delete(&archiveFileInfo->special.cryptInfoSpecialEntry);
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    String_delete(archiveFileInfo->special.chunkSpecialEntry.name);
    return error;
  }

  /* write special chunks */
  error = Chunk_create(&archiveFileInfo->special.chunkInfoSpecial,
                       CHUNK_ID_SPECIAL,
                       CHUNK_DEFINITION_SPECIAL,
                       Chunk_getSize(CHUNK_DEFINITION_SPECIAL,0,&archiveFileInfo->special.chunkSpecial),
                       &archiveFileInfo->special.chunkSpecial
                      );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecialEntry);
    Crypt_delete(&archiveFileInfo->special.cryptInfoSpecialEntry);
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    String_delete(archiveFileInfo->special.chunkSpecialEntry.name);
    return error;
  }
  error = Chunk_create(&archiveFileInfo->special.chunkInfoSpecialEntry,
                       CHUNK_ID_SPECIAL_ENTRY,
                       CHUNK_DEFINITION_SPECIAL_ENTRY,
                       Chunk_getSize(CHUNK_DEFINITION_SPECIAL_ENTRY,archiveFileInfo->blockLength,&archiveFileInfo->special.chunkSpecialEntry),
                       &archiveFileInfo->special.chunkSpecialEntry
                      );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecialEntry);
    Crypt_delete(&archiveFileInfo->special.cryptInfoSpecialEntry);
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    String_delete(archiveFileInfo->special.chunkSpecialEntry.name);
    return error;
  }

  /* close chunks */
  error = Chunk_close(&archiveFileInfo->special.chunkInfoSpecialEntry);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecialEntry);
    Crypt_delete(&archiveFileInfo->special.cryptInfoSpecialEntry);
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    String_delete(archiveFileInfo->special.chunkSpecialEntry.name);
    return error;
  }
  error = Chunk_close(&archiveFileInfo->special.chunkInfoSpecial);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecialEntry);
    Crypt_delete(&archiveFileInfo->special.cryptInfoSpecialEntry);
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    String_delete(archiveFileInfo->special.chunkSpecialEntry.name);
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

  /* find next file, directory, link, special chunk */
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
        && (chunkHeader.id != CHUNK_ID_SPECIAL)
       )
    {
      error = Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
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
         && (chunkHeader.id != CHUNK_ID_SPECIAL)
        );

  /* get file type */
  switch (chunkHeader.id)
  {
    case CHUNK_ID_FILE:      (*fileType) = FILE_TYPE_FILE;      break;
    case CHUNK_ID_DIRECTORY: (*fileType) = FILE_TYPE_DIRECTORY; break;
    case CHUNK_ID_LINK:      (*fileType) = FILE_TYPE_LINK;      break;
    case CHUNK_ID_SPECIAL:   (*fileType) = FILE_TYPE_SPECIAL;   break;
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
  ChunkHeader chunkHeader,subChunkHeader;
  bool        foundFileEntry,foundFileData;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo       = archiveInfo;
  archiveFileInfo->mode              = FILE_MODE_READ;
  archiveFileInfo->fileType          = FILE_TYPE_FILE;

  archiveFileInfo->file.buffer       = NULL;
  archiveFileInfo->file.bufferLength = 0;

  /* init file chunk */
  error = Chunk_init(&archiveFileInfo->file.chunkInfoFile,
                     NULL,
                     &archiveInfo->fileHandle,
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
      Chunk_done(&archiveFileInfo->file.chunkInfoFile);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_FILE)
    {
      error = Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveFileInfo->file.chunkInfoFile);
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
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }
  archiveFileInfo->cryptAlgorithm         = archiveFileInfo->file.chunkFile.cryptAlgorithm;
  archiveFileInfo->file.compressAlgorithm = archiveFileInfo->file.chunkFile.compressAlgorithm;

  /* init crypt password (if needed) */
  if (!initCryptPassword(archiveFileInfo->cryptAlgorithm,
                         &archiveInfo->jobOptions->cryptPassword
                        )
     )
  {
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return ERROR_NO_PASSWORD;
  }

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
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
                    archiveInfo->jobOptions->cryptPassword
                   );
  if (error != ERROR_NONE)
  {
    free(archiveFileInfo->file.buffer);
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }
  error = Crypt_new(&archiveFileInfo->file.cryptInfoFileData,
                    archiveFileInfo->cryptAlgorithm,
                    archiveInfo->jobOptions->cryptPassword
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }
  error = Crypt_new(&archiveFileInfo->file.cryptInfoData,
                    archiveFileInfo->cryptAlgorithm,
                    archiveInfo->jobOptions->cryptPassword
                   );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
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
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return ERROR_COMPRESS_ERROR;
  }

  /* init file entry/data chunks */
  error = Chunk_init(&archiveFileInfo->file.chunkInfoFileEntry,
                     &archiveFileInfo->file.chunkInfoFile,
                     &archiveInfo->fileHandle,
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
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }
  error = Chunk_init(&archiveFileInfo->file.chunkInfoFileData,
                     &archiveFileInfo->file.chunkInfoFile,
                     &archiveInfo->fileHandle,
                     archiveFileInfo->blockLength,
                     &archiveFileInfo->file.cryptInfoFileData
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->file.chunkInfoFileEntry);
    Compress_delete(&archiveFileInfo->file.compressInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }

  /* read file entry/data chunks */
  error = ERROR_NONE;
  foundFileEntry = FALSE;
  foundFileData  = FALSE;
  while (   !Chunk_eofSub(&archiveFileInfo->file.chunkInfoFile)
         && (error == ERROR_NONE)
         && (!foundFileEntry || !foundFileData)         
        )
  {
    error = Chunk_nextSub(&archiveFileInfo->file.chunkInfoFile,&subChunkHeader);
    if (error != ERROR_NONE)
    {
      break;
    }

    switch (subChunkHeader.id)
    {
      case CHUNK_ID_FILE_ENTRY:
        error = Chunk_open(&archiveFileInfo->file.chunkInfoFileEntry,
                           &subChunkHeader,
                           CHUNK_ID_FILE_ENTRY,
                           CHUNK_DEFINITION_FILE_ENTRY,
                           subChunkHeader.size,
                           &archiveFileInfo->file.chunkFileEntry
                          );
        if (error != ERROR_NONE)
        {
          break;
        }

        String_set(name,archiveFileInfo->file.chunkFileEntry.name);
        fileInfo->type            = FILE_TYPE_FILE;
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
                           &subChunkHeader,
                           CHUNK_ID_FILE_DATA,
                           CHUNK_DEFINITION_FILE_DATA,
                           Chunk_getSize(CHUNK_DEFINITION_FILE_DATA,archiveFileInfo->blockLength,NULL),
                           &archiveFileInfo->file.chunkFileData
                          );
        if (error != ERROR_NONE)
        {
          break;
        }

        if (fragmentOffset != NULL) (*fragmentOffset) = archiveFileInfo->file.chunkFileData.fragmentOffset;
        if (fragmentSize   != NULL) (*fragmentSize)   = archiveFileInfo->file.chunkFileData.fragmentSize;

        foundFileData = TRUE;
        break;
      default:
        error = Chunk_skipSub(&archiveFileInfo->file.chunkInfoFile,&subChunkHeader);
        if (error != ERROR_NONE)
        {
          break;
        }
        break;
    }
  }
  if ((error != ERROR_NONE) || !foundFileEntry || !foundFileData)
  {
    if (foundFileEntry) String_delete(archiveFileInfo->file.chunkFileEntry.name);

    Chunk_done(&archiveFileInfo->file.chunkInfoFileData);
    Chunk_done(&archiveFileInfo->file.chunkInfoFileEntry);
    Compress_delete(&archiveFileInfo->file.compressInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);

    if      (error != ERROR_NONE) return error;
    else if (!foundFileEntry)     return ERROR_NO_FILE_ENTRY;
    else if (!foundFileData)      return ERROR_NO_FILE_DATA;
    HALT_INTERNAL_ERROR_UNREACHABLE();
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
                                  String          directoryName,
                                  FileInfo        *fileInfo
                                 )
{
  Errors      error;
  ChunkHeader chunkHeader,subChunkHeader;
  bool        foundDirectoryEntry;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo = archiveInfo;
  archiveFileInfo->mode        = FILE_MODE_READ;
  archiveFileInfo->fileType    = FILE_TYPE_DIRECTORY;

  /* init file chunk */
  error = Chunk_init(&archiveFileInfo->directory.chunkInfoDirectory,
                     NULL,
                     &archiveInfo->fileHandle,
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
      Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_DIRECTORY)
    {
      error = Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
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
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }
  archiveFileInfo->cryptAlgorithm = archiveFileInfo->directory.chunkDirectory.cryptAlgorithm;

  /* init crypt password (if needed) */
  if (!initCryptPassword(archiveFileInfo->cryptAlgorithm,
                         &archiveInfo->jobOptions->cryptPassword
                        )
     )
  {
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return ERROR_NO_PASSWORD;
  }

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }
  assert(archiveFileInfo->blockLength > 0);

  /* init crypt */
  error = Crypt_new(&archiveFileInfo->directory.cryptInfoDirectoryEntry,
                    archiveFileInfo->cryptAlgorithm,
                    archiveInfo->jobOptions->cryptPassword
                   );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }

  /* init directory entry */
  error = Chunk_init(&archiveFileInfo->directory.chunkInfoDirectoryEntry,
                     &archiveFileInfo->directory.chunkInfoDirectory,
                     &archiveInfo->fileHandle,
                     archiveFileInfo->blockLength,
                     &archiveFileInfo->directory.cryptInfoDirectoryEntry
                    );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }

  /* read directory entry */
  error = ERROR_NONE;
  foundDirectoryEntry = FALSE;
  while (   !Chunk_eofSub(&archiveFileInfo->directory.chunkInfoDirectory)
         && (error == ERROR_NONE)
         && !foundDirectoryEntry
        )
  {
    error = Chunk_nextSub(&archiveFileInfo->directory.chunkInfoDirectory,&subChunkHeader);
    if (error != ERROR_NONE)
    {
      break;
    }

    switch (subChunkHeader.id)
    {
      case CHUNK_ID_DIRECTORY_ENTRY:
        error = Chunk_open(&archiveFileInfo->directory.chunkInfoDirectoryEntry,
                           &subChunkHeader,
                           CHUNK_ID_DIRECTORY_ENTRY,
                           CHUNK_DEFINITION_DIRECTORY_ENTRY,
                           subChunkHeader.size,
                           &archiveFileInfo->directory.chunkDirectoryEntry
                          );
        if (error != ERROR_NONE)
        {
          break;
        }

        String_set(directoryName,archiveFileInfo->directory.chunkDirectoryEntry.name);
        fileInfo->type            = FILE_TYPE_DIRECTORY;
        fileInfo->timeLastAccess  = archiveFileInfo->directory.chunkDirectoryEntry.timeLastAccess;
        fileInfo->timeModified    = archiveFileInfo->directory.chunkDirectoryEntry.timeModified;
        fileInfo->timeLastChanged = archiveFileInfo->directory.chunkDirectoryEntry.timeLastChanged;
        fileInfo->userId          = archiveFileInfo->directory.chunkDirectoryEntry.userId;
        fileInfo->groupId         = archiveFileInfo->directory.chunkDirectoryEntry.groupId;
        fileInfo->permission      = archiveFileInfo->directory.chunkDirectoryEntry.permission;

        foundDirectoryEntry = TRUE;
        break;
      default:
        error = Chunk_skipSub(&archiveFileInfo->directory.chunkInfoDirectory,&subChunkHeader);
        if (error != ERROR_NONE)
        {
          break;
        }
        break;
    }
  }
  if (!foundDirectoryEntry)
  {
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
    Crypt_delete(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);

    if      (error != ERROR_NONE)  return error;
    else if (!foundDirectoryEntry) return ERROR_NO_DIRECTORY_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveFileInfo->cryptAlgorithm;

  return ERROR_NONE;
}

Errors Archive_readLinkEntry(ArchiveInfo     *archiveInfo,
                             ArchiveFileInfo *archiveFileInfo,
                             CryptAlgorithms *cryptAlgorithm,
                             String          linkName,
                             String          destinationName,
                             FileInfo        *fileInfo
                            )
{
  Errors      error;
  ChunkHeader chunkHeader,subChunkHeader;
  bool        foundLinkEntry;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo = archiveInfo;
  archiveFileInfo->mode        = FILE_MODE_READ;
  archiveFileInfo->fileType    = FILE_TYPE_LINK;

  /* init file chunk */
  error = Chunk_init(&archiveFileInfo->link.chunkInfoLink,
                     NULL,
                     &archiveInfo->fileHandle,
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
      Chunk_done(&archiveFileInfo->link.chunkInfoLink);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_LINK)
    {
      Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveFileInfo->link.chunkInfoLink);
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
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }
  archiveFileInfo->cryptAlgorithm = archiveFileInfo->link.chunkLink.cryptAlgorithm;

  /* init crypt password (if needed) */
  if (!initCryptPassword(archiveFileInfo->cryptAlgorithm,
                         &archiveInfo->jobOptions->cryptPassword
                        )
     )
  {
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return ERROR_NO_PASSWORD;
  }

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }
  assert(archiveFileInfo->blockLength > 0);

  /* init crypt */
  error = Crypt_new(&archiveFileInfo->link.cryptInfoLinkEntry,
                    archiveFileInfo->cryptAlgorithm,
                    archiveInfo->jobOptions->cryptPassword
                   );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }

  /* init link entry */
  error = Chunk_init(&archiveFileInfo->link.chunkInfoLinkEntry,
                     &archiveFileInfo->link.chunkInfoLink,
                     &archiveInfo->fileHandle,
                     archiveFileInfo->blockLength,
                     &archiveFileInfo->link.cryptInfoLinkEntry
                    );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }

  /* read link entry */
  error = ERROR_NONE;
  foundLinkEntry = FALSE;
  while (   !Chunk_eofSub(&archiveFileInfo->link.chunkInfoLink)
         && (error == ERROR_NONE)
         && !foundLinkEntry         
        )
  {
    error = Chunk_nextSub(&archiveFileInfo->link.chunkInfoLink,&subChunkHeader);
    if (error != ERROR_NONE)
    {
      break;
    }

    switch (subChunkHeader.id)
    {
      case CHUNK_ID_LINK_ENTRY:
        error = Chunk_open(&archiveFileInfo->link.chunkInfoLinkEntry,
                           &subChunkHeader,
                           CHUNK_ID_LINK_ENTRY,
                           CHUNK_DEFINITION_LINK_ENTRY,
                           subChunkHeader.size,
                           &archiveFileInfo->link.chunkLinkEntry
                          );
        if (error != ERROR_NONE)
        {
          break;
        }

        String_set(linkName,archiveFileInfo->link.chunkLinkEntry.name);
        String_set(destinationName,archiveFileInfo->link.chunkLinkEntry.destinationName);
        fileInfo->type            = FILE_TYPE_LINK;
        fileInfo->timeLastAccess  = archiveFileInfo->link.chunkLinkEntry.timeLastAccess;
        fileInfo->timeModified    = archiveFileInfo->link.chunkLinkEntry.timeModified;
        fileInfo->timeLastChanged = archiveFileInfo->link.chunkLinkEntry.timeLastChanged;
        fileInfo->userId          = archiveFileInfo->link.chunkLinkEntry.userId;
        fileInfo->groupId         = archiveFileInfo->link.chunkLinkEntry.groupId;
        fileInfo->permission      = archiveFileInfo->link.chunkLinkEntry.permission;

        foundLinkEntry = TRUE;
        break;
      default:
        error = Chunk_skipSub(&archiveFileInfo->link.chunkInfoLink,&subChunkHeader);
        if (error != ERROR_NONE)
        {
          break;
        }
        break;
    }
  }
  if (!foundLinkEntry)
  {
    Chunk_done(&archiveFileInfo->link.chunkInfoLinkEntry);
    Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);

    if      (error != ERROR_NONE) return error;
    else if (!foundLinkEntry)     return ERROR_NO_LINK_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveFileInfo->cryptAlgorithm;

  return ERROR_NONE;
}

Errors Archive_readSpecialEntry(ArchiveInfo     *archiveInfo,
                                ArchiveFileInfo *archiveFileInfo,
                                CryptAlgorithms *cryptAlgorithm,
                                String          specialName,
                                FileInfo        *fileInfo
                               )
{
  Errors      error;
  ChunkHeader chunkHeader,subChunkHeader;
  bool        foundSpecialEntry;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo = archiveInfo;
  archiveFileInfo->mode        = FILE_MODE_READ;
  archiveFileInfo->fileType    = FILE_TYPE_SPECIAL;

  /* init file chunk */
  error = Chunk_init(&archiveFileInfo->special.chunkInfoSpecial,
                     NULL,
                     &archiveInfo->fileHandle,
                     0,
                     NULL
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* find next special chunk */
  do
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_SPECIAL)
    {
      Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_SPECIAL);

  /* read special chunk */
  error = Chunk_open(&archiveFileInfo->special.chunkInfoSpecial,
                     &chunkHeader,
                     CHUNK_ID_SPECIAL,
                     CHUNK_DEFINITION_SPECIAL,
                     Chunk_getSize(CHUNK_DEFINITION_SPECIAL,0,NULL),
                     &archiveFileInfo->special.chunkSpecial
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }
  archiveFileInfo->cryptAlgorithm = archiveFileInfo->special.chunkSpecial.cryptAlgorithm;

  /* init crypt password (if needed) */
  if (!initCryptPassword(archiveFileInfo->cryptAlgorithm,
                         &archiveInfo->jobOptions->cryptPassword
                        )
     )
  {
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return ERROR_NO_PASSWORD;
  }

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }
  assert(archiveFileInfo->blockLength > 0);

  /* init crypt */
  error = Crypt_new(&archiveFileInfo->special.cryptInfoSpecialEntry,
                    archiveFileInfo->cryptAlgorithm,
                    archiveInfo->jobOptions->cryptPassword
                   );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }

  /* init special entry */
  error = Chunk_init(&archiveFileInfo->special.chunkInfoSpecialEntry,
                     &archiveFileInfo->special.chunkInfoSpecial,
                     &archiveInfo->fileHandle,
                     archiveFileInfo->blockLength,
                     &archiveFileInfo->special.cryptInfoSpecialEntry
                    );
  if (error != ERROR_NONE)
  {
    Crypt_delete(&archiveFileInfo->special.cryptInfoSpecialEntry);
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }

  /* read special entry */
  error = ERROR_NONE;
  foundSpecialEntry = FALSE;
  while (   !Chunk_eofSub(&archiveFileInfo->special.chunkInfoSpecial)
         && (error == ERROR_NONE)
         && !foundSpecialEntry         
        )
  {
    error = Chunk_nextSub(&archiveFileInfo->special.chunkInfoSpecial,&subChunkHeader);
    if (error != ERROR_NONE)
    {
      break;
    }

    switch (subChunkHeader.id)
    {
      case CHUNK_ID_SPECIAL_ENTRY:
        error = Chunk_open(&archiveFileInfo->special.chunkInfoSpecialEntry,
                           &subChunkHeader,
                           CHUNK_ID_SPECIAL_ENTRY,
                           CHUNK_DEFINITION_SPECIAL_ENTRY,
                           subChunkHeader.size,
                           &archiveFileInfo->special.chunkSpecialEntry
                          );
        if (error != ERROR_NONE)
        {
          break;
        }

        String_set(specialName,archiveFileInfo->special.chunkSpecialEntry.name);
        fileInfo->type            = FILE_TYPE_SPECIAL;
        fileInfo->timeLastAccess  = archiveFileInfo->special.chunkSpecialEntry.timeLastAccess;
        fileInfo->timeModified    = archiveFileInfo->special.chunkSpecialEntry.timeModified;
        fileInfo->timeLastChanged = archiveFileInfo->special.chunkSpecialEntry.timeLastChanged;
        fileInfo->userId          = archiveFileInfo->special.chunkSpecialEntry.userId;
        fileInfo->groupId         = archiveFileInfo->special.chunkSpecialEntry.groupId;
        fileInfo->permission      = archiveFileInfo->special.chunkSpecialEntry.permission;
        fileInfo->specialType     = archiveFileInfo->special.chunkSpecialEntry.specialType;
        fileInfo->major           = archiveFileInfo->special.chunkSpecialEntry.major;
        fileInfo->minor           = archiveFileInfo->special.chunkSpecialEntry.minor;

        foundSpecialEntry = TRUE;
        break;
      default:
        error = Chunk_skipSub(&archiveFileInfo->special.chunkInfoSpecial,&subChunkHeader);
        if (error != ERROR_NONE)
        {
          break;
        }
        break;
    }
  }
  if (!foundSpecialEntry)
  {
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecialEntry);
    Crypt_delete(&archiveFileInfo->special.cryptInfoSpecialEntry);
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);

    if      (error != ERROR_NONE) return error;
    else if (!foundSpecialEntry)  return ERROR_NO_SPECIAL_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
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
        case FILE_TYPE_FILE:
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
          Chunk_done(&archiveFileInfo->file.chunkInfoFileData);
          Chunk_done(&archiveFileInfo->file.chunkInfoFileEntry);
          Compress_delete(&archiveFileInfo->file.compressInfoData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
          Chunk_done(&archiveFileInfo->file.chunkInfoFile);
          free(archiveFileInfo->file.buffer);
          String_delete(archiveFileInfo->file.chunkFileEntry.name);
          break;
        case FILE_TYPE_DIRECTORY:
          String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
          break;
        case FILE_TYPE_LINK:
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
          Chunk_done(&archiveFileInfo->link.chunkInfoLinkEntry);
          Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
          Chunk_done(&archiveFileInfo->link.chunkInfoLink);
          String_delete(archiveFileInfo->link.chunkLinkEntry.name);
          String_delete(archiveFileInfo->link.chunkLinkEntry.destinationName);
          break;
        case FILE_TYPE_SPECIAL:
          /* close chunks */
          error = Chunk_close(&archiveFileInfo->special.chunkInfoSpecialEntry);
          if (error != ERROR_NONE)
          {
            return error;
          }
          error = Chunk_close(&archiveFileInfo->special.chunkInfoSpecial);
          if (error != ERROR_NONE)
          {
            return error;
          }

          /* free resources */
          Chunk_done(&archiveFileInfo->special.chunkInfoSpecialEntry);
          Crypt_delete(&archiveFileInfo->special.cryptInfoSpecialEntry);
          Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
          String_delete(archiveFileInfo->special.chunkSpecialEntry.name);
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
        case FILE_TYPE_FILE:
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
          Chunk_done(&archiveFileInfo->file.chunkInfoFileData);
          Chunk_done(&archiveFileInfo->file.chunkInfoFileEntry);
          Compress_delete(&archiveFileInfo->file.compressInfoData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoFileData);
          Crypt_delete(&archiveFileInfo->file.cryptInfoFileEntry);
          Chunk_done(&archiveFileInfo->file.chunkInfoFile);
          free(archiveFileInfo->file.buffer);
          String_delete(archiveFileInfo->file.chunkFileEntry.name);
          break;
        case FILE_TYPE_DIRECTORY:
          String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
          break;
        case FILE_TYPE_LINK:
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
          Chunk_done(&archiveFileInfo->link.chunkInfoLinkEntry);
          Crypt_delete(&archiveFileInfo->link.cryptInfoLinkEntry);
          Chunk_done(&archiveFileInfo->link.chunkInfoLink);
          String_delete(archiveFileInfo->link.chunkLinkEntry.name);
          String_delete(archiveFileInfo->link.chunkLinkEntry.destinationName);
          break;
        case FILE_TYPE_SPECIAL:
          /* close chunks */
          error = Chunk_close(&archiveFileInfo->special.chunkInfoSpecialEntry);
          if (error != ERROR_NONE)
          {
            return error;
          }
          error = Chunk_close(&archiveFileInfo->special.chunkInfoSpecial);
          if (error != ERROR_NONE)
          {
            return error;
          }

          /* free resources */
          Chunk_done(&archiveFileInfo->special.chunkInfoSpecialEntry);
          Crypt_delete(&archiveFileInfo->special.cryptInfoSpecialEntry);
          Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
          String_delete(archiveFileInfo->special.chunkSpecialEntry.name);
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

  return File_getSize(&archiveFileInfo->archiveInfo->fileHandle);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
