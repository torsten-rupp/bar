/***********************************************************************\
*
* $Revision: 7108 $
* $Date: 2017-01-20 17:08:03 +0100 (Fri, 20 Jan 2017) $
* $Author: torsten $
* Contents: Backup ARchiver server
* Systems: all
*
\***********************************************************************/

#define __SERVER_IO_IMPLEMENTATION__

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

#define SESSION_KEY_SIZE            1024     // number of session key bits

#define LOCK_TIMEOUT                (10*60*MS_PER_SECOND)  // general lock timeout [ms]
#define READ_TIMEOUT                (5LL*MS_PER_SECOND)

#define BUFFER_SIZE       8192
#define BUFFER_DELTA_SIZE 4096

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

//TODO: required?
#if 0
LOCAL void freeCommandNode(ServerIOCommandNode *commandNode, void *userData)
{
  assert(commandNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(commandNode->data);
  String_delete(commandNode->name);
}
#endif

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

//TODO: required?
#if 0
LOCAL void deleteCommandNode(ServerIOCommandNode *commandNode)
{
  assert(commandNode != NULL);

  freeCommandNode(commandNode,NULL);
  LIST_DELETE_NODE(commandNode);
}
#endif

/***********************************************************************\
* Name   : getLine
* Purpose: get line from input buffer
* Input  : serverIO - server i/o
* Output : -
* Return : TRUE iff line available
* Notes  : -
\***********************************************************************/

LOCAL bool getLine(ServerIO *serverIO)
{
  char ch;

  assert(serverIO != NULL);

  while (   !serverIO->lineFlag
         && (serverIO->inputBufferIndex < serverIO->inputBufferLength)
        )
  {
    ch = serverIO->inputBuffer[serverIO->inputBufferIndex]; serverIO->inputBufferIndex++;
    if      (isprint(ch))
    {
      String_appendChar(serverIO->line,ch);
    }
    else if (ch == '\n')
    {
      serverIO->lineFlag = TRUE;
    }
  }

  return serverIO->lineFlag;
}

/***********************************************************************\
* Name   : doneLine
* Purpose: done line
* Input  : serverIO - server i/o
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneLine(ServerIO *serverIO)
{
  String_clear(serverIO->line);
  serverIO->lineFlag = FALSE;
}

/***********************************************************************\
* Name   : sendData
* Purpose: send data
* Input  : serverIO - server i/o
*          line     - data line
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors sendData(ServerIO *serverIO, ConstString line)
{
  SemaphoreLock semaphoreLock;
  Errors        error;
  uint          n;

  assert(serverIO != NULL);
  assert(line != NULL);

  error = ERROR_UNKNOWN;
//  if (!serverIO->isConnected)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&serverIO->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      // get line length
      n = String_length(line);

//TODO: avoid copy and add LF?
      // extend output buffer if needed
      if ((n+1) > serverIO->outputBufferSize)
      {
fprintf(stderr,"%s, %d: extend output buffer %d -> %d\n",__FILE__,__LINE__,serverIO->outputBufferSize,n);
        serverIO->outputBuffer = (char*)realloc(serverIO->outputBuffer,serverIO->outputBufferSize+BUFFER_DELTA_SIZE);
        if (serverIO->outputBuffer == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }
        serverIO->outputBufferSize += BUFFER_DELTA_SIZE;
      }

      // init output buffer
      memCopy(serverIO->outputBuffer,serverIO->outputBufferSize,String_cString(line),n);
      serverIO->outputBuffer[n] = '\n';

      // send data
//fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__); debugDumpMemory(serverIO->outputBuffer,n+1,0);
      switch (serverIO->type)
      {
        case SERVER_IO_TYPE_NONE:
          error = ERROR_CONNECT_FAIL;
          break;
        case SERVER_IO_TYPE_BATCH:
          error = File_write(&serverIO->file.fileHandle,serverIO->outputBuffer,n+1);
          (void)File_flush(&serverIO->file.fileHandle);
          break;
        case SERVER_IO_TYPE_NETWORK:
          error = Network_send(&serverIO->network.socketHandle,serverIO->outputBuffer,n+1);
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }
    }
  }

  return error;
}

#if 0
//TODO obsolete
/***********************************************************************\
* Name   : receiveData
* Purpose: receive data
* Input  : serverIO - server i/o
* Output : -
* Return : TRUE if received data, FALSE on disconnect
* Notes  : -
\***********************************************************************/

LOCAL bool receiveData(ServerIO *serverIO)
{
  char   buffer[4096];
  ulong  readBytes;
  ulong  i;
  Errors error;

  assert(serverIO != NULL);

  switch (serverIO->type)
  {
    case SERVER_IO_TYPE_NONE:
      break;
    case SERVER_IO_TYPE_BATCH:
      (void)File_read(&serverIO->file.fileHandle,buffer,sizeof(buffer),&readBytes);
//fprintf(stderr,"%s, %d: readBytes=%d buffer=%s\n",__FILE__,__LINE__,readBytes,buffer);
      if (readBytes > 0)
      {
        do
        {
          // received data -> process
          for (i = 0; i < readBytes; i++)
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
          error = File_read(&serverIO->file.fileHandle,buffer,sizeof(buffer),&readBytes);
        }
        while ((error == ERROR_NONE) && (readBytes > 0));
      }
      else
      {
        // disconnect
fprintf(stderr,"%s, %d: DISCONNECT?\n",__FILE__,__LINE__);
        return FALSE;
      }
      break;
    case SERVER_IO_TYPE_NETWORK:
      (void)Network_receive(&serverIO->network.socketHandle,buffer,sizeof(buffer),NO_WAIT,&readBytes);
//buffer[readBytes]=0;
//fprintf(stderr,"%s, %d: rec socket %d: bytes %d: %s\n",__FILE__,__LINE__,Network_getSocket(&serverIO->network.socketHandle),readBytes,buffer);
      if (readBytes > 0)
      {
        do
        {
          // received data -> process
          for (i = 0; i < readBytes; i++)
          {
            if (buffer[i] != '\n')
            {
              String_appendChar(serverIO->line,buffer[i]);
            }
            else
            {
//fprintf(stderr,"%s, %d: process %s\n",__FILE__,__LINE__,String_cString(serverIO->line));
              processData(serverIO,serverIO->line);
              String_clear(serverIO->line);
            }
          }
          error = Network_receive(&serverIO->network.socketHandle,buffer,sizeof(buffer),NO_WAIT,&readBytes);
        }
        while ((error == ERROR_NONE) && (readBytes > 0));
      }
      else
      {
        // disconnect
fprintf(stderr,"%s, %d: DISCONNECT?\n",__FILE__,__LINE__);
        return FALSE;
      }
      break;
  }

  return TRUE;
}
#endif

#if 0
//TODO obsolete
/***********************************************************************\
* Name   : waitData
* Purpose: wait for and receive data
* Input  : serverIO - server i/o
*          timeout  - timeout [ms] or NO_WAIT, WAIT_FOREVER
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool waitData(ServerIO *serverIO, long timeout)
{
  struct pollfd   pollfds[1];
  struct timespec pollTimeout;
  sigset_t        signalMask;

  assert(serverIO != NULL);

  // wait for data from slave
//TODO: batch?
uint64 t0 = Misc_getTimestamp();
  pollfds[0].fd       = Network_getSocket(&serverIO->network.socketHandle);
  pollfds[0].events   = POLLIN|POLLERR|POLLNVAL;
  pollTimeout.tv_sec  = timeout/MS_PER_SECOND;
  pollTimeout.tv_nsec = (timeout%MS_PER_SECOND)*NS_PER_MS;
  if (ppoll(pollfds,1,&pollTimeout,&signalMask) <= 0)
  {
fprintf(stderr,"%s, %d: ppoll timeout/error %d: polltime=%llu: %s\n",__FILE__,__LINE__,Network_getSocket(&serverIO->network.socketHandle),(Misc_getTimestamp()-t0)/1000,strerror(errno));
    return FALSE;
  }
fprintf(stderr,"%s, %d: polltime=%llums\n",__FILE__,__LINE__,(Misc_getTimestamp()-t0)/1000);

  // process data results/commands
  if ((pollfds[0].revents & POLLIN) != 0)
  {
    // received data
    if (!receiveData(serverIO))
    {
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
      return FALSE;
    }
  }
  else if ((pollfds[0].revents & (POLLERR|POLLNVAL)) != 0)
  {
    // error/disconnect
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
    return FALSE;
  }

  return TRUE;
}
#endif

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
  String        s;
  locale_t      locale;
  va_list       arguments;
  SemaphoreLock semaphoreLock;
  Errors        error;

  assert(serverIO != NULL);
  assert(actionCommand != NULL);

  // init variables
  s = String_new();

  // format action
  locale = uselocale(POSIXLocale);
  {
    String_format(s,"%u 0 0 action=%s ",id,actionCommand);
    va_start(arguments,format);
    String_vformat(s,format,arguments);
    va_end(arguments);
  }
  uselocale(locale);

  // send action
  error = sendData(serverIO,s);
  if (error != ERROR_NONE)
  {
    String_delete(s);
    return error;
  }
  #ifndef NDEBUG
    if (globalOptions.serverDebugLevel >= 1)
    {
      fprintf(stderr,"DEBUG: sent action '%s'\n",String_cString(s));
    }
  #endif /* not DEBUG */

  // free resources
  String_delete(s);

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

bool ServerIO_parseEncryptType(const char *encryptTypeText, ServerIOEncryptTypes *encryptTypes, void *userData)
{
  assert(encryptTypeText != NULL);
  assert(encryptTypes != NULL);

  UNUSED_VARIABLE(userData);

  if      (stringEqualsIgnoreCase(encryptTypeText,"NONE")) (*encryptTypes) = SERVER_IO_ENCRYPT_TYPE_NONE;
  else if (stringEqualsIgnoreCase(encryptTypeText,"RSA" )) (*encryptTypes) = SERVER_IO_ENCRYPT_TYPE_RSA;
  else                                                     (*encryptTypes) = SERVER_IO_ENCRYPT_TYPE_NONE;

  return TRUE;
}

#ifdef NDEBUG
void ServerIO_init(ServerIO *serverIO)
#else /* not NDEBUG */
void __ServerIO_init(const char *__fileName__,
                     ulong      __lineNb__,
                     ServerIO   *serverIO
                    )
#endif /* NDEBUG */
{
  assert(serverIO != NULL);

  Semaphore_init(&serverIO->lock);
  #ifndef NO_SESSION_ID
    Crypt_randomize(serverIO->sessionId,sizeof(SessionId));
  #else /* not NO_SESSION_ID */
    memset(serverIO->sessionId,0,sizeof(SessionId));
  #endif /* NO_SESSION_ID */
  Crypt_initKey(&serverIO->publicKey,CRYPT_PADDING_TYPE_PKCS1);
  Crypt_initKey(&serverIO->privateKey,CRYPT_PADDING_TYPE_PKCS1);

  serverIO->pollfds           = (struct pollfd*)malloc(64*sizeof(struct pollfd));
  if (serverIO->pollfds == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  serverIO->pollfdCount       = 0;
  serverIO->maxPollfdCount    = 64;

  serverIO->inputBuffer       = (char*)malloc(BUFFER_SIZE);
  if (serverIO->inputBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  serverIO->inputBufferIndex  = 0;
  serverIO->inputBufferLength = 0;
  serverIO->inputBufferSize   = BUFFER_SIZE;

  serverIO->outputBuffer      = (char*)malloc(BUFFER_SIZE);
  if (serverIO->outputBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  serverIO->outputBufferSize  = BUFFER_SIZE;

  serverIO->line              = String_new();
  serverIO->lineFlag          = FALSE;

  serverIO->type              = SERVER_IO_TYPE_NONE;
  serverIO->isConnected       = FALSE;
  serverIO->commandId         = 0;
  Semaphore_init(&serverIO->resultList.lock);
  List_init(&serverIO->resultList);

  #ifndef NDEBUG
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,serverIO,sizeof(ServerIO));
  #else /* NDEBUG */
    DEBUG_ADD_RESOURCE_TRACE(serverIO,sizeof(ServerIO));
  #endif /* not NDEBUG */
}

#ifdef NDEBUG
void ServerIO_done(ServerIO *serverIO)
#else /* not NDEBUG */
void __ServerIO_done(const char *__fileName__,
                     ulong      __lineNb__,
                     ServerIO   *serverIO
                    )
#endif /* NDEBUG */
{
  assert(serverIO != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(serverIO,sizeof(ServerIO));
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,serverIO,sizeof(ServerIO));
  #endif /* NDEBUG */

  List_done(&serverIO->resultList,CALLBACK((ListNodeFreeFunction)freeResultNode,NULL));
  Semaphore_done(&serverIO->resultList.lock);
  String_delete(serverIO->line);
  free(serverIO->outputBuffer);
  free(serverIO->inputBuffer);
  free(serverIO->pollfds);
  Crypt_doneKey(&serverIO->privateKey);
  Crypt_doneKey(&serverIO->publicKey);
  Semaphore_done(&serverIO->lock);
}

void ServerIO_connectBatch(ServerIO   *serverIO,
                           FileHandle fileHandle
                          )
{
  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);

  serverIO->type              = SERVER_IO_TYPE_BATCH;
  serverIO->file.fileHandle   = fileHandle;
  serverIO->isConnected       = TRUE;

  serverIO->inputBufferIndex  = 0;
  serverIO->inputBufferLength = 0;
  String_clear(serverIO->line);
  serverIO->lineFlag          = FALSE;
  List_clear(&serverIO->resultList,CALLBACK((ListNodeFreeFunction)freeResultNode,NULL));
}

void ServerIO_connectNetwork(ServerIO     *serverIO,
                             SocketHandle *socketHandle,
                             ConstString  hostName,
                             uint         hostPort
                            )
{
  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);

  // init variables
  serverIO->type                 = SERVER_IO_TYPE_NETWORK;
  serverIO->network.socketHandle = *socketHandle;
  serverIO->network.name         = String_duplicate(hostName);
  serverIO->network.port         = hostPort;
  serverIO->isConnected          = TRUE;

  serverIO->inputBufferIndex     = 0;
  serverIO->inputBufferLength    = 0;
  String_clear(serverIO->line);
  serverIO->lineFlag             = FALSE;
  List_clear(&serverIO->resultList,CALLBACK((ListNodeFreeFunction)freeResultNode,NULL));
}

void ServerIO_disconnect(ServerIO *serverIO)
{
  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);

  switch (serverIO->type)
  {
    case SERVER_IO_TYPE_NONE:
      break;
    case SERVER_IO_TYPE_BATCH:
      break;
    case SERVER_IO_TYPE_NETWORK:
      Network_disconnect(&serverIO->network.socketHandle);
      String_delete(serverIO->network.name);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }
  serverIO->isConnected = FALSE;
}

