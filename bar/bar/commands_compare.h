/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive test function
* Systems: all
*
\***********************************************************************/

#ifndef __COMMANDS_COMPARE__
#define __COMMANDS_COMPARE__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory 

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "stringlists.h"

#include "bar.h"
#include "entrylists.h"
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
* Name   : Commands_compare
* Purpose: compare archive and file system content
* Input  : archiveFileNameList              - list with archive files
*          includeEntryList                 - include entry list
*          excludePatternList               - exclude pattern list
*          jobOptions                       - job options
*          archiveGetCryptPasswordFunction  - get password call back
*          archiveGetCryptPasswordUserData  - user data for get password
*                                             call back
* Output : -
* Return : ERROR_NONE if archive ok, otherwise error code
* Notes  : -
\***********************************************************************/

Errors Command_compare(const StringList                *archiveFileNameList,
                       const EntryList                 *includeEntryList,
                       const PatternList               *excludePatternList,
                       JobOptions                      *jobOptions,
                       ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                       void                            *archiveGetCryptPasswordUserData
                      );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMANDS_COMPARE__ */

/* end of file */
