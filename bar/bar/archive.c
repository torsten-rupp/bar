/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive functions
* Systems: all
*
\***********************************************************************/

#define __ARCHIVE_IMPLEMENATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "autofree.h"
#include "strings.h"
#include "lists.h"
#include "files.h"
#include "devices.h"
#include "semaphores.h"

#include "errors.h"
#include "chunks.h"
#include "compress.h"
#include "passwords.h"
#include "crypt.h"
#include "archive_format.h"
#include "storage.h"
#include "index.h"
#include "bar_global.h"
#include "bar.h"

#include "archive.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// archive entry types
LOCAL const struct
{
  const char        *name;
  ArchiveEntryTypes archiveEntryType;
} ARCHIVE_ENTRY_TYPES[] =
{
  { "file",     ARCHIVE_ENTRY_TYPE_FILE      },
  { "image",    ARCHIVE_ENTRY_TYPE_IMAGE     },
  { "directory",ARCHIVE_ENTRY_TYPE_DIRECTORY },
  { "link",     ARCHIVE_ENTRY_TYPE_LINK      },
  { "hardlink", ARCHIVE_ENTRY_TYPE_HARDLINK  },
  { "special",  ARCHIVE_ENTRY_TYPE_SPECIAL   }
};

// size of buffer for processing data
#define MAX_BUFFER_SIZE (64*1024)

/* size of data block to write without splitting
   Note: a single part of a splitted archive may be larger around this size
*/
#define DATA_BLOCK_SIZE (16*1024)

// default alignment
#define DEFAULT_ALIGNMENT 4

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
  Semaphore          lock;
  const PasswordNode *newPasswordNode;
} PasswordList;

// password handle
typedef struct
{
  ArchiveInfo         *archiveInfo;
  PasswordModes       passwordMode;                      // password input mode
  const Password      *cryptPassword;                    // crypt password
  const PasswordNode  *passwordNode;                     // next password node to use
  GetPasswordFunction getPasswordFunction;               // password input callback
  void                *getPasswordUserData;
} PasswordHandle;

/***************************** Variables *******************************/

// list with all known decryption passwords
LOCAL PasswordList decryptPasswordList;

/****************************** Macros *********************************/

#define FILE_SYSTEM_CONSTANT_TO_TYPE(n) ((FileSystemTypes)(n))

// debug only: store encoded data into file
//#define DEBUG_ENCODED_DATA_FILENAME "test-encoded.dat"

//#ifdef DEBUG_ENCODED_DATA_FILENAME
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//#endif /* DEBUG_ENCODED_DATA_FILENAME */

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
* Input  : archiveInfo         - archive info
*          jobOptions          - job options
*          passwordMode        - password mode
*          getPasswordFunction - get password call-back (can
*                                be NULL)
*          getPasswordUserData - user data for get password call-back
* Output : password - password
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getCryptPassword(Password            *password,
                              ArchiveInfo         *archiveInfo,
                              const JobOptions    *jobOptions,
                              GetPasswordFunction getPasswordFunction,
                              void                *getPasswordUserData
                             )
{
  Errors error;

  assert(password != NULL);
  assert(archiveInfo != NULL);
  assert(jobOptions != NULL);

  error = ERROR_UNKNOWN;

  switch (jobOptions->cryptPasswordMode)
  {
    case PASSWORD_MODE_DEFAULT:
      if (globalOptions.cryptPassword != NULL)
      {
        Password_set(password,globalOptions.cryptPassword);
        error = ERROR_NONE;
      }
      else
      {
        if (!archiveInfo->cryptPasswordReadFlag && (getPasswordFunction != NULL))
        {
          error = getPasswordFunction(NULL,  // loginName
                                      password,
                                      PASSWORD_TYPE_CRYPT,
                                      (archiveInfo->ioType == ARCHIVE_IO_TYPE_STORAGE_FILE)
                                        ? String_cString(Storage_getPrintableName(&archiveInfo->storage.storageSpecifier,NULL))
                                        : NULL,
                                      TRUE,
                                      TRUE,
                                      getPasswordUserData
                                     );
          archiveInfo->cryptPasswordReadFlag = TRUE;
        }
        else
        {
          error = ERROR_NO_CRYPT_PASSWORD;
        }
      }
      break;
    case PASSWORD_MODE_ASK:
      if (globalOptions.cryptPassword != NULL)
      {
        Password_set(password,globalOptions.cryptPassword);
        error = ERROR_NONE;
      }
      else
      {
        if (!archiveInfo->cryptPasswordReadFlag && (getPasswordFunction != NULL))
        {
          error = getPasswordFunction(NULL,  // loginName
                                      password,
                                      PASSWORD_TYPE_CRYPT,
                                      (archiveInfo->ioType == ARCHIVE_IO_TYPE_STORAGE_FILE)
                                        ? String_cString(Storage_getPrintableName(&archiveInfo->storage.storageSpecifier,NULL))
                                        : NULL,
                                      TRUE,
                                      TRUE,
                                      getPasswordUserData
                                     );
          archiveInfo->cryptPasswordReadFlag = TRUE;
        }
        else
        {
          error = ERROR_NO_CRYPT_PASSWORD;
        }
      }
      break;
    case PASSWORD_MODE_CONFIG:
      if      (jobOptions->cryptPassword != NULL)
      {
        Password_set(password,jobOptions->cryptPassword);
        error = ERROR_NONE;
      }
      else if (globalOptions.cryptPassword != NULL)
      {
        Password_set(password,globalOptions.cryptPassword);
        error = ERROR_NONE;
      }
      else
      {
        if (!archiveInfo->cryptPasswordReadFlag && (getPasswordFunction != NULL))
        {
          error = getPasswordFunction(NULL,  // loginName
                                      password,
                                      PASSWORD_TYPE_CRYPT,
                                      (archiveInfo->ioType == ARCHIVE_IO_TYPE_STORAGE_FILE)
                                        ? String_cString(Storage_getPrintableName(&archiveInfo->storage.storageSpecifier,NULL))
                                        : NULL,
                                      TRUE,
                                      TRUE,
                                      getPasswordUserData
                                     );
          archiveInfo->cryptPasswordReadFlag = TRUE;
        }
        else
        {
          error = ERROR_NO_CRYPT_PASSWORD;
        }
      }
      break;
    case PASSWORD_MODE_NONE:
      error = ERROR_NO_CRYPT_PASSWORD;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

/***********************************************************************\
* Name   : getNextDecryptPassword
* Purpose: get next decrypt password
* Input  : passwordHandle - password handle
* Output : -
* Return : password or NULL if no more passwords
* Notes  : Ordering of password search:
*            - next password from list
*            - password from config
*            - password from command line
*            - ask for password (only if no command line password set)
\***********************************************************************/

LOCAL const Password *getNextDecryptPassword(PasswordHandle *passwordHandle)
{
  bool           semaphoreLock;
  const Password *password;
  Password       newPassword;
  Errors         error;

  assert(passwordHandle != NULL);

  password = NULL;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&passwordHandle->archiveInfo->passwordLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    while ((password == NULL) && (passwordHandle->passwordMode != PASSWORD_MODE_NONE))
    {
      if      (passwordHandle->passwordNode != NULL)
      {
        // get next password from list
        password = passwordHandle->passwordNode->password;
        passwordHandle->passwordNode = passwordHandle->passwordNode->next;
      }
      else if (passwordHandle->passwordMode == PASSWORD_MODE_CONFIG)
      {
        if (passwordHandle->cryptPassword != NULL)
        {
          // get password
          password = (Password*)passwordHandle->cryptPassword;
        }

        // next password mode is: default
        passwordHandle->passwordMode = PASSWORD_MODE_DEFAULT;
      }
      else if (passwordHandle->passwordMode == PASSWORD_MODE_DEFAULT)
      {
        if (globalOptions.cryptPassword != NULL)
        {
          // get password
          password = globalOptions.cryptPassword;
        }

        // next password mode is: ask
        passwordHandle->passwordMode = PASSWORD_MODE_ASK;
      }
      else if (   (passwordHandle->passwordMode == PASSWORD_MODE_ASK)
               && (passwordHandle->getPasswordFunction != NULL)
              )
      {
        // input password
        Password_init(&newPassword);
        error = passwordHandle->getPasswordFunction(NULL,  // loginName
                                                    &newPassword,
                                                    PASSWORD_TYPE_CRYPT,
                                                    (passwordHandle->archiveInfo->ioType == ARCHIVE_IO_TYPE_STORAGE_FILE)
                                                      ? String_cString(Storage_getPrintableName(&passwordHandle->archiveInfo->storage.storageSpecifier,NULL))
                                                      : NULL,
                                                    FALSE,
                                                    FALSE,
                                                    passwordHandle->getPasswordUserData
                                                   );
        if (error == ERROR_NONE)
        {
          // add to password list
          password = Archive_appendDecryptPassword(&newPassword);

          // free resources
          Password_done(&newPassword);
        }
        else
        {
          // next password mode is: none
          passwordHandle->passwordMode = PASSWORD_MODE_NONE;
        }
      }
    }
  }

  return password;
}

/***********************************************************************\
* Name   : getFirstDecryptPassword
* Purpose: get first decrypt password
* Input  : archiveInfo         - archive info
*          jobOptions          - job options
*          passwordMode        - password mode
*          getPasswordFunction - get password call-back
*          getPasswordUserData - user data for get password call-back
* Output : passwordHandle - intialized password handle
* Return : password or NULL if no more passwords
* Notes  : -
\***********************************************************************/

LOCAL const Password *getFirstDecryptPassword(PasswordHandle      *passwordHandle,
                                              ArchiveInfo         *archiveInfo,
                                              const JobOptions    *jobOptions,
                                              PasswordModes       passwordMode,
                                              GetPasswordFunction getPasswordFunction,
                                              void                *getPasswordUserData
                                             )
{
  assert(passwordHandle != NULL);

  passwordHandle->archiveInfo         = archiveInfo;
  passwordHandle->passwordMode        = (passwordMode != PASSWORD_MODE_DEFAULT) ? passwordMode : jobOptions->cryptPasswordMode;
  passwordHandle->cryptPassword       = jobOptions->cryptPassword;
  passwordHandle->passwordNode        = decryptPasswordList.head;
  passwordHandle->getPasswordFunction = getPasswordFunction;
  passwordHandle->getPasswordUserData = getPasswordUserData;

  return getNextDecryptPassword(passwordHandle);
}

/***********************************************************************\
* Name   : initCryptPassword
* Purpose: initialize crypt password
* Input  : archiveInfo - archive info data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors initCryptPassword(ArchiveInfo *archiveInfo)
{
  SemaphoreLock semaphoreLock;
  Password      *cryptPassword;
  Errors        error;

  assert(archiveInfo != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveInfo->passwordLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    if (Crypt_isEncrypted(archiveInfo->jobOptions->cryptAlgorithm) && (archiveInfo->cryptPassword == NULL))
    {
      cryptPassword = Password_new();
      if (cryptPassword == NULL)
      {
        return ERROR_NO_CRYPT_PASSWORD;
      }
      error = getCryptPassword(cryptPassword,
                               archiveInfo,
                               archiveInfo->jobOptions,
                               archiveInfo->getPasswordFunction,
                               archiveInfo->getPasswordUserData
                              );
      if (error != ERROR_NONE)
      {
        Password_delete(cryptPassword);
        Semaphore_unlock(&archiveInfo->passwordLock);
        return error;
      }
      archiveInfo->cryptPassword = cryptPassword;
    }
  }

  return ERROR_NONE;
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : chunkHeaderEOF
* Purpose: check if chunk header end-of-file
* Input  : archiveInfo - archive info data
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
* Input  : archiveInfo - archive info data
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
* Input  : archiveInfo - archive info data
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

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : isSplittedArchive
* Purpose: check if archive should be splitted
* Input  : archiveInfo - archive info data
* Output : -
* Return : TRUE iff archive should be splitted
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isSplittedArchive(const ArchiveInfo *archiveInfo)
{
  assert(archiveInfo != NULL);

  return (archiveInfo->jobOptions->archivePartSize > 0LL);
}

/***********************************************************************\
* Name   : isNewPartNeeded
* Purpose: check if new archive part should be created
* Input  : archiveInfo - archive info data
*          headerLength      - length of header data to write
*          headerWrittenFlag - TRUE iff header already written
*          minBytes          - additional space needed in archive file
*                              part
* Output : -
* Return : TRUE if new file part should be created, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool isNewPartNeeded(ArchiveInfo *archiveInfo,
                           ulong       minBytes
                          )
{
  bool   newPartFlag;
  uint64 archiveFileSize;

  assert(archiveInfo != NULL);
  assert(archiveInfo->chunkIO != NULL);
  assert(archiveInfo->chunkIO->getSize != NULL);
  assert(archiveInfo->jobOptions != NULL);
  assert(archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);
  assert(Semaphore_isOwned(&archiveInfo->chunkIOLock));

  newPartFlag = FALSE;
  if (isSplittedArchive(archiveInfo))
  {
    // get current archive size
    if (archiveInfo->file.openFlag)
    {
      archiveFileSize = archiveInfo->chunkIO->getSize(archiveInfo->chunkIOUserData);
    }
    else
    {
      archiveFileSize = 0LL;
    }
//fprintf(stderr,"%s, %d: archiveFileSize=%llu %lu %llu\n",__FILE__,__LINE__,archiveFileSize,minBytes,archiveInfo->archiveFileSize);

    if ((archiveInfo->archiveFileSize+archiveFileSize+minBytes) >= archiveInfo->jobOptions->archivePartSize)
    {
//fprintf(stderr,"%s, %d: archiveFileSize=%lld minBytes=%lld\n",__FILE__,__LINE__,archiveFileSize,minBytes);
      // less than min. number of bytes left in part -> create new part
      newPartFlag = TRUE;
    }
  }

  return newPartFlag;
}

/***********************************************************************\
* Name   : findNextArchivePart
* Purpose: find next suitable archive part
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void findNextArchivePart(ArchiveInfo *archiveInfo)
{
  uint64 storageSize;

  // find next suitable archive name
  if (isSplittedArchive(archiveInfo))
  {
    do
    {
      storageSize = archiveInfo->archiveGetSizeFunction(archiveInfo->archiveGetSizeUserData,
                                                archiveInfo->indexHandle,
//                                                archiveInfo->entityId,
                                                archiveInfo->storageId,
                                                archiveInfo->partNumber
                                               );
      if (storageSize > archiveInfo->jobOptions->archivePartSize)
      {
        archiveInfo->partNumber++;
      }
    }
    while (storageSize > archiveInfo->jobOptions->archivePartSize);

    archiveInfo->archiveFileSize = storageSize;
  }
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : readEncryptionKey
* Purpose: read encryption key
* Input  : archiveInfo - archive info data
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

  // init key chunk
  error = Chunk_init(&chunkInfoKey,
                     NULL,  // parentChunkInfo
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_KEY,
                     CHUNK_DEFINITION_KEY,
                     0,  // alignment
                     NULL,  // cryptInfo
                     &chunkKey
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // read key chunk
  error = Chunk_open(&chunkInfoKey,
                     chunkHeader,
                     Chunk_getSize(&chunkInfoKey,NULL,0)
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkInfoKey);
    return error;
  }
  archiveInfo->cryptKeyDataLength = chunkHeader->size-Chunk_getSize(&chunkInfoKey,NULL,0);
  if (archiveInfo->cryptKeyData != NULL) free(archiveInfo->cryptKeyData);
  archiveInfo->cryptKeyData = malloc(archiveInfo->cryptKeyDataLength);
  if (archiveInfo->cryptKeyData == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  error = Chunk_readData(&chunkInfoKey,
                         archiveInfo->cryptKeyData,
                         archiveInfo->cryptKeyDataLength,
                         NULL
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
    return ERRORX_(CORRUPT_DATA,0,"%s",String_cString(archiveInfo->printableStorageName));
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

  // get decrypt key
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
* Input  : archiveInfo - archive info data
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

  // init BAR chunk
  error = Chunk_init(&chunkInfoBar,
                     NULL,  // parentChunkInfo
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_BAR,
                     CHUNK_DEFINITION_BAR,
                     0,  // alignment
                     NULL,  // cryptInfo
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
* Input  : archiveInfo - archive info data
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
  assert(Semaphore_isOwned(&archiveInfo->chunkIOLock));

  // init key chunk
  error = Chunk_init(&chunkInfoKey,
                     NULL,  // parentChunkInfo
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_KEY,
                     CHUNK_DEFINITION_KEY,
                     0,  // alignment
                     NULL,  // cryptInfo
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
* Input  : archiveInfo - archive info data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createArchiveFile(ArchiveInfo *archiveInfo)
{
  AutoFreeList  autoFreeList;
  SemaphoreLock semaphoreLock;
  Errors        error;

  assert(archiveInfo != NULL);
  assert(archiveInfo->jobOptions != NULL);
  assert(archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);

  if (!archiveInfo->file.openFlag)
  {
    // init variables
    AutoFree_init(&autoFreeList);

    SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
    {
      AUTOFREE_ADD(&autoFreeList,&archiveInfo->chunkIOLock,{ Semaphore_unlock(&archiveInfo->chunkIOLock); });

      // get intermediate data filename
      error = File_getTmpFileName(archiveInfo->file.fileName,NULL,tmpDirectory);
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      AUTOFREE_ADD(&autoFreeList,&archiveInfo->file.fileName,{ File_delete(archiveInfo->file.fileName,FALSE); });
      DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

      // create file
      error = File_open(&archiveInfo->file.fileHandle,
                        archiveInfo->file.fileName,
                        FILE_OPEN_CREATE
                       );
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      AUTOFREE_ADD(&autoFreeList,&archiveInfo->file.fileHandle,{ File_close(&archiveInfo->file.fileHandle); });
      DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

      // write BAR file info header
      error = writeFileInfo(archiveInfo);
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

      // write encrypted key if asymmetric encryption enabled
      if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
      {
        error = writeEncryptionKey(archiveInfo);
        if (error != ERROR_NONE)
        {
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
      }

      if (   (archiveInfo->indexHandle != NULL)
          && !archiveInfo->jobOptions->noIndexDatabaseFlag
          && !archiveInfo->jobOptions->dryRunFlag
          && !archiveInfo->jobOptions->noStorageFlag
         )
      {
        // create storage index
        error = Index_newStorage(archiveInfo->indexHandle,
                                 DATABASE_ID_NONE,  // entityId,
                                 NULL, // storageName
                                 INDEX_STATE_CREATE,
                                 INDEX_MODE_MANUAL,
                                 &archiveInfo->storageId
                                );
        if (error != ERROR_NONE)
        {
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        AUTOFREE_ADD(&autoFreeList,&archiveInfo->storageId,{ Index_deleteStorage(archiveInfo->indexHandle,archiveInfo->storageId); });
      }
      else
      {
        // no index
        archiveInfo->storageId = DATABASE_ID_NONE;
      }

      // call-back for init archive
      if (archiveInfo->archiveInitFunction != NULL)
      {
        error = archiveInfo->archiveInitFunction(archiveInfo->archiveInitUserData,
                                                 archiveInfo->indexHandle,
                                                 archiveInfo->jobUUID,
                                                 archiveInfo->scheduleUUID,
                                                 archiveInfo->archiveType,
//                                                 archiveInfo->entityId,
                                                 archiveInfo->storageId,
                                                 isSplittedArchive(archiveInfo)
                                                   ? (int)archiveInfo->partNumber
                                                   : ARCHIVE_PART_NUMBER_NONE
                                                );
        if (error != ERROR_NONE)
        {
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
      }

      // mark archive file "open"
      archiveInfo->file.openFlag = TRUE;
    }

    // free resources
    AutoFree_done(&autoFreeList);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : closeArchiveFile
* Purpose: close archive file
* Input  : archiveInfo  - archive info data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors closeArchiveFile(ArchiveInfo *archiveInfo)
{
  SemaphoreLock semaphoreLock;
  Errors        error;

  assert(archiveInfo != NULL);
  assert(archiveInfo->jobOptions != NULL);
  assert(archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);
#ifndef WERROR
#warning TODO: nicht offen wenn keine datei archiviert wurde
#endif
//  assert(archiveInfo->file.openFlag);
if (!archiveInfo->file.openFlag) return ERROR_NONE;

  SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // close file
    (void)File_close(&archiveInfo->file.fileHandle);

    // mark archive file "closed"
    archiveInfo->file.openFlag = FALSE;

    // call-back to store archive
    if (archiveInfo->archiveStoreFunction != NULL)
    {
      error = archiveInfo->archiveStoreFunction(archiveInfo->archiveStoreUserData,
                                                archiveInfo->indexHandle,
                                                archiveInfo->jobUUID,
                                                archiveInfo->scheduleUUID,
                                                archiveInfo->archiveType,
//                                                archiveInfo->entityId,
                                                archiveInfo->storageId,
                                                isSplittedArchive(archiveInfo)
                                                  ? (int)archiveInfo->partNumber
                                                  : ARCHIVE_PART_NUMBER_NONE,
                                                archiveInfo->file.fileName,
                                                Archive_getSize(archiveInfo)
                                               );
      if (error != ERROR_NONE)
      {
        Semaphore_unlock(&archiveInfo->chunkIOLock);
        return error;
      }
      DEBUG_TESTCODE() { Semaphore_unlock(&archiveInfo->chunkIOLock); return DEBUG_TESTCODE_ERROR(); }
    }

    // call-back for done archive
    if (archiveInfo->archiveDoneFunction != NULL)
    {
      error = archiveInfo->archiveDoneFunction(archiveInfo->archiveDoneUserData,
                                               archiveInfo->indexHandle,
                                                archiveInfo->jobUUID,
                                                archiveInfo->scheduleUUID,
                                                archiveInfo->archiveType,
//                                                archiveInfo->entityId,
                                               archiveInfo->storageId,
                                               isSplittedArchive(archiveInfo)
                                                 ? (int)archiveInfo->partNumber
                                                 : ARCHIVE_PART_NUMBER_NONE
                                              );
      if (error != ERROR_NONE)
      {
        Semaphore_unlock(&archiveInfo->chunkIOLock);
        return error;
      }
      DEBUG_TESTCODE() { Semaphore_unlock(&archiveInfo->chunkIOLock); return DEBUG_TESTCODE_ERROR(); }
    }

    // increment part number
    if (isSplittedArchive(archiveInfo))
    {
      archiveInfo->partNumber++;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : ensureArchiveSpace
* Purpose: ensure space is available in archive for not fragmented
*          writing
* Input  : archiveInfo  - archive info data
*          minBytes     - minimal number of bytes
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
  assert(Semaphore_isOwned(&archiveInfo->chunkIOLock));

  // check if split is necessary
  if (isNewPartNeeded(archiveInfo,
                      minBytes
                     )
     )
  {
    // split needed -> close archive file
    if (archiveInfo->file.openFlag)
    {
      error = closeArchiveFile(archiveInfo);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
  }

  // create new archive
  error = createArchiveFile(archiveInfo);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : transferArchiveFileData
* Purpose: transfer file data from temporary file to archive
* Input  : archiveInfo  - archive info data
*          fileHandle   - file handle of temporary file
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors transferArchiveFileData(const ArchiveInfo *archiveInfo,
                                     FileHandle        *fileHandle
                                    )
{
  #define BUFFER_SIZE (1024*1024)

  void    *buffer;
  Errors  error;
  uint64  length;
  ulong   n;

  assert(archiveInfo != NULL);
  assert(archiveInfo->chunkIO != NULL);
  assert(archiveInfo->chunkIO->write != NULL);
  assert(fileHandle != NULL);
  assert(Semaphore_isOwned(&archiveInfo->chunkIOLock));

  // allocate transfer buffer
  buffer = (char*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // seek to begin of file
  error = File_seek(fileHandle,0LL);
  if (error != ERROR_NONE)
  {
    free(buffer);
    return error;
  }

  // transfer data
  length = File_getSize(fileHandle);
  while (length > 0LL)
  {
    n = MIN(length,BUFFER_SIZE);

    error = File_read(fileHandle,buffer,n,NULL);
    if (error != ERROR_NONE)
    {
      free(buffer);
      return error;
    }

    error = archiveInfo->chunkIO->write(archiveInfo->chunkIOUserData,buffer,n);
    if (error != ERROR_NONE)
    {
      free(buffer);
      return error;
    }

    length -= (uint64)n;
  }

  // truncate file
  File_truncate(fileHandle,0LL);

  // free resources
  free(buffer);

  return ERROR_NONE;

  #undef BUFFER_SIZE
}

/***********************************************************************\
* Name   : writeFileChunks
* Purpose: write file chunks
* Input  : archiveEntryInfo - archive file entry info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeFileChunks(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors                    error;
  FileExtendedAttributeNode *fileExtendedAttributeNode;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(!archiveEntryInfo->file.headerWrittenFlag);

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

  // create extended attribute chunks
  LIST_ITERATE(archiveEntryInfo->file.fileExtendedAttributeList,fileExtendedAttributeNode)
  {
    String_set(archiveEntryInfo->file.chunkFileExtendedAttribute.name,fileExtendedAttributeNode->name);
    archiveEntryInfo->file.chunkFileExtendedAttribute.value.data   = fileExtendedAttributeNode->data;
    archiveEntryInfo->file.chunkFileExtendedAttribute.value.length = fileExtendedAttributeNode->dataLength;

    error = Chunk_create(&archiveEntryInfo->file.chunkFileExtendedAttribute.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Chunk_close(&archiveEntryInfo->file.chunkFileExtendedAttribute.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // create file delta chunk
  if (Compress_isCompressed(archiveEntryInfo->file.deltaCompressAlgorithm))
  {
    assert(Compress_isXDeltaCompressed(archiveEntryInfo->file.deltaCompressAlgorithm));

    archiveEntryInfo->file.chunkFileDelta.deltaAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->file.deltaCompressAlgorithm);
    String_set(archiveEntryInfo->file.chunkFileDelta.name,DeltaSource_getName(&archiveEntryInfo->file.deltaSourceHandle));
    archiveEntryInfo->file.chunkFileDelta.size           = DeltaSource_getSize(&archiveEntryInfo->file.deltaSourceHandle);

    error = Chunk_create(&archiveEntryInfo->file.chunkFileDelta.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Chunk_close(&archiveEntryInfo->file.chunkFileDelta.info);
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

  // mark header "written"
  archiveEntryInfo->file.headerWrittenFlag = TRUE;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : flushFileDataBlocks
* Purpose: flush file data blocks
* Input  : archiveEntryInfo  - archive file entry info data
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
  ulong  maxBlockCount;
  ulong  byteLength;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(archiveEntryInfo->file.byteCompressInfo.blockLength != 0);

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
      // write header (if not already written)
      if (!archiveEntryInfo->file.headerWrittenFlag)
      {
        error = writeFileChunks(archiveEntryInfo);
        if (error != ERROR_NONE)
        {
          return error;
        }
      }

      // get max. number of byte-compressed data blocks to write
      maxBlockCount = MIN(archiveEntryInfo->file.byteBufferSize/archiveEntryInfo->file.byteCompressInfo.blockLength,
                          blockCount
                         );

      // get byte-compressed data blocks
      Compress_getCompressedData(&archiveEntryInfo->file.byteCompressInfo,
                                 archiveEntryInfo->file.byteBuffer,
                                 maxBlockCount*archiveEntryInfo->file.byteCompressInfo.blockLength,
                                 &byteLength
                                );
      assert((byteLength%archiveEntryInfo->file.byteCompressInfo.blockLength) == 0);
#ifndef WERROR
#warning remove?
#endif
assert(byteLength > 0L);

      if (byteLength > 0L)
      {
        // write header (if not already written)
        if (!archiveEntryInfo->file.headerWrittenFlag)
        {
          error = writeFileChunks(archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        // encrypt block
        error = Crypt_encryptBytes(&archiveEntryInfo->file.cryptInfo,
                                   archiveEntryInfo->file.byteBuffer,
                                   byteLength
                                  );
        if (error != ERROR_NONE)
        {
          return error;
        }

        #ifdef DEBUG_ENCODED_DATA_FILENAME
          {
            int handle = open64(DEBUG_ENCODED_DATA_FILENAME,O_CREAT|O_WRONLY|O_APPEND|O_LARGEFILE,0664);
            write(handle,archiveEntryInfo->file.byteBuffer,byteLength);
            close(handle);
          }
        #endif /* DEBUG_ENCODED_DATA_FILENAME */

        // write data block
//fprintf(stderr,"%s, %d: write %d\n",__FILE__,__LINE__,byteLength);
        error = Chunk_writeData(&archiveEntryInfo->file.chunkFileData.info,
                                archiveEntryInfo->file.byteBuffer,
                                byteLength
                               );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
    }
  }
  while (blockCount > 0);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeFileDataBlocks
* Purpose: encrypt and write file blocks, split archive
* Input  : archiveEntryInfo - archive file entry info data
*          allowNewPartFlag - TRUE iff new archive part can be created
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeFileDataBlocks(ArchiveEntryInfo *archiveEntryInfo,
                                 bool             allowNewPartFlag
                                )
{
  uint   blockCount;
  ulong  byteLength;
  ulong  minBytes;
  bool   newPartFlag;
  Errors error;
  bool   eofDelta;
  ulong  deltaLength;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(archiveEntryInfo->archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);
  assert(archiveEntryInfo->file.byteCompressInfo.blockLength != 0);

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
      // get byte-compressed data blocks
      assert(archiveEntryInfo->file.byteBufferSize >= (blockCount*archiveEntryInfo->file.byteCompressInfo.blockLength));
      Compress_getCompressedData(&archiveEntryInfo->file.byteCompressInfo,
                                 archiveEntryInfo->file.byteBuffer,
//                                 archiveEntryInfo->file.byteBufferSize,
                                 blockCount*archiveEntryInfo->file.byteCompressInfo.blockLength,
//                                 archiveEntryInfo->file.byteCompressInfo.blockLength,
                                 &byteLength
                                );

      // calculate min. bytes to tramsfer to archive
      minBytes = (ulong)File_getSize(&archiveEntryInfo->file.intermediateFileHandle)+byteLength;

      // lock
      Semaphore_lock(&archiveEntryInfo->archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);

      // check if split is allowed and necessary
      newPartFlag =    allowNewPartFlag
                    && isNewPartNeeded(archiveEntryInfo->archiveInfo,
                                       (!archiveEntryInfo->file.headerWrittenFlag ? archiveEntryInfo->file.headerLength : 0) + minBytes
                                      );

      // split
      if (newPartFlag)
      {
        // write last compressed block (if any)
        if (byteLength > 0L)
        {
          // write header (if not already written)
          if (!archiveEntryInfo->file.headerWrittenFlag)
          {
            error = writeFileChunks(archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }

          // encrypt block
          error = Crypt_encryptBytes(&archiveEntryInfo->file.cryptInfo,
                                     archiveEntryInfo->file.byteBuffer,
                                     byteLength
                                    );
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
            return error;
          }

          #ifdef DEBUG_ENCODED_DATA_FILENAME
            {
              int handle = open64(DEBUG_ENCODED_DATA_FILENAME,O_CREAT|O_WRONLY|O_APPEND|O_LARGEFILE,0664);
              write(handle,archiveEntryInfo->file.byteBuffer,byteLength);
              close(handle);
            }
          #endif /* DEBUG_ENCODED_DATA_FILENAME */

          // store block into chunk
          error = Chunk_writeData(&archiveEntryInfo->file.chunkFileData.info,
                                  archiveEntryInfo->file.byteBuffer,
                                  byteLength
                                );
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
            return error;
          }
        }

        // flush delta compress
        error = Compress_flush(&archiveEntryInfo->file.deltaCompressInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }
        eofDelta = FALSE;
        do
        {
          // flush compressed full data blocks
          error = flushFileDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_FULL);
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
            return error;
          }

          // get next delta-compressed data
          Compress_getCompressedData(&archiveEntryInfo->file.deltaCompressInfo,
                                     archiveEntryInfo->file.deltaBuffer,
                                     archiveEntryInfo->file.deltaBufferSize,
                                     &deltaLength
                                    );
          if (deltaLength > 0)
          {
            // byte-compress data
            error = Compress_deflate(&archiveEntryInfo->file.byteCompressInfo,
                                     archiveEntryInfo->file.deltaBuffer,
                                     deltaLength,
                                     NULL
                                    );
            if (error != ERROR_NONE)
            {
              Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
              return error;
            }
          }
          else
          {
            eofDelta = TRUE;
          }
        }
        while (!eofDelta);

        // flush byte compress
        error = Compress_flush(&archiveEntryInfo->file.byteCompressInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }
        error = flushFileDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_ANY);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }

        // update part size
        archiveEntryInfo->file.chunkFileData.fragmentSize = Compress_getInputLength(&archiveEntryInfo->file.deltaCompressInfo);
        error = Chunk_update(&archiveEntryInfo->file.chunkFileData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }

        // close chunks
        error = Chunk_close(&archiveEntryInfo->file.chunkFileData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->file.chunkFileEntry.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->file.chunkFile.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }
        assert(archiveEntryInfo->file.headerWrittenFlag);

        // create archive file
        error = createArchiveFile(archiveEntryInfo->archiveInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }

        // transfer intermediate data into archive
        error = transferArchiveFileData(archiveEntryInfo->archiveInfo,
                                        &archiveEntryInfo->file.intermediateFileHandle
                                       );
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }

        // close archive
        error = closeArchiveFile(archiveEntryInfo->archiveInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }

        // mark header "not written"
        archiveEntryInfo->file.headerWrittenFlag = FALSE;

        // store fragment offset, count for next fragment
        archiveEntryInfo->file.chunkFileData.fragmentOffset += archiveEntryInfo->file.chunkFileData.fragmentSize;
        archiveEntryInfo->file.chunkFileData.fragmentSize   =  0LL;

        // set new delta base-offset
        if (archiveEntryInfo->image.deltaSourceHandleInitFlag)
        {
          DeltaSource_setBaseOffset(&archiveEntryInfo->file.deltaSourceHandle,
                               archiveEntryInfo->file.chunkFileData.fragmentSize
                              );
        }

        /* reset compress, encrypt (do it here because data if buffered and can be
          processed before a new file is opened)
        */
        Compress_reset(&archiveEntryInfo->file.deltaCompressInfo);
        Compress_reset(&archiveEntryInfo->file.byteCompressInfo);
        Crypt_reset(&archiveEntryInfo->file.cryptInfo,0);

        // find next suitable archive part
        findNextArchivePart(archiveEntryInfo->archiveInfo);

        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
      }
      else
      {
        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);

        if (byteLength > 0L)
        {
          // write header (if not already written)
          if (!archiveEntryInfo->file.headerWrittenFlag)
          {
            error = writeFileChunks(archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }

          // encrypt block
          error = Crypt_encryptBytes(&archiveEntryInfo->file.cryptInfo,
                                     archiveEntryInfo->file.byteBuffer,
                                     byteLength
                                    );
          if (error != ERROR_NONE)
          {
            return error;
          }

          #ifdef DEBUG_ENCODED_DATA_FILENAME
            {
              int handle = open64(DEBUG_ENCODED_DATA_FILENAME,O_CREAT|O_WRONLY|O_APPEND|O_LARGEFILE,0664);
              write(handle,archiveEntryInfo->file.byteBuffer,byteLength);
              close(handle);
            }
          #endif /* DEBUG_ENCODED_DATA_FILENAME */

          // write data block
//fprintf(stderr,"%s, %d: write %d\n",__FILE__,__LINE__,byteLength);
          error = Chunk_writeData(&archiveEntryInfo->file.chunkFileData.info,
                                  archiveEntryInfo->file.byteBuffer,
                                  byteLength
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }
      }
    }
  }
  while (blockCount > 0);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : readFileDataBlock
