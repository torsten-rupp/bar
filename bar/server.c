/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/server.c,v $
* $Revision: 1.1 $
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
} ClientNode;

typedef struct
{
  LIST_HEADER(ClientNode);
} ClientList;

/***************************** Variables *******************************/
LOCAL ClientList clientList;
LOCAL bool       quitFlag;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

LOCAL Errors connectClient(const        String name,
                           SocketHandle socketHandle
                          )
{
  assert(name != NULL);

  return ERROR_NONE;
}

LOCAL void disconnectClient(ClientNode *clientNode)
{
  assert(clientNode != NULL);

  Network_disconnect(&clientNode->socketHandle);
}

LOCAL void freeClientNode(ClientNode *clientNode)
{
  assert(clientNode != NULL);

  String_delete(clientNode->name);
}

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

LOCAL void processCommand(String command)
{
}

/*---------------------------------------------------------------------*/

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
  ClientNode   *deleteClientNode;

  /* initialise variables */
  List_init(&clientList);
  quitFlag = FALSE;

  /* start server */
  error = Network_initServer(&serverSocketHandle,serverPort);
  if (error != ERROR_NONE)
  {
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

        List_append(&clientList,clientNode);

        info(1,"Connected client '%s:%u'\n",String_cString(clientNode->name),clientNode->port);
      }
      else
      {
      }
    }

    /* process client command/disconnect clients */
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

            clientNode = clientNode->next;
          }
          else
          {
            /* disconnect */
            info(1,"Disconnected client '%s:%u'\n",String_cString(clientNode->name),clientNode->port);

            deleteClientNode = clientNode;
            clientNode = clientNode->next;
            List_remove(&clientList,deleteClientNode);

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
