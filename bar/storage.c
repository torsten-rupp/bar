/***********************************************************************\
*
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
#include <time.h>
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
#include "common/devices.h"
#include "common/network.h"
#include "common/semaphores.h"
#include "common/passwords.h"
#include "common/patterns.h"
#include "common/misc.h"

// TODO: remove bar.h
#include "bar.h"
#include "bar_common.h"
#include "errors.h"
#include "crypt.h"
#include "archive.h"
#include "jobs.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
/* file data buffer size */
#define BUFFER_SIZE           (64*1024)

// max. number of password input requests
#define MAX_PASSWORD_REQUESTS 3

// max. number of read/write/lstat retries
#define MAX_READ_RETRIES   3
#define MAX_WRITE_RETRIES  3
#define MAX_LSTATE_RETRIES 3
#define MAX_MKDIR_RETRIES  3
#define MAX_UNLINK_RETRIES 3

#define RETRY_DELAY 100

// different timeouts [ms]
#define ALLOCATE_SERVER_TIMEOUT (3*60*MS_PER_SECOND)
#define SSH_TIMEOUT             (30*MS_PER_SECOND)
#define READ_TIMEOUT            (60*MS_PER_SECOND)
#define WRITE_TIMEOUT           (60*MS_PER_SECOND)

#define INITIAL_BUFFER_SIZE   (64*1024)
#define INCREMENT_BUFFER_SIZE ( 8*1024)
#define MAX_BUFFER_SIZE       (64*1024)
#define MAX_FILENAME_LENGTH   ( 8*1024)

// HTTP codes
#define HTTP_CODE_CONTINUE               100
#define HTTP_CODE_OK                     200
#define HTTP_CODE_CREATED                201
#define HTTP_CODE_ACCEPTED               202
#define HTTP_CODE_MULTI_STATUS           207
#define HTTP_CODE_SERVER_READY           220
#define HTTP_CODE_BAD_REQUEST            400
#define HTTP_CODE_UNAUTHORIZED           401
#define HTTP_CODE_FORBITTEN              403
#define HTTP_CODE_NOT_FOUND              404
#define HTTP_CODE_BAD_METHOD             405
#define HTTP_CODE_NOT_ACCEPTABLE         406
#define HTTP_CODE_RESOURCE_TIMEOUT       408
#define HTTP_CODE_RESOURCE_NOT_AVAILABLE 410
#define HTTP_CODE_ENTITY_TOO_LARGE       413
#define HTTP_CODE_EXPECTATIONFAILED      417

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
#if   defined(PLATFORM_LINUX)
LOCAL sighandler_t oldSignalAlarmHandler;
#elif defined(PLATFORM_WINDOWS)
//TODO
#endif /* PLATFORM_... */

#ifdef HAVE_SSH2
  LOCAL Password defaultSSHPassword;
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

#ifdef HAVE_SIGALRM
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
#endif /* HAVE_SIGALRM */

/***********************************************************************\
* Name   : updateStorageRunningInfo
* Purpose: update storage running info
* Input  : storageInfo - storage info
* Output : -
* Return : TRUE to continue, FALSE to abort
* Notes  : -
\***********************************************************************/

LOCAL bool updateStorageRunningInfo(const StorageInfo *storageInfo)
{
  bool result;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  if (storageInfo->updateProgressFunction != NULL)
  {
    result = storageInfo->updateProgressFunction(storageInfo->progress.storageDoneBytes,
                                                 storageInfo->progress.volumeNumber,
                                                 storageInfo->progress.volumeDone,
                                                 storageInfo->progress.message.code,
                                                 storageInfo->progress.message.text,
                                                 storageInfo->updateProgressUserData
                                                );
  }
  else
  {
    result = TRUE;
  }

  return result;
}

/***********************************************************************\
* Name   : resetVolumeDone
* Purpose: reset volume progres done
* Input  : storageInfo - storage info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void resetVolumeDone(StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  storageInfo->opticalDisk.write.step = 0;
  storageInfo->progress.volumeDone    = 0.0;
}

/***********************************************************************\
* Name   : updateStorageRunningInfo
* Purpose: update volume progress
* Input  : storageInfo   - storage info
*          stepIncrement - step increment
*          percentage    - percentage [0..100]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateVolumeDone(StorageInfo *storageInfo, uint stepIncrement, double percentage)
{
  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  storageInfo->opticalDisk.write.step += stepIncrement;
  assert(storageInfo->opticalDisk.write.step <= storageInfo->opticalDisk.write.steps);
  storageInfo->progress.volumeDone = ((double)storageInfo->opticalDisk.write.step*100.0+percentage)/(double)(storageInfo->opticalDisk.write.steps);
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
* Name   : waitCurlSocketRead
* Purpose: wait for Curl socket read
* Input  : curlMultiHandle - Curl multi handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors waitCurlSocketRead(CURLM *curlMultiHandle)
{
  CURLMcode curlmCode;
  long      curlTimeout;
  Errors    error;
  int       fdCount;

  assert(curlMultiHandle != NULL);

  // get a suitable timeout
  curl_multi_timeout(curlMultiHandle,&curlTimeout);

  // wait
  curlmCode = curl_multi_poll(curlMultiHandle,
                              NULL,0,  // extra fds
                              MAX(curlTimeout,READ_TIMEOUT),
                              &fdCount
                             );
  switch (curlmCode)
  {
    case CURLM_OK:
      // OK
      error = ERROR_NONE;
      break;
    default:
      // error
      error = ERROR_NETWORK_RECEIVE;
      break;
  }

  return error;
}

/***********************************************************************\
* Name   : waitCurlSocketWrite
* Purpose: wait for Curl socket write
* Input  : curlMultiHandle - Curl multi handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors waitCurlSocketWrite(CURLM *curlMultiHandle)
{
  CURLMcode curlmCode;
  long      curlTimeout;
  Errors    error;
  int       fdCount;

  assert(curlMultiHandle != NULL);

  // get a suitable timeout
  curl_multi_timeout(curlMultiHandle,&curlTimeout);

  // wait
  fdCount=0;
  curlmCode = curl_multi_poll(curlMultiHandle,
                              NULL,0,  // extra fds
                              MAX(curlTimeout,WRITE_TIMEOUT),
                              &fdCount
                             );
  switch (curlmCode)
  {
    case CURLM_OK:
//fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,fdCount);
      // OK
      error = ERROR_NONE;
      break;
    default:
      // error
      error = ERROR_NETWORK_RECEIVE;
      break;
  }

  return error;
}

/***********************************************************************\
* Name   : getCurlHTTPResponseError
* Purpose: get curl HTTP response error
* Input  : curlHandle  - CURL handle
*          archiveName - archive name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getCurlHTTPResponseError(CURL *curlHandle, ConstString archiveName)
{
  CURLcode curlCode;
  long     responseCode;
  Errors   error;

  assert(curlHandle != NULL);

  curlCode = curl_easy_getinfo(curlHandle,CURLINFO_RESPONSE_CODE,&responseCode);
  if (curlCode == CURLE_OK)
  {
    switch (responseCode)
    {
      case 0:
      case HTTP_CODE_CONTINUE:
      case HTTP_CODE_OK:
      case HTTP_CODE_CREATED:
      case HTTP_CODE_ACCEPTED:
      case HTTP_CODE_MULTI_STATUS:
      case HTTP_CODE_SERVER_READY:
        error = ERROR_NONE;
        break;
      case HTTP_CODE_BAD_REQUEST:
      case HTTP_CODE_BAD_METHOD:
        error = ERRORX_(BAD_REQUEST,responseCode,"bad request");
        break;
      case HTTP_CODE_UNAUTHORIZED:
      case HTTP_CODE_FORBITTEN:
        error = ERRORX_(FILE_ACCESS_DENIED,curlCode,"%s",String_cString(archiveName));
        break;
      case HTTP_CODE_NOT_FOUND:
        error = ERROR_FILE_NOT_FOUND_;
        break;
      default:
        error = ERRORX_(INVALID_RESPONSE,responseCode,"unhandled HTTP response");
        break;
    }
  }
  else
  {
    error = ERRORX_(INVALID_RESPONSE,curlCode,"%s",curl_easy_strerror(curlCode));
  }

  return error;
}

#endif /* HAVE_CURL */

