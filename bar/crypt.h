/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/crypt.h,v $
* $Revision: 1.14 $
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

#include "errors.h"
#include "passwords.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/* available chipers */
typedef enum
{
  CRYPT_ALGORITHM_NONE,

  CRYPT_ALGORITHM_3DES,
  CRYPT_ALGORITHM_CAST5,
  CRYPT_ALGORITHM_BLOWFISH,
  CRYPT_ALGORITHM_AES128,
  CRYPT_ALGORITHM_AES192,
  CRYPT_ALGORITHM_AES256,
  CRYPT_ALGORITHM_TWOFISH128,
  CRYPT_ALGORITHM_TWOFISH256,

  CRYPT_ALGORITHM_UNKNOWN=65535,
} CryptAlgorithms;

#define DEFAULT_CRYPT_KEY_BITS 2048

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
* Return : -
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

Errors Crypt_new(CryptInfo       *cryptInfo,
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

void Crypt_delete(CryptInfo *cryptInfo);

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
* Name   : PublicKey_delete
* Purpose: delete public/private key
* Input  : cryptKey - crypt key
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_deleteKey(CryptKey *cryptKey);

/***********************************************************************\
* Name   : Crypt_getKeyData
* Purpose: get public/private key data as string
* Input  : string   - string variable
*          cryptKey - crypt key
* Output : -
* Return : string with key data
* Notes  : -
\***********************************************************************/

String Crypt_getKeyData(String         string,
                        const CryptKey *cryptKey
                       );
/***********************************************************************\
* Name   : Crypt_setKeyData
* Purpose: set public/private key data from string
* Input  : cryptKey - crypt key
*          string   - string with key data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_setKeyData(CryptKey     *cryptKey,
                        const String string
                       );

/***********************************************************************\
* Name   : Crypt_keyEncrypt
* Purpose: encrypt with public key
* Input  : cryptKey                 - crypt key
*          buffer                   - buffer with data
*          bufferLength             - length of data
*          encryptedBuffer          - buffer for encrypted data
*          maxEncryptedBufferLength - max. length of encrypted data
* Output : encryptedBuffer       - encrypted data
*          encryptedBufferLength - length of encrypted data
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_keyEncrypt(CryptKey   *cryptKey,
                        const void *buffer,
                        ulong      bufferLength,
                        void       *encryptedBuffer,
                        ulong      maxEncryptedBufferLength,
                        ulong      *encryptedBufferLength
                       );

/***********************************************************************\
* Name   : Crypt_keyDecrypt
* Purpose: decrypt with private key
* Input  : cryptKey              - crypt key
*          encryptedBuffer       - encrypted data
*          encryptedBufferLength - length of encrypted data
*          buffer                - buffer for data
*          maxBufferLength       - max. length of buffer for data
* Output : buffer       - data
*          bufferLength - length of data
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_keyDecrypt(CryptKey   *cryptKey,
                        const void *encryptedBuffer,
                        ulong      encryptedBufferLength,
                        void       *buffer,
                        ulong      maxBufferLength,
                        ulong      *bufferLength
                       );

#ifdef __cplusplus
  }
#endif

#endif /* __CRYPT__ */

/* end of file */
