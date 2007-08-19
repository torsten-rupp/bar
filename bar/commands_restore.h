/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_restore.h,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: Backup ARchiver archive functions: restore
* Systems : all
*
\***********************************************************************/

#ifndef __COMMAND_RESTORE__
#define __COMMAND_RESTORE__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

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
                     uint         directoryStripCount,
                     const char   *directory,
                     const char   *password
                    );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMAND_RESTORE__ */

/* end of file */
