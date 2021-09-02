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

#include "common/global.h"
#include "common/strings.h"
#include "common/passwords.h"

#include "archive_format_const.h"
#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// salt length
#define CRYPT_SALT_LENGTH CHUNK_CONST_SALT_LENGTH

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

// crypt types
typedef enum
{
  CRYPT_TYPE_NONE,

  CRYPT_TYPE_SYMMETRIC,
  CRYPT_TYPE_ASYMMETRIC,
} CryptTypes;

// crypt modes
#define CRYPT_MODE_KDF        0          // use key derivation function to generate password
//TODO: required? Can be default?
#define CRYPT_MODE_SIMPLE_KEY (1 << 0)   // use simple function to generate password
#define CRYPT_MODE_CBC_       (1 << 1)   // cipher block chaining (Note: name clash on Windows!)
#define CRYPT_MODE_CTS_       (1 << 2)   // cipher text stealing (Note: name clash on Windows!)
#define CRYPT_MODE_CTS_DATA   (1 << 3)   // cipher text stealing for data (Crypt_encryptBytes()/Crypt_decryptBytes)

#define CRYPT_MODE_NONE 0

// key derive types
typedef enum
{
  CRYPT_KEY_DERIVE_NONE,
  CRYPT_KEY_DERIVE_SIMPLE,
  CRYPT_KEY_DERIVE_FUNCTION
} CryptKeyDeriveTypes;

// available hash algorithms
typedef enum
{
  CRYPT_HASH_ALGORITHM_NONE     = CHUNK_CONST_HASH_ALGORITHM_NONE,

  CRYPT_HASH_ALGORITHM_SHA2_224 = CHUNK_CONST_HASH_ALGORITHM_SHA2_224,
  CRYPT_HASH_ALGORITHM_SHA2_256 = CHUNK_CONST_HASH_ALGORITHM_SHA2_256,
  CRYPT_HASH_ALGORITHM_SHA2_384 = CHUNK_CONST_HASH_ALGORITHM_SHA2_384,
  CRYPT_HASH_ALGORITHM_SHA2_512 = CHUNK_CONST_HASH_ALGORITHM_SHA2_512,

  CRYPT_HASH_ALGORITHM_UNKNOW   = 0xFFFF
} CryptHashAlgorithms;

// available message authentication code (MAC) algorithms
typedef enum
{
  CRYPT_MAC_ALGORITHM_NONE     = CHUNK_CONST_MAC_ALGORITHM_NONE,

  CRYPT_MAC_ALGORITHM_SHA2_224 = CHUNK_CONST_MAC_ALGORITHM_SHA2_224,
  CRYPT_MAC_ALGORITHM_SHA2_256 = CHUNK_CONST_MAC_ALGORITHM_SHA2_256,
  CRYPT_MAC_ALGORITHM_SHA2_384 = CHUNK_CONST_MAC_ALGORITHM_SHA2_384,
  CRYPT_MAC_ALGORITHM_SHA2_512 = CHUNK_CONST_MAC_ALGORITHM_SHA2_512,

  CRYPT_MAC_ALGORITHM_UNKNOW   = 0xFFFF
} CryptMACAlgorithms;

// crypt key padding types
typedef enum
{
  CRYPT_PADDING_TYPE_NONE,

  CRYPT_PADDING_TYPE_PKCS1,
  CRYPT_PADDING_TYPE_OAEP
} CryptPaddingTypes;

// crypy key modes
#define CRYPT_KEY_MODE_NONE 0
#define CRYPT_KEY_MODE_TRANSIENT (1 << 0)   // transient key (less secure)

// signatures states
typedef enum
{
  CRYPT_SIGNATURE_STATE_NONE,

  CRYPT_SIGNATURE_STATE_OK,
  CRYPT_SIGNATURE_STATE_INVALID,
  CRYPT_SIGNATURE_STATE_SKIPPED,

  CRYPT_SIGNATURE_STATE_UNKNOWN
} CryptSignatureStates;

/***************************** Datatypes *******************************/

// crypt mode set (see CRYPT_MODE_*)
typedef uint CryptMode;

// crypt salt
typedef struct
{
  byte data[CRYPT_SALT_LENGTH];
  uint length;
} CryptSalt;

// crypt info
typedef struct
{
  CryptAlgorithms  cryptAlgorithm;
  CryptMode        cryptMode;
  CryptSalt        cryptSalt;
  uint             blockLength;
  #ifdef HAVE_GCRYPT
    gcry_cipher_hd_t gcry_cipher_hd;
  #endif /* HAVE_GCRYPT */
} CryptInfo;

// public/private key
typedef struct
{
  CryptPaddingTypes cryptPaddingType;
  void              *data;              // data
  uint              dataLength;         // data length [bytes]
  #ifdef HAVE_GCRYPT
    gcry_sexp_t key;                    // public/private key
  #endif /* HAVE_GCRYPT */
} CryptKey;

