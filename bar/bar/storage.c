/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: storage functions
* Systems: all
*
\***********************************************************************/

#define __STORAGE_IMPLEMENATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#ifdef HAVE_CURL
  #include <curl/curl.h>
#endif /* HAVE_CURL */
#ifdef HAVE_MXML
  #include <mxml.h>
#endif /* HAVE_MXML */
#ifdef HAVE_SSH2
  #include <libssh2.h>
  #include <libssh2_sftp.h>
#endif /* HAVE_SSH2 */
#ifdef HAVE_ISO9660
  #include <cdio/cdio.h>
  #include <cdio/iso9660.h>
  #include <cdio/logging.h>
#endif /* HAVE_ISO9660 */
#include <signal.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "autofree.h"
#include "strings.h"
#include "stringlists.h"
#include "files.h"
#include "network.h"
#include "errors.h"

#include "errors.h"
#include "crypt.h"
#include "passwords.h"
#include "misc.h"
#include "archive.h"
#include "bar_global.h"
#include "bar.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
/* file data buffer size */
#define BUFFER_SIZE (64*1024)

// different timeouts [ms]
#define SSH_TIMEOUT    (30*1000)
#define READ_TIMEOUT   (60*1000)

#define INITIAL_BUFFER_SIZE   (64*1024)
#define INCREMENT_BUFFER_SIZE ( 8*1024)
#define MAX_BUFFER_SIZE       (64*1024)
#define MAX_FILENAME_LENGTH   ( 8*1024)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL sighandler_t oldSignalAlarmHandler;
#ifdef HAVE_SSH2
  LOCAL Password *defaultSSHPassword;
#endif /* HAVE_SSH2 */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : signalHandler
* Purpose: signal handler
* Input  : signalHandler - signal number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void signalHandler(int signalNumber)
{
  /* Note: curl trigger from time to time a SIGALRM. The curl option
           CURLOPT_NOSIGNAL should stop this. But it seems there is
           a bug in curl which cause random crashes when
           CURLOPT_NOSIGNAL is enabled. Thus: do not use it!
           Instead install this signal handler to catch the not wanted
           signal and discard it.
  */
  UNUSED_VARIABLE(signalNumber);
}

/***********************************************************************\
* Name   : updateStatusInfo
* Purpose: update status info
* Input  : storageStatusInfo - storage status info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateStatusInfo(const StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  if (storageHandle->storageStatusInfoFunction != NULL)
  {
    storageHandle->storageStatusInfoFunction(storageHandle->storageStatusInfoUserData,
                                             &storageHandle->runningInfo
                                            );
  }
}

#ifdef HAVE_CURL
/***********************************************************************\
* Name   : curlNopDataCallback
* Purpose: curl nop data callback: just discard data
* Input  : buffer   - buffer for data
*          size     - size of element
*          n        - number of elements
*          userData - user data
* Output : -
* Return : always size*n
* Notes  : -
\***********************************************************************/

LOCAL size_t curlNopDataCallback(void   *buffer,
                                 size_t size,
                                 size_t n,
                                 void   *userData
                                )
{
  UNUSED_VARIABLE(buffer);
  UNUSED_VARIABLE(userData);

  return size*n;
}

/***********************************************************************\
* Name   : waitCurlSocket
* Purpose: wait for Curl socket
* Input  : curlMultiHandle - Curl multi handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors waitCurlSocket(CURLM *curlMultiHandle)
{
  sigset_t        signalMask;
  fd_set          fdSetRead,fdSetWrite,fdSetException;
  int             maxFD;
  long            curlTimeout;
  struct timespec ts;
  Errors          error;

  assert(curlMultiHandle != NULL);

  // Note: ignore SIGALRM in pselect()
  sigemptyset(&signalMask);
  sigaddset(&signalMask,SIGALRM);

  // get file descriptors from the transfers
  FD_ZERO(&fdSetRead);
  FD_ZERO(&fdSetWrite);
  FD_ZERO(&fdSetException);
  curl_multi_fdset(curlMultiHandle,&fdSetRead,&fdSetWrite,&fdSetException,&maxFD);

  // get a suitable timeout
  curl_multi_timeout(curlMultiHandle,&curlTimeout);

  if (curlTimeout > (long)READ_TIMEOUT)
  {
    ts.tv_sec  = curlTimeout/1000L;
    ts.tv_nsec = (curlTimeout%1000L)*1000000L;
  }
  else
  {
    ts.tv_sec  = READ_TIMEOUT/1000L;
    ts.tv_nsec = (READ_TIMEOUT%1000L)*1000000L;
  }

  // wait
  switch (pselect(maxFD+1,&fdSetRead,&fdSetWrite,&fdSetException,&ts,&signalMask))
  {
    case -1:
      // error
      error = ERROR_NETWORK_RECEIVE;
      break;
    case 0:
      // timeout
      error = ERROR_NETWORK_TIMEOUT;
      break;
    default:
      // OK
      error = ERROR_NONE;
      break;
  }

  return error;
}

#endif /* HAVE_CURL */

#if defined(HAVE_CURL) || defined(HAVE_FTP) || defined(HAVE_SSH2)
/***********************************************************************\
* Name   : initBandWidthLimiter
* Purpose: init band width limiter structure
* Input  : storageBandWidthLimiter - storage band width limiter
*          maxBandWidthList        - list with max. band width to use
*                                    [bit/s] or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initBandWidthLimiter(StorageBandWidthLimiter *storageBandWidthLimiter,
                                BandWidthList           *maxBandWidthList
                               )
{
  ulong maxBandWidth;
  uint  z;

  assert(storageBandWidthLimiter != NULL);

  maxBandWidth = getBandWidth(maxBandWidthList);

  storageBandWidthLimiter->maxBandWidthList     = maxBandWidthList;
  storageBandWidthLimiter->maxBlockSize         = 64*1024;
  storageBandWidthLimiter->blockSize            = 64*1024;
  for (z = 0; z < SIZE_OF_ARRAY(storageBandWidthLimiter->measurements); z++)
  {
    storageBandWidthLimiter->measurements[z] = maxBandWidth;
  }
  storageBandWidthLimiter->measurementCount     = 0;
  storageBandWidthLimiter->measurementNextIndex = 0;
  storageBandWidthLimiter->measurementBytes     = 0L;
  storageBandWidthLimiter->measurementTime      = 0LL;
}

/***********************************************************************\
* Name   : doneBandWidthLimiter
* Purpose: done band width limiter structure
* Input  : storageBandWidthLimiter - storage band width limiter
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneBandWidthLimiter(StorageBandWidthLimiter *storageBandWidthLimiter)
{
  assert(storageBandWidthLimiter != NULL);

  UNUSED_VARIABLE(storageBandWidthLimiter);
}

/***********************************************************************\
* Name   : limitBandWidth
* Purpose: limit used band width
* Input  : storageBandWidthLimiter - storage band width limiter
*          transmittedBytes        - transmitted bytes
*          transmissionTime        - time for transmission [us]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void limitBandWidth(StorageBandWidthLimiter *storageBandWidthLimiter,
                          ulong                   transmittedBytes,
                          uint64                  transmissionTime
                         )
{
  uint   z;
  ulong  averageBandWidth;   // average band width [bits/s]
  ulong  maxBandWidth;
  uint64 calculatedTime;
  uint64 delayTime;          // delay time [us]

  assert(storageBandWidthLimiter != NULL);

  if (storageBandWidthLimiter->maxBandWidthList != NULL)
  {
    storageBandWidthLimiter->measurementBytes += transmittedBytes;
    storageBandWidthLimiter->measurementTime += transmissionTime;
//fprintf(stderr,"%s, %d: sum %lu bytes %llu us\n",__FILE__,__LINE__,storageBandWidthLimiter->measurementBytes,storageBandWidthLimiter->measurementTime);

    if (storageBandWidthLimiter->measurementTime > MS_TO_US(100LL))   // too small time values are not reliable, thus accumlate over time
    {
      // calculate average band width
      averageBandWidth = 0;
      if (storageBandWidthLimiter->measurementCount > 0)
      {
        for (z = 0; z < storageBandWidthLimiter->measurementCount; z++)
        {
          averageBandWidth += storageBandWidthLimiter->measurements[z];
        }
        averageBandWidth /= storageBandWidthLimiter->measurementCount;
      }
      else
      {
        averageBandWidth = 0L;
      }
//fprintf(stderr,"%s, %d: averageBandWidth=%lu bits/s\n",__FILE__,__LINE__,averageBandWidth);

      // get max. band width to use
      maxBandWidth = getBandWidth(storageBandWidthLimiter->maxBandWidthList);

      // calculate delay time
      if (maxBandWidth > 0L)
      {
        calculatedTime = (BYTES_TO_BITS(storageBandWidthLimiter->measurementBytes)*US_PER_SECOND)/maxBandWidth;
        delayTime      = (calculatedTime > storageBandWidthLimiter->measurementTime) ? calculatedTime-storageBandWidthLimiter->measurementTime : 0LL;
      }
      else
      {
        // no band width limit -> no delay
        delayTime = 0LL;
      }

#if 0
/* disabled 2012-08-26: it seems the underlaying libssh2 implementation
   always transmit data in block sizes >32k? Thus it is not useful to
   reduce the block size, because this only cause additional overhead
   without any effect on the effectively used band width.
*/

      // recalculate optimal block size for band width limitation (hystersis: +-1024 bytes/s)
      if (maxBandWidth > 0L)
      {
        if     (   (averageBandWidth > (maxBandWidth+BYTES_TO_BITS(1024)))
                || (delayTime > 0LL)
               )
        {
          // recalculate max. block size to send to send a little less in a single step (if possible)
          if      ((storageBandWidthLimiter->blockSize > 32*1024) && (delayTime > 30000LL*MS_PER_SECOND)) storageBandWidthLimiter->blockSize -= 32*1024;
          else if ((storageBandWidthLimiter->blockSize > 16*1024) && (delayTime > 10000LL*MS_PER_SECOND)) storageBandWidthLimiter->blockSize -= 16*1024;
          else if ((storageBandWidthLimiter->blockSize >  8*1024) && (delayTime >  5000LL*MS_PER_SECOND)) storageBandWidthLimiter->blockSize -=  8*1024;
          else if ((storageBandWidthLimiter->blockSize >  4*1024) && (delayTime >  1000LL*MS_PER_SECOND)) storageBandWidthLimiter->blockSize -=  4*1024;
//fprintf(stderr,"%s,%d: storageBandWidthLimiter->measurementBytes=%ld storageBandWidthLimiter->max=%ld calculated time=%llu us storageBandWidthLimiter->measurementTime=%lu us blockSize=%ld\n",__FILE__,__LINE__,storageBandWidthLimiter->measurementBytes,storageBandWidthLimiter->max,calculatedTime,storageBandWidthLimiter->measurementTime,storageBandWidthLimiter->blockSize);
        }
        else if (averageBandWidth < (maxBandWidth-BYTES_TO_BITS(1024)))
        {
          // increase max. block size to send a little more in a single step (if possible)
          if (storageBandWidthLimiter->blockSize < (storageBandWidthLimiter->maxBlockSize-4*1024)) storageBandWidthLimiter->blockSize += 4*1024;
//fprintf(stderr,"%s,%d: ++ averageBandWidth=%lu bit/s storageBandWidthLimiter->max=%lu bit/s storageBandWidthLimiter->measurementTime=%llu us storageBandWidthLimiter->blockSize=%llu\n",__FILE__,__LINE__,averageBandWidth,storageBandWidthLimiter->max,storageBandWidthLimiter->measurementTime,storageBandWidthLimiter->blockSize);
        }
      }
#endif /* 0 */

      // delay if needed
      if (delayTime > 0)
      {
        Misc_udelay(delayTime);
      }

      // calculate and store new current bandwidth
      storageBandWidthLimiter->measurements[storageBandWidthLimiter->measurementNextIndex] = (ulong)((BYTES_TO_BITS((uint64)storageBandWidthLimiter->measurementBytes)*US_PER_SECOND)/(storageBandWidthLimiter->measurementTime+delayTime));
//fprintf(stderr,"%s, %d: new measurement[%d] %lu bits/us\n",__FILE__,__LINE__,storageBandWidthLimiter->measurementNextIndex,storageBandWidthLimiter->measurements[storageBandWidthLimiter->measurementNextIndex]);
      storageBandWidthLimiter->measurementNextIndex = (storageBandWidthLimiter->measurementNextIndex+1)%SIZE_OF_ARRAY(storageBandWidthLimiter->measurements);
      if (storageBandWidthLimiter->measurementCount < SIZE_OF_ARRAY(storageBandWidthLimiter->measurements)) storageBandWidthLimiter->measurementCount++;

      // reset accumulated measurement
      storageBandWidthLimiter->measurementBytes = 0L;
      storageBandWidthLimiter->measurementTime  = 0LL;
    }
  }
}
#endif /* defined(HAVE_CURL) || defined(HAVE_FTP) || defined(HAVE_SSH2) */

