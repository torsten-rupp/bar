/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: storage functions
* Systems: all
*
\***********************************************************************/

#define __STORAGE_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <poll.h>
#include <time.h>
#include <sys/select.h>
#ifdef HAVE_CURL
  #include <curl/curl.h>
#endif /* HAVE_CURL */
#ifdef HAVE_MXML
  #include <mxml.h>
#endif /* HAVE_MXML */
#ifdef HAVE_SSH2
  #include <libssh2.h>
#endif /* HAVE_SSH2 */
#include <signal.h>
#include <errno.h>
#include <assert.h>

#include "common/global.h"
#include "common/autofree.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/files.h"
#include "common/network.h"
#include "common/semaphores.h"
#include "common/passwords.h"
#include "common/misc.h"

#include "errors.h"
#include "crypt.h"
#include "archive.h"
#include "jobs.h"
#include "bar_global.h"
#include "bar.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
/* file data buffer size */
#define BUFFER_SIZE (64*1024)

// max. number of password input requests
#define MAX_PASSWORD_REQUESTS 3

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
* Name   : updateStorageStatusInfo
* Purpose: update storage status info
* Input  : storageInfo - storage info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateStorageStatusInfo(const StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  if (storageInfo->updateStatusInfoFunction != NULL)
  {
    storageInfo->updateStatusInfoFunction(&storageInfo->runningInfo,
                                          storageInfo->updateStatusInfoUserData
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
  CURLMcode       curlmCode;
  int             maxFD;
  long            curlTimeout;
  struct timespec ts;
  Errors          error;

  assert(curlMultiHandle != NULL);

  // get file descriptors from the transfers
  FD_ZERO(&fdSetRead);
  FD_ZERO(&fdSetWrite);
  FD_ZERO(&fdSetException);
  curlmCode = curl_multi_fdset(curlMultiHandle,&fdSetRead,&fdSetWrite,&fdSetException,&maxFD);
  if (curlmCode != CURLM_OK)
  {
    return ERRORX_(IO,0,"%s",curl_multi_strerror(curlmCode));
  }

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

  // Note: ignore SIGALRM in pselect()
  sigemptyset(&signalMask);
  sigaddset(&signalMask,SIGALRM);

//TODO: replace by ppoll()
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
          // recalculate max. block size to send a little less in a single step (if possible)
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
* Name   : initSSHLogin
* Purpose: init SSH login
* Input  : hostName                - host name
*          loginName               - login name
*          loginPassword           - login password
*          jobOptions              - job options
*          getNamePasswordFunction - get password call-back (can be
*                                    NULL)
*          getNamePasswordUserData - user data for get password call-back
* Output : -
* Return : TRUE if SSH password intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool initSSHLogin(ConstString             hostName,
                        String                  loginName,
                        Password                *loginPassword,
                        const JobOptions        *jobOptions,
                        GetNamePasswordFunction getNamePasswordFunction,
                        void                    *getNamePasswordUserData
                       )
{
  SemaphoreLock semaphoreLock;
  String        s;
  bool          initFlag;

  assert(!String_isEmpty(hostName));
  assert(loginName != NULL);
  assert(loginPassword != NULL);

  initFlag = FALSE;

  if (jobOptions != NULL)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      if (Password_isEmpty(&jobOptions->sshServer.password))
      {
        switch (globalOptions.runMode)
        {
          case RUN_MODE_INTERACTIVE:
            if (defaultSSHPassword == NULL)
            {
              s = !String_isEmpty(loginName)
                    ? String_format(String_new(),"SSH login password for %S@%S",loginName,hostName)
                    : String_format(String_new(),"SSH login password for %S",hostName);
              if (Password_input(loginPassword,String_cString(s),PASSWORD_INPUT_MODE_ANY))
              {
                initFlag = TRUE;
              }
              String_delete(s);
            }
            else
            {
              Password_set(loginPassword,defaultSSHPassword);
              initFlag = TRUE;
            }
            break;
          case RUN_MODE_BATCH:
          case RUN_MODE_SERVER:
            if (getNamePasswordFunction != NULL)
            {
              s = !String_isEmpty(loginName)
                    ? String_format(String_new(),"%S@%S",loginName,hostName)
                    : String_format(String_new(),"%S",hostName);
              if (getNamePasswordFunction(loginName,
                                          loginPassword,
                                          PASSWORD_TYPE_SSH,
                                          String_cString(s),
                                          TRUE,
                                          TRUE,
                                          getNamePasswordUserData
                                         ) == ERROR_NONE
                 )
              {
                initFlag = TRUE;
              }
              String_delete(s);
            }
            break;
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
* Input  : hostName         - host name
*          hostPort         - host SSH port
*          loginName        - login name
*          loginPassword    - login password
*          publicKey        - SSH public key
*          publicKeyLength  - SSH public key length
*          privateKey       - SSH private key
*          privateKeyLength - SSH private key length
* Output : -
* Return : ERROR_NONE if login is possible, error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors checkSSHLogin(ConstString hostName,
                           uint        hostPort,
                           ConstString loginName,
                           Password    *loginPassword,
                           void        *publicKey,
                           uint        publicKeyLength,
                           void        *privateKey,
                           uint        privateKeyLength
                          )
{
  SocketHandle socketHandle;
  Errors       error;

  printInfo(5,"SSH: host %s:%d\n",String_cString(hostName),hostPort);
  error = Network_connect(&socketHandle,
                          SOCKET_TYPE_SSH,
                          hostName,
                          hostPort,
                          loginName,
                          loginPassword,
                          publicKey,
                          publicKeyLength,
                          privateKey,
                          privateKeyLength,
                            SOCKET_FLAG_NONE
                          | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                          | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0)
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
  struct timespec pollTimeout;
  struct pollfd   pollfds[1];

  assert(socketHandle != NULL);

  // get session
  session = Network_getSSHSession(socketHandle);
  assert(session != NULL);

  // Note: ignore SIGALRM in pselect()
  sigemptyset(&signalMask);
  sigaddset(&signalMask,SIGALRM);

  // wait for max. 60s
#if 1
  pollTimeout.tv_sec  = 60L;
  pollTimeout.tv_nsec = 0L;
  pollfds[0].fd     = socketHandle->handle;
  pollfds[0].events = POLLERR|POLLNVAL;
  if ((libssh2_session_block_directions(session) & LIBSSH2_SESSION_BLOCK_INBOUND ) != 0) pollfds[0].events |= POLLIN;
  if ((libssh2_session_block_directions(session) & LIBSSH2_SESSION_BLOCK_OUTBOUND) != 0) pollfds[0].events |= POLLOUT;
  return (   (ppoll(pollfds,1,&pollTimeout,&signalMask) > 0)
          && ((pollfds[0].revents & (POLLERR|POLLNVAL)) == 0)
         );
#else
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
#endif
}
#endif /* HAVE_SSH2 */

// ----------------------------------------------------------------------

#include "storage_file.c"
#include "storage_ftp.c"
#include "storage_scp.c"
#include "storage_sftp.c"
#include "storage_webdav.c"
#include "storage_optical.c"
#include "storage_device.c"
#include "storage_master.c"

/*---------------------------------------------------------------------*/

Errors Storage_initAll(void)
{
  Errors error;

  error = ERROR_NONE;

  oldSignalAlarmHandler = signal(SIGALRM,signalHandler);
  #if defined(HAVE_SSH2)
    defaultSSHPassword = NULL;
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
  if (error == ERROR_NONE)
  {
    error = StorageFile_initAll();
  }
  if (error == ERROR_NONE)
  {
    error = StorageFTP_initAll();
  }
  if (error == ERROR_NONE)
  {
    error = StorageSCP_initAll();
  }
  if (error == ERROR_NONE)
  {
    error = StorageSFTP_initAll();
  }
  if (error == ERROR_NONE)
  {
    error = StorageWebDAV_initAll();
  }
  if (error == ERROR_NONE)
  {
    error = StorageOptical_initAll();
  }
  if (error == ERROR_NONE)
  {
    error = StorageDevice_initAll();
  }
  if (error == ERROR_NONE)
  {
    error = StorageMaster_initAll();
  }

  return error;
}

void Storage_doneAll(void)
{
  StorageMaster_doneAll();
  StorageDevice_doneAll();
  StorageOptical_doneAll();
  StorageWebDAV_doneAll();
  StorageSFTP_doneAll();
  StorageSCP_doneAll();
  StorageFTP_doneAll();
  #if   defined(HAVE_CURL)
    curl_global_cleanup();
  #endif /* HAVE_CURL */
  StorageFile_doneAll();

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
  storageSpecifier->archivePatternString = NULL;
  storageSpecifier->storageName          = String_new();
  storageSpecifier->printableStorageName = String_new();

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(storageSpecifier,sizeof(StorageSpecifier));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,storageSpecifier,sizeof(StorageSpecifier));
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
  if (sourceStorageSpecifier->archivePatternString != NULL)
  {
    destinationStorageSpecifier->archivePatternString = String_duplicate(sourceStorageSpecifier->archivePatternString);
    Pattern_copy(&destinationStorageSpecifier->archivePattern,&sourceStorageSpecifier->archivePattern);
  }
  else
  {
    destinationStorageSpecifier->archivePatternString = NULL;
  }
  destinationStorageSpecifier->storageName          = String_new();
  destinationStorageSpecifier->printableStorageName = String_new();

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(destinationStorageSpecifier,sizeof(StorageSpecifier));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,destinationStorageSpecifier,sizeof(StorageSpecifier));
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
    DEBUG_REMOVE_RESOURCE_TRACE(storageSpecifier,sizeof(StorageSpecifier));
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,storageSpecifier,sizeof(StorageSpecifier));
  #endif /* NDEBUG */

  String_delete(storageSpecifier->printableStorageName);
  String_delete(storageSpecifier->storageName);
  if (storageSpecifier->archivePatternString != NULL)
  {
    Pattern_done(&storageSpecifier->archivePattern);
    String_delete(storageSpecifier->archivePatternString);
  }
  String_delete(storageSpecifier->archiveName);
  String_delete(storageSpecifier->deviceName);
  Password_delete(storageSpecifier->loginPassword);
  String_delete(storageSpecifier->loginName);
  String_delete(storageSpecifier->hostName);
}

bool Storage_equalSpecifiers(const StorageSpecifier *storageSpecifier1,
                             ConstString            archiveName1,
                             const StorageSpecifier *storageSpecifier2,
                             ConstString            archiveName2
                            )
{
  bool result;

  assert(storageSpecifier1 != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageSpecifier1);
  assert(storageSpecifier2 != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageSpecifier2);

  result = FALSE;

  if (storageSpecifier1->type == storageSpecifier2->type)
  {
    switch (storageSpecifier1->type)
    {
      case STORAGE_TYPE_FILESYSTEM:
        result = StorageFile_equalSpecifiers(storageSpecifier1,archiveName1,storageSpecifier2,archiveName2);
        break;
      case STORAGE_TYPE_FTP:
        result = StorageFTP_equalSpecifiers(storageSpecifier1,archiveName1,storageSpecifier2,archiveName2);
        break;
      case STORAGE_TYPE_SSH:
      case STORAGE_TYPE_SCP:
        result = StorageSCP_equalSpecifiers(storageSpecifier1,archiveName1,storageSpecifier2,archiveName2);
        break;
      case STORAGE_TYPE_SFTP:
        result = StorageSFTP_equalSpecifiers(storageSpecifier1,archiveName1,storageSpecifier2,archiveName2);
        break;
      case STORAGE_TYPE_WEBDAV:
        result = StorageWebDAV_equalSpecifiers(storageSpecifier1,archiveName1,storageSpecifier2,archiveName2);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        result = StorageOptical_equalSpecifiers(storageSpecifier1,archiveName1,storageSpecifier2,archiveName2);
        break;
      case STORAGE_TYPE_DEVICE:
        result = StorageDevice_equalSpecifiers(storageSpecifier1,archiveName1,storageSpecifier2,archiveName2);
        break;
      default:
        break;
    }
  }

  return result;
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
  if      (String_matchCString(sshSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^:]+?):(\\d*)/{0,1}$",NULL,STRING_NO_ASSIGN,loginName,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <login name>@<host name>:<host port>
    if (loginName != NULL) String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (hostPort != NULL)
    {
      if (!String_isEmpty(s)) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);
    }

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^/]+)/{0,1}$",NULL,STRING_NO_ASSIGN,loginName,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>@<host name>
    if (loginName != NULL) String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^([^@:/]*?):(\\d*)/{0,1}$",NULL,STRING_NO_ASSIGN,hostName,s,NULL))
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

bool Storage_parseSCPSpecifier(ConstString scpSpecifier,
                               String      hostName,
                               uint        *hostPort,
                               String      loginName,
                               Password    *loginPassword
                              )
{
  assert(scpSpecifier != NULL);
  assert(hostName != NULL);
  assert(loginName != NULL);

  return StorageSCP_parseSpecifier(scpSpecifier,hostName,hostPort,loginName,loginPassword);
}

bool Storage_parseSFTPSpecifier(ConstString sftpSpecifier,
                                String      hostName,
                                uint        *hostPort,
                                String      loginName,
                                Password    *loginPassword
                               )
{
  assert(sftpSpecifier != NULL);
  assert(hostName != NULL);
  assert(loginName != NULL);

  return StorageSFTP_parseSpecifier(sftpSpecifier,hostName,hostPort,loginName,loginPassword);
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
  assert(deviceName != NULL);

  return StorageOptical_parseSpecifier(opticalSpecifier,defaultDeviceName,deviceName);
}

bool Storage_parseDeviceSpecifier(ConstString deviceSpecifier,
                                  ConstString defaultDeviceName,
                                  String      deviceName
                                 )
{
  assert(deviceSpecifier != NULL);
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
  Errors          error;

  assert(storageSpecifier != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageSpecifier);
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
      String_sub(string,storageName,6,nextIndex-6);
      String_trimEnd(string,"/");
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
      String_sub(archiveName,storageName,nextIndex,STRING_END);
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
      String_sub(string,storageName,6,nextIndex-6);
      String_trimEnd(string,"/");
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
      String_sub(archiveName,storageName,nextIndex,STRING_END);
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
      String_sub(string,storageName,6,nextIndex-6);
      String_trimEnd(string,"/");
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
      String_sub(archiveName,storageName,nextIndex,STRING_END);
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
      String_sub(string,storageName,7,nextIndex-7);
      String_trimEnd(string,"/");
      if (!StorageSFTP_parseSpecifier(string,
                                      storageSpecifier->hostName,
                                      &storageSpecifier->hostPort,
                                      storageSpecifier->loginName,
                                      storageSpecifier->loginPassword
                                     )
         )
      {
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_SSH_SPECIFIER;
      }
      String_sub(archiveName,storageName,nextIndex,STRING_END);
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
      String_sub(string,storageName,9,nextIndex-9);
      String_trimEnd(string,"/");
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
      String_sub(archiveName,storageName,nextIndex,STRING_END);
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
    if (String_matchCString(storageName,5,"^[^:]*:",&nextIndex,NULL,NULL))  // cd://<device>:<file name>
    {
      String_sub(string,storageName,5,nextIndex-5);
      String_trimEnd(string,"/");
      if (!Storage_parseOpticalSpecifier(string,
                                         NULL,  // defaultDeviceName
                                         storageSpecifier->deviceName
                                        )
         )
      {
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      String_sub(archiveName,storageName,nextIndex,STRING_END);
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
    if (String_matchCString(storageName,6,"^[^:]*:",&nextIndex,NULL,NULL))  // dvd://<device>:<file name>
    {
      String_sub(string,storageName,6,nextIndex-6);
      String_trimEnd(string,"/");
      if (!Storage_parseOpticalSpecifier(string,
                                         NULL,  // defaultDeviceName
                                         storageSpecifier->deviceName
                                        )
         )
      {
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      String_sub(archiveName,storageName,nextIndex,STRING_END);
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
    if (String_matchCString(storageName,5,"^[^:]*:",&nextIndex,NULL,NULL))  // bd://<device>:<file name>
    {
      String_sub(string,storageName,5,nextIndex-5);
      String_trimEnd(string,"/");
      if (!Storage_parseOpticalSpecifier(string,
                                         NULL,  // defaultDeviceName
                                         storageSpecifier->deviceName
                                        )
         )
      {
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      String_sub(archiveName,storageName,nextIndex,STRING_END);
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
    if (String_matchCString(storageName,9,"^[^:]*:",&nextIndex,NULL,NULL))  // device://<device>:<file name>
    {
      String_sub(string,storageName,9,nextIndex-9);
      String_trimEnd(string,"/");
      if (!Storage_parseDeviceSpecifier(string,
                                        NULL,
                                        storageSpecifier->deviceName
                                       )
         )
      {
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      String_sub(archiveName,storageName,nextIndex,STRING_END);
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

  // get archive base name (all which is not a pattern), check if pattern
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

  if (hasPatternFlag)
  {
    // clean-up
    if (storageSpecifier->archivePatternString != NULL)
    {
      Pattern_done(&storageSpecifier->archivePattern);
      String_delete(storageSpecifier->archivePatternString);
    }

    // get file pattern string
    storageSpecifier->archivePatternString = String_new();
    File_initSplitFileName(&archiveNameTokenizer,archiveName);
    {
      if (File_getNextSplitFileName(&archiveNameTokenizer,&token))
      {
        if (!String_isEmpty(token))
        {
          File_setFileName(storageSpecifier->archivePatternString,token);
        }
        else
        {
          File_setFileNameChar(storageSpecifier->archivePatternString,FILE_SEPARATOR_CHAR);
        }
        while (File_getNextSplitFileName(&archiveNameTokenizer,&token))
        {
          File_appendFileName(storageSpecifier->archivePatternString,token);
        }
      }
    }
    File_doneSplitFileName(&archiveNameTokenizer);

    // parse file pattern
    error = Pattern_init(&storageSpecifier->archivePattern,
                         storageSpecifier->archivePatternString,
//TODO: glob? parameter?
                         PATTERN_TYPE_GLOB,
                         PATTERN_FLAG_NONE
                        );
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
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

  result = FALSE;

  // init variables
  Storage_initSpecifier(&storageSpecifier1);
  Storage_initSpecifier(&storageSpecifier2);

  // parse storage names and compare
  if (   (Storage_parseName(&storageSpecifier1,storageName1) == ERROR_NONE)
      && (Storage_parseName(&storageSpecifier2,storageName2) == ERROR_NONE)
     )
  {
    result = Storage_equalSpecifiers(&storageSpecifier1,NULL,&storageSpecifier2,NULL);
  }

  // free resources
  Storage_doneSpecifier(&storageSpecifier2);
  Storage_doneSpecifier(&storageSpecifier1);

  return result;
}

String Storage_getName(String           string,
                       StorageSpecifier *storageSpecifier,
                       ConstString      archiveName
                      )
{
  assert(storageSpecifier != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageSpecifier);

  // get result variable to use
  if (string == NULL)
  {
    string = storageSpecifier->storageName;
  }

  // get file to use
  if (archiveName == NULL)
  {
    if (storageSpecifier->archivePatternString != NULL)
    {
      archiveName = storageSpecifier->archivePatternString;
    }
    else
    {
      archiveName = storageSpecifier->archiveName;
    }
  }

  String_clear(storageSpecifier->storageName);
  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      StorageFile_getName(string,storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_FTP:
      StorageFTP_getName(string,storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_SSH:
      if (!String_isEmpty(archiveName))
      {
        String_append(string,archiveName);
      }
      break;
    case STORAGE_TYPE_SCP:
      StorageSCP_getName(string,storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_SFTP:
      StorageSFTP_getName(string,storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_WEBDAV:
      StorageWebDAV_getName(string,storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      StorageOptical_getName(string,storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_DEVICE:
      StorageDevice_getName(string,storageSpecifier,archiveName);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return string;
}

String Storage_getPrintableName(String                 string,
                                const StorageSpecifier *storageSpecifier,
                                ConstString            archiveName
                               )
{
  assert(storageSpecifier != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageSpecifier);

  // get result variable to use
  if (string == NULL)
  {
    string = storageSpecifier->storageName;
  }

  // get file to use
  if (archiveName == NULL)
  {
//TODO: implement Storage_getPattern()
    if (storageSpecifier->archivePatternString != NULL)
    {
      archiveName = storageSpecifier->archivePatternString;
    }
    else
    {
      archiveName = storageSpecifier->archiveName;
    }
  }

  String_clear(string);
  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      StorageFile_getPrintableName(string,storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_FTP:
      StorageFTP_getPrintableName(string,storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_SSH:
      if (!String_isEmpty(archiveName))
      {
        String_append(string,archiveName);
      }
      break;
    case STORAGE_TYPE_SCP:
      StorageSCP_getPrintableName(string,storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_SFTP:
      StorageSFTP_getPrintableName(string,storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_WEBDAV:
      StorageWebDAV_getPrintableName(string,storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      StorageOptical_getPrintableName(string,storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_DEVICE:
      StorageDevice_getPrintableName(string,storageSpecifier,archiveName);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return string;
}

#ifdef NDEBUG
  Errors Storage_init(StorageInfo                     *storageInfo,
                      ServerIO                        *masterIO,
                      const StorageSpecifier          *storageSpecifier,
                      const JobOptions                *jobOptions,
                      BandWidthList                   *maxBandWidthList,
                      ServerConnectionPriorities      serverConnectionPriority,
                      StorageUpdateStatusInfoFunction storageUpdateStatusInfoFunction,
                      void                            *storageUpdateStatusInfoUserData,
                      GetNamePasswordFunction         getNamePasswordFunction,
                      void                            *getNamePasswordUserData,
                      StorageRequestVolumeFunction    storageRequestVolumeFunction,
                      void                            *storageRequestVolumeUserData
                     )
#else /* not NDEBUG */
  Errors __Storage_init(const char                      *__fileName__,
                        ulong                           __lineNb__,
                        StorageInfo                     *storageInfo,
                        ServerIO                        *masterIO,
                        const StorageSpecifier          *storageSpecifier,
                        const JobOptions                *jobOptions,
                        BandWidthList                   *maxBandWidthList,
                        ServerConnectionPriorities      serverConnectionPriority,
                        StorageUpdateStatusInfoFunction storageUpdateStatusInfoFunction,
                        void                            *storageUpdateStatusInfoUserData,
                        GetNamePasswordFunction         getNamePasswordFunction,
                        void                            *getNamePasswordUserData,
                        StorageRequestVolumeFunction    storageRequestVolumeFunction,
                        void                            *storageRequestVolumeUserData
                       )
#endif /* NDEBUG */
{
  AutoFreeList autoFreeList;
  Errors       error;

  assert(storageInfo != NULL);
  assert(storageSpecifier != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageSpecifier);

  #if !defined(HAVE_CURL) && !defined(HAVE_FTP) && !defined(HAVE_SSH2)
    UNUSED_VARIABLE(serverConnectionPriority);
  #endif /* !defined(HAVE_CURL) && !defined(HAVE_FTP) && !defined(HAVE_SSH2) */

  // initialize variables
  AutoFree_init(&autoFreeList);
  Semaphore_init(&storageInfo->lock,SEMAPHORE_TYPE_BINARY);
  Storage_duplicateSpecifier(&storageInfo->storageSpecifier,storageSpecifier);
  storageInfo->jobOptions                = jobOptions;
  storageInfo->updateStatusInfoFunction  = storageUpdateStatusInfoFunction;
  storageInfo->updateStatusInfoUserData  = storageUpdateStatusInfoUserData;
  storageInfo->getNamePasswordFunction   = getNamePasswordFunction;
  storageInfo->getNamePasswordUserData   = getNamePasswordUserData;
  storageInfo->requestVolumeFunction     = storageRequestVolumeFunction;
  storageInfo->requestVolumeUserData     = storageRequestVolumeUserData;
  if ((jobOptions != NULL) && jobOptions->waitFirstVolumeFlag)
  {
    storageInfo->volumeNumber            = 0;
    storageInfo->requestedVolumeNumber   = 0;
    storageInfo->volumeState             = STORAGE_VOLUME_STATE_UNKNOWN;
  }
  else
  {
    storageInfo->volumeNumber            = 1;
    storageInfo->requestedVolumeNumber   = 1;
    storageInfo->volumeState             = STORAGE_VOLUME_STATE_LOADED;
  }
  AUTOFREE_ADD(&autoFreeList,&storageInfo->lock,{ Semaphore_done(&storageInfo->lock); });
  AUTOFREE_ADD(&autoFreeList,&storageInfo->storageSpecifier,{ Storage_doneSpecifier(&storageInfo->storageSpecifier); });

  if (masterIO != NULL)
  {
    storageInfo->type      = STORAGE_TYPE_MASTER;
    storageInfo->master.io = masterIO;
  }
  else
  {
    storageInfo->type      = storageInfo->storageSpecifier.type;
  }

  // init protocol specific values
  error = ERROR_UNKNOWN;
  switch (storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      UNUSED_VARIABLE(maxBandWidthList);

      error = ERROR_NONE;
      break;
    case STORAGE_TYPE_FILESYSTEM:
      UNUSED_VARIABLE(maxBandWidthList);

      error = StorageFile_init(storageInfo,storageSpecifier,jobOptions);
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_init(storageInfo,storageSpecifier,jobOptions,maxBandWidthList,serverConnectionPriority);
      break;
    case STORAGE_TYPE_SSH:
      UNUSED_VARIABLE(maxBandWidthList);

      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_init(storageInfo,storageSpecifier,jobOptions,maxBandWidthList,serverConnectionPriority);
      break;
    case STORAGE_TYPE_SFTP:
      error = StorageSFTP_init(storageInfo,storageSpecifier,jobOptions,maxBandWidthList,serverConnectionPriority);
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_init(storageInfo,storageSpecifier,jobOptions,maxBandWidthList,serverConnectionPriority);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_init(storageInfo,storageSpecifier,jobOptions);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_init(storageInfo,storageSpecifier,jobOptions);
      break;
    case STORAGE_TYPE_MASTER:
      error = StorageMaster_init(storageInfo,storageSpecifier,jobOptions);
      break;
    default:
      UNUSED_VARIABLE(maxBandWidthList);

      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  assert(error != ERROR_UNKNOWN);
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  storageInfo->runningInfo.volumeNumber   = 0;
  storageInfo->runningInfo.volumeProgress = 0;

  // free resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(storageInfo,sizeof(Storage));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,storageInfo,sizeof(StorageInfo));
  #endif /* NDEBUG */

  return ERROR_NONE;
}

#ifdef NDEBUG
  Errors Storage_done(StorageInfo *storageInfo)
#else /* not NDEBUG */
  Errors __Storage_done(const char  *__fileName__,
                        ulong       __lineNb__,
                        StorageInfo *storageInfo
                       )
#endif /* NDEBUG */
{
  Errors error;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(storageInfo,sizeof(Storage));
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,storageInfo,sizeof(StorageInfo));
  #endif /* NDEBUG */

  error = ERROR_NONE;

  switch (storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      error = StorageFile_done(storageInfo);
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_done(storageInfo);
      break;
    case STORAGE_TYPE_SSH:
      // free SSH server connection
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_done(storageInfo);
      break;
    case STORAGE_TYPE_SFTP:
      error = StorageSFTP_done(storageInfo);
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_done(storageInfo);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_done(storageInfo);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_done(storageInfo);
      break;
    case STORAGE_TYPE_MASTER:
error = ERROR_STILL_NOT_IMPLEMENTED;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  Storage_doneSpecifier(&storageInfo->storageSpecifier);
  Semaphore_done(&storageInfo->lock);

  return error;
}

bool Storage_isServerAllocationPending(StorageInfo *storageInfo)
{
  bool serverAllocationPending;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  serverAllocationPending = FALSE;
  switch (storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      serverAllocationPending = StorageFile_isServerAllocationPending(storageInfo);
      break;
    case STORAGE_TYPE_FTP:
      serverAllocationPending = StorageFTP_isServerAllocationPending(storageInfo);
      break;
    case STORAGE_TYPE_SSH:
      #if defined(HAVE_SSH2)
        serverAllocationPending = isServerAllocationPending(storageInfo->ssh.serverId);
      #else /* not HAVE_SSH2 */
        serverAllocationPending = FALSE;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      serverAllocationPending = StorageSCP_isServerAllocationPending(storageInfo);
      break;
    case STORAGE_TYPE_SFTP:
      serverAllocationPending = StorageSFTP_isServerAllocationPending(storageInfo);
      break;
    case STORAGE_TYPE_WEBDAV:
      serverAllocationPending = StorageWebDAV_isServerAllocationPending(storageInfo);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      break;
    case STORAGE_TYPE_DEVICE:
      break;
    case STORAGE_TYPE_MASTER:
//TODO
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return serverAllocationPending;
}

const StorageSpecifier *Storage_getStorageSpecifier(const StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);

  return &storageInfo->storageSpecifier;
}

Errors Storage_preProcess(StorageInfo *storageInfo,
                          ConstString archiveName,
                          time_t      time,
                          bool        initialFlag
                         )
{
  Errors error;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  error = ERROR_NONE;
  switch (storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      error = StorageFile_preProcess(storageInfo,archiveName,time,initialFlag);
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_preProcess(storageInfo,archiveName,time,initialFlag);
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_preProcess(storageInfo,archiveName,time,initialFlag);
      break;
    case STORAGE_TYPE_SFTP:
      error = StorageSFTP_preProcess(storageInfo,archiveName,time,initialFlag);
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_preProcess(storageInfo,archiveName,time,initialFlag);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_preProcess(storageInfo,archiveName,time,initialFlag);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_preProcess(storageInfo,archiveName,time,initialFlag);
      break;
    case STORAGE_TYPE_MASTER:
      error = StorageMaster_preProcess(storageInfo,archiveName,time,initialFlag);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

Errors Storage_postProcess(StorageInfo *storageInfo,
                           ConstString archiveName,
                           time_t      time,
                           bool        finalFlag
                          )
{
  Errors error;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  error = ERROR_NONE;
  switch (storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      error = StorageFile_postProcess(storageInfo,archiveName,time,finalFlag);
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_postProcess(storageInfo,archiveName,time,finalFlag);
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_postProcess(storageInfo,archiveName,time,finalFlag);
      break;
    case STORAGE_TYPE_SFTP:
      error = StorageSFTP_postProcess(storageInfo,archiveName,time,finalFlag);
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_postProcess(storageInfo,archiveName,time,finalFlag);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_postProcess(storageInfo,archiveName,time,finalFlag);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_postProcess(storageInfo,archiveName,time,finalFlag);
      break;
    case STORAGE_TYPE_MASTER:
      error = StorageMaster_postProcess(storageInfo,archiveName,time,finalFlag);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
       #endif /* NDEBUG */
      break;
  }

  return error;
}

uint Storage_getVolumeNumber(const StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);

  return storageInfo->volumeNumber;
}

void Storage_setVolumeNumber(StorageInfo *storageInfo,
                             uint        volumeNumber
                            )
{
  assert(storageInfo != NULL);

  storageInfo->volumeNumber = volumeNumber;
}

Errors Storage_unloadVolume(StorageInfo *storageInfo)
{
  Errors error;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  error = ERROR_UNKNOWN;
  switch (storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
    case STORAGE_TYPE_FILESYSTEM:
      error = ERROR_NONE;
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_unloadVolume(storageInfo);
      break;
    case STORAGE_TYPE_SSH:
    case STORAGE_TYPE_SCP:
      error = ERROR_NONE;
      break;
    case STORAGE_TYPE_SFTP:
      error = ERROR_NONE;
      break;
    case STORAGE_TYPE_WEBDAV:
      error = ERROR_NONE;
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_unloadVolume(storageInfo);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_unloadVolume(storageInfo);
      break;
    case STORAGE_TYPE_MASTER:
error = ERROR_STILL_NOT_IMPLEMENTED;
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

bool Storage_exists(StorageInfo *storageInfo, ConstString archiveName)
{
  bool existsFlag;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  // get archive name
  if (archiveName == NULL) archiveName = storageInfo->storageSpecifier.archiveName;
  if (String_isEmpty(archiveName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  existsFlag = FALSE;
  switch (storageInfo->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      existsFlag = StorageFile_exists(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_FTP:
      existsFlag = StorageFTP_exists(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_SSH:
      existsFlag = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      existsFlag = StorageSCP_exists(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_SFTP:
      existsFlag = StorageSFTP_exists(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_WEBDAV:
      existsFlag = StorageWebDAV_exists(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      existsFlag = StorageOptical_exists(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_DEVICE:
      existsFlag = StorageDevice_exists(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_MASTER:
//TODO
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return existsFlag;
}

bool Storage_isFile(StorageInfo *storageInfo, ConstString archiveName)
{
  bool isFileFlag;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  // get archive name
  if (archiveName == NULL) archiveName = storageInfo->storageSpecifier.archiveName;
  if (String_isEmpty(archiveName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  isFileFlag = FALSE;
  switch (storageInfo->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      isFileFlag = StorageFile_isFile(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_FTP:
      isFileFlag = StorageFTP_isFile(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_SSH:
      isFileFlag = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      isFileFlag = StorageSCP_isFile(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_SFTP:
      isFileFlag = StorageSFTP_isFile(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_WEBDAV:
      isFileFlag = StorageWebDAV_isFile(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      isFileFlag = StorageOptical_isFile(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_DEVICE:
      isFileFlag = StorageDevice_isFile(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_MASTER:
//TODO
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return isFileFlag;
}

bool Storage_isDirectory(StorageInfo *storageInfo, ConstString archiveName)
{
  bool isDirectoryFlag;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  // get archive name
  if (archiveName == NULL) archiveName = storageInfo->storageSpecifier.archiveName;
  if (String_isEmpty(archiveName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  isDirectoryFlag = FALSE;
  switch (storageInfo->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      isDirectoryFlag = StorageFile_isDirectory(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_FTP:
      isDirectoryFlag = StorageFTP_isDirectory(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_SSH:
      isDirectoryFlag = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      isDirectoryFlag = StorageSCP_isDirectory(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_SFTP:
      isDirectoryFlag = StorageSFTP_isDirectory(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_WEBDAV:
      isDirectoryFlag = StorageWebDAV_isDirectory(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      isDirectoryFlag = StorageOptical_isDirectory(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_DEVICE:
      isDirectoryFlag = StorageDevice_isDirectory(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_MASTER:
//TODO
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return isDirectoryFlag;
}

bool Storage_isReadable(StorageInfo *storageInfo, ConstString archiveName)
{
  bool isReadableFlag;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  // get archive name
  if (archiveName == NULL) archiveName = storageInfo->storageSpecifier.archiveName;
  if (String_isEmpty(archiveName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  isReadableFlag = FALSE;
  switch (storageInfo->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      isReadableFlag = StorageFile_isReadable(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_FTP:
      isReadableFlag = StorageFTP_isReadable(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_SSH:
      isReadableFlag = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      isReadableFlag = StorageSCP_isReadable(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_SFTP:
      isReadableFlag = StorageSFTP_isReadable(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_WEBDAV:
      isReadableFlag = StorageWebDAV_isReadable(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      isReadableFlag = StorageOptical_isReadable(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_DEVICE:
      isReadableFlag = StorageDevice_isReadable(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_MASTER:
//TODO
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return isReadableFlag;
}

bool Storage_isWriteable(StorageInfo *storageInfo, ConstString archiveName)
{
  bool isWriteableFlag;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  // get archive name
  if (archiveName == NULL) archiveName = storageInfo->storageSpecifier.archiveName;
  if (String_isEmpty(archiveName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  isWriteableFlag = FALSE;
  switch (storageInfo->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      isWriteableFlag = StorageFile_isWriteable(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_FTP:
      isWriteableFlag = StorageFTP_isWriteable(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_SSH:
      isWriteableFlag = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      isWriteableFlag = StorageSCP_isWriteable(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_SFTP:
      isWriteableFlag = StorageSFTP_isWriteable(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_WEBDAV:
      isWriteableFlag = StorageWebDAV_isWriteable(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      isWriteableFlag = StorageOptical_isWriteable(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_DEVICE:
      isWriteableFlag = StorageDevice_isWriteable(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_MASTER:
//TODO
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return isWriteableFlag;
}

Errors Storage_getTmpName(String archiveName, StorageInfo *storageInfo)
{
  Errors error;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  if (!String_isEmpty(storageInfo->storageSpecifier.archiveName))
  {
    String_set(archiveName,storageInfo->storageSpecifier.archiveName);
  }
  else
  {
    String_setCString(archiveName,"archive");
  }

  error = ERROR_UNKNOWN;
  switch (storageInfo->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      error = StorageFile_getTmpName(archiveName,storageInfo);
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_getTmpName(archiveName,storageInfo);
      break;
    case STORAGE_TYPE_SSH:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_getTmpName(archiveName,storageInfo);
      break;
    case STORAGE_TYPE_SFTP:
      error = StorageSFTP_getTmpName(archiveName,storageInfo);
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_getTmpName(archiveName,storageInfo);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_getTmpName(archiveName,storageInfo);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_getTmpName(archiveName,storageInfo);
      break;
    case STORAGE_TYPE_MASTER:
      error = StorageMaster_getTmpName(archiveName,storageInfo);
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

#ifdef NDEBUG
  Errors Storage_create(StorageHandle *storageHandle,
                        StorageInfo   *storageInfo,
                        ConstString   archiveName,
                        uint64        archiveSize
                       )
#else /* not NDEBUG */
  Errors __Storage_create(const char    *__fileName__,
                          ulong         __lineNb__,
                          StorageHandle *storageHandle,
                          StorageInfo   *storageInfo,
                          ConstString   archiveName,
                          uint64        archiveSize
                         )
#endif /* NDEBUG */
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  // init variables
  storageHandle->storageInfo = storageInfo;
  storageHandle->mode        = STORAGE_MODE_WRITE;

  // get archive name
  if (archiveName == NULL) archiveName = storageInfo->storageSpecifier.archiveName;
  if (String_isEmpty(archiveName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  error = ERROR_UNKNOWN;
  switch (storageInfo->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      error = StorageFile_create(storageHandle,archiveName,archiveSize);
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_create(storageHandle,archiveName,archiveSize);
      break;
    case STORAGE_TYPE_SSH:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_create(storageHandle,archiveName,archiveSize);
      break;
    case STORAGE_TYPE_SFTP:
      error = StorageSFTP_create(storageHandle,archiveName,archiveSize);
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
    case STORAGE_TYPE_MASTER:
      error = StorageMaster_create(storageHandle,archiveName,archiveSize);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  assert(error != ERROR_UNKNOWN);
  if (error != ERROR_NONE)
  {
    return error;
  }

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(storageHandle,sizeof(StorageHandle));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,storageHandle,sizeof(StorageHandle));
  #endif /* NDEBUG */

  return error;
}

#ifdef NDEBUG
  Errors Storage_open(StorageHandle *storageHandle,
                      StorageInfo   *storageInfo,
                      ConstString   archiveName
                     )
#else /* not NDEBUG */
  Errors __Storage_open(const char    *__fileName__,
                        ulong         __lineNb__,
                        StorageHandle *storageHandle,
                        StorageInfo   *storageInfo,
                        ConstString   archiveName
                       )
#endif /* NDEBUG */
{
  Errors error;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  // init variables
  storageHandle->storageInfo = storageInfo;
  storageHandle->mode        = STORAGE_MODE_READ;

  // get archive name
  if (archiveName == NULL) archiveName = storageInfo->storageSpecifier.archiveName;
  if (String_isEmpty(archiveName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  error = ERROR_UNKNOWN;
  switch (storageHandle->storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      error = StorageFile_open(storageHandle,archiveName);
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_open(storageHandle,archiveName);
      break;
    case STORAGE_TYPE_SSH:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_open(storageHandle,archiveName);
      break;
    case STORAGE_TYPE_SFTP:
      error = StorageSFTP_open(storageHandle,archiveName);
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
    case STORAGE_TYPE_MASTER:
fprintf(stderr,"%s, %d: Storage_open\n",__FILE__,__LINE__);
error = ERROR_STILL_NOT_IMPLEMENTED;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  assert(error != ERROR_UNKNOWN);
  if (error != ERROR_NONE)
  {
    return error;
  }

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(storageHandle,sizeof(StorageHandle));
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,storageHandle,sizeof(StorageHandle));
  #endif /* NDEBUG */

  return error;
}

#ifdef NDEBUG
  void Storage_close(StorageHandle *storageHandle)
#else /* not NDEBUG */
  void __Storage_close(const char    *__fileName__,
                       ulong         __lineNb__,
                       StorageHandle *storageHandle
                      )
#endif /* NDEBUG */
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle->storageInfo);

  switch (storageHandle->storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      StorageFile_close(storageHandle);
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
      StorageSFTP_close(storageHandle);
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
    case STORAGE_TYPE_MASTER:
      StorageMaster_close(storageHandle);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(storageHandle,sizeof(StorageHandle));
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,storageHandle,sizeof(StorageHandle));
  #endif /* NDEBUG */
}

bool Storage_eof(StorageHandle *storageHandle)
{
  bool eofFlag;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle->storageInfo);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle->storageInfo);

  eofFlag = TRUE;
  switch (storageHandle->storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      eofFlag = StorageFile_eof(storageHandle);
      break;
    case STORAGE_TYPE_FTP:
      eofFlag = StorageFTP_eof(storageHandle);
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      eofFlag = StorageSCP_eof(storageHandle);
      break;
    case STORAGE_TYPE_SFTP:
      eofFlag = StorageSFTP_eof(storageHandle);
      break;
    case STORAGE_TYPE_WEBDAV:
      eofFlag = StorageWebDAV_eof(storageHandle);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      eofFlag = StorageOptical_eof(storageHandle);
      break;
    case STORAGE_TYPE_DEVICE:
      eofFlag = StorageDevice_eof(storageHandle);
      break;
    case STORAGE_TYPE_MASTER:
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
//TODO
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return eofFlag;
}

Errors Storage_read(StorageHandle *storageHandle,
                    void          *buffer,
                    ulong         bufferSize,
                    ulong         *bytesRead
                   )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle->storageInfo);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(buffer != NULL);

  if (bytesRead != NULL) (*bytesRead) = 0L;
  error = ERROR_NONE;
  switch (storageHandle->storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      error = StorageFile_read(storageHandle,buffer,bufferSize,bytesRead);
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_read(storageHandle,buffer,bufferSize,bytesRead);
      break;
    case STORAGE_TYPE_SSH:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_read(storageHandle,buffer,bufferSize,bytesRead);
      break;
    case STORAGE_TYPE_SFTP:
      error = StorageSFTP_read(storageHandle,buffer,bufferSize,bytesRead);
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_read(storageHandle,buffer,bufferSize,bytesRead);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_read(storageHandle,buffer,bufferSize,bytesRead);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_read(storageHandle,buffer,bufferSize,bytesRead);
      break;
    case STORAGE_TYPE_MASTER:
error = ERROR_STILL_NOT_IMPLEMENTED;
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
                     ulong         bufferLength
                    )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle->storageInfo);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(buffer != NULL);

  error = ERROR_NONE;
  switch (storageHandle->storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      error = StorageFile_write(storageHandle,buffer,bufferLength);
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_write(storageHandle,buffer,bufferLength);
      break;
    case STORAGE_TYPE_SSH:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_write(storageHandle,buffer,bufferLength);
      break;
    case STORAGE_TYPE_SFTP:
      error = StorageSFTP_write(storageHandle,buffer,bufferLength);
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_write(storageHandle,buffer,bufferLength);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_write(storageHandle,buffer,bufferLength);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_write(storageHandle,buffer,bufferLength);
      break;
    case STORAGE_TYPE_MASTER:
      error = StorageMaster_write(storageHandle,buffer,bufferLength);
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

Errors Storage_tell(StorageHandle *storageHandle,
                    uint64        *offset
                   )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle->storageInfo);
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  switch (storageHandle->storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      error = StorageFile_tell(storageHandle,offset);
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
      error = StorageSFTP_tell(storageHandle,offset);
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
    case STORAGE_TYPE_MASTER:
error = ERROR_STILL_NOT_IMPLEMENTED;
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
  assert(storageHandle->storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle->storageInfo);

  error = ERROR_NONE;
  switch (storageHandle->storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      error = StorageFile_seek(storageHandle,offset);
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
      error = StorageSFTP_seek(storageHandle,offset);
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
    case STORAGE_TYPE_MASTER:
error = ERROR_STILL_NOT_IMPLEMENTED;
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
  assert(storageHandle->storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle->storageInfo);

  size = 0LL;
  switch (storageHandle->storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      size = StorageFile_getSize(storageHandle);
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
      size = StorageSFTP_getSize(storageHandle);
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
    case STORAGE_TYPE_MASTER:
      size = StorageMaster_getSize(storageHandle);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return size;
}

Errors Storage_rename(const StorageInfo *storageInfo,
                      ConstString       fromArchiveName,
                      ConstString       toArchiveName
                     )
{
  Errors error;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  // get archive names
  if (fromArchiveName == NULL) fromArchiveName = storageInfo->storageSpecifier.archiveName;
  if (toArchiveName == NULL) toArchiveName = storageInfo->storageSpecifier.archiveName;
  if (String_isEmpty(fromArchiveName) || String_isEmpty(toArchiveName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  error = ERROR_UNKNOWN;
  switch (storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      error = StorageFile_rename(storageInfo,fromArchiveName,toArchiveName);
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_rename(storageInfo,fromArchiveName,toArchiveName);
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_rename(storageInfo,fromArchiveName,toArchiveName);
      break;
    case STORAGE_TYPE_SFTP:
      error = StorageSFTP_rename(storageInfo,fromArchiveName,toArchiveName);
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_rename(storageInfo,fromArchiveName,toArchiveName);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_rename(storageInfo,fromArchiveName,toArchiveName);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_rename(storageInfo,fromArchiveName,toArchiveName);
      break;
    case STORAGE_TYPE_MASTER:
error = ERROR_STILL_NOT_IMPLEMENTED;
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

Errors Storage_delete(StorageInfo *storageInfo, ConstString archiveName)
{
  Errors error;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  // get archive name
  if (archiveName == NULL) archiveName = storageInfo->storageSpecifier.archiveName;
  if (String_isEmpty(archiveName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  error = ERROR_UNKNOWN;
  switch (storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      error = StorageFile_delete(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_delete(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_delete(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_SFTP:
      error = StorageSFTP_delete(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_delete(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_delete(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_delete(storageInfo,archiveName);
      break;
    case STORAGE_TYPE_MASTER:
error = ERROR_STILL_NOT_IMPLEMENTED;
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

Errors Storage_pruneDirectories(StorageInfo *storageInfo, ConstString archiveName)
{
  String                     directoryName;
  JobOptions                 jobOptions;
  bool                       isEmpty;
  Errors                     error;
  StorageDirectoryListHandle storageDirectoryListHandle;

  // get archive name
  if (archiveName == NULL) archiveName = storageInfo->storageSpecifier.archiveName;
  if (String_isEmpty(archiveName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  directoryName = File_getDirectoryName(String_new(),archiveName);
  Job_initOptions(&jobOptions);
  do
  {
    // check if directory is empty
    isEmpty = FALSE;
    error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                      &storageInfo->storageSpecifier,
                                      directoryName,
                                      &jobOptions,
                                      SERVER_CONNECTION_PRIORITY_LOW
                                     );
    if (error == ERROR_NONE)
    {
      isEmpty = Storage_endOfDirectoryList(&storageDirectoryListHandle);
      Storage_closeDirectoryList(&storageDirectoryListHandle);

      // delete empty directory
      if (isEmpty)
      {
        switch (storageInfo->type)
        {
          case STORAGE_TYPE_NONE:
            break;
          case STORAGE_TYPE_FILESYSTEM:
            error = StorageFile_delete(storageInfo,directoryName);
            break;
          case STORAGE_TYPE_FTP:
            error = StorageFTP_delete(storageInfo,directoryName);
            break;
          case STORAGE_TYPE_SSH:
            #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
            #else /* not HAVE_SSH2 */
            #endif /* HAVE_SSH2 */
            break;
          case STORAGE_TYPE_SCP:
            error = StorageSCP_delete(storageInfo,directoryName);
            break;
          case STORAGE_TYPE_SFTP:
            error = StorageSFTP_delete(storageInfo,directoryName);
            break;
          case STORAGE_TYPE_WEBDAV:
            error = StorageWebDAV_delete(storageInfo,directoryName);
            break;
          case STORAGE_TYPE_CD:
          case STORAGE_TYPE_DVD:
          case STORAGE_TYPE_BD:
            error = StorageOptical_delete(storageInfo,directoryName);
            break;
          case STORAGE_TYPE_DEVICE:
            error = StorageDevice_delete(storageInfo,directoryName);
            break;
          case STORAGE_TYPE_MASTER:
error = ERROR_STILL_NOT_IMPLEMENTED;
            break;
          default:
            #ifndef NDEBUG
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            #endif /* NDEBUG */
            break;
        }
      }
    }

    // get parent directory
    File_getDirectoryName(directoryName,directoryName);
  }
  while (   (error == ERROR_NONE)
         && isEmpty
         && !String_isEmpty(directoryName)
        );
  Job_doneOptions(&jobOptions);
  String_delete(directoryName);

  return error;
}

#if 0
still not complete
Errors Storage_getInfo(StorageInfo *storageInfo,
                       FileInfo    *fileInfo
                      )
{
  String infoFileName;
  Errors error;

  assert(storageInfo != NULL);
  assert(fileInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  infoFileName = (fileName != NULL) ? archiveName : storageInfo->storageSpecifier.archiveName;
  memset(fileInfo,0,sizeof(fileInfo));

  error = ERROR_UNKNOWN;
  switch (storageInfo->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      errors = StorageFile_getInfo(storageInfo,archiveName,fileInfo);
      break;
    case STORAGE_TYPE_FTP:
      errors = StorageFTP_getInfo(storageInfo,archiveName,fileInfo);
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      errors = StorageSCP_getInfo(storageInfo,archiveName,fileInfo);
      break;
    case STORAGE_TYPE_SFTP:
      errors = StorageSFTP_getInfo(storageInfo,archiveName,fileInfo);
      break;
    case STORAGE_TYPE_WEBDAV:
      errors = StorageWebDAV_getInfo(storageInfo,archiveName,fileInfo);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      errors = StorageOptical_getInfo(storageInfo,archiveName,fileInfo);
      break;
    case STORAGE_TYPE_DEVICE:
      errors = StorageDevice_getInfo(storageInfo,archiveName,fileInfo);
      break;
    case STORAGE_TYPE_MASTER:
error = ERROR_STILL_NOT_IMPLEMENTED;
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
                                 ConstString                pathName,
                                 const JobOptions           *jobOptions,
                                 ServerConnectionPriorities serverConnectionPriority
                                )
{
  String directory;
  Errors error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageSpecifier);

  // initialize variables
  Storage_duplicateSpecifier(&storageDirectoryListHandle->storageSpecifier,storageSpecifier);

  // get directory
  if      (!String_isEmpty(pathName))
  {
    directory = String_duplicate(pathName);
  }
  else
  {
    directory = String_duplicate(storageDirectoryListHandle->storageSpecifier.archiveName);
  }

  // open directory listing
  error = ERROR_UNKNOWN;
  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_NONE:
      error = ERROR_NONE;
      break;
    case STORAGE_TYPE_FILESYSTEM:
      error = StorageFile_openDirectoryList(storageDirectoryListHandle,storageSpecifier,directory,jobOptions,serverConnectionPriority);
      break;
    case STORAGE_TYPE_FTP:
      error = StorageFTP_openDirectoryList(storageDirectoryListHandle,storageSpecifier,directory,jobOptions,serverConnectionPriority);
      break;
    case STORAGE_TYPE_SSH:
      UNUSED_VARIABLE(jobOptions);
      UNUSED_VARIABLE(serverConnectionPriority);

      #ifdef HAVE_SSH2
//TODO
error = ERROR_FUNCTION_NOT_SUPPORTED;
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      error = StorageSCP_openDirectoryList(storageDirectoryListHandle,storageSpecifier,directory,jobOptions,serverConnectionPriority);
      break;
    case STORAGE_TYPE_SFTP:
      error = StorageSFTP_openDirectoryList(storageDirectoryListHandle,storageSpecifier,directory,jobOptions,serverConnectionPriority);
      break;
    case STORAGE_TYPE_WEBDAV:
      error = StorageWebDAV_openDirectoryList(storageDirectoryListHandle,storageSpecifier,directory,jobOptions,serverConnectionPriority);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_openDirectoryList(storageDirectoryListHandle,storageSpecifier,directory,jobOptions,serverConnectionPriority);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_openDirectoryList(storageDirectoryListHandle,storageSpecifier,directory,jobOptions,serverConnectionPriority);
      break;
    case STORAGE_TYPE_MASTER:
//TODO
error = ERROR_STILL_NOT_IMPLEMENTED;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  assert(error != ERROR_UNKNOWN);
  if (error != ERROR_NONE)
  {
    String_delete(directory);
    Storage_doneSpecifier(&storageDirectoryListHandle->storageSpecifier);
    return error;
  }

  // free resources
  String_delete(directory);

  DEBUG_ADD_RESOURCE_TRACE(storageDirectoryListHandle,sizeof(StorageDirectoryListHandle));

  return ERROR_NONE;
}

void Storage_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(storageDirectoryListHandle,sizeof(StorageDirectoryListHandle));

  switch (storageDirectoryListHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      StorageFile_closeDirectoryList(storageDirectoryListHandle);
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
      StorageSFTP_closeDirectoryList(storageDirectoryListHandle);
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
    case STORAGE_TYPE_MASTER:
//TODO
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
  DEBUG_CHECK_RESOURCE_TRACE(storageDirectoryListHandle);

  endOfDirectoryFlag = TRUE;
  switch (storageDirectoryListHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      endOfDirectoryFlag = StorageFile_endOfDirectoryList(storageDirectoryListHandle);
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
      endOfDirectoryFlag = StorageSFTP_endOfDirectoryList(storageDirectoryListHandle);
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
    case STORAGE_TYPE_MASTER:
//      error = ERROR_STILL_NOT_IMPLEMENTED;
endOfDirectoryFlag = TRUE;
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
  DEBUG_CHECK_RESOURCE_TRACE(storageDirectoryListHandle);

  error = ERROR_NONE;
  switch (storageDirectoryListHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      error = StorageFile_readDirectoryList(storageDirectoryListHandle,fileName,fileInfo);
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
      error = StorageSFTP_readDirectoryList(storageDirectoryListHandle,fileName,fileInfo);
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
    case STORAGE_TYPE_MASTER:
error = ERROR_STILL_NOT_IMPLEMENTED;
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

Errors Storage_copy(const StorageSpecifier          *storageSpecifier,
                    const JobOptions                *jobOptions,
                    BandWidthList                   *maxBandWidthList,
                    StorageUpdateStatusInfoFunction storageUpdateStatusInfoFunction,
                    void                            *storageUpdateStatusInfoUserData,
                    StorageRequestVolumeFunction    storageRequestVolumeFunction,
                    void                            *storageRequestVolumeUserData,
                    ConstString                     localFileName
                   )
{
  AutoFreeList  autoFreeList;
  void          *buffer;
  Errors        error;
  StorageInfo   storageInfo;
  StorageHandle storageHandle;
  FileHandle    fileHandle;
  ulong         bytesRead;

  assert(storageSpecifier != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageSpecifier);
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
  error = Storage_init(&storageInfo,
NULL, // masterIO
                       storageSpecifier,
                       jobOptions,
                       maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK(storageUpdateStatusInfoFunction,storageUpdateStatusInfoUserData),
                       CALLBACK(NULL,NULL),
                       CALLBACK(storageRequestVolumeFunction,storageRequestVolumeUserData)
                      );
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  error = Storage_open(&storageHandle,
                       &storageInfo,
                       NULL  // archiveName
                      );
  if (error != ERROR_NONE)
  {
    (void)Storage_done(&storageInfo);
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,&storageInfo,{ Storage_close(&storageHandle); (void)Storage_done(&storageInfo); });

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
  (void)Storage_done(&storageInfo);

  // free resources
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

Errors Storage_forAll(ConstString     storagePatternString,
                      StorageFunction storageFunction,
                      void            *storageUserData
                     )
{
  StorageSpecifier           storageSpecifier;
  JobOptions                 jobOptions;
  StringList                 directoryList;
  String                     archiveName;
  Errors                     error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  FileInfo                   fileInfo;

  assert(storagePatternString != NULL);
  assert(storageFunction != NULL);

  Storage_initSpecifier(&storageSpecifier);
  Job_initOptions(&jobOptions);
  StringList_init(&directoryList);
  archiveName = String_new();

  error = Storage_parseName(&storageSpecifier,storagePatternString);
  if (error == ERROR_NONE)
  {
    // read directory and scan all sub-directories
    StringList_append(&directoryList,storageSpecifier.archiveName);
    while (!StringList_isEmpty(&directoryList))
    {
      StringList_removeLast(&directoryList,archiveName);

      // open directory
      error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                        &storageSpecifier,
                                        archiveName,
                                        &jobOptions,
                                        SERVER_CONNECTION_PRIORITY_LOW
                                       );

      if (error == ERROR_NONE)
      {
        // read directory
        while (   !Storage_endOfDirectoryList(&storageDirectoryListHandle)
               && (error == ERROR_NONE)
              )
        {
          // read next directory entry
          error = Storage_readDirectoryList(&storageDirectoryListHandle,archiveName,&fileInfo);
          if (error != ERROR_NONE)
          {
            continue;
          }

          // check if sub-directory
          if (fileInfo.type == FILE_TYPE_DIRECTORY)
          {
            StringList_append(&directoryList,archiveName);
          }

          // match pattern and call callback
          if (   ((storageSpecifier.archivePatternString == NULL) && String_equals(storageSpecifier.archiveName,archiveName))
              || ((storageSpecifier.archivePatternString != NULL) && Pattern_match(&storageSpecifier.archivePattern,archiveName,PATTERN_MATCH_MODE_EXACT))
             )
          {
            // callback
            error = storageFunction(Storage_getName(NULL,&storageSpecifier,archiveName),&fileInfo,storageUserData);
          }
        }

        // close directory
        Storage_closeDirectoryList(&storageDirectoryListHandle);
      }
    }
  }

  // free resources
  String_delete(archiveName);
  StringList_done(&directoryList);
  Job_doneOptions(&jobOptions);
  Storage_doneSpecifier(&storageSpecifier);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
