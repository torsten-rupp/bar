/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver crypt functions
* Systems: all
*
\***********************************************************************/

#define __CRYPT_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#ifdef HAVE_ARPA_INET_H
  #include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */
#ifdef HAVE_GCRYPT
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  #include <gcrypt.h>
  #pragma GCC diagnostic warning "-Wdeprecated-declarations"
#endif /* HAVE_GCRYPT */
#include <zlib.h>
#include <errno.h>
#include <assert.h>

#include "common/global.h"
#include "common/strings.h"
#include "common/lists.h"
#include "common/misc.h"
#include "common/files.h"
#include "common/passwords.h"

#include "archive_format.h"
#include "chunks.h"

#include "crypt.h"

/****************** Conditional compilation switches *******************/

#define _DEBUG_ASYMMETRIC_CRYPT

/***************************** Constants *******************************/

#define MAX_KEY_SIZE 2048               // max. size of a key in bits

#define BLOCK_LENGTH_CRYPT_NONE 4       // block size if no encryption

// crypt algorithm names
LOCAL const struct
{
  const char      *name;
  CryptAlgorithms cryptAlgorithm;
} CRYPT_ALGORITHMS[] =
{
  { "none",       CRYPT_ALGORITHM_NONE        },

  { "3DES",       CRYPT_ALGORITHM_3DES        },
  { "CAST5",      CRYPT_ALGORITHM_CAST5       },
  { "BLOWFISH",   CRYPT_ALGORITHM_BLOWFISH    },
  { "AES128",     CRYPT_ALGORITHM_AES128      },
  { "AES192",     CRYPT_ALGORITHM_AES192      },
  { "AES256",     CRYPT_ALGORITHM_AES256      },
  { "TWOFISH128", CRYPT_ALGORITHM_TWOFISH128  },
  { "TWOFISH256", CRYPT_ALGORITHM_TWOFISH256  },
  { "SERPENT128", CRYPT_ALGORITHM_SERPENT128  },
  { "SERPENT192", CRYPT_ALGORITHM_SERPENT192  },
  { "SERPENT256", CRYPT_ALGORITHM_SERPENT256  },
  { "CAMELLIA128",CRYPT_ALGORITHM_CAMELLIA128 },
  { "CAMELLIA192",CRYPT_ALGORITHM_CAMELLIA192 },
  { "CAMELLIA256",CRYPT_ALGORITHM_CAMELLIA256 },
};

// hash algorithm names
LOCAL const struct
{
  const char          *name;
  CryptHashAlgorithms cryptHashAlgorithm;
} CRYPT_HASH_ALGORITHMS[] =
{
  { "SHA2-224",   CRYPT_HASH_ALGORITHM_SHA2_224 },
  { "SHA2-256",   CRYPT_HASH_ALGORITHM_SHA2_256 },
  { "SHA2-384",   CRYPT_HASH_ALGORITHM_SHA2_384 },
  { "SHA2-512",   CRYPT_HASH_ALGORITHM_SHA2_512 },
};

// MAC algorithm names
LOCAL const struct
{
  const char         *name;
  CryptMACAlgorithms cryptMACAlgorithm;
} CRYPT_MAC_ALGORITHMS[] =
{
  { "SHA2-224",   CRYPT_MAC_ALGORITHM_SHA2_224 },
  { "SHA2-256",   CRYPT_MAC_ALGORITHM_SHA2_256 },
  { "SHA2-384",   CRYPT_MAC_ALGORITHM_SHA2_384 },
  { "SHA2-512",   CRYPT_MAC_ALGORITHM_SHA2_512 },
};

// key derivation
#define KEY_DERIVE_ALGORITHM      GCRY_KDF_PBKDF2
#define KEY_DERIVE_HASH_ALGORITHM GCRY_MD_SHA512
#define KEY_DERIVE_ITERATIONS     20000
#define KEY_DERIVE_KEY_SIZE       (512/8)

/* PKCS1 encoded message buffer for RSA encryption/decryption
   format 0x00 0x02 <PS random data> 0x00 <key data>

   +------+------+----------------+------+------------------------+
   | 0x00 | 0x02 |    random      | 0x00 | key data               |
   +------+------+----------------+------+------------------------+
    <----> <----> <--------------> <----> <---------------------->
      1      1     random length     1     key length
    <------------------------------------------------------------>
                               64 (512bit)

   Note: random length must be at least 8 bytes and do not contain 0-bytes!

   See: https://tools.ietf.org/html/rfc3447
*/
#define PKCS1_ENCODED_MESSAGE_LENGTH         (512/8)  // [bytes]
#define PKCS1_MIN_RANDOM_LENGTH              8  // [bytes]
#define PKCS1_KEY_LENGTH                     ((PKCS1_ENCODED_MESSAGE_LENGTH-(1+1+PKCS1_MIN_RANDOM_LENGTH+1))*8)  // [bits]
#define PKCS1_ENCODED_MESSAGE_PADDING_LENGTH (PKCS1_ENCODED_MESSAGE_LENGTH-ALIGN(PKCS1_KEY_LENGTH,8)/8-1-1-1)  // [bytes]

// used symmetric encryption algorithm in RSA hybrid encryption
#define SECRET_KEY_CRYPT_ALGORITHM CRYPT_ALGORITHM_AES256

// empty salt
LOCAL const byte NO_SALT[512/8] = {0,0,0,0,
                                   0,0,0,0,
                                   0,0,0,0,
                                   0,0,0,0,
                                   0,0,0,0,
                                   0,0,0,0,
                                   0,0,0,0,
                                   0,0,0,0,
                                   0,0,0,0,
                                   0,0,0,0,
                                   0,0,0,0,
                                   0,0,0,0,
                                   0,0,0,0,
                                   0,0,0,0,
                                   0,0,0,0,
                                   0,0,0,0
                                 };

/***************************** Datatypes *******************************/

typedef struct ATTRIBUTE_PACKED
{
  uint32 crc;                     // CRC encrypted key data (big endian)
  uint32 dataLength;              // encrypted key data length (big endian)
  byte   data[0];                 // encrypted key data
} KeyImportExportInfo;

/***************************** Variables *******************************/
uint cryptKeyLengths[CRYPT_ALGORITHM_MAX];
uint cryptBlockLengths[CRYPT_ALGORITHM_MAX];

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifdef HAVE_GCRYPT
/***********************************************************************\
* Name   : getCryptKeyLength
* Purpose: get key length of crypt algorihm
* Input  : cryptAlgorithm - crypt algorithm
* Output : keyLength - key length
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getCryptKeyLength(CryptAlgorithms cryptAlgorithm,
                               uint            *keyLength
                              )
{
  assert(keyLength != NULL);

  switch (cryptAlgorithm)
  {
    case CRYPT_ALGORITHM_NONE:
      (*keyLength) = 0;
      break;
    case CRYPT_ALGORITHM_3DES:
    case CRYPT_ALGORITHM_CAST5:
    case CRYPT_ALGORITHM_BLOWFISH:
    case CRYPT_ALGORITHM_AES128:
    case CRYPT_ALGORITHM_AES192:
    case CRYPT_ALGORITHM_AES256:
    case CRYPT_ALGORITHM_TWOFISH128:
    case CRYPT_ALGORITHM_TWOFISH256:
    case CRYPT_ALGORITHM_SERPENT128:
    case CRYPT_ALGORITHM_SERPENT192:
    case CRYPT_ALGORITHM_SERPENT256:
    case CRYPT_ALGORITHM_CAMELLIA128:
    case CRYPT_ALGORITHM_CAMELLIA192:
    case CRYPT_ALGORITHM_CAMELLIA256:
      #ifdef HAVE_GCRYPT
        {
          int          gcryptAlgorithm;
          gcry_error_t gcryptError;
          size_t       n;

          gcryptAlgorithm = 0;
          switch (cryptAlgorithm)
          {
            case CRYPT_ALGORITHM_3DES:        gcryptAlgorithm = GCRY_CIPHER_3DES;        break;
            case CRYPT_ALGORITHM_CAST5:       gcryptAlgorithm = GCRY_CIPHER_CAST5;       break;
            case CRYPT_ALGORITHM_BLOWFISH:    gcryptAlgorithm = GCRY_CIPHER_BLOWFISH;    break;
            case CRYPT_ALGORITHM_AES128:      gcryptAlgorithm = GCRY_CIPHER_AES;         break;
            case CRYPT_ALGORITHM_AES192:      gcryptAlgorithm = GCRY_CIPHER_AES192;      break;
            case CRYPT_ALGORITHM_AES256:      gcryptAlgorithm = GCRY_CIPHER_AES256;      break;
            case CRYPT_ALGORITHM_TWOFISH128:  gcryptAlgorithm = GCRY_CIPHER_TWOFISH128;  break;
            case CRYPT_ALGORITHM_TWOFISH256:  gcryptAlgorithm = GCRY_CIPHER_TWOFISH;     break;
            case CRYPT_ALGORITHM_SERPENT128:  gcryptAlgorithm = GCRY_CIPHER_SERPENT128;  break;
            case CRYPT_ALGORITHM_SERPENT192:  gcryptAlgorithm = GCRY_CIPHER_SERPENT192;  break;
            case CRYPT_ALGORITHM_SERPENT256:  gcryptAlgorithm = GCRY_CIPHER_SERPENT256;  break;
            case CRYPT_ALGORITHM_CAMELLIA128: gcryptAlgorithm = GCRY_CIPHER_CAMELLIA128; break;
            case CRYPT_ALGORITHM_CAMELLIA192: gcryptAlgorithm = GCRY_CIPHER_CAMELLIA192; break;
            case CRYPT_ALGORITHM_CAMELLIA256: gcryptAlgorithm = GCRY_CIPHER_CAMELLIA256; break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break;
          }

          // check if algorithm available
          gcryptError = gcry_cipher_algo_info(gcryptAlgorithm,
                                              GCRYCTL_TEST_ALGO,
                                              NULL,
                                              NULL
                                             );
          if (gcryptError != 0)
          {
            char buffer[128];

            gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
            return ERRORX_(INIT_CIPHER,
                           gcryptError,
                           "cipher '%s' not available: %s",
                           Crypt_algorithmToString(cryptAlgorithm,NULL),
                           buffer
                          );
          }

          // get key length
          gcryptError = gcry_cipher_algo_info(gcryptAlgorithm,
                                              GCRYCTL_GET_KEYLEN,
                                              NULL,
                                              &n
                                             );
          if (gcryptError != 0)
          {
            char buffer[128];

            gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
            return ERRORX_(INIT_CIPHER,
                           gcryptError,
                           "detect key length of '%s': %s",
                           gcry_cipher_algo_name(gcryptAlgorithm),
                           buffer
                          );
          }
          (*keyLength) = n*8;
        }
      #else /* not HAVE_GCRYPT */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : getCryptBlockLength