#ifdef HAVE_SSH2
/***********************************************************************\
* Name   : initDefaultSSHPassword
* Purpose: init default SSH password
* Input  : hostName   - host name
*          loginName  - login name
*          jobOptions - job options
* Output : -
* Return : TRUE if SSH password intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool initDefaultSSHPassword(ConstString hostName, ConstString loginName, const JobOptions *jobOptions)
{
  SemaphoreLock semaphoreLock;
  String        s;
  bool          initFlag;

  initFlag = FALSE;

  if (jobOptions != NULL)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
    {
      if (jobOptions->sshServer.password == NULL)
      {
        if (globalOptions.runMode == RUN_MODE_INTERACTIVE)
        {
          if (defaultSSHPassword == NULL)
          {
            Password *password = Password_new();
            s = !String_isEmpty(loginName)
                  ? String_format(String_new(),"SSH login password for %S@%S",loginName,hostName)
                  : String_format(String_new(),"SSH login password for %S",hostName);
            if (Password_input(password,String_cString(s),PASSWORD_INPUT_MODE_ANY))
            {
              defaultSSHPassword = password;
              initFlag = TRUE;
            }
            else
            {
                Password_delete(password);
            }
            String_delete(s);
          }
          else
          {
            initFlag = TRUE;
          }
        }
      }
      else
      {
        initFlag = TRUE;
      }
    }
  }

  return initFlag;
}

/***********************************************************************\
* Name   : checkSSHLogin
* Purpose: check if SSH login is possible
* Input  : hostName           - host name
*          hostPort           - host SSH port
*          loginName          - login name
*          loginPassword      - login password
*          publicKeyFileName  - SSH public key file name
*          privateKeyFileName - SSH private key file name
* Output : -
* Return : ERROR_NONE if login is possible, error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors checkSSHLogin(ConstString hostName,
                           uint        hostPort,
                           ConstString loginName,
                           Password    *loginPassword,
                           ConstString publicKeyFileName,
                           ConstString privateKeyFileName
                          )
{
  SocketHandle socketHandle;
  Errors       error;

  printInfo(5,"SSH: host %s:%d\n",String_cString(hostName),hostPort);
  printInfo(5,"SSH: public key '%s'\n",String_cString(publicKeyFileName));
  printInfo(5,"SSH: private key '%s'\n",String_cString(privateKeyFileName));
  error = Network_connect(&socketHandle,
                          SOCKET_TYPE_SSH,
                          hostName,
                          hostPort,
                          loginName,
                          loginPassword,
                          publicKeyFileName,
                          privateKeyFileName,
                          0
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }
  Network_disconnect(&socketHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : waitSSHSessionSocket
* Purpose: wait a little until SSH session socket can be read/write
* Input  : socketHandle - socket handle
* Output : -
* Return : TRUE if session socket can be read/write, FALSE on
+          error/timeout
* Notes  : -
\***********************************************************************/

LOCAL bool waitSSHSessionSocket(SocketHandle *socketHandle)
{
  LIBSSH2_SESSION *session;
  sigset_t        signalMask;
  struct timespec ts;
  fd_set          fdSet;

  assert(socketHandle != NULL);

  // get session
  session = Network_getSSHSession(socketHandle);
  assert(session != NULL);

  // Note: ignore SIGALRM in pselect()
  sigemptyset(&signalMask);
  sigaddset(&signalMask,SIGALRM);

  // wait for max. 60s
  ts.tv_sec  = 60L;
  ts.tv_nsec = 0L;
  FD_ZERO(&fdSet);
  FD_SET(socketHandle->handle,&fdSet);
  return (pselect(socketHandle->handle+1,
                 ((libssh2_session_block_directions(session) & LIBSSH2_SESSION_BLOCK_INBOUND ) != 0) ? &fdSet : NULL,
                 ((libssh2_session_block_directions(session) & LIBSSH2_SESSION_BLOCK_OUTBOUND) != 0) ? &fdSet : NULL,
                 NULL,
                 &ts,
                 &signalMask
                ) > 0
         );
}
#endif /* HAVE_SSH2 */

// ----------------------------------------------------------------------

#include "storage_ftp.c"
#include "storage_scp.c"
#include "storage_webdav.c"
#include "storage_optical.c"
#include "storage_device.c"

#ifdef HAVE_SSH2
/***********************************************************************\
* Name   : sftpSendCallback
* Purpose: sftp send callback: count total sent bytes and pass to
*          original function
* Input  : socket   - libssh2 socket
*          buffer   - buffer with data
*          length   - length to send
*          flags    - libssh2 flags
*          abstract - pointer to user data
* Output : -
* Return : number of bytes sent
* Notes  : parameters are hidden in LIBSSH2_SEND_FUNC()!
\***********************************************************************/

LOCAL LIBSSH2_SEND_FUNC(sftpSendCallback)
{
  StorageHandle *storageHandle;
  ssize_t       n;

  assert(abstract != NULL);

  storageHandle = *((StorageHandle**)abstract);
  assert(storageHandle != NULL);
  assert(storageHandle->sftp.oldSendCallback != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  n = storageHandle->sftp.oldSendCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageHandle->sftp.totalSentBytes += (uint64)n;

  return n;
}

/***********************************************************************\
* Name   : sftpReceiveCallback
* Purpose: sftp receive callback: count total received bytes and pass to
*          original function
* Input  : socket   - libssh2 socket
*          buffer   - buffer with data
*          length   - length to receive
*          flags    - libssh2 flags
*          abstract - pointer to user data
* Output : -
* Return : number of bytes received
* Notes  : parameters are hidden in LIBSSH2_RECV_FUNC()!
\***********************************************************************/

LOCAL LIBSSH2_RECV_FUNC(sftpReceiveCallback)
{
  StorageHandle *storageHandle;
  ssize_t       n;

  assert(abstract != NULL);

  storageHandle = *((StorageHandle**)abstract);
  assert(storageHandle != NULL);
  assert(storageHandle->sftp.oldReceiveCallback != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  n = storageHandle->sftp.oldReceiveCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageHandle->sftp.totalReceivedBytes += (uint64)n;

  return n;
}
#endif /* HAVE_SSH2 */

/*---------------------------------------------------------------------*/

Errors Storage_initAll(void)
{
  Errors error;

  error = ERROR_NONE;

  oldSignalAlarmHandler = signal(SIGALRM,signalHandler);
  #if defined(HAVE_SSH2)
    defaultSSHPassword = Password_new();
  #endif /* HAVE_SSH2 */

  #if   defined(HAVE_CURL)
    if (error == ERROR_NONE)
    {
      if (curl_global_init(CURL_GLOBAL_ALL) != 0)
      {
        error = ERROR_INIT_STORAGE;
      }
    }
  #endif
  #if defined(HAVE_CURL) || defined(HAVE_FTP)
    if (error == ERROR_NONE)
    {
      error = StorageFTP_initAll();
    }
  #endif /* HAVE_CURL || HAVE_FTP */
  #if defined(HAVE_SSH2)
    if (error == ERROR_NONE)
    {
      error = StorageSCP_initAll();
    }
  #endif /* HAVE_SSH2 */
  #if defined(HAVE_CURL)
    if (error == ERROR_NONE)
    {
      error = StorageWebDAV_initAll();
    }
  #endif /* HAVE_CURL */
  #ifdef HAVE_ISO9660
    if (error == ERROR_NONE)
    {
      error = StorageOptical_initAll();
    }
  #endif /* HAVE_ISO9660 */
  if (error == ERROR_NONE)
  {
    error = StorageDevice_initAll();
  }

  return error;
}

void Storage_doneAll(void)
{
  StorageDevice_doneAll();
  #ifdef HAVE_ISO9660
    StorageOptical_doneAll();
  #endif /* HAVE_ISO9660 */
  #ifdef HAVE_SSH2
    Password_delete(defaultSSHPassword);
  #endif /* HAVE_SSH2 */
  #if defined(HAVE_CURL)
    StorageWebDAV_doneAll();
  #endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */
  #if defined(HAVE_SSH2)
    StorageSCP_doneAll();
  #endif /* HAVE_SSH2 */
  #if defined(HAVE_CURL) || defined(HAVE_FTP)
    StorageFTP_doneAll();
  #endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */
  #if   defined(HAVE_CURL)
    curl_global_cleanup();
  #endif /* HAVE_CURL */

  #if defined(HAVE_SSH2)
    Password_delete(defaultSSHPassword);
  #endif /* HAVE_SSH2 */
  if (oldSignalAlarmHandler != SIG_ERR)
  {
    (void)signal(SIGALRM,oldSignalAlarmHandler);
  }
}

#ifdef NDEBUG
  void Storage_initSpecifier(StorageSpecifier *storageSpecifier)
#else /* not NDEBUG */
  void __Storage_initSpecifier(const char       *__fileName__,
                               ulong            __lineNb__,
                               StorageSpecifier *storageSpecifier
                              )
#endif /* NDEBUG */
{
  assert(storageSpecifier != NULL);

  storageSpecifier->hostName             = String_new();
  storageSpecifier->hostPort             = 0;
  storageSpecifier->loginName            = String_new();
  storageSpecifier->loginPassword        = Password_new();
  storageSpecifier->deviceName           = String_new();
  storageSpecifier->archiveName          = String_new();
  storageSpecifier->archivePatternString = String_new();
  storageSpecifier->storageName          = String_new();
  storageSpecifier->printableStorageName = String_new();

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE("storage specifier",storageSpecifier);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,"storage specifier",storageSpecifier);
  #endif /* NDEBUG */
}

#ifdef NDEBUG
  void Storage_duplicateSpecifier(StorageSpecifier       *destinationStorageSpecifier,
                                  const StorageSpecifier *sourceStorageSpecifier
                                 )
#else /* not NDEBUG */
  void __Storage_duplicateSpecifier(const char             *__fileName__,
                                    ulong                  __lineNb__,
                                    StorageSpecifier       *destinationStorageSpecifier,
                                    const StorageSpecifier *sourceStorageSpecifier
                                   )
#endif /* NDEBUG */
{
  assert(destinationStorageSpecifier != NULL);
  assert(sourceStorageSpecifier != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(sourceStorageSpecifier);

  destinationStorageSpecifier->type                 = sourceStorageSpecifier->type;
  destinationStorageSpecifier->hostName             = String_duplicate(sourceStorageSpecifier->hostName);
  destinationStorageSpecifier->hostPort             = sourceStorageSpecifier->hostPort;
  destinationStorageSpecifier->loginName            = String_duplicate(sourceStorageSpecifier->loginName);
  destinationStorageSpecifier->loginPassword        = Password_duplicate(sourceStorageSpecifier->loginPassword);
  destinationStorageSpecifier->deviceName           = String_duplicate(sourceStorageSpecifier->deviceName);
  destinationStorageSpecifier->archiveName          = String_duplicate(sourceStorageSpecifier->archiveName);
  destinationStorageSpecifier->archivePatternString = String_duplicate(sourceStorageSpecifier->archivePatternString);
  destinationStorageSpecifier->storageName          = String_new();
  destinationStorageSpecifier->printableStorageName = String_new();

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE("duplicated storage specifier",destinationStorageSpecifier);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,"duplicated storage specifier",destinationStorageSpecifier);
  #endif /* NDEBUG */
}

#ifdef NDEBUG
  void Storage_doneSpecifier(StorageSpecifier *storageSpecifier)
#else /* not NDEBUG */
  void __Storage_doneSpecifier(const char       *__fileName__,
                               ulong            __lineNb__,
                               StorageSpecifier *storageSpecifier
                              )
#endif /* NDEBUG */
{
  assert(storageSpecifier != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageSpecifier);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(storageSpecifier);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,storageSpecifier);
  #endif /* NDEBUG */

  String_delete(storageSpecifier->printableStorageName);
  String_delete(storageSpecifier->storageName);
  String_delete(storageSpecifier->archivePatternString);
  String_delete(storageSpecifier->archiveName);
  String_delete(storageSpecifier->deviceName);
  Password_delete(storageSpecifier->loginPassword);
  String_delete(storageSpecifier->loginName);
  String_delete(storageSpecifier->hostName);
}

bool Storage_parseFTPSpecifier(ConstString ftpSpecifier,
                               String      hostName,
                               uint        *hostPort,
                               String      loginName,
                               Password    *loginPassword
                              )
{
  assert(ftpSpecifier != NULL);
  assert(hostName != NULL);
  assert(loginName != NULL);

  return StorageFTP_parseSpecifier(ftpSpecifier,hostName,hostPort,loginName,loginPassword);
}

bool Storage_parseSCPSpecifier(ConstString scpSpecifier,
                               String      hostName,
                               uint        *hostPort,
                               String      loginName
                              )
{
  assert(scpSpecifier != NULL);
  assert(hostName != NULL);
  assert(loginName != NULL);

  return StorageSCP_parseSpecifier(scpSpecifier,hostName,hostPort,loginName);
}

bool Storage_parseSSHSpecifier(ConstString sshSpecifier,
                               String      hostName,
                               uint        *hostPort,
                               String      loginName
                              )
{
  const char* LOGINNAME_MAP_FROM[] = {"\\@"};
  const char* LOGINNAME_MAP_TO[]   = {"@"};

  bool   result;
  String s;

  assert(sshSpecifier != NULL);
  assert(hostName != NULL);
  assert(loginName != NULL);

  String_clear(hostName);
  if (hostPort != NULL) (*hostPort) = 0;
  String_clear(loginName);

  s = String_new();
  if      (String_matchCString(sshSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^:]+?):(\\d*)/{0,1}$",NULL,NULL,loginName,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <login name>@<host name>:<host port>
    if (loginName != NULL) String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (hostPort != NULL)
    {
      if (!String_isEmpty(s)) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);
    }

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^/]+)/{0,1}$",NULL,NULL,loginName,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>@<host name>
    if (loginName != NULL) String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^([^@:/]*?):(\\d*)/{0,1}$",NULL,NULL,hostName,s,NULL))
  {
    // <host name>:<host port>
    if (hostPort != NULL)
    {
      if (!String_isEmpty(s)) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);
    }

    result = TRUE;
  }
  else if (!String_isEmpty(sshSpecifier))
  {
    // <host name>
    String_set(hostName,sshSpecifier);

    result = TRUE;
  }
  else
  {
    result = FALSE;
  }
  String_delete(s);

  return result;
}