Errors ServerIO_startSession(ServerIO *serverIO)
{
  String encodedId;
  String n,e;
  String s;
  Errors error;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);

  // init variables
  encodedId = String_new();
  n         = String_new();
  e         = String_new();
  s         = String_new();

  // get encoded session id
  encodedId = Misc_hexEncode(String_new(),serverIO->sessionId,sizeof(SessionId));

  // create new session keys
  error = Crypt_createPublicPrivateKeyPair(&serverIO->publicKey,
                                           &serverIO->privateKey,
                                           SESSION_KEY_SIZE,
                                           CRYPT_KEY_MODE_TRANSIENT
                                          );
  if (   (error == ERROR_NONE)
      && Crypt_getPublicKeyModulusExponent(&serverIO->publicKey,n,e)
     )
  {
//fprintf(stderr,"%s, %d: create key pair\n",__FILE__,__LINE__); gcry_sexp_dump(serverIO->publicKey.key);
    // format session data
    String_format(s,
                  "SESSION id=%S encryptTypes=%s n=%S e=%S",
                  encodedId,
                  "RSA,NONE",
                  n,
                  e
                 );
  }
  else
  {
    // format session data
    String_format(s,
                  "SESSION id=%S encryptTypes=%s",
                  encodedId,
                  "NONE"
                 );
  }

  // send session data
  error = sendData(serverIO,s);
  if (error != ERROR_NONE)
  {
    String_delete(s);
    String_delete(e);
    String_delete(n);
    String_delete(encodedId);
    return error;
  }
  #ifndef NDEBUG
    if (globalOptions.serverDebugLevel >= 1)
    {
      fprintf(stderr,"DEBUG: send session data '%s'\n",String_cString(s));
    }
  #endif /* not DEBUG */

  // free resources
  String_delete(s);
  String_delete(e);
  String_delete(n);
  String_delete(encodedId);

  return ERROR_NONE;
}

