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
#include <locale.h>
#include <assert.h>

#include "common/files.h"
#include "common/global.h"
#include "common/lists.h"
#include "common/misc.h"
#include "common/network.h"
#include "common/semaphores.h"
#include "common/stringmaps.h"
#include "common/strings.h"

// TODO: remove bar.h
#include "bar.h"
#include "bar_common.h"
#include "crypt.h"

#include "server_io.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define SESSION_KEY_SIZE  1024     // number of session key bits

#define LOCK_TIMEOUT      (10LL*MS_PER_MINUTE)  // general lock timeout [ms]
#define READ_TIMEOUT      (5LL*MS_PER_SECOND)

#define BUFFER_SIZE       (64*1024)
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

  StringMap_delete(resultNode->resultMap);
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
* Name   : initIO
* Purpose: init i/o
* Input  : serverIO - server i/o
*          type     - type; see SERVER_IO_TYPE_...
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initIO(ServerIO *serverIO, ServerIOTypes type)
{
  assert(serverIO != NULL);

  serverIO->type              = type;
  Semaphore_init(&serverIO->lock,SEMAPHORE_TYPE_BINARY);

  serverIO->type              = type;

  #ifndef NO_SESSION_ID
    Crypt_randomize(serverIO->sessionId,sizeof(SessionId));
  #else /* not NO_SESSION_ID */
    memClear(serverIO->sessionId,sizeof(SessionId));
  #endif /* NO_SESSION_ID */
  serverIO->encryptType       = SERVER_IO_ENCRYPT_TYPE_NONE;
  Crypt_initKey(&serverIO->publicKey,CRYPT_PADDING_TYPE_PKCS1);
  Crypt_initKey(&serverIO->privateKey,CRYPT_PADDING_TYPE_PKCS1);

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

  serverIO->commandId         = 1;
  Semaphore_init(&serverIO->resultList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&serverIO->resultList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeResultNode,NULL));

  switch (type)
  {
    case SERVER_IO_TYPE_NONE:
      break;
    case SERVER_IO_TYPE_NETWORK:
      serverIO->network.name = String_new();
      break;
    case SERVER_IO_TYPE_BATCH:
      serverIO->file.isConnected = FALSE;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }
}

/***********************************************************************\
* Name   : doneIO
* Purpose: done server i/o
* Input  : serverIO - server i/o
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneIO(ServerIO *serverIO)
{
  assert(serverIO != NULL);

  switch (serverIO->type)
  {
    case SERVER_IO_TYPE_NONE:
      break;
    case SERVER_IO_TYPE_NETWORK:
      String_delete(serverIO->network.name);
      break;
    case SERVER_IO_TYPE_BATCH:
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }

  List_done(&serverIO->resultList);
  Semaphore_done(&serverIO->resultList.lock);
  String_delete(serverIO->line);
  free(serverIO->outputBuffer);
  free(serverIO->inputBuffer);
  Crypt_doneKey(&serverIO->privateKey);
  Crypt_doneKey(&serverIO->publicKey);
  Semaphore_done(&serverIO->lock);

  serverIO->type = SERVER_IO_TYPE_NONE;
}

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
    if      (!iscntrl(ch))
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
  assert(serverIO != NULL);

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
  Errors error;
  uint   n;
  uint   newOutputBufferSize;

  assert(serverIO != NULL);
  assert(line != NULL);

  error = ERROR_NETWORK_TIMEOUT_SEND;
  SEMAPHORE_LOCKED_DO(&serverIO->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // get line length
    n = String_length(line);

//TODO: avoid copy and add LF?
    // extend output buffer if needed
    if ((n+1) > serverIO->outputBufferSize)
    {
      newOutputBufferSize = ALIGN((n+1),BUFFER_DELTA_SIZE);
//fprintf(stderr,"%s, %d: extend output buffer %d -> %d\n",__FILE__,__LINE__,serverIO->outputBufferSize,newOutputBufferSize);
      serverIO->outputBuffer = (char*)realloc(serverIO->outputBuffer,newOutputBufferSize);
      if (serverIO->outputBuffer == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      serverIO->outputBufferSize = newOutputBufferSize;
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
        error = File_write(&serverIO->file.outputHandle,serverIO->outputBuffer,n+1);
        (void)File_flush(&serverIO->file.outputHandle);
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

  return error;
}

/***********************************************************************\
* Name   : receiveData
* Purpose: receive data
* Input  : serverIO - server i/o
*          timeout  - timeout [ms] or WAIT_FOREVER/NO_WAIT
* Output : -
* Return : TRUE if received data, FALSE on disconnect
* Notes  : -
\***********************************************************************/

LOCAL bool receiveData(ServerIO *serverIO, long timeout)
{
  uint  maxBytes;
  ulong readBytes;

  assert(serverIO != NULL);
  assert(serverIO->inputBufferIndex <= serverIO->inputBufferLength);
  assert(serverIO->inputBufferLength <= serverIO->inputBufferSize);

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
      case SERVER_IO_TYPE_NETWORK:
        (void)Network_receive(&serverIO->network.socketHandle,
                              &serverIO->inputBuffer[serverIO->inputBufferLength],
                              maxBytes,
                              timeout,
                              &readBytes
                             );
//fprintf(stderr,"%s, %d: received socket: maxBytes=%d received=%d at %d: ",__FILE__,__LINE__,maxBytes,readBytes,serverIO->inputBufferLength);
//fwrite(&serverIO->inputBuffer[serverIO->inputBufferLength],readBytes,1,stderr); fprintf(stderr,"\n");
        if (readBytes > 0)
        {
          // read as much as possible
          do
          {
//fprintf(stderr,"%s, %d: readBytes=%d: %d\n",__FILE__,__LINE__,readBytes,serverIO->inputBufferLength); debugDumpMemory(&serverIO->inputBuffer[serverIO->inputBufferLength],readBytes,0);
            serverIO->inputBufferLength += (uint)readBytes;

//fprintf(stderr,"%s:%d: c index=%d length=%d size=%d\n",__FILE__,__LINE__,serverIO->inputBufferIndex,serverIO->inputBufferLength,serverIO->inputBufferSize);
            maxBytes = serverIO->inputBufferSize-serverIO->inputBufferLength;
            if (maxBytes > 0)
            {
              (void)Network_receive(&serverIO->network.socketHandle,
                                    &serverIO->inputBuffer[serverIO->inputBufferLength],
                                    maxBytes,
                                    NO_WAIT,
                                    &readBytes
                                   );
            }
            else
            {
              readBytes = 0L;
            }
          }
          while (readBytes > 0);
        }
        else
        {
          // disconnect
          return FALSE;
        }
        break;
      case SERVER_IO_TYPE_BATCH:
        // read only single character
// TODO: how to read as much characters as possible without blocking?
#if 1
        (void)File_read(&serverIO->file.inputHandle,
                        &serverIO->inputBuffer[serverIO->inputBufferLength],
                        1,
                        &readBytes
                       );
        if (readBytes > 0)
        {
          serverIO->inputBufferLength += (uint)readBytes;
        }
        else
        {
          // disconnect
          return FALSE;
        }
#else
        (void)File_read(&serverIO->file.inputHandle,
                        &serverIO->inputBuffer[serverIO->inputBufferLength],
                        maxBytes,
                        &readBytes
                       );
//fprintf(stderr,"%s, %d: readBytes=%d buffer=%s\n",__FILE__,__LINE__,readBytes,serverIO->inputBuffer);
        if (readBytes > 0)
        {
          // read as much as possible
          do
          {
            serverIO->inputBufferLength += (uint)readBytes;

            maxBytes = serverIO->inputBufferSize-serverIO->inputBufferLength;
            if (maxBytes > 0)
            {
              (void)File_read(&serverIO->file.inputHandle,
                              &serverIO->inputBuffer[serverIO->inputBufferLength],
                              maxBytes,
                              &readBytes
                             );
            }
            else
            {
              readBytes = 0L;
            }
          }
          while (readBytes > 0);
        }
        else
        {
          // disconnect
          return FALSE;
        }
#endif
        break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
    }
  }

  return TRUE;
}

