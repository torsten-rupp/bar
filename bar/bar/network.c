/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Network functions
* Systems: all
*
\***********************************************************************/

#define __NETWORK_IMPLEMENATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#ifdef HAVE_SYS_SOCKET_H
  #include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */
#ifdef HAVE_NETINET_IN_H
  #include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */
#ifdef HAVE_ARPA_INET_H
  #include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */
#ifdef HAVE_NETDB_H
  #include <netdb.h>
#endif /* HAVE_NETDB_H */
#include <fcntl.h>
#include <unistd.h>
#ifdef HAVE_SYS_IOCTL_H
  #include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */
#include <poll.h>
#include <signal.h>
#ifdef HAVE_SSH2
  #include <openssl/crypto.h>
  #include <libssh2.h>
#endif /* HAVE_SSH2 */
#ifdef HAVE_GNU_TLS
  #include <gnutls/gnutls.h>
  #include <gnutls/x509.h>
#endif /* HAVE_GNU_TLS */
#include <errno.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
  #include <linux/tcp.h>
#elif defined(PLATFORM_WINDOWS)
  #include <windows.h>
  #include <winsock2.h>
#endif /* PLATFORM_... */

#include "global.h"
#include "strings.h"
#include "files.h"
#include "misc.h"
#include "errors.h"

#include "bar.h"
#include "passwords.h"

#include "network.h"

/****************** Conditional compilation switches *******************/

#define _GNUTLS_DEBUG     // enable for GNU TLS debug output

/***************************** Constants *******************************/
#ifdef HAVE_GNU_TLS
  #define DH_BITS 1024
#else /* not HAVE_GNU_TLS */
#endif /* HAVE_GNU_TLS */

#define SEND_TIMEOUT 30000

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
#ifdef HAVE_SSH2
  LOCAL uint             cryptoMaxLocks;
  LOCAL pthread_mutex_t* cryptoLocks;
  LOCAL long*            cryptoLockCounters;
#endif /* HAVE_SSH2 */

#ifdef HAVE_GCRYPT
  GCRY_THREAD_OPTION_PTHREAD_IMPL;
#endif /* HAVE_GCRYPT */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifdef GNUTLS_DEBUG
LOCAL void gnuTLSLog(int level, const char *s)
{
  fprintf(stderr,"DEBUG GNU TLS %d: %s",level,s);
}
#endif /* GNUTLS_DEBUG */

#ifdef HAVE_SSH2
/***********************************************************************\
* Name   : cryptoIdCallback
* Purpose: libcrypto callback
* Input  : -
* Output : -
* Return : see openssl/crypto.h
* Notes  : -
\***********************************************************************/

LOCAL unsigned long cryptoIdCallback(void)
{
  #if  defined(PLATFORM_LINUX)
    return ((unsigned long)pthread_self());
  #elif defined(PLATFORM_WINDOWS)
    // nothing to return?
    return 0;
  #endif /* PLATFORM_... */
}

/***********************************************************************\
* Name   : cryptoLockingCallback
* Purpose: libcrypto callback
* Input  : mode     - see openssl/crypto.h
*          n        - see openssl/crypto.h
*          fileName - see openssl/crypto.h
*          lineNb   - see openssl/crypto.h
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void cryptoLockingCallback(int mode, int n, const char *fileName, int lineNb)
{
  UNUSED_VARIABLE(fileName);
  UNUSED_VARIABLE(lineNb);

  if      ((mode & CRYPTO_LOCK) != 0)
  {
    pthread_mutex_lock(&cryptoLocks[n]);
    cryptoLockCounters[n]++;
  }
  else if ((mode & CRYPTO_UNLOCK) != 0)
  {
    pthread_mutex_unlock(&cryptoLocks[n]);
  }
  else
  {
    HALT_INTERNAL_ERROR("unknown mode 0x%x in libcrypto locking callback",mode);
  }
}
#endif /* HAVE_SSH2 */

#ifdef HAVE_GNU_TLS

/***********************************************************************\
* Name   : validateCertificate
* Purpose: valciate SSL certificate
* Input  : cert       - TLS cerificate or NULL
*          certLength - TSL cerificate data length
* Output : -
* Return : ERROR_NONE if certificate is valid or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors validateCertificate(const void *certData,
                                 uint       certLength
                                )
{
  gnutls_x509_crt_t cert;
  gnutls_datum_t    datum;
  time_t            certActivationTime,certExpireTime;
  char              buffer[64];

  // check if certificate is valid
  if (gnutls_x509_crt_init(&cert) != GNUTLS_E_SUCCESS)
  {
    return ERROR_INVALID_TLS_CERTIFICATE;
  }
  datum.data = (void*)certData;
  datum.size = certLength;
  if (gnutls_x509_crt_import(cert,&datum,GNUTLS_X509_FMT_PEM) != GNUTLS_E_SUCCESS)
  {
    gnutls_x509_crt_deinit(cert);
    return ERROR_INVALID_TLS_CERTIFICATE;
  }
  certActivationTime = gnutls_x509_crt_get_activation_time(cert);
  if (certActivationTime != (time_t)(-1))
  {
    if (time(NULL) < certActivationTime)
    {
      gnutls_x509_crt_deinit(cert);
      return ERRORX_(TLS_CERTIFICATE_NOT_ACTIVE,0,"%s",Misc_formatDateTimeCString(buffer,sizeof(buffer),(uint64)certActivationTime,DATE_TIME_FORMAT_LOCALE));
    }
  }
  certExpireTime = gnutls_x509_crt_get_expiration_time(cert);
  if (certExpireTime != (time_t)(-1))
  {
    if (time(NULL) > certExpireTime)
    {
      gnutls_x509_crt_deinit(cert);
      return ERRORX_(TLS_CERTIFICATE_EXPIRED,0,"%s",Misc_formatDateTimeCString(buffer,sizeof(buffer),(uint64)certExpireTime,DATE_TIME_FORMAT_LOCALE));
    }
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
    return ERROR_INVALID_TLS_CA;
  }
#endif /* 0 */
  gnutls_x509_crt_deinit(cert);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : initSSL
