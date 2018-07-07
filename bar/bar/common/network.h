/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Network functions
* Systems: all
*
\***********************************************************************/

#ifndef __NETWORK__
#define __NETWORK__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_NETINET_IN_H
  #include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */
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

#include "passwords.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define SOCKET_FLAG_NONE         0
#define SOCKET_FLAG_NON_BLOCKING (1 << 0)
#define SOCKET_FLAG_NO_DELAY     (1 << 1)
#define SOCKET_FLAG_KEEP_ALIVE   (1 << 2)

/***************************** Datatypes *******************************/
typedef enum
{
  SOCKET_TYPE_PLAIN,
  SOCKET_TYPE_TLS,
  SOCKET_TYPE_SSH
} SocketTypes;

typedef struct
{
  SocketTypes type;
  int         handle;
  uint        flags;
  bool        isConnected;                   // TRUE iff connected
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
        gnutls_certificate_credentials_t credentials;
        gnutls_dh_params_t               dhParams;
        gnutls_session_t                 session;
      } gnuTLS;
    #endif /* HAVE_GNU_TLS */
  };
} SocketHandle;

typedef struct
{
  enum
  {
    SOCKET_ADDRESS_TYPE_NONE,
    SOCKET_ADDRESS_TYPE_V4,
    SOCKET_ADDRESS_TYPE_V6
  } type;
  union
  {
    struct in_addr  v4;
    struct in6_addr v6;
  } address;
} SocketAddress;

typedef enum
{
  SERVER_SOCKET_TYPE_PLAIN,
  SERVER_SOCKET_TYPE_TLS,
} ServerSocketTypes;

typedef struct
{
  ServerSocketTypes socketType;
  int               handle;
  #ifdef HAVE_GNU_TLS
    const void *caData;
    uint       caLength;
    const void *certData;
    uint       certLength;
    const void *keyData;
    uint       keyLength;
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
* Name   : Network_getHostName
* Purpose: get host name
* Input  : hostName - host name variable
* Output : -
* Return : host name
* Notes  : -
\***********************************************************************/

String Network_getHostName(String hostName);

/***********************************************************************\
* Name   : Network_exists, Network_existsCString
* Purpose: check if host name is valid
* Input  : hostName - host name
* Output : -
* Return : TRUE iff host name exists and is valid, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Network_hostExists(ConstString hostName);
bool Network_hostExistsCString(const char *hostName);

/***********************************************************************\
* Name   : Network_connect
* Purpose: connect to host
* Input  : socketType          - socket type; see SOCKET_TYPE_*
*          hostName            - host name
*          hostPort            - host port (host byte order)
*          loginName           - login user name
*          password            - SSH private key password or NULL
*          sshPublicKeyData    - SSH public key data for login or NULL
*          sshPublicKeyLength  - SSH public key data length
*          sshPrivateKeyData   - SSH private key data for login or NULL
*          sshPrivateKeyLength - SSH private key data length
*          flags               - socket flags; see SOCKET_FLAG_*
* Output : socketHandle - socket handle
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_connect(SocketHandle *socketHandle,
                       SocketTypes  socketType,
                       ConstString  hostName,
                       uint         hostPort,
                       ConstString  loginName,
                       Password     *password,
                       const void   *sshPublicKeyData,
                       uint         sshPublicKeyLength,
                       const void   *sshPrivateKeyData,
                       uint         sshPrivateKeyLength,
                       uint         flags
                      );

/***********************************************************************\
* Name   : Network_connectDescriptor
* Purpose: connect to host by descriptor
* Input  : socketType          - socket type; see SOCKET_TYPE_*
*          socketDescriptor    - socket descriptor
*          loginName           - login user name
*          password            - SSH private key password or NULL
*          sshPublicKeyData    - SSH public key data for login or NULL
*          sshPublicKeyLength  - SSH public key data length
*          sshPrivateKeyData   - SSH private key data for login or NULL
*          sshPrivateKeyLength - SSH private key data length
*          flags               - socket flags; see SOCKET_FLAG_*
* Output : socketHandle - socket handle
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_connectDescriptor(SocketHandle *socketHandle,
                                 int          socketDescriptor,
                                 SocketTypes  socketType,
                                 ConstString  loginName,
                                 Password     *password,
                                 const void   *sshPublicKeyData,
                                 uint         sshPublicKeyLength,
                                 const void   *sshPrivateKeyData,
                                 uint         sshPrivateKeyLength,
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
* Name   : Network_isConnected
* Purpose: check if connected
* Input  : socketHandle - socket handle
* Output : -
* Return : TRUE iff connected
* Notes  : connection state is only updated by calling Network_receive()!
\***********************************************************************/

INLINE bool Network_isConnected(SocketHandle *socketHandle);
#if defined(NDEBUG) || defined(__NETWORK_IMPLEMENTATION__)
INLINE bool Network_isConnected(SocketHandle *socketHandle)
{
  assert(socketHandle != NULL);

  return socketHandle->isConnected;
}
#endif /* NDEBUG || __NETWORK_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Network_getSocket
* Purpose: get socket from socket handle
* Input  : socketHandle - socket handle
* Output : -
* Return : socket
* Notes  : -
\***********************************************************************/

INLINE int Network_getSocket(const SocketHandle *socketHandle);
#if defined(NDEBUG) || defined(__NETWORK_IMPLEMENTATION__)
INLINE int Network_getSocket(const SocketHandle *socketHandle)
{
  assert(socketHandle != NULL);

  return socketHandle->handle;
}
#endif /* NDEBUG || __NETWORK_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Network_getSSHSession
* Purpose: get SSH session from socket handle
* Input  : socketHandle - socket handle
* Output : -
* Return : ssh session
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SSH2
INLINE LIBSSH2_SESSION *Network_getSSHSession(SocketHandle *socketHandle);
#if defined(NDEBUG) || defined(__NETWORK_IMPLEMENTATION__)
INLINE LIBSSH2_SESSION *Network_getSSHSession(SocketHandle *socketHandle)
{
  assert(socketHandle != NULL);
  assert(socketHandle->type == SOCKET_TYPE_SSH);

  return socketHandle->ssh2.session;
}
#endif /* NDEBUG || __NETWORK_IMPLEMENTATION__ */
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
                         ConstString  line
                        );

