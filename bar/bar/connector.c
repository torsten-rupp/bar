/***********************************************************************\
*
* $Revision: 4126 $
* $Date: 2015-09-19 10:57:45 +0200 (Sat, 19 Sep 2015) $
* $Author: torsten $
* Contents: Backup ARchiver connector functions
* Systems: all
*
\***********************************************************************/

#define __CONNECTOR_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <pthread.h>
#include <locale.h>
#include <time.h>
#include <signal.h>
#include <assert.h>

#include "global.h"
#include "autofree.h"
#include "lists.h"
#include "strings.h"
#include "semaphores.h"

#include "network.h"
#include "entrylists.h"
#include "patternlists.h"
#include "misc.h"
#include "archive.h"
#include "bar.h"

#include "server.h"

#include "connector.h"

/****************** Conditional compilation switches *******************/

#define CONNECTOR_DEBUG

/***************************** Constants *******************************/
#define SLEEP_TIME_CONNECTOR_THREAD (   10)  // [s]

#define READ_TIMEOUT                ( 5LL*MS_PER_SECOND)
#define CONNECTOR_DEBUG_LEVEL       1
#define CONNECTOR_COMMAND_TIMEOUT   (60LL*MS_PER_SECOND)

/***************************** Datatypes *******************************/

/***********************************************************************\
* Name   : ConnectorCommandFunction
* Purpose: connector command function
* Input  : clientInfo  - client info
*          indexHandle - index handle or NULL
*          id          - command id
*          argumentMap - argument map
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*ConnectorCommandFunction)(ConnectorInfo   *connectorInfo,
                                        IndexHandle     *indexHandle,
                                        uint            id,
                                        const StringMap argumentMap
                                       );

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/
LOCAL void connectorThreadCode(ConnectorInfo *connectorInfo);

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#if 0
/***********************************************************************\
* Name   : findConnector
* Purpose: find connector by name/port
* Input  : connectorInfo - connector info
* Output : -
* Return : connector or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL ConnectorNode *findConnector(const ConnectorInfo *connectorInfo)
{
  ConnectorNode *connectorNode;

  assert(connectorInfo != NULL);
//  assert(Semaphore_isLocked(&connectorList.lock));

  return LIST_FIND(&connectorList,
                   connectorNode,
                      (connectorNode->hostPort == connectorInfo->port)
                   && String_equals(connectorNode->hostName,connectorInfo->name)
                   && (!connectorInfo->forceSSL || connectorNode->sslFlag)
                  );
}

/***********************************************************************\
* Name   : findConnectorBySocket
* Purpose: find connector by socket
* Input  : fd - socket handle
* Output : -
* Return : connector or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL ConnectorNode *findConnectorBySocket(int fd)
{
  ConnectorNode *connectorNode;

  assert(fd != -1);
//  assert(Semaphore_isLocked(&connectorList.lock));

  return LIST_FIND(&connectorList,connectorNode,Network_getSocket(&connectorNode->socketHandle) == fd);
}
#endif

/***********************************************************************\
* Name   : initSession
* Purpose: init session
* Input  : connectorInfo - connector info
*          hostName  - host name
*          hostPort  - host port
*          forceSSL  - TRUE to force SSL
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors doneSession(ConnectorInfo *connectorInfo)
{
  assert(connectorInfo != NULL);
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
}

/***********************************************************************\
* Name   : connectorConnect
* Purpose: connect to connector and get session id/public key
* Input  : connectorInfo - connector info
*          hostName      - host name
*          hostPort      - host port
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors connectorConnect(ConnectorInfo *connectorInfo,
                              ConstString   hostName,
                              uint          hostPort
                             )
{
  SocketHandle socketHandle;
  Errors       error;
  String       id;
  String       n,e;

  assert(connectorInfo != NULL);

  // init variables

  // connect to connector
  error = Network_connect(&socketHandle,
//TODO
//                          forceSSL ? SOCKET_TYPE_TLS : SOCKET_TYPE_PLAIN,
SOCKET_TYPE_PLAIN,
                          hostName,
                          hostPort,
                          NULL,  // loginName
                          NULL,  // password
                          NULL,  // sshPublicKeyData
                          0,     // sshPublicKeyLength
                          NULL,  // sshPrivateKeyData
                          0,     // sshPrivateKeyLength
                          SOCKET_FLAG_NON_BLOCKING|SOCKET_FLAG_NO_DELAY
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // connect network server i/o
  ServerIO_connectNetwork(&connectorInfo->io,
                          &socketHandle,
                          hostName,
                          hostPort
                         );
//fprintf(stderr,"%s, %d: Network_getSocket(&connectorInfo->io.network.socketHandle)=%d\n",__FILE__,__LINE__,Network_getSocket(&connectorInfo->io.network.socketHandle));

  // accept session data
  error = ServerIO_acceptSession(&connectorInfo->io);
  if (error != ERROR_NONE)
  {
    return error;
  }

//TODO
  // start SSL
#if 0
  error = Connector_executeCommand(connectorInfo,
                                   CONNECTOR_DEBUG_LEVEL,
                                   CONNECTOR_COMMAND_TIMEOUT,
                                   NULL,
                                   "START_SSL"
                                  );
  if (error != ERROR_NONE)
  {
    return error;
  }
#endif

  // start connector thread
  if (!Thread_init(&connectorInfo->thread,"BAR connector",globalOptions.niceLevel,connectorThreadCode,connectorInfo))
  {
    HALT_FATAL_ERROR("Cannot initialize connector thread!");
  }

  // free resources

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : connectorDisconnect
* Purpose: disconnect from connector
* Input  : connectorInfo - connector info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void connectorDisconnect(ConnectorInfo *connectorInfo)
{
  assert(connectorInfo != NULL);

fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
  // stop connector thread
  Thread_quit(&connectorInfo->thread);
  if (!Thread_join(&connectorInfo->thread))
  {
    HALT_FATAL_ERROR("Cannot terminate connector thread!");
  }

  ServerIO_disconnect(&connectorInfo->io);
fprintf(stderr,"%s, %d: ServerIO_disconnect\n",__FILE__,__LINE__);
}

//TODO
#if 0
/***********************************************************************\
* Name   : sendConnector
* Purpose: send data to connector
* Input  : connectorInfo - connector info
*          data          - data string
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL uint sendConnector(ConnectorInfo *connectorInfo, ConstString data)
{
//TODO
//  SemaphoreLock semaphoreLock;
  uint          commandId;

  assert(connectorInfo != NULL);
  assert(data != NULL);

  #ifdef CONNECTOR_DEBUG
    fprintf(stderr,"DEBUG: result=%s",String_cString(data));
  #endif /* CONNECTOR_DEBUG */

  // new command id
  commandId = atomicIncrement(&connectorInfo->io.commandId,1);

  // format command

  // send data
//    SEMAPHORE_LOCKED_DO(semaphoreLock,&connectorInfo->network.writeLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    (void)Network_send(&connectorInfo->io.network.socketHandle,String_cString(data),String_length(data));
  }

  return commandId;
}
#endif

