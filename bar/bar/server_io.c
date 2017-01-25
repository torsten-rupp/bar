/***********************************************************************\
*
* $Revision: 7108 $
* $Date: 2017-01-20 17:08:03 +0100 (Fri, 20 Jan 2017) $
* $Author: torsten $
* Contents: Backup ARchiver server
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <locale.h>
//#include <time.h>
//#include <signal.h>
#include <assert.h>

#include "global.h"
#include "lists.h"
#include "semaphores.h"
#include "strings.h"
#include "stringmaps.h"
#include "files.h"
#include "network.h"

#include "bar.h"
#include "crypt.h"

#include "server_io.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define SESSION_KEY_SIZE                         1024     // number of session key bits
#define LOCK_TIMEOUT                             (10*60*1000)  // general lock timeout [ms]

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : freeCommandNode
* Purpose: free server i/o command node
* Input  : commandNode - command node
*          userData    - not used
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeCommandNode(ServerIOCommandNode *commandNode, void *userData)
{
  assert(commandNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(commandNode->data);
  String_delete(commandNode->name);
}

/***********************************************************************\
* Name   : freeResultNode
* Purpose: free server i/o result node
* Input  : resultNode - result node
*          userData   - not used
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeResultNode(ServerIOResultNode *resultNode, void *userData)
{
  assert(resultNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(resultNode->data);
}

/***********************************************************************\
* Name   : deleteResultNode
* Purpose: delete result node
* Input  : resultNode - result node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deleteResultNode(ServerIOResultNode *resultNode)
{
  assert(resultNode != NULL);

  freeResultNode(resultNode,NULL);
  LIST_DELETE_NODE(resultNode);
}

/***********************************************************************\
* Name   : deleteCommandNode
* Purpose: delete command node
* Input  : commandNode - command node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deleteCommandNode(ServerIOCommandNode *commandNode)
{
  assert(commandNode != NULL);

  freeCommandNode(commandNode,NULL);
  LIST_DELETE_NODE(commandNode);
}

/***********************************************************************\
* Name   : encodeHex
* Purpose: encoded data as hex-string
* Input  : string     - string variable
*          data       - data to encode
*          dataLength - length of data [bytes]
* Output : string - string
* Return : string
* Notes  : -
\***********************************************************************/

LOCAL String encodeHex(String string, const byte *data, uint length)
{
  uint z;

  assert(string != NULL);

  for (z = 0; z < length; z++)
  {
    String_format(string,"%02x",data[z]);
  }

  return string;
}

/***********************************************************************\
* Name   : decodeHex
* Purpose: decode hex-string into data
* Input  : s             - hex-string
*          maxDataLength - max. data length  [bytes]
* Output : data       - data
*          dataLength - length of data [bytes]
* Return : TRUE iff data decoded
* Notes  : -
\***********************************************************************/

LOCAL bool decodeHex(const char *s, byte *data, uint *dataLength, uint maxDataLength)
{
  uint i;
  char t[3];
  char *w;

  assert(s != NULL);
  assert(data != NULL);
  assert(dataLength != NULL);

  i = 0;
  while (((*s) != '\0') && (i < maxDataLength))
  {
    t[0] = (*s); s++;
    if ((*s) != '\0')
    {
      t[1] = (*s); s++;
      t[2] = '\0';

      data[i] = (byte)strtol(t,&w,16);
      if ((*w) != '\0') return FALSE;
      i++;
    }
    else
    {
      return FALSE;
    }
  }
  (*dataLength) = i;

  return TRUE;
}

