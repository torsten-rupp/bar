/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: storage functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#ifdef HAVE_FTP
  #include <ftplib.h>
#endif /* HAVE_FTP */
#ifdef HAVE_SSH2
  #include <libssh2.h>
  #include <libssh2_sftp.h>
#endif /* HAVE_SSH2 */
#ifdef HAVE_ISO9660
  #include <cdio/cdio.h>
  #include <cdio/iso9660.h>
#endif /* HAVE_ISO9660 */
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "stringlists.h"
#include "files.h"
#include "network.h"
#include "database.h"
#include "errors.h"

#include "errors.h"
#include "crypt.h"
#include "passwords.h"
#include "misc.h"
#include "archive.h"
#include "bar.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
/* file data buffer size */
#define BUFFER_SIZE (64*1024)

#define READ_TIMEOUT (60*1000)

#define MAX_BUFFER_SIZE     (64*1024)
#define MAX_FILENAME_LENGTH (8*1024)

#define UNLOAD_VOLUME_DELAY_TIME (10LL*MISC_US_PER_SECOND) /* [us] */
#define LOAD_VOLUME_DELAY_TIME   (10LL*MISC_US_PER_SECOND) /* [us] */

#define MAX_CD_SIZE  (900LL*1024LL*1024LL)     // 900M
#define MAX_DVD_SIZE (2LL*4613734LL*1024LL)    // 9G (dual layer)
#define MAX_BD_SIZE  (2LL*25LL*1024LL*1024LL)  // 50G (dual layer)

#define CD_VOLUME_SIZE      (700LL*1024LL*1024LL)
#define CD_VOLUME_ECC_SIZE  (560LL*1024LL*1024LL)
#define DVD_VOLUME_SIZE     (4482LL*1024LL*1024LL)
#define DVD_VOLUME_ECC_SIZE (3600LL*1024LL*1024LL)
#define BD_VOLUME_SIZE      (25LL*1024LL*1024LL*1024LL)
#define BD_VOLUME_ECC_SIZE  (20LL*1024LL*1024LL*1024LL)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
#ifdef HAVE_FTP
  LOCAL Password *defaultFTPPassword;
#endif /* HAVE_FTP */
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
* Name   : updateStatusInfo
* Purpose: update status info
* Input  : storageStatusInfo - storage status info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateStatusInfo(const StorageFileHandle *storageFileHandle)
{
  assert(storageFileHandle != NULL);

  if (storageFileHandle->storageStatusInfoFunction != NULL)
  {
    storageFileHandle->storageStatusInfoFunction(storageFileHandle->storageStatusInfoUserData,
                                                 &storageFileHandle->runningInfo
                                                );
  }
}

/***********************************************************************\
* Name   : initFTPPassword
* Purpose: init FTP password
* Input  : hostName   - host name
*          loginName  - login name
*          jobOptions - job options
* Output : -
* Return : TRUE if FTP password intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

#ifdef HAVE_FTP
LOCAL bool initFTPPassword(const String hostName, const String loginName, const JobOptions *jobOptions)
{
  String s;
  bool   initFlag;

  assert(jobOptions != NULL);

  if (jobOptions->ftpServer.password == NULL)
  {
    if (globalOptions.runMode == RUN_MODE_INTERACTIVE)
    {
      s = String_format(String_new(),"FTP login password for %S@%S",hostName,loginName);
      initFlag = Password_input(defaultFTPPassword,String_cString(s),PASSWORD_INPUT_MODE_ANY);
      String_delete(s);
    }
    else
    {
      initFlag = FALSE;
    }
  }
  else
  {
    initFlag = TRUE;
  }

  return initFlag;
}
#endif /* HAVE_FTP */

/***********************************************************************\
* Name   : initSSHPassword
* Purpose: init SSH password
* Input  : hostName   - host name
*          loginName  - login name
*          jobOptions - job options
* Output : -
* Return : TRUE if SSH password intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SSH2
LOCAL bool initSSHPassword(const String hostName, const String loginName, const JobOptions *jobOptions)
{
  String s;
  bool   initFlag;

  assert(jobOptions != NULL);

  if (jobOptions->sshServer.password == NULL)
  {
    if (globalOptions.runMode == RUN_MODE_INTERACTIVE)
    {
      s = !String_isEmpty(loginName)
            ? String_format(String_new(),"SSH login password for %S@%S",loginName,hostName)
            : String_format(String_new(),"SSH login password for %S",hostName);
      initFlag = Password_input(defaultSSHPassword,String_cString(s),PASSWORD_INPUT_MODE_ANY);
      String_delete(s);
    }
    else
    {
      initFlag = FALSE;
    }
  }
  else
  {
    initFlag = TRUE;
  }

  return initFlag;
}
#endif /* HAVE_SSH2 */

#ifdef HAVE_FTP
/***********************************************************************\
* Name   : checkFTPLogin
* Purpose: check if FTP login is possible
* Input  : hostName      - host name
*          hostPort      - host port or 0
*          loginName     - login name
*          loginPassword - login password
* Output : -
* Return : ERROR_NONE if login is possible, error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors checkFTPLogin(const String hostName,
                           uint         hostPort,
                           const String loginName,
                           Password     *loginPassword
                          )
{
  netbuf     *ftpControl;
  const char *plainLoginPassword;

// NYI: TODO: support different FTP port
  UNUSED_VARIABLE(hostPort);

  // check host name (Note: FTP library crash if host name is not valid!)
  if (!Network_hostExists(hostName))
  {
    return ERROR_HOST_NOT_FOUND;
  }

  // connect
  if (FtpConnect(String_cString(hostName),&ftpControl) != 1)
  {
    return ERROR_FTP_SESSION_FAIL;
  }

  // login
  plainLoginPassword = Password_deploy(loginPassword);
  if (FtpLogin(String_cString(loginName),
               plainLoginPassword,
               ftpControl
              ) != 1
     )
  {
    Password_undeploy(loginPassword);
    FtpClose(ftpControl);
    return ERROR_FTP_AUTHENTICATION;
  }
  Password_undeploy(loginPassword);
  FtpQuit(ftpControl);

  return ERROR_NONE;
}
#endif /* HAVE_FTP */

#ifdef HAVE_FTP
/***********************************************************************\
* Name   : ftpTimeoutCallback
* Purpose: callback on FTP timeout
* Input  : loginName - login name
*          password  - login password
*          hostName  - host name
* Output : -
* Return : always 0 to trigger error
* Notes  : -
\***********************************************************************/

LOCAL int ftpTimeoutCallback(netbuf *control,
                             int    transferdBytes,
                             void   *userData
                            )
{
  UNUSED_VARIABLE(control);
  UNUSED_VARIABLE(transferdBytes);
  UNUSED_VARIABLE(userData);

  return 0;
}

#endif /* HAVE_FTP */

#ifdef HAVE_SSH2
/***********************************************************************\
* Name   : checkSSHLogin
* Purpose: check if SSH login is possible
* Input  : loginName          - login name
*          password           - login password
*          hostName           - host name
*          hostPort           - host SSH port
*          publicKeyFileName  - SSH public key file name
*          privateKeyFileName - SSH private key file name
* Output : -
* Return : ERROR_NONE if login is possible, error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors checkSSHLogin(const String loginName,
                           Password     *password,
                           const String hostName,
                           uint         hostPort,
                           const String publicKeyFileName,
                           const String privateKeyFileName
                          )
{
  SocketHandle socketHandle;
  Errors       error;

  printInfo(5,"SSH: public key '%s'\n",String_cString(publicKeyFileName));
  printInfo(5,"SSH: private key '%s'\n",String_cString(privateKeyFileName));
  error = Network_connect(&socketHandle,
                          SOCKET_TYPE_SSH,
                          hostName,
                          hostPort,
                          loginName,
                          password,
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
#endif /* HAVE_SSH2 */

#ifdef HAVE_SSH2
/***********************************************************************\
* Name   : sshSendCallback
* Purpose: ssh send callback
* Input  : socket   - libssh2 socket
*          buffer   - buffer with data
*          length   - length to send
*          flags    - libssh2 flags
*          abstract - pointer to user data
* Output : -
* Return : number of bytes sent
* Notes  : -
\***********************************************************************/

LOCAL LIBSSH2_SEND_FUNC(sshSendCallback)
{
  StorageFileHandle *storageFileHandle;
  ssize_t           n;

  assert(abstract != NULL);

  storageFileHandle = *((StorageFileHandle**)abstract);
  assert(storageFileHandle != NULL);
  assert(storageFileHandle->scp.oldSendCallback != NULL);

  n = storageFileHandle->scp.oldSendCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageFileHandle->scp.totalSentBytes += (uint64)n;

  return n;
}

/***********************************************************************\
* Name   : sshReceiveCallback
* Purpose: ssh receive callback
* Input  : socket   - libssh2 socket
*          buffer   - buffer with data
*          length   - length to receive
*          flags    - libssh2 flags
*          abstract - pointer to user data
* Output : -
* Return : number of bytes received
* Notes  : -
\***********************************************************************/

LOCAL LIBSSH2_RECV_FUNC(sshReceiveCallback)
{
  StorageFileHandle *storageFileHandle;
  ssize_t           n;

  assert(abstract != NULL);

  storageFileHandle = *((StorageFileHandle**)abstract);
  assert(storageFileHandle != NULL);
  assert(storageFileHandle->scp.oldReceiveCallback != NULL);

  n = storageFileHandle->scp.oldReceiveCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageFileHandle->scp.totalReceivedBytes += (uint64)n;

  return n;
}
#endif /* HAVE_SSH2 */

#ifdef HAVE_SSH2
/***********************************************************************\
* Name   : sftpSendCallback
* Purpose: sftp send callback
* Input  : socket   - libssh2 socket
*          buffer   - buffer with data
*          length   - length to send
*          flags    - libssh2 flags
*          abstract - pointer to user data
* Output : -
* Return : number of bytes sent
* Notes  : -
\***********************************************************************/

LOCAL LIBSSH2_SEND_FUNC(sftpSendCallback)
{
  StorageFileHandle *storageFileHandle;
  ssize_t           n;

  assert(abstract != NULL);

  storageFileHandle = *((StorageFileHandle**)abstract);
  assert(storageFileHandle != NULL);
  assert(storageFileHandle->sftp.oldSendCallback != NULL);

  n = storageFileHandle->sftp.oldSendCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageFileHandle->sftp.totalSentBytes += (uint64)n;

  return n;
}

/***********************************************************************\
* Name   : sftpReceiveCallback
* Purpose: sftp receive callback
* Input  : socket   - libssh2 socket
*          buffer   - buffer with data
*          length   - length to receive
*          flags    - libssh2 flags
*          abstract - pointer to user data
* Output : -
* Return : number of bytes received
* Notes  : -
\***********************************************************************/

LOCAL LIBSSH2_RECV_FUNC(sftpReceiveCallback)
{
  StorageFileHandle *storageFileHandle;
  ssize_t           n;

  assert(abstract != NULL);

  storageFileHandle = *((StorageFileHandle**)abstract);
  assert(storageFileHandle != NULL);
  assert(storageFileHandle->sftp.oldReceiveCallback != NULL);

  n = storageFileHandle->sftp.oldReceiveCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageFileHandle->sftp.totalReceivedBytes += (uint64)n;

  return n;
}
#endif /* HAVE_SSH2 */

#ifdef HAVE_SSH2
/***********************************************************************\
* Name   : waitSessionSocket
* Purpose: wait a little until session socket can be read/write
* Input  : socketHandle - socket handle
* Output : -
* Return : TRUE if session socket can be read/write, FALSE on
+          error/timeout
* Notes  : -
\***********************************************************************/

LOCAL bool waitSessionSocket(SocketHandle *socketHandle)
{
  LIBSSH2_SESSION *session;
  struct timeval  tv;
  fd_set          fdSet;

  assert(socketHandle != NULL);

  // get session
  session = Network_getSSHSession(socketHandle);
  assert(session != NULL);

  // wait for max. 60s
  tv.tv_sec  = 60;
  tv.tv_usec = 0;
  FD_ZERO(&fdSet);
  FD_SET(socketHandle->handle,&fdSet);
  return (select(socketHandle->handle+1,
                 ((libssh2_session_block_directions(session) & LIBSSH2_SESSION_BLOCK_INBOUND ) != 0) ? &fdSet : NULL,
                 ((libssh2_session_block_directions(session) & LIBSSH2_SESSION_BLOCK_OUTBOUND) != 0) ? &fdSet : NULL,
                 NULL,
                 &tv
                ) > 0
         );
}
#endif /* HAVE_SSH2 */

#if defined(HAVE_FTP) || defined(HAVE_SSH2)
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
#endif /* defined(HAVE_FTP) || defined(HAVE_SSH2) */

#if defined(HAVE_FTP) || defined(HAVE_SSH2)
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
//fprintf(stderr,"%s, %d: delayTime=%llu us %lu\n",__FILE__,__LINE__,delayTime,storageBandWidthLimiter->blockSize);
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
#endif /* defined(HAVE_FTP) || defined(HAVE_SSH2) */

/***********************************************************************\
* Name   : executeIOOutput
* Purpose: process exec stdout, stderr output
* Input  : userData - user data (not used)
*          line     - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOutput(void         *userData,
                           const String line
                          )
{
  assert(line != NULL);

  UNUSED_VARIABLE(userData);

  printInfo(4,"%s\n",String_cString(line));
}

/***********************************************************************\
* Name   : requestNewMedium
* Purpose: request new cd/dvd/bd medium
* Input  : storageFileHandle - storage file handle
*          waitFlag          - TRUE to wait for new medium
* Output : -
* Return : TRUE if new medium loaded, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors requestNewMedium(StorageFileHandle *storageFileHandle, bool waitFlag)
{
  TextMacro             textMacros[2];
  bool                  mediumRequestedFlag;
  StorageRequestResults storageRequestResult;

  TEXT_MACRO_N_STRING (textMacros[0],"%device",storageFileHandle->storageSpecifier.deviceName);
  TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageFileHandle->requestedVolumeNumber      );

  if (   (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_UNKNOWN)
      || (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_LOADED)
     )
  {
    // sleep a short time to give hardware time for finishing volume, then unload current volume
    printInfo(0,"Unload medium #%d...",storageFileHandle->volumeNumber);
    Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
    Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.unloadVolumeCommand),
                        textMacros,SIZE_OF_ARRAY(textMacros),
                        (ExecuteIOFunction)executeIOOutput,
                        (ExecuteIOFunction)executeIOOutput,
                        NULL
                       );
    printInfo(0,"ok\n");

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_UNLOADED;
  }

  // request new medium
  mediumRequestedFlag  = FALSE;
  storageRequestResult = STORAGE_REQUEST_VOLUME_UNKNOWN;
  if      (storageFileHandle->requestVolumeFunction != NULL)
  {
    mediumRequestedFlag = TRUE;

    // request new medium via call back, unload if requested
    do
    {
      storageRequestResult = storageFileHandle->requestVolumeFunction(storageFileHandle->requestVolumeUserData,
                                                                      storageFileHandle->requestedVolumeNumber
                                                                     );
      if (storageRequestResult == STORAGE_REQUEST_VOLUME_UNLOAD)
      {
        // sleep a short time to give hardware time for finishing volume, then unload current medium
        printInfo(0,"Unload medium...");
        Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.unloadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            (ExecuteIOFunction)executeIOOutput,
                            (ExecuteIOFunction)executeIOOutput,
                            NULL
                           );
        printInfo(0,"ok\n");
      }
    }
    while (storageRequestResult == STORAGE_REQUEST_VOLUME_UNLOAD);

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else if (storageFileHandle->opticalDisk.write.requestVolumeCommand != NULL)
  {
    mediumRequestedFlag = TRUE;

    // request new volume via external command
    printInfo(0,"Request new medium #%d...",storageFileHandle->requestedVolumeNumber);
    if (Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.requestVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            (ExecuteIOFunction)executeIOOutput,
                            (ExecuteIOFunction)executeIOOutput,
                            NULL
                           ) == ERROR_NONE
       )
    {
      printInfo(0,"ok\n");
      storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
    }
    else
    {
      printInfo(0,"FAIL\n");
      storageRequestResult = STORAGE_REQUEST_VOLUME_FAIL;
    }

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else
  {
    if (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_UNLOADED)
    {
      if (waitFlag)
      {
        mediumRequestedFlag = TRUE;

        printInfo(0,"Please insert medium #%d into drive '%s' and press ENTER to continue\n",storageFileHandle->requestedVolumeNumber,String_cString(storageFileHandle->storageSpecifier.deviceName));
        Misc_waitEnter();

        storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
      }
      else
      {
        printInfo(0,"Please insert medium #%d into drive '%s'\n",storageFileHandle->requestedVolumeNumber,String_cString(storageFileHandle->storageSpecifier.deviceName));
      }
    }
    else
    {
      if (waitFlag)
      {
        mediumRequestedFlag = TRUE;

        printInfo(0,"Press ENTER to continue\n");
        Misc_waitEnter();

        storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
      }
    }

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }

  if (mediumRequestedFlag)
  {
    switch (storageRequestResult)
    {
      case STORAGE_REQUEST_VOLUME_OK:
        // load volume, then sleep a short time to give hardware time for reading volume information
        printInfo(0,"Load medium #%d...",storageFileHandle->requestedVolumeNumber);
        Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.loadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            (ExecuteIOFunction)executeIOOutput,
                            (ExecuteIOFunction)executeIOOutput,
                            NULL
                           );
        Misc_udelay(LOAD_VOLUME_DELAY_TIME);
        printInfo(0,"ok\n");

        // store new volume number
        storageFileHandle->volumeNumber = storageFileHandle->requestedVolumeNumber;

        // update status info
        storageFileHandle->runningInfo.volumeNumber = storageFileHandle->volumeNumber;
        updateStatusInfo(storageFileHandle);

        storageFileHandle->volumeState = STORAGE_VOLUME_STATE_LOADED;
        return ERROR_NONE;
        break;
      case STORAGE_REQUEST_VOLUME_ABORTED:
        return ERROR_NONE;
        break;
      default:
        return ERROR_LOAD_VOLUME_FAIL;
        break;
    }
  }
  else
  {
    return ERROR_NONE;
  }
}

/***********************************************************************\
* Name   : requestNewVolume
* Purpose: request new volume
* Input  : storageFileHandle - storage file handle
*          waitFlag          - TRUE to wait for new volume
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors requestNewVolume(StorageFileHandle *storageFileHandle, bool waitFlag)
{
  TextMacro             textMacros[2];
  bool                  volumeRequestedFlag;
  StorageRequestResults storageRequestResult;

  TEXT_MACRO_N_STRING (textMacros[0],"%device",storageFileHandle->storageSpecifier.deviceName);
  TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageFileHandle->requestedVolumeNumber);

  if (   (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_UNKNOWN)
      || (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_LOADED)
     )
  {
    // sleep a short time to give hardware time for finishing volume; unload current volume
    printInfo(0,"Unload volume #%d...",storageFileHandle->volumeNumber);
    Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
    Misc_executeCommand(String_cString(storageFileHandle->device.unloadVolumeCommand),
                        textMacros,SIZE_OF_ARRAY(textMacros),
                        (ExecuteIOFunction)executeIOOutput,
                        (ExecuteIOFunction)executeIOOutput,
                        NULL
                       );
    printInfo(0,"ok\n");

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_UNLOADED;
  }

  // request new volume
  volumeRequestedFlag  = FALSE;
  storageRequestResult = STORAGE_REQUEST_VOLUME_UNKNOWN;
  if      (storageFileHandle->requestVolumeFunction != NULL)
  {
    volumeRequestedFlag = TRUE;

    // request new volume via call back, unload if requested
    do
    {
      storageRequestResult = storageFileHandle->requestVolumeFunction(storageFileHandle->requestVolumeUserData,
                                                                      storageFileHandle->requestedVolumeNumber
                                                                     );
      if (storageRequestResult == STORAGE_REQUEST_VOLUME_UNLOAD)
      {
        // sleep a short time to give hardware time for finishing volume, then unload current medium
        printInfo(0,"Unload volume...");
        Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
        Misc_executeCommand(String_cString(storageFileHandle->device.unloadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            (ExecuteIOFunction)executeIOOutput,
                            (ExecuteIOFunction)executeIOOutput,
                            NULL
                           );
        printInfo(0,"ok\n");
      }
    }
    while (storageRequestResult == STORAGE_REQUEST_VOLUME_UNLOAD);

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else if (storageFileHandle->device.requestVolumeCommand != NULL)
  {
    volumeRequestedFlag = TRUE;

    // request new volume via external command
    printInfo(0,"Request new volume #%d...",storageFileHandle->requestedVolumeNumber);
    if (Misc_executeCommand(String_cString(storageFileHandle->device.loadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            (ExecuteIOFunction)executeIOOutput,
                            (ExecuteIOFunction)executeIOOutput,
                            NULL
                           ) == ERROR_NONE
       )
    {
      printInfo(0,"ok\n");
      storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
    }
    else
    {
      printInfo(0,"FAIL\n");
      storageRequestResult = STORAGE_REQUEST_VOLUME_FAIL;
    }

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else
  {
    if (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_UNLOADED)
    {
      if (waitFlag)
      {
        volumeRequestedFlag = TRUE;

        printInfo(0,"Please insert volume #%d into drive '%s' and press ENTER to continue\n",storageFileHandle->requestedVolumeNumber,storageFileHandle->storageSpecifier.deviceName);
        Misc_waitEnter();

        storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
      }
      else
      {
        printInfo(0,"Please insert volume #%d into drive '%s'\n",storageFileHandle->requestedVolumeNumber,storageFileHandle->storageSpecifier.deviceName);
      }
    }
    else
    {
      if (waitFlag)
      {
        volumeRequestedFlag = TRUE;

        printInfo(0,"Press ENTER to continue\n");
        Misc_waitEnter();

        storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
      }
    }

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }

  if (volumeRequestedFlag)
  {
    switch (storageRequestResult)
    {
      case STORAGE_REQUEST_VOLUME_OK:
        // load volume; sleep a short time to give hardware time for reading volume information
        printInfo(0,"Load volume #%d...",storageFileHandle->requestedVolumeNumber);
        Misc_executeCommand(String_cString(storageFileHandle->device.loadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            (ExecuteIOFunction)executeIOOutput,
                            (ExecuteIOFunction)executeIOOutput,
                            NULL
                           );
        Misc_udelay(LOAD_VOLUME_DELAY_TIME);
        printInfo(0,"ok\n");

        // store new volume number
        storageFileHandle->volumeNumber = storageFileHandle->requestedVolumeNumber;

        // update status info
        storageFileHandle->runningInfo.volumeNumber = storageFileHandle->volumeNumber;
        updateStatusInfo(storageFileHandle);

        storageFileHandle->volumeState = STORAGE_VOLUME_STATE_LOADED;
        return ERROR_NONE;
        break;
      case STORAGE_REQUEST_VOLUME_ABORTED:
        return ERROR_NONE;
        break;
      default:
        return ERROR_LOAD_VOLUME_FAIL;
        break;
    }
  }
  else
  {
    return ERROR_NONE;
  }
}

/***********************************************************************\
* Name   : executeIOmkisofs
* Purpose: process mkisofs output
* Input  : storageFileHandle - storage file handle variable
*          line              - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOmkisofs(StorageFileHandle *storageFileHandle,
                            const String      line
                           )
{
  String s;
  double p;

  assert(storageFileHandle != NULL);

//fprintf(stderr,"%s,%d: line=%s\n",__FILE__,__LINE__,String_cString(line));
  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".* ([0-9\\.]+)% done.*",NULL,NULL,s,NULL))
  {
//fprintf(stderr,"%s,%d: mkisofs: %s\n",__FILE__,__LINE__,String_cString(line));
    p = String_toDouble(s,0,NULL,NULL,0);
    storageFileHandle->runningInfo.volumeProgress = ((double)storageFileHandle->opticalDisk.write.step*100.0+p)/(double)(storageFileHandle->opticalDisk.write.steps*100);
    updateStatusInfo(storageFileHandle);
  }
  String_delete(s);

  executeIOOutput(NULL,line);
}

/***********************************************************************\
* Name   : executeIODVDisaster
* Purpose: process dvdisaster output
* Input  : storageFileHandle - storage file handle variable
*          line              - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOdvdisaster(StorageFileHandle *storageFileHandle,
                               const String      line
                              )
{
  String s;
  double p;

  assert(storageFileHandle != NULL);

  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".*adding space\\): +([0-9\\.]+)%",NULL,NULL,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
    storageFileHandle->runningInfo.volumeProgress = ((double)(storageFileHandle->opticalDisk.write.step+0)*100.0+p)/(double)(storageFileHandle->opticalDisk.write.steps*100);
    updateStatusInfo(storageFileHandle);
  }
  if (String_matchCString(line,STRING_BEGIN,".*generation: +([0-9\\.]+)%",NULL,NULL,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
    storageFileHandle->runningInfo.volumeProgress = ((double)(storageFileHandle->opticalDisk.write.step+1)*100.0+p)/(double)(storageFileHandle->opticalDisk.write.steps*100);
    updateStatusInfo(storageFileHandle);
  }
  String_delete(s);

  executeIOOutput(NULL,line);
}

/***********************************************************************\
* Name   : executeIOgrowisofs
* Purpose: process growisofs output
* Input  : storageFileHandle - storage file handle variable
*          line              - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOgrowisofs(StorageFileHandle *storageFileHandle,
                              const String      line
                             )
{
  String s;
  double p;

  assert(storageFileHandle != NULL);

  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".* \\(([0-9\\.]+)%\\) .*",NULL,NULL,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
    storageFileHandle->runningInfo.volumeProgress = ((double)storageFileHandle->opticalDisk.write.step*100.0+p)/(double)(storageFileHandle->opticalDisk.write.steps*100);
    updateStatusInfo(storageFileHandle);
  }
  String_delete(s);

  executeIOOutput(NULL,line);
}

/*---------------------------------------------------------------------*/

