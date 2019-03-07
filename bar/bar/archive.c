/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive functions
* Systems: all
*
\***********************************************************************/

#define __ARCHIVE_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "common/global.h"
#include "common/autofree.h"
#include "common/strings.h"
#include "common/lists.h"
#include "common/files.h"
#include "common/devices.h"
#include "common/semaphores.h"
#include "common/passwords.h"

#include "errors.h"
#include "chunks.h"
#include "compress.h"
#include "crypt.h"
#include "archive_format.h"
#include "storage.h"
#include "index.h"
#include "bar_global.h"
#include "bar.h"

#include "archive.h"

/****************** Conditional compilation switches *******************/
#define SUPPORT_BAR0_CHUNK       // require simple key, CTS, no salt
// NYI: multi crypt support
#define _MULTI_CRYPT

/***************************** Constants *******************************/

// archive types
LOCAL const struct
{
  const char   *name;
  const char   *shortName;
  ArchiveTypes archiveType;
} ARCHIVE_TYPES[] =
{
  { "normal",       "N", ARCHIVE_TYPE_NORMAL       },
  { "full",         "F", ARCHIVE_TYPE_FULL         },
  { "incremental",  "I", ARCHIVE_TYPE_INCREMENTAL  },
  { "differential", "D", ARCHIVE_TYPE_DIFFERENTIAL },
  { "continuous",   "C", ARCHIVE_TYPE_CONTINUOUS   }
};

// archive entry types
LOCAL const struct
{
  const char        *name;
  ArchiveEntryTypes archiveEntryType;
} ARCHIVE_ENTRY_TYPES[] =
{
  {"file",     ARCHIVE_ENTRY_TYPE_FILE     },
  {"image",    ARCHIVE_ENTRY_TYPE_IMAGE    },
  {"directory",ARCHIVE_ENTRY_TYPE_DIRECTORY},
  {"link",     ARCHIVE_ENTRY_TYPE_LINK     },
  {"hardlink", ARCHIVE_ENTRY_TYPE_HARDLINK },
  {"special",  ARCHIVE_ENTRY_TYPE_SPECIAL  }
};

// size of buffer for processing data
#define MAX_BUFFER_SIZE (64*1024)

/* size of data block to write without splitting
   Note: a single part of a splitted archive may be larger around this size
*/
#define DATA_BLOCK_SIZE (16*1024)

// default alignment
#define DEFAULT_ALIGNMENT 4

// used message authentication code algorithm for signatures (currently fixed)
#define SIGNATURE_HASH_ALGORITHM CRYPT_HASH_ALGORITHM_SHA2_512

// max. size of hash value
#define MAX_HASH_SIZE 1024

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
const ChunkIO CHUNK_IO_STORAGE =
{
  (bool(*)(void*))Storage_eof,
  (Errors(*)(void*,void*,ulong,ulong*))Storage_read,
  (Errors(*)(void*,const void*,ulong))Storage_write,
  (Errors(*)(void*,uint64*))Storage_tell,
  (Errors(*)(void*,uint64))Storage_seek,
  (uint64(*)(void*))Storage_getSize
};

#define MAX_INDEX_LIST 256

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
  ArchiveHandle           *archiveHandle;
  const Password          *jobCryptPassword;             // job crypt password or NULL
  PasswordModes           passwordMode;                  // password input mode
  const PasswordNode      *passwordNode;                 // next password node to use
  GetNamePasswordFunction getNamePasswordFunction;       // password input callback
  void                    *getNamePasswordUserData;
} PasswordHandle;

// crypt info list
typedef struct DecryptKeyNode
{
  LIST_NODE_HEADER(struct DecryptKeyNode);

  CryptSalt        cryptSalt;
  CryptKey         cryptKey;
//  ArchiveCryptInfo archiveCryptInfo;
  Password         *password;
  uint             keyLength;
//  CryptAlgorithms cryptAlgorithm;
} DecryptKeyNode;

typedef struct
{
  LIST_HEADER(DecryptKeyNode);
  Semaphore            lock;
  const DecryptKeyNode *newDecryptKeyNode;               // new added decrypt key
} DecryptKeyList;

// decrypt key iterator
typedef struct
{
  ArchiveHandle           *archiveHandle;
  PasswordModes           passwordMode;                  // password input mode
  const Password          *jobCryptPassword;             // job crypt password or NULL
  GetNamePasswordFunction getNamePasswordFunction;       // password input callback
  void                    *getNamePasswordUserData;
  DecryptKeyNode          *nextDecryptKeyNode;           // next decrypt key node to use
} DecryptKeyIterator;

// archive index node
typedef struct ArchiveIndexNode
{
  LIST_NODE_HEADER(struct ArchiveIndexNode);

  IndexId           storageId;
  ArchiveEntryTypes type;
  union
  {
    struct
    {
      String name;
      uint64 size;
      uint64 timeLastAccess;
      uint64 timeModified;
      uint64 timeLastChanged;
      uint32 userId;
      uint32 groupId;
      uint32 permission;
      uint64 fragmentOffset;
      uint64 fragmentSize;
    } file;
    struct
    {
      String          name;
      FileSystemTypes fileSystemType;
      int64           size;
      ulong           blockSize;
      uint64          blockOffset;
      uint64          blockCount;
    } image;
    struct
    {
      String name;
      uint64 timeLastAccess;
      uint64 timeModified;
      uint64 timeLastChanged;
      uint32 userId;
      uint32 groupId;
      uint32 permission;
    } directory;
    struct
    {
      String name;
      String destinationName;
      uint64 timeLastAccess;
      uint64 timeModified;
      uint64 timeLastChanged;
      uint32 userId;
      uint32 groupId;
      uint32 permission;
    } link;
    struct
    {
      String     name;
      uint64     size;
      uint64     timeLastAccess;
      uint64     timeModified;
      uint64     timeLastChanged;
      uint32     userId;
      uint32     groupId;
      uint32     permission;
      uint64     fragmentOffset;
      uint64     fragmentSize;
    } hardlink;
    struct
    {
      String           name;
      FileSpecialTypes specialType;
      uint64           timeLastAccess;
      uint64           timeModified;
      uint64           timeLastChanged;
      uint32           userId;
      uint32           groupId;
      uint32           permission;
      uint32           major;
      uint32           minor;
    } special;
    struct
    {
      String       userName;
      String       hostName;
      String       jobUUID;
      String       scheduleUUID;
      ArchiveTypes archiveType;
      uint64       createdDateTime;
      String       comment;
    } meta;
  };
} ArchiveIndexNode;

/***************************** Variables *******************************/

// list with all known decryption passwords
LOCAL PasswordList   decryptPasswordList;

// list with all known decrypt keys
LOCAL DecryptKeyList decryptKeyList;

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
* Name   : freeDecryptKeyNode
* Purpose: free decrypt key node
* Input  : cryptInfoNode - crypt key node
*          userData      - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeDecryptKeyNode(DecryptKeyNode *decryptKeyNode, void *userData)
{
  assert(decryptKeyNode != NULL);

  UNUSED_VARIABLE(userData);

  Crypt_doneKey(&decryptKeyNode->cryptKey);
  Password_delete(decryptKeyNode->password);
}

/***********************************************************************\
* Name   : freeArchiveCryptInfoNode
* Purpose: free decrypt key node
* Input  : cryptInfoNode - crypt key node
*          userData      - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeArchiveCryptInfoNode(ArchiveCryptInfoNode *archiveCryptInfoNode, void *userData)
{
  assert(archiveCryptInfoNode != NULL);

  UNUSED_VARIABLE(userData);

  Crypt_doneKey(&archiveCryptInfoNode->archiveCryptInfo.cryptKey);
  Crypt_doneSalt(&archiveCryptInfoNode->archiveCryptInfo.cryptSalt);
}

/***********************************************************************\
* Name   : getCryptPassword
* Purpose: get crypt password if password not set
* Input  : archiveHandle           - archive handle
*          jobOptions              - job options
*          passwordMode            - password mode
*          getNamePasswordFunction - get password call-back (can be
*                                    NULL)
*          getNamePasswordUserData - user data for get password call-back
* Output : password - password
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getCryptPassword(Password                *password,
                              ArchiveHandle           *archiveHandle,
                              const JobOptions        *jobOptions,
                              GetNamePasswordFunction getNamePasswordFunction,
                              void                    *getNamePasswordUserData
                             )
{
  String printableStorageName;
  Errors error;

  assert(password != NULL);
  assert(archiveHandle != NULL);
  assert(archiveHandle->storageInfo != NULL);
  assert(jobOptions != NULL);

  // init variables
  printableStorageName = String_new();

  error = ERROR_UNKNOWN;

  // get printable storage name
  switch (archiveHandle->mode)
  {
    case ARCHIVE_MODE_CREATE:
      String_set(printableStorageName,archiveHandle->create.tmpFileName);
      break;
    case ARCHIVE_MODE_READ:
      Storage_getPrintableName(printableStorageName,&archiveHandle->storageInfo->storageSpecifier,NULL);
      break;
  }

  switch (jobOptions->cryptPasswordMode)
  {
    case PASSWORD_MODE_NONE:
      error = ERROR_NO_CRYPT_PASSWORD;
      break;
    case PASSWORD_MODE_DEFAULT:
      if (!Password_isEmpty(&globalOptions.cryptPassword))
      {
        Password_set(password,&globalOptions.cryptPassword);
        error = ERROR_NONE;
      }
      else
      {
        if (!archiveHandle->cryptPasswordReadFlag && (getNamePasswordFunction != NULL))
        {
          error = getNamePasswordFunction(NULL,  // loginName
                                          password,
                                          PASSWORD_TYPE_CRYPT,
                                          (archiveHandle->mode == ARCHIVE_MODE_READ)
                                            ? String_cString(printableStorageName)
                                            : NULL,
                                          TRUE,  // validateFlag
                                          TRUE,  // weakCheckFlag
                                          getNamePasswordUserData
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
      if      (!Password_isEmpty(&jobOptions->cryptPassword))
      {
        Password_set(password,&jobOptions->cryptPassword);
        error = ERROR_NONE;
      }
      else if (!Password_isEmpty(&globalOptions.cryptPassword))
      {
        Password_set(password,&globalOptions.cryptPassword);
        error = ERROR_NONE;
      }
      else
      {
        if (!archiveHandle->cryptPasswordReadFlag && (getNamePasswordFunction != NULL))
        {
          error = getNamePasswordFunction(NULL,  // loginName
                                          password,
                                          PASSWORD_TYPE_CRYPT,
                                          (archiveHandle->mode == ARCHIVE_MODE_READ)
                                            ? String_cString(printableStorageName)
                                            : NULL,
                                          TRUE,  // validateFlag
                                          TRUE,  // weakCheckFlag
                                          getNamePasswordUserData
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
      if (!Password_isEmpty(&jobOptions->cryptPassword))
      {
        Password_set(password,&jobOptions->cryptPassword);
        error = ERROR_NONE;
      }
      else
      {
        if (!archiveHandle->cryptPasswordReadFlag && (getNamePasswordFunction != NULL))
        {
          error = getNamePasswordFunction(NULL,  // loginName
                                          password,
                                          PASSWORD_TYPE_CRYPT,
                                          (archiveHandle->mode == ARCHIVE_MODE_READ)
                                            ? String_cString(printableStorageName)
                                            : NULL,
                                          TRUE,  // validateFlag
                                          TRUE,  // weakCheckFlag
                                          getNamePasswordUserData
                                         );
          archiveHandle->cryptPasswordReadFlag = TRUE;
        }
        else
        {
          error = ERROR_NO_CRYPT_PASSWORD;
        }
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  assert(error != ERROR_UNKNOWN);

  // free resources
  String_delete(printableStorageName);

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
  const Password *password;
  String         printableStorageName;
  Password       newPassword;
  Errors         error;

  assert(passwordHandle != NULL);
  assert(passwordHandle->archiveHandle != NULL);
  assert(passwordHandle->archiveHandle->storageInfo != NULL);

  password = NULL;
  SEMAPHORE_LOCKED_DO(&passwordHandle->archiveHandle->passwordLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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
           case PASSWORD_MODE_DEFAULT:
             password = &globalOptions.cryptPassword;

             // next password mode is: ask
             passwordHandle->passwordMode = PASSWORD_MODE_ASK;
             break;
           case PASSWORD_MODE_CONFIG:
             password = passwordHandle->jobCryptPassword;

             // next password mode is: default
             passwordHandle->passwordMode = PASSWORD_MODE_DEFAULT;
             break;
           case PASSWORD_MODE_ASK:
             if (passwordHandle->getNamePasswordFunction != NULL)
             {
               // input password
               printableStorageName = Storage_getPrintableName(String_new(),&passwordHandle->archiveHandle->storageInfo->storageSpecifier,NULL);
               Password_init(&newPassword);
               error = passwordHandle->getNamePasswordFunction(NULL,  // loginName
                                                               &newPassword,
                                                               PASSWORD_TYPE_CRYPT,
                                                               (passwordHandle->archiveHandle->mode == ARCHIVE_MODE_READ)
                                                                 ? String_cString(printableStorageName)
                                                                 : NULL,
                                                               FALSE,  // validateFlag
                                                               FALSE,  // weakCheckFlag
                                                               passwordHandle->getNamePasswordUserData
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
               String_delete(printableStorageName);
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
* Input  : archiveHandle           - archive handle
*          jobOptions              - job options
*          passwordMode            - password mode
*          getNamePasswordFunction - get password call-back
*          getNamePasswordUserData - user data for get password call-back
* Output : passwordHandle - intialized password handle
* Return : password or NULL if no more passwords
* Notes  : -
\***********************************************************************/

LOCAL const Password *getFirstDecryptPassword(PasswordHandle          *passwordHandle,
                                              ArchiveHandle           *archiveHandle,
                                              const JobOptions        *jobOptions,
                                              PasswordModes           passwordMode,
                                              GetNamePasswordFunction getNamePasswordFunction,
                                              void                    *getNamePasswordUserData
                                             )
{
  const Password *password;

  assert(passwordHandle != NULL);
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);

  password = NULL;

  SEMAPHORE_LOCKED_DO(&archiveHandle->passwordLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    passwordHandle->archiveHandle           = archiveHandle;
    passwordHandle->jobCryptPassword        = &jobOptions->cryptPassword;
    passwordHandle->passwordMode            = (passwordMode != PASSWORD_MODE_DEFAULT) ? passwordMode : jobOptions->cryptPasswordMode;
    passwordHandle->passwordNode            = decryptPasswordList.head;
    passwordHandle->getNamePasswordFunction = getNamePasswordFunction;
    passwordHandle->getNamePasswordUserData = getNamePasswordUserData;

    password = getNextDecryptPassword(passwordHandle);
  }

  return password;
}

/***********************************************************************\
* Name   : deriveDecryptKey
* Purpose: derive decrypt key for password with appropiated salt and key
*          length
* Input  : cryptKeyDeriveType - key derive type; see CryptKeyDeriveTypes
*          cryptSalt          - crypt salt
*          keyLength          - key length [bits]
* Output : decryptKeyNode - decrypt key node
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors deriveDecryptKey(DecryptKeyNode      *decryptKeyNode,
                              CryptKeyDeriveTypes cryptKeyDeriveType,
                              const CryptSalt     *cryptSalt,
                              uint                keyLength
                             )
{
  Errors error;

  assert(decryptKeyNode != NULL);
  assert(keyLength > 0);
  assert(cryptSalt != NULL);

  // derive decrypt key from password with salt
  error = Crypt_deriveKey(&decryptKeyNode->cryptKey,
                          cryptKeyDeriveType,
                          Crypt_isSalt(cryptSalt) ? cryptSalt : NULL,
                          decryptKeyNode->password,
                          keyLength
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // store new salt and key length
  Crypt_copySalt(&decryptKeyNode->cryptSalt,cryptSalt);
  decryptKeyNode->keyLength  = keyLength;
//fprintf(stderr,"%s, %d: decrypt key\n",__FILE__,__LINE__); debugDumpMemory(decryptKeyNode->cryptKey.data,decryptKeyNode->cryptKey.dataLength,0);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : addDecryptKey
* Purpose: add decrypt key for password with salt and key length
* Input  : password           - password
*          cryptKeyDeriveType - key derive type; see CryptKeyDeriveTypes
*          cryptSalt          - crypt salt
*          keyLength          - key length [bits]
* Output : -
* Return : crypt key or NULL
* Notes  : It is safe to add a decrypt key at the list end. Other
*          iterators will get aware of new added decrypt keys.
\***********************************************************************/

