/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_test.h,v $
* $Revision: 1.8 $
* $Author: torsten $
* Contents: Backup ARchiver archive test function
* Systems : all
*
\***********************************************************************/

#ifndef __COMMANDS_TEST__
#define __COMMANDS_TEST__

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
* Name   : Commands_test
* Purpose: compare archive and file system content
* Input  : fileNameList - list with archive files
*          includeList  - include list
*          excludeList  - exclude list
*          optiosn      - options
* Output : -
* Return : ERROR_NONE if archive ok, otherwise error code
* Notes  : -
\***********************************************************************/

Errors Command_test(StringList  *fileNameList,
                    PatternList *includeList,
                    PatternList *excludeList,
                    Options     *options
                   );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMANDS_TEST__ */

/* end of file */
