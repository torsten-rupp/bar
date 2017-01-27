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

/***************************** Constants *******************************/
#define SLEEP_TIME_SLAVE_THREAD (   10)  // [s]

#define READ_TIMEOUT            ( 5LL*MS_PER_SECOND)
#define COMMAND_TIMEOUT         (10LL*MS_PER_SECOND)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL bool      quitFlag;
LOCAL Thread    slaveThread;

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
fprintf(stderr,"%s, %d: Network_getSocket(&slaveInfo->io.network.socketHandle)=%d\n",__FILE__,__LINE__,Network_getSocket(&slaveInfo->io.network.socketHandle));

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

LOCAL void processSlave(SlaveInfo *slaveInfo, ConstString line)
{
  uint                 commandId;
  bool                 completedFlag;
  Errors               error;
  String               name;
  String               data;
  ServerIOResultNode           *resultNode;
  ServerIOCommandNode          *commandNode;
  SemaphoreLock        semaphoreLock;
  uint                 i;
  StringMap            argumentMap;

//  assert(Semaphore_isLocked(&slaveList));

  // init variables
  name = String_new();
  data = String_new();

  // parse
  if      (String_parse(line,STRING_BEGIN,"%u %y %u % S",NULL,&commandId,&completedFlag,&error,data))
  {
    // init result
    resultNode = LIST_NEW_NODE(ServerIOResultNode);
    if (resultNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    resultNode->id            = commandId;
    resultNode->error         = (error != ERROR_NONE) ? Errorx_(error,0,"%s",String_cString(data)) : ERROR_NONE;
    resultNode->completedFlag = completedFlag;
    resultNode->data          = String_duplicate(data);

    // add result
//TODO:
#if 0
    SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveInfo->resultList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      List_append(&slaveInfo->resultList,resultNode);
    }
fprintf(stderr,"%s, %d: %p added result commandId=%d data=%s %d\n",__FILE__,__LINE__,slaveInfo,commandId,String_cString(data),slaveInfo->resultList.count);
//    Semaphore_signalModified(&slaveList.lock);
#endif
  }
  else if (!String_parse(line,STRING_BEGIN,"%u %S % S",NULL,&commandId,name,data))
  {
    // init command
    resultNode = LIST_NEW_NODE(ServerIOResultNode);
    if (resultNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    commandNode->id = commandId;
    commandNode->name      = String_duplicate(name);
    commandNode->data      = String_duplicate(data);

#if 0
    // parse arguments
    argumentMap = StringMap_new();
    if (argumentMap == NULL)
    {
      String_delete(arguments);
      String_delete(name);
      return;
    }
    if (!StringMap_parse(argumentMap,data,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,STRING_BEGIN,NULL))
    {
      StringMap_delete(argumentMap);
      String_delete(name);
      String_delete(name);
      return;
    }
#endif

    // add command
//TODO:
#if 0
    SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveInfo->commandList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      List_append(&slaveInfo->commandList,commandNode);
    }
fprintf(stderr,"%s, %d: %p added command %s %d\n",__FILE__,__LINE__,slaveInfo,commandId,String_cString(line),slaveInfo->commandList.count);
//    Semaphore_signalModified(&slaveList.lock);
#endif
  }
  else
  {
fprintf(stderr,"%s, %d: unkown %s\n",__FILE__,__LINE__,String_cString(line));
    // unknown
  }

  // free resources
  String_delete(data);
  String_delete(name);
}