/***********************************************************************\
* Name   : Connector_setJobOptionInteger
* Purpose: set job int value
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          value         - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Connector_setJobOptionInteger(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, int value)
{
  assert(connectorInfo != NULL);
  assert(name != NULL);

  return Connector_executeCommand(connectorInfo,CONNECTOR_DEBUG_LEVEL,CONNECTOR_COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%d",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Connector_setJobOptionInteger64
* Purpose: set job int64 value
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          value         - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Connector_setJobOptionInteger64(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, int64 value)
{
  assert(connectorInfo != NULL);
  assert(name != NULL);

  return Connector_executeCommand(connectorInfo,CONNECTOR_DEBUG_LEVEL,CONNECTOR_COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%lld",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Connector_setJobOptionBoolean
* Purpose: set job boolean value
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          value         - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Connector_setJobOptionBoolean(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, bool value)
{
  assert(connectorInfo != NULL);
  assert(name != NULL);

  return Connector_executeCommand(connectorInfo,CONNECTOR_DEBUG_LEVEL,CONNECTOR_COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%y",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Connector_setJobOptionString
* Purpose: set job string value
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          value         - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Connector_setJobOptionString(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, ConstString value)
{
  assert(connectorInfo != NULL);
  assert(name != NULL);

  return Connector_executeCommand(connectorInfo,CONNECTOR_DEBUG_LEVEL,CONNECTOR_COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'S",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Connector_setJobOptionCString
* Purpose: set job c-string value
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          value         - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Connector_setJobOptionCString(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, const char *value)
{
  assert(connectorInfo != NULL);
  assert(name != NULL);

  return Connector_executeCommand(connectorInfo,CONNECTOR_DEBUG_LEVEL,CONNECTOR_COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'s",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Connector_setJobOptionPassword
* Purpose: set job password option
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          password      - password
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Connector_setJobOptionPassword(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, Password *password)
{
  const char *plainPassword;
  Errors     error;

  assert(connectorInfo != NULL);
  assert(name != NULL);

  plainPassword = Password_deploy(password);
  error = Connector_executeCommand(connectorInfo,CONNECTOR_DEBUG_LEVEL,CONNECTOR_COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'s",jobUUID,name,plainPassword);
  Password_undeploy(password,plainPassword);

  return error;
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : connectorCommand_preProcess
* Purpose: pre-process
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            archiveName=<name>
*            time=<n>
*            initialFlag=yes|no
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_preProcess(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
fprintf(stderr,"%s, %d: connectorCommand_preProcess\n",__FILE__,__LINE__);
UNUSED_VARIABLE(indexHandle);
UNUSED_VARIABLE(argumentMap);

  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : connectorCommand_postProcess
* Purpose: post-process
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            archiveName=<name>
*            time=<n>
*            finalFlag=yes|no
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_postProcess(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
fprintf(stderr,"%s, %d: connectorCommand_postProcess\n",__FILE__,__LINE__);
UNUSED_VARIABLE(indexHandle);
UNUSED_VARIABLE(argumentMap);

  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : connectorCommand_storageCreate
* Purpose: create storage
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            archiveName=<name>
*            archiveSize=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_storageCreate(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String archiveName;
  uint64 archiveSize;
  Errors error;

  UNUSED_VARIABLE(indexHandle);

  // get archive name, archive size
  archiveName = String_new();
  if (!StringMap_getString(argumentMap,"archiveName",archiveName,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected archiveName=<name>");
    String_delete(archiveName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"archiveSize",&archiveSize,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected archiveSize=<n>");
    String_delete(archiveName);
    return;
  }

  // create storage
  error = Storage_create(&connectorInfo->storageHandle,
                         &connectorInfo->storageInfo,
                         archiveName,
                         archiveSize
                        );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"create storage fail");
    String_delete(archiveName);
    return;
  }
  connectorInfo->storageOpenFlag = TRUE;

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(archiveName);
}

/***********************************************************************\
* Name   : connectorCommand_storageWrite
* Purpose: write storage
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            offset=<n>
*            length=<n>
*            data=<base64 encoded data>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_storageWrite(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint64 offset;
  uint   length;
  String data;
  void   *buffer;
  Errors error;

  UNUSED_VARIABLE(indexHandle);

  // get offset, length, data
  if (!StringMap_getUInt64(argumentMap,"offset",&offset,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected offset=<n>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"length",&length,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected length=<n>");
    return;
  }
  data = String_new();
  if (!StringMap_getString(argumentMap,"data",data,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected data=<data>");
    String_delete(data);
    return;
  }

  // check if storage is open
  if (!connectorInfo->storageOpenFlag)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_INVALID_STORAGE,"storage not open");
    String_delete(data);
    return;
  }

  // decode data
  buffer = malloc(length);
  if (buffer == NULL)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_INSUFFICIENT_MEMORY,"insufficient memory");
    String_delete(data);
    return;
  }
  if (!Misc_base64Decode(buffer,NULL,data,STRING_BEGIN,length))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_INSUFFICIENT_MEMORY,"decode base64 data fail");
    String_delete(data);
    return;
  }

  // write to storage
  error = Storage_seek(&connectorInfo->storageHandle,offset);
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"write storage fail");
    free(buffer);
    String_delete(data);
    return;
  }
  error = Storage_write(&connectorInfo->storageHandle,buffer,length);
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"write storage fail");
    free(buffer);
    String_delete(data);
    return;
  }

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  free(buffer);
  String_delete(data);
}

/***********************************************************************\
* Name   : connectorCommand_storageClose
* Purpose: close storage
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_storageClose(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint64 archiveSize;

  // get archive size
  archiveSize = Storage_getSize(&connectorInfo->storageHandle);
UNUSED_VARIABLE(archiveSize);
UNUSED_VARIABLE(indexHandle);
UNUSED_VARIABLE(argumentMap);

  // close storage
  if (connectorInfo->storageOpenFlag)
  {
    Storage_close(&connectorInfo->storageHandle);
  }
  connectorInfo->storageOpenFlag = FALSE;

//TODO: index

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : connectorCommand_indexFindUUID
* Purpose: find index UUID
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<text>
*            scheduleUUUID=<text>
*          Result:
*            uuidId=<n>
*            lastExecutedDateTime=<n>
*            lastErrorMessage=<text>
*            executionCount=<n>
*            averageDurationNormal=<n>
*            averageDurationFull=<n>
*            averageDurationIncremental=<n>
*            averageDurationDifferential=<n>
*            averageDurationContinuous=<n>
*            totalEntityCount=<n>
*            totalStorageCount=<n>
*            totalStorageSize=<n>
*            totalEntryCount=<n>
*            totalEntrySize=<n>
\***********************************************************************/

LOCAL void connectorCommand_indexFindUUID(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUUID,MISC_UUID_STRING_LENGTH);
  IndexId      uuidId;
  uint64       lastExecutedDateTime;
  String       lastErrorMessage;
  ulong        executionCount;
  uint64       averageDurationNormal,averageDurationFull,averageDurationIncremental,averageDurationDifferential,averageDurationContinuous;
  ulong        totalEntityCount;
  ulong        totalStorageCount;
  uint64       totalStorageSize;
  ulong        totalEntryCount;
  uint64       totalEntrySize;

  // get jobUUID, scheduleUUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<text>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUUID,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected scheduleUUID=<text>");
    return;
  }

  // find job data
  lastErrorMessage = String_new();

  if (Index_findUUID(indexHandle,
                     jobUUID,
                     scheduleUUUID,
                     &uuidId,
                     &lastExecutedDateTime,
                     lastErrorMessage,
                     &executionCount,
                     &averageDurationNormal,
                     &averageDurationFull,
                     &averageDurationIncremental,
                     &averageDurationDifferential,
                     &averageDurationContinuous,
                     &totalEntityCount,
                     &totalStorageCount,
                     &totalStorageSize,
                     &totalEntryCount,
                     &totalEntrySize
                    )
     )
  {
    ServerIO_sendResult(&connectorInfo->io,
                        id,
                        TRUE,
                        ERROR_NONE,
                        "uuidId=%lld lastExecutedDateTime=%llu lastErrorMessage=%S executionCount=%lu averageDurationNormal=%llu averageDurationFull=%llu averageDurationIncremental=%llu averageDurationDifferential=%llu averageDurationContinuous=%llu totalEntityCount=%lu totalStorageCount=%lu totalStorageSize=%llu totalEntryCount=%lu totalEntrySize=%llu",
                        uuidId,
                        lastExecutedDateTime,
                        lastErrorMessage,
                        executionCount,
                        averageDurationNormal,
                        averageDurationFull,
                        averageDurationIncremental,
                        averageDurationDifferential,
                        averageDurationContinuous,
                        totalEntityCount,
                        totalStorageCount,
                        totalStorageSize,
                        totalEntryCount,
                        totalEntrySize
                       );
  }
  else
  {
    ServerIO_sendResult(&connectorInfo->io,
                        id,
                        TRUE,
                        ERROR_NONE,
                        "uuidId=%lld",
                        INDEX_ID_NONE
                       );
  }

  // free resources
  String_delete(lastErrorMessage);
}

