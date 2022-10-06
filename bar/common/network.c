/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Network functions
* Systems: all
*
\***********************************************************************/

#define __NETWORK_IMPLEMENTATION__

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
  #include <winsock2.h>
  #include <windows.h>
  #include <in6addr.h>
#endif /* PLATFORM_... */

#include "common/global.h"
#include "common/strings.h"
#include "common/files.h"
#include "common/misc.h"
#include "common/passwords.h"

#include "errors.h"

#include "network.h"

/****************** Conditional compilation switches *******************/

#define _GNUTLS_DEBUG     // enable for GNU TLS debug output

/***************************** Constants *******************************/
#ifdef HAVE_GNU_TLS
  #define DH_BITS 1024
#else /* not HAVE_GNU_TLS */
#endif /* HAVE_GNU_TLS */

#define SEND_TIMEOUT 30000

#if   defined(PLATFORM_LINUX)
  #define SHUTDOWN_FLAGS SHUT_RDWR
#elif defined(PLATFORM_WINDOWS)
  #define SHUTDOWN_FLAGS SD_BOTH
#endif /* PLATFORM_... */

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
#ifdef HAVE_SSH2
  LOCAL uint             cryptoMaxLocks;
  LOCAL pthread_mutex_t* cryptoLocks;
  LOCAL long*            cryptoLockCounters;
#endif /* HAVE_SSH2 */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : disconnectDescriptor
* Purpose: disconnect socket descriptor
* Input  : socketDescriptor - socket descriptor
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void disconnectDescriptor(int socketDescriptor)
{
  assert(socketDescriptor >= 0);

  shutdown(socketDescriptor,SHUTDOWN_FLAGS);
  close(socketDescriptor);
}

