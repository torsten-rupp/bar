/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_restore.h,v $
* $Revision: 1.7 $
* $Author: torsten $
* Contents: Backup ARchiver archive restore function
* Systems : all
*
\***********************************************************************/

#ifndef __COMMANDS_RESTORE__
#define __COMMANDS_RESTORE__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "stringlists.h"

#include "bar.h"
#include "patterns.h"
#include "crypt.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Command_restore
* Purpose: restore archive content
* Input  : fileNameList - list with archive files
*          includeList  - include list
*          excludeList  - exclude list
*          optiosn      - options
* Output : -
* Return : ERROR_NONE if all files restored, otherwise error code
* Notes  : -
\***********************************************************************/

Errors Command_restore(StringList    *fileNameList,
                       PatternList   *includeList,
                       PatternList   *excludeList,
                       const Options *options
                      );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMANDS_RESTORE__ */

/* end of file */