LOCAL CryptKey *addDecryptKey(const Password      *password,
                              CryptKeyDeriveTypes cryptKeyDeriveType,
                              const CryptSalt     *cryptSalt,
                              uint                keyLength
                             )
{
  DecryptKeyNode *decryptKeyNode;
  Errors         error;

  assert(password != NULL);
  assert(keyLength > 0);
  assert(Semaphore_isLocked(&decryptKeyList.lock));

  // find decrypt key
  decryptKeyNode = LIST_FIND(&decryptKeyList,
                             decryptKeyNode,
                             Password_equals(decryptKeyNode->password,password)
                            );

  // create new decrypt key node if required
  if (decryptKeyNode == NULL)
  {
    // init new decrypt key node
    decryptKeyNode = LIST_NEW_NODE(DecryptKeyNode);
    if (decryptKeyNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    Crypt_initSalt(&decryptKeyNode->cryptSalt);
    Crypt_initKey(&decryptKeyNode->cryptKey,CRYPT_PADDING_TYPE_NONE);
    decryptKeyNode->password  = Password_duplicate(password);
    decryptKeyNode->keyLength = 0;

    // add to decrypt key list
    List_append(&decryptKeyList,decryptKeyNode);
  }
  assert(decryptKeyNode != NULL);

  // check if salt/key length changed => calculate new key derivation
  if (   (decryptKeyNode->keyLength != keyLength)
      || !Crypt_equalsSalt(&decryptKeyNode->cryptSalt,cryptSalt)
     )
  {
    error = deriveDecryptKey(decryptKeyNode,
                             cryptKeyDeriveType,
                             cryptSalt,
                             keyLength
                            );
    if (error != ERROR_NONE)
    {
      return NULL;
    }
  }

  return (decryptKeyNode != NULL) ? &decryptKeyNode->cryptKey : NULL;
}

/***********************************************************************\
* Name   : getNextDecryptKey
* Purpose: get next decrypt key
* Input  : cryptInfoIterator  - crypt info iterator
*          cryptKeyDeriveType - key derive type; see CryptKeyDeriveTypes
*          cryptSalt          - crypt salt
*          keyLength          - key length [bits]
* Output : -
* Return : decrypt key or NULL if no more decrypt keys
* Notes  : Ordering of decrypt keys search:
*            - next decrypt key from list
*            - decrypt key with password from config
*            - decrypt key with password from command line
*            - ask for password (only if no command line password set)
*              and add decrypt key
\***********************************************************************/

LOCAL const CryptKey *getNextDecryptKey(DecryptKeyIterator  *decryptKeyIterator,
                                        CryptKeyDeriveTypes cryptKeyDeriveType,
                                        const CryptSalt     *cryptSalt,
                                        uint                keyLength
                                       )
{
  DecryptKeyNode *decryptKeyNode;
  const CryptKey *decryptKey;
  String         printableStorageName;
  Password       newPassword;
  Errors         error;

  assert(decryptKeyIterator != NULL);
  assert(decryptKeyIterator->archiveHandle != NULL);
  assert(decryptKeyIterator->archiveHandle->storageInfo != NULL);
  assert(cryptSalt != NULL);

  decryptKey = NULL;
  SEMAPHORE_LOCKED_DO(&decryptKeyList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    while ((decryptKey == NULL) && (decryptKeyIterator->passwordMode != PASSWORD_MODE_NONE))
    {
      if      (decryptKeyIterator->nextDecryptKeyNode != NULL)
      {
        // get next crypt info from list
        decryptKeyNode = decryptKeyIterator->nextDecryptKeyNode;
        decryptKeyIterator->nextDecryptKeyNode = decryptKeyNode->next;

        // check if key length/salt change
        if (   (decryptKeyNode->keyLength != keyLength)
            || !Crypt_equalsSalt(&decryptKeyNode->cryptSalt,cryptSalt)
           )
        {
          if (deriveDecryptKey(decryptKeyNode,
                               cryptKeyDeriveType,
                               cryptSalt,
                               keyLength
                              ) == ERROR_NONE
             )
          {
            decryptKey = &decryptKeyNode->cryptKey;
          }
        }
        else
        {
          decryptKey = &decryptKeyNode->cryptKey;
        }
      }
      else
      {
        switch (decryptKeyIterator->passwordMode)
        {
           case PASSWORD_MODE_NONE:
             // no more decrypt keys
             break;
           case PASSWORD_MODE_CONFIG:
             // get decrypt key from job config
             if (decryptKeyIterator->jobCryptPassword != NULL)
             {
               decryptKey = addDecryptKey(decryptKeyIterator->jobCryptPassword,
                                          cryptKeyDeriveType,
                                          cryptSalt,
                                          keyLength
                                         );
             }

             // next password mode is: default
             decryptKeyIterator->passwordMode = PASSWORD_MODE_DEFAULT;
             break;
           case PASSWORD_MODE_DEFAULT:
             // get decrypt key from global config
             if (!Password_isEmpty(&globalOptions.cryptPassword))
             {
               decryptKey = addDecryptKey(&globalOptions.cryptPassword,
                                          cryptKeyDeriveType,
                                          cryptSalt,
                                          keyLength
                                         );
             }

             // next password mode is: ask
             decryptKeyIterator->passwordMode = PASSWORD_MODE_ASK;
             break;
           case PASSWORD_MODE_ASK:
             // input password and derive decrypt key
             if (decryptKeyIterator->getNamePasswordFunction != NULL)
             {
               // input password
               printableStorageName = Storage_getPrintableName(String_new(),&decryptKeyIterator->archiveHandle->storageInfo->storageSpecifier,NULL);
               Password_init(&newPassword);
               error = decryptKeyIterator->getNamePasswordFunction(NULL,  // loginName
                                                                   &newPassword,
                                                                   PASSWORD_TYPE_CRYPT,
                                                                   (decryptKeyIterator->archiveHandle->mode == ARCHIVE_MODE_READ)
                                                                     ? String_cString(printableStorageName)
                                                                     : NULL,
                                                                   FALSE,  // validateFlag
                                                                   FALSE,  // weakCheckFlag
                                                                   decryptKeyIterator->getNamePasswordUserData
                                                                  );
               if ((error == ERROR_NONE) && !Password_isEmpty(&newPassword))
               {
                 // add to decrypt key list
                 decryptKey = addDecryptKey(&newPassword,
                                            cryptKeyDeriveType,
                                            cryptSalt,
                                            keyLength
                                           );
               }
               Password_done(&newPassword);
               String_delete(printableStorageName);
             }

             // next password mode is: none
             decryptKeyIterator->passwordMode = PASSWORD_MODE_NONE;
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

  return decryptKey;
}

/***********************************************************************\
* Name   : getFirstDecryptKey
* Purpose: get first decrypt key
* Input  : archiveHandle           - archive handle
*          jobOptions              - job options
*          passwordMode            - password mode
*          cryptPassword           - config crypt password (can be NULL)
*          getNamePasswordFunction - get password call-back (can be
*                                    NULL)
*          getNamePasswordUserData - user data for get password call-back
*          cryptKeyDeriveType      - key derive type; see
*                                    CryptKeyDeriveTypes
*          cryptSalt               - crypt salt
*          keyLength               - key length [bits]
* Output : decryptKeyIterator - decrypt key iterator
* Return : decrypt key or NULL if no decrypt key
* Notes  : -
\***********************************************************************/

LOCAL const CryptKey *getFirstDecryptKey(DecryptKeyIterator      *decryptKeyIterator,
                                         ArchiveHandle           *archiveHandle,
                                         PasswordModes           passwordMode,
                                         const Password          *cryptPassword,
                                         GetNamePasswordFunction getNamePasswordFunction,
                                         void                    *getNamePasswordUserData,
                                         CryptKeyDeriveTypes     cryptKeyDeriveType,
                                         const CryptSalt         *cryptSalt,
                                         uint                    keyLength
                                        )
{
  const CryptKey *decryptKey;

  assert(decryptKeyIterator != NULL);
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(cryptSalt != NULL);

  decryptKey = NULL;
  SEMAPHORE_LOCKED_DO(&decryptKeyList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    decryptKeyIterator->archiveHandle           = archiveHandle;
    decryptKeyIterator->passwordMode            = passwordMode;
    decryptKeyIterator->jobCryptPassword        = cryptPassword;
    decryptKeyIterator->getNamePasswordFunction = getNamePasswordFunction;
    decryptKeyIterator->getNamePasswordUserData = getNamePasswordUserData;
    decryptKeyIterator->nextDecryptKeyNode      = (DecryptKeyNode*)List_first(&decryptKeyList);

    decryptKey = getNextDecryptKey(decryptKeyIterator,
                                   cryptKeyDeriveType,
                                   cryptSalt,
                                   keyLength
                                  );
  }

  return decryptKey;
}

/***********************************************************************\
* Name   : addArchiveCryptInfoNode
* Purpose: add new archive crypt info node
* Input  : archiveHandle      - archive handle
*          cryptType          - crypt type (symmetric/asymmetric; see
*                               CRYPT_TYPE_...)
*          cryptMode          - crypt mode; see CRYPT_MODE_...
*          cryptKeyDeriveType - key derive type; see CryptKeyDeriveTypes
* Output : -
* Return : archive crypt info node
* Notes  : -
\***********************************************************************/

LOCAL ArchiveCryptInfoNode *addArchiveCryptInfoNode(ArchiveHandle       *archiveHandle,
                                                    CryptTypes          cryptType,
                                                    CryptMode           cryptMode,
                                                    CryptKeyDeriveTypes cryptKeyDeriveType
                                                   )
{
  ArchiveCryptInfoNode *archiveCryptInfoNode;

  assert(archiveHandle != NULL);

  // init node
  archiveCryptInfoNode = LIST_NEW_NODE(ArchiveCryptInfoNode);
  if (archiveCryptInfoNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  archiveCryptInfoNode->archiveCryptInfo.cryptType          = cryptType;
  archiveCryptInfoNode->archiveCryptInfo.cryptMode          = cryptMode;
  archiveCryptInfoNode->archiveCryptInfo.cryptKeyDeriveType = cryptKeyDeriveType;
  Crypt_initSalt(&archiveCryptInfoNode->archiveCryptInfo.cryptSalt);
  Crypt_initKey(&archiveCryptInfoNode->archiveCryptInfo.cryptKey,CRYPT_PADDING_TYPE_NONE);
  List_append(&archiveHandle->archiveCryptInfoList,archiveCryptInfoNode);

  // keep reference to current archive crypt info
  archiveHandle->archiveCryptInfo = &archiveCryptInfoNode->archiveCryptInfo;

  return archiveCryptInfoNode;
}

/***********************************************************************\
* Name   : initCryptPassword
* Purpose: initialize crypt password
* Input  : archiveHandle - archive handle
*          password      - crypt password to use or NULL
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors initCryptPassword(ArchiveHandle *archiveHandle, const Password *password)
{
  Password *cryptPassword;
  Errors   error;

  assert(archiveHandle != NULL);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);

  SEMAPHORE_LOCKED_DO(&archiveHandle->passwordLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (   Crypt_isEncrypted(archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0])
        && (archiveHandle->cryptPassword == NULL)
       )
    {
      // allocate crypt password
      cryptPassword = Password_new();
      if (cryptPassword == NULL)
      {
        Semaphore_unlock(&archiveHandle->passwordLock);
        return ERROR_INIT_PASSWORD;
      }

      if      (password != NULL)
      {
        // use given crypt password
        Password_set(cryptPassword,password);
      }
      else if (!Password_isEmpty(&archiveHandle->storageInfo->jobOptions->cryptPassword))
      {
        // use crypt password
        Password_set(cryptPassword,&archiveHandle->storageInfo->jobOptions->cryptPassword);
      }
      else
      {
        // get crypt password
        error = getCryptPassword(cryptPassword,
                                 archiveHandle,
                                 archiveHandle->storageInfo->jobOptions,
                                 CALLBACK(archiveHandle->getNamePasswordFunction,archiveHandle->getNamePasswordUserData)
                                );
        if (error != ERROR_NONE)
        {
          Password_delete(cryptPassword);
          Semaphore_unlock(&archiveHandle->passwordLock);
          return error;
        }
      }

      // check if passwort is not empty
      if (Password_isEmpty(cryptPassword))
      {
        Password_delete(cryptPassword);
        Semaphore_unlock(&archiveHandle->passwordLock);
        return ERROR_NO_CRYPT_PASSWORD;
      }

      // set crypt password
      archiveHandle->cryptPassword = cryptPassword;
    }
  }

  return ERROR_NONE;
}

#ifdef MULTI_CRYPT

/***********************************************************************\
* Name   : multiCryptInit
* Purpose: multi-crypt init
* Input  :
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors multiCryptInit(CryptInfo       *cryptInfo,
                            CryptAlgorithms *cryptAlgorithms,
                            CryptKey        *cryptKey,
                            uint            count,
                            const CryptSalt *cryptSalt,
                            Password        *cryptPassword
                           )
{
  int    i;
  Errors error;

HALT_NOT_YET_IMPLEMENTED();

  for (i = 0; i < (int)count; i++)
  {
    error = Crypt_init(cryptInfo,
                       cryptAlgorithms[i],
                       CRYPT_MODE_CBC,
                       cryptSalt,
                       crypyKey
                      );
    if (error != ERROR_NONE)
    {
      while (i >= 0)
      {
        i--;
      }
      return error;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : multiCryptDone
* Purpose: multi-crypt done
* Input  :
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL void multiCryptDone(CryptInfo       *cryptInfo,
                          CryptAlgorithms *cryptAlgorithms,
                          uint            cryptAlgorithmCount
                         )
{
HALT_NOT_YET_IMPLEMENTED();
}

/***********************************************************************\
* Name   : cryptGetBlockLength
* Purpose:
* Input  :
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cryptGetBlockLength(CryptAlgorithms *cryptAlgorithms,
                                 uint            cryptAlgorithmCount,
                                 uint            *blockLength
                                )
{
  uint   i;
  Errors error;
  uint   n;

  assert(blockLength != NULL);

  (*blockLength) = 1;
  for (i = 0; i < cryptAlgorithmCount; i++)
  {
    error = Crypt_getBlockLength(cryptAlgorithms[0],&n);
    if (error != ERROR_NONE)
    {
      return error;
    }
    (*blockLength) = lcm((*blockLength),n);
//fprintf(stderr,"%s, %d: n=%d %d\n",__FILE__,__LINE__,n,*blockLength);
  }

  return ERROR_NONE;
}
#endif

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
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);

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
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);

  return (archiveHandle->storageInfo->jobOptions->archivePartSize > 0LL);
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
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->chunkIO != NULL);
  assert(archiveHandle->chunkIO->getSize != NULL);
  assert(archiveHandle->mode == ARCHIVE_MODE_CREATE);
  assert(Semaphore_isOwned(&archiveHandle->lock));

  newPartFlag = FALSE;
  if (isSplittedArchive(archiveHandle))
  {
    // get current archive size
    if (archiveHandle->create.openFlag)
    {
      archiveFileSize = archiveHandle->chunkIO->getSize(archiveHandle->chunkIOUserData);
    }
    else
    {
      archiveFileSize = 0LL;
    }
//fprintf(stderr,"%s, %d: archiveFileSize=%llu %lu %llu\n",__FILE__,__LINE__,archiveFileSize,minBytes,archiveHandle->archiveFileSize);

    if ((archiveHandle->archiveFileSize+archiveFileSize+minBytes) >= archiveHandle->storageInfo->jobOptions->archivePartSize)
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
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void findNextArchivePart(ArchiveHandle *archiveHandle)
{
  uint64 storageSize;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);

  // find next suitable archive name
  if (   isSplittedArchive(archiveHandle)
      && (archiveHandle->storageInfo->jobOptions->archiveFileMode == ARCHIVE_FILE_MODE_APPEND)
     )
  {
    do
    {
      storageSize = archiveHandle->archiveGetSizeFunction(archiveHandle->storageInfo,
                                                          archiveHandle->storageId,
                                                          archiveHandle->partNumber,
                                                          archiveHandle->archiveGetSizeUserData
                                                         );
      if (storageSize > archiveHandle->storageInfo->jobOptions->archivePartSize)
      {
        archiveHandle->partNumber++;
      }
    }
    while (storageSize > archiveHandle->storageInfo->jobOptions->archivePartSize);

    archiveHandle->archiveFileSize = storageSize;
  }
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : calcuateHash
* Purpose: calculate hash
* Input  : chunkIO         - chunk I/O
*          chunkIOUserData - chunk I/O user data
*          cryptHash       - crypt hash variable
*          start,end       - start/end offset
* Output : cryptHash - hash value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors calcuateHash(const ChunkIO *chunkIO,
                          void          *chunkIOUserData,
                          CryptHash     *cryptHash,
                          uint64        start,
                          uint64        end
                         )
{
  #define BUFFER_SIZE (1024*1024)

  void   *buffer;
  Errors error;
  uint64 index;
  uint64 offset;
  ulong  n;

  assert(chunkIO != NULL);
  assert(chunkIO->tell != NULL);
  assert(chunkIO->seek != NULL);
  assert(chunkIO->read != NULL);
  assert(cryptHash != NULL);
  assert(end >= start);

  // init variables
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // save current index
  error = chunkIO->tell(chunkIOUserData,&index);
  if (error != ERROR_NONE)
  {
    free(buffer);
    return error;
  }

  // seek to data start
  error = chunkIO->seek(chunkIOUserData,start);
  if (error != ERROR_NONE)
  {
    (void)chunkIO->seek(chunkIOUserData,index);
    free(buffer);
    return error;
  }

  // calculate hash
//fprintf(stderr,"%s, %d: offset=%llu\n",__FILE__,__LINE__,startOffset);
//fprintf(stderr,"%s, %d: chunkSignature.info.offset=%llu\n",__FILE__,__LINE__,endOffset);
  offset = start;
  while (   (offset < end)
         && (error == ERROR_NONE)
        )
  {
    n = (ulong)MIN(end-offset,BUFFER_SIZE);

    error = chunkIO->read(chunkIOUserData,buffer,n,NULL);
    if (error == ERROR_NONE)
    {
//fprintf(stderr,"%s, %d: n=%lu\n",__FILE__,__LINE__,n); debugDumpMemory(buffer,n,0);
      Crypt_updateHash(cryptHash,buffer,n);

      offset += (uint64)n;
    }
  }
  if (error != ERROR_NONE)
  {
    (void)chunkIO->seek(chunkIOUserData,index);
    free(buffer);
    return error;
  }

  // restore index
  error = chunkIO->seek(chunkIOUserData,index);
  if (error != ERROR_NONE)
  {
    free(buffer);
    return error;
  }

  // free resources
  free(buffer);

  return ERROR_NONE;

  #undef BUFFER_SIZE
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : freeArchiveIndexNode
* Purpose: free archive index node
* Input  : archiveIndexNode - archive index node
*          userData         - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeArchiveIndexNode(ArchiveIndexNode *archiveIndexNode, void *userData)
{
  assert(archiveIndexNode != NULL);

  UNUSED_VARIABLE(userData);

  switch (archiveIndexNode->type)
  {
    case ARCHIVE_ENTRY_TYPE_NONE:
      break;
    case ARCHIVE_ENTRY_TYPE_FILE:
      String_delete(archiveIndexNode->file.name);
      break;
    case ARCHIVE_ENTRY_TYPE_IMAGE:
      String_delete(archiveIndexNode->image.name);
      break;
    case ARCHIVE_ENTRY_TYPE_DIRECTORY:
      String_delete(archiveIndexNode->directory.name);
      break;
    case ARCHIVE_ENTRY_TYPE_LINK:
      String_delete(archiveIndexNode->link.destinationName);
      String_delete(archiveIndexNode->link.name);
      break;
    case ARCHIVE_ENTRY_TYPE_HARDLINK:
      String_delete(archiveIndexNode->hardlink.name);
      break;
    case ARCHIVE_ENTRY_TYPE_SPECIAL:
      String_delete(archiveIndexNode->special.name);
      break;
    case ARCHIVE_ENTRY_TYPE_META:
      String_delete(archiveIndexNode->meta.comment);
      String_delete(archiveIndexNode->meta.scheduleUUID);
      String_delete(archiveIndexNode->meta.jobUUID);
      String_delete(archiveIndexNode->meta.hostName);
      String_delete(archiveIndexNode->meta.userName);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
}

/***********************************************************************\
* Name   : deleteArchiveIndexNode
* Purpose: delete archive index node
* Input  : archiveIndexNode - archive index node to delete
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deleteArchiveIndexNode(ArchiveIndexNode *archiveIndexNode)
{
  assert(archiveIndexNode != NULL);

  freeArchiveIndexNode(archiveIndexNode,NULL);
  LIST_DELETE_NODE(archiveIndexNode);
}

/***********************************************************************\
* Name   : flushArchiveIndexList
* Purpose: flush archive index list and write to index database
* Input  : archiveHandle    - archive handle
*          maxIndexEntries  - max. entries in list for flush
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors flushArchiveIndexList(ArchiveHandle *archiveHandle, uint maxIndexEntries)
{
  ArchiveIndexList archiveIndexList;
  Errors           error;
  ArchiveIndexNode *archiveIndexNode;

  assert(archiveHandle != NULL);

  error = ERROR_NONE;

  // init variables
  List_init(&archiveIndexList);

  // get list to flush
  SEMAPHORE_LOCKED_DO(&archiveHandle->archiveIndexList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (List_count(&archiveHandle->archiveIndexList) >= maxIndexEntries)
    {
      List_move(&archiveIndexList,NULL,&archiveHandle->archiveIndexList,NULL,NULL);
    }
  }

  // flush list
  if (!List_isEmpty(&archiveIndexList))
  {
    SEMAPHORE_LOCKED_DO(&archiveHandle->indexLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      // start transaction
      error = Index_beginTransaction(archiveHandle->indexHandle,INDEX_TIMEOUT);
      if (error != ERROR_NONE)
      {
        Semaphore_unlock(&archiveHandle->indexLock);
        return error;
      }

      // add to index
//fprintf(stderr,"%s, %d: flush archive index list to db\n",__FILE__,__LINE__);
      while (!List_isEmpty(&archiveIndexList) && (error == ERROR_NONE))
      {
        archiveIndexNode = (ArchiveIndexNode*)List_removeFirst(&archiveIndexList);

        switch (archiveIndexNode->type)
        {
          case ARCHIVE_ENTRY_TYPE_FILE:
            error = Index_addFile(archiveHandle->indexHandle,
                                  archiveIndexNode->storageId,
                                  archiveIndexNode->file.name,
                                  archiveIndexNode->file.size,
                                  archiveIndexNode->file.timeLastAccess,
                                  archiveIndexNode->file.timeModified,
                                  archiveIndexNode->file.timeLastChanged,
                                  archiveIndexNode->file.userId,
                                  archiveIndexNode->file.groupId,
                                  archiveIndexNode->file.permission,
                                  archiveIndexNode->file.fragmentOffset,
                                  archiveIndexNode->file.fragmentSize
                                 );
            break;
          case ARCHIVE_ENTRY_TYPE_IMAGE:
            error = Index_addImage(archiveHandle->indexHandle,
                                   archiveIndexNode->storageId,
                                   archiveIndexNode->image.name,
                                   archiveIndexNode->image.fileSystemType,
                                   archiveIndexNode->image.size,
                                   archiveIndexNode->image.blockSize,
                                   archiveIndexNode->image.blockOffset,
                                   archiveIndexNode->image.blockCount
                                  );
            break;
          case ARCHIVE_ENTRY_TYPE_DIRECTORY:
            error = Index_addDirectory(archiveHandle->indexHandle,
                                       archiveIndexNode->storageId,
                                       archiveIndexNode->directory.name,
                                       archiveIndexNode->directory.timeLastAccess,
                                       archiveIndexNode->directory.timeModified,
                                       archiveIndexNode->directory.timeLastChanged,
                                       archiveIndexNode->directory.userId,
                                       archiveIndexNode->directory.groupId,
                                       archiveIndexNode->directory.permission
                                      );
            break;
          case ARCHIVE_ENTRY_TYPE_LINK:
            error = Index_addLink(archiveHandle->indexHandle,
                                  archiveIndexNode->storageId,
                                  archiveIndexNode->link.name,
                                  archiveIndexNode->link.destinationName,
                                  archiveIndexNode->link.timeLastAccess,
                                  archiveIndexNode->link.timeModified,
                                  archiveIndexNode->link.timeLastChanged,
                                  archiveIndexNode->link.userId,
                                  archiveIndexNode->link.groupId,
                                  archiveIndexNode->link.permission
                                 );
            break;
          case ARCHIVE_ENTRY_TYPE_HARDLINK:
            error = Index_addHardlink(archiveHandle->indexHandle,
                                      archiveIndexNode->storageId,
                                      archiveIndexNode->hardlink.name,
                                      archiveIndexNode->hardlink.size,
                                      archiveIndexNode->hardlink.timeLastAccess,
                                      archiveIndexNode->hardlink.timeModified,
                                      archiveIndexNode->hardlink.timeLastChanged,
                                      archiveIndexNode->hardlink.userId,
                                      archiveIndexNode->hardlink.groupId,
                                      archiveIndexNode->hardlink.permission,
                                      archiveIndexNode->hardlink.fragmentOffset,
                                      archiveIndexNode->hardlink.fragmentSize
                                     );
            break;
          case ARCHIVE_ENTRY_TYPE_SPECIAL:
            error = Index_addSpecial(archiveHandle->indexHandle,
                                     archiveIndexNode->storageId,
                                     archiveIndexNode->special.name,
                                     archiveIndexNode->special.specialType,
                                     archiveIndexNode->special.timeLastAccess,
                                     archiveIndexNode->special.timeModified,
                                     archiveIndexNode->special.timeLastChanged,
                                     archiveIndexNode->special.userId,
                                     archiveIndexNode->special.groupId,
                                     archiveIndexNode->special.permission,
                                     archiveIndexNode->special.major,
                                     archiveIndexNode->special.minor
                                    );
            break;
          case ARCHIVE_ENTRY_TYPE_META:
//TODO
#if 0
            error = Index_addMeta(archiveHandle->indexHandle,
                                  archiveIndexNode->meta.userName,
                                  archiveIndexNode->meta.hostName,
                                  archiveIndexNode->meta.jobUUID,
                                  archiveIndexNode->meta.scheduleUUID,
                                  archiveIndexNode->meta.archiveType,
                                  archiveIndexNode->meta.createdDateTime,
                                  archiveIndexNode->meta.comment
                                 );
#endif
            break;
          default:
            #ifndef NDEBUG
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            #endif /* NDEBUG */
            break;
        }

        deleteArchiveIndexNode(archiveIndexNode);
      }

      // end transaction
      (void)Index_endTransaction(archiveHandle->indexHandle);
    }
  }

  // free resources
  List_done(&archiveIndexList,(ListNodeFreeFunction)CALLBACK(freeArchiveIndexNode,NULL));

  return error;
}

/***********************************************************************\
* Name   : addArchiveIndexNode
* Purpose: add archive index node and flush
* Input  : archiveHandle    - archive handle
*          archiveIndexNode - archive index node
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors addArchiveIndexNode(ArchiveHandle *archiveHandle, ArchiveIndexNode *archiveIndexNode)
{
  Errors error;

  // flush index list
  error = flushArchiveIndexList(archiveHandle,MAX_INDEX_LIST);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // append to list
  SEMAPHORE_LOCKED_DO(&archiveHandle->archiveIndexList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    List_append(&archiveHandle->archiveIndexList,archiveIndexNode);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : indexAddFile
* Purpose: add file index node
* Input  : archiveHandle   - archive handle
*          storageId       - index id of index
*          name            - name
*          size            - size [bytes]
*          timeLastAccess  - last access date/time stamp [s]
*          timeModified    - modified date/time stamp [s]
*          timeLastChanged - last changed date/time stamp [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
*          fragmentOffset  - fragment offset [bytes]
*          fragmentSize    - fragment size [bytes]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors indexAddFile(ArchiveHandle *archiveHandle,
                          IndexId       storageId,
                          ConstString   name,
                          uint64        size,
                          uint64        timeLastAccess,
                          uint64        timeModified,
                          uint64        timeLastChanged,
                          uint32        userId,
                          uint32        groupId,
                          uint32        permission,
                          uint64        fragmentOffset,
                          uint64        fragmentSize
                         )
{
  ArchiveIndexNode *archiveIndexNode;
  Errors           error;

  archiveIndexNode = LIST_NEW_NODE(ArchiveIndexNode);
  if (archiveIndexNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  archiveIndexNode->storageId            = storageId;
  archiveIndexNode->type                 = ARCHIVE_ENTRY_TYPE_FILE;
  archiveIndexNode->file.name            = String_duplicate(name);
  archiveIndexNode->file.size            = size;
  archiveIndexNode->file.timeLastAccess  = timeLastAccess;
  archiveIndexNode->file.timeModified    = timeModified;
  archiveIndexNode->file.timeLastChanged = timeLastChanged;
  archiveIndexNode->file.userId          = userId;
  archiveIndexNode->file.groupId         = groupId;
  archiveIndexNode->file.permission      = permission;
  archiveIndexNode->file.fragmentOffset  = fragmentOffset;
  archiveIndexNode->file.fragmentSize    = fragmentSize;

  error = addArchiveIndexNode(archiveHandle,archiveIndexNode);
  if (error != ERROR_NONE)
  {
    deleteArchiveIndexNode(archiveIndexNode);
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : indexAddImage
* Purpose: add image index node
* Input  : archiveHandle  - archive handle
*          storageId      - index id of index
*          imageName      - image name
*          fileSystemType - file system type
*          size           - size [bytes]
*          blockSize      - block size [bytes]
*          blockOffset    - block offset [blocks]
*          blockCount     - number of blocks
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors indexAddImage(ArchiveHandle   *archiveHandle,
                           IndexId         storageId,
                           ConstString     name,
                           FileSystemTypes fileSystemType,
                           int64           size,
                           ulong           blockSize,
                           uint64          blockOffset,
                           uint64          blockCount
                          )
{
  ArchiveIndexNode *archiveIndexNode;
  Errors           error;

  archiveIndexNode = LIST_NEW_NODE(ArchiveIndexNode);
  if (archiveIndexNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  archiveIndexNode->storageId             = storageId;
  archiveIndexNode->type                  = ARCHIVE_ENTRY_TYPE_IMAGE;
  archiveIndexNode->image.name            = String_duplicate(name);
  archiveIndexNode->image.fileSystemType  = fileSystemType;
  archiveIndexNode->image.size            = size;
  archiveIndexNode->image.blockSize       = blockSize;
  archiveIndexNode->image.blockOffset     = blockOffset;
  archiveIndexNode->image.blockCount      = blockCount;

  error = addArchiveIndexNode(archiveHandle,archiveIndexNode);
  if (error != ERROR_NONE)
  {
    deleteArchiveIndexNode(archiveIndexNode);
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : indexAddDirectory
* Purpose: add directory index node
* Input  : archiveHandle   - archive handle
*          storageId       - index id of index
*          directoryName   - name
*          timeLastAccess  - last access date/time stamp [s]
*          timeModified    - modified date/time stamp [s]
*          timeLastChanged - last changed date/time stamp [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors indexAddDirectory(ArchiveHandle *archiveHandle,
                               IndexId       storageId,
                               String        name,
                               uint64        timeLastAccess,
                               uint64        timeModified,
                               uint64        timeLastChanged,
                               uint32        userId,
                               uint32        groupId,
                               uint32        permission
                              )
{
  ArchiveIndexNode *archiveIndexNode;
  Errors           error;

  archiveIndexNode = LIST_NEW_NODE(ArchiveIndexNode);
  if (archiveIndexNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  archiveIndexNode->storageId                 = storageId;
  archiveIndexNode->type                      = ARCHIVE_ENTRY_TYPE_DIRECTORY;
  archiveIndexNode->directory.name            = String_duplicate(name);
  archiveIndexNode->directory.timeLastAccess  = timeLastAccess;
  archiveIndexNode->directory.timeModified    = timeModified;
  archiveIndexNode->directory.timeLastChanged = timeLastChanged;
  archiveIndexNode->directory.userId          = userId;
  archiveIndexNode->directory.groupId         = groupId;
  archiveIndexNode->directory.permission      = permission;

  error = addArchiveIndexNode(archiveHandle,archiveIndexNode);
  if (error != ERROR_NONE)
  {
    deleteArchiveIndexNode(archiveIndexNode);
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : indexAddLink
* Purpose: add link index node
* Input  : archiveHandle   - archive handle
*          storageId       - index id of index
*          name            - link name
*          destinationName - destination name
*          timeLastAccess  - last access date/time stamp [s]
*          timeModified    - modified date/time stamp [s]
*          timeLastChanged - last changed date/time stamp [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors indexAddLink(ArchiveHandle *archiveHandle,
                          IndexId       storageId,
                          ConstString   name,
                          ConstString   destinationName,
                          uint64        timeLastAccess,
                          uint64        timeModified,
                          uint64        timeLastChanged,
                          uint32        userId,
                          uint32        groupId,
                          uint32        permission
                         )
{
  ArchiveIndexNode *archiveIndexNode;
  Errors           error;

  archiveIndexNode = LIST_NEW_NODE(ArchiveIndexNode);
  if (archiveIndexNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  archiveIndexNode->storageId            = storageId;
  archiveIndexNode->type                 = ARCHIVE_ENTRY_TYPE_LINK;
  archiveIndexNode->link.name            = String_duplicate(name);
  archiveIndexNode->link.destinationName = String_duplicate(destinationName);
  archiveIndexNode->link.timeLastAccess  = timeLastAccess;
  archiveIndexNode->link.timeModified    = timeModified;
  archiveIndexNode->link.timeLastChanged = timeLastChanged;
  archiveIndexNode->link.userId          = userId;
  archiveIndexNode->link.groupId         = groupId;
  archiveIndexNode->link.permission      = permission;

  error = addArchiveIndexNode(archiveHandle,archiveIndexNode);
  if (error != ERROR_NONE)
  {
    deleteArchiveIndexNode(archiveIndexNode);
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : indexAddHardlink
* Purpose: add hardlink index node
* Input  : archiveHandle   - archive handle
*          storageId       - index id of index
*          name            - name
*          size            - size [bytes]
*          timeLastAccess  - last access date/time stamp [s]
*          timeModified    - modified date/time stamp [s]
*          timeLastChanged - last changed date/time stamp [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
*          fragmentOffset  - fragment offset [bytes]
*          fragmentSize    - fragment size [bytes]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors indexAddHardlink(ArchiveHandle *archiveHandle,
                              IndexId       storageId,
                              ConstString   name,
                              uint64        size,
                              uint64        timeLastAccess,
                              uint64        timeModified,
                              uint64        timeLastChanged,
                              uint32        userId,
                              uint32        groupId,
                              uint32        permission,
                              uint64        fragmentOffset,
                              uint64        fragmentSize
                             )
{
  ArchiveIndexNode *archiveIndexNode;
  Errors           error;

  archiveIndexNode = LIST_NEW_NODE(ArchiveIndexNode);
  if (archiveIndexNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  archiveIndexNode->storageId                = storageId;
  archiveIndexNode->type                     = ARCHIVE_ENTRY_TYPE_HARDLINK;
  archiveIndexNode->hardlink.name            = String_duplicate(name);
  archiveIndexNode->hardlink.size            = size;
  archiveIndexNode->hardlink.timeLastAccess  = timeLastAccess;
  archiveIndexNode->hardlink.timeModified    = timeModified;
  archiveIndexNode->hardlink.timeLastChanged = timeLastChanged;
  archiveIndexNode->hardlink.userId          = userId;
  archiveIndexNode->hardlink.groupId         = groupId;
  archiveIndexNode->hardlink.permission      = permission;
  archiveIndexNode->hardlink.fragmentOffset  = fragmentOffset;
  archiveIndexNode->hardlink.fragmentSize    = fragmentSize;

  error = addArchiveIndexNode(archiveHandle,archiveIndexNode);
  if (error != ERROR_NONE)
  {
    deleteArchiveIndexNode(archiveIndexNode);
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : indexAddSpecial
* Purpose: add special index node
* Input  : archiveHandle   - archive handle
*          storageId       - index id of index
*          name            - name
*          specialType     - special type; see FileSpecialTypes
*          timeLastAccess  - last access date/time stamp [s]
*          timeModified    - modified date/time stamp [s]
*          timeLastChanged - last changed date/time stamp [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
*          major,minor     - major,minor number
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors indexAddSpecial(ArchiveHandle    *archiveHandle,
                             IndexId          storageId,
                             ConstString      name,
                             FileSpecialTypes specialType,
                             uint64           timeLastAccess,
                             uint64           timeModified,
                             uint64           timeLastChanged,
                             uint32           userId,
                             uint32           groupId,
                             uint32           permission,
                             uint32           major,
                             uint32           minor
                            )
{
  ArchiveIndexNode *archiveIndexNode;
  Errors           error;

  archiveIndexNode = LIST_NEW_NODE(ArchiveIndexNode);
  if (archiveIndexNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  archiveIndexNode->storageId               = storageId;
  archiveIndexNode->type                    = ARCHIVE_ENTRY_TYPE_SPECIAL;
  archiveIndexNode->special.name            = String_duplicate(name);
  archiveIndexNode->special.specialType     = specialType;
  archiveIndexNode->special.timeLastAccess  = timeLastAccess;
  archiveIndexNode->special.timeModified    = timeModified;
  archiveIndexNode->special.timeLastChanged = timeLastChanged;
  archiveIndexNode->special.userId          = userId;
  archiveIndexNode->special.groupId         = groupId;
  archiveIndexNode->special.permission      = permission;
  archiveIndexNode->special.major           = major;
  archiveIndexNode->special.minor           = minor;

  error = addArchiveIndexNode(archiveHandle,archiveIndexNode);
  if (error != ERROR_NONE)
  {
    deleteArchiveIndexNode(archiveIndexNode);
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : indexAddMeta
* Purpose: add meta index node
* Input  : archiveHandle   - archive handle
*          storageId       - index id of index
*          userName        - user name
*          hostName        - host name
*          jobUUID         - job UUID
*          scheduleUUID    - schedule UUID
*          archiveType     - archive type
*          createdDateTime - created date/time
*          comment         - comment
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors indexAddMeta(ArchiveHandle *archiveHandle,
                          IndexId       storageId,
                          ConstString   userName,
                          ConstString   hostName,
                          ConstString   jobUUID,
                          ConstString   scheduleUUID,
                          ArchiveTypes  archiveType,
                          uint64        createdDateTime,
                          ConstString   comment
                         )
{
  ArchiveIndexNode *archiveIndexNode;
  Errors           error;

  archiveIndexNode = LIST_NEW_NODE(ArchiveIndexNode);
  if (archiveIndexNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  archiveIndexNode->storageId            = storageId;
  archiveIndexNode->type                 = ARCHIVE_ENTRY_TYPE_META;
  archiveIndexNode->meta.userName        = String_duplicate(userName);
  archiveIndexNode->meta.hostName        = String_duplicate(hostName);
  archiveIndexNode->meta.jobUUID         = String_duplicate(jobUUID);
  archiveIndexNode->meta.scheduleUUID    = String_duplicate(scheduleUUID);
  archiveIndexNode->meta.archiveType     = archiveType;
  archiveIndexNode->meta.createdDateTime = createdDateTime;
  archiveIndexNode->meta.comment         = String_duplicate(comment);

  error = addArchiveIndexNode(archiveHandle,archiveIndexNode);
  if (error != ERROR_NONE)
  {
    deleteArchiveIndexNode(archiveIndexNode);
    return error;
  }

  return ERROR_NONE;
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : readHeader
* Purpose: read header
* Input  : archiveHandle - archive handle
*          chunkHeader - key chunk header
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors readHeader(ArchiveHandle     *archiveHandle,
                        const ChunkHeader *chunkHeader
                       )
{
  Errors   error;
  ChunkBAR chunkBAR;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(chunkHeader != NULL);
  assert(chunkHeader->id == CHUNK_ID_BAR);

  // init BAR chunk
  error = Chunk_init(&chunkBAR.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_BAR,
                     CHUNK_DEFINITION_BAR,
//TODO: DEFAULT_ALIGNMENT
                     0,  // alignment
                     NULL,  // cryptInfo
                     &chunkBAR
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // add new crypt info (Note: older BAR version use CTS+simple key derivation)
  addArchiveCryptInfoNode(archiveHandle,
                          CRYPT_TYPE_NONE,
                          CRYPT_MODE_CTS,
                          CRYPT_KEY_DERIVE_SIMPLE
                         );

  // read BAR chunk
  error = Chunk_open(&chunkBAR.info,
                     chunkHeader,
                     chunkHeader->size,
                     archiveHandle
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkBAR.info);
    return error;
  }

  // close chunk
  error = Chunk_close(&chunkBAR.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkBAR.info);
    return error;
  }
  Chunk_done(&chunkBAR.info);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : readSalt
* Purpose: read salt
* Input  : archiveHandle - archive handle
*          chunkHeader   - key chunk header
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors readSalt(ArchiveHandle     *archiveHandle,
                      const ChunkHeader *chunkHeader
                     )
{
  Errors               error;
  ChunkSalt            chunkSalt;
  ArchiveCryptInfoNode *archiveCryptInfoNode;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(chunkHeader != NULL);
  assert(chunkHeader->id == CHUNK_ID_SALT);

  // check size
  if (chunkHeader->size < CHUNK_FIXED_SIZE_SALT)
  {
    return ERROR_INVALID_CHUNK_SIZE;
  }

  // init salt chunk
  error = Chunk_init(&chunkSalt.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_SALT,
                     CHUNK_DEFINITION_SALT,
//TODO: DEFAULT_ALIGNMENT
                     0,  // alignment
                     NULL,  // cryptInfo
                     &chunkSalt
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // open salt chunk
  error = Chunk_open(&chunkSalt.info,
                     chunkHeader,
                     CHUNK_FIXED_SIZE_SALT,
                     archiveHandle
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkSalt.info);
    return error;
  }

  // check if all data read
  if (!Chunk_eofSub(&chunkSalt.info))
  {
    Chunk_close(&chunkSalt.info);
    Chunk_done(&chunkSalt.info);
    return ERRORX_(CORRUPT_DATA,0,"%s",String_cString(archiveHandle->printableStorageName));
  }

  // add new crypt info
  archiveCryptInfoNode = addArchiveCryptInfoNode(archiveHandle,
                                                 CRYPT_TYPE_NONE,
                                                 CRYPT_MODE_NONE,
                                                 CRYPT_KEY_DERIVE_FUNCTION
                                                );
  assert(archiveCryptInfoNode != NULL);

  // set salt
  Crypt_setSalt(&archiveCryptInfoNode->archiveCryptInfo.cryptSalt,
                chunkSalt.salt,
                sizeof(chunkSalt.salt)
               );

  // close chunk
  error = Chunk_close(&chunkSalt.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkSalt.info);
    return error;
  }
  Chunk_done(&chunkSalt.info);

  // free resources

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : readEncryptionKey
* Purpose: read asymmetric encryption key and decrypt with private key
* Input  : archiveHandle   - archive handle
*          chunkHeader     - key chunk header
*          privateCryptKey - private decryption key
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors readEncryptionKey(ArchiveHandle     *archiveHandle,
                               const ChunkHeader *chunkHeader,
                               const CryptKey    *privateCryptKey
                              )
{
  Errors               error;
  ChunkKey             chunkKey;
  uint                 encryptedKeyDataLength;
  void                 *encryptedKeyData;
  ArchiveCryptInfoNode *archiveCryptInfoNode;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(chunkHeader != NULL);
  assert(chunkHeader->id == CHUNK_ID_KEY);

  // check size
  if (chunkHeader->size < CHUNK_FIXED_SIZE_KEY)
  {
    return ERROR_INVALID_CHUNK_SIZE;
  }

  // get last archive crypt info
  archiveCryptInfoNode = LIST_TAIL(&archiveHandle->archiveCryptInfoList);
  assert(archiveCryptInfoNode != NULL);

  // init key chunk
  error = Chunk_init(&chunkKey.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_KEY,
                     CHUNK_DEFINITION_KEY,
//TODO: DEFAULT_ALIGNMENT
                     0,  // alignment
                     NULL,  // cryptInfo
                     &chunkKey
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // open key chunk
  error = Chunk_open(&chunkKey.info,
                     chunkHeader,
                     CHUNK_FIXED_SIZE_KEY,
                     archiveHandle
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkKey.info);
    return error;
  }

  // read key chunk
  encryptedKeyDataLength = chunkHeader->size-CHUNK_FIXED_SIZE_KEY;
  encryptedKeyData       = malloc(encryptedKeyDataLength);
  if (encryptedKeyData == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  error = Chunk_readData(&chunkKey.info,
                         encryptedKeyData,
                         encryptedKeyDataLength,
                         NULL  // bytesRead: read all
                        );
  if (error != ERROR_NONE)
  {
    free(encryptedKeyData);
    Chunk_close(&chunkKey.info);
    Chunk_done(&chunkKey.info);
    return error;
  }

  // check if all data read
  if (!Chunk_eofSub(&chunkKey.info))
  {
    free(encryptedKeyData);
    Chunk_close(&chunkKey.info);
    Chunk_done(&chunkKey.info);
    return ERRORX_(CORRUPT_DATA,0,"%s",String_cString(archiveHandle->printableStorageName));
  }

  // close chunk
  error = Chunk_close(&chunkKey.info);
  if (error != ERROR_NONE)
  {
    free(encryptedKeyData);
    Chunk_done(&chunkKey.info);
    return error;
  }
  Chunk_done(&chunkKey.info);

  // get decrypt key
  error = Crypt_getDecryptKey(&archiveCryptInfoNode->archiveCryptInfo.cryptKey,
                              0,  // keyLength
                              privateCryptKey,
                              encryptedKeyData,
                              encryptedKeyDataLength
                             );
  if (error != ERROR_NONE)
  {
    free(encryptedKeyData);
    return error;
  }
//fprintf(stderr,"%s, %d: %d encrypted random key \n",__FILE__,__LINE__,archiveHandle->cryptKey.dataLength); debugDumpMemory(archiveHandle->cryptKey.data,archiveHandle->cryptKey.dataLength,0);

//TODO: not required to store encrypted key when reading archives
  // set crypt data
  if (archiveHandle->encryptedKeyData != NULL) free(archiveHandle->encryptedKeyData);
  archiveHandle->encryptedKeyData       = encryptedKeyData;
  archiveHandle->encryptedKeyDataLength = encryptedKeyDataLength;

  // set asymmetric crypt type
  archiveCryptInfoNode->archiveCryptInfo.cryptType = CRYPT_TYPE_ASYMMETRIC;

  // free resources

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
  Errors   error;
  ChunkBAR chunkBAR;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
//  assert(archiveHandle->archiveCryptInfo != NULL);

  // init BAR chunk
  error = Chunk_init(&chunkBAR.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_BAR,
                     CHUNK_DEFINITION_BAR,
                     DEFAULT_ALIGNMENT,
                     NULL,  // cryptInfo
                     &chunkBAR
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }
//TODO: remove
#if 0
  assert(sizeof(chunkBAR.salt) == archiveHandle->archiveCryptInfo->cryptSalt.length);
  Crypt_getSalt(chunkBAR.salt,sizeof(chunkBAR.salt),&archiveHandle->archiveCryptInfo->cryptSalt);
#endif

  // write header chunks
  error = Chunk_create(&chunkBAR.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkBAR.info);
    return error;
  }

  // close chunks
  error = Chunk_close(&chunkBAR.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkBAR.info);
    return error;
  }

  // free resources
  Chunk_done(&chunkBAR.info);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeSalt
* Purpose: write new salt chunk
* Input  : archiveHandle - archive handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeSalt(ArchiveHandle *archiveHandle)
{
  Errors    error;
  ChunkSalt chunkSalt;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(Semaphore_isOwned(&archiveHandle->lock));
  assert(archiveHandle->archiveCryptInfo != NULL);

  // init key chunk
  error = Chunk_init(&chunkSalt.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_SALT,
                     CHUNK_DEFINITION_SALT,
//TODO: DEFAULT_ALIGNMENT
                     0,  // alignment
                     NULL,  // cryptInfo
                     &chunkSalt
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get salt
  assert(sizeof(chunkSalt.salt) == archiveHandle->archiveCryptInfo->cryptSalt.length);
  Crypt_getSalt(chunkSalt.salt,sizeof(chunkSalt.salt),&archiveHandle->archiveCryptInfo->cryptSalt);

  // write salt
  error = Chunk_create(&chunkSalt.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkSalt.info);
    return error;
  }

  // close chunk
  error = Chunk_close(&chunkSalt.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkSalt.info);
    return error;
  }

  // free resources
  Chunk_done(&chunkSalt.info);

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
  Errors   error;
  ChunkKey chunkKey;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(Semaphore_isOwned(&archiveHandle->lock));

  // init key chunk
  error = Chunk_init(&chunkKey.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_KEY,
                     CHUNK_DEFINITION_KEY,
//TODO: DEFAULT_ALIGNMENT
                     0,  // alignment
                     NULL,  // cryptInfo
                     &chunkKey
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // write encrypted encryption key
  error = Chunk_create(&chunkKey.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkKey.info);
    return error;
  }
  error = Chunk_writeData(&chunkKey.info,
                          archiveHandle->encryptedKeyData,
                          archiveHandle->encryptedKeyDataLength
                         );
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkKey.info);
    return error;
  }

  // close chunk
  error = Chunk_close(&chunkKey.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkKey.info);
    return error;
  }

  // free resources
  Chunk_done(&chunkKey.info);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeMeta
* Purpose: write archive meta chunks
* Input  : archiveHandle - archive handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeMeta(ArchiveHandle *archiveHandle)
{
  Errors         error;
  ChunkMeta      chunkMeta;
  ChunkMetaEntry chunkMetaEntry;
  CryptInfo      cryptInfo;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);

  // init meta chunk
  error = Chunk_init(&chunkMeta.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_META,
                     CHUNK_DEFINITION_META,
                     DEFAULT_ALIGNMENT,
                     NULL,  // cryptInfo,
                     &chunkMeta
                    );
  if (error != ERROR_NONE)
  {
    return error;
  }
#ifdef MULTI_CRYPT
  chunkMeta.cryptAlgorithms[0] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[0]);
  chunkMeta.cryptAlgorithms[1] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[1]);
  chunkMeta.cryptAlgorithms[2] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[2]);
  chunkMeta.cryptAlgorithms[3] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[3]);
#else
  chunkMeta.cryptAlgorithms[0] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0]);
  chunkMeta.cryptAlgorithms[1] = CRYPT_ALGORITHM_NONE;
  chunkMeta.cryptAlgorithms[2] = CRYPT_ALGORITHM_NONE;
  chunkMeta.cryptAlgorithms[3] = CRYPT_ALGORITHM_NONE;
#endif

  // init crypt
  error = Crypt_init(&cryptInfo,
//TODO: MULTI_CRYPT
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkMeta.info);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&cryptInfo); return DEBUG_TESTCODE_ERROR(); }

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
    return error;
  }
  Misc_getCurrentUserName(chunkMetaEntry.userName);
  Network_getHostName(chunkMetaEntry.hostName);
  String_set(chunkMetaEntry.jobUUID,archiveHandle->jobUUID);
  String_set(chunkMetaEntry.scheduleUUID,archiveHandle->scheduleUUID);
  chunkMetaEntry.archiveType     = archiveHandle->archiveType;
  chunkMetaEntry.createdDateTime = Misc_getCurrentDateTime();
  String_set(chunkMetaEntry.comment,archiveHandle->storageInfo->jobOptions->comment);

  // write meta chunks
  error = Chunk_create(&chunkMeta.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkMetaEntry.info);
    Crypt_done(&cryptInfo);
    Chunk_done(&chunkMeta.info);
    return error;
  }
  error = Chunk_create(&chunkMetaEntry.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkMetaEntry.info);
    Crypt_done(&cryptInfo);
    Chunk_done(&chunkMeta.info);
    return error;
  }

  // close chunks
  error = Chunk_close(&chunkMetaEntry.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkMetaEntry.info);
    Crypt_done(&cryptInfo);
    Chunk_done(&chunkMeta.info);
    return error;
  }
  error = Chunk_close(&chunkMeta.info);
  if (error != ERROR_NONE)
  {
    Chunk_done(&chunkMetaEntry.info);
    Crypt_done(&cryptInfo);
    Chunk_done(&chunkMeta.info);
    return error;
  }

  // free resources
  Chunk_done(&chunkMetaEntry.info);
  Crypt_done(&cryptInfo);
  Chunk_done(&chunkMeta.info);

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
  AutoFreeList   autoFreeList;
  CryptHash      signatureHash;
  uint64         index;
  Errors         error;
  byte           hash[MAX_HASH_SIZE];
  uint           hashLength;
  byte           signature[MAX_HASH_SIZE];
  uint           signatureLength;
  ChunkSignature chunkSignature;

  assert(archiveHandle != NULL);
  assert(archiveHandle->chunkIO != NULL);
  assert(archiveHandle->chunkIO->tell != NULL);

  if (Crypt_isKeyAvailable(&globalOptions.signaturePrivateKey))
  {
    // init variables
    AutoFree_init(&autoFreeList);

    // init signature hash
    error = Crypt_initHash(&signatureHash,SIGNATURE_HASH_ALGORITHM);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&signatureHash,{ Crypt_doneHash(&signatureHash); });

    // get signature hash (Note: signature is also calculate from begin of archive)
    error = archiveHandle->chunkIO->tell(archiveHandle->chunkIOUserData,&index);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    error = calcuateHash(archiveHandle->chunkIO,
                         archiveHandle->chunkIOUserData,
                         &signatureHash,
                         0LL,  // start
                         index
                        );
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
/*
TODO: support dynamic hash length?
    hashLength = Crypt_getHashLength(&signatureHash);
    hash = malloc(hashLength);
    if (hash == NULL)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_INSUFFICIENT_MEMORY;
    }
    AUTOFREE_ADD(&autoFreeList,hash,{ free(hash); });
*/
    Crypt_getHash(&signatureHash,hash,sizeof(hash),&hashLength);
//fprintf(stderr,"%s, %d: write signature\n",__FILE__,__LINE__); debugDumpMemory(hash,hashLength,0);

    // get signature
    error = Crypt_getSignature(&globalOptions.signaturePrivateKey,
                               hash,
                               hashLength,
                               signature,
                               sizeof(signature),
                               &signatureLength
                              );
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    assert(signatureLength < sizeof(signature));
#if 0
// test only
    error = Crypt_verifySignature(&globalOptions.signaturePublicKey,
                                  hash,
                                  hashLength,
                                  signature,
                                  signatureLength,
                                  NULL
                                 );
if (error != ERROR_NONE)
{
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
exit(1);
}
#endif

    // init signature chunk
    error = Chunk_init(&chunkSignature.info,
                       NULL,  // parentChunkInfo
                       archiveHandle->chunkIO,
                       archiveHandle->chunkIOUserData,
                       CHUNK_ID_SIGNATURE,
                       CHUNK_DEFINITION_SIGNATURE,
                       DEFAULT_ALIGNMENT,
                       NULL,  // cryptInfo
                       &chunkSignature
                      );
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    chunkSignature.hashAlgorithm = SIGNATURE_HASH_ALGORITHM;
    chunkSignature.value.data    = signature;
    chunkSignature.value.length  = signatureLength;
    AUTOFREE_ADD(&autoFreeList,&chunkSignature.info,{ Chunk_done(&chunkSignature.info); });

    // write signature chunk
    error = Chunk_create(&chunkSignature.info);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    error = Chunk_close(&chunkSignature.info);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // free resources
    Chunk_done(&chunkSignature.info);
    Crypt_doneHash(&signatureHash);
    AutoFree_done(&autoFreeList);
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

LOCAL Errors createArchiveFile(ArchiveHandle *archiveHandle)
{
  AutoFreeList autoFreeList;
  Errors       error;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(archiveHandle->mode == ARCHIVE_MODE_CREATE);
  assert(Semaphore_isOwned(&archiveHandle->lock));

  if (!archiveHandle->create.openFlag)
  {
    // init variables
    AutoFree_init(&autoFreeList);

    SEMAPHORE_LOCKED_DO(&archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      AUTOFREE_ADD(&autoFreeList,&archiveHandle->lock,{ Semaphore_unlock(&archiveHandle->lock); });

      // create intermediate data filename
      error = File_getTmpFileName(archiveHandle->create.tmpFileName,"archive",tmpDirectory);
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      AUTOFREE_ADD(&autoFreeList,&archiveHandle->create.tmpFileName,{ File_delete(archiveHandle->create.tmpFileName,FALSE); });
      DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

      // create temporary file
      error = File_open(&archiveHandle->create.tmpFileHandle,
                        archiveHandle->create.tmpFileName,
                        FILE_OPEN_CREATE
                       );
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      AUTOFREE_ADD(&autoFreeList,&archiveHandle->create.tmpFileHandle,{ File_close(&archiveHandle->create.tmpFileHandle); });
      DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

      // write header
      error = writeHeader(archiveHandle);
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

      // write salt if encryption enabled
      if (archiveHandle->archiveCryptInfo->cryptType != CRYPT_TYPE_NONE)
      {
        // write salt
        error = writeSalt(archiveHandle);
        if (error != ERROR_NONE)
        {
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
      }

      // write encrypted key if asymmetric encryption enabled
      if (archiveHandle->archiveCryptInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
      {
        // write encrypted key
        error = writeEncryptionKey(archiveHandle);
        if (error != ERROR_NONE)
        {
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
      }

      if (archiveHandle->createMeta)
      {
        // write meta data
        error = writeMeta(archiveHandle);
        if (error != ERROR_NONE)
        {
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
      }

      if (   (archiveHandle->indexHandle != NULL)
          && !archiveHandle->storageInfo->jobOptions->noIndexDatabaseFlag
          && !IS_SET(archiveHandle->storageInfo->storageFlags,STORAGE_FLAG_DRY_RUN)
          && !IS_SET(archiveHandle->storageInfo->storageFlags,STORAGE_FLAG_NO_STORAGE)
         )
      {
        // create storage index
        SEMAPHORE_LOCKED_DO(&archiveHandle->indexLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          error = Index_newStorage(archiveHandle->indexHandle,
                                   archiveHandle->entityId,
                                   archiveHandle->hostName,
                                   NULL, // storageName
                                   Misc_getCurrentDateTime(),  // created
                                   0LL,  // size
                                   INDEX_STATE_CREATE,
                                   INDEX_MODE_AUTO,
                                   &archiveHandle->storageId
                                  );
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveHandle->indexLock);
            AutoFree_cleanup(&autoFreeList);
            return error;
          }
        }
        AUTOFREE_ADD(&autoFreeList,&archiveHandle->storageId,
        {
          SEMAPHORE_LOCKED_DO(&archiveHandle->indexLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
          {
            Index_deleteStorage(archiveHandle->indexHandle,archiveHandle->storageId);
          }
        });
      }
      else
      {
        // no index
        archiveHandle->storageId = DATABASE_ID_NONE;
      }

      // call-back for init archive
      if (archiveHandle->archiveInitFunction != NULL)
      {
        error = archiveHandle->archiveInitFunction(archiveHandle->storageInfo,
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

      // mark create archive file "open"
      archiveHandle->create.openFlag = TRUE;
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
* Output : storageId            - storage index id
*          intermediateFileName - intermediate file name
*          partNumber           - part number
*          archiveSize          - archive size [bytes]
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors closeArchiveFile(ArchiveHandle *archiveHandle,
                              IndexId       *storageId,
                              String        intermediateFileName,
                              int           *partNumber,
                              uint64        *archiveSize
                             )
{
  Errors error;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->mode == ARCHIVE_MODE_CREATE);
  assert(Semaphore_isOwned(&archiveHandle->lock));
  assert(storageId != NULL);
  assert(intermediateFileName != NULL);
  assert(partNumber != NULL);
  assert(archiveSize != NULL);

  // init variables
  (*storageId)   = INDEX_ID_NONE;
  String_clear(intermediateFileName);
  (*partNumber)  = ARCHIVE_PART_NUMBER_NONE;
  (*archiveSize) = 0LL;

  if (archiveHandle->create.openFlag)
  {
    // add signature
    if (!archiveHandle->storageInfo->jobOptions->noSignatureFlag)
    {
      error = writeSignature(archiveHandle);
      if (error != ERROR_NONE)
      {
        (void)File_close(&archiveHandle->create.tmpFileHandle);
        archiveHandle->create.openFlag = FALSE;
        Semaphore_unlock(&archiveHandle->lock);
        return error;
      }
    }

    // close file
    (void)File_close(&archiveHandle->create.tmpFileHandle);

    // mark created archive file "closed"
    archiveHandle->create.openFlag = FALSE;

    // get archive file data, increment part number
    (*storageId) = archiveHandle->storageId;
    String_set(intermediateFileName,archiveHandle->create.tmpFileName);
    if (isSplittedArchive(archiveHandle))
    {
      (*partNumber) = archiveHandle->partNumber;
      archiveHandle->partNumber++;
    }
    (*archiveSize) = Archive_getSize(archiveHandle);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storeArchiveFile
* Purpose: store archive file
* Input  : archiveHandle        - archive handle
*          storageId            - storage index id
*          intermediateFileName - intermediate file name
*          partNumber           - part number
*          archiveSize          - archive size [bytes]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors storeArchiveFile(ArchiveHandle *archiveHandle,
                              IndexId       storageId,
                              ConstString   intermediateFileName,
                              int           partNumber,
                              uint64        archiveSize
                             )
{
  Errors error;

  assert(archiveHandle != NULL);

  // call-back to store archive
  if (archiveHandle->archiveStoreFunction != NULL)
  {
    error = archiveHandle->archiveStoreFunction(archiveHandle->storageInfo,
                                                archiveHandle->uuidId,
                                                archiveHandle->jobUUID,
                                                archiveHandle->scheduleUUID,
                                                archiveHandle->entityId,
                                                archiveHandle->archiveType,
                                                storageId,
                                                partNumber,
                                                intermediateFileName,
                                                archiveSize,
                                                archiveHandle->archiveStoreUserData
                                               );
    if (error != ERROR_NONE)
    {
      return error;
    }
    DEBUG_TESTCODE() { return DEBUG_TESTCODE_ERROR(); }
  }

  // call-back for done archive
  if (archiveHandle->archiveDoneFunction != NULL)
  {
    error = archiveHandle->archiveDoneFunction(archiveHandle->storageInfo,
                                               archiveHandle->uuidId,
                                               archiveHandle->jobUUID,
                                               archiveHandle->scheduleUUID,
                                               archiveHandle->entityId,
                                               archiveHandle->archiveType,
                                               storageId,
                                               partNumber,
                                               archiveHandle->archiveDoneUserData
                                              );
    if (error != ERROR_NONE)
    {
      return error;
    }
    DEBUG_TESTCODE() { return DEBUG_TESTCODE_ERROR(); }
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
                                ulong         minBytes
                               )
{
  Errors  error;
  IndexId storageId;
  String  intermediateFileName;
  int     partNumber;
  uint64  archiveSize;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->mode == ARCHIVE_MODE_CREATE);
  assert(Semaphore_isOwned(&archiveHandle->lock));

  // check if split is necessary
  if (isNewPartNeeded(archiveHandle,
                      minBytes
                     )
     )
  {
    // split needed -> close created archive file
    if (archiveHandle->create.openFlag)
    {
      // close archive file
      intermediateFileName = String_new();
      error = closeArchiveFile(archiveHandle,&storageId,intermediateFileName,&partNumber,&archiveSize);
      if (error != ERROR_NONE)
      {
        String_delete(intermediateFileName);
        return error;
      }

      // store archive file
      error = storeArchiveFile(archiveHandle,storageId,intermediateFileName,partNumber,archiveSize);
      if (error != ERROR_NONE)
      {
        String_delete(intermediateFileName);
        return error;
      }
      String_delete(intermediateFileName);
    }
  }

  // create new archive
  error = createArchiveFile(archiveHandle);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : transferToArchive
* Purpose: transfer file data from temporary file to archive, update
*          signature hash
* Input  : archiveHandle - archive handle
*          fileHandle    - file handle of temporary file
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors transferToArchive(const ArchiveHandle *archiveHandle,
                               FileHandle          *fileHandle
                              )
{
  #define TRANSFER_BUFFER_SIZE (1024*1024)

  void    *buffer;
  Errors  error;
  uint64  length;
  ulong   n;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->chunkIO != NULL);
  assert(archiveHandle->chunkIO->write != NULL);
  assert(fileHandle != NULL);
  assert(Semaphore_isOwned(&archiveHandle->lock));

  // init variables
  buffer = malloc(TRANSFER_BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // seek to begin of file
  error = File_seek(fileHandle,0LL);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get length
  length = File_getSize(fileHandle);

  // transfer data
  while (length > 0LL)
  {
    n = MIN(length,TRANSFER_BUFFER_SIZE);

    // read data
    error = File_read(fileHandle,buffer,n,NULL);
    if (error != ERROR_NONE)
    {
      free(buffer);
      return error;
    }

    // write to storage
    error = archiveHandle->chunkIO->write(archiveHandle->chunkIOUserData,buffer,n);
    if (error != ERROR_NONE)
    {
      free(buffer);
      return error;
    }

    length -= (uint64)n;
  }

  // free resources
  free(buffer);

  // truncate file for reusage
  File_truncate(fileHandle,0LL);

  return ERROR_NONE;

  #undef TRANSFER_BUFFER_SIZE
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
  DEBUG_CHECK_RESOURCE_TRACE(archiveEntryInfo->archiveHandle);
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
    assert(Compress_isDeltaCompressed(archiveEntryInfo->file.deltaCompressAlgorithm));

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
  DEBUG_CHECK_RESOURCE_TRACE(archiveEntryInfo->archiveHandle);
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
//TODO
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
  uint    blockCount;
  ulong   byteLength;
  ulong   minBytes;
  bool    newPartFlag;
  Errors  error;
  bool    eofDelta;
  ulong   deltaLength;
  IndexId storageId;
  String  intermediateFileName;
  int     partNumber;
  uint64  archiveSize;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveEntryInfo->archiveHandle);
  assert(archiveEntryInfo->archiveHandle->mode == ARCHIVE_MODE_CREATE);
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
      Semaphore_lock(&archiveEntryInfo->archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);

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
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
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
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
            return error;
          }
        }

        // flush delta compress
        error = Compress_flush(&archiveEntryInfo->file.deltaCompressInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }
        eofDelta = FALSE;
        do
        {
          // flush compressed full data blocks
          error = flushFileDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_FULL);
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
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
              Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
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
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }
        error = flushFileDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_ANY);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // update fragment size
        archiveEntryInfo->file.chunkFileData.fragmentSize = Compress_getInputLength(&archiveEntryInfo->file.deltaCompressInfo);
        error = Chunk_update(&archiveEntryInfo->file.chunkFileData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // close chunks
        error = Chunk_close(&archiveEntryInfo->file.chunkFileData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->file.chunkFileEntry.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->file.chunkFile.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // reset header "written"
        archiveEntryInfo->file.headerWrittenFlag = FALSE;

        // create archive file (if not already exists and open)
        error = createArchiveFile(archiveEntryInfo->archiveHandle);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // transfer intermediate data into archive
        error = transferToArchive(archiveEntryInfo->archiveHandle,
                                  &archiveEntryInfo->file.intermediateFileHandle
                                 );
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // store into index database
        if (archiveEntryInfo->archiveHandle->indexHandle != NULL)
        {
          error = indexAddFile(archiveEntryInfo->archiveHandle,
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
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
            return error;
          }
        }

        // close archive
        intermediateFileName = String_new();
        error = closeArchiveFile(archiveEntryInfo->archiveHandle,&storageId,intermediateFileName,&partNumber,&archiveSize);
        if (error != ERROR_NONE)
        {
          String_delete(intermediateFileName);
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // update fragment offset for next fragment, reset size
        archiveEntryInfo->file.chunkFileData.fragmentOffset += archiveEntryInfo->file.chunkFileData.fragmentSize;
        archiveEntryInfo->file.chunkFileData.fragmentSize   =  0LL;

        // set new delta base-offset
        if (archiveEntryInfo->file.deltaSourceHandleInitFlag)
        {
          assert(Compress_isDeltaCompressed(archiveEntryInfo->file.deltaCompressAlgorithm));
          DeltaSource_setBaseOffset(&archiveEntryInfo->file.deltaSourceHandle,
                                    archiveEntryInfo->file.chunkFileData.fragmentSize
                                   );
        }

        /* reset compress, encrypt (do it here because data if buffered and can be
          processed before a new file is opened)
        */
        Compress_reset(&archiveEntryInfo->file.deltaCompressInfo);
        Compress_reset(&archiveEntryInfo->file.byteCompressInfo);
        Crypt_reset(&archiveEntryInfo->file.cryptInfo);

        // find next suitable archive part
        findNextArchivePart(archiveEntryInfo->archiveHandle);

        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);

        // store archive file
        error = storeArchiveFile(archiveEntryInfo->archiveHandle,storageId,intermediateFileName,partNumber,archiveSize);
        if (error != ERROR_NONE)
        {
          String_delete(intermediateFileName);
          return error;
        }
        String_delete(intermediateFileName);
      }
      else
      {
        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);

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
  DEBUG_CHECK_RESOURCE_TRACE(archiveEntryInfo->archiveHandle);
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
    assert(Compress_isDeltaCompressed(archiveEntryInfo->image.deltaCompressAlgorithm));

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
  DEBUG_CHECK_RESOURCE_TRACE(archiveEntryInfo->archiveHandle);
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
* Purpose: encrypt and write image data blocks, split archive
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
  uint    blockCount;
  ulong   byteLength;
  ulong   minBytes;
  bool    newPartFlag;
  Errors  error;
  bool    eofDelta;
  ulong   deltaLength;
  IndexId storageId;
  String  intermediateFileName;
  int     partNumber;
  uint64  archiveSize;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveEntryInfo->archiveHandle);
  assert(archiveEntryInfo->archiveHandle->mode == ARCHIVE_MODE_CREATE);
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
      Semaphore_lock(&archiveEntryInfo->archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);

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
        assert(archiveEntryInfo->image.headerWrittenFlag);

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
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
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
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
            return error;
          }
        }

        // flush delta compress
        error = Compress_flush(&archiveEntryInfo->image.deltaCompressInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }
        eofDelta = FALSE;
        do
        {
          // flush compressed full data blocks
          error = flushImageDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_FULL);
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
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
              Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
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
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }
        error = flushImageDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_ANY);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // update part size
        assert(archiveEntryInfo->image.blockSize > 0);
        archiveEntryInfo->image.chunkImageData.blockCount = Compress_getInputLength(&archiveEntryInfo->image.deltaCompressInfo)/archiveEntryInfo->image.blockSize;
        error = Chunk_update(&archiveEntryInfo->image.chunkImageData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // close chunks
        error = Chunk_close(&archiveEntryInfo->image.chunkImageData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->image.chunkImageEntry.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->image.chunkImage.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // reset header "written"
        archiveEntryInfo->image.headerWrittenFlag = FALSE;

        // create archive file (if not already exists and open)
        error = createArchiveFile(archiveEntryInfo->archiveHandle);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // transfer intermediate data into archive
        error = transferToArchive(archiveEntryInfo->archiveHandle,
                                  &archiveEntryInfo->image.intermediateFileHandle
                                 );
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // store in index database
        if (archiveEntryInfo->archiveHandle->indexHandle != NULL)
        {
          error = indexAddImage(archiveEntryInfo->archiveHandle,
                                archiveEntryInfo->archiveHandle->storageId,
                                archiveEntryInfo->image.chunkImageEntry.name,
                                archiveEntryInfo->image.chunkImageEntry.fileSystemType,
                                archiveEntryInfo->image.chunkImageEntry.size,
                                archiveEntryInfo->image.chunkImageEntry.blockSize,
                                archiveEntryInfo->image.chunkImageData.blockOffset,
                                archiveEntryInfo->image.chunkImageData.blockCount
                               );
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
            return error;
          }
        }

        // close archive
        intermediateFileName = String_new();
        error = closeArchiveFile(archiveEntryInfo->archiveHandle,&storageId,intermediateFileName,&partNumber,&archiveSize);
        if (error != ERROR_NONE)
        {
          String_delete(intermediateFileName);
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // store block offset, count for next fragment
        archiveEntryInfo->image.chunkImageData.blockOffset += archiveEntryInfo->image.chunkImageData.blockCount;
        archiveEntryInfo->image.chunkImageData.blockCount  =  0;

        // set new delta base-offset
        if (archiveEntryInfo->image.deltaSourceHandleInitFlag)
        {
          assert(Compress_isDeltaCompressed(archiveEntryInfo->image.deltaCompressAlgorithm));
          DeltaSource_setBaseOffset(&archiveEntryInfo->image.deltaSourceHandle,
                                    archiveEntryInfo->image.chunkImageData.blockCount*(uint64)archiveEntryInfo->image.blockSize
                                   );
        }

        /* reset compress, encrypt (do it here because data if buffered and can be
          processed before a new file is opened)
        */
        Compress_reset(&archiveEntryInfo->image.deltaCompressInfo);
        Compress_reset(&archiveEntryInfo->image.byteCompressInfo);
        Crypt_reset(&archiveEntryInfo->image.cryptInfo);

        // find next suitable archive part
        findNextArchivePart(archiveEntryInfo->archiveHandle);

        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);

        // store archive file
        error = storeArchiveFile(archiveEntryInfo->archiveHandle,storageId,intermediateFileName,partNumber,archiveSize);
        if (error != ERROR_NONE)
        {
          String_delete(intermediateFileName);
          return error;
        }
        String_delete(intermediateFileName);
      }
      else
      {
        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);

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
  DEBUG_CHECK_RESOURCE_TRACE(archiveEntryInfo->archiveHandle);
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
    assert(Compress_isDeltaCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm));

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
  DEBUG_CHECK_RESOURCE_TRACE(archiveEntryInfo->archiveHandle);
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
  IndexId          storageId;
  String           intermediateFileName;
  int              partNumber;
  uint64           archiveSize;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveEntryInfo->archiveHandle);
  assert(archiveEntryInfo->archiveHandle->mode == ARCHIVE_MODE_CREATE);
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
      Semaphore_lock(&archiveEntryInfo->archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);

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
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
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
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
            return error;
          }
        }

        // flush delta compress
        error = Compress_flush(&archiveEntryInfo->hardLink.deltaCompressInfo);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }
        eofDelta = FALSE;
        do
        {
          // flush compressed full data blocks
          error = flushHardLinkDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_FULL);
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
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
              Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
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
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }
        error = flushHardLinkDataBlocks(archiveEntryInfo,COMPRESS_BLOCK_TYPE_ANY);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // update fragment size
        archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize = Compress_getInputLength(&archiveEntryInfo->hardLink.deltaCompressInfo);
        error = Chunk_update(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // close chunks
        error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkData.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }
        error = Chunk_close(&archiveEntryInfo->hardLink.chunkHardLink.info);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // reset header "written"
        archiveEntryInfo->hardLink.headerWrittenFlag = FALSE;

        // create archive file (if not already exists and open)
        error = createArchiveFile(archiveEntryInfo->archiveHandle);
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // transfer intermediate data into archive
        error = transferToArchive(archiveEntryInfo->archiveHandle,
                                  &archiveEntryInfo->hardLink.intermediateFileHandle
                                 );
        if (error != ERROR_NONE)
        {
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // store in index database
        if (archiveEntryInfo->archiveHandle->indexHandle != NULL)
        {
          STRINGLIST_ITERATE(archiveEntryInfo->hardLink.fileNameList,stringNode,fileName)
          {
            error = indexAddFile(archiveEntryInfo->archiveHandle,
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
          if (error != ERROR_NONE)
          {
            Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
            return error;
          }
        }

        // close archive
        intermediateFileName = String_new();
        error = closeArchiveFile(archiveEntryInfo->archiveHandle,&storageId,intermediateFileName,&partNumber,&archiveSize);
        if (error != ERROR_NONE)
        {
          String_delete(intermediateFileName);
          Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
          return error;
        }

        // update fragment offset for next fragment, reset size
        archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset += archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize;
        archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize   =  0LL;

        // set new delta base-offset
        if (archiveEntryInfo->hardLink.deltaSourceHandleInitFlag)
        {
          assert(Compress_isDeltaCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm));
          DeltaSource_setBaseOffset(&archiveEntryInfo->hardLink.deltaSourceHandle,
                                    archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset
                                   );
        }

        /* reset compress, encrypt (do it here because data if buffered and can be
          processed before a new file is opened)
        */
        Compress_reset(&archiveEntryInfo->hardLink.deltaCompressInfo);
        Compress_reset(&archiveEntryInfo->hardLink.byteCompressInfo);
        Crypt_reset(&archiveEntryInfo->hardLink.cryptInfo);

        // find next suitable archive part
        findNextArchivePart(archiveEntryInfo->archiveHandle);

        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);

        // store archive file
        error = storeArchiveFile(archiveEntryInfo->archiveHandle,storageId,intermediateFileName,partNumber,archiveSize);
        if (error != ERROR_NONE)
        {
          String_delete(intermediateFileName);
          return error;
        }
        String_delete(intermediateFileName);
      }
      else
      {
        // unlock
        Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);

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
  Semaphore_init(&decryptPasswordList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&decryptPasswordList);
  decryptPasswordList.newPasswordNode = NULL;

  Semaphore_init(&decryptKeyList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&decryptKeyList);
  decryptKeyList.newDecryptKeyNode = NULL;

  return ERROR_NONE;
}

