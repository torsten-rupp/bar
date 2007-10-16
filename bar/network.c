/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/network.c,v $
* $Revision: 1.9 $
* $Author: torsten $
* Contents: Network functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef HAVE_GNU_TLS
  #include <gnutls/gnutls.h>
#endif /* HAVE_GNU_TLS */
#include <assert.h>

#include <linux/tcp.h>

#include "global.h"
#include "strings.h"
#include "errors.h"
#include "bar.h"

#include "network.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#ifdef HAVE_GNU_TLS
  #define DH_BITS 1024
#else /* not HAVE_GNU_TLS */
#endif /* HAVE_GNU_TLS */

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

Errors Network_init(void)
{
  #ifdef HAVE_GNU_TLS
    gnutls_global_init();
    gnutls_global_set_log_level(10);
  #endif /* HAVE_GNU_TLS */

  return ERROR_NONE;
}

void Network_done(void)
{
  #ifdef HAVE_GNU_TLS
  #endif /* HAVE_GNU_TLS */
}

Errors Network_connect(SocketHandle *socketHandle,
                       String       hostName,
                       uint         hostPort,
                       uint         flags
                      )
{
  struct hostent     *hostAddressEntry;
  in_addr_t          ipAddress;
  struct sockaddr_in socketAddress;

  assert(socketHandle != NULL);

  /* get host IP address */
  hostAddressEntry = gethostbyname(String_cString(hostName));
  if (hostAddressEntry != NULL)
  {
    assert(hostAddressEntry->h_length > 0);
    ipAddress = (*((in_addr_t*)hostAddressEntry->h_addr_list[0]));
  }
  else
  {
    ipAddress = inet_addr(String_cString(hostName));
  }
  if (ipAddress == INADDR_NONE)
  {
    return ERROR_HOST_NOT_FOUND;
  }

  /* connect */
  socketHandle->handle = socket(AF_INET,SOCK_STREAM,0);
  if (socketHandle->handle == -1)
  {
    return ERROR_CONNECT_FAIL;
  }
  socketAddress.sin_family      = AF_INET;
  socketAddress.sin_addr.s_addr = ipAddress;
  socketAddress.sin_port        = htons(hostPort);
  if (connect(socketHandle->handle,
              (struct sockaddr*)&socketAddress,
              sizeof(socketAddress)
             ) != 0
     )
  {
    close(socketHandle->handle);
    return ERROR_CONNECT_FAIL;
  }

  if ((flags & SOCKET_FLAG_NON_BLOCKING) != 0)
  {
    /* enable non-blocking */
    fcntl(socketHandle->handle,F_SETFL,O_NONBLOCK);
  }

  return ERROR_NONE;
}