/***********************************************************************\
* Name   : sendData
* Purpose: send data
* Input  : serverIO - server i/o
*          data     - data string
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void sendData(ServerIO *serverIO, ConstString data)
{
  SemaphoreLock semaphoreLock;

  assert(serverIO != NULL);
  assert(data != NULL);

//  if (!serverIO->quitFlag)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&serverIO->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      switch (serverIO->type)
      {
        case SERVER_IO_TYPE_NONE:
          break;
        case SERVER_IO_TYPE_BATCH:
          (void)File_write(&serverIO->file.fileHandle,String_cString(data),String_length(data));
          (void)File_flush(&serverIO->file.fileHandle);
          break;
        case SERVER_IO_TYPE_NETWORK:
          (void)Network_send(&serverIO->network.socketHandle,String_cString(data),String_length(data));
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }
    }
  }
}

LOCAL void processData(ServerIO *serverIO, ConstString line)
{
  uint                 id;
  bool                 completedFlag;
  Errors               error;
  String               name;
  String               data;
  ServerIOResultNode   *resultNode;
  ServerIOCommandNode  *commandNode;
  SemaphoreLock        semaphoreLock;
  uint                 i;
  StringMap            argumentMap;

  assert(serverIO != NULL);
  assert(line != NULL);

  // init variables
  name = String_new();
  data = String_new();

  // parse
  if      (String_parse(line,STRING_BEGIN,"%u %y %u % S",NULL,&id,&completedFlag,&error,data))
  {
    // result
    #ifndef NDEBUG
      if (globalOptions.serverDebugFlag)
      {
        fprintf(stderr,"DEBUG: receive result #%u %d %d: %s\n",id,completedFlag,error,String_cString(data));
      }
    #endif /* not DEBUG */

    // init result
    resultNode = LIST_NEW_NODE(ServerIOResultNode);
    if (resultNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    resultNode->id            = id;
    resultNode->error         = (error != ERROR_NONE) ? Errorx_(error,0,"%s",String_cString(data)) : ERROR_NONE;
    resultNode->completedFlag = completedFlag;
    resultNode->data          = String_duplicate(data);

    // add result
    SEMAPHORE_LOCKED_DO(semaphoreLock,&serverIO->resultList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      List_append(&serverIO->resultList,resultNode);
    }
  }
  else if (String_parse(line,STRING_BEGIN,"%u %S % S",NULL,&id,name,data))
  {
    // command
    #ifndef NDEBUG
      if (globalOptions.serverDebugFlag)
      {
        fprintf(stderr,"DEBUG: receive command #%u %s: %s\n",id,String_cString(name),String_cString(data));
      }
    #endif /* not DEBUG */

    // init command
    commandNode = LIST_NEW_NODE(ServerIOCommandNode);
    if (commandNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    commandNode->id   = id;
    commandNode->name = String_duplicate(name);
    commandNode->data = String_duplicate(data);

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
    SEMAPHORE_LOCKED_DO(semaphoreLock,&serverIO->commandList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      List_append(&serverIO->commandList,commandNode);
    }
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

LOCAL bool receiveData(ServerIO *serverIO, long timeout)
{
  struct pollfd   pollfds[1];
  struct timespec pollTimeout;
  sigset_t        signalMask;
  char            buffer[4096];
  ulong           receivedBytes;
  ulong           i;
  Errors          error;

  assert(serverIO != NULL);

  // wait for data from slave
  pollfds[0].fd       = Network_getSocket(&serverIO->network.socketHandle);
  pollfds[0].events   = POLLIN|POLLERR|POLLNVAL;
  pollTimeout.tv_sec  = timeout/MS_PER_SECOND;
  pollTimeout.tv_nsec = (timeout%MS_PER_SECOND)*NS_PER_MS;
  if (ppoll(pollfds,1,&pollTimeout,&signalMask) <= 0)
  {
fprintf(stderr,"%s, %d: poll fail %s\n",__FILE__,__LINE__,strerror(errno));
return FALSE;
  }

  // process data results/commands
  if ((pollfds[0].revents & POLLIN) != 0)
  {
    // received data
    Network_receive(&serverIO->network.socketHandle,buffer,sizeof(buffer),NO_WAIT,&receivedBytes);
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
            String_appendChar(serverIO->line,buffer[i]);
          }
          else
          {
            processData(serverIO,serverIO->line);
            String_clear(serverIO->line);
          }
        }
        error = Network_receive(&serverIO->network.socketHandle,buffer,sizeof(buffer),NO_WAIT,&receivedBytes);
      }
      while ((error == ERROR_NONE) && (receivedBytes > 0));
    }
    else
    {
      // disconnect
      return FALSE;
    }
  }
  else if ((pollfds[0].revents & (POLLERR|POLLNVAL)) != 0)
  {
    // error/disconnect
    return FALSE;
  }

  return TRUE;
}

