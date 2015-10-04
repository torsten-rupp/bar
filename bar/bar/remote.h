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
#include "stringmaps.h"

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

void Remote_initHost(RemoteHost *remoteHost);

void Remote_doneHost(RemoteHost *remoteHost);

void Remote_copyHost(RemoteHost *toRemoteHost, const RemoteHost *fromRemoteHost);

void Remote_duplicateHost(RemoteHost *toRemoteHost, const RemoteHost *fromRemoteHost);

Errors Remote_serverConnect(SocketHandle *socketHandle,
                            ConstString  hostName,
                            uint         hostPort,
                            bool         hostForceSSL
                           );

void Remote_serverDisconnect(SocketHandle *socketHandle);

Errors Remote_executeCommand(const RemoteHost *remoteHost,
                             StringMap        resultMap,
                             const char       *format,
                             ...
                            );

Errors Remote_jobStart(const RemoteHost                *remoteHost,
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
  //                     ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
  //                     void                            *archiveGetCryptPasswordUserData,
  //                     CreateStatusInfoFunction        createStatusInfoFunction,
  //                     void                            *createStatusInfoUserData,
                       StorageRequestVolumeFunction    storageRequestVolumeFunction,
                       void                            *storageRequestVolumeUserData
                      );

Errors Remote_jobAbort(const RemoteHost *remoteHost,
                       ConstString      jobUUID
                      );

#ifdef __cplusplus
  }
#endif

#endif /* __REMOTE__ */

/* end of file */
