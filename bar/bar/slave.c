/***********************************************************************\
*
* $Revision: 4126 $
* $Date: 2015-09-19 10:57:45 +0200 (Sat, 19 Sep 2015) $
* $Author: torsten $
* Contents: Backup ARchiver slave functions
* Systems: all
*
\***********************************************************************/

#define __SLAVE_IMPLEMENTATION__

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

#include "slave.h"

/****************** Conditional compilation switches *******************/

#define SLAVE_DEBUG

/***************************** Constants *******************************/
#define SLEEP_TIME_SLAVE_THREAD (   10)  // [s]

#define READ_TIMEOUT            ( 5LL*MS_PER_SECOND)
#define SLAVE_DEBUG_LEVEL       1
#define SLAVE_COMMAND_TIMEOUT   (10LL*MS_PER_SECOND)

/***************************** Datatypes *******************************/

/***********************************************************************\
* Name   : SlaveCommandFunction
* Purpose: slave command function
* Input  : clientInfo  - client info
*          indexHandle - index handle or NULL
*          id          - command id
*          argumentMap - argument map
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*SlaveCommandFunction)(SlaveInfo       *slaveInfo,
                                    IndexHandle     *indexHandle,
                                    uint            id,
                                    const StringMap argumentMap
                                   );

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/
LOCAL void slaveThreadCode(SlaveInfo *slaveInfo);

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#if 0
/***********************************************************************\
* Name   : findSlave
* Purpose: find slave by name/port
* Input  : slaveInfo - slave info
* Output : -
* Return : slave or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL SlaveNode *findSlave(const SlaveInfo *slaveInfo)
{
  SlaveNode *slaveNode;

  assert(slaveInfo != NULL);
//  assert(Semaphore_isLocked(&slaveList.lock));

  return LIST_FIND(&slaveList,
                   slaveNode,
                      (slaveNode->hostPort == slaveInfo->port)
                   && String_equals(slaveNode->hostName,slaveInfo->name)
                   && (!slaveInfo->forceSSL || slaveNode->sslFlag)
                  );
}

/***********************************************************************\
* Name   : findSlaveBySocket
* Purpose: find slave by socket
* Input  : fd - socket handle
* Output : -
* Return : slave or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL SlaveNode *findSlaveBySocket(int fd)
{
  SlaveNode *slaveNode;

  assert(fd != -1);
//  assert(Semaphore_isLocked(&slaveList.lock));

  return LIST_FIND(&slaveList,slaveNode,Network_getSocket(&slaveNode->socketHandle) == fd);
}
#endif

/***********************************************************************\
* Name   : initSession
* Purpose: init session
* Input  : slaveInfo - slave info
*          hostName  - host name
*          hostPort  - host port
*          forceSSL  - TRUE to force SSL
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors initSession(SlaveInfo       *slaveInfo,
                         const SessionId sessionId
                        )
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

  assert(slaveInfo != NULL);

  // init variables
  hostName      = String_new();
  encryptedUUID = String_new();
  n             = String_new();
  e             = String_new();
string=String_new();

//TODO
  // start SSL
#if 0
  error = Slave_executeCommand(slaveInfo,
                               SLAVE_DEBUG_LEVEL,
                               SLAVE_COMMAND_TIMEOUT,
                               NULL,
                               "START_SSL"
                              );
  if (error != ERROR_NONE)
  {
    String_delete(e);
    String_delete(n);
    String_delete(encryptedUUID);
    String_delete(hostName);
    return error;
  }
#endif

  // get host name, get encrypted UUID for authorization
  hostName = Network_getHostName(String_new());
  error = ServerIO_encryptData(&slaveInfo->io,
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
//assert(ServerIO_decryptString(&slaveInfo->io,string,SERVER_IO_ENCRYPT_TYPE_RSA,encryptedUUID)==ERROR_NONE); fprintf(stderr,"%s, %d: dectecryp encryptedUUID: %s\n",__FILE__,__LINE__,String_cString(string));

  // authorize with UUID
  error = Slave_executeCommand(slaveInfo,
                               SLAVE_DEBUG_LEVEL,
                               SLAVE_COMMAND_TIMEOUT,
                               NULL,
                               "AUTHORIZE encryptType=RSA name=%'S encryptedUUID=%'S n=%S e=%S",
                               hostName,
                               encryptedUUID,
                               n,
                               e
                              );
  if (error != ERROR_NONE)
  {
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
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

/***********************************************************************\
* Name   : initSession
* Purpose: init session
* Input  : slaveInfo - slave info
*          hostName  - host name
*          hostPort  - host port
*          forceSSL  - TRUE to force SSL
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors doneSession(SlaveInfo *slaveInfo)
{
  assert(slaveInfo != NULL);
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
}

/***********************************************************************\
* Name   : slaveConnect
* Purpose: connect to slave
* Input  : slaveInfo - slave info
*          hostName  - host name
*          hostPort  - host port
*          forceSSL  - TRUE to force SSL
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors slaveConnect(SlaveInfo    *slaveInfo,
                          ConstString  hostName,
                          uint         hostPort,
                          SessionId    sessionId
                         )
{
  String       line;
  StringMap    argumentMap;
  SocketHandle socketHandle;
  Errors       error;
  String       id;
  String       n,e;

  assert(slaveInfo != NULL);

  // init variables
  line        = String_new();
  argumentMap = StringMap_new();

fprintf(stderr,"%s, %d: %s %d\n",__FILE__,__LINE__,String_cString(hostName),hostPort);
  // connect to slave
  error = Network_connect(&socketHandle,
//TODO
//                          forceSSL ? SOCKET_TYPE_TLS : SOCKET_TYPE_PLAIN,
SOCKET_TYPE_PLAIN,
                          hostName,
                          hostPort,
                          NULL,  // loginName
                          NULL,  // password
                          NULL,  // sshPublicKeyFileName
                          0,
                          NULL,  // sshPrivateKeyFileName
                          0,
//                          SOCKET_FLAG_NON_BLOCKING|SOCKET_FLAG_NO_DELAY
0
                         );
  if (error != ERROR_NONE)
  {
    StringMap_delete(argumentMap);
    String_delete(line);
    return error;
  }

  // connect network server i/o
  ServerIO_connectNetwork(&slaveInfo->io,
                          hostName,
                          hostPort,
                          socketHandle
                         );
//fprintf(stderr,"%s, %d: Network_getSocket(&slaveInfo->io.network.socketHandle)=%d\n",__FILE__,__LINE__,Network_getSocket(&slaveInfo->io.network.socketHandle));

  // get session data
  error = Network_readLine(&slaveInfo->io.network.socketHandle,line,SLAVE_COMMAND_TIMEOUT);
  if (error != ERROR_NONE)
  {
    StringMap_delete(argumentMap);
    String_delete(line);
    return error;
  }
  if (!String_startsWithCString(line,"SESSION"))
  {
    StringMap_delete(argumentMap);
    String_delete(line);
return ERROR_(UNKNOWN,0);
  }
  if (!StringMap_parse(argumentMap,line,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,7,NULL))
  {
    StringMap_delete(argumentMap);
    String_delete(line);
return ERROR_(UNKNOWN,0);
  }

  id = String_new();
  if (!StringMap_getString(argumentMap,"id",line,NULL))
  {
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
return ERROR_(UNKNOWN,0);
  }
  if (!Misc_hexDecode(sessionId,
                      NULL,
                      line,
                      STRING_BEGIN,
                      sizeof(SessionId)
                     )
     )
  {
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
return ERROR_(UNKNOWN,0);
  }
  n = String_new();
  e = String_new();
  if (!StringMap_getString(argumentMap,"n",n,NULL))
  {
    String_delete(e);
    String_delete(n);
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
return ERROR_(UNKNOWN,0);
  }
  if (!StringMap_getString(argumentMap,"e",e,NULL))
  {
    String_delete(e);
    String_delete(n);
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
return ERROR_(UNKNOWN,0);
  }
  if (!Crypt_setPublicKeyModulusExponent(&slaveInfo->io.publicKey,n,e))
  {
    String_delete(e);
    String_delete(n);
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
return ERROR_(UNKNOWN,0);
  }

  // start slave thread
  if (!Thread_init(&slaveInfo->thread,"BAR slave",globalOptions.niceLevel,slaveThreadCode,slaveInfo))
  {
    HALT_FATAL_ERROR("Cannot initialize slave thread!");
  }

  // free resources
  String_delete(e);
  String_delete(n);
  String_delete(id);
  StringMap_delete(argumentMap);
  String_delete(line);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : slaveDisconnect
* Purpose: disconnect from slave
* Input  : slaveInfo - slave info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void slaveDisconnect(SlaveInfo *slaveInfo)
{
  assert(slaveInfo != NULL);

  ServerIO_disconnect(&slaveInfo->io);
}

//TODO
#if 0
/***********************************************************************\
* Name   : sendSlave
* Purpose: send data to slave
* Input  : slaveInfo - slave info
*          data      - data string
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL uint sendSlave(SlaveInfo *slaveInfo, ConstString data)
{
//TODO
//  SemaphoreLock semaphoreLock;
  uint          commandId;

  assert(slaveInfo != NULL);
  assert(data != NULL);

  #ifdef SLAVE_DEBUG
    fprintf(stderr,"DEBUG: result=%s",String_cString(data));
  #endif /* SLAVE_DEBUG */

  // new command id
  commandId = atomicIncrement(&slaveInfo->io.commandId,1);

  // format command

  // send data
