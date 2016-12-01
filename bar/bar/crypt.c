/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver crypt functions
* Systems: all
*
\***********************************************************************/

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

#include "global.h"
#include "strings.h"
#include "lists.h"
#include "misc.h"

#include "archive_format.h"
#include "chunks.h"
#include "files.h"
#include "passwords.h"

#include "crypt.h"

/****************** Conditional compilation switches *******************/

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
#define KEY_DERIVE_ITERATIONS     50000
#define KEY_DERIVE_KEY_SIZE       (512/8)

// PKCS1 encoded message buffer for RSA encryption/decryption
#define PKCS1_ENCODED_MESSAGE_LENGTH         (512/8)
#define PKCS1_RANDOM_KEY_LENGTH              (PKCS1_ENCODED_MESSAGE_LENGTH*8-(1+1+8+1)*8)
#define PKCS1_ENCODED_MESSAGE_PADDING_LENGTH (PKCS1_ENCODED_MESSAGE_LENGTH-(PKCS1_RANDOM_KEY_LENGTH+7)/8-1-1-1)

// used symmetric encryption algorithm in RSA hybrid encryption
#define SECRET_KEY_CRYPT_ALGORITHM CRYPT_ALGORITHM_AES256

/***************************** Datatypes *******************************/

typedef struct
{
  uint32 crc;
  uint   dataLength;
  byte   data[0];
} FileCryptKey;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifdef HAVE_GCRYPT
  GCRY_THREAD_OPTION_PTHREAD_IMPL;
#endif /* HAVE_GCRYPT */

/*---------------------------------------------------------------------*/

Errors Crypt_initAll(void)
{
  #ifdef HAVE_GCRYPT
    // enable pthread-support before any other function is called
    gcry_control(GCRYCTL_SET_THREAD_CBS,&gcry_threads_pthread);

    // check version and do internal library init
    if (!gcry_check_version(GCRYPT_VERSION))
    {
      return ERRORX_(INIT_CRYPT,0,"Wrong gcrypt version (needed: %d)",GCRYPT_VERSION);
    }

//TODO: OK?
//    gcry_control(GCRYCTL_DISABLE_SECMEM,0);
    gcry_control(GCRYCTL_INIT_SECMEM,16*1024,0);
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED,0);
//    gcry_control(GCRYCTL_ENABLE_QUICK_RANDOM,0);
    #ifndef NDEBUG
//NYI: required/useful?
//      gcry_control(GCRYCTL_SET_DEBUG_FLAGS,1,0);
    #endif
  #endif /* HAVE_GCRYPT */

  return ERROR_NONE;
}

void Crypt_doneAll(void)
{
}

