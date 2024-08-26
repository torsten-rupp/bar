/***********************************************************************\
*
* Contents: Backup ARchiver archive test functions
* Systems: all
*
\***********************************************************************/

#ifndef __COMMANDS_TEST__
#define __COMMANDS_TEST__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/stringlists.h"

#include "bar.h"
#include "entrylists.h"
#include "common/patternlists.h"
#include "deltasourcelists.h"
#include "crypt.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***********************************************************************\
* Name   : TestRunningInfoFunction
* Purpose: test running info call-back
* Input  : runningInfo - running info
*          userData    - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*TestRunningInfoFunction)(const RunningInfo *runningInfo,
                                       void              *userData
                                      );

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Commands_test
* Purpose: compare archive and file system content
* Input  : archiveFileNameList     - list with archive files
*          includeEntryList        - include entry list (can be NULL)
*          excludePatternList      - exclude pattern list (can be NULL)
*          jobOptions              - job options
*          getNamePasswordFunction - get password call back
*          getNamePasswordUserData - user data for get password call back
*          isAbortedFunction       - check for aborted (can be NULL)
*          isAbortedUserData       - user data for check for aborted
* Output : -
* Return : ERROR_NONE if archive ok, otherwise error code
* Notes  : -
\***********************************************************************/

Errors Command_test(const StringList        *archiveFileNameList,
                    const EntryList         *includeEntryList,
                    const PatternList       *excludePatternList,
                    JobOptions              *jobOptions,
                    TestRunningInfoFunction updateRunningInfoFunction,
                    void                    *updateRunningInfoUserData,
                    GetNamePasswordFunction getNamePasswordFunction,
                    void                    *getNamePasswordUserData,
                    IsAbortedFunction       isAbortedFunction,
                    void                    *isAbortedUserData,
                    LogHandle               *logHandle
                   );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMANDS_TEST__ */

/* end of file */
