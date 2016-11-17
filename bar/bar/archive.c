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
#include <string.h>
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

// i/o via storage functions
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
  ArchiveHandle       *archiveHandle;
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
* Input  : archiveHandle       - archive handle
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
                              ArchiveHandle       *archiveHandle,
                              const JobOptions    *jobOptions,
                              GetPasswordFunction getPasswordFunction,
                              void                *getPasswordUserData
                             )
{
  Errors error;

  assert(password != NULL);
  assert(archiveHandle != NULL);
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
        if (!archiveHandle->cryptPasswordReadFlag && (getPasswordFunction != NULL))
        {
          error = getPasswordFunction(NULL,  // loginName
                                      password,
                                      PASSWORD_TYPE_CRYPT,
                                      (archiveHandle->ioType == ARCHIVE_IO_TYPE_STORAGE_FILE)
                                        ? String_cString(Storage_getPrintableName(&archiveHandle->storage.storageSpecifier,NULL))
                                        : NULL,
                                      TRUE,
                                      TRUE,
                                      getPasswordUserData
                                     );
          archiveHandle->cryptPasswordReadFlag = TRUE;
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
        if (!archiveHandle->cryptPasswordReadFlag && (getPasswordFunction != NULL))
        {
          error = getPasswordFunction(NULL,  // loginName
                                      password,
                                      PASSWORD_TYPE_CRYPT,
                                      (archiveHandle->ioType == ARCHIVE_IO_TYPE_STORAGE_FILE)
                                        ? String_cString(Storage_getPrintableName(&archiveHandle->storage.storageSpecifier,NULL))
                                        : NULL,
                                      TRUE,
                                      TRUE,
                                      getPasswordUserData
                                     );
          archiveHandle->cryptPasswordReadFlag = TRUE;
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
        if (!archiveHandle->cryptPasswordReadFlag && (getPasswordFunction != NULL))
        {
          error = getPasswordFunction(NULL,  // loginName
                                      password,
                                      PASSWORD_TYPE_CRYPT,
                                      (archiveHandle->ioType == ARCHIVE_IO_TYPE_STORAGE_FILE)
                                        ? String_cString(Storage_getPrintableName(&archiveHandle->storage.storageSpecifier,NULL))
                                        : NULL,
                                      TRUE,
                                      TRUE,
                                      getPasswordUserData
                                     );
          archiveHandle->cryptPasswordReadFlag = TRUE;
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
  assert(passwordHandle->archiveHandle != NULL);

  password = NULL;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&passwordHandle->archiveHandle->passwordLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    while ((password == NULL) && (passwordHandle->passwordMode != PASSWORD_MODE_NONE))
    {
      if      (passwordHandle->passwordNode != NULL)
      {
        // get next password from list
        password = passwordHandle->passwordNode->password;
        passwordHandle->passwordNode = passwordHandle->passwordNode->next;
      }
      else
      {
        switch (passwordHandle->passwordMode)
        {
           case PASSWORD_MODE_NONE:
             break;
           case PASSWORD_MODE_CONFIG:
             password = passwordHandle->cryptPassword;

             // next password mode is: default
             passwordHandle->passwordMode = PASSWORD_MODE_DEFAULT;
             break;
           case PASSWORD_MODE_DEFAULT:
             password = globalOptions.cryptPassword;

             // next password mode is: ask
             passwordHandle->passwordMode = PASSWORD_MODE_ASK;
             break;
           case PASSWORD_MODE_ASK:
             if (passwordHandle->getPasswordFunction != NULL)
             {
               // input password
               Password_init(&newPassword);
               error = passwordHandle->getPasswordFunction(NULL,  // loginName
                                                           &newPassword,
                                                           PASSWORD_TYPE_CRYPT,
                                                           (passwordHandle->archiveHandle->ioType == ARCHIVE_IO_TYPE_STORAGE_FILE)
                                                             ? String_cString(Storage_getPrintableName(&passwordHandle->archiveHandle->storage.storageSpecifier,NULL))
                                                             : NULL,
                                                           FALSE,
                                                           FALSE,
                                                           passwordHandle->getPasswordUserData
                                                          );
               if (error == ERROR_NONE)
               {
                 // add to password list
                 password = Archive_appendDecryptPassword(&newPassword);
               }
               else
               {
                 // next password mode is: none
                 passwordHandle->passwordMode = PASSWORD_MODE_NONE;
               }
               Password_done(&newPassword);
             }
             else
             {
               // next password mode is: none
               passwordHandle->passwordMode = PASSWORD_MODE_NONE;
             }
             break;
           default:
             #ifndef NDEBUG
               HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
             #endif /* NDEBUG */
             break; /* not reached */
        }
      }
    }
  }

  return password;
}

/***********************************************************************\
* Name   : getFirstDecryptPassword
* Purpose: get first decrypt password
* Input  : archiveHandle       - archive handle
*          jobOptions          - job options
*          passwordMode        - password mode
*          getPasswordFunction - get password call-back
*          getPasswordUserData - user data for get password call-back
* Output : passwordHandle - intialized password handle
* Return : password or NULL if no more passwords
* Notes  : -
\***********************************************************************/

LOCAL const Password *getFirstDecryptPassword(PasswordHandle      *passwordHandle,
                                              ArchiveHandle       *archiveHandle,
                                              const JobOptions    *jobOptions,
                                              PasswordModes       passwordMode,
                                              GetPasswordFunction getPasswordFunction,
                                              void                *getPasswordUserData
                                             )
{
  assert(passwordHandle != NULL);

  passwordHandle->archiveHandle         = archiveHandle;
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
* Input  : archiveHandle - archive handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors initCryptPassword(ArchiveHandle *archiveHandle)
{
  SemaphoreLock semaphoreLock;
  Password      *cryptPassword;
  Errors        error;

  assert(archiveHandle != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveHandle->passwordLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (Crypt_isEncrypted(archiveHandle->jobOptions->cryptAlgorithm) && (archiveHandle->cryptPassword == NULL))
    {
      cryptPassword = Password_new();
      if (cryptPassword == NULL)
      {
        return ERROR_NO_CRYPT_PASSWORD;
      }
      error = getCryptPassword(cryptPassword,
                               archiveHandle,
                               archiveHandle->jobOptions,
                               archiveHandle->getPasswordFunction,
                               archiveHandle->getPasswordUserData
                              );
      if (error != ERROR_NONE)
      {
        Password_delete(cryptPassword);
        Semaphore_unlock(&archiveHandle->passwordLock);
        return error;
      }
      archiveHandle->cryptPassword = cryptPassword;
    }
  }

  return ERROR_NONE;
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : chunkHeaderEOF
* Purpose: check if chunk header end-of-file
* Input  : archiveHandle - archive handle
* Output : -
* Return : TRUE iff no more chunk header, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool chunkHeaderEOF(ArchiveHandle *archiveHandle)
{
  assert(archiveHandle != NULL);

  return    !archiveHandle->nextChunkHeaderReadFlag
         && Chunk_eof(archiveHandle->chunkIO,archiveHandle->chunkIOUserData);
}

/***********************************************************************\
* Name   : getNextChunkHeader
* Purpose: read next chunk header
* Input  : archiveHandle - archive handle
* Output : chunkHeader - read chunk header
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getNextChunkHeader(ArchiveHandle *archiveHandle, ChunkHeader *chunkHeader)
{
  Errors error;

  assert(archiveHandle != NULL);
  assert(chunkHeader != NULL);

  if (!archiveHandle->nextChunkHeaderReadFlag)
  {
    if (Chunk_eof(archiveHandle->chunkIO,archiveHandle->chunkIOUserData))
    {
      return ERROR_END_OF_ARCHIVE;
    }

    error = Chunk_next(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,chunkHeader);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  else
  {
    (*chunkHeader) = archiveHandle->nextChunkHeader;
    archiveHandle->nextChunkHeaderReadFlag = FALSE;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : ungetNextChunkHeader
* Purpose: restore chunk header for next read
* Input  : archiveHandle - archive handle
*          chunkHeader - read chunk header
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void ungetNextChunkHeader(ArchiveHandle *archiveHandle, ChunkHeader *chunkHeader)
{
  assert(archiveHandle != NULL);
  assert(chunkHeader != NULL);

  archiveHandle->nextChunkHeaderReadFlag = TRUE;
  archiveHandle->nextChunkHeader         = (*chunkHeader);
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : isSplittedArchive
* Purpose: check if archive should be splitted
* Input  : archiveHandle - archive handle
* Output : -
* Return : TRUE iff archive should be splitted
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isSplittedArchive(const ArchiveHandle *archiveHandle)
{
  assert(archiveHandle != NULL);

  return (archiveHandle->jobOptions->archivePartSize > 0LL);
}

/***********************************************************************\
* Name   : isNewPartNeeded
* Purpose: check if new archive part should be created
* Input  : archiveHandle - archive handle
*          minBytes    - additional space needed in archive file part
* Output : -
* Return : TRUE if new part should be created, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool isNewPartNeeded(const ArchiveHandle *archiveHandle,
                           ulong               minBytes
                          )
{
  bool   newPartFlag;
  uint64 archiveFileSize;

  assert(archiveHandle != NULL);
  assert(archiveHandle->chunkIO != NULL);
  assert(archiveHandle->chunkIO->getSize != NULL);
  assert(archiveHandle->jobOptions != NULL);
  assert(archiveHandle->ioType == ARCHIVE_IO_TYPE_FILE);
  assert(Semaphore_isOwned(&archiveHandle->chunkIOLock));

  newPartFlag = FALSE;
  if (isSplittedArchive(archiveHandle))
  {
    // get current archive size
    if (archiveHandle->file.openFlag)
    {
      archiveFileSize = archiveHandle->chunkIO->getSize(archiveHandle->chunkIOUserData);
    }
    else
    {
      archiveFileSize = 0LL;
    }
//fprintf(stderr,"%s, %d: archiveFileSize=%llu %lu %llu\n",__FILE__,__LINE__,archiveFileSize,minBytes,archiveHandle->archiveFileSize);

    if ((archiveHandle->archiveFileSize+archiveFileSize+minBytes) >= archiveHandle->jobOptions->archivePartSize)
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
* Input  : archiveHandle - archive handle
*          indexHandle - indexHandle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void findNextArchivePart(ArchiveHandle *archiveHandle, IndexHandle *indexHandle)
{
  uint64 storageSize;

  // find next suitable archive name
  if (   isSplittedArchive(archiveHandle)
      && (archiveHandle->jobOptions->archiveFileMode == ARCHIVE_FILE_MODE_APPEND)
     )
  {
    do
    {
      storageSize = archiveHandle->archiveGetSizeFunction(indexHandle,
                                                        archiveHandle->storageId,
                                                        archiveHandle->partNumber,
                                                        archiveHandle->archiveGetSizeUserData
                                                       );
      if (storageSize > archiveHandle->jobOptions->archivePartSize)
      {
        archiveHandle->partNumber++;
      }
    }
    while (storageSize > archiveHandle->jobOptions->archivePartSize);

    archiveHandle->archiveFileSize = storageSize;
  }
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : readEncryptionKey
* Purpose: read encryption key
* Input  : archiveHandle - archive handle
*          chunkHeader - key chunk header
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors readEncryptionKey(ArchiveHandle     *archiveHandle,
                               const ChunkHeader *chunkHeader
                              )
{
  Errors    error;
  ChunkInfo chunkInfoKey;
  ChunkKey  chunkKey;

  assert(archiveHandle != NULL);
  assert(chunkHeader != NULL);

  // init key chunk
  error = Chunk_init(&chunkInfoKey,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
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
  archiveHandle->cryptKeyDataLength = chunkHeader->size-Chunk_getSize(&chunkInfoKey,NULL,0);
  if (archiveHandle->cryptKeyData != NULL) free(archiveHandle->cryptKeyData);
  archiveHandle->cryptKeyData = malloc(archiveHandle->cryptKeyDataLength);
  if (archiveHandle->cryptKeyData == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  error = Chunk_readData(&chunkInfoKey,
                         archiveHandle->cryptKeyData,
                         archiveHandle->cryptKeyDataLength,
                         NULL
                        );
  if (error != ERROR_NONE)
  {
    free(archiveHandle->cryptKeyData); archiveHandle->cryptKeyData = NULL;
    Chunk_close(&chunkInfoKey);
    Chunk_done(&chunkInfoKey);
    return error;
  }

  // check if all data read
  if (!Chunk_eofSub(&chunkInfoKey))
  {
    free(archiveHandle->cryptKeyData); archiveHandle->cryptKeyData = NULL;
    Chunk_close(&chunkInfoKey);
    Chunk_done(&chunkInfoKey);
    return ERRORX_(CORRUPT_DATA,0,"%s",String_cString(archiveHandle->printableStorageName));
  }

  // close chunk
  error = Chunk_close(&chunkInfoKey);
  if (error != ERROR_NONE)
  {
    free(archiveHandle->cryptKeyData); archiveHandle->cryptKeyData = NULL;
    Chunk_done(&chunkInfoKey);
    return error;
  }
  Chunk_done(&chunkInfoKey);

  // get decrypt key
  if (archiveHandle->cryptPassword == NULL)
  {
    archiveHandle->cryptPassword = Password_new();
    if (archiveHandle->cryptPassword == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
  }
  error = Crypt_getDecryptKey(&archiveHandle->cryptKey,
                              archiveHandle->cryptKeyData,
                              archiveHandle->cryptKeyDataLength,
                              archiveHandle->cryptPassword
                             );
  if (error != ERROR_NONE)
  {
    Password_delete(archiveHandle->cryptPassword); archiveHandle->cryptPassword = NULL;
    free(archiveHandle->cryptKeyData); archiveHandle->cryptKeyData = NULL;
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeHeader
* Purpose: write archive header chunks
* Input  : archiveHandle - archive handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeHeader(ArchiveHandle *archiveHandle)
{
  Errors         error;
  ChunkInfo      chunkInfoBar;
  ChunkKey       chunkBar;
  ChunkMeta      chunkMeta;
  ChunkMetaEntry chunkMetaEntry;
  CryptInfo      cryptInfo;

  assert(archiveHandle != NULL);

  // init BAR chunk
  error = Chunk_init(&chunkInfoBar,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
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

  // init meta chunk
  error = Chunk_init(&chunkMeta.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_META,
                     CHUNK_DEFINITION_META,
                     0,  // alignment,
                     NULL,  // cryptInfo,
                     &chunkMeta
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkInfoBar);
    return error;
  }
  chunkMeta.cryptAlgorithm = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithm);
  memcpy(chunkMeta.salt,archiveHandle->cryptSalt,sizeof(chunkMeta.salt));

  // init crypt
  error = Crypt_init(&cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     archiveHandle->cryptSalt,
                     sizeof(chunkMeta.salt)
                    );
  if (error != ERROR_NONE)
  {
//    AutoFree_cleanup(&autoFreeList);
    Chunk_done(&chunkInfoBar);
    return error;
  }
//  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
//  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo); });

  // init meta entry chunk
  error = Chunk_init(&chunkMetaEntry.info,
                     &chunkMeta.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_META_ENTRY,
                     CHUNK_DEFINITION_META_ENTRY,
                     archiveHandle->blockLength,
                     &cryptInfo,
                     &chunkMetaEntry
                    );
  if (error != ERROR_NONE)
  {
    Crypt_done(&cryptInfo);
    Chunk_done(&chunkMeta.info);
    Chunk_done(&chunkInfoBar);
    return error;
  }
  Misc_getCurrentUserName(chunkMetaEntry.userName);
  Network_getHostName(chunkMetaEntry.hostName);
  String_set(chunkMetaEntry.jobUUID,archiveHandle->jobUUID);
  String_set(chunkMetaEntry.scheduleUUID,archiveHandle->scheduleUUID);
  chunkMetaEntry.archiveType     = archiveHandle->jobOptions->archiveType;
  chunkMetaEntry.createdDateTime = Misc_getCurrentDateTime();
  String_set(chunkMetaEntry.comment,archiveHandle->jobOptions->comment);

  // write header chunks
  error = Chunk_create(&chunkInfoBar);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkMetaEntry.info);
    Crypt_done(&cryptInfo);
    Chunk_done(&chunkMeta.info);
    Chunk_done(&chunkInfoBar);
    return error;
  }
  error = Chunk_create(&chunkMeta.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkMetaEntry.info);
    Crypt_done(&cryptInfo);
    Chunk_done(&chunkMeta.info);
    Chunk_done(&chunkInfoBar);
    return error;
  }
  error = Chunk_create(&chunkMetaEntry.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkMetaEntry.info);
    Crypt_done(&cryptInfo);
    Chunk_done(&chunkMeta.info);
    Chunk_done(&chunkInfoBar);
    return error;
  }

  // close chunks
  error = Chunk_close(&chunkMetaEntry.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkMetaEntry.info);
    Crypt_done(&cryptInfo);
    Chunk_done(&chunkMeta.info);
    Chunk_done(&chunkInfoBar);
    return error;
  }
  error = Chunk_close(&chunkMeta.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkMetaEntry.info);
    Crypt_done(&cryptInfo);
    Chunk_done(&chunkMeta.info);
    Chunk_done(&chunkInfoBar);
    return error;
  }
  error = Chunk_close(&chunkInfoBar);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkMetaEntry.info);
    Crypt_done(&cryptInfo);
    Chunk_done(&chunkMeta.info);
    Chunk_done(&chunkInfoBar);
    return error;
  }

  // free resources
  Chunk_done(&chunkMetaEntry.info);
  Crypt_done(&cryptInfo);
  Chunk_done(&chunkMeta.info);
  Chunk_done(&chunkInfoBar);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeEncryptionKey
* Purpose: write new encryption key chunk
* Input  : archiveHandle - archive handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeEncryptionKey(ArchiveHandle *archiveHandle)
{
  Errors    error;
  ChunkInfo chunkInfoKey;
  ChunkKey  chunkKey;

  assert(archiveHandle != NULL);
  assert(Semaphore_isOwned(&archiveHandle->chunkIOLock));

  // init key chunk
  error = Chunk_init(&chunkInfoKey,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
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
                          archiveHandle->cryptKeyData,
                          archiveHandle->cryptKeyDataLength
                         );
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkInfoKey);
    return error;
  }

  // close chunk
  error = Chunk_close(&chunkInfoKey);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkInfoKey);
    return error;
  }

  // free resources
  Chunk_done(&chunkInfoKey);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeSignature
* Purpose: write new signature chunk
* Input  : archiveHandle - archive handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeSignature(ArchiveHandle *archiveHandle)
{
  void           *hash;
  uint           hashLength;
  Errors         error;
  ChunkSignature chunkSignature;

  if (!archiveHandle->jobOptions->noSignatureFlag)
  {
    // get hash
    hashLength = Crypt_getHashLength(&archiveHandle->signatureHash);
    hash = malloc(hashLength);
    if (hash == NULL)
    {
      return ERROR_INSUFFICIENT_MEMORY;
    }
    Crypt_getHash(&archiveHandle->signatureHash,hash,hashLength);
#if 0
debugDumpMemory(hash,hashLength,0);
#endif

    // encrypt signature

#if 0
    // init crypt
    error = Crypt_init(&cryptInfo,
                       archiveHandle->jobOptions->cryptAlgorithm,
                       archiveHandle->cryptPassword,
                       archiveHandle->cryptSalt,
                       sizeof(chunkMeta.salt)
                      );
    if (error != ERROR_NONE)
    {
  //    AutoFree_cleanup(&autoFreeList);
      Chunk_done(&chunkInfoBar);
      return error;
    }
#endif

    // init signature chunk
    error = Chunk_init(&chunkSignature.info,
                       NULL,  // parentChunkInfo
                       archiveHandle->chunkIO,
                       archiveHandle->chunkIOUserData,
                       CHUNK_ID_SIGNATURE,
                       CHUNK_DEFINITION_SIGNATURE,
                       0,  // alignment
                       NULL,  // cryptInfo
                       &chunkSignature
                      );
    if (error != ERROR_NONE)
    {
      free(hash);
      return error;
    }
    chunkSignature.value.data   = hash;
    chunkSignature.value.length = hashLength;

    // write signature chunk
    error = Chunk_create(&chunkSignature.info);
    if (error != ERROR_NONE)
    {
      Chunk_done(&chunkSignature.info);
      free(hash);
      return error;
    }
    error = Chunk_close(&chunkSignature.info);
    if (error != ERROR_NONE)
    {
      Chunk_done(&chunkSignature.info);
      free(hash);
      return error;
    }

    // free resources
    Chunk_done(&chunkSignature.info);
    free(hash);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : createArchiveFile
* Purpose: create and open new archive file
* Input  : archiveHandle - archive handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createArchiveFile(ArchiveHandle *archiveHandle, IndexHandle *indexHandle)
{
  AutoFreeList  autoFreeList;
  SemaphoreLock semaphoreLock;
  Errors        error;

  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);
  assert(archiveHandle->ioType == ARCHIVE_IO_TYPE_FILE);

  if (!archiveHandle->file.openFlag)
  {
    // init variables
    AutoFree_init(&autoFreeList);

    SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      AUTOFREE_ADD(&autoFreeList,&archiveHandle->chunkIOLock,{ Semaphore_unlock(&archiveHandle->chunkIOLock); });

      // get intermediate data filename
      error = File_getTmpFileName(archiveHandle->file.fileName,"archive-XXXXXX",tmpDirectory);
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      AUTOFREE_ADD(&autoFreeList,&archiveHandle->file.fileName,{ File_delete(archiveHandle->file.fileName,FALSE); });
      DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

      // create file
      error = File_open(&archiveHandle->file.fileHandle,
                        archiveHandle->file.fileName,
                        FILE_OPEN_CREATE
                       );
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      AUTOFREE_ADD(&autoFreeList,&archiveHandle->file.fileHandle,{ File_close(&archiveHandle->file.fileHandle); });
      DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

      // write BAR header
      error = writeHeader(archiveHandle);
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

      // write encrypted key if asymmetric encryption enabled
      if (archiveHandle->cryptType == CRYPT_TYPE_ASYMMETRIC)
      {
        error = writeEncryptionKey(archiveHandle);
        if (error != ERROR_NONE)
        {
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
      }

      if (   (indexHandle != NULL)
          && !archiveHandle->jobOptions->noIndexDatabaseFlag
          && !archiveHandle->jobOptions->dryRunFlag
          && !archiveHandle->jobOptions->noStorageFlag
         )
      {
        // create storage index
        error = Index_newStorage(indexHandle,
                                 archiveHandle->entityId,
                                 NULL, // storageName
                                 INDEX_STATE_CREATE,
                                 INDEX_MODE_AUTO,
                                 &archiveHandle->storageId
                                );
        if (error != ERROR_NONE)
        {
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        AUTOFREE_ADD(&autoFreeList,&archiveHandle->storageId,{ Index_deleteStorage(indexHandle,archiveHandle->storageId); });
      }
      else
      {
        // no index
        archiveHandle->storageId = DATABASE_ID_NONE;
      }

      // call-back for init archive
      if (archiveHandle->archiveInitFunction != NULL)
      {
        error = archiveHandle->archiveInitFunction(indexHandle,
                                                 archiveHandle->uuidId,
                                                 archiveHandle->jobUUID,
                                                 archiveHandle->scheduleUUID,
                                                 archiveHandle->entityId,
                                                 archiveHandle->archiveType,
                                                 archiveHandle->storageId,
                                                 isSplittedArchive(archiveHandle)
                                                   ? (int)archiveHandle->partNumber
                                                   : ARCHIVE_PART_NUMBER_NONE,
                                                 archiveHandle->archiveInitUserData
                                                );
        if (error != ERROR_NONE)
        {
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
      }

      // mark archive file "open"
      archiveHandle->file.openFlag = TRUE;
    }

    // free resources
    AutoFree_done(&autoFreeList);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : closeArchiveFile
* Purpose: close archive file
* Input  : archiveHandle - archive handle
*          indexHandle   - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors closeArchiveFile(ArchiveHandle *archiveHandle, IndexHandle *indexHandle)
{
  SemaphoreLock semaphoreLock;
  Errors        error;

  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);
  assert(archiveHandle->ioType == ARCHIVE_IO_TYPE_FILE);
#ifndef WERROR
#warning TODO: nicht offen wenn keine datei archiviert wurde
#endif
//  assert(archiveHandle->file.openFlag);
if (!archiveHandle->file.openFlag) return ERROR_NONE;

  SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // add signature
    if (!archiveHandle->jobOptions->noSignatureFlag)
    {
      error = writeSignature(archiveHandle);
      if (error != ERROR_NONE)
      {
        Semaphore_unlock(&archiveHandle->chunkIOLock);
        return error;
      }
    }

    // close file
    (void)File_close(&archiveHandle->file.fileHandle);

    // mark archive file "closed"
    archiveHandle->file.openFlag = FALSE;

    // call-back to store archive
    if (archiveHandle->archiveStoreFunction != NULL)
    {
      error = archiveHandle->archiveStoreFunction(indexHandle,
                                                archiveHandle->uuidId,
                                                archiveHandle->jobUUID,
                                                archiveHandle->scheduleUUID,
                                                archiveHandle->entityId,
                                                archiveHandle->archiveType,
                                                archiveHandle->storageId,
                                                isSplittedArchive(archiveHandle)
                                                  ? (int)archiveHandle->partNumber
                                                  : ARCHIVE_PART_NUMBER_NONE,
                                                archiveHandle->file.fileName,
                                                Archive_getSize(archiveHandle),
                                                archiveHandle->archiveStoreUserData
                                               );
      if (error != ERROR_NONE)
      {
        Semaphore_unlock(&archiveHandle->chunkIOLock);
        return error;
      }
      DEBUG_TESTCODE() { Semaphore_unlock(&archiveHandle->chunkIOLock); return DEBUG_TESTCODE_ERROR(); }
    }

    // call-back for done archive
    if (archiveHandle->archiveDoneFunction != NULL)
    {
      error = archiveHandle->archiveDoneFunction(indexHandle,
                                               archiveHandle->uuidId,
                                               archiveHandle->jobUUID,
                                               archiveHandle->scheduleUUID,
                                               archiveHandle->entityId,
                                               archiveHandle->archiveType,
                                               archiveHandle->storageId,
                                               isSplittedArchive(archiveHandle)
                                                 ? (int)archiveHandle->partNumber
                                                 : ARCHIVE_PART_NUMBER_NONE,
                                               archiveHandle->archiveDoneUserData
                                              );
      if (error != ERROR_NONE)
      {
        Semaphore_unlock(&archiveHandle->chunkIOLock);
        return error;
      }
      DEBUG_TESTCODE() { Semaphore_unlock(&archiveHandle->chunkIOLock); return DEBUG_TESTCODE_ERROR(); }
    }

    // increment part number
    if (isSplittedArchive(archiveHandle))
    {
      archiveHandle->partNumber++;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : ensureArchiveSpace
* Purpose: ensure space is available in archive for not fragmented
*          writing
* Input  : archiveHandle - archive handle
*          minBytes      - minimal number of bytes
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors ensureArchiveSpace(ArchiveHandle *archiveHandle,
                                IndexHandle   *indexHandle,
                                ulong         minBytes
                               )
{
  Errors error;

  assert(archiveHandle != NULL);
  assert(archiveHandle->ioType == ARCHIVE_IO_TYPE_FILE);
  assert(Semaphore_isOwned(&archiveHandle->chunkIOLock));

  // check if split is necessary
  if (isNewPartNeeded(archiveHandle,
                      minBytes
                     )
     )
  {
    // split needed -> close archive file
    if (archiveHandle->file.openFlag)
    {
      error = closeArchiveFile(archiveHandle,indexHandle);
      if (error != ERROR_NONE)
      {
        return error;
      }

//TODO: write index entry?

    }
  }

  // create archive (if not already exists and open)
  error = createArchiveFile(archiveHandle,indexHandle);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : transferArchiveFileData
* Purpose: transfer file data from temporary file to archive, update
*          signature hash
* Input  : archiveHandle - archive handle
*          fileHandle    - file handle of temporary file
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors transferArchiveFileData(const ArchiveHandle *archiveHandle,
                                     FileHandle          *fileHandle
                                    )
{
  #define BUFFER_SIZE (1024*1024)

  void    *buffer;
  Errors  error;
  uint64  length;
  ulong   n;

  assert(archiveHandle != NULL);
  assert(archiveHandle->chunkIO != NULL);
  assert(archiveHandle->chunkIO->write != NULL);
  assert(fileHandle != NULL);
  assert(Semaphore_isOwned(&archiveHandle->chunkIOLock));

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

    // read data
    error = File_read(fileHandle,buffer,n,NULL);
    if (error != ERROR_NONE)
    {
      free(buffer);
      return error;
    }

    // update signature hash
    Crypt_updateHash(&archiveHandle->signatureHash,buffer,n);

    // transfer to archive
    error = archiveHandle->chunkIO->write(archiveHandle->chunkIOUserData,buffer,n);
    if (error != ERROR_NONE)
    {
      free(buffer);
      return error;
    }

    length -= (uint64)n;
  }

  // truncate file for reusage
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
  assert(archiveEntryInfo->archiveHandle != NULL);
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
  assert(archiveEntryInfo->archiveHandle != NULL);
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
  assert(archiveEntryInfo->archiveHandle != NULL);
  assert(archiveEntryInfo->archiveHandle->ioType == ARCHIVE_IO_TYPE_FILE);
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
      Semaphore_lock(&archiveEntryInfo->archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);

      // check if split is allowed and necessary
      newPartFlag =    allowNewPartFlag
                    && isNewPartNeeded(archiveEntryInfo->archiveHandle,
                                       (!archiveEntryInfo->file.headerWrittenFlag ? archiveEntryInfo->file.headerLength : 0) + minBytes
                                      );

      // split
      if (newPartFlag)
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
        assert(archiveEntryInfo->file.headerWrittenFlag);

        // write last compressed block (if any)
        if (byteLength > 0L)
        {
          // encrypt block
          error = Crypt_encryptBytes(&archiveEntryInfo->file.cryptInfo,
                                     archiveEntryInfo->file.byteBuffer,
                                     byteLength
                                    );
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
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
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
            return error;
          }
        }

        // flush delta compress
        error = Compress_flush(&archiveEntryInfo->file.deltaCompressInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }
        eofDelta = FALSE;
        do
        {
          // flush compressed full data blocks
          error = flushFileDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_FULL);
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
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
              Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
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
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }
        error = flushFileDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_ANY);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // update part size
        archiveEntryInfo->file.chunkFileData.fragmentSize = Compress_getInputLength(&archiveEntryInfo->file.deltaCompressInfo);
        error = Chunk_update(&archiveEntryInfo->file.chunkFileData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // close chunks
        error = Chunk_close(&archiveEntryInfo->file.chunkFileData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->file.chunkFileEntry.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->file.chunkFile.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // reset header "written"
        archiveEntryInfo->file.headerWrittenFlag = FALSE;

        // create archive file (if not already exists and open)
        error = createArchiveFile(archiveEntryInfo->archiveHandle,archiveEntryInfo->indexHandle);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // transfer intermediate data into archive
        error = transferArchiveFileData(archiveEntryInfo->archiveHandle,
                                        &archiveEntryInfo->file.intermediateFileHandle
                                       );
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // close archive
        error = closeArchiveFile(archiveEntryInfo->archiveHandle,archiveEntryInfo->indexHandle);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // store in index database
        if (   (archiveEntryInfo->indexHandle != NULL)
            && (archiveEntryInfo->archiveHandle->storageId != DATABASE_ID_NONE)
           )
        {
          error = Index_addFile(archiveEntryInfo->indexHandle,
                                archiveEntryInfo->archiveHandle->storageId,
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
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
            return error;
          }
        }

        // store fragment offset, count for next fragment
        archiveEntryInfo->file.chunkFileData.fragmentOffset += archiveEntryInfo->file.chunkFileData.fragmentSize;
        archiveEntryInfo->file.chunkFileData.fragmentSize   =  0LL;

        // set new delta base-offset
        if (archiveEntryInfo->file.deltaSourceHandleInitFlag)
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
        findNextArchivePart(archiveEntryInfo->archiveHandle,archiveEntryInfo->indexHandle);

        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
      }
      else
      {
        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);

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
* Input  : archiveEntryInfo - archive image entry info data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeImageChunks(ArchiveEntryInfo *archiveEntryInfo)
{
  Errors error;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveHandle != NULL);
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
  assert(archiveEntryInfo->archiveHandle != NULL);
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
        error = writeImageChunks(archiveEntryInfo);
        if (error != ERROR_NONE)
        {
          return error;
        }
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
  assert(archiveEntryInfo->archiveHandle != NULL);
  assert(archiveEntryInfo->archiveHandle->ioType == ARCHIVE_IO_TYPE_FILE);
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
      Semaphore_lock(&archiveEntryInfo->archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);

      // check if split is allowed and necessary
      newPartFlag =    allowNewPartFlag
                    && isNewPartNeeded(archiveEntryInfo->archiveHandle,
                                       (!archiveEntryInfo->image.headerWrittenFlag ? archiveEntryInfo->image.headerLength : 0) + minBytes
                                      );

      // split
      if (newPartFlag)
      {
        // create new part (if not already created)
        if (!archiveEntryInfo->image.headerWrittenFlag)
        {
          error = writeImageChunks(archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            return error;
          }
        }
        assert(archiveEntryInfo->hardLink.headerWrittenFlag);

        // write last compressed block (if any)
        if (byteLength > 0L)
        {
          // encrypt block
          error = Crypt_encryptBytes(&archiveEntryInfo->image.cryptInfo,
                                     archiveEntryInfo->image.byteBuffer,
                                     byteLength
                                    );
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
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
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
            return error;
          }
        }

        // flush delta compress
        error = Compress_flush(&archiveEntryInfo->image.deltaCompressInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }
        eofDelta = FALSE;
        do
        {
          // flush compressed full data blocks
          error = flushImageDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_FULL);
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
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
              Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
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
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }
        error = flushImageDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_ANY);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // update part size
        assert(archiveEntryInfo->image.blockSize > 0);
        archiveEntryInfo->image.chunkImageData.blockCount = Compress_getInputLength(&archiveEntryInfo->image.deltaCompressInfo)/archiveEntryInfo->image.blockSize;
        error = Chunk_update(&archiveEntryInfo->image.chunkImageData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // close chunks
        error = Chunk_close(&archiveEntryInfo->image.chunkImageData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->image.chunkImageEntry.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->image.chunkImage.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // reset header "written"
        archiveEntryInfo->image.headerWrittenFlag = FALSE;

        // mark header "not written"
        archiveEntryInfo->image.headerWrittenFlag = FALSE;

        // create archive file (if not already exists and open)
        error = createArchiveFile(archiveEntryInfo->archiveHandle,archiveEntryInfo->indexHandle);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // transfer intermediate data into archive
        error = transferArchiveFileData(archiveEntryInfo->archiveHandle,
                                        &archiveEntryInfo->image.intermediateFileHandle
                                       );
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // close archive
        error = closeArchiveFile(archiveEntryInfo->archiveHandle,archiveEntryInfo->indexHandle);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // store in index database
        if (   (archiveEntryInfo->indexHandle != NULL)
            && (archiveEntryInfo->archiveHandle->storageId != DATABASE_ID_NONE)
           )
        {
          error = Index_addImage(archiveEntryInfo->indexHandle,
                                 archiveEntryInfo->archiveHandle->storageId,
                                 archiveEntryInfo->image.chunkImageEntry.name,
                                 archiveEntryInfo->image.chunkImageEntry.fileSystemType,
                                 archiveEntryInfo->image.chunkImageEntry.size,
                                 archiveEntryInfo->image.chunkImageEntry.blockSize,
                                 archiveEntryInfo->image.chunkImageData.blockOffset,
                                 archiveEntryInfo->image.chunkImageData.blockCount
                                );
        }

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
        findNextArchivePart(archiveEntryInfo->archiveHandle,archiveEntryInfo->indexHandle);

        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
      }
      else
      {
        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);

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
  assert(archiveEntryInfo->archiveHandle != NULL);
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
  assert(archiveEntryInfo->archiveHandle != NULL);
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
  uint             blockCount;
  ulong            byteLength;
  ulong            minBytes;
  bool             newPartFlag;
  Errors           error;
  bool             eofDelta;
  ulong            deltaLength;
  const StringNode *stringNode;
  String           fileName;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveHandle != NULL);
  assert(archiveEntryInfo->archiveHandle->ioType == ARCHIVE_IO_TYPE_FILE);
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
      Semaphore_lock(&archiveEntryInfo->archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);

      // check if split is allowed and necessary
      newPartFlag =    allowNewPartFlag
                    && isNewPartNeeded(archiveEntryInfo->archiveHandle,
                                       (!archiveEntryInfo->hardLink.headerWrittenFlag ? archiveEntryInfo->hardLink.headerLength : 0) + minBytes
                                      );

      // split
      if (newPartFlag)
      {
        // create new part (if not already created)
        if (!archiveEntryInfo->hardLink.headerWrittenFlag)
        {
          error = writeHardLinkChunks(archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            return error;
          }
        }
        assert(archiveEntryInfo->hardLink.headerWrittenFlag);

        // write last compressed block (if any)
        if (byteLength > 0L)
        {
          // encrypt block
          error = Crypt_encryptBytes(&archiveEntryInfo->hardLink.cryptInfo,
                                     archiveEntryInfo->hardLink.byteBuffer,
                                     byteLength
                                    );
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
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
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
            return error;
          }
        }

        // flush delta compress
        error = Compress_flush(&archiveEntryInfo->hardLink.deltaCompressInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }
        eofDelta = FALSE;
        do
        {
          // flush compressed full data blocks
          error = flushHardLinkDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_FULL);
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
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
              Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
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
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }
        error = flushHardLinkDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_ANY);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // update part size
        archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize = Compress_getInputLength(&archiveEntryInfo->hardLink.deltaCompressInfo);
        error = Chunk_update(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // close chunks
        error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLink.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // reset header "written"
        archiveEntryInfo->hardLink.headerWrittenFlag = FALSE;

        // create archive file (if not already exists and open)
        error = createArchiveFile(archiveEntryInfo->archiveHandle,archiveEntryInfo->indexHandle);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // transfer intermediate data into archive
        error = transferArchiveFileData(archiveEntryInfo->archiveHandle,
                                        &archiveEntryInfo->hardLink.intermediateFileHandle
                                       );
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // close archive
        error = closeArchiveFile(archiveEntryInfo->archiveHandle,archiveEntryInfo->indexHandle);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
          return error;
        }

        // store in index database
        if (   (archiveEntryInfo->indexHandle != NULL)
            && (archiveEntryInfo->archiveHandle->storageId != DATABASE_ID_NONE)
           )
        {
          STRINGLIST_ITERATE(archiveEntryInfo->hardLink.fileNameList,stringNode,fileName)
          {
            error = Index_addFile(archiveEntryInfo->indexHandle,
                                  archiveEntryInfo->archiveHandle->storageId,
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

        // store fragment offset, count for next fragment
        archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset += archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize;
        archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize   =  0LL;

        // set new delta base-offset
        if (archiveEntryInfo->hardLink.deltaSourceHandleInitFlag)
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
        findNextArchivePart(archiveEntryInfo->archiveHandle,archiveEntryInfo->indexHandle);

        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
      }
      else
      {
        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);

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

  SEMAPHORE_LOCKED_DO(semaphoreLock,&decryptPasswordList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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
  SEMAPHORE_LOCKED_DO(semaphoreLock,&decryptPasswordList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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

  modified = FALSE;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&decryptPasswordList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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
  Errors Archive_create(ArchiveHandle          *archiveHandle,
                        IndexId                uuidId,
                        ConstString            jobUUID,
                        ConstString            scheduleUUID,
                        DeltaSourceList        *deltaSourceList,
                        const JobOptions       *jobOptions,
                        IndexHandle            *indexHandle,
                        IndexId                entityId,
                        ArchiveTypes           archiveType,
                        ArchiveInitFunction    archiveInitFunction,
                        void                   *archiveInitUserData,
                        ArchiveDoneFunction    archiveDoneFunction,
                        void                   *archiveDoneUserData,
                        ArchiveGetSizeFunction archiveGetSizeFunction,
                        void                   *archiveGetSizeUserData,
                        ArchiveStoreFunction   archiveStoreFunction,
                        void                   *archiveStoreUserData,
                        GetPasswordFunction    getPasswordFunction,
                        void                   *getPasswordUserData,
                        LogHandle              *logHandle
                       )
#else /* not NDEBUG */
  Errors __Archive_create(const char             *__fileName__,
                          ulong                   __lineNb__,
                          ArchiveHandle          *archiveHandle,
                          IndexId                uuidId,
                          ConstString            jobUUID,
                          ConstString            scheduleUUID,
                          DeltaSourceList        *deltaSourceList,
                          const JobOptions       *jobOptions,
                          IndexHandle            *indexHandle,
                          IndexId                entityId,
                          ArchiveTypes           archiveType,
                          ArchiveInitFunction    archiveInitFunction,
                          void                   *archiveInitUserData,
                          ArchiveDoneFunction    archiveDoneFunction,
                          void                   *archiveDoneUserData,
                          ArchiveGetSizeFunction archiveGetSizeFunction,
                          void                   *archiveGetSizeUserData,
                          ArchiveStoreFunction   archiveStoreFunction,
                          void                   *archiveStoreUserData,
                          GetPasswordFunction    getPasswordFunction,
                          void                   *getPasswordUserData,
                          LogHandle              *logHandle
                         )
#endif /* NDEBUG */
{
  AutoFreeList autoFreeList;
  Errors       error;
  bool         okFlag;
  ulong        maxCryptKeyDataLength;

  assert(archiveHandle != NULL);
  assert(archiveStoreFunction != NULL);
  assert(jobOptions != NULL);
  assert((indexHandle == NULL) || (entityId == INDEX_ID_NONE) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));

  // init variables
  AutoFree_init(&autoFreeList);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(jobOptions->cryptAlgorithm,&archiveHandle->blockLength);
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  assert(archiveHandle->blockLength > 0);
  if (archiveHandle->blockLength > MAX_BUFFER_SIZE)
  {
    AutoFree_cleanup(&autoFreeList);
    return ERROR_UNSUPPORTED_BLOCK_SIZE;
  }

  // init archive info
  archiveHandle->jobUUID                 = String_duplicate(jobUUID);
  archiveHandle->scheduleUUID            = String_duplicate(scheduleUUID);
  archiveHandle->jobOptions              = jobOptions;
  archiveHandle->deltaSourceList         = deltaSourceList;
  archiveHandle->indexHandle             = indexHandle;
  archiveHandle->uuidId                  = uuidId;
  archiveHandle->entityId                = entityId;
  archiveHandle->archiveType             = archiveType;
  archiveHandle->archiveInitFunction     = archiveInitFunction;
  archiveHandle->archiveInitUserData     = archiveInitUserData;
  archiveHandle->archiveDoneFunction     = archiveDoneFunction;
  archiveHandle->archiveDoneUserData     = archiveDoneUserData;
  archiveHandle->archiveGetSizeFunction  = archiveGetSizeFunction;
  archiveHandle->archiveGetSizeUserData  = archiveGetSizeUserData;
  archiveHandle->archiveStoreFunction    = archiveStoreFunction;
  archiveHandle->archiveStoreUserData    = archiveStoreUserData;
  archiveHandle->getPasswordFunction     = getPasswordFunction;
  archiveHandle->getPasswordUserData     = getPasswordUserData;
  archiveHandle->logHandle               = logHandle;

  Semaphore_init(&archiveHandle->passwordLock);
  archiveHandle->cryptType               = Crypt_isEncrypted(jobOptions->cryptAlgorithm) ? jobOptions->cryptType : CRYPT_TYPE_NONE;
  archiveHandle->cryptPassword           = NULL;
  archiveHandle->cryptPasswordReadFlag   = FALSE;
  archiveHandle->cryptKeyData            = NULL;
  archiveHandle->cryptKeyDataLength      = 0;
  Crypt_randomize(archiveHandle->cryptSalt,sizeof(archiveHandle->cryptSalt));

  archiveHandle->ioType                  = ARCHIVE_IO_TYPE_FILE;
  archiveHandle->file.fileName           = String_new();
  archiveHandle->file.openFlag           = FALSE;
  archiveHandle->printableStorageName    = NULL;
  Semaphore_init(&archiveHandle->chunkIOLock);
  archiveHandle->chunkIO                 = &CHUNK_IO_FILE;
  archiveHandle->chunkIOUserData         = &archiveHandle->file.fileHandle;

  archiveHandle->storageId               = DATABASE_ID_NONE;

  archiveHandle->entries                 = 0LL;
  archiveHandle->archiveFileSize         = 0LL;
  archiveHandle->partNumber              = 0;

  archiveHandle->pendingError            = ERROR_NONE;
  archiveHandle->nextChunkHeaderReadFlag = FALSE;

  archiveHandle->interrupt.openFlag      = FALSE;
  archiveHandle->interrupt.offset        = 0LL;
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->passwordLock,{ Semaphore_done(&archiveHandle->passwordLock); });
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->file.fileName,{ String_delete(archiveHandle->file.fileName); });
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->chunkIOLock,{ Semaphore_done(&archiveHandle->chunkIOLock); });

  // init key (if asymmetric encryption used)
  if (archiveHandle->cryptType == CRYPT_TYPE_ASYMMETRIC)
  {
    // check if public key available
    if (jobOptions->cryptPublicKey.data == NULL)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_PUBLIC_KEY;
    }

    // read public key
    Crypt_initKey(&archiveHandle->cryptKey,CRYPT_PADDING_TYPE_NONE);
    error = Crypt_setKeyData(&archiveHandle->cryptKey,
                             jobOptions->cryptPublicKey.data,
                             jobOptions->cryptPublicKey.length,
                             NULL,  // password
                             NULL,  // salt
                             0  // saltLength
                            );
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // create new random key for encryption
    archiveHandle->cryptPassword = Password_new();
    if (archiveHandle->cryptPassword == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    AUTOFREE_ADD(&autoFreeList,&archiveHandle->cryptPassword,{ Password_delete(archiveHandle->cryptPassword); });
    maxCryptKeyDataLength = 2*MAX_PASSWORD_LENGTH;
    okFlag = FALSE;
    do
    {
      archiveHandle->cryptKeyData = malloc(maxCryptKeyDataLength);
      if (archiveHandle->cryptKeyData == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      error = Crypt_getRandomEncryptKey(&archiveHandle->cryptKey,
                                        jobOptions->cryptAlgorithm,
                                        archiveHandle->cryptPassword,
                                        maxCryptKeyDataLength,
                                        archiveHandle->cryptKeyData,
                                        &archiveHandle->cryptKeyDataLength
                                       );
      if (error != ERROR_NONE)
      {
        free(archiveHandle->cryptKeyData);
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      if (archiveHandle->cryptKeyDataLength < maxCryptKeyDataLength)
      {
        okFlag = TRUE;
      }
      else
      {
        free(archiveHandle->cryptKeyData);
        maxCryptKeyDataLength += 64;
      }
    }
    while (!okFlag);
    AUTOFREE_ADD(&autoFreeList,&archiveHandle->cryptKeyData,{ free(archiveHandle->cryptKeyData); });
    DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
#if 0
Password_dump(archiveHandle->cryptPassword);
{
int z;
byte *p=archiveHandle->cryptKeyData;
fprintf(stderr,"data: ");for (z=0;z<archiveHandle->cryptKeyDataLength;z++) fprintf(stderr,"%02x",p[z]); fprintf(stderr,"\n");
}
#endif /* 0 */
  }

  // init signature hash
  error = Crypt_initHash(&archiveHandle->signatureHash,CRYPT_HASH_ALGORITHM_SHA2_512);
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->signatureHash,{ Crypt_doneHash(&archiveHandle->signatureHash); });
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  // free resources
  AutoFree_done(&autoFreeList);

  #ifdef DEBUG_ENCODED_DATA_FILENAME
    {
      int handle = open64(DEBUG_ENCODED_DATA_FILENAME,O_CREAT|O_WRONLY|O_TRUNC|O_LARGEFILE,0664);
      close(handle);
    }
  #endif /* DEBUG_ENCODED_DATA_FILENAME */

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveHandle,sizeof(ArchiveHandle));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveHandle,sizeof(ArchiveHandle));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_open(ArchiveHandle          *archiveHandle,
                      StorageInfo            *storageInfo,
                      ConstString            fileName,
                      DeltaSourceList        *deltaSourceList,
                      const JobOptions       *jobOptions,
                      GetPasswordFunction    getPasswordFunction,
                      void                   *getPasswordUserData,
                      LogHandle              *logHandle
                     )