* Purpose: init SSL encryption on socket
* Input  : socketHandle - socket handle
*          caData       - TLS CA data or NULL
*          caLength     - TSL CA data length
*          cert         - TLS cerificate or NULL
*          certLength   - TSL cerificate data length
*          key          - TLS key or NULL
*          keyLength    - TSL key data length
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors initSSL(SocketHandle *socketHandle,
                     const void   *caData,
                     uint         caLength,
                     const void   *certData,
                     uint         certLength,
                     const void   *keyData,
                     uint         keyLength
                    )
{
  gnutls_datum_t caDatum,certDatum,keyDatum;
  int            result;

  assert(socketHandle != NULL);
  assert(caData != NULL);
  assert(certData != NULL);
  assert(keyData != NULL);

//TODO
UNUSED_VARIABLE(caDatum);
UNUSED_VARIABLE(caData);
UNUSED_VARIABLE(caLength);

  // init certificate and key
  if (gnutls_certificate_allocate_credentials(&socketHandle->gnuTLS.credentials) != GNUTLS_E_SUCCESS)
  {
    return ERROR_INIT_TLS;
  }

  #ifdef GNUTLS_DEBUG
    fprintf(stderr,"DEBUG GNU TLS: CA: "); write(STDERR_FILENO,caData,caLength); fprintf(stderr,"\n");
    fprintf(stderr,"DEBUG GNU TLS: certificate: "); write(STDERR_FILENO,certData,certLength); fprintf(stderr,"\n");
    fprintf(stderr,"DEBUG GNU TLS: key:"); write(STDERR_FILENO,keyData,keyLength); fprintf(stderr,"\n");
  #endif /* GNUTLS_DEBUG */

  certDatum.data = (void*)certData;
  certDatum.size = certLength;
  keyDatum.data  = (void*)keyData;
  keyDatum.size  = keyLength;
  result = gnutls_certificate_set_x509_key_mem(socketHandle->gnuTLS.credentials,
                                               &certDatum,
                                               &keyDatum,
                                               GNUTLS_X509_FMT_PEM
                                              );
  if (result != GNUTLS_E_SUCCESS)
  {
    gnutls_certificate_free_credentials(socketHandle->gnuTLS.credentials);
    return ERROR_INVALID_TLS_CERTIFICATE;
  }

  gnutls_dh_params_init(&socketHandle->gnuTLS.dhParams);
  result = gnutls_dh_params_generate2(socketHandle->gnuTLS.dhParams,DH_BITS);
  if (result != GNUTLS_E_SUCCESS)
  {
    gnutls_dh_params_deinit(socketHandle->gnuTLS.dhParams);
    gnutls_certificate_free_credentials(socketHandle->gnuTLS.credentials);
    return ERROR_INIT_TLS;
  }
  gnutls_certificate_set_dh_params(socketHandle->gnuTLS.credentials,
                                   socketHandle->gnuTLS.dhParams
                                  );

  // initialize session
  if (gnutls_init(&socketHandle->gnuTLS.session,GNUTLS_SERVER) != 0)
  {
    return ERROR_INIT_TLS;
  }

  if (gnutls_set_default_priority(socketHandle->gnuTLS.session) != 0)
  {
    gnutls_deinit(socketHandle->gnuTLS.session);
    return ERROR_INIT_TLS;
  }

  if (gnutls_credentials_set(socketHandle->gnuTLS.session,
                             GNUTLS_CRD_CERTIFICATE,
                             socketHandle->gnuTLS.credentials
                            ) != 0
     )
  {
    gnutls_deinit(socketHandle->gnuTLS.session);
    gnutls_dh_params_deinit(socketHandle->gnuTLS.dhParams);
    gnutls_certificate_free_credentials(socketHandle->gnuTLS.credentials);
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

  // do handshake
  gnutls_transport_set_int(socketHandle->gnuTLS.session, socketHandle->handle);
  do
  {
    result = gnutls_handshake(socketHandle->gnuTLS.session);
  }
  while ((result < 0) && gnutls_error_is_fatal(result) == 0);
  if (result != GNUTLS_E_SUCCESS)
  {
    gnutls_deinit(socketHandle->gnuTLS.session);
    gnutls_dh_params_deinit(socketHandle->gnuTLS.dhParams);
    gnutls_certificate_free_credentials(socketHandle->gnuTLS.credentials);
    return ERRORX_(TLS_HANDSHAKE,result,"%s",gnutls_strerror(result));
  }

#if 0
NYI: how to enable client authentication?
  result = gnutls_certificate_verify_peers2(socketHandle->gnuTLS.session,&status);
  if (result != GNUTLS_E_SUCCESS)
  {
    gnutls_deinit(socketHandle->gnuTLS.session);
    gnutls_dh_params_deinit(socketHandle->gnuTLS.dhParams);
    gnutls_certificate_free_credentials(socketHandle->gnuTLS.credentials);
    return ERRORX_(TLS_HANDSHAKE,result,"%s",gnutls_strerror(result));
  }
#endif /* 0 */

  return ERROR_NONE;
}
#endif /* HAVE_GNU_TLS */

// ----------------------------------------------------------------------

Errors Network_initAll(void)
{
  #ifdef HAVE_SSH2
    uint i;
  #endif /* HAVE_SSH2 */

  #ifdef HAVE_SSH2
    // initialize crypto multi-thread support
    cryptoMaxLocks = (uint)CRYPTO_num_locks();
    cryptoLocks = (pthread_mutex_t*)OPENSSL_malloc(cryptoMaxLocks*sizeof(pthread_mutex_t));
    if (cryptoLocks == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    cryptoLockCounters = (long*)OPENSSL_malloc(cryptoMaxLocks*sizeof(long));
    if (cryptoLockCounters == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    for (i = 0; i < cryptoMaxLocks; i++)
    {
      pthread_mutex_init(&cryptoLocks[i],NULL);
      cryptoLockCounters[i] = 0L;
    }
    CRYPTO_set_id_callback(cryptoIdCallback);
    CRYPTO_set_locking_callback(cryptoLockingCallback);
  #else /* not HAVE_SSH2 */
  #endif /* HAVE_SSH2 */

  #ifdef HAVE_GCRYPT
    gcry_control(GCRYCTL_SET_THREAD_CBS,&gcry_threads_pthread);
  #endif /* HAVE_GCRYPT */
  #ifdef HAVE_GNU_TLS
    gnutls_global_init();
    #ifdef GNUTLS_DEBUG
      gnutls_global_set_log_level(10);
      gnutls_global_set_log_function(gnuTLSLog);
    #endif /* GNUTLS_DEBUG */
  #endif /* HAVE_GNU_TLS */

  #ifdef HAVE_SIGPIP
    /* ignore SIGPIPE which may triggered in some socket read/write
       operations if the socket is closed
    */
    signal(SIGPIPE,SIG_IGN);
  #endif

  return ERROR_NONE;
}

void Network_doneAll(void)
{
  #ifdef HAVE_SSH2
    uint i;
  #endif /* HAVE_SSH2 */

  #ifdef HAVE_GNU_TLS
    gnutls_global_deinit();
  #endif /* HAVE_GNU_TLS */
  #ifdef HAVE_SSH2
    CRYPTO_set_locking_callback(NULL);
    CRYPTO_set_id_callback(NULL);
    for (i = 0; i < cryptoMaxLocks; i++)
    {
      pthread_mutex_destroy(&cryptoLocks[i]);
    }
    OPENSSL_free(cryptoLockCounters);
    OPENSSL_free(cryptoLocks);
  #else /* not HAVE_SSH2 */
  #endif /* HAVE_SSH2 */
}

String Network_getHostName(String hostName)
{
  char buffer[256];

  assert(hostName != NULL);

  String_clear(hostName);

  if (gethostname(buffer,sizeof(buffer)) == 0)
  {
    buffer[sizeof(buffer)-1] = '\0';
    String_setCString(hostName,buffer);
  }

  return hostName;
}

bool Network_hostExists(ConstString hostName)
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
  #endif /* HAVE_GETHOSTBYNAME_R */

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
  #else /* not HAVE_GETHOSTBYNAME... */
    return FALSE;
  #endif /* HAVE_GETHOSTBYNAME... */
}

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
                      )
{
  #if   defined(HAVE_GETHOSTBYNAME_R)
    char           buffer[512];
    struct hostent bufferAddressEntry;
    int            getHostByNameError;
  #elif defined(HAVE_GETHOSTBYNAME)
  #endif /* HAVE_GETHOSTBYNAME* */
  struct hostent     *hostAddressEntry;
  #ifdef PLATFORM_LINUX
    in_addr_t          ipAddress;
  #else /* not PLATFORM_LINUX */
    unsigned long      ipAddress;
  #endif /* PLATFORM_LINUX */
  struct sockaddr_in socketAddress;
  #ifdef HAVE_SSH2
    int                ssh2Error;
    char               *ssh2ErrorText;
  #endif /* HAVE_SSH2 */
  Errors             error;

  assert(socketHandle != NULL);
  assert(hostName != NULL);

  // initialize variables
  socketHandle->type        = socketType;
  socketHandle->flags       = flags;
  socketHandle->isConnected = FALSE;

  switch (socketType)
  {
    case SOCKET_TYPE_PLAIN:
      {
        #if  defined(PLATFORM_LINUX)
          long   socketFlags;
          int    n;
        #elif defined(PLATFORM_WINDOWS)
          u_long n;
        #endif /* PLATFORM_... */

        // get host IP address
        #if   defined(HAVE_GETHOSTBYNAME_R)
          if (gethostbyname_r(String_cString(hostName),
                              &bufferAddressEntry,
                              buffer,
                              sizeof(buffer),
                              &hostAddressEntry,
                              &getHostByNameError
                             ) != 0)

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
          #ifdef PLATFORM_LINUX
            ipAddress = (*((in_addr_t*)hostAddressEntry->h_addr_list[0]));
          #else /* not PLATFORM_LINUX */
            ipAddress = (*((unsigned long*)hostAddressEntry->h_addr_list[0]));
          #endif /* PLATFORM_LINUX */
        }
        else
        {
          ipAddress = inet_addr(String_cString(hostName));
        }
        if (ipAddress == INADDR_NONE)
        {
          return ERRORX_(HOST_NOT_FOUND,0,"%s",String_cString(hostName));
        }

        // connect
        socketHandle->handle = socket(AF_INET,SOCK_STREAM,0);
        if (socketHandle->handle == -1)
        {
          return ERROR_(CONNECT_FAIL,errno);
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
          error = ERROR_(CONNECT_FAIL,errno);
          close(socketHandle->handle);
          return error;
        }

        if (flags != SOCKET_FLAG_NONE)
        {
          // enable non-blocking
          #if  defined(PLATFORM_LINUX)
            if ((flags & SOCKET_FLAG_NON_BLOCKING) != 0)
            {
              socketFlags = fcntl(socketHandle->handle,F_GETFL,0);
              fcntl(socketHandle->handle,F_SETFL,socketFlags = O_NONBLOCK);
            }
            if ((flags & SOCKET_FLAG_KEEP_ALIVE  ) != 0)
            {
              n = 1;
              setsockopt(socketHandle->handle,SOL_SOCKET,SO_KEEPALIVE,(void*)&n,sizeof(int));
            }
          #elif defined(PLATFORM_WINDOWS)
            if ((flags & SOCKET_FLAG_NON_BLOCKING) != 0)
            {
              n = 1;
              ioctlsocket(socketHandle->handle,FIONBIO,&n);
            }
            if ((flags & SOCKET_FLAG_KEEP_ALIVE  ) != 0)
            {
              n = 1;
              setsockopt(socketHandle->handle,SOL_SOCKET,SO_KEEPALIVE,(char*)&n,sizeof(int));
            }
          #endif /* PLATFORM_... */
        }
      }
      break;
    case SOCKET_TYPE_TLS:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case SOCKET_TYPE_SSH:
      #ifdef HAVE_SSH2
      {
        const char *plainPassword;
        #if  defined(PLATFORM_LINUX)
          long       socketFlags;
          int        n;
        #elif defined(PLATFORM_WINDOWS)
          u_long     n;
        #endif /* PLATFORM_... */
        int result;

        assert(loginName != NULL);
        assert(sshPublicKeyData != NULL);
        assert(sshPrivateKeyData != NULL);

        // initialize variables

        // get host IP address
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
          #ifdef PLATFORM_LINUX
            ipAddress = (*((in_addr_t*)hostAddressEntry->h_addr_list[0]));
          #else /* not PLATFORM_LINUX */
            ipAddress = (*((unsigned long*)hostAddressEntry->h_addr_list[0]));
          #endif /* PLATFORM_LINUX */
        }
        else
        {
          ipAddress = inet_addr(String_cString(hostName));
        }
        if (ipAddress == INADDR_NONE)
        {
          return ERRORX_(HOST_NOT_FOUND,0,"%s",String_cString(hostName));
        }

        // check login name
        if (String_isEmpty(loginName))
        {
          return ERROR_NO_LOGIN_NAME;
        }

        // connect
        socketHandle->handle = socket(AF_INET,SOCK_STREAM,0);
        if (socketHandle->handle == -1)
        {
          return ERROR_(CONNECT_FAIL,errno);
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
          error = ERRORX_(CONNECT_FAIL,errno,"%s",strerror(errno));
          close(socketHandle->handle);
          return error;
        }

        // init SSL session
        socketHandle->ssh2.session = libssh2_session_init();
        if (socketHandle->ssh2.session == NULL)
        {
          close(socketHandle->handle);
          return ERROR_SSH_SESSION_FAIL;
        }
        if      (globalOptions.verboseLevel >= 6) libssh2_trace(socketHandle->ssh2.session,
                                                                  LIBSSH2_TRACE_SOCKET
                                                                | LIBSSH2_TRACE_TRANS
                                                                | LIBSSH2_TRACE_KEX
                                                                | LIBSSH2_TRACE_AUTH
                                                                | LIBSSH2_TRACE_CONN
                                                                | LIBSSH2_TRACE_SCP
                                                                | LIBSSH2_TRACE_SFTP
                                                                | LIBSSH2_TRACE_ERROR
                                                                | LIBSSH2_TRACE_PUBLICKEY
                                                               );
        else if (globalOptions.verboseLevel >= 5) libssh2_trace(socketHandle->ssh2.session,
                                                                  LIBSSH2_TRACE_KEX
                                                                | LIBSSH2_TRACE_AUTH
                                                                | LIBSSH2_TRACE_SCP
                                                                | LIBSSH2_TRACE_SFTP
                                                                | LIBSSH2_TRACE_ERROR
                                                                | LIBSSH2_TRACE_PUBLICKEY
                                                               );
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
        // authorize with key
        plainPassword = Password_deploy(password);
        result = libssh2_userauth_publickey_frommemory(socketHandle->ssh2.session,
                                                       String_cString(loginName),
                                                       String_length(loginName),
                                                       sshPublicKeyData,
                                                       sshPublicKeyLength,
                                                       sshPrivateKeyData,
                                                       sshPrivateKeyLength,
                                                       plainPassword
                                                      );
        if (result != 0)
        {
          ssh2Error = libssh2_session_last_error(socketHandle->ssh2.session,&ssh2ErrorText,NULL,0);
          // Note: work-around for missleading error message from libssh2: original error (-16) is overwritten by callback-error (-19) in libssh2.
          if (ssh2Error == LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED)
          {
            error = ERRORX_(SSH_AUTHENTICATION,ssh2Error,"Unable to initialize private key from file");
          }
          else
          {
            error = ERRORX_(SSH_AUTHENTICATION,ssh2Error,"%s",ssh2ErrorText);
          }
          Password_undeploy(password,plainPassword);
          libssh2_session_disconnect(socketHandle->ssh2.session,"");
          libssh2_session_free(socketHandle->ssh2.session);
          close(socketHandle->handle);
          return error;
        }
        Password_undeploy(password,plainPassword);
#else
        // authorize interactive
        if (libssh2_userauth_keyboard_interactive(socketHandle->ssh2.session,
                                                  String_cString(loginName),
                                                  NULL
                                                ) != 0
           )
        {
          ssh2Error = libssh2_session_last_error(socketHandle->ssh2.session,&ssh2ErrorText,NULL,0);
          // Note: work-around for missleading error message from libssh2: original error (-16) is overwritten by callback-error (-19) in libssh2.
          if (ssh2Error == LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED)
          {
            error = ERRORX_(SSH_AUTHENTICATION,ssh2Error,"Unable to initialize private key");
          }
          else
          {
            error = ERRORX_(SSH_AUTHENTICATION,ssh2Error,"%s",ssh2ErrorText);
          }
          libssh2_session_disconnect(socketHandle->ssh2.session,"");
          libssh2_session_free(socketHandle->ssh2.session);
          close(socketHandle->handle);
          return error;
        }
#endif /* 0 */
        if (flags != SOCKET_FLAG_NONE)
        {
          // enable non-blocking
          #if  defined(PLATFORM_LINUX)
            if ((flags & SOCKET_FLAG_NON_BLOCKING) != 0)
            {
              socketFlags = fcntl(socketHandle->handle,F_GETFL,0);
              fcntl(socketHandle->handle,F_SETFL,socketFlags | O_NONBLOCK);
            }
            if ((flags & SOCKET_FLAG_KEEP_ALIVE  ) != 0)
            {
              n = 1;
              setsockopt(socketHandle->handle,SOL_SOCKET,SO_KEEPALIVE,(void*)&n,sizeof(int));
            }
          #elif defined(PLATFORM_WINDOWS)
            if ((flags & SOCKET_FLAG_NON_BLOCKING) != 0)
            {
              n = 1;
              ioctlsocket(socketHandle->handle,FIONBIO,&n);
            }
            if ((flags & SOCKET_FLAG_KEEP_ALIVE  ) != 0)
            {
              n = 1;
              setsockopt(socketHandle->handle,SOL_SOCKET,SO_KEEPALIVE,(char*)&n,sizeof(int));
            }
          #endif /* PLATFORM_... */
        }
      }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(loginName);
        UNUSED_VARIABLE(password);
        UNUSED_VARIABLE(sshPublicKeyData);
        UNUSED_VARIABLE(sshPublicKeyLength);
        UNUSED_VARIABLE(sshPrivateKeyData);
        UNUSED_VARIABLE(sshPrivateKeyLength);

        close(socketHandle->handle);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  socketHandle->isConnected = TRUE;

  return ERROR_NONE;
}