/***********************************************************************\
* Name   : connectorCommand_indexNewUUID
* Purpose: add new index UUID
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<text>
*          Result:
*            uuidId=<n>
\***********************************************************************/

LOCAL void connectorCommand_indexNewUUID(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  Errors       error;
  IndexId      uuidId;

  // get jobUUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<text>");
    return;
  }

  // create new UUID
  error = Index_newUUID(indexHandle,jobUUID,&uuidId);
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"create new UUID fail");
    return;
  }

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"uuidId=%lld",uuidId);
}

/***********************************************************************\
* Name   : connectorCommand_indexNewEntity
* Purpose: create new index entity
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            scheduleUUID=<uuid>
*            archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL
*            createdDateTime=<n>
*            locked=yes|no
*          Result:
*            entityId=<n>
\***********************************************************************/

LOCAL void connectorCommand_indexNewEntity(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes archiveType;
  uint64       createdDateTime;
  bool         locked;
  Errors       error;
  IndexId      entityId;

  // get jobUUID, scheduleUUID, archiveType, createdDateTime, locked
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<text>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected scheduleUUID=<text>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)Archive_parseType,ARCHIVE_TYPE_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    return;
  }
  StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL);
  StringMap_getBool(argumentMap,"locked",&locked,FALSE);

  // create new entity
  error = Index_newEntity(indexHandle,jobUUID,scheduleUUID,archiveType,createdDateTime,locked,&entityId);
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"create new entity fail");
    return;
  }

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"entityId=%lld",entityId);
}

/***********************************************************************\
* Name   : connectorCommand_indexNewStorage
* Purpose: create new index storage
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            entityId=<n>
*            storageName=<name>
*            createdDateTime=<n>
*            size=<n>
*            indexState=<n>
*            indexMode=<n>
*          Result:
*            storageId=<n>
\***********************************************************************/

LOCAL void connectorCommand_indexNewStorage(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId      entityId;
  String       storageName;
  uint64       createdDateTime;
  uint64       size;
  IndexStates  indexState;
  IndexModes   indexMode;
  Errors       error;
  IndexId      storageId;

  // get entityId, storageName, createdDateTime, size, indexMode, indexState
  if (!StringMap_getInt64(argumentMap,"entityId",&entityId,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected entityId=<n>");
    return;
  }
  storageName = String_new();
  if (!StringMap_getString(argumentMap,"storageName",storageName,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageName=<name>");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected createdDateTime=<n>");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected size=<n>");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"indexState",&indexState,(StringMapParseEnumFunction)Index_parseState,INDEX_STATE_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexState=NONE|OK|CREATE|UPDATE_REQUESTED|UPDATE|ERROR");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"indexMode",&indexMode,(StringMapParseEnumFunction)Index_parseMode,INDEX_MODE_MANUAL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexMode=MANUAL|AUTO");
    String_delete(storageName);
    return;
  }

  // create new entity
  error = Index_newStorage(indexHandle,entityId,storageName,createdDateTime,size,indexState,indexMode,&storageId);
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"create new storage fail");
    String_delete(storageName);
    return;
  }

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"storageId=%lld",storageId);

  // free resources
  String_delete(storageName);
}