#if 0
/***********************************************************************\
* Name   : slaveThreadCode
* Purpose: slave thread code
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void slaveThreadCode(void)
{
  typedef struct SlaveJobInfoNode
  {
    LIST_NODE_HEADER(struct SlaveJobInfoNode);

    String    jobUUID;
    SlaveInfo slaveInfo;
  } SlaveJobInfoNode;

  typedef struct
  {
    LIST_HEADER(SlaveJobInfoNode);
  } SlaveJobInfoList;

  struct pollfd         *pollfds;
  uint                  maxPollfdCount;
  uint                  pollfdCount;

  SemaphoreLock    semaphoreLock;
  SlaveInfo        slaveInfo;
  struct timespec       pollTimeout;
  sigset_t              signalMask;
  uint                  pollfdIndex;
  char                  buffer[4096];
  ulong                 receivedBytes;
  ulong                 i;
  Errors                error;

  // init variables
  maxPollfdCount = 64;  // initial max. number of parallel connections
  pollfds = (struct pollfd*)malloc(maxPollfdCount * sizeof(struct pollfd));
  if (pollfds == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // Note: ignore SIGALRM in ppoll()
  sigemptyset(&signalMask);
  sigaddset(&signalMask,SIGALRM);

  // process client requests
  while (!quitFlag)
  {
    // connect slaves
    SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
#if 0
          if (isSlaveJob(jobNode) && !Slave_isConnected(&jobNode->slaveInfo))
          {
fprintf(stderr,"%s, %d: req connect jobUUID=%s host=%s\n",__FILE__,__LINE__,String_cString(jobUUID),String_cString(jobNode->slaveInfo.name));
            Slave_copyHost(&slaveInfo,&jobNode->slaveInfo);
            tryConnectFlag = TRUE;
          }

      // try to connect
      if (tryConnectFlag)
      {
//        (void)Slave_connect(&slaveInfo);
        error = Slave_connect(&slaveInfo);
fprintf(stderr,"%s, %d: connect result host=%s: %s\n",__FILE__,__LINE__,String_cString(slaveInfo.name),Error_getText(error));
      }
#endif
    }
    if (quitFlag) break;

    // get connected slaves to wait for
//fprintf(stderr,"%s, %d: lock: get slaves\n",__FILE__,__LINE__);
    pollfdCount = 0;
    SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      LIST_ITERATE(&slaveList,slaveNode)
      {
        if (slaveNode->isConnected)
        {
          if (pollfdCount >= maxPollfdCount)
          {
            maxPollfdCount += 64;
            pollfds = (struct pollfd*)realloc(pollfds,maxPollfdCount);
            if (pollfds == NULL) HALT_INSUFFICIENT_MEMORY();
          }
          pollfds[pollfdCount].fd     = Network_getSocket(&slaveNode->socketHandle);
          pollfds[pollfdCount].events = POLLIN|POLLERR|POLLNVAL;
          pollfdCount++;
        }
      }
    }
fprintf(stderr,"%s, %d: active %d\n",__FILE__,__LINE__,pollfdCount);

    if (pollfdCount > 0)
    {
      // wait for slave results/requests
      pollTimeout.tv_sec  = SLEEP_TIME_SLAVE_THREAD;
      pollTimeout.tv_nsec = 0;
fprintf(stderr,"%s, %d: wait...\n",__FILE__,__LINE__);
      (void)ppoll(pollfds,pollfdCount,&pollTimeout,&signalMask);
fprintf(stderr,"%s, %d: wait done -------\n",__FILE__,__LINE__);
      if (quitFlag) break;

      // process slave results/requests
      SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        for (pollfdIndex = 0; pollfdIndex < pollfdCount; pollfdIndex++)
        {
          // find slave
          slaveNode = LIST_FIND(&slaveList,slaveNode,pollfds[pollfdIndex].fd == Network_getSocket(&slaveNode->socketHandle));
assert(slaveNode != NULL);

          if ((pollfds[pollfdIndex].revents & POLLIN) != 0)
          {
            // received data
            Network_receive(&slaveNode->socketHandle,buffer,sizeof(buffer),NO_WAIT,&receivedBytes);
//fprintf(stderr,"%s, %d: buffer=%s\n",__FILE__,__LINE__,buffer);

            SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
            {
              // find client node
              slaveNode = LIST_FIND(&slaveList,
                                     slaveNode,
                                     pollfds[pollfdIndex].fd == Network_getSocket(&slaveNode->socketHandle)
                                    );
              if (slaveNode != NULL)
              {
                if (receivedBytes > 0)
                {
                  do
                  {
                    // received data -> process
                    for (i = 0; i < receivedBytes; i++)
                    {
                      if (buffer[i] != '\n')
                      {
                        String_appendChar(slaveNode->line,buffer[i]);
                      }
                      else
                      {
//fprintf(stderr,"%s, %d: process %s \n",__FILE__,__LINE__,String_cString(slaveNode->line));
                        processSlave(slaveNode,slaveNode->line);
//fprintf(stderr,"%s, %d: process done\n",__FILE__,__LINE__);
                        String_clear(slaveNode->line);
                      }
                    }
                    error = Network_receive(&slaveNode->socketHandle,buffer,sizeof(buffer),NO_WAIT,&receivedBytes);
                  }
                  while ((error == ERROR_NONE) && (receivedBytes > 0));
                }
                else
                {
                  // disconnect
//                  deleteClient(disconnectClientNode);
fprintf(stderr,"%s, %d: ---------- DISCONNECT1\n",__FILE__,__LINE__);
                  slaveNode->isConnected = FALSE;

                  printInfo(1,"Disconnected slave '%s:%u'\n",String_cString(slaveNode->hostName),slaveNode->hostPort);
                }
              }
            }
          }
          else if ((pollfds[pollfdIndex].revents & (POLLERR|POLLNVAL)) != 0)
          {
            // error/disconnect

            // done client and free resources
//                deleteClient(disconnectClientNode);
fprintf(stderr,"%s, %d: ---------- DISCONNECT2\n",__FILE__,__LINE__);
            slaveNode->isConnected = FALSE;

            printInfo(1,"Disconnected slave '%s:%u'\n",String_cString(slaveNode->hostName),slaveNode->hostPort);
          }
        }
      }
    }
    else
    {
      // sleep
      SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        (void)Semaphore_waitModified(&slaveList.lock,SLEEP_TIME_SLAVE_THREAD*MS_PER_SECOND);
      }
    }
    if (quitFlag) break;
  }

  // free resources
  free(pollfds);
}
#endif

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
  SemaphoreLock semaphoreLock;
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
  if (!quitFlag)
  {
//    SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveInfo->network.writeLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      (void)Network_send(&slaveInfo->io.network.socketHandle,String_cString(data),String_length(data));
    }
  }

  return commandId;
}

LOCAL Errors waitForResult(SlaveInfo *slaveInfo)
{

  return ERROR_NONE;
}

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

LOCAL Errors Slave_setJobOptionInteger(const SlaveInfo *slaveInfo, ConstString jobUUID, const char *name, int value)
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

LOCAL Errors Slave_setJobOptionInteger64(const SlaveInfo *slaveInfo, ConstString jobUUID, const char *name, int64 value)
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

LOCAL Errors Slave_setJobOptionBoolean(const SlaveInfo *slaveInfo, ConstString jobUUID, const char *name, bool value)
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

LOCAL Errors Slave_setJobOptionString(const SlaveInfo *slaveInfo, ConstString jobUUID, const char *name, ConstString value)
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

LOCAL Errors Slave_setJobOptionCString(const SlaveInfo *slaveInfo, ConstString jobUUID, const char *name, const char *value)
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

LOCAL Errors Slave_setJobOptionPassword(const SlaveInfo *slaveInfo, ConstString jobUUID, const char *name, Password *password)
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

Errors Slave_initAll(void)
{
  // init variables
  quitFlag = FALSE;

//  Semaphore_init(&slaveList.lock);
//  List_init(&slaveList);

//TODO
#if 0
  // start slave thread
  if (!Thread_init(&slaveThread,"BAR slave",globalOptions.niceLevel,slaveThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize slave thread!");
  }
#endif

  return ERROR_NONE;
}

Slave_doneAll(void)
{
  // quit slave thread
  quitFlag = TRUE;
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
//  Thread_join(&slaveThread);

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
                     SlaveConnectStatusInfoFunction slaveConnectStatusInfoFunction,
                     void                           *slaveConnectStatusInfoUserData
                    )
{
  Errors error;

  assert(slaveInfo != NULL);
  assert(hostName != NULL);

  // slave connect
  error = slaveConnect(slaveInfo,
                       hostName,
                       hostPort
                      );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // init callback
  slaveInfo->slaveConnectStatusInfoFunction = slaveConnectStatusInfoFunction;
  slaveInfo->slaveConnectStatusInfoUserData = slaveConnectStatusInfoUserData;

  printInfo(2,"Connected slave host '%s:%d'\n",String_cString(hostName),hostPort);

  return ERROR_NONE;
}

void Slave_disconnect(SlaveInfo *slaveInfo)
{
  assert(slaveInfo != NULL);

  slaveDisconnect(slaveInfo);
}

// ----------------------------------------------------------------------

bool Slave_waitCommand(const SlaveInfo *slaveInfo,
                       long            timeout,
                       uint            *id,
                       String          name,
                       StringMap       argumentMap
                      )
{
  assert(slaveInfo != NULL);
  assert(id != NULL);
  assert(name != NULL);

  return ServerIO_waitCommand(&slaveInfo->io,
                              timeout,
                              id,
                              name,
                              argumentMap
                             );
}

//TODO
LOCAL Errors Slave_vexecuteCommand(SlaveInfo  *slaveInfo,
                                   long       timeout,
                                   StringMap  resultMap,
                                   const char *format,
                                   va_list    arguments
                                  )
{
  uint   id;
  Errors error;

  assert(slaveInfo != NULL);

  // init variables

#if 1
//TODO: lock
  // send command
  error = ServerIO_vsendCommand(&slaveInfo->io,
                                &id,
                                format,
                                arguments
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // wait for result
fprintf(stderr,"%s, %d: timeout=%ld\n",__FILE__,__LINE__,timeout);
  error = ServerIO_waitResult(&slaveInfo->io,
                              timeout,
                              id,
                              NULL,  // error,
                              NULL,  // completedFlag,
                              resultMap
                             );
  if (error != ERROR_NONE)
  {
    return error;
  }
//  printInfo(4,"Received slave result: error=%d completedFlag=%d data=%s\n",error,String_cString(data));
//fprintf(stderr,"%s, %d: received error=%d/%s: data=%s\n",__FILE__,__LINE__,Error_getCode(error),Error_getText(error),String_cString(data));

  // check error
  if (error != ERROR_NONE)
  {
    return error;
  }
#else
  error = ServerIO_vexecuteCommand(&slaveInfo->io,
                                   timeout,
                                   NULL,  // error,
                                   NULL,  // completedFlag,
                                   resultMap
                                  );
  if (error != ERROR_NONE)
  {
    return error;
  }
#endif

  // free resources

  return ERROR_NONE;
}

Errors Slave_executeCommand(const SlaveInfo *slaveInfo,
                            long            timeout,
                            StringMap       resultMap,
                            const char      *format,
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
//  bool            quitFlag;

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
fprintf(stderr,"%s, %d: *******************************\n",__FILE__,__LINE__);

  return ERROR_NONE;

  #undef SET_OPTION_BOOLEAN(name,value)
  #undef SET_OPTION_INTEGER64(name,value)
  #undef SET_OPTION_INTEGER(name,value)
  #undef SET_OPTION_PASSWORD(name,value)
  #undef SET_OPTION_CSTRING(name,value)
  #undef SET_OPTION_STRING(name,value)
}

Errors Slave_jobAbort(const SlaveInfo *slaveInfo,
                      ConstString     jobUUID
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

Errors Slave_process(const SlaveInfo *slaveInfo,
                     long            timeout
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
    if (ServerIO_waitCommand(&slaveInfo->io,
                             timeout,
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
                    "OK"
                   );
    }
  }
  while (error == ERROR_NONE);
//  ServerIO_wait(&slaveInfo->io,timeout);


  // free resources
  StringMap_delete(argumentMap);
  String_delete(name);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
