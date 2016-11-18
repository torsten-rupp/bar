/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver crypt functions
* Systems: all
*
\***********************************************************************/

#ifndef __CRYPT__
#define __CRYPT__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_GCRYPT
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  #include <gcrypt.h>
  #pragma GCC diagnostic warning "-Wdeprecated-declarations"
#endif /* HAVE_GCRYPT */
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "passwords.h"

#include "archive_format_const.h"
#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// available chipers
typedef enum
{
  CRYPT_ALGORITHM_NONE        = CHUNK_CONST_CRYPT_ALGORITHM_NONE,

  CRYPT_ALGORITHM_3DES        = CHUNK_CONST_CRYPT_ALGORITHM_3DES,
  CRYPT_ALGORITHM_CAST5       = CHUNK_CONST_CRYPT_ALGORITHM_CAST5,
  CRYPT_ALGORITHM_BLOWFISH    = CHUNK_CONST_CRYPT_ALGORITHM_BLOWFISH,
  CRYPT_ALGORITHM_AES128      = CHUNK_CONST_CRYPT_ALGORITHM_AES128,
  CRYPT_ALGORITHM_AES192      = CHUNK_CONST_CRYPT_ALGORITHM_AES192,
  CRYPT_ALGORITHM_AES256      = CHUNK_CONST_CRYPT_ALGORITHM_AES256,
  CRYPT_ALGORITHM_TWOFISH128  = CHUNK_CONST_CRYPT_ALGORITHM_TWOFISH128,
  CRYPT_ALGORITHM_TWOFISH256  = CHUNK_CONST_CRYPT_ALGORITHM_TWOFISH256,
  CRYPT_ALGORITHM_SERPENT128  = CHUNK_CONST_CRYPT_ALGORITHM_SERPENT128,
  CRYPT_ALGORITHM_SERPENT192  = CHUNK_CONST_CRYPT_ALGORITHM_SERPENT192,
  CRYPT_ALGORITHM_SERPENT256  = CHUNK_CONST_CRYPT_ALGORITHM_SERPENT256,
  CRYPT_ALGORITHM_CAMELLIA128 = CHUNK_CONST_CRYPT_ALGORITHM_CAMELLIA128,
  CRYPT_ALGORITHM_CAMELLIA192 = CHUNK_CONST_CRYPT_ALGORITHM_CAMELLIA192,
  CRYPT_ALGORITHM_CAMELLIA256 = CHUNK_CONST_CRYPT_ALGORITHM_CAMELLIA256,

  CRYPT_ALGORITHM_UNKNOWN     = 0xFFFF
} CryptAlgorithms;

#define MIN_ASYMMETRIC_CRYPT_KEY_BITS 1024
#define MAX_ASYMMETRIC_CRYPT_KEY_BITS 3072
#define DEFAULT_ASYMMETRIC_CRYPT_KEY_BITS 2048

// available hash algorithms
typedef enum
{
  CRYPT_HASH_ALGORITHM_SHA2_224 = CHUNK_CONST_HASH_ALGORITHM_SHA2_224,
  CRYPT_HASH_ALGORITHM_SHA2_256 = CHUNK_CONST_HASH_ALGORITHM_SHA2_256,
  CRYPT_HASH_ALGORITHM_SHA2_384 = CHUNK_CONST_HASH_ALGORITHM_SHA2_384,
  CRYPT_HASH_ALGORITHM_SHA2_512 = CHUNK_CONST_HASH_ALGORITHM_SHA2_512,

  CRYPT_HASH_ALGORITHM_UNKNOW   = 0xFFFF
} CryptHashAlgorithms;

typedef enum
{
  CRYPT_TYPE_NONE,

  CRYPT_TYPE_SYMMETRIC,
  CRYPT_TYPE_ASYMMETRIC,
} CryptTypes;

typedef enum
{
  CRYPT_PADDING_TYPE_NONE,

  CRYPT_PADDING_TYPE_PKCS1,
  CRYPT_PADDING_TYPE_OAEP
} CryptPaddingTypes;

/***************************** Datatypes *******************************/