bool Storage_parseWebDAVSpecifier(ConstString webdavSpecifier,
                                  String      hostName,
                                  String      loginName,
                                  Password    *loginPassword
                                 )
{
  assert(webdavSpecifier != NULL);
  assert(hostName != NULL);
  assert(loginName != NULL);

  return StorageWebDAV_parseSpecifier(webdavSpecifier,hostName,loginName,loginPassword);
}

bool Storage_parseOpticalSpecifier(ConstString opticalSpecifier,
                                   ConstString defaultDeviceName,
                                   String      deviceName
                                  )
{
  assert(opticalSpecifier != NULL);
  assert(defaultDeviceName != NULL);
  assert(deviceName != NULL);

  return StorageOptical_parseSpecifier(opticalSpecifier,defaultDeviceName,deviceName);
}

bool Storage_parseDeviceSpecifier(ConstString deviceSpecifier,
                                  ConstString defaultDeviceName,
                                  String      deviceName
                                 )
{
  assert(deviceSpecifier != NULL);
  assert(defaultDeviceName != NULL);
  assert(deviceName != NULL);

  return StorageDevice_parseSpecifier(deviceSpecifier,defaultDeviceName,deviceName);
}

StorageTypes Storage_getType(ConstString storageName)
{
  StorageSpecifier storageSpecifier;
  StorageTypes     type;

  assert(storageName != NULL);

  Storage_initSpecifier(&storageSpecifier);
  if (Storage_parseName(&storageSpecifier,storageName) == ERROR_NONE)
  {
    type = storageSpecifier.type;
  }
  else
  {
    type = STORAGE_TYPE_UNKNOWN;
  }
  Storage_doneSpecifier(&storageSpecifier);

  return type;
}

