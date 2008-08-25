/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/crypt.c,v $
* $Revision: 1.25 $
* $Author: torsten $
* Contents: Backup ARchiver crypt functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#ifdef HAVE_GCRYPT
  #include <gcrypt.h>
#endif /* HAVE_GCRYPT */
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

#define MAX_KEY_SIZE 2048                 // max. size of a key in bits

#define BLOCK_LENGTH_CRYPT_NONE 4       // block size if no encryption

LOCAL const struct { const char *name; CryptAlgorithms cryptAlgorithm; } CRYPT_ALGORITHMS[] =
{
  {"none",      CRYPT_ALGORITHM_NONE       },

  {"3DES",      CRYPT_ALGORITHM_3DES       },
  {"CAST5",     CRYPT_ALGORITHM_CAST5      },
  {"BLOWFISH",  CRYPT_ALGORITHM_BLOWFISH   },
  {"AES128",    CRYPT_ALGORITHM_AES128     },
  {"AES192",    CRYPT_ALGORITHM_AES192     },
  {"AES256",    CRYPT_ALGORITHM_AES256     },
  {"TWOFISH128",CRYPT_ALGORITHM_TWOFISH128 },
  {"TWOFISH256",CRYPT_ALGORITHM_TWOFISH256 },
};

/***************************** Datatypes *******************************/

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

/***********************************************************************\
* Name   : base64Encode
* Purpose: encode base64
* Input  : s      - string variable
*          data   - data to encode
*          length - length of data to encode
* Output : -
* Return : encoded strings
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

/***********************************************************************\
* Name   : base64Decode
* Purpose: decode base64
* Input  : data      - data variable
*          maxLength - max. length of data
*          s         - base64 string
* Output : -
* Return : length of decoded data
* Notes  : -
\***********************************************************************/

LOCAL uint base64Decode(byte *data, uint maxLength, const String s)
{
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
  uint i0,i1,i2,i3;
  char b0,b1,b2;

  length = 0;
  z = 0;
  while (z < String_length(s))
  {
    i0 = ((z+0) < String_length(s))?BASE64_DECODING_TABLE[(byte)String_index(s,z+0)]:0;
    i1 = ((z+1) < String_length(s))?BASE64_DECODING_TABLE[(byte)String_index(s,z+1)]:0;
    i2 = ((z+2) < String_length(s))?BASE64_DECODING_TABLE[(byte)String_index(s,z+2)]:0;
    i3 = ((z+3) < String_length(s))?BASE64_DECODING_TABLE[(byte)String_index(s,z+3)]:0;

    b0 = (char)((i0 << 2) | ((i1 & 0x30) >> 4));
    b1 = (char)(((i1 & 0x0F) << 4) | ((i2 & 0x3C) >> 2));
    b2 = (char)(((i2 & 0x03) << 6) | i3);

    if (length < maxLength) { data[length] = b0; length++; }
    if (length < maxLength) { data[length] = b1; length++; }
    if (length < maxLength) { data[length] = b2; length++; }

    z += 4;
  }

  return length;
}

/*---------------------------------------------------------------------*/

Errors Crypt_initAll(void)
{
  #ifdef HAVE_GCRYPT
    /* enable pthread-support before any other function is called */
    gcry_control(GCRYCTL_SET_THREAD_CBS,&gcry_threads_pthread);

    /* check version and do internal library init */
    if (!gcry_check_version(GCRYPT_VERSION))
    {
      printError("Wrong gcrypt version (needed: %d)\n",GCRYPT_VERSION);
      return ERROR_INIT_CRYPT;
    }

    gcry_control(GCRYCTL_DISABLE_SECMEM,         0);
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED,0);
    gcry_control(GCRYCTL_ENABLE_QUICK_RANDOM,    0);
    #ifndef NDEBUG
      gcry_control(GCRYCTL_SET_DEBUG_FLAGS,1,0);
    #endif
  #endif /* HAVE_GCRYPT */

  return ERROR_NONE;
}

