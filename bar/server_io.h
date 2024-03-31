/***********************************************************************\
*
* Contents: Backup ARchiver server input/output
* Systems: all
*
\***********************************************************************/

#ifndef __SERVER_IO__
#define __SERVER_IO__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/files.h"
#include "common/global.h"
#include "common/lists.h"
#include "common/network.h"
#include "common/semaphores.h"
#include "common/stringmaps.h"
#include "common/strings.h"

#include "errors.h"
#include "configuration.h"
#include "crypt.h"
#include "bar_common.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define SESSION_ID_LENGTH 64      // max. length of session id

// server encrypt type
typedef enum
{
  SERVER_IO_ENCRYPT_TYPE_NONE,
  SERVER_IO_ENCRYPT_TYPE_RSA
} ServerIOEncryptTypes;

// server actions
typedef enum
{
  SERVER_IO_ACTION_NONE,
  SERVER_IO_ACTION_SKIP,
  SERVER_IO_ACTION_SKIP_ALL,
  SERVER_IO_ACTION_ABORT
} ServerIOActions;

/***************************** Datatypes *******************************/

// server i/o types
typedef enum
{
  SERVER_IO_TYPE_NONE,
  SERVER_IO_TYPE_NETWORK,
  SERVER_IO_TYPE_BATCH
} ServerIOTypes;

// session id
typedef byte SessionId[SESSION_ID_LENGTH];

// server command
typedef struct ServerIOCommandNode
{
  LIST_NODE_HEADER(struct ServerIOCommandNode);

  uint   id;
  String name;
  String data;
} ServerIOCommandNode;

typedef struct
{
  LIST_HEADER(ServerIOCommandNode);

  Semaphore lock;
} ServerIOCommandList;

// server result
typedef struct ServerIOResultNode
{
  LIST_NODE_HEADER(struct ServerIOResultNode);

  uint   id;
  bool   completedFlag;
  Errors error;
//TODO: union
  String data;
  StringMap resultMap;
} ServerIOResultNode;

typedef struct
{
  LIST_HEADER(ServerIOResultNode);

  Semaphore lock;
} ServerIOResultList;

// server i/o
typedef struct
{
  Semaphore            lock;

  // type
  ServerIOTypes        type;

  // session
  SessionId            sessionId;
  ServerIOEncryptTypes encryptType;
  CryptKey             publicKey,privateKey;

  // input/output buffer
  char                 *inputBuffer;
  uint                 inputBufferIndex;
  uint                 inputBufferLength;
  uint                 inputBufferSize;

  char                 *outputBuffer;
  uint                 outputBufferSize;

  // current input     line
  String               line;
  bool                 lineFlag;                    // TRUE iff line complete

  // connection
  union
  {
    // i/o via network
    struct
    {
      SocketHandle socketHandle;
      String       name;
      uint         port;
      Semaphore    lock;
    }                  network;

    // i/o via file
    struct
    {
      FileHandle inputHandle;
      FileHandle outputHandle;
      bool       isConnected;
    }                  file;
  };

  // globad command id counter
  uint                 commandId;

  // results list
  ServerIOResultList   resultList;
} ServerIO;

/***********************************************************************\
* Name   : ServerIOCommandResultFunction
* Purpose: command result function
* Input  : resultMap - resultMap
*          userData  - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*ServerIOCommandResultFunction)(const StringMap resultMap, void *userData);

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define ServerIO_connectNetwork(...) __ServerIO_connectNetwork(__FILE__,__LINE__, ## __VA_ARGS__)
  #define ServerIO_acceptNetwork(...)  __ServerIO_acceptNetwork (__FILE__,__LINE__, ## __VA_ARGS__)
  #define ServerIO_connectBatch(...)   __ServerIO_connectBatch  (__FILE__,__LINE__, ## __VA_ARGS__)
  #define ServerIO_disconnect(...)     __ServerIO_disconnect    (__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : ServerIO_encryptTypeToString
* Purpose: get name of server I/O crypt type
* Input  : encryptType  - crypt type
*          defaultValue - default value
* Output : -
* Return : crypt type name
* Notes  : -
\***********************************************************************/