Errors Storage_parseName(StorageSpecifier *storageSpecifier,
                         ConstString      storageName
                        )
{
  AutoFreeList    autoFreeList;
  String          string;
  String          archiveName;
  long            nextIndex;
  bool            hasPatternFlag;
  StringTokenizer archiveNameTokenizer;
  ConstString     token;

  assert(storageSpecifier != NULL);
  assert(storageName != NULL);

  // initialise variables
  AutoFree_init(&autoFreeList);
  string      = String_new();
  archiveName = String_new();
  AUTOFREE_ADD(&autoFreeList,&string,{ String_delete(string); });
  AUTOFREE_ADD(&autoFreeList,&archiveName,{ String_delete(archiveName); });

  String_clear(storageSpecifier->hostName);
  storageSpecifier->hostPort = 0;
  String_clear(storageSpecifier->loginName);
  Password_clear(storageSpecifier->loginPassword);
  String_clear(storageSpecifier->deviceName);

  if      (String_startsWithCString(storageName,"ftp://"))
  {
    if (   String_matchCString(storageName,6,"^[^:]+:([^@]|\\@)+?@[^:]+:\\d*/{0,1}",&nextIndex,NULL,NULL)  // ftp://<login name>:<login password>@<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,6,"^[^:]+:([^@]|\\@)+?@[^/]+/{0,1}",&nextIndex,NULL,NULL)       // ftp://<login name>:<login password>@<host name>/<file name>
        || String_matchCString(storageName,6,"^([^@]|\\@)+?@[^:]+:\\d*/{0,1}",&nextIndex,NULL,NULL)        // ftp://<login name>@<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,6,"^([^@]|\\@)+?@[^/]+/{0,1}",&nextIndex,NULL,NULL)             // ftp://<login name>@<host name>/<file name>
        || String_matchCString(storageName,6,"^[^:]+:\\d*/{0,1}",&nextIndex,NULL,NULL)                     // ftp://<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,6,"^[^/]+/{0,1}",&nextIndex,NULL,NULL)                          // ftp://<host name>/<file name>
       )
    {
      String_sub(string,storageName,6,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseFTPSpecifier(string,
                                     storageSpecifier->hostName,
                                     &storageSpecifier->hostPort,
                                     storageSpecifier->loginName,
                                     storageSpecifier->loginPassword
                                    )
         )
      {
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_FTP_SPECIFIER;
      }
      String_sub(archiveName,storageName,6+nextIndex,STRING_END);
    }
    else
    {
      // ftp://<file name>
      String_sub(archiveName,storageName,6,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_FTP;
  }
  else if (String_startsWithCString(storageName,"ssh://"))
  {
    if (   String_matchCString(storageName,6,"^([^@]|\\@)+?@[^:]+:\\d*/{0,1}",&nextIndex,NULL,NULL)   // ssh://<login name>@<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,6,"^([^@]|\\@)+?@[^/]+/{0,1}",&nextIndex,NULL,NULL)        // ssh://<login name>@<host name>/<file name>
        || String_matchCString(storageName,6,"^[^:]+:\\d*/{0,1}",&nextIndex,NULL,NULL)                // ssh://<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,6,"^[^/]+/{0,1}",&nextIndex,NULL,NULL)                     // ssh://<host name>/<file name>
       )
    {
      String_sub(string,storageName,6,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseSSHSpecifier(string,
                                     storageSpecifier->hostName,
                                     &storageSpecifier->hostPort,
                                     storageSpecifier->loginName
                                    )
         )
      {
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_SSH_SPECIFIER;
      }
      String_sub(archiveName,storageName,6+nextIndex,STRING_END);
    }
    else
    {
      // ssh://<file name>
      String_sub(archiveName,storageName,6,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_SSH;
  }
  else if (String_startsWithCString(storageName,"scp://"))
  {
    if (   String_matchCString(storageName,6,"^([^@]|\\@)+?@[^:]+:\\d*/{0,1}",&nextIndex,NULL,NULL)   // scp://<login name>@<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,6,"^([^@]|\\@)+?@[^/]+/{0,1}",&nextIndex,NULL,NULL)        // scp://<login name>@<host name>/<file name>
        || String_matchCString(storageName,6,"^[^:]+:\\d*/{0,1}",&nextIndex,NULL,NULL)                // scp://<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,6,"^[^/]+/{0,1}",&nextIndex,NULL,NULL)                     // scp://<host name>/<file name>
       )
    {
      String_sub(string,storageName,6,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseSSHSpecifier(string,
                                     storageSpecifier->hostName,
                                     &storageSpecifier->hostPort,
                                     storageSpecifier->loginName
                                    )
         )
      {
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_SSH_SPECIFIER;
      }
      String_sub(archiveName,storageName,6+nextIndex,STRING_END);
    }
    else
    {
      // scp://<file name>
      String_sub(archiveName,storageName,6,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_SCP;
  }
  else if (String_startsWithCString(storageName,"sftp://"))
  {
    if (   String_matchCString(storageName,7,"^([^@]|\\@)+?@[^:]+:\\d*/{0,1}",&nextIndex,NULL,NULL)   // sftp://<login name>@<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,7,"^([^@]|\\@)+?@[^/]+/{0,1}",&nextIndex,NULL,NULL)        // sftp://<login name>@<host name>/<file name>
        || String_matchCString(storageName,7,"^[^:]+:\\d*/{0,1}",&nextIndex,NULL,NULL)                // sftp://<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,7,"^[^/]+/{0,1}",&nextIndex,NULL,NULL)                     // sftp://<host name>/<file name>
       )
    {
      String_sub(string,storageName,7,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseSSHSpecifier(string,
                                     storageSpecifier->hostName,
                                     &storageSpecifier->hostPort,
                                     storageSpecifier->loginName
                                    )
         )
      {
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_SSH_SPECIFIER;
      }
      String_sub(archiveName,storageName,7+nextIndex,STRING_END);
    }
    else
    {
      // sftp://<file name>
      String_sub(archiveName,storageName,7,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_SFTP;
  }
  else if (String_startsWithCString(storageName,"webdav://"))
  {
    if (   String_matchCString(storageName,9,"^[^:]+:([^@]|\\@)+?@[^/]+/{0,1}",&nextIndex,NULL,NULL)  // webdav://<login name>:<login password>@<host name>/<file name>
        || String_matchCString(storageName,9,"^([^@]|\\@)+?@[^/]+/{0,1}",&nextIndex,NULL,NULL)        // webdav://<login name>@<host name>/<file name>
        || String_matchCString(storageName,9,"^[^/]+/{0,1}",&nextIndex,NULL,NULL)                     // webdav://<host name>/<file name>
       )
    {
      String_sub(string,storageName,9,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseWebDAVSpecifier(string,
                                        storageSpecifier->hostName,
                                        storageSpecifier->loginName,
                                        storageSpecifier->loginPassword
                                       )
         )
      {
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_WEBDAV_SPECIFIER;
      }
      String_sub(archiveName,storageName,9+nextIndex,STRING_END);
    }
    else
    {
      // webdav://<file name>
      String_sub(archiveName,storageName,9,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_WEBDAV;
  }
  else if (String_startsWithCString(storageName,"cd://"))
  {
    if (String_matchCString(storageName,5,"^[^:]+:[^/]*/{0,1}",&nextIndex,NULL,NULL))
    {
      // cd://<device name>:<file name>
      String_sub(string,storageName,5,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseDeviceSpecifier(string,
                                        NULL,
                                        storageSpecifier->deviceName
                                       )
         )
      {
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      String_sub(archiveName,storageName,5+nextIndex,STRING_END);
    }
    else
    {
      // cd://<file name>  AutoFreeList autoFreeList;
      String_sub(archiveName,storageName,5,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_CD;
  }
  else if (String_startsWithCString(storageName,"dvd://"))
  {
    if (String_matchCString(storageName,6,"^[^:]+:[^/]*/{0,1}",&nextIndex,NULL,NULL))
    {
      // dvd://<device name>:<file name>
      String_sub(string,storageName,6,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseDeviceSpecifier(string,
                                        NULL,
                                        storageSpecifier->deviceName
                                       )
         )
      {
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      String_sub(archiveName,storageName,6+nextIndex,STRING_END);
    }
    else
    {
      // dvd://<file name>
      String_sub(archiveName,storageName,6,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_DVD;
  }
  else if (String_startsWithCString(storageName,"bd://"))
  {
    if (String_matchCString(storageName,5,"^[^:]+:[^/]*/{0,1}",&nextIndex,NULL,NULL))
    {
      // bd://<device name>:<file name>
      String_sub(string,storageName,5,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseDeviceSpecifier(string,
                                        NULL,
                                        storageSpecifier->deviceName
                                       )
         )
      {
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      String_sub(archiveName,storageName,5+nextIndex,STRING_END);
    }
    else
    {
      // bd://<file name>
      String_sub(archiveName,storageName,5,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_BD;
  }
  else if (String_startsWithCString(storageName,"device://"))
  {
    if (String_matchCString(storageName,9,"^[^:]+:[^/]*/{0,1}",&nextIndex,NULL,NULL))
    {
      // device://<device name>:<file name>
      String_sub(string,storageName,9,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseDeviceSpecifier(string,
                                        NULL,
                                        storageSpecifier->deviceName
                                       )
         )
      {
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      String_sub(archiveName,storageName,9+nextIndex,STRING_END);
    }
    else
    {
      // device://<file name>
      String_sub(archiveName,storageName,9,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_DEVICE;
  }
  else if (String_startsWithCString(storageName,"file://"))
  {
    String_sub(archiveName,storageName,7,STRING_END);

    storageSpecifier->type = STORAGE_TYPE_FILESYSTEM;
  }
  else
  {
    String_set(archiveName,storageName);

    storageSpecifier->type = STORAGE_TYPE_FILESYSTEM;
  }

  // get base file name
  hasPatternFlag = FALSE;
  String_clear(storageSpecifier->archiveName);
  File_initSplitFileName(&archiveNameTokenizer,archiveName);
  {
    if (File_getNextSplitFileName(&archiveNameTokenizer,&token))
    {
      if (!Pattern_checkIsPattern(token))
      {
        if (!String_isEmpty(token))
        {
          File_setFileName(storageSpecifier->archiveName,token);
        }
        else
        {
          File_setFileNameChar(storageSpecifier->archiveName,FILE_SEPARATOR_CHAR);
        }
        while (File_getNextSplitFileName(&archiveNameTokenizer,&token) && !hasPatternFlag)
        {
          if (!Pattern_checkIsPattern(token))
          {
            File_appendFileName(storageSpecifier->archiveName,token);
          }
          else
          {
            hasPatternFlag = TRUE;
          }
        }
      }
      else
      {
        hasPatternFlag = TRUE;
      }
    }
  }
  File_doneSplitFileName(&archiveNameTokenizer);

  // get file pattern string
  String_clear(storageSpecifier->archivePatternString);
  if (hasPatternFlag)
  {
    File_initSplitFileName(&archiveNameTokenizer,archiveName);
    {
      while (File_getNextSplitFileName(&archiveNameTokenizer,&token))
      {
        File_appendFileName(storageSpecifier->archivePatternString,token);
      }
    }
    File_doneSplitFileName(&archiveNameTokenizer);
  }

  // free resources
  String_delete(archiveName);
  String_delete(string);
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

bool Storage_equalNames(ConstString storageName1,
                        ConstString storageName2
                       )
{
  StorageSpecifier storageSpecifier1,storageSpecifier2;
  bool             result;

  // parse storage names
  Storage_initSpecifier(&storageSpecifier1);
  Storage_initSpecifier(&storageSpecifier2);
  if (   (Storage_parseName(&storageSpecifier1,storageName1) != ERROR_NONE)
      || (Storage_parseName(&storageSpecifier2,storageName2) != ERROR_NONE)
     )
  {
    Storage_doneSpecifier(&storageSpecifier2);
    Storage_doneSpecifier(&storageSpecifier1);
    return FALSE;
  }

  // compare
  result = FALSE;
  if (storageSpecifier1.type == storageSpecifier2.type)
  {
    switch (storageSpecifier1.type)
    {
      case STORAGE_TYPE_FILESYSTEM:
        result = String_equals(storageSpecifier1.archiveName,storageSpecifier2.archiveName);
        break;
      case STORAGE_TYPE_FTP:
        result = StorageFTP_equalNames(&storageSpecifier1,&storageSpecifier2);
        break;
      case STORAGE_TYPE_SSH:
      case STORAGE_TYPE_SCP:
        result = StorageSCP_equalNames(&storageSpecifier1,&storageSpecifier2);
        break;
      case STORAGE_TYPE_SFTP:
      case STORAGE_TYPE_WEBDAV:
        result = StorageWebDAV_equalNames(&storageSpecifier1,&storageSpecifier2);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        result = StorageOptical_equalNames(&storageSpecifier1,&storageSpecifier2);
        break;
      case STORAGE_TYPE_DEVICE:
        result = StorageDevice_equalNames(&storageSpecifier1,&storageSpecifier2);
        break;
      default:
        break;
    }
  }

  // free resources
  Storage_doneSpecifier(&storageSpecifier2);
  Storage_doneSpecifier(&storageSpecifier1);

  return result;
}

String Storage_getName(StorageSpecifier *storageSpecifier,
                       ConstString      archiveName
                      )
{
  ConstString storageFileName;
  const char  *plainLoginPassword;

  assert(storageSpecifier != NULL);

  // get file to use
  if      (archiveName != NULL)
  {
    storageFileName = archiveName;
  }
  else if (storageSpecifier->archivePatternString != NULL)
  {
    storageFileName = storageSpecifier->archivePatternString;
  }
  else
  {
    storageFileName = storageSpecifier->archiveName;
  }

  String_clear(storageSpecifier->storageName);
  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!String_isEmpty(storageFileName))
      {
        String_append(storageSpecifier->storageName,storageFileName);
      }
      break;
    case STORAGE_TYPE_FTP:
      StorageFTP_getName(storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_SSH:
      if (!String_isEmpty(storageFileName))
      {
        String_append(storageSpecifier->storageName,storageFileName);
      }
      break;
    case STORAGE_TYPE_SCP:
      StorageSCP_getName(storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_SFTP:
      String_appendCString(storageSpecifier->storageName,"sftp://");
      if (!String_isEmpty(storageSpecifier->loginName))
      {
        String_append(storageSpecifier->storageName,storageSpecifier->loginName);
        if (!Password_isEmpty(storageSpecifier->loginPassword))
        {
          String_appendChar(storageSpecifier->storageName,':');
          plainLoginPassword = Password_deploy(storageSpecifier->loginPassword);
          String_appendCString(storageSpecifier->storageName,plainLoginPassword);
          Password_undeploy(storageSpecifier->loginPassword);
        }
        String_appendChar(storageSpecifier->storageName,'@');
      }
      String_append(storageSpecifier->storageName,storageSpecifier->hostName);
      if (!String_isEmpty(storageFileName))
      {
        String_appendChar(storageSpecifier->storageName,'/');
        String_append(storageSpecifier->storageName,storageFileName);
      }
      break;
    case STORAGE_TYPE_WEBDAV:
      StorageWebDAV_getName(storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      StorageOptical_getName(storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_DEVICE:
      StorageDevice_getName(storageSpecifier,archiveName);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return storageSpecifier->storageName;
}

const char *Storage_getNameCString(StorageSpecifier *storageSpecifier,
                                   ConstString      archiveName
                                  )
{
  return String_cString(Storage_getName(storageSpecifier,archiveName));
}

ConstString Storage_getPrintableName(StorageSpecifier *storageSpecifier,
                                     ConstString      archiveName
                                    )
{
  ConstString storageFileName;

  assert(storageSpecifier != NULL);

  // get file to use
  if      (!String_isEmpty(archiveName))
  {
    storageFileName = archiveName;
  }
  else if (!String_isEmpty(storageSpecifier->archivePatternString))
  {
    storageFileName = storageSpecifier->archivePatternString;
  }
  else
  {
    storageFileName = storageSpecifier->archiveName;
  }

  String_clear(storageSpecifier->storageName);
  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!String_isEmpty(storageFileName))
      {
        String_append(storageSpecifier->storageName,storageFileName);
      }
      break;
    case STORAGE_TYPE_FTP:
      StorageFTP_getPrintableName(storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_SSH:
      if (!String_isEmpty(storageFileName))
      {
        String_append(storageSpecifier->storageName,storageFileName);
      }
      break;
    case STORAGE_TYPE_SCP:
      StorageSCP_getPrintableName(storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_SFTP:
      String_appendCString(storageSpecifier->storageName,"sftp://");
      String_append(storageSpecifier->storageName,storageSpecifier->hostName);
      if ((storageSpecifier->hostPort != 0) && (storageSpecifier->hostPort != 22))
      {
        String_format(storageSpecifier->storageName,":%d",storageSpecifier->hostPort);
      }
      if (!String_isEmpty(storageFileName))
      {
        String_appendChar(storageSpecifier->storageName,'/');
        String_append(storageSpecifier->storageName,storageFileName);
      }
      break;
    case STORAGE_TYPE_WEBDAV:
      StorageWebDAV_getPrintableName(storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      StorageOptical_getPrintableName(storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_DEVICE:
      StorageDevice_getPrintableName(storageSpecifier,archiveName);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return storageSpecifier->storageName;
}

const char *Storage_getPrintableNameCString(StorageSpecifier *storageSpecifier,
                                            ConstString      archiveName
                                           )
{
  return String_cString(Storage_getPrintableName(storageSpecifier,archiveName));
}

#ifdef NDEBUG
  Errors Storage_init(StorageHandle                *storageHandle,
                      const StorageSpecifier       *storageSpecifier,
                      const JobOptions             *jobOptions,
                      BandWidthList                *maxBandWidthList,
                      ServerConnectionPriorities   serverConnectionPriority,
                      StorageRequestVolumeFunction storageRequestVolumeFunction,
                      void                         *storageRequestVolumeUserData,
                      StorageStatusInfoFunction    storageStatusInfoFunction,
                      void                         *storageStatusInfoUserData
                     )
#else /* not NDEBUG */
  Errors __Storage_init(const char                   *__fileName__,
                        ulong                        __lineNb__,
                        StorageHandle                *storageHandle,
                        const StorageSpecifier       *storageSpecifier,
                        const JobOptions             *jobOptions,
                        BandWidthList                *maxBandWidthList,
                        ServerConnectionPriorities   serverConnectionPriority,
                        StorageRequestVolumeFunction storageRequestVolumeFunction,
                        void                         *storageRequestVolumeUserData,
                        StorageStatusInfoFunction    storageStatusInfoFunction,
                        void                         *storageStatusInfoUserData
                       )
#endif /* NDEBUG */
{
  AutoFreeList autoFreeList;
  Errors       error;

  assert(storageHandle != NULL);
  assert(storageSpecifier != NULL);

  #if !defined(HAVE_CURL) && !defined(HAVE_FTP) && !defined(HAVE_SSH2)
    UNUSED_VARIABLE(serverConnectionPriority);
  #endif /* !defined(HAVE_CURL) && !defined(HAVE_FTP) && !defined(HAVE_SSH2) */

  // initialize variables
  AutoFree_init(&autoFreeList);
  storageHandle->mode                      = STORAGE_MODE_UNKNOWN;
  Storage_duplicateSpecifier(&storageHandle->storageSpecifier,storageSpecifier);
  storageHandle->jobOptions                = jobOptions;
  storageHandle->mountedDeviceFlag         = FALSE;
  storageHandle->requestVolumeFunction     = storageRequestVolumeFunction;
  storageHandle->requestVolumeUserData     = storageRequestVolumeUserData;
  if ((jobOptions != NULL) && jobOptions->waitFirstVolumeFlag)
  {
    storageHandle->volumeNumber            = 0;
    storageHandle->requestedVolumeNumber   = 0;
    storageHandle->volumeState             = STORAGE_VOLUME_STATE_UNKNOWN;
  }
  else
  {
    storageHandle->volumeNumber            = 1;
    storageHandle->requestedVolumeNumber   = 1;
    storageHandle->volumeState             = STORAGE_VOLUME_STATE_LOADED;
  }
  storageHandle->storageStatusInfoFunction = storageStatusInfoFunction;
  storageHandle->storageStatusInfoUserData = storageStatusInfoUserData;
  AUTOFREE_ADD(&autoFreeList,&storageHandle->storageSpecifier,{ Storage_doneSpecifier(&storageHandle->storageSpecifier); });

  // mount device if needed
  if ((jobOptions != NULL) && !String_isEmpty(jobOptions->mountDeviceName))
  {
    if (!Device_isMounted(jobOptions->mountDeviceName))
    {
      error = Device_mount(jobOptions->mountDeviceName);
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      storageHandle->mountedDeviceFlag = TRUE;
    }
  }

  // init protocol specific values
  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      UNUSED_VARIABLE(maxBandWidthList);

      break;
    case STORAGE_TYPE_FILESYSTEM:
      UNUSED_VARIABLE(maxBandWidthList);
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_init(storageHandle,storageSpecifier,jobOptions,maxBandWidthList,serverConnectionPriority);
      break;
    case STORAGE_TYPE_SSH:
      UNUSED_VARIABLE(maxBandWidthList);

      AutoFree_cleanup(&autoFreeList);
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_init(storageHandle,storageSpecifier,jobOptions,maxBandWidthList,serverConnectionPriority);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          SSHServer sshServer;

          // init variables
          storageHandle->sftp.sshPublicKeyFileName   = NULL;
          storageHandle->sftp.sshPrivateKeyFileName  = NULL;
          storageHandle->sftp.oldSendCallback        = NULL;
          storageHandle->sftp.oldReceiveCallback     = NULL;
          storageHandle->sftp.totalSentBytes         = 0LL;
          storageHandle->sftp.totalReceivedBytes     = 0LL;
          storageHandle->sftp.sftp                   = NULL;
          storageHandle->sftp.sftpHandle             = NULL;
          storageHandle->sftp.readAheadBuffer.offset = 0LL;
          storageHandle->sftp.readAheadBuffer.length = 0L;
          initBandWidthLimiter(&storageHandle->sftp.bandWidthLimiter,maxBandWidthList);

          // allocate read-ahead buffer
          storageHandle->sftp.readAheadBuffer.data = (byte*)malloc(MAX_BUFFER_SIZE);
          if (storageHandle->sftp.readAheadBuffer.data == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          AUTOFREE_ADD(&autoFreeList,storageHandle->sftp.readAheadBuffer.data,{ free(storageHandle->sftp.readAheadBuffer.data); });

          // get SSH server settings
          storageHandle->sftp.server = getSSHServerSettings(storageHandle->storageSpecifier.hostName,jobOptions,&sshServer);
          if (String_isEmpty(storageHandle->storageSpecifier.loginName)) String_set(storageHandle->storageSpecifier.loginName,sshServer.loginName);
          if (String_isEmpty(storageHandle->storageSpecifier.loginName)) String_setCString(storageHandle->storageSpecifier.loginName,getenv("LOGNAME"));
          if (String_isEmpty(storageHandle->storageSpecifier.loginName)) String_setCString(storageHandle->storageSpecifier.loginName,getenv("USER"));
          if (storageHandle->storageSpecifier.hostPort == 0) storageHandle->storageSpecifier.hostPort = sshServer.port;
          storageHandle->sftp.sshPublicKeyFileName  = sshServer.publicKeyFileName;
          storageHandle->sftp.sshPrivateKeyFileName = sshServer.privateKeyFileName;
          if (String_isEmpty(storageHandle->storageSpecifier.hostName))
          {
            AutoFree_cleanup(&autoFreeList);
            return ERROR_NO_HOST_NAME;
          }

          // allocate SSH server
          if (!allocateServer(storageHandle->sftp.server,serverConnectionPriority,60*1000L))
          {
            AutoFree_cleanup(&autoFreeList);
            return ERROR_TOO_MANY_CONNECTIONS;
          }
          AUTOFREE_ADD(&autoFreeList,storageHandle->sftp.server,{ freeServer(storageHandle->sftp.server); });

          // check if SSH login is possible
          error = ERROR_UNKNOWN;
          if ((error != ERROR_NONE) && (sshServer.password != NULL))
          {
            error = checkSSHLogin(storageHandle->storageSpecifier.hostName,
                                  storageHandle->storageSpecifier.hostPort,
                                  storageHandle->storageSpecifier.loginName,
                                  sshServer.password,
                                  storageHandle->sftp.sshPublicKeyFileName,
                                  storageHandle->sftp.sshPrivateKeyFileName
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(storageHandle->storageSpecifier.loginPassword,sshServer.password);
            }
          }
          if (error != ERROR_NONE)
          {
            // initialize default password
            if (   initDefaultSSHPassword(storageHandle->storageSpecifier.hostName,storageHandle->storageSpecifier.loginName,jobOptions)
                && !Password_isEmpty(defaultSSHPassword)
               )
            {
              error = checkSSHLogin(storageHandle->storageSpecifier.hostName,
                                    storageHandle->storageSpecifier.hostPort,
                                    storageHandle->storageSpecifier.loginName,
                                    defaultSSHPassword,
                                    storageHandle->sftp.sshPublicKeyFileName,
                                    storageHandle->sftp.sshPrivateKeyFileName
                                   );
              if (error == ERROR_NONE)
              {
                Password_set(storageHandle->storageSpecifier.loginPassword,defaultSSHPassword);
              }
            }
            else
            {
              error = !Password_isEmpty(defaultSSHPassword) ? ERROR_INVALID_SSH_PASSWORD : ERROR_NO_SSH_PASSWORD;
            }
          }
          assert(error != ERROR_UNKNOWN);
          if (error != ERROR_NONE)
          {
            AutoFree_cleanup(&autoFreeList);
            return error;
          }

          // free resources
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(maxBandWidthList);

        AutoFree_cleanup(&autoFreeList);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_init(storageHandle,storageSpecifier,jobOptions,maxBandWidthList,serverConnectionPriority);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_init(storageHandle,storageSpecifier,jobOptions);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_init(storageHandle,storageSpecifier,jobOptions);
      break;
    default:
      UNUSED_VARIABLE(maxBandWidthList);

      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  storageHandle->runningInfo.volumeNumber   = 0;
  storageHandle->runningInfo.volumeProgress = 0;

  // free resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE("storage handle",storageHandle);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,"storage handle",storageHandle);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Storage_done(StorageHandle *storageHandle)
#else /* not NDEBUG */
  Errors __Storage_done(const char    *__fileName__,
                        ulong         __lineNb__,
                        StorageHandle *storageHandle
                       )
#endif /* NDEBUG */
{
  Errors error;
  Errors tmpError;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(storageHandle);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,storageHandle);
  #endif /* NDEBUG */

  error = ERROR_NONE;

  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_done(storageHandle);
      break;
    case STORAGE_TYPE_SSH:
      // free SSH server connection
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_done(storageHandle);
      break;
    case STORAGE_TYPE_SFTP:
      // free SSH server connection
      #ifdef HAVE_SSH2
        freeServer(storageHandle->sftp.server);
        free(storageHandle->sftp.readAheadBuffer.data);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_done(storageHandle);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_done(storageHandle);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_done(storageHandle);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  // unmount device if mounted before
  if ((storageHandle->jobOptions != NULL) && storageHandle->mountedDeviceFlag)
  {
    tmpError = Device_umount(storageHandle->jobOptions->mountDeviceName);
    if (tmpError != ERROR_NONE)
    {
      if (error == ERROR_NONE) error = tmpError;
    }
  }

  Storage_doneSpecifier(&storageHandle->storageSpecifier);

  return error;
}

bool Storage_isServerAllocationPending(StorageHandle *storageHandle)
{
  bool serverAllocationPending;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  serverAllocationPending = FALSE;
  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      break;
    case STORAGE_TYPE_FTP:
      serverAllocationPending = StorageFTP_isServerAllocationPending(storageHandle);
      break;
    case STORAGE_TYPE_SSH:
      #if defined(HAVE_SSH2)
        serverAllocationPending = isServerAllocationPending(storageHandle->ssh.server);
      #else /* not HAVE_SSH2 */
        serverAllocationPending = FALSE;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      serverAllocationPending = StorageSCP_isServerAllocationPending(storageHandle);
      break;
    case STORAGE_TYPE_SFTP:
      #if defined(HAVE_SSH2)
        serverAllocationPending = isServerAllocationPending(storageHandle->sftp.server);
      #else /* not HAVE_SSH2 */
        serverAllocationPending = FALSE;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      serverAllocationPending = StorageWebDAV_isServerAllocationPending(storageHandle);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      break;
    case STORAGE_TYPE_DEVICE:
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return serverAllocationPending;
}

const StorageSpecifier *Storage_getStorageSpecifier(const StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);

  return &storageHandle->storageSpecifier;
}

Errors Storage_preProcess(StorageHandle *storageHandle,
                          bool          initialFlag
                         )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  error = ERROR_NONE;
  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      {
        TextMacro textMacros[2];

        if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
        {
          if (!initialFlag)
          {
            // init macros
            TEXT_MACRO_N_STRING (textMacros[0],"%file",  storageHandle->fileSystem.fileHandle.name);
            TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageHandle->volumeNumber              );

            if (globalOptions.file.writePreProcessCommand != NULL)
            {
              // write pre-processing
              if (error == ERROR_NONE)
              {
                printInfo(0,"Write pre-processing...");
                error = Misc_executeCommand(String_cString(globalOptions.file.writePreProcessCommand),
                                            textMacros,
                                            SIZE_OF_ARRAY(textMacros),
                                            CALLBACK(executeIOOutput,NULL),
                                            CALLBACK(executeIOOutput,NULL)
                                           );
                printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
              }
            }
            if (error != ERROR_NONE)
            {
              break;
            }
          }
        }
      }
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_preProcess(storageHandle,initialFlag);
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_preProcess(storageHandle,initialFlag);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          TextMacro textMacros[1];

          if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
          {
            if (!initialFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageHandle->volumeNumber              );

              if (globalOptions.sftp.writePreProcessCommand != NULL)
              {
                // write pre-processing
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write pre-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.sftp.writePreProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              CALLBACK(executeIOOutput,NULL),
                                              CALLBACK(executeIOOutput,NULL)
                                             );
                  printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
                }
              }
              if (error != ERROR_NONE)
              {
                break;
              }
            }
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_preProcess(storageHandle,initialFlag);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_preProcess(storageHandle,initialFlag);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_preProcess(storageHandle,initialFlag);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

Errors Storage_postProcess(StorageHandle *storageHandle,
                           bool          finalFlag
                          )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  error = ERROR_NONE;
  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      {
        TextMacro textMacros[2];

        if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
        {
          if (!finalFlag)
          {
            // init macros
            TEXT_MACRO_N_STRING (textMacros[0],"%file",  storageHandle->fileSystem.fileHandle.name);
            TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageHandle->volumeNumber              );

            if (globalOptions.file.writePostProcessCommand != NULL)
            {
              // write post-process
              if (error == ERROR_NONE)
              {
                printInfo(0,"Write post-processing...");
                error = Misc_executeCommand(String_cString(globalOptions.file.writePostProcessCommand),
                                            textMacros,
                                            SIZE_OF_ARRAY(textMacros),
                                            CALLBACK(executeIOOutput,NULL),
                                            CALLBACK(executeIOOutput,NULL)
                                           );
                printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
              }
            }
            if (error != ERROR_NONE)
            {
              break;
            }
          }
        }
      }
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_postProcess(storageHandle,finalFlag);
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_postProcess(storageHandle,finalFlag);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          TextMacro textMacros[1];

          if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
          {
            if (!finalFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageHandle->volumeNumber);

              if (globalOptions.sftp.writePostProcessCommand != NULL)
              {
                // write post-process
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write post-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.sftp.writePostProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              CALLBACK(executeIOOutput,NULL),
                                              CALLBACK(executeIOOutput,NULL)
                                             );
                  printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
                }
              }
              if (error != ERROR_NONE)
              {
                break;
              }
            }
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_postProcess(storageHandle,finalFlag);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_postProcess(storageHandle,finalFlag);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_postProcess(storageHandle,finalFlag);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
       #endif /* NDEBUG */
      break;
  }

  return error;
}

uint Storage_getVolumeNumber(const StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);

  return storageHandle->volumeNumber;
}

void Storage_setVolumeNumber(StorageHandle *storageHandle,
                             uint          volumeNumber
                            )
{
  assert(storageHandle != NULL);

  storageHandle->volumeNumber = volumeNumber;
}

Errors Storage_unloadVolume(StorageHandle *storageHandle)
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  error = ERROR_UNKNOWN;
  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
    case STORAGE_TYPE_FILESYSTEM:
      error = ERROR_NONE;
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_unloadVolume(storageHandle);
      break;
    case STORAGE_TYPE_SSH:
    case STORAGE_TYPE_SCP:
      error = StorageSCP_unloadVolume(storageHandle);
      break;
    case STORAGE_TYPE_SFTP:
      error = ERROR_NONE;
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_unloadVolume(storageHandle);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_unloadVolume(storageHandle);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_unloadVolume(storageHandle);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

Errors Storage_create(StorageHandle *storageHandle,
                      ConstString   archiveName,
                      uint64        archiveSize
                     )
{
  Errors error;
  String directoryName;

  assert(storageHandle != NULL);
  assert(archiveName != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  // init variables
  storageHandle->mode = STORAGE_MODE_WRITE;

  // check if archive name given
  if (String_isEmpty(storageHandle->storageSpecifier.archiveName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      // check if archive file exists
      if ((storageHandle->jobOptions != NULL) && !storageHandle->jobOptions->overwriteArchiveFilesFlag && File_exists(archiveName))
      {
        return ERRORX_(FILE_EXISTS_,0,String_cString(archiveName));
      }

      if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
      {
        // create directory if not existing
        directoryName = File_getFilePathName(String_new(),archiveName);
        if (!String_isEmpty(directoryName) && !File_exists(directoryName))
        {
          error = File_makeDirectory(directoryName,
                                     FILE_DEFAULT_USER_ID,
                                     FILE_DEFAULT_GROUP_ID,
                                     FILE_DEFAULT_PERMISSION
                                    );
          if (error != ERROR_NONE)
          {
            String_delete(directoryName);
            return error;
          }
        }
        String_delete(directoryName);

        // open file
        error = File_open(&storageHandle->fileSystem.fileHandle,
                          archiveName,
                          FILE_OPEN_CREATE
                         );
        if (error != ERROR_NONE)
        {
          return error;
        }

        DEBUG_ADD_RESOURCE_TRACE("storage create file",&storageHandle->fileSystem);
      }
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_create(storageHandle,archiveName,archiveSize);
      break;
    case STORAGE_TYPE_SSH:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_create(storageHandle,archiveName,archiveSize);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          // connect
          error = Network_connect(&storageHandle->sftp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageHandle->storageSpecifier.hostName,
                                  storageHandle->storageSpecifier.hostPort,
                                  storageHandle->storageSpecifier.loginName,
                                  storageHandle->storageSpecifier.loginPassword,
                                  storageHandle->sftp.sshPublicKeyFileName,
                                  storageHandle->sftp.sshPrivateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }
          libssh2_session_set_timeout(Network_getSSHSession(&storageHandle->sftp.socketHandle),READ_TIMEOUT);

          // install send/receive callback to track number of sent/received bytes
          storageHandle->sftp.totalSentBytes     = 0LL;
          storageHandle->sftp.totalReceivedBytes = 0LL;
          (*(libssh2_session_abstract(Network_getSSHSession(&storageHandle->sftp.socketHandle)))) = storageHandle;
          storageHandle->sftp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->sftp.socketHandle),LIBSSH2_CALLBACK_SEND,sftpSendCallback   );
          storageHandle->sftp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->sftp.socketHandle),LIBSSH2_CALLBACK_RECV,sftpReceiveCallback);

          // init session
          storageHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageHandle->sftp.socketHandle));
          if (storageHandle->sftp.sftp == NULL)
          {
            char *sshErrorText;

            libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&sshErrorText,NULL,0);
            error = ERRORX_(SSH,
                            libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle)),
                            sshErrorText
                           );
            Network_disconnect(&storageHandle->sftp.socketHandle);
            return error;
          }

          if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
          {
            // create file
            storageHandle->sftp.sftpHandle = libssh2_sftp_open(storageHandle->sftp.sftp,
                                                                   String_cString(archiveName),
                                                                   LIBSSH2_FXF_CREAT|LIBSSH2_FXF_WRITE|LIBSSH2_FXF_TRUNC,
// ???
LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR
                                                                  );
            if (storageHandle->sftp.sftpHandle == NULL)
            {
              char *sshErrorText;

              libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&sshErrorText,NULL,0);
              error = ERRORX_(SSH,
                              libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle)),
                              sshErrorText
                             );
              libssh2_sftp_shutdown(storageHandle->sftp.sftp);
              Network_disconnect(&storageHandle->sftp.socketHandle);
              return error;
            }
          }

          DEBUG_ADD_RESOURCE_TRACE("storage create sftp",&storageHandle->sftp);
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(fileSize);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_create(storageHandle,archiveName,archiveSize);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_create(storageHandle,archiveName,archiveSize);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_create(storageHandle,archiveName,archiveSize);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return ERROR_NONE;
}

Errors Storage_open(StorageHandle *storageHandle, ConstString archiveName)
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  // init variables
  storageHandle->mode = STORAGE_MODE_READ;

  // get file name
  if (archiveName == NULL) archiveName = storageHandle->storageSpecifier.archiveName;
  if (String_isEmpty(archiveName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      // check if file exists
      if (!File_exists(archiveName))
      {
        return ERRORX_(FILE_NOT_FOUND_,0,String_cString(archiveName));
      }

      // open file
      error = File_open(&storageHandle->fileSystem.fileHandle,
                        archiveName,
                        FILE_OPEN_READ
                       );
      if (error != ERROR_NONE)
      {
        return error;
      }

      DEBUG_ADD_RESOURCE_TRACE("storage open file",&storageHandle->fileSystem);
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_open(storageHandle,archiveName);
      break;
    case STORAGE_TYPE_SSH:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_open(storageHandle,archiveName);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;

          // init variables
          storageHandle->sftp.index                  = 0LL;
          storageHandle->sftp.readAheadBuffer.offset = 0LL;
          storageHandle->sftp.readAheadBuffer.length = 0L;

          // connect
          error = Network_connect(&storageHandle->sftp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageHandle->storageSpecifier.hostName,
                                  storageHandle->storageSpecifier.hostPort,
                                  storageHandle->storageSpecifier.loginName,
                                  storageHandle->storageSpecifier.loginPassword,
                                  storageHandle->sftp.sshPublicKeyFileName,
                                  storageHandle->sftp.sshPrivateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }
          libssh2_session_set_timeout(Network_getSSHSession(&storageHandle->sftp.socketHandle),READ_TIMEOUT);

          // install send/receive callback to track number of sent/received bytes
          storageHandle->sftp.totalSentBytes     = 0LL;
          storageHandle->sftp.totalReceivedBytes = 0LL;
          (*(libssh2_session_abstract(Network_getSSHSession(&storageHandle->sftp.socketHandle)))) = storageHandle;
          storageHandle->sftp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->sftp.socketHandle),LIBSSH2_CALLBACK_SEND,sftpSendCallback   );
          storageHandle->sftp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->sftp.socketHandle),LIBSSH2_CALLBACK_RECV,sftpReceiveCallback);

          // init session
          storageHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageHandle->sftp.socketHandle));
          if (storageHandle->sftp.sftp == NULL)
          {
            error = ERROR_(SSH,libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle)));
            Network_disconnect(&storageHandle->sftp.socketHandle);
            return error;
          }

          // open file
          storageHandle->sftp.sftpHandle = libssh2_sftp_open(storageHandle->sftp.sftp,
                                                                 String_cString(archiveName),
                                                                 LIBSSH2_FXF_READ,
                                                                 0
                                                                );
          if (storageHandle->sftp.sftpHandle == NULL)
          {
            char *sshErrorText;

            libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&sshErrorText,NULL,0);
            error = ERRORX_(SSH,
                            libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle)),
                            sshErrorText
                           );
            libssh2_sftp_shutdown(storageHandle->sftp.sftp);
            Network_disconnect(&storageHandle->sftp.socketHandle);
            return error;
          }

          // get file size
          if (libssh2_sftp_fstat(storageHandle->sftp.sftpHandle,
                                 &sftpAttributes
                                ) != 0
             )
          {
            char *sshErrorText;

            libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&sshErrorText,NULL,0);
            error = ERRORX_(SSH,
                            libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle)),
                            sshErrorText
                           );
            libssh2_sftp_close(storageHandle->sftp.sftpHandle);
            libssh2_sftp_shutdown(storageHandle->sftp.sftp);
            Network_disconnect(&storageHandle->sftp.socketHandle);
            return error;
          }
          storageHandle->sftp.size = sftpAttributes.filesize;

          DEBUG_ADD_RESOURCE_TRACE("storage open sftp",&storageHandle->sftp);
        }
      #else /* not HAVE_SSH2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_open(storageHandle,archiveName);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_open(storageHandle,archiveName);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_open(storageHandle,archiveName);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return ERROR_NONE;
}