//    SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveInfo->network.writeLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    (void)Network_send(&slaveInfo->io.network.socketHandle,String_cString(data),String_length(data));
  }

  return commandId;
}
#endif

/***********************************************************************\
* Name   : Slave_setJobOptionInteger
* Purpose: set job int value
* Input  : slaveInfo - slave info
*          jobUUID   - job UUID
*          name      - value name
*          value     - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Slave_setJobOptionInteger(SlaveInfo *slaveInfo, ConstString jobUUID, const char *name, int value)
{
  assert(slaveInfo != NULL);
  assert(name != NULL);

  return Slave_executeCommand(slaveInfo,SLAVE_DEBUG_LEVEL,SLAVE_COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%d",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Slave_setJobOptionInteger64
* Purpose: set job int64 value
* Input  : slaveInfo - slave info
*          jobUUID   - job UUID
*          name      - value name
*          value     - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Slave_setJobOptionInteger64(SlaveInfo *slaveInfo, ConstString jobUUID, const char *name, int64 value)
{
  assert(slaveInfo != NULL);
  assert(name != NULL);

  return Slave_executeCommand(slaveInfo,SLAVE_DEBUG_LEVEL,SLAVE_COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%lld",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Slave_setJobOptionBoolean
* Purpose: set job boolean value
* Input  : slaveInfo - slave info
*          jobUUID   - job UUID
*          name      - value name
*          value     - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Slave_setJobOptionBoolean(SlaveInfo *slaveInfo, ConstString jobUUID, const char *name, bool value)
{
  assert(slaveInfo != NULL);
  assert(name != NULL);

  return Slave_executeCommand(slaveInfo,SLAVE_DEBUG_LEVEL,SLAVE_COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%y",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Slave_setJobOptionString
* Purpose: set job string value
* Input  : slaveInfo - slave info
*          jobUUID   - job UUID
*          name      - value name
*          value     - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Slave_setJobOptionString(SlaveInfo *slaveInfo, ConstString jobUUID, const char *name, ConstString value)
{
  assert(slaveInfo != NULL);
  assert(name != NULL);

  return Slave_executeCommand(slaveInfo,SLAVE_DEBUG_LEVEL,SLAVE_COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'S",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Slave_setJobOptionCString
* Purpose: set job c-string value
* Input  : slaveInfo - slave info
*          jobUUID   - job UUID
*          name      - value name
*          value     - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Slave_setJobOptionCString(SlaveInfo *slaveInfo, ConstString jobUUID, const char *name, const char *value)
{
  assert(slaveInfo != NULL);
  assert(name != NULL);

  return Slave_executeCommand(slaveInfo,SLAVE_DEBUG_LEVEL,SLAVE_COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'s",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Slave_setJobOptionPassword
* Purpose: set job password option
* Input  : slaveInfo - slave info
*          jobUUID   - job UUID
*          name      - value name
*          password  - password
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Slave_setJobOptionPassword(SlaveInfo *slaveInfo, ConstString jobUUID, const char *name, Password *password)
{
  const char *plainPassword;
  Errors     error;

  assert(slaveInfo != NULL);
  assert(name != NULL);

  plainPassword = Password_deploy(password);
  error = Slave_executeCommand(slaveInfo,SLAVE_DEBUG_LEVEL,SLAVE_COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'s",jobUUID,name,plainPassword);
  Password_undeploy(password,plainPassword);

  return error;
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : slaveCommand_preProcess
* Purpose: pre-process
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            archiveName=<name>
*            time=<n>
*            initialFlag=yes|no
*          Result:
\***********************************************************************/