/***********************************************************************\
* Name   : vsendCommand
* Purpose: send command
* Input  : serverIO   - server i/o
*          debugLevel -
*          format     - command format string
*          arguments  - arguments for command format
* Output : id - command id
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors vsendCommand(ServerIO   *serverIO,
                          uint       debugLevel,
                          uint       *id,
                          const char *format,
                          va_list    arguments
                         )
{
  String   s;
  Errors   error;
  #ifdef HAVE_NEWLOCALE
    locale_t oldLocale;
  #endif /* HAVE_NEWLOCALE */

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(id != NULL);
  assert(format != NULL);

  // init variables
  s = String_new();

  // get new command id
  (*id) = atomicIncrement(&serverIO->commandId,1);

  // format command
  #ifdef HAVE_NEWLOCALE
    oldLocale = uselocale(POSIXLocale);
  #endif /* HAVE_NEWLOCALE */
  {
    String_format(s,"%u ",*id);
    String_appendVFormat(s,format,arguments);
  }
  #ifdef HAVE_NEWLOCALE
    uselocale(oldLocale);
  #endif /* HAVE_NEWLOCALE */

  // send command
  error = sendData(serverIO,s);
  if (error != ERROR_NONE)
  {
    String_delete(s);
    return error;
  }
  #ifndef NDEBUG
    if (globalOptions.debug.serverLevel >= debugLevel)
    {
      fprintf(stderr,"DEBUG: sent command %s\n",String_cString(s));
    }
  #else
    UNUSED_VARIABLE(debugLevel);
  #endif /* not DEBUG */

  // free resources
  String_delete(s);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : receiveResult
* Purpose: receive result synchronous
* Input  : serverIO              - server i/o
*          timeout               - timeout [ms] or WAIT_FOREVER
* Output : id            - command id
*          completedFlag - TRUE iff command is completed
*          resultMap     - resuits map
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors receiveResult(ServerIO  *serverIO,
                           long      timeout,
                           uint      *id,
                           bool      *completedFlag,
                           StringMap resultMap
                          )
{
  bool               resultFlag;
  TimeoutInfo        timeoutInfo;
  Errors             error;
  uint               errorCode;
  String             data;
  #ifndef NDEBUG
    size_t             iteratorVariable;
    Codepoint          codepoint;
  #endif /* not NDEBU G */

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(id != NULL);
  assert(completedFlag != NULL);
  assert(resultMap != NULL);

  // init variables
  data = String_new();
  Misc_initTimeout(&timeoutInfo,timeout);

  resultFlag = FALSE;
  error      = ERROR_UNKNOWN;
  do
  {
//fprintf(stderr,"%s, %d: serverIO->line=%s\n",__FILE__,__LINE__,String_cString(serverIO->line));
    while (   !getLine(serverIO)
           && !Misc_isTimeout(&timeoutInfo)
          )
    {
      receiveData(serverIO,Misc_getRestTimeout(&timeoutInfo,MAX_ULONG));
    }

    if (getLine(serverIO))
    {
      // parse
      if      (String_parse(serverIO->line,STRING_BEGIN,"%u %y %u % S",NULL,id,completedFlag,&errorCode,data))
      {
        // command results: <id> <complete flag> <error code> <data>
        #ifndef NDEBUG
          if (globalOptions.debug.serverLevel >= 1)
          {
            fprintf(stderr,"DEBUG: received result #%u completed=%d error=%d: %s\n",*id,*completedFlag,errorCode,String_cString(data));
          }
        #endif /* not DEBUG */

        // get result
        if (errorCode == ERROR_CODE_NONE)
        {
          if (!StringMap_parse(resultMap,data,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,STRING_BEGIN,NULL))
          {
            // parse error -> discard
            #ifndef NDEBUG
              if (globalOptions.debug.serverLevel >= 1)
              {
                fprintf(stderr,"DEBUG: parse result fail: %s\n",String_cString(data));
              }
            #endif /* not DEBUG */
            continue;
          }

          error = ERROR_NONE;
        }
        else
        {
          error = ERRORF_(errorCode,"%s",String_cString(data));
        }
        resultFlag = TRUE;
      }
      else
      {
        // unknown -> ignore
        #ifndef NDEBUG
          if (globalOptions.debug.serverLevel >= 1)
          {
            fprintf(stderr,"DEBUG: skipped unknown data: %s\n",String_cString(serverIO->line));
            STRING_CHAR_ITERATE_UTF8(serverIO->line,iteratorVariable,codepoint)
            {
              if (iswprint(codepoint))
              {
                fprintf(stderr,"%s",charUTF8(codepoint));
              }
            }
            fprintf(stderr,"\n");
          }
        #endif /* not DEBUG */
      }

      // done line
      doneLine(serverIO);
    }
  }
  while (   !resultFlag
         && !Misc_isTimeout(&timeoutInfo)
        );
  if (!resultFlag && Misc_isTimeout(&timeoutInfo))
  {
    error = ERROR_NETWORK_TIMEOUT_RECEIVE;
  }
  assert(error != ERROR_UNKNOWN);

  // free resources
  Misc_doneTimeout(&timeoutInfo);
  String_delete(data);

  return error;
}