void Storage_close(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->fileSystem);

      switch (storageHandle->mode)
      {
        case STORAGE_MODE_UNKNOWN:
          break;
        case STORAGE_MODE_WRITE:
          if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
          {
            File_close(&storageHandle->fileSystem.fileHandle);
          }
          break;
        case STORAGE_MODE_READ:
          File_close(&storageHandle->fileSystem.fileHandle);
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
      break;
    case STORAGE_TYPE_FTP:
      StorageFTP_close(storageHandle);
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_SCP:
      StorageSCP_close(storageHandle);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->sftp);

        switch (storageHandle->mode)
        {
          case STORAGE_MODE_UNKNOWN:
            break;
          case STORAGE_MODE_WRITE:
            if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
            {
              (void)libssh2_sftp_close(storageHandle->sftp.sftpHandle);
            }
            break;
          case STORAGE_MODE_READ:
            (void)libssh2_sftp_close(storageHandle->sftp.sftpHandle);
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
          #endif /* NDEBUG */
        }
        libssh2_sftp_shutdown(storageHandle->sftp.sftp);
        Network_disconnect(&storageHandle->sftp.socketHandle);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      StorageWebDAV_close(storageHandle);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      StorageOptical_close(storageHandle);
      break;
    case STORAGE_TYPE_DEVICE:
      StorageDevice_close(storageHandle);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
}