void Archive_doneAll(void)
{
  List_done(&decryptKeyList,(ListNodeFreeFunction)freeDecryptKeyNode,NULL);
  Semaphore_done(&decryptKeyList.lock);

  List_done(&decryptPasswordList,(ListNodeFreeFunction)freePasswordNode,NULL);
  Semaphore_done(&decryptPasswordList.lock);
}

bool Archive_parseType(const char *name, ArchiveTypes *archiveType, void *userData)
{
  uint i;

  assert(name != NULL);
  assert(archiveType != NULL);

  UNUSED_VARIABLE(userData);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(ARCHIVE_TYPES))
         && !stringEqualsIgnoreCase(ARCHIVE_TYPES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(ARCHIVE_TYPES))
  {
    (*archiveType) = ARCHIVE_TYPES[i].archiveType;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

const char *Archive_archiveTypeToString(ArchiveTypes archiveType, const char *defaultValue)
{
  return ((ARRAY_FIRST(ARCHIVE_TYPES).archiveType <= archiveType) && (archiveType <= ARRAY_LAST(ARCHIVE_TYPES).archiveType))
           ? ARCHIVE_TYPES[archiveType-ARRAY_FIRST(ARCHIVE_TYPES).archiveType].name
           : defaultValue;
}

const char *Archive_archiveTypeToShortString(ArchiveTypes archiveType, const char *defaultValue)
{
  return ((ARRAY_FIRST(ARCHIVE_TYPES).archiveType <= archiveType) && (archiveType <= ARRAY_LAST(ARCHIVE_TYPES).archiveType))
           ? ARCHIVE_TYPES[archiveType-ARRAY_FIRST(ARCHIVE_TYPES).archiveType].name
           : defaultValue;
}

const char *Archive_archiveEntryTypeToString(ArchiveEntryTypes archiveEntryType, const char *defaultValue)
{
  return ((ARRAY_FIRST(ARCHIVE_ENTRY_TYPES).archiveEntryType <= archiveEntryType) && (archiveEntryType <= ARRAY_LAST(ARCHIVE_ENTRY_TYPES).archiveEntryType))
           ? ARCHIVE_ENTRY_TYPES[archiveEntryType-ARRAY_FIRST(ARCHIVE_ENTRY_TYPES).archiveEntryType].name
           : defaultValue;
}

const char *Archive_typeToString(ArchiveTypes archiveType, const char *defaultValue)
{
  uint       z;
  const char *name;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(ARCHIVE_TYPES))
         && (ARCHIVE_TYPES[z].archiveType != archiveType)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(ARCHIVE_TYPES))
  {
    name = ARCHIVE_TYPES[z].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

const char *Archive_typeToShortString(ArchiveTypes archiveType)
{
  uint       z;
  const char *name;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(ARCHIVE_TYPES))
         && (ARCHIVE_TYPES[z].archiveType != archiveType)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(ARCHIVE_TYPES))
  {
    name = ARCHIVE_TYPES[z].shortName;
  }
  else
  {
    name = "U";
  }

  return name;
}

bool Archive_parseArchiveEntryType(const char *name, ArchiveEntryTypes *archiveEntryType)
{
  uint i;

  assert(name != NULL);
  assert(archiveEntryType != NULL);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(ARCHIVE_ENTRY_TYPES))
         && !stringEqualsIgnoreCase(ARCHIVE_ENTRY_TYPES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(ARCHIVE_ENTRY_TYPES))
  {
    (*archiveEntryType) = ARCHIVE_ENTRY_TYPES[i].archiveEntryType;
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

  return (chunkHeader.id == CHUNK_ID_BAR);
}

void Archive_clearDecryptPasswords(void)
{
  SEMAPHORE_LOCKED_DO(&decryptPasswordList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    List_clear(&decryptPasswordList,(ListNodeFreeFunction)freePasswordNode,NULL);
  }
}

