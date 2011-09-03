/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/archive.c,v $
* $Revision: 1.22 $
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
#include "devices.h"

#include "errors.h"
#include "chunks.h"
#include "compress.h"
#include "passwords.h"
#include "crypt.h"
#include "archive_format.h"
#include "storage.h"
#include "index.h"

#include "archive.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/* size of buffer for reading/writing file data */
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

/* block modes */
typedef enum
{
  BLOCK_MODE_WRITE,            // write block
  BLOCK_MODE_FLUSH,            // flush compress data buffers and read/write block
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
  String                          fileName;                          // name of archive for which password is needed
  PasswordModes                   passwordMode;                      // password input mode
  const Password                  *cryptPassword;                    // crypt password
  const PasswordNode              *passwordNode;
  ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction;
  void                            *archiveGetCryptPasswordUserData;
  bool                            inputFlag;                         // TRUE if input dialog was called
} PasswordHandle;

/***************************** Variables *******************************/

/* list with all known decryption passwords */
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
  const Password *password;
  Password       newPassword;
  Errors         error;

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
      Password_init(&newPassword);
      error = passwordHandle->archiveGetCryptPasswordFunction(passwordHandle->archiveGetCryptPasswordUserData,
                                                              &newPassword,
                                                              passwordHandle->fileName,
                                                              FALSE,
                                                              FALSE
                                                             );
      if (error != ERROR_NONE)
      {
        return NULL;
      }

      /* add to password list */
      password = Archive_appendDecryptPassword(&newPassword);

      /* free resources */
      Password_done(&newPassword);

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
                     CHUNK_ID_KEY,
                     CHUNK_DEFINITION_KEY,
                     0,
                     NULL,
                     &chunkKey
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* read key chunk */
  error = Chunk_open(&chunkInfoKey,
                     chunkHeader,
                     Chunk_getSize(CHUNK_DEFINITION_KEY,0,NULL)
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkInfoKey);
    return error;
  }
  archiveInfo->cryptKeyDataLength = chunkHeader->size-Chunk_getSize(CHUNK_DEFINITION_KEY,0,NULL);
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
    free(archiveInfo->cryptKeyData); archiveInfo->cryptKeyData = NULL;
    Chunk_close(&chunkInfoKey);
    Chunk_done(&chunkInfoKey);
    return error;
  }

  /* check if all data read */
  if (!Chunk_eofSub(&chunkInfoKey))
  {
    free(archiveInfo->cryptKeyData); archiveInfo->cryptKeyData = NULL;
    Chunk_close(&chunkInfoKey);
    Chunk_done(&chunkInfoKey);
    return ERROR_CORRUPT_DATA;
  }

  /* close chunk */
  error = Chunk_close(&chunkInfoKey);
  if (error != ERROR_NONE)
  {
    free(archiveInfo->cryptKeyData); archiveInfo->cryptKeyData = NULL;
    Chunk_done(&chunkInfoKey);
    return error;
  }
  Chunk_done(&chunkInfoKey);

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
    free(archiveInfo->cryptKeyData); archiveInfo->cryptKeyData = NULL;
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
                     CHUNK_ID_BAR,
                     CHUNK_DEFINITION_BAR,
                     0,
                     NULL,
                     &chunkBar
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* write header */
  error = Chunk_create(&chunkInfoBar
//                       Chunk_getSize(CHUNK_DEFINITION_BAR,0,&chunkBar),
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
                     CHUNK_ID_KEY,
                     CHUNK_DEFINITION_KEY,
                     0,
                     NULL,
                     &chunkKey
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* write encrypted encryption key */
  error = Chunk_create(&chunkInfoKey
//                       Chunk_getSize(CHUNK_DEFINITION_KEY,0,&chunkKey),
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
  assert(archiveInfo->jobOptions != NULL);
  assert(archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);
  assert(!archiveInfo->file.openFlag);

  /* get output filename */
  error = File_getTmpFileName(archiveInfo->file.fileName,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* create file */
  error = File_open(&archiveInfo->file.fileHandle,
                    archiveInfo->file.fileName,
                    FILE_OPENMODE_CREATE
                   );
  if (error != ERROR_NONE)
  {
    File_delete(archiveInfo->file.fileName,FALSE);
    return error;
  }

  /* write bar header */
  error = writeHeader(archiveInfo);
  if (error != ERROR_NONE)
  {
    File_close(&archiveInfo->file.fileHandle);
    File_delete(archiveInfo->file.fileName,FALSE);
    return error;
  }

  /* write encrypted key if asymmetric encryption enabled */
  if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
  {
    error = writeEncryptionKey(archiveInfo);
    if (error != ERROR_NONE)
    {
      File_close(&archiveInfo->file.fileHandle);
      File_delete(archiveInfo->file.fileName,FALSE);
      return error;
    }
  }

  /* init index */
  if (   (archiveInfo->databaseHandle != NULL)
      && !archiveInfo->jobOptions->dryRunFlag
      && !archiveInfo->jobOptions->noStorageFlag
     )
  {
    error = Index_create(archiveInfo->databaseHandle,
                         NULL,
                         INDEX_STATE_CREATE,
                         INDEX_MODE_MANUAL,
                         &archiveInfo->storageId
                        );
    if (error != ERROR_NONE)
    {
      File_close(&archiveInfo->file.fileHandle);
      File_delete(archiveInfo->file.fileName,FALSE);
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
  assert(archiveInfo->chunkIO != NULL);
  assert(archiveInfo->chunkIO->getSize != NULL);
  assert(archiveInfo->jobOptions != NULL);
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
                                                (!archiveInfo->jobOptions->dryRunFlag && !archiveInfo->jobOptions->noStorageFlag)
                                                  ? archiveInfo->databaseHandle
                                                  : NULL,
                                                archiveInfo->storageId,
                                                archiveInfo->file.fileName,
                                                (archiveInfo->jobOptions->archivePartSize > 0)
                                                  ? archiveInfo->partNumber
                                                  : ARCHIVE_PART_NUMBER_NONE,
                                                lastPartFlag
                                               );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

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
* Input  : archiveEntryInfo  - archive file info block
*          blockMode         - block write mode; see BlockModes
*          allowNewPartFlag - TRUE iff new archive part can be created
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeFileDataBlock(ArchiveEntryInfo *archiveEntryInfo,
                                BlockModes       blockMode,
                                bool             allowNewPartFlag
                               )
{
  ulong  length;
  bool   newPartFlag;
  Errors error;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(archiveEntryInfo->archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);

  /* get compressed block */
  Compress_getBlock(&archiveEntryInfo->file.compressInfoData,
                    archiveEntryInfo->file.buffer,
                    &length
                   );

  /* check if split is allowed and necessary */
  newPartFlag =    allowNewPartFlag
                && checkNewPartNeeded(archiveEntryInfo->archiveInfo,
                                      archiveEntryInfo->file.headerLength,
                                      archiveEntryInfo->file.headerWrittenFlag,
                                      ((length > 0) || (blockMode == BLOCK_MODE_WRITE))?archiveEntryInfo->archiveInfo->blockLength:0
                                     );

  /* split */
  if (newPartFlag)
  {
    /* create chunk-headers */
    if (!archiveEntryInfo->file.headerWrittenFlag && (!archiveEntryInfo->file.createdFlag || (length > 0)))
    {
      error = Chunk_create(&archiveEntryInfo->file.chunkFile.info
//                           Chunk_getSize(CHUNK_DEFINITION_FILE,0,&archiveEntryInfo->file.chunkFile),
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = Chunk_create(&archiveEntryInfo->file.chunkFileEntry.info
//                           Chunk_getSize(CHUNK_DEFINITION_FILE_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->file.chunkFileEntry),
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = Chunk_create(&archiveEntryInfo->file.chunkFileData.info
//                           Chunk_getSize(CHUNK_DEFINITION_FILE_DATA,archiveEntryInfo->blockLength,&archiveEntryInfo->file.chunkFileData),
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      archiveEntryInfo->file.createdFlag       = TRUE;
      archiveEntryInfo->file.headerWrittenFlag = TRUE;
    }

    if (length > 0)
    {
      /* encrypt block */
      error = Crypt_encrypt(&archiveEntryInfo->file.cryptInfoData,
                            archiveEntryInfo->file.buffer,
                            archiveEntryInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      /* write block */
      error = Chunk_writeData(&archiveEntryInfo->file.chunkFileData.info,
                              archiveEntryInfo->file.buffer,
                              archiveEntryInfo->blockLength
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    /* flush compress buffer */
    Compress_flush(&archiveEntryInfo->file.compressInfoData);
    while (Compress_getAvailableBlocks(&archiveEntryInfo->file.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
    {
      /* get compressed block */
      Compress_getBlock(&archiveEntryInfo->file.compressInfoData,
                        archiveEntryInfo->file.buffer,
                        &length
                       );

      /* encrypt block */
      error = Crypt_encrypt(&archiveEntryInfo->file.cryptInfoData,
                            archiveEntryInfo->file.buffer,
                            archiveEntryInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      /* write */
      error = Chunk_writeData(&archiveEntryInfo->file.chunkFileData.info,
                              archiveEntryInfo->file.buffer,
                              archiveEntryInfo->blockLength
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    /* update part size */
    archiveEntryInfo->file.chunkFileData.fragmentSize = Compress_getInputLength(&archiveEntryInfo->file.compressInfoData);
    error = Chunk_update(&archiveEntryInfo->file.chunkFileData.info);
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* close chunks */
    error = Chunk_close(&archiveEntryInfo->file.chunkFileData.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Chunk_close(&archiveEntryInfo->file.chunkFileEntry.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Chunk_close(&archiveEntryInfo->file.chunkFile.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    archiveEntryInfo->file.headerWrittenFlag = FALSE;

    /* close file */
    closeArchiveFile(archiveEntryInfo->archiveInfo,FALSE);

    /* reset compress (do it here because data if buffered and can be processed before a new file is opened) */
    Compress_reset(&archiveEntryInfo->file.compressInfoData);
  }
  else
  {
    if (!archiveEntryInfo->file.createdFlag || (length > 0))
    {
      /* open file if needed */
      if (!archiveEntryInfo->archiveInfo->file.openFlag)
      {
        /* create file */
        error = createArchiveFile(archiveEntryInfo->archiveInfo);
        if (error != ERROR_NONE)
        {
          return error;
        }

        /* initialise variables */
        archiveEntryInfo->file.headerWrittenFlag = FALSE;

        archiveEntryInfo->file.chunkFileData.fragmentOffset = archiveEntryInfo->file.chunkFileData.fragmentOffset+archiveEntryInfo->file.chunkFileData.fragmentSize;
        archiveEntryInfo->file.chunkFileData.fragmentSize   = 0LL;

        /* reset data crypt */
        Crypt_reset(&archiveEntryInfo->file.cryptInfoData,0);
      }

      /* create chunk-headers */
      if (!archiveEntryInfo->file.headerWrittenFlag)
      {
        error = Chunk_create(&archiveEntryInfo->file.chunkFile.info
//                             Chunk_getSize(CHUNK_DEFINITION_FILE,0,&archiveEntryInfo->file.chunkFile),
                            );
        if (error != ERROR_NONE)
        {
          return error;
        }

        error = Chunk_create(&archiveEntryInfo->file.chunkFileEntry.info
//                             Chunk_getSize(CHUNK_DEFINITION_FILE_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->file.chunkFileEntry),
                            );
        if (error != ERROR_NONE)
        {
          return error;
        }

        error = Chunk_create(&archiveEntryInfo->file.chunkFileData.info
//                             Chunk_getSize(CHUNK_DEFINITION_FILE_DATA,archiveEntryInfo->blockLength,&archiveEntryInfo->file.chunkFileData),
                            );
        if (error != ERROR_NONE)
        {
          return error;
        }

        archiveEntryInfo->file.createdFlag       = TRUE;
        archiveEntryInfo->file.headerWrittenFlag = TRUE;
      }

      if (length > 0)
      {
        /* encrypt block */
        error = Crypt_encrypt(&archiveEntryInfo->file.cryptInfoData,
                              archiveEntryInfo->file.buffer,
                              archiveEntryInfo->blockLength
                             );
        if (error != ERROR_NONE)
        {
          return error;
        }

        /* write block */
        error = Chunk_writeData(&archiveEntryInfo->file.chunkFileData.info,
                                archiveEntryInfo->file.buffer,
                                archiveEntryInfo->blockLength
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
* Input  : archiveEntryInfo - archive file info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors readFileDataBlock(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors error;

  assert(archiveEntryInfo != NULL);

  if (!Chunk_eofSub(&archiveEntryInfo->file.chunkFileData.info))
  {
    /* read */
    error = Chunk_readData(&archiveEntryInfo->file.chunkFileData.info,
                           archiveEntryInfo->file.buffer,
                           archiveEntryInfo->blockLength
                          );
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* decrypt */
    error = Crypt_decrypt(&archiveEntryInfo->file.cryptInfoData,
                          archiveEntryInfo->file.buffer,
                          archiveEntryInfo->blockLength
                         );
    if (error != ERROR_NONE)
    {
      return error;
    }

    Compress_putBlock(&archiveEntryInfo->file.compressInfoData,
                      archiveEntryInfo->file.buffer,
                      archiveEntryInfo->blockLength
                     );
  }
  else
  {
    Compress_flush(&archiveEntryInfo->file.compressInfoData);
    if (Compress_getAvailableBlocks(&archiveEntryInfo->file.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) <= 0)
    {
      return ERROR_COMPRESS_EOF;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeImageDataBlock
* Purpose: write image data block, encrypt and split file
* Input  : archiveEntryInfo  - archive file info block
*          blockMode         - block write mode; see BlockModes
*          allowNewPartFlag  - TRUE iff new archive part can be created
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeImageDataBlock(ArchiveEntryInfo *archiveEntryInfo,
                                 BlockModes       blockMode,
                                 bool             allowNewPartFlag
                                )
{
  ulong  length;
  bool   newPartFlag;
  Errors error;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(archiveEntryInfo->archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);

  /* get compressed block */
  Compress_getBlock(&archiveEntryInfo->image.compressInfoData,
                    archiveEntryInfo->image.buffer,
                    &length
                   );

  /* check if split is allowed and necessary */
  newPartFlag =    allowNewPartFlag
                && checkNewPartNeeded(archiveEntryInfo->archiveInfo,
                                      archiveEntryInfo->image.headerLength,
                                      archiveEntryInfo->image.headerWrittenFlag,
                                      ((length > 0) || (blockMode == BLOCK_MODE_WRITE))?archiveEntryInfo->archiveInfo->blockLength:0
                                     );

  /* split */
  if (newPartFlag)
  {
    /* create chunk-headers */
    if (!archiveEntryInfo->image.headerWrittenFlag && (!archiveEntryInfo->image.createdFlag || (length > 0)))
    {
      error = Chunk_create(&archiveEntryInfo->image.chunkImage.info
//                           Chunk_getSize(CHUNK_DEFINITION_IMAGE,0,&archiveEntryInfo->image.chunkImage),
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = Chunk_create(&archiveEntryInfo->image.chunkImageEntry.info
//                           Chunk_getSize(CHUNK_DEFINITION_IMAGE_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->image.chunkImageEntry),
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = Chunk_create(&archiveEntryInfo->image.chunkImageData.info
//                           Chunk_getSize(CHUNK_DEFINITION_IMAGE_DATA,archiveEntryInfo->blockLength,&archiveEntryInfo->image.chunkImageData),
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      archiveEntryInfo->image.createdFlag       = TRUE;
      archiveEntryInfo->image.headerWrittenFlag = TRUE;
    }

    if (length > 0)
    {
      /* encrypt block */
      error = Crypt_encrypt(&archiveEntryInfo->image.cryptInfoData,
                            archiveEntryInfo->image.buffer,
                            archiveEntryInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      /* write block */
      error = Chunk_writeData(&archiveEntryInfo->image.chunkImageData.info,
                              archiveEntryInfo->image.buffer,
                              archiveEntryInfo->blockLength
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    /* flush compress buffer */
    Compress_flush(&archiveEntryInfo->image.compressInfoData);
    while (Compress_getAvailableBlocks(&archiveEntryInfo->image.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
    {
      /* get compressed block */
      Compress_getBlock(&archiveEntryInfo->image.compressInfoData,
                        archiveEntryInfo->image.buffer,
                        &length
                       );

      /* encrypt block */
      error = Crypt_encrypt(&archiveEntryInfo->image.cryptInfoData,
                            archiveEntryInfo->image.buffer,
                            archiveEntryInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      /* write */
      error = Chunk_writeData(&archiveEntryInfo->image.chunkImageData.info,
                              archiveEntryInfo->image.buffer,
                              archiveEntryInfo->blockLength
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    /* update part size */
    assert(archiveEntryInfo->image.blockSize > 0);
    archiveEntryInfo->image.chunkImageData.blockCount = Compress_getInputLength(&archiveEntryInfo->image.compressInfoData)/archiveEntryInfo->image.blockSize;
    error = Chunk_update(&archiveEntryInfo->image.chunkImageData.info);
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* close chunks */
    error = Chunk_close(&archiveEntryInfo->image.chunkImageData.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Chunk_close(&archiveEntryInfo->image.chunkImageEntry.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Chunk_close(&archiveEntryInfo->image.chunkImage.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    archiveEntryInfo->image.headerWrittenFlag = FALSE;

    /* close file */
    closeArchiveFile(archiveEntryInfo->archiveInfo,FALSE);

    /* reset compress (do it here because data if buffered and can be processed before a new file is opened) */
    Compress_reset(&archiveEntryInfo->image.compressInfoData);
  }
  else
  {
    if (!archiveEntryInfo->image.createdFlag || (length > 0))
    {
      /* open file if needed */
      if (!archiveEntryInfo->archiveInfo->file.openFlag)
      {
        /* create file */
        error = createArchiveFile(archiveEntryInfo->archiveInfo);
        if (error != ERROR_NONE)
        {
          return error;
        }

        /* initialise variables */
        archiveEntryInfo->image.headerWrittenFlag = FALSE;

        archiveEntryInfo->image.chunkImageData.blockOffset = archiveEntryInfo->image.chunkImageData.blockOffset+archiveEntryInfo->image.chunkImageData.blockCount;
        archiveEntryInfo->image.chunkImageData.blockCount  = 0;

        /* reset data crypt */
        Crypt_reset(&archiveEntryInfo->image.cryptInfoData,0);
      }

      /* create chunk-headers */
      if (!archiveEntryInfo->image.headerWrittenFlag)
      {
        error = Chunk_create(&archiveEntryInfo->image.chunkImage.info
//                             Chunk_getSize(CHUNK_DEFINITION_IMAGE,0,&archiveEntryInfo->image.chunkImage),
                            );
        if (error != ERROR_NONE)
        {
          return error;
        }

        error = Chunk_create(&archiveEntryInfo->image.chunkImageEntry.info
//                             Chunk_getSize(CHUNK_DEFINITION_IMAGE_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->image.chunkImageEntry),
                            );
        if (error != ERROR_NONE)
        {
          return error;
        }

        error = Chunk_create(&archiveEntryInfo->image.chunkImageData.info
//                             Chunk_getSize(CHUNK_DEFINITION_IMAGE_DATA,archiveEntryInfo->blockLength,&archiveEntryInfo->image.chunkImageData),
                            );
        if (error != ERROR_NONE)
        {
          return error;
        }

        archiveEntryInfo->image.createdFlag       = TRUE;
        archiveEntryInfo->image.headerWrittenFlag = TRUE;
      }

      if (length > 0)
      {
        /* encrypt block */
        error = Crypt_encrypt(&archiveEntryInfo->image.cryptInfoData,
                              archiveEntryInfo->image.buffer,
                              archiveEntryInfo->blockLength
                             );
        if (error != ERROR_NONE)
        {
          return error;
        }

        /* write block */
        error = Chunk_writeData(&archiveEntryInfo->image.chunkImageData.info,
                                archiveEntryInfo->image.buffer,
                                archiveEntryInfo->blockLength
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
* Input  : archiveEntryInfo - archive file info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors readImageDataBlock(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors error;

  assert(archiveEntryInfo != NULL);

  if (!Chunk_eofSub(&archiveEntryInfo->image.chunkImageData.info))
  {
    /* read */
    error = Chunk_readData(&archiveEntryInfo->image.chunkImageData.info,
                           archiveEntryInfo->image.buffer,
                           archiveEntryInfo->blockLength
                          );
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* decrypt */
    error = Crypt_decrypt(&archiveEntryInfo->image.cryptInfoData,
                          archiveEntryInfo->image.buffer,
                          archiveEntryInfo->blockLength
                         );
    if (error != ERROR_NONE)
    {
      return error;
    }

    Compress_putBlock(&archiveEntryInfo->image.compressInfoData,
                      archiveEntryInfo->image.buffer,
                      archiveEntryInfo->blockLength
                     );
  }
  else
  {
    Compress_flush(&archiveEntryInfo->image.compressInfoData);
    if (Compress_getAvailableBlocks(&archiveEntryInfo->image.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) <= 0)
    {
      return ERROR_COMPRESS_EOF;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeHardLinkDataBlock
* Purpose: write hard link data block, encrypt and split file
* Input  : archiveEntryInfo  - archive file info block
*          blockMode         - block write mode; see BlockModes
*          allowNewPartFlag  - TRUE iff new archive part can be created
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeHardLinkDataBlock(ArchiveEntryInfo *archiveEntryInfo,
                                    BlockModes       blockMode,
                                    bool             allowNewPartFlag
                                   )
{
  ulong             length;
  bool              newPartFlag;
  Errors            error;
  ChunkHardLinkName *chunkHardLinkName;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(archiveEntryInfo->archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);

  /* get compressed block */
  Compress_getBlock(&archiveEntryInfo->hardLink.compressInfoData,
                    archiveEntryInfo->hardLink.buffer,
                    &length
                   );

  /* check if split is allowed and necessary */
  newPartFlag =    allowNewPartFlag
                && checkNewPartNeeded(archiveEntryInfo->archiveInfo,
                                      archiveEntryInfo->hardLink.headerLength,
                                      archiveEntryInfo->hardLink.headerWrittenFlag,
                                      ((length > 0) || (blockMode == BLOCK_MODE_WRITE))?archiveEntryInfo->archiveInfo->blockLength:0
                                     );

  /* split */
  if (newPartFlag)
  {
    /* create chunk-headers */
    if (!archiveEntryInfo->hardLink.headerWrittenFlag && (!archiveEntryInfo->hardLink.createdFlag || (length > 0)))
    {
      error = Chunk_create(&archiveEntryInfo->hardLink.chunkHardLink.info
//                           Chunk_getSize(CHUNK_DEFINITION_HARDLINK,0,&archiveEntryInfo->hardLink.chunkHardLink),
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = Chunk_create(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info
//                           Chunk_getSize(CHUNK_DEFINITION_HARDLINK_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->hardLink.chunkHardLinkEntry),
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      LIST_ITERATE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
      {
        error = Chunk_create(&chunkHardLinkName->info);
        if (error != ERROR_NONE)
        {
          return error;
        }
      }

      error = Chunk_create(&archiveEntryInfo->hardLink.chunkHardLinkData.info
//                           Chunk_getSize(CHUNK_DEFINITION_HARDLINK_DATA,archiveEntryInfo->blockLength,&archiveEntryInfo->hardLink.chunkHardLinkData),
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }

      archiveEntryInfo->hardLink.createdFlag       = TRUE;
      archiveEntryInfo->hardLink.headerWrittenFlag = TRUE;
    }

    if (length > 0)
    {
      /* encrypt block */
      error = Crypt_encrypt(&archiveEntryInfo->hardLink.cryptInfoData,
                            archiveEntryInfo->hardLink.buffer,
                            archiveEntryInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      /* write block */
      error = Chunk_writeData(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                              archiveEntryInfo->hardLink.buffer,
                              archiveEntryInfo->blockLength
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    /* flush compress buffer */
    Compress_flush(&archiveEntryInfo->hardLink.compressInfoData);
    while (Compress_getAvailableBlocks(&archiveEntryInfo->hardLink.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
    {
      /* get compressed block */
      Compress_getBlock(&archiveEntryInfo->hardLink.compressInfoData,
                        archiveEntryInfo->hardLink.buffer,
                        &length
                       );

      /* encrypt block */
      error = Crypt_encrypt(&archiveEntryInfo->hardLink.cryptInfoData,
                            archiveEntryInfo->hardLink.buffer,
                            archiveEntryInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      /* write */
      error = Chunk_writeData(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                              archiveEntryInfo->hardLink.buffer,
                              archiveEntryInfo->blockLength
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    /* update part size */
    archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize = Compress_getInputLength(&archiveEntryInfo->hardLink.compressInfoData);
    error = Chunk_update(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* close chunks */
    error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    LIST_ITERATE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
    {
      error = Chunk_close(&chunkHardLinkName->info);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
    error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLink.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    archiveEntryInfo->hardLink.headerWrittenFlag = FALSE;

    /* close file */
    closeArchiveFile(archiveEntryInfo->archiveInfo,FALSE);

    /* reset compress (do it here because data if buffered and can be processed before a new file is opened) */
    Compress_reset(&archiveEntryInfo->hardLink.compressInfoData);
  }
  else
  {
    if (!archiveEntryInfo->hardLink.createdFlag || (length > 0))
    {
      /* open file if needed */
      if (!archiveEntryInfo->archiveInfo->file.openFlag)
      {
        /* create file */
        error = createArchiveFile(archiveEntryInfo->archiveInfo);
        if (error != ERROR_NONE)
        {
          return error;
        }

        /* initialise variables */
        archiveEntryInfo->hardLink.headerWrittenFlag = FALSE;

        archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset = archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset+archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize;
        archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize   = 0LL;

        /* reset data crypt */
        Crypt_reset(&archiveEntryInfo->hardLink.cryptInfoData,0);
      }

      /* create chunk-headers */
      if (!archiveEntryInfo->hardLink.headerWrittenFlag)
      {
        error = Chunk_create(&archiveEntryInfo->hardLink.chunkHardLink.info
//                             Chunk_getSize(CHUNK_DEFINITION_HARDLINK,0,&archiveEntryInfo->hardLink.chunkHardLink),
                            );
        if (error != ERROR_NONE)
        {
          return error;
        }

        error = Chunk_create(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info
//                             Chunk_getSize(CHUNK_DEFINITION_HARDLINK_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->hardLink.chunkHardLinkEntry),
                            );
        if (error != ERROR_NONE)
        {
          return error;
        }

        LIST_ITERATE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
        {
          error = Chunk_create(&chunkHardLinkName->info);
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        error = Chunk_create(&archiveEntryInfo->hardLink.chunkHardLinkData.info
//                             Chunk_getSize(CHUNK_DEFINITION_HARDLINK_DATA,archiveEntryInfo->blockLength,&archiveEntryInfo->hardLink.chunkHardLinkData),
                            );
        if (error != ERROR_NONE)
        {
          return error;
        }

        archiveEntryInfo->hardLink.createdFlag       = TRUE;
        archiveEntryInfo->hardLink.headerWrittenFlag = TRUE;
      }

      if (length > 0)
      {
        /* encrypt block */
        error = Crypt_encrypt(&archiveEntryInfo->hardLink.cryptInfoData,
                              archiveEntryInfo->hardLink.buffer,
                              archiveEntryInfo->blockLength
                             );
        if (error != ERROR_NONE)
        {
          return error;
        }

        /* write block */
        error = Chunk_writeData(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                                archiveEntryInfo->hardLink.buffer,
                                archiveEntryInfo->blockLength
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
* Name   : readHardLinkDataBlock
* Purpose: read hard link data block, decrypt
* Input  : archiveEntryInfo - archive file info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors readHardLinkDataBlock(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors error;

  assert(archiveEntryInfo != NULL);

  if (!Chunk_eofSub(&archiveEntryInfo->hardLink.chunkHardLinkData.info))
  {
    /* read */
    error = Chunk_readData(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                           archiveEntryInfo->hardLink.buffer,
                           archiveEntryInfo->blockLength
                          );
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* decrypt */
    error = Crypt_decrypt(&archiveEntryInfo->hardLink.cryptInfoData,
                          archiveEntryInfo->hardLink.buffer,
                          archiveEntryInfo->blockLength
                         );
    if (error != ERROR_NONE)
    {
      return error;
    }

    Compress_putBlock(&archiveEntryInfo->hardLink.compressInfoData,
                      archiveEntryInfo->hardLink.buffer,
                      archiveEntryInfo->blockLength
                     );
  }
  else
  {
    Compress_flush(&archiveEntryInfo->hardLink.compressInfoData);
    if (Compress_getAvailableBlocks(&archiveEntryInfo->hardLink.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) <= 0)
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

const Password *Archive_appendDecryptPassword(const Password *password)
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

  return passwordNode->password;
}

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
  archiveInfo->file.fileName                   = String_new();
  archiveInfo->file.openFlag                   = FALSE;
  archiveInfo->printableName                   = String_new();

  archiveInfo->databaseHandle                  = databaseHandle;
  archiveInfo->storageId                       = DATABASE_ID_NONE;

  archiveInfo->chunkIO                         = &CHUNK_IO_FILE;
  archiveInfo->chunkIOUserData                 = &archiveInfo->file.fileHandle;

  archiveInfo->partNumber                      = 0;

  archiveInfo->pendingError                    = ERROR_NONE;
  archiveInfo->nextChunkHeaderReadFlag         = FALSE;

  /* init key (if asymmetric encryption used) */
  if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
  {
    /* check if public key available */
    if (jobOptions->cryptPublicKeyFileName == NULL)
    {
      String_delete(archiveInfo->file.fileName);
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
      String_delete(archiveInfo->file.fileName);
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
        String_delete(archiveInfo->file.fileName);
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
  archiveInfo->storage.storageName             = String_duplicate(storageName);
  archiveInfo->printableName                   = Storage_getPrintableName(String_new(),storageName);

  archiveInfo->databaseHandle                  = NULL;
  archiveInfo->storageId                       = DATABASE_ID_NONE;

  archiveInfo->chunkIO                         = &CHUNK_IO_STORAGE_FILE;
  archiveInfo->chunkIOUserData                 = &archiveInfo->storage.storageFileHandle;

  archiveInfo->partNumber                      = 0;

  archiveInfo->pendingError                    = ERROR_NONE;
  archiveInfo->nextChunkHeaderReadFlag         = FALSE;

  error = Storage_init(&archiveInfo->storage.storageFileHandle,
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
    String_delete(archiveInfo->printableName);
    String_delete(archiveInfo->storage.storageName);
    String_delete(fileName);
    return error;
  }

  /* open storage file */
  error = Storage_open(&archiveInfo->storage.storageFileHandle,
                       fileName
                      );
  if (error != ERROR_NONE)
  {
    Storage_done(&archiveInfo->storage.storageFileHandle);
    String_delete(archiveInfo->printableName);
    String_delete(archiveInfo->storage.storageName);
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
      Storage_close(&archiveInfo->storage.storageFileHandle);
      Storage_done(&archiveInfo->storage.storageFileHandle);
      break;
  }

  if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
  {
    assert(archiveInfo->cryptKeyData != NULL);

    free(archiveInfo->cryptKeyData);
    Crypt_doneKey(&archiveInfo->cryptKey);
  }

  if (archiveInfo->cryptPassword  != NULL) Password_delete(archiveInfo->cryptPassword);
  if (archiveInfo->printableName != NULL) String_delete(archiveInfo->printableName);
  switch (archiveInfo->ioType)
  {
    case ARCHIVE_IO_TYPE_FILE:
      if (archiveInfo->file.fileName != NULL) String_delete(archiveInfo->file.fileName);
      break;
    case ARCHIVE_IO_TYPE_STORAGE_FILE:
      if (archiveInfo->storage.storageName != NULL) String_delete(archiveInfo->storage.storageName);
      break;
  }

  return ERROR_NONE;
}

bool Archive_eof(ArchiveInfo *archiveInfo,
                 bool        skipUnknownChunksFlag
                )
{
  bool           chunkHeaderFoundFlag;
  ChunkHeader    chunkHeader;
  bool           decryptedFlag;
  PasswordHandle passwordHandle;
  const Password *password;

  assert(archiveInfo != NULL);

  /* check for pending error */
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    return FALSE;
  }

  /* find next file, image, directory, link, special chunk */
  chunkHeaderFoundFlag = FALSE;
  while (!Chunk_eof(archiveInfo->chunkIO,archiveInfo->chunkIOUserData) && !chunkHeaderFoundFlag)
  {
    /* get next chunk header */
    archiveInfo->pendingError = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (archiveInfo->pendingError != ERROR_NONE)
    {
      return FALSE;
    }

    /* find next file, image, directory, link, special chunk */
    switch (chunkHeader.id)
    {
      case CHUNK_ID_BAR:
        /* bar header is simply ignored */
        archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
        if (archiveInfo->pendingError != ERROR_NONE)
        {
          return FALSE;
        }
        break;
      case CHUNK_ID_KEY:
        /* check if private key available */
        if (archiveInfo->jobOptions->cryptPrivateKeyFileName == NULL)
        {
          return FALSE;
        }

        /* read private key, try to read key with no password, all passwords */
        Crypt_initKey(&archiveInfo->cryptKey);
        decryptedFlag = FALSE;
        archiveInfo->pendingError = Crypt_readKeyFile(&archiveInfo->cryptKey,
                                                      archiveInfo->jobOptions->cryptPrivateKeyFileName,
                                                      NULL
                                                     );
        if (archiveInfo->pendingError == ERROR_NONE)
        {
          decryptedFlag = TRUE;
        }
        password = getFirstDecryptPassword(&passwordHandle,
                                           archiveInfo->printableName,
                                           archiveInfo->jobOptions,
                                           archiveInfo->jobOptions->cryptPasswordMode,
                                           archiveInfo->archiveGetCryptPasswordFunction,
                                           archiveInfo->archiveGetCryptPasswordUserData
                                          );
        while (   !decryptedFlag
               && (password != NULL)
              )
        {
          archiveInfo->pendingError = Crypt_readKeyFile(&archiveInfo->cryptKey,
                                                        archiveInfo->jobOptions->cryptPrivateKeyFileName,
                                                        password
                                                       );
          if (archiveInfo->pendingError == ERROR_NONE)
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
          return FALSE;
        }

        /* read encryption key */
        archiveInfo->pendingError = readEncryptionKey(archiveInfo,&chunkHeader);
        if (archiveInfo->pendingError != ERROR_NONE)
        {
          return FALSE;
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
      case CHUNK_ID_HARDLINK:
      case CHUNK_ID_SPECIAL:
        chunkHeaderFoundFlag = TRUE;
        break;
      default:
        if (skipUnknownChunksFlag)
        {
          archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
          if (archiveInfo->pendingError != ERROR_NONE)
          {
            return FALSE;
          }
          if (globalOptions.verboseLevel >= 3)
          {
            printWarning("Skipped unexpected chunk '%s' (offset %ld)\n",Chunk_idToString(chunkHeader.id),chunkHeader.offset);
          }
        }
        else
        {
          return FALSE;
        }
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

Errors Archive_newFileEntry(ArchiveInfo                     *archiveInfo,
                            ArchiveEntryInfo                *archiveEntryInfo,
                            const String                    fileName,
                            const FileInfo                  *fileInfo,
                            CompressSourceGetEntryDataBlock sourceGetEntryDataBlock,
                            void                            *sourceGetEntryDataBlockUserData,
                            bool                            compressFlag
                           )
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveEntryInfo != NULL);
  assert(fileInfo != NULL);

  /* init crypt password */
  if ((archiveInfo->jobOptions->cryptAlgorithm != CRYPT_ALGORITHM_NONE) && (archiveInfo->cryptPassword == NULL))
  {
    archiveInfo->cryptPassword = getCryptPassword(archiveInfo->printableName,
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
  archiveEntryInfo->archiveInfo            = archiveInfo;
  archiveEntryInfo->mode                   = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm         = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength            = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType       = ARCHIVE_ENTRY_TYPE_FILE;

  archiveEntryInfo->file.compressAlgorithm = compressFlag?archiveInfo->jobOptions->compressAlgorithm:COMPRESS_ALGORITHM_NONE;

  archiveEntryInfo->file.createdFlag       = FALSE;
  archiveEntryInfo->file.headerLength      = 0;
  archiveEntryInfo->file.headerWrittenFlag = FALSE;

  archiveEntryInfo->file.buffer            = NULL;
  archiveEntryInfo->file.bufferLength      = 0;

  /* init file chunk */
  error = Chunk_init(&archiveEntryInfo->file.chunkFile.info,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_FILE,
                     CHUNK_DEFINITION_FILE,
                     0,
                     NULL,
                     &archiveEntryInfo->file.chunkFile
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }
  archiveEntryInfo->file.chunkFile.compressAlgorithm = archiveEntryInfo->file.compressAlgorithm;
  archiveEntryInfo->file.chunkFile.cryptAlgorithm    = archiveInfo->jobOptions->cryptAlgorithm;

  /* init file entry chunk */
  error = Crypt_init(&archiveEntryInfo->file.chunkFileEntry.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    return error;
  }
  error = Chunk_init(&archiveEntryInfo->file.chunkFileEntry.info,
                     &archiveEntryInfo->file.chunkFile.info,
                     NULL,
                     NULL,
                     CHUNK_ID_FILE_ENTRY,
                     CHUNK_DEFINITION_FILE_ENTRY,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->file.chunkFileEntry.cryptInfo,
                     &archiveEntryInfo->file.chunkFileEntry
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    return error;
  }
  archiveEntryInfo->file.chunkFileEntry.size            = fileInfo->size;
  archiveEntryInfo->file.chunkFileEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveEntryInfo->file.chunkFileEntry.timeModified    = fileInfo->timeModified;
  archiveEntryInfo->file.chunkFileEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveEntryInfo->file.chunkFileEntry.userId          = fileInfo->userId;
  archiveEntryInfo->file.chunkFileEntry.groupId         = fileInfo->groupId;
  archiveEntryInfo->file.chunkFileEntry.permission      = fileInfo->permission;
  String_set(archiveEntryInfo->file.chunkFileEntry.name,fileName);

  /* init file data chunk */
  error = Crypt_init(&archiveEntryInfo->file.chunkFileData.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    return error;
  }
  error = Chunk_init(&archiveEntryInfo->file.chunkFileData.info,
                     &archiveEntryInfo->file.chunkFile.info,
                     NULL,
                     NULL,
                     CHUNK_ID_FILE_DATA,
                     CHUNK_DEFINITION_FILE_DATA,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->file.chunkFileData.cryptInfo,
                     &archiveEntryInfo->file.chunkFileData
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    return error;
  }
  archiveEntryInfo->file.chunkFileData.fragmentOffset = 0LL;
  archiveEntryInfo->file.chunkFileData.fragmentSize   = 0LL;

  /* allocate buffer */
  archiveEntryInfo->file.bufferLength = (MAX_BUFFER_SIZE/archiveEntryInfo->blockLength)*archiveEntryInfo->blockLength;
  archiveEntryInfo->file.buffer = (byte*)malloc(archiveEntryInfo->file.bufferLength);
  if (archiveEntryInfo->file.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init data compress */
  error = Compress_new(&archiveEntryInfo->file.compressInfoData,
                       COMPRESS_MODE_DEFLATE,
                       archiveEntryInfo->file.compressAlgorithm,
                       archiveEntryInfo->blockLength,
                       sourceGetEntryDataBlock,
                       sourceGetEntryDataBlockUserData
                      );
  if (error != ERROR_NONE)
  {
    free(archiveEntryInfo->file.buffer);
    Chunk_done(&archiveEntryInfo->file.chunkFileData.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    return error;
  }

  /* init crypt */
  error = Crypt_init(&archiveEntryInfo->file.cryptInfoData,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Compress_delete(&archiveEntryInfo->file.compressInfoData);
    free(archiveEntryInfo->file.buffer);
    Chunk_done(&archiveEntryInfo->file.chunkFileData.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    return error;
  }

  /* calculate header length */
  archiveEntryInfo->file.headerLength = Chunk_getSize(CHUNK_DEFINITION_FILE,      0,                           &archiveEntryInfo->file.chunkFile     )+
                                       Chunk_getSize(CHUNK_DEFINITION_FILE_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->file.chunkFileEntry)+
                                       Chunk_getSize(CHUNK_DEFINITION_FILE_DATA, archiveEntryInfo->blockLength,&archiveEntryInfo->file.chunkFileData );

  return ERROR_NONE;
}

Errors Archive_newImageEntry(ArchiveInfo      *archiveInfo,
                             ArchiveEntryInfo *archiveEntryInfo,
                             const String     deviceName,
                             const DeviceInfo *deviceInfo,
                            bool              compressFlag
                            )
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveEntryInfo != NULL);
  assert(deviceInfo != NULL);
  assert(deviceInfo->blockSize > 0);

  /* init crypt password */
  if ((archiveInfo->jobOptions->cryptAlgorithm != CRYPT_ALGORITHM_NONE) && (archiveInfo->cryptPassword == NULL))
  {
    archiveInfo->cryptPassword = getCryptPassword(archiveInfo->printableName,
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
  archiveEntryInfo->archiveInfo             = archiveInfo;
  archiveEntryInfo->mode                    = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm          = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength             = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType        = ARCHIVE_ENTRY_TYPE_IMAGE;

  archiveEntryInfo->image.blockSize         = deviceInfo->blockSize;
  archiveEntryInfo->image.compressAlgorithm = compressFlag?archiveInfo->jobOptions->compressAlgorithm:COMPRESS_ALGORITHM_NONE;

  archiveEntryInfo->image.createdFlag       = FALSE;
  archiveEntryInfo->image.headerLength      = 0;
  archiveEntryInfo->image.headerWrittenFlag = FALSE;

  archiveEntryInfo->image.buffer            = NULL;
  archiveEntryInfo->image.bufferLength      = 0;

  /* init image chunk */
  error = Chunk_init(&archiveEntryInfo->image.chunkImage.info,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_IMAGE,
                     CHUNK_DEFINITION_IMAGE,
                     0,
                     NULL,
                     &archiveEntryInfo->image.chunkImage
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }
  archiveEntryInfo->image.chunkImage.compressAlgorithm = archiveEntryInfo->image.compressAlgorithm;
  archiveEntryInfo->image.chunkImage.cryptAlgorithm    = archiveInfo->jobOptions->cryptAlgorithm;

  /* init image entry chunk */
  error = Crypt_init(&archiveEntryInfo->image.chunkImageEntry.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    return error;
  }
  error = Chunk_init(&archiveEntryInfo->image.chunkImageEntry.info,
                     &archiveEntryInfo->image.chunkImage.info,
                     NULL,
                     NULL,
                     CHUNK_ID_IMAGE_ENTRY,
                     CHUNK_DEFINITION_IMAGE_ENTRY,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->image.chunkImageEntry.cryptInfo,
                     &archiveEntryInfo->image.chunkImageEntry
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    return error;
  }
  archiveEntryInfo->image.chunkImageEntry.size      = deviceInfo->size;
  archiveEntryInfo->image.chunkImageEntry.blockSize = deviceInfo->blockSize;
  String_set(archiveEntryInfo->image.chunkImageEntry.name,deviceName);

  /* init image data chunk */
  error = Crypt_init(&archiveEntryInfo->image.chunkImageData.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    return error;
  }
  error = Chunk_init(&archiveEntryInfo->image.chunkImageData.info,
                     &archiveEntryInfo->image.chunkImage.info,
                     NULL,
                     NULL,
                     CHUNK_ID_IMAGE_DATA,
                     CHUNK_DEFINITION_IMAGE_DATA,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->image.chunkImageData.cryptInfo,
                     &archiveEntryInfo->image.chunkImageData
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    return error;
  }
  archiveEntryInfo->image.chunkImageData.blockOffset = 0LL;
  archiveEntryInfo->image.chunkImageData.blockCount  = 0LL;

  /* allocate buffer */
  archiveEntryInfo->image.bufferLength = (MAX_BUFFER_SIZE/archiveEntryInfo->blockLength)*archiveEntryInfo->blockLength;
  archiveEntryInfo->image.buffer = (byte*)malloc(archiveEntryInfo->image.bufferLength);
  if (archiveEntryInfo->image.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init data compress */
  error = Compress_new(&archiveEntryInfo->image.compressInfoData,
                       COMPRESS_MODE_DEFLATE,
                       archiveEntryInfo->image.compressAlgorithm,
                       archiveEntryInfo->blockLength
//??? NYI
,NULL
,NULL
                      );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->image.chunkImageData.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
    free(archiveEntryInfo->image.buffer);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    return error;
  }

  /* init crypt */
  error = Crypt_init(&archiveEntryInfo->image.cryptInfoData,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Compress_delete(&archiveEntryInfo->image.compressInfoData);
    Chunk_done(&archiveEntryInfo->image.chunkImageData.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
    free(archiveEntryInfo->image.buffer);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    return error;
  }

  /* calculate header length */
  archiveEntryInfo->image.headerLength = Chunk_getSize(CHUNK_DEFINITION_IMAGE,      0,                           &archiveEntryInfo->image.chunkImage     )+
                                        Chunk_getSize(CHUNK_DEFINITION_IMAGE_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->image.chunkImageEntry)+
                                        Chunk_getSize(CHUNK_DEFINITION_IMAGE_DATA, archiveEntryInfo->blockLength,&archiveEntryInfo->image.chunkImageData );

  return ERROR_NONE;
}

Errors Archive_newDirectoryEntry(ArchiveInfo      *archiveInfo,
                                 ArchiveEntryInfo *archiveEntryInfo,
                                 const String     directoryName,
                                 const FileInfo   *fileInfo
                                )
{
  Errors error;
  ulong  length;

  assert(archiveInfo != NULL);
  assert(archiveEntryInfo != NULL);
  assert(fileInfo != NULL);

  /* init crypt password */
  if ((archiveInfo->jobOptions->cryptAlgorithm != CRYPT_ALGORITHM_NONE) && (archiveInfo->cryptPassword == NULL))
  {
    archiveInfo->cryptPassword = getCryptPassword(archiveInfo->printableName,
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
  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm   = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength      = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_DIRECTORY;

  /* init directory chunk */
  error = Chunk_init(&archiveEntryInfo->directory.chunkDirectory.info,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_DIRECTORY,
                     CHUNK_DEFINITION_DIRECTORY,
                     0,
                     NULL,
                     &archiveEntryInfo->directory.chunkDirectory
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }
  archiveEntryInfo->directory.chunkDirectory.cryptAlgorithm = archiveInfo->jobOptions->cryptAlgorithm;

  /* init directory entry chunk */
  error = Crypt_init(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
    return error;
  }
  error = Chunk_init(&archiveEntryInfo->directory.chunkDirectoryEntry.info,
                     &archiveEntryInfo->directory.chunkDirectory.info,
                     NULL,
                     NULL,
                     CHUNK_ID_DIRECTORY_ENTRY,
                     CHUNK_DEFINITION_DIRECTORY_ENTRY,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,
                     &archiveEntryInfo->directory.chunkDirectoryEntry
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
    return error;
  }
  archiveEntryInfo->directory.chunkDirectoryEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveEntryInfo->directory.chunkDirectoryEntry.timeModified    = fileInfo->timeModified;
  archiveEntryInfo->directory.chunkDirectoryEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveEntryInfo->directory.chunkDirectoryEntry.userId          = fileInfo->userId;
  archiveEntryInfo->directory.chunkDirectoryEntry.groupId         = fileInfo->groupId;
  archiveEntryInfo->directory.chunkDirectoryEntry.permission      = fileInfo->permission;
  String_set(archiveEntryInfo->directory.chunkDirectoryEntry.name,directoryName);

  /* calculate header length */
  length = Chunk_getSize(CHUNK_DEFINITION_DIRECTORY,      0,                           &archiveEntryInfo->directory.chunkDirectory     )+
           Chunk_getSize(CHUNK_DEFINITION_DIRECTORY_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->directory.chunkDirectoryEntry);

  if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
  {
    /* ensure space in archive */
    error = ensureArchiveSpace(archiveEntryInfo->archiveInfo,
                               length
                              );
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
      Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo);
      Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
      return error;
    }

    /* write directory chunk */
    error = Chunk_create(&archiveEntryInfo->directory.chunkDirectory.info
  //                       Chunk_getSize(CHUNK_DEFINITION_DIRECTORY,0,&archiveEntryInfo->directory.chunkDirectory),
                        );
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
      Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo);
      Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
      return error;
    }
    error = Chunk_create(&archiveEntryInfo->directory.chunkDirectoryEntry.info
  //                       Chunk_getSize(CHUNK_DEFINITION_DIRECTORY_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->directory.chunkDirectoryEntry),
                        );
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
      Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo);
      Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
      return error;
    }
  }

  return ERROR_NONE;
}

Errors Archive_newLinkEntry(ArchiveInfo      *archiveInfo,
                            ArchiveEntryInfo *archiveEntryInfo,
                            const String     linkName,
                            const String     destinationName,
                            const FileInfo   *fileInfo
                           )
{
  Errors error;
  ulong  length;

  assert(archiveInfo != NULL);
  assert(archiveEntryInfo != NULL);
  assert(fileInfo != NULL);

  /* init crypt password */
  if ((archiveInfo->jobOptions->cryptAlgorithm != CRYPT_ALGORITHM_NONE) && (archiveInfo->cryptPassword == NULL))
  {
    archiveInfo->cryptPassword = getCryptPassword(archiveInfo->printableName,
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
  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm   = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength      = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_LINK;

  /* init link chunk */
  error = Chunk_init(&archiveEntryInfo->link.chunkLink.info,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_LINK,
                     CHUNK_DEFINITION_LINK,
                     0,
                     NULL,&archiveEntryInfo->link.chunkLink
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }
  archiveEntryInfo->link.chunkLink.cryptAlgorithm = archiveInfo->jobOptions->cryptAlgorithm;

  /* init link entry chunk */
  error = Crypt_init(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->link.chunkLink.info);
    return error;
  }
  error = Chunk_init(&archiveEntryInfo->link.chunkLinkEntry.info,
                     &archiveEntryInfo->link.chunkLink.info,
                     NULL,
                     NULL,
                     CHUNK_ID_LINK_ENTRY,
                     CHUNK_DEFINITION_LINK_ENTRY,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->link.chunkLinkEntry.cryptInfo,
                     &archiveEntryInfo->link.chunkLinkEntry
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->link.chunkLink.info);
    return error;
  }
  archiveEntryInfo->link.chunkLinkEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveEntryInfo->link.chunkLinkEntry.timeModified    = fileInfo->timeModified;
  archiveEntryInfo->link.chunkLinkEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveEntryInfo->link.chunkLinkEntry.userId          = fileInfo->userId;
  archiveEntryInfo->link.chunkLinkEntry.groupId         = fileInfo->groupId;
  archiveEntryInfo->link.chunkLinkEntry.permission      = fileInfo->permission;
  String_set(archiveEntryInfo->link.chunkLinkEntry.name,linkName);
  String_set(archiveEntryInfo->link.chunkLinkEntry.destinationName,destinationName);

  /* calculate length */
  length = Chunk_getSize(CHUNK_DEFINITION_LINK,      0,                           &archiveEntryInfo->link.chunkLink     )+
           Chunk_getSize(CHUNK_DEFINITION_LINK_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->link.chunkLinkEntry);

  if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
  {
    /* ensure space in archive */
    error = ensureArchiveSpace(archiveEntryInfo->archiveInfo,
                               length
                              );
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveEntryInfo->link.chunkLinkEntry.info);
      Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo);
      Chunk_done(&archiveEntryInfo->link.chunkLink.info);
      return error;
    }

    /* write link chunks */
    error = Chunk_create(&archiveEntryInfo->link.chunkLink.info
  //                       Chunk_getSize(CHUNK_DEFINITION_LINK,0,&archiveEntryInfo->link.chunkLink),
                        );
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveEntryInfo->link.chunkLinkEntry.info);
      Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo);
      Chunk_done(&archiveEntryInfo->link.chunkLink.info);
      return error;
    }
    error = Chunk_create(&archiveEntryInfo->link.chunkLinkEntry.info
  //                       Chunk_getSize(CHUNK_DEFINITION_LINK_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->link.chunkLinkEntry),
                        );
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveEntryInfo->link.chunkLinkEntry.info);
      Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo);
      Chunk_done(&archiveEntryInfo->link.chunkLink.info);
      return error;
    }
  }

  return ERROR_NONE;
}

Errors Archive_newHardLinkEntry(ArchiveInfo      *archiveInfo,
                                ArchiveEntryInfo *archiveEntryInfo,
                                const StringList *fileNameList,
                                const FileInfo   *fileInfo,
                                bool             compressFlag
                               )
{
  Errors            error;
  const StringNode  *stringNode;
  String            fileName;
  ChunkHardLinkName *chunkHardLinkName;

  assert(archiveInfo != NULL);
  assert(archiveEntryInfo != NULL);
  assert(fileInfo != NULL);

  /* init crypt password */
  if ((archiveInfo->jobOptions->cryptAlgorithm != CRYPT_ALGORITHM_NONE) && (archiveInfo->cryptPassword == NULL))
  {
    archiveInfo->cryptPassword = getCryptPassword(archiveInfo->printableName,
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
  archiveEntryInfo->archiveInfo                = archiveInfo;
  archiveEntryInfo->mode                       = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->hardLink.compressAlgorithm = compressFlag?archiveInfo->jobOptions->compressAlgorithm:COMPRESS_ALGORITHM_NONE;

  archiveEntryInfo->cryptAlgorithm             = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength                = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType           = ARCHIVE_ENTRY_TYPE_HARDLINK;

  List_init(&archiveEntryInfo->hardLink.chunkHardLinkNameList);

  archiveEntryInfo->hardLink.createdFlag       = FALSE;
  archiveEntryInfo->hardLink.headerLength      = 0;
  archiveEntryInfo->hardLink.headerWrittenFlag = FALSE;

  archiveEntryInfo->hardLink.buffer            = NULL;
  archiveEntryInfo->hardLink.bufferLength      = 0L;

  /* init hard link chunk */
  error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLink.info,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_HARDLINK,
                     CHUNK_DEFINITION_HARDLINK,
                     0,
                     NULL,
                     &archiveEntryInfo->hardLink.chunkHardLink
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }
  archiveEntryInfo->hardLink.chunkHardLink.compressAlgorithm = archiveEntryInfo->hardLink.compressAlgorithm;
  archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithm    = archiveInfo->jobOptions->cryptAlgorithm;

  /* init hard link entry chunk */
  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    return error;
  }
  error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info,
                     &archiveEntryInfo->hardLink.chunkHardLink.info,
                     NULL,
                     NULL,
                     CHUNK_ID_HARDLINK_ENTRY,
                     CHUNK_DEFINITION_HARDLINK_ENTRY,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,
                     &archiveEntryInfo->hardLink.chunkHardLinkEntry
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    return error;
  }
  archiveEntryInfo->hardLink.chunkHardLinkEntry.size            = fileInfo->size;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.timeModified    = fileInfo->timeModified;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.userId          = fileInfo->userId;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.groupId         = fileInfo->groupId;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.permission      = fileInfo->permission;

  /* init hard link name chunk */
  STRINGLIST_ITERATE(fileNameList,stringNode,fileName)
  {
    /* new node */
    chunkHardLinkName = LIST_NEW_NODE(ChunkHardLinkName);
    if (chunkHardLinkName == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    /* init hard link name chunk */
    error = Crypt_init(&chunkHardLinkName->cryptInfo,
                       archiveInfo->jobOptions->cryptAlgorithm,
                       archiveInfo->cryptPassword
                      );
    if (error != ERROR_NONE)
    {
      LIST_DELETE_NODE(chunkHardLinkName);
      LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
      {
        Crypt_done(&chunkHardLinkName->cryptInfo);
        Chunk_done(&chunkHardLinkName->info);
      }
      Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
      Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
      Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
      return error;
    }
    error = Chunk_init(&chunkHardLinkName->info,
                       &archiveEntryInfo->hardLink.chunkHardLink.info,
                       NULL,
                       NULL,
                       CHUNK_ID_HARDLINK_NAME,
                       CHUNK_DEFINITION_HARDLINK_NAME,
                       archiveEntryInfo->blockLength,
                       &chunkHardLinkName->cryptInfo,
                       chunkHardLinkName
                      );
    if (error != ERROR_NONE)
    {
      Crypt_done(&chunkHardLinkName->cryptInfo);
      LIST_DELETE_NODE(chunkHardLinkName);
      LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
      {
        Crypt_done(&chunkHardLinkName->cryptInfo);
        Chunk_done(&chunkHardLinkName->info);
      }
      Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
      Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
      Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
      return error;
    }
    String_set(chunkHardLinkName->name,fileName);

    List_append(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName);
  }

  /* init hard link data chunk */
  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
    {
      Crypt_done(&chunkHardLinkName->cryptInfo);
      Chunk_done(&chunkHardLinkName->info);
    }
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    return error;
  }
  error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                     &archiveEntryInfo->hardLink.chunkHardLink.info,
                     NULL,
                     NULL,
                     CHUNK_ID_HARDLINK_DATA,
                     CHUNK_DEFINITION_HARDLINK_DATA,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,
                     &archiveEntryInfo->hardLink.chunkHardLinkData
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
    LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
    {
      Crypt_done(&chunkHardLinkName->cryptInfo);
      Chunk_done(&chunkHardLinkName->info);
    }
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    return error;
  }
  archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset = 0LL;
  archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize   = 0LL;

  /* allocate data buffer */
  archiveEntryInfo->hardLink.bufferLength = (MAX_BUFFER_SIZE/archiveEntryInfo->blockLength)*archiveEntryInfo->blockLength;
  archiveEntryInfo->hardLink.buffer = (byte*)malloc(archiveEntryInfo->hardLink.bufferLength);
  if (archiveEntryInfo->hardLink.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init data compress */
  error = Compress_new(&archiveEntryInfo->hardLink.compressInfoData,
                       COMPRESS_MODE_DEFLATE,
                       archiveEntryInfo->hardLink.compressAlgorithm,
                       archiveEntryInfo->blockLength
//??? NYI
,NULL
,NULL
                      );
  if (error != ERROR_NONE)
  {
    free(archiveEntryInfo->hardLink.buffer);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
    LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
    {
      Crypt_done(&chunkHardLinkName->cryptInfo);
      Chunk_done(&chunkHardLinkName->info);
    }
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    return error;
  }

  /* init crypt */
  error = Crypt_init(&archiveEntryInfo->hardLink.cryptInfoData,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Compress_delete(&archiveEntryInfo->hardLink.compressInfoData);
    free(archiveEntryInfo->hardLink.buffer);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
    LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
    {
      Crypt_done(&chunkHardLinkName->cryptInfo);
      Chunk_done(&chunkHardLinkName->info);
    }
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    return error;
  }

  /* calculate header length */
  archiveEntryInfo->hardLink.headerLength = Chunk_getSize(CHUNK_DEFINITION_HARDLINK,      0,                           &archiveEntryInfo->hardLink.chunkHardLink     )+
                                           Chunk_getSize(CHUNK_DEFINITION_HARDLINK_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->hardLink.chunkHardLinkEntry)+
                                           Chunk_getSize(CHUNK_DEFINITION_HARDLINK_DATA, archiveEntryInfo->blockLength,&archiveEntryInfo->hardLink.chunkHardLinkData );
  LIST_ITERATE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
  {
    archiveEntryInfo->hardLink.headerLength += Chunk_getSize(CHUNK_DEFINITION_HARDLINK_NAME,archiveEntryInfo->blockLength,chunkHardLinkName);
  }

  return ERROR_NONE;
}

Errors Archive_newSpecialEntry(ArchiveInfo      *archiveInfo,
                               ArchiveEntryInfo *archiveEntryInfo,
                               const String     specialName,
                               const FileInfo   *fileInfo
                              )
{
  Errors error;
  ulong  length;

  assert(archiveInfo != NULL);
  assert(archiveEntryInfo != NULL);
  assert(fileInfo != NULL);

  /* init crypt password */
  if ((archiveInfo->jobOptions->cryptAlgorithm != CRYPT_ALGORITHM_NONE) && (archiveInfo->cryptPassword == NULL))
  {
    archiveInfo->cryptPassword = getCryptPassword(archiveInfo->printableName,
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
  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm   = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength      = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_SPECIAL;

  /* init special chunk */
  error = Chunk_init(&archiveEntryInfo->special.chunkSpecial.info,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_SPECIAL,
                     CHUNK_DEFINITION_SPECIAL,
                     0,
                     NULL,
                     &archiveEntryInfo->special.chunkSpecial
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }
  archiveEntryInfo->special.chunkSpecial.cryptAlgorithm = archiveInfo->jobOptions->cryptAlgorithm;

  /* init special entry chunk */
  error = Crypt_init(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->special.chunkSpecial.info);
    return error;
  }
  error = Chunk_init(&archiveEntryInfo->special.chunkSpecialEntry.info,
                     &archiveEntryInfo->special.chunkSpecial.info,
                     NULL,
                     NULL,
                     CHUNK_ID_SPECIAL_ENTRY,
                     CHUNK_DEFINITION_SPECIAL_ENTRY,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->special.chunkSpecialEntry.cryptInfo,
                     &archiveEntryInfo->special.chunkSpecialEntry
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->special.chunkSpecial.info);
    return error;
  }
  archiveEntryInfo->special.chunkSpecialEntry.specialType     = fileInfo->specialType;
  archiveEntryInfo->special.chunkSpecialEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveEntryInfo->special.chunkSpecialEntry.timeModified    = fileInfo->timeModified;
  archiveEntryInfo->special.chunkSpecialEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveEntryInfo->special.chunkSpecialEntry.userId          = fileInfo->userId;
  archiveEntryInfo->special.chunkSpecialEntry.groupId         = fileInfo->groupId;
  archiveEntryInfo->special.chunkSpecialEntry.permission      = fileInfo->permission;
  archiveEntryInfo->special.chunkSpecialEntry.major           = fileInfo->major;
  archiveEntryInfo->special.chunkSpecialEntry.minor           = fileInfo->minor;
  String_set(archiveEntryInfo->special.chunkSpecialEntry.name,specialName);

  /* calculate length */
  length = Chunk_getSize(CHUNK_DEFINITION_SPECIAL,      0,                           &archiveEntryInfo->special.chunkSpecial     )+
           Chunk_getSize(CHUNK_DEFINITION_SPECIAL_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->special.chunkSpecialEntry);

  if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
  {
    /* ensure space in archive */
    error = ensureArchiveSpace(archiveEntryInfo->archiveInfo,
                               length
                              );
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveEntryInfo->special.chunkSpecialEntry.info);
      Crypt_done(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo);
      Chunk_done(&archiveEntryInfo->special.chunkSpecial.info);
      return error;
    }

    /* write special chunks */
    error = Chunk_create(&archiveEntryInfo->special.chunkSpecial.info
  //                       Chunk_getSize(CHUNK_DEFINITION_SPECIAL,0,&archiveEntryInfo->special.chunkSpecial),
                        );
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveEntryInfo->special.chunkSpecialEntry.info);
      Crypt_done(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo);
      Chunk_done(&archiveEntryInfo->special.chunkSpecial.info);
      return error;
    }
    error = Chunk_create(&archiveEntryInfo->special.chunkSpecialEntry.info
  //                       Chunk_getSize(CHUNK_DEFINITION_SPECIAL_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->special.chunkSpecialEntry),
  //                       &archiveEntryInfo->special.chunkSpecialEntry
                        );
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveEntryInfo->special.chunkSpecialEntry.info);
      Crypt_done(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo);
      Chunk_done(&archiveEntryInfo->special.chunkSpecial.info);
      return error;
    }
  }

  return ERROR_NONE;
}

Errors Archive_getNextArchiveEntryType(ArchiveInfo       *archiveInfo,
                                       ArchiveEntryTypes *archiveEntryType,
                                       bool              skipUnknownChunksFlag
                                      )
{
  Errors         error;
  ChunkHeader    chunkHeader;
  bool           decryptedFlag;
  PasswordHandle passwordHandle;
  const Password *password;

  assert(archiveInfo != NULL);
  assert(archiveEntryType != NULL);

  /* check for pending error */
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

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
                                           archiveInfo->printableName,
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
      case CHUNK_ID_HARDLINK:
      case CHUNK_ID_SPECIAL:
        break;
      default:
        if (skipUnknownChunksFlag)
        {
          /* skip unknown chunks */
          error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
          if (error != ERROR_NONE)
          {
            return error;
          }
          if (globalOptions.verboseLevel >= 3)
          {
            printWarning("Skipped unexpected chunk '%s'\n",Chunk_idToString(chunkHeader.id));
          }
        }
        else
        {
          /* unknown chunk */
          return ERROR_UNKNOWN_CHUNK;
        }
        break;
    }
  }
  while (   (chunkHeader.id != CHUNK_ID_FILE)
         && (chunkHeader.id != CHUNK_ID_IMAGE)
         && (chunkHeader.id != CHUNK_ID_DIRECTORY)
         && (chunkHeader.id != CHUNK_ID_LINK)
         && (chunkHeader.id != CHUNK_ID_HARDLINK)
         && (chunkHeader.id != CHUNK_ID_SPECIAL)
        );

  /* get file type */
  switch (chunkHeader.id)
  {
    case CHUNK_ID_FILE:      (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_FILE;      break;
    case CHUNK_ID_IMAGE:     (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_IMAGE;     break;
    case CHUNK_ID_DIRECTORY: (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_DIRECTORY; break;
    case CHUNK_ID_LINK:      (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_LINK;      break;
    case CHUNK_ID_HARDLINK:  (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_HARDLINK;  break;
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
                             ArchiveEntryInfo   *archiveEntryInfo,
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
  assert(archiveEntryInfo != NULL);
  assert(fileInfo != NULL);

  /* check for pending error */
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  /* init archive file info */
  archiveEntryInfo->archiveInfo       = archiveInfo;
  archiveEntryInfo->mode              = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType  = ARCHIVE_ENTRY_TYPE_FILE;

  archiveEntryInfo->file.buffer       = NULL;
  archiveEntryInfo->file.bufferLength = 0;

  /* init file chunk */
  error = Chunk_init(&archiveEntryInfo->file.chunkFile.info,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_FILE,
                     CHUNK_DEFINITION_FILE,
                     0,
                     NULL,
                     &archiveEntryInfo->file.chunkFile
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
      Chunk_done(&archiveEntryInfo->file.chunkFile.info);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_FILE)
    {
      error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->file.chunkFile.info);
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_FILE);

  /* read file chunk */
  error = Chunk_open(&archiveEntryInfo->file.chunkFile.info,
                     &chunkHeader,
                     Chunk_getSize(CHUNK_DEFINITION_FILE,0,NULL)
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  archiveEntryInfo->cryptAlgorithm         = archiveEntryInfo->file.chunkFile.cryptAlgorithm;
  archiveEntryInfo->file.compressAlgorithm = archiveEntryInfo->file.chunkFile.compressAlgorithm;

  /* detect block length of used crypt algorithm */
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  /* allocate buffer */
  archiveEntryInfo->file.bufferLength = (MAX_BUFFER_SIZE/archiveEntryInfo->blockLength)*archiveEntryInfo->blockLength;
  archiveEntryInfo->file.buffer = (byte*)malloc(archiveEntryInfo->file.bufferLength);
  if (archiveEntryInfo->file.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init data compress */
  error = Compress_new(&archiveEntryInfo->file.compressInfoData,
                       COMPRESS_MODE_INFLATE,
                       archiveEntryInfo->file.compressAlgorithm,
                       archiveEntryInfo->blockLength
//??? NYI
,NULL
,NULL
                      );
  if (error != ERROR_NONE)
  {
    free(archiveEntryInfo->file.buffer);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }

  /* try to read file entry with all passwords */
  Chunk_tell(&archiveEntryInfo->file.chunkFile.info,&index);
  if (archiveEntryInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo->printableName,
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
  error              = ERROR_NONE;
  while (   !foundFileEntryFlag
         && !foundFileDataFlag
         && (error == ERROR_NONE)
        )
  {
    /* reset chunk read position */
    Chunk_seek(&archiveEntryInfo->file.chunkFile.info,index);

    /* init crypt */
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->file.chunkFileEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->file.chunkFileData.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->file.cryptInfoData,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
      }
    }

    /* init file entry chunk */
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->file.chunkFileEntry.info,
                         &archiveEntryInfo->file.chunkFile.info,
                         NULL,
                         NULL,
                         CHUNK_ID_FILE_ENTRY,
                         CHUNK_DEFINITION_FILE_ENTRY,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->file.chunkFileEntry.cryptInfo,
                         &archiveEntryInfo->file.chunkFileEntry
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->file.cryptInfoData);
        Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
      }
    }

    /* init file data chunk */
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->file.chunkFileData.info,
                         &archiveEntryInfo->file.chunkFile.info,
                         NULL,
                         NULL,
                         CHUNK_ID_FILE_DATA,
                         CHUNK_DEFINITION_FILE_DATA,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->file.chunkFileData.cryptInfo,
                         &archiveEntryInfo->file.chunkFileData
                        );
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
        Crypt_done(&archiveEntryInfo->file.cryptInfoData);
        Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
      }
    }

    /* try to read file entry/data chunks */
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveEntryInfo->file.chunkFile.info)
             && (error == ERROR_NONE)
             && (!foundFileEntryFlag || !foundFileDataFlag)
            )
      {
        error = Chunk_nextSub(&archiveEntryInfo->file.chunkFile.info,&subChunkHeader);
        if (error != ERROR_NONE)
        {
          break;
        }

        switch (subChunkHeader.id)
        {
          case CHUNK_ID_FILE_ENTRY:
            /* read file entry chunk */
            error = Chunk_open(&archiveEntryInfo->file.chunkFileEntry.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            /* get file meta data */
            String_set(fileName,archiveEntryInfo->file.chunkFileEntry.name);
            fileInfo->type            = FILE_TYPE_FILE;
            fileInfo->size            = archiveEntryInfo->file.chunkFileEntry.size;
            fileInfo->timeLastAccess  = archiveEntryInfo->file.chunkFileEntry.timeLastAccess;
            fileInfo->timeModified    = archiveEntryInfo->file.chunkFileEntry.timeModified;
            fileInfo->timeLastChanged = archiveEntryInfo->file.chunkFileEntry.timeLastChanged;
            fileInfo->userId          = archiveEntryInfo->file.chunkFileEntry.userId;
            fileInfo->groupId         = archiveEntryInfo->file.chunkFileEntry.groupId;
            fileInfo->permission      = archiveEntryInfo->file.chunkFileEntry.permission;

            foundFileEntryFlag = TRUE;
            break;
          case CHUNK_ID_FILE_DATA:
            /* read file data chunk */
            error = Chunk_open(&archiveEntryInfo->file.chunkFileData.info,
                               &subChunkHeader,
                               Chunk_getSize(CHUNK_DEFINITION_FILE_DATA,archiveEntryInfo->blockLength,NULL)
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            if (fragmentOffset != NULL) (*fragmentOffset) = archiveEntryInfo->file.chunkFileData.fragmentOffset;
            if (fragmentSize   != NULL) (*fragmentSize)   = archiveEntryInfo->file.chunkFileData.fragmentSize;

            foundFileDataFlag = TRUE;
            break;
          default:
            error = Chunk_skipSub(&archiveEntryInfo->file.chunkFile.info,&subChunkHeader);
            if (error != ERROR_NONE)
            {
              break;
            }
            break;
        }
      }
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->file.chunkFileData.info);
        Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
        Crypt_done(&archiveEntryInfo->file.cryptInfoData);
        Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
      }
    }

    if (error != ERROR_NONE)
    {
      if (   (archiveEntryInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
          && (archiveInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        /* get next password */
        password = getNextDecryptPassword(&passwordHandle);

        /* reset error and try next password */
        if (password != NULL)
        {
          error = ERROR_NONE;
        }
      }
      else
      {
        /* no more passwords when no encryption or asymmetric encryption is used */
        password = NULL;
      }
    }
  } /* while */

  if (!foundFileEntryFlag || !foundFileDataFlag)
  {
    if (foundFileEntryFlag) String_delete(archiveEntryInfo->file.chunkFileEntry.name);
    Compress_delete(&archiveEntryInfo->file.compressInfoData);
    free(archiveEntryInfo->file.buffer);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);

    if      (error != ERROR_NONE) return error;
    else if (!passwordFlag)       return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)      return ERROR_INVALID_PASSWORD;
    else if (!foundFileEntryFlag) return ERROR_NO_FILE_ENTRY;
    else if (!foundFileDataFlag)  return ERROR_NO_FILE_DATA;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (compressAlgorithm != NULL) (*compressAlgorithm) = archiveEntryInfo->file.compressAlgorithm;
  if (cryptAlgorithm    != NULL) (*cryptAlgorithm)    = archiveEntryInfo->cryptAlgorithm;
  if (cryptType         != NULL) (*cryptType)         = archiveInfo->cryptType;

  /* reset compress, crypt */
  Compress_reset(&archiveEntryInfo->file.compressInfoData);
  Crypt_reset(&archiveEntryInfo->file.chunkFileData.cryptInfo,0);

  return ERROR_NONE;
}

Errors Archive_readImageEntry(ArchiveInfo        *archiveInfo,
                              ArchiveEntryInfo   *archiveEntryInfo,
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
  assert(archiveEntryInfo != NULL);
  assert(deviceInfo != NULL);

  /* check for pending error */
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  /* init archive file info */
  archiveEntryInfo->archiveInfo        = archiveInfo;
  archiveEntryInfo->mode               = ARCHIVE_MODE_READ;
//???
  archiveEntryInfo->archiveEntryType   = ARCHIVE_ENTRY_TYPE_IMAGE;

  archiveEntryInfo->image.buffer       = NULL;
  archiveEntryInfo->image.bufferLength = 0;

  /* init image chunk */
  error = Chunk_init(&archiveEntryInfo->image.chunkImage.info,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_IMAGE,
                     CHUNK_DEFINITION_IMAGE,
                     0,
                     NULL,
                     &archiveEntryInfo->image.chunkImage
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
      Chunk_done(&archiveEntryInfo->image.chunkImage.info);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_IMAGE)
    {
      error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->image.chunkImage.info);
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_IMAGE);

  /* read image chunk */
  error = Chunk_open(&archiveEntryInfo->image.chunkImage.info,
                     &chunkHeader,
                     Chunk_getSize(CHUNK_DEFINITION_IMAGE,0,NULL)
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  archiveEntryInfo->cryptAlgorithm          = archiveEntryInfo->image.chunkImage.cryptAlgorithm;
  archiveEntryInfo->image.compressAlgorithm = archiveEntryInfo->image.chunkImage.compressAlgorithm;

  /* detect block length of used crypt algorithm */
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  /* allocate buffer */
  archiveEntryInfo->image.bufferLength = (MAX_BUFFER_SIZE/archiveEntryInfo->blockLength)*archiveEntryInfo->blockLength;
  archiveEntryInfo->image.buffer = (byte*)malloc(archiveEntryInfo->image.bufferLength);
  if (archiveEntryInfo->image.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init data compress */
  error = Compress_new(&archiveEntryInfo->image.compressInfoData,
                       COMPRESS_MODE_INFLATE,
                       archiveEntryInfo->image.compressAlgorithm,
                       archiveEntryInfo->blockLength
//??? NYI
,NULL
,NULL
                      );
  if (error != ERROR_NONE)
  {
    free(archiveEntryInfo->image.buffer);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }

  /* try to read image entry with all passwords */
  Chunk_tell(&archiveEntryInfo->image.chunkImage.info,&index);
  if (archiveEntryInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo->printableName,
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
  error               = ERROR_NONE;
  while (   !foundImageEntryFlag
         && !foundImageDataFlag
         && (error == ERROR_NONE)
        )
  {
    error = ERROR_NONE;

    /* reset chunk read position */
    Chunk_seek(&archiveEntryInfo->image.chunkImage.info,index);

    /* init crypt */
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->image.chunkImageEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->image.chunkImageData.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->image.cryptInfoData,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
      }
    }

    /* init image entry chunk */
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->image.chunkImageEntry.info,
                         &archiveEntryInfo->image.chunkImage.info,
                         NULL,
                         NULL,
                         CHUNK_ID_IMAGE_ENTRY,
                         CHUNK_DEFINITION_IMAGE_ENTRY,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->image.chunkImageEntry.cryptInfo,
                         &archiveEntryInfo->image.chunkImageEntry
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->image.cryptInfoData);
        Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
      }
    }

    /* init image data chunk */
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->image.chunkImageData.info,
                         &archiveEntryInfo->image.chunkImage.info,
                         NULL,
                         NULL,
                         CHUNK_ID_IMAGE_DATA,
                         CHUNK_DEFINITION_IMAGE_DATA,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->image.chunkImageData.cryptInfo,
                         &archiveEntryInfo->image.chunkImageData
                        );
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
        Crypt_done(&archiveEntryInfo->image.cryptInfoData);
        Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
      }
    }

    /* try to read image entry/data chunks */
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveEntryInfo->image.chunkImage.info)
             && (error == ERROR_NONE)
             && (!foundImageEntryFlag || !foundImageDataFlag)
            )
      {
        error = Chunk_nextSub(&archiveEntryInfo->image.chunkImage.info,&subChunkHeader);
        if (error != ERROR_NONE)
        {
          break;
        }

        switch (subChunkHeader.id)
        {
          case CHUNK_ID_IMAGE_ENTRY:
            /* read image entry chunk */
            error = Chunk_open(&archiveEntryInfo->image.chunkImageEntry.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            /* get image meta data */
            String_set(deviceName,archiveEntryInfo->image.chunkImageEntry.name);
            deviceInfo->size      = archiveEntryInfo->image.chunkImageEntry.size;
            deviceInfo->blockSize = archiveEntryInfo->image.chunkImageEntry.blockSize;

            foundImageEntryFlag = TRUE;
            break;
          case CHUNK_ID_IMAGE_DATA:
            /* read image data chunk */
            error = Chunk_open(&archiveEntryInfo->image.chunkImageData.info,
                               &subChunkHeader,
                               Chunk_getSize(CHUNK_DEFINITION_FILE_DATA,archiveEntryInfo->blockLength,NULL)
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            if (blockOffset != NULL) (*blockOffset) = archiveEntryInfo->image.chunkImageData.blockOffset;
            if (blockCount  != NULL) (*blockCount)  = archiveEntryInfo->image.chunkImageData.blockCount;

            foundImageDataFlag = TRUE;
            break;
          default:
            error = Chunk_skipSub(&archiveEntryInfo->image.chunkImage.info,&subChunkHeader);
            if (error != ERROR_NONE)
            {
              break;
            }
            break;
        }
      }
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->image.chunkImageData.info);
        Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
        Crypt_done(&archiveEntryInfo->image.cryptInfoData);
        Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
      }
    }

    if (error != ERROR_NONE)
    {
      if (   (archiveEntryInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
          && (archiveInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        /* get next password */
        password = getNextDecryptPassword(&passwordHandle);

        /* reset error and try next password */
        if (password != NULL)
        {
          error = ERROR_NONE;
        }
      }
      else
      {
        /* no more passwords when no encryption or asymmetric encryption is used */
        password = NULL;
      }
    }
  } /* while */

  if (!foundImageEntryFlag || !foundImageDataFlag)
  {
    if (foundImageEntryFlag) String_delete(archiveEntryInfo->image.chunkImageEntry.name);
    Compress_delete(&archiveEntryInfo->image.compressInfoData);
    free(archiveEntryInfo->image.buffer);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);

    if      (error != ERROR_NONE)  return error;
    else if (!passwordFlag)        return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)       return ERROR_INVALID_PASSWORD;
    else if (!foundImageEntryFlag) return ERROR_NO_IMAGE_ENTRY;
    else if (!foundImageDataFlag)  return ERROR_NO_IMAGE_DATA;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (compressAlgorithm != NULL) (*compressAlgorithm) = archiveEntryInfo->image.compressAlgorithm;
  if (cryptAlgorithm    != NULL) (*cryptAlgorithm)    = archiveEntryInfo->cryptAlgorithm;
  if (cryptType         != NULL) (*cryptType)         = archiveInfo->cryptType;

  /* reset compress, crypt */
  Compress_reset(&archiveEntryInfo->image.compressInfoData);
  Crypt_reset(&archiveEntryInfo->image.chunkImageData.cryptInfo,0);

  return ERROR_NONE;
}

Errors Archive_readDirectoryEntry(ArchiveInfo      *archiveInfo,
                                  ArchiveEntryInfo *archiveEntryInfo,
                                  CryptAlgorithms  *cryptAlgorithm,
                                  CryptTypes       *cryptType,
                                  String           directoryName,
                                  FileInfo         *fileInfo
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
  assert(archiveEntryInfo != NULL);
  assert(fileInfo != NULL);

  /* check for pending error */
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  /* init archive file info */
  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_DIRECTORY;

  /* init directory chunk */
  error = Chunk_init(&archiveEntryInfo->directory.chunkDirectory.info,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_DIRECTORY,
                     CHUNK_DEFINITION_DIRECTORY,
                     0,
                     NULL,
                     &archiveEntryInfo->directory.chunkDirectory
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
      Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_DIRECTORY)
    {
      error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_DIRECTORY);

  /* read directory chunk */
  error = Chunk_open(&archiveEntryInfo->directory.chunkDirectory.info,
                     &chunkHeader,
                     Chunk_getSize(CHUNK_DEFINITION_DIRECTORY,0,NULL)
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  archiveEntryInfo->cryptAlgorithm = archiveEntryInfo->directory.chunkDirectory.cryptAlgorithm;

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  /* try to read directory entry with all passwords */
  Chunk_tell(&archiveEntryInfo->directory.chunkDirectory.info,&index);
  if (archiveEntryInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo->printableName,
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
  error                   = ERROR_NONE;
  while (   !foundDirectoryEntryFlag
         && (error == ERROR_NONE)
        )
  {
    /* reset chunk read position */
    Chunk_seek(&archiveEntryInfo->directory.chunkDirectory.info,index);

    /* init crypt */
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
      }
    }

    /* init directory entry chunk */
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->directory.chunkDirectoryEntry.info,
                         &archiveEntryInfo->directory.chunkDirectory.info,
                         NULL,
                         NULL,
                         CHUNK_ID_DIRECTORY_ENTRY,
                         CHUNK_DEFINITION_DIRECTORY_ENTRY,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,
                         &archiveEntryInfo->directory.chunkDirectoryEntry
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo);
      }
    }

    /* read directory entry */
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveEntryInfo->directory.chunkDirectory.info)
             && (error == ERROR_NONE)
             && !foundDirectoryEntryFlag
            )
      {
        error = Chunk_nextSub(&archiveEntryInfo->directory.chunkDirectory.info,&subChunkHeader);
        if (error != ERROR_NONE)
        {
          break;
        }

        switch (subChunkHeader.id)
        {
          case CHUNK_ID_DIRECTORY_ENTRY:
            /* read directory entry chunk */
            error = Chunk_open(&archiveEntryInfo->directory.chunkDirectoryEntry.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            /* get directory meta data */
            String_set(directoryName,archiveEntryInfo->directory.chunkDirectoryEntry.name);
            fileInfo->type            = FILE_TYPE_DIRECTORY;
            fileInfo->timeLastAccess  = archiveEntryInfo->directory.chunkDirectoryEntry.timeLastAccess;
            fileInfo->timeModified    = archiveEntryInfo->directory.chunkDirectoryEntry.timeModified;
            fileInfo->timeLastChanged = archiveEntryInfo->directory.chunkDirectoryEntry.timeLastChanged;
            fileInfo->userId          = archiveEntryInfo->directory.chunkDirectoryEntry.userId;
            fileInfo->groupId         = archiveEntryInfo->directory.chunkDirectoryEntry.groupId;
            fileInfo->permission      = archiveEntryInfo->directory.chunkDirectoryEntry.permission;

            foundDirectoryEntryFlag = TRUE;
            break;
          default:
            error = Chunk_skipSub(&archiveEntryInfo->directory.chunkDirectory.info,&subChunkHeader);
            if (error != ERROR_NONE)
            {
              break;
            }
            break;
        }
      }
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
        Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo);
      }
    }

    if (error != ERROR_NONE)
    {
      if (   (archiveEntryInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
          && (archiveInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        /* get next password */
        password = getNextDecryptPassword(&passwordHandle);

        /* reset error and try next password */
        if (password != NULL)
        {
          error = ERROR_NONE;
        }
      }
      else
      {
        /* no more passwords when no encryption or asymmetric encryption is used */
        password = NULL;
      }
    }
  } /* while */

  if (!foundDirectoryEntryFlag)
  {
    Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);

    if      (error != ERROR_NONE)      return error;
    else if (!passwordFlag)            return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)           return ERROR_INVALID_PASSWORD;
    else if (!foundDirectoryEntryFlag) return ERROR_NO_DIRECTORY_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveEntryInfo->cryptAlgorithm;
  if (cryptType      != NULL) (*cryptType)      = archiveInfo->cryptType;

  return ERROR_NONE;
}

Errors Archive_readLinkEntry(ArchiveInfo      *archiveInfo,
                             ArchiveEntryInfo *archiveEntryInfo,
                             CryptAlgorithms  *cryptAlgorithm,
                             CryptTypes       *cryptType,
                             String           linkName,
                             String           destinationName,
                             FileInfo         *fileInfo
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
  assert(archiveEntryInfo != NULL);
  assert(fileInfo != NULL);

  /* check for pending error */
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  /* init archive file info */
  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_LINK;

  /* init link chunk */
  error = Chunk_init(&archiveEntryInfo->link.chunkLink.info,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_LINK,
                     CHUNK_DEFINITION_LINK,
                     0,
                     NULL,
                     &archiveEntryInfo->link.chunkLink
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* find next soft link chunk */
  do
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveEntryInfo->link.chunkLink.info);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_LINK)
    {
      Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->link.chunkLink.info);
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_LINK);

  /* read link chunk */
  error = Chunk_open(&archiveEntryInfo->link.chunkLink.info,
                     &chunkHeader,
                     Chunk_getSize(CHUNK_DEFINITION_LINK,0,NULL)
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->link.chunkLink.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  archiveEntryInfo->cryptAlgorithm = archiveEntryInfo->link.chunkLink.cryptAlgorithm;

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->link.chunkLink.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  /* try to read link entry with all passwords */
  Chunk_tell(&archiveEntryInfo->link.chunkLink.info,&index);
  if (archiveEntryInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo->printableName,
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
  foundLinkEntryFlag = FALSE;
  error              = ERROR_NONE;
  while (   !foundLinkEntryFlag
         && (error == ERROR_NONE)
        )
  {
    error = ERROR_NONE;

    /* reset chunk read position */
    Chunk_seek(&archiveEntryInfo->link.chunkLink.info,index);

    /* init crypt */
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
      }
    }

    /* init link entry chunk */
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->link.chunkLinkEntry.info,
                         &archiveEntryInfo->link.chunkLink.info,
                         NULL,
                         NULL,
                         CHUNK_ID_LINK_ENTRY,
                         CHUNK_DEFINITION_LINK_ENTRY,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->link.chunkLinkEntry.cryptInfo,
                         &archiveEntryInfo->link.chunkLinkEntry
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo);
      }
    }

    /* read link entry */
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveEntryInfo->link.chunkLink.info)
             && (error == ERROR_NONE)
             && !foundLinkEntryFlag
            )
      {
        error = Chunk_nextSub(&archiveEntryInfo->link.chunkLink.info,&subChunkHeader);
        if (error != ERROR_NONE)
        {
          break;
        }

        switch (subChunkHeader.id)
        {
          case CHUNK_ID_LINK_ENTRY:
            /* read link entry chunk */
            error = Chunk_open(&archiveEntryInfo->link.chunkLinkEntry.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            /* get link meta data */
            String_set(linkName,archiveEntryInfo->link.chunkLinkEntry.name);
            String_set(destinationName,archiveEntryInfo->link.chunkLinkEntry.destinationName);
            fileInfo->type            = FILE_TYPE_LINK;
            fileInfo->timeLastAccess  = archiveEntryInfo->link.chunkLinkEntry.timeLastAccess;
            fileInfo->timeModified    = archiveEntryInfo->link.chunkLinkEntry.timeModified;
            fileInfo->timeLastChanged = archiveEntryInfo->link.chunkLinkEntry.timeLastChanged;
            fileInfo->userId          = archiveEntryInfo->link.chunkLinkEntry.userId;
            fileInfo->groupId         = archiveEntryInfo->link.chunkLinkEntry.groupId;
            fileInfo->permission      = archiveEntryInfo->link.chunkLinkEntry.permission;

            foundLinkEntryFlag = TRUE;
            break;
          default:
            error = Chunk_skipSub(&archiveEntryInfo->link.chunkLink.info,&subChunkHeader);
            if (error != ERROR_NONE)
            {
              break;
            }
            break;
        }
      }
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->link.chunkLinkEntry.info);
        Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo);
      }
    }

    if (error != ERROR_NONE)
    {
      if (   (archiveEntryInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
          && (archiveInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        /* get next password */
        password = getNextDecryptPassword(&passwordHandle);

        /* reset error and try next password */
        if (password != NULL)
        {
          error = ERROR_NONE;
        }
      }
      else
      {
        /* no more passwords when no encryption or asymmetric encryption is used */
        password = NULL;
      }
    }
  } /* while */

  if (!foundLinkEntryFlag)
  {
    Chunk_done(&archiveEntryInfo->link.chunkLink.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);

    if      (error != ERROR_NONE)     return error;
    else if (!passwordFlag)           return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)          return ERROR_INVALID_PASSWORD;
    else if (!foundLinkEntryFlag) return ERROR_NO_LINK_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveEntryInfo->cryptAlgorithm;
  if (cryptType      != NULL) (*cryptType)      = archiveInfo->cryptType;

  return ERROR_NONE;
}

Errors Archive_readHardLinkEntry(ArchiveInfo        *archiveInfo,
                                 ArchiveEntryInfo   *archiveEntryInfo,
                                 CompressAlgorithms *compressAlgorithm,
                                 CryptAlgorithms    *cryptAlgorithm,
                                 CryptTypes         *cryptType,
                                 StringList         *fileNameList,
                                 FileInfo           *fileInfo,
                                 uint64             *fragmentOffset,
                                 uint64             *fragmentSize
                                )
{
  Errors            error;
  ChunkHeader       chunkHeader;
  uint64            index;
  PasswordHandle    passwordHandle;
  const Password    *password;
  bool              passwordFlag;
  bool              decryptedFlag;
  ChunkHeader       subChunkHeader;
  bool              foundHardLinkEntryFlag,foundHardLinkDataFlag;
  ChunkHardLinkName *chunkHardLinkName;

  assert(archiveInfo != NULL);
  assert(archiveEntryInfo != NULL);
  assert(fileNameList != NULL);
  assert(fileInfo != NULL);

  /* check for pending error */
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  /* init archive file info */
  archiveEntryInfo->archiveInfo           = archiveInfo;
  archiveEntryInfo->mode                  = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType      = ARCHIVE_ENTRY_TYPE_HARDLINK;

  archiveEntryInfo->hardLink.buffer       = NULL;
  archiveEntryInfo->hardLink.bufferLength = 0;

  /* init hard link chunk */
  error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLink.info,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_HARDLINK,
                     CHUNK_DEFINITION_HARDLINK,
                     0,
                     NULL,
                     &archiveEntryInfo->hardLink.chunkHardLink
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  /* find next hard link chunk */
  do
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_HARDLINK)
    {
      error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_HARDLINK);

  /* read hard link chunk */
  error = Chunk_open(&archiveEntryInfo->hardLink.chunkHardLink.info,
                     &chunkHeader,
                     Chunk_getSize(CHUNK_DEFINITION_HARDLINK,0,NULL)
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  archiveEntryInfo->cryptAlgorithm         = archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithm;
  archiveEntryInfo->hardLink.compressAlgorithm = archiveEntryInfo->hardLink.chunkHardLink.compressAlgorithm;

  /* detect block length of used crypt algorithm */
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  /* allocate buffer */
  archiveEntryInfo->hardLink.bufferLength = (MAX_BUFFER_SIZE/archiveEntryInfo->blockLength)*archiveEntryInfo->blockLength;
  archiveEntryInfo->hardLink.buffer = (byte*)malloc(archiveEntryInfo->hardLink.bufferLength);
  if (archiveEntryInfo->hardLink.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init data compress */
  error = Compress_new(&archiveEntryInfo->hardLink.compressInfoData,
                       COMPRESS_MODE_INFLATE,
                       archiveEntryInfo->hardLink.compressAlgorithm,
                       archiveEntryInfo->blockLength
//??? NYI
,NULL
,NULL
                      );
  if (error != ERROR_NONE)
  {
    free(archiveEntryInfo->hardLink.buffer);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }

  /* try to read hard link entry with all passwords */
  Chunk_tell(&archiveEntryInfo->hardLink.chunkHardLink.info,&index);
  if (archiveEntryInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo->printableName,
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
  foundHardLinkEntryFlag = FALSE;
  foundHardLinkDataFlag  = FALSE;
  error                  = ERROR_NONE;
  while (   !foundHardLinkEntryFlag
         && !foundHardLinkDataFlag
         && (error == ERROR_NONE)
        )
  {
    error = ERROR_NONE;

    /* reset chunk read position */
    Chunk_seek(&archiveEntryInfo->hardLink.chunkHardLink.info,index);

    /* init crypt */
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.cryptInfoData,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
      }
    }

    /* init hard link entry chunk */
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info,
                         &archiveEntryInfo->hardLink.chunkHardLink.info,
                         NULL,
                         NULL,
                         CHUNK_ID_HARDLINK_ENTRY,
                         CHUNK_DEFINITION_HARDLINK_ENTRY,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,
                         &archiveEntryInfo->hardLink.chunkHardLinkEntry
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->hardLink.cryptInfoData);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
      }
    }

    /* init hard link data chunk */
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                         &archiveEntryInfo->hardLink.chunkHardLink.info,
                         NULL,
                         NULL,
                         CHUNK_ID_HARDLINK_DATA,
                         CHUNK_DEFINITION_HARDLINK_DATA,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,
                         &archiveEntryInfo->hardLink.chunkHardLinkData
                        );
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
        Crypt_done(&archiveEntryInfo->hardLink.cryptInfoData);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
      }
    }

    /* try to read hard link entry/name/data chunks */
    if (error == ERROR_NONE)
    {
      List_init(&archiveEntryInfo->hardLink.chunkHardLinkNameList);
      while (   !Chunk_eofSub(&archiveEntryInfo->hardLink.chunkHardLink.info)
             && (error == ERROR_NONE)
             && (!foundHardLinkEntryFlag || !foundHardLinkDataFlag)
            )
      {
        error = Chunk_nextSub(&archiveEntryInfo->hardLink.chunkHardLink.info,&subChunkHeader);
        if (error != ERROR_NONE)
        {
          break;
        }

        switch (subChunkHeader.id)
        {
          case CHUNK_ID_HARDLINK_ENTRY:
            /* read hard link entry chunk */
            error = Chunk_open(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            /* get hard link meta data */
            fileInfo->type            = FILE_TYPE_HARDLINK;
            fileInfo->size            = archiveEntryInfo->hardLink.chunkHardLinkEntry.size;
            fileInfo->timeLastAccess  = archiveEntryInfo->hardLink.chunkHardLinkEntry.timeLastAccess;
            fileInfo->timeModified    = archiveEntryInfo->hardLink.chunkHardLinkEntry.timeModified;
            fileInfo->timeLastChanged = archiveEntryInfo->hardLink.chunkHardLinkEntry.timeLastChanged;
            fileInfo->userId          = archiveEntryInfo->hardLink.chunkHardLinkEntry.userId;
            fileInfo->groupId         = archiveEntryInfo->hardLink.chunkHardLinkEntry.groupId;
            fileInfo->permission      = archiveEntryInfo->hardLink.chunkHardLinkEntry.permission;

            foundHardLinkEntryFlag = TRUE;
            break;
          case CHUNK_ID_HARDLINK_NAME:
            /* new hard link name */
            chunkHardLinkName = LIST_NEW_NODE(ChunkHardLinkName);
            if (chunkHardLinkName == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }

            /* init hard link name chunk */
            error = Crypt_init(&chunkHardLinkName->cryptInfo,
                               archiveEntryInfo->cryptAlgorithm,
                               password
                              );
            if (error != ERROR_NONE)
            {
              LIST_DELETE_NODE(chunkHardLinkName);
              break;
            }
            error = Chunk_init(&chunkHardLinkName->info,
                               &archiveEntryInfo->hardLink.chunkHardLink.info,
                               NULL,
                               NULL,
                               CHUNK_ID_HARDLINK_NAME,
                               CHUNK_DEFINITION_HARDLINK_NAME,
                               archiveEntryInfo->blockLength,
                               &chunkHardLinkName->cryptInfo,
                               chunkHardLinkName
                              );
            if (error != ERROR_NONE)
            {
              Crypt_done(&chunkHardLinkName->cryptInfo);
              LIST_DELETE_NODE(chunkHardLinkName);
              break;
            }

            /* read hard link name chunk */
            error = Chunk_open(&chunkHardLinkName->info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              Chunk_done(&chunkHardLinkName->info);
              Crypt_done(&chunkHardLinkName->cryptInfo);
              LIST_DELETE_NODE(chunkHardLinkName);
              break;
            }
            StringList_append(fileNameList,chunkHardLinkName->name);

            List_append(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName);
            break;
          case CHUNK_ID_HARDLINK_DATA:
            /* read hard link data chunk */
            error = Chunk_open(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                               &subChunkHeader,
                               Chunk_getSize(CHUNK_DEFINITION_HARDLINK_DATA,archiveEntryInfo->blockLength,NULL)
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            if (fragmentOffset != NULL) (*fragmentOffset) = archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset;
            if (fragmentSize   != NULL) (*fragmentSize)   = archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize;

            foundHardLinkDataFlag = TRUE;
            break;
          default:
            error = Chunk_skipSub(&archiveEntryInfo->hardLink.chunkHardLink.info,&subChunkHeader);
            if (error != ERROR_NONE)
            {
              break;
            }
            break;
        }
      }
      if (error != ERROR_NONE)
      {
        /* free resources */
        Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
        LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
        {
          Crypt_done(&chunkHardLinkName->cryptInfo);
          Chunk_done(&chunkHardLinkName->info);
        }
        Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
        Crypt_done(&archiveEntryInfo->hardLink.cryptInfoData);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
      }
    }

    if (error != ERROR_NONE)
    {
      if (   (archiveEntryInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
          && (archiveInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        /* get next password */
        password = getNextDecryptPassword(&passwordHandle);

        /* reset error and try next password */
        if (password != NULL)
        {
          error = ERROR_NONE;
        }
      }
      else
      {
        /* no more passwords when no encryption or asymmetric encryption is used */
        password = NULL;
      }
    }
  } /* while */

  if (!foundHardLinkEntryFlag || !foundHardLinkDataFlag)
  {
    /* free resources */
    Compress_delete(&archiveEntryInfo->hardLink.compressInfoData);
    free(archiveEntryInfo->hardLink.buffer);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);

    if      (error != ERROR_NONE)     return error;
    else if (!passwordFlag)           return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)          return ERROR_INVALID_PASSWORD;
    else if (!foundHardLinkEntryFlag) return ERROR_NO_FILE_ENTRY;
    else if (!foundHardLinkDataFlag)  return ERROR_NO_FILE_DATA;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (compressAlgorithm != NULL) (*compressAlgorithm) = archiveEntryInfo->hardLink.compressAlgorithm;
  if (cryptAlgorithm    != NULL) (*cryptAlgorithm)    = archiveEntryInfo->cryptAlgorithm;
  if (cryptType         != NULL) (*cryptType)         = archiveInfo->cryptType;

  /* reset compress, crypt */
  Compress_reset(&archiveEntryInfo->hardLink.compressInfoData);
  Crypt_reset(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,0);

  return ERROR_NONE;
}

Errors Archive_readSpecialEntry(ArchiveInfo      *archiveInfo,
                                ArchiveEntryInfo *archiveEntryInfo,
                                CryptAlgorithms  *cryptAlgorithm,
                                CryptTypes       *cryptType,
                                String           specialName,
                                FileInfo         *fileInfo
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
  assert(archiveEntryInfo != NULL);
  assert(fileInfo != NULL);

  /* check for pending error */
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  /* init archive file info */
  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_SPECIAL;

  /* init special chunk */
  error = Chunk_init(&archiveEntryInfo->special.chunkSpecial.info,
                     NULL,
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_SPECIAL,
                     CHUNK_DEFINITION_SPECIAL,
                     0,
                     NULL,
                     &archiveEntryInfo->special.chunkSpecial
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
      Chunk_done(&archiveEntryInfo->special.chunkSpecial.info);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_SPECIAL)
    {
      Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->special.chunkSpecial.info);
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_SPECIAL);

  /* read special chunk */
  error = Chunk_open(&archiveEntryInfo->special.chunkSpecial.info,
                     &chunkHeader,
                     Chunk_getSize(CHUNK_DEFINITION_SPECIAL,0,NULL)
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->special.chunkSpecial.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  archiveEntryInfo->cryptAlgorithm = archiveEntryInfo->special.chunkSpecial.cryptAlgorithm;

  /* detect block length of use crypt algorithm */
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->special.chunkSpecial.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  /* try to read special entry with all passwords */
  Chunk_tell(&archiveEntryInfo->special.chunkSpecial.info,&index);
  if (archiveEntryInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo->printableName,
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
  error                 = ERROR_NONE;
  while (   !foundSpecialEntryFlag
         && (error == ERROR_NONE)
        )
  {
    error = ERROR_NONE;

    /* reset chunk read position */
    Chunk_seek(&archiveEntryInfo->special.chunkSpecial.info,index);

    /* init crypt */
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
      }
    }

    /* init special entry chunk */
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->special.chunkSpecialEntry.info,
                         &archiveEntryInfo->special.chunkSpecial.info,
                         NULL,
                         NULL,
                         CHUNK_ID_SPECIAL_ENTRY,
                         CHUNK_DEFINITION_SPECIAL_ENTRY,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->special.chunkSpecialEntry.cryptInfo,
                         &archiveEntryInfo->special.chunkSpecialEntry
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo);
      }
    }

    /* read special entry */
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveEntryInfo->special.chunkSpecial.info)
             && (error == ERROR_NONE)
             && !foundSpecialEntryFlag
            )
      {
        error = Chunk_nextSub(&archiveEntryInfo->special.chunkSpecial.info,&subChunkHeader);
        if (error != ERROR_NONE)
        {
          break;
        }

        switch (subChunkHeader.id)
        {
          case CHUNK_ID_SPECIAL_ENTRY:
            /* read special entry chunk */
            error = Chunk_open(&archiveEntryInfo->special.chunkSpecialEntry.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            /* get special meta data */
            String_set(specialName,archiveEntryInfo->special.chunkSpecialEntry.name);
            fileInfo->type            = FILE_TYPE_SPECIAL;
            fileInfo->timeLastAccess  = archiveEntryInfo->special.chunkSpecialEntry.timeLastAccess;
            fileInfo->timeModified    = archiveEntryInfo->special.chunkSpecialEntry.timeModified;
            fileInfo->timeLastChanged = archiveEntryInfo->special.chunkSpecialEntry.timeLastChanged;
            fileInfo->userId          = archiveEntryInfo->special.chunkSpecialEntry.userId;
            fileInfo->groupId         = archiveEntryInfo->special.chunkSpecialEntry.groupId;
            fileInfo->permission      = archiveEntryInfo->special.chunkSpecialEntry.permission;
            fileInfo->specialType     = archiveEntryInfo->special.chunkSpecialEntry.specialType;
            fileInfo->major           = archiveEntryInfo->special.chunkSpecialEntry.major;
            fileInfo->minor           = archiveEntryInfo->special.chunkSpecialEntry.minor;

            foundSpecialEntryFlag = TRUE;
            break;
          default:
            error = Chunk_skipSub(&archiveEntryInfo->special.chunkSpecial.info,&subChunkHeader);
            if (error != ERROR_NONE)
            {
              break;
            }
            break;
        }
      }
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->special.chunkSpecialEntry.info);
        Crypt_done(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo);
      }
    }

    if (error != ERROR_NONE)
    {
      if (   (archiveEntryInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
          && (archiveInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        /* get next password */
        password = getNextDecryptPassword(&passwordHandle);

        /* reset error and try next password */
        if (password != NULL)
        {
          error = ERROR_NONE;
        }
      }
      else
      {
        /* no more passwords when no encryption or asymmetric encryption is used */
        password = NULL;
      }
    }
  } /* while */

  if (!foundSpecialEntryFlag)
  {
    Chunk_done(&archiveEntryInfo->special.chunkSpecial.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);

    if      (error != ERROR_NONE)    return error;
    else if (!passwordFlag)          return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)         return ERROR_INVALID_PASSWORD;
    else if (!foundSpecialEntryFlag) return ERROR_NO_SPECIAL_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveEntryInfo->cryptAlgorithm;
  if (cryptType      != NULL) (*cryptType)      = archiveInfo->cryptType;

  return ERROR_NONE;
}

Errors Archive_closeEntry(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors error,tmpError;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(archiveEntryInfo->archiveInfo->jobOptions != NULL);

  error = ERROR_NONE;
  switch (archiveEntryInfo->mode)
  {
    case ARCHIVE_MODE_WRITE:
      switch (archiveEntryInfo->archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          {
            if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
            {
              /* flush data */
              Compress_flush(&archiveEntryInfo->file.compressInfoData);
              if (   !archiveEntryInfo->file.createdFlag
                  || (Compress_getAvailableBlocks(&archiveEntryInfo->file.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
                 )
              {
                while (   (error == ERROR_NONE)
                       && (Compress_getAvailableBlocks(&archiveEntryInfo->file.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
                      )
                {
                  error = writeFileDataBlock(archiveEntryInfo,BLOCK_MODE_WRITE,FALSE);
                }
                if (error == ERROR_NONE)
                {
                  error = writeFileDataBlock(archiveEntryInfo,BLOCK_MODE_FLUSH,FALSE);
                }
              }

              /* update file and chunks if header is written */
              if (archiveEntryInfo->file.headerWrittenFlag)
              {
                /* update part size */
                archiveEntryInfo->file.chunkFileData.fragmentSize = Compress_getInputLength(&archiveEntryInfo->file.compressInfoData);
                if (error == ERROR_NONE)
                {
                  tmpError = Chunk_update(&archiveEntryInfo->file.chunkFileData.info);
                  if (tmpError != ERROR_NONE) error = tmpError;
                }

                /* close chunks */
                tmpError = Chunk_close(&archiveEntryInfo->file.chunkFileData.info);
                if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
                tmpError = Chunk_close(&archiveEntryInfo->file.chunkFileEntry.info);
                if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
                tmpError = Chunk_close(&archiveEntryInfo->file.chunkFile.info);
                if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              }

              if (   (archiveEntryInfo->archiveInfo->databaseHandle != NULL)
                  && !archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag
                  && !archiveEntryInfo->archiveInfo->jobOptions->noStorageFlag
                 )
              {
                /* store in index database */
                if (error == ERROR_NONE)
                {
                  error = Index_addFile(archiveEntryInfo->archiveInfo->databaseHandle,
                                        archiveEntryInfo->archiveInfo->storageId,
                                        archiveEntryInfo->file.chunkFileEntry.name,
                                        archiveEntryInfo->file.chunkFileEntry.size,
                                        archiveEntryInfo->file.chunkFileEntry.timeLastAccess,
                                        archiveEntryInfo->file.chunkFileEntry.timeModified,
                                        archiveEntryInfo->file.chunkFileEntry.timeLastChanged,
                                        archiveEntryInfo->file.chunkFileEntry.userId,
                                        archiveEntryInfo->file.chunkFileEntry.groupId,
                                        archiveEntryInfo->file.chunkFileEntry.permission,
                                        archiveEntryInfo->file.chunkFileData.fragmentOffset,
                                        archiveEntryInfo->file.chunkFileData.fragmentSize
                                       );
                }
              }
            }

            /* free resources */
            Crypt_done(&archiveEntryInfo->file.cryptInfoData);
            Compress_delete(&archiveEntryInfo->file.compressInfoData);
            Chunk_done(&archiveEntryInfo->file.chunkFileData.info);
            Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
            Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
            Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->file.chunkFile.info);
            free(archiveEntryInfo->file.buffer);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
            {
              /* flush data */
              Compress_flush(&archiveEntryInfo->image.compressInfoData);
              if (   !archiveEntryInfo->image.createdFlag
                  || (Compress_getAvailableBlocks(&archiveEntryInfo->image.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
                 )
              {
                while (   (error == ERROR_NONE)
                       && (Compress_getAvailableBlocks(&archiveEntryInfo->image.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
                      )
                {
                  error = writeImageDataBlock(archiveEntryInfo,BLOCK_MODE_WRITE,FALSE);
                }
                if (error == ERROR_NONE)
                {
                  error = writeImageDataBlock(archiveEntryInfo,BLOCK_MODE_FLUSH,FALSE);
                }
              }

              /* update file and chunks if header is written */
              if (archiveEntryInfo->image.headerWrittenFlag)
              {
                /* update part block count */
                assert(archiveEntryInfo->image.blockSize > 0);
                assert((Compress_getInputLength(&archiveEntryInfo->image.compressInfoData) % archiveEntryInfo->image.blockSize) == 0);
                archiveEntryInfo->image.chunkImageData.blockCount = Compress_getInputLength(&archiveEntryInfo->image.compressInfoData)/archiveEntryInfo->image.blockSize;
                if (error == ERROR_NONE)
                {
                  tmpError = Chunk_update(&archiveEntryInfo->image.chunkImageData.info);
                  if (tmpError != ERROR_NONE) error = tmpError;
                }

                /* close chunks */
                tmpError = Chunk_close(&archiveEntryInfo->image.chunkImageData.info);
                if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
                tmpError = Chunk_close(&archiveEntryInfo->image.chunkImageEntry.info);
                if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
                tmpError = Chunk_close(&archiveEntryInfo->image.chunkImage.info);
                if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              }

              if (   (archiveEntryInfo->archiveInfo->databaseHandle != NULL)
                  && !archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag
                  && !archiveEntryInfo->archiveInfo->jobOptions->noStorageFlag
                 )
              {
                /* store in index database */
                if (error == ERROR_NONE)
                {
                  error = Index_addImage(archiveEntryInfo->archiveInfo->databaseHandle,
                                         archiveEntryInfo->archiveInfo->storageId,
                                         archiveEntryInfo->image.chunkImageEntry.name,
                                         archiveEntryInfo->image.chunkImageEntry.size,
                                         archiveEntryInfo->image.chunkImageEntry.blockSize,
                                         archiveEntryInfo->image.chunkImageData.blockOffset,
                                         archiveEntryInfo->image.chunkImageData.blockCount
                                        );
                }
              }
            }

            /* free resources */
            Crypt_done(&archiveEntryInfo->image.cryptInfoData);
            Compress_delete(&archiveEntryInfo->image.compressInfoData);
            Chunk_done(&archiveEntryInfo->image.chunkImageData.info);
            Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
            Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
            Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->image.chunkImage.info);
            free(archiveEntryInfo->image.buffer);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          {
            if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
            {
              /* close chunks */
              tmpError = Chunk_close(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              tmpError = Chunk_close(&archiveEntryInfo->directory.chunkDirectory.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

              if (   (archiveEntryInfo->archiveInfo->databaseHandle != NULL)
                  && !archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag
                  && !archiveEntryInfo->archiveInfo->jobOptions->noStorageFlag
                 )
              {
                /* store in index database */
                if (error == ERROR_NONE)
                {
                  error = Index_addDirectory(archiveEntryInfo->archiveInfo->databaseHandle,
                                             archiveEntryInfo->archiveInfo->storageId,
                                             archiveEntryInfo->directory.chunkDirectoryEntry.name,
                                             archiveEntryInfo->directory.chunkDirectoryEntry.timeLastAccess,
                                             archiveEntryInfo->directory.chunkDirectoryEntry.timeModified,
                                             archiveEntryInfo->directory.chunkDirectoryEntry.timeLastChanged,
                                             archiveEntryInfo->directory.chunkDirectoryEntry.userId,
                                             archiveEntryInfo->directory.chunkDirectoryEntry.groupId,
                                             archiveEntryInfo->directory.chunkDirectoryEntry.permission
                                            );
                }
              }
            }

            /* free resources */
            Chunk_done(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
            Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          {
            if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
            {
              /* close chunks */
              tmpError = Chunk_close(&archiveEntryInfo->link.chunkLinkEntry.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              tmpError = Chunk_close(&archiveEntryInfo->link.chunkLink.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

              if (   (archiveEntryInfo->archiveInfo->databaseHandle != NULL)
                  && !archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag
                  && !archiveEntryInfo->archiveInfo->jobOptions->noStorageFlag
                 )
              {
                /* store in index database */
                if (error == ERROR_NONE)
                {
                  error = Index_addLink(archiveEntryInfo->archiveInfo->databaseHandle,
                                        archiveEntryInfo->archiveInfo->storageId,
                                        archiveEntryInfo->link.chunkLinkEntry.name,
                                        archiveEntryInfo->link.chunkLinkEntry.destinationName,
                                        archiveEntryInfo->link.chunkLinkEntry.timeLastAccess,
                                        archiveEntryInfo->link.chunkLinkEntry.timeModified,
                                        archiveEntryInfo->link.chunkLinkEntry.timeLastChanged,
                                        archiveEntryInfo->link.chunkLinkEntry.userId,
                                        archiveEntryInfo->link.chunkLinkEntry.groupId,
                                        archiveEntryInfo->link.chunkLinkEntry.permission
                                       );
                }
              }
            }

            /* free resources */
            Chunk_done(&archiveEntryInfo->link.chunkLinkEntry.info);
            Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->link.chunkLink.info);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          {
            ChunkHardLinkName *chunkHardLinkName;

            if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
            {
              /* flush data */
              Compress_flush(&archiveEntryInfo->hardLink.compressInfoData);
              if (   !archiveEntryInfo->hardLink.createdFlag
                  || (Compress_getAvailableBlocks(&archiveEntryInfo->hardLink.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
                 )
              {
                while (   (error == ERROR_NONE)
                       && (Compress_getAvailableBlocks(&archiveEntryInfo->hardLink.compressInfoData,COMPRESS_BLOCK_TYPE_ANY) > 0)
                      )
                {
                  error = writeHardLinkDataBlock(archiveEntryInfo,BLOCK_MODE_WRITE,FALSE);
                }
                if (error == ERROR_NONE)
                {
                  error = writeHardLinkDataBlock(archiveEntryInfo,BLOCK_MODE_FLUSH,FALSE);
                }
              }

              /* update file and chunks if header is written */
              if (archiveEntryInfo->hardLink.headerWrittenFlag)
              {
                /* update part size */
                archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize = Compress_getInputLength(&archiveEntryInfo->hardLink.compressInfoData);
                if (error == ERROR_NONE)
                {
                  tmpError = Chunk_update(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
                  if (tmpError != ERROR_NONE) error = tmpError;
                }

                /* close chunks */
                tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
                if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
                LIST_ITERATE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
                {
                  tmpError = Chunk_close(&chunkHardLinkName->info);
                  if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
                }
                tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
                if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
                tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLink.info);
                if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              }

              if (   (archiveEntryInfo->archiveInfo->databaseHandle != NULL)
                  && !archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag
                  && !archiveEntryInfo->archiveInfo->jobOptions->noStorageFlag
                 )
              {
                /* store in index database */
                if (error == ERROR_NONE)
                {
                  LIST_ITERATE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
                  {
                    error = Index_addFile(archiveEntryInfo->archiveInfo->databaseHandle,
                                          archiveEntryInfo->archiveInfo->storageId,
                                          chunkHardLinkName->name,
                                          archiveEntryInfo->hardLink.chunkHardLinkEntry.size,
                                          archiveEntryInfo->hardLink.chunkHardLinkEntry.timeLastAccess,
                                          archiveEntryInfo->hardLink.chunkHardLinkEntry.timeModified,
                                          archiveEntryInfo->hardLink.chunkHardLinkEntry.timeLastChanged,
                                          archiveEntryInfo->hardLink.chunkHardLinkEntry.userId,
                                          archiveEntryInfo->hardLink.chunkHardLinkEntry.groupId,
                                          archiveEntryInfo->hardLink.chunkHardLinkEntry.permission,
                                          archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset,
                                          archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize
                                         );
                    if (error != ERROR_NONE) break;
                  }
                }
              }
            }

            /* free resources */
            Crypt_done(&archiveEntryInfo->hardLink.cryptInfoData);
            Compress_delete(&archiveEntryInfo->hardLink.compressInfoData);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
            LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
            {
              Chunk_done(&chunkHardLinkName->info);
              Crypt_done(&chunkHardLinkName->cryptInfo);
            }
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
            free(archiveEntryInfo->hardLink.buffer);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          {
            if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
            {
              /* close chunks */
              tmpError = Chunk_close(&archiveEntryInfo->special.chunkSpecialEntry.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              tmpError = Chunk_close(&archiveEntryInfo->special.chunkSpecial.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

              if (   (archiveEntryInfo->archiveInfo->databaseHandle != NULL)
                  && !archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag
                  && !archiveEntryInfo->archiveInfo->jobOptions->noStorageFlag
                 )
              {
                /* store in index database */
                if (error == ERROR_NONE)
                {
                  error = Index_addSpecial(archiveEntryInfo->archiveInfo->databaseHandle,
                                           archiveEntryInfo->archiveInfo->storageId,
                                           archiveEntryInfo->special.chunkSpecialEntry.name,
                                           archiveEntryInfo->special.chunkSpecialEntry.specialType,
                                           archiveEntryInfo->special.chunkSpecialEntry.timeLastAccess,
                                           archiveEntryInfo->special.chunkSpecialEntry.timeModified,
                                           archiveEntryInfo->special.chunkSpecialEntry.timeLastChanged,
                                           archiveEntryInfo->special.chunkSpecialEntry.userId,
                                           archiveEntryInfo->special.chunkSpecialEntry.groupId,
                                           archiveEntryInfo->special.chunkSpecialEntry.permission,
                                           archiveEntryInfo->special.chunkSpecialEntry.major,
                                           archiveEntryInfo->special.chunkSpecialEntry.minor
                                          );
                }
              }
            }

            /* free resources */
            Chunk_done(&archiveEntryInfo->special.chunkSpecialEntry.info);
            Crypt_done(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->special.chunkSpecial.info);
          }
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break; /* not reached */
      }
      break;
    case ARCHIVE_MODE_READ:
      switch (archiveEntryInfo->archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          {
            /* close chunks */
            tmpError = Chunk_close(&archiveEntryInfo->file.chunkFileData.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->file.chunkFileEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->file.chunkFile.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            /* free resources */
            Chunk_done(&archiveEntryInfo->file.chunkFileData.info);
            Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
            Compress_delete(&archiveEntryInfo->file.compressInfoData);
            Crypt_done(&archiveEntryInfo->file.cryptInfoData);
            Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
            Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->file.chunkFile.info);
            free(archiveEntryInfo->file.buffer);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            /* close chunks */
            tmpError = Chunk_close(&archiveEntryInfo->image.chunkImageData.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->image.chunkImageEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->image.chunkImage.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            /* free resources */
            Crypt_done(&archiveEntryInfo->image.cryptInfoData);
            Compress_delete(&archiveEntryInfo->image.compressInfoData);
            Chunk_done(&archiveEntryInfo->image.chunkImageData.info);
            Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
            Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
            Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->image.chunkImage.info);
            free(archiveEntryInfo->image.buffer);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          {
            /* close chunks */
            tmpError = Chunk_close(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->directory.chunkDirectory.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            /* free resources */
            Chunk_done(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
            Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          {
            /* close chunks */
            tmpError = Chunk_close(&archiveEntryInfo->link.chunkLinkEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->link.chunkLink.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            /* free resources */
            Chunk_done(&archiveEntryInfo->link.chunkLinkEntry.info);
            Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->link.chunkLink.info);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          {
            ChunkHardLinkName *chunkHardLinkName;

            /* close chunks */
            tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            LIST_ITERATE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
            {
              tmpError = Chunk_close(&chunkHardLinkName->info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            }
            tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLink.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            /* free resources */
            Crypt_done(&archiveEntryInfo->hardLink.cryptInfoData);
            Compress_delete(&archiveEntryInfo->hardLink.compressInfoData);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
            LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
            {
              Chunk_done(&chunkHardLinkName->info);
              Crypt_done(&chunkHardLinkName->cryptInfo);
            }
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
            free(archiveEntryInfo->hardLink.buffer);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          {
            /* close chunks */
            tmpError = Chunk_close(&archiveEntryInfo->special.chunkSpecialEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->special.chunkSpecial.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            /* free resources */
            Chunk_done(&archiveEntryInfo->special.chunkSpecialEntry.info);
            Crypt_done(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->special.chunkSpecial.info);
          }
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break; /* not reached */
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return error;
}

Errors Archive_writeData(ArchiveEntryInfo *archiveEntryInfo,
                         const void       *buffer,
                         ulong            length,
                         uint             elementSize
                        )
{
  byte   *p;
  ulong  writtenBytes;
  Errors error;
  ulong  blockLength;
  ulong  writtenBlockBytes;
  ulong  deflatedBytes;
  bool   allowNewPartFlag;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);

  if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
  {
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
      switch (archiveEntryInfo->archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          assert((archiveEntryInfo->file.bufferLength%archiveEntryInfo->blockLength) == 0);

          while (writtenBlockBytes < blockLength)
          {
            /* compress */
            error = Compress_deflate(&archiveEntryInfo->file.compressInfoData,
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
/* ???
fprintf(stderr,"%s,%d: avild =%d\n",__FILE__,__LINE__,
Compress_getAvailableBlocks(&archiveEntryInfo->file.compressInfoData,
                                               COMPRESS_BLOCK_TYPE_FULL
                                              ));
*/
            while (Compress_getAvailableBlocks(&archiveEntryInfo->file.compressInfoData,
                                               COMPRESS_BLOCK_TYPE_FULL
                                              ) > 0
                  )
            {
              error = writeFileDataBlock(archiveEntryInfo,BLOCK_MODE_WRITE,allowNewPartFlag);
              if (error != ERROR_NONE)
              {
                return error;
              }
            }
            assert(Compress_getAvailableBlocks(&archiveEntryInfo->file.compressInfoData,COMPRESS_BLOCK_TYPE_FULL) == 0);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          assert((archiveEntryInfo->image.bufferLength%archiveEntryInfo->blockLength) == 0);

          while (writtenBlockBytes < blockLength)
          {
            /* compress */
            error = Compress_deflate(&archiveEntryInfo->image.compressInfoData,
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
            while (Compress_getAvailableBlocks(&archiveEntryInfo->image.compressInfoData,
                                               COMPRESS_BLOCK_TYPE_FULL
                                              ) > 0
                  )
            {
              error = writeImageDataBlock(archiveEntryInfo,BLOCK_MODE_WRITE,allowNewPartFlag);
              if (error != ERROR_NONE)
              {
                return error;
              }
            }
            assert(Compress_getAvailableBlocks(&archiveEntryInfo->image.compressInfoData,COMPRESS_BLOCK_TYPE_FULL) == 0);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          assert((archiveEntryInfo->hardLink.bufferLength%archiveEntryInfo->blockLength) == 0);

          while (writtenBlockBytes < blockLength)
          {
            /* compress */
            error = Compress_deflate(&archiveEntryInfo->hardLink.compressInfoData,
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
            while (Compress_getAvailableBlocks(&archiveEntryInfo->hardLink.compressInfoData,
                                               COMPRESS_BLOCK_TYPE_FULL
                                              ) > 0
                  )
            {
              error = writeHardLinkDataBlock(archiveEntryInfo,BLOCK_MODE_WRITE,allowNewPartFlag);
              if (error != ERROR_NONE)
              {
                return error;
              }
            }
            assert(Compress_getAvailableBlocks(&archiveEntryInfo->hardLink.compressInfoData,COMPRESS_BLOCK_TYPE_FULL) == 0);
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
  }

  return ERROR_NONE;
}

Errors Archive_readData(ArchiveEntryInfo *archiveEntryInfo,
                        void             *buffer,
                        ulong            length
                       )
{
  byte   *p;
  Errors error;
  ulong  n;
  ulong  inflatedBytes;

  assert(archiveEntryInfo != NULL);

  switch (archiveEntryInfo->archiveEntryType)
  {
    case ARCHIVE_ENTRY_TYPE_FILE:
      p = (byte*)buffer;
      while (length > 0)
      {
        /* fill decompressor with compressed data blocks */
        do
        {
          n = Compress_getAvailableBytes(&archiveEntryInfo->file.compressInfoData);
          if (n <= 0)
          {
            error = readFileDataBlock(archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
        }
        while (n <= 0);

        /* decompress data */
        error = Compress_inflate(&archiveEntryInfo->file.compressInfoData,p,1,&inflatedBytes);
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
          n = Compress_getAvailableBytes(&archiveEntryInfo->image.compressInfoData);
          if (n <= 0)
          {
            error = readImageDataBlock(archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
        }
        while (n <= 0);

        /* decompress data */
        error = Compress_inflate(&archiveEntryInfo->image.compressInfoData,p,1,&inflatedBytes);
        if (error != ERROR_NONE)
        {
          return error;
        }
        p += inflatedBytes;
        length -= inflatedBytes;
      }
      break;
    case ARCHIVE_ENTRY_TYPE_HARDLINK:
      p = (byte*)buffer;
      while (length > 0)
      {
        /* fill decompressor with compressed data blocks */
        do
        {
          n = Compress_getAvailableBytes(&archiveEntryInfo->hardLink.compressInfoData);
          if (n <= 0)
          {
            error = readHardLinkDataBlock(archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
        }
        while (n <= 0);

        /* decompress data */
        error = Compress_inflate(&archiveEntryInfo->hardLink.compressInfoData,p,1,&inflatedBytes);
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

bool Archive_eofData(ArchiveEntryInfo *archiveEntryInfo)
{
  bool eofFlag;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);

  eofFlag = FALSE;
  switch (archiveEntryInfo->mode)
  {
    case ARCHIVE_MODE_WRITE:
      eofFlag = TRUE;
      break;
    case ARCHIVE_MODE_READ:
      switch (archiveEntryInfo->archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          eofFlag = Chunk_eofSub(&archiveEntryInfo->file.chunkFile.info);
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          eofFlag = Chunk_eofSub(&archiveEntryInfo->image.chunkImage.info);
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          eofFlag = Chunk_eofSub(&archiveEntryInfo->directory.chunkDirectory.info);
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          eofFlag = Chunk_eofSub(&archiveEntryInfo->link.chunkLink.info);
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          eofFlag = Chunk_eofSub(&archiveEntryInfo->hardLink.chunkHardLink.info);
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          eofFlag = Chunk_eofSub(&archiveEntryInfo->special.chunkSpecial.info);
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break; /* not reached */
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return eofFlag;
}

uint64 Archive_tell(ArchiveInfo *archiveInfo)
{
  uint64 index;

  assert(archiveInfo != NULL);

  index = 0LL;
  if (!archiveInfo->jobOptions->dryRunFlag)
  {
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
  }

  return index;
}

uint64 Archive_getSize(ArchiveInfo *archiveInfo)
{
  uint64 size;

  assert(archiveInfo != NULL);

  size = 0LL;
  if (!archiveInfo->jobOptions->dryRunFlag)
  {
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
  }

  return size;
}

Errors Archive_addIndex(DatabaseHandle *databaseHandle,
                        const String   storageName,
                        IndexModes     indexMode,
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
                       indexMode,
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
  ArchiveEntryInfo  archiveEntryInfo;
  ArchiveEntryTypes archiveEntryType;

  assert(databaseHandle != NULL);
  assert(storageName != NULL);

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
    Index_setState(databaseHandle,
                   storageId,
                   INDEX_STATE_ERROR,
                   0LL,
                   "%s (error code: %d)",
                   Errors_getText(error),
                   Errors_getCode(error)
                  );
    freeJobOptions(&jobOptions);
    return error;
  }

  /* clear index */
  error = Index_clear(databaseHandle,
                      storageId
                     );
  if (error != ERROR_NONE)
  {
    Archive_close(&archiveInfo);
    Index_setState(databaseHandle,
                   storageId,
                   INDEX_STATE_ERROR,
                   0LL,
                   "%s (error code: %d)",
                   Errors_getText(error),
                   Errors_getCode(error)
                  );
    freeJobOptions(&jobOptions);
    return error;
  }

  /* index archive contents */
  Index_setState(databaseHandle,
                 storageId,
                 INDEX_STATE_UPDATE,
                 0LL,
                 NULL
                );
  error = ERROR_NONE;
  while (   !Archive_eof(&archiveInfo,FALSE)
         && ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
         && (error == ERROR_NONE)
        )
  {
    /* pause */
    while ((pauseFlag != NULL) && (*pauseFlag))
    {
      Misc_udelay(5000*1000);
    }

    /* get next file type */
    error = Archive_getNextArchiveEntryType(&archiveInfo,
                                            &archiveEntryType,
                                            FALSE
                                           );
    if (error == ERROR_NONE)
    {
      /* read entry */
      switch (archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          {
            String             fileName;
            ArchiveEntryInfo   archiveEntryInfo;
            CompressAlgorithms compressAlgorithm;
            CryptAlgorithms    cryptAlgorithm;
            CryptTypes         cryptType;
            FileInfo           fileInfo;
            uint64             fragmentOffset,fragmentSize;

            /* open archive file */
            fileName = String_new();
            error = Archive_readFileEntry(&archiveInfo,
                                          &archiveEntryInfo,
                                          &compressAlgorithm,
                                          &cryptAlgorithm,
                                          &cryptType,
                                          fileName,
                                          &fileInfo,
                                          &fragmentOffset,
                                          &fragmentSize
                                         );
            if (error != ERROR_NONE)
            {
              String_delete(fileName);
              break;
            }

            /* add to index database */
            error = Index_addFile(databaseHandle,
                                  storageId,
                                  fileName,
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
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              break;
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(fileName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            String             deviceName;
            ArchiveEntryInfo   archiveEntryInfo;
            CompressAlgorithms compressAlgorithm;
            CryptAlgorithms    cryptAlgorithm;
            CryptTypes         cryptType;
            DeviceInfo         deviceInfo;
            uint64             blockOffset,blockCount;

            /* open archive file */
            deviceName = String_new();
            error = Archive_readImageEntry(&archiveInfo,
                                           &archiveEntryInfo,
                                           &compressAlgorithm,
                                           &cryptAlgorithm,
                                           &cryptType,
                                           deviceName,
                                           &deviceInfo,
                                           &blockOffset,
                                           &blockCount
                                          );
            if (error != ERROR_NONE)
            {
              String_delete(deviceName);
              break;
            }

            /* add to index database */
            error = Index_addImage(databaseHandle,
                                   storageId,
                                   deviceName,
                                   deviceInfo.size,
                                   deviceInfo.blockSize,
                                   blockOffset,
                                   blockCount
                                  );
            if (error != ERROR_NONE)
            {
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(deviceName);
              break;
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(deviceName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          {
            String          directoryName;
            CryptAlgorithms cryptAlgorithm;
            CryptTypes      cryptType;
            FileInfo        fileInfo;

            /* open archive directory */
            directoryName = String_new();
            error = Archive_readDirectoryEntry(&archiveInfo,
                                               &archiveEntryInfo,
                                               &cryptAlgorithm,
                                               &cryptType,
                                               directoryName,
                                               &fileInfo
                                              );
            if (error != ERROR_NONE)
            {
              String_delete(directoryName);
              break;
            }

            /* add to index database */
            error = Index_addDirectory(databaseHandle,
                                       storageId,
                                       directoryName,
                                       fileInfo.timeLastAccess,
                                       fileInfo.timeModified,
                                       fileInfo.timeLastChanged,
                                       fileInfo.userId,
                                       fileInfo.groupId,
                                       fileInfo.permission
                                      );
            if (error != ERROR_NONE)
            {
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(directoryName);
              break;
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(directoryName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          {
            String          linkName;
            String          destinationName;
            CryptAlgorithms cryptAlgorithm;
            CryptTypes      cryptType;
            FileInfo        fileInfo;

            /* open archive link */
            linkName        = String_new();
            destinationName = String_new();
            error = Archive_readLinkEntry(&archiveInfo,
                                          &archiveEntryInfo,
                                          &cryptAlgorithm,
                                          &cryptType,
                                          linkName,
                                          destinationName,
                                          &fileInfo
                                         );
            if (error != ERROR_NONE)
            {
              String_delete(destinationName);
              String_delete(linkName);
              break;
            }

            /* add to index database */
            error = Index_addLink(databaseHandle,
                                  storageId,
                                  linkName,
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
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(destinationName);
              String_delete(linkName);
              break;
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(destinationName);
            String_delete(linkName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          {
            StringList         fileNameList;
            ArchiveEntryInfo   archiveEntryInfo;
            CompressAlgorithms compressAlgorithm;
            CryptAlgorithms    cryptAlgorithm;
            CryptTypes         cryptType;
            FileInfo           fileInfo;
            uint64             fragmentOffset,fragmentSize;
            const StringNode   *stringNode;
            String             fileName;

            /* open archive file */
            StringList_init(&fileNameList);
            error = Archive_readHardLinkEntry(&archiveInfo,
                                          &archiveEntryInfo,
                                          &compressAlgorithm,
                                          &cryptAlgorithm,
                                          &cryptType,
                                          &fileNameList,
                                          &fileInfo,
                                          &fragmentOffset,
                                          &fragmentSize
                                         );
            if (error != ERROR_NONE)
            {
              StringList_done(&fileNameList);
              break;
            }
//fprintf(stderr,"%s,%d: index update %s\n",__FILE__,__LINE__,String_cString(name));

            /* add to index database */
            STRINGLIST_ITERATE(&fileNameList,stringNode,fileName)
            {
              error = Index_addHardLink(databaseHandle,
                                        storageId,
                                        fileName,
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
                break;
              }
            }
            if (error != ERROR_NONE)
            {
              Archive_closeEntry(&archiveEntryInfo);
              StringList_done(&fileNameList);
              break;
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveEntryInfo);
            StringList_done(&fileNameList);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          {
            String          fileName;
            CryptAlgorithms cryptAlgorithm;
            CryptTypes      cryptType;
            FileInfo        fileInfo;

            /* open archive link */
            fileName = String_new();
            error = Archive_readSpecialEntry(&archiveInfo,
                                             &archiveEntryInfo,
                                             &cryptAlgorithm,
                                             &cryptType,
                                             fileName,
                                             &fileInfo
                                            );
            if (error != ERROR_NONE)
            {
              String_delete(fileName);
              break;
            }

            /* add to index database */
            error = Index_addSpecial(databaseHandle,
                                     storageId,
                                     fileName,
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
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              break;
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(fileName);
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
                   Misc_getCurrentDateTime(),
                   NULL
                  );
  }
  else
  {
    Index_setState(databaseHandle,
                   storageId,
                   INDEX_STATE_ERROR,
                   0LL,
                   "%s (error code: %d)",
                   Errors_getText(error),
                   Errors_getCode(error)
                  );
  }

  /* update name/size */
  error = Index_update(databaseHandle,
                       storageId,
                       storageName,
                       Archive_getSize(&archiveInfo)
                      );
  if (error != ERROR_NONE)
  {
    Archive_close(&archiveInfo);
    Index_setState(databaseHandle,
                   storageId,
                   INDEX_STATE_ERROR,
                   0LL,
                   "%s (error code: %d)",
                   Errors_getText(error),
                   Errors_getCode(error)
                  );
    freeJobOptions(&jobOptions);
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