bool Storage_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
      {
        return File_eof(&storageHandle->fileSystem.fileHandle);
      }
      else
      {
        return TRUE;
      }
      break;
    case STORAGE_TYPE_FTP:
      return StorageFTP_eof(storageHandle);
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      return StorageSCP_eof(storageHandle);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
        {
          return storageHandle->sftp.index >= storageHandle->sftp.size;
        }
        else
        {
          return TRUE;
        }
      #else /* not HAVE_SSH2 */
        return TRUE;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      return StorageWebDAV_eof(storageHandle);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      return StorageOptical_eof(storageHandle);
      break;
    case STORAGE_TYPE_DEVICE:
      return StorageDevice_eof(storageHandle);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return TRUE;
}

Errors Storage_read(StorageHandle *storageHandle,
                    void          *buffer,
                    ulong         size,
                    ulong         *bytesRead
                   )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(buffer != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

//fprintf(stderr,"%s,%d: size=%lu\n",__FILE__,__LINE__,size);
  if (bytesRead != NULL) (*bytesRead) = 0L;
  error = ERROR_NONE;
  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
      {
        error = File_read(&storageHandle->fileSystem.fileHandle,buffer,size,bytesRead);
      }
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_read(storageHandle,buffer,size,bytesRead);
      break;
    case STORAGE_TYPE_SSH:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_read(storageHandle,buffer,size,bytesRead);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          ulong   index;
          ulong   bytesAvail;
          ulong   length;
          uint64  startTimestamp,endTimestamp;
          uint64  startTotalReceivedBytes,endTotalReceivedBytes;
          ssize_t n;

          if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
          {
            assert(storageHandle->sftp.sftpHandle != NULL);
            assert(storageHandle->sftp.readAheadBuffer.data != NULL);

            while (   (size > 0)
                   && (error == ERROR_NONE)
                  )
            {
              // copy as much data as available from read-ahead buffer
              if (   (storageHandle->sftp.index >= storageHandle->sftp.readAheadBuffer.offset)
                  && (storageHandle->sftp.index < (storageHandle->sftp.readAheadBuffer.offset+storageHandle->sftp.readAheadBuffer.length))
                 )
              {
                // copy data from read-ahead buffer
                index      = (ulong)(storageHandle->sftp.index-storageHandle->sftp.readAheadBuffer.offset);
                bytesAvail = MIN(size,storageHandle->sftp.readAheadBuffer.length-index);
                memcpy(buffer,storageHandle->sftp.readAheadBuffer.data+index,bytesAvail);

                // adjust buffer, size, bytes read, index
                buffer = (byte*)buffer+bytesAvail;
                size -= bytesAvail;
                if (bytesRead != NULL) (*bytesRead) += bytesAvail;
                storageHandle->sftp.index += (uint64)bytesAvail;
              }

              // read rest of data
              if (size > 0)
              {
                assert(storageHandle->sftp.index >= (storageHandle->sftp.readAheadBuffer.offset+storageHandle->sftp.readAheadBuffer.length));

                // get max. number of bytes to receive in one step
                if (storageHandle->sftp.bandWidthLimiter.maxBandWidthList != NULL)
                {
                  length = MIN(storageHandle->sftp.bandWidthLimiter.blockSize,size);
                }
                else
                {
                  length = size;
                }
                assert(length > 0L);

                // get start time, start received bytes
                startTimestamp          = Misc_getTimestamp();
                startTotalReceivedBytes = storageHandle->sftp.totalReceivedBytes;

                #if   defined(HAVE_SSH2_SFTP_SEEK64)
                  libssh2_sftp_seek64(storageHandle->sftp.sftpHandle,storageHandle->sftp.index);
                #elif defined(HAVE_SSH2_SFTP_SEEK2)
                  libssh2_sftp_seek2(storageHandle->sftp.sftpHandle,storageHandle->sftp.index);
                #else /* not HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
                  libssh2_sftp_seek(storageHandle->sftp.sftpHandle,storageHandle->sftp.index);
                #endif /* HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */

                if (length <= MAX_BUFFER_SIZE)
                {
                  // read into read-ahead buffer
                  n = libssh2_sftp_read(storageHandle->sftp.sftpHandle,
                                        (char*)storageHandle->sftp.readAheadBuffer.data,
                                        MIN((size_t)(storageHandle->sftp.size-storageHandle->sftp.index),MAX_BUFFER_SIZE)
                                       );
                  if (n < 0)
                  {
                    error = ERROR_(IO_ERROR,errno);
                    break;
                  }
                  storageHandle->sftp.readAheadBuffer.offset = storageHandle->sftp.index;
                  storageHandle->sftp.readAheadBuffer.length = (ulong)n;

                  // copy data from read-ahead buffer
                  bytesAvail = MIN(length,storageHandle->sftp.readAheadBuffer.length);
                  memcpy(buffer,storageHandle->sftp.readAheadBuffer.data,bytesAvail);

                  // adjust buffer, size, bytes read, index
                  buffer = (byte*)buffer+bytesAvail;
                  size -= bytesAvail;
                  if (bytesRead != NULL) (*bytesRead) += bytesAvail;
                  storageHandle->sftp.index += (uint64)bytesAvail;
                }
                else
                {
                  // read direct
                  n = libssh2_sftp_read(storageHandle->sftp.sftpHandle,
                                        buffer,
                                        length
                                       );
                  if (n < 0)
                  {
                    error = ERROR_(IO_ERROR,errno);
                    break;
                  }
                  bytesAvail = (ulong)n;

                  // adjust buffer, size, bytes read, index
                  buffer = (byte*)buffer+(ulong)bytesAvail;
                  size -= (ulong)bytesAvail;
                  if (bytesRead != NULL) (*bytesRead) += (ulong)bytesAvail;
                  storageHandle->sftp.index += (uint64)bytesAvail;
                }

                // get end time, end received bytes
                endTimestamp          = Misc_getTimestamp();
                endTotalReceivedBytes = storageHandle->sftp.totalReceivedBytes;
                assert(endTotalReceivedBytes >= startTotalReceivedBytes);

                /* limit used band width if requested (note: when the system time is
                   changing endTimestamp may become smaller than startTimestamp;
                   thus do not check this with an assert())
                */
                if (endTimestamp >= startTimestamp)
                {
                  limitBandWidth(&storageHandle->sftp.bandWidthLimiter,
                                 endTotalReceivedBytes-startTotalReceivedBytes,
                                 endTimestamp-startTimestamp
                                );
                }
              }
            }
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_read(storageHandle,buffer,size,bytesRead);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_read(storageHandle,buffer,size,bytesRead);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_read(storageHandle,buffer,size,bytesRead);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

Errors Storage_write(StorageHandle *storageHandle,
                     const void    *buffer,
                     ulong         size
                    )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(buffer != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  error = ERROR_NONE;
  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
      {
        error = File_write(&storageHandle->fileSystem.fileHandle,buffer,size);
      }
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_write(storageHandle,buffer,size);
      break;
    case STORAGE_TYPE_SSH:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_write(storageHandle,buffer,size);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          ulong   writtenBytes;
          ulong   length;
          uint64  startTimestamp,endTimestamp;
          uint64  startTotalSentBytes,endTotalSentBytes;
          ssize_t n;

          if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
          {
            assert(storageHandle->sftp.sftpHandle != NULL);

            writtenBytes = 0L;
            while (writtenBytes < size)
            {
              // get max. number of bytes to send in one step
              if (storageHandle->sftp.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageHandle->sftp.bandWidthLimiter.blockSize,size-writtenBytes);
              }
              else
              {
                length = size-writtenBytes;
              }
              assert(length > 0L);

              // get start time, start received bytes
              startTimestamp      = Misc_getTimestamp();
              startTotalSentBytes = storageHandle->sftp.totalSentBytes;

              // send data
              do
              {
                n = libssh2_sftp_write(storageHandle->sftp.sftpHandle,
                                       buffer,
                                       length
                                      );
                if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*MISC_US_PER_MS);
              }
              while (n == LIBSSH2_ERROR_EAGAIN);

              // get end time, end received bytes
              endTimestamp      = Misc_getTimestamp();
              endTotalSentBytes = storageHandle->sftp.totalSentBytes;
              assert(endTotalSentBytes >= startTotalSentBytes);

// ??? is it possible in blocking-mode that write() return 0 and this is not an error?
#if 1
              if      (n == 0)
              {
                // should not happen in blocking-mode: bug? libssh2 API changed somewhere between 0.18 and 1.2.4? => wait for data
                if (!waitSSHSessionSocket(&storageHandle->sftp.socketHandle))
                {
                  error = ERROR_NETWORK_SEND;
                  break;
                }
              }
              else if (n < 0)
              {
                error = ERROR_NETWORK_SEND;
                break;
              }
#else /* 0 */
              if (n <= 0)
              {
                error = ERROR_NETWORK_SEND;
                break;
              }
#endif /* 0 */
              buffer = (byte*)buffer+n;
              writtenBytes += (ulong)n;

              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageHandle->sftp.bandWidthLimiter,
                               endTotalSentBytes-startTotalSentBytes,
                               endTimestamp-startTimestamp
                              );
              }
            }
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_write(storageHandle,buffer,size);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_write(storageHandle,buffer,size);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_write(storageHandle,buffer,size);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