const Password *Archive_appendDecryptPassword(const Password *password)
{
  PasswordNode *passwordNode;

  assert(password != NULL);

  passwordNode = NULL;
  SEMAPHORE_LOCKED_DO(&decryptPasswordList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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

//TODO: remove
#if 0
const CryptKey *Archive_appendCryptInfo(const Password *password)
{
  DecryptKeyNode *decryptKeyNode;

  assert(password != NULL);

  decryptKeyNode = NULL;
  SEMAPHORE_LOCKED_DO(&decryptKeyList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
//TODO:
//    cryptInfo = addCryptInfo();
    // find crypt key
    decryptKeyNode = LIST_FIND(&decryptKeyList,decryptKeyNode,Password_equals(decryptKeyNode->password,password));
    if (decryptKeyNode == NULL)
    {
      // add new decrypt key
      decryptKeyNode = LIST_NEW_NODE(DecryptKeyNode);
      if (decryptKeyNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
//      decryptKeyNode->cryptAlgorithm = cryptAlgorithm;
      Crypt_initKey(&decryptKeyNode->cryptKey,CRYPT_PADDING_TYPE_NONE);
      decryptKeyNode->password = Password_duplicate(password);

      // derive decrypt key from password with salt
//TODO:
#if 0
      error = Crypt_deriveKey(&decryptKeyNode->cryptKey,
                              salt,
                              password,
                              keyLength
                             );
//TODO: error?
      if (error != ERROR_NONE)
      {
        Password_delete(decryptKeyNode->password);
        Crypt_doneKey(&decryptKeyNode->cryptKey);
        LIST_DELETE_NODE(decryptKeyNode);
        return NULL;
      }
#endif

      // add to decrypt key list
      List_append(&decryptKeyList,decryptKeyNode);
    }

    // set reference to new added crypt fino
    decryptKeyList.newDecryptKeyNode = decryptKeyNode;
  }
  assert(decryptKeyNode != NULL);

  return &decryptKeyNode->cryptKey;
}
#endif

bool Archive_waitDecryptPassword(Password *password, long timeout)
{
  bool modified;

  Password_clear(password);

  modified = FALSE;
  SEMAPHORE_LOCKED_DO(&decryptPasswordList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
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
  Errors Archive_create(ArchiveHandle           *archiveHandle,
                        ConstString             hostName,
                        StorageInfo             *storageInfo,
                        ConstString             archiveName,
//                        IndexHandle             *indexHandle,
                        IndexId                 uuidId,
                        IndexId                 entityId,
                        ConstString             jobUUID,
                        ConstString             scheduleUUID,
                        DeltaSourceList         *deltaSourceList,
                        ArchiveTypes            archiveType,
                        const Password          *password,
                        bool                    createMeta,
                        bool                    dryRun,
                        ArchiveInitFunction     archiveInitFunction,
                        void                    *archiveInitUserData,
                        ArchiveDoneFunction     archiveDoneFunction,
                        void                    *archiveDoneUserData,
                        ArchiveGetSizeFunction  archiveGetSizeFunction,
                        void                    *archiveGetSizeUserData,
                        ArchiveStoreFunction    archiveStoreFunction,
                        void                    *archiveStoreUserData,
                        GetNamePasswordFunction getNamePasswordFunction,
                        void                    *getNamePasswordUserData,
                        LogHandle               *logHandle
                       )
#else /* not NDEBUG */
  Errors __Archive_create(const char              *__fileName__,
                          ulong                    __lineNb__,
                          ArchiveHandle           *archiveHandle,
                          ConstString             hostName,
                          StorageInfo             *storageInfo,
                          ConstString             archiveName,
//                          IndexHandle             *indexHandle,
                          IndexId                 uuidId,
                          IndexId                 entityId,
                          ConstString             jobUUID,
                          ConstString             scheduleUUID,
                          DeltaSourceList         *deltaSourceList,
                          ArchiveTypes            archiveType,
                          const Password          *password,
                          bool                    createMeta,
                          bool                    dryRun,
                          ArchiveInitFunction     archiveInitFunction,
                          void                    *archiveInitUserData,
                          ArchiveDoneFunction     archiveDoneFunction,
                          void                    *archiveDoneUserData,
                          ArchiveGetSizeFunction  archiveGetSizeFunction,
                          void                    *archiveGetSizeUserData,
                          ArchiveStoreFunction    archiveStoreFunction,
                          void                    *archiveStoreUserData,
                          GetNamePasswordFunction getNamePasswordFunction,
                          void                    *getNamePasswordUserData,
                          LogHandle               *logHandle
                         )
#endif /* NDEBUG */
{
  AutoFreeList         autoFreeList;
  ArchiveCryptInfoNode *archiveCryptInfoNode;
  Errors               error;
  uint                 keyLength;
  CryptKey             publicCryptKey;
  bool                 okFlag;
  ulong                maxEncryptedKeyDataLength;

  assert(archiveHandle != NULL);
  assert(storageInfo != NULL);
  assert(storageInfo->jobOptions != NULL);
  assert(archiveStoreFunction != NULL);
//  assert((indexHandle == NULL) || (entityId == INDEX_ID_NONE) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));

//TODO:
UNUSED_VARIABLE(storageInfo);

  // init variables
  AutoFree_init(&autoFreeList);

  // init archive info
  archiveHandle->hostName                = String_duplicate(hostName);
  AUTOFREE_ADD(&autoFreeList,archiveHandle->hostName,{ String_delete(archiveHandle->hostName); });
  archiveHandle->storageInfo             = storageInfo;
  archiveHandle->uuidId                  = uuidId;
  archiveHandle->entityId                = entityId;

  archiveHandle->jobUUID                 = String_duplicate(jobUUID);
  AUTOFREE_ADD(&autoFreeList,archiveHandle->jobUUID,{ String_delete(archiveHandle->jobUUID); });
  archiveHandle->scheduleUUID            = String_duplicate(scheduleUUID);
  AUTOFREE_ADD(&autoFreeList,archiveHandle->scheduleUUID,{ String_delete(archiveHandle->scheduleUUID); });

  archiveHandle->deltaSourceList         = deltaSourceList;
  archiveHandle->archiveType             = archiveType;
  archiveHandle->createMeta              = createMeta;
  archiveHandle->dryRun                  = dryRun;

  archiveHandle->archiveInitFunction     = archiveInitFunction;
  archiveHandle->archiveInitUserData     = archiveInitUserData;
  archiveHandle->archiveDoneFunction     = archiveDoneFunction;
  archiveHandle->archiveDoneUserData     = archiveDoneUserData;
  archiveHandle->archiveGetSizeFunction  = archiveGetSizeFunction;
  archiveHandle->archiveGetSizeUserData  = archiveGetSizeUserData;
  archiveHandle->archiveStoreFunction    = archiveStoreFunction;
  archiveHandle->archiveStoreUserData    = archiveStoreUserData;
  archiveHandle->getNamePasswordFunction = getNamePasswordFunction;
  archiveHandle->getNamePasswordUserData = getNamePasswordUserData;
  archiveHandle->logHandle               = logHandle;

  List_init(&archiveHandle->archiveCryptInfoList);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->archiveCryptInfoList,{ List_done(&archiveHandle->archiveCryptInfoList,(ListNodeFreeFunction)freeArchiveCryptInfoNode,NULL); });
  archiveHandle->archiveCryptInfo        = NULL;

  Semaphore_init(&archiveHandle->passwordLock,SEMAPHORE_TYPE_BINARY);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->passwordLock,{ Semaphore_done(&archiveHandle->passwordLock); });
  archiveHandle->cryptPassword           = NULL;
  archiveHandle->cryptPasswordReadFlag   = FALSE;
  archiveHandle->encryptedKeyData        = NULL;
  archiveHandle->encryptedKeyDataLength  = 0;
  archiveHandle->signatureKeyData        = NULL;
  archiveHandle->signatureKeyDataLength  = 0;

  archiveHandle->mode                    = ARCHIVE_MODE_CREATE;
  Semaphore_init(&archiveHandle->lock,SEMAPHORE_TYPE_BINARY);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->lock,{ Semaphore_done(&archiveHandle->lock); });
  archiveHandle->archiveName             = String_duplicate(archiveName);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->archiveName,{ String_delete(archiveHandle->archiveName); });
  archiveHandle->printableStorageName    = NULL;
  archiveHandle->create.tmpFileName      = String_new();
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->create.tmpFileName,{ String_delete(archiveHandle->create.tmpFileName); });
  archiveHandle->create.openFlag         = FALSE;
  archiveHandle->chunkIO                 = &CHUNK_IO_FILE;
  archiveHandle->chunkIOUserData         = &archiveHandle->create.tmpFileHandle;

  Semaphore_init(&archiveHandle->indexLock,SEMAPHORE_TYPE_BINARY);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->indexLock,{ Semaphore_done(&archiveHandle->indexLock); });
  archiveHandle->indexHandle             = NULL;
  archiveHandle->storageId               = DATABASE_ID_NONE;
  List_init(&archiveHandle->archiveIndexList);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->archiveIndexList,{ List_done(&archiveHandle->archiveIndexList,(ListNodeFreeFunction)CALLBACK(freeArchiveIndexNode,NULL)); });
  Semaphore_init(&archiveHandle->archiveIndexList.lock,SEMAPHORE_TYPE_BINARY);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->archiveIndexList.lock,{ Semaphore_done(&archiveHandle->archiveIndexList.lock); });

  archiveHandle->entries                 = 0LL;
  archiveHandle->archiveFileSize         = 0LL;
  archiveHandle->partNumber              = 0;

  archiveHandle->pendingError            = ERROR_NONE;
  archiveHandle->nextChunkHeaderReadFlag = FALSE;

  archiveHandle->interrupt.openFlag      = FALSE;
  archiveHandle->interrupt.offset        = 0LL;

  // open index
  archiveHandle->indexHandle = Index_open((archiveHandle->storageInfo->type == STORAGE_TYPE_MASTER) ? archiveHandle->storageInfo->master.io : NULL,
                                          INDEX_TIMEOUT
                                         );
  if (archiveHandle->indexHandle != NULL)
  {
    AUTOFREE_ADD(&autoFreeList,archiveHandle->indexHandle,{ Index_close(archiveHandle->indexHandle); });
  }

  // create new crypt info
  archiveCryptInfoNode = addArchiveCryptInfoNode(archiveHandle,
                                                 Crypt_isEncrypted(storageInfo->jobOptions->cryptAlgorithms[0])
                                                   ? storageInfo->jobOptions->cryptType
                                                   : CRYPT_TYPE_NONE,
                                                 CRYPT_MODE_NONE,
                                                 CRYPT_KEY_DERIVE_FUNCTION
                                                );
  assert(archiveHandle->archiveCryptInfo != NULL);

  // create intitial random crypt salt
  Crypt_randomSalt(&archiveCryptInfoNode->archiveCryptInfo.cryptSalt);

  // detect crypt block length, crypt key length
  error = Crypt_getBlockLength(storageInfo->jobOptions->cryptAlgorithms[0],&archiveHandle->blockLength);
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  assert(archiveHandle->blockLength > 0);
  if (archiveHandle->blockLength > MAX_BUFFER_SIZE)
  {
    AutoFree_cleanup(&autoFreeList);
    return ERROR_UNSUPPORTED_BLOCK_LENGTH;
  }
  error = Crypt_getKeyLength(storageInfo->jobOptions->cryptAlgorithms[0],&keyLength);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // init encryption key
  switch (archiveHandle->archiveCryptInfo->cryptType)
  {
    case CRYPT_TYPE_NONE:
      // nothing to do
      break;
    case CRYPT_TYPE_SYMMETRIC:
      // init crypt password
      error = initCryptPassword(archiveHandle,password);
      if (error !=  ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }

      // derive crypt key from password with salt
      error = Crypt_deriveKey(&archiveCryptInfoNode->archiveCryptInfo.cryptKey,
                              archiveCryptInfoNode->archiveCryptInfo.cryptKeyDeriveType,
                              &archiveCryptInfoNode->archiveCryptInfo.cryptSalt,
                              archiveHandle->cryptPassword,
                              keyLength
                             );
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      break;
    case CRYPT_TYPE_ASYMMETRIC:
      {
        const Key *cryptPublicKey;

        // check if public key available
        if      (isKeyAvailable(&storageInfo->jobOptions->cryptPublicKey))
        {
          cryptPublicKey = &storageInfo->jobOptions->cryptPublicKey;
        }
        else if (isKeyAvailable(&globalOptions.cryptPublicKey))
        {
          cryptPublicKey = &globalOptions.cryptPublicKey;
        }
        else
        {
          AutoFree_cleanup(&autoFreeList);
          return ERROR_NO_PUBLIC_CRYPT_KEY;
        }

        // init public key
        Crypt_initKey(&publicCryptKey,CRYPT_PADDING_TYPE_NONE);
        error = Crypt_setPublicPrivateKeyData(&publicCryptKey,
                                              cryptPublicKey->data,
                                              cryptPublicKey->length,
                                              CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                                              CRYPT_KEY_DERIVE_NONE,
                                              NULL,  // cryptSalt
                                              NULL  // password
                                             );
        if (error != ERROR_NONE)
        {
          Crypt_doneKey(&publicCryptKey);
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
        AUTOFREE_ADD(&autoFreeList,&archiveHandle->cryptPassword,{ Crypt_doneKey(&publicCryptKey); });

        // create new random key for encryption
        maxEncryptedKeyDataLength = 2*MAX_PASSWORD_LENGTH;
        okFlag = FALSE;
        do
        {
          archiveHandle->encryptedKeyData = malloc(maxEncryptedKeyDataLength);
          if (archiveHandle->encryptedKeyData == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          error = Crypt_getRandomCryptKey(&archiveCryptInfoNode->archiveCryptInfo.cryptKey,
                                          keyLength,
                                          &publicCryptKey,
                                          archiveHandle->encryptedKeyData,
                                          maxEncryptedKeyDataLength,
                                          &archiveHandle->encryptedKeyDataLength
                                         );
          if (error != ERROR_NONE)
          {
            free(archiveHandle->encryptedKeyData);
            AutoFree_cleanup(&autoFreeList);
            return error;
          }
          if (archiveHandle->encryptedKeyDataLength < maxEncryptedKeyDataLength)
          {
            okFlag = TRUE;
          }
          else
          {
            free(archiveHandle->encryptedKeyData);
            maxEncryptedKeyDataLength += 64;
          }
        }
        while (!okFlag);
//fprintf(stderr,"%s, %d: random encrypt key %p %d %p\n",__FILE__,__LINE__,archiveHandle->cryptKey.data,archiveHandle->cryptKey.dataLength,archiveHandle->cryptKey.key); debugDumpMemory(archiveHandle->cryptKey.data,archiveHandle->cryptKey.dataLength,0);
        AUTOFREE_ADD(&autoFreeList,&archiveHandle->encryptedKeyData,{ free(archiveHandle->encryptedKeyData); });
        DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

        // free resources
        Crypt_doneKey(&publicCryptKey);
      }
      break;
  }

  // free resources
  AutoFree_done(&autoFreeList);

  #ifdef DEBUG_ENCODED_DATA_FILENAME
    {
      int handle = open64(DEBUG_ENCODED_DATA_FILENAME,O_CREAT|O_WRONLY|O_TRUNC|O_LARGEFILE,0664);
      close(handle);
    }
  #endif /* DEBUG_ENCODED_DATA_FILENAME */

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveHandle,ArchiveHandle);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveHandle,ArchiveHandle);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_open(ArchiveHandle           *archiveHandle,
                      StorageInfo             *storageInfo,
                      ConstString             archiveName,
                      DeltaSourceList         *deltaSourceList,
                      GetNamePasswordFunction getNamePasswordFunction,
                      void                    *getNamePasswordUserData,
                      LogHandle               *logHandle
                     )
#else /* not NDEBUG */
  Errors __Archive_open(const char              *__fileName__,
                        ulong                   __lineNb__,
                        ArchiveHandle           *archiveHandle,
                        StorageInfo             *storageInfo,
                        ConstString             archiveName,
                        DeltaSourceList         *deltaSourceList,
                        GetNamePasswordFunction getNamePasswordFunction,
                        void                    *getNamePasswordUserData,
                        LogHandle               *logHandle
                       )
#endif /* NDEBUG */
{
  AutoFreeList autoFreeList;
  Errors       error;
  ChunkHeader  chunkHeader;

  assert(archiveHandle != NULL);
  assert(storageInfo != NULL);
  assert(storageInfo->jobOptions != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  archiveHandle->hostName                = NULL;
  archiveHandle->storageInfo             = storageInfo;
  archiveHandle->indexHandle             = NULL;
  archiveHandle->uuidId                  = DATABASE_ID_NONE;
  archiveHandle->entityId                = DATABASE_ID_NONE;

  archiveHandle->jobUUID                 = NULL;
  archiveHandle->scheduleUUID            = NULL;

  archiveHandle->deltaSourceList         = deltaSourceList;
  archiveHandle->archiveType             = ARCHIVE_TYPE_NONE;
  archiveHandle->createMeta              = FALSE;
  archiveHandle->dryRun                  = FALSE;

  archiveHandle->archiveInitFunction     = NULL;
  archiveHandle->archiveInitUserData     = NULL;
  archiveHandle->archiveDoneFunction     = NULL;
  archiveHandle->archiveDoneUserData     = NULL;
  archiveHandle->archiveStoreFunction    = NULL;
  archiveHandle->archiveStoreUserData    = NULL;
  archiveHandle->getNamePasswordFunction = getNamePasswordFunction;
  archiveHandle->getNamePasswordUserData = getNamePasswordUserData;
  archiveHandle->logHandle               = logHandle;

  List_init(&archiveHandle->archiveCryptInfoList);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->archiveCryptInfoList,{ List_done(&archiveHandle->archiveCryptInfoList,(ListNodeFreeFunction)freeArchiveCryptInfoNode,NULL); });
  archiveHandle->archiveCryptInfo        = NULL;

  Semaphore_init(&archiveHandle->passwordLock,SEMAPHORE_TYPE_BINARY);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->passwordLock,{ Semaphore_done(&archiveHandle->passwordLock); });
  archiveHandle->cryptPassword           = NULL;
  archiveHandle->cryptPasswordReadFlag   = FALSE;
  archiveHandle->encryptedKeyData        = NULL;
  archiveHandle->encryptedKeyDataLength  = 0;
  archiveHandle->signatureKeyDataLength  = 0;

  archiveHandle->mode                    = ARCHIVE_MODE_READ;
  Semaphore_init(&archiveHandle->lock,SEMAPHORE_TYPE_BINARY);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->lock,{ Semaphore_done(&archiveHandle->lock); });
  archiveHandle->archiveName             = String_duplicate(archiveName);
  AUTOFREE_ADD(&autoFreeList,archiveHandle->archiveName,{ String_delete(archiveHandle->archiveName); });
  archiveHandle->printableStorageName    = Storage_getPrintableName(String_new(),&storageInfo->storageSpecifier,archiveName);
  AUTOFREE_ADD(&autoFreeList,archiveHandle->printableStorageName,{ String_delete(archiveHandle->printableStorageName); });
  archiveHandle->read.storageFileName    = NULL;
  archiveHandle->chunkIO                 = &CHUNK_IO_STORAGE;
  archiveHandle->chunkIOUserData         = &archiveHandle->read.storageHandle;

  Semaphore_init(&archiveHandle->indexLock,SEMAPHORE_TYPE_BINARY);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->indexLock,{ Semaphore_done(&archiveHandle->indexLock); });
  archiveHandle->indexHandle             = NULL;
  archiveHandle->storageId               = DATABASE_ID_NONE;
  List_init(&archiveHandle->archiveIndexList);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->archiveIndexList,{ List_done(&archiveHandle->archiveIndexList,(ListNodeFreeFunction)CALLBACK(freeArchiveIndexNode,NULL)); });
  Semaphore_init(&archiveHandle->archiveIndexList.lock,SEMAPHORE_TYPE_BINARY);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->archiveIndexList.lock,{ Semaphore_done(&archiveHandle->archiveIndexList.lock); });

  archiveHandle->entries                 = 0LL;
  archiveHandle->archiveFileSize         = 0LL;
  archiveHandle->partNumber              = 0;

  archiveHandle->pendingError            = ERROR_NONE;
  archiveHandle->nextChunkHeaderReadFlag = FALSE;

  archiveHandle->interrupt.openFlag      = FALSE;
  archiveHandle->interrupt.offset        = 0LL;

  // open index
  archiveHandle->indexHandle = Index_open((archiveHandle->storageInfo->type == STORAGE_TYPE_MASTER) ? archiveHandle->storageInfo->master.io : NULL,
                                          INDEX_TIMEOUT
                                         );
  if (archiveHandle->indexHandle != NULL)
  {
    AUTOFREE_ADD(&autoFreeList,archiveHandle->indexHandle,{ Index_close(archiveHandle->indexHandle); });
  }

  // open storage
  error = Storage_open(&archiveHandle->read.storageHandle,
                       archiveHandle->storageInfo,
                       archiveHandle->archiveName
                      );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->read.storageHandle,{ Storage_close(&archiveHandle->read.storageHandle); });
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
    if (!storageInfo->jobOptions->noStopOnErrorFlag)
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
    DEBUG_ADD_RESOURCE_TRACE(archiveHandle,ArchiveHandle);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveHandle,ArchiveHandle);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_openHandle(ArchiveHandle       *archiveHandle,
                            const ArchiveHandle *fromArchiveHandle
                           )
#else /* not NDEBUG */
  Errors __Archive_openHandle(const char          *__fileName__,
                              ulong               __lineNb__,
                              ArchiveHandle       *archiveHandle,
                              const ArchiveHandle *fromArchiveHandle
                             )
#endif /* NDEBUG */
{
  AutoFreeList autoFreeList;
  Errors       error;

  assert(archiveHandle != NULL);
  assert(fromArchiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(fromArchiveHandle);
  assert(fromArchiveHandle->storageInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  archiveHandle->hostName                = NULL;
  archiveHandle->storageInfo             = fromArchiveHandle->storageInfo;
  archiveHandle->indexHandle             = NULL;
  archiveHandle->uuidId                  = DATABASE_ID_NONE;
  archiveHandle->entityId                = DATABASE_ID_NONE;

  archiveHandle->jobUUID                 = NULL;
  archiveHandle->scheduleUUID            = NULL;

  archiveHandle->deltaSourceList         = fromArchiveHandle->deltaSourceList;
  archiveHandle->archiveType             = ARCHIVE_TYPE_NONE;
  archiveHandle->createMeta              = FALSE;
  archiveHandle->dryRun                  = FALSE;

  archiveHandle->archiveInitFunction     = NULL;
  archiveHandle->archiveInitUserData     = NULL;
  archiveHandle->archiveDoneFunction     = NULL;
  archiveHandle->archiveDoneUserData     = NULL;
  archiveHandle->archiveStoreFunction    = NULL;
  archiveHandle->archiveStoreUserData    = NULL;
  archiveHandle->getNamePasswordFunction = fromArchiveHandle->getNamePasswordFunction;
  archiveHandle->getNamePasswordUserData = fromArchiveHandle->getNamePasswordUserData;
  archiveHandle->logHandle               = fromArchiveHandle->logHandle;

  List_init(&archiveHandle->archiveCryptInfoList);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->archiveCryptInfoList,{ List_done(&archiveHandle->archiveCryptInfoList,(ListNodeFreeFunction)freeArchiveCryptInfoNode,NULL); });
  archiveHandle->archiveCryptInfo        = fromArchiveHandle->archiveCryptInfo;

  Semaphore_init(&archiveHandle->passwordLock,SEMAPHORE_TYPE_BINARY);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->passwordLock,{ Semaphore_done(&archiveHandle->passwordLock); });
  archiveHandle->cryptPassword           = NULL;
  archiveHandle->cryptPasswordReadFlag   = FALSE;
  archiveHandle->encryptedKeyData        = NULL;
  archiveHandle->encryptedKeyDataLength  = 0;
  archiveHandle->signatureKeyData        = NULL;
  archiveHandle->signatureKeyDataLength  = 0;

  archiveHandle->mode                    = ARCHIVE_MODE_READ;
  archiveHandle->archiveName             = String_duplicate(fromArchiveHandle->archiveName);
  AUTOFREE_ADD(&autoFreeList,archiveHandle->archiveName,{ String_delete(archiveHandle->archiveName); });
  archiveHandle->printableStorageName    = String_duplicate(fromArchiveHandle->printableStorageName);
  AUTOFREE_ADD(&autoFreeList,archiveHandle->printableStorageName,{ String_delete(archiveHandle->printableStorageName); });
  archiveHandle->read.storageFileName    = NULL;
  Semaphore_init(&archiveHandle->lock,SEMAPHORE_TYPE_BINARY);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->lock,{ Semaphore_done(&archiveHandle->lock); });
  archiveHandle->chunkIO                 = &CHUNK_IO_STORAGE;
  archiveHandle->chunkIOUserData         = &archiveHandle->read.storageHandle;

  Semaphore_init(&archiveHandle->indexLock,SEMAPHORE_TYPE_BINARY);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->indexLock,{ Semaphore_done(&archiveHandle->indexLock); });
  archiveHandle->storageId               = DATABASE_ID_NONE;
  List_init(&archiveHandle->archiveIndexList);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->archiveIndexList,{ List_done(&archiveHandle->archiveIndexList,(ListNodeFreeFunction)CALLBACK(freeArchiveIndexNode,NULL)); });
  Semaphore_init(&archiveHandle->archiveIndexList.lock,SEMAPHORE_TYPE_BINARY);
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->archiveIndexList.lock,{ Semaphore_done(&archiveHandle->archiveIndexList.lock); });

  archiveHandle->entries                 = 0LL;
  archiveHandle->archiveFileSize         = 0LL;
  archiveHandle->partNumber              = 0;

  archiveHandle->pendingError            = ERROR_NONE;
  archiveHandle->nextChunkHeaderReadFlag = FALSE;

  archiveHandle->interrupt.openFlag      = FALSE;
  archiveHandle->interrupt.offset        = 0LL;

  // open index
  archiveHandle->indexHandle = Index_open((archiveHandle->storageInfo->type == STORAGE_TYPE_MASTER) ? archiveHandle->storageInfo->master.io : NULL,
                                          INDEX_TIMEOUT
                                         );
  if (archiveHandle->indexHandle != NULL)
  {
    AUTOFREE_ADD(&autoFreeList,archiveHandle->indexHandle,{ Index_close(archiveHandle->indexHandle); });
  }

  // open storage
  error = Storage_open(&archiveHandle->read.storageHandle,
                       archiveHandle->storageInfo,
                       archiveHandle->archiveName
                      );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveHandle->read.storageHandle,{ Storage_close(&archiveHandle->read.storageHandle); });
  DEBUG_TESTCODE() { AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  // free resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveHandle,ArchiveHandle);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveHandle,ArchiveHandle);
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
  Errors  error;
  IndexId storageId;
  String  intermediateFileName;
  int     partNumber;
  uint64  archiveSize;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);

  // flush index list
  error = flushArchiveIndexList(archiveHandle,0);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // close file/storage, store archive file (if created)
  intermediateFileName = String_new();
  SEMAPHORE_LOCKED_DO(&archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    switch (archiveHandle->mode)
    {
      case ARCHIVE_MODE_CREATE:
        error = closeArchiveFile(archiveHandle,&storageId,intermediateFileName,&partNumber,&archiveSize);
        break;
      case ARCHIVE_MODE_READ:
        Storage_close(&archiveHandle->read.storageHandle);
        error = ERROR_NONE;
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
  }
  if (error != ERROR_NONE)
  {
    String_delete(intermediateFileName);
    return error;
  }
  if (   (archiveHandle->mode == ARCHIVE_MODE_CREATE)
      && !String_isEmpty(intermediateFileName)
     )
  {
    error = storeArchiveFile(archiveHandle,storageId,intermediateFileName,partNumber,archiveSize);
    if (error != ERROR_NONE)
    {
      String_delete(intermediateFileName);
      return error;
    }
  }
  String_delete(intermediateFileName);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(archiveHandle,ArchiveHandle);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveHandle,ArchiveHandle);
  #endif /* NDEBUG */

  if (archiveHandle->indexHandle != NULL)
  {
    // clear index busy handler, close index
    Index_setBusyHandler(archiveHandle->indexHandle,CALLBACK(NULL,NULL));
    if (archiveHandle->indexHandle != NULL)
    {
      Index_close(archiveHandle->indexHandle);
   }
 }

  // free resources
  Semaphore_done(&archiveHandle->archiveIndexList.lock);
  List_done(&archiveHandle->archiveIndexList,(ListNodeFreeFunction)CALLBACK(freeArchiveIndexNode,NULL));
  Semaphore_done(&archiveHandle->indexLock);
  switch (archiveHandle->mode)
  {
    case ARCHIVE_MODE_CREATE:
      String_delete(archiveHandle->create.tmpFileName);
      break;
    case ARCHIVE_MODE_READ:
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  String_delete(archiveHandle->printableStorageName);
  String_delete(archiveHandle->archiveName);
  Semaphore_done(&archiveHandle->lock);
  if (archiveHandle->encryptedKeyData != NULL) free(archiveHandle->encryptedKeyData);
  if (archiveHandle->cryptPassword != NULL) Password_delete(archiveHandle->cryptPassword);
  Semaphore_done(&archiveHandle->passwordLock);
  List_done(&archiveHandle->archiveCryptInfoList,(ListNodeFreeFunction)freeArchiveCryptInfoNode,NULL);
  if (archiveHandle->scheduleUUID != NULL) String_delete(archiveHandle->scheduleUUID);
  if (archiveHandle->jobUUID != NULL) String_delete(archiveHandle->jobUUID);
  String_delete(archiveHandle->hostName);

  return error;
}

const ArchiveCryptInfo *Archive_getCryptInfo(const ArchiveHandle *archiveHandle)
{
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);

  return archiveHandle->archiveCryptInfo;
}

