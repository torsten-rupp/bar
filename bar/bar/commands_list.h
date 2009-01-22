/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/commands_list.h,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: Backup ARchiver archive list function
* Systems : all
*
\***********************************************************************/

#ifndef __COMMANDS_LIST__
#define __COMMANDS_LIST__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "stringlists.h"

#include "bar.h"
#include "patternlists.h"
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
*          jobOptiosn   - job options
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Command_list(StringList  *archiveFileNameList,
                    PatternList *includePatternList,
                    PatternList *excludePatternList,
                    JobOptions  *joboptions
                   );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMAND_LIST__ */

/* end of file */