/***********************************************************************\
* Name   : vsyncExecuteCommand
* Purpose: execute server command synchronous
* Input  : serverIO              - server i/o
*          debugLevel            - debug level
*          timeout               - timeout [ms] or WAIT_FOREVER
*          commandResultFunction - command result function (can be NULL)
*          commandResultUserData - user data for command result function
*          format                - format string
*          arguments             - arguments
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors vsyncExecuteCommand(ServerIO                      *serverIO,
                                 uint                          debugLevel,
                                 long                          timeout,
                                 ServerIOCommandResultFunction commandResultFunction,
                                 void                          *commandResultUserData,
                                 const char                    *format,
                                 va_list                       arguments
                                )
{
  uint        id;
  Errors      error;
  TimeoutInfo timeoutInfo;
  bool        completedFlag;
  StringMap   resultMap;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(format != NULL);

  // init variables
  Misc_initTimeout(&timeoutInfo,timeout);

  // send command
  error = vsendCommand(serverIO,
                       debugLevel,
                       &id,
                       format,
                       arguments
                      );
  if (error != ERROR_NONE)
  {
    Misc_doneTimeout(&timeoutInfo);
    return error;
  }

  // wait for result
  resultMap = StringMap_new();
  do
  {
    error = receiveResult(serverIO,
                          Misc_getRestTimeout(&timeoutInfo,MAX_ULONG),
                          &id,
                          &completedFlag,
                          resultMap
                         );
    if ((error == ERROR_NONE) && (commandResultFunction != NULL))
    {
      error = commandResultFunction(resultMap,commandResultUserData);
    }
  }
  while (   (error == ERROR_NONE)
         && !completedFlag
         && !Misc_isTimeout(&timeoutInfo)
        );
  StringMap_delete(resultMap);

  #ifndef NDEBUG
    if (globalOptions.debug.serverLevel >= 1)
    {
      if      (error != ERROR_NONE)
      {
        fprintf(stderr,"DEBUG: execute command %u: '%s' fail: %s\n",id,format,Error_getText(error));
      }
      else if (Misc_isTimeout(&timeoutInfo))
      {
        fprintf(stderr,"DEBUG: timeout execute command %u: '%s'\n",id,format);
      }
    }
  #endif /* not DEBUG */

  // free resources
  Misc_doneTimeout(&timeoutInfo);

  return error;
}

/***********************************************************************\
* Name   : syncExecuteCommand
* Purpose: execute server command synchronous
* Input  : serverIO              - server i/o
*          debugLevel            - debug level
*          timeout               - timeout [ms] or WAIT_FOREVER
*          commandResultFunction - command result function (can be NULL)
*          commandResultUserData - user data for command result function
*          format                - format string
*          ...                   - optional arguments
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors syncExecuteCommand(ServerIO                      *serverIO,
                                uint                          debugLevel,
                                long                          timeout,
                                ServerIOCommandResultFunction commandResultFunction,
                                void                          *commandResultUserData,
                                const char                    *format,
                                ...
                               )
{
  va_list arguments;
  Errors  error;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(format != NULL);

  va_start(arguments,format);
  error = vsyncExecuteCommand(serverIO,
                              debugLevel,
                              timeout,
                              commandResultFunction,
                              commandResultUserData,
                              format,
                              arguments
                             );
  va_end(arguments);

  return error;
}

// ----------------------------------------------------------------------

const char *ServerIO_encryptTypeToString(ServerIOEncryptTypes encryptType,
                                         const char           *defaultValue
                                        )
{
  const char *name;

  switch (encryptType)
  {
    case SERVER_IO_ENCRYPT_TYPE_NONE: name = "NONE";       break;
    case SERVER_IO_ENCRYPT_TYPE_RSA:  name = "RSA";        break;
    default:                          name = defaultValue; break;
  }

  return name;
}

bool ServerIO_parseEncryptType(const char           *encryptTypeText,
                               ServerIOEncryptTypes *encryptType,
                               void                 *userData
                              )
{
  assert(encryptTypeText != NULL);
  assert(encryptType != NULL);

  UNUSED_VARIABLE(userData);

  if      (stringEqualsIgnoreCase(encryptTypeText,"NONE")) (*encryptType) = SERVER_IO_ENCRYPT_TYPE_NONE;
  else if (stringEqualsIgnoreCase(encryptTypeText,"RSA" )) (*encryptType) = SERVER_IO_ENCRYPT_TYPE_RSA;
  else                                                     (*encryptType) = SERVER_IO_ENCRYPT_TYPE_NONE;

  return TRUE;
}

bool ServerIO_parseAction(const char      *actionText,
                          ServerIOActions *action,
                          void            *userData
                         )
{
  assert(actionText != NULL);
  assert(action != NULL);

  UNUSED_VARIABLE(userData);

  if      (stringEqualsIgnoreCase(actionText,"SKIP"    )) (*action) = SERVER_IO_ACTION_SKIP;
  else if (stringEqualsIgnoreCase(actionText,"SKIP_ALL")) (*action) = SERVER_IO_ACTION_SKIP_ALL;
  else if (stringEqualsIgnoreCase(actionText,"ABORT"   )) (*action) = SERVER_IO_ACTION_ABORT;
  else                                                    (*action) = SERVER_IO_ACTION_NONE;

  return TRUE;
}

#ifdef NDEBUG
  Errors ServerIO_connectNetwork(ServerIO    *serverIO,
                                 ConstString hostName,
                                 uint        hostPort,
                                 TLSModes    tlsMode,
                                 const void  *caData,
                                 uint        caLength,
                                 const void  *certData,
                                 uint        certLength,
                                 const void  *keyData,
                                 uint        keyLength
                                )
#else /* not NDEBUG */
  Errors __ServerIO_connectNetwork(const char *__fileName__,
                                   ulong      __lineNb__,
                                   ServerIO    *serverIO,
                                   ConstString hostName,
                                   uint        hostPort,
                                   TLSModes    tlsMode,
                                   const void  *caData,
                                   uint        caLength,
                                   const void  *certData,
                                   uint        certLength,
                                   const void  *keyData,
                                   uint        keyLength
                                  )