// crypt hash info block
typedef struct
{
  CryptHashAlgorithms cryptHashAlgorithm;
  union
  {
    struct
    {
      void *data;
      uint dataLength;
    } none;
    #ifdef HAVE_GCRYPT
      gcry_md_hd_t gcry_md_hd;
    #endif /* HAVE_GCRYPT */
  };
} CryptHash;

// crypt message authentication code info block
typedef struct
{
  CryptMACAlgorithms cryptMACAlgorithm;
  #ifdef HAVE_GCRYPT
    gcry_mac_hd_t gcry_mac_hd;
  #endif /* HAVE_GCRYPT */
} CryptMAC;

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
* Input  : cryptHashAlgorithm - crypt hash algorithm
* Output : -
* Return : number
* Notes  : -
\***********************************************************************/

#define CRYPT_HASH_ALGORITHM_TO_CONSTANT(cryptHashAlgorithm) \
  ((uint16)(cryptHashAlgorithm))

/***********************************************************************\
* Name   : CRYPT_CONSTANT_TO_MAC_ALGORITHM
* Purpose: convert archive definition constant to MAC algorithm enum
*          value
* Input  : n - number
* Output : -
* Return : crypt MAC algorithm
* Notes  : -
\***********************************************************************/

#define CRYPT_CONSTANT_TO_MAC_ALGORITHM(n) \
  ((CryptMACAlgorithms)(n))

/***********************************************************************\
* Name   : CRYPT_MAC_ALGORITHM_TO_CONSTANT
* Purpose: convert MAC algorithm enum value to archive definition
*          constant
* Input  : cryptMACAlgorithm - crypt MAC algorithm
* Output : -
* Return : number
* Notes  : -
\***********************************************************************/

#define CRYPT_MAC_ALGORITHM_TO_CONSTANT(cryptMACAlgorithm) \
  ((uint16)(cryptMACAlgorithm))

#ifndef NDEBUG
  #define Crypt_initKey(...)      __Crypt_initKey     (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_duplicateKey(...) __Crypt_duplicateKey(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_doneKey(...)      __Crypt_doneKey     (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_newKey(...)       __Crypt_newKey      (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_deleteKey(...)    __Crypt_deleteKey   (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_init(...)         __Crypt_init        (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_done(...)         __Crypt_done        (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_new(...)          __Crypt_new         (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_delete(...)       __Crypt_delete      (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_initHash(...)     __Crypt_initHash    (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_doneHash(...)     __Crypt_doneHash    (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_newHash(...)      __Crypt_newHash     (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_deleteHash(...)   __Crypt_deleteHash  (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_initMAC(...)      __Crypt_initMAC     (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_doneMAC(...)      __Crypt_doneMAC     (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_newMAC(...)       __Crypt_newMAC      (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Crypt_deleteMAC(...)    __Crypt_deleteMAC   (__FILE__,__LINE__, ## __VA_ARGS__)
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

INLINE bool Crypt_isSymmetricSupported(void);
#if defined(NDEBUG) || defined(__CRYPT_IMPLEMENTATION__)
INLINE bool Crypt_isSymmetricSupported(void)
{
  #ifdef HAVE_GCRYPT
    return TRUE;
  #else /* not HAVE_GCRYPT */
    return FALSE;
  #endif /* HAVE_GCRYPT */
}
#endif /* NDEBUG || __CRYPT_IMPLEMENTATION__ */

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
* Name   : Crypt_isValidMACAlgorithm
* Purpose: check if valid crypt MAC algoritm
* Input  : n - crypt MAC algorithm constant
* Output : -
* Return : TRUE iff valid, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Crypt_isValidMACAlgorithm(uint16 n);

/***********************************************************************\
* Name   : Crypt_isValidSignatureState
* Purpose: check if valid signature state
* Input  : cryptSignatureState - signature state
* Output : -
* Return : TRUE iff valid, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool Crypt_isValidSignatureState(CryptSignatureStates cryptSignatureState);
#if defined(NDEBUG) || defined(__CRYPT_IMPLEMENTATION__)
INLINE bool Crypt_isValidSignatureState(CryptSignatureStates cryptSignatureState)
{
  return    (cryptSignatureState == CRYPT_SIGNATURE_STATE_NONE)
         || (cryptSignatureState == CRYPT_SIGNATURE_STATE_OK  );
}
#endif /* NDEBUG || __CRYPT_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Crypt_algorithmToString
* Purpose: get name of crypt algorithm
* Input  : cryptAlgorithm - crypt algorithm
*          defaultValue   - default value for not supported crypt
*                           algorithms
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
* Return : TRUE iff parsed
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
* Name   : Crypt_macAlgorithmToString
* Purpose: get name of crypt MAC algorithm
* Input  : cryptMACAlgorithm - crypt MAC algorithm
*          defaultValue      - default value
* Output : -
* Return : algorithm name
* Notes  : -
\***********************************************************************/

