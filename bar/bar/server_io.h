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

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define SESSION_ID_LENGTH 64      // max. length of session id

/***************************** Datatypes *******************************/

// session id
typedef byte SessionId[SESSION_ID_LENGTH];

#if 0
// server info
typedef struct
{
  SessionId    sessionId;
  CryptKey     publicKey,secretKey;

  uint         commandId;

  // connection
  String       name;
  uint         port;
  SocketHandle socketHandle;
} ServerInfo;
#endif

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
  Semaphore        lock;

  SessionId        sessionId;
  CryptKey         publicKey,privateKey;

  uint             commandId;

  String           line;

  enum
  {
    SERVER_IO_TYPE_BATCH,
    SERVER_IO_TYPE_NETWORK
  }                type;
  union
  {
    // i/o via file
    struct
    {
      FileHandle   fileHandle;
    } file;

    // i/o via network
    struct
    {
      String       name;
      uint         port;
      Semaphore    lock;
      SocketHandle socketHandle;
    } network;
  };

  ServerIOCommandList commandList;
  ServerIOResultList  resultList;
} ServerIO;

//TODO: remove
typedef struct
{
  Semaphore    lock;
  SocketHandle socketHandle;
} ServerConnectInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : ServerIO_initBatch
* Purpose: init server batch i/o
* Input  : serverIO   - server i/o
*          fileHandle - file handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ServerIO_initBatch(ServerIO   *serverIO,
                        FileHandle fileHandle
                       );

/***********************************************************************\
* Name   : ServerIO_initNetwork
* Purpose: init server network i/o
* Input  : serverIO     - server i/o
*          name         - remote name
*          port         - remote port
*          socketHandle - socket handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ServerIO_initNetwork(ServerIO     *serverIO,
                          ConstString  name,
                          uint         port,
                          SocketHandle socketHandle
                         );

/***********************************************************************\
* Name   : ServerIO_done
* Purpose: done server i/o
* Input  : serverIO - server i/o
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void ServerIO_done(ServerIO *serverIO);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : ServerIO_sendSessionId
* Purpose: send session id to client
* Input  : serverIO - server i/o
* Output : -
* Return : -
* Notes  : send data:
*          SESSION
*            id=<encoded id>
*            encryptTypes=<types>
*            n=<n>
*            e=<n>
\***********************************************************************/

void ServerIO_sendSessionId(ServerIO *serverIO);

/***********************************************************************\
* Name   : ServerIO_decryptPassword
* Purpose: decrypt password from hex-string with session data
* Input  : password          - password
*          serverIO          - server i/o
*          encryptType       - encrypt type
*          encryptedPassword - encrypted password
* Output : -
* Return : TRUE iff encrypted password equals password
* Notes  : -
\***********************************************************************/

bool ServerIO_decryptPassword(Password       *password,
                              const ServerIO *serverIO,
                              ConstString    encryptType,
                              ConstString    encryptedPassword
                             );

/***********************************************************************\
* Name   : ServerIO_checkPassword
* Purpose: check password with session data
* Input  : serverIO          - server i/o
*          encryptType       - encrypt type
*          encryptedPassword - encrypted password
*          password          - password
* Output : -
* Return : TRUE iff encrypted password equals password
* Notes  : -
\***********************************************************************/

bool ServerIO_checkPassword(const ServerIO *serverIO,
                            ConstString    encryptType,
                            ConstString    encryptedPassword,
                            const Password *password
                           );

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : ServerIO_vsendCommand
* Purpose:
* Input  : serverIO - server i/o
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors ServerIO_vsendCommand(ServerIO   *serverIO,
                             uint       *id,
                             const char *format,
                             va_list    arguments
                            );

/***********************************************************************\
* Name   : ServerIO_sendCommand
* Purpose:
* Input  : serverIO - server i/o
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors ServerIO_sendCommand(ServerIO   *serverIO,
                            uint       *id,
                            const char *format,
                            ...
                           );

/***********************************************************************\
* Name   : ServerIO_waitCommand
* Purpose: wait for command
* Input  : serverIO - server i/o
*          timeout  - timeout [ms] or WAIT_FOREVER
* Output : id        - command id
*          name      - command name
*          arguments - command arguments (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_waitCommand(ServerIO  *serverIO,
                            long      timeout,
                            uint      *id,
                            String    name,
                            StringMap argumentMap
                           );

/***********************************************************************\
* Name   : ServerIO_sendResult
* Purpose: send client result
* Input  : serverIO - server i/o
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors ServerIO_sendResult(ServerIO   *serverIO,
                           uint       id,
                           bool       completeFlag,
                           Errors     error,
                           const char *format,
                           ...
                          );

/***********************************************************************\
* Name   : ServerIO_waitResult
* Purpose: wait for result
* Input  : serverIO - server i/o
*          id       - result id to wait for
*          timeout  - timeout [ms] or WAIT_FOREVER
* Output : error         - error code (can be NULL)
*          completedFlag - TRUE iff completed (can be NULL)
*          resultMap     - result map (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors ServerIO_waitResult(ServerIO  *serverIO,
                           uint      id,
                           long      timeout,
                           Errors    *error,
                           bool      *completedFlag,
                           StringMap resultMap
                          );

/***********************************************************************\
* Name   : ServerIO_clientAction
* Purpose: execute client action
* Input  : serverIO - server i/o
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors ServerIO_clientAction(ServerIO   *serverIO,
                             uint       id,
                             StringMap  resultMap,
                             const char *actionCommand,
                             long       timeout,
                             const char *format,
                             ...
                            );

/***********************************************************************\
* Name   : ServerIO_sendMaster
* Purpose: send master
* Input  : serverIO - server i/o
* Output : -
* Return : -
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
