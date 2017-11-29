/***********************************************************************\
*
* $Revision: 7100 $
* $Date: 2017-01-19 15:55:44 +0100 (Thu, 19 Jan 2017) $
* $Author: torsten $
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

#include "global.h"
#include "lists.h"
#include "semaphores.h"
#include "strings.h"
#include "stringmaps.h"
#include "files.h"
#include "network.h"

#include "crypt.h"
#include "bar_global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define SESSION_ID_LENGTH 64      // max. length of session id

// server encrypt type
typedef enum
{
  SERVER_IO_ENCRYPT_TYPE_NONE,
  SERVER_IO_ENCRYPT_TYPE_RSA,
} ServerIOEncryptTypes;

/***************************** Datatypes *******************************/

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
  String data;
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

  // session
  SessionId            sessionId;
  ServerIOEncryptTypes encryptType;
  CryptKey             publicKey,privateKey;

  // poll handles
  struct pollfd        *pollfds;
  uint                 pollfdCount;
  uint                 maxPollfdCount;

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
  enum
  {
    SERVER_IO_TYPE_NONE,
    SERVER_IO_TYPE_BATCH,
    SERVER_IO_TYPE_NETWORK
  }                type;
  union
  {
    // i/o via file
    struct
    {
      FileHandle fileHandle;
    }                  file;

    // i/o via network
    struct
    {
      SocketHandle socketHandle;
      String       name;
      uint         port;
      Semaphore    lock;
    }                  network;
  };
  bool                 isConnected;

  // command
  uint                 commandId;

  // results list
  ServerIOResultList   resultList;
} ServerIO;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define ServerIO_init(...) __ServerIO_init(__FILE__,__LINE__, ## __VA_ARGS__)
  #define ServerIO_done(...) __ServerIO_done(__FILE__,__LINE__, ## __VA_ARGS__)
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

const char *ServerIO_encryptTypeToString(ServerIOEncryptTypes encryptType, const char *defaultValue);

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
* Name   : ServerIO_init
* Purpose: init server i/o
* Input  : serverIO - server i/o
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void ServerIO_init(ServerIO *serverIO);
#else /* not NDEBUG */
void __ServerIO_init(const char *__fileName__,
                     ulong      __lineNb__,
                     ServerIO   *serverIO
                    );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : ServerIO_done
* Purpose: done server i/o
* Input  : serverIO - server i/o
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void ServerIO_done(ServerIO *serverIO);
#else /* not NDEBUG */
void __ServerIO_done(const char *__fileName__,
                     ulong      __lineNb__,
                     ServerIO   *serverIO
                    );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : ServerIO_connectBatch
* Purpose: connect server batch i/o
* Input  : serverIO   - server i/o
*          fileHandle - file handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ServerIO_connectBatch(ServerIO   *serverIO,
                           FileHandle fileHandle
                          );

/***********************************************************************\
* Name   : ServerIO_connectNetwork
* Purpose: connect server network i/o
* Input  : serverIO     - server i/o
*          socketHandle - socket handle
*          hostName     - remote host name
*          hostPort     - remote host port
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ServerIO_connectNetwork(ServerIO     *serverIO,
                             SocketHandle *socketHandle,
                             ConstString  hostName,
                             uint         hostPort
                            );

/***********************************************************************\
* Name   : ServerIO_disconnect
* Purpose: disconnect server i/o
* Input  : serverIO - server i/o
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ServerIO_disconnect(ServerIO *serverIO);

/***********************************************************************\
* Name   : ServerIO_disconnect
* Purpose: check if connected
* Input  : serverIO - server i/o
* Output : -
* Return : TRUE iff connected
* Notes  : -
\***********************************************************************/

INLINE bool ServerIO_isConnected(ServerIO *serverIO);
#if defined(NDEBUG) || defined(__SERVER_IO_IMPLEMENTATION__)
INLINE bool ServerIO_isConnected(ServerIO *serverIO)
{
  assert(serverIO != NULL);

  return serverIO->isConnected;
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

Errors ServerIO_startSession(ServerIO *serverIO);

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

Errors ServerIO_acceptSession(ServerIO *serverIO);

/***********************************************************************\
* Name   : ServerIO_decryptData
* Purpose: decrypt data from hex/base64-string with session data
* Input  : serverIO      - server i/o
*          encryptType   - encrypt type
*          encryptedData - encrypted data (encoded string)
*          maxDataLength - max. data length
* Output : data       - decrypted data (secure memory)
*          dataLength - decrypted data length
* Return : TRUE iff decrypted
* Notes  : data is allocated and must be freed with
*          ServerIO_decryptDone()!
*          Supported string formats:
*            base64:<data>
*            hex:<data>
*            <data>
\***********************************************************************/

Errors ServerIO_decryptData(const ServerIO       *serverIO,
                            ServerIOEncryptTypes encryptType,
                            ConstString          encryptedData,
                            void                 **data,
                            uint                 *dataLength
                           );

//TODO
Errors ServerIO_encryptData(const ServerIO       *serverIO,
                            ServerIOEncryptTypes encryptType,
                            const void           *data,
                            uint                 dataLength,
                            String               encryptedData
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
* Name   : ServerIO_decryptPassword
* Purpose: decrypt password from hex-string with session data
* Input  : password          - password
*          serverIO          - server i/o
*          encryptType       - encrypt type
//TODO: use base64
*          encryptedPassword - encrypted password, hex-encoded
* Output : -
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
* Input  : password        - password
*          serverIO        - server i/o
*          encryptType     - encrypt type
*          encryptedString - encrypted string, base64-encoded
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_decryptString(const ServerIO       *serverIO,
                              String               string,
                              ServerIOEncryptTypes encryptType,
                              ConstString          encryptedString
                             );

/***********************************************************************\
* Name   : ServerIO_decryptKey
* Purpose: decrypt crypt key from base64-encoded string with session
*          data
* Input  : password     - password
*          serverIO     - server i/o
*          encryptType  - encrypt type
*          encryptedKey - encrypted key, base64-encoded
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_decryptKey(const ServerIO       *serverIO,
                           CryptKey             *cryptKey,
                           ServerIOEncryptTypes encryptType,
                           ConstString          encryptedKey
                          );

/***********************************************************************\
* Name   : ServerIO_verifyPassword
* Purpose: verify password with session data
* Input  : serverIO          - server i/o
*          encryptType       - encrypt type
*          encryptedPassword - encrypted password
*          passwordHash      - password hash data
* Output : -
* Return : TRUE iff encrypted password equals password
* Notes  : -
\***********************************************************************/

bool ServerIO_verifyPassword(const ServerIO       *serverIO,
                             ServerIOEncryptTypes encryptType,
                             ConstString          encryptedPassword,
                             const Hash           *passwordHash
                            );

/***********************************************************************\
* Name   : ServerIO_verifyHash
* Purpose: verify hash with session data
* Input  : serverIO          - server i/o
*          encryptType       - encrypt type
*          encryptedPassword - encrypted password
*          passwordHash      - password hash
* Output : -
* Return : TRUE iff hash of encrypted password equals hash
* Notes  : -
\***********************************************************************/

bool ServerIO_verifyHash(const ServerIO       *serverIO,
                         ServerIOEncryptTypes encryptType,
                         ConstString          encryptedData,
                         const CryptHash      *hash
                        );

// ----------------------------------------------------------------------

//TODO
void ServerIO_clearWait(ServerIO *serverIO);

void ServerIO_addWait(ServerIO *serverIO,
                           int      handle
                          );

void ServerIO_wait(ServerIO *serverIO);

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
* Name   : ServerIO_vsendCommand
* Purpose: send command
* Input  : serverIO  - server i/o
*          format    - command format string
*          arguments - arguments for command format string
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

/***********************************************************************\
* Name   : ServerIO_sendCommand
* Purpose: send command
* Input  : serverIO - server i/o
*          format   - command format string
*          ...      - optional arguments for command format string
* Output : id - command id
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_sendCommand(ServerIO   *serverIO,
                            uint       debugLevel,
                            uint       *id,
                            const char *format,
                            ...
                           );

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : ServerIO_vexecuteCommand
* Purpose: execute server command
* Input  : serverIO  - server i/o
*          timeout   - timeout [ms] or WAIT_FOREVER
*          format    - format string
*          arguments - optional arguments
* Output : resultMap - result map (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_vexecuteCommand(ServerIO   *serverIO,
                                uint       debugLevel,
                                long       timeout,
                                StringMap  resultMap,
                                const char *format,
                                va_list    arguments
                               );

/***********************************************************************\
* Name   : ServerIO_executeCommand
* Purpose: execute server command
* Input  : serverIO - server i/o
*          timeout  - timeout [ms] or WAIT_FOREVER
*          format   - format string
*          ...      - optional arguments
* Output : resultMap - result map (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_executeCommand(ServerIO   *serverIO,
                               uint       debugLevel,
                               long       timeout,
                               StringMap  resultMap,
                               const char *format,
                               ...
                              );

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : ServerIO_sendResult
* Purpose: send client result
* Input  : serverIO      - server i/o
*          id            - command id
*          completedFlag - TRUE iff completed
*          error         - error code
*          format        - command format string
*          ...           - optional arguments for command format string
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_sendResult(ServerIO   *serverIO,
                           uint       id,
                           bool       completedFlag,
                           Errors     error,
                           const char *format,
                           ...
                          );

/***********************************************************************\
* Name   : ServerIO_waitResult
* Purpose: wait for result
* Input  : serverIO - server i/o
*          timeout  - timeout [ms] or WAIT_FOREVER
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
* Input  : serverIO - server i/o
*          timeout  - timeout [ms] or WAIT_FOREVER
*          id       - command id
*          resultMap -
*          actionCommand -
*          format        -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_clientAction(ServerIO   *serverIO,
                             long       timeout,
                             uint       id,
                             StringMap  resultMap,
                             const char *actionCommand,
                             const char *format,
                             ...
                            );

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : ServerIO_sendMaster
* Purpose: send master
* Input  : serverIO - server i/o
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_sendMaster(const ServerIO     *serverIO,
                           ServerIOResultList *resultList,
                           const char         *format,
                           ...
                          );

#ifdef __cplusplus
  }
#endif

#endif /* __SERVER_IO__ */

/* end of file */
