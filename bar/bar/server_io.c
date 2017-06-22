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

#define SESSION_KEY_SIZE                         1024     // number of session key bits

#define LOCK_TIMEOUT                             (10*60*1000)  // general lock timeout [ms]

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

LOCAL void disconnect(ServerIO *serverIO)
{
  assert(serverIO != NULL);

  switch (serverIO->type)
  {
    case SERVER_IO_TYPE_NONE:
      break;
    case SERVER_IO_TYPE_BATCH:
      break;
    case SERVER_IO_TYPE_NETWORK:
      Network_disconnect(&serverIO->network.socketHandle);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }
  serverIO->isConnected = FALSE;
}

/***********************************************************************\
* Name   : fillLine
* Purpose: fill-in line from input buffer
* Input  : serverIO - server i/o
* Output : -
* Return : TRUE iff line available
* Notes  : -
\***********************************************************************/

LOCAL bool fillLine(ServerIO *serverIO)
{
  char ch;

  assert(serverIO != NULL);

  while (   !serverIO->lineFlag
         && (serverIO->inputBufferIndex < serverIO->inputBufferLength)
        )
  {
    ch = serverIO->inputBuffer[serverIO->inputBufferIndex]; serverIO->inputBufferIndex++;
    if (ch != '\n')
    {
assert(ch != 0);
      String_appendChar(serverIO->line,ch);
    }
    else
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
fprintf(stderr,"%s, %d: uuuuuuuuuuuuuuuuuuuuuuu\n",__FILE__,__LINE__);
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
        disconnect(serverIO);
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
//        disconnect(serverIO);
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
    disconnect(serverIO);
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
    if (globalOptions.serverDebugFlag)
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
}

void ServerIO_done(ServerIO *serverIO)
{
  assert(serverIO != NULL);

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
                             ConstString  hostName,
                             uint         hostPort,
                             SocketHandle socketHandle
                            )
{
  assert(serverIO != NULL);

  // init variables
  serverIO->type                 = SERVER_IO_TYPE_NETWORK;
  serverIO->network.name         = String_duplicate(hostName);
  serverIO->network.port         = hostPort;
  serverIO->network.socketHandle = socketHandle;
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

  disconnect(serverIO);
}

Errors ServerIO_sendSessionId(ServerIO *serverIO)
{
  String encodedId;
  String n,e;
  String s;
  Errors error;

  assert(serverIO != NULL);

  // init variables
  s = String_new();

  // format session data
  encodedId = encodeHex(String_new(),serverIO->sessionId,sizeof(SessionId));
  n         = Crypt_getPublicPrivateKeyModulus(&serverIO->publicKey);
  e         = Crypt_getPublicPrivateKeyExponent(&serverIO->publicKey);
  if ((n !=NULL) && (e != NULL))
  {
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
    String_delete(e);
    String_delete(n);
    String_delete(encodedId);
    String_delete(s);
    return error;
  }
  #ifndef NDEBUG
    if (globalOptions.serverDebugFlag)
    {
      fprintf(stderr,"DEBUG: send session id '%s'\n",String_cString(s));
    }
  #endif /* not DEBUG */

  // free resources
  String_delete(e);
  String_delete(n);
  String_delete(encodedId);
  String_delete(s);

  return ERROR_NONE;
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
    Crypt_decryptKey(&serverIO->privateKey,
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
    if (Crypt_decryptKey(&serverIO->privateKey,
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

//TODO
void ServerIO_clearWait(ServerIO *serverIO)
{
  assert(serverIO != NULL);

  serverIO->pollfdCount = 0;
}

void ServerIO_addWait(ServerIO *serverIO,
                      int      handle
                     )
{
  assert(serverIO != NULL);
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
  assert(serverIO->pollfds != NULL);
}

bool ServerIO_receiveData(ServerIO *serverIO)
{
  uint   maxBytes;
  ulong  readBytes;
  Errors error;

  assert(serverIO != NULL);
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
            error = File_read(&serverIO->file.fileHandle,&serverIO->inputBuffer[serverIO->inputBufferLength],maxBytes,&readBytes);
          }
          while ((error == ERROR_NONE) && (readBytes > 0));
        }
        else
        {
          // disconnect
fprintf(stderr,"%s, %d: DISCONNECT?\n",__FILE__,__LINE__);
          disconnect(serverIO);
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
//fprintf(stderr,"%s, %d: +++++++++++++ rec socket: maxBytes=%d received=%d at %d: ",__FILE__,__LINE__,maxBytes,readBytes,serverIO->inputBufferLength);
//fwrite(&serverIO->inputBuffer[serverIO->inputBufferLength],readBytes,1,stderr); fprintf(stderr,"\n");
        if (readBytes > 0)
        {
          do
          {
            serverIO->inputBufferLength += (uint)readBytes;

            maxBytes = serverIO->inputBufferSize-serverIO->inputBufferLength;
            error = Network_receive(&serverIO->network.socketHandle,&serverIO->inputBuffer[serverIO->inputBufferLength],maxBytes,NO_WAIT,&readBytes);
          }
          while ((error == ERROR_NONE) && (readBytes > 0));
        }
        else
        {
          // disconnect
fprintf(stderr,"%s, %d: DISCONNECT?\n",__FILE__,__LINE__);
//        disconnect(serverIO);
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
  assert(id != NULL);
  assert(name != NULL);

  // init variables
  data = String_new();

  commandFlag = FALSE;
  while (   !commandFlag
         && fillLine(serverIO)
        )
  {
    // parse
    if      (String_parse(serverIO->line,STRING_BEGIN,"%u %y %u % S",NULL,&resultId,&completedFlag,&errorCode,data))
    {
      // result
      #ifndef NDEBUG
        if (globalOptions.serverDebugFlag)
        {
          fprintf(stderr,"DEBUG: receive result #%u completed=%d error=%d: %s\n",resultId,completedFlag,errorCode,String_cString(data));
        }
      #endif /* not DEBUG */

      // init result
      resultNode = LIST_NEW_NODE(ServerIOResultNode);
      if (resultNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      resultNode->id            = resultId;
      resultNode->error         = (errorCode != 0) ? Errorx_(errorCode,0,"%s",String_cString(data)) : ERROR_NONE;
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
        if (globalOptions.serverDebugFlag)
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
asm("int3");
String_parse(serverIO->line,STRING_BEGIN,"%u %S % S",NULL,id,name,data);
      // unknown
      #ifndef NDEBUG
        if (globalOptions.serverDebugFlag)
        {
          fprintf(stderr,"DEBUG: skipped unknown data: %s\n",String_cString(serverIO->line));
        }
      #endif /* not DEBUG */
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,String_cString(serverIO->line));
exit(1);
    }

    // done line
    doneLine(serverIO);
  }

  // free resources
  String_delete(data);

  return commandFlag;
}

Errors ServerIO_vsendCommand(ServerIO   *serverIO,
                             uint       *id,
                             const char *format,
                             va_list    arguments
                            )
{
  String   s;
  locale_t locale;
  Errors   error;

  assert(serverIO != NULL);
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
    if (globalOptions.serverDebugFlag)
    {
      fprintf(stderr,"DEBUG: sent command '%s'\n",String_cString(s));
    }
  #endif /* not DEBUG */

  // free resources
  String_delete(s);

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

Errors ServerIO_vexecuteCommand(ServerIO   *serverIO,
                                long       timeout,
                                StringMap  resultMap,
                                const char *format,
                                va_list    arguments
                               )
{
  uint   id;
  Errors error;

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

  // wait for result
  error = ServerIO_waitResult(serverIO,
                              timeout,
                              id,
                              NULL, // &error,
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
                               long       timeout,
                               StringMap  resultMap,
                               const char *format,
                               ...
                              )
{
  va_list arguments;
  Errors  error;

  va_start(arguments,format);
  error = ServerIO_vexecuteCommand(serverIO,timeout,resultMap,format,arguments);
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
    if (globalOptions.serverDebugFlag)
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
    if (globalOptions.serverDebugFlag)
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
