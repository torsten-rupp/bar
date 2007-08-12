/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_create.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Backup ARchiver archive functions
* Systems : all
*
\***********************************************************************/

#ifndef __COMMAND_CREATE_H__
#define __COMMAND_CREATE_H__

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
* Name   : command_create
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool command_create(const char  *fileName,
                    PatternList *includeList,
                    PatternList *excludeList,
                    const char  *tmpDirectory,
                    ulong       partSize
                   );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMAND_CREATE_H__ */

/* end of file */