const char *Crypt_macAlgorithmToString(CryptMACAlgorithms cryptMACAlgorithm, const char *defaultValue);

/***********************************************************************\
* Name   : Crypt_parseMACAlgorithm
* Purpose: parse crypt MAC algorithm
* Input  : name - name of crypt MAC algorithm
* Output : cryptMACAlgorithm - crypt MAC algorithm
* Return : TRUE if parsed
* Notes  : -
\***********************************************************************/

bool Crypt_parseMACAlgorithm(const char *name, CryptMACAlgorithms *cryptMACAlgorithm);

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
#if defined(NDEBUG) || defined(__CRYPT_IMPLEMENTATION__)
INLINE bool Crypt_isEncrypted(CryptAlgorithms cryptAlgorithm)
{
  return cryptAlgorithm != CRYPT_ALGORITHM_NONE;
}
#endif /* NDEBUG || __CRYPT_IMPLEMENTATION__ */

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
* Output : keyLength - key length [bits]
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
* Name   : Crypt_initSalt
* Purpose: init crypt salt
* Input  : cryptSalt - crypt salt variable
* Output : -
* Return : cryptSalt
* Notes  : -
\***********************************************************************/

CryptSalt *Crypt_initSalt(CryptSalt *cryptSalt);

/***********************************************************************\
* Name   : Crypt_duplicateSalt
* Purpose: duplicate crypt salt
* Input  : cryptSalt     - crypt salt variable
*          fromCryptSalt - from crypt salt
* Output : -
* Return : crypt salt
* Notes  : -
\***********************************************************************/

CryptSalt *Crypt_duplicateSalt(CryptSalt *cryptSalt, const CryptSalt *fromCryptSalt);

/***********************************************************************\
* Name   : Crypt_doneSalt
* Purpose: done crypt salt
* Input  : cryptSalt - crypt salt
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_doneSalt(CryptSalt *cryptSalt);

/***********************************************************************\
* Name   : Crypt_getSalt
* Purpose: get crypt salt
* Input  : data      - buffer for salt data
*          cryptSalt - crypt salt
* Output : data   - salt data
*          length - salt length
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Crypt_getSalt(byte *data, uint length, const CryptSalt *cryptSalt);
#if defined(NDEBUG) || defined(__CRYPT_IMPLEMENTATION__)
INLINE void Crypt_getSalt(byte *data, uint length, const CryptSalt *cryptSalt)
{
  assert(data != NULL);
  assert(cryptSalt != NULL);

  memCopyFast(data,length,cryptSalt->data,cryptSalt->length);
}
#endif /* NDEBUG || __CRYPT_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Crypt_setSalt
* Purpose: set crypt salt
* Input  : cryptSalt - crypt salt
*          data      - salt data
*          length    - salt length
* Output : -
* Return : crypt salt
* Notes  : -
\***********************************************************************/

CryptSalt *Crypt_setSalt(CryptSalt *cryptSalt, const byte *data, uint length);

/***********************************************************************\
* Name   : Crypt_setSalt
* Purpose: set crypt salt
* Input  : cryptSalt - crypt salt
* Output : -
* Return : crypt salt
* Notes  : -
\***********************************************************************/

CryptSalt *Crypt_clearSalt(CryptSalt *cryptSalt);

/***********************************************************************\
* Name   : Crypt_copySalt
* Purpose: cioy crypt salt
* Input  : cryptSalt     - crypt salt
*          fromCryptSalt - from crypt salt
* Output : -
* Return : crypt salt
* Notes  : -
\***********************************************************************/

CryptSalt *Crypt_copySalt(CryptSalt *cryptSalt, const CryptSalt *fromCryptSalt);

/***********************************************************************\
* Name   : Crypt_randomSalt
* Purpose: get random crypt salt
* Input  : cryptSalt - crypt salt
* Output : -
* Return : crypt salt
* Notes  : -
\***********************************************************************/

CryptSalt *Crypt_randomSalt(CryptSalt *cryptSalt);

/***********************************************************************\
* Name   : Crypt_isSalt
* Purpose: check if crypt salt
* Input  : cryptSalt - crypt salt to check
* Output : -
* Return : TRUE iff valid salt
* Notes  : -
\***********************************************************************/

INLINE bool Crypt_isSalt(const CryptSalt *cryptSalt);
#if defined(NDEBUG) || defined(__CRYPT_IMPLEMENTATION__)
INLINE bool Crypt_isSalt(const CryptSalt *cryptSalt)
{
  return (cryptSalt != NULL) && (cryptSalt->length > 0);
}
#endif /* NDEBUG || __CRYPT_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Crypt_equalsSalt
* Purpose: compare crypt salts
* Input  : cryptSalt0,cryptSalt1 - crypt salt to compare
* Output : -
* Return : TRUE iff equals
* Notes  : -
\***********************************************************************/