#if 0
Errors Archive_storageInterrupt(ArchiveHandle *archiveHandle)
{
  Errors error;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);

  switch (archiveHandle->mode)
  {
    case ARCHIVE_MODE_CREATE:
      SEMAPHORE_LOCKED_DO(&archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        archiveHandle->interrupt.openFlag = archiveHandle->file.openFlag;
        if (archiveHandle->file.openFlag)
        {
          error = File_tell(&archiveHandle->file.tmpFileHandle,&archiveHandle->interrupt.offset);
          if (error != ERROR_NONE)
          {
            return error;
          }
          File_close(&archiveHandle->file.tmpFileHandle);
          archiveHandle->file.openFlag = FALSE;
        }
      }
      break;
    case ARCHIVE_MODE_READ:
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
  Errors error;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);

  switch (archiveHandle->mode)
  {
    case ARCHIVE_MODE_CREATE:
      SEMAPHORE_LOCKED_DO(&archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        if (archiveHandle->interrupt.openFlag)
        {
          error = File_open(&archiveHandle->file.tmpFileHandle,archiveHandle->file.tmpFileName,FILE_OPEN_WRITE);
          if (error != ERROR_NONE)
          {
            return error;
          }
#ifndef WERROR
#warning seek?
#endif
          error = File_seek(&archiveHandle->file.tmpFileHandle,archiveHandle->interrupt.offset);
          if (error != ERROR_NONE)
          {
            return error;
          }
        }
        archiveHandle->file.openFlag = archiveHandle->interrupt.openFlag;
      }
      break;
    case ARCHIVE_MODE_READ:
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
#endif

bool Archive_eof(ArchiveHandle *archiveHandle,
                 ArchiveFlags  flags
                )
{
  bool           chunkHeaderFoundFlag;
  bool           scanFlag;
  ChunkHeader    chunkHeader;
  CryptKey       privateCryptKey;
  bool           decryptedFlag;
  PasswordHandle passwordHandle;
  const Password *password;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->chunkIO != NULL);
  assert(archiveHandle->chunkIO->seek != NULL);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    return FALSE;
  }

  // find next file/image/directory/link/hard link/special/meta/signature chunk
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

    // find next file/image/directory/link/hardlink/special/meta/signature chunk
    scanFlag = FALSE;
    switch (chunkHeader.id)
    {
      case CHUNK_ID_BAR:
        // read BAR header
        archiveHandle->pendingError = readHeader(archiveHandle,&chunkHeader);
        if (archiveHandle->pendingError != ERROR_NONE)
        {
          return FALSE;
        }
        break;
      case CHUNK_ID_SALT:
        // read salt
        archiveHandle->pendingError = readSalt(archiveHandle,&chunkHeader);
        if (archiveHandle->pendingError != ERROR_NONE)
        {
          return FALSE;
        }
        break;
      case CHUNK_ID_KEY:
        {
          const Key *cryptPrivateKey;

          // check if private key available
          if      (isKeyAvailable(&archiveHandle->storageInfo->jobOptions->cryptPrivateKey))
          {
            cryptPrivateKey = &archiveHandle->storageInfo->jobOptions->cryptPrivateKey;
          }
          else if (isKeyAvailable(&globalOptions.cryptPrivateKey))
          {
            cryptPrivateKey = &globalOptions.cryptPrivateKey;
          }
          else
          {
            archiveHandle->pendingError = ERROR_NO_PRIVATE_CRYPT_KEY;
            return FALSE;
          }
          assert(isKeyAvailable(cryptPrivateKey));
          assert(archiveHandle->archiveCryptInfo != NULL);

//fprintf(stderr,"%s, %d: private key1 \n",__FILE__,__LINE__); debugDumpMemory(cryptPrivateKey->data,cryptPrivateKey->length,0);
          // init private key: try with no password/salt, then all passwords
          Crypt_initKey(&privateCryptKey,CRYPT_PADDING_TYPE_NONE);
          decryptedFlag = FALSE;
//fprintf(stderr,"%s, %d: %p: %p %d\n",__FILE__,__LINE__,cryptPrivateKey,cryptPrivateKey->data,cryptPrivateKey->length);
//debugDumpMemory(cryptPrivateKey->data,cryptPrivateKey->length,0);
          archiveHandle->pendingError = Crypt_setPublicPrivateKeyData(&privateCryptKey,
                                                                      cryptPrivateKey->data,
                                                                      cryptPrivateKey->length,
                                                                      CRYPT_MODE_CBC|CRYPT_MODE_CTS,
//CRYPT_KEY_DERIVE_NONE,//                                                                      archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
CRYPT_KEY_DERIVE_FUNCTION,//
                                                                      NULL,  // salt
                                                                      NULL  // password
                                                                     );
          if (archiveHandle->pendingError == ERROR_NONE)
          {
            decryptedFlag = TRUE;
          }
          else
          {
            password = getFirstDecryptPassword(&passwordHandle,
                                               archiveHandle,
                                               archiveHandle->storageInfo->jobOptions,
                                               archiveHandle->storageInfo->jobOptions->cryptPasswordMode,
                                               CALLBACK(archiveHandle->getNamePasswordFunction,archiveHandle->getNamePasswordUserData)
                                              );
            while (   !decryptedFlag
                   && (password != NULL)
                  )
            {
              archiveHandle->pendingError = Crypt_setPublicPrivateKeyData(&privateCryptKey,
                                                                          cryptPrivateKey->data,
                                                                          cryptPrivateKey->length,
                                                                          CRYPT_MODE_CBC|CRYPT_MODE_CTS,
//archiveHandle->archiveCryptInfo->cryptMode,
//                                                                          archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
//CRYPT_KEY_DERIVE_NONE,
CRYPT_KEY_DERIVE_FUNCTION,//
                                                                          NULL,  // salt
                                                                          password
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
          }
          if (!decryptedFlag)
          {
            archiveHandle->pendingError = ERROR_KEY_DECRYPT_FAIL;
            Crypt_doneKey(&privateCryptKey);
            return FALSE;
          }

          // read encryption key for asymmetric encrypted data
          archiveHandle->pendingError = readEncryptionKey(archiveHandle,&chunkHeader,&privateCryptKey);
          if (archiveHandle->pendingError != ERROR_NONE)
          {
            Crypt_doneKey(&privateCryptKey);
            return FALSE;
          }
// Password_dump(archiveHandle->cryptPassword);
          // free resources
          Crypt_doneKey(&privateCryptKey);
        }
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
        break;
      default:
        if (IS_SET(flags,ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS))
        {
          // unknown chunk -> switch to scan mode
          if (!scanFlag)
          {
            if (IS_SET(flags,ARCHIVE_FLAG_PRINT_UNKNOWN_CHUNKS))
            {
              printWarning("Skipped unknown chunk '%s' at offset %llu in '%s'\n",
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
          archiveHandle->pendingError = ERRORX_(UNKNOWN_CHUNK,
                                                0,
                                                "'%s' at offset %llu",
                                                Chunk_idToString(chunkHeader.id),
                                                chunkHeader.offset
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
                              CompressAlgorithms              deltaCompressAlgorithm,
                              CompressAlgorithms              byteCompressAlgorithm,
                              ConstString                     fileName,
                              const FileInfo                  *fileInfo,
                              const FileExtendedAttributeList *fileExtendedAttributeList,
                              uint64                          fragmentOffset,
                              uint64                          fragmentSize,
                              ArchiveFlags                    archiveFlags
                             )
#else /* not NDEBUG */
  Errors __Archive_newFileEntry(const char                      *__fileName__,
                                ulong                           __lineNb__,
                                ArchiveEntryInfo                *archiveEntryInfo,
                                ArchiveHandle                   *archiveHandle,
                                CompressAlgorithms              deltaCompressAlgorithm,
                                CompressAlgorithms              byteCompressAlgorithm,
                                ConstString                     fileName,
                                const FileInfo                  *fileInfo,
                                const FileExtendedAttributeList *fileExtendedAttributeList,
                                uint64                          fragmentOffset,
                                uint64                          fragmentSize,
                                ArchiveFlags                    archiveFlags
                               )
#endif /* NDEBUG */
{
  AutoFreeList                    autoFreeList;
  Errors                          error;
  const FileExtendedAttributeNode *fileExtendedAttributeNode;

  assert(archiveEntryInfo != NULL);
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(archiveHandle->blockLength > 0);
  assert(archiveHandle->mode == ARCHIVE_MODE_CREATE);
  assert(fileInfo != NULL);

  UNUSED_VARIABLE(fragmentSize);

  // init variables
  AutoFree_init(&autoFreeList);

  // init archive entry info
  archiveEntryInfo->archiveHandle                  = archiveHandle;

#ifdef MULTI_CRYPT
fprintf(stderr,"%s, %d: %d %d %d %d\n",__FILE__,__LINE__,
archiveHandle->jobOptions->cryptAlgorithms[0],
archiveHandle->jobOptions->cryptAlgorithms[1],
archiveHandle->jobOptions->cryptAlgorithms[2],
archiveHandle->jobOptions->cryptAlgorithms[3]
);
#else
#endif
  memCopyFast(&archiveEntryInfo->cryptAlgorithms,sizeof(archiveEntryInfo->cryptAlgorithms),&archiveHandle->storageInfo->jobOptions->cryptAlgorithms,sizeof(archiveHandle->storageInfo->jobOptions->cryptAlgorithms));
  archiveEntryInfo->blockLength                    = archiveHandle->blockLength;

  archiveEntryInfo->archiveEntryType               = ARCHIVE_ENTRY_TYPE_FILE;

  archiveEntryInfo->file.fileExtendedAttributeList = fileExtendedAttributeList;

  archiveEntryInfo->file.deltaCompressAlgorithm    = (archiveFlags & ARCHIVE_FLAG_TRY_DELTA_COMPRESS) ? deltaCompressAlgorithm : COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->file.byteCompressAlgorithm     = (archiveFlags & ARCHIVE_FLAG_TRY_BYTE_COMPRESS ) ? byteCompressAlgorithm  : COMPRESS_ALGORITHM_NONE;

  archiveEntryInfo->file.deltaSourceHandleInitFlag = FALSE;

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
  if ((archiveFlags & ARCHIVE_FLAG_TRY_DELTA_COMPRESS) && Compress_isCompressed(deltaCompressAlgorithm))
  {
    error = DeltaSource_openEntry(&archiveEntryInfo->file.deltaSourceHandle,
                                  archiveHandle->deltaSourceList,
                                  NULL, // storageName
                                  fileName,
                                  SOURCE_SIZE_UNKNOWN,
                                  archiveHandle->storageInfo->jobOptions
                                 );
    if      (error == ERROR_NONE)
    {
      // delta compress
      archiveEntryInfo->file.deltaSourceHandleInitFlag = TRUE;
      AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.deltaSourceHandle,{ DeltaSource_closeEntry(&archiveEntryInfo->file.deltaSourceHandle); });
    }
    else if (archiveHandle->storageInfo->jobOptions->forceDeltaCompressionFlag)
    {
      // no delta source -> error
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    else
    {
      // no delta source -> print warning and disable delta compress
      printWarning("File '%s' not delta compressed (no source file found)\n",String_cString(fileName));
      logMessage(archiveHandle->logHandle,
                 LOG_TYPE_WARNING,
                 "File '%s' not delta compressed (no source file found)\n",
                 String_cString(fileName)
                );
      archiveEntryInfo->file.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
    }
  }

  // init file chunk
  error = Chunk_init(&archiveEntryInfo->file.chunkFile.info,
                     NULL,  // parentChunkInfo
                     &CHUNK_IO_FILE,
                     &archiveEntryInfo->file.intermediateFileHandle,
                     CHUNK_ID_FILE,
                     CHUNK_DEFINITION_FILE,
//TODO: DEFAULT_ALIGNMENT
                     DEFAULT_ALIGNMENT,
                     NULL,  // cryptInfo
                     &archiveEntryInfo->file.chunkFile
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Chunk_done(&archiveEntryInfo->file.chunkFile.info); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  archiveEntryInfo->file.chunkFile.compressAlgorithm  = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->file.byteCompressAlgorithm);
#ifdef MULTI_CRYPT
  archiveEntryInfo->file.chunkFile.cryptAlgorithms[0] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[0]);
  archiveEntryInfo->file.chunkFile.cryptAlgorithms[1] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[1]);
  archiveEntryInfo->file.chunkFile.cryptAlgorithms[2] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[2]);
  archiveEntryInfo->file.chunkFile.cryptAlgorithms[3] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[3]);
#else
  archiveEntryInfo->file.chunkFile.cryptAlgorithm = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0]);
#endif
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFile.info,{ Chunk_done(&archiveEntryInfo->file.chunkFile.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->file.chunkFileEntry.cryptInfo,
//TODO MULTI_CRYPT
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo,
//TODO MULTI_CRYPT
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->file.chunkFileDelta.cryptInfo,
//TODO MULTI_CRYPT
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileDelta.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->file.chunkFileData.cryptInfo,
//TODO MULTI_CRYPT
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileData.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->file.cryptInfo,
//TODO MULTI_CRYPT
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
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
    assert(Compress_isDeltaCompressed(archiveEntryInfo->file.deltaCompressAlgorithm));

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
  archiveEntryInfo->file.chunkFileData.fragmentOffset = fragmentOffset;
  archiveEntryInfo->file.chunkFileData.fragmentSize   = 0LL;
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.chunkFileData.info,{ Chunk_done(&archiveEntryInfo->file.chunkFileData.info); });

  // init delta compress (if no delta-compression is enabled, use identity-compressor), byte compress
  error = Compress_init(&archiveEntryInfo->file.deltaCompressInfo,
                        COMPRESS_MODE_DEFLATE,
                        archiveEntryInfo->file.deltaCompressAlgorithm,
                        1,  // blockLength
                        &archiveEntryInfo->file.deltaSourceHandle
                       );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Compress_done(&archiveEntryInfo->file.deltaCompressInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->file.deltaCompressInfo,{ Compress_done(&archiveEntryInfo->file.deltaCompressInfo); });

  // init byte compress (if no byte-compression is enabled, use identity-compressor)
  error = Compress_init(&archiveEntryInfo->file.byteCompressInfo,
                        COMPRESS_MODE_DEFLATE,
                        archiveEntryInfo->file.byteCompressAlgorithm,
                        archiveEntryInfo->blockLength,
                        NULL  // deltaSourceHandle
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
    assert(Compress_isDeltaCompressed(archiveEntryInfo->file.deltaCompressAlgorithm));

    archiveEntryInfo->file.chunkFileDelta.deltaAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->file.deltaCompressAlgorithm);
    String_set(archiveEntryInfo->file.chunkFileDelta.name,DeltaSource_getName(&archiveEntryInfo->file.deltaSourceHandle));
    archiveEntryInfo->file.chunkFileDelta.size           = DeltaSource_getSize(&archiveEntryInfo->file.deltaSourceHandle);

    archiveEntryInfo->file.headerLength += Chunk_getSize(&archiveEntryInfo->file.chunkFileDelta.info,
                                                         &archiveEntryInfo->file.chunkFileDelta,
                                                         0
                                                        );
  }

  // find next suitable archive part
  findNextArchivePart(archiveHandle);

  if (!archiveHandle->dryRun)
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
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,ArchiveEntryInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,ArchiveEntryInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_newImageEntry(ArchiveEntryInfo   *archiveEntryInfo,
                               ArchiveHandle      *archiveHandle,
                               CompressAlgorithms deltaCompressAlgorithm,
                               CompressAlgorithms byteCompressAlgorithm,
                               ConstString        deviceName,
                               const DeviceInfo   *deviceInfo,
                               FileSystemTypes    fileSystemType,
                               uint64             blockOffset,
                               uint64             blockCount,
                               ArchiveFlags       archiveFlags
                              )
#else /* not NDEBUG */
  Errors __Archive_newImageEntry(const char         *__fileName__,
                                 ulong              __lineNb__,
                                 ArchiveEntryInfo   *archiveEntryInfo,
                                 ArchiveHandle      *archiveHandle,
                                 CompressAlgorithms deltaCompressAlgorithm,
                                 CompressAlgorithms byteCompressAlgorithm,
                                 ConstString        deviceName,
                                 const DeviceInfo   *deviceInfo,
                                 FileSystemTypes    fileSystemType,
                                 uint64             blockOffset,
                                 uint64             blockCount,
                                 ArchiveFlags       archiveFlags
                                )
#endif /* NDEBUG */
{
  AutoFreeList  autoFreeList;
  Errors        error;

  assert(archiveEntryInfo != NULL);
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(archiveHandle->blockLength > 0);
  assert(archiveHandle->mode == ARCHIVE_MODE_CREATE);
  assert(deviceInfo != NULL);
  assert(deviceInfo->blockSize > 0);

  UNUSED_VARIABLE(blockCount);

  // init variables
  AutoFree_init(&autoFreeList);

  // init archive entry info
  archiveEntryInfo->archiveHandle                   = archiveHandle;

  memCopyFast(&archiveEntryInfo->cryptAlgorithms,sizeof(archiveEntryInfo->cryptAlgorithms),&archiveHandle->storageInfo->jobOptions->cryptAlgorithms,sizeof(archiveHandle->storageInfo->jobOptions->cryptAlgorithms));
  archiveEntryInfo->blockLength                     = archiveHandle->blockLength;

  archiveEntryInfo->archiveEntryType                = ARCHIVE_ENTRY_TYPE_IMAGE;

  archiveEntryInfo->image.deltaSourceHandleInitFlag = FALSE;

  archiveEntryInfo->image.blockSize                 = deviceInfo->blockSize;

  archiveEntryInfo->image.deltaCompressAlgorithm    = (archiveFlags & ARCHIVE_FLAG_TRY_DELTA_COMPRESS) ? deltaCompressAlgorithm : COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->image.byteCompressAlgorithm     = (archiveFlags & ARCHIVE_FLAG_TRY_BYTE_COMPRESS ) ? byteCompressAlgorithm  : COMPRESS_ALGORITHM_NONE;

  archiveEntryInfo->image.headerLength              = 0;
  archiveEntryInfo->image.headerWrittenFlag         = FALSE;

  archiveEntryInfo->image.byteBuffer                = NULL;
  archiveEntryInfo->image.byteBufferSize            = 0L;
  archiveEntryInfo->image.deltaBuffer               = NULL;
  archiveEntryInfo->image.deltaBufferSize           = 0L;

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
  if ((archiveFlags & ARCHIVE_FLAG_TRY_DELTA_COMPRESS) && Compress_isCompressed(deltaCompressAlgorithm))
  {
    error = DeltaSource_openEntry(&archiveEntryInfo->image.deltaSourceHandle,
                                  archiveHandle->deltaSourceList,
                                  NULL, // storageName
                                  deviceName,
                                  SOURCE_SIZE_UNKNOWN,
                                  archiveHandle->storageInfo->jobOptions
                                 );
    if      (error == ERROR_NONE)
    {
      // delta compress
      archiveEntryInfo->image.deltaSourceHandleInitFlag = TRUE;
      AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.deltaSourceHandle,{ DeltaSource_closeEntry(&archiveEntryInfo->image.deltaSourceHandle); });
    }
    else if (archiveHandle->storageInfo->jobOptions->forceDeltaCompressionFlag)
    {
      // no delta source -> error
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    else
    {
      // no delta source -> print warning and disable delta compress
      printWarning("Image of devicee '%s' not delta compressed (no source file found)\n",String_cString(deviceName));
      logMessage(archiveHandle->logHandle,
                 LOG_TYPE_WARNING,
                 "Image of device '%s' not delta compressed (no source file found)\n",
                 String_cString(deviceName)
                );
      archiveEntryInfo->image.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
    }
  }

  // init image chunk
  error = Chunk_init(&archiveEntryInfo->image.chunkImage.info,
                     NULL,  // parentChunkInfo
                     &CHUNK_IO_FILE,
                     &archiveEntryInfo->image.intermediateFileHandle,
                     CHUNK_ID_IMAGE,
                     CHUNK_DEFINITION_IMAGE,
//TODO: DEFAULT_ALIGNMENT
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
  archiveEntryInfo->image.chunkImage.compressAlgorithm  = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->image.byteCompressAlgorithm);
#ifdef MULTI_CRYPT
  archiveEntryInfo->image.chunkImage.cryptAlgorithms[0] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[0]);
  archiveEntryInfo->image.chunkImage.cryptAlgorithms[1] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[1]);
  archiveEntryInfo->image.chunkImage.cryptAlgorithms[2] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[2]);
  archiveEntryInfo->image.chunkImage.cryptAlgorithms[3] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[3]);
#else
  archiveEntryInfo->image.chunkImage.cryptAlgorithm = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0]);
#endif
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImage.info,{ Chunk_done(&archiveEntryInfo->image.chunkImage.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->image.chunkImageEntry.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImageEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->image.chunkImageDelta.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImageDelta.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->image.chunkImageData.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImageData.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->image.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
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

  if (Compress_isCompressed(archiveEntryInfo->image.deltaCompressAlgorithm))
  {
    assert(Compress_isDeltaCompressed(archiveEntryInfo->image.deltaCompressAlgorithm));
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
  archiveEntryInfo->image.chunkImageData.blockOffset = blockOffset;
  archiveEntryInfo->image.chunkImageData.blockCount  = 0LL;
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.chunkImageData.info,{ Chunk_done(&archiveEntryInfo->image.chunkImageData.info); });

  // init delta compress (if no delta-compression is enabled, use identity-compressor), byte compress
  error = Compress_init(&archiveEntryInfo->image.deltaCompressInfo,
                        COMPRESS_MODE_DEFLATE,
                        archiveEntryInfo->image.deltaCompressAlgorithm,
                        1,  // blockLength
                        &archiveEntryInfo->image.deltaSourceHandle
                       );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->image.deltaCompressInfo,{ Compress_done(&archiveEntryInfo->image.deltaCompressInfo); });

  // init byte compress (if no byte-compression is enabled, use identity-compressor)
  error = Compress_init(&archiveEntryInfo->image.byteCompressInfo,
                        COMPRESS_MODE_DEFLATE,
                        archiveEntryInfo->image.byteCompressAlgorithm,
                        archiveEntryInfo->blockLength,
                        NULL  // deltaSourceHandle
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
    assert(Compress_isDeltaCompressed(archiveEntryInfo->image.deltaCompressAlgorithm));

    archiveEntryInfo->image.chunkImageDelta.deltaAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->image.deltaCompressAlgorithm);
    String_set(archiveEntryInfo->image.chunkImageDelta.name,DeltaSource_getName(&archiveEntryInfo->image.deltaSourceHandle));
    archiveEntryInfo->image.chunkImageDelta.size           = DeltaSource_getSize(&archiveEntryInfo->image.deltaSourceHandle);

    archiveEntryInfo->image.headerLength += Chunk_getSize(&archiveEntryInfo->image.chunkImageDelta.info,
                                                          &archiveEntryInfo->image.chunkImageDelta,
                                                          0
                                                         );
  }

  // find next suitable archive part
  findNextArchivePart(archiveHandle);

  if (!archiveHandle->dryRun)
  {
    // write header
    error = writeImageChunks(archiveEntryInfo);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
  }

  // done resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,ArchiveEntryInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,ArchiveEntryInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_newDirectoryEntry(ArchiveEntryInfo                *archiveEntryInfo,
                                   ArchiveHandle                   *archiveHandle,
                                   ConstString                     directoryName,
                                   const FileInfo                  *fileInfo,
                                   const FileExtendedAttributeList *fileExtendedAttributeList
                                  )
#else /* not NDEBUG */
  Errors __Archive_newDirectoryEntry(const char                      *__fileName__,
                                     ulong                           __lineNb__,
                                     ArchiveEntryInfo                *archiveEntryInfo,
                                     ArchiveHandle                   *archiveHandle,
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
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(archiveHandle->blockLength > 0);
  assert(archiveHandle->mode == ARCHIVE_MODE_CREATE);
  assert(fileInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  // init archive entry info
  archiveEntryInfo->archiveHandle    = archiveHandle;

  memCopyFast(&archiveEntryInfo->cryptAlgorithms,sizeof(archiveEntryInfo->cryptAlgorithms),&archiveHandle->storageInfo->jobOptions->cryptAlgorithms,sizeof(archiveHandle->storageInfo->jobOptions->cryptAlgorithms));
  archiveEntryInfo->blockLength      = archiveHandle->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_DIRECTORY;

  // init directory entry chunk
  error = Chunk_init(&archiveEntryInfo->directory.chunkDirectory.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_DIRECTORY,
                     CHUNK_DEFINITION_DIRECTORY,
//TODO: DEFAULT_ALIGNMENT
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->directory.chunkDirectory
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
#ifdef MULTI_CRYPT
  archiveEntryInfo->directory.chunkDirectory.cryptAlgorithms[0] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[0]);
  archiveEntryInfo->directory.chunkDirectory.cryptAlgorithms[1] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[1]);
  archiveEntryInfo->directory.chunkDirectory.cryptAlgorithms[2] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[2]);
  archiveEntryInfo->directory.chunkDirectory.cryptAlgorithms[3] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[3]);
#else
  archiveEntryInfo->directory.chunkDirectory.cryptAlgorithm = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0]);
#endif
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->directory.chunkDirectory.info,{ Chunk_done(&archiveEntryInfo->directory.chunkDirectory.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
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
  findNextArchivePart(archiveHandle);

  if (!archiveHandle->dryRun)
  {
    // lock archive
    Semaphore_forceLock(&archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);
//TODO: required?
assert(Semaphore_isOwned(&archiveHandle->lock));

    // ensure space in archive
    error = ensureArchiveSpace(archiveHandle,
                               headerLength
                              );
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveHandle->lock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write directory chunk
    error = Chunk_create(&archiveEntryInfo->directory.chunkDirectory.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveHandle->lock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write directory entry chunk
    error = Chunk_create(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveHandle->lock);
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
      Semaphore_unlock(&archiveHandle->lock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
  }

  // done resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,ArchiveEntryInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,ArchiveEntryInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_newLinkEntry(ArchiveEntryInfo                *archiveEntryInfo,
                              ArchiveHandle                   *archiveHandle,
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
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(archiveHandle->blockLength > 0);
  assert(archiveHandle->mode == ARCHIVE_MODE_CREATE);
  assert(fileInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  // init archive entry info
  archiveEntryInfo->archiveHandle    = archiveHandle;

  memCopyFast(&archiveEntryInfo->cryptAlgorithms,sizeof(archiveEntryInfo->cryptAlgorithms),&archiveHandle->storageInfo->jobOptions->cryptAlgorithms,sizeof(archiveHandle->storageInfo->jobOptions->cryptAlgorithms));
  archiveEntryInfo->blockLength      = archiveHandle->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_LINK;

  // init link chunk
  error = Chunk_init(&archiveEntryInfo->link.chunkLink.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_LINK,
                     CHUNK_DEFINITION_LINK,
//TODO: DEFAULT_ALIGNMENT
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->link.chunkLink
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
#ifdef MULTI_CRYPT
  archiveEntryInfo->link.chunkLink.cryptAlgorithms[0] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[0]);
  archiveEntryInfo->link.chunkLink.cryptAlgorithms[1] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[1]);
  archiveEntryInfo->link.chunkLink.cryptAlgorithms[2] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[2]);
  archiveEntryInfo->link.chunkLink.cryptAlgorithms[3] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[3]);
#else
  archiveEntryInfo->link.chunkLink.cryptAlgorithm = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0]);
#endif
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->link.chunkLink.info,{ Chunk_done(&archiveEntryInfo->link.chunkLink.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->link.chunkLinkEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->link.chunkLinkExtendedAttribute.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
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


  if (!archiveHandle->dryRun)
  {
    // lock archive
    Semaphore_forceLock(&archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

    // ensure space in archive
    error = ensureArchiveSpace(archiveHandle,
                               headerLength
                              );
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveHandle->lock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write link chunk
    error = Chunk_create(&archiveEntryInfo->link.chunkLink.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveHandle->lock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write link entry chunk
    error = Chunk_create(&archiveEntryInfo->link.chunkLinkEntry.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveHandle->lock);
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
      Semaphore_unlock(&archiveHandle->lock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
  }

  // done resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,ArchiveEntryInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,ArchiveEntryInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_newHardLinkEntry(ArchiveEntryInfo                *archiveEntryInfo,
                                  ArchiveHandle                   *archiveHandle,
                                  CompressAlgorithms              deltaCompressAlgorithm,
                                  CompressAlgorithms              byteCompressAlgorithm,
                                  const StringList                *fileNameList,
                                  const FileInfo                  *fileInfo,
                                  const FileExtendedAttributeList *fileExtendedAttributeList,
                                  uint64                          fragmentOffset,
                                  uint64                          fragmentSize,
                                  ArchiveFlags                    archiveFlags
                                 )
#else /* not NDEBUG */
  Errors __Archive_newHardLinkEntry(const char                      *__fileName__,
                                    ulong                           __lineNb__,
                                    ArchiveEntryInfo                *archiveEntryInfo,
                                    ArchiveHandle                   *archiveHandle,
                                    CompressAlgorithms              deltaCompressAlgorithm,
                                    CompressAlgorithms              byteCompressAlgorithm,
                                    const StringList                *fileNameList,
                                    const FileInfo                  *fileInfo,
                                    const FileExtendedAttributeList *fileExtendedAttributeList,
                                    uint64                          fragmentOffset,
                                    uint64                          fragmentSize,
                                    ArchiveFlags                    archiveFlags
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
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(archiveHandle->blockLength > 0);
  assert(archiveHandle->mode == ARCHIVE_MODE_CREATE);
  assert(fileInfo != NULL);

  UNUSED_VARIABLE(fragmentSize);

  // init variables
  AutoFree_init(&autoFreeList);

  // init archive entry info
  archiveEntryInfo->archiveHandle                      = archiveHandle;

  memCopyFast(&archiveEntryInfo->cryptAlgorithms,sizeof(archiveEntryInfo->cryptAlgorithms),&archiveHandle->storageInfo->jobOptions->cryptAlgorithms,sizeof(archiveHandle->storageInfo->jobOptions->cryptAlgorithms));
  archiveEntryInfo->blockLength                        = archiveHandle->blockLength;

  archiveEntryInfo->archiveEntryType                   = ARCHIVE_ENTRY_TYPE_HARDLINK;

  archiveEntryInfo->hardLink.fileNameList              = fileNameList;
  archiveEntryInfo->hardLink.fileExtendedAttributeList = fileExtendedAttributeList;

  archiveEntryInfo->hardLink.deltaCompressAlgorithm    = (archiveFlags & ARCHIVE_FLAG_TRY_DELTA_COMPRESS) ? deltaCompressAlgorithm : COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->hardLink.byteCompressAlgorithm     = (archiveFlags & ARCHIVE_FLAG_TRY_BYTE_COMPRESS ) ? byteCompressAlgorithm  : COMPRESS_ALGORITHM_NONE;

  archiveEntryInfo->hardLink.deltaSourceHandleInitFlag = FALSE;

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
  if ((archiveFlags & ARCHIVE_FLAG_TRY_DELTA_COMPRESS) && Compress_isCompressed(deltaCompressAlgorithm))
  {
    error = ERROR_NONE;
    STRINGLIST_ITERATE(fileNameList,stringNode,fileName)
    {
      error = DeltaSource_openEntry(&archiveEntryInfo->hardLink.deltaSourceHandle,
                                    archiveHandle->deltaSourceList,
                                    NULL, // storageName
                                    fileName,
                                    SOURCE_SIZE_UNKNOWN,
                                    archiveHandle->storageInfo->jobOptions
                                   );
      if (error == ERROR_NONE) break;
    }
    if      (error == ERROR_NONE)
    {
      // delta compress
      archiveEntryInfo->hardLink.deltaSourceHandleInitFlag = TRUE;
      AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.deltaSourceHandle,{ DeltaSource_closeEntry(&archiveEntryInfo->hardLink.deltaSourceHandle); });
    }
    else if (archiveHandle->storageInfo->jobOptions->forceDeltaCompressionFlag)
    {
      // no delta source -> error
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    else
    {
      // no delta source -> print warning and disable delta compress
      printWarning("File '%s' not delta compressed (no source file found)\n",String_cString(StringList_first(fileNameList,NULL)));
      logMessage(archiveHandle->logHandle,
                 LOG_TYPE_WARNING,
                 "File '%s' not delta compressed (no source file found)\n",
                 String_cString(StringList_first(fileNameList,NULL))
                );
      archiveEntryInfo->hardLink.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
    }
  }

  // init hard link chunk
  error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLink.info,
                     NULL,  // parentChunkInfo
                     &CHUNK_IO_FILE,
                     &archiveEntryInfo->hardLink.intermediateFileHandle,
                     CHUNK_ID_HARDLINK,
                     CHUNK_DEFINITION_HARDLINK,
//TODO: DEFAULT_ALIGNMENT
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->hardLink.chunkHardLink
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  archiveEntryInfo->hardLink.chunkHardLink.compressAlgorithm  = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->hardLink.byteCompressAlgorithm);
#ifdef MULTI_CRYPT
  archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithms[0] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[0]);
  archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithms[1] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[1]);
  archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithms[2] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[2]);
  archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithms[3] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[3]);
#else
  archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithm = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0]);
#endif
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLink.info,{ Chunk_done(&archiveEntryInfo->hardLink.chunkHardLink.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->hardLink.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
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
    assert(Compress_isDeltaCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm));

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
  archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset = fragmentOffset;
//TODO: multi crypt
  archiveEntryInfo->hardLink.chunkHardLinkData.fragmentSize   = 0LL;
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.chunkHardLinkData.info,{ Chunk_done(&archiveEntryInfo->hardLink.chunkHardLinkData.info); });

  // init delta compress (if no delta-compression is enabled, use identity-compressor), byte compress
  error = Compress_init(&archiveEntryInfo->hardLink.deltaCompressInfo,
                        COMPRESS_MODE_DEFLATE,
                        archiveEntryInfo->hardLink.deltaCompressAlgorithm,
                        1,  // blockLength
                        &archiveEntryInfo->hardLink.deltaSourceHandle
                       );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->hardLink.deltaCompressInfo,{ Compress_done(&archiveEntryInfo->hardLink.deltaCompressInfo); });

  // init byte compress (if no byte-compression is enabled, use identity-compressor)
  error = Compress_init(&archiveEntryInfo->hardLink.byteCompressInfo,
                        COMPRESS_MODE_DEFLATE,
                        archiveEntryInfo->hardLink.byteCompressAlgorithm,
                        archiveEntryInfo->blockLength,
                        NULL  // deltaSourceHandle
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
    assert(Compress_isDeltaCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm));

    archiveEntryInfo->hardLink.chunkHardLinkDelta.deltaAlgorithm = COMPRESS_ALGORITHM_TO_CONSTANT(archiveEntryInfo->hardLink.deltaCompressAlgorithm);
    String_set(archiveEntryInfo->hardLink.chunkHardLinkDelta.name,DeltaSource_getName(&archiveEntryInfo->hardLink.deltaSourceHandle));
    archiveEntryInfo->hardLink.chunkHardLinkDelta.size           = DeltaSource_getSize(&archiveEntryInfo->hardLink.deltaSourceHandle);

    archiveEntryInfo->hardLink.headerLength += Chunk_getSize(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info,
                                                             &archiveEntryInfo->hardLink.chunkHardLinkDelta,
                                                             0
                                                            );
  }

  // find next suitable archive part
  findNextArchivePart(archiveHandle);

  if (!archiveHandle->dryRun)
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
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,ArchiveEntryInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,ArchiveEntryInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_newSpecialEntry(ArchiveEntryInfo                *archiveEntryInfo,
                                 ArchiveHandle                   *archiveHandle,
                                 ConstString                     specialName,
                                 const FileInfo                  *fileInfo,
                                 const FileExtendedAttributeList *fileExtendedAttributeList
                                )
#else /* not NDEBUG */
  Errors __Archive_newSpecialEntry(const char                      *__fileName__,
                                   ulong                           __lineNb__,
                                   ArchiveEntryInfo                *archiveEntryInfo,
                                   ArchiveHandle                   *archiveHandle,
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
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(archiveHandle->blockLength > 0);
  assert(archiveHandle->mode == ARCHIVE_MODE_CREATE);
  assert(fileInfo != NULL);

  // init variables
  AutoFree_init(&autoFreeList);

  // init archive entry info
  archiveEntryInfo->archiveHandle    = archiveHandle;

  memCopyFast(&archiveEntryInfo->cryptAlgorithms,sizeof(archiveEntryInfo->cryptAlgorithms),&archiveHandle->storageInfo->jobOptions->cryptAlgorithms,sizeof(archiveHandle->storageInfo->jobOptions->cryptAlgorithms));
  archiveEntryInfo->blockLength      = archiveHandle->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_SPECIAL;

  // init special chunk
  error = Chunk_init(&archiveEntryInfo->special.chunkSpecial.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_SPECIAL,
                     CHUNK_DEFINITION_SPECIAL,
//TODO: DEFAULT_ALIGNMENT
                     0,  // alignment
                     NULL,  // cryptInfo
                     &archiveEntryInfo->special.chunkSpecial
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
#ifdef MULTI_CRYPT
  archiveEntryInfo->special.chunkSpecial.cryptAlgorithms[0] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0]);
  archiveEntryInfo->special.chunkSpecial.cryptAlgorithms[1] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->storageInfo->jobOptions->cryptAlgorithms[1]);
  archiveEntryInfo->special.chunkSpecial.cryptAlgorithms[2] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->storageInfo->jobOptions->cryptAlgorithms[2]);
  archiveEntryInfo->special.chunkSpecial.cryptAlgorithms[3] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->storageInfo->jobOptions->cryptAlgorithms[3]);
#else
  archiveEntryInfo->special.chunkSpecial.cryptAlgorithm = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0]);
#endif
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->special.chunkSpecial.info,{ Chunk_done(&archiveEntryInfo->special.chunkSpecial.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo); });

  error = Crypt_init(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
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
  findNextArchivePart(archiveHandle);

  if (!archiveHandle->dryRun)
  {
    // lock archive
    Semaphore_forceLock(&archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

    // ensure space in archive
    error = ensureArchiveSpace(archiveHandle,
                               headerLength
                              );
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveHandle->lock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write special chunk
    error = Chunk_create(&archiveEntryInfo->special.chunkSpecial.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveHandle->lock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write special entry chunk
    error = Chunk_create(&archiveEntryInfo->special.chunkSpecialEntry.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveHandle->lock);
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
      Semaphore_unlock(&archiveHandle->lock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
  }

  // done resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,ArchiveEntryInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,ArchiveEntryInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_newMetaEntry(ArchiveEntryInfo *archiveEntryInfo,
                              ArchiveHandle    *archiveHandle,
                              ConstString      userName,
                              ConstString      hostName,
                              ConstString      jobUUID,
                              ConstString      scheduleUUID,
                              ArchiveTypes     archiveType,
                              uint64           createdDateTime,
                              ConstString      comment
                             )
#else /* not NDEBUG */
  Errors __Archive_newMetaEntry(const char       *__fileName__,
                                ulong            __lineNb__,
                                ArchiveEntryInfo *archiveEntryInfo,
                                ArchiveHandle    *archiveHandle,
                                ConstString      userName,
                                ConstString      hostName,
                                ConstString      jobUUID,
                                ConstString      scheduleUUID,
                                ArchiveTypes     archiveType,
                                uint64           createdDateTime,
                                ConstString      comment
                               )
#endif /* NDEBUG */
{
  AutoFreeList autoFreeList;
  Errors       error;
  ulong        headerLength;

  assert(archiveEntryInfo != NULL);
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(archiveHandle->blockLength > 0);
  assert(archiveHandle->mode == ARCHIVE_MODE_CREATE);

  // init variables
  AutoFree_init(&autoFreeList);

  // init archive entry info
  archiveEntryInfo->archiveHandle    = archiveHandle;

  memCopyFast(&archiveEntryInfo->cryptAlgorithms,sizeof(archiveEntryInfo->cryptAlgorithms),&archiveHandle->storageInfo->jobOptions->cryptAlgorithms,sizeof(archiveHandle->storageInfo->jobOptions->cryptAlgorithms));
  archiveEntryInfo->blockLength      = archiveHandle->blockLength;

  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_META;

  // init meta entry chunk
  error = Chunk_init(&archiveEntryInfo->meta.chunkMeta.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_META,
                     CHUNK_DEFINITION_META,
                     DEFAULT_ALIGNMENT,
                     NULL,  // cryptInfo
                     &archiveEntryInfo->meta.chunkMeta
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
#ifdef MULTI_CRYPT
  archiveEntryInfo->meta.chunkMeta.cryptAlgorithms[0] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[0]);
  archiveEntryInfo->meta.chunkMeta.cryptAlgorithms[1] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[1]);
  archiveEntryInfo->meta.chunkMeta.cryptAlgorithms[2] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[2]);
  archiveEntryInfo->meta.chunkMeta.cryptAlgorithms[3] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->jobOptions->cryptAlgorithms[3]);
#else
  archiveEntryInfo->meta.chunkMeta.cryptAlgorithms[0] = CRYPT_ALGORITHM_TO_CONSTANT(archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0]);
  archiveEntryInfo->meta.chunkMeta.cryptAlgorithms[1] = CRYPT_ALGORITHM_NONE;
  archiveEntryInfo->meta.chunkMeta.cryptAlgorithms[2] = CRYPT_ALGORITHM_NONE;
  archiveEntryInfo->meta.chunkMeta.cryptAlgorithms[3] = CRYPT_ALGORITHM_NONE;
