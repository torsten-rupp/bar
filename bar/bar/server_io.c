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
* Name   : init
* Purpose: init server i/o
* Input  : serverIO - server i/o
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void init(ServerIO *serverIO)
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
  List_init(&serverIO->resultList);
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
        case SERVER_IO_TYPE_BATCH:
          (void)File_write(&serverIO->file.fileHandle,String_cString(data),String_length(data));
          (void)File_flush(&serverIO->file.fileHandle);
          break;
        case SERVER_IO_TYPE_NETWORK:
          (void)Network_send(&serverIO->network.socketHandle,String_cString(data),String_length(data));
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break;
      }
    }
  }
}

#if 0
/***********************************************************************\
* Name   : sendCommand
* Purpose: send command
* Input  : serverIO  - server i/o
*          id        - command id
*          format    - format string
*          arguments - arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void sendCommand(ServerIO *serverIO, uint id, const char *format, va_list arguments)
{
  locale_t      locale;
  String        command;
  SemaphoreLock semaphoreLock;

  assert(serverIO != NULL);
fprintf(stderr,"%s, %d: serverIO->typ=%d\n",__FILE__,__LINE__,serverIO->type);
  assert(serverIO->type == SERVER_IO_TYPE_NETWORK);

  command = String_new();

  locale = uselocale(POSIXLocale);
  {
    String_format(command,"%u ",id);
    String_vformat(command,format,arguments);
    String_appendChar(command,'\n');
  }
  uselocale(locale);

  #ifndef NDEBUG
    if (globalOptions.serverDebugFlag)
    {
      fprintf(stderr,"DEBUG: send master=%s",String_cString(command));
    }
  #endif /* not DEBUG */
  SEMAPHORE_LOCKED_DO(semaphoreLock,&serverIO->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    (void)Network_send(&serverIO->network.socketHandle,String_cString(command),String_length(command));
  }

  String_delete(command);
}

/***********************************************************************\
* Name   : sendClientResult
* Purpose: send result to client
* Input  : serverIO      - server i/o
*          id            - command id
*          completedFlag - TRUE if command is completed, FALSE otherwise
*          error         - ERROR_NONE or error code
*          format        - format string
*          ...           - optional arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void sendResult(ServerIO *serverIO, uint id, bool completedFlag, Errors error, const char *format, ...)
{
  String   result;
  locale_t locale;
  va_list  arguments;

  // init variables
  result = String_new();

  // format result
  locale = uselocale(POSIXLocale);
  {
    String_format(result,"%u %d %u ",id,completedFlag ? 1 : 0,Error_getCode(error));
    va_start(arguments,format);
    String_vformat(result,format,arguments);
    va_end(arguments);
    String_appendChar(result,'\n');
  }
  uselocale(locale);

  // send
  #ifndef NDEBUG
    if (globalOptions.serverDebugFlag)
    {
      fprintf(stderr,"DEBUG: send result '%s'",String_cString(result));
    }
  #endif /* not DEBUG */
  sendData(serverIO,result);

  // free resources
  String_delete(result);
}

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
    String_appendChar(action,'\n');
  }
  uselocale(locale);

  #ifndef NDEBUG
    if (globalOptions.serverDebugFlag)
    {
      fprintf(stderr,"DEBUG: sent action=%s",String_cString(action));
    }
  #endif /* not DEBUG */
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

void ServerIO_initBatch(ServerIO   *serverIO,
                        FileHandle fileHandle
                       )
{
  assert(serverIO != NULL);

  init(serverIO);
  serverIO->type            = SERVER_IO_TYPE_BATCH;
  serverIO->file.fileHandle = fileHandle;
}

void ServerIO_initNetwork(ServerIO     *serverIO,
                          ConstString  name,
                          uint         port,
                          SocketHandle socketHandle
                         )
{
  assert(serverIO != NULL);

  init(serverIO);
  serverIO->type                 = SERVER_IO_TYPE_NETWORK;
  serverIO->network.name         = String_duplicate(name);
  serverIO->network.port         = port;
  serverIO->network.socketHandle = socketHandle;
}

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
  List_init(&serverIO->resultList);
}

void ServerIO_done(ServerIO *serverIO)
{
  assert(serverIO != NULL);

  List_done(&serverIO->resultList,CALLBACK(freeResultNode,NULL));
  Crypt_doneKey(&serverIO->privateKey);
  Crypt_doneKey(&serverIO->publicKey);
  Semaphore_done(&serverIO->lock);
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
  Errors             error;

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
    String_appendChar(command,'\n');
  }
  uselocale(locale);

  #ifndef NDEBUG
    if (globalOptions.serverDebugFlag)
    {
      fprintf(stderr,"DEBUG: sent command=%s",String_cString(command));
    }
  #endif /* not DEBUG */
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
    String_appendChar(result,'\n');
  }
  uselocale(locale);

  // send result
  #ifndef NDEBUG
    if (globalOptions.serverDebugFlag)
    {
      fprintf(stderr,"DEBUG: send result '%s'",String_cString(result));
    }
  #endif /* not DEBUG */
  sendData(serverIO,result);

  // free resources
  String_delete(result);

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
  String             command;
  locale_t           locale;
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

  // send action
  action = String_new();
  locale = uselocale(POSIXLocale);
  {
    String_format(action,"%u 0 0 action=%s ",id,actionCommand);
    va_start(arguments,format);
    String_vformat(action,format,arguments);
    va_end(arguments);
    String_appendChar(action,'\n');
  }
  uselocale(locale);

  #ifndef NDEBUG
    if (globalOptions.serverDebugFlag)
    {
      fprintf(stderr,"DEBUG: sent action=%s",String_cString(action));
    }
  #endif /* not DEBUG */
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