#if 0
/***********************************************************************\
* Name   : clientAction
* Purpose: execute client action
* Input  : serverIO  - server i/o
*          id            - command id
*          resultMap     - result map variable
*          actionCommand - action command
*          timeout       - timeout or WAIT_FOREVER
*          format        - arguments format string
*          ...           - optional arguments
* Output : resultMap - results
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors sendAction(ServerIO *serverIO, uint id, StringMap resultMap, const char *actionCommand, long timeout, const char *format, ...)
{
  String        action;
  locale_t      locale;
  va_list       arguments;
  SemaphoreLock semaphoreLock;
  Errors        error;

  assert(serverIO != NULL);
  assert(actionCommand != NULL);

  error = ERROR_UNKNOWN;

  // send action
  action = String_new();
  locale = uselocale(POSIXLocale);
  {
    String_format(action,"%u 0 0 action=%s ",id,actionCommand);
    va_start(arguments,format);
    String_vformat(action,format,arguments);
    va_end(arguments);
    #ifndef NDEBUG
      if (globalOptions.serverDebugFlag)
      {
        fprintf(stderr,"DEBUG: send action '%s'\n",String_cString(action));
      }
    #endif /* not DEBUG */
    String_appendChar(action,'\n');
  }
  uselocale(locale);

  sendData(serverIO,action);

  // free resources
  String_delete(action);

#if 0
  // wait for result, timeout, or quit
  while ((serverIO->action.error == ERROR_UNKNOWN) && !serverIO->quitFlag)
  {
    if (!Semaphore_waitModified(&serverIO->action.lock,timeout))
    {
      Semaphore_unlock(&serverIO->action.lock);
      return ERROR_NETWORK_TIMEOUT;
    }
  }
  if (serverIO->quitFlag)
  {
    Semaphore_unlock(&serverIO->action.lock);
    return ERROR_ABORTED;
  }

  // get action result
  error = serverIO->action.error;
  if (resultMap != NULL)
  {
    StringMap_move(resultMap,serverIO->action.resultMap);
  }
  else
  {
    StringMap_clear(serverIO->action.resultMap);
  }
#endif

  return error;
}
#endif

// ----------------------------------------------------------------------

void ServerIO_init(ServerIO *serverIO)
{
  assert(serverIO != NULL);

  Semaphore_init(&serverIO->lock);
  #ifndef NO_SESSION_ID
    Crypt_randomize(serverIO->sessionId,sizeof(SessionId));
  #else /* not NO_SESSION_ID */
    memset(serverIO->sessionId,0,sizeof(SessionId));
  #endif /* NO_SESSION_ID */
  (void)Crypt_createPublicPrivateKeyPair(&serverIO->publicKey,
                                         &serverIO->privateKey,
                                         SESSION_KEY_SIZE,
                                         CRYPT_PADDING_TYPE_PKCS1,
                                         CRYPT_KEY_MODE_TRANSIENT
                                        );
  serverIO->commandId = 0;
  serverIO->line = String_new();
  serverIO->type = SERVER_IO_TYPE_NONE;
  Semaphore_init(&serverIO->commandList.lock);
  List_init(&serverIO->commandList);
  Semaphore_init(&serverIO->resultList.lock);
  List_init(&serverIO->resultList);
}