#endif
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->meta.chunkMeta.info,{ Chunk_done(&archiveEntryInfo->meta.chunkMeta.info); });

  // init crypt
  error = Crypt_init(&archiveEntryInfo->meta.chunkMetaEntry.cryptInfo,
//TODO: multi crypt
                     archiveHandle->storageInfo->jobOptions->cryptAlgorithms[0],
                     CRYPT_MODE_CBC,
                     &archiveHandle->archiveCryptInfo->cryptSalt,
                     &archiveHandle->archiveCryptInfo->cryptKey
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->meta.chunkMetaEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->meta.chunkMetaEntry.cryptInfo); });

  // init sub-chunks
  error = Chunk_init(&archiveEntryInfo->meta.chunkMetaEntry.info,
                     &archiveEntryInfo->meta.chunkMeta.info,
                     CHUNK_USE_PARENT,
                     CHUNK_USE_PARENT,
                     CHUNK_ID_META_ENTRY,
                     CHUNK_DEFINITION_META_ENTRY,
                     MAX(archiveEntryInfo->blockLength,DEFAULT_ALIGNMENT),
                     &archiveEntryInfo->meta.chunkMetaEntry.cryptInfo,
                     &archiveEntryInfo->meta.chunkMetaEntry
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  String_set(archiveEntryInfo->meta.chunkMetaEntry.userName,userName);
  String_set(archiveEntryInfo->meta.chunkMetaEntry.hostName,hostName);
  String_set(archiveEntryInfo->meta.chunkMetaEntry.jobUUID,jobUUID);
  String_set(archiveEntryInfo->meta.chunkMetaEntry.scheduleUUID,scheduleUUID);
  archiveEntryInfo->meta.chunkMetaEntry.archiveType     = archiveType;
  archiveEntryInfo->meta.chunkMetaEntry.createdDateTime = createdDateTime;
  String_set(archiveEntryInfo->meta.chunkMetaEntry.comment,comment);
  AUTOFREE_ADD(&autoFreeList,&archiveEntryInfo->meta.chunkMetaEntry.info,{ Chunk_done(&archiveEntryInfo->meta.chunkMetaEntry.info); });

  // calculate header size
  headerLength = Chunk_getSize(&archiveEntryInfo->meta.chunkMeta.info,     &archiveEntryInfo->meta.chunkMeta,     0)+
                 Chunk_getSize(&archiveEntryInfo->meta.chunkMetaEntry.info,&archiveEntryInfo->meta.chunkMetaEntry,0);

  // find next suitable archive part
  findNextArchivePart(archiveHandle);

  if (!archiveHandle->dryRun)
  {
    // lock archive
    Semaphore_forceLock(&archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

    // ensure space in archive
    error = ensureArchiveSpace(archiveHandle,
                               headerLength
                              );
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveHandle->lock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write meta chunk
    error = Chunk_create(&archiveEntryInfo->meta.chunkMeta.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveHandle->lock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // write meta entry chunk
    error = Chunk_create(&archiveEntryInfo->meta.chunkMetaEntry.info);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&archiveHandle->lock);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
  }

  // done resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,ArchiveEntryInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,ArchiveEntryInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

Errors Archive_getNextArchiveEntry(ArchiveHandle          *archiveHandle,
                                   ArchiveEntryTypes      *archiveEntryType,
                                   const ArchiveCryptInfo **archiveCryptInfo,
                                   uint64                 *offset,
                                   ArchiveFlags           flags
                                  )
{
  Errors         error;
  bool           scanMode;
  ChunkHeader    chunkHeader;
  CryptKey       privateCryptKey;
  bool           decryptedFlag;
  PasswordHandle passwordHandle;
  const Password *password;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // find next file/image/directory/link/hard link/special/meta/signature chunk
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
        // BAR header
        error = readHeader(archiveHandle,&chunkHeader);
        if (error != ERROR_NONE)
        {
          return error;
        }

        scanMode = FALSE;
        break;
      case CHUNK_ID_SALT:
        // read salt
        archiveHandle->pendingError = readSalt(archiveHandle,&chunkHeader);
        if (archiveHandle->pendingError != ERROR_NONE)
        {
          return FALSE;
        }
        break;
      case CHUNK_ID_KEY:
        {
          const Key *cryptPrivateKey;

          // check if private key available
          if      (isKeyAvailable(&archiveHandle->storageInfo->jobOptions->cryptPrivateKey))
          {
            cryptPrivateKey = &archiveHandle->storageInfo->jobOptions->cryptPrivateKey;
          }
          else if (isKeyAvailable(&globalOptions.cryptPrivateKey))
          {
            cryptPrivateKey = &globalOptions.cryptPrivateKey;
          }
          else
          {
            return ERROR_NO_PRIVATE_CRYPT_KEY;
          }
          assert(isKeyAvailable(cryptPrivateKey));

          // init private key: try with no password/salt, then all passwords
          Crypt_initKey(&privateCryptKey,CRYPT_PADDING_TYPE_NONE);
          decryptedFlag = FALSE;
          error = Crypt_setPublicPrivateKeyData(&privateCryptKey,
                                                cryptPrivateKey->data,
                                                cryptPrivateKey->length,
                                                CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                                                archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                                NULL,  // cryptSalt
                                                NULL  // password
                                               );
          if (error == ERROR_NONE)
          {
            decryptedFlag = TRUE;
          }
          password = getFirstDecryptPassword(&passwordHandle,
                                             archiveHandle,
                                             archiveHandle->storageInfo->jobOptions,
                                             archiveHandle->storageInfo->jobOptions->cryptPasswordMode,
                                             CALLBACK(archiveHandle->getNamePasswordFunction,archiveHandle->getNamePasswordUserData)
                                            );
          while (   !decryptedFlag
                 && (password != NULL)
                )
          {
            error = Crypt_setPublicPrivateKeyData(&privateCryptKey,
                                                  cryptPrivateKey->data,
                                                  cryptPrivateKey->length,
                                                  CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                                                  archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                                  &archiveHandle->archiveCryptInfo->cryptSalt,
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
            Crypt_doneKey(&privateCryptKey);
            return error;
          }

          // read encryption key for asymmetric encrypted data
          error = readEncryptionKey(archiveHandle,&chunkHeader,&privateCryptKey);
          if (error != ERROR_NONE)
          {
            Crypt_doneKey(&privateCryptKey);
            return error;
          }

          // free resources
          Crypt_doneKey(&privateCryptKey);

          scanMode = FALSE;
        }
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
        if (IS_SET(flags,ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS))
        {
          // unknown chunk -> switch to scan mode
          if (!scanMode)
          {
            if (IS_SET(flags,ARCHIVE_FLAG_PRINT_UNKNOWN_CHUNKS))
            {
              printWarning("Skipped unknown chunk '%s' at offset %llu in '%s'\n",
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
          return ERRORX_(UNKNOWN_CHUNK,
                         0,
                         "'%s' at offset %llu",
                         Chunk_idToString(chunkHeader.id),
                         chunkHeader.offset
                        );
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
  if (archiveCryptInfo != NULL) (*archiveCryptInfo) = archiveHandle->archiveCryptInfo;
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
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);

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

#ifdef NDEBUG
  Errors Archive_readMetaEntry(ArchiveEntryInfo *archiveEntryInfo,
                               ArchiveHandle    *archiveHandle,
                               String           userName,
                               String           hostName,
                               String           jobUUID,
                               String           scheduleUUID,
                               ArchiveTypes     *archiveType,
                               uint64           *createdDateTime,
                               String           comment
                              )
#else /* not NDEBUG */
  Errors __Archive_readMetaEntry(const char       *__fileName__,
                                 ulong            __lineNb__,
                                 ArchiveEntryInfo *archiveEntryInfo,
                                 ArchiveHandle    *archiveHandle,
                                 String           userName,
                                 String           hostName,
                                 String           jobUUID,
                                 String           scheduleUUID,
                                 ArchiveTypes     *archiveType,
                                 uint64           *createdDateTime,
                                 String           comment
                                )
#endif /* NDEBUG */
{
  AutoFreeList       autoFreeList1,autoFreeList2;
  Errors             error;
//  ChunkMeta          chunkMeta;
  ChunkHeader        chunkHeader;
//  CryptAlgorithms    cryptAlgorithm;
//  uint               blockLength;
  uint               keyLength;
  DecryptKeyIterator decryptKeyIterator;
  const CryptKey     *decryptKey;
  bool               passwordFlag;
//  CryptInfo          cryptInfo;
//  ChunkMetaEntry     chunkMetaEntry;
  uint64             index;
  bool               decryptedFlag;
  ChunkHeader        subChunkHeader;
  bool               foundMetaEntryFlag;

  assert(archiveEntryInfo != NULL);
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(archiveHandle->mode == ARCHIVE_MODE_READ);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  archiveEntryInfo->archiveHandle    = archiveHandle;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_META;

  // init meta chunk
  error = Chunk_init(&archiveEntryInfo->meta.chunkMeta.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_META,
                     CHUNK_DEFINITION_META,
                     DEFAULT_ALIGNMENT,
                     NULL,  // cryptInfo
                     &archiveEntryInfo->meta.chunkMeta
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->meta.chunkMeta.info,{ Chunk_done(&archiveEntryInfo->meta.chunkMeta.info); });

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
  assert(Chunk_getSize(&archiveEntryInfo->meta.chunkMeta.info,NULL,0) == ALIGN(CHUNK_FIXED_SIZE_META,archiveEntryInfo->meta.chunkMeta.info.alignment));
  error = Chunk_open(&archiveEntryInfo->meta.chunkMeta.info,
                     &chunkHeader,
                     CHUNK_FIXED_SIZE_META,
                     archiveHandle
                    );
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->meta.chunkMeta.info,{ Chunk_close(&archiveEntryInfo->meta.chunkMeta.info); });

  // get and check crypt algorithm
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->meta.chunkMeta.cryptAlgorithms[0]))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
//TODO: multi crypt
  archiveEntryInfo->cryptAlgorithms[0] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->meta.chunkMeta.cryptAlgorithms[0]);

  // detect crypt block length, crypt key length
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithms[0],&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);
  error = Crypt_getKeyLength(archiveEntryInfo->cryptAlgorithms[0],&keyLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  // try to read meta entry with all decrypt keys
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->meta.chunkMeta.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0]))
  {
    if (archiveHandle->archiveCryptInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      decryptKey = &archiveHandle->archiveCryptInfo->cryptKey;
    }
    else
    {
      decryptKey = getFirstDecryptKey(&decryptKeyIterator,
                                      archiveHandle,
                                      archiveHandle->storageInfo->jobOptions->cryptPasswordMode,
                                      &archiveHandle->storageInfo->jobOptions->cryptPassword,
                                      CALLBACK(archiveHandle->getNamePasswordFunction,archiveHandle->getNamePasswordUserData),
                                      archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                      &archiveHandle->archiveCryptInfo->cryptSalt,
                                      keyLength
                                     );
    }
    passwordFlag  = (decryptKey != NULL);
    decryptedFlag = FALSE;
  }
  else
  {
    decryptKey    = NULL;
    passwordFlag  = FALSE;
    decryptedFlag = TRUE;
  }
  foundMetaEntryFlag = FALSE;
  do
  {
    // reset
    AutoFree_freeAll(&autoFreeList2);
    error = Chunk_seek(&archiveEntryInfo->meta.chunkMeta.info,index);

    // check decrypt key (if encrypted)
//TODO: multi-crypt
    if (   Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0])
        && (decryptKey == NULL)
       )
    {
      error = ERROR_NO_DECRYPT_KEY;
    }

    // init meta entry crypt
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->meta.chunkMetaEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->meta.chunkMetaEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->meta.chunkMetaEntry.cryptInfo); });
      }
    }

    // init meta entry chunk
    if (error == ERROR_NONE)
    {
      error = Chunk_init(&archiveEntryInfo->meta.chunkMetaEntry.info,
                         &archiveEntryInfo->meta.chunkMeta.info,
                         CHUNK_USE_PARENT,
                         CHUNK_USE_PARENT,
                         CHUNK_ID_META_ENTRY,
                         CHUNK_DEFINITION_META_ENTRY,
                         archiveEntryInfo->blockLength,
                         &archiveEntryInfo->meta.chunkMetaEntry.cryptInfo,
                         &archiveEntryInfo->meta.chunkMetaEntry
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->meta.chunkMetaEntry.info,{ Chunk_done(&archiveEntryInfo->meta.chunkMetaEntry.info); });
      }
    }

    // read meta entry
    if (error == ERROR_NONE)
    {
      while (   !Chunk_eofSub(&archiveEntryInfo->meta.chunkMeta.info)
             && !foundMetaEntryFlag
             && (error == ERROR_NONE)
            )
      {
        error = Chunk_nextSub(&archiveEntryInfo->meta.chunkMeta.info,&subChunkHeader);
        if (error != ERROR_NONE)
        {
          break;
        }

        switch (subChunkHeader.id)
        {
          case CHUNK_ID_META_ENTRY:
            // read meta entry chunk
            error = Chunk_open(&archiveEntryInfo->meta.chunkMetaEntry.info,
                               &subChunkHeader,
                               subChunkHeader.size,
                               archiveHandle
                              );
            if (error != ERROR_NONE)
            {
              break;
            }
            AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->meta.chunkMetaEntry.info,{ Chunk_close(&archiveEntryInfo->meta.chunkMetaEntry.info); });

            // get meta data
            if (userName        != NULL) String_set(userName,archiveEntryInfo->meta.chunkMetaEntry.userName);
            if (hostName        != NULL) String_set(hostName,archiveEntryInfo->meta.chunkMetaEntry.hostName);
            if (jobUUID         != NULL) String_set(jobUUID,archiveEntryInfo->meta.chunkMetaEntry.jobUUID);
            if (scheduleUUID    != NULL) String_set(scheduleUUID,archiveEntryInfo->meta.chunkMetaEntry.scheduleUUID);
            if (archiveType     != NULL) (*archiveType) = archiveEntryInfo->meta.chunkMetaEntry.archiveType;
            if (createdDateTime != NULL) (*createdDateTime) = archiveEntryInfo->meta.chunkMetaEntry.createdDateTime;
            if (comment         != NULL) String_set(comment,archiveEntryInfo->meta.chunkMetaEntry.comment);

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
            error = Chunk_skipSub(&archiveEntryInfo->meta.chunkMeta.info,&subChunkHeader);
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
      if (   Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0])
          && (archiveHandle->archiveCryptInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        // get next decrypt key
        decryptKey = getNextDecryptKey(&decryptKeyIterator,
                                       archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                       &archiveHandle->archiveCryptInfo->cryptSalt,
                                       keyLength
                                      );
      }
      else
      {
        // no more decrypt keys when no encryption or asymmetric encryption is used
        decryptKey = NULL;
      }
    }
  }
  while ((error != ERROR_NONE) && (decryptKey != NULL));
  AUTOFREE_ADD(&autoFreeList1,&autoFreeList2,{ AutoFree_cleanup(&autoFreeList2); });
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
    if      (error != ERROR_NONE)                                                      return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0]) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                           return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundMetaEntryFlag)                                                      return ERROR_NO_META_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  // done resources
  AutoFree_done(&autoFreeList1);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,ArchiveEntryInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,ArchiveEntryInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_readFileEntry(ArchiveEntryInfo          *archiveEntryInfo,
                               ArchiveHandle             *archiveHandle,
                               CompressAlgorithms        *deltaCompressAlgorithm,
                               CompressAlgorithms        *byteCompressAlgorithm,
                               CryptTypes                *cryptType,
                               CryptAlgorithms           *cryptAlgorithm,
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
                                 CryptTypes                *cryptType,
                                 CryptAlgorithms           *cryptAlgorithm,
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
  AutoFreeList       autoFreeList1,autoFreeList2;
  Errors             error;
  ChunkHeader        chunkHeader;
  uint64             index;
  uint               keyLength;
  DecryptKeyIterator decryptKeyIterator;
  const CryptKey     *decryptKey;
  bool               passwordFlag;
  bool               decryptedFlag;
  ChunkHeader        subChunkHeader;
  bool               foundFileEntryFlag,foundFileDataFlag;

  assert(archiveEntryInfo != NULL);
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(archiveHandle->mode == ARCHIVE_MODE_READ);
  assert(fileName != NULL);
  assert(SIZE_OF_ARRAY(archiveEntryInfo->cryptAlgorithms) == 4);

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

  archiveEntryInfo->archiveHandle                  = archiveHandle;
  archiveEntryInfo->archiveEntryType               = ARCHIVE_ENTRY_TYPE_FILE;

  archiveEntryInfo->file.deltaSourceHandleInitFlag = FALSE;

  archiveEntryInfo->file.byteBuffer                = NULL;
  archiveEntryInfo->file.byteBufferSize            = 0L;
  archiveEntryInfo->file.deltaBuffer               = NULL;
  archiveEntryInfo->file.deltaBufferSize           = 0L;

  // init file chunk
  error = Chunk_init(&archiveEntryInfo->file.chunkFile.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_FILE,
                     CHUNK_DEFINITION_FILE,
//TODO: DEFAULT_ALIGNMENT
                     DEFAULT_ALIGNMENT,
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
  assert(Chunk_getSize(&archiveEntryInfo->file.chunkFile.info,NULL,0) == ALIGN(CHUNK_FIXED_SIZE_FILE,archiveEntryInfo->file.chunkFile.info.alignment));
  error = Chunk_open(&archiveEntryInfo->file.chunkFile.info,
                     &chunkHeader,
                     CHUNK_FIXED_SIZE_FILE,
                     archiveHandle
                    );
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->file.chunkFile.info,{ Chunk_close(&archiveEntryInfo->file.chunkFile.info); });

  // get and check compress algorithms
  if (!Compress_isValidAlgorithm(archiveEntryInfo->file.chunkFile.compressAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_COMPRESS_ALGORITHM;
  }
  archiveEntryInfo->file.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->file.byteCompressAlgorithm  = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->file.chunkFile.compressAlgorithm);

  // get and check crypt algorithm
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->file.chunkFile.cryptAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
#ifdef MULTI_CRYPT
  archiveEntryInfo->cryptAlgorithms[0] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->file.chunkFile.cryptAlgorithms[0]);
  archiveEntryInfo->cryptAlgorithms[1] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->file.chunkFile.cryptAlgorithms[1]);
  archiveEntryInfo->cryptAlgorithms[2] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->file.chunkFile.cryptAlgorithms[2]);
  archiveEntryInfo->cryptAlgorithms[3] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->file.chunkFile.cryptAlgorithms[3]);
#else
  archiveEntryInfo->cryptAlgorithms[0] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->file.chunkFile.cryptAlgorithm);
  archiveEntryInfo->cryptAlgorithms[1] = CRYPT_ALGORITHM_NONE;
  archiveEntryInfo->cryptAlgorithms[2] = CRYPT_ALGORITHM_NONE;
  archiveEntryInfo->cryptAlgorithms[3] = CRYPT_ALGORITHM_NONE;
#endif

  // detect crypt block length, crypt key length
#ifndef MULTI_CRYPT
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithms[0],&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
#else
  error = cryptGetBlockLength(archiveEntryInfo->cryptAlgorithms,SIZE_OF_ARRAY(archiveEntryInfo->cryptAlgorithms),&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
#endif
  assert(archiveEntryInfo->blockLength > 0);
  error = Crypt_getKeyLength(archiveEntryInfo->cryptAlgorithms[0],&keyLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

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

  // try to read file entry with all decrypt keys
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->file.chunkFile.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0]))
  {
    if (archiveHandle->archiveCryptInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      decryptKey = &archiveHandle->archiveCryptInfo->cryptKey;
    }
    else
    {
      decryptKey = getFirstDecryptKey(&decryptKeyIterator,
                                      archiveHandle,
                                      archiveHandle->storageInfo->jobOptions->cryptPasswordMode,
                                      &archiveHandle->storageInfo->jobOptions->cryptPassword,
                                      CALLBACK(archiveHandle->getNamePasswordFunction,archiveHandle->getNamePasswordUserData),
                                      archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                      &archiveHandle->archiveCryptInfo->cryptSalt,
                                      keyLength
                                     );
    }
    passwordFlag  = (decryptKey != NULL);
    decryptedFlag = FALSE;
  }
  else
  {
    decryptKey    = NULL;
    passwordFlag  = FALSE;
    decryptedFlag = TRUE;
  }
  foundFileEntryFlag = FALSE;
  foundFileDataFlag  = FALSE;
  do
  {
//fprintf(stderr,"%s, %d: decrpyt key \n",__FILE__,__LINE__); debugDumpMemory(decryptKey->data,decryptKey->dataLength,0);
    // reset
    AutoFree_freeAll(&autoFreeList2);
    error = Chunk_seek(&archiveEntryInfo->file.chunkFile.info,index);

    // check decrypt key (if encrypted)
//TODO: multi-crypt
    if (   Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0])
        && (decryptKey == NULL)
       )
    {
      error = ERROR_NO_DECRYPT_KEY;
    }

    // init file entry/extended attribute/delta/data/file crypt
    if (error == ERROR_NONE)
    {
//fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__); debugDumpMemory(archiveHandle->cryptSalt,sizeof(archiveHandle->cryptSalt),0);
#ifndef MULTI_CRYPT
      error = Crypt_init(&archiveEntryInfo->file.chunkFileEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
#else
      error = multiCryptInit(&archiveEntryInfo->file.chunkFileEntry.cryptInfos,
                             archiveEntryInfo->cryptAlgorithms,
                             SIZE_OF_ARRAY(archiveEntryInfo->cryptAlgorithms),
                             decryptKey,
                             &archiveHandle->archiveCryptInfo->cryptSalt,
NULL//                             password
                       );
#endif
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->file.chunkFileEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileEntry.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileExtendedAttribute.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->file.chunkFileDelta.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->file.chunkFileDelta.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileDelta.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->file.chunkFileData.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->file.chunkFileData.cryptInfo,{ Crypt_done(&archiveEntryInfo->file.chunkFileData.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->file.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
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
                               subChunkHeader.size,
                               archiveHandle
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
              memClear(fileInfo,sizeof(FileInfo));
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
            if (fileExtendedAttributeList != NULL)
            {
              // read file extended attribute chunk
              error = Chunk_open(&archiveEntryInfo->file.chunkFileExtendedAttribute.info,
                                 &subChunkHeader,
                                 subChunkHeader.size,
                                 archiveHandle
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
            break;
          case CHUNK_ID_FILE_DELTA:
            // read file delta chunk
            error = Chunk_open(&archiveEntryInfo->file.chunkFileDelta.info,
                               &subChunkHeader,
                               subChunkHeader.size,
                               archiveHandle
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            // get delta compress meta data
            archiveEntryInfo->file.deltaCompressAlgorithm = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->file.chunkFileDelta.deltaAlgorithm);
            if (   !Compress_isValidAlgorithm(archiveEntryInfo->file.chunkFileDelta.deltaAlgorithm)
                || !Compress_isDeltaCompressed(archiveEntryInfo->file.deltaCompressAlgorithm)
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
            // read file data chunk (only header)
            assert(Chunk_getSize(&archiveEntryInfo->file.chunkFileData.info,NULL,0) == ALIGN(CHUNK_FIXED_SIZE_FILE_DATA,archiveEntryInfo->file.chunkFileData.info.alignment));
            error = Chunk_open(&archiveEntryInfo->file.chunkFileData.info,
                               &subChunkHeader,
                               CHUNK_FIXED_SIZE_FILE_DATA,
                               archiveHandle
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
      if (   Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0])
          && (archiveHandle->archiveCryptInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        // get next decrypt key
        decryptKey = getNextDecryptKey(&decryptKeyIterator,
                                       archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                       &archiveHandle->archiveCryptInfo->cryptSalt,
                                       keyLength
                                      );
      }
      else
      {
        // no more decrypt keys when no encryption or asymmetric encryption is used
        decryptKey = NULL;
      }
    }
  }
  while ((error != ERROR_NONE) && (decryptKey != NULL));
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
    if      (error != ERROR_NONE)                                                      return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0]) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                           return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundFileEntryFlag)                                                      return ERROR_NO_FILE_ENTRY;
    else if (!foundFileDataFlag)                                                       return ERRORX_(NO_FILE_DATA,0,"%s",String_cString(fileName));
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  // init delta decompress (if no delta-compression is enabled, use identity-compressor)
  error = Compress_init(&archiveEntryInfo->file.deltaCompressInfo,
                        COMPRESS_MODE_INFLATE,
                        archiveEntryInfo->file.deltaCompressAlgorithm,
                        1,  // blockLength
                        &archiveEntryInfo->file.deltaSourceHandle
                       );
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->file.deltaCompressInfo,{ Compress_done(&archiveEntryInfo->file.deltaCompressInfo); });

  // init byte decompress (if no byte-compression is enabled, use identity-compressor)
  error = Compress_init(&archiveEntryInfo->file.byteCompressInfo,
                        COMPRESS_MODE_INFLATE,
                        archiveEntryInfo->file.byteCompressAlgorithm,
                        archiveEntryInfo->blockLength,
                        NULL  // deltaSourceHandle
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
  if (cryptAlgorithm         != NULL) (*cryptAlgorithm)         = archiveEntryInfo->cryptAlgorithms[0];
  if (cryptType              != NULL) (*cryptType)              = archiveHandle->archiveCryptInfo->cryptType;

  // done resources
  AutoFree_done(&autoFreeList2);
  AutoFree_done(&autoFreeList1);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,ArchiveEntryInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,ArchiveEntryInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_readImageEntry(ArchiveEntryInfo   *archiveEntryInfo,
                                ArchiveHandle      *archiveHandle,
                                CompressAlgorithms *deltaCompressAlgorithm,
                                CompressAlgorithms *byteCompressAlgorithm,
                                CryptTypes         *cryptType,
                                CryptAlgorithms    *cryptAlgorithm,
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
                                  CryptTypes         *cryptType,
                                  CryptAlgorithms    *cryptAlgorithm,
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
  AutoFreeList       autoFreeList1,autoFreeList2;
  Errors             error;
  ChunkHeader        chunkHeader;
  uint64             index;
  uint               keyLength;
  DecryptKeyIterator decryptKeyIterator;
  const CryptKey     *decryptKey;
  bool               passwordFlag;
  bool               decryptedFlag;
  ChunkHeader        subChunkHeader;
  bool               foundImageEntryFlag,foundImageDataFlag;

  assert(archiveEntryInfo != NULL);
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(archiveHandle->mode == ARCHIVE_MODE_READ);
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

  archiveEntryInfo->archiveHandle                   = archiveHandle;
  archiveEntryInfo->archiveEntryType                = ARCHIVE_ENTRY_TYPE_IMAGE;

  archiveEntryInfo->image.deltaSourceHandleInitFlag = FALSE;

  archiveEntryInfo->image.byteBuffer                = NULL;
  archiveEntryInfo->image.byteBufferSize            = 0L;
  archiveEntryInfo->image.deltaBuffer               = NULL;
  archiveEntryInfo->image.deltaBufferSize           = 0L;

  // init image chunk
  error = Chunk_init(&archiveEntryInfo->image.chunkImage.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_IMAGE,
                     CHUNK_DEFINITION_IMAGE,
//TODO: DEFAULT_ALIGNMENT
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
  assert(Chunk_getSize(&archiveEntryInfo->image.chunkImage.info,NULL,0) == ALIGN(CHUNK_FIXED_SIZE_IMAGE,archiveEntryInfo->image.chunkImage.info.alignment));
  error = Chunk_open(&archiveEntryInfo->image.chunkImage.info,
                     &chunkHeader,
                     CHUNK_FIXED_SIZE_IMAGE,
                     archiveHandle
                    );
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->image.chunkImage.info,{ Chunk_close(&archiveEntryInfo->image.chunkImage.info); });

  // get and check compress algorithms
  if (!Compress_isValidAlgorithm(archiveEntryInfo->image.chunkImage.compressAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_COMPRESS_ALGORITHM;
  }
  archiveEntryInfo->image.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->image.byteCompressAlgorithm  = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->image.chunkImage.compressAlgorithm);

  // get and check crypt algorithm
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->image.chunkImage.cryptAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
#ifdef MULTI_CRYPT
  archiveEntryInfo->cryptAlgorithms[0] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->image.chunkImage.cryptAlgorithms[0]);
  archiveEntryInfo->cryptAlgorithms[1] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->image.chunkImage.cryptAlgorithms[1]);
  archiveEntryInfo->cryptAlgorithms[2] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->image.chunkImage.cryptAlgorithms[2]);
  archiveEntryInfo->cryptAlgorithms[3] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->image.chunkImage.cryptAlgorithms[3]);
#else
  archiveEntryInfo->cryptAlgorithms[0] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->image.chunkImage.cryptAlgorithm);
  archiveEntryInfo->cryptAlgorithms[1] = CRYPT_ALGORITHM_NONE;
  archiveEntryInfo->cryptAlgorithms[2] = CRYPT_ALGORITHM_NONE;
  archiveEntryInfo->cryptAlgorithms[3] = CRYPT_ALGORITHM_NONE;
#endif

  // detect crypt block length, crypt key length
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithms[0],&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);
  error = Crypt_getKeyLength(archiveEntryInfo->cryptAlgorithms[0],&keyLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

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

  // try to read image entry with all decrypt keys
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->image.chunkImage.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0]))
  {
    if (archiveHandle->archiveCryptInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      decryptKey = &archiveHandle->archiveCryptInfo->cryptKey;
    }
    else
    {
      decryptKey = getFirstDecryptKey(&decryptKeyIterator,
                                      archiveHandle,
                                      archiveHandle->storageInfo->jobOptions->cryptPasswordMode,
                                      &archiveHandle->storageInfo->jobOptions->cryptPassword,
                                      CALLBACK(archiveHandle->getNamePasswordFunction,archiveHandle->getNamePasswordUserData),
                                      archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                      &archiveHandle->archiveCryptInfo->cryptSalt,
                                      keyLength
                                     );
    }
    passwordFlag  = (decryptKey != NULL);
    decryptedFlag = FALSE;
  }
  else
  {
    decryptKey    = NULL;
    passwordFlag  = FALSE;
    decryptedFlag = TRUE;
  }
  foundImageEntryFlag = FALSE;
  foundImageDataFlag  = FALSE;
  do
  {
    // reset
    AutoFree_freeAll(&autoFreeList2);
    error = Chunk_seek(&archiveEntryInfo->image.chunkImage.info,index);

    // check decrypt key (if encrypted)
//TODO: multi-crypt
    if (   Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0])
        && (decryptKey == NULL)
       )
    {
      error = ERROR_NO_DECRYPT_KEY;
    }

    // init image entry/delta/data/image crypt
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->image.chunkImageEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->image.chunkImageEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->image.chunkImageDelta.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->image.chunkImageDelta.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->image.chunkImageData.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->image.chunkImageData.cryptInfo,{ Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->image.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
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
                               subChunkHeader.size,
                               archiveHandle
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
                               subChunkHeader.size,
                               archiveHandle
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            // get delta compress meta data
            archiveEntryInfo->image.deltaCompressAlgorithm = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->image.chunkImageDelta.deltaAlgorithm);
            if (   !Compress_isValidAlgorithm(archiveEntryInfo->image.chunkImageDelta.deltaAlgorithm)
                || !Compress_isDeltaCompressed(archiveEntryInfo->image.deltaCompressAlgorithm)
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
            // read image data chunk (only header)
            assert(Chunk_getSize(&archiveEntryInfo->image.chunkImageData.info,NULL,0) == ALIGN(CHUNK_FIXED_SIZE_IMAGE_DATA,archiveEntryInfo->image.chunkImageData.info.alignment));
            error = Chunk_open(&archiveEntryInfo->image.chunkImageData.info,
                               &subChunkHeader,
                               CHUNK_FIXED_SIZE_IMAGE_DATA,
                               archiveHandle
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
      if (   Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0])
          && (archiveHandle->archiveCryptInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        // get next decrypt key
        decryptKey = getNextDecryptKey(&decryptKeyIterator,
                                       archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                       &archiveHandle->archiveCryptInfo->cryptSalt,
                                       keyLength
                                      );
      }
      else
      {
        // no more decrypt keys when no encryption or asymmetric encryption is used
        decryptKey = NULL;
      }
    }
  }
  while ((error != ERROR_NONE) && (decryptKey != NULL));
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
    if      (error != ERROR_NONE)                                                      return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0]) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                           return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundImageEntryFlag)                                                     return ERROR_NO_IMAGE_ENTRY;
    else if (!foundImageDataFlag)                                                      return ERROR_NO_IMAGE_DATA;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  // init delta decompress (if no delta-compression is enabled, use identity-compressor)
  error = Compress_init(&archiveEntryInfo->image.deltaCompressInfo,
                        COMPRESS_MODE_INFLATE,
                        archiveEntryInfo->image.deltaCompressAlgorithm,
                        1,  // blockLength
                        &archiveEntryInfo->image.deltaSourceHandle
                       );
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->image.deltaCompressInfo,{ Compress_done(&archiveEntryInfo->image.deltaCompressInfo); });

  // init byte decompress (if no byte-compression is enabled, use identity-compressor)
  error = Compress_init(&archiveEntryInfo->image.byteCompressInfo,
                        COMPRESS_MODE_INFLATE,
                        archiveEntryInfo->image.byteCompressAlgorithm,
                        archiveEntryInfo->blockLength,
                        NULL  // deltaSourceHandle
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
  if (cryptAlgorithm         != NULL) (*cryptAlgorithm)         = archiveEntryInfo->cryptAlgorithms[0];
  if (cryptType              != NULL) (*cryptType)              = archiveHandle->archiveCryptInfo->cryptType;

  // done resources
  AutoFree_done(&autoFreeList2);
  AutoFree_done(&autoFreeList1);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,ArchiveEntryInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,ArchiveEntryInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_readDirectoryEntry(ArchiveEntryInfo          *archiveEntryInfo,
                                    ArchiveHandle             *archiveHandle,
                                    CryptTypes                *cryptType,
                                    CryptAlgorithms           *cryptAlgorithm,
                                    String                    directoryName,
                                    FileInfo                  *fileInfo,
                                    FileExtendedAttributeList *fileExtendedAttributeList
                                   )
#else /* not NDEBUG */
  Errors __Archive_readDirectoryEntry(const char                *__fileName__,
                                      ulong                     __lineNb__,
                                      ArchiveEntryInfo          *archiveEntryInfo,
                                      ArchiveHandle             *archiveHandle,
                                      CryptTypes                *cryptType,
                                      CryptAlgorithms           *cryptAlgorithm,
                                      String                    directoryName,
                                      FileInfo                  *fileInfo,
                                      FileExtendedAttributeList *fileExtendedAttributeList
                                     )
#endif /* NDEBUG */
{
  AutoFreeList       autoFreeList1,autoFreeList2;
  Errors             error;
  ChunkHeader        chunkHeader;
  uint64             index;
  uint               keyLength;
  DecryptKeyIterator decryptKeyIterator;
  const CryptKey     *decryptKey;
  bool               passwordFlag;
  bool               decryptedFlag;
  bool               foundDirectoryEntryFlag;
  ChunkHeader        subChunkHeader;

  assert(archiveEntryInfo != NULL);
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(archiveHandle->mode == ARCHIVE_MODE_READ);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  archiveEntryInfo->archiveHandle    = archiveHandle;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_DIRECTORY;

  // init directory chunk
  error = Chunk_init(&archiveEntryInfo->directory.chunkDirectory.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_DIRECTORY,
                     CHUNK_DEFINITION_DIRECTORY,
//TODO: DEFAULT_ALIGNMENT
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
  assert(Chunk_getSize(&archiveEntryInfo->directory.chunkDirectory.info,NULL,0) == ALIGN(CHUNK_FIXED_SIZE_DIRECTORY,archiveEntryInfo->directory.chunkDirectory.info.alignment));
  error = Chunk_open(&archiveEntryInfo->directory.chunkDirectory.info,
                     &chunkHeader,
                     CHUNK_FIXED_SIZE_DIRECTORY,
                     archiveHandle
                    );
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->directory.chunkDirectory.info,{ Chunk_close(&archiveEntryInfo->directory.chunkDirectory.info); });

  // get and check crypt algorithm
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->directory.chunkDirectory.cryptAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
#ifdef MULTI_CRYPT
  archiveEntryInfo->cryptAlgorithms[0] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->directory.chunkDirectory.cryptAlgorithms[0]);
  archiveEntryInfo->cryptAlgorithms[1] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->directory.chunkDirectory.cryptAlgorithms[1]);
  archiveEntryInfo->cryptAlgorithms[2] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->directory.chunkDirectory.cryptAlgorithms[2]);
  archiveEntryInfo->cryptAlgorithms[3] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->directory.chunkDirectory.cryptAlgorithms[3]);
#else
  archiveEntryInfo->cryptAlgorithms[0] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->directory.chunkDirectory.cryptAlgorithm);
  archiveEntryInfo->cryptAlgorithms[1] = CRYPT_ALGORITHM_NONE;
  archiveEntryInfo->cryptAlgorithms[2] = CRYPT_ALGORITHM_NONE;
  archiveEntryInfo->cryptAlgorithms[3] = CRYPT_ALGORITHM_NONE;
