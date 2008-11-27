/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/archive.c,v $
* $Revision: 1.2 $
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

typedef struct PasswordNode
{
  LIST_NODE_HEADER(struct PasswordNode);

  Password *password;
} PasswordNode;

typedef struct
{
  LIST_HEADER(PasswordNode);
} PasswordList;

typedef struct
{
  String             fileName;
  PasswordModes      passwordMode;
  const Password     *cryptPassword;
  const PasswordNode *passwordNode;
  bool               inputFlag;
} PasswordHandle;

/***************************** Variables *******************************/

/* list with all known decrption passwords */
LOCAL PasswordList decryptPasswordList;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : appendCryptPassword
* Purpose: append password to password list
* Input  : password - password
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendCryptPassword(Password *password)
{
  PasswordNode *passwordNode;

  assert(password != NULL);

  passwordNode = LIST_NEW_NODE(PasswordNode);
  if (passwordNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  passwordNode->password = password;

  List_append(&decryptPasswordList,passwordNode);
}

/***********************************************************************\
* Name   : freePasswordNode
* Purpose: free password node
* Input  : passwordNode - password node
*          userData     - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freePasswordNode(PasswordNode *passwordNode, void *userData)
{
  assert(passwordNode != NULL);

  UNUSED_VARIABLE(userData);

  Password_delete(passwordNode->password);
  LIST_DELETE_NODE(passwordNode);
}

/***********************************************************************\
* Name   : getCryptPassword
* Purpose: get crypt password if password not set
* Input  : fileName     - file name
*          jobOptions   - job options
*          passwordMode - password mode
* Output : -
* Return : password or NULL if no password given
* Notes  : -
\***********************************************************************/

LOCAL Password *getCryptPassword(const String     fileName,
                                 const JobOptions *jobOptions,
                                 PasswordModes    passwordMode
                                )
{
  Password *password;
  Errors   error;

  assert(fileName != NULL);
  assert(jobOptions != NULL);

  password = NULL;
  switch ((passwordMode != PASSWORD_MODE_DEFAULT)?passwordMode:jobOptions->cryptPasswordMode)
  {
    case PASSWORD_MODE_DEFAULT:
      if (globalOptions.cryptPassword != NULL)
      {
        password = Password_duplicate(globalOptions.cryptPassword);
      }
      else
      {
        password = Password_new();
        if (password == NULL)
        {
          return NULL;
        }
        error = inputCryptPassword(password,fileName,TRUE,TRUE);
        if (error != ERROR_NONE)
        {
          Password_delete(password);
          return NULL;
        }
      }
      break;
    case PASSWORD_MODE_ASK:
      password = Password_new();
      if (password == NULL)
      {
        return NULL;
      }
      error = inputCryptPassword(password,fileName,TRUE,TRUE);
      if (error != ERROR_NONE)
      {
        Password_delete(password);
        return NULL;
      }
      break;
    case PASSWORD_MODE_CONFIG:
      if (jobOptions->cryptPassword != NULL)
      {
        password = Password_duplicate(jobOptions->cryptPassword);
      }
      else
      {
        password = Password_new();
        if (password == NULL)
        {
          return NULL;
        }
        error = inputCryptPassword(password,fileName,TRUE,TRUE);
        if (error != ERROR_NONE)
        {
          Password_delete(password);
          return NULL;
        }
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        return NULL; /* not reached */
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return password;
}

/***********************************************************************\
* Name   : getNextCryptPassword
* Purpose: get next crypt password
* Input  : passwordHandle - password handle
* Output : -
* Return : password or NULL if no more passwords
* Notes  : -
\***********************************************************************/

LOCAL const Password *getNextCryptPassword(PasswordHandle *passwordHandle)
{
  Password *password;
  Errors   error;

  assert(passwordHandle != NULL);

  password = NULL;
  if      (passwordHandle->passwordNode != NULL)
  {
    /* next password from list */
    password = passwordHandle->passwordNode->password;
    passwordHandle->passwordNode = passwordHandle->passwordNode->next;
  }
  else if (!passwordHandle->inputFlag && (passwordHandle->passwordMode==PASSWORD_MODE_DEFAULT) && (globalOptions.cryptPassword!=NULL))
  {
    /* get default password */
    password = Password_duplicate(globalOptions.cryptPassword);

    /* add to password list */
    if (password != NULL)
    {
      appendCryptPassword(password);
    }

    passwordHandle->passwordMode = PASSWORD_MODE_ASK;
  }
  else if (!passwordHandle->inputFlag && (passwordHandle->passwordMode==PASSWORD_MODE_CONFIG) && (passwordHandle->cryptPassword!=NULL))
  {
    /* get config password */
    password = Password_duplicate(passwordHandle->cryptPassword);

    /* add to password list */
    if (password != NULL)
    {
      appendCryptPassword(password);
    }

    passwordHandle->passwordMode = PASSWORD_MODE_ASK;
  }
  else if (!passwordHandle->inputFlag && (passwordHandle->passwordMode==PASSWORD_MODE_ASK))
  {
    /* input password */
    password = Password_new();
    if (password == NULL)
    {
      return NULL;
    }
    error = inputCryptPassword(password,passwordHandle->fileName,FALSE,FALSE);
    if (error != ERROR_NONE)
    {
      Password_delete(password);
      return NULL;
    }

    /* add to password list */
    if (password != NULL)
    {
      appendCryptPassword(password);
    }

    passwordHandle->inputFlag = TRUE;
  }
  else
  {
    password = NULL;
  }

  return password;
}

/***********************************************************************\
* Name   : getFirstCryptPassword
* Purpose: get first crypt password
* Input  : fileName     - file name
*          jobOptions   - job options
*          passwordMode - password mode
* Output : passwordHandle - intialized password handle
* Return : password or NULL if no more passwords
* Notes  : -
\***********************************************************************/

LOCAL const Password *getFirstCryptPassword(PasswordHandle   *passwordHandle,
                                            const String     fileName,
                                            const JobOptions *jobOptions,
                                            PasswordModes    passwordMode
                                           )
{
  assert(passwordHandle != NULL);

  passwordHandle->fileName      = fileName;
  passwordHandle->passwordMode  = (passwordMode != PASSWORD_MODE_DEFAULT)?passwordMode:jobOptions->cryptPasswordMode;
  passwordHandle->cryptPassword = jobOptions->cryptPassword;
  passwordHandle->passwordNode  = decryptPasswordList.head;
  passwordHandle->inputFlag     = FALSE;

  return getNextCryptPassword(passwordHandle);
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
* Name   : readEncryptionKey
* Purpose: read encryption key
* Input  : archiveInfo - archive info block
*          chunkHeader - key chunk header
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors readEncryptionKey(ArchiveInfo       *archiveInfo,
                               const ChunkHeader *chunkHeader
                              )
{
  Errors    error;
  ChunkInfo chunkInfoKey;
  ChunkKey  chunkKey;

  assert(archiveInfo != NULL);
  assert(chunkHeader != NULL);

  /* create key chunk */
  error = Chunk_init(&chunkInfoKey,
                     NULL,
                     &archiveInfo->fileHandle,
                     0,
                     NULL
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* read key chunk */
  error = Chunk_open(&chunkInfoKey,
                     chunkHeader,
                     CHUNK_ID_KEY,
                     CHUNK_DEFINITION_KEY,
                     Chunk_getSize(CHUNK_DEFINITION_KEY,0,NULL),
                     &chunkKey
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkInfoKey);
    return error;
  }
  archiveInfo->cryptKeyDataLength = chunkHeader->size-chunkInfoKey.definitionSize;
  if (archiveInfo->cryptKeyData != NULL) free(archiveInfo->cryptKeyData);
  archiveInfo->cryptKeyData = malloc(archiveInfo->cryptKeyDataLength);
  if (archiveInfo->cryptKeyData == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  error = Chunk_readData(&chunkInfoKey,
                         archiveInfo->cryptKeyData,
                         archiveInfo->cryptKeyDataLength
                        );
  if (error != ERROR_NONE)
  {
    free(archiveInfo->cryptKeyData);
    Chunk_done(&chunkInfoKey);
    return error;
  }

  /* decrypt key */
  if (archiveInfo->cryptPassword == NULL) archiveInfo->cryptPassword = Password_new();
  error = Crypt_getDecryptKey(&archiveInfo->cryptKey,
                              archiveInfo->cryptKeyData,
                              archiveInfo->cryptKeyDataLength,
                              archiveInfo->cryptPassword
                             );
  if (error != ERROR_NONE)
  {
    Password_delete(archiveInfo->cryptPassword);
    free(archiveInfo->cryptKeyData);
    Chunk_done(&chunkInfoKey);
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeEncryptionKey
* Purpose: write new encryption key
* Input  : archiveInfo - archive info block
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

LOCAL Errors writeEncryptionKey(ArchiveInfo *archiveInfo)
{
  Errors    error;
  ChunkInfo chunkInfoKey;
  ChunkKey  chunkKey;

  assert(archiveInfo != NULL);

  /* create key chunk */
  error = Chunk_init(&chunkInfoKey,
                     NULL,
                     &archiveInfo->fileHandle,
                     0,
                     NULL
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* write encrypted encryption key */
  error = Chunk_create(&chunkInfoKey,
                       CHUNK_ID_KEY,
                       CHUNK_DEFINITION_KEY,
                       Chunk_getSize(CHUNK_DEFINITION_KEY,0,&chunkKey),
                       &chunkKey
                      );
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkInfoKey);
    return error;
  }
  error = Chunk_writeData(&chunkInfoKey,
                          archiveInfo->cryptKeyData,
                          archiveInfo->cryptKeyDataLength
                         );
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkInfoKey);
    return error;
  }

  /* free resources */
  error = Chunk_close(&chunkInfoKey);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkInfoKey);
    return error;
  }
  Chunk_done(&chunkInfoKey);

  return ERROR_NONE;
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
  error = File_getTmpFileName(archiveInfo->fileName,NULL,globalOptions.tmpDirectory);
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* create file */
  error = File_open(&archiveInfo->fileHandle,archiveInfo->fileName,FILE_OPENMODE_CREATE);
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* write encrypted key if asymmetric encryption enabled */
  if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
  {
    error = writeEncryptionKey(archiveInfo);
    if (error != ERROR_NONE)
    {
      File_close(&archiveInfo->fileHandle);
      return error;
    }
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
      archiveInfo->fileOpenFlag = FALSE;
      return error;
    }
  }
  if (archiveInfo->jobOptions->archivePartSize > 0)
  {
    archiveInfo->partNumber++;
  }

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
    while (Compress_getAvailableBlocks(&archiveFileInfo->file.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
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
    if (Compress_getAvailableBlocks(&archiveFileInfo->file.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) <= 0)
    {
      return ERROR_COMPRESS_EOF;
    }
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Archive_initAll(void)
{
  Errors error;

  /* initialize variables */
  List_init(&decryptPasswordList);

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
  /* free resources */
  List_done(&decryptPasswordList,(ListNodeFreeFunction)freePasswordNode,NULL);
}

Errors Archive_create(ArchiveInfo            *archiveInfo,
                      ArchiveNewFileFunction archiveNewFileFunction,
                      void                   *archiveNewFileUserData,
                      JobOptions             *jobOptions
                     )
{
  Errors error;
  bool   okFlag;
  ulong  maxCryptKeyDataLength;

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
  archiveInfo->passwordMode            = PASSWORD_MODE_DEFAULT;

  archiveInfo->cryptPassword           = NULL;
  archiveInfo->cryptType               = (jobOptions->cryptAlgorithm != CRYPT_ALGORITHM_NONE)?jobOptions->cryptType:CRYPT_TYPE_NONE;
  archiveInfo->cryptKeyData            = NULL;
  archiveInfo->cryptKeyDataLength      = 0;

  archiveInfo->partNumber              = 0;
  archiveInfo->fileOpenFlag            = FALSE;
  archiveInfo->fileName                = String_new();

  archiveInfo->nextChunkHeaderReadFlag = FALSE;

  /* init key (if asymmetric encryption used) */
  if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
  {
    /* check if public key available */
    if (jobOptions->cryptPublicKeyFileName == NULL)
    {
      String_delete(archiveInfo->fileName);
      return ERROR_NO_PUBLIC_KEY;
    }

    /* read public key */
    Crypt_initKey(&archiveInfo->cryptKey);
    error = Crypt_readKeyFile(&archiveInfo->cryptKey,
                              jobOptions->cryptPublicKeyFileName,
                              NULL
                             );
    if (error != ERROR_NONE)
    {
      String_delete(archiveInfo->fileName);
      return error;
    }

    /* create new random key for encryption */
    archiveInfo->cryptPassword = Password_new();
    if (archiveInfo->cryptPassword == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    maxCryptKeyDataLength = 2*MAX_PASSWORD_LENGTH;
    okFlag = FALSE;
    do
    {
      archiveInfo->cryptKeyData = malloc(maxCryptKeyDataLength);
      if (archiveInfo->cryptKeyData == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      error = Crypt_getRandomEncryptKey(&archiveInfo->cryptKey,
                                        jobOptions->cryptAlgorithm,
                                        archiveInfo->cryptPassword,
                                        maxCryptKeyDataLength,
                                        archiveInfo->cryptKeyData,
                                        &archiveInfo->cryptKeyDataLength
                                       );
      if (error != ERROR_NONE)
      {
        String_delete(archiveInfo->fileName);
        return error;
      }
      if (archiveInfo->cryptKeyDataLength < maxCryptKeyDataLength)
      {
        okFlag = TRUE;
      }
      else
      {
        free(archiveInfo->cryptKeyData);
        maxCryptKeyDataLength += 64;
      }
    }
    while (!okFlag);
#if 0
Password_dump(archiveInfo->cryptPassword);
{
int z;
byte *p=archiveInfo->cryptKeyData;
fprintf(stderr,"data: ");for (z=0;z<archiveInfo->cryptKeyDataLength;z++) fprintf(stderr,"%02x",p[z]); fprintf(stderr,"\n");
}
#endif /* 0 */
  }

  return ERROR_NONE;
}

Errors Archive_open(ArchiveInfo   *archiveInfo,
                    const String  archiveFileName,
                    JobOptions    *jobOptions,
                    PasswordModes passwordMode
                   )
{
  ChunkHeader    chunkHeader;
  Errors         error;
  PasswordHandle passwordHandle;
  const Password *password;
  bool           decryptedFlag;

  assert(archiveInfo != NULL);
  assert(archiveFileName != NULL);

  /* init */
  archiveInfo->archiveNewFileFunction  = NULL;
  archiveInfo->archiveNewFileUserData  = NULL;
  archiveInfo->jobOptions              = jobOptions;
  archiveInfo->passwordMode            = passwordMode;

  archiveInfo->cryptPassword           = NULL;
  archiveInfo->cryptType               = CRYPT_TYPE_NONE;
  archiveInfo->cryptKeyData            = NULL;
  archiveInfo->cryptKeyDataLength      = 0;

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

  /* check if key chunk -> read key for asymmetric encryption */
  error = getNextChunkHeader(archiveInfo,&chunkHeader);
  if (error != ERROR_NONE)
  {
    File_close(&archiveInfo->fileHandle);
    String_delete(archiveInfo->fileName);
    return error;
  }
  if (chunkHeader.id == CHUNK_ID_KEY)
  {
    /* check if private key available */
    if (jobOptions->cryptPrivateKeyFileName == NULL)
    {
      File_close(&archiveInfo->fileHandle);
      String_delete(archiveInfo->fileName);
      return ERROR_NO_PUBLIC_KEY;
    }

    /* read private key, try to read key with no password, all passwords */
    Crypt_initKey(&archiveInfo->cryptKey);
    decryptedFlag = FALSE;
    error = Crypt_readKeyFile(&archiveInfo->cryptKey,
                              jobOptions->cryptPrivateKeyFileName,
                              NULL
                             );
    if (error == ERROR_NONE)
    {
      decryptedFlag = TRUE;
    }
    password = getFirstCryptPassword(&passwordHandle,
                                     archiveInfo->fileName,
                                     archiveInfo->jobOptions,
                                     archiveInfo->passwordMode
                                    );
    while (   !decryptedFlag
           && (password != NULL)
          )
    {
      error = Crypt_readKeyFile(&archiveInfo->cryptKey,
                                jobOptions->cryptPrivateKeyFileName,
                                password
                               );
      if (error == ERROR_NONE)
      {
        decryptedFlag = TRUE;
      }
      else
      {
        /* next password */
        password = getNextCryptPassword(&passwordHandle);
      }
    }
    if (!decryptedFlag)
    {
      File_close(&archiveInfo->fileHandle);
      String_delete(archiveInfo->fileName);
      return error;
    }

    /* read encryption key */
    error = readEncryptionKey(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      File_close(&archiveInfo->fileHandle);
      String_delete(archiveInfo->fileName);
      return error;
    }
#if 0
Password_dump(archiveInfo->cryptPassword);
{
int z;
byte *p=archiveInfo->cryptKeyData;
fprintf(stderr,"data: ");for (z=0;z<archiveInfo->cryptKeyDataLength;z++) fprintf(stderr,"%02x",p[z]); fprintf(stderr,"\n");
}
#endif /* 0 */
    archiveInfo->cryptType = CRYPT_TYPE_ASYMMETRIC;
  }
  else
  {
    ungetNextChunkHeader(archiveInfo,&chunkHeader);
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

  if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
  {
    assert(archiveInfo->cryptKeyData != NULL);

    free(archiveInfo->cryptKeyData);
    Crypt_doneKey(&archiveInfo->cryptKey);
  }

  if (archiveInfo->cryptPassword  != NULL) Password_delete(archiveInfo->cryptPassword);
  if (archiveInfo->fileName != NULL) String_delete(archiveInfo->fileName);

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

  /* init crypt password */
  if ((archiveInfo->jobOptions->cryptAlgorithm != CRYPT_ALGORITHM_NONE) && (archiveInfo->cryptPassword == NULL))
  {
    archiveInfo->cryptPassword = getCryptPassword(archiveInfo->fileName,
                                                  archiveInfo->jobOptions,
                                                  archiveInfo->passwordMode
                                                 );
    if (archiveInfo->cryptPassword == NULL)
    {
      return ERROR_NO_CRYPT_PASSWORD;
    }
  }

  /* init archive file info */
  archiveFileInfo->archiveInfo                         = archiveInfo;
  archiveFileInfo->mode                                = ARCHIVE_FILE_MODE_WRITE;

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
  error = Crypt_init(&archiveFileInfo->file.cryptInfoFileEntry,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    free(archiveFileInfo->file.buffer);
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }
  error = Crypt_init(&archiveFileInfo->file.cryptInfoFileData,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveFileInfo->file.cryptInfoFileEntry);
    free(archiveFileInfo->file.buffer);
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    return error;
  }
  error = Crypt_init(&archiveFileInfo->file.cryptInfoData,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_done(&archiveFileInfo->file.cryptInfoFileEntry);
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
    Crypt_done(&archiveFileInfo->file.cryptInfoData);
    Crypt_done(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_done(&archiveFileInfo->file.cryptInfoFileEntry);
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
    Crypt_done(&archiveFileInfo->file.cryptInfoData);
    Crypt_done(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_done(&archiveFileInfo->file.cryptInfoFileEntry);
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
    Crypt_done(&archiveFileInfo->file.cryptInfoData);
    Crypt_done(&archiveFileInfo->file.cryptInfoFileData);
    Crypt_done(&archiveFileInfo->file.cryptInfoFileEntry);
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

  /* init crypt password */
  if ((archiveInfo->jobOptions->cryptAlgorithm != CRYPT_ALGORITHM_NONE) && (archiveInfo->cryptPassword == NULL))
  {
    archiveInfo->cryptPassword = getCryptPassword(archiveInfo->fileName,
                                                  archiveInfo->jobOptions,
                                                  archiveInfo->passwordMode
                                                 );
    if (archiveInfo->cryptPassword == NULL)
    {
      return ERROR_NO_CRYPT_PASSWORD;
    }
  }

  /* init archive file info */
  archiveFileInfo->archiveInfo                                   = archiveInfo;
  archiveFileInfo->mode                                          = ARCHIVE_FILE_MODE_WRITE;

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
  error = Crypt_init(&archiveFileInfo->directory.cryptInfoDirectoryEntry,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
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
    Crypt_done(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
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
    Crypt_done(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
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
    Crypt_done(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
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
    Crypt_done(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
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

  /* init crypt password */
  if ((archiveInfo->jobOptions->cryptAlgorithm != CRYPT_ALGORITHM_NONE) && (archiveInfo->cryptPassword == NULL))
  {
    archiveInfo->cryptPassword = getCryptPassword(archiveInfo->fileName,
                                                  archiveInfo->jobOptions,
                                                  archiveInfo->passwordMode
                                                 );
    if (archiveInfo->cryptPassword == NULL)
    {
      return ERROR_NO_CRYPT_PASSWORD;
    }
  }

  /* init archive file info */
  archiveFileInfo->archiveInfo                         = archiveInfo;
  archiveFileInfo->mode                                = ARCHIVE_FILE_MODE_WRITE;

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
  error = Crypt_init(&archiveFileInfo->link.cryptInfoLinkEntry,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
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
    Crypt_done(&archiveFileInfo->link.cryptInfoLinkEntry);
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
    Crypt_done(&archiveFileInfo->link.cryptInfoLinkEntry);
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
    Crypt_done(&archiveFileInfo->link.cryptInfoLinkEntry);
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
    Crypt_done(&archiveFileInfo->link.cryptInfoLinkEntry);
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

  /* init crypt password */
  if ((archiveInfo->jobOptions->cryptAlgorithm != CRYPT_ALGORITHM_NONE) && (archiveInfo->cryptPassword == NULL))
  {
    archiveInfo->cryptPassword = getCryptPassword(archiveInfo->fileName,
                                                  archiveInfo->jobOptions,
                                                  archiveInfo->passwordMode
                                                 );
    if (archiveInfo->cryptPassword == NULL)
    {
      return ERROR_NO_CRYPT_PASSWORD;
    }
  }

  /* init archive file info */
  archiveFileInfo->archiveInfo                               = archiveInfo;
  archiveFileInfo->mode                                      = ARCHIVE_FILE_MODE_WRITE;

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
  error = Crypt_init(&archiveFileInfo->special.cryptInfoSpecialEntry,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
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
    Crypt_done(&archiveFileInfo->special.cryptInfoSpecialEntry);
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
    Crypt_done(&archiveFileInfo->special.cryptInfoSpecialEntry);
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
    Crypt_done(&archiveFileInfo->special.cryptInfoSpecialEntry);
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
    Crypt_done(&archiveFileInfo->special.cryptInfoSpecialEntry);
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
                             CryptTypes         *cryptType,
                             String             name,
                             FileInfo           *fileInfo,
                             uint64             *fragmentOffset,
                             uint64             *fragmentSize
                            )
{
  Errors         error;
  ChunkHeader    chunkHeader;
  uint64         index;
  bool           decryptedFlag;
  PasswordHandle passwordHandle;
  const Password *password;
  ChunkHeader    subChunkHeader;
  bool           foundFileEntryFlag,foundFileDataFlag;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo       = archiveInfo;
  archiveFileInfo->mode              = ARCHIVE_FILE_MODE_READ;
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

  /* detect block length of used crypt algorithm */
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

  /* init compress */
  error = Compress_new(&archiveFileInfo->file.compressInfoData,
                       COMPRESS_MODE_INFLATE,
                       archiveFileInfo->file.compressAlgorithm,
                       archiveFileInfo->blockLength
                      );
  if (error != ERROR_NONE)
  {
    free(archiveFileInfo->file.buffer);
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return ERROR_COMPRESS_ERROR;
  }

  /* try to read file entry with all passwords */
  Chunk_tell(&archiveFileInfo->file.chunkInfoFile,&index);
  if (archiveFileInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstCryptPassword(&passwordHandle,
                                       archiveInfo->fileName,
                                       archiveInfo->jobOptions,
                                       archiveInfo->passwordMode
                                      );
    }
  }
  else
  {
    password = NULL;
  }
  decryptedFlag      = FALSE;
  foundFileEntryFlag = FALSE;
  foundFileDataFlag  = FALSE;
  while (   !decryptedFlag
         && ((archiveFileInfo->cryptAlgorithm == CRYPT_ALGORITHM_NONE) || (password != NULL))
        )
  {
    error = ERROR_NONE;

    /* reset chunk read position */
    Chunk_seek(&archiveFileInfo->file.chunkInfoFile,index);

    /* init crypt */
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveFileInfo->file.cryptInfoFileEntry,
                         archiveFileInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveFileInfo->file.cryptInfoFileData,
                         archiveFileInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveFileInfo->file.cryptInfoFileEntry);
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveFileInfo->file.cryptInfoData,
                         archiveFileInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveFileInfo->file.cryptInfoFileData);
        Crypt_done(&archiveFileInfo->file.cryptInfoFileEntry);
      }
    }

    /* init file entry/data chunks */
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveFileInfo->file.chunkInfoFileEntry,
                         &archiveFileInfo->file.chunkInfoFile,
                         &archiveInfo->fileHandle,
                         archiveFileInfo->blockLength,
                         &archiveFileInfo->file.cryptInfoFileEntry
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveFileInfo->file.cryptInfoData);
        Crypt_done(&archiveFileInfo->file.cryptInfoFileData);
        Crypt_done(&archiveFileInfo->file.cryptInfoFileEntry);
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveFileInfo->file.chunkInfoFileData,
                         &archiveFileInfo->file.chunkInfoFile,
                         &archiveInfo->fileHandle,
                         archiveFileInfo->blockLength,
                         &archiveFileInfo->file.cryptInfoFileData
                        );
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveFileInfo->file.chunkInfoFileEntry);
        Crypt_done(&archiveFileInfo->file.cryptInfoData);
        Crypt_done(&archiveFileInfo->file.cryptInfoFileData);
        Crypt_done(&archiveFileInfo->file.cryptInfoFileEntry);
      }
    }

    /* try to read file entry/data chunks */
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveFileInfo->file.chunkInfoFile)
             && (error == ERROR_NONE)
             && (!foundFileEntryFlag || !foundFileDataFlag)
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

            foundFileEntryFlag = TRUE;
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

            foundFileDataFlag = TRUE;
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
    }

    if (error == ERROR_NONE)
    {
      decryptedFlag = TRUE;
    }
    else
    {
      /* free resources */
      Chunk_done(&archiveFileInfo->file.chunkInfoFileData);
      Chunk_done(&archiveFileInfo->file.chunkInfoFileEntry);
      Crypt_done(&archiveFileInfo->file.cryptInfoData);
      Crypt_done(&archiveFileInfo->file.cryptInfoFileData);
      Crypt_done(&archiveFileInfo->file.cryptInfoFileEntry);

      if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
      {
        /* no more passwords when asymmetric encryption used */
        password = NULL;
      }
      else
      {
        /* next password */
        password = getNextCryptPassword(&passwordHandle);
      }
    }
  } /* while */

  if (!foundFileEntryFlag || !foundFileDataFlag)
  {
    if (foundFileEntryFlag) String_delete(archiveFileInfo->file.chunkFileEntry.name);

    if (decryptedFlag)
    {
      Chunk_done(&archiveFileInfo->file.chunkInfoFileData);
      Chunk_done(&archiveFileInfo->file.chunkInfoFileEntry);
      Crypt_done(&archiveFileInfo->file.cryptInfoData);
      Crypt_done(&archiveFileInfo->file.cryptInfoFileData);
      Crypt_done(&archiveFileInfo->file.cryptInfoFileEntry);
    }
    Compress_delete(&archiveFileInfo->file.compressInfoData);
    free(archiveFileInfo->file.buffer);
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);

    if      (error != ERROR_NONE) return error;
    else if (!foundFileEntryFlag) return ERROR_NO_FILE_ENTRY;
    else if (!foundFileDataFlag)  return ERROR_NO_FILE_DATA;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (compressAlgorithm != NULL) (*compressAlgorithm) = archiveFileInfo->file.compressAlgorithm;
  if (cryptAlgorithm    != NULL) (*cryptAlgorithm)    = archiveFileInfo->cryptAlgorithm;
  if (cryptType         != NULL) (*cryptType)         = archiveInfo->cryptType;

  /* reset compress, crypt */
  Compress_reset(&archiveFileInfo->file.compressInfoData);
  Crypt_reset(&archiveFileInfo->file.cryptInfoFileData,0);

  return ERROR_NONE;
}

Errors Archive_readDirectoryEntry(ArchiveInfo     *archiveInfo,
                                  ArchiveFileInfo *archiveFileInfo,
                                  CryptAlgorithms *cryptAlgorithm,
                                  CryptTypes      *cryptType,
                                  String          directoryName,
                                  FileInfo        *fileInfo
                                 )
{
  Errors         error;
  ChunkHeader    chunkHeader;
  uint64         index;
  bool           decryptedFlag;
  bool           foundDirectoryEntryFlag;
  PasswordHandle passwordHandle;
  const Password *password;
  ChunkHeader    subChunkHeader;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo = archiveInfo;
  archiveFileInfo->mode        = ARCHIVE_FILE_MODE_READ;
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

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }
  assert(archiveFileInfo->blockLength > 0);

  /* try to read directory entry with all passwords */
  Chunk_tell(&archiveFileInfo->directory.chunkInfoDirectory,&index);
  if (archiveFileInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstCryptPassword(&passwordHandle,
                                       archiveInfo->fileName,
                                       archiveInfo->jobOptions,
                                       archiveInfo->passwordMode
                                      );
    }
  }
  else
  {
    password = NULL;
  }
  decryptedFlag           = FALSE;
  foundDirectoryEntryFlag = FALSE;
  while (   !decryptedFlag
         && ((archiveFileInfo->cryptAlgorithm == CRYPT_ALGORITHM_NONE) || (password != NULL))
        )
  {
    error = ERROR_NONE;

    /* reset chunk read position */
    Chunk_seek(&archiveFileInfo->directory.chunkInfoDirectory,index);

    /* init crypt */
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveFileInfo->directory.cryptInfoDirectoryEntry,
                         archiveFileInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
      }
    }

    /* init directory entry */
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveFileInfo->directory.chunkInfoDirectoryEntry,
                         &archiveFileInfo->directory.chunkInfoDirectory,
                         &archiveInfo->fileHandle,
                         archiveFileInfo->blockLength,
                         &archiveFileInfo->directory.cryptInfoDirectoryEntry
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
      }
    }

    /* read directory entry */
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveFileInfo->directory.chunkInfoDirectory)
             && (error == ERROR_NONE)
             && !foundDirectoryEntryFlag
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

            foundDirectoryEntryFlag = TRUE;
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
    }

    if (error == ERROR_NONE)
    {
      decryptedFlag = TRUE;
    }
    else
    {
      /* free resources */
      Crypt_done(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
      Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);

      if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
      {
        /* no more passwords when asymmetric encryption used */
        password = NULL;
      }
      else
      {
        /* next password */
        password = getNextCryptPassword(&passwordHandle);
      }
    }
  } /* while */

  if (!foundDirectoryEntryFlag)
  {
    if (decryptedFlag)
    {
      Crypt_done(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
      Chunk_done(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
    }
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);

    if      (error != ERROR_NONE)      return error;
    else if (!foundDirectoryEntryFlag) return ERROR_NO_DIRECTORY_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveFileInfo->cryptAlgorithm;
  if (cryptType      != NULL) (*cryptType)      = archiveInfo->cryptType;

  return ERROR_NONE;
}

Errors Archive_readLinkEntry(ArchiveInfo     *archiveInfo,
                             ArchiveFileInfo *archiveFileInfo,
                             CryptAlgorithms *cryptAlgorithm,
                             CryptTypes      *cryptType,
                             String          linkName,
                             String          destinationName,
                             FileInfo        *fileInfo
                            )
{
  Errors         error;
  ChunkHeader    chunkHeader;
  uint64         index;
  bool           decryptedFlag;
  bool           foundLinkEntryFlag;
  PasswordHandle passwordHandle;
  const Password *password;
  ChunkHeader    subChunkHeader;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo = archiveInfo;
  archiveFileInfo->mode        = ARCHIVE_FILE_MODE_READ;
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

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }
  assert(archiveFileInfo->blockLength > 0);

  /* try to read link entry with all passwords */
  Chunk_tell(&archiveFileInfo->link.chunkInfoLink,&index);
  if (archiveFileInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstCryptPassword(&passwordHandle,
                                       archiveInfo->fileName,
                                       archiveInfo->jobOptions,
                                       archiveInfo->passwordMode
                                      );
    }
  }
  else
  {
    password = NULL;
  }
  decryptedFlag      = FALSE;
  foundLinkEntryFlag = FALSE;
  while (   !decryptedFlag
         && ((archiveFileInfo->cryptAlgorithm == CRYPT_ALGORITHM_NONE) || (password != NULL))
        )
  {
    error = ERROR_NONE;

    /* reset chunk read position */
    Chunk_seek(&archiveFileInfo->link.chunkInfoLink,index);

    /* init crypt */
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveFileInfo->link.cryptInfoLinkEntry,
                         archiveFileInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
      }
    }

    /* init link entry */
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveFileInfo->link.chunkInfoLinkEntry,
                         &archiveFileInfo->link.chunkInfoLink,
                         &archiveInfo->fileHandle,
                         archiveFileInfo->blockLength,
                         &archiveFileInfo->link.cryptInfoLinkEntry
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveFileInfo->link.cryptInfoLinkEntry);
      }
    }

    /* read link entry */
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveFileInfo->link.chunkInfoLink)
             && (error == ERROR_NONE)
             && !foundLinkEntryFlag
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

            foundLinkEntryFlag = TRUE;
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
    }

    if (error == ERROR_NONE)
    {
      decryptedFlag = TRUE;
    }
    else
    {
      /* free resources */
      Chunk_done(&archiveFileInfo->link.chunkInfoLinkEntry);
      Crypt_done(&archiveFileInfo->link.cryptInfoLinkEntry);

      if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
      {
        /* no more passwords when asymmetric encryption used */
        password = NULL;
      }
      else
      {
        /* next password */
        password = getNextCryptPassword(&passwordHandle);
      }
    }
  } /* while */

  if (!foundLinkEntryFlag)
  {
    if (decryptedFlag)
    {
      Chunk_done(&archiveFileInfo->link.chunkInfoLinkEntry);
      Crypt_done(&archiveFileInfo->link.cryptInfoLinkEntry);
    }
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);

    if      (error != ERROR_NONE) return error;
    else if (!foundLinkEntryFlag) return ERROR_NO_LINK_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveFileInfo->cryptAlgorithm;
  if (cryptType      != NULL) (*cryptType)      = archiveInfo->cryptType;

  return ERROR_NONE;
}