const char *ServerIO_encryptTypeToString(ServerIOEncryptTypes encryptType,
                                         const char           *defaultValue
                                        );

/***********************************************************************\
* Name   : ServerIO_parseEncryptType
* Purpose: parse job state text
* Input  : encryptTypeText - encrypt type text
*          encryptType     - encrypt type variable
*          userData        - user data (not used)
* Output : encryptType - encrypt type
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool ServerIO_parseEncryptType(const char           *encryptTypeText,
                               ServerIOEncryptTypes *encryptType,
                               void                 *userData
                              );

/***********************************************************************\
* Name   : ServerIO_parseAction
* Purpose: parse action text
* Input  : actionText - action text
*          action     - action variable
*          userData   - user data (not used)
* Output : action - action
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool ServerIO_parseAction(const char      *actionText,
                          ServerIOActions *action,
                          void            *userData
                         );

/***********************************************************************\
* Name   : ServerIO_connectNetwork
* Purpose: connect server network i/o
* Input  : serverIO   - server i/o
*          hostName   - slave host name
*          hostPort   - slave host port
*          tlsMode    - TLS mode; see TLS_MODES_...
*          caData     - TLS CA data or NULL
*          caLength   - TLS CA data length
*          cert       - TLS cerificate or NULL
*          certLength - TLS cerificate data length
*          keyData    - key data or NULL
*          keyLength  - key data length
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

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
                                );
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
                                  );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : ServerIO_acceptNetwork
* Purpose: accept server network i/o
* Input  : serverIO           - server i/o
*          serverSocketHandle - server socket handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors ServerIO_acceptNetwork(ServerIO                 *serverIO,
                                const ServerSocketHandle *serverSocketHandle
                               );
#else /* not NDEBUG */
  Errors __ServerIO_acceptNetwork(const char *__fileName__,
                                  ulong      __lineNb__,
                                  ServerIO                 *serverIO,
                                  const ServerSocketHandle *serverSocketHandle
                                 );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : ServerIO_rejectNetwork
* Purpose: reject server network i/o
* Input  : serverSocketHandle - server socket handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_rejectNetwork(const ServerSocketHandle *serverSocketHandle);

/***********************************************************************\
* Name   : ServerIO_connectBatch
* Purpose: connect server batch i/o
* Input  : serverIO - server i/o
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors ServerIO_connectBatch(ServerIO *serverIO,
                               int      inputDescriptor,
                               int      outputDescriptor
                              );
#else /* not NDEBUG */
  Errors __ServerIO_connectBatch(const char *__fileName__,
                                 ulong      __lineNb__,
                                 ServerIO   *serverIO,
                                 int        inputDescriptor,
                                 int        outputDescriptor
                                );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : ServerIO_disconnect
* Purpose: disconnect server i/o
* Input  : serverIO - server i/o
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void ServerIO_disconnect(ServerIO *serverIO);
#else /* not NDEBUG */
  void __ServerIO_disconnect(const char *__fileName__,
                             ulong      __lineNb__,
                             ServerIO   *serverIO
                            );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : ServerIO_setEnd
* Purpose: end server network i/o
* Input  : serverIO - server i/o
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ServerIO_setEnd(ServerIO *serverIO);

/***********************************************************************\
* Name   : ServerIO_isConnected
* Purpose: check if connected
* Input  : serverIO - server i/o
* Output : -
* Return : TRUE iff connected
* Notes  : -
\***********************************************************************/

INLINE bool ServerIO_isConnected(const ServerIO *serverIO);
#if defined(NDEBUG) || defined(__SERVER_IO_IMPLEMENTATION__)
INLINE bool ServerIO_isConnected(const ServerIO *serverIO)
{
  bool isConnected;

  assert(serverIO != NULL);

  isConnected = FALSE;
  switch (serverIO->type)
  {
    case SERVER_IO_TYPE_NONE:
      break;
    case SERVER_IO_TYPE_NETWORK:
      isConnected = Network_isConnected(&serverIO->network.socketHandle);
      break;
    case SERVER_IO_TYPE_BATCH:
      isConnected = serverIO->file.isConnected;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }

  return isConnected;
}
#endif /* NDEBUG || __SERVER_IO_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : ServerIO_isTLS
* Purpose: check if TLS connection
* Input  : serverIO - server i/o
* Output : -
* Return : TRUE iff TLS connection
* Notes  : -
\***********************************************************************/

