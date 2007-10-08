/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/crypt.c,v $
* $Revision: 1.10 $
* $Author: torsten $
* Contents: Backup ARchiver crypt functions
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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

  {"3des",      CRYPT_ALGORITHM_3DES       },
  {"cast5",     CRYPT_ALGORITHM_CAST5      },
  {"blowfish",  CRYPT_ALGORITHM_BLOWFISH   },
  {"aes128",    CRYPT_ALGORITHM_AES128     },
  {"aes192",    CRYPT_ALGORITHM_AES192     },
  {"aes256",    CRYPT_ALGORITHM_AES256     },
  {"twofish128",CRYPT_ALGORITHM_TWOFISH128 },
  {"twofish256",CRYPT_ALGORITHM_TWOFISH256 },
};

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

Errors Crypt_init(void)
{
  if (!gcry_check_version(GCRYPT_VERSION))
  {
    printError("Wrong gcrypt version (needed: %d)\n",GCRYPT_VERSION);
    return ERROR_INIT_CRYPT;
  }

//  gcry_set_progress_handler (progress_handler, NULL);
  
  gcry_control(GCRYCTL_DISABLE_SECMEM,         0);
  gcry_control(GCRYCTL_INITIALIZATION_FINISHED,0);
  gcry_control(GCRYCTL_ENABLE_QUICK_RANDOM,    0);
  #ifndef NDEBUG
    gcry_control(GCRYCTL_SET_DEBUG_FLAGS,1,0);
  #endif

  return ERROR_NONE;
}

void Crypt_done(void)
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
      {
        int    gcryptAlgorithm;
        int    gcryptError;
        size_t n;

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
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
          #endif /* NDEBUG */
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
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

Errors Crypt_new(CryptInfo       *cryptInfo,
                 CryptAlgorithms cryptAlgorithm,
                 const char      *password
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
          return ERROR_NO_PASSWORD;
        }
        passwordLength = strlen(password);
        if (passwordLength <= 0)
        {
          return ERROR_NO_PASSWORD;
        }

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
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
          #endif /* NDEBUG */
        }

        /* get key length, block length */
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
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
          #endif /* NDEBUG */
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
        for (z = 0; z < (keyLength+7)/8; z++)
        {
          key[z] = password[z%passwordLength];
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
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
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
      gcry_cipher_close(cryptInfo->gcry_cipher_hd);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
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
      {
        uint ivLength;
        char iv[MAX_KEY_SIZE/8];
        int  z;
        int  gcryptError;

        gcry_cipher_reset(cryptInfo->gcry_cipher_hd);

        if (seed != 0)
        {
          /* get IV length */
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
            #ifndef NDEBUG
              default:
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                break; /* not reached */
            #endif /* NDEBUG */
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
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
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
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
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
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