void Crypt_doneAll(void)
{
}

const char *Crypt_getAlgorithmName(CryptAlgorithms cryptAlgorithm)
{
  int        z;
  const char *s;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(CRYPT_ALGORITHMS))
         && (CRYPT_ALGORITHMS[z].cryptAlgorithm != cryptAlgorithm)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(CRYPT_ALGORITHMS))
  {
    s = CRYPT_ALGORITHMS[z].name;
  }
  else
  {
    s = "unknown";
  }

  return s;
}

CryptAlgorithms Crypt_getAlgorithm(const char *name)
{
  int             z;
  CryptAlgorithms cryptAlgorithm;

  assert(name != NULL);

  z = 0;
  while (   (z < SIZE_OF_ARRAY(CRYPT_ALGORITHMS))
         && (strcmp(CRYPT_ALGORITHMS[z].name,name) != 0)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(CRYPT_ALGORITHMS))
  {
    cryptAlgorithm = CRYPT_ALGORITHMS[z].cryptAlgorithm;
  }
  else
  {
    cryptAlgorithm = CRYPT_ALGORITHM_UNKNOWN;
  }

  return cryptAlgorithm;
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
      #ifdef HAVE_GCRYPT
        {
          int          gcryptAlgorithm;
          gcry_error_t gcryptError;
          size_t       n;

          gcryptAlgorithm = 0;
          switch (cryptAlgorithm)
          {
            case CRYPT_ALGORITHM_3DES:       gcryptAlgorithm = GCRY_CIPHER_3DES;       break;
            case CRYPT_ALGORITHM_CAST5:      gcryptAlgorithm = GCRY_CIPHER_CAST5;      break;
            case CRYPT_ALGORITHM_BLOWFISH:   gcryptAlgorithm = GCRY_CIPHER_BLOWFISH;   break;
            case CRYPT_ALGORITHM_AES128:     gcryptAlgorithm = GCRY_CIPHER_AES;        break;
            case CRYPT_ALGORITHM_AES192:     gcryptAlgorithm = GCRY_CIPHER_AES192;     break;
            case CRYPT_ALGORITHM_AES256:     gcryptAlgorithm = GCRY_CIPHER_AES256;     break;
            case CRYPT_ALGORITHM_TWOFISH128: gcryptAlgorithm = GCRY_CIPHER_TWOFISH128; break;
            case CRYPT_ALGORITHM_TWOFISH256: gcryptAlgorithm = GCRY_CIPHER_TWOFISH;    break;
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

Errors Crypt_new(CryptInfo       *cryptInfo,
                 CryptAlgorithms cryptAlgorithm,
                 const Password  *password
                )
{
  assert(cryptInfo != NULL);

  /* init variables */
  cryptInfo->cryptAlgorithm = cryptAlgorithm;

  /* init crypt algorithm */
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
      #ifdef HAVE_GCRYPT
        {
          uint         passwordLength;
          int          gcryptAlgorithm;
          gcry_error_t gcryptError;
          size_t       n;
          uint         maxKeyLength;
          uint         keyLength;
          char         key[MAX_KEY_SIZE/8];
          int          z;

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
            case CRYPT_ALGORITHM_3DES:       gcryptAlgorithm = GCRY_CIPHER_3DES;       break;
            case CRYPT_ALGORITHM_CAST5:      gcryptAlgorithm = GCRY_CIPHER_CAST5;      break;
            case CRYPT_ALGORITHM_BLOWFISH:   gcryptAlgorithm = GCRY_CIPHER_BLOWFISH;   break;
            case CRYPT_ALGORITHM_AES128:     gcryptAlgorithm = GCRY_CIPHER_AES;        break;
            case CRYPT_ALGORITHM_AES192:     gcryptAlgorithm = GCRY_CIPHER_AES192;     break;
            case CRYPT_ALGORITHM_AES256:     gcryptAlgorithm = GCRY_CIPHER_AES256;     break;
            case CRYPT_ALGORITHM_TWOFISH128: gcryptAlgorithm = GCRY_CIPHER_TWOFISH128; break;
            case CRYPT_ALGORITHM_TWOFISH256: gcryptAlgorithm = GCRY_CIPHER_TWOFISH;    break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }

          /* get max. key length, block length */
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

          /* get key length */
          keyLength = 0;
          switch (cryptAlgorithm)
          {
            case CRYPT_ALGORITHM_3DES:       keyLength = 192; break;
            case CRYPT_ALGORITHM_CAST5:      keyLength = 128; break;
            case CRYPT_ALGORITHM_BLOWFISH:   keyLength = 128; break;
            case CRYPT_ALGORITHM_AES128:     keyLength = 128; break;
            case CRYPT_ALGORITHM_AES192:     keyLength = 192; break;
            case CRYPT_ALGORITHM_AES256:     keyLength = 256; break;
            case CRYPT_ALGORITHM_TWOFISH128: keyLength = 128; break;
            case CRYPT_ALGORITHM_TWOFISH256: keyLength = 256; break;
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

          /* init cipher */
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

          /* set key */
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
          /* set 0 IV */
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

  return ERROR_NONE;
}

void Crypt_delete(CryptInfo *cryptInfo)
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
      #ifdef HAVE_GCRYPT
        {
          uint         ivLength;
          char         iv[MAX_KEY_SIZE/8];
          int          z;
          gcry_error_t gcryptError;

          gcry_cipher_reset(cryptInfo->gcry_cipher_hd);

          if (seed != 0)
          {
            /* get IV length */
            ivLength = 0;
            switch (cryptInfo->cryptAlgorithm)
            {
              case CRYPT_ALGORITHM_3DES:       ivLength = 192; break;
              case CRYPT_ALGORITHM_CAST5:      ivLength = 128; break;
              case CRYPT_ALGORITHM_BLOWFISH:   ivLength = 128; break;
              case CRYPT_ALGORITHM_AES128:     ivLength = 128; break;
              case CRYPT_ALGORITHM_AES192:     ivLength = 192; break;
              case CRYPT_ALGORITHM_AES256:     ivLength = 256; break;
              case CRYPT_ALGORITHM_TWOFISH128: ivLength = 128; break;
              case CRYPT_ALGORITHM_TWOFISH256: ivLength = 256; break;
              default:
                #ifndef NDEBUG
                  HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                #endif /* NDEBUG */
                break; /* not reached */
            }

            /* set IV */
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
  gcry_error_t gcryptError;

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
      #ifdef HAVE_GCRYPT
        assert((bufferLength%cryptInfo->blockLength) == 0);

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
        UNUSED_VARIABLE(gcryptError);
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
  gcry_error_t gcryptError;

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
      #ifdef HAVE_GCRYPT
      assert((bufferLength%cryptInfo->blockLength) == 0);

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
        UNUSED_VARIABLE(gcryptError);
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

void Crypt_initKey(CryptKey *cryptKey)
{
  assert(cryptKey != NULL);

  #ifdef HAVE_GCRYPT
    cryptKey->key = NULL;
  #endif /* HAVE_GCRYPT */
}

void Crypt_doneKey(CryptKey *cryptKey)
{
  assert(cryptKey != NULL);

  #ifdef HAVE_GCRYPT
    if (cryptKey->key != NULL) gcry_sexp_release(cryptKey->key);
  #endif /* HAVE_GCRYPT */
}

CryptKey *Crypt_newKey(void)
{
  CryptKey *cryptKey;

  cryptKey = (CryptKey*)malloc(sizeof(CryptKey));
  if (cryptKey == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  Crypt_initKey(cryptKey);

  return cryptKey;
}

Errors Crypt_createKeys(CryptKey *publicCryptKey,
                        CryptKey *privateCryptKey,
                        uint     bits
                       )
{
  String       description;
  gcry_sexp_t  sexpKeyParameters;
  gcry_sexp_t  sexpKey;
  gcry_error_t gcryptError;

  assert(publicCryptKey != NULL);
  assert(privateCryptKey != NULL);

  /* init keys */
  Crypt_initKey(publicCryptKey);
  Crypt_initKey(privateCryptKey);

  /* create key parameters */
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
    return ERROR_CREATE_KEY_FAIL;
  }

  /* generate keys */
  gcryptError = gcry_pk_genkey(&sexpKey,sexpKeyParameters);
  if (gcryptError != 0)
  {
    gcry_sexp_release(sexpKeyParameters);
    String_delete(description);
    Crypt_doneKey(privateCryptKey);
    Crypt_doneKey(publicCryptKey);
    return ERROR_CREATE_KEY_FAIL;
  }
  gcry_sexp_release(sexpKeyParameters);
  String_delete(description);
  publicCryptKey->key  = gcry_sexp_find_token(sexpKey,"public-key",0);
  privateCryptKey->key = gcry_sexp_find_token(sexpKey,"private-key",0);
  gcry_sexp_release(sexpKey);

  return ERROR_NONE;
}

void Crypt_deleteKey(CryptKey *cryptKey)
{
  assert(cryptKey != NULL);

  Crypt_doneKey(cryptKey);
  free(cryptKey);
}

String Crypt_getKeyData(String         string,
                        const CryptKey *cryptKey
                       )
{
  gcry_sexp_t sexpToken;
  const char  *data; 
  size_t      dataLength;
  size_t      bufferLength;
  char        *buffer;

  assert(cryptKey != NULL);
  assert(string != NULL);

  String_clear(string);
  sexpToken = NULL;
  if (sexpToken == NULL) sexpToken = gcry_sexp_find_token(cryptKey->key,"public-key",0);
  if (sexpToken == NULL) sexpToken = gcry_sexp_find_token(cryptKey->key,"private-key",0);
  if (sexpToken != NULL)
  {
    data = gcry_sexp_nth_data(sexpToken,0,&dataLength);
    if (data != NULL)
    {
      bufferLength = gcry_sexp_sprint(sexpToken,GCRYSEXP_FMT_ADVANCED,NULL,0);
      buffer = (char*)malloc(bufferLength);
      if (buffer == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      gcry_sexp_sprint(sexpToken,GCRYSEXP_FMT_ADVANCED,buffer,bufferLength);
      base64Encode(string,buffer,bufferLength);
      free(buffer);
    }
    gcry_sexp_release(sexpToken);
  }

  return string;
}

Errors Crypt_setKeyData(CryptKey     *cryptKey,
                        const String string
                       )
{
  uint         maxBufferLength;
  char         *buffer;
  size_t       bufferLength;
  gcry_error_t gcryptError;

  assert(cryptKey != NULL);
  assert(string != NULL);

  /* decode baes64 */
  maxBufferLength = String_length(string);
  buffer = (char*)malloc(maxBufferLength);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  bufferLength = base64Decode(buffer,maxBufferLength,string);

  /* create S-expression with key */
  if (cryptKey->key != NULL) gcry_sexp_release(cryptKey->key);
  gcryptError = gcry_sexp_new(&cryptKey->key,buffer,bufferLength,1);
  if (gcryptError != 0)
  {
    free(buffer);
    return ERROR_INVALID_KEY;
  }
  free(buffer);

  return ERROR_NONE;
}

Errors Crypt_keyEncrypt(CryptKey   *cryptKey,
                        const void *buffer,
                        ulong      bufferLength,
                        ulong      maxEncryptBufferLength,
                        void       *encryptBuffer,
                        ulong      *encryptBufferLength
                       )
{
  gcry_mpi_t   n;
  byte         *p;
  ulong        z;
  gcry_sexp_t  sexpData;
  gcry_sexp_t  sexpEncryptData;
  gcry_sexp_t  sexpToken;
  const char   *encryptData;
  size_t       encryptDataLength;
  gcry_error_t gcryptError;

  assert(cryptKey != NULL);
  assert(buffer != NULL);
  assert(encryptBuffer != NULL);
  assert(encryptBufferLength != NULL);

fprintf(stderr,"%s,%d: ---------------------------------\n",__FILE__,__LINE__);
gcry_sexp_dump(cryptKey->key);
fprintf(stderr,"%s,%d: %d\n",__FILE__,__LINE__,bufferLength);

  /* create mpi from data: push data bytes into mpi by shift+add */
  n = gcry_mpi_new(0);
  if (n == NULL)
  {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
    return ERROR_KEY_ENCRYPT_FAIL;
  }
  p = (byte*)buffer;
  for (z = 0; z < bufferLength; z++)
  {
    gcry_mpi_mul_ui(n,n,256);
    gcry_mpi_add_ui(n,n,p[z]);
  }
gcry_mpi_dump(n);fprintf(stderr,"\n");

  /* create S-expression with data */
  sexpData = NULL;
//  gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags raw) (value %b))",(int)bufferLength,(char*)buffer);
//  gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags raw) (value %m))",n);
  gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (value %m))",n);
//  gcryptError = gcry_sexp_build(&sexpData,NULL,"%m",n);
  if (gcryptError != 0)
  {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
    gcry_mpi_release(n);
    return ERROR_KEY_ENCRYPT_FAIL;
  }
//gcry_sexp_dump(sexpData);

  /* encrypt */
  sexpEncryptData = NULL;
  gcryptError = gcry_pk_encrypt(&sexpEncryptData,sexpData,cryptKey->key);
  if (gcryptError != 0)
  {
fprintf(stderr,"%s,%d: %x %s %d\n",__FILE__,__LINE__,gcryptError,gcry_strerror(gcryptError),bufferLength);
    gcry_sexp_release(sexpData);    
    gcry_mpi_release(n);
    return ERROR_KEY_ENCRYPT_FAIL;
  }
//gcry_sexp_dump(sexpEncryptData);

  /* get encrypted data */
  sexpToken = gcry_sexp_find_token(sexpEncryptData,"a",0);
  if (sexpToken == NULL)
  {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
    gcry_sexp_release(sexpEncryptData);
    gcry_sexp_release(sexpData);
    gcry_mpi_release(n);
    return ERROR_KEY_ENCRYPT_FAIL;
  }
//gcry_sexp_dump(sexpToken);
  encryptData = gcry_sexp_nth_data(sexpToken,1,&encryptDataLength);
  if (encryptData == NULL)
  {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
    gcry_sexp_release(sexpEncryptData);
    gcry_sexp_release(sexpData);
    gcry_mpi_release(n);
    return ERROR_KEY_ENCRYPT_FAIL;
  }
  (*encryptBufferLength) = MIN(encryptDataLength,maxEncryptBufferLength);
  memcpy(encryptBuffer,encryptData,*encryptBufferLength);
  gcry_sexp_release(sexpToken);

  /* free resources */
  gcry_sexp_release(sexpEncryptData);
  gcry_sexp_release(sexpData);
  gcry_mpi_release(n);

  return ERROR_NONE;
}

Errors Crypt_keyDecrypt(CryptKey   *cryptKey,
                        const void *encryptBuffer,
                        ulong      encryptBufferLength,
                        ulong      maxBufferLength,
                        void       *buffer,
                        ulong      *bufferLength
                       )
{
  gcry_sexp_t  sexpEncryptData;
  gcry_sexp_t  sexpData;
  const char   *data;
  size_t       dataLength;
  gcry_error_t gcryptError;

  assert(cryptKey != NULL);
  assert(encryptBuffer != NULL);
  assert(buffer != NULL);
  assert(bufferLength != NULL);

  /* create S-expression with encrypted data */
  gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (rsa (a %b)))",encryptBufferLength,encryptBuffer);
  if (gcryptError != 0)
  {
    return ERROR_KEY_ENCRYPT_FAIL;
  }
//gcry_sexp_dump(sexpEncryptData);

  /* decrypt */
  gcryptError = gcry_pk_decrypt(&sexpData,sexpEncryptData,cryptKey->key);
  if (gcryptError != 0)
  {
    gcry_sexp_release(sexpEncryptData);    
    return ERROR_KEY_ENCRYPT_FAIL;
  }
//gcry_sexp_dump(sexpData);

  /* get decrypted data */
  data = gcry_sexp_nth_data(sexpData,0,&dataLength);
  if (data == NULL)
  {
    gcry_sexp_release(sexpData);
    gcry_sexp_release(sexpEncryptData);
    return ERROR_KEY_ENCRYPT_FAIL;
  }
  (*bufferLength) = MIN(dataLength,maxBufferLength);
  memcpy(buffer,data,*bufferLength);

  /* free resources */
  gcry_sexp_release(sexpData);
  gcry_sexp_release(sexpEncryptData);

  return ERROR_NONE;
}

Errors Crypt_getRandomEncryptKey(CryptKey *publicKey,
                                 uint     bits,
                                 Password *password,
                                 uint     maxEncryptBufferLength,
                                 void     *encryptBuffer,
                                 uint     *encryptBufferLength
                                )
{
  gcry_mpi_t   n;
  bool         okFlag;
  void         *buffer;
  size_t       bufferLength,maxBufferLength;
  gcry_sexp_t  sexpData;
  gcry_sexp_t  sexpEncryptData;
  gcry_sexp_t  sexpToken;
  const char   *encryptData;
  size_t       encryptDataLength;
  gcry_error_t gcryptError;

  assert(publicKey != NULL);
  assert(password != NULL);
  assert(encryptBuffer != NULL);
  assert(encryptBufferLength != NULL);

//fprintf(stderr,"%s,%d: ---------------------------------\n",__FILE__,__LINE__);
//gcry_sexp_dump(publicKey->key);

  /* create random mpi from data: push data bytes into mpi by shift+add */
  n = gcry_mpi_snew(bits);
  if (n == NULL)
  {
//fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
    return ERROR_KEY_ENCRYPT_FAIL;
  }
  gcry_mpi_randomize(n,bits,GCRY_STRONG_RANDOM);
//gcry_mpi_dump(n);fprintf(stderr,"\n");

  /* set password */
  maxBufferLength = 512;
  okFlag          = FALSE;
  do
  {
    buffer = Password_allocSecure(maxBufferLength);
    if (buffer == NULL)
    {
      gcry_mpi_release(n);
      return ERROR_KEY_ENCRYPT_FAIL;
    }
    gcryptError = gcry_mpi_print(GCRYMPI_FMT_STD,
                                 buffer,
                                 maxBufferLength,
                                 &bufferLength,
                                 n
                                );
    if (gcryptError != 0)
    {
//fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
      Password_freeSecure(buffer);
      gcry_mpi_release(n);
      return ERROR_KEY_ENCRYPT_FAIL;
    }
    if (bufferLength < maxBufferLength)
    {
      okFlag = TRUE;
    }
    else
    {
      Password_freeSecure(buffer);
      maxBufferLength += 64;
    }
  }
  while (!okFlag);
  Password_setBuffer(password,buffer,bufferLength);
  Password_freeSecure(buffer);

  /* create S-expression with data */
  sexpData = NULL;
//  gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags raw) (value %b))",(int)bufferLength,(char*)buffer);
//  gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (flags raw) (value %m))",n);
  gcryptError = gcry_sexp_build(&sexpData,NULL,"(data (value %m))",n);
//  gcryptError = gcry_sexp_build(&sexpData,NULL,"%m",n);
  if (gcryptError != 0)
  {
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
    gcry_mpi_release(n);
    return ERROR_KEY_ENCRYPT_FAIL;
  }
//fprintf(stderr,"%s,%d: --- plain data \n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpData);

  /* encrypt */
  sexpEncryptData = NULL;
  gcryptError = gcry_pk_encrypt(&sexpEncryptData,sexpData,publicKey->key);
  if (gcryptError != 0)
  {
//fprintf(stderr,"%s,%d: %x %s %d\n",__FILE__,__LINE__,gcryptError,gcry_strerror(gcryptError),bufferLength);
    gcry_sexp_release(sexpData);    
    gcry_mpi_release(n);
    return ERROR_KEY_ENCRYPT_FAIL;
  }
//fprintf(stderr,"%s,%d: --- encrypted data \n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpEncryptData);

  /* get encrypted data */
  sexpToken = gcry_sexp_find_token(sexpEncryptData,"a",0);
  if (sexpToken == NULL)
  {
    gcry_sexp_release(sexpEncryptData);
    gcry_sexp_release(sexpData);
    gcry_mpi_release(n);
    return ERROR_KEY_ENCRYPT_FAIL;
  }
//gcry_sexp_dump(sexpToken);
  encryptData = gcry_sexp_nth_data(sexpToken,1,&encryptDataLength);
  if (encryptData == NULL)
  {
    gcry_sexp_release(sexpEncryptData);
    gcry_sexp_release(sexpData);
    gcry_mpi_release(n);
    return ERROR_KEY_ENCRYPT_FAIL;
  }
  (*encryptBufferLength) = MIN(encryptDataLength,maxEncryptBufferLength);
  memcpy(encryptBuffer,encryptData,*encryptBufferLength);
  gcry_sexp_release(sexpToken);

  /* free resources */
  gcry_sexp_release(sexpEncryptData);
  gcry_sexp_release(sexpData);
  gcry_mpi_release(n);

  return ERROR_NONE;
}

Errors Crypt_getDecryptKey(CryptKey   *privateKey,
                           const void *encryptData,
                           uint       encryptDataLength,
                           Password   *password
                          )
{
  gcry_sexp_t  sexpEncryptData;
  gcry_sexp_t  sexpData;
  const char   *data;
  size_t       dataLength;
  gcry_error_t gcryptError;

  assert(privateKey != NULL);
  assert(encryptData != NULL);
  assert(password != NULL);
//gcry_sexp_dump(privateKey->key);

  /* create S-expression with encrypted data */
  gcryptError = gcry_sexp_build(&sexpEncryptData,NULL,"(enc-val (rsa (a %b)))",encryptDataLength,encryptData);
  if (gcryptError != 0)
  {
    return ERROR_KEY_DECRYPT_FAIL;
  }
//fprintf(stderr,"%s,%d: --- encrypted data \n",__FILE__,__LINE__);
//gcry_sexp_dump(sexpEncryptData);

  /* decrypt */
  gcryptError = gcry_pk_decrypt(&sexpData,sexpEncryptData,privateKey->key);
  if (gcryptError != 0)
  {
    gcry_sexp_release(sexpEncryptData);    
    return ERROR_KEY_DECRYPT_FAIL;
  }
//gcry_sexp_dump(sexpData);

  /* get decrypted data */
  data = gcry_sexp_nth_data(sexpData,0,&dataLength);
  if (data == NULL)
  {
    gcry_sexp_release(sexpData);
    gcry_sexp_release(sexpEncryptData);
    return ERROR_KEY_DECRYPT_FAIL;
  }
  Password_setBuffer(password,data,dataLength);

  /* free resources */
  gcry_sexp_release(sexpData);
  gcry_sexp_release(sexpEncryptData);

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