* Purpose: read file data block, decrypt
* Input  : archiveEntryInfo - archive file entry info data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors readFileDataBlock(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors error;
  ulong  maxBytes;
  ulong  bytesRead;
  ulong  n;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->file.byteCompressInfo.blockLength != 0);

  if      (!Chunk_eofSub(&archiveEntryInfo->file.chunkFileData.info))
  {
    // get max. bytes to read (always multiple of block length)
    maxBytes = FLOOR(MIN(Compress_getFreeCompressSpace(&archiveEntryInfo->file.byteCompressInfo),
                         archiveEntryInfo->file.byteBufferSize
                        ),
                     archiveEntryInfo->file.byteCompressInfo.blockLength
                    );
    assert((maxBytes%archiveEntryInfo->file.byteCompressInfo.blockLength) == 0);

    // read data from archive
    error = Chunk_readData(&archiveEntryInfo->file.chunkFileData.info,
                           archiveEntryInfo->file.byteBuffer,
                           maxBytes,
                           &bytesRead
                          );
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (bytesRead <= 0L)
    {
      return ERROR_READ_FILE;
    }
    if ((bytesRead%archiveEntryInfo->file.byteCompressInfo.blockLength) != 0)
    {
      return ERROR_INCOMPLETE_ARCHIVE;
    }

    // decrypt data
    error = Crypt_decryptBytes(&archiveEntryInfo->file.cryptInfo,
                               archiveEntryInfo->file.byteBuffer,
                               bytesRead
                              );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // put decrypted data into decompressor
    Compress_putCompressedData(&archiveEntryInfo->file.byteCompressInfo,
                               archiveEntryInfo->file.byteBuffer,
                               bytesRead
                              );
  }
  else if (!Compress_isFlush(&archiveEntryInfo->file.byteCompressInfo))
  {
    // no more data in archive -> flush byte-compress
    error = Compress_flush(&archiveEntryInfo->file.byteCompressInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  else
  {
    // check for end-of-compressed byte-data
    error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->file.byteCompressInfo,
                                                   &n
                                                  );
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (n <= 0)
    {
      return ERROR_COMPRESS_EOF;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeImageChunks
* Purpose: write image chunks
* Input  : archiveEntryInfo  - archive image entry info data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeImageChunks(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors error;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(!archiveEntryInfo->image.headerWrittenFlag);

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
    assert(Compress_isXDeltaCompressed(archiveEntryInfo->image.deltaCompressAlgorithm));

    error = Chunk_create(&archiveEntryInfo->image.chunkImageDelta.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Chunk_close(&archiveEntryInfo->image.chunkImageDelta.info);
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

  // mark header "written"
  archiveEntryInfo->image.headerWrittenFlag = TRUE;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : flushImageDataBlocks
* Purpose: flush image data blocks
* Input  : archiveEntryInfo  - archive image entry info data
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
  ulong  maxBlockCount;
  ulong  byteLength;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(archiveEntryInfo->image.byteCompressInfo.blockLength != 0);

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
      // create new part (if not already created)
      if (!archiveEntryInfo->image.headerWrittenFlag)
      {
        writeImageChunks(archiveEntryInfo);
      }

      // get max. number of byte-compressed data blocks to write
      maxBlockCount = MIN(archiveEntryInfo->image.byteBufferSize/archiveEntryInfo->image.byteCompressInfo.blockLength,
                          blockCount
                         );

      // get byte-compressed data
      Compress_getCompressedData(&archiveEntryInfo->image.byteCompressInfo,
                                 archiveEntryInfo->image.byteBuffer,
                                 maxBlockCount*archiveEntryInfo->image.byteCompressInfo.blockLength,
                                 &byteLength
                                );
      assert((byteLength%archiveEntryInfo->image.byteCompressInfo.blockLength) == 0);

      // encrypt block
      error = Crypt_encryptBytes(&archiveEntryInfo->image.cryptInfo,
                                 archiveEntryInfo->image.byteBuffer,
                                 byteLength
                                );
      if (error != ERROR_NONE)
      {
        return error;
      }

      #ifdef DEBUG_ENCODED_DATA_FILENAME
        {
          int handle = open(DEBUG_ENCODED_DATA_FILENAME,O_CREAT|O_WRONLY|O_APPEND|O_LARGEFILE,0664);
          write(handle,archiveEntryInfo->image.byteBuffer,byteLength);
          close(handle);
        }
      #endif /* DEBUG_ENCODED_DATA_FILENAME */

      // store block into chunk
      error = Chunk_writeData(&archiveEntryInfo->image.chunkImageData.info,
                              archiveEntryInfo->image.byteBuffer,
                              byteLength
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
* Name   : writeImageDataBlocks
* Purpose: encrpyt and write image data blocks, split archive
* Input  : archiveEntryInfo  - archive image entry info data
*          allowNewPartFlag  - TRUE iff new archive part can be created
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeImageDataBlocks(ArchiveEntryInfo *archiveEntryInfo,
                                  bool             allowNewPartFlag
                                 )
{
  uint   blockCount;
  ulong  byteLength;
  ulong  minBytes;
  bool   newPartFlag;
  Errors error;
  bool   eofDelta;
  ulong  deltaLength;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(archiveEntryInfo->archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);
  assert(archiveEntryInfo->image.byteCompressInfo.blockLength != 0);

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
      // get next byte-compressed data block (only 1 block, because of splitting)
      assert(archiveEntryInfo->image.byteBufferSize >= (blockCount*archiveEntryInfo->image.byteCompressInfo.blockLength));
      Compress_getCompressedData(&archiveEntryInfo->image.byteCompressInfo,
                                 archiveEntryInfo->image.byteBuffer,
//                                 archiveEntryInfo->image.byteBufferSize,
                                 blockCount*archiveEntryInfo->image.byteCompressInfo.blockLength,
//                                 archiveEntryInfo->image.byteCompressInfo.blockLength,
                                 &byteLength
                                );

      // calculate min. bytes to tramsfer to archive
      minBytes = (ulong)File_getSize(&archiveEntryInfo->image.intermediateFileHandle)+byteLength;

      // lock
      Semaphore_lock(&archiveEntryInfo->archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);

      // check if split is allowed and necessary
      newPartFlag =    allowNewPartFlag
                    && isNewPartNeeded(archiveEntryInfo->archiveInfo,
                                       (!archiveEntryInfo->image.headerWrittenFlag ? archiveEntryInfo->image.headerLength : 0) + minBytes
                                      );

      // split
      if (newPartFlag)
      {
        // write last compressed block (if any)
        if (byteLength > 0L)
        {
          // create new part (if not already created)
          if (!archiveEntryInfo->image.headerWrittenFlag)
          {
            writeImageChunks(archiveEntryInfo);
          }

          // encrypt block
          error = Crypt_encryptBytes(&archiveEntryInfo->image.cryptInfo,
                                     archiveEntryInfo->image.byteBuffer,
                                     byteLength
                                    );
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
            return error;
          }

          #ifdef DEBUG_ENCODED_DATA_FILENAME
            {
              int handle = open64(DEBUG_ENCODED_DATA_FILENAME,O_CREAT|O_WRONLY|O_APPEND|O_LARGEFILE,0664);
              write(handle,archiveEntryInfo->image.byteBuffer,byteLength);
              close(handle);
            }
          #endif /* DEBUG_ENCODED_DATA_FILENAME */

          // store block into chunk
          error = Chunk_writeData(&archiveEntryInfo->image.chunkImageData.info,
                                  archiveEntryInfo->image.byteBuffer,
                                  byteLength
                                );
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
            return error;
          }
        }

        // flush delta compress
        error = Compress_flush(&archiveEntryInfo->image.deltaCompressInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }
        eofDelta = FALSE;
        do
        {
          // flush compressed full data blocks
          error = flushImageDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_FULL);
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
            return error;
          }

          // get next delta-compressed data
          Compress_getCompressedData(&archiveEntryInfo->image.deltaCompressInfo,
                                     archiveEntryInfo->image.deltaBuffer,
                                     archiveEntryInfo->image.deltaBufferSize,
                                     &deltaLength
                                    );
          if (deltaLength > 0)
          {
            // byte-compress data
            error = Compress_deflate(&archiveEntryInfo->image.byteCompressInfo,
                                     archiveEntryInfo->image.deltaBuffer,
                                     deltaLength,
                                     NULL
                                    );
            if (error != ERROR_NONE)
            {
              Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
              return error;
            }
          }
          else
          {
            eofDelta = TRUE;
          }
        }
        while (!eofDelta);

        // flush byte compress
        error = Compress_flush(&archiveEntryInfo->image.byteCompressInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }
        error = flushImageDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_ANY);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }

        // update part size
        assert(archiveEntryInfo->image.blockSize > 0);
        archiveEntryInfo->image.chunkImageData.blockCount = Compress_getInputLength(&archiveEntryInfo->image.deltaCompressInfo)/archiveEntryInfo->image.blockSize;
        error = Chunk_update(&archiveEntryInfo->image.chunkImageData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }

        // close chunks
        error = Chunk_close(&archiveEntryInfo->image.chunkImageData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->image.chunkImageEntry.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->image.chunkImage.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }
        assert(archiveEntryInfo->image.headerWrittenFlag);

        // create archive file
        error = createArchiveFile(archiveEntryInfo->archiveInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }

        // transfer intermediate data into archive
        error = transferArchiveFileData(archiveEntryInfo->archiveInfo,
                                        &archiveEntryInfo->image.intermediateFileHandle
                                       );
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }

        // close archive
        error = closeArchiveFile(archiveEntryInfo->archiveInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }

        // mark header "not written"
        archiveEntryInfo->image.headerWrittenFlag = FALSE;

        // store block offset, count for next fragment
        archiveEntryInfo->image.chunkImageData.blockOffset += archiveEntryInfo->image.chunkImageData.blockCount;
        archiveEntryInfo->image.chunkImageData.blockCount  =  0;

        // set new delta base-offset
        if (archiveEntryInfo->image.deltaSourceHandleInitFlag)
        {
          DeltaSource_setBaseOffset(&archiveEntryInfo->image.deltaSourceHandle,
                               archiveEntryInfo->image.chunkImageData.blockCount*(uint64)archiveEntryInfo->image.blockSize
                              );
        }

        /* reset compress, encrypt (do it here because data if buffered and can be
          processed before a new file is opened)
        */
        Compress_reset(&archiveEntryInfo->image.deltaCompressInfo);
        Compress_reset(&archiveEntryInfo->image.byteCompressInfo);
        Crypt_reset(&archiveEntryInfo->image.cryptInfo,0);

        // find next suitable archive part
        findNextArchivePart(archiveEntryInfo->archiveInfo);

        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
      }
      else
      {
        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);

        if (byteLength > 0L)
        {
          // write header (if not already written)
          if (!archiveEntryInfo->image.headerWrittenFlag)
          {
            error = writeImageChunks(archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }

          // encrypt block
          error = Crypt_encryptBytes(&archiveEntryInfo->image.cryptInfo,
                                     archiveEntryInfo->image.byteBuffer,
                                     byteLength
                                    );
          if (error != ERROR_NONE)
          {
            return error;
          }

          #ifdef DEBUG_ENCODED_DATA_FILENAME
            {
              int handle = open64(DEBUG_ENCODED_DATA_FILENAME,O_CREAT|O_WRONLY|O_APPEND|O_LARGEFILE,0664);
              write(handle,archiveEntryInfo->image.byteBuffer,byteLength);
              close(handle);
            }
          #endif /* DEBUG_ENCODED_DATA_FILENAME */

          // store block into chunk
          error = Chunk_writeData(&archiveEntryInfo->image.chunkImageData.info,
                                  archiveEntryInfo->image.byteBuffer,
                                  byteLength
                                );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }
      }
    }
  }
  while (blockCount > 0);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : readImageDataBlock
