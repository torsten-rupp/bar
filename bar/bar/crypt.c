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
  #include <gcrypt.h>
#endif /* HAVE_GCRYPT */
#include <zlib.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "lists.h"

#include "bar.h"
#include "archive_format.h"
#include "chunks.h"
#include "files.h"
#include "passwords.h"

#include "crypt.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define MAX_KEY_SIZE 2048               // max. size of a key in bits

#define BLOCK_LENGTH_CRYPT_NONE 4       // block size if no encryption

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

#ifdef HAVE_GCRYPT
/***********************************************************************\
* Name   : base64Encode
* Purpose: encode base64
* Input  : s      - string variable
*          data   - data to encode
*          length - length of data to encode
* Output : -
* Return : encoded string
* Notes  : -
\***********************************************************************/

LOCAL String base64Encode(String s, const byte *data, uint length)
{
  const char BASE64_ENCODING_TABLE[] =
  {
    'A','B','C','D','E','F','G','H',
    'I','J','K','L','M','N','O','P',
    'Q','R','S','T','U','V','W','X',
    'Y','Z','a','b','c','d','e','f',
    'g','h','i','j','k','l','m','n',
    'o','p','q','r','s','t','u','v',
    'w','x','y','z','0','1','2','3',
    '4','5','6','7','8','9','+','/'
  };

  uint z;
  char b0,b1,b2;
  uint i0,i1,i2,i3;

  z = 0;
  while (z < length)
  {
    b0 = ((z+0) < length)?data[z+0]:0;
    b1 = ((z+1) < length)?data[z+1]:0;
    b2 = ((z+2) < length)?data[z+2]:0;

    i0 = (uint)(b0 & 0xFC) >> 2;
    assert(i0 < 64);
    i1 = (uint)((b0 & 0x03) << 4) | (uint)((b1 & 0xF0) >> 4);
    assert(i1 < 64);
    i2 = (uint)((b1 & 0x0F) << 2) | (uint)((b2 & 0xC0) >> 6);
    assert(i2 < 64);
    i3 = (uint)(b2 & 0x3F);
    assert(i3 < 64);

    String_appendChar(s,BASE64_ENCODING_TABLE[i0]);
    String_appendChar(s,BASE64_ENCODING_TABLE[i1]);
    String_appendChar(s,BASE64_ENCODING_TABLE[i2]);
    String_appendChar(s,BASE64_ENCODING_TABLE[i3]);

    z += 3;
  }

  return s;
}
#endif /* HAVE_GCRYPT */

#ifdef HAVE_GCRYPT
/***********************************************************************\
* Name   : base64Decode
* Purpose: decode base64
* Input  : data      - data variable
*          maxLength - max. length of data
*          s         - base64 string
* Output : -
* Return : length of decoded data or -1 on error
* Notes  : -
\***********************************************************************/