#endif

  // detect crypt block length, crypt key length
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithms[0],&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);
  error = Crypt_getKeyLength(archiveEntryInfo->cryptAlgorithms[0],&keyLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  // try to read directory entry with all decrypt keys
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->directory.chunkDirectory.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0]))
  {
    if (archiveHandle->archiveCryptInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      decryptKey = &archiveHandle->archiveCryptInfo->cryptKey;
    }
    else
    {
      decryptKey = getFirstDecryptKey(&decryptKeyIterator,
                                      archiveHandle,
                                      archiveHandle->storageInfo->jobOptions->cryptPasswordMode,
                                      &archiveHandle->storageInfo->jobOptions->cryptPassword,
                                      CALLBACK(archiveHandle->getNamePasswordFunction,archiveHandle->getNamePasswordUserData),
                                      archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                      &archiveHandle->archiveCryptInfo->cryptSalt,
                                      keyLength
                                     );
    }
    passwordFlag  = (decryptKey != NULL);
    decryptedFlag = FALSE;
  }
  else
  {
    decryptKey    = NULL;
    passwordFlag  = FALSE;
    decryptedFlag = TRUE;
  }
  foundDirectoryEntryFlag = FALSE;
  do
  {
//fprintf(stderr,"%s, %d: decrpyt key \n",__FILE__,__LINE__); debugDumpMemory(decryptKey->data,decryptKey->dataLength,0);
    // reset
    AutoFree_freeAll(&autoFreeList2);
    error = Chunk_seek(&archiveEntryInfo->directory.chunkDirectory.info,index);

    // check decrypt key (if encrypted)
//TODO: multi-crypt
    if (   Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0])
        && (decryptKey == NULL)
       )
    {
      error = ERROR_NO_DECRYPT_KEY;
    }

    // init crypt
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->directory.chunkDirectoryEntry.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
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
                               subChunkHeader.size,
                               archiveHandle
                              );
            if (error != ERROR_NONE)
            {
              break;
            }
            AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->directory.chunkDirectoryEntry.info,{ Chunk_close(&archiveEntryInfo->directory.chunkDirectoryEntry.info); });

            // get directory meta data
            String_set(directoryName,archiveEntryInfo->directory.chunkDirectoryEntry.name);
            if (fileInfo != NULL)
            {
              memClear(fileInfo,sizeof(FileInfo));
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
            if (fileExtendedAttributeList != NULL)
            {
              // read directory extended attribute chunk
              error = Chunk_open(&archiveEntryInfo->directory.chunkDirectoryExtendedAttribute.info,
                                 &subChunkHeader,
                                 subChunkHeader.size,
                                 archiveHandle
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
      if (   Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0])
          && (archiveHandle->archiveCryptInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        // get next decrypt key
        decryptKey = getNextDecryptKey(&decryptKeyIterator,
                                       archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                       &archiveHandle->archiveCryptInfo->cryptSalt,
                                       keyLength
                                      );
      }
      else
      {
        // no more decrypt keys when no encryption or asymmetric encryption is used
        decryptKey = NULL;
      }
    }
  }
  while ((error != ERROR_NONE) && (decryptKey != NULL));
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
    if      (error != ERROR_NONE)                                                      return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0]) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                           return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundDirectoryEntryFlag)                                                 return ERROR_NO_DIRECTORY_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveEntryInfo->cryptAlgorithms[0];
  if (cryptType      != NULL) (*cryptType)      = archiveHandle->archiveCryptInfo->cryptType;

  // done resources
  AutoFree_done(&autoFreeList2);
  AutoFree_done(&autoFreeList1);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,ArchiveEntryInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,ArchiveEntryInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_readLinkEntry(ArchiveEntryInfo          *archiveEntryInfo,
                               ArchiveHandle             *archiveHandle,
                               CryptTypes                *cryptType,
                               CryptAlgorithms           *cryptAlgorithm,
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
                                 CryptTypes                *cryptType,
                                 CryptAlgorithms           *cryptAlgorithm,
                                 String                    linkName,
                                 String                    destinationName,
                                 FileInfo                  *fileInfo,
                                 FileExtendedAttributeList *fileExtendedAttributeList
                                )
#endif /* NDEBUG */
{
  AutoFreeList       autoFreeList1,autoFreeList2;
  Errors             error;
  ChunkHeader        chunkHeader;
  uint64             index;
  uint               keyLength;
  DecryptKeyIterator decryptKeyIterator;
  const CryptKey     *decryptKey;
  bool               passwordFlag;
  bool               decryptedFlag;
  bool               foundLinkEntryFlag;
  ChunkHeader        subChunkHeader;

  assert(archiveEntryInfo != NULL);
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(archiveHandle->mode == ARCHIVE_MODE_READ);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  archiveEntryInfo->archiveHandle    = archiveHandle;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_LINK;

  // init link chunk
  error = Chunk_init(&archiveEntryInfo->link.chunkLink.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_LINK,
                     CHUNK_DEFINITION_LINK,
//TODO: DEFAULT_ALIGNMENT
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
  assert(Chunk_getSize(&archiveEntryInfo->link.chunkLink.info,NULL,0) == ALIGN(CHUNK_FIXED_SIZE_LINK,archiveEntryInfo->link.chunkLink.info.alignment));
  error = Chunk_open(&archiveEntryInfo->link.chunkLink.info,
                     &chunkHeader,
                     CHUNK_FIXED_SIZE_LINK,
                     archiveHandle
                    );
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->link.chunkLink.info,{ Chunk_close(&archiveEntryInfo->link.chunkLink.info); });

  // get and check crypt algorithm
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->link.chunkLink.cryptAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
#ifdef MULTI_CRYPT
  archiveEntryInfo->cryptAlgorithms[0] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->link.chunkLink.cryptAlgorithms[0]);
  archiveEntryInfo->cryptAlgorithms[1] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->link.chunkLink.cryptAlgorithms[1]);
  archiveEntryInfo->cryptAlgorithms[2] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->link.chunkLink.cryptAlgorithms[2]);
  archiveEntryInfo->cryptAlgorithms[3] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->link.chunkLink.cryptAlgorithms[3]);
#else
  archiveEntryInfo->cryptAlgorithms[0] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->link.chunkLink.cryptAlgorithm);
  archiveEntryInfo->cryptAlgorithms[1] = CRYPT_ALGORITHM_NONE;
  archiveEntryInfo->cryptAlgorithms[2] = CRYPT_ALGORITHM_NONE;
  archiveEntryInfo->cryptAlgorithms[3] = CRYPT_ALGORITHM_NONE;
#endif

  // detect crypt block length, crypt key length
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithms[0],&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);
  error = Crypt_getKeyLength(archiveEntryInfo->cryptAlgorithms[0],&keyLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  // try to read link entry with all decrypt keys
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->link.chunkLink.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0]))
  {
    if (archiveHandle->archiveCryptInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      decryptKey = &archiveHandle->archiveCryptInfo->cryptKey;
    }
    else
    {
      decryptKey = getFirstDecryptKey(&decryptKeyIterator,
                                      archiveHandle,
                                      archiveHandle->storageInfo->jobOptions->cryptPasswordMode,
                                      &archiveHandle->storageInfo->jobOptions->cryptPassword,
                                      CALLBACK(archiveHandle->getNamePasswordFunction,archiveHandle->getNamePasswordUserData),
                                      archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                      &archiveHandle->archiveCryptInfo->cryptSalt,
                                      keyLength
                                     );
    }
    passwordFlag  = (decryptKey != NULL);
    decryptedFlag = FALSE;
  }
  else
  {
    decryptKey    = NULL;
    passwordFlag  = FALSE;
    decryptedFlag = TRUE;
  }
  foundLinkEntryFlag = FALSE;
  do
  {
    // reset
    AutoFree_freeAll(&autoFreeList2);
    error = Chunk_seek(&archiveEntryInfo->link.chunkLink.info,index);

    // check decrypt key (if encrypted)
//TODO: multi-crypt
    if (   Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0])
        && (decryptKey == NULL)
       )
    {
      error = ERROR_NO_DECRYPT_KEY;
    }

    // init crypt
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->link.chunkLinkEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->link.chunkLinkEntry.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->link.chunkLinkExtendedAttribute.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
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
                               subChunkHeader.size,
                               archiveHandle
                              );
            if (error != ERROR_NONE)
            {
              break;
            }
            AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->link.chunkLinkEntry.info,{ Chunk_close(&archiveEntryInfo->link.chunkLinkEntry.info); });

            // get link meta data
            String_set(linkName,archiveEntryInfo->link.chunkLinkEntry.name);
            String_set(destinationName,archiveEntryInfo->link.chunkLinkEntry.destinationName);
            if (fileInfo != NULL)
            {
              memClear(fileInfo,sizeof(FileInfo));
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
            if (fileExtendedAttributeList != NULL)
            {
              // read link extended attribute chunk
              error = Chunk_open(&archiveEntryInfo->link.chunkLinkExtendedAttribute.info,
                                 &subChunkHeader,
                                 subChunkHeader.size,
                                 archiveHandle
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
      if (   Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0])
          && (archiveHandle->archiveCryptInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        // get next decrypt key
        decryptKey = getNextDecryptKey(&decryptKeyIterator,
                                       archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                       &archiveHandle->archiveCryptInfo->cryptSalt,
                                       keyLength
                                      );
      }
      else
      {
        // no more decrypt keys when no encryption or asymmetric encryption is used
        decryptKey = NULL;
      }
    }
  }
  while ((error != ERROR_NONE) && (decryptKey != NULL));
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
    if      (error != ERROR_NONE)                                                      return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0]) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                           return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundLinkEntryFlag)                                                      return ERROR_NO_LINK_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveEntryInfo->cryptAlgorithms[0];
  if (cryptType      != NULL) (*cryptType)      = archiveHandle->archiveCryptInfo->cryptType;

  // done resources
  AutoFree_done(&autoFreeList2);
  AutoFree_done(&autoFreeList1);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,ArchiveEntryInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,ArchiveEntryInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_readHardLinkEntry(ArchiveEntryInfo          *archiveEntryInfo,
                                   ArchiveHandle             *archiveHandle,
                                   CompressAlgorithms        *deltaCompressAlgorithm,
                                   CompressAlgorithms        *byteCompressAlgorithm,
                                   CryptTypes                *cryptType,
                                   CryptAlgorithms           *cryptAlgorithm,
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
                                     CryptTypes                *cryptType,
                                     CryptAlgorithms           *cryptAlgorithm,
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
  AutoFreeList       autoFreeList1,autoFreeList2;
  Errors             error;
  ChunkHeader        chunkHeader;
  uint64             index;
  uint               keyLength;
  DecryptKeyIterator decryptKeyIterator;
  const CryptKey     *decryptKey;
  bool               passwordFlag;
  bool               decryptedFlag;
  ChunkHeader        subChunkHeader;
  bool               foundHardLinkEntryFlag,foundHardLinkDataFlag;

  assert(archiveEntryInfo != NULL);
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(archiveHandle->mode == ARCHIVE_MODE_READ);
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

  archiveEntryInfo->archiveHandle                      = archiveHandle;
  archiveEntryInfo->archiveEntryType                   = ARCHIVE_ENTRY_TYPE_HARDLINK;

  archiveEntryInfo->hardLink.deltaSourceHandleInitFlag = FALSE;

  archiveEntryInfo->hardLink.byteBuffer                = NULL;
  archiveEntryInfo->hardLink.byteBufferSize            = 0L;
  archiveEntryInfo->hardLink.deltaBuffer               = NULL;
  archiveEntryInfo->hardLink.deltaBufferSize           = 0L;

  // init hard link chunk
  error = Chunk_init(&archiveEntryInfo->hardLink.chunkHardLink.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_HARDLINK,
                     CHUNK_DEFINITION_HARDLINK,
//TODO: DEFAULT_ALIGNMENT
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
  assert(Chunk_getSize(&archiveEntryInfo->hardLink.chunkHardLink.info,NULL,0) == ALIGN(CHUNK_FIXED_SIZE_HARDLINK,archiveEntryInfo->hardLink.chunkHardLink.info.alignment));
  error = Chunk_open(&archiveEntryInfo->hardLink.chunkHardLink.info,
                     &chunkHeader,
                     CHUNK_FIXED_SIZE_HARDLINK,
                     archiveHandle
                    );
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->hardLink.chunkHardLink.info,{ Chunk_close(&archiveEntryInfo->hardLink.chunkHardLink.info); });

  // get and check compress algorithms
  if (!Compress_isValidAlgorithm(archiveEntryInfo->hardLink.chunkHardLink.compressAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_COMPRESS_ALGORITHM;
  }
  archiveEntryInfo->hardLink.deltaCompressAlgorithm = COMPRESS_ALGORITHM_NONE;
  archiveEntryInfo->hardLink.byteCompressAlgorithm  = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->hardLink.chunkHardLink.compressAlgorithm);

  // get and check crypt algorithm
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
#ifdef MULTI_CRYPT
  archiveEntryInfo->cryptAlgorithms[0] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithms[0]);
  archiveEntryInfo->cryptAlgorithms[1] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithms[1]);
  archiveEntryInfo->cryptAlgorithms[2] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithms[2]);
  archiveEntryInfo->cryptAlgorithms[3] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithms[3]);
#else
  archiveEntryInfo->cryptAlgorithms[0] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->hardLink.chunkHardLink.cryptAlgorithm);
  archiveEntryInfo->cryptAlgorithms[1] = CRYPT_ALGORITHM_NONE;
  archiveEntryInfo->cryptAlgorithms[2] = CRYPT_ALGORITHM_NONE;
  archiveEntryInfo->cryptAlgorithms[3] = CRYPT_ALGORITHM_NONE;
#endif

  // detect crypt block length, crypt key length
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithms[0],&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);
  error = Crypt_getKeyLength(archiveEntryInfo->cryptAlgorithms[0],&keyLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

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

  // try to read hard link entry with all decrypt keys
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->hardLink.chunkHardLink.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0]))
  {
    if (archiveHandle->archiveCryptInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      decryptKey = &archiveHandle->archiveCryptInfo->cryptKey;
    }
    else
    {
      decryptKey = getFirstDecryptKey(&decryptKeyIterator,
                                      archiveHandle,
                                      archiveHandle->storageInfo->jobOptions->cryptPasswordMode,
                                      &archiveHandle->storageInfo->jobOptions->cryptPassword,
                                      CALLBACK(archiveHandle->getNamePasswordFunction,archiveHandle->getNamePasswordUserData),
                                      archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                      &archiveHandle->archiveCryptInfo->cryptSalt,
                                      keyLength
                                     );
    }
    passwordFlag  = (decryptKey != NULL);
    decryptedFlag = FALSE;
  }
  else
  {
    decryptKey    = NULL;
    passwordFlag  = FALSE;
    decryptedFlag = TRUE;
  }
  foundHardLinkEntryFlag = FALSE;
  foundHardLinkDataFlag  = FALSE;
  StringList_clear(fileNameList);
  do
  {
    // reset
    AutoFree_freeAll(&autoFreeList2);
    error = Chunk_seek(&archiveEntryInfo->hardLink.chunkHardLink.info,index);

    // check decrypt key (if encrypted)
//TODO: multi-crypt
    if (   Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0])
        && (decryptKey == NULL)
       )
    {
      error = ERROR_NO_DECRYPT_KEY;
    }

    // init crypt
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkEntry.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkName.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkDelta.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.chunkHardLinkData.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->hardLink.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.cryptInfo,{ Crypt_done(&archiveEntryInfo->hardLink.cryptInfo); });
      }
    }

    // init hard link entry/extended attributes/delta/data chunks
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
                               subChunkHeader.size,
                               archiveHandle
                              );
            if (error != ERROR_NONE)
            {
              break;
            }
            AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->hardLink.chunkHardLinkEntry.info,{ Chunk_close(&archiveEntryInfo->hardLink.chunkHardLinkEntry.info); });

            // get hard link meta data
            if (fileInfo != NULL)
            {
              memClear(fileInfo,sizeof(FileInfo));
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
                               subChunkHeader.size,
                               archiveHandle
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
            if (fileExtendedAttributeList != NULL)
            {
              // read hard link extended attribute chunk
              error = Chunk_open(&archiveEntryInfo->hardLink.chunkHardLinkExtendedAttribute.info,
                                 &subChunkHeader,
                                 subChunkHeader.size,
                                 archiveHandle
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
            break;
          case CHUNK_ID_HARDLINK_DELTA:
            // read hard link delta chunk
            error = Chunk_open(&archiveEntryInfo->hardLink.chunkHardLinkDelta.info,
                               &subChunkHeader,
                               subChunkHeader.size,
                               archiveHandle
                              );
            if (error != ERROR_NONE)
            {
              break;
            }

            // get delta compress meta data
            archiveEntryInfo->hardLink.deltaCompressAlgorithm = COMPRESS_CONSTANT_TO_ALGORITHM(archiveEntryInfo->hardLink.chunkHardLinkDelta.deltaAlgorithm);
            if (   !Compress_isValidAlgorithm(archiveEntryInfo->hardLink.chunkHardLinkDelta.deltaAlgorithm)
                || !Compress_isDeltaCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm)
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
            // read hard link data chunk (only header)
            assert(Chunk_getSize(&archiveEntryInfo->hardLink.chunkHardLinkData.info,NULL,0) == ALIGN(CHUNK_FIXED_SIZE_HARDLINK_DATA,archiveEntryInfo->hardLink.chunkHardLinkData.info.alignment));
            error = Chunk_open(&archiveEntryInfo->hardLink.chunkHardLinkData.info,
                               &subChunkHeader,
                               CHUNK_FIXED_SIZE_HARDLINK_DATA,
                               archiveHandle
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
      if (   Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0])
          && (archiveHandle->archiveCryptInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        // get next decrypt key
        decryptKey = getNextDecryptKey(&decryptKeyIterator,
                                       archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                       &archiveHandle->archiveCryptInfo->cryptSalt,
                                       keyLength
                                      );
      }
      else
      {
        // no more decrypt keys when no encryption or asymmetric encryption is used
        decryptKey = NULL;
      }
    }
  }
  while ((error != ERROR_NONE) && (decryptKey != NULL));
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
    if      (error != ERROR_NONE)                                                      return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0]) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                           return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundHardLinkEntryFlag)                                                  return ERROR_NO_FILE_ENTRY;
    else if (!foundHardLinkDataFlag)                                                   return ERRORX_(NO_FILE_DATA,0,"%s",String_cString(StringList_first(fileNameList,NULL)));
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  // init delta decompress (if no delta-compression is enabled, use identity-compressor)
  error = Compress_init(&archiveEntryInfo->hardLink.deltaCompressInfo,
                        COMPRESS_MODE_INFLATE,
                        archiveEntryInfo->hardLink.deltaCompressAlgorithm,
                        1,  // blockLength
                        &archiveEntryInfo->hardLink.deltaSourceHandle
                       );
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->hardLink.deltaCompressInfo,{ Compress_done(&archiveEntryInfo->hardLink.deltaCompressInfo); });

  // init byte decompress (if no byte-compression is enabled, use identity-compressor)
  error = Compress_init(&archiveEntryInfo->hardLink.byteCompressInfo,
                        COMPRESS_MODE_INFLATE,
                        archiveEntryInfo->hardLink.byteCompressAlgorithm,
                        archiveEntryInfo->blockLength,
                        NULL  // deltaSourceHandle
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
  if (cryptAlgorithm         != NULL) (*cryptAlgorithm)         = archiveEntryInfo->cryptAlgorithms[0];
  if (cryptType              != NULL) (*cryptType)              = archiveHandle->archiveCryptInfo->cryptType;

  // done resources
  AutoFree_done(&autoFreeList2);
  AutoFree_done(&autoFreeList1);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,ArchiveEntryInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,ArchiveEntryInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Archive_readSpecialEntry(ArchiveEntryInfo          *archiveEntryInfo,
                                  ArchiveHandle             *archiveHandle,
                                  CryptTypes                *cryptType,
                                  CryptAlgorithms           *cryptAlgorithm,
                                  String                    specialName,
                                  FileInfo                  *fileInfo,
                                  FileExtendedAttributeList *fileExtendedAttributeList
                                 )
#else /* not NDEBUG */
  Errors __Archive_readSpecialEntry(const char                *__fileName__,
                                    ulong                     __lineNb__,
                                    ArchiveEntryInfo          *archiveEntryInfo,
                                    ArchiveHandle             *archiveHandle,
                                    CryptTypes                *cryptType,
                                    CryptAlgorithms           *cryptAlgorithm,
                                    String                    specialName,
                                    FileInfo                  *fileInfo,
                                    FileExtendedAttributeList *fileExtendedAttributeList
                                   )
#endif /* NDEBUG */
{
  AutoFreeList       autoFreeList1,autoFreeList2;
  Errors             error;
  ChunkHeader        chunkHeader;
  uint64             index;
  uint               keyLength;
  DecryptKeyIterator decryptKeyIterator;
  const CryptKey     *decryptKey;
  bool               passwordFlag;
  bool               decryptedFlag;
  bool               foundSpecialEntryFlag;
  ChunkHeader        subChunkHeader;

  assert(archiveEntryInfo != NULL);
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->archiveCryptInfo != NULL);
  assert(archiveHandle->mode == ARCHIVE_MODE_READ);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList1);

  archiveEntryInfo->archiveHandle    = archiveHandle;
  archiveEntryInfo->archiveEntryType = ARCHIVE_ENTRY_TYPE_SPECIAL;

  // init special chunk
  error = Chunk_init(&archiveEntryInfo->special.chunkSpecial.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_SPECIAL,
                     CHUNK_DEFINITION_SPECIAL,
//TODO: DEFAULT_ALIGNMENT
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
  assert(Chunk_getSize(&archiveEntryInfo->special.chunkSpecial.info,NULL,0) == ALIGN(CHUNK_FIXED_SIZE_SPECIAL,archiveEntryInfo->special.chunkSpecial.info.alignment));
  error = Chunk_open(&archiveEntryInfo->special.chunkSpecial.info,
                     &chunkHeader,
                     CHUNK_FIXED_SIZE_SPECIAL,
                     archiveHandle
                    );
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList1,&archiveEntryInfo->special.chunkSpecial.info,{ Chunk_close(&archiveEntryInfo->special.chunkSpecial.info); });

  // get and check crypt algorithm
  if (!Crypt_isValidAlgorithm(archiveEntryInfo->special.chunkSpecial.cryptAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return ERROR_INVALID_CRYPT_ALGORITHM;
  }
#ifdef MULTI_CRYPT
  archiveEntryInfo->cryptAlgorithms[0] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->special.chunkSpecial.cryptAlgorithms[0]);
  archiveEntryInfo->cryptAlgorithms[1] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->special.chunkSpecial.cryptAlgorithms[1]);
  archiveEntryInfo->cryptAlgorithms[2] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->special.chunkSpecial.cryptAlgorithms[2]);
  archiveEntryInfo->cryptAlgorithms[3] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->special.chunkSpecial.cryptAlgorithms[3]);
#else
  archiveEntryInfo->cryptAlgorithms[0] = CRYPT_CONSTANT_TO_ALGORITHM(archiveEntryInfo->special.chunkSpecial.cryptAlgorithm);
  archiveEntryInfo->cryptAlgorithms[1] = CRYPT_ALGORITHM_NONE;
  archiveEntryInfo->cryptAlgorithms[2] = CRYPT_ALGORITHM_NONE;
  archiveEntryInfo->cryptAlgorithms[3] = CRYPT_ALGORITHM_NONE;
#endif

  // detect crypt block length, crypt key length
  error = Crypt_getBlockLength(archiveEntryInfo->cryptAlgorithms[0],&archiveEntryInfo->blockLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }
  assert(archiveEntryInfo->blockLength > 0);
  error = Crypt_getKeyLength(archiveEntryInfo->cryptAlgorithms[0],&keyLength);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList1);
    return error;
  }

  // try to read special entry with all decrypt keys
  AutoFree_init(&autoFreeList2);
  Chunk_tell(&archiveEntryInfo->special.chunkSpecial.info,&index);
  if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0]))
  {
    if (archiveHandle->archiveCryptInfo->cryptType == CRYPT_TYPE_ASYMMETRIC)
    {
      decryptKey = &archiveHandle->archiveCryptInfo->cryptKey;
    }
    else
    {
      decryptKey = getFirstDecryptKey(&decryptKeyIterator,
                                      archiveHandle,
                                      archiveHandle->storageInfo->jobOptions->cryptPasswordMode,
                                      &archiveHandle->storageInfo->jobOptions->cryptPassword,
                                      CALLBACK(archiveHandle->getNamePasswordFunction,archiveHandle->getNamePasswordUserData),
                                      archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                      &archiveHandle->archiveCryptInfo->cryptSalt,
                                      keyLength
                                     );
    }
    passwordFlag  = (decryptKey != NULL);
    decryptedFlag = FALSE;
  }
  else
  {
    decryptKey    = NULL;
    passwordFlag  = FALSE;
    decryptedFlag = TRUE;
  }
  foundSpecialEntryFlag = FALSE;
  error                 = ERROR_NONE;
  while (   !foundSpecialEntryFlag
         && (error == ERROR_NONE)
        )
  {
    // reset
    AutoFree_freeAll(&autoFreeList2);
    error = Chunk_seek(&archiveEntryInfo->special.chunkSpecial.info,index);

    // check decrypt key (if encrypted)
//TODO: multi-crypt
    if (   Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0])
        && (decryptKey == NULL)
       )
    {
      error = ERROR_NO_DECRYPT_KEY;
    }

    // init crypt
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
                        );
      if (error == ERROR_NONE)
      {
        AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo,{ Crypt_done(&archiveEntryInfo->special.chunkSpecialEntry.cryptInfo); });
      }
    }
    if (error == ERROR_NONE)
    {
      error = Crypt_init(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.cryptInfo,
                         archiveEntryInfo->cryptAlgorithms[0],
                         archiveHandle->archiveCryptInfo->cryptMode|CRYPT_MODE_CBC,
                         &archiveHandle->archiveCryptInfo->cryptSalt,
                         decryptKey
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
                               subChunkHeader.size,
                               archiveHandle
                              );
            if (error != ERROR_NONE)
            {
              break;
            }
            AUTOFREE_ADD(&autoFreeList2,&archiveEntryInfo->special.chunkSpecialEntry.info,{ Chunk_close(&archiveEntryInfo->special.chunkSpecialEntry.info); });

            // get special meta data
            String_set(specialName,archiveEntryInfo->special.chunkSpecialEntry.name);
            if (fileInfo != NULL)
            {
              memClear(fileInfo,sizeof(FileInfo));
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
            if (fileExtendedAttributeList != NULL)
            {
              // read special extended attribute chunk
              error = Chunk_open(&archiveEntryInfo->special.chunkSpecialExtendedAttribute.info,
                                 &subChunkHeader,
                                 subChunkHeader.size,
                                 archiveHandle
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
      if (   Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0])
          && (archiveHandle->archiveCryptInfo->cryptType != CRYPT_TYPE_ASYMMETRIC)
         )
      {
        // get next decrypt key
        decryptKey = getNextDecryptKey(&decryptKeyIterator,
                                       archiveHandle->archiveCryptInfo->cryptKeyDeriveType,
                                       &archiveHandle->archiveCryptInfo->cryptSalt,
                                       keyLength
                                      );
      }
      else
      {
        // no more decrypt keys when no encryption or asymmetric encryption is used
        decryptKey = NULL;
      }
    }
  }
  while ((error != ERROR_NONE) && (decryptKey != NULL));
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
    if      (error != ERROR_NONE)                                                      return error;
    else if (Crypt_isEncrypted(archiveEntryInfo->cryptAlgorithms[0]) && !passwordFlag) return ERROR_NO_CRYPT_PASSWORD;
    else if (!decryptedFlag)                                                           return ERROR_INVALID_CRYPT_PASSWORD;
    else if (!foundSpecialEntryFlag)                                                   return ERROR_NO_SPECIAL_ENTRY;
    HALT_INTERNAL_ERROR_UNREACHABLE();
  }

  if (cryptAlgorithm != NULL) (*cryptAlgorithm) = archiveEntryInfo->cryptAlgorithms[0];
  if (cryptType      != NULL) (*cryptType)      = archiveHandle->archiveCryptInfo->cryptType;

  // done resources
  AutoFree_done(&autoFreeList2);
  AutoFree_done(&autoFreeList1);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(archiveEntryInfo,ArchiveEntryInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,ArchiveEntryInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

Errors Archive_verifySignatureEntry(ArchiveHandle        *archiveHandle,
                                    uint64               offset,
                                    CryptSignatureStates *cryptSignatureState
                                   )
{
  AutoFreeList        autoFreeList;
  Errors              error;
  ChunkHeader         chunkHeader;
  ChunkSignature      chunkSignature;
  CryptHashAlgorithms cryptHashAlgorithm;
  byte                hash[MAX_HASH_SIZE];
  uint                hashLength;
  CryptHash           signatureHash;

  assert(archiveHandle != NULL);
  assert(archiveHandle->storageInfo != NULL);

  // init variables
  if (cryptSignatureState != NULL) (*cryptSignatureState) = CRYPT_SIGNATURE_STATE_NONE;

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList);

  // find next signature chunk
  do
  {
    error = getNextChunkHeader(archiveHandle,&chunkHeader);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    if (chunkHeader.id != CHUNK_ID_SIGNATURE)
    {
      error = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_SIGNATURE);

  // init signature chunk
  error = Chunk_init(&chunkSignature.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_SIGNATURE,
                     CHUNK_DEFINITION_SIGNATURE,
                     DEFAULT_ALIGNMENT,
                     NULL,  // cryptInfo
                     &chunkSignature
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&chunkSignature.info,{ Chunk_done(&chunkSignature.info); });

  // read signature chunk
  error = Chunk_open(&chunkSignature.info,
                     &chunkHeader,
                     chunkHeader.size,
                     archiveHandle
                    );
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // get and check hash algorithms
  if (!Crypt_isValidHashAlgorithm(chunkSignature.hashAlgorithm))
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    Chunk_close(&chunkSignature.info);
    AutoFree_cleanup(&autoFreeList);
    return ERROR_INVALID_HASH_ALGORITHM;
  }
  cryptHashAlgorithm = CRYPT_CONSTANT_TO_HASH_ALGORITHM(chunkSignature.hashAlgorithm);
  if (cryptHashAlgorithm != SIGNATURE_HASH_ALGORITHM)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    Chunk_close(&chunkSignature.info);
    AutoFree_cleanup(&autoFreeList);
    return ERROR_UNKNOWN_HASH_ALGORITHM;
  }

  // init signature hash
  error = Crypt_initHash(&signatureHash,SIGNATURE_HASH_ALGORITHM);
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    Chunk_close(&chunkSignature.info);
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&signatureHash,{ Crypt_doneHash(&signatureHash); });

  // get signature hash
  error = calcuateHash(archiveHandle->chunkIO,
                       archiveHandle->chunkIOUserData,
                       &signatureHash,
                       offset,
                       chunkSignature.info.offset
                      );
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    Crypt_doneHash(&signatureHash);
    Chunk_close(&chunkSignature.info);
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  (void)Crypt_getHash(&signatureHash,hash,sizeof(hash),&hashLength);

  // done signature hash
  AUTOFREE_REMOVE(&autoFreeList,&signatureHash);
  Crypt_doneHash(&signatureHash);

  // verify signature