Errors Archive_readSpecialEntry(ArchiveInfo     *archiveInfo,
                                ArchiveFileInfo *archiveFileInfo,
                                CryptAlgorithms *cryptAlgorithm,
                                CryptTypes      *cryptType,
                                String          specialName,
                                FileInfo        *fileInfo
                               )
{
  Errors         error;
  ChunkHeader    chunkHeader;
  uint64         index;
  bool           decryptedFlag;
  bool           foundSpecialEntryFlag;
  PasswordHandle passwordHandle;
  const Password *password;
  ChunkHeader    subChunkHeader;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo = archiveInfo;
  archiveFileInfo->mode        = ARCHIVE_FILE_MODE_READ;
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

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);
    return error;
  }
  assert(archiveFileInfo->blockLength > 0);

  /* try to read file entry with all passwords */
  Chunk_tell(&archiveFileInfo->special.chunkInfoSpecial,&index);
  if (archiveFileInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstCryptPassword(&passwordHandle,
                                       archiveInfo->fileName,
                                       archiveInfo->jobOptions,
                                       archiveInfo->passwordMode
                                      );
    }
  }
  else
  {
    password = NULL;
  }
  decryptedFlag         = FALSE;
  foundSpecialEntryFlag = FALSE;
  while (   !decryptedFlag
         && ((archiveFileInfo->cryptAlgorithm == CRYPT_ALGORITHM_NONE) || (password != NULL))
        )
  {
    error = ERROR_NONE;

    /* reset chunk read position */
    Chunk_seek(&archiveFileInfo->special.chunkInfoSpecial,index);

    /* init crypt */
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveFileInfo->special.cryptInfoSpecialEntry,
                         archiveFileInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
      }
    }

    /* init special entry */
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveFileInfo->special.chunkInfoSpecialEntry,
                         &archiveFileInfo->special.chunkInfoSpecial,
                         &archiveInfo->fileHandle,
                         archiveFileInfo->blockLength,
                         &archiveFileInfo->special.cryptInfoSpecialEntry
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveFileInfo->special.cryptInfoSpecialEntry);
      }
    }

    /* read special entry */
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveFileInfo->special.chunkInfoSpecial)
             && (error == ERROR_NONE)
             && !foundSpecialEntryFlag
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

            foundSpecialEntryFlag = TRUE;
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
    }

    if (error == ERROR_NONE)
    {
      decryptedFlag = TRUE;
    }
    else
    {
      /* free resources */
      Chunk_done(&archiveFileInfo->special.chunkInfoSpecialEntry);
      Crypt_done(&archiveFileInfo->special.cryptInfoSpecialEntry);

      if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
      {
        /* no more passwords when asymmetric encryption used */
        password = NULL;
      }
      else
      {
        /* next password */
        password = getNextCryptPassword(&passwordHandle);
      }
    }
  } /* while */

  if (!foundSpecialEntryFlag)
  {
    if (decryptedFlag)
    {
      Chunk_done(&archiveFileInfo->special.chunkInfoSpecialEntry);
      Crypt_done(&archiveFileInfo->special.cryptInfoSpecialEntry);
    }
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    Chunk_skip(&archiveInfo->fileHandle,&chunkHeader);

    if      (error != ERROR_NONE)    return error;
    else if (!foundSpecialEntryFlag) return ERROR_NO_SPECIAL_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveFileInfo->cryptAlgorithm;
  if (cryptType      != NULL) (*cryptType)      = archiveInfo->cryptType;

  return ERROR_NONE;
}