* Purpose: read image data block, decrypt
* Input  : archiveEntryInfo - archive image entry info data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors readImageDataBlock(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors error;
  ulong  maxBytes;
  ulong  bytesRead;
  ulong  n;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->image.byteCompressInfo.blockLength != 0);

  if (!Chunk_eofSub(&archiveEntryInfo->image.chunkImageData.info))
  {
    // get max. bytes to read (always multiple of block length)
    maxBytes = FLOOR(MIN(Compress_getFreeCompressSpace(&archiveEntryInfo->image.byteCompressInfo),
                         archiveEntryInfo->image.byteBufferSize
                        ),
                     archiveEntryInfo->image.byteCompressInfo.blockLength
                    );
    assert((maxBytes%archiveEntryInfo->image.byteCompressInfo.blockLength) == 0);

    // read data from archive
    error = Chunk_readData(&archiveEntryInfo->image.chunkImageData.info,
                           archiveEntryInfo->image.byteBuffer,
                           maxBytes,
                           &bytesRead
                          );
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (bytesRead <= 0L)
    {
      return ERROR_READ_FILE;
    }
    if ((bytesRead%archiveEntryInfo->image.byteCompressInfo.blockLength) != 0)
    {
      return ERROR_INCOMPLETE_ARCHIVE;
    }

    // decrypt data
    error = Crypt_decryptBytes(&archiveEntryInfo->image.cryptInfo,
                               archiveEntryInfo->image.byteBuffer,
                               bytesRead
                              );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // put decrypted data into decompressor
    Compress_putCompressedData(&archiveEntryInfo->image.byteCompressInfo,
                               archiveEntryInfo->image.byteBuffer,
                               bytesRead
                              );
  }
  else if (!Compress_isFlush(&archiveEntryInfo->image.byteCompressInfo))
  {
    // no more data in archive -> flush byte-compress
    error = Compress_flush(&archiveEntryInfo->image.byteCompressInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  else
  {
    // check for end-of-compressed byte-data
    error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->image.byteCompressInfo,
                                                   &n
                                                  );
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (n <= 0)
    {
      return ERROR_COMPRESS_EOF;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeHardLinkChunks
* Purpose: write hard link chunks
* Input  : archiveEntryInfo  - archive hard link entry info data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeHardLinkChunks(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors                    error;
  StringNode                *stringNode;
  String                    fileName;
  FileExtendedAttributeNode *fileExtendedAttributeNode;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(!archiveEntryInfo->hardLink.headerWrittenFlag);

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
  STRINGLIST_ITERATE(archiveEntryInfo->hardLink.fileNameList,stringNode,fileName)
  {
    String_set(archiveEntryInfo->hardLink.chunkHardLinkName.name,fileName);

    error = Chunk_create(&archiveEntryInfo->hardLink.chunkHardLinkName.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkName.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // create extended attribute chunks
  LIST_ITERATE(archiveEntryInfo->hardLink.fileExtendedAttributeList,fileExtendedAttributeNode)
  {
    String_set(archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.name,fileExtendedAttributeNode->name);
    archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.value.data   = fileExtendedAttributeNode->data;
    archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.value.length = fileExtendedAttributeNode->dataLength;

    error = Chunk_create(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // create hard link delta chunk
  if (Compress_isCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm))
  {
    assert(Compress_isXDeltaCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm));

    error = Chunk_create(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
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

  // mark header "written"
  archiveEntryInfo->hardLink.headerWrittenFlag = TRUE;

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : flushHardLinkDataBlocks
* Purpose: flush hard link data blocks
* Input  : archiveEntryInfo  - archive hard link entry info data
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
  ulong  maxBlockCount;
  ulong  byteLength;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(archiveEntryInfo->hardLink.byteCompressInfo.blockLength != 0);

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
      // create new part (if not already created)
      if (!archiveEntryInfo->hardLink.headerWrittenFlag)
      {
        writeHardLinkChunks(archiveEntryInfo);
      }

      // get max. number of byte-compressed data blocks to write
      maxBlockCount = MIN(archiveEntryInfo->hardLink.byteBufferSize/archiveEntryInfo->hardLink.byteCompressInfo.blockLength,
                          blockCount
                         );

      // get next byte-compressed data
      Compress_getCompressedData(&archiveEntryInfo->hardLink.byteCompressInfo,
                                 archiveEntryInfo->hardLink.byteBuffer,
                                 maxBlockCount*archiveEntryInfo->hardLink.byteCompressInfo.blockLength,
                                 &byteLength
                                );
      assert((byteLength%archiveEntryInfo->hardLink.byteCompressInfo.blockLength) == 0);

      // encrypt block
      error = Crypt_encryptBytes(&archiveEntryInfo->hardLink.cryptInfo,
                                 archiveEntryInfo->hardLink.byteBuffer,
                                 byteLength
                                );
      if (error != ERROR_NONE)
      {
        return error;
      }

      #ifdef DEBUG_ENCODED_DATA_FILENAME
        {
          int handle = open64(DEBUG_ENCODED_DATA_FILENAME,O_CREAT|O_WRONLY|O_APPEND|O_LARGEFILE,0664);
          write(handle,archiveEntryInfo->hardLink.byteBuffer,byteLength);
          close(handle);
        }
      #endif /* DEBUG_ENCODED_DATA_FILENAME */

      // store block into chunk
      error = Chunk_writeData(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                              archiveEntryInfo->hardLink.byteBuffer,
                              byteLength
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
* Name   : writeHardLinkDataBlocks
* Purpose: encrypt and write hard link block, split archive
* Input  : archiveEntryInfo - archive entry info
*          allowNewPartFlag - TRUE iff new archive part can be created
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeHardLinkDataBlocks(ArchiveEntryInfo *archiveEntryInfo,
                                     bool             allowNewPartFlag
                                    )
{
  uint   blockCount;
  ulong  byteLength;
  ulong  minBytes;
  bool   newPartFlag;
  Errors error;
  bool   eofDelta;
  ulong  deltaLength;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(archiveEntryInfo->archiveInfo->ioType == ARCHIVE_IO_TYPE_FILE);
  assert(archiveEntryInfo->hardLink.byteCompressInfo.blockLength != 0);

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
      // get byte-compressed data blocks
      assert(archiveEntryInfo->hardLink.byteBufferSize >= (blockCount*archiveEntryInfo->hardLink.byteCompressInfo.blockLength));
      Compress_getCompressedData(&archiveEntryInfo->hardLink.byteCompressInfo,
                                 archiveEntryInfo->hardLink.byteBuffer,
//                                 archiveEntryInfo->hardLink.byteBufferSize,
                                 blockCount*archiveEntryInfo->hardLink.byteCompressInfo.blockLength,
//                                 archiveEntryInfo->hardLink.byteCompressInfo.blockLength,
                                 &byteLength
                                );

      // calculate min. bytes to tramsfer to archive
      minBytes = (ulong)File_getSize(&archiveEntryInfo->hardLink.intermediateFileHandle)+byteLength;

      // lock
      Semaphore_lock(&archiveEntryInfo->archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);

      // check if split is allowed and necessary
      newPartFlag =    allowNewPartFlag
                    && isNewPartNeeded(archiveEntryInfo->archiveInfo,
                                       (!archiveEntryInfo->hardLink.headerWrittenFlag ? archiveEntryInfo->hardLink.headerLength : 0) + minBytes
                                      );

      // split
      if (newPartFlag)
      {
        // write last compressed block (if any)
        if (byteLength > 0L)
        {
          // create new part (if not already created)
          if (!archiveEntryInfo->hardLink.headerWrittenFlag)
          {
            writeHardLinkChunks(archiveEntryInfo);
          }

          // encrypt block
          error = Crypt_encryptBytes(&archiveEntryInfo->hardLink.cryptInfo,
                                     archiveEntryInfo->hardLink.byteBuffer,
                                     byteLength
                                    );
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
            return error;
          }

          #ifdef DEBUG_ENCODED_DATA_FILENAME
            {
              int handle = open64(DEBUG_ENCODED_DATA_FILENAME,O_CREAT|O_WRONLY|O_APPEND|O_LARGEFILE,0664);
              write(handle,archiveEntryInfo->hardLink.byteBuffer,byteLength);
              close(handle);
            }
          #endif /* DEBUG_ENCODED_DATA_FILENAME */

          // store block into chunk
          error = Chunk_writeData(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                                  archiveEntryInfo->hardLink.byteBuffer,
                                  byteLength
                                 );
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
            return error;
          }
        }

        // flush delta compress
        error = Compress_flush(&archiveEntryInfo->hardLink.deltaCompressInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }
        eofDelta = FALSE;
        do
        {
          // flush compressed full data blocks
          error = flushHardLinkDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_FULL);
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
            return error;
          }

          // get next delta-compressed byte
          Compress_getCompressedData(&archiveEntryInfo->hardLink.deltaCompressInfo,
                                     archiveEntryInfo->hardLink.deltaBuffer,
                                     archiveEntryInfo->hardLink.deltaBufferSize,
                                     &deltaLength
                                    );
          if (deltaLength > 0)
          {
            // byte-compress data
            error = Compress_deflate(&archiveEntryInfo->hardLink.byteCompressInfo,
                                    archiveEntryInfo->hardLink.deltaBuffer,
                                    deltaLength,
                                    NULL
                                    );
            if (error != ERROR_NONE)
            {
              Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
              return error;
            }
          }
          else
          {
            eofDelta = TRUE;
          }
        }
        while (!eofDelta);

        // flush byte compress
        error = Compress_flush(&archiveEntryInfo->hardLink.byteCompressInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }
        error = flushHardLinkDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_ANY);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }

        // update part size
        archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize = Compress_getInputLength(&archiveEntryInfo->hardLink.deltaCompressInfo);
        error = Chunk_update(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }

        // close chunks
        error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLink.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }
        assert(archiveEntryInfo->hardLink.headerWrittenFlag);

        // create archive file
        error = createArchiveFile(archiveEntryInfo->archiveInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }

        // transfer intermediate data into archive
        error = transferArchiveFileData(archiveEntryInfo->archiveInfo,
                                        &archiveEntryInfo->hardLink.intermediateFileHandle
                                       );
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }

        // close archive
        error = closeArchiveFile(archiveEntryInfo->archiveInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
          return error;
        }

        // mark header "not written"
        archiveEntryInfo->hardLink.headerWrittenFlag = FALSE;

        // store fragment offset, count for next fragment
        archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset += archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize;
        archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize   =  0LL;

        // set new delta base-offset
        if (archiveEntryInfo->image.deltaSourceHandleInitFlag)
        {
          DeltaSource_setBaseOffset(&archiveEntryInfo->hardLink.deltaSourceHandle,
                               archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset
                              );
        }

        /* reset compress, encrypt (do it here because data if buffered and can be
          processed before a new file is opened)
        */
        Compress_reset(&archiveEntryInfo->hardLink.deltaCompressInfo);
        Compress_reset(&archiveEntryInfo->hardLink.byteCompressInfo);
        Crypt_reset(&archiveEntryInfo->hardLink.cryptInfo,0);

        // find next suitable archive part
        findNextArchivePart(archiveEntryInfo->archiveInfo);

        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
      }
      else
      {
        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);

        if (byteLength > 0L)
        {
          // write header (if not already written)
          if (!archiveEntryInfo->hardLink.headerWrittenFlag)
          {
            error = writeHardLinkChunks(archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }

          // encrypt block
          error = Crypt_encryptBytes(&archiveEntryInfo->hardLink.cryptInfo,
                                     archiveEntryInfo->hardLink.byteBuffer,
                                     byteLength
                                    );
          if (error != ERROR_NONE)
          {
            return error;
          }

          #ifdef DEBUG_ENCODED_DATA_FILENAME
            {
              int handle = open64(DEBUG_ENCODED_DATA_FILENAME,O_CREAT|O_WRONLY|O_APPEND|O_LARGEFILE,0664);
              write(handle,archiveEntryInfo->hardLink.byteBuffer,byteLength);
              close(handle);
            }
          #endif /* DEBUG_ENCODED_DATA_FILENAME */

          // write block
          error = Chunk_writeData(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                                  archiveEntryInfo->hardLink.byteBuffer,
                                  byteLength
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }
      }
    }
  }
  while (blockCount > 0);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : readHardLinkDataBlock
* Purpose: read hard link data block, decrypt
* Input  : archiveEntryInfo - archive entry info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors readHardLinkDataBlock(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors error;
  ulong  maxBytes;
  ulong  bytesRead;
  ulong  n;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->hardLink.byteCompressInfo.blockLength != 0);

  if (!Chunk_eofSub(&archiveEntryInfo->hardLink.chunkHardLinkData.info))
  {
    // get max. bytes to read (always multiple of block length)
    maxBytes = FLOOR(MIN(Compress_getFreeCompressSpace(&archiveEntryInfo->hardLink.byteCompressInfo),
                         archiveEntryInfo->hardLink.byteBufferSize
                        ),
                     archiveEntryInfo->hardLink.byteCompressInfo.blockLength
                    );
    assert((maxBytes%archiveEntryInfo->hardLink.byteCompressInfo.blockLength) == 0);

    // read data from archive
    error = Chunk_readData(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                           archiveEntryInfo->hardLink.byteBuffer,
                           maxBytes,
                           &bytesRead
                          );
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (bytesRead <= 0L)
    {
      return ERROR_READ_FILE;
    }
    if ((bytesRead%archiveEntryInfo->hardLink.byteCompressInfo.blockLength) != 0)
    {
      return ERROR_INCOMPLETE_ARCHIVE;
    }

    // decrypt data
    error = Crypt_decryptBytes(&archiveEntryInfo->hardLink.cryptInfo,
                               archiveEntryInfo->hardLink.byteBuffer,
                               bytesRead
                              );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // put decrypted data into decompressor
    Compress_putCompressedData(&archiveEntryInfo->hardLink.byteCompressInfo,
                               archiveEntryInfo->hardLink.byteBuffer,
                               bytesRead
                              );
  }
  else if (!Compress_isFlush(&archiveEntryInfo->hardLink.byteCompressInfo))
  {
    // no more data in archive -> flush byte-compress
    error = Compress_flush(&archiveEntryInfo->hardLink.byteCompressInfo);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  else
  {
    // check for end-of-compressed byte-data
    error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->hardLink.byteCompressInfo,
                                                   &n
                                                  );
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (n <= 0)
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
  Semaphore_init(&decryptPasswordList.lock);
  List_init(&decryptPasswordList);
  decryptPasswordList.newPasswordNode = NULL;

  return ERROR_NONE;
}

void Archive_doneAll(void)
{
  List_done(&decryptPasswordList,(ListNodeFreeFunction)freePasswordNode,NULL);
  Semaphore_done(&decryptPasswordList.lock);
}

bool Archive_parseArchiveEntryType(const char *name, ArchiveEntryTypes *archiveEntryType)
{
  uint z;

  assert(name != NULL);
  assert(archiveEntryType != NULL);

  z = 0;
  while (   (z < SIZE_OF_ARRAY(ARCHIVE_ENTRY_TYPES))
         && !stringEqualsIgnoreCase(ARCHIVE_ENTRY_TYPES[z].name,name)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(ARCHIVE_ENTRY_TYPES))
  {
    (*archiveEntryType) = ARCHIVE_ENTRY_TYPES[z].archiveEntryType;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Archive_isArchiveFile(ConstString fileName)
{
  FileHandle  fileHandle;
  Errors      error;
  ChunkHeader chunkHeader;

  error = File_open(&fileHandle,
                    fileName,
                    FILE_OPEN_READ
                   );
  if (error != ERROR_NONE)
  {
    return false;
  }

  error = Chunk_next(&CHUNK_IO_FILE,&fileHandle,&chunkHeader);
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    return error;
  }

  File_close(&fileHandle);

  return    (chunkHeader.id == CHUNK_ID_BAR)
         && (chunkHeader.size == sizeof(uint32));
}

void Archive_clearDecryptPasswords(void)
{
  SemaphoreLock semaphoreLock;

  SEMAPHORE_LOCKED_DO(semaphoreLock,&decryptPasswordList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    List_clear(&decryptPasswordList,(ListNodeFreeFunction)freePasswordNode,NULL);
  }
}

const Password *Archive_appendDecryptPassword(const Password *password)
{
  PasswordNode  *passwordNode;
  SemaphoreLock semaphoreLock;

  assert(password != NULL);

  passwordNode = NULL;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&decryptPasswordList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find password
    passwordNode = decryptPasswordList.head;
    while ((passwordNode != NULL) && !Password_equals(passwordNode->password,password))
    {
      passwordNode = passwordNode->next;
    }
    if (passwordNode == NULL)
    {
      // add password
      passwordNode = LIST_NEW_NODE(PasswordNode);
      if (passwordNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      passwordNode->password = Password_duplicate(password);

      List_append(&decryptPasswordList,passwordNode);
    }

    // set reference to new password
    decryptPasswordList.newPasswordNode = passwordNode;
  }
  assert(passwordNode != NULL);

  return passwordNode->password;
}

bool Archive_waitDecryptPassword(Password *password, long timeout)
{
  SemaphoreLock semaphoreLock;
  bool          modified;

  Password_clear(password);
  SEMAPHORE_LOCKED_DO(semaphoreLock,&decryptPasswordList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    modified = Semaphore_waitModified(&decryptPasswordList.lock,timeout);
    if (decryptPasswordList.newPasswordNode != NULL)
    {
      Password_set(password,decryptPasswordList.newPasswordNode->password);
    }
  }

  return modified;
}

#ifdef NDEBUG
  Errors Archive_create(ArchiveInfo                     *archiveInfo,
                        DeltaSourceList                 *deltaSourceList,
                        const JobOptions                *jobOptions,
                        IndexHandle                     *indexHandle,
                        ConstString                     jobUUID,
                        ConstString                     scheduleUUID,
                        ArchiveTypes                    archiveType,
                        ArchiveInitFunction             archiveInitFunction,
                        void                            *archiveInitUserData,
                        ArchiveDoneFunction             archiveDoneFunction,
                        void                            *archiveDoneUserData,
                        ArchiveGetSizeFunction          archiveGetSizeFunction,
                        void                            *archiveGetSizeUserData,
                        ArchiveStoreFunction            archiveStoreFunction,
                        void                            *archiveStoreUserData,
                        GetPasswordFunction             getPasswordFunction,
                        void                            *getPasswordUserData,
                        LogHandle                       *logHandle
                       )
#else /* not NDEBUG */
  Errors __Archive_create(const char                      *__fileName__,
                          ulong                           __lineNb__,
                          ArchiveInfo                     *archiveInfo,
                          DeltaSourceList                 *deltaSourceList,
                          const JobOptions                *jobOptions,
                          IndexHandle                     *indexHandle,
                          ConstString                     jobUUID,
                          ConstString                     scheduleUUID,
                          ArchiveTypes                    archiveType,
                          ArchiveInitFunction             archiveInitFunction,
                          void                            *archiveInitUserData,
                          ArchiveDoneFunction             archiveDoneFunction,
                          void                            *archiveDoneUserData,
                          ArchiveGetSizeFunction          archiveGetSizeFunction,
                          void                            *archiveGetSizeUserData,
                          ArchiveStoreFunction            archiveStoreFunction,
                          void                            *archiveStoreUserData,
                          GetPasswordFunction             getPasswordFunction,
                          void                            *getPasswordUserData,
                          LogHandle                       *logHandle
                         )
#endif /* NDEBUG */
{
  AutoFreeList autoFreeList;
  Errors       error;
  bool         okFlag;
  ulong        maxCryptKeyDataLength;

  assert(archiveInfo != NULL);
  assert(archiveStoreFunction != NULL);
  assert(jobOptions != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(jobOptions->cryptAlgorithm,&archiveInfo->blockLength);
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  assert(archiveInfo->blockLength > 0);
  if (archiveInfo->blockLength > MAX_BUFFER_SIZE)
  {
    AutoFree_cleanup(&autoFreeList);
    return ERROR_UNSUPPORTED_BLOCK_SIZE;
  }

  // init archive info
  archiveInfo->deltaSourceList         = deltaSourceList;
  archiveInfo->jobOptions              = jobOptions;
  archiveInfo->archiveInitFunction     = archiveInitFunction;
  archiveInfo->archiveInitUserData     = archiveInitUserData;
  archiveInfo->archiveDoneFunction     = archiveDoneFunction;
  archiveInfo->archiveDoneUserData     = archiveDoneUserData;
  archiveInfo->archiveGetSizeFunction  = archiveGetSizeFunction;
  archiveInfo->archiveGetSizeUserData  = archiveGetSizeUserData;
  archiveInfo->archiveStoreFunction    = archiveStoreFunction;
  archiveInfo->archiveStoreUserData    = archiveStoreUserData;
  archiveInfo->getPasswordFunction     = getPasswordFunction;
  archiveInfo->getPasswordUserData     = getPasswordUserData;
  archiveInfo->logHandle               = logHandle;

  Semaphore_init(&archiveInfo->passwordLock);
  archiveInfo->cryptType               = Crypt_isEncrypted(jobOptions->cryptAlgorithm) ? jobOptions->cryptType : CRYPT_TYPE_NONE;
  archiveInfo->cryptPassword           = NULL;
  archiveInfo->cryptPasswordReadFlag   = FALSE;
  archiveInfo->cryptKeyData            = NULL;
  archiveInfo->cryptKeyDataLength      = 0;

  archiveInfo->ioType                  = ARCHIVE_IO_TYPE_FILE;
  archiveInfo->file.fileName           = String_new();
  archiveInfo->file.openFlag           = FALSE;
  archiveInfo->printableStorageName    = NULL;
  Semaphore_init(& archiveInfo->chunkIOLock);
  archiveInfo->chunkIO                 = &CHUNK_IO_FILE;
  archiveInfo->chunkIOUserData         = &archiveInfo->file.fileHandle;

  archiveInfo->indexHandle             = indexHandle;
  archiveInfo->jobUUID                 = String_duplicate(jobUUID);
  archiveInfo->scheduleUUID            = String_duplicate(scheduleUUID);
  archiveInfo->archiveType             = archiveType;
//  archiveInfo->entityId                = DATABASE_ID_NONE;
  archiveInfo->storageId               = DATABASE_ID_NONE;

  archiveInfo->entries                 = 0LL;
  archiveInfo->archiveFileSize         = 0LL;
  archiveInfo->partNumber              = 0;

  archiveInfo->pendingError            = ERROR_NONE;
  archiveInfo->nextChunkHeaderReadFlag = FALSE;

  archiveInfo->interrupt.openFlag      = FALSE;
  archiveInfo->interrupt.offset        = 0LL;
  AUTOFREE_ADD(&autoFreeList,&archiveInfo->passwordLock,{ Semaphore_done(&archiveInfo->passwordLock); });
  AUTOFREE_ADD(&autoFreeList,&archiveInfo->file.fileName,{ String_delete(archiveInfo->file.fileName); });
  AUTOFREE_ADD(&autoFreeList,&archiveInfo->chunkIOLock,{ Semaphore_done(&archiveInfo->chunkIOLock); });

  // init key (if asymmetric encryption used)
  if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
  {
    // check if public key available
    if (jobOptions->cryptPublicKeyFileName == NULL)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_PUBLIC_KEY;
    }

    // read public key
    Crypt_initKey(&archiveInfo->cryptKey,CRYPT_PADDING_TYPE_NONE);
    error = Crypt_readKeyFile(&archiveInfo->cryptKey,
                              jobOptions->cryptPublicKeyFileName,
                              NULL
                             );
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // create new random key for encryption
    archiveInfo->cryptPassword = Password_new();
    if (archiveInfo->cryptPassword == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    AUTOFREE_ADD(&autoFreeList,&archiveInfo->cryptPassword,{ Password_delete(archiveInfo->cryptPassword); });
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
        free(archiveInfo->cryptKeyData);
        AutoFree_cleanup(&autoFreeList);
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
    AUTOFREE_ADD(&autoFreeList,&archiveInfo->cryptKeyData,{ free(archiveInfo->cryptKeyData); });
#if 0
Password_dump(archiveInfo->cryptPassword);
{
int z;
byte *p=archiveInfo->cryptKeyData;
fprintf(stderr,"data: ");for (z=0;z<archiveInfo->cryptKeyDataLength;z++) fprintf(stderr,"%02x",p[z]); fprintf(stderr,"\n");
}
#endif /* 0 */
  }

fprintf(stderr,"%s, %d: xxxxxxxxxxxxxxxxxxxxxxxxx\n",__FILE__,__LINE__);
#if 0
  // create index
  if (   (archiveInfo->indexHandle != NULL)
      && !archiveInfo->jobOptions->noIndexDatabaseFlag
      && !archiveInfo->jobOptions->dryRunFlag
      && !archiveInfo->jobOptions->noStorageFlag
     )
  {
    error = Index_newEntity(archiveInfo->indexHandle,
                            jobUUID,
                            scheduleUUID,
                            archiveType,
                            &archiveInfo->entityId
                           );
    if (error != ERROR_NONE)
    {
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }
fprintf(stderr,"%s, %d:crate emn %llu \n",__FILE__,__LINE__,archiveInfo->entityId);
  }
#endif

  // free resources
  AutoFree_done(&autoFreeList);

  #ifdef DEBUG_ENCODED_DATA_FILENAME
    {
      int handle = open64(DEBUG_ENCODED_DATA_FILENAME,O_CREAT|O_WRONLY|O_TRUNC|O_LARGEFILE,0664);
      close(handle);
    }
  #endif /* DEBUG_ENCODED_DATA_FILENAME */

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveInfo,sizeof(ArchiveInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveInfo,sizeof(ArchiveInfo));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_open(ArchiveInfo         *archiveInfo,
                      StorageHandle       *storageHandle,
                      StorageSpecifier    *storageSpecifier,
                      ConstString         fileName,
                      DeltaSourceList     *deltaSourceList,
                      const JobOptions    *jobOptions,
                      GetPasswordFunction getPasswordFunction,
                      void                *getPasswordUserData,
                      LogHandle           *logHandle
                     )
#else /* not NDEBUG */
  Errors __Archive_open(const char          *__fileName__,
                        ulong               __lineNb__,
                        ArchiveInfo         *archiveInfo,
                        StorageHandle       *storageHandle,
                        StorageSpecifier    *storageSpecifier,
                        ConstString         fileName,
                        DeltaSourceList     *deltaSourceList,
                        const JobOptions    *jobOptions,
                        GetPasswordFunction getPasswordFunction,
                        void                *getPasswordUserData,
                        LogHandle           *logHandle
                       )
#endif /* NDEBUG */
{
  AutoFreeList autoFreeList;
  Errors       error;
  ChunkHeader  chunkHeader;

  assert(archiveInfo != NULL);
  assert(storageSpecifier != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  archiveInfo->deltaSourceList         = deltaSourceList;
  archiveInfo->jobOptions              = jobOptions;
  archiveInfo->archiveInitFunction     = NULL;
  archiveInfo->archiveInitUserData     = NULL;
  archiveInfo->archiveDoneFunction     = NULL;
  archiveInfo->archiveDoneUserData     = NULL;
  archiveInfo->archiveStoreFunction    = NULL;
  archiveInfo->archiveStoreUserData    = NULL;
  archiveInfo->getPasswordFunction     = getPasswordFunction;
  archiveInfo->getPasswordUserData     = getPasswordUserData;
  archiveInfo->logHandle               = logHandle;

  Semaphore_init(&archiveInfo->passwordLock);
  archiveInfo->cryptPassword           = NULL;
  archiveInfo->cryptPasswordReadFlag   = FALSE;
  archiveInfo->cryptType               = CRYPT_TYPE_NONE;
  archiveInfo->cryptKeyData            = NULL;
  archiveInfo->cryptKeyDataLength      = 0;

  archiveInfo->ioType                  = ARCHIVE_IO_TYPE_STORAGE_FILE;
  Storage_duplicateSpecifier(&archiveInfo->storage.storageSpecifier,storageSpecifier);
  archiveInfo->storage.storageHandle   = storageHandle;
  archiveInfo->printableStorageName    = String_duplicate(Storage_getPrintableName(storageSpecifier,fileName));
  Semaphore_init(&archiveInfo->chunkIOLock);
  archiveInfo->chunkIO                 = &CHUNK_IO_STORAGE_FILE;
  archiveInfo->chunkIOUserData         = &archiveInfo->storage.storageArchiveHandle;

  archiveInfo->indexHandle             = NULL;
  archiveInfo->jobUUID                 = NULL;
  archiveInfo->scheduleUUID            = NULL;
//  archiveInfo->entityId                = DATABASE_ID_NONE;
  archiveInfo->storageId               = DATABASE_ID_NONE;

  archiveInfo->entries                 = 0LL;
  archiveInfo->archiveFileSize         = 0LL;
  archiveInfo->partNumber              = 0;

  archiveInfo->pendingError            = ERROR_NONE;
  archiveInfo->nextChunkHeaderReadFlag = FALSE;

  archiveInfo->interrupt.openFlag      = FALSE;
  archiveInfo->interrupt.offset        = 0LL;
  AUTOFREE_ADD(&autoFreeList,&archiveInfo->passwordLock,{ Semaphore_done(&archiveInfo->passwordLock); });
  AUTOFREE_ADD(&autoFreeList,&archiveInfo->storage.storageSpecifier,{ Storage_doneSpecifier(&archiveInfo->storage.storageSpecifier); });
  AUTOFREE_ADD(&autoFreeList,archiveInfo->printableStorageName,{ String_delete(archiveInfo->printableStorageName); });
  AUTOFREE_ADD(&autoFreeList,&archiveInfo->chunkIOLock,{ Semaphore_done(&archiveInfo->chunkIOLock); });

  error = Storage_open(&archiveInfo->storage.storageArchiveHandle,
                       archiveInfo->storage.storageHandle,
                       fileName
                      );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,archiveInfo->storage.storageHandle,{ Storage_close(&archiveInfo->storage.storageArchiveHandle); });
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  // check if BAR archive file
  error = getNextChunkHeader(archiveInfo,&chunkHeader);
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  if (chunkHeader.id != CHUNK_ID_BAR)
  {
    if (!jobOptions->noStopOnErrorFlag)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NOT_AN_ARCHIVE_FILE;
    }
    else
    {
      printWarning("No BAR header found! This may be a broken archive or not an archive.\n");
    }
  }
  ungetNextChunkHeader(archiveInfo,&chunkHeader);
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  // free resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveInfo,sizeof(ArchiveInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveInfo,sizeof(ArchiveInfo));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_close(ArchiveInfo *archiveInfo)
#else /* not NDEBUG */
  Errors __Archive_close(const char  *__fileName__,
                         ulong       __lineNb__,
                         ArchiveInfo *archiveInfo
                        )
#endif /* NDEBUG */
{
  Errors error;

  assert(archiveInfo != NULL);

  #ifndef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveInfo,sizeof(ArchiveInfo));
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACE(archiveInfo,sizeof(ArchiveInfo));
  #endif /* NDEBUG */

  // init variables
  error = ERROR_UNKNOWN;

  // close file/storage
  switch (archiveInfo->ioType)
  {
    case ARCHIVE_IO_TYPE_FILE:
      error = closeArchiveFile(archiveInfo);
      break;
    case ARCHIVE_IO_TYPE_STORAGE_FILE:
      Storage_close(&archiveInfo->storage.storageArchiveHandle);
      error = ERROR_NONE;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  // free resources
  if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
  {
    assert(archiveInfo->cryptKeyData != NULL);

    free(archiveInfo->cryptKeyData);
    Crypt_doneKey(&archiveInfo->cryptKey);
  }

  if (archiveInfo->jobUUID != NULL) String_delete(archiveInfo->jobUUID);
  if (archiveInfo->scheduleUUID != NULL) String_delete(archiveInfo->scheduleUUID);

  Semaphore_done(&archiveInfo->chunkIOLock);
  if (archiveInfo->cryptPassword  != NULL) Password_delete(archiveInfo->cryptPassword);
  String_delete(archiveInfo->printableStorageName);
  switch (archiveInfo->ioType)
  {
    case ARCHIVE_IO_TYPE_FILE:
      if (archiveInfo->file.fileName != NULL) String_delete(archiveInfo->file.fileName);
      break;
    case ARCHIVE_IO_TYPE_STORAGE_FILE:
      Storage_doneSpecifier(&archiveInfo->storage.storageSpecifier);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  Semaphore_done(&archiveInfo->passwordLock);

  return error;
}

Errors Archive_storageInterrupt(ArchiveInfo *archiveInfo)
{
  SemaphoreLock semaphoreLock;
  Errors        error;

  assert(archiveInfo != NULL);

  switch (archiveInfo->ioType)
  {
    case ARCHIVE_IO_TYPE_FILE:
      SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        archiveInfo->interrupt.openFlag = archiveInfo->file.openFlag;
        if (archiveInfo->file.openFlag)
        {
          error = File_tell(&archiveInfo->file.fileHandle,&archiveInfo->interrupt.offset);
          if (error != ERROR_NONE)
          {
            return error;
          }
          File_close(&archiveInfo->file.fileHandle);
          archiveInfo->file.openFlag = FALSE;
        }
      }
      break;
    case ARCHIVE_IO_TYPE_STORAGE_FILE:
      error = Storage_tell(&archiveInfo->storage.storageArchiveHandle,&archiveInfo->interrupt.offset);
      if (error != ERROR_NONE)
      {
        return error;
      }
      Storage_close(&archiveInfo->storage.storageArchiveHandle);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

Errors Archive_storageContinue(ArchiveInfo *archiveInfo)
{
  SemaphoreLock semaphoreLock;
  Errors        error;

  assert(archiveInfo != NULL);

  switch (archiveInfo->ioType)
  {
    case ARCHIVE_IO_TYPE_FILE:
      SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
      {
        if (archiveInfo->interrupt.openFlag)
        {
          error = File_open(&archiveInfo->file.fileHandle,archiveInfo->file.fileName,FILE_OPEN_WRITE);
          if (error != ERROR_NONE)
          {
            return error;
          }
#ifndef WERROR
#warning seek?
#endif
          error = File_seek(&archiveInfo->file.fileHandle,archiveInfo->interrupt.offset);
          if (error != ERROR_NONE)
          {
            return error;
          }
        }
        archiveInfo->file.openFlag = archiveInfo->interrupt.openFlag;
      }
      break;
    case ARCHIVE_IO_TYPE_STORAGE_FILE:
      error = Storage_open(&archiveInfo->storage.storageArchiveHandle,
                           archiveInfo->storage.storageHandle,
                           NULL  // archiveName
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }
#ifndef WERROR
#warning seek?
#endif
      error = Storage_seek(&archiveInfo->storage.storageArchiveHandle,archiveInfo->interrupt.offset);
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

bool Archive_eof(ArchiveInfo *archiveInfo,
                 bool        skipUnknownChunksFlag
                )
{
  bool           chunkHeaderFoundFlag;
  bool           scanFlag;
  ChunkHeader    chunkHeader;
  bool           decryptedFlag;
  PasswordHandle passwordHandle;
  const Password *password;

  assert(archiveInfo != NULL);
  assert(archiveInfo->chunkIO != NULL);
  assert(archiveInfo->chunkIO->seek != NULL);
  assert(archiveInfo->jobOptions != NULL);

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    return FALSE;
  }

  // find next file, image, directory, link, hard link, special chunk
  chunkHeaderFoundFlag = FALSE;
  scanFlag             = FALSE;
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
        scanFlag = FALSE;
        break;
      case CHUNK_ID_KEY:
        // check if private key available
        if (archiveInfo->jobOptions->cryptPrivateKeyFileName == NULL)
        {
          archiveInfo->pendingError = ERROR_NO_PRIVATE_KEY;
          return FALSE;
        }

        // read private key, try to read key with no password, all passwords
        Crypt_initKey(&archiveInfo->cryptKey,CRYPT_PADDING_TYPE_NONE);
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
                                           archiveInfo,
                                           archiveInfo->jobOptions,
                                           archiveInfo->jobOptions->cryptPasswordMode,
                                           archiveInfo->getPasswordFunction,
                                           archiveInfo->getPasswordUserData
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
          archiveInfo->pendingError = ERROR_KEY_ENCRYPT_FAIL;
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

        scanFlag = FALSE;
        break;
      case CHUNK_ID_FILE:
      case CHUNK_ID_IMAGE:
      case CHUNK_ID_DIRECTORY:
      case CHUNK_ID_LINK:
      case CHUNK_ID_HARDLINK:
      case CHUNK_ID_SPECIAL:
        chunkHeaderFoundFlag = TRUE;

        scanFlag = FALSE;
        break;
      default:
        if (skipUnknownChunksFlag)
        {
          // unknown chunk -> switch to scan mode
          if (!scanFlag)
          {
            if (globalOptions.verboseLevel >= 3)
            {
              printWarning("Skipped unknown chunk '%s' (offset %llu) in '%s'. Switch to scan mode.\n",
                           Chunk_idToString(chunkHeader.id),
                           chunkHeader.offset,
                           String_cString(archiveInfo->printableStorageName)
                          );
            }

            scanFlag = TRUE;
          }

          // skip 1 byte
          archiveInfo->pendingError = archiveInfo->chunkIO->seek(archiveInfo->chunkIOUserData,chunkHeader.offset+1LL);
          if (archiveInfo->pendingError != ERROR_NONE)
          {
            return FALSE;
          }
        }
        else
        {
          // report unknown chunk
          archiveInfo->pendingError = ERROR_UNKNOWN_CHUNK;
          printWarning("Skipped unknown chunk '%s' (offset %llu) in '%s'. Switch to scan mode.\n",
                       Chunk_idToString(chunkHeader.id),
                       chunkHeader.offset,
                       String_cString(archiveInfo->printableStorageName)
                      );
          return FALSE;
        }
        break;
    }
  }

  // store chunk header for read
  if (chunkHeaderFoundFlag)
  {
    ungetNextChunkHeader(archiveInfo,&chunkHeader);
  }

  return !chunkHeaderFoundFlag;
}

#ifdef NDEBUG
  Errors Archive_newFileEntry(ArchiveEntryInfo                *archiveEntryInfo,
                              ArchiveInfo                     *archiveInfo,
                              ConstString                     fileName,
                              const FileInfo                  *fileInfo,
                              const FileExtendedAttributeList *fileExtendedAttributeList,
                              const bool                      deltaCompressFlag,
                              const bool                      byteCompressFlag
                             )
#else /* not NDEBUG */
  Errors __Archive_newFileEntry(const char                      *__fileName__,
                                ulong                           __lineNb__,
                                ArchiveEntryInfo                *archiveEntryInfo,
                                ArchiveInfo                     *archiveInfo,
                                ConstString                     fileName,
                                const FileInfo                  *fileInfo,
                                const FileExtendedAttributeList *fileExtendedAttributeList,
                                const bool                      deltaCompressFlag,
                                const bool                      byteCompressFlag
                               )
#endif /* NDEBUG */
{
  AutoFreeList                    autoFreeList;
  Errors                          error;
  const FileExtendedAttributeNode *fileExtendedAttributeNode;

  assert(archiveEntryInfo != NULL);
  assert(archiveInfo != NULL);
  assert(archiveInfo->jobOptions != NULL);
  assert(archiveInfo->blockLength > 0);
  assert(fileInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  // init crypt password
  error = initCryptPassword(archiveInfo);
  if (error !=  ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // init archive entry info
  archiveEntryInfo->archiveInfo                    = archiveInfo;
  archiveEntryInfo->mode                           = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm                 = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength                    = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType               = ARCHIVE_ENTRY_TYPE_FILE;

  archiveEntryInfo->file.deltaCompressAlgorithm    = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->file.byteCompressAlgorithm     = byteCompressFlag ? archiveInfo->jobOptions->compressAlgorithms.byte : COMPRESS_ALGORITHM_NONE;

  archiveEntryInfo->file.fileExtendedAttributeList = fileExtendedAttributeList;

  archiveEntryInfo->file.deltaSourceHandleInitFlag      = FALSE;

  archiveEntryInfo->file.headerLength              = 0;
  archiveEntryInfo->file.headerWrittenFlag         = FALSE;

  archiveEntryInfo->file.byteBuffer                = NULL;
  archiveEntryInfo->file.byteBufferSize            = 0L;
  archiveEntryInfo->file.deltaBuffer               = NULL;
  archiveEntryInfo->file.deltaBufferSize           = 0L;

  // get intermediate output file
  error = File_getTmpFile(&archiveEntryInfo->file.intermediateFileHandle,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { (void)File_close(&archiveEntryInfo->file.intermediateFileHandle); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.intermediateFileHandle,{ (void)File_close(&archiveEntryInfo->file.intermediateFileHandle); });

  // allocate buffers
  archiveEntryInfo->file.byteBufferSize = FLOOR(MAX_BUFFER_SIZE,archiveEntryInfo->blockLength);
  archiveEntryInfo->file.byteBuffer = (byte*)malloc(archiveEntryInfo->file.byteBufferSize);
  if (archiveEntryInfo->file.byteBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,archiveEntryInfo->file.byteBuffer,{ free(archiveEntryInfo->file.byteBuffer); });
  archiveEntryInfo->file.deltaBufferSize = MAX_BUFFER_SIZE;
  archiveEntryInfo->file.deltaBuffer = (byte*)malloc(archiveEntryInfo->file.deltaBufferSize);
  if (archiveEntryInfo->file.deltaBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,archiveEntryInfo->file.deltaBuffer,{ free(archiveEntryInfo->file.deltaBuffer); });

  // get source for delta-compression
  if (deltaCompressFlag)
  {
    error = DeltaSource_openEntry(&archiveEntryInfo->file.deltaSourceHandle,
                                  archiveEntryInfo->archiveInfo->deltaSourceList,
                                  NULL, // storageName
                                  fileName,
                                  SOURCE_SIZE_UNKNOWN,
                                  archiveInfo->jobOptions
                                 );
    if      (error == ERROR_NONE)
    {
      archiveEntryInfo->file.deltaSourceHandleInitFlag   = TRUE;
      archiveEntryInfo->file.deltaCompressAlgorithm = archiveInfo->jobOptions->compressAlgorithms.delta;
      AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.deltaSourceHandle,{ DeltaSource_closeEntry(&archiveEntryInfo->file.deltaSourceHandle); });
    }
    else if (archiveInfo->jobOptions->forceDeltaCompressionFlag)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    else
    {
      printWarning("File '%s' not delta compressed (no source file found)\n",String_cString(fileName));
      logMessage(archiveEntryInfo->archiveInfo->logHandle,
                 LOG_TYPE_WARNING,
                 "File '%s' not delta compressed (no source file found)\n",
                 String_cString(fileName)
                );
    }
  }

  // init file chunk
  error = Chunk_init(&archiveEntryInfo->file.chunkFile.info,
                     NULL,  // parentChunkInfo
                     &CHUNK_IO_FILE,
                     &archiveEntryInfo->file.intermediateFileHandle,
                     CHUNK_ID_FILE,
                     CHUNK_DEFINITION_FILE,
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->file.chunkFile
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Chunk_done(&archiveEntryInfo->file.chunkFile.info); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  archiveEntryInfo->file.chunkFile.compressAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->file.byteCompressAlgorithm);
  archiveEntryInfo->file.chunkFile.cryptAlgorithm    = CRYPT_ALGORITHM_TO_CONSTANT(archiveInfo->jobOptions->cryptAlgorithm);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFile.info,{ Chunk_done(&archiveEntryInfo->file.chunkFile.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->file.chunkFileEntry.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->file.chunkFileDelta.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileDelta.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->file.chunkFileData.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileData.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->file.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.cryptInfo); });

  // init sub-chunks
  error = Chunk_init(&archiveEntryInfo->file.chunkFileEntry.info,
                     &archiveEntryInfo->file.chunkFile.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_FILE_ENTRY,
                     CHUNK_DEFINITION_FILE_ENTRY,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->file.chunkFileEntry.cryptInfo,
                     &archiveEntryInfo->file.chunkFileEntry
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  archiveEntryInfo->file.chunkFileEntry.size            = fileInfo->size;
  archiveEntryInfo->file.chunkFileEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveEntryInfo->file.chunkFileEntry.timeModified    = fileInfo->timeModified;
  archiveEntryInfo->file.chunkFileEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveEntryInfo->file.chunkFileEntry.userId          = fileInfo->userId;
  archiveEntryInfo->file.chunkFileEntry.groupId         = fileInfo->groupId;
  archiveEntryInfo->file.chunkFileEntry.permission      = fileInfo->permission;
  String_set(archiveEntryInfo->file.chunkFileEntry.name,fileName);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileEntry.info,{ Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info); });

  error = Chunk_init(&archiveEntryInfo->file.chunkFileExtendedAttribute.info,
                     &archiveEntryInfo->file.chunkFile.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_FILE_EXTENDED_ATTRIBUTE,
                     CHUNK_DEFINITION_FILE_EXTENDED_ATTRIBUTE,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo,
                     &archiveEntryInfo->file.chunkFileExtendedAttribute
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Chunk_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.info); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileExtendedAttribute.info,{ Chunk_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.info); });

  error = Chunk_init(&archiveEntryInfo->file.chunkFileDelta.info,
                     &archiveEntryInfo->file.chunkFile.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_FILE_DELTA,
                     CHUNK_DEFINITION_FILE_DELTA,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->file.chunkFileDelta.cryptInfo,
                     &archiveEntryInfo->file.chunkFileDelta
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  if (Compress_isCompressed(archiveEntryInfo->file.deltaCompressAlgorithm))
  {
    assert(Compress_isXDeltaCompressed(archiveEntryInfo->file.deltaCompressAlgorithm));

    archiveEntryInfo->file.chunkFileDelta.deltaAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->file.deltaCompressAlgorithm);
    String_set(archiveEntryInfo->file.chunkFileDelta.name,DeltaSource_getName(&archiveEntryInfo->file.deltaSourceHandle));
    archiveEntryInfo->file.chunkFileDelta.size           = DeltaSource_getSize(&archiveEntryInfo->file.deltaSourceHandle);
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileDelta.info,{ Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info); });

  error = Chunk_init(&archiveEntryInfo->file.chunkFileData.info,
                     &archiveEntryInfo->file.chunkFile.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_FILE_DATA,
                     CHUNK_DEFINITION_FILE_DATA,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->file.chunkFileData.cryptInfo,
                     &archiveEntryInfo->file.chunkFileData
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Chunk_done(&archiveEntryInfo->file.chunkFileData.info); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  archiveEntryInfo->file.chunkFileData.fragmentOffset = 0LL;
  archiveEntryInfo->file.chunkFileData.fragmentSize   = 0LL;
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileData.info,{ Chunk_done(&archiveEntryInfo->file.chunkFileData.info); });

  // init delta compress (if no delta-compression is enabled, use identity-compressor), byte compress
  error = Compress_init(&archiveEntryInfo->file.deltaCompressInfo,
                        COMPRESS_MODE_DEFLATE,
                        archiveEntryInfo->file.deltaCompressAlgorithm,
                        1,
                        &archiveEntryInfo->file.deltaSourceHandle
                       );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Compress_done(&archiveEntryInfo->file.deltaCompressInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.deltaCompressInfo,{ Compress_done(&archiveEntryInfo->file.deltaCompressInfo); });

  // init byte compress
  error = Compress_init(&archiveEntryInfo->file.byteCompressInfo,
                        COMPRESS_MODE_DEFLATE,
                        archiveEntryInfo->file.byteCompressAlgorithm,
                        archiveEntryInfo->blockLength,
                        NULL
                       );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Compress_done(&archiveEntryInfo->file.byteCompressInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.byteCompressInfo,{ Compress_done(&archiveEntryInfo->file.byteCompressInfo); });

  // calculate header size
  archiveEntryInfo->file.headerLength = Chunk_getSize(&archiveEntryInfo->file.chunkFile.info,     &archiveEntryInfo->file.chunkFile,     0)+
                                        Chunk_getSize(&archiveEntryInfo->file.chunkFileEntry.info,&archiveEntryInfo->file.chunkFileEntry,0)+
                                        Chunk_getSize(&archiveEntryInfo->file.chunkFileData.info, &archiveEntryInfo->file.chunkFileData, 0);
  LIST_ITERATE(archiveEntryInfo->file.fileExtendedAttributeList,fileExtendedAttributeNode)
  {
    String_set(archiveEntryInfo->file.chunkFileExtendedAttribute.name,fileExtendedAttributeNode->name);

    archiveEntryInfo->file.headerLength += Chunk_getSize(&archiveEntryInfo->file.chunkFileExtendedAttribute.info,
                                                         &archiveEntryInfo->file.chunkFileExtendedAttribute,
                                                         fileExtendedAttributeNode->dataLength
                                                        );
  }
  if (Compress_isCompressed(archiveEntryInfo->file.deltaCompressAlgorithm))
  {
    archiveEntryInfo->file.chunkFileDelta.deltaAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->file.deltaCompressAlgorithm);
    String_set(archiveEntryInfo->file.chunkFileDelta.name,DeltaSource_getName(&archiveEntryInfo->file.deltaSourceHandle));
    archiveEntryInfo->file.chunkFileDelta.size           = DeltaSource_getSize(&archiveEntryInfo->file.deltaSourceHandle);

    archiveEntryInfo->file.headerLength += Chunk_getSize(&archiveEntryInfo->file.chunkFileDelta.info,
                                                         &archiveEntryInfo->file.chunkFileDelta,
                                                         0
                                                        );
  }

  // find next suitable archive part
  findNextArchivePart(archiveEntryInfo->archiveInfo);

  if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
  {
    // write header
    error = writeFileChunks(archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
  }

  // done resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_newImageEntry(ArchiveEntryInfo *archiveEntryInfo,
                               ArchiveInfo      *archiveInfo,
                               ConstString      deviceName,
                               const DeviceInfo *deviceInfo,
                               FileSystemTypes  fileSystemType,
                               const bool       deltaCompressFlag,
                               const bool       byteCompressFlag
                              )
#else /* not NDEBUG */
  Errors __Archive_newImageEntry(const char       *__fileName__,
                                 ulong            __lineNb__,
                                 ArchiveEntryInfo *archiveEntryInfo,
                                 ArchiveInfo      *archiveInfo,
                                 ConstString      deviceName,
                                 const DeviceInfo *deviceInfo,
                                 FileSystemTypes  fileSystemType,
                                 const bool       deltaCompressFlag,
                                 const bool       byteCompressFlag
                                )
#endif /* NDEBUG */
{
  AutoFreeList  autoFreeList;
  Errors        error;

  assert(archiveEntryInfo != NULL);
  assert(archiveInfo != NULL);
  assert(archiveInfo->jobOptions != NULL);
  assert(archiveInfo->blockLength > 0);
  assert(deviceInfo != NULL);
  assert(deviceInfo->blockSize > 0);

  // init variables
  AutoFree_init(&autoFreeList);

  // init crypt password
  error = initCryptPassword(archiveInfo);
  if (error !=  ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // init archive entry info
  archiveEntryInfo->archiveInfo                  = archiveInfo;
  archiveEntryInfo->mode                         = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm               = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength                  = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType             = ARCHIVE_ENTRY_TYPE_IMAGE;

  archiveEntryInfo->image.blockSize              = deviceInfo->blockSize;

  archiveEntryInfo->image.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->image.byteCompressAlgorithm  = byteCompressFlag ?archiveInfo->jobOptions->compressAlgorithms.byte :COMPRESS_ALGORITHM_NONE;

  archiveEntryInfo->image.deltaSourceHandleInitFlag   = FALSE;

  archiveEntryInfo->image.headerLength           = 0;
  archiveEntryInfo->image.headerWrittenFlag      = FALSE;

  archiveEntryInfo->image.byteBuffer             = NULL;
  archiveEntryInfo->image.byteBufferSize         = 0L;
  archiveEntryInfo->image.deltaBuffer            = NULL;
  archiveEntryInfo->image.deltaBufferSize        = 0L;

  // get intermediate output file
  error = File_getTmpFile(&archiveEntryInfo->image.intermediateFileHandle,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { (void)File_close(&archiveEntryInfo->image.intermediateFileHandle); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.intermediateFileHandle,{ (void)File_close(&archiveEntryInfo->image.intermediateFileHandle); });

  // allocate buffers
  archiveEntryInfo->image.byteBufferSize = FLOOR(MAX_BUFFER_SIZE,archiveEntryInfo->blockLength);
  archiveEntryInfo->image.byteBuffer = (byte*)malloc(archiveEntryInfo->image.byteBufferSize);
  if (archiveEntryInfo->image.byteBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,archiveEntryInfo->image.byteBuffer,{ free(archiveEntryInfo->image.byteBuffer); });
  archiveEntryInfo->image.deltaBufferSize = MAX_BUFFER_SIZE;
  archiveEntryInfo->image.deltaBuffer = (byte*)malloc(archiveEntryInfo->image.deltaBufferSize);
  if (archiveEntryInfo->image.deltaBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,archiveEntryInfo->image.deltaBuffer,{ free(archiveEntryInfo->image.deltaBuffer); });

  // get source for delta-compression
  if (deltaCompressFlag)
  {
    error = DeltaSource_openEntry(&archiveEntryInfo->image.deltaSourceHandle,
                                  archiveEntryInfo->archiveInfo->deltaSourceList,
                                  NULL, // storageName
                                  deviceName,
                                  SOURCE_SIZE_UNKNOWN,
                                  archiveInfo->jobOptions
                                 );
    if (error == ERROR_NONE)
    {
      archiveEntryInfo->image.deltaSourceHandleInitFlag   = TRUE;
      archiveEntryInfo->image.deltaCompressAlgorithm = archiveInfo->jobOptions->compressAlgorithms.delta;
      AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.deltaSourceHandle,{ DeltaSource_closeEntry(&archiveEntryInfo->image.deltaSourceHandle); });
    }
    else
    {
      if (archiveInfo->jobOptions->forceDeltaCompressionFlag)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      else
      {
        logMessage(archiveEntryInfo->archiveInfo->logHandle,
                   LOG_TYPE_WARNING,
                   "Image of device '%s' not delta compressed (no source file found)\n",
                   String_cString(deviceName)
                  );
      }
    }
  }

  // init image chunk
  error = Chunk_init(&archiveEntryInfo->image.chunkImage.info,
                     NULL,  // parentChunkInfo
                     &CHUNK_IO_FILE,
                     &archiveEntryInfo->image.intermediateFileHandle,
                     CHUNK_ID_IMAGE,
                     CHUNK_DEFINITION_IMAGE,
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->image.chunkImage
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Chunk_done(&archiveEntryInfo->file.chunkFile.info); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  archiveEntryInfo->image.chunkImage.compressAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->image.byteCompressAlgorithm);
  archiveEntryInfo->image.chunkImage.cryptAlgorithm    = CRYPT_ALGORITHM_TO_CONSTANT(archiveInfo->jobOptions->cryptAlgorithm);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImage.info,{ Chunk_done(&archiveEntryInfo->image.chunkImage.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->image.chunkImageEntry.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImageEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->image.chunkImageDelta.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImageDelta.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->image.chunkImageData.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImageData.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->image.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.cryptInfo); });

  // init sub-chunks
  error = Chunk_init(&archiveEntryInfo->image.chunkImageEntry.info,
                     &archiveEntryInfo->image.chunkImage.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_IMAGE_ENTRY,
                     CHUNK_DEFINITION_IMAGE_ENTRY,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->image.chunkImageEntry.cryptInfo,
                     &archiveEntryInfo->image.chunkImageEntry
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  archiveEntryInfo->image.chunkImageEntry.fileSystemType = fileSystemType;
  archiveEntryInfo->image.chunkImageEntry.size           = deviceInfo->size;
  archiveEntryInfo->image.chunkImageEntry.blockSize      = deviceInfo->blockSize;
  String_set(archiveEntryInfo->image.chunkImageEntry.name,deviceName);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImageEntry.info,{ Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info); });

  error = Chunk_init(&archiveEntryInfo->image.chunkImageDelta.info,
                     &archiveEntryInfo->image.chunkImage.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_IMAGE_DELTA,
                     CHUNK_DEFINITION_IMAGE_DELTA,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->image.chunkImageDelta.cryptInfo,
                     &archiveEntryInfo->image.chunkImageDelta
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  if (Compress_isCompressed(archiveEntryInfo->image.deltaCompressAlgorithm))
  {
    assert(Compress_isXDeltaCompressed(archiveEntryInfo->image.deltaCompressAlgorithm));

    archiveEntryInfo->image.chunkImageDelta.deltaAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->image.deltaCompressAlgorithm);
    String_set(archiveEntryInfo->image.chunkImageDelta.name,DeltaSource_getName(&archiveEntryInfo->image.deltaSourceHandle));
    archiveEntryInfo->image.chunkImageDelta.size = DeltaSource_getSize(&archiveEntryInfo->image.deltaSourceHandle);
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImageDelta.info,{ Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info); });

  error = Chunk_init(&archiveEntryInfo->image.chunkImageData.info,
                     &archiveEntryInfo->image.chunkImage.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_IMAGE_DATA,
                     CHUNK_DEFINITION_IMAGE_DATA,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->image.chunkImageData.cryptInfo,
                     &archiveEntryInfo->image.chunkImageData
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  archiveEntryInfo->image.chunkImageData.blockOffset = 0LL;
  archiveEntryInfo->image.chunkImageData.blockCount  = 0LL;
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImageData.info,{ Chunk_done(&archiveEntryInfo->image.chunkImageData.info); });

  // init delta compress (if no delta-compression is enabled, use identity-compressor), byte compress
  error = Compress_init(&archiveEntryInfo->image.deltaCompressInfo,
                        COMPRESS_MODE_DEFLATE,
                        archiveEntryInfo->image.deltaCompressAlgorithm,
                        1,
                        &archiveEntryInfo->image.deltaSourceHandle
                       );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.deltaCompressInfo,{ Compress_done(&archiveEntryInfo->image.deltaCompressInfo); });

  // init byte compress
  error = Compress_init(&archiveEntryInfo->image.byteCompressInfo,
                        COMPRESS_MODE_DEFLATE,
                        archiveEntryInfo->image.byteCompressAlgorithm,
                        archiveEntryInfo->blockLength,
                        NULL
                       );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.deltaCompressInfo,{ Compress_done(&archiveEntryInfo->image.deltaCompressInfo); });

  // calculate header size
  archiveEntryInfo->image.headerLength = Chunk_getSize(&archiveEntryInfo->image.chunkImage.info,     &archiveEntryInfo->image.chunkImage,     0)+
                                         Chunk_getSize(&archiveEntryInfo->image.chunkImageEntry.info,&archiveEntryInfo->image.chunkImageEntry,0)+
                                         Chunk_getSize(&archiveEntryInfo->image.chunkImageData.info, &archiveEntryInfo->image.chunkImageData, 0);
  if (Compress_isCompressed(archiveEntryInfo->image.deltaCompressAlgorithm))
  {
    archiveEntryInfo->image.chunkImageDelta.deltaAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->image.deltaCompressAlgorithm);
    String_set(archiveEntryInfo->image.chunkImageDelta.name,DeltaSource_getName(&archiveEntryInfo->image.deltaSourceHandle));
    archiveEntryInfo->image.chunkImageDelta.size           = DeltaSource_getSize(&archiveEntryInfo->image.deltaSourceHandle);

    archiveEntryInfo->image.headerLength += Chunk_getSize(&archiveEntryInfo->image.chunkImageDelta.info,
                                                          &archiveEntryInfo->image.chunkImageDelta,
                                                          0
                                                         );
  }

  // find next suitable archive part
  findNextArchivePart(archiveEntryInfo->archiveInfo);

  if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
  {
    // write header
    error = writeImageChunks(archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
  }

  // find next suitable archive part
  findNextArchivePart(archiveEntryInfo->archiveInfo);

  // done resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_newDirectoryEntry(ArchiveEntryInfo                *archiveEntryInfo,
                                   ArchiveInfo                     *archiveInfo,
                                   ConstString                     directoryName,
                                   const FileInfo                  *fileInfo,
                                   const FileExtendedAttributeList *fileExtendedAttributeList
                                  )
