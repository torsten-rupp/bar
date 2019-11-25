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

#include "common/global.h"
#include "common/semaphores.h"
#include "common/stringmaps.h"
#include "common/strings.h"

#include "entrylists.h"
#include "storage.h"
#include "server_io.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/* connector states

   NONE -> CONNECTED -> AUTHORIZED -> DISCONNECTED -\
    ^                                               |
    \-----------------------------------------------/

*/
typedef enum
{
  CONNECTOR_STATE_NONE,
  CONNECTOR_STATE_CONNECTED,
  CONNECTOR_STATE_AUTHORIZED,
  CONNECTOR_STATE_DISCONNECTED
} ConnectorStates;

// connector info
typedef struct
{
bool            forceSSL;                // force SSL connection to connector hose
  ConnectorStates state;
  ServerIO        io;
  Thread          thread;

  StorageInfo     storageInfo;
  bool            storageInitFlag;       // TRUE iff storage initialized
  StorageHandle   storageHandle;
  bool            storageOpenFlag;       // TRUE iff storage created and open
} ConnectorInfo;

// command result function callback
typedef ServerIOCommandResultFunction ConnectorCommandResultFunction;

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
  void __Connector_init(const char    *__fileName__,
                        ulong         __lineNb__,
                        ConnectorInfo *connectorInfo
                       );
#endif /* NDEBUG */

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
* Input  : connectorInfo - connector info
*          hostName      - slave host name
*          hostPort      - slave host port
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Connector_connect(ConnectorInfo *connectorInfo,
                         ConstString   hostName,
                         uint          hostPort
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
* Name   : Connector_isAuthorized
* Purpose: check if connector is authorized
* Input  : connectorInfo - connector info
* Output : -
* Return : TRUE iff authorized
* Notes  : -
\***********************************************************************/

INLINE bool Connector_isAuthorized(const ConnectorInfo *connectorInfo);
#if defined(NDEBUG) || defined(__CONNECTOR_IMPLEMENTATION__)
INLINE bool Connector_isAuthorized(const ConnectorInfo *connectorInfo)
{
  assert(connectorInfo != NULL);

  return (connectorInfo->state == CONNECTOR_STATE_AUTHORIZED);
}
#endif /* NDEBUG || __CONNECTOR_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Connector_isConnected
* Purpose: check if connector is connected
* Input  : connectorInfo - connector info
* Output : -
* Return : TRUE iff connected or authorized
* Notes  : -
\***********************************************************************/

INLINE bool Connector_isConnected(const ConnectorInfo *connectorInfo);
#if defined(NDEBUG) || defined(__CONNECTOR_IMPLEMENTATION__)
INLINE bool Connector_isConnected(const ConnectorInfo *connectorInfo)
{
  assert(connectorInfo != NULL);

  return    (connectorInfo->state == CONNECTOR_STATE_CONNECTED)
         || (connectorInfo->state == CONNECTOR_STATE_AUTHORIZED);
}
#endif /* NDEBUG || __CONNECTOR_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Connector_isDisconnected
* Purpose: check if connector is disconnected
* Input  : connectorInfo - connector info
* Output : -
* Return : TRUE iff disconnected
* Notes  : -
\***********************************************************************/

INLINE bool Connector_isDisconnected(const ConnectorInfo *connectorInfo);
#if defined(NDEBUG) || defined(__CONNECTOR_IMPLEMENTATION__)
INLINE bool Connector_isDisconnected(const ConnectorInfo *connectorInfo)
{
  assert(connectorInfo != NULL);

  return (connectorInfo->state == CONNECTOR_STATE_DISCONNECTED);
}
#endif /* NDEBUG || __CONNECTOR_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Connector_authorize
* Purpose: authorize
* Input  : connectorInfo - connector info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Connector_authorize(ConnectorInfo *connectorInfo);

/***********************************************************************\
* Name   : Connector_initStorage
* Purpose: init storage
* Input  : connectorInfo - connector info
*          storageName   - storage name
*          jobOptions    - job options
*          storageFlags  - storage flags
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Connector_initStorage(ConnectorInfo *connectorInfo,
                             ConstString   storageName,
                             JobOptions    *jobOptions,
                             StorageFlags  storageFlags
                            );

/***********************************************************************\
* Name   : Connector_doneStorage
* Purpose: done storage
* Input  : connectorInfo - connector info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Connector_doneStorage(ConnectorInfo *connectorInfo);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Connector_executeCommand
* Purpose: execute command on connector host
* Input  : connectorInfo         - connector info
*          timeout               - timeout [ms] or WAIT_FOREVER
*          commandResultFunction - command result function (can be NULL)
*          commandResultUserData - user data for command result function
*          format                - command
*          ...                   - optional command arguments
* Output : resultMap - result map
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Connector_executeCommand(ConnectorInfo                  *connectorInfo,
                                uint                           debugLevel,
                                long                           timeout,
                                ConnectorCommandResultFunction commandResultFunction,
                                void                           *commandResultUserData,
                                const char                     *format,
                                ...
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

/***********************************************************************\
* Name   : Connector_create
* Purpose: create job on slave host
* Input  : connectorInfo                - connector info
*          jobName                      - job name
*          jobUUID                      - job UUID
*          scheduleUUID                 - schedule UUID
*          storageName                  - storage name
*          includeEntryList             - include entry list
*          excludePatternList           - exclude pattern list
*          jobOptions                   - job options
*          archiveType                  - archive type to create
*          scheduleTitle                - schedule title
*          scheduleCustomText           - schedule custom text
*          storageFlags                 - storage flags; see
*                                         STROAGE_FLAG_...
*          getNamePasswordFunction      - get password call back (can
*                                         be NULL)
*          getNamePasswordUserData      - user data for get password
*                                         call back
*          statusInfoFunction           - status info call back
*                                         function (can be NULL)
*          statusInfoUserData           - user data for status info
*                                         function
*          storageRequestVolumeFunction - request volume call back
*                                         function (can be NULL)
*          storageRequestVolumeUserData - user data for request
*                                         volume
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Connector_create(ConnectorInfo                *connectorInfo,
                        ConstString                  jobName,
                        ConstString                  jobUUID,
                        ConstString                  scheduleUUID,
                        ConstString                  storageName,
                        const EntryList              *includeEntryList,
                        const PatternList            *excludePatternList,
                        const JobOptions             *jobOptions,
                        ArchiveTypes                 archiveType,
                        ConstString                  scheduleTitle,
                        ConstString                  scheduleCustomText,
                        StorageFlags                 storageFlags,
                        GetNamePasswordFunction      getNamePasswordFunction,
                        void                         *getNamePasswordUserData,
                        StatusInfoFunction           statusInfoFunction,
                        void                         *statusInfoUserData,
                        StorageRequestVolumeFunction storageRequestVolumeFunction,
                        void                         *storageRequestVolumeUserData
                       );

#ifdef __cplusplus
  }
#endif

#endif /* __CONNECTOR__ */

/* end of file */
