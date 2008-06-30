/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/crypt.c,v $
* $Revision: 1.23 $
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

#include "crypt.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define MAX_KEY_SIZE 2048          // max. size of a key in bits

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

// *blockLength=16;
//return ERROR_NONE;
  switch (cryptAlgorithm)
  {
    case CRYPT_ALGORITHM_NONE:
      (*blockLength) = 1;
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
          int    gcryptAlgorithm;
          int    gcryptError;
          size_t n;

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
          uint   passwordLength;
          int    gcryptAlgorithm;
          int    gcryptError;
          size_t n;
          uint   maxKeyLength;
          uint   keyLength;
          char   key[MAX_KEY_SIZE/8];
          int    z;

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
          uint ivLength;
          char iv[MAX_KEY_SIZE/8];
          int  z;
          int  gcryptError;

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
  int gcryptError;

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
  int gcryptError;

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

#ifdef __cplusplus
  }
#endif

/* end of file */
