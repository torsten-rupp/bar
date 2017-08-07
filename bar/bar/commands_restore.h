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

#include "stringlists.h"

#include "bar.h"
#include "entrylists.h"
#include "patternlists.h"
#include "deltasourcelists.h"
#include "crypt.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
/* status info data */
typedef struct
{
  String     storageName;                  // current storage name
  uint64     storageDoneSize;              // number of bytes processed of current archive
  uint64     storageTotalSize;             // total bytes of current archive
  ulong      doneCount;                    // number of entries processed
  uint64     doneSize;                     // number of bytes processed
  ulong      skippedEntryCount;            // number of skipped entries
  uint64     skippedEntrySize;             // sum of skipped bytes
  ulong      errorEntryCount;              // number of entries with errors
  uint64     errorEntrySize;               // sum of bytes of entries with errors
  String     entryName;                    // current entry name
  uint64     entryDoneSize;                // number of bytes processed of current entry
  uint64     entryTotalSize;               // total number of bytes of current entry
//TODO remove
  const char *requestPasswordType;         // request password type or NULL
  const char *requestPasswordText;         // request password host name or NULL
  const char *requestVolume;               // request volume or NULL
} RestoreStatusInfo;

/***********************************************************************\
* Name   : RestoreStatusInfoFunction
* Purpose: restore status info call-back
* Input  : restoreStatusInfo - restore status info
*          userData          - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*RestoreUpdateStatusInfoFunction)(const RestoreStatusInfo *restoreStatusInfo,
                                               void                    *userData
                                              );

/***********************************************************************\
* Name   : RestoreErrorFunction
* Purpose: restore status info call-back
* Input  : error             - error code
*          restoreStatusInfo - restore status info
*          userData          - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*RestoreHandleErrorFunction)(Errors                  error,
                                            const RestoreStatusInfo *restoreStatusInfo,
                                            void                    *userData
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
*          deltaSourceList              - delta source list
*          jobOptions                   - job options
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
                       DeltaSourceList                 *deltaSourceList,
                       JobOptions                      *jobOptions,
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
