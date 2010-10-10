/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/network.c,v $
* $Revision: 1.12 $
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
#include <sys/ioctl.h>
#include <sys/select.h>
#include <signal.h>
#ifdef HAVE_SSH2
  #include <libssh2.h>
#endif /* HAVE_SSH2 */
#ifdef HAVE_GNU_TLS
  #include <gnutls/gnutls.h>
  #include <gnutls/x509.h>
#endif /* HAVE_GNU_TLS */
#include <errno.h>
#include <assert.h>

#include <linux/tcp.h>

#include "global.h"
#include "strings.h"
#include "files.h"
#include "errors.h"

#include "bar.h"
#include "passwords.h"

#include "network.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#ifdef HAVE_GNU_TLS
  #define DH_BITS 1024
#else /* not HAVE_GNU_TLS */
#endif /* HAVE_GNU_TLS */

#define SEND_TIMEOUT 30000

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
#ifdef HAVE_SSH2
  LOCAL String defaultSSHPublicKeyFileName;
  LOCAL String defaultSSHPrivateKeyFileName;
#endif /* HAVE_SSH2 */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

Errors Network_initAll(void)
{
  #ifdef HAVE_SSH2
    defaultSSHPublicKeyFileName  = String_new();
    defaultSSHPrivateKeyFileName = String_new();
    File_setFileNameCString(defaultSSHPublicKeyFileName,getenv("HOME"));
    File_appendFileNameCString(defaultSSHPublicKeyFileName,".ssh");
    File_appendFileNameCString(defaultSSHPublicKeyFileName,"id_rsa.pub");
    File_setFileNameCString(defaultSSHPrivateKeyFileName,getenv("HOME"));
    File_appendFileNameCString(defaultSSHPrivateKeyFileName,".ssh");
    File_appendFileNameCString(defaultSSHPrivateKeyFileName,"id_rsa");
  #else /* not HAVE_SSH2 */
  #endif /* HAVE_SSH2 */

  #ifdef HAVE_GNU_TLS
    gnutls_global_init();
    gnutls_global_set_log_level(10);
  #endif /* HAVE_GNU_TLS */

  /* ignore SIGPIPE which may triggered in some socket read/write
     operations if the socket is closed
  */
  signal(SIGPIPE,SIG_IGN);

  return ERROR_NONE;
}

void Network_doneAll(void)
{
  #ifdef HAVE_GNU_TLS
    gnutls_global_deinit();
  #endif /* HAVE_GNU_TLS */
  #ifdef HAVE_SSH2
    String_delete(defaultSSHPublicKeyFileName); defaultSSHPublicKeyFileName=NULL;
    String_delete(defaultSSHPrivateKeyFileName); defaultSSHPrivateKeyFileName=NULL;
  #else /* not HAVE_SSH2 */
  #endif /* HAVE_SSH2 */
}

bool Network_hostExists(const String hostName)
{
  return Network_hostExistsCString(String_cString(hostName));
}

bool Network_hostExistsCString(const char *hostName)
{
  #if   defined(HAVE_GETHOSTBYNAME_R)
    char           buffer[512];
    struct hostent bufferAddressEntry;
    struct hostent *hostAddressEntry;
    int            getHostByNameError;
  #elif defined(HAVE_GETHOSTBYNAME)
    const struct hostent *hostAddressEntry;
  #endif /* HAVE_GETHOSTBYNAME* */

  #if   defined(HAVE_GETHOSTBYNAME_R)
    return    (gethostbyname_r(hostName,
                               &bufferAddressEntry,
                               buffer,
                               sizeof(buffer),
                               &hostAddressEntry,
                               &getHostByNameError
                              ) == 0)
           && (hostAddressEntry != NULL);
  #elif defined(HAVE_GETHOSTBYNAME)
    return gethostbyname(hostName) != NULL;
  #else /* not HAVE_GETHOSTBYNAME* */
    return FALSE;
  #endif /* HAVE_GETHOSTBYNAME_R */
}