// crypt info block
typedef struct
{
  CryptAlgorithms  cryptAlgorithm;
  uint             blockLength;
  #ifdef HAVE_GCRYPT
    gcry_cipher_hd_t gcry_cipher_hd;
  #endif /* HAVE_GCRYPT */
} CryptInfo;

// public/private key
typedef struct
{
  #ifdef HAVE_GCRYPT
    gcry_sexp_t key;
  #endif /* HAVE_GCRYPT */
  CryptPaddingTypes cryptPaddingType;
} CryptKey;

// crypt hash info block
typedef struct
{
  CryptHashAlgorithms cryptHashAlgorithm;
  #ifdef HAVE_GCRYPT
    gcry_md_hd_t gcry_md_hd;
  #endif /* HAVE_GCRYPT */
} CryptHashInfo;

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

/***********************************************************************\
* Name   : CRYPT_CONSTANT_TO_HASH_ALGORITHM
* Purpose: convert archive definition constant to hash algorithm enum
*          value
* Input  : n - number
* Output : -
* Return : crypt hash algorithm
* Notes  : -
\***********************************************************************/

#define CRYPT_CONSTANT_TO_HASH_ALGORITHM(n) \
  ((CryptHashAlgorithms)(n))

/***********************************************************************\
* Name   : CRYPT_HASH_ALGORITHM_TO_CONSTANT
* Purpose: convert hash algorithm enum value to archive definition
*          constant
* Input  : cryptHashAlgorithm - crypt ash algorithm
* Output : -
* Return : number
* Notes  : -
\***********************************************************************/

#define CRYPT_HASH_ALGORITHM_TO_CONSTANT(cryptHashAlgorithm) \
  ((uint16)(cryptHashAlgorithm))

#ifndef NDEBUG
  #define Crypt_init(...)     __Crypt_init(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_done(...)     __Crypt_done(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_initHash(...) __Crypt_initHash(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_doneHash(...) __Crypt_doneHash(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

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
* Name   : Crypt_isSymmetricSupported
* Purpose: check if symmetric encryption is supported
* Input  : -
* Output : -
* Return : TRUE iff symmetric encryption is supported
* Notes  : -
\***********************************************************************/

bool Crypt_isSymmetricSupported(void);

/***********************************************************************\
* Name   : Crypt_isValidAlgorithm
* Purpose: check if valid crypt algoritm
* Input  : n - crypt algorithm constant
* Output : -
* Return : TRUE iff valid, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Crypt_isValidAlgorithm(uint16 n);

/***********************************************************************\
* Name   : Crypt_isValidHashAlgorithm
* Purpose: check if valid crypt hash algoritm
* Input  : n - crypt hash algorithm constant
* Output : -
* Return : TRUE iff valid, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Crypt_isValidHashAlgorithm(uint16 n);

/***********************************************************************\
* Name   : Crypt_algorithmToString
* Purpose: get name of crypt algorithm
* Input  : cryptAlgorithm - crypt algorithm
*          defaultValue   - default value
* Output : -
* Return : algorithm name
* Notes  : -
\***********************************************************************/

const char *Crypt_algorithmToString(CryptAlgorithms cryptAlgorithm, const char *defaultValue);

/***********************************************************************\
* Name   : Crypt_parseAlgorithm
* Purpose: parse crypt algorithm
* Input  : name - name of crypt algorithm
* Output : cryptAlgorithm - crypt algorithm
* Return : TRUE if parsed
* Notes  : -
\***********************************************************************/

bool Crypt_parseAlgorithm(const char *name, CryptAlgorithms *cryptAlgorithm);

/***********************************************************************\
* Name   : Crypt_hashAlgorithmToString
* Purpose: get name of crypt hash algorithm
* Input  : cryptHashAlgorithm - crypt hash algorithm
*          defaultValue       - default value
* Output : -
* Return : algorithm name
* Notes  : -
\***********************************************************************/

const char *Crypt_hashAlgorithmToString(CryptHashAlgorithms cryptHashAlgorithm, const char *defaultValue);

/***********************************************************************\
* Name   : Crypt_parseHashAlgorithm
* Purpose: parse crypt hash algorithm
* Input  : name - name of crypt hash algorithm
* Output : cryptHashAlgorithm - crypt hash algorithm
* Return : TRUE if parsed
* Notes  : -
\***********************************************************************/

