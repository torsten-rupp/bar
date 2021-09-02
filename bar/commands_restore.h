/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive restore functions
* Systems: all
*
\***********************************************************************/

#ifndef __COMMANDS_RESTORE__
#define __COMMANDS_RESTORE__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/stringlists.h"

#include "bar.h"
#include "bar_global.h"
#include "entrylists.h"
#include "common/patternlists.h"
#include "deltasourcelists.h"
#include "crypt.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***********************************************************************\
* Name   : RestoreStatusInfoFunction
* Purpose: restore status info call-back
* Input  : statusInfo - status info
*          userData   - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*RestoreUpdateStatusInfoFunction)(const StatusInfo *statusInfo,
                                               void             *userData
                                              );

/***********************************************************************\
* Name   : RestoreErrorFunction
* Purpose: restore status info call-back
* Input  : error      - error code
*          statusInfo - status info
*          userData   - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*RestoreHandleErrorFunction)(Errors           error,
                                            const StatusInfo *statusInfo,
                                            void             *userData
                                           );

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Command_restore
* Purpose: restore archive content
* Input  : storageNameList              - list with storage names
*          includeEntryList             - include entry list
*          excludePatternList           - exclude pattern list
*          jobOptions                   - job options
*          dryRun                       - TRUE for dry-run (no storage,
*                                         no incremental data, no update
*                                         database)
*          updateStatusInfoFunction     - status info call back
*                                         function (can be NULL)
*          updateStatusInfoUserData     - user data for status info
*                                         function
*          handleErrorFunction          - error call back (can be NULL)
*          handleErrorUserData          - user data for error call back
*          getNamePasswordFunction      - get password call back (can be
*                                         NULL)
*          getNamePasswordUserData      - user data for get password call
*                                         back
*          isPauseFunction              - check for pause (can be NULL)
*          isPauseUserData              - user data for check for pause
*          isAbortedFunction            - check for aborted (can be NULL)
*          isAbortedUserData            - user data for check for
*                                         aborted
*          logHandle                    - log handle (can be NULL)
* Output : -
* Return : ERROR_NONE if all files restored, otherwise error code
* Notes  : -
\***********************************************************************/

Errors Command_restore(const StringList                *storageNameList,
                       const EntryList                 *includeEntryList,
                       const PatternList               *excludePatternList,
                       JobOptions                      *jobOptions,
                       StorageFlags                    storageFlags,
                       RestoreUpdateStatusInfoFunction updateStatusInfoFunction,
                       void                            *updateStatusInfoUserData,
                       RestoreHandleErrorFunction      handleErrorFunction,
                       void                            *handleErrorUserData,
                       GetNamePasswordFunction         getNamePasswordFunction,
                       void                            *getNamePasswordUserData,
                       IsPauseFunction                 isPauseFunction,
                       void                            *isPauseUserData,
                       IsAbortedFunction               isAbortedFunction,
                       void                            *isAbortedUserData,
                       LogHandle                       *logHandle
                      );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMANDS_RESTORE__ */

/* end of file */