#else /* not NDEBUG */
  Errors __Archive_newDirectoryEntry(const char                      *__fileName__,
                                     ulong                           __lineNb__,
                                     ArchiveEntryInfo                *archiveEntryInfo,
                                     ArchiveInfo                     *archiveInfo,
                                     ConstString                     directoryName,
                                     const FileInfo                  *fileInfo,
                                     const FileExtendedAttributeList *fileExtendedAttributeList
                                    )
#endif /* NDEBUG */
{
  AutoFreeList                    autoFreeList;
  Errors                          error;
  ulong                           headerLength;
  const FileExtendedAttributeNode *fileExtendedAttributeNode;

  assert(archiveEntryInfo != NULL);
  assert(archiveInfo != NULL);
  assert(archiveInfo->jobOptions != NULL);
  assert(archiveInfo->blockLength > 0);
  assert(fileInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  // init crypt password
  error = initCryptPassword(archiveInfo);
  if (error !=  ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // init archive entry info
  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm   = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength      = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_DIRECTORY;

  // init directory entry chunk
  error = Chunk_init(&archiveEntryInfo->directory.chunkDirectory.info,
                     NULL,  // parentChunkInfo
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_DIRECTORY,
                     CHUNK_DEFINITION_DIRECTORY,
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->directory.chunkDirectory
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  archiveEntryInfo->directory.chunkDirectory.cryptAlgorithm = CRYPT_ALGORITHM_TO_CONSTANT(archiveInfo->jobOptions->cryptAlgorithm);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->directory.chunkDirectory.info,{ Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.cryptInfo); });

  // init sub-chunks
  error = Chunk_init(&archiveEntryInfo->directory.chunkDirectoryEntry.info,
                     &archiveEntryInfo->directory.chunkDirectory.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_DIRECTORY_ENTRY,
                     CHUNK_DEFINITION_DIRECTORY_ENTRY,
                     MAX(archiveEntryInfo->blockLength,DEFAULT_ALIGNMENT),
                     &archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,
                     &archiveEntryInfo->directory.chunkDirectoryEntry
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  archiveEntryInfo->directory.chunkDirectoryEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveEntryInfo->directory.chunkDirectoryEntry.timeModified    = fileInfo->timeModified;
  archiveEntryInfo->directory.chunkDirectoryEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveEntryInfo->directory.chunkDirectoryEntry.userId          = fileInfo->userId;
  archiveEntryInfo->directory.chunkDirectoryEntry.groupId         = fileInfo->groupId;
  archiveEntryInfo->directory.chunkDirectoryEntry.permission      = fileInfo->permission;
  String_set(archiveEntryInfo->directory.chunkDirectoryEntry.name,directoryName);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->directory.chunkDirectoryEntry.info,{ Chunk_done(&archiveEntryInfo->directory.chunkDirectoryEntry.info); });

  error = Chunk_init(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.info,
                     &archiveEntryInfo->directory.chunkDirectory.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_DIRECTORY_EXTENDED_ATTRIBUTE,
                     CHUNK_DEFINITION_DIRECTORY_EXTENDED_ATTRIBUTE,
                     MAX(archiveEntryInfo->blockLength,DEFAULT_ALIGNMENT),
                     &archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.cryptInfo,
                     &archiveEntryInfo->directory.chunkDirectoryExtendedAttribute
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.info,{ Chunk_done(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.info); });

  // calculate header size
  headerLength = Chunk_getSize(&archiveEntryInfo->directory.chunkDirectory.info,     &archiveEntryInfo->directory.chunkDirectory,     0)+
                 Chunk_getSize(&archiveEntryInfo->directory.chunkDirectoryEntry.info,&archiveEntryInfo->directory.chunkDirectoryEntry,0);
  LIST_ITERATE(fileExtendedAttributeList,fileExtendedAttributeNode)
  {
    String_set(archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.name,fileExtendedAttributeNode->name);

    headerLength += Chunk_getSize(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.info,
                                  &archiveEntryInfo->directory.chunkDirectoryExtendedAttribute,
                                  fileExtendedAttributeNode->dataLength
                                 );
  }

  // find next suitable archive part
  findNextArchivePart(archiveEntryInfo->archiveInfo);

  if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
  {
    // lock archive
    Semaphore_forceLock(&archiveEntryInfo->archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

    // ensure space in archive
    error = ensureArchiveSpace(archiveEntryInfo->archiveInfo,
                               headerLength
                              );
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write directory chunk
    error = Chunk_create(&archiveEntryInfo->directory.chunkDirectory.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write directory entry chunk
    error = Chunk_create(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write extended attribute chunks
    LIST_ITERATE(fileExtendedAttributeList,fileExtendedAttributeNode)
    {
      String_set(archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.name,fileExtendedAttributeNode->name);
      archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.value.data   = fileExtendedAttributeNode->data;
      archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.value.length = fileExtendedAttributeNode->dataLength;

      error = Chunk_create(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.info);
      if (error != ERROR_NONE)
      {
        break;
      }
      error = Chunk_close(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.info);
      if (error != ERROR_NONE)
      {
        break;
      }
    }
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
  }

  // done resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_newLinkEntry(ArchiveEntryInfo                *archiveEntryInfo,
                              ArchiveInfo                     *archiveInfo,
                              ConstString                     linkName,
                              ConstString                     destinationName,
                              const FileInfo                  *fileInfo,
                              const FileExtendedAttributeList *fileExtendedAttributeList
                             )
#else /* not NDEBUG */
  Errors __Archive_newLinkEntry(const char                      *__fileName__,
                                ulong                           __lineNb__,
                                ArchiveEntryInfo                *archiveEntryInfo,
                                ArchiveInfo                     *archiveInfo,
                                ConstString                     linkName,
                                ConstString                     destinationName,
                                const FileInfo                  *fileInfo,
                                const FileExtendedAttributeList *fileExtendedAttributeList
                               )
#endif /* NDEBUG */
{
  AutoFreeList                    autoFreeList;
  Errors                          error;
  ulong                           headerLength;
  const FileExtendedAttributeNode *fileExtendedAttributeNode;

  assert(archiveEntryInfo != NULL);
  assert(archiveInfo != NULL);
  assert(archiveInfo->jobOptions != NULL);
  assert(archiveInfo->blockLength > 0);
  assert(fileInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  // init crypt password
  error = initCryptPassword(archiveInfo);
  if (error !=  ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // init archive entry info
  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm   = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength      = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_LINK;

  // init link chunk
  error = Chunk_init(&archiveEntryInfo->link.chunkLink.info,
                     NULL,  // parentChunkInfo
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_LINK,
                     CHUNK_DEFINITION_LINK,
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->link.chunkLink
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  archiveEntryInfo->link.chunkLink.cryptAlgorithm = CRYPT_ALGORITHM_TO_CONSTANT(archiveInfo->jobOptions->cryptAlgorithm);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->link.chunkLink.info,{ Chunk_done(&archiveEntryInfo->link.chunkLink.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->link.chunkLinkEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->link.chunkLinkExtendedAttribute.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->link.chunkLinkExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->link.chunkLinkExtendedAttribute.cryptInfo); });

  // init sub-chunks
  error = Chunk_init(&archiveEntryInfo->link.chunkLinkEntry.info,
                     &archiveEntryInfo->link.chunkLink.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_LINK_ENTRY,
                     CHUNK_DEFINITION_LINK_ENTRY,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->link.chunkLinkEntry.cryptInfo,
                     &archiveEntryInfo->link.chunkLinkEntry
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
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
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->link.chunkLinkEntry.info,{ Chunk_done(&archiveEntryInfo->link.chunkLinkEntry.info); });

  error = Chunk_init(&archiveEntryInfo->link.chunkLinkExtendedAttribute.info,
                     &archiveEntryInfo->link.chunkLink.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_LINK_EXTENDED_ATTRIBUTE,
                     CHUNK_DEFINITION_LINK_EXTENDED_ATTRIBUTE,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->link.chunkLinkExtendedAttribute.cryptInfo,
                     &archiveEntryInfo->link.chunkLinkExtendedAttribute
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->link.chunkLinkExtendedAttribute.info,{ Chunk_done(&archiveEntryInfo->link.chunkLinkExtendedAttribute.info); });

  // calculate header length
  headerLength = Chunk_getSize(&archiveEntryInfo->link.chunkLink.info,     &archiveEntryInfo->link.chunkLink,     0)+
                 Chunk_getSize(&archiveEntryInfo->link.chunkLinkEntry.info,&archiveEntryInfo->link.chunkLinkEntry,0);
  LIST_ITERATE(fileExtendedAttributeList,fileExtendedAttributeNode)
  {
    String_set(archiveEntryInfo->link.chunkLinkExtendedAttribute.name,fileExtendedAttributeNode->name);

    headerLength += Chunk_getSize(&archiveEntryInfo->link.chunkLinkExtendedAttribute.info,
                                  &archiveEntryInfo->link.chunkLinkExtendedAttribute,
                                  fileExtendedAttributeNode->dataLength
                                 );
  }


  if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
  {
    // lock archive
    Semaphore_forceLock(&archiveEntryInfo->archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

    // ensure space in archive
    error = ensureArchiveSpace(archiveEntryInfo->archiveInfo,
                               headerLength
                              );
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write link chunk
    error = Chunk_create(&archiveEntryInfo->link.chunkLink.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write link entry chunk
    error = Chunk_create(&archiveEntryInfo->link.chunkLinkEntry.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write extended attribute chunks
    LIST_ITERATE(fileExtendedAttributeList,fileExtendedAttributeNode)
    {
      String_set(archiveEntryInfo->link.chunkLinkExtendedAttribute.name,fileExtendedAttributeNode->name);
      archiveEntryInfo->link.chunkLinkExtendedAttribute.value.data   = fileExtendedAttributeNode->data;
      archiveEntryInfo->link.chunkLinkExtendedAttribute.value.length = fileExtendedAttributeNode->dataLength;

      error = Chunk_create(&archiveEntryInfo->link.chunkLinkExtendedAttribute.info);
      if (error != ERROR_NONE)
      {
        break;
      }
      error = Chunk_close(&archiveEntryInfo->link.chunkLinkExtendedAttribute.info);
      if (error != ERROR_NONE)
      {
        break;
      }
    }
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
  }

  // done resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_newHardLinkEntry(ArchiveEntryInfo                *archiveEntryInfo,
                                  ArchiveInfo                     *archiveInfo,
                                  const StringList                *fileNameList,
                                  const FileInfo                  *fileInfo,
                                  const FileExtendedAttributeList *fileExtendedAttributeList,
                                  const bool                      deltaCompressFlag,
                                  const bool                      byteCompressFlag
                                 )
#else /* not NDEBUG */
  Errors __Archive_newHardLinkEntry(const char                      *__fileName__,
                                    ulong                           __lineNb__,
                                    ArchiveEntryInfo                *archiveEntryInfo,
                                    ArchiveInfo                     *archiveInfo,
                                    const StringList                *fileNameList,
                                    const FileInfo                  *fileInfo,
                                    const FileExtendedAttributeList *fileExtendedAttributeList,
                                    const bool                      deltaCompressFlag,
                                    const bool                      byteCompressFlag
                                   )
#endif /* NDEBUG */
{
  AutoFreeList                    autoFreeList;
  Errors                          error;
  const FileExtendedAttributeNode *fileExtendedAttributeNode;
  const StringNode                *stringNode;
  String                          fileName;

  assert(archiveEntryInfo != NULL);
  assert(archiveInfo != NULL);
  assert(archiveInfo->jobOptions != NULL);
  assert(archiveInfo->blockLength > 0);
  assert(fileInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  // init crypt password
  error = initCryptPassword(archiveInfo);
  if (error !=  ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // init archive entry info
  archiveEntryInfo->archiveInfo                        = archiveInfo;
  archiveEntryInfo->mode                               = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm                     = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength                        = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType                   = ARCHIVE_ENTRY_TYPE_HARDLINK;

  archiveEntryInfo->hardLink.deltaCompressAlgorithm    = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->hardLink.byteCompressAlgorithm     = byteCompressFlag ?archiveInfo->jobOptions->compressAlgorithms.byte :COMPRESS_ALGORITHM_NONE;

  archiveEntryInfo->hardLink.fileNameList              = fileNameList;
  archiveEntryInfo->hardLink.fileExtendedAttributeList = fileExtendedAttributeList;

  archiveEntryInfo->hardLink.deltaSourceHandleInitFlag      = FALSE;

  archiveEntryInfo->hardLink.headerLength              = 0;
  archiveEntryInfo->hardLink.headerWrittenFlag         = FALSE;

  archiveEntryInfo->hardLink.byteBuffer                = NULL;
  archiveEntryInfo->hardLink.byteBufferSize            = 0L;
  archiveEntryInfo->hardLink.deltaBuffer               = NULL;
  archiveEntryInfo->hardLink.deltaBufferSize           = 0L;

  // get intermediate output file
  error = File_getTmpFile(&archiveEntryInfo->hardLink.intermediateFileHandle,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { (void)File_close(&archiveEntryInfo->hardLink.intermediateFileHandle); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.intermediateFileHandle,{ (void)File_close(&archiveEntryInfo->hardLink.intermediateFileHandle); });

  // allocate buffers
  archiveEntryInfo->hardLink.byteBufferSize = FLOOR(MAX_BUFFER_SIZE,archiveEntryInfo->blockLength);
  archiveEntryInfo->hardLink.byteBuffer = (byte*)malloc(archiveEntryInfo->hardLink.byteBufferSize);
  if (archiveEntryInfo->hardLink.byteBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.byteBuffer,{ free(archiveEntryInfo->hardLink.byteBuffer); });
  archiveEntryInfo->hardLink.deltaBufferSize = MAX_BUFFER_SIZE;
  archiveEntryInfo->hardLink.deltaBuffer = (byte*)malloc(archiveEntryInfo->hardLink.deltaBufferSize);
  if (archiveEntryInfo->hardLink.deltaBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.deltaBuffer,{ free(archiveEntryInfo->hardLink.deltaBuffer); });

  // get source for delta-compression
  if (deltaCompressFlag)
  {
    error = ERROR_NONE;
    STRINGLIST_ITERATE(fileNameList,stringNode,fileName)
    {
      error = DeltaSource_openEntry(&archiveEntryInfo->hardLink.deltaSourceHandle,
                                    archiveEntryInfo->archiveInfo->deltaSourceList,
                                    NULL, // storageName
                                    fileName,
                                    SOURCE_SIZE_UNKNOWN,
                                    archiveInfo->jobOptions
                                   );
      if (error == ERROR_NONE) break;
    }
    if      (error == ERROR_NONE)
    {
      archiveEntryInfo->hardLink.deltaSourceHandleInitFlag   = TRUE;
      archiveEntryInfo->hardLink.deltaCompressAlgorithm = archiveInfo->jobOptions->compressAlgorithms.delta;
      AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.deltaSourceHandle,{ DeltaSource_closeEntry(&archiveEntryInfo->hardLink.deltaSourceHandle); });
    }
    else if (archiveInfo->jobOptions->forceDeltaCompressionFlag)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    else
    {
      logMessage(archiveEntryInfo->archiveInfo->logHandle,
                 LOG_TYPE_WARNING,
                 "File '%s' not delta compressed (no source file found)\n",
                 String_cString(StringList_first(fileNameList,NULL))
                );
    }
  }

  // init hard link chunk
  error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLink.info,
                     NULL,  // parentChunkInfo
                     &CHUNK_IO_FILE,
                     &archiveEntryInfo->hardLink.intermediateFileHandle,
                     CHUNK_ID_HARDLINK,
                     CHUNK_DEFINITION_HARDLINK,
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->hardLink.chunkHardLink
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  archiveEntryInfo->hardLink.chunkHardLink.compressAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->hardLink.byteCompressAlgorithm);
  archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithm    = CRYPT_ALGORITHM_TO_CONSTANT(archiveInfo->jobOptions->cryptAlgorithm);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLink.info,{ Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->hardLink.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.cryptInfo); });

  // init sub-chunks
  error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info,
                     &archiveEntryInfo->hardLink.chunkHardLink.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_HARDLINK_ENTRY,
                     CHUNK_DEFINITION_HARDLINK_ENTRY,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,
                     &archiveEntryInfo->hardLink.chunkHardLinkEntry
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  archiveEntryInfo->hardLink.chunkHardLinkEntry.size            = fileInfo->size;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.timeModified    = fileInfo->timeModified;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.userId          = fileInfo->userId;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.groupId         = fileInfo->groupId;
  archiveEntryInfo->hardLink.chunkHardLinkEntry.permission      = fileInfo->permission;
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkEntry.info,{ Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info); });

  error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkName.info,
                     &archiveEntryInfo->hardLink.chunkHardLink.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_HARDLINK_NAME,
                     CHUNK_DEFINITION_HARDLINK_NAME,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo,
                     &archiveEntryInfo->hardLink.chunkHardLinkName
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkName.info,{ Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkName.info); });

  error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.info,
                     &archiveEntryInfo->hardLink.chunkHardLink.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_HARDLINK_EXTENDED_ATTRIBUTE,
                     CHUNK_DEFINITION_HARDLINK_EXTENDED_ATTRIBUTE,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo,
                     &archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.info,{ Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.info); });

  error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info,
                     &archiveEntryInfo->hardLink.chunkHardLink.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_HARDLINK_DELTA,
                     CHUNK_DEFINITION_HARDLINK_DELTA,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,
                     &archiveEntryInfo->hardLink.chunkHardLinkDelta
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  if (Compress_isCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm))
  {
    assert(Compress_isXDeltaCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm));

    archiveEntryInfo->hardLink.chunkHardLinkDelta.deltaAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->hardLink.deltaCompressAlgorithm);
    String_set(archiveEntryInfo->hardLink.chunkHardLinkDelta.name,DeltaSource_getName(&archiveEntryInfo->hardLink.deltaSourceHandle));
    archiveEntryInfo->hardLink.chunkHardLinkDelta.size = DeltaSource_getSize(&archiveEntryInfo->hardLink.deltaSourceHandle);
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkDelta.info,{ Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info); });

  error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                     &archiveEntryInfo->hardLink.chunkHardLink.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_HARDLINK_DATA,
                     CHUNK_DEFINITION_HARDLINK_DATA,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,
                     &archiveEntryInfo->hardLink.chunkHardLinkData
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset = 0LL;
  archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize   = 0LL;
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkData.info,{ Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info); });

  // init delta compress (if no delta-compression is enabled, use identity-compressor), byte compress
  error = Compress_init(&archiveEntryInfo->hardLink.deltaCompressInfo,
                        COMPRESS_MODE_DEFLATE,
                        archiveEntryInfo->hardLink.deltaCompressAlgorithm,
                        1,
                        &archiveEntryInfo->hardLink.deltaSourceHandle
                       );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.deltaCompressInfo,{ Compress_done(&archiveEntryInfo->hardLink.deltaCompressInfo); });

  // init byte compress
  error = Compress_init(&archiveEntryInfo->hardLink.byteCompressInfo,
                        COMPRESS_MODE_DEFLATE,
                        archiveEntryInfo->hardLink.byteCompressAlgorithm,
                        archiveEntryInfo->blockLength,
                        NULL
                       );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.byteCompressInfo,{ Compress_done(&archiveEntryInfo->hardLink.byteCompressInfo); });

  // calculate header size
  archiveEntryInfo->hardLink.headerLength = Chunk_getSize(&archiveEntryInfo->hardLink.chunkHardLink.info,     &archiveEntryInfo->hardLink.chunkHardLink,     0)+
                                            Chunk_getSize(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info,&archiveEntryInfo->hardLink.chunkHardLinkEntry,0)+
                                            Chunk_getSize(&archiveEntryInfo->hardLink.chunkHardLinkData.info, &archiveEntryInfo->hardLink.chunkHardLinkData, 0);
  STRINGLIST_ITERATE(archiveEntryInfo->hardLink.fileNameList,stringNode,fileName)
  {
    String_set(archiveEntryInfo->hardLink.chunkHardLinkName.name,fileName);

    archiveEntryInfo->hardLink.headerLength += Chunk_getSize(&archiveEntryInfo->hardLink.chunkHardLinkName.info,
                                                             &archiveEntryInfo->hardLink.chunkHardLinkName,
                                                             0
                                                            );
  }
  LIST_ITERATE(archiveEntryInfo->hardLink.fileExtendedAttributeList,fileExtendedAttributeNode)
  {
    String_set(archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.name,fileExtendedAttributeNode->name);

    archiveEntryInfo->hardLink.headerLength += Chunk_getSize(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.info,
                                                             &archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute,
                                                             fileExtendedAttributeNode->dataLength
                                                            );
  }
  if (Compress_isCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm))
  {
    archiveEntryInfo->hardLink.chunkHardLinkDelta.deltaAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->hardLink.deltaCompressAlgorithm);
    String_set(archiveEntryInfo->hardLink.chunkHardLinkDelta.name,DeltaSource_getName(&archiveEntryInfo->hardLink.deltaSourceHandle));
    archiveEntryInfo->hardLink.chunkHardLinkDelta.size           = DeltaSource_getSize(&archiveEntryInfo->hardLink.deltaSourceHandle);

    archiveEntryInfo->hardLink.headerLength += Chunk_getSize(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info,
                                                             &archiveEntryInfo->hardLink.chunkHardLinkDelta,
                                                             0
                                                            );
  }

  // find next suitable archive part
  findNextArchivePart(archiveEntryInfo->archiveInfo);

  if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
  {
    // create new part
    error = writeHardLinkChunks(archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
  }

  // done resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_newSpecialEntry(ArchiveEntryInfo                *archiveEntryInfo,
                                 ArchiveInfo                     *archiveInfo,
                                 ConstString                     specialName,
                                 const FileInfo                  *fileInfo,
                                 const FileExtendedAttributeList *fileExtendedAttributeList
                                )
#else /* not NDEBUG */
  Errors __Archive_newSpecialEntry(const char                      *__fileName__,
                                   ulong                           __lineNb__,
                                   ArchiveEntryInfo                *archiveEntryInfo,
                                   ArchiveInfo                     *archiveInfo,
                                   ConstString                     specialName,
                                   const FileInfo                  *fileInfo,
                                   const FileExtendedAttributeList *fileExtendedAttributeList
                                  )
