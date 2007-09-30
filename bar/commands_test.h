/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_test.h,v $
* $Revision: 1.5 $
* $Author: torsten $
* Contents: Backup ARchiver archive test function
* Systems : all
*
\***********************************************************************/

#ifndef __COMMANDS_TEST__
#define __COMMANDS_TEST__

/****************************** Includes *******************************/
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
*          password     - crypt password
* Output : -
* Return : TRUE if archive ok, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Command_test(StringList  *fileNameList,
                  PatternList *includeList,
                  PatternList *excludeList,
                  const char  *password
                 );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMANDS_TEST__ */

/* end of file */