LOCAL int base64Decode(byte *data, uint maxLength, const String s)
{
  #define VALID_BASE64_CHAR(ch) (   (((ch) >= 'A') && ((ch) <= 'Z')) \
                                 || (((ch) >= 'a') && ((ch) <= 'z')) \
                                 || (((ch) >= '0') && ((ch) <= '9')) \
                                 || ((ch) == '+') \
                                 || ((ch) == '/') \
                                 || ((ch) == '=') \
                                )

  const byte BASE64_DECODING_TABLE[] =
  {
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,62,0,0,0,63,
    52,53,54,55,56,57,58,59,
    60,61,0,0,0,0,0,0,
    0,0,1,2,3,4,5,6,
    7,8,9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,
    23,24,25,0,0,0,0,0,
    0,26,27,28,29,30,31,32,
    33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,
    49,50,51,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
  };

  uint length;
  uint z;
  char x0,x1,x2,x3;
  uint i0,i1,i2,i3;
  char b0,b1,b2;

  length = 0;
  z = 0;
  while (z < (uint)String_length(s))
  {
    x0 = String_index(s,z+0); if (!VALID_BASE64_CHAR(x0)) return -1;
    x1 = String_index(s,z+1); if (!VALID_BASE64_CHAR(x1)) return -1;
    x2 = String_index(s,z+2); if (!VALID_BASE64_CHAR(x2)) return -1;
    x3 = String_index(s,z+3); if (!VALID_BASE64_CHAR(x3)) return -1;

    i0 = ((z+0) < (uint)String_length(s))?BASE64_DECODING_TABLE[(byte)x0]:0;
    i1 = ((z+1) < (uint)String_length(s))?BASE64_DECODING_TABLE[(byte)x1]:0;
    i2 = ((z+2) < (uint)String_length(s))?BASE64_DECODING_TABLE[(byte)x2]:0;
    i3 = ((z+3) < (uint)String_length(s))?BASE64_DECODING_TABLE[(byte)x3]:0;

    b0 = (char)((i0 << 2) | ((i1 & 0x30) >> 4));
    b1 = (char)(((i1 & 0x0F) << 4) | ((i2 & 0x3C) >> 2));
    b2 = (char)(((i2 & 0x03) << 6) | i3);

    if (length < maxLength) { data[length] = b0; length++; }
    if (length < maxLength) { data[length] = b1; length++; }
    if (length < maxLength) { data[length] = b2; length++; }

    z += 4;
  }

  return length;

  #undef VALID_BASE64_CHAR
}
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
      printError("Wrong gcrypt version (needed: %d)\n",GCRYPT_VERSION);
      return ERROR_INIT_CRYPT;
    }

    gcry_control(GCRYCTL_DISABLE_SECMEM,         0);
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED,0);
    gcry_control(GCRYCTL_ENABLE_QUICK_RANDOM,    0);
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

