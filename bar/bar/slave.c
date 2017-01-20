/***********************************************************************\
*
* $Revision: 4126 $
* $Date: 2015-09-19 10:57:45 +0200 (Sat, 19 Sep 2015) $
* $Author: torsten $
* Contents: Backup ARchiver slave functions
* Systems: all
*
\***********************************************************************/

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
#define SLEEP_TIME_SLAVE_THREAD          (   10)  // [s]
#define READ_TIMEOUT (5LL*MS_PER_SECOND)

/***************************** Datatypes *******************************/

typedef struct CommandNode
{
  LIST_NODE_HEADER(struct CommandNode);

  uint   commandId;
  String name;
  String data;
} CommandNode;

typedef struct
{
  LIST_HEADER(CommandNode);

  Semaphore lock;
} CommandList;

typedef struct ResultNode
{
  LIST_NODE_HEADER(struct ResultNode);

  uint   commandId;
  Errors error;
  bool   completedFlag;
  String data;
} ResultNode;

typedef struct
{
  LIST_HEADER(ResultNode);

  Semaphore lock;
} ResultList;

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
* Name   : freeCommandNode
* Purpose: free command node
* Input  : commandNode - command node
*          userData    - not used
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeCommandNode(CommandNode *commandNode, void *userData)
{
  assert(commandNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(commandNode->data);
  String_delete(commandNode->name);
}

/***********************************************************************\
* Name   : freeResultNode
* Purpose: free result node
* Input  : resultNode - result node
*          userData   - not used
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeResultNode(ResultNode *resultNode, void *userData)
{
  assert(resultNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(resultNode->data);
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
                          uint         hostPort
                         )
{
  String line;
  Errors error;

  assert(slaveInfo != NULL);

  // init variables
  line = String_new();

  // connect to slave
  error = Network_connect(&slaveInfo->socketHandle,
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
    String_delete(line);
    return error;
  }
  String_set(slaveInfo->name,hostName);
  slaveInfo->port = hostPort;

  // authorize
  error = Network_readLine(&slaveInfo->socketHandle,line,30LL*MS_PER_SECOND);
  if (error != ERROR_NONE)
  {
    String_delete(line);
    return error;
  }

  // set connected state
  slaveInfo->isConnected = TRUE;

  // free resources
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

  Network_disconnect(&slaveInfo->socketHandle);
  slaveInfo->isConnected = FALSE;
}

#if 0
/***********************************************************************\
* Name   :
* Purpose:
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

//typedef bool(*StringMapParseEnumFunction)(const char *name, uint *value);
LOCAL bool parseEnumState(const char *name, uint *value)
{
//  if (stringEqualsIgnoreCase(name,"running")) value = SERVER_STATE_RUNNING;
//  else return FALSE;

  return TRUE;
}
#endif

LOCAL void processSlave(SlaveInfo *slaveInfo, ConstString line)
{
  uint                 commandId;
  bool                 completedFlag;
  Errors               error;
  String               name;
  String               data;
  ResultNode           *resultNode;
  CommandNode          *commandNode;
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
    resultNode = LIST_NEW_NODE(ResultNode);
    if (resultNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    resultNode->commandId     = commandId;
    resultNode->error         = (error != ERROR_NONE) ? Errorx_(error,0,"%s",String_cString(data)) : ERROR_NONE;
    resultNode->completedFlag = completedFlag;
    resultNode->data          = String_duplicate(data);

    // add result
    SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveInfo->resultList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      List_append(&slaveInfo->resultList,resultNode);
    }
fprintf(stderr,"%s, %d: %p added result commandId=%d data=%s %d\n",__FILE__,__LINE__,slaveInfo,commandId,String_cString(data),slaveInfo->resultList.count);
//    Semaphore_signalModified(&slaveList.lock);
  }
  else if (!String_parse(line,STRING_BEGIN,"%u %S % S",NULL,&commandId,name,data))
  {
    // init command
    resultNode = LIST_NEW_NODE(ResultNode);
    if (resultNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    commandNode->commandId = commandId;
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
    SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveInfo->commandList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      List_append(&slaveInfo->commandList,commandNode);
    }
fprintf(stderr,"%s, %d: %p added command %s %d\n",__FILE__,__LINE__,slaveInfo,commandId,String_cString(line),slaveInfo->commandList.count);
//    Semaphore_signalModified(&slaveList.lock);
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

LOCAL bool slaveWait2(SlaveInfo *slaveInfo, long timeout)
{
  struct pollfd   pollfds[1];
  struct timespec pollTimeout;
  sigset_t        signalMask;
  char            buffer[4096];
  ulong           receivedBytes;
  ulong           i;
  Errors          error;

  assert(slaveInfo != NULL);
//  assert(Semaphore_isLocked(&slaveList.lock));

  if (slaveInfo->isConnected)
  {
    // wait for data from slave
    pollfds[0].fd     = Network_getSocket(&slaveInfo->socketHandle);
    pollfds[0].events = POLLIN|POLLERR|POLLNVAL;
  }
  else
  {
    // slave not connected
fprintf(stderr,"%s, %d: disconnected????\n",__FILE__,__LINE__);
    return FALSE;
  }

  // wait for data from slave
  pollTimeout.tv_sec  = timeout/MS_PER_SECOND;
  pollTimeout.tv_nsec = (timeout%MS_PER_SECOND)*NS_PER_MS;
  if (ppoll(pollfds,1,&pollTimeout,&signalMask) <= 0)
  {
fprintf(stderr,"%s, %d: poll fail\n",__FILE__,__LINE__);
return FALSE;
  }

  // process slave results/commands
  if ((pollfds[0].revents & POLLIN) != 0)
  {
    // received data
    Network_receive(&slaveInfo->socketHandle,buffer,sizeof(buffer),NO_WAIT,&receivedBytes);
//fprintf(stderr,"%s, %d: buffer=%s\n",__FILE__,__LINE__,buffer);
    if (receivedBytes > 0)
    {
      do
      {
        // received data -> process
        for (i = 0; i < receivedBytes; i++)
        {
          if (buffer[i] != '\n')
          {
            String_appendChar(slaveInfo->line,buffer[i]);
          }
          else
          {
//fprintf(stderr,"%s, %d: process %s \n",__FILE__,__LINE__,String_cString(slaveInfo->line));
            processSlave(slaveInfo,slaveInfo->line);
//fprintf(stderr,"%s, %d: process done\n",__FILE__,__LINE__);
            String_clear(slaveInfo->line);
          }
        }
        error = Network_receive(&slaveInfo->socketHandle,buffer,sizeof(buffer),NO_WAIT,&receivedBytes);
      }
      while ((error == ERROR_NONE) && (receivedBytes > 0));
    }
    else
    {
      // disconnect
      slaveDisconnect(slaveInfo);

      printInfo(1,"Disconnected slave '%s:%u'\n",String_cString(slaveInfo->name),slaveInfo->port);
    }
  }
  else if ((pollfds[0].revents & (POLLERR|POLLNVAL)) != 0)
  {
    // error/disconnect
    slaveDisconnect(slaveInfo);

    printInfo(1,"Disconnected slave '%s:%u'\n",String_cString(slaveInfo->name),slaveInfo->port);
  }

  return TRUE;
}

LOCAL bool slaveWait(SlaveInfo *slaveInfo, long timeout)
{
  SemaphoreLock   semaphoreLock;
  struct pollfd   pollfds[1];
  struct timespec pollTimeout;
  sigset_t        signalMask;
  char            buffer[4096];
  ulong           receivedBytes;
  ulong           i;
  Errors          error;

//  assert(Semaphore_isLocked(&slaveList.lock));

  if (slaveInfo->isConnected)
  {
    // wait for data from slave
    pollfds[0].fd     = Network_getSocket(&slaveInfo->socketHandle);
    pollfds[0].events = POLLIN|POLLERR|POLLNVAL;
  }
  else
  {
    // slave not connected
    return FALSE;
  }

  // wait for data from slave
  pollTimeout.tv_sec  = timeout/MS_PER_SECOND;
  pollTimeout.tv_nsec = (timeout%MS_PER_SECOND)*NS_PER_MS;
fprintf(stderr,"%s, %d: wait...\n",__FILE__,__LINE__);
  if (ppoll(pollfds,1,&pollTimeout,&signalMask) <= 0)
  {
return FALSE;
  }
fprintf(stderr,"%s, %d: wait done -------\n",__FILE__,__LINE__);

  // process slave results/commands
  if ((pollfds[0].revents & POLLIN) != 0)
  {
    // received data
    Network_receive(&slaveInfo->socketHandle,buffer,sizeof(buffer),NO_WAIT,&receivedBytes);
//fprintf(stderr,"%s, %d: buffer=%s\n",__FILE__,__LINE__,buffer);
    if (receivedBytes > 0)
    {
      do
      {
        // received data -> process
        for (i = 0; i < receivedBytes; i++)
        {
          if (buffer[i] != '\n')
          {
            String_appendChar(slaveInfo->line,buffer[i]);
          }
          else
          {
//fprintf(stderr,"%s, %d: process %s \n",__FILE__,__LINE__,String_cString(slaveInfo->line));
            processSlave(slaveInfo,slaveInfo->line);
//fprintf(stderr,"%s, %d: process done\n",__FILE__,__LINE__);
            String_clear(slaveInfo->line);
          }
        }
        error = Network_receive(&slaveInfo->socketHandle,buffer,sizeof(buffer),NO_WAIT,&receivedBytes);
      }
      while ((error == ERROR_NONE) && (receivedBytes > 0));
    }
    else
    {
      // disconnect
//                  deleteClient(disconnectClientNode);
fprintf(stderr,"%s, %d: xxxxxxxxxxxxxxx\n",__FILE__,__LINE__);
      slaveInfo->isConnected = FALSE;

      printInfo(1,"Disconnected slave '%s:%u'\n",String_cString(slaveInfo->name),slaveInfo->port);
    }
  }
  else if ((pollfds[0].revents & (POLLERR|POLLNVAL)) != 0)
  {
    // error/disconnect

    // done client and free resources
//                deleteClient(disconnectClientNode);
fprintf(stderr,"%s, %d: xxxxerrr\n",__FILE__,__LINE__);
    slaveInfo->isConnected = FALSE;

    printInfo(1,"Disconnected slave '%s:%u'\n",String_cString(slaveInfo->name),slaveInfo->port);
  }

  return TRUE;
}

LOCAL CommandNode *slaveWaitCommand(SlaveInfo *slaveInfo, long timeout)
{
  CommandNode   *commandNode;
  SemaphoreLock semaphoreLock;

  assert(slaveInfo != NULL);
//  assert(Semaphore_isLocked(&slaveList.lock));

  commandNode = NULL;

fprintf(stderr,"%s, %d: slaveWaitCommand\n",__FILE__,__LINE__);
  SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveInfo->commandList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // wait for some result
    while (List_isEmpty(&slaveInfo->commandList))
    {
      if (!slaveWait2(slaveInfo,timeout))
      {
fprintf(stderr,"%s, %d: slaveWaitCommand timeout %ld\n",__FILE__,__LINE__,timeout);
        Semaphore_unlock(&slaveInfo->commandList.lock);
        return NULL;
      }
    }

    // get next command
    commandNode = List_removeFirst(&slaveInfo->commandList);
  }

  return commandNode;
}

LOCAL ResultNode *slaveWaitResult(SlaveInfo *slaveInfo, long timeout)
{
  ResultNode    *resultNode;
  SemaphoreLock semaphoreLock;

  assert(slaveInfo != NULL);
//  assert(Semaphore_isLocked(&slaveList.lock));

  resultNode = NULL;

fprintf(stderr,"%s, %d: slaveWaitResult timeout=%ld\n",__FILE__,__LINE__,timeout);
  SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveInfo->resultList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // wait for some result
    while (List_isEmpty(&slaveInfo->resultList))
    {
      if (!slaveWait2(slaveInfo,timeout))
      {
fprintf(stderr,"%s, %d: slaveWaitResult timeout %ld\n",__FILE__,__LINE__,timeout);
        Semaphore_unlock(&slaveInfo->resultList.lock);
        return NULL;
      }
    }

    // get next result
    resultNode = List_removeFirst(&slaveInfo->resultList);
  }

  return resultNode;
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
  commandId = slaveInfo->commandId;
  slaveInfo->commandId++;

  // format command

  // send data
  if (!quitFlag)
  {
//    SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveInfo->network.writeLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      (void)Network_send(&slaveInfo->socketHandle,String_cString(data),String_length(data));
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

  return Slave_executeCommand(slaveInfo,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%d",jobUUID,name,value);
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

  return Slave_executeCommand(slaveInfo,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%lld",jobUUID,name,value);
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

  return Slave_executeCommand(slaveInfo,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%y",jobUUID,name,value);
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

  return Slave_executeCommand(slaveInfo,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'S",jobUUID,name,value);
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

  return Slave_executeCommand(slaveInfo,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'s",jobUUID,name,value);
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
  error = Slave_executeCommand(slaveInfo,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'s",jobUUID,name,plainPassword);
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

  slaveInfo->name                           = String_new();
  slaveInfo->port                           = 0;
//  slaveInfo->forceSSL                        = forceSSL;
  slaveInfo->isConnected                    = FALSE;
  slaveInfo->slaveConnectStatusInfoFunction = NULL;
  slaveInfo->slaveConnectStatusInfoUserData = NULL;
  slaveInfo->line                           = String_new();
  slaveInfo->commandId                      = 0;
  Semaphore_init(&slaveInfo->commandList.lock);
  List_init(&slaveInfo->commandList);
  Semaphore_init(&slaveInfo->resultList.lock);
  List_init(&slaveInfo->resultList);
}

void Slave_duplicate(SlaveInfo *slaveInfo, const SlaveInfo *fromSlaveInfo)
{
  Slave_init(slaveInfo);

  String_set(slaveInfo->name,fromSlaveInfo->name);
  slaveInfo->port                           = fromSlaveInfo->port;
  slaveInfo->slaveConnectStatusInfoFunction = fromSlaveInfo->slaveConnectStatusInfoFunction;
  slaveInfo->slaveConnectStatusInfoUserData = fromSlaveInfo->slaveConnectStatusInfoUserData;
}

void Slave_done(SlaveInfo *slaveInfo)
{
  assert(slaveInfo != NULL);

  slaveInfo->isConnected = FALSE;
  List_done(&slaveInfo->resultList,CALLBACK(freeResultNode,NULL));
  Semaphore_done(&slaveInfo->resultList.lock);
  List_done(&slaveInfo->commandList,CALLBACK(freeCommandNode,NULL));
  Semaphore_done(&slaveInfo->commandList.lock);
  String_delete(slaveInfo->name);
}

Errors Slave_connect(SlaveInfo                      *slaveInfo,
                     ConstString                    hostName,
                     uint                           hostPort,
                     SlaveConnectStatusInfoFunction slaveConnectStatusInfoFunction,
                     void                           *slaveConnectStatusInfoUserData
                    )
{
  String        line;
  SocketHandle  socketHandle;
  Errors        error;
  bool          sslFlag;
  SemaphoreLock semaphoreLock;

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

void Slave_disconnect(const SlaveInfo *slaveInfo)
{
  assert(slaveInfo != NULL);

  slaveDisconnect(slaveInfo);
}

bool Slave_isConnected(const SlaveInfo *slaveInfo)
{
  bool          isConnected;
  SemaphoreLock semaphoreLock;

  assert(slaveInfo != NULL);

  isConnected = FALSE;

  // find slave and check if connected, remove from list if not connected
  isConnected = slaveInfo->isConnected;
#if 0
      if (Network_isConnected(&slaveInfo->socketHandle))
      {
        isConnected = TRUE;
      }
      else
      {
        List_remove(&slaveList,slaveInfo);
      }
#endif

#if 0
  // disconnect and discard not connected slave
  if ((slaveInfo != NULL) && !isConnected)
  {
    Network_disconnect(&slaveInfo->socketHandle);

    String_delete(slaveInfo->hostName);
    LIST_DELETE_NODE(slaveInfo);
  }
#endif

  return isConnected;
}

Errors Slave_getCommand(const SlaveInfo *slaveInfo,
                        uint            *commandId,
                        String          name,
                        String          data,
                        long            timeout
                       )
{
  SemaphoreLock semaphoreLock;
  SemaphoreLock semaphoreLockCommandList;
  CommandNode   *commandNode;

  assert(slaveInfo != NULL);
  assert(commandId != NULL);
  assert(name != NULL);
  assert(data != NULL);

  // wait for command
  commandNode = slaveWaitCommand(slaveInfo,30LL*MS_PER_SECOND);
  if (commandNode == NULL)
  {
//    Semaphore_unlock(&slaveList.lock);
    return ERROR_NETWORK_TIMEOUT;
  }

  // get command
  (*commandId) = commandNode->commandId;
  String_set(name,commandNode->name);
  String_set(data,commandNode->data);

  // free resources
  freeCommandNode(commandNode,NULL);
  LIST_DELETE_NODE(commandNode);

  return ERROR_NONE;
}

//TODO
LOCAL Errors Slave_vexecuteCommand(SlaveInfo  *slaveInfo,
                                   StringMap  resultMap,
                                   const char *format,
                                   va_list    arguments
                                  )
{
  String         line;
  String         data;
  SemaphoreLock  semaphoreLock;
  SocketHandle   socketHandle;
  bool           sslFlag;
  locale_t       locale;
  ResultNode     *resultNode;
  uint           commandId;
  Errors         error;

  assert(slaveInfo != NULL);

  // init variables
  line = String_new();
  data = String_new();

#if 0
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
  // check if slave known, try to connect
  if (slaveInfo == NULL)
  {
    // try to connect
    error = slaveConnect(&socketHandle,slaveInfo->name,slaveInfo->port,slaveInfo->forceSSL);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&slaveList.lock);
      String_delete(data);
      String_delete(line);
      return error;
    }
//TODO
sslFlag = TRUE;

    // add slave
    slaveInfo = LIST_NEW_NODE(SlaveNode);
    if (slaveNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    slaveNode->hostName      = String_duplicate(slaveInfo->name);
    slaveNode->hostPort      = slaveInfo->port;
    slaveNode->sslFlag       = sslFlag;
    slaveNode->commandId     = 0;
    slaveNode->commandString = String_new();
    slaveNode->socketHandle  = socketHandle;
    slaveNode->isConnected   = TRUE;
    List_append(&slaveList,slaveNode);
  }
#endif

//TODO: lock
  // create new command id
  slaveInfo->commandId++;

  // format command
  locale = uselocale(POSIXLocale);
  {
    String_format(line,"%d ",slaveInfo->commandId);
    String_vformat(line,format,arguments);
    String_appendChar(line,'\n');
  }
  uselocale(locale);

  // send command
  (void)Network_send(&slaveInfo->socketHandle,String_cString(line),String_length(line));
  printInfo(4,"Sent slave command: %s",String_cString(line));
fprintf(stderr,"%s, %d: sent %s\n",__FILE__,__LINE__,String_cString(line));

  // wait for result
  do
  {
    // wait for result
    resultNode = slaveWaitResult(slaveInfo,30LL*MS_PER_SECOND);
    if (resultNode == NULL)
    {
      String_delete(data);
      String_delete(line);
      return ERROR_NETWORK_TIMEOUT;
    }

    // get result
    commandId = resultNode->commandId;
    error     = resultNode->error;
    String_set(data,resultNode->data);

    // free resources
    freeResultNode(resultNode,NULL);
    LIST_DELETE_NODE(resultNode);
  }
  while (resultNode->commandId != slaveInfo->commandId);
  printInfo(4,"Received slave result: error=%d completedFlag=%d data=%s\n",error,String_cString(data));
fprintf(stderr,"%s, %d: received error=%d/%s: data=%s\n",__FILE__,__LINE__,Error_getCode(error),Error_getText(error),String_cString(data));

  // check error
  if (error != ERROR_NONE)
  {
    String_delete(data);
    String_delete(line);
    return error;
  }

  // parse result
  if (resultMap != NULL)
  {
    StringMap_clear(resultMap);
    if (!StringMap_parse(resultMap,data,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,STRING_BEGIN,NULL))
    {
      String_delete(data);
      String_delete(line);
      return ERROR_INVALID_RESPONSE;
    }
  }

  // free resources
  String_delete(data);
  String_delete(line);

  return ERROR_NONE;
}

Errors Slave_executeCommand(const SlaveInfo *slaveInfo,
                            StringMap       resultMap,
                            const char      *format,
                            ...
                           )
{
  va_list  arguments;
  Errors   error;

  assert(slaveInfo != NULL);

  va_start(arguments,format);
  error = Slave_vexecuteCommand(slaveInfo,resultMap,format,arguments);
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
  error = Slave_executeCommand(slaveInfo,NULL,"JOB_NEW name=%'S jobUUID=%S master=%'S",name,jobUUID,Network_getHostName(s));
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
  if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,NULL,"INCLUDE_LIST_CLEAR jobUUID=%S",jobUUID);
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
                                                          NULL,
                                                          "INCLUDE_LIST_ADD jobUUID=%S entryType=%s patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          entryTypeText,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,entryNode->patternType,NULL),
                                                          entryNode->string
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,NULL,"EXCLUDE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(excludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,
                                                          NULL,
                                                          "EXCLUDE_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->patternType,NULL),
                                                          patternNode->string
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,NULL,"MOUNT_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(mountList,mountNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,
                                                          NULL,
                                                          "MOUNT_LIST_ADD jobUUID=%S name=%'S alwaysUnmount=%y",
                                                          jobUUID,
                                                          mountNode->name,
                                                          mountNode->alwaysUnmount
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,NULL,"EXCLUDE_COMPRESS_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(compressExcludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,
                                                          NULL,
                                                          "EXCLUDE_COMPRESS_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->patternType,NULL),
                                                          patternNode->string
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,NULL,"SOURCE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(deltaSourceList,deltaSourceNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveInfo,
                                                          NULL,
                                                          "SOURCE_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,deltaSourceNode->patternType,NULL),
                                                          deltaSourceNode->storageName
                                                         );
  }
  if (error != ERROR_NONE)
  {
    (void)Slave_executeCommand(slaveInfo,NULL,"JOB_DELETE jobUUID=%S",jobUUID);
    String_delete(s);
    return error;
  }
fprintf(stderr,"%s, %d: %d: Slave_jobStart %s\n",__FILE__,__LINE__,error,Error_getText(error));

  // start execute job
  error = Slave_executeCommand(slaveInfo,NULL,"JOB_START jobUUID=%S archiveType=%s dryRun=%y",jobUUID,"FULL"/*archiveType*/,FALSE);
  if (error != ERROR_NONE)
  {
    (void)Slave_executeCommand(slaveInfo,NULL,"JOB_DELETE jobUUID=%S",jobUUID);
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
  error = Slave_executeCommand(slaveInfo,NULL,"JOB_ABORT jobUUID=%S",jobUUID);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // free resources

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