bool Crypt_parseHashAlgorithm(const char *name, CryptHashAlgorithms *cryptHashAlgorithm);

/***********************************************************************\
* Name   : Crypt_typeToString
* Purpose: get name of crypt type
* Input  : cryptType    - crypt type
* Output : -
* Return : mode string
* Notes  : -
\***********************************************************************/

const char *Crypt_typeToString(CryptTypes cryptType);

/***********************************************************************\
* Name   : Crypt_isEncrypted
* Purpose: check if encrypted with some algorithm
* Input  : compressAlgorithm - compress algorithm
* Output : -
* Return : TRUE iff encrypted, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool Crypt_isEncrypted(CryptAlgorithms cryptAlgorithm);
#if defined(NDEBUG) || defined(__COMPRESS_IMPLEMENATION__)
INLINE bool Crypt_isEncrypted(CryptAlgorithms cryptAlgorithm)
{
  return cryptAlgorithm != CRYPT_ALGORITHM_NONE;
}
#endif /* NDEBUG || __COMPRESS_IMPLEMENATION__ */

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

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Crypt_new
* Purpose: create new crypt handle
* Input  : cryptInfo      - crypt info block
*          cryptAlgorithm - crypt algorithm to use
*          password       - crypt password
*          salt           - encryption salt (can be NULL)
*          saltLength     - encryption salt length
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Errors Crypt_init(CryptInfo       *cryptInfo,
                  CryptAlgorithms cryptAlgorithm,
                  const Password  *password,
                  const byte      *salt,
                  uint            saltLength
                 );
#else /* not NDEBUG */
Errors __Crypt_init(const char      *__fileName__,
                    ulong           __lineNb__,
                    CryptInfo       *cryptInfo,
                    CryptAlgorithms cryptAlgorithm,
                    const Password  *password,
                    const byte      *salt,
                    uint            saltLength
                   );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_delete
* Purpose: delete crypt handle
* Input  : cryptInfo - crypt info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void Crypt_done(CryptInfo *cryptInfo);
#else /* not NDEBUG */
void __Crypt_done(const char *__fileName__,
                  ulong      __lineNb__,
                  CryptInfo  *cryptInfo
                 );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_reset