Errors Archive_closeEntry(ArchiveFileInfo *archiveFileInfo)
{
  Errors error;

  assert(archiveFileInfo != NULL);
  assert(archiveFileInfo->archiveInfo != NULL);

  switch (archiveFileInfo->mode)
  {
    case ARCHIVE_FILE_MODE_WRITE:
      switch (archiveFileInfo->fileType)
      {
        case FILE_TYPE_FILE:
          /* flush last blocks */
          Compress_flush(&archiveFileInfo->file.compressInfoData);
          if (   !archiveFileInfo->file.createdFlag
              || (Compress_getAvailableBlocks(&archiveFileInfo->file.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
             )
          {
            while (Compress_getAvailableBlocks(&archiveFileInfo->file.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
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
          Crypt_done(&archiveFileInfo->file.cryptInfoData);
          Crypt_done(&archiveFileInfo->file.cryptInfoFileData);
          Crypt_done(&archiveFileInfo->file.cryptInfoFileEntry);
          Chunk_done(&archiveFileInfo->file.chunkInfoFile);
          free(archiveFileInfo->file.buffer);
          String_delete(archiveFileInfo->file.chunkFileEntry.name);
          break;
        case FILE_TYPE_DIRECTORY:
          /* close chunks */
          error = Chunk_close(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
          if (error != ERROR_NONE)
          {
            return error;
          }
          error = Chunk_close(&archiveFileInfo->directory.chunkInfoDirectory);
          if (error != ERROR_NONE)
          {
            return error;
          }

          /* free resources */
          Chunk_done(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
          Crypt_done(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
          Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
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
          Crypt_done(&archiveFileInfo->link.cryptInfoLinkEntry);
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
          Crypt_done(&archiveFileInfo->special.cryptInfoSpecialEntry);
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
    case ARCHIVE_FILE_MODE_READ:
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
          Crypt_done(&archiveFileInfo->file.cryptInfoData);
          Crypt_done(&archiveFileInfo->file.cryptInfoFileData);
          Crypt_done(&archiveFileInfo->file.cryptInfoFileEntry);
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
          Crypt_done(&archiveFileInfo->link.cryptInfoLinkEntry);
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
          Crypt_done(&archiveFileInfo->special.cryptInfoSpecialEntry);
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

Errors Archive_writeFileData(ArchiveFileInfo *archiveFileInfo, const void *buffer, ulong length)
{
  byte   *p;
  Errors error;
  ulong  deflatedBytess;

  assert(archiveFileInfo != NULL);
  assert(archiveFileInfo->archiveInfo != NULL);
  assert((archiveFileInfo->file.bufferLength%archiveFileInfo->blockLength) == 0);

  p = (byte*)buffer;
  while (length > 0)
  {
    /* compress */
    error = Compress_deflate(&archiveFileInfo->file.compressInfoData,p,length,&deflatedBytess);
    if (error != ERROR_NONE)
    {
      return error;
    }
    p += deflatedBytess;
    length -= deflatedBytess;

    /* check if block can be encrypted and written to file */
    while (Compress_getAvailableBlocks(&archiveFileInfo->file.compressInfoData,COMPRESS_BLOCK_TYPE_FULL) > 0)
    {
      error = writeDataBlock(archiveFileInfo,BLOCK_MODE_WRITE);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
    assert(Compress_getAvailableBlocks(&archiveFileInfo->file.compressInfoData,COMPRESS_BLOCK_TYPE_FULL) == 0);
  }

  archiveFileInfo->file.bufferLength = 0;

  return ERROR_NONE;
}

Errors Archive_readFileData(ArchiveFileInfo *archiveFileInfo, void *buffer, ulong length)
{
  byte   *p;
  Errors error;
  ulong  n;
  ulong  inflatedBytes;

  assert(archiveFileInfo != NULL);

  p = (byte*)buffer;
  while (length > 0)
  {
    /* fill decompressor with compressed data blocks */
    do
    {
      n = Compress_getAvailableBytes(&archiveFileInfo->file.compressInfoData);
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
    error = Compress_inflate(&archiveFileInfo->file.compressInfoData,p,1,&inflatedBytes);
    if (error != ERROR_NONE)
    {
      return error;
    }
    p += inflatedBytes;
    length -= inflatedBytes;
  }

  return ERROR_NONE;
}

uint64 Archive_getSize(ArchiveFileInfo *archiveFileInfo)
{
  assert(archiveFileInfo != NULL);
  assert(archiveFileInfo->archiveInfo != NULL);

  return (archiveFileInfo->archiveInfo->fileOpenFlag)?File_getSize(&archiveFileInfo->archiveInfo->fileHandle):0LL;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