INLINE bool ServerIO_isTLS(const ServerIO *serverIO);
#if defined(NDEBUG) || defined(__SERVER_IO_IMPLEMENTATION__)
INLINE bool ServerIO_isTLS(const ServerIO *serverIO)
{
  bool hasTLS;

  assert(serverIO != NULL);

  hasTLS = FALSE;
  switch (serverIO->type)
  {
    case SERVER_IO_TYPE_NONE:
      break;
    case SERVER_IO_TYPE_NETWORK:
      hasTLS = Network_isTLS(&serverIO->network.socketHandle);
      break;
    case SERVER_IO_TYPE_BATCH:
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }

  return hasTLS;
}
#endif /* NDEBUG || __SERVER_IO_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : ServerIO_isInsecureTLS
* Purpose: check if insecure TLS connection
* Input  : serverIO - server i/o
* Output : -
* Return : TRUE iff insecure TLS connection
* Notes  : -
\***********************************************************************/

INLINE bool ServerIO_isInsecureTLS(const ServerIO *serverIO);
#if defined(NDEBUG) || defined(__SERVER_IO_IMPLEMENTATION__)
INLINE bool ServerIO_isInsecureTLS(const ServerIO *serverIO)
{
  bool hasTLS;

  assert(serverIO != NULL);

  hasTLS = FALSE;
  switch (serverIO->type)
  {
    case SERVER_IO_TYPE_NONE:
      break;
    case SERVER_IO_TYPE_NETWORK:
      hasTLS = Network_isInsecureTLS(&serverIO->network.socketHandle);
      break;
    case SERVER_IO_TYPE_BATCH:
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }

  return hasTLS;
}
#endif /* NDEBUG || __SERVER_IO_IMPLEMENTATION__ */


// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : ServerIO_startSession
* Purpose: start a new session
* Input  : serverIO - server i/o
* Output : -
* Return : ERROR_NONE or error code
* Notes  : send data:
*          SESSION
*            id=<encoded id>
*            encryptTypes=<types>
*            n=<n>
*            e=<n>
\***********************************************************************/

Errors xxxServerIO_startSession(ServerIO *serverIO);

/***********************************************************************\
* Name   : ServerIO_acceptSession
* Purpose: accept a session
* Input  : serverIO - server i/o
* Output : -
* Return : ERROR_NONE or error code
* Notes  : received data:
*          SESSION
*            id=<encoded id>
*            encryptTypes=<types>
*            n=<n>
*            e=<n>
\***********************************************************************/

Errors xxxServerIO_acceptSession(ServerIO *serverIO);

/***********************************************************************\
* Name   : ServerIO_decryptData
* Purpose: decrypt data from hex/base64-string with session data
* Input  : serverIO        - server i/o
*          encryptType     - encrypt type; see
*                            SERVER_IO_ENCRYPT_TYPE_...
*          encryptedString - encrypted data (encoded string)
* Output : data       - decrypted data (secure memory)
*          dataLength - decrypted data length
* Return : ERROR_NONE or error code
* Notes  : data is allocated and must be freed with
*          ServerIO_decryptDone(data,dataLength)!
*          Supported string formats:
*            base64:<data>
*            hex:<data>
*            <data>
\***********************************************************************/

Errors ServerIO_decryptData(const ServerIO       *serverIO,
                            void                 **data,
                            uint                 *dataLength,
                            ServerIOEncryptTypes encryptType,
                            ConstString          encryptedString
                           );

