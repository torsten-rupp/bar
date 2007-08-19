/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/crypt.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: archive functions
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

#include "bar.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

typedef enum
{
  CRYPT_ALGORITHM_NONE,

  CRYPT_ALGORITHM_AES128,
} CryptAlgorithms;

/***************************** Datatypes *******************************/

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
* Input  : -
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
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

void Crypt_delete(CryptInfo *cryptInfo);

/***********************************************************************\
* Name   : Crypt_encrypt
* Purpose: encrypt data
* Input  : -
* Output : -
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
* Input  : -
* Output : -
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