#endif /* NDEBUG */
{
  AutoFreeList                    autoFreeList;
  Errors                          error;
  ulong                           headerLength;
  const FileExtendedAttributeNode *fileExtendedAttributeNode;

  assert(archiveEntryInfo != NULL);
  assert(archiveInfo != NULL);
  assert(archiveInfo->jobOptions != NULL);
  assert(archiveInfo->blockLength > 0);
  assert(fileInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  // init crypt password
  error = initCryptPassword(archiveInfo);
  if (error !=  ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // init archive entry info
  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm   = archiveInfo->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength      = archiveInfo->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_SPECIAL;

  // init special chunk
  error = Chunk_init(&archiveEntryInfo->special.chunkSpecial.info,
                     NULL,  // parentChunkInfo
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_SPECIAL,
                     CHUNK_DEFINITION_SPECIAL,
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->special.chunkSpecial
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  archiveEntryInfo->special.chunkSpecial.cryptAlgorithm = CRYPT_ALGORITHM_TO_CONSTANT(archiveInfo->jobOptions->cryptAlgorithm);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->special.chunkSpecial.info,{ Chunk_done(&archiveEntryInfo->special.chunkSpecial.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.cryptInfo,
                     archiveInfo->jobOptions->cryptAlgorithm,
                     archiveInfo->cryptPassword
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->special.chunkSpecialExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.cryptInfo); });

  // init sub-chunks
  error = Chunk_init(&archiveEntryInfo->special.chunkSpecialEntry.info,
                     &archiveEntryInfo->special.chunkSpecial.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_SPECIAL_ENTRY,
                     CHUNK_DEFINITION_SPECIAL_ENTRY,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->special.chunkSpecialEntry.cryptInfo,
                     &archiveEntryInfo->special.chunkSpecialEntry
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
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
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->special.chunkSpecialEntry.info,{ Chunk_done(&archiveEntryInfo->special.chunkSpecialEntry.info); });

  error = Chunk_init(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.info,
                     &archiveEntryInfo->special.chunkSpecial.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_SPECIAL_EXTENDED_ATTRIBUTE,
                     CHUNK_DEFINITION_SPECIAL_EXTENDED_ATTRIBUTE,
                     archiveEntryInfo->blockLength,
                     &archiveEntryInfo->special.chunkSpecialExtendedAttribute.cryptInfo,
                     &archiveEntryInfo->special.chunkSpecialExtendedAttribute
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->special.chunkSpecialExtendedAttribute.info,{ Chunk_done(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.info); });

  // calculate header length
  headerLength = Chunk_getSize(&archiveEntryInfo->special.chunkSpecial.info,     &archiveEntryInfo->special.chunkSpecial,     0)+
                 Chunk_getSize(&archiveEntryInfo->special.chunkSpecialEntry.info,&archiveEntryInfo->special.chunkSpecialEntry,0);
  LIST_ITERATE(fileExtendedAttributeList,fileExtendedAttributeNode)
  {
    String_set(archiveEntryInfo->special.chunkSpecialExtendedAttribute.name,fileExtendedAttributeNode->name);

    headerLength += Chunk_getSize(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.info,
                                  &archiveEntryInfo->special.chunkSpecialExtendedAttribute,
                                  fileExtendedAttributeNode->dataLength
                                 );
  }

  // find next suitable archive part
  findNextArchivePart(archiveEntryInfo->archiveInfo);

  if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
  {
    // lock archive
    Semaphore_forceLock(&archiveEntryInfo->archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

    // ensure space in archive
    error = ensureArchiveSpace(archiveEntryInfo->archiveInfo,
                               headerLength
                              );
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write special chunk
    error = Chunk_create(&archiveEntryInfo->special.chunkSpecial.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write special entry chunk
    error = Chunk_create(&archiveEntryInfo->special.chunkSpecialEntry.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write extended attribute chunks
    LIST_ITERATE(fileExtendedAttributeList,fileExtendedAttributeNode)
    {
      String_set(archiveEntryInfo->special.chunkSpecialExtendedAttribute.name,fileExtendedAttributeNode->name);
      archiveEntryInfo->special.chunkSpecialExtendedAttribute.value.data   = fileExtendedAttributeNode->data;
      archiveEntryInfo->special.chunkSpecialExtendedAttribute.value.length = fileExtendedAttributeNode->dataLength;

      error = Chunk_create(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.info);
      if (error != ERROR_NONE)
      {
        break;
      }
      error = Chunk_close(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.info);
      if (error != ERROR_NONE)
      {
        break;
      }
    }
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
  }

  // done resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

Errors Archive_getNextArchiveEntryType(ArchiveInfo       *archiveInfo,
                                       ArchiveEntryTypes *archiveEntryType,
                                       bool              skipUnknownChunksFlag
                                      )
{
  Errors         error;
  bool           scanMode;
  ChunkHeader    chunkHeader;
  bool           decryptedFlag;
  PasswordHandle passwordHandle;
  const Password *password;

  assert(archiveInfo != NULL);
  assert(archiveInfo->jobOptions != NULL);
  assert(archiveEntryType != NULL);

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  // find next file, image, directory, link, special chunk
  scanMode = FALSE;
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

        scanMode = FALSE;
        break;
      case CHUNK_ID_KEY:
        // check if private key available
        if (archiveInfo->jobOptions->cryptPrivateKeyFileName == NULL)
        {
          return ERROR_NO_PRIVATE_KEY;
        }

        // read private key, try to read key with no password, all passwords
        Crypt_initKey(&archiveInfo->cryptKey,CRYPT_PADDING_TYPE_NONE);
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
                                           archiveInfo,
                                           archiveInfo->jobOptions,
                                           archiveInfo->jobOptions->cryptPasswordMode,
                                           archiveInfo->getPasswordFunction,
                                           archiveInfo->getPasswordUserData
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

        scanMode = FALSE;
        break;
      case CHUNK_ID_FILE:
      case CHUNK_ID_IMAGE:
      case CHUNK_ID_DIRECTORY:
      case CHUNK_ID_LINK:
      case CHUNK_ID_HARDLINK:
      case CHUNK_ID_SPECIAL:
        scanMode = FALSE;
        break;
      default:
        if (skipUnknownChunksFlag)
        {
          // unknown chunk -> switch to scan mode
          if (!scanMode)
          {
            if (globalOptions.verboseLevel >= 3)
            {
              printWarning("Skipped unknown chunk '%s' (offset %llu) in '%s'. Switch to scan mode.\n",
                           Chunk_idToString(chunkHeader.id),
                           chunkHeader.offset,
                           String_cString(archiveInfo->printableStorageName)
                          );
            }

            scanMode = TRUE;
          }

          // skip 1 byte
          error = archiveInfo->chunkIO->seek(archiveInfo->chunkIOUserData,chunkHeader.offset+1LL);
          if (error != ERROR_NONE)
          {
            return error;
          }
        }
        else
        {
          // report unknown chunk
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

#ifdef NDEBUG
  Errors Archive_readFileEntry(ArchiveEntryInfo          *archiveEntryInfo,
                               ArchiveInfo               *archiveInfo,
                               CompressAlgorithms        *deltaCompressAlgorithm,
                               CompressAlgorithms        *byteCompressAlgorithm,
                               CryptAlgorithms           *cryptAlgorithm,
                               CryptTypes                *cryptType,
                               String                    fileName,
                               FileInfo                  *fileInfo,
                               FileExtendedAttributeList *fileExtendedAttributeList,
                               String                    deltaSourceName,
                               uint64                    *deltaSourceSize,
                               uint64                    *fragmentOffset,
                               uint64                    *fragmentSize
                              )
#else /* not NDEBUG */
  Errors __Archive_readFileEntry(const char                *__fileName__,
                                 ulong                     __lineNb__,
                                 ArchiveEntryInfo          *archiveEntryInfo,
                                 ArchiveInfo               *archiveInfo,
                                 CompressAlgorithms        *deltaCompressAlgorithm,
                                 CompressAlgorithms        *byteCompressAlgorithm,
                                 CryptAlgorithms           *cryptAlgorithm,
                                 CryptTypes                *cryptType,
                                 String                    fileName,
                                 FileInfo                  *fileInfo,
                                 FileExtendedAttributeList *fileExtendedAttributeList,
                                 String                    deltaSourceName,
                                 uint64                    *deltaSourceSize,
                                 uint64                    *fragmentOffset,
                                 uint64                    *fragmentSize
                                )
#endif /* NDEBUG */
{
  AutoFreeList   autoFreeList1,autoFreeList2;
  Errors         error;
  ChunkHeader    chunkHeader;
  uint64         index;
  PasswordHandle passwordHandle;
  const Password *password;
  bool           passwordFlag;
  bool           decryptedFlag;
  ChunkHeader    subChunkHeader;
  bool           foundFileEntryFlag,foundFileDataFlag;

  assert(archiveEntryInfo != NULL);
  assert(archiveInfo != NULL);
  assert(archiveInfo->jobOptions != NULL);
  assert(fileName != NULL);

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  if (deltaSourceSize != NULL) (*deltaSourceSize) = 0LL;

  archiveEntryInfo->archiveInfo               = archiveInfo;
  archiveEntryInfo->mode                      = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType          = ARCHIVE_ENTRY_TYPE_FILE;

  archiveEntryInfo->file.deltaSourceHandleInitFlag = FALSE;

  archiveEntryInfo->file.byteBuffer           = NULL;
  archiveEntryInfo->file.byteBufferSize       = 0L;
  archiveEntryInfo->file.deltaBuffer          = NULL;
  archiveEntryInfo->file.deltaBufferSize      = 0L;

  // init file chunk
  error = Chunk_init(&archiveEntryInfo->file.chunkFile.info,
                     NULL,  // parentChunkInfo
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_FILE,
                     CHUNK_DEFINITION_FILE,
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->file.chunkFile
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->file.chunkFile.info,{ Chunk_done(&archiveEntryInfo->file.chunkFile.info); });

  // find next file chunk
  do
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList1);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_FILE)
    {
      error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList1);
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_FILE);

  // read file chunk
  error = Chunk_open(&archiveEntryInfo->file.chunkFile.info,
                     &chunkHeader,
                     Chunk_getSize(&archiveEntryInfo->file.chunkFile.info,NULL,0)
                    );
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    (void)Chunk_close(&archiveEntryInfo->file.chunkFile.info);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->file.chunkFile.info,{ Chunk_close(&archiveEntryInfo->file.chunkFile.info); });
  if (!Compress_isValidAlgorithm(archiveEntryInfo->file.chunkFile.compressAlgorithm))
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_COMPRESS_ALGORITHM;
  }
  archiveEntryInfo->file.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->file.byteCompressAlgorithm  = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->file.chunkFile.compressAlgorithm);
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->file.chunkFile.cryptAlgorithm))
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
  archiveEntryInfo->cryptAlgorithm = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->file.chunkFile.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  // allocate buffers
  archiveEntryInfo->file.byteBufferSize = FLOOR(MAX_BUFFER_SIZE,archiveEntryInfo->blockLength);
  archiveEntryInfo->file.byteBuffer = (byte*)malloc(archiveEntryInfo->file.byteBufferSize);
  if (archiveEntryInfo->file.byteBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList1,archiveEntryInfo->file.byteBuffer,{ free(archiveEntryInfo->file.byteBuffer); });
  archiveEntryInfo->file.deltaBufferSize = MAX_BUFFER_SIZE;
  archiveEntryInfo->file.deltaBuffer = (byte*)malloc(archiveEntryInfo->file.deltaBufferSize);
  if (archiveEntryInfo->file.deltaBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList1,archiveEntryInfo->file.deltaBuffer,{ free(archiveEntryInfo->file.deltaBuffer); });

  // try to read file entry with all passwords
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->file.chunkFile.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm))
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo,
                                         archiveInfo->jobOptions,
                                         archiveInfo->jobOptions->cryptPasswordMode,
                                         archiveInfo->getPasswordFunction,
                                         archiveInfo->getPasswordUserData
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
  do
  {
    // reset
    error = ERROR_NONE;
    AutoFree_freeAll(&autoFreeList2);
    // reset chunk read position
    Chunk_seek(&archiveEntryInfo->file.chunkFile.info,index);

    // init file entry/extended attribute/delta/data/file crypt
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->file.chunkFileEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->file.chunkFileEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->file.chunkFileDelta.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->file.chunkFileDelta.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->file.chunkFileData.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->file.chunkFileData.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->file.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->file.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.cryptInfo); });
      }
    }

    // init file entry/extended attributes/delta/data chunks
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->file.chunkFileEntry.info,
                         &archiveEntryInfo->file.chunkFile.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_FILE_ENTRY,
                         CHUNK_DEFINITION_FILE_ENTRY,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->file.chunkFileEntry.cryptInfo,
                         &archiveEntryInfo->file.chunkFileEntry
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->file.chunkFileEntry.info,{ Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->file.chunkFileExtendedAttribute.info,
                         &archiveEntryInfo->file.chunkFile.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_FILE_EXTENDED_ATTRIBUTE,
                         CHUNK_DEFINITION_FILE_EXTENDED_ATTRIBUTE,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo,
                         &archiveEntryInfo->file.chunkFileExtendedAttribute
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->file.chunkFileExtendedAttribute.info,{ Chunk_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.info); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->file.chunkFileDelta.info,
                         &archiveEntryInfo->file.chunkFile.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_FILE_DELTA,
                         CHUNK_DEFINITION_FILE_DELTA,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->file.chunkFileDelta.cryptInfo,
                         &archiveEntryInfo->file.chunkFileDelta
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->file.chunkFileDelta.info,{ Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->file.chunkFileData.info,
                         &archiveEntryInfo->file.chunkFile.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_FILE_DATA,
                         CHUNK_DEFINITION_FILE_DATA,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->file.chunkFileData.cryptInfo,
                         &archiveEntryInfo->file.chunkFileData
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->file.chunkFileData.info,{ Chunk_done(&archiveEntryInfo->file.chunkFileData.info); });
      }
    }

    // try to read file entry/data chunks
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveEntryInfo->file.chunkFile.info)
             && !foundFileDataFlag
             && (error == ERROR_NONE)
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
            AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->file.chunkFileEntry.info,{ Chunk_close(&archiveEntryInfo->file.chunkFileEntry.info); });

            // get file meta data
            String_set(fileName,archiveEntryInfo->file.chunkFileEntry.name);
            if (fileInfo != NULL)
            {
              memset(fileInfo,0,sizeof(FileInfo));
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
          case CHUNK_ID_FILE_EXTENDED_ATTRIBUTE:
            {
              if (fileExtendedAttributeList != NULL)
              {
                // read file extended attribute chunk
                error = Chunk_open(&archiveEntryInfo->file.chunkFileExtendedAttribute.info,
                                   &subChunkHeader,
                                   subChunkHeader.size
                                  );
                if (error != ERROR_NONE)
                {
                  break;
                }

                // add extended attribute to list
                File_addExtendedAttribute(fileExtendedAttributeList,
                                          archiveEntryInfo->file.chunkFileExtendedAttribute.name,
                                          archiveEntryInfo->file.chunkFileExtendedAttribute.value.data,
                                          archiveEntryInfo->file.chunkFileExtendedAttribute.value.length
                                         );

                // close file extended attribute chunk
                error = Chunk_close(&archiveEntryInfo->file.chunkFileExtendedAttribute.info);
                if (error != ERROR_NONE)
                {
                  break;
                }
              }
              else
              {
                // skip file extended attribute chunk
                error = Chunk_skipSub(&archiveEntryInfo->file.chunkFile.info,&subChunkHeader);
                if (error != ERROR_NONE)
                {
                  break;
                }
              }
            }
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
            if (   !Compress_isValidAlgorithm(archiveEntryInfo->file.chunkFileDelta.deltaAlgorithm)
                || !Compress_isXDeltaCompressed(archiveEntryInfo->file.deltaCompressAlgorithm)
               )
            {
              (void)Chunk_close(&archiveEntryInfo->file.chunkFileDelta.info);
              error = ERROR_INVALID_COMPRESS_ALGORITHM;
              break;
            }
            if (deltaSourceName != NULL) String_set(deltaSourceName,archiveEntryInfo->file.chunkFileDelta.name);
            if (deltaSourceSize != NULL) (*deltaSourceSize) = archiveEntryInfo->file.chunkFileDelta.size;

            // close file delta chunk
            error = Chunk_close(&archiveEntryInfo->file.chunkFileDelta.info);
            if (error != ERROR_NONE)
            {
              break;
            }
            break;
          case CHUNK_ID_FILE_DATA:
            // read file data chunk
            error = Chunk_open(&archiveEntryInfo->file.chunkFileData.info,
                               &subChunkHeader,
                               Chunk_getSize(&archiveEntryInfo->file.chunkFileData.info,NULL,0)
                              );
            if (error != ERROR_NONE)
            {
              break;
            }
            AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->file.chunkFileData.info,{ Chunk_close(&archiveEntryInfo->file.chunkFileData.info); });

            // get data meta data
            if (fragmentOffset != NULL) (*fragmentOffset) = archiveEntryInfo->file.chunkFileData.fragmentOffset;
            if (fragmentSize   != NULL) (*fragmentSize)   = archiveEntryInfo->file.chunkFileData.fragmentSize;

            foundFileDataFlag = TRUE;
            break;
          default:
            // unknown sub-chunk -> skip
            if (globalOptions.verboseLevel >= 3)
            {
              printWarning("Skipped unknown sub-chunk '%s' (offset %llu) in '%s'.\n",
                           Chunk_idToString(subChunkHeader.id),
                           subChunkHeader.offset,
                           String_cString(archiveInfo->printableStorageName)
                          );
            }
            error = Chunk_skipSub(&archiveEntryInfo->file.chunkFile.info,&subChunkHeader);
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
      if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && (archiveInfo->cryptType != CRYPT_TYPE_ASYMMETRIC))
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
  }
  while ((error != ERROR_NONE) && (password != NULL));
  AUTOFREE_ADD(&autoFreeList1,&autoFreeList2,{ AutoFree_cleanup(&autoFreeList2); });
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  // check if mandatory file entry chunk and file data chunk found
  if (!foundFileEntryFlag || !foundFileDataFlag)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    if      (error != ERROR_NONE)                                                  return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                       return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundFileEntryFlag)                                                  return ERROR_NO_FILE_ENTRY;
    else if (!foundFileDataFlag)                                                   return ERRORX_(NO_FILE_DATA,0,"%s",String_cString(fileName));
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  // init delta decompress (if no delta-compression is enabled, use identity-compressor)
  error = Compress_init(&archiveEntryInfo->file.deltaCompressInfo,
                        COMPRESS_MODE_INFLATE,
                        archiveEntryInfo->file.deltaCompressAlgorithm,
                        1,
                        &archiveEntryInfo->file.deltaSourceHandle
                       );
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->file.deltaCompressInfo,{ Compress_done(&archiveEntryInfo->file.deltaCompressInfo); });

  // init byte decompress
  error = Compress_init(&archiveEntryInfo->file.byteCompressInfo,
                        COMPRESS_MODE_INFLATE,
                        archiveEntryInfo->file.byteCompressAlgorithm,
                        archiveEntryInfo->blockLength,
                        NULL
                       );
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->file.byteCompressInfo,{ Compress_done(&archiveEntryInfo->file.byteCompressInfo); });

  // init variables
  if (deltaCompressAlgorithm != NULL) (*deltaCompressAlgorithm) = archiveEntryInfo->file.deltaCompressAlgorithm;
  if (byteCompressAlgorithm  != NULL) (*byteCompressAlgorithm)  = archiveEntryInfo->file.byteCompressAlgorithm;
  if (cryptAlgorithm         != NULL) (*cryptAlgorithm)         = archiveEntryInfo->cryptAlgorithm;
  if (cryptType              != NULL) (*cryptType)              = archiveInfo->cryptType;

  // done resources
  AutoFree_done(&autoFreeList2);
  AutoFree_done(&autoFreeList1);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_readImageEntry(ArchiveEntryInfo   *archiveEntryInfo,
                                ArchiveInfo        *archiveInfo,
                                CompressAlgorithms *deltaCompressAlgorithm,
                                CompressAlgorithms *byteCompressAlgorithm,
                                CryptAlgorithms    *cryptAlgorithm,
                                CryptTypes         *cryptType,
                                String             deviceName,
                                DeviceInfo         *deviceInfo,
                                FileSystemTypes    *fileSystemType,
                                String             deltaSourceName,
                                uint64             *deltaSourceSize,
                                uint64             *blockOffset,
                                uint64             *blockCount
                               )
#else /* not NDEBUG */
  Errors __Archive_readImageEntry(const char         *__fileName__,
                                  ulong              __lineNb__,
                                  ArchiveEntryInfo   *archiveEntryInfo,
                                  ArchiveInfo        *archiveInfo,
                                  CompressAlgorithms *deltaCompressAlgorithm,
                                  CompressAlgorithms *byteCompressAlgorithm,
                                  CryptAlgorithms    *cryptAlgorithm,
                                  CryptTypes         *cryptType,
                                  String             deviceName,
                                  DeviceInfo         *deviceInfo,
                                  FileSystemTypes    *fileSystemType,
                                  String             deltaSourceName,
                                  uint64             *deltaSourceSize,
                                  uint64             *blockOffset,
                                  uint64             *blockCount
                                 )
#endif /* NDEBUG */
{
  AutoFreeList   autoFreeList1,autoFreeList2;
  Errors         error;
  ChunkHeader    chunkHeader;
  uint64         index;
  PasswordHandle passwordHandle;
  const Password *password;
  bool           passwordFlag;
  bool           decryptedFlag;
  ChunkHeader    subChunkHeader;
  bool           foundImageEntryFlag,foundImageDataFlag;

  assert(archiveEntryInfo != NULL);
  assert(archiveInfo != NULL);
  assert(archiveInfo->jobOptions != NULL);
  assert(deviceInfo != NULL);

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  if (deltaSourceSize != NULL) (*deltaSourceSize) = 0LL;

  archiveEntryInfo->archiveInfo                = archiveInfo;
  archiveEntryInfo->mode                       = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType           = ARCHIVE_ENTRY_TYPE_IMAGE;

  archiveEntryInfo->image.deltaSourceHandleInitFlag = FALSE;

  archiveEntryInfo->image.byteBuffer           = NULL;
  archiveEntryInfo->image.byteBufferSize       = 0L;
  archiveEntryInfo->image.deltaBuffer          = NULL;
  archiveEntryInfo->image.deltaBufferSize      = 0L;

  // init image chunk
  error = Chunk_init(&archiveEntryInfo->image.chunkImage.info,
                     NULL,  // parentChunkInfo
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_IMAGE,
                     CHUNK_DEFINITION_IMAGE,
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->image.chunkImage
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->image.chunkImage.info,{ Chunk_done(&archiveEntryInfo->image.chunkImage.info); });

  // find next image chunk
  do
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList1);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_IMAGE)
    {
      error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList1);
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_IMAGE);

  // read image chunk
  error = Chunk_open(&archiveEntryInfo->image.chunkImage.info,
                     &chunkHeader,
                     Chunk_getSize(&archiveEntryInfo->image.chunkImage.info,NULL,0)
                    );
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->image.chunkImage.info,{ Chunk_close(&archiveEntryInfo->image.chunkImage.info); });
  if (!Compress_isValidAlgorithm(archiveEntryInfo->image.chunkImage.compressAlgorithm))
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_COMPRESS_ALGORITHM;
  }
  archiveEntryInfo->image.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->image.byteCompressAlgorithm  = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->image.chunkImage.compressAlgorithm);
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->image.chunkImage.cryptAlgorithm))
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
  archiveEntryInfo->cryptAlgorithm = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->image.chunkImage.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  // allocate buffers
  archiveEntryInfo->image.byteBufferSize = FLOOR(MAX_BUFFER_SIZE,archiveEntryInfo->blockLength);
  archiveEntryInfo->image.byteBuffer = (byte*)malloc(archiveEntryInfo->image.byteBufferSize);
  if (archiveEntryInfo->image.byteBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList1,archiveEntryInfo->image.byteBuffer,{ free(archiveEntryInfo->image.byteBuffer); });
  archiveEntryInfo->image.deltaBufferSize = MAX_BUFFER_SIZE;
  archiveEntryInfo->image.deltaBuffer = (byte*)malloc(archiveEntryInfo->image.deltaBufferSize);
  if (archiveEntryInfo->image.deltaBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList1,archiveEntryInfo->image.deltaBuffer,{ free(archiveEntryInfo->image.deltaBuffer); });

  // try to read image entry with all passwords
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->image.chunkImage.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm))
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo,
                                         archiveInfo->jobOptions,
                                         archiveInfo->jobOptions->cryptPasswordMode,
                                         archiveInfo->getPasswordFunction,
                                         archiveInfo->getPasswordUserData
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
  do
  {
    // reset
    error = ERROR_NONE;
    AutoFree_freeAll(&autoFreeList2);

    // reset chunk read position
    Chunk_seek(&archiveEntryInfo->image.chunkImage.info,index);

    // init image entry/delta/data/image crypt
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->image.chunkImageEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->image.chunkImageEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->image.chunkImageDelta.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->image.chunkImageDelta.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->image.chunkImageData.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->image.chunkImageData.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->image.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->image.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.cryptInfo); });
      }
    }

    // init image entry chunk
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->image.chunkImageEntry.info,
                         &archiveEntryInfo->image.chunkImage.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_IMAGE_ENTRY,
                         CHUNK_DEFINITION_IMAGE_ENTRY,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->image.chunkImageEntry.cryptInfo,
                         &archiveEntryInfo->image.chunkImageEntry
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->image.chunkImageEntry.info,{ Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->image.chunkImageDelta.info,
                         &archiveEntryInfo->image.chunkImage.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_IMAGE_DELTA,
                         CHUNK_DEFINITION_IMAGE_DELTA,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->image.chunkImageDelta.cryptInfo,
                         &archiveEntryInfo->image.chunkImageDelta
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->image.chunkImageDelta.info,{ Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->image.chunkImageData.info,
                         &archiveEntryInfo->image.chunkImage.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_IMAGE_DATA,
                         CHUNK_DEFINITION_IMAGE_DATA,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->image.chunkImageData.cryptInfo,
                         &archiveEntryInfo->image.chunkImageData
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->image.chunkImageData.info,{ Chunk_done(&archiveEntryInfo->image.chunkImageData.info); });
      }
    }

    // try to read image entry/data chunks
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveEntryInfo->image.chunkImage.info)
             && !foundImageDataFlag
             && (error == ERROR_NONE)
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
            AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->image.chunkImageEntry.info,{ Chunk_close(&archiveEntryInfo->image.chunkImageEntry.info); });

            // get image meta data
            String_set(deviceName,archiveEntryInfo->image.chunkImageEntry.name);
            if (fileSystemType != NULL)
            {
              (*fileSystemType) = FILE_SYSTEM_CONSTANT_TO_TYPE(archiveEntryInfo->image.chunkImageEntry.fileSystemType);
            }
            if (deviceInfo != NULL)
            {
              deviceInfo->size      = archiveEntryInfo->image.chunkImageEntry.size;
              deviceInfo->blockSize = archiveEntryInfo->image.chunkImageEntry.blockSize;
            }

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
            if (   !Compress_isValidAlgorithm(archiveEntryInfo->image.chunkImageDelta.deltaAlgorithm)
                || !Compress_isXDeltaCompressed(archiveEntryInfo->image.deltaCompressAlgorithm)
               )
            {
              (void)Chunk_close(&archiveEntryInfo->image.chunkImageDelta.info);
              error = ERROR_INVALID_COMPRESS_ALGORITHM;
              break;
            }
            if (deltaSourceName != NULL) String_set(deltaSourceName,archiveEntryInfo->image.chunkImageDelta.name);
            if (deltaSourceSize != NULL) (*deltaSourceSize) = archiveEntryInfo->image.chunkImageDelta.size;

            // close image delta chunk
            error = Chunk_close(&archiveEntryInfo->image.chunkImageDelta.info);
            if (error != ERROR_NONE)
            {
              break;
            }
            break;
          case CHUNK_ID_IMAGE_DATA:
            // read image data chunk
            error = Chunk_open(&archiveEntryInfo->image.chunkImageData.info,
                               &subChunkHeader,
                               Chunk_getSize(&archiveEntryInfo->image.chunkImageData.info,NULL,0)
                              );
            if (error != ERROR_NONE)
            {
              break;
            }
            AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->image.chunkImageData.info,{ Chunk_close(&archiveEntryInfo->image.chunkImageData.info); });

            if (blockOffset != NULL) (*blockOffset) = archiveEntryInfo->image.chunkImageData.blockOffset;
            if (blockCount  != NULL) (*blockCount)  = archiveEntryInfo->image.chunkImageData.blockCount;

            foundImageDataFlag = TRUE;
            break;
          default:
            // unknown sub-chunk -> skip
            if (globalOptions.verboseLevel >= 3)
            {
              printWarning("Skipped unknown sub-chunk '%s' (offset %llu) in '%s'.\n",
                           Chunk_idToString(subChunkHeader.id),
                           subChunkHeader.offset,
                           String_cString(archiveInfo->printableStorageName)
                          );
            }
            error = Chunk_skipSub(&archiveEntryInfo->image.chunkImage.info,&subChunkHeader);
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
      if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && (archiveInfo->cryptType != CRYPT_TYPE_ASYMMETRIC))
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
  }
  while ((error != ERROR_NONE) && (password != NULL));
  AUTOFREE_ADD(&autoFreeList1,&autoFreeList2,{ AutoFree_cleanup(&autoFreeList2); });
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  if (!foundImageEntryFlag || !foundImageDataFlag)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    if      (error != ERROR_NONE)                                                  return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                       return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundImageEntryFlag)                                                 return ERROR_NO_IMAGE_ENTRY;
    else if (!foundImageDataFlag)                                                  return ERROR_NO_IMAGE_DATA;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  // init delta decompress (if no delta-compression is enabled, use identity-compressor)
  error = Compress_init(&archiveEntryInfo->image.deltaCompressInfo,
                        COMPRESS_MODE_INFLATE,
                        archiveEntryInfo->image.deltaCompressAlgorithm,
                        1,
                        &archiveEntryInfo->image.deltaSourceHandle
                       );
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->image.deltaCompressInfo,{ Compress_done(&archiveEntryInfo->image.deltaCompressInfo); });

  // init byte decompress
  error = Compress_init(&archiveEntryInfo->image.byteCompressInfo,
                        COMPRESS_MODE_INFLATE,
                        archiveEntryInfo->image.byteCompressAlgorithm,
                        archiveEntryInfo->blockLength,
                        NULL
                       );
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->image.byteCompressInfo,{ Compress_done(&archiveEntryInfo->image.byteCompressInfo); });

  // init variables
  if (deltaCompressAlgorithm != NULL) (*deltaCompressAlgorithm) = archiveEntryInfo->image.deltaCompressAlgorithm;
  if (byteCompressAlgorithm  != NULL) (*byteCompressAlgorithm)  = archiveEntryInfo->image.byteCompressAlgorithm;
  if (cryptAlgorithm         != NULL) (*cryptAlgorithm)         = archiveEntryInfo->cryptAlgorithm;
  if (cryptType              != NULL) (*cryptType)              = archiveInfo->cryptType;

  // done resources
  AutoFree_done(&autoFreeList2);
  AutoFree_done(&autoFreeList1);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_readDirectoryEntry(ArchiveEntryInfo          *archiveEntryInfo,
                                    ArchiveInfo               *archiveInfo,
                                    CryptAlgorithms           *cryptAlgorithm,
                                    CryptTypes                *cryptType,
                                    String                    directoryName,
                                    FileInfo                  *fileInfo,
                                    FileExtendedAttributeList *fileExtendedAttributeList
                                   )
#else /* not NDEBUG */
  Errors __Archive_readDirectoryEntry(const char                *__fileName__,
                                      ulong                     __lineNb__,
                                      ArchiveEntryInfo          *archiveEntryInfo,
                                      ArchiveInfo               *archiveInfo,
                                      CryptAlgorithms           *cryptAlgorithm,
                                      CryptTypes                *cryptType,
                                      String                    directoryName,
                                      FileInfo                  *fileInfo,
                                      FileExtendedAttributeList *fileExtendedAttributeList
                                     )