/***********************************************************************\
* Name   : ServerIO_decryptDone
* Purpose: free decrypt data
* Input  : data       - decrypted data (secure memory)
*          dataLength - decrypted data length
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ServerIO_decryptDone(void *data, uint dataLength);

/***********************************************************************\
* Name   : ServerIO_encryptData
* Purpose: encrypt data to hex/base64-string with session data
* Input  : serverIO   - server i/o
*          data       - data
*          dataLength - data length
* Output : encryptedString - encrypted data (encoded string)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_encryptData(const ServerIO *serverIO,
                            const void     *data,
                            uint           dataLength,
                            String         encryptedString
                           );

/***********************************************************************\
* Name   : ServerIO_decryptPassword
* Purpose: decrypt password from hex-string with session data
* Input  : serverIO          - server i/o
//TODO: use base64
*          encryptType     - encrypt type; see
*                            SERVER_IO_ENCRYPT_TYPE_...
*          encryptedPassword - encrypted password, hex-encoded
* Output : password - decrypted password
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_decryptPassword(const ServerIO       *serverIO,
                                Password             *password,
                                ServerIOEncryptTypes encryptType,
                                ConstString          encryptedPassword
                               );

/***********************************************************************\
* Name   : ServerIO_decryptString
* Purpose: decrypt string from base64-encoded string with session data
* Input  : serverIO        - server i/o
*          encryptedString - encrypted data (encoded string)
*          encryptType     - encrypt type; see
*                            SERVER_IO_ENCRYPT_TYPE_...
* Output : string - decrypted string
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_decryptString(const ServerIO       *serverIO,
                              String               string,
                              ConstString          encryptedString,
                              ServerIOEncryptTypes encryptType
                             );

/***********************************************************************\
* Name   : ServerIO_decryptKey
* Purpose: decrypt crypt key from base64-encoded string with session
*          data
* Input  : serverIO     - server i/o
*          encryptedKey - encrypted key, base64-encoded
*          encryptType  - encrypt type; see SERVER_IO_ENCRYPT_TYPE_...
* Output : cryptKey - decrypted key
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_decryptKey(const ServerIO       *serverIO,
                           CryptKey             *cryptKey,
                           ConstString          encryptedKey,
                           ServerIOEncryptTypes encryptType
                          );

/***********************************************************************\
* Name   : ServerIO_verifyPassword
* Purpose: verify password with session data
* Input  : serverIO             - server i/o
*          encryptedPassword    - encrypted password
*          encryptType          - encrypt type; see
*                                 SERVER_IO_ENCRYPT_TYPE_...
*          requiredPasswordHash - required password hash
* Output : -
* Return : TRUE iff encrypted password equals password
* Notes  : -
\***********************************************************************/

bool ServerIO_verifyPassword(const ServerIO       *serverIO,
                             ConstString          encryptedPassword,
                             ServerIOEncryptTypes encryptType,
                             const Hash           *requiredPasswordHash
                            );

/***********************************************************************\
* Name   : ServerIO_verifyHash
* Purpose: verify hash with session data
* Input  : serverIO      - server i/o
*          encryptedHash - encrypted hash
*          encryptType   - encrypt type; see SERVER_IO_ENCRYPT_TYPE_...
*          requiredHash  - required hash
* Output : -
* Return : TRUE iff hash of encrypted password equals hash
* Notes  : -
\***********************************************************************/

bool ServerIO_verifyHash(const ServerIO       *serverIO,
                         ConstString          encryptedHash,
                         ServerIOEncryptTypes encryptType,
                         const CryptHash      *requiredHash
                        );

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : ServerIO_receiveData
* Purpose: receive data
* Input  : serverIO - server i/o
* Output : -
* Return : TRUE if data received, FALSE on disconnect
* Notes  : -
\***********************************************************************/

bool ServerIO_receiveData(ServerIO *serverIO);

/***********************************************************************\
* Name   : ServerIO_getCommand
* Purpose: get command
* Input  : serverIO - server i/o
* Output : id        - command id
*          name      - command name
*          arguments - command arguments (can be NULL)
* Return : TRUE iff command received
* Notes  : -
\***********************************************************************/

bool ServerIO_getCommand(ServerIO  *serverIO,
                         uint      *id,
                         String    name,
                         StringMap argumentMap
                        );

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : ServerIO_vsendCommand, ServerIO_sendCommand
* Purpose: send command
* Input  : serverIO   - server i/o
*          debugLevel - debug level
*          format     - command format string
*          ...        - optional arguments for command format string
* Output : id - command id
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_vsendCommand(ServerIO   *serverIO,
                             uint       debugLevel,
                             uint       *id,
                             const char *format,
                             va_list    arguments
                            );
