/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_list.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Backup ARchiver archive functions
* Systems : all
*
\***********************************************************************/

#ifndef __COMMAND_LIST_H__
#define __COMMAND_LIST_H__

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
* Name   : command_list
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool command_list(FileNameList *fileNameList,
                  PatternList  *includeList,
                  PatternList  *excludeList
                 );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMAND_LIST_H__ */

/* end of file */
