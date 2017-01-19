/***********************************************************************\
*
* $Revision: 4135 $
* $Date: 2015-09-19 11:03:57 +0200 (Sat, 19 Sep 2015) $
* $Author: torsten $
* Contents: Backup ARchiver slave functions
* Systems: all
*
\***********************************************************************/

#ifndef __SLAVE__
#define __SLAVE__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "bar_global.h"
#define SESSION_ID_LENGTH 64      // max. length of session id
typedef byte SessionId[SESSION_ID_LENGTH];

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***********************************************************************\
* Name   : SlaveConnectStatusInfoFunction
* Purpose: slave connect status info call-back
* Input  : isConnected - TRUE iff connected
*          userData    - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*SlaveConnectStatusInfoFunction)(bool isConnected,
                                              void *userData
                                             );

/***************************** Variables *******************************/

// slave info
typedef struct
{
  String       name;                         // name of slave host where job should run
  uint         port;                         // port of slave host where job should run or 0 for default
  bool         forceSSL;                     // force SSL connection to slave hose
  bool         isConnected;
  SocketHandle socketHandle;
  String       line;

  SessionId    sessionId;
  CryptKey     publicKey,secretKey;

  uint         commandId;

  ServerCommandList  commandList;
  ServerResultList   resultList;
SlaveConnectStatusInfoFunction slaveConnectStatusInfoFunction;
void                           *slaveConnectStatusInfoUserData;

  SocketHandle *masterSocketHandle;
} SlaveInfo;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Slave_initAll
* Purpose: initialize slaves
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Slave_initAll(void);

/***********************************************************************\
* Name   : Slave_doneAll
* Purpose: deinitialize slaves
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Slave_doneAll(void);

/***********************************************************************\
* Name   : Slave_initHost
* Purpose: init slave host info
* Input  : slaveInfo   - slave info variable
*          defaultPort - default port
* Output : slaveInfo - slave info
* Return : -
* Notes  : -
\***********************************************************************/

void Slave_initHost(SlaveInfo *slaveInfo, uint defaultPort);

/***********************************************************************\
* Name   : Slave_doneHost
* Purpose: done slave host info
* Input  : slaveInfo - slave info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Slave_doneHost(SlaveInfo *slaveInfo);

/***********************************************************************\
* Name   : Slave_copyHost
* Purpose: copy slave host info
* Input  : toSlaveInfo   - slave host variable
*          fromSlaveInfo - from slave host
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

//void Slave_copyHost(SlaveInfo *toSlaveInfo, const SlaveInfo *fromSlaveInfo);

/***********************************************************************\
* Name   : Slave_duplicateHost
* Purpose: duplicate slave host info
* Input  : toSlaveInfo   - slave host variable
*          fromSlaveInfo - from slave host
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

//void Slave_duplicateHost(SlaveInfo *toSlaveInfo, const SlaveInfo *fromSlaveInfo);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Slave_connect
* Purpose: connect to slave host
* Input  : slaveInfo                      - slave info
*          slaveConnectStatusInfoFunction - status info call back
*                                           function (can be NULL)
*          slaveConnectStatusInfoUserData - user data for status info
*                                           function
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Slave_connect(SlaveInfo                      *slaveInfo,
                     ConstString                    hostName,
                     uint                           hostPort,
                     SlaveConnectStatusInfoFunction slaveConnectStatusInfoFunction,
                     void                           *slaveConnectStatusInfoUserData
                    );

/***********************************************************************\
* Name   : Slave_disconnect
* Purpose: disconnect slave
* Input  : slaveInfo - slave info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Slave_disconnect(const SlaveInfo *slaveInfo);

/***********************************************************************\
* Name   : Slave_isConnected
* Purpose: check if slave host is connected
* Input  : slaveInfo - slave info
* Output : -
* Return : TRUE iff connected
* Notes  : -
\***********************************************************************/

bool Slave_isConnected(const SlaveInfo *slaveInfo);

SocketHandle *Slave_getSocketHandle(const SlaveInfo *slaveInfo);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Slave_executeCommand
* Purpose: execute command on slave host
* Input  : slaveInfo - slave info
*          format    - command
*          ...       - optional command arguments
* Output : resultMap - result map
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Slave_executeCommand(const SlaveInfo *slaveInfo,
                            StringMap       resultMap,
                            const char      *format,
                            ...
                           );

/***********************************************************************\
* Name   : Slave_jobStart
* Purpose: start job on slave host
* Input  : slaveInfo                    - slave info
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

Errors Slave_jobStart(const SlaveInfo                 *slaveInfo,
                      ConstString                     name,
                      ConstString                     jobUUID,
                      ConstString                     scheduleUUID,
                      ConstString                     storageName,
                      const EntryList                 *includeEntryList,
                      const PatternList               *excludePatternList,
                      const MountList                 *mountList,
                      const PatternList               *compressExcludePatternList,
                      const DeltaSourceList           *deltaSourceList,
                      const JobOptions                *jobOptions,
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
* Name   : Slave_jobAbort
* Purpose: abort job on slave host
* Input  : slaveInfo - slave host
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Slave_jobAbort(const SlaveInfo *slaveInfo,
                      ConstString     jobUUID
                     );

#ifdef __cplusplus
  }
#endif

#endif /* __SLAVE__ */

/* end of file */
