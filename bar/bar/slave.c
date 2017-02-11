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
#include "bar.h"

#include "server.h"

#include "slave.h"

/****************** Conditional compilation switches *******************/

#define SLAVE_DEBUG

/***************************** Constants *******************************/
#define SLEEP_TIME_SLAVE_THREAD (   10)  // [s]

#define READ_TIMEOUT            ( 5LL*MS_PER_SECOND)
#define COMMAND_TIMEOUT         (10LL*MS_PER_SECOND)

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
                          uint         hostPort
                         )
{
  String       line;
  SocketHandle socketHandle;
  Errors       error;

  assert(slaveInfo != NULL);

  // init variables

  // connect to slave
  error = Network_connect(&socketHandle,
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
                          SOCKET_FLAG_NON_BLOCKING|SOCKET_FLAG_NO_DELAY
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // connect network server i/o
  ServerIO_connectNetwork(&slaveInfo->io,
                          hostName,
                          hostPort,
                          socketHandle
                         );
//fprintf(stderr,"%s, %d: Network_getSocket(&slaveInfo->io.network.socketHandle)=%d\n",__FILE__,__LINE__,Network_getSocket(&slaveInfo->io.network.socketHandle));

  // authorize
  line = String_new();
  error = Network_readLine(&slaveInfo->io.network.socketHandle,line,30LL*MS_PER_SECOND);
  if (error != ERROR_NONE)
  {
    String_delete(line);
    return error;
  }
  String_delete(line);

  // free resources

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

  return Slave_executeCommand(slaveInfo,COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%d",jobUUID,name,value);
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

  return Slave_executeCommand(slaveInfo,COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%lld",jobUUID,name,value);
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

  return Slave_executeCommand(slaveInfo,COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%y",jobUUID,name,value);
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

  return Slave_executeCommand(slaveInfo,COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'S",jobUUID,name,value);
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

  return Slave_executeCommand(slaveInfo,COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'s",jobUUID,name,value);
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
  error = Slave_executeCommand(slaveInfo,COMMAND_TIMEOUT,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'s",jobUUID,name,plainPassword);
  Password_undeploy(password,plainPassword);

  return error;
}

// ----------------------------------------------------------------------

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

  error = Storage_create(&slaveInfo->storageHandle,
                         &slaveInfo->storageInfo,
                         archiveName,
                         archiveSize
                        );
  if (error != ERROR_NONE)
  {
fprintf(stderr,"%s, %d: error=%s\n",__FILE__,__LINE__,Error_getText(error));
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"create storage fail");
    String_delete(archiveName);
    return;
  }
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

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
*            data=<base64 encoded data>
*          Result:
\***********************************************************************/

LOCAL void slaveCommand_storageWrite(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint64 offset;
  String data;
  void   *buffer;
  ulong  bufferLength;
  Errors error;

  // get offset, data
  if (!StringMap_getUInt64(argumentMap,"offset",&offset,0LL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected offset=<n>");
    return;
  }
  data = String_new();
  if (!StringMap_getString(argumentMap,"data",data,NULL))
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected data=<data>");
    String_delete(data);
    return;
  }

  // decode data
  bufferLength = Misc_base64DecodeLength(data,STRING_BEGIN);
  buffer = malloc(bufferLength);
  if (buffer == NULL)
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_INSUFFICIENT_MEMORY,"insufficient memory");
    String_delete(data);
    return;
  }
  Misc_base64Decode(buffer,bufferLength,data,STRING_BEGIN);

//TODO: offset?

  // write storage
  error = Storage_write(&slaveInfo->storageHandle,buffer,bufferLength);
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&slaveInfo->io,id,TRUE,error,"write storage fail");
    free(buffer);
    String_delete(data);
    return;
  }

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

  // close storage
  Storage_close(&slaveInfo->storageHandle);

//TODO: index

  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : slaveCommand_indexEntityAdd
* Purpose: close storage
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            error=<n>
*          Result:
*            errorMessage=<text>
\***********************************************************************/

LOCAL void slaveCommand_indexEntityAdd(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : slaveCommand_indexEntryAdd
* Purpose: close storage
* Input  : slaveInfo   - slave info
*          indexHandle - index handle
*          id          - command id
*          argumentMap - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            error=<n>
*          Result:
*            errorMessage=<text>
\***********************************************************************/

LOCAL void slaveCommand_indexEntryAdd(SlaveInfo *slaveInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

  ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_NONE,"");
}

// server commands
const struct
{
  const char           *name;
  SlaveCommandFunction slaveCommandFunction;
}
SLAVE_COMMANDS[] =
{
  { "STORAGE_CREATE",                 slaveCommand_storageCreate  },
  { "STORAGE_WRITE",                  slaveCommand_storageWrite   },
  { "STORAGE_CLOSE",                  slaveCommand_storageClose   },

  { "INDEX_ENTITY_ADD",               slaveCommand_indexEntityAdd },
  { "INDEX_ENTRY_ADD",                slaveCommand_indexEntryAdd  },
};

/***********************************************************************\
* Name   : findCommand
* Purpose: find command
* Input  : name - command name
* Output : slaveCommandFunction - slave command function
* Return : TRUE if command found, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool findCommand(ConstString          name,
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

fprintf(stderr,"%s, %d: start slave\n",__FILE__,__LINE__);
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
slaveDisconnect(slaveInfo);
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
              fprintf(stderr,"DEBUG: received command '%s'\n",String_cString(name));
            #endif
            if (!findCommand(name,&slaveCommandFunction))
            {
              ServerIO_sendResult(&slaveInfo->io,id,TRUE,ERROR_PARSING,"unknown command '%S'",name);
              continue;
            }
            assert(slaveCommandFunction != NULL);

            // process command
            slaveCommandFunction(slaveInfo,NULL,id,argumentMap);
          }
        }
        else
        {
fprintf(stderr,"%s, %d: disc\n",__FILE__,__LINE__);
slaveDisconnect(slaveInfo);
          break;
        }
      }
      else if ((pollfds[0].revents & (POLLERR|POLLNVAL)) != 0)
      {
fprintf(stderr,"%s, %d: error/disc\n",__FILE__,__LINE__);
slaveDisconnect(slaveInfo);
        break;
      }
    }
  }

  // free resources
  StringMap_delete(argumentMap);
  String_delete(name);

fprintf(stderr,"%s, %d: end slave\n",__FILE__,__LINE__);
}

// ----------------------------------------------------------------------

Errors Slave_initAll(void)
{
  // init variables

  return ERROR_NONE;
}

void Slave_doneAll(void)
{
//TODO
//  List_done(&slaveList,
//  Semaphore_done(&slaveList.lock);
}

void Slave_init(SlaveInfo *slaveInfo)
{
  assert(slaveInfo != NULL);

//TODO: remove
//  slaveInfo->forceSSL                        = forceSSL;
  ServerIO_init(&slaveInfo->io);
  slaveInfo->slaveConnectStatusInfoFunction = NULL;
  slaveInfo->slaveConnectStatusInfoUserData = NULL;
}

void Slave_duplicate(SlaveInfo *slaveInfo, const SlaveInfo *fromSlaveInfo)
{
  Slave_init(slaveInfo);

//  String_set(slaveInfo->name,fromSlaveInfo->name);
//  slaveInfo->port                           = fromSlaveInfo->port;
  slaveInfo->slaveConnectStatusInfoFunction = fromSlaveInfo->slaveConnectStatusInfoFunction;
  slaveInfo->slaveConnectStatusInfoUserData = fromSlaveInfo->slaveConnectStatusInfoUserData;
}

void Slave_done(SlaveInfo *slaveInfo)
{
  assert(slaveInfo != NULL);

//TODO: remove
//  List_done(&slaveInfo->resultList,CALLBACK(freeResultNode,NULL));
//  Semaphore_done(&slaveInfo->resultList.lock);
//  List_done(&slaveInfo->commandList,CALLBACK(freeCommandNode,NULL));
//  Semaphore_done(&slaveInfo->commandList.lock);
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
                       hostPort
                      );
  if (error != ERROR_NONE)
  {
    return error;
  }

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
  indexHandle = Index_open(INDEX_PRIORITY_HIGH,INDEX_TIMEOUT);
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

  // start slave thread
  if (!Thread_init(&slaveInfo->thread,"BAR slave",globalOptions.niceLevel,slaveThreadCode,slaveInfo))
  {
    HALT_FATAL_ERROR("Cannot initialize slave thread!");
  }

  // init callback
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
//TODO

  // done storage
  Storage_done(&slaveInfo->storageInfo);

  slaveDisconnect(slaveInfo);
}

// ----------------------------------------------------------------------

#if 0
bool Slave_waitCommand(SlaveInfo *slaveInfo,
                       long      timeout,
                       uint      *id,
                       String    name,
                       StringMap argumentMap
                      )
{
  assert(slaveInfo != NULL);
  assert(id != NULL);
  assert(name != NULL);

  return ServerIO_getCommand(&slaveInfo->io,
                             id,
                             name,
                             argumentMap
                            );
}
#endif

LOCAL Errors Slave_vexecuteCommand(SlaveInfo  *slaveInfo,
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
  error = Slave_vexecuteCommand(slaveInfo,timeout,resultMap,format,arguments);
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
                               COMMAND_TIMEOUT,
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
                                                               ConfigValue_selectToString(CONFIG_VALUE_COMPRESS_ALGORITHMS,jobOptions->compressAlgorithms.delta,"none"),
                                                               ConfigValue_selectToString(CONFIG_VALUE_COMPRESS_ALGORITHMS,jobOptions->compressAlgorithms.byte ,"none")
                                                              )
                      );
  SET_OPTION_CSTRING  ("crypt-algorithm",        ConfigValue_selectToString(CONFIG_VALUE_CRYPT_ALGORITHMS,jobOptions->cryptAlgorithms[0],NULL));
  SET_OPTION_CSTRING  ("crypt-type",             ConfigValue_selectToString(CONFIG_VALUE_CRYPT_TYPES,jobOptions->cryptType,NULL));
  SET_OPTION_CSTRING  ("crypt-password-mode",    ConfigValue_selectToString(CONFIG_VALUE_PASSWORD_MODES,jobOptions->cryptPasswordMode,NULL));
  SET_OPTION_PASSWORD ("crypt-password",         jobOptions->cryptPassword               );
  SET_OPTION_STRING   ("crypt-public-key",       Misc_base64Encode(s,jobOptions->cryptPublicKey.data,jobOptions->cryptPublicKey.length      ));

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
fprintf(stderr,"%s, %d: e=%s\n",__FILE__,__LINE__,Error_getText(error));

  // set lists
  if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,COMMAND_TIMEOUT,NULL,"INCLUDE_LIST_CLEAR jobUUID=%S",jobUUID);
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
                                                          COMMAND_TIMEOUT,
                                                          NULL,
                                                          "INCLUDE_LIST_ADD jobUUID=%S entryType=%s patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          entryTypeText,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,entryNode->patternType,NULL),
                                                          entryNode->string
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,COMMAND_TIMEOUT,NULL,"EXCLUDE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(excludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,
                                                          COMMAND_TIMEOUT,
                                                          NULL,
                                                          "EXCLUDE_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->patternType,NULL),
                                                          patternNode->string
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,COMMAND_TIMEOUT,NULL,"MOUNT_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(mountList,mountNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,
                                                          COMMAND_TIMEOUT,
                                                          NULL,
                                                          "MOUNT_LIST_ADD jobUUID=%S name=%'S alwaysUnmount=%y",
                                                          jobUUID,
                                                          mountNode->name,
                                                          mountNode->alwaysUnmount
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,COMMAND_TIMEOUT,NULL,"EXCLUDE_COMPRESS_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(compressExcludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,
                                                          COMMAND_TIMEOUT,
                                                          NULL,
                                                          "EXCLUDE_COMPRESS_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->patternType,NULL),
                                                          patternNode->string
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,COMMAND_TIMEOUT,NULL,"SOURCE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(deltaSourceList,deltaSourceNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,
                                                          COMMAND_TIMEOUT,
                                                          NULL,
                                                          "SOURCE_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,deltaSourceNode->patternType,NULL),
                                                          deltaSourceNode->storageName
                                                         );
  }
  if (error != ERROR_NONE)
  {
    (void)Slave_executeCommand(slaveInfo,COMMAND_TIMEOUT,NULL,"JOB_DELETE jobUUID=%S",jobUUID);
    String_delete(s);
    return error;
  }
fprintf(stderr,"%s, %d: %d: Slave_jobStart %s\n",__FILE__,__LINE__,error,Error_getText(error));

  // start execute job
  error = Slave_executeCommand(slaveInfo,COMMAND_TIMEOUT,NULL,"JOB_START jobUUID=%S archiveType=%s dryRun=%y",jobUUID,"FULL"/*archiveType*/,FALSE);
  if (error != ERROR_NONE)
  {
    (void)Slave_executeCommand(slaveInfo,COMMAND_TIMEOUT,NULL,"JOB_DELETE jobUUID=%S",jobUUID);
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
  error = Slave_executeCommand(slaveInfo,COMMAND_TIMEOUT,NULL,"JOB_ABORT jobUUID=%S",jobUUID);
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