#if defined(HAVE_CURL) || defined(HAVE_FTP) || defined(HAVE_SSH2) || defined(HAVE_SMB2)
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
  uint  i;

  assert(storageBandWidthLimiter != NULL);

  maxBandWidth = (maxBandWidthList != NULL) ? getBandWidth(maxBandWidthList) : 0L;

  storageBandWidthLimiter->maxBandWidthList     = maxBandWidthList;
  storageBandWidthLimiter->maxBlockSize         = 64*1024;
  storageBandWidthLimiter->blockSize            = 64*1024;
  for (i = 0; i < SIZE_OF_ARRAY(storageBandWidthLimiter->measurements); i++)
  {
    storageBandWidthLimiter->measurements[i] = maxBandWidth;
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
  uint   i;
  ulong  averageBandWidth;   // average band width [bits/s]
  ulong  maxBandWidth;
  uint64 calculatedTime;
  uint64 delayTime;          // delay time [us]

  assert(storageBandWidthLimiter != NULL);

  if (storageBandWidthLimiter->maxBandWidthList != NULL)
  {
    storageBandWidthLimiter->measurementBytes += transmittedBytes;
    storageBandWidthLimiter->measurementTime  += transmissionTime;
//fprintf(stderr,"%s, %d: sum %lu bytes %"PRIu64" us\n",__FILE__,__LINE__,storageBandWidthLimiter->measurementBytes,storageBandWidthLimiter->measurementTime);

    // too small sizes/time values are not reliable, thus accumulate
    if (   (storageBandWidthLimiter->measurementBytes > 1*MB           )
        || (storageBandWidthLimiter->measurementTime  > MS_TO_US(100LL))
       )
    {
      // calculate average band width
      averageBandWidth = 0;
      if (storageBandWidthLimiter->measurementCount > 0)
      {
        for (i = 0; i < storageBandWidthLimiter->measurementCount; i++)
        {
          averageBandWidth += storageBandWidthLimiter->measurements[i];
        }
        averageBandWidth /= storageBandWidthLimiter->measurementCount;
      }
//fprintf(stderr,"%s, %d: averageBandWidth=%lu bits/s\n",__FILE__,__LINE__,averageBandWidth);

      // get max. band width to use [bit/s]
      maxBandWidth = getBandWidth(storageBandWidthLimiter->maxBandWidthList);

      // calculate delay time
      if (maxBandWidth > 0L)
      {
        calculatedTime = (BYTES_TO_BITS(storageBandWidthLimiter->measurementBytes)*US_PER_SECOND)/maxBandWidth;  // [us]
        delayTime      = (calculatedTime > storageBandWidthLimiter->measurementTime)
                           ? calculatedTime-storageBandWidthLimiter->measurementTime
                           : 0LL;
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
//fprintf(stderr,"%s,%d: storageBandWidthLimiter->measurementBytes=%ld storageBandWidthLimiter->max=%ld calculated time=%"PRIu64" us storageBandWidthLimiter->measurementTime=%lu us blockSize=%ld\n",__FILE__,__LINE__,storageBandWidthLimiter->measurementBytes,storageBandWidthLimiter->max,calculatedTime,storageBandWidthLimiter->measurementTime,storageBandWidthLimiter->blockSize);
        }
        else if (averageBandWidth < (maxBandWidth-BYTES_TO_BITS(1024)))
        {
          // increase max. block size to send a little more in a single step (if possible)
          if (storageBandWidthLimiter->blockSize < (storageBandWidthLimiter->maxBlockSize-4*1024)) storageBandWidthLimiter->blockSize += 4*1024;
//fprintf(stderr,"%s,%d: ++ averageBandWidth=%lu bit/s storageBandWidthLimiter->max=%lu bit/s storageBandWidthLimiter->measurementTime=%"PRIu64" us storageBandWidthLimiter->blockSize=%"PRIu64"\n",__FILE__,__LINE__,averageBandWidth,storageBandWidthLimiter->max,storageBandWidthLimiter->measurementTime,storageBandWidthLimiter->blockSize);
        }
      }
#endif /* 0 */

      // delay if needed
      if (delayTime > 0)
      {
        Misc_udelay(delayTime);
      }

      // calculate and store new current bandwidth
      assert((storageBandWidthLimiter->measurementTime+delayTime) != 0LL);
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
*          userName                - user name
*          password                - password
*          jobOptions              - job options
*          getNamePasswordFunction - get password call-back (can be
*                                    NULL)
*          getNamePasswordUserData - user data for get password call-back
* Output : userName - user name
*          password - password
* Return : TRUE if SSH password intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool initSSHLogin(ConstString             hostName,
                        String                  userName,
                        Password                *password,
                        const JobOptions        *jobOptions,
                        GetNamePasswordFunction getNamePasswordFunction,
                        void                    *getNamePasswordUserData
                       )
{
  String s;
  bool   initFlag;

  assert(!String_isEmpty(hostName));
  assert(userName != NULL);
  assert(password != NULL);

  initFlag = FALSE;

  if (jobOptions != NULL)
  {
    SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      if (Password_isEmpty(&jobOptions->sshServer.password))
      {
        switch (globalOptions.runMode)
        {
          case RUN_MODE_INTERACTIVE:
            if (Password_isEmpty(&defaultSSHPassword))
            {
              s = !String_isEmpty(userName)
                    ? String_format(String_new(),"SSH login password for %S@%S",userName,hostName)
                    : String_format(String_new(),"SSH login password for %S",hostName);
              if (Password_input(password,String_cString(s),PASSWORD_INPUT_MODE_ANY))
              {
                initFlag = TRUE;
              }
              String_delete(s);
            }
            else
            {
              Password_set(password,&defaultSSHPassword);
              initFlag = TRUE;
            }
            break;
          case RUN_MODE_BATCH:
          case RUN_MODE_SERVER:
            if (getNamePasswordFunction != NULL)
            {
              s = !String_isEmpty(userName)
                    ? String_format(String_new(),"%S@%S",userName,hostName)
                    : String_format(String_new(),"%S",hostName);
              if (getNamePasswordFunction(userName,
                                          password,
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
*          userName         - user name
*          password         - password
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
                           ConstString userName,
                           Password    *password,
                           const void  *publicKeyData,
                           uint        publicKeyLength,
                           const void  *privateKeyData,
                           uint        privateKeyLength
                          )
{
  SocketHandle socketHandle;
  Errors       error;

  assert(userName != NULL);

  printInfo(5,"SSH: host %s:%d\n",String_cString(hostName),hostPort);
  error = Network_connect(&socketHandle,
                          SOCKET_TYPE_SSH,
                          hostName,
                          hostPort,
                          userName,
                          password,
                          NULL,  // caData
                          0,     // caLength
                          NULL,  // certData
                          0,     // certLength
                          publicKeyData,
                          publicKeyLength,
                          privateKeyData,
                          privateKeyLength,
                            SOCKET_FLAG_NONE
                          | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                          | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                          30*MS_PER_SECOND
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
  SignalMask signalMask;

  assert(socketHandle != NULL);

  // Note: ignore SIGALRM in Misc_waitHandle()
  MISC_SIGNAL_MASK_CLEAR(signalMask);
  #ifdef HAVE_SIGALRM
    MISC_SIGNAL_MASK_SET(signalMask,SIGALRM);
  #endif /* HAVE_SIGALRM */

  return (Misc_waitHandle(socketHandle->handle,
                          &signalMask,
                          HANDLE_EVENT_ANY,
                          60*MS_PER_SECOND
                         ) != 0);
}
#endif /* HAVE_SSH2 */

/***********************************************************************\
* Name   : transferFileToStorage
* Purpose: transfer file data from temporary file to storage
* Input  : fromFileHandle              - file handle of temporary file
*          storageHandle               - storage handle
*          storageTransferInfoFunction - update transfer info function
*                                        (can be NULL)
*          storageTransferInfoUserData - user data for update transfer
*                                        info function
*          isAbortedFunction           - is abort check callback (can
*                                        be NULL)
*          isAbortedUserData           - user data for is aborted
*                                        check
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors transferFileToStorage(FileHandle                  *fileHandle,
                                   StorageHandle               *storageHandle,
                                   StorageTransferInfoFunction storageTransferInfoFunction,
                                   void                        *storageTransferInfoUserData,
                                   IsAbortedFunction           isAbortedFunction,
                                   void                        *isAbortedUserData
                                  )
{
  #define TRANSFER_BUFFER_SIZE (1024*1024)

  void    *buffer;
  Errors  error;
  uint64  size;
  uint64  transferedBytes;
  ulong   n;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(fileHandle != NULL);

  // init variables
  buffer = malloc(TRANSFER_BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // seek to begin of file
  error = File_seek(fileHandle,0LL);
  if (error != ERROR_NONE)
  {
    free(buffer);
    return error;
  }

  // get total size
  size = File_getSize(fileHandle);

  // transfer data
  transferedBytes = 0L;
  while (   (transferedBytes < size)
         && (   (isAbortedFunction == NULL)
             || !isAbortedFunction(isAbortedUserData)
            )
// TODO: remove
         && !Storage_isAborted(storageHandle->storageInfo)
        )
  {
    // get block size
    n = (ulong)MIN(size-transferedBytes,TRANSFER_BUFFER_SIZE);

    // read data
    error = File_read(fileHandle,buffer,n,NULL);
    if (error != ERROR_NONE)
    {
      free(buffer);
      return error;
    }

    // transfer to storage
    error = Storage_write(storageHandle,buffer,n);
    if (error != ERROR_NONE)
    {
      free(buffer);
      return error;
    }

    // next part
    transferedBytes += (uint64)n;

    // update running info
    if (storageTransferInfoFunction != NULL)
    {
      storageTransferInfoFunction(transferedBytes,
                                  size,
                                  storageTransferInfoUserData
                                 );
    }
// TODO: remove
// TODO: replace by storageTransferInfoFunction
    storageHandle->storageInfo->progress.storageDoneBytes += (uint64)n;
    if (!updateStorageRunningInfo(storageHandle->storageInfo))
    {
      free(buffer);
      return ERROR_ABORTED;
    }

    // pause storage (if requested)
    Storage_pause(storageHandle->storageInfo);
  }

  // free resources
  free(buffer);

  return ERROR_NONE;

  #undef TRANSFER_BUFFER_SIZE
}

/***********************************************************************\
* Name   : transferStorageToStorage
* Purpose: transfer data from storage to storage
* Input  : fromStorageHandle           - from storage handle
*          toStorageHandle             - to storage handle
*          storageTransferInfoFunction - update transfer info function
*                                        (can be NULL)
*          storageTransferInfoUserData - user data for update transfer
*                                        info function
*          isAbortedFunction           - is abort check callback (can
*                                        be NULL)
*          isAbortedUserData           - user data for is aborted
*                                        check
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors transferStorageToStorage(StorageHandle               *fromStorageHandle,
                                      StorageHandle               *toStorageHandle,
                                      StorageTransferInfoFunction storageTransferInfoFunction,
                                      void                        *storageTransferInfoUserData,
                                      IsAbortedFunction           isAbortedFunction,
                                      void                        *isAbortedUserData
                                    )
{
  #define TRANSFER_BUFFER_SIZE (1024*1024)

  void    *buffer;
  Errors  error;
  uint64  size;
  uint64  transferedBytes;
  ulong   n;

  assert(fromStorageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(fromStorageHandle);
  assert(toStorageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(toStorageHandle);

  // init variables
  buffer = malloc(TRANSFER_BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // get total size
  size = Storage_getSize(fromStorageHandle);

  // transfer data
  error           = ERROR_NONE;
  transferedBytes = 0L;
  while (   (transferedBytes < size)
         && (error == ERROR_NONE)
         && (   (isAbortedFunction == NULL)
             || !isAbortedFunction(isAbortedUserData)
            )
        )
  {
    // get data size
    n = (ulong)MIN(size-transferedBytes,TRANSFER_BUFFER_SIZE);

    // read data
    error = Storage_read(fromStorageHandle,buffer,n,NULL);
    if (error != ERROR_NONE)
    {
      break;
    }

    // transfer to storage
    error = Storage_write(toStorageHandle,buffer,n);
    if (error != ERROR_NONE)
    {
      break;
    }

    // next part
    transferedBytes += (uint64)n;

    // update running info
    if (storageTransferInfoFunction != NULL)
    {
      storageTransferInfoFunction(transferedBytes,size,storageTransferInfoUserData);
    }
// TODO: replace by storageTransferInfoFunction
    toStorageHandle->storageInfo->progress.storageDoneBytes += (uint64)n;
    if (!updateStorageRunningInfo(toStorageHandle->storageInfo))
    {
      break;
    }

    // check abort
// TODO: replace by isAbortedFunction
    if (isAbortedFunction != NULL)
    {
      if (isAbortedFunction(isAbortedUserData))
      {
        error = ERROR_ABORTED;
        break;
      }
    }
    if (Storage_isAborted(fromStorageHandle->storageInfo))
    {
      error = ERROR_ABORTED;
      break;
    }
    if (Storage_isAborted(toStorageHandle->storageInfo))
    {
      error = ERROR_ABORTED;
      break;
    }

    // pause storage (if requested)
    Storage_pause(toStorageHandle->storageInfo);
  }

  // free resources
  free(buffer);

  return error;

  #undef TRANSFER_BUFFER_SIZE
}

#ifndef NDEBUG
/***********************************************************************\
* Name   : debugGetEmulateBlockDevice
* Purpose: get emulated block device file name
* Input  : -
* Output : -
* Return : emulated block device file name or NULL
* Notes  : -
\***********************************************************************/

LOCAL_INLINE char *debugGetEmulateBlockDevice(void)
{
  return getenv(DEVICE_DEBUG_EMULATE_BLOCK_DEVICE);
}
#endif // NDEBUG

// ----------------------------------------------------------------------

#include "storage_file.c"
#include "storage_ftp.c"
#include "storage_scp.c"
#include "storage_sftp.c"
#include "storage_webdav.c"
#include "storage_smb.c"
#include "storage_optical.c"
#include "storage_device.c"
#include "storage_master.c"

/*---------------------------------------------------------------------*/

Errors Storage_initAll(void)
{
  Errors error;

  error = ERROR_NONE;

  #ifdef HAVE_SIGALRM
    oldSignalAlarmHandler = signal(SIGALRM,signalHandler);
  #endif /* HAVE_SIGALRM */
  #if defined(HAVE_SSH2)
    Password_init(&defaultSSHPassword);
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

  struct
  {
    Errors(*initAll)(void);
    void(*doneAll)(void);
  }
  INIT_DONE[] =
  {
    { StorageFile_initAll,    StorageFile_doneAll    },
    { StorageFTP_initAll,     StorageFTP_doneAll     },
    { StorageSCP_initAll,     StorageSCP_doneAll     },
    { StorageSFTP_initAll,    StorageSFTP_doneAll    },
    { StorageWebDAV_initAll,  StorageWebDAV_doneAll  },
    { StorageSMB_initAll,     StorageSMB_doneAll     },
    { StorageOptical_initAll, StorageOptical_doneAll },
    { StorageDevice_initAll,  StorageDevice_doneAll  },
    { StorageMaster_initAll,  StorageMaster_doneAll  }
  };
  size_t i = 0;
  while ((i < SIZE_OF_ARRAY(INIT_DONE)) && (error == ERROR_NONE))
  {
    error = INIT_DONE[i].initAll();
    i++;
  }
  if (error != ERROR_NONE)
  {
    while (i > 0)
    {
      i--;
      INIT_DONE[i].doneAll();
    }
  }

  return error;
}

void Storage_doneAll(void)
{
  StorageMaster_doneAll();
  StorageDevice_doneAll();
  StorageOptical_doneAll();
  StorageSMB_doneAll();
  StorageWebDAV_doneAll();
  StorageSFTP_doneAll();
  StorageSCP_doneAll();
  StorageFTP_doneAll();
  #if   defined(HAVE_CURL)
    curl_global_cleanup();
  #endif /* HAVE_CURL */
  StorageFile_doneAll();

  #if defined(HAVE_SSH2)
    Password_done(&defaultSSHPassword);
  #endif /* HAVE_SSH2 */
  #ifdef HAVE_SIGALRM
    if (oldSignalAlarmHandler != SIG_ERR)
    {
      (void)signal(SIGALRM,oldSignalAlarmHandler);
    }
  #endif /* HAVE_SIGALRM */
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

  storageSpecifier->type                 = STORAGE_TYPE_NONE;
  storageSpecifier->hostName             = String_new();
  storageSpecifier->hostPort             = 0;
  storageSpecifier->userName             = String_new();
  Password_init(&storageSpecifier->password);
  storageSpecifier->shareName            = String_new();
  storageSpecifier->deviceName           = String_new();
  storageSpecifier->archiveName          = String_new();
  storageSpecifier->archivePatternString = NULL;
  storageSpecifier->storageName          = String_new();
  storageSpecifier->printableStorageName = String_new();

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(storageSpecifier,StorageSpecifier);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,storageSpecifier,StorageSpecifier);
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
  destinationStorageSpecifier->userName             = String_duplicate(sourceStorageSpecifier->userName);
  Password_initDuplicate(&destinationStorageSpecifier->password,&sourceStorageSpecifier->password);
  destinationStorageSpecifier->shareName            = String_duplicate(sourceStorageSpecifier->shareName);
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
    DEBUG_ADD_RESOURCE_TRACE(destinationStorageSpecifier,StorageSpecifier);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,destinationStorageSpecifier,StorageSpecifier);
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
    DEBUG_REMOVE_RESOURCE_TRACE(storageSpecifier,StorageSpecifier);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,storageSpecifier,StorageSpecifier);
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
  String_delete(storageSpecifier->shareName);
  Password_done(&storageSpecifier->password);
  String_delete(storageSpecifier->userName);
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

  if (archiveName1 == NULL) archiveName1 = storageSpecifier1->archiveName;
  if (archiveName2 == NULL) archiveName2 = storageSpecifier2->archiveName;

//fprintf(stderr,"%s:%d: %d %d\n",__FILE__,__LINE__,storageSpecifier1->type,storageSpecifier2->type);
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
      case STORAGE_TYPE_WEBDAVS:
        result = StorageWebDAV_equalSpecifiers(storageSpecifier1,archiveName1,storageSpecifier2,archiveName2);
        break;
      case STORAGE_TYPE_SMB:
        result = StorageSMB_equalSpecifiers(storageSpecifier1,archiveName1,storageSpecifier2,archiveName2);
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
                               String      userName,
                               Password    *password
                              )
{
  assert(ftpSpecifier != NULL);
  assert(hostName != NULL);
  assert(userName != NULL);

  return StorageFTP_parseSpecifier(ftpSpecifier,hostName,hostPort,userName,password);
}

bool Storage_parseSSHSpecifier(ConstString sshSpecifier,
                               String      hostName,
                               uint        *hostPort,
                               String      userName
                              )
{
  const char* LOGINNAME_MAP_FROM[] = {"\\@"};
  const char* LOGINNAME_MAP_TO[]   = {"@"};

  bool   result;
  String s;

  assert(sshSpecifier != NULL);
  assert(hostName != NULL);
  assert(userName != NULL);

  String_clear(hostName);
  if (hostPort != NULL) (*hostPort) = 0;
  String_clear(userName);

  s = String_new();
  if      (String_matchCString(sshSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^:]+?):(\\d*)/{0,1}$",NULL,STRING_NO_ASSIGN,userName,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <login name>@<host name>:<host port>
    if (userName != NULL) String_mapCString(userName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (hostPort != NULL)
    {
      if (!String_isEmpty(s)) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);
    }

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^/]+)/{0,1}$",NULL,STRING_NO_ASSIGN,userName,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>@<host name>
    if (userName != NULL) String_mapCString(userName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);

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
                               String      userName,
                               Password    *password
                              )
{
  assert(scpSpecifier != NULL);
  assert(hostName != NULL);
  assert(userName != NULL);

  return StorageSCP_parseSpecifier(scpSpecifier,hostName,hostPort,userName,password);
}

bool Storage_parseSFTPSpecifier(ConstString sftpSpecifier,
                                String      hostName,
                                uint        *hostPort,
                                String      userName,
                                Password    *password
                               )
{
  assert(sftpSpecifier != NULL);
  assert(hostName != NULL);
  assert(userName != NULL);

  return StorageSFTP_parseSpecifier(sftpSpecifier,hostName,hostPort,userName,password);
}

bool Storage_parseWebDAVSpecifier(ConstString webdavSpecifier,
                                  String      hostName,
                                  uint        *hostPort,
                                  String      userName,
                                  Password    *password
                                 )
{
  assert(webdavSpecifier != NULL);
  assert(hostName != NULL);
  assert(userName != NULL);

  return StorageWebDAV_parseSpecifier(webdavSpecifier,hostName,hostPort,userName,password);
}

bool Storage_parseSMBSpecifier(ConstString smbSpecifier,
                               String      hostName,
                               String      userName,
                               Password    *password,
                               String      shareName
                              )
{
  assert(smbSpecifier != NULL);
  assert(hostName != NULL);
  assert(userName != NULL);
  assert(shareName != NULL);

  return StorageSMB_parseSpecifier(smbSpecifier,hostName,userName,password,shareName);
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

StorageTypes Storage_parseType(ConstString storageName)
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

  // initialize variables
  AutoFree_init(&autoFreeList);
  string      = String_new();
  archiveName = String_new();
  AUTOFREE_ADD(&autoFreeList,&string,{ String_delete(string); });
  AUTOFREE_ADD(&autoFreeList,&archiveName,{ String_delete(archiveName); });

  String_clear(storageSpecifier->hostName);
  storageSpecifier->hostPort = 0;
  String_clear(storageSpecifier->userName);
  Password_clear(&storageSpecifier->password);
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
                                     storageSpecifier->userName,
                                     &storageSpecifier->password
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
                                     storageSpecifier->userName
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
      if (!Storage_parseSCPSpecifier(string,
                                     storageSpecifier->hostName,
                                     &storageSpecifier->hostPort,
                                     storageSpecifier->userName,
                                     &storageSpecifier->password
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
                                      storageSpecifier->userName,
                                      &storageSpecifier->password
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
                                        &storageSpecifier->hostPort,
                                        storageSpecifier->userName,
                                        &storageSpecifier->password
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
  else if (String_startsWithCString(storageName,"webdavs://"))
  {
    if (   String_matchCString(storageName,10,"^[^:]+:([^@]|\\@)+?@[^/]+/{0,1}",&nextIndex,NULL,NULL)  // webdav://<login name>:<login password>@<host name>/<file name>
        || String_matchCString(storageName,10,"^([^@]|\\@)+?@[^/]+/{0,1}",&nextIndex,NULL,NULL)        // webdav://<login name>@<host name>/<file name>
        || String_matchCString(storageName,10,"^[^/]+/{0,1}",&nextIndex,NULL,NULL)                     // webdav://<host name>/<file name>
       )
    {
      String_sub(string,storageName,10,nextIndex-10);
      String_trimEnd(string,"/");
      if (!Storage_parseWebDAVSpecifier(string,
                                        storageSpecifier->hostName,
                                        &storageSpecifier->hostPort,
                                        storageSpecifier->userName,
                                        &storageSpecifier->password
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
      String_sub(archiveName,storageName,10,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_WEBDAVS;
  }
  else if (String_startsWithCString(storageName,"smb://"))
  {
    if (   String_matchCString(storageName,6,"^[^:]+:([^@]|\\@)+?@[^/]+/{0,1}",&nextIndex,NULL,NULL)  // smb://<login name>:<login password>@<host name>/<file name>
        || String_matchCString(storageName,6,"^([^@]|\\@)+?@[^/]+/{0,1}",&nextIndex,NULL,NULL)        // smb://<login name>@<host name>/<file name>
        || String_matchCString(storageName,6,"^[^/]+/{0,1}",&nextIndex,NULL,NULL)                     // smb://<host name>/<file name>
       )
    {
      String_sub(string,storageName,6,nextIndex-6);
      String_trimEnd(string,"/");
      if (!Storage_parseSMBSpecifier(string,
                                     storageSpecifier->hostName,
                                     storageSpecifier->userName,
                                     &storageSpecifier->password,
                                     storageSpecifier->shareName
                                    )
         )
      {
        AutoFree_cleanup(&autoFreeList);
        return ERROR_INVALID_SMB_SPECIFIER;
      }
      String_sub(archiveName,storageName,nextIndex,STRING_END);
    }
    else
    {
      // smb://<file name>
      String_sub(archiveName,storageName,6,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_SMB;
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
          File_setFileNameChar(storageSpecifier->archiveName,FILE_PATH_SEPARATOR_CHAR);
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
    String  archivePatternString;
    Pattern archivePattern;

    // get file pattern string
    archivePatternString = String_new();
    File_initSplitFileName(&archiveNameTokenizer,archiveName);
    {
      if (File_getNextSplitFileName(&archiveNameTokenizer,&token))
      {
        if (!String_isEmpty(token))
        {
          File_setFileName(archivePatternString,token);
        }
        else
        {
          File_setFileNameChar(archivePatternString,FILE_PATH_SEPARATOR_CHAR);
        }
        while (File_getNextSplitFileName(&archiveNameTokenizer,&token))
        {
          File_appendFileName(archivePatternString,token);
        }
      }
    }
    File_doneSplitFileName(&archiveNameTokenizer);

    // parse file pattern
    error = Pattern_init(&archivePattern,
                         archivePatternString,
                         globalOptions.patternType,
                         PATTERN_FLAG_NONE
                        );
    if (error != ERROR_NONE)
    {
      String_delete(archivePatternString);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // save
    if (storageSpecifier->archivePatternString != NULL)
    {
      Pattern_done(&storageSpecifier->archivePattern);
      String_delete(storageSpecifier->archivePatternString);
    }
    storageSpecifier->archivePatternString = archivePatternString;
    Pattern_move(&storageSpecifier->archivePattern,&archivePattern);
  }
  else
  {
    if (storageSpecifier->archivePatternString != NULL)
    {
      Pattern_done(&storageSpecifier->archivePattern);
      String_delete(storageSpecifier->archivePatternString);
    }
    storageSpecifier->archivePatternString = NULL;
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

String Storage_getName(String                 string,
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
    case STORAGE_TYPE_WEBDAVS:
      StorageWebDAV_getName(string,storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_SMB:
      StorageSMB_getName(string,storageSpecifier,archiveName);
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
    case STORAGE_TYPE_WEBDAVS:
      StorageWebDAV_getPrintableName(string,storageSpecifier,archiveName);
      break;
    case STORAGE_TYPE_SMB:
      StorageSMB_getPrintableName(string,storageSpecifier,archiveName);
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

uint Storage_getServerSettings(Server                 *server,
                               const StorageSpecifier *storageSpecifier,
                               const JobOptions       *jobOptions
                              )
{
  uint             serverId;
  const ServerNode *existingServerNode;

  assert(server != NULL);
  assert(storageSpecifier != NULL);

  // get default settings
  serverId                        = 0;
  server->type                    = SERVER_TYPE_NONE;
  server->name                    = NULL;
  server->maxConnectionCount      = 0;
  server->maxStorageSize          = (jobOptions != NULL) ? jobOptions->maxStorageSize : 0LL;
  server->writePreProcessCommand  = NULL;
  server->writePostProcessCommand = NULL;

  // get server specific settings
  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
      {
        // find server
        existingServerNode = LIST_FIND(&globalOptions.serverList,
                                       existingServerNode,
                                          (existingServerNode->server.type == SERVER_TYPE_FILE)
                                       && String_startsWith(existingServerNode->server.name,storageSpecifier->archiveName)
                                      );
        if (existingServerNode != NULL)
        {
          // get server settings
          serverId = existingServerNode->server.id;
          Configuration_initServer(server,existingServerNode->server.name,SERVER_TYPE_FILE);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(existingServerNode->server.writePreProcessCommand )
                                                               ? existingServerNode->server.writePreProcessCommand
                                                               : globalOptions.file.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(existingServerNode->server.writePostProcessCommand)
                                                               ? existingServerNode->server.writePostProcessCommand
                                                               : globalOptions.file.writePostProcessCommand
                                                            );
        }
      }
      break;
    case STORAGE_TYPE_FTP:
      SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
      {
        // find server
        existingServerNode = LIST_FIND(&globalOptions.serverList,
                               existingServerNode,
                                  (existingServerNode->server.type == SERVER_TYPE_FTP)
                               && String_equals(existingServerNode->server.name,storageSpecifier->hostName)
                              );
        if (existingServerNode != NULL)
        {
          // get server settings
          serverId  = existingServerNode->server.id;
          Configuration_initServer(server,existingServerNode->server.name,SERVER_TYPE_FTP);
          server->ftp.userName = String_duplicate(existingServerNode->server.ftp.userName);
          Password_set(&server->ftp.password,&existingServerNode->server.ftp.password);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(existingServerNode->server.writePreProcessCommand )
                                                               ? existingServerNode->server.writePreProcessCommand
                                                               : globalOptions.ftp.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(existingServerNode->server.writePostProcessCommand)
                                                               ? existingServerNode->server.writePostProcessCommand
                                                               : globalOptions.ftp.writePostProcessCommand
                                                            );
        }
      }
      break;
    case STORAGE_TYPE_SSH:
    case STORAGE_TYPE_SCP:
      SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
      {
        // find server
        existingServerNode = LIST_FIND(&globalOptions.serverList,
                               existingServerNode,
                                  (existingServerNode->server.type == SERVER_TYPE_SSH)
                               && String_equals(existingServerNode->server.name,storageSpecifier->hostName)
                              );
        if (existingServerNode != NULL)
        {
          // get server settings
          serverId  = existingServerNode->server.id;
          Configuration_initServer(server,existingServerNode->server.name,SERVER_TYPE_SSH);
          server->ssh.port      = existingServerNode->server.ssh.port;
          server->ssh.userName = String_duplicate(existingServerNode->server.ssh.userName);
          Password_set(&server->ssh.password,&existingServerNode->server.ssh.password);
          Configuration_duplicateKey(&server->ssh.publicKey,&existingServerNode->server.ssh.publicKey);
          Configuration_duplicateKey(&server->ssh.privateKey,&existingServerNode->server.ssh.privateKey);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(existingServerNode->server.writePreProcessCommand )
                                                               ? existingServerNode->server.writePreProcessCommand
                                                               : globalOptions.scp.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(existingServerNode->server.writePostProcessCommand)
                                                              ? existingServerNode->server.writePostProcessCommand
                                                              : globalOptions.scp.writePostProcessCommand
                                                            );
        }
      }
      break;
    case STORAGE_TYPE_SFTP:
      SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
      {
        // find server
        existingServerNode = LIST_FIND(&globalOptions.serverList,
                               existingServerNode,
                                  (existingServerNode->server.type == SERVER_TYPE_SSH)
                               && String_equals(existingServerNode->server.name,storageSpecifier->hostName)
                              );
        if (existingServerNode != NULL)
        {
          // get server settings
          serverId  = existingServerNode->server.id;
          Configuration_initServer(server,existingServerNode->server.name,SERVER_TYPE_SSH);
          server->ssh.port      = existingServerNode->server.ssh.port;
          server->ssh.userName = String_duplicate(existingServerNode->server.ssh.userName);
          Password_set(&server->ssh.password,&existingServerNode->server.ssh.password);
          Configuration_duplicateKey(&server->ssh.publicKey,&existingServerNode->server.ssh.publicKey);
          Configuration_duplicateKey(&server->ssh.privateKey,&existingServerNode->server.ssh.privateKey);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(existingServerNode->server.writePreProcessCommand )
                                                               ? existingServerNode->server.writePreProcessCommand
                                                               : globalOptions.sftp.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(existingServerNode->server.writePostProcessCommand)
                                                               ? existingServerNode->server.writePostProcessCommand
                                                               : globalOptions.sftp.writePostProcessCommand
                                                            );
        }
      }
      break;
    case STORAGE_TYPE_WEBDAV:
      SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
      {
        // find server
        existingServerNode = LIST_FIND(&globalOptions.serverList,
                                       existingServerNode,
                                          (existingServerNode->server.type == SERVER_TYPE_WEBDAV)
                                       && String_equals(existingServerNode->server.name,storageSpecifier->hostName)
                                      );
        if (existingServerNode != NULL)
        {
          // get server settings
          serverId = existingServerNode->server.id;
          Configuration_initServer(server,existingServerNode->server.name,SERVER_TYPE_WEBDAV);
          server->webDAV.port      = existingServerNode->server.webDAV.port;
          server->webDAV.userName = String_duplicate(existingServerNode->server.webDAV.userName);
          Password_set(&server->webDAV.password,&existingServerNode->server.webDAV.password);
          Configuration_duplicateKey(&server->webDAV.publicKey,&existingServerNode->server.webDAV.publicKey);
          Configuration_duplicateKey(&server->webDAV.privateKey,&existingServerNode->server.webDAV.privateKey);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(existingServerNode->server.writePreProcessCommand )
                                                               ? existingServerNode->server.writePreProcessCommand
                                                               : globalOptions.webdav.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(existingServerNode->server.writePostProcessCommand)
                                                               ? existingServerNode->server.writePostProcessCommand
                                                               : globalOptions.webdav.writePostProcessCommand
                                                            );
        }
      }
      break;
    case STORAGE_TYPE_WEBDAVS:
      SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
      {
        // find server
        existingServerNode = LIST_FIND(&globalOptions.serverList,
                                       existingServerNode,
                                          (existingServerNode->server.type == SERVER_TYPE_WEBDAVS)
                                       && String_equals(existingServerNode->server.name,storageSpecifier->hostName)
                                      );
        if (existingServerNode != NULL)
        {
          // get server settings
          serverId = existingServerNode->server.id;
          Configuration_initServer(server,existingServerNode->server.name,SERVER_TYPE_WEBDAVS);
          server->webDAV.port      = existingServerNode->server.webDAV.port;
          server->webDAV.userName = String_duplicate(existingServerNode->server.webDAV.userName);
          Password_set(&server->webDAV.password,&existingServerNode->server.webDAV.password);
          Configuration_duplicateKey(&server->webDAV.publicKey,&existingServerNode->server.webDAV.publicKey);
          Configuration_duplicateKey(&server->webDAV.privateKey,&existingServerNode->server.webDAV.privateKey);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(existingServerNode->server.writePreProcessCommand )
                                                               ? existingServerNode->server.writePreProcessCommand
                                                               : globalOptions.webdav.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(existingServerNode->server.writePostProcessCommand)
                                                               ? existingServerNode->server.writePostProcessCommand
                                                               : globalOptions.webdav.writePostProcessCommand
                                                            );
        }
      }
      break;
    case STORAGE_TYPE_SMB:
      SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
      {
        // find server
        existingServerNode = LIST_FIND(&globalOptions.serverList,
                                       existingServerNode,
                                          (existingServerNode->server.type == SERVER_TYPE_SMB)
                                       && String_equals(existingServerNode->server.name,storageSpecifier->hostName)
                                      );
        if (existingServerNode != NULL)
        {
          // get server settings
          serverId = existingServerNode->server.id;
          Configuration_initServer(server,existingServerNode->server.name,SERVER_TYPE_SMB);
          server->smb.userName = String_duplicate(existingServerNode->server.smb.userName);
          Password_set(&server->smb.password,&existingServerNode->server.smb.password);
          server->smb.shareName = String_duplicate(existingServerNode->server.smb.shareName);
          server->writePreProcessCommand  = String_duplicate(!String_isEmpty(existingServerNode->server.writePreProcessCommand )
                                                               ? existingServerNode->server.writePreProcessCommand
                                                               : globalOptions.smb.writePreProcessCommand
                                                            );
          server->writePostProcessCommand = String_duplicate(!String_isEmpty(existingServerNode->server.writePostProcessCommand)
                                                               ? existingServerNode->server.writePostProcessCommand
                                                               : globalOptions.smb.writePostProcessCommand
                                                            );
        }
      }
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
    case STORAGE_TYPE_DEVICE:
      // nothing to do
      break;
    case STORAGE_TYPE_ANY:
      // nothing to do
      break;
    case STORAGE_TYPE_UNKNOWN:
      // nothing to do
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; // not reached
    #endif /* NDEBUG */
  }

  return serverId;
}

#ifdef NDEBUG
  Errors Storage_init(StorageInfo                   *storageInfo,
                      ServerIO                      *masterIO,
                      const StorageSpecifier        *storageSpecifier,
// TODO: only storageOnMasterFlag need, pass this value and avoid jobOptions?
                      const JobOptions              *jobOptions,
                      BandWidthList                 *maxBandWidthList,
                      ServerConnectionPriorities    serverConnectionPriority,
                      StorageUpdateProgressFunction storageUpdateProgressFunction,
                      void                          *storageUpdateProgressUserData,
                      GetNamePasswordFunction       getNamePasswordFunction,
                      void                          *getNamePasswordUserData,
                      StorageVolumeRequestFunction  storageVolumeRequestFunction,
                      void                          *storageVolumeRequestUserData,
                      IsPauseFunction               isPauseFunction,
                      void                          *isPauseUserData,
                      IsAbortedFunction             isAbortedFunction,
                      void                          *isAbortedUserData,
                      LogHandle                     *logHandle
                     )
#else /* not NDEBUG */
  Errors __Storage_init(const char                    *__fileName__,
                        ulong                         __lineNb__,
                        StorageInfo                   *storageInfo,
                        ServerIO                      *masterIO,
                        const StorageSpecifier        *storageSpecifier,
                        const JobOptions              *jobOptions,
                        BandWidthList                 *maxBandWidthList,
                        ServerConnectionPriorities    serverConnectionPriority,
                        StorageUpdateProgressFunction storageUpdateProgressFunction,
                        void                          *storageUpdateProgressUserData,
                        GetNamePasswordFunction       getNamePasswordFunction,
                        void                          *getNamePasswordUserData,
                        StorageVolumeRequestFunction  storageVolumeRequestFunction,
                        void                          *storageVolumeRequestUserData,
                        IsPauseFunction               isPauseFunction,
                        void                          *isPauseUserData,
                        IsAbortedFunction             isAbortedFunction,
                        void                          *isAbortedUserData,
                        LogHandle                     *logHandle
                       )
#endif /* NDEBUG */
{
  AutoFreeList autoFreeList;
  Errors       error;

  assert(storageInfo != NULL);
  assert(storageSpecifier != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageSpecifier);
  assert(maxBandWidthList != NULL);

  #if !defined(HAVE_CURL) && !defined(HAVE_FTP) && !defined(HAVE_SSH2)
    UNUSED_VARIABLE(serverConnectionPriority);
  #endif /* !defined(HAVE_CURL) && !defined(HAVE_FTP) && !defined(HAVE_SSH2) */

  // initialize variables
  AutoFree_init(&autoFreeList);
  Semaphore_init(&storageInfo->lock,SEMAPHORE_TYPE_BINARY);
  Storage_duplicateSpecifier(&storageInfo->storageSpecifier,storageSpecifier);
  storageInfo->jobOptions              = jobOptions;
  storageInfo->masterIO                = masterIO;
  storageInfo->logHandle               = logHandle;
  storageInfo->updateProgressFunction  = storageUpdateProgressFunction;
  storageInfo->updateProgressUserData  = storageUpdateProgressUserData;
  storageInfo->getNamePasswordFunction = getNamePasswordFunction;
  storageInfo->getNamePasswordUserData = getNamePasswordUserData;
  storageInfo->volumeRequestFunction   = storageVolumeRequestFunction;
  storageInfo->volumeRequestUserData   = storageVolumeRequestUserData;
  storageInfo->isPauseFunction         = isPauseFunction;
  storageInfo->isPauseUserData         = isPauseUserData;
  storageInfo->isAbortedFunction       = isAbortedFunction;
  storageInfo->isAbortedUserData       = isAbortedUserData;
  if ((jobOptions != NULL) && jobOptions->waitFirstVolumeFlag)
  {
    storageInfo->volumeNumber        = 0;
    storageInfo->volumeRequestNumber = 1;
    storageInfo->volumeState         = STORAGE_VOLUME_STATE_UNKNOWN;
  }
  else
  {
    storageInfo->volumeNumber        = 1;
    storageInfo->volumeRequestNumber = 1;
    storageInfo->volumeState         = STORAGE_VOLUME_STATE_LOADED;
  }
  AUTOFREE_ADD(&autoFreeList,&storageInfo->lock,{ Semaphore_done(&storageInfo->lock); });
  AUTOFREE_ADD(&autoFreeList,&storageInfo->storageSpecifier,{ Storage_doneSpecifier(&storageInfo->storageSpecifier); });

  // init protocol specific values
  error = ERROR_UNKNOWN;
  if (   (   (jobOptions == NULL)
          || jobOptions->storageOnMasterFlag
         )
      && (masterIO != NULL)
     )
  {
    error = StorageMaster_init(storageInfo,storageSpecifier,jobOptions);
  }
  else
  {
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        UNUSED_VARIABLE(maxBandWidthList);
        error = ERROR_NONE;
        break;
      case STORAGE_TYPE_FILESYSTEM:
        UNUSED_VARIABLE(maxBandWidthList);
        error = StorageFile_init(storageInfo,jobOptions);
        break;
      case STORAGE_TYPE_FTP:
        error = StorageFTP_init(storageInfo,jobOptions,maxBandWidthList,serverConnectionPriority);
        break;
      case STORAGE_TYPE_SSH:
        UNUSED_VARIABLE(maxBandWidthList);
        error = ERROR_FUNCTION_NOT_SUPPORTED;
        break;
      case STORAGE_TYPE_SCP:
        error = StorageSCP_init(storageInfo,jobOptions,maxBandWidthList,serverConnectionPriority);
        break;
      case STORAGE_TYPE_SFTP:
        error = StorageSFTP_init(storageInfo,jobOptions,maxBandWidthList,serverConnectionPriority);
        break;
      case STORAGE_TYPE_WEBDAV:
      case STORAGE_TYPE_WEBDAVS:
        error = StorageWebDAV_init(storageInfo,jobOptions,maxBandWidthList,serverConnectionPriority);
        break;
      case STORAGE_TYPE_SMB:
        error = StorageSMB_init(storageInfo,jobOptions,maxBandWidthList,serverConnectionPriority);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        error = StorageOptical_init(storageInfo,jobOptions);
        break;
      case STORAGE_TYPE_DEVICE:
        error = StorageDevice_init(storageInfo,jobOptions);
        break;
      default:
        UNUSED_VARIABLE(maxBandWidthList);
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }
  assert(error != ERROR_UNKNOWN);
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  storageInfo->progress.storageDoneBytes = 0LL;
  storageInfo->progress.volumeNumber     = 0;
  storageInfo->progress.volumeDone       = 0.0;
  storageInfo->progress.message.code     = MESSAGE_CODE_NONE;
  storageInfo->progress.message.text     = String_new();

  // free resources
  AutoFree_done(&autoFreeList);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(storageInfo,StorageInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,storageInfo,StorageInfo);
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
    DEBUG_REMOVE_RESOURCE_TRACE(storageInfo,StorageInfo);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,storageInfo,StorageInfo);
  #endif /* NDEBUG */

  String_delete(storageInfo->progress.message.text);

  error = ERROR_UNKNOWN;
  if (   (   (storageInfo->jobOptions == NULL)
          || storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageInfo->masterIO != NULL)
     )
  {
    error = StorageMaster_done(storageInfo);
  }
  else
  {
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        error = ERROR_NONE;
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
      case STORAGE_TYPE_WEBDAVS:
        error = StorageWebDAV_done(storageInfo);
        break;
      case STORAGE_TYPE_SMB:
        error = StorageSMB_done(storageInfo);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        error = StorageOptical_done(storageInfo);
        break;
      case STORAGE_TYPE_DEVICE:
        error = StorageDevice_done(storageInfo);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }
  assert(error != ERROR_UNKNOWN);

  Storage_doneSpecifier(&storageInfo->storageSpecifier);
  Semaphore_done(&storageInfo->lock);

  return error;
}

bool Storage_isServerAllocationPending(const StorageInfo *storageInfo)
{
  bool serverAllocationPending;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  serverAllocationPending = FALSE;
  if (   (   (storageInfo->jobOptions == NULL)
          || storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageInfo->masterIO != NULL)
     )
  {
//TODO
  }
  else
  {
    switch (storageInfo->storageSpecifier.type)
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
      case STORAGE_TYPE_WEBDAVS:
        serverAllocationPending = StorageWebDAV_isServerAllocationPending(storageInfo);
        break;
      case STORAGE_TYPE_SMB:
        serverAllocationPending = StorageSMB_isServerAllocationPending(storageInfo);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        serverAllocationPending = FALSE;
        break;
      case STORAGE_TYPE_DEVICE:
        serverAllocationPending = FALSE;
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }

  return serverAllocationPending;
}

// TODO:required?
const StorageSpecifier *Storage_getStorageSpecifier(const StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);

  return &storageInfo->storageSpecifier;
}

void Storage_pause(const StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);

  while (   ((storageInfo->isPauseFunction != NULL) && storageInfo->isPauseFunction(storageInfo->isPauseUserData))
         && !Storage_isAborted(storageInfo)
        )
  {
    Misc_udelay(500LL*US_PER_MS);
  }
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
  assert(storageInfo->jobOptions != NULL);

  error = ERROR_UNKNOWN;
  if (   (  (storageInfo->jobOptions == NULL)
          || storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageInfo->masterIO != NULL)
     )
  {
    error = StorageMaster_preProcess(storageInfo,archiveName,time,initialFlag);
  }
  else
  {
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        error = ERROR_NONE;
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
      case STORAGE_TYPE_WEBDAVS:
        error = StorageWebDAV_preProcess(storageInfo,archiveName,time,initialFlag);
        break;
      case STORAGE_TYPE_SMB:
        error = StorageSMB_preProcess(storageInfo,archiveName,time,initialFlag);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        error = StorageOptical_preProcess(storageInfo,archiveName,time,initialFlag);
        break;
      case STORAGE_TYPE_DEVICE:
        error = StorageDevice_preProcess(storageInfo,archiveName,time,initialFlag);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }
  assert(error != ERROR_UNKNOWN);

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
  assert(storageInfo->jobOptions != NULL);

  error = ERROR_UNKNOWN;
  if (   (   (storageInfo->jobOptions == NULL)
          || storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageInfo->masterIO != NULL)
     )
  {
    error = StorageMaster_postProcess(storageInfo,archiveName,time,finalFlag);
  }
  else
  {
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        error = ERROR_NONE;
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
      case STORAGE_TYPE_WEBDAVS:
        error = StorageWebDAV_postProcess(storageInfo,archiveName,time,finalFlag);
        break;
      case STORAGE_TYPE_SMB:
        error = StorageSMB_postProcess(storageInfo,archiveName,time,finalFlag);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        error = StorageOptical_postProcess(storageInfo,archiveName,time,finalFlag);
        break;
      case STORAGE_TYPE_DEVICE:
        error = StorageDevice_postProcess(storageInfo,archiveName,time,finalFlag);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
         #endif /* NDEBUG */
        break;
    }
  }
  assert(error != ERROR_UNKNOWN);

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

bool Storage_exists(StorageInfo *storageInfo, ConstString archiveName)
{
  bool existsFlag;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  // get archive name
  if (archiveName == NULL) archiveName = storageInfo->storageSpecifier.archiveName;
  if (String_isEmpty(archiveName))
  {
    return FALSE;
  }

  existsFlag = FALSE;
  if (   (   (storageInfo->jobOptions == NULL)
          || storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageInfo->masterIO != NULL)
     )
  {
    existsFlag = StorageMaster_exists(storageInfo,archiveName);
  }
  else
  {
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        break;
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
      case STORAGE_TYPE_WEBDAVS:
        existsFlag = StorageWebDAV_exists(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_SMB:
        existsFlag = StorageSMB_exists(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        existsFlag = StorageOptical_exists(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_DEVICE:
        existsFlag = StorageDevice_exists(storageInfo,archiveName);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
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
  if (   (   (storageInfo->jobOptions == NULL)
          || storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageInfo->masterIO != NULL)
     )
  {
//TODO
  }
  else
  {
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        break;
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
      case STORAGE_TYPE_WEBDAVS:
        isFileFlag = StorageWebDAV_isFile(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_SMB:
        isFileFlag = StorageSMB_isFile(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        isFileFlag = StorageOptical_isFile(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_DEVICE:
        isFileFlag = StorageDevice_isFile(storageInfo,archiveName);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
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
  if (   (   (storageInfo->jobOptions == NULL)
          || storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageInfo->masterIO != NULL)
     )
  {
//TODO
  }
  else
  {
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        break;
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
      case STORAGE_TYPE_WEBDAVS:
        isDirectoryFlag = StorageWebDAV_isDirectory(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_SMB:
        isDirectoryFlag = StorageSMB_isDirectory(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        isDirectoryFlag = StorageOptical_isDirectory(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_DEVICE:
        isDirectoryFlag = StorageDevice_isDirectory(storageInfo,archiveName);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
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
  if (   (   (storageInfo->jobOptions == NULL)
          || storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageInfo->masterIO != NULL)
     )
  {
//TODO
  }
  else
  {
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        break;
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
      case STORAGE_TYPE_WEBDAVS:
        isReadableFlag = StorageWebDAV_isReadable(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_SMB:
        isReadableFlag = StorageSMB_isReadable(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        isReadableFlag = StorageOptical_isReadable(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_DEVICE:
        isReadableFlag = StorageDevice_isReadable(storageInfo,archiveName);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }

  return isReadableFlag;
}

bool Storage_isWritable(StorageInfo *storageInfo, ConstString archiveName)
{
  bool isWritableFlag;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  // get archive name
  if (archiveName == NULL) archiveName = storageInfo->storageSpecifier.archiveName;
  if (String_isEmpty(archiveName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  isWritableFlag = FALSE;
  if (   (   (storageInfo->jobOptions == NULL)
          || storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageInfo->masterIO != NULL)
     )
  {
//TODO
  }
  else
  {
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        break;
      case STORAGE_TYPE_FILESYSTEM:
        isWritableFlag = StorageFile_isWritable(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_FTP:
        isWritableFlag = StorageFTP_isWritable(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_SSH:
        isWritableFlag = ERROR_FUNCTION_NOT_SUPPORTED;
        break;
      case STORAGE_TYPE_SCP:
        isWritableFlag = StorageSCP_isWritable(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_SFTP:
        isWritableFlag = StorageSFTP_isWritable(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_WEBDAV:
      case STORAGE_TYPE_WEBDAVS:
        isWritableFlag = StorageWebDAV_isWritable(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_SMB:
        isWritableFlag = StorageSMB_isWritable(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        isWritableFlag = StorageOptical_isWritable(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_DEVICE:
        isWritableFlag = StorageDevice_isWritable(storageInfo,archiveName);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }

  return isWritableFlag;
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
  if (   (   (storageInfo->jobOptions == NULL)
          || storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageInfo->masterIO != NULL)
     )
  {
    error = StorageMaster_getTmpName(archiveName,storageInfo);
  }
  else
  {
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        error = ERROR_NONE;
        break;
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
      case STORAGE_TYPE_WEBDAVS:
        error = StorageWebDAV_getTmpName(archiveName,storageInfo);
        break;
      case STORAGE_TYPE_SMB:
        error = StorageSMB_getTmpName(archiveName,storageInfo);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        error = StorageOptical_getTmpName(archiveName,storageInfo);
        break;
      case STORAGE_TYPE_DEVICE:
        error = StorageDevice_getTmpName(archiveName,storageInfo);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

#ifdef NDEBUG
  Errors Storage_create(StorageHandle *storageHandle,
                        StorageInfo   *storageInfo,
                        ConstString   archiveName,
                        uint64        archiveSize,
                        bool          forceFlag
                       )
#else /* not NDEBUG */
  Errors __Storage_create(const char    *__fileName__,
                          ulong         __lineNb__,
                          StorageHandle *storageHandle,
                          StorageInfo   *storageInfo,
                          ConstString   archiveName,
                          uint64        archiveSize,
                          bool          forceFlag
                         )
#endif /* NDEBUG */
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);
  assert(storageInfo->jobOptions != NULL);

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
  if (   (   (storageInfo->jobOptions == NULL)
          || storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageInfo->masterIO != NULL)
     )
  {
    error = StorageMaster_create(storageHandle,archiveName,archiveSize,forceFlag);
  }
  else
  {
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        error = ERROR_NONE;
        break;
      case STORAGE_TYPE_FILESYSTEM:
        error = StorageFile_create(storageHandle,archiveName,archiveSize,forceFlag);
        break;
      case STORAGE_TYPE_FTP:
        error = StorageFTP_create(storageHandle,archiveName,archiveSize,forceFlag);
        break;
      case STORAGE_TYPE_SSH:
        error = ERROR_FUNCTION_NOT_SUPPORTED;
        break;
      case STORAGE_TYPE_SCP:
        error = StorageSCP_create(storageHandle,archiveName,archiveSize,forceFlag);
        break;
      case STORAGE_TYPE_SFTP:
        error = StorageSFTP_create(storageHandle,archiveName,archiveSize,forceFlag);
        break;
      case STORAGE_TYPE_WEBDAV:
      case STORAGE_TYPE_WEBDAVS:
        error = StorageWebDAV_create(storageHandle,archiveName,archiveSize,forceFlag);
        break;
      case STORAGE_TYPE_SMB:
        error = StorageSMB_create(storageHandle,archiveName,archiveSize,forceFlag);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        error = StorageOptical_create(storageHandle,archiveName,archiveSize,forceFlag);
        break;
      case STORAGE_TYPE_DEVICE:
        error = StorageDevice_create(storageHandle,archiveName,archiveSize,forceFlag);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }
  assert(error != ERROR_UNKNOWN);
  if (error != ERROR_NONE)
  {
    return error;
  }

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(storageHandle,StorageHandle);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,storageHandle,StorageHandle);
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
  if (   (   (storageInfo->jobOptions == NULL)
          || storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageInfo->masterIO != NULL)
     )
  {
    error = StorageMaster_open(storageHandle,archiveName);
  }
  else
  {
    switch (storageHandle->storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        error = ERROR_NONE;
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
      case STORAGE_TYPE_WEBDAVS:
        error = StorageWebDAV_open(storageHandle,archiveName);
        break;
      case STORAGE_TYPE_SMB:
        error = StorageSMB_open(storageHandle,archiveName);
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
  }
  assert(error != ERROR_UNKNOWN);
  if (error != ERROR_NONE)
  {
    return error;
  }

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(storageHandle,StorageHandle);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,storageHandle,StorageHandle);
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

  if (   (   (storageHandle->storageInfo->jobOptions == NULL)
          || storageHandle->storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageHandle->storageInfo->masterIO != NULL)
     )
  {
    StorageMaster_close(storageHandle);
  }
  else
  {
    switch (storageHandle->storageInfo->storageSpecifier.type)
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
      case STORAGE_TYPE_WEBDAVS:
        StorageWebDAV_close(storageHandle);
        break;
      case STORAGE_TYPE_SMB:
        StorageSMB_close(storageHandle);
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

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(storageHandle,StorageHandle);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,storageHandle,StorageHandle);
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
  if (   (   (storageHandle->storageInfo->jobOptions == NULL)
          || storageHandle->storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageHandle->storageInfo->masterIO != NULL)
     )
  {
//TODO
  }
  else
  {
    switch (storageHandle->storageInfo->storageSpecifier.type)
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
      case STORAGE_TYPE_WEBDAVS:
        eofFlag = StorageWebDAV_eof(storageHandle);
        break;
      case STORAGE_TYPE_SMB:
        eofFlag = StorageSMB_eof(storageHandle);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        eofFlag = StorageOptical_eof(storageHandle);
        break;
      case STORAGE_TYPE_DEVICE:
        eofFlag = StorageDevice_eof(storageHandle);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
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
  assert(storageHandle->storageInfo->jobOptions != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle->storageInfo);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(buffer != NULL);

  if (bytesRead != NULL) (*bytesRead) = 0L;

  error = ERROR_UNKNOWN;
  if (   (   (storageHandle->storageInfo->jobOptions == NULL)
          || storageHandle->storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageHandle->storageInfo->masterIO != NULL)
     )
  {
error = ERROR_STILL_NOT_IMPLEMENTED;
  }
  else
  {
    switch (storageHandle->storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        error = ERROR_NONE;
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
      case STORAGE_TYPE_WEBDAVS:
        error = StorageWebDAV_read(storageHandle,buffer,bufferSize,bytesRead);
        break;
      case STORAGE_TYPE_SMB:
        error = StorageSMB_read(storageHandle,buffer,bufferSize,bytesRead);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        error = StorageOptical_read(storageHandle,buffer,bufferSize,bytesRead);
        break;
      case STORAGE_TYPE_DEVICE:
        error = StorageDevice_read(storageHandle,buffer,bufferSize,bytesRead);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
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

  error = ERROR_UNKNOWN;
  if (   (   (storageHandle->storageInfo->jobOptions == NULL)
          || storageHandle->storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageHandle->storageInfo->masterIO != NULL)
     )
  {
    error = StorageMaster_write(storageHandle,buffer,bufferLength);
  }
  else
  {
    switch (storageHandle->storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        error = ERROR_NONE;
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
      case STORAGE_TYPE_WEBDAVS:
        error = StorageWebDAV_write(storageHandle,buffer,bufferLength);
        break;
      case STORAGE_TYPE_SMB:
        error = StorageSMB_write(storageHandle,buffer,bufferLength);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        error = StorageOptical_write(storageHandle,buffer,bufferLength);
        break;
      case STORAGE_TYPE_DEVICE:
        error = StorageDevice_write(storageHandle,buffer,bufferLength);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

Errors Storage_transferFromFile(FileHandle                  *fromFileHandle,
                                StorageHandle               *storageHandle,
                                StorageTransferInfoFunction storageTransferInfoFunction,
                                void                        *storageTransferInfoUserData,
                                IsAbortedFunction           isAbortedFunction,
                                void                        *isAbortedUserData
                               )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle->storageInfo);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(fromFileHandle != NULL);

  error = ERROR_UNKNOWN;
  if (   (   (storageHandle->storageInfo->jobOptions == NULL)
          || storageHandle->storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageHandle->storageInfo->masterIO != NULL)
     )
  {
    error = StorageMaster_transfer(storageHandle,fromFileHandle);
  }
  else
  {
    switch (storageHandle->storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        error = ERROR_NONE;
        break;
      case STORAGE_TYPE_FILESYSTEM:
        error = transferFileToStorage(fromFileHandle,
                                      storageHandle,
                                      CALLBACK_(storageTransferInfoFunction,storageTransferInfoUserData),
                                      CALLBACK_(isAbortedFunction,isAbortedUserData)
                                     );
        break;
      case STORAGE_TYPE_FTP:
        error = transferFileToStorage(fromFileHandle,
                                      storageHandle,
                                      CALLBACK_(storageTransferInfoFunction,storageTransferInfoUserData),
                                      CALLBACK_(isAbortedFunction,isAbortedUserData)
                                     );
        break;
      case STORAGE_TYPE_SSH:
        error = ERROR_FUNCTION_NOT_SUPPORTED;
        break;
      case STORAGE_TYPE_SCP:
        error = transferFileToStorage(fromFileHandle,
                                      storageHandle,
                                      CALLBACK_(storageTransferInfoFunction,storageTransferInfoUserData),
                                      CALLBACK_(isAbortedFunction,isAbortedUserData)
                                     );
        break;
      case STORAGE_TYPE_SFTP:
        error = transferFileToStorage(fromFileHandle,
                                      storageHandle,
                                      CALLBACK_(storageTransferInfoFunction,storageTransferInfoUserData),
                                      CALLBACK_(isAbortedFunction,isAbortedUserData)
                                     );
        break;
      case STORAGE_TYPE_WEBDAV:
      case STORAGE_TYPE_WEBDAVS:
        error = transferFileToStorage(fromFileHandle,
                                      storageHandle,
                                      CALLBACK_(storageTransferInfoFunction,storageTransferInfoUserData),
                                      CALLBACK_(isAbortedFunction,isAbortedUserData)
                                     );
        break;
      case STORAGE_TYPE_SMB:
        error = transferFileToStorage(fromFileHandle,
                                      storageHandle,
                                      CALLBACK_(storageTransferInfoFunction,storageTransferInfoUserData),
                                      CALLBACK_(isAbortedFunction,isAbortedUserData)
                                     );
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        error = transferFileToStorage(fromFileHandle,
                                      storageHandle,
                                      CALLBACK_(storageTransferInfoFunction,storageTransferInfoUserData),
                                      CALLBACK_(isAbortedFunction,isAbortedUserData)
                                     );
        break;
      case STORAGE_TYPE_DEVICE:
        error = transferFileToStorage(fromFileHandle,
                                      storageHandle,
                                      CALLBACK_(storageTransferInfoFunction,storageTransferInfoUserData),
                                      CALLBACK_(isAbortedFunction,isAbortedUserData)
                                     );
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
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
  if (   (   (storageHandle->storageInfo->jobOptions == NULL)
          || storageHandle->storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageHandle->storageInfo->masterIO != NULL)
     )
  {
    size = StorageMaster_getSize(storageHandle);
  }
  else
  {
    switch (storageHandle->storageInfo->storageSpecifier.type)
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
      case STORAGE_TYPE_WEBDAVS:
        size = StorageWebDAV_getSize(storageHandle);
        break;
      case STORAGE_TYPE_SMB:
        size = StorageSMB_getSize(storageHandle);
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
  }

  return size;
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

  error = ERROR_UNKNOWN;
  if (   (   (storageHandle->storageInfo->jobOptions == NULL)
          || storageHandle->storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageHandle->storageInfo->masterIO != NULL)
     )
  {
error = ERROR_STILL_NOT_IMPLEMENTED;
  }
  else
  {
    switch (storageHandle->storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        (*offset) = 0LL;
        error = ERROR_NONE;
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
      case STORAGE_TYPE_WEBDAVS:
        error = StorageWebDAV_tell(storageHandle,offset);
        break;
      case STORAGE_TYPE_SMB:
        error = StorageSMB_tell(storageHandle,offset);
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

  error = ERROR_UNKNOWN;
  if (   (   (storageHandle->storageInfo->jobOptions == NULL)
          || storageHandle->storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageHandle->storageInfo->masterIO != NULL)
     )
  {
error = ERROR_STILL_NOT_IMPLEMENTED;
  }
  else
  {
    switch (storageHandle->storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        error = ERROR_NONE;
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
      case STORAGE_TYPE_WEBDAVS:
        error = StorageWebDAV_seek(storageHandle,offset);
        break;
      case STORAGE_TYPE_SMB:
        error = StorageSMB_seek(storageHandle,offset);
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
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

Errors Storage_copyToLocal(const StorageSpecifier        *storageSpecifier,
                           ConstString                   localFileName,
                           const JobOptions              *jobOptions,
                           BandWidthList                 *maxBandWidthList,
                           StorageUpdateProgressFunction storageUpdateProgressFunction,
                           void                          *storageUpdateProgressUserData,
                           StorageVolumeRequestFunction  storageVolumeRequestFunction,
                           void                          *storageVolumeRequestUserData
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
                       CALLBACK_(storageUpdateProgressFunction,storageUpdateProgressUserData),
                       CALLBACK_(NULL,NULL),  // updateStatusInfo
                       CALLBACK_(storageVolumeRequestFunction,storageVolumeRequestUserData),
                       CALLBACK_(NULL,NULL),  // isPause
                       CALLBACK_(NULL,NULL),  // isAborted
                       NULL  // logHandle
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

Errors Storage_copy(StorageInfo                 *fromStorageInfo,
                    ConstString                 fromArchiveName,
                    StorageInfo                 *toStorageInfo,
                    ConstString                 toArchiveName,
                    uint64                      archiveSize,
                    StorageTransferInfoFunction storageTransferInfoFunction,
                    void                        *storageTransferInfoUserData,
                    IsAbortedFunction           isAbortedFunction,
                    void                        *isAbortedUserData
                   )
{
  StorageHandle fromStorageHandle,toStorageHandle;
  Errors        error;

  // open storages
  error = Storage_open(&fromStorageHandle,fromStorageInfo,fromArchiveName);
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Storage_create(&toStorageHandle,toStorageInfo,toArchiveName,archiveSize,TRUE);
  if (error != ERROR_NONE)
  {
    Storage_close(&fromStorageHandle);
    return error;
  }

  // transfer data
  error = transferStorageToStorage(&fromStorageHandle,
                                   &toStorageHandle,
                                   CALLBACK_(storageTransferInfoFunction,storageTransferInfoUserData),
                                   CALLBACK_(isAbortedFunction,isAbortedUserData)
                                  );
  if (error != ERROR_NONE)
  {
    Storage_close(&toStorageHandle);
    Storage_close(&fromStorageHandle);
    (void)Storage_delete(toStorageInfo,toArchiveName);
    return error;
  }

  // close storages
  Storage_close(&toStorageHandle);
  Storage_close(&fromStorageHandle);

  // free resources

  return ERROR_NONE;
}

Errors Storage_rename(StorageInfo *storageInfo,
                      ConstString fromArchiveName,
                      ConstString toArchiveName
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
  if (   (   (storageInfo->jobOptions == NULL)
          || storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageInfo->masterIO != NULL)
     )
  {
error = ERROR_STILL_NOT_IMPLEMENTED;
  }
  else
  {
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        error = ERROR_NONE;
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
      case STORAGE_TYPE_WEBDAVS:
        error = StorageWebDAV_rename(storageInfo,fromArchiveName,toArchiveName);
        break;
      case STORAGE_TYPE_SMB:
        error = StorageSMB_rename(storageInfo,fromArchiveName,toArchiveName);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        error = StorageOptical_rename(storageInfo,fromArchiveName,toArchiveName);
        break;
      case STORAGE_TYPE_DEVICE:
        error = StorageDevice_rename(storageInfo,fromArchiveName,toArchiveName);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

Errors Storage_makeDirectory(StorageInfo *storageInfo, ConstString pathName)
{
  String          directoryName;
  ConstString     name;
  JobOptions      jobOptions;
  StringTokenizer stringTokenizer;
  Errors          error;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  // get path name
  if (pathName == NULL) pathName = storageInfo->storageSpecifier.archiveName;
  if (String_isEmpty(pathName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  error         = ERROR_NONE;
  directoryName = String_new();
  Job_initOptions(&jobOptions);
  File_initSplitFileName(&stringTokenizer,pathName);
  while (   (error == ERROR_NONE)
         && File_getNextSplitFileName(&stringTokenizer,&name)
        )
  {
    File_appendFileName(directoryName,name);
    if (!Storage_exists(storageInfo,directoryName))
    {
      switch (storageInfo->storageSpecifier.type)
      {
        case STORAGE_TYPE_NONE:
          error = ERROR_NONE;
          break;
        case STORAGE_TYPE_FILESYSTEM:
          error = StorageFile_makeDirectory(storageInfo,directoryName);
          break;
        case STORAGE_TYPE_FTP:
          error = StorageFTP_makeDirectory(storageInfo,directoryName);
          break;
        case STORAGE_TYPE_SSH:
          #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
          #else /* not HAVE_SSH2 */
          #endif /* HAVE_SSH2 */
          break;
        case STORAGE_TYPE_SCP:
          error = StorageSCP_makeDirectory(storageInfo,directoryName);
          break;
        case STORAGE_TYPE_SFTP:
          error = StorageSFTP_makeDirectory(storageInfo,directoryName);
          break;
        case STORAGE_TYPE_WEBDAV:
        case STORAGE_TYPE_WEBDAVS:
          error = StorageWebDAV_makeDirectory(storageInfo,directoryName);
          break;
        case STORAGE_TYPE_SMB:
          error = StorageSMB_makeDirectory(storageInfo,directoryName);
          break;
        case STORAGE_TYPE_CD:
        case STORAGE_TYPE_DVD:
        case STORAGE_TYPE_BD:
          error = StorageOptical_makeDirectory(storageInfo,directoryName);
          break;
        case STORAGE_TYPE_DEVICE:
          error = StorageDevice_makeDirectory(storageInfo,directoryName);
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break;
      }
    }
  }
  File_doneSplitFileName(&stringTokenizer);
  Job_doneOptions(&jobOptions);
  String_delete(directoryName);

  return error;
}

Errors Storage_pruneDirectories(StorageInfo *storageInfo, ConstString pathName)
{
  String                     directoryName;
  JobOptions                 jobOptions;
  bool                       isEmpty;
  Errors                     error;
  StorageDirectoryListHandle storageDirectoryListHandle;

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  // get path name
  if (pathName == NULL) pathName = storageInfo->storageSpecifier.archiveName;
  if (String_isEmpty(pathName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  directoryName = File_getDirectoryName(String_new(),pathName);
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
        error = ERROR_UNKNOWN;
        if (   (   (storageInfo->jobOptions == NULL)
                || storageInfo->jobOptions->storageOnMasterFlag
               )
            && (storageInfo->masterIO != NULL)
           )
        {
error = ERROR_STILL_NOT_IMPLEMENTED;
        }
        else
        {
          switch (storageInfo->storageSpecifier.type)
          {
            case STORAGE_TYPE_NONE:
              error = ERROR_NONE;
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
            case STORAGE_TYPE_WEBDAVS:
              error = StorageWebDAV_delete(storageInfo,directoryName);
              break;
            case STORAGE_TYPE_SMB:
              error = StorageSMB_delete(storageInfo,directoryName);
              break;
            case STORAGE_TYPE_CD:
            case STORAGE_TYPE_DVD:
            case STORAGE_TYPE_BD:
              error = StorageOptical_delete(storageInfo,directoryName);
              break;
            case STORAGE_TYPE_DEVICE:
              error = StorageDevice_delete(storageInfo,directoryName);
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break;
          }
        }
        assert(error != ERROR_UNKNOWN);
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
  if (   (   (storageInfo->jobOptions == NULL)
          || storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageInfo->masterIO != NULL)
     )
  {
error = ERROR_STILL_NOT_IMPLEMENTED;
  }
  else
  {
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        error = ERROR_NONE;
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
      case STORAGE_TYPE_WEBDAVS:
        error = StorageWebDAV_delete(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_SMB:
        error = StorageSMB_delete(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        error = StorageOptical_delete(storageInfo,archiveName);
        break;
      case STORAGE_TYPE_DEVICE:
        error = StorageDevice_delete(storageInfo,archiveName);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

Errors Storage_getFileInfo(FileInfo    *fileInfo,
                           StorageInfo *storageInfo,
                           ConstString archiveName
                          )
{
  Errors error;

  assert(fileInfo != NULL);
  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);
  assert(storageInfo->jobOptions != NULL);

  // get archive name
  if (archiveName == NULL) archiveName = storageInfo->storageSpecifier.archiveName;
  if (String_isEmpty(archiveName))
  {
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  error = ERROR_UNKNOWN;
  if (   (   (storageInfo->jobOptions == NULL)
          || storageInfo->jobOptions->storageOnMasterFlag
         )
      && (storageInfo->masterIO != NULL)
     )
  {
error = ERROR_STILL_NOT_IMPLEMENTED;
  }
  else
  {
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        error = ERROR_NONE;
        break;
      case STORAGE_TYPE_FILESYSTEM:
        error = StorageFile_getFileInfo(fileInfo,storageInfo,archiveName);
        break;
      case STORAGE_TYPE_FTP:
        error = StorageFTP_getFileInfo(fileInfo,storageInfo,archiveName);
        break;
      case STORAGE_TYPE_SSH:
        #ifdef HAVE_SSH2
          HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
        #else /* not HAVE_SSH2 */
        #endif /* HAVE_SSH2 */
        break;
      case STORAGE_TYPE_SCP:
        error = StorageSCP_getFileInfo(fileInfo,storageInfo,archiveName);
        break;
      case STORAGE_TYPE_SFTP:
        error = StorageSFTP_getFileInfo(fileInfo,storageInfo,archiveName);
        break;
      case STORAGE_TYPE_WEBDAV:
      case STORAGE_TYPE_WEBDAVS:
        error = StorageWebDAV_getFileInfo(fileInfo,storageInfo,archiveName);
        break;
      case STORAGE_TYPE_SMB:
        error = StorageSMB_getFileInfo(fileInfo,storageInfo,archiveName);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        error = StorageOptical_getFileInfo(fileInfo,storageInfo,archiveName);
        break;
      case STORAGE_TYPE_DEVICE:
        error = StorageDevice_getFileInfo(fileInfo,storageInfo,archiveName);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

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
    case STORAGE_TYPE_WEBDAVS:
      error = StorageWebDAV_openDirectoryList(storageDirectoryListHandle,storageSpecifier,directory,jobOptions,serverConnectionPriority);
      break;
    case STORAGE_TYPE_SMB:
      error = StorageSMB_openDirectoryList(storageDirectoryListHandle,storageSpecifier,directory,jobOptions,serverConnectionPriority);
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = StorageOptical_openDirectoryList(storageDirectoryListHandle,storageSpecifier,directory,jobOptions,serverConnectionPriority);
      break;
    case STORAGE_TYPE_DEVICE:
      error = StorageDevice_openDirectoryList(storageDirectoryListHandle,storageSpecifier,directory,jobOptions,serverConnectionPriority);
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

  DEBUG_ADD_RESOURCE_TRACE(storageDirectoryListHandle,StorageDirectoryListHandle);

  return ERROR_NONE;
}

void Storage_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(storageDirectoryListHandle,StorageDirectoryListHandle);

  switch (storageDirectoryListHandle->storageSpecifier.type)
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
    case STORAGE_TYPE_WEBDAVS:
      StorageWebDAV_closeDirectoryList(storageDirectoryListHandle);
      break;
    case STORAGE_TYPE_SMB:
      StorageSMB_closeDirectoryList(storageDirectoryListHandle);
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
  DEBUG_CHECK_RESOURCE_TRACE(storageDirectoryListHandle);

  endOfDirectoryFlag = TRUE;

  switch (storageDirectoryListHandle->storageSpecifier.type)
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
    case STORAGE_TYPE_WEBDAVS:
      endOfDirectoryFlag = StorageWebDAV_endOfDirectoryList(storageDirectoryListHandle);
      break;
    case STORAGE_TYPE_SMB:
      endOfDirectoryFlag = StorageSMB_endOfDirectoryList(storageDirectoryListHandle);
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
  DEBUG_CHECK_RESOURCE_TRACE(storageDirectoryListHandle);

  error = ERROR_UNKNOWN;
  switch (storageDirectoryListHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      error = ERROR_NONE;
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
    case STORAGE_TYPE_WEBDAVS:
      error = StorageWebDAV_readDirectoryList(storageDirectoryListHandle,fileName,fileInfo);
      break;
    case STORAGE_TYPE_SMB:
      error = StorageSMB_readDirectoryList(storageDirectoryListHandle,fileName,fileInfo);
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

Errors Storage_forAll(const StorageSpecifier  *storageSpecifier,
                      ConstString             directory,
                      const char              *patternString,
                      bool                    skipUnreadableFlag,
                      StorageFunction         storageFunction,
                      void                    *storageUserData,
                      StorageProgressFunction storageProgressFunction,
                      void                    *storageProgressUserData
                     )
{
  JobOptions                 jobOptions;
  StringList                 directoryList;
  String                     name;
  Pattern                    pattern;
  FileSystemInfo             fileSystemInfo;
  ulong                      totalCount;
  Errors                     error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  ulong                      doneCount;
  FileInfo                   fileInfo;

  assert(storageSpecifier != NULL);
  assert(storageFunction != NULL);

// TODO:
#ifndef WERROR
#warning skipUnreadableFlag
#endif
UNUSED_VARIABLE(skipUnreadableFlag);

  // init variables
  Job_initOptions(&jobOptions);
  StringList_init(&directoryList);
  name = String_new();

  // parse pattern
  if (patternString != NULL)
  {
    error = Pattern_initCString(&pattern,
                                patternString,
                                PATTERN_TYPE_GLOB,
                                PATTERN_FLAG_NONE
                               );
    if (error != ERROR_NONE)
    {
      String_delete(name);
      StringList_done(&directoryList);
      Job_doneOptions(&jobOptions);
      return error;
    }
  }

  // get total number of files (if possible)
  totalCount = 0L;
  if ((directory != NULL) || !String_isEmpty(storageSpecifier->archiveName))
  {
    if (File_getFileSystemInfo(&fileSystemInfo,
                               (directory != NULL)
                                 ? directory
                                 : storageSpecifier->archiveName
                              ) == ERROR_NONE
       )
    {
      totalCount = fileSystemInfo.totalFiles;
    }
  }

  // read directory and scan all sub-directories
  StringList_append(&directoryList,(directory != NULL) ? directory : storageSpecifier->archiveName);
  doneCount = 0L;
  error     = ERROR_NONE;
  while (   !StringList_isEmpty(&directoryList)
         && (error == ERROR_NONE)
        )
  {
    StringList_removeLast(&directoryList,name);

    // open directory
    error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                      storageSpecifier,
                                      name,
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
        error = Storage_readDirectoryList(&storageDirectoryListHandle,name,&fileInfo);
        if (error != ERROR_NONE)
        {
          break;
        }

        // check if sub-directory, add to directory list
        if (fileInfo.type == FILE_TYPE_DIRECTORY)
        {
          // open storage
          StorageInfo storageInfo;
          error = Storage_init(&storageInfo,
                               NULL, // masterIO
                               storageSpecifier,
                               NULL, // jobOptions,
                               &globalOptions.indexDatabaseMaxBandWidthList,
                               SERVER_CONNECTION_PRIORITY_HIGH,
                               CALLBACK_(NULL,NULL),  // storageUpdateProgress
                               CALLBACK_(NULL,NULL),  // updateStatusInfo
                               CALLBACK_(NULL,NULL),  // storageVolumeRequest
                               CALLBACK_(NULL,NULL),  // isPause
                               CALLBACK_(NULL,NULL),  // isAborted
                               NULL  // logHandle
                              );
          if (error == ERROR_NONE)
          {
            if (!skipUnreadableFlag || Storage_isReadable(&storageInfo,name))
            {
              StringList_append(&directoryList,name);
            }
            Storage_done(&storageInfo);
          }
        }

        // match pattern and call storage callback on match
        if (   (storageFunction != NULL)
            && (   (   (patternString == NULL)
                    && String_equals(storageSpecifier->archiveName,name)
                   )
                || (   (patternString != NULL)
                    && Pattern_match(&pattern,name,STRING_BEGIN,PATTERN_MATCH_MODE_EXACT,NULL,NULL)
                   )
               )
           )
        {
          error = storageFunction(Storage_getName(NULL,storageSpecifier,name),
                                  &fileInfo,
                                  storageUserData
                                 );
        }

        // call progress callback
        if (storageProgressFunction != NULL)
        {
          storageProgressFunction(doneCount,totalCount,storageProgressUserData);
        }

        doneCount++;
      }

      // close directory
      Storage_closeDirectoryList(&storageDirectoryListHandle);
    }
  }

  // free resources
  if (patternString != NULL)
  {
    Pattern_done(&pattern);
  }
  String_delete(name);
  StringList_done(&directoryList);
  Job_doneOptions(&jobOptions);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