Errors Storage_initAll(void)
{
  #ifdef HAVE_FTP
    FtpInit();
    defaultFTPPassword = Password_new();
  #endif /* HAVE_FTP */
  #ifdef HAVE_SSH2
    defaultSSHPassword = Password_new();
  #endif /* HAVE_SSH2 */

  return ERROR_NONE;
}

void Storage_doneAll(void)
{
  #ifdef HAVE_SSH2
    Password_delete(defaultSSHPassword);
  #endif /* HAVE_SSH2 */
  #ifdef HAVE_FTP
    Password_delete(defaultFTPPassword);
  #endif /* HAVE_FTP */
}

void Storage_initSpecifier(StorageSpecifier *storageSpecifier)
{
  assert(storageSpecifier != NULL);

  storageSpecifier->string        = String_new();
  storageSpecifier->hostName      = String_new();
  storageSpecifier->hostPort      = 0;
  storageSpecifier->loginName     = String_new();
  storageSpecifier->loginPassword = Password_new();
  storageSpecifier->deviceName    = String_new();
}

void Storage_doneSpecifier(StorageSpecifier *storageSpecifier)
{
  assert(storageSpecifier != NULL);

  String_delete(storageSpecifier->deviceName);
  Password_delete(storageSpecifier->loginPassword);
  String_delete(storageSpecifier->loginName);
  String_delete(storageSpecifier->hostName);
  String_delete(storageSpecifier->string);
}

bool Storage_parseFTPSpecifier(const String ftpSpecifier,
                               String       hostName,
                               uint         *hostPort,
                               String       loginName,
                               Password     *loginPassword
                              )
{
  const char* LOGINNAME_MAP_FROM[] = {"\\@"};
  const char* LOGINNAME_MAP_TO[]   = {"@"};

  bool   result;
  String s,t;

  assert(ftpSpecifier != NULL);

  String_clear(hostName);
  if (hostPort != NULL) (*hostPort) = 0;
  String_clear(loginName);
  if (loginPassword != NULL) Password_clear(loginPassword);

  s = String_new();
  t = String_new();
  if      (String_matchCString(ftpSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@:/]*?):([[:digit:]]+)$",NULL,NULL,loginName,s,STRING_NO_ASSIGN,hostName,t,NULL))
  {
    // <login name>:<login password>@<host name>:<host port>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (loginPassword != NULL) Password_setString(loginPassword,s);
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(t,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (String_matchCString(ftpSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)$",NULL,NULL,loginName,s,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>:<login password>@<host name>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (loginPassword != NULL) Password_setString(loginPassword,s);

    result = TRUE;
  }
  else if (String_matchCString(ftpSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^@:/]*?):([[:digit:]]+)$",NULL,NULL,loginName,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <login name>@<host name>:<host port>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (String_matchCString(ftpSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^@/]*?)$",NULL,NULL,loginName,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>@<host name>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));

    result = TRUE;
  }
  else if (String_matchCString(ftpSpecifier,STRING_BEGIN,"^([^@:/]*?):([[:digit:]]+)$",NULL,NULL,hostName,s,NULL))
  {
    // <host name>:<host port>
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (!String_isEmpty(ftpSpecifier))
  {
    // <host name>
    String_set(hostName,ftpSpecifier);

    result = TRUE;
  }
  else
  {
    result = FALSE;
  }
  String_delete(t);
  String_delete(s);

  return result;
}

bool Storage_parseSSHSpecifier(const String sshSpecifier,
                               String       hostName,
                               uint         *hostPort,
                               String       loginName
                              )
{
  const char* LOGINNAME_MAP_FROM[] = {"\\@"};
  const char* LOGINNAME_MAP_TO[]   = {"@"};

  bool   result;
  String s;

  assert(sshSpecifier != NULL);

  String_clear(hostName);
  if (hostPort != NULL) (*hostPort) = 0;
  String_clear(loginName);

  s = String_new();
  if      (String_matchCString(sshSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^@:/]*?):[[:digit:]]+$",NULL,NULL,loginName,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <login name>@<host name>:<host port>
    result = String_parse(sshSpecifier,STRING_BEGIN,"%S@%S:%d",NULL,loginName,hostName,hostPort);

    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^@/]*?)$",NULL,NULL,loginName,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>@<host name>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^([^@:/]*?):[[:digit:]]+$",NULL,NULL,hostName,s,NULL))
  {
    // <host name>:<host port>
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);

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

bool Storage_parseDeviceSpecifier(const String deviceSpecifier,
                                  const String defaultDeviceName,
                                  String       deviceName
                                 )
{
  bool result;

  assert(deviceSpecifier != NULL);

  String_clear(deviceName);

  if (String_matchCString(deviceSpecifier,STRING_BEGIN,"[^:]*:.*",NULL,NULL,NULL))
  {
    result = String_parse(deviceSpecifier,STRING_BEGIN,"%S:%S",NULL,deviceName,NULL);
  }
  else
  {
    if (deviceName != NULL) String_set(deviceName,defaultDeviceName);

    result =TRUE;
  }

  return result;
}