void ServerIO_done(ServerIO *serverIO)
{
  assert(serverIO != NULL);

  List_done(&serverIO->resultList,CALLBACK(freeResultNode,NULL));
  Semaphore_done(&serverIO->resultList.lock);
  List_done(&serverIO->commandList,CALLBACK(freeCommandNode,NULL));
  Semaphore_done(&serverIO->commandList.lock);
  String_delete(serverIO->line);
  Crypt_doneKey(&serverIO->privateKey);
  Crypt_doneKey(&serverIO->publicKey);
  Semaphore_done(&serverIO->lock);
}

Errors ServerIO_initBatch(ServerIO   *serverIO,
                          FileHandle fileHandle
                         )
{
  assert(serverIO != NULL);
  assert(serverIO->type == SERVER_IO_TYPE_NONE);

  serverIO->type            = SERVER_IO_TYPE_BATCH;
  serverIO->file.fileHandle = fileHandle;

  String_clear(serverIO->line);
  List_clear(&serverIO->commandList,CALLBACK(freeCommandNode,NULL));
  List_clear(&serverIO->resultList,CALLBACK(freeResultNode,NULL));

  return ERROR_NONE;
}

Errors ServerIO_initNetwork(ServerIO     *serverIO,
                            ConstString  hostName,
                            uint         hostPort,
                            SocketHandle socketHandle
                           )
{
  assert(serverIO != NULL);
  assert(serverIO->type == SERVER_IO_TYPE_NONE);

  // inti variables
  serverIO->type                 = SERVER_IO_TYPE_NETWORK;
  serverIO->network.name         = String_duplicate(hostName);
  serverIO->network.port         = hostPort;
  serverIO->network.socketHandle = socketHandle;
  serverIO->network.isConnected  = TRUE;

  String_clear(serverIO->line);
  List_clear(&serverIO->commandList,CALLBACK(freeCommandNode,NULL));
  List_clear(&serverIO->resultList,CALLBACK(freeResultNode,NULL));

  return ERROR_NONE;
}

void ServerIO_disconnect(ServerIO *serverIO)
{
  Errors error;

  assert(serverIO != NULL);

  switch (serverIO->type)
  {
    case SERVER_IO_TYPE_NONE:
      break;
    case SERVER_IO_TYPE_BATCH:
      break;
    case SERVER_IO_TYPE_NETWORK:
      Network_disconnect(&serverIO->network.socketHandle);
      serverIO->network.isConnected = FALSE;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }
  serverIO->type = SERVER_IO_TYPE_NONE;
}

void ServerIO_sendSessionId(ServerIO *serverIO)
{
  String encodedId;
  String n,e;
  String s;

  assert(serverIO != NULL);

  // format session data
  encodedId = encodeHex(String_new(),serverIO->sessionId,sizeof(SessionId));
  n         = Crypt_getPublicPrivateKeyModulus(&serverIO->publicKey);
  e         = Crypt_getPublicPrivateKeyExponent(&serverIO->publicKey);
  if ((n !=NULL) && (e != NULL))
  {
    s  = String_format(String_new(),
                       "SESSION id=%S encryptTypes=%s n=%S e=%S",
                       encodedId,
                       "RSA,NONE",
                       n,
                       e
                      );
  }
  else
  {
    s  = String_format(String_new(),
                       "SESSION id=%S encryptTypes=%s",
                       encodedId,
                       "NONE"
                      );
  }
  String_appendChar(s,'\n');

  // send session data
  sendData(serverIO,s);

  // free resources
  String_delete(s);
  String_delete(e);
  String_delete(n);
  String_delete(encodedId);
}

