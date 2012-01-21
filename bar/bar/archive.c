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


#include <sys/types.h>
       #include <sys/stat.h>
       #include <fcntl.h>
       #include <errno.h>

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// size of buffer for reading/writing file data
#define MAX_BUFFER_SIZE (64*1024)

// i/o via file functions
const ChunkIO CHUNK_IO_FILE =
{
  (bool(*)(void*))File_eof,
  (Errors(*)(void*,void*,ulong,ulong*))File_read,
  (Errors(*)(void*,const void*,ulong))File_write,
  (Errors(*)(void*,uint64*))File_tell,
  (Errors(*)(void*,uint64))File_seek,
  (uint64(*)(void*))File_getSize
};

// i/o via storage file functions
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

// block modes
typedef enum
{
  BLOCK_MODE_WRITE,            // write block
  BLOCK_MODE_FLUSH,            // flush compress data buffers and read/write block
} BlockModes;

// decrypt password list
typedef struct PasswordNode
{
  LIST_NODE_HEADER(struct PasswordNode);

  Password *password;
} PasswordNode;

typedef struct
{
  LIST_HEADER(PasswordNode);
} PasswordList;

// password handle
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

// list with all known decryption passwords
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
    // next password from list
    password = passwordHandle->passwordNode->password;
    passwordHandle->passwordNode = passwordHandle->passwordNode->next;
  }
  else if (   (passwordHandle->passwordMode == PASSWORD_MODE_CONFIG)
           && (passwordHandle->cryptPassword != NULL)
          )
  {
    // get password
    password = (Password*)passwordHandle->cryptPassword;

    // next password is: default
    passwordHandle->passwordMode = PASSWORD_MODE_DEFAULT;
  }
  else if (   (passwordHandle->passwordMode == PASSWORD_MODE_DEFAULT)
           && (globalOptions.cryptPassword != NULL)
          )
  {
    // get password
    password = globalOptions.cryptPassword;

    // next password is: ask
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
      // input password
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

      // add to password list
      password = Archive_appendDecryptPassword(&newPassword);

      // free resources
      Password_done(&newPassword);

      // next password is: none
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
* Name   : chunkHeaderEOF
* Purpose: check if chunk header end-of-file
* Input  : archiveInfo - archive info block
* Output : -
* Return : TRUE iff no more chunk header, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool chunkHeaderEOF(ArchiveInfo *archiveInfo)
{
  assert(archiveInfo != NULL);

  return    !archiveInfo->nextChunkHeaderReadFlag
         && Chunk_eof(archiveInfo->chunkIO,archiveInfo->chunkIOUserData);
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
      // get file size
      fileSize = archiveInfo->chunkIO->getSize(archiveInfo->chunkIOUserData);

      if      (   !headerWrittenFlag
               && (fileSize+headerLength >= archiveInfo->jobOptions->archivePartSize)
              )
      {
        // file header cannot be written without fragmentation -> new part
        newPartFlag = TRUE;
      }
      else if ((fileSize+minBytes) >= archiveInfo->jobOptions->archivePartSize)
      {
        // less than min. number of bytes left in part -> create new part
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

  // create key chunk
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

  // read key chunk
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

  // check if all data read
  if (!Chunk_eofSub(&chunkInfoKey))
  {
    free(archiveInfo->cryptKeyData); archiveInfo->cryptKeyData = NULL;
    Chunk_close(&chunkInfoKey);
    Chunk_done(&chunkInfoKey);
    return ERROR_CORRUPT_DATA;
  }

  // close chunk
  error = Chunk_close(&chunkInfoKey);
  if (error != ERROR_NONE)
  {
    free(archiveInfo->cryptKeyData); archiveInfo->cryptKeyData = NULL;
    Chunk_done(&chunkInfoKey);
    return error;
  }
  Chunk_done(&chunkInfoKey);

  // decrypt key
  if (archiveInfo->cryptPassword == NULL)
  {
    archiveInfo->cryptPassword = Password_new();
    if (archiveInfo->cryptPassword == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
  }
  error = Crypt_getDecryptKey(&archiveInfo->cryptKey,
                              archiveInfo->cryptKeyData,
                              archiveInfo->cryptKeyDataLength,
                              archiveInfo->cryptPassword
                             );
  if (error != ERROR_NONE)
  {
    Password_delete(archiveInfo->cryptPassword); archiveInfo->cryptPassword = NULL;
    free(archiveInfo->cryptKeyData); archiveInfo->cryptKeyData = NULL;
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeFileInfo
* Purpose: write archive info chunk
* Input  : archiveInfo - archive info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeFileInfo(ArchiveInfo *archiveInfo)
{
  Errors    error;
  ChunkInfo chunkInfoBar;
  ChunkKey  chunkBar;

  assert(archiveInfo != NULL);

  // create key chunk
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

  // write header
  error = Chunk_create(&chunkInfoBar);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkInfoBar);
    return error;
  }

  // free resources
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
* Purpose: write new encryption key chunk
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

  // create key chunk
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

  // write encrypted encryption key
  error = Chunk_create(&chunkInfoKey);
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

  // free resources
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

  // get output filename
  error = File_getTmpFileName(archiveInfo->file.fileName,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // create file
  error = File_open(&archiveInfo->file.fileHandle,
                    archiveInfo->file.fileName,
                    FILE_OPEN_CREATE
                   );
  if (error != ERROR_NONE)
  {
    File_delete(archiveInfo->file.fileName,FALSE);
    return error;
  }

  // write BAR file info
  error = writeFileInfo(archiveInfo);
  if (error != ERROR_NONE)
  {
    File_close(&archiveInfo->file.fileHandle);
    File_delete(archiveInfo->file.fileName,FALSE);
    return error;
  }

  // write encrypted key if asymmetric encryption enabled
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

  // init index
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

  // mark archive file "open"
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

  // get size of archive
  fileSize = archiveInfo->chunkIO->getSize(archiveInfo->chunkIOUserData);

  if (archiveInfo->file.openFlag)
  {
    // close file
    File_close(&archiveInfo->file.fileHandle);
    archiveInfo->file.openFlag = FALSE;
  }

  // call back new archive created
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

  // increament part number
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

  // check if split necessary
  if (checkNewPartNeeded(archiveInfo,
                         0,
                         FALSE,
                         minBytes
                        )
     )
  {
    // split needed -> close archive file
    closeArchiveFile(archiveInfo,FALSE);
  }

  // create new file if needed
  if (!archiveInfo->file.openFlag)
  {
    // create file
    error = createArchiveFile(archiveInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeFileChunks
* Purpose: write file chunks
* Input  : archiveEntryInfo  - archive file entry info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeFileChunks(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors error;

  // open archive file if needed
  if (!archiveEntryInfo->archiveInfo->file.openFlag)
  {
    // create archive file
    error = createArchiveFile(archiveEntryInfo->archiveInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // initialise variables
    archiveEntryInfo->file.headerWrittenFlag = FALSE;

    archiveEntryInfo->file.chunkFileData.fragmentOffset = archiveEntryInfo->file.chunkFileData.fragmentOffset+archiveEntryInfo->file.chunkFileData.fragmentSize;
    archiveEntryInfo->file.chunkFileData.fragmentSize   = 0LL;

    // reset data crypt
    Crypt_reset(&archiveEntryInfo->file.cryptInfo,0);
  }

  // create file chunk
  error = Chunk_create(&archiveEntryInfo->file.chunkFile.info);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // create file entry chunk
  error = Chunk_create(&archiveEntryInfo->file.chunkFileEntry.info);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // create file delta chunk
  if (Compress_isCompressed(archiveEntryInfo->file.deltaCompressAlgorithm))
  {
    error = Chunk_create(&archiveEntryInfo->file.chunkFileDelta.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // create file data chunk
  error = Chunk_create(&archiveEntryInfo->file.chunkFileData.info);
  if (error != ERROR_NONE)
  {
    return error;
  }

  archiveEntryInfo->file.headerWrittenFlag = TRUE;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : flushFileDataBlocks
* Purpose: flush file data blocks
* Input  : archiveEntryInfo  - archive file entry info block
*          compressBlockType - compress block type; see CompressBlockTypes
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors flushFileDataBlocks(ArchiveEntryInfo   *archiveEntryInfo,
                                 CompressBlockTypes compressBlockType
                                )
{
  Errors error;
  uint   blockCount;
  ulong  length;

  // create new part (if not already exists)
  if (!archiveEntryInfo->file.headerWrittenFlag)
  {
    writeFileChunks(archiveEntryInfo);
  }

  // flush data
  do
  {
    error = Compress_getAvailableCompressedBlocks(&archiveEntryInfo->file.byteCompressInfo,
                                                  compressBlockType,
                                                  &blockCount
                                                 );
    if (error != ERROR_NONE)
    {
      return error;
    }

    if (blockCount > 0)
    {
      // get next compressed data-block
      Compress_getCompressedBlock(&archiveEntryInfo->file.byteCompressInfo,
                                  archiveEntryInfo->file.buffer,
                                  &length
                                 );
      assert(length <= archiveEntryInfo->blockLength);
#if 0
{
int h = open("encoded.testdata",O_CREAT|O_WRONLY|O_APPEND,0664);
fprintf(stderr,"%s, %d: length=%d %d %s\n",__FILE__,__LINE__,length,write(h,archiveEntryInfo->file.buffer,length),strerror(errno));
close(h);
}
#endif /* 0 */

      // encrypt block
      error = Crypt_encrypt(&archiveEntryInfo->file.cryptInfo,
                            archiveEntryInfo->file.buffer,
                            archiveEntryInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // store block into chunk
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
  while (blockCount > 0);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeFileDataBlock
* Purpose: encrypt and split file, write file data chunk
* Input  : archiveEntryInfo  - archive file entry info block
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
  bool   eofDelta;
  byte   deltaBuffer[1];
  ulong  deltaLength;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(archiveEntryInfo->archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);

  // get next compressed data-block
  Compress_getCompressedBlock(&archiveEntryInfo->file.byteCompressInfo,
                              archiveEntryInfo->file.buffer,
                              &length
                             );
  assert(length <= archiveEntryInfo->blockLength);
#if 0
{
int h = open("encoded.testdata",O_CREAT|O_WRONLY|O_APPEND,0664);
fprintf(stderr,"%s, %d: length=%d %d %s\n",__FILE__,__LINE__,length,write(h,archiveEntryInfo->file.buffer,length),strerror(errno));
close(h);
}
#endif /* 0 */

  // check if split is allowed and necessary
  newPartFlag =    allowNewPartFlag
                && checkNewPartNeeded(archiveEntryInfo->archiveInfo,
                                      archiveEntryInfo->file.headerLength,
                                      archiveEntryInfo->file.headerWrittenFlag,
                                      ((length > 0) || (blockMode == BLOCK_MODE_WRITE))
                                        ?archiveEntryInfo->archiveInfo->blockLength
                                        :0
                                     );

  // split
  if (newPartFlag)
  {
    // create new part
    if (!archiveEntryInfo->file.headerWrittenFlag)
    {
      writeFileChunks(archiveEntryInfo);
    }

    // write last compressed block (if any)
    if (length > 0L)
    {
      // encrypt block
      error = Crypt_encrypt(&archiveEntryInfo->file.cryptInfo,
                            archiveEntryInfo->file.buffer,
                            archiveEntryInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // store block into chunk
      error = Chunk_writeData(&archiveEntryInfo->file.chunkFileData.info,
                              archiveEntryInfo->file.buffer,
                              archiveEntryInfo->blockLength
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    // flush delta compress
    error = Compress_flush(&archiveEntryInfo->file.deltaCompressInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
    eofDelta = FALSE;
    do
    {
      // flush compressed full data blocks
      error = flushFileDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_FULL);
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get next delta-compressed byte
      Compress_getCompressedBlock(&archiveEntryInfo->file.deltaCompressInfo,
                                  deltaBuffer,
                                  &deltaLength
                                 );
      assert(deltaLength <= 1);
      if (deltaLength > 0)
      {
        // compress data
        error = Compress_deflate(&archiveEntryInfo->file.byteCompressInfo,
                                 deltaBuffer,
                                 1,
                                 NULL
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      else
      {
        eofDelta = TRUE;
      }
    }
    while (!eofDelta);

    // flush data compress
    error = Compress_flush(&archiveEntryInfo->file.byteCompressInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = flushFileDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_ANY);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // update part size
    archiveEntryInfo->file.chunkFileData.fragmentSize = Compress_getInputLength(&archiveEntryInfo->file.deltaCompressInfo);
    error = Chunk_update(&archiveEntryInfo->file.chunkFileData.info);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // close chunks
    error = Chunk_close(&archiveEntryInfo->file.chunkFileData.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (Compress_isCompressed(archiveEntryInfo->file.deltaCompressAlgorithm))
    {
      error = Chunk_close(&archiveEntryInfo->file.chunkFileDelta.info);
      if (error != ERROR_NONE)
      {
        return error;
      }
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

    // close archive file
    closeArchiveFile(archiveEntryInfo->archiveInfo,FALSE);

    // reset compress (do it here because data if buffered and can be processed before a new file is opened)
    Compress_reset(&archiveEntryInfo->file.deltaCompressInfo);
    Compress_reset(&archiveEntryInfo->file.byteCompressInfo);
  }
  else
  {
    if (length > 0L)
    {
      // create chunk-headers
      if (!archiveEntryInfo->file.headerWrittenFlag)
      {
        writeFileChunks(archiveEntryInfo);
      }

      // encrypt block
      error = Crypt_encrypt(&archiveEntryInfo->file.cryptInfo,
                            archiveEntryInfo->file.buffer,
                            archiveEntryInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // write block
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

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : readFileDataBlock
* Purpose: read file data block, decrypt
* Input  : archiveEntryInfo - archive file entry info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors readFileDataBlock(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors error;
  uint   blockCount;

  assert(archiveEntryInfo != NULL);

  if      (!Chunk_eofSub(&archiveEntryInfo->file.chunkFileData.info))
  {
    // read data block from archive
    error = Chunk_readData(&archiveEntryInfo->file.chunkFileData.info,
                           archiveEntryInfo->file.buffer,
                           archiveEntryInfo->blockLength
                          );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // decrypt data block
    error = Crypt_decrypt(&archiveEntryInfo->file.cryptInfo,
                          archiveEntryInfo->file.buffer,
                          archiveEntryInfo->blockLength
                         );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // put compressed block into decompressor
    Compress_putCompressedBlock(&archiveEntryInfo->file.byteCompressInfo,
                                archiveEntryInfo->file.buffer,
                                archiveEntryInfo->blockLength
                               );
  }
  else if (!Compress_isFlush(&archiveEntryInfo->file.byteCompressInfo))
  {
    // no more data in archive -> flush data compress
    error = Compress_flush(&archiveEntryInfo->file.byteCompressInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  else
  {
    // check for end-of-compressed data
    error = Compress_getAvailableCompressedBlocks(&archiveEntryInfo->file.byteCompressInfo,
                                                  COMPRESS_BLOCK_TYPE_ANY,
                                                  &blockCount
                                                 );
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (blockCount <= 0)
    {
      return ERROR_COMPRESS_EOF;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeImageChunks
* Purpose: write image chunks
* Input  : archiveEntryInfo  - archive image entry info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeImageChunks(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors error;

  // open archive file if needed
  if (!archiveEntryInfo->archiveInfo->file.openFlag)
  {
    // create archive file
    error = createArchiveFile(archiveEntryInfo->archiveInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // initialise variables
    archiveEntryInfo->image.headerWrittenFlag = FALSE;

    archiveEntryInfo->image.chunkImageData.blockOffset = archiveEntryInfo->image.chunkImageData.blockOffset+archiveEntryInfo->image.chunkImageData.blockCount;
    archiveEntryInfo->image.chunkImageData.blockCount  = 0;

    // reset data crypt
    Crypt_reset(&archiveEntryInfo->image.cryptInfo,0);
  }

  // create file chunk
  error = Chunk_create(&archiveEntryInfo->image.chunkImage.info);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // create file entry chunk
  error = Chunk_create(&archiveEntryInfo->image.chunkImageEntry.info);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // create file delta chunk
  if (Compress_isCompressed(archiveEntryInfo->image.deltaCompressAlgorithm))
  {
    error = Chunk_create(&archiveEntryInfo->image.chunkImageDelta.info);
    if (error != ERROR_NONE)
   {
      return error;
    }
  }

  // create file data chunk
  error = Chunk_create(&archiveEntryInfo->image.chunkImageData.info);
  if (error != ERROR_NONE)
  {
    return error;
  }

  archiveEntryInfo->image.headerWrittenFlag = TRUE;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : flushImageDataBlocks
* Purpose: flush image data blocks
* Input  : archiveEntryInfo  - archive image entry info block
*          compressBlockType - compress block type; see CompressBlockTypes
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors flushImageDataBlocks(ArchiveEntryInfo   *archiveEntryInfo,
                                 CompressBlockTypes compressBlockType
                                )
{
  Errors error;
  uint   blockCount;
  ulong  length;

  // create new part (if not already exists)
  if (!archiveEntryInfo->image.headerWrittenFlag)
  {
    writeImageChunks(archiveEntryInfo);
  }

  // flush data
  do
  {
    error = Compress_getAvailableCompressedBlocks(&archiveEntryInfo->image.byteCompressInfo,
                                                  compressBlockType,
                                                  &blockCount
                                                 );
    if (error != ERROR_NONE)
    {
      return error;
    }

    if (blockCount > 0)
    {
      // get next compressed data-block
      Compress_getCompressedBlock(&archiveEntryInfo->image.byteCompressInfo,
                                  archiveEntryInfo->image.buffer,
                                  &length
                                 );
      assert(length <= archiveEntryInfo->blockLength);
#if 0
{
int h = open("encoded.testdata",O_CREAT|O_WRONLY|O_APPEND,0664);
fprintf(stderr,"%s, %d: length=%d %d %s\n",__FILE__,__LINE__,length,write(h,archiveEntryInfo->image.buffer,length),strerror(errno));
close(h);
}
#endif /* 0 */

      // encrypt block
      error = Crypt_encrypt(&archiveEntryInfo->image.cryptInfo,
                            archiveEntryInfo->image.buffer,
                            archiveEntryInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // store block into chunk
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
  while (blockCount > 0);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeImageDataBlock
* Purpose: write image data block, encrypt and split file
* Input  : archiveEntryInfo  - archive image entry info block
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
  bool   eofDelta;
  byte   deltaBuffer[1];
  ulong  deltaLength;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(archiveEntryInfo->archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);

  // get next compressed data-block
  Compress_getCompressedBlock(&archiveEntryInfo->image.byteCompressInfo,
                              archiveEntryInfo->image.buffer,
                              &length
                             );
  assert(length <= archiveEntryInfo->blockLength);

  // check if split is allowed and necessary
  newPartFlag =    allowNewPartFlag
                && checkNewPartNeeded(archiveEntryInfo->archiveInfo,
                                      archiveEntryInfo->image.headerLength,
                                      archiveEntryInfo->image.headerWrittenFlag,
                                      ((length > 0) || (blockMode == BLOCK_MODE_WRITE))?archiveEntryInfo->archiveInfo->blockLength:0
                                     );

  // split
  if (newPartFlag)
  {
    // create new part
    if (!archiveEntryInfo->image.headerWrittenFlag)
    {
      writeImageChunks(archiveEntryInfo);
    }

    // write last compressed block (if any)
    if (length > 0L)
    {
      // encrypt block
      error = Crypt_encrypt(&archiveEntryInfo->image.cryptInfo,
                            archiveEntryInfo->image.buffer,
                            archiveEntryInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // store block into chunk
      error = Chunk_writeData(&archiveEntryInfo->image.chunkImageData.info,
                              archiveEntryInfo->image.buffer,
                              archiveEntryInfo->blockLength
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    // flush delta compress
    error = Compress_flush(&archiveEntryInfo->image.deltaCompressInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
    eofDelta = FALSE;
    do
    {
      // flush compressed full data blocks
      error = flushImageDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_FULL);
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get next delta-compressed byte
      Compress_getCompressedBlock(&archiveEntryInfo->image.deltaCompressInfo,
                                  deltaBuffer,
                                  &deltaLength
                                 );
      assert(deltaLength <= 1);
      if (deltaLength > 0)
      {
        // compress data
        error = Compress_deflate(&archiveEntryInfo->image.byteCompressInfo,
                                 deltaBuffer,
                                 1,
                                 NULL
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      else
      {
        eofDelta = TRUE;
      }
    }
    while (!eofDelta);

    // flush data compress
    error = Compress_flush(&archiveEntryInfo->image.byteCompressInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = flushImageDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_ANY);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // update part size
    assert(archiveEntryInfo->image.blockSize > 0);
    archiveEntryInfo->image.chunkImageData.blockCount = Compress_getInputLength(&archiveEntryInfo->image.deltaCompressInfo)/archiveEntryInfo->image.blockSize;
    error = Chunk_update(&archiveEntryInfo->image.chunkImageData.info);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // close chunks
    error = Chunk_close(&archiveEntryInfo->image.chunkImageData.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (Compress_isCompressed(archiveEntryInfo->image.deltaCompressAlgorithm))
    {
      error = Chunk_close(&archiveEntryInfo->image.chunkImageDelta.info);
      if (error != ERROR_NONE)
      {
        return error;
      }
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

    // close archive file
    closeArchiveFile(archiveEntryInfo->archiveInfo,FALSE);

    // reset compress (do it here because data if buffered and can be processed before a new file is opened)
    Compress_reset(&archiveEntryInfo->file.deltaCompressInfo);
    Compress_reset(&archiveEntryInfo->image.byteCompressInfo);
  }
  else
  {
    if (length > 0L)
    {
      // create chunk-headers
      if (!archiveEntryInfo->image.headerWrittenFlag)
      {
        writeImageChunks(archiveEntryInfo);
      }

      // encrypt block
      error = Crypt_encrypt(&archiveEntryInfo->image.cryptInfo,
                            archiveEntryInfo->image.buffer,
                            archiveEntryInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // store block into chunk
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

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : readImageDataBlock
* Purpose: read image data block, decrypt
* Input  : archiveEntryInfo - archive image entry info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors readImageDataBlock(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors error;
  uint   blockCount;

  assert(archiveEntryInfo != NULL);

  if (!Chunk_eofSub(&archiveEntryInfo->image.chunkImageData.info))
  {
    // read
    error = Chunk_readData(&archiveEntryInfo->image.chunkImageData.info,
                           archiveEntryInfo->image.buffer,
                           archiveEntryInfo->blockLength
                          );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // decrypt
    error = Crypt_decrypt(&archiveEntryInfo->image.cryptInfo,
                          archiveEntryInfo->image.buffer,
                          archiveEntryInfo->blockLength
                         );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // put compressed block into decompressor
    Compress_putCompressedBlock(&archiveEntryInfo->image.byteCompressInfo,
                                archiveEntryInfo->image.buffer,
                                archiveEntryInfo->blockLength
                               );
  }
  else if (!Compress_isFlush(&archiveEntryInfo->image.byteCompressInfo))
  {
    // no more data in archive -> flush data compress
    error = Compress_flush(&archiveEntryInfo->image.byteCompressInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  else
  {
    // check for end-of-compressed data
    error = Compress_getAvailableCompressedBlocks(&archiveEntryInfo->image.byteCompressInfo,
                                                  COMPRESS_BLOCK_TYPE_ANY,
                                                  &blockCount
                                                 );
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (blockCount <= 0)
    {
      return ERROR_COMPRESS_EOF;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeHardLinkChunks
* Purpose: write hard link chunks
* Input  : archiveEntryInfo  - archive hard link entry info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeHardLinkChunks(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors            error;
  ChunkHardLinkName *chunkHardLinkName;

  // open archive file if needed
  if (!archiveEntryInfo->archiveInfo->file.openFlag)
  {
    // create archive file
    error = createArchiveFile(archiveEntryInfo->archiveInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // initialise variables
    archiveEntryInfo->hardLink.headerWrittenFlag = FALSE;

    archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset = archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset+archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize;
    archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize   = 0LL;

    // reset data crypt
    Crypt_reset(&archiveEntryInfo->hardLink.cryptInfo,0);
  }

  // create hard link chunk
  error = Chunk_create(&archiveEntryInfo->hardLink.chunkHardLink.info);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // create hard link entry chunk
  error = Chunk_create(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // create hard link name chunks
  LIST_ITERATE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
  {
    error = Chunk_create(&chunkHardLinkName->info);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // create hard link delta chunk
  if (Compress_isCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm))
  {
    error = Chunk_create(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
    if (error != ERROR_NONE)
   {
      return error;
    }
  }

  // create hard link data chunk
  error = Chunk_create(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
  if (error != ERROR_NONE)
  {
    return error;
  }

  archiveEntryInfo->hardLink.headerWrittenFlag = TRUE;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : flushHardLinkDataBlocks
* Purpose: flush hard link data blocks
* Input  : archiveEntryInfo  - archive hard link entry info block
*          compressBlockType - compress block type; see CompressBlockTypes
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors flushHardLinkDataBlocks(ArchiveEntryInfo   *archiveEntryInfo,
                                     CompressBlockTypes compressBlockType
                                    )
{
  Errors error;
  uint   blockCount;
  ulong  length;

  // create new part (if not already exists)
  if (!archiveEntryInfo->hardLink.headerWrittenFlag)
  {
    writeHardLinkChunks(archiveEntryInfo);
  }

  // flush data
  do
  {
    error = Compress_getAvailableCompressedBlocks(&archiveEntryInfo->hardLink.byteCompressInfo,
                                                  compressBlockType,
                                                  &blockCount
                                                 );
    if (error != ERROR_NONE)
    {
      return error;
    }

    if (blockCount > 0)
    {
      // get next compressed data-block
      Compress_getCompressedBlock(&archiveEntryInfo->hardLink.byteCompressInfo,
                                  archiveEntryInfo->hardLink.buffer,
                                  &length
                                 );
      assert(length <= archiveEntryInfo->blockLength);
#if 0
{
int h = open("encoded.testdata",O_CREAT|O_WRONLY|O_APPEND,0664);
fprintf(stderr,"%s, %d: length=%d %d %s\n",__FILE__,__LINE__,length,write(h,archiveEntryInfo->hardLink.buffer,length),strerror(errno));
close(h);
}
#endif /* 0 */

      // encrypt block
      error = Crypt_encrypt(&archiveEntryInfo->hardLink.cryptInfo,
                            archiveEntryInfo->hardLink.buffer,
                            archiveEntryInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // store block into chunk
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
  while (blockCount > 0);

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
  bool              eofDelta;
  byte              deltaBuffer[1];
  ulong             deltaLength;
  ChunkHardLinkName *chunkHardLinkName;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(archiveEntryInfo->archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);

  // get next compressed data-block
  Compress_getCompressedBlock(&archiveEntryInfo->hardLink.byteCompressInfo,
                              archiveEntryInfo->hardLink.buffer,
                              &length
                             );
  assert(length <= archiveEntryInfo->blockLength);

  // check if split is allowed and necessary
  newPartFlag =    allowNewPartFlag
                && checkNewPartNeeded(archiveEntryInfo->archiveInfo,
                                      archiveEntryInfo->hardLink.headerLength,
                                      archiveEntryInfo->hardLink.headerWrittenFlag,
                                      ((length > 0) || (blockMode == BLOCK_MODE_WRITE))?archiveEntryInfo->archiveInfo->blockLength:0
                                     );

  // split
  if (newPartFlag)
  {
    // create new part
    if (!archiveEntryInfo->hardLink.headerWrittenFlag)
    {
      writeHardLinkChunks(archiveEntryInfo);
    }

    // write last compressed block (if any)
    if (length > 0L)
    {
      // encrypt block
      error = Crypt_encrypt(&archiveEntryInfo->hardLink.cryptInfo,
                            archiveEntryInfo->hardLink.buffer,
                            archiveEntryInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // store block into chunk
      error = Chunk_writeData(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                              archiveEntryInfo->hardLink.buffer,
                              archiveEntryInfo->blockLength
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    // flush delta compress
    error = Compress_flush(&archiveEntryInfo->hardLink.deltaCompressInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
    eofDelta = FALSE;
    do
    {
      // flush compressed full data blocks
      error = flushHardLinkDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_FULL);
      if (error != ERROR_NONE)
      {
        return error;
      }

      // get next delta-compressed byte
      Compress_getCompressedBlock(&archiveEntryInfo->hardLink.deltaCompressInfo,
                                  deltaBuffer,
                                  &deltaLength
                                 );
      assert(deltaLength <= 1);
      if (deltaLength > 0)
      {
        // compress data
        error = Compress_deflate(&archiveEntryInfo->hardLink.byteCompressInfo,
                                 deltaBuffer,
                                 1,
                                 NULL
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      else
      {
        eofDelta = TRUE;
      }
    }
    while (!eofDelta);

    // flush data compress
    error = Compress_flush(&archiveEntryInfo->hardLink.byteCompressInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = flushHardLinkDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_ANY);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // update part size
    archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize = Compress_getInputLength(&archiveEntryInfo->hardLink.deltaCompressInfo);
    error = Chunk_update(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // close chunks
    error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (Compress_isCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm))
    {
      error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
      if (error != ERROR_NONE)
      {
        return error;
      }
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

    // close archive file
    closeArchiveFile(archiveEntryInfo->archiveInfo,FALSE);

    // reset compress (do it here because data if buffered and can be processed before a new file is opened)
    Compress_reset(&archiveEntryInfo->hardLink.deltaCompressInfo);
    Compress_reset(&archiveEntryInfo->hardLink.byteCompressInfo);
  }
  else
  {
    if (length > 0L)
    {
      // create chunk-headers
      if (!archiveEntryInfo->hardLink.headerWrittenFlag)
      {
        writeHardLinkChunks(archiveEntryInfo);
      }

      // encrypt block
      error = Crypt_encrypt(&archiveEntryInfo->hardLink.cryptInfo,
                            archiveEntryInfo->hardLink.buffer,
                            archiveEntryInfo->blockLength
                           );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // write block
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
  uint   blockCount;

  assert(archiveEntryInfo != NULL);

  if (!Chunk_eofSub(&archiveEntryInfo->hardLink.chunkHardLinkData.info))
  {
    // read
    error = Chunk_readData(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                           archiveEntryInfo->hardLink.buffer,
                           archiveEntryInfo->blockLength
                          );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // decrypt
    error = Crypt_decrypt(&archiveEntryInfo->hardLink.cryptInfo,
                          archiveEntryInfo->hardLink.buffer,
                          archiveEntryInfo->blockLength
                         );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // put compressed block into decompressor
    Compress_putCompressedBlock(&archiveEntryInfo->hardLink.byteCompressInfo,
                                archiveEntryInfo->hardLink.buffer,
                                archiveEntryInfo->blockLength
                               );
  }
  else if (!Compress_isFlush(&archiveEntryInfo->hardLink.byteCompressInfo))
  {
    // no more data in archive -> flush data compress
    error = Compress_flush(&archiveEntryInfo->hardLink.byteCompressInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  else
  {
    // check for end-of-compressed data
    error = Compress_getAvailableCompressedBlocks(&archiveEntryInfo->hardLink.byteCompressInfo,
                                                  COMPRESS_BLOCK_TYPE_ANY,
                                                  &blockCount
                                                 );
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (blockCount <= 0)
    {
      return ERROR_COMPRESS_EOF;
    }
  }

  return ERROR_NONE;
}

//---------------------------------------------------------------------

Errors Archive_initAll(void)
{
  // initialize variables
  List_init(&decryptPasswordList);

  return ERROR_NONE;
}

void Archive_doneAll(void)
{
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

  // detect block length of use crypt algorithm
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

  // init
  archiveInfo->jobOptions                      = jobOptions;
  archiveInfo->archiveNewFileFunction          = archiveNewFileFunction;
  archiveInfo->archiveNewFileUserData          = archiveNewFileUserData;
  archiveInfo->archiveGetCryptPasswordFunction = archiveGetCryptPasswordFunction;
  archiveInfo->archiveGetCryptPasswordUserData = archiveGetCryptPasswordUserData;

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

  // init key (if asymmetric encryption used)
  if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
  {
    // check if public key available
    if (jobOptions->cryptPublicKeyFileName == NULL)
    {
      String_delete(archiveInfo->printableName);
      String_delete(archiveInfo->file.fileName);
      return ERROR_NO_PUBLIC_KEY;
    }

    // read public key
    Crypt_initKey(&archiveInfo->cryptKey);
    error = Crypt_readKeyFile(&archiveInfo->cryptKey,
                              jobOptions->cryptPublicKeyFileName,
                              NULL
                             );
    if (error != ERROR_NONE)
    {
      String_delete(archiveInfo->printableName);
      String_delete(archiveInfo->file.fileName);
      return error;
    }

    // create new random key for encryption
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
        String_delete(archiveInfo->printableName);
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
  String      fileName;
  Errors      error;
  ChunkHeader chunkHeader;

  assert(archiveInfo != NULL);
  assert(storageName != NULL);

  // init variables
  fileName = String_new();

  // init
  archiveInfo->jobOptions                      = jobOptions;
  archiveInfo->archiveNewFileFunction          = NULL;
  archiveInfo->archiveNewFileUserData          = NULL;
  archiveInfo->archiveGetCryptPasswordFunction = archiveGetCryptPasswordFunction;
  archiveInfo->archiveGetCryptPasswordUserData = archiveGetCryptPasswordUserData;

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

  // init storage
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

  // open storage file
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

  // check if BAR archive file
  error = getNextChunkHeader(archiveInfo,&chunkHeader);
  if (error != ERROR_NONE)
  {
    Storage_close(&archiveInfo->storage.storageFileHandle);
    Storage_done(&archiveInfo->storage.storageFileHandle);
    String_delete(archiveInfo->printableName);
    String_delete(archiveInfo->storage.storageName);
    String_delete(fileName);
    return error;
  }
  if (chunkHeader.id != CHUNK_ID_BAR)
  {
    Storage_close(&archiveInfo->storage.storageFileHandle);
    Storage_done(&archiveInfo->storage.storageFileHandle);
    String_delete(archiveInfo->printableName);
    String_delete(archiveInfo->storage.storageName);
    String_delete(fileName);
    return ERROR_NOT_AN_ARCHIVE_FILE;
  }
  ungetNextChunkHeader(archiveInfo,&chunkHeader);

  // free resources
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

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    return FALSE;
  }

  // find next file, image, directory, link, hard link, special chunk
  chunkHeaderFoundFlag = FALSE;
  while (   !chunkHeaderFoundFlag
         && !chunkHeaderEOF(archiveInfo)
        )
  {
    // get next chunk header
    archiveInfo->pendingError = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (archiveInfo->pendingError != ERROR_NONE)
    {
      return FALSE;
    }

    // find next file, image, directory, link, special chunk
    switch (chunkHeader.id)
    {
      case CHUNK_ID_BAR:
        // bar header is simply ignored
        archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
        if (archiveInfo->pendingError != ERROR_NONE)
        {
          return FALSE;
        }
        break;
      case CHUNK_ID_KEY:
        // check if private key available
        if (archiveInfo->jobOptions->cryptPrivateKeyFileName == NULL)
        {
          return FALSE;
        }

        // read private key, try to read key with no password, all passwords
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
            // next password
            password = getNextDecryptPassword(&passwordHandle);
          }
        }
        if (!decryptedFlag)
        {
          return FALSE;
        }

        // read encryption key
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
    // store chunk header for read
    ungetNextChunkHeader(archiveInfo,&chunkHeader);
  }

  return !chunkHeaderFoundFlag;
}

Errors Archive_newFileEntry(ArchiveInfo                     *archiveInfo,
                            ArchiveEntryInfo                *archiveEntryInfo,
                            CompressSourceGetEntryDataBlock sourceGetEntryDataBlock,
                            void                            *sourceGetEntryDataBlockUserData,
                            const String                    fileName,
                            const FileInfo                  *fileInfo,
                            const String                    deltaSourceName,
                            bool                            deltaCompressFlag,
                            bool                            byteCompressFlag
                           )
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveEntryInfo != NULL);
  assert(fileInfo != NULL);

  // init crypt password
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

  // init archive file info
  archiveEntryInfo->archiveInfo                 = archiveInfo;
  archiveEntryInfo->mode                        = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm              = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength                 = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType            = ARCHIVE_ENTRY_TYPE_FILE;

  archiveEntryInfo->file.deltaCompressAlgorithm = deltaCompressFlag?archiveInfo->jobOptions->compressAlgorithm.delta:COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->file.byteCompressAlgorithm  = byteCompressFlag ?archiveInfo->jobOptions->compressAlgorithm.byte :COMPRESS_ALGORITHM_NONE;

  archiveEntryInfo->file.deltaSourceInit        = FALSE;

  archiveEntryInfo->file.headerLength           = 0;
  archiveEntryInfo->file.headerWrittenFlag      = FALSE;

  archiveEntryInfo->file.buffer                 = NULL;
  archiveEntryInfo->file.bufferLength           = 0L;

  // allocate buffer
  archiveEntryInfo->file.bufferLength = (MAX_BUFFER_SIZE/archiveEntryInfo->blockLength)*archiveEntryInfo->blockLength;
  archiveEntryInfo->file.buffer = (byte*)malloc(archiveEntryInfo->file.bufferLength);
  if (archiveEntryInfo->file.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init file chunk
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
    free(archiveEntryInfo->file.buffer);
    return error;
  }
  archiveEntryInfo->file.chunkFile.compressAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->file.byteCompressAlgorithm);
  archiveEntryInfo->file.chunkFile.cryptAlgorithm    = CRYPT_ALGORITHM_TO_CONSTANT(archiveInfo->jobOptions->cryptAlgorithm);

  // init file entry crypt, file entry chunk
  error = Crypt_init(&archiveEntryInfo->file.chunkFileEntry.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    free(archiveEntryInfo->file.buffer);
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
    free(archiveEntryInfo->file.buffer);
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

  // init file delta crypt, file delta chunk
  error = Crypt_init(&archiveEntryInfo->file.chunkFileDelta.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    free(archiveEntryInfo->file.buffer);
    return error;
  }
  error = Chunk_init(&archiveEntryInfo->file.chunkFileDelta.info,
                     &archiveEntryInfo->file.chunkFile.info,
                     NULL,
                     NULL,
                     CHUNK_ID_FILE_DELTA,
                     CHUNK_DEFINITION_FILE_DELTA,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->file.chunkFileDelta.cryptInfo,
                     &archiveEntryInfo->file.chunkFileDelta
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    free(archiveEntryInfo->file.buffer);
    return error;
  }
  if (Compress_isCompressed(archiveEntryInfo->file.deltaCompressAlgorithm))
  {
    archiveEntryInfo->file.chunkFileDelta.deltaAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->file.deltaCompressAlgorithm);
    String_set(archiveEntryInfo->file.chunkFileDelta.name,deltaSourceName);
  }

  // init file data crypt, file data chunk
  error = Crypt_init(&archiveEntryInfo->file.chunkFileData.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    free(archiveEntryInfo->file.buffer);
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
    Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    free(archiveEntryInfo->file.buffer);
    return error;
  }
  archiveEntryInfo->file.chunkFileData.fragmentOffset = 0LL;
  archiveEntryInfo->file.chunkFileData.fragmentSize   = 0LL;

  // init delta compress (if no delta-compression is enabled, use identity-compressor)
  error = Compress_new(&archiveEntryInfo->file.deltaCompressInfo,
                       COMPRESS_MODE_DEFLATE,
                       archiveEntryInfo->file.deltaCompressAlgorithm,
                       1,
                       sourceGetEntryDataBlock,
                       sourceGetEntryDataBlockUserData
                      );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->file.chunkFileData.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    free(archiveEntryInfo->file.buffer);
    return error;
  }

  // init data compress
  error = Compress_new(&archiveEntryInfo->file.byteCompressInfo,
                       COMPRESS_MODE_DEFLATE,
                       archiveEntryInfo->file.byteCompressAlgorithm,
                       archiveEntryInfo->blockLength,
                       NULL,
                       NULL
                      );
  if (error != ERROR_NONE)
  {
    Compress_delete(&archiveEntryInfo->file.deltaCompressInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFileData.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    free(archiveEntryInfo->file.buffer);
    return error;
  }

  // init file crypt
  error = Crypt_init(&archiveEntryInfo->file.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Compress_delete(&archiveEntryInfo->file.byteCompressInfo);
    Compress_delete(&archiveEntryInfo->file.deltaCompressInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFileData.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
    Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    free(archiveEntryInfo->file.buffer);
    return error;
  }

  // calculate header length
  archiveEntryInfo->file.headerLength = Chunk_getSize(CHUNK_DEFINITION_FILE,      0,                            &archiveEntryInfo->file.chunkFile     )+
                                        Chunk_getSize(CHUNK_DEFINITION_FILE_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->file.chunkFileEntry)+
                                        ((Compress_isCompressed(archiveEntryInfo->file.deltaCompressAlgorithm))
                                          ? Chunk_getSize(CHUNK_DEFINITION_FILE_DELTA,archiveEntryInfo->blockLength,&archiveEntryInfo->file.chunkFileDelta)
                                          : 0
                                        )+
                                        Chunk_getSize(CHUNK_DEFINITION_FILE_DATA, archiveEntryInfo->blockLength,&archiveEntryInfo->file.chunkFileData );

  return ERROR_NONE;
}

Errors Archive_newImageEntry(ArchiveInfo                     *archiveInfo,
                             ArchiveEntryInfo                *archiveEntryInfo,
                             CompressSourceGetEntryDataBlock sourceGetEntryDataBlock,
                             void                            *sourceGetEntryDataBlockUserData,
                             const String                    deviceName,
                             const DeviceInfo                *deviceInfo,
                             const String                    deltaSourceName,
                             bool                            deltaCompressFlag,
                             bool                            byteCompressFlag
                            )
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveEntryInfo != NULL);
  assert(deviceInfo != NULL);
  assert(deviceInfo->blockSize > 0);

  // init crypt password
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

  // init archive file info
  archiveEntryInfo->archiveInfo                  = archiveInfo;
  archiveEntryInfo->mode                         = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm               = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength                  = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType             = ARCHIVE_ENTRY_TYPE_IMAGE;

  archiveEntryInfo->image.blockSize              = deviceInfo->blockSize;

  archiveEntryInfo->image.deltaCompressAlgorithm = deltaCompressFlag?archiveInfo->jobOptions->compressAlgorithm.delta:COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->image.byteCompressAlgorithm  = byteCompressFlag ?archiveInfo->jobOptions->compressAlgorithm.byte :COMPRESS_ALGORITHM_NONE;

  archiveEntryInfo->image.deltaSourceInit        = FALSE;

  archiveEntryInfo->image.headerLength           = 0;
  archiveEntryInfo->image.headerWrittenFlag      = FALSE;

  archiveEntryInfo->image.buffer                 = NULL;
  archiveEntryInfo->image.bufferLength           = 0L;

  // allocate buffer
  archiveEntryInfo->image.bufferLength = (MAX_BUFFER_SIZE/archiveEntryInfo->blockLength)*archiveEntryInfo->blockLength;
  archiveEntryInfo->image.buffer = (byte*)malloc(archiveEntryInfo->image.bufferLength);
  if (archiveEntryInfo->image.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init image chunk
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
    free(archiveEntryInfo->image.buffer);
    return error;
  }
  archiveEntryInfo->image.chunkImage.compressAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->image.byteCompressAlgorithm);
  archiveEntryInfo->image.chunkImage.cryptAlgorithm    = archiveInfo->jobOptions->cryptAlgorithm;

  // init image entry crpy, image entry chunk
  error = Crypt_init(&archiveEntryInfo->image.chunkImageEntry.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    free(archiveEntryInfo->image.buffer);
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
    free(archiveEntryInfo->image.buffer);
    return error;
  }
  archiveEntryInfo->image.chunkImageEntry.size      = deviceInfo->size;
  archiveEntryInfo->image.chunkImageEntry.blockSize = deviceInfo->blockSize;
  String_set(archiveEntryInfo->image.chunkImageEntry.name,deviceName);

  // init image delta crypt, image delta chunk
  error = Crypt_init(&archiveEntryInfo->image.chunkImageDelta.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    free(archiveEntryInfo->image.buffer);
    return error;
  }
  error = Chunk_init(&archiveEntryInfo->image.chunkImageDelta.info,
                     &archiveEntryInfo->image.chunkImage.info,
                     NULL,
                     NULL,
                     CHUNK_ID_IMAGE_DELTA,
                     CHUNK_DEFINITION_IMAGE_DELTA,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->image.chunkImageDelta.cryptInfo,
                     &archiveEntryInfo->image.chunkImageDelta
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    free(archiveEntryInfo->image.buffer);
    return error;
  }
  if (Compress_isCompressed(archiveEntryInfo->image.deltaCompressAlgorithm))
  {
    archiveEntryInfo->image.chunkImageDelta.deltaAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->image.deltaCompressAlgorithm);
    String_set(archiveEntryInfo->image.chunkImageDelta.name,deltaSourceName);
  }

  // init image data crypt, image data chunk
  error = Crypt_init(&archiveEntryInfo->image.chunkImageData.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    free(archiveEntryInfo->image.buffer);
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
    Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    free(archiveEntryInfo->image.buffer);
    return error;
  }
  archiveEntryInfo->image.chunkImageData.blockOffset = 0LL;
  archiveEntryInfo->image.chunkImageData.blockCount  = 0LL;

  // init delta compress (if no delta-compression is enabled, use identity-compressor)
  error = Compress_new(&archiveEntryInfo->image.deltaCompressInfo,
                       COMPRESS_MODE_DEFLATE,
                       archiveEntryInfo->image.deltaCompressAlgorithm,
                       1,
                       sourceGetEntryDataBlock,
                       sourceGetEntryDataBlockUserData
                      );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->image.chunkImageData.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    free(archiveEntryInfo->image.buffer);
    return error;
  }

  // init data compress
  error = Compress_new(&archiveEntryInfo->image.byteCompressInfo,
                       COMPRESS_MODE_DEFLATE,
                       archiveEntryInfo->image.byteCompressAlgorithm,
                       archiveEntryInfo->blockLength,
                       NULL,
                       NULL
                      );
  if (error != ERROR_NONE)
  {
    Compress_delete(&archiveEntryInfo->image.deltaCompressInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImageData.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    free(archiveEntryInfo->image.buffer);
    return error;
  }

  // init file crypt
  error = Crypt_init(&archiveEntryInfo->image.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Compress_delete(&archiveEntryInfo->image.byteCompressInfo);
    Compress_delete(&archiveEntryInfo->image.deltaCompressInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImageData.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
    Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    free(archiveEntryInfo->image.buffer);
    return error;
  }

  // calculate header length
  archiveEntryInfo->image.headerLength = Chunk_getSize(CHUNK_DEFINITION_IMAGE,      0,                           &archiveEntryInfo->image.chunkImage     )+
                                        Chunk_getSize(CHUNK_DEFINITION_IMAGE_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->image.chunkImageEntry)+
                                        ((Compress_isCompressed(archiveEntryInfo->image.deltaCompressAlgorithm))
                                          ? Chunk_getSize(CHUNK_DEFINITION_IMAGE_DELTA,archiveEntryInfo->blockLength,&archiveEntryInfo->image.chunkImageDelta)
                                          : 0
                                        )+
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

  // init crypt password
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

  // init archive file info
  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm   = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength      = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_DIRECTORY;

  // init directory chunk
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

  // init directory entry chunk
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

  // calculate header length
  length = Chunk_getSize(CHUNK_DEFINITION_DIRECTORY,      0,                           &archiveEntryInfo->directory.chunkDirectory     )+
           Chunk_getSize(CHUNK_DEFINITION_DIRECTORY_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->directory.chunkDirectoryEntry);

  if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
  {
    // ensure space in archive
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

    // write directory chunk
    error = Chunk_create(&archiveEntryInfo->directory.chunkDirectory.info);
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
      Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo);
      Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
      return error;
    }
    error = Chunk_create(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
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

  // init crypt password
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

  // init archive file info
  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm   = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength      = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_LINK;

  // init link chunk
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

  // init link entry chunk
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

  // calculate length
  length = Chunk_getSize(CHUNK_DEFINITION_LINK,      0,                           &archiveEntryInfo->link.chunkLink     )+
           Chunk_getSize(CHUNK_DEFINITION_LINK_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->link.chunkLinkEntry);

  if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
  {
    // ensure space in archive
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

    // write link chunks
    error = Chunk_create(&archiveEntryInfo->link.chunkLink.info);
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveEntryInfo->link.chunkLinkEntry.info);
      Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo);
      Chunk_done(&archiveEntryInfo->link.chunkLink.info);
      return error;
    }
    error = Chunk_create(&archiveEntryInfo->link.chunkLinkEntry.info);
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

Errors Archive_newHardLinkEntry(ArchiveInfo                     *archiveInfo,
                                ArchiveEntryInfo                *archiveEntryInfo,
                                CompressSourceGetEntryDataBlock sourceGetEntryDataBlock,
                                void                            *sourceGetEntryDataBlockUserData,
                                const StringList                *fileNameList,
                                const FileInfo                  *fileInfo,
                                const String                    deltaSourceName,
                                bool                            deltaCompressFlag,
                                bool                            byteCompressFlag
                               )
{
  Errors            error;
  const StringNode  *stringNode;
  String            fileName;
  ChunkHardLinkName *chunkHardLinkName;

  assert(archiveInfo != NULL);
  assert(archiveEntryInfo != NULL);
  assert(fileInfo != NULL);

  // init crypt password
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

  // init archive file info
  archiveEntryInfo->archiveInfo                     = archiveInfo;
  archiveEntryInfo->mode                            = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->hardLink.deltaCompressAlgorithm = deltaCompressFlag?archiveInfo->jobOptions->compressAlgorithm.delta:COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->hardLink.byteCompressAlgorithm  = byteCompressFlag ?archiveInfo->jobOptions->compressAlgorithm.byte :COMPRESS_ALGORITHM_NONE;

  archiveEntryInfo->cryptAlgorithm                  = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength                     = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType                = ARCHIVE_ENTRY_TYPE_HARDLINK;

  List_init(&archiveEntryInfo->hardLink.chunkHardLinkNameList);

  archiveEntryInfo->hardLink.deltaSourceInit        = FALSE;

  archiveEntryInfo->hardLink.headerLength           = 0;
  archiveEntryInfo->hardLink.headerWrittenFlag      = FALSE;

  archiveEntryInfo->hardLink.buffer                 = NULL;
  archiveEntryInfo->hardLink.bufferLength           = 0L;

  // allocate data buffer
  archiveEntryInfo->hardLink.bufferLength = (MAX_BUFFER_SIZE/archiveEntryInfo->blockLength)*archiveEntryInfo->blockLength;
  archiveEntryInfo->hardLink.buffer = (byte*)malloc(archiveEntryInfo->hardLink.bufferLength);
  if (archiveEntryInfo->hardLink.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init hard link chunk
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
    free(archiveEntryInfo->hardLink.buffer);
    return error;
  }
  archiveEntryInfo->hardLink.chunkHardLink.compressAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->hardLink.byteCompressAlgorithm);
  archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithm    = archiveInfo->jobOptions->cryptAlgorithm;

  // init hard link entry crypt, hard link entry chunk
  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    free(archiveEntryInfo->hardLink.buffer);
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
    free(archiveEntryInfo->hardLink.buffer);
    return error;
  }
  archiveEntryInfo->hardLink.chunkHardLinkEntry.size            = fileInfo->size;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.timeModified    = fileInfo->timeModified;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.userId          = fileInfo->userId;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.groupId         = fileInfo->groupId;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.permission      = fileInfo->permission;

  // init hard link name chunks
  STRINGLIST_ITERATE(fileNameList,stringNode,fileName)
  {
    // new node
    chunkHardLinkName = LIST_NEW_NODE(ChunkHardLinkName);
    if (chunkHardLinkName == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    // init chunk
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
      free(archiveEntryInfo->hardLink.buffer);
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
      free(archiveEntryInfo->hardLink.buffer);
      return error;
    }
    String_set(chunkHardLinkName->name,fileName);

    List_append(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName);
  }

  // init hard link delta crypt, hard link delta chunk
  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,
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
    free(archiveEntryInfo->hardLink.buffer);
    return error;
  }
  error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info,
                     &archiveEntryInfo->hardLink.chunkHardLink.info,
                     NULL,
                     NULL,
                     CHUNK_ID_HARDLINK_DELTA,
                     CHUNK_DEFINITION_HARDLINK_DELTA,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,
                     &archiveEntryInfo->hardLink.chunkHardLinkDelta
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
    LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
    {
      Crypt_done(&chunkHardLinkName->cryptInfo);
      Chunk_done(&chunkHardLinkName->info);
    }
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    free(archiveEntryInfo->hardLink.buffer);
    return error;
  }
  if (Compress_isCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm))
  {
    archiveEntryInfo->hardLink.chunkHardLinkDelta.deltaAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->hardLink.deltaCompressAlgorithm);
    String_set(archiveEntryInfo->hardLink.chunkHardLinkDelta.name,deltaSourceName);
  }

  // init hard link data crypt, harda link data chunk
  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
    LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
    {
      Crypt_done(&chunkHardLinkName->cryptInfo);
      Chunk_done(&chunkHardLinkName->info);
    }
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    free(archiveEntryInfo->hardLink.buffer);
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
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
    LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
    {
      Crypt_done(&chunkHardLinkName->cryptInfo);
      Chunk_done(&chunkHardLinkName->info);
    }
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    free(archiveEntryInfo->hardLink.buffer);
    return error;
  }
  archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset = 0LL;
  archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize   = 0LL;

  // init delta compress (if no delta-compression is enabled, use identity-compressor)
  error = Compress_new(&archiveEntryInfo->hardLink.deltaCompressInfo,
                       COMPRESS_MODE_DEFLATE,
                       archiveEntryInfo->hardLink.deltaCompressAlgorithm,
                       1,
                       sourceGetEntryDataBlock,
                       sourceGetEntryDataBlockUserData
                      );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
    LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
    {
      Crypt_done(&chunkHardLinkName->cryptInfo);
      Chunk_done(&chunkHardLinkName->info);
    }
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    free(archiveEntryInfo->hardLink.buffer);
    return error;
  }

  // init data compress
  error = Compress_new(&archiveEntryInfo->hardLink.byteCompressInfo,
                       COMPRESS_MODE_DEFLATE,
                       archiveEntryInfo->hardLink.byteCompressAlgorithm,
                       archiveEntryInfo->blockLength,
                       NULL,
                       NULL
                      );
  if (error != ERROR_NONE)
  {
    Compress_delete(&archiveEntryInfo->hardLink.deltaCompressInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
    LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
    {
      Crypt_done(&chunkHardLinkName->cryptInfo);
      Chunk_done(&chunkHardLinkName->info);
    }
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    free(archiveEntryInfo->hardLink.buffer);
    return error;
  }

  // init hard link crypt
  error = Crypt_init(&archiveEntryInfo->hardLink.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    Compress_delete(&archiveEntryInfo->hardLink.deltaCompressInfo);
    Compress_delete(&archiveEntryInfo->hardLink.byteCompressInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
    LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
    {
      Crypt_done(&chunkHardLinkName->cryptInfo);
      Chunk_done(&chunkHardLinkName->info);
    }
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    free(archiveEntryInfo->hardLink.buffer);
    return error;
  }

  // calculate header length
  archiveEntryInfo->hardLink.headerLength = Chunk_getSize(CHUNK_DEFINITION_HARDLINK,      0,                           &archiveEntryInfo->hardLink.chunkHardLink     )+
                                            Chunk_getSize(CHUNK_DEFINITION_HARDLINK_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->hardLink.chunkHardLinkEntry)+
                                            ((Compress_isCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm))
                                              ? Chunk_getSize(CHUNK_DEFINITION_HARDLINK_DELTA,archiveEntryInfo->blockLength,&archiveEntryInfo->hardLink.chunkHardLinkDelta)
                                              : 0
                                            )+
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

  // init crypt password
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

  // init archive file info
  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm   = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength      = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_SPECIAL;

  // init special chunk
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

  // init special entry chunk
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

  // calculate length
  length = Chunk_getSize(CHUNK_DEFINITION_SPECIAL,      0,                           &archiveEntryInfo->special.chunkSpecial     )+
           Chunk_getSize(CHUNK_DEFINITION_SPECIAL_ENTRY,archiveEntryInfo->blockLength,&archiveEntryInfo->special.chunkSpecialEntry);

  if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
  {
    // ensure space in archive
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

    // write special chunks
    error = Chunk_create(&archiveEntryInfo->special.chunkSpecial.info);
    if (error != ERROR_NONE)
    {
      Chunk_done(&archiveEntryInfo->special.chunkSpecialEntry.info);
      Crypt_done(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo);
      Chunk_done(&archiveEntryInfo->special.chunkSpecial.info);
      return error;
    }
    error = Chunk_create(&archiveEntryInfo->special.chunkSpecialEntry.info);
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

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  // find next file, image, directory, link, special chunk
  do
  {
    // get next chunk
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      return error;
    }

    switch (chunkHeader.id)
    {
      case CHUNK_ID_BAR:
        // bar header is simply ignored
        error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
        if (error != ERROR_NONE)
        {
          return error;
        }
        break;
      case CHUNK_ID_KEY:
        // check if private key available
        if (archiveInfo->jobOptions->cryptPrivateKeyFileName == NULL)
        {
          return ERROR_NO_PRIVATE_KEY;
        }

        // read private key, try to read key with no password, all passwords
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
            // next password
            password = getNextDecryptPassword(&passwordHandle);
          }
        }
        if (!decryptedFlag)
        {
          return error;
        }

        // read encryption key
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
          // skip unknown chunks
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
          // unknown chunk
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

  // get file type
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

  // store chunk header for read
  ungetNextChunkHeader(archiveInfo,&chunkHeader);

  return ERROR_NONE;
}

Errors Archive_skipNextEntry(ArchiveInfo *archiveInfo)
{
  Errors      error;
  ChunkHeader chunkHeader;

  assert(archiveInfo != NULL);

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  // read next chunk
  error = getNextChunkHeader(archiveInfo,&chunkHeader);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // skip entry
  error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Archive_readFileEntry(ArchiveInfo        *archiveInfo,
                             ArchiveEntryInfo   *archiveEntryInfo,
                             CompressAlgorithms *deltaCompressAlgorithm,
                             CompressAlgorithms *byteCompressAlgorithm,
                             CryptAlgorithms    *cryptAlgorithm,
                             CryptTypes         *cryptType,
                             String             fileName,
                             FileInfo           *fileInfo,
                             String             deltaSourceName,
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
  assert(fileName != NULL);

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  // init archive file info
  archiveEntryInfo->archiveInfo            = archiveInfo;
  archiveEntryInfo->mode                   = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType       = ARCHIVE_ENTRY_TYPE_FILE;

  archiveEntryInfo->file.deltaSourceInit   = FALSE;

  archiveEntryInfo->file.buffer            = NULL;
  archiveEntryInfo->file.bufferLength      = 0L;

  // init file chunk
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

  // find next file chunk
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

  // read file chunk
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
  archiveEntryInfo->file.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->file.byteCompressAlgorithm  = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->file.chunkFile.compressAlgorithm);
  archiveEntryInfo->cryptAlgorithm              = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->file.chunkFile.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  // allocate buffer
  archiveEntryInfo->file.bufferLength = (MAX_BUFFER_SIZE/archiveEntryInfo->blockLength)*archiveEntryInfo->blockLength;
  archiveEntryInfo->file.buffer = (byte*)malloc(archiveEntryInfo->file.bufferLength);
  if (archiveEntryInfo->file.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init data decompress
  error = Compress_new(&archiveEntryInfo->file.byteCompressInfo,
                       COMPRESS_MODE_INFLATE,
                       archiveEntryInfo->file.byteCompressAlgorithm,
                       archiveEntryInfo->blockLength,
                       NULL,
                       NULL
                      );
  if (error != ERROR_NONE)
  {
    free(archiveEntryInfo->file.buffer);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }

  // try to read file entry with all passwords
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
    // reset chunk read position
    Chunk_seek(&archiveEntryInfo->file.chunkFile.info,index);

    // init file entry/delta/data/file crypt
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
      error = Crypt_init(&archiveEntryInfo->file.chunkFileDelta.cryptInfo,
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
      error = Crypt_init(&archiveEntryInfo->file.chunkFileData.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->file.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
      }
    }

    // init file entry/delta/data chunks
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
        Crypt_done(&archiveEntryInfo->file.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->file.chunkFileDelta.info,
                         &archiveEntryInfo->file.chunkFile.info,
                         NULL,
                         NULL,
                         CHUNK_ID_FILE_DELTA,
                         CHUNK_DEFINITION_FILE_DELTA,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->file.chunkFileDelta.cryptInfo,
                         &archiveEntryInfo->file.chunkFileDelta
                        );
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
        Crypt_done(&archiveEntryInfo->file.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
      }
    }
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
        Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info);
        Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
        Crypt_done(&archiveEntryInfo->file.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
      }
    }

    // try to read file entry/data chunks
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
            // read file entry chunk
            error = Chunk_open(&archiveEntryInfo->file.chunkFileEntry.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            // get file meta data
            String_set(fileName,archiveEntryInfo->file.chunkFileEntry.name);
            if (fileInfo != NULL)
            {
              fileInfo->type            = FILE_TYPE_FILE;
              fileInfo->size            = archiveEntryInfo->file.chunkFileEntry.size;
              fileInfo->timeLastAccess  = archiveEntryInfo->file.chunkFileEntry.timeLastAccess;
              fileInfo->timeModified    = archiveEntryInfo->file.chunkFileEntry.timeModified;
              fileInfo->timeLastChanged = archiveEntryInfo->file.chunkFileEntry.timeLastChanged;
              fileInfo->userId          = archiveEntryInfo->file.chunkFileEntry.userId;
              fileInfo->groupId         = archiveEntryInfo->file.chunkFileEntry.groupId;
              fileInfo->permission      = archiveEntryInfo->file.chunkFileEntry.permission;
            }

            foundFileEntryFlag = TRUE;
            break;
          case CHUNK_ID_FILE_DELTA:
            // read file delta chunk
            error = Chunk_open(&archiveEntryInfo->file.chunkFileDelta.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            // get delta compress meta data
            archiveEntryInfo->file.deltaCompressAlgorithm = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->file.chunkFileDelta.deltaAlgorithm);
            if (deltaSourceName != NULL) String_set(deltaSourceName,archiveEntryInfo->file.chunkFileDelta.name);
            break;
          case CHUNK_ID_FILE_DATA:
            // read file data chunk
            error = Chunk_open(&archiveEntryInfo->file.chunkFileData.info,
                               &subChunkHeader,
                               Chunk_getSize(CHUNK_DEFINITION_FILE_DATA,archiveEntryInfo->blockLength,NULL)
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            // get data meta data
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
        Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info);
        Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
        Crypt_done(&archiveEntryInfo->file.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
      }
    }

    if (error != ERROR_NONE)
    {
      if (   (archiveEntryInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
          && (archiveInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        // get next password
        password = getNextDecryptPassword(&passwordHandle);

        // reset error and try next password
        if (password != NULL)
        {
          error = ERROR_NONE;
        }
      }
      else
      {
        // no more passwords when no encryption or asymmetric encryption is used
        password = NULL;
      }
    }
  } /* while */
  if (error != ERROR_NONE)
  {
    return error;
  }

  // check if mandatory file entry chunk and file data chunk found
  if (!foundFileEntryFlag || !foundFileDataFlag)
  {
    Chunk_done(&archiveEntryInfo->file.chunkFileData.info);
    Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info);
    Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
    Crypt_done(&archiveEntryInfo->file.cryptInfo);
    Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
    Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
    Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
    Compress_delete(&archiveEntryInfo->file.byteCompressInfo);
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

  // init delta decompress (if no delta-compression is enabled, use identity-compressor)
  error = Compress_new(&archiveEntryInfo->file.deltaCompressInfo,
                       COMPRESS_MODE_INFLATE,
                       archiveEntryInfo->file.deltaCompressAlgorithm,
                       1,
                       Source_getEntryDataBlock,
                       &archiveEntryInfo->file.sourceEntryInfo
                      );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->file.chunkFileData.info);
    Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info);
    Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
    Crypt_done(&archiveEntryInfo->file.cryptInfo);
    Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
    Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
    Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
    Compress_delete(&archiveEntryInfo->file.byteCompressInfo);
    free(archiveEntryInfo->file.buffer);
    Chunk_done(&archiveEntryInfo->file.chunkFile.info);

    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);

    return error;
  }

  // init variables
  if (deltaCompressAlgorithm != NULL) (*deltaCompressAlgorithm) = archiveEntryInfo->file.deltaCompressAlgorithm;
  if (byteCompressAlgorithm  != NULL) (*byteCompressAlgorithm)  = archiveEntryInfo->file.byteCompressAlgorithm;
  if (cryptAlgorithm         != NULL) (*cryptAlgorithm)         = archiveEntryInfo->cryptAlgorithm;
  if (cryptType              != NULL) (*cryptType)              = archiveInfo->cryptType;

  // reset compress, crypt
//  Compress_reset(&archiveEntryInfo->file.byteCompressInfo);
//  Crypt_reset(&archiveEntryInfo->file.chunkFileData.cryptInfo,0);

  return ERROR_NONE;
}

Errors Archive_readImageEntry(ArchiveInfo        *archiveInfo,
                              ArchiveEntryInfo   *archiveEntryInfo,
                              CompressAlgorithms *deltaCompressAlgorithm,
                              CompressAlgorithms *byteCompressAlgorithm,
                              CryptAlgorithms    *cryptAlgorithm,
                              CryptTypes         *cryptType,
                              String             deviceName,
                              DeviceInfo         *deviceInfo,
                              String             deltaSourceName,
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

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  // init archive file info
  archiveEntryInfo->archiveInfo           = archiveInfo;
  archiveEntryInfo->mode                  = ARCHIVE_MODE_READ;
//???
  archiveEntryInfo->archiveEntryType      = ARCHIVE_ENTRY_TYPE_IMAGE;

  archiveEntryInfo->image.deltaSourceInit = FALSE;

  archiveEntryInfo->image.buffer          = NULL;
  archiveEntryInfo->image.bufferLength    = 0L;

  // init image chunk
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

  // find next image chunk
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

  // read image chunk
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
  archiveEntryInfo->image.byteCompressAlgorithm = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->image.chunkImage.compressAlgorithm);
  archiveEntryInfo->cryptAlgorithm              = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->image.chunkImage.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  // allocate buffer
  archiveEntryInfo->image.bufferLength = (MAX_BUFFER_SIZE/archiveEntryInfo->blockLength)*archiveEntryInfo->blockLength;
  archiveEntryInfo->image.buffer = (byte*)malloc(archiveEntryInfo->image.bufferLength);
  if (archiveEntryInfo->image.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init data decompress
  error = Compress_new(&archiveEntryInfo->image.byteCompressInfo,
                       COMPRESS_MODE_INFLATE,
                       archiveEntryInfo->image.byteCompressAlgorithm,
                       archiveEntryInfo->blockLength,
                       NULL,
                       NULL
                      );
  if (error != ERROR_NONE)
  {
    free(archiveEntryInfo->image.buffer);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }

  // try to read image entry with all passwords
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

    // reset chunk read position
    Chunk_seek(&archiveEntryInfo->image.chunkImage.info,index);

    // init image entry/delta/data/image crypt
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
      error = Crypt_init(&archiveEntryInfo->image.chunkImageDelta.cryptInfo,
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
      error = Crypt_init(&archiveEntryInfo->image.chunkImageData.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->image.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
      }
    }

    // init image entry chunk
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
        Crypt_done(&archiveEntryInfo->image.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->image.chunkImageDelta.info,
                         &archiveEntryInfo->image.chunkImage.info,
                         NULL,
                         NULL,
                         CHUNK_ID_IMAGE_DELTA,
                         CHUNK_DEFINITION_IMAGE_DELTA,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->image.chunkImageDelta.cryptInfo,
                         &archiveEntryInfo->image.chunkImageDelta
                        );
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
        Crypt_done(&archiveEntryInfo->image.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
      }
    }
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
        Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info);
        Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
        Crypt_done(&archiveEntryInfo->image.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
      }
    }

    // try to read image entry/data chunks
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
            // read image entry chunk
            error = Chunk_open(&archiveEntryInfo->image.chunkImageEntry.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            // get image meta data
            String_set(deviceName,archiveEntryInfo->image.chunkImageEntry.name);
            deviceInfo->size      = archiveEntryInfo->image.chunkImageEntry.size;
            deviceInfo->blockSize = archiveEntryInfo->image.chunkImageEntry.blockSize;

            foundImageEntryFlag = TRUE;
            break;
          case CHUNK_ID_IMAGE_DELTA:
            // read image delta chunk
            error = Chunk_open(&archiveEntryInfo->image.chunkImageDelta.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            // get delta compress meta data
            archiveEntryInfo->image.deltaCompressAlgorithm = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->image.chunkImageDelta.deltaAlgorithm);
            if (deltaSourceName != NULL) String_set(deltaSourceName,archiveEntryInfo->image.chunkImageDelta.name);
            break;
          case CHUNK_ID_IMAGE_DATA:
            // read image data chunk
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
        Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info);
        Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
        Crypt_done(&archiveEntryInfo->image.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
      }
    }

    if (error != ERROR_NONE)
    {
      if (   (archiveEntryInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
          && (archiveInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        // get next password
        password = getNextDecryptPassword(&passwordHandle);

        // reset error and try next password
        if (password != NULL)
        {
          error = ERROR_NONE;
        }
      }
      else
      {
        // no more passwords when no encryption or asymmetric encryption is used
        password = NULL;
      }
    }
  } /* while */

  if (!foundImageEntryFlag || !foundImageDataFlag)
  {
    Chunk_done(&archiveEntryInfo->image.chunkImageData.info);
    Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info);
    Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
    Crypt_done(&archiveEntryInfo->image.cryptInfo);
    Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
    Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
    Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
    Compress_delete(&archiveEntryInfo->image.byteCompressInfo);
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

  // init delta decompress (if no delta-compression is enabled, use identity-compressor)
  error = Compress_new(&archiveEntryInfo->image.deltaCompressInfo,
                       COMPRESS_MODE_INFLATE,
                       archiveEntryInfo->image.deltaCompressAlgorithm,
                       1,
                       Source_getEntryDataBlock,
                       &archiveEntryInfo->image.sourceEntryInfo
                      );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->image.chunkImageData.info);
    Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info);
    Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
    Crypt_done(&archiveEntryInfo->image.cryptInfo);
    Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
    Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
    Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
    Compress_delete(&archiveEntryInfo->image.byteCompressInfo);
    free(archiveEntryInfo->image.buffer);
    Chunk_done(&archiveEntryInfo->image.chunkImage.info);

    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);

    return error;
  }

  // init variables
  if (deltaCompressAlgorithm != NULL) (*deltaCompressAlgorithm) = archiveEntryInfo->image.deltaCompressAlgorithm;
  if (byteCompressAlgorithm  != NULL) (*byteCompressAlgorithm)  = archiveEntryInfo->image.byteCompressAlgorithm;
  if (cryptAlgorithm         != NULL) (*cryptAlgorithm)         = archiveEntryInfo->cryptAlgorithm;
  if (cryptType              != NULL) (*cryptType)              = archiveInfo->cryptType;

  // reset compress, crypt
//  Compress_reset(&archiveEntryInfo->image.byteCompressInfo);
//  Crypt_reset(&archiveEntryInfo->image.chunkImageData.cryptInfo,0);

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

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  // init archive file info
  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_DIRECTORY;

  // init directory chunk
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

  // find next directory chunk
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

  // read directory chunk
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

  // detect block length of use crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  // try to read directory entry with all passwords
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
    // reset chunk read position
    Chunk_seek(&archiveEntryInfo->directory.chunkDirectory.info,index);

    // init crypt
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

    // init directory entry chunk
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

    // read directory entry
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
            // read directory entry chunk
            error = Chunk_open(&archiveEntryInfo->directory.chunkDirectoryEntry.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            // get directory meta data
            String_set(directoryName,archiveEntryInfo->directory.chunkDirectoryEntry.name);
            if (fileInfo != NULL)
            {
              fileInfo->type            = FILE_TYPE_DIRECTORY;
              fileInfo->timeLastAccess  = archiveEntryInfo->directory.chunkDirectoryEntry.timeLastAccess;
              fileInfo->timeModified    = archiveEntryInfo->directory.chunkDirectoryEntry.timeModified;
              fileInfo->timeLastChanged = archiveEntryInfo->directory.chunkDirectoryEntry.timeLastChanged;
              fileInfo->userId          = archiveEntryInfo->directory.chunkDirectoryEntry.userId;
              fileInfo->groupId         = archiveEntryInfo->directory.chunkDirectoryEntry.groupId;
              fileInfo->permission      = archiveEntryInfo->directory.chunkDirectoryEntry.permission;
            }

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
        // get next password
        password = getNextDecryptPassword(&passwordHandle);

        // reset error and try next password
        if (password != NULL)
        {
          error = ERROR_NONE;
        }
      }
      else
      {
        // no more passwords when no encryption or asymmetric encryption is used
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

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  // init archive file info
  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_LINK;

  // init link chunk
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

  // find next soft link chunk
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

  // read link chunk
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

  // detect block length of use crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->link.chunkLink.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  // try to read link entry with all passwords
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

    // reset chunk read position
    Chunk_seek(&archiveEntryInfo->link.chunkLink.info,index);

    // init crypt
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

    // init link entry chunk
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

    // read link entry
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
            // read link entry chunk
            error = Chunk_open(&archiveEntryInfo->link.chunkLinkEntry.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            // get link meta data
            String_set(linkName,archiveEntryInfo->link.chunkLinkEntry.name);
            String_set(destinationName,archiveEntryInfo->link.chunkLinkEntry.destinationName);
            if (fileInfo != NULL)
            {
              fileInfo->type            = FILE_TYPE_LINK;
              fileInfo->timeLastAccess  = archiveEntryInfo->link.chunkLinkEntry.timeLastAccess;
              fileInfo->timeModified    = archiveEntryInfo->link.chunkLinkEntry.timeModified;
              fileInfo->timeLastChanged = archiveEntryInfo->link.chunkLinkEntry.timeLastChanged;
              fileInfo->userId          = archiveEntryInfo->link.chunkLinkEntry.userId;
              fileInfo->groupId         = archiveEntryInfo->link.chunkLinkEntry.groupId;
              fileInfo->permission      = archiveEntryInfo->link.chunkLinkEntry.permission;
            }

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
        // get next password
        password = getNextDecryptPassword(&passwordHandle);

        // reset error and try next password
        if (password != NULL)
        {
          error = ERROR_NONE;
        }
      }
      else
      {
        // no more passwords when no encryption or asymmetric encryption is used
        password = NULL;
      }
    }
  } // while

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
                                 CompressAlgorithms *deltaCompressAlgorithm,
                                 CompressAlgorithms *byteCompressAlgorithm,
                                 CryptAlgorithms    *cryptAlgorithm,
                                 CryptTypes         *cryptType,
                                 StringList         *fileNameList,
                                 FileInfo           *fileInfo,
                                 String             deltaSourceName,
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

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  // init archive file info
  archiveEntryInfo->archiveInfo           = archiveInfo;
  archiveEntryInfo->mode                  = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType      = ARCHIVE_ENTRY_TYPE_HARDLINK;

  archiveEntryInfo->hardLink.buffer       = NULL;
  archiveEntryInfo->hardLink.bufferLength = 0L;

  // init hard link chunk
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

  // find next hard link chunk
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

  // read hard link chunk
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
  archiveEntryInfo->hardLink.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->hardLink.byteCompressAlgorithm  = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->hardLink.chunkHardLink.compressAlgorithm);
  archiveEntryInfo->cryptAlgorithm                  = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  // allocate buffer
  archiveEntryInfo->hardLink.bufferLength = (MAX_BUFFER_SIZE/archiveEntryInfo->blockLength)*archiveEntryInfo->blockLength;
  archiveEntryInfo->hardLink.buffer = (byte*)malloc(archiveEntryInfo->hardLink.bufferLength);
  if (archiveEntryInfo->hardLink.buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init data decompress
  error = Compress_new(&archiveEntryInfo->hardLink.byteCompressInfo,
                       COMPRESS_MODE_INFLATE,
                       archiveEntryInfo->hardLink.byteCompressAlgorithm,
                       archiveEntryInfo->blockLength,
                       NULL,
                       NULL
                      );
  if (error != ERROR_NONE)
  {
    free(archiveEntryInfo->hardLink.buffer);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }

  // try to read hard link entry with all passwords
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

    // reset chunk read position
    Chunk_seek(&archiveEntryInfo->hardLink.chunkHardLink.info,index);

    // init hard link entry/delta/data/hard link crypt
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
      error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,
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
      error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error != ERROR_NONE)
      {
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
      }
    }

    // init hard link entry/delta/data chunks
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
        Crypt_done(&archiveEntryInfo->hardLink.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info,
                         &archiveEntryInfo->hardLink.chunkHardLink.info,
                         NULL,
                         NULL,
                         CHUNK_ID_HARDLINK_DELTA,
                         CHUNK_DEFINITION_HARDLINK_DELTA,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,
                         &archiveEntryInfo->hardLink.chunkHardLinkDelta
                        );
      if (error != ERROR_NONE)
      {
        Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
        Crypt_done(&archiveEntryInfo->hardLink.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
      }
    }
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
        Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
        Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
        Crypt_done(&archiveEntryInfo->hardLink.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
      }
    }

    // try to read hard link entry/name/data chunks
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
            // read hard link entry chunk
            error = Chunk_open(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            // get hard link meta data
            if (fileInfo != NULL)
            {
              fileInfo->type            = FILE_TYPE_HARDLINK;
              fileInfo->size            = archiveEntryInfo->hardLink.chunkHardLinkEntry.size;
              fileInfo->timeLastAccess  = archiveEntryInfo->hardLink.chunkHardLinkEntry.timeLastAccess;
              fileInfo->timeModified    = archiveEntryInfo->hardLink.chunkHardLinkEntry.timeModified;
              fileInfo->timeLastChanged = archiveEntryInfo->hardLink.chunkHardLinkEntry.timeLastChanged;
              fileInfo->userId          = archiveEntryInfo->hardLink.chunkHardLinkEntry.userId;
              fileInfo->groupId         = archiveEntryInfo->hardLink.chunkHardLinkEntry.groupId;
              fileInfo->permission      = archiveEntryInfo->hardLink.chunkHardLinkEntry.permission;
            }

            foundHardLinkEntryFlag = TRUE;
            break;
          case CHUNK_ID_HARDLINK_NAME:
            // new hard link name
            chunkHardLinkName = LIST_NEW_NODE(ChunkHardLinkName);
            if (chunkHardLinkName == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }

            // init hard link name chunk
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

            // read hard link name chunk
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
          case CHUNK_ID_HARDLINK_DELTA:
            // read hard link delta chunk
            error = Chunk_open(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            // get delta compress meta data
            archiveEntryInfo->hardLink.deltaCompressAlgorithm = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->hardLink.chunkHardLinkDelta.deltaAlgorithm);
            if (deltaSourceName != NULL) String_set(deltaSourceName,archiveEntryInfo->hardLink.chunkHardLinkDelta.name);
            break;
          case CHUNK_ID_HARDLINK_DATA:
            // read hard link data chunk
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
        // free resources
        Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
        Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
        LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
        {
          Crypt_done(&chunkHardLinkName->cryptInfo);
          Chunk_done(&chunkHardLinkName->info);
        }
        Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
        Crypt_done(&archiveEntryInfo->hardLink.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
        Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
      }
    }

    if (error != ERROR_NONE)
    {
      if (   (archiveEntryInfo->cryptAlgorithm != CRYPT_ALGORITHM_NONE)
          && (archiveInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        // get next password
        password = getNextDecryptPassword(&passwordHandle);

        // reset error and try next password
        if (password != NULL)
        {
          error = ERROR_NONE;
        }
      }
      else
      {
        // no more passwords when no encryption or asymmetric encryption is used
        password = NULL;
      }
    }
  } /* while */

  if (!foundHardLinkEntryFlag || !foundHardLinkDataFlag)
  {
    // free resources
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
    LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
    {
      Crypt_done(&chunkHardLinkName->cryptInfo);
      Chunk_done(&chunkHardLinkName->info);
    }
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
    Crypt_done(&archiveEntryInfo->hardLink.cryptInfo);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
    Compress_delete(&archiveEntryInfo->hardLink.byteCompressInfo);
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

  // init delta decompress (if no delta-compression is enabled, use identity-compressor)
  error = Compress_new(&archiveEntryInfo->hardLink.deltaCompressInfo,
                       COMPRESS_MODE_INFLATE,
                       archiveEntryInfo->hardLink.deltaCompressAlgorithm,
                       1,
                       Source_getEntryDataBlock,
                       &archiveEntryInfo->hardLink.sourceEntryInfo
                      );
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
    LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
    {
      Crypt_done(&chunkHardLinkName->cryptInfo);
      Chunk_done(&chunkHardLinkName->info);
    }
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
    Crypt_done(&archiveEntryInfo->hardLink.cryptInfo);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
    Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
    Compress_delete(&archiveEntryInfo->hardLink.byteCompressInfo);
    free(archiveEntryInfo->hardLink.buffer);
    Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);

    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);

    return error;
  }

  // init variables
  if (deltaCompressAlgorithm != NULL) (*deltaCompressAlgorithm) = archiveEntryInfo->hardLink.deltaCompressAlgorithm;
  if (byteCompressAlgorithm  != NULL) (*byteCompressAlgorithm)  = archiveEntryInfo->hardLink.byteCompressAlgorithm;
  if (cryptAlgorithm         != NULL) (*cryptAlgorithm)         = archiveEntryInfo->cryptAlgorithm;
  if (cryptType              != NULL) (*cryptType)              = archiveInfo->cryptType;

  // reset compress, crypt
//  Compress_reset(&archiveEntryInfo->hardLink.byteCompressInfo);
//  Crypt_reset(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,0);

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

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  // init archive file info
  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_SPECIAL;

  // init special chunk
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

  // find next special chunk
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

  // read special chunk
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

  // detect block length of use crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    Chunk_done(&archiveEntryInfo->special.chunkSpecial.info);
    Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  // try to read special entry with all passwords
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

    // reset chunk read position
    Chunk_seek(&archiveEntryInfo->special.chunkSpecial.info,index);

    // init crypt
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

    // init special entry chunk
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

    // read special entry
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
            // read special entry chunk
            error = Chunk_open(&archiveEntryInfo->special.chunkSpecialEntry.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            // get special meta data
            String_set(specialName,archiveEntryInfo->special.chunkSpecialEntry.name);
            if (fileInfo != NULL)
            {
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
            }

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
        // get next password
        password = getNextDecryptPassword(&passwordHandle);

        // reset error and try next password
        if (password != NULL)
        {
          error = ERROR_NONE;
        }
      }
      else
      {
        // no more passwords when no encryption or asymmetric encryption is used
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
  bool   eofDelta;
  uint   blockCount;
  byte   deltaBuffer[1];
  ulong  deltaLength;

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
              // flush delta compress
              error = Compress_flush(&archiveEntryInfo->file.deltaCompressInfo);
              if (error == ERROR_NONE)
              {
                eofDelta = FALSE;
                do
                {
                  // flush compressed full data blocks
                  error = flushFileDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_FULL);
                  if (error == ERROR_NONE)
                  {
                    // get next delta-compressed byte
                    Compress_getCompressedBlock(&archiveEntryInfo->file.deltaCompressInfo,
                                                deltaBuffer,
                                                &deltaLength
                                               );
                    assert(deltaLength <= 1);
                    if (deltaLength > 0)
                    {
                      // compress data
                      error = Compress_deflate(&archiveEntryInfo->file.byteCompressInfo,
                                               deltaBuffer,
                                               1,
                                               NULL
                                              );
                    }
                    else
                    {
                      // no more delta-compressed data
                      eofDelta = TRUE;
                    }
                  }
                }
                while (   (error == ERROR_NONE)
                       && !eofDelta
                      );
              }

              // flush data compress
              if (error == ERROR_NONE)
              {
                error = Compress_flush(&archiveEntryInfo->file.byteCompressInfo);
              }
              if (error == ERROR_NONE)
              {
                error = flushFileDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_ANY);
              }

              // update file and chunks if header is written
              if (archiveEntryInfo->file.headerWrittenFlag)
              {
                // update part size
                archiveEntryInfo->file.chunkFileData.fragmentSize = Compress_getInputLength(&archiveEntryInfo->file.deltaCompressInfo);
                if (error == ERROR_NONE)
                {
                  tmpError = Chunk_update(&archiveEntryInfo->file.chunkFileData.info);
                  if (tmpError != ERROR_NONE) error = tmpError;
                }

                // close chunks
                tmpError = Chunk_close(&archiveEntryInfo->file.chunkFileData.info);
                if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
                if (Compress_isCompressed(archiveEntryInfo->file.deltaCompressAlgorithm))
                {
                  tmpError = Chunk_close(&archiveEntryInfo->file.chunkFileDelta.info);
                  if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
                }
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
                // store in index database
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

            // free resources
            Crypt_done(&archiveEntryInfo->file.cryptInfo);
            Compress_delete(&archiveEntryInfo->file.byteCompressInfo);
            Compress_delete(&archiveEntryInfo->file.deltaCompressInfo);
            Chunk_done(&archiveEntryInfo->file.chunkFileData.info);
            Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
            Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info);
            Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
            Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
            free(archiveEntryInfo->file.buffer);
            Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->file.chunkFile.info);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
            {
              // flush data compress
              error = Compress_flush(&archiveEntryInfo->image.byteCompressInfo);
              if (error == ERROR_NONE)
              {
                do
                {
                  error = Compress_getAvailableCompressedBlocks(&archiveEntryInfo->image.byteCompressInfo,
                                                                COMPRESS_BLOCK_TYPE_ANY,
                                                                &blockCount
                                                               );
                  if (error == ERROR_NONE)
                  {
                    if (blockCount > 0)
                    {
                      error = writeImageDataBlock(archiveEntryInfo,BLOCK_MODE_WRITE,FALSE);
                    }
                  }
                }
                while (   (error == ERROR_NONE)
                       && (blockCount > 0)
                      );
                if (error == ERROR_NONE)
                {
                  error = writeImageDataBlock(archiveEntryInfo,BLOCK_MODE_FLUSH,FALSE);
                }
              }

              // update file and chunks if header is written
              if (archiveEntryInfo->image.headerWrittenFlag)
              {
                // update part block count
                assert(archiveEntryInfo->image.blockSize > 0);
                assert((Compress_getInputLength(&archiveEntryInfo->image.byteCompressInfo) % archiveEntryInfo->image.blockSize) == 0);
                archiveEntryInfo->image.chunkImageData.blockCount = Compress_getInputLength(&archiveEntryInfo->image.byteCompressInfo)/archiveEntryInfo->image.blockSize;
                if (error == ERROR_NONE)
                {
                  tmpError = Chunk_update(&archiveEntryInfo->image.chunkImageData.info);
                  if (tmpError != ERROR_NONE) error = tmpError;
                }

                // close chunks
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
                // store in index database
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

            // free resources
            Crypt_done(&archiveEntryInfo->image.cryptInfo);
            Compress_delete(&archiveEntryInfo->image.byteCompressInfo);
            Compress_delete(&archiveEntryInfo->image.deltaCompressInfo);
            Chunk_done(&archiveEntryInfo->image.chunkImageData.info);
            Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
            Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info);
            Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
            Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
            free(archiveEntryInfo->image.buffer);
            Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->image.chunkImage.info);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          {
            if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
            {
              // close chunks
              tmpError = Chunk_close(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              tmpError = Chunk_close(&archiveEntryInfo->directory.chunkDirectory.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

              if (   (archiveEntryInfo->archiveInfo->databaseHandle != NULL)
                  && !archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag
                  && !archiveEntryInfo->archiveInfo->jobOptions->noStorageFlag
                 )
              {
                // store in index database
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

            // free resources
            Chunk_done(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
            Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          {
            if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
            {
              // close chunks
              tmpError = Chunk_close(&archiveEntryInfo->link.chunkLinkEntry.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              tmpError = Chunk_close(&archiveEntryInfo->link.chunkLink.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

              if (   (archiveEntryInfo->archiveInfo->databaseHandle != NULL)
                  && !archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag
                  && !archiveEntryInfo->archiveInfo->jobOptions->noStorageFlag
                 )
              {
                // store in index database
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

            // free resources
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
              // flush data compress
              error = Compress_flush(&archiveEntryInfo->hardLink.byteCompressInfo);
              if (error == ERROR_NONE)
              {
                do
                {
                  error = Compress_getAvailableCompressedBlocks(&archiveEntryInfo->hardLink.byteCompressInfo,
                                                                COMPRESS_BLOCK_TYPE_ANY,
                                                                &blockCount
                                                               );
                  if (error == ERROR_NONE)
                  {
                    if (blockCount > 0)
                    {
                     error = writeHardLinkDataBlock(archiveEntryInfo,BLOCK_MODE_WRITE,FALSE);
                    }
                  }
                }
                while (   (error == ERROR_NONE)
                       && (blockCount > 0)
                      );
                if (error == ERROR_NONE)
                {
                  error = writeHardLinkDataBlock(archiveEntryInfo,BLOCK_MODE_FLUSH,FALSE);
                }
              }

              // update file and chunks if header is written
              if (archiveEntryInfo->hardLink.headerWrittenFlag)
              {
                // update part size
                archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize = Compress_getInputLength(&archiveEntryInfo->hardLink.byteCompressInfo);
                if (error == ERROR_NONE)
                {
                  tmpError = Chunk_update(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
                  if (tmpError != ERROR_NONE) error = tmpError;
                }

                // close chunks
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
                // store in index database
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

            // free resources
            Crypt_done(&archiveEntryInfo->hardLink.cryptInfo);
            Compress_delete(&archiveEntryInfo->hardLink.byteCompressInfo);
            Compress_delete(&archiveEntryInfo->hardLink.deltaCompressInfo);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
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
              // close chunks
              tmpError = Chunk_close(&archiveEntryInfo->special.chunkSpecialEntry.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              tmpError = Chunk_close(&archiveEntryInfo->special.chunkSpecial.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

              if (   (archiveEntryInfo->archiveInfo->databaseHandle != NULL)
                  && !archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag
                  && !archiveEntryInfo->archiveInfo->jobOptions->noStorageFlag
                 )
              {
                // store in index database
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

            // free resources
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
            // close chunks
            tmpError = Chunk_close(&archiveEntryInfo->file.chunkFileData.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            if (Compress_isCompressed(archiveEntryInfo->file.deltaCompressAlgorithm))
            {
              tmpError = Chunk_close(&archiveEntryInfo->file.chunkFileDelta.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            }
            tmpError = Chunk_close(&archiveEntryInfo->file.chunkFileEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->file.chunkFile.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            // free resources
            Compress_delete(&archiveEntryInfo->file.deltaCompressInfo);
            if (archiveEntryInfo->file.deltaSourceInit)
            {
              Source_closeEntry(&archiveEntryInfo->file.sourceEntryInfo);
            }
            Chunk_done(&archiveEntryInfo->file.chunkFileData.info);
            Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info);
            Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);
            Crypt_done(&archiveEntryInfo->file.cryptInfo);
            Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
            Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
            Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);
            Compress_delete(&archiveEntryInfo->file.byteCompressInfo);
            free(archiveEntryInfo->file.buffer);
            Chunk_done(&archiveEntryInfo->file.chunkFile.info);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            // close chunks
            tmpError = Chunk_close(&archiveEntryInfo->image.chunkImageData.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            if (Compress_isCompressed(archiveEntryInfo->image.deltaCompressAlgorithm))
            {
              tmpError = Chunk_close(&archiveEntryInfo->image.chunkImageDelta.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            }
            tmpError = Chunk_close(&archiveEntryInfo->image.chunkImageEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->image.chunkImage.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            // free resources
            Compress_delete(&archiveEntryInfo->image.deltaCompressInfo);
            if (archiveEntryInfo->image.deltaSourceInit)
            {
              Source_closeEntry(&archiveEntryInfo->image.sourceEntryInfo);
            }
            Chunk_done(&archiveEntryInfo->image.chunkImageData.info);
            Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info);
            Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);
            Crypt_done(&archiveEntryInfo->image.cryptInfo);
            Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
            Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
            Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);
            Compress_delete(&archiveEntryInfo->image.byteCompressInfo);
            free(archiveEntryInfo->image.buffer);
            Chunk_done(&archiveEntryInfo->image.chunkImage.info);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          {
            // close chunks
            tmpError = Chunk_close(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->directory.chunkDirectory.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            // free resources
            Chunk_done(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
            Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          {
            // close chunks
            tmpError = Chunk_close(&archiveEntryInfo->link.chunkLinkEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->link.chunkLink.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            // free resources
            Chunk_done(&archiveEntryInfo->link.chunkLinkEntry.info);
            Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo);
            Chunk_done(&archiveEntryInfo->link.chunkLink.info);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          {
            ChunkHardLinkName *chunkHardLinkName;

            // close chunks
            tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            if (Compress_isCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm))
            {
              tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            }
            LIST_ITERATE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
            {
              tmpError = Chunk_close(&chunkHardLinkName->info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            }
            tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLink.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            // free resources
            Compress_delete(&archiveEntryInfo->hardLink.deltaCompressInfo);
            if (archiveEntryInfo->hardLink.deltaSourceInit)
            {
              Source_closeEntry(&archiveEntryInfo->hardLink.sourceEntryInfo);
            }
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
            LIST_DONE(&archiveEntryInfo->hardLink.chunkHardLinkNameList,chunkHardLinkName)
            {
              Chunk_done(&chunkHardLinkName->info);
              Crypt_done(&chunkHardLinkName->cryptInfo);
            }
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
            Crypt_done(&archiveEntryInfo->hardLink.cryptInfo);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);
            Compress_delete(&archiveEntryInfo->hardLink.byteCompressInfo);
            free(archiveEntryInfo->hardLink.buffer);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          {
            // close chunks
            tmpError = Chunk_close(&archiveEntryInfo->special.chunkSpecialEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->special.chunkSpecial.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            // free resources
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
  byte   deltaBuffer[1];
  ulong  deltaLength;
  bool   allowNewPartFlag;
  uint   blockCount;

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

      // write data block
      writtenBlockBytes = 0L;
      switch (archiveEntryInfo->archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          assert((archiveEntryInfo->file.bufferLength%archiveEntryInfo->blockLength) == 0);

          while (writtenBlockBytes < blockLength)
          {
            // do compress (delta+data)
            do
            {
              // check if compressed data is available
              error = Compress_getAvailableCompressedBlocks(&archiveEntryInfo->file.byteCompressInfo,
                                                            COMPRESS_BLOCK_TYPE_FULL,
                                                            &blockCount
                                                           );
              if (error != ERROR_NONE)
              {
                return error;
              }

              if (blockCount <= 0)
              {
                // compress delta
                error = Compress_deflate(&archiveEntryInfo->file.deltaCompressInfo,
                                         p+writtenBlockBytes,
                                         length-writtenBlockBytes,
                                         &deflatedBytes
                                        );
                if (error != ERROR_NONE)
                {
                  return error;
                }
                writtenBlockBytes += deflatedBytes;

                // get next delta-compressed byte
                Compress_getCompressedBlock(&archiveEntryInfo->file.deltaCompressInfo,
                                            deltaBuffer,
                                            &deltaLength
                                           );
                assert(deltaLength <= 1);
                if (deltaLength > 0)
                {
                  // compress data
                  error = Compress_deflate(&archiveEntryInfo->file.byteCompressInfo,
                                           deltaBuffer,
                                           1,
                                           NULL
                                          );
                  if (error != ERROR_NONE)
                  {
                    return error;
                  }
                }
              }
            }
            while (   (writtenBlockBytes < blockLength)
                   && (blockCount <= 0)
                  );

            // check if compressed data blocks are available and can be encrypted and written to file
            allowNewPartFlag = ((elementSize <= 1) || (writtenBlockBytes >= blockLength));
/* ???
Compress_getAvailableBlocks(&archiveEntryInfo->file.byteCompressInfo,
                                               COMPRESS_BLOCK_TYPE_FULL,
                                               &blockCount
                                              );
fprintf(stderr,"%s,%d: avild =%d\n",__FILE__,__LINE__,blockCount);
*/

            // write compressed data
            do
            {
              error = Compress_getAvailableCompressedBlocks(&archiveEntryInfo->file.byteCompressInfo,
                                                            COMPRESS_BLOCK_TYPE_FULL,
                                                            &blockCount
                                                           );
              if (error != ERROR_NONE)
              {
                return error;
              }
              if (blockCount > 0)
              {
                error = writeFileDataBlock(archiveEntryInfo,BLOCK_MODE_WRITE,allowNewPartFlag);
                if (error != ERROR_NONE)
                {
                  return error;
                }
              }
            }
            while (blockCount > 0);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          assert((archiveEntryInfo->image.bufferLength%archiveEntryInfo->blockLength) == 0);

          while (writtenBlockBytes < blockLength)
          {
            // compress
            error = Compress_deflate(&archiveEntryInfo->image.byteCompressInfo,
                                     p+writtenBlockBytes,
                                     blockLength-writtenBlockBytes,
                                     &deflatedBytes
                                    );
            if (error != ERROR_NONE)
            {
              return error;
            }
            writtenBlockBytes += deflatedBytes;

            // check if compressed data blocks are available and can be encrypted and written to file
            allowNewPartFlag = ((elementSize <= 1) || (writtenBlockBytes >= blockLength));
            do
            {
              error = Compress_getAvailableCompressedBlocks(&archiveEntryInfo->image.byteCompressInfo,
                                                            COMPRESS_BLOCK_TYPE_FULL,
                                                            &blockCount
                                                           );
              if (error != ERROR_NONE)
              {
                return error;
              }

              if (blockCount > 0)
              {
                error = writeImageDataBlock(archiveEntryInfo,BLOCK_MODE_WRITE,allowNewPartFlag);
                if (error != ERROR_NONE)
                {
                  return error;
                }
              }
            }
            while (blockCount > 0);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          assert((archiveEntryInfo->hardLink.bufferLength%archiveEntryInfo->blockLength) == 0);

          while (writtenBlockBytes < blockLength)
          {
            // compress
            error = Compress_deflate(&archiveEntryInfo->hardLink.byteCompressInfo,
                                     p+writtenBlockBytes,
                                     length-writtenBlockBytes,
                                     &deflatedBytes
                                    );
            if (error != ERROR_NONE)
            {
              return error;
            }
            writtenBlockBytes += deflatedBytes;

            // check if compressed data blocks are available and can be encrypted and written to file
            allowNewPartFlag = ((elementSize <= 1) || (writtenBlockBytes >= blockLength));
            do
            {
              error = Compress_getAvailableCompressedBlocks(&archiveEntryInfo->hardLink.byteCompressInfo,
                                                            COMPRESS_BLOCK_TYPE_FULL,
                                                            &blockCount
                                                           );
              if (error != ERROR_NONE)
              {
                return error;
              }

              if (blockCount > 0)
              {
                error = writeHardLinkDataBlock(archiveEntryInfo,BLOCK_MODE_WRITE,allowNewPartFlag);
                if (error != ERROR_NONE)
                {
                  return error;
                }
              }
            }
            while (blockCount > 0);
          }
          break;
        default:
          HALT_INTERNAL_ERROR("write data not supported for entry type");
          break;
      }

      // next data
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
  ulong  availableBytes;
  byte   deltaBuffer[1];
  ulong  inflatedBytes;

  assert(archiveEntryInfo != NULL);

  switch (archiveEntryInfo->archiveEntryType)
  {
    case ARCHIVE_ENTRY_TYPE_FILE:
      if (   Compress_isCompressed(archiveEntryInfo->file.deltaCompressAlgorithm)
          && !archiveEntryInfo->file.deltaSourceInit
         )
      {
        // get source for delta-compression
        error = Source_openEntry(&archiveEntryInfo->file.sourceEntryInfo,
                                 archiveEntryInfo->file.chunkFileDelta.name,
                                 archiveEntryInfo->archiveInfo->jobOptions,
                                 archiveEntryInfo->file.chunkFileEntry.name
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }

        archiveEntryInfo->file.deltaSourceInit = TRUE;
      }

      // read data: decrypt+decompress (delta/data)
      p = (byte*)buffer;
      while (length > 0)
      {
        // get number of available bytes in delta-decompressor
        error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->file.deltaCompressInfo,
                                                       &availableBytes
                                                      );
        if (error != ERROR_NONE)
        {
          return error;
        }

        if (availableBytes <= 0)
        {
          // no data in delta-decompressor -> do data decompress and fill delta-compressor

          // get number of available bytes in data decompressor
          error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->file.byteCompressInfo,
                                                         &availableBytes
                                                        );
          if (error != ERROR_NONE)
          {
            return error;
          }
          if (availableBytes <= 0)
          {
            // fill data-decompressor
            if (!Compress_isFlush(&archiveEntryInfo->file.byteCompressInfo))
            {
              do
              {
                error = readFileDataBlock(archiveEntryInfo);
//fprintf(stderr,"%s, %d: flush=%d size=%d index=%d\n",__FILE__,__LINE__,Compress_isFlush(&archiveEntryInfo->file.byteCompressInfo),
//archiveEntryInfo->file.chunkFileData.info.size,archiveEntryInfo->file.chunkFileData.info.index);
                if (error != ERROR_NONE)
                {
                  return error;
                }
                error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->file.byteCompressInfo,
                                                               &availableBytes
                                                              );
                if (error != ERROR_NONE)
                {
                  return error;
                }
//fprintf(stderr,"%s, %d: availableBytes=%d\n",__FILE__,__LINE__,availableBytes);
              }
              while (   !Compress_isFlush(&archiveEntryInfo->file.byteCompressInfo)
                     && (availableBytes <= 0L)
                    );
            }
          }

//fprintf(stderr,"%s, %d: availableBytes=%d flushed=%d end=%d\n",__FILE__,__LINE__,availableBytes,Compress_isFlush(&archiveEntryInfo->file.byteCompressInfo),Compress_isEndOfData(&archiveEntryInfo->file.byteCompressInfo));
          if      (availableBytes > 0L)
          {
            // decompress next byte in data-compressor
            error = Compress_inflate(&archiveEntryInfo->file.byteCompressInfo,
                                     deltaBuffer,
                                     1,
                                     &inflatedBytes
                                    );
            if (error != ERROR_NONE)
            {
              return error;
            }
            if (inflatedBytes > 0)
            {
              // put byte into delta-decompressor
              Compress_putCompressedBlock(&archiveEntryInfo->file.deltaCompressInfo,
                                          deltaBuffer,
                                          1
                                         );
            }
          }
          else if (Compress_isEndOfData(&archiveEntryInfo->file.byteCompressInfo))
          {
            // no more bytes in data-compressor -> flush delta-compressor
            error = Compress_flush(&archiveEntryInfo->file.deltaCompressInfo);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
        }

        // decompress delta
        error = Compress_inflate(&archiveEntryInfo->file.deltaCompressInfo,
                                 p,
                                 length,
                                 &inflatedBytes
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (inflatedBytes > 0)
        {
          // got decompressed data
          p += inflatedBytes;
          length -= inflatedBytes;
        }
      }
      break;
    case ARCHIVE_ENTRY_TYPE_IMAGE:
      if (   Compress_isCompressed(archiveEntryInfo->image.deltaCompressAlgorithm)
          && !archiveEntryInfo->image.deltaSourceInit
         )
      {
        // get source for delta-compression
        error = Source_openEntry(&archiveEntryInfo->image.sourceEntryInfo,
                                 archiveEntryInfo->image.chunkImageDelta.name,
                                 archiveEntryInfo->archiveInfo->jobOptions,
                                 archiveEntryInfo->image.chunkImageEntry.name
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }

        archiveEntryInfo->image.deltaSourceInit = TRUE;
      }

      // read data: decrypt+decompress (delta/data)
      p = (byte*)buffer;
      while (length > 0L)
      {
        // get number of available bytes in delta-decompressor
        error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->image.deltaCompressInfo,
                                                       &availableBytes
                                                      );
        if (error != ERROR_NONE)
        {
          return error;
        }

        if (availableBytes <= 0)
        {
          // no data in delta-decompressor -> do data decompress and fill delta-compressor

          // get number of available bytes in data decompressor
          error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->image.byteCompressInfo,
                                                         &availableBytes
                                                        );
          if (error != ERROR_NONE)
          {
            return error;
          }
          if (availableBytes <= 0)
          {
            // fill data-decompressor
            if (!Compress_isFlush(&archiveEntryInfo->image.byteCompressInfo))
            {
              do
              {
                error = readImageDataBlock(archiveEntryInfo);
//fprintf(stderr,"%s, %d: flush=%d size=%d index=%d\n",__FILE__,__LINE__,Compress_isFlush(&archiveEntryInfo->image.byteCompressInfo),
//archiveEntryInfo->image.chunkImageData.info.size,archiveEntryInfo->image.chunkImageData.info.index);
                if (error != ERROR_NONE)
                {
                  return error;
                }
                error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->image.byteCompressInfo,
                                                               &availableBytes
                                                              );
                if (error != ERROR_NONE)
                {
                  return error;
                }
//fprintf(stderr,"%s, %d: availableBytes=%d\n",__FILE__,__LINE__,availableBytes);
              }
              while (   !Compress_isFlush(&archiveEntryInfo->image.byteCompressInfo)
                     && (availableBytes <= 0L)
                    );
            }
          }

//fprintf(stderr,"%s, %d: availableBytes=%d flushed=%d end=%d\n",__FILE__,__LINE__,availableBytes,Compress_isFlush(&archiveEntryInfo->image.byteCompressInfo),Compress_isEndOfData(&archiveEntryInfo->image.byteCompressInfo));
          if      (availableBytes > 0L)
          {
            // decompress next byte in data-compressor
            error = Compress_inflate(&archiveEntryInfo->image.byteCompressInfo,
                                     deltaBuffer,
                                     1,
                                     &inflatedBytes
                                    );
            if (error != ERROR_NONE)
            {
              return error;
            }
            if (inflatedBytes > 0)
            {
              // put byte into delta-decompressor
              Compress_putCompressedBlock(&archiveEntryInfo->image.deltaCompressInfo,
                                          deltaBuffer,
                                          1
                                         );
            }
          }
          else if (Compress_isEndOfData(&archiveEntryInfo->image.byteCompressInfo))
          {
            // no more bytes in data-compressor -> flush delta-compressor
            error = Compress_flush(&archiveEntryInfo->image.deltaCompressInfo);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
        }

        // decompress delta
        error = Compress_inflate(&archiveEntryInfo->image.deltaCompressInfo,
                                 p,
                                 length,
                                 &inflatedBytes
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (inflatedBytes > 0)
        {
          // got decompressed data
          p += inflatedBytes;
          length -= inflatedBytes;
        }
      }
      break;
    case ARCHIVE_ENTRY_TYPE_HARDLINK:
      if (   Compress_isCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm)
          && !archiveEntryInfo->hardLink.deltaSourceInit
         )
      {
        // get source for delta-compression
        if (List_empty(&archiveEntryInfo->hardLink.chunkHardLinkNameList))
        {
          return ERROR_DELTA_SOURCE_NOT_FOUND;
        }
        error = Source_openEntry(&archiveEntryInfo->hardLink.sourceEntryInfo,
                                 archiveEntryInfo->hardLink.chunkHardLinkDelta.name,
                                 archiveEntryInfo->archiveInfo->jobOptions,
                                 ((ChunkHardLinkName*)List_first(&archiveEntryInfo->hardLink.chunkHardLinkNameList))->name
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }

        archiveEntryInfo->hardLink.deltaSourceInit = TRUE;
      }

      // read data: decrypt+decompress (delta/data)
      p = (byte*)buffer;
      while (length > 0L)
      {
        // get number of available bytes in delta-decompressor
        error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->hardLink.deltaCompressInfo,
                                                       &availableBytes
                                                      );
        if (error != ERROR_NONE)
        {
          return error;
        }

        if (availableBytes <= 0)
        {
          // no data in delta-decompressor -> do data decompress and fill delta-compressor

          // get number of available bytes in data decompressor
          error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->hardLink.byteCompressInfo,
                                                         &availableBytes
                                                        );
          if (error != ERROR_NONE)
          {
            return error;
          }
          if (availableBytes <= 0)
          {
            // fill data-decompressor
            if (!Compress_isFlush(&archiveEntryInfo->hardLink.byteCompressInfo))
            {
              do
              {
                error = readHardLinkDataBlock(archiveEntryInfo);
//fprintf(stderr,"%s, %d: flush=%d size=%d index=%d\n",__FILE__,__LINE__,Compress_isFlush(&archiveEntryInfo->hardLink.byteCompressInfo),
//archiveEntryInfo->hardLink.chunkHardLinkData.info.size,archiveEntryInfo->hardLink.chunkHardLinkData.info.index);
                if (error != ERROR_NONE)
                {
                  return error;
                }
                error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->hardLink.byteCompressInfo,
                                                               &availableBytes
                                                              );
                if (error != ERROR_NONE)
                {
                  return error;
                }
//fprintf(stderr,"%s, %d: availableBytes=%d\n",__FILE__,__LINE__,availableBytes);
              }
              while (   !Compress_isFlush(&archiveEntryInfo->hardLink.byteCompressInfo)
                     && (availableBytes <= 0L)
                    );
            }
          }

//fprintf(stderr,"%s, %d: availableBytes=%d flushed=%d end=%d\n",__FILE__,__LINE__,availableBytes,Compress_isFlush(&archiveEntryInfo->hardLink.byteCompressInfo),Compress_isEndOfData(&archiveEntryInfo->hardLink.byteCompressInfo));
          if      (availableBytes > 0L)
          {
            // decompress next byte in data-compressor
            error = Compress_inflate(&archiveEntryInfo->hardLink.byteCompressInfo,
                                     deltaBuffer,
                                     1,
                                     &inflatedBytes
                                    );
            if (error != ERROR_NONE)
            {
              return error;
            }
            if (inflatedBytes > 0)
            {
              // put byte into delta-decompressor
              Compress_putCompressedBlock(&archiveEntryInfo->hardLink.deltaCompressInfo,
                                          deltaBuffer,
                                          1
                                         );
            }
          }
          else if (Compress_isEndOfData(&archiveEntryInfo->hardLink.byteCompressInfo))
          {
            // no more bytes in data-compressor -> flush delta-compressor
            error = Compress_flush(&archiveEntryInfo->hardLink.deltaCompressInfo);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
        }

        // decompress delta
        error = Compress_inflate(&archiveEntryInfo->hardLink.deltaCompressInfo,
                                 p,
                                 length,
                                 &inflatedBytes
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (inflatedBytes > 0)
        {
          // got decompressed data
          p += inflatedBytes;
          length -= inflatedBytes;
        }
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
  uint64 offset;

  assert(archiveInfo != NULL);

  offset = 0LL;
  if (!archiveInfo->jobOptions->dryRunFlag)
  {
    switch (archiveInfo->ioType)
    {
      case ARCHIVE_IO_TYPE_FILE:
        if (archiveInfo->file.openFlag)
        {
          archiveInfo->chunkIO->tell(archiveInfo->chunkIOUserData,&offset);
        }
        else
        {
          offset = 0LL;
        }
        break;
      case ARCHIVE_IO_TYPE_STORAGE_FILE:
        archiveInfo->chunkIO->tell(archiveInfo->chunkIOUserData,&offset);
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          return 0LL; /* not reached */
          break; /* not reached */
      #endif /* NDEBUG */
    }
  }

  return offset;
}

Errors Archive_seek(ArchiveInfo *archiveInfo,
                    uint64      offset
                   )
{
  Errors error;

  assert(archiveInfo != NULL);

  if (!archiveInfo->jobOptions->dryRunFlag)
  {
    switch (archiveInfo->ioType)
    {
      case ARCHIVE_IO_TYPE_FILE:
        if (archiveInfo->file.openFlag)
        {
          error = archiveInfo->chunkIO->seek(archiveInfo->chunkIOUserData,offset);
        }
        break;
      case ARCHIVE_IO_TYPE_STORAGE_FILE:
        error = archiveInfo->chunkIO->seek(archiveInfo->chunkIOUserData,offset);
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
  }

  return error;
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

  // create new index
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

  // add index
  error = Archive_updateIndex(databaseHandle,
                              storageId,
                              storageName,
                              cryptPassword,
                              cryptPrivateKeyFileName,
                              NULL,
                              NULL,
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

Errors Archive_updateIndex(DatabaseHandle               *databaseHandle,
                           int64                        storageId,
                           const String                 storageName,
                           Password                     *cryptPassword,
                           String                       cryptPrivateKeyFileName,
                           ArchivePauseCallbackFunction pauseCallback,
                           void                         *pauseUserData,
                           ArchiveAbortCallbackFunction abortCallback,
                           void                         *abortUserData
                          )
{
  Errors            error;
  JobOptions        jobOptions;
  ArchiveInfo       archiveInfo;
  ArchiveEntryInfo  archiveEntryInfo;
  ArchiveEntryTypes archiveEntryType;

  assert(databaseHandle != NULL);
  assert(storageName != NULL);

  // init job options
  initJobOptions(&jobOptions);
  jobOptions.cryptPassword           = Password_duplicate(cryptPassword);
  jobOptions.cryptPrivateKeyFileName = String_duplicate(cryptPrivateKeyFileName);

  // open archive
  error = Archive_open(&archiveInfo,
                       storageName,
                       &jobOptions,
                       NULL,
                       NULL
                      );
  if (error != ERROR_NONE)
  {
    freeJobOptions(&jobOptions);
    return error;
  }

  // clear index
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

  // index archive contents
  Index_setState(databaseHandle,
                 storageId,
                 INDEX_STATE_UPDATE,
                 0LL,
                 NULL
                );
  error = ERROR_NONE;
  while (   !Archive_eof(&archiveInfo,FALSE)
         && ((abortCallback == NULL) || !abortCallback(abortUserData))
         && (error == ERROR_NONE)
        )
  {
    // pause
    while ((pauseCallback != NULL) && pauseCallback(pauseUserData))
    {
      Misc_udelay(5000*1000);
    }

    // get next file type
    error = Archive_getNextArchiveEntryType(&archiveInfo,
                                            &archiveEntryType,
                                            FALSE
                                           );
    if (error == ERROR_NONE)
    {
      // read entry
      switch (archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          {
            ArchiveEntryInfo archiveEntryInfo;
            String           fileName;
            FileInfo         fileInfo;
            uint64           fragmentOffset,fragmentSize;

            // open archive file
            fileName = String_new();
            error = Archive_readFileEntry(&archiveInfo,
                                          &archiveEntryInfo,
//???
//NULL,
//NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          fileName,
                                          &fileInfo,
                                          NULL,
                                          &fragmentOffset,
                                          &fragmentSize
                                         );
            if (error != ERROR_NONE)
            {
              String_delete(fileName);
              break;
            }

            // add to index database
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

            // close archive file, free resources
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(fileName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            String           deviceName;
            ArchiveEntryInfo archiveEntryInfo;
            DeviceInfo       deviceInfo;
            uint64           blockOffset,blockCount;

            // open archive file
            deviceName = String_new();
            error = Archive_readImageEntry(&archiveInfo,
                                           &archiveEntryInfo,
//???
//NULL,
//NULL,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL,
                                           deviceName,
                                           &deviceInfo,
                                           NULL,
                                           &blockOffset,
                                           &blockCount
                                          );
            if (error != ERROR_NONE)
            {
              String_delete(deviceName);
              break;
            }

            // add to index database
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

            // close archive file, free resources
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(deviceName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          {
            String   directoryName;
            FileInfo fileInfo;

            // open archive directory
            directoryName = String_new();
            error = Archive_readDirectoryEntry(&archiveInfo,
                                               &archiveEntryInfo,
                                               NULL,
                                               NULL,
                                               directoryName,
                                               &fileInfo
                                              );
            if (error != ERROR_NONE)
            {
              String_delete(directoryName);
              break;
            }

            // add to index database
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

            // close archive file, free resources
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(directoryName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          {
            String   linkName;
            String   destinationName;
            FileInfo fileInfo;

            // open archive link
            linkName        = String_new();
            destinationName = String_new();
            error = Archive_readLinkEntry(&archiveInfo,
                                          &archiveEntryInfo,
                                          NULL,
                                          NULL,
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

            // add to index database
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

            // close archive file, free resources
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(destinationName);
            String_delete(linkName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          {
            StringList       fileNameList;
            ArchiveEntryInfo archiveEntryInfo;
            FileInfo         fileInfo;
            uint64           fragmentOffset,fragmentSize;
            const StringNode *stringNode;
            String           fileName;

            // open archive file
            StringList_init(&fileNameList);
            error = Archive_readHardLinkEntry(&archiveInfo,
                                          &archiveEntryInfo,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &fileNameList,
                                          &fileInfo,
                                          NULL,
                                          &fragmentOffset,
                                          &fragmentSize
                                         );
            if (error != ERROR_NONE)
            {
              StringList_done(&fileNameList);
              break;
            }
//fprintf(stderr,"%s,%d: index update %s\n",__FILE__,__LINE__,String_cString(name));

            // add to index database
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

            // close archive file, free resources
            Archive_closeEntry(&archiveEntryInfo);
            StringList_done(&fileNameList);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          {
            String   fileName;
            FileInfo fileInfo;

            // open archive link
            fileName = String_new();
            error = Archive_readSpecialEntry(&archiveInfo,
                                             &archiveEntryInfo,
                                             NULL,
                                             NULL,
                                             fileName,
                                             &fileInfo
                                            );
            if (error != ERROR_NONE)
            {
              String_delete(fileName);
              break;
            }

            // add to index database
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

            // close archive file, free resources
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

      // update temporary size (ignore error)
      Index_update(databaseHandle,
                   storageId,
                   NULL,
                   Archive_tell(&archiveInfo)
                  );
    }
  }
  if      (error == ERROR_NONE)
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
    Archive_close(&archiveInfo);
    if (Errors_getCode(error) == ERROR_NO_CRYPT_PASSWORD)
    {
      Index_setState(databaseHandle,
                     storageId,
                     INDEX_STATE_UPDATE_REQUESTED,
                     0LL,
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
    freeJobOptions(&jobOptions);
    return error;
  }

  // update name/size
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

  // close archive
  Archive_close(&archiveInfo);

  // free resources
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

#if 0
Errors Archive_copy(const String                    storageName,
                    JobOptions                      *jobOptions,
                    ArchiveGetCryptPasswordFunction archiveGetCryptPassword,
                    void                            *archiveGetCryptPasswordData,
                    const String                    newStorageName
                   )
{
  Errors      error;
  ArchiveInfo sourceArchiveInfo;
  ArchiveInfo destinationArchiveInfo;
  Errors      failError;

  // open source
  error = Archive_open(&sourceArchiveInfo,
                       storageName,
                       jobOptions,
                       archiveGetCryptPassword,
                       archiveGetCryptPasswordData
                      )
  if (error != ERROR_NONE)
  {
    return error;
  }

  // create destination
  error = Archive_create(&destinationArchiveInfo,
                         jobOptions,
                         ArchiveNewFileFunction          archiveNewFileFunction,
                         void                            *archiveNewFileUserData,
                         NULL,
                         NULL,
                         NULL,
                        );
  if (error != ERROR_NONE)
  {
    Archive_close(&sourceArchiveInfo);
    return error;
  }

  // copy archive entries
  failError = ERROR_NONE;
  while (   //((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
         && !Archive_eof(&sourceArchiveInfo,TRUE)
         && (failError == ERROR_NONE)
        )
  {
#if 0
    // pause
    while ((restoreInfo.pauseFlag != NULL) && (*restoreInfo.pauseFlag))
    {
      Misc_udelay(500*1000);
    }
#endif /* 0 */

    // get next archive entry type
    error = Archive_getNextArchiveEntryType(&archiveInfo,
                                            &archiveEntryType,
                                            TRUE
                                           );
    if (error != ERROR_NONE)
    {
      if (failError == ERROR_NONE) failError = error;
      break;
    }

    switch (archiveEntryType)
    {
      case ARCHIVE_ENTRY_TYPE_FILE:
        {
          String       fileName;
          FileInfo     fileInfo;
          uint64       fragmentOffset,fragmentSize;
          String       destinationFileName;
          FragmentNode *fragmentNode;
          String       parentDirectoryName;
          FileHandle   fileHandle;
          uint64       length;
          ulong        n;

          // read file
          fileName = String_new();
          error = Archive_readFileEntry(&archiveInfo,
                                        &archiveEntryInfo,
                                        NULL,
                                        NULL,
                                        NULL,
                                        fileName,
                                        &fileInfo,
                                        &fragmentOffset,
                                        &fragmentSize
                                       );
          if (error != ERROR_NONE)
          {
            String_delete(fileName);
            if (failError == ERROR_NONE) failError = error;
            continue;
          }

          // check if file fragment already exists, file already exists
          fragmentNode = FragmentList_find(&fragmentList,destinationFileName);
          if (fragmentNode != NULL)
          {
            if (!jobOptions->overwriteFilesFlag && FragmentList_checkEntryExists(fragmentNode,fragmentOffset,fragmentSize))
            {
              printInfo(1,
                        "  Restore file '%s'...skipped (file part %llu..%llu exists)\n",
                        String_cString(destinationFileName),
                        fragmentOffset,
                        (fragmentSize > 0LL)?fragmentOffset+fragmentSize-1:fragmentOffset
                       );
              String_delete(destinationFileName);
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              continue;
            }
          }
          else
          {
            fragmentNode = FragmentList_add(&fragmentList,destinationFileName,fileInfo.size,&fileInfo,sizeof(FileInfo));
          }

          // copy file entry

            // seek to fragment position
            error = File_seek(&fileHandle,fragmentOffset);
            if (error != ERROR_NONE)
            {
              printInfo(2,"FAIL!\n");
              printError("Cannot write file '%s' (error: %s)\n",
                         String_cString(destinationFileName),
                         Errors_getText(error)
                        );
              File_close(&fileHandle);
              String_delete(destinationFileName);
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              if (jobOptions->stopOnErrorFlag)
              {
                restoreInfo.failError = error;
              }
              continue;
            }
          }

          // write file data
          length = 0;
          while (   //((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
                 && (length < fragmentSize)
                )
          {
#if 0
            // pause
            while ((restoreInfo.pauseFlag != NULL) && (*restoreInfo.pauseFlag))
            {
              Misc_udelay(500*1000);
            }
#endif /* 0 */

            n = MIN(fragmentSize-length,BUFFER_SIZE);

            error = Archive_readData(&archiveEntryInfo,buffer,n);
            if (error != ERROR_NONE)
            {
              printInfo(2,"FAIL!\n");
              printError("Cannot read content of archive '%s' (error: %s)!\n",
                         String_cString(printableArchiveName),
                         Errors_getText(error)
                        );
              if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
              break;
            }

            error = File_write(&fileHandle,buffer,n);
            if (error != ERROR_NONE)
            {
              printInfo(2,"FAIL!\n");
              printError("Cannot write file '%s' (error: %s)\n",
                         String_cString(destinationFileName),
                         Errors_getText(error)
                        );
              if (jobOptions->stopOnErrorFlag)
              {
                restoreInfo.failError = error;
              }
              break;
            }
//              abortFlag = !updateStatusInfo(&restoreInfo);

            length += n;
          }
          if      (failError != ERROR_NONE)
          {
            if (!jobOptions->dryRunFlag)
            {
              File_close(&fileHandle);
            }
            String_delete(destinationFileName);
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(fileName);
            continue;
          }
#if 0
          else if ((restoreInfo.requestedAbortFlag != NULL) && (*restoreInfo.requestedAbortFlag))
          {
            printInfo(2,"ABORTED\n");
            if (!jobOptions->dryRunFlag)
            {
              File_close(&fileHandle);
            }
            String_delete(destinationFileName);
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(fileName);
            continue;
          }
#endif /* 0 */

          // add fragment to file fragment list
          FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);
//FragmentList_debugPrintInfo(fragmentNode,String_cString(fileName));

          // close file
          if (!jobOptions->dryRunFlag)
          {
            File_close(&fileHandle);
          }

          // close source archive file, free resources
          (void)Archive_closeEntry(&sourceArchiveEntryInfo);

          // free resources
          String_delete(fileName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        {
          String       imageName;
          DeviceInfo   deviceInfo;
          uint64       blockOffset,blockCount;
          String       destinationDeviceName;
          FragmentNode *fragmentNode;
          DeviceHandle deviceHandle;
          uint64       block;
          ulong        bufferBlockCount;

          // read image
          imageName = String_new();
          error = Archive_readImageEntry(&archiveInfo,
                                         &archiveEntryInfo,
                                         NULL,
//                                         NULL,
//                                         NULL,
                                         imageName,
                                         &deviceInfo,
                                         NULL,
                                         &blockOffset,
                                         &blockCount
                                        );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'image' content of archive '%s' (error: %s)!\n",
                       String_cString(printableArchiveName),
                       Errors_getText(error)
                      );
            String_delete(imageName);
            if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
            break;
          }

          if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,imageName,PATTERN_MATCH_MODE_EXACT))
              && ((excludePatternList == NULL) || !PatternList_match(excludePatternList,imageName,PATTERN_MATCH_MODE_EXACT))
             )
          {
            String_set(restoreInfo.statusInfo.name,imageName);
            restoreInfo.statusInfo.entryDoneBytes  = 0LL;
            restoreInfo.statusInfo.entryTotalBytes = blockCount;
            abortFlag = !updateStatusInfo(&restoreInfo);

            // get destination filename
            destinationDeviceName = getDestinationDeviceName(String_new(),
                                                             imageName,
                                                             jobOptions->destination
                                                            );


            // check if image fragment exists
            fragmentNode = FragmentList_find(&fragmentList,imageName);
            if (fragmentNode != NULL)
            {
              if (!jobOptions->overwriteFilesFlag && FragmentList_checkEntryExists(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize))
              {
                printInfo(1,
                          "  Restore image '%s'...skipped (image part %llu..%llu exists)\n",
                          String_cString(destinationDeviceName),
                          blockOffset*(uint64)deviceInfo.blockSize,
                          ((blockCount > 0)?blockOffset+blockCount-1:blockOffset)*(uint64)deviceInfo.blockSize
                         );
                String_delete(destinationDeviceName);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(imageName);
                continue;
              }
            }
            else
            {
              fragmentNode = FragmentList_add(&fragmentList,imageName,deviceInfo.size,NULL,0);
            }

            printInfo(2,"  Restore image '%s'...",String_cString(destinationDeviceName));

            // open device
            if (!jobOptions->dryRunFlag)
            {
              error = Device_open(&deviceHandle,destinationDeviceName,DEVICE_OPENMODE_WRITE);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot write to device '%s' (error: %s)\n",
                           String_cString(destinationDeviceName),
                           Errors_getText(error)
                          );
                String_delete(destinationDeviceName);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(imageName);
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo.failError = error;
                }
                continue;
              }
              error = Device_seek(&deviceHandle,blockOffset*(uint64)deviceInfo.blockSize);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot write to device '%s' (error: %s)\n",
                           String_cString(destinationDeviceName),
                           Errors_getText(error)
                          );
                Device_close(&deviceHandle);
                String_delete(destinationDeviceName);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(imageName);
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo.failError = error;
                }
                continue;
              }
            }

            // write image data
            block = 0;
            while (   ((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
                   && (block < blockCount)
                  )
            {
              // pause
              while ((restoreInfo.pauseFlag != NULL) && (*restoreInfo.pauseFlag))
              {
                Misc_udelay(500*1000);
              }

              assert(deviceInfo.blockSize > 0);
              bufferBlockCount = MIN(blockCount-block,BUFFER_SIZE/deviceInfo.blockSize);

              error = Archive_readData(&archiveEntryInfo,buffer,bufferBlockCount*deviceInfo.blockSize);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot read content of archive '%s' (error: %s)!\n",
                           String_cString(printableArchiveName),
                           Errors_getText(error)
                          );
                if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
                break;
              }
              if (!jobOptions->dryRunFlag)
              {
                error = Device_write(&deviceHandle,buffer,bufferBlockCount*deviceInfo.blockSize);
                if (error != ERROR_NONE)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot write to device '%s' (error: %s)\n",
                             String_cString(destinationDeviceName),
                             Errors_getText(error)
                            );
                  if (jobOptions->stopOnErrorFlag)
                  {
                    restoreInfo.failError = error;
                  }
                  break;
                }
              }
              restoreInfo.statusInfo.entryDoneBytes += bufferBlockCount*deviceInfo.blockSize;
              abortFlag = !updateStatusInfo(&restoreInfo);

              block += (uint64)bufferBlockCount;
            }
            if (!jobOptions->dryRunFlag)
            {
              Device_close(&deviceHandle);
              if ((restoreInfo.requestedAbortFlag != NULL) && (*restoreInfo.requestedAbortFlag))
              {
                printInfo(2,"ABORTED\n");
                String_delete(destinationDeviceName);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(imageName);
                continue;
              }
            }

            // add fragment to file fragment list
            FragmentList_addEntry(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize);
//FragmentList_debugPrintInfo(fragmentNode,String_cString(fileName));

            // discard fragment list if file is complete
            if (FragmentList_checkEntryComplete(fragmentNode))
            {
              FragmentList_discard(&fragmentList,fragmentNode);
            }

            if (!jobOptions->dryRunFlag)
            {
              printInfo(2,"ok\n");
            }
            else
            {
              printInfo(2,"ok (dry-run)\n");
            }

            /* check if all data read.
               Note: it is not possible to check if all data is read when
               compression is used. The decompressor may not all data even
               data is _not_ corrupt.
            */
            if (   !Compress_isCompressed(archiveEntryInfo.image.compressAlgorithm)
                && !Archive_eofData(&archiveEntryInfo))
            {
              printWarning("unexpected data at end of image entry '%S'.\n",imageName);
            }

            // free resources
            String_delete(destinationDeviceName);
          }
          else
          {
            // skip
            printInfo(3,"  Restore '%s'...skipped\n",String_cString(imageName));
          }

          // close archive file, free resources
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printWarning("close 'image' entry fail (error: %s)\n",Errors_getText(error));
          }

          // free resources
          String_delete(imageName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        {
          String   directoryName;
          FileInfo fileInfo;
          String   destinationFileName;
//            FileInfo localFileInfo;

          // read directory
          directoryName = String_new();
          error = Archive_readDirectoryEntry(&archiveInfo,
                                             &archiveEntryInfo,
                                             NULL,
                                             NULL,
                                             directoryName,
                                             &fileInfo
                                            );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'directory' content of archive '%s' (error: %s)!\n",
                       String_cString(printableArchiveName),
                       Errors_getText(error)
                      );
            String_delete(directoryName);
            if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
            break;
          }

          if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
              && ((excludePatternList == NULL) || !PatternList_match(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT))
             )
          {
            String_set(restoreInfo.statusInfo.name,directoryName);
            restoreInfo.statusInfo.entryDoneBytes  = 0LL;
            restoreInfo.statusInfo.entryTotalBytes = 00L;
            abortFlag = !updateStatusInfo(&restoreInfo);

            // get destination filename
            destinationFileName = getDestinationFileName(String_new(),
                                                         directoryName,
                                                         jobOptions->destination,
                                                         jobOptions->directoryStripCount
                                                        );


            // check if directory already exists
            if (!jobOptions->overwriteFilesFlag && File_exists(destinationFileName))
            {
              printInfo(1,
                        "  Restore directory '%s'...skipped (file exists)\n",
                        String_cString(destinationFileName)
                       );
              String_delete(destinationFileName);
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(directoryName);
              continue;
            }

            printInfo(2,"  Restore directory '%s'...",String_cString(destinationFileName));

            // create directory
            if (!jobOptions->dryRunFlag)
            {
              error = File_makeDirectory(destinationFileName,
                                         FILE_DEFAULT_USER_ID,
                                         FILE_DEFAULT_GROUP_ID,
                                         fileInfo.permission
                                        );
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot create directory '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           Errors_getText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(directoryName);
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo.failError = error;
                }
                continue;
              }
            }

            // set file time, file owner/group
            if (!jobOptions->dryRunFlag)
            {
              if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
              if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
              error = File_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                if (jobOptions->stopOnErrorFlag)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot set directory info of '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Errors_getText(error)
                            );
                  String_delete(destinationFileName);
                  Archive_closeEntry(&archiveEntryInfo);
                  String_delete(directoryName);
                  if (jobOptions->stopOnErrorFlag)
                  {
                    restoreInfo.failError = error;
                  }
                  continue;
                }
                else
                {
                  printWarning("Cannot set directory info of '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Errors_getText(error)
                              );
                }
              }
            }

            if (!jobOptions->dryRunFlag)
            {
              printInfo(2,"ok\n");
            }
            else
            {
              printInfo(2,"ok (dry-run)\n");
            }

            // check if all data read
            if (!Archive_eofData(&archiveEntryInfo))
            {
              printWarning("unexpected data at end of directory entry '%S'.\n",directoryName);
            }

            // free resources
            String_delete(destinationFileName);
          }
          else
          {
            // skip
            printInfo(3,"  Restore '%s'...skipped\n",String_cString(directoryName));
          }

          // close archive file
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printWarning("close 'directory' entry fail (error: %s)\n",Errors_getText(error));
          }

          // free resources
          String_delete(directoryName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_LINK:
        {
          String   linkName;
          String   fileName;
          FileInfo fileInfo;
          String   destinationFileName;
          String   parentDirectoryName;
//            FileInfo localFileInfo;

          // read link
          linkName = String_new();
          fileName = String_new();
          error = Archive_readLinkEntry(&archiveInfo,
                                        &archiveEntryInfo,
                                        NULL,
                                        NULL,
                                        linkName,
                                        fileName,
                                        &fileInfo
                                       );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'link' content of archive '%s' (error: %s)!\n",
                       String_cString(printableArchiveName),
                       Errors_getText(error)
                      );
            String_delete(fileName);
            String_delete(linkName);
            if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
            break;
          }

          if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
              && ((excludePatternList == NULL) || !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT))
             )
          {
            String_set(restoreInfo.statusInfo.name,linkName);
            restoreInfo.statusInfo.entryDoneBytes  = 0LL;
            restoreInfo.statusInfo.entryTotalBytes = 00L;
            abortFlag = !updateStatusInfo(&restoreInfo);

            // get destination filename
            destinationFileName = getDestinationFileName(String_new(),
                                                         linkName,
                                                         jobOptions->destination,
                                                         jobOptions->directoryStripCount
                                                        );

            // create parent directories if not existing
            if (!jobOptions->dryRunFlag)
            {
              parentDirectoryName = File_getFilePathName(String_new(),destinationFileName);
              if (!File_exists(parentDirectoryName))
              {
                // create directory
                error = File_makeDirectory(parentDirectoryName,
                                           FILE_DEFAULT_USER_ID,
                                           FILE_DEFAULT_GROUP_ID,
                                           FILE_DEFAULT_PERMISSION
                                          );
                if (error != ERROR_NONE)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot create directory '%s' (error: %s)\n",
                             String_cString(parentDirectoryName),
                             Errors_getText(error)
                            );
                  String_delete(parentDirectoryName);
                  String_delete(destinationFileName);
                  Archive_closeEntry(&archiveEntryInfo);
                  String_delete(fileName);
                  String_delete(linkName);
                  if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
                  continue;
                }

                // set directory owner ship
                error = File_setOwner(parentDirectoryName,
                                      (jobOptions->owner.userId != FILE_DEFAULT_USER_ID)?jobOptions->owner.userId:fileInfo.userId,
                                      (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID)?jobOptions->owner.groupId:fileInfo.groupId
                                     );
                if (error != ERROR_NONE)
                {
                  if (jobOptions->stopOnErrorFlag)
                  {
                    printInfo(2,"FAIL!\n");
                    printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                               String_cString(parentDirectoryName),
                               Errors_getText(error)
                              );
                    String_delete(parentDirectoryName);
                    String_delete(destinationFileName);
                    Archive_closeEntry(&archiveEntryInfo);
                    String_delete(fileName);
                    if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
                    continue;
                  }
                  else
                  {
                    printWarning("Cannot set owner ship of directory '%s' (error: %s)\n",
                                 String_cString(parentDirectoryName),
                                 Errors_getText(error)
                                );
                  }
                }
              }
              String_delete(parentDirectoryName);
            }

            // check if link areadly exists
            if (!jobOptions->overwriteFilesFlag && File_exists(destinationFileName))
            {
              printInfo(1,
                        "  Restore link '%s'...skipped (file exists)\n",
                        String_cString(destinationFileName)
                       );
              String_delete(destinationFileName);
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              String_delete(linkName);
              if (jobOptions->stopOnErrorFlag)
              {
                restoreInfo.failError = ERRORX(FILE_EXISTS,0,String_cString(destinationFileName));
              }
              continue;
            }

            printInfo(2,"  Restore link '%s'...",String_cString(destinationFileName));

            // create link
            if (!jobOptions->dryRunFlag)
            {
              error = File_makeLink(destinationFileName,fileName);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot create link '%s' -> '%s' (error: %s)\n",
                           String_cString(destinationFileName),
                           String_cString(fileName),
                           Errors_getText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo.failError = error;
                }
                continue;
              }
            }

            // set file time, file owner/group
            if (!jobOptions->dryRunFlag)
            {
              if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
              if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
              error = File_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                if (jobOptions->stopOnErrorFlag)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot set file info of '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Errors_getText(error)
                            );
                  String_delete(destinationFileName);
                  Archive_closeEntry(&archiveEntryInfo);
                  String_delete(fileName);
                  String_delete(linkName);
                  if (jobOptions->stopOnErrorFlag)
                  {
                    restoreInfo.failError = error;
                  }
                  continue;
                }
                else
                {
                  printWarning("Cannot set file info of '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Errors_getText(error)
                              );
                }
              }
            }

            if (!jobOptions->dryRunFlag)
            {
              printInfo(2,"ok\n");
            }
            else
            {
              printInfo(2,"ok (dry-run)\n");
            }

            // check if all data read
            if (!Archive_eofData(&archiveEntryInfo))
            {
              printWarning("unexpected data at end of link entry '%S'.\n",linkName);
            }

            // free resources
            String_delete(destinationFileName);
          }
          else
          {
            // skip
            printInfo(3,"  Restore '%s'...skipped\n",String_cString(linkName));
          }

          // close archive file
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printWarning("close 'link' entry fail (error: %s)\n",Errors_getText(error));
          }

          // free resources
          String_delete(fileName);
          String_delete(linkName);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        {
          StringList       fileNameList;
          FileInfo         fileInfo;
          uint64           fragmentOffset,fragmentSize;
          String           hardLinkFileName;
          String           destinationFileName;
          bool             restoredDataFlag;
          const StringNode *stringNode;
          String           fileName;
          FragmentNode     *fragmentNode;
          String           parentDirectoryName;
//            FileInfo         localFileInfo;
          FileHandle       fileHandle;
          uint64           length;
          ulong            n;

          // read hard link
          StringList_init(&fileNameList);
          error = Archive_readHardLinkEntry(&archiveInfo,
                                            &archiveEntryInfo,
                                            NULL,
                                            NULL,
                                            NULL,
                                            &fileNameList,
                                            &fileInfo,
                                            &fragmentOffset,
                                            &fragmentSize
                                           );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'hard link' content of archive '%s' (error: %s)!\n",
                       String_cString(printableArchiveName),
                       Errors_getText(error)
                      );
            StringList_done(&fileNameList);
            if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
            continue;
          }

          hardLinkFileName    = String_new();
          destinationFileName = String_new();
          restoredDataFlag    = FALSE;
          STRINGLIST_ITERATE(&fileNameList,stringNode,fileName)
          {
            if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                && ((excludePatternList == NULL) || !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
               )
            {
              String_set(restoreInfo.statusInfo.name,fileName);
              restoreInfo.statusInfo.entryDoneBytes  = 0LL;
              restoreInfo.statusInfo.entryTotalBytes = fragmentSize;
              abortFlag = !updateStatusInfo(&restoreInfo);

              // get destination filename
              getDestinationFileName(destinationFileName,
                                     fileName,
                                     jobOptions->destination,
                                     jobOptions->directoryStripCount
                                    );

              printInfo(2,"  Restore hard link '%s'...",String_cString(destinationFileName));

              // create parent directories if not existing
              if (!jobOptions->dryRunFlag)
              {
                parentDirectoryName = File_getFilePathName(String_new(),destinationFileName);
                if (!File_exists(parentDirectoryName))
                {
                  // create directory
                  error = File_makeDirectory(parentDirectoryName,
                                             FILE_DEFAULT_USER_ID,
                                             FILE_DEFAULT_GROUP_ID,
                                             FILE_DEFAULT_PERMISSION
                                            );
                  if (error != ERROR_NONE)
                  {
                    printInfo(2,"FAIL!\n");
                    printError("Cannot create directory '%s' (error: %s)\n",
                               String_cString(parentDirectoryName),
                               Errors_getText(error)
                              );
                    String_delete(parentDirectoryName);
                    if (jobOptions->stopOnErrorFlag)
                    {
                      restoreInfo.failError = error;
                      break;
                    }
                    else
                    {
                      continue;
                    }
                  }

                  // set directory owner ship
                  error = File_setOwner(parentDirectoryName,
                                        (jobOptions->owner.userId != FILE_DEFAULT_USER_ID)?jobOptions->owner.userId:fileInfo.userId,
                                        (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID)?jobOptions->owner.groupId:fileInfo.groupId
                                       );
                  if (error != ERROR_NONE)
                  {
                    if (jobOptions->stopOnErrorFlag)
                    {
                      printInfo(2,"FAIL!\n");
                      printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                                 String_cString(parentDirectoryName),
                                 Errors_getText(error)
                                );
                      String_delete(parentDirectoryName);
                      restoreInfo.failError = error;
                      break;
                    }
                    else
                    {
                      printWarning("Cannot set owner ship of directory '%s' (error: %s)\n",
                                   String_cString(parentDirectoryName),
                                   Errors_getText(error)
                                  );
                    }
                  }
                }
                String_delete(parentDirectoryName);
              }

              if (!restoredDataFlag)
              {
                // check if file fragment already eixsts, file already exists
                fragmentNode = FragmentList_find(&fragmentList,fileName);
                if (fragmentNode != NULL)
                {
                  if (!jobOptions->overwriteFilesFlag && FragmentList_checkEntryExists(fragmentNode,fragmentOffset,fragmentSize))
                  {
                    printInfo(2,"skipped (file part %llu..%llu exists)\n",
                              String_cString(destinationFileName),
                              fragmentOffset,
                              (fragmentSize > 0LL)?fragmentOffset+fragmentSize-1:fragmentOffset
                             );
                    continue;
                  }
                }
                else
                {
                  if (!jobOptions->overwriteFilesFlag && File_exists(destinationFileName))
                  {
                    printInfo(2,"skipped (file exists)\n",String_cString(destinationFileName));
                    continue;
                  }
                  fragmentNode = FragmentList_add(&fragmentList,fileName,fileInfo.size,&fileInfo,sizeof(FileInfo));
                }

                // create file
                if (!jobOptions->dryRunFlag)
                {
                  // open file
                  error = File_open(&fileHandle,destinationFileName,FILE_OPEN_WRITE);
                  if (error != ERROR_NONE)
                  {
                    printInfo(2,"FAIL!\n");
                    printError("Cannot create/write to file '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Errors_getText(error)
                              );
                    if (jobOptions->stopOnErrorFlag)
                    {
                      restoreInfo.failError = error;
                      break;
                    }
                    else
                    {
                      continue;
                    }
                  }

                  // seek to fragment position
                  error = File_seek(&fileHandle,fragmentOffset);
                  if (error != ERROR_NONE)
                  {
                    printInfo(2,"FAIL!\n");
                    printError("Cannot write file '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Errors_getText(error)
                              );
                    File_close(&fileHandle);
                    if (jobOptions->stopOnErrorFlag)
                    {
                      restoreInfo.failError = error;
                      break;
                    }
                    else
                    {
                      continue;
                    }
                  }
                  String_set(hardLinkFileName,destinationFileName);
                }

                // write file data
                length = 0;
                while (   ((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
                       && (length < fragmentSize)
                      )
                {
                  // pause
                  while ((restoreInfo.pauseFlag != NULL) && (*restoreInfo.pauseFlag))
                  {
                    Misc_udelay(500*1000);
                  }

                  n = MIN(fragmentSize-length,BUFFER_SIZE);

                  error = Archive_readData(&archiveEntryInfo,buffer,n);
                  if (error != ERROR_NONE)
                  {
                    printInfo(2,"FAIL!\n");
                    printError("Cannot read content of archive '%s' (error: %s)!\n",
                               String_cString(printableArchiveName),
                               Errors_getText(error)
                              );
                    restoreInfo.failError = error;
                    break;
                  }
                  if (!jobOptions->dryRunFlag)
                  {
                    error = File_write(&fileHandle,buffer,n);
                    if (error != ERROR_NONE)
                    {
                      printInfo(2,"FAIL!\n");
                      printError("Cannot write file '%s' (error: %s)\n",
                                 String_cString(destinationFileName),
                                 Errors_getText(error)
                                );
                      if (jobOptions->stopOnErrorFlag)
                      {
                        restoreInfo.failError = error;
                      }
                      break;
                    }
                  }
                  restoreInfo.statusInfo.entryDoneBytes += n;
                  abortFlag = !updateStatusInfo(&restoreInfo);

                  length += n;
                }
                if      (restoreInfo.failError != ERROR_NONE)
                {
                  if (!jobOptions->dryRunFlag)
                  {
                    File_close(&fileHandle);
                    break;
                  }
                }
                else if ((restoreInfo.requestedAbortFlag != NULL) && (*restoreInfo.requestedAbortFlag))
                {
                  printInfo(2,"ABORTED\n");
                  File_close(&fileHandle);
                  break;
                }

                // add fragment to file fragment list
                FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);
//FragmentList_debugPrintInfo(fragmentNode,String_cString(fileName));

                // set file size
                if (!jobOptions->dryRunFlag)
                {
                  if (File_getSize(&fileHandle) > fileInfo.size)
                  {
                    File_truncate(&fileHandle,fileInfo.size);
                  }
                }

                // close file
                if (!jobOptions->dryRunFlag)
                {
                  File_close(&fileHandle);
                }

                // set file time, file owner/group
                if (!jobOptions->dryRunFlag)
                {
                  if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
                  if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
                  error = File_setFileInfo(destinationFileName,&fileInfo);
                  if (error != ERROR_NONE)
                  {
                    if (jobOptions->stopOnErrorFlag)
                    {
                      printInfo(2,"FAIL!\n");
                      printError("Cannot set file info of '%s' (error: %s)\n",
                                 String_cString(destinationFileName),
                                 Errors_getText(error)
                                );
                      restoreInfo.failError = error;
                      break;
                    }
                    else
                    {
                      printWarning("Cannot set file info of '%s' (error: %s)\n",
                                   String_cString(destinationFileName),
                                   Errors_getText(error)
                                  );
                    }
                  }
                }

                if (!jobOptions->dryRunFlag)
                {
                  printInfo(2,"ok\n");
                }
                else
                {
                  printInfo(2,"ok (dry-run)\n");
                }

                // discard fragment list if file is complete
                if (FragmentList_checkEntryComplete(fragmentNode))
                {
                  FragmentList_discard(&fragmentList,fragmentNode);
                }

                restoredDataFlag = TRUE;
              }
              else
              {
                // check file if exists
                if (!jobOptions->overwriteFilesFlag && File_exists(destinationFileName))
                {
                  printInfo(2,"skipped (file exists)\n",String_cString(destinationFileName));
                  continue;
                }

                // create hard link
                if (!jobOptions->dryRunFlag)
                {
                  error = File_makeHardLink(destinationFileName,hardLinkFileName);
                  if (error != ERROR_NONE)
                  {
                    printInfo(2,"FAIL!\n");
                    printError("Cannot create/write to file '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Errors_getText(error)
                              );
                    if (jobOptions->stopOnErrorFlag)
                    {
                      restoreInfo.failError = error;
                      break;
                    }
                    else
                    {
                      continue;
                    }
                  }
                }

                if (!jobOptions->dryRunFlag)
                {
                  printInfo(2,"ok\n");
                }
                else
                {
                  printInfo(2,"ok (dry-run)\n");
                }

                /* check if all data read.
                   Note: it is not possible to check if all data is read when
                   compression is used. The decompressor may not all data even
                   data is _not_ corrupt.
                */
                if (   !Compress_isCompressed(archiveEntryInfo.hardLink.compressAlgorithm)
                    && !Archive_eofData(&archiveEntryInfo))
                {
                  printWarning("unexpected data at end of hard link entry '%S'.\n",fileName);
                }
              }
            }
            else
            {
              // skip
              printInfo(3,"  Restore '%s'...skipped\n",String_cString(fileName));
            }
          }
          String_delete(destinationFileName);
          String_delete(hardLinkFileName);
          if (restoreInfo.failError != ERROR_NONE)
          {
            Archive_closeEntry(&archiveEntryInfo);
            StringList_done(&fileNameList);
            continue;
          }

          // close archive file, free resources
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printWarning("close 'hard link' entry fail (error: %s)\n",Errors_getText(error));
          }

          // free resources
          StringList_done(&fileNameList);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
        {
          String   fileName;
          FileInfo fileInfo;
          String   destinationFileName;
          String   parentDirectoryName;
//            FileInfo localFileInfo;

          // read special device
          fileName = String_new();
          error = Archive_readSpecialEntry(&archiveInfo,
                                           &archiveEntryInfo,
                                           NULL,
                                           NULL,
                                           fileName,
                                           &fileInfo
                                          );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'special' content of archive '%s' (error: %s)!\n",
                       String_cString(printableArchiveName),
                       Errors_getText(error)
                      );
            String_delete(fileName);
            if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
            break;
          }

          if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
              && ((excludePatternList == NULL) || !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
             )
          {
            String_set(restoreInfo.statusInfo.name,fileName);
            restoreInfo.statusInfo.entryDoneBytes  = 0LL;
            restoreInfo.statusInfo.entryTotalBytes = 00L;
            abortFlag = !updateStatusInfo(&restoreInfo);

            // get destination filename
            destinationFileName = getDestinationFileName(String_new(),
                                                         fileName,
                                                         jobOptions->destination,
                                                         jobOptions->directoryStripCount
                                                        );

            // create parent directories if not existing
            if (!jobOptions->dryRunFlag)
            {
              parentDirectoryName = File_getFilePathName(String_new(),destinationFileName);
              if (!File_exists(parentDirectoryName))
              {
                // create directory
                error = File_makeDirectory(parentDirectoryName,
                                           FILE_DEFAULT_USER_ID,
                                           FILE_DEFAULT_GROUP_ID,
                                           FILE_DEFAULT_PERMISSION
                                          );
                if (error != ERROR_NONE)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot create directory '%s' (error: %s)\n",
                             String_cString(parentDirectoryName),
                             Errors_getText(error)
                            );
                  String_delete(parentDirectoryName);
                  String_delete(destinationFileName);
                  Archive_closeEntry(&archiveEntryInfo);
                  String_delete(fileName);
                  if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
                  continue;
                }

                // set directory owner ship
                error = File_setOwner(parentDirectoryName,
                                      (jobOptions->owner.userId != FILE_DEFAULT_USER_ID)?jobOptions->owner.userId:fileInfo.userId,
                                      (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID)?jobOptions->owner.groupId:fileInfo.groupId
                                     );
                if (error != ERROR_NONE)
                {
                  if (jobOptions->stopOnErrorFlag)
                  {
                    printInfo(2,"FAIL!\n");
                    printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                               String_cString(parentDirectoryName),
                               Errors_getText(error)
                              );
                    String_delete(parentDirectoryName);
                    String_delete(destinationFileName);
                    Archive_closeEntry(&archiveEntryInfo);
                    String_delete(fileName);
                    if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
                    continue;
                  }
                  else
                  {
                    printWarning("Cannot set owner ship of directory '%s' (error: %s)\n",
                                 String_cString(parentDirectoryName),
                                 Errors_getText(error)
                                );
                  }
                }
              }
              String_delete(parentDirectoryName);
            }

            // check if special file already exists
            if (!jobOptions->overwriteFilesFlag && File_exists(destinationFileName))
            {
              printInfo(1,
                        "  Restore special device '%s'...skipped (file exists)\n",
                        String_cString(destinationFileName)
                       );
              String_delete(destinationFileName);
              Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              if (jobOptions->stopOnErrorFlag)
              {
                restoreInfo.failError = ERRORX(FILE_EXISTS,0,String_cString(destinationFileName));
              }
              continue;
            }

            printInfo(2,"  Restore special device '%s'...",String_cString(destinationFileName));

            // create special device
            if (!jobOptions->dryRunFlag)
            {
              error = File_makeSpecial(destinationFileName,
                                       fileInfo.specialType,
                                       fileInfo.major,
                                       fileInfo.minor
                                      );
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot create special device '%s' (error: %s)\n",
                           String_cString(fileName),
                           Errors_getText(error)
                          );
                String_delete(destinationFileName);
                Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                if (jobOptions->stopOnErrorFlag)
                {
                  restoreInfo.failError = error;
                }
                continue;
              }
            }

            // set file time, file owner/group
            if (!jobOptions->dryRunFlag)
            {
              if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
              if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
              error = File_setFileInfo(destinationFileName,&fileInfo);
              if (error != ERROR_NONE)
              {
                if (jobOptions->stopOnErrorFlag)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot set file info of '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Errors_getText(error)
                            );
                  String_delete(destinationFileName);
                  Archive_closeEntry(&archiveEntryInfo);
                  String_delete(fileName);
                  if (jobOptions->stopOnErrorFlag)
                  {
                    restoreInfo.failError = error;
                  }
                  continue;
                }
                else
                {
                  printWarning("Cannot set file info of '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Errors_getText(error)
                              );
                }
              }
            }

            if (!jobOptions->dryRunFlag)
            {
              printInfo(2,"ok\n");
            }
            else
            {
              printInfo(2,"ok (dry-run)\n");
            }

            // check if all data read
            if (!Archive_eofData(&archiveEntryInfo))
            {
              printWarning("unexpected data at end of special entry '%S'.\n",fileName);
            }

            // free resources
            String_delete(destinationFileName);
          }
          else
          {
            // skip
            printInfo(3,"  Restore '%s'...skipped\n",String_cString(fileName));
          }

          // close archive file
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printWarning("close 'special' entry fail (error: %s)\n",Errors_getText(error));
          }

          // free resources
          String_delete(fileName);
        }
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; /* not reached */
    }
  }


  // free resources
  Archive_close(&destinationArchiveInfo);
  Archive_close(&sourceArchiveInfo);

  return ERROR_NONE;
}
#endif /* 0 */

#ifdef __cplusplus
  }
#endif

/* end of file */