#else /* not NDEBUG */
  Errors __Archive_open(const char             *__fileName__,
                        ulong                  __lineNb__,
                        ArchiveHandle          *archiveHandle,
                        StorageInfo            *storageInfo,
                        ConstString            fileName,
                        DeltaSourceList        *deltaSourceList,
                        const JobOptions       *jobOptions,
                        GetPasswordFunction    getPasswordFunction,
                        void                   *getPasswordUserData,
                        LogHandle              *logHandle
                       )
#endif /* NDEBUG */
{
  AutoFreeList autoFreeList;
  Errors       error;
  ChunkHeader  chunkHeader;

  assert(archiveHandle != NULL);
  assert(storageInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  archiveHandle->deltaSourceList         = deltaSourceList;
  archiveHandle->jobOptions              = jobOptions;
  archiveHandle->archiveInitFunction     = NULL;
  archiveHandle->archiveInitUserData     = NULL;
  archiveHandle->archiveDoneFunction     = NULL;
  archiveHandle->archiveDoneUserData     = NULL;
  archiveHandle->archiveStoreFunction    = NULL;
  archiveHandle->archiveStoreUserData    = NULL;
  archiveHandle->getPasswordFunction     = getPasswordFunction;
  archiveHandle->getPasswordUserData     = getPasswordUserData;
  archiveHandle->logHandle               = logHandle;

  Semaphore_init(&archiveHandle->passwordLock);
  archiveHandle->cryptType               = CRYPT_TYPE_NONE;
  archiveHandle->cryptPassword           = NULL;
  archiveHandle->cryptPasswordReadFlag   = FALSE;
  archiveHandle->cryptKeyData            = NULL;
  archiveHandle->cryptKeyDataLength      = 0;
  memset(archiveHandle->cryptSalt,0,sizeof(archiveHandle->cryptSalt));

  archiveHandle->ioType                  = ARCHIVE_IO_TYPE_STORAGE_FILE;
  archiveHandle->storage.storageInfo     = storageInfo;
  Storage_duplicateSpecifier(&archiveHandle->storage.storageSpecifier,&storageInfo->storageSpecifier);
  archiveHandle->printableStorageName    = String_duplicate(Storage_getPrintableName(&archiveHandle->storage.storageSpecifier,fileName));
  Semaphore_init(&archiveHandle->chunkIOLock);
  archiveHandle->chunkIO                 = &CHUNK_IO_STORAGE_FILE;
  archiveHandle->chunkIOUserData         = &archiveHandle->storage.storageHandle;

//  archiveHandle->indexHandle             = NULL;
  archiveHandle->jobUUID                 = NULL;
  archiveHandle->scheduleUUID            = NULL;
//  archiveHandle->entityId                = DATABASE_ID_NONE;
  archiveHandle->storageId               = DATABASE_ID_NONE;

  archiveHandle->entries                 = 0LL;
  archiveHandle->archiveFileSize         = 0LL;
  archiveHandle->partNumber              = 0;

  archiveHandle->pendingError            = ERROR_NONE;
  archiveHandle->nextChunkHeaderReadFlag = FALSE;

  archiveHandle->interrupt.openFlag      = FALSE;
  archiveHandle->interrupt.offset        = 0LL;
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->passwordLock,{ Semaphore_done(&archiveHandle->passwordLock); });
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->storage.storageSpecifier,{ Storage_doneSpecifier(&archiveHandle->storage.storageSpecifier); });
  AUTOFREE_ADD(&autoFreeList,archiveHandle->printableStorageName,{ String_delete(archiveHandle->printableStorageName); });
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->chunkIOLock,{ Semaphore_done(&archiveHandle->chunkIOLock); });

  // init signature hash
  error = Crypt_initHash(&archiveHandle->signatureHash,CRYPT_HASH_ALGORITHM_SHA2_512);
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->signatureHash,{ Crypt_doneHash(&archiveHandle->signatureHash); });
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  // open storage
  error = Storage_open(&archiveHandle->storage.storageHandle,
                       archiveHandle->storage.storageInfo,
                       fileName
                      );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->storage.storageHandle,{ Storage_close(&archiveHandle->storage.storageHandle); });
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  // check if BAR archive file
  error = getNextChunkHeader(archiveHandle,&chunkHeader);
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
  ungetNextChunkHeader(archiveHandle,&chunkHeader);
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  // free resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveHandle,sizeof(ArchiveHandle));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveHandle,sizeof(ArchiveHandle));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_close(ArchiveHandle *archiveHandle)
#else /* not NDEBUG */
  Errors __Archive_close(const char    *__fileName__,
                         ulong         __lineNb__,
                         ArchiveHandle *archiveHandle
                        )