#endif /* NDEBUG */
{
  AutoFreeList   autoFreeList1,autoFreeList2;
  Errors         error;
  ChunkHeader    chunkHeader;
  uint64         index;
  PasswordHandle passwordHandle;
  const Password *password;
  bool           passwordFlag;
  bool           decryptedFlag;
  bool           foundDirectoryEntryFlag;
  ChunkHeader    subChunkHeader;

  assert(archiveEntryInfo != NULL);
  assert(archiveInfo != NULL);
  assert(archiveInfo->jobOptions != NULL);

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_DIRECTORY;

  // init directory chunk
  error = Chunk_init(&archiveEntryInfo->directory.chunkDirectory.info,
                     NULL,  // parentChunkInfo
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_DIRECTORY,
                     CHUNK_DEFINITION_DIRECTORY,
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->directory.chunkDirectory
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->directory.chunkDirectory.info,{ Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info); });

  // find next directory chunk
  do
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList1);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_DIRECTORY)
    {
      error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList1);
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_DIRECTORY);

  // read directory chunk
  error = Chunk_open(&archiveEntryInfo->directory.chunkDirectory.info,
                     &chunkHeader,
                     Chunk_getSize(&archiveEntryInfo->directory.chunkDirectory.info,NULL,0)
                    );
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->directory.chunkDirectory.info,{ Chunk_close(&archiveEntryInfo->directory.chunkDirectory.info); });
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->directory.chunkDirectory.cryptAlgorithm))
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
  archiveEntryInfo->cryptAlgorithm = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->directory.chunkDirectory.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  // try to read directory entry with all passwords
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->directory.chunkDirectory.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm))
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo,
                                         archiveInfo->jobOptions,
                                         archiveInfo->jobOptions->cryptPasswordMode,
                                         archiveInfo->getPasswordFunction,
                                         archiveInfo->getPasswordUserData
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
  do
  {
    // reset
    error = ERROR_NONE;
    AutoFree_freeAll(&autoFreeList2);

    // reset chunk read position
    Chunk_seek(&archiveEntryInfo->directory.chunkDirectory.info,index);

    // init crypt
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.cryptInfo); });
      }
    }

    // init directory entry chunk
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->directory.chunkDirectoryEntry.info,
                         &archiveEntryInfo->directory.chunkDirectory.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_DIRECTORY_ENTRY,
                         CHUNK_DEFINITION_DIRECTORY_ENTRY,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,
                         &archiveEntryInfo->directory.chunkDirectoryEntry
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->directory.chunkDirectoryEntry.info,{ Chunk_done(&archiveEntryInfo->directory.chunkDirectoryEntry.info); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.info,
                         &archiveEntryInfo->directory.chunkDirectory.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_DIRECTORY_EXTENDED_ATTRIBUTE,
                         CHUNK_DEFINITION_DIRECTORY_EXTENDED_ATTRIBUTE,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.cryptInfo,
                         &archiveEntryInfo->directory.chunkDirectoryExtendedAttribute
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.info,{ Chunk_done(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.info); });
      }
    }

    // read directory entry
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveEntryInfo->directory.chunkDirectory.info)
             && (error == ERROR_NONE)
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
              memset(fileInfo,0,sizeof(FileInfo));
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
          case CHUNK_ID_DIRECTORY_EXTENDED_ATTRIBUTE:
            {
              if (fileExtendedAttributeList != NULL)
              {
                // read directory extended attribute chunk
                error = Chunk_open(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.info,
                                   &subChunkHeader,
                                   subChunkHeader.size
                                  );
                if (error != ERROR_NONE)
                {
                  break;
                }

                // add extended attribute to list
                File_addExtendedAttribute(fileExtendedAttributeList,
                                          archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.name,
                                          archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.value.data,
                                          archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.value.length
                                         );

                // close directory extended attribute chunk
                error = Chunk_close(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.info);
                if (error != ERROR_NONE)
                {
                  break;
                }
              }
              else
              {
                // skip file extended attribute chunk
                error = Chunk_skipSub(&archiveEntryInfo->directory.chunkDirectory.info,&subChunkHeader);
                if (error != ERROR_NONE)
                {
                  break;
                }
              }
            }
            break;
          default:
            // unknown sub-chunk -> skip
            if (globalOptions.verboseLevel >= 3)
            {
              printWarning("Skipped unknown sub-chunk '%s' (offset %llu).\n",
                           Chunk_idToString(subChunkHeader.id),
                           subChunkHeader.offset,
                           String_cString(archiveInfo->printableStorageName)
                          );
            }
            error = Chunk_skipSub(&archiveEntryInfo->directory.chunkDirectory.info,&subChunkHeader);
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
      if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && (archiveInfo->cryptType != CRYPT_TYPE_ASYMMETRIC))
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
  }
  while ((error != ERROR_NONE) && (password != NULL));
  AUTOFREE_ADD(&autoFreeList1,&autoFreeList2,{ AutoFree_cleanup(&autoFreeList2); });
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  if (!foundDirectoryEntryFlag)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    if      (error != ERROR_NONE)                                                  return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                       return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundDirectoryEntryFlag)                                             return ERROR_NO_DIRECTORY_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveEntryInfo->cryptAlgorithm;
  if (cryptType      != NULL) (*cryptType)      = archiveInfo->cryptType;

  // done resources
  AutoFree_done(&autoFreeList2);
  AutoFree_done(&autoFreeList1);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_readLinkEntry(ArchiveEntryInfo          *archiveEntryInfo,
                               ArchiveInfo               *archiveInfo,
                               CryptAlgorithms           *cryptAlgorithm,
                               CryptTypes                *cryptType,
                               String                    linkName,
                               String                    destinationName,
                               FileInfo                  *fileInfo,
                               FileExtendedAttributeList *fileExtendedAttributeList
                              )
#else /* not NDEBUG */
  Errors __Archive_readLinkEntry(const char                *__fileName__,
                                 ulong                     __lineNb__,
                                 ArchiveEntryInfo          *archiveEntryInfo,
                                 ArchiveInfo               *archiveInfo,
                                 CryptAlgorithms           *cryptAlgorithm,
                                 CryptTypes                *cryptType,
                                 String                    linkName,
                                 String                    destinationName,
                                 FileInfo                  *fileInfo,
                                 FileExtendedAttributeList *fileExtendedAttributeList
                                )
#endif /* NDEBUG */
{
  AutoFreeList   autoFreeList1,autoFreeList2;
  Errors         error;
  ChunkHeader    chunkHeader;
  uint64         index;
  PasswordHandle passwordHandle;
  const Password *password;
  bool           passwordFlag;
  bool           decryptedFlag;
  bool           foundLinkEntryFlag;
  ChunkHeader    subChunkHeader;

  assert(archiveEntryInfo != NULL);
  assert(archiveInfo != NULL);
  assert(archiveInfo->jobOptions != NULL);

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_LINK;

  // init link chunk
  error = Chunk_init(&archiveEntryInfo->link.chunkLink.info,
                     NULL,  // parentChunkInfo
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_LINK,
                     CHUNK_DEFINITION_LINK,
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->link.chunkLink
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->link.chunkLink.info,{ Chunk_done(&archiveEntryInfo->link.chunkLink.info); });

  // find next soft link chunk
  do
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList1);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_LINK)
    {
      if (error != ERROR_NONE)
      {
        archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
        AutoFree_cleanup(&autoFreeList1);
        return error;
      }
      archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_LINK);

  // read link chunk
  error = Chunk_open(&archiveEntryInfo->link.chunkLink.info,
                     &chunkHeader,
                     Chunk_getSize(&archiveEntryInfo->link.chunkLink.info,NULL,0)
                    );
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->link.chunkLink.info,{ Chunk_close(&archiveEntryInfo->link.chunkLink.info); });
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->link.chunkLink.cryptAlgorithm))
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
  archiveEntryInfo->cryptAlgorithm = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->link.chunkLink.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  // try to read link entry with all passwords
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->link.chunkLink.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm))
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo,
                                         archiveInfo->jobOptions,
                                         archiveInfo->jobOptions->cryptPasswordMode,
                                         archiveInfo->getPasswordFunction,
                                         archiveInfo->getPasswordUserData
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
  do
  {
    // reset
    error = ERROR_NONE;
    AutoFree_freeAll(&autoFreeList2);

    // reset chunk read position
    Chunk_seek(&archiveEntryInfo->link.chunkLink.info,index);

    // init crypt
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->link.chunkLinkEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->link.chunkLinkExtendedAttribute.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->link.chunkLinkExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->link.chunkLinkExtendedAttribute.cryptInfo); });
      }
    }

    // init chunks
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->link.chunkLinkEntry.info,
                         &archiveEntryInfo->link.chunkLink.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_LINK_ENTRY,
                         CHUNK_DEFINITION_LINK_ENTRY,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->link.chunkLinkEntry.cryptInfo,
                         &archiveEntryInfo->link.chunkLinkEntry
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->link.chunkLinkEntry.info,{ Chunk_done(&archiveEntryInfo->link.chunkLinkEntry.info); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->link.chunkLinkExtendedAttribute.info,
                         &archiveEntryInfo->link.chunkLink.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_LINK_EXTENDED_ATTRIBUTE,
                         CHUNK_DEFINITION_LINK_EXTENDED_ATTRIBUTE,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->link.chunkLinkExtendedAttribute.cryptInfo,
                         &archiveEntryInfo->link.chunkLinkExtendedAttribute
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->link.chunkLinkExtendedAttribute.info,{ Chunk_done(&archiveEntryInfo->link.chunkLinkExtendedAttribute.info); });
      }
    }

    // read link entry
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveEntryInfo->link.chunkLink.info)
             && (error == ERROR_NONE)
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
              memset(fileInfo,0,sizeof(FileInfo));
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
          case CHUNK_ID_LINK_EXTENDED_ATTRIBUTE:
            {
              if (fileExtendedAttributeList != NULL)
              {
                // read link extended attribute chunk
                error = Chunk_open(&archiveEntryInfo->link.chunkLinkExtendedAttribute.info,
                                   &subChunkHeader,
                                   subChunkHeader.size
                                  );
                if (error != ERROR_NONE)
                {
                  break;
                }

                // add extended attribute to list
                File_addExtendedAttribute(fileExtendedAttributeList,
                                          archiveEntryInfo->link.chunkLinkExtendedAttribute.name,
                                          archiveEntryInfo->link.chunkLinkExtendedAttribute.value.data,
                                          archiveEntryInfo->link.chunkLinkExtendedAttribute.value.length
                                         );

                // close link extended attribute chunk
                error = Chunk_close(&archiveEntryInfo->link.chunkLinkExtendedAttribute.info);
                if (error != ERROR_NONE)
                {
                  break;
                }
              }
              else
              {
                // skip file extended attribute chunk
                error = Chunk_skipSub(&archiveEntryInfo->link.chunkLink.info,&subChunkHeader);
                if (error != ERROR_NONE)
                {
                  break;
                }
              }
            }
            break;
          default:
            // unknown sub-chunk -> skip
            if (globalOptions.verboseLevel >= 3)
            {
              printWarning("Skipped unknown sub-chunk '%s' (offset %llu).\n",
                           Chunk_idToString(subChunkHeader.id),
                           subChunkHeader.offset,
                           String_cString(archiveInfo->printableStorageName)
                          );
            }
            error = Chunk_skipSub(&archiveEntryInfo->link.chunkLink.info,&subChunkHeader);
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
      if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && (archiveInfo->cryptType != CRYPT_TYPE_ASYMMETRIC))
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
  }
  while ((error != ERROR_NONE) && (password != NULL));
  AUTOFREE_ADD(&autoFreeList1,&autoFreeList2,{ AutoFree_cleanup(&autoFreeList2); });
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  if (!foundLinkEntryFlag)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    if      (error != ERROR_NONE)                                                  return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                       return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundLinkEntryFlag)                                                  return ERROR_NO_LINK_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveEntryInfo->cryptAlgorithm;
  if (cryptType      != NULL) (*cryptType)      = archiveInfo->cryptType;

  // done resources
  AutoFree_done(&autoFreeList2);
  AutoFree_done(&autoFreeList1);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_readHardLinkEntry(ArchiveEntryInfo          *archiveEntryInfo,
                                   ArchiveInfo               *archiveInfo,
                                   CompressAlgorithms        *deltaCompressAlgorithm,
                                   CompressAlgorithms        *byteCompressAlgorithm,
                                   CryptAlgorithms           *cryptAlgorithm,
                                   CryptTypes                *cryptType,
                                   StringList                *fileNameList,
                                   FileInfo                  *fileInfo,
                                   FileExtendedAttributeList *fileExtendedAttributeList,
                                   String                    deltaSourceName,
                                   uint64                    *deltaSourceSize,
                                   uint64                    *fragmentOffset,
                                   uint64                    *fragmentSize
                                  )
#else /* not NDEBUG */
  Errors __Archive_readHardLinkEntry(const char                *__fileName__,
                                     ulong                     __lineNb__,
                                     ArchiveEntryInfo          *archiveEntryInfo,
                                     ArchiveInfo               *archiveInfo,
                                     CompressAlgorithms        *deltaCompressAlgorithm,
                                     CompressAlgorithms        *byteCompressAlgorithm,
                                     CryptAlgorithms           *cryptAlgorithm,
                                     CryptTypes                *cryptType,
                                     StringList                *fileNameList,
                                     FileInfo                  *fileInfo,
                                     FileExtendedAttributeList *fileExtendedAttributeList,
                                     String                    deltaSourceName,
                                     uint64                    *deltaSourceSize,
                                     uint64                    *fragmentOffset,
                                     uint64                    *fragmentSize
                                    )
#endif /* NDEBUG */
{
  AutoFreeList   autoFreeList1,autoFreeList2;
  Errors         error;
  ChunkHeader    chunkHeader;
  uint64         index;
  PasswordHandle passwordHandle;
  const Password *password;
  bool           passwordFlag;
  bool           decryptedFlag;
  ChunkHeader    subChunkHeader;
  bool           foundHardLinkEntryFlag,foundHardLinkDataFlag;

  assert(archiveEntryInfo != NULL);
  assert(archiveInfo != NULL);
  assert(archiveInfo->jobOptions != NULL);
  assert(fileNameList != NULL);

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  if (deltaSourceSize != NULL) (*deltaSourceSize) = 0LL;

  archiveEntryInfo->archiveInfo                   = archiveInfo;
  archiveEntryInfo->mode                          = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType              = ARCHIVE_ENTRY_TYPE_HARDLINK;

  archiveEntryInfo->hardLink.deltaSourceHandleInitFlag = FALSE;

  archiveEntryInfo->hardLink.byteBuffer           = NULL;
  archiveEntryInfo->hardLink.byteBufferSize       = 0L;
  archiveEntryInfo->hardLink.deltaBuffer          = NULL;
  archiveEntryInfo->hardLink.deltaBufferSize      = 0L;

  // init hard link chunk
  error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLink.info,
                     NULL,  // parentChunkInfo
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_HARDLINK,
                     CHUNK_DEFINITION_HARDLINK,
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->hardLink.chunkHardLink
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->hardLink.chunkHardLink.info,{ Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info); });

  // find next hard link chunk
  do
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList1);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_HARDLINK)
    {
      error = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList1);
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_HARDLINK);

  // read hard link chunk
  error = Chunk_open(&archiveEntryInfo->hardLink.chunkHardLink.info,
                     &chunkHeader,
                     Chunk_getSize(&archiveEntryInfo->hardLink.chunkHardLink.info,NULL,0)
                    );
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->hardLink.chunkHardLink.info,{ Chunk_close(&archiveEntryInfo->hardLink.chunkHardLink.info); });
  if (!Compress_isValidAlgorithm(archiveEntryInfo->hardLink.chunkHardLink.compressAlgorithm))
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_COMPRESS_ALGORITHM;
  }
  archiveEntryInfo->hardLink.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->hardLink.byteCompressAlgorithm  = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->hardLink.chunkHardLink.compressAlgorithm);
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithm))
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
  archiveEntryInfo->cryptAlgorithm = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  // allocate buffers
  archiveEntryInfo->hardLink.byteBufferSize = FLOOR(MAX_BUFFER_SIZE,archiveEntryInfo->blockLength);
  archiveEntryInfo->hardLink.byteBuffer = (byte*)malloc(archiveEntryInfo->hardLink.byteBufferSize);
  if (archiveEntryInfo->hardLink.byteBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList1,archiveEntryInfo->hardLink.byteBuffer,{ free(archiveEntryInfo->hardLink.byteBuffer); });
  archiveEntryInfo->hardLink.deltaBufferSize = MAX_BUFFER_SIZE;
  archiveEntryInfo->hardLink.deltaBuffer = (byte*)malloc(archiveEntryInfo->hardLink.deltaBufferSize);
  if (archiveEntryInfo->hardLink.deltaBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList1,archiveEntryInfo->hardLink.deltaBuffer,{ free(archiveEntryInfo->hardLink.deltaBuffer); });

  // try to read hard link entry with all passwords
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->hardLink.chunkHardLink.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm))
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo,
                                         archiveInfo->jobOptions,
                                         archiveInfo->jobOptions->cryptPasswordMode,
                                         archiveInfo->getPasswordFunction,
                                         archiveInfo->getPasswordUserData
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
  do
  {
    // reset
    error = ERROR_NONE;
    AutoFree_freeAll(&autoFreeList2);

    // reset chunk read position
    Chunk_seek(&archiveEntryInfo->hardLink.chunkHardLink.info,index);

    // init crypt
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.cryptInfo); });
      }
    }

    // init hardlink entry/extended attributes/delta/data chunks
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info,
                         &archiveEntryInfo->hardLink.chunkHardLink.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_HARDLINK_ENTRY,
                         CHUNK_DEFINITION_HARDLINK_ENTRY,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,
                         &archiveEntryInfo->hardLink.chunkHardLinkEntry
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkEntry.info,{ Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.info,
                         &archiveEntryInfo->hardLink.chunkHardLink.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_HARDLINK_EXTENDED_ATTRIBUTE,
                         CHUNK_DEFINITION_HARDLINK_EXTENDED_ATTRIBUTE,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo,
                         &archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.info,{ Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.info); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkName.info,
                         &archiveEntryInfo->hardLink.chunkHardLink.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_HARDLINK_NAME,
                         CHUNK_DEFINITION_HARDLINK_NAME,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo,
                         &archiveEntryInfo->hardLink.chunkHardLinkName
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkName.info,{ Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkName.info); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info,
                         &archiveEntryInfo->hardLink.chunkHardLink.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_HARDLINK_DELTA,
                         CHUNK_DEFINITION_HARDLINK_DELTA,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,
                         &archiveEntryInfo->hardLink.chunkHardLinkDelta
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkDelta.info,{ Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                         &archiveEntryInfo->hardLink.chunkHardLink.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_HARDLINK_DATA,
                         CHUNK_DEFINITION_HARDLINK_DATA,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,
                         &archiveEntryInfo->hardLink.chunkHardLinkData
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkData.info,{ Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info); });
      }
    }

    // try to read hard link entry/name/data chunks
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveEntryInfo->hardLink.chunkHardLink.info)
             && !foundHardLinkDataFlag
             && (error == ERROR_NONE)
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
            AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkEntry.info,{ Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info); });

            // get hard link meta data
            if (fileInfo != NULL)
            {
              memset(fileInfo,0,sizeof(FileInfo));
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
            // read hard link name chunk
            error = Chunk_open(&archiveEntryInfo->hardLink.chunkHardLinkName.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            StringList_append(fileNameList,archiveEntryInfo->hardLink.chunkHardLinkName.name);

            // close hard link name chunk
            error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkName.info);
            if (error != ERROR_NONE)
            {
              break;
            }
            break;
          case CHUNK_ID_HARDLINK_EXTENDED_ATTRIBUTE:
            {
              if (fileExtendedAttributeList != NULL)
              {
                // read hard link extended attribute chunk
                error = Chunk_open(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.info,
                                   &subChunkHeader,
                                   subChunkHeader.size
                                  );
                if (error != ERROR_NONE)
                {
                  break;
                }

                // add extended attribute to list
                File_addExtendedAttribute(fileExtendedAttributeList,
                                          archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.name,
                                          archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.value.data,
                                          archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.value.length
                                         );

                // close hard link extended attribute name chunk
                error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.info);
                if (error != ERROR_NONE)
                {
                  break;
                }
              }
              else
              {
                // skip file extended attribute chunk
                error = Chunk_skipSub(&archiveEntryInfo->hardLink.chunkHardLink.info,&subChunkHeader);
                if (error != ERROR_NONE)
                {
                  break;
                }
              }
            }
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
            if (   !Compress_isValidAlgorithm(archiveEntryInfo->hardLink.chunkHardLinkDelta.deltaAlgorithm)
                || !Compress_isXDeltaCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm)
               )
            {
              (void)Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
              error = ERROR_INVALID_COMPRESS_ALGORITHM;
              break;
            }
            if (deltaSourceName != NULL) String_set(deltaSourceName,archiveEntryInfo->hardLink.chunkHardLinkDelta.name);
            if (deltaSourceSize != NULL) (*deltaSourceSize) = archiveEntryInfo->hardLink.chunkHardLinkDelta.size;

            // close link extended attribute chunk
            error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
            if (error != ERROR_NONE)
            {
              break;
            }
            break;
          case CHUNK_ID_HARDLINK_DATA:
            // read hard link data chunk
            error = Chunk_open(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                               &subChunkHeader,
                               Chunk_getSize(&archiveEntryInfo->hardLink.chunkHardLinkData.info,NULL,0)
                              );
            if (error != ERROR_NONE)
            {
              break;
            }
            AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkData.info,{ Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkData.info); });

            // get fragment meta data
            if (fragmentOffset != NULL) (*fragmentOffset) = archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset;
            if (fragmentSize   != NULL) (*fragmentSize)   = archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize;

            foundHardLinkDataFlag = TRUE;
            break;
          default:
            // unknown sub-chunk -> skip
            if (globalOptions.verboseLevel >= 3)
            {
              printWarning("Skipped unknown sub-chunk '%s' (offset %llu).\n",
                           Chunk_idToString(subChunkHeader.id),
                           subChunkHeader.offset,
                           String_cString(archiveInfo->printableStorageName)
                          );
            }
            error = Chunk_skipSub(&archiveEntryInfo->hardLink.chunkHardLink.info,&subChunkHeader);
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
      if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && (archiveInfo->cryptType != CRYPT_TYPE_ASYMMETRIC))
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
  }
  while ((error != ERROR_NONE) && (password != NULL));
  AUTOFREE_ADD(&autoFreeList1,&autoFreeList2,{ AutoFree_cleanup(&autoFreeList2); });
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  if (!foundHardLinkEntryFlag || !foundHardLinkDataFlag)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    if      (error != ERROR_NONE)                                                  return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                       return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundHardLinkEntryFlag)                                              return ERROR_NO_FILE_ENTRY;
    else if (!foundHardLinkDataFlag)                                               return ERRORX_(NO_FILE_DATA,0,"%s",String_cString(StringList_first(fileNameList,NULL)));
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  // init delta decompress (if no delta-compression is enabled, use identity-compressor)
  error = Compress_init(&archiveEntryInfo->hardLink.deltaCompressInfo,
                        COMPRESS_MODE_INFLATE,
                        archiveEntryInfo->hardLink.deltaCompressAlgorithm,
                        1,
                        &archiveEntryInfo->hardLink.deltaSourceHandle
                       );
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->hardLink.deltaCompressInfo,{ Compress_done(&archiveEntryInfo->hardLink.deltaCompressInfo); });

  // init byte decompress
  error = Compress_init(&archiveEntryInfo->hardLink.byteCompressInfo,
                        COMPRESS_MODE_INFLATE,
                        archiveEntryInfo->hardLink.byteCompressAlgorithm,
                        archiveEntryInfo->blockLength,
                        NULL
                       );
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  // init variables
  if (deltaCompressAlgorithm != NULL) (*deltaCompressAlgorithm) = archiveEntryInfo->hardLink.deltaCompressAlgorithm;
  if (byteCompressAlgorithm  != NULL) (*byteCompressAlgorithm)  = archiveEntryInfo->hardLink.byteCompressAlgorithm;
  if (cryptAlgorithm         != NULL) (*cryptAlgorithm)         = archiveEntryInfo->cryptAlgorithm;
  if (cryptType              != NULL) (*cryptType)              = archiveInfo->cryptType;

  // done resources
  AutoFree_done(&autoFreeList2);
  AutoFree_done(&autoFreeList1);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_readSpecialEntry(ArchiveEntryInfo          *archiveEntryInfo,
                                  ArchiveInfo               *archiveInfo,
                                  CryptAlgorithms           *cryptAlgorithm,
                                  CryptTypes                *cryptType,
                                  String                    specialName,
                                  FileInfo                  *fileInfo,
                                  FileExtendedAttributeList *fileExtendedAttributeList
                                 )
#else /* not NDEBUG */
  Errors __Archive_readSpecialEntry(const char                *__fileName__,
                                    ulong                     __lineNb__,
                                    ArchiveEntryInfo          *archiveEntryInfo,
                                    ArchiveInfo               *archiveInfo,
                                    CryptAlgorithms           *cryptAlgorithm,
                                    CryptTypes                *cryptType,
                                    String                    specialName,
                                    FileInfo                  *fileInfo,
                                    FileExtendedAttributeList *fileExtendedAttributeList
                                   )
