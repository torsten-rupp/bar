/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive create function
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
#include "patternlists.h"
#include "deltasourcelists.h"
#include "compress.h"
#include "crypt.h"
#include "storage.h"
#include "server.h"
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
* Name   : Command_create
* Purpose: create archive
* Input  : jobUUID                          - unique job id to store or NULL
*          scheduleUUID                     - unique schedule id to store or NULL
*          storageName                      - storage name
*          includeEntryList                 - include entry list
*          excludePatternList               - exclude pattern list
*          compressExcludePatternList       - exclude compression pattern
*                                             list
*          deltaSourceList                  - delta soruce list
*          jobOptions                       - job options
*          archiveType                      - archive type; see
*                                             ArchiveTypes (normal/full/
*                                             incremental)
*          archiveGetCryptPasswordFunction  - get password call back
*                                              (can be NULL)
*          archiveGetCryptPasswordUserData  - user data for get password
*                                             call back
*          createStatusInfoFunction         - status info call back
*                                             function (can be NULL)
*          createStatusInfoUserData         - user data for status info
*                                             function
*          storageRequestVolumeFunction     - request volume call back
*                                             function (can be NULL)
*          storageRequestVolumeUserData     - user data for request
*                                             volume
*          pauseCreateFlag                  - pause creation flag (can
*                                             be NULL)
*          pauseStorageFlag                 - pause storage flag (can
*                                             be NULL)
*          requestedAbortFlag               - request abort flag (can be
*                                             NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Command_create(ConstString                     jobUUID,
                      ConstString                     scheduleUUID,
                      ConstString                     storageName,
                      const EntryList                 *includeEntryList,
                      const PatternList               *excludePatternList,
                      const PatternList               *compressExcludePatternList,
                      DeltaSourceList                 *deltaSourceList,
                      JobOptions                      *jobOptions,
                      ArchiveTypes                    archiveType,
                      ConstString                     scheduleTitle,
                      ConstString                     scheduleCustomText,
                      ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                      void                            *archiveGetCryptPasswordUserData,
                      CreateStatusInfoFunction        createStatusInfoFunction,
                      void                            *createStatusInfoUserData,
                      StorageRequestVolumeFunction    storageRequestVolumeFunction,
                      void                            *storageRequestVolumeUserData,
                      bool                            *pauseCreateFlag,
                      bool                            *pauseStorageFlag,
                      bool                            *requestedAbortFlag,
                      LogHandle                       *logHandle
                     );

/***********************************************************************\
* Name   : Command_create
* Purpose: create archive
* Input  : serverInfo                       - server info
*          jobUUID                          - unique job id to store or NULL
*          hostName                         -
*          hostPort                         -
*          hostForceSSL                     -
*          scheduleUUID                     - unique schedule id to store or NULL
*          storageName                      - storage name
*          includeEntryList                 - include entry list
*          excludePatternList               - exclude pattern list
*          compressExcludePatternList       - exclude compression pattern
*                                             list
*          deltaSourceList                  - delta soruce list
*          jobOptions                       - job options
*          archiveType                      - archive type; see
*                                             ArchiveTypes (normal/full/
*                                             incremental)
*          archiveGetCryptPasswordFunction  - get password call back
*                                              (can be NULL)
*          archiveGetCryptPasswordUserData  - user data for get password
*                                             call back
*          createStatusInfoFunction         - status info call back
*                                             function (can be NULL)
*          createStatusInfoUserData         - user data for status info
*                                             function
*          storageRequestVolumeFunction     - request volume call back
*                                             function (can be NULL)
*          storageRequestVolumeUserData     - user data for request
*                                             volume
*          pauseCreateFlag                  - pause creation flag (can
*                                             be NULL)
*          pauseStorageFlag                 - pause storage flag (can
*                                             be NULL)
*          requestedAbortFlag               - request abort flag (can be
*                                             NULL)
*          logHandle                        - log handle (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Command_createRemote(ServerInfo                      *serverInfo,
                            ConstString                     jobUUID,
                            ConstString                     scheduleUUID,
                            ConstString                     storageName,
                            const EntryList                 *includeEntryList,
                            const PatternList               *excludePatternList,
                            const PatternList               *compressExcludePatternList,
                            DeltaSourceList                 *deltaSourceList,
                            JobOptions                      *jobOptions,
                            ArchiveTypes                    archiveType,
                            ConstString                     scheduleTitle,
                            ConstString                     scheduleCustomText,
                            ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                            void                            *archiveGetCryptPasswordUserData,
                            CreateStatusInfoFunction        createStatusInfoFunction,
                            void                            *createStatusInfoUserData,
                            StorageRequestVolumeFunction    storageRequestVolumeFunction,
                            void                            *storageRequestVolumeUserData,
                            bool                            *pauseCreateFlag,
                            bool                            *pauseStorageFlag,
                            bool                            *requestedAbortFlag,
                            LogHandle                       *logHandle
                           );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMANDS_CREATE__ */

/* end of file */