//fprintf(stderr,"%s, %d: hash %d\n",__FILE__,__LINE__,hashLength); debugDumpMemory(hash,hashLength,0);
//fprintf(stderr,"%s, %d: signature %d\n",__FILE__,__LINE__,chunkSignature.value.length); debugDumpMemory(chunkSignature.value.data,chunkSignature.value.length,0);
  error = Crypt_verifySignature(&globalOptions.signaturePublicKey,
                                hash,
                                hashLength,
                                chunkSignature.value.data,
                                chunkSignature.value.length,
                                cryptSignatureState
                               );
  if (error != ERROR_NONE)
  {
    archiveHandle->pendingError = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
    Chunk_close(&chunkSignature.info);
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // close chunk
  Chunk_close(&chunkSignature.info);

  // free resources
  Chunk_done(&chunkSignature.info);
  AutoFree_done(&autoFreeList);

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
  Errors error,tmpError;
  bool   eofDelta;
  ulong  deltaLength;

  assert(archiveEntryInfo != NULL);
  assert(archiveEntryInfo->archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveEntryInfo->archiveHandle);
  assert(archiveEntryInfo->archiveHandle->storageInfo != NULL);
  assert(archiveEntryInfo->archiveHandle->storageInfo->jobOptions != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(archiveEntryInfo,ArchiveEntryInfo);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,archiveEntryInfo,ArchiveEntryInfo);
  #endif /* NDEBUG */

  error = ERROR_NONE;
  switch (archiveEntryInfo->archiveHandle->mode)
  {
    case ARCHIVE_MODE_CREATE:
      switch (archiveEntryInfo->archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          {
            if (!archiveEntryInfo->archiveHandle->dryRun)
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
                // update fragment size
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
              SEMAPHORE_LOCKED_DO(&archiveEntryInfo->archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
              {
                // create archive file (if not already exists and open)
                tmpError = createArchiveFile(archiveEntryInfo->archiveHandle);
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
                  break;
                }

                // transfer intermediate data into archive
                tmpError = transferToArchive(archiveEntryInfo->archiveHandle,
                                             &archiveEntryInfo->file.intermediateFileHandle
                                            );
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
                  break;
                }
              }

              // store in index database
              if (error == ERROR_NONE)
              {
                if (archiveEntryInfo->archiveHandle->indexHandle != NULL)
                {
                  error = indexAddFile(archiveEntryInfo->archiveHandle,
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
              assert(Compress_isCompressed(archiveEntryInfo->file.deltaCompressAlgorithm));

              DeltaSource_closeEntry(&archiveEntryInfo->file.deltaSourceHandle);
            }

            free(archiveEntryInfo->file.deltaBuffer);
            free(archiveEntryInfo->file.byteBuffer);

            (void)File_close(&archiveEntryInfo->file.intermediateFileHandle);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            if (!archiveEntryInfo->archiveHandle->dryRun)
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
              SEMAPHORE_LOCKED_DO(&archiveEntryInfo->archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
              {
                // create archive file (if not already exists and open)
                tmpError = createArchiveFile(archiveEntryInfo->archiveHandle);
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
                  break;
                }

                // transfer intermediate data into archive
                tmpError = transferToArchive(archiveEntryInfo->archiveHandle,
                                             &archiveEntryInfo->image.intermediateFileHandle
                                            );
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
                  break;
                }
              }

              // store in index database
              if (error == ERROR_NONE)
              {
                if (archiveEntryInfo->archiveHandle->indexHandle != NULL)
                {
                  error = indexAddImage(archiveEntryInfo->archiveHandle,
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
            if (Compress_isCompressed(archiveEntryInfo->image.deltaCompressAlgorithm))
            {
               assert(Compress_isDeltaCompressed(archiveEntryInfo->image.deltaCompressAlgorithm));
               Chunk_done(&archiveEntryInfo->image.chunkImageDelta.info);
            }
            Chunk_done(&archiveEntryInfo->image.chunkImageEntry.info);

            Crypt_done(&archiveEntryInfo->image.cryptInfo);
            Crypt_done(&archiveEntryInfo->image.chunkImageData.cryptInfo);
            Crypt_done(&archiveEntryInfo->image.chunkImageDelta.cryptInfo);
            Crypt_done(&archiveEntryInfo->image.chunkImageEntry.cryptInfo);

            Chunk_done(&archiveEntryInfo->image.chunkImage.info);

            if (archiveEntryInfo->image.deltaSourceHandleInitFlag)
            {
              assert(Compress_isCompressed(archiveEntryInfo->image.deltaCompressAlgorithm));

              DeltaSource_closeEntry(&archiveEntryInfo->image.deltaSourceHandle);
            }

            free(archiveEntryInfo->image.deltaBuffer);
            free(archiveEntryInfo->image.byteBuffer);

            File_close(&archiveEntryInfo->image.intermediateFileHandle);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          {
            if (!archiveEntryInfo->archiveHandle->dryRun)
            {
              // close chunks
              tmpError = Chunk_close(&archiveEntryInfo->directory.chunkDirectoryEntry.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              tmpError = Chunk_close(&archiveEntryInfo->directory.chunkDirectory.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

              // unlock archive
              Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);

              // store in index database
              if (error == ERROR_NONE)
              {
                if (archiveEntryInfo->archiveHandle->indexHandle != NULL)
                {
                  error = indexAddDirectory(archiveEntryInfo->archiveHandle,
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
            if (!archiveEntryInfo->archiveHandle->dryRun)
            {
              // close chunks
              tmpError = Chunk_close(&archiveEntryInfo->link.chunkLinkEntry.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              tmpError = Chunk_close(&archiveEntryInfo->link.chunkLink.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

              // unlock archive
              Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);

              // store in index database
              if (error == ERROR_NONE)
              {
                if (archiveEntryInfo->archiveHandle->indexHandle != NULL)
                {
                  error = indexAddLink(archiveEntryInfo->archiveHandle,
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

            if (!archiveEntryInfo->archiveHandle->dryRun)
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
                // update fragment size
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
              SEMAPHORE_LOCKED_DO(&archiveEntryInfo->archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
              {
                // create archive file (if not already exists and open)
                tmpError = createArchiveFile(archiveEntryInfo->archiveHandle);
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
                  break;
                }

                // transfer intermediate data into archive
                tmpError = transferToArchive(archiveEntryInfo->archiveHandle,
                                             &archiveEntryInfo->hardLink.intermediateFileHandle
                                            );
                if (tmpError != ERROR_NONE)
                {
                  if (error == ERROR_NONE) error = tmpError;
                  Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);
                  break;
                }
              }

              // store in index database
              if (error == ERROR_NONE)
              {
                if (archiveEntryInfo->archiveHandle->indexHandle != NULL)
                {
                  STRINGLIST_ITERATE(archiveEntryInfo->hardLink.fileNameList,stringNode,fileName)
                  {
                    error = indexAddFile(archiveEntryInfo->archiveHandle,
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
              assert(Compress_isDeltaCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm));

              DeltaSource_closeEntry(&archiveEntryInfo->hardLink.deltaSourceHandle);
            }

            free(archiveEntryInfo->hardLink.deltaBuffer);
            free(archiveEntryInfo->hardLink.byteBuffer);

            (void)File_close(&archiveEntryInfo->hardLink.intermediateFileHandle);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          {
            if (!archiveEntryInfo->archiveHandle->dryRun)
            {
              // close chunks
              tmpError = Chunk_close(&archiveEntryInfo->special.chunkSpecialEntry.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              tmpError = Chunk_close(&archiveEntryInfo->special.chunkSpecial.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

              // unlock archive
              Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);

              // store in index database
              if (error == ERROR_NONE)
              {
                if (archiveEntryInfo->archiveHandle->indexHandle != NULL)
                {
                  error = indexAddSpecial(archiveEntryInfo->archiveHandle,
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
        case ARCHIVE_ENTRY_TYPE_META:
          {
            if (!archiveEntryInfo->archiveHandle->dryRun)
            {
              // close chunks
              tmpError = Chunk_close(&archiveEntryInfo->meta.chunkMetaEntry.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
              tmpError = Chunk_close(&archiveEntryInfo->meta.chunkMeta.info);
              if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

              // unlock archive
              Semaphore_unlock(&archiveEntryInfo->archiveHandle->lock);

              // store in index database
              if (error == ERROR_NONE)
              {
                if (archiveEntryInfo->archiveHandle->indexHandle != NULL)
                {
                  error = indexAddMeta(archiveEntryInfo->archiveHandle,
                                       archiveEntryInfo->archiveHandle->storageId,
                                       archiveEntryInfo->meta.chunkMetaEntry.userName,
                                       archiveEntryInfo->meta.chunkMetaEntry.hostName,
                                       archiveEntryInfo->meta.chunkMetaEntry.jobUUID,
                                       archiveEntryInfo->meta.chunkMetaEntry.scheduleUUID,
                                       archiveEntryInfo->meta.chunkMetaEntry.archiveType,
                                       archiveEntryInfo->meta.chunkMetaEntry.createdDateTime,
                                       archiveEntryInfo->meta.chunkMetaEntry.comment
                                      );
                }
              }
            }

            // free resources
            Chunk_done(&archiveEntryInfo->meta.chunkMetaEntry.info);

            Crypt_done(&archiveEntryInfo->meta.chunkMetaEntry.cryptInfo);

            Chunk_done(&archiveEntryInfo->meta.chunkMeta.info);
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
              assert(Compress_isCompressed(archiveEntryInfo->file.deltaCompressAlgorithm));

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
              assert(Compress_isCompressed(archiveEntryInfo->image.deltaCompressAlgorithm));

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
              assert(Compress_isCompressed(archiveEntryInfo->hardLink.deltaCompressAlgorithm));

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
        case ARCHIVE_ENTRY_TYPE_META:
          {
            // close chunks
            tmpError = Chunk_close(&archiveEntryInfo->meta.chunkMetaEntry.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;
            tmpError = Chunk_close(&archiveEntryInfo->meta.chunkMeta.info);
            if ((error == ERROR_NONE) && (tmpError != ERROR_NONE)) error = tmpError;

            // free resources
            Chunk_done(&archiveEntryInfo->meta.chunkMetaEntry.info);

            Crypt_done(&archiveEntryInfo->meta.chunkMetaEntry.cryptInfo);

            Chunk_done(&archiveEntryInfo->meta.chunkMeta.info);
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

  archiveEntryInfo->archiveHandle->entries++;

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
  DEBUG_CHECK_RESOURCE_TRACE(archiveEntryInfo->archiveHandle);
  assert(archiveEntryInfo->archiveHandle->storageInfo != NULL);
  assert(archiveEntryInfo->archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveEntryInfo->archiveHandle->mode == ARCHIVE_MODE_CREATE);
  assert(elementSize > 0);

  if (!archiveEntryInfo->archiveHandle->dryRun)
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
  assert(archiveEntryInfo->archiveHandle->storageInfo != NULL);
  assert(archiveEntryInfo->archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveEntryInfo->archiveHandle->mode == ARCHIVE_MODE_READ);

  switch (archiveEntryInfo->archiveEntryType)
  {
    case ARCHIVE_ENTRY_TYPE_FILE:
      if (   Compress_isCompressed(archiveEntryInfo->file.deltaCompressAlgorithm)
          && !archiveEntryInfo->file.deltaSourceHandleInitFlag
         )
      {
        // get source for delta-compression
        error = DeltaSource_openEntry(&archiveEntryInfo->file.deltaSourceHandle,
                                      archiveEntryInfo->archiveHandle->deltaSourceList,
                                      archiveEntryInfo->file.chunkFileDelta.name,
                                      archiveEntryInfo->file.chunkFileEntry.name,
                                      archiveEntryInfo->file.chunkFileDelta.size,
                                      archiveEntryInfo->archiveHandle->storageInfo->jobOptions
                                     );
        if (error != ERROR_NONE)
        {
          return error;
        }
        archiveEntryInfo->file.deltaSourceHandleInitFlag = TRUE;

        // set delta base-offset
        DeltaSource_setBaseOffset(&archiveEntryInfo->file.deltaSourceHandle,
                             archiveEntryInfo->file.chunkFileData.fragmentOffset
                            );
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
                return ERRORX_(INFLATE_FAIL,0,"not data");
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
        // get source for delta-compression
        error = DeltaSource_openEntry(&archiveEntryInfo->image.deltaSourceHandle,
                                      archiveEntryInfo->archiveHandle->deltaSourceList,
                                      archiveEntryInfo->image.chunkImageDelta.name,
                                      archiveEntryInfo->image.chunkImageEntry.name,
                                      archiveEntryInfo->image.chunkImageDelta.size,
                                      archiveEntryInfo->archiveHandle->storageInfo->jobOptions
                                     );
        if (error != ERROR_NONE)
        {
          return error;
        }
        archiveEntryInfo->image.deltaSourceHandleInitFlag = TRUE;

        // set delta base-offset
        DeltaSource_setBaseOffset(&archiveEntryInfo->image.deltaSourceHandle,
                             archiveEntryInfo->image.chunkImageData.blockOffset*(uint64)archiveEntryInfo->image.blockSize
                            );

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
                return ERRORX_(INFLATE_FAIL,0,"no data");
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
                                      archiveEntryInfo->archiveHandle->storageInfo->jobOptions
                                     );
        if (error != ERROR_NONE)
        {
          return error;
        }
        archiveEntryInfo->hardLink.deltaSourceHandleInitFlag = TRUE;

        // set delta base-offset
        DeltaSource_setBaseOffset(&archiveEntryInfo->hardLink.deltaSourceHandle,
                             archiveEntryInfo->hardLink.chunkHardLinkData.fragmentOffset
                            );
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
                return ERRORX_(INFLATE_FAIL,0,"no data");
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
  DEBUG_CHECK_RESOURCE_TRACE(archiveEntryInfo->archiveHandle);

  eofFlag = FALSE;
  switch (archiveEntryInfo->archiveHandle->mode)
  {
    case ARCHIVE_MODE_CREATE:
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
        case ARCHIVE_ENTRY_TYPE_META:
          eofFlag = Chunk_eofSub(&archiveEntryInfo->meta.chunkMeta.info);
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
  uint64 offset;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->chunkIO != NULL);
  assert(archiveHandle->chunkIO->tell != NULL);

  offset = 0LL;
  switch (archiveHandle->mode)
  {
    case ARCHIVE_MODE_CREATE:
      SEMAPHORE_LOCKED_DO(&archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        if (archiveHandle->create.openFlag)
        {
          archiveHandle->chunkIO->tell(archiveHandle->chunkIOUserData,&offset);
        }
        else
        {
          offset = 0LL;
        }
      }
      break;
    case ARCHIVE_MODE_READ:
      if (archiveHandle->nextChunkHeaderReadFlag)
      {
        offset = archiveHandle->nextChunkHeader.offset;
      }
      else
      {
        archiveHandle->chunkIO->tell(archiveHandle->chunkIOUserData,&offset);
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        return 0LL; /* not reached */
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return offset;
}

Errors Archive_seek(ArchiveHandle *archiveHandle,
                    uint64        offset
                   )
{
  Errors error;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->chunkIO != NULL);
  assert(archiveHandle->chunkIO->seek != NULL);

  error = ERROR_NONE;

  switch (archiveHandle->mode)
  {
    case ARCHIVE_MODE_CREATE:
      SEMAPHORE_LOCKED_DO(&archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        if (archiveHandle->create.openFlag)
        {
          error = archiveHandle->chunkIO->seek(archiveHandle->chunkIOUserData,offset);
        }
      }
      break;
    case ARCHIVE_MODE_READ:
      error = archiveHandle->chunkIO->seek(archiveHandle->chunkIOUserData,offset);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  archiveHandle->nextChunkHeaderReadFlag = FALSE;

  return error;
}

uint64 Archive_getSize(ArchiveHandle *archiveHandle)
{
  uint64 size;

  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveHandle->storageInfo != NULL);
  assert(archiveHandle->storageInfo->jobOptions != NULL);
  assert(archiveHandle->chunkIO != NULL);
  assert(archiveHandle->chunkIO->getSize != NULL);

  size = 0LL;
  if (!archiveHandle->dryRun)
  {
    switch (archiveHandle->mode)
    {
      case ARCHIVE_MODE_CREATE:
        SEMAPHORE_LOCKED_DO(&archiveHandle->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          size = (archiveHandle->create.openFlag)
                   ? archiveHandle->chunkIO->getSize(archiveHandle->chunkIOUserData)
                   : 0LL;
        }
        break;
      case ARCHIVE_MODE_READ:
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

Errors Archive_verifySignatures(ArchiveHandle        *archiveHandle,
                                CryptSignatureStates *allCryptSignaturesState
                               )
{
  AutoFreeList         autoFreeList;
  Errors               error;
  uint64               offset;
  ChunkSignature       chunkSignature;
  ChunkHeader          chunkHeader;
  uint64               lastSignatureOffset;
  CryptHashAlgorithms  cryptHashAlgorithm;
  CryptHash            signatureHash;
  byte                 hash[MAX_HASH_SIZE];
  uint                 hashLength;
  CryptSignatureStates cryptSignatureState;

  assert(archiveHandle != NULL);

  // check for pending error
  if (archiveHandle->pendingError != ERROR_NONE)
  {
    error = archiveHandle->pendingError;
    archiveHandle->pendingError = ERROR_NONE;
    return error;
  }

  // init variables
  AutoFree_init(&autoFreeList);
  if (allCryptSignaturesState != NULL) (*allCryptSignaturesState) = CRYPT_SIGNATURE_STATE_NONE;

  // init signature chunk
  error = Chunk_init(&chunkSignature.info,
                     NULL,  // parentChunkInfo
                     archiveHandle->chunkIO,
                     archiveHandle->chunkIOUserData,
                     CHUNK_ID_SIGNATURE,
                     CHUNK_DEFINITION_SIGNATURE,
                     DEFAULT_ALIGNMENT,
                     NULL,  // cryptInfo
                     &chunkSignature
                    );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&chunkSignature.info,{ Chunk_done(&chunkSignature.info); });

  // check all signatures
  offset              = Archive_tell(archiveHandle);
  lastSignatureOffset = 0LL;
  while (   (error == ERROR_NONE)
         && (   (allCryptSignaturesState == NULL)
             || ((*allCryptSignaturesState) == CRYPT_SIGNATURE_STATE_NONE)
             || ((*allCryptSignaturesState) == CRYPT_SIGNATURE_STATE_OK  )
            )
         && !chunkHeaderEOF(archiveHandle)
        )
  {
    // get next chunk
    error = getNextChunkHeader(archiveHandle,&chunkHeader);
    if (error != ERROR_NONE)
    {
      break;
    }
//fprintf(stderr,"%s, %d: '%s'\n",__FILE__,__LINE__,chunkHeader.idChars);

    // check if signature
    if (chunkHeader.id == CHUNK_ID_SIGNATURE)
    {
      // check if signature public key is available
      if (!Crypt_isKeyAvailable(&globalOptions.signaturePublicKey))
      {
        error = ERROR_NO_PUBLIC_SIGNATURE_KEY;
        break;
      }

      // read signature chunk
      error = Chunk_open(&chunkSignature.info,
                         &chunkHeader,
                         chunkHeader.size,
                         NULL  // transformUserData
                        );
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }

      // get and check hash algorithms
      if (!Crypt_isValidHashAlgorithm(chunkSignature.hashAlgorithm))
      {
        Chunk_close(&chunkSignature.info);
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_HASH_ALGORITHM;
      }
      cryptHashAlgorithm = CRYPT_CONSTANT_TO_HASH_ALGORITHM(chunkSignature.hashAlgorithm);
      if (cryptHashAlgorithm != SIGNATURE_HASH_ALGORITHM)
      {
        Chunk_close(&chunkSignature.info);
        AutoFree_cleanup(&autoFreeList);
        return ERROR_UNKNOWN_HASH_ALGORITHM;
      }

      // init signature hash
      error = Crypt_initHash(&signatureHash,SIGNATURE_HASH_ALGORITHM);
      if (error != ERROR_NONE)
      {
        Chunk_close(&chunkSignature.info);
        AutoFree_cleanup(&autoFreeList);
        return error;
      }

      // get signature hash
//fprintf(stderr,"%s, %d: %llx .. %llx\n",__FILE__,__LINE__,lastSignatureOffset,chunkSignature.info.offset);
      error = calcuateHash(archiveHandle->chunkIO,
                           archiveHandle->chunkIOUserData,
                           &signatureHash,
                           lastSignatureOffset,
                           chunkSignature.info.offset
                          );
      if (error != ERROR_NONE)
      {
        Crypt_doneHash(&signatureHash);
        Chunk_close(&chunkSignature.info);
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      (void)Crypt_getHash(&signatureHash,hash,sizeof(hash),&hashLength);

      // done signature hash
      Crypt_doneHash(&signatureHash);

      // compare signatures
//fprintf(stderr,"%s, %d: hash %d\n",__FILE__,__LINE__,hashLength); debugDumpMemory(hash,hashLength,0);
//fprintf(stderr,"%s, %d: signature %d\n",__FILE__,__LINE__,chunkSignature.value.length); debugDumpMemory(chunkSignature.value.data,chunkSignature.value.length,0);
      error = Crypt_verifySignature(&globalOptions.signaturePublicKey,
                                    hash,
                                    hashLength,
                                    chunkSignature.value.data,
                                    chunkSignature.value.length,
                                    &cryptSignatureState
                                   );
      if (error != ERROR_NONE)
      {
        Chunk_close(&chunkSignature.info);
        AutoFree_cleanup(&autoFreeList);
        return error;
      }

      // close chunk
      Chunk_close(&chunkSignature.info);

      // get all signatures state
      switch (cryptSignatureState)
      {
        case CRYPT_SIGNATURE_STATE_NONE:
          // nothing to do
          break;
        case CRYPT_SIGNATURE_STATE_OK:
          if (allCryptSignaturesState != NULL)
          {
            if ((*allCryptSignaturesState) == CRYPT_SIGNATURE_STATE_NONE)
            {
              (*allCryptSignaturesState) = CRYPT_SIGNATURE_STATE_OK;
            }
          }
          break;
        case CRYPT_SIGNATURE_STATE_INVALID:
          if (allCryptSignaturesState != NULL)
          {
            (*allCryptSignaturesState) = CRYPT_SIGNATURE_STATE_INVALID;
          }
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break; /* not reached */
      }

      // get next signature offset
      lastSignatureOffset = Chunk_endOffset(&chunkSignature.info);
    }
    else
    {
      // skip to next chunk
      error = Chunk_skip(archiveHandle->chunkIO,archiveHandle->chunkIOUserData,&chunkHeader);
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }
  }
  (void)Archive_seek(archiveHandle,offset);
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // free resources
  Chunk_done(&chunkSignature.info);
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

Errors Archive_addToIndex(IndexHandle *indexHandle,
                          ConstString hostName,
                          StorageInfo *storageInfo,
                          IndexModes  indexMode,
                          uint64      *totalTimeLastChanged,
                          uint64      *totalEntries,
                          uint64      *totalSize,
                          LogHandle   *logHandle
                         )
{
  String  printableStorageName;
  Errors  error;
  IndexId storageId;

  assert(indexHandle != NULL);
  assert(storageInfo != NULL);

  // create new storage index
  printableStorageName = Storage_getPrintableName(String_new(),&storageInfo->storageSpecifier,NULL);
  error = Index_newStorage(indexHandle,
                           DATABASE_ID_NONE, // entityId
                           hostName,
                           printableStorageName,
                           0LL,  // created
                           0LL,  // size
                           INDEX_STATE_UPDATE,
                           indexMode,
                           &storageId
                          );
  if (error != ERROR_NONE)
  {
    String_delete(printableStorageName);
    return error;
  }
  String_delete(printableStorageName);

  // add index
  error = Archive_updateIndex(indexHandle,
                              storageId,
                              storageInfo,
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

//TODO: improve speed: read n entries, then transction with add
Errors Archive_updateIndex(IndexHandle       *indexHandle,
                           IndexId           storageId,
                           StorageInfo       *storageInfo,
                           uint64            *totalTimeLastChanged,
                           uint64            *totalEntries,
                           uint64            *totalSize,
                           IsPauseFunction   isPauseFunction,
                           void              *isPauseUserData,
                           IsAbortedFunction isAbortedFunction,
                           void              *isAbortedUserData,
                           LogHandle         *logHandle
                          )
{
  StorageSpecifier   storageSpecifier;
  String             printableStorageName;
  Errors             error;
  uint64             timeLastChanged;
  bool               abortedFlag,serverAllocationPendingFlag;
  ArchiveHandle      archiveHandle;
  ArchiveEntryInfo   archiveEntryInfo;
  ArchiveEntryTypes  archiveEntryType;
  String             fileName;
  String             imageName;
  String             directoryName;
  String             linkName;
  String             destinationName;
  StringList         fileNameList;
  StaticString       (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString       (scheduleUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes       archiveType;
  uint64             createdDateTime;

  assert(indexHandle != NULL);
  assert(storageInfo != NULL);

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  printableStorageName = String_new();

  // get printable name
  Storage_getPrintableName(printableStorageName,&storageInfo->storageSpecifier,NULL);

  // open archive (Note optimization: try sftp for scp protocol, because sftp support seek()-operation)
  if (storageSpecifier.type == STORAGE_TYPE_SCP)
  {
    // try to open scp-storage first with sftp
    storageSpecifier.type = STORAGE_TYPE_SFTP;
    error = Archive_open(&archiveHandle,
                         storageInfo,
                         NULL,  // archive name
                         NULL,  // deltaSourceList
                         CALLBACK(NULL,NULL),  // getNamePasswordFunction
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
                           CALLBACK(NULL,NULL),  // getNamePasswordFunction
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
                         CALLBACK(NULL,NULL),  // getNamePasswordFunction
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

  // get current uuidId, entityId
//TODO

  // clear index
  error = Index_clearStorage(indexHandle,
                             storageId
                            );
  if (error != ERROR_NONE)
  {
    printInfo(4,"Failed to create index for '%s' (error: %s)\n",String_cString(printableStorageName),Error_getText(error));

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
  abortedFlag                 = (isAbortedFunction != NULL) && isAbortedFunction(isAbortedUserData);
  serverAllocationPendingFlag = Storage_isServerAllocationPending(storageInfo);
  fileName                    = String_new();
  imageName                   = String_new();
  directoryName               = String_new();
  linkName                    = String_new();
  destinationName             = String_new();
  StringList_init(&fileNameList);
//uint64 t0,t1; t0 = Misc_getTimestamp();
  while (   !Archive_eof(&archiveHandle,ARCHIVE_FLAG_NONE)
         && (error == ERROR_NONE)
         && !abortedFlag
         && !serverAllocationPendingFlag
        )
  {
    // get next file type
    error = Archive_getNextArchiveEntry(&archiveHandle,
                                        &archiveEntryType,
                                        NULL,  // archiveCryptInfo
                                        NULL,  // offset
                                        ARCHIVE_FLAG_NONE
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

          // read file entry
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
          indexAddFile(&archiveHandle,
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

          // read image entry
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
          indexAddImage(&archiveHandle,
                        storageId,
                        imageName,
                        fileSystemType,
                        deviceInfo.size,
                        deviceInfo.blockSize,
                        blockOffset,
                        blockCount
                       );
          pprintInfo(4,"INDEX: ","Added image '%s', %lubytes to index for '%s'\n",String_cString(imageName),deviceInfo.size,String_cString(printableStorageName));

          // close archive file, free resources
          (void)Archive_closeEntry(&archiveEntryInfo);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        {
          FileInfo fileInfo;

          // read directory entry
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
          indexAddDirectory(&archiveHandle,
                            storageId,
                            directoryName,
                            fileInfo.timeLastAccess,
                            fileInfo.timeModified,
                            fileInfo.timeLastChanged,
                            fileInfo.userId,
                            fileInfo.groupId,
                            fileInfo.permission
                           );

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

          // read link entry
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
          indexAddLink(&archiveHandle,
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

          // save max. time last changed
          if (timeLastChanged < fileInfo.timeLastChanged) timeLastChanged = fileInfo.timeLastChanged;

          pprintInfo(4,"INDEX: ","Added link '%s' to index for '%s'\n",String_cString(linkName),String_cString(printableStorageName));

          // close archive file, free resources
          (void)Archive_closeEntry(&archiveEntryInfo);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        {
          ArchiveEntryInfo archiveEntryInfo;
          FileInfo         fileInfo;
          uint64           fragmentOffset,fragmentSize;
          StringNode       *stringNode;
          String           name;

          // read hard link entry
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
            break;
          }

          // add to index database
          STRINGLIST_ITERATE(&fileNameList,stringNode,name)
          {
            indexAddHardlink(&archiveHandle,
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
          }
          if (error != ERROR_NONE)
          {
            (void)Archive_closeEntry(&archiveEntryInfo);
            break;
          }

          // save max. time last changed
          if (timeLastChanged < fileInfo.timeLastChanged) timeLastChanged = fileInfo.timeLastChanged;

          pprintInfo(4,"INDEX: ","Added hardlink '%s', %lubytes to index for '%s'\n",String_cString(StringList_first(&fileNameList,NULL)),fileInfo.size,String_cString(printableStorageName));

          // close archive file, free resources
          (void)Archive_closeEntry(&archiveEntryInfo);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
        {
          FileInfo fileInfo;

          // read special entry
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
          indexAddSpecial(&archiveHandle,
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

          // save max. time last changed
          if (timeLastChanged < fileInfo.timeLastChanged) timeLastChanged = fileInfo.timeLastChanged;

          pprintInfo(4,"INDEX: ","Added special '%s' to index for '%s'\n",String_cString(fileName),String_cString(printableStorageName));

          // close archive file, free resources
          (void)Archive_closeEntry(&archiveEntryInfo);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_META:
        {
          // read meta entry
          error = Archive_readMetaEntry(&archiveEntryInfo,
                                        &archiveHandle,
                                        NULL,  // userName
                                        NULL,  // hostName
                                        jobUUID,
                                        scheduleUUID,
                                        &archiveType,
                                        &createdDateTime,
                                        NULL  // comment
                                       );
          if (error != ERROR_NONE)
          {
            break;
          }

          // add to index database
          indexAddMeta(&archiveHandle,
                       storageId,
                       NULL,  // userName
                       NULL,  // hostName
                       jobUUID,
                       scheduleUUID,
                       archiveType,
                       createdDateTime,
                       NULL  // comment
                      );

          pprintInfo(4,"INDEX: ","Added meta '%s' to index for '%s'\n",String_cString(jobUUID),String_cString(printableStorageName));

          // close archive file, free resources
          (void)Archive_closeEntry(&archiveEntryInfo);
        }
        break;
      case ARCHIVE_ENTRY_TYPE_SIGNATURE:
        error = Archive_skipNextEntry(&archiveHandle);
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

    // flush index
    error = flushArchiveIndexList(&archiveHandle,0);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // update temporary entries, size (ignore error)
    (void)Index_storageUpdate(indexHandle,
                              storageId,
                              NULL,  // storageName
                              Archive_getSize(&archiveHandle)
                             );

#if 1
    if ((isPauseFunction != NULL) && isPauseFunction(isPauseUserData))
    {
/*
      error = doneTransaction(&archiveHandle);
      if (error != ERROR_NONE)
      {
        break;
      }
*/

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
      while (isPauseFunction(isPauseUserData));

#if 0
      // reopen temporary closed storage
      error = Archive_storageContinue(&archiveHandle);
      if (error != ERROR_NONE)
      {
        break;
      }
#endif /* 0 */
    }
#endif

    // check if aborted, check if server allocation pending
    abortedFlag                 = (isAbortedFunction != NULL) && isAbortedFunction(isAbortedUserData);
    serverAllocationPendingFlag = Storage_isServerAllocationPending(storageInfo);
  }
  StringList_done(&fileNameList);
  String_delete(destinationName);
  String_delete(linkName);
  String_delete(directoryName);
  String_delete(imageName);
  String_delete(fileName);

/*
  error = doneTransaction(&archiveHandle);
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
  else */if (abortedFlag)
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

    // update entity, uuid
//TODO
//    Index_updateEntityInfos();
//    Index_updateUUIDInfos();

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

  #undef MAX_ARCHIVE_CONTENT_LIST_LENGTH
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
Errors Archive_copy(ConstString             storageName,
                    JobOptions              *jobOptions,
                    bool                    dryRun,
                    GetNamePasswordFunction getNamePasswordFunction,
                    void                    *getNamePasswordData,
                    ConstString             newStorageName
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
NULL,//                      ConstString         archiveName,
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
                                        NULL,  // cryptSalt
                                        NULL,  // cryptKey
                                        NULL,  // offset
                                        ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS
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
            if (!dryRun)
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
            if (!dryRun)
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
          if (!dryRun)
          {
            File_close(&fileHandle);
          }

          // close source archive file
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
            if (!dryRun)
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
              if (!dryRun)
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
            if (!dryRun)
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

            if (!dryRun)
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

          // close archive entry
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
            if (!dryRun)
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

            // set file time, file owner/group, file permission, file attributes
            if (!dryRun)
            {
              error = File_setInfo(&fileInfo,destinationFileName);
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

              if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
              if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
              error = File_setOwner(destinationFileName,fileInfo.userId,fileInfo.groupId);
              if (error != ERROR_NONE)
              {
                if (   !jobOptions->noStopOnAttributeErrorFlag
                    && !File_isNetworkFileSystem(fragmentNode->name)
                   )
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot set file owner/group of '%s' (error: %s)\n",
                             String_cString(fragmentNode->name),
                             Error_getText(error)
                            );
                  return error;
                }
                else
                {
                  printWarning("Cannot set file owner/group of '%s' (error: %s)\n",
                               String_cString(fragmentNode->name),
                               Error_getText(error)
                              );
                }
              }

              error = File_setAttributes(fileInfo.attributes,destinationFileName);
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

            if (!dryRun)
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

          // close archive entry
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
            if (!dryRun)
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
            if (!dryRun)
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

            // set file time, file owner/group, file permission, file attributes
            if (!dryRun)
            {
              error = File_setInfo(&fileInfo,destinationFileName);
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

              if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
              if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
              error = File_setOwner(destinationFileName,fileInfo.userId,fileInfo.groupId);
              if (error != ERROR_NONE)
              {
                if (   !jobOptions->noStopOnAttributeErrorFlag
                    && !File_isNetworkFileSystem(fragmentNode->name)
                   )
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot set file owner/group of '%s' (error: %s)\n",
                             String_cString(fragmentNode->name),
                             Error_getText(error)
                            );
                  return error;
                }
                else
                {
                  printWarning("Cannot set file owner/group of '%s' (error: %s)\n",
                               String_cString(fragmentNode->name),
                               Error_getText(error)
                              );
                }
              }

              error = File_setAttributes(fileInfo.attributes,destinationFileName);
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

            if (!dryRun)
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

          // close archive entry
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
            printError("Cannot read 'hardlink' content of archive '%s' (error: %s)!\n",
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
              if (!dryRun)
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
                if (!dryRun)
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
                  if (!dryRun)
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
                  if (!dryRun)
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
                if (!dryRun)
                {
                  if (File_getSize(&fileHandle) > fileInfo.size)
                  {
                    File_truncate(&fileHandle,fileInfo.size);
                  }
                }

                // close file
                if (!dryRun)
                {
                  File_close(&fileHandle);
                }

                // set file time, file owner/group, file permission, file attributes
                if (!dryRun)
                {
                  error = File_setInfo(&fileInfo,destinationFileName);
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

                  if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
                  if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
                  error = File_setOwner(destinationFileName,fileInfo.userId,fileInfo.groupId);
                  if (error != ERROR_NONE)
                  {
                    if (   !jobOptions->noStopOnAttributeErrorFlag
                        && !File_isNetworkFileSystem(fragmentNode->name)
                       )
                    {
                      printInfo(1,"FAIL!\n");
                      printError("Cannot set file owner/group of '%s' (error: %s)\n",
                                 String_cString(fragmentNode->name),
                                 Error_getText(error)
                                );
                      return error;
                    }
                    else
                    {
                      printWarning("Cannot set file owner/group of '%s' (error: %s)\n",
                                   String_cString(fragmentNode->name),
                                   Error_getText(error)
                                  );
                    }
                  }

                  error = File_setAttributes(fileInfo.attributes,destinationFileName);
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

                if (!dryRun)
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
                if (!dryRun)
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

                if (!dryRun)
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

          // close archive entry
          error = Archive_closeEntry(&archiveEntryInfo);
          if (error != ERROR_NONE)
          {
            printWarning("close 'hardlink' entry fail (error: %s)\n",Error_getText(error));
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
            if (!dryRun)
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
            if (!dryRun)
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

            // set file time, file owner/group, file permission, file attributes
            if (!dryRun)
            {
              error = File_setInfo(&fileInfo,destinationFileName);
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

              if (jobOptions->owner.userId  != FILE_DEFAULT_USER_ID ) fileInfo.userId  = jobOptions->owner.userId;
              if (jobOptions->owner.groupId != FILE_DEFAULT_GROUP_ID) fileInfo.groupId = jobOptions->owner.groupId;
              error = File_setOwner(destinationFileName,fileInfo.userId,fileInfo.groupId);
              if (error != ERROR_NONE)
              {
                if (   !jobOptions->noStopOnAttributeErrorFlag
                    && !File_isNetworkFileSystem(fragmentNode->name)
                   )
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot set file owner/group of '%s' (error: %s)\n",
                             String_cString(fragmentNode->name),
                             Error_getText(error)
                            );
                  return error;
                }
                else
                {
                  printWarning("Cannot set file owner/group of '%s' (error: %s)\n",
                               String_cString(fragmentNode->name),
                               Error_getText(error)
                              );
                }
              }

              error = File_setAttributes(fileInfo.attributes,destinationFileName);
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

            if (!dryRun)
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

          // close archive entry
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