bool ServerIO_decryptPassword(Password       *password,
                              const ServerIO *serverIO,
                              ConstString    encryptType,
                              ConstString    encryptedPassword
                             )
{
  byte encryptedBuffer[1024];
  uint encryptedBufferLength;
  byte encodedBuffer[1024];
  uint encodedBufferLength;
  uint i;

  assert(password != NULL);

  // decode hex-string
  if (!decodeHex(String_cString(encryptedPassword),encryptedBuffer,&encryptedBufferLength,sizeof(encryptedBuffer)))
  {
    return FALSE;
  }

  // decrypt password
  if      (String_equalsIgnoreCaseCString(encryptType,"RSA") && Crypt_isAsymmetricSupported())
  {
//fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,encryptedBufferLength);
    Crypt_keyDecrypt(&serverIO->privateKey,
                     encryptedBuffer,
                     encryptedBufferLength,
                     encodedBuffer,
                     &encodedBufferLength,
                     sizeof(encodedBuffer)
                    );
  }
  else if (String_equalsIgnoreCaseCString(encryptType,"NONE"))
  {
    memcpy(encodedBuffer,encryptedBuffer,encryptedBufferLength);
    encodedBufferLength = encryptedBufferLength;
  }
  else
  {
    return FALSE;
  }

//fprintf(stderr,"%s, %d: n=%d s='",__FILE__,__LINE__,encodedBufferLength); for (i = 0; i < encodedBufferLength; i++) { fprintf(stderr,"%c",encodedBuffer[i]^clientInfo->sessionId[i]); } fprintf(stderr,"'\n");

  // decode password (XOR with session id)
  Password_clear(password);
  i = 0;
  while (   (i < encodedBufferLength)
         && ((char)(encodedBuffer[i]^serverIO->sessionId[i]) != '\0')
        )
  {
    Password_appendChar(password,(char)(encodedBuffer[i]^serverIO->sessionId[i]));
    i++;
  }

  return TRUE;
}

bool ServerIO_checkPassword(const ServerIO *serverIO,
                            ConstString    encryptType,
                            ConstString    encryptedPassword,
                            const Password *password
                           )
{
  byte encryptedBuffer[1024];
  uint encryptedBufferLength;
  byte encodedBuffer[1024];
  uint encodedBufferLength;
  uint n;
  uint i;
  bool okFlag;

  // decode hex-string
  if (!decodeHex(String_cString(encryptedPassword),encryptedBuffer,&encryptedBufferLength,sizeof(encryptedBuffer)))
  {
    return FALSE;
  }

  // decrypt password
  if      (String_equalsIgnoreCaseCString(encryptType,"RSA") && Crypt_isAsymmetricSupported())
  {
//fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,encryptedBufferLength);
    if (Crypt_keyDecrypt(&serverIO->privateKey,
                         encryptedBuffer,
                         encryptedBufferLength,
                         encodedBuffer,
                         &encodedBufferLength,
                         sizeof(encodedBuffer)
                        ) != ERROR_NONE
       )
    {
      return FALSE;
    }
  }
  else if (String_equalsIgnoreCaseCString(encryptType,"NONE"))
  {
    memcpy(encodedBuffer,encryptedBuffer,encryptedBufferLength);
    encodedBufferLength = encryptedBufferLength;
  }
  else
  {
    return FALSE;
  }

//fprintf(stderr,"%s, %d: n=%d s='",__FILE__,__LINE__,encodedBufferLength); for (i = 0; i < encodedBufferLength; i++) { fprintf(stderr,"%c",encodedBuffer[i]^clientInfo->sessionId[i]); } fprintf(stderr,"'\n");

  // check password length
  n = 0;
  while (   (n < encodedBufferLength)
         && ((char)(encodedBuffer[n]^serverIO->sessionId[n]) != '\0')
        )
  {
    n++;
  }
  if (password != NULL)
  {
    if (Password_length(password) != n)
    {
      return FALSE;
    }
  }

  // check encoded password
  if (password != NULL)
  {
    okFlag = TRUE;
    i = 0;
    while ((i < Password_length(password)) && okFlag)
    {
      okFlag = (Password_getChar(password,i) == (encodedBuffer[i]^serverIO->sessionId[i]));
      i++;
    }
    if (!okFlag)
    {
      return FALSE;
    }
  }

  return TRUE;
}

