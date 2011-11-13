/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/crypt.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Backup ARchive crypt functions
* Systems: all
*
\***********************************************************************/

#ifndef __CRYPT__
#define __CRYPT__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_GCRYPT
  #include <gcrypt.h>
#endif /* HAVE_GCRYPT */
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "passwords.h"

#include "archive_format_const.h"
#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/* available chipers */
typedef enum
{
  CRYPT_ALGORITHM_NONE       = CHUNK_CONST_CRYPT_ALGORITHM_NONE,

  CRYPT_ALGORITHM_3DES       = CHUNK_CONST_CRYPT_ALGORITHM_3DES,
  CRYPT_ALGORITHM_CAST5      = CHUNK_CONST_CRYPT_ALGORITHM_CAST5,
  CRYPT_ALGORITHM_BLOWFISH   = CHUNK_CONST_CRYPT_ALGORITHM_BLOWFISH,
  CRYPT_ALGORITHM_AES128     = CHUNK_CONST_CRYPT_ALGORITHM_AES128,
  CRYPT_ALGORITHM_AES192     = CHUNK_CONST_CRYPT_ALGORITHM_AES192,
  CRYPT_ALGORITHM_AES256     = CHUNK_CONST_CRYPT_ALGORITHM_AES256,
  CRYPT_ALGORITHM_TWOFISH128 = CHUNK_CONST_CRYPT_ALGORITHM_TWOFISH128,
  CRYPT_ALGORITHM_TWOFISH256 = CHUNK_CONST_CRYPT_ALGORITHM_TWOFISH256,

  CRYPT_ALGORITHM_UNKNOWN = 0xFFFF,
} CryptAlgorithms;

#define MIN_ASYMMETRIC_CRYPT_KEY_BITS 1024
#define MAX_ASYMMETRIC_CRYPT_KEY_BITS 3072
#define DEFAULT_ASYMMETRIC_CRYPT_KEY_BITS 2048

typedef enum
{
  CRYPT_TYPE_NONE,

  CRYPT_TYPE_SYMMETRIC,
  CRYPT_TYPE_ASYMMETRIC,
} CryptTypes;

/***************************** Datatypes *******************************/

/* crypt info block */
typedef struct
{
  CryptAlgorithms  cryptAlgorithm;
  uint             blockLength;
  #ifdef HAVE_GCRYPT
    gcry_cipher_hd_t gcry_cipher_hd;
  #endif /* HAVE_GCRYPT */
} CryptInfo;

/* public/private key */
typedef struct
{
  #ifdef HAVE_GCRYPT
    gcry_sexp_t key;
  #endif /* HAVE_GCRYPT */
} CryptKey;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : CRYPT_CONSTANT_TO_ALGORITHM
* Purpose: convert archive definition constant to algorithm enum value
* Input  : n - number
* Output : -
* Return : crypt algorithm
* Notes  : -
\***********************************************************************/

#define CRYPT_CONSTANT_TO_ALGORITHM(n) \
  ((CryptAlgorithms)(n))

/***********************************************************************\
* Name   : CRYPT_ALGORITHM_TO_CONSTANT
* Purpose: convert algorithm enum value to archive definition constant
* Input  : cryptAlgorithm - crypt algorithm
* Output : -
* Return : number
* Notes  : -
\***********************************************************************/

#define CRYPT_ALGORITHM_TO_CONSTANT(cryptAlgorithm) \
  ((uint16)(cryptAlgorithm))

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Crypt_initAll
* Purpose: initialize crypt functions
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_initAll(void);

/***********************************************************************\
* Name   : Crypt_doneAll
* Purpose: deinitialize crypt functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_doneAll(void);

/***********************************************************************\
* Name   : Crypt_getAlgorithmName
* Purpose: get name of crypt algorithm
* Input  : cryptAlgorithm - crypt algorithm
* Output : -
* Return : algorithm name
* Notes  : -
\***********************************************************************/