void Network_disconnect(SocketHandle *socketHandle)
{
  assert(socketHandle != NULL);
  assert(socketHandle->handle >= 0);

  socketHandle->isConnected = TRUE;
  switch (socketHandle->type)
  {
    case SOCKET_TYPE_PLAIN:
      break;
    case SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
        gnutls_deinit(socketHandle->gnuTLS.session);
        gnutls_dh_params_deinit(socketHandle->gnuTLS.dhParams);
        gnutls_certificate_free_credentials(socketHandle->gnuTLS.credentials);
      #else /* not HAVE_GNU_TLS */
      #endif /* HAVE_GNU_TLS */
      break;
    case SOCKET_TYPE_SSH:
      #ifdef HAVE_SSH2
        libssh2_session_disconnect(socketHandle->ssh2.session,"");
        libssh2_session_free(socketHandle->ssh2.session);
        Misc_udelay(1000*1000);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  close(socketHandle->handle);
}

bool Network_eof(SocketHandle *socketHandle)
{
  bool eofFlag;

  assert(socketHandle != NULL);

  eofFlag = TRUE;
  switch (socketHandle->type)
  {
    case SOCKET_TYPE_PLAIN:
      {
        #if  defined(PLATFORM_LINUX)
          int    n;
        #elif defined(PLATFORM_WINDOWS)
          u_long n;
        #endif /* PLATFORM_... */

        #if  defined(PLATFORM_LINUX)
          eofFlag = (ioctl(socketHandle->handle,FIONREAD,&n,sizeof(int)) != 0) || (n == 0);
        #elif defined(PLATFORM_WINDOWS)
          eofFlag = (ioctlsocket(socketHandle->handle,FIONREAD,&n) != 0) || (n == 0);
        #endif /* PLATFORM_... */
      }
      break;
    case SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_GNU_TLS */
      #endif /* HAVE_GNU_TLS */
      break;
    case SOCKET_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
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
        #if  defined(PLATFORM_LINUX)
          int    n;
        #elif defined(PLATFORM_WINDOWS)
          u_long n;
        #endif /* PLATFORM_... */

        #if  defined(PLATFORM_LINUX)
          if (ioctl(socketHandle->handle,FIONREAD,&n,sizeof(int)) == 0)
          {
            bytesAvailable = (ulong)n;
          }
        #elif defined(PLATFORM_WINDOWS)
          if (ioctlsocket(socketHandle->handle,FIONREAD,&n) == 0)
          {
            bytesAvailable = (ulong)n;
          }
        #endif /* PLATFORM_... */
      }
      break;
    case SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_GNU_TLS */
      #endif /* HAVE_GNU_TLS */
      break;
    case SOCKET_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
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
  sigset_t        signalMask;
  struct timespec pollTimeout;
  struct pollfd   pollfds[1];
  long            n;

  assert(socketHandle != NULL);
  assert(bytesReceived != NULL);

  n = -1L;
  switch (socketHandle->type)
  {
    case SOCKET_TYPE_PLAIN:
      if (timeout == WAIT_FOREVER)
      {
        // receive
        n = recv(socketHandle->handle,buffer,maxLength,0);
      }
      else
      {
        // Note: ignore SIGALRM in ppoll()
        sigemptyset(&signalMask);
        sigaddset(&signalMask,SIGALRM);

        // wait for data
        pollTimeout.tv_sec  = timeout/1000L;
        pollTimeout.tv_nsec = (timeout%1000L)*1000000L;
        pollfds[0].fd     = socketHandle->handle;
        pollfds[0].events = POLLIN|POLLERR|POLLNVAL;
        if (   (ppoll(pollfds,1,&pollTimeout,&signalMask) > 0)
            || ((pollfds[0].revents & (POLLERR|POLLNVAL)) == 0)
           )
        {
          // receive
          n = recv(socketHandle->handle,buffer,maxLength,0);

          // check if disconected
          socketHandle->isConnected = (n > 0);
        }
        else
        {
          socketHandle->isConnected = FALSE;
        }
      }
      break;
    case SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
        if (timeout == WAIT_FOREVER)
        {
          // receive
          n = gnutls_record_recv(socketHandle->gnuTLS.session,buffer,maxLength);
        }
        else
        {
          // Note: ignore SIGALRM in ppoll()
          sigemptyset(&signalMask);
          sigaddset(&signalMask,SIGALRM);

          // wait for data
          pollfds[0].fd     = socketHandle->handle;
          pollfds[0].events = POLLIN|POLLERR|POLLNVAL;
          if (   (ppoll(pollfds,1,&pollTimeout,&signalMask) > 0)
              && ((pollfds[0].revents & (POLLERR|POLLNVAL)) == 0)
             )
          {
            // receive
            n = gnutls_record_recv(socketHandle->gnuTLS.session,buffer,maxLength);

            // check if disconected
            socketHandle->isConnected = (n > 0);
          }
          else
          {
            // disconnected
            socketHandle->isConnected = FALSE;
          }
        }
      #else /* not HAVE_GNU_TLS */
      #endif /* HAVE_GNU_TLS */
      break;
    case SOCKET_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  (*bytesReceived) = (n >= 0) ? n : 0;

  return (n >= 0) ? ERROR_NONE : ERROR_NETWORK_RECEIVE;
}

Errors Network_send(SocketHandle *socketHandle,
                    const void   *buffer,
                    ulong        length
                   )
{
  ulong           sentBytes;
  sigset_t        signalMask;
  struct timespec pollTimeout;
  struct pollfd   pollfds[1];
  long            n;

  assert(socketHandle != NULL);

  sentBytes = 0L;
  if (length > 0)
  {
    switch (socketHandle->type)
    {
      case SOCKET_TYPE_PLAIN:
          do
          {
            assert(socketHandle->handle < FD_SETSIZE);

            // Note: ignore SIGALRM in ppoll()
            sigemptyset(&signalMask);
            sigaddset(&signalMask,SIGALRM);

            // wait until space in buffer is available
            pollTimeout.tv_sec  = SEND_TIMEOUT/1000L;
            pollTimeout.tv_nsec = (SEND_TIMEOUT%1000L)*1000000L;
            pollfds[0].fd     = socketHandle->handle;
            pollfds[0].events = POLLOUT|POLLERR|POLLNVAL;
            if (   (ppoll(pollfds,1,&pollTimeout,&signalMask) == -1)
                || ((pollfds[0].revents & (POLLERR|POLLNVAL)) != 0)
               )
            {
              break;
            }

            // send data
            n = send(socketHandle->handle,((byte*)buffer)+sentBytes,length-sentBytes,MSG_NOSIGNAL);
            if      (n > 0) sentBytes += n;
            else if ((n == -1) && (errno != EAGAIN)) break;
          }
          while (sentBytes < length);
        break;
      case SOCKET_TYPE_TLS:
        #ifdef HAVE_GNU_TLS
          do
          {
            assert(socketHandle->handle < FD_SETSIZE);

            // Note: ignore SIGALRM in ppoll()
            sigemptyset(&signalMask);
            sigaddset(&signalMask,SIGALRM);

            // wait until space in buffer is available
            pollTimeout.tv_sec  = SEND_TIMEOUT/1000L;
            pollTimeout.tv_nsec = (SEND_TIMEOUT%1000L)*1000000L;
            pollfds[0].fd     = socketHandle->handle;
            pollfds[0].events = POLLOUT|POLLERR|POLLNVAL;
            if (   (ppoll(pollfds,1,&pollTimeout,&signalMask) == -1)
                || ((pollfds[0].revents & (POLLERR|POLLNVAL)) != 0)
               )
            {
              break;
            }

            // send data
            n = gnutls_record_send(socketHandle->gnuTLS.session,((byte*)buffer)+sentBytes,length-sentBytes);
            if      (n > 0) sentBytes += n;
            else if ((n < 0) && (errno != GNUTLS_E_AGAIN)) break;
          }
          while (sentBytes < length);
        #else /* not HAVE_GNU_TLS */
          sentBytes = 0L;
        #endif /* HAVE_GNU_TLS */
        break;
      case SOCKET_TYPE_SSH:
        #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
        #else /* not HAVE_SSH2 */
          return ERROR_FUNCTION_NOT_SUPPORTED;
        #endif /* HAVE_SSH2 */
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
//  if (sentBytes != length) fprintf(stderr,"%s,%d: send error %d: %s\n",__FILE__,__LINE__,errno,strerror(errno));
  }

  return (sentBytes == length) ? ERROR_NONE : ERROR_NETWORK_SEND;
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
    // read character
    error = Network_receive(socketHandle,&ch,1,timeout,&bytesReceived);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // check eol, append to line
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
                         ConstString  line
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
                          ServerSocketTypes  serverSocketType,
                          const void         *caData,
                          uint               caLength,
                          const void         *certData,
                          uint               certLength,
                          const void         *keyData,
                          uint               keyLength
                         )
{
  struct sockaddr_in socketAddress;
  int                n;
  Errors             error;

  assert(serverSocketHandle != NULL);

  // init variables
  serverSocketHandle->socketType = serverSocketType;

  // create socket
  serverSocketHandle->handle = socket(AF_INET,SOCK_STREAM,0);
  if (serverSocketHandle->handle == -1)
  {
    return ERROR_(CONNECT_FAIL,errno);
  }

  // reuse address
  n = 1;
  if (setsockopt(serverSocketHandle->handle,SOL_SOCKET,SO_REUSEADDR,(void*)&n,sizeof(int)) != 0)
  {
    error = ERROR_(CONNECT_FAIL,errno);
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
    error = ERROR_(CONNECT_FAIL,errno);
    close(serverSocketHandle->handle);
    return error;
  }
  listen(serverSocketHandle->handle,5);

  switch (serverSocketType)
  {
    case SERVER_SOCKET_TYPE_PLAIN:
      break;
    case SERVER_SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
      {
        gnutls_x509_crt_t cert;
        gnutls_datum_t    datum;
        time_t            certActivationTime,certExpireTime;
        char              buffer[64];

        // check if all key files exists
        if (caData == NULL)
        {
          close(serverSocketHandle->handle);
          return ERROR_NO_TLS_CA;
        }
        if (certData == NULL)
        {
          close(serverSocketHandle->handle);
          return ERROR_NO_TLS_CERTIFICATE;
        }
        if (keyData == NULL)
        {
          close(serverSocketHandle->handle);
          return ERROR_NO_TLS_KEY;
        }

        // check if certificate is valid
        if (gnutls_x509_crt_init(&cert) != GNUTLS_E_SUCCESS)
        {
          close(serverSocketHandle->handle);
          return ERROR_INVALID_TLS_CERTIFICATE;
        }
        datum.data = (void*)certData;
        datum.size = certLength;
        if (gnutls_x509_crt_import(cert,&datum,GNUTLS_X509_FMT_PEM) != GNUTLS_E_SUCCESS)
        {
          gnutls_x509_crt_deinit(cert);
          close(serverSocketHandle->handle);
          return ERROR_INVALID_TLS_CERTIFICATE;
        }
        certActivationTime = gnutls_x509_crt_get_activation_time(cert);
        if (certActivationTime != (time_t)(-1))
        {
          if (time(NULL) < certActivationTime)
          {
            gnutls_x509_crt_deinit(cert);
            close(serverSocketHandle->handle);
            return ERRORX_(TLS_CERTIFICATE_NOT_ACTIVE,0,"%s",Misc_formatDateTimeCString(buffer,sizeof(buffer),(uint64)certActivationTime,DATE_TIME_FORMAT_LOCALE));
          }
        }
        certExpireTime = gnutls_x509_crt_get_expiration_time(cert);
        if (certExpireTime != (time_t)(-1))
        {
          if (time(NULL) > certExpireTime)
          {
            gnutls_x509_crt_deinit(cert);
            close(serverSocketHandle->handle);
            return ERRORX_(TLS_CERTIFICATE_EXPIRED,0,"%s",Misc_formatDateTimeCString(buffer,sizeof(buffer),(uint64)certExpireTime,DATE_TIME_FORMAT_LOCALE));
          }
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

        // store CA, certificate, key for connect requests
        serverSocketHandle->caData     = caData;
        serverSocketHandle->caLength   = caLength;
        serverSocketHandle->certData   = certData;
        serverSocketHandle->certLength = certLength;
        serverSocketHandle->keyData    = keyData;
        serverSocketHandle->keyLength  = keyLength;
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

  switch (serverSocketHandle->socketType)
  {
    case SERVER_SOCKET_TYPE_PLAIN:
      break;
    case SERVER_SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
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

Errors Network_startSSL(SocketHandle *socketHandle,
                        const void   *caData,
                        uint         caLength,
                        const void   *certData,
                        uint         certLength,
                        const void   *keyData,
                        uint         keyLength
                       )
{
  #ifdef HAVE_GNU_TLS
    #if  defined(PLATFORM_LINUX)
      long               socketFlags;
    #elif defined(PLATFORM_WINDOWS)
      u_long             n;
    #endif /* PLATFORM_... */
    Errors error;
  #endif /* HAVE_GNU_TLS */

  assert(socketHandle->type == SOCKET_TYPE_PLAIN);

  #ifdef HAVE_GNU_TLS
    // check if all key files exists
    if (caData == NULL)
    {
      return ERROR_NO_TLS_CA;
    }
    if (certData == NULL)
    {
      return ERROR_NO_TLS_CERTIFICATE;
    }
    if (keyData == NULL)
    {
      return ERROR_NO_TLS_KEY;
    }

    // check if certificate is valid
    error = validateCertificate(certData,certLength);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // temporary disable non-blocking
    if ((socketHandle->flags & SOCKET_FLAG_NON_BLOCKING) !=  0)
    {
      #if  defined(PLATFORM_LINUX)
        socketFlags = fcntl(socketHandle->handle,F_GETFL,0);
        fcntl(socketHandle->handle,F_SETFL,socketFlags & ~O_NONBLOCK);
      #elif defined(PLATFORM_WINDOWS)
        n = 0;
        ioctlsocket(socketHandle->handle,FIONBIO,&n);
      #endif /* PLATFORM_... */
    }

    // init SSL
    error = initSSL(socketHandle,caData,caLength,certData,certLength,keyData,keyLength);
    if (error == ERROR_NONE)
    {
      socketHandle->type = SOCKET_TYPE_TLS;
    }

    // re-enable temporary non-blocking
    if ((socketHandle->flags & SOCKET_FLAG_NON_BLOCKING) !=  0)
    {
      #if  defined(PLATFORM_LINUX)
        socketFlags = fcntl(socketHandle->handle,F_GETFL,0);
        fcntl(socketHandle->handle,F_SETFL,socketFlags | O_NONBLOCK);
      #elif defined(PLATFORM_WINDOWS)
        n = 1;
        ioctlsocket(socketHandle->handle,FIONBIO,&n);
      #endif /* PLATFORM_... */
    }

    return ERROR_NONE;
  #else /* not HAVE_GNU_TLS */
    UNUSED_VARIABLE(socketHandle);
    UNUSED_VARIABLE(caData);
    UNUSED_VARIABLE(caLength);
    UNUSED_VARIABLE(certData);
    UNUSED_VARIABLE(certLength);
    UNUSED_VARIABLE(keyData);
    UNUSED_VARIABLE(keyLength);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_GNU_TLS */
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
  #if   defined(PLATFORM_LINUX)
    socklen_t          socketAddressLength;
  #elif defined(PLATFORM_WINDOWS)
    int                socketAddressLength;
  #endif /* PLATFORM_... */
  Errors             error;
  #if  defined(PLATFORM_LINUX)
    long               socketFlags;
  #elif defined(PLATFORM_WINDOWS)
    u_long             n;
  #endif /* PLATFORM_... */

//unsigned int status;

  assert(socketHandle != NULL);
  assert(serverSocketHandle != NULL);

  // init variables
  socketHandle->flags = flags;

  // accept
  socketAddressLength = sizeof(socketAddress);
  socketHandle->handle = accept(serverSocketHandle->handle,
                                (struct sockaddr*)&socketAddress,
                                &socketAddressLength
                               );
  if (socketHandle->handle == -1)
  {
    error = ERROR_(CONNECT_FAIL,errno);
    close(socketHandle->handle);
    return error;
  }

  switch (serverSocketHandle->socketType)
  {
    case SERVER_SOCKET_TYPE_PLAIN:
      socketHandle->type = SOCKET_TYPE_PLAIN;
      break;
    case SERVER_SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
fprintf(stderr,"%s, %d: call initSSL\n",__FILE__,__LINE__);
        // init SSL
        error = initSSL(socketHandle,
                        serverSocketHandle->caData,
                        serverSocketHandle->caLength,
                        serverSocketHandle->certData,
                        serverSocketHandle->certLength,
                        serverSocketHandle->keyData,
                        serverSocketHandle->keyLength
                       );
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
asm("int3");
        if (error != ERROR_NONE)
        {
          return error;
        }

        socketHandle->type = SOCKET_TYPE_TLS;
      #else /* not HAVE_GNU_TLS */
        UNUSED_VARIABLE(socketHandle);
        UNUSED_VARIABLE(serverSocketHandle);
        UNUSED_VARIABLE(flags);

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
    // enable non-blocking
    #if  defined(PLATFORM_LINUX)
      socketFlags = fcntl(socketHandle->handle,F_GETFL,0);
      fcntl(socketHandle->handle,F_SETFL,socketFlags | O_NONBLOCK);
    #elif defined(PLATFORM_WINDOWS)
      n = 1;
      ioctlsocket(socketHandle->handle,FIONBIO,&n);
    #endif /* PLATFORM_... */
  }

  return ERROR_NONE;
}

void Network_getLocalInfo(SocketHandle *socketHandle,
                          String       name,
                          uint         *port
                         )
{
  struct sockaddr_in   socketAddress;
  #if   defined(PLATFORM_LINUX)
    socklen_t            socketAddressLength;
  #elif defined(PLATFORM_WINDOWS)
    int                  socketAddressLength;
  #endif /* PLATFORM_... */
  #ifdef HAVE_GETHOSTBYADDR
    const struct hostent *hostEntry;
  #endif

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
      hostEntry = gethostbyaddr((const char*)&socketAddress.sin_addr,
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
  #if   defined(PLATFORM_LINUX)
    socklen_t            socketAddressLength;
  #elif defined(PLATFORM_WINDOWS)
    int                  socketAddressLength;
  #endif /* PLATFORM_... */
  #ifdef HAVE_GETHOSTBYADDR_R
    const struct hostent *hostEntry;
  #endif

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
  #ifdef HAVE_SSH2
    #if  defined(PLATFORM_LINUX)
      long   socketFlags;
    #elif defined(PLATFORM_WINDOWS)
      u_long n;
    #endif /* PLATFORM_... */
  #endif /* HAVE_SSH2 */

  assert(networkExecuteHandle != NULL);
  assert(socketHandle != NULL);
  assert(socketHandle->type == SOCKET_TYPE_SSH);
  assert(command != NULL);

  // initialize variables
  networkExecuteHandle->socketHandle        = socketHandle;
  networkExecuteHandle->stdoutBuffer.index  = 0;
  networkExecuteHandle->stdoutBuffer.length = 0;
  networkExecuteHandle->stderrBuffer.index  = 0;
  networkExecuteHandle->stderrBuffer.length = 0;

  #ifdef HAVE_SSH2
    // open channel
    networkExecuteHandle->channel = libssh2_channel_open_session(socketHandle->ssh2.session);
    if (networkExecuteHandle->channel == NULL)
    {
      return ERROR_NETWORK_EXECUTE_FAIL;
    }

    // execute command
    if (libssh2_channel_exec(networkExecuteHandle->channel,
                             command
                            ) != 0
       )
    {
      libssh2_channel_close(networkExecuteHandle->channel);
      libssh2_channel_wait_closed(networkExecuteHandle->channel);
      return ERROR_NETWORK_EXECUTE_FAIL;
    }

    // enable non-blocking
    #if  defined(PLATFORM_LINUX)
      socketFlags = fcntl(socketHandle->handle,F_GETFL,0);
      fcntl(socketHandle->handle,F_SETFL,socketFlags | O_NONBLOCK);
    #elif defined(PLATFORM_WINDOWS)
      n = 1;
      ioctlsocket(socketHandle->handle,FIONBIO,&n);
    #endif /* PLATFORM_... */
    libssh2_channel_set_blocking(networkExecuteHandle->channel,0);

    // disable stderr if not requested
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

  return (sentBytes == (long)length) ? ERROR_NONE : ERROR_NETWORK_SEND;
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

  (*bytesRead) = (n >= 0) ? n : 0;

  return (n >= 0) ? ERROR_NONE : ERROR_NETWORK_RECEIVE;
}

Errors Network_executeWriteLine(NetworkExecuteHandle *networkExecuteHandle,
                                ConstString          line
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
          // read character
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

        // check eol, append to line
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
          // read character
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

        // check eol, append to line
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
  #if defined(HAVE_SSH2) && defined(HAVE_SSH2_KEEPALIVE_SEND)
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