Errors ServerIO_sendCommand(ServerIO   *serverIO,
                            uint       debugLevel,
                            uint       *id,
                            const char *format,
                            ...
                           );

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : ServerIO_vexecuteCommand, ServerIO_executeCommand
* Purpose: execute server command
* Input  : serverIO              - server i/o
*          debugLevel            - debug level
*          timeout               - timeout [ms] or WAIT_FOREVER/NO_WAIT
*          commandResultFunction - command result function (can be NULL)
*          commandResultUserData - user data for command result function
*          format                - format string
*          .../arguments         - optional arguments
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_vexecuteCommand(ServerIO                      *serverIO,
                                uint                          debugLevel,
                                long                          timeout,
                                ServerIOCommandResultFunction commandResultFunction,
                                void                          *commandResultUserData,
                                const char                    *format,
                                va_list                       arguments
                               );
Errors ServerIO_executeCommand(ServerIO                      *serverIO,
                               uint                          debugLevel,
                               long                          timeout,
                               ServerIOCommandResultFunction commandResultFunction,
                               void                          *commandResultUserData,
                               const char *format,
                               ...
                              );

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : ServerIO_vsendResult, ServerIO_sendResult
* Purpose: send client result
* Input  : serverIO      - server i/o
*          id            - command id
*          completedFlag - TRUE iff completed
*          error         - error code
*          format        - command format string
*          .../arguments - optional arguments for command format string
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_vsendResult(ServerIO   *serverIO,
                            uint       id,
                            bool       completedFlag,
                            Errors     error,
                            const char *format,
                            va_list    arguments
                          );
Errors ServerIO_sendResult(ServerIO   *serverIO,
                           uint       id,
                           bool       completedFlag,
                           Errors     error,
                           const char *format,
                           ...
                          );

/***********************************************************************\
* Name   : ServerIO_passResult
* Purpose: pass client result
* Input  : serverIO      - server i/o
*          id            - command id
*          completedFlag - TRUE iff completed
*          error         - error code
*          resultMap     - results to pass
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_passResult(ServerIO        *serverIO,
                           uint            id,
                           bool            completedFlag,
                           Errors          error,
                           const StringMap resultMap
                          );

/***********************************************************************\
* Name   : ServerIO_waitResults
* Purpose: wait for results
* Input  : serverIO - server i/o
*          timeout  - timeout [ms] or WAIT_FOREVER/NO_WAIT
*          ids      - command ids to wait for
*          idCount  - number of command ids
* Output : index         - id index [0..idCount-1] (can be NULL)
*          completedFlag - TRUE iff completed (can be NULL)
*          resultMap     - result map (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_waitResults(ServerIO   *serverIO,
                            long       timeout,
                            const uint ids[],
                            uint       idCount,
                            uint       *index,
                            bool       *completedFlag,
                            StringMap  resultMap
                           );

/***********************************************************************\
* Name   : ServerIO_waitResult
* Purpose: wait for result
* Input  : serverIO - server i/o
*          timeout  - timeout [ms] or WAIT_FOREVER/NO_WAIT
*          id       - command id to wait for
* Output : completedFlag - TRUE iff completed (can be NULL)
*          resultMap     - result map (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_waitResult(ServerIO  *serverIO,
                           long      timeout,
                           uint      id,
                           bool      *completedFlag,
                           StringMap resultMap
                          );

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : ServerIO_clientAction
* Purpose: execute client action
* Input  : serverIO      - server i/o
*          timeout       - timeout [ms] or WAIT_FOREVER/NO_WAIT
*          resultMap     - result
*          actionCommand - action command
*          format        - optional arguments for action command
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_clientAction(ServerIO   *serverIO,
                             long       timeout,
                             StringMap  resultMap,
                             const char *actionCommand,
                             const char *format,
                             ...
                            );

void ServerIO_clientActionResult(ServerIO   *serverIO,
                                 uint       id,
                                 Errors     error,
                                 StringMap  resultMap
                                );

#ifdef __cplusplus
  }
#endif

#endif /* __SERVER_IO__ */

/* end of file */
