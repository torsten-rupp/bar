/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_test.h,v $
* $Revision: 1.4 $
* $Author: torsten $
* Contents: Backup ARchiver archive test function
* Systems : all
*
\***********************************************************************/

#ifndef __COMMAND_TEST__
#define __COMMAND_TEST__

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
* Name   : command_test
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool command_test(StringList  *fileNameList,
                  PatternList *includeList,
                  PatternList *excludeList,
                  const char  *password
                 );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMAND_TEST__ */

/* end of file */
