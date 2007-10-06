/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/crypt.h,v $
* $Revision: 1.7 $
* $Author: torsten $
* Contents: Backup ARchive crypt functions
* Systems : all
*
\***********************************************************************/

#ifndef __CRYPT__
#define __CRYPT__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <gcrypt.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "errors.h"

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

/***************************** Datatypes *******************************/

/* password */
typedef struct
{
  char data[256];
} Password;

/* crypt info block */
typedef struct
{
  CryptAlgorithms  cryptAlgorithm;
  uint             blockLength;
  gcry_cipher_hd_t gcry_cipher_hd;
} CryptInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Crypt_init
* Purpose: initialize crypt functions
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Crypt_init(void);

/***********************************************************************\
* Name   : Crypt_done
* Purpose: deinitialize crypt functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Crypt_done(void);

#if 0
/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

const char *Crypt_setPassword(PasswordInfo *passwordInfo);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

const char *Crypt_Password(PasswordInfo *passwordInfo, char *buffer, uint bufferSize);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

const char *Crypt_Password(PasswordInfo *passwordInfo, char *buffer, uint bufferSize);
#endif /* 0 */

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
                 const char      *password
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

#ifdef __cplusplus
  }
#endif

#endif /* __CRYPT__ */

/* end of file */