Errors Network_connect(SocketHandle *socketHandle,
                       SocketTypes  socketType,
                       const String hostName,
                       uint         hostPort,
                       const String loginName,
                       Password     *password,
                       const String sshPublicKeyFileName,
                       const String sshPrivateKeyFileName,
                       uint         flags
                      )
{
  #if   defined(HAVE_GETHOSTBYNAME_R)
    char           buffer[512];
    struct hostent bufferAddressEntry;
    int            getHostByNameError;
  #elif defined(HAVE_GETHOSTBYNAME)
  #endif /* HAVE_GETHOSTBYNAME* */
  struct hostent     *hostAddressEntry;
  in_addr_t          ipAddress;
  struct sockaddr_in socketAddress;
  int                ssh2Error;
  char               *ssh2ErrorText;
  Errors             error;
  long               socketFlags;

  assert(socketHandle != NULL);

  /* initialize variables */
  socketHandle->type = socketType;

  switch (socketType)
  {
    case SOCKET_TYPE_PLAIN:
      /* get host IP address */
      #if   defined(HAVE_GETHOSTBYNAME_R)
        if (   (gethostbyname_r(String_cString(hostName),
                                &bufferAddressEntry,
                                buffer,
                                sizeof(buffer),
                                 &hostAddressEntry,
                                &getHostByNameError
                               ) != 0)
            && (hostAddressEntry != NULL)
           )
        {
          hostAddressEntry = NULL;
        }
      #elif defined(HAVE_GETHOSTBYNAME)
        hostAddressEntry = gethostbyname(String_cString(hostName));
      #else /* not HAVE_GETHOSTBYNAME_R */
        hostAddressEntry = NULL;
      #endif /* HAVE_GETHOSTBYNAME_R */
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
        return ERROR(CONNECT_FAIL,errno);
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
        error = ERROR(CONNECT_FAIL,errno);
        close(socketHandle->handle);
        return error;
      }

      if ((flags & SOCKET_FLAG_NON_BLOCKING) != 0)
      {
        /* enable non-blocking */
        socketFlags = fcntl(socketHandle->handle,F_GETFL,0);
        fcntl(socketHandle->handle,F_SETFL,socketFlags | O_NONBLOCK);
      }

      break;
    case SOCKET_TYPE_SSH:
      #ifdef HAVE_SSH2
      {
        const char *plainPassword;
        long       socketFlags;

        assert(loginName != NULL);

        /* initialise variables */

        /* get host IP address */
        #if   defined(HAVE_GETHOSTBYNAME_R)
          if (  (gethostbyname_r(String_cString(hostName),
                                 &bufferAddressEntry,
                                 buffer,
                                 sizeof(buffer),
                                 &hostAddressEntry,
                                 &getHostByNameError
                                ) != 0)
              && (hostAddressEntry != NULL)
             )
          {
            hostAddressEntry = NULL;
          }
        #elif defined(HAVE_GETHOSTBYNAME)
          hostAddressEntry = gethostbyname(String_cString(hostName));
        #else /* not HAVE_GETHOSTBYNAME_R */
          hostAddressEntry = NULL;
        #endif /* HAVE_GETHOSTBYNAME_R */
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
          return ERROR(CONNECT_FAIL,errno);
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
          error = ERROR(CONNECT_FAIL,errno);
          close(socketHandle->handle);
          return error;
        }

        /* check login name */
        if (String_empty(loginName))
        {
          close(socketHandle->handle);
          return ERROR_NO_LOGIN_NAME;
        }

        /* init session */
        socketHandle->ssh2.session = libssh2_session_init();
        if (socketHandle->ssh2.session == NULL)
        {
          close(socketHandle->handle);
          return ERROR_SSH_SESSION_FAIL;
        }
        if (libssh2_session_startup(socketHandle->ssh2.session,
                                    socketHandle->handle
                                   ) != 0
           )
        {
          libssh2_session_disconnect(socketHandle->ssh2.session,"");
          libssh2_session_free(socketHandle->ssh2.session);
          close(socketHandle->handle);
          return ERROR_SSH_SESSION_FAIL;
        }
        #ifdef HAVE_SSH2_KEEPALIVE_CONFIG
// NYI/???: does not work?
//          libssh2_keepalive_config(socketHandle->ssh2.session,0,2*60);
        #endif /* HAVE_SSH2_KEEPALIVE_CONFIG */

#if 1
        plainPassword = Password_deploy(password);
        if (libssh2_userauth_publickey_fromfile(socketHandle->ssh2.session,
                                                String_cString(loginName),
                                                String_cString(!String_empty(sshPublicKeyFileName)?sshPublicKeyFileName:defaultSSHPublicKeyFileName),
                                                String_cString(!String_empty(sshPrivateKeyFileName)?sshPrivateKeyFileName:defaultSSHPrivateKeyFileName),
                                                plainPassword
                                               ) != 0)
        {
          ssh2Error = libssh2_session_last_error(socketHandle->ssh2.session,&ssh2ErrorText,NULL,0);
          error = ERRORX(SSH_AUTHENTIFICATION,ssh2Error,ssh2ErrorText);
          Password_undeploy(password);
          libssh2_session_disconnect(socketHandle->ssh2.session,"");
          libssh2_session_free(socketHandle->ssh2.session);
          close(socketHandle->handle);
          return error;
        }
        Password_undeploy(password);
#else
        if (libssh2_userauth_keyboard_interactive(socketHandle->ssh2.session,
                                                  String_cString(loginName),
                                                  NULL
                                                ) != 0
           )
        {
          ssh2Error = libssh2_session_last_error(socketHandle->ssh2.session,&ssh2ErrorText,NULL,0);
          error = ERRORX(SSH_AUTHENTIFICATION,ssh2Error,ssh2ErrorText);
          libssh2_session_disconnect(socketHandle->ssh2.session,"");
          libssh2_session_free(socketHandle->ssh2.session);
          close(socketHandle->handle);
          return error;
        }
#endif /* 0 */

        if ((flags & SOCKET_FLAG_NON_BLOCKING) != 0)
        {
          /* enable non-blocking */
          socketFlags = fcntl(socketHandle->handle,F_GETFL,0);
          fcntl(socketHandle->handle,F_SETFL,socketFlags | O_NONBLOCK);
        }
      }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(loginName);
        UNUSED_VARIABLE(password);
        UNUSED_VARIABLE(sshPublicKeyFileName);
        UNUSED_VARIABLE(sshPrivateKeyFileName);

        close(socketHandle->handle);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case SOCKET_TYPE_TLS:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
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
    case SOCKET_TYPE_SSH:
      #ifdef HAVE_SSH2
        libssh2_session_disconnect(socketHandle->ssh2.session,"");
        libssh2_session_free(socketHandle->ssh2.session);
        sleep(1);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
        gnutls_deinit(socketHandle->gnuTLS.session);
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

