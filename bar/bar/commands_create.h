/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
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

#include "errors.h"
#include "entrylists.h"
#include "common/patternlists.h"
#include "deltasourcelists.h"
#include "compress.h"
#include "crypt.h"
#include "storage.h"
#include "server.h"
#include "bar_global.h"

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
*          jobUUID                      - unique job id to store or NULL
*          scheduleUUID                 - unique schedule id to store or
*                                         NULL
*          storageName                  - storage name
*          includeEntryList             - include entry list
*          excludePatternList           - exclude pattern list
*          mountList                    - mount list or NULL
*          compressExcludePatternList   - exclude compression pattern
*                                         list
*          deltaSourceList              - delta soruce list
*          jobOptions                   - job options
*          entityId                     - entityId or INDEX_ID_NONE
*          archiveType                  - archive type; see
*                                         ArchiveTypes (normal/full/
*                                         incremental)
*          scheduleTitle                - schedule title
*          scheduleCustomText           - schedule custome text or NULL
*          startDateTime                - date/time of start [s]
*          dryRun                       - TRUE for dry-run (no storage,
*                                         no incremental data, no update
*                                         database)
*          getNamePasswordFunction      - get password callback (can
*                                         be NULL)
*          getNamePasswordUserData      - user data for get password
*          statusInfoFunction           - status info callback
*                                         function (can be NULL)
*          statusInfoUserData           - user data for status info
*          storageRequestVolumeFunction - request volume callback
*                                         function (can be NULL)
*          storageRequestVolumeUserData - user data for request
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
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Command_create(ServerIO                     *masterIO,
                      ConstString                  jobUUID,
                      ConstString                  scheduleUUID,
                      ConstString                  storageName,
                      const EntryList              *includeEntryList,
                      const PatternList            *excludePatternList,
                      MountList                    *mountList,
                      const PatternList            *compressExcludePatternList,
                      DeltaSourceList              *deltaSourceList,
                      JobOptions                   *jobOptions,
                      ArchiveTypes                 archiveType,
                      ConstString                  scheduleTitle,
                      ConstString                  scheduleCustomText,
                      uint64                       startDateTime,
                      bool                         dryRun,
                      GetNamePasswordFunction      getNamePasswordFunction,
                      void                         *getNamePasswordUserData,
                      StatusInfoFunction           statusInfoFunction,
                      void                         *statusInfoUserData,
                      StorageRequestVolumeFunction storageRequestVolumeFunction,
                      void                         *storageRequestVolumeUserData,
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