/***********************************************************************\
* Name   : disconnect
* Purpose: disconnect socket
* Input  : socketHandle - socket handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void disconnect(SocketHandle *socketHandle)
{
  assert(socketHandle != NULL);
  assert(socketHandle->handle >= 0);

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
        Misc_udelay(US_PER_S);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  if (socketHandle->isConnected)
  {
    disconnectDescriptor(socketHandle->handle);
  }
  socketHandle->isConnected = FALSE;
}

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
      return ERRORX_(TLS_CERTIFICATE_NOT_ACTIVE,0,"%s",Misc_formatDateTimeCString(buffer,sizeof(buffer),(uint64)certActivationTime,FALSE,DATE_TIME_FORMAT_LOCALE));
    }
  }
  certExpireTime = gnutls_x509_crt_get_expiration_time(cert);
  if (certExpireTime != (time_t)(-1))
  {
    if (time(NULL) > certExpireTime)
    {
      gnutls_x509_crt_deinit(cert);
      return ERRORX_(TLS_CERTIFICATE_EXPIRED,0,"%s",Misc_formatDateTimeCString(buffer,sizeof(buffer),(uint64)certExpireTime,FALSE,DATE_TIME_FORMAT_LOCALE));
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
*          tlsType      - TLS type; see NETWORK_TLS_TYPE_...
*          caData       - TLS CA data or NULL (PEM encoded)
*          caLength     - TSL CA data length
*          cert         - TLS cerificate or NULL (PEM encoded)
*          certLength   - TSL cerificate data length
*          key          - TLS key or NULL (PEM encoded)
*          keyLength    - TSL key data length
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors initSSL(SocketHandle    *socketHandle,
                     NetworkTLSTypes tlsType,
                     const void      *caData,
                     uint            caLength,
                     const void      *certData,
                     uint            certLength,
                     const void      *keyData,
                     uint            keyLength
                    )
{
  gnutls_datum_t caDatum,certDatum,keyDatum;
  int            result;

  assert(socketHandle != NULL);
  assert(caData != NULL);
  assert(caLength > 0);
  assert(certData != NULL);
  assert(certLength > 0);
  assert(keyData != NULL);
  assert(keyLength > 0);

//TODO
UNUSED_VARIABLE(caDatum);
UNUSED_VARIABLE(caData);
UNUSED_VARIABLE(caLength);

  // init certificate and key
  result = gnutls_certificate_allocate_credentials(&socketHandle->gnuTLS.credentials);
  if (result != GNUTLS_E_SUCCESS)
  {
    return ERROR_INIT_TLS;
  }

  #ifdef GNUTLS_DEBUG
    fprintf(stderr,"DEBUG GNU TLS: CA:\n"); write(STDERR_FILENO,caData,caLength); fprintf(stderr,"\n");
    fprintf(stderr,"DEBUG GNU TLS: certificate:\n"); write(STDERR_FILENO,certData,certLength); fprintf(stderr,"\n");
    fprintf(stderr,"DEBUG GNU TLS: key %d:\n",keyLength); write(STDERR_FILENO,keyData,keyLength); fprintf(stderr,"\n");
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
  switch (tlsType)
  {
    case NETWORK_TLS_TYPE_SERVER: result = gnutls_init(&socketHandle->gnuTLS.session,GNUTLS_SERVER); break;
    case NETWORK_TLS_TYPE_CLIENT: result = gnutls_init(&socketHandle->gnuTLS.session,GNUTLS_CLIENT); break;
  }
  if (result != GNUTLS_E_SUCCESS)
  {
    gnutls_dh_params_deinit(socketHandle->gnuTLS.dhParams);
    gnutls_certificate_free_credentials(socketHandle->gnuTLS.credentials);
    return ERROR_INIT_TLS;
  }

  result = gnutls_set_default_priority(socketHandle->gnuTLS.session);
  if (result != GNUTLS_E_SUCCESS)
  {
    gnutls_deinit(socketHandle->gnuTLS.session);
    gnutls_dh_params_deinit(socketHandle->gnuTLS.dhParams);
    gnutls_certificate_free_credentials(socketHandle->gnuTLS.credentials);
    return ERROR_INIT_TLS;
  }

  result = gnutls_credentials_set(socketHandle->gnuTLS.session,
                                  GNUTLS_CRD_CERTIFICATE,
                                  socketHandle->gnuTLS.credentials
                                 );
  if (result != GNUTLS_E_SUCCESS)
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
  gnutls_transport_set_int(socketHandle->gnuTLS.session,socketHandle->handle);
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
    // Note: avoid warning: CRYPTO_set_id_callback() may be empty?
    (void)cryptoIdCallback;
    CRYPTO_set_id_callback(cryptoIdCallback);
    // Note: avoid warning: CRYPTO_set_locking_callback() may be empty?
    (void)cryptoLockingCallback;
    CRYPTO_set_locking_callback(cryptoLockingCallback);
  #else /* not HAVE_SSH2 */
  #endif /* HAVE_SSH2 */

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
                       const void   *caData,
                       uint         caLength,
                       const void   *certData,
                       uint         certLength,
                       const void   *publicKeyData,
                       uint         publicKeyLength,
                       const void   *privateKeyData,
                       uint         privateKeyLength,
                       SocketFlags  socketFlags
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
  int                socketDescriptor;
  Errors             error;

  assert(socketHandle != NULL);
  assert(hostName != NULL);

  socketDescriptor = -1;
  switch (socketType)
  {
    case SOCKET_TYPE_PLAIN:
      {
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
        socketDescriptor = socket(AF_INET,SOCK_STREAM,0);
        if (socketDescriptor == -1)
        {
          return ERRORX_(CONNECT_FAIL,errno,"%E",errno);
        }
        socketAddress.sin_family      = AF_INET;
        socketAddress.sin_addr.s_addr = ipAddress;
        socketAddress.sin_port        = htons(hostPort);
        if (connect(socketDescriptor,
                    (struct sockaddr*)&socketAddress,
                    sizeof(socketAddress)
                   ) != 0
           )
        {
          error = ERRORX_(CONNECT_FAIL,errno,"%E",errno);
          disconnectDescriptor(socketDescriptor);
          return error;
        }
      }
      break;
    case SOCKET_TYPE_TLS:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case SOCKET_TYPE_SSH:
      #ifdef HAVE_SSH2
      {
        assert(loginName != NULL);

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
        socketDescriptor = socket(AF_INET,SOCK_STREAM,0);
        if (socketDescriptor == -1)
        {
          return ERRORX_(CONNECT_FAIL,errno,"%E",errno);
        }
        socketAddress.sin_family      = AF_INET;
        socketAddress.sin_addr.s_addr = ipAddress;
        socketAddress.sin_port        = htons(hostPort);
        if (connect(socketDescriptor,
                    (struct sockaddr*)&socketAddress,
                    sizeof(socketAddress)
                   ) != 0
           )
        {
          error = ERRORX_(CONNECT_FAIL,errno,"%E",errno);
          disconnectDescriptor(socketDescriptor);
          return error;
        }
      }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(loginName);
        UNUSED_VARIABLE(password);
        UNUSED_VARIABLE(sshPublicKeyData);
        UNUSED_VARIABLE(sshPublicKeyLength);
        UNUSED_VARIABLE(sshPrivateKeyData);
        UNUSED_VARIABLE(sshPrivateKeyLength);

        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return Network_connectDescriptor(socketHandle,
                                   socketDescriptor,
                                   socketType,
                                   loginName,
                                   password,
                                   caData,
                                   caLength,
                                   certData,
                                   certLength,
                                   publicKeyData,
                                   publicKeyLength,
                                   privateKeyData,
                                   privateKeyLength,
                                   socketFlags
                                  );
}