Errors ServerIO_acceptSession(ServerIO *serverIO)
{
  String    line;
  StringMap argumentMap;
  Errors    error;
  String    id;
  String    n,e;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);

  // init variables
  line        = String_new();
  argumentMap = StringMap_new();
  id          = String_new();
  n           = String_new();
  e           = String_new();

  // get session data
  error = Network_readLine(&serverIO->network.socketHandle,line,READ_TIMEOUT);
  if (error != ERROR_NONE)
  {
    String_delete(e);
    String_delete(n);
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
    return error;
  }
  if (!String_startsWithCString(line,"SESSION"))
  {
    String_delete(e);
    String_delete(n);
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
//TODO
return ERROR_(UNKNOWN,0);
  }
  if (!StringMap_parse(argumentMap,line,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,7,NULL))
  {
    String_delete(e);
    String_delete(n);
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
return ERROR_(UNKNOWN,0);
  }

  if (!StringMap_getString(argumentMap,"id",id,NULL))
  {
    String_delete(e);
    String_delete(n);
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
return ERROR_(UNKNOWN,0);
  }
  if (!Misc_hexDecode(serverIO->sessionId,
                      NULL,  // dataLength
                      id,
                      STRING_BEGIN,
                      sizeof(serverIO->sessionId)
                     )
     )
  {
    String_delete(e);
    String_delete(n);
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
return ERROR_(UNKNOWN,0);
  }
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
//fprintf(stderr,"%s, %d: connector public n=%s\n",__FILE__,__LINE__,String_cString(n));
//fprintf(stderr,"%s, %d: connector public e=%s\n",__FILE__,__LINE__,String_cString(e));
  if (!Crypt_setPublicKeyModulusExponent(&serverIO->publicKey,n,e))
  {
    String_delete(e);
    String_delete(n);
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
return ERROR_(UNKNOWN,0);
  }

  // free resources
  String_delete(e);
  String_delete(n);
  String_delete(id);
  StringMap_delete(argumentMap);
  String_delete(line);

  return ERROR_NONE;
}