StorageTypes Storage_getType(const String storageName)
{
  StorageSpecifier storageSpecifier;
  StorageTypes     type;

  Storage_initSpecifier(&storageSpecifier);
  if (Storage_parseName(storageName,&storageSpecifier,NULL) == ERROR_NONE)
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

Errors Storage_parseName(const String     storageName,
                         StorageSpecifier *storageSpecifier,
                         String           fileName
                        )
{
  long nextIndex;

  assert(storageSpecifier != NULL);

  String_clear(storageSpecifier->string);
  String_clear(storageSpecifier->hostName);
  storageSpecifier->hostPort = 0;
  String_clear(storageSpecifier->loginName);
  Password_clear(storageSpecifier->loginPassword);
  String_clear(storageSpecifier->deviceName);

  if      (String_startsWithCString(storageName,"ftp://"))
  {
    if (   String_matchCString(storageName,6,"^[^:]*:([^@]|\\@)*?@[^/]*/{0,1}",&nextIndex,NULL,NULL)  // ftp://<login name>:<login password>@<host name>[:<host port>]/<file name>
        || String_matchCString(storageName,6,"^([^@]|\\@)*?@[^/]*/{0,1}",&nextIndex,NULL,NULL)        // ftp://<login name>@<host name>[:<host port>]/<file name>
        || String_matchCString(storageName,6,"^[^/]*/{0,1}",&nextIndex,NULL,NULL)                     // ftp://<host name>[:<host port>]/<file name>
       )
    {
      String_sub(storageSpecifier->string,storageName,6,nextIndex-1);
      if (!Storage_parseFTPSpecifier(storageSpecifier->string,
                                     storageSpecifier->hostName,
                                     &storageSpecifier->hostPort,
                                     storageSpecifier->loginName,
                                     storageSpecifier->loginPassword
                                    )
         )
      {
        return ERROR_INVALID_FTP_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,6+nextIndex,STRING_END);
    }
    else
    {
      if (fileName != NULL) String_sub(fileName,storageName,6,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_FTP;
  }
  else if (String_startsWithCString(storageName,"ssh://"))
  {
    if (   String_matchCString(storageName,6,"^([^@]|\\@)*?@[^:]*:[^/]*/{0,1}",&nextIndex,NULL,NULL)  // ssh://<login name>@<host name>[:<host port>]/<file name>
        || String_matchCString(storageName,6,"^[^:]*:[^/]*/{0,1}",&nextIndex,NULL,NULL)               // ssh://<host name>[:<host port>]/<file name>
        || String_matchCString(storageName,6,"^[^/]*/{0,1}",&nextIndex,NULL,NULL)                     // ssh://<host name>/<file name>
       )
    {
      String_sub(storageSpecifier->string,storageName,6,nextIndex-1);
      if (!Storage_parseSSHSpecifier(storageSpecifier->string,
                                     storageSpecifier->hostName,
                                     &storageSpecifier->hostPort,
                                     storageSpecifier->loginName
                                    )
         )
      {
        return ERROR_INVALID_SSH_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,6+nextIndex,STRING_END);
    }
    else
    {
      if (fileName != NULL) String_sub(fileName,storageName,6,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_SSH;
  }
  else if (String_startsWithCString(storageName,"scp://"))
  {
    if (   String_matchCString(storageName,6,"^([^@]|\\@)*?@[^:]*:[^/]*/{0,1}",&nextIndex,NULL,NULL)  // scp://<login name>@<host name>[:<host port>]/<file name>
        || String_matchCString(storageName,6,"^[^:]*:[^/]*/{0,1}",&nextIndex,NULL,NULL)               // scp://<host name>[:<host port>]/<file name>
        || String_matchCString(storageName,6,"^[^/]*/{0,1}",&nextIndex,NULL,NULL)                     // scp://<host name>/<file name>
       )
    {
      String_sub(storageSpecifier->string,storageName,6,nextIndex-1);
      if (!Storage_parseSSHSpecifier(storageSpecifier->string,
                                     storageSpecifier->hostName,
                                     &storageSpecifier->hostPort,
                                     storageSpecifier->loginName
                                    )
         )
      {
        return ERROR_INVALID_SSH_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,6+nextIndex,STRING_END);
    }
    else
    {
      if (fileName != NULL) String_sub(fileName,storageName,6,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_SCP;
  }
  else if (String_startsWithCString(storageName,"sftp://"))
  {
    if (   String_matchCString(storageName,7,"^([^@]|\\@)*?@[^:]*:[^/]*/{0,1}",&nextIndex,NULL,NULL)  // sftp://<login name>@<host name>[:<host port>]/<file name>
        || String_matchCString(storageName,7,"^[^:]*:[^/]*/{0,1}",&nextIndex,NULL,NULL)               // sftp://<host name>[:<host port>]/<file name>
        || String_matchCString(storageName,7,"^[^/]*/{0,1}",&nextIndex,NULL,NULL)                     // sftp://<host name>/<file name>
       )
    {
      String_sub(storageSpecifier->string,storageName,7,nextIndex-1);
      if (!Storage_parseSSHSpecifier(storageSpecifier->string,
                                     storageSpecifier->hostName,
                                     &storageSpecifier->hostPort,
                                     storageSpecifier->loginName
                                    )
         )
      {
        return ERROR_INVALID_SSH_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,7+nextIndex,STRING_END);
    }
    else
    {
      if (fileName != NULL) String_sub(fileName,storageName,7,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_SFTP;
  }
  else if (String_startsWithCString(storageName,"cd://"))
  {
    if (   String_matchCString(storageName,5,"^[^:]+:[^/]*/{0,1}",&nextIndex,NULL,NULL)               // cd://<device name>:<file name>
        || String_matchCString(storageName,5,"^[^/]*/{0,1}",&nextIndex,NULL,NULL)                     // cd://<file name>
       )
    {
      String_sub(storageSpecifier->string,storageName,5,nextIndex-1);
      if (!Storage_parseDeviceSpecifier(storageSpecifier->string,
                                        NULL,
                                        storageSpecifier->deviceName
                                       )
         )
      {
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,5+nextIndex,STRING_END);
    }
    else
    {
      if (fileName != NULL) String_sub(fileName,storageName,5,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_CD;
  }
  else if (String_startsWithCString(storageName,"dvd://"))
  {
    if (   String_matchCString(storageName,6,"^[^:]+:[^/]*/{0,1}",&nextIndex,NULL,NULL)               // dvd://<device name>:<file name>
        || String_matchCString(storageName,6,"^[^/]*/{0,1}",&nextIndex,NULL,NULL)                     // dvd://<device name>:<file name>
       )
    {
      String_sub(storageSpecifier->string,storageName,6,nextIndex-1);
      if (!Storage_parseDeviceSpecifier(storageSpecifier->string,
                                        NULL,
                                        storageSpecifier->deviceName
                                       )
         )
      {
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,6+nextIndex,STRING_END);
    }
    else
    {
      if (fileName != NULL) String_sub(fileName,storageName,6,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_DVD;
  }
  else if (String_startsWithCString(storageName,"bd://"))
  {
    if (   String_matchCString(storageName,5,"^[^:]+:[^/]*/{0,1}",&nextIndex,NULL,NULL)               // bd://<device name>:<file name>
        || String_matchCString(storageName,5,"^[^/]*/{0,1}",&nextIndex,NULL,NULL)                     // bd://<device name>:<file name>
       )
    {
      String_sub(storageSpecifier->string,storageName,5,nextIndex-1);
      if (!Storage_parseDeviceSpecifier(storageSpecifier->string,
                                        NULL,
                                        storageSpecifier->deviceName
                                       )
         )
      {
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,5+nextIndex,STRING_END);
    }
    else
    {
      if (fileName != NULL) String_sub(fileName,storageName,5,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_BD;
  }
  else if (String_startsWithCString(storageName,"device://"))
  {
    if (   String_matchCString(storageName,9,"^[^:]+:[^/]*/{0,1}",&nextIndex,NULL,NULL)               // device://<device name>:<file name>
        || String_matchCString(storageName,9,"^[^/]*/{0,1}",&nextIndex,NULL,NULL)                     // device://<device name>:<file name>
       )
    {
      String_sub(storageSpecifier->string,storageName,9,nextIndex-1);
      if (!Storage_parseDeviceSpecifier(storageSpecifier->string,
                                        NULL,
                                        storageSpecifier->deviceName
                                       )
         )
      {
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,9+nextIndex,STRING_END);
    }
    else
    {
      if (fileName != NULL) String_sub(fileName,storageName,9,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_DEVICE;
  }
  else if (String_startsWithCString(storageName,"file://"))
  {
    if (fileName != NULL) String_sub(fileName,storageName,7,STRING_END);

    storageSpecifier->type = STORAGE_TYPE_FILESYSTEM;
  }
  else
  {
    if (fileName != NULL) String_set(fileName,storageName);

    storageSpecifier->type = STORAGE_TYPE_FILESYSTEM;
  }

  return ERROR_NONE;
}

bool Storage_equalNames(const String storageName1,
                        const String storageName2
                       )
{
  StorageSpecifier storageSpecifier1,storageSpecifier2;
  String           fileName1,fileName2;
  bool             result;

  // parse storage names
  Storage_initSpecifier(&storageSpecifier1);
  Storage_initSpecifier(&storageSpecifier2);
  fileName1         = String_new();
  fileName2         = String_new();
  if (   (Storage_parseName(storageName1,&storageSpecifier1,fileName1) != ERROR_NONE)
      || (Storage_parseName(storageName2,&storageSpecifier2,fileName2) != ERROR_NONE)
     )
  {
    String_delete(fileName2);
    String_delete(fileName1);
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
        result = String_equals(fileName1,fileName2);
        break;
      case STORAGE_TYPE_FTP:
      case STORAGE_TYPE_SSH:
      case STORAGE_TYPE_SCP:
      case STORAGE_TYPE_SFTP:
        result =    String_equals(storageSpecifier1.hostName,storageSpecifier2.hostName)
                 && String_equals(storageSpecifier1.loginName,storageSpecifier2.loginName)
                 && String_equals(fileName1,fileName2);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
      case STORAGE_TYPE_DEVICE:
        result =    String_equals(storageSpecifier1.deviceName,storageSpecifier2.deviceName)
                 && String_equals(fileName1,fileName2);
        result = String_equals(fileName1,fileName2);
        break;
      default:
        break;
    }
  }

  // free resources
  String_delete(fileName2);
  String_delete(fileName1);
  Storage_doneSpecifier(&storageSpecifier2);
  Storage_doneSpecifier(&storageSpecifier1);

  return result;
}

String Storage_getName(String       storageName,
                       StorageTypes storageType,
                       const String storageSpecifier,
                       const String fileName
                      )
{
  assert(storageName != NULL);
  assert(storageSpecifier != NULL);

  String_clear(storageName);
  switch (storageType)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!String_isEmpty(fileName))
      {
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_FTP:
      String_appendCString(storageName,"ftp://");
      String_append(storageName,storageSpecifier);
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_SSH:
      if (!String_isEmpty(fileName))
      {
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_SCP:
      String_appendCString(storageName,"scp://");
      String_append(storageName,storageSpecifier);
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_SFTP:
      String_appendCString(storageName,"sftp://");
      String_append(storageName,storageSpecifier);
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_CD:
      String_appendCString(storageName,"cd://");
      String_append(storageName,storageSpecifier);
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_DVD:
      String_appendCString(storageName,"dvd://");
      String_append(storageName,storageSpecifier);
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_BD:
      String_appendCString(storageName,"bd://");
      String_append(storageName,storageSpecifier);
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_DEVICE:
      String_appendCString(storageName,"device://");
      String_append(storageName,storageSpecifier);
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return storageName;
}

String Storage_getPrintableName(String       string,
                                const String storageName
                               )
{
  StorageSpecifier storageSpecifier;
  String           fileName;

  assert(string != NULL);
  assert(storageName != NULL);

  String_clear(string);

  Storage_initSpecifier(&storageSpecifier);
  fileName = String_new();
  if (Storage_parseName(storageName,&storageSpecifier,fileName) == ERROR_NONE)
  {
    switch (storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        break;
      case STORAGE_TYPE_FILESYSTEM:
        if (!String_isEmpty(fileName))
        {
          String_append(string,fileName);
        }
        break;
      case STORAGE_TYPE_FTP:
        {
          String loginName;
          String hostName;
          uint   hostPort;

          loginName = String_new();
          hostName  = String_new();

          String_appendCString(string,"ftp://");
          if (Storage_parseFTPSpecifier(storageSpecifier.string,hostName,&hostPort,loginName,NULL))
          {
            String_append(string,loginName);
            String_appendChar(string,'@');
            String_append(string,hostName);
            if ((hostPort != 0) && (hostPort != 21))
            {
              String_format(string,":%d",hostPort);
            }
          }
          else
          {
            String_append(string,storageSpecifier.string);
          }
          if (!String_isEmpty(fileName))
          {
            String_appendChar(string,'/');
            String_append(string,fileName);
          }

          String_delete(hostName);
          String_delete(loginName);
        }
        break;
      case STORAGE_TYPE_SSH:
        if (!String_isEmpty(fileName))
        {
          String_append(string,fileName);
        }
        break;
      case STORAGE_TYPE_SCP:
        String_appendCString(string,"scp://");
        String_append(string,storageSpecifier.string);
        if (!String_isEmpty(fileName))
        {
          String_appendChar(string,'/');
          String_append(string,fileName);
        }
        break;
      case STORAGE_TYPE_SFTP:
        String_appendCString(string,"sftp://");
        String_append(string,storageSpecifier.string);
        if (!String_isEmpty(fileName))
        {
          String_appendChar(string,'/');
          String_append(string,fileName);
        }
        break;
      case STORAGE_TYPE_CD:
        String_appendCString(string,"cd://");
        String_append(string,storageSpecifier.string);
        if (!String_isEmpty(fileName))
        {
          String_appendChar(string,'/');
          String_append(string,fileName);
        }
        break;
      case STORAGE_TYPE_DVD:
        String_appendCString(string,"dvd://");
        String_append(string,storageSpecifier.string);
        if (!String_isEmpty(fileName))
        {
          String_appendChar(string,'/');
          String_append(string,fileName);
        }
        break;
      case STORAGE_TYPE_BD:
        String_appendCString(string,"bd://");
        String_append(string,storageSpecifier.string);
        if (!String_isEmpty(fileName))
        {
          String_appendChar(string,'/');
          String_append(string,fileName);
        }
        break;
      case STORAGE_TYPE_DEVICE:
        String_appendCString(string,"device://");
        String_append(string,storageSpecifier.string);
        if (!String_isEmpty(fileName))
        {
          String_appendChar(string,'/');
          String_append(string,fileName);
        }
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }
  String_delete(fileName);
  Storage_doneSpecifier(&storageSpecifier);

  return string;
}

Errors Storage_init(StorageFileHandle            *storageFileHandle,
                    const String                 storageName,
                    const JobOptions             *jobOptions,
                    BandWidthList                *maxBandWidthList,
                    StorageRequestVolumeFunction storageRequestVolumeFunction,
                    void                         *storageRequestVolumeUserData,
                    StorageStatusInfoFunction    storageStatusInfoFunction,
                    void                         *storageStatusInfoUserData,
                    String                       fileName
                   )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageName != NULL);
  assert(jobOptions != NULL);
  assert(fileName != NULL);

  storageFileHandle->mode                      = STORAGE_MODE_UNKNOWN;
  storageFileHandle->jobOptions                = jobOptions;
  storageFileHandle->requestVolumeFunction     = storageRequestVolumeFunction;
  storageFileHandle->requestVolumeUserData     = storageRequestVolumeUserData;
  if (jobOptions->waitFirstVolumeFlag)
  {
    storageFileHandle->volumeNumber          = 0;
    storageFileHandle->requestedVolumeNumber = 0;
    storageFileHandle->volumeState           = STORAGE_VOLUME_STATE_UNKNOWN;
  }
  else
  {
    storageFileHandle->volumeNumber          = 1;
    storageFileHandle->requestedVolumeNumber = 1;
    storageFileHandle->volumeState           = STORAGE_VOLUME_STATE_LOADED;
  }
  storageFileHandle->storageStatusInfoFunction = storageStatusInfoFunction;
  storageFileHandle->storageStatusInfoUserData = storageStatusInfoUserData;

  Storage_initSpecifier(&storageFileHandle->storageSpecifier);
  error = Storage_parseName(storageName,&storageFileHandle->storageSpecifier,fileName);
  if (error != ERROR_NONE)
  {
    Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
    return error;
  }
  if (String_isEmpty(fileName))
  {
    Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }
  switch (storageFileHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      // init variables
      storageFileHandle->type = STORAGE_TYPE_FILESYSTEM;
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        {
          FTPServer ftpServer;

          // init variables
          storageFileHandle->type = STORAGE_TYPE_FTP;
          initBandWidthLimiter(&storageFileHandle->ftp.bandWidthLimiter,maxBandWidthList);

          // allocate read-ahead buffer
          storageFileHandle->ftp.readAheadBuffer.data = (byte*)malloc(MAX_BUFFER_SIZE);
          if (storageFileHandle->ftp.readAheadBuffer.data == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }

          // get FTP server settings
          getFTPServerSettings(storageFileHandle->storageSpecifier.hostName,jobOptions,&ftpServer);
          if (String_isEmpty(storageFileHandle->storageSpecifier.loginName)) String_set(storageFileHandle->storageSpecifier.loginName,ftpServer.loginName);

          // check FTP login, get correct password
          error = ERROR_FTP_SESSION_FAIL;
          if ((error != ERROR_NONE) && !Password_empty(storageFileHandle->storageSpecifier.loginPassword))
          {
            error = checkFTPLogin(storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  storageFileHandle->storageSpecifier.loginPassword
                                 );
          }
          if ((error != ERROR_NONE) && (ftpServer.password != NULL))
          {
            error = checkFTPLogin(storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  ftpServer.password
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(storageFileHandle->storageSpecifier.loginPassword,ftpServer.password);
            }
          }
          if ((error != ERROR_NONE) && !Password_empty(defaultFTPPassword))
          {
            error = checkFTPLogin(storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  defaultFTPPassword
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(storageFileHandle->storageSpecifier.loginPassword,defaultFTPPassword);
            }
          }
          if (error != ERROR_NONE)
          {
            // initialize default password
            if (initFTPPassword(storageFileHandle->storageSpecifier.hostName,storageFileHandle->storageSpecifier.loginName,jobOptions))
            {
              error = checkFTPLogin(storageFileHandle->storageSpecifier.hostName,
                                    storageFileHandle->storageSpecifier.hostPort,
                                    storageFileHandle->storageSpecifier.loginName,
                                    defaultFTPPassword
                                   );
              if (error == ERROR_NONE)
              {
                Password_set(storageFileHandle->storageSpecifier.loginPassword,defaultFTPPassword);
              }
            }
            else
            {
              error = !Password_empty(defaultFTPPassword) ? ERROR_INVALID_FTP_PASSWORD : ERROR_NO_FTP_PASSWORD;
            }
          }
          if (error != ERROR_NONE)
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return error;
          }
        }
      #else /* not HAVE_FTP */
        Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          Errors    error;
          SSHServer sshServer;

          // init variables
          storageFileHandle->type                      = STORAGE_TYPE_SCP;
          storageFileHandle->scp.sshPublicKeyFileName  = NULL;
          storageFileHandle->scp.sshPrivateKeyFileName = NULL;
          initBandWidthLimiter(&storageFileHandle->scp.bandWidthLimiter,maxBandWidthList);

          // allocate read-ahead buffer
          storageFileHandle->scp.readAheadBuffer.data = (byte*)malloc(MAX_BUFFER_SIZE);
          if (storageFileHandle->scp.readAheadBuffer.data == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }

          // get SSH server settings
          getSSHServerSettings(storageFileHandle->storageSpecifier.hostName,jobOptions,&sshServer);
          if (String_isEmpty(storageFileHandle->storageSpecifier.loginName)) String_set(storageFileHandle->storageSpecifier.loginName,sshServer.loginName);
          if (storageFileHandle->storageSpecifier.hostPort == 0) storageFileHandle->storageSpecifier.hostPort = sshServer.port;
          storageFileHandle->scp.sshPublicKeyFileName  = sshServer.publicKeyFileName;
          storageFileHandle->scp.sshPrivateKeyFileName = sshServer.privateKeyFileName;

          // check if SSH login is possible
          error = ERROR_UNKNOWN;
          if ((error == ERROR_UNKNOWN) && (sshServer.password != NULL))
          {
            error = checkSSHLogin(storageFileHandle->storageSpecifier.loginName,
                                  sshServer.password,
                                  storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->scp.sshPublicKeyFileName,
                                  storageFileHandle->scp.sshPrivateKeyFileName
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(storageFileHandle->storageSpecifier.loginPassword,sshServer.password);
            }
          }
          if (error == ERROR_UNKNOWN)
          {
            // initialize default password
            if (initSSHPassword(storageFileHandle->storageSpecifier.hostName,storageFileHandle->storageSpecifier.loginName,jobOptions))
            {
              error = checkSSHLogin(storageFileHandle->storageSpecifier.loginName,
                                    defaultSSHPassword,
                                    storageFileHandle->storageSpecifier.hostName,
                                    storageFileHandle->storageSpecifier.hostPort,
                                    storageFileHandle->scp.sshPublicKeyFileName,
                                    storageFileHandle->scp.sshPrivateKeyFileName
                                   );
              if (error == ERROR_NONE)
              {
                Password_set(storageFileHandle->storageSpecifier.loginPassword,defaultSSHPassword);
              }
            }
            else
            {
              error = !Password_empty(defaultSSHPassword) ? ERROR_INVALID_SSH_PASSWORD : ERROR_NO_SSH_PASSWORD;
            }
          }
          if (error != ERROR_NONE)
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return error;
          }

          // free resources
        }
      #else /* not HAVE_SSH2 */
        Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          Errors    error;
          SSHServer sshServer;

          // init variables
          storageFileHandle->type                       = STORAGE_TYPE_SFTP;
          storageFileHandle->sftp.sshPublicKeyFileName  = NULL;
          storageFileHandle->sftp.sshPrivateKeyFileName = NULL;
          initBandWidthLimiter(&storageFileHandle->sftp.bandWidthLimiter,maxBandWidthList);

          // allocate read-ahead buffer
          storageFileHandle->sftp.readAheadBuffer.data = (byte*)malloc(MAX_BUFFER_SIZE);
          if (storageFileHandle->sftp.readAheadBuffer.data == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }

          // get SSH server settings
          getSSHServerSettings(storageFileHandle->storageSpecifier.hostName,jobOptions,&sshServer);
          if (String_isEmpty(storageFileHandle->storageSpecifier.loginName)) String_set(storageFileHandle->storageSpecifier.loginName,sshServer.loginName);
          if (storageFileHandle->storageSpecifier.hostPort == 0) storageFileHandle->storageSpecifier.hostPort = sshServer.port;
          storageFileHandle->sftp.sshPublicKeyFileName  = sshServer.publicKeyFileName;
          storageFileHandle->sftp.sshPrivateKeyFileName = sshServer.privateKeyFileName;

          // check if SSH login is possible
          error = ERROR_UNKNOWN;
          if ((error != ERROR_NONE) && (sshServer.password != NULL))
          {
            error = checkSSHLogin(storageFileHandle->storageSpecifier.loginName,
                                  sshServer.password,
                                  storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->sftp.sshPublicKeyFileName,
                                  storageFileHandle->sftp.sshPrivateKeyFileName
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(storageFileHandle->storageSpecifier.loginPassword,sshServer.password);
            }
          }
          if (error != ERROR_NONE)
          {
            // initialize default password
            if (initSSHPassword(storageFileHandle->storageSpecifier.hostName,storageFileHandle->storageSpecifier.loginName,jobOptions))
            {
              error = checkSSHLogin(storageFileHandle->storageSpecifier.loginName,
                                    defaultSSHPassword,
                                    storageFileHandle->storageSpecifier.hostName,
                                    storageFileHandle->storageSpecifier.hostPort,
                                    storageFileHandle->sftp.sshPublicKeyFileName,
                                    storageFileHandle->sftp.sshPrivateKeyFileName
                                   );
              if (error == ERROR_NONE)
              {
                Password_set(storageFileHandle->storageSpecifier.loginPassword,defaultSSHPassword);
              }
            }
            else
            {
              error = !Password_empty(defaultSSHPassword) ? ERROR_INVALID_SSH_PASSWORD : ERROR_NO_SSH_PASSWORD;
            }
          }
          if (error != ERROR_NONE)
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return error;
          }

          // free resources
        }
      #else /* not HAVE_SSH2 */
        Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      {
        OpticalDisk    opticalDisk;
        uint64         volumeSize,maxMediumSize;
        FileSystemInfo fileSystemInfo;
        Errors         error;
        String         sourceFileName,fileBaseName,destinationFileName;

        // get device name
        if (String_isEmpty(storageFileHandle->storageSpecifier.deviceName))
        {
          switch (storageFileHandle->storageSpecifier.type)
          {
            case STORAGE_TYPE_CD:
              String_set(storageFileHandle->storageSpecifier.deviceName,globalOptions.cd.defaultDeviceName);
              break;
            case STORAGE_TYPE_DVD:
              String_set(storageFileHandle->storageSpecifier.deviceName,globalOptions.dvd.defaultDeviceName);
              break;
            case STORAGE_TYPE_BD:
              String_set(storageFileHandle->storageSpecifier.deviceName,globalOptions.bd.defaultDeviceName);
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
               #endif /* NDEBUG */
              break;
          }
        }

        // get cd/dvd/bd settings
        switch (storageFileHandle->storageSpecifier.type)
        {
          case STORAGE_TYPE_CD:
            getCDSettings(jobOptions,&opticalDisk);
            break;
          case STORAGE_TYPE_DVD:
            getDVDSettings(jobOptions,&opticalDisk);
            break;
          case STORAGE_TYPE_BD:
            getBDSettings(jobOptions,&opticalDisk);
            break;
          default:
            #ifndef NDEBUG
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            #endif /* NDEBUG */
            break;
        }

        /* check space in temporary directory: should be enough to hold
           raw cd/dvd/bd data (volumeSize) and cd/dvd image (4G, single layer)
           including error correction codes (2x)
        */
        error = File_getFileSystemInfo(&fileSystemInfo,tmpDirectory);
        if (error != ERROR_NONE)
        {
          Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
          return error;
        }
        volumeSize    = 0LL;
        maxMediumSize = 0LL;
        switch (storageFileHandle->storageSpecifier.type)
        {
          case STORAGE_TYPE_CD:
            volumeSize = (jobOptions->volumeSize > 0LL)
                          ?jobOptions->volumeSize
                          :((globalOptions.cd.volumeSize > 0LL)
                            ?globalOptions.cd.volumeSize
                            :(jobOptions->errorCorrectionCodesFlag
                              ?CD_VOLUME_ECC_SIZE
                              :CD_VOLUME_SIZE
                             )
                           );
            maxMediumSize = MAX_CD_SIZE;
            break;
          case STORAGE_TYPE_DVD:
            volumeSize = (jobOptions->volumeSize > 0LL)
                          ?jobOptions->volumeSize
                          :((globalOptions.dvd.volumeSize > 0LL)
                            ?globalOptions.dvd.volumeSize
                            :(jobOptions->errorCorrectionCodesFlag
                              ?DVD_VOLUME_ECC_SIZE
                              :DVD_VOLUME_SIZE
                             )
                           );
            maxMediumSize = MAX_DVD_SIZE;
            break;
          case STORAGE_TYPE_BD:
            volumeSize = (jobOptions->volumeSize > 0LL)
                          ?jobOptions->volumeSize
                          :((globalOptions.bd.volumeSize > 0LL)
                            ?globalOptions.bd.volumeSize
                            :(jobOptions->errorCorrectionCodesFlag
                              ?BD_VOLUME_ECC_SIZE
                              :BD_VOLUME_SIZE
                            )
                           );
            maxMediumSize = MAX_BD_SIZE;
            break;
          default:
            #ifndef NDEBUG
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            #endif /* NDEBUG */
            break;
        }
        if (fileSystemInfo.freeBytes < (volumeSize+maxMediumSize*(jobOptions->errorCorrectionCodesFlag?2:1)))
        {
          printWarning("Insufficient space in temporary directory '%s' for medium (%.1f%s free, %.1f%s recommended)!\n",
                       String_cString(tmpDirectory),
                       BYTES_SHORT(fileSystemInfo.freeBytes),BYTES_UNIT(fileSystemInfo.freeBytes),
                       BYTES_SHORT((volumeSize+maxMediumSize*(jobOptions->errorCorrectionCodesFlag?2:1))),BYTES_UNIT((volumeSize+maxMediumSize*(jobOptions->errorCorrectionCodesFlag?2:1)))
                      );
        }

        // init variables
        storageFileHandle->type                                     = storageFileHandle->storageSpecifier.type;

        #ifdef HAVE_ISO9660
          storageFileHandle->opticalDisk.read.buffer.data = (byte*)malloc(ISO_BLOCKSIZE);
          if (storageFileHandle->opticalDisk.read.buffer.data == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
        #endif /* HAVE_ISO9660 */

        storageFileHandle->opticalDisk.write.requestVolumeCommand   = opticalDisk.requestVolumeCommand;
        storageFileHandle->opticalDisk.write.unloadVolumeCommand    = opticalDisk.unloadVolumeCommand;
        storageFileHandle->opticalDisk.write.loadVolumeCommand      = opticalDisk.loadVolumeCommand;
        storageFileHandle->opticalDisk.write.volumeSize             = volumeSize;
        storageFileHandle->opticalDisk.write.imagePreProcessCommand = opticalDisk.imagePreProcessCommand;
        storageFileHandle->opticalDisk.write.imagePostProcessCommand= opticalDisk.imagePostProcessCommand;
        storageFileHandle->opticalDisk.write.imageCommand           = opticalDisk.imageCommand;
        storageFileHandle->opticalDisk.write.eccPreProcessCommand   = opticalDisk.eccPreProcessCommand;
        storageFileHandle->opticalDisk.write.eccPostProcessCommand  = opticalDisk.eccPostProcessCommand;
        storageFileHandle->opticalDisk.write.eccCommand             = opticalDisk.eccCommand;
        storageFileHandle->opticalDisk.write.writePreProcessCommand = opticalDisk.writePreProcessCommand;
        storageFileHandle->opticalDisk.write.writePostProcessCommand= opticalDisk.writePostProcessCommand;
        storageFileHandle->opticalDisk.write.writeCommand           = opticalDisk.writeCommand;
        storageFileHandle->opticalDisk.write.writeImageCommand      = opticalDisk.writeImageCommand;
        storageFileHandle->opticalDisk.write.steps                  = jobOptions->errorCorrectionCodesFlag?4:1;
        storageFileHandle->opticalDisk.write.directory              = String_new();
        storageFileHandle->opticalDisk.write.step                   = 0;
        if (jobOptions->waitFirstVolumeFlag)
        {
          storageFileHandle->opticalDisk.write.number  = 0;
          storageFileHandle->opticalDisk.write.newFlag = TRUE;
        }
        else
        {
          storageFileHandle->opticalDisk.write.number  = 1;
          storageFileHandle->opticalDisk.write.newFlag = FALSE;
        }
        StringList_init(&storageFileHandle->opticalDisk.write.fileNameList);
        storageFileHandle->opticalDisk.write.fileName               = String_new();
        storageFileHandle->opticalDisk.write.totalSize              = 0LL;

        // create temporary directory for medium files
        error = File_getTmpDirectoryName(storageFileHandle->opticalDisk.write.directory,NULL,tmpDirectory);
        if (error != ERROR_NONE)
        {
          #ifdef HAVE_ISO9660
            free(storageFileHandle->opticalDisk.read.buffer.data);
          #endif /* HAVE_ISO9660 */
          String_delete(storageFileHandle->opticalDisk.write.fileName);
          String_delete(storageFileHandle->opticalDisk.write.directory);
          Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
          return error;
        }

        if (!jobOptions->noBAROnMediumFlag)
        {
          // store a copy of BAR executable on medium (ignore errors)
          sourceFileName = String_newCString(globalOptions.barExecutable);
          fileBaseName = File_getFileBaseName(String_new(),sourceFileName);
          destinationFileName = File_appendFileName(String_duplicate(storageFileHandle->opticalDisk.write.directory),fileBaseName);
          File_copy(sourceFileName,destinationFileName);
          StringList_append(&storageFileHandle->opticalDisk.write.fileNameList,destinationFileName);
          String_delete(destinationFileName);
          String_delete(fileBaseName);
          String_delete(sourceFileName);
        }

        // request first medium
        storageFileHandle->requestedVolumeNumber = 1;
      }
      break;
    case STORAGE_TYPE_DEVICE:
      {
        Device         device;
        FileSystemInfo fileSystemInfo;
        Errors         error;

        // get device settings
        getDeviceSettings(storageFileHandle->storageSpecifier.deviceName,jobOptions,&device);

        // check space in temporary directory: 2x volumeSize
        error = File_getFileSystemInfo(&fileSystemInfo,tmpDirectory);
        if (error != ERROR_NONE)
        {
          Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
          return error;
        }
        if (fileSystemInfo.freeBytes < (device.volumeSize*2))
        {
          printWarning("Insufficient space in temporary directory '%s' (%.1f%s free, %.1f%s recommended)!\n",
                       String_cString(tmpDirectory),
                       BYTES_SHORT(fileSystemInfo.freeBytes),BYTES_UNIT(fileSystemInfo.freeBytes),
                       BYTES_SHORT(device.volumeSize*2),BYTES_UNIT(device.volumeSize*2)
                      );
        }

        // init variables
        storageFileHandle->type                          = STORAGE_TYPE_DEVICE;
        storageFileHandle->device.requestVolumeCommand   = device.requestVolumeCommand;
        storageFileHandle->device.unloadVolumeCommand    = device.unloadVolumeCommand;
        storageFileHandle->device.loadVolumeCommand      = device.loadVolumeCommand;
        storageFileHandle->device.volumeSize             = device.volumeSize;
        storageFileHandle->device.imagePreProcessCommand = device.imagePreProcessCommand;
        storageFileHandle->device.imagePostProcessCommand= device.imagePostProcessCommand;
        storageFileHandle->device.imageCommand           = device.imageCommand;
        storageFileHandle->device.eccPreProcessCommand   = device.eccPreProcessCommand;
        storageFileHandle->device.eccPostProcessCommand  = device.eccPostProcessCommand;
        storageFileHandle->device.eccCommand             = device.eccCommand;
        storageFileHandle->device.writePreProcessCommand = device.writePreProcessCommand;
        storageFileHandle->device.writePostProcessCommand= device.writePostProcessCommand;
        storageFileHandle->device.writeCommand           = device.writeCommand;
        storageFileHandle->device.directory              = String_new();
        if (jobOptions->waitFirstVolumeFlag)
        {
          storageFileHandle->device.number  = 0;
          storageFileHandle->device.newFlag = TRUE;
        }
        else
        {
          storageFileHandle->device.number  = 1;
          storageFileHandle->device.newFlag = FALSE;
        }
        StringList_init(&storageFileHandle->device.fileNameList);
        storageFileHandle->device.fileName  = String_new();
        storageFileHandle->device.totalSize = 0LL;

        // create temporary directory for device files
        error = File_getTmpDirectoryName(storageFileHandle->device.directory,NULL,tmpDirectory);
        if (error != ERROR_NONE)
        {
          String_delete(storageFileHandle->device.fileName);
          String_delete(storageFileHandle->device.directory);
          Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
          return error;
        }

        // request first volume for device
        storageFileHandle->requestedVolumeNumber = 1;
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  storageFileHandle->runningInfo.volumeNumber   = 0;
  storageFileHandle->runningInfo.volumeProgress = 0;

  return ERROR_NONE;
}

Errors Storage_done(StorageFileHandle *storageFileHandle)
{
  Errors error;

  assert(storageFileHandle != NULL);

  error = ERROR_NONE;

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        free(storageFileHandle->ftp.readAheadBuffer.data);
      #else /* not HAVE_FTP */
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        free(storageFileHandle->scp.readAheadBuffer.data);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        free(storageFileHandle->sftp.readAheadBuffer.data);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      {
        String fileName;
        Errors tmpError;

        // delete files
        fileName = String_new();
        while (!StringList_isEmpty(&storageFileHandle->opticalDisk.write.fileNameList))
        {
          StringList_getFirst(&storageFileHandle->opticalDisk.write.fileNameList,fileName);
          tmpError = File_delete(fileName,FALSE);
          if (tmpError != ERROR_NONE)
          {
            if (error == ERROR_NONE) error = tmpError;
          }
        }
        String_delete(fileName);

        // delete temporare directory
        File_delete(storageFileHandle->opticalDisk.write.directory,FALSE);

        // free resources
        #ifdef HAVE_ISO9660
          free(storageFileHandle->opticalDisk.read.buffer.data);
        #endif /* HAVE_ISO9660 */
        String_delete(storageFileHandle->opticalDisk.write.fileName);
        String_delete(storageFileHandle->opticalDisk.write.directory);
      }
      break;
    case STORAGE_TYPE_DEVICE:
      {
        String fileName;
        Errors tmpError;

        // delete files
        fileName = String_new();
        while (!StringList_isEmpty(&storageFileHandle->device.fileNameList))
        {
          StringList_getFirst(&storageFileHandle->device.fileNameList,fileName);
          tmpError = File_delete(fileName,FALSE);
          if (tmpError != ERROR_NONE)
          {
            if (error == ERROR_NONE) error = tmpError;
          }
        }
        String_delete(fileName);

        // delete temporare directory
        File_delete(storageFileHandle->device.directory,FALSE);

        // free resources
        String_delete(storageFileHandle->device.fileName);
        String_delete(storageFileHandle->device.directory);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  Storage_doneSpecifier(&storageFileHandle->storageSpecifier);

  return error;
}

String Storage_getHandleName(String                  storageName,
                             const StorageFileHandle *storageFileHandle,
                             const String            fileName
                            )
{
  String storageSpecifier;

  assert(storageName != NULL);
  assert(storageFileHandle != NULL);

  // initialize variables
  storageSpecifier = String_new();

  // format specifier
  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        if (!String_isEmpty(storageFileHandle->storageSpecifier.hostName))
        {
          if (!String_isEmpty(storageFileHandle->storageSpecifier.loginName))
          {
            String_format(storageSpecifier,"%S@",storageFileHandle->storageSpecifier.loginName);
          }
          String_format(storageSpecifier,"%S",storageFileHandle->storageSpecifier.hostName);
          if (storageFileHandle->storageSpecifier.hostPort != 0)
          {
            String_format(storageSpecifier,":%d",storageFileHandle->storageSpecifier.hostPort);
          }
        }
      #else /* not HAVE_FTP */
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        if (!String_isEmpty(storageFileHandle->storageSpecifier.hostName))
        {
          if (!String_isEmpty(storageFileHandle->storageSpecifier.loginName))
          {
            String_format(storageSpecifier,"%S@",storageFileHandle->storageSpecifier.loginName);
          }
          String_append(storageSpecifier,storageFileHandle->storageSpecifier.hostName);
          if (storageFileHandle->storageSpecifier.hostPort != 0)
          {
            String_format(storageSpecifier,":%d",storageFileHandle->storageSpecifier.hostPort);
          }
        }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        if (!String_isEmpty(storageFileHandle->storageSpecifier.hostName))
        {
          if (!String_isEmpty(storageFileHandle->storageSpecifier.loginName))
          {
            String_format(storageSpecifier,"%S@",storageFileHandle->storageSpecifier.loginName);
          }
          String_append(storageSpecifier,storageFileHandle->storageSpecifier.hostName);
          if (storageFileHandle->storageSpecifier.hostPort != 0)
          {
            String_format(storageSpecifier,":%d",storageFileHandle->storageSpecifier.hostPort);
          }
        }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_CD:
      if (!String_isEmpty(storageFileHandle->storageSpecifier.deviceName))
      {
        String_append(storageSpecifier,storageFileHandle->storageSpecifier.deviceName);
        String_appendChar(storageSpecifier,':');
      }
      break;
    case STORAGE_TYPE_DVD:
      if (!String_isEmpty(storageFileHandle->storageSpecifier.deviceName))
      {
        String_append(storageSpecifier,storageFileHandle->storageSpecifier.deviceName);
        String_appendChar(storageSpecifier,':');
      }
      break;
    case STORAGE_TYPE_BD:
      if (!String_isEmpty(storageFileHandle->storageSpecifier.deviceName))
      {
        String_append(storageSpecifier,storageFileHandle->storageSpecifier.deviceName);
        String_appendChar(storageSpecifier,':');
      }
      break;
    case STORAGE_TYPE_DEVICE:
      if (!String_isEmpty(storageFileHandle->storageSpecifier.deviceName))
      {
        String_append(storageSpecifier,storageFileHandle->storageSpecifier.deviceName);
        String_appendChar(storageSpecifier,':');
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  // get name
  Storage_getName(storageName,storageFileHandle->type,storageSpecifier,fileName);

  // free resources
  String_delete(storageSpecifier);

  return storageName;
}

Errors Storage_preProcess(StorageFileHandle *storageFileHandle,
                          bool              initialFlag
                         )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);

  error = ERROR_NONE;
  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      {
        TextMacro textMacros[2];

        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          if (!initialFlag)
          {
            // init macros
            TEXT_MACRO_N_STRING (textMacros[0],"%file",  storageFileHandle->fileSystem.fileHandle.name);
            TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageFileHandle->volumeNumber              );

            if (globalOptions.file.writePreProcessCommand != NULL)
            {
              // write pre-processing
              if (error == ERROR_NONE)
              {
                printInfo(0,"Write pre-processing...");
                error = Misc_executeCommand(String_cString(globalOptions.file.writePreProcessCommand),
                                            textMacros,
                                            SIZE_OF_ARRAY(textMacros),
                                            (ExecuteIOFunction)executeIOOutput,
                                            (ExecuteIOFunction)executeIOOutput,
                                            NULL
                                           );
                printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
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
      #ifdef HAVE_FTP
        {
          TextMacro textMacros[1];

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            if (!initialFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageFileHandle->volumeNumber              );

              if (globalOptions.ftp.writePreProcessCommand != NULL)
              {
                // write pre-processing
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write pre-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.ftp.writePreProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              (ExecuteIOFunction)executeIOOutput,
                                              (ExecuteIOFunction)executeIOOutput,
                                              NULL
                                             );
                  printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
                }
              }
              if (error != ERROR_NONE)
              {
                break;
              }
            }
          }
        }
      #else /* not HAVE_FTP */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          TextMacro textMacros[1];

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            if (!initialFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageFileHandle->volumeNumber              );

              if (globalOptions.scp.writePreProcessCommand != NULL)
              {
                // write pre-processing
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write pre-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.scp.writePreProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              (ExecuteIOFunction)executeIOOutput,
                                              (ExecuteIOFunction)executeIOOutput,
                                              NULL
                                             );
                  printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
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
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          TextMacro textMacros[1];

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            if (!initialFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageFileHandle->volumeNumber              );

              if (globalOptions.sftp.writePreProcessCommand != NULL)
              {
                // write pre-processing
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write pre-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.sftp.writePreProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              (ExecuteIOFunction)executeIOOutput,
                                              (ExecuteIOFunction)executeIOOutput,
                                              NULL
                                             );
                  printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
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
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        // request next medium
        if (storageFileHandle->opticalDisk.write.newFlag)
        {
          storageFileHandle->opticalDisk.write.number++;
          storageFileHandle->opticalDisk.write.newFlag = FALSE;

          storageFileHandle->requestedVolumeNumber = storageFileHandle->opticalDisk.write.number;
        }

        // check if new medium is required
        if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
        {
          // request load new medium
          error = requestNewMedium(storageFileHandle,FALSE);
        }
      }
      break;
    case STORAGE_TYPE_DEVICE:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        // request next volume
        if (storageFileHandle->device.newFlag)
        {
          storageFileHandle->device.number++;
          storageFileHandle->device.newFlag = FALSE;

          storageFileHandle->requestedVolumeNumber = storageFileHandle->device.number;
        }

        // check if new volume is required
        if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
        {
          error = requestNewVolume(storageFileHandle,FALSE);
        }
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

Errors Storage_postProcess(StorageFileHandle *storageFileHandle,
                           bool              finalFlag
                          )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);

  error = ERROR_NONE;
  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      {
        TextMacro textMacros[2];

        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          if (!finalFlag)
          {
            // init macros
            TEXT_MACRO_N_STRING (textMacros[0],"%file",  storageFileHandle->fileSystem.fileHandle.name);
            TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageFileHandle->volumeNumber              );

            if (globalOptions.file.writePostProcessCommand != NULL)
            {
              // write post-process
              if (error == ERROR_NONE)
              {
                printInfo(0,"Write post-processing...");
                error = Misc_executeCommand(String_cString(globalOptions.file.writePostProcessCommand),
                                            textMacros,
                                            SIZE_OF_ARRAY(textMacros),
                                            (ExecuteIOFunction)executeIOOutput,
                                            (ExecuteIOFunction)executeIOOutput,
                                            NULL
                                           );
                printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
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
      #ifdef HAVE_FTP
        {
          TextMacro textMacros[1];

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            if (!finalFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageFileHandle->volumeNumber              );

              if (globalOptions.ftp.writePostProcessCommand != NULL)
              {
                // write post-process
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write post-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.ftp.writePostProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              (ExecuteIOFunction)executeIOOutput,
                                              (ExecuteIOFunction)executeIOOutput,
                                              NULL
                                             );
                  printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
                }
              }
              if (error != ERROR_NONE)
              {
                break;
              }
            }
          }
        }
      #else /* not HAVE_FTP */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          TextMacro textMacros[1];

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            if (!finalFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageFileHandle->volumeNumber              );

              if (globalOptions.scp.writePostProcessCommand != NULL)
              {
                // write post-process
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write post-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.scp.writePostProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              (ExecuteIOFunction)executeIOOutput,
                                              (ExecuteIOFunction)executeIOOutput,
                                              NULL
                                             );
                  printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
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
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          TextMacro textMacros[1];

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            if (!finalFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageFileHandle->volumeNumber              );

              if (globalOptions.sftp.writePostProcessCommand != NULL)
              {
                // write post-process
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write post-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.sftp.writePostProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              (ExecuteIOFunction)executeIOOutput,
                                              (ExecuteIOFunction)executeIOOutput,
                                              NULL
                                             );
                  printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
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
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      {
        String    imageFileName;
        TextMacro textMacros[5];
        String    fileName;
        FileInfo  fileInfo;
        bool      retryFlag;

        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          if (finalFlag || (storageFileHandle->opticalDisk.write.totalSize > storageFileHandle->opticalDisk.write.volumeSize))
          {
            // medium size limit reached -> create medium and request new volume

            // update info
            storageFileHandle->runningInfo.volumeProgress = 0.0;
            updateStatusInfo(storageFileHandle);

            // get temporary image file name
            imageFileName = String_new();
            error = File_getTmpFileName(imageFileName,NULL,tmpDirectory);
            if (error != ERROR_NONE)
            {
              break;
            }

            // init macros
            TEXT_MACRO_N_STRING (textMacros[0],"%device",   storageFileHandle->storageSpecifier.deviceName);
            TEXT_MACRO_N_STRING (textMacros[1],"%directory",storageFileHandle->opticalDisk.write.directory);
            TEXT_MACRO_N_STRING (textMacros[2],"%image",    imageFileName                                 );
            TEXT_MACRO_N_INTEGER(textMacros[3],"%sectors",  0                                             );
            TEXT_MACRO_N_INTEGER(textMacros[4],"%number",   storageFileHandle->volumeNumber               );

            if (storageFileHandle->jobOptions->alwaysCreateImageFlag || storageFileHandle->jobOptions->errorCorrectionCodesFlag)
            {
              // create medium image
              printInfo(0,"Make medium image #%d with %d file(s)...",storageFileHandle->opticalDisk.write.number,StringList_count(&storageFileHandle->opticalDisk.write.fileNameList));
              storageFileHandle->opticalDisk.write.step = 0;
              error = Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.imageCommand),
                                          textMacros,SIZE_OF_ARRAY(textMacros),
                                          (ExecuteIOFunction)executeIOmkisofs,
                                          (ExecuteIOFunction)executeIOmkisofs,
                                          storageFileHandle
                                         );
              if (error != ERROR_NONE)
              {
                printInfo(0,"FAIL\n");
                File_delete(imageFileName,FALSE);
                String_delete(imageFileName);
                break;
              }
              File_getFileInfo(&fileInfo,imageFileName);
              printInfo(0,"ok (%llu bytes)\n",fileInfo.size);

              if (storageFileHandle->jobOptions->errorCorrectionCodesFlag)
              {
                // add error-correction codes to medium image
                printInfo(0,"Add ECC to image #%d...",storageFileHandle->opticalDisk.write.number);
                storageFileHandle->opticalDisk.write.step = 1;
                error = Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.eccCommand),
                                            textMacros,SIZE_OF_ARRAY(textMacros),
                                            (ExecuteIOFunction)executeIOdvdisaster,
                                            (ExecuteIOFunction)executeIOdvdisaster,
                                            storageFileHandle
                                           );
                if (error != ERROR_NONE)
                {
                  printInfo(0,"FAIL\n");
                  File_delete(imageFileName,FALSE);
                  String_delete(imageFileName);
                  break;
                }
                File_getFileInfo(&fileInfo,imageFileName);
                printInfo(0,"ok (%llu bytes)\n",fileInfo.size);
              }

              // get number of image sectors
              if (File_getFileInfo(&fileInfo,imageFileName) == ERROR_NONE)
              {
                TEXT_MACRO_N_INTEGER(textMacros[3],"%sectors",(ulong)(fileInfo.size/2048LL));
              }

              // check if new medium is required
              if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
              {
                // request load new medium
                error = requestNewMedium(storageFileHandle,TRUE);
                if (error != ERROR_NONE)
                {
                  File_delete(imageFileName,FALSE);
                  String_delete(imageFileName);
                  break;
                }
                updateStatusInfo(storageFileHandle);
              }

              retryFlag = TRUE;
              do
              {
                retryFlag = FALSE;

                // write image to medium
                printInfo(0,"Write image to medium #%d...",storageFileHandle->opticalDisk.write.number);
                storageFileHandle->opticalDisk.write.step = 3;
                error = Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.writeImageCommand),
                                            textMacros,SIZE_OF_ARRAY(textMacros),
                                            (ExecuteIOFunction)executeIOgrowisofs,
                                            (ExecuteIOFunction)executeIOgrowisofs,
                                            storageFileHandle
                                           );
                if (error == ERROR_NONE)
                {
                  printInfo(0,"ok\n");
                  retryFlag = FALSE;
                }
                else
                {
                  printInfo(0,"FAIL\n");
                  retryFlag = Misc_getYesNo("Retry write image to medium?");
                }
              }
              while ((error != ERROR_NONE) && retryFlag);
              if (error != ERROR_NONE)
              {
                File_delete(imageFileName,FALSE);
                String_delete(imageFileName);
                break;
              }
            }
            else
            {
              // check if new medium is required
              if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
              {
                // request load new medium
                error = requestNewMedium(storageFileHandle,TRUE);
                if (error != ERROR_NONE)
                {
                  File_delete(imageFileName,FALSE);
                  String_delete(imageFileName);
                  break;
                }
                updateStatusInfo(storageFileHandle);
              }

              retryFlag = TRUE;
              do
              {
                retryFlag = FALSE;

                // write to medium
                printInfo(0,"Write medium #%d with %d file(s)...",storageFileHandle->opticalDisk.write.number,StringList_count(&storageFileHandle->opticalDisk.write.fileNameList));
                storageFileHandle->opticalDisk.write.step = 0;
                error = Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.writeCommand),
                                            textMacros,SIZE_OF_ARRAY(textMacros),
                                            (ExecuteIOFunction)executeIOgrowisofs,
                                            (ExecuteIOFunction)executeIOOutput,
                                            storageFileHandle
                                           );
                if (error == ERROR_NONE)
                {
                  printInfo(0,"ok\n");
                }
                else
                {
                  printInfo(0,"FAIL (error: %s)\n",Errors_getText(error));
                  retryFlag = Misc_getYesNo("Retry write image to medium?");
                }
              }
              while ((error != ERROR_NONE) && retryFlag);
              if (error != ERROR_NONE)
              {
                File_delete(imageFileName,FALSE);
                String_delete(imageFileName);
                break;
              }
            }

            // delete image
            File_delete(imageFileName,FALSE);
            String_delete(imageFileName);

            // update info
            storageFileHandle->runningInfo.volumeProgress = 1.0;
            updateStatusInfo(storageFileHandle);

            // delete stored files
            fileName = String_new();
            while (!StringList_isEmpty(&storageFileHandle->opticalDisk.write.fileNameList))
            {
              StringList_getFirst(&storageFileHandle->opticalDisk.write.fileNameList,fileName);
              error = File_delete(fileName,FALSE);
              if (error != ERROR_NONE)
              {
                break;
              }
            }
            String_delete(fileName);
            if (error != ERROR_NONE)
            {
              break;
            }

            // reset
            storageFileHandle->opticalDisk.write.newFlag   = TRUE;
            storageFileHandle->opticalDisk.write.totalSize = 0;
          }
        }
        else
        {
          // update info
          storageFileHandle->opticalDisk.write.step = 3;
          storageFileHandle->runningInfo.volumeProgress = 1.0;
          updateStatusInfo(storageFileHandle);
        }
      }
      break;
    case STORAGE_TYPE_DEVICE:
      {
        String    imageFileName;
        TextMacro textMacros[4];
        String    fileName;

        if (storageFileHandle->device.volumeSize == 0LL)
        {
          printWarning("Device volume size is 0 bytes!\n");
        }

        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          if (finalFlag || (storageFileHandle->device.totalSize > storageFileHandle->device.volumeSize))
          {
            // device size limit reached -> write to device volume and request new volume

            // check if new volume is required
            if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
            {
              error = requestNewVolume(storageFileHandle,TRUE);
              if (error != ERROR_NONE)
              {
                break;
              }
              updateStatusInfo(storageFileHandle);
            }

            // get temporary image file name
            imageFileName = String_new();
            error = File_getTmpFileName(imageFileName,NULL,tmpDirectory);
            if (error != ERROR_NONE)
            {
              break;
            }

            // init macros
            TEXT_MACRO_N_STRING (textMacros[0],"%device",   storageFileHandle->storageSpecifier.deviceName);
            TEXT_MACRO_N_STRING (textMacros[1],"%directory",storageFileHandle->device.directory           );
            TEXT_MACRO_N_STRING (textMacros[2],"%image",    imageFileName                                 );
            TEXT_MACRO_N_INTEGER(textMacros[3],"%number",   storageFileHandle->volumeNumber               );

            // create image
            if (error == ERROR_NONE)
            {
              printInfo(0,"Make image pre-processing of volume #%d...",storageFileHandle->volumeNumber);
              error = Misc_executeCommand(String_cString(storageFileHandle->device.imagePreProcessCommand ),
                                          textMacros,
                                          SIZE_OF_ARRAY(textMacros),
                                          (ExecuteIOFunction)executeIOOutput,
                                          (ExecuteIOFunction)executeIOOutput,
                                          NULL
                                         );
              printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
            }
            if (error == ERROR_NONE)
            {
              printInfo(0,"Make image volume #%d...",storageFileHandle->volumeNumber);
              error = Misc_executeCommand(String_cString(storageFileHandle->device.imageCommand),
                                          textMacros,
                                          SIZE_OF_ARRAY(textMacros),
                                          (ExecuteIOFunction)executeIOOutput,
                                          (ExecuteIOFunction)executeIOOutput,
                                          NULL
                                         );
              printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
            }
            if (error == ERROR_NONE)
            {
              printInfo(0,"Make image post-processing of volume #%d...",storageFileHandle->volumeNumber);
              error = Misc_executeCommand(String_cString(storageFileHandle->device.imagePostProcessCommand),
                                          textMacros,
                                          SIZE_OF_ARRAY(textMacros),
                                          (ExecuteIOFunction)executeIOOutput,
                                          (ExecuteIOFunction)executeIOOutput,
                                          NULL
                                         );
              printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
            }

            // write to device
            if (error == ERROR_NONE)
            {
              printInfo(0,"Write device pre-processing of volume #%d...",storageFileHandle->volumeNumber);
              error = Misc_executeCommand(String_cString(storageFileHandle->device.writePreProcessCommand),
                                          textMacros,
                                          SIZE_OF_ARRAY(textMacros),
                                          (ExecuteIOFunction)executeIOOutput,
                                          (ExecuteIOFunction)executeIOOutput,
                                          NULL
                                         );
              printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
            }
            if (error == ERROR_NONE)
            {
              printInfo(0,"Write device volume #%d...",storageFileHandle->volumeNumber);
              error = Misc_executeCommand(String_cString(storageFileHandle->device.writeCommand),
                                          textMacros,
                                          SIZE_OF_ARRAY(textMacros),
                                          (ExecuteIOFunction)executeIOOutput,
                                          (ExecuteIOFunction)executeIOOutput,
                                          NULL
                                         );
              printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
            }
            if (error == ERROR_NONE)
            {
              printInfo(0,"Write device post-processing of volume #%d...",storageFileHandle->volumeNumber);
              error = Misc_executeCommand(String_cString(storageFileHandle->device.writePostProcessCommand),
                                          textMacros,
                                          SIZE_OF_ARRAY(textMacros),
                                          (ExecuteIOFunction)executeIOOutput,
                                          (ExecuteIOFunction)executeIOOutput,
                                          NULL
                                         );
              printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
            }

            if (error != ERROR_NONE)
            {
              File_delete(imageFileName,FALSE);
              String_delete(imageFileName);
              break;
            }
            File_delete(imageFileName,FALSE);
            String_delete(imageFileName);

            // delete stored files
            fileName = String_new();
            while (!StringList_isEmpty(&storageFileHandle->device.fileNameList))
            {
              StringList_getFirst(&storageFileHandle->device.fileNameList,fileName);
              error = File_delete(fileName,FALSE);
              if (error != ERROR_NONE)
              {
                break;
              }
            }
            String_delete(fileName);
            if (error != ERROR_NONE)
            {
              break;
            }

            // reset
            storageFileHandle->device.newFlag   = TRUE;
            storageFileHandle->device.totalSize = 0;
          }
        }
        else
        {
          // update info
          storageFileHandle->runningInfo.volumeProgress = 1.0;
          updateStatusInfo(storageFileHandle);
        }
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
       #endif /* NDEBUG */
      break;
  }

  return error;
}