bool Crypt_isSymmetricSupported(void)
{
  #ifdef HAVE_GCRYPT
    return TRUE;
  #else /* not HAVE_GCRYPT */
    return FALSE;
  #endif /* HAVE_GCRYPT */
}

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
  assert(buffer != NULL);

  #ifdef HAVE_GCRYPT
    gcry_create_nonce((unsigned char*)buffer,(size_t)length);
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(length);
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_getKeyLength(CryptAlgorithms cryptAlgorithm,
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
          gcryptError = gcry_cipher_algo_info(gcryptAlgorithm,
                                              GCRYCTL_GET_KEYLEN,
                                              NULL,
                                              &n
                                             );
          if (gcryptError != 0)
          {
            return ERRORX_(INIT_CIPHER,
                           0,
                           "Cannot detect block length of '%s' cipher (error: %s)",
                           gcry_cipher_algo_name(gcryptAlgorithm),
                           gpg_strerror(gcryptError)
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

Errors Crypt_getBlockLength(CryptAlgorithms cryptAlgorithm,
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
          gcryptError = gcry_cipher_algo_info(gcryptAlgorithm,
                                              GCRYCTL_GET_BLKLEN,
                                              NULL,
                                              &n
                                             );
          if (gcryptError != 0)
          {
            return ERRORX_(INIT_CIPHER,
                           0,
                           "Cannot detect block length of '%s' cipher (error: %s)",
                           gcry_cipher_algo_name(gcryptAlgorithm),
                           gpg_strerror(gcryptError)
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

/*---------------------------------------------------------------------*/

#warning remove!
#define _CTS_ONCE

#ifdef NDEBUG
Errors Crypt_init(CryptInfo       *cryptInfo,
                  CryptAlgorithms cryptAlgorithm,
                  uint            cryptMode,
                  const Password  *password,
                  const byte      *salt,
                  uint            saltLength
                 )
#else /* not NDEBUG */
Errors __Crypt_init(const char      *__fileName__,
                    ulong           __lineNb__,
                    CryptInfo       *cryptInfo,
                    CryptAlgorithms cryptAlgorithm,
                    uint            cryptMode,
                    const Password  *password,
                    const byte      *salt,
                    uint            saltLength
                   )
#endif /* NDEBUG */
{
  assert(cryptInfo != NULL);
  assert(saltLength <= sizeof(cryptInfo->salt));

  // init variables
  cryptInfo->cryptAlgorithm = cryptAlgorithm;
  if (salt != NULL)
  {
    memcpy(cryptInfo->salt,salt,MIN(sizeof(cryptInfo->salt),saltLength));
    cryptInfo->saltLength = saltLength;
  }
  else
  {
    cryptInfo->saltLength = 0;
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
          uint         passwordLength;
          int          gcryptAlgorithm;
          int          gcryptMode;
          unsigned int gcryptFlags;
          gcry_error_t gcryptError;
          size_t       n;
          uint         maxKeyLength;
          uint         keyLength;
          char         key[MAX_KEY_SIZE/8];
          uint         z;
          Errors       error;

          // check password
          if (password == NULL)
          {
            return ERROR_NO_CRYPT_PASSWORD;
          }
          passwordLength = Password_length(password);
          if (passwordLength <= 0)
          {
            return ERROR_NO_CRYPT_PASSWORD;
          }

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
          gcryptMode = GCRY_CIPHER_MODE_NONE;
          if ((cryptMode & CRYPT_MODE_CBC) == CRYPT_MODE_CBC) gcryptMode = GCRY_CIPHER_MODE_CBC;
          gcryptFlags = 0;
          if ((cryptMode & CRYPT_MODE_CTS) == CRYPT_MODE_CTS) gcryptFlags |= GCRY_CIPHER_CBC_CTS;

          // get max. key length, block length
          gcryptError = gcry_cipher_algo_info(gcryptAlgorithm,
                                              GCRYCTL_GET_KEYLEN,
                                              NULL,
                                              &n
                                             );
          if (gcryptError != 0)
          {
            return ERRORX_(INIT_CIPHER,
                           0,
                           "Cannot detect max. key length of cipher '%s' (error: %s)",
                           gcry_cipher_algo_name(gcryptAlgorithm),
                           gpg_strerror(gcryptError)
                          );
          }
          maxKeyLength = n*8;
          gcryptError = gcry_cipher_algo_info(gcryptAlgorithm,
                                              GCRYCTL_GET_BLKLEN,
                                              NULL,
                                              &n
                                             );
          if (gcryptError != 0)
          {
            return ERRORX_(INIT_CIPHER,
                           0,
                           "Cannot detect block length of cipher '%s' (error: %s)",
                           gcry_cipher_algo_name(gcryptAlgorithm),
                           gpg_strerror(gcryptError)
                          );
          }
          cryptInfo->blockLength = n;

          // get key length [bits]
          keyLength = 0;
          switch (cryptAlgorithm)
          {
            case CRYPT_ALGORITHM_3DES:        keyLength = 192; break;
            case CRYPT_ALGORITHM_CAST5:       keyLength = 128; break;
            case CRYPT_ALGORITHM_BLOWFISH:    keyLength = 128; break;
            case CRYPT_ALGORITHM_AES128:      keyLength = 128; break;
            case CRYPT_ALGORITHM_AES192:      keyLength = 192; break;
            case CRYPT_ALGORITHM_AES256:      keyLength = 256; break;
            case CRYPT_ALGORITHM_TWOFISH128:  keyLength = 128; break;
            case CRYPT_ALGORITHM_TWOFISH256:  keyLength = 256; break;
            case CRYPT_ALGORITHM_SERPENT128:  keyLength = 128; break;
            case CRYPT_ALGORITHM_SERPENT192:  keyLength = 192; break;
            case CRYPT_ALGORITHM_SERPENT256:  keyLength = 256; break;
            case CRYPT_ALGORITHM_CAMELLIA128: keyLength = 128; break;
            case CRYPT_ALGORITHM_CAMELLIA192: keyLength = 192; break;
            case CRYPT_ALGORITHM_CAMELLIA256: keyLength = 256; break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }
          if (keyLength > maxKeyLength)
          {
            return ERRORX_(INIT_CIPHER,
                           0,
                           "Cipher '%s' does not support %dbit keys\n",
                           gcry_cipher_algo_name(gcryptAlgorithm),
                           keyLength
                          );
          }

          // init cipher
          gcryptError = gcry_cipher_open(&cryptInfo->gcry_cipher_hd,
                                         gcryptAlgorithm,
                                         gcryptMode,
                                         gcryptFlags
                                        );
          if (gcryptError != 0)
          {
            return ERRORX_(INIT_CIPHER,
                           0,
                           "Init cipher '%s' failed (error: %s)",
                           gcry_cipher_algo_name(gcryptAlgorithm),
                           gpg_strerror(gcryptError)
                          );
          }

          // set key
          assert(sizeof(key) >= (keyLength+7)/8);
          memset(key,0,sizeof(key));
          for (z = 0; z < (keyLength+7)/8; z++)
          {
            key[z] = Password_getChar(password,z);
          }
          gcryptError = gcry_cipher_setkey(cryptInfo->gcry_cipher_hd,
                                           key,
                                           (keyLength+7)/8
                                          );
          if (gcryptError != 0)
          {
            error = ERRORX_(INIT_CIPHER,
                            0,
                            "Cannot set key for cipher '%s' with %dbit (error: %s)",
                            gcry_cipher_algo_name(gcryptAlgorithm),
                            keyLength,
                            gpg_strerror(gcryptError)
                           );
            gcry_cipher_close(cryptInfo->gcry_cipher_hd);
            return error;
          }

#if 1
fprintf(stderr,"%s, %d: set IV 1\n",__FILE__,__LINE__); debugDumpMemory(salt,cryptInfo->blockLength,0);
          // set salt as IV
          if (cryptInfo->saltLength > 0)
          {
            if (cryptInfo->saltLength < cryptInfo->blockLength)
            {
              gcry_cipher_close(cryptInfo->gcry_cipher_hd);
              return ERROR_INVALID_SALT_LENGTH;
            }
            gcryptError = gcry_cipher_setiv(cryptInfo->gcry_cipher_hd,
                                            cryptInfo->salt,
                                            cryptInfo->blockLength
                                           );
            if (gcryptError != 0)
            {
              error = ERRORX_(INIT_CIPHER,
                              0,
                              "Cannot set cipher IV (error: %s)",
                              gpg_strerror(gcryptError)
                             );
              gcry_cipher_close(cryptInfo->gcry_cipher_hd);
              return error;
            }
          }
#endif /* 0 */

//TODO
#warning remove
#ifdef CBS_ONCE
          gcryptError = gcry_cipher_cts(cryptInfo->gcry_cipher_hd,TRUE);
          if (gcryptError != 0)
          {
            return ERROR_ENCRYPT_FAIL;
          }
#endif
        }
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(password);
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
    DEBUG_ADD_RESOURCE_TRACE(cryptInfo,sizeof(CryptInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptInfo,sizeof(CryptInfo));
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
    DEBUG_REMOVE_RESOURCE_TRACE(cryptInfo,sizeof(CryptInfo));
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptInfo,sizeof(CryptInfo));
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

Errors Crypt_reset(CryptInfo *cryptInfo, uint64 seed)
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

          if (cryptInfo->saltLength > 0)
          {
            // set IV
            gcryptError = gcry_cipher_setiv(cryptInfo->gcry_cipher_hd,
                                            cryptInfo->salt,
                                            cryptInfo->blockLength
                                         );
            if (gcryptError != 0)
            {
              return ERRORX_(INIT_CIPHER,
                             0,
                             "Cannot set cipher IV (error: %s)",
                             gpg_strerror(gcryptError)
                            );
            }
          }

//TODO
#warning remove
#ifdef CBS_ONCE
          gcryptError = gcry_cipher_cts(cryptInfo->gcry_cipher_hd,TRUE);
          if (gcryptError != 0)
          {
            return ERROR_ENCRYPT_FAIL;
          }
#endif
        }
      #else /* not HAVE_GCRYPT */
        UNUSED_VARIABLE(seed);
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

fprintf(stderr,"%s, %d: Crypt_encrypt\n",__FILE__,__LINE__);
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

#if 0
#ifndef CBS_ONCE
        gcryptError = gcry_cipher_cts(cryptInfo->gcry_cipher_hd,TRUE);
        if (gcryptError != 0)
        {
          return ERROR_ENCRYPT_FAIL;
        }
#endif
#endif

        gcryptError = gcry_cipher_encrypt(cryptInfo->gcry_cipher_hd,
                                          buffer,
                                          bufferLength,
                                          NULL,
                                          0
                                         );
        if (gcryptError != 0)
        {
          return ERROR_ENCRYPT_FAIL;
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

#if 0
#ifndef CTS_ONCE
        gcryptError = gcry_cipher_cts(cryptInfo->gcry_cipher_hd,TRUE);
        if (gcryptError != 0)
        {
          return ERROR_ENCRYPT_FAIL;
        }
#endif
#endif

        gcryptError = gcry_cipher_decrypt(cryptInfo->gcry_cipher_hd,
                                          buffer,
                                          bufferLength,
                                          NULL,
                                          0
                                         );
        if (gcryptError != 0)
        {
          return ERROR_ENCRYPT_FAIL;
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

Errors Crypt_encryptBytes(CryptInfo *cryptInfo,
                          void      *buffer,
                          ulong      bufferLength
                         )
{
  #ifdef HAVE_GCRYPT
    gcry_error_t gcryptError;
  #endif

  assert(cryptInfo != NULL);
  assert(buffer != NULL);

fprintf(stderr,"%s, %d: Crypt_encryptBytes\n",__FILE__,__LINE__);
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

#if 0
        gcryptError = gcry_cipher_cts(cryptInfo->gcry_cipher_hd,FALSE);
        if (gcryptError != 0)
        {
          return ERROR_ENCRYPT_FAIL;
        }
#endif

        gcryptError = gcry_cipher_encrypt(cryptInfo->gcry_cipher_hd,
                                          buffer,
                                          bufferLength,
                                          NULL,
                                          0
                                         );
        if (gcryptError != 0)
        {
          return ERROR_ENCRYPT_FAIL;
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

#if 0
        gcryptError = gcry_cipher_cts(cryptInfo->gcry_cipher_hd,FALSE);
        if (gcryptError != 0)
        {
          return ERROR_ENCRYPT_FAIL;
        }
#endif

        gcryptError = gcry_cipher_decrypt(cryptInfo->gcry_cipher_hd,
                                          buffer,
                                          bufferLength,
                                          NULL,
                                          0
                                         );
        if (gcryptError != 0)
        {
          return ERROR_ENCRYPT_FAIL;
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

bool Crypt_isAsymmetricSupported(void)
{
  #ifdef HAVE_GCRYPT
    return TRUE;
  #else /* not HAVE_GCRYPT */
    return FALSE;
  #endif /* HAVE_GCRYPT */
}

void Crypt_initKey(CryptKey          *cryptKey,
                   CryptPaddingTypes cryptPaddingType
                  )
{
  assert(cryptKey != NULL);

  #ifdef HAVE_GCRYPT
    cryptKey->key              = NULL;
    cryptKey->cryptPaddingType = cryptPaddingType;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);
    UNUSED_VARIABLE(cryptPaddingType);
  #endif /* HAVE_GCRYPT */
}

void Crypt_doneKey(CryptKey *cryptKey)
{
  assert(cryptKey != NULL);

  #ifdef HAVE_GCRYPT
    if (cryptKey->key != NULL) gcry_sexp_release(cryptKey->key);
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_getKeyData(CryptKey       *cryptKey,
                        void           **keyData,
                        uint           *keyDataLength,
                        const Password *password,
                        const byte     *salt,
                        uint           saltLength
                       )
{
  #ifdef HAVE_GCRYPT
    uint         blockLength;
    gcry_sexp_t  sexpToken;
    size_t       dataLength;
    uint         fileCryptKeyLength;
    FileCryptKey *fileCryptKey;
    CryptInfo    cryptInfo;
    Errors       error;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);
  assert(keyData != NULL);
  assert(keyDataLength != NULL);

  #ifdef HAVE_GCRYPT
    // get crypt algorithm block length
    error = Crypt_getBlockLength(SECRET_KEY_CRYPT_ALGORITHM,&blockLength);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // find key token
    sexpToken = NULL;
    if (sexpToken == NULL) sexpToken = gcry_sexp_find_token(cryptKey->key,"public-key",0);
    if (sexpToken == NULL) sexpToken = gcry_sexp_find_token(cryptKey->key,"private-key",0);
    if (sexpToken == NULL)
    {
      return ERROR_INVALID_KEY;
    }

    // get data length
    dataLength = gcry_sexp_sprint(sexpToken,GCRYSEXP_FMT_ADVANCED,NULL,0);

    // allocate file key
    fileCryptKeyLength = sizeof(FileCryptKey)+ALIGN(dataLength,blockLength);
    fileCryptKey = (FileCryptKey*)Password_allocSecure(fileCryptKeyLength);
    if (fileCryptKey == NULL)
    {
      gcry_sexp_release(sexpToken);
      return ERROR_INSUFFICIENT_MEMORY;
    }
    memset(fileCryptKey,0,fileCryptKeyLength);
    fileCryptKey->dataLength = htons(dataLength);

    // get key data
    gcry_sexp_sprint(sexpToken,GCRYSEXP_FMT_ADVANCED,(char*)fileCryptKey->data,dataLength);
#if 0
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__); debugDumpMemory(fileCryptKey->data,dataLength,FALSE);
#endif /* 0 */

    // encrypt key
    if (password != NULL)
    {
      // initialize crypt
      error = Crypt_init(&cryptInfo,
                         SECRET_KEY_CRYPT_ALGORITHM,
                         CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                         password,
//TODO
                         NULL,  // salt
                         0  // saltLength
                        );
      if (error != ERROR_NONE)
      {
        Password_freeSecure(fileCryptKey);
        gcry_sexp_release(sexpToken);
        return error;
      }

      // encrypt
      error = Crypt_encrypt(&cryptInfo,(char*)fileCryptKey->data,ALIGN(dataLength,blockLength));
      if (error != ERROR_NONE)
      {
        Crypt_done(&cryptInfo);
        Password_freeSecure(fileCryptKey);
        gcry_sexp_release(sexpToken);
        return error;
      }

      // done crypt
      Crypt_done(&cryptInfo);
    }
#if 0
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__); debugDumpMemory(fileCryptKey->data,dataLength,FALSE);
#endif /* 0 */

    // calculate CRC
    fileCryptKey->crc = htonl(crc32(crc32(0,Z_NULL,0),fileCryptKey->data,dataLength));

    (*keyData      ) = fileCryptKey;
    (*keyDataLength) = fileCryptKeyLength;

    // free resources
    gcry_sexp_release(sexpToken);

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);
    UNUSED_VARIABLE(string);
    UNUSED_VARIABLE(password);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_setKeyData(CryptKey       *cryptKey,
                        const void     *keyData,
                        uint           keyDataLength,
                        const Password *password,
                        const byte     *salt,
                        uint           saltLength
                       )
{
  #ifdef HAVE_GCRYPT
    void         *data;
    uint         blockLength;
    CryptInfo    cryptInfo;
    Errors       error;
    gcry_error_t gcryptError;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);
  assert(keyData != NULL);
  assert(keyDataLength >= sizeof(FileCryptKey));

  #ifdef HAVE_GCRYPT
    // decrypt key
    if (password != NULL)
    {
      // get crypt algorithm block length
      error = Crypt_getBlockLength(SECRET_KEY_CRYPT_ALGORITHM,&blockLength);
      if (error != ERROR_NONE)
      {
        return ERROR_INVALID_BLOCK_LENGTH_;
      }

      // allocate secure memory
      data = Password_allocSecure(keyDataLength);
      if (data == NULL)
      {
        return ERROR_INSUFFICIENT_MEMORY;
      }

      // initialize crypt
      error = Crypt_init(&cryptInfo,
                         SECRET_KEY_CRYPT_ALGORITHM,
                         CRYPT_MODE_CBC|CRYPT_MODE_CTS,
                         password,
                         salt,
                         saltLength
                        );
      if (error != ERROR_NONE)
      {
        Password_freeSecure(data);
        return error;
      }

      // decrypt
      memcpy(data,keyData,keyDataLength);
      error = Crypt_decrypt(&cryptInfo,data,ALIGN(keyDataLength,blockLength));
      if (error != ERROR_NONE)
      {
        Crypt_done(&cryptInfo);
        Password_freeSecure(data);
        return error;
      }

      // done crypt
      Crypt_done(&cryptInfo);
    }
#if 0
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__); debugDumpMemory(secureData,dataLength,FALSE);
#endif /* 0 */

    // create S-expression with key
    if (cryptKey->key != NULL)
    {
      gcry_sexp_release(cryptKey->key);
      cryptKey->key = NULL;
    }
    gcryptError = gcry_sexp_new(&cryptKey->key,
                                (password != NULL) ? data : keyData,
                                keyDataLength,
                                1
                               );
    if (gcryptError != 0)
    {
      if (password != NULL)
      {
        Password_freeSecure(data);
      }
      return ERROR_INVALID_KEY;
    }

    // free resources
    if (password != NULL)
    {
      Password_freeSecure(data);
    }

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);
    UNUSED_VARIABLE(string);
    UNUSED_VARIABLE(password);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_getKeyString(CryptKey       *cryptKey,
                          String         string,
                          const Password *password,
                          const byte     *salt,
                          uint           saltLength
                         )
{
  #ifdef HAVE_GCRYPT
    Errors error;
    void   *keyData;
    uint   keyDataLength;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);
  assert(string != NULL);

  #ifdef HAVE_GCRYPT
    // get encrypted key
    error = Crypt_getKeyData(cryptKey,
                             &keyData,
                             &keyDataLength,
                             password,
                             salt,
                             saltLength
                            );
    if (error != ERROR_NONE)
    {
      Password_freeSecure(keyData);
      return error;
    }

    // encode base64
    Misc_base64Encode(string,(byte*)keyData,keyDataLength);

    // free resources
    Password_freeSecure(keyData);

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);
    UNUSED_VARIABLE(string);
    UNUSED_VARIABLE(password);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_setKeyString(CryptKey       *cryptKey,
                          const String   string,
                          const Password *password,
                          const byte     *salt,
                          uint           saltLength
                         )
{
  #ifdef HAVE_GCRYPT
    Errors       error;
    uint         blockLength;
    uint         fileCryptKeyLength;
    uint         fileCryptKeySize;
    FileCryptKey *fileCryptKey;
    void         *data;
    uint         dataLength;
    uint32       crc;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);
  assert(string != NULL);

  #ifdef HAVE_GCRYPT
    // get crypt algorithm block length
    error = Crypt_getBlockLength(SECRET_KEY_CRYPT_ALGORITHM,&blockLength);
    if (error != ERROR_NONE)
    {
      return ERROR_INVALID_BLOCK_LENGTH_;
    }
//TODO
    UNUSED_VARIABLE(error);

    // get crypt length
    fileCryptKeyLength = Misc_base64DecodeLength(string,STRING_BEGIN);
    if (fileCryptKeyLength < sizeof(FileCryptKey))
    {
      return ERROR_INVALID_KEY;
    }

    // allocate file key
    fileCryptKeySize = offsetof(FileCryptKey,data)+ALIGN(fileCryptKeyLength-offsetof(FileCryptKey,data),blockLength);
    fileCryptKey = (FileCryptKey*)Password_allocSecure(fileCryptKeySize);
    if (fileCryptKey == NULL)
    {
      return ERROR_INSUFFICIENT_MEMORY;
    }
    memset(fileCryptKey,0,fileCryptKeySize);

    // decode base64
    if (Misc_base64Decode((byte*)fileCryptKey,fileCryptKeyLength,string,STRING_BEGIN) == -1)
    {
      Password_freeSecure(fileCryptKey);
      return ERROR_INVALID_KEY;
    }

    // get data, data length
    data       = (char*)fileCryptKey->data;
    dataLength = ntohs(fileCryptKey->dataLength);

    // check key CRC
    crc = crc32(crc32(0,Z_NULL,0),(Bytef*)data,dataLength);
    if (crc != ntohl(fileCryptKey->crc))
    {
      Password_freeSecure(fileCryptKey);
      return ERROR_INVALID_KEY;
    }

    // decrypt and set key
    error = Crypt_setKeyData(cryptKey,
                             data,
                             dataLength,
                             password,
                             salt,
                             saltLength
                            );
    if (error != ERROR_NONE)
    {
      Password_freeSecure(fileCryptKey);
      return ERROR_INVALID_KEY;
    }

    // free resources
    Password_freeSecure(fileCryptKey);

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);
    UNUSED_VARIABLE(string);
    UNUSED_VARIABLE(password);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

String Crypt_getKeyModulus(CryptKey *cryptKey)
{
  #ifdef HAVE_GCRYPT
    gcry_sexp_t   sexpToken;
    gcry_sexp_t   rsaToken;
    gcry_sexp_t   nToken;
    gcry_mpi_t    n;
    unsigned char *s;
    String        string;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);

  #ifdef HAVE_GCRYPT
    // find key token
    sexpToken = NULL;
    if (sexpToken == NULL) sexpToken = gcry_sexp_find_token(cryptKey->key,"public-key",0);
    if (sexpToken == NULL) sexpToken = gcry_sexp_find_token(cryptKey->key,"private-key",0);
    if (sexpToken == NULL)
    {
      return NULL;
    }

    // get RSA, modulus token
    rsaToken = gcry_sexp_find_token(sexpToken,"rsa",0);
    assert(rsaToken != NULL);
    nToken   = gcry_sexp_find_token(rsaToken,"n",0);
    assert(nToken != NULL);

    // get modulus number
    n = gcry_sexp_nth_mpi(nToken,1,GCRYMPI_FMT_USG);

    // format string
    gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,n);
    string = String_newCString((char*)s);
    free(s);

    // free resources
    gcry_mpi_release(n);
    gcry_sexp_release(nToken);
    gcry_sexp_release(rsaToken);
    gcry_sexp_release(sexpToken);

    return string;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);

    return NULL;
  #endif /* HAVE_GCRYPT */
}

String Crypt_getKeyExponent(CryptKey *cryptKey)
{
  #ifdef HAVE_GCRYPT
    gcry_sexp_t   sexpToken;
    gcry_sexp_t   rsaToken;
    gcry_sexp_t   eToken;
    gcry_mpi_t    e;
    unsigned char *s;
    String        string;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);

  #ifdef HAVE_GCRYPT
    // find key token
    sexpToken = NULL;
    if (sexpToken == NULL) sexpToken = gcry_sexp_find_token(cryptKey->key,"public-key",0);
    if (sexpToken == NULL) sexpToken = gcry_sexp_find_token(cryptKey->key,"private-key",0);
    if (sexpToken == NULL)
    {
      return NULL;
    }

    // get RSA, modulus token
    rsaToken = gcry_sexp_find_token(sexpToken,"rsa",0);
    assert(rsaToken != NULL);
    eToken   = gcry_sexp_find_token(rsaToken,"e",0);
    assert(eToken != NULL);

    // get modulus number
    e = gcry_sexp_nth_mpi(eToken,1,GCRYMPI_FMT_USG);

    // format string
    gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,e);
    string = String_newCString((char*)s);
    free(s);

    // free resources
    gcry_mpi_release(e);
    gcry_sexp_release(eToken);
    gcry_sexp_release(rsaToken);
    gcry_sexp_release(sexpToken);

    return string;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);

    return NULL;
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_readKeyFile(CryptKey       *cryptKey,
                         const String   fileName,
                         const Password *password,
                         const byte     *salt,
                         uint           saltLength
                        )
{
  String     data;
  FileHandle fileHandle;
  Errors     error;

  assert(cryptKey != NULL);
  assert(fileName != NULL);

  // check if read is available
  if (!File_exists(fileName))
  {
    return ERRORX_(KEY_NOT_FOUND,0,"%s",String_cString(fileName));
  }

  // read file contents
  data = String_new();
  error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    String_delete(data);
    return error;
  }
  error = File_readLine(&fileHandle,data);
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    String_delete(data);
    return error;
  }
  File_close(&fileHandle);

  // set key data
  error = Crypt_setKeyString(cryptKey,data,password,salt,saltLength);
  if (error != ERROR_NONE)
  {
    String_delete(data);
    return error;
  }

  // free resources */
  String_delete(data);

  return ERROR_NONE;
}