#endif /* NDEBUG */
{
  AutoFreeList   autoFreeList1,autoFreeList2;
  Errors         error;
  ChunkHeader    chunkHeader;
  uint64         index;
  PasswordHandle passwordHandle;
  const Password *password;
  bool           passwordFlag;
  bool           decryptedFlag;
  bool           foundSpecialEntryFlag;
  ChunkHeader    subChunkHeader;

  assert(archiveEntryInfo != NULL);
  assert(archiveInfo != NULL);
  assert(archiveInfo->jobOptions != NULL);

  // check for pending error
  if (archiveInfo->pendingError != ERROR_NONE)
  {
    error = archiveInfo->pendingError;
    archiveInfo->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  archiveEntryInfo->archiveInfo      = archiveInfo;
  archiveEntryInfo->mode             = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_SPECIAL;

  // init special chunk
  error = Chunk_init(&archiveEntryInfo->special.chunkSpecial.info,
                     NULL,  // parentChunkInfo
                     archiveInfo->chunkIO,
                     archiveInfo->chunkIOUserData,
                     CHUNK_ID_SPECIAL,
                     CHUNK_DEFINITION_SPECIAL,
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->special.chunkSpecial
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->special.chunkSpecial.info,{ Chunk_done(&archiveEntryInfo->special.chunkSpecial.info); });

  // find next special chunk
  do
  {
    error = getNextChunkHeader(archiveInfo,&chunkHeader);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList1);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_SPECIAL)
    {
      if (error != ERROR_NONE)
      {
        archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
        AutoFree_cleanup(&autoFreeList1);
        return error;
      }
      archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_SPECIAL);

  // read special chunk
  error = Chunk_open(&archiveEntryInfo->special.chunkSpecial.info,
                     &chunkHeader,
                     Chunk_getSize(&archiveEntryInfo->special.chunkSpecial.info,NULL,0)
                    );
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->special.chunkSpecial.info,{ Chunk_close(&archiveEntryInfo->special.chunkSpecial.info); });
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->special.chunkSpecial.cryptAlgorithm))
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
  archiveEntryInfo->cryptAlgorithm = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->special.chunkSpecial.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  // try to read special entry with all passwords
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->special.chunkSpecial.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm))
  {
    if (archiveInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveInfo->cryptPassword != NULL);
      password = archiveInfo->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveInfo,
                                         archiveInfo->jobOptions,
                                         archiveInfo->jobOptions->cryptPasswordMode,
                                         archiveInfo->getPasswordFunction,
                                         archiveInfo->getPasswordUserData
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
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.cryptInfo,
                         archiveEntryInfo->cryptAlgorithm,
                         password
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->special.chunkSpecialExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.cryptInfo); });
      }
    }

    // init special entry chunk
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->special.chunkSpecialEntry.info,
                         &archiveEntryInfo->special.chunkSpecial.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_SPECIAL_ENTRY,
                         CHUNK_DEFINITION_SPECIAL_ENTRY,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->special.chunkSpecialEntry.cryptInfo,
                         &archiveEntryInfo->special.chunkSpecialEntry
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->special.chunkSpecialEntry.info,{ Chunk_done(&archiveEntryInfo->special.chunkSpecialEntry.info); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.info,
                         &archiveEntryInfo->special.chunkSpecial.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_SPECIAL_EXTENDED_ATTRIBUTE,
                         CHUNK_DEFINITION_SPECIAL_EXTENDED_ATTRIBUTE,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->special.chunkSpecialExtendedAttribute.cryptInfo,
                         &archiveEntryInfo->special.chunkSpecialExtendedAttribute
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->special.chunkSpecialExtendedAttribute.info,{ Chunk_done(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.info); });
      }
    }

    // read special entry
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveEntryInfo->special.chunkSpecial.info)
             && (error == ERROR_NONE)
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
              memset(fileInfo,0,sizeof(FileInfo));
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
          case CHUNK_ID_SPECIAL_EXTENDED_ATTRIBUTE:
            {
              if (fileExtendedAttributeList != NULL)
              {
                // read special extended attribute chunk
                error = Chunk_open(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.info,
                                   &subChunkHeader,
                                   subChunkHeader.size
                                  );
                if (error != ERROR_NONE)
                {
                  break;
                }

                // add extended attribute to list
                File_addExtendedAttribute(fileExtendedAttributeList,
                                          archiveEntryInfo->special.chunkSpecialExtendedAttribute.name,
                                          archiveEntryInfo->special.chunkSpecialExtendedAttribute.value.data,
                                          archiveEntryInfo->special.chunkSpecialExtendedAttribute.value.length
                                         );

                // close special extended attribute chunk
                error = Chunk_close(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.info);
                if (error != ERROR_NONE)
                {
                  break;
                }
              }
              else
              {
                // skip file extended attribute chunk
                error = Chunk_skipSub(&archiveEntryInfo->special.chunkSpecial.info,&subChunkHeader);
                if (error != ERROR_NONE)
                {
                  break;
                }
              }
            }
            break;
          default:
            // unknown sub-chunk -> skip
            if (globalOptions.verboseLevel >= 3)
            {
              printWarning("Skipped unknown sub-chunk '%s' (offset %llu).\n",
                           Chunk_idToString(subChunkHeader.id),
                           subChunkHeader.offset,
                           String_cString(archiveInfo->printableStorageName)
                          );
            }
            error = Chunk_skipSub(&archiveEntryInfo->special.chunkSpecial.info,&subChunkHeader);
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
      if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && (archiveInfo->cryptType != CRYPT_TYPE_ASYMMETRIC))
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
  }
  while ((error != ERROR_NONE) && (password != NULL));
  AUTOFREE_ADD(&autoFreeList1,&autoFreeList2,{ AutoFree_cleanup(&autoFreeList2); });
  if (error != ERROR_NONE)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  if (!foundSpecialEntryFlag)
  {
    archiveInfo->pendingError = Chunk_skip(archiveInfo->chunkIO,archiveInfo->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    if      (error != ERROR_NONE)                                                  return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                       return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundSpecialEntryFlag)                                               return ERROR_NO_SPECIAL_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveEntryInfo->cryptAlgorithm;
  if (cryptType      != NULL) (*cryptType)      = archiveInfo->cryptType;

  // done resources
  AutoFree_done(&autoFreeList2);
  AutoFree_done(&autoFreeList1);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_closeEntry(ArchiveEntryInfo *archiveEntryInfo)
#else /* not NDEBUG */
  Errors __Archive_closeEntry(const char       *__fileName__,
                              ulong            __lineNb__,
                              ArchiveEntryInfo *archiveEntryInfo
                             )
#endif /* NDEBUG */
{
  Errors        error,tmpError;
  bool          eofDelta;
  ulong         deltaLength;
  SemaphoreLock semaphoreLock;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(archiveEntryInfo->archiveInfo->jobOptions != NULL);

  #ifndef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACE(archiveEntryInfo,sizeof(ArchiveEntryInfo));
  #endif /* NDEBUG */

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
                    // get next delta-compressed data
                    Compress_getCompressedData(&archiveEntryInfo->file.deltaCompressInfo,
                                               archiveEntryInfo->file.deltaBuffer,
                                               archiveEntryInfo->file.deltaBufferSize,
                                               &deltaLength
                                              );
                    if (deltaLength > 0)
                    {
                      // byte-compress data
                      error = Compress_deflate(&archiveEntryInfo->file.byteCompressInfo,
                                               archiveEntryInfo->file.deltaBuffer,
                                               deltaLength,
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

              // flush byte compress
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
                tmpError = Chunk_close(&archiveEntryInfo->file.chunkFileEntry.info);
                if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
                tmpError = Chunk_close(&archiveEntryInfo->file.chunkFile.info);
                if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              }

              // transfer to archive
              SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveEntryInfo->archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
              {
                // create archive file
                tmpError = createArchiveFile(archiveEntryInfo->archiveInfo);
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
                  break;
                }

                // transfer intermediate data into archive
                tmpError = transferArchiveFileData(archiveEntryInfo->archiveInfo,
                                                   &archiveEntryInfo->file.intermediateFileHandle
                                                  );
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
                  break;
                }
              }

              if (   (archiveEntryInfo->archiveInfo->indexHandle != NULL)
                  && (archiveEntryInfo->archiveInfo->storageId != DATABASE_ID_NONE)
                 )
              {
                // store in index database
                if (error == ERROR_NONE)
                {
                  error = Index_addFile(archiveEntryInfo->archiveInfo->indexHandle,
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
            Compress_done(&archiveEntryInfo->file.byteCompressInfo);
            Compress_done(&archiveEntryInfo->file.deltaCompressInfo);

            Chunk_done(&archiveEntryInfo->file.chunkFileData.info);
            Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info);
            Chunk_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.info);
            Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);

            Crypt_done(&archiveEntryInfo->file.cryptInfo);
            Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
            Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
            Crypt_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo);
            Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);

            Chunk_done(&archiveEntryInfo->file.chunkFile.info);

            if (archiveEntryInfo->file.deltaSourceHandleInitFlag)
            {
              assert(Compress_isXDeltaCompressed(archiveEntryInfo->file.deltaCompressAlgorithm));
              DeltaSource_closeEntry(&archiveEntryInfo->file.deltaSourceHandle);
            }

            free(archiveEntryInfo->file.deltaBuffer);
            free(archiveEntryInfo->file.byteBuffer);

            (void)File_close(&archiveEntryInfo->file.intermediateFileHandle);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
            {
              // flush delta compress
              error = Compress_flush(&archiveEntryInfo->image.deltaCompressInfo);
              if (error == ERROR_NONE)
              {
                eofDelta = FALSE;
                do
                {
                  // flush compressed full data blocks
                  error = flushImageDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_FULL);
                  if (error == ERROR_NONE)
                  {
                    // get next delta-compressed data
                    Compress_getCompressedData(&archiveEntryInfo->image.deltaCompressInfo,
                                               archiveEntryInfo->image.deltaBuffer,
                                               archiveEntryInfo->image.deltaBufferSize,
                                               &deltaLength
                                              );
                    if (deltaLength > 0)
                    {
                      // byte-compress data
                      error = Compress_deflate(&archiveEntryInfo->image.byteCompressInfo,
                                               archiveEntryInfo->image.deltaBuffer,
                                               deltaLength,
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

              // flush byte compress
              if (error == ERROR_NONE)
              {
                error = Compress_flush(&archiveEntryInfo->image.byteCompressInfo);
              }
              if (error == ERROR_NONE)
              {
                error = flushImageDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_ANY);
              }

              // update file and chunks if header is written
              if (archiveEntryInfo->image.headerWrittenFlag)
              {
                // update part block count
                assert(archiveEntryInfo->image.blockSize > 0);
                assert((Compress_getInputLength(&archiveEntryInfo->image.deltaCompressInfo) % archiveEntryInfo->image.blockSize) == 0);
                archiveEntryInfo->image.chunkImageData.blockCount = Compress_getInputLength(&archiveEntryInfo->image.deltaCompressInfo)/archiveEntryInfo->image.blockSize;
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

              // transfer to archive
              SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveEntryInfo->archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
              {
                // create archive file if needed
                tmpError = createArchiveFile(archiveEntryInfo->archiveInfo);
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
                  break;
                }

                // transfer intermediate data into archive
                tmpError = transferArchiveFileData(archiveEntryInfo->archiveInfo,
                                                   &archiveEntryInfo->image.intermediateFileHandle
                                                  );
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
                  break;
                }
              }

              if (   (archiveEntryInfo->archiveInfo->indexHandle != NULL)
                  && (archiveEntryInfo->archiveInfo->storageId != DATABASE_ID_NONE)
                 )
              {
                // store in index database
                if (error == ERROR_NONE)
                {
                  error = Index_addImage(archiveEntryInfo->archiveInfo->indexHandle,
                                         archiveEntryInfo->archiveInfo->storageId,
                                         archiveEntryInfo->image.chunkImageEntry.name,
                                         archiveEntryInfo->image.chunkImageEntry.fileSystemType,
                                         archiveEntryInfo->image.chunkImageEntry.size,
                                         archiveEntryInfo->image.chunkImageEntry.blockSize,
                                         archiveEntryInfo->image.chunkImageData.blockOffset,
                                         archiveEntryInfo->image.chunkImageData.blockCount
                                        );
                }
              }
            }

            // free resources
            Compress_done(&archiveEntryInfo->image.byteCompressInfo);
            Compress_done(&archiveEntryInfo->image.deltaCompressInfo);

            Chunk_done(&archiveEntryInfo->image.chunkImageData.info);
            Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info);
            Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);

            Crypt_done(&archiveEntryInfo->image.cryptInfo);
            Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
            Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
            Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);

            Chunk_done(&archiveEntryInfo->image.chunkImage.info);

            if (archiveEntryInfo->image.deltaSourceHandleInitFlag)
            {
              assert(Compress_isXDeltaCompressed(archiveEntryInfo->image.deltaCompressAlgorithm));
              DeltaSource_closeEntry(&archiveEntryInfo->image.deltaSourceHandle);
            }

            free(archiveEntryInfo->image.deltaBuffer);
            free(archiveEntryInfo->image.byteBuffer);

            File_close(&archiveEntryInfo->image.intermediateFileHandle);
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

              // unlock archive
              Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);

              if (   (archiveEntryInfo->archiveInfo->indexHandle != NULL)
                  && (archiveEntryInfo->archiveInfo->storageId != DATABASE_ID_NONE)
                 )
              {
                // store in index database
                if (error == ERROR_NONE)
                {
                  error = Index_addDirectory(archiveEntryInfo->archiveInfo->indexHandle,
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
            Chunk_done(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.info);
            Chunk_done(&archiveEntryInfo->directory.chunkDirectoryEntry.info);

            Crypt_done(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.cryptInfo);
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

              // unlock archive
              Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);

              if (   (archiveEntryInfo->archiveInfo->indexHandle != NULL)
                  && (archiveEntryInfo->archiveInfo->storageId != DATABASE_ID_NONE)
                 )
              {
                // store in index database
                if (error == ERROR_NONE)
                {
                  error = Index_addLink(archiveEntryInfo->archiveInfo->indexHandle,
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
            Crypt_done(&archiveEntryInfo->link.chunkLinkExtendedAttribute.cryptInfo);
            Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo);

            Chunk_done(&archiveEntryInfo->link.chunkLinkExtendedAttribute.info);
            Chunk_done(&archiveEntryInfo->link.chunkLinkEntry.info);

            Chunk_done(&archiveEntryInfo->link.chunkLink.info);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          {
            StringNode *stringNode;
            String     fileName;

            if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
            {
              // flush delta compress
              error = Compress_flush(&archiveEntryInfo->hardLink.deltaCompressInfo);
              if (error == ERROR_NONE)
              {
                eofDelta = FALSE;
                do
                {
                  // flush compressed full data blocks
                  error = flushHardLinkDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_FULL);
                  if (error == ERROR_NONE)
                  {
                    // get next delta-compressed data
                    Compress_getCompressedData(&archiveEntryInfo->hardLink.deltaCompressInfo,
                                               archiveEntryInfo->hardLink.deltaBuffer,
                                               archiveEntryInfo->hardLink.deltaBufferSize,
                                               &deltaLength
                                              );
                    if (deltaLength > 0)
                    {
                      // byte-compress data
                      error = Compress_deflate(&archiveEntryInfo->hardLink.byteCompressInfo,
                                               archiveEntryInfo->hardLink.deltaBuffer,
                                               deltaLength,
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

              // flush byte compress
              if (error == ERROR_NONE)
              {
                error = Compress_flush(&archiveEntryInfo->hardLink.byteCompressInfo);
              }
              if (error == ERROR_NONE)
              {
                error = flushHardLinkDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_ANY);
              }

              // update file and chunks if header is written
              if (archiveEntryInfo->hardLink.headerWrittenFlag)
              {
                // update part size
                archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize = Compress_getInputLength(&archiveEntryInfo->hardLink.deltaCompressInfo);
                if (error == ERROR_NONE)
                {
                  tmpError = Chunk_update(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
                  if (tmpError != ERROR_NONE) error = tmpError;
                }

                // close chunks
                tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
                if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
                tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
                if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
                tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLink.info);
                if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              }

              // transfer to archive
              SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveEntryInfo->archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
              {
                // create archive file if needed
                tmpError = createArchiveFile(archiveEntryInfo->archiveInfo);
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
                  break;
                }

                // transfer intermediate data into archive
                tmpError = transferArchiveFileData(archiveEntryInfo->archiveInfo,
                                                   &archiveEntryInfo->hardLink.intermediateFileHandle
                                                  );
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);
                  break;
                }
              }

              if (   (archiveEntryInfo->archiveInfo->indexHandle != NULL)
                  && (archiveEntryInfo->archiveInfo->storageId != DATABASE_ID_NONE)
                 )
              {
                // store in index database
                if (error == ERROR_NONE)
                {
                  STRINGLIST_ITERATE(archiveEntryInfo->hardLink.fileNameList,stringNode,fileName)
                  {
                    error = Index_addFile(archiveEntryInfo->archiveInfo->indexHandle,
                                          archiveEntryInfo->archiveInfo->storageId,
                                          fileName,
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
            Compress_done(&archiveEntryInfo->hardLink.byteCompressInfo);
            Compress_done(&archiveEntryInfo->hardLink.deltaCompressInfo);

            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkName.info);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.info);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);

            Crypt_done(&archiveEntryInfo->hardLink.cryptInfo);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);

            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info);

            if (Compress_isCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm))
            {
              assert(Compress_isXDeltaCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm));
              DeltaSource_closeEntry(&archiveEntryInfo->hardLink.deltaSourceHandle);
            }

            free(archiveEntryInfo->hardLink.deltaBuffer);
            free(archiveEntryInfo->hardLink.byteBuffer);

            (void)File_close(&archiveEntryInfo->hardLink.intermediateFileHandle);
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

              // unlock archive
              Semaphore_unlock(&archiveEntryInfo->archiveInfo->chunkIOLock);

              if (   (archiveEntryInfo->archiveInfo->indexHandle != NULL)
                  && (archiveEntryInfo->archiveInfo->storageId != DATABASE_ID_NONE)
                 )
              {
                // store in index database
                if (error == ERROR_NONE)
                {
                  error = Index_addSpecial(archiveEntryInfo->archiveInfo->indexHandle,
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
            Crypt_done(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.cryptInfo);
            Crypt_done(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo);

            Chunk_done(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.info);
            Chunk_done(&archiveEntryInfo->special.chunkSpecialEntry.info);

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
            tmpError = Chunk_close(&archiveEntryInfo->file.chunkFileEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->file.chunkFile.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            // free resources
            if (archiveEntryInfo->file.deltaSourceHandleInitFlag)
            {
              DeltaSource_closeEntry(&archiveEntryInfo->file.deltaSourceHandle);
            }

            Compress_done(&archiveEntryInfo->file.deltaCompressInfo);

            Chunk_done(&archiveEntryInfo->file.chunkFileData.info);
            Chunk_done(&archiveEntryInfo->file.chunkFileDelta.info);
            Chunk_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.info);
            Chunk_done(&archiveEntryInfo->file.chunkFileEntry.info);

            Crypt_done(&archiveEntryInfo->file.cryptInfo);
            Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo);
            Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo);
            Crypt_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo);
            Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo);

            Compress_done(&archiveEntryInfo->file.byteCompressInfo);

            free(archiveEntryInfo->file.deltaBuffer);
            free(archiveEntryInfo->file.byteBuffer);

            Chunk_done(&archiveEntryInfo->file.chunkFile.info);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            // close chunks
            tmpError = Chunk_close(&archiveEntryInfo->image.chunkImageData.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->image.chunkImageEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->image.chunkImage.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            // free resources
            if (archiveEntryInfo->image.deltaSourceHandleInitFlag)
            {
              DeltaSource_closeEntry(&archiveEntryInfo->image.deltaSourceHandle);
            }

            Compress_done(&archiveEntryInfo->image.deltaCompressInfo);

            Chunk_done(&archiveEntryInfo->image.chunkImageData.info);
            Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info);
            Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);

            Crypt_done(&archiveEntryInfo->image.cryptInfo);
            Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
            Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
            Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);

            Compress_done(&archiveEntryInfo->image.byteCompressInfo);

            free(archiveEntryInfo->image.deltaBuffer);
            free(archiveEntryInfo->image.byteBuffer);

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
            Chunk_done(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.info);
            Chunk_done(&archiveEntryInfo->directory.chunkDirectoryEntry.info);

            Crypt_done(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.cryptInfo);
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
            Chunk_done(&archiveEntryInfo->link.chunkLinkExtendedAttribute.info);
            Chunk_done(&archiveEntryInfo->link.chunkLinkEntry.info);

            Crypt_done(&archiveEntryInfo->link.chunkLinkExtendedAttribute.cryptInfo);
            Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo);

            Chunk_done(&archiveEntryInfo->link.chunkLink.info);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          {
            // close chunks
            tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLink.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            // free resources
            if (archiveEntryInfo->hardLink.deltaSourceHandleInitFlag)
            {
              DeltaSource_closeEntry(&archiveEntryInfo->hardLink.deltaSourceHandle);
            }

            Compress_done(&archiveEntryInfo->hardLink.deltaCompressInfo);

            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkName.info);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.info);
            Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);

            Crypt_done(&archiveEntryInfo->hardLink.cryptInfo);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo);
            Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo);

            Compress_done(&archiveEntryInfo->hardLink.byteCompressInfo);

            free(archiveEntryInfo->hardLink.deltaBuffer);
            free(archiveEntryInfo->hardLink.byteBuffer);

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
            Chunk_done(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.info);
            Chunk_done(&archiveEntryInfo->special.chunkSpecialEntry.info);

            Crypt_done(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.cryptInfo);
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

  if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
  {
    archiveEntryInfo->archiveInfo->entries++;
  }

  return error;
}

Errors Archive_writeData(ArchiveEntryInfo *archiveEntryInfo,
                         const void       *buffer,
                         ulong            length,
                         uint             elementSize
                        )
{
  const byte *p;
  ulong      writtenLength;
  Errors     error;
  ulong      dataBlockLength;
  ulong      writtenDataBlockLength;
  ulong      freeSpace;
  ulong      deflatedBytes;
  ulong      deltaLength;
  bool       allowNewPartFlag;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveInfo != NULL);
  assert(archiveEntryInfo->archiveInfo->jobOptions != NULL);
  assert(elementSize > 0);

  if (!archiveEntryInfo->archiveInfo->jobOptions->dryRunFlag)
  {
    p            = (const byte*)buffer;
    writtenLength = 0L;
    while (writtenLength < length)
    {
      /* get size of data block which must be written without splitting
         (make sure written data size is aligned at element size boundaries)
      */
      dataBlockLength = ALIGN(MIN(length-writtenLength,DATA_BLOCK_SIZE-(elementSize-1)),elementSize);
      assert(dataBlockLength > 0L);
      assert(dataBlockLength < MAX_BUFFER_SIZE);

      // write data block
      writtenDataBlockLength = 0L;
      switch (archiveEntryInfo->archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          assert((archiveEntryInfo->file.byteBufferSize%archiveEntryInfo->blockLength) == 0);

          while (writtenDataBlockLength < dataBlockLength)
          {
            // do compress (delta+byte)
            do
            {
              // check if there is space for delta-compressed data
              if (Compress_isFreeDataSpace(&archiveEntryInfo->file.deltaCompressInfo))
              {
                // do delta-compress data
                error = Compress_deflate(&archiveEntryInfo->file.deltaCompressInfo,
                                         p+writtenDataBlockLength,
                                         dataBlockLength-writtenDataBlockLength,
                                         &deflatedBytes
                                        );
                if (error != ERROR_NONE)
                {
                  return error;
                }
                writtenDataBlockLength += deflatedBytes;
              }

              // check if there is space for byte-compressed data
              freeSpace = Compress_getFreeDataSpace(&archiveEntryInfo->file.byteCompressInfo);
              if (freeSpace > 0L)
              {
                // get delta-compressed data
                Compress_getCompressedData(&archiveEntryInfo->file.deltaCompressInfo,
                                           archiveEntryInfo->file.deltaBuffer,
                                           MIN(freeSpace,archiveEntryInfo->file.deltaBufferSize),
                                           &deltaLength
                                          );

                // do byte-compress data
                error = Compress_deflate(&archiveEntryInfo->file.byteCompressInfo,
                                         archiveEntryInfo->file.deltaBuffer,
                                         deltaLength,
                                         NULL
                                        );
                if (error != ERROR_NONE)
                {
                  return error;
                }
              }
            }
            while (   (writtenDataBlockLength < dataBlockLength)
                   && (freeSpace > 0L)
                  );

            // check if new part is allowed: only when data block is completed
            allowNewPartFlag = (writtenDataBlockLength >= dataBlockLength);

            // write compressed data
            error = writeFileDataBlocks(archiveEntryInfo,allowNewPartFlag);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          assert((archiveEntryInfo->image.byteBufferSize%archiveEntryInfo->blockLength) == 0);

          while (writtenDataBlockLength < dataBlockLength)
          {
            // do compress (delta+byte)
            do
            {
              // check if there is space for delta-compressed data
              if (Compress_isFreeDataSpace(&archiveEntryInfo->image.deltaCompressInfo))
              {
                // do delta-compress data
                error = Compress_deflate(&archiveEntryInfo->image.deltaCompressInfo,
                                         p+writtenDataBlockLength,
                                         dataBlockLength-writtenDataBlockLength,
                                         &deflatedBytes
                                        );
                if (error != ERROR_NONE)
                {
                  return error;
                }
                writtenDataBlockLength += deflatedBytes;
              }

              // check if there is space for byte-compressed data
              freeSpace = Compress_getFreeDataSpace(&archiveEntryInfo->image.byteCompressInfo);
              if (freeSpace > 0L)
              {
                // get delta-compressed data
                Compress_getCompressedData(&archiveEntryInfo->image.deltaCompressInfo,
                                           archiveEntryInfo->image.deltaBuffer,
                                           MIN(freeSpace,archiveEntryInfo->image.deltaBufferSize),
                                           &deltaLength
                                          );

                // do byte-compress data
                error = Compress_deflate(&archiveEntryInfo->image.byteCompressInfo,
                                         archiveEntryInfo->image.deltaBuffer,
                                         deltaLength,
                                         NULL
                                        );
                if (error != ERROR_NONE)
                {
                  return error;
                }
              }
            }
            while (   (writtenDataBlockLength < dataBlockLength)
                   && (freeSpace > 0L)
                  );

            // check if compressed data blocks are available and can be encrypted and written to file
            allowNewPartFlag = ((elementSize <= 1) || (writtenDataBlockLength >= dataBlockLength));

            // write compressed data
            error = writeImageDataBlocks(archiveEntryInfo,allowNewPartFlag);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          assert((archiveEntryInfo->hardLink.byteBufferSize%archiveEntryInfo->blockLength) == 0);

          while (writtenDataBlockLength < dataBlockLength)
          {
            // do compress (delta+byte)
            do
            {
              // check if there is space for delta-compressed data
              if (Compress_isFreeDataSpace(&archiveEntryInfo->hardLink.deltaCompressInfo))
              {
                // do delta-compress data
                error = Compress_deflate(&archiveEntryInfo->hardLink.deltaCompressInfo,
                                         p+writtenDataBlockLength,
                                         dataBlockLength-writtenDataBlockLength,
                                         &deflatedBytes
                                        );
                if (error != ERROR_NONE)
                {
                  return error;
                }
                writtenDataBlockLength += deflatedBytes;
              }

              // check if there is space for byte-compressed data
              freeSpace = Compress_getFreeDataSpace(&archiveEntryInfo->hardLink.byteCompressInfo);
              if (freeSpace > 0L)
              {
                // get delta-compressed data
                Compress_getCompressedData(&archiveEntryInfo->hardLink.deltaCompressInfo,
                                           archiveEntryInfo->hardLink.deltaBuffer,
                                           MIN(freeSpace,archiveEntryInfo->hardLink.deltaBufferSize),
                                           &deltaLength
                                          );

                // do byte-compress data
                error = Compress_deflate(&archiveEntryInfo->hardLink.byteCompressInfo,
                                         archiveEntryInfo->hardLink.deltaBuffer,
                                         deltaLength,
                                         NULL
                                        );
                if (error != ERROR_NONE)
                {
                  return error;
                }
              }
            }
            while (   (writtenDataBlockLength < dataBlockLength)
                   && (freeSpace > 0L)
                  );

            // check if compressed data blocks are available and can be encrypted and written to file
            allowNewPartFlag = ((elementSize <= 1) || (writtenDataBlockLength >= dataBlockLength));

            // write compressed data
            error = writeHardLinkDataBlocks(archiveEntryInfo,allowNewPartFlag);
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
          break;
        default:
          HALT_INTERNAL_ERROR("write data not supported for entry type");
          break;
      }

      // next data
      p             += dataBlockLength;
      writtenLength += dataBlockLength;
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
  bool   deltaDecompressEmptyFlag,byteDecompressEmptyFlag;
  ulong  maxInflateBytes;
  ulong  inflatedBytes;

  assert(archiveEntryInfo != NULL);

  switch (archiveEntryInfo->archiveEntryType)
  {
    case ARCHIVE_ENTRY_TYPE_FILE:
      if (   Compress_isCompressed(archiveEntryInfo->file.deltaCompressAlgorithm)
          && !archiveEntryInfo->file.deltaSourceHandleInitFlag
         )
      {
        assert(Compress_isXDeltaCompressed(archiveEntryInfo->file.deltaCompressAlgorithm));

        // get source for delta-compression
        error = DeltaSource_openEntry(&archiveEntryInfo->file.deltaSourceHandle,
                                      archiveEntryInfo->archiveInfo->deltaSourceList,
                                      archiveEntryInfo->file.chunkFileDelta.name,
                                      archiveEntryInfo->file.chunkFileEntry.name,
                                      archiveEntryInfo->file.chunkFileDelta.size,
                                      archiveEntryInfo->archiveInfo->jobOptions
                                     );
        if (error != ERROR_NONE)
        {
          return error;
        }

        // set delta base-offset
        DeltaSource_setBaseOffset(&archiveEntryInfo->file.deltaSourceHandle,
                             archiveEntryInfo->file.chunkFileData.fragmentOffset
                            );

        archiveEntryInfo->file.deltaSourceHandleInitFlag = TRUE;
      }

      // read data: decrypt+decompress (delta+byte)
      p = (byte*)buffer;
      while (length > 0L)
      {
        // check if delta-decompressor is empty
#ifndef WERROR
#warning todo use getFreeDecompressSpace
#endif
        error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->file.deltaCompressInfo,
                                                       &availableBytes
                                                      );
        if (error != ERROR_NONE)
        {
          return error;
        }
        deltaDecompressEmptyFlag = (availableBytes <= 0L);

        if (deltaDecompressEmptyFlag)
        {
          // no data in delta-decompressor -> do byte-decompress and fill delta-decompressor
          if (!Compress_isEndOfData(&archiveEntryInfo->file.deltaCompressInfo))
          {
            // check if byte-decompressor is empty
            error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->file.byteCompressInfo,
                                                           &availableBytes
                                                          );
            if (error != ERROR_NONE)
            {
              return error;
            }
            byteDecompressEmptyFlag = (availableBytes <= 0L);

            if (byteDecompressEmptyFlag)
            {
              // no data in byte-decompressor -> read block and fill byte-decompressor
              if (!Compress_isEndOfData(&archiveEntryInfo->file.byteCompressInfo))
              {
                do
                {
                  error = readFileDataBlock(archiveEntryInfo);
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
                  byteDecompressEmptyFlag = (availableBytes <= 0L);
                }
                while (   byteDecompressEmptyFlag
                       && !Compress_isEndOfData(&archiveEntryInfo->file.byteCompressInfo)
                      );
              }
            }

            if (!byteDecompressEmptyFlag)
            {
              // decompress next byte-data into delta-buffer
              maxInflateBytes = MIN(Compress_getFreeCompressSpace(&archiveEntryInfo->file.deltaCompressInfo),
                                    archiveEntryInfo->file.deltaBufferSize
                                   );
              error = Compress_inflate(&archiveEntryInfo->file.byteCompressInfo,
                                       archiveEntryInfo->file.deltaBuffer,
                                       maxInflateBytes,
                                       &inflatedBytes
                                      );
//fprintf(stderr,"%s, %d: inflatedBytes=%d\n",__FILE__,__LINE__,inflatedBytes);
//dumpMemory(archiveEntryInfo->file.deltaBuffer,inflatedBytes);
              if (error != ERROR_NONE)
              {
                return error;
              }
              if (inflatedBytes > 0L)
              {
                // put bytes into delta-decompressor
                Compress_putCompressedData(&archiveEntryInfo->file.deltaCompressInfo,
                                           archiveEntryInfo->file.deltaBuffer,
                                           inflatedBytes
                                          );
              }
              else
              {
                // no data decompressed -> error in inflate
                return ERROR_INFLATE_FAIL;
              }
            }
            else
            {
              // no more bytes in byte-decompressor -> flush delta-decompressor
              error = Compress_flush(&archiveEntryInfo->file.deltaCompressInfo);
              if (error != ERROR_NONE)
              {
                return error;
              }
            }
          }
          else
          {
            // no more data in delta-compressor -> end of data
            return ERROR_END_OF_DATA;
          }
        }

        // decompress next delta-data into buffer
        error = Compress_inflate(&archiveEntryInfo->file.deltaCompressInfo,
                                 p,
                                 length,
                                 &inflatedBytes
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if      (inflatedBytes > 0L)
        {
          // got decompressed data
          p += inflatedBytes;
          length -= inflatedBytes;
        }
      }
      break;
    case ARCHIVE_ENTRY_TYPE_IMAGE:
      if (   Compress_isCompressed(archiveEntryInfo->image.deltaCompressAlgorithm)
          && !archiveEntryInfo->image.deltaSourceHandleInitFlag
         )
      {
        assert(Compress_isXDeltaCompressed(archiveEntryInfo->image.deltaCompressAlgorithm));

        // get source for delta-compression
        error = DeltaSource_openEntry(&archiveEntryInfo->image.deltaSourceHandle,
                                      archiveEntryInfo->archiveInfo->deltaSourceList,
                                      archiveEntryInfo->image.chunkImageDelta.name,
                                      archiveEntryInfo->image.chunkImageEntry.name,
                                      archiveEntryInfo->image.chunkImageDelta.size,
                                      archiveEntryInfo->archiveInfo->jobOptions
                                     );
        if (error != ERROR_NONE)
        {
          return error;
        }

        // set delta base-offset
        DeltaSource_setBaseOffset(&archiveEntryInfo->image.deltaSourceHandle,
                             archiveEntryInfo->image.chunkImageData.blockOffset*(uint64)archiveEntryInfo->image.blockSize
                            );

        archiveEntryInfo->image.deltaSourceHandleInitFlag = TRUE;
      }

      // read data: decrypt+decompress (delta+byte)
      p = (byte*)buffer;
      while (length > 0L)
      {
        // check if delta-decompressor is empty
        error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->image.deltaCompressInfo,
                                                       &availableBytes
                                                      );
        if (error != ERROR_NONE)
        {
          return error;
        }
        deltaDecompressEmptyFlag = (availableBytes <= 0L);

        if (deltaDecompressEmptyFlag)
        {
          // no data in delta-decompressor -> do byte-decompress and fill delta-decompressor
          if (!Compress_isEndOfData(&archiveEntryInfo->image.deltaCompressInfo))
          {
            // check if byte-decompressor is empty
            error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->image.byteCompressInfo,
                                                           &availableBytes
                                                          );
            if (error != ERROR_NONE)
            {
              return error;
            }
            byteDecompressEmptyFlag = (availableBytes <= 0L);

            if (byteDecompressEmptyFlag)
            {
              // no data in byte-decompressor -> read block and fill byte-decompressor
              if (!Compress_isEndOfData(&archiveEntryInfo->image.byteCompressInfo))
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
                  byteDecompressEmptyFlag = (availableBytes <= 0L);
//fprintf(stderr,"%s, %d: availableBytes=%d\n",__FILE__,__LINE__,availableBytes);
                }
                while (   byteDecompressEmptyFlag
                       && !Compress_isEndOfData(&archiveEntryInfo->image.byteCompressInfo)
                      );
              }
            }

//fprintf(stderr,"%s, %d: availableBytes=%d flushed=%d end=%d\n",__FILE__,__LINE__,availableBytes,Compress_isFlush(&archiveEntryInfo->image.byteCompressInfo),Compress_isEndOfData(&archiveEntryInfo->image.byteCompressInfo));
            if (!byteDecompressEmptyFlag)
            {
              // decompress next byte-data into delta buffer
              maxInflateBytes = MIN(Compress_getFreeCompressSpace(&archiveEntryInfo->image.deltaCompressInfo),
                                    archiveEntryInfo->image.deltaBufferSize
                                   );
              error = Compress_inflate(&archiveEntryInfo->image.byteCompressInfo,
                                       archiveEntryInfo->image.deltaBuffer,
                                       maxInflateBytes,
                                       &inflatedBytes
                                      );
              if (error != ERROR_NONE)
              {
                return error;
              }
              if (inflatedBytes > 0L)
              {
                // put bytes into delta-decompressor
                Compress_putCompressedData(&archiveEntryInfo->image.deltaCompressInfo,
                                           archiveEntryInfo->image.deltaBuffer,
                                           inflatedBytes
                                          );
              }
              else
              {
                // no data decompressed -> error in inflate
                return ERROR_INFLATE_FAIL;
              }
            }
            else
            {
              // no more bytes in byte-decompressor -> flush delta-decompressor
              error = Compress_flush(&archiveEntryInfo->image.deltaCompressInfo);
              if (error != ERROR_NONE)
              {
                return error;
              }
            }
          }
          else
          {
            // no more data in delta-compressor -> end of data
            return ERROR_END_OF_DATA;
          }
        }

        // decompress next delta-data into buffer
        error = Compress_inflate(&archiveEntryInfo->image.deltaCompressInfo,
                                 p,
                                 length,
                                 &inflatedBytes
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (inflatedBytes > 0L)
        {
          // got decompressed data
          p += inflatedBytes;
          length -= inflatedBytes;
        }
      }
      break;
    case ARCHIVE_ENTRY_TYPE_HARDLINK:
      if (   Compress_isCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm)
          && !archiveEntryInfo->hardLink.deltaSourceHandleInitFlag
         )
      {
        assert(Compress_isXDeltaCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm));

        // get source for delta-compression
        if (StringList_isEmpty(archiveEntryInfo->hardLink.fileNameList))
        {
          return ERROR_DELTA_SOURCE_NOT_FOUND;
        }
        error = DeltaSource_openEntry(&archiveEntryInfo->hardLink.deltaSourceHandle,
                                      archiveEntryInfo->archiveInfo->deltaSourceList,
                                      archiveEntryInfo->hardLink.chunkHardLinkDelta.name,
                                      StringList_first(archiveEntryInfo->hardLink.fileNameList,NULL),
                                      archiveEntryInfo->hardLink.chunkHardLinkDelta.size,
                                      archiveEntryInfo->archiveInfo->jobOptions
                                     );
        if (error != ERROR_NONE)
        {
          return error;
        }

        // set delta base-offset
        DeltaSource_setBaseOffset(&archiveEntryInfo->hardLink.deltaSourceHandle,
                             archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset
                            );

        archiveEntryInfo->hardLink.deltaSourceHandleInitFlag = TRUE;
      }

      // read data: decrypt+decompress (delta+byte)
      p = (byte*)buffer;
      while (length > 0L)
      {
        // check if delta-decompressor is empty
        error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->hardLink.deltaCompressInfo,
                                                       &availableBytes
                                                      );
        if (error != ERROR_NONE)
        {
          return error;
        }
        deltaDecompressEmptyFlag = (availableBytes <= 0L);

        if (deltaDecompressEmptyFlag)
        {
          // no data in delta-decompressor -> do byte-decompress and fill delta-decompressor
          if (!Compress_isEndOfData(&archiveEntryInfo->hardLink.deltaCompressInfo))
          {
            // check if byte-decompressor is empty
            error = Compress_getAvailableDecompressedBytes(&archiveEntryInfo->hardLink.byteCompressInfo,
                                                           &availableBytes
                                                          );
            if (error != ERROR_NONE)
            {
              return error;
            }
            byteDecompressEmptyFlag = (availableBytes <= 0L);

            if (byteDecompressEmptyFlag)
            {
              // no data in byte-decompressor -> read block and fill byte-decompressor
              if (!Compress_isEndOfData(&archiveEntryInfo->hardLink.byteCompressInfo))
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
                  byteDecompressEmptyFlag = (availableBytes <= 0L);
//fprintf(stderr,"%s, %d: availableBytes=%d\n",__FILE__,__LINE__,availableBytes);
                }
                while (   byteDecompressEmptyFlag
                       && !Compress_isEndOfData(&archiveEntryInfo->hardLink.byteCompressInfo)
                      );
              }
            }

