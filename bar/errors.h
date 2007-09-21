/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/errors.h,v $
* $Revision: 1.3 $
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

  /* pattern matching errors */
  ERROR_INVALID_PATTERN,

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

  /* archive errors */
  ERROR_END_OF_ARCHIVE,
  ERROR_NO_FILE_ENTRY,
  ERROR_NO_FILE_DATA,
  ERROR_END_OF_DATA,
  ERROR_CRC_ERROR,

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
