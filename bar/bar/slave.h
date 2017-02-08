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

//#include "bar_global.h"
#include "storage.h"
#include "server_io.h"

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
bool         forceSSL;                     // force SSL connection to slave hose

  ServerIO     io;
  Thread       thread;
SlaveConnectStatusInfoFunction slaveConnectStatusInfoFunction;
void                           *slaveConnectStatusInfoUserData;
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

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Slave_init
* Purpose: init slave info
* Input  : slaveInfo - slave info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Slave_init(SlaveInfo *slaveInfo);

/***********************************************************************\
* Name   : Slave_duplicate
* Purpose: init duplicate slave info
* Input  : slaveInfo     - slave info
*          fromSlaveInfo - from slave info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Slave_duplicate(SlaveInfo *slaveInfo, const SlaveInfo *fromSlaveInfo);

/***********************************************************************\
* Name   : Slave_done
* Purpose: done slave info
* Input  : slaveInfo - slave info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Slave_done(SlaveInfo *slaveInfo);

/***********************************************************************\
* Name   : Slave_connect
* Purpose: connect to slave host
* Input  : slaveInfo                      - slave info
*          hostName                       - slave host name
*          hostPort                       - slave host port
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

void Slave_disconnect(SlaveInfo *slaveInfo);

/***********************************************************************\
* Name   : Slave_isConnected
* Purpose: check if slave host is connected
* Input  : slaveInfo - slave info
* Output : -
* Return : TRUE iff connected
* Notes  : -
\***********************************************************************/

INLINE bool Slave_isConnected(const SlaveInfo *slaveInfo);
#if defined(NDEBUG) || defined(__SLAVE_IMPLEMENTATION__)
INLINE bool Slave_isConnected(const SlaveInfo *slaveInfo)
{
  assert(slaveInfo != NULL);

  return ServerIO_isConnected(&slaveInfo->io);
}
#endif /* NDEBUG || __SLAVE_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Slave_isConnected
* Purpose: check if slave host is connected
* Input  : slaveInfo - slave info
* Output : -
* Return : TRUE iff connected
* Notes  : -
\***********************************************************************/

bool Slave_ping(SlaveInfo *slaveInfo);

SocketHandle *Slave_getSocketHandle(const SlaveInfo *slaveInfo);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Slave_waitCommand
* Purpose: wait for command from slave to execute
* Input  : slaveInfo - slave info
*          timeout   - timeout [ms] or WAIT_FOREVER
* Output : id          - command id
*          name        - command name (can be NULL)
*          argumentMap - argument map (can be NULL)
* Return : TRUE iff command received
* Notes  : -
\***********************************************************************/

bool Slave_waitCommand(const SlaveInfo *slaveInfo,
                       long            timeout,
                       uint            *id,
                       String          name,
                       StringMap       argumentMap
                      );

/***********************************************************************\
* Name   : Slave_executeCommand
* Purpose: execute command on slave host
* Input  : slaveInfo - slave info
*          timeout   - timeout [ms] or WAIT_FOREVER
*          resultMap - result map (can be NULL)
*          format    - command
*          ...       - optional command arguments
* Output : resultMap - result map
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Slave_executeCommand(const SlaveInfo *slaveInfo,
                            long            timeout,
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

Errors Slave_process(SlaveInfo *slaveInfo,
                     long      timeout
                    );

#ifdef __cplusplus
  }
#endif

#endif /* __SLAVE__ */

/* end of file */