Errors Crypt_writeKeyFile(CryptKey       *cryptKey,
                          const String   fileName,
                          const Password *password,
                          const byte     *salt,
                          uint           saltLength
                         )
{
  String     data;
  Errors     error;
  FileHandle fileHandle;

  assert(cryptKey != NULL);
  assert(fileName != NULL);

  // get key string
  data = String_new();
  error = Crypt_getKeyString(cryptKey,data,password,salt,saltLength);
  if (error != ERROR_NONE)
  {
    String_delete(data);
    return error;
  }

  // write file
  error = File_open(&fileHandle,fileName,FILE_OPEN_CREATE);
  if (error != ERROR_NONE)
  {
    String_delete(data);
    return error;
  }
  error = File_writeLine(&fileHandle,data);
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    String_delete(data);
    return error;
  }
  File_close(&fileHandle);

  // free resources */
  String_delete(data);

  return ERROR_NONE;
}

Errors Crypt_createKeyPair(CryptKey          *publicCryptKey,
                           CryptKey          *privateCryptKey,
                           uint              bits,
                           CryptPaddingTypes cryptPaddingType
                          )
{
  #ifdef HAVE_GCRYPT
    String       description;
    gcry_sexp_t  sexpKeyParameters;
    gcry_sexp_t  sexpKey;
    gcry_error_t gcryptError;
  #endif /* HAVE_GCRYPT */

  assert(publicCryptKey != NULL);
  assert(privateCryptKey != NULL);

  #ifdef HAVE_GCRYPT
    // init keys
    Crypt_initKey(publicCryptKey,cryptPaddingType);
    Crypt_initKey(privateCryptKey,cryptPaddingType);

    // create key parameters
    description = String_format(String_new(),"(genkey (rsa (nbits 4:%d)))",bits);
    gcryptError = gcry_sexp_new(&sexpKeyParameters,
                                String_cString(description),
                                0,
                                1
                               );
    if (gcryptError != 0)
    {
      String_delete(description);
      Crypt_doneKey(privateCryptKey);
      Crypt_doneKey(publicCryptKey);
      return ERRORX_(CREATE_KEY_FAIL,gcryptError,"%s",gpg_strerror(gcryptError));
    }

    // generate keys
    gcryptError = gcry_pk_genkey(&sexpKey,sexpKeyParameters);
    if (gcryptError != 0)
    {
      gcry_sexp_release(sexpKeyParameters);
      String_delete(description);
      Crypt_doneKey(privateCryptKey);
      Crypt_doneKey(publicCryptKey);
      return ERRORX_(CREATE_KEY_FAIL,gcryptError,"%s",gpg_strerror(gcryptError));
    }
    gcry_sexp_release(sexpKeyParameters);
    String_delete(description);
//gcry_sexp_dump(sexpKey);
    publicCryptKey->key  = gcry_sexp_find_token(sexpKey,"public-key",0);
    privateCryptKey->key = gcry_sexp_find_token(sexpKey,"private-key",0);
    gcry_sexp_release(sexpKey);

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(publicCryptKey);
    UNUSED_VARIABLE(privateCryptKey);
    UNUSED_VARIABLE(bits);
    UNUSED_VARIABLE(cryptPaddingType);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_keyEncrypt(const CryptKey *cryptKey,
                        const void     *buffer,
                        uint           bufferLength,
                        void           *encryptBuffer,
                        uint           *encryptBufferLength,
                        uint           maxEncryptBufferLength
                       )
{
  #ifdef HAVE_GCRYPT
//    gcry_mpi_t   n;
//    byte         *p;
//    ulong        z;
    Errors       error;
    gcry_error_t gcryptError;
    gcry_sexp_t  sexpData;
    gcry_sexp_t  sexpEncryptData;
    gcry_sexp_t  sexpToken;
    const char   *encryptData;
    size_t       encryptDataLength;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);
  assert(buffer != NULL);
  assert(encryptBuffer != NULL);
  assert(encryptBufferLength != NULL);

  #ifdef HAVE_GCRYPT
fprintf(stderr,"%s,%d: ---------------------------------\n",__FILE__,__LINE__);
//gcry_sexp_dump(cryptKey->key);
//fprintf(stderr,"%s,%d: %d\n",__FILE__,__LINE__,bufferLength);

// TODO: required?
#if 0
    // create mpi from data: push data bytes into mpi by shift+add
    n = gcry_mpi_new(0);
    if (n == NULL)
    {
      return ERROR_KEY_ENCRYPT_FAIL;
    }
    p = (byte*)buffer;
    for (z = 0; z < bufferLength; z++)
    {
      gcry_mpi_mul_ui(n,n,256);
      gcry_mpi_add_ui(n,n,p[z]);
    }
gcry_mpi_dump(n);fprintf(stderr,"\n");
#endif

fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
asm("int3");
    // create S-expression with data
    switch (cryptKey->cryptPaddingType)
    {
      case CRYPT_PADDING_TYPE_NONE:
        gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags raw) (value %b))",bufferLength,(char*)buffer);
        break;
      case CRYPT_PADDING_TYPE_PKCS1:
        gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags pkcs1) (value %b))",bufferLength,(char*)buffer);
        break;
      case CRYPT_PADDING_TYPE_OAEP:
        gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags oaep) (value %b))",bufferLength,(char*)buffer);
        break;
      default:
        return ERRORX_(KEY_ENCRYPT_FAIL,0,"unknown padding type");
        break;
    }
    if (gcryptError != 0)
    {
      error = ERRORX_(KEY_ENCRYPT_FAIL,gcryptError,"%s",gcry_strerror(gcryptError));
//      gcry_mpi_release(n);
      return error;
    }