Errors ServerIO_decryptData(const ServerIO       *serverIO,
                            ServerIOEncryptTypes encryptType,
                            ConstString          encryptedString,
                            void                 **data,
                            uint                 *dataLength
                           )
{
  byte   encryptedBuffer[1024];
  uint   encryptedBufferLength;
  Errors error;
  byte   *buffer;
  uint   bufferLength;
  uint   i;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(encryptedString != NULL);
  assert(data != NULL);
  assert(dataLength != NULL);

  // convert hex/base64-string to data
  if      (String_startsWithCString(encryptedString,"base64:"))
  {
//TODO: alloc
//    encryptedBufferLength =
    if (!Misc_base64Decode(encryptedBuffer,&encryptedBufferLength,encryptedString,7,sizeof(encryptedBuffer)))
    {
      return ERROR_INVALID_ENCODING;
    }
  }
  else if (String_startsWithCString(encryptedString,"hex:"))
  {
    if (!Misc_hexDecode(encryptedBuffer,&encryptedBufferLength,encryptedString,4,sizeof(encryptedBuffer)))
    {
      return ERROR_INVALID_ENCODING;
    }
  }
  else
  {
    if (!Misc_hexDecode(encryptedBuffer,&encryptedBufferLength,encryptedString,STRING_BEGIN,sizeof(encryptedBuffer)))
    {
      return ERROR_INVALID_ENCODING;
    }
  }
//fprintf(stderr,"%s, %d: encryptedBufferLength=%d\n",__FILE__,__LINE__,encryptedBufferLength); debugDumpMemory(encryptedBuffer,encryptedBufferLength,0);

  // allocate secure memory
  bufferLength = encryptedBufferLength;
  buffer = allocSecure(bufferLength);
  if (buffer == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }

  // decrypt
  switch (encryptType)
  {
    case SERVER_IO_ENCRYPT_TYPE_NONE:
      memCopy(buffer,bufferLength,encryptedBuffer,encryptedBufferLength);
      break;
    case SERVER_IO_ENCRYPT_TYPE_RSA:
      if (Crypt_isAsymmetricSupported())
      {
//fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,encryptedBufferLength);
//fprintf(stderr,"%s, %d: my public key:\n",__FILE__,__LINE__); Crypt_dumpKey(&serverIO->publicKey);
//fprintf(stderr,"%s, %d: my private key:\n",__FILE__,__LINE__); Crypt_dumpKey(&serverIO->privateKey);
        error = Crypt_decryptWithPrivateKey(&serverIO->privateKey,
                                            encryptedBuffer,
                                            encryptedBufferLength,
                                            buffer,
                                            &bufferLength,
                                            bufferLength
                                           );
        if (error != ERROR_NONE)
        {
          freeSecure(buffer);
          return error;
        }
      }
      else
      {
        freeSecure(buffer);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      }
      break;
  }
//fprintf(stderr,"%s, %d: encoded data %d\n",__FILE__,__LINE__,bufferLength); debugDumpMemory(buffer,bufferLength,0);

  // decode data (XOR with session id)
//fprintf(stderr,"%s, %d: session id\n",__FILE__,__LINE__); debugDumpMemory(serverIO->sessionId,SESSION_ID_LENGTH,0);
  for (i = 0; i < bufferLength; i++)
  {
    buffer[i] = buffer[i]^serverIO->sessionId[i%SESSION_ID_LENGTH];
  }

  // set return values
  (*data)       = buffer;
  (*dataLength) = bufferLength;
//fprintf(stderr,"%s, %d: data %d\n",__FILE__,__LINE__,bufferLength); debugDumpMemory(buffer,bufferLength,0);

  return ERROR_NONE;
}

void ServerIO_decryptDone(void *data, uint dataLength)
{
  assert(data != NULL);

  UNUSED_VARIABLE(dataLength);

  freeSecure(data);
}

