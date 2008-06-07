/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/storage.c,v $
* $Revision: 1.26 $
* $Author: torsten $
* Contents: storage functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#ifdef HAVE_SSH2
  #include <libssh2.h>
  #include <libssh2_sftp.h>
#endif /* HAVE_SSH2 */
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "errors.h"
#include "strings.h"

#include "files.h"
#include "network.h"
#include "passwords.h"
#include "misc.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define MAX_BUFFER_SIZE     (4*1024)
#define MAX_FILENAME_LENGTH (4*1024)

#define UNLOAD_VOLUME_DELAY_TIME (10LL*1000LL*1000LL) /* [us] */
#define LOAD_VOLUME_DELAY_TIME   (10LL*1000LL*1000LL) /* [us] */

#define MAX_CD_SIZE  (700LL*1024LL*1024LL)
#define MAX_DVD_SIZE (4613734LL*1024LL)

#define DVD_VOLUME_SIZE            MAX_DVD_SIZE
#define DVD_VOLUME_ECC_SIZE        (3600LL*1024LL*1024LL)

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
* Name   : initPassword
* Purpose: init FTP password
* Input  : -
* Output : -
* Return : TRUE if FTP password intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

#ifdef HAVE_FTP
LOCAL bool initFTPPassword(const JobOptions *jobOptions)
{
  assert(jobOptions != NULL);

  if (jobOptions->ftpServer.password == NULL)
  {
    return Password_input(defaultFTPPassword,"FTP login password");
  }
  else
  {
    return TRUE;
  }
}
#endif /* HAVE_FTP */

/***********************************************************************\
* Name   : initSSHPassword
* Purpose: init SSH password
* Input  : -
* Output : -
* Return : TRUE if SSH password intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SSH2
LOCAL bool initSSHPassword(const JobOptions *jobOptions)
{
  assert(jobOptions != NULL);

  if (jobOptions->sshServer.password == NULL)
  {
    return Password_input(defaultSSHPassword,"Login password");
  }
  else
  {
    return TRUE;
  }
}
#endif /* HAVE_SSH2 */

/***********************************************************************\
* Name   : limitBandWidth
* Purpose: limit used band width
* Input  : storageBandWidth - storage band width
*          transmittedBytes - transmitted bytes
*          transmissionTime - time for transmission
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SSH2
LOCAL void limitBandWidth(StorageBandWidth *storageBandWidth,
                          ulong            transmittedBytes,
                          uint64           transmissionTime
                         )
{
  uint   z;
  ulong  averageBandWidth;
  uint64 delayTime;

  assert(storageBandWidth != NULL);

  if (storageBandWidth->max > 0)
  {
    storageBandWidth->measurementBytes += transmittedBytes;
    storageBandWidth->measurementTime += transmissionTime;
/*
fprintf(stderr,"%s,%d: storageBandWidth->blockSize=%ld sum=%lu %llu\n",__FILE__,__LINE__,
storageBandWidth->blockSize,
storageBandWidth->measurementBytes,
storageBandWidth->measurementTime
);
*/

    if ((ulong)(storageBandWidth->measurementTime/1000LL) > 100L)   // to small time values are not reliable, thus accumlate time
    {
      /* calculate average band width */
      averageBandWidth = 0;
      for (z = 0; z < MAX_BAND_WIDTH_MEASUREMENTS; z++)
      {
        averageBandWidth += storageBandWidth->measurements[z];
      }
      averageBandWidth /= MAX_BAND_WIDTH_MEASUREMENTS;
//fprintf(stderr,"%s,%d: averageBandWidth=%lu storageBandWidth->max=%lu deleta=%llu\n",__FILE__,__LINE__,averageBandWidth,storageBandWidth->max,storageBandWidth->measurementTime);

      /* delay if needed, recalculate optimal band width block size */
      if      (averageBandWidth > (storageBandWidth->max+1000*8))
      {
//fprintf(stderr,"%s,%d: -- averageBandWidth=%lu storageBandWidth->max=%lu deleta=%llu\n",__FILE__,__LINE__,averageBandWidth,storageBandWidth->max,storageBandWidth->measurementTime);
        delayTime = ((uint64)(averageBandWidth-storageBandWidth->max)*1000000LL)/averageBandWidth;
//fprintf(stderr,"%s,%d: %llu %llu\n",__FILE__,__LINE__,(storageBandWidth->measurementBytes*8LL*1000000LL)/storageBandWidth->max,storageBandWidth->measurementTime);
        delayTime = (storageBandWidth->measurementBytes*8LL*1000000LL)/storageBandWidth->max-storageBandWidth->measurementTime;
//        if (storageBandWidth->blockSize > 1024) storageBandWidth->blockSize -= 1024;
      }
      else if (averageBandWidth < (storageBandWidth->max-1000*8))
      {
        delayTime = 0LL;
  fprintf(stderr,"%s,%d: ++ averageBandWidth=%lu storageBandWidth->max=%lu deleta=%llu\n",__FILE__,__LINE__,averageBandWidth,storageBandWidth->max,storageBandWidth->measurementTime);
//        storageBandWidth->blockSize += 1024;
      }
else {
        delayTime = 0LL;
fprintf(stderr,"%s,%d: == averageBandWidth=%lu storageBandWidth->max=%lu deleta=%llu\n",__FILE__,__LINE__,averageBandWidth,storageBandWidth->max,storageBandWidth->measurementTime);
}
      if (delayTime > 0) Misc_udelay(delayTime);

      /* calculate bandwidth */
      storageBandWidth->measurements[storageBandWidth->measurementNextIndex] = (ulong)(((uint64)storageBandWidth->measurementBytes*8LL*1000000LL)/(storageBandWidth->measurementTime+delayTime));
//fprintf(stderr,"%s,%d: %lu\n",__FILE__,__LINE__,storageBandWidth->measurements[storageBandWidth->measurementNextIndex]);
      storageBandWidth->measurementNextIndex = (storageBandWidth->measurementNextIndex+1)%MAX_BAND_WIDTH_MEASUREMENTS;

      storageBandWidth->measurementBytes = 0;
      storageBandWidth->measurementTime  = 0;
    }
  }
}
#endif /* HAVE_SSH2 */