//fprintf(stderr,"%s, %d: availableBytes=%d flushed=%d end=%d\n",__FILE__,__LINE__,availableBytes,Compress_isFlush(&archiveEntryInfo->hardLink.byteCompressInfo),Compress_isEndOfData(&archiveEntryInfo->hardLink.byteCompressInfo));
            if (!byteDecompressEmptyFlag)
            {
              // decompress next byte-data into delta buffer
              maxInflateBytes = MIN(Compress_getFreeCompressSpace(&archiveEntryInfo->hardLink.deltaCompressInfo),
                                    archiveEntryInfo->hardLink.deltaBufferSize
                                   );
              error = Compress_inflate(&archiveEntryInfo->hardLink.byteCompressInfo,
                                       archiveEntryInfo->hardLink.deltaBuffer,
                                       maxInflateBytes,
                                       &inflatedBytes
                                      );
              if (error != ERROR_NONE)
              {
                return error;
              }
              if (inflatedBytes > 0L)
              {
                // put bytes into delta-decompressor
                Compress_putCompressedData(&archiveEntryInfo->hardLink.deltaCompressInfo,
                                           archiveEntryInfo->hardLink.deltaBuffer,
                                           inflatedBytes
                                          );
              }
              else
              {
                // no data decompressed -> error in inflate
                return ERROR_INFLATE_FAIL;
              }
            }
            else
            {
              // no more bytes in byte-compressor -> flush delta-compressor
              error = Compress_flush(&archiveEntryInfo->hardLink.deltaCompressInfo);
              if (error != ERROR_NONE)
              {
                return error;
              }
            }
          }
          else
          {
            // no more data in delta-compressor -> end of data
            return ERROR_END_OF_DATA;
          }
        }

        // decompress next delta-data into buffer
        error = Compress_inflate(&archiveEntryInfo->hardLink.deltaCompressInfo,
                                 p,
                                 length,
                                 &inflatedBytes
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (inflatedBytes > 0L)
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
  SemaphoreLock semaphoreLock;
  uint64        offset;

  assert(archiveInfo != NULL);
  assert(archiveInfo->chunkIO != NULL);
  assert(archiveInfo->chunkIO->tell != NULL);
  assert(archiveInfo->jobOptions != NULL);

  offset = 0LL;
  if (!archiveInfo->jobOptions->dryRunFlag)
  {
    switch (archiveInfo->ioType)
    {
      case ARCHIVE_IO_TYPE_FILE:
        SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
        {
          if (archiveInfo->file.openFlag)
          {
            archiveInfo->chunkIO->tell(archiveInfo->chunkIOUserData,&offset);
          }
          else
          {
            offset = 0LL;
          }
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
  SemaphoreLock semaphoreLock;
  Errors        error;

  assert(archiveInfo != NULL);
  assert(archiveInfo->chunkIO != NULL);
  assert(archiveInfo->chunkIO->seek != NULL);

  error = ERROR_NONE;

  if (!archiveInfo->jobOptions->dryRunFlag)
  {
    switch (archiveInfo->ioType)
    {
      case ARCHIVE_IO_TYPE_FILE:
        SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
        {
          if (archiveInfo->file.openFlag)
          {
            error = archiveInfo->chunkIO->seek(archiveInfo->chunkIOUserData,offset);
          }
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
  SemaphoreLock semaphoreLock;
  uint64        size;

  assert(archiveInfo != NULL);
  assert(archiveInfo->chunkIO != NULL);
  assert(archiveInfo->chunkIO->getSize != NULL);
  assert(archiveInfo->jobOptions != NULL);

  size = 0LL;
  if (!archiveInfo->jobOptions->dryRunFlag)
  {
    switch (archiveInfo->ioType)
    {
      case ARCHIVE_IO_TYPE_FILE:
        SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveInfo->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
        {
          size = (archiveInfo->file.openFlag)
                   ? archiveInfo->chunkIO->getSize(archiveInfo->chunkIOUserData)
                   : 0LL;
        }
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

Errors Archive_addToIndex(IndexHandle      *indexHandle,
                          StorageHandle    *storageHandle,
                          ConstString      storageName,
                          IndexModes       indexMode,
                          const JobOptions *jobOptions,
                          uint64           *totalTimeLastChanged,
                          uint64           *totalEntries,
                          uint64           *totalSize,
                          LogHandle        *logHandle
                         )
{
  Errors  error;
  IndexId storageId;

  assert(indexHandle != NULL);
  assert(storageName != NULL);

  // create new index
  error = Index_newStorage(indexHandle,
                           DATABASE_ID_NONE, // entityId
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
  error = Archive_updateIndex(indexHandle,
                              storageId,
                              storageHandle,
                              storageName,
                              jobOptions,
                              totalTimeLastChanged,
                              totalEntries,
                              totalSize,
                              CALLBACK(NULL,NULL),
                              CALLBACK(NULL,NULL),
                              logHandle
                             );
  if (error != ERROR_NONE)
  {
    Archive_remIndex(indexHandle,storageId);
    return error;
  }

  return ERROR_NONE;
}

Errors Archive_updateIndex(IndexHandle                  *indexHandle,
                           IndexId                      storageId,
                           StorageHandle                *storageHandle,
                           ConstString                  storageName,
                           const JobOptions             *jobOptions,
                           uint64                       *totalTimeLastChanged,
                           uint64                       *totalEntries,
                           uint64                       *totalSize,
                           ArchivePauseCallbackFunction pauseCallback,
                           void                         *pauseUserData,
                           ArchiveAbortCallbackFunction abortCallback,
                           void                         *abortUserData,
                           LogHandle                    *logHandle
                          )
{
  #define TRANSACTION_NAME "ARCHIVE_INDEX"

  StorageSpecifier  storageSpecifier;
  String            printableStorageName;
  Errors            error;
  uint64            timeLastChanged;
  bool              abortedFlag,serverAllocationPendingFlag;
  ArchiveInfo       archiveInfo;
  ArchiveEntryInfo  archiveEntryInfo;
  ArchiveEntryTypes archiveEntryType;

  assert(indexHandle != NULL);
  assert(storageName != NULL);

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  printableStorageName = String_new();

  // get printable name (if possible)
  if (Storage_parseName(&storageSpecifier,storageName) == ERROR_NONE)
  {
    String_set(printableStorageName,Storage_getPrintableName(&storageSpecifier,NULL));
  }
  else
  {
    String_set(printableStorageName,storageName);
  }

  // open archive (Note optimization: try sftp for scp protocol, because sftp support seek()-operation)
  if (storageSpecifier.type == STORAGE_TYPE_SCP)
  {
    // try to open scp-storage first with sftp
    storageSpecifier.type = STORAGE_TYPE_SFTP;
    error = Archive_open(&archiveInfo,
                         storageHandle,
                         &storageSpecifier,
                         NULL,  // archive name
                         NULL,  // deltaSourceList
                         jobOptions,
                         CALLBACK(NULL,NULL),
                         logHandle
                        );

    if (error != ERROR_NONE)
    {
      // open scp-storage
      storageSpecifier.type = STORAGE_TYPE_SCP;
      error = Archive_open(&archiveInfo,
                           storageHandle,
                           &storageSpecifier,
                           NULL,  // archive name
                           NULL,  // deltaSourceList
                           jobOptions,
                           CALLBACK(NULL,NULL),
                           logHandle
                          );
    }
  }
  else
  {
    // open other storage types
    error = Archive_open(&archiveInfo,
                         storageHandle,
                         &storageSpecifier,
                         NULL,  // archive name
                         NULL,  // deltaSourceList
                         jobOptions,
                         CALLBACK(NULL,NULL),
                         logHandle
                        );
  }
  if (error != ERROR_NONE)
  {
    printInfo(4,"Failed to create index for '%s' (error: %s)\n",String_cString(printableStorageName),Error_getText(error));

    Index_setState(indexHandle,
                   storageId,
                   INDEX_STATE_ERROR,
                   0LL,
                   "%s (error code: %d)",
                   Error_getText(error),
                   Error_getCode(error)
                  );
    String_delete(printableStorageName);
    Storage_doneSpecifier(&storageSpecifier);
    return error;
  }

  // clear index
  error = Index_clearStorage(indexHandle,
                             storageId
                            );
  if (error != ERROR_NONE)
  {
    printInfo(4,"Failed to create index for '%s' (error: %s)\n",String_cString(printableStorageName),Error_getText(error));

    Archive_close(&archiveInfo);
    Index_setState(indexHandle,
                   storageId,
                   INDEX_STATE_ERROR,
                   0LL,
                   "%s (error code: %d)",
                   Error_getText(error),
                   Error_getCode(error)
                  );
    String_delete(printableStorageName);
    Storage_doneSpecifier(&storageSpecifier);
    return error;
  }

  // set state 'update'
  Index_setState(indexHandle,
                 storageId,
                 INDEX_STATE_UPDATE,
                 0LL,
                 NULL
                );

  // index archive contents
  printInfo(4,"Create index for '%s'\n",String_cString(printableStorageName));
  error = Index_beginTransaction(indexHandle,TRANSACTION_NAME);
  if (error != ERROR_NONE)
  {
    return error;
  }
  timeLastChanged             = 0LL;
  abortedFlag                 = (abortCallback != NULL) && abortCallback(abortUserData);
  serverAllocationPendingFlag = Storage_isServerAllocationPending(storageHandle);
  while (   !Archive_eof(&archiveInfo,FALSE)
         && (error == ERROR_NONE)
         && !abortedFlag
         && !serverAllocationPendingFlag
        )
  {
    // pause
    if ((pauseCallback != NULL) && pauseCallback(pauseUserData))
    {
#if 0
      // temporarly close storage
      error = Archive_storageInterrupt(&archiveInfo);
      if (error != ERROR_NONE)
      {
        break;
      }
#endif /* 0 */

      // wait
      while ((pauseCallback != NULL) && pauseCallback(pauseUserData))
      {
        Misc_udelay(5000*1000);
      }

      // reopen storage
#if 0
      error = Archive_storageContinue(&archiveInfo);
      if (error != ERROR_NONE)
      {
        break;
      }
#endif /* 0 */
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
            error = Archive_readFileEntry(&archiveEntryInfo,
                                          &archiveInfo,
                                          NULL,  // deltaCompressAlgorithm
                                          NULL,  // byteCompressAlgorithm
                                          NULL,  // cryptAlgorithm
                                          NULL,  // cryptType
                                          fileName,
                                          &fileInfo,
                                          NULL,  // fileExtendedAttributeList
                                          NULL,  // deltaSourceName
                                          NULL,  // deltaSourceSize
                                          &fragmentOffset,
                                          &fragmentSize
                                         );
            if (error != ERROR_NONE)
            {
              String_delete(fileName);
              break;
            }

            // add to index database
            error = Index_addFile(indexHandle,
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
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              break;
            }

            // save max. time last changed
            if (timeLastChanged < fileInfo.timeLastChanged) timeLastChanged = fileInfo.timeLastChanged;

            pprintInfo(4,"INDEX: ","Added file '%s', %lubytes to index for '%s'\n",String_cString(fileName),fileInfo.size,String_cString(printableStorageName));

            // close archive file, free resources
            (void)Archive_closeEntry(&archiveEntryInfo);
            String_delete(fileName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            String           imageName;
            ArchiveEntryInfo archiveEntryInfo;
            DeviceInfo       deviceInfo;
            FileSystemTypes  fileSystemType;
            uint64           blockOffset,blockCount;

            // open archive file
            imageName = String_new();
            error = Archive_readImageEntry(&archiveEntryInfo,
                                           &archiveInfo,
                                           NULL,  // deltaCompressAlgorithm
                                           NULL,  // byteCompressAlgorithm
                                           NULL,  // cryptAlgorithm
                                           NULL,  // cryptType
                                           imageName,
                                           &deviceInfo,
                                           &fileSystemType,
                                           NULL,  // deltaSourceName
                                           NULL,  // deltaSourceSize
                                           &blockOffset,
                                           &blockCount
                                          );
            if (error != ERROR_NONE)
            {
              String_delete(imageName);
              break;
            }

            // add to index database
            error = Index_addImage(indexHandle,
                                   storageId,
                                   imageName,
                                   fileSystemType,
                                   deviceInfo.size,
                                   deviceInfo.blockSize,
                                   blockOffset,
                                   blockCount
                                  );
            if (error != ERROR_NONE)
            {
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(imageName);
              break;
            }
            pprintInfo(4,"INDEX: ","Added image '%s', %lubytes to index for '%s'\n",String_cString(imageName),deviceInfo.size,String_cString(printableStorageName));

            // close archive file, free resources
            (void)Archive_closeEntry(&archiveEntryInfo);
            String_delete(imageName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          {
            String   directoryName;
            FileInfo fileInfo;

            // open archive directory
            directoryName = String_new();
            error = Archive_readDirectoryEntry(&archiveEntryInfo,
                                               &archiveInfo,
                                               NULL,  // cryptAlgorithm
                                               NULL,  // cryptType
                                               directoryName,
                                               &fileInfo,
                                               NULL   // fileExtendedAttributeList
                                              );
            if (error != ERROR_NONE)
            {
              String_delete(directoryName);
              break;
            }

            // add to index database
            error = Index_addDirectory(indexHandle,
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
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(directoryName);
              break;
            }

            // save max. time last changed
            if (timeLastChanged < fileInfo.timeLastChanged) timeLastChanged = fileInfo.timeLastChanged;

            pprintInfo(4,"INDEX: ","Added directory '%s' to index for '%s'\n",String_cString(directoryName),String_cString(printableStorageName));

            // close archive file, free resources
            (void)Archive_closeEntry(&archiveEntryInfo);
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
            error = Archive_readLinkEntry(&archiveEntryInfo,
                                          &archiveInfo,
                                          NULL,  // cryptAlgorithm
                                          NULL,  // cryptType
                                          linkName,
                                          destinationName,
                                          &fileInfo,
                                          NULL   // fileExtendedAttributeList
                                         );
            if (error != ERROR_NONE)
            {
              String_delete(destinationName);
              String_delete(linkName);
              break;
            }

            // add to index database
            error = Index_addLink(indexHandle,
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
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(destinationName);
              String_delete(linkName);
              break;
            }

            // save max. time last changed
            if (timeLastChanged < fileInfo.timeLastChanged) timeLastChanged = fileInfo.timeLastChanged;

            pprintInfo(4,"INDEX: ","Added link '%s' to index for '%s'\n",String_cString(linkName),String_cString(printableStorageName));

            // close archive file, free resources
            (void)Archive_closeEntry(&archiveEntryInfo);
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
            error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                              &archiveInfo,
                                              NULL,  // deltaCompressAlgorithm
                                              NULL,  // byteCompressAlgorithm
                                              NULL,  // cryptAlgorithm
                                              NULL,  // cryptType
                                              &fileNameList,
                                              &fileInfo,
                                              NULL,  // fileExtendedAttributeList
                                              NULL,  // deltaSourceName
                                              NULL,  // deltaSourceSize
                                              &fragmentOffset,
                                              &fragmentSize
                                             );
            if (error != ERROR_NONE)
            {
              StringList_done(&fileNameList);
              break;
            }

            // add to index database
            STRINGLIST_ITERATE(&fileNameList,stringNode,fileName)
            {
              error = Index_addHardlink(indexHandle,
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
              (void)Archive_closeEntry(&archiveEntryInfo);
              StringList_done(&fileNameList);
              break;
            }

            // save max. time last changed
            if (timeLastChanged < fileInfo.timeLastChanged) timeLastChanged = fileInfo.timeLastChanged;

            pprintInfo(4,"INDEX: ","Added hardlink '%s', %lubytes to index for '%s'\n",String_cString(StringList_first(&fileNameList,NULL)),fileInfo.size,String_cString(printableStorageName));

            // close archive file, free resources
            (void)Archive_closeEntry(&archiveEntryInfo);
            StringList_done(&fileNameList);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          {
            String   fileName;
            FileInfo fileInfo;

            // open archive link
            fileName = String_new();
            error = Archive_readSpecialEntry(&archiveEntryInfo,
                                             &archiveInfo,
                                             NULL,  // cryptAlgorithm
                                             NULL,  // cryptType
                                             fileName,
                                             &fileInfo,
                                             NULL   // fileExtendedAttributeList
                                            );
            if (error != ERROR_NONE)
            {
              String_delete(fileName);
              break;
            }

            // add to index database
            error = Index_addSpecial(indexHandle,
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
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              break;
            }

            // save max. time last changed
            if (timeLastChanged < fileInfo.timeLastChanged) timeLastChanged = fileInfo.timeLastChanged;

            pprintInfo(4,"INDEX: ","Added special '%s' to index for '%s'\n",String_cString(fileName),String_cString(printableStorageName));

            // close archive file, free resources
            (void)Archive_closeEntry(&archiveEntryInfo);
            String_delete(fileName);
          }
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break; /* not reached */
      }

      // update temporary entries, size (ignore error)
      Index_storageUpdate(indexHandle,
                          storageId,
                          NULL,  // storageName
                          Archive_getSize(&archiveInfo)
                         );
    }

    // check if aborted, check if server allocation pending
    abortedFlag                 = (abortCallback != NULL) && abortCallback(abortUserData);
    serverAllocationPendingFlag = Storage_isServerAllocationPending(storageHandle);
  }
  if (error == ERROR_NONE)
  {
    error = Index_endTransaction(indexHandle,TRANSACTION_NAME);
  }
  else
  {
    (void)Index_rollbackTransaction(indexHandle,TRANSACTION_NAME);
  }
  if      (error != ERROR_NONE)
  {
    printInfo(4,"Failed to create index for '%s' (error: %s)\n",String_cString(printableStorageName),Error_getText(error));

    Index_setState(indexHandle,
                   storageId,
                   INDEX_STATE_ERROR,
                   0LL, // lastCheckedTimestamp
                   "%s (error code: %d)",
                   Error_getText(error),
                   Error_getCode(error)
                  );
  }
  else if (abortedFlag)
  {
    printInfo(4,"Aborted create index for '%s'\n",String_cString(printableStorageName));

    Index_setState(indexHandle,
                   storageId,
                   INDEX_STATE_ERROR,
                   0LL,
                   "aborted"
                  );

    error = ERROR_ABORTED;
  }
  else if (serverAllocationPendingFlag)
  {
    printInfo(4,"Interrupted create index for '%s'\n",String_cString(printableStorageName));

    Index_setState(indexHandle,
                   storageId,
                   INDEX_STATE_UPDATE_REQUESTED,
                   0LL,
                   NULL
                  );

    error = ERROR_INTERRUPTED;
  }
  else
  {
    printInfo(4,"Done create index for '%s'\n",String_cString(printableStorageName));

    // set index state 'OK', last checked time
    Index_setState(indexHandle,
                   storageId,
                   INDEX_STATE_OK,
                   Misc_getCurrentDateTime(),
                   NULL
                  );

    // update name/entries/size
    error = Index_storageUpdate(indexHandle,
                                storageId,
                                storageName,
                                Archive_getSize(&archiveInfo)
                               );
    if (error != ERROR_NONE)
    {
      Index_setState(indexHandle,
                     storageId,
                     INDEX_STATE_ERROR,
                     0LL, // lastCheckedTimestamp
                     "%s (error code: %d)",
                     Error_getText(error),
                     Error_getCode(error)
                    );
    }
    if (totalTimeLastChanged != NULL) (*totalTimeLastChanged) = timeLastChanged;
    if (totalEntries != NULL) (*totalEntries) = Archive_getEntries(&archiveInfo);
    if (totalSize != NULL) (*totalSize) = Archive_getSize(&archiveInfo);
  }

  // close archive
  Archive_close(&archiveInfo);

  // free resources
  String_delete(printableStorageName);
  Storage_doneSpecifier(&storageSpecifier);

  return error;

  #undef TRANSACTION_NAME
}

Errors Archive_remIndex(IndexHandle *indexHandle,
                        IndexId     storageId
                       )
{
  Errors error;

  assert(indexHandle != NULL);

  error = Index_deleteStorage(indexHandle,storageId);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

#if 0
Errors Archive_copy(ConstString         storageName,
                    JobOptions          *jobOptions,
                    getPasswordFunction archiveGetCryptPassword,
                    void                *archiveGetCryptPasswordData,
                    ConstString         newStorageName
                   )
{
  Errors      error;
  ArchiveInfo sourceArchiveInfo;
  ArchiveInfo destinationArchiveInfo;
  Errors      failError;

  // open source
  error = Archive_open(&sourceArchiveInfo,
                       &storageSpecifier,
//                        DeltaSourceList                 *deltaSourceList,
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
//                        DeltaSourceList                 *deltaSourceList,
                         jobOptions,
archiveInfo->archiveInitFunction              = NULL;
archiveInfo->archiveInitUserData              = NULL;
                         archiveStoreFunction          archiveStoreFunction,
                         void                            *archiveNewFileUserData,
                         CALLBACK(NULL,NULL)
                         NULL, // indexHandle
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
      Misc_udelay(500L*1000L);
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
          error = Archive_readFileEntry(&archiveEntryInfo,
                                        &archiveInfo,
                                        NULL,  // deltaCompressAlgorithm
                                        NULL,  // byteCompressAlgorithm
                                        NULL,  // cryptAlgorithm
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
              (void)Archive_closeEntry(&archiveEntryInfo);
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
                         Error_getText(error)
                        );
              File_close(&fileHandle);
              String_delete(destinationFileName);
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              if (!jobOptions->noStopOnErrorFlag)
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
              Misc_udelay(500L*1000L);
            }
#endif /* 0 */

            n = MIN(fragmentSize-length,BUFFER_SIZE);

            error = Archive_readData(&archiveEntryInfo,buffer,n);
            if (error != ERROR_NONE)
            {
              printInfo(2,"FAIL!\n");
              printError("Cannot read content of archive '%s' (error: %s)!\n",
                         String_cString(printableArchiveName),
                         Error_getText(error)
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
                         Error_getText(error)
                        );
              if (!jobOptions->noStopOnErrorFlag)
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
            (void)Archive_closeEntry(&archiveEntryInfo);
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
            (void)Archive_closeEntry(&archiveEntryInfo);
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
          error = Archive_readImageEntry(&archiveEntryInfo,
                                         &archiveInfo,
                                         NULL,
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
                       Error_getText(error)
                      );
            String_delete(imageName);
            if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
            break;
          }

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,imageName,PATTERN_MATCH_MODE_EXACT))
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
                (void)Archive_closeEntry(&archiveEntryInfo);
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
                           Error_getText(error)
                          );
                String_delete(destinationDeviceName);
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(imageName);
                if (!jobOptions->noStopOnErrorFlag)
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
                           Error_getText(error)
                          );
                Device_close(&deviceHandle);
                String_delete(destinationDeviceName);
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(imageName);
                if (!jobOptions->noStopOnErrorFlag)
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
                Misc_udelay(500L*1000L);
              }

              assert(deviceInfo.blockSize > 0);
              bufferBlockCount = MIN(blockCount-block,BUFFER_SIZE/deviceInfo.blockSize);

              error = Archive_readData(&archiveEntryInfo,buffer,bufferBlockCount*deviceInfo.blockSize);
              if (error != ERROR_NONE)
              {
                printInfo(2,"FAIL!\n");
                printError("Cannot read content of archive '%s' (error: %s)!\n",
                           String_cString(printableArchiveName),
                           Error_getText(error)
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
                             Error_getText(error)
                            );
                  if (!jobOptions->noStopOnErrorFlag)
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
                (void)Archive_closeEntry(&archiveEntryInfo);
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
            printWarning("close 'image' entry fail (error: %s)\n",Error_getText(error));
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
          error = Archive_readDirectoryEntry(&archiveEntryInfo,
                                             &archiveInfo,
                                             NULL,
                                             NULL,
                                             directoryName,
                                             &fileInfo
                                            );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'directory' content of archive '%s' (error: %s)!\n",
                       String_cString(printableArchiveName),
                       Error_getText(error)
                      );
            String_delete(directoryName);
            if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
            break;
          }

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
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
              (void)Archive_closeEntry(&archiveEntryInfo);
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
                           Error_getText(error)
                          );
                String_delete(destinationFileName);
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(directoryName);
                if (!jobOptions->noStopOnErrorFlag)
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
                if (!jobOptions->noStopOnErrorFlag)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot set directory info of '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Error_getText(error)
                            );
                  String_delete(destinationFileName);
                  (void)Archive_closeEntry(&archiveEntryInfo);
                  String_delete(directoryName);
                  if (!jobOptions->noStopOnErrorFlag)
                  {
                    restoreInfo.failError = error;
                  }
                  continue;
                }
                else
                {
                  printWarning("Cannot set directory info of '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Error_getText(error)
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
            printWarning("close 'directory' entry fail (error: %s)\n",Error_getText(error));
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
          error = Archive_readLinkEntry(&archiveEntryInfo,
                                        &archiveInfo,
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
                       Error_getText(error)
                      );
            String_delete(fileName);
            String_delete(linkName);
            if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
            break;
          }

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
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
                             Error_getText(error)
                            );
                  String_delete(parentDirectoryName);
                  String_delete(destinationFileName);
                  (void)Archive_closeEntry(&archiveEntryInfo);
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
                  if (!jobOptions->noStopOnErrorFlag)
                  {
                    printInfo(2,"FAIL!\n");
                    printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                               String_cString(parentDirectoryName),
                               Error_getText(error)
                              );
                    String_delete(parentDirectoryName);
                    String_delete(destinationFileName);
                    (void)Archive_closeEntry(&archiveEntryInfo);
                    String_delete(fileName);
                    if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
                    continue;
                  }
                  else
                  {
                    printWarning("Cannot set owner ship of directory '%s' (error: %s)\n",
                                 String_cString(parentDirectoryName),
                                 Error_getText(error)
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
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              String_delete(linkName);
              if (!jobOptions->noStopOnErrorFlag)
              {
                restoreInfo.failError = ERRORX(FILE_EXISTS_,0,String_cString(destinationFileName));
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
                           Error_getText(error)
                          );
                String_delete(destinationFileName);
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (!jobOptions->noStopOnErrorFlag)
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
                if (!jobOptions->noStopOnErrorFlag)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot set file info of '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Error_getText(error)
                            );
                  String_delete(destinationFileName);
                  (void)Archive_closeEntry(&archiveEntryInfo);
                  String_delete(fileName);
                  String_delete(linkName);
                  if (!jobOptions->noStopOnErrorFlag)
                  {
                    restoreInfo.failError = error;
                  }
                  continue;
                }
                else
                {
                  printWarning("Cannot set file info of '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Error_getText(error)
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
            printWarning("close 'link' entry fail (error: %s)\n",Error_getText(error));
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
          error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                            &archiveInfo,
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
                       Error_getText(error)
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
            if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
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
                               Error_getText(error)
                              );
                    String_delete(parentDirectoryName);
                    if (!jobOptions->noStopOnErrorFlag)
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
                    if (!jobOptions->noStopOnErrorFlag)
                    {
                      printInfo(2,"FAIL!\n");
                      printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                                 String_cString(parentDirectoryName),
                                 Error_getText(error)
                                );
                      String_delete(parentDirectoryName);
                      restoreInfo.failError = error;
                      break;
                    }
                    else
                    {
                      printWarning("Cannot set owner ship of directory '%s' (error: %s)\n",
                                   String_cString(parentDirectoryName),
                                   Error_getText(error)
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
                               Error_getText(error)
                              );
                    if (!jobOptions->noStopOnErrorFlag)
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
                               Error_getText(error)
                              );
                    File_close(&fileHandle);
                    if (!jobOptions->noStopOnErrorFlag)
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
                    Misc_udelay(500L*1000L);
                  }

                  n = MIN(fragmentSize-length,BUFFER_SIZE);

                  error = Archive_readData(&archiveEntryInfo,buffer,n);
                  if (error != ERROR_NONE)
                  {
                    printInfo(2,"FAIL!\n");
                    printError("Cannot read content of archive '%s' (error: %s)!\n",
                               String_cString(printableArchiveName),
                               Error_getText(error)
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
                                 Error_getText(error)
                                );
                      if (!jobOptions->noStopOnErrorFlag)
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
                    if (!jobOptions->noStopOnErrorFlag)
                    {
                      printInfo(2,"FAIL!\n");
                      printError("Cannot set file info of '%s' (error: %s)\n",
                                 String_cString(destinationFileName),
                                 Error_getText(error)
                                );
                      restoreInfo.failError = error;
                      break;
                    }
                    else
                    {
                      printWarning("Cannot set file info of '%s' (error: %s)\n",
                                   String_cString(destinationFileName),
                                   Error_getText(error)
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
                               Error_getText(error)
                              );
                    if (!jobOptions->noStopOnErrorFlag)
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
            (void)Archive_closeEntry(&archiveEntryInfo);
            StringList_done(&fileNameList);
            continue;
          }

          // close archive file, free resources
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printWarning("close 'hard link' entry fail (error: %s)\n",Error_getText(error));
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
          error = Archive_readSpecialEntry(&archiveEntryInfo,
                                           &archiveInfo,
                                           NULL,
                                           NULL,
                                           fileName,
                                           &fileInfo
                                          );
          if (error != ERROR_NONE)
          {
            printError("Cannot read 'special' content of archive '%s' (error: %s)!\n",
                       String_cString(printableArchiveName),
                       Error_getText(error)
                      );
            String_delete(fileName);
            if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
            break;
          }

          if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
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
                             Error_getText(error)
                            );
                  String_delete(parentDirectoryName);
                  String_delete(destinationFileName);
                  (void)Archive_closeEntry(&archiveEntryInfo);
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
                  if (!jobOptions->noStopOnErrorFlag)
                  {
                    printInfo(2,"FAIL!\n");
                    printError("Cannot set owner ship of directory '%s' (error: %s)\n",
                               String_cString(parentDirectoryName),
                               Error_getText(error)
                              );
                    String_delete(parentDirectoryName);
                    String_delete(destinationFileName);
                    (void)Archive_closeEntry(&archiveEntryInfo);
                    String_delete(fileName);
                    if (restoreInfo.failError == ERROR_NONE) restoreInfo.failError = error;
                    continue;
                  }
                  else
                  {
                    printWarning("Cannot set owner ship of directory '%s' (error: %s)\n",
                                 String_cString(parentDirectoryName),
                                 Error_getText(error)
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
              (void)Archive_closeEntry(&archiveEntryInfo);
              String_delete(fileName);
              if (!jobOptions->noStopOnErrorFlag)
              {
                restoreInfo.failError = ERRORX(FILE_EXISTS_,0,String_cString(destinationFileName));
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
                           Error_getText(error)
                          );
                String_delete(destinationFileName);
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                if (!jobOptions->noStopOnErrorFlag)
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
                if (!jobOptions->noStopOnErrorFlag)
                {
                  printInfo(2,"FAIL!\n");
                  printError("Cannot set file info of '%s' (error: %s)\n",
                             String_cString(destinationFileName),
                             Error_getText(error)
                            );
                  String_delete(destinationFileName);
                  (void)Archive_closeEntry(&archiveEntryInfo);
                  String_delete(fileName);
                  if (!jobOptions->noStopOnErrorFlag)
                  {
                    restoreInfo.failError = error;
                  }
                  continue;
                }
                else
                {
                  printWarning("Cannot set file info of '%s' (error: %s)\n",
                               String_cString(destinationFileName),
                               Error_getText(error)
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
            printWarning("close 'special' entry fail (error: %s)\n",Error_getText(error));
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
