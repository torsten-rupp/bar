/***********************************************************************\
*
* $Revision: 4135 $
* $Date: 2015-09-19 11:03:57 +0200 (Sat, 19 Sep 2015) $
* $Author: torsten $
* Contents: Backup ARchiver remote functions
* Systems: all
*
\***********************************************************************/

#ifndef __REMOTE__
#define __REMOTE__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

// remote host
typedef struct
{
  String name;                               // name of remote host where job should run
  uint   port;                               // port of remote host where job should run or 0 for default
  bool   forceSSL;                           // force SSL connection to remote hose
} RemoteHost;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Remote_initHost
* Purpose: init remote host info
* Input  : remoteHost  - remote host variable
*          defaultPort - default port
* Output : remoteHost - remote host
* Return : -
* Notes  : -
\***********************************************************************/

void Remote_initHost(RemoteHost *remoteHost, uint defaultPort);

/***********************************************************************\
* Name   : Remote_doneHost
* Purpose: done remote host info
* Input  : remoteHost - remote host
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Remote_doneHost(RemoteHost *remoteHost);

/***********************************************************************\
* Name   : Remote_copyHost
* Purpose: copy remote host info
* Input  : toRemoteHost   - remote host variable
*          fromRemoteHost - from remote host
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Remote_copyHost(RemoteHost *toRemoteHost, const RemoteHost *fromRemoteHost);

/***********************************************************************\
* Name   : Remote_duplicateHost
* Purpose: duplicate remote host info
* Input  : toRemoteHost   - remote host variable
*          fromRemoteHost - from remote host
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Remote_duplicateHost(RemoteHost *toRemoteHost, const RemoteHost *fromRemoteHost);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Remote_connect
* Purpose: connect to remote host
* Input  : remoteHost - remote host
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Remote_connect(const RemoteHost *remoteHost);

/***********************************************************************\
* Name   : Remote_isConnected
* Purpose: check if remote host is connected
* Input  : remoteHost - remote host
* Output : -
* Return : TRUE iff connected
* Notes  : -
\***********************************************************************/

bool Remote_isConnected(const RemoteHost *remoteHost);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Remote_executeCommand
* Purpose: execute command on remote host
* Input  : remoteHost - remote host
*          format     - command
*          ...        - optional command arguments
* Output : resultMap - result map
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Remote_executeCommand(const RemoteHost *remoteHost,
                             StringMap        resultMap,
                             const char       *format,
                             ...
                            );

/***********************************************************************\
* Name   : Remote_jobStart
* Purpose: start job on remote host
* Input  : remoteHost                   - remote host
*          name                         - job name
*          jobUUID                      - job UUID
*          scheduleUUID                 - schedule UUID
*          storageName                  - storage name
*          includeEntryList             - include entry list
*          excludePatternList           - exclude pattern list 
*          mountList                    - mount list
*          compressExcludePatternList   - compress exclude list
*          deltaSourceList              - delta source list
*          jobOptions                   - job options
*          archiveType                  - archive type to create
*          scheduleTitle                - schedule title
*          scheduleCustomText           - schedule custom text
*          storageRequestVolumeFunction - 
*          storageRequestVolumeUserData -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Remote_jobStart(const RemoteHost                *remoteHost,
                       ConstString                     name,
                       ConstString                     jobUUID,
                       ConstString                     scheduleUUID,
                       ConstString                     storageName,
                       const EntryList                 *includeEntryList,
                       const PatternList               *excludePatternList,
                       MountList                       *mountList,
                       const PatternList               *compressExcludePatternList,
                       DeltaSourceList                 *deltaSourceList,
                       JobOptions                      *jobOptions,
                       ArchiveTypes                    archiveType,
                       ConstString                     scheduleTitle,
                       ConstString                     scheduleCustomText,
  //                     GetPasswordFunction getPasswordFunction,
  //                     void                            *getPasswordUserData,
  //                     CreateStatusInfoFunction        createStatusInfoFunction,
  //                     void                            *createStatusInfoUserData,
                       StorageRequestVolumeFunction    storageRequestVolumeFunction,
                       void                            *storageRequestVolumeUserData
                      );

/***********************************************************************\
* Name   : Remote_jobAbort
* Purpose: abort job on remote host
* Input  : remoteHost - remote host
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Remote_jobAbort(const RemoteHost *remoteHost,
                       ConstString      jobUUID
                      );

#ifdef __cplusplus
  }
#endif

#endif /* __REMOTE__ */

/* end of file */