//gcry_sexp_dump(sexpData);

    // encrypt
    gcryptError = gcry_pk_encrypt(&sexpEncryptData,sexpData,cryptKey->key);
    if (gcryptError != 0)
    {
      error = ERRORX_(KEY_ENCRYPT_FAIL,gcryptError,"%s",gcry_strerror(gcryptError));
      gcry_sexp_release(sexpData);
//      gcry_mpi_release(n);
      return error;
    }
//gcry_sexp_dump(sexpEncryptData);

    // get encrypted data
    sexpToken = gcry_sexp_find_token(sexpEncryptData,"a",0);
    if (sexpToken == NULL)
    {
      error = ERRORX_(KEY_ENCRYPT_FAIL,gcryptError,"%s",gcry_strerror(gcryptError));
      gcry_sexp_release(sexpEncryptData);
      gcry_sexp_release(sexpData);
//      gcry_mpi_release(n);
      return error;
    }
//gcry_sexp_dump(sexpToken);
    encryptData = gcry_sexp_nth_data(sexpToken,1,&encryptDataLength);
    if (encryptData == NULL)
    {
      error = ERRORX_(KEY_ENCRYPT_FAIL,gcryptError,"%s",gcry_strerror(gcryptError));
      gcry_sexp_release(sexpEncryptData);
      gcry_sexp_release(sexpData);
//      gcry_mpi_release(n);
      return error;
    }
    (*encryptBufferLength) = MIN(encryptDataLength,maxEncryptBufferLength);
    memcpy(encryptBuffer,encryptData,*encryptBufferLength);
    gcry_sexp_release(sexpToken);

    // free resources
    gcry_sexp_release(sexpEncryptData);
    gcry_sexp_release(sexpData);