/***********************************************************************\
* Name   : connectorCommand_indexAddFile
* Purpose: add index file entry
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            storageId=<n>
*            fileName=<name>
*            size=<n>
*            timeLastAccess=<n>
*            timeModified=<n>
*            timeLastChanged=<n>
*            userId=<n>
*            groupId=<n>
*            permission=<n>
*            fragmentOffset=<n>
*            fragmentSize=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexAddFile(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId storageId;
  String  fileName;
  uint64  size;
  uint64  timeLastAccess;
  uint64  timeModified;
  uint64  timeLastChanged;
  uint32  userId;
  uint32  groupId;
  uint32  permission;
  uint64  fragmentOffset;
  uint64  fragmentSize;
  Errors  error;

  // get storageId, fileName, size, timeLastAccess, timeModified, timeLastChanged, userId, groupId, permission, fragmentOffset, fragmentSize
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }
  fileName = String_new();
  if (!StringMap_getString(argumentMap,"fileName",fileName,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fileName=<name>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected size=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastAccess=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeModified=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastChanged=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected userId=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected groupId=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected permission=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentOffset",&fragmentOffset,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fragmentOffset=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentSize",&fragmentSize,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fragmentSize=<n>");
    String_delete(fileName);
    return;
  }

  // add index file entry
  error = Index_addFile(indexHandle,
                        storageId,
                        fileName,
                        size,
                        timeLastAccess,
                        timeModified,
                        timeLastChanged,
                        userId,
                        groupId,
                        permission,
                        fragmentOffset,
                        fragmentSize
                       );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"create new storage fail");
    String_delete(fileName);
    return;
  }

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(fileName);
}

/***********************************************************************\
* Name   : connectorCommand_indexAddImage
* Purpose: add index file entry
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            storageId=<n>
*            fileName=<name>
*            size=<n>
*            timeLastAccess=<n>
*            timeModified=<n>
*            timeLastChanged=<n>
*            userId=<n>
*            groupId=<n>
*            permission=<n>
*            fragmentOffset=<n>
*            fragmentSize=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexAddImage(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId         storageId;
  String          imageName;
  FileSystemTypes fileSystemType;
  uint64          size;
  ulong           blockSize;
  uint64          blockOffset;
  uint64          blockCount;
  Errors          error;

  // get storageId, imageName, fileSystemType, size, blockSize, blockOffset, blockCount
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }
  imageName = String_new();
  if (!StringMap_getString(argumentMap,"imageName",imageName,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected imageName=<name>");
    String_delete(imageName);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"fileSystemType",&fileSystemType,(StringMapParseEnumFunction)FileSystem_parseFileSystemType,FILE_SYSTEM_TYPE_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fileSystemType=CHARACTER_DEVICE|BLOCK_DEVICE|FIFO|SOCKET|OTHER");
    String_delete(imageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected size=<n>");
    String_delete(imageName);
    return;
  }
  if (!StringMap_getULong(argumentMap,"blockSize",&blockSize,0L))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected blockSize=<n>");
    String_delete(imageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"blockOffset",&blockOffset,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected blockOffset=<n>");
    String_delete(imageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"blockCount",&blockCount,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected blockCount=<n>");
    String_delete(imageName);
    return;
  }

  // add index image entry
  error = Index_addImage(indexHandle,
                         storageId,
                         imageName,
                         fileSystemType,
                         size,
                         blockSize,
                         blockOffset,
                         blockCount
                        );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"create new storage fail");
    String_delete(imageName);
    return;
  }

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(imageName);
}

/***********************************************************************\
* Name   : connectorCommand_indexAddDirectory
* Purpose: add index file entry
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            storageId=<n>
*            fileName=<name>
*            size=<n>
*            timeLastAccess=<n>
*            timeModified=<n>
*            timeLastChanged=<n>
*            userId=<n>
*            groupId=<n>
*            permission=<n>
*            fragmentOffset=<n>
*            fragmentSize=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexAddDirectory(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId storageId;
  String  directoryName;
  uint64  timeLastAccess;
  uint64  timeModified;
  uint64  timeLastChanged;
  uint32  userId;
  uint32  groupId;
  uint32  permission;
  Errors  error;

  // get storageId, directoryName, timeLastAccess, timeModified, timeLastChanged, userId, groupId, permission
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }
  directoryName = String_new();
  if (!StringMap_getString(argumentMap,"directoryName",directoryName,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected directoryName=<name>");
    String_delete(directoryName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastAccess=<n>");
    String_delete(directoryName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeModified=<n>");
    String_delete(directoryName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastChanged=<n>");
    String_delete(directoryName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected userId=<n>");
    String_delete(directoryName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected groupId=<n>");
    String_delete(directoryName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected permission=<n>");
    String_delete(directoryName);
    return;
  }

  // add index directory entry
  error = Index_addDirectory(indexHandle,
                             storageId,
                             directoryName,
                             timeLastAccess,
                             timeModified,
                             timeLastChanged,
                             userId,
                             groupId,
                             permission
                            );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"create new storage fail");
    String_delete(directoryName);
    return;
  }

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(directoryName);
}

/***********************************************************************\
* Name   : connectorCommand_indexAddLink
* Purpose: add index file entry
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            storageId=<n>
*            linkName=<name>
*            destinationName=<name>
*            size=<n>
*            timeLastAccess=<n>
*            timeModified=<n>
*            timeLastChanged=<n>
*            userId=<n>
*            groupId=<n>
*            permission=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexAddLink(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId storageId;
  String  linkName;
  String  destinationName;
  uint64  timeLastAccess;
  uint64  timeModified;
  uint64  timeLastChanged;
  uint32  userId;
  uint32  groupId;
  uint32  permission;
  Errors  error;

  // get storageId, linkName, destinationName, timeLastAccess, timeModified, timeLastChanged, userId, groupId, permission
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }
  linkName = String_new();
  if (!StringMap_getString(argumentMap,"linkName",linkName,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected linkName=<name>");
    String_delete(linkName);
    return;
  }
  destinationName = String_new();
  if (!StringMap_getString(argumentMap,"destinationName",destinationName,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected destinationName=<name>");
    String_delete(destinationName);
    String_delete(linkName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastAccess=<n>");
    String_delete(linkName);
    String_delete(destinationName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeModified=<n>");
    String_delete(destinationName);
    String_delete(linkName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastChanged=<n>");
    String_delete(destinationName);
    String_delete(linkName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected userId=<n>");
    String_delete(destinationName);
    String_delete(linkName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected groupId=<n>");
    String_delete(destinationName);
    String_delete(linkName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected permission=<n>");
    String_delete(destinationName);
    String_delete(linkName);
    return;
  }

  // add index link entry
  error = Index_addLink(indexHandle,
                        storageId,
                        linkName,
                        destinationName,
                        timeLastAccess,
                        timeModified,
                        timeLastChanged,
                        userId,
                        groupId,
                        permission
                       );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"create new storage fail");
    String_delete(destinationName);
    String_delete(linkName);
    return;
  }

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(destinationName);
  String_delete(linkName);
}

/***********************************************************************\
* Name   : connectorCommand_indexAddHardlink
* Purpose: add index file entry
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            storageId=<n>
*            fileName=<name>
*            size=<n>
*            timeLastAccess=<n>
*            timeModified=<n>
*            timeLastChanged=<n>
*            userId=<n>
*            groupId=<n>
*            permission=<n>
*            fragmentOffset=<n>
*            fragmentSize=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexAddHardlink(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId storageId;
  String  fileName;
  uint64  size;
  uint64  timeLastAccess;
  uint64  timeModified;
  uint64  timeLastChanged;
  uint32  userId;
  uint32  groupId;
  uint32  permission;
  uint64  fragmentOffset;
  uint64  fragmentSize;
  Errors  error;

  // get storageId, fileName, size, timeLastAccess, timeModified, timeLastChanged, userId, groupId, permission, fragmentOffset, fragmentSize
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }
  fileName = String_new();
  if (!StringMap_getString(argumentMap,"fileName",fileName,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fileName=<name>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected size=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastAccess=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeModified=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastChanged=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected userId=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected groupId=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected permission=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentOffset",&fragmentOffset,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fragmentOffset=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentSize",&fragmentSize,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fragmentSize=<n>");
    String_delete(fileName);
    return;
  }

  // add index hardlink entry
  error = Index_addHardlink(indexHandle,
                            storageId,
                            fileName,
                            size,
                            timeLastAccess,
                            timeModified,
                            timeLastChanged,
                            userId,
                            groupId,
                            permission,
                            fragmentOffset,
                            fragmentSize
                           );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"create new storage fail");
    String_delete(fileName);
    return;
  }

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(fileName);
}

/***********************************************************************\
* Name   : connectorCommand_indexAddSpecial
* Purpose: add index file entry
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            storageId=<n>
*            fileName=<name>
*            size=<n>
*            timeLastAccess=<n>
*            timeModified=<n>
*            timeLastChanged=<n>
*            userId=<n>
*            groupId=<n>
*            permission=<n>
*            fragmentOffset=<n>
*            fragmentSize=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexAddSpecial(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId          storageId;
  String           name;
  FileSpecialTypes specialType;
  uint64           timeLastAccess;
  uint64           timeModified;
  uint64           timeLastChanged;
  uint32           userId;
  uint32           groupId;
  uint32           permission;
  uint64           fragmentOffset;
  uint64           fragmentSize;
  Errors           error;

  // get storageId, name, specialType, timeLastAccess, timeModified, timeLastChanged, userId, groupId, permission, fragmentOffset, fragmentSize
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"specialType",&specialType,(StringMapParseEnumFunction)File_parseFileSpecialType,FILE_SPECIAL_TYPE_OTHER))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected specialType=CHARACTER_DEVICE|BLOCK_DEVICE|FIFO|SOCKET|OTHER");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastAccess=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeModified=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastChanged=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected userId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected groupId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected permission=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentOffset",&fragmentOffset,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fragmentOffset=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentSize",&fragmentSize,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fragmentSize=<n>");
    String_delete(name);
    return;
  }

  // add index special entry
  error = Index_addSpecial(indexHandle,
                           storageId,
                           name,
                           specialType,
                           timeLastAccess,
                           timeModified,
                           timeLastChanged,
                           userId,
                           groupId,
                           permission,
                           fragmentOffset,
                           fragmentSize
                          );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"create new storage fail");
    String_delete(name);
    return;
  }

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : connectorCommand_indexSetState
* Purpose: set index state
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            indexId=<n>
*            indexState=<state>
*            lastCheckedDateTime=<n>
*            errorMessage=<text>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexSetState(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId     indexId;
  IndexStates indexState;
  uint64      lastCheckedDateTime;
  String      errorMessage;
  Errors      error;

  // get indexId, indexState, lastCheckedDateTime, errorMessage
  if (!StringMap_getInt64(argumentMap,"indexId",&indexId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexId=<n>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"indexState",&indexState,(StringMapParseEnumFunction)Index_parseState,INDEX_STATE_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexState=NONE|OK|CREATE|UPDATE_REQUESTED|UPDATE|ERROR");
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"lastCheckedDateTime",&lastCheckedDateTime,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected lastCheckedDateTime=<n>");
    return;
  }
  errorMessage = String_new();
  if (!StringMap_getString(argumentMap,"errorMessage",errorMessage,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected errorMessage=<name>");
    String_delete(errorMessage);
    return;
  }

  // set state
  error = Index_setState(indexHandle,
                         indexId,
                         indexState,
                         lastCheckedDateTime,
                         "%s",
                         String_cString(errorMessage)
                        );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"set state fail");
    String_delete(errorMessage);
    return;
  }

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(errorMessage);
}

/***********************************************************************\
* Name   : connectorCommand_indexStorageUpdate
* Purpose: update storage
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            storageId=<n>
*            storageName=<text>
*            storageSize=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexStorageUpdate(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId storageId;
  String  storageName;
  uint64  storageSize;
  Errors  error;

  // get storageId, storageName, storageSize
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }
  storageName = String_new();
  if (!StringMap_getString(argumentMap,"storageName",storageName,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageName=<name>");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"storageSize",&storageSize,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageSize=<n>");
    String_delete(storageName);
    return;
  }

  // update storage
  error = Index_storageUpdate(indexHandle,
                              storageId,
                              storageName,
                              storageSize
                             );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"update storage fail");
    String_delete(storageName);
    return;
  }

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(storageName);
}

/***********************************************************************\
* Name   : connectorCommand_indexUpdateStorageInfos
* Purpose: update storage infos
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            storageId=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexUpdateStorageInfos(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId storageId;
  Errors  error;

  // get storageId
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }

  // update storage infos
  error = Index_updateStorageInfos(indexHandle,
                                   storageId
                                  );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"update storage infos fail");
    return;
  }

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : connectorCommand_indexNewHistory
* Purpose: new index history entry
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            scheduleUUID=<uuid>
*            hostName=<name>
*            archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL
*            createdDateTime=<n>
*            errorMessage=<text>
*            duration=<n>
*            totalEntryCount=<n>
*            totalEntrySize=<n>
*            skippedEntryCount=<n>
*            skippedEntrySize=<n>
*            errorEntryCount=<n>
*            errorEntrySize=<n>
*          Result:
*            errorMessage=<text>
\***********************************************************************/