Errors ServerIO_encryptData(const ServerIO       *serverIO,
                            ServerIOEncryptTypes encryptType,
                            const void           *data,
                            uint                 dataLength,
                            String               encryptedData
                           )
{
  byte   *buffer;
  uint   bufferLength;
  uint   i;
  Errors error;
  byte   encryptedBuffer[1024];
  uint   encryptedBufferLength;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(data != NULL);
  assert(dataLength < sizeof(encryptedBuffer));
  assert(encryptedData != NULL);
//fprintf(stderr,"%s, %d: data %d\n",__FILE__,__LINE__,dataLength); debugDumpMemory(data,dataLength,0);

  // allocate secure memory
  bufferLength = dataLength;
  buffer = allocSecure(bufferLength);
  if (buffer == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }

  // encode data (XOR with session id)
//fprintf(stderr,"%s, %d: session id\n",__FILE__,__LINE__); debugDumpMemory(serverIO->sessionId,SESSION_ID_LENGTH,0);
  for (i = 0; i < bufferLength; i++)
  {
    buffer[i] = ((const byte*)data)[i]^serverIO->sessionId[i%SESSION_ID_LENGTH];
  }
//fprintf(stderr,"%s, %d: encoded data %d\n",__FILE__,__LINE__,bufferLength); debugDumpMemory(buffer,bufferLength,0);

  // encrypt
  switch (encryptType)
  {
    case SERVER_IO_ENCRYPT_TYPE_NONE:
      memCopy(encryptedBuffer,sizeof(encryptedBuffer),buffer,bufferLength);
      encryptedBufferLength = bufferLength;
      break;
    case SERVER_IO_ENCRYPT_TYPE_RSA:
      if      (Crypt_isAsymmetricSupported())
      {
        error = Crypt_encryptWithPublicKey(&serverIO->publicKey,
                                           buffer,
                                           bufferLength,
                                           encryptedBuffer,
                                           &encryptedBufferLength,
                                           sizeof(encryptedBuffer)
                                          );
        if (error != ERROR_NONE)
        {
          freeSecure(buffer);
          return error;
        }
//fprintf(stderr,"%s, %d: encryptedBuffer %d\n",__FILE__,__LINE__,encryptedBufferLength); debugDumpMemory(encryptedBuffer,encryptedBufferLength,0);
      }
      else
      {
        freeSecure(buffer);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      }
      break;
  }

  // convert to base64-string
  String_setCString(encryptedData,"base64:");
  Misc_base64Encode(encryptedData,encryptedBuffer,encryptedBufferLength);
//fprintf(stderr,"%s, %d: encryptedBufferLength=%d base64=%s\n",__FILE__,__LINE__,encryptedBufferLength,String_cString(encryptedData));

  // free resources
  freeSecure(buffer);

  return ERROR_NONE;
}

Errors ServerIO_decryptString(const ServerIO       *serverIO,
                              String               string,
                              ServerIOEncryptTypes encryptType,
                              ConstString          encryptedData
                             )
{
  Errors error;
  void   *data;
  uint   dataLength;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(string != NULL);
  assert(encryptedData != NULL);

  // decrypt
  error = ServerIO_decryptData(serverIO,
                               encryptType,
                               encryptedData,
                               &data,
                               &dataLength
                              );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // set string
  String_setBuffer(string,data,dataLength);

  // free resources
  ServerIO_decryptDone(data,dataLength);

  return ERROR_NONE;
}

Errors ServerIO_decryptPassword(const ServerIO       *serverIO,
                                Password             *password,
                                ServerIOEncryptTypes encryptType,
                                ConstString          encryptedPassword
                               )
{
  Errors error;
  void   *data;
  uint   dataLength;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(password != NULL);
  assert(encryptedPassword != NULL);

  // decrypt
  error = ServerIO_decryptData(serverIO,
                               encryptType,
                               encryptedPassword,
                               &data,
                               &dataLength
                              );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // set password
  Password_setBuffer(password,data,dataLength);

  // free resources
  ServerIO_decryptDone(data,dataLength);

  return ERROR_NONE;
}