#ifdef HAVE_SSH2
LIBSSH2_SESSION *Network_getSSHSession(SocketHandle *socketHandle)
{
  assert(socketHandle != NULL);
  assert(socketHandle->type == SOCKET_TYPE_SSH);

  return socketHandle->ssh2.session;
}
#endif /* HAVE_SSH2 */

bool Network_eof(SocketHandle *socketHandle)
{
  bool eofFlag;

  assert(socketHandle != NULL);

  eofFlag = TRUE;
  switch (socketHandle->type)
  {
    case SOCKET_TYPE_PLAIN:
      {
        int n;

        eofFlag = (ioctl(socketHandle->handle,FIONREAD,&n,sizeof(int)) != 0) || (n == 0);
      }
      break;
    case SOCKET_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_GNU_TLS */
      #endif /* HAVE_GNU_TLS */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return eofFlag;
}

ulong Network_getAvaibleBytes(SocketHandle *socketHandle)
{
  ulong bytesAvailable;

  assert(socketHandle != NULL);

  bytesAvailable = 0L;
  switch (socketHandle->type)
  {
    case SOCKET_TYPE_PLAIN:
      {
        int n;

        if (ioctl(socketHandle->handle,FIONREAD,&n,sizeof(int)) == 0)
        {
          bytesAvailable = n;
        }
      }
      break;
    case SOCKET_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_GNU_TLS */
      #endif /* HAVE_GNU_TLS */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return bytesAvailable;
}

Errors Network_receive(SocketHandle *socketHandle,
                       void         *buffer,
                       ulong        maxLength,
                       long         timeout,
                       ulong        *bytesReceived
                      )
{
  struct timeval tv;
  fd_set         fdSet;
  long           n;

  assert(socketHandle != NULL);
  assert(bytesReceived != NULL);

  n = -1L;
  switch (socketHandle->type)
  {
    case SOCKET_TYPE_PLAIN:
      if (timeout == WAIT_FOREVER)
      {
        n = recv(socketHandle->handle,buffer,maxLength,0);
      }
      else
      {

        tv.tv_sec  = timeout/1000;
        tv.tv_usec = (timeout%1000)*1000;
        FD_ZERO(&fdSet);
        assert(socketHandle->handle < FD_SETSIZE);
        FD_SET(socketHandle->handle,&fdSet);
        if (select(socketHandle->handle+1,&fdSet,NULL,NULL,&tv) > 0)
        {
          n = recv(socketHandle->handle,buffer,maxLength,0);
        }
      }
      break;
    case SOCKET_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
        if (timeout == WAIT_FOREVER)
        {
          n = gnutls_record_recv(socketHandle->gnuTLS.session,buffer,maxLength);
        }
        else
        {
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
        }
      #else /* not HAVE_GNU_TLS */
      #endif /* HAVE_GNU_TLS */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  (*bytesReceived) = (n >= 0)?n:0;

  return (n >= 0)?ERROR_NONE:ERROR_NETWORK_RECEIVE;
}

Errors Network_send(SocketHandle *socketHandle,
                    const void   *buffer,
                    ulong        length
                   )
{
//  Errors         error;
  ulong          sentBytes;
  struct timeval tv;
  fd_set         fdSetInput,fdSetOutput,fdSetError;
  long           n;

  assert(socketHandle != NULL);

  sentBytes = 0L;
  if (length > 0)
  {
    switch (socketHandle->type)
    {
      case SOCKET_TYPE_PLAIN:
          do
          {
            /* wait until space in buffer is available */
            assert(socketHandle->handle < FD_SETSIZE);
            tv.tv_sec  = SEND_TIMEOUT/1000;
            tv.tv_usec = (SEND_TIMEOUT%1000)*1000;
            FD_ZERO(&fdSetInput);
            FD_SET(socketHandle->handle,&fdSetInput);
            FD_ZERO(&fdSetOutput);
            FD_SET(socketHandle->handle,&fdSetOutput);
            FD_ZERO(&fdSetError);
            FD_SET(socketHandle->handle,&fdSetError);
            select(socketHandle->handle+1,NULL,&fdSetOutput,NULL,&tv);

            /* send data */
            n = send(socketHandle->handle,((char*)buffer)+sentBytes,length-sentBytes,0);
            if      (n > 0) sentBytes += n;
            else if ((n == -1) && (errno != EAGAIN)) break;
          }
          while (sentBytes < length);
        break;
      case SOCKET_TYPE_SSH:
        #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
        #else /* not HAVE_SSH2 */
          return ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_SSH2 */
        break;
      case SOCKET_TYPE_TLS:
        #ifdef HAVE_GNU_TLS
          do
          {
            /* wait until space in buffer is available */
            assert(socketHandle->handle < FD_SETSIZE);
            tv.tv_sec  = SEND_TIMEOUT/1000;
            tv.tv_usec = (SEND_TIMEOUT%1000)*1000;
            FD_ZERO(&fdSetInput);
            FD_SET(socketHandle->handle,&fdSetInput);
            FD_ZERO(&fdSetOutput);
            FD_SET(socketHandle->handle,&fdSetOutput);
            FD_ZERO(&fdSetError);
            FD_SET(socketHandle->handle,&fdSetError);
            select(socketHandle->handle+1,NULL,&fdSetOutput,NULL,&tv);

            /* send data */
            n = gnutls_record_send(socketHandle->gnuTLS.session,((char*)buffer)+sentBytes,length-sentBytes);
            if      (n > 0) sentBytes += n;
            else if ((n < 0) && (errno != GNUTLS_E_AGAIN)) break;
          }
          while (sentBytes < length);
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
//  if (sentBytes != length) fprintf(stderr,"%s,%d: send error %d: %s\n",__FILE__,__LINE__,errno,strerror(errno));
  }

  return (sentBytes == length)?ERROR_NONE:ERROR_NETWORK_SEND;
}

Errors Network_readLine(SocketHandle *socketHandle,
                        String       line,
                        long         timeout
                       )
{
  bool   endOfLineFlag;
  Errors error;
  char   ch;
  ulong  bytesReceived;

  String_clear(line);
  endOfLineFlag = FALSE;
  while (!endOfLineFlag)
  {
// ??? optimize?
    /* read character */
    error = Network_receive(socketHandle,&ch,1,timeout,&bytesReceived);
    if (error != ERROR_NONE)
    {
      return error;
    }

    /* check eol, append to line */
    if (bytesReceived > 0)
    {
      if (ch != '\n')
      {
        if (ch != '\r')
        {
          String_appendChar(line,ch);
        }
      }
      else
      {
        endOfLineFlag = TRUE;
      }
    }
    else
    {
      endOfLineFlag = TRUE;
    }
  }

  return ERROR_NONE;
}

Errors Network_writeLine(SocketHandle *socketHandle,
                         const String line
                        )
{
  Errors error;

  assert(socketHandle != NULL);

  error = Network_send(socketHandle,String_cString(line),String_length(line));
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Network_send(socketHandle,"\n",1);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
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
  Errors             error;

  assert(serverSocketHandle != NULL);

  serverSocketHandle->type = serverType;

  // create socket
  serverSocketHandle->handle = socket(AF_INET,SOCK_STREAM,0);
  if (serverSocketHandle->handle == -1)
  {
    return ERROR(CONNECT_FAIL,errno);
  }

  // reuse address
  n = 1;
  if (setsockopt(serverSocketHandle->handle,SOL_SOCKET,SO_REUSEADDR,&n,sizeof(int)) != 0)
  {
    error = ERROR(CONNECT_FAIL,errno);
    close(serverSocketHandle->handle);
    return error;
  }

  // bind and listen socket
  socketAddress.sin_family      = AF_INET;
  socketAddress.sin_addr.s_addr = INADDR_ANY;
  socketAddress.sin_port        = htons(serverPort);
  if (bind(serverSocketHandle->handle,
           (struct sockaddr*)&socketAddress,
           sizeof(socketAddress)
          ) != 0
     )
  {
    error = ERROR(CONNECT_FAIL,errno);
    close(serverSocketHandle->handle);
    return error;
  }
  listen(serverSocketHandle->handle,5);

  switch (serverType)
  {
    case SERVER_TYPE_PLAIN:
      break;
    case SERVER_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
      {
        void              *certData;
        ulong             certDataSize;
        gnutls_datum_t    datum; 
        gnutls_x509_crt_t cert;

        // check if all key files exists and can be read
        if ((caFileName == NULL) || !File_isFileCString(caFileName) || !File_isReadableCString(caFileName))
        {
          close(serverSocketHandle->handle);
          return ERROR_NO_TLS_CA;
        }
        if ((certFileName == NULL) || !File_isFileCString(certFileName) || !File_isReadableCString(certFileName))
        {
          close(serverSocketHandle->handle);
          return ERROR_NO_TLS_CERTIFICATE;
        }
        if ((keyFileName == NULL) || !File_isFileCString(keyFileName) || !File_isReadableCString(keyFileName))
        {
          close(serverSocketHandle->handle);
          return ERROR_NO_TLS_KEY;
        }

        // check if certificate is valid
        error = File_getDataCString(certFileName,&certData,&certDataSize);
        if (error != ERROR_NONE)
        {
          close(serverSocketHandle->handle);
          return error;
        }
        if (gnutls_x509_crt_init(&cert) != GNUTLS_E_SUCCESS)
        {
          free(certData);
          close(serverSocketHandle->handle);
          return ERROR_INVALID_TLS_CERTIFICATE;
        }
        datum.data = certData;
        datum.size = certDataSize;
        if (gnutls_x509_crt_import(cert,&datum,GNUTLS_X509_FMT_PEM) != GNUTLS_E_SUCCESS)
        {
          free(certData);
          close(serverSocketHandle->handle);
          return ERROR_INVALID_TLS_CERTIFICATE;
        }
        if (time(NULL) > gnutls_x509_crt_get_expiration_time(cert))
        {
          free(certData);
          close(serverSocketHandle->handle);
          return ERROR_TLS_CERTIFICATE_EXPIRED;
        }
        if (time(NULL) < gnutls_x509_crt_get_activation_time(cert))
        {
          free(certData);
          close(serverSocketHandle->handle);
          return ERROR_TLS_CERTIFICATE_NOT_ACTIVE;
        }
#if 0
NYI: how to do certificate verification?
gnutls_x509_crt_t ca;
gnutls_x509_crt_init(&ca);
data=Xread_file("/etc/ssl/certs/bar-ca.pem",&size);
d.data=data,d.size=size;
fprintf(stderr,"%s,%d: import=%d\n",__FILE__,__LINE__,gnutls_x509_crt_import(ca,&d,GNUTLS_X509_FMT_PEM));

        if (gnutls_x509_crt_verify(cert,&ca,1,0,&verify));

or

        result = gnutls_certificate_set_x509_trust_file(serverSocketHandle->gnuTLSCredentials,
                                                        caFileName,
                                                        GNUTLS_X509_FMT_PEM
                                                       );
        if (result < 0)
        {
          gnutls_certificate_free_credentials(serverSocketHandle->gnuTLSCredentials);
          close(serverSocketHandle->handle);
          return ERROR_INVALID_TLS_CA;
        }
#endif /* 0 */
        gnutls_x509_crt_deinit(cert);
        free(certData);

        // init certificate and key
        if (gnutls_certificate_allocate_credentials(&serverSocketHandle->gnuTLSCredentials) != GNUTLS_E_SUCCESS)
        {
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
          return ERROR_INVALID_TLS_CERTIFICATE;
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
        gnutls_certificate_set_dh_params(serverSocketHandle->gnuTLSCredentials,
                                         serverSocketHandle->gnuTLSDHParams
                                        );
      }
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
  Errors             error;
  int                result;
  long               socketFlags;

//unsigned int status;

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
    error = ERROR(CONNECT_FAIL,errno);
    close(socketHandle->handle);
    return error;
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
        if (gnutls_init(&socketHandle->gnuTLS.session,GNUTLS_SERVER) != 0)
        {
          close(socketHandle->handle);
          return ERROR_INIT_TLS;
        }

        if (gnutls_set_default_priority(socketHandle->gnuTLS.session) != 0)
        {
          gnutls_deinit(socketHandle->gnuTLS.session);
          close(socketHandle->handle);
          return ERROR_INIT_TLS;
        }

        if (gnutls_credentials_set(socketHandle->gnuTLS.session,
                                   GNUTLS_CRD_CERTIFICATE,
                                   serverSocketHandle->gnuTLSCredentials
                                  ) != 0
           )
        {
          gnutls_deinit(socketHandle->gnuTLS.session);
          close(socketHandle->handle);
          return ERROR_INIT_TLS;
        }

#if 0
NYI: how to enable client authentication?
NYI: how to do certificate verification?
        gnutls_certificate_server_set_request(socketHandle->gnuTLS.session,
                                              GNUTLS_CERT_REQUEST
                                             );
//        gnutls_certificate_server_set_request(socketHandle->gnuTLS.session,GNUTLS_CERT_REQUIRE);
#endif /* 0 */

        gnutls_dh_set_prime_bits(socketHandle->gnuTLS.session,
                                 DH_BITS
                                );
        gnutls_transport_set_ptr(socketHandle->gnuTLS.session,
                                 (gnutls_transport_ptr_t)(long)socketHandle->handle
                                );

        /* do handshake */
        result = gnutls_handshake(socketHandle->gnuTLS.session);
        if (result < 0)
        {
          gnutls_deinit(socketHandle->gnuTLS.session);
          close(socketHandle->handle);
          return ERRORX(TLS_HANDSHAKE,result,gnutls_strerror(result));
        }

#if 0
NYI: how to enable client authentication?
        result = gnutls_certificate_verify_peers2(socketHandle->gnuTLS.session,&status);
        if (result < 0)
        {
          gnutls_deinit(socketHandle->gnuTLS.session);
          close(socketHandle->handle);
          return ERRORX(TLS_HANDSHAKE,result,gnutls_strerror(result));
        }
#endif /* 0 */
      #else /* not HAVE_GNU_TLS */
        UNUSED_VARIABLE(result);
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
    socketFlags = fcntl(socketHandle->handle,F_GETFL,0);
    fcntl(socketHandle->handle,F_SETFL,socketFlags | O_NONBLOCK);
  }

  return ERROR_NONE;
}

void Network_getLocalInfo(SocketHandle *socketHandle,
                          String       name,
                          uint         *port
                         )
{
  struct sockaddr_in   socketAddress;
  socklen_t            socketAddressLength;
  const struct hostent *hostEntry;

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
    #ifdef HAVE_GETHOSTBYADDR
      hostEntry = gethostbyaddr(&socketAddress.sin_addr,
                                sizeof(socketAddress.sin_addr),
                                AF_INET
                               );
      if (hostEntry != NULL)
      {
        String_setCString(name,hostEntry->h_name);
      }
      else
      {
        String_setCString(name,inet_ntoa(socketAddress.sin_addr));
      }
    #else /* not HAVE_GETHOSTBYADDR */
      String_setCString(name,inet_ntoa(socketAddress.sin_addr));
    #endif /* HAVE_GETHOSTBYADDR */
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
  struct sockaddr_in   socketAddress;
  socklen_t            socketAddressLength;
  const struct hostent *hostEntry;

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
    #ifdef HAVE_GETHOSTBYADDR_R
      hostEntry = gethostbyaddr(&socketAddress.sin_addr,
                                sizeof(socketAddress.sin_addr),
                                AF_INET
                               );
      if (hostEntry != NULL)
      {
        String_setCString(name,hostEntry->h_name);
      }
      else
      {
        String_setCString(name,inet_ntoa(socketAddress.sin_addr));
      }
    #else /* not HAVE_GETHOSTBYADDR_R */
      String_setCString(name,inet_ntoa(socketAddress.sin_addr));
    #endif /* HAVE_GETHOSTBYADDR_R */
    (*port) = ntohs(socketAddress.sin_port);
  }
  else
  {
    String_setCString(name,"unknown");
    (*port) = 0;
  }
}

/*---------------------------------------------------------------------*/

Errors Network_execute(NetworkExecuteHandle *networkExecuteHandle,
                       SocketHandle         *socketHandle,
                       ulong                ioMask,
                       const char           *command
                      )
{
  long socketFlags;

  assert(networkExecuteHandle != NULL);
  assert(socketHandle != NULL);
  assert(socketHandle->type == SOCKET_TYPE_SSH);
  assert(command != NULL);

  /* initialize variables */
  networkExecuteHandle->socketHandle        = socketHandle;
  networkExecuteHandle->stdoutBuffer.index  = 0;
  networkExecuteHandle->stdoutBuffer.length = 0;
  networkExecuteHandle->stderrBuffer.index  = 0;
  networkExecuteHandle->stderrBuffer.length = 0;

  #ifdef HAVE_SSH2
    /* open channel */
    networkExecuteHandle->channel = libssh2_channel_open_session(socketHandle->ssh2.session);
    if (networkExecuteHandle->channel == NULL)
    {
      return ERROR_NETWORK_EXECUTE_FAIL;
    }

    /* execute command */
    if (libssh2_channel_exec(networkExecuteHandle->channel,
                             command
                            ) != 0
       )
    {
      libssh2_channel_close(networkExecuteHandle->channel);
      libssh2_channel_wait_closed(networkExecuteHandle->channel);
      return ERROR_NETWORK_EXECUTE_FAIL;
    }

    /* enable non-blocking */
    socketFlags = fcntl(socketHandle->handle,F_GETFL,0);
    fcntl(socketHandle->handle,F_SETFL,socketFlags | O_NONBLOCK);
    libssh2_channel_set_blocking(networkExecuteHandle->channel,0);

    /* disable stderr if not requested */
    if ((ioMask & NETWORK_EXECUTE_IO_MASK_STDERR) == 0) libssh2_channel_handle_extended_data(networkExecuteHandle->channel,LIBSSH2_CHANNEL_EXTENDED_DATA_IGNORE);

    return ERROR_NONE;  
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(networkExecuteHandle);
    UNUSED_VARIABLE(ioMask);
    UNUSED_VARIABLE(command);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

int Network_terminate(NetworkExecuteHandle *networkExecuteHandle)
{
  int exitcode;

  assert(networkExecuteHandle != NULL);

  #ifdef HAVE_SSH2
    libssh2_channel_close(networkExecuteHandle->channel);
    libssh2_channel_wait_closed(networkExecuteHandle->channel);
    exitcode = libssh2_channel_get_exit_status(networkExecuteHandle->channel);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(networkExecuteHandle);

    exitcode = 128;
  #endif /* HAVE_SSH2 */

  return exitcode;
}

bool Network_executeEOF(NetworkExecuteHandle  *networkExecuteHandle,
                        NetworkExecuteIOTypes ioType,
                        long                  timeout
                       )
{
  Errors error;
  ulong  bytesRead;
  bool   eofFlag;

  assert(networkExecuteHandle != NULL);

  eofFlag = TRUE;
  switch (ioType)
  {
    case NETWORK_EXECUTE_IO_TYPE_STDOUT:
      if (networkExecuteHandle->stdoutBuffer.index >= networkExecuteHandle->stdoutBuffer.length)
      {
        error = Network_executeRead(networkExecuteHandle,
                                    NETWORK_EXECUTE_IO_TYPE_STDOUT,
                                    networkExecuteHandle->stdoutBuffer.data,
                                    sizeof(networkExecuteHandle->stdoutBuffer.data),
                                    timeout,
                                    &bytesRead
                                   );
        if (error != ERROR_NONE)
        {
          return TRUE;
        }
//fprintf(stderr,"%s,%d: bytesRead=%lu\n",__FILE__,__LINE__,bytesRead);
        networkExecuteHandle->stdoutBuffer.index = 0;
        networkExecuteHandle->stdoutBuffer.length = bytesRead;
      }
      eofFlag = (networkExecuteHandle->stdoutBuffer.index >= networkExecuteHandle->stdoutBuffer.length);
      break;
    case NETWORK_EXECUTE_IO_TYPE_STDERR:
      if (networkExecuteHandle->stderrBuffer.index >= networkExecuteHandle->stderrBuffer.length)
      {
        error = Network_executeRead(networkExecuteHandle,
                                    NETWORK_EXECUTE_IO_TYPE_STDERR,
                                    networkExecuteHandle->stderrBuffer.data,
                                    sizeof(networkExecuteHandle->stderrBuffer.data),
                                    timeout,
                                    &bytesRead
                                   );
        if (error != ERROR_NONE)
        {
          return TRUE;
        }
//fprintf(stderr,"%s,%d: bytesRead=%lu\n",__FILE__,__LINE__,bytesRead);
        networkExecuteHandle->stderrBuffer.index = 0;
        networkExecuteHandle->stderrBuffer.length = bytesRead;
      }
      eofFlag = (networkExecuteHandle->stderrBuffer.index >= networkExecuteHandle->stderrBuffer.length);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return eofFlag;
}

void Network_executeSendEOF(NetworkExecuteHandle *networkExecuteHandle)
{
  assert(networkExecuteHandle != NULL);

  #ifdef HAVE_SSH2
    libssh2_channel_send_eof(networkExecuteHandle->channel);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(networkExecuteHandle);
  #endif /* HAVE_SSH2 */
}

Errors Network_executeWrite(NetworkExecuteHandle *networkExecuteHandle,
                            const void           *buffer,
                            ulong                length
                           )
{
  long sentBytes;

  assert(networkExecuteHandle != NULL);

  #ifdef HAVE_SSH2
    sentBytes = libssh2_channel_write(networkExecuteHandle->channel,buffer,length);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(networkExecuteHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(length);
    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return (sentBytes == length)?ERROR_NONE:ERROR_NETWORK_SEND;
}

Errors Network_executeRead(NetworkExecuteHandle  *networkExecuteHandle,
                           NetworkExecuteIOTypes ioType,
                           void                  *buffer,
                           ulong                 maxLength,
                           long                  timeout,
                           ulong                 *bytesRead
                          )
{
  long n;

  assert(networkExecuteHandle != NULL);

  n = -1L;
  #ifdef HAVE_SSH2
    if (timeout == WAIT_FOREVER)
    {
      switch (ioType)
      {
        case NETWORK_EXECUTE_IO_TYPE_STDOUT:
          n = libssh2_channel_read(networkExecuteHandle->channel,buffer,maxLength);
          break;
        case NETWORK_EXECUTE_IO_TYPE_STDERR:
          n = libssh2_channel_read_stderr(networkExecuteHandle->channel,buffer,maxLength);
          break;
      }
    }
    else
    {
      LIBSSH2_POLLFD fds[1];

      fds[0].type       = LIBSSH2_POLLFD_CHANNEL;
      fds[0].fd.channel = networkExecuteHandle->channel;
      switch (ioType)
      {
        case NETWORK_EXECUTE_IO_TYPE_STDOUT: fds[0].events = LIBSSH2_POLLFD_POLLIN;  break;
        case NETWORK_EXECUTE_IO_TYPE_STDERR: fds[0].events = LIBSSH2_POLLFD_POLLEXT; break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
      fds[0].revents    = 0;
      if (libssh2_poll(fds,1,timeout) > 0)
      {
        switch (ioType)
        {
          case NETWORK_EXECUTE_IO_TYPE_STDOUT:
            n = libssh2_channel_read(networkExecuteHandle->channel,buffer,maxLength);
            break;
          case NETWORK_EXECUTE_IO_TYPE_STDERR:
            n = libssh2_channel_read_stderr(networkExecuteHandle->channel,buffer,maxLength);
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
          #endif /* NDEBUG */
        }
      }
      else
      {
        n = 0;
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(networkExecuteHandle);
    UNUSED_VARIABLE(ioType);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(maxLength);
    UNUSED_VARIABLE(timeout);
    UNUSED_VARIABLE(bytesRead);
    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  (*bytesRead) = (n >= 0)?n:0;

  return (n >= 0)?ERROR_NONE:ERROR_NETWORK_RECEIVE;
}

Errors Network_executeWriteLine(NetworkExecuteHandle *networkExecuteHandle,
                                const String         line
                               )
{
  Errors error;

  assert(networkExecuteHandle != NULL);

  error = Network_executeWrite(networkExecuteHandle,
                               String_cString(line),
                               String_length(line)
                              );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Network_executeWrite(networkExecuteHandle,
                               "\n",
                               1
                              );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Network_executeReadLine(NetworkExecuteHandle  *networkExecuteHandle,
                               NetworkExecuteIOTypes ioType,
                               String                line,
                               long                  timeout
                              )
{
  bool   endOfLineFlag;
  Errors error;
  ulong  bytesRead;

  assert(networkExecuteHandle != NULL);

  String_clear(line);
  endOfLineFlag = FALSE;
  while (!endOfLineFlag)
  {
    switch (ioType)
    {
      case NETWORK_EXECUTE_IO_TYPE_STDOUT:
        if (networkExecuteHandle->stdoutBuffer.index >= networkExecuteHandle->stdoutBuffer.length)
        {
          /* read character */
          error = Network_executeRead(networkExecuteHandle,
                                      NETWORK_EXECUTE_IO_TYPE_STDOUT,
                                      networkExecuteHandle->stdoutBuffer.data,
                                      sizeof(networkExecuteHandle->stdoutBuffer.data),
                                      timeout,
                                      &bytesRead
                                     );
          if (error != ERROR_NONE)
          {
            return error;
          }
    //fprintf(stderr,"%s,%d: bytesRead=%lu\n",__FILE__,__LINE__,bytesRead);

          networkExecuteHandle->stdoutBuffer.index = 0;
          networkExecuteHandle->stdoutBuffer.length = bytesRead;
        }

        /* check eol, append to line */
        if (networkExecuteHandle->stdoutBuffer.index < networkExecuteHandle->stdoutBuffer.length)
        {
          while (   !endOfLineFlag
                 && (networkExecuteHandle->stdoutBuffer.index < networkExecuteHandle->stdoutBuffer.length)
                )
          {
            if      (networkExecuteHandle->stdoutBuffer.data[networkExecuteHandle->stdoutBuffer.index] == '\n')
            {
              endOfLineFlag = TRUE;
            }
            else if (networkExecuteHandle->stdoutBuffer.data[networkExecuteHandle->stdoutBuffer.index] != '\r')
            {
              String_appendChar(line,networkExecuteHandle->stdoutBuffer.data[networkExecuteHandle->stdoutBuffer.index]);
            }
            networkExecuteHandle->stdoutBuffer.index++;
          }
        }
        else
        {
          endOfLineFlag = TRUE;
        }
        break;
      case NETWORK_EXECUTE_IO_TYPE_STDERR:
        if (networkExecuteHandle->stderrBuffer.index >= networkExecuteHandle->stderrBuffer.length)
        {
          /* read character */
          error = Network_executeRead(networkExecuteHandle,
                                      NETWORK_EXECUTE_IO_TYPE_STDERR,
                                      networkExecuteHandle->stderrBuffer.data,
                                      sizeof(networkExecuteHandle->stderrBuffer.data),
                                      timeout,
                                      &bytesRead
                                     );
          if (error != ERROR_NONE)
          {
            return error;
          }
    //fprintf(stderr,"%s,%d: bytesRead=%lu\n",__FILE__,__LINE__,bytesRead);

          networkExecuteHandle->stderrBuffer.index = 0;
          networkExecuteHandle->stderrBuffer.length = bytesRead;
        }

        /* check eol, append to line */
        if (networkExecuteHandle->stderrBuffer.index < networkExecuteHandle->stderrBuffer.length)
        {
          while (   !endOfLineFlag
                 && (networkExecuteHandle->stderrBuffer.index < networkExecuteHandle->stderrBuffer.length)
                )
          {
            if      (networkExecuteHandle->stderrBuffer.data[networkExecuteHandle->stderrBuffer.index] == '\n')
            {
              endOfLineFlag = TRUE;
            }
            else if (networkExecuteHandle->stderrBuffer.data[networkExecuteHandle->stderrBuffer.index] != '\r')
            {
              String_appendChar(line,networkExecuteHandle->stderrBuffer.data[networkExecuteHandle->stderrBuffer.index]);
            }
            networkExecuteHandle->stderrBuffer.index++;
          }
        }
        else
        {
          endOfLineFlag = TRUE;
        }
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
  }

  return ERROR_NONE;
}

void Network_executeFlush(NetworkExecuteHandle  *networkExecuteHandle,
                          NetworkExecuteIOTypes ioType
                         )
{
  assert(networkExecuteHandle != NULL);

  #ifdef HAVE_SSH2
    switch (ioType)
    {
      case NETWORK_EXECUTE_IO_TYPE_STDOUT:
        libssh2_channel_flush(networkExecuteHandle->channel);
        break;
      case NETWORK_EXECUTE_IO_TYPE_STDERR:
        libssh2_channel_flush_stderr(networkExecuteHandle->channel);
        break;
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(networkExecuteHandle);
    UNUSED_VARIABLE(ioType);
  #endif /* HAVE_SSH2 */
}

Errors Network_executeKeepAlive(NetworkExecuteHandle *networkExecuteHandle)
{
  #if defined(HAVE_SSH2_KEEPALIVE_SEND)
    int dummy;
  #endif

  assert(networkExecuteHandle != NULL);
  assert(networkExecuteHandle->socketHandle != NULL);

  #ifdef HAVE_SSH2
    #if defined(HAVE_SSH2_CHANNEL_SEND_KEEPALIVE)
      if (libssh2_channel_send_keepalive(networkExecuteHandle->channel) != 0)
      {
        return ERROR_NETWORK_SEND;
      }
    #elif defined(HAVE_SSH2_KEEPALIVE_SEND)
      if (libssh2_keepalive_send(networkExecuteHandle->socketHandle->ssh2.session,&dummy) != 0)
      {
        return ERROR_NETWORK_SEND;
      }
      UNUSED_VARIABLE(dummy);
    #else /* not HAVE_SSH2_CHANNEL_SEND_KEEPALIVE */
      UNUSED_VARIABLE(networkExecuteHandle);
    #endif /* HAVE_SSH2_CHANNEL_SEND_KEEPALIVE */
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(networkExecuteHandle);
  #endif /* HAVE_SSH2 */

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