LOCAL void connectorCommand_indexNewHistory(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String       hostName;
  ArchiveTypes archiveType;
  uint64       createdDateTime;
  String       errorMessage;
  uint64       duration;
  ulong        totalEntryCount;
  uint64       totalEntrySize;
  ulong        skippedEntryCount;
  uint64       skippedEntrySize;
  ulong        errorEntryCount;
  uint64       errorEntrySize;
  Errors       error;
  IndexId      historyId;

  // get jobUUID, scheduleUUID, hostName, archiveType, createdDateTime, errorMessage, duration, totalEntryCount, totalEntrySize, skippedEntryCount, skippedEntrySize, errorEntryCount, errorEntrySize
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<text>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected scheduleUUID=<text>");
    return;
  }
  hostName = String_new();
  if (!StringMap_getString(argumentMap,"hostName",hostName,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected hostName=<text>");
    String_delete(hostName);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)Archive_parseType,ARCHIVE_TYPE_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected createdDateTime=<n>");
    String_delete(hostName);
    return;
  }
  errorMessage = String_new();
  if (!StringMap_getString(argumentMap,"errorMessage",errorMessage,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected hostName=<text>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"duration",&duration,0L))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected duration=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getULong(argumentMap,"totalEntryCount",&totalEntryCount,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected totalEntryCount=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"totalEntrySize",&totalEntrySize,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected totalEntrySize=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getULong(argumentMap,"skippedEntryCount",&skippedEntryCount,0L))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected skippedEntryCount=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"skippedEntrySize",&skippedEntrySize,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected skippedEntrySize=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getULong(argumentMap,"errorEntryCount",&errorEntryCount,0L))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected errorEntryCount=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"errorEntrySize",&errorEntrySize,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected errorEntrySize=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }

  // add index history entry
  error = Index_newHistory(indexHandle,
                           jobUUID,
                           scheduleUUID,
                           hostName,
                           archiveType,
                           createdDateTime,
                           String_cString(errorMessage),
                           duration,
                           totalEntryCount,
                           totalEntrySize,
                           skippedEntryCount,
                           skippedEntrySize,
                           errorEntryCount,
                           errorEntrySize,
                           &historyId
                          );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"create new history entry");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"historyId=%lld",historyId);

  // free resources
  String_delete(errorMessage);
  String_delete(hostName);
}

// server commands
const struct
{
  const char           *name;
  ConnectorCommandFunction connectorCommandFunction;
}
CONNECTOR_COMMANDS[] =
{
  { "PREPROCESS",                connectorCommand_preProcess              },
  { "POSTPROCESS",               connectorCommand_postProcess             },

  { "STORAGE_CREATE",            connectorCommand_storageCreate           },
  { "STORAGE_WRITE",             connectorCommand_storageWrite            },
  { "STORAGE_CLOSE",             connectorCommand_storageClose            },

  { "INDEX_FIND_UUID",           connectorCommand_indexFindUUID           },
  { "INDEX_NEW_UUID",            connectorCommand_indexNewUUID            },
  { "INDEX_NEW_ENTITY",          connectorCommand_indexNewEntity          },
  { "INDEX_NEW_STORAGE",         connectorCommand_indexNewStorage         },
  { "INDEX_ADD_FILE",            connectorCommand_indexAddFile            },
  { "INDEX_ADD_IMAGE",           connectorCommand_indexAddImage           },
  { "INDEX_ADD_DIRECTORY",       connectorCommand_indexAddDirectory       },
  { "INDEX_ADD_LINK",            connectorCommand_indexAddLink            },
  { "INDEX_ADD_HARDLINK",        connectorCommand_indexAddHardlink        },
  { "INDEX_ADD_SPECIAL",         connectorCommand_indexAddSpecial         },

  { "INDEX_SET_STATE",           connectorCommand_indexSetState           },
  { "INDEX_STORAGE_UPDATE",      connectorCommand_indexStorageUpdate      },
  { "INDEX_UPDATE_STORAGE_INFOS",connectorCommand_indexUpdateStorageInfos },


  { "INDEX_NEW_HISTORY",         connectorCommand_indexNewHistory         },
};

/***********************************************************************\
* Name   : findConnectorCommand
* Purpose: find connector command
* Input  : name - command name
* Output : connectorCommandFunction - connector command function
* Return : TRUE if command found, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool findConnectorCommand(ConstString              name,
                                ConnectorCommandFunction *connectorCommandFunction
                               )
{
  uint i;

  assert(name != NULL);
  assert(connectorCommandFunction != NULL);

  // find command by name
  i = 0;
  while ((i < SIZE_OF_ARRAY(CONNECTOR_COMMANDS)) && !String_equalsCString(name,CONNECTOR_COMMANDS[i].name))
  {
    i++;
  }
  if (i >= SIZE_OF_ARRAY(CONNECTOR_COMMANDS))
  {
    return FALSE;
  }
  (*connectorCommandFunction) = CONNECTOR_COMMANDS[i].connectorCommandFunction;

  return TRUE;
}

/***********************************************************************\
* Name   : connectorThreadCode
* Purpose: connector thread code
* Input  : connectorInfo - connector info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void connectorThreadCode(ConnectorInfo *connectorInfo)
{
  #define TIMEOUT (5*MS_PER_SECOND)

  String               name;
  StringMap            argumentMap;
  sigset_t             signalMask;
  IndexHandle          *indexHandle;
  struct pollfd        pollfds[1];
  struct timespec      pollTimeout;
  int                  n;
  uint                 id;
  ConnectorCommandFunction connectorCommandFunction;

  // init variables
  name        = String_new();
  argumentMap = StringMap_new();

  // Note: ignore SIGALRM in ppoll()
  sigemptyset(&signalMask);
  sigaddset(&signalMask,SIGALRM);

  // init index
  indexHandle = Index_open(NULL,INDEX_TIMEOUT);

  // process client requests
  while (!Thread_isQuit(&connectorInfo->thread))
  {
//fprintf(stderr,"%s, %d: connector thread wiat command\n",__FILE__,__LINE__);
    // wait for disconnect, command, or result
    pollfds[0].fd     = Network_getSocket(&connectorInfo->io.network.socketHandle);
    pollfds[0].events = POLLIN|POLLERR|POLLNVAL;
    pollTimeout.tv_sec  = (long)(TIMEOUT /MS_PER_SECOND);
    pollTimeout.tv_nsec = (long)((TIMEOUT%MS_PER_SECOND)*1000LL);
    n = ppoll(pollfds,1,&pollTimeout,&signalMask);
    if      (n < 0)
    {
fprintf(stderr,"%s, %d: poll fail\n",__FILE__,__LINE__);
//connectorDisconnect(connectorInfo);
break;
    }
    else if (n > 0)
    {
      if      ((pollfds[0].revents & POLLIN) != 0)
      {
        if (ServerIO_receiveData(&connectorInfo->io))
        {
          while (ServerIO_getCommand(&connectorInfo->io,
                                     &id,
                                     name,
                                     argumentMap
                                    )
                )
          {
            // find command
            #ifdef CONNECTOR_DEBUG
//TODO: enable
//              fprintf(stderr,"DEBUG: received command '%s'\n",String_cString(name));
            #endif
            if (!findConnectorCommand(name,&connectorCommandFunction))
            {
              ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_PARSING,"unknown command '%S'",name);
              continue;
            }
            assert(connectorCommandFunction != NULL);

            // process command
            connectorCommandFunction(connectorInfo,indexHandle,id,argumentMap);
          }
        }
        else
        {
fprintf(stderr,"%s, %d: disc\n",__FILE__,__LINE__);
//connectorDisconnect(connectorInfo);
          break;
        }
      }
      else if ((pollfds[0].revents & (POLLERR|POLLNVAL)) != 0)
      {
fprintf(stderr,"%s, %d: error/disc\n",__FILE__,__LINE__);
//connectorDisconnect(connectorInfo);
        break;
      }
    }
  }

  // done index
  Index_close(indexHandle);

  // free resources
  StringMap_delete(argumentMap);
  String_delete(name);
}

/***********************************************************************\
* Name   : Connector_vexecuteCommand
* Purpose: execute command on connector host
* Input  : connectorInfo - connector info
*          timeout       - timeout [ms] or WAIT_FOREVER
*          resultMap     - result map variable (can be NULL)
*          format        - command
*          arguments     - arguments
* Output : resultMap - result map
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Connector_vexecuteCommand(ConnectorInfo *connectorInfo,
                                       uint          debugLevel,
                                       long          timeout,
                                       StringMap     resultMap,
                                       const char    *format,
                                       va_list       arguments
                                      )
{
  Errors error;

  assert(connectorInfo != NULL);

//TODO
  // init variables

  error = ServerIO_vexecuteCommand(&connectorInfo->io,
                                   debugLevel,
                                   timeout,
                                   resultMap,
                                   format,
                                   arguments
                                  );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // free resources

  return ERROR_NONE;
}
// ----------------------------------------------------------------------

Errors Connector_initAll(void)
{
  // init variables

  return ERROR_NONE;
}

void Connector_doneAll(void)
{
}

#ifdef NDEBUG
  void Connector_init(ConnectorInfo *connectorInfo)
#else /* not NDEBUG */
  void __Connector_init(const char       *__fileName__,
                        ulong            __lineNb__,
                        ConnectorInfo *connectorInfo
                       )
