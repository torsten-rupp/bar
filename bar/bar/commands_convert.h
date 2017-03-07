/***********************************************************************\
*
* $Revision: 6759 $
* $Date: 2016-11-15 15:58:10 +0100 (Tue, 15 Nov 2016) $
* $Author: torsten $
* Contents: Backup ARchiver archive convert functions
* Systems: all
*
\***********************************************************************/

#ifndef __COMMANDS_CONVERT__
#define __COMMANDS_CONVERT__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "stringlists.h"

#include "bar.h"
#include "entrylists.h"
#include "patternlists.h"
#include "deltasourcelists.h"
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
* Name   : Commands_convert
* Purpose: convert archive and file system content
* Input  : archiveFileNameList - list with archive files
*          includeEntryList    - include entry list
*          excludePatternList  - exclude pattern list
*          deltaSourceList     - delta source list
*          jobOptions          - job options
*          getPasswordFunction - get password call back
*          getPasswordUserData - user data for get password call back
*          logHandle           - log handle (can be NULL)
* Output : -
* Return : ERROR_NONE if archive ok, otherwise error code
* Notes  : -
\***********************************************************************/

Errors Command_convert(const StringList    *archiveFileNameList,
                       const EntryList     *includeEntryList,
                       const PatternList   *excludePatternList,
                       DeltaSourceList     *deltaSourceList,
                       JobOptions          *jobOptions,
                       GetPasswordFunction getPasswordFunction,
                       void                *getPasswordUserData,
                       LogHandle           *logHandle
                      );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMANDS_CONVERT__ */

/* end of file */