const char *Crypt_algorithmToString(CryptAlgorithms cryptAlgorithm, const char *defaultValue)
{
  uint       z;
  const char *name;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(CRYPT_ALGORITHMS))
         && (CRYPT_ALGORITHMS[z].cryptAlgorithm != cryptAlgorithm)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(CRYPT_ALGORITHMS))
  {
    name = CRYPT_ALGORITHMS[z].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Crypt_parseAlgorithm(const char *name, CryptAlgorithms *cryptAlgorithm)
{
  uint z;

  assert(name != NULL);
  assert(cryptAlgorithm != NULL);

  z = 0;
  while (   (z < SIZE_OF_ARRAY(CRYPT_ALGORITHMS))
         && !stringEqualsIgnoreCase(CRYPT_ALGORITHMS[z].name,name)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(CRYPT_ALGORITHMS))
  {
    (*cryptAlgorithm) = CRYPT_ALGORITHMS[z].cryptAlgorithm;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

const char *Crypt_typeToString(CryptTypes cryptType, const char *defaultValue)
{
  const char *typeName;

  switch (cryptType)
  {
    case CRYPT_TYPE_NONE      : typeName = "NONE";       break;
    case CRYPT_TYPE_SYMMETRIC : typeName = "SYMMETRIC";  break;
    case CRYPT_TYPE_ASYMMETRIC: typeName = "ASYMMETRIC"; break;
    default:
      typeName = defaultValue;
      break;
  }

  return typeName;
}

void Crypt_randomize(byte *buffer, uint length)
{
  assert(buffer != NULL);

  #ifdef HAVE_GCRYPT
    gcry_randomize((unsigned char*)buffer,length,GCRY_STRONG_RANDOM);
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
            printError("Cannot detect block length of '%s' cipher (error: %s)\n",
                       gcry_cipher_algo_name(gcryptAlgorithm),
                       gpg_strerror(gcryptError)
                      );
            return ERROR_INIT_CIPHER;
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
            printError("Cannot detect block length of '%s' cipher (error: %s)\n",
                       gcry_cipher_algo_name(gcryptAlgorithm),
                       gpg_strerror(gcryptError)
                      );
            return ERROR_INIT_CIPHER;
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

bool Crypt_isSymmetricSupported(void)
{
  #ifdef HAVE_GCRYPT
    return TRUE;
  #else /* not HAVE_GCRYPT */
    return FALSE;
  #endif /* HAVE_GCRYPT */
}

#ifdef NDEBUG
Errors Crypt_init(CryptInfo       *cryptInfo,
                  CryptAlgorithms cryptAlgorithm,
                  const Password  *password
                 )
#else /* not NDEBUG */
Errors __Crypt_init(const char    *__fileName__,
                    ulong         __lineNb__,
                    CryptInfo       *cryptInfo,
                    CryptAlgorithms cryptAlgorithm,
                    const Password  *password
                   )
#endif /* NDEBUG */
{
  assert(cryptInfo != NULL);

  // init variables
  cryptInfo->cryptAlgorithm = cryptAlgorithm;

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
          gcry_error_t gcryptError;
          size_t       n;
          uint         maxKeyLength;
          uint         keyLength;
          char         key[MAX_KEY_SIZE/8];
          uint         z;

          if (password == NULL)
          {
            return ERROR_NO_CRYPT_PASSWORD;
          }
          passwordLength = Password_length(password);
          if (passwordLength <= 0)
          {
            return ERROR_NO_CRYPT_PASSWORD;
          }

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

          // get max. key length, block length
          gcryptError = gcry_cipher_algo_info(gcryptAlgorithm,
                                              GCRYCTL_GET_KEYLEN,
                                              NULL,
                                              &n
                                             );
          if (gcryptError != 0)
          {
            printError("Cannot detect max. key length of cipher '%s' (error: %s)\n",
                       gcry_cipher_algo_name(gcryptAlgorithm),
                       gpg_strerror(gcryptError)
                      );
            return ERROR_INIT_CIPHER;
          }
          maxKeyLength = n*8;
          gcryptError = gcry_cipher_algo_info(gcryptAlgorithm,
                                              GCRYCTL_GET_BLKLEN,
                                              NULL,
                                              &n
                                             );
          if (gcryptError != 0)
          {
            printError("Cannot detect block length of cipher '%s' (error: %s)\n",
                       gcry_cipher_algo_name(gcryptAlgorithm),
                       gpg_strerror(gcryptError)
                      );
            return ERROR_INIT_CIPHER;
          }
          cryptInfo->blockLength = n;

          // get key length
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
            printError("Cipher '%s' does not support %dbit keys\n",
                       gcry_cipher_algo_name(gcryptAlgorithm),
                       keyLength
                      );
            return ERROR_INIT_CIPHER;
          }

          // init cipher
          gcryptError = gcry_cipher_open(&cryptInfo->gcry_cipher_hd,
                                         gcryptAlgorithm,
                                         GCRY_CIPHER_MODE_CBC,
                                         GCRY_CIPHER_CBC_CTS
                                        );
          if (gcryptError != 0)
          {
            printError("Init cipher '%s' failed (error: %s)\n",
                       gcry_cipher_algo_name(gcryptAlgorithm),
                       gpg_strerror(gcryptError)
                      );
            return ERROR_INIT_CIPHER;
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
            printError("Cannot set key for cipher '%s' with %dbit (error: %s)\n",
                       gcry_cipher_algo_name(gcryptAlgorithm),
                       keyLength,
                       gpg_strerror(gcryptError)
                      );
            gcry_cipher_close(cryptInfo->gcry_cipher_hd);
            return ERROR_INIT_CIPHER;
          }


#if 0
//NYI: useful to set iv?
          // set 0 IV
          gcryptError = gcry_cipher_setiv(cryptInfo->gcry_cipher_hd,
                                          NULL,
                                          0
                                         );
          if (gcryptError != 0)
          {
            printError("Cannot set cipher IV (error: %s)\n",gpg_strerror(gcryptError));
            gcry_cipher_close(cryptInfo->gcry_cipher_hd);
            return ERROR_INIT_CIPHER;
          }

          gcry_cipher_reset(cryptInfo->gcry_cipher_hd);
#endif /* 0 */
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
    DEBUG_ADD_RESOURCE_TRACE("crypt",cryptInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,"crypt",cryptInfo);
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
    DEBUG_REMOVE_RESOURCE_TRACE(cryptInfo);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,cryptInfo);
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
          uint         ivLength;
          char         iv[MAX_KEY_SIZE/8];
          uint         z;
          gcry_error_t gcryptError;

          gcry_cipher_reset(cryptInfo->gcry_cipher_hd);

          if (seed != 0)
          {
            // get IV length
            ivLength = 0;
            switch (cryptInfo->cryptAlgorithm)
            {
              case CRYPT_ALGORITHM_3DES:        ivLength = 192; break;
              case CRYPT_ALGORITHM_CAST5:       ivLength = 128; break;
              case CRYPT_ALGORITHM_BLOWFISH:    ivLength = 128; break;
              case CRYPT_ALGORITHM_AES128:      ivLength = 128; break;
              case CRYPT_ALGORITHM_AES192:      ivLength = 192; break;
              case CRYPT_ALGORITHM_AES256:      ivLength = 256; break;
              case CRYPT_ALGORITHM_TWOFISH128:  ivLength = 128; break;
              case CRYPT_ALGORITHM_TWOFISH256:  ivLength = 256; break;
              case CRYPT_ALGORITHM_SERPENT128:  ivLength = 128; break;
              case CRYPT_ALGORITHM_SERPENT192:  ivLength = 192; break;
              case CRYPT_ALGORITHM_SERPENT256:  ivLength = 256; break;
              case CRYPT_ALGORITHM_CAMELLIA128: ivLength = 128; break;
              case CRYPT_ALGORITHM_CAMELLIA192: ivLength = 192; break;
              case CRYPT_ALGORITHM_CAMELLIA256: ivLength = 256; break;
              default:
                #ifndef NDEBUG
                  HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                #endif /* NDEBUG */
                break; /* not reached */
            }

            // set IV
            assert(sizeof(iv) >= (ivLength+7)/8);
            for (z = 0; z < (ivLength+7)/8; z++)
            {
              iv[z] = (seed >> (z%8)*8) & 0xFF;
            }
            gcryptError = gcry_cipher_setiv(cryptInfo->gcry_cipher_hd,
                                            iv,
                                            (ivLength+7)/8
                                           );
            if (gcryptError != 0)
            {
              printError("Cannot set cipher IV (error: %s)\n",
                         gpg_strerror(gcryptError)
                        );
              return ERROR_INIT_CIPHER;
            }
          }
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

        gcryptError = gcry_cipher_cts(cryptInfo->gcry_cipher_hd,TRUE);
        if (gcryptError != 0)
        {
          return ERROR_ENCRYPT_FAIL;
        }

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

        gcryptError = gcry_cipher_cts(cryptInfo->gcry_cipher_hd,TRUE);
        if (gcryptError != 0)
        {
          return ERROR_ENCRYPT_FAIL;
        }

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

        gcryptError = gcry_cipher_cts(cryptInfo->gcry_cipher_hd,FALSE);
        if (gcryptError != 0)
        {
          return ERROR_ENCRYPT_FAIL;
        }

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
                          ulong      bufferLength
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

        gcryptError = gcry_cipher_cts(cryptInfo->gcry_cipher_hd,FALSE);
        if (gcryptError != 0)
        {
          return ERROR_ENCRYPT_FAIL;
        }

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
                        String         string,
                        const Password *password
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
  assert(string != NULL);

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
{
int z;
byte *p=fileCryptKey->data;
printf("raw data: "); for (z=0;z<dataLength;z++,p++) printf("%02x",*p); printf("\n");
p++;
}
#endif /* 0 */

    // encrypt key
    if (password != NULL)
    {
      // initialize crypt
      error = Crypt_init(&cryptInfo,SECRET_KEY_CRYPT_ALGORITHM,password);
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
{
int z;
byte *p=fileCryptKey->data;
printf("cryp@t data: "); for (z=0;z<dataLength;z++,p++) printf("%02x",*p); printf("\n");
p++;
}
#endif /* 0 */

    // calculate CRC
    fileCryptKey->crc = htonl(crc32(crc32(0,Z_NULL,0),fileCryptKey->data,dataLength));

    // encode base64
    String_clear(string);
    base64Encode(string,(byte*)fileCryptKey,fileCryptKeyLength);
  //fprintf(stderr,"%s,%d: %d %s\n",__FILE__,__LINE__,String_length(string),String_cString(string));

    // free resources
    Password_freeSecure(fileCryptKey);
    gcry_sexp_release(sexpToken);

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
    gcry_sexp_t sexpToken;
    gcry_sexp_t rsaToken;
    gcry_sexp_t nToken;
    gcry_mpi_t  n;
    char        *s;
    String      string;
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
    gcry_mpi_aprint(GCRYMPI_FMT_HEX,(unsigned char**)&s,NULL,n);
    string = String_newCString(s);
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
    gcry_sexp_t sexpToken;
    gcry_sexp_t rsaToken;
    gcry_sexp_t eToken;
    gcry_mpi_t  e;
    char        *s;
    String      string;
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
    gcry_mpi_aprint(GCRYMPI_FMT_HEX,(unsigned char**)&s,NULL,e);
    string = String_newCString(s);
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

Errors Crypt_setKeyData(CryptKey       *cryptKey,
                        const String   string,
                        const Password *password
                       )
{
  #ifdef HAVE_GCRYPT
    uint         blockLength;
    uint         fileCryptKeyLength;
    FileCryptKey *fileCryptKey;
    char         *data;
    size_t       dataLength;
    uint32       crc;
    CryptInfo    cryptInfo;
    Errors       error;
    gcry_error_t gcryptError;
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

    // allocate file key
    fileCryptKeyLength = String_length(string);
    fileCryptKey = (FileCryptKey*)Password_allocSecure(fileCryptKeyLength);
    if (fileCryptKey == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    memset(fileCryptKey,0,fileCryptKeyLength);
    data = (char*)fileCryptKey->data;

    // decode base64
    if (base64Decode((byte*)fileCryptKey,fileCryptKeyLength,string) == -1)
    {
      return ERROR_INVALID_KEY;
    }

    // get data length
    dataLength = ntohs(fileCryptKey->dataLength);

    // check key CRC
    crc = crc32(crc32(0,Z_NULL,0),fileCryptKey->data,dataLength);
    if (crc != ntohl(fileCryptKey->crc))
    {
      return ERROR_INVALID_KEY;
    }
#if 0
{
int z;
byte *p=data;
printf("cry data: "); for (z=0;z<dataLength;z++,p++) printf("%02x",*p); printf("\n");
p++;
}
#endif /* 0 */

    // decrypt key
    if (password != NULL)
    {
      // initialize crypt
      error = Crypt_init(&cryptInfo,SECRET_KEY_CRYPT_ALGORITHM,password);
      if (error != ERROR_NONE)
      {
        Password_freeSecure(fileCryptKey);
        return error;
      }

      // decrypt
      error = Crypt_decrypt(&cryptInfo,data,ALIGN(dataLength,blockLength));
      if (error != ERROR_NONE)
      {
        Crypt_done(&cryptInfo);
        Password_freeSecure(fileCryptKey);
        return error;
      }

      // done crypt
      Crypt_done(&cryptInfo);
    }
#if 0
{
int z;
byte *p=data;
printf("decryp data: "); for (z=0;z<dataLength;z++,p++) printf("%02x",*p); printf("\n");
p++;
}
#endif /* 0 */

    // create S-expression with key
    if (cryptKey->key != NULL)
    {
      gcry_sexp_release(cryptKey->key);
      cryptKey->key = NULL;
    }
    gcryptError = gcry_sexp_new(&cryptKey->key,data,dataLength,1);
    if (gcryptError != 0)
    {
      free(fileCryptKey);
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

Errors Crypt_readKeyFile(CryptKey       *cryptKey,
                         const String   fileName,
                         const Password *password
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
    return ERRORX_(KEY_NOT_FOUND,0,String_cString(fileName));
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
  error = Crypt_setKeyData(cryptKey,data,password);
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
                          const Password *password
                         )
{
  String     data;
  Errors     error;
  FileHandle fileHandle;

  assert(cryptKey != NULL);
  assert(fileName != NULL);

  // get key data
  data = String_new();
  error = Crypt_getKeyData(cryptKey,data,password);
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

Errors Crypt_createKeys(CryptKey          *publicCryptKey,
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
      return ERRORX_(CREATE_KEY_FAIL,gcryptError,gpg_strerror(gcryptError));
    }

    // generate keys
    gcryptError = gcry_pk_genkey(&sexpKey,sexpKeyParameters);
    if (gcryptError != 0)
    {
      gcry_sexp_release(sexpKeyParameters);
      String_delete(description);
      Crypt_doneKey(privateCryptKey);
      Crypt_doneKey(publicCryptKey);
      return ERRORX_(CREATE_KEY_FAIL,gcryptError,gpg_strerror(gcryptError));
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

    // create S-expression with data
    switch (cryptKey->cryptPaddingType)
    {
      case CRYPT_PADDING_TYPE_NONE:
        gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (value %b))",bufferLength,buffer);
        break;
      case CRYPT_PADDING_TYPE_PKCS1:
        gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags pkcs1) (value %b))",bufferLength,buffer);
        break;
      case CRYPT_PADDING_TYPE_OAEP:
        gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags oaep) (value %b))",bufferLength,buffer);
        break;
      default:
        return ERROR_KEY_ENCRYPT_FAIL;
        break;
    }
    if (gcryptError != 0)
    {
      error = ERRORX_(KEY_ENCRYPT_FAIL,gcryptError,gcry_strerror(gcryptError));
//      gcry_mpi_release(n);
      return error;
    }
//gcry_sexp_dump(sexpData);

    // encrypt
    gcryptError = gcry_pk_encrypt(&sexpEncryptData,sexpData,cryptKey->key);
    if (gcryptError != 0)
    {
      error = ERRORX_(KEY_ENCRYPT_FAIL,gcryptError,gcry_strerror(gcryptError));
      gcry_sexp_release(sexpData);
//      gcry_mpi_release(n);
      return error;
    }
//gcry_sexp_dump(sexpEncryptData);

    // get encrypted data
    sexpToken = gcry_sexp_find_token(sexpEncryptData,"a",0);
    if (sexpToken == NULL)
    {
      error = ERRORX_(KEY_ENCRYPT_FAIL,gcryptError,gcry_strerror(gcryptError));
      gcry_sexp_release(sexpEncryptData);
      gcry_sexp_release(sexpData);
//      gcry_mpi_release(n);
      return error;
    }
//gcry_sexp_dump(sexpToken);
    encryptData = gcry_sexp_nth_data(sexpToken,1,&encryptDataLength);
    if (encryptData == NULL)
    {
      error = ERRORX_(KEY_ENCRYPT_FAIL,gcryptError,gcry_strerror(gcryptError));
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
    //    gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (flags raw) (rsa (a %b)))",encryptBufferLength,encryptBuffer);
          break;
        case CRYPT_PADDING_TYPE_PKCS1:
          gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (flags pkcs1) (rsa (a %b)))",encryptBufferLength,encryptBuffer);
          break;
        case CRYPT_PADDING_TYPE_OAEP:
          gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (flags oaep) (rsa (a %b)))",encryptBufferLength,encryptBuffer);
    //    gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (flags pss) (rsa (a %b)))",encryptBufferLength,encryptBuffer);
          break;
        default:
          return ERROR_KEY_ENCRYPT_FAIL;
          break;
      }
      if (gcryptError != 0)
      {
        return ERRORX_(KEY_ENCRYPT_FAIL,gcryptError,gcry_strerror(gcryptError));
      }
    //fprintf(stderr,"%s, %d: encrypted data\n",__FILE__,__LINE__); gcry_sexp_dump(sexpEncryptData);

      // decrypt
      gcryptError = gcry_pk_decrypt(&sexpData,sexpEncryptData,cryptKey->key);
      if (gcryptError != 0)
      {
        error = ERRORX_(KEY_ENCRYPT_FAIL,gcryptError,gcry_strerror(gcryptError));
        gcry_sexp_release(sexpEncryptData);
        return error;
      }
    //fprintf(stderr,"%s, %d: plain data\n",__FILE__,__LINE__); gcry_sexp_dump(sexpData);

      // get decrypted data
      data = gcry_sexp_nth_data(sexpData,1,&dataLength);
      if (data == NULL)
      {
        error = ERRORX_(KEY_ENCRYPT_FAIL,gcryptError,gcry_strerror(gcryptError));
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

Errors Crypt_getRandomEncryptKey(CryptKey        *publicKey,
                                 CryptAlgorithms cryptAlgorithm,
                                 Password        *password,
                                 uint            maxEncryptBufferLength,
                                 void            *encryptBuffer,
                                 uint            *encryptBufferLength
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

  assert(publicKey != NULL);
  assert(password != NULL);
  assert(encryptBuffer != NULL);
  assert(encryptBufferLength != NULL);

  #ifdef HAVE_GCRYPT
//fprintf(stderr,"%s,%d: ---------------------------------\n",__FILE__,__LINE__);
//fprintf(stderr,"%s,%d: public key\n",__FILE__,__LINE__);
//gcry_sexp_dump(publicKey->key);

    // check if public key is available
    sexpToken = gcry_sexp_find_token(publicKey->key,"public-key",0);
    if (sexpToken == NULL)
    {
      return ERROR_NOT_A_PUBLIC_KEY;
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

    // create random password
    Password_random(password,(PKCS1_RANDOM_KEY_LENGTH+7)/8);

    // create padded encoded message block: format 0x00 0x02 PS 0x00 key; size 512bit
    pkcs1EncodedMessage = Password_allocSecure(PKCS1_ENCODED_MESSAGE_LENGTH);
    if (pkcs1EncodedMessage == NULL)
    {
      return ERROR_KEY_ENCRYPT_FAIL;
    }
    pkcs1EncodedMessage[0] = 0x00;
    pkcs1EncodedMessage[1] = 0x02;
    gcry_randomize((unsigned char*)&pkcs1EncodedMessage[1+1],PKCS1_ENCODED_MESSAGE_PADDING_LENGTH,GCRY_STRONG_RANDOM);
    pkcs1EncodedMessage[1+1+PKCS1_ENCODED_MESSAGE_PADDING_LENGTH] = 0x00;
    p = Password_deploy(password);
    memcpy(&pkcs1EncodedMessage[1+1+PKCS1_ENCODED_MESSAGE_PADDING_LENGTH+1],p,(PKCS1_RANDOM_KEY_LENGTH+7)/8);
    Password_undeploy(password);

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
    (*encryptBufferLength) = MIN(encryptDataLength,maxEncryptBufferLength);
    memcpy(encryptBuffer,encryptData,(*encryptBufferLength));
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
    UNUSED_VARIABLE(publicKey);
    UNUSED_VARIABLE(cryptAlgorithm);
    UNUSED_VARIABLE(password);
    UNUSED_VARIABLE(maxEncryptBufferLength);
    UNUSED_VARIABLE(encryptBuffer);
    UNUSED_VARIABLE(encryptBufferLength);

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
    sexpToken = gcry_sexp_find_token(privateKey->key,"private-key",0);
    if (sexpToken == NULL)
    {
      return ERROR_NOT_A_PRIVATE_KEY;
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

#ifndef NDEBUG
void Crypt_dumpKey(const CryptKey *cryptKey)
{
  gcry_sexp_t   sexpToken;
  gcry_sexp_t   rsaToken;
  gcry_sexp_t   nToken,eToken;
  gcry_mpi_t    n,e;
  gcry_sexp_t   dToken,pToken,qToken,uToken;
  gcry_mpi_t    d,p,q,u;
  unsigned char *s;

//gcry_sexp_dump(cryptKey->key);

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
}
#endif /* NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
