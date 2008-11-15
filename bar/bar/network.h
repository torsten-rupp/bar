/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/network.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Network functions
* Systems: all
*
\***********************************************************************/

#ifndef __NETWORK__
#define __NETWORK__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_FTP
  #include <ftplib.h>
#endif /* HAVE_FTP */
#ifdef HAVE_SSH2
  #include <libssh2.h>
#endif /* HAVE_SSH2 */
#ifdef HAVE_GNU_TLS
  #include <gnutls/gnutls.h>
#endif /* HAVE_GNU_TLS */
#include <assert.h>

#include "global.h"
#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define SOCKET_FLAG_NON_BLOCKING (1 << 0)

/***************************** Datatypes *******************************/
typedef enum
{
  SOCKET_TYPE_PLAIN,
  SOCKET_TYPE_SSH,
  SOCKET_TYPE_TLS,
} SocketTypes;

typedef struct
{
  SocketTypes type;
  int         handle;
  union
  {
    #ifdef HAVE_FTP
      struct
      {
        netbuf *control;
        netbuf *data;
      } ftp;
    #endif /* HAVE_SSH2 */
    #ifdef HAVE_SSH2
      struct
      {
        LIBSSH2_SESSION *session;
      } ssh2;
    #endif /* HAVE_SSH2 */
    #ifdef HAVE_GNU_TLS
      struct
      {
        gnutls_session_t session;
      } gnuTLS;
    #endif /* HAVE_GNU_TLS */
  };
/* optimize ???
  byte  *buffer;
  ulong index;
  ulong length;
*/
} SocketHandle;

typedef enum
{
  SERVER_TYPE_PLAIN,
  SERVER_TYPE_TLS,
} ServerTypes;

typedef struct
{
  ServerTypes type;
  int         handle; 
  #ifdef HAVE_GNU_TLS
    bool                             initTLSFlag;
    gnutls_certificate_credentials_t gnuTLSCredentials;
    gnutls_dh_params_t               gnuTLSDHParams;
  #endif /* HAVE_GNU_TLS */
} ServerSocketHandle;

typedef enum
{
  NETWORK_EXECUTE_IO_TYPE_STDOUT,
  NETWORK_EXECUTE_IO_TYPE_STDERR,
} NetworkExecuteIOTypes;

#define NETWORK_EXECUTE_IO_MASK_STDOUT (1 << NETWORK_EXECUTE_IO_TYPE_STDOUT)
#define NETWORK_EXECUTE_IO_MASK_STDERR (1 << NETWORK_EXECUTE_IO_TYPE_STDERR)

typedef struct
{
  SocketHandle *socketHandle;
  #ifdef HAVE_SSH2
     LIBSSH2_CHANNEL *channel;
  #endif /* HAVE_SSH2 */
// optimize ???
  struct
  {
    char data[256];
    uint index;
    uint length;
  } stdoutBuffer;
  struct
  {
    char data[256];
    uint index;
    uint length;
  } stderrBuffer;
} NetworkExecuteHandle;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Network_initAll
* Purpose: init network functions
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_initAll(void);

/***********************************************************************\
* Name   : Network_doneAll
* Purpose: deinitialize network functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Network_doneAll(void);

/***********************************************************************\
* Name   : Network_connect
* Purpose: connect to host
* Input  : socketType            - socket type; see SOCKET_TYPE_*
*          hostName              - host name
*          hostPort              - host port
*          flags                 - socket falgs
*          loginName             - login user name
*          password              - SSH password
*          sshPublicKeyFileName  - SSH public key file for login
*          sshPrivateKeyFileName - SSH private key file for login
* Output : socketHandle - socket handle
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_connect(SocketHandle *socketHandle,
                       SocketTypes  socketType,
                       const String hostName,
                       uint         hostPort,
                       const String loginName,
                       Password     *password,
                       const String sshPublicKeyFileName,
                       const String sshPrivateKeyFileName,
                       uint         flags
                      );

/***********************************************************************\
* Name   : Network_disconnect
* Purpose: disconnect from host
* Input  : socketHandle - socket handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Network_disconnect(SocketHandle *socketHandle);

/***********************************************************************\
* Name   : Network_getSocket
* Purpose: get socket from socket handle
* Input  : socketHandle - socket handle
* Output : -
* Return : socket
* Notes  : -
\***********************************************************************/

int Network_getSocket(SocketHandle *socketHandle);