// ----------------------------------------------------------------------

Errors ServerIO_vsendCommand(ServerIO   *serverIO,
                             uint       *id,
                             const char *format,
                             va_list    arguments
                            )
{
  String             command;
  locale_t           locale;
  SemaphoreLock      semaphoreLock;
  ServerIOResultNode *resultNode;

  assert(serverIO != NULL);
  assert(id != NULL);
  assert(format != NULL);

  // get new command id
  (*id) = atomicIncrement(&serverIO->commandId,1);

  // send command
  command = String_new();
  locale = uselocale(POSIXLocale);
  {
    String_format(command,"%u ",*id);
    String_vformat(command,format,arguments);
    #ifndef NDEBUG
      if (globalOptions.serverDebugFlag)
      {
        fprintf(stderr,"DEBUG: send command '%s'\n",String_cString(command));
      }
    #endif /* not DEBUG */
    String_appendChar(command,'\n');
  }
  uselocale(locale);

  sendData(serverIO,command);

  // free resources
  String_delete(command);

  return ERROR_NONE;
}

Errors ServerIO_sendCommand(ServerIO   *serverIO,
                            uint       *id,
                            const char *format,
                            ...
                           )
{
  va_list arguments;
  Errors  error;

  va_start(arguments,format);
  error = ServerIO_vsendCommand(serverIO,id,format,arguments);
  va_end(arguments);

  return error;
}

Errors ServerIO_waitCommand(ServerIO  *serverIO,
                            long      timeout,
                            uint      *id,
                            String    name,
                            StringMap argumentMap
                           )
{
  SemaphoreLock       semaphoreLock;
  ServerIOCommandNode *commandNode;

  assert(serverIO != NULL);

  // wait for result
  SEMAPHORE_LOCKED_DO(semaphoreLock,&serverIO->commandList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // wait for command
    while (List_isEmpty(&serverIO->commandList))
    {
      // no command -> wait for data
      Semaphore_unlock(&serverIO->commandList.lock);
      {
        if (!receiveData(serverIO,timeout))
        {
          return ERROR_NETWORK_TIMEOUT;
        }
      }
      Semaphore_lock(&serverIO->commandList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);
    }

    // get command
    commandNode = List_removeFirst(&serverIO->commandList);
  }
  assert(commandNode != NULL);

  // get arguments
  if (argumentMap != NULL)
  {
    if (!StringMap_parse(argumentMap,commandNode->data,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,STRING_BEGIN,NULL))
    {
      deleteCommandNode(commandNode);
      return ERROR_PARSE;
    }
  }

  // free resources
  deleteCommandNode(commandNode);

  return ERROR_NONE;
}

Errors ServerIO_sendResult(ServerIO   *serverIO,
                           uint       id,
                           bool       completedFlag,
                           Errors     error,
                           const char *format,
                           ...
                          )
{
  String   result;
  locale_t locale;
  va_list  arguments;

  assert(serverIO != NULL);
  assert(format != NULL);

  // init variables
  result = String_new();

  // format result
  locale = uselocale(POSIXLocale);
  {
    String_format(result,"%u %d %u ",id,completedFlag ? 1 : 0,Error_getCode(error));
    va_start(arguments,format);
    String_vformat(result,format,arguments);
    va_end(arguments);
    #ifndef NDEBUG
      if (globalOptions.serverDebugFlag)
      {
        fprintf(stderr,"DEBUG: send result '%s'\n",String_cString(result));
      }
    #endif /* not DEBUG */
    String_appendChar(result,'\n');
  }
  uselocale(locale);

  // send result
  sendData(serverIO,result);

  // free resources
  String_delete(result);

  return ERROR_NONE;
}