INLINE bool Crypt_equalsSalt(const CryptSalt *cryptSalt0, const CryptSalt *cryptSalt1);
#if defined(NDEBUG) || defined(__CRYPT_IMPLEMENTATION__)
INLINE bool Crypt_equalsSalt(const CryptSalt *cryptSalt0, const CryptSalt *cryptSalt1)
{
  assert(cryptSalt0 != NULL);
  assert(cryptSalt1 != NULL);

  return memEquals(cryptSalt0->data,cryptSalt0->length,
                   cryptSalt1->data,cryptSalt1->length
                  );
}
#endif /* NDEBUG || __CRYPT_IMPLEMENTATION__ */

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Crypt_init
* Purpose: create new crypt handle
* Input  : cryptInfo      - crypt info block
*          cryptAlgorithm - crypt algorithm to use
*          cryptMode      - crypt mode; see CRYPT_MODE_...
*          cryptSalt      - encryption salt (can be NULL)
*          cryptKey       - crypt key
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Errors Crypt_init(CryptInfo       *cryptInfo,
                  CryptAlgorithms cryptAlgorithm,
                  CryptMode       cryptMode,
                  const CryptSalt *cryptSalt,
                  const CryptKey  *cryptKey
                 );
#else /* not NDEBUG */
Errors __Crypt_init(const char      *__fileName__,
                    ulong           __lineNb__,
                    CryptInfo       *cryptInfo,
                    CryptAlgorithms cryptAlgorithm,
                    CryptMode       cryptMode,
                    const CryptSalt *cryptSalt,
                    const CryptKey  *cryptKey
                   );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_done
* Purpose: done crypt handle
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
* Name   : Crypt_new
* Purpose: create new crypt handle
* Input  : cryptInfo      - crypt info block
*          cryptAlgorithm - crypt algorithm to use
*          cryptMode      - crypt mode; see CRYPT_MODE_...
*          cryptSalt      - encryption salt (can be NULL)
*          cryptKey       - crypt key
* Output : -
* Return : crypt info block or NULL
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
CryptInfo *Crypt_new(CryptAlgorithms cryptAlgorithm,
                     CryptMode       cryptMode,
                     const CryptSalt *cryptSalt,
                     const CryptKey  *cryptKey
                    );