uint Storage_getVolumeNumber(const StorageFileHandle *storageFileHandle)
{
  assert(storageFileHandle != NULL);

  return storageFileHandle->volumeNumber;
}

void Storage_setVolumeNumber(StorageFileHandle *storageFileHandle,
                             uint              volumeNumber
                            )
{
  assert(storageFileHandle != NULL);

  storageFileHandle->volumeNumber = volumeNumber;
}

Errors Storage_unloadVolume(StorageFileHandle *storageFileHandle)
{
  Errors error;

  assert(storageFileHandle != NULL);

  error = ERROR_UNKNOWN;
  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_NONE:
    case STORAGE_TYPE_FILESYSTEM:
    case STORAGE_TYPE_FTP:
    case STORAGE_TYPE_SSH:
    case STORAGE_TYPE_SCP:
    case STORAGE_TYPE_SFTP:
      error = ERROR_NONE;
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      {
        TextMacro textMacros[1];

        TEXT_MACRO_N_STRING(textMacros[0],"%device",storageFileHandle->storageSpecifier.deviceName);
        error = Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.unloadVolumeCommand),
                                    textMacros,SIZE_OF_ARRAY(textMacros),
                                    (ExecuteIOFunction)executeIOOutput,
                                    (ExecuteIOFunction)executeIOOutput,
                                    NULL
                                   );
      }
      break;
    case STORAGE_TYPE_DEVICE:
      {
        TextMacro textMacros[1];

        TEXT_MACRO_N_STRING(textMacros[0],"%device",storageFileHandle->storageSpecifier.deviceName);
        error = Misc_executeCommand(String_cString(storageFileHandle->device.unloadVolumeCommand),
                                    textMacros,SIZE_OF_ARRAY(textMacros),
                                    (ExecuteIOFunction)executeIOOutput,
                                    (ExecuteIOFunction)executeIOOutput,
                                    NULL
                                   );
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

Errors Storage_create(StorageFileHandle *storageFileHandle,
                      const String      fileName,
                      uint64            fileSize
                     )
{
  Errors error;
  String directoryName;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);
  assert(fileName != NULL);

  // init variables
  storageFileHandle->mode = STORAGE_MODE_WRITE;

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      // check if archive file exists
      if (!storageFileHandle->jobOptions->overwriteArchiveFilesFlag && File_exists(fileName))
      {
        return ERRORX(FILE_EXISTS,0,String_cString(fileName));
      }

      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        // create directory if not existing
        directoryName = File_getFilePathName(String_new(),fileName);
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
        error = File_open(&storageFileHandle->fileSystem.fileHandle,
                          fileName,
                          FILE_OPEN_CREATE
                         );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        {
          String          pathName;
          String          directoryName;
          StringTokenizer pathNameTokenizer;
          String          name;
          const char      *plainPassword;

          // initialise variables

          // connect
          if (!Network_hostExists(storageFileHandle->storageSpecifier.hostName))
          {
            return ERROR_HOST_NOT_FOUND;
          }
          if (FtpConnect(String_cString(storageFileHandle->storageSpecifier.hostName),&storageFileHandle->ftp.control) != 1)
          {
            return ERROR_FTP_SESSION_FAIL;
          }

          // login
          plainPassword = Password_deploy(storageFileHandle->storageSpecifier.loginPassword);
          if (FtpLogin(String_cString(storageFileHandle->storageSpecifier.loginName),
                       plainPassword,
                       storageFileHandle->ftp.control
                      ) != 1
             )
          {
            Password_undeploy(storageFileHandle->storageSpecifier.loginPassword);
            FtpClose(storageFileHandle->ftp.control);
            return ERROR_FTP_AUTHENTICATION;
          }
          Password_undeploy(storageFileHandle->storageSpecifier.loginPassword);

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            // create directory (try it and ignore errors)
            pathName      = File_getFilePathName(String_new(),fileName);
            directoryName = File_newFileName();
            File_initSplitFileName(&pathNameTokenizer,pathName);
            if (File_getNextSplitFileName(&pathNameTokenizer,&name))
            {
              // create root-directory
              if (!String_isEmpty(name))
              {
                File_setFileName(directoryName,name);
              }
              else
              {
                File_setFileNameChar(directoryName,FILES_PATHNAME_SEPARATOR_CHAR);
              }
              (void)FtpMkdir(String_cString(directoryName),storageFileHandle->ftp.control);

              // create sub-directories
              while (File_getNextSplitFileName(&pathNameTokenizer,&name))
              {
                if (!String_isEmpty(name))
                {
                  // get sub-directory
                  File_appendFileName(directoryName,name);

                  // create sub-directory
                  (void)FtpMkdir(String_cString(directoryName),storageFileHandle->ftp.control);
                }
              }
            }
            File_doneSplitFileName(&pathNameTokenizer);
            File_deleteFileName(directoryName);
            File_deleteFileName(pathName);

            // set timeout callback (120s) to avoid infinite waiting on read/write
            if (FtpOptions(FTPLIB_IDLETIME,
                           120*1000,
                           storageFileHandle->ftp.control
                          ) != 1
               )
            {
              FtpClose(storageFileHandle->ftp.control);
              return ERRORX(CREATE_FILE,0,"ftp access");
            }
            if (FtpOptions(FTPLIB_CALLBACK,
                           (long)ftpTimeoutCallback,
                           storageFileHandle->ftp.control
                          ) != 1
               )
            {
              FtpClose(storageFileHandle->ftp.control);
              return ERRORX(CREATE_FILE,0,"ftp access");
            }

            // create file: first try non-passive, then passive mode
            FtpOptions(FTPLIB_CONNMODE,FTPLIB_PORT,storageFileHandle->ftp.control);
            if (FtpAccess(String_cString(fileName),
                          FTPLIB_FILE_WRITE,
                          FTPLIB_IMAGE,
                          storageFileHandle->ftp.control,
                          &storageFileHandle->ftp.data
                         ) != 1
               )
            {
              FtpOptions(FTPLIB_CONNMODE,FTPLIB_PASSIVE,storageFileHandle->ftp.control);
              if (FtpAccess(String_cString(fileName),
                            FTPLIB_FILE_WRITE,
                            FTPLIB_IMAGE,
                            storageFileHandle->ftp.control,
                            &storageFileHandle->ftp.data
                           ) != 1
                 )
              {
                FtpClose(storageFileHandle->ftp.control);
                return ERRORX(CREATE_FILE,0,"ftp access");
              }
            }
          }
        }
      #else /* not HAVE_FTP */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          // connect
          error = Network_connect(&storageFileHandle->scp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  storageFileHandle->storageSpecifier.loginPassword,
                                  storageFileHandle->scp.sshPublicKeyFileName,
                                  storageFileHandle->scp.sshPrivateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }
          libssh2_session_set_timeout(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),READ_TIMEOUT);

          // install send/receive callback to track number of sent/received bytes
          storageFileHandle->scp.totalSentBytes     = 0LL;
          storageFileHandle->scp.totalReceivedBytes = 0LL;
          (*(libssh2_session_abstract(Network_getSSHSession(&storageFileHandle->scp.socketHandle)))) = storageFileHandle;
          storageFileHandle->scp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,sshSendCallback   );
          storageFileHandle->scp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,sshReceiveCallback);

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            // open channel and file for writing
            #ifdef HAVE_SSH2_SCP_SEND64
              storageFileHandle->scp.channel = libssh2_scp_send64(Network_getSSHSession(&storageFileHandle->scp.socketHandle),
                                                                  String_cString(fileName),
// ???
0600,
                                                                  (libssh2_uint64_t)fileSize,
// ???
                                                                  0L,
                                                                  0L
                                                                 );
            #else /* not HAVE_SSH2_SCP_SEND64 */
              storageFileHandle->scp.channel = libssh2_scp_send(Network_getSSHSession(&storageFileHandle->scp.socketHandle),
                                                                String_cString(fileName),
// ???
0600,
                                                                (size_t)fileSize
                                                               );
            #endif /* HAVE_SSH2_SCP_SEND64 */
            if (storageFileHandle->scp.channel == NULL)
            {
              char *sshErrorText;

              libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->scp.socketHandle),&sshErrorText,NULL,0);
              error = ERRORX(SSH,
                             libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->scp.socketHandle)),
                             sshErrorText
                            );
              Network_disconnect(&storageFileHandle->scp.socketHandle);
              return error;
            }
          }
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(fileSize);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          // connect
          error = Network_connect(&storageFileHandle->sftp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  storageFileHandle->storageSpecifier.loginPassword,
                                  storageFileHandle->sftp.sshPublicKeyFileName,
                                  storageFileHandle->sftp.sshPrivateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }
          libssh2_session_set_timeout(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),READ_TIMEOUT);

          // install send/receive callback to track number of sent/received bytes
          storageFileHandle->sftp.totalSentBytes     = 0LL;
          storageFileHandle->sftp.totalReceivedBytes = 0LL;
          (*(libssh2_session_abstract(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)))) = storageFileHandle;
          storageFileHandle->sftp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),LIBSSH2_CALLBACK_SEND,sftpSendCallback   );
          storageFileHandle->sftp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),LIBSSH2_CALLBACK_RECV,sftpReceiveCallback);

          // init session
          storageFileHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageFileHandle->sftp.socketHandle));
          if (storageFileHandle->sftp.sftp == NULL)
          {
            char *sshErrorText;

            libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),&sshErrorText,NULL,0);
            error = ERRORX(SSH,
                           libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)),
                           sshErrorText
                          );
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            return error;
          }

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            // create file
            storageFileHandle->sftp.sftpHandle = libssh2_sftp_open(storageFileHandle->sftp.sftp,
                                                                   String_cString(fileName),
                                                                   LIBSSH2_FXF_CREAT|LIBSSH2_FXF_WRITE|LIBSSH2_FXF_TRUNC,
// ???
LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR
                                                                  );
            if (storageFileHandle->sftp.sftpHandle == NULL)
            {
              char *sshErrorText;

              libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),&sshErrorText,NULL,0);
              error = ERRORX(SSH,
                             libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)),
                             sshErrorText
                            );
              libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
              Network_disconnect(&storageFileHandle->sftp.socketHandle);
              return error;
            }
          }
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(fileSize);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      // create file name
      String_set(storageFileHandle->opticalDisk.write.fileName,storageFileHandle->opticalDisk.write.directory);
      File_appendFileName(storageFileHandle->opticalDisk.write.fileName,fileName);

      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        // create directory if not existing
        directoryName = File_getFilePathName(String_new(),storageFileHandle->opticalDisk.write.fileName);
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

        // create file
        error = File_open(&storageFileHandle->opticalDisk.write.fileHandle,
                          storageFileHandle->opticalDisk.write.fileName,
                          FILE_OPEN_CREATE
                         );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      break;
    case STORAGE_TYPE_DEVICE:
      // create file name
      String_set(storageFileHandle->device.fileName,storageFileHandle->device.directory);
      File_appendFileName(storageFileHandle->device.fileName,fileName);

      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        // open file
        error = File_open(&storageFileHandle->device.fileHandle,
                          storageFileHandle->device.fileName,
                          FILE_OPEN_CREATE
                         );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return ERROR_NONE;
}

