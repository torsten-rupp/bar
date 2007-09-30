/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_list.h,v $
* $Revision: 1.5 $
* $Author: torsten $
* Contents: Backup ARchiver archive list function
* Systems : all
*
\***********************************************************************/

#ifndef __COMMANDS_LIST__
#define __COMMANDS_LIST__

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
* Name   : Command_list
* Purpose: list content of archive(s)
* Input  : fileNameList - list with archive files
*          includeList  - include list
*          excludeList  - exclude list
*          password     - crypt password
* Output : -
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Command_list(StringList  *archiveFileNameList,
                  PatternList *includePatternList,
                  PatternList *excludePatternList,
                  const char  *password
                 );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMAND_LIST__ */

/* end of file */
