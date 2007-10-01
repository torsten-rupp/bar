/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/server.c,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: Backup ARchiver server
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>
#include <assert.h>

#include "global.h"
#include "lists.h"
#include "strings.h"
#include "msgqueues.h"

#include "network.h"
#include "bar.h"

//#include "server_parser.h"

#include "server.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
typedef struct ClientNode
{
  NODE_HEADER(struct ClientNode);

  String       name;
  uint         port;
  SocketHandle socketHandle;
  bool         authentificationFlag;
  MsgQueue     commandMsgQueue;
  pthread_t    thread;
  bool         exitFlag;
  String       commandString;
} ClientNode;

typedef struct
{
  LIST_HEADER(ClientNode);
} ClientList;

typedef struct
{
  String command;
} CommandMsg;

/***************************** Variables *******************************/
LOCAL ClientList clientList;
LOCAL bool       quitFlag;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors connectClient(const        String name,
                           SocketHandle socketHandle
                          )
{
  assert(name != NULL);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void disconnectClient(ClientNode *clientNode)
{
  assert(clientNode != NULL);

  Network_disconnect(&clientNode->socketHandle);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeClientNode(ClientNode *clientNode)
{
  assert(clientNode != NULL);

  String_delete(clientNode->name);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL ClientNode *findClient(const String name)
{
  ClientNode *clientNode;

  assert(name != NULL);

  clientNode = clientList.head;
  while (   (clientNode != NULL)
         && !String_equals(clientNode->name,name)
        )
  {
    clientNode = clientNode->next;
  }

  return clientNode;
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void processCommand(String command)
{
}

/***********************************************************************\
* Name   : process
* Purpose: client process thread
* Input  : clientNode - client node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void process(ClientNode *clientNode)
{
  CommandMsg commandMsg;

  assert(clientNode != NULL);

  while (   !clientNode->exitFlag
         && MsgQueue_get(&clientNode->commandMsgQueue,&commandMsg,NULL,sizeof(commandMsg))
        )
  {
fprintf(stderr,"%s,%d: command=%s\n",__FILE__,__LINE__,String_cString(commandMsg.command));

Network_send(&clientNode->socketHandle,"Hello\n",6);

    /* free resources */
    String_delete(commandMsg.command);
  }

  clientNode->exitFlag = TRUE;
}

/***********************************************************************\
* Name   : freeCommandMsg
* Purpose: free command msg
* Input  : commandMsg - command message
*          userData   - user data (ignored)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeCommandMsg(CommandMsg *commandMsg, void *userData)
{
  assert(commandMsg != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(commandMsg->command);
}

/*---------------------------------------------------------------------*/

Errors Server_init(void)
{
  return ERROR_NONE;
}

void Server_done(void)
{
}

bool Server_run(uint       serverPort,
                const char *serverPassword
               )
{
  Errors       error;
  SocketHandle serverSocketHandle;
  fd_set       selectSet;
  ClientNode   *clientNode;
  SocketHandle socketHandle;
  String       name;
  uint         port;
  char         buffer[256];
  ulong        receivedBytes;
  ulong        z;
  CommandMsg   commandMsg;
  ClientNode   *deleteClientNode;

  /* initialise variables */
  List_init(&clientList);
  quitFlag = FALSE;

  /* start server */
  error = Network_initServer(&serverSocketHandle,serverPort);
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize server (error: %s)!\n",
               getErrorText(error)
              );
    return FALSE;
  }

  /* run server */
  name = String_new();
  while (!quitFlag)
  {
    /* wait for command */
    FD_ZERO(&selectSet);
    FD_SET(Network_getSocket(&serverSocketHandle),&selectSet);
    clientNode = clientList.head;
    while (clientNode != NULL)
    {
      FD_SET(Network_getSocket(&clientNode->socketHandle),&selectSet);
      clientNode = clientNode->next;
    }
    select(FD_SETSIZE,&selectSet,NULL,NULL,NULL);

    /* connect new clients */
    if (FD_ISSET(Network_getSocket(&serverSocketHandle),&selectSet))
    {
      error = Network_accept(&socketHandle,
                             &serverSocketHandle
                            );
      if (error == ERROR_NONE)
      {
        Network_getRemoteInfo(&socketHandle,name,&port);

        clientNode = LIST_NEW_NODE(ClientNode);
        if (clientNode == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }
        clientNode->name                 = String_copy(name);
        clientNode->port                 = port;
        clientNode->socketHandle         = socketHandle;
        clientNode->authentificationFlag = FALSE;
        clientNode->exitFlag             = FALSE;
        clientNode->commandString        = String_new();
        if (!MsgQueue_init(&clientNode->commandMsgQueue,0))
        {
          HALT_FATAL_ERROR("Cannot initialise client command message queue!");
        }
        if (pthread_create(&clientNode->thread,NULL,(void*(*)(void*))process,clientNode) != 0)
        {
          HALT_FATAL_ERROR("Cannot initialise client thread!");
        }

        List_append(&clientList,clientNode);

        info(1,"Connected client '%s:%u'\n",String_cString(clientNode->name),clientNode->port);
      }
      else
      {
        printError("Cannot estable client connection (error: %s)!\n",
                   getErrorText(error)
                  );
      }
    }

    /* process client commands/disconnect clients */
    clientNode = clientList.head;
    while (clientNode != NULL)
    {
      if (FD_ISSET(Network_getSocket(&clientNode->socketHandle),&selectSet))
      {
        error = Network_receive(&clientNode->socketHandle,buffer,sizeof(buffer),&receivedBytes);
        if (error == ERROR_NONE)
        {
          if (receivedBytes > 0)
          {
            /* received data -> process */
            for (z = 0; z < receivedBytes; z++)
            {
              if (buffer[z] != '\n')
              {
                String_appendChar(clientNode->commandString,buffer[z]);
              }
              else
              {
                commandMsg.command = String_copy(clientNode->commandString);
                MsgQueue_put(&clientNode->commandMsgQueue,&commandMsg,sizeof(commandMsg));
                String_clear(clientNode->commandString);
              }
            }

            clientNode = clientNode->next;
          }
          else
          {
            /* disconnect */
            info(1,"Disconnected client '%s:%u'\n",String_cString(clientNode->name),clientNode->port);

            /* remove from list */
            deleteClientNode = clientNode;
            clientNode = clientNode->next;
            List_remove(&clientList,deleteClientNode);

            /* stop thread */
            deleteClientNode->exitFlag = TRUE;
            MsgQueue_setEndOfMsg(&deleteClientNode->commandMsgQueue);
            pthread_join(deleteClientNode->thread,NULL);

            /* delete */
            String_delete(deleteClientNode->commandString);
            MsgQueue_done(&deleteClientNode->commandMsgQueue,(MsgQueueMsgFreeFunction)freeCommandMsg,NULL);
            String_delete(deleteClientNode->name);
            LIST_DELETE_NODE(deleteClientNode);
          }
        }
        else
        {
          clientNode = clientNode->next;
        }
      }
      else
      {
        clientNode = clientNode->next;
      }
    }
  }
  String_delete(name);

  /* done server */
  Network_doneServer(&serverSocketHandle);

  /* free resources */
  List_done(&clientList,(ListNodeFreeFunction)freeClientNode,NULL);

  return TRUE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
