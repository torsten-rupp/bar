/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_test.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Backup ARchiver archive functions
* Systems : all
*
\***********************************************************************/

#ifndef __COMMAND_TEST_H__
#define __COMMAND_TEST_H__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "bar.h"

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

bool command_test(FileNameList *fileNameList,
                  PatternList  *includeList,
                  PatternList  *excludeList
                 );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMAND_TEST_H__ */

/* end of file */