Errors Storage_open(StorageFileHandle *storageFileHandle,
                    const String      fileName
                   )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(fileName != NULL);

  // init variables
  storageFileHandle->mode = STORAGE_MODE_READ;

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      // check if file exists
      if (!File_exists(fileName))
      {
        return ERRORX(FILE_NOT_FOUND,0,String_cString(fileName));
      }

      // open file
      error = File_open(&storageFileHandle->fileSystem.fileHandle,
                        fileName,
                        FILE_OPEN_READ
                       );
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        {
          const char *plainPassword;
          String     tmpFileName;
          FileHandle fileHandle;
          bool       foundFlag;
          String     line;
          int        size;

          // initialise variables
          storageFileHandle->ftp.control                = NULL;
          storageFileHandle->ftp.data                   = NULL;
          storageFileHandle->ftp.index                  = 0LL;
          storageFileHandle->ftp.readAheadBuffer.offset = 0LL;
          storageFileHandle->ftp.readAheadBuffer.length = 0L;

          // connect
          if (!Network_hostExists(storageFileHandle->storageSpecifier.hostName))
          {
            return ERROR_HOST_NOT_FOUND;
          }
          if (FtpConnect(String_cString(storageFileHandle->storageSpecifier.hostName),&storageFileHandle->ftp.control) != 1)
          {
            return ERROR_FTP_SESSION_FAIL;

          }

          // login
          plainPassword = Password_deploy(storageFileHandle->storageSpecifier.loginPassword);
          if (FtpLogin(String_cString(storageFileHandle->storageSpecifier.loginName),
                       plainPassword,
                       storageFileHandle->ftp.control
                      ) != 1
             )
          {
            Password_undeploy(storageFileHandle->storageSpecifier.loginPassword);
            FtpClose(storageFileHandle->ftp.control);
            return ERROR_FTP_AUTHENTICATION;
          }
          Password_undeploy(storageFileHandle->storageSpecifier.loginPassword);

          // check if file exists: first try non-passive, then passive mode
          tmpFileName = File_newFileName();
          error = File_getTmpFileName(tmpFileName,NULL,tmpDirectory);
          if (error != ERROR_NONE)
          {
            FtpClose(storageFileHandle->ftp.control);
            return error;
          }
          FtpOptions(FTPLIB_CONNMODE,FTPLIB_PORT,storageFileHandle->ftp.control);
          if (FtpDir(String_cString(tmpFileName),String_cString(fileName),storageFileHandle->ftp.control) != 1)
          {
            FtpOptions(FTPLIB_CONNMODE,FTPLIB_PASSIVE,storageFileHandle->ftp.control);
            if (FtpDir(String_cString(tmpFileName),String_cString(fileName),storageFileHandle->ftp.control) != 1)
            {
              File_delete(tmpFileName,FALSE);
              File_deleteFileName(tmpFileName);
              FtpClose(storageFileHandle->ftp.control);
              return ERRORX(FILE_NOT_FOUND,0,String_cString(fileName));
            }
          }
          error = File_open(&fileHandle,tmpFileName,FILE_OPEN_READ);
          if (error != ERROR_NONE)
          {
            File_delete(tmpFileName,FALSE);
            File_deleteFileName(tmpFileName);
            FtpClose(storageFileHandle->ftp.control);
            return error;
          }
          foundFlag = FALSE;
          line = String_new();
          while (!File_eof(&fileHandle) && !foundFlag)
          {
            error = File_readLine(&fileHandle,line);
            if (error == ERROR_NONE)
            {
              foundFlag =   String_parse(line,
                                         STRING_BEGIN,
                                         "%32s %* %* %* %llu %d-%d-%d %d:%d %S",
                                         NULL,
                                         NULL,
                                         NULL,
                                         NULL,NULL,NULL,
                                         NULL,NULL,
                                         NULL
                                        )
                         || String_parse(line,
                                         STRING_BEGIN,
                                         "%32s %* %* %* %llu %* %* %*:%* %S",
                                         NULL,
                                         NULL,
                                         NULL,
                                         NULL
                                        )
                         || String_parse(line,
                                         STRING_BEGIN,
                                         "%32s %* %* %* %llu %* %* %* %S",
                                         NULL,
                                         NULL,
                                         NULL,
                                         NULL
                                        );
            }
          }
          String_delete(line);
          File_close(&fileHandle);
          File_delete(tmpFileName,FALSE);
          File_deleteFileName(tmpFileName);
          if (!foundFlag)
          {
            FtpClose(storageFileHandle->ftp.control);
            return ERRORX(FILE_NOT_FOUND,0,String_cString(fileName));
          }

          // get file size
          if (FtpSize(String_cString(fileName),
                      &size,
                      FTPLIB_IMAGE,
                      storageFileHandle->ftp.control
                     ) != 1
             )
          {
            FtpClose(storageFileHandle->ftp.control);
            return ERROR_FTP_GET_SIZE;
          }
          storageFileHandle->ftp.size = (uint64)size;

          // open file for reading: first try non-passive, then passive mode
          FtpOptions(FTPLIB_CONNMODE,FTPLIB_PORT,storageFileHandle->ftp.control);
          if (FtpAccess(String_cString(fileName),
                        FTPLIB_FILE_READ,
                        FTPLIB_IMAGE,
                        storageFileHandle->ftp.control,
                        &storageFileHandle->ftp.data
                       ) != 1
             )
          {
            FtpOptions(FTPLIB_CONNMODE,FTPLIB_PASSIVE,storageFileHandle->ftp.control);
            if (FtpAccess(String_cString(fileName),
                          FTPLIB_FILE_READ,
                          FTPLIB_IMAGE,
                          storageFileHandle->ftp.control,
                          &storageFileHandle->ftp.data
                         ) != 1
               )
            {
              FtpClose(storageFileHandle->ftp.control);
              return ERRORX(OPEN_FILE,0,"ftp access");
            }
          }
        }
      #else /* not HAVE_FTP */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          struct stat fileInfo;

          // init variables
          storageFileHandle->scp.index                  = 0LL;
          storageFileHandle->scp.readAheadBuffer.offset = 0LL;
          storageFileHandle->scp.readAheadBuffer.length = 0L;

          // connect
          error = Network_connect(&storageFileHandle->scp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  storageFileHandle->storageSpecifier.loginPassword,
                                  storageFileHandle->scp.sshPublicKeyFileName,
                                  storageFileHandle->scp.sshPrivateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }
          libssh2_session_set_timeout(Network_getSSHSession(&storageFileHandle->scp.socketHandle),READ_TIMEOUT);

          // install send/receive callback to track number of sent/received bytes
          storageFileHandle->scp.totalSentBytes     = 0LL;
          storageFileHandle->scp.totalReceivedBytes = 0LL;
          (*(libssh2_session_abstract(Network_getSSHSession(&storageFileHandle->scp.socketHandle)))) = storageFileHandle;
          storageFileHandle->scp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,sshSendCallback   );
          storageFileHandle->scp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,sshReceiveCallback);

          // open channel and file for reading
          storageFileHandle->scp.channel = libssh2_scp_recv(Network_getSSHSession(&storageFileHandle->scp.socketHandle),
                                                            String_cString(fileName),
                                                            &fileInfo
                                                           );
          if (storageFileHandle->scp.channel == NULL)
          {
            char *sshErrorText;

            libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->scp.socketHandle),&sshErrorText,NULL,0);
            error = ERRORX(SSH,
                           libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->scp.socketHandle)),
                           sshErrorText
                          );
            Network_disconnect(&storageFileHandle->scp.socketHandle);
            return error;
          }
          storageFileHandle->scp.size = (uint64)fileInfo.st_size;
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(fileName);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;

          // init variables
          storageFileHandle->sftp.index                  = 0LL;
          storageFileHandle->sftp.readAheadBuffer.offset = 0LL;
          storageFileHandle->sftp.readAheadBuffer.length = 0L;

          // connect
          error = Network_connect(&storageFileHandle->sftp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  storageFileHandle->storageSpecifier.loginPassword,
                                  storageFileHandle->sftp.sshPublicKeyFileName,
                                  storageFileHandle->sftp.sshPrivateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }
          libssh2_session_set_timeout(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),READ_TIMEOUT);

          // install send/receive callback to track number of sent/received bytes
          storageFileHandle->sftp.totalSentBytes     = 0LL;
          storageFileHandle->sftp.totalReceivedBytes = 0LL;
          (*(libssh2_session_abstract(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)))) = storageFileHandle;
          storageFileHandle->sftp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),LIBSSH2_CALLBACK_SEND,sftpSendCallback   );
          storageFileHandle->sftp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),LIBSSH2_CALLBACK_RECV,sftpReceiveCallback);

          // init session
          storageFileHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageFileHandle->sftp.socketHandle));
          if (storageFileHandle->sftp.sftp == NULL)
          {
            error = ERROR(SSH,libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)));
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            return error;
          }

          // open file
          storageFileHandle->sftp.sftpHandle = libssh2_sftp_open(storageFileHandle->sftp.sftp,
                                                                 String_cString(fileName),
                                                                 LIBSSH2_FXF_READ,
                                                                 0
                                                                );
          if (storageFileHandle->sftp.sftpHandle == NULL)
          {
            char *sshErrorText;

            libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),&sshErrorText,NULL,0);
            error = ERRORX(SSH,
                           libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)),
                           sshErrorText
                          );
            libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            return error;
          }

          // get file size
          if (libssh2_sftp_fstat(storageFileHandle->sftp.sftpHandle,
                                 &sftpAttributes
                                ) != 0
             )
          {
            char *sshErrorText;

            libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),&sshErrorText,NULL,0);
            error = ERRORX(SSH,
                           libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)),
                           sshErrorText
                          );
            libssh2_sftp_close(storageFileHandle->sftp.sftpHandle);
            libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            return error;
          }
          storageFileHandle->sftp.size = sftpAttributes.filesize;
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(fileName);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        {
          // initialise variables
          storageFileHandle->opticalDisk.read.index             = 0LL;
          storageFileHandle->opticalDisk.read.buffer.blockIndex = 0LL;
          storageFileHandle->opticalDisk.read.buffer.length     = 0L;

          // check if device exists
          if (!File_exists(storageFileHandle->storageSpecifier.deviceName))
          {
            return ERRORX(OPTICAL_DISK_NOT_FOUND,0,String_cString(storageFileHandle->storageSpecifier.deviceName));
          }

          // open optical disk/ISO 9660 file
          storageFileHandle->opticalDisk.read.iso9660Handle = iso9660_open(String_cString(storageFileHandle->storageSpecifier.deviceName));
          if (storageFileHandle->opticalDisk.read.iso9660Handle == NULL)
          {
            if (File_isFile(storageFileHandle->storageSpecifier.deviceName))
            {
              return ERRORX(OPEN_ISO9660_FILE,errno,String_cString(storageFileHandle->storageSpecifier.deviceName));
            }
            else
            {
              return ERRORX(OPEN_OPTICAL_DISK,errno,String_cString(storageFileHandle->storageSpecifier.deviceName));
            }
          }

          // prepare file for reading
          storageFileHandle->opticalDisk.read.iso9660Stat = iso9660_ifs_stat_translate(storageFileHandle->opticalDisk.read.iso9660Handle,
                                                                                       String_cString(fileName)
                                                                                      );
          if (storageFileHandle->opticalDisk.read.iso9660Stat == NULL)
          {
            iso9660_close(storageFileHandle->opticalDisk.read.iso9660Handle);
            return ERRORX(FILE_NOT_FOUND,errno,String_cString(fileName));
          }
        }
      #else /* not HAVE_ISO9660 */
        UNUSED_VARIABLE(fileName);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      // init variables

      // open file