//    gcry_mpi_release(n);

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferLength);
    UNUSED_VARIABLE(maxEncryptBufferLength);
    UNUSED_VARIABLE(encryptBuffer);
    UNUSED_VARIABLE(encryptBufferLength);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_keyDecrypt(const CryptKey *cryptKey,
                        const void     *encryptBuffer,
                        uint           encryptBufferLength,
                        void           *buffer,
                        uint           *bufferLength,
                        uint           maxBufferLength
                       )
{
  #ifdef HAVE_GCRYPT
    Errors       error;
    gcry_error_t gcryptError;
    gcry_sexp_t  sexpEncryptData;
    gcry_sexp_t  sexpData;
    const char   *data;
    size_t       dataLength;
  #endif /* HAVE_GCRYPT */

  assert(cryptKey != NULL);
  assert(encryptBuffer != NULL);
  assert(buffer != NULL);
  assert(bufferLength != NULL);

  (*bufferLength) = 0;

  if (encryptBufferLength > 0)
  {
    #ifdef HAVE_GCRYPT
      // create S-expression with encrypted data
      switch (cryptKey->cryptPaddingType)
      {
        case CRYPT_PADDING_TYPE_NONE:
          gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (rsa (a %b)))",encryptBufferLength,encryptBuffer);
//          gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (flags raw) (rsa (a %b)))",encryptBufferLength,encryptBuffer);
          break;
        case CRYPT_PADDING_TYPE_PKCS1:
          gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (flags pkcs1) (rsa (a %b)))",encryptBufferLength,encryptBuffer);
          break;
        case CRYPT_PADDING_TYPE_OAEP:
          gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (flags oaep) (rsa (a %b)))",encryptBufferLength,encryptBuffer);
//          gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (flags pss) (rsa (a %b)))",encryptBufferLength,encryptBuffer);
          break;
        default:
          return ERRORX_(KEY_DECRYPT_FAIL,0,"unknown padding type");
          break;
      }
      if (gcryptError != 0)
      {
        return ERRORX_(KEY_DECRYPT_FAIL,gcryptError,"%s",gcry_strerror(gcryptError));
      }
//fprintf(stderr,"%s, %d: encrypted data\n",__FILE__,__LINE__); gcry_sexp_dump(sexpEncryptData);

      // decrypt
      gcryptError = gcry_pk_decrypt(&sexpData,sexpEncryptData,cryptKey->key);
      if (gcryptError != 0)
      {
        error = ERRORX_(KEY_DECRYPT_FAIL,gcryptError,"%s",gcry_strerror(gcryptError));
        gcry_sexp_release(sexpEncryptData);
        return error;
      }
//fprintf(stderr,"%s, %d: plain data\n",__FILE__,__LINE__); gcry_sexp_dump(sexpData);

      // get decrypted data
      data = gcry_sexp_nth_data(sexpData,1,&dataLength);
      if (data == NULL)
      {
        error = ERRORX_(KEY_DECRYPT_FAIL,gcryptError,"%s",gcry_strerror(gcryptError));
        gcry_sexp_release(sexpData);
        gcry_sexp_release(sexpEncryptData);
        return error;
      }
      (*bufferLength) = MIN(dataLength,maxBufferLength);
      memcpy(buffer,data,*bufferLength);

      // free resources
      gcry_sexp_release(sexpData);
      gcry_sexp_release(sexpEncryptData);

      return ERROR_NONE;
    #else /* not HAVE_GCRYPT */
      UNUSED_VARIABLE(cryptKey);
      UNUSED_VARIABLE(encryptBuffer);
      UNUSED_VARIABLE(encryptBufferLength);
      UNUSED_VARIABLE(maxBufferLength);
      UNUSED_VARIABLE(buffer);
      UNUSED_VARIABLE(bufferLength);

      return ERROR_FUNCTION_NOT_SUPPORTED;
    #endif /* HAVE_GCRYPT */
  }
  else
  {
    return ERROR_NONE;
  }
}

