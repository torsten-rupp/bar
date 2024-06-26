/***********************************************************************\
*
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

#include "bar.h"

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
* Input  : archiveFileNameList     - list with archive files
*          newJobUUID              - new job UUID or NULL
*          newEntityUUID           - new entity UUID or NULL
*          newCreatedDateTime      - new created date/time or 0
*          newJobOptions           - new job options
*          getNamePasswordFunction - get password call back
*          getNamePasswordUserData - user data for get password call back
*          logHandle               - log handle (can be NULL)
* Output : -
* Return : ERROR_NONE if archive ok, otherwise error code
* Notes  : -
\***********************************************************************/

Errors Command_convert(const StringList        *archiveFileNameList,
                       const char              *newJobUUID,
                       const char              *newEntityUUID,
                       uint64                  newCreatedDateTime,
                       JobOptions              *newJobOptions,
                       GetNamePasswordFunction getNamePasswordFunction,
                       void                    *getNamePasswordUserData,
                       LogHandle               *logHandle
                      );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMANDS_CONVERT__ */

/* end of file */