#if 0
      error = File_open(&storageFileHandle->fileSystem.fileHandle,
                        storageFileHandle->fileSystem.fileName,
                        FILE_OPEN_READ
                       );
      if (error != ERROR_NONE)
      {
        String_delete(storageFileHandle->fileSystem.fileName);
        String_delete(storageSpecifier);
        return error;
      }
#endif /* 0 */
      UNUSED_VARIABLE(fileName);
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return ERROR_NONE;
}

void Storage_close(StorageFileHandle *storageFileHandle)
{
  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      switch (storageFileHandle->mode)
      {
        case STORAGE_MODE_UNKNOWN:
          break;
        case STORAGE_MODE_WRITE:
          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            File_close(&storageFileHandle->fileSystem.fileHandle);
          }
          break;
        case STORAGE_MODE_READ:
          File_close(&storageFileHandle->fileSystem.fileHandle);
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        switch (storageFileHandle->mode)
        {
          case STORAGE_MODE_UNKNOWN:
            break;
          case STORAGE_MODE_WRITE:
            if (!storageFileHandle->jobOptions->dryRunFlag)
            {
              assert(storageFileHandle->ftp.data != NULL);
              FtpClose(storageFileHandle->ftp.data);
            }
            break;
          case STORAGE_MODE_READ:
            assert(storageFileHandle->ftp.data != NULL);
            FtpClose(storageFileHandle->ftp.data);
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
          #endif /* NDEBUG */
        }
        assert(storageFileHandle->ftp.control != NULL);
        FtpClose(storageFileHandle->ftp.control);
      #else /* not HAVE_FTP */
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        switch (storageFileHandle->mode)
        {
          case STORAGE_MODE_UNKNOWN:
            break;
          case STORAGE_MODE_WRITE:
            if (!storageFileHandle->jobOptions->dryRunFlag)
            {
              libssh2_channel_send_eof(storageFileHandle->scp.channel);
//???
//              libssh2_channel_wait_eof(storageFileHandle->scp.channel);
              libssh2_channel_wait_closed(storageFileHandle->scp.channel);
              libssh2_channel_free(storageFileHandle->scp.channel);
            }
            break;
          case STORAGE_MODE_READ:
            libssh2_channel_close(storageFileHandle->scp.channel);
            libssh2_channel_wait_closed(storageFileHandle->scp.channel);
            libssh2_channel_free(storageFileHandle->scp.channel);
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
          #endif /* NDEBUG */
        }
        Network_disconnect(&storageFileHandle->scp.socketHandle);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        switch (storageFileHandle->mode)
        {
          case STORAGE_MODE_UNKNOWN:
            break;
          case STORAGE_MODE_WRITE:
            if (!storageFileHandle->jobOptions->dryRunFlag)
            {
              libssh2_sftp_close(storageFileHandle->sftp.sftpHandle);
            }
            break;
          case STORAGE_MODE_READ:
            libssh2_sftp_close(storageFileHandle->sftp.sftpHandle);
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
          #endif /* NDEBUG */
        }
        libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
        Network_disconnect(&storageFileHandle->sftp.socketHandle);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      switch (storageFileHandle->mode)
      {
        case STORAGE_MODE_UNKNOWN:
          break;
        case STORAGE_MODE_WRITE:
          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            storageFileHandle->opticalDisk.write.totalSize += File_getSize(&storageFileHandle->opticalDisk.write.fileHandle);
            File_close(&storageFileHandle->opticalDisk.write.fileHandle);
          }
          StringList_append(&storageFileHandle->opticalDisk.write.fileNameList,storageFileHandle->opticalDisk.write.fileName);
          break;
        case STORAGE_MODE_READ:
          #ifdef HAVE_ISO9660
            assert(storageFileHandle->opticalDisk.read.iso9660Handle != NULL);
            assert(storageFileHandle->opticalDisk.read.iso9660Stat != NULL);

            free(storageFileHandle->opticalDisk.read.iso9660Stat);
            iso9660_close(storageFileHandle->opticalDisk.read.iso9660Handle);
          #endif /* HAVE_ISO9660 */
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
      break;
    case STORAGE_TYPE_DEVICE:
      switch (storageFileHandle->mode)
      {
        case STORAGE_MODE_UNKNOWN:
          break;
        case STORAGE_MODE_WRITE:
          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            storageFileHandle->device.totalSize += File_getSize(&storageFileHandle->device.fileHandle);
            File_close(&storageFileHandle->device.fileHandle);
          }
          break;
        case STORAGE_MODE_READ:
          storageFileHandle->device.totalSize += File_getSize(&storageFileHandle->device.fileHandle);
          File_close(&storageFileHandle->device.fileHandle);
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
      StringList_append(&storageFileHandle->device.fileNameList,storageFileHandle->device.fileName);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
}

Errors Storage_delete(StorageFileHandle *storageFileHandle,
                      const String      fileName
                     )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);

  error = ERROR_UNKNOWN;
  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_delete(fileName,FALSE);
      }
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          error = (FtpDelete(String_cString(fileName),storageFileHandle->ftp.data) == 1)?ERROR_NONE:ERROR_DELETE_FILE;
        }
      #else /* not HAVE_FTP */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
#if 0
whould this be a possible implementation?
        {
          String command;

          // there is no unlink command for scp: execute either 'rm' or 'del' on remote server
          command = String_new();
          if (error != ERROR_NONE)
          {
            String_format(String_clear(command),"rm %'S",fileName);
            error = (libssh2_channel_exec(storageFileHandle->scp.channel,
                                          String_cString(command)
                                         ) != 0
                    )?ERROR_NONE:ERROR_DELETE_FILE;
          }
          if (error != ERROR_NONE)
          {
            String_format(String_clear(command),"del %'S",fileName);
            error = (libssh2_channel_exec(storageFileHandle->scp.channel,
                                          String_cString(command)
                                         ) != 0
                    )?ERROR_NONE:ERROR_DELETE_FILE;
          }
          String_delete(command);
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
#endif /* 0 */
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        error = Network_connect(&storageFileHandle->sftp.socketHandle,
                                SOCKET_TYPE_SSH,
                                storageFileHandle->storageSpecifier.hostName,
                                storageFileHandle->storageSpecifier.hostPort,
                                storageFileHandle->storageSpecifier.loginName,
                                storageFileHandle->storageSpecifier.loginPassword,
                                storageFileHandle->sftp.sshPublicKeyFileName,
                                storageFileHandle->sftp.sshPrivateKeyFileName,
                                0
                               );
        if (error == ERROR_NONE)
        {
          libssh2_session_set_timeout(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),READ_TIMEOUT);

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            // init session
            storageFileHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageFileHandle->sftp.socketHandle));
            if (storageFileHandle->sftp.sftp != NULL)
            {
              // delete file
              if (libssh2_sftp_unlink(storageFileHandle->sftp.sftp,
                                      String_cString(fileName)
                                     ) == 0
                 )
              {
                error = ERROR_NONE;
              }
              else
              {
                 char *sshErrorText;

                 libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),&sshErrorText,NULL,0);
                 error = ERRORX(SSH,
                                libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)),
                                sshErrorText
                               );
              }

              libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
            }
            else
            {
              char *sshErrorText;

              libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),&sshErrorText,NULL,0);
              error = ERRORX(SSH,
                             libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)),
                             sshErrorText
                            );
              Network_disconnect(&storageFileHandle->sftp.socketHandle);
            }
          }
          Network_disconnect(&storageFileHandle->sftp.socketHandle);
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_DEVICE:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

bool Storage_eof(StorageFileHandle *storageFileHandle)
{
  assert(storageFileHandle != NULL);
  assert(storageFileHandle->mode == STORAGE_MODE_READ);
  assert(storageFileHandle->jobOptions != NULL);

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        return File_eof(&storageFileHandle->fileSystem.fileHandle);
      }
      else
      {
        return TRUE;
      }
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          return storageFileHandle->ftp.index >= storageFileHandle->ftp.size;
        }
        else
        {
          return TRUE;
        }
      #else /* not HAVE_FTP */
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          return storageFileHandle->scp.index >= storageFileHandle->scp.size;
        }
        else
        {
          return TRUE;
        }
      #else /* not HAVE_SSH2 */
        return TRUE;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          return storageFileHandle->sftp.index >= storageFileHandle->sftp.size;
        }
        else
        {
          return TRUE;
        }
      #else /* not HAVE_SSH2 */
        return TRUE;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        assert(storageFileHandle->opticalDisk.read.iso9660Handle != NULL);
        assert(storageFileHandle->opticalDisk.read.iso9660Stat != NULL);

        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          return storageFileHandle->opticalDisk.read.index >= storageFileHandle->opticalDisk.read.iso9660Stat->size;
        }
        else
        {
          return TRUE;
        }
      #else /* not HAVE_ISO9660 */
        return TRUE;
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        return File_eof(&storageFileHandle->device.fileHandle);
      }
      else
      {
        return TRUE;
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return TRUE;
}

Errors Storage_read(StorageFileHandle *storageFileHandle,
                    void              *buffer,
                    ulong             size,
                    ulong             *bytesRead
                   )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->mode == STORAGE_MODE_READ);
  assert(storageFileHandle->jobOptions != NULL);
  assert(storageFileHandle->mode == STORAGE_MODE_READ);
  assert(buffer != NULL);
  assert(bytesRead != NULL);

//fprintf(stderr,"%s,%d: size=%lu\n",__FILE__,__LINE__,size);
  (*bytesRead) = 0L;
  error = ERROR_NONE;
  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_read(&storageFileHandle->fileSystem.fileHandle,buffer,size,bytesRead);
      }
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        {
          ulong   i;
          ssize_t readBytes;
          ulong   length;
          uint64  startTimestamp,endTimestamp;
          ulong   n;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->ftp.control != NULL);
            assert(storageFileHandle->ftp.data != NULL);
            assert(storageFileHandle->ftp.readAheadBuffer.data != NULL);


            // copy as much as available from read-ahead buffer
            if (   (storageFileHandle->ftp.index >= storageFileHandle->ftp.readAheadBuffer.offset)
                && (storageFileHandle->ftp.index < (storageFileHandle->ftp.readAheadBuffer.offset+storageFileHandle->ftp.readAheadBuffer.length))
               )
            {
              // copy data
              i = (ulong)(storageFileHandle->ftp.index-storageFileHandle->ftp.readAheadBuffer.offset);
              n = MIN(size,storageFileHandle->ftp.readAheadBuffer.length-i);
              memcpy(buffer,storageFileHandle->ftp.readAheadBuffer.data+i,n);

              // adjust buffer, size, bytes read, index
              buffer = (byte*)buffer+n;
              size -= n;
              (*bytesRead) += n;
              storageFileHandle->ftp.index += (uint64)n;
            }

            // read rest of data
            while (size > 0L)
            {
              // get max. number of bytes to send in one step
              if (storageFileHandle->ftp.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->ftp.bandWidthLimiter.blockSize,size);
              }
              else
              {
                length = size;
              }

              // get start time
              startTimestamp = Misc_getTimestamp();

              if (length < MAX_BUFFER_SIZE)
              {
                // read into read-ahead buffer
                readBytes = FtpRead(storageFileHandle->ftp.readAheadBuffer.data,
                                    MIN((size_t)(storageFileHandle->ftp.size-storageFileHandle->ftp.index),MAX_BUFFER_SIZE),
                                    storageFileHandle->ftp.data
                                  );
                if (readBytes == 0)
                {
                  // wait a short time for more data
                  Misc_udelay(250*1000);

                  // read into read-ahead buffer
                  readBytes = FtpRead(storageFileHandle->ftp.readAheadBuffer.data,
                                      MIN((size_t)(storageFileHandle->ftp.size-storageFileHandle->ftp.index),MAX_BUFFER_SIZE),
                                      storageFileHandle->ftp.data
                                     );
                }
                if (readBytes <= 0)
                {
                  error = ERROR(IO_ERROR,errno);
                  break;
                }
                storageFileHandle->ftp.readAheadBuffer.offset = storageFileHandle->ftp.index;
                storageFileHandle->ftp.readAheadBuffer.length = (ulong)readBytes;

                // copy data
                n = MIN(length,storageFileHandle->ftp.readAheadBuffer.length);
                memcpy(buffer,storageFileHandle->ftp.readAheadBuffer.data,n);

                // adjust buffer, size, bytes read, index
                buffer = (byte*)buffer+n;
                size -= n;
                (*bytesRead) += n;
                storageFileHandle->ftp.index += (uint64)n;
              }
              else
              {
                // read direct
                readBytes = FtpRead(buffer,
                                    length,
                                    storageFileHandle->ftp.data
                                   );
                if (readBytes == 0)
                {
                  // wait a short time for more data
                  Misc_udelay(250*1000);

                  // read direct
                  readBytes = FtpRead(buffer,
                                      size,
                                      storageFileHandle->ftp.data
                                     );

                }
                if (readBytes <= 0)
                {
                  error = ERROR(IO_ERROR,errno);
                  break;
                }
                n = (ulong)readBytes;

                // adjust buffer, size, bytes read, index
                buffer = (byte*)buffer+n;
                size -= n;
                (*bytesRead) += n;
                storageFileHandle->ftp.index += (uint64)n;
              }

              // get end time
              endTimestamp = Misc_getTimestamp();

              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->ftp.bandWidthLimiter,
                               n,
                               endTimestamp-startTimestamp
                              );
              }
            }
          }
        }
      #else /* not HAVE_FTP */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          ulong   index;
          ulong   bytesAvail;
          ulong   length;
          uint64  startTimestamp,endTimestamp;
          uint64  startTotalReceivedBytes,endTotalReceivedBytes;
          ssize_t n;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->scp.channel != NULL);
            assert(storageFileHandle->scp.readAheadBuffer.data != NULL);

            // copy as much as available from read-ahead buffer
            if (   (storageFileHandle->scp.index >= storageFileHandle->scp.readAheadBuffer.offset)
                && (storageFileHandle->scp.index < (storageFileHandle->scp.readAheadBuffer.offset+storageFileHandle->scp.readAheadBuffer.length))
               )
            {
              // copy data from read-ahead buffer
              index      = (ulong)(storageFileHandle->scp.index-storageFileHandle->scp.readAheadBuffer.offset);
              bytesAvail = MIN(size,storageFileHandle->scp.readAheadBuffer.length-index);
              memcpy(buffer,storageFileHandle->scp.readAheadBuffer.data+index,bytesAvail);

              // adjust buffer, size, bytes read, index
              buffer = (byte*)buffer+bytesAvail;
              size -= bytesAvail;
              (*bytesRead) += bytesAvail;
              storageFileHandle->scp.index += (uint64)bytesAvail;
            }

            // read rest of data
            while (size > 0L)
            {
              // get max. number of bytes to send in one step
              if (storageFileHandle->ftp.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->ftp.bandWidthLimiter.blockSize,size);
              }
              else
              {
                length = size;
              }

              // get start time, start received bytes
              startTimestamp          = Misc_getTimestamp();
              startTotalReceivedBytes = storageFileHandle->scp.totalReceivedBytes;

              if (length < MAX_BUFFER_SIZE)
              {
                // read into read-ahead buffer
                do
                {
                  n = libssh2_channel_read(storageFileHandle->scp.channel,
                                           (char*)storageFileHandle->scp.readAheadBuffer.data,
                                           MIN((size_t)(storageFileHandle->scp.size-storageFileHandle->scp.index),MAX_BUFFER_SIZE)
                                         );
                  if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100*1000);
                }
                while (n == LIBSSH2_ERROR_EAGAIN);
                if (n < 0)
                {
                  error = ERROR(IO_ERROR,errno);
                  break;
                }
                storageFileHandle->scp.readAheadBuffer.offset = storageFileHandle->scp.index;
                storageFileHandle->scp.readAheadBuffer.length = (ulong)n;