Errors Crypt_deriveEncryptKey(Password   *key,
                              Password   *password,
                              const byte *salt,
                              uint       saltLength
                             )
{
  #ifdef HAVE_GCRYPT
    void         *data;
    gcry_error_t gcryptError;
    const void   *p;
  #endif /* HAVE_GCRYPT */

  assert(key != NULL);

  #ifdef HAVE_GCRYPT
    // allocate secure memory
    data = Password_allocSecure(KEY_DERIVE_KEY_SIZE);
    if (data == NULL)
    {
      return ERROR_INSUFFICIENT_MEMORY;
    }

    // derive key
    p = Password_deploy(password);
    gcryptError = gcry_kdf_derive(p,
                                  (size_t)Password_length(password),
                                  KEY_DERIVE_ALGORITHM,
                                  KEY_DERIVE_HASH_ALGORITHM,
                                  salt,
                                  saltLength,
                                  KEY_DERIVE_ITERATIONS,
                                  KEY_DERIVE_KEY_SIZE,
                                  data
                                 );
    Password_undeploy(password);
    if (gcryptError != 0)
    {
      Password_freeSecure(data);
      return ERRORX_(INIT_KEY,
                     0,
                     "%s",
                     gpg_strerror(gcryptError)
                    );
    }

    // set key
    Password_setBuffer(key,data,KEY_DERIVE_KEY_SIZE);

    // free resources
    Password_freeSecure(data);
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(cryptKey);
    UNUSED_VARIABLE(cryptPaddingType);
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_getRandomEncryptKey(Password        *key,
                                 CryptKey        *publicKey,
                                 CryptAlgorithms cryptAlgorithm,
                                 uint            maxEncryptedKeyBufferLength,
                                 void            *encryptedKeyBuffer,
                                 uint            *encryptedKeyBufferLength
                                )
{
  #ifdef HAVE_GCRYPT
    gcry_sexp_t  sexpToken;
    Errors       error;
    uint         keyLength;
    byte         *pkcs1EncodedMessage;
    const char   *p;
    gcry_sexp_t  sexpData;
    gcry_sexp_t  sexpEncryptData;
    const char   *encryptData;
    size_t       encryptDataLength;
    gcry_error_t gcryptError;
  #endif /* HAVE_GCRYPT */

  assert(key != NULL);
  assert(publicKey != NULL);
  assert(encryptedKeyBuffer != NULL);
  assert(encryptedKeyBufferLength != NULL);

  #ifdef HAVE_GCRYPT
//fprintf(stderr,"%s,%d: ---------------------------------\n",__FILE__,__LINE__);
//fprintf(stderr,"%s,%d: public key\n",__FILE__,__LINE__);
//gcry_sexp_dump(publicKey->key);

    // check if public key is available
    if (!Crypt_isKeyAvailable(publicKey))
    {
//TODO: filename
      return ERRORX_(NO_PUBLIC_CRYPT_KEY,0,"crypt-key");
    }
    sexpToken = gcry_sexp_find_token(publicKey->key,"public-key",0);
    if (sexpToken == NULL)
    {
//TODO: filename
      return ERRORX_(NOT_A_PUBLIC_KEY,0,"crypt-key");
    }
    gcry_sexp_release(sexpToken);

    // get key length
    error = Crypt_getKeyLength(cryptAlgorithm,&keyLength);
    if (error != ERROR_NONE)
    {
      return error;
    }
    assert(keyLength > 0);
    if (keyLength > PKCS1_RANDOM_KEY_LENGTH)
    {
      return ERROR_INVALID_KEY_LENGTH;
    }

    // create random key
    Password_random(key,(PKCS1_RANDOM_KEY_LENGTH+7)/8);

    // create padded encoded message block: format 0x00 0x02 <PS random data> 0x00 <key data>; size 512bit
    pkcs1EncodedMessage = Password_allocSecure(PKCS1_ENCODED_MESSAGE_LENGTH);
    if (pkcs1EncodedMessage == NULL)
    {
      return ERROR_KEY_ENCRYPT_FAIL;
    }
    pkcs1EncodedMessage[0] = 0x00;
    pkcs1EncodedMessage[1] = 0x02;
    gcry_randomize((unsigned char*)&pkcs1EncodedMessage[1+1],PKCS1_ENCODED_MESSAGE_PADDING_LENGTH,GCRY_STRONG_RANDOM);
    pkcs1EncodedMessage[1+1+PKCS1_ENCODED_MESSAGE_PADDING_LENGTH] = 0x00;
    p = Password_deploy(key);
    memcpy(&pkcs1EncodedMessage[1+1+PKCS1_ENCODED_MESSAGE_PADDING_LENGTH+1],p,(PKCS1_RANDOM_KEY_LENGTH+7)/8);
    Password_undeploy(key);

    // create S-expression with data
    gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags raw) (value %b))",PKCS1_ENCODED_MESSAGE_LENGTH,(char*)pkcs1EncodedMessage);
    if (gcryptError != 0)
    {
      Password_freeSecure(pkcs1EncodedMessage);
      return ERROR_KEY_ENCRYPT_FAIL;
    }
//fprintf(stderr,"%s,%d: --- randomkey plain data \n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpData);

    // encrypt
    gcryptError = gcry_pk_encrypt(&sexpEncryptData,sexpData,publicKey->key);
    if (gcryptError != 0)
    {
//fprintf(stderr,"%s,%d: %x %s %d\n",__FILE__,__LINE__,gcryptError,gcry_strerror(gcryptError),bufferLength);
      gcry_sexp_release(sexpData);
      Password_freeSecure(pkcs1EncodedMessage);
      return ERROR_KEY_ENCRYPT_FAIL;
    }
//fprintf(stderr,"%s,%d: --- randomkey encrypted data \n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpEncryptData);

    // get encrypted data
    sexpToken = gcry_sexp_find_token(sexpEncryptData,"a",0);
    if (sexpToken == NULL)
    {
      gcry_sexp_release(sexpEncryptData);
      gcry_sexp_release(sexpData);
      Password_freeSecure(pkcs1EncodedMessage);
      return ERROR_KEY_ENCRYPT_FAIL;
    }
//gcry_sexp_dump(sexpToken);
    encryptData = gcry_sexp_nth_data(sexpToken,1,&encryptDataLength);
    if (encryptData == NULL)
    {
      gcry_sexp_release(sexpEncryptData);
      gcry_sexp_release(sexpData);
      Password_freeSecure(pkcs1EncodedMessage);
      return ERROR_KEY_ENCRYPT_FAIL;
    }
    (*encryptedKeyBufferLength) = MIN(encryptDataLength,maxEncryptedKeyBufferLength);
    memcpy(encryptedKeyBuffer,encryptData,(*encryptedKeyBufferLength));
    gcry_sexp_release(sexpToken);