/***********************************************************************\
* Name   : Network_getSSHSession
* Purpose: get SSH session from socket handle
* Input  : socketHandle - socket handle
* Output : -
* Return : ssh session
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SSH2
LIBSSH2_SESSION *Network_getSSHSession(SocketHandle *socketHandle);
#endif /* HAVE_SSH2 */

/***********************************************************************\
* Name   : Network_eof
* Purpose: check of end-of-file reached
* Input  : socketHandle - socket handle
* Output : -
* Return : TRUE iff end-of-file reached, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Network_eof(SocketHandle *socketHandle);

/***********************************************************************\
* Name   : Network_getAvaibleBytes
* Purpose: get number of available bytes
* Input  : socketHandle - socket handle
* Output : -
* Return : number of available bytes which can be received without
*          blocking
* Notes  : -
\***********************************************************************/

ulong Network_getAvaibleBytes(SocketHandle *socketHandle);

/***********************************************************************\
* Name   : Network_receive
* Purpose: receive data from host
* Input  : socketHandle - socket handle
*          buffer       - data buffer
*          timeout      - timeout [ms] or WAIT_FOREVER
*          maxLength    - max. length of data (in bytes)
* Output : bytesReceived - number of bytes received
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_receive(SocketHandle *socketHandle,
                       void         *buffer,
                       ulong        maxLength,
                       long         timeout,
                       ulong        *bytesReceived
                      );

/***********************************************************************\
* Name   : Network_send
* Purpose: send data to host
* Input  : socketHandle - socket handle
*          buffer       - data buffer
*          length       - length of data (in bytes)
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_send(SocketHandle *socketHandle,
                    const void   *buffer,
                    ulong        length
                   );

/***********************************************************************\
* Name   : Network_readLine
* Purpose: read line from host (end of line: \n or \r\n)
* Input  : socketHandle - socket handle
*          line         - string variable
*          timeout      - timeout [ms] or WAIT_FOREVER
* Output : line - read line
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Network_readLine(SocketHandle *socketHandle,
                        String       line,
                        long         timeout
                       );

/***********************************************************************\
* Name   : Network_writeLine
* Purpose: write line to host
* Input  : socketHandle - socket handle
*          line         - line to write
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Network_writeLine(SocketHandle *socketHandle,
                         const String line
                        );

/***********************************************************************\
* Name   : Network_initServer
* Purpose: initialize a server socket
* Input  : serverPort  - server port
*          ServerTypes - server type; see SERVER_TYPE_*
*          caFileName   - file with TLS CA or NULL
*          certFileName - file with TLS cerificate or NULL
*          keyFileName  - file with TLS key or NULL
* Output : serverSocketHandle - server socket handle
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_initServer(ServerSocketHandle *serverSocketHandle,
                          ServerTypes        serverType,
                          uint               serverPort,
                          const char         *caFileName,
                          const char         *certFileName,
                          const char         *keyFileName
                         );

/***********************************************************************\
* Name   : Network_doneServer
* Purpose: deinitialize server
* Input  : serverSocketHandle - socket handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Network_doneServer(ServerSocketHandle *serverSocketHandle);

/***********************************************************************\
* Name   : Network_getServerSocket
* Purpose: get socket from socket handle
* Input  : serverSocketHandle - server socket handle
* Output : -
* Return : socket
* Notes  : -
\***********************************************************************/

int Network_getServerSocket(ServerSocketHandle *serverSocketHandle);

/***********************************************************************\
* Name   : Network_accept
* Purpose: accept client connection
* Input  : serverSocketHandle - server socket handle
*          flags              - socket falgs
* Output : socketHandle - server socket handle
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_accept(SocketHandle             *socketHandle,
                      const ServerSocketHandle *serverSocketHandle,
                      uint                     flags
                     );

/***********************************************************************\
* Name   : Network_getLocalInfo
* Purpose: get local socket info
* Input  : socketHandle - socket handle
*          name         - name variable
* Output : name - local name
*          port - local port
* Return : -
* Notes  : -
\***********************************************************************/

void Network_getLocalInfo(SocketHandle *socketHandle,
                          String name,
                          uint   *port
                         );

/***********************************************************************\
* Name   : Network_getRemoteInfo
* Purpose: get remove socket info
* Input  : socketHandle - socket handle
*          name         - name variable
* Output : name - remote name
*          port - remote port
* Return : -
* Notes  : -
\***********************************************************************/