#endif /* NDEBUG */
{
  Errors          error;
  String          line;
  StringMap       argumentMap;
  String          id;
  String          encryptTypes;
  String          n,e;
  StringTokenizer stringTokenizer;
  ConstString     token;

  assert(serverIO != NULL);
  assert(hostName != NULL);
  assert(hostPort > 0);

  // init variables
  initIO(serverIO,SERVER_IO_TYPE_NETWORK);
  serverIO->inputBufferIndex  = 0;
  serverIO->inputBufferLength = 0;
  String_clear(serverIO->line);
  serverIO->lineFlag          = FALSE;
  List_clear(&serverIO->resultList);

  // connect to server
  error = Network_connect(&serverIO->network.socketHandle,
                          SOCKET_TYPE_PLAIN,
                          hostName,
                          hostPort,
                          NULL,  // loginName
                          NULL,  // password
                          NULL,  // caData
                          0,  // caLength
                          NULL,  // certData
                          0,  // certLength
                          NULL,  // publicKeyData
                          0,     // publicKeyLength
                          NULL,  // keyData
                          0,  // keyLength
                          SOCKET_FLAG_NON_BLOCKING|SOCKET_FLAG_NO_DELAY,
                          30*MS_PER_SECOND
                         );
  if (error != ERROR_NONE)
  {
    doneIO(serverIO);
    return error;
  }

  // read session data
  line         = String_new();
  argumentMap  = StringMap_new();
  id           = String_new();
  encryptTypes = String_new();
  n            = String_new();
  e            = String_new();
  error = Network_readLine(&serverIO->network.socketHandle,line,READ_TIMEOUT);
  if (error != ERROR_NONE)
  {
    String_delete(e);
    String_delete(n);
    String_delete(encryptTypes);
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
    Network_disconnect(&serverIO->network.socketHandle);
    doneIO(serverIO);
    return error;
  }
  if (!String_startsWithCString(line,"SESSION"))
  {
    String_delete(e);
    String_delete(n);
    String_delete(encryptTypes);
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
    Network_disconnect(&serverIO->network.socketHandle);
    doneIO(serverIO);
    return ERROR_INVALID_RESPONSE;
  }
  if (!StringMap_parse(argumentMap,line,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,7,NULL))
  {
    String_delete(e);
    String_delete(n);
    String_delete(encryptTypes);
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
    Network_disconnect(&serverIO->network.socketHandle);
    doneIO(serverIO);
    return ERROR_INVALID_RESPONSE;
  }

  // get id, encryptTypes, n, e
  if (!StringMap_getString(argumentMap,"id",id,NULL))
  {
    String_delete(e);
    String_delete(n);
    String_delete(encryptTypes);
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
    Network_disconnect(&serverIO->network.socketHandle);
    doneIO(serverIO);
    return ERRORX_(EXPECTED_PARAMETER,0,"id");
  }
  if (!StringMap_getString(argumentMap,"encryptTypes",encryptTypes,NULL))
  {
    String_delete(e);
    String_delete(n);
    String_delete(encryptTypes);
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
    Network_disconnect(&serverIO->network.socketHandle);
    doneIO(serverIO);
    return ERRORX_(EXPECTED_PARAMETER,0,"encryptTypes");
  }
  StringMap_getString(argumentMap,"n",n,NULL);
  StringMap_getString(argumentMap,"e",e,NULL);
//fprintf(stderr,"%s, %d: connector public n=%s\n",__FILE__,__LINE__,String_cString(n));
//fprintf(stderr,"%s, %d: connector public e=%s\n",__FILE__,__LINE__,String_cString(e));

  // decode session id
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
    String_delete(encryptTypes);
    String_delete(id);
    StringMap_delete(argumentMap);
    String_delete(line);
    Network_disconnect(&serverIO->network.socketHandle);
    doneIO(serverIO);
    return ERROR_INVALID_RESPONSE;
  }

  // get first usable encryption type
  String_initTokenizer(&stringTokenizer,encryptTypes,STRING_BEGIN,",",NULL,TRUE);
  while (String_getNextToken(&stringTokenizer,&token,NULL))
  {
    if (ServerIO_parseEncryptType(String_cString(token),&serverIO->encryptType,NULL))
    {
      break;
    }
  }
  String_doneTokenizer(&stringTokenizer);

  if (!String_isEmpty(n) && !String_isEmpty(e))
  {
    // set public key
    if (!Crypt_setPublicKeyModulusExponent(&serverIO->publicKey,n,e))
    {
      String_delete(e);
      String_delete(n);
      String_delete(encryptTypes);
      String_delete(id);
      StringMap_delete(argumentMap);
      String_delete(line);
      Network_disconnect(&serverIO->network.socketHandle);
      doneIO(serverIO);
      return ERROR_INVALID_KEY;
    }
  }

  switch (tlsMode)
  {
    case TLS_MODE_NONE:
      break;
    case TLS_MODE_TRY:
      error = syncExecuteCommand(serverIO,
                                 1,  // debug level
                                 30*MS_PER_SECOND,
                                 CALLBACK_(NULL,NULL),  // commandResultFunction
                                 "START_TLS"
                                );
      if (error == ERROR_NONE)
      {
        error = Network_startTLS(&serverIO->network.socketHandle,
                                 NETWORK_TLS_TYPE_CLIENT,
                                 caData,
                                 caLength,
                                 certData,
                                 certLength,
                                 keyData,
                                 keyLength,
                                 30*MS_PER_SECOND
                                );
        if (error != ERROR_NONE)
        {
          String_delete(e);
          String_delete(n);
          String_delete(encryptTypes);
          String_delete(id);
          StringMap_delete(argumentMap);
          String_delete(line);
          Network_disconnect(&serverIO->network.socketHandle);
          doneIO(serverIO);
          return error;
        }
      }
      break;
    case TLS_MODE_FORCE:
      error = syncExecuteCommand(serverIO,
                                 1,  // debug level
                                 30*MS_PER_SECOND,
                                 CALLBACK_(NULL,NULL),  // commandResultFunction
                                 "START_TLS"
                                );
      if (error != ERROR_NONE)
      {
        String_delete(e);
        String_delete(n);
        String_delete(encryptTypes);
        String_delete(id);
        StringMap_delete(argumentMap);
        String_delete(line);
        Network_disconnect(&serverIO->network.socketHandle);
        doneIO(serverIO);
        return error;
      }
      error = Network_startTLS(&serverIO->network.socketHandle,
                               NETWORK_TLS_TYPE_CLIENT,
                               caData,
                               caLength,
                               certData,
                               certLength,
                               keyData,
                               keyLength,
                               30*MS_PER_SECOND
                              );
      if (error != ERROR_NONE)
      {
        String_delete(e);
        String_delete(n);
        String_delete(encryptTypes);
        String_delete(id);
        StringMap_delete(argumentMap);
        String_delete(line);
        Network_disconnect(&serverIO->network.socketHandle);
        doneIO(serverIO);
        return error;
      }
      break;
  }

  // free resources
  String_delete(e);
  String_delete(n);
  String_delete(encryptTypes);
  String_delete(id);
  StringMap_delete(argumentMap);
  String_delete(line);

  // get remote info
  Network_getRemoteInfo(&serverIO->network.socketHandle,
                        serverIO->network.name,
                        &serverIO->network.port,
                        NULL  // socketAddress
                       );

  #ifndef NDEBUG
    if (globalOptions.debug.serverLevel >= 1)
    {
      fprintf(stderr,"DEBUG: connected to %s:%d\n",String_cString(hostName),hostPort);
    }
  #endif /* not DEBUG */

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(serverIO,ServerIO);
  #else /* NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,serverIO,ServerIO);
  #endif /* not NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors ServerIO_acceptNetwork(ServerIO                 *serverIO,
                                const ServerSocketHandle *serverSocketHandle
                               )