LOCAL void slaveCommand_preProcess(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
fprintf(stderr,"%s, %d: slaveCommand_preProcess\n",__FILE__,__LINE__);
UNUSED_VARIABLE(indexHandle);
UNUSED_VARIABLE(argumentMap);

  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : slaveCommand_postProcess
* Purpose: post-process
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            archiveName=<name>
*            time=<n>
*            finalFlag=yes|no
*          Result:
\***********************************************************************/

LOCAL void slaveCommand_postProcess(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
fprintf(stderr,"%s, %d: slaveCommand_postProcess\n",__FILE__,__LINE__);
UNUSED_VARIABLE(indexHandle);
UNUSED_VARIABLE(argumentMap);

  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : slaveCommand_storageCreate
* Purpose: create storage
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            archiveName=<name>
*            archiveSize=<n>
*          Result:
\***********************************************************************/

LOCAL void slaveCommand_storageCreate(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String archiveName;
  uint64 archiveSize;
  Errors error;

  UNUSED_VARIABLE(indexHandle);

  // get archive name, archive size
  archiveName = String_new();
  if (!StringMap_getString(argumentMap,"archiveName",archiveName,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected archiveName=<name>");
    String_delete(archiveName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"archiveSize",&archiveSize,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected archiveSize=<n>");
    String_delete(archiveName);
    return;
  }

  // create storage
  error = Storage_create(&slaveInfo->storageHandle,
                         &slaveInfo->storageInfo,
                         archiveName,
                         archiveSize
                        );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"create storage fail");
    String_delete(archiveName);
    return;
  }
  slaveInfo->storageOpenFlag = TRUE;

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(archiveName);
}

/***********************************************************************\
* Name   : slaveCommand_storageWrite
* Purpose: write storage
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            offset=<n>
*            length=<n>
*            data=<base64 encoded data>
*          Result:
\***********************************************************************/

LOCAL void slaveCommand_storageWrite(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected offset=<n>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"length",&length,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected length=<n>");
    return;
  }
  data = String_new();
  if (!StringMap_getString(argumentMap,"data",data,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected data=<data>");
    String_delete(data);
    return;
  }

  // check if storage is open
  if (!slaveInfo->storageOpenFlag)
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_INVALID_STORAGE,"storage not open");
    String_delete(data);
    return;
  }

  // decode data
  buffer = malloc(length);
  if (buffer == NULL)
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_INSUFFICIENT_MEMORY,"insufficient memory");
    String_delete(data);
    return;
  }
  if (!Misc_base64Decode(buffer,NULL,data,STRING_BEGIN,length))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_INSUFFICIENT_MEMORY,"decode base64 data fail");
    String_delete(data);
    return;
  }

  // write to storage
  error = Storage_seek(&slaveInfo->storageHandle,offset);
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"write storage fail");
    free(buffer);
    String_delete(data);
    return;
  }
  error = Storage_write(&slaveInfo->storageHandle,buffer,length);
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"write storage fail");
    free(buffer);
    String_delete(data);
    return;
  }

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  free(buffer);
  String_delete(data);
}

/***********************************************************************\
* Name   : slaveCommand_storageClose
* Purpose: close storage
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void slaveCommand_storageClose(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint64 archiveSize;

  // get archive size
  archiveSize = Storage_getSize(&slaveInfo->storageHandle);
UNUSED_VARIABLE(archiveSize);
UNUSED_VARIABLE(indexHandle);
UNUSED_VARIABLE(argumentMap);

  // close storage
  if (slaveInfo->storageOpenFlag)
  {
    Storage_close(&slaveInfo->storageHandle);
  }
  slaveInfo->storageOpenFlag = FALSE;

//TODO: index

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : slaveCommand_indexFindUUID
* Purpose: find index UUID
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

LOCAL void slaveCommand_indexFindUUID(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<text>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUUID,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected scheduleUUID=<text>");
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
    ServerIO_sendResult(&slaveInfo->io,
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
    ServerIO_sendResult(&slaveInfo->io,
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
* Name   : slaveCommand_indexNewUUID
* Purpose: add new index UUID
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<text>
*          Result:
*            uuidId=<n>
\***********************************************************************/

