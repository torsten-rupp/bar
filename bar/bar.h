/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar.h,v $
* $Revision: 1.6 $
* $Author: torsten $
* Contents: Backup ARchiver main program
* Systems :
*
\***********************************************************************/

#ifndef __BAR__
#define __BAR__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "lists.h"
#include "strings.h"

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

  /* crypt errors */
  ERROR_UNSUPPORTED_BLOCK_SIZE,
  ERROR_INIT_CRYPT,
  ERROR_NO_PASSWORD,
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

  /* others */
  ERROR_UNKNOWN,
} Errors;

typedef enum
{
  EXITCODE_OK=0,
  EXITCODE_FAIL=1,

  EXITCODE_INVALID_ARGUMENT=5,

  EXITCODE_INIT_FAIL=125,
  EXITCODE_FATAL_ERROR=126,

  EXITCODE_UNKNOWN=128
} ExitCodes;

typedef struct FileNameNode
{
  NODE_HEADER(struct FileNameNode);

  String fileName;
} FileNameNode;

typedef struct
{
  LIST_HEADER(FileNameNode);
} FileNameList;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

void printError(const char *text, ...);

#ifdef __cplusplus
  }
#endif

#endif /* __BAR__ */

/* end of file */