#endif /* NDEBUG */
{
  assert(connectorInfo != NULL);

//TODO: remove
//  connectorInfo->forceSSL                           = forceSSL;
  ServerIO_init(&connectorInfo->io);
  connectorInfo->storageOpenFlag                    = FALSE;
  connectorInfo->connectorConnectStatusInfoFunction = NULL;
  connectorInfo->connectorConnectStatusInfoUserData = NULL;

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(connectorInfo,sizeof(ConnectorInfo));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,connectorInfo,sizeof(ConnectorInfo));
  #endif /* NDEBUG */
}

void Connector_duplicate(ConnectorInfo *connectorInfo, const ConnectorInfo *fromConnectorInfo)
{
  assert(connectorInfo != NULL);
  assert(fromConnectorInfo != NULL);

  Connector_init(connectorInfo);

  connectorInfo->connectorConnectStatusInfoFunction = fromConnectorInfo->connectorConnectStatusInfoFunction;
  connectorInfo->connectorConnectStatusInfoUserData = fromConnectorInfo->connectorConnectStatusInfoUserData;
}

void Connector_done(ConnectorInfo *connectorInfo)
{
  assert(connectorInfo != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(connectorInfo,sizeof(ConnectorInfo));

  if (connectorInfo->storageOpenFlag)
  {
    Storage_close(&connectorInfo->storageHandle);
  }
  ServerIO_done(&connectorInfo->io);
}

Errors Connector_connect(ConnectorInfo                      *connectorInfo,
                         ConstString                        hostName,
                         uint                               hostPort,
                         ConnectorConnectStatusInfoFunction connectorConnectStatusInfoFunction,
                         void                               *connectorConnectStatusInfoUserData
                        )
{
  AutoFreeList     autoFreeList;
//  SessionId        sessionId;
  Errors           error;
//  IndexHandle      *indexHandle;

  assert(connectorInfo != NULL);
  assert(hostName != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  // init variables
  AutoFree_init(&autoFreeList);

  // open index
//  indexHandle = Index_open(NULL,INDEX_TIMEOUT);
//  AUTOFREE_ADD(&autoFreeList,indexHandle,{ Index_close(indexHandle); });

  // connect connector, get session id/public key
  error = connectorConnect(connectorInfo,
                           hostName,
                           hostPort
                          );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,connectorInfo,{ connectorDisconnect(connectorInfo); });

  // init status callback
  connectorInfo->connectorConnectStatusInfoFunction = connectorConnectStatusInfoFunction;
  connectorInfo->connectorConnectStatusInfoUserData = connectorConnectStatusInfoUserData;

  printInfo(2,"Connected connector '%s:%d'\n",String_cString(hostName),hostPort);

  // free resources
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

void Connector_disconnect(ConnectorInfo *connectorInfo)
{
  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  connectorDisconnect(connectorInfo);
}

Errors Connector_authorize(ConnectorInfo *connectorInfo)
{
  SocketHandle socketHandle;
  Errors       error;
  String       hostName;
  String       encryptedUUID;
  String       n,e;
  uint         i;
  byte         buffer[MISC_UUID_STRING_LENGTH];
  uint         bufferLength;
String string;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  // init variables
  hostName      = String_new();
  encryptedUUID = String_new();
  n             = String_new();
  e             = String_new();
string=String_new();


  // get modules/exponent from public key
//  Crypt_getPublicKeyModulusExponent(&connectorInfo->io.publicKey,e,n);

  // get host name/encrypted UUID for authorization
  hostName = Network_getHostName(String_new());
  error = ServerIO_encryptData(&connectorInfo->io,
                               SERVER_IO_ENCRYPT_TYPE_RSA,
                               String_cString(uuid),
                               String_length(uuid),
                               encryptedUUID
                              );
  if (error != ERROR_NONE)
  {
    String_delete(e);
    String_delete(n);
    String_delete(encryptedUUID);
    String_delete(hostName);
    return error;
  }
fprintf(stderr,"%s, %d: uuid=%s encryptedUUID=%s\n",__FILE__,__LINE__,String_cString(uuid),String_cString(encryptedUUID));
//assert(ServerIO_decryptString(&connectorInfo->io,string,SERVER_IO_ENCRYPT_TYPE_RSA,encryptedUUID)==ERROR_NONE); fprintf(stderr,"%s, %d: dectecryp encryptedUUID: %s\n",__FILE__,__LINE__,String_cString(string));

  // authorize with UUID
  error = Connector_executeCommand(connectorInfo,
                                   CONNECTOR_DEBUG_LEVEL,
                                   CONNECTOR_COMMAND_TIMEOUT,
                                   NULL,  // resultMap
                                   "AUTHORIZE encryptType=RSA name=%'S encryptedUUID=%'S",
//                                   "AUTHORIZE encryptType=RSA n=%S e=%S name=%'S encryptedUUID=%'S",
//                                   n,
//                                   e,
                                   hostName,
                                   encryptedUUID
                                  );
  if (error != ERROR_NONE)
  {
    String_delete(e);
    String_delete(n);
    String_delete(encryptedUUID);
    String_delete(hostName);
    return error;
  }

  // free resources
  String_delete(e);
  String_delete(n);
  String_delete(encryptedUUID);
  String_delete(hostName);

  return ERROR_NONE;
}

Errors Connector_initStorage(ConnectorInfo *connectorInfo,
                             ConstString   storageName,
                             JobOptions    *jobOptions
                            )
{
  String           printableStorageName;
  Errors           error;
  StorageSpecifier storageSpecifier;

  assert(connectorInfo != NULL);
  assert(storageName != NULL);
  assert(jobOptions != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  // init variables
  printableStorageName = String_new();

  // parse storage name
  Storage_initSpecifier(&storageSpecifier);
  error = Storage_parseName(&storageSpecifier,storageName);
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize storage '%s' (error: %s)\n",
               String_cString(storageName),
               Error_getText(error)
              );
    Storage_doneSpecifier(&storageSpecifier);
    return error;
  }
  DEBUG_TESTCODE() { Storage_doneSpecifier(&storageSpecifier); return DEBUG_TESTCODE_ERROR(); }

  // get printable storage name
  Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);

  // init storage
  error = Storage_init(&connectorInfo->storageInfo,
                       NULL, // masterIO
                       &storageSpecifier,
                       jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
CALLBACK(NULL,NULL),//                       CALLBACK(updateStorageStatusInfo,connectorInfo),
CALLBACK(NULL,NULL),//                       CALLBACK(getPasswordFunction,getPasswordUserData),
CALLBACK(NULL,NULL)//                       CALLBACK(storageRequestVolumeFunction,storageRequestVolumeUserData)
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize storage '%s' (error: %s)\n",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    return error;
  }
  DEBUG_TESTCODE() { Storage_done(&connectorInfo->storageInfo); Storage_doneSpecifier(&storageSpecifier); return DEBUG_TESTCODE_ERROR(); }

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(printableStorageName);

  return ERROR_NONE;
}

