/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar.h,v $
* $Revision: 1.13 $
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

#include "patterns.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef enum
{
  EXITCODE_OK=0,
  EXITCODE_FAIL=1,

  EXITCODE_INVALID_ARGUMENT=5,
  EXITCODE_CONFIG_ERROR,

  EXITCODE_INIT_FAIL=125,
  EXITCODE_FATAL_ERROR=126,

  EXITCODE_UNKNOWN=128
} ExitCodes;

typedef struct
{
  uint64        maxTmpSize;

  const char    *tmpDirectory;

  uint          sshPort;
  const char    *sshPublicKeyFile;
  const char    *sshPrivatKeyFile;
  const char    *sshPassword;

  bool          overwriteFlag;
  bool          skipUnreadableFlag;
  bool          quietFlag;
  long          verboseLevel;
} GlobalOptions;

/***************************** Variables *******************************/

extern GlobalOptions globalOptions;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : getErrorText
* Purpose: get errror text of error code
* Input  : error - error
* Output : -
* Return : error text (read only!)
* Notes  : -
\***********************************************************************/

const char *getErrorText(Errors error);

/***********************************************************************\
* Name   : info
* Purpose: output info
* Input  : verboseLevel - verbosity level
*          format       - format string (like printf)
*          ...          - optional arguments (like printf)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void info(uint verboseLevel, const char *format, ...);

/***********************************************************************\
* Name   : warning
* Purpose: output warning
* Input  : format - format string (like printf)
*          ...    - optional arguments (like printf)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void warning(const char *format, ...);

/***********************************************************************\
* Name   : printError
* Purpose: print error message
*          text - format string (like printf)
*          ...  - optional arguments (like printf)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void printError(const char *text, ...);

#ifdef __cplusplus
  }
#endif

#endif /* __BAR__ */

/* end of file */