#else /* not NDEBUG */
  Errors __ServerIO_acceptNetwork(const char               *__fileName__,
                                  ulong                    __lineNb__,
                                  ServerIO                 *serverIO,
                                  const ServerSocketHandle *serverSocketHandle
                                 )
#endif /* NDEBUG */
{
  Errors error;

  String encodedId;
  String n,e;
  String s;

  assert(serverIO != NULL);
  assert(serverSocketHandle != NULL);

  // init variables
  initIO(serverIO,SERVER_IO_TYPE_NETWORK);
  serverIO->inputBufferIndex  = 0;
  serverIO->inputBufferLength = 0;
  String_clear(serverIO->line);
  serverIO->lineFlag          = FALSE;
  List_clear(&serverIO->resultList);

  // connect client
  error = Network_accept(&serverIO->network.socketHandle,
                         serverSocketHandle,
                         SOCKET_FLAG_NON_BLOCKING|SOCKET_FLAG_NO_DELAY,
                         30*MS_PER_SECOND
                        );
  if (error != ERROR_NONE)
  {
    doneIO(serverIO);
    return error;
  }

  // start session

  // get encoded session id
  encodedId = Misc_hexEncode(String_new(),serverIO->sessionId,sizeof(SessionId));

  // create new session keys
  n         = String_new();
  e         = String_new();
  s         = String_new();
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
    // format session data with RSA+none
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
    // format session data with none
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
    Network_disconnect(&serverIO->network.socketHandle);
    doneIO(serverIO);
    return error;
  }
  #ifndef NDEBUG
    if (globalOptions.debug.serverLevel >= 1)
    {
      fprintf(stderr,"DEBUG: send session data '%s'\n",String_cString(s));
    }
  #endif /* not DEBUG */

  // free resources
  String_delete(s);
  String_delete(e);
  String_delete(n);
  String_delete(encodedId);

  // get remote info
  Network_getRemoteInfo(&serverIO->network.socketHandle,
                        serverIO->network.name,
                        &serverIO->network.port,
                        NULL  // socketAddress
                       );

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(serverIO,ServerIO);
  #else /* NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,serverIO,ServerIO);
  #endif /* not NDEBUG */

  return ERROR_NONE;
}

Errors ServerIO_rejectNetwork(const ServerSocketHandle *serverSocketHandle)
{
  assert(serverSocketHandle != NULL);

  return Network_reject(serverSocketHandle);
}

#ifdef NDEBUG
  Errors ServerIO_connectBatch(ServerIO *serverIO,
                               int      inputDescriptor,
                               int      outputDescriptor
                              )
#else /* not NDEBUG */
  Errors __ServerIO_connectBatch(const char *__fileName__,
                                 ulong      __lineNb__,
                                 ServerIO   *serverIO,
                                 int        inputDescriptor,
                                 int        outputDescriptor
                                )
#endif /* NDEBUG */
{
  Errors error;

  assert(serverIO != NULL);

  // init variables
  initIO(serverIO,SERVER_IO_TYPE_BATCH);
  serverIO->inputBufferIndex  = 0;
  serverIO->inputBufferLength = 0;
  String_clear(serverIO->line);
  serverIO->lineFlag          = FALSE;
  List_clear(&serverIO->resultList);

  error = File_openDescriptor(&serverIO->file.inputHandle,inputDescriptor,FILE_OPEN_READ|FILE_STREAM);
  if (error != ERROR_NONE)
  {
    doneIO(serverIO);
    return error;
  }
  error = File_openDescriptor(&serverIO->file.outputHandle,outputDescriptor,FILE_OPEN_APPEND|FILE_STREAM);
  if (error != ERROR_NONE)
  {
    File_close(&serverIO->file.inputHandle);
    doneIO(serverIO);
    return error;
  }

  // set connected
  serverIO->file.isConnected = TRUE;

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(serverIO,ServerIO);
  #else /* NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,serverIO,ServerIO);
  #endif /* not NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  void ServerIO_disconnect(ServerIO *serverIO)
#else /* not NDEBUG */
  void __ServerIO_disconnect(const char *__fileName__,
                             ulong      __lineNb__,
                             ServerIO   *serverIO
                            )
#endif /* NDEBUG */
{
  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(serverIO,ServerIO);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,serverIO,ServerIO);
  #endif /* NDEBUG */

//TODO
#if 0
  // signal disconnect to wait result
  SEMAPHORE_LOCKED_DO(&serverIO->resultList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,10*MS_PER_SECOND)
  {
    Semaphore_signalModified(&serverIO->resultList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);
  }
#endif

  // done i/o
  switch (serverIO->type)
  {
    case SERVER_IO_TYPE_NONE:
      break;
    case SERVER_IO_TYPE_NETWORK:
      Network_disconnect(&serverIO->network.socketHandle);
      break;
    case SERVER_IO_TYPE_BATCH:
      if (ServerIO_isConnected(serverIO))
      {
        serverIO->file.isConnected = FALSE;
        File_close(&serverIO->file.outputHandle);
        File_close(&serverIO->file.inputHandle);
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }
  doneIO(serverIO);
}

void ServerIO_setEnd(ServerIO *serverIO)
{
  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);

  Semaphore_setEnd(&serverIO->lock);
}

