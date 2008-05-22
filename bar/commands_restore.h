/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_restore.h,v $
* $Revision: 1.11 $
* $Author: torsten $
* Contents: Backup ARchiver archive restore function
* Systems : all
*
\***********************************************************************/

#ifndef __COMMANDS_RESTORE__
#define __COMMANDS_RESTORE__

/****************************** Includes *******************************/
#include "config.h"

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
/* status info data */
typedef struct
{
  ulong  doneFiles;                        // number of files processed
  uint64 doneBytes;                        // number of bytes processed
  ulong  skippedFiles;                     // number of skipped files
  uint64 skippedBytes;                     // sum of skipped bytes
  ulong  errorFiles;                       // number of files with errors
  uint64 errorBytes;                       // sum of byste in files with errors
  String fileName;                         // current file name
  uint64 fileDoneBytes;                    // number of bytes processed of current file
  uint64 fileTotalBytes;                   // total number of bytes of current file
  String storageName;                      // current storage name
  uint64 storageDoneBytes;                 // number of bytes processed of current storage
  uint64 storageTotalBytes;                // total bytes of current storage
} RestoreStatusInfo;

typedef void(*RestoreStatusInfoFunction)(Errors                  error,
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
* Input  : archiveFileNameList      - list with archive files
*          includePatternList       - include list
*          excludePatternList       - exclude list
*          jobOptions               - job options
*          createStatusInfoFunction - status info call back function
*                                     (can be NULL)
*          createStatusInfoUserData - user data for status info function
*          pauseFlag                - pause flag (can be NULL)
*          requestedAbortFlag       - request abort flag (can be NULL)
* Output : -
* Return : ERROR_NONE if all files restored, otherwise error code
* Notes  : -
\***********************************************************************/

Errors Command_restore(StringList                *archiveFileNameList,
                       PatternList               *includePatternList,
                       PatternList               *excludePatternList,
                       JobOptions                *jobOptions,
                       RestoreStatusInfoFunction restoreStatusInfoFunction,
                       void                      *restoreStatusInfoUserData,
                       bool                      *pauseFlag,
                       bool                      *requestedAbortFlag
                      );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMANDS_RESTORE__ */

/* end of file */