void Network_getRemoteInfo(SocketHandle *socketHandle,
                           String name,
                           uint   *port
                          );

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : Network_execute
* Purpose: execute remote command
* Input  : networkExecuteHandle - network execute handle variable
*          socketHandle         - SSH socket handle
*          ioMaks               - i/o mask
*          command              - command to execute
* Output : networkExecuteHandle - network execute handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Network_execute(NetworkExecuteHandle *networkExecuteHandle,
                       SocketHandle         *socketHandle,
                       ulong                ioMask,
                       const char           *command
                      );

/***********************************************************************\
* Name   : Network_terminate
* Purpose: terminate execution of remote command
* Input  : networkExecuteHandle - network execute handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

int Network_terminate(NetworkExecuteHandle *networkExecuteHandle);

/***********************************************************************\
* Name   : Network_executeEOF
* Purpose: check of end-of-data from remote execution
* Input  : networkExecuteHandle - network execute handle
*          ioType               - i/o type; see
*                                 NETWORK_EXECUTE_IO_TYPES_*
*          timeout              - timeout or WAIT_FOREVER [ms]
* Output : -
* Return : TRUE iff end-of-data, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Network_executeEOF(NetworkExecuteHandle  *networkExecuteHandle,
                        NetworkExecuteIOTypes ioType,
                        long                  timeout
                       );

/***********************************************************************\
* Name   : Network_executeSendEOF
* Purpose: send end-of-data to remote execution
* Input  : networkExecuteHandle - network execute handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Network_executeSendEOF(NetworkExecuteHandle *networkExecuteHandle);

/***********************************************************************\
* Name   : Network_executeWrite
* Purpose: write data to remote stdin
* Input  : networkExecuteHandle - network execute handle
* Output : buffer               - data buffer
*          length               - length of data
* Return : ERROR_NONE if data written, error code otherwise
* Notes  : -
\***********************************************************************/

Errors Network_executeWrite(NetworkExecuteHandle *networkExecuteHandle,
                            const void           *buffer,
                            ulong                length
                           );

/***********************************************************************\
* Name   : Network_executeRead
* Purpose: read remote stdout/stderr
* Input  : networkExecuteHandle - network execute handle
*          ioType               - i/o type; see
*                                 NETWORK_EXECUTE_IO_TYPES_*
*          buffer               - data buffer
*          maxLength            - max. size of buffer
*          timeout              - timeout or WAIT_FOREVER [ms]
* Output : bytesRead - number of bytes read
* Return : ERROR_NONE if data written, error code otherwise
* Notes  : -
\***********************************************************************/

Errors Network_executeRead(NetworkExecuteHandle  *networkExecuteHandle,
                           NetworkExecuteIOTypes ioType,
                           void                  *buffer,
                           ulong                 maxLength,
                           long                  timeout,
                           ulong                 *bytesRead
                          );

/***********************************************************************\
* Name   : Network_executeWriteLine
* Purpose: write line to remote stdin
* Input  : networkExecuteHandle - network execute handle
*          line                 - string
* Output : -
* Return : ERROR_NONE if data written, error code otherwise
* Notes  : '\n' is appended
\***********************************************************************/

Errors Network_executeWriteLine(NetworkExecuteHandle *networkExecuteHandle,
                                const String         line
                               );

/***********************************************************************\
* Name   : Network_executeReadLine
* Purpose: read line from remote stdout/stderr
* Input  : networkExecuteHandle - network execute handle
*          ioType               - i/o type; see
*                                 NETWORK_EXECUTE_IO_TYPES_*
*          line                 - string variable
*          timeout              - timeout or WAIT_FOREVER [ms]
* Output : -
* Return : ERROR_NONE if data written, error code otherwise
* Notes  : -
\***********************************************************************/

Errors Network_executeReadLine(NetworkExecuteHandle  *networkExecuteHandle,
                               NetworkExecuteIOTypes ioType,
                               String                line,
                               long                  timeout
                              );

/***********************************************************************\
* Name   : Network_executeFlush
* Purpose: flush stdout/stderr for remote program
* Input  : networkExecuteHandle - network execute handle
*          ioType               - i/o type; see
*                                 NETWORK_EXECUTE_IO_TYPES_*
* Output : -
* Return : ERROR_NONE if data written, error code otherwise
* Notes  : -
\***********************************************************************/

void Network_executeFlush(NetworkExecuteHandle  *networkExecuteHandle,
                          NetworkExecuteIOTypes ioType
                         );

/***********************************************************************\
* Name   : Network_keepAlive
* Purpose: send keep alive
* Input  : socketHandle - socket handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Network_executeKeepAlive(NetworkExecuteHandle *networkExecuteHandle);

#ifdef __cplusplus
  }
#endif

#endif /* __NETWORK__ */

/* end of file */