LOCAL void slaveCommand_indexNewUUID(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  Errors       error;
  IndexId      uuidId;

  // get jobUUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<text>");
    return;
  }

  // create new UUID
  error = Index_newUUID(indexHandle,jobUUID,&uuidId);
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"create new UUID fail");
    return;
  }

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"uuidId=%lld",uuidId);
}

/***********************************************************************\
* Name   : slaveCommand_indexNewEntity
* Purpose: create new index entity
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

LOCAL void slaveCommand_indexNewEntity(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<text>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected scheduleUUID=<text>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)Archive_parseType,ARCHIVE_TYPE_NONE))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    return;
  }
  StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL);
  StringMap_getBool(argumentMap,"locked",&locked,FALSE);

  // create new entity
  error = Index_newEntity(indexHandle,jobUUID,scheduleUUID,archiveType,createdDateTime,locked,&entityId);
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"create new entity fail");
    return;
  }

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"entityId=%lld",entityId);
}

/***********************************************************************\
* Name   : slaveCommand_indexNewStorage
* Purpose: create new index storage
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

LOCAL void slaveCommand_indexNewStorage(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected entityId=<n>");
    return;
  }
  storageName = String_new();
  if (!StringMap_getString(argumentMap,"storageName",storageName,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageName=<name>");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected createdDateTime=<n>");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected size=<n>");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"indexState",&indexState,(StringMapParseEnumFunction)Index_parseState,INDEX_STATE_NONE))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexState=NONE|OK|CREATE|UPDATE_REQUESTED|UPDATE|ERROR");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"indexMode",&indexMode,(StringMapParseEnumFunction)Index_parseMode,INDEX_MODE_MANUAL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexMode=MANUAL|AUTO");
    String_delete(storageName);
    return;
  }

  // create new entity
  error = Index_newStorage(indexHandle,entityId,storageName,createdDateTime,size,indexState,indexMode,&storageId);
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"create new storage fail");
    String_delete(storageName);
    return;
  }

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"storageId=%lld",storageId);

  // free resources
  String_delete(storageName);
}

/***********************************************************************\
* Name   : slaveCommand_indexAddFile
* Purpose: add index file entry
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

LOCAL void slaveCommand_indexAddFile(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }
  fileName = String_new();
  if (!StringMap_getString(argumentMap,"fileName",fileName,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fileName=<name>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected size=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastAccess=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeModified=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastChanged=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected userId=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected groupId=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected permission=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentOffset",&fragmentOffset,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fragmentOffset=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentSize",&fragmentSize,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fragmentSize=<n>");
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"create new storage fail");
    String_delete(fileName);
    return;
  }

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(fileName);
}

/***********************************************************************\
* Name   : slaveCommand_indexAddImage
* Purpose: add index file entry
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

LOCAL void slaveCommand_indexAddImage(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }
  imageName = String_new();
  if (!StringMap_getString(argumentMap,"imageName",imageName,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected imageName=<name>");
    String_delete(imageName);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"fileSystemType",&fileSystemType,(StringMapParseEnumFunction)FileSystem_parseFileSystemType,FILE_SYSTEM_TYPE_NONE))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fileSystemType=CHARACTER_DEVICE|BLOCK_DEVICE|FIFO|SOCKET|OTHER");
    String_delete(imageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected size=<n>");
    String_delete(imageName);
    return;
  }
  if (!StringMap_getULong(argumentMap,"blockSize",&blockSize,0L))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected blockSize=<n>");
    String_delete(imageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"blockOffset",&blockOffset,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected blockOffset=<n>");
    String_delete(imageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"blockCount",&blockCount,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected blockCount=<n>");
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"create new storage fail");
    String_delete(imageName);
    return;
  }

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(imageName);
}

/***********************************************************************\
* Name   : slaveCommand_indexAddDirectory
* Purpose: add index file entry
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

LOCAL void slaveCommand_indexAddDirectory(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }
  directoryName = String_new();
  if (!StringMap_getString(argumentMap,"directoryName",directoryName,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected directoryName=<name>");
    String_delete(directoryName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastAccess=<n>");
    String_delete(directoryName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeModified=<n>");
    String_delete(directoryName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastChanged=<n>");
    String_delete(directoryName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected userId=<n>");
    String_delete(directoryName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected groupId=<n>");
    String_delete(directoryName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected permission=<n>");
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"create new storage fail");
    String_delete(directoryName);
    return;
  }

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(directoryName);
}

/***********************************************************************\
* Name   : slaveCommand_indexAddLink
* Purpose: add index file entry
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

LOCAL void slaveCommand_indexAddLink(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }
  linkName = String_new();
  if (!StringMap_getString(argumentMap,"linkName",linkName,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected linkName=<name>");
    String_delete(linkName);
    return;
  }
  destinationName = String_new();
  if (!StringMap_getString(argumentMap,"destinationName",destinationName,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected destinationName=<name>");
    String_delete(destinationName);
    String_delete(linkName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastAccess=<n>");
    String_delete(linkName);
    String_delete(destinationName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeModified=<n>");
    String_delete(destinationName);
    String_delete(linkName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastChanged=<n>");
    String_delete(destinationName);
    String_delete(linkName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected userId=<n>");
    String_delete(destinationName);
    String_delete(linkName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected groupId=<n>");
    String_delete(destinationName);
    String_delete(linkName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected permission=<n>");
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"create new storage fail");
    String_delete(destinationName);
    String_delete(linkName);
    return;
  }

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(destinationName);
  String_delete(linkName);
}

/***********************************************************************\
* Name   : slaveCommand_indexAddHardlink
* Purpose: add index file entry
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

LOCAL void slaveCommand_indexAddHardlink(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }
  fileName = String_new();
  if (!StringMap_getString(argumentMap,"fileName",fileName,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fileName=<name>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected size=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastAccess=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeModified=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastChanged=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected userId=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected groupId=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected permission=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentOffset",&fragmentOffset,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fragmentOffset=<n>");
    String_delete(fileName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentSize",&fragmentSize,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fragmentSize=<n>");
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"create new storage fail");
    String_delete(fileName);
    return;
  }

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(fileName);
}

/***********************************************************************\
* Name   : slaveCommand_indexAddSpecial
* Purpose: add index file entry
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

LOCAL void slaveCommand_indexAddSpecial(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"specialType",&specialType,(StringMapParseEnumFunction)File_parseFileSpecialType,FILE_SPECIAL_TYPE_OTHER))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected specialType=CHARACTER_DEVICE|BLOCK_DEVICE|FIFO|SOCKET|OTHER");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastAccess=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeModified=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected timeLastChanged=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected userId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected groupId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected permission=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentOffset",&fragmentOffset,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fragmentOffset=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentSize",&fragmentSize,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected fragmentSize=<n>");
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"create new storage fail");
    String_delete(name);
    return;
  }

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : slaveCommand_indexSetState
* Purpose: set index state
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            indexId=<n>
*            indexState=<state>
*            lastCheckedDateTime=<n>
*            errorMessage=<text>
*          Result:
\***********************************************************************/