//fprintf(stderr,"%s,%d: n=%ld storageFileHandle->scp.bufferOffset=%llu storageFileHandle->scp.bufferLength=%lu\n",__FILE__,__LINE__,n,
//storageFileHandle->scp.readAheadBuffer.offset,storageFileHandle->scp.readAheadBuffer.length);

                // copy data from read-ahead buffer
                bytesAvail = MIN(length,storageFileHandle->scp.readAheadBuffer.length);
                memcpy(buffer,storageFileHandle->scp.readAheadBuffer.data,bytesAvail);

                // adjust buffer, size, bytes read, index
                buffer = (byte*)buffer+bytesAvail;
                size -= bytesAvail;
                (*bytesRead) += bytesAvail;
                storageFileHandle->scp.index += (uint64)bytesAvail;
              }
              else
              {
                // read direct
                do
                {
                  n = libssh2_channel_read(storageFileHandle->scp.channel,
                                                   buffer,
                                                   length
                                                  );
                  if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100*1000);
                }
                while (n == LIBSSH2_ERROR_EAGAIN);
                if (n < 0)
                {
                  error = ERROR(IO_ERROR,errno);
                  break;
                }

                // adjust buffer, size, bytes read, index
                buffer = (byte*)buffer+(ulong)n;
                size -= (ulong)n;
                (*bytesRead) += (ulong)n;
                storageFileHandle->scp.index += (uint64)n;
              }

              // get end time, end received bytes
              endTimestamp          = Misc_getTimestamp();
              endTotalReceivedBytes = storageFileHandle->scp.totalReceivedBytes;
              assert(endTotalReceivedBytes >= startTotalReceivedBytes);

              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->scp.bandWidthLimiter,
                               endTotalReceivedBytes-startTotalReceivedBytes,
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
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          ulong   index;
          ulong   bytesAvail;
          ulong   length;
          uint64  startTimestamp,endTimestamp;
          uint64  startTotalReceivedBytes,endTotalReceivedBytes;
          ssize_t n;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->sftp.sftpHandle != NULL);
            assert(storageFileHandle->sftp.readAheadBuffer.data != NULL);

            // copy as much as available from read-ahead buffer
            if (   (storageFileHandle->sftp.index >= storageFileHandle->sftp.readAheadBuffer.offset)
                && (storageFileHandle->sftp.index < (storageFileHandle->sftp.readAheadBuffer.offset+storageFileHandle->sftp.readAheadBuffer.length))
               )
            {
              // copy data from read-ahead buffer
              index      = (ulong)(storageFileHandle->sftp.index-storageFileHandle->sftp.readAheadBuffer.offset);
              bytesAvail = MIN(size,storageFileHandle->sftp.readAheadBuffer.length-index);
              memcpy(buffer,storageFileHandle->sftp.readAheadBuffer.data+index,bytesAvail);

              // adjust buffer, size, bytes read, index
              buffer = (byte*)buffer+bytesAvail;
              size -= bytesAvail;
              (*bytesRead) += bytesAvail;
              storageFileHandle->sftp.index += bytesAvail;
            }

            // read rest of data
            if (size > 0)
            {
              // get max. number of bytes to send in one step
              if (storageFileHandle->ftp.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->ftp.bandWidthLimiter.blockSize,size);
              }
              else
              {
                length = size;
              }

              // get start time, start received bytes
              startTimestamp          = Misc_getTimestamp();
              startTotalReceivedBytes = storageFileHandle->sftp.totalReceivedBytes;

              #ifdef HAVE_SSH2_SFTP_SEEK2
                libssh2_sftp_seek2(storageFileHandle->sftp.sftpHandle,storageFileHandle->sftp.index);
              #else
                libssh2_sftp_seek(storageFileHandle->sftp.sftpHandle,storageFileHandle->sftp.index);
              #endif

              if (length < MAX_BUFFER_SIZE)
              {
                // read into read-ahead buffer
                n = libssh2_sftp_read(storageFileHandle->sftp.sftpHandle,
                                      (char*)storageFileHandle->sftp.readAheadBuffer.data,
                                      MIN((size_t)(storageFileHandle->sftp.size-storageFileHandle->sftp.index),MAX_BUFFER_SIZE)
                                     );
                if (n < 0)
                {
                  error = ERROR(IO_ERROR,errno);
                  break;
                }
                storageFileHandle->sftp.readAheadBuffer.offset = storageFileHandle->sftp.index;
                storageFileHandle->sftp.readAheadBuffer.length = (ulong)n;
//fprintf(stderr,"%s,%d: n=%ld storageFileHandle->sftp.bufferOffset=%llu storageFileHandle->sftp.bufferLength=%lu\n",__FILE__,__LINE__,n,
//storageFileHandle->sftp.readAheadBuffer.offset,storageFileHandle->sftp.readAheadBuffer.length);

                // copy data from read-ahead buffer
                bytesAvail = MIN(length,storageFileHandle->sftp.readAheadBuffer.length);
                memcpy(buffer,storageFileHandle->sftp.readAheadBuffer.data,bytesAvail);

                // adjust buffer, size, bytes read, index
                (*bytesRead) += bytesAvail;
                storageFileHandle->sftp.index += (uint64)bytesAvail;
              }
              else
              {
                // read direct
                n = libssh2_sftp_read(storageFileHandle->sftp.sftpHandle,
                                      buffer,
                                      length
                                     );
                if (n < 0)
                {
                  error = ERROR(IO_ERROR,errno);
                  break;
                }

                // adjust buffer, size, bytes read, index
                (*bytesRead) += (ulong)n;
                storageFileHandle->sftp.index += (uint64)n;
              }

              // get end time, end received bytes
              endTimestamp          = Misc_getTimestamp();
              endTotalReceivedBytes = storageFileHandle->sftp.totalReceivedBytes;
              assert(endTotalReceivedBytes >= startTotalReceivedBytes);

              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->sftp.bandWidthLimiter,
                               endTotalReceivedBytes-startTotalReceivedBytes,
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
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        {
          uint64   blockIndex;
          uint     blockOffset;
          long int readBytes;
          ulong    n;

          assert(storageFileHandle->opticalDisk.read.iso9660Handle != NULL);
          assert(storageFileHandle->opticalDisk.read.iso9660Stat != NULL);

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->opticalDisk.read.buffer.data != NULL);

            while (   (size > 0L)
                   && (storageFileHandle->opticalDisk.read.index < (uint64)storageFileHandle->opticalDisk.read.iso9660Stat->size)
                  )
            {
              // get ISO9660 block index, offset
              blockIndex  = (int64)(storageFileHandle->opticalDisk.read.index/ISO_BLOCKSIZE);
              blockOffset = (uint)(storageFileHandle->opticalDisk.read.index%ISO_BLOCKSIZE);
//fprintf(stderr,"%s, %d: blockIndex=%ld blockOffset=%d\n",__FILE__,__LINE__,blockIndex,blockOffset);

              if (   (blockIndex != storageFileHandle->opticalDisk.read.buffer.blockIndex)
                  || (blockOffset >= storageFileHandle->opticalDisk.read.buffer.length)
                 )
              {
                // read ISO9660 block
                readBytes = iso9660_iso_seek_read(storageFileHandle->opticalDisk.read.iso9660Handle,
                                                  storageFileHandle->opticalDisk.read.buffer.data,
                                                  storageFileHandle->opticalDisk.read.iso9660Stat->lsn+(lsn_t)blockIndex,
                                                  1 // read 1 block
                                                 );
                if (readBytes < ISO_BLOCKSIZE)
                {
                  error = ERROR(IO_ERROR,errno);
                  break;
                }
                storageFileHandle->opticalDisk.read.buffer.blockIndex = blockIndex;
                storageFileHandle->opticalDisk.read.buffer.length     = (((blockIndex+1)*ISO_BLOCKSIZE) <= (uint64)storageFileHandle->opticalDisk.read.iso9660Stat->size)
                                                                          ? ISO_BLOCKSIZE
                                                                          : (ulong)(storageFileHandle->opticalDisk.read.iso9660Stat->size%ISO_BLOCKSIZE);
              }

              // copy data
              n = MIN(size,storageFileHandle->opticalDisk.read.buffer.length-blockOffset);
              memcpy(buffer,storageFileHandle->opticalDisk.read.buffer.data+blockOffset,n);

              // adjust buffer, size, bytes read, index
              buffer = (byte*)buffer+n;
              size -= n;
              (*bytesRead) += n;
              storageFileHandle->opticalDisk.read.index += n;
            }
          }
        }
      #else /* not HAVE_ISO9660 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_read(&storageFileHandle->device.fileHandle,buffer,size,bytesRead);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

Errors Storage_write(StorageFileHandle *storageFileHandle,
                     const void        *buffer,
                     ulong             size
                    )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->mode == STORAGE_MODE_WRITE);
  assert(storageFileHandle->jobOptions != NULL);
  assert(buffer != NULL);

  error = ERROR_NONE;
  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_write(&storageFileHandle->fileSystem.fileHandle,buffer,size);
      }
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        {
          ulong  writtenBytes;
          ulong  length;
          uint64 startTimestamp,endTimestamp;
          long   n;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->ftp.control != NULL);
            assert(storageFileHandle->ftp.data != NULL);

            writtenBytes = 0L;
            while (writtenBytes < size)
            {
              // get max. number of bytes to send in one step
              if (storageFileHandle->ftp.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->ftp.bandWidthLimiter.blockSize,size-writtenBytes);
              }
              else
              {
                length = size-writtenBytes;
              }

              // get start time
              startTimestamp = Misc_getTimestamp();

              // send data
              n = FtpWrite((void*)buffer,length,storageFileHandle->ftp.data);
              if      (n < 0)
              {
                error = ERROR_NETWORK_SEND;
                break;
              }
              else if (n == 0)
              {
                n = FtpWrite((void*)buffer,length,storageFileHandle->ftp.data);
                if      (n <= 0)
                {
                  error = ERROR_NETWORK_SEND;
                  break;
                }
              }
              buffer = (byte*)buffer+n;
              writtenBytes += n;

              // get end time
              endTimestamp = Misc_getTimestamp();

              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->ftp.bandWidthLimiter,
                               n,
                               endTimestamp-startTimestamp
                              );
              }
            }
          }
        }
      #else /* not HAVE_FTP */
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          ulong   writtenBytes;
          ulong   length;
          uint64  startTimestamp,endTimestamp;
          uint64  startTotalSentBytes,endTotalSentBytes;
          ssize_t n;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->scp.channel != NULL);

            writtenBytes = 0L;
            while (writtenBytes < size)
            {
              // get max. number of bytes to send in one step
              if (storageFileHandle->scp.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->scp.bandWidthLimiter.blockSize,size-writtenBytes);
              }
              else
              {
                length = size-writtenBytes;
              }

              // workaround for libssh2-problem: it seems sending of blocks >=4k cause problems, e. g. corrupt ssh MAC?
              length = MIN(length,4*1024);

              // get start time, start received bytes
              startTimestamp      = Misc_getTimestamp();
              startTotalSentBytes = storageFileHandle->scp.totalSentBytes;

              // send data
              do
              {
                n = libssh2_channel_write(storageFileHandle->scp.channel,
                                          buffer,
                                          length
                                         );
                if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100*1000);
              }
              while (n == LIBSSH2_ERROR_EAGAIN);

              // get end time, end received bytes
              endTimestamp      = Misc_getTimestamp();
              endTotalSentBytes = storageFileHandle->scp.totalSentBytes;
              assert(endTotalSentBytes >= startTotalSentBytes);

// ??? is it possible in blocking-mode that write() return 0 and this is not an error?
#if 1
              if      (n == 0)
              {
                // should not happen in blocking-mode: bug? libssh2 API changed somewhere between 0.18 and 1.2.4? => wait for data
                if (!waitSessionSocket(&storageFileHandle->scp.socketHandle))
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
              buffer = (byte*)buffer+(ulong)n;
              writtenBytes += (ulong)n;


              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->scp.bandWidthLimiter,
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
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          ulong   writtenBytes;
          ulong   length;
          uint64  startTimestamp,endTimestamp;
          uint64  startTotalSentBytes,endTotalSentBytes;
          ssize_t n;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->sftp.sftpHandle != NULL);

            writtenBytes = 0L;
            while (writtenBytes < size)
            {
              // get max. number of bytes to send in one step
              if (storageFileHandle->sftp.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->sftp.bandWidthLimiter.blockSize,size-writtenBytes);
              }
              else
              {
                length = size-writtenBytes;
              }

              // get start time, start received bytes
              startTimestamp      = Misc_getTimestamp();
              startTotalSentBytes = storageFileHandle->sftp.totalSentBytes;

              // send data
              do
              {
                n = libssh2_sftp_write(storageFileHandle->sftp.sftpHandle,
                                       buffer,
                                       length
                                      );
                if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100*1000);
              }
              while (n == LIBSSH2_ERROR_EAGAIN);

              // get end time, end received bytes
              endTimestamp      = Misc_getTimestamp();
              endTotalSentBytes = storageFileHandle->sftp.totalSentBytes;
              assert(endTotalSentBytes >= startTotalSentBytes);

// ??? is it possible in blocking-mode that write() return 0 and this is not an error?
#if 1
              if      (n == 0)
              {
                // should not happen in blocking-mode: bug? libssh2 API changed somewhere between 0.18 and 1.2.4? => wait for data
                if (!waitSessionSocket(&storageFileHandle->sftp.socketHandle))
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
              size -= n;

              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->sftp.bandWidthLimiter,
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
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_write(&storageFileHandle->opticalDisk.write.fileHandle,buffer,size);
      }
      break;
    case STORAGE_TYPE_DEVICE:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_write(&storageFileHandle->device.fileHandle,buffer,size);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

uint64 Storage_getSize(StorageFileHandle *storageFileHandle)
{
  uint64 size;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);

  size = 0LL;
  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        size = File_getSize(&storageFileHandle->fileSystem.fileHandle);
      }
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          size = storageFileHandle->ftp.size;
        }
      #else /* not HAVE_FTP */
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          size = storageFileHandle->scp.size;
        }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          size = storageFileHandle->sftp.size;
        }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          size = (uint64)storageFileHandle->opticalDisk.read.iso9660Stat->size;
        }
      #else /* not HAVE_ISO9660 */
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        size = File_getSize(&storageFileHandle->device.fileHandle);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return size;
}

Errors Storage_tell(StorageFileHandle *storageFileHandle,
                    uint64            *offset
                   )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_tell(&storageFileHandle->fileSystem.fileHandle,offset);
      }
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          (*offset) = storageFileHandle->ftp.index;
          error     = ERROR_NONE;
        }
      #else /* not HAVE_FTP */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          (*offset) = storageFileHandle->scp.index;
          error     = ERROR_NONE;
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          (*offset) = storageFileHandle->sftp.index;
          error     = ERROR_NONE;
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          (*offset) = storageFileHandle->opticalDisk.read.index;
          error     = ERROR_NONE;
        }
      #else /* not HAVE_ISO9660 */
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_tell(&storageFileHandle->device.fileHandle,offset);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

Errors Storage_seek(StorageFileHandle *storageFileHandle,
                    uint64            offset
                   )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);

  error = ERROR_NONE;
  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_seek(&storageFileHandle->fileSystem.fileHandle,offset);
      }
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        /* ftp protocol does not support a seek-function. Thus try to
           read and discard data to position the read index to the
           requested offset.
           Note: this is slow!
        */

        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          assert(storageFileHandle->ftp.readAheadBuffer.data != NULL);

          if      (offset > storageFileHandle->ftp.index)
          {
            uint64  skip;
            uint64  i;
            uint64  n;
            ssize_t readBytes;

            skip = offset-storageFileHandle->ftp.index;

            while (skip > 0LL)
            {
              // skip data in read-ahead buffer
              if (   (storageFileHandle->ftp.index >= storageFileHandle->ftp.readAheadBuffer.offset)
                  && (storageFileHandle->ftp.index < (storageFileHandle->ftp.readAheadBuffer.offset+storageFileHandle->ftp.readAheadBuffer.length))
                 )
              {
                i = storageFileHandle->ftp.index-storageFileHandle->ftp.readAheadBuffer.offset;
                n = MIN(skip,storageFileHandle->ftp.readAheadBuffer.length-i);
                skip -= n;
                storageFileHandle->ftp.index += n;
              }

              if (skip > 0LL)
              {
                // read data
                readBytes = FtpRead(storageFileHandle->ftp.readAheadBuffer.data,
                                    MIN((size_t)skip,MAX_BUFFER_SIZE),
                                    storageFileHandle->ftp.data
                                   );
                if (readBytes == 0)
                {
                  // wait a short time for more data
                  Misc_udelay(250*1000);

                  // read into read-ahead buffer
                  readBytes = FtpRead(storageFileHandle->ftp.readAheadBuffer.data,
                                      MIN((size_t)skip,MAX_BUFFER_SIZE),
                                      storageFileHandle->ftp.data
                                     );
                }
                if (readBytes <= 0)
                {
                  error = ERROR(IO_ERROR,errno);
                  break;
                }
                storageFileHandle->ftp.readAheadBuffer.offset = storageFileHandle->ftp.index;
                storageFileHandle->ftp.readAheadBuffer.length = (uint64)readBytes;
              }
            }
          }
          else if (offset < storageFileHandle->ftp.index)
          {
            error = ERROR_FUNCTION_NOT_SUPPORTED;
          }
        }
      #else /* not HAVE_FTP */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        /* scp protocol does not support a seek-function. Thus try to
           read and discard data to position the read index to the
           requested offset.
           Note: this is slow!
        */

        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          assert(storageFileHandle->scp.channel != NULL);
          assert(storageFileHandle->scp.readAheadBuffer.data != NULL);

          if      (offset > storageFileHandle->scp.index)
          {
            uint64  skip;
            uint64  i;
            uint64  n;
            ssize_t readBytes;

            skip = offset-storageFileHandle->scp.index;

            while (skip > 0LL)
            {
              // skip data in read-ahead buffer
              if (   (storageFileHandle->scp.index >= storageFileHandle->scp.readAheadBuffer.offset)
                  && (storageFileHandle->scp.index < (storageFileHandle->scp.readAheadBuffer.offset+storageFileHandle->scp.readAheadBuffer.length))
                 )
              {
                i = storageFileHandle->scp.index-storageFileHandle->scp.readAheadBuffer.offset;
                n = MIN(skip,storageFileHandle->scp.readAheadBuffer.length-i);
                skip -= n;
                storageFileHandle->scp.index += n;
              }

              if (skip > 0LL)
              {
                // wait for data
                if (!waitSessionSocket(&storageFileHandle->scp.socketHandle))
                {
                  error = ERROR(IO_ERROR,errno);
                  break;
                }

                // read data
                readBytes = libssh2_channel_read(storageFileHandle->scp.channel,
                                                 (char*)storageFileHandle->scp.readAheadBuffer.data,
                                                 MIN((size_t)skip,MAX_BUFFER_SIZE)
                                                );
                if (readBytes < 0)
                {
                  error = ERROR(IO_ERROR,errno);
                  break;
                }
                storageFileHandle->scp.readAheadBuffer.offset = storageFileHandle->scp.index;
                storageFileHandle->scp.readAheadBuffer.length = (uint64)readBytes;
              }
            }
          }
          else if (offset < storageFileHandle->scp.index)
          {
            error = ERROR_FUNCTION_NOT_SUPPORTED;
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          #ifdef HAVE_SSH2_SFTP_SEEK2
            libssh2_sftp_seek2(storageFileHandle->sftp.sftpHandle,
                               offset
                              );
          #else
            libssh2_sftp_seek(storageFileHandle->sftp.sftpHandle,
                              (size_t)offset
                             );
          #endif
          storageFileHandle->sftp.index = offset;
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          storageFileHandle->opticalDisk.read.index = offset;
          error = ERROR_NONE;
        }
      #else /* not HAVE_ISO9660 */
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_seek(&storageFileHandle->device.fileHandle,offset);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

/*---------------------------------------------------------------------*/