#if 0
{
  int z;
  byte *p=encryptBuffer;
  printf("encryptData: "); for (z=0;z<(*encryptBufferLength);z++,p++) printf("%02x",*p); printf("\n");
  p++;
}
#endif /* 0 */

    // free resources
    gcry_sexp_release(sexpEncryptData);
    gcry_sexp_release(sexpData);
    Password_freeSecure(pkcs1EncodedMessage);

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(key);
    UNUSED_VARIABLE(publicKey);
    UNUSED_VARIABLE(cryptAlgorithm);
    UNUSED_VARIABLE(maxEncryptedKeyBufferLength);
    UNUSED_VARIABLE(encryptedKeyBuffer);
    UNUSED_VARIABLE(encryptedKedBufferLength);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_getDecryptKey(CryptKey   *privateKey,
                           const void *encryptData,
                           uint       encryptDataLength,
                           Password   *password
                          )
{
  #ifdef HAVE_GCRYPT
    gcry_sexp_t  sexpToken;
    gcry_sexp_t  sexpEncryptData;
    gcry_sexp_t  sexpData;
    byte         *pkcs1EncodedMessage;
    const byte   *data;
    size_t       dataLength;
    gcry_error_t gcryptError;
  #endif /* HAVE_GCRYPT */

  assert(privateKey != NULL);
  assert(encryptData != NULL);
  assert(password != NULL);

  #ifdef HAVE_GCRYPT
//fprintf(stderr,"%s,%d: ---------------------------------\n",__FILE__,__LINE__);
//fprintf(stderr,"%s,%d: private key\n",__FILE__,__LINE__);
//gcry_sexp_dump(privateKey->key);

    // check if private key available
    if (!Crypt_isKeyAvailable(privateKey))
    {
//TODO: filename
      return ERRORX_(NO_PRIVATE_CRYPT_KEY,0,"crypt-key");
    }
    sexpToken = gcry_sexp_find_token(privateKey->key,"private-key",0);
    if (sexpToken == NULL)
    {
//TODO: file name
      return ERRORX_(NOT_A_PRIVATE_KEY,0,"crypt-key");
    }
    gcry_sexp_release(sexpToken);

    // create S-expression with encrypted data
    gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (rsa (a %b)))",encryptDataLength,encryptData);
    if (gcryptError != 0)
    {
      return ERROR_KEY_DECRYPT_FAIL;
    }
//fprintf(stderr,"%s,%d: --- encrypted key data \n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpEncryptData);

    // decrypt
    gcryptError = gcry_pk_decrypt(&sexpData,sexpEncryptData,privateKey->key);
    if (gcryptError != 0)
    {
      gcry_sexp_release(sexpEncryptData);
      return ERROR_KEY_DECRYPT_FAIL;
    }
//fprintf(stderr,"%s,%d: --- key data \n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpData);

    // get decrypted data
    pkcs1EncodedMessage = Password_allocSecure(PKCS1_ENCODED_MESSAGE_LENGTH);
    if (pkcs1EncodedMessage == NULL)
    {
      return ERROR_KEY_ENCRYPT_FAIL;
    }
    data = (const byte*)gcry_sexp_nth_data(sexpData,0,&dataLength);
    if (data == NULL)
    {
      gcry_sexp_release(sexpData);
      gcry_sexp_release(sexpEncryptData);
      return ERROR_KEY_DECRYPT_FAIL;
    }

    // check if key length is valid
    if (dataLength > PKCS1_ENCODED_MESSAGE_LENGTH)
    {
      gcry_sexp_release(sexpData);
      gcry_sexp_release(sexpEncryptData);
      return ERROR_WRONG_PRIVATE_KEY;
    }

    // MPI does not store leading 0 -> do padding with 0 for required length
    memmove(&pkcs1EncodedMessage[PKCS1_ENCODED_MESSAGE_LENGTH-dataLength],&data[0],dataLength);
    memset(&pkcs1EncodedMessage[0],0,PKCS1_ENCODED_MESSAGE_LENGTH-dataLength);

    // decode message block, get key
    if (   (pkcs1EncodedMessage[0] != 0x00)
        || (pkcs1EncodedMessage[1] != 0x02)
        || (pkcs1EncodedMessage[1+1+PKCS1_ENCODED_MESSAGE_PADDING_LENGTH] != 0x00)
       )
    {
      gcry_sexp_release(sexpData);
      gcry_sexp_release(sexpEncryptData);
      return ERROR_WRONG_PRIVATE_KEY;
    }
    Password_setBuffer(password,&pkcs1EncodedMessage[1+1+PKCS1_ENCODED_MESSAGE_PADDING_LENGTH+1],(PKCS1_RANDOM_KEY_LENGTH+7)/8);

    // free resources
    Password_freeSecure(pkcs1EncodedMessage);
    gcry_sexp_release(sexpData);
    gcry_sexp_release(sexpEncryptData);

    return ERROR_NONE;
  #else /* not HAVE_GCRYPT */
    UNUSED_VARIABLE(privateKey);
    UNUSED_VARIABLE(encryptData);
    UNUSED_VARIABLE(encryptDataLength);
    UNUSED_VARIABLE(password);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GCRYPT */
}

Errors Crypt_getSignature(CryptKey *privateKey,
                          void     *buffer,
                          uint     bufferLength,
                          void     *signature,
                          uint     maxSignatureLength,
                          uint     *signatureLength
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

  assert(privateKey != NULL);
  assert(buffer != NULL);
  assert(signature != NULL);
  assert(signatureLength != NULL);

  #ifdef HAVE_GCRYPT
    // check if private key is available
    if (!Crypt_isKeyAvailable(privateKey))
    {
//TODO: filename
      return ERRORX_(NO_PRIVATE_SIGNATURE_KEY,0,"signature-key");
    }
    sexpToken = gcry_sexp_find_token(privateKey->key,"private-key",0);
    if (sexpToken == NULL)
    {
//TOOD: filename
      return ERRORX_(NOT_A_PRIVATE_KEY,0,"signature-key");
    }
    gcry_sexp_release(sexpToken);

    // create S-expression with data
    gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags raw) (value %b))",bufferLength,(char*)buffer);
//TODO: does not work?
//    gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags raw) (hash sha512 %b))",bufferLength,(char*)buffer);
    if (gcryptError != 0)
    {
      return ERRORX_(KEY_ENCRYPT_FAIL,gcryptError,"%s",gcry_strerror(gcryptError));
    }
//fprintf(stderr,"%s, %d: data \n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpData);

    // sign
    gcryptError = gcry_pk_sign(&sexpSignatureData,sexpData,privateKey->key);
    if (gcryptError != 0)
    {
      gcry_sexp_release(sexpData);
      return ERRORX_(KEY_ENCRYPT_FAIL,gcryptError,"%s",gcry_strerror(gcryptError));
    }
//fprintf(stderr,"%s, %d: signature data\n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpSignatureData);

    // get signature data
    sexpToken = gcry_sexp_find_token(sexpSignatureData,"s",0);
    if (sexpToken == NULL)
    {
      gcry_sexp_release(sexpSignatureData);
      gcry_sexp_release(sexpData);
      return ERRORX_(KEY_ENCRYPT_FAIL,gcryptError,"%s",gcry_strerror(gcryptError));
    }
//fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpToken);
    signatureData = gcry_sexp_nth_data(sexpToken,1,&signatureDataLength);
    if (signatureData == NULL)
    {
      gcry_sexp_release(sexpSignatureData);
      gcry_sexp_release(sexpData);
      return ERRORX_(KEY_ENCRYPT_FAIL,gcryptError,"%s",gcry_strerror(gcryptError));
    }
    (*signatureLength) = MIN(signatureDataLength,maxSignatureLength);
    memcpy(signature,signatureData,(*signatureLength));
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