Errors Connector_doneStorage(ConnectorInfo *connectorInfo)
{
  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  // close storage (if open)
  if (connectorInfo->storageOpenFlag)
  {
    Storage_close(&connectorInfo->storageHandle);
  }

  // done storage
  Storage_done(&connectorInfo->storageInfo);
}

Errors Connector_executeCommand(ConnectorInfo *connectorInfo,
                                uint          debugLevel,
                                long          timeout,
                                StringMap     resultMap,
                                const char    *format,
                                ...
                               )
{
  va_list  arguments;
  Errors   error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  va_start(arguments,format);
  error = Connector_vexecuteCommand(connectorInfo,debugLevel,timeout,resultMap,format,arguments);
  va_end(arguments);

  return error;
}

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
//                          ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
//                          void                            *archiveGetCryptPasswordUserData,
//                          CreateStatusInfoFunction        createStatusInfoFunction,
//                          void                            *createStatusInfoUserData,
                          StorageRequestVolumeFunction    storageRequestVolumeFunction,
                          void                            *storageRequestVolumeUserData
                         )
{
  #define SET_OPTION_STRING(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Connector_setJobOptionString(connectorInfo, \
                                                                    jobUUID, \
                                                                    name, \
                                                                    value \
                                                                   ); \
    } \
    while (0)
  #define SET_OPTION_CSTRING(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Connector_setJobOptionCString(connectorInfo, \
                                                                     jobUUID, \
                                                                     name, \
                                                                     value \
                                                                    ); \
    } \
    while (0)
  #define SET_OPTION_PASSWORD(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Connector_setJobOptionPassword(connectorInfo, \
                                                                     jobUUID, \
                                                                     name, \
                                                                     value \
                                                                    ); \
    } \
    while (0)
  #define SET_OPTION_INTEGER(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Connector_setJobOptionInteger(connectorInfo, \
                                                                     jobUUID, \
                                                                     name, \
                                                                     value \
                                                                    ); \
    } \
    while (0)
  #define SET_OPTION_INTEGER64(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Connector_setJobOptionInteger64(connectorInfo, \
                                                                      jobUUID, \
                                                                      name, \
                                                                      value \
                                                                     ); \
    } \
    while (0)
  #define SET_OPTION_BOOLEAN(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Connector_setJobOptionBoolean(connectorInfo, \
                                                                     jobUUID, \
                                                                     name, \
                                                                     value \
                                                                    ); \
    } \
    while (0)

  String          s;
  Errors          error;
  EntryNode       *entryNode;
  const char      *entryTypeText;
  PatternNode     *patternNode;
  MountNode       *mountNode;
  DeltaSourceNode *deltaSourceNode;

UNUSED_VARIABLE(scheduleUUID);
UNUSED_VARIABLE(archiveType);
UNUSED_VARIABLE(scheduleTitle);
UNUSED_VARIABLE(scheduleCustomText);
UNUSED_VARIABLE(storageRequestVolumeFunction);
UNUSED_VARIABLE(storageRequestVolumeUserData);
error = ERROR_STILL_NOT_IMPLEMENTED;

  assert(connectorInfo != NULL);
  assert(jobUUID != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  // init variables
  s = String_new();

  error = ERROR_NONE;

  // create temporary job
  error = Connector_executeCommand(connectorInfo,
                                   CONNECTOR_DEBUG_LEVEL,
                                   CONNECTOR_COMMAND_TIMEOUT,
                                   NULL,
                                   "JOB_NEW name=%'S jobUUID=%S master=%'S",
                                   name,
                                   jobUUID,
                                   Network_getHostName(s)
                                  );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // set options
  error = ERROR_NONE;
  SET_OPTION_STRING   ("archive-name",           storageName);
  SET_OPTION_CSTRING  ("archive-type",           ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,jobOptions->archiveType,NULL));

  SET_OPTION_STRING   ("incremental-list-file",  jobOptions->incrementalListFileName);

  SET_OPTION_INTEGER64("archive-part-size",      jobOptions->archivePartSize);

//  SET_OPTION_INTEGER  ("directory-strip",        jobOptions->directoryStripCount);
//  SET_OPTION_STRING   ("destination",            jobOptions->destination);
//  SET_OPTION_STRING   ("owner",                  jobOptions->owner);

  SET_OPTION_CSTRING  ("pattern-type",           ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,jobOptions->patternType,NULL));

  SET_OPTION_STRING   ("compress-algorithm",     String_format(String_clear(s),
                                                               "%s+%s",
                                                               ConfigValue_selectToString(CONFIG_VALUE_COMPRESS_ALGORITHMS,jobOptions->compressAlgorithms.value.delta,"none"),
                                                               ConfigValue_selectToString(CONFIG_VALUE_COMPRESS_ALGORITHMS,jobOptions->compressAlgorithms.value.byte, "none")
                                                              )
                      );
  SET_OPTION_CSTRING  ("crypt-algorithm",        ConfigValue_selectToString(CONFIG_VALUE_CRYPT_ALGORITHMS,jobOptions->cryptAlgorithms.values[0],NULL));
  SET_OPTION_CSTRING  ("crypt-type",             ConfigValue_selectToString(CONFIG_VALUE_CRYPT_TYPES,jobOptions->cryptType,NULL));
  SET_OPTION_CSTRING  ("crypt-password-mode",    ConfigValue_selectToString(CONFIG_VALUE_PASSWORD_MODES,jobOptions->cryptPasswordMode,NULL));
  SET_OPTION_PASSWORD ("crypt-password",         jobOptions->cryptPassword               );
  SET_OPTION_STRING   ("crypt-public-key",       Misc_base64Encode(s,jobOptions->cryptPublicKey.data,jobOptions->cryptPublicKey.length));

  SET_OPTION_STRING   ("pre-command",            jobOptions->preProcessScript            );
  SET_OPTION_STRING   ("post-command",           jobOptions->postProcessScript           );

  SET_OPTION_STRING   ("ftp-login-name",         jobOptions->ftpServer.loginName         );
  SET_OPTION_PASSWORD ("ftp-password",           jobOptions->ftpServer.password          );

  SET_OPTION_INTEGER  ("ssh-port",               jobOptions->sshServer.port              );
  SET_OPTION_STRING   ("ssh-login-name",         jobOptions->sshServer.loginName         );
  SET_OPTION_PASSWORD ("ssh-password",           jobOptions->sshServer.password          );
  SET_OPTION_STRING   ("ssh-public-key",         Misc_base64Encode(s,jobOptions->sshServer.publicKey.data,jobOptions->sshServer.publicKey.length));
  SET_OPTION_STRING   ("ssh-private-key",        Misc_base64Encode(s,jobOptions->sshServer.privateKey.data,jobOptions->sshServer.privateKey.length));

  SET_OPTION_INTEGER64("max-storage-size",       jobOptions->maxStorageSize);