#endif /* NDEBUG */
{
  Errors error;

  assert(archiveHandle != NULL);

  #ifndef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveHandle,sizeof(ArchiveHandle));
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACE(archiveHandle,sizeof(ArchiveHandle));
  #endif /* NDEBUG */

  // init variables
  error = ERROR_UNKNOWN;

  // close file/storage
  switch (archiveHandle->ioType)
  {
    case ARCHIVE_IO_TYPE_FILE:
      error = closeArchiveFile(archiveHandle,archiveHandle->indexHandle);
      break;
    case ARCHIVE_IO_TYPE_STORAGE_FILE:
      Storage_close(&archiveHandle->storage.storageHandle);
      error = ERROR_NONE;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  // free resources
  Crypt_doneHash(&archiveHandle->signatureHash);

  if (archiveHandle->cryptType == CRYPT_TYPE_ASYMMETRIC)
  {
    assert(archiveHandle->cryptKeyData != NULL);

    free(archiveHandle->cryptKeyData);
    Crypt_doneKey(&archiveHandle->cryptKey);
  }

  if (archiveHandle->jobUUID != NULL) String_delete(archiveHandle->jobUUID);
  if (archiveHandle->scheduleUUID != NULL) String_delete(archiveHandle->scheduleUUID);

  Semaphore_done(&archiveHandle->chunkIOLock);
  if (archiveHandle->cryptPassword  != NULL) Password_delete(archiveHandle->cryptPassword);
  String_delete(archiveHandle->printableStorageName);
  switch (archiveHandle->ioType)
  {
    case ARCHIVE_IO_TYPE_FILE:
      if (archiveHandle->file.fileName != NULL) String_delete(archiveHandle->file.fileName);
      break;
    case ARCHIVE_IO_TYPE_STORAGE_FILE:
      Storage_doneSpecifier(&archiveHandle->storage.storageSpecifier);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  Semaphore_done(&archiveHandle->passwordLock);

  return error;
}

Errors Archive_storageInterrupt(ArchiveHandle *archiveHandle)
{
  SemaphoreLock semaphoreLock;
  Errors        error;

  assert(archiveHandle != NULL);

  switch (archiveHandle->ioType)
  {
    case ARCHIVE_IO_TYPE_FILE:
      SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        archiveHandle->interrupt.openFlag = archiveHandle->file.openFlag;
        if (archiveHandle->file.openFlag)
        {
          error = File_tell(&archiveHandle->file.fileHandle,&archiveHandle->interrupt.offset);
          if (error != ERROR_NONE)
          {
            return error;
          }
          File_close(&archiveHandle->file.fileHandle);
          archiveHandle->file.openFlag = FALSE;
        }
      }
      break;
    case ARCHIVE_IO_TYPE_STORAGE_FILE:
      error = Storage_tell(&archiveHandle->storage.storageHandle,&archiveHandle->interrupt.offset);
      if (error != ERROR_NONE)
      {
        return error;
      }
      Storage_close(&archiveHandle->storage.storageHandle);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

Errors Archive_storageContinue(ArchiveHandle *archiveHandle)
{
  SemaphoreLock semaphoreLock;
  Errors        error;

  assert(archiveHandle != NULL);

  switch (archiveHandle->ioType)
  {
    case ARCHIVE_IO_TYPE_FILE:
      SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        if (archiveHandle->interrupt.openFlag)
        {
          error = File_open(&archiveHandle->file.fileHandle,archiveHandle->file.fileName,FILE_OPEN_WRITE);
          if (error != ERROR_NONE)
          {
            return error;
          }
#ifndef WERROR
#warning seek?
#endif
          error = File_seek(&archiveHandle->file.fileHandle,archiveHandle->interrupt.offset);
          if (error != ERROR_NONE)
          {
            return error;
          }
        }
        archiveHandle->file.openFlag = archiveHandle->interrupt.openFlag;
      }
      break;
    case ARCHIVE_IO_TYPE_STORAGE_FILE:
      error = Storage_open(&archiveHandle->storage.storageHandle,
                           archiveHandle->storage.storageInfo,
                           NULL  // archiveName
                          );
      if (error != ERROR_NONE)
      {
        return error;
      }
#ifndef WERROR
#warning seek?
#endif
      error = Storage_seek(&archiveHandle->storage.storageHandle,archiveHandle->interrupt.offset);
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

bool Archive_eof(ArchiveHandle *archiveHandle,
                 bool          skipUnknownChunksFlag
                )
{
  bool           chunkHeaderFoundFlag;
  bool           scanFlag;
  ChunkHeader    chunkHeader;
  bool           decryptedFlag;
  PasswordHandle passwordHandle;
  const Password *password;

  assert(archiveHandle != NULL);
  assert(archiveHandle->chunkIO != NULL);
  assert(archiveHandle->chunkIO->seek != NULL);
  assert(archiveHandle->jobOptions != NULL);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    return FALSE;
  }

  // find next file, image, directory, link, hard link, special chunk
  chunkHeaderFoundFlag = FALSE;
  scanFlag             = FALSE;
  while (   !chunkHeaderFoundFlag
         && !chunkHeaderEOF(archiveHandle)
        )
  {
    // get next chunk header
    archiveHandle->pendingError = getNextChunkHeader(archiveHandle,&chunkHeader);
    if (archiveHandle->pendingError != ERROR_NONE)
    {
      return FALSE;
    }

    // find next file, image, directory, link, special chunk
    switch (chunkHeader.id)
    {
      case CHUNK_ID_BAR:
        // bar header is simply ignored
        archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
        if (archiveHandle->pendingError != ERROR_NONE)
        {
          return FALSE;
        }
        scanFlag = FALSE;
        break;
      case CHUNK_ID_KEY:
        // check if private key available
        if (archiveHandle->jobOptions->cryptPrivateKey.data == NULL)
        {
          archiveHandle->pendingError = ERROR_NO_PRIVATE_KEY;
          return FALSE;
        }

        // read private key, try to read key with no password, all passwords
        Crypt_initKey(&archiveHandle->cryptKey,CRYPT_PADDING_TYPE_NONE);
        decryptedFlag = FALSE;
        archiveHandle->pendingError = Crypt_setKeyData(&archiveHandle->cryptKey,
                                                       archiveHandle->jobOptions->cryptPrivateKey.data,
                                                       archiveHandle->jobOptions->cryptPrivateKey.length,
                                                       NULL,  // password
                                                       NULL,  // salt
                                                       0  // saltLength
                                                      );
        if (archiveHandle->pendingError == ERROR_NONE)
        {
          decryptedFlag = TRUE;
        }
        password = getFirstDecryptPassword(&passwordHandle,
                                           archiveHandle,
                                           archiveHandle->jobOptions,
                                           archiveHandle->jobOptions->cryptPasswordMode,
                                           archiveHandle->getPasswordFunction,
                                           archiveHandle->getPasswordUserData
                                          );
        while (   !decryptedFlag
               && (password != NULL)
              )
        {
          archiveHandle->pendingError = Crypt_setKeyData(&archiveHandle->cryptKey,
                                                         archiveHandle->jobOptions->cryptPrivateKey.data,
                                                         archiveHandle->jobOptions->cryptPrivateKey.length,
                                                         password,
                                                         NULL,  // salt
                                                         0  // saltLength
                                                        );
          if (archiveHandle->pendingError == ERROR_NONE)
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
          archiveHandle->pendingError = ERROR_KEY_ENCRYPT_FAIL;
          return FALSE;
        }

        // read encryption key
        archiveHandle->pendingError = readEncryptionKey(archiveHandle,&chunkHeader);
        if (archiveHandle->pendingError != ERROR_NONE)
        {
          return FALSE;
        }
#if 0
Password_dump(archiveHandle->cryptPassword);
{
int z;
byte *p=archiveHandle->cryptKeyData;
fprintf(stderr,"data: ");for (z=0;z<archiveHandle->cryptKeyDataLength;z++) fprintf(stderr,"%02x",p[z]); fprintf(stderr,"\n");
}
#endif /* 0 */
        archiveHandle->cryptType = CRYPT_TYPE_ASYMMETRIC;

        scanFlag = FALSE;
        break;
      case CHUNK_ID_FILE:
      case CHUNK_ID_IMAGE:
      case CHUNK_ID_DIRECTORY:
      case CHUNK_ID_LINK:
      case CHUNK_ID_HARDLINK:
      case CHUNK_ID_SPECIAL:
      case CHUNK_ID_META:
      case CHUNK_ID_SIGNATURE:
        chunkHeaderFoundFlag = TRUE;

        scanFlag = FALSE;
        break;
      default:
        if (skipUnknownChunksFlag)
        {
          // unknown chunk -> switch to scan mode
          if (!scanFlag)
          {
            if (isPrintInfo(3))
            {
              printWarning("Skipped unknown chunk '%s' (offset %llu) in '%s'. Switch to scan mode.\n",
                           Chunk_idToString(chunkHeader.id),
                           chunkHeader.offset,
                           String_cString(archiveHandle->printableStorageName)
                          );
            }

            scanFlag = TRUE;
          }

          // skip 1 byte
          archiveHandle->pendingError = archiveHandle->chunkIO->seek(archiveHandle->chunkIOUserData,chunkHeader.offset+1LL);
          if (archiveHandle->pendingError != ERROR_NONE)
          {
            return FALSE;
          }
        }
        else
        {
          // report unknown chunk
          archiveHandle->pendingError = ERROR_UNKNOWN_CHUNK;
          printWarning("Skipped unknown chunk '%s' (offset %llu) in '%s'. Switch to scan mode.\n",
                       Chunk_idToString(chunkHeader.id),
                       chunkHeader.offset,
                       String_cString(archiveHandle->printableStorageName)
                      );
          return FALSE;
        }
        break;
    }
  }

  // store chunk header for read
  if (chunkHeaderFoundFlag)
  {
    ungetNextChunkHeader(archiveHandle,&chunkHeader);
  }

  return !chunkHeaderFoundFlag;
}

#ifdef NDEBUG
  Errors Archive_newFileEntry(ArchiveEntryInfo                *archiveEntryInfo,
                              ArchiveHandle                   *archiveHandle,
                              IndexHandle                     *indexHandle,
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
                                ArchiveHandle                   *archiveHandle,
                                IndexHandle                     *indexHandle,
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
  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);
  assert(archiveHandle->blockLength > 0);
  assert(fileInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  // init crypt password
  error = initCryptPassword(archiveHandle);
  if (error !=  ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // init archive entry info
  archiveEntryInfo->archiveHandle                    = archiveHandle;
  archiveEntryInfo->indexHandle                    = indexHandle;
  archiveEntryInfo->mode                           = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm                 = archiveHandle->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength                    = archiveHandle->blockLength;

  archiveEntryInfo->archiveEntryType               = ARCHIVE_ENTRY_TYPE_FILE;

  archiveEntryInfo->file.deltaCompressAlgorithm    = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->file.byteCompressAlgorithm     = byteCompressFlag ? archiveHandle->jobOptions->compressAlgorithms.byte : COMPRESS_ALGORITHM_NONE;

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
                                  archiveEntryInfo->archiveHandle->deltaSourceList,
                                  NULL, // storageName
                                  fileName,
                                  SOURCE_SIZE_UNKNOWN,
                                  archiveHandle->jobOptions
                                 );
    if      (error == ERROR_NONE)
    {
      archiveEntryInfo->file.deltaSourceHandleInitFlag   = TRUE;
      archiveEntryInfo->file.deltaCompressAlgorithm = archiveHandle->jobOptions->compressAlgorithms.delta;
      AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.deltaSourceHandle,{ DeltaSource_closeEntry(&archiveEntryInfo->file.deltaSourceHandle); });
    }
    else if (archiveHandle->jobOptions->forceDeltaCompressionFlag)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    else
    {
      printWarning("File '%s' not delta compressed (no source file found)\n",String_cString(fileName));
      logMessage(archiveEntryInfo->archiveHandle->logHandle,
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
  archiveEntryInfo->file.chunkFile.cryptAlgorithm    = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithm);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFile.info,{ Chunk_done(&archiveEntryInfo->file.chunkFile.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->file.chunkFileEntry.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->file.chunkFileDelta.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileDelta.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->file.chunkFileData.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileData.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->file.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
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
  findNextArchivePart(archiveEntryInfo->archiveHandle,archiveEntryInfo->indexHandle);

  if (!archiveEntryInfo->archiveHandle->jobOptions->dryRunFlag)
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
                               ArchiveHandle    *archiveHandle,
                               IndexHandle      *indexHandle,
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
                                 ArchiveHandle    *archiveHandle,
                                 IndexHandle      *indexHandle,
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
  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);
  assert(archiveHandle->blockLength > 0);
  assert(deviceInfo != NULL);
  assert(deviceInfo->blockSize > 0);

  // init variables
  AutoFree_init(&autoFreeList);

  // init crypt password
  error = initCryptPassword(archiveHandle);
  if (error !=  ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // init archive entry info
  archiveEntryInfo->archiveHandle                  = archiveHandle;
  archiveEntryInfo->indexHandle                    = indexHandle;
  archiveEntryInfo->mode                         = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm               = archiveHandle->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength                  = archiveHandle->blockLength;

  archiveEntryInfo->archiveEntryType             = ARCHIVE_ENTRY_TYPE_IMAGE;

  archiveEntryInfo->image.blockSize              = deviceInfo->blockSize;

  archiveEntryInfo->image.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->image.byteCompressAlgorithm  = byteCompressFlag ?archiveHandle->jobOptions->compressAlgorithms.byte :COMPRESS_ALGORITHM_NONE;

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
                                  archiveEntryInfo->archiveHandle->deltaSourceList,
                                  NULL, // storageName
                                  deviceName,
                                  SOURCE_SIZE_UNKNOWN,
                                  archiveHandle->jobOptions
                                 );
    if (error == ERROR_NONE)
    {
      archiveEntryInfo->image.deltaSourceHandleInitFlag   = TRUE;
      archiveEntryInfo->image.deltaCompressAlgorithm = archiveHandle->jobOptions->compressAlgorithms.delta;
      AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.deltaSourceHandle,{ DeltaSource_closeEntry(&archiveEntryInfo->image.deltaSourceHandle); });
    }
    else
    {
      if (archiveHandle->jobOptions->forceDeltaCompressionFlag)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      else
      {
        logMessage(archiveEntryInfo->archiveHandle->logHandle,
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
  archiveEntryInfo->image.chunkImage.cryptAlgorithm    = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithm);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImage.info,{ Chunk_done(&archiveEntryInfo->image.chunkImage.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->image.chunkImageEntry.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImageEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->image.chunkImageDelta.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImageDelta.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->image.chunkImageData.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImageData.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->image.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
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
  findNextArchivePart(archiveEntryInfo->archiveHandle,archiveEntryInfo->indexHandle);

  if (!archiveEntryInfo->archiveHandle->jobOptions->dryRunFlag)
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
  findNextArchivePart(archiveEntryInfo->archiveHandle,archiveEntryInfo->indexHandle);

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
                                   ArchiveHandle                   *archiveHandle,
                                   IndexHandle                     *indexHandle,
                                   ConstString                     directoryName,
                                   const FileInfo                  *fileInfo,
                                   const FileExtendedAttributeList *fileExtendedAttributeList
                                  )
#else /* not NDEBUG */
  Errors __Archive_newDirectoryEntry(const char                      *__fileName__,
                                     ulong                           __lineNb__,
                                     ArchiveEntryInfo                *archiveEntryInfo,
                                     ArchiveHandle                   *archiveHandle,
                                     IndexHandle                     *indexHandle,
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
  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);
  assert(archiveHandle->blockLength > 0);
  assert(fileInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  // init crypt password
  error = initCryptPassword(archiveHandle);
  if (error !=  ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // init archive entry info
  archiveEntryInfo->archiveHandle      = archiveHandle;
  archiveEntryInfo->indexHandle      = indexHandle;
  archiveEntryInfo->mode             = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm   = archiveHandle->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength      = archiveHandle->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_DIRECTORY;

  // init directory entry chunk
  error = Chunk_init(&archiveEntryInfo->directory.chunkDirectory.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
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
  archiveEntryInfo->directory.chunkDirectory.cryptAlgorithm = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithm);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->directory.chunkDirectory.info,{ Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
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
  findNextArchivePart(archiveEntryInfo->archiveHandle,indexHandle);

  if (!archiveEntryInfo->archiveHandle->jobOptions->dryRunFlag)
  {
    // lock archive
    Semaphore_forceLock(&archiveEntryInfo->archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

    // ensure space in archive
    error = ensureArchiveSpace(archiveEntryInfo->archiveHandle,
                               indexHandle,
                               headerLength
                              );
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write directory chunk
    error = Chunk_create(&archiveEntryInfo->directory.chunkDirectory.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write directory entry chunk
    error = Chunk_create(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
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
      Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
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
                              ArchiveHandle                   *archiveHandle,
                              IndexHandle                     *indexHandle,
                              ConstString                     linkName,
                              ConstString                     destinationName,
                              const FileInfo                  *fileInfo,
                              const FileExtendedAttributeList *fileExtendedAttributeList
                             )
#else /* not NDEBUG */
  Errors __Archive_newLinkEntry(const char                      *__fileName__,
                                ulong                           __lineNb__,
                                ArchiveEntryInfo                *archiveEntryInfo,
                                ArchiveHandle                   *archiveHandle,
                                IndexHandle                     *indexHandle,
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
  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);
  assert(archiveHandle->blockLength > 0);
  assert(fileInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  // init crypt password
  error = initCryptPassword(archiveHandle);
  if (error !=  ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // init archive entry info
  archiveEntryInfo->archiveHandle      = archiveHandle;
  archiveEntryInfo->indexHandle      = indexHandle;
  archiveEntryInfo->mode             = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm   = archiveHandle->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength      = archiveHandle->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_LINK;

  // init link chunk
  error = Chunk_init(&archiveEntryInfo->link.chunkLink.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
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
  archiveEntryInfo->link.chunkLink.cryptAlgorithm = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithm);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->link.chunkLink.info,{ Chunk_done(&archiveEntryInfo->link.chunkLink.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->link.chunkLinkEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->link.chunkLinkExtendedAttribute.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
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


  if (!archiveEntryInfo->archiveHandle->jobOptions->dryRunFlag)
  {
    // lock archive
    Semaphore_forceLock(&archiveEntryInfo->archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

    // ensure space in archive
    error = ensureArchiveSpace(archiveEntryInfo->archiveHandle,
                               indexHandle,
                               headerLength
                              );
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write link chunk
    error = Chunk_create(&archiveEntryInfo->link.chunkLink.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write link entry chunk
    error = Chunk_create(&archiveEntryInfo->link.chunkLinkEntry.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
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
      Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
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
                                  ArchiveHandle                   *archiveHandle,
                                  IndexHandle                     *indexHandle,
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
                                    ArchiveHandle                   *archiveHandle,
                                    IndexHandle                     *indexHandle,
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
  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);
  assert(archiveHandle->blockLength > 0);
  assert(fileInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  // init crypt password
  error = initCryptPassword(archiveHandle);
  if (error !=  ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // init archive entry info
  archiveEntryInfo->archiveHandle                        = archiveHandle;
  archiveEntryInfo->indexHandle                        = indexHandle;
  archiveEntryInfo->mode                               = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm                     = archiveHandle->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength                        = archiveHandle->blockLength;

  archiveEntryInfo->archiveEntryType                   = ARCHIVE_ENTRY_TYPE_HARDLINK;

  archiveEntryInfo->hardLink.deltaCompressAlgorithm    = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->hardLink.byteCompressAlgorithm     = byteCompressFlag ?archiveHandle->jobOptions->compressAlgorithms.byte :COMPRESS_ALGORITHM_NONE;

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
                                    archiveEntryInfo->archiveHandle->deltaSourceList,
                                    NULL, // storageName
                                    fileName,
                                    SOURCE_SIZE_UNKNOWN,
                                    archiveHandle->jobOptions
                                   );
      if (error == ERROR_NONE) break;
    }
    if      (error == ERROR_NONE)
    {
      archiveEntryInfo->hardLink.deltaSourceHandleInitFlag   = TRUE;
      archiveEntryInfo->hardLink.deltaCompressAlgorithm = archiveHandle->jobOptions->compressAlgorithms.delta;
      AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.deltaSourceHandle,{ DeltaSource_closeEntry(&archiveEntryInfo->hardLink.deltaSourceHandle); });
    }
    else if (archiveHandle->jobOptions->forceDeltaCompressionFlag)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    else
    {
      logMessage(archiveEntryInfo->archiveHandle->logHandle,
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
  archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithm    = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithm);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLink.info,{ Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->hardLink.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
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
  findNextArchivePart(archiveEntryInfo->archiveHandle,indexHandle);

  if (!archiveEntryInfo->archiveHandle->jobOptions->dryRunFlag)
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
                                 ArchiveHandle                   *archiveHandle,
                                 IndexHandle                     *indexHandle,
                                 ConstString                     specialName,
                                 const FileInfo                  *fileInfo,
                                 const FileExtendedAttributeList *fileExtendedAttributeList
                                )
#else /* not NDEBUG */
  Errors __Archive_newSpecialEntry(const char                      *__fileName__,
                                   ulong                           __lineNb__,
                                   ArchiveEntryInfo                *archiveEntryInfo,
                                   ArchiveHandle                   *archiveHandle,
                                   IndexHandle                     *indexHandle,
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
  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);
  assert(archiveHandle->blockLength > 0);
  assert(fileInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  // init crypt password
  error = initCryptPassword(archiveHandle);
  if (error !=  ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // init archive entry info
  archiveEntryInfo->archiveHandle      = archiveHandle;
  archiveEntryInfo->indexHandle      = indexHandle;
  archiveEntryInfo->mode             = ARCHIVE_MODE_WRITE;

  archiveEntryInfo->cryptAlgorithm   = archiveHandle->jobOptions->cryptAlgorithm;
  archiveEntryInfo->blockLength      = archiveHandle->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_SPECIAL;

  // init special chunk
  error = Chunk_init(&archiveEntryInfo->special.chunkSpecial.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
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
  archiveEntryInfo->special.chunkSpecial.cryptAlgorithm = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithm);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->special.chunkSpecial.info,{ Chunk_done(&archiveEntryInfo->special.chunkSpecial.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.cryptInfo,
                     archiveHandle->jobOptions->cryptAlgorithm,
                     archiveHandle->cryptPassword,
                     NULL,  // salt
                     0  // saltLength
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
  findNextArchivePart(archiveEntryInfo->archiveHandle,indexHandle);

  if (!archiveEntryInfo->archiveHandle->jobOptions->dryRunFlag)
  {
    // lock archive
    Semaphore_forceLock(&archiveEntryInfo->archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

    // ensure space in archive
    error = ensureArchiveSpace(archiveEntryInfo->archiveHandle,
                               indexHandle,
                               headerLength
                              );
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write special chunk
    error = Chunk_create(&archiveEntryInfo->special.chunkSpecial.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write special entry chunk
    error = Chunk_create(&archiveEntryInfo->special.chunkSpecialEntry.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
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
      Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
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

Errors Archive_getNextArchiveEntry(ArchiveHandle     *archiveHandle,
                                   ArchiveEntryTypes *archiveEntryType,
                                   uint64            *offset,
                                   bool              skipUnknownChunksFlag
                                  )
{
  Errors         error;
  bool           scanMode;
  ChunkHeader    chunkHeader;
  bool           decryptedFlag;
  PasswordHandle passwordHandle;
  const Password *password;

  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // find next file, image, directory, link, special chunk
  scanMode = FALSE;
  do
  {
    // get next chunk
    error = getNextChunkHeader(archiveHandle,&chunkHeader);
    if (error != ERROR_NONE)
    {
      return error;
    }

    switch (chunkHeader.id)
    {
      case CHUNK_ID_BAR:
        // bar header is simply ignored
        error = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
        if (error != ERROR_NONE)
        {
          return error;
        }

        scanMode = FALSE;
        break;
      case CHUNK_ID_KEY:
        // check if private key available
        if (archiveHandle->jobOptions->cryptPrivateKey.data == NULL)
        {
          return ERROR_NO_PRIVATE_KEY;
        }

        // read private key, try to read key with no password, all passwords
        Crypt_initKey(&archiveHandle->cryptKey,CRYPT_PADDING_TYPE_NONE);
        decryptedFlag = FALSE;
        error = Crypt_setKeyData(&archiveHandle->cryptKey,
                                 archiveHandle->jobOptions->cryptPrivateKey.data,
                                 archiveHandle->jobOptions->cryptPrivateKey.length,
                                 NULL,  // password
                                 NULL,  // salt
                                 0  // saltLength
                                );
        if (error == ERROR_NONE)
        {
          decryptedFlag = TRUE;
        }
        password = getFirstDecryptPassword(&passwordHandle,
                                           archiveHandle,
                                           archiveHandle->jobOptions,
                                           archiveHandle->jobOptions->cryptPasswordMode,
                                           archiveHandle->getPasswordFunction,
                                           archiveHandle->getPasswordUserData
                                          );
        while (   !decryptedFlag
               && (password != NULL)
              )
        {
          error = Crypt_setKeyData(&archiveHandle->cryptKey,
                                   archiveHandle->jobOptions->cryptPrivateKey.data,
                                   archiveHandle->jobOptions->cryptPrivateKey.length,
                                   password,
                                   NULL,  // salt
                                   0  // saltLength
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
        error = readEncryptionKey(archiveHandle,&chunkHeader);
        if (error != ERROR_NONE)
        {
          return error;
        }
#if 0
Password_dump(archiveHandle->cryptPassword);
{
int z;
byte *p=archiveHandle->cryptKeyData;
fprintf(stderr,"data: ");for (z=0;z<archiveHandle->cryptKeyDataLength;z++) fprintf(stderr,"%02x",p[z]); fprintf(stderr,"\n");
}
#endif /* 0 */
        archiveHandle->cryptType = CRYPT_TYPE_ASYMMETRIC;

        scanMode = FALSE;
        break;
      case CHUNK_ID_FILE:
      case CHUNK_ID_IMAGE:
      case CHUNK_ID_DIRECTORY:
      case CHUNK_ID_LINK:
      case CHUNK_ID_HARDLINK:
      case CHUNK_ID_SPECIAL:
      case CHUNK_ID_META:
      case CHUNK_ID_SIGNATURE:
        scanMode = FALSE;
        break;
      default:
        if (skipUnknownChunksFlag)
        {
          // unknown chunk -> switch to scan mode
          if (!scanMode)
          {
            if (isPrintInfo(3))
            {
              printWarning("Skipped unknown chunk '%s' (offset %llu) in '%s'. Switch to scan mode.\n",
                           Chunk_idToString(chunkHeader.id),
                           chunkHeader.offset,
                           String_cString(archiveHandle->printableStorageName)
                          );
            }

            scanMode = TRUE;
          }

          // skip 1 byte
          error = archiveHandle->chunkIO->seek(archiveHandle->chunkIOUserData,chunkHeader.offset+1LL);
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
         && (chunkHeader.id != CHUNK_ID_META)
         && (chunkHeader.id != CHUNK_ID_SIGNATURE)
        );

  // get archive entry type, offset
  if (archiveEntryType != NULL)
  {
    switch (chunkHeader.id)
    {
      case CHUNK_ID_FILE:      (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_FILE;      break;
      case CHUNK_ID_IMAGE:     (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_IMAGE;     break;
      case CHUNK_ID_DIRECTORY: (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_DIRECTORY; break;
      case CHUNK_ID_LINK:      (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_LINK;      break;
      case CHUNK_ID_HARDLINK:  (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_HARDLINK;  break;
      case CHUNK_ID_SPECIAL:   (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_SPECIAL;   break;
      case CHUNK_ID_META:      (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_META;      break;
      case CHUNK_ID_SIGNATURE: (*archiveEntryType) = ARCHIVE_ENTRY_TYPE_SIGNATURE; break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
  }
  if (offset != NULL) (*offset) = chunkHeader.offset;

  // store chunk header for read
  ungetNextChunkHeader(archiveHandle,&chunkHeader);

  return ERROR_NONE;
}

Errors Archive_skipNextEntry(ArchiveHandle *archiveHandle)
{
  Errors      error;
  ChunkHeader chunkHeader;

  assert(archiveHandle != NULL);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // read next chunk
  error = getNextChunkHeader(archiveHandle,&chunkHeader);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // skip entry
  error = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Archive_readMetaEntry(ArchiveHandle *archiveHandle,
                             String        userName,
                             String        hostName,
                             String        jobUUID,
                             String        scheduleUUID,
                             ArchiveTypes  *archiveType,
                             uint64        *createdDateTime,
                             String        comment
                            )
{
  AutoFreeList    autoFreeList1,autoFreeList2;
  Errors          error;
  ChunkMeta       chunkMeta;
  ChunkHeader     chunkHeader;
  CryptAlgorithms cryptAlgorithm;
  uint            blockLength;
  PasswordHandle  passwordHandle;
  const Password  *password;
  bool            passwordFlag;
  CryptInfo       cryptInfo;
  ChunkMetaEntry  chunkMetaEntry;
  uint64          index;
  bool            decryptedFlag;
  ChunkHeader     subChunkHeader;
  bool            foundMetaEntryFlag;

  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  // init meta chunk
  error = Chunk_init(&chunkMeta.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_META,
                     CHUNK_DEFINITION_META,
                     0,  // alignment
                     NULL,  // cryptInfo
                     &chunkMeta
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&chunkMeta.info,{ Chunk_done(&chunkMeta.info); });

  // find next meta chunk
  do
  {
    error = getNextChunkHeader(archiveHandle,&chunkHeader);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList1);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_META)
    {
      error = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList1);
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_META);

  // read meta chunk
  error = Chunk_open(&chunkMeta.info,
                     &chunkHeader,
                     Chunk_getSize(&chunkMeta.info,NULL,0)
                    );
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    (void)Chunk_close(&chunkMeta.info);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&chunkMeta.info,{ Chunk_close(&chunkMeta.info); });
  if (!Crypt_isValidAlgorithm(chunkMeta.cryptAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
  cryptAlgorithm = CRYPT_CONSTANT_TO_ALGORITHM(chunkMeta.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(cryptAlgorithm,&blockLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(blockLength > 0);

  // try to read meta entry with all passwords
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&chunkMeta.info,&index);
  if (Crypt_isEncrypted(cryptAlgorithm))
  {
    if (archiveHandle->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveHandle->cryptPassword != NULL);
      password = archiveHandle->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveHandle,
                                         archiveHandle->jobOptions,
                                         archiveHandle->jobOptions->cryptPasswordMode,
                                         archiveHandle->getPasswordFunction,
                                         archiveHandle->getPasswordUserData
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
  foundMetaEntryFlag = FALSE;
  do
  {
    // reset
    error = ERROR_NONE;
    AutoFree_freeAll(&autoFreeList2);
    Chunk_seek(&chunkMeta.info,index);

    // init meta entry crypt
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&cryptInfo,
                         cryptAlgorithm,
                         password,
                         NULL,  // salt
                         0  // saltLength
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&cryptInfo,{ Crypt_done(&cryptInfo); });
      }
    }

    // init meta entry chunk
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&chunkMetaEntry.info,
                         &chunkMeta.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_META_ENTRY,
                         CHUNK_DEFINITION_META_ENTRY,
                         blockLength,
                         &cryptInfo,
                         &chunkMetaEntry
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&chunkMetaEntry.info,{ Chunk_done(&chunkMetaEntry.info); });
      }
    }

    // try to read meta entry
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&chunkMeta.info)
             && !foundMetaEntryFlag
             && (error == ERROR_NONE)
            )
      {
        error = Chunk_nextSub(&chunkMeta.info,&subChunkHeader);
        if (error != ERROR_NONE)
        {
          break;
        }

        switch (subChunkHeader.id)
        {
          case CHUNK_ID_META_ENTRY:
            // read meta entry chunk
            error = Chunk_open(&chunkMetaEntry.info,
                               &subChunkHeader,
                               subChunkHeader.size
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            // get meta data
            if (userName        != NULL) String_set(userName,chunkMetaEntry.userName);
            if (hostName        != NULL) String_set(hostName,chunkMetaEntry.hostName);
            if (jobUUID         != NULL) String_set(jobUUID,chunkMetaEntry.jobUUID);
            if (scheduleUUID    != NULL) String_set(scheduleUUID,chunkMetaEntry.scheduleUUID);
            if (archiveType     != NULL) (*archiveType) = chunkMetaEntry.archiveType;
            if (createdDateTime != NULL) (*createdDateTime) = chunkMetaEntry.createdDateTime;
            if (comment         != NULL) String_set(comment,chunkMetaEntry.comment);

            // close meta entry chunk
            Chunk_close(&chunkMetaEntry.info);

            foundMetaEntryFlag = TRUE;
            break;
          default:
            // unknown sub-chunk -> skip
            if (isPrintInfo(3))
            {
              printWarning("Skipped unknown sub-chunk '%s' (offset %llu) in '%s'.\n",
                           Chunk_idToString(subChunkHeader.id),
                           subChunkHeader.offset,
                           String_cString(archiveHandle->printableStorageName)
                          );
            }
            error = Chunk_skipSub(&chunkMeta.info,&subChunkHeader);
            if (error != ERROR_NONE)
            {
              break;
            }
            break;
        }
      }
    }

    // free resources
    AutoFree_cleanup(&autoFreeList2);

    if (error != ERROR_NONE)
    {
      if (Crypt_isEncrypted(cryptAlgorithm) && (archiveHandle->cryptType != CRYPT_TYPE_ASYMMETRIC))
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
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AutoFree_done(&autoFreeList2);

  // check if mandatory meta entry chunk found
  if (!foundMetaEntryFlag)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    if      (error != ERROR_NONE)                                return error;
    else if (Crypt_isEncrypted(cryptAlgorithm) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                     return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundMetaEntryFlag)                                return ERROR_NO_META_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  // close
  Chunk_close(&chunkMeta.info);

  // done resources
  Chunk_done(&chunkMeta.info);
  AutoFree_done(&autoFreeList1);

  return ERROR_NONE;

}

#ifdef NDEBUG
  Errors Archive_readFileEntry(ArchiveEntryInfo          *archiveEntryInfo,
                               ArchiveHandle             *archiveHandle,
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
                                 ArchiveHandle             *archiveHandle,
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
  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);
  assert(fileName != NULL);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  if (deltaSourceSize != NULL) (*deltaSourceSize) = 0LL;

  archiveEntryInfo->archiveHandle               = archiveHandle;
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
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
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
    error = getNextChunkHeader(archiveHandle,&chunkHeader);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList1);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_FILE)
    {
      error = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    (void)Chunk_close(&archiveEntryInfo->file.chunkFile.info);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->file.chunkFile.info,{ Chunk_close(&archiveEntryInfo->file.chunkFile.info); });
  if (!Compress_isValidAlgorithm(archiveEntryInfo->file.chunkFile.compressAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_COMPRESS_ALGORITHM;
  }
  archiveEntryInfo->file.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->file.byteCompressAlgorithm  = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->file.chunkFile.compressAlgorithm);
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->file.chunkFile.cryptAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
  archiveEntryInfo->cryptAlgorithm = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->file.chunkFile.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
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
    if (archiveHandle->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveHandle->cryptPassword != NULL);
      password = archiveHandle->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveHandle,
                                         archiveHandle->jobOptions,
                                         archiveHandle->jobOptions->cryptPasswordMode,
                                         archiveHandle->getPasswordFunction,
                                         archiveHandle->getPasswordUserData
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
            if (isPrintInfo(3))
            {
              printWarning("Skipped unknown sub-chunk '%s' (offset %llu) in '%s'.\n",
                           Chunk_idToString(subChunkHeader.id),
                           subChunkHeader.offset,
                           String_cString(archiveHandle->printableStorageName)
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
      if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && (archiveHandle->cryptType != CRYPT_TYPE_ASYMMETRIC))
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  // check if mandatory file entry chunk and file data chunk found
  if (!foundFileEntryFlag || !foundFileDataFlag)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->file.byteCompressInfo,{ Compress_done(&archiveEntryInfo->file.byteCompressInfo); });

  // init variables
  if (deltaCompressAlgorithm != NULL) (*deltaCompressAlgorithm) = archiveEntryInfo->file.deltaCompressAlgorithm;
  if (byteCompressAlgorithm  != NULL) (*byteCompressAlgorithm)  = archiveEntryInfo->file.byteCompressAlgorithm;
  if (cryptAlgorithm         != NULL) (*cryptAlgorithm)         = archiveEntryInfo->cryptAlgorithm;
  if (cryptType              != NULL) (*cryptType)              = archiveHandle->cryptType;

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
                                ArchiveHandle      *archiveHandle,
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
                                  ArchiveHandle      *archiveHandle,
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
  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);
  assert(deviceInfo != NULL);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  if (deltaSourceSize != NULL) (*deltaSourceSize) = 0LL;

  archiveEntryInfo->archiveHandle                = archiveHandle;
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
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
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
    error = getNextChunkHeader(archiveHandle,&chunkHeader);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList1);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_IMAGE)
    {
      error = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->image.chunkImage.info,{ Chunk_close(&archiveEntryInfo->image.chunkImage.info); });
  if (!Compress_isValidAlgorithm(archiveEntryInfo->image.chunkImage.compressAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_COMPRESS_ALGORITHM;
  }
  archiveEntryInfo->image.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->image.byteCompressAlgorithm  = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->image.chunkImage.compressAlgorithm);
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->image.chunkImage.cryptAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
  archiveEntryInfo->cryptAlgorithm = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->image.chunkImage.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
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
    if (archiveHandle->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveHandle->cryptPassword != NULL);
      password = archiveHandle->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveHandle,
                                         archiveHandle->jobOptions,
                                         archiveHandle->jobOptions->cryptPasswordMode,
                                         archiveHandle->getPasswordFunction,
                                         archiveHandle->getPasswordUserData
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
            if (isPrintInfo(3))
            {
              printWarning("Skipped unknown sub-chunk '%s' (offset %llu) in '%s'.\n",
                           Chunk_idToString(subChunkHeader.id),
                           subChunkHeader.offset,
                           String_cString(archiveHandle->printableStorageName)
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
      if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && (archiveHandle->cryptType != CRYPT_TYPE_ASYMMETRIC))
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  if (!foundImageEntryFlag || !foundImageDataFlag)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->image.byteCompressInfo,{ Compress_done(&archiveEntryInfo->image.byteCompressInfo); });

  // init variables
  if (deltaCompressAlgorithm != NULL) (*deltaCompressAlgorithm) = archiveEntryInfo->image.deltaCompressAlgorithm;
  if (byteCompressAlgorithm  != NULL) (*byteCompressAlgorithm)  = archiveEntryInfo->image.byteCompressAlgorithm;
  if (cryptAlgorithm         != NULL) (*cryptAlgorithm)         = archiveEntryInfo->cryptAlgorithm;
  if (cryptType              != NULL) (*cryptType)              = archiveHandle->cryptType;

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
                                    ArchiveHandle             *archiveHandle,
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
                                      ArchiveHandle             *archiveHandle,
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
  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  archiveEntryInfo->archiveHandle      = archiveHandle;
  archiveEntryInfo->mode             = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_DIRECTORY;

  // init directory chunk
  error = Chunk_init(&archiveEntryInfo->directory.chunkDirectory.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
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
    error = getNextChunkHeader(archiveHandle,&chunkHeader);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList1);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_DIRECTORY)
    {
      error = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->directory.chunkDirectory.info,{ Chunk_close(&archiveEntryInfo->directory.chunkDirectory.info); });
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->directory.chunkDirectory.cryptAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
  archiveEntryInfo->cryptAlgorithm = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->directory.chunkDirectory.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  // try to read directory entry with all passwords
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->directory.chunkDirectory.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm))
  {
    if (archiveHandle->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveHandle->cryptPassword != NULL);
      password = archiveHandle->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveHandle,
                                         archiveHandle->jobOptions,
                                         archiveHandle->jobOptions->cryptPasswordMode,
                                         CALLBACK(archiveHandle->getPasswordFunction,archiveHandle->getPasswordUserData)
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
            if (isPrintInfo(3))
            {
              printWarning("Skipped unknown sub-chunk '%s' (offset %llu).\n",
                           Chunk_idToString(subChunkHeader.id),
                           subChunkHeader.offset,
                           String_cString(archiveHandle->printableStorageName)
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
      if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && (archiveHandle->cryptType != CRYPT_TYPE_ASYMMETRIC))
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  if (!foundDirectoryEntryFlag)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    if      (error != ERROR_NONE)                                                  return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                       return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundDirectoryEntryFlag)                                             return ERROR_NO_DIRECTORY_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveEntryInfo->cryptAlgorithm;
  if (cryptType      != NULL) (*cryptType)      = archiveHandle->cryptType;

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
                               ArchiveHandle             *archiveHandle,
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
                                 ArchiveHandle             *archiveHandle,
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
  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  archiveEntryInfo->archiveHandle      = archiveHandle;
  archiveEntryInfo->mode             = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_LINK;

  // init link chunk
  error = Chunk_init(&archiveEntryInfo->link.chunkLink.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
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
    error = getNextChunkHeader(archiveHandle,&chunkHeader);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList1);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_LINK)
    {
      if (error != ERROR_NONE)
      {
        archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
        AutoFree_cleanup(&autoFreeList1);
        return error;
      }
      archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->link.chunkLink.info,{ Chunk_close(&archiveEntryInfo->link.chunkLink.info); });
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->link.chunkLink.cryptAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
  archiveEntryInfo->cryptAlgorithm = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->link.chunkLink.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  // try to read link entry with all passwords
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->link.chunkLink.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm))
  {
    if (archiveHandle->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveHandle->cryptPassword != NULL);
      password = archiveHandle->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveHandle,
                                         archiveHandle->jobOptions,
                                         archiveHandle->jobOptions->cryptPasswordMode,
                                         archiveHandle->getPasswordFunction,
                                         archiveHandle->getPasswordUserData
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
            if (isPrintInfo(3))
            {
              printWarning("Skipped unknown sub-chunk '%s' (offset %llu).\n",
                           Chunk_idToString(subChunkHeader.id),
                           subChunkHeader.offset,
                           String_cString(archiveHandle->printableStorageName)
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
      if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && (archiveHandle->cryptType != CRYPT_TYPE_ASYMMETRIC))
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  if (!foundLinkEntryFlag)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    if      (error != ERROR_NONE)                                                  return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                       return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundLinkEntryFlag)                                                  return ERROR_NO_LINK_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveEntryInfo->cryptAlgorithm;
  if (cryptType      != NULL) (*cryptType)      = archiveHandle->cryptType;

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
                                   ArchiveHandle             *archiveHandle,
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
                                     ArchiveHandle             *archiveHandle,
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
  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);
  assert(fileNameList != NULL);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  if (deltaSourceSize != NULL) (*deltaSourceSize) = 0LL;

  archiveEntryInfo->archiveHandle                   = archiveHandle;
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
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
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
    error = getNextChunkHeader(archiveHandle,&chunkHeader);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList1);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_HARDLINK)
    {
      error = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->hardLink.chunkHardLink.info,{ Chunk_close(&archiveEntryInfo->hardLink.chunkHardLink.info); });
  if (!Compress_isValidAlgorithm(archiveEntryInfo->hardLink.chunkHardLink.compressAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_COMPRESS_ALGORITHM;
  }
  archiveEntryInfo->hardLink.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->hardLink.byteCompressAlgorithm  = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->hardLink.chunkHardLink.compressAlgorithm);
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
  archiveEntryInfo->cryptAlgorithm = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
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
    if (archiveHandle->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveHandle->cryptPassword != NULL);
      password = archiveHandle->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveHandle,
                                         archiveHandle->jobOptions,
                                         archiveHandle->jobOptions->cryptPasswordMode,
                                         archiveHandle->getPasswordFunction,
                                         archiveHandle->getPasswordUserData
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
            if (isPrintInfo(3))
            {
              printWarning("Skipped unknown sub-chunk '%s' (offset %llu).\n",
                           Chunk_idToString(subChunkHeader.id),
                           subChunkHeader.offset,
                           String_cString(archiveHandle->printableStorageName)
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
      if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && (archiveHandle->cryptType != CRYPT_TYPE_ASYMMETRIC))
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  if (!foundHardLinkEntryFlag || !foundHardLinkDataFlag)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  // init variables
  if (deltaCompressAlgorithm != NULL) (*deltaCompressAlgorithm) = archiveEntryInfo->hardLink.deltaCompressAlgorithm;
  if (byteCompressAlgorithm  != NULL) (*byteCompressAlgorithm)  = archiveEntryInfo->hardLink.byteCompressAlgorithm;
  if (cryptAlgorithm         != NULL) (*cryptAlgorithm)         = archiveEntryInfo->cryptAlgorithm;
  if (cryptType              != NULL) (*cryptType)              = archiveHandle->cryptType;

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
                                  ArchiveHandle             *archiveHandle,
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
                                    ArchiveHandle             *archiveHandle,
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
  assert(archiveHandle != NULL);
  assert(archiveHandle->jobOptions != NULL);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  archiveEntryInfo->archiveHandle      = archiveHandle;
  archiveEntryInfo->mode             = ARCHIVE_MODE_READ;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_SPECIAL;

  // init special chunk
  error = Chunk_init(&archiveEntryInfo->special.chunkSpecial.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
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
    error = getNextChunkHeader(archiveHandle,&chunkHeader);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList1);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_SPECIAL)
    {
      if (error != ERROR_NONE)
      {
        archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
        AutoFree_cleanup(&autoFreeList1);
        return error;
      }
      archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->special.chunkSpecial.info,{ Chunk_close(&archiveEntryInfo->special.chunkSpecial.info); });
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->special.chunkSpecial.cryptAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
  archiveEntryInfo->cryptAlgorithm = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->special.chunkSpecial.cryptAlgorithm);

  // detect block length of used crypt algorithm
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithm,&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);

  // try to read special entry with all passwords
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->special.chunkSpecial.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm))
  {
    if (archiveHandle->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      assert(archiveHandle->cryptPassword != NULL);
      password = archiveHandle->cryptPassword;
    }
    else
    {
      password = getFirstDecryptPassword(&passwordHandle,
                                         archiveHandle,
                                         archiveHandle->jobOptions,
                                         archiveHandle->jobOptions->cryptPasswordMode,
                                         archiveHandle->getPasswordFunction,
                                         archiveHandle->getPasswordUserData
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
                         password,
                         NULL,  // salt
                         0  // saltLength
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
            if (isPrintInfo(3))
            {
              printWarning("Skipped unknown sub-chunk '%s' (offset %llu).\n",
                           Chunk_idToString(subChunkHeader.id),
                           subChunkHeader.offset,
                           String_cString(archiveHandle->printableStorageName)
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
      if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && (archiveHandle->cryptType != CRYPT_TYPE_ASYMMETRIC))
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
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  if (!foundSpecialEntryFlag)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    if      (error != ERROR_NONE)                                                  return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithm) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                       return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundSpecialEntryFlag)                                               return ERROR_NO_SPECIAL_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveEntryInfo->cryptAlgorithm;
  if (cryptType      != NULL) (*cryptType)      = archiveHandle->cryptType;

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
  assert(archiveEntryInfo->archiveHandle != NULL);
  assert(archiveEntryInfo->archiveHandle->jobOptions != NULL);

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
            if (!archiveEntryInfo->archiveHandle->jobOptions->dryRunFlag)
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
              SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveEntryInfo->archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
              {
                // create archive file (if not already exists and open)
                tmpError = createArchiveFile(archiveEntryInfo->archiveHandle,archiveEntryInfo->indexHandle);
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
                  break;
                }

                // transfer intermediate data into archive
                tmpError = transferArchiveFileData(archiveEntryInfo->archiveHandle,
                                                   &archiveEntryInfo->file.intermediateFileHandle
                                                  );
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
                  break;
                }
              }

              // store in index database
              if (error == ERROR_NONE)
              {
                if (   (archiveEntryInfo->indexHandle != NULL)
                    && (archiveEntryInfo->archiveHandle->storageId != DATABASE_ID_NONE)
                   )
                {
                  error = Index_addFile(archiveEntryInfo->indexHandle,
                                        archiveEntryInfo->archiveHandle->storageId,
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
            if (!archiveEntryInfo->archiveHandle->jobOptions->dryRunFlag)
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
              SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveEntryInfo->archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
              {
                // create archive file (if not already exists and open)
                tmpError = createArchiveFile(archiveEntryInfo->archiveHandle,archiveEntryInfo->indexHandle);
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
                  break;
                }

                // transfer intermediate data into archive
                tmpError = transferArchiveFileData(archiveEntryInfo->archiveHandle,
                                                   &archiveEntryInfo->image.intermediateFileHandle
                                                  );
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
                  break;
                }
              }

              // store in index database
              if (error == ERROR_NONE)
              {
                if (   (archiveEntryInfo->indexHandle != NULL)
                    && (archiveEntryInfo->archiveHandle->storageId != DATABASE_ID_NONE)
                   )
                {
                  error = Index_addImage(archiveEntryInfo->indexHandle,
                                         archiveEntryInfo->archiveHandle->storageId,
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
            if (!archiveEntryInfo->archiveHandle->jobOptions->dryRunFlag)
            {
              // close chunks
              tmpError = Chunk_close(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              tmpError = Chunk_close(&archiveEntryInfo->directory.chunkDirectory.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

              // unlock archive
              Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);

              if (   (archiveEntryInfo->indexHandle != NULL)
                  && (archiveEntryInfo->archiveHandle->storageId != DATABASE_ID_NONE)
                 )
              {
                // store in index database
                if (error == ERROR_NONE)
                {
                  error = Index_addDirectory(archiveEntryInfo->indexHandle,
                                             archiveEntryInfo->archiveHandle->storageId,
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
            if (!archiveEntryInfo->archiveHandle->jobOptions->dryRunFlag)
            {
              // close chunks
              tmpError = Chunk_close(&archiveEntryInfo->link.chunkLinkEntry.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              tmpError = Chunk_close(&archiveEntryInfo->link.chunkLink.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

              // unlock archive
              Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);

              // store in index database
              if (error == ERROR_NONE)
              {
                if (   (archiveEntryInfo->indexHandle != NULL)
                    && (archiveEntryInfo->archiveHandle->storageId != DATABASE_ID_NONE)
                   )
                {
                 error = Index_addLink(archiveEntryInfo->indexHandle,
                                        archiveEntryInfo->archiveHandle->storageId,
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

            if (!archiveEntryInfo->archiveHandle->jobOptions->dryRunFlag)
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
              SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveEntryInfo->archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
              {
                // create archive file (if not already exists and open)
                tmpError = createArchiveFile(archiveEntryInfo->archiveHandle,archiveEntryInfo->indexHandle);
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
                  break;
                }

                // transfer intermediate data into archive
                tmpError = transferArchiveFileData(archiveEntryInfo->archiveHandle,
                                                   &archiveEntryInfo->hardLink.intermediateFileHandle
                                                  );
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);
                  break;
                }
              }

              if (error == ERROR_NONE)
              {
                if (   (archiveEntryInfo->indexHandle != NULL)
                    && (archiveEntryInfo->archiveHandle->storageId != DATABASE_ID_NONE)
                   )
                {
                  // store in index database
                  STRINGLIST_ITERATE(archiveEntryInfo->hardLink.fileNameList,stringNode,fileName)
                  {
                    error = Index_addFile(archiveEntryInfo->indexHandle,
                                          archiveEntryInfo->archiveHandle->storageId,
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
            if (!archiveEntryInfo->archiveHandle->jobOptions->dryRunFlag)
            {
              // close chunks
              tmpError = Chunk_close(&archiveEntryInfo->special.chunkSpecialEntry.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              tmpError = Chunk_close(&archiveEntryInfo->special.chunkSpecial.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

              // unlock archive
              Semaphore_unlock(&archiveEntryInfo->archiveHandle->chunkIOLock);

              if (   (archiveEntryInfo->indexHandle != NULL)
                  && (archiveEntryInfo->archiveHandle->storageId != DATABASE_ID_NONE)
                 )
              {
                // store in index database
                if (error == ERROR_NONE)
                {
                  error = Index_addSpecial(archiveEntryInfo->indexHandle,
                                           archiveEntryInfo->archiveHandle->storageId,
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

  if (!archiveEntryInfo->archiveHandle->jobOptions->dryRunFlag)
  {
    archiveEntryInfo->archiveHandle->entries++;
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
  assert(archiveEntryInfo->archiveHandle != NULL);
  assert(archiveEntryInfo->archiveHandle->jobOptions != NULL);
  assert(elementSize > 0);

  if (!archiveEntryInfo->archiveHandle->jobOptions->dryRunFlag)
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
                                      archiveEntryInfo->archiveHandle->deltaSourceList,
                                      archiveEntryInfo->file.chunkFileDelta.name,
                                      archiveEntryInfo->file.chunkFileEntry.name,
                                      archiveEntryInfo->file.chunkFileDelta.size,
                                      archiveEntryInfo->archiveHandle->jobOptions
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
                                      archiveEntryInfo->archiveHandle->deltaSourceList,
                                      archiveEntryInfo->image.chunkImageDelta.name,
                                      archiveEntryInfo->image.chunkImageEntry.name,
                                      archiveEntryInfo->image.chunkImageDelta.size,
                                      archiveEntryInfo->archiveHandle->jobOptions
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
                                      archiveEntryInfo->archiveHandle->deltaSourceList,
                                      archiveEntryInfo->hardLink.chunkHardLinkDelta.name,
                                      StringList_first(archiveEntryInfo->hardLink.fileNameList,NULL),
                                      archiveEntryInfo->hardLink.chunkHardLinkDelta.size,
                                      archiveEntryInfo->archiveHandle->jobOptions
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
  assert(archiveEntryInfo->archiveHandle != NULL);

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

uint64 Archive_tell(ArchiveHandle *archiveHandle)
{
  SemaphoreLock semaphoreLock;
  uint64        offset;

  assert(archiveHandle != NULL);
  assert(archiveHandle->chunkIO != NULL);
  assert(archiveHandle->chunkIO->tell != NULL);
  assert(archiveHandle->jobOptions != NULL);

  offset = 0LL;
  if (!archiveHandle->jobOptions->dryRunFlag)
  {
    switch (archiveHandle->ioType)
    {
      case ARCHIVE_IO_TYPE_FILE:
        SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          if (archiveHandle->file.openFlag)
          {
            archiveHandle->chunkIO->tell(archiveHandle->chunkIOUserData,&offset);
          }
          else
          {
            offset = 0LL;
          }
        }
        break;
      case ARCHIVE_IO_TYPE_STORAGE_FILE:
        archiveHandle->chunkIO->tell(archiveHandle->chunkIOUserData,&offset);
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

Errors Archive_seek(ArchiveHandle *archiveHandle,
                    uint64        offset
                   )
{
  SemaphoreLock semaphoreLock;
  Errors        error;

  assert(archiveHandle != NULL);
  assert(archiveHandle->chunkIO != NULL);
  assert(archiveHandle->chunkIO->seek != NULL);

  error = ERROR_NONE;

  if (!archiveHandle->jobOptions->dryRunFlag)
  {
    switch (archiveHandle->ioType)
    {
      case ARCHIVE_IO_TYPE_FILE:
        SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          if (archiveHandle->file.openFlag)
          {
            error = archiveHandle->chunkIO->seek(archiveHandle->chunkIOUserData,offset);
          }
        }
        break;
      case ARCHIVE_IO_TYPE_STORAGE_FILE:
        error = archiveHandle->chunkIO->seek(archiveHandle->chunkIOUserData,offset);
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
    archiveHandle->nextChunkHeaderReadFlag = FALSE;
  }

  return error;
}

uint64 Archive_getSize(ArchiveHandle *archiveHandle)
{
  SemaphoreLock semaphoreLock;
  uint64        size;

  assert(archiveHandle != NULL);
  assert(archiveHandle->chunkIO != NULL);
  assert(archiveHandle->chunkIO->getSize != NULL);
  assert(archiveHandle->jobOptions != NULL);

  size = 0LL;
  if (!archiveHandle->jobOptions->dryRunFlag)
  {
    switch (archiveHandle->ioType)
    {
      case ARCHIVE_IO_TYPE_FILE:
        SEMAPHORE_LOCKED_DO(semaphoreLock,&archiveHandle->chunkIOLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          size = (archiveHandle->file.openFlag)
                   ? archiveHandle->chunkIO->getSize(archiveHandle->chunkIOUserData)
                   : 0LL;
        }
        break;
      case ARCHIVE_IO_TYPE_STORAGE_FILE:
        size = archiveHandle->chunkIO->getSize(archiveHandle->chunkIOUserData);
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
                          StorageInfo      *storageInfo,
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
  assert(storageInfo != NULL);

  // create new storage index
  error = Index_newStorage(indexHandle,
                           DATABASE_ID_NONE, // entityId
                           Storage_getPrintableName(&storageInfo->storageSpecifier,NULL),
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
                              storageInfo,
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
    (void)Index_deleteStorage(indexHandle,storageId);
    return error;
  }

  return ERROR_NONE;
}

Errors Archive_updateIndex(IndexHandle                  *indexHandle,
                           IndexId                      storageId,
                           StorageInfo                  *storageInfo,
                           const JobOptions             *jobOptions,
                           uint64                       *totalTimeLastChanged,
                           uint64                       *totalEntries,
                           uint64                       *totalSize,
                           ArchivePauseCallbackFunction pauseCallbackFunction,
                           void                         *pauseCallbackUserData,
                           ArchiveAbortCallbackFunction abortCallbackFunction,
                           void                         *abortCallbackUserData,
                           LogHandle                    *logHandle
                          )
{
//TODO: remove?
#define _ARCHIVE_UPDATE_INDEX_WITH_TRANSACTION

  StorageSpecifier  storageSpecifier;
  String            printableStorageName;
  Errors            error;
  uint64            timeLastChanged;
  bool              abortedFlag,serverAllocationPendingFlag;
  ArchiveHandle     archiveHandle;
  ArchiveEntryInfo  archiveEntryInfo;
  ArchiveEntryTypes archiveEntryType;
  String            fileName;
  String            imageName;
  String            directoryName;
  String            linkName;
  String            destinationName;

  assert(indexHandle != NULL);
  assert(storageInfo != NULL);

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  printableStorageName = String_new();

  // get printable name
  String_set(printableStorageName,Storage_getPrintableName(&storageInfo->storageSpecifier,NULL));

  // open archive (Note optimization: try sftp for scp protocol, because sftp support seek()-operation)
  if (storageSpecifier.type == STORAGE_TYPE_SCP)
  {
    // try to open scp-storage first with sftp
    storageSpecifier.type = STORAGE_TYPE_SFTP;
    error = Archive_open(&archiveHandle,
                         storageInfo,
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
      error = Archive_open(&archiveHandle,
                           storageInfo,
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
    error = Archive_open(&archiveHandle,
                         storageInfo,
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

  // set state 'update'
  Index_setState(indexHandle,
                 storageId,
                 INDEX_STATE_UPDATE,
                 0LL,
                 NULL
                );

  // index archive contents
  printInfo(4,"Create index for '%s'\n",String_cString(printableStorageName));
#if ARCHIVE_UPDATE_INDEX_WITH_TRANSACTION
  error = Index_beginTransaction(indexHandle,INDEX_TIMEOUT);
  if (error != ERROR_NONE)
  {
    Archive_close(&archiveHandle);
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
#endif

  // clear index
  error = Index_clearStorage(indexHandle,
                             storageId
                            );
  if (error != ERROR_NONE)
  {
    printInfo(4,"Failed to create index for '%s' (error: %s)\n",String_cString(printableStorageName),Error_getText(error));

#if ARCHIVE_UPDATE_INDEX_WITH_TRANSACTION
    (void)Index_rollbackTransaction(indexHandle);
#endif
    Archive_close(&archiveHandle);
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

  // read archive content
  timeLastChanged             = 0LL;
  abortedFlag                 = (abortCallbackFunction != NULL) && abortCallbackFunction(abortCallbackUserData);
  serverAllocationPendingFlag = Storage_isServerAllocationPending(storageInfo);
  fileName        = String_new();
  imageName       = String_new();
  directoryName   = String_new();
  linkName        = String_new();
  destinationName = String_new();
  while (   !Archive_eof(&archiveHandle,FALSE)
         && (error == ERROR_NONE)
         && !abortedFlag
         && !serverAllocationPendingFlag
        )
  {
//TODO
//fprintf(stderr,"%s, %d: add i\n",__FILE__,__LINE__); Misc_udelay(1L*US_PER_SECOND);
#if 1
    // pause
    if ((pauseCallbackFunction != NULL) && pauseCallbackFunction(pauseCallbackUserData))
    {
#if ARCHIVE_UPDATE_INDEX_WITH_TRANSACTION
      // end transaction
      error = Index_endTransaction(indexHandle);
      if (error != ERROR_NONE)
      {
        break;
      }
#endif

#if 0
      // temporarly close storage
      error = Archive_storageInterrupt(&archiveHandle);
      if (error != ERROR_NONE)
      {
        break;
      }
#endif /* 0 */

      // wait
      do
      {
        Misc_udelay(10LL*US_PER_SECOND);
      }
      while (pauseCallbackFunction(pauseCallbackUserData));

#if 0
      // reopen temporary closed storage
      error = Archive_storageContinue(&archiveHandle);
      if (error != ERROR_NONE)
      {
        break;
      }
#endif /* 0 */

#if ARCHIVE_UPDATE_INDEX_WITH_TRANSACTION
      // start transaction
      error = Index_beginTransaction(indexHandle,INDEX_TIMEOUT);
      if (error != ERROR_NONE)
      {
        break;
      }
#endif
    }
#endif

    // get next file type
    error = Archive_getNextArchiveEntry(&archiveHandle,
                                        &archiveEntryType,
                                        NULL,  // offset
                                        FALSE
                                       );
    if (error != ERROR_NONE)
    {
      break;
    }

    // read entry
    switch (archiveEntryType)
    {
      case ARCHIVE_ENTRY_TYPE_FILE:
        {
          ArchiveEntryInfo archiveEntryInfo;
          FileInfo         fileInfo;
          uint64           fragmentOffset,fragmentSize;

          // open archive file
          error = Archive_readFileEntry(&archiveEntryInfo,
                                        &archiveHandle,
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
            break;
          }

          // save max. time last changed
          if (timeLastChanged < fileInfo.timeLastChanged) timeLastChanged = fileInfo.timeLastChanged;

          pprintInfo(4,"INDEX: ","Added file '%s', %lubytes to index for '%s'\n",String_cString(fileName),fileInfo.size,String_cString(printableStorageName));

          // close archive file, free resources
          (void)Archive_closeEntry(&archiveEntryInfo);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        {
          ArchiveEntryInfo archiveEntryInfo;
          DeviceInfo       deviceInfo;
          FileSystemTypes  fileSystemType;
          uint64           blockOffset,blockCount;

          // open archive file
          error = Archive_readImageEntry(&archiveEntryInfo,
                                         &archiveHandle,
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
            break;
          }
          pprintInfo(4,"INDEX: ","Added image '%s', %lubytes to index for '%s'\n",String_cString(imageName),deviceInfo.size,String_cString(printableStorageName));

          // close archive file, free resources
          (void)Archive_closeEntry(&archiveEntryInfo);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        {
          FileInfo fileInfo;

          // open archive directory
          error = Archive_readDirectoryEntry(&archiveEntryInfo,
                                             &archiveHandle,
                                             NULL,  // cryptAlgorithm
                                             NULL,  // cryptType
                                             directoryName,
                                             &fileInfo,
                                             NULL  // fileExtendedAttributeList
                                            );
          if (error != ERROR_NONE)
          {
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
            break;
          }

          // save max. time last changed
          if (timeLastChanged < fileInfo.timeLastChanged) timeLastChanged = fileInfo.timeLastChanged;

          pprintInfo(4,"INDEX: ","Added directory '%s' to index for '%s'\n",String_cString(directoryName),String_cString(printableStorageName));

          // close archive file, free resources
          (void)Archive_closeEntry(&archiveEntryInfo);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_LINK:
        {
          FileInfo fileInfo;

          // open archive link
          error = Archive_readLinkEntry(&archiveEntryInfo,
                                        &archiveHandle,
                                        NULL,  // cryptAlgorithm
                                        NULL,  // cryptType
                                        linkName,
                                        destinationName,
                                        &fileInfo,
                                        NULL   // fileExtendedAttributeList
                                       );
          if (error != ERROR_NONE)
          {
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
            break;
          }

          // save max. time last changed
          if (timeLastChanged < fileInfo.timeLastChanged) timeLastChanged = fileInfo.timeLastChanged;

          pprintInfo(4,"INDEX: ","Added link '%s' to index for '%s'\n",String_cString(linkName),String_cString(printableStorageName));

          // close archive file, free resources
          (void)Archive_closeEntry(&archiveEntryInfo);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        {
          StringList       fileNameList;
          String           fileName;
          ArchiveEntryInfo archiveEntryInfo;
          FileInfo         fileInfo;
          uint64           fragmentOffset,fragmentSize;
          const StringNode *stringNode;

          // open archive file
          StringList_init(&fileNameList);
          error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                            &archiveHandle,
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
          FileInfo fileInfo;

          // open archive link
          error = Archive_readSpecialEntry(&archiveEntryInfo,
                                           &archiveHandle,
                                           NULL,  // cryptAlgorithm
                                           NULL,  // cryptType
                                           fileName,
                                           &fileInfo,
                                           NULL   // fileExtendedAttributeList
                                          );
          if (error != ERROR_NONE)
          {
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
            break;
          }

          // save max. time last changed
          if (timeLastChanged < fileInfo.timeLastChanged) timeLastChanged = fileInfo.timeLastChanged;

          pprintInfo(4,"INDEX: ","Added special '%s' to index for '%s'\n",String_cString(fileName),String_cString(printableStorageName));

          // close archive file, free resources
          (void)Archive_closeEntry(&archiveEntryInfo);
        }
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; /* not reached */
    }
    if (error != ERROR_NONE)
    {
      break;
    }

#if 0
    // update temporary entries, size (ignore error)
    Index_storageUpdate(indexHandle,
                        storageId,
                        NULL,  // storageName
                        Archive_getSize(&archiveHandle)
                       );
#endif

#if 1
    if ((pauseCallbackFunction != NULL) && pauseCallbackFunction(pauseCallbackUserData))
    {
#if ARCHIVE_UPDATE_INDEX_WITH_TRANSACTION
      // end transaction
      error = Index_endTransaction(indexHandle);
      if (error != ERROR_NONE)
      {
        break;
      }
#endif

#if 0
      // temporarly close storage
      error = Archive_storageInterrupt(&archiveHandle);
      if (error != ERROR_NONE)
      {
        break;
      }
#endif /* 0 */

      // wait
      do
      {
        Misc_udelay(10LL*US_PER_SECOND);
      }
      while (pauseCallbackFunction(pauseCallbackUserData));

#if 0
      // reopen temporary closed storage
      error = Archive_storageContinue(&archiveHandle);
      if (error != ERROR_NONE)
      {
        break;
      }
#endif /* 0 */

#if ARCHIVE_UPDATE_INDEX_WITH_TRANSACTION
      // start transacation
      error = Index_beginTransaction(indexHandle,INDEX_TIMEOUT);
      if (error != ERROR_NONE)
      {
        break;
      }
#endif
    }
#endif

    // check if aborted, check if server allocation pending
    abortedFlag                 = (abortCallbackFunction != NULL) && abortCallbackFunction(abortCallbackUserData);
    serverAllocationPendingFlag = Storage_isServerAllocationPending(storageInfo);
  }
  String_delete(destinationName);
  String_delete(linkName);
  String_delete(directoryName);
  String_delete(imageName);
  String_delete(fileName);
#if ARCHIVE_UPDATE_INDEX_WITH_TRANSACTION
  if (error == ERROR_NONE)
  {
    error = Index_endTransaction(indexHandle);
  }
  else
  {
    (void)Index_rollbackTransaction(indexHandle);
  }
#endif

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
                   0LL,  // lastCheckedTimestamp
                   "aborted"
                  );

    error = ERROR_ABORTED;
  }
  else if (serverAllocationPendingFlag)
  {
    printInfo(4,"Interrupted create index for '%s'\n",String_cString(printableStorageName));

    // set index state
    Index_setState(indexHandle,
                   storageId,
                   INDEX_STATE_UPDATE_REQUESTED,
                   0LL,  // lastCheckedTimestamp
                   NULL  // error text
                  );

    error = ERROR_INTERRUPTED;
  }
  else
  {
    printInfo(4,"Done create index for '%s'\n",String_cString(printableStorageName));

    // update storages info (aggregated values)
    if (error == ERROR_NONE)
    {
      error = Index_updateStorageInfos(indexHandle,
                                       storageId
                                      );
    }

    // update name/size
    if (error == ERROR_NONE)
    {
      error = Index_storageUpdate(indexHandle,
                                  storageId,
                                  printableStorageName,
                                  Archive_getSize(&archiveHandle)
                                 );
    }

    // set index state, last checked time
    if (error == ERROR_NONE)
    {
      // set index state 'OK', last checked time
      Index_setState(indexHandle,
                     storageId,
                     INDEX_STATE_OK,
                     Misc_getCurrentDateTime(),
                     NULL
                    );
    }
    else
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

    // get total time last changed/entries/size
    if (totalTimeLastChanged != NULL) (*totalTimeLastChanged) = timeLastChanged;
    if (totalEntries != NULL) (*totalEntries) = Archive_getEntries(&archiveHandle);
    if (totalSize != NULL) (*totalSize) = Archive_getSize(&archiveHandle);
  }

  // close archive
  Archive_close(&archiveHandle);

  // free resources
  String_delete(printableStorageName);
  Storage_doneSpecifier(&storageSpecifier);

  return error;
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
  Errors        error;
  ArchiveHandle sourceArchiveHandle;
  ArchiveHandle destinationArchiveHandle;
  Errors        failError;

  // open source
  error = Archive_open(&sourceArchiveHandle,
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
  error = Archive_create(&destinationArchiveHandle,
//                        DeltaSourceList                 *deltaSourceList,
                         jobOptions,
archiveHandle->archiveInitFunction              = NULL;
archiveHandle->archiveInitUserData              = NULL;
                         archiveStoreFunction          archiveStoreFunction,
                         void                            *archiveNewFileUserData,
                         CALLBACK(NULL,NULL)
                         NULL, // indexHandle
                        );
  if (error != ERROR_NONE)
  {
    Archive_close(&sourceArchiveHandle);
    return error;
  }

  // copy archive entries
  failError = ERROR_NONE;
  while (   //((restoreInfo.requestedAbortFlag == NULL) || !(*restoreInfo.requestedAbortFlag))
         && !Archive_eof(&sourceArchiveHandle,TRUE)
         && (failError == ERROR_NONE)
        )
  {
#if 0
    // pause
    while ((restoreInfo.pauseFlag != NULL) && (*restoreInfo.pauseFlag))
    {
      Misc_udelay(500L*US_PER_MS);
    }
#endif /* 0 */

    // get next archive entry type
    error = Archive_getNextArchiveEntry(&archiveHandle,
                                        &archiveEntryType,
                                        NULL,  // offset
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
                                        &archiveHandle,
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
              Misc_udelay(500L*US_PER_MS);
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
                                         &archiveHandle,
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
                Misc_udelay(500L*US_PER_MS);
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
              printInfo(2,"OK\n");
            }
            else
            {
              printInfo(2,"OK (dry-run)\n");
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
                                             &archiveHandle,
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
              printInfo(2,"OK\n");
            }
            else
            {
              printInfo(2,"OK (dry-run)\n");
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
                                        &archiveHandle,
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
              printInfo(2,"OK\n");
            }
            else
            {
              printInfo(2,"OK (dry-run)\n");
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
                                            &archiveHandle,
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
                    Misc_udelay(500L*US_PER_MS);
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
                  printInfo(2,"OK\n");
                }
                else
                {
                  printInfo(2,"OK (dry-run)\n");
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
                  printInfo(2,"OK\n");
                }
                else
                {
                  printInfo(2,"OK (dry-run)\n");
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
                                           &archiveHandle,
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
              printInfo(2,"OK\n");
            }
            else
            {
              printInfo(2,"OK (dry-run)\n");
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
  Archive_close(&destinationArchiveHandle);
  Archive_close(&sourceArchiveHandle);

  return ERROR_NONE;
}
#endif /* 0 */

#ifdef __cplusplus
  }
#endif

/* end of file */