Errors Crypt_verifySignature(CryptKey             *publicKey,
                             const void           *buffer,
                             uint                 bufferLength,
                             const void           *signature,
                             uint                 signatureLength,
                             CryptSignatureStates *cryptSignatureState
                            )
{
  #ifdef HAVE_GCRYPT
    gcry_sexp_t  sexpToken;
    gcry_sexp_t  sexpData;
    gcry_sexp_t  sexpSignatureData;
    gcry_error_t gcryptError;
  #endif /* HAVE_GCRYPT */

  assert(publicKey != NULL);
  assert(buffer != NULL);
  assert(signature != NULL);

  if (cryptSignatureState != NULL) (*cryptSignatureState) = CRYPT_SIGNATURE_STATE_NONE;

  #ifdef HAVE_GCRYPT
    // check if public key is available
    if (!Crypt_isKeyAvailable(publicKey))
    {
//TODO: filename
      return ERRORX_(NO_PUBLIC_SIGNATURE_KEY,0,"signature-key");
    }
    sexpToken = gcry_sexp_find_token(publicKey->key,"public-key",0);
    if (sexpToken == NULL)
    {
//TODO: filename
      return ERRORX_(NOT_A_PUBLIC_KEY,0,"signature-key");
    }
    gcry_sexp_release(sexpToken);

    // create S-expression with signature
    gcryptError = gcry_sexp_build(&sexpSignatureData,NULL,"(sig-val (rsa (s %b)))",signatureLength,(char*)signature);
    if (gcryptError != 0)
    {
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
//gcry_sexp_dump(cryptKey->key);

  #ifdef HAVE_GCRYPT
    sexpToken = gcry_sexp_find_token(cryptKey->key,"public-key",0);
    if (sexpToken != NULL)
    {
      printf("Public key:\n");

      rsaToken = gcry_sexp_find_token(sexpToken,"rsa",0);
      nToken   = gcry_sexp_find_token(rsaToken,"n",0);
      eToken   = gcry_sexp_find_token(rsaToken,"e",0);
//fprintf(stderr,"%s, %d: rsa\n",__FILE__,__LINE__); gcry_sexp_dump(rsaToken);
//fprintf(stderr,"%s, %d: nToken\n",__FILE__,__LINE__); gcry_sexp_dump(nToken);
//fprintf(stderr,"%s, %d: eToken\n",__FILE__,__LINE__); gcry_sexp_dump(eToken);

      n = gcry_sexp_nth_mpi(nToken,1,GCRYMPI_FMT_USG);
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,n);
      printf("  n=%s\n",s);
      free(s);

      e = gcry_sexp_nth_mpi(eToken,1,GCRYMPI_FMT_USG);
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,e);
      printf("  e=%s\n",s);
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
      printf("Private key:\n");

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
      printf("  n=%s\n",s);
      free(s);
      e = gcry_sexp_nth_mpi(eToken,1,GCRYMPI_FMT_USG);
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,e);
      printf("  e=%s\n",s);
      free(s);
      d = gcry_sexp_nth_mpi(dToken,1,GCRYMPI_FMT_USG);
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,d);
      printf("  d=%s\n",s);
      free(s);
      p = gcry_sexp_nth_mpi(pToken,1,GCRYMPI_FMT_USG);
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,p);
      printf("  p=%s\n",s);
      free(s);
      q = gcry_sexp_nth_mpi(qToken,1,GCRYMPI_FMT_USG);
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,q);
      printf("  q=%s\n",s);
      free(s);
      u = gcry_sexp_nth_mpi(uToken,1,GCRYMPI_FMT_USG);
      gcry_mpi_aprint(GCRYMPI_FMT_HEX,&s,NULL,u);
      printf("  u=%s\n",s);
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
#endif /* NDEBUG */

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
    case CRYPT_HASH_ALGORITHM_SHA2_224:
    case CRYPT_HASH_ALGORITHM_SHA2_256:
    case CRYPT_HASH_ALGORITHM_SHA2_384:
    case CRYPT_HASH_ALGORITHM_SHA2_512:
      #ifdef HAVE_GCRYPT
        {
          int          hashAlgorithm;

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
            return ERROR_INIT_HASH;
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
    DEBUG_ADD_RESOURCE_TRACE(cryptHash,sizeof(CryptHash));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptHash,sizeof(CryptHash));
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
    DEBUG_REMOVE_RESOURCE_TRACE(cryptHash,sizeof(CryptHash));
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptHash,sizeof(CryptHash));
  #endif /* NDEBUG */

  #ifdef HAVE_GCRYPT
    gcry_md_close(cryptHash->gcry_md_hd);
  #endif /* HAVE_GCRYPT */
}

void Crypt_resetHash(CryptHash *cryptHash)
{
  assert(cryptHash != NULL);

  #ifdef HAVE_GCRYPT
    gcry_md_reset(cryptHash->gcry_md_hd);
  #endif /* HAVE_GCRYPT */
}

void Crypt_updateHash(CryptHash *cryptHash,
                      void      *buffer,
                      ulong     bufferLength
                     )
{
  assert(cryptHash != NULL);

  #ifdef HAVE_GCRYPT
    gcry_md_write(cryptHash->gcry_md_hd,buffer,bufferLength);
  #endif /* HAVE_GCRYPT */
}

uint Crypt_getHashLength(const CryptHash *cryptHash)
{
  uint hashLength;

  assert(cryptHash != NULL);

  hashLength = 0;
  switch (cryptHash->cryptHashAlgorithm)
  {
    case CRYPT_HASH_ALGORITHM_SHA2_224:
    case CRYPT_HASH_ALGORITHM_SHA2_256:
    case CRYPT_HASH_ALGORITHM_SHA2_384:
    case CRYPT_HASH_ALGORITHM_SHA2_512:
      #ifdef HAVE_GCRYPT
        {
          int hashAlgorithm;

          hashAlgorithm = GCRY_MD_NONE;
          switch (cryptHash->cryptHashAlgorithm)
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

          hashLength = gcry_md_get_algo_dlen(hashAlgorithm);
        }
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
  assert(cryptHash != NULL);
  assert(buffer != NULL);

  switch (cryptHash->cryptHashAlgorithm)
  {
    case CRYPT_HASH_ALGORITHM_SHA2_224:
    case CRYPT_HASH_ALGORITHM_SHA2_256:
    case CRYPT_HASH_ALGORITHM_SHA2_384:
    case CRYPT_HASH_ALGORITHM_SHA2_512:
      #ifdef HAVE_GCRYPT
        {
          int  hashAlgorithm;
          uint n;

          hashAlgorithm = GCRY_MD_NONE;
          switch (cryptHash->cryptHashAlgorithm)
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

          n = gcry_md_get_algo_dlen(hashAlgorithm);
          if (n > bufferSize)
          {
            return NULL;
          }

          if (hashLength != NULL) (*hashLength) = n;
          memcpy(buffer,gcry_md_read(cryptHash->gcry_md_hd,hashAlgorithm),n);
        }
      #else /* not HAVE_GCRYPT */
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

bool Crypt_equalsHash(const CryptHash *cryptHash,
                      const void      *hash,
                      uint            hashLength
                     )
{
  bool equalsFlag;

  assert(cryptHash != NULL);

  equalsFlag = FALSE;
  switch (cryptHash->cryptHashAlgorithm)
  {
    case CRYPT_HASH_ALGORITHM_SHA2_224:
    case CRYPT_HASH_ALGORITHM_SHA2_256:
    case CRYPT_HASH_ALGORITHM_SHA2_384:
    case CRYPT_HASH_ALGORITHM_SHA2_512:
      #ifdef HAVE_GCRYPT
        {
          int hashAlgorithm;

          hashAlgorithm = GCRY_MD_NONE;
          switch (cryptHash->cryptHashAlgorithm)
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

          equalsFlag =    (gcry_md_get_algo_dlen(hashAlgorithm) == hashLength)
                       && (memcmp(hash,gcry_md_read(cryptHash->gcry_md_hd,hashAlgorithm),hashLength) == 0);
        }
      #else /* not HAVE_GCRYPT */
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
            return ERROR_INIT_MAC;
          }

          gcryptError = gcry_mac_setkey(cryptMAC->gcry_mac_hd,keyData,keyDataLength);
          if (gcryptError != 0)
          {
            gcry_mac_close(cryptMAC->gcry_mac_hd);
            return ERROR_INIT_MAC;
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
    DEBUG_ADD_RESOURCE_TRACE(cryptMAC,sizeof(CryptMAC));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptMAC,sizeof(CryptMAC));
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
    DEBUG_REMOVE_RESOURCE_TRACE(cryptMAC,sizeof(CryptMAC));
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptMAC,sizeof(CryptMAC));
  #endif /* NDEBUG */

  #ifdef HAVE_GCRYPT
    gcry_mac_close(cryptMAC->gcry_mac_hd);
  #endif /* HAVE_GCRYPT */
}

void Crypt_resetMAC(CryptMAC *cryptMAC)
{
  assert(cryptMAC != NULL);

  #ifdef HAVE_GCRYPT
    gcry_mac_reset(cryptMAC->gcry_mac_hd);
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

#ifdef __cplusplus
  }
#endif

/* end of file */
