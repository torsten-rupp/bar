/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/server.c,v $
* $Revision: 1.3 $
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
#include "arrays.h"
#include "msgqueues.h"
#include "stringlists.h"

#include "bar.h"
#include "network.h"
#include "files.h"

//#include "server_parser.h"

#include "server.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define PROTOCOL_VERSION_MAJOR 1
#define PROTOCOL_VERSION_MINOR 0

/***************************** Datatypes *******************************/

typedef struct ClientNode
{
  NODE_HEADER(struct ClientNode);

  String       name;
  uint         port;
  SocketHandle socketHandle;
  bool         authentificationFlag;
  MsgQueue     commandMsgQueue;
  pthread_t    threadId;
  bool         exitFlag;
  String       commandString;
} ClientNode;

typedef struct
{
  LIST_HEADER(ClientNode);
} ClientList;

typedef void(*ServerCommandFunction)(ClientNode *clientNode, uint id, const String arguments[], uint argumentCount);

typedef struct
{
  ServerCommandFunction serverCommandFunction;
  uint                  id;
  Array                 arguments;
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
* Name   : getBooleanValue
* Purpose: get boolean value from string
* Input  : string - string
* Output : -
* Return : TRUE if string is either ", yes or true, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool getBooleanValue(const String s)
{
  return    String_equalsCString(s,"1")
         || String_equalsCString(s,"yes")
         || String_equalsCString(s,"true");
}

/***********************************************************************\
* Name   : sendResult
* Purpose: send result to client
* Input  : clientNode   - client node
*          id           - command id
*          completeFlag - TRUE if command is completed, FALSE otherwise
*          errorCode    - error code
*          format       - format string
*          ...          - optional arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void sendResult(ClientNode *clientNode, uint id, bool completeFlag, uint errorCode, const char *format, ...)
{
  String  result;
  va_list arguments;

  result = String_new();

  String_format(result,"%d %d %d ",id,completeFlag?1:0,errorCode);
  va_start(arguments,format);
  String_vformat(result,format,arguments);
  va_end(arguments);
  String_appendChar(result,'\n');

  Network_send(&clientNode->socketHandle,String_cString(result),String_length(result));
fprintf(stderr,"%s,%d: res=%s\n",__FILE__,__LINE__,String_cString(result));

  String_delete(result);
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : getTotalSubDirectorySize
* Purpose: get total size of files in sub-directory
* Input  : subDirectory - path name
* Output : -
* Return : total size of files in sub-directory (in bytes)
* Notes  : -
\***********************************************************************/

LOCAL uint64 getTotalSubDirectorySize(const String subDirectory)
{
  uint64          totalSize;
  StringList      pathNameList;
  String          pathName;
  DirectoryHandle directoryHandle;
  String          fileName;
  FileInfo        fileInfo;
  Errors          error;

  totalSize = 0;

  StringList_init(&pathNameList);
  StringList_append(&pathNameList,String_copy(subDirectory));
  pathName = String_new();
  fileName = String_new();
  while (!StringList_empty(&pathNameList))
  {
    pathName = StringList_getFirst(&pathNameList,pathName);

    error = File_openDirectory(&directoryHandle,pathName);
    if (error != ERROR_NONE)
    {
      continue;
    }

    while (!File_endOfDirectory(&directoryHandle))
    {
      error = File_readDirectory(&directoryHandle,fileName);
      if (error != ERROR_NONE)
      {
        continue;
      }

      error = File_getFileInfo(fileName,&fileInfo);
      if (error != ERROR_NONE)
      {
        continue;
      }

      switch (File_getType(fileName))
      {
        case FILETYPE_FILE:
          totalSize += fileInfo.size;
          break;
        case FILETYPE_DIRECTORY:
          StringList_append(&pathNameList,String_copy(fileName));
          break;
        case FILETYPE_LINK:
          break;
        default:
          break;
      }
    }

    File_closeDirectory(&directoryHandle);
  }
  String_delete(pathName);
  String_delete(fileName);
  StringList_done(&pathNameList,NULL);

  return totalSize;
}

/*---------------------------------------------------------------------*/

LOCAL void serverCommand_auth(ClientNode *clientNode, uint id, const String arguments[], uint argumentCount)
{
  assert(clientNode != NULL);
  assert(arguments != NULL);

  if (argumentCount < 1)
  {
    sendResult(clientNode,id,TRUE,1,"expected password");
    return;
  }

  sendResult(clientNode,id,TRUE,0,"");
}

LOCAL void serverCommand_deviceList(ClientNode *clientNode, uint id, const String arguments[], uint argumentCount)
{
  Errors       error;
  DeviceHandle deviceHandle;
  String       deviceName;

  assert(clientNode != NULL);
  assert(arguments != NULL);

  error = File_openDevices(&deviceHandle);
  if (error != ERROR_NONE)
  {
    sendResult(clientNode,id,TRUE,1,"cannot open device list (error: %s)",getErrorText(error));
    return;
  }

  deviceName = String_new();
  while (!File_endOfDevices(&deviceHandle))
  {
    error = File_readDevice(&deviceHandle,deviceName);
    if (error != ERROR_NONE)
    {
      sendResult(clientNode,id,TRUE,1,"cannot read device list (error: %s)",getErrorText(error));
      File_closeDevices(&deviceHandle);
      String_delete(deviceName);
      return;
    }

    sendResult(clientNode,id,FALSE,0,"%S",deviceName);
  }
  String_delete(deviceName);

  File_closeDevices(&deviceHandle);

  sendResult(clientNode,id,TRUE,0,"");
}

LOCAL void serverCommand_fileList(ClientNode *clientNode, uint id, const String arguments[], uint argumentCount)
{
  bool            totalSizeFlag;
  Errors          error;
  DirectoryHandle directoryHandle;
  String          fileName;
  FileInfo        fileInfo;
  uint64          totalSize;

  assert(clientNode != NULL);
  assert(arguments != NULL);

  if (argumentCount < 1)
  {
    sendResult(clientNode,id,TRUE,1,"expected path name");
    return;
  }
  totalSizeFlag = ((argumentCount >= 2) && getBooleanValue(arguments[1]));

  error = File_openDirectory(&directoryHandle,arguments[0]);
  if (error != ERROR_NONE)
  {
    sendResult(clientNode,id,TRUE,1,"cannot open '%S' (error: %s)",arguments[0],getErrorText(error));
    return;
  }

  fileName = String_new();
  while (!File_endOfDirectory(&directoryHandle))
  {
    error = File_readDirectory(&directoryHandle,fileName);
    if (error != ERROR_NONE)
    {
      sendResult(clientNode,id,TRUE,1,"cannot read directory '%S' (error: %s)",arguments[0],getErrorText(error));
      File_closeDirectory(&directoryHandle);
      String_delete(fileName);
      return;
    }

    error = File_getFileInfo(fileName,&fileInfo);
    if (error != ERROR_NONE)
    {
      sendResult(clientNode,id,TRUE,1,"cannot read file info of '%S' (error: %s)",fileName,getErrorText(error));
      File_closeDirectory(&directoryHandle);
      String_delete(fileName);
      return;
    }

    switch (File_getType(fileName))
    {
      case FILETYPE_FILE:
        sendResult(clientNode,id,FALSE,0,
                   "FILE %llu %S",
                   fileInfo.size,
                   fileName
                  );
        break;
      case FILETYPE_DIRECTORY:
        if (totalSizeFlag)
        {         
          totalSize = getTotalSubDirectorySize(fileName);
        }
        else
        {
          totalSize = 0;
        }
        sendResult(clientNode,id,FALSE,0,
                   "DIRECTORY %llu %S",
                   totalSize,
                   fileName
                  );
        break;
      case FILETYPE_LINK:
        sendResult(clientNode,id,FALSE,0,
                   "LINK %S",
                   fileName
                  );
        break;
      default:
        sendResult(clientNode,id,FALSE,0,
                   "unknown %S",
                   fileName
                  );
        break;
    }
  }
  String_delete(fileName);

  File_closeDirectory(&directoryHandle);

  sendResult(clientNode,id,TRUE,0,"");
}

#if 0
LOCAL uint serverCommand_new(ClientNode *clientNode, uint id, const String arguments[], uint argumentCoun)
{
  assert(clientNode != NULL);
  assert(arguments != NULL);

  return ERROR_NONE;
}

LOCAL uint serverCommand_include(ClientNode *clientNode, uint id, const String arguments[], uint argumentCount)
{
  assert(clientNode != NULL);
  assert(arguments != NULL);

  return ERROR_NONE;
}

LOCAL uint serverCommand_exclude(ClientNode *clientNode, uint id, const String arguments[], uint argumentCount)
{
  assert(clientNode != NULL);
  assert(arguments != NULL);

  return ERROR_NONE;
}

LOCAL uint serverCommand_start(ClientNode *clientNode, uint id, const String arguments[], uint argumentCount)
{
  assert(clientNode != NULL);
  assert(arguments != NULL);

  return ERROR_NONE;
}

LOCAL uint serverCommand_cancel(ClientNode *clientNode, uint id, const String arguments[], uint argumentCount)
{
  assert(clientNode != NULL);
  assert(arguments != NULL);

  return ERROR_NONE;
}
#endif /* 0 */

const struct { const char *name; ServerCommandFunction serverCommandFunction; } SERVER_COMMANDS[] =
{
  {"AUTH",        serverCommand_auth       },
  {"DEVICE_LIST", serverCommand_deviceList },
  {"FILE_LIST",   serverCommand_fileList   },
//  {"NEW",    serverCommand_new    },
//  {"INCLUDE",serverCommand_include},
//  {"EXCLUDE",serverCommand_exclude},
//  {"START",  serverCommand_start  },
//  {"CANCEL", serverCommand_cancel },
};

/***********************************************************************\
* Name   : freeArgumentsArrayElement
* Purpose: free argument array element
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeArgumentsArrayElement(String *string, void *userData)
{
  assert(string != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(*string);
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : parseCommand
* Purpose: parse command
* Input  : string - command
* Output : commandMsg - command message
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool parseCommand(CommandMsg *commandMsg,
                        String      string
                       )
{
  StringTokenizer stringTokenizer;
  String          token;
  uint            z;
  long            index;
  String          argument;

  assert(commandMsg != NULL);

  /* initialize tokenizer */
  String_initTokenizer(&stringTokenizer,string,STRING_WHITE_SPACES,"\"'",TRUE);

  /* get command */
  if (!String_getNextToken(&stringTokenizer,&token,NULL))
  {
    String_doneTokenizer(&stringTokenizer);
    return FALSE;
  }
  z = 0;
  while ((z < SIZE_OF_ARRAY(SERVER_COMMANDS)) && !String_equalsCString(token,SERVER_COMMANDS[z].name))
  {
    z++;
  }
  if (z >= SIZE_OF_ARRAY(SERVER_COMMANDS))
  {
    String_doneTokenizer(&stringTokenizer);
    return FALSE;
  }
  commandMsg->serverCommandFunction = SERVER_COMMANDS[z].serverCommandFunction;

  /* get id */
  if (!String_getNextToken(&stringTokenizer,&token,NULL))
  {
    String_doneTokenizer(&stringTokenizer);
    return FALSE;
  }
  commandMsg->id = String_toInteger(token,&index);
  if (index != STRING_END)
  {
    String_doneTokenizer(&stringTokenizer);
    return FALSE;
  }

  /* get arguments */
  commandMsg->arguments = Array_new(sizeof(String),0);
  while (String_getNextToken(&stringTokenizer,&token,NULL))
  {
    argument = String_copy(token);
    Array_append(commandMsg->arguments,&argument);
  }

  /* free resources */
  String_doneTokenizer(&stringTokenizer);

  return TRUE;
}

