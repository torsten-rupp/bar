/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive restore function
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
#include "crypt.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
/* status info data */
typedef struct
{
  ulong  doneEntries;                      // number of entries processed
  uint64 doneBytes;                        // number of bytes processed
  ulong  skippedEntries;                   // number of skipped entries
  uint64 skippedBytes;                     // sum of skipped bytes
  ulong  errorEntries;                     // number of entries with errors
  uint64 errorBytes;                       // sum of byste in entries with errors
  String name;                             // current file name
  uint64 entryDoneBytes;                   // number of bytes processed of current entry
  uint64 entryTotalBytes;                  // total number of bytes of current entry
  String storageName;                      // current storage name
  uint64 archiveDoneBytes;                 // number of bytes processed of current archive
  uint64 archiveTotalBytes;                // total bytes of current archive
} RestoreStatusInfo;

/***********************************************************************\
* Name   : RestoreStatusInfoFunction
* Purpose: restore status info call-back
* Input  : userData          - user data
* @param   error             - error code
* @param   restoreStatusInfo - restore status info
* Output : -
* Return : bool TRUE to continue, FALSE to abort
* Notes  : -
\***********************************************************************/

typedef bool(*RestoreStatusInfoFunction)(void                    *userData,
                                         Errors                  error,
                                         const RestoreStatusInfo *restoreStatusInfo
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
* Input  : archiveFileNameList              - list with archive files
*          includeEntryList                 - include entry list
*          excludePatternList               - exclude pattern list
*          jobOptions                       - job options
*          archiveGetCryptPasswordFunction  - get password call back
*          archiveGetCryptPasswordUserData  - user data for get password
*                                             call back
*          createStatusInfoFunction         - status info call back
*                                             function (can be NULL)
*          createStatusInfoUserData         - user data for status info
*                                             function
*          pauseFlag                        - pause flag (can be NULL)
*          requestedAbortFlag               - request abort flag (can be
*                                             NULL)
* Output : -
* Return : ERROR_NONE if all files restored, otherwise error code
* Notes  : -
\***********************************************************************/

Errors Command_restore(const StringList                *archiveFileNameList,
                       const EntryList                 *includeEntryList,
                       const PatternList               *excludePatternList,
                       JobOptions                      *jobOptions,
                       ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                       void                            *archiveGetCryptPasswordUserData,
                       RestoreStatusInfoFunction       restoreStatusInfoFunction,
                       void                            *restoreStatusInfoUserData,
                       bool                            *pauseFlag,
                       bool                            *requestedAbortFlag
                      );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMANDS_RESTORE__ */

/* end of file */