void Network_disconnect(SocketHandle *socketHandle)
{
  assert(socketHandle != NULL);
  assert(socketHandle->handle >= 0);

  switch (socketHandle->type)
  {
    case SOCKET_TYPE_PLAIN:
      break;
    case SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
        gnutls_deinit(socketHandle->gnuTLSSession);
      #else /* not HAVE_GNU_TLS */
      #endif /* HAVE_GNU_TLS */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  close(socketHandle->handle);
}

int Network_getSocket(SocketHandle *socketHandle)
{
  assert(socketHandle != NULL);

  return socketHandle->handle;
}

Errors Network_send(SocketHandle *socketHandle,
                    const void   *buffer,
                    ulong        length
                   )
{
  long sentBytes;

  assert(socketHandle != NULL);

  switch (socketHandle->type)
  {
    case SOCKET_TYPE_PLAIN:
      sentBytes = send(socketHandle->handle,buffer,length,0);
      break;
    case SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
        sentBytes = gnutls_record_send(socketHandle->gnuTLSSession,buffer,length);
      #else /* not HAVE_GNU_TLS */
        sentBytes = 0L;
      #endif /* HAVE_GNU_TLS */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return (sentBytes == length)?ERROR_NONE:ERROR_NETWORK_SEND;
}

Errors Network_receive(SocketHandle *socketHandle,
                       void         *buffer,
                       ulong        maxLength,
                       ulong        *receivedBytes
                      )
{
  long n;

  assert(socketHandle != NULL);
  assert(receivedBytes != NULL);

  switch (socketHandle->type)
  {
    case SOCKET_TYPE_PLAIN:
      n = recv(socketHandle->handle,buffer,maxLength,0);
      break;
    case SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
        n = gnutls_record_recv(socketHandle->gnuTLSSession,buffer,maxLength);
      #else /* not HAVE_GNU_TLS */
        n = 0L;
      #endif /* HAVE_GNU_TLS */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  (*receivedBytes) = (n >= 0)?n:0;

  return ((*receivedBytes) >= 0)?ERROR_NONE:ERROR_NETWORK_RECEIVE;
}

Errors Network_initServer(ServerSocketHandle *serverSocketHandle,
                          uint               serverPort,
                          ServerTypes        serverType,
                          const char         *caFileName,
                          const char         *certFileName,
                          const char         *keyFileName
                         )
{
  #ifdef HAVE_GNU_TLS
    int result;
  #endif /* HAVE_GNU_TLS */
  struct sockaddr_in socketAddress;
  int                n;

  assert(serverSocketHandle != NULL);

  serverSocketHandle->type = serverType;

  serverSocketHandle->handle = socket(AF_INET,SOCK_STREAM,0);
  if (serverSocketHandle->handle == -1)
  {
    return ERROR_CONNECT_FAIL;
  }

  n = 1;
  if (setsockopt(serverSocketHandle->handle,SOL_SOCKET,SO_REUSEADDR,&n,sizeof(int)) != 0)
  {
    close(serverSocketHandle->handle);
    return ERROR_CONNECT_FAIL;
  }

  socketAddress.sin_family      = AF_INET;
  socketAddress.sin_addr.s_addr = INADDR_ANY;
  socketAddress.sin_port        = htons(serverPort);
  if (bind(serverSocketHandle->handle,
           (struct sockaddr*)&socketAddress,
           sizeof(socketAddress)
          ) != 0
     )
  {
    close(serverSocketHandle->handle);
    return ERROR_CONNECT_FAIL;
  }
  listen(serverSocketHandle->handle,5);

  switch (serverType)
  {
    case SERVER_TYPE_PLAIN:
      break;
    case SERVER_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
        if (   (caFileName == NULL)
            || (certFileName == NULL)
            || (keyFileName == NULL)
           )
        {
          close(serverSocketHandle->handle);
          return ERROR_INIT_TLS;
        }

        if (gnutls_certificate_allocate_credentials(&serverSocketHandle->gnuTLSCredentials) != 0)
        {
          close(serverSocketHandle->handle);
          return ERROR_INIT_TLS;
        }
        result = gnutls_certificate_set_x509_trust_file(serverSocketHandle->gnuTLSCredentials,
                                                        caFileName,
                                                        GNUTLS_X509_FMT_PEM
                                                       );
        if (result < 0)
        {
          gnutls_certificate_free_credentials(serverSocketHandle->gnuTLSCredentials);
          close(serverSocketHandle->handle);
          return ERROR_INIT_TLS;
        }
        result = gnutls_certificate_set_x509_key_file(serverSocketHandle->gnuTLSCredentials,
                                                      certFileName,
                                                      keyFileName,
                                                      GNUTLS_X509_FMT_PEM
                                                     );
        if (result < 0)
        {
          gnutls_certificate_free_credentials(serverSocketHandle->gnuTLSCredentials);
          close(serverSocketHandle->handle);
          return ERROR_INIT_TLS;
        }

        gnutls_dh_params_init(&serverSocketHandle->gnuTLSDHParams);
        result = gnutls_dh_params_generate2(serverSocketHandle->gnuTLSDHParams,DH_BITS);
        if (result < 0)
        {
          gnutls_dh_params_deinit(serverSocketHandle->gnuTLSDHParams);
          gnutls_certificate_free_credentials(serverSocketHandle->gnuTLSCredentials);
          close(serverSocketHandle->handle);
          return ERROR_INIT_TLS;
        }
        gnutls_certificate_set_dh_params(serverSocketHandle->gnuTLSCredentials,serverSocketHandle->gnuTLSDHParams);
      #else /* not HAVE_GNU_TLS */
        UNUSED_VARIABLE(caFileName);
        UNUSED_VARIABLE(certFileName);
        UNUSED_VARIABLE(keyFileName);
      #endif /* HAVE_GNU_TLS */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

void Network_doneServer(ServerSocketHandle *serverSocketHandle)
{
  assert(serverSocketHandle != NULL);

  switch (serverSocketHandle->type)
  {
    case SERVER_TYPE_PLAIN:
      break;
    case SERVER_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
        gnutls_dh_params_deinit(serverSocketHandle->gnuTLSDHParams);
        gnutls_certificate_free_credentials(serverSocketHandle->gnuTLSCredentials);
      #else /* not HAVE_GNU_TLS */
      #endif /* HAVE_GNU_TLS */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  close(serverSocketHandle->handle);
}

int Network_getServerSocket(ServerSocketHandle *serverSocketHandle)
{
  assert(serverSocketHandle != NULL);

  return serverSocketHandle->handle;
}

Errors Network_accept(SocketHandle             *socketHandle,
                      const ServerSocketHandle *serverSocketHandle,
                      uint                     flags
                     )
{
  struct sockaddr_in socketAddress;
  socklen_t          socketAddressLength;
  int                result;

  assert(socketHandle != NULL);
  assert(serverSocketHandle != NULL);

  /* accept */
  socketAddressLength = sizeof(socketAddress);
  socketHandle->handle = accept(serverSocketHandle->handle,
                                (struct sockaddr*)&socketAddress,
                                &socketAddressLength
                               );
  if (socketHandle->handle == -1)
  {
    close(socketHandle->handle);
    return ERROR_CONNECT_FAIL;
  }

  /* initialise TLS session */
  switch (serverSocketHandle->type)
  {
    case SERVER_TYPE_PLAIN:
      socketHandle->type = SOCKET_TYPE_PLAIN;
      break;
    case SERVER_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
        socketHandle->type = SOCKET_TYPE_TLS;

        /* initialise session */
        if (gnutls_init(&socketHandle->gnuTLSSession,GNUTLS_SERVER) != 0)
        {
          close(socketHandle->handle);
          return ERROR_INIT_TLS;
        }

        if (gnutls_set_default_priority(socketHandle->gnuTLSSession) != 0)
        {
          gnutls_deinit(socketHandle->gnuTLSSession);
          close(socketHandle->handle);
          return ERROR_INIT_TLS;
        }

        if (gnutls_credentials_set(socketHandle->gnuTLSSession,GNUTLS_CRD_CERTIFICATE,serverSocketHandle->gnuTLSCredentials) != 0)
        {
          gnutls_deinit(socketHandle->gnuTLSSession);
          close(socketHandle->handle);
          return ERROR_INIT_TLS;
        }

        gnutls_certificate_server_set_request(socketHandle->gnuTLSSession,GNUTLS_CERT_REQUEST);
      //  gnutls_certificate_server_set_request(socketHandle->gnuTLSSession,GNUTLS_CERT_REQUIRE);

        gnutls_dh_set_prime_bits(socketHandle->gnuTLSSession,DH_BITS);
        gnutls_transport_set_ptr(socketHandle->gnuTLSSession,(gnutls_transport_ptr_t)socketHandle->handle);

        /* do handshake */
        result = gnutls_handshake(socketHandle->gnuTLSSession);
        if (result < 0)
        {
          gnutls_deinit(socketHandle->gnuTLSSession);
          close(socketHandle->handle);
          return ERROR_TLS_HANDSHAKE;
        }
      #else /* not HAVE_GNU_TLS */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GNU_TLS */
      break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
  }

  if ((flags & SOCKET_FLAG_NON_BLOCKING) != 0)
  {
    /* enable non-blocking */
    fcntl(socketHandle->handle,F_SETFL,O_NONBLOCK);
  }

  return ERROR_NONE;
}

void Network_getLocalInfo(SocketHandle *socketHandle,
                          String       name,
                          uint         *port
                         )
{
  struct sockaddr_in socketAddress;
  socklen_t          socketAddressLength;

  assert(socketHandle != NULL);
  assert(name != NULL);
  assert(port != NULL);

  socketAddressLength = sizeof(socketAddress);
  if (getsockname(socketHandle->handle,
                  (struct sockaddr*)&socketAddress,
                  &socketAddressLength
                 ) == 0
     )
  {
    String_setCString(name,inet_ntoa(socketAddress.sin_addr));
    (*port) = ntohs(socketAddress.sin_port);
  }
  else
  {
    String_setCString(name,"unknown");
    (*port) = 0;
  }
}

void Network_getRemoteInfo(SocketHandle *socketHandle,
                           String       name,
                           uint         *port
                          )
{
  struct sockaddr_in socketAddress;
  socklen_t          socketAddressLength;

  assert(socketHandle != NULL);
  assert(name != NULL);
  assert(port != NULL);

  socketAddressLength = sizeof(socketAddress);
  if (getpeername(socketHandle->handle,
                  (struct sockaddr*)&socketAddress,
                  &socketAddressLength
                 ) == 0
     )
  {
    String_setCString(name,inet_ntoa(socketAddress.sin_addr));
    (*port) = ntohs(socketAddress.sin_port);
  }
  else
  {
    String_setCString(name,"unknown");
    (*port) = 0;
  }
}

#ifdef __cplusplus
  }
#endif

/* end of file */