/***********************************************************************\
* Name   : Network_initServer
* Purpose: initialize a server socket
* Input  : serverPort        - server port (host byte order)
*          ServerSocketTypes - server socket type; see
*                              SERVER_SOCKET_TYPE_*
*          caData            - TLS CA data or NULL
*          caLength          - TLS CA data length
*          cert              - TLS cerificate or NULL
*          certLength        - TLS cerificate data length
*          key               - TLS private key or NULL
*          keyLength         - TLS private key data length
* Output : serverSocketHandle - server socket handle
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_initServer(ServerSocketHandle *serverSocketHandle,
                          uint               serverPort,
                          ServerSocketTypes  serverSocketType,
                          const void         *caData,
                          uint               caLength,
                          const void         *certData,
                          uint               certLength,
                          const void         *keyData,
                          uint               keyLength
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
* Name   : Network_startSSL
* Purpose: start SSL/TLS encryption on socket connection
* Input  : socketHandle - socket handle
*          caData       - TLS CA data or NULL (PEM encoded)
*          caLength     - TLS CA data length
*          cert         - TLS cerificate or NULL (PEM encoded)
*          certLength   - TLS cerificate data length
*          key          - TLS private key or NULL (PEM encoded)
*          keyLength    - TLS private key data length
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : call after Network_accept() to establish a SSL encryption
\***********************************************************************/

Errors Network_startSSL(SocketHandle *socketHandle,
                        const void   *caData,
                        uint         caLength,
                        const void   *certData,
                        uint         certLength,
                        const void   *keyData,
                        uint         keyLength
                       );

/***********************************************************************\
* Name   : Network_getLocalInfo
* Purpose: get local socket info
* Input  : socketHandle - socket handle
* Output : name          - local name (name or IP address as n.n.n.n)
*          port          - local port (host byte order)
*          socketAddress - local socket address (can be NULL)
* Return : -
* Notes  : -
\***********************************************************************/

void Network_getLocalInfo(SocketHandle  *socketHandle,
                          String        name,
                          uint          *port,
                          SocketAddress *socketAddress
                         );

/***********************************************************************\
* Name   : Network_getRemoteInfo
* Purpose: get remove socket info
* Input  : socketHandle - socket handle
* Output : name          - remote name (name or IP address, can be NULL)
*          port          - remote port (host byte order, can be NULL)
*          socketAddress - remote socket address (can be NULL)
* Return : -
* Notes  : -
\***********************************************************************/

void Network_getRemoteInfo(SocketHandle  *socketHandle,
                           String        name,
                           uint          *port,
                           SocketAddress *socketAddress
                          );

/***********************************************************************\
* Name   : Network_isLocalHost
* Purpose: check if address local host
* Input  : socketAddress - socket address
* Output : -
* Return : TRUE iff local host address
* Notes  : -
\***********************************************************************/

bool Network_isLocalHost(const SocketAddress *socketAddress);

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
                                ConstString          line
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