* Purpose: reset crypt handle
* Input  : cryptInfo - crypt info block
*          seed      - seed value for initializing IV (use 0LL if not
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
* Purpose: encrypt data block
* Input  : cryptInfo    - crypt info block
*          buffer       - data
*          bufferLength - length of data (multiple of block length!)
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
* Purpose: decrypt data block
* Input  : cryptInfo    - crypt info block
*          buffer       - encrypted data
*          bufferLength - length of data (multiple of block length!)
* Output : buffer - data
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_decrypt(CryptInfo *cryptInfo,
                     void      *buffer,
                     ulong      bufferLength
                    );

/***********************************************************************\
* Name   : Crypt_encryptBytes
* Purpose: encrypt data bytes (without Cipher Text Stealing)
* Input  : cryptInfo    - crypt info block
*          buffer       - data
*          bufferLength - length of data (multiple of block length!)
* Output : buffer - encrypted data
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_encryptBytes(CryptInfo *cryptInfo,
                          void      *buffer,
                          ulong      bufferLength
                         );

/***********************************************************************\
* Name   : Crypt_decryptBytes
* Purpose: decrypt data bytes (without Cipher Text Stealing)
* Input  : cryptInfo    - crypt info block
*          buffer       - encrypted data
*          bufferLength - length of data (multiple of block length!)
* Output : buffer - data
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_decryptBytes(CryptInfo *cryptInfo,
                          void      *buffer,
                          ulong      bufferLength
                         );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Crypt_isAsymmetricSupported
* Purpose: check if asymmetric encryption is supported
* Input  : -
* Output : -
* Return : TRUE iff asymmetric encryption is supported
* Notes  : -
\***********************************************************************/

bool Crypt_isAsymmetricSupported(void);

/***********************************************************************\
* Name   : Crypt_initKey
* Purpose: initialize public/private key
* Input  : cryptKey         - crypt key
*          cryptPaddingType - padding type
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_initKey(CryptKey          *cryptKey,
                   CryptPaddingTypes cryptPaddingType
                  );

/***********************************************************************\
* Name   : public/private
* Purpose: deinitialize public/private key
* Input  : cryptKey - crypt key
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_doneKey(CryptKey *cryptKey);

/***********************************************************************\
* Name   : Crypt_getKeyData
* Purpose: get public/private key data as base64-encoded string
* Input  : cryptKey      - crypt key
*          keyData       - data variable
*          keyDataLength - data length variable
*          password      - password to encrypt key (can be NULL)
*          salt          - encryption salt (can be NULL)
*          saltLength    - encryption salt length
* Output : keyData       - data with encrypted key
*          keyDataLength - length of key data
* Return : ERROR_NONE or error code
* Notes  : keyData must be freed with Password_freeSecure()!
\***********************************************************************/

Errors Crypt_getKeyData(CryptKey       *cryptKey,
                        void           **keyData,
                        uint           *keyDataLength,
                        const Password *password,
                        const byte     *salt,
                        uint           saltLength
                       );

/***********************************************************************\
* Name   : Crypt_setKeyData
* Purpose: set public/private key data from base64-encoded string
* Input  : cryptKey      - crypt key
*          keyData       - data with encrypted key
*          keyDataLength - length of key data
*          password      - password to decrypt key (can be NULL)
*          salt          - encryption salt (can be NULL)
*          saltLength    - encryption salt length
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_setKeyData(CryptKey       *cryptKey,
                        const void     *keyData,
                        uint           keyDataLength,
                        const Password *password,
                        const byte     *salt,
                        uint           saltLength
                       );

/***********************************************************************\
* Name   : Crypt_getKeyString
* Purpose: get public/private key data as base64-encoded string
* Input  : cryptKey   - crypt key
*          string     - string variable
*          password   - password to encrypt key (can be NULL)
*          salt       - encryption salt (can be NULL)
*          saltLength - encryption salt length
* Output : string - string with encrypted key data
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_getKeyString(CryptKey       *cryptKey,
                          String         string,
                          const Password *password,
                          const byte     *salt,
                          uint           saltLength
                         );

/***********************************************************************\
* Name   : Crypt_setKeyString
* Purpose: set public/private key data from base64-encoded string
* Input  : cryptKey   - crypt key
*          string     - string with encrypted key data
*          password   - password to decrypt key (can be NULL)
*          salt       - encryption salt (can be NULL)
*          saltLength - encryption salt length
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_setKeyString(CryptKey       *cryptKey,
                          const String   string,
                          const Password *password,
                          const byte     *salt,
                          uint           saltLength
                         );

/***********************************************************************\
* Name   : Crypt_getKeyModulus, Crypt_getKeyExponent
* Purpose: get public/private key modulus/exponent as hex-string
* Input  : cryptKey - crypt key
*          string   - string variable
* Output : string - string with key modulus
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

String Crypt_getKeyModulus(CryptKey *cryptKey);
String Crypt_getKeyExponent(CryptKey *cryptKey);

/***********************************************************************\
* Name   : Crypt_readKeyFile
* Purpose: read key from file
* Input  : fileName   - file name
*          password   - password tor decrypt key (can be NULL)
*          salt       - encryption salt (can be NULL)
*          saltLength - encryption salt length
* Output : cryptKey - crypt key
* Return : ERROR_NONE or error code
* Notes  : use own specific file format
\***********************************************************************/

Errors Crypt_readKeyFile(CryptKey       *cryptKey,
                         const String   fileName,
                         const Password *password,
                         const byte     *salt,
                         uint           saltLength
                        );

/***********************************************************************\
* Name   : Crypt_writeKeyFile
* Purpose: write key to file
* Input  : cryptKey   - crypt key
*          fileName   - file name
*          password   - password to encrypt key (can be NULL)
*          salt       - encryption salt (can be NULL)
*          saltLength - encryption salt length
* Output : -
* Return : ERROR_NONE or error code
* Notes  : use own specific file format
\***********************************************************************/

Errors Crypt_writeKeyFile(CryptKey       *cryptKey,
                          const String   fileName,
                          const Password *password,
                          const byte     *salt,
                          uint           saltLength
                         );

/***********************************************************************\
* Name   : Crypt_createKeys
* Purpose: create new public/private key pair encryption/decryption
* Input  : bits - number of RSA key bits
* Output : publicCryptKey  - public crypt key (encryption or signature
*                            check)
*          privateCryptKey - private crypt key (decryption or signature
*                            generation)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_createKeys(CryptKey          *publicCryptKey,
                        CryptKey          *privateCryptKey,
                        uint              bits,
                        CryptPaddingTypes cryptPaddingType
                       );

/***********************************************************************\
* Name   : Crypt_keyEncrypt
* Purpose: encrypt data
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

Errors Crypt_keyEncrypt(const CryptKey *cryptKey,
                        const void     *buffer,
                        uint           bufferLength,
                        void           *encryptBuffer,
                        uint           *encryptBufferLength,
                        uint           maxEncryptBufferLength
                       );

/***********************************************************************\
* Name   : Crypt_keyDecrypt
* Purpose: decrypt data
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

Errors Crypt_keyDecrypt(const CryptKey *cryptKey,
                        const void     *encryptBuffer,
                        uint           encryptBufferLength,
                        void           *buffer,
                        uint           *bufferLength,
                        uint           maxBufferLength
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

/***********************************************************************\
* Name   : Crypt_dumpKey
* Purpose: dump key to stdout
* Input  : cryptKey - crypt key
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
void Crypt_dumpKey(const CryptKey *cryptKey);
#endif /* NDEBUG */

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Crypt_initHash
* Purpose: init hash
* Input  : cryptHash - crypt hash variable
* Output : cryptHash - crypt hash
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Errors Crypt_initHash(CryptHashInfo       *cryptHashInfo,
                      CryptHashAlgorithms cryptHashAlgorithm
                     );
#else /* not NDEBUG */
Errors __Crypt_initHash(const char          *__fileName__,
                        ulong               __lineNb__,
                        CryptHashInfo       *cryptHashInfo,
                        CryptHashAlgorithms cryptHashAlgorithm
                       );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_doneHash
* Purpose: done hash
* Input  : cryptHash - crypt hash
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void Crypt_doneHash(CryptHashInfo *cryptHashInfo);
#else /* not NDEBUG */
void __Crypt_doneHash(const char    *__fileName__,
                      ulong         __lineNb__,
                      CryptHashInfo *cryptHashInfo
                     );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_resetHash
* Purpose: reset hash
* Input  : cryptHash    - crypt hash
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_resetHash(CryptHashInfo *cryptHashInfo);

/***********************************************************************\
* Name   : Crypt_updateHash
* Purpose: update hash
* Input  : cryptHash    - crypt hash
*          buffer       - buffer with data
*          bufferLength - buffer length
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_updateHash(CryptHashInfo *cryptHashInfo,
                      void          *buffer,
                      ulong         bufferLength
                     );

/***********************************************************************\
* Name   : Crypt_getHashLength
* Purpose: get hash length
* Input  : cryptHash - crypt hash
* Output : -
* Return : hash length [bytes]
* Notes  : -
\***********************************************************************/

uint Crypt_getHashLength(const CryptHashInfo *cryptHashInfo);

/***********************************************************************\
* Name   : Crypt_getHash
* Purpose: get hash
* Input  : cryptHash  - crypt hash
*          buffer     - buffer for hash
*          bufferSize - buffer size
* Output : -
* Return : hash buffer
* Notes  : -
\***********************************************************************/

void *Crypt_getHash(const CryptHashInfo *cryptHashInfo, void *buffer, uint bufferSize);

/***********************************************************************\
* Name   : Crypt_equalsHash
* Purpose: compare with hash
* Input  : cryptHash  - crypt hash
*          buffer     - buffer with hash to compare with
*          bufferSize - buffer size
* Output : -
* Return : TRUE iff hash equals
* Notes  : -
\***********************************************************************/

bool Crypt_equalsHash(const const CryptHashInfo *cryptHashInfo, void *buffer, uint bufferSize);

#ifdef __cplusplus
  }
#endif

#endif /* __CRYPT__ */

/* end of file */