LOCAL void slaveCommand_indexSetState(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId     indexId;
  IndexStates indexState;
  uint64      lastCheckedDateTime;
  String      errorMessage;
  Errors      error;

  // get indexId, indexState, lastCheckedDateTime, errorMessage
  if (!StringMap_getInt64(argumentMap,"indexId",&indexId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexId=<n>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"indexState",&indexState,(StringMapParseEnumFunction)Index_parseState,INDEX_STATE_NONE))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexState=NONE|OK|CREATE|UPDATE_REQUESTED|UPDATE|ERROR");
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"lastCheckedDateTime",&lastCheckedDateTime,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected lastCheckedDateTime=<n>");
    return;
  }
  errorMessage = String_new();
  if (!StringMap_getString(argumentMap,"errorMessage",errorMessage,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected errorMessage=<name>");
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"set state fail");
    String_delete(errorMessage);
    return;
  }

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(errorMessage);
}

/***********************************************************************\
* Name   : slaveCommand_indexStorageUpdate
* Purpose: update storage
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            storageId=<n>
*            storageName=<text>
*            storageSize=<n>
*          Result:
\***********************************************************************/

LOCAL void slaveCommand_indexStorageUpdate(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId storageId;
  String  storageName;
  uint64  storageSize;
  Errors  error;

  // get storageId, storageName, storageSize
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }
  storageName = String_new();
  if (!StringMap_getString(argumentMap,"storageName",storageName,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageName=<name>");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"storageSize",&storageSize,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageSize=<n>");
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"update storage fail");
    String_delete(storageName);
    return;
  }

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(storageName);
}

/***********************************************************************\
* Name   : slaveCommand_indexUpdateStorageInfos
* Purpose: update storage infos
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            storageId=<n>
*          Result:
\***********************************************************************/

LOCAL void slaveCommand_indexUpdateStorageInfos(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId storageId;
  Errors  error;

  // get storageId
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected storageId=<n>");
    return;
  }

  // update storage infos
  error = Index_updateStorageInfos(indexHandle,
                                   storageId
                                  );
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"update storage infos fail");
    return;
  }

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : slaveCommand_indexNewHistory
* Purpose: new index history entry
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
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

LOCAL void slaveCommand_indexNewHistory(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<text>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected scheduleUUID=<text>");
    return;
  }
  hostName = String_new();
  if (!StringMap_getString(argumentMap,"hostName",hostName,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected hostName=<text>");
    String_delete(hostName);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)Archive_parseType,ARCHIVE_TYPE_NONE))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected createdDateTime=<n>");
    String_delete(hostName);
    return;
  }
  errorMessage = String_new();
  if (!StringMap_getString(argumentMap,"errorMessage",errorMessage,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected hostName=<text>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"duration",&duration,0L))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected duration=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getULong(argumentMap,"totalEntryCount",&totalEntryCount,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected totalEntryCount=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"totalEntrySize",&totalEntrySize,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected totalEntrySize=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getULong(argumentMap,"skippedEntryCount",&skippedEntryCount,0L))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected skippedEntryCount=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"skippedEntrySize",&skippedEntrySize,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected skippedEntrySize=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getULong(argumentMap,"errorEntryCount",&errorEntryCount,0L))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected errorEntryCount=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"errorEntrySize",&errorEntrySize,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected errorEntrySize=<n>");
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
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"create new history entry");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }

  // send result
  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"historyId=%lld",historyId);

  // free resources
  String_delete(errorMessage);
  String_delete(hostName);
}