Errors ServerIO_decryptData(const ServerIO       *serverIO,
                            void                 **data,
                            uint                 *dataLength,
                            ServerIOEncryptTypes encryptType,
                            ConstString          encryptedString
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
//TODO: dynamic dalloc
//    encryptedBufferLength =
    if (!Misc_base64Decode(encryptedBuffer,sizeof(encryptedBuffer),&encryptedBufferLength,encryptedString,7))
    {
      return ERROR_INVALID_ENCODING;
    }
//fprintf(stderr,"%s, %d: encryptedBufferLength=%d\n",__FILE__,__LINE__,encryptedBufferLength); debugDumpMemory(encryptedBuffer,encryptedBufferLength,0);
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

Errors ServerIO_encryptData(const ServerIO *serverIO,
                            const void     *data,
                            uint           dataLength,
                            String         encryptedString
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
  assert(encryptedString != NULL);
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
  encryptedBufferLength = 0;
  switch (serverIO->encryptType)
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
  String_setCString(encryptedString,"base64:");
  Misc_base64Encode(encryptedString,encryptedBuffer,encryptedBufferLength);
//fprintf(stderr,"%s, %d: encryptedBufferLength=%d base64=%s\n",__FILE__,__LINE__,encryptedBufferLength,String_cString(encryptedData));

  // free resources
  freeSecure(buffer);

  return ERROR_NONE;
}

Errors ServerIO_decryptString(const ServerIO       *serverIO,
                              String               string,
                              ConstString          encryptedString,
                              ServerIOEncryptTypes encryptType
                             )
{
  Errors error;
  void   *data;
  uint   dataLength;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(string != NULL);
  assert(encryptedString != NULL);

  // decrypt
  error = ServerIO_decryptData(serverIO,
                               &data,
                               &dataLength,
                               encryptType,
                               encryptedString
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
                               &data,
                               &dataLength,
                               encryptType,
                               encryptedPassword
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
                           ConstString          encryptedKey,
                           ServerIOEncryptTypes encryptType
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
  if (!Misc_base64Decode(encryptedBuffer,sizeof(encryptedBuffer),&encryptedBufferLength,encryptedKey,STRING_BEGIN))
  {
    return FALSE;
  }

  // decrypt key
  encodedBufferLength = 0;
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
          return error;
        }
      }
      else
      {
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
    return error;
  }
  freeSecure(keyData);

  return ERROR_NONE;
}

bool ServerIO_verifyPassword(const ServerIO       *serverIO,
                             ConstString          encryptedPassword,
                             ServerIOEncryptTypes encryptType,
                             const Hash           *passwordHash
                            )
{
  Errors     error;
  void       *data;
  uint       dataLength;
  const char *password;
  CryptHash  cryptHash;
  bool       okFlag;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(passwordHash != NULL);

  if (passwordHash->cryptHashAlgorithm != CRYPT_HASH_ALGORITHM_NONE)
  {
    // decrypt password
//fprintf(stderr,"%s, %d: encryptedPassword=%s\n",__FILE__,__LINE__,String_cString(encryptedPassword));
    error = ServerIO_decryptData(serverIO,
                                 &data,
                                 &dataLength,
                                 encryptType,
                                 encryptedPassword
                                );
    if (error != ERROR_NONE)
    {
      return error;
    }
    password = (const char*)data;
//fprintf(stderr,"%s, %d: n=%d s='",__FILE__,__LINE__,encodedBufferLength); for (i = 0; i < encodedBufferLength; i++) { fprintf(stderr,"%c",encodedBuffer[i]^clientInfo->sessionId[i]); } fprintf(stderr,"'\n");
//fprintf(stderr,"%s, %d: decrypted password '%s'\n",__FILE__,__LINE__,password);

    // calculate password hash
    Crypt_initHash(&cryptHash,passwordHash->cryptHashAlgorithm);
    Crypt_updateHash(&cryptHash,password,stringLength(password));
//fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__); Crypt_dumpHash(&cryptHash);

    // derive keys
//TODO

    // check passwords
    okFlag = Crypt_equalsHashBuffer(&cryptHash,passwordHash->data,passwordHash->length);

    // free resources
    Crypt_doneHash(&cryptHash);
    ServerIO_decryptDone(data,dataLength);
  }
  else
  {
    okFlag = TRUE;
  }

  return okFlag;
}

