/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/crypt.c,v $
* $Revision: 1.2 $
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

#define MAX_KEY_SIZE 128
#define BUFFER_SIZE  (64*1024)

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

Errors Crypt_getBlockLength(CryptAlgorithms cryptAlgorithm,
                            uint            *blockLength
                           )
{
  int    gcryptError;
  size_t n;

  assert(blockLength != NULL);

  switch (cryptAlgorithm)
  {
    case CRYPT_ALGORITHM_NONE:
      (*blockLength) = 1;
      break;
    case CRYPT_ALGORITHM_AES128:
      gcryptError = gcry_cipher_algo_info(GCRY_CIPHER_AES,
                                          GCRYCTL_GET_BLKLEN,
                                          NULL,
                                          &n
                                         );
      if (gcryptError != 0)
      {
        printError("Cannot detect block length of AES cipher (error: %s)\n",gpg_strerror(gcryptError));
        return ERROR_INIT_CIPHER;
      }
      (*blockLength) = n;
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
  int    gcryptError;
  size_t n;
  char   key[MAX_KEY_SIZE/8];
  int    passwordLength;
  int    z;

  assert(cryptInfo != NULL);

  /* init variables */
  cryptInfo->cryptAlgorithm = cryptAlgorithm;

  /* init crypt algorithm */
  switch (cryptAlgorithm)
  {
    case CRYPT_ALGORITHM_NONE:
      break;
    case CRYPT_ALGORITHM_AES128:
      if (password == NULL)
      {
        printError("No password given for cipher!\n");
        return ERROR_NO_PASSWORD;
      }

      /* get key length, block length */
      gcryptError = gcry_cipher_algo_info(GCRY_CIPHER_AES,
                                          GCRYCTL_GET_KEYLEN,
                                          NULL,
                                          &n
                                         );
      if (gcryptError != 0)
      {
        printError("Cannot detect max. key length of AES cipher (error: %s)\n",gpg_strerror(gcryptError));
        return ERROR_INIT_CIPHER;
      }
      if (n*8 < 128)
      {
        printError("AES cipher does not support 128bit keys\n");
        return ERROR_INIT_CIPHER;
      }
      gcryptError = gcry_cipher_algo_info(GCRY_CIPHER_AES,
                                          GCRYCTL_GET_BLKLEN,
                                          NULL,
                                          &n
                                         );
      if (gcryptError != 0)
      {
        printError("Cannot detect block length of AES cipher (error: %s)\n",gpg_strerror(gcryptError));
        return ERROR_INIT_CIPHER;
      }
      if (n > BUFFER_SIZE)
      {
        HALT_INTERNAL_ERROR("AES cipher block length %d to large\n",n);
      }
      cryptInfo->blockLength = n;

      /* init AES cipher */
      gcryptError = gcry_cipher_open(&cryptInfo->gcry_cipher_hd,
                                     GCRY_CIPHER_AES,
                                     GCRY_CIPHER_MODE_CBC,
                                     GCRY_CIPHER_CBC_CTS
                                    );
      if (gcryptError != 0)
      {
        printError("Open AES cipher failed (error: %s)\n",gpg_strerror(gcryptError));
        return ERROR_INIT_CIPHER;
      }

      /* set key */
      passwordLength=strlen(password);
      for (z=0; z<MAX_KEY_SIZE/8; z++)
      {
        key[z] = password[z%passwordLength];
      }
      gcryptError = gcry_cipher_setkey(cryptInfo->gcry_cipher_hd,
                                       key,
                                       sizeof(key)
                                      );
      memset(key,0,sizeof(key));
      if (gcryptError != 0)
      {
        printError("Cannot set cipher key (error: %s)\n",gpg_strerror(gcryptError));
        gcry_cipher_close(cryptInfo->gcry_cipher_hd);
        return ERROR_INIT_CIPHER;
      }
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
    case CRYPT_ALGORITHM_AES128:
      gcry_cipher_close(cryptInfo->gcry_cipher_hd);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
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
    case CRYPT_ALGORITHM_AES128:
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
    case CRYPT_ALGORITHM_AES128:
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