Errors Network_connectDescriptor(SocketHandle *socketHandle,
                                 int          socketDescriptor,
                                 SocketTypes  socketType,
                                 ConstString  loginName,
                                 Password     *password,
                                 const void   *caData,
                                 uint         caLength,
                                 const void   *certData,
                                 uint         certLength,
                                 const void   *publicKeyData,
                                 uint         publicKeyLength,
                                 const void   *privateKeyData,
                                 uint         privateKeyLength,
                                 SocketFlags  socketFlags
                                )
{
  #ifdef HAVE_SSH2
    int    ssh2Error;
    char   *ssh2ErrorText;
    Errors error;
  #endif /* HAVE_SSH2 */

  assert(socketHandle != NULL);

  // initialize variables
  socketHandle->type        = socketType;
  socketHandle->handle      = socketDescriptor;
  socketHandle->flags       = socketFlags;
  socketHandle->isConnected = FALSE;

  switch (socketType)
  {
    case SOCKET_TYPE_PLAIN:
      {
        #if  defined(PLATFORM_LINUX)
          long   flags;
          int    n;
        #elif defined(PLATFORM_WINDOWS)
          u_long n;
        #endif /* PLATFORM_... */

        if (socketFlags != SOCKET_FLAG_NONE)
        {
          // enable non-blocking
          #if  defined(PLATFORM_LINUX)
            if ((socketFlags & SOCKET_FLAG_NON_BLOCKING) != 0)
            {
              flags = fcntl(socketHandle->handle,F_GETFL,0);
              fcntl(socketHandle->handle,F_SETFL,flags | O_NONBLOCK);
            }
            if ((socketFlags & SOCKET_FLAG_NO_DELAY    ) != 0)
            {
              n = 1;
              setsockopt(socketHandle->handle,IPPROTO_TCP,TCP_NODELAY,(void*)&n,sizeof(int));
            }
            if ((socketFlags & SOCKET_FLAG_KEEP_ALIVE  ) != 0)
            {
              n = 1;
              setsockopt(socketHandle->handle,SOL_SOCKET,SO_KEEPALIVE,(void*)&n,sizeof(int));
            }
          #elif defined(PLATFORM_WINDOWS)
            if ((socketFlags & SOCKET_FLAG_NON_BLOCKING) != 0)
            {
              n = 1;
              ioctlsocket(socketHandle->handle,FIONBIO,&n);
            }
            if ((socketFlags & SOCKET_FLAG_NO_DELAY    ) != 0)
            {
              n = 1;
              setsockopt(socketHandle->handle,IPPROTO_TCP,TCP_NODELAY,(char*)&n,sizeof(int));
            }
            if ((socketFlags & SOCKET_FLAG_KEEP_ALIVE  ) != 0)
            {
              n = 1;
              setsockopt(socketHandle->handle,SOL_SOCKET,SO_KEEPALIVE,(char*)&n,sizeof(int));
            }
          #endif /* PLATFORM_... */
        }
      }
      break;
    case SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
      {
        assert(loginName != NULL);

        // check login name
        if (String_isEmpty(loginName))
        {
          return ERROR_NO_LOGIN_NAME;
        }

        // check if certificate is valid
        error = validateCertificate(certData,certLength);
        if (error != ERROR_NONE)
        {
          return error;
        }

        // init SSL
        error = initSSL(socketHandle,
                        NETWORK_TLS_TYPE_SERVER,
                        caData,
                        caLength,
                        certData,
                        certLength,
                        privateKeyData,
                        privateKeyLength
                       );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      #else /* not HAVE_GNU_TLS */
        UNUSED_VARIABLE(loginName);
        UNUSED_VARIABLE(password);
        UNUSED_VARIABLE(caData);
        UNUSED_VARIABLE(caLength);
        UNUSED_VARIABLE(certData);
        UNUSED_VARIABLE(certLength);
        UNUSED_VARIABLE(publicKeyData);
        UNUSED_VARIABLE(publicKeyLength);
        UNUSED_VARIABLE(privateKeyData);
        UNUSED_VARIABLE(privateKeyLength);

        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GNU_TLS */
      break;
    case SOCKET_TYPE_SSH:
      #ifdef HAVE_SSH2
      {
        #if  defined(PLATFORM_LINUX)
          long       flags;
          int        n;
        #elif defined(PLATFORM_WINDOWS)
          u_long     n;
        #endif /* PLATFORM_... */
        int result;

        assert(loginName != NULL);

        UNUSED_VARIABLE(caData);
        UNUSED_VARIABLE(caLength);
        UNUSED_VARIABLE(certData);
        UNUSED_VARIABLE(certLength);

        // check login name
        if (String_isEmpty(loginName))
        {
          return ERROR_NO_LOGIN_NAME;
        }

        // init SSH session
        socketHandle->ssh2.session = libssh2_session_init();
        if (socketHandle->ssh2.session == NULL)
        {
          return ERROR_SSH_SESSION_FAIL;
        }
        if ((socketFlags & SOCKET_FLAG_VERBOSE_MASK) != 0)
        {
          libssh2_trace(socketHandle->ssh2.session,
                          0
                        | (((socketFlags & SOCKET_FLAG_VERBOSE2) != 0)
                             ?   LIBSSH2_TRACE_SOCKET
                               | LIBSSH2_TRACE_TRANS
                               | LIBSSH2_TRACE_CONN
                             : 0
                          )
                        | (((socketFlags & SOCKET_FLAG_VERBOSE1) != 0)
                             ?   LIBSSH2_TRACE_KEX
                               | LIBSSH2_TRACE_AUTH
                               | LIBSSH2_TRACE_SCP
                               | LIBSSH2_TRACE_SFTP
                               | LIBSSH2_TRACE_ERROR
                               | LIBSSH2_TRACE_PUBLICKEY
                             : 0
                          )
                       );
        }
        if (libssh2_session_startup(socketHandle->ssh2.session,
                                    socketHandle->handle
                                   ) != 0
           )
        {
          ssh2Error = libssh2_session_last_error(socketHandle->ssh2.session,&ssh2ErrorText,NULL,0);
          error = ERRORX_(SSH_SESSION_FAIL,ssh2Error,"%s",ssh2ErrorText);
          libssh2_session_disconnect(socketHandle->ssh2.session,"");
          libssh2_session_free(socketHandle->ssh2.session);
          return error;
        }
        #ifdef HAVE_SSH2_KEEPALIVE_CONFIG
// NYI/???: does not work?
//          libssh2_keepalive_config(socketHandle->ssh2.session,0,2*60);
        #endif /* HAVE_SSH2_KEEPALIVE_CONFIG */

#if 1
        // authorize with key/password
        result = 0;
        PASSWORD_DEPLOY_DO(plainPassword,password)
        {
          if ((publicKeyData != NULL) && (privateKeyData != NULL))
          {
            result = libssh2_userauth_publickey_frommemory(socketHandle->ssh2.session,
                                                           String_cString(loginName),
                                                           String_length(loginName),
                                                           publicKeyData,
                                                           publicKeyLength,
                                                           privateKeyData,
                                                           privateKeyLength,
                                                           plainPassword
                                                          );
          }
          else
          {
            result = libssh2_userauth_password(socketHandle->ssh2.session,
                                               String_cString(loginName),
                                               plainPassword
                                              );
          }
        }
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
          libssh2_session_disconnect(socketHandle->ssh2.session,"");
          libssh2_session_free(socketHandle->ssh2.session);
          return error;
        }
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
          return error;
        }
#endif /* 0 */
        if (socketFlags != SOCKET_FLAG_NONE)
        {
          // enable non-blocking
          #if  defined(PLATFORM_LINUX)
            if ((socketFlags & SOCKET_FLAG_NON_BLOCKING) != 0)
            {
              flags = fcntl(socketHandle->handle,F_GETFL,0);
              fcntl(socketHandle->handle,F_SETFL,flags | O_NONBLOCK);
            }
            if ((socketFlags & SOCKET_FLAG_KEEP_ALIVE  ) != 0)
            {
              n = 1;
              setsockopt(socketHandle->handle,SOL_SOCKET,SO_KEEPALIVE,(void*)&n,sizeof(int));
            }
          #elif defined(PLATFORM_WINDOWS)
            if ((socketFlags & SOCKET_FLAG_NON_BLOCKING) != 0)
            {
              n = 1;
              ioctlsocket(socketHandle->handle,FIONBIO,&n);
            }
            if ((socketFlags & SOCKET_FLAG_KEEP_ALIVE  ) != 0)
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
        UNUSED_VARIABLE(caData);
        UNUSED_VARIABLE(caLength);
        UNUSED_VARIABLE(certData);
        UNUSED_VARIABLE(certLength);
        UNUSED_VARIABLE(publicKeyData);
        UNUSED_VARIABLE(publicKeyLength);
        UNUSED_VARIABLE(privateKeyData);
        UNUSED_VARIABLE(privateKeyLength);

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

  disconnect(socketHandle);
}

void Network_disconnectDescriptor(int socketDescriptor)
{
  assert(socketDescriptor >= 0);

  disconnectDescriptor(socketDescriptor);
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
  SignalMask signalMask;
  uint       events;
  long       n;

  assert(socketHandle != NULL);
  assert(buffer != NULL);
  assert(maxLength > 0);
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
        // Note: ignore SIGALRM in Misc_wait()
        #ifdef HAVE_SIGALRM
          // Note: ignore SIGALRM in poll()/pselect()
          MISC_SIGNAL_MASK_CLEAR(signalMask);
          MISC_SIGNAL_MASK_SET(signalMask,SIGALRM);
        #endif /* HAVE_SIGALRM */

        // wait for data
        events = Misc_waitHandle(socketHandle->handle,&signalMask,HANDLE_EVENT_INPUT,timeout);
        if      (Misc_isHandleEvent(events,HANDLE_EVENT_INPUT))
        {
          n = recv(socketHandle->handle,buffer,maxLength,0);
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
          // Note: ignore SIGALRM in Misc_wait()
          #ifdef HAVE_SIGALRM
            // Note: ignore SIGALRM in poll()/pselect()
            MISC_SIGNAL_MASK_CLEAR(signalMask);
            MISC_SIGNAL_MASK_SET(signalMask,SIGALRM);
          #endif /* HAVE_SIGALRM */

          // wait for data
          events = Misc_waitHandle(socketHandle->handle,&signalMask,HANDLE_EVENT_INPUT,timeout);
          if (Misc_isHandleEvent(events,HANDLE_EVENT_INPUT))
          {
            n = gnutls_record_recv(socketHandle->gnuTLS.session,buffer,maxLength);
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
  ulong      sentBytes;
  SignalMask signalMask;
  uint       events;
  long       n;

  assert(socketHandle != NULL);
  assert(buffer != NULL);

  sentBytes = 0L;
  if (length > 0)
  {
    switch (socketHandle->type)
    {
      case SOCKET_TYPE_PLAIN:
        do
        {
          // Note: ignore SIGALRM in Misc_wait()
          #ifdef HAVE_SIGALRM
            // Note: ignore SIGALRM in poll()/pselect()
            MISC_SIGNAL_MASK_CLEAR(signalMask);
            MISC_SIGNAL_MASK_SET(signalMask,SIGALRM);
          #endif /* HAVE_SIGALRM */

          // wait until space in buffer is available
          events = Misc_waitHandle(socketHandle->handle,&signalMask,HANDLE_EVENT_OUTPUT,SEND_TIMEOUT);
          if ((events & HANDLE_EVENT_OUTPUT) != 0)
          {
            // send data
            #ifdef HAVE_MSG_NOSIGNAL
              #define FLAGS MSG_NOSIGNAL
            #else
              #define FLAGS 0
            #endif
            n = send(socketHandle->handle,(const char*)(((byte*)buffer)+sentBytes),length-sentBytes,FLAGS);
            #undef FLAGS
            if      (n > 0) sentBytes += (ulong)n;
            else if ((n == -1) && (errno != EAGAIN)) break;
          }
          else
          {
            break;
          }
        }
        while (sentBytes < length);
        break;
      case SOCKET_TYPE_TLS:
        #ifdef HAVE_GNU_TLS
          do
          {
            // Note: ignore SIGALRM in Misc_wait()
            #ifdef HAVE_SIGALRM
              // Note: ignore SIGALRM in poll()/pselect()
              MISC_SIGNAL_MASK_CLEAR(signalMask);
              MISC_SIGNAL_MASK_SET(signalMask,SIGALRM);
            #endif /* HAVE_SIGALRM */

            // wait until space in buffer is available
            events = Misc_waitHandle(socketHandle->handle,&signalMask,HANDLE_EVENT_OUTPUT,SEND_TIMEOUT);
            if ((events & HANDLE_EVENT_OUTPUT) != 0)
            {
              // send data
              n = gnutls_record_send(socketHandle->gnuTLS.session,((byte*)buffer)+sentBytes,length-sentBytes);
              if      (n > 0) sentBytes += n;
              else if ((n < 0) && (errno != GNUTLS_E_AGAIN)) break;
            }
            else
            {
              break;
            }
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
      if (ch != '\r')
      {
        if (ch != '\n')
        {
          String_appendChar(line,ch);
        }
        else
        {
          endOfLineFlag = TRUE;
        }
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
  int                n;
  struct sockaddr_in socketAddress;
  Errors             error;

  assert(serverSocketHandle != NULL);

  // init variables
  serverSocketHandle->socketType = serverSocketType;

  // create socket
  serverSocketHandle->handle = socket(AF_INET,SOCK_STREAM,0);
  if (serverSocketHandle->handle == -1)
  {
    return ERRORX_(CONNECT_FAIL,errno,"%E",errno);
  }

  // reuse address
  n = 1;
  if (setsockopt(serverSocketHandle->handle,SOL_SOCKET,SO_REUSEADDR,(void*)&n,sizeof(int)) != 0)
  {
    error = ERRORX_(CONNECT_FAIL,errno,"%E",errno);
    disconnectDescriptor(serverSocketHandle->handle);
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
    error = ERRORX_(BIND_FAIL,errno,"%E",errno);
    disconnectDescriptor(serverSocketHandle->handle);
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
          disconnectDescriptor(serverSocketHandle->handle);
          return ERROR_NO_TLS_CA;
        }
        if (certData == NULL)
        {
          disconnectDescriptor(serverSocketHandle->handle);
          return ERROR_NO_TLS_CERTIFICATE;
        }
        if (keyData == NULL)
        {
          disconnectDescriptor(serverSocketHandle->handle);
          return ERROR_NO_TLS_KEY;
        }

        // check if certificate is valid
        if (gnutls_x509_crt_init(&cert) != GNUTLS_E_SUCCESS)
        {
          disconnectDescriptor(serverSocketHandle->handle);
          return ERROR_INVALID_TLS_CERTIFICATE;
        }
        datum.data = (void*)certData;
        datum.size = certLength;
        if (gnutls_x509_crt_import(cert,&datum,GNUTLS_X509_FMT_PEM) != GNUTLS_E_SUCCESS)
        {
          gnutls_x509_crt_deinit(cert);
          disconnectDescriptor(serverSocketHandle->handle);
          return ERROR_INVALID_TLS_CERTIFICATE;
        }
        certActivationTime = gnutls_x509_crt_get_activation_time(cert);
        if (certActivationTime != (time_t)(-1))
        {
          if (time(NULL) < certActivationTime)
          {
            gnutls_x509_crt_deinit(cert);
            disconnectDescriptor(serverSocketHandle->handle);
            return ERRORX_(TLS_CERTIFICATE_NOT_ACTIVE,0,"%s",Misc_formatDateTimeCString(buffer,sizeof(buffer),(uint64)certActivationTime,FALSE,DATE_TIME_FORMAT_LOCALE));
          }
        }
        certExpireTime = gnutls_x509_crt_get_expiration_time(cert);
        if (certExpireTime != (time_t)(-1))
        {
          if (time(NULL) > certExpireTime)
          {
            gnutls_x509_crt_deinit(cert);
            disconnectDescriptor(serverSocketHandle->handle);
            return ERRORX_(TLS_CERTIFICATE_EXPIRED,0,"%s",Misc_formatDateTimeCString(buffer,sizeof(buffer),(uint64)certExpireTime,FALSE,DATE_TIME_FORMAT_LOCALE));
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
          gnutls_x509_crt_deinit(cert);
          disconnectDescriptor(serverSocketHandle->handle);
          return ERROR_INVALID_TLS_CA;
        }
        gnutls_certificate_free_credentials(serverSocketHandle->gnuTLSCredentials);
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
        UNUSED_VARIABLE(caData);
        UNUSED_VARIABLE(caLength);
        UNUSED_VARIABLE(certData);
        UNUSED_VARIABLE(certLength);
        UNUSED_VARIABLE(keyData);
        UNUSED_VARIABLE(keyLength);
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
  disconnectDescriptor(serverSocketHandle->handle);
}

Errors Network_startTLS(SocketHandle    *socketHandle,
                        NetworkTLSTypes tlsType,
                        const void      *caData,
                        uint            caLength,
                        const void      *certData,
                        uint            certLength,
                        const void      *keyData,
                        uint            keyLength
                       )
{
  #ifdef HAVE_GNU_TLS
    #if  defined(PLATFORM_LINUX)
      long               flags;
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
        flags = fcntl(socketHandle->handle,F_GETFL,0);
        (void)fcntl(socketHandle->handle,F_SETFL,flags & ~O_NONBLOCK);
      #elif defined(PLATFORM_WINDOWS)
        n = 0;
        (void)ioctlsocket(socketHandle->handle,FIONBIO,&n);
      #endif /* PLATFORM_... */
    }

    // init SSL
    error = initSSL(socketHandle,tlsType,caData,caLength,certData,certLength,keyData,keyLength);
    if (error != ERROR_NONE)
    {
      #if  defined(PLATFORM_LINUX)
        flags = fcntl(socketHandle->handle,F_GETFL,0);
        (void)fcntl(socketHandle->handle,F_SETFL,flags | O_NONBLOCK);
      #elif defined(PLATFORM_WINDOWS)
        n = 1;
        (void)ioctlsocket(socketHandle->handle,FIONBIO,&n);
      #endif /* PLATFORM_... */
      return error;
    }

    // re-enable temporary non-blocking
    if ((socketHandle->flags & SOCKET_FLAG_NON_BLOCKING) !=  0)
    {
      #if  defined(PLATFORM_LINUX)
        flags = fcntl(socketHandle->handle,F_GETFL,0);
        (void)fcntl(socketHandle->handle,F_SETFL,flags | O_NONBLOCK);
      #elif defined(PLATFORM_WINDOWS)
        n = 1;
        (void)ioctlsocket(socketHandle->handle,FIONBIO,&n);
      #endif /* PLATFORM_... */
    }

    socketHandle->type = SOCKET_TYPE_TLS;

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

int Network_getServerSocket(const ServerSocketHandle *serverSocketHandle)
{
  assert(serverSocketHandle != NULL);

  return serverSocketHandle->handle;
}

Errors Network_accept(SocketHandle             *socketHandle,
                      const ServerSocketHandle *serverSocketHandle,
                      SocketFlags              socketFlags
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
    long               flags;
  #elif defined(PLATFORM_WINDOWS)
    u_long             n;
  #endif /* PLATFORM_... */

//unsigned int status;

  assert(socketHandle != NULL);
  assert(serverSocketHandle != NULL);

  // init variables
  socketHandle->flags = socketFlags;

  // accept
  socketAddressLength = sizeof(socketAddress);
  socketHandle->handle = accept(serverSocketHandle->handle,
                                (struct sockaddr*)&socketAddress,
                                &socketAddressLength
                               );
  if (socketHandle->handle == -1)
  {
    return ERRORX_(CONNECT_FAIL,errno,"%E",errno);
  }

  switch (serverSocketHandle->socketType)
  {
    case SERVER_SOCKET_TYPE_PLAIN:
      socketHandle->type = SOCKET_TYPE_PLAIN;
      break;
    case SERVER_SOCKET_TYPE_TLS:
      #ifdef HAVE_GNU_TLS
        // init SSL
        error = initSSL(socketHandle,
                        NETWORK_TLS_TYPE_SERVER,
                        serverSocketHandle->caData,
                        serverSocketHandle->caLength,
                        serverSocketHandle->certData,
                        serverSocketHandle->certLength,
                        serverSocketHandle->keyData,
                        serverSocketHandle->keyLength
                       );
        if (error != ERROR_NONE)
        {
          disconnectDescriptor(socketHandle->handle);
          return error;
        }

        socketHandle->type = SOCKET_TYPE_TLS;
      #else /* not HAVE_GNU_TLS */
        UNUSED_VARIABLE(socketHandle);
        UNUSED_VARIABLE(serverSocketHandle);
        UNUSED_VARIABLE(socketFlags);

        disconnectDescriptor(serverSocketHandle->handle);

        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GNU_TLS */
      break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
  }

  if ((socketFlags & SOCKET_FLAG_NON_BLOCKING) != 0)
  {
    // enable non-blocking
    #if  defined(PLATFORM_LINUX)
      flags = fcntl(socketHandle->handle,F_GETFL,0);
      fcntl(socketHandle->handle,F_SETFL,flags | O_NONBLOCK);
    #elif defined(PLATFORM_WINDOWS)
      n = 1;
      ioctlsocket(socketHandle->handle,FIONBIO,&n);
    #endif /* PLATFORM_... */
  }

  socketHandle->isConnected = TRUE;

  return ERROR_NONE;
}

Errors Network_reject(const ServerSocketHandle *serverSocketHandle)
{
  struct sockaddr_in socketAddress;
  #if   defined(PLATFORM_LINUX)
    socklen_t          socketAddressLength;
  #elif defined(PLATFORM_WINDOWS)
    int                socketAddressLength;
  #endif /* PLATFORM_... */
  int handle;

  // accept
  socketAddressLength = sizeof(socketAddress);
  handle = accept(serverSocketHandle->handle,
                  (struct sockaddr*)&socketAddress,
                  &socketAddressLength
                 );
  if (handle == -1)
  {
    return ERRORX_(CONNECT_FAIL,errno,"%E",errno);
  }

  // disconnect
  disconnectDescriptor(handle);

  return ERROR_NONE;
}

void Network_getLocalInfo(SocketHandle  *socketHandle,
                          String        name,
                          uint          *port,
                          SocketAddress *socketAddress
                         )
{
  struct sockaddr_in   sockAddrIn;
  #if   defined(PLATFORM_LINUX)
    socklen_t            sockAddrInLength;
  #elif defined(PLATFORM_WINDOWS)
    int                  sockAddrInLength;
  #endif /* PLATFORM_... */
  #if   defined(HAVE_GETHOSTBYADDR_R)
    char           buffer[512];
    struct hostent bufferAddressEntry;
    struct hostent *hostAddressEntry;
    int            getHostByAddrError;
  #elif defined(HAVE_GETHOSTBYADDR)
    const struct hostent *hostEntry;
  #endif /* HAVE_GETHOSTBYNAME* */

  assert(socketHandle != NULL);
  assert(name != NULL);
  assert(port != NULL);

  sockAddrInLength = sizeof(sockAddrIn);
  if (getsockname(socketHandle->handle,
                  (struct sockaddr*)&sockAddrIn,
                  &sockAddrInLength
                 ) == 0
     )
  {
    if (name != NULL)
    {
      #if   defined(HAVE_GETHOSTBYADDR_R)
        if (   (gethostbyaddr_r(&sockAddrIn.sin_addr,
                                sizeof(sockAddrIn.sin_addr),
                                AF_INET,
                                &bufferAddressEntry,
                                buffer,
                                sizeof(buffer),
                                &hostAddressEntry,
                                &getHostByAddrError
                               ) == 0
               )
            && (hostAddressEntry != NULL)
           )
        {
          String_setCString(name,hostAddressEntry->h_name);
        }
        else
        {
          String_setCString(name,inet_ntoa(sockAddrIn.sin_addr));
        }
      #elif defined(HAVE_GETHOSTBYADDR)
        hostEntry = gethostbyaddr((const char*)&sockAddrIn.sin_addr,
                                  sizeof(sockAddrIn.sin_addr),
                                  AF_INET
                                 );
        if (hostEntry != NULL)
        {
          String_setCString(name,hostEntry->h_name);
        }
        else
        {
          String_setCString(name,inet_ntoa(sockAddrIn.sin_addr));
        }
      #else /* not HAVE_GETHOSTBYADDR* */
        String_setCString(name,inet_ntoa(sockAddrIn.sin_addr));
      #endif /* HAVE_GETHOSTBYADDR* */
    }
    if (port != NULL) (*port) = ntohs(sockAddrIn.sin_port);
    if (socketAddress != NULL)
    {
      if      (sockAddrIn.sin_family == AF_INET)
      {
        socketAddress->type = SOCKET_ADDRESS_TYPE_V4;
        memcpy(&socketAddress->address.v4,&sockAddrIn.sin_addr,sizeof(sockAddrIn.sin_addr));
      }
      else if (sockAddrIn.sin_family == AF_INET6)
      {
        socketAddress->type = SOCKET_ADDRESS_TYPE_V6;
        memcpy(&socketAddress->address.v6,&sockAddrIn.sin_addr,sizeof(sockAddrIn.sin_addr));
      }
      else
      {
        socketAddress->type = SOCKET_ADDRESS_TYPE_NONE;
      }
    }
  }
  else
  {
    if (name != NULL) String_setCString(name,"unknown");
    if (port != NULL) (*port) = 0;
    if (socketAddress != NULL) socketAddress->type = SOCKET_ADDRESS_TYPE_NONE;
  }
}

void Network_getRemoteInfo(SocketHandle  *socketHandle,
                           String        name,
                           uint          *port,
                           SocketAddress *socketAddress
                          )
{
  struct sockaddr_in   sockAddrIn;
  #if   defined(PLATFORM_LINUX)
    socklen_t            sockAddrInLength;
  #elif defined(PLATFORM_WINDOWS)
    int                  sockAddrInLength;
  #endif /* PLATFORM_... */
  #if   defined(HAVE_GETHOSTBYADDR_R)
    char           buffer[512];
    struct hostent bufferAddressEntry;
    struct hostent *hostAddressEntry;
    int            getHostByAddrError;
  #elif defined(HAVE_GETHOSTBYADDR)
    const struct hostent *hostEntry;
  #endif /* HAVE_GETHOSTBYNAME* */

  assert(socketHandle != NULL);
  assert(name != NULL);
  assert(port != NULL);

  sockAddrInLength = sizeof(sockAddrIn);
  if (getpeername(socketHandle->handle,
                  (struct sockaddr*)&sockAddrIn,
                  &sockAddrInLength
                 ) == 0
     )
  {
    if (name != NULL)
    {
      #if   defined(HAVE_GETHOSTBYADDR_R)
        if (   (gethostbyaddr_r(&sockAddrIn.sin_addr,
                                sizeof(sockAddrIn.sin_addr),
                                AF_INET,
                                &bufferAddressEntry,
                                buffer,
                                sizeof(buffer),
                                &hostAddressEntry,
                                &getHostByAddrError
                               ) == 0
               )
            && (hostAddressEntry != NULL)
           )
        {
          String_setCString(name,hostAddressEntry->h_name);
        }
        else
        {
          String_setCString(name,inet_ntoa(sockAddrIn.sin_addr));
        }
      #elif defined(HAVE_GETHOSTBYADDR)
        hostEntry = gethostbyaddr((const void*)&sockAddrIn.sin_addr,
                                  sizeof(sockAddrIn.sin_addr),
                                  AF_INET
                                 );
        if (hostEntry != NULL)
        {
          String_setCString(name,hostEntry->h_name);
        }
        else
        {
          String_setCString(name,inet_ntoa(sockAddrIn.sin_addr));
        }
      #else /* not HAVE_GETHOSTBYADDR* */
        String_setCString(name,inet_ntoa(sockAddrIn.sin_addr));
      #endif /* HAVE_GETHOSTBYADDR* */
    }
    if (port != NULL) (*port) = ntohs(sockAddrIn.sin_port);
    if (socketAddress != NULL)
    {
      if      (sockAddrIn.sin_family == AF_INET)
      {
        socketAddress->type = SOCKET_ADDRESS_TYPE_V4;
        memcpy(&socketAddress->address.v4,&sockAddrIn.sin_addr,sizeof(sockAddrIn.sin_addr));
      }
      else if (sockAddrIn.sin_family == AF_INET6)
      {
        socketAddress->type = SOCKET_ADDRESS_TYPE_V6;
        memcpy(&socketAddress->address.v6,&sockAddrIn.sin_addr,sizeof(sockAddrIn.sin_addr));
      }
      else
      {
        socketAddress->type = SOCKET_ADDRESS_TYPE_NONE;
      }
    }
  }
  else
  {
    if (name != NULL) String_setCString(name,"unknown");
    if (port != NULL) (*port) = 0;
    if (socketAddress != NULL) socketAddress->type = SOCKET_ADDRESS_TYPE_NONE;
  }
}

bool Network_isLocalHost(const SocketAddress *socketAddress)
{
  bool isLocalHost;
  union
  {
    struct in_addr v4;
    struct in6_addr v6;
  } address;
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    int addressSize;
  #endif /* PLATFORM_... */

  assert(socketAddress != NULL);

  isLocalHost = FALSE;
  switch (socketAddress->type)
  {
    case SOCKET_ADDRESS_TYPE_NONE:
      break;
    case SOCKET_ADDRESS_TYPE_V4:
      #if   defined(PLATFORM_LINUX)
        inet_pton(AF_INET,"127.0.0.1",&address.v4);
        isLocalHost = (memcmp(&socketAddress->address.v4,&address.v4,sizeof(socketAddress->address.v4)) == 0);
      #elif defined(PLATFORM_WINDOWS)
        addressSize = sizeof(socketAddress->address.v4);
        isLocalHost =    (WSAStringToAddressA("127.0.0.1",AF_INET,NULL,(LPSOCKADDR)&address.v4,&addressSize) == 0)
                      && (addressSize == sizeof(socketAddress->address.v4))
                      && (memcmp(&socketAddress->address.v4,&address.v4,sizeof(socketAddress->address.v4)) == 0);
      #endif /* PLATFORM_... */
      break;
    case SOCKET_ADDRESS_TYPE_V6:
      #if   defined(PLATFORM_LINUX)
        inet_pton(AF_INET,"::1",&address.v6);
        isLocalHost = (memcmp(&socketAddress->address.v6,&address.v6,sizeof(socketAddress->address.v6)) == 0);
      #elif defined(PLATFORM_WINDOWS)
        addressSize = sizeof(socketAddress->address.v6);
        isLocalHost =    (WSAStringToAddressA("::1",AF_INET6,NULL,(LPSOCKADDR)&address.v6,&addressSize) == 0)
                      && (addressSize == sizeof(socketAddress->address.v6))
                      && (memcmp(&socketAddress->address.v6,&address.v6,sizeof(socketAddress->address.v6)) == 0);
      #endif /* PLATFORM_... */
      break;
  }

  return isLocalHost;
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
      long   flags;
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
      flags = fcntl(socketHandle->handle,F_GETFL,0);
      fcntl(socketHandle->handle,F_SETFL,flags | O_NONBLOCK);
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

  eofFlag   = TRUE;
  bytesRead = 0;
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
        networkExecuteHandle->stdoutBuffer.index  = 0;
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
        networkExecuteHandle->stderrBuffer.index  = 0;
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
  bytesRead     = 0;
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

          networkExecuteHandle->stdoutBuffer.index  = 0;
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

          networkExecuteHandle->stderrBuffer.index  = 0;
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
