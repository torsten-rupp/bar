/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/errors.h,v $
* $Revision: 1.8 $
* $Author: torsten $
* Contents: Backup ARchiver errors
* Systems : all
*
\***********************************************************************/

#ifndef __ERRORS__
#define __ERRORS__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <assert.h>

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef enum
{
  ERROR_NONE,

  /* general errors */
  ERROR_INSUFFICIENT_MEMORY,
  ERROR_INIT,
  ERROR_INVALID_ARGUMENT,
  ERROR_CONFIG,
  ERROR_ABORTED,

  /* pattern matching errors */
  ERROR_INVALID_PATTERN,

  /* SSL errors */
  ERROR_INIT_TLS,
  ERROR_INIT_INVALID_CERTIFICATE,
  ERROR_TLS_HANDSHAKE,

  /* compress errors */
  ERROR_INIT_COMPRESS,
  ERROR_COMPRESS_ERROR,
  ERROR_DEFLATE_ERROR,
  ERROR_INFLATE_ERROR,

  /* crypt errors */
  ERROR_UNSUPPORTED_BLOCK_SIZE,
  ERROR_INIT_CRYPT,
  ERROR_NO_PASSWORD,
  ERROR_INVALID_PASSWORD,
  ERROR_INIT_CIPHER,
  ERROR_ENCRYPT_FAIL,
  ERROR_DECRYPT_FAIL,

  /* file errors */
  ERROR_CREATE_FILE,
  ERROR_OPEN_FILE,
  ERROR_OPEN_DIRECTORY,
  ERROR_IO_ERROR,
  ERROR_FILE_EXITS,
  ERROR_FILE_NOT_FOUND,

  /* archive errors */
  ERROR_END_OF_ARCHIVE,
  ERROR_NO_FILE_ENTRY,
  ERROR_NO_FILE_DATA,
  ERROR_END_OF_DATA,
  ERROR_CRC_ERROR,
  ERROR_FILE_INCOMPLETE,
  ERROR_WRONG_FILE_TYPE,
  ERROR_FILES_DIFFER,

  /* network errors */
  ERROR_HOST_NOT_FOUND,
  ERROR_CONNECT_FAIL,
  ERROR_NO_SSH_PASSWORD,
  ERROR_SSH_SESSION_FAIL,
  ERROR_SSH_AUTHENTIFICATION,
  ERROR_AUTHORIZATION,
  ERROR_NETWORK_SEND,
  ERROR_NETWORK_RECEIVE,

  /* others */
  ERROR_UNKNOWN,
} Errors;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifdef __cplusplus
  }
#endif

#endif /* __ERRORS__ */

/* end of file */