* Purpose: get block length of crypt algorihm
* Input  : cryptAlgorithm - crypt algorithm
* Output : blockLength - block length
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getCryptBlockLength(CryptAlgorithms cryptAlgorithm,
                                 uint            *blockLength
                                )
{
  assert(blockLength != NULL);

  switch (cryptAlgorithm)
  {
    case CRYPT_ALGORITHM_NONE:
      (*blockLength) = BLOCK_LENGTH_CRYPT_NONE;
      break;
    case CRYPT_ALGORITHM_3DES:
    case CRYPT_ALGORITHM_CAST5:
    case CRYPT_ALGORITHM_BLOWFISH:
    case CRYPT_ALGORITHM_AES128:
    case CRYPT_ALGORITHM_AES192:
    case CRYPT_ALGORITHM_AES256:
    case CRYPT_ALGORITHM_TWOFISH128:
    case CRYPT_ALGORITHM_TWOFISH256:
    case CRYPT_ALGORITHM_SERPENT128:
    case CRYPT_ALGORITHM_SERPENT192:
    case CRYPT_ALGORITHM_SERPENT256:
    case CRYPT_ALGORITHM_CAMELLIA128:
    case CRYPT_ALGORITHM_CAMELLIA192:
    case CRYPT_ALGORITHM_CAMELLIA256:
      #ifdef HAVE_GCRYPT
        {
          int          gcryptAlgorithm;
          gcry_error_t gcryptError;
          size_t       n;

          gcryptAlgorithm = 0;
          switch (cryptAlgorithm)
          {
            case CRYPT_ALGORITHM_3DES:        gcryptAlgorithm = GCRY_CIPHER_3DES;        break;
            case CRYPT_ALGORITHM_CAST5:       gcryptAlgorithm = GCRY_CIPHER_CAST5;       break;
            case CRYPT_ALGORITHM_BLOWFISH:    gcryptAlgorithm = GCRY_CIPHER_BLOWFISH;    break;
            case CRYPT_ALGORITHM_AES128:      gcryptAlgorithm = GCRY_CIPHER_AES;         break;
            case CRYPT_ALGORITHM_AES192:      gcryptAlgorithm = GCRY_CIPHER_AES192;      break;
            case CRYPT_ALGORITHM_AES256:      gcryptAlgorithm = GCRY_CIPHER_AES256;      break;
            case CRYPT_ALGORITHM_TWOFISH128:  gcryptAlgorithm = GCRY_CIPHER_TWOFISH128;  break;
            case CRYPT_ALGORITHM_TWOFISH256:  gcryptAlgorithm = GCRY_CIPHER_TWOFISH;     break;
            case CRYPT_ALGORITHM_SERPENT128:  gcryptAlgorithm = GCRY_CIPHER_SERPENT128;  break;
            case CRYPT_ALGORITHM_SERPENT192:  gcryptAlgorithm = GCRY_CIPHER_SERPENT192;  break;
            case CRYPT_ALGORITHM_SERPENT256:  gcryptAlgorithm = GCRY_CIPHER_SERPENT256;  break;
            case CRYPT_ALGORITHM_CAMELLIA128: gcryptAlgorithm = GCRY_CIPHER_CAMELLIA128; break;
            case CRYPT_ALGORITHM_CAMELLIA192: gcryptAlgorithm = GCRY_CIPHER_CAMELLIA192; break;
            case CRYPT_ALGORITHM_CAMELLIA256: gcryptAlgorithm = GCRY_CIPHER_CAMELLIA256; break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break;
          }

          // check if algorithm available
          gcryptError = gcry_cipher_algo_info(gcryptAlgorithm,
                                              GCRYCTL_TEST_ALGO,
                                              NULL,
                                              NULL
                                             );
          if (gcryptError != 0)
          {
            char buffer[128];

            gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
            return ERRORX_(INIT_CIPHER,
                           gcryptError,
                           "cipher '%s' not available: %s",
                           Crypt_algorithmToString(cryptAlgorithm,NULL),
                           buffer
                          );
          }

          // get block length
          gcryptError = gcry_cipher_algo_info(gcryptAlgorithm,
                                              GCRYCTL_GET_BLKLEN,
                                              NULL,
                                              &n
                                             );
          if (gcryptError != 0)
          {
            char buffer[128];

            gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
            return ERRORX_(INIT_CIPHER,
                           gcryptError,
                           "detect block length of '%s': %s",
                           gcry_cipher_algo_name(gcryptAlgorithm),
                           buffer
                          );
          }
          (*blockLength) = n;
        }
      #else /* not HAVE_GCRYPT */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return ERROR_NONE;
}
#endif /* HAVE_GCRYPT */

#ifdef HAVE_GCRYPT
//  GCRY_THREAD_OPTION_PTHREAD_IMPL;
#endif /* HAVE_GCRYPT */

Errors Crypt_initAll(void)
{
  #ifdef HAVE_GCRYPT
    Errors error;
  #endif /* HAVE_GCRYPT */

  #ifdef HAVE_GCRYPT
    // check version and do internal library init
    assert(GCRYPT_VERSION_NUMBER >= 0x010600);
    if (!gcry_check_version(GCRYPT_VERSION))
    {
      return ERRORX_(INIT_CRYPT,0,"Wrong gcrypt version (needed: %d)",GCRYPT_VERSION);
    }

//    gcry_control(GCRYCTL_ENABLE_QUICK_RANDOM,0);
    #ifndef NDEBUG
//NYI: required/useful?
//      gcry_control(GCRYCTL_SET_DEBUG_FLAGS,1,0);
    #endif

    // get key lengths
    error = getCryptKeyLength(CRYPT_ALGORITHM_NONE,&cryptKeyLengths[CRYPT_ALGORITHM_NONE]);
    if (error != ERROR_NONE) return error;
    error = getCryptKeyLength(CRYPT_ALGORITHM_3DES,&cryptKeyLengths[CRYPT_ALGORITHM_3DES]);
    if (error != ERROR_NONE) return error;
    error = getCryptKeyLength(CRYPT_ALGORITHM_CAST5,&cryptKeyLengths[CRYPT_ALGORITHM_CAST5]);
    if (error != ERROR_NONE) return error;
    error = getCryptKeyLength(CRYPT_ALGORITHM_BLOWFISH,&cryptKeyLengths[CRYPT_ALGORITHM_BLOWFISH]);
    if (error != ERROR_NONE) return error;
    error = getCryptKeyLength(CRYPT_ALGORITHM_AES128,&cryptKeyLengths[CRYPT_ALGORITHM_AES128]);
    if (error != ERROR_NONE) return error;
    error = getCryptKeyLength(CRYPT_ALGORITHM_AES192,&cryptKeyLengths[CRYPT_ALGORITHM_AES192]);
    if (error != ERROR_NONE) return error;
    error = getCryptKeyLength(CRYPT_ALGORITHM_AES256,&cryptKeyLengths[CRYPT_ALGORITHM_AES256]);
    if (error != ERROR_NONE) return error;
    error = getCryptKeyLength(CRYPT_ALGORITHM_TWOFISH128,&cryptKeyLengths[CRYPT_ALGORITHM_TWOFISH128]);
    if (error != ERROR_NONE) return error;
    error = getCryptKeyLength(CRYPT_ALGORITHM_TWOFISH256,&cryptKeyLengths[CRYPT_ALGORITHM_TWOFISH256]);
    if (error != ERROR_NONE) return error;
    error = getCryptKeyLength(CRYPT_ALGORITHM_SERPENT128,&cryptKeyLengths[CRYPT_ALGORITHM_SERPENT128]);
    if (error != ERROR_NONE) return error;
    error = getCryptKeyLength(CRYPT_ALGORITHM_SERPENT192,&cryptKeyLengths[CRYPT_ALGORITHM_SERPENT192]);
    if (error != ERROR_NONE) return error;
    error = getCryptKeyLength(CRYPT_ALGORITHM_SERPENT256,&cryptKeyLengths[CRYPT_ALGORITHM_SERPENT256]);
    if (error != ERROR_NONE) return error;
    error = getCryptKeyLength(CRYPT_ALGORITHM_CAMELLIA128,&cryptKeyLengths[CRYPT_ALGORITHM_CAMELLIA128]);
    if (error != ERROR_NONE) return error;
    error = getCryptKeyLength(CRYPT_ALGORITHM_CAMELLIA192,&cryptKeyLengths[CRYPT_ALGORITHM_CAMELLIA192]);
    if (error != ERROR_NONE) return error;
    error = getCryptKeyLength(CRYPT_ALGORITHM_CAMELLIA256,&cryptKeyLengths[CRYPT_ALGORITHM_CAMELLIA256]);
    if (error != ERROR_NONE) return error;

    // get block lengths
    error = getCryptBlockLength(CRYPT_ALGORITHM_NONE,&cryptBlockLengths[CRYPT_ALGORITHM_NONE]);
    if (error != ERROR_NONE) return error;
    error = getCryptBlockLength(CRYPT_ALGORITHM_3DES,&cryptBlockLengths[CRYPT_ALGORITHM_3DES]);
    if (error != ERROR_NONE) return error;
    error = getCryptBlockLength(CRYPT_ALGORITHM_CAST5,&cryptBlockLengths[CRYPT_ALGORITHM_CAST5]);
    if (error != ERROR_NONE) return error;
    error = getCryptBlockLength(CRYPT_ALGORITHM_BLOWFISH,&cryptBlockLengths[CRYPT_ALGORITHM_BLOWFISH]);
    if (error != ERROR_NONE) return error;
    error = getCryptBlockLength(CRYPT_ALGORITHM_AES128,&cryptBlockLengths[CRYPT_ALGORITHM_AES128]);
    if (error != ERROR_NONE) return error;
    error = getCryptBlockLength(CRYPT_ALGORITHM_AES192,&cryptBlockLengths[CRYPT_ALGORITHM_AES192]);
    if (error != ERROR_NONE) return error;
    error = getCryptBlockLength(CRYPT_ALGORITHM_AES256,&cryptBlockLengths[CRYPT_ALGORITHM_AES256]);
    if (error != ERROR_NONE) return error;
    error = getCryptBlockLength(CRYPT_ALGORITHM_TWOFISH128,&cryptBlockLengths[CRYPT_ALGORITHM_TWOFISH128]);
    if (error != ERROR_NONE) return error;
    error = getCryptBlockLength(CRYPT_ALGORITHM_TWOFISH256,&cryptBlockLengths[CRYPT_ALGORITHM_TWOFISH256]);
    if (error != ERROR_NONE) return error;
    error = getCryptBlockLength(CRYPT_ALGORITHM_SERPENT128,&cryptBlockLengths[CRYPT_ALGORITHM_SERPENT128]);
    if (error != ERROR_NONE) return error;
    error = getCryptBlockLength(CRYPT_ALGORITHM_SERPENT192,&cryptBlockLengths[CRYPT_ALGORITHM_SERPENT192]);
    if (error != ERROR_NONE) return error;
    error = getCryptBlockLength(CRYPT_ALGORITHM_SERPENT256,&cryptBlockLengths[CRYPT_ALGORITHM_SERPENT256]);
    if (error != ERROR_NONE) return error;
    error = getCryptBlockLength(CRYPT_ALGORITHM_CAMELLIA128,&cryptBlockLengths[CRYPT_ALGORITHM_CAMELLIA128]);
    if (error != ERROR_NONE) return error;
    error = getCryptBlockLength(CRYPT_ALGORITHM_CAMELLIA192,&cryptBlockLengths[CRYPT_ALGORITHM_CAMELLIA192]);
    if (error != ERROR_NONE) return error;
    error = getCryptBlockLength(CRYPT_ALGORITHM_CAMELLIA256,&cryptBlockLengths[CRYPT_ALGORITHM_CAMELLIA256]);
    if (error != ERROR_NONE) return error;
  #endif /* HAVE_GCRYPT */

  return ERROR_NONE;
}

void Crypt_doneAll(void)
{
}

/*---------------------------------------------------------------------*/

CryptSalt *Crypt_initSalt(CryptSalt *cryptSalt)
{
  assert(cryptSalt != NULL);

  cryptSalt->dataLength = 0;

  return cryptSalt;
}

CryptSalt *Crypt_duplicateSalt(CryptSalt *cryptSalt, const CryptSalt *fromCryptSalt)
{
  assert(cryptSalt != NULL);
  assert(fromCryptSalt != NULL);
  assert(sizeof(cryptSalt->data) >= fromCryptSalt->dataLength);

  cryptSalt->dataLength = MIN(fromCryptSalt->dataLength,sizeof(cryptSalt->data));
  memCopyFast(cryptSalt->data,cryptSalt->dataLength,fromCryptSalt->data,fromCryptSalt->dataLength);

  return cryptSalt;
}

void Crypt_doneSalt(CryptSalt *cryptSalt)
{
  assert(cryptSalt != NULL);

  UNUSED_VARIABLE(cryptSalt);
}

CryptSalt *Crypt_setSalt(CryptSalt *cryptSalt, const byte *data, uint dataLength)
{
  assert(cryptSalt != NULL);
  assert(data != NULL);

  memCopyFast(cryptSalt->data,sizeof(cryptSalt->data),data,dataLength);
  cryptSalt->dataLength = MIN(sizeof(cryptSalt->data),dataLength);

  return cryptSalt;
}

CryptSalt *Crypt_clearSalt(CryptSalt *cryptSalt)
{
  assert(cryptSalt != NULL);

  memClear(cryptSalt->data,sizeof(cryptSalt->data));
  cryptSalt->dataLength = 0;

  return cryptSalt;
}

CryptSalt *Crypt_copySalt(CryptSalt *cryptSalt, const CryptSalt *fromCryptSalt)
{
  assert(cryptSalt != NULL);
  assert(fromCryptSalt != NULL);
  assert(sizeof(cryptSalt->data) >= fromCryptSalt->dataLength);

  cryptSalt->dataLength = MIN(fromCryptSalt->dataLength,sizeof(cryptSalt->data));
  memCopyFast(cryptSalt->data,cryptSalt->dataLength,fromCryptSalt->data,fromCryptSalt->dataLength);

  return cryptSalt;
}

CryptSalt *Crypt_randomSalt(CryptSalt *cryptSalt)
{
  assert(cryptSalt != NULL);

  Crypt_randomize(cryptSalt->data,sizeof(cryptSalt->data));
  cryptSalt->dataLength = sizeof(cryptSalt->data);

  return cryptSalt;
}

/*---------------------------------------------------------------------*/

bool Crypt_isValidAlgorithm(uint16 n)
{
  uint i;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(CRYPT_ALGORITHMS))
         && (CRYPT_ALGORITHMS[i].cryptAlgorithm != CRYPT_CONSTANT_TO_ALGORITHM(n))
        )
  {
    i++;
  }

  return (i < SIZE_OF_ARRAY(CRYPT_ALGORITHMS));
}

bool Crypt_isValidHashAlgorithm(uint16 n)
{
  uint i;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(CRYPT_HASH_ALGORITHMS))
         && (CRYPT_HASH_ALGORITHMS[i].cryptHashAlgorithm != CRYPT_CONSTANT_TO_HASH_ALGORITHM(n))
        )
  {
    i++;
  }

  return (i < SIZE_OF_ARRAY(CRYPT_HASH_ALGORITHMS));
}

bool Crypt_isValidMACAlgorithm(uint16 n)
{
  uint i;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(CRYPT_MAC_ALGORITHMS))
         && (CRYPT_MAC_ALGORITHMS[i].cryptMACAlgorithm != CRYPT_CONSTANT_TO_MAC_ALGORITHM(n))
        )
  {
    i++;
  }

  return (i < SIZE_OF_ARRAY(CRYPT_MAC_ALGORITHMS));
}