bool ServerIO_verifyHash(const ServerIO       *serverIO,
                         ConstString          encryptedHash,
                         ServerIOEncryptTypes encryptType,
                         const CryptHash      *requiredHash
                        )
{
  Errors    error;
  void      *data;
  uint      dataLength;
  CryptHash hash;
  bool      okFlag;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(encryptedHash != NULL);
  assert(requiredHash != NULL);

  // decrypt and get hash
  error = ServerIO_decryptData(serverIO,
                               &data,
                               &dataLength,
                               encryptType,
                               encryptedHash
                              );
  if (error != ERROR_NONE)
  {
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

bool ServerIO_receiveData(ServerIO *serverIO)
{
  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);

  return receiveData(serverIO,NO_WAIT);
}

bool ServerIO_getCommand(ServerIO  *serverIO,
                         uint      *id,
                         String    name,
                         StringMap argumentMap
                        )
{
  bool               commandFlag;
  uint               resultId;
  bool               completedFlag;
  uint               errorCode;
  String             data;
  ServerIOResultNode *resultNode;
  #ifndef NDEBUG
    size_t             iteratorVariable;
    Codepoint          codepoint;
  #endif /* not NDEBU G */

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
//fprintf(stderr,"%s, %d: serverIO->line=%s\n",__FILE__,__LINE__,String_cString(serverIO->line));
    // parse
    if      (String_parse(serverIO->line,STRING_BEGIN,"%u %y %u % S",NULL,&resultId,&completedFlag,&errorCode,data))
    {
      // command results: <id> <complete flag> <error code> <data>
      #ifndef NDEBUG
        if (globalOptions.debug.serverLevel >= 1)
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
      resultNode->error         = (errorCode != ERROR_CODE_NONE) ? ERRORF_(errorCode,"%s",String_cString(data)) : ERROR_NONE;
      resultNode->completedFlag = completedFlag;
      resultNode->data          = String_duplicate(data);
      resultNode->resultMap     = StringMap_new();
      if (errorCode == ERROR_CODE_NONE)
      {
        if (!StringMap_parse(resultNode->resultMap,data,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,STRING_BEGIN,NULL))
        {
          // parse error -> discard
          #ifndef NDEBUG
            if (globalOptions.debug.serverLevel >= 1)
            {
              fprintf(stderr,"DEBUG: parse result fail: %s\n",String_cString(data));
            }
          #endif /* not DEBUG */
          StringMap_delete(resultNode->resultMap);
          String_delete(resultNode->data);
          LIST_DELETE_NODE(resultNode);
          continue;
        }
      }

      // store result
      SEMAPHORE_LOCKED_DO(&serverIO->resultList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        List_append(&serverIO->resultList,resultNode);
//fprintf(stderr,"%s, %d: appended result: %d %d %d %s\n",__FILE__,__LINE__,resultNode->id,resultNode->error,resultNode->completedFlag,String_cString(resultNode->data));
      }
    }
    else if (String_parse(serverIO->line,STRING_BEGIN,"%u %S % S",NULL,id,name,data))
    {
      // command: <id> <name> <data>

      // parse arguments
      if (argumentMap != NULL)
      {
//fprintf(stderr,"%s, %d: parse %s\n",__FILE__,__LINE__,String_cString(commandNode->data));
        commandFlag = StringMap_parse(argumentMap,data,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,STRING_BEGIN,NULL);
        #ifndef NDEBUG
          if (!commandFlag)
          {
            if (globalOptions.debug.serverLevel >= 1)
            {
              fprintf(stderr,"DEBUG: skipped malformed data: %s\n",String_cString(serverIO->line));
            }
          }
        #endif /* not DEBUG */
      }
      else
      {
        commandFlag = TRUE;
      }
    }
    else
    {
      // unknown
      #ifndef NDEBUG
        if (globalOptions.debug.serverLevel >= 1)
        {
          fprintf(stderr,"DEBUG: skipped unknown data: %s\n",String_cString(serverIO->line));
          STRING_CHAR_ITERATE_UTF8(serverIO->line,iteratorVariable,codepoint)
          {
            if (iswprint(codepoint))
            {
              fprintf(stderr,"%s",charUTF8(codepoint));
            }
          }
          fprintf(stderr,"\n");
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
  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);

  return vsendCommand(serverIO,debugLevel,id,format,arguments);
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
  error = vsendCommand(serverIO,debugLevel,id,format,arguments);
  va_end(arguments);

  return error;
}

Errors ServerIO_vexecuteCommand(ServerIO                      *serverIO,
                                uint                          debugLevel,
                                long                          timeout,
                                ServerIOCommandResultFunction commandResultFunction,
                                void                          *commandResultUserData,
                                const char                    *format,
                                va_list                       arguments
                               )
{
  uint      id;
  Errors    error;
  bool      completedFlag;
  StringMap resultMap;

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

  // wait for results
  resultMap = StringMap_new();
  do
  {
    error = ServerIO_waitResult(serverIO,
                                timeout,
                                id,
                                &completedFlag,
                                resultMap
                               );
    if ((error == ERROR_NONE) && (commandResultFunction != NULL))
    {
      error = commandResultFunction(resultMap,commandResultUserData);
    }
  }
  while ((error == ERROR_NONE) && !completedFlag);
  StringMap_delete(resultMap);

  #ifndef NDEBUG
    if (error != ERROR_NONE)
    {
      if (globalOptions.debug.serverLevel >= 1)
      {
        fprintf(stderr,"DEBUG: execute command %u: '%s' fail: %s\n",id,format,Error_getText(error));
      }
    }
  #endif /* not DEBUG */

  return error;
}

Errors ServerIO_executeCommand(ServerIO                      *serverIO,
                               uint                          debugLevel,
                               long                          timeout,
                               ServerIOCommandResultFunction commandResultFunction,
                               void                          *commandResultUserData,
                               const char                    *format,
                               ...
                              )
{
  va_list arguments;
  Errors  error;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(format != NULL);

  va_start(arguments,format);
  error = ServerIO_vexecuteCommand(serverIO,
                                   debugLevel,
                                   timeout,
                                   commandResultFunction,
                                   commandResultUserData,
                                   format,
                                   arguments
                                  );
  va_end(arguments);

  return error;
}

Errors ServerIO_vsendResult(ServerIO   *serverIO,
                            uint       id,
                            bool       completedFlag,
                            Errors     error,
                            const char *format,
                            va_list    arguments
                           )
{
  String   s;
  #ifdef HAVE_NEWLOCALE
    locale_t oldLocale;
  #endif /* HAVE_NEWLOCALE */

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(format != NULL);

  // init variables
  s = String_new();

  // format result
  #ifdef HAVE_NEWLOCALE
    oldLocale = uselocale(POSIXLocale);
  #endif /* HAVE_NEWLOCALE */
  {
    String_format(s,"%u %d %u ",id,completedFlag ? 1 : 0,Error_getCode(error));
    String_appendVFormat(s,format,arguments);
  }
  #ifdef HAVE_NEWLOCALE
    uselocale(oldLocale);
  #endif /* HAVE_NEWLOCALE */

  // send result
  error = sendData(serverIO,s);
  if (error != ERROR_NONE)
  {
    String_delete(s);
    return error;
  }
  #ifndef NDEBUG
    if (globalOptions.debug.serverLevel >= 1)
    {
      fprintf(stderr,"DEBUG: send result '%s'\n",String_cString(s));
    }
  #endif /* not DEBUG */

  // free resources
  String_delete(s);

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
  va_list arguments;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(format != NULL);

  va_start(arguments,format);
  error = ServerIO_vsendResult(serverIO,id,completedFlag,error,format,arguments);
  va_end(arguments);

  return error;
}

Errors ServerIO_passResult(ServerIO        *serverIO,
                           uint            id,
                           bool            completedFlag,
                           Errors          error,
                           const StringMap resultMap
                          )
{
  String            s,t;
  #ifdef HAVE_NEWLOCALE
    locale_t        oldLocale;
  #endif /* HAVE_NEWLOCALE */
  StringMapIterator iteratorVariable;
  const char        *name;
  StringMapTypes    type;
  StringMapValue    value;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(resultMap != NULL);

  // init variables
  s = String_new();
  t = String_new();

  // format result
  #ifdef HAVE_NEWLOCALE
    oldLocale = uselocale(POSIXLocale);
  #endif /* HAVE_NEWLOCALE */
  {
    String_format(s,"%u %d %u ",id,completedFlag ? 1 : 0,Error_getCode(error));
    STRINGMAP_ITERATE(resultMap,iteratorVariable,name,type,value)
    {
      UNUSED_VARIABLE(type);

      String_appendFormat(s," %s=%'S",name,value.text);
    }
  }
  #ifdef HAVE_NEWLOCALE
    uselocale(oldLocale);
  #endif /* HAVE_NEWLOCALE */

  // send result
  error = sendData(serverIO,s);
  if (error != ERROR_NONE)
  {
    String_delete(t);
    String_delete(s);
    return error;
  }
  #ifndef NDEBUG
    if (globalOptions.debug.serverLevel >= 1)
    {
      fprintf(stderr,"DEBUG: send result '%s'\n",String_cString(s));
    }
  #endif /* not DEBUG */

  // free resources
  String_delete(t);
  String_delete(s);

  return ERROR_NONE;
}

Errors ServerIO_waitResults(ServerIO   *serverIO,
                            long       timeout,
                            const uint ids[],
                            uint       idCount,
                            uint       *index,
                            bool       *completedFlag,
                            StringMap  resultMap
                           )
{
  TimeoutInfo        timeoutInfo;
  uint               i;
  ServerIOResultNode *resultNode;
  Errors             error;

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);

  // wait for result, timeout, or quit
  resultNode     = NULL;
  Misc_initTimeout(&timeoutInfo,timeout);
  SEMAPHORE_LOCKED_DO(&serverIO->resultList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,timeout)
  {
    do
    {
      // find some matching result
      i          = 0;
      resultNode = NULL;
      while ((i < idCount) && (resultNode == NULL))
      {
        resultNode = LIST_FIND(&serverIO->resultList,resultNode,resultNode->id == ids[i]);
        if (resultNode != NULL)
        {
          if (index != NULL) (*index) = i;
          List_remove(&serverIO->resultList,resultNode);
        }
        else
        {
          i++;
        }
      }

      // if not found -> wait a short time
      if (resultNode == NULL)
      {
        Semaphore_waitModified(&serverIO->resultList.lock,Misc_getRestTimeout(&timeoutInfo,250));
      }
    }
    while (   ServerIO_isConnected(serverIO)
           && (resultNode == NULL)
           && !Misc_isTimeout(&timeoutInfo)
          );
  }
  Misc_doneTimeout(&timeoutInfo);
  if      (!ServerIO_isConnected(serverIO))
  {
    return ERROR_DISCONNECTED;
  }
  else if (resultNode == NULL)
  {
    return ERROR_NETWORK_TIMEOUT_RECEIVE;
  }

  // get and parse result
  error = resultNode->error;
  if (completedFlag != NULL) (*completedFlag) = resultNode->completedFlag;
  if (resultMap != NULL)
  {
    if (resultNode->error == ERROR_NONE)
    {
      if (!StringMap_parse(resultMap,resultNode->data,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,STRING_BEGIN,NULL))
      {
        deleteResultNode(resultNode);
        return ERROR_PARSE;
      }
    }
    else
    {
      StringMap_clear(resultMap);
    }
  }

  // free resources
  deleteResultNode(resultNode);

  return error;
}

Errors ServerIO_waitResult(ServerIO  *serverIO,
                           long      timeout,
                           uint      id,
                           bool      *completedFlag,
                           StringMap resultMap
                          )
{
  return ServerIO_waitResults(serverIO,timeout,&id,1,NULL,completedFlag,resultMap);
}

Errors ServerIO_clientAction(ServerIO   *serverIO,
                             long       timeout,
                             StringMap  resultMap,
                             const char *actionCommand,
                             const char *format,
                             ...
                            )
{
  uint          id;
  String        s;
  va_list       arguments;
  Errors        error;
  #ifdef HAVE_NEWLOCALE
    locale_t oldLocale;
  #endif /* HAVE_NEWLOCALE */

  assert(serverIO != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(serverIO);
  assert(actionCommand != NULL);

  // init variables
  s = String_new();

  // get new command id
  id = atomicIncrement(&serverIO->commandId,1);

  // format action command
  #ifdef HAVE_NEWLOCALE
    oldLocale = uselocale(POSIXLocale);
  #endif /* HAVE_NEWLOCALE */
  {
    String_format(s,"%u %s ",id,actionCommand);
    va_start(arguments,format);
    String_appendVFormat(s,format,arguments);
    va_end(arguments);
  }
  #ifdef HAVE_NEWLOCALE
    uselocale(oldLocale);
  #endif /* HAVE_NEWLOCALE */

  // send action command
  error = sendData(serverIO,s);
  if (error != ERROR_NONE)
  {
    return error;
  }
  #ifndef NDEBUG
    if (globalOptions.debug.serverLevel >= 1)
    {
      fprintf(stderr,"DEBUG: send action '%s'\n",String_cString(s));
    }
  #endif /* not DEBUG */

  // wait for result, timeout, or quit
  error = ServerIO_waitResult(serverIO,timeout,id,NULL /* completedFlag */,resultMap);

  // free resources
  String_delete(s);

  return error;
}

void ServerIO_clientActionResult(ServerIO   *serverIO,
                                 uint       id,
                                 Errors     error,
                                 StringMap  resultMap
                                )
{
  ServerIOResultNode *resultNode;

  SEMAPHORE_LOCKED_DO(&serverIO->resultList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    resultNode = LIST_FIND(&serverIO->resultList,resultNode,resultNode->id == id);
    if (resultNode != NULL)
    {
      resultNode->error         = error;
      resultNode->completedFlag = TRUE;
      StringMap_move(resultNode->resultMap,resultMap);
    }

    Semaphore_signalModified(&serverIO->resultList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);
  }
}

#ifdef __cplusplus
  }
#endif

/* end of file */