Errors ServerIO_decryptKey(const ServerIO       *serverIO,
                           CryptKey             *cryptKey,
                           ServerIOEncryptTypes encryptType,
                           ConstString          encryptedKey
                          )
{
  byte   encryptedBuffer[2048];
  uint   encryptedBufferLength;
  Errors error;
  byte   encodedBuffer[2048];
  uint   encodedBufferLength;
  byte   *keyData;
  uint   keyDataLength;
  uint   i;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(cryptKey != NULL);
  assert(encryptedKey != NULL);

  // decode base64
  if (!Misc_base64Decode(encryptedBuffer,&encryptedBufferLength,encryptedKey,STRING_BEGIN,sizeof(encryptedBuffer)))
  {
    return FALSE;
  }

  // decrypt key
  switch (encryptType)
  {
    case SERVER_IO_ENCRYPT_TYPE_NONE:
      memcpy(encodedBuffer,encryptedBuffer,encryptedBufferLength);
      encodedBufferLength = encryptedBufferLength;
      break;
    case SERVER_IO_ENCRYPT_TYPE_RSA:
      if (Crypt_isAsymmetricSupported())
      {
    //fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,encryptedBufferLength);
        error = Crypt_decryptWithPrivateKey(&serverIO->privateKey,
                                            encryptedBuffer,
                                            encryptedBufferLength,
                                            encodedBuffer,
                                            &encodedBufferLength,
                                            sizeof(encodedBuffer)
                                           );
        if (error != ERROR_NONE)
        {
//          freeSecure(buffer);
return ERROR_UNKNOWN;
        }
      }
      else
      {
//        freeSecure(buffer);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      }
      break;
  }

//fprintf(stderr,"%s, %d: n=%d s='",__FILE__,__LINE__,encodedBufferLength); for (i = 0; i < encodedBufferLength; i++) { fprintf(stderr,"%c",encodedBuffer[i]^clientInfo->sessionId[i]); } fprintf(stderr,"'\n");

  // decode key (XOR with session id)
  keyDataLength = encodedBufferLength;
  keyData = allocSecure(keyDataLength);
  if (keyData == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }
  for (i = 0; i < encodedBufferLength; i++)
  {
    keyData[i] = encodedBuffer[i]^serverIO->sessionId[i];
  }
  error = Crypt_setPublicPrivateKeyData(cryptKey,
                                        keyData,
                                        keyDataLength,
                                        CRYPT_MODE_NONE,
                                        CRYPT_KEY_DERIVE_NONE,
                                        NULL,  // cryptSalt
                                        NULL  // password
                                       );
  if (error != ERROR_NONE)
  {
    freeSecure(keyData);
return ERROR_UNKNOWN;
  }
  freeSecure(keyData);

  return ERROR_NONE;
}

bool ServerIO_verifyPassword(const ServerIO       *serverIO,
                             ServerIOEncryptTypes encryptType,
                             ConstString          encryptedPassword,
                             const Password       *password
                            )
{
  Errors error;
  void   *data;
  uint   dataLength;
  uint   i;
  bool   okFlag;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(password != NULL);

  // decrypt s
//fprintf(stderr,"%s, %d: encryptedPassword=%s\n",__FILE__,__LINE__,String_cString(encryptedPassword));
  error = ServerIO_decryptData(serverIO,
                               encryptType,
                               encryptedPassword,
                               &data,
                               &dataLength
                              );
  if (error != ERROR_NONE)
  {
    return error;
  }
//fprintf(stderr,"%s, %d: n=%d s='",__FILE__,__LINE__,encodedBufferLength); for (i = 0; i < encodedBufferLength; i++) { fprintf(stderr,"%c",encodedBuffer[i]^clientInfo->sessionId[i]); } fprintf(stderr,"'\n");
//fprintf(stderr,"%s, %d: decrypted password\n",__FILE__,__LINE__); debugDumpMemory(data,dataLength,0);

  // check password
  okFlag = TRUE;
  if (password != NULL)
  {
    i = 0;
    while (   okFlag
           && (i < dataLength)
           && (i < Password_length(password))
          )
    {
      okFlag = (Password_getChar(password,i) == ((const byte*)data)[i]);
      i++;
    }
  }

  // free resources
  ServerIO_decryptDone(data,dataLength);

  return okFlag;
}

#if 0
bool ServerIO_verifyPasswordHash(const ServerIO       *serverIO,
                                 ServerIOEncryptTypes encryptType,
                                 ConstString          encryptedPassword,
                                 const CryptHash      *passwordHash
                                )
{
  Errors error;
  void   *data;
  uint   dataLength;
  uint   n;
  uint   i;
  bool   okFlag;

  // decrypt password
  error = ServerIO_decryptData(serverIO,
                               encryptType,
                               encryptedPassword,
                               &data,
                               &dataLength
                              );
  if (error != ERROR_NONE)
  {
    return error;
  }
//fprintf(stderr,"%s, %d: n=%d s='",__FILE__,__LINE__,encodedBufferLength); for (i = 0; i < encodedBufferLength; i++) { fprintf(stderr,"%c",encodedBuffer[i]^clientInfo->sessionId[i]); } fprintf(stderr,"'\n");

  // get password hash

  // check password hash
  okFlag = TRUE;
#if 0
  if (password != NULL)
  {
    okFlag = (Password_length(password) == dataLength);

    i = 0;
    while (okFlag && (i < Password_length(password)))
    {
      okFlag = (Password_getChar(password,i) == data[i]);
      i++;
    }
  }
#endif

  // free resources
  ServerIO_decryptDone(data,dataLength);

  return okFlag;
}
#endif

bool ServerIO_verifyHash(const ServerIO       *serverIO,
                         ServerIOEncryptTypes encryptType,
                         ConstString          encryptedData,
                         const CryptHash      *requiredHash
                        )
{
  Errors    error;
  void      *data;
  uint      dataLength;
  CryptHash hash;
  bool okFlag;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(encryptedData != NULL);
  assert(requiredHash != NULL);

fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
  // decrypt and get hash
  error = ServerIO_decryptData(serverIO,
                               encryptType,
                               encryptedData,
                               &data,
                               &dataLength
                              );
  if (error != ERROR_NONE)
  {
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,Error_getText(error));
    return error;
  }
#ifndef NDEBUG
fprintf(stderr,"%s, %d: decrypted data: %d\n",__FILE__,__LINE__,dataLength); debugDumpMemory(data,dataLength,FALSE);
#endif

  // get hash
  error = Crypt_initHash(&hash,requiredHash->cryptHashAlgorithm);
  if (error != ERROR_NONE)
  {
    ServerIO_decryptDone(data,dataLength);
    return error;
  }
  Crypt_updateHash(&hash,data,dataLength);

  // compare hashes
  okFlag = Crypt_equalsHash(requiredHash,&hash);

  // free resources
  Crypt_doneHash(&hash);
  ServerIO_decryptDone(data,dataLength);

  return okFlag;
}

// ----------------------------------------------------------------------

//TODO
void ServerIO_clearWait(ServerIO *serverIO)
{
  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);

  serverIO->pollfdCount = 0;
}

