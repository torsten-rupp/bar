/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar.h,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: Backup ARchiver
* Systems :
*
\***********************************************************************/

#ifndef __BAR_H__
#define __BAR_H__

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

  ERROR_INSUFFICIENT_MEMORY,
  ERROR_IO_ERROR,

  ERROR_UNKNOWN,
} Errors;

typedef enum
{
  EXITCODE_OK=0,
  EXITCODE_FAIL=1,

  EXITCODE_INVALID_ARGUMENT=5,

  EXITCODE_FATAL_ERROR=126,

  EXITCODE_UNKNOWN=128
} ExitCodes;

typedef struct PatternNode
{
  NODE_HEADER(struct PatternNode);

  String pattern;
} PatternNode;

typedef struct
{
  LIST_HEADER(PatternNode);
} PatternList;

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

#endif /* __BAR_H__ */

/* end of file */