uint64 Storage_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  size = 0LL;
  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
      {
        size = File_getSize(&storageHandle->fileSystem.fileHandle);
      }
      break;
    case STORAGE_TYPE_FTP:
      size = StorageFTP_getSize(storageHandle);
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      size = StorageSCP_getSize(storageHandle);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
        {
          size = storageHandle->sftp.size;
        }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      size = StorageWebDAV_getSize(storageHandle);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      size = StorageOptical_getSize(storageHandle);
      break;
    case STORAGE_TYPE_DEVICE:
      size = StorageDevice_getSize(storageHandle);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return size;
}

Errors Storage_tell(StorageHandle *storageHandle,
                    uint64        *offset
                   )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(offset != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  (*offset) = 0LL;

  error = ERROR_NONE;
  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
      {
        error = File_tell(&storageHandle->fileSystem.fileHandle,offset);
      }
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_tell(storageHandle,offset);
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_tell(storageHandle,offset);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
        {
          (*offset) = storageHandle->sftp.index;
          error     = ERROR_NONE;
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_tell(storageHandle,offset);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_tell(storageHandle,offset);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_tell(storageHandle,offset);

      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

Errors Storage_seek(StorageHandle *storageHandle,
                    uint64        offset
                   )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  error = ERROR_NONE;
  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
      {
        error = File_seek(&storageHandle->fileSystem.fileHandle,offset);
      }
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_seek(storageHandle,offset);
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_seek(storageHandle,offset);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
        {
          assert(storageHandle->sftp.sftpHandle != NULL);
          assert(storageHandle->sftp.readAheadBuffer.data != NULL);

          if      (offset > storageHandle->sftp.index)
          {
            uint64 skip;
            uint64 i;
            uint64 n;

            skip = offset-storageHandle->sftp.index;
            if (skip > 0LL)
            {
              // skip data in read-ahead buffer
              if (   (storageHandle->sftp.index >= storageHandle->sftp.readAheadBuffer.offset)
                  && (storageHandle->sftp.index < (storageHandle->sftp.readAheadBuffer.offset+storageHandle->sftp.readAheadBuffer.length))
                 )
              {
                i = storageHandle->sftp.index-storageHandle->sftp.readAheadBuffer.offset;
                n = MIN(skip,storageHandle->sftp.readAheadBuffer.length-i);
                skip -= n;
                storageHandle->sftp.index += (uint64)n;
              }

              if (skip > 0LL)
              {
                #if   defined(HAVE_SSH2_SFTP_SEEK64)
                  libssh2_sftp_seek64(storageHandle->sftp.sftpHandle,offset);
                #elif defined(HAVE_SSH2_SFTP_SEEK2)
                  libssh2_sftp_seek2(storageHandle->sftp.sftpHandle,offset);
                #else /* not HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
                  libssh2_sftp_seek(storageHandle->sftp.sftpHandle,(size_t)offset);
                #endif /* HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
                storageHandle->sftp.index = offset;
              }
            }
          }
          else if (offset < storageHandle->sftp.index)
          {
// NYI: ??? support seek backward
            error = ERROR_FUNCTION_NOT_SUPPORTED;
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_seek(storageHandle,offset);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_seek(storageHandle,offset);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_seek(storageHandle,offset);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

Errors Storage_delete(StorageHandle *storageHandle,
                      ConstString   storageFileName
                     )
{
  ConstString deleteFileName;
  Errors      error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  deleteFileName = (storageFileName != NULL) ? storageFileName : storageHandle->storageSpecifier.archiveName;

  error = ERROR_UNKNOWN;
  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
      {
        error = File_delete(deleteFileName,FALSE);
      }
      else
      {
        error = ERROR_NONE;
      }
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_delete(storageHandle,storageFileName);
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_delete(storageHandle,storageFileName);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_WEBDAVSSH2
        error = Network_connect(&storageHandle->sftp.socketHandle,
                                SOCKET_TYPE_SSH,
                                storageHandle->storageSpecifier.hostName,
                                storageHandle->storageSpecifier.hostPort,
                                storageHandle->storageSpecifier.loginName,
                                storageHandle->storageSpecifier.loginPassword,
                                storageHandle->sftp.sshPublicKeyFileName,
                                storageHandle->sftp.sshPrivateKeyFileName,
                                0
                               );
        if (error == ERROR_NONE)
        {
          libssh2_session_set_timeout(Network_getSSHSession(&storageHandle->sftp.socketHandle),READ_TIMEOUT);

          // init session
          storageHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageHandle->sftp.socketHandle));
          if (storageHandle->sftp.sftp != NULL)
          {
            if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
            {
              // delete file
              if (libssh2_sftp_unlink(storageHandle->sftp.sftp,
                                      String_cString(deleteFileName)
                                     ) == 0
                 )
              {
                error = ERROR_NONE;
              }
              else
              {
                 char *sshErrorText;

                 libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&sshErrorText,NULL,0);
                 error = ERRORX_(SSH,
                                 libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle)),
                                 sshErrorText
                                );
              }
            }
            else
            {
              error = ERROR_NONE;
            }

            libssh2_sftp_shutdown(storageHandle->sftp.sftp);
          }
          else
          {
            char *sshErrorText;

            libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&sshErrorText,NULL,0);
            error = ERRORX_(SSH,
                            libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle)),
                            sshErrorText
                           );
            Network_disconnect(&storageHandle->sftp.socketHandle);
          }
          Network_disconnect(&storageHandle->sftp.socketHandle);
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_delete(storageHandle,storageFileName);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_delete(storageHandle,storageFileName);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_delete(storageHandle,storageFileName);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

#if 0
still not complete
Errors Storage_getFileInfo(StorageHandle *storageHandle,
                           ConstString   fileName,
                           FileInfo      *fileInfo
                          )
{
  String infoFileName;
  Errors error;

  assert(storageHandle != NULL);
  assert(fileInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  infoFileName = (fileName != NULL) ? fileName : storageHandle->storageSpecifier.archiveName;
  memset(fileInfo,0,sizeof(fileInfo));

  error = ERROR_UNKNOWN;
  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      error = File_getFileInfo(infoFileName,fileInfo);
      break;
    case STORAGE_TYPE_FTP:
      errors = StorageFTP_getFileInfo(storageHandle,fileName,fileInfo);
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      errors = StorageSCP_getFileInfo(storageHandle,fileName,fileInfo);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;

          error = Network_connect(&storageHandle->sftp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageHandle->storageSpecifier.hostName,
                                  storageHandle->storageSpecifier.hostPort,
                                  storageHandle->storageSpecifier.loginName,
                                  storageHandle->storageSpecifier.loginPassword,
                                  storageHandle->sftp.sshPublicKeyFileName,
                                  storageHandle->sftp.sshPrivateKeyFileName,
                                  0
                                 );
          if (error == ERROR_NONE)
          {
            libssh2_session_set_timeout(Network_getSSHSession(&storageHandle->sftp.socketHandle),READ_TIMEOUT);

            // init session
            storageHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageHandle->sftp.socketHandle));
            if (storageHandle->sftp.sftp != NULL)
            {
              // get file fino
              if (libssh2_sftp_lstat(storageHandle->sftp.sftp,
                                     String_cString(infoFileName),
                                     &sftpAttributes
                                    ) == 0
                 )
              {
                if      (LIBSSH2_SFTP_S_ISREG (sftpAttributes.flags)) fileInfo->type = FILE_TYPE_FILE;
                else if (LIBSSH2_SFTP_S_ISDIR (sftpAttributes.flags)) fileInfo->type = FILE_TYPE_DIRECTORY;
                else if (LIBSSH2_SFTP_S_ISLNK (sftpAttributes.flags)) fileInfo->type = FILE_TYPE_LINK;
                else if (LIBSSH2_SFTP_S_ISCHR (sftpAttributes.flags)) fileInfo->type = FILE_TYPE_SPECIAL;
                else if (LIBSSH2_SFTP_S_ISBLK (sftpAttributes.flags)) fileInfo->type = FILE_TYPE_SPECIAL;
                else if (LIBSSH2_SFTP_S_ISFIFO(sftpAttributes.flags)) fileInfo->type = FILE_TYPE_SPECIAL;
                else if (LIBSSH2_SFTP_S_ISSOCK(sftpAttributes.flags)) fileInfo->type = FILE_TYPE_SPECIAL;
                else                                                  fileInfo->type = FILE_TYPE_UNKNOWN;
                fileInfo->size            = (uint64)sftpAttributes.filesize;
                fileInfo->timeLastAccess  = (uint64)sftpAttributes.atime;
                fileInfo->timeModified    = (uint64)sftpAttributes.mtime;
                fileInfo->userId          = sftpAttributes.uid;
                fileInfo->groupId         = sftpAttributes.gid;
                fileInfo->permission      = (FilePermission)sftpAttributes.permissions;

                error = ERROR_NONE;
              }
              else
              {
                 char *sshErrorText;

                 libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&sshErrorText,NULL,0);
                 error = ERRORX_(SSH,
                                 libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle)),
                                 sshErrorText
                                );
              }

              libssh2_sftp_shutdown(storageHandle->sftp.sftp);
            }
            else
            {
              char *sshErrorText;

              libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&sshErrorText,NULL,0);
              error = ERRORX_(SSH,
                              libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle)),
                              sshErrorText
                             );
              Network_disconnect(&storageHandle->sftp.socketHandle);
            }
            Network_disconnect(&storageHandle->sftp.socketHandle);
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      errors = StorageWebDAV_getFileInfo(storageHandle,fileName,fileInfo);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      errors = StorageOptical_getFileInfo(storageHandle,fileName,fileInfo);
      break;
    case STORAGE_TYPE_DEVICE:
      errors = StorageDevice_getFileInfo(storageHandle,fileName,fileInfo);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