void ServerIO_addWait(ServerIO *serverIO,
                      int      handle
                     )
{
  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(serverIO->pollfds != NULL);

fprintf(stderr,"%s, %d: serverIO->pollfdCount=%d\n",__FILE__,__LINE__,serverIO->pollfdCount);
  if (serverIO->pollfdCount >= serverIO->maxPollfdCount)
  {
    serverIO->maxPollfdCount += 64;
    serverIO->pollfds = (struct pollfd*)realloc(serverIO->pollfds,serverIO->maxPollfdCount);
    if (serverIO->pollfds == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
  }
  serverIO->pollfds[serverIO->pollfdCount].fd     = handle;
  serverIO->pollfds[serverIO->pollfdCount].events = POLLIN|POLLERR|POLLNVAL;
  serverIO->pollfdCount++;
}

void ServerIO_wait(ServerIO *serverIO)
{
  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(serverIO->pollfds != NULL);
}

bool ServerIO_receiveData(ServerIO *serverIO)
{
  uint   maxBytes;
  ulong  readBytes;
  Errors error;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(serverIO->inputBufferIndex <= serverIO->inputBufferLength);

  // shift input buffer
  if (serverIO->inputBufferIndex > 0)
  {
    memCopy(&serverIO->inputBuffer[0],serverIO->inputBufferSize,
            &serverIO->inputBuffer[serverIO->inputBufferIndex],serverIO->inputBufferLength-serverIO->inputBufferIndex
           );
    serverIO->inputBufferLength -= serverIO->inputBufferIndex;
    serverIO->inputBufferIndex = 0;
  }
  assert(serverIO->inputBufferIndex == 0);

  // get max. number of bytes to receive
  maxBytes = serverIO->inputBufferSize-serverIO->inputBufferLength;

  if (maxBytes > 0)
  {
    switch (serverIO->type)
    {
      case SERVER_IO_TYPE_NONE:
        break;
      case SERVER_IO_TYPE_BATCH:
        (void)File_read(&serverIO->file.fileHandle,
                        &serverIO->inputBuffer[serverIO->inputBufferLength],
                        maxBytes,
                        &readBytes
                       );
//fprintf(stderr,"%s, %d: readBytes=%d buffer=%s\n",__FILE__,__LINE__,readBytes,buffer);
        if (readBytes > 0)
        {
          do
          {
            serverIO->inputBufferLength += (uint)readBytes;

            maxBytes = serverIO->inputBufferSize-serverIO->inputBufferLength;
            error = File_read(&serverIO->file.fileHandle,
                              &serverIO->inputBuffer[serverIO->inputBufferLength],
                              maxBytes,
                              &readBytes
                             );
          }
          while ((error == ERROR_NONE) && (readBytes > 0));
        }
        else
        {
          // disconnect
          return FALSE;
        }
        break;
      case SERVER_IO_TYPE_NETWORK:
        (void)Network_receive(&serverIO->network.socketHandle,
                              &serverIO->inputBuffer[serverIO->inputBufferLength],
                              maxBytes,
                              NO_WAIT,
                              &readBytes
                             );
//fprintf(stderr,"%s, %d: received socket: maxBytes=%d received=%d at %d: ",__FILE__,__LINE__,maxBytes,readBytes,serverIO->inputBufferLength);
//fwrite(&serverIO->inputBuffer[serverIO->inputBufferLength],readBytes,1,stderr); fprintf(stderr,"\n");
        if (readBytes > 0)
        {
          do
          {
//fprintf(stderr,"%s, %d: readBytes=%d: %d\n",__FILE__,__LINE__,readBytes,serverIO->inputBufferLength); debugDumpMemory(&serverIO->inputBuffer[serverIO->inputBufferLength],readBytes,0);
            serverIO->inputBufferLength += (uint)readBytes;

            maxBytes = serverIO->inputBufferSize-serverIO->inputBufferLength;
            error = Network_receive(&serverIO->network.socketHandle,
                                    &serverIO->inputBuffer[serverIO->inputBufferLength],
                                    maxBytes,
                                    NO_WAIT,
                                    &readBytes
                                   );
          }
          while ((error == ERROR_NONE) && (readBytes > 0));
        }
        else
        {
          // disconnect
          return FALSE;
        }
        break;
    }
  }

  return TRUE;
}

bool ServerIO_getCommand(ServerIO  *serverIO,
                         uint      *id,
                         String    name,
                         StringMap argumentMap
                        )
{
  bool commandFlag;
  uint               resultId;
  bool               completedFlag;
  uint               errorCode;
  String             data;
  ServerIOResultNode *resultNode;
  SemaphoreLock      semaphoreLock;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(id != NULL);
  assert(name != NULL);

  // init variables
  data = String_new();

  commandFlag = FALSE;
  while (   !commandFlag
         && getLine(serverIO)
        )
  {
    // parse
    if      (String_parse(serverIO->line,STRING_BEGIN,"%u %y %u % S",NULL,&resultId,&completedFlag,&errorCode,data))
    {
      // result
      #ifndef NDEBUG
        if (globalOptions.serverDebugLevel >= 1)
        {
          fprintf(stderr,"DEBUG: received result #%u completed=%d error=%d: %s\n",resultId,completedFlag,errorCode,String_cString(data));
        }
      #endif /* not DEBUG */

      // init result
      resultNode = LIST_NEW_NODE(ServerIOResultNode);
      if (resultNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      resultNode->id            = resultId;
      resultNode->error         = (errorCode != 0) ? ERRORF_(errorCode,"%s",String_cString(data)) : ERROR_NONE;
      resultNode->completedFlag = completedFlag;
      resultNode->data          = String_duplicate(data);

//TODO: parse arguments?

      // add result
      SEMAPHORE_LOCKED_DO(semaphoreLock,&serverIO->resultList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        List_append(&serverIO->resultList,resultNode);
//fprintf(stderr,"%s, %d: appended result: %d %d %d %s\n",__FILE__,__LINE__,resultNode->id,resultNode->error,resultNode->completedFlag,String_cString(resultNode->data));
      }
    }
    else if (String_parse(serverIO->line,STRING_BEGIN,"%u %S % S",NULL,id,name,data))
    {
      // command
      #ifndef NDEBUG
        if (globalOptions.serverDebugLevel >= 1)
        {
          fprintf(stderr,"DEBUG: received command #%u name=%s: %s\n",*id,String_cString(name),String_cString(data));
        }
      #endif /* not DEBUG */

      // parse arguments
      if (argumentMap != NULL)
      {
//fprintf(stderr,"%s, %d: parse %s\n",__FILE__,__LINE__,String_cString(commandNode->data));
        commandFlag = StringMap_parse(argumentMap,data,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,STRING_BEGIN,NULL);
      }
      else
      {
        commandFlag = TRUE;
      }
    }
    else
    {
//TODO
fprintf(stderr,"DEBUG: skipped unknown data: %s\n",String_cString(serverIO->line));
      // unknown
      #ifndef NDEBUG
        if (globalOptions.serverDebugLevel >= 1)
        {
          fprintf(stderr,"DEBUG: skipped unknown data: %s\n",String_cString(serverIO->line));
        }
      #endif /* not DEBUG */
    }

    // done line
    doneLine(serverIO);
  }

  // free resources
  String_delete(data);

  return commandFlag;
}

Errors ServerIO_vsendCommand(ServerIO   *serverIO,
                             uint       debugLevel,
                             uint       *id,
                             const char *format,
                             va_list    arguments
                            )
{
  String   s;
  locale_t locale;
  Errors   error;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(id != NULL);
  assert(format != NULL);

  // init variables
  s = String_new();

  // get new command id
  (*id) = atomicIncrement(&serverIO->commandId,1);

  // format command
  locale = uselocale(POSIXLocale);
  {
    String_format(s,"%u ",*id);
    String_vformat(s,format,arguments);
  }
  uselocale(locale);

  // send command
  error = sendData(serverIO,s);
  if (error != ERROR_NONE)
  {
    String_delete(s);
    return error;
  }
  #ifndef NDEBUG
    if (globalOptions.serverDebugLevel >= debugLevel)
    {
      fprintf(stderr,"DEBUG: sent command '%s'\n",String_cString(s));
    }
  #endif /* not DEBUG */

  // free resources
  String_delete(s);

  return ERROR_NONE;
}

Errors ServerIO_sendCommand(ServerIO   *serverIO,
                            uint       debugLevel,
                            uint       *id,
                            const char *format,
                            ...
                           )
{
  va_list arguments;
  Errors  error;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(format != NULL);

  va_start(arguments,format);
  error = ServerIO_vsendCommand(serverIO,debugLevel,id,format,arguments);
  va_end(arguments);

  return error;
}

Errors ServerIO_vexecuteCommand(ServerIO   *serverIO,
                                uint       debugLevel,
                                long       timeout,
                                StringMap  resultMap,
                                const char *format,
                                va_list    arguments
                               )
{
  uint   id;
  Errors error;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(format != NULL);

  error = ERROR_UNKNOWN;

  // send command
  error = ServerIO_vsendCommand(serverIO,
                                debugLevel,
                                &id,
                                format,
                                arguments
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // wait for result
  error = ServerIO_waitResult(serverIO,
                              timeout,
                              id,
                              NULL, // &completedFlag,
                              resultMap
                             );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors ServerIO_executeCommand(ServerIO   *serverIO,
                               uint       debugLevel,
                               long       timeout,
                               StringMap  resultMap,
                               const char *format,
                               ...
                              )
{
  va_list arguments;
  Errors  error;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(format != NULL);

  va_start(arguments,format);
  error = ServerIO_vexecuteCommand(serverIO,debugLevel,timeout,resultMap,format,arguments);
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
  String   s;
  locale_t locale;
  va_list  arguments;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(format != NULL);

  // init variables
  s = String_new();

  // format result
  locale = uselocale(POSIXLocale);
  {
    String_format(s,"%u %d %u ",id,completedFlag ? 1 : 0,Error_getCode(error));
    va_start(arguments,format);
    String_vformat(s,format,arguments);
    va_end(arguments);
  }
  uselocale(locale);

  // send result
  error = sendData(serverIO,s);
  if (error != ERROR_NONE)
  {
    String_delete(s);
    return error;
  }
  #ifndef NDEBUG
    if (globalOptions.serverDebugLevel >= 1)
    {
      fprintf(stderr,"DEBUG: send result '%s'\n",String_cString(s));
    }
  #endif /* not DEBUG */

  // free resources
  String_delete(s);

  return ERROR_NONE;
}

Errors ServerIO_waitResult(ServerIO  *serverIO,
                           long      timeout,
                           uint      id,
                           bool      *completedFlag,
                           StringMap resultMap
                          )
{
  SemaphoreLock      semaphoreLock;
  ServerIOResultNode *resultNode;
  Errors             error;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);

  // wait for result
  resultNode = NULL;
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
        // not found -> wait
        if (!Semaphore_waitModified(&serverIO->resultList.lock,timeout))
        {
          break;
        }
      }
    }
    while (resultNode == NULL);
  }
  if (resultNode == NULL)
  {
    return ERROR_NETWORK_TIMEOUT;
  }

  // get and parse result
  error = resultNode->error;
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

  return error;
}

