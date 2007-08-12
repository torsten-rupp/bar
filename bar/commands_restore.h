/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_restore.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Backup ARchiver archive functions
* Systems : all
*
\***********************************************************************/

#ifndef __COMMAND_RESTORE_H__
#define __COMMAND_RESTORE_H__

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
* Name   : command_restore
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool command_restore(FileNameList *fileNameList,
                     PatternList  *includeList,
                     PatternList  *excludeList,
                     const char   *directory
                    );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMAND_RESTORE_H__ */

/* end of file */