// server commands
const struct
{
  const char           *name;
  SlaveCommandFunction slaveCommandFunction;
}
SLAVE_COMMANDS[] =
{
  { "PREPROCESS",                slaveCommand_preProcess              },
  { "POSTPROCESS",               slaveCommand_postProcess             },

  { "STORAGE_CREATE",            slaveCommand_storageCreate           },
  { "STORAGE_WRITE",             slaveCommand_storageWrite            },
  { "STORAGE_CLOSE",             slaveCommand_storageClose            },

  { "INDEX_FIND_UUID",           slaveCommand_indexFindUUID           },
  { "INDEX_NEW_UUID",            slaveCommand_indexNewUUID            },
  { "INDEX_NEW_ENTITY",          slaveCommand_indexNewEntity          },
  { "INDEX_NEW_STORAGE",         slaveCommand_indexNewStorage         },
  { "INDEX_ADD_FILE",            slaveCommand_indexAddFile            },
  { "INDEX_ADD_IMAGE",           slaveCommand_indexAddImage           },
  { "INDEX_ADD_DIRECTORY",       slaveCommand_indexAddDirectory       },
  { "INDEX_ADD_LINK",            slaveCommand_indexAddLink            },
  { "INDEX_ADD_HARDLINK",        slaveCommand_indexAddHardlink        },
  { "INDEX_ADD_SPECIAL",         slaveCommand_indexAddSpecial         },

  { "INDEX_SET_STATE",           slaveCommand_indexSetState           },
  { "INDEX_STORAGE_UPDATE",      slaveCommand_indexStorageUpdate      },
  { "INDEX_UPDATE_STORAGE_INFOS",slaveCommand_indexUpdateStorageInfos },


  { "INDEX_NEW_HISTORY",         slaveCommand_indexNewHistory         },
};

