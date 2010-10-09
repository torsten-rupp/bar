/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/archive.c,v $
* $Revision: 1.19 $
* $Author: torsten $
* Contents: Backup ARchiver archive functions
* Systems: all
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
#include "files.h"
#include "errors.h"

#include "archive_format.h"
#include "chunks.h"
#include "compress.h"
#include "passwords.h"
#include "crypt.h"

#include "archive.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define MAX_BUFFER_SIZE (64*1024)

/* i/o via file functions */
const ChunkIO CHUNK_IO_FILE =
{
  (bool(*)(void*))File_eof,
  (Errors(*)(void*,void*,ulong,ulong*))File_read,
  (Errors(*)(void*,const void*,ulong))File_write,
  (Errors(*)(void*,uint64*))File_tell,
  (Errors(*)(void*,uint64))File_seek,
  (uint64(*)(void*))File_getSize
};

/* i/o via storage file functions */
const ChunkIO CHUNK_IO_STORAGE_FILE =
{
  (bool(*)(void*))Storage_eof,
  (Errors(*)(void*,void*,ulong,ulong*))Storage_read,
  (Errors(*)(void*,const void*,ulong))Storage_write,
  (Errors(*)(void*,uint64*))Storage_tell,
  (Errors(*)(void*,uint64))Storage_seek,
  (uint64(*)(void*))Storage_getSize
};

/***************************** Datatypes *******************************/

typedef enum
{
  BLOCK_MODE_WRITE,
  BLOCK_MODE_FLUSH,
} BlockModes;

/* decrypt password list */
typedef struct PasswordNode
{
  LIST_NODE_HEADER(struct PasswordNode);

  Password *password;
} PasswordNode;

typedef struct
{
  LIST_HEADER(PasswordNode);
} PasswordList;

/* password handle */
typedef struct
{
  String                          fileName;
  PasswordModes                   passwordMode;
  const Password                  *cryptPassword;
  const PasswordNode              *passwordNode;
  ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction;
  void                            *archiveGetCryptPasswordUserData;
  bool                            inputFlag;
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
* Input  : fileName                        - file name
*          jobOptions                      - job options
*          passwordMode                    - password mode
*          archiveGetCryptPasswordFunction - get password call back
*          archiveGetCryptPasswordUserData - user data for get password
*                                            call back
* Output : -
* Return : password or NULL if no password given
* Notes  : -
\***********************************************************************/

LOCAL Password *getCryptPassword(const String                    fileName,
                                 const JobOptions                *jobOptions,
                                 PasswordModes                   passwordMode,
                                 ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                                 void                            *archiveGetCryptPasswordUserData
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
        if (archiveGetCryptPasswordFunction != NULL)
        {
          password = Password_new();
          if (password == NULL)
          {
            return NULL;
          }
          error = archiveGetCryptPasswordFunction(archiveGetCryptPasswordUserData,password,fileName,TRUE,TRUE);
          if (error != ERROR_NONE)
          {
            Password_delete(password);
            return NULL;
          }
        }
        else
        {
          return NULL;
        }
      }
      break;
    case PASSWORD_MODE_ASK:
      if (archiveGetCryptPasswordFunction != NULL)
      {
        password = Password_new();
        if (password == NULL)
        {
          return NULL;
        }
        error = archiveGetCryptPasswordFunction(archiveGetCryptPasswordUserData,password,fileName,TRUE,TRUE);
        if (error != ERROR_NONE)
        {
          Password_delete(password);
          return NULL;
        }
      }
      else
      {
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
        if (archiveGetCryptPasswordFunction != NULL)
        {
          password = Password_new();
          if (password == NULL)
          {
            return NULL;
          }
          error = archiveGetCryptPasswordFunction(archiveGetCryptPasswordUserData,password,fileName,TRUE,TRUE);
          if (error != ERROR_NONE)
          {
            Password_delete(password);
            return NULL;
          }
        }
        else
        {
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
* Name   : getNextDecryptPassword
* Purpose: get next decrypt password
* Input  : passwordHandle - password handle
* Output : -
* Return : password or NULL if no more passwords
* Notes  : -
\***********************************************************************/

LOCAL const Password *getNextDecryptPassword(PasswordHandle *passwordHandle)
{
  Password *password;
  Errors   error;

  assert(passwordHandle != NULL);

  if      (passwordHandle->passwordNode != NULL)
  {
    /* next password from list */
    password = passwordHandle->passwordNode->password;
    passwordHandle->passwordNode = passwordHandle->passwordNode->next;
  }
  else if (   (passwordHandle->passwordMode == PASSWORD_MODE_CONFIG)
           && (passwordHandle->cryptPassword != NULL)
          )
  {
    /* get password */
    password = (Password*)passwordHandle->cryptPassword;

    /* next password is: default */
    passwordHandle->passwordMode = PASSWORD_MODE_DEFAULT;
  }
  else if (   (passwordHandle->passwordMode == PASSWORD_MODE_DEFAULT)
           && (globalOptions.cryptPassword != NULL)
          )
  {
    /* get password */
    password = globalOptions.cryptPassword;

    /* next password is: ask */
    passwordHandle->passwordMode = PASSWORD_MODE_ASK;
  }
  else if (   !passwordHandle->inputFlag
           && (   (passwordHandle->passwordMode == PASSWORD_MODE_ASK)
               || (globalOptions.cryptPassword == NULL)
              )
          )
  {
    if (passwordHandle->archiveGetCryptPasswordFunction != NULL)
    {
      /* input password */
      password = Password_new();
      if (password == NULL)
      {
        return NULL;
      }
      error = passwordHandle->archiveGetCryptPasswordFunction(passwordHandle->archiveGetCryptPasswordUserData,password,passwordHandle->fileName,FALSE,FALSE);
      if (error != ERROR_NONE)
      {
        Password_delete(password);
        return NULL;
      }

      /* add to password list */
      Archive_appendDecryptPassword(password);

      /* free resources */
      Password_delete(password);

      /* next password is: none */
      passwordHandle->inputFlag = TRUE;
    }
    else
    {
      return NULL;
    }
  }
  else
  {
    return NULL;
  }

  return password;
}

/***********************************************************************\
* Name   : getFirstDecryptPassword
* Purpose: get first decrypt password
* Input  : fileName                        - file name
*          jobOptions                      - job options
*          passwordMode                    - password mode
*          archiveGetCryptPasswordFunction - get password call back
*          archiveGetCryptPasswordUserData - user data for get password
*                                            call back
* Output : passwordHandle - intialized password handle
* Return : password or NULL if no more passwords
* Notes  : -
\***********************************************************************/

LOCAL const Password *getFirstDecryptPassword(PasswordHandle                  *passwordHandle,
                                              const String                    fileName,
                                              const JobOptions                *jobOptions,
                                              PasswordModes                   passwordMode,
                                              ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                                              void                            *archiveGetCryptPasswordUserData
                                             )
{
  assert(passwordHandle != NULL);

  passwordHandle->fileName                        = fileName;
  passwordHandle->passwordMode                    = (passwordMode != PASSWORD_MODE_DEFAULT)?passwordMode:jobOptions->cryptPasswordMode;
  passwordHandle->cryptPassword                   = jobOptions->cryptPassword;
  passwordHandle->passwordNode                    = decryptPasswordList.head;
  passwordHandle->archiveGetCryptPasswordFunction = archiveGetCryptPasswordFunction;
  passwordHandle->archiveGetCryptPasswordUserData = archiveGetCryptPasswordUserData;
  passwordHandle->inputFlag                       = FALSE;

  return getNextDecryptPassword(passwordHandle);
}

/***********************************************************************\
* Name   : getNextChunkHeader
* Purpose: read next chunk header
* Input  : archiveInfo - archive info block
* Output : chunkHeader - read chunk header
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getNextChunkHeader(ArchiveInfo *archiveInfo, ChunkHeader *chunkHeader)
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(chunkHeader != NULL);

  if (!archiveInfo->nextChunkHeaderReadFlag)
  {
    if (Chunk_eof(archiveInfo->chunkIO,archiveInfo->chunkIOUserData))
    {
      return ERROR_END_OF_ARCHIVE;
    }

    error = Chunk_next(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,chunkHeader);
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
*                              part
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

  assert(archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);

  newPartFlag = FALSE;
  if (archiveInfo->jobOptions->archivePartSize > 0)
  {
    if (archiveInfo->file.openFlag)
    {
      /* get file size */
      fileSize = archiveInfo->chunkIO->getSize(archiveInfo->chunkIOUserData);

      if      (   !headerWrittenFlag
               && (fileSize+headerLength >= archiveInfo->jobOptions->archivePartSize)
              )
      {
        /* file header cannot be written without fragmentation -> new part */
        newPartFlag = TRUE;
      }
      else if ((fileSize+minBytes) >= archiveInfo->jobOptions->archivePartSize)
      {
        /* less than min. number of bytes left in part -> create new part */
        newPartFlag = TRUE;
      }
    }
  }

  return newPartFlag;
}

#if 0
/***********************************************************************\
* Name   : readHeader
* Purpose: read encryption key
* Input  : archiveInfo - archive info block
*          chunkHeader - key chunk header
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors readHeader(ArchiveInfo       *archiveInfo,
                        const ChunkHeader *chunkHeader
                       )
{
  Errors    error;
  ChunkInfo chunkInfoBar;
  ChunkBar  chunkBar;

  assert(archiveInfo != NULL);
  assert(chunkHeader != NULL);

  /* create key chunk */
  error = Chunk_init(&chunkInfoBar,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     0,
                     NULL
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* read key chunk */
  error = Chunk_open(&chunkInfoBar,
                     chunkHeader,
                     CHUNK_ID_BAR,
                     CHUNK_DEFINITION_BAR,
                     Chunk_getSize(CHUNK_DEFINITION_BAR,0,NULL),
                     &chunkBar
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkInfoBar);
    return error;
  }

  return ERROR_NONE;
}
#endif /* 0 */

/***********************************************************************\
* Name   : readEncryptionKey
* Purpose: read encryption key
* Input  : archiveInfo - archive info block
*          chunkHeader - key chunk header
* Output : -
* Return : ERROR_NONE or error code
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
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
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
* Name   : writeHeader
* Purpose: write header
* Input  : archiveInfo - archive info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeHeader(ArchiveInfo *archiveInfo)
{
  Errors    error;
  ChunkInfo chunkInfoBar;
  ChunkKey  chunkBar;

  assert(archiveInfo != NULL);

  /* create key chunk */
  error = Chunk_init(&chunkInfoBar,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     0,
                     NULL
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* write header */
  error = Chunk_create(&chunkInfoBar,
                       CHUNK_ID_BAR,
                       CHUNK_DEFINITION_BAR,
                       Chunk_getSize(CHUNK_DEFINITION_BAR,0,&chunkBar),
                       &chunkBar
                      );
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkInfoBar);
    return error;
  }

  /* free resources */
  error = Chunk_close(&chunkInfoBar);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkInfoBar);
    return error;
  }
  Chunk_done(&chunkInfoBar);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeEncryptionKey
* Purpose: write new encryption key
* Input  : archiveInfo - archive info block
* Output : -
* Return : ERROR_NONE or error code
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
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
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
* Name   : createArchiveFile
* Purpose: create and open new archive file
* Input  : archiveInfo - archive info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createArchiveFile(ArchiveInfo *archiveInfo)
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);
  assert(!archiveInfo->file.openFlag);

  /* get output filename */
  error = File_getTmpFileName(archiveInfo->fileName,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* create file */
  error = File_open(&archiveInfo->file.fileHandle,
                    archiveInfo->fileName,
                    FILE_OPENMODE_CREATE
                   );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* write bar header */
  error = writeHeader(archiveInfo);
  if (error != ERROR_NONE)
  {
    File_close(&archiveInfo->file.fileHandle);
    return error;
  }

  /* write encrypted key if asymmetric encryption enabled */
  if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
  {
    error = writeEncryptionKey(archiveInfo);
    if (error != ERROR_NONE)
    {
      File_close(&archiveInfo->file.fileHandle);
      return error;
    }
  }

  /* init index */
  if (archiveInfo->databaseHandle != NULL)
  {
    error = Index_create(archiveInfo->databaseHandle,
                         NULL,
                         INDEX_STATE_CREATE,
                         &archiveInfo->storageId
                        );
    if (error != ERROR_NONE)
    {
      File_close(&archiveInfo->file.fileHandle);
      return error;
    }
  }

  /* mark archive file "open" */
  archiveInfo->file.openFlag = TRUE;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : closeArchiveFile
* Purpose: close archive file
* Input  : archiveInfo  - archive info block
*          lastPartFlag - TRUE iff last archive part, FALSE otherwise
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors closeArchiveFile(ArchiveInfo *archiveInfo,
                              bool        lastPartFlag
                             )
{
  uint64 fileSize;
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);
  assert(archiveInfo->file.openFlag);

  /* get size of archive */
  fileSize = archiveInfo->chunkIO->getSize(archiveInfo->chunkIOUserData);

  if (archiveInfo->file.openFlag)
  {
    /* close file */
    File_close(&archiveInfo->file.fileHandle);
    archiveInfo->file.openFlag = FALSE;
  }

  /* call back new archive created */
  if (archiveInfo->archiveNewFileFunction != NULL)
  {
    error = archiveInfo->archiveNewFileFunction(archiveInfo->archiveNewFileUserData,
                                                archiveInfo->databaseHandle,
                                                archiveInfo->storageId,
                                                archiveInfo->fileName,
//                                                archiveInfo->cryptPassword,
//                                                archiveInfo->jobOptions->cryptPrivateKeyFileName,
                                                (archiveInfo->jobOptions->archivePartSize > 0)?archiveInfo->partNumber:ARCHIVE_PART_NUMBER_NONE,
                                                lastPartFlag
                                               );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  /* clear archive entry list */
//  List_clear(&archiveInfo->archiveEntryList,(ListNodeFreeFunction)Archive_freeArchiveEntryNode,NULL);

  /* increament part number */
  if (archiveInfo->jobOptions->archivePartSize > 0)
  {
    archiveInfo->partNumber++;
  }

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

  assert(archiveInfo != NULL);
  assert(archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);

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

  /* create new file if needed */
  if (!archiveInfo->file.openFlag)
  {
    /* create file */
    error = createArchiveFile(archiveInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeFileDataBlock
* Purpose: write file data block, encrypt and split file
* Input  : archiveFileInfo  - archive file info block
*          blockMode        - block write mode; see BlockModes
*          allowNewPartFlag - TRUE iff new archive part can be created
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeFileDataBlock(ArchiveFileInfo *archiveFileInfo, BlockModes blockMode, bool allowNewPartFlag)
{
  ulong  length;
  bool   newPartFlag;
//  ulong  n;
  Errors error;

  assert(archiveFileInfo != NULL);
  assert(archiveFileInfo->archiveInfo != NULL);
  assert(archiveFileInfo->archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);

  /* get compressed block */
  Compress_getBlock(&archiveFileInfo->file.compressInfoData,
                    archiveFileInfo->file.buffer,
                    &length
                   );

  /* check if split is allowed and necessary */
  newPartFlag =    allowNewPartFlag
                && checkNewPartNeeded(archiveFileInfo->archiveInfo,
                                      archiveFileInfo->file.headerLength,
                                      archiveFileInfo->file.headerWrittenFlag,
                                      ((length > 0) || (blockMode == BLOCK_MODE_WRITE))?archiveFileInfo->archiveInfo->blockLength:0
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

    if (!archiveFileInfo->file.createdFlag || (length > 0))
    {
      /* open file if needed */
      if (!archiveFileInfo->archiveInfo->file.openFlag)
      {
        /* create file */
        error = createArchiveFile(archiveFileInfo->archiveInfo);
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
* Name   : readFileDataBlock
* Purpose: read file data block, decrypt
* Input  : archiveFileInfo - archive file info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors readFileDataBlock(ArchiveFileInfo *archiveFileInfo)
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

/***********************************************************************\
* Name   : writeImageDataBlock
* Purpose: write image data block, encrypt and split file
* Input  : archiveFileInfo  - archive file info block
*          blockMode        - block write mode; see BlockModes
*          allowNewPartFlag - TRUE iff new archive part can be created
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeImageDataBlock(ArchiveFileInfo *archiveFileInfo, BlockModes blockMode, bool allowNewPartFlag)
{
  ulong  length;
  bool   newPartFlag;
  Errors error;

  assert(archiveFileInfo != NULL);
  assert(archiveFileInfo->archiveInfo != NULL);
  assert(archiveFileInfo->archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);

  /* get compressed block */
  Compress_getBlock(&archiveFileInfo->image.compressInfoData,
                    archiveFileInfo->image.buffer,
                    &length
                   );

  /* check if split is allowed and necessary */
  newPartFlag =    allowNewPartFlag
                && checkNewPartNeeded(archiveFileInfo->archiveInfo,
                                      archiveFileInfo->image.headerLength,
                                      archiveFileInfo->image.headerWrittenFlag,
                                      ((length > 0) || (blockMode == BLOCK_MODE_WRITE))?archiveFileInfo->archiveInfo->blockLength:0
                                     );

  /* split */
  if (newPartFlag)
  {
    /* create chunk-headers */
    if (!archiveFileInfo->image.headerWrittenFlag && (!archiveFileInfo->image.createdFlag || (length > 0)))
    {
      error = Chunk_create(&archiveFileInfo->image.chunkInfoImage,
                           CHUNK_ID_IMAGE,
                           CHUNK_DEFINITION_IMAGE,
                           Chunk_getSize(CHUNK_DEFINITION_IMAGE,0,&archiveFileInfo->image.chunkImage),
                           &archiveFileInfo->image.chunkImage
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = Chunk_create(&archiveFileInfo->image.chunkInfoImageEntry,
                           CHUNK_ID_IMAGE_ENTRY,
                           CHUNK_DEFINITION_IMAGE_ENTRY,
                           Chunk_getSize(CHUNK_DEFINITION_IMAGE_ENTRY,archiveFileInfo->blockLength,&archiveFileInfo->image.chunkImageEntry),
                           &archiveFileInfo->image.chunkImageEntry
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = Chunk_create(&archiveFileInfo->image.chunkInfoImageData,
                           CHUNK_ID_IMAGE_DATA,
                           CHUNK_DEFINITION_IMAGE_DATA,
                           Chunk_getSize(CHUNK_DEFINITION_IMAGE_DATA,archiveFileInfo->blockLength,&archiveFileInfo->image.chunkImageData),
                           &archiveFileInfo->image.chunkImageData
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      archiveFileInfo->image.createdFlag       = TRUE;
      archiveFileInfo->image.headerWrittenFlag = TRUE;
    }

    if (length > 0)
    {
      /* encrypt block */
      error = Crypt_encrypt(&archiveFileInfo->image.cryptInfoData,
                            archiveFileInfo->image.buffer,
                            archiveFileInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      /* write block */
      error = Chunk_writeData(&archiveFileInfo->image.chunkInfoImageData,
                              archiveFileInfo->image.buffer,
                              archiveFileInfo->blockLength
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    /* flush compress buffer */
    Compress_flush(&archiveFileInfo->image.compressInfoData);
    while (Compress_getAvailableBlocks(&archiveFileInfo->image.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
    {
      /* get compressed block */
      Compress_getBlock(&archiveFileInfo->image.compressInfoData,
                        archiveFileInfo->image.buffer,
                        &length
                       );

      /* encrypt block */
      error = Crypt_encrypt(&archiveFileInfo->image.cryptInfoData,
                            archiveFileInfo->image.buffer,
                            archiveFileInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      /* write */
      error = Chunk_writeData(&archiveFileInfo->image.chunkInfoImageData,
                              archiveFileInfo->image.buffer,
                              archiveFileInfo->blockLength
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    /* update part size */
    assert(archiveFileInfo->image.blockSize > 0);
    archiveFileInfo->image.chunkImageData.blockCount = Compress_getInputLength(&archiveFileInfo->image.compressInfoData)/archiveFileInfo->image.blockSize;
    error = Chunk_update(&archiveFileInfo->image.chunkInfoImageData,
                         &archiveFileInfo->image.chunkImageData
                        );
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* close chunks */
    error = Chunk_close(&archiveFileInfo->image.chunkInfoImageData);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Chunk_close(&archiveFileInfo->image.chunkInfoImageEntry);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Chunk_close(&archiveFileInfo->image.chunkInfoImage);
    if (error != ERROR_NONE)
    {
      return error;
    }
    archiveFileInfo->image.headerWrittenFlag = FALSE;

    /* close file */
    closeArchiveFile(archiveFileInfo->archiveInfo,FALSE);

    /* reset compress (do it here because data if buffered and can be processed before a new file is opened) */
    Compress_reset(&archiveFileInfo->image.compressInfoData);
  }
  else
  {
    if (!archiveFileInfo->image.createdFlag || (length > 0))
    {
      /* open file if needed */
      if (!archiveFileInfo->archiveInfo->file.openFlag)
      {
        /* create file */
        error = createArchiveFile(archiveFileInfo->archiveInfo);
        if (error != ERROR_NONE)
        {
          return error;
        }

        /* initialise variables */
        archiveFileInfo->image.headerWrittenFlag = FALSE;

        archiveFileInfo->image.chunkImageData.blockOffset = archiveFileInfo->image.chunkImageData.blockOffset+archiveFileInfo->image.chunkImageData.blockCount;
        archiveFileInfo->image.chunkImageData.blockCount  = 0;

        /* reset data crypt */
        Crypt_reset(&archiveFileInfo->image.cryptInfoData,0);
      }

      /* create chunk-headers */
      if (!archiveFileInfo->image.headerWrittenFlag)
      {
        error = Chunk_create(&archiveFileInfo->image.chunkInfoImage,
                             CHUNK_ID_IMAGE,
                             CHUNK_DEFINITION_IMAGE,
                             Chunk_getSize(CHUNK_DEFINITION_IMAGE,0,&archiveFileInfo->image.chunkImage),
                             &archiveFileInfo->image.chunkImage
                            );
        if (error != ERROR_NONE)
        {
          return error;
        }

        error = Chunk_create(&archiveFileInfo->image.chunkInfoImageEntry,
                             CHUNK_ID_IMAGE_ENTRY,
                             CHUNK_DEFINITION_IMAGE_ENTRY,
                             Chunk_getSize(CHUNK_DEFINITION_IMAGE_ENTRY,archiveFileInfo->blockLength,&archiveFileInfo->image.chunkImageEntry),
                             &archiveFileInfo->image.chunkImageEntry
                            );
        if (error != ERROR_NONE)
        {
          return error;
        }

        error = Chunk_create(&archiveFileInfo->image.chunkInfoImageData,
                             CHUNK_ID_IMAGE_DATA,
                             CHUNK_DEFINITION_IMAGE_DATA,
                             Chunk_getSize(CHUNK_DEFINITION_IMAGE_DATA,archiveFileInfo->blockLength,&archiveFileInfo->image.chunkImageData),
                             &archiveFileInfo->image.chunkImageData
                            );
        if (error != ERROR_NONE)
        {
          return error;
        }

        archiveFileInfo->image.createdFlag       = TRUE;
        archiveFileInfo->image.headerWrittenFlag = TRUE;
      }

      if (length > 0)
      {
        /* encrypt block */
        error = Crypt_encrypt(&archiveFileInfo->image.cryptInfoData,
                              archiveFileInfo->image.buffer,
                              archiveFileInfo->blockLength
                             );
        if (error != ERROR_NONE)
        {
          return error;
        }

        /* write block */
        error = Chunk_writeData(&archiveFileInfo->image.chunkInfoImageData,
                                archiveFileInfo->image.buffer,
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
* Name   : readImageDataBlock
* Purpose: read image data block, decrypt
* Input  : archiveFileInfo - archive file info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors readImageDataBlock(ArchiveFileInfo *archiveFileInfo)
{
  Errors error;

  assert(archiveFileInfo != NULL);

  if (!Chunk_eofSub(&archiveFileInfo->image.chunkInfoImageData))
  {
    /* read */
    error = Chunk_readData(&archiveFileInfo->image.chunkInfoImageData,
                           archiveFileInfo->image.buffer,
                           archiveFileInfo->blockLength
                          );
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* decrypt */
    error = Crypt_decrypt(&archiveFileInfo->image.cryptInfoData,
                          archiveFileInfo->image.buffer,
                          archiveFileInfo->blockLength
                         );
    if (error != ERROR_NONE)
    {
      return error;
    }

    Compress_putBlock(&archiveFileInfo->image.compressInfoData,
                      archiveFileInfo->image.buffer,
                      archiveFileInfo->blockLength
                     );
  }
  else
  {
    Compress_flush(&archiveFileInfo->image.compressInfoData);
    if (Compress_getAvailableBlocks(&archiveFileInfo->image.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) <= 0)
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
  error = Chunk_initAll();
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

void Archive_clearDecryptPasswords(void)
{
  List_clear(&decryptPasswordList,(ListNodeFreeFunction)freePasswordNode,NULL);
}

void Archive_appendDecryptPassword(const Password *password)
{
  PasswordNode *passwordNode;

  assert(password != NULL);

  passwordNode = LIST_NEW_NODE(PasswordNode);
  if (passwordNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  passwordNode->password = Password_duplicate(password);

  List_append(&decryptPasswordList,passwordNode);
}

#if 0
void Archive_freeArchiveEntryNode(ArchiveEntryNode *archiveEntryNode, void *userData)
{
  assert(archiveEntryNode != NULL);

  UNUSED_VARIABLE(userData);

  switch (archiveEntryNode->type)
  {
    case ARCHIVE_ENTRY_TYPE_NONE:
      break;
    case ARCHIVE_ENTRY_TYPE_FILE:
      String_delete(archiveEntryNode->file.fileName);
      break;
    case ARCHIVE_ENTRY_TYPE_IMAGE:
      String_delete(archiveEntryNode->image.imageName);
      break;
    case ARCHIVE_ENTRY_TYPE_DIRECTORY:
      String_delete(archiveEntryNode->directory.directoryName);
      break;
    case ARCHIVE_ENTRY_TYPE_LINK:
      String_delete(archiveEntryNode->link.linkName);
      String_delete(archiveEntryNode->link.destinationName);
      break;
    case ARCHIVE_ENTRY_TYPE_SPECIAL:
      String_delete(archiveEntryNode->special.fileName);
      break;
    case ARCHIVE_ENTRY_TYPE_UNKNOWN:
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
}

ArchiveEntryNode *Archive_copyArchiveEntryNode(ArchiveEntryNode *archiveEntryNode, void *userData)
{
  ArchiveEntryNode *newArchiveEntryNode;

  assert(archiveEntryNode != NULL);

  UNUSED_VARIABLE(userData);

  newArchiveEntryNode = LIST_NEW_NODE(ArchiveEntryNode);
  if (newArchiveEntryNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  memcpy(newArchiveEntryNode,archiveEntryNode,sizeof(ArchiveEntryNode));
  switch (archiveEntryNode->type)
  {
    case ARCHIVE_ENTRY_TYPE_NONE:
      break;
    case ARCHIVE_ENTRY_TYPE_FILE:
      newArchiveEntryNode->file.fileName = String_duplicate(archiveEntryNode->file.fileName);
      break;
    case ARCHIVE_ENTRY_TYPE_IMAGE:
      newArchiveEntryNode->image.imageName = String_duplicate(archiveEntryNode->image.imageName);
      break;
    case ARCHIVE_ENTRY_TYPE_DIRECTORY:
      newArchiveEntryNode->directory.directoryName = String_duplicate(archiveEntryNode->directory.directoryName);
      break;
    case ARCHIVE_ENTRY_TYPE_LINK:
      newArchiveEntryNode->link.linkName = String_duplicate(archiveEntryNode->link.linkName);
      newArchiveEntryNode->link.destinationName = String_duplicate(archiveEntryNode->link.destinationName);
      break;
    case ARCHIVE_ENTRY_TYPE_SPECIAL:
      newArchiveEntryNode->special.fileName = String_duplicate(archiveEntryNode->special.fileName);
      break;
    case ARCHIVE_ENTRY_TYPE_UNKNOWN:
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return newArchiveEntryNode;
}
#endif /* 0 */

Errors Archive_create(ArchiveInfo                     *archiveInfo,
                      JobOptions                      *jobOptions,
                      ArchiveNewFileFunction          archiveNewFileFunction,
                      void                            *archiveNewFileUserData,
                      ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                      void                            *archiveGetCryptPasswordUserData,
                      DatabaseHandle                  *databaseHandle
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

  /* init */

  archiveInfo->archiveNewFileFunction          = archiveNewFileFunction;
  archiveInfo->archiveNewFileUserData          = archiveNewFileUserData;
  archiveInfo->archiveGetCryptPasswordFunction = archiveGetCryptPasswordFunction;
  archiveInfo->archiveGetCryptPasswordUserData = archiveGetCryptPasswordUserData;
  archiveInfo->jobOptions                      = jobOptions;

  archiveInfo->cryptType                       = (jobOptions->cryptAlgorithm != CRYPT_ALGORITHM_NONE)?jobOptions->cryptType:CRYPT_TYPE_NONE;
  archiveInfo->cryptPassword                   = NULL;
  archiveInfo->cryptKeyData                    = NULL;
  archiveInfo->cryptKeyDataLength              = 0;

  archiveInfo->ioType                          = ARCHIVE_IO_TYPE_FILE;
  archiveInfo->fileName                        = String_new();
  archiveInfo->file.openFlag                   = FALSE;

  archiveInfo->databaseHandle                  = databaseHandle;
  archiveInfo->storageId                       = DATABASE_ID_NONE;
  
  archiveInfo->chunkIO                         = &CHUNK_IO_FILE;
  archiveInfo->chunkIOUserData                 = &archiveInfo->file.fileHandle;

  archiveInfo->partNumber                      = 0;

  archiveInfo->nextChunkHeaderReadFlag         = FALSE;

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

Errors Archive_open(ArchiveInfo                     *archiveInfo,
                    const String                    storageName,
                    JobOptions                      *jobOptions,
                    ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                    void                            *archiveGetCryptPasswordUserData
                   )
{
  String fileName;
  Errors error;

  assert(archiveInfo != NULL);
  assert(storageName != NULL);

  /* init variables */
  fileName = String_new();

  /* init */
  archiveInfo->archiveNewFileFunction          = NULL;
  archiveInfo->archiveNewFileUserData          = NULL;
  archiveInfo->archiveGetCryptPasswordFunction = archiveGetCryptPasswordFunction;
  archiveInfo->archiveGetCryptPasswordUserData = archiveGetCryptPasswordUserData;
  archiveInfo->jobOptions                      = jobOptions;

  archiveInfo->cryptPassword                   = NULL;
  archiveInfo->cryptType                       = CRYPT_TYPE_NONE;
  archiveInfo->cryptKeyData                    = NULL;
  archiveInfo->cryptKeyDataLength              = 0;

  archiveInfo->ioType                          = ARCHIVE_IO_TYPE_STORAGE_FILE;
  archiveInfo->fileName                        = String_duplicate(storageName);

  archiveInfo->databaseHandle                  = NULL;
  archiveInfo->storageId                       = DATABASE_ID_NONE;

  archiveInfo->chunkIO                         = &CHUNK_IO_STORAGE_FILE;
  archiveInfo->chunkIOUserData                 = &archiveInfo->storageFile.storageFileHandle;

  archiveInfo->partNumber                      = 0;

  archiveInfo->nextChunkHeaderReadFlag         = FALSE;

  error = Storage_init(&archiveInfo->storageFile.storageFileHandle,
                       storageName,
                       jobOptions,
                       NULL,
                       NULL,
                       NULL,
                       NULL,
                       fileName
                      );
  if (error != ERROR_NONE)
  {
    String_delete(archiveInfo->fileName);
    String_delete(fileName);
    return error;
  }

  /* open storage file */
  error = Storage_open(&archiveInfo->storageFile.storageFileHandle,
                       fileName
                      );
  if (error != ERROR_NONE)
  {
    Storage_done(&archiveInfo->storageFile.storageFileHandle);
    String_delete(archiveInfo->fileName);
    String_delete(fileName);
    return error;
  }

  /* free resources */
  String_delete(fileName);

  return ERROR_NONE;
}

Errors Archive_close(ArchiveInfo *archiveInfo)
{
  assert(archiveInfo != NULL);

  switch (archiveInfo->ioType)
  {
    case ARCHIVE_IO_TYPE_FILE:
      if (archiveInfo->file.openFlag)
      {
        closeArchiveFile(archiveInfo,TRUE);
      }
      break;
    case ARCHIVE_IO_TYPE_STORAGE_FILE:
      Storage_close(&archiveInfo->storageFile.storageFileHandle);
      Storage_done(&archiveInfo->storageFile.storageFileHandle);
      break;
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

bool Archive_eof(ArchiveInfo *archiveInfo)
{
  bool           chunkHeaderFoundFlag;
  Errors         error;
  ChunkHeader    chunkHeader;
  bool           decryptedFlag;
  PasswordHandle passwordHandle;
  const Password *password;

  assert(archiveInfo != NULL);

  /* find next file, image, directory, link, special chunk */
  chunkHeaderFoundFlag = FALSE;
  while (!Chunk_eof(archiveInfo->chunkIO,archiveInfo->chunkIOUserData) && !chunkHeaderFoundFlag)
  {
    /* get next chunk header */
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* find next file, image, directory, link, special chunk */
    switch (chunkHeader.id)
    {
      case CHUNK_ID_BAR:
        /* bar header is simply ignored */
        error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
        if (error != ERROR_NONE)
        {
          return error;
        }
        break;
      case CHUNK_ID_KEY:
        /* check if private key available */
        if (archiveInfo->jobOptions->cryptPrivateKeyFileName == NULL)
        {
          return ERROR_NO_PRIVATE_KEY;
        }

        /* read private key, try to read key with no password, all passwords */
        Crypt_initKey(&archiveInfo->cryptKey);
        decryptedFlag = FALSE;
        error = Crypt_readKeyFile(&archiveInfo->cryptKey,
                                  archiveInfo->jobOptions->cryptPrivateKeyFileName,
                                  NULL
                                 );
        if (error == ERROR_NONE)
        {
          decryptedFlag = TRUE;
        }
        password = getFirstDecryptPassword(&passwordHandle,
                                           archiveInfo->fileName,
                                           archiveInfo->jobOptions,
                                           archiveInfo->jobOptions->cryptPasswordMode,
                                           archiveInfo->archiveGetCryptPasswordFunction,
                                           archiveInfo->archiveGetCryptPasswordUserData
                                          );
        while (   !decryptedFlag
               && (password != NULL)
              )
        {
          error = Crypt_readKeyFile(&archiveInfo->cryptKey,
                                    archiveInfo->jobOptions->cryptPrivateKeyFileName,
                                    password
                                   );
          if (error == ERROR_NONE)
          {
            decryptedFlag = TRUE;
          }
          else
          {
            /* next password */
            password = getNextDecryptPassword(&passwordHandle);
          }
        }
        if (!decryptedFlag)
        {
          return error;
        }

        /* read encryption key */
        error = readEncryptionKey(archiveInfo,&chunkHeader);
        if (error != ERROR_NONE)
        {
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
        break;
      case CHUNK_ID_FILE:
      case CHUNK_ID_IMAGE:
      case CHUNK_ID_DIRECTORY:
      case CHUNK_ID_LINK:
      case CHUNK_ID_SPECIAL:
        chunkHeaderFoundFlag = TRUE;
        break;
      default:
        error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
        if (error != ERROR_NONE)
        {
          return error;
        }
        printWarning("Skipped unexpected chunk '%s' (offset %ld)\n",Chunk_idToString(chunkHeader.id),chunkHeader.offset);
        break;
    }
  }

  if (chunkHeaderFoundFlag)
  {
    /* store chunk header for read */
    ungetNextChunkHeader(archiveInfo,&chunkHeader);
  }

  return !chunkHeaderFoundFlag;
}

Errors Archive_newFileEntry(ArchiveInfo     *archiveInfo,
                            ArchiveFileInfo *archiveFileInfo,
                            const String    fileName,
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
                                                  archiveInfo->jobOptions->cryptPasswordMode,
                                                  archiveInfo->archiveGetCryptPasswordFunction,
                                                  archiveInfo->archiveGetCryptPasswordUserData
                                                 );
    if (archiveInfo->cryptPassword == NULL)
    {
      return ERROR_NO_CRYPT_PASSWORD;
    }
  }

  /* init archive file info */
  archiveFileInfo->archiveInfo                         = archiveInfo;
  archiveFileInfo->mode                                = ARCHIVE_MODE_WRITE;

  archiveFileInfo->cryptAlgorithm                      = archiveInfo->jobOptions->cryptAlgorithm;
  archiveFileInfo->blockLength                         = archiveInfo->blockLength;

  archiveFileInfo->archiveEntryType                    = ARCHIVE_ENTRY_TYPE_FILE;

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
  archiveFileInfo->file.chunkFileEntry.name            = String_duplicate(fileName);

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
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
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
    return error;
  }

  /* init entry/data file-chunks */
  error = Chunk_init(&archiveFileInfo->file.chunkInfoFileEntry,
                     &archiveFileInfo->file.chunkInfoFile,
                     NULL,
                     NULL,
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
                     NULL,
                     NULL,
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

Errors Archive_newImageEntry(ArchiveInfo     *archiveInfo,
                             ArchiveFileInfo *archiveFileInfo,
                             const String    deviceName,
                             DeviceInfo      *deviceInfo
                            )
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(deviceInfo != NULL);
  assert(deviceInfo->blockSize > 0);

  /* init crypt password */
  if ((archiveInfo->jobOptions->cryptAlgorithm != CRYPT_ALGORITHM_NONE) && (archiveInfo->cryptPassword == NULL))
  {
    archiveInfo->cryptPassword = getCryptPassword(archiveInfo->fileName,
                                                  archiveInfo->jobOptions,
                                                  archiveInfo->jobOptions->cryptPasswordMode,
                                                  archiveInfo->archiveGetCryptPasswordFunction,
                                                  archiveInfo->archiveGetCryptPasswordUserData
                                                 );
    if (archiveInfo->cryptPassword == NULL)
    {
      return ERROR_NO_CRYPT_PASSWORD;
    }
  }

  /* init archive file info */
  archiveFileInfo->archiveInfo                        = archiveInfo;
  archiveFileInfo->mode                               = ARCHIVE_MODE_WRITE;

  archiveFileInfo->cryptAlgorithm                     = archiveInfo->jobOptions->cryptAlgorithm;
  archiveFileInfo->blockLength                        = archiveInfo->blockLength;

  archiveFileInfo->archiveEntryType                   = ARCHIVE_ENTRY_TYPE_IMAGE;

  archiveFileInfo->image.blockSize                    = deviceInfo->blockSize;
  archiveFileInfo->image.compressAlgorithm            = (deviceInfo->size > globalOptions.compressMinFileSize)?archiveInfo->jobOptions->compressAlgorithm:COMPRESS_ALGORITHM_NONE;

  archiveFileInfo->image.chunkImage.compressAlgorithm = archiveFileInfo->image.compressAlgorithm;
  archiveFileInfo->image.chunkImage.cryptAlgorithm    = archiveInfo->jobOptions->cryptAlgorithm;

  archiveFileInfo->image.chunkImageEntry.size         = deviceInfo->size;
  archiveFileInfo->image.chunkImageEntry.blockSize    = deviceInfo->blockSize;
  archiveFileInfo->image.chunkImageEntry.name         = String_duplicate(deviceName);

  archiveFileInfo->image.chunkImageData.blockOffset   = 0;
  archiveFileInfo->image.chunkImageData.blockCount    = 0;

  archiveFileInfo->image.createdFlag                  = FALSE;
  archiveFileInfo->image.headerLength                 = 0;
  archiveFileInfo->image.headerWrittenFlag            = FALSE;

  archiveFileInfo->image.buffer                       = NULL;
  archiveFileInfo->image.bufferLength                 = 0;

  /* init file-chunk */
  error = Chunk_init(&archiveFileInfo->image.chunkInfoImage,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     0,
                     NULL
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* allocate buffer */
  archiveFileInfo->image.bufferLength = (MAX_BUFFER_SIZE/archiveFileInfo->blockLength)*archiveFileInfo->blockLength;
  archiveFileInfo->image.buffer = (byte*)malloc(archiveFileInfo->image.bufferLength);
  if (archiveFileInfo->image.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init crypt */
  error = Crypt_init(&archiveFileInfo->image.cryptInfoImageEntry,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    free(archiveFileInfo->image.buffer);
    Chunk_done(&archiveFileInfo->image.chunkInfoImage);
    return error;
  }
  error = Crypt_init(&archiveFileInfo->image.cryptInfoImageData,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveFileInfo->image.cryptInfoImageEntry);
    free(archiveFileInfo->image.buffer);
    Chunk_done(&archiveFileInfo->image.chunkInfoImage);
    return error;
  }
  error = Crypt_init(&archiveFileInfo->image.cryptInfoData,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveFileInfo->image.cryptInfoImageData);
    Crypt_done(&archiveFileInfo->image.cryptInfoImageEntry);
    free(archiveFileInfo->image.buffer);
    Chunk_done(&archiveFileInfo->image.chunkInfoImage);
    return error;
  }

  /* init compress */
  error = Compress_new(&archiveFileInfo->image.compressInfoData,
                       COMPRESS_MODE_DEFLATE,
                       archiveFileInfo->image.compressAlgorithm,
                       archiveFileInfo->blockLength
                      );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveFileInfo->image.cryptInfoData);
    Crypt_done(&archiveFileInfo->image.cryptInfoImageData);
    Crypt_done(&archiveFileInfo->image.cryptInfoImageEntry);
    free(archiveFileInfo->image.buffer);
    Chunk_done(&archiveFileInfo->image.chunkInfoImage);
    return error;
  }

  /* init entry/data file-chunks */
  error = Chunk_init(&archiveFileInfo->image.chunkInfoImageEntry,
                     &archiveFileInfo->image.chunkInfoImage,
                     NULL,
                     NULL,
                     archiveFileInfo->blockLength,
                     &archiveFileInfo->image.cryptInfoImageEntry
                    );
  if (error != ERROR_NONE)
  {
    Compress_delete(&archiveFileInfo->image.compressInfoData);
    Crypt_done(&archiveFileInfo->image.cryptInfoData);
    Crypt_done(&archiveFileInfo->image.cryptInfoImageData);
    Crypt_done(&archiveFileInfo->image.cryptInfoImageEntry);
    free(archiveFileInfo->image.buffer);
    Chunk_done(&archiveFileInfo->image.chunkInfoImage);
    return error;
  }
  error = Chunk_init(&archiveFileInfo->image.chunkInfoImageData,
                     &archiveFileInfo->image.chunkInfoImage,
                     NULL,
                     NULL,
                     archiveFileInfo->blockLength,
                     &archiveFileInfo->image.cryptInfoImageData
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->image.chunkInfoImageEntry);
    Compress_delete(&archiveFileInfo->image.compressInfoData);
    Crypt_done(&archiveFileInfo->image.cryptInfoData);
    Crypt_done(&archiveFileInfo->image.cryptInfoImageData);
    Crypt_done(&archiveFileInfo->image.cryptInfoImageEntry);
    free(archiveFileInfo->image.buffer);
    Chunk_done(&archiveFileInfo->image.chunkInfoImage);
    return error;
  }

  /* calculate header length */
  archiveFileInfo->image.headerLength = Chunk_getSize(CHUNK_DEFINITION_IMAGE,      0,                           &archiveFileInfo->image.chunkImage     )+
                                        Chunk_getSize(CHUNK_DEFINITION_IMAGE_ENTRY,archiveFileInfo->blockLength,&archiveFileInfo->image.chunkImageEntry)+
                                        Chunk_getSize(CHUNK_DEFINITION_IMAGE_DATA, archiveFileInfo->blockLength,&archiveFileInfo->image.chunkImageData );

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
                                                  archiveInfo->jobOptions->cryptPasswordMode,
                                                  archiveInfo->archiveGetCryptPasswordFunction,
                                                  archiveInfo->archiveGetCryptPasswordUserData
                                                 );
    if (archiveInfo->cryptPassword == NULL)
    {
      return ERROR_NO_CRYPT_PASSWORD;
    }
  }

  /* init archive file info */
  archiveFileInfo->archiveInfo                                   = archiveInfo;
  archiveFileInfo->mode                                          = ARCHIVE_MODE_WRITE;

  archiveFileInfo->cryptAlgorithm                                = archiveInfo->jobOptions->cryptAlgorithm;
  archiveFileInfo->blockLength                                   = archiveInfo->blockLength;

  archiveFileInfo->archiveEntryType                              = ARCHIVE_ENTRY_TYPE_DIRECTORY;

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
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
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
                     NULL,
                     NULL,
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
                                                  archiveInfo->jobOptions->cryptPasswordMode,
                                                  archiveInfo->archiveGetCryptPasswordFunction,
                                                  archiveInfo->archiveGetCryptPasswordUserData
                                                 );
    if (archiveInfo->cryptPassword == NULL)
    {
      return ERROR_NO_CRYPT_PASSWORD;
    }
  }

  /* init archive file info */
  archiveFileInfo->archiveInfo                         = archiveInfo;
  archiveFileInfo->mode                                = ARCHIVE_MODE_WRITE;

  archiveFileInfo->cryptAlgorithm                      = archiveInfo->jobOptions->cryptAlgorithm;
  archiveFileInfo->blockLength                         = archiveInfo->blockLength;

  archiveFileInfo->archiveEntryType                    = ARCHIVE_ENTRY_TYPE_LINK;

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
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
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
                     NULL,
                     NULL,
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
                                                  archiveInfo->jobOptions->cryptPasswordMode,
                                                  archiveInfo->archiveGetCryptPasswordFunction,
                                                  archiveInfo->archiveGetCryptPasswordUserData
                                                 );
    if (archiveInfo->cryptPassword == NULL)
    {
      return ERROR_NO_CRYPT_PASSWORD;
    }
  }

  /* init archive file info */
  archiveFileInfo->archiveInfo                               = archiveInfo;
  archiveFileInfo->mode                                      = ARCHIVE_MODE_WRITE;

  archiveFileInfo->cryptAlgorithm                            = archiveInfo->jobOptions->cryptAlgorithm;
  archiveFileInfo->blockLength                               = archiveInfo->blockLength;

  archiveFileInfo->archiveEntryType                          = ARCHIVE_ENTRY_TYPE_SPECIAL;

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
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
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
                     NULL,
                     NULL,
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

Errors Archive_getNextArchiveEntryType(ArchiveInfo       *archiveInfo,
                                       ArchiveEntryTypes *archiveEntryType
                                      )
{
  Errors         error;
  ChunkHeader    chunkHeader;
  bool           decryptedFlag;
  PasswordHandle passwordHandle;
  const Password *password;

  assert(archiveInfo != NULL);
  assert(archiveEntryType != NULL);

  /* find next file, image, directory, link, special chunk */
  do
  {
    /* get next chunk */
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      return error;
    }

    switch (chunkHeader.id)
    {
      case CHUNK_ID_BAR:
        /* bar header is simply ignored */
        error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
        if (error != ERROR_NONE)
        {
          return error;
        }
        break;
      case CHUNK_ID_KEY:
        /* check if private key available */
        if (archiveInfo->jobOptions->cryptPrivateKeyFileName == NULL)
        {
          return ERROR_NO_PRIVATE_KEY;
        }

        /* read private key, try to read key with no password, all passwords */
        Crypt_initKey(&archiveInfo->cryptKey);
        decryptedFlag = FALSE;
        error = Crypt_readKeyFile(&archiveInfo->cryptKey,
                                  archiveInfo->jobOptions->cryptPrivateKeyFileName,
                                  NULL
                                 );
        if (error == ERROR_NONE)
        {
          decryptedFlag = TRUE;
        }
        password = getFirstDecryptPassword(&passwordHandle,
                                           archiveInfo->fileName,
                                           archiveInfo->jobOptions,
                                           archiveInfo->jobOptions->cryptPasswordMode,
                                           archiveInfo->archiveGetCryptPasswordFunction,
                                           archiveInfo->archiveGetCryptPasswordUserData
                                          );
        while (   !decryptedFlag
               && (password != NULL)
              )
        {
          error = Crypt_readKeyFile(&archiveInfo->cryptKey,
                                    archiveInfo->jobOptions->cryptPrivateKeyFileName,
                                    password
                                   );
          if (error == ERROR_NONE)
          {
            decryptedFlag = TRUE;
          }
          else
          {
            /* next password */
            password = getNextDecryptPassword(&passwordHandle);
          }
        }
        if (!decryptedFlag)
        {
          return error;
        }

        /* read encryption key */
        error = readEncryptionKey(archiveInfo,&chunkHeader);
        if (error != ERROR_NONE)
        {
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
        break;
      case CHUNK_ID_FILE:
      case CHUNK_ID_IMAGE:
      case CHUNK_ID_DIRECTORY:
      case CHUNK_ID_LINK:
      case CHUNK_ID_SPECIAL:
        break;
      default:
        /* skip unknown chunks */
        error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
        if (error != ERROR_NONE)
        {
          return error;
        }
        printWarning("Skipped unexpected chunk '%s'\n",Chunk_idToString(chunkHeader.id));
        break;
    }
  }
  while (   (chunkHeader.id != CHUNK_ID_FILE)
         && (chunkHeader.id != CHUNK_ID_IMAGE)
         && (chunkHeader.id != CHUNK_ID_DIRECTORY)
         && (chunkHeader.id != CHUNK_ID_LINK)
         && (chunkHeader.id != CHUNK_ID_SPECIAL)
        );

  /* get file type */
  switch (chunkHeader.id)
  {
    case CHUNK_ID_FILE:      (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_FILE;      break;
    case CHUNK_ID_IMAGE:     (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_IMAGE;     break;
    case CHUNK_ID_DIRECTORY: (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_DIRECTORY; break;
    case CHUNK_ID_LINK:      (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_LINK;      break;
    case CHUNK_ID_SPECIAL:   (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_SPECIAL;   break;
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
                             String             fileName,
                             FileInfo           *fileInfo,
                             uint64             *fragmentOffset,
                             uint64             *fragmentSize
                            )
{
  Errors         error;
  ChunkHeader    chunkHeader;
  uint64         index;
  PasswordHandle passwordHandle;
  const Password *password;
  bool           passwordFlag;
  bool           decryptedFlag;
  ChunkHeader    subChunkHeader;
  bool           foundFileEntryFlag,foundFileDataFlag;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo       = archiveInfo;
  archiveFileInfo->mode              = ARCHIVE_MODE_READ;
  archiveFileInfo->archiveEntryType  = ARCHIVE_ENTRY_TYPE_FILE;

  archiveFileInfo->file.buffer       = NULL;
  archiveFileInfo->file.bufferLength = 0;

  /* init file chunk */
  error = Chunk_init(&archiveFileInfo->file.chunkInfoFile,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
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
      error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
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
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  archiveFileInfo->cryptAlgorithm         = archiveFileInfo->file.chunkFile.cryptAlgorithm;
  archiveFileInfo->file.compressAlgorithm = archiveFileInfo->file.chunkFile.compressAlgorithm;

  /* detect block length of used crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
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
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
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
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo->fileName,
                                         archiveInfo->jobOptions,
                                         archiveInfo->jobOptions->cryptPasswordMode,
                                         archiveInfo->archiveGetCryptPasswordFunction,
                                         archiveInfo->archiveGetCryptPasswordUserData
                                        );
    }
    passwordFlag  = (password != NULL);
    decryptedFlag = FALSE;
  }
  else
  {
    password      = NULL;
    passwordFlag  = FALSE;
    decryptedFlag = TRUE;
  }
  foundFileEntryFlag = FALSE;
  foundFileDataFlag  = FALSE;
  while (   !foundFileEntryFlag
         && !foundFileDataFlag
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
                         NULL,
                         NULL,
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
                         NULL,
                         NULL,
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

            String_set(fileName,archiveFileInfo->file.chunkFileEntry.name);
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

    if (error != ERROR_NONE)
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
        password = getNextDecryptPassword(&passwordHandle);
      }
    }
  } /* while */

  if (!foundFileEntryFlag || !foundFileDataFlag)
  {
    if (foundFileEntryFlag) String_delete(archiveFileInfo->file.chunkFileEntry.name);
    Compress_delete(&archiveFileInfo->file.compressInfoData);
    free(archiveFileInfo->file.buffer);
    Chunk_done(&archiveFileInfo->file.chunkInfoFile);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);

    if      (error != ERROR_NONE) return error;
    else if (!passwordFlag)       return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)      return ERROR_INVALID_PASSWORD;
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

Errors Archive_readImageEntry(ArchiveInfo        *archiveInfo,
                              ArchiveFileInfo    *archiveFileInfo,
                              CompressAlgorithms *compressAlgorithm,
                              CryptAlgorithms    *cryptAlgorithm,
                              CryptTypes         *cryptType,
                              String             deviceName,
                              DeviceInfo         *deviceInfo,
                              uint64             *blockOffset,
                              uint64             *blockCount
                             )
{
  Errors         error;
  ChunkHeader    chunkHeader;
  uint64         index;
  PasswordHandle passwordHandle;
  const Password *password;
  bool           passwordFlag;
  bool           decryptedFlag;
  ChunkHeader    subChunkHeader;
  bool           foundImageEntryFlag,foundImageDataFlag;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(deviceInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo        = archiveInfo;
  archiveFileInfo->mode               = ARCHIVE_MODE_READ;
//???
  archiveFileInfo->archiveEntryType   = ARCHIVE_ENTRY_TYPE_IMAGE;

  archiveFileInfo->image.buffer       = NULL;
  archiveFileInfo->image.bufferLength = 0;

  /* init file chunk */
  error = Chunk_init(&archiveFileInfo->image.chunkInfoImage,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     0,
                     NULL
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* find next image chunk */
  do
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveFileInfo->image.chunkInfoImage);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_IMAGE)
    {
      error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveFileInfo->image.chunkInfoImage);
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_IMAGE);

  /* read image chunk, find image data */
  error = Chunk_open(&archiveFileInfo->image.chunkInfoImage,
                     &chunkHeader,
                     CHUNK_ID_IMAGE,
                     CHUNK_DEFINITION_IMAGE,
                     Chunk_getSize(CHUNK_DEFINITION_IMAGE,0,NULL),
                     &archiveFileInfo->image.chunkImage
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->image.chunkInfoImage);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  archiveFileInfo->cryptAlgorithm          = archiveFileInfo->image.chunkImage.cryptAlgorithm;
  archiveFileInfo->image.compressAlgorithm = archiveFileInfo->image.chunkImage.compressAlgorithm;

  /* detect block length of used crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->image.chunkInfoImage);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  assert(archiveFileInfo->blockLength > 0);

  /* allocate buffer */
  archiveFileInfo->image.bufferLength = (MAX_BUFFER_SIZE/archiveFileInfo->blockLength)*archiveFileInfo->blockLength;
  archiveFileInfo->image.buffer = (byte*)malloc(archiveFileInfo->image.bufferLength);
  if (archiveFileInfo->image.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init compress */
  error = Compress_new(&archiveFileInfo->image.compressInfoData,
                       COMPRESS_MODE_INFLATE,
                       archiveFileInfo->image.compressAlgorithm,
                       archiveFileInfo->blockLength
                      );
  if (error != ERROR_NONE)
  {
    free(archiveFileInfo->image.buffer);
    Chunk_done(&archiveFileInfo->image.chunkInfoImage);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }

  /* try to read image entry with all passwords */
  Chunk_tell(&archiveFileInfo->image.chunkInfoImage,&index);
  if (archiveFileInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo->fileName,
                                         archiveInfo->jobOptions,
                                         archiveInfo->jobOptions->cryptPasswordMode,
                                         archiveInfo->archiveGetCryptPasswordFunction,
                                         archiveInfo->archiveGetCryptPasswordUserData
                                        );
    }
    passwordFlag  = (password != NULL);
    decryptedFlag = FALSE;
  }
  else
  {
    password      = NULL;
    passwordFlag  = FALSE;
    decryptedFlag = TRUE;
  }
  foundImageEntryFlag = FALSE;
  foundImageDataFlag  = FALSE;
  while (   !foundImageEntryFlag
         && !foundImageDataFlag
         && ((archiveFileInfo->cryptAlgorithm == CRYPT_ALGORITHM_NONE) || (password != NULL))
        )
  {
    error = ERROR_NONE;

    /* reset chunk read position */
    Chunk_seek(&archiveFileInfo->image.chunkInfoImage,index);

    /* init crypt */
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveFileInfo->image.cryptInfoImageEntry,
                         archiveFileInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveFileInfo->image.cryptInfoImageData,
                         archiveFileInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveFileInfo->image.cryptInfoImageEntry);
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveFileInfo->image.cryptInfoData,
                         archiveFileInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveFileInfo->image.cryptInfoImageData);
        Crypt_done(&archiveFileInfo->image.cryptInfoImageEntry);
      }
    }

    /* init image entry/data chunks */
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveFileInfo->image.chunkInfoImageEntry,
                         &archiveFileInfo->image.chunkInfoImage,
                         NULL,
                         NULL,
                         archiveFileInfo->blockLength,
                         &archiveFileInfo->image.cryptInfoImageEntry
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveFileInfo->image.cryptInfoData);
        Crypt_done(&archiveFileInfo->image.cryptInfoImageData);
        Crypt_done(&archiveFileInfo->image.cryptInfoImageEntry);
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveFileInfo->image.chunkInfoImageData,
                         &archiveFileInfo->image.chunkInfoImage,
                         NULL,
                         NULL,
                         archiveFileInfo->blockLength,
                         &archiveFileInfo->image.cryptInfoImageData
                        );
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveFileInfo->image.chunkInfoImageEntry);
        Crypt_done(&archiveFileInfo->image.cryptInfoData);
        Crypt_done(&archiveFileInfo->image.cryptInfoImageData);
        Crypt_done(&archiveFileInfo->image.cryptInfoImageEntry);
      }
    }

    /* try to read image entry/data chunks */
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveFileInfo->image.chunkInfoImage)
             && (error == ERROR_NONE)
             && (!foundImageEntryFlag || !foundImageDataFlag)
            )
      {
        error = Chunk_nextSub(&archiveFileInfo->image.chunkInfoImage,&subChunkHeader);
        if (error != ERROR_NONE)
        {
          break;
        }

        switch (subChunkHeader.id)
        {
          case CHUNK_ID_IMAGE_ENTRY:
            error = Chunk_open(&archiveFileInfo->image.chunkInfoImageEntry,
                               &subChunkHeader,
                               CHUNK_ID_IMAGE_ENTRY,
                               CHUNK_DEFINITION_IMAGE_ENTRY,
                               subChunkHeader.size,
                               &archiveFileInfo->image.chunkImageEntry
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            String_set(deviceName,archiveFileInfo->image.chunkImageEntry.name);
            deviceInfo->size      = archiveFileInfo->image.chunkImageEntry.size;
            deviceInfo->blockSize = archiveFileInfo->image.chunkImageEntry.blockSize;

            foundImageEntryFlag = TRUE;
            break;
          case CHUNK_ID_IMAGE_DATA:
            error = Chunk_open(&archiveFileInfo->image.chunkInfoImageData,
                               &subChunkHeader,
                               CHUNK_ID_IMAGE_DATA,
                               CHUNK_DEFINITION_IMAGE_DATA,
                               Chunk_getSize(CHUNK_DEFINITION_FILE_DATA,archiveFileInfo->blockLength,NULL),
                               &archiveFileInfo->image.chunkImageData
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            if (blockOffset != NULL) (*blockOffset) = archiveFileInfo->image.chunkImageData.blockOffset;
            if (blockCount  != NULL) (*blockCount)  = archiveFileInfo->image.chunkImageData.blockCount;

            foundImageDataFlag = TRUE;
            break;
          default:
            error = Chunk_skipSub(&archiveFileInfo->image.chunkInfoImage,&subChunkHeader);
            if (error != ERROR_NONE)
            {
              break;
            }
            break;
        }
      }
    }

    if (error != ERROR_NONE)
    {
      /* free resources */
      Chunk_done(&archiveFileInfo->image.chunkInfoImageData);
      Chunk_done(&archiveFileInfo->image.chunkInfoImageEntry);
      Crypt_done(&archiveFileInfo->image.cryptInfoData);
      Crypt_done(&archiveFileInfo->image.cryptInfoImageData);
      Crypt_done(&archiveFileInfo->image.cryptInfoImageEntry);

      if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
      {
        /* no more passwords when asymmetric encryption used */
        password = NULL;
      }
      else
      {
        /* next password */
        password = getNextDecryptPassword(&passwordHandle);
      }
    }
  } /* while */

  if (!foundImageEntryFlag || !foundImageDataFlag)
  {
    if (foundImageEntryFlag) String_delete(archiveFileInfo->image.chunkImageEntry.name);
    Compress_delete(&archiveFileInfo->image.compressInfoData);
    free(archiveFileInfo->image.buffer);
    Chunk_done(&archiveFileInfo->image.chunkInfoImage);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);

    if      (error != ERROR_NONE)  return error;
    else if (!passwordFlag)        return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)       return ERROR_INVALID_PASSWORD;
    else if (!foundImageEntryFlag) return ERROR_NO_IMAGE_ENTRY;
    else if (!foundImageDataFlag)  return ERROR_NO_IMAGE_DATA;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (compressAlgorithm != NULL) (*compressAlgorithm) = archiveFileInfo->image.compressAlgorithm;
  if (cryptAlgorithm    != NULL) (*cryptAlgorithm)    = archiveFileInfo->cryptAlgorithm;
  if (cryptType         != NULL) (*cryptType)         = archiveInfo->cryptType;

  /* reset compress, crypt */
  Compress_reset(&archiveFileInfo->image.compressInfoData);
  Crypt_reset(&archiveFileInfo->image.cryptInfoImageData,0);

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
  PasswordHandle passwordHandle;
  const Password *password;
  bool           passwordFlag;
  bool           decryptedFlag;
  bool           foundDirectoryEntryFlag;
  ChunkHeader    subChunkHeader;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo      = archiveInfo;
  archiveFileInfo->mode             = ARCHIVE_MODE_READ;
  archiveFileInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_DIRECTORY;

  /* init file chunk */
  error = Chunk_init(&archiveFileInfo->directory.chunkInfoDirectory,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
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
      error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
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
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  archiveFileInfo->cryptAlgorithm = archiveFileInfo->directory.chunkDirectory.cryptAlgorithm;

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
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
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo->fileName,
                                         archiveInfo->jobOptions,
                                         archiveInfo->jobOptions->cryptPasswordMode,
                                         archiveInfo->archiveGetCryptPasswordFunction,
                                         archiveInfo->archiveGetCryptPasswordUserData
                                        );
    }
    passwordFlag  = (password != NULL);
    decryptedFlag = FALSE;
  }
  else
  {
    password      = NULL;
    passwordFlag  = FALSE;
    decryptedFlag = TRUE;
  }
  foundDirectoryEntryFlag = FALSE;
  while (   !foundDirectoryEntryFlag
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
                         NULL,
                         NULL,
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

    if (error != ERROR_NONE)
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
        password = getNextDecryptPassword(&passwordHandle);
      }
    }
  } /* while */

  if (!foundDirectoryEntryFlag)
  {
    Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);

    if      (error != ERROR_NONE)      return error;
    else if (!passwordFlag)            return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)           return ERROR_INVALID_PASSWORD;
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
  PasswordHandle passwordHandle;
  const Password *password;
  bool           passwordFlag;
  bool           decryptedFlag;
  bool           foundLinkEntryFlag;
  ChunkHeader    subChunkHeader;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo      = archiveInfo;
  archiveFileInfo->mode             = ARCHIVE_MODE_READ;
  archiveFileInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_LINK;

  /* init file chunk */
  error = Chunk_init(&archiveFileInfo->link.chunkInfoLink,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
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
      Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
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
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  archiveFileInfo->cryptAlgorithm = archiveFileInfo->link.chunkLink.cryptAlgorithm;

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
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
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo->fileName,
                                         archiveInfo->jobOptions,
                                         archiveInfo->jobOptions->cryptPasswordMode,
                                         archiveInfo->archiveGetCryptPasswordFunction,
                                         archiveInfo->archiveGetCryptPasswordUserData
                                        );
    }
    passwordFlag  = (password != NULL);
    decryptedFlag = FALSE;
  }
  else
  {
    password      = NULL;
    passwordFlag  = FALSE;
    decryptedFlag = TRUE;
  }
  decryptedFlag      = FALSE;
  foundLinkEntryFlag = FALSE;
  while (   !foundLinkEntryFlag
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
                         NULL,
                         NULL,
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

    if (error != ERROR_NONE)
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
        password = getNextDecryptPassword(&passwordHandle);
      }
    }
  } /* while */

  if (!foundLinkEntryFlag)
  {
    Chunk_done(&archiveFileInfo->link.chunkInfoLink);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);

    if      (error != ERROR_NONE) return error;
    else if (!passwordFlag)       return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)      return ERROR_INVALID_PASSWORD;
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
  PasswordHandle passwordHandle;
  const Password *password;
  bool           passwordFlag;
  bool           decryptedFlag;
  bool           foundSpecialEntryFlag;
  ChunkHeader    subChunkHeader;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo      = archiveInfo;
  archiveFileInfo->mode             = ARCHIVE_MODE_READ;
  archiveFileInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_SPECIAL;

  /* init file chunk */
  error = Chunk_init(&archiveFileInfo->special.chunkInfoSpecial,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
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
      Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
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
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  archiveFileInfo->cryptAlgorithm = archiveFileInfo->special.chunkSpecial.cryptAlgorithm;

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveFileInfo->cryptAlgorithm,&archiveFileInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  assert(archiveFileInfo->blockLength > 0);

  /* try to read special entry with all passwords */
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
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo->fileName,
                                         archiveInfo->jobOptions,
                                         archiveInfo->jobOptions->cryptPasswordMode,
                                         archiveInfo->archiveGetCryptPasswordFunction,
                                         archiveInfo->archiveGetCryptPasswordUserData
                                        );
    }
    passwordFlag  = (password != NULL);
    decryptedFlag = FALSE;
  }
  else
  {
    password      = NULL;
    passwordFlag  = FALSE;
    decryptedFlag = TRUE;
  }
  foundSpecialEntryFlag = FALSE;
  while (   !foundSpecialEntryFlag
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
                         NULL,
                         NULL,
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

    if (error != ERROR_NONE)
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
        password = getNextDecryptPassword(&passwordHandle);
      }
    }
  } /* while */

  if (!foundSpecialEntryFlag)
  {
    Chunk_done(&archiveFileInfo->special.chunkInfoSpecial);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);

    if      (error != ERROR_NONE)    return error;
    else if (!passwordFlag)          return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)         return ERROR_INVALID_PASSWORD;
    else if (!foundSpecialEntryFlag) return ERROR_NO_SPECIAL_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveFileInfo->cryptAlgorithm;
  if (cryptType      != NULL) (*cryptType)      = archiveInfo->cryptType;

  return ERROR_NONE;
}

Errors Archive_closeEntry(ArchiveFileInfo *archiveFileInfo)
{
  Errors error,tmpError;

  assert(archiveFileInfo != NULL);
  assert(archiveFileInfo->archiveInfo != NULL);

  error = ERROR_NONE;
  switch (archiveFileInfo->mode)
  {
    case ARCHIVE_MODE_WRITE:
      switch (archiveFileInfo->archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          /* flush data */
          Compress_flush(&archiveFileInfo->file.compressInfoData);
          if (   !archiveFileInfo->file.createdFlag
              || (Compress_getAvailableBlocks(&archiveFileInfo->file.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
             )
          {
            while (   (error == ERROR_NONE)
                   && (Compress_getAvailableBlocks(&archiveFileInfo->file.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
                  )
            {
              error = writeFileDataBlock(archiveFileInfo,BLOCK_MODE_WRITE,FALSE);
            }
            if (error == ERROR_NONE)
            {
              error = writeFileDataBlock(archiveFileInfo,BLOCK_MODE_FLUSH,FALSE);
            }
          }

          /* update file and chunks if header is written */
          if (archiveFileInfo->file.headerWrittenFlag)
          {
            /* update part size */
            archiveFileInfo->file.chunkFileData.fragmentSize = Compress_getInputLength(&archiveFileInfo->file.compressInfoData);
            if (error == ERROR_NONE)
            {
              tmpError = Chunk_update(&archiveFileInfo->file.chunkInfoFileData,
                                      &archiveFileInfo->file.chunkFileData
                                     );
              if (tmpError != ERROR_NONE) error = tmpError;
            }

            /* close chunks */
            tmpError = Chunk_close(&archiveFileInfo->file.chunkInfoFileData);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveFileInfo->file.chunkInfoFileEntry);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveFileInfo->file.chunkInfoFile);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
          }

          if (archiveFileInfo->archiveInfo->databaseHandle != NULL)
          {
            /* store in index database */
            if (error == ERROR_NONE)
            {
              error = Index_addFile(archiveFileInfo->archiveInfo->databaseHandle,
                                    archiveFileInfo->archiveInfo->storageId,
                                    archiveFileInfo->file.chunkFileEntry.name,
                                    archiveFileInfo->file.chunkFileEntry.size,
                                    archiveFileInfo->file.chunkFileEntry.timeLastAccess,
                                    archiveFileInfo->file.chunkFileEntry.timeModified,
                                    archiveFileInfo->file.chunkFileEntry.timeLastChanged,
                                    archiveFileInfo->file.chunkFileEntry.userId,
                                    archiveFileInfo->file.chunkFileEntry.groupId,
                                    archiveFileInfo->file.chunkFileEntry.permission,
                                    archiveFileInfo->file.chunkFileData.fragmentOffset,
                                    archiveFileInfo->file.chunkFileData.fragmentSize
                                   );
            }
          }

          /* call back new archive entry created */
          if (error == ERROR_NONE)
          {
/*
            if (archiveInfo->archiveNewEntryFunction != NULL)
            {
              archiveEntry.fileName = archiveFileInfo->file.
              error = archiveInfo->archiveNewEntryFunction(archiveFileInfo->archiveInfo->archiveNewEntryData,
                                                           &archiveInfo->archiveEntry
                                                          );
            }
*/
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
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          /* flush data */
          Compress_flush(&archiveFileInfo->image.compressInfoData);
          if (   !archiveFileInfo->image.createdFlag
              || (Compress_getAvailableBlocks(&archiveFileInfo->image.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
             )
          {
            while (   (error == ERROR_NONE)
                   && (Compress_getAvailableBlocks(&archiveFileInfo->image.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
                  )
            {
              error = writeImageDataBlock(archiveFileInfo,BLOCK_MODE_WRITE,FALSE);
            }
            if (error == ERROR_NONE)
            {
              error = writeImageDataBlock(archiveFileInfo,BLOCK_MODE_FLUSH,FALSE);
            }
          }

          /* update file and chunks if header is written */
          if (archiveFileInfo->image.headerWrittenFlag)
          {
            /* update part block count */
            assert(archiveFileInfo->image.blockSize > 0);
            assert((Compress_getInputLength(&archiveFileInfo->image.compressInfoData) % archiveFileInfo->image.blockSize) == 0);
            archiveFileInfo->image.chunkImageData.blockCount = Compress_getInputLength(&archiveFileInfo->image.compressInfoData)/archiveFileInfo->image.blockSize;
            if (error == ERROR_NONE)
            {
              tmpError = Chunk_update(&archiveFileInfo->image.chunkInfoImageData,
                                      &archiveFileInfo->image.chunkImageData
                                     );
              if (tmpError != ERROR_NONE) error = tmpError;
            }

            /* close chunks */
            tmpError = Chunk_close(&archiveFileInfo->image.chunkInfoImageData);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveFileInfo->image.chunkInfoImageEntry);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveFileInfo->image.chunkInfoImage);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
          }

          if (archiveFileInfo->archiveInfo->databaseHandle != NULL)
          {
            /* store in index database */
            if (error == ERROR_NONE)
            {
              error = Index_addImage(archiveFileInfo->archiveInfo->databaseHandle,
                                     archiveFileInfo->archiveInfo->storageId,
                                     archiveFileInfo->image.chunkImageEntry.name,
                                     archiveFileInfo->image.chunkImageEntry.size,
                                     archiveFileInfo->image.chunkImageEntry.blockSize,
                                     archiveFileInfo->image.chunkImageData.blockOffset,
                                     archiveFileInfo->image.chunkImageData.blockCount
                                    );
            }
          }

          /* call back new archive entry created */

          /* free resources */
          Chunk_done(&archiveFileInfo->image.chunkInfoImageData);
          Chunk_done(&archiveFileInfo->image.chunkInfoImageEntry);
          Compress_delete(&archiveFileInfo->image.compressInfoData);
          Crypt_done(&archiveFileInfo->image.cryptInfoData);
          Crypt_done(&archiveFileInfo->image.cryptInfoImageData);
          Crypt_done(&archiveFileInfo->image.cryptInfoImageEntry);
          Chunk_done(&archiveFileInfo->image.chunkInfoImage);
          free(archiveFileInfo->image.buffer);
          String_delete(archiveFileInfo->image.chunkImageEntry.name);
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          /* close chunks */
          tmpError = Chunk_close(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
          tmpError = Chunk_close(&archiveFileInfo->directory.chunkInfoDirectory);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

          if (archiveFileInfo->archiveInfo->databaseHandle != NULL)
          {
            /* store in index database */
            if (error == ERROR_NONE)
            {
              error = Index_addDirectory(archiveFileInfo->archiveInfo->databaseHandle,
                                         archiveFileInfo->archiveInfo->storageId,
                                         archiveFileInfo->directory.chunkDirectoryEntry.name,
                                         archiveFileInfo->directory.chunkDirectoryEntry.timeLastAccess,
                                         archiveFileInfo->directory.chunkDirectoryEntry.timeModified,
                                         archiveFileInfo->directory.chunkDirectoryEntry.timeLastChanged,
                                         archiveFileInfo->directory.chunkDirectoryEntry.userId,
                                         archiveFileInfo->directory.chunkDirectoryEntry.groupId,
                                         archiveFileInfo->directory.chunkDirectoryEntry.permission
                                        );
            }
          }

          /* call back new archive entry created */

          /* free resources */
          Chunk_done(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
          Crypt_done(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
          Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
          String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          /* close chunks */
          tmpError = Chunk_close(&archiveFileInfo->link.chunkInfoLinkEntry);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
          tmpError = Chunk_close(&archiveFileInfo->link.chunkInfoLink);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

          if (archiveFileInfo->archiveInfo->databaseHandle != NULL)
          {
            /* store in index database */
            if (error == ERROR_NONE)
            {
              error = Index_addLink(archiveFileInfo->archiveInfo->databaseHandle,
                                    archiveFileInfo->archiveInfo->storageId,
                                    archiveFileInfo->link.chunkLinkEntry.name,
                                    archiveFileInfo->link.chunkLinkEntry.destinationName,
                                    archiveFileInfo->link.chunkLinkEntry.timeLastAccess,
                                    archiveFileInfo->link.chunkLinkEntry.timeModified,
                                    archiveFileInfo->link.chunkLinkEntry.timeLastChanged,
                                    archiveFileInfo->link.chunkLinkEntry.userId,
                                    archiveFileInfo->link.chunkLinkEntry.groupId,
                                    archiveFileInfo->link.chunkLinkEntry.permission
                                   );
            }
          }

          /* call back new archive entry created */

          /* free resources */
          Chunk_done(&archiveFileInfo->link.chunkInfoLinkEntry);
          Crypt_done(&archiveFileInfo->link.cryptInfoLinkEntry);
          Chunk_done(&archiveFileInfo->link.chunkInfoLink);
          String_delete(archiveFileInfo->link.chunkLinkEntry.name);
          String_delete(archiveFileInfo->link.chunkLinkEntry.destinationName);
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          /* close chunks */
          tmpError = Chunk_close(&archiveFileInfo->special.chunkInfoSpecialEntry);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
          tmpError = Chunk_close(&archiveFileInfo->special.chunkInfoSpecial);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

          if (archiveFileInfo->archiveInfo->databaseHandle != NULL)
          {
            /* store in index database */
            if (error == ERROR_NONE)
            {
              error = Index_addSpecial(archiveFileInfo->archiveInfo->databaseHandle,
                                       archiveFileInfo->archiveInfo->storageId,
                                       archiveFileInfo->special.chunkSpecialEntry.name,
                                       archiveFileInfo->special.chunkSpecialEntry.specialType,
                                       archiveFileInfo->special.chunkSpecialEntry.timeLastAccess,
                                       archiveFileInfo->special.chunkSpecialEntry.timeModified,
                                       archiveFileInfo->special.chunkSpecialEntry.timeLastChanged,
                                       archiveFileInfo->special.chunkSpecialEntry.userId,
                                       archiveFileInfo->special.chunkSpecialEntry.groupId,
                                       archiveFileInfo->special.chunkSpecialEntry.permission,
                                       archiveFileInfo->special.chunkSpecialEntry.major,
                                       archiveFileInfo->special.chunkSpecialEntry.minor
                                      );
            }
          }

          /* call back new archive entry created */

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
    case ARCHIVE_MODE_READ:
      switch (archiveFileInfo->archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          /* close chunks */
          tmpError = Chunk_close(&archiveFileInfo->file.chunkInfoFileData);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
          tmpError = Chunk_close(&archiveFileInfo->file.chunkInfoFileEntry);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
          tmpError = Chunk_close(&archiveFileInfo->file.chunkInfoFile);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

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
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          /* close chunks */
          tmpError = Chunk_close(&archiveFileInfo->image.chunkInfoImageData);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
          tmpError = Chunk_close(&archiveFileInfo->image.chunkInfoImageEntry);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
          tmpError = Chunk_close(&archiveFileInfo->image.chunkInfoImage);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

          /* free resources */
          Chunk_done(&archiveFileInfo->image.chunkInfoImageData);
          Chunk_done(&archiveFileInfo->image.chunkInfoImageEntry);
          Compress_delete(&archiveFileInfo->image.compressInfoData);
          Crypt_done(&archiveFileInfo->image.cryptInfoData);
          Crypt_done(&archiveFileInfo->image.cryptInfoImageData);
          Crypt_done(&archiveFileInfo->image.cryptInfoImageEntry);
          Chunk_done(&archiveFileInfo->image.chunkInfoImage);
          free(archiveFileInfo->image.buffer);
          String_delete(archiveFileInfo->image.chunkImageEntry.name);
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          /* close chunks */
          tmpError = Chunk_close(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
          tmpError = Chunk_close(&archiveFileInfo->directory.chunkInfoDirectory);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

          /* free resources */
          Chunk_done(&archiveFileInfo->directory.chunkInfoDirectoryEntry);
          Crypt_done(&archiveFileInfo->directory.cryptInfoDirectoryEntry);
          Chunk_done(&archiveFileInfo->directory.chunkInfoDirectory);
          String_delete(archiveFileInfo->directory.chunkDirectoryEntry.name);
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          /* close chunks */
          tmpError = Chunk_close(&archiveFileInfo->link.chunkInfoLinkEntry);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
          tmpError = Chunk_close(&archiveFileInfo->link.chunkInfoLink);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

          /* free resources */
          Chunk_done(&archiveFileInfo->link.chunkInfoLinkEntry);
          Crypt_done(&archiveFileInfo->link.cryptInfoLinkEntry);
          Chunk_done(&archiveFileInfo->link.chunkInfoLink);
          String_delete(archiveFileInfo->link.chunkLinkEntry.name);
          String_delete(archiveFileInfo->link.chunkLinkEntry.destinationName);
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          /* close chunks */
          tmpError = Chunk_close(&archiveFileInfo->special.chunkInfoSpecialEntry);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
          tmpError = Chunk_close(&archiveFileInfo->special.chunkInfoSpecial);
          if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

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

  return error;
}

Errors Archive_writeData(ArchiveFileInfo *archiveFileInfo,
                         const void      *buffer,
                         ulong           length,
                         uint            elementSize
                        )
{
  byte   *p;
  ulong  writtenBytes;
  Errors error;
  ulong  blockLength;
  ulong  writtenBlockBytes;
  ulong  deflatedBytes;
  bool   allowNewPartFlag;

  assert(archiveFileInfo != NULL);
  assert(archiveFileInfo->archiveInfo != NULL);

  p            = (byte*)buffer;
  writtenBytes = 0L;
  while (writtenBytes < length)
  {
    /* get size of data block which must be written without splitting
       (make sure written data is aligned at element size boundaries)
    */
    if ((elementSize > 1) && (length > elementSize))
    {
      blockLength = elementSize;
    }
    else
    {
      blockLength = length;
    }

    /* write data block */
    writtenBlockBytes = 0L;
    switch (archiveFileInfo->archiveEntryType)
    {
      case ARCHIVE_ENTRY_TYPE_FILE:
        assert((archiveFileInfo->file.bufferLength%archiveFileInfo->blockLength) == 0);

        while (writtenBlockBytes < blockLength)
        {
          /* compress */
          error = Compress_deflate(&archiveFileInfo->file.compressInfoData,
                                   p+writtenBlockBytes,
                                   length-writtenBlockBytes,
                                   &deflatedBytes
                                  );
          if (error != ERROR_NONE)
          {
            return error;
          }
          writtenBlockBytes += deflatedBytes;

          /* check if compressed data blocks are available and can be encrypted and written to file */
          allowNewPartFlag = ((elementSize <= 1) || (writtenBlockBytes >= blockLength));
          while (Compress_getAvailableBlocks(&archiveFileInfo->file.compressInfoData,
                                             COMPRESS_BLOCK_TYPE_FULL
                                            ) > 0
                )
          {
            error = writeFileDataBlock(archiveFileInfo,BLOCK_MODE_WRITE,allowNewPartFlag);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
          assert(Compress_getAvailableBlocks(&archiveFileInfo->file.compressInfoData,COMPRESS_BLOCK_TYPE_FULL) == 0);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        assert((archiveFileInfo->image.bufferLength%archiveFileInfo->blockLength) == 0);

        while (writtenBlockBytes < blockLength)
        {
          /* compress */
          error = Compress_deflate(&archiveFileInfo->image.compressInfoData,
                                   p+writtenBlockBytes,
                                   blockLength-writtenBlockBytes,
                                   &deflatedBytes
                                  );
          if (error != ERROR_NONE)
          {
            return error;
          }
          writtenBlockBytes += deflatedBytes;

          /* check if compressed data blocks are available and can be encrypted and written to file */
          allowNewPartFlag = ((elementSize <= 1) || (writtenBlockBytes >= blockLength));
          while (Compress_getAvailableBlocks(&archiveFileInfo->image.compressInfoData,
                                             COMPRESS_BLOCK_TYPE_FULL
                                            ) > 0
                )
          {
            error = writeImageDataBlock(archiveFileInfo,BLOCK_MODE_WRITE,allowNewPartFlag);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
          assert(Compress_getAvailableBlocks(&archiveFileInfo->image.compressInfoData,COMPRESS_BLOCK_TYPE_FULL) == 0);
        }
        break;
      default:
        HALT_INTERNAL_ERROR("write data not supported for entry type");
        break;
    }

    /* next data */
    p            += blockLength;
    writtenBytes += blockLength;
  }

  archiveFileInfo->file.bufferLength = 0;

  return ERROR_NONE;
}

Errors Archive_readData(ArchiveFileInfo *archiveFileInfo,
                        void            *buffer,
                        ulong           length
                       )
{
  byte   *p;
  Errors error;
  ulong  n;
  ulong  inflatedBytes;

  assert(archiveFileInfo != NULL);

  switch (archiveFileInfo->archiveEntryType)
  {
    case ARCHIVE_ENTRY_TYPE_FILE:
      p = (byte*)buffer;
      while (length > 0)
      {
        /* fill decompressor with compressed data blocks */
        do
        {
          n = Compress_getAvailableBytes(&archiveFileInfo->file.compressInfoData);
          if (n <= 0)
          {
            error = readFileDataBlock(archiveFileInfo);
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
      break;
    case ARCHIVE_ENTRY_TYPE_IMAGE:
      p = (byte*)buffer;
      while (length > 0)
      {
        /* fill decompressor with compressed data blocks */
        do
        {
          n = Compress_getAvailableBytes(&archiveFileInfo->image.compressInfoData);
          if (n <= 0)
          {
            error = readImageDataBlock(archiveFileInfo);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
        }
        while (n <= 0);

        /* decompress data */
        error = Compress_inflate(&archiveFileInfo->image.compressInfoData,p,1,&inflatedBytes);
        if (error != ERROR_NONE)
        {
          return error;
        }
        p += inflatedBytes;
        length -= inflatedBytes;
      }
      break;
    default:
      HALT_INTERNAL_ERROR("write data not supported for entry type");
      break;
  }

  return ERROR_NONE;
}

uint64 Archive_tell(ArchiveInfo *archiveInfo)
{
  uint64 index;

  assert(archiveInfo != NULL);

  index = 0LL;
  switch (archiveInfo->ioType)
  {
    case ARCHIVE_IO_TYPE_FILE:
      if (archiveInfo->file.openFlag)
      {
        archiveInfo->chunkIO->tell(archiveInfo->chunkIOUserData,&index);
      }
      else
      {
        index = 0LL;
      }
      break;
    case ARCHIVE_IO_TYPE_STORAGE_FILE:
      archiveInfo->chunkIO->tell(archiveInfo->chunkIOUserData,&index);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        return 0LL; /* not reached */
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return index;
}

uint64 Archive_getSize(ArchiveInfo *archiveInfo)
{
  uint64 size;

  assert(archiveInfo != NULL);

  size = 0LL;
  switch (archiveInfo->ioType)
  {
    case ARCHIVE_IO_TYPE_FILE:
      size = (archiveInfo->file.openFlag)
               ?archiveInfo->chunkIO->getSize(archiveInfo->chunkIOUserData)
               :0LL;
      break;
    case ARCHIVE_IO_TYPE_STORAGE_FILE:
      size = archiveInfo->chunkIO->getSize(archiveInfo->chunkIOUserData);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        return 0LL; /* not reached */
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return size;
}

Errors Archive_addIndex(DatabaseHandle *databaseHandle,
                        const String   storageName,
                        Password       *cryptPassword,
                        String         cryptPrivateKeyFileName
                       )
{
  Errors error;
  int64  storageId;

  assert(databaseHandle != NULL);
  assert(storageName != NULL);

  /* create new index */
  error = Index_create(databaseHandle,
                       storageName,
                       INDEX_STATE_UPDATE,
                       &storageId
                      );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* add index */
  error = Archive_updateIndex(databaseHandle,
                              storageId,
                              storageName,
                              cryptPassword,
                              cryptPrivateKeyFileName,
                              NULL,
                              NULL
                             );
  if (error != ERROR_NONE)
  {
    Archive_remIndex(databaseHandle,storageId);
    return error;
  }

  return ERROR_NONE;
}

Errors Archive_updateIndex(DatabaseHandle *databaseHandle,
                           int64          storageId,
                           const String   storageName,
                           Password       *cryptPassword,
                           String         cryptPrivateKeyFileName,
                           bool           *pauseFlag,
                           bool           *requestedAbortFlag                           
                          )
{
  Errors            error;
  JobOptions        jobOptions;
  ArchiveInfo       archiveInfo;
  ArchiveFileInfo   archiveFileInfo;
  ArchiveEntryTypes archiveEntryType;
uint xxx=0;

  assert(databaseHandle != NULL);
  assert(storageName != NULL);
fprintf(stderr,"%s,%d: start index %s\n",__FILE__,__LINE__,String_cString(storageName));

  /* init job options */
  initJobOptions(&jobOptions);
  jobOptions.cryptPassword           = Password_duplicate(cryptPassword);
  jobOptions.cryptPrivateKeyFileName = String_duplicate(cryptPrivateKeyFileName);

  /* open archive */
  error = Archive_open(&archiveInfo,
                       storageName,
                       &jobOptions,
                       NULL,
                       NULL
                      );
  if (error != ERROR_NONE)
  {
    Index_setState(databaseHandle,storageId,INDEX_STATE_ERROR,"%s (error code: %d)",Errors_getText(error),Errors_getCode(error));
    freeJobOptions(&jobOptions);
fprintf(stderr,"%s,%d: index error %d\n",__FILE__,__LINE__,String_cString(storageName),error);
    return error;
  }

  /* clear index */
  error = Index_clear(databaseHandle,
                      storageId
                     );
  if (error != ERROR_NONE)
  {
    Archive_close(&archiveInfo);
    Index_setState(databaseHandle,storageId,INDEX_STATE_ERROR,"%s (error code: %d)",Errors_getText(error),Errors_getCode(error));
    freeJobOptions(&jobOptions);
fprintf(stderr,"%s,%d: index error %d\n",__FILE__,__LINE__,String_cString(storageName),error);
    return error;
  }

  /* index archive contents */
  Index_setState(databaseHandle,
                 storageId,
                 INDEX_STATE_UPDATE,
                 NULL
                );
  error = ERROR_NONE;
  while (   !Archive_eof(&archiveInfo)
         && ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
         && (error == ERROR_NONE)
        )
  {
if ((xxx % 100) == 0)
{
fprintf(stderr,"%s,%d: index count %d: %s\n",__FILE__,__LINE__,xxx,String_cString(storageName));
}
xxx++;

    /* pause */
    while ((pauseFlag != NULL) && (*pauseFlag))
    {
      Misc_udelay(5000*1000);
    }

    /* get next file type */
    error = Archive_getNextArchiveEntryType(&archiveInfo,
                                            &archiveEntryType
                                           );
    if (error == ERROR_NONE)
    {
      /* read entry */
      switch (archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          {
            String             name;
            ArchiveFileInfo    archiveFileInfo;
            CompressAlgorithms compressAlgorithm;
            CryptAlgorithms    cryptAlgorithm;
            CryptTypes         cryptType;
            FileInfo           fileInfo;
            uint64             fragmentOffset,fragmentSize;

            /* open archive file */
            name = String_new();
            error = Archive_readFileEntry(&archiveInfo,
                                          &archiveFileInfo,
                                          &compressAlgorithm,
                                          &cryptAlgorithm,
                                          &cryptType,
                                          name,
                                          &fileInfo,
                                          &fragmentOffset,
                                          &fragmentSize
                                         );
            if (error != ERROR_NONE)
            {
fprintf(stderr,"%s,%d: %d %s\n",__FILE__,__LINE__,error,Errors_getText(error));
              String_delete(name);
              break;
            }
//fprintf(stderr,"%s,%d: index update %s\n",__FILE__,__LINE__,String_cString(name));

            /* add to index database */
            error = Index_addFile(databaseHandle,
                                  storageId,
                                  name,
                                  fileInfo.size,
                                  fileInfo.timeLastAccess,
                                  fileInfo.timeModified,
                                  fileInfo.timeLastChanged,
                                  fileInfo.userId,
                                  fileInfo.groupId,
                                  fileInfo.permission,
                                  fragmentOffset,
                                  fragmentSize
                                 );
            if (error != ERROR_NONE)
            {
fprintf(stderr,"%s,%d: %d %s\n",__FILE__,__LINE__,error,Errors_getText(error));
              Archive_closeEntry(&archiveFileInfo);
              String_delete(name);
              break;
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(name);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            String             name;
            ArchiveFileInfo    archiveFileInfo;
            CompressAlgorithms compressAlgorithm;
            CryptAlgorithms    cryptAlgorithm;
            CryptTypes         cryptType;
            DeviceInfo         deviceInfo;
            uint64             blockOffset,blockCount;

            /* open archive file */
            name = String_new();
            error = Archive_readImageEntry(&archiveInfo,
                                           &archiveFileInfo,
                                           &compressAlgorithm,
                                           &cryptAlgorithm,
                                           &cryptType,
                                           name,
                                           &deviceInfo,
                                           &blockOffset,
                                           &blockCount
                                          );
            if (error != ERROR_NONE)
            {
fprintf(stderr,"%s,%d: %d %s\n",__FILE__,__LINE__,error,Errors_getText(error));
              String_delete(name);
              break;
            }

            /* add to index database */
            error = Index_addImage(databaseHandle,
                                   storageId,
                                   name,
                                   deviceInfo.size,
                                   deviceInfo.blockSize,
                                   blockOffset,
                                   blockCount
                                  );
            if (error != ERROR_NONE)
            {
fprintf(stderr,"%s,%d: %d %s\n",__FILE__,__LINE__,error,Errors_getText(error));
              Archive_closeEntry(&archiveFileInfo);
              String_delete(name);
              break;
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(name);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          {
            String          name;
            CryptAlgorithms cryptAlgorithm;
            CryptTypes      cryptType;
            FileInfo        fileInfo;

            /* open archive directory */
            name = String_new();
            error = Archive_readDirectoryEntry(&archiveInfo,
                                               &archiveFileInfo,
                                               &cryptAlgorithm,
                                               &cryptType,
                                               name,
                                               &fileInfo
                                              );
            if (error != ERROR_NONE)
            {
fprintf(stderr,"%s,%d: %d %s\n",__FILE__,__LINE__,error,Errors_getText(error));
              String_delete(name);
              break;
            }

            /* add to index database */
            error = Index_addDirectory(databaseHandle,
                                       storageId,
                                       name,
                                       fileInfo.timeLastAccess,
                                       fileInfo.timeModified,
                                       fileInfo.timeLastChanged,
                                       fileInfo.userId,
                                       fileInfo.groupId,
                                       fileInfo.permission
                                      );
            if (error != ERROR_NONE)
            {
fprintf(stderr,"%s,%d: %d %s\n",__FILE__,__LINE__,error,Errors_getText(error));
              Archive_closeEntry(&archiveFileInfo);
              String_delete(name);
              break;
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(name);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          {
            String          name;
            String          destinationName;
            CryptAlgorithms cryptAlgorithm;
            CryptTypes      cryptType;
            FileInfo        fileInfo;

            /* open archive link */
            name            = String_new();
            destinationName = String_new();
            error = Archive_readLinkEntry(&archiveInfo,
                                          &archiveFileInfo,
                                          &cryptAlgorithm,
                                          &cryptType,
                                          name,
                                          destinationName,
                                          &fileInfo
                                         );
            if (error != ERROR_NONE)
            {
fprintf(stderr,"%s,%d: %d %s\n",__FILE__,__LINE__,error,Errors_getText(error));
              String_delete(destinationName);
              String_delete(name);
              break;
            }

            /* add to index database */
            error = Index_addLink(databaseHandle,
                                  storageId,
                                  name,
                                  destinationName,
                                  fileInfo.timeLastAccess,
                                  fileInfo.timeModified,
                                  fileInfo.timeLastChanged,
                                  fileInfo.userId,
                                  fileInfo.groupId,
                                  fileInfo.permission
                                 );
            if (error != ERROR_NONE)
            {
fprintf(stderr,"%s,%d: %d %s\n",__FILE__,__LINE__,error,Errors_getText(error));
              Archive_closeEntry(&archiveFileInfo);
              String_delete(destinationName);
              String_delete(name);
              break;
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(destinationName);
            String_delete(name);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          {
            String          name;
            CryptAlgorithms cryptAlgorithm;
            CryptTypes      cryptType;
            FileInfo        fileInfo;

            /* open archive link */
            name = String_new();
            error = Archive_readSpecialEntry(&archiveInfo,
                                             &archiveFileInfo,
                                             &cryptAlgorithm,
                                             &cryptType,
                                             name,
                                             &fileInfo
                                            );
            if (error != ERROR_NONE)
            {
fprintf(stderr,"%s,%d: %d %s\n",__FILE__,__LINE__,error,Errors_getText(error));
              String_delete(name);
              break;
            }

            /* add to index database */
            error = Index_addSpecial(databaseHandle,
                                     storageId,
                                     name,
                                     fileInfo.type,
                                     fileInfo.timeLastAccess,
                                     fileInfo.timeModified,
                                     fileInfo.timeLastChanged,
                                     fileInfo.userId,
                                     fileInfo.groupId,
                                     fileInfo.permission,
                                     fileInfo.major,
                                     fileInfo.minor
                                    );
            if (error != ERROR_NONE)
            {
fprintf(stderr,"%s,%d: %d %s\n",__FILE__,__LINE__,error,Errors_getText(error));
              Archive_closeEntry(&archiveFileInfo);
              String_delete(name);
              break;
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(name);
          }
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break; /* not reached */
      }

      /* update temporary size (ignore error) */
      Index_update(databaseHandle,
                   storageId,
                   NULL,
                   Archive_tell(&archiveInfo)
                  );
    }
  }
  if (error == ERROR_NONE)
  {
    Index_setState(databaseHandle,
                   storageId,
                   INDEX_STATE_OK,
                   NULL
                  );
  }
  else
  {
    Index_setState(databaseHandle,
                   storageId,
                   INDEX_STATE_ERROR,
                   "%s (error code: %d)",
                   Errors_getText(error),
                   Errors_getCode(error)
                  );
  }
fprintf(stderr,"%s,%d: done index %s %d %d\n",__FILE__,__LINE__,String_cString(storageName),error,xxx);

  /* update name/size */
  error = Index_update(databaseHandle,
                       storageId,
                       storageName,
                       Archive_getSize(&archiveInfo)
                      );
  if (error != ERROR_NONE)
  {
    Archive_close(&archiveInfo);
    Index_setState(databaseHandle,storageId,INDEX_STATE_ERROR,"%s (error code: %d)",Errors_getText(error),Errors_getCode(error));
    freeJobOptions(&jobOptions);
fprintf(stderr,"%s,%d: index error %d\n",__FILE__,__LINE__,String_cString(storageName),error);
    return error;
  }

  /* close archive */
  Archive_close(&archiveInfo);

  /* free resources */
  freeJobOptions(&jobOptions);

  return ERROR_NONE;
}

Errors Archive_remIndex(DatabaseHandle *databaseHandle,
                        int64          storageId
                       )
{
  Errors error;

  assert(databaseHandle != NULL);

  error = Index_delete(databaseHandle,storageId);
  if (error != ERROR_NONE)
  {
    return error;
  }
  
  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