const char *Crypt_algorithmToString(CryptAlgorithms cryptAlgorithm, const char *defaultValue)
{
 uint       i;
  const char *name;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(CRYPT_ALGORITHMS))
         && (CRYPT_ALGORITHMS[i].cryptAlgorithm != cryptAlgorithm)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(CRYPT_ALGORITHMS))
  {
    name = CRYPT_ALGORITHMS[i].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Crypt_parseAlgorithm(const char *name, CryptAlgorithms *cryptAlgorithm)
{
  uint i;

  assert(name != NULL);
  assert(cryptAlgorithm != NULL);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(CRYPT_ALGORITHMS))
         && !stringEqualsIgnoreCase(CRYPT_ALGORITHMS[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(CRYPT_ALGORITHMS))
  {
    (*cryptAlgorithm) = CRYPT_ALGORITHMS[i].cryptAlgorithm;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

const char *Crypt_hashAlgorithmToString(CryptHashAlgorithms cryptHashAlgorithm, const char *defaultValue)
{
  uint       i;
  const char *name;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(CRYPT_HASH_ALGORITHMS))
         && (CRYPT_HASH_ALGORITHMS[i].cryptHashAlgorithm != cryptHashAlgorithm)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(CRYPT_HASH_ALGORITHMS))
  {
    name = CRYPT_HASH_ALGORITHMS[i].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Crypt_parseHashAlgorithm(const char *name, CryptHashAlgorithms *cryptHashAlgorithm)
{
  uint i;

  assert(name != NULL);
  assert(cryptHashAlgorithm != NULL);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(CRYPT_HASH_ALGORITHMS))
         && !stringEqualsIgnoreCase(CRYPT_HASH_ALGORITHMS[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(CRYPT_HASH_ALGORITHMS))
  {
    (*cryptHashAlgorithm) = CRYPT_HASH_ALGORITHMS[i].cryptHashAlgorithm;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

const char *Crypt_macAlgorithmToString(CryptMACAlgorithms cryptMACAlgorithm, const char *defaultValue)
{
  uint       i;
  const char *name;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(CRYPT_MAC_ALGORITHMS))
         && (CRYPT_MAC_ALGORITHMS[i].cryptMACAlgorithm != cryptMACAlgorithm)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(CRYPT_MAC_ALGORITHMS))
  {
    name = CRYPT_MAC_ALGORITHMS[i].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Crypt_parseMACAlgorithm(const char *name, CryptMACAlgorithms *cryptMACAlgorithm)
{
  uint i;

  assert(name != NULL);
  assert(cryptMACAlgorithm != NULL);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(CRYPT_MAC_ALGORITHMS))
         && !stringEqualsIgnoreCase(CRYPT_MAC_ALGORITHMS[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(CRYPT_MAC_ALGORITHMS))
  {
    (*cryptMACAlgorithm) = CRYPT_MAC_ALGORITHMS[i].cryptMACAlgorithm;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

const char *Crypt_typeToString(CryptTypes cryptType)
{
  const char *typeName = NULL;

  switch (cryptType)
  {
    case CRYPT_TYPE_NONE      : typeName = "NONE";       break;
    case CRYPT_TYPE_SYMMETRIC : typeName = "SYMMETRIC";  break;
    case CRYPT_TYPE_ASYMMETRIC: typeName = "ASYMMETRIC"; break;
  }

  return typeName;
}

void Crypt_randomize(byte *buffer, uint length)
{
  #ifdef HAVE_GCRYPT
  #else /* not HAVE_GCRYPT */
    uint i;
  #endif /* HAVE_GCRYPT */

  assert(buffer != NULL);

  #ifdef HAVE_GCRYPT
    gcry_create_nonce((unsigned char*)buffer,(size_t)length);
  #else /* not HAVE_GCRYPT */
    for (i = 0; i < length; i++)
    {
      buffer[i] = (byte)(random()%256);
    }
  #endif /* HAVE_GCRYPT */
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
Errors Crypt_init(CryptInfo       *cryptInfo,
                  CryptAlgorithms cryptAlgorithm,
                  CryptMode       cryptMode,
                  const CryptSalt *cryptSalt,
                  const CryptKey  *cryptKey
                 )
#else /* not NDEBUG */
Errors __Crypt_init(const char      *__fileName__,
                    ulong           __lineNb__,
                    CryptInfo       *cryptInfo,
                    CryptAlgorithms cryptAlgorithm,
                    CryptMode       cryptMode,
                    const CryptSalt *cryptSalt,
                    const CryptKey  *cryptKey
                   )
#endif /* NDEBUG */
{
  assert(cryptInfo != NULL);

  // init variables
  cryptInfo->cryptAlgorithm = cryptAlgorithm;
  cryptInfo->cryptMode      = cryptMode;
  if (Crypt_isSaltAvailable(cryptSalt))
  {
    Crypt_copySalt(&cryptInfo->cryptSalt,cryptSalt);
  }
  else
  {
    Crypt_clearSalt(&cryptInfo->cryptSalt);
  }

  // init crypt algorithm
  switch (cryptAlgorithm)
  {
    case CRYPT_ALGORITHM_NONE:
      break;
    case CRYPT_ALGORITHM_3DES:
    case CRYPT_ALGORITHM_CAST5:
    case CRYPT_ALGORITHM_BLOWFISH:
    case CRYPT_ALGORITHM_AES128:
    case CRYPT_ALGORITHM_AES192:
    case CRYPT_ALGORITHM_AES256:
    case CRYPT_ALGORITHM_TWOFISH128:
    case CRYPT_ALGORITHM_TWOFISH256:
    case CRYPT_ALGORITHM_SERPENT128:
    case CRYPT_ALGORITHM_SERPENT192:
    case CRYPT_ALGORITHM_SERPENT256:
    case CRYPT_ALGORITHM_CAMELLIA128:
    case CRYPT_ALGORITHM_CAMELLIA192:
    case CRYPT_ALGORITHM_CAMELLIA256:
      #ifdef HAVE_GCRYPT
        {
          int          gcryptAlgorithm;
          int          gcryptMode;
          unsigned int gcryptFlags;
          gcry_error_t gcryptError;
          size_t       n;
          uint         keyLength;

          assert(cryptKey != NULL);

          // get gcrpyt algorithm, gcrypt mode, gcrypt flags
          gcryptAlgorithm = 0;
          switch (cryptAlgorithm)
          {
            case CRYPT_ALGORITHM_3DES:        gcryptAlgorithm = GCRY_CIPHER_3DES;        break;
            case CRYPT_ALGORITHM_CAST5:       gcryptAlgorithm = GCRY_CIPHER_CAST5;       break;
            case CRYPT_ALGORITHM_BLOWFISH:    gcryptAlgorithm = GCRY_CIPHER_BLOWFISH;    break;
            case CRYPT_ALGORITHM_AES128:      gcryptAlgorithm = GCRY_CIPHER_AES;         break;
            case CRYPT_ALGORITHM_AES192:      gcryptAlgorithm = GCRY_CIPHER_AES192;      break;
            case CRYPT_ALGORITHM_AES256:      gcryptAlgorithm = GCRY_CIPHER_AES256;      break;
            case CRYPT_ALGORITHM_TWOFISH128:  gcryptAlgorithm = GCRY_CIPHER_TWOFISH128;  break;
            case CRYPT_ALGORITHM_TWOFISH256:  gcryptAlgorithm = GCRY_CIPHER_TWOFISH;     break;
            case CRYPT_ALGORITHM_SERPENT128:  gcryptAlgorithm = GCRY_CIPHER_SERPENT128;  break;
            case CRYPT_ALGORITHM_SERPENT192:  gcryptAlgorithm = GCRY_CIPHER_SERPENT192;  break;
            case CRYPT_ALGORITHM_SERPENT256:  gcryptAlgorithm = GCRY_CIPHER_SERPENT256;  break;
            case CRYPT_ALGORITHM_CAMELLIA128: gcryptAlgorithm = GCRY_CIPHER_CAMELLIA128; break;
            case CRYPT_ALGORITHM_CAMELLIA192: gcryptAlgorithm = GCRY_CIPHER_CAMELLIA192; break;
            case CRYPT_ALGORITHM_CAMELLIA256: gcryptAlgorithm = GCRY_CIPHER_CAMELLIA256; break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }
          gcryptMode  = ((cryptMode & CRYPT_MODE_CBC_) == CRYPT_MODE_CBC_) ? GCRY_CIPHER_MODE_CBC : GCRY_CIPHER_MODE_NONE;
          gcryptFlags = 0;

          // check if algorithm available
          gcryptError = gcry_cipher_algo_info(gcryptAlgorithm,
                                              GCRYCTL_TEST_ALGO,
                                              NULL,
                                              NULL
                                             );
          if (gcryptError != 0)
          {
            char buffer[128];

            gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
            return ERRORX_(INIT_CIPHER,
                           gcryptError,
                           "cipher '%s' not available: %s",
                           Crypt_algorithmToString(cryptAlgorithm,NULL),
                           buffer
                          );
          }

          // get block length, key length
          gcryptError = gcry_cipher_algo_info(gcryptAlgorithm,
                                              GCRYCTL_GET_BLKLEN,
                                              NULL,
                                              &n
                                             );
          if (gcryptError != 0)
          {
            char buffer[128];

            gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
            return ERRORX_(INIT_CIPHER,
                           gcryptError,
                           "detect block length of '%s': %s",
                           gcry_cipher_algo_name(gcryptAlgorithm),
                           buffer
                          );
          }
          cryptInfo->blockLength = n;
          gcryptError = gcry_cipher_algo_info(gcryptAlgorithm,
                                              GCRYCTL_GET_KEYLEN,
                                              NULL,
                                              &n
                                             );
          if (gcryptError != 0)
          {
            char buffer[128];

            gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
            return ERRORX_(INIT_CIPHER,
                           gcryptError,
                           "detect key length of '%s': %s",
                           gcry_cipher_algo_name(gcryptAlgorithm),
                           buffer
                          );
          }
          keyLength = (uint)(n*8);
//fprintf(stderr,"%s, %d: blockLength=%d\n",__FILE__,__LINE__,cryptInfo->blockLength);

          // check key
          if (!Crypt_isKeyAvailable(cryptKey))
          {
            return ERROR_NO_CRYPT_KEY;
          }
          if (keyLength > cryptKey->dataLength*8)
          {
            return ERROR_INVALID_KEY_LENGTH;
          }

          // init cipher
          gcryptError = gcry_cipher_open(&cryptInfo->gcry_cipher_hd,
                                         gcryptAlgorithm,
                                         gcryptMode,
                                         gcryptFlags
                                        );
          if (gcryptError != 0)
          {
            char buffer[128];

            gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
            return ERRORX_(INIT_CIPHER,
                           gcryptError,
                           "'%s': %s",
                           gcry_cipher_algo_name(gcryptAlgorithm),
                           buffer
                          );
          }

          // set key (Note: use correct key length which may be smaller than provided crypt key length)
#ifndef NDEBUG
//fprintf(stderr,"%s, %d: crypt key %d\n",__FILE__,__LINE__,cryptKey->dataLength); debugDumpMemory(cryptKey->data,cryptKey->dataLength,0);
#endif
          gcryptError = gcry_cipher_setkey(cryptInfo->gcry_cipher_hd,
                                           cryptKey->data,
                                           keyLength/8
                                          );
          if (gcryptError != 0)
          {
            char buffer[128];

            gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
            gcry_cipher_close(cryptInfo->gcry_cipher_hd);
            return ERRORX_(INIT_CIPHER,
                           gcryptError,
                           "set key for '%s' with %dbit: %s",
                           gcry_cipher_algo_name(gcryptAlgorithm),
                           cryptKey->dataLength*8,
                           buffer
                          );
          }

          // set salt as IV
          if (Crypt_isSaltAvailable(&cryptInfo->cryptSalt))
          {
            if (cryptInfo->cryptSalt.dataLength < cryptInfo->blockLength)
            {
              gcry_cipher_close(cryptInfo->gcry_cipher_hd);
              return ERROR_INVALID_SALT_LENGTH;
            }
            gcryptError = gcry_cipher_setiv(cryptInfo->gcry_cipher_hd,
                                            cryptInfo->cryptSalt.data,
                                            cryptInfo->blockLength
                                           );
            if (gcryptError != 0)
            {
              char buffer[128];

              gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
              gcry_cipher_close(cryptInfo->gcry_cipher_hd);
              return ERRORX_(INIT_CIPHER,gcryptError,"set IV: %s",buffer);
            }
          }
        }
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(cryptInfo);
        UNUSED_VARIABLE(cryptAlgorithm);
        UNUSED_VARIABLE(cryptMode);
        UNUSED_VARIABLE(cryptSalt);
        UNUSED_VARIABLE(cryptKey);

        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(cryptInfo,CryptInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptInfo,CryptInfo);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
void Crypt_done(CryptInfo *cryptInfo)
#else /* not NDEBUG */
void __Crypt_done(const char *__fileName__,
                  ulong      __lineNb__,
                  CryptInfo  *cryptInfo
                 )
#endif /* NDEBUG */
{
  assert(cryptInfo != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(cryptInfo,CryptInfo);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptInfo,CryptInfo);
  #endif /* NDEBUG */

  switch (cryptInfo->cryptAlgorithm)
  {
    case CRYPT_ALGORITHM_NONE:
      break;
    case CRYPT_ALGORITHM_3DES:
    case CRYPT_ALGORITHM_CAST5:
    case CRYPT_ALGORITHM_BLOWFISH:
    case CRYPT_ALGORITHM_AES128:
    case CRYPT_ALGORITHM_AES192:
    case CRYPT_ALGORITHM_AES256:
    case CRYPT_ALGORITHM_TWOFISH128:
    case CRYPT_ALGORITHM_TWOFISH256:
    case CRYPT_ALGORITHM_SERPENT128:
    case CRYPT_ALGORITHM_SERPENT192:
    case CRYPT_ALGORITHM_SERPENT256:
    case CRYPT_ALGORITHM_CAMELLIA128:
    case CRYPT_ALGORITHM_CAMELLIA192:
    case CRYPT_ALGORITHM_CAMELLIA256:
      #ifdef HAVE_GCRYPT
        gcry_cipher_close(cryptInfo->gcry_cipher_hd);
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
}

#ifdef NDEBUG
CryptInfo *Crypt_new(CryptAlgorithms cryptAlgorithm,
                     CryptMode       cryptMode,
                     const CryptSalt *cryptSalt,
                     const CryptKey  *cryptKey
                    )
#else /* not NDEBUG */
CryptInfo *__Crypt_new(const char      *__fileName__,
                       ulong           __lineNb__,
                       CryptAlgorithms cryptAlgorithm,
                       CryptMode       cryptMode,
                       const CryptSalt *cryptSalt,
                       const CryptKey  *cryptKey
                      )
#endif /* NDEBUG */
{
  CryptInfo *cryptInfo;

  cryptInfo = (CryptInfo*)malloc(sizeof(CryptInfo));
  if (cryptInfo == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  #ifndef NDEBUG
    __Crypt_init(__fileName__,__lineNb__,cryptInfo,cryptAlgorithm,cryptMode,cryptSalt,cryptKey);
  #else /* not NDEBUG */
    Crypt_init(cryptInfo,cryptAlgorithm,cryptMode,cryptSalt,cryptKey);
  #endif /* NDEBUG */

  return cryptInfo;
}

#ifdef NDEBUG
void Crypt_delete(CryptInfo *cryptInfo)
#else /* not NDEBUG */
void __Crypt_delete(const char *__fileName__,
                    ulong      __lineNb__,
                    CryptInfo  *cryptInfo
                   )
#endif /* NDEBUG */
{
  if (cryptInfo != NULL)
  {
    #ifndef NDEBUG
      __Crypt_done(__fileName__,__lineNb__,cryptInfo);
    #else /* not NDEBUG */
      Crypt_done(cryptInfo);
    #endif /* NDEBUG */
    free(cryptInfo);
  }
}

Errors Crypt_reset(CryptInfo *cryptInfo)
{

  assert(cryptInfo != NULL);

  switch (cryptInfo->cryptAlgorithm)
  {
    case CRYPT_ALGORITHM_NONE:
      break;
    case CRYPT_ALGORITHM_3DES:
    case CRYPT_ALGORITHM_CAST5:
    case CRYPT_ALGORITHM_BLOWFISH:
    case CRYPT_ALGORITHM_AES128:
    case CRYPT_ALGORITHM_AES192:
    case CRYPT_ALGORITHM_AES256:
    case CRYPT_ALGORITHM_TWOFISH128:
    case CRYPT_ALGORITHM_TWOFISH256:
    case CRYPT_ALGORITHM_SERPENT128:
    case CRYPT_ALGORITHM_SERPENT192:
    case CRYPT_ALGORITHM_SERPENT256:
    case CRYPT_ALGORITHM_CAMELLIA128:
    case CRYPT_ALGORITHM_CAMELLIA192:
    case CRYPT_ALGORITHM_CAMELLIA256:
      #ifdef HAVE_GCRYPT
        {
          gcry_error_t gcryptError;

          gcry_cipher_reset(cryptInfo->gcry_cipher_hd);

          // set salt as IV
          if (Crypt_isSaltAvailable(&cryptInfo->cryptSalt))
          {
            if (cryptInfo->cryptSalt.dataLength < cryptInfo->blockLength)
            {
              return ERROR_INVALID_SALT_LENGTH;
            }
            gcryptError = gcry_cipher_setiv(cryptInfo->gcry_cipher_hd,
                                            cryptInfo->cryptSalt.data,
                                            cryptInfo->blockLength
                                         );
            if (gcryptError != 0)
            {
              char buffer[128];

              gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
              return ERRORX_(INIT_CIPHER,gcryptError,"set IV: %s",buffer);
            }
          }
        }
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(cryptInfo);

        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return ERROR_NONE;
}

Errors Crypt_encrypt(CryptInfo *cryptInfo,
                     void      *buffer,
                     ulong     bufferLength
                    )
{
  #ifdef HAVE_GCRYPT
    gcry_error_t gcryptError;
  #endif

  assert(cryptInfo != NULL);
  assert(buffer != NULL);

  switch (cryptInfo->cryptAlgorithm)
  {
    case CRYPT_ALGORITHM_NONE:
      break;
    case CRYPT_ALGORITHM_3DES:
    case CRYPT_ALGORITHM_CAST5:
    case CRYPT_ALGORITHM_BLOWFISH:
    case CRYPT_ALGORITHM_AES128:
    case CRYPT_ALGORITHM_AES192:
    case CRYPT_ALGORITHM_AES256:
    case CRYPT_ALGORITHM_TWOFISH128:
    case CRYPT_ALGORITHM_TWOFISH256:
    case CRYPT_ALGORITHM_SERPENT128:
    case CRYPT_ALGORITHM_SERPENT192:
    case CRYPT_ALGORITHM_SERPENT256:
    case CRYPT_ALGORITHM_CAMELLIA128:
    case CRYPT_ALGORITHM_CAMELLIA192:
    case CRYPT_ALGORITHM_CAMELLIA256:
      #ifdef HAVE_GCRYPT
        assert(cryptInfo->blockLength > 0);
        assert((bufferLength%cryptInfo->blockLength) == 0);

        gcryptError = gcry_cipher_cts(cryptInfo->gcry_cipher_hd,
                                      IS_SET(cryptInfo->cryptMode,CRYPT_MODE_CTS_)
                                     );
        if (gcryptError != 0)
        {
          char buffer[128];

          gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
          return ERRORX_(ENCRYPT,gcryptError,"%s",buffer);
        }

        gcryptError = gcry_cipher_encrypt(cryptInfo->gcry_cipher_hd,
                                          buffer,
                                          bufferLength,
                                          NULL,
                                          0
                                         );
        if (gcryptError != 0)
        {
          char buffer[128];

          gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
          return ERRORX_(ENCRYPT,gcryptError,"%s",buffer);
        }
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(buffer);
        UNUSED_VARIABLE(bufferLength);

        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return ERROR_NONE;
}

Errors Crypt_decrypt(CryptInfo *cryptInfo,
                     void      *buffer,
                     ulong     bufferLength
                    )
{
  #ifdef HAVE_GCRYPT
    gcry_error_t gcryptError;
  #endif

  assert(cryptInfo != NULL);
  assert(buffer != NULL);

  switch (cryptInfo->cryptAlgorithm)
  {
    case CRYPT_ALGORITHM_NONE:
      break;
    case CRYPT_ALGORITHM_3DES:
    case CRYPT_ALGORITHM_CAST5:
    case CRYPT_ALGORITHM_BLOWFISH:
    case CRYPT_ALGORITHM_AES128:
    case CRYPT_ALGORITHM_AES192:
    case CRYPT_ALGORITHM_AES256:
    case CRYPT_ALGORITHM_TWOFISH128:
    case CRYPT_ALGORITHM_TWOFISH256:
    case CRYPT_ALGORITHM_SERPENT128:
    case CRYPT_ALGORITHM_SERPENT192:
    case CRYPT_ALGORITHM_SERPENT256:
    case CRYPT_ALGORITHM_CAMELLIA128:
    case CRYPT_ALGORITHM_CAMELLIA192:
    case CRYPT_ALGORITHM_CAMELLIA256:
      #ifdef HAVE_GCRYPT
        assert(cryptInfo->blockLength > 0);
        assert((bufferLength%cryptInfo->blockLength) == 0);

        gcryptError = gcry_cipher_cts(cryptInfo->gcry_cipher_hd,
                                      IS_SET(cryptInfo->cryptMode,CRYPT_MODE_CTS_)
                                     );
        if (gcryptError != 0)
        {
          char buffer[128];

          gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
          return ERRORX_(ENCRYPT,gcryptError,"%s",buffer);
        }

        gcryptError = gcry_cipher_decrypt(cryptInfo->gcry_cipher_hd,
                                          buffer,
                                          bufferLength,
                                          NULL,
                                          0
                                         );
        if (gcryptError != 0)
        {
          char buffer[128];

          gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
          return ERRORX_(ENCRYPT,gcryptError,"%s",buffer);
        }
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(buffer);
        UNUSED_VARIABLE(bufferLength);

        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return ERROR_NONE;
}

//TODO    : remove
Errors Crypt_encryptBytes(CryptInfo *cryptInfo,
                          void      *buffer,
                          ulong     bufferLength
                         )
{
  #ifdef HAVE_GCRYPT
    gcry_error_t gcryptError;
  #endif

  assert(cryptInfo != NULL);
  assert(buffer != NULL);

  switch (cryptInfo->cryptAlgorithm)
  {
    case CRYPT_ALGORITHM_NONE:
      break;
    case CRYPT_ALGORITHM_3DES:
    case CRYPT_ALGORITHM_CAST5:
    case CRYPT_ALGORITHM_BLOWFISH:
    case CRYPT_ALGORITHM_AES128:
    case CRYPT_ALGORITHM_AES192:
    case CRYPT_ALGORITHM_AES256:
    case CRYPT_ALGORITHM_TWOFISH128:
    case CRYPT_ALGORITHM_TWOFISH256:
    case CRYPT_ALGORITHM_SERPENT128:
    case CRYPT_ALGORITHM_SERPENT192:
    case CRYPT_ALGORITHM_SERPENT256:
    case CRYPT_ALGORITHM_CAMELLIA128:
    case CRYPT_ALGORITHM_CAMELLIA192:
    case CRYPT_ALGORITHM_CAMELLIA256:
      #ifdef HAVE_GCRYPT
        assert(cryptInfo->blockLength > 0);
        assert((bufferLength%cryptInfo->blockLength) == 0);

        gcryptError = gcry_cipher_cts(cryptInfo->gcry_cipher_hd,
                                      IS_SET(cryptInfo->cryptMode,CRYPT_MODE_CTS_DATA)
                                     );
        if (gcryptError != 0)
        {
          char buffer[128];

          gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
          return ERRORX_(ENCRYPT,gcryptError,"%s",buffer);
        }

        gcryptError = gcry_cipher_encrypt(cryptInfo->gcry_cipher_hd,
                                          buffer,
                                          bufferLength,
                                          NULL,
                                          0
                                         );
        if (gcryptError != 0)
        {
          char buffer[128];

          gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
          return ERRORX_(ENCRYPT,gcryptError,"%s",buffer);
        }
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(buffer);
        UNUSED_VARIABLE(bufferLength);

        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return ERROR_NONE;
}

Errors Crypt_decryptBytes(CryptInfo *cryptInfo,
                          void      *buffer,
                          ulong     bufferLength
                         )
{
  #ifdef HAVE_GCRYPT
    gcry_error_t gcryptError;
  #endif

  assert(cryptInfo != NULL);
  assert(buffer != NULL);

  switch (cryptInfo->cryptAlgorithm)
  {
    case CRYPT_ALGORITHM_NONE:
      break;
    case CRYPT_ALGORITHM_3DES:
    case CRYPT_ALGORITHM_CAST5:
    case CRYPT_ALGORITHM_BLOWFISH:
    case CRYPT_ALGORITHM_AES128:
    case CRYPT_ALGORITHM_AES192:
    case CRYPT_ALGORITHM_AES256:
    case CRYPT_ALGORITHM_TWOFISH128:
    case CRYPT_ALGORITHM_TWOFISH256:
    case CRYPT_ALGORITHM_SERPENT128:
    case CRYPT_ALGORITHM_SERPENT192:
    case CRYPT_ALGORITHM_SERPENT256:
    case CRYPT_ALGORITHM_CAMELLIA128:
    case CRYPT_ALGORITHM_CAMELLIA192:
    case CRYPT_ALGORITHM_CAMELLIA256:
      #ifdef HAVE_GCRYPT
        assert(cryptInfo->blockLength > 0);
        assert((bufferLength%cryptInfo->blockLength) == 0);

        gcryptError = gcry_cipher_cts(cryptInfo->gcry_cipher_hd,
                                      IS_SET(cryptInfo->cryptMode,CRYPT_MODE_CTS_DATA)
                                     );
        if (gcryptError != 0)
        {
          char buffer[128];

          gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
          return ERRORX_(ENCRYPT,gcryptError,"%s",buffer);
        }

        gcryptError = gcry_cipher_decrypt(cryptInfo->gcry_cipher_hd,
                                          buffer,
                                          bufferLength,
                                          NULL,
                                          0
                                         );
        if (gcryptError != 0)
        {
          char buffer[128];

          gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
          return ERRORX_(ENCRYPT,gcryptError,"%s",buffer);
        }
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(buffer);
        UNUSED_VARIABLE(bufferLength);

        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
  void Crypt_initKey(CryptKey          *cryptKey,
                     CryptPaddingTypes cryptPaddingType
                    )
#else /* not NDEBUG */
  void __Crypt_initKey(const char        *__fileName__,
                       ulong             __lineNb__,
                       CryptKey          *cryptKey,
                       CryptPaddingTypes cryptPaddingType
                      )
#endif /* NDEBUG */
{
  assert(cryptKey != NULL);

  // set key
  cryptKey->cryptPaddingType = cryptPaddingType;
  cryptKey->data             = NULL;
  cryptKey->dataLength       = 0;
  #ifdef HAVE_GCRYPT
    cryptKey->key = NULL;
  #else /* not HAVE_GCRYPT */
  #endif /* HAVE_GCRYPT */

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(cryptKey,CryptKey);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptKey,CryptKey);
  #endif /* NDEBUG */
}

#ifdef NDEBUG
  Errors Crypt_duplicateKey(CryptKey       *cryptKey,
                            const CryptKey *fromCryptKey
                           )
#else /* not NDEBUG */
  Errors __Crypt_duplicateKey(const char     *__fileName__,
                              ulong          __lineNb__,
                              CryptKey       *cryptKey,
                              const CryptKey *fromCryptKey
                             )
#endif /* NDEBUG */
{
  uint dataLength;
  void *data;

  // allocate secure memory
  dataLength = fromCryptKey->dataLength;
  data       = allocSecure(dataLength);
  if (data == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }
  memCopyFast(data,dataLength,fromCryptKey->data,fromCryptKey->dataLength);

  // set key
  cryptKey->cryptPaddingType = fromCryptKey->cryptPaddingType;
  cryptKey->data             = data;
  cryptKey->dataLength       = dataLength;
  #ifdef HAVE_GCRYPT
    cryptKey->key = NULL;
    if (fromCryptKey->key != NULL)
    {
      if (cryptKey->key == NULL) cryptKey->key = gcry_sexp_find_token(fromCryptKey->key,"public-key",0);
      if (cryptKey->key == NULL) cryptKey->key = gcry_sexp_find_token(fromCryptKey->key,"private-key",0);
    }
  #endif /* HAVE_GCRYPT */

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(cryptKey,CryptKey);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptKey,CryptKey);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  void Crypt_doneKey(CryptKey *cryptKey)
#else /* not NDEBUG */
  void __Crypt_doneKey(const char *__fileName__,
                       ulong      __lineNb__,
                       CryptKey   *cryptKey
                      )
#endif /* NDEBUG */
{
  assert(cryptKey != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(cryptKey,CryptKey);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptKey,CryptKey);
  #endif /* NDEBUG */

  #ifdef HAVE_GCRYPT
    if (cryptKey->key != NULL) gcry_sexp_release(cryptKey->key);
  #else /* not HAVE_GCRYPT */
  #endif /* HAVE_GCRYPT */
  if (cryptKey->data != NULL)
  {
    freeSecure(cryptKey->data);
  }
}

#ifdef NDEBUG
  CryptKey *Crypt_newKey(CryptPaddingTypes cryptPaddingType)
#else /* not NDEBUG */
  CryptKey *__Crypt_newKey(const char        *__fileName__,
                           ulong             __lineNb__,
                           CryptPaddingTypes cryptPaddingType
                          )
#endif /* NDEBUG */
{
  CryptKey *cryptKey;

  cryptKey = (CryptKey*)malloc(sizeof(CryptKey));
  if (cryptKey == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  #ifndef NDEBUG
    __Crypt_initKey(__fileName__,__lineNb__,cryptKey,cryptPaddingType);
  #else /* not NDEBUG */
    Crypt_initKey(cryptKey,cryptPaddingType);
  #endif /* NDEBUG */

  return cryptKey;
}

#ifdef NDEBUG
  void Crypt_deleteKey(CryptKey *cryptKey)
#else /* not NDEBUG */
  void __Crypt_deleteKey(const char        *__fileName__,
                         ulong             __lineNb__,
                         CryptKey          *cryptKey
                        )
#endif /* NDEBUG */
{
  if (cryptKey != NULL)
  {
    #ifndef NDEBUG
      __Crypt_doneKey(__fileName__,__lineNb__,cryptKey);
    #else /* not NDEBUG */
      Crypt_doneKey(cryptKey);
    #endif /* NDEBUG */
    free(cryptKey);
  }
}

void Crypt_clearKey(CryptKey *cryptKey)
{
  assert(cryptKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(cryptKey);

  #ifdef HAVE_GCRYPT
    if (cryptKey->key != NULL)
    {
      gcry_sexp_release(cryptKey->key);
      cryptKey->key = NULL;
    }
  #else /* not HAVE_GCRYPT */
  #endif /* HAVE_GCRYPT */
  if (cryptKey->data != NULL)
  {
    freeSecure(cryptKey->data);
    cryptKey->data = NULL;
  }
}

Errors Crypt_deriveKey(CryptKey            *cryptKey,
                       CryptKeyDeriveTypes cryptKeyDeriveType,
                       const CryptSalt     *cryptSalt,
                       const Password      *password,
                       uint                keyLength
                      )
{
  void *data;
  uint dataLength;
  uint i;
  #ifdef HAVE_GCRYPT
    gcry_error_t gcryptError;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(cryptKey);
  assert((password == NULL) || (cryptKeyDeriveType != CRYPT_KEY_DERIVE_NONE));

  // allocate secure memory
  dataLength = ALIGN(keyLength,8)/8;
  data       = allocSecure(dataLength);
  if (data == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }

  // derive key
  switch (cryptKeyDeriveType)
  {
    case CRYPT_KEY_DERIVE_NONE:
      break;
    case CRYPT_KEY_DERIVE_SIMPLE:
      for (i = 0; i < dataLength; i++)
      {
        ((byte*)data)[i] = Password_getChar(password,i);
      }
      break;
    case CRYPT_KEY_DERIVE_FUNCTION:
      #ifdef HAVE_GCRYPT
        assert(dataLength < sizeof(NO_SALT));

        gcryptError = 0;
        PASSWORD_DEPLOY_DO(plainPassword,password)
        {
          gcryptError = gcry_kdf_derive(plainPassword,
                                        (size_t)Password_length(password),
                                        KEY_DERIVE_ALGORITHM,
                                        KEY_DERIVE_HASH_ALGORITHM,
                                        (cryptSalt != NULL) ? cryptSalt->data : NO_SALT,
                                        (cryptSalt != NULL) ? cryptSalt->dataLength : sizeof(NO_SALT),
                                        KEY_DERIVE_ITERATIONS,
                                        dataLength,
                                        data
                                       );
        }
        if (gcryptError != 0)
        {
          char buffer[128];

          gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
          freeSecure(data);
          return ERRORX_(INIT_KEY,gcryptError,"%s",buffer);
        }
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(NO_SALT);

        UNUSED_VARIABLE(cryptKey);
        UNUSED_VARIABLE(cryptKeyDeriveType);
        UNUSED_VARIABLE(cryptSalt);
        UNUSED_VARIABLE(password);
        UNUSED_VARIABLE(keyLength);

        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GCRYPT */
      break;
  }

  // set key
  if (cryptKey->data != NULL) freeSecure(cryptKey->data);
  cryptKey->data       = data;
  cryptKey->dataLength = dataLength;
  #ifdef HAVE_GCRYPT
    if (cryptKey->key != NULL) gcry_sexp_release(cryptKey->key);
    cryptKey->key = NULL;
  #endif /* HAVE_GCRYPT */

  return ERROR_NONE;
}

Errors Crypt_getPublicPrivateKeyData(CryptKey            *cryptKey,
                                     void                **encryptedKeyData,
                                     uint                *encryptedKeyDataLength,
                                     uint                cryptMode,
                                     CryptKeyDeriveTypes cryptKeyDeriveType,
                                     const CryptSalt     *cryptSalt,
                                     const Password      *password
                                    )
{
  #ifdef HAVE_GCRYPT
    uint                blockLength;
    gcry_sexp_t         sexpToken;
    uint                keyDataLength;
    uint                alignedKeyDataLength;
    void                *keyData;
    uint                keyLength;
    CryptKey            encryptKey;
    CryptInfo           cryptInfo;
    Errors              error;
    uint32              crc;
    KeyImportExportInfo *keyImportExportInfo;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(cryptKey);
  assert(encryptedKeyData != NULL);
  assert(encryptedKeyDataLength != NULL);

  #ifdef HAVE_GCRYPT
    // get crypt algorithm block length
    blockLength = Crypt_getBlockLength(SECRET_KEY_CRYPT_ALGORITHM);

    // find key token (public/private)
    sexpToken = NULL;
    if (sexpToken == NULL) sexpToken = gcry_sexp_find_token(cryptKey->key,"public-key",0);
    if (sexpToken == NULL) sexpToken = gcry_sexp_find_token(cryptKey->key,"private-key",0);
    if (sexpToken == NULL)
    {
      return ERROR_INVALID_KEY;
    }

    // get data length, aligned data length
    keyDataLength        = (uint)gcry_sexp_sprint(sexpToken,GCRYSEXP_FMT_ADVANCED,NULL,0);
    alignedKeyDataLength = ALIGN(keyDataLength,blockLength);
    assert(alignedKeyDataLength >= keyDataLength);

    // allocate key data
    keyData = allocSecure(alignedKeyDataLength);
    if (keyData == NULL)
    {
      gcry_sexp_release(sexpToken);
      return ERROR_INSUFFICIENT_MEMORY;
    }

    // get key data
    gcry_sexp_sprint(sexpToken,GCRYSEXP_FMT_ADVANCED,(char*)keyData,keyDataLength);
    gcry_sexp_release(sexpToken);
    memClear((byte*)keyData+keyDataLength,alignedKeyDataLength-keyDataLength);
    #ifdef DEBUG_ASYMMETRIC_CRYPT
      fprintf(stderr,"%s, %d: %d raw key\n",__FILE__,__LINE__,keyDataLength); debugDumpMemory(keyData,alignedKeyDataLength,FALSE);
    #endif

    // encrypt key data (if password given)
    if (password != NULL)
    {
      // get required key length for algorithm
      keyLength = Crypt_getKeyLength(SECRET_KEY_CRYPT_ALGORITHM);

      // initialize crypt
      Crypt_initKey(&encryptKey,CRYPT_PADDING_TYPE_NONE);
      error = Crypt_deriveKey(&encryptKey,
                              cryptKeyDeriveType,
                              cryptSalt,
                              password,
                              keyLength
                             );
      if (error != ERROR_NONE)
      {
        Crypt_doneKey(&encryptKey);
        freeSecure(keyData);
        return error;
      }
      error = Crypt_init(&cryptInfo,
                         SECRET_KEY_CRYPT_ALGORITHM,
                         cryptMode,
                         cryptSalt,
                         &encryptKey
                        );
      if (error != ERROR_NONE)
      {
        Crypt_doneKey(&encryptKey);
        freeSecure(keyData);
        return error;
      }

      // encrypt
      error = Crypt_encrypt(&cryptInfo,(char*)keyData,alignedKeyDataLength);
      if (error != ERROR_NONE)
      {
        Crypt_done(&cryptInfo);
        Crypt_doneKey(&encryptKey);
        freeSecure(keyData);
        return error;
      }

      // done crypt
      Crypt_done(&cryptInfo);
      Crypt_doneKey(&encryptKey);

      #ifdef DEBUG_ASYMMETRIC_CRYPT
      {
        CryptKey  cryptKey;
        CryptInfo cryptInfo;
        void     *data;

        Crypt_initKey(&cryptKey,CRYPT_PADDING_TYPE_NONE);
        Crypt_deriveKey(&cryptKey,
                        cryptKeyDeriveType,
                        cryptSalt,
                        password,
                        keyLength
                       );
        Crypt_init(&cryptInfo,
                   SECRET_KEY_CRYPT_ALGORITHM,
                   cryptMode,
                   cryptSalt,
                   &cryptKey
                  );

        data = malloc(alignedKeyDataLength);
        memCopyFast(data,alignedKeyDataLength,keyData,alignedKeyDataLength);
        Crypt_decrypt(&cryptInfo,(char*)data,alignedKeyDataLength);
//fprintf(stderr,"%s, %d: decrypted key\n",__FILE__,__LINE__);
//debugDumpMemory(data,alignedKeyDataLength,0);
        free(data);

        Crypt_done(&cryptInfo);
        Crypt_doneKey(&cryptKey);
      }
      #endif
    }
    #ifdef DEBUG_ASYMMETRIC_CRYPT
      fprintf(stderr,"%s, %d: %d encrypted key\n",__FILE__,__LINE__,dataLength); debugDumpMemory(encryptedKeyInfo->data,alignedKeyDataLength,FALSE);
    #endif

    // calculate CRC
    crc = crc32(crc32(0,Z_NULL,0),keyData,alignedKeyDataLength);

    // get encrypted key export data
    keyImportExportInfo = allocSecure(sizeof(KeyImportExportInfo)+alignedKeyDataLength);
    if (keyImportExportInfo == NULL)
    {
      freeSecure(keyData);
      return ERROR_INSUFFICIENT_MEMORY;
    }
    keyImportExportInfo->crc        = htonl(crc);
    keyImportExportInfo->dataLength = htonl(keyDataLength);
    memCopyFast(keyImportExportInfo->data,alignedKeyDataLength,keyData,alignedKeyDataLength);

    (*encryptedKeyData)       = keyImportExportInfo;
    (*encryptedKeyDataLength) = sizeof(KeyImportExportInfo)+alignedKeyDataLength;

    // free resources
    freeSecure(keyData);

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);
    UNUSED_VARIABLE(encryptedKeyData);
    UNUSED_VARIABLE(encryptedKeyDataLength);
    UNUSED_VARIABLE(cryptMode);
    UNUSED_VARIABLE(cryptKeyDeriveType);
    UNUSED_VARIABLE(cryptSalt);
    UNUSED_VARIABLE(password);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_setPublicPrivateKeyData(CryptKey            *cryptKey,
                                     const void          *encryptedKeyData,
                                     uint                encryptedKeyDataLength,
                                     uint                cryptMode,
                                     CryptKeyDeriveTypes cryptKeyDeriveType,
                                     const CryptSalt     *cryptSalt,
                                     const Password      *password
                                    )
{
  #ifdef HAVE_GCRYPT
    uint                      blockLength;
    const KeyImportExportInfo *keyImportExportInfo;
    uint                      keyDataLength;
    uint                      alignedKeyDataLength;
    void                      *keyData;
    uint32                    crc;
    uint                      keyLength;
    CryptKey                  encryptKey;
    CryptInfo                 cryptInfo;
    Errors                    error;
    gcry_error_t              gcryptError;
    gcry_sexp_t               key;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(cryptKey);
  assert(encryptedKeyData != NULL);

  #ifdef HAVE_GCRYPT
    // get crypt algorithm block length
    blockLength = Crypt_getBlockLength(SECRET_KEY_CRYPT_ALGORITHM);

    keyImportExportInfo = (const KeyImportExportInfo*)encryptedKeyData;

    // check min. key length
    if (encryptedKeyDataLength < sizeof(KeyImportExportInfo))
    {
      return ERROR_INVALID_KEY;
    }

    // check CRC
    crc = crc32(crc32(0,Z_NULL,0),(Bytef*)keyImportExportInfo->data,encryptedKeyDataLength-sizeof(KeyImportExportInfo));
    if (crc != ntohl(keyImportExportInfo->crc))
    {
      return ERROR_CORRUPT_KEY;
    }

    // get length
    keyDataLength = ntohl(keyImportExportInfo->dataLength);
    if (keyDataLength > encryptedKeyDataLength-sizeof(KeyImportExportInfo))
    {
      return ERROR_INVALID_KEY;
    }

    // get aligned data length
    alignedKeyDataLength = ALIGN(keyDataLength,blockLength);
    assert(alignedKeyDataLength >= keyDataLength);
    if ((sizeof(KeyImportExportInfo)+alignedKeyDataLength) > encryptedKeyDataLength)
    {
      return ERROR_INVALID_KEY;
    }

    // allocate secure memory and get key data
    keyData = allocSecure(alignedKeyDataLength);
    if (keyData == NULL)
    {
      return ERROR_INSUFFICIENT_MEMORY;
    }
    memCopyFast(keyData,alignedKeyDataLength,keyImportExportInfo->data,alignedKeyDataLength);
    #ifdef DEBUG_ASYMMETRIC_CRYPT
      fprintf(stderr,"%s, %d: encrypted private key\n",__FILE__,__LINE__); debugDumpMemory(keyData,alignedKeyDataLength,FALSE);
    #endif

    // decrypt key
    if (password != NULL)
    {
      // get required key length for algorithm
      keyLength = Crypt_getKeyLength(SECRET_KEY_CRYPT_ALGORITHM);

      // initialize crypt
      Crypt_initKey(&encryptKey,CRYPT_PADDING_TYPE_NONE);
      error = Crypt_deriveKey(&encryptKey,
                              cryptKeyDeriveType,
                              cryptSalt,
                              password,
                              keyLength
                             );
      if (error != ERROR_NONE)
      {
        Crypt_doneKey(&encryptKey);
        freeSecure(keyData);
        return error;
      }
      #ifdef DEBUG_ASYMMETRIC_CRYPT
//fprintf(stderr,"%s, %d: derived key %d\n",__FILE__,__LINE__,encryptKey.dataLength); debugDumpMemory(encryptKey.data,encryptKey.dataLength,FALSE);
        fprintf(stderr,"%s, %d: derived key %d\n",__FILE__,__LINE__,encryptKey.dataLength); debugDumpMemory(data,encryptKey.dataLength,FALSE);
      #endif
      error = Crypt_init(&cryptInfo,
                         SECRET_KEY_CRYPT_ALGORITHM,
                         cryptMode,
                         cryptSalt,
                         &encryptKey
                        );
      if (error != ERROR_NONE)
      {
        Crypt_doneKey(&encryptKey);
        freeSecure(keyData);
        return error;
      }

      // decrypt
      error = Crypt_decrypt(&cryptInfo,keyData,alignedKeyDataLength);
      if (error != ERROR_NONE)
      {
        Crypt_done(&cryptInfo);
        freeSecure(keyData);
        return error;
      }

      // done crypt
      Crypt_done(&cryptInfo);
      Crypt_doneKey(&encryptKey);
    }
    #ifdef DEBUG_ASYMMETRIC_CRYPT
      fprintf(stderr,"%s, %d: decrypted private key\n",__FILE__,__LINE__); debugDumpMemory(data,alignedKeyDataLength,FALSE);
    #endif

    // create key
// TODO: use somethign like this? gcryptError = gcry_sexp_build(&key,NULL,"(private-key (rsa (n %m) (e %m)))",nToken,eToken);
    gcryptError = gcry_sexp_new(&key,
                                keyData,
                                0,  //dataLength,
                                1  // autodetect
                               );
    if (gcryptError != 0)
    {
      char buffer[128];

      gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
      freeSecure(keyData);
      return ERRORX_(INVALID_KEY,gcryptError,"%s",buffer);
    }
    assert(key != NULL);

    // set key
    if (cryptKey->data != NULL) freeSecure(cryptKey->data);
    cryptKey->data       = keyData;
    cryptKey->dataLength = keyDataLength;
    if (cryptKey->key != NULL) gcry_sexp_release(cryptKey->key);
    cryptKey->key = key;

    // free resources

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);
    UNUSED_VARIABLE(encryptedKeyData);
    UNUSED_VARIABLE(encryptedKeyDataLength);
    UNUSED_VARIABLE(cryptMode);
    UNUSED_VARIABLE(cryptKeyDeriveType);
    UNUSED_VARIABLE(cryptSalt);
    UNUSED_VARIABLE(password);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

bool Crypt_getPublicKeyModulusExponent(CryptKey *cryptKey,
                                       String   modulus,
                                       String   exponent
                                      )
{
  #ifdef HAVE_GCRYPT
    gcry_sexp_t   sexpToken;
    gcry_sexp_t   rsaToken;
    gcry_sexp_t   nToken,eToken;
    gcry_mpi_t    n,e;
    unsigned char *s;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(cryptKey);

  #ifdef HAVE_GCRYPT
    // find key token
    sexpToken = gcry_sexp_find_token(cryptKey->key,"public-key",0);
    if (sexpToken == NULL)
    {
      return FALSE;
    }

    // get RSA, modulus/exponent token
    rsaToken = gcry_sexp_find_token(sexpToken,"rsa",0);
    assert(rsaToken != NULL);
    nToken   = gcry_sexp_find_token(rsaToken,"n",0);
    assert(nToken != NULL);
    eToken   = gcry_sexp_find_token(rsaToken,"e",0);
    assert(eToken != NULL);

    // get modulus/exponent number
    n = gcry_sexp_nth_mpi(nToken,1,GCRYMPI_FMT_USG);
    e = gcry_sexp_nth_mpi(eToken,1,GCRYMPI_FMT_USG);

    // get strings
    if (modulus != NULL)
    {
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,n);
      String_setCString(modulus,(const char*)s);
      free(s);
    }
    if (exponent != NULL)
    {
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,e);
      String_setCString(exponent,(const char*)s);
      free(s);
    }

    // free resources
    gcry_mpi_release(e);
    gcry_mpi_release(n);
    gcry_sexp_release(eToken);
    gcry_sexp_release(nToken);
    gcry_sexp_release(rsaToken);
    gcry_sexp_release(sexpToken);
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);
    UNUSED_VARIABLE(modulus);
    UNUSED_VARIABLE(exponent);
  #endif /* HAVE_GCRYPT */

  return TRUE;
}

bool Crypt_setPublicKeyModulusExponent(CryptKey    *cryptKey,
                                       ConstString modulus,
                                       ConstString exponent
                                      )
{
  #ifdef HAVE_GCRYPT
    gcry_mpi_t   nToken,eToken;
    gcry_sexp_t  key;
    gcry_error_t gcryptError;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(cryptKey);

  #ifdef HAVE_GCRYPT
    if ((modulus != NULL) && (exponent != NULL))
    {
      // get n, e
      gcryptError = gcry_mpi_scan(&nToken,GCRYMPI_FMT_HEX,(char*)String_cString(modulus),0,NULL);
      if (gcryptError != 0)
      {
        return FALSE;
      }
      gcryptError = gcry_mpi_scan(&eToken,GCRYMPI_FMT_HEX,(char*)String_cString(exponent),0,NULL);
      if (gcryptError != 0)
      {
        gcry_mpi_release(nToken);
        return FALSE;
      }

      // create public key
//TODO: changed %M -> %m
      gcryptError = gcry_sexp_build(&key,NULL,"(public-key (rsa (n %m) (e %m)))",nToken,eToken);
      if (gcryptError != 0)
      {
        gcry_mpi_release(eToken);
        gcry_mpi_release(nToken);
        return FALSE;
      }
      if (cryptKey->key != NULL) gcry_sexp_release(cryptKey->key);
      cryptKey->key = key;
//fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__); gcry_sexp_dump(cryptKey->key);

      // free resources
      gcry_mpi_release(eToken);
      gcry_mpi_release(nToken);
    }

    return TRUE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);
    UNUSED_VARIABLE(modulus);
    UNUSED_VARIABLE(exponent);

    return FALSE;
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_readPublicPrivateKeyFile(CryptKey            *cryptKey,
                                      const char          *fileName,
                                      CryptMode           cryptMode,
                                      CryptKeyDeriveTypes cryptKeyDeriveType,
                                      const CryptSalt     *cryptSalt,
                                      const Password      *password
                                     )
{
  Errors     error;
  FileHandle fileHandle;
  uint       dataLength;
  void       *data;
  void       *keyData;
  uint       keyDataLength;

  assert(cryptKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(cryptKey);
  assert(fileName != NULL);

  // check if read is available
  if (!File_existsCString(fileName))
  {
    return ERRORX_(KEY_NOT_FOUND,0,"%s",fileName);
  }

  // open file
  error = File_openCString(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get data size
  dataLength = File_getSize(&fileHandle);
  if (dataLength == 0LL)
  {
    error = ERRORX_(IO,errno,"%E",errno);
    (void)File_close(&fileHandle);
    return error;
  }

  // allocate secure memory
  data = (char*)allocSecure((size_t)dataLength);
  if (data == NULL)
  {
    (void)File_close(&fileHandle);
    return ERROR_INSUFFICIENT_MEMORY;
  }

  // read file contents
  error = File_read(&fileHandle,data,dataLength,NULL);
  if (error != ERROR_NONE)
  {
    freeSecure(data);
    File_close(&fileHandle);
    return error;
  }

  // close file
  File_close(&fileHandle);

  // decode base64
  keyDataLength = Misc_base64DecodeLengthBuffer(data,dataLength);
  if (keyDataLength <= 0)
  {
    freeSecure(data);
    return ERROR_INVALID_KEY;
  }
  keyData = allocSecure(keyDataLength);
  if (keyData == NULL)
  {
    freeSecure(data);
    return ERROR_INSUFFICIENT_MEMORY;
  }
  if (!Misc_base64DecodeBuffer(keyData,keyDataLength,NULL,data,dataLength))
  {
    freeSecure(keyData);
    freeSecure(data);
    return ERROR_INVALID_KEY;
  }

  // set key data
  error = Crypt_setPublicPrivateKeyData(cryptKey,
                                        keyData,
                                        keyDataLength,
                                        cryptMode,
                                        cryptKeyDeriveType,
                                        cryptSalt,
                                        password
                                       );
  if (error != ERROR_NONE)
  {
    freeSecure(keyData);
    freeSecure(data);
    return error;
  }

  // free resources */
  freeSecure(keyData);
  freeSecure(data);

  return ERROR_NONE;
}

Errors Crypt_writePublicPrivateKeyFile(CryptKey            *cryptKey,
                                       const char          *fileName,
                                       CryptMode           cryptMode,
                                       CryptKeyDeriveTypes cryptKeyDeriveType,
                                       const CryptSalt     *cryptSalt,
                                       const Password      *password
                                      )
{
  void       *keyData;
  uint       keyDataLength;
  uint       dataLength;
  void       *data;
  Errors     error;
  FileHandle fileHandle;

  assert(cryptKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(cryptKey);
  assert(fileName != NULL);

  // get key data
  error = Crypt_getPublicPrivateKeyData(cryptKey,
                                        &keyData,
                                        &keyDataLength,
                                        cryptMode,
                                        cryptKeyDeriveType,
                                        cryptSalt,
                                        password
                                       );
  if (error != ERROR_NONE)
  {
    return error;
  }

// TODO: secure memory
  // encode base64
  dataLength = Misc_base64EncodeLength(keyData,keyDataLength);
  data = allocSecure(dataLength);
  if (data == NULL)
  {
    freeSecure(keyData);
    return ERROR_INSUFFICIENT_MEMORY;
  }
  if (!Misc_base64EncodeBuffer(data,dataLength,keyData,keyDataLength))
  {
    freeSecure(keyData);
    return ERROR_INVALID_KEY;
  }

  // write file
  error = File_openCString(&fileHandle,fileName,FILE_OPEN_CREATE);
  if (error != ERROR_NONE)
  {
    freeSecure(data);
    freeSecure(keyData);
    return error;
  }
  error = File_write(&fileHandle,data,dataLength);
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    freeSecure(data);
    freeSecure(keyData);
    return error;
  }
  File_close(&fileHandle);

  // free resources */
  freeSecure(data);
  freeSecure(keyData);

  return ERROR_NONE;
}

Errors Crypt_createPublicPrivateKeyPair(CryptKey *publicCryptKey,
                                        CryptKey *privateCryptKey,
                                        uint     keyLength,
                                        uint     cryptKeyMode
                                       )
{
  #ifdef HAVE_GCRYPT
    String       description;
    gcry_sexp_t  sexpKeyParameters;
    gcry_sexp_t  sexpKey;
    gcry_error_t gcryptError;
  #endif /* HAVE_GCRYPT */

  assert(publicCryptKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(publicCryptKey);
  assert(privateCryptKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(privateCryptKey);

  #ifdef HAVE_GCRYPT
    // init key parameters
    description = String_format(String_new(),"(genkey (rsa %s(nbits 4:%d)))",((cryptKeyMode & CRYPT_KEY_MODE_TRANSIENT) != 0) ? "(flags transient-key) " : "",keyLength);
//fprintf(stderr,"%s, %d: description=%s\n",__FILE__,__LINE__,String_cString(description));
    gcryptError = gcry_sexp_new(&sexpKeyParameters,
                                String_cString(description),
                                0,
                                1  // autodetect
                               );
    if (gcryptError != 0)
    {
      char buffer[128];

      gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
      String_delete(description);
      return ERRORX_(CREATE_KEY_FAIL,gcryptError,"%s",buffer);
    }

    // generate key pair
    gcryptError = gcry_pk_genkey(&sexpKey,sexpKeyParameters);
    if (gcryptError != 0)
    {
      char buffer[128];

      gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
      gcry_sexp_release(sexpKeyParameters);
      String_delete(description);
      return ERRORX_(CREATE_KEY_FAIL,gcryptError,"%s",buffer);
    }
    gcry_sexp_release(sexpKeyParameters);
    String_delete(description);
//gcry_sexp_dump(sexpKey);
    if (publicCryptKey->key != NULL) gcry_sexp_release(publicCryptKey->key);
    publicCryptKey->key  = gcry_sexp_find_token(sexpKey,"public-key",0);
    if (privateCryptKey->key != NULL) gcry_sexp_release(privateCryptKey->key);
    privateCryptKey->key = gcry_sexp_find_token(sexpKey,"private-key",0);
    gcry_sexp_release(sexpKey);

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(publicCryptKey);
    UNUSED_VARIABLE(privateCryptKey);
    UNUSED_VARIABLE(keyLength);
    UNUSED_VARIABLE(cryptKeyMode);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_encryptWithPublicKey(const CryptKey *publicCryptKey,
                                  const void     *buffer,
                                  uint           bufferLength,
                                  void           *encryptBuffer,
                                  uint           *encryptBufferLength,
                                  uint           maxEncryptBufferLength
                                 )
{
  #ifdef HAVE_GCRYPT
    gcry_error_t gcryptError;
    gcry_sexp_t  sexpData;
    gcry_sexp_t  sexpEncryptData;
    gcry_sexp_t  sexpToken;
    const char   *encryptData;
    size_t       encryptDataLength;
  #endif /* HAVE_GCRYPT */

  assert(publicCryptKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(publicCryptKey);
  assert(buffer != NULL);
  assert(encryptBuffer != NULL);
  assert(encryptBufferLength != NULL);

  #ifdef HAVE_GCRYPT
//gcry_sexp_dump(cryptKey->key); fprintf(stderr,"%s,%d: %d\n",__FILE__,__LINE__,bufferLength);
    if (publicCryptKey->key == NULL)
    {
      return ERROR_NO_PUBLIC_CRYPT_KEY;
    }

    // create S-expression with data
    switch (publicCryptKey->cryptPaddingType)
    {
      case CRYPT_PADDING_TYPE_NONE:
        gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags raw) (value %b))",(int)bufferLength,(char*)buffer);
        break;
      case CRYPT_PADDING_TYPE_PKCS1:
        gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags pkcs1) (value %b))",(int)bufferLength,(char*)buffer);
        break;
      case CRYPT_PADDING_TYPE_OAEP:
        gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags oaep) (value %b))",(int)bufferLength,(char*)buffer);
        break;
      default:
        return ERRORX_(KEY_ENCRYPT,0,"unknown padding type");
        break;
    }
    if (gcryptError != 0)
    {
      char buffer[128];

      gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
      return ERRORX_(KEY_ENCRYPT,gcryptError,"%s",buffer);
    }

    // encrypt
    gcryptError = gcry_pk_encrypt(&sexpEncryptData,sexpData,publicCryptKey->key);
    if (gcryptError != 0)
    {
      char buffer[128];

      gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
      gcry_sexp_release(sexpData);
      return ERRORX_(KEY_ENCRYPT,gcryptError,"%s",buffer);
    }

    // get encrypted data
    sexpToken = gcry_sexp_find_token(sexpEncryptData,"a",0);
    if (sexpToken == NULL)
    {
      gcry_sexp_release(sexpEncryptData);
      gcry_sexp_release(sexpData);
      return ERROR_KEY_ENCRYPT;
    }
    encryptData = gcry_sexp_nth_data(sexpToken,1,&encryptDataLength);
    if (encryptData == NULL)
    {
      gcry_sexp_release(sexpEncryptData);
      gcry_sexp_release(sexpData);
      return ERROR_KEY_ENCRYPT;
    }
    (*encryptBufferLength) = MIN(encryptDataLength,maxEncryptBufferLength);
    memCopyFast(encryptBuffer,*encryptBufferLength,encryptData,*encryptBufferLength);
    gcry_sexp_release(sexpToken);

    // free resources
    gcry_sexp_release(sexpEncryptData);
    gcry_sexp_release(sexpData);

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(publicCryptKey);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferLength);
    UNUSED_VARIABLE(encryptBuffer);
    UNUSED_VARIABLE(encryptBufferLength);
    UNUSED_VARIABLE(maxEncryptBufferLength);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_decryptWithPrivateKey(const CryptKey *privateCryptKey,
                                   const void     *encryptBuffer,
                                   uint           encryptBufferLength,
                                   void           *buffer,
                                   uint           *bufferLength,
                                   uint           maxBufferLength
                                  )
{
  #ifdef HAVE_GCRYPT
    gcry_error_t gcryptError;
    gcry_sexp_t  sexpEncryptData;
    gcry_sexp_t  sexpData;
    const char   *data;
    size_t       dataLength;
  #endif /* HAVE_GCRYPT */

  assert(privateCryptKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(privateCryptKey);
  assert(encryptBuffer != NULL);
  assert(buffer != NULL);

  if (encryptBufferLength > 0)
  {
    #ifdef HAVE_GCRYPT
      if (privateCryptKey->key == NULL)
      {
        return ERROR_NO_PRIVATE_CRYPT_KEY;
      }

      // create S-expression with encrypted data
//fprintf(stderr,"%s, %d: encryptBuffer: privateCryptKey->cryptPaddingType=%d\n",__FILE__,__LINE__,privateCryptKey->cryptPaddingType); debugDumpMemory(encryptBuffer,encryptBufferLength,0);
      switch (privateCryptKey->cryptPaddingType)
      {
        case CRYPT_PADDING_TYPE_NONE:
          gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (rsa (a %b)))",encryptBufferLength,encryptBuffer);
          break;
        case CRYPT_PADDING_TYPE_PKCS1:
          gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (flags pkcs1) (rsa (a %b)))",encryptBufferLength,encryptBuffer);
          break;
        case CRYPT_PADDING_TYPE_OAEP:
          gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (flags oaep) (rsa (a %b)))",encryptBufferLength,encryptBuffer);
          break;
        default:
          return ERRORX_(KEY_DECRYPT,0,"unknown padding type");
          break;
      }
      if (gcryptError != 0)
      {
        char buffer[128];

        gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
        return ERRORX_(KEY_DECRYPT,gcryptError,"%s",gcry_strerror(gcryptError));
      }
//fprintf(stderr,"%s, %d: sexpEncryptData=%s\n",__FILE__,__LINE__); gcry_sexp_dump(&sexpEncryptData);

      // decrypt
//fprintf(stderr,"%s, %d: private key\n",__FILE__,__LINE__); gcry_sexp_dump(privateCryptKey->key);
      gcryptError = gcry_pk_decrypt(&sexpData,sexpEncryptData,privateCryptKey->key);
      if (gcryptError != 0)
      {
        char buffer[128];

        gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
        gcry_sexp_release(sexpEncryptData);
        return ERRORX_(DECRYPT,gcryptError,"%s",buffer);
      }
//fprintf(stderr,"%s, %d: plain data\n",__FILE__,__LINE__); gcry_sexp_dump(sexpData);

      // get decrypted data
      data = gcry_sexp_nth_data(sexpData,1,&dataLength);
      if (data == NULL)
      {
        gcry_sexp_release(sexpData);
        gcry_sexp_release(sexpEncryptData);
        return ERROR_DECRYPT;
      }
      if (bufferLength != NULL) (*bufferLength) = MIN(dataLength,maxBufferLength);
      memCopyFast(buffer,*bufferLength,data,*bufferLength);

      // free resources
      gcry_sexp_release(sexpData);
      gcry_sexp_release(sexpEncryptData);

      return ERROR_NONE;
    #else /* not HAVE_GCRYPT */
      UNUSED_VARIABLE(privateCryptKey);
      UNUSED_VARIABLE(encryptBuffer);
      UNUSED_VARIABLE(encryptBufferLength);
      UNUSED_VARIABLE(buffer);
      UNUSED_VARIABLE(bufferLength);
      UNUSED_VARIABLE(maxBufferLength);

      return ERROR_FUNCTION_NOT_SUPPORTED;
    #endif /* HAVE_GCRYPT */
  }
  else
  {
    return ERROR_NONE;
  }
}

Errors Crypt_getRandomCryptKey(CryptKey       *cryptKey,
                               uint           keyLength,
                               const CryptKey *publicKey,
                               void           *encryptedKeyData,
                               uint           maxEncryptedKeyDataLength,
                               uint           *encryptedKeyDataLength
                              )
{
  #ifdef HAVE_GCRYPT
    gcry_sexp_t  sexpToken;
    void         *data;
    uint         dataLength;
    byte         *pkcs1EncodedMessage;
    byte         randomBuffer[32];
    uint         i,j;
    Errors       error;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(cryptKey);
  assert(keyLength > 0);
  assert(publicKey != NULL);
  assert(encryptedKeyData != NULL);
  assert(encryptedKeyDataLength != NULL);

  #ifdef HAVE_GCRYPT
//fprintf(stderr,"%s,%d: ---------------------------------\n",__FILE__,__LINE__);
//fprintf(stderr,"%s,%d: public key\n",__FILE__,__LINE__);
//gcry_sexp_dump(publicKey->key);

    // check key length
    if (keyLength > PKCS1_KEY_LENGTH)
    {
      return ERROR_INVALID_KEY_LENGTH;
    }

    // check if public key is available
    if (!Crypt_isKeyAvailable(publicKey))
    {
      return ERROR_NO_PUBLIC_CRYPT_KEY;
    }
    sexpToken = gcry_sexp_find_token(publicKey->key,"public-key",0);
    if (sexpToken == NULL)
    {
      return ERROR_NOT_A_PUBLIC_CRYPT_KEY;
    }
    gcry_sexp_release(sexpToken);

    // create random key
    dataLength = ALIGN(PKCS1_KEY_LENGTH,8)/8;
    data = allocSecure(dataLength);
    if (data == NULL)
    {
      return ERROR_INSUFFICIENT_MEMORY;
    }
    gcry_create_nonce((unsigned char*)data,dataLength);

    // create padded encoded message block: format 0x00 0x02 <PS random data> 0x00 <key data>; size 512bit
    pkcs1EncodedMessage = allocSecure(PKCS1_ENCODED_MESSAGE_LENGTH);
    if (pkcs1EncodedMessage == NULL)
    {
      freeSecure(data);
      return ERROR_INSUFFICIENT_MEMORY;
    }
    pkcs1EncodedMessage[0] = 0x00;
    pkcs1EncodedMessage[1] = 0x02;
    j = 0;
    gcry_create_nonce((unsigned char*)randomBuffer,sizeof(randomBuffer));
    for (i = 0; i < PKCS1_ENCODED_MESSAGE_PADDING_LENGTH; i++)
    {
      // get random byte (exclude 0-bytes)
      while ((j >= sizeof(randomBuffer)) || (randomBuffer[j] == 0))
      {
        if (j >= sizeof(randomBuffer))
        {
          gcry_create_nonce((unsigned char*)randomBuffer,sizeof(randomBuffer));
          j = 0;
        }
        else
        {
          j++;
        }
      }

      // store random byte
      pkcs1EncodedMessage[1+1+i] = randomBuffer[j];
      j++;
    }
    pkcs1EncodedMessage[1+1+PKCS1_ENCODED_MESSAGE_PADDING_LENGTH] = 0x00;
    memCopyFast(&pkcs1EncodedMessage[1+1+PKCS1_ENCODED_MESSAGE_PADDING_LENGTH+1],dataLength,data,dataLength);
#ifdef DEBUG_ASYMMETRIC_CRYPT
fprintf(stderr,"%s, %d: encoded pkcs1EncodedMessage %d\n",__FILE__,__LINE__,PKCS1_ENCODED_MESSAGE_LENGTH); debugDumpMemory(pkcs1EncodedMessage,PKCS1_ENCODED_MESSAGE_LENGTH,0);
#endif

    error = Crypt_encryptWithPublicKey(publicKey,
                                       pkcs1EncodedMessage,
                                       PKCS1_ENCODED_MESSAGE_LENGTH,
                                       encryptedKeyData,
                                       encryptedKeyDataLength,
                                       maxEncryptedKeyDataLength
                                      );
    if (error != ERROR_NONE)
    {
      freeSecure(pkcs1EncodedMessage);
      freeSecure(data);
      return error;
    }

    // set key
    if (cryptKey->data != NULL) freeSecure(cryptKey->data);
    cryptKey->data       = data;
    cryptKey->dataLength = ALIGN(keyLength,8)/8;
    if (cryptKey->key != NULL) gcry_sexp_release(cryptKey->key);
    cryptKey->key        = NULL;

    // free resources
    freeSecure(pkcs1EncodedMessage);

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);
    UNUSED_VARIABLE(keyLength);
    UNUSED_VARIABLE(publicKey);
    UNUSED_VARIABLE(encryptedKeyData);
    UNUSED_VARIABLE(maxEncryptedKeyDataLength);
    UNUSED_VARIABLE(encryptedKeyDataLength);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_getDecryptKey(CryptKey       *cryptKey,
                           uint           keyLength,
                           const CryptKey *privateKey,
                           const void     *encryptedKeyData,
                           uint           encryptedKeyDataLength
                          )
{
  #ifdef HAVE_GCRYPT
    gcry_sexp_t  sexpToken;
    gcry_sexp_t  sexpEncryptData;
    gcry_sexp_t  sexpData;
    byte         *pkcs1EncodedMessage;
    const byte   *keyData;
    size_t       dataLength;
    byte         *data;
    gcry_error_t gcryptError;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(cryptKey);
  assert(privateKey != NULL);
  assert(encryptedKeyData != NULL);

  #ifdef HAVE_GCRYPT
//fprintf(stderr,"%s,%d: ---------------------------------\n",__FILE__,__LINE__);
//fprintf(stderr,"%s,%d: private key\n",__FILE__,__LINE__);
//gcry_sexp_dump(privateKey->key);
    // check key length
    if (keyLength > PKCS1_KEY_LENGTH)
    {
      return ERROR_INVALID_KEY_LENGTH;
    }

    // check if private key available
    if (!Crypt_isKeyAvailable(privateKey))
    {
      return ERROR_NO_PRIVATE_CRYPT_KEY;
    }
    sexpToken = gcry_sexp_find_token(privateKey->key,"private-key",0);
    if (sexpToken == NULL)
    {
      return ERROR_NOT_A_PRIVATE_CRYPT_KEY;
    }
    gcry_sexp_release(sexpToken);

    // create S-expression with encrypted data
#ifdef DEBUG_ASYMMETRIC_CRYPT
fprintf(stderr,"%s, %d: encrypted random key %d\n",__FILE__,__LINE__,encryptedKeyDataLength); debugDumpMemory(encryptedKeyData,encryptedKeyDataLength,0);
#endif
    gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (rsa (a %b)))",encryptedKeyDataLength,encryptedKeyData);
    if (gcryptError != 0)
    {
      char buffer[128];

      gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
      return ERRORX_(KEY_DECRYPT,gcryptError,"%s",buffer);
    }
//fprintf(stderr,"%s,%d: --- encrypted key data \n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpEncryptData);

    // decrypt
    gcryptError = gcry_pk_decrypt(&sexpData,sexpEncryptData,privateKey->key);
    if (gcryptError != 0)
    {
      char buffer[128];

      gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
      gcry_sexp_release(sexpEncryptData);
      return ERRORX_(KEY_DECRYPT,gcryptError,"%s",buffer);
    }
//fprintf(stderr,"%s,%d: --- key data \n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpData);

    // get decrypted data
    pkcs1EncodedMessage = allocSecure(PKCS1_ENCODED_MESSAGE_LENGTH);
    if (pkcs1EncodedMessage == NULL)
    {
      gcry_sexp_release(sexpData);
      gcry_sexp_release(sexpEncryptData);
      return ERROR_KEY_ENCRYPT;
    }
    keyData = (const byte*)gcry_sexp_nth_data(sexpData,0,&dataLength);
    if (keyData == NULL)
    {
      freeSecure(pkcs1EncodedMessage);
      gcry_sexp_release(sexpData);
      gcry_sexp_release(sexpEncryptData);
      return ERROR_KEY_DECRYPT;
    }

    // check if key length is valid
    if (dataLength > PKCS1_ENCODED_MESSAGE_LENGTH)
    {
      freeSecure(pkcs1EncodedMessage);
      gcry_sexp_release(sexpData);
      gcry_sexp_release(sexpEncryptData);
      return ERROR_WRONG_PRIVATE_KEY;
    }

    // MPI does not store leading 0 -> do padding with 0 for required length
    memCopy(&pkcs1EncodedMessage[PKCS1_ENCODED_MESSAGE_LENGTH-dataLength],dataLength,&keyData[0],dataLength);
    memClear(&pkcs1EncodedMessage[0],PKCS1_ENCODED_MESSAGE_LENGTH-dataLength);
#ifdef DEBUG_ASYMMETRIC_CRYPT
fprintf(stderr,"%s, %d: pkcs1EncodedMessage %d\n",__FILE__,__LINE__,PKCS1_ENCODED_MESSAGE_LENGTH); debugDumpMemory(pkcs1EncodedMessage,PKCS1_ENCODED_MESSAGE_LENGTH,0);
#endif

    // check PKCS1 encoded message block
    if (   (pkcs1EncodedMessage[0] != 0x00)
        || (pkcs1EncodedMessage[1] != 0x02)
        || (pkcs1EncodedMessage[1+1+PKCS1_ENCODED_MESSAGE_PADDING_LENGTH] != 0x00)
       )
    {
      freeSecure(pkcs1EncodedMessage);
      gcry_sexp_release(sexpData);
      gcry_sexp_release(sexpEncryptData);
      return ERROR_WRONG_PRIVATE_KEY;
    }

    // get key length
    if (keyLength == 0) keyLength = PKCS1_KEY_LENGTH;

    // get key data
    data = allocSecure(ALIGN(keyLength,8)/8);
    if (data == NULL)
    {
      freeSecure(pkcs1EncodedMessage);
      gcry_sexp_release(sexpData);
      gcry_sexp_release(sexpEncryptData);
      return ERROR_INSUFFICIENT_MEMORY;
    }
    memCopy(data,ALIGN(keyLength,8)/8,&pkcs1EncodedMessage[1+1+PKCS1_ENCODED_MESSAGE_PADDING_LENGTH+1],dataLength);
#ifdef DEBUG_ASYMMETRIC_CRYPT
fprintf(stderr,"%s, %d: key data %d\n",__FILE__,__LINE__,keyLength); debugDumpMemory(data,ALIGN(keyLength,8)/8,0);
#endif

    // set key
    if (cryptKey->data != NULL) freeSecure(cryptKey->data);
    cryptKey->data       = data;
    cryptKey->dataLength = ALIGN(keyLength,8)/8;
    if (cryptKey->key != NULL) gcry_sexp_release(cryptKey->key);
    cryptKey->key        = NULL;

    // free resources
    freeSecure(pkcs1EncodedMessage);
    gcry_sexp_release(sexpData);
    gcry_sexp_release(sexpEncryptData);

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);
    UNUSED_VARIABLE(keyLength);
    UNUSED_VARIABLE(privateKey);
    UNUSED_VARIABLE(encryptedKeyData);
    UNUSED_VARIABLE(encryptedKeyDataLength);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_getSignature(void           *signature,
                          uint           maxSignatureLength,
                          uint           *signatureLength,
                          const CryptKey *privateKey,
                          const void     *buffer,
                          uint           bufferLength
                         )
{
  #ifdef HAVE_GCRYPT
    gcry_error_t gcryptError;
    gcry_sexp_t  sexpToken;
    gcry_sexp_t  sexpData;
    gcry_sexp_t  sexpSignatureData;
    const char   *signatureData;
    size_t       signatureDataLength;
  #endif /* HAVE_GCRYPT */

  assert(signature != NULL);
  assert(signatureLength != NULL);
  assert(privateKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(privateKey);
  assert(buffer != NULL);

  #ifdef HAVE_GCRYPT
    // check if private key is available
    if (!Crypt_isKeyAvailable(privateKey))
    {
      return ERROR_NO_PRIVATE_SIGNATURE_KEY;
    }
    sexpToken = gcry_sexp_find_token(privateKey->key,"private-key",0);
    if (sexpToken == NULL)
    {
      return ERROR_NOT_A_PRIVATE_SIGNATURE_KEY;
    }
    gcry_sexp_release(sexpToken);

    // create S-expression with data
    gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags raw) (value %b))",bufferLength,(char*)buffer);
//TODO: does not work?
//    gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags raw) (hash sha512 %b))",bufferLength,(char*)buffer);
    if (gcryptError != 0)
    {
      char buffer[128];

      gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
      return ERRORX_(KEY_ENCRYPT,gcryptError,"%s",gcry_strerror(gcryptError));
    }
//fprintf(stderr,"%s, %d: data \n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpData);

    // sign
    gcryptError = gcry_pk_sign(&sexpSignatureData,sexpData,privateKey->key);
    if (gcryptError != 0)
    {
      char buffer[128];

      gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
      gcry_sexp_release(sexpData);
      return ERRORX_(KEY_ENCRYPT,gcryptError,"%s",gcry_strerror(gcryptError));
    }
//fprintf(stderr,"%s, %d: signature data\n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpSignatureData);

    // get signature data
    sexpToken = gcry_sexp_find_token(sexpSignatureData,"s",0);
    if (sexpToken == NULL)
    {
      gcry_sexp_release(sexpSignatureData);
      gcry_sexp_release(sexpData);
      return ERROR_KEY_ENCRYPT;
    }
//fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpToken);
    signatureData = gcry_sexp_nth_data(sexpToken,1,&signatureDataLength);
    if (signatureData == NULL)
    {
      gcry_sexp_release(sexpSignatureData);
      gcry_sexp_release(sexpData);
      return ERROR_KEY_ENCRYPT;
    }
    (*signatureLength) = MIN(signatureDataLength,maxSignatureLength);
    memCopyFast(signature,(*signatureLength),signatureData,(*signatureLength));
    gcry_sexp_release(sexpToken);

    // free resources
    gcry_sexp_release(sexpSignatureData);
    gcry_sexp_release(sexpData);

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(privateKey);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferLength);
    UNUSED_VARIABLE(signature);
    UNUSED_VARIABLE(maxSignatureLength);
    UNUSED_VARIABLE(signatureLength);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_verifySignature(const void           *signature,
                             uint                 signatureLength,
                             CryptSignatureStates *cryptSignatureState,
                             const CryptKey       *publicKey,
                             const void           *buffer,
                             uint                 bufferLength
                            )
{
  #ifdef HAVE_GCRYPT
    gcry_sexp_t  sexpToken;
    gcry_sexp_t  sexpData;
    gcry_sexp_t  sexpSignatureData;
    gcry_error_t gcryptError;
  #endif /* HAVE_GCRYPT */

  assert(signature != NULL);
  assert(publicKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(publicKey);
  assert(buffer != NULL);

  if (cryptSignatureState != NULL) (*cryptSignatureState) = CRYPT_SIGNATURE_STATE_NONE;

  #ifdef HAVE_GCRYPT
    // check if public key is available
    if (!Crypt_isKeyAvailable(publicKey))
    {
      return ERROR_NO_PUBLIC_SIGNATURE_KEY;
    }
    sexpToken = gcry_sexp_find_token(publicKey->key,"public-key",0);
    if (sexpToken == NULL)
    {
      return ERROR_NOT_A_PUBLIC_SIGNATURE_KEY;
    }
    gcry_sexp_release(sexpToken);

    // create S-expression with signature
    gcryptError = gcry_sexp_build(&sexpSignatureData,NULL,"(sig-val (rsa (s %b)))",signatureLength,(char*)signature);
    if (gcryptError != 0)
    {
      char buffer[128];

      gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
      return ERRORX_(INVALID_SIGNATURE,gcryptError,"%s",gcry_strerror(gcryptError));
    }
//fprintf(stderr,"%s, %d: signature\n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpSignatureData);

    // create S-expression with data
    gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags raw) (value %b))",bufferLength,(char*)buffer);
//TODO: does not work?
//    gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (hash sha512 %b))",bufferLength,(char*)buffer);
    if (gcryptError != 0)
    {
      char buffer[128];

      gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
      gcry_sexp_release(sexpSignatureData);
      return ERRORX_(INVALID_SIGNATURE,gcryptError,"%s",gcry_strerror(gcryptError));
    }
//fprintf(stderr,"%s, %d: data \n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpData);

    // verify
    gcryptError = gcry_pk_verify(sexpSignatureData,sexpData,publicKey->key);
    if (cryptSignatureState != NULL)
    {
      (*cryptSignatureState) = (gcryptError == 0)
                                 ? CRYPT_SIGNATURE_STATE_OK
                                 : CRYPT_SIGNATURE_STATE_INVALID;
    }

    // free resources
    gcry_sexp_release(sexpData);
    gcry_sexp_release(sexpSignatureData);

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(publicKey);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferLength);
    UNUSED_VARIABLE(signature);
    UNUSED_VARIABLE(signatureLength);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
Errors Crypt_initHash(CryptHash           *cryptHash,
                      CryptHashAlgorithms cryptHashAlgorithm
                     )
#else /* not NDEBUG */
Errors __Crypt_initHash(const char          *__fileName__,
                        ulong               __lineNb__,
                        CryptHash           *cryptHash,
                        CryptHashAlgorithms cryptHashAlgorithm
                       )
#endif /* NDEBUG */
{
  #ifdef HAVE_GCRYPT
    gcry_error_t gcryptError;
  #endif /* HAVE_GCRYPT */

  assert(cryptHash != NULL);

  // init variables
  cryptHash->cryptHashAlgorithm = cryptHashAlgorithm;

  // init crypt algorithm
  switch (cryptHashAlgorithm)
  {
    case CRYPT_HASH_ALGORITHM_NONE:
      cryptHash->none.data       = NULL;
      cryptHash->none.dataLength = 0;
      break;
    case CRYPT_HASH_ALGORITHM_SHA2_224:
    case CRYPT_HASH_ALGORITHM_SHA2_256:
    case CRYPT_HASH_ALGORITHM_SHA2_384:
    case CRYPT_HASH_ALGORITHM_SHA2_512:
      #ifdef HAVE_GCRYPT
        {
          int hashAlgorithm;

          hashAlgorithm = GCRY_MD_NONE;
          switch (cryptHashAlgorithm)
          {
            case CRYPT_HASH_ALGORITHM_SHA2_224: hashAlgorithm = GCRY_MD_SHA224; break;
            case CRYPT_HASH_ALGORITHM_SHA2_256: hashAlgorithm = GCRY_MD_SHA256; break;
            case CRYPT_HASH_ALGORITHM_SHA2_384: hashAlgorithm = GCRY_MD_SHA384; break;
            case CRYPT_HASH_ALGORITHM_SHA2_512: hashAlgorithm = GCRY_MD_SHA512; break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }

          gcryptError = gcry_md_open(&cryptHash->gcry_md_hd,hashAlgorithm,GCRY_MD_FLAG_SECURE);
          if (gcryptError != 0)
          {
            char buffer[128];

            gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
            return ERRORX_(INIT_HASH,gcryptError,"%s",buffer);
          }
        }
      #else /* not HAVE_GCRYPT */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(cryptHash,CryptHash);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptHash,CryptHash);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
void Crypt_doneHash(CryptHash *cryptHash)
#else /* not NDEBUG */
void __Crypt_doneHash(const char *__fileName__,
                      ulong      __lineNb__,
                      CryptHash  *cryptHash
                     )
#endif /* NDEBUG */
{
  assert(cryptHash != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(cryptHash,CryptHash);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptHash,CryptHash);
  #endif /* NDEBUG */

  switch (cryptHash->cryptHashAlgorithm)
  {
    case CRYPT_HASH_ALGORITHM_NONE:
      if (cryptHash->none.data != NULL) freeSecure(cryptHash->none.data);
      break;
    case CRYPT_HASH_ALGORITHM_SHA2_224:
    case CRYPT_HASH_ALGORITHM_SHA2_256:
    case CRYPT_HASH_ALGORITHM_SHA2_384:
    case CRYPT_HASH_ALGORITHM_SHA2_512:
      #ifdef HAVE_GCRYPT
        gcry_md_close(cryptHash->gcry_md_hd);
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(cryptHash);
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
}

#ifdef NDEBUG
  CryptHash *Crypt_newHash(CryptHashAlgorithms cryptHashAlgorithm)
#else /* not NDEBUG */
  CryptHash *__Crypt_newHash(const char          *__fileName__,
                             ulong               __lineNb__,
                             CryptHashAlgorithms cryptHashAlgorithm
                            )
#endif /* NDEBUG */
{
  CryptHash *cryptHash;

  cryptHash = (CryptHash*)malloc(sizeof(CryptHash));
  if (cryptHash == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  #ifndef NDEBUG
    __Crypt_initHash(__fileName__,__lineNb__,cryptHash,cryptHashAlgorithm);
  #else /* not NDEBUG */
    Crypt_initHash(cryptHash,cryptHashAlgorithm);
  #endif /* NDEBUG */

  return cryptHash;
}

#ifdef NDEBUG
  void Crypt_deleteHash(CryptHash *cryptHash)
#else /* not NDEBUG */
  void __Crypt_deleteHash(const char *__fileName__,
                          ulong      __lineNb__,
                          CryptHash  *cryptHash
                         )
#endif /* NDEBUG */
{
  if (cryptHash != NULL)
  {
    #ifndef NDEBUG
      __Crypt_doneHash(__fileName__,__lineNb__,cryptHash);
    #else /* not NDEBUG */
      Crypt_doneHash(cryptHash);
    #endif /* NDEBUG */
    free(cryptHash);
  }
}

void Crypt_resetHash(CryptHash *cryptHash)
{
  assert(cryptHash != NULL);

  switch (cryptHash->cryptHashAlgorithm)
  {
    case CRYPT_HASH_ALGORITHM_NONE:
      if (cryptHash->none.data != NULL) freeSecure(cryptHash->none.data);
      cryptHash->none.data       = NULL;
      cryptHash->none.dataLength = 0;
      break;
    case CRYPT_HASH_ALGORITHM_SHA2_224:
    case CRYPT_HASH_ALGORITHM_SHA2_256:
    case CRYPT_HASH_ALGORITHM_SHA2_384:
    case CRYPT_HASH_ALGORITHM_SHA2_512:
      #ifdef HAVE_GCRYPT
        gcry_md_reset(cryptHash->gcry_md_hd);
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(cryptHash);
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
}

void Crypt_updateHash(CryptHash  *cryptHash,
                      const void *buffer,
                      ulong      bufferLength
                     )
{
  assert(cryptHash != NULL);

  switch (cryptHash->cryptHashAlgorithm)
  {
    case CRYPT_HASH_ALGORITHM_NONE:
      if (cryptHash->none.data != NULL) freeSecure(cryptHash->none.data);
      cryptHash->none.data = allocSecure(bufferLength);
      if (cryptHash->none.data != NULL)
      {
        memCopyFast(cryptHash->none.data,bufferLength,buffer,bufferLength);
      }
      else
      {
        cryptHash->none.dataLength = 0;
      }
      break;
    case CRYPT_HASH_ALGORITHM_SHA2_224:
    case CRYPT_HASH_ALGORITHM_SHA2_256:
    case CRYPT_HASH_ALGORITHM_SHA2_384:
    case CRYPT_HASH_ALGORITHM_SHA2_512:
      #ifdef HAVE_GCRYPT
        gcry_md_write(cryptHash->gcry_md_hd,buffer,bufferLength);
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(cryptHash);
        UNUSED_VARIABLE(buffer);
        UNUSED_VARIABLE(bufferLength);
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
}

uint Crypt_getHashLength(const CryptHash *cryptHash)
{
  uint hashLength;

  assert(cryptHash != NULL);

  hashLength = 0;
  switch (cryptHash->cryptHashAlgorithm)
  {
    case CRYPT_HASH_ALGORITHM_NONE:
      hashLength = cryptHash->none.dataLength;
      break;
    case CRYPT_HASH_ALGORITHM_SHA2_224:
    case CRYPT_HASH_ALGORITHM_SHA2_256:
    case CRYPT_HASH_ALGORITHM_SHA2_384:
    case CRYPT_HASH_ALGORITHM_SHA2_512:
      #ifdef HAVE_GCRYPT
        {
          int gcryAlgo;

          gcryAlgo = GCRY_MD_NONE;
          switch (cryptHash->cryptHashAlgorithm)
          {
            case CRYPT_HASH_ALGORITHM_SHA2_224: gcryAlgo = GCRY_MD_SHA224; break;
            case CRYPT_HASH_ALGORITHM_SHA2_256: gcryAlgo = GCRY_MD_SHA256; break;
            case CRYPT_HASH_ALGORITHM_SHA2_384: gcryAlgo = GCRY_MD_SHA384; break;
            case CRYPT_HASH_ALGORITHM_SHA2_512: gcryAlgo = GCRY_MD_SHA512; break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }

          hashLength = gcry_md_get_algo_dlen(gcryAlgo);
        }
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(cryptHash);
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return hashLength;
}

void *Crypt_getHash(const CryptHash *cryptHash,
                    void            *buffer,
                    uint            bufferSize,
                    uint            *hashLength
                   )
{
  void *hashData;

  assert(cryptHash != NULL);

  hashData = NULL;
  switch (cryptHash->cryptHashAlgorithm)
  {
    case CRYPT_HASH_ALGORITHM_NONE:
      if (buffer != NULL)
      {
        if (cryptHash->none.dataLength <= bufferSize)
        {
          memCopyFast(buffer,cryptHash->none.dataLength,cryptHash->none.data,cryptHash->none.dataLength);
          hashData = buffer;
        }
        else
        {
          hashData = NULL;
        }
      }
      else
      {
        hashData = cryptHash->none.data;
      }
      if (hashLength != NULL) (*hashLength) = cryptHash->none.dataLength;
      break;
    case CRYPT_HASH_ALGORITHM_SHA2_224:
    case CRYPT_HASH_ALGORITHM_SHA2_256:
    case CRYPT_HASH_ALGORITHM_SHA2_384:
    case CRYPT_HASH_ALGORITHM_SHA2_512:
      #ifdef HAVE_GCRYPT
        {
          int  gcryAlgo;
          uint n;

          gcryAlgo = GCRY_MD_NONE;
          switch (cryptHash->cryptHashAlgorithm)
          {
            case CRYPT_HASH_ALGORITHM_SHA2_224: gcryAlgo = GCRY_MD_SHA224; break;
            case CRYPT_HASH_ALGORITHM_SHA2_256: gcryAlgo = GCRY_MD_SHA256; break;
            case CRYPT_HASH_ALGORITHM_SHA2_384: gcryAlgo = GCRY_MD_SHA384; break;
            case CRYPT_HASH_ALGORITHM_SHA2_512: gcryAlgo = GCRY_MD_SHA512; break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }

          n = gcry_md_get_algo_dlen(gcryAlgo);
          if (buffer != NULL)
          {
            if (n <= bufferSize)
            {
              memCopyFast(buffer,n,gcry_md_read(cryptHash->gcry_md_hd,gcryAlgo),n);
              hashData = buffer;
            }
            else
            {
              hashData = NULL;
            }
          }
          else
          {
            hashData = gcry_md_read(cryptHash->gcry_md_hd,gcryAlgo);
          }
          if (hashLength != NULL) (*hashLength) = n;
        }
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(buffer);
        UNUSED_VARIABLE(bufferSize);
        UNUSED_VARIABLE(hashLength);

        return NULL;
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return hashData;
}

bool Crypt_equalsHash(const CryptHash *cryptHash0,
                      const CryptHash *cryptHash1
                     )
{
  bool equalsFlag;
  #ifdef HAVE_GCRYPT
    int gcryAlgo;
  #endif /* HAVE_GCRYPT */

  assert(cryptHash0 != NULL);
  assert(cryptHash1 != NULL);

  equalsFlag = (cryptHash0->cryptHashAlgorithm == cryptHash1->cryptHashAlgorithm);
  if (equalsFlag)
  {
    switch (cryptHash1->cryptHashAlgorithm)
    {
      case CRYPT_HASH_ALGORITHM_NONE:
        equalsFlag = memEquals(cryptHash0->none.data,
                               cryptHash0->none.dataLength,
                               cryptHash1->none.data,
                               cryptHash1->none.dataLength
                              );
        break;
      case CRYPT_HASH_ALGORITHM_SHA2_224:
      case CRYPT_HASH_ALGORITHM_SHA2_256:
      case CRYPT_HASH_ALGORITHM_SHA2_384:
      case CRYPT_HASH_ALGORITHM_SHA2_512:
        #ifdef HAVE_GCRYPT
          gcryAlgo = GCRY_MD_NONE;
          switch (cryptHash1->cryptHashAlgorithm)
          {
            case CRYPT_HASH_ALGORITHM_SHA2_224: gcryAlgo = GCRY_MD_SHA224; break;
            case CRYPT_HASH_ALGORITHM_SHA2_256: gcryAlgo = GCRY_MD_SHA256; break;
            case CRYPT_HASH_ALGORITHM_SHA2_384: gcryAlgo = GCRY_MD_SHA384; break;
            case CRYPT_HASH_ALGORITHM_SHA2_512: gcryAlgo = GCRY_MD_SHA512; break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }

          equalsFlag = Crypt_equalsHashBuffer(cryptHash0,
                                              gcry_md_read(cryptHash1->gcry_md_hd,gcryAlgo),
                                              gcry_md_get_algo_dlen(gcryAlgo)
                                             );
        #else /* not HAVE_GCRYPT */
          UNUSED_VARIABLE(cryptHash0);
          UNUSED_VARIABLE(cryptHash1);

          return FALSE;
        #endif /* HAVE_GCRYPT */
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }

  return equalsFlag;
}

bool Crypt_equalsHashBuffer(const CryptHash *cryptHash,
                            const void      *buffer,
                            uint            bufferLength
                           )
{
  bool equalsFlag;

  assert(cryptHash != NULL);

  equalsFlag = FALSE;
  switch (cryptHash->cryptHashAlgorithm)
  {
    case CRYPT_HASH_ALGORITHM_NONE:
      equalsFlag =    (cryptHash->none.dataLength == bufferLength)
                   && memEquals(buffer,
                                bufferLength,
                                cryptHash->none.data,
                                cryptHash->none.dataLength
                               );
      break;
    case CRYPT_HASH_ALGORITHM_SHA2_224:
    case CRYPT_HASH_ALGORITHM_SHA2_256:
    case CRYPT_HASH_ALGORITHM_SHA2_384:
    case CRYPT_HASH_ALGORITHM_SHA2_512:
      #ifdef HAVE_GCRYPT
        {
          int gcryAlgo;

          gcryAlgo = GCRY_MD_NONE;
          switch (cryptHash->cryptHashAlgorithm)
          {
            case CRYPT_HASH_ALGORITHM_SHA2_224: gcryAlgo = GCRY_MD_SHA224; break;
            case CRYPT_HASH_ALGORITHM_SHA2_256: gcryAlgo = GCRY_MD_SHA256; break;
            case CRYPT_HASH_ALGORITHM_SHA2_384: gcryAlgo = GCRY_MD_SHA384; break;
            case CRYPT_HASH_ALGORITHM_SHA2_512: gcryAlgo = GCRY_MD_SHA512; break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }

          equalsFlag =    (gcry_md_get_algo_dlen(gcryAlgo) == bufferLength)
                       && memEquals(gcry_md_read(cryptHash->gcry_md_hd,gcryAlgo),
                                    bufferLength,
                                    buffer,
                                    bufferLength
                                   );
        }
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(buffer);
        UNUSED_VARIABLE(bufferLength);
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return equalsFlag;
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
Errors Crypt_initMAC(CryptMAC           *cryptMAC,
                     CryptMACAlgorithms cryptMACAlgorithm,
                     const void         *keyData,
                     uint               keyDataLength
                    )
#else /* not NDEBUG */
Errors __Crypt_initMAC(const char         *__fileName__,
                       ulong              __lineNb__,
                       CryptMAC           *cryptMAC,
                       CryptMACAlgorithms cryptMACAlgorithm,
                       const void         *keyData,
                       uint               keyDataLength
                      )
#endif /* NDEBUG */
{
  #ifdef HAVE_GCRYPT
    gcry_error_t gcryptError;
  #endif /* HAVE_GCRYPT */

  assert(cryptMAC != NULL);

  // init variables
  cryptMAC->cryptMACAlgorithm = cryptMACAlgorithm;

  // init crypt algorithm
  switch (cryptMACAlgorithm)
  {
    case CRYPT_MAC_ALGORITHM_SHA2_224:
    case CRYPT_MAC_ALGORITHM_SHA2_256:
    case CRYPT_MAC_ALGORITHM_SHA2_384:
    case CRYPT_MAC_ALGORITHM_SHA2_512:
      #ifdef HAVE_GCRYPT
        {
          int macAlgorithm;

          macAlgorithm = GCRY_MAC_NONE;
          switch (cryptMACAlgorithm)
          {
            case CRYPT_MAC_ALGORITHM_SHA2_224: macAlgorithm = GCRY_MAC_HMAC_SHA224; break;
            case CRYPT_MAC_ALGORITHM_SHA2_256: macAlgorithm = GCRY_MAC_HMAC_SHA256; break;
            case CRYPT_MAC_ALGORITHM_SHA2_384: macAlgorithm = GCRY_MAC_HMAC_SHA384; break;
            case CRYPT_MAC_ALGORITHM_SHA2_512: macAlgorithm = GCRY_MAC_HMAC_SHA512; break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }

          gcryptError = gcry_mac_open(&cryptMAC->gcry_mac_hd,macAlgorithm,GCRY_MAC_FLAG_SECURE,NULL);
          if (gcryptError != 0)
          {
            char buffer[128];

            gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
            return ERRORX_(INIT_MAC,gcryptError,"%s",buffer);
          }

          gcryptError = gcry_mac_setkey(cryptMAC->gcry_mac_hd,keyData,keyDataLength);
          if (gcryptError != 0)
          {
            char buffer[128];

            gpg_strerror_r(gcryptError,buffer,sizeof(buffer));
            gcry_mac_close(cryptMAC->gcry_mac_hd);
            return ERRORX_(INIT_MAC,gcryptError,"%s",buffer);
          }
        }
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(keyData);
        UNUSED_VARIABLE(keyDataLength);

        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(cryptMAC,CryptMAC);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptMAC,CryptMAC);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
void Crypt_doneMAC(CryptMAC *cryptMAC)
#else /* not NDEBUG */
void __Crypt_doneMAC(const char *__fileName__,
                     ulong      __lineNb__,
                     CryptMAC   *cryptMAC
                    )
#endif /* NDEBUG */
{
  assert(cryptMAC != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(cryptMAC,CryptMAC);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptMAC,CryptMAC);
  #endif /* NDEBUG */

  #ifdef HAVE_GCRYPT
    gcry_mac_close(cryptMAC->gcry_mac_hd);
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptMAC);
  #endif /* HAVE_GCRYPT */
}

#ifdef NDEBUG
  CryptMAC *Crypt_newMAC(CryptMACAlgorithms cryptMACAlgorithm,
                         const void         *keyData,
                         uint               keyDataLength
                        )
#else /* not NDEBUG */
  CryptMAC *__Crypt_newMAC(const char         *__fileName__,
                           ulong              __lineNb__,
                           CryptMACAlgorithms cryptMACAlgorithm,
                           const void         *keyData,
                           uint               keyDataLength
                          )
#endif /* NDEBUG */
{
  CryptMAC *cryptMAC;

  cryptMAC = (CryptMAC*)malloc(sizeof(CryptMAC));
  if (cryptMAC == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  #ifndef NDEBUG
    __Crypt_initMAC(__fileName__,__lineNb__,cryptMAC,cryptMACAlgorithm,keyData,keyDataLength);
  #else /* not NDEBUG */
    Crypt_initMAC(cryptMAC,cryptMACAlgorithm,keyData,keyDataLength);
  #endif /* NDEBUG */

  return cryptMAC;
}

#ifdef NDEBUG
  void Crypt_deleteMAC(CryptMAC *cryptMAC)
#else /* not NDEBUG */
  void __Crypt_deleteMAC(const char *__fileName__,
                         ulong      __lineNb__,
                         CryptMAC   *cryptMAC
                        )
#endif /* NDEBUG */
{
  if (cryptMAC != NULL)
  {
    #ifndef NDEBUG
      __Crypt_doneMAC(__fileName__,__lineNb__,cryptMAC);
    #else /* not NDEBUG */
      Crypt_doneMAC(cryptMAC);
    #endif /* NDEBUG */
    free(cryptMAC);
  }
}

void Crypt_resetMAC(CryptMAC *cryptMAC)
{
  assert(cryptMAC != NULL);

  #ifdef HAVE_GCRYPT
    gcry_mac_reset(cryptMAC->gcry_mac_hd);
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptMAC);
  #endif /* HAVE_GCRYPT */
}

void Crypt_updateMAC(CryptMAC *cryptMAC,
                     void     *buffer,
                     ulong    bufferLength
                    )
{
  assert(cryptMAC != NULL);

  #ifdef HAVE_GCRYPT
    gcry_mac_write(cryptMAC->gcry_mac_hd,buffer,bufferLength);
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptMAC);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferLength);
  #endif /* HAVE_GCRYPT */
}

uint Crypt_getMACLength(const CryptMAC *cryptMAC)
{
  uint macLength;

  assert(cryptMAC != NULL);

  macLength = 0;
  switch (cryptMAC->cryptMACAlgorithm)
  {
    case CRYPT_MAC_ALGORITHM_SHA2_224:
    case CRYPT_MAC_ALGORITHM_SHA2_256:
    case CRYPT_MAC_ALGORITHM_SHA2_384:
    case CRYPT_MAC_ALGORITHM_SHA2_512:
      #ifdef HAVE_GCRYPT
        {
          int macAlgorithm;

          macAlgorithm = GCRY_MAC_NONE;
          switch (cryptMAC->cryptMACAlgorithm)
          {
            case CRYPT_MAC_ALGORITHM_SHA2_224: macAlgorithm = GCRY_MAC_HMAC_SHA224; break;
            case CRYPT_MAC_ALGORITHM_SHA2_256: macAlgorithm = GCRY_MAC_HMAC_SHA256; break;
            case CRYPT_MAC_ALGORITHM_SHA2_384: macAlgorithm = GCRY_MAC_HMAC_SHA384; break;
            case CRYPT_MAC_ALGORITHM_SHA2_512: macAlgorithm = GCRY_MAC_HMAC_SHA512; break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }

          macLength = gcry_mac_get_algo_maclen(macAlgorithm);
        }
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return macLength;
}

void *Crypt_getMAC(const CryptMAC *cryptMAC,
                   void           *buffer,
                   uint           bufferSize,
                   uint           *macLength
                  )
{
  assert(cryptMAC != NULL);
  assert(buffer != NULL);

  switch (cryptMAC->cryptMACAlgorithm)
  {
    case CRYPT_MAC_ALGORITHM_SHA2_224:
    case CRYPT_MAC_ALGORITHM_SHA2_256:
    case CRYPT_MAC_ALGORITHM_SHA2_384:
    case CRYPT_MAC_ALGORITHM_SHA2_512:
      #ifdef HAVE_GCRYPT
        {
          int    macAlgorithm;
          size_t n;

          macAlgorithm = GCRY_MAC_NONE;
          switch (cryptMAC->cryptMACAlgorithm)
          {
            case CRYPT_MAC_ALGORITHM_SHA2_224: macAlgorithm = GCRY_MAC_HMAC_SHA224; break;
            case CRYPT_MAC_ALGORITHM_SHA2_256: macAlgorithm = GCRY_MAC_HMAC_SHA256; break;
            case CRYPT_MAC_ALGORITHM_SHA2_384: macAlgorithm = GCRY_MAC_HMAC_SHA384; break;
            case CRYPT_MAC_ALGORITHM_SHA2_512: macAlgorithm = GCRY_MAC_HMAC_SHA512; break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }

          n = gcry_mac_get_algo_maclen(macAlgorithm);
          if (n > bufferSize)
          {
            return NULL;
          }

          if (macLength != NULL) (*macLength) = n;
          gcry_mac_read(cryptMAC->gcry_mac_hd,buffer,&n);
        }
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(buffer);
        UNUSED_VARIABLE(bufferSize);
        UNUSED_VARIABLE(macLength);

        return NULL;
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  return buffer;
}

bool Crypt_verifyMAC(const CryptMAC *cryptMAC,
                     void           *mac,
                     uint           macLength
                    )
{
  bool equalsFlag;

  assert(cryptMAC != NULL);

  equalsFlag = FALSE;
  switch (cryptMAC->cryptMACAlgorithm)
  {
    case CRYPT_MAC_ALGORITHM_SHA2_224:
    case CRYPT_MAC_ALGORITHM_SHA2_256:
    case CRYPT_MAC_ALGORITHM_SHA2_384:
    case CRYPT_MAC_ALGORITHM_SHA2_512:
      #ifdef HAVE_GCRYPT
        {
          equalsFlag = (gcry_mac_verify(cryptMAC->gcry_mac_hd,mac,macLength) == 0);
        }
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(mac);
        UNUSED_VARIABLE(macLength);
      #endif /* HAVE_GCRYPT */
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

   return equalsFlag;
}

#ifndef NDEBUG
void Crypt_dumpKey(const CryptKey *cryptKey)
{
  #ifdef HAVE_GCRYPT
    gcry_sexp_t   sexpToken;
    gcry_sexp_t   rsaToken;
    gcry_sexp_t   nToken,eToken;
    gcry_mpi_t    n,e;
    gcry_sexp_t   dToken,pToken,qToken,uToken;
    gcry_mpi_t    d,p,q,u;
    unsigned char *s;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(cryptKey);

  if (cryptKey->dataLength > 0)
  {
    fprintf(stderr,"Crypt key: %ubytes\n",cryptKey->dataLength);
    debugDumpMemory(cryptKey->data,cryptKey->dataLength,FALSE);
  }

//gcry_sexp_dump(cryptKey->key);

  #ifdef HAVE_GCRYPT
    sexpToken = gcry_sexp_find_token(cryptKey->key,"public-key",0);
    if (sexpToken != NULL)
    {
//      fputs("Public key:\n",stderr);

      rsaToken = gcry_sexp_find_token(sexpToken,"rsa",0);
      nToken   = gcry_sexp_find_token(rsaToken,"n",0);
      eToken   = gcry_sexp_find_token(rsaToken,"e",0);
//fprintf(stderr,"%s, %d: rsa\n",__FILE__,__LINE__); gcry_sexp_dump(rsaToken);
//fprintf(stderr,"%s, %d: nToken\n",__FILE__,__LINE__); gcry_sexp_dump(nToken);
//fprintf(stderr,"%s, %d: eToken\n",__FILE__,__LINE__); gcry_sexp_dump(eToken);

      n = gcry_sexp_nth_mpi(nToken,1,GCRYMPI_FMT_USG);
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,n);
      fprintf(stderr,"  n=%s\n",s);
      free(s);

      e = gcry_sexp_nth_mpi(eToken,1,GCRYMPI_FMT_USG);
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,e);
      fprintf(stderr,"  e=%s\n",s);
      free(s);

      gcry_mpi_release(e);
      gcry_mpi_release(n);

      gcry_sexp_release(eToken);
      gcry_sexp_release(nToken);

      gcry_sexp_release(rsaToken);
      gcry_sexp_release(sexpToken);
    }

    sexpToken = gcry_sexp_find_token(cryptKey->key,"private-key",0);
    if (sexpToken != NULL)
    {
//      fputs("Private key:\n",stderr);

      rsaToken = gcry_sexp_find_token(sexpToken,"rsa",0);
      nToken   = gcry_sexp_find_token(rsaToken,"n",0);
      eToken   = gcry_sexp_find_token(rsaToken,"e",0);
      dToken   = gcry_sexp_find_token(rsaToken,"d",0);
      pToken   = gcry_sexp_find_token(rsaToken,"p",0);
      qToken   = gcry_sexp_find_token(rsaToken,"q",0);
      uToken   = gcry_sexp_find_token(rsaToken,"u",0);
//fprintf(stderr,"%s, %d: rsa\n",__FILE__,__LINE__); gcry_sexp_dump(rsaToken);
//fprintf(stderr,"%s, %d: nToken\n",__FILE__,__LINE__); gcry_sexp_dump(nToken);
//fprintf(stderr,"%s, %d: eToken\n",__FILE__,__LINE__); gcry_sexp_dump(eToken);
//fprintf(stderr,"%s, %d: dToken\n",__FILE__,__LINE__); gcry_sexp_dump(dToken);
//fprintf(stderr,"%s, %d: pToken\n",__FILE__,__LINE__); gcry_sexp_dump(pToken);
//fprintf(stderr,"%s, %d: qToken\n",__FILE__,__LINE__); gcry_sexp_dump(qToken);
//fprintf(stderr,"%s, %d: uToken\n",__FILE__,__LINE__); gcry_sexp_dump(uToken);

      n = gcry_sexp_nth_mpi(nToken,1,GCRYMPI_FMT_USG);
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,n);
      fprintf(stderr,"  n=%s\n",s);
      free(s);
      e = gcry_sexp_nth_mpi(eToken,1,GCRYMPI_FMT_USG);
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,e);
      fprintf(stderr,"  e=%s\n",s);
      free(s);
      d = gcry_sexp_nth_mpi(dToken,1,GCRYMPI_FMT_USG);
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,d);
      fprintf(stderr,"  d=%s\n",s);
      free(s);
      p = gcry_sexp_nth_mpi(pToken,1,GCRYMPI_FMT_USG);
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,p);
      fprintf(stderr,"  p=%s\n",s);
      free(s);
      q = gcry_sexp_nth_mpi(qToken,1,GCRYMPI_FMT_USG);
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,q);
      fprintf(stderr,"  q=%s\n",s);
      free(s);
      u = gcry_sexp_nth_mpi(uToken,1,GCRYMPI_FMT_USG);
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,u);
      fprintf(stderr,"  u=%s\n",s);
      free(s);

      gcry_mpi_release(u);
      gcry_mpi_release(q);
      gcry_mpi_release(p);
      gcry_mpi_release(d);
      gcry_mpi_release(e);
      gcry_mpi_release(n);

      gcry_sexp_release(uToken);
      gcry_sexp_release(qToken);
      gcry_sexp_release(pToken);
      gcry_sexp_release(dToken);
      gcry_sexp_release(eToken);
      gcry_sexp_release(nToken);

      gcry_sexp_release(rsaToken);
      gcry_sexp_release(sexpToken);
    }
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);
  #endif /* HAVE_GCRYPT */
}

void Crypt_dumpSalt(const CryptSalt *cryptSalt)
{
  assert(cryptSalt != NULL);
  assert(cryptSalt->dataLength <= CRYPT_SALT_LENGTH);

  if (cryptSalt->dataLength > 0)
  {
    fprintf(stderr,"Crypt salt: %ubytes\n",cryptSalt->dataLength);
    debugDumpMemory(cryptSalt->data,cryptSalt->dataLength,FALSE);
  }
}

void Crypt_dumpHash(const CryptHash *cryptHash)
{
  const void *hashData;
  uint       hashLength;

  assert(cryptHash != NULL);

  fprintf(stderr,"'%s':\n",Crypt_hashAlgorithmToString(cryptHash->cryptHashAlgorithm,NULL));
  hashData = Crypt_getHash(cryptHash,NULL,0,&hashLength);
  if (hashData != NULL)
  {
    debugDumpMemory(hashData,hashLength,FALSE);
  }
}

void Crypt_dumpMAC(const CryptMAC *cryptMAC)
{
  assert(cryptMAC != NULL);

HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