Errors ServerIO_clientAction(ServerIO   *serverIO,
                             long       timeout,
                             uint       id,
                             StringMap  resultMap,
                             const char *actionCommand,
                             const char *format,
                             ...
                            )
{
  String             s;
  locale_t           locale;
  va_list            arguments;
  SemaphoreLock      semaphoreLock;
  ServerIOResultNode *resultNode;
  Errors             error;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(actionCommand != NULL);

  error = ERROR_UNKNOWN;

  // init variables
  s = String_new();

  // get new command id
  id = atomicIncrement(&serverIO->commandId,1);

  // format action
  locale = uselocale(POSIXLocale);
  {
    String_format(s,"%u 0 0 action=%s ",id,actionCommand);
    va_start(arguments,format);
    String_vformat(s,format,arguments);
    va_end(arguments);
  }
  uselocale(locale);

  // send action
  sendData(serverIO,s);
  if (error != ERROR_NONE)
  {
    String_delete(s);
    return error;
  }
  #ifndef NDEBUG
    if (globalOptions.serverDebugLevel >= 1)
    {
      fprintf(stderr,"DEBUG: send action '%s'\n",String_cString(s));
    }
  #endif /* not DEBUG */

  // free resources
  String_delete(s);

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
  String        s;
//uint commandId;
  va_list       arguments;
  Errors        error;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(format != NULL);

  // init variables
  s = String_new();

error=ERROR_NONE;
//commandId = 0;
UNUSED_VARIABLE(resultList);

    // send command
    va_start(arguments,format);
//    sendData(serverIO,commandId,format,arguments);
    va_end(arguments);

    // wait for results
//TODO

  // free resources
  String_delete(s);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