/***********************************************************************\
* Name   : freeCommand
* Purpose: free allocated command message
* Input  : commandMsg - command message
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeCommand(CommandMsg *commandMsg)
{
  assert(commandMsg != NULL);

  Array_delete(commandMsg->arguments,(ArrayElementFreeFunction)freeArgumentsArrayElement,NULL);
}

/***********************************************************************\
* Name   : clientThread
* Purpose: client processing thread
* Input  : clientNode - client node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clientThread(ClientNode *clientNode)
{
  CommandMsg commandMsg;
  String     result;

  assert(clientNode != NULL);

  result = String_new();
  while (   !clientNode->exitFlag
         && MsgQueue_get(&clientNode->commandMsgQueue,&commandMsg,NULL,sizeof(commandMsg))
        )
  {
    /* execute command */
    commandMsg.serverCommandFunction(clientNode,
                                     commandMsg.id,
                                     Array_cArray(commandMsg.arguments),
                                     Array_length(commandMsg.arguments)
                                    );

    /* free resources */
    freeCommand(&commandMsg);
  }
  String_delete(result);

  clientNode->exitFlag = TRUE;
}

/***********************************************************************\
* Name   : processCommand
* Purpose: process client command
* Input  : clientNode - client node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void processCommand(ClientNode *clientNode, const String command)
{
  CommandMsg commandMsg;

  assert(clientNode != NULL);

fprintf(stderr,"%s,%d: command=%s\n",__FILE__,__LINE__,String_cString(command));
  if (String_equalsCString(command,"VERSION"))
  {
    /* version info */
    sendResult(clientNode,0,TRUE,0,"%d %d\n",PROTOCOL_VERSION_MAJOR,PROTOCOL_VERSION_MINOR);
  }