Errors Storage_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                 const String               storageName,
                                 const JobOptions           *jobOptions
                                )
{
  Errors           error;
  StorageSpecifier storageSpecifier;
  String           pathName;

  assert(storageDirectoryListHandle != NULL);
  assert(storageName != NULL);
  assert(jobOptions != NULL);

  // get storage specifier, file name, path name
  Storage_initSpecifier(&storageSpecifier);
  pathName = String_new();
  error = Storage_parseName(storageName,&storageSpecifier,pathName);
  if (error != ERROR_NONE)
  {
    String_delete(pathName);
    Storage_doneSpecifier(&storageSpecifier);
    return error;
  }

  // open directory listing
  error = ERROR_UNKNOWN;
  switch (storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      UNUSED_VARIABLE(jobOptions);

      // init variables
      storageDirectoryListHandle->type = STORAGE_TYPE_FILESYSTEM;

      // open directory
      error = File_openDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle,
                                     pathName
                                    );
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        {
          String     hostName;
          uint       hostPort;
          String     loginName;
          Password   *loginPassword;
          FTPServer  ftpServer;
          const char *plainLoginPassword;
          netbuf     *control;

          // init variables
          storageDirectoryListHandle->type                 = STORAGE_TYPE_FTP;
          storageDirectoryListHandle->ftp.fileListFileName = String_new();
          storageDirectoryListHandle->ftp.line             = String_new();

          // create temporary list file
          error = File_getTmpFileName(storageDirectoryListHandle->ftp.fileListFileName,NULL,tmpDirectory);
          if (error != ERROR_NONE)
          {
            String_delete(storageDirectoryListHandle->ftp.line);
            String_delete(storageDirectoryListHandle->ftp.fileListFileName);
            break;
          }

          // parse storage specifier string
          hostName      = String_new();
          loginName     = String_new();
          loginPassword = Password_new();
          if (!Storage_parseFTPSpecifier(storageSpecifier.string,hostName,&hostPort,loginName,loginPassword))
          {
            error = ERROR_FTP_SESSION_FAIL;
            Password_delete(loginPassword);
            String_delete(loginName);
            String_delete(hostName);
            File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
            String_delete(storageDirectoryListHandle->ftp.line);
            String_delete(storageDirectoryListHandle->ftp.fileListFileName);
            break;
          }

          // get FTP server settings
          getFTPServerSettings(hostName,jobOptions,&ftpServer);
          if (String_isEmpty(loginName)) String_set(loginName,ftpServer.loginName);

          // check FTP login, get correct password
          error = ERROR_UNKNOWN;
          if ((error != ERROR_NONE) && !Password_empty(loginPassword))
          {
            error = checkFTPLogin(hostName,
                                  hostPort,
                                  loginName,
                                  loginPassword
                                 );
          }
          if ((error != ERROR_NONE) && (ftpServer.password != NULL))
          {
            error = checkFTPLogin(hostName,
                                  hostPort,
                                  loginName,
                                  ftpServer.password
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(loginPassword,ftpServer.password);
            }
          }
          if ((error != ERROR_NONE) && !Password_empty(defaultFTPPassword))
          {
            error = checkFTPLogin(hostName,
                                  hostPort,
                                  loginName,
                                  defaultFTPPassword
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(loginPassword,defaultFTPPassword);
            }
          }
          if (error != ERROR_NONE)
          {
            // initialize default password
            if (initFTPPassword(hostName,loginName,jobOptions))
            {
              error = checkFTPLogin(hostName,
                                    hostPort,
                                    loginName,
                                    defaultFTPPassword
                                   );
              if (error == ERROR_NONE)
              {
                Password_set(loginPassword,defaultFTPPassword);
              }
            }
            else
            {
              error = !Password_empty(ftpServer.password) ? ERROR_INVALID_FTP_PASSWORD : ERROR_NO_FTP_PASSWORD;
            }
          }
          if (error != ERROR_NONE)
          {
            Password_delete(loginPassword);
            String_delete(loginName);
            String_delete(hostName);
            File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
            String_delete(storageDirectoryListHandle->ftp.line);
            String_delete(storageDirectoryListHandle->ftp.fileListFileName);
            break;
          }

          // connect
          if (!Network_hostExists(hostName))
          {
            error = ERRORX(HOST_NOT_FOUND,0,String_cString(hostName));
            Password_delete(loginPassword);
            String_delete(loginName);
            String_delete(hostName);
            File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
            String_delete(storageDirectoryListHandle->ftp.line);
            String_delete(storageDirectoryListHandle->ftp.fileListFileName);
            break;
          }
          if (FtpConnect(String_cString(hostName),&control) != 1)
          {
            error = ERROR_FTP_SESSION_FAIL;
            Password_delete(loginPassword);
            String_delete(loginName);
            String_delete(hostName);
            File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
            String_delete(storageDirectoryListHandle->ftp.line);
            String_delete(storageDirectoryListHandle->ftp.fileListFileName);
            break;
          }

          // login
          plainLoginPassword = Password_deploy(loginPassword);
          if (FtpLogin(String_cString(loginName),
                       plainLoginPassword,
                       control
                      ) != 1
             )
          {
            error = ERROR_FTP_AUTHENTICATION;
            Password_undeploy(loginPassword);
            FtpClose(control);
            Password_delete(loginPassword);
            String_delete(loginName);
            String_delete(hostName);
            File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
            String_delete(storageDirectoryListHandle->ftp.line);
            String_delete(storageDirectoryListHandle->ftp.fileListFileName);
            break;
          }
          Password_undeploy(loginPassword);

          // read directory: first try non-passive, then passive mode
          FtpOptions(FTPLIB_CONNMODE,FTPLIB_PORT,control);
          if (FtpDir(String_cString(storageDirectoryListHandle->ftp.fileListFileName),String_cString(pathName),control) != 1)
          {
            FtpOptions(FTPLIB_CONNMODE,FTPLIB_PASSIVE,control);
            if (FtpDir(String_cString(storageDirectoryListHandle->ftp.fileListFileName),String_cString(pathName),control) != 1)
            {
              error = ERRORX(OPEN_DIRECTORY,0,String_cString(pathName));
              FtpClose(control);
              Password_delete(loginPassword);
              String_delete(loginName);
              String_delete(hostName);
              File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
              String_delete(storageDirectoryListHandle->ftp.line);
              String_delete(storageDirectoryListHandle->ftp.fileListFileName);
              break;
            }
          }

          // disconnect
          FtpQuit(control);

          // open list file
          error = File_open(&storageDirectoryListHandle->ftp.fileHandle,storageDirectoryListHandle->ftp.fileListFileName,FILE_OPEN_READ);
          if (error != ERROR_NONE)
          {
            Password_delete(loginPassword);
            String_delete(loginName);
            String_delete(hostName);
            File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
            String_delete(storageDirectoryListHandle->ftp.line);
            String_delete(storageDirectoryListHandle->ftp.fileListFileName);
            break;
          }

          // free resources
          Password_delete(loginPassword);
          String_delete(loginName);
          String_delete(hostName);
        }
      #else /* not HAVE_FTP */
        UNUSED_VARIABLE(jobOptions);

        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_FTP */
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
//      error = ERROR_FUNCTION_NOT_SUPPORTED;
//      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          String    loginName;
          String    hostName;
          uint      hostPort;
          SSHServer sshServer;

          // init variables
          storageDirectoryListHandle->type               = STORAGE_TYPE_SFTP;
          storageDirectoryListHandle->sftp.pathName      = String_new();
          storageDirectoryListHandle->sftp.buffer        = (char*)malloc(MAX_FILENAME_LENGTH);
          if (storageDirectoryListHandle->sftp.buffer == NULL)
          {
            error = ERROR_INSUFFICIENT_MEMORY;
            break;
          }
          storageDirectoryListHandle->sftp.bufferLength  = 0;
          storageDirectoryListHandle->sftp.entryReadFlag = FALSE;

          // parse storage specifier string
          loginName = String_new();
          hostName  = String_new();
          if (!Storage_parseSSHSpecifier(storageSpecifier.string,hostName,&hostPort,loginName))
          {
            error = ERROR_SSH_SESSION_FAIL;
            String_delete(hostName);
            String_delete(loginName);
            free(storageDirectoryListHandle->sftp.buffer);
            String_delete(storageDirectoryListHandle->sftp.pathName);
            break;
          }
          String_set(storageDirectoryListHandle->sftp.pathName,pathName);

          // get SSH server settings
          getSSHServerSettings(hostName,jobOptions,&sshServer);
          if (String_isEmpty(loginName)) String_set(loginName,sshServer.loginName);

          // open network connection
          error = Network_connect(&storageDirectoryListHandle->sftp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  hostName,
                                  (hostPort != 0)?hostPort:sshServer.port,
                                  loginName,
                                  (sshServer.password != NULL)?sshServer.password:defaultSSHPassword,
                                  sshServer.publicKeyFileName,
                                  sshServer.privateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            String_delete(hostName);
            String_delete(loginName);
            free(storageDirectoryListHandle->sftp.buffer);
            String_delete(storageDirectoryListHandle->sftp.pathName);
            break;
          }
          libssh2_session_set_timeout(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle),READ_TIMEOUT);

          // init FTP session
          storageDirectoryListHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle));
          if (storageDirectoryListHandle->sftp.sftp == NULL)
          {
            error = ERROR(SSH,libssh2_session_last_errno(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle)));
            Network_disconnect(&storageDirectoryListHandle->sftp.socketHandle);
            String_delete(hostName);
            String_delete(loginName);
            free(storageDirectoryListHandle->sftp.buffer);
            String_delete(storageDirectoryListHandle->sftp.pathName);
            break;
          }

          // open directory for reading
          storageDirectoryListHandle->sftp.sftpHandle = libssh2_sftp_opendir(storageDirectoryListHandle->sftp.sftp,
                                                                             String_cString(storageDirectoryListHandle->sftp.pathName)
                                                                            );
          if (storageDirectoryListHandle->sftp.sftpHandle == NULL)
          {
            error = ERROR(SSH,libssh2_session_last_errno(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle)));
            libssh2_sftp_shutdown(storageDirectoryListHandle->sftp.sftp);
            Network_disconnect(&storageDirectoryListHandle->sftp.socketHandle);
            String_delete(hostName);
            String_delete(loginName);
            free(storageDirectoryListHandle->sftp.buffer);
            String_delete(storageDirectoryListHandle->sftp.pathName);
            break;
          }

          // free resources
          String_delete(hostName);
          String_delete(loginName);
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(jobOptions);

        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      // init variables
      switch (storageSpecifier.type)
      {
        case STORAGE_TYPE_CD : storageDirectoryListHandle->type = STORAGE_TYPE_CD;  break;
        case STORAGE_TYPE_DVD: storageDirectoryListHandle->type = STORAGE_TYPE_DVD; break;
        case STORAGE_TYPE_BD : storageDirectoryListHandle->type = STORAGE_TYPE_BD;  break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break; /* not reached */
      }

      #ifdef HAVE_ISO9660
        UNUSED_VARIABLE(jobOptions);

        storageDirectoryListHandle->opticalDisk.pathName = String_duplicate(pathName);

        // check if device exists
        if (!File_exists(storageName))
        {
          return ERRORX(OPTICAL_DISK_NOT_FOUND,0,String_cString(storageName));
        }

        // open optical disk/ISO 9660 file
        storageDirectoryListHandle->opticalDisk.iso9660Handle = iso9660_open(String_cString(storageName));
        if (storageDirectoryListHandle->opticalDisk.iso9660Handle == NULL)
        {
          if (File_isFile(storageName))
          {
            error = ERRORX(OPEN_ISO9660_FILE,errno,String_cString(storageName));
          }
          else
          {
            error = ERRORX(OPEN_OPTICAL_DISK,errno,String_cString(storageName));
          }
        }

        // open directory for reading
        storageDirectoryListHandle->opticalDisk.cdioList = iso9660_ifs_readdir(storageDirectoryListHandle->opticalDisk.iso9660Handle,
                                                                               String_cString(pathName)
                                                                              );
        if (storageDirectoryListHandle->opticalDisk.cdioList == NULL)
        {
          iso9660_close(storageDirectoryListHandle->opticalDisk.iso9660Handle);
          return ERRORX(FILE_NOT_FOUND,errno,String_cString(pathName));
        }

        storageDirectoryListHandle->opticalDisk.cdioNextNode = _cdio_list_begin(storageDirectoryListHandle->opticalDisk.cdioList);
      #else /* not HAVE_ISO9660 */
        // open directory
        error = File_openDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle,
                                       storageName
                                      );
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      UNUSED_VARIABLE(jobOptions);

      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  // free resources
  String_delete(pathName);
  Storage_doneSpecifier(&storageSpecifier);

  return error;
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
      #ifdef HAVE_FTP
        File_close(&storageDirectoryListHandle->ftp.fileHandle);
        File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
        String_delete(storageDirectoryListHandle->ftp.line);
        String_delete(storageDirectoryListHandle->ftp.fileListFileName);
      #else /* not HAVE_FTP */
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      HALT_INTERNAL_ERROR("scp does not support directory operations");
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
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        _cdio_list_free(storageDirectoryListHandle->opticalDisk.cdioList,true);
        iso9660_close(storageDirectoryListHandle->opticalDisk.iso9660Handle);
        String_delete(storageDirectoryListHandle->opticalDisk.pathName);
      #else /* not HAVE_ISO9660 */
        File_closeDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle);
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
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
      #ifdef HAVE_FTP
        endOfDirectoryFlag = File_eof(&storageDirectoryListHandle->ftp.fileHandle);
      #else /* not HAVE_FTP */
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      HALT_INTERNAL_ERROR("scp does not support directory operations");
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
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        endOfDirectoryFlag = (storageDirectoryListHandle->opticalDisk.cdioNextNode == NULL);
      #else /* not HAVE_ISO9660 */
        endOfDirectoryFlag = File_endOfDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle);
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
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

  error = ERROR_UNKNOWN;
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
          error = File_getFileInfo(fileInfo,fileName);
        }
      }
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        {
          bool   readFlag;
          char   permission[32];
          uint64 size;
          uint   year,month,day;
          uint   hour,minute;
          uint   n;

          readFlag = FALSE;
          while (!readFlag && !File_eof(&storageDirectoryListHandle->ftp.fileHandle))
          {
            // read line
            error = File_readLine(&storageDirectoryListHandle->ftp.fileHandle,storageDirectoryListHandle->ftp.line);
            if (error != ERROR_NONE)
            {
              return error;
            }
//fprintf(stderr,"%s,%d: line=%s\n",__FILE__,__LINE__,String_cString(storageDirectoryListHandle->ftp.line));

            // parse
            if      (String_parse(storageDirectoryListHandle->ftp.line,
                                  STRING_BEGIN,
                                  "%32s %* %* %* %llu %d-%d-%d %d:%d %S",
                                  NULL,
                                  permission,
                                  &size,
                                  &year,&month,&day,
                                  &hour,&minute,
                                  fileName
                                 )
                    )
            {
              if (fileInfo != NULL)
              {
                n = strlen(permission);

                switch (permission[0])
                {
                  case 'd': fileInfo->type = FILE_TYPE_DIRECTORY; break;
                  default:  fileInfo->type = FILE_TYPE_FILE; break;
                }
                fileInfo->size            = size;
                fileInfo->timeLastAccess  = 0LL;
                fileInfo->timeModified    = Misc_makeDateTime(year,month,day,
                                                              hour,minute,0
                                                             );
                fileInfo->timeLastChanged = 0LL;
                fileInfo->userId          = 0;
                fileInfo->groupId         = 0;
                fileInfo->permission      = 0;
                if ((n > 1) && (permission[1] = 'r')) fileInfo->permission |= S_IRUSR;
                if ((n > 2) && (permission[2] = 'w')) fileInfo->permission |= S_IWUSR;
                if ((n > 3) && (permission[3] = 'x')) fileInfo->permission |= S_IXUSR;
                if ((n > 4) && (permission[4] = 'r')) fileInfo->permission |= S_IRGRP;
                if ((n > 5) && (permission[5] = 'w')) fileInfo->permission |= S_IWGRP;
                if ((n > 6) && (permission[6] = 'x')) fileInfo->permission |= S_IXGRP;
                if ((n > 7) && (permission[7] = 'r')) fileInfo->permission |= S_IROTH;
                if ((n > 8) && (permission[8] = 'w')) fileInfo->permission |= S_IWOTH;
                if ((n > 9) && (permission[9] = 'x')) fileInfo->permission |= S_IXOTH;
                fileInfo->major           = 0;
                fileInfo->minor           = 0;
              }

              readFlag = TRUE;
            }
            else if (String_parse(storageDirectoryListHandle->ftp.line,
                                  STRING_BEGIN,
                                  "%32s %* %* %* %llu %* %* %*:%* %S",
                                  NULL,
                                  permission,
                                  &size,
                                  fileName
                                 )
                    )
            {
              if (fileInfo != NULL)
              {
                n = strlen(permission);

                switch (permission[0])
                {
                  case 'd': fileInfo->type = FILE_TYPE_DIRECTORY; break;
                  default:  fileInfo->type = FILE_TYPE_FILE; break;
                }
                fileInfo->size            = size;
                fileInfo->timeLastAccess  = 0LL;
                fileInfo->timeModified    = 0LL;
                fileInfo->timeLastChanged = 0LL;
                fileInfo->userId          = 0;
                fileInfo->groupId         = 0;
                fileInfo->permission      = 0;
                if ((n > 1) && (permission[1] = 'r')) fileInfo->permission |= S_IRUSR;
                if ((n > 2) && (permission[2] = 'w')) fileInfo->permission |= S_IWUSR;
                if ((n > 3) && (permission[3] = 'x')) fileInfo->permission |= S_IXUSR;
                if ((n > 4) && (permission[4] = 'r')) fileInfo->permission |= S_IRGRP;
                if ((n > 5) && (permission[5] = 'w')) fileInfo->permission |= S_IWGRP;
                if ((n > 6) && (permission[6] = 'x')) fileInfo->permission |= S_IXGRP;
                if ((n > 7) && (permission[7] = 'r')) fileInfo->permission |= S_IROTH;
                if ((n > 8) && (permission[8] = 'w')) fileInfo->permission |= S_IWOTH;
                if ((n > 9) && (permission[9] = 'x')) fileInfo->permission |= S_IXOTH;
                fileInfo->major           = 0;
                fileInfo->minor           = 0;
              }

              readFlag = TRUE;
            }
            else if (String_parse(storageDirectoryListHandle->ftp.line,
                                  STRING_BEGIN,
                                  "%32s %* %* %* %llu %* %* %* %S",
                                  NULL,
                                  permission,
                                  &size,
                                  fileName
                                 )
                    )
            {
              if (fileInfo != NULL)
              {
                n = strlen(permission);

                switch (permission[0])
                {
                  case 'd': fileInfo->type = FILE_TYPE_DIRECTORY; break;
                  default:  fileInfo->type = FILE_TYPE_FILE; break;
                }
                fileInfo->size            = size;
                fileInfo->timeLastAccess  = 0LL;
                fileInfo->timeModified    = 0LL;
                fileInfo->timeLastChanged = 0LL;
                fileInfo->userId          = 0;
                fileInfo->groupId         = 0;
                fileInfo->permission      = 0;
                if ((n > 1) && (permission[1] = 'r')) fileInfo->permission |= S_IRUSR;
                if ((n > 2) && (permission[2] = 'w')) fileInfo->permission |= S_IWUSR;
                if ((n > 3) && (permission[3] = 'x')) fileInfo->permission |= S_IXUSR;
                if ((n > 4) && (permission[4] = 'r')) fileInfo->permission |= S_IRGRP;
                if ((n > 5) && (permission[5] = 'w')) fileInfo->permission |= S_IWGRP;
                if ((n > 6) && (permission[6] = 'x')) fileInfo->permission |= S_IXGRP;
                if ((n > 7) && (permission[7] = 'r')) fileInfo->permission |= S_IROTH;
                if ((n > 8) && (permission[8] = 'w')) fileInfo->permission |= S_IWOTH;
                if ((n > 9) && (permission[9] = 'x')) fileInfo->permission |= S_IXOTH;
                fileInfo->major           = 0;
                fileInfo->minor           = 0;
              }

              readFlag = TRUE;
            }
          }
        }
      #else /* not HAVE_FTP */
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      HALT_INTERNAL_ERROR("scp does not support directory operations");
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
                error = ERROR(IO_ERROR,errno);
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
              else if (S_ISLNK(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_LINK;
              }
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
              else if (S_ISSOCK(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_SPECIAL;
                fileInfo->specialType = FILE_SPECIAL_TYPE_SOCKET;
              }
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
              memset(fileInfo->cast,0,sizeof(FileCast));
            }

            storageDirectoryListHandle->sftp.entryReadFlag = FALSE;
          }
        }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        {
          iso9660_stat_t *iso9660Stat;
          char           *s;

          if (storageDirectoryListHandle->opticalDisk.cdioNextNode != NULL)
          {
            iso9660Stat = (iso9660_stat_t*)_cdio_list_node_data(storageDirectoryListHandle->opticalDisk.cdioNextNode);
            assert(iso9660Stat != NULL);

            s = (char*)malloc(strlen(iso9660Stat->filename)+1);
            if (s == NULL)
            {
              error = ERROR_INSUFFICIENT_MEMORY;
              break;
            }
            iso9660_name_translate(iso9660Stat->filename,s);
            String_set(fileName,storageDirectoryListHandle->opticalDisk.pathName);
            File_appendFileNameCString(fileName,s);
            free(s);

            if (fileInfo != NULL)
            {
              if      (iso9660Stat->type == _STAT_FILE)
              {
                fileInfo->type = FILE_TYPE_FILE;
              }
              else if (iso9660Stat->type == _STAT_DIR)
              {
                fileInfo->type = FILE_TYPE_DIRECTORY;
              }
              else
              {
                fileInfo->type = FILE_TYPE_UNKNOWN;
              }
              fileInfo->size            = iso9660Stat->size;
              fileInfo->timeLastAccess  = (uint64)mktime(&iso9660Stat->tm);
              fileInfo->timeModified    = (uint64)mktime(&iso9660Stat->tm);
              fileInfo->timeLastChanged = 0LL;
              fileInfo->userId          = iso9660Stat->xa.user_id;
              fileInfo->groupId         = iso9660Stat->xa.group_id;
              fileInfo->permission      = iso9660Stat->xa.attributes;
              fileInfo->major           = 0;
              fileInfo->minor           = 0;
              memset(fileInfo->cast,0,sizeof(FileCast));
            }

            storageDirectoryListHandle->opticalDisk.cdioNextNode = _cdio_list_node_next(storageDirectoryListHandle->opticalDisk.cdioNextNode);
          }
          else
          {
            error = ERROR_END_OF_DIRECTORY;
          }
        }
      #else /* not HAVE_ISO9660 */
        error = File_readDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle,fileName);
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

Errors Storage_copy(const String                 storageName,
                    const JobOptions             *jobOptions,
                    BandWidthList                *maxBandWidthList,
                    StorageRequestVolumeFunction storageRequestVolumeFunction,
                    void                         *storageRequestVolumeUserData,
                    StorageStatusInfoFunction    storageStatusInfoFunction,
                    void                         *storageStatusInfoUserData,
                    const String                 localFileName
                   )
{
  String            fileName;
  void              *buffer;
  Errors            error;
  StorageFileHandle storageFileHandle;
  FileHandle        fileHandle;
  ulong             bytesRead;

  // initialize variables
  buffer   = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // open storage
  fileName = String_new();
  error = Storage_init(&storageFileHandle,
                       storageName,
                       jobOptions,
                       maxBandWidthList,
                       storageRequestVolumeFunction,
                       storageRequestVolumeUserData,
                       storageStatusInfoFunction,
                       storageStatusInfoUserData,
                       fileName
                      );
  if (error != ERROR_NONE)
  {
    String_delete(fileName);
    free(buffer);
    return error;
  }
  error = Storage_open(&storageFileHandle,
                       fileName
                      );
  if (error != ERROR_NONE)
  {
    String_delete(fileName);
    free(buffer);
    return error;
  }

  // create local file
  error = File_open(&fileHandle,
                    localFileName,
                    FILE_OPEN_CREATE
                   );
  if (error != ERROR_NONE)
  {
    Storage_close(&storageFileHandle);
    Storage_done(&storageFileHandle);
    String_delete(fileName);
    free(buffer);
    return error;
  }

  // copy data from archive to local file
  while (   (error == ERROR_NONE)
         && !Storage_eof(&storageFileHandle)
        )
  {
    error = Storage_read(&storageFileHandle,
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
    Storage_close(&storageFileHandle);
    Storage_done(&storageFileHandle);
    String_delete(fileName);
    free(buffer);
  }

  // close local file
  File_close(&fileHandle);

  // close archive
  Storage_close(&storageFileHandle);
  Storage_done(&storageFileHandle);
  String_delete(fileName);

  // free resources
  free(buffer);

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