Errors ServerIO_waitResult(ServerIO  *serverIO,
                           uint      id,
                           long      timeout,
                           Errors    *error,
                           bool      *completedFlag,
                           StringMap resultMap
                          )
{
  SemaphoreLock      semaphoreLock;
  ServerIOResultNode *resultNode;

  assert(serverIO != NULL);

  // wait for result
  SEMAPHORE_LOCKED_DO(semaphoreLock,&serverIO->resultList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,timeout)
  {
    do
    {
      // find matching result
      resultNode = LIST_FIND(&serverIO->resultList,resultNode,resultNode->id == id);
      if (resultNode != NULL)
      {
        // found -> get result
        List_remove(&serverIO->resultList,resultNode);
      }
      else
      {
        // not found -> wait for data
        Semaphore_unlock(&serverIO->resultList.lock);
        {
          if (!receiveData(serverIO,timeout))
          {
            return ERROR_NETWORK_TIMEOUT;
          }
        }
        Semaphore_lock(&serverIO->resultList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);
      }
    }
    while (resultNode == NULL);
  }

  // get result
  if (error != NULL) (*error) = resultNode->error;
  if (completedFlag != NULL) (*completedFlag) = resultNode->completedFlag;
  if (resultMap != NULL)
  {
    if (!StringMap_parse(resultMap,resultNode->data,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,STRING_BEGIN,NULL))
    {
      deleteResultNode(resultNode);
      return ERROR_PARSE;
    }
  }

  // free resources
  deleteResultNode(resultNode);

  return ERROR_NONE;
}