else if (String_equalsCString(command,"QUIT"))
{
quitFlag = TRUE;
sendResult(clientNode,0,TRUE,0,"ok\n");
}
  else
  {
    /* parse command */
    if (!parseCommand(&commandMsg,command))
    {
      sendResult(clientNode,0,TRUE,1,"parse error");
      return;
    }

    /* send command to client thread */
    MsgQueue_put(&clientNode->commandMsgQueue,&commandMsg,sizeof(commandMsg));
  }
}

/*---------------------------------------------------------------------*/

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

  freeCommand(commandMsg);
}

/***********************************************************************\
* Name   : freeClientNode
* Purpose: free client node
* Input  : clientNode - client node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeClientNode(ClientNode *clientNode)
{
  assert(clientNode != NULL);

  /* stop client thread */
  clientNode->exitFlag = TRUE;
  MsgQueue_setEndOfMsg(&clientNode->commandMsgQueue);
  pthread_join(clientNode->threadId,NULL);

  /* delete */
  String_delete(clientNode->commandString);
  MsgQueue_done(&clientNode->commandMsgQueue,(MsgQueueMsgFreeFunction)freeCommandMsg,NULL);
  String_delete(clientNode->name);
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
        if (pthread_create(&clientNode->threadId,NULL,(void*(*)(void*))clientThread,clientNode) != 0)
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
            do
            {
              for (z = 0; z < receivedBytes; z++)
              {
                if (buffer[z] != '\n')
                {
                  String_appendChar(clientNode->commandString,buffer[z]);
                }
                else
                {
                  processCommand(clientNode,clientNode->commandString);
                  String_clear(clientNode->commandString);
                }
              }
              error = Network_receive(&clientNode->socketHandle,buffer,sizeof(buffer),&receivedBytes);
            }
            while ((error == ERROR_NONE) && (receivedBytes > 0));

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

            /* delete */
            freeClientNode(deleteClientNode);
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
