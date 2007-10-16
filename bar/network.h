/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/network.h,v $
* $Revision: 1.8 $
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
  SOCKET_TYPE_TLS,
} SocketTypes;

typedef struct
{
  SocketTypes      type;
  int              handle; 
  #ifdef HAVE_GNU_TLS
    gnutls_session_t gnuTLSSession;
  #endif /* HAVE_GNU_TLS */
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

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Network_init
* Purpose: init network functions
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_init(void);

/***********************************************************************\
* Name   : Network_done
* Purpose: deinitialize network functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Network_done(void);

/***********************************************************************\
* Name   : Network_connect
* Purpose: connect to host
* Input  : hostName - host name
*          hostPort - host port
*          flags    - socket falgs
* Output : socketHandle - socket handle
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_connect(SocketHandle *socketHandle,
                       String       hostName,
                       uint         hostPort,
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
* Name   : Network_receive
* Purpose: 
* Input  : socketHandle - socket handle
*          buffer       - data buffer
*          maxLength    - max. length of data (in bytes)
* Output : receivedBytes - number of bytes received
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Network_receive(SocketHandle *socketHandle,
                       void         *buffer,
                       ulong        maxLength,
                       ulong        *receivedBytes
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
                          uint               serverPort,
                          ServerTypes        serverType,
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

#ifdef __cplusplus
  }
#endif

#endif /* __NETWORK__ */

/* end of file */