Errors ServerIO_vexecuteCommand(ServerIO   *serverIO,
                                StringMap  resultMap,
                                long       timeout,
                                const char *format,
                                va_list    arguments
                               )
{
  uint               id;
  SemaphoreLock      semaphoreLock;
  ServerIOResultNode *resultNode;
  Errors             error;

  assert(serverIO != NULL);
  assert(format != NULL);

  error = ERROR_UNKNOWN;

  // send command
  error = ServerIO_vsendCommand(serverIO,
                                &id,
                                format,
                                arguments
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // wait for result, timeout, or quit
  SEMAPHORE_LOCKED_DO(semaphoreLock,&serverIO->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    do
    {
      // check result
      resultNode = LIST_FIND(&serverIO->resultList,resultNode,resultNode->id == id);
      if (resultNode != NULL)
      {
        // get result
        List_remove(&serverIO->resultList,resultNode);
      }
      else
      {
        // wait for result
        if (!Semaphore_waitModified(&serverIO->lock,timeout))
        {
          Semaphore_unlock(&serverIO->lock);
          return ERROR_NETWORK_TIMEOUT;
        }
      }
    }
    while (resultNode == NULL);
  }

  // get action result
  error = resultNode->error;
  if (resultMap != NULL)
  {
//    if (!StringMap_parse(command->argumentMap,arguments,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,STRING_BEGIN,NULL))
//    {
//    }
//    StringMap_move(resultMap,serverIO->action.resultMap);
  }
  else
  {
//    StringMap_clear(serverIO->action.resultMap);
  }

  // free resources
  deleteResultNode(resultNode);

#if 0
  // wait for result, timeout, or quit
  while ((serverIO->action.error == ERROR_UNKNOWN) && !serverIO->quitFlag)
  {
    if (!Semaphore_waitModified(&serverIO->action.lock,timeout))
    {
      Semaphore_unlock(&serverIO->action.lock);
      return ERROR_NETWORK_TIMEOUT;
    }
  }
  if (serverIO->quitFlag)
  {
    Semaphore_unlock(&serverIO->action.lock);
    return ERROR_ABORTED;
  }

  // get action result
  error = serverIO->action.error;
  if (resultMap != NULL)
  {
    StringMap_move(resultMap,serverIO->action.resultMap);
  }
  else
  {
    StringMap_clear(serverIO->action.resultMap);
  }
#endif

  return error;
}

Errors ServerIO_executeCommand(ServerIO   *serverIO,
                               StringMap  resultMap,
                               long       timeout,
                               const char *format,
                               ...
                              )
{
  va_list arguments;
  Errors  error;

  va_start(arguments,format);
  error = ServerIO_vexecuteCommand(serverIO,resultMap,timeout,format,arguments);
  va_end(arguments);

  return error;
}

Errors ServerIO_clientAction(ServerIO   *serverIO,
                             uint       id,
                             StringMap  resultMap,
                             const char *actionCommand,
                             long       timeout,
                             const char *format,
                             ...
                            )
{
  String             action;
  locale_t           locale;
  va_list            arguments;
  SemaphoreLock      semaphoreLock;
  ServerIOResultNode *resultNode;
  Errors             error;

  assert(serverIO != NULL);
  assert(actionCommand != NULL);

  error = ERROR_UNKNOWN;

  // get new command id
  id = atomicIncrement(&serverIO->commandId,1);

  // format action
  action = String_new();
  locale = uselocale(POSIXLocale);
  {
    String_format(action,"%u 0 0 action=%s ",id,actionCommand);
    va_start(arguments,format);
    String_vformat(action,format,arguments);
    va_end(arguments);
    #ifndef NDEBUG
      if (globalOptions.serverDebugFlag)
      {
        fprintf(stderr,"DEBUG: send action '%s'\n",String_cString(action));
      }
    #endif /* not DEBUG */
    String_appendChar(action,'\n');
  }
  uselocale(locale);

  // send action
  sendData(serverIO,action);

  // free resources
  String_delete(action);

  // wait for result, timeout, or quit
  SEMAPHORE_LOCKED_DO(semaphoreLock,&serverIO->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    do
    {
      // check result
      resultNode = LIST_FIND(&serverIO->resultList,resultNode,resultNode->id == id);
      if (resultNode != NULL)
      {
        // get result
        List_remove(&serverIO->resultList,resultNode);
      }
      else
      {
        // wait for result
        if (!Semaphore_waitModified(&serverIO->lock,timeout))
        {
          Semaphore_unlock(&serverIO->lock);
          return ERROR_NETWORK_TIMEOUT;
        }
      }
    }
    while (resultNode == NULL);
  }

  // get action result
  error = resultNode->error;
  if (resultMap != NULL)
  {
//    if (!StringMap_parse(command->argumentMap,arguments,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,STRING_BEGIN,NULL))
//    {
//    }
//    StringMap_move(resultMap,serverIO->action.resultMap);
  }
  else
  {
//    StringMap_clear(serverIO->action.resultMap);
  }

  // free resources
  deleteResultNode(resultNode);

#if 0
  // wait for result, timeout, or quit
  while ((serverIO->action.error == ERROR_UNKNOWN) && !serverIO->quitFlag)
  {
    if (!Semaphore_waitModified(&serverIO->action.lock,timeout))
    {
      Semaphore_unlock(&serverIO->action.lock);
      return ERROR_NETWORK_TIMEOUT;
    }
  }
  if (serverIO->quitFlag)
  {
    Semaphore_unlock(&serverIO->action.lock);
    return ERROR_ABORTED;
  }

  // get action result
  error = serverIO->action.error;
  if (resultMap != NULL)
  {
    StringMap_move(resultMap,serverIO->action.resultMap);
  }
  else
  {
    StringMap_clear(serverIO->action.resultMap);
  }
#endif

  return error;
}

Errors ServerIO_sendMaster(const ServerIO     *serverIO,
                           ServerIOResultList *resultList,
                           const char         *format,
                           ...
                          )
{
  String        line;
uint commandId;
  va_list       arguments;
  Errors        error;
  SemaphoreLock semaphoreLock;
//  serverIO    *serverIO;

  assert(serverIO != NULL);
  assert(format != NULL);

  // init variables
  line = String_new();

error=ERROR_NONE;
commandId = 0;

    // send command
    va_start(arguments,format);
//    sendData(serverIO,commandId,format,arguments);
    va_end(arguments);

    // wait for results
//TODO

  // free resources
  String_delete(line);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