Errors Storage_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                 const StorageSpecifier     *storageSpecifier,
                                 const JobOptions           *jobOptions,
                                 ServerConnectionPriorities serverConnectionPriority
                                )
{
  AutoFreeList autoFreeList;
  Errors       error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(jobOptions != NULL);

  #if !defined(HAVE_CURL) && !defined(HAVE_FTP) && (!defined(HAVE_CURL) || !defined(HAVE_MXML))
    UNUSED_VARIABLE(serverConnectionPriority);
  #endif

  // initialize variables
  AutoFree_init(&autoFreeList);
  Storage_duplicateSpecifier(&storageDirectoryListHandle->storageSpecifier,storageSpecifier);
  AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->storageSpecifier,{ Storage_doneSpecifier(&storageDirectoryListHandle->storageSpecifier); });

  // open directory listing
  error = ERROR_UNKNOWN;
  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      UNUSED_VARIABLE(jobOptions);

      // init variables
      storageDirectoryListHandle->type = STORAGE_TYPE_FILESYSTEM;

      // open directory
      error = File_openDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle,
                                     storageSpecifier->archiveName
                                    );
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_openDirectoryList(storageDirectoryListHandle,storageSpecifier,jobOptions,serverConnectionPriority);
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
error = ERROR_FUNCTION_NOT_SUPPORTED;
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(jobOptions);

        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_openDirectoryList(storageDirectoryListHandle,storageSpecifier,jobOptions,serverConnectionPriority);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          SSHServer sshServer;

          // init variables
          storageDirectoryListHandle->type               = STORAGE_TYPE_SFTP;
          storageDirectoryListHandle->sftp.pathName      = String_new();
          storageDirectoryListHandle->sftp.buffer        = (char*)malloc(MAX_FILENAME_LENGTH);
          if (storageDirectoryListHandle->sftp.buffer == NULL)
          {
            String_delete(storageDirectoryListHandle->sftp.pathName);
            AutoFree_cleanup(&autoFreeList);
            return ERROR_INSUFFICIENT_MEMORY;
          }
          storageDirectoryListHandle->sftp.bufferLength  = 0;
          storageDirectoryListHandle->sftp.entryReadFlag = FALSE;
          AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->sftp.buffer,{ free(storageDirectoryListHandle->sftp.buffer); });
          AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->sftp.pathName,{ String_delete(storageDirectoryListHandle->sftp.pathName); });

          // set pathname
          String_set(storageDirectoryListHandle->sftp.pathName,storageDirectoryListHandle->storageSpecifier.archiveName);

          // get SSH server settings
          getSSHServerSettings(storageDirectoryListHandle->storageSpecifier.hostName,jobOptions,&sshServer);
          if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_set(storageDirectoryListHandle->storageSpecifier.loginName,sshServer.loginName);
          if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("LOGNAME"));
          if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("USER"));
          if (storageDirectoryListHandle->storageSpecifier.hostPort == 0) storageDirectoryListHandle->storageSpecifier.hostPort = sshServer.port;
          if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.hostName))
          {
            AutoFree_cleanup(&autoFreeList);
            return ERROR_NO_HOST_NAME;
          }

          // open network connection
          error = ERROR_UNKNOWN;
          if ((error == ERROR_UNKNOWN) && !Password_isEmpty(sshServer.password))
          {
            error = Network_connect(&storageDirectoryListHandle->sftp.socketHandle,
                                    SOCKET_TYPE_SSH,
                                    storageDirectoryListHandle->storageSpecifier.hostName,
                                    storageDirectoryListHandle->storageSpecifier.hostPort,
                                    storageDirectoryListHandle->storageSpecifier.loginName,
                                    sshServer.password,
                                    sshServer.publicKeyFileName,
                                    sshServer.privateKeyFileName,
                                    0
                                   );
          }
          if (error == ERROR_UNKNOWN)
          {
            // initialize default password
            if (   initDefaultSSHPassword(storageDirectoryListHandle->storageSpecifier.hostName,storageDirectoryListHandle->storageSpecifier.loginName,jobOptions)
                && !Password_isEmpty(defaultSSHPassword)
               )
            {
              error = Network_connect(&storageDirectoryListHandle->sftp.socketHandle,
                                      SOCKET_TYPE_SSH,
                                      storageDirectoryListHandle->storageSpecifier.hostName,
                                      storageDirectoryListHandle->storageSpecifier.hostPort,
                                      storageDirectoryListHandle->storageSpecifier.loginName,
                                      defaultSSHPassword,
                                      sshServer.publicKeyFileName,
                                      sshServer.privateKeyFileName,
                                      0
                                     );
            }
            else
            {
              error = !Password_isEmpty(defaultSSHPassword)
                        ? ERRORX_(INVALID_SSH_PASSWORD,0,String_cString(storageDirectoryListHandle->storageSpecifier.hostName))
                        : ERRORX_(NO_SSH_PASSWORD,0,String_cString(storageDirectoryListHandle->storageSpecifier.hostName));
            }
          }
          assert(error != ERROR_UNKNOWN);
          if (error != ERROR_NONE)
          {
            AutoFree_cleanup(&autoFreeList);
            return error;
          }
          libssh2_session_set_timeout(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle),READ_TIMEOUT);
          AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->sftp.socketHandle,{ Network_disconnect(&storageDirectoryListHandle->sftp.socketHandle); });

          // init SFTP session
          storageDirectoryListHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle));
          if (storageDirectoryListHandle->sftp.sftp == NULL)
          {
            error = ERROR_(SSH,libssh2_session_last_errno(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle)));
            AutoFree_cleanup(&autoFreeList);
            return error;
          }
          AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->sftp.sftp,{ libssh2_sftp_shutdown(storageDirectoryListHandle->sftp.sftp); });

          // open directory for reading
          storageDirectoryListHandle->sftp.sftpHandle = libssh2_sftp_opendir(storageDirectoryListHandle->sftp.sftp,
                                                                             String_cString(storageDirectoryListHandle->sftp.pathName)
                                                                            );
          if (storageDirectoryListHandle->sftp.sftpHandle == NULL)
          {
            error = ERROR_(SSH,libssh2_session_last_errno(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle)));
            AutoFree_cleanup(&autoFreeList);
            return error;
          }

          // free resources
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(jobOptions);

        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_openDirectoryList(storageDirectoryListHandle,storageSpecifier,jobOptions,serverConnectionPriority);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_openDirectoryList(storageDirectoryListHandle,storageSpecifier,jobOptions);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_openDirectoryList(storageDirectoryListHandle,storageSpecifier,jobOptions);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

void Storage_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);

  switch (storageDirectoryListHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      File_closeDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle);
      break;
    case STORAGE_TYPE_FTP:
      StorageFTP_closeDirectoryList(storageDirectoryListHandle);
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      StorageSCP_closeDirectoryList(storageDirectoryListHandle);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        libssh2_sftp_closedir(storageDirectoryListHandle->sftp.sftpHandle);
        libssh2_sftp_shutdown(storageDirectoryListHandle->sftp.sftp);
        Network_disconnect(&storageDirectoryListHandle->sftp.socketHandle);
        free(storageDirectoryListHandle->sftp.buffer);
        String_delete(storageDirectoryListHandle->sftp.pathName);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      StorageWebDAV_closeDirectoryList(storageDirectoryListHandle);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      StorageOptical_closeDirectoryList(storageDirectoryListHandle);
      break;
    case STORAGE_TYPE_DEVICE:
      StorageDevice_closeDirectoryList(storageDirectoryListHandle);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  Storage_doneSpecifier(&storageDirectoryListHandle->storageSpecifier);
}

bool Storage_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  bool endOfDirectoryFlag;

  assert(storageDirectoryListHandle != NULL);

  endOfDirectoryFlag = TRUE;
  switch (storageDirectoryListHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      endOfDirectoryFlag = File_endOfDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle);
      break;
    case STORAGE_TYPE_FTP:
      endOfDirectoryFlag = StorageFTP_endOfDirectoryList(storageDirectoryListHandle);
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      endOfDirectoryFlag = StorageSCP_endOfDirectoryList(storageDirectoryListHandle);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          int n;

          // read entry iff not already read
          while (!storageDirectoryListHandle->sftp.entryReadFlag)
          {
            n = libssh2_sftp_readdir(storageDirectoryListHandle->sftp.sftpHandle,
                                     storageDirectoryListHandle->sftp.buffer,
                                     MAX_FILENAME_LENGTH,
                                     &storageDirectoryListHandle->sftp.attributes
                                    );
            if (n > 0)
            {
              if (   !S_ISDIR(storageDirectoryListHandle->sftp.attributes.flags)
                  && ((n != 1) || (strncmp(storageDirectoryListHandle->sftp.buffer,".", 1) != 0))
                  && ((n != 2) || (strncmp(storageDirectoryListHandle->sftp.buffer,"..",2) != 0))
                 )
              {
                storageDirectoryListHandle->sftp.bufferLength = n;
                storageDirectoryListHandle->sftp.entryReadFlag = TRUE;
              }
            }
            else
            {
              break;
            }
          }

          endOfDirectoryFlag = !storageDirectoryListHandle->sftp.entryReadFlag;
        }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      endOfDirectoryFlag = StorageWebDAV_endOfDirectoryList(storageDirectoryListHandle);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      endOfDirectoryFlag = StorageOptical_endOfDirectoryList(storageDirectoryListHandle);
      break;
    case STORAGE_TYPE_DEVICE:
      endOfDirectoryFlag = StorageDevice_endOfDirectoryList(storageDirectoryListHandle);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return endOfDirectoryFlag;
}

Errors Storage_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                 String                     fileName,
                                 FileInfo                   *fileInfo
                                )
{
  Errors error;

  assert(storageDirectoryListHandle != NULL);

  error = ERROR_NONE;
  switch (storageDirectoryListHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      error = File_readDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle,fileName);
      if (error == ERROR_NONE)
      {
        if (fileInfo != NULL)
        {
          (void)File_getFileInfo(fileName,fileInfo);
        }
      }
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_readDirectoryList(storageDirectoryListHandle,fileName,fileInfo);
      break;
    case STORAGE_TYPE_SSH:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_readDirectoryList(storageDirectoryListHandle,fileName,fileInfo);
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          int n;

          if (!storageDirectoryListHandle->sftp.entryReadFlag)
          {
            do
            {
              n = libssh2_sftp_readdir(storageDirectoryListHandle->sftp.sftpHandle,
                                       storageDirectoryListHandle->sftp.buffer,
                                       MAX_FILENAME_LENGTH,
                                       &storageDirectoryListHandle->sftp.attributes
                                      );
              if      (n > 0)
              {
                if (   !S_ISDIR(storageDirectoryListHandle->sftp.attributes.flags)
                    && ((n != 1) || (strncmp(storageDirectoryListHandle->sftp.buffer,".", 1) != 0))
                    && ((n != 2) || (strncmp(storageDirectoryListHandle->sftp.buffer,"..",2) != 0))
                   )
                {
                  storageDirectoryListHandle->sftp.entryReadFlag = TRUE;
                  error = ERROR_NONE;
                }
              }
              else
              {
                error = ERROR_(IO_ERROR,errno);
              }
            }
            while (!storageDirectoryListHandle->sftp.entryReadFlag && (error == ERROR_NONE));
          }
          else
          {
            error = ERROR_NONE;
          }

          if (storageDirectoryListHandle->sftp.entryReadFlag)
          {
            String_set(fileName,storageDirectoryListHandle->sftp.pathName);
            File_appendFileNameBuffer(fileName,storageDirectoryListHandle->sftp.buffer,storageDirectoryListHandle->sftp.bufferLength);

            if (fileInfo != NULL)
            {
              if      (S_ISREG(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_FILE;
              }
              else if (S_ISDIR(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_DIRECTORY;
              }
              #ifdef HAVE_S_ISLNK
              else if (S_ISLNK(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_LINK;
              }
              #endif /* HAVE_S_ISLNK */
              else if (S_ISCHR(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_SPECIAL;
                fileInfo->specialType = FILE_SPECIAL_TYPE_CHARACTER_DEVICE;
              }
              else if (S_ISBLK(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_SPECIAL;
                fileInfo->specialType = FILE_SPECIAL_TYPE_BLOCK_DEVICE;
              }
              else if (S_ISFIFO(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_SPECIAL;
                fileInfo->specialType = FILE_SPECIAL_TYPE_FIFO;
              }
              #ifdef HAVE_S_ISSOCK
              else if (S_ISSOCK(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_SPECIAL;
                fileInfo->specialType = FILE_SPECIAL_TYPE_SOCKET;
              }
              #endif /* HAVE_S_ISSOCK */
              else
              {
                fileInfo->type        = FILE_TYPE_UNKNOWN;
              }
              fileInfo->size            = storageDirectoryListHandle->sftp.attributes.filesize;
              fileInfo->timeLastAccess  = storageDirectoryListHandle->sftp.attributes.atime;
              fileInfo->timeModified    = storageDirectoryListHandle->sftp.attributes.mtime;
              fileInfo->timeLastChanged = 0LL;
              fileInfo->userId          = storageDirectoryListHandle->sftp.attributes.uid;
              fileInfo->groupId         = storageDirectoryListHandle->sftp.attributes.gid;
              fileInfo->permission      = storageDirectoryListHandle->sftp.attributes.permissions;
              fileInfo->major           = 0;
              fileInfo->minor           = 0;
              memset(&fileInfo->cast,0,sizeof(FileCast));
            }

            storageDirectoryListHandle->sftp.entryReadFlag = FALSE;
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_readDirectoryList(storageDirectoryListHandle,fileName,fileInfo);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_readDirectoryList(storageDirectoryListHandle,fileName,fileInfo);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_readDirectoryList(storageDirectoryListHandle,fileName,fileInfo);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

Errors Storage_copy(const StorageSpecifier       *storageSpecifier,
                    const JobOptions             *jobOptions,
                    BandWidthList                *maxBandWidthList,
                    StorageRequestVolumeFunction storageRequestVolumeFunction,
                    void                         *storageRequestVolumeUserData,
                    StorageStatusInfoFunction    storageStatusInfoFunction,
                    void                         *storageStatusInfoUserData,
                    ConstString                  localFileName
                   )
{
  AutoFreeList      autoFreeList;
  void              *buffer;
  Errors            error;
  StorageHandle     storageHandle;
  FileHandle        fileHandle;
  ulong             bytesRead;

  assert(storageSpecifier != NULL);
  assert(jobOptions != NULL);
  assert(localFileName != NULL);

  // init variables
  AutoFree_init(&autoFreeList);
  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,buffer,{ free(buffer); });

  // open storage
  error = Storage_init(&storageHandle,
                       storageSpecifier,
                       jobOptions,
                       maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK(storageRequestVolumeFunction,storageRequestVolumeUserData),
                       CALLBACK(storageStatusInfoFunction,storageStatusInfoUserData)
                      );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  error = Storage_open(&storageHandle,NULL);
  if (error != ERROR_NONE)
  {
    (void)Storage_done(&storageHandle);
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&storageHandle,{ Storage_close(&storageHandle); (void)Storage_done(&storageHandle); });

  // create local file
  error = File_open(&fileHandle,
                    localFileName,
                    FILE_OPEN_CREATE
                   );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&fileHandle,{ File_close(&fileHandle); });
  AUTOFREE_ADD(&autoFreeList,localFileName,{ (void)File_delete(localFileName,FALSE); });

  // copy data from archive to local file
  while (   (error == ERROR_NONE)
         && !Storage_eof(&storageHandle)
        )
  {
    error = Storage_read(&storageHandle,
                         buffer,
                         BUFFER_SIZE,
                         &bytesRead
                        );
    if (error == ERROR_NONE)
    {
      error = File_write(&fileHandle,
                         buffer,
                         bytesRead
                        );
    }
  }
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // close local file
  File_close(&fileHandle);

  // close archive
  Storage_close(&storageHandle);
  (void)Storage_done(&storageHandle);

  // free resources
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