/***********************************************************************\
* Name   : requestNewDVD
* Purpose: request new dvd
* Input  : storageFileHandle - storage file handle
*          waitFlag          - TRUE to wait for new dvd
* Output : -
* Return : TRUE if new dvd loaded, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors requestNewDVD(StorageFileHandle *storageFileHandle, bool waitFlag)
{
  TextMacro  textMacros[2];
  bool       dvdRequestedFlag,dvdLoadedFlag;

  TEXT_MACRO_STRING(textMacros[0],"%device",storageFileHandle->device.name          );
  TEXT_MACRO_INT   (textMacros[1],"%number",storageFileHandle->requestedVolumeNumber);

  if (   (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_UNKNOWN)
      || (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_LOADED)
     )
  {
    /* sleep a short time to give hardware time for finishing volume, then unload current volume */
    printInfo(0,"Unload DVD #%d...",storageFileHandle->volumeNumber);
    Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
    Misc_executeCommand(String_cString(globalOptions.dvd.unloadVolumeCommand),
                        textMacros,SIZE_OF_ARRAY(textMacros),
                        NULL,
                        NULL,
                        NULL
                       );
    printInfo(0,"ok\n");

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_UNLOADED;
  }

  /* request new dvd */
  dvdRequestedFlag = FALSE;
  dvdLoadedFlag    = FALSE;
  if      (storageFileHandle->requestVolumeFunction != NULL)
  {
    dvdRequestedFlag = TRUE;

    /* request new volume via call back */
    dvdLoadedFlag = storageFileHandle->requestVolumeFunction(storageFileHandle->requestVolumeUserData,
                                                             storageFileHandle->requestedVolumeNumber
                                                            );

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else if (globalOptions.dvd.requestVolumeCommand != NULL)
  {
    dvdRequestedFlag = TRUE;

    /* request new volume via external command */
    printInfo(0,"Request new DVD #%d...",storageFileHandle->requestedVolumeNumber);
    if (Misc_executeCommand(String_cString(globalOptions.dvd.requestVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            NULL,
                            NULL,
                            NULL
                           ) == ERROR_NONE
       )
    {
      printInfo(0,"ok\n");
      dvdLoadedFlag = TRUE;
    }
    else
    {
      printInfo(0,"FAIL\n");
      dvdLoadedFlag = FALSE;
    }

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else
  {
    if (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_UNLOADED)
    {
      printInfo(0,"Please insert DVD #%d into drive '%s'\n",storageFileHandle->requestedVolumeNumber,String_cString(storageFileHandle->dvd.name));
    }
    if (waitFlag)
    {
      dvdRequestedFlag = TRUE;

      printInfo(0,"<<press ENTER to continue>>\n");
      Misc_waitEnter();

      dvdLoadedFlag = TRUE;
    }

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }

  if (dvdRequestedFlag)
  {
    if (dvdLoadedFlag)
    {
      /* load volume, then sleep a short time to give hardware time for reading volume information */
      printInfo(0,"Load DVD #%d...",storageFileHandle->requestedVolumeNumber);
      Misc_executeCommand(String_cString(globalOptions.dvd.loadVolumeCommand),
                          textMacros,SIZE_OF_ARRAY(textMacros),
                          NULL,
                          NULL,
                          NULL
                         );
      Misc_udelay(LOAD_VOLUME_DELAY_TIME);
      printInfo(0,"ok\n");

      /* store new volume number */
      storageFileHandle->volumeNumber = storageFileHandle->requestedVolumeNumber;

      /* update status info */
      storageFileHandle->runningInfo.volumeNumber = storageFileHandle->volumeNumber;
      updateStatusInfo(storageFileHandle);

      storageFileHandle->volumeState = STORAGE_VOLUME_STATE_LOADED;
    }
    else
    {
      return ERROR_LOAD_VOLUME_FAIL;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : requestNewVolume
* Purpose: request new volume
* Input  : storageFileHandle - storage file handle
*          waitFlag          - TRUE to wait for new volume
* Output : -
* Return : TRUE if new volume loaded, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors requestNewVolume(StorageFileHandle *storageFileHandle, bool waitFlag)
{
  TextMacro textMacros[2];
  bool      volumeRequestedFlag,volumeLoadedFlag;

  TEXT_MACRO_STRING(textMacros[0],"%device",storageFileHandle->device.name          );
  TEXT_MACRO_INT   (textMacros[1],"%number",storageFileHandle->requestedVolumeNumber);

  if (   (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_UNKNOWN)
      || (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_LOADED)
     )
  {
    /* sleep a short time to give hardware time for finishing volume; unload current volume */
    printInfo(0,"Unload volume #%d...",storageFileHandle->volumeNumber);
    Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
    Misc_executeCommand(String_cString(storageFileHandle->device.device.unloadVolumeCommand),
                        textMacros,SIZE_OF_ARRAY(textMacros),
                        NULL,
                        NULL,
                        NULL
                       );
    printInfo(0,"ok\n");

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_UNLOADED;
  }

  /* request new volume */
  volumeRequestedFlag = FALSE;
  volumeLoadedFlag    = FALSE;
  if      (storageFileHandle->requestVolumeFunction != NULL)
  {
    volumeRequestedFlag = TRUE;

    /* request new volume via call back */
    volumeLoadedFlag = storageFileHandle->requestVolumeFunction(storageFileHandle->requestVolumeUserData,
                                                                storageFileHandle->requestedVolumeNumber
                                                               );

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else if (storageFileHandle->device.device.requestVolumeCommand != NULL)
  {
    volumeRequestedFlag = TRUE;

    /* request new volume via external command */
    printInfo(0,"Request new volume #%d...",storageFileHandle->requestedVolumeNumber);
    if (Misc_executeCommand(String_cString(storageFileHandle->device.device.loadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            NULL,
                            NULL,
                            NULL
                           ) == ERROR_NONE
       )
    {
      printInfo(0,"ok\n");
      volumeLoadedFlag = TRUE;
    }
    else
    {
      printInfo(0,"FAIL\n");
      volumeLoadedFlag = FALSE;
    }

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else
  {
    if (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_UNLOADED)
    {
      printInfo(0,"Please insert volume #%d into drive '%s'\n",storageFileHandle->requestedVolumeNumber,storageFileHandle->device.name);
    }
    if (waitFlag)
    {
      volumeRequestedFlag = TRUE;

      printInfo(0,"<<press ENTER to continue>>\n");
      Misc_waitEnter();

      volumeLoadedFlag = TRUE;
    }

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }

  if (volumeRequestedFlag)
  {
    if (volumeLoadedFlag)
    {
      /* load volume; sleep a short time to give hardware time for reading volume information */
      printInfo(0,"Load volume #%d...",storageFileHandle->requestedVolumeNumber);
      Misc_executeCommand(String_cString(storageFileHandle->device.device.loadVolumeCommand),
                          textMacros,SIZE_OF_ARRAY(textMacros),
                          NULL,
                          NULL,
                          NULL
                         );
      Misc_udelay(LOAD_VOLUME_DELAY_TIME);
      printInfo(0,"ok\n");

      /* store new volume number */
      storageFileHandle->volumeNumber = storageFileHandle->requestedVolumeNumber;

      /* update status info */
      storageFileHandle->runningInfo.volumeNumber = storageFileHandle->volumeNumber;
      updateStatusInfo(storageFileHandle);

      storageFileHandle->volumeState = STORAGE_VOLUME_STATE_LOADED;
    }
    else
    {
      return ERROR_LOAD_VOLUME_FAIL;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : processIOmkisofs
* Purpose: process mkisofs output
* Input  : storageFileHandle - storage file handle variable
*          line              - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void processIOmkisofs(StorageFileHandle *storageFileHandle,
                            const String      line
                           )
{
  String s;
  double p;

  s = String_new();
  if (String_match(line,STRING_BEGIN,".* ([0-9\\.]+)% done.*",NULL,s,NULL))
  {
//fprintf(stderr,"%s,%d: mkisofs: %s\n",__FILE__,__LINE__,String_cString(line));
    p = String_toDouble(s,0,NULL,NULL,0);
    storageFileHandle->runningInfo.volumeProgress = ((double)storageFileHandle->dvd.step*100.0+p)/(double)(storageFileHandle->dvd.steps*100);
    updateStatusInfo(storageFileHandle);
  }
  String_delete(s);
}

/***********************************************************************\
* Name   : processIOdvdisaster
* Purpose: process dvdisaster output
* Input  : storageFileHandle - storage file handle variable
*          line              - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void processIOdvdisaster(StorageFileHandle *storageFileHandle,
                               const String      line
                              )
{
  String s;
  double p;

  s = String_new();
  if (String_match(line,STRING_BEGIN,".*adding space\\): +([0-9\\.]+)%",NULL,s,NULL))
  {
//fprintf(stderr,"%s,%d: dvdisaster1: %s\n",__FILE__,__LINE__,String_cString(line));
    p = String_toDouble(s,0,NULL,NULL,0);
    storageFileHandle->runningInfo.volumeProgress = ((double)(storageFileHandle->dvd.step+0)*100.0+p)/(double)(storageFileHandle->dvd.steps*100);
    updateStatusInfo(storageFileHandle);
  }
  if (String_match(line,STRING_BEGIN,".*generation: +([0-9\\.]+)%",NULL,s,NULL))
  {
//fprintf(stderr,"%s,%d: dvdisaster2: %s\n",__FILE__,__LINE__,String_cString(line));
    p = String_toDouble(s,0,NULL,NULL,0);
    storageFileHandle->runningInfo.volumeProgress = ((double)(storageFileHandle->dvd.step+1)*100.0+p)/(double)(storageFileHandle->dvd.steps*100);
    updateStatusInfo(storageFileHandle);
  }

  String_delete(s);
}

/***********************************************************************\
* Name   : processIOgrowisofs
* Purpose: process growisofs output
* Input  : storageFileHandle - storage file handle variable
*          line              - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void processIOgrowisofs(StorageFileHandle *storageFileHandle,
                              const String      line
                             )
{
  String s;
  double p;

  s = String_new();
  if (String_match(line,STRING_BEGIN,".* \\(([0-9\\.]+)%\\) .*",NULL,s,NULL))
  {
//fprintf(stderr,"%s,%d: growisofs2: %s\n",__FILE__,__LINE__,String_cString(line));
    p = String_toDouble(s,0,NULL,NULL,0);
    storageFileHandle->runningInfo.volumeProgress = ((double)storageFileHandle->dvd.step*100.0+p)/(double)(storageFileHandle->dvd.steps*100);
    updateStatusInfo(storageFileHandle);
  }
  String_delete(s);
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

StorageTypes Storage_getType(const String storageName,
                             String       storageSpecifier
                            )
{
  long i;

  if      (String_findCString(storageName,STRING_BEGIN,"ftp:") == 0)
  {
    if (storageSpecifier != NULL) String_sub(storageSpecifier,storageName,4,STRING_END);
    return STORAGE_TYPE_FTP;
  }
  if      (String_findCString(storageName,STRING_BEGIN,"ssh:") == 0)
  {
    if (storageSpecifier != NULL) String_sub(storageSpecifier,storageName,4,STRING_END);
    return STORAGE_TYPE_SSH;
  }
  if (String_findCString(storageName,STRING_BEGIN,"scp:") == 0)
  {
    if (storageSpecifier != NULL) String_sub(storageSpecifier,storageName,4,STRING_END);
    return STORAGE_TYPE_SCP;
  }
  if (String_findCString(storageName,STRING_BEGIN,"sftp:") == 0)
  {
    if (storageSpecifier != NULL) String_sub(storageSpecifier,storageName,5,STRING_END);
    return STORAGE_TYPE_SFTP;
  }
/*
  if (String_findCString(storageName,STRING_BEGIN,"cd:") == 0)
  {
    if (storageSpecifier != NULL) String_sub(storageSpecifier,storageName,3,STRING_END);
    return STORAGE_TYPE_CD;
  }
*/
  if (String_findCString(storageName,STRING_BEGIN,"dvd:") == 0)
  {
    if (storageSpecifier != NULL) String_sub(storageSpecifier,storageName,4,STRING_END);
    return STORAGE_TYPE_DVD;
  }
  i = String_findChar(storageName,STRING_BEGIN,':');
  if (i >= 0)
  {    
    if (storageSpecifier != NULL) String_set(storageSpecifier,storageName);
    return STORAGE_TYPE_DEVICE;
  }

  if (storageSpecifier != NULL) String_set(storageSpecifier,storageName);
  return STORAGE_TYPE_FILESYSTEM;
}

bool Storage_parseFTPSpecifier(const String ftpSpecifier,
                               String       loginName,
                               String       hostName,
                               String       fileName
                              )
{
  long i0,i1;

  assert(ftpSpecifier != NULL);

  /* get user name */
  i0 = 0;
  i1 = String_findChar(ftpSpecifier,i0,'@');
  if (i1 < 0)
  {
    printError("No user name given!\n");
    return FALSE;
  }
  if (loginName != NULL) String_sub(loginName,ftpSpecifier,i0,i1-i0);

  /* get host name */
  i0 = i1+1;
  i1 = String_findChar(ftpSpecifier,i0,':');
  if (i1 < 0)
  {
    printError("No host name given!\n");
    return FALSE;
  }
  if (hostName != NULL) String_sub(hostName,ftpSpecifier,i0,i1-i0);

  /* get file name */
  i0 = i1+1;
  if (fileName != NULL) String_sub(fileName,ftpSpecifier,i0,STRING_END);

  return TRUE;
}

bool Storage_parseSSHSpecifier(const String sshSpecifier,
                               String       loginName,
                               String       hostName,
                               String       fileName
                              )
{
  long i0,i1;

  assert(sshSpecifier != NULL);

  /* get user name */
  i0 = 0;
  i1 = String_findChar(sshSpecifier,i0,'@');
  if (i1 < 0)
  {
    printError("No user name given!\n");
    return FALSE;
  }
  if (loginName != NULL) String_sub(loginName,sshSpecifier,i0,i1-i0);

  /* get host name */
  i0 = i1+1;
  i1 = String_findChar(sshSpecifier,i0,':');
  if (i1 < 0)
  {
    printError("No host name given!\n");
    return FALSE;
  }
  if (hostName != NULL) String_sub(hostName,sshSpecifier,i0,i1-i0);

  /* get file name */
  i0 = i1+1;
  if (fileName != NULL) String_sub(fileName,sshSpecifier,i0,STRING_END);

  return TRUE;
}

bool Storage_parseDeviceSpecifier(const String deviceSpecifier,
                                  const String defaultDeviceName,
                                  String       deviceName,
                                  String       fileName
                                 )
{
  long i;

  assert(deviceSpecifier != NULL);

  i = String_findChar(deviceSpecifier,0,':');
  if (i >= 0)
  {
    /* get device name */
    if (deviceName != NULL)
    {
      if (i > 0)
      {
        String_sub(deviceName,deviceSpecifier,0,i);
      }
      else
      {
        String_set(deviceName,defaultDeviceName);
      }
    }

    /* get file name */
    if (fileName != NULL) String_sub(fileName,deviceSpecifier,i+1,STRING_END);
  }
  else
  {
    /* default device name */
    if (deviceName != NULL) String_set(deviceName,defaultDeviceName);

    /* get file name */
    if (fileName != NULL) String_set(fileName,deviceSpecifier);
  }

  return TRUE;
}

Errors Storage_prepare(const String     storageName,
                       const JobOptions *jobOptions
                      )
{
  String    storageSpecifier;
  #ifdef HAVE_FTP
    FTPServer ftpServer;
  #endif /* HAVE_FTP */
  #ifdef HAVE_SSH2
    SSHServer sshServer;
  #endif /* HAVE_SSH2 */

  assert(storageName != NULL);
  assert(jobOptions != NULL);

  storageSpecifier = String_new();
  switch (Storage_getType(storageName,storageSpecifier))
  {
    case STORAGE_TYPE_FILESYSTEM:
      {
        Errors     error;
        FileHandle fileHandle;

        /* check if file can be created */
        error = File_open(&fileHandle,storageName,FILE_OPENMODE_CREATE);
        if (error != ERROR_NONE)
        {
          return error;
        }
        File_close(&fileHandle);
        File_delete(storageName,FALSE);
      }
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        {
          String     loginName;
          String     hostName;
          String     hostFileName;
          netbuf     *ftpControl;
          Password   *password;
          const char *plainPassword;

          /* parse ssh login specification */
          loginName    = String_new();
          hostName     = String_new();
          hostFileName = String_new();
          if (!Storage_parseFTPSpecifier(storageSpecifier,loginName,hostName,hostFileName))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(loginName);
            String_delete(storageSpecifier);
            return ERROR_FTP_SESSION_FAIL;
          }

          /* initialize password */
          if (!initFTPPassword(jobOptions))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(loginName);
            String_delete(storageSpecifier);
            return ERROR_NO_PASSWORD;
          }

          /* check if FTP login is possible */
          getFTPServer(hostName,jobOptions,&ftpServer);
          if (String_empty(loginName)) String_set(loginName,ftpServer.loginName);
          if (FtpConnect(String_cString(hostName),&ftpControl) != 1)
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(loginName);
            String_delete(storageSpecifier);
            return ERROR_FTP_SESSION_FAIL;

          }
          password = (ftpServer.password != NULL)?ftpServer.password:defaultFTPPassword;
          plainPassword = Password_deploy(password);
          if (FtpLogin(String_cString(loginName),
                       plainPassword,
                       ftpControl
                      ) != 1
             )
          {
            Password_undeploy(password);
            FtpQuit(ftpControl);
            return ERROR_FTP_AUTHENTIFICATION;
          }
          Password_undeploy(password);
          FtpQuit(ftpControl);

          /* free resources */
          String_delete(hostFileName);
          String_delete(hostName);
          String_delete(loginName);
        }
      #else /* not HAVE_FTP */
        UNUSED_VARIABLE(jobOptions);
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
    case STORAGE_TYPE_SCP:
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          String       loginName;
          String       hostName;
          String       hostFileName;
          Errors       error;
          SocketHandle socketHandle;

          /* parse ssh login specification */
          loginName    = String_new();
          hostName     = String_new();
          hostFileName = String_new();
          if (!Storage_parseSSHSpecifier(storageSpecifier,loginName,hostName,hostFileName))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(loginName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* initialize password */
          if (!initSSHPassword(jobOptions))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(loginName);
            String_delete(storageSpecifier);
            return ERROR_NO_PASSWORD;
          }

          /* check if SSH login is possible */
          getSSHServer(hostName,jobOptions,&sshServer);
          if (String_empty(loginName)) String_set(loginName,sshServer.loginName);
          error = Network_connect(&socketHandle,
                                  SOCKET_TYPE_SSH,
                                  hostName,
                                  sshServer.port,
                                  loginName,
                                  (sshServer.password != NULL)?sshServer.password:defaultSSHPassword,
                                  sshServer.publicKeyFileName,
                                  sshServer.privateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(loginName);
            String_delete(storageSpecifier);
            return error;
          }
          Network_disconnect(&socketHandle);

          /* free resources */
          String_delete(hostFileName);
          String_delete(hostName);
          String_delete(loginName);
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(jobOptions);
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      {
        Errors     error;
        FileHandle fileHandle;

        /* check if device can be opened */
        error = File_open(&fileHandle,storageName,FILE_OPENMODE_READ);
        if (error != ERROR_NONE)
        {
          return error;
        }
        File_close(&fileHandle);
        File_delete(storageName,FALSE);
      }
      break;
    case STORAGE_TYPE_DEVICE:
      {
        Errors     error;
        FileHandle fileHandle;

        /* check if device can be opened */
        error = File_open(&fileHandle,storageName,FILE_OPENMODE_READ);
        if (error != ERROR_NONE)
        {
          return error;
        }
        File_close(&fileHandle);
        File_delete(storageName,FALSE);
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  String_delete(storageSpecifier);

  return ERROR_NONE;
}

Errors Storage_init(StorageFileHandle            *storageFileHandle,
                    const String                 storageName,
                    const JobOptions             *jobOptions,
                    StorageRequestVolumeFunction storageRequestVolumeFunction,
                    void                         *storageRequestVolumeUserData,
                    StorageStatusInfoFunction    storageStatusInfoFunction,
                    void                         *storageStatusInfoUserData,
                    String                       fileName
                   )
{
  String    storageSpecifier;
  #ifdef HAVE_FTP
    FTPServer ftpServer;
  #endif /* HAVE_FTP */
  #ifdef HAVE_SSH2
    SSHServer sshServer;
  #endif /* HAVE_SSH2 */

  assert(storageFileHandle != NULL);
  assert(storageName != NULL);
  assert(jobOptions != NULL);
  assert(fileName != NULL);

  storageFileHandle->jobOptions                = jobOptions;
  storageFileHandle->requestVolumeFunction     = storageRequestVolumeFunction;
  storageFileHandle->requestVolumeUserData     = storageRequestVolumeUserData;
  storageFileHandle->storageStatusInfoFunction = storageStatusInfoFunction;
  storageFileHandle->storageStatusInfoUserData = storageStatusInfoUserData;
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

  storageSpecifier = String_new();
  switch (Storage_getType(storageName,storageSpecifier))
  {
    case STORAGE_TYPE_FILESYSTEM:
      /* init variables */
      storageFileHandle->type = STORAGE_TYPE_FILESYSTEM;
      String_set(fileName,storageSpecifier);
      break;
    case STORAGE_TYPE_SSH:
      String_delete(storageSpecifier);
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        {
          uint z;

          /* init variables */
          storageFileHandle->type                    = STORAGE_TYPE_FTP;
          storageFileHandle->ftp.hostName            = String_new();
          storageFileHandle->ftp.loginName           = String_new();
          storageFileHandle->ftp.password            = Password_new();
          storageFileHandle->ftp.bandWidth.max       = globalOptions.maxBandWidth;
          storageFileHandle->ftp.bandWidth.blockSize = 64*1024;
          for (z = 0; z < MAX_BAND_WIDTH_MEASUREMENTS; z++)
          {
            storageFileHandle->ftp.bandWidth.measurements[z] = globalOptions.maxBandWidth;
          }
          storageFileHandle->ftp.bandWidth.measurementNextIndex = 0;
          storageFileHandle->ftp.bandWidth.measurementBytes     = 0;
          storageFileHandle->ftp.bandWidth.measurementTime      = 0;

          /* parse storage string */
          if (!Storage_parseFTPSpecifier(storageSpecifier,
                                         storageFileHandle->ftp.loginName,
                                         storageFileHandle->ftp.hostName,
                                         fileName
                                        )
             )
          {
            Password_delete(storageFileHandle->ftp.password);
            String_delete(storageFileHandle->ftp.loginName);
            String_delete(storageFileHandle->ftp.hostName);
            String_delete(storageSpecifier);
            return ERROR_INVALID_SSH_SPEFICIER;
          }

          /* get FTP server data */
          getFTPServer(storageFileHandle->ftp.hostName,jobOptions,&ftpServer);
          if (String_empty(storageFileHandle->ftp.loginName)) String_set(storageFileHandle->ftp.loginName,ftpServer.loginName);
          Password_set(storageFileHandle->ftp.password,ftpServer.password);

          /* free resources */
        }
      #else /* not HAVE_FTP */
        String_delete(storageSpecifier);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          uint z;

          /* init variables */
          storageFileHandle->type                      = STORAGE_TYPE_SCP;
          storageFileHandle->scp.hostName              = String_new();
          storageFileHandle->scp.hostPort              = 0;
          storageFileHandle->scp.loginName             = String_new();
          storageFileHandle->scp.password              = Password_new();
          storageFileHandle->scp.sshPublicKeyFileName  = NULL;
          storageFileHandle->scp.sshPrivateKeyFileName = NULL;
          storageFileHandle->scp.bandWidth.max         = globalOptions.maxBandWidth;
          storageFileHandle->scp.bandWidth.blockSize   = 64*1024;
          for (z = 0; z < MAX_BAND_WIDTH_MEASUREMENTS; z++)
          {
            storageFileHandle->scp.bandWidth.measurements[z] = globalOptions.maxBandWidth;
          }
          storageFileHandle->scp.bandWidth.measurementNextIndex = 0;
          storageFileHandle->scp.bandWidth.measurementBytes     = 0;
          storageFileHandle->scp.bandWidth.measurementTime      = 0;

          /* parse storage string */
          if (!Storage_parseSSHSpecifier(storageSpecifier,
                                         storageFileHandle->scp.loginName,
                                         storageFileHandle->scp.hostName,
                                         fileName
                                        )
             )
          {
            Password_delete(storageFileHandle->scp.password);
            String_delete(storageFileHandle->scp.loginName);
            String_delete(storageFileHandle->scp.hostName);
            String_delete(storageSpecifier);
            return ERROR_INVALID_SSH_SPEFICIER;
          }

          /* get SSH server data */
          getSSHServer(storageFileHandle->scp.hostName,jobOptions,&sshServer);
          storageFileHandle->scp.hostPort = sshServer.port;
          if (String_empty(storageFileHandle->scp.loginName)) String_set(storageFileHandle->scp.loginName,sshServer.loginName);
          storageFileHandle->scp.sshPublicKeyFileName  = sshServer.publicKeyFileName;
          storageFileHandle->scp.sshPrivateKeyFileName = sshServer.privateKeyFileName;
          Password_set(storageFileHandle->scp.password,sshServer.password);

          /* free resources */
        }
      #else /* not HAVE_SSH2 */
        String_delete(storageSpecifier);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          uint z;

          /* init variables */
          storageFileHandle->type                       = STORAGE_TYPE_SFTP;    
          storageFileHandle->sftp.hostName              = String_new();         
          storageFileHandle->sftp.hostPort              = 0;                    
          storageFileHandle->sftp.loginName             = String_new();         
          storageFileHandle->sftp.password              = Password_new();       
          storageFileHandle->sftp.sshPublicKeyFileName  = NULL;                 
          storageFileHandle->sftp.sshPrivateKeyFileName = NULL;                 
          storageFileHandle->sftp.bandWidth.max         = globalOptions.maxBandWidth;
          storageFileHandle->sftp.bandWidth.blockSize   = 64*1024;
          for (z = 0; z < MAX_BAND_WIDTH_MEASUREMENTS; z++)
          {
            storageFileHandle->sftp.bandWidth.measurements[z] = globalOptions.maxBandWidth;
          }
          storageFileHandle->sftp.bandWidth.measurementNextIndex = 0;
          storageFileHandle->sftp.bandWidth.measurementBytes     = 0;
          storageFileHandle->sftp.bandWidth.measurementTime      = 0;

          /* allocate read-ahead buffer */
          storageFileHandle->sftp.readAheadBuffer.data = (byte*)malloc(MAX_BUFFER_SIZE);
          if (storageFileHandle->sftp.readAheadBuffer.data == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }

          /* parse storage string */
          if (!Storage_parseSSHSpecifier(storageSpecifier,
                                         storageFileHandle->sftp.loginName,
                                         storageFileHandle->sftp.hostName,
                                         fileName
                                        )
             )
          {
            Password_delete(storageFileHandle->sftp.password);
            String_delete(storageFileHandle->sftp.loginName);
            String_delete(storageFileHandle->sftp.hostName);
            String_delete(storageSpecifier);
            return ERROR_INVALID_SSH_SPEFICIER;
          }

          /* get SSH server data */
          getSSHServer(storageFileHandle->sftp.hostName,jobOptions,&sshServer);
          storageFileHandle->sftp.hostPort = sshServer.port;
          if (String_empty(storageFileHandle->sftp.loginName)) String_set(storageFileHandle->sftp.loginName,sshServer.loginName);
          storageFileHandle->sftp.sshPublicKeyFileName  = sshServer.publicKeyFileName;
          storageFileHandle->sftp.sshPrivateKeyFileName = sshServer.privateKeyFileName;
          Password_set(storageFileHandle->sftp.password,sshServer.password);
        }
      #else /* not HAVE_SSH2 */
        String_delete(storageSpecifier);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      {
        FileSystemInfo fileSystemInfo;
        String         deviceName;
        Errors         error;
        String         sourceFileName,fileBaseName,destinationFileName;

        /* parse storage string */
        deviceName = String_new();
        if (!Storage_parseDeviceSpecifier(storageSpecifier,
                                          jobOptions->deviceName,
                                          deviceName,
                                          fileName
                                         )
           )
        {
          String_delete(deviceName);
          String_delete(storageSpecifier);
          return ERROR_INVALID_DEVICE_SPECIFIER;
        }

        /* check space in temporary directory */
        error = File_getFileSystemInfo(globalOptions.tmpDirectory,&fileSystemInfo);
        if (error != ERROR_NONE)
        {
          String_delete(deviceName);
          String_delete(storageSpecifier);
          return error;
        }
        if (fileSystemInfo.freeBytes < (globalOptions.dvd.volumeSize+MAX_DVD_SIZE*(jobOptions->errorCorrectionCodesFlag?2:1)))
        {
          printWarning("Insufficient space in temporary directory '%s' (%.1f%s free, %.1f%s recommended)!\n",
                       String_cString(globalOptions.tmpDirectory),
                       BYTES_SHORT(fileSystemInfo.freeBytes),BYTES_UNIT(fileSystemInfo.freeBytes),
                       BYTES_SHORT((globalOptions.dvd.volumeSize+MAX_DVD_SIZE*(jobOptions->errorCorrectionCodesFlag?2:1))),BYTES_UNIT((globalOptions.dvd.volumeSize+MAX_DVD_SIZE*(jobOptions->errorCorrectionCodesFlag?2:1)))
                      );
        }

        /* init variables */
        storageFileHandle->type             = STORAGE_TYPE_DVD;
        storageFileHandle->dvd.name         = deviceName;
        storageFileHandle->dvd.steps        = jobOptions->errorCorrectionCodesFlag?4:1;
        storageFileHandle->dvd.directory    = String_new();
        storageFileHandle->dvd.volumeSize   = (globalOptions.dvd.volumeSize > 0LL)?globalOptions.dvd.volumeSize:(jobOptions->errorCorrectionCodesFlag?DVD_VOLUME_ECC_SIZE:DVD_VOLUME_SIZE);
        storageFileHandle->dvd.step         = 0;

        if (jobOptions->waitFirstVolumeFlag)
        {
          storageFileHandle->dvd.number  = 0;
          storageFileHandle->dvd.newFlag = TRUE;
        }
        else
        {
          storageFileHandle->dvd.number  = 1;
          storageFileHandle->dvd.newFlag = FALSE;
        }
        StringList_init(&storageFileHandle->dvd.fileNameList);
        storageFileHandle->dvd.fileName     = String_new();
        storageFileHandle->dvd.totalSize    = 0LL;

        /* create temporary directory for DVD files */
        error = File_getTmpDirectoryName(storageFileHandle->dvd.directory,NULL,globalOptions.tmpDirectory);
        if (error != ERROR_NONE)
        {
          String_delete(storageFileHandle->device.fileName);
          String_delete(storageFileHandle->device.directory);
          String_delete(deviceName);
          String_delete(storageSpecifier);
          return error;
        }

        if (!jobOptions->noBAROnDVDFlag)
        {
          /* store a copy of BAR executable on DVD (ignore errors) */
          sourceFileName = String_newCString(globalOptions.barExecutable);
          fileBaseName = File_getFileBaseName(String_new(),sourceFileName);
          destinationFileName = File_appendFileName(String_duplicate(storageFileHandle->dvd.directory),fileBaseName);
          File_copy(sourceFileName,destinationFileName);
          StringList_append(&storageFileHandle->dvd.fileNameList,destinationFileName);
          String_delete(destinationFileName);
          String_delete(fileBaseName);
          String_delete(sourceFileName);
        }

        /* request first DVD */
        storageFileHandle->requestedVolumeNumber = 1;
      }
      break;
    case STORAGE_TYPE_DEVICE:
      {
        FileSystemInfo fileSystemInfo;
        String         deviceName;
        Errors         error;

        /* parse storage string */
        deviceName = String_new();
        if (!Storage_parseDeviceSpecifier(storageSpecifier,
                                          jobOptions->deviceName,
                                          deviceName,
                                          fileName
                                         )
           )
        {
          String_delete(deviceName);
          String_delete(storageSpecifier);
          return ERROR_INVALID_DEVICE_SPECIFIER;
        }

        /* get device */
        getDevice(deviceName,jobOptions,&storageFileHandle->device.device);

        /* check space in temporary directory */
        error = File_getFileSystemInfo(globalOptions.tmpDirectory,&fileSystemInfo);
        if (error != ERROR_NONE)
        {
          String_delete(deviceName);
          String_delete(storageSpecifier);
          return error;
        }
        if (fileSystemInfo.freeBytes < (storageFileHandle->device.device.volumeSize*2))
        {
          printWarning("Insufficient space in temporary directory '%s' (%.1f%s free, %.1f%s recommended)!\n",
                       String_cString(globalOptions.tmpDirectory),
                       BYTES_SHORT(fileSystemInfo.freeBytes),BYTES_UNIT(fileSystemInfo.freeBytes),
                       BYTES_SHORT(storageFileHandle->device.device.volumeSize*2),BYTES_UNIT(storageFileHandle->device.device.volumeSize*2)
                      );
        }

        /* init variables */
        storageFileHandle->type                = STORAGE_TYPE_DEVICE;
        storageFileHandle->device.name         = deviceName;
        storageFileHandle->device.directory    = String_new();

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
        storageFileHandle->device.fileName     = String_new();
        storageFileHandle->device.totalSize    = 0LL;

        /* create temporary directory for device files */
        error = File_getTmpDirectoryName(storageFileHandle->device.directory,NULL,globalOptions.tmpDirectory);
        if (error != ERROR_NONE)
        {
          String_delete(storageFileHandle->device.fileName);
          String_delete(storageFileHandle->device.directory);
          String_delete(deviceName);
          String_delete(storageSpecifier);
          return error;
        }

        /* request first volume for device */
        storageFileHandle->requestedVolumeNumber = 1;
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  String_delete(storageSpecifier);

  storageFileHandle->runningInfo.volumeNumber   = 0;
  storageFileHandle->runningInfo.volumeProgress = 0;

  return ERROR_NONE;
}

Errors Storage_done(StorageFileHandle *storageFileHandle)
{
  assert(storageFileHandle != NULL);

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        Password_delete(storageFileHandle->ftp.password);
        String_delete(storageFileHandle->ftp.loginName);
        String_delete(storageFileHandle->ftp.hostName);
      #else /* not HAVE_FTP */
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        Password_delete(storageFileHandle->scp.password);
        String_delete(storageFileHandle->scp.loginName);
        String_delete(storageFileHandle->scp.hostName);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        free(storageFileHandle->sftp.readAheadBuffer.data);
        Password_delete(storageFileHandle->sftp.password);
        String_delete(storageFileHandle->scp.loginName);
        String_delete(storageFileHandle->sftp.hostName);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      {
        Errors error;
        String fileName;

        /* delete files */
        fileName = String_new();
        while (!StringList_empty(&storageFileHandle->dvd.fileNameList))
        {
          StringList_getFirst(&storageFileHandle->dvd.fileNameList,fileName);
          error = File_delete(fileName,FALSE);
          if (error != ERROR_NONE)
          {
            String_delete(fileName);
            return error;
          }
        }
        String_delete(fileName);

        /* delete temporare directory */
        File_delete(storageFileHandle->dvd.directory,FALSE);

        /* free resources */
        String_delete(storageFileHandle->dvd.fileName);
        String_delete(storageFileHandle->dvd.directory);
        String_delete(storageFileHandle->dvd.name);
      }
      break;
    case STORAGE_TYPE_DEVICE:
      {
        Errors error;
        String fileName;

        /* delete files */
        fileName = String_new();
        while (!StringList_empty(&storageFileHandle->device.fileNameList))
        {
          StringList_getFirst(&storageFileHandle->device.fileNameList,fileName);
          error = File_delete(fileName,FALSE);
          if (error != ERROR_NONE)
          {
            String_delete(fileName);
            return error;
          }
        }
        String_delete(fileName);

        /* delete temporare directory */
        File_delete(storageFileHandle->device.directory,FALSE);

        /* free resources */
        String_delete(storageFileHandle->device.fileName);
        String_delete(storageFileHandle->device.directory);
        String_delete(storageFileHandle->device.name);
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

Errors Storage_preProcess(StorageFileHandle *storageFileHandle)
{
  Errors error;

  assert(storageFileHandle != NULL);

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
      #else /* not HAVE_FTP */
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:    
      /* request next dvd */
      if (storageFileHandle->dvd.newFlag)
      {
        storageFileHandle->dvd.number++;
        storageFileHandle->dvd.newFlag = FALSE;

        storageFileHandle->requestedVolumeNumber = storageFileHandle->dvd.number;
      }

      /* check if new dvd is required */
      if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
      {
        /* request load new DVD */
        error = requestNewDVD(storageFileHandle,FALSE);
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      break;
    case STORAGE_TYPE_DEVICE:
      /* request next volume */
      if (storageFileHandle->device.newFlag)
      {
        storageFileHandle->device.number++;
        storageFileHandle->device.newFlag = FALSE;

        storageFileHandle->requestedVolumeNumber = storageFileHandle->device.number;
      }

      /* check if new volume is required */
      if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
      {
        error = requestNewVolume(storageFileHandle,FALSE);
        if (error != ERROR_NONE)
        {
          return ERROR_LOAD_VOLUME_FAIL;
        }
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

Errors Storage_postProcess(StorageFileHandle *storageFileHandle,
                           bool              finalFlag
                          )
{
  assert(storageFileHandle != NULL);

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
      #else /* not HAVE_FTP */
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      {
        String    imageFileName;
        TextMacro textMacros[5];
        String    fileName;
        Errors    error;
        FileInfo  fileInfo;

        if (finalFlag || (storageFileHandle->dvd.totalSize > storageFileHandle->dvd.volumeSize))
        {
          /* update info */
          storageFileHandle->runningInfo.volumeProgress = 0.0;
          updateStatusInfo(storageFileHandle);

          /* get temporary image file name */
          imageFileName = String_new();
          error = File_getTmpFileName(imageFileName,NULL,globalOptions.tmpDirectory);
          if (error != ERROR_NONE)
          {
            return error;
          }

          /* init macros */
          TEXT_MACRO_STRING(textMacros[0],"%device", storageFileHandle->dvd.name     );
          TEXT_MACRO_STRING(textMacros[1],"%file",   storageFileHandle->dvd.directory);
          TEXT_MACRO_STRING(textMacros[2],"%image",  imageFileName                   );
          TEXT_MACRO_INT   (textMacros[3],"%sectors",0                               );
          TEXT_MACRO_INT   (textMacros[4],"%number", storageFileHandle->volumeNumber );

          if (storageFileHandle->jobOptions->errorCorrectionCodesFlag)
          {
            /* create DVD image */
            printInfo(0,"Make DVD image #%d with %d file(s)...",storageFileHandle->dvd.number,StringList_count(&storageFileHandle->dvd.fileNameList));
            storageFileHandle->dvd.step = 0;
            error = Misc_executeCommand(String_cString(globalOptions.dvd.imageCommand),
                                        textMacros,SIZE_OF_ARRAY(textMacros),
                                        (ExecuteIOFunction)processIOmkisofs,
                                        (ExecuteIOFunction)processIOmkisofs,
                                        storageFileHandle
                                       );
            if (error != ERROR_NONE)
            {
              printInfo(0,"FAIL\n");
              File_delete(imageFileName,FALSE);
              String_delete(imageFileName);
              return error;
            }
            File_getFileInfo(imageFileName,&fileInfo);
            printInfo(0,"ok (%llu bytes)\n",fileInfo.size);

            /* add error-correction codes to DVD image */
            printInfo(0,"Add ECC to image #%d...",storageFileHandle->dvd.number);
            storageFileHandle->dvd.step = 1;
            error = Misc_executeCommand(String_cString(globalOptions.dvd.eccCommand),
                                        textMacros,SIZE_OF_ARRAY(textMacros),
                                        (ExecuteIOFunction)processIOdvdisaster,
                                        (ExecuteIOFunction)processIOdvdisaster,
                                        storageFileHandle
                                       );
            if (error != ERROR_NONE)
            {
              printInfo(0,"FAIL\n");
              File_delete(imageFileName,FALSE);
              String_delete(imageFileName);
              return error;
            }
            File_getFileInfo(imageFileName,&fileInfo);
            printInfo(0,"ok (%llu bytes)\n",fileInfo.size);

            /* get number of image sectors */
            if (File_getFileInfo(imageFileName,&fileInfo) == ERROR_NONE)
            {
              TEXT_MACRO_INT(textMacros[3],"%sectors",(ulong)(fileInfo.size/2048LL));
            }

            /* check if new dvd is required */
            if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
            {
              /* request load new DVD */
              error = requestNewDVD(storageFileHandle,TRUE);
              if (error != ERROR_NONE)
              {
                File_delete(imageFileName,FALSE);
                String_delete(imageFileName);
                return error;
              }
              updateStatusInfo(storageFileHandle);
            }

            /* write to DVD */
            printInfo(0,"Write DVD #%d...",storageFileHandle->dvd.number);
            storageFileHandle->dvd.step = 3;
            error = Misc_executeCommand(String_cString(globalOptions.dvd.writeCommand),
                                        textMacros,SIZE_OF_ARRAY(textMacros),
                                        (ExecuteIOFunction)processIOgrowisofs,
                                        (ExecuteIOFunction)processIOgrowisofs,
                                        storageFileHandle
                                       );
            if (error != ERROR_NONE)
            {
              printInfo(0,"FAIL\n");
              File_delete(imageFileName,FALSE);
              String_delete(imageFileName);
              return error;
            }
            printInfo(0,"ok\n");
          }
          else
          {
            /* check if new dvd is required */
            if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
            {
              /* request load new DVD */
              error = requestNewDVD(storageFileHandle,TRUE);
              if (error != ERROR_NONE)
              {
                File_delete(imageFileName,FALSE);
                String_delete(imageFileName);
                return error;
              }
              updateStatusInfo(storageFileHandle);
            }

            /* write to DVD */
            printInfo(0,"Write DVD #%d with %d file(s)...",storageFileHandle->dvd.number,StringList_count(&storageFileHandle->dvd.fileNameList));
            storageFileHandle->dvd.step = 0;
            error = Misc_executeCommand(String_cString(globalOptions.dvd.writeCommand),
                                        textMacros,SIZE_OF_ARRAY(textMacros),
                                        (ExecuteIOFunction)processIOgrowisofs,
                                        NULL,
                                        storageFileHandle
                                       );
            if (error != ERROR_NONE)
            {
              printInfo(0,"FAIL\n");
              File_delete(imageFileName,FALSE);
              String_delete(imageFileName);
              return error;
            }
            printInfo(0,"ok\n");
          }

          /* delete image */
          File_delete(imageFileName,FALSE);
          String_delete(imageFileName);

          /* update info */
          storageFileHandle->runningInfo.volumeProgress = 1.0;
          updateStatusInfo(storageFileHandle);

          /* delete stored files */
          fileName = String_new();
          while (!StringList_empty(&storageFileHandle->dvd.fileNameList))
          {
            StringList_getFirst(&storageFileHandle->dvd.fileNameList,fileName);
            error = File_delete(fileName,FALSE);
            if (error != ERROR_NONE)
            {
              String_delete(fileName);
              return error;
            }
          }
          String_delete(fileName);

          /* reset */
          storageFileHandle->dvd.newFlag   = TRUE;
          storageFileHandle->dvd.totalSize = 0;
        }
      }
      break;
    case STORAGE_TYPE_DEVICE:
      {
        String    imageFileName;
        TextMacro textMacros[4];
        String    fileName;
        Errors    error;

        if (finalFlag || (storageFileHandle->device.totalSize > storageFileHandle->device.device.volumeSize))
        {
          /* check if new volume is required */
          if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
          {
            error = requestNewVolume(storageFileHandle,TRUE);
            if (error != ERROR_NONE)
            {
              return ERROR_LOAD_VOLUME_FAIL;
            }
            updateStatusInfo(storageFileHandle);
          }

          /* get temporary image file name */
          imageFileName = String_new();
          error = File_getTmpFileName(imageFileName,NULL,globalOptions.tmpDirectory);
          if (error != ERROR_NONE)
          {
            return error;
          }

          /* init macros */
          TEXT_MACRO_STRING(textMacros[0],"%device",storageFileHandle->device.name     );
          TEXT_MACRO_STRING(textMacros[1],"%file",  storageFileHandle->device.directory);
          TEXT_MACRO_STRING(textMacros[2],"%image", imageFileName                      );
          TEXT_MACRO_INT   (textMacros[3],"%number",storageFileHandle->volumeNumber    );

          /* create image */
          if (error == ERROR_NONE)
          {
            printInfo(0,"Make image pre-processing of volume #%d...",storageFileHandle->volumeNumber);
            error = Misc_executeCommand(String_cString(storageFileHandle->device.device.imagePreProcessCommand ),textMacros,SIZE_OF_ARRAY(textMacros),NULL,NULL,NULL);
            printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
          }
          if (error == ERROR_NONE)
          {
            printInfo(0,"Make image volume #%d...",storageFileHandle->volumeNumber);
            error = Misc_executeCommand(String_cString(storageFileHandle->device.device.imageCommand           ),textMacros,SIZE_OF_ARRAY(textMacros),NULL,NULL,NULL);
            printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
          }
          if (error == ERROR_NONE)
          {
            printInfo(0,"Make image post-processing of volume #%d...",storageFileHandle->volumeNumber);
            error = Misc_executeCommand(String_cString(storageFileHandle->device.device.imagePostProcessCommand),textMacros,SIZE_OF_ARRAY(textMacros),NULL,NULL,NULL);
            printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
          }

          /* write onto device */
          if (error == ERROR_NONE)
          {
            printInfo(0,"Write device pre-processing of volume #%d...",storageFileHandle->volumeNumber);
            error = Misc_executeCommand(String_cString(storageFileHandle->device.device.writePreProcessCommand ),textMacros,SIZE_OF_ARRAY(textMacros),NULL,NULL,NULL);
            printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
          }
          if (error == ERROR_NONE)
          {
            printInfo(0,"Write device volume #%d...",storageFileHandle->volumeNumber);
            error = Misc_executeCommand(String_cString(storageFileHandle->device.device.writeCommand           ),textMacros,SIZE_OF_ARRAY(textMacros),NULL,NULL,NULL);
            printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
          }
          if (error == ERROR_NONE)
          {
            printInfo(0,"Write device post-processing of volume #%d...",storageFileHandle->volumeNumber);
            error = Misc_executeCommand(String_cString(storageFileHandle->device.device.writePostProcessCommand),textMacros,SIZE_OF_ARRAY(textMacros),NULL,NULL,NULL);
            printInfo(0,(error == ERROR_NONE)?"ok\n":"FAIL\n");
          }

          if (error != ERROR_NONE)
          {
            File_delete(imageFileName,FALSE);
            String_delete(imageFileName);
            return error;
          }
          File_delete(imageFileName,FALSE);
          String_delete(imageFileName);

          /* delete stored files */
          fileName = String_new();
          while (!StringList_empty(&storageFileHandle->device.fileNameList))
          {
            StringList_getFirst(&storageFileHandle->device.fileNameList,fileName);
            error = File_delete(fileName,FALSE);
            if (error != ERROR_NONE)
            {
              String_delete(fileName);
              return error;
            }
          }
          String_delete(fileName);

          /* reset */
          storageFileHandle->device.newFlag   = TRUE;
          storageFileHandle->device.totalSize = 0;
        }
      }   
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
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

Errors Storage_create(StorageFileHandle *storageFileHandle,
                      const String      fileName,
                      uint64            fileSize,
                      const JobOptions  *jobOptions
                     )
{
  Errors error;
  String directoryName;

  assert(storageFileHandle != NULL);
  assert(fileName != NULL);
  assert(jobOptions != NULL);

  UNUSED_VARIABLE(jobOptions);

  /* init variables */
  storageFileHandle->mode = STORAGE_MODE_WRITE;

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      /* check if archive file exists */
      if (!jobOptions->overwriteArchiveFilesFlag && File_exists(fileName))
      {
        return ERROR_FILE_EXITS;
      }

      /* open file */
      error = File_open(&storageFileHandle->fileSystem.fileHandle,
                        fileName,
                        FILE_OPENMODE_CREATE
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

        /* initialise variables */

        /* connect */
        if (FtpConnect(String_cString(storageFileHandle->ftp.hostName),&storageFileHandle->ftp.control) != 1)
        {
          return ERROR_FTP_SESSION_FAIL;

        }

        /* login */
        plainPassword = Password_deploy(storageFileHandle->ftp.password);
        if (FtpLogin(String_cString(storageFileHandle->ftp.loginName),
                     plainPassword,
                     storageFileHandle->ftp.control
                    ) != 1
           )
        {
          Password_undeploy(storageFileHandle->ftp.password);
          FtpQuit(storageFileHandle->ftp.control);
          return ERROR_FTP_AUTHENTIFICATION;
        }
        Password_undeploy(storageFileHandle->ftp.password);

        /* create file */
        if (FtpAccess(String_cString(fileName),
                      FTPLIB_FILE_WRITE,
                      FTPLIB_IMAGE,
                      storageFileHandle->ftp.control,
                      &storageFileHandle->ftp.data
                     ) != 1
           )
        {
          FtpQuit(storageFileHandle->ftp.control);
          return ERROR_FTP_AUTHENTIFICATION;
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
          /* open network connection */
          error = Network_connect(&storageFileHandle->scp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageFileHandle->scp.hostName,
                                  storageFileHandle->scp.hostPort,
                                  storageFileHandle->scp.loginName,
                                  storageFileHandle->scp.password,
                                  storageFileHandle->scp.sshPublicKeyFileName,
                                  storageFileHandle->scp.sshPrivateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }

          /* open channel and send file */
          storageFileHandle->scp.channel = libssh2_scp_send(Network_getSSHSession(&storageFileHandle->scp.socketHandle),
                                                            String_cString(fileName),
// ???
0600,
                                                            fileSize
                                                           );
          if (storageFileHandle->scp.channel == NULL)
          {
            printError("Init ssh channel fail!\n");
            Network_disconnect(&storageFileHandle->scp.socketHandle);
            return ERROR_SSH_SESSION_FAIL;
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
          /* open network connection */
          error = Network_connect(&storageFileHandle->sftp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageFileHandle->sftp.hostName,
                                  storageFileHandle->sftp.hostPort,
                                  storageFileHandle->sftp.loginName,
                                  storageFileHandle->sftp.password,
                                  storageFileHandle->sftp.sshPublicKeyFileName,
                                  storageFileHandle->sftp.sshPrivateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }

          /* init FTP session */
          storageFileHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageFileHandle->scp.socketHandle));
          if (storageFileHandle->sftp.sftp == NULL)
          {
            printError("Init sftp fail!\n");
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* create FTP file */
          storageFileHandle->sftp.sftpHandle = libssh2_sftp_open(storageFileHandle->sftp.sftp,
                                                                 String_cString(fileName),
                                                                 LIBSSH2_FXF_CREAT|LIBSSH2_FXF_WRITE|LIBSSH2_FXF_TRUNC,
// ???
LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR
                                                                );
          if (storageFileHandle->sftp.sftpHandle == NULL)
          {
            printError("Create sftp file fail!\n");
            libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            return ERROR_SSH_SESSION_FAIL;
          }
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(fileSize);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      /* create file name */
      String_set(storageFileHandle->dvd.fileName,storageFileHandle->dvd.directory);
      File_appendFileName(storageFileHandle->dvd.fileName,fileName);

      /* create directory if not existing */
      directoryName = File_getFilePathName(String_new(),storageFileHandle->dvd.fileName);
      if (!File_exists(directoryName))
      {
        error = File_makeDirectory(directoryName);
        if (error != ERROR_NONE)
        {
          String_delete(directoryName);
          return error;
        }
      }
      String_delete(directoryName);

      /* create file */
      error = File_open(&storageFileHandle->dvd.fileHandle,
                        storageFileHandle->dvd.fileName,
                        FILE_OPENMODE_CREATE
                       );
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    case STORAGE_TYPE_DEVICE:
      /* create file name */
      String_set(storageFileHandle->device.fileName,storageFileHandle->device.directory);
      File_appendFileName(storageFileHandle->device.fileName,fileName);

      /* open file */
      error = File_open(&storageFileHandle->device.fileHandle,
                        storageFileHandle->device.fileName,
                        FILE_OPENMODE_CREATE
                       );
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

Errors Storage_open(StorageFileHandle *storageFileHandle,
                    const String      fileName,
                    const JobOptions  *jobOptions
                   )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(fileName != NULL);
  assert(jobOptions != NULL);

  /* init variables */
  storageFileHandle->mode = STORAGE_MODE_READ;

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      /* open file */
      error = File_open(&storageFileHandle->fileSystem.fileHandle,
                        fileName,
                        FILE_OPENMODE_READ
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

        /* initialise variables */

        /* connect */
        if (FtpConnect(String_cString(storageFileHandle->ftp.hostName),&storageFileHandle->ftp.control) != 1)
        {
          return ERROR_FTP_SESSION_FAIL;

        }

        /* login */
        plainPassword = Password_deploy(storageFileHandle->ftp.password);
        if (FtpLogin(String_cString(storageFileHandle->ftp.loginName),
                     plainPassword,
                     storageFileHandle->ftp.control
                    ) != 1
           )
        {
          Password_undeploy(storageFileHandle->ftp.password);
          FtpQuit(storageFileHandle->ftp.control);
          return ERROR_FTP_AUTHENTIFICATION;
        }
        Password_undeploy(storageFileHandle->ftp.password);

        /* read file */
        if (FtpAccess(String_cString(fileName),
                      FTPLIB_FILE_READ,
                      FTPLIB_IMAGE,
                      storageFileHandle->ftp.control,
                      &storageFileHandle->ftp.data
                     ) != 1
           )
        {
          FtpQuit(storageFileHandle->ftp.control);
          return ERROR_FTP_AUTHENTIFICATION;
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

          /* init variables */
          storageFileHandle->scp.bandWidth.max = globalOptions.maxBandWidth;

          /* open network connection */
          error = Network_connect(&storageFileHandle->scp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageFileHandle->scp.hostName,
                                  storageFileHandle->scp.hostPort,
                                  storageFileHandle->scp.loginName,
                                  storageFileHandle->scp.password,
                                  storageFileHandle->scp.sshPublicKeyFileName,
                                  storageFileHandle->scp.sshPrivateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }

          /* open channel and receive file */
          storageFileHandle->scp.channel = libssh2_scp_recv(Network_getSSHSession(&storageFileHandle->scp.socketHandle),
                                                            String_cString(fileName),
                                                            &fileInfo
                                                           );
          if (storageFileHandle->scp.channel == NULL)
          {
            printError("Init ssh channel fail!\n");
            Network_disconnect(&storageFileHandle->scp.socketHandle);
            return ERROR_SSH_SESSION_FAIL;
          }
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(jobOptions);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;

          /* init variables */
          storageFileHandle->sftp.index                  = 0LL;
          storageFileHandle->sftp.readAheadBuffer.offset = 0LL;
          storageFileHandle->sftp.readAheadBuffer.length = 0L;
          storageFileHandle->sftp.bandWidth.max          = globalOptions.maxBandWidth;
          
          /* open network connection */
          error = Network_connect(&storageFileHandle->sftp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageFileHandle->sftp.hostName,
                                  storageFileHandle->sftp.hostPort,
                                  storageFileHandle->sftp.loginName,
                                  storageFileHandle->sftp.password,
                                  storageFileHandle->sftp.sshPublicKeyFileName,
                                  storageFileHandle->sftp.sshPrivateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }

          /* init FTP session */
          storageFileHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageFileHandle->scp.socketHandle));
          if (storageFileHandle->sftp.sftp == NULL)
          {
            printError("Init sftp fail!\n");
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* open FTP file */
          storageFileHandle->sftp.sftpHandle = libssh2_sftp_open(storageFileHandle->sftp.sftp,
                                                                 String_cString(fileName),
                                                                 LIBSSH2_FXF_READ,
                                                                 0
                                                                );
          if (storageFileHandle->sftp.sftpHandle == NULL)
          {
            printError("Create sftp file fail!\n");
            libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* get file size */
          if (libssh2_sftp_fstat(storageFileHandle->sftp.sftpHandle,
                                 &sftpAttributes
                                ) != 0
             )
          {
            printError("Cannot detect sftp file size!\n");
            libssh2_sftp_close(storageFileHandle->sftp.sftpHandle);
            libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            return ERROR_SSH_SESSION_FAIL;
          }
          storageFileHandle->sftp.size = sftpAttributes.filesize;
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(jobOptions);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      /* init variables */
      /* open file */
#if 0
      error = File_open(&storageFileHandle->fileSystem.fileHandle,
                        storageFileHandle->fileSystem.fileName,
                        FILE_OPENMODE_READ
                       );
      if (error != ERROR_NONE)
      {
        String_delete(storageFileHandle->fileSystem.fileName);
        String_delete(storageSpecifier);
        return error;
      }
#endif /* 0 */
      break;
    case STORAGE_TYPE_DEVICE:
      /* init variables */
      /* open file */
#if 0
      error = File_open(&storageFileHandle->fileSystem.fileHandle,
                        storageFileHandle->fileSystem.fileName,
                        FILE_OPENMODE_READ
                       );
      if (error != ERROR_NONE)
      {
        String_delete(storageFileHandle->fileSystem.fileName);
        String_delete(storageSpecifier);
        return error;
      }
#endif /* 0 */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

void Storage_close(StorageFileHandle *storageFileHandle)
{
  assert(storageFileHandle != NULL);

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      File_close(&storageFileHandle->fileSystem.fileHandle);
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        assert(storageFileHandle->ftp.control != NULL);
        assert(storageFileHandle->ftp.data != NULL);

        FtpClose(storageFileHandle->ftp.data);
        FtpQuit(storageFileHandle->ftp.control);
      #else /* not HAVE_FTP */
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        switch (storageFileHandle->mode)
        {
          case STORAGE_MODE_WRITE:
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
            libssh2_channel_send_eof(storageFileHandle->scp.channel);
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
//            libssh2_channel_wait_eof(storageFileHandle->scp.channel);
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
            libssh2_channel_wait_closed(storageFileHandle->scp.channel);
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
            break;
          case STORAGE_MODE_READ:
            libssh2_channel_close(storageFileHandle->scp.channel);
            libssh2_channel_wait_closed(storageFileHandle->scp.channel);
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
          #endif /* NDEBUG */
        }
        libssh2_channel_free(storageFileHandle->scp.channel);
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
        Network_disconnect(&storageFileHandle->scp.socketHandle);
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        libssh2_sftp_close(storageFileHandle->sftp.sftpHandle);
        libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
        Network_disconnect(&storageFileHandle->sftp.socketHandle);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      storageFileHandle->dvd.totalSize += File_getSize(&storageFileHandle->dvd.fileHandle);
      File_close(&storageFileHandle->dvd.fileHandle);
      StringList_append(&storageFileHandle->dvd.fileNameList,storageFileHandle->dvd.fileName);
      break;
    case STORAGE_TYPE_DEVICE:
      storageFileHandle->device.totalSize += File_getSize(&storageFileHandle->device.fileHandle);
      File_close(&storageFileHandle->device.fileHandle);
      StringList_append(&storageFileHandle->device.fileNameList,storageFileHandle->device.fileName);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
}

bool Storage_eof(StorageFileHandle *storageFileHandle)
{
  assert(storageFileHandle != NULL);

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      return File_eof(&storageFileHandle->fileSystem.fileHandle);
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        return storageFileHandle->sftp.index >= storageFileHandle->sftp.size;
      #else /* not HAVE_SSH2 */
        return TRUE;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      #ifdef HAVE_SSH2
        return File_eof(&storageFileHandle->dvd.fileHandle);
      #else /* not HAVE_SSH2 */
        return TRUE;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DEVICE:
      return File_eof(&storageFileHandle->device.fileHandle);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
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
  assert(buffer != NULL);
  assert(bytesRead != NULL);

//fprintf(stderr,"%s,%d: size=%lu\n",__FILE__,__LINE__,size);
  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      error = File_read(&storageFileHandle->fileSystem.fileHandle,buffer,size,bytesRead);
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        assert(storageFileHandle->ftp.control != NULL);
        assert(storageFileHandle->ftp.data != NULL);

        (*bytesRead) = FtpRead(buffer,size,storageFileHandle->ftp.data);
      #else /* not HAVE_FTP */
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        (*bytesRead) = libssh2_channel_read(storageFileHandle->scp.channel,
                                            buffer,
                                            size
                                           );
      #else /* not HAVE_SSH2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          ulong  i;
          size_t n;

          assert(storageFileHandle->sftp.readAheadBuffer.data != NULL);

          (*bytesRead) = 0;

          /* copy as much as available from read-ahead buffer */
          if (   (storageFileHandle->sftp.index >= storageFileHandle->sftp.readAheadBuffer.offset)
              && (storageFileHandle->sftp.index < (storageFileHandle->sftp.readAheadBuffer.offset+storageFileHandle->sftp.readAheadBuffer.length))
             )
          {
            i = storageFileHandle->sftp.index-storageFileHandle->sftp.readAheadBuffer.offset;
            n = MIN(size,storageFileHandle->sftp.readAheadBuffer.length-i);
            memcpy(buffer,storageFileHandle->sftp.readAheadBuffer.data+i,n);
            buffer = (byte*)buffer+n;
            size -= n;
            (*bytesRead) += n;
            storageFileHandle->sftp.index += n;
          }

          /* read rest of data */
          if (size > 0)
          {
            libssh2_sftp_seek(storageFileHandle->sftp.sftpHandle,storageFileHandle->sftp.index);
            if (size < MAX_BUFFER_SIZE)
            {
              /* read into read-ahead buffer */
              n = libssh2_sftp_read(storageFileHandle->sftp.sftpHandle,
                                   storageFileHandle->sftp.readAheadBuffer.data,
                                   MIN((size_t)(storageFileHandle->sftp.size-storageFileHandle->sftp.index),MAX_BUFFER_SIZE)
                                  );
              if (n < 0)
              {
                return ERROR(IO_ERROR,errno);
              }
              storageFileHandle->sftp.readAheadBuffer.offset = storageFileHandle->sftp.index;
              storageFileHandle->sftp.readAheadBuffer.length = n;
//fprintf(stderr,"%s,%d: n=%ld storageFileHandle->sftp.bufferOffset=%llu storageFileHandle->sftp.bufferLength=%lu\n",__FILE__,__LINE__,n,
//storageFileHandle->sftp.readAheadBuffer.offset,storageFileHandle->sftp.readAheadBuffer.length);

              n = MIN(size,storageFileHandle->sftp.readAheadBuffer.length);
              memcpy(buffer,storageFileHandle->sftp.readAheadBuffer.data,n);
            }
            else
            {
              /* read directlu */
              n = libssh2_sftp_read(storageFileHandle->sftp.sftpHandle,
                                    buffer,
                                    size
                                   );
              if (n < 0)
              {
                return ERROR(IO_ERROR,errno);
              }
            }
            (*bytesRead) += n;
            storageFileHandle->sftp.index += n;
          }
        }
      #else /* not HAVE_SSH2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      error = File_read(&storageFileHandle->dvd.fileHandle,buffer,size,bytesRead);
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    case STORAGE_TYPE_DEVICE:
      error = File_read(&storageFileHandle->device.fileHandle,buffer,size,bytesRead);
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

Errors Storage_write(StorageFileHandle *storageFileHandle,
                     const void        *buffer,
                     ulong             size
                    )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->mode == STORAGE_MODE_WRITE);
  assert(buffer != NULL);

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      error = File_write(&storageFileHandle->fileSystem.fileHandle,buffer,size);
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
        {
          ulong  writtenBytes;
          long   length;
          long   n;
          uint64 startTimestamp,endTimestamp;

          assert(storageFileHandle->ftp.control != NULL);
          assert(storageFileHandle->ftp.data != NULL);

          writtenBytes = 0;
          while (writtenBytes < size)
          {
            /* get start time */
            startTimestamp = Misc_getTimestamp();

            /* send data */
            if (storageFileHandle->ftp.bandWidth.max > 0)
            {
              length = MIN(storageFileHandle->ftp.bandWidth.blockSize,size-writtenBytes);
            }
            else
            {
              length = size-writtenBytes;
            }
            n = FtpWrite((void*)buffer,length,storageFileHandle->ftp.data);
            if (n < 0)
            {
              return ERROR_NETWORK_SEND;
            }
            buffer = (byte*)buffer+n;
            writtenBytes += n;

            /* get end time, transmission time */
            endTimestamp = Misc_getTimestamp();
            assert(endTimestamp >= startTimestamp);

            /* limit used band width if requested */
            limitBandWidth(&storageFileHandle->ftp.bandWidth,n,endTimestamp-startTimestamp);
          };
        }
      #else /* not HAVE_FTP */
      #endif /* HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          ulong  writtenBytes;
          long   length;
          long   n;
          uint64 startTimestamp,endTimestamp;

          writtenBytes = 0;
          while (writtenBytes < size)
          {
            /* get start time */
            startTimestamp = Misc_getTimestamp();

            /* send data */
            if (storageFileHandle->scp.bandWidth.max > 0)
            {
              length = MIN(storageFileHandle->scp.bandWidth.blockSize,size-writtenBytes);
            }
            else
            {
              length = size-writtenBytes;
            }
fprintf(stderr,"%s,%d: %p\n",__FILE__,__LINE__,storageFileHandle->scp.channel);
            do
            {
              n = libssh2_channel_write(storageFileHandle->scp.channel,
                                        buffer,
                                        length
                                       );
fprintf(stderr,"%s,%d: n=%ld\n",__FILE__,__LINE__,n);
            }
            while (n == LIBSSH2_ERROR_EAGAIN);
            if (n < 0)
            {
              return ERROR_NETWORK_SEND;
            }
            buffer = (byte*)buffer+n;
            writtenBytes += n;

            /* get end time, transmission time */
            endTimestamp = Misc_getTimestamp();
            assert(endTimestamp >= startTimestamp);

            /* limit used band width if requested */
fprintf(stderr,"%s,%d: writtenBytes=%ld size=%ld\n",__FILE__,__LINE__,writtenBytes,size);
            limitBandWidth(&storageFileHandle->scp.bandWidth,n,endTimestamp-startTimestamp);
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
          };
        }
      #else /* not HAVE_SSH2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          ulong  writtenBytes;
          long   length;
          long   n;
          uint64 startTimestamp,endTimestamp;

          writtenBytes = 0;
          while (writtenBytes < size)
          {
            /* get start time */
            startTimestamp = Misc_getTimestamp();

            /* send data */
            if (storageFileHandle->sftp.bandWidth.max > 0)
            {
              length = MIN(storageFileHandle->sftp.bandWidth.blockSize,size-writtenBytes);
            }
            else
            {
              length = size-writtenBytes;
            }
            do
            {
              n = libssh2_sftp_write(storageFileHandle->sftp.sftpHandle,
                                     buffer,
                                     length
                                    );
            }
            while (n == LIBSSH2_ERROR_EAGAIN);
            if (n < 0)
            {
              return ERROR_NETWORK_SEND;
            }
            buffer = (byte*)buffer+n;
            size -= n;

            /* get end time, transmission time */
            endTimestamp = Misc_getTimestamp();
            assert(endTimestamp >= startTimestamp);

            /* limit used band width if requested */
            limitBandWidth(&storageFileHandle->sftp.bandWidth,n,endTimestamp-startTimestamp);
          };
        }
      #else /* not HAVE_SSH2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      error = File_write(&storageFileHandle->dvd.fileHandle,buffer,size);
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    case STORAGE_TYPE_DEVICE:
      error = File_write(&storageFileHandle->device.fileHandle,buffer,size);
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

uint64 Storage_getSize(StorageFileHandle *storageFileHandle)
{
  assert(storageFileHandle != NULL);

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      return File_getSize(&storageFileHandle->fileSystem.fileHandle);
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        return storageFileHandle->sftp.size;
      #else /* not HAVE_SSH2 */
        return 0LL;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      #ifdef HAVE_SSH2
        return File_getSize(&storageFileHandle->dvd.fileHandle);
      #else /* not HAVE_SSH2 */
        return 0LL;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DEVICE:
      return File_getSize(&storageFileHandle->device.fileHandle);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return 0;
}

Errors Storage_tell(StorageFileHandle *storageFileHandle,
                    uint64            *offset
                   )
{
  assert(storageFileHandle != NULL);
  assert(offset != NULL);

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      return File_tell(&storageFileHandle->fileSystem.fileHandle,offset);
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        (*offset) = storageFileHandle->sftp.index;
        return ERROR_NONE;
      #else /* not HAVE_SSH2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      #ifdef HAVE_SSH2
        return File_tell(&storageFileHandle->dvd.fileHandle,offset);
      #else /* not HAVE_SSH2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DEVICE:
      return File_tell(&storageFileHandle->device.fileHandle,offset);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

Errors Storage_seek(StorageFileHandle *storageFileHandle,
                    uint64            offset
                   )
{
  assert(storageFileHandle != NULL);

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      return File_seek(&storageFileHandle->fileSystem.fileHandle,offset);
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
//??? large file?
//fprintf(stderr,"%s,%d: seek %llu\n",__FILE__,__LINE__,offset);
        libssh2_sftp_seek(storageFileHandle->sftp.sftpHandle,
                          offset
                         );
        storageFileHandle->sftp.index = offset;
      #else /* not HAVE_SSH2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      #ifdef HAVE_SSH2
        return File_seek(&storageFileHandle->dvd.fileHandle,offset);
      #else /* not HAVE_SSH2 */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DEVICE:
      return File_seek(&storageFileHandle->device.fileHandle,offset);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Storage_openDirectory(StorageDirectoryHandle *storageDirectoryHandle,
                             const String           storageName,
                             const JobOptions       *jobOptions
                            )
{
  String    storageSpecifier;
  #ifdef HAVE_FRP
    FTPServer ftpServer;
  #endif /* HAVE_FRP */
  #ifdef HAVE_SSH2
    SSHServer sshServer;
  #endif /* HAVE_SSH2 */
  Errors    error;

  assert(storageDirectoryHandle != NULL);
  assert(storageName != NULL);
  assert(jobOptions != NULL);

  storageSpecifier = String_new();
  switch (Storage_getType(storageName,storageSpecifier))
  {
    case STORAGE_TYPE_FILESYSTEM:
      /* init variables */
      storageDirectoryHandle->type = STORAGE_TYPE_FILESYSTEM;

      /* open file */
      error = File_openDirectory(&storageDirectoryHandle->fileSystem.directoryHandle,
                                 storageName
                                );
      if (error != ERROR_NONE)
      {
        String_delete(storageSpecifier);
        return error;
      }
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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
          String loginName;
          String hostName;
          String hostFileName;

          /* init variables */
          storageDirectoryHandle->type          = STORAGE_TYPE_SFTP;
          storageDirectoryHandle->sftp.pathName = String_new();

          /* parse storage string */
          loginName    = String_new();
          hostName     = String_new();
          hostFileName = String_new();
          if (!Storage_parseSSHSpecifier(storageSpecifier,loginName,hostName,hostFileName))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(loginName);
            String_delete(storageDirectoryHandle->sftp.pathName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }
          String_set(storageDirectoryHandle->sftp.pathName,hostFileName);

          /* open network connection */
          getSSHServer(hostName,jobOptions,&sshServer);
          if (String_empty(loginName)) String_set(loginName,sshServer.loginName);
          error = Network_connect(&storageDirectoryHandle->sftp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  hostName,
                                  sshServer.port,
                                  loginName,
                                  (sshServer.password != NULL)?sshServer.password:defaultSSHPassword,
                                  sshServer.publicKeyFileName,
                                  sshServer.privateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(loginName);
            String_delete(storageDirectoryHandle->sftp.pathName);
            String_delete(storageSpecifier);
            return error;
          }

          /* init FTP session */
          storageDirectoryHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageDirectoryHandle->sftp.socketHandle));
          if (storageDirectoryHandle->sftp.sftp == NULL)
          {
            printError("Init sftp fail!\n");
            Network_disconnect(&storageDirectoryHandle->sftp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(loginName);
            String_delete(storageDirectoryHandle->sftp.pathName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* open FTP file */
          storageDirectoryHandle->sftp.sftpHandle = libssh2_sftp_opendir(storageDirectoryHandle->sftp.sftp,
                                                                         String_cString(hostFileName)
                                                                        );
          if (storageDirectoryHandle->sftp.sftpHandle == NULL)
          {
            printError("Create sftp file fail!\n");
            libssh2_sftp_shutdown(storageDirectoryHandle->sftp.sftp);
            Network_disconnect(&storageDirectoryHandle->sftp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(loginName);
            String_delete(storageDirectoryHandle->sftp.pathName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(jobOptions);
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      /* init variables */
      storageDirectoryHandle->type = STORAGE_TYPE_DEVICE;

      /* open file */
      error = File_openDirectory(&storageDirectoryHandle->dvd.directoryHandle,
                                 storageName
                                );
      if (error != ERROR_NONE)
      {
        String_delete(storageSpecifier);
        return error;
      }
      break;
    case STORAGE_TYPE_DEVICE:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  String_delete(storageSpecifier);

  return ERROR_NONE;
}

void Storage_closeDirectory(StorageDirectoryHandle *storageDirectoryHandle)
{
  assert(storageDirectoryHandle != NULL);

  switch (storageDirectoryHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      File_closeDirectory(&storageDirectoryHandle->fileSystem.directoryHandle);
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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
        libssh2_sftp_closedir(storageDirectoryHandle->sftp.sftpHandle);
        libssh2_sftp_shutdown(storageDirectoryHandle->sftp.sftp);
        Network_disconnect(&storageDirectoryHandle->sftp.socketHandle);
        String_delete(storageDirectoryHandle->sftp.pathName);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      File_closeDirectory(&storageDirectoryHandle->dvd.directoryHandle);
      break;
    case STORAGE_TYPE_DEVICE:
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
}

bool Storage_endOfDirectory(StorageDirectoryHandle *storageDirectoryHandle)
{
  bool endOfDirectoryFlag;

  assert(storageDirectoryHandle != NULL);

  endOfDirectoryFlag = TRUE;
  switch (storageDirectoryHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      endOfDirectoryFlag = File_endOfDirectory(&storageDirectoryHandle->fileSystem.directoryHandle);
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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
        int                     n;
        LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;

        /* read entry iff not already read */
        if (!storageDirectoryHandle->sftp.entryReadFlag)
        {
          n = libssh2_sftp_readdir(storageDirectoryHandle->sftp.sftpHandle,
                                   storageDirectoryHandle->sftp.buffer,
                                   MAX_FILENAME_LENGTH,
                                   &sftpAttributes
                                  );
          if (n >= 0)
          {
            storageDirectoryHandle->sftp.entryReadFlag = TRUE;
          }
          storageDirectoryHandle->sftp.bufferLength = n;
        }

        endOfDirectoryFlag = !storageDirectoryHandle->sftp.entryReadFlag;
      }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      endOfDirectoryFlag = File_endOfDirectory(&storageDirectoryHandle->dvd.directoryHandle);
      break;
    case STORAGE_TYPE_DEVICE:
      endOfDirectoryFlag = TRUE;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return endOfDirectoryFlag;
}

Errors Storage_readDirectory(StorageDirectoryHandle *storageDirectoryHandle,
                             String                 fileName
                            )
{
  Errors error;

  assert(storageDirectoryHandle != NULL);

  error = ERROR_UNKNOWN;
  switch (storageDirectoryHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      error = File_readDirectory(&storageDirectoryHandle->fileSystem.directoryHandle,fileName);
      break;
    case STORAGE_TYPE_FTP:
      #ifdef HAVE_FTP
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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
        int                     n;
        LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;

        if (!storageDirectoryHandle->sftp.entryReadFlag)
        {
          n = libssh2_sftp_readdir(storageDirectoryHandle->sftp.sftpHandle,
                                   storageDirectoryHandle->sftp.buffer,
                                   MAX_FILENAME_LENGTH,
                                   &sftpAttributes
                                  );
          if (n < 0)
          {
            return ERROR(IO_ERROR,errno);
          }
          storageDirectoryHandle->sftp.bufferLength = n;
        }

        String_set(fileName,storageDirectoryHandle->sftp.pathName);
        File_appendFileNameBuffer(fileName,storageDirectoryHandle->sftp.buffer,storageDirectoryHandle->sftp.bufferLength);

        storageDirectoryHandle->sftp.entryReadFlag = FALSE;
      }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      error = File_readDirectory(&storageDirectoryHandle->dvd.directoryHandle,fileName);
      break;
    case STORAGE_TYPE_DEVICE:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return error;
}
 
#ifdef __cplusplus
  }
#endif

/* end of file */