#else /* not NDEBUG */
CryptInfo *__Crypt_new(const char      *__fileName__,
                       ulong           __lineNb__,
                       CryptAlgorithms cryptAlgorithm,
                       CryptMode       cryptMode,
                       const CryptSalt *cryptSalt,
                       const CryptKey  *cryptKey
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
void Crypt_delete(CryptInfo *cryptInfo);
#else /* not NDEBUG */
void __Crypt_delete(const char *__fileName__,
                    ulong      __lineNb__,
                    CryptInfo  *cryptInfo
                   );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_reset
* Purpose: reset crypt handle
* Input  : cryptInfo - crypt info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_reset(CryptInfo *cryptInfo);

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
                     ulong     bufferLength
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

INLINE bool Crypt_isAsymmetricSupported(void);
#if defined(NDEBUG) || defined(__CRYPT_IMPLEMENTATION__)
 INLINE bool Crypt_isAsymmetricSupported(void)
{
  #ifdef HAVE_GCRYPT
    return TRUE;
  #else /* not HAVE_GCRYPT */
    return FALSE;
  #endif /* HAVE_GCRYPT */
}
#endif /* NDEBUG || __CRYPT_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Crypt_initKey
* Purpose: initialize public/private key
* Input  : cryptKey         - crypt key variable
*          cryptPaddingType - padding type; see CryptPaddingTypes
* Output : cryptKey - crypt key
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Crypt_initKey(CryptKey          *cryptKey,
                     CryptPaddingTypes cryptPaddingType
                    );
#else /* not NDEBUG */
  void __Crypt_initKey(const char        *__fileName__,
                       ulong             __lineNb__,
                       CryptKey          *cryptKey,
                       CryptPaddingTypes cryptPaddingType
                      );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_duplicateKey
* Purpose: duplicate public/private key
* Input  : cryptKey     - crypt key variable
*          fromCryptKey - from crypt key
* Output : cryptKey - crypt key
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Crypt_duplicateKey(CryptKey       *cryptKey,
                            const CryptKey *fromCryptKey
                           );
#else /* not NDEBUG */
  Errors __Crypt_duplicateKey(const char     *__fileName__,
                              ulong          __lineNb__,
                              CryptKey       *cryptKey,
                              const CryptKey *fromCryptKey
                             );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_doneKey
* Purpose: deinitialize public/private key
* Input  : cryptKey - crypt key
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Crypt_doneKey(CryptKey *cryptKey);
#else /* not NDEBUG */
  void __Crypt_doneKey(const char        *__fileName__,
                       ulong             __lineNb__,
                       CryptKey          *cryptKey
                      );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_newKey
* Purpose: new public/private key
* Input  : cryptPaddingType - padding type; see CryptPaddingTypes
* Output : -
* Return : crypt key
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  CryptKey *Crypt_newKey(CryptPaddingTypes cryptPaddingType);
#else /* not NDEBUG */
  CryptKey *__Crypt_newKey(const char        *__fileName__,
                           ulong             __lineNb__,
                           CryptPaddingTypes cryptPaddingType
                          );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_deleteKey
* Purpose: delete public/private key
* Input  : cryptKey - crypt key
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Crypt_deleteKey(CryptKey *cryptKey);
#else /* not NDEBUG */
  void __Crypt_deleteKey(const char        *__fileName__,
                         ulong             __lineNb__,
                         CryptKey          *cryptKey
                        );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_clearKey
* Purpose: clear key
* Input  : cryptKey - crypt key
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_clearKey(CryptKey *cryptKey);

/***********************************************************************\
* Name   : Crypt_isKeyAvailable
* Purpose: check if key available
* Input  : cryptKey - crypt key
* Output : -
* Return : TRUE iff key available
* Notes  : -
\***********************************************************************/

INLINE bool Crypt_isKeyAvailable(const CryptKey *cryptKey);
#if defined(NDEBUG) || defined(__CRYPT_IMPLEMENTATION__)
INLINE bool Crypt_isKeyAvailable(const CryptKey *cryptKey)
{
  return cryptKey->data != NULL;
}
#endif /* NDEBUG || __CRYPT_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Crypt_deriveKey
* Purpose: derive and generate a crypt key from a password
* Input  : cryptKey           - crypt key
*          cryptKeyDeriveType - key derive type; see CryptKeyDeriveTypes
*          cryptSalt          - encryption salt (can be NULL)
*          password           - password
*          keyLength          - crypt key length [bits]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_deriveKey(CryptKey            *cryptKey,
                       CryptKeyDeriveTypes cryptKeyDeriveType,
                       const CryptSalt     *cryptSalt,
                       const Password      *password,
                       uint                keyLength
                      );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Crypt_getPublicPrivateKeyData
* Purpose: encrypt public/private key and get encrypted key
* Input  : cryptKey               - crypt key
*          encryptedKeyData       - encrypted key variable
*          encryptedkeyDataLength - encrypted key length variable
*          cryptMode              - crypt mode; see CRYPT_MODE_...
*          cryptKeyDeriveType     - key derive type; see
*                                   CryptKeyDeriveTypes
*          cryptSalt              - encryption salt (can be NULL)
*          password               - password for encryption (can be
*                                   NULL)
* Output : encryptedKey           - encrypted key base64 encoded
*          encryptedKeyLength     - length of encrypted key [bytes]
* Return : ERROR_NONE or error code
* Notes  : encryptedKey must be freed with freeSecure()!
\***********************************************************************/

Errors Crypt_getPublicPrivateKeyData(CryptKey            *cryptKey,
                                     void                **encryptedKeyData,
                                     uint                *encryptedKeyDataLength,
                                     CryptMode           cryptMode,
                                     CryptKeyDeriveTypes cryptKeyDeriveType,
                                     const CryptSalt     *cryptSalt,
                                     const Password      *password
                                    );

/***********************************************************************\
* Name   : Crypt_setPublicPrivateKeyData
* Purpose: decrypt public/private key and set key data
* Input  : cryptKey               - crypt key
*          encryptedKeyData       - encrypted key base64 encoded
*          encryptedKeyDataLength - length of encrypted key
*          cryptMode              - crypt mode; see CRYPT_MODE_...
*          cryptKeyDeriveType     - key derive type; see
*                                   CryptKeyDeriveTypes
*          cryptSalt              - encryption salt (can be NULL)
*          password               - password for decryption (can be NULL)
* Output : cryptKey - crypt key
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_setPublicPrivateKeyData(CryptKey            *cryptKey,
                                     const void          *encryptedKeyData,
                                     uint                encryptedKeyDataLength,
                                     CryptMode           cryptMode,
                                     CryptKeyDeriveTypes cryptKeyDeriveType,
                                     const CryptSalt     *cryptSalt,
                                     const Password      *password
                                    );

/***********************************************************************\
* Name   : Crypt_getPublicKeyModulusExponent
* Purpose: get public key modulus/exponent as hex-string
* Input  : cryptKey - crypt key
*          modulus  - modulus variable
*          exponent - exponent variable
* Output : modulus  - key modulus
*          exponent - key exponent
* Return : TRUE iff got modules/exponent
* Notes  : -
\***********************************************************************/

bool Crypt_getPublicKeyModulusExponent(CryptKey *cryptKey,
                                       String   modulus,
                                       String   exponent
                                      );

/***********************************************************************\
* Name   : Crypt_setPublicKeyModulusExponent
* Purpose: set public key modulus+exponent from hex-string
* Input  : cryptKey - crypt key
*          modulus  - key modulus
*          exponent - key exponent
* Output : -
* Return : TRUE iff set modules/exponent
* Notes  : -
\***********************************************************************/

bool Crypt_setPublicKeyModulusExponent(CryptKey    *cryptKey,
                                       ConstString modulus,
                                       ConstString exponent
                                      );

/***********************************************************************\
* Name   : Crypt_readPublicPrivateKeyFile
* Purpose: read key from file (base64-encoded)
* Input  : fileName           - file name
*          cryptMode          - crypt mode; see CRYPT_MODE_...
*          cryptKeyDeriveType - key derive type; see
*          cryptSalt          - encryption salt (can be NULL)
*          password           - password tor decrypt key (can be NULL)
* Output : cryptKey - crypt key
* Return : ERROR_NONE or error code
* Notes  : use own specific file format
\***********************************************************************/

Errors Crypt_readPublicPrivateKeyFile(CryptKey            *cryptKey,
                                      const char          *fileName,
                                      CryptMode           cryptMode,
                                      CryptKeyDeriveTypes cryptKeyDeriveType,
                                      const CryptSalt     *cryptSalt,
                                      const Password      *password
                                     );

/***********************************************************************\
* Name   : Crypt_writePublicPrivateKeyFile
* Purpose: write key to file (base64-encoded)
* Input  : cryptKey           - crypt key
*          fileName           - file name
*          cryptMode          - crypt mode; see CRYPT_MODE_...
*          cryptKeyDeriveType - key derive type; see
*          cryptSalt          - encryption salt (can be NULL)
*          password           - password to encrypt key (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : use own specific file format
\***********************************************************************/

Errors Crypt_writePublicPrivateKeyFile(CryptKey            *cryptKey,
                                       const char          *fileName,
                                       CryptMode           cryptMode,
                                       CryptKeyDeriveTypes cryptKeyDeriveType,
                                       const CryptSalt     *cryptSalt,
                                       const Password      *password
                                      );

/***********************************************************************\
* Name   : Crypt_createPublicPrivateKeyPair
* Purpose: create new public/private key pair encryption/decryption
* Input  : publicCryptKey  - public crypt key (encryption or signature
*                            check)
*          privateCryptKey - private crypt key (decryption or signature
*                            generation)
*          keyLength       - number of RSA key bits
*          cryptKeyMode    - crypt key mode; see CRYPT_KEY_MODE_...
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_createPublicPrivateKeyPair(CryptKey *publicCryptKey,
                                        CryptKey *privateCryptKey,
                                        uint     keyLength,
                                        uint     cryptKeyMode
                                       );

/***********************************************************************\
* Name   : Crypt_encryptWithPublicKey
* Purpose: encrypt data with public key
* Input  : cryptKey               - public/private crypt key
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

Errors Crypt_encryptWithPublicKey(const CryptKey *publicCryptKey,
                                  const void     *buffer,
                                  uint           bufferLength,
                                  void           *encryptBuffer,
                                  uint           *encryptBufferLength,
                                  uint           maxEncryptBufferLength
                                 );

/***********************************************************************\
* Name   : Crypt_decryptWithPrivateKey
* Purpose: decrypt data with private
* Input  : privateCryptKey     - public/private crypt key
*          encryptBuffer       - encrypted data
*          encryptBufferLength - length of encrypted data
*          buffer              - buffer for data
*          maxBufferLength     - max. length of buffer for data
* Output : buffer       - data (allocated memory)
*          bufferLength - length of data
* Return : ERROR_NONE or error code
* Notes  : if bufferLength==maxBufferLength buffer was to small!
\***********************************************************************/

Errors Crypt_decryptWithPrivateKey(const CryptKey *privateCryptKey,
                                   const void     *encryptBuffer,
                                   uint           encryptBufferLength,
                                   void           *buffer,
                                   uint           *bufferLength,
                                   uint           maxBufferLength
                                  );

/***********************************************************************\
* Name   : Crypt_getRandomCryptKey
* Purpose: get random crypt key and encrypted key data
* Input  : cryptKey                  - crypt key variable
*          keyLength                 - crypt key length [bits]
*          publicKey                 - public key for encryption of
*                                      random key
*          encryptedKey              - buffer for encrypted random key
*          maxEncryptedKeyDataLength - max. length of encryption buffer
*                                      [bytes]
*          encryptedKeyDataLength    - buffer length variable
* Output : cryptKey               - created random crypt key
*          encryptedKeyData       - encrypted random key
*          encryptedKeyDataLength - length of encrypted random key [bytes]
* Return : ERROR_NONE or error code
* Notes  : if encryptBufferLength==maxEncryptBufferLength buffer was to
*          small!
\***********************************************************************/

Errors Crypt_getRandomCryptKey(CryptKey       *cryptKey,
                               uint           keyLength,
                               const CryptKey *publicKey,
                               void           *encryptedKey,
                               uint           maxEncryptedKeyDataLength,
                               uint           *encryptedKeyDataLength
                              );

// TODO: remove?
/***********************************************************************\
* Name   : Crypt_getDecryptKey
* Purpose: get crypt key from encrypted key data and private key
* Input  : cryptKey               - crypt key variable
*          keyLength              - crypt key length [bits]
*          privateKey             - private key to decrypt key
*          encryptedKeyData       - encrypted random key
*          encryptedKeyDataLength - length of encrypted random key [bytes]
* Output : cryptKey - decryption key
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_getDecryptKey(CryptKey       *cryptKey,
                           uint           keyLength,
                           const CryptKey *privateKey,
                           const void     *encryptedKeyData,
                           uint           encryptedKeyDataLength
                          );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Crypt_getSignature
* Purpose: get signature for data
* Input  : privateKey         - private signature key
*          buffer             - data to sign
*          bufferLength       - length of data
*          maxSignatureLength - size of signature data buffer
* Output : signature       - signagure data
*          signatureLength - signature data length
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_getSignature(CryptKey *privateKey,
                          void     *buffer,
                          uint     bufferLength,
                          void     *signature,
                          uint     maxSignatureLength,
                          uint     *signatureLength
                         );

/***********************************************************************\
* Name   : Crypt_verifySignature
* Purpose: decrypt data block
* Input  : publicKey       - public signature key
*          buffer          - data to verify
*          bufferLength    - length of data
*          signature       - signature data
*          signatureLength - signature data length
* Output : cryptSignatureState - signature state (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_verifySignature(CryptKey             *publicKey,
                             const void           *buffer,
                             uint                 bufferLength,
                             const void           *signature,
                             uint                 signatureLength,
                             CryptSignatureStates *cryptSignatureState
                            );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Crypt_initHash
* Purpose: init hash
* Input  : cryptHash          - crypt hash variable
*          cryptHashAlgorithm - hash algorithm; see CryptHashAlgorithms
* Output : cryptHash - crypt hash info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Errors Crypt_initHash(CryptHash           *cryptHash,
                      CryptHashAlgorithms cryptHashAlgorithm
                     );
#else /* not NDEBUG */
Errors __Crypt_initHash(const char          *__fileName__,
                        ulong               __lineNb__,
                        CryptHash           *cryptHash,
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
void Crypt_doneHash(CryptHash *cryptHash);
#else /* not NDEBUG */
void __Crypt_doneHash(const char *__fileName__,
                      ulong      __lineNb__,
                      CryptHash  *cryptHash
                     );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_newHash
* Purpose: new hash
* Input  : cryptHashAlgorithm - hash algorithm; see CryptHashAlgorithms
* Output : -
* Return : crypt hash or NULL
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  CryptHash *Crypt_newHash(CryptHashAlgorithms cryptHashAlgorithm);
#else /* not NDEBUG */
  CryptHash *__Crypt_newHash(const char          *__fileName__,
                             ulong               __lineNb__,
                             CryptHashAlgorithms cryptHashAlgorithm
                            );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_deleteHash
* Purpose: delete hash
* Input  : cryptHash - crypt hash
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Crypt_deleteHash(CryptHash *cryptHash);
#else /* not NDEBUG */
  void __Crypt_deleteHash(const char *__fileName__,
                          ulong      __lineNb__,
                          CryptHash  *cryptHash
                         );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_resetHash
* Purpose: reset hash
* Input  : cryptHash - crypt hash
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_resetHash(CryptHash *cryptHash);

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

void Crypt_updateHash(CryptHash  *cryptHash,
                      const void *buffer,
                      ulong      bufferLength
                     );

/***********************************************************************\
* Name   : Crypt_getHashLength
* Purpose: get hash length
* Input  : cryptHash - crypt hash
* Output : -
* Return : hash length [bytes]
* Notes  : -
\***********************************************************************/

uint Crypt_getHashLength(const CryptHash *cryptHash);

/***********************************************************************\
* Name   : Crypt_getHash
* Purpose: get hash
* Input  : cryptHash  - crypt hash
*          buffer     - buffer for hash
*          bufferSize - buffer size
* Output : hashLength - hash length (can be NULL)
* Return : hash buffer or NULL
* Notes  : -
\***********************************************************************/

void *Crypt_getHash(const CryptHash *cryptHash,
                    void            *buffer,
                    uint            bufferSize,
                    uint            *hashLength
                   );

/***********************************************************************\
* Name   : Crypt_equalsHash
* Purpose: compare with hash
* Input  : cryptHash  - crypt hash
*          hash       - buffer with hash to compare with
*          hashLength - hash length
* Output : -
* Return : TRUE iff hash equals
* Notes  : -
\***********************************************************************/

bool Crypt_equalsHash(const CryptHash *cryptHash0,
                      const CryptHash *cryptHash1
                     );
bool Crypt_equalsHashBuffer(const CryptHash *cryptHash,
                            const void      *buffer,
                            uint            bufferLength
                           );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Crypt_initMAC
* Purpose: init message authentication code
* Input  : cryptMAC          - crypt MAC info variable
*          cryptMACAlgorithm - MAC algorithm; see CryptMACAlgorithms
* Output : cryptMAC - crypt MAC
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
Errors Crypt_initMAC(CryptMAC           *cryptMAC,
                     CryptMACAlgorithms cryptMACAlgorithm,
                     const void         *keyData,
                     uint               keyDataLength
                    );
#else /* not NDEBUG */
Errors __Crypt_initMAC(const char         *__fileName__,
                       ulong              __lineNb__,
                       CryptMAC           *cryptMAC,
                       CryptMACAlgorithms cryptMACAlgorithm,
                       const void         *keyData,
                       uint               keyDataLength
                      );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_doneMAC
* Purpose: done message authentication code
* Input  : cryptMAC - crypt MAC
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void Crypt_doneMAC(CryptMAC *cryptMAC);
#else /* not NDEBUG */
void __Crypt_doneMAC(const char *__fileName__,
                     ulong      __lineNb__,
                     CryptMAC   *cryptMAC
                    );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_newMAC
* Purpose: new MAC
* Input  : cryptMACAlgorithm - hash algorithm; see CryptMACAlgorithms
* Output : -
* Return : crypt MAC or NULL
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  CryptMAC *Crypt_newMAC(CryptMACAlgorithms cryptMACAlgorithm,
                         const void         *keyData,
                         uint               keyDataLength
                        );
#else /* not NDEBUG */
  CryptMAC *__Crypt_newMAC(const char         *__fileName__,
                           ulong              __lineNb__,
                           CryptMACAlgorithms cryptMACAlgorithm,
                           const void         *keyData,
                           uint               keyDataLength
                          );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_deleteMAC
* Purpose: delete MAC
* Input  : cryptMAC - crypt MAC
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Crypt_deleteMAC(CryptMAC *cryptMAC);
#else /* not NDEBUG */
  void __Crypt_deleteMAC(const char *__fileName__,
                         ulong      __lineNb__,
                         CryptMAC   *cryptMAC
                        );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Crypt_resetMAC
* Purpose: reset message authentication code
* Input  : cryptMAC - crypt MAC
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_resetMAC(CryptMAC *cryptMAC);

/***********************************************************************\
* Name   : Crypt_updateMAC
* Purpose: update message authentication code
* Input  : cryptMAC     - crypt MAC
*          buffer       - buffer with data
*          bufferLength - buffer length
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_updateMAC(CryptMAC *cryptMAC,
                     void     *buffer,
                     ulong    bufferLength
                    );

/***********************************************************************\
* Name   : Crypt_getMACLength
* Purpose: get message authentication code length
* Input  : cryptMAC - crypt MAC
* Output : -
* Return : MAC length [bytes]
* Notes  : -
\***********************************************************************/

uint Crypt_getMACLength(const CryptMAC *cryptMAC);

/***********************************************************************\
* Name   : Crypt_getMAC
* Purpose: get message authentication code
* Input  : cryptMAC   - crypt MAC
*          mac        - buffer for MAC
*          maxMACSize - buffer size
* Output : macLength - MAC length (can be NULL)
* Return : buffer or NULL
* Notes  : -
\***********************************************************************/

void *Crypt_getMAC(const CryptMAC *cryptMAC,
                   void           *buffer,
                   uint           bufferSize,
                   uint           *macLength
                  );

/***********************************************************************\
* Name   : Crypt_verifyMAC
* Purpose: verify message authentication code
* Input  : cryptMAC  - crypt MAC
*          mac       - buffer with MAC to compare with
*          macLength - MAC length
* Output : -
* Return : TRUE iff MAC equals
* Notes  : -
\***********************************************************************/

bool Crypt_verifyMAC(const CryptMAC *cryptMAC,
                     void           *mac,
                     uint           macLength
                    );

#ifdef __cplusplus
  }
#endif

#ifndef NDEBUG
/***********************************************************************\
* Name   : Crypt_dumpKey, Crypt_dumpSalt, Crypt_dumpHash, Crypt_dumpMAC
* Purpose: dump key/salt/hash/MAC
* Input  : text      - text
*          cryptKey  - crypt key
*          cryptSalt - crypt salt
*          cryptHash - crypt hash
*          cryptMAC  - crypt MAC
* Output : -
* Return : -
* Notes  : Debug only!
\***********************************************************************/

void Crypt_dumpKey(const CryptKey *cryptKey);
void Crypt_dumpSalt(const CryptSalt *cryptSalt);
void Crypt_dumpHash(const CryptHash *cryptHash);
void Crypt_dumpMAC(const CryptMAC *cryptMAC);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __CRYPT__ */

/* end of file */