const char *Crypt_getAlgorithmName(CryptAlgorithms cryptAlgorithm);

/***********************************************************************\
* Name   : Crypt_getAlgorithm
* Purpose: get crypt algorithm
* Input  : name - name of crypt algorithm
* Output : -
* Return : crypt algorithm
* Notes  : -
\***********************************************************************/

CryptAlgorithms Crypt_getAlgorithm(const char *name);

/***********************************************************************\
* Name   : Crypt_getTypeName
* Purpose: get name of crypt type
* Input  : cryptType - crypt type
* Output : -
* Return : mode string
* Notes  : -
\***********************************************************************/

const char *Crypt_getTypeName(CryptTypes cryptType);

/***********************************************************************\
* Name   : Crypt_randomize
* Purpose: fill buffer with randomized data
* Input  : buffer - buffer to fill with randomized data
*          length - length of buffer
* Output : buffer - buffer with randomized data
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_randomize(byte *buffer, uint length);

/***********************************************************************\
* Name   : Crypt_getKeyLength
* Purpose: get key length of crypt algorithm
* Input  : -
* Output : keyLength - key length (bits)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_getKeyLength(CryptAlgorithms cryptAlgorithm,
                          uint            *keyLength
                         );

/***********************************************************************\
* Name   : Crypt_getBlockLength
* Purpose: get block length of crypt algorithm
* Input  : -
* Output : blockLength - block length
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_getBlockLength(CryptAlgorithms cryptAlgorithm,
                            uint            *blockLength
                           );

/***********************************************************************\
* Name   : Crypt_new
* Purpose: create new crypt handle
* Input  : cryptInfo      - crypt info block
*          cryptAlgorithm - crypt algorithm to use
*          password       - crypt password
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_init(CryptInfo       *cryptInfo,
                  CryptAlgorithms cryptAlgorithm,
                  const Password  *password
                 );

/***********************************************************************\
* Name   : Crypt_delete
* Purpose: delete crypt handle
* Input  : cryptInfo - crypt info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_done(CryptInfo *cryptInfo);

/***********************************************************************\
* Name   : Crypt_reset
* Purpose: reset crypt handle
* Input  : cryptInfo - crypt info block
*          seed      - seed value for initializing IV (use 0 if not
*                      needed)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_reset(CryptInfo *cryptInfo,
                   uint64    seed
                  );

/***********************************************************************\
* Name   : Crypt_encrypt
* Purpose: encrypt data
* Input  : cryptInfo    - crypt info block
*          buffer       - data
*          bufferLength - length of data
* Output : buffer - encrypted data
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_encrypt(CryptInfo *cryptInfo,
                     void      *buffer,
                     ulong      bufferLength
                    );

/***********************************************************************\
* Name   : Crypt_decrypt
* Purpose: decrypt data
* Input  : cryptInfo    - crypt info block
*          buffer       - encrypted data
*          bufferLength - length of data
* Output : buffer - data
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_decrypt(CryptInfo *cryptInfo,
                     void      *buffer,
                     ulong      bufferLength
                    );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Crypt_initKey
* Purpose: initialise public/private key
* Input  : cryptKey - crypt key
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_initKey(CryptKey *cryptKey);

/***********************************************************************\
* Name   : public/private
* Purpose: deinitialise public/private key
* Input  : cryptKey - crypt key
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_doneKey(CryptKey *cryptKey);

/***********************************************************************\
* Name   : PublicKey_new
* Purpose: create new public/private key
* Input  : -
* Output : -
* Return : crypt key or NULL iff insufficient memory
* Notes  : -
\***********************************************************************/

CryptKey *Crypt_newKey(void);

