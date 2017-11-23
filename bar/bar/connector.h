/***********************************************************************\
*
* $Revision: 4135 $
* $Date: 2015-09-19 11:03:57 +0200 (Sat, 19 Sep 2015) $
* $Author: torsten $
* Contents: Backup ARchiver connector functions
* Systems: all
*
\***********************************************************************/

#ifndef __CONNECTOR__
#define __CONNECTOR__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

//#include "bar_global.h"
#include "storage.h"
#include "server_io.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***********************************************************************\
* Name   : ConnectorConnectStatusInfoFunction
* Purpose: connector status info call-back
* Input  : isConnected - TRUE iff connected
*          userData    - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*ConnectorConnectStatusInfoFunction)(bool isConnected,
                                                  void *userData
                                                 );

/***************************** Variables *******************************/

// connector info
typedef struct
{
bool         forceSSL;                     // force SSL connection to connector hose

  ServerIO      io;
  Thread        thread;

  StorageInfo   storageInfo;
  StorageHandle storageHandle;
  bool          storageOpenFlag;          // TRUE iff storage created and open

ConnectorConnectStatusInfoFunction connectorConnectStatusInfoFunction;
void                               *connectorConnectStatusInfoUserData;
} ConnectorInfo;

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define Connector_init(...) __Connector_init(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Connector_initAll
* Purpose: initialize connectors
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Connector_initAll(void);

/***********************************************************************\
* Name   : Connector_doneAll
* Purpose: deinitialize connectors
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Connector_doneAll(void);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Connector_init
* Purpose: init connector info
* Input  : connectorInfo - connector info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Connector_init(ConnectorInfo *connectorInfo);
#else /* not NDEBUG */
  void __Connector_init(const char       *__fileName__,
                        ulong            __lineNb__,
                        ConnectorInfo *connectorInfo
                       );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Connector_duplicate
* Purpose: init duplicate connector info
* Input  : connectorInfo     - connector info
*          fromConnectorInfo - from connector info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Connector_duplicate(ConnectorInfo *connectorInfo, const ConnectorInfo *fromConnectorInfo);

/***********************************************************************\
* Name   : Connector_done
* Purpose: done connector info
* Input  : connectorInfo - connector info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Connector_done(ConnectorInfo *connectorInfo);

/***********************************************************************\
* Name   : Connector_connect
* Purpose: connect to connector host
* Input  : connectorInfo                      - connector info
*          hostName                           - slave host name
*          hostPort                           - slave host port
*          connectorConnectStatusInfoFunction - status info call back
*                                               function (can be NULL)
*          connectorConnectStatusInfoUserData - user data for status info
*                                               function
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Connector_connect(ConnectorInfo                      *connectorInfo,
                         ConstString                        hostName,
                         uint                               hostPort,
                         ConnectorConnectStatusInfoFunction connectorConnectStatusInfoFunction,
                         void                               *connectorConnectStatusInfoUserData
                        );

/***********************************************************************\
* Name   : Connector_disconnect
* Purpose: disconnect connector
* Input  : connectorInfo - connector info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Connector_disconnect(ConnectorInfo *connectorInfo);

/***********************************************************************\
* Name   : Connector_isConnected
* Purpose: check if connector is connected
* Input  : connectorInfo - connector info
* Output : -
* Return : TRUE iff connected
* Notes  : -
\***********************************************************************/

INLINE bool Connector_isConnected(const ConnectorInfo *connectorInfo);
#if defined(NDEBUG) || defined(__CONNECTOR_IMPLEMENTATION__)
INLINE bool Connector_isConnected(const ConnectorInfo *connectorInfo)
{
  assert(connectorInfo != NULL);

//  return ServerIO_isConnected(&connectorInfo->io);
  return !Thread_isQuit(&connectorInfo->thread);
}
#endif /* NDEBUG || __CONNECTOR_IMPLEMENTATION__ */

Errors Connector_authorize(ConnectorInfo *connectorInfo);

Errors Connector_initStorage(ConnectorInfo *connectorInfo,
                             ConstString   storageName,
                             JobOptions    *jobOptions
                            );
Errors Connector_doneStorage(ConnectorInfo *connectorInfo);

/***********************************************************************\
* Name   : Connector_ping
* Purpose: check if slave is alive
* Input  : connectorInfo - connector info
* Output : -
* Return : TRUE iff connected
* Notes  : -
\***********************************************************************/

bool Connector_ping(ConnectorInfo *connectorInfo);

SocketHandle *Connector_getSocketHandle(const ConnectorInfo *connectorInfo);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Connector_waitCommand
* Purpose: wait for command from connector to execute
* Input  : connectorInfo - connector info
*          timeout       - timeout [ms] or WAIT_FOREVER
* Output : id          - command id
*          name        - command name (can be NULL)
*          argumentMap - argument map (can be NULL)
* Return : TRUE iff command received
* Notes  : -
\***********************************************************************/

#if 0
bool Connector_waitCommand(ConnectorInfo *connectorInfo,
                           long          timeout,
                           uint          *id,
                           String        name,
                           StringMap     argumentMap
                          );
#endif

/***********************************************************************\
* Name   : Connector_executeCommand
* Purpose: execute command on connector host
* Input  : connectorInfo - connector info
*          timeout       - timeout [ms] or WAIT_FOREVER
*          resultMap     - result map variable (can be NULL)
*          format        - command
*          ...           - optional command arguments
* Output : resultMap - result map
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Connector_executeCommand(ConnectorInfo *connectorInfo,
                                uint          debugLevel,
                                long          timeout,
                                StringMap     resultMap,
                                const char    *format,
                                ...
                               );

/***********************************************************************\
* Name   : Connector_jobStart
* Purpose: start job on slave host
* Input  : connectorInfo                - connector info
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
*          dryRun                       - TRUE for dry-run (no storage,
*                                         no incremental data, no update
*                                         database)
*          storageRequestVolumeFunction -
*          storageRequestVolumeUserData -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Connector_jobStart(ConnectorInfo                   *connectorInfo,
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
                          bool                            dryRun,
//                          GetPasswordFunction getPasswordFunction,
//                          void                            *getPasswordUserData,
//                          CreateStatusInfoFunction        createStatusInfoFunction,
//                          void                            *createStatusInfoUserData,
                          StorageRequestVolumeFunction    storageRequestVolumeFunction,
                          void                            *storageRequestVolumeUserData
                         );

/***********************************************************************\
* Name   : Connector_jobAbort
* Purpose: abort job on slave host
* Input  : connectorInfo - connector host
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Connector_jobAbort(ConnectorInfo *connectorInfo,
                          ConstString   jobUUID
                         );

//TODO
#if 0
Errors Connector_process(ConnectorInfo *connectorInfo,
                         long          timeout
                        );
#endif

#ifdef __cplusplus
  }
#endif

#endif /* __CONNECTOR__ */

/* end of file */