/***********************************************************************\
* Name   : findSlaveCommand
* Purpose: find slave command
* Input  : name - command name
* Output : slaveCommandFunction - slave command function
* Return : TRUE if command found, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool findSlaveCommand(ConstString          name,
                            SlaveCommandFunction *slaveCommandFunction
                           )
{
  uint i;

  assert(name != NULL);
  assert(slaveCommandFunction != NULL);

  // find command by name
  i = 0;
  while ((i < SIZE_OF_ARRAY(SLAVE_COMMANDS)) && !String_equalsCString(name,SLAVE_COMMANDS[i].name))
  {
    i++;
  }
  if (i >= SIZE_OF_ARRAY(SLAVE_COMMANDS))
  {
    return FALSE;
  }
  (*slaveCommandFunction) = SLAVE_COMMANDS[i].slaveCommandFunction;

  return TRUE;
}

/***********************************************************************\
* Name   : slaveThreadCode
* Purpose: slave thread code
* Input  : slaveInfo - slave info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void slaveThreadCode(SlaveInfo *slaveInfo)
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
  SlaveCommandFunction slaveCommandFunction;

  // init variables
  name        = String_new();
  argumentMap = StringMap_new();

  // Note: ignore SIGALRM in ppoll()
  sigemptyset(&signalMask);
  sigaddset(&signalMask,SIGALRM);

  // init index
  indexHandle = Index_open(NULL,INDEX_TIMEOUT);

  // process client requests
  while (!Thread_isQuit(&slaveInfo->thread))
  {
//fprintf(stderr,"%s, %d: slave thread wiat command\n",__FILE__,__LINE__);
    // wait for disconnect, command, or result
    pollfds[0].fd     = Network_getSocket(&slaveInfo->io.network.socketHandle);
    pollfds[0].events = POLLIN|POLLERR|POLLNVAL;
    pollTimeout.tv_sec  = (long)(TIMEOUT /MS_PER_SECOND);
    pollTimeout.tv_nsec = (long)((TIMEOUT%MS_PER_SECOND)*1000LL);
    n = ppoll(pollfds,1,&pollTimeout,&signalMask);
    if      (n < 0)
    {
fprintf(stderr,"%s, %d: poll fail\n",__FILE__,__LINE__);
//slaveDisconnect(slaveInfo);
break;
    }
    else if (n > 0)
    {
      if      ((pollfds[0].revents & POLLIN) != 0)
      {
        if (ServerIO_receiveData(&slaveInfo->io))
        {
          while (ServerIO_getCommand(&slaveInfo->io,
                                     &id,
                                     name,
                                     argumentMap
                                    )
                )
          {
            // find command
            #ifdef SLAVE_DEBUG
//TODO: enable
//              fprintf(stderr,"DEBUG: received command '%s'\n",String_cString(name));
            #endif
            if (!findSlaveCommand(name,&slaveCommandFunction))
            {
              ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_PARSING,"unknown command '%S'",name);
              continue;
            }
            assert(slaveCommandFunction != NULL);

            // process command
            slaveCommandFunction(slaveInfo,indexHandle,id,argumentMap);
          }
        }
        else
        {
fprintf(stderr,"%s, %d: disc\n",__FILE__,__LINE__);
//slaveDisconnect(slaveInfo);
          break;
        }
      }
      else if ((pollfds[0].revents & (POLLERR|POLLNVAL)) != 0)
      {
fprintf(stderr,"%s, %d: error/disc\n",__FILE__,__LINE__);
//slaveDisconnect(slaveInfo);
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

// ----------------------------------------------------------------------

Errors Slave_initAll(void)
{
  // init variables

  return ERROR_NONE;
}

void Slave_doneAll(void)
{
}

void Slave_init(SlaveInfo *slaveInfo)
{
  assert(slaveInfo != NULL);

//TODO: remove
//  slaveInfo->forceSSL                        = forceSSL;
  ServerIO_init(&slaveInfo->io);
  slaveInfo->storageOpenFlag                = FALSE;
  slaveInfo->slaveConnectStatusInfoFunction = NULL;
  slaveInfo->slaveConnectStatusInfoUserData = NULL;
}

void Slave_duplicate(SlaveInfo *slaveInfo, const SlaveInfo *fromSlaveInfo)
{
  assert(slaveInfo != NULL);
  assert(fromSlaveInfo != NULL);

  Slave_init(slaveInfo);

  slaveInfo->slaveConnectStatusInfoFunction = fromSlaveInfo->slaveConnectStatusInfoFunction;
  slaveInfo->slaveConnectStatusInfoUserData = fromSlaveInfo->slaveConnectStatusInfoUserData;
}

void Slave_done(SlaveInfo *slaveInfo)
{
  assert(slaveInfo != NULL);

  if (slaveInfo->storageOpenFlag)
  {
    Storage_close(&slaveInfo->storageHandle);
  }
  ServerIO_done(&slaveInfo->io);
}

Errors Slave_connect(SlaveInfo                      *slaveInfo,
                     ConstString                    hostName,
                     uint                           hostPort,
                     ConstString                    storageName,
                     JobOptions                     *jobOptions,
                     SlaveConnectStatusInfoFunction slaveConnectStatusInfoFunction,
                     void                           *slaveConnectStatusInfoUserData
                    )
{
  AutoFreeList     autoFreeList;
  SessionId        sessionId;
  String           printableStorageName;
  Errors           error;
  StorageSpecifier storageSpecifier;
  IndexHandle      *indexHandle;

  assert(slaveInfo != NULL);
  assert(hostName != NULL);

  // init variables
  AutoFree_init(&autoFreeList);
  printableStorageName         = String_new();
  AUTOFREE_ADD(&autoFreeList,printableStorageName,{ String_delete(printableStorageName); });

  // slave connect
  error = slaveConnect(slaveInfo,
                       hostName,
                       hostPort,
                       sessionId
                      );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,slaveInfo,{ slaveDisconnect(slaveInfo); });

  // start session
  error = initSession(slaveInfo,
                      sessionId
                     );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,slaveInfo,{ doneSession(slaveInfo); });

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
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Storage_doneSpecifier(&storageSpecifier); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&storageSpecifier,{ Storage_doneSpecifier(&storageSpecifier); });

  // open index
  indexHandle = Index_open(NULL,INDEX_TIMEOUT);
  AUTOFREE_ADD(&autoFreeList,indexHandle,{ Index_close(indexHandle); });

  // get printable storage name
  Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);

  // init storage
  error = Storage_init(&slaveInfo->storageInfo,
                       NULL, // masterIO
                       &storageSpecifier,
                       jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
CALLBACK(NULL,NULL),//                       CALLBACK(updateStorageStatusInfo,slaveInfo),
CALLBACK(NULL,NULL),//                       CALLBACK(getPasswordFunction,getPasswordUserData),
CALLBACK(NULL,NULL)//                       CALLBACK(storageRequestVolumeFunction,storageRequestVolumeUserData)
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize storage '%s' (error: %s)\n",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Storage_done(&slaveInfo->storageInfo); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,&slaveInfo->storageInfo,{ Storage_done(&slaveInfo->storageInfo); });

  // init status callback
  slaveInfo->slaveConnectStatusInfoFunction = slaveConnectStatusInfoFunction;
  slaveInfo->slaveConnectStatusInfoUserData = slaveConnectStatusInfoUserData;

  printInfo(2,"Connected slave host '%s:%d'\n",String_cString(hostName),hostPort);

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(printableStorageName);
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

void Slave_disconnect(SlaveInfo *slaveInfo)
{
  assert(slaveInfo != NULL);

  // stop slave thread
  Thread_quit(&slaveInfo->thread);
  if (!Thread_join(&slaveInfo->thread))
  {
    HALT_FATAL_ERROR("Cannot terminate slave thread!");
  }

  // close storage (if open)
  if (slaveInfo->storageOpenFlag)
  {
    Storage_close(&slaveInfo->storageHandle);
  }

  // done storage
  Storage_done(&slaveInfo->storageInfo);

  slaveDisconnect(slaveInfo);
}

// ----------------------------------------------------------------------

LOCAL Errors Slave_vexecuteCommand(SlaveInfo  *slaveInfo,
                                   uint       debugLevel,
                                   long       timeout,
                                   StringMap  resultMap,
                                   const char *format,
                                   va_list    arguments
                                  )
{
  Errors error;

  assert(slaveInfo != NULL);

//TODO
  // init variables

  error = ServerIO_vexecuteCommand(&slaveInfo->io,
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

Errors Slave_executeCommand(SlaveInfo  *slaveInfo,
                            uint       debugLevel,
                            long       timeout,
                            StringMap  resultMap,
                            const char *format,
                            ...
                           )
{
  va_list  arguments;
  Errors   error;

  assert(slaveInfo != NULL);

  va_start(arguments,format);
  error = Slave_vexecuteCommand(slaveInfo,debugLevel,timeout,resultMap,format,arguments);
  va_end(arguments);

  return error;
}

Errors Slave_jobStart(SlaveInfo                       *slaveInfo,
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
 //                     ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
 //                     void                            *archiveGetCryptPasswordUserData,
 //                     CreateStatusInfoFunction        createStatusInfoFunction,
 //                     void                            *createStatusInfoUserData,
                      StorageRequestVolumeFunction    storageRequestVolumeFunction,
                      void                            *storageRequestVolumeUserData
                     )
{
  #define SET_OPTION_STRING(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Slave_setJobOptionString   (slaveInfo, \
                                                                   jobUUID, \
                                                                   name, \
                                                                   value \
                                                                  ); \
    } \
    while (0)
  #define SET_OPTION_CSTRING(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Slave_setJobOptionCString  (slaveInfo, \
                                                                   jobUUID, \
                                                                   name, \
                                                                   value \
                                                                  ); \
    } \
    while (0)
  #define SET_OPTION_PASSWORD(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Slave_setJobOptionPassword (slaveInfo, \
                                                                   jobUUID, \
                                                                   name, \
                                                                   value \
                                                                  ); \
    } \
    while (0)
  #define SET_OPTION_INTEGER(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Slave_setJobOptionInteger  (slaveInfo, \
                                                                   jobUUID, \
                                                                   name, \
                                                                   value \
                                                                  ); \
    } \
    while (0)
  #define SET_OPTION_INTEGER64(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Slave_setJobOptionInteger64(slaveInfo, \
                                                                   jobUUID, \
                                                                   name, \
                                                                   value \
                                                                  ); \
    } \
    while (0)
  #define SET_OPTION_BOOLEAN(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Slave_setJobOptionBoolean  (slaveInfo, \
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

  assert(slaveInfo != NULL);
  assert(jobUUID != NULL);

  // init variables
  s = String_new();

  error = ERROR_NONE;

  // create temporary job
  error = Slave_executeCommand(slaveInfo,
                               SLAVE_DEBUG_LEVEL,
                               SLAVE_COMMAND_TIMEOUT,
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
  if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,SLAVE_DEBUG_LEVEL,SLAVE_COMMAND_TIMEOUT,NULL,"INCLUDE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(includeEntryList,entryNode)
  {
    switch (entryNode->type)
    {
      case ENTRY_TYPE_FILE :   entryTypeText = "FILE";    break;
      case ENTRY_TYPE_IMAGE:   entryTypeText = "IMAGE";   break;
      case ENTRY_TYPE_UNKNOWN:
      default:                 entryTypeText = "UNKNOWN"; break;
    }
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,
                                                          SLAVE_DEBUG_LEVEL,
                                                          SLAVE_COMMAND_TIMEOUT,
                                                          NULL,
                                                          "INCLUDE_LIST_ADD jobUUID=%S entryType=%s patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          entryTypeText,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,entryNode->patternType,NULL),
                                                          entryNode->string
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,SLAVE_DEBUG_LEVEL,SLAVE_COMMAND_TIMEOUT,NULL,"EXCLUDE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(excludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,
                                                          SLAVE_DEBUG_LEVEL,
                                                          SLAVE_COMMAND_TIMEOUT,
                                                          NULL,
                                                          "EXCLUDE_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->pattern.type,NULL),
                                                          patternNode->string
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,SLAVE_DEBUG_LEVEL,SLAVE_COMMAND_TIMEOUT,NULL,"MOUNT_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(mountList,mountNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,
                                                          SLAVE_DEBUG_LEVEL,
                                                          SLAVE_COMMAND_TIMEOUT,
                                                          NULL,
                                                          "MOUNT_LIST_ADD jobUUID=%S name=%'S alwaysUnmount=%y",
                                                          jobUUID,
                                                          mountNode->name,
                                                          mountNode->alwaysUnmount
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,SLAVE_DEBUG_LEVEL,SLAVE_COMMAND_TIMEOUT,NULL,"EXCLUDE_COMPRESS_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(compressExcludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,
                                                          SLAVE_DEBUG_LEVEL,
                                                          SLAVE_COMMAND_TIMEOUT,
                                                          NULL,
                                                          "EXCLUDE_COMPRESS_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->pattern.type,NULL),
                                                          patternNode->string
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,SLAVE_DEBUG_LEVEL,SLAVE_COMMAND_TIMEOUT,NULL,"SOURCE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(deltaSourceList,deltaSourceNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,
                                                          SLAVE_DEBUG_LEVEL,
                                                          SLAVE_COMMAND_TIMEOUT,
                                                          NULL,
                                                          "SOURCE_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,deltaSourceNode->patternType,NULL),
                                                          deltaSourceNode->storageName
                                                         );
  }
  if (error != ERROR_NONE)
  {
    (void)Slave_executeCommand(slaveInfo,SLAVE_DEBUG_LEVEL,SLAVE_COMMAND_TIMEOUT,NULL,"JOB_DELETE jobUUID=%S",jobUUID);
    String_delete(s);
    return error;
  }
fprintf(stderr,"%s, %d: %d: Slave_jobStart %s\n",__FILE__,__LINE__,error,Error_getText(error));

  // start execute job
  error = Slave_executeCommand(slaveInfo,SLAVE_DEBUG_LEVEL,SLAVE_COMMAND_TIMEOUT,NULL,"JOB_START jobUUID=%S archiveType=%s dryRun=%y",jobUUID,Archive_archiveTypeToString(archiveType,NULL),FALSE);
  if (error != ERROR_NONE)
  {
    (void)Slave_executeCommand(slaveInfo,SLAVE_DEBUG_LEVEL,SLAVE_COMMAND_TIMEOUT,NULL,"JOB_DELETE jobUUID=%S",jobUUID);
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

Errors Slave_jobAbort(SlaveInfo   *slaveInfo,
                      ConstString jobUUID
                     )
{
  Errors error;

  assert(slaveInfo != NULL);
  assert(jobUUID != NULL);

  error = ERROR_NONE;

  // abort execute job
  error = Slave_executeCommand(slaveInfo,SLAVE_DEBUG_LEVEL,SLAVE_COMMAND_TIMEOUT,NULL,"JOB_ABORT jobUUID=%S",jobUUID);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // free resources

  return ERROR_NONE;
}

#if 0
Errors Slave_process(SlaveInfo *slaveInfo,
                     long      timeout
                    )
{
  uint      id;
  String    name;
  StringMap argumentMap;
  Errors    error;

  assert(slaveInfo != NULL);

  // init variables
  name        = String_new();
  argumentMap = StringMap_new();

  // process commands
  do
  {
fprintf(stderr,"%s, %d: wait command\n",__FILE__,__LINE__);
    if (ServerIO_getCommand(&slaveInfo->io,
                            &id,
                            name,
                            argumentMap
                           )
       )
    {
fprintf(stderr,"%s, %d: ---------------- got command #%u: %s\n",__FILE__,__LINE__,id,String_cString(name));
ServerIO_sendResult(&slaveInfo->io,
                    id,
                    TRUE,
                    ERROR_NONE,
                    ""
                   );
fprintf(stderr,"%s, %d: sent OK result\n",__FILE__,__LINE__);
    }
  }
  while (error == ERROR_NONE);
//  ServerIO_wait(&slaveInfo->io,timeout);

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