/***********************************************************************\
* Name   : Crypt_readKeyFile
* Purpose: read key from file
* Input  : fileName - file name
*          password - password for decryption key
* Output : cryptKey - crypt key
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_readKeyFile(CryptKey       *cryptKey,
                         const String   fileName,
                         const Password *password
                        );

/***********************************************************************\
* Name   : Crypt_writeKeyFile
* Purpose: write key to file
* Input  : cryptKey - crypt key
*          fileName - file name
*          password - password for encryption key
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_writeKeyFile(CryptKey       *cryptKey,
                          const String   fileName,
                          const Password *password
                         );

/***********************************************************************\
* Name   : Crypt_createKeys
* Purpose: create new public/private key pair
* Input  : bits - number of RSA key bits
* Output : publicKey  - public crypt key
*          privateKey - private crypt key
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_createKeys(CryptKey *publicKey,
                        CryptKey *privateKey,
                        uint     bits
                       );

/***********************************************************************\
* Name   : Crypt_keyEncrypt
* Purpose: encrypt with public key
* Input  : cryptKey               - crypt key
*          buffer                 - buffer with data
*          bufferLength           - length of data
*          encryptBuffer          - buffer for encrypted data
*          maxEncryptBufferLength - max. length of encrypted data
* Output : encryptBuffer       - encrypted data (allocated)
*          encryptBufferLength - length of encrypted data
* Return : ERROR_NONE or error code
* Notes  : if encryptBufferLength==maxEncryptBufferLength buffer was to
*          small!
\***********************************************************************/

Errors Crypt_keyEncrypt(CryptKey   *cryptKey,
                        const void *buffer,
                        ulong      bufferLength,
                        ulong      maxEncryptBufferLength,
                        void       *encryptBuffer,
                        ulong      *encryptBufferLength
                       );

/***********************************************************************\
* Name   : Crypt_keyDecrypt
* Purpose: decrypt with private key
* Input  : cryptKey            - crypt key
*          encryptBuffer       - encrypted data
*          encryptBufferLength - length of encrypted data
*          buffer              - buffer for data
*          maxBufferLength     - max. length of buffer for data
* Output : buffer       - data (allocated memory)
*          bufferLength - length of data
* Return : ERROR_NONE or error code
* Notes  : if bufferLength==maxBufferLength buffer was to small!
\***********************************************************************/

Errors Crypt_keyDecrypt(CryptKey   *cryptKey,
                        const void *encryptBuffer,
                        ulong      encryptBufferLength,
                        ulong      maxBufferLength,
                        void       *buffer,
                        ulong      *bufferLength
                       );

/***********************************************************************\
* Name   : Crypt_getRandomEncryptKey
* Purpose: get random encryption key
* Input  : publicKey              - public key for encryption of random
*                                   key
*          cryptAlgorithm         - used symmetric crypt algorithm
*          encryptBuffer          - buffer for encrypted random key
*          maxEncryptBufferLength - max. length of encryption buffer
* Output : password            - created random password
*          encryptBuffer       - encrypted random key
*          encryptBufferLength - length of encrypted random key
* Return : ERROR_NONE or error code
* Notes  : if encryptBufferLength==maxEncryptBufferLength buffer was to
*          small!
\***********************************************************************/

Errors Crypt_getRandomEncryptKey(CryptKey        *publicKey,
                                 CryptAlgorithms cryptAlgorithm,
                                 Password        *password,
                                 uint            maxEncryptBufferLength,
                                 void            *encryptBuffer,
                                 uint            *encryptBufferLength
                                );

/***********************************************************************\
* Name   : Crypt_getDecryptKey
* Purpose: get decryption key
* Input  : privateKey        - private key to decrypt key
*          encryptData       - encrypted random key
*          encryptDataLength - length of encrypted random key
* Output : password - decryption password
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_getDecryptKey(CryptKey   *privateKey,
                           const void *encryptData,
                           uint       encryptDataLength,
                           Password   *password
                          );

                                 

#ifdef __cplusplus
  }
#endif

#endif /* __CRYPT__ */

/* end of file */
