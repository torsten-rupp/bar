/***********************************************************************\
*
* Contents: Backup ARchiver archive create functions
* Systems: all
*
\***********************************************************************/

#ifndef __COMMANDS_CREATE__
#define __COMMANDS_CREATE__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "bar_common.h"
#include "errors.h"
#include "entrylists.h"
#include "common/patternlists.h"
#include "deltasourcelists.h"
#include "compress.h"
#include "crypt.h"
#include "storage.h"
#include "server.h"

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
* Name   : Command_create
* Purpose: create archive
* Input  : masterIO                     - master i/o or NULL
*          jobUUID                      - job UUID to store or NULL
*          entityUUID                   - entity UUID to store or NULL
*          scheduleTitle                - schedule title
*          archiveType                  - archive type; see
*                                         ArchiveTypes (normal/full/
*                                         incremental)
*          storageName                  - storage name
*          includeEntryList             - include entry list
*          excludePatternList           - exclude pattern list
*          customText                   - custome text or NULL
*          jobOptions                   - job options
*          createdDateTime              - date/time of created [s]
*          getNamePasswordFunction      - get password callback (can
*                                         be NULL)
*          getNamePasswordUserData      - user data for get password
*          runningInfoFunction          - running info callback
*                                         function (can be NULL)
*          runningInfoUserData          - user data for running info
*          storageVolumeRequestFunction - request volume callback
*                                         function (can be NULL)
*          storageVolumeRequestUserData - user data for request
*                                         volume
*          isPauseCreateFunction        - is pause check callback (can
*                                         be NULL)
*          isPauseCreateUserData        - user data for is pause create
*                                         check
*          isPauseStorageFunction       - is pause storage callback (can
*                                         be NULL)
*          isPauseStorageUserData       - user data for is pause storage
*                                         check
*          isAbortedFunction            - is abort check callback (can be
*                                         NULL)
*          isAbortedUserData            - user data for is aborted check
*          logHandle                    - log handle (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Command_create(ServerIO                     *masterIO,
                      const char                   *jobUUID,
                      const char                   *scheduleUUID,
                      const char                   *scheduleTitle,
                      const char                   *entityUUID,
                      ArchiveTypes                 archiveType,
                      ConstString                  storageName,
                      const EntryList              *includeEntryList,
                      const PatternList            *excludePatternList,
                      const char                   *customText,
                      JobOptions                   *jobOptions,
                      uint64                       createdDateTime,
                      GetNamePasswordFunction      getNamePasswordFunction,
                      void                         *getNamePasswordUserData,
                      RunningInfoFunction          runningInfoFunction,
                      void                         *runningInfoUserData,
                      StorageVolumeRequestFunction storageVolumeRequestFunction,
                      void                         *storageVolumeRequestUserData,
                      IsPauseFunction              isPauseCreateFunction,
                      void                         *isPauseCreateUserData,
                      IsPauseFunction              isPauseStorageFunction,
                      void                         *isPauseStorageUserData,
                      IsAbortedFunction            isAbortedFunction,
                      void                         *isAbortedUserData,
                      LogHandle                    *logHandle
                     );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMANDS_CREATE__ */

/* end of file */