//TODO
#if 0
  SET_OPTION_INTEGER  ("min-keep",               jobOptions->minKeep);
  SET_OPTION_INTEGER  ("max-keep",               jobOptions->maxKeep);
  SET_OPTION_INTEGER  ("max-age",                jobOptions->maxAge);
#endif

  SET_OPTION_INTEGER64("volume-size",            jobOptions->volumeSize                  );
  SET_OPTION_BOOLEAN  ("ecc",                    jobOptions->errorCorrectionCodesFlag);
  SET_OPTION_BOOLEAN  ("blank",                  jobOptions->blankFlag);

  SET_OPTION_BOOLEAN  ("skip-unreadable",        jobOptions->skipUnreadableFlag);
  SET_OPTION_BOOLEAN  ("raw-images",             jobOptions->rawImagesFlag);
  SET_OPTION_CSTRING  ("archive-file-mode",      ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_FILE_MODES,jobOptions->archiveFileMode,NULL));
  SET_OPTION_BOOLEAN  ("overwrite-files",        jobOptions->overwriteEntriesFlag        );
  SET_OPTION_BOOLEAN  ("wait-first-volume",      jobOptions->waitFirstVolumeFlag         );

  SET_OPTION_STRING   ("comment",                jobOptions->comment                     );

  // set lists
  if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,CONNECTOR_DEBUG_LEVEL,CONNECTOR_COMMAND_TIMEOUT,NULL,"INCLUDE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(includeEntryList,entryNode)
  {
    switch (entryNode->type)
    {
      case ENTRY_TYPE_FILE :   entryTypeText = "FILE";    break;
      case ENTRY_TYPE_IMAGE:   entryTypeText = "IMAGE";   break;
      case ENTRY_TYPE_UNKNOWN:
      default:                 entryTypeText = "UNKNOWN"; break;
    }
    if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                              CONNECTOR_DEBUG_LEVEL,
                                                              CONNECTOR_COMMAND_TIMEOUT,
                                                              NULL,
                                                              "INCLUDE_LIST_ADD jobUUID=%S entryType=%s patternType=%s pattern=%'S",
                                                              jobUUID,
                                                              entryTypeText,
                                                              ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,entryNode->patternType,NULL),
                                                              entryNode->string
                                                             );
  }

  if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,CONNECTOR_DEBUG_LEVEL,CONNECTOR_COMMAND_TIMEOUT,NULL,"EXCLUDE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(excludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                              CONNECTOR_DEBUG_LEVEL,
                                                              CONNECTOR_COMMAND_TIMEOUT,
                                                              NULL,
                                                              "EXCLUDE_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                              jobUUID,
                                                              ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->pattern.type,NULL),
                                                              patternNode->string
                                                             );
  }

  if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,CONNECTOR_DEBUG_LEVEL,CONNECTOR_COMMAND_TIMEOUT,NULL,"MOUNT_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(mountList,mountNode)
  {
    if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                              CONNECTOR_DEBUG_LEVEL,
                                                              CONNECTOR_COMMAND_TIMEOUT,
                                                              NULL,
                                                              "MOUNT_LIST_ADD jobUUID=%S name=%'S alwaysUnmount=%y",
                                                              jobUUID,
                                                              mountNode->name,
                                                              mountNode->alwaysUnmount
                                                             );
  }

  if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,CONNECTOR_DEBUG_LEVEL,CONNECTOR_COMMAND_TIMEOUT,NULL,"EXCLUDE_COMPRESS_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(compressExcludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                              CONNECTOR_DEBUG_LEVEL,
                                                              CONNECTOR_COMMAND_TIMEOUT,
                                                              NULL,
                                                              "EXCLUDE_COMPRESS_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                              jobUUID,
                                                              ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->pattern.type,NULL),
                                                              patternNode->string
                                                             );
  }

  if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,CONNECTOR_DEBUG_LEVEL,CONNECTOR_COMMAND_TIMEOUT,NULL,"SOURCE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(deltaSourceList,deltaSourceNode)
  {
    if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                              CONNECTOR_DEBUG_LEVEL,
                                                              CONNECTOR_COMMAND_TIMEOUT,
                                                              NULL,
                                                              "SOURCE_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                              jobUUID,
                                                              ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,deltaSourceNode->patternType,NULL),
                                                              deltaSourceNode->storageName
                                                             );
  }
  if (error != ERROR_NONE)
  {
    (void)Connector_executeCommand(connectorInfo,CONNECTOR_DEBUG_LEVEL,CONNECTOR_COMMAND_TIMEOUT,NULL,"JOB_DELETE jobUUID=%S",jobUUID);
    String_delete(s);
    return error;
  }
fprintf(stderr,"%s, %d: %d: Connector_jobStart %s\n",__FILE__,__LINE__,error,Error_getText(error));

  // start execute job
  error = Connector_executeCommand(connectorInfo,CONNECTOR_DEBUG_LEVEL,CONNECTOR_COMMAND_TIMEOUT,NULL,"JOB_START jobUUID=%S archiveType=%s dryRun=%y",jobUUID,Archive_archiveTypeToString(archiveType,NULL),FALSE);
  if (error != ERROR_NONE)
  {
    (void)Connector_executeCommand(connectorInfo,CONNECTOR_DEBUG_LEVEL,CONNECTOR_COMMAND_TIMEOUT,NULL,"JOB_DELETE jobUUID=%S",jobUUID);
    String_delete(s);
    return error;
  }

  // free resources
  String_delete(s);

  return ERROR_NONE;

  #undef SET_OPTION_BOOLEAN
  #undef SET_OPTION_INTEGER64
  #undef SET_OPTION_INTEGER
  #undef SET_OPTION_PASSWORD
  #undef SET_OPTION_CSTRING
  #undef SET_OPTION_STRING
}

Errors Connector_jobAbort(ConnectorInfo *connectorInfo,
                          ConstString   jobUUID
                         )
{
  Errors error;

  assert(connectorInfo != NULL);
  assert(jobUUID != NULL);

  error = ERROR_NONE;

  // abort execute job
  error = Connector_executeCommand(connectorInfo,CONNECTOR_DEBUG_LEVEL,CONNECTOR_COMMAND_TIMEOUT,NULL,"JOB_ABORT jobUUID=%S",jobUUID);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // free resources

  return ERROR_NONE;
}

#if 0
Errors Connector_process(ConnectorInfo *connectorInfo,
                         long      timeout
                        )
{
  uint      id;
  String    name;
  StringMap argumentMap;
  Errors    error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  // init variables
  name        = String_new();
  argumentMap = StringMap_new();

  // process commands
  do
  {
fprintf(stderr,"%s, %d: wait command\n",__FILE__,__LINE__);
    if (ServerIO_getCommand(&connectorInfo->io,
                            &id,
                            name,
                            argumentMap
                           )
       )
    {
fprintf(stderr,"%s, %d: ---------------- got command #%u: %s\n",__FILE__,__LINE__,id,String_cString(name));
ServerIO_sendResult(&connectorInfo->io,
                    id,
                    TRUE,
                    ERROR_NONE,
                    ""
                   );
fprintf(stderr,"%s, %d: sent OK result\n",__FILE__,__LINE__);
    }
  }
  while (error == ERROR_NONE);
//  ServerIO_wait(&connectorInfo->io,timeout);

  // free resources
  StringMap_delete(argumentMap);
  String_delete(name);

  return ERROR_NONE;
}
#endif

#ifdef __cplusplus
  }
#endif

/* end of file */
