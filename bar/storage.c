/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/storage.c,v $
* $Revision: 1.14 $
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

#define UNLOAD_VOLUME_DELAY_TIME 10000 /* [ms] */
#define LOAD_VOLUME_DELAY_TIME   10000 /* [ms] */

#define MAX_CD_SIZE  (700LL*1024LL*1024LL)
#define MAX_DVD_SIZE (4613734LL*1024LL)

#define DVD_VOLUME_SIZE            MAX_DVD_SIZE
#define DVD_VOLUME_ECC_SIZE        (3600LL*1024LL*1024LL)
#define DVD_UNLOAD_VOLUME_COMMAND  "echo eject -r %d"
#define DVD_LOAD_VOLUME_COMMAND    "echo eject -t %d"
#define DVD_WRITE_COMMAND          "echo growisofs -r -Z %d -A BAR -V Backup -volset %n -quiet %f 1>/dev/null 2>/dev/null"
#define DVD_IMAGE_COMMAND          "echo mkisofs -V Backup -volset %n -o %i %f 1>/dev/null 2>/dev/null"
#define DVD_ECC_COMMAND            "echo dvdisaster -mRS02 -n dvd -c -i %i 1>/dev/null 2>/dev/null"
#define DVD_WRITE_IMAGE_COMMAND    "echo growisofs -Z %d -use-the-force-luke=dao:$s 1>/dev/null 2>/dev/null"

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL Password *defaultSSHPassword;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : initSSHPassword
* Purpose: init ssh password
* Input  : -
* Output : -
* Return : TRUE if ssh password intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SSH2
LOCAL bool initSSHPassword(const Options *options)
{
  const char *sshAskPassword;

  assert(options != NULL);

  if (options->defaultSSHServer.password == NULL)
  {
    if ((sshAskPassword = getenv("SSH_ASKPASS")) != NULL)
    {
      /* call external password program */
      FILE *inputHandle;
      int  ch;

      /* open pipe to external password program */
      inputHandle = popen(sshAskPassword,"r");
      if (inputHandle == NULL)
      {
        return FALSE;
      }

      /* read password, discard last LF */
      while ((ch = getc(inputHandle) != EOF) && ((char)ch != '\n'))
      {
        Password_appendChar(defaultSSHPassword,(char)ch);
      }

      /* close pipe */
      pclose(inputHandle);

      return (Password_length(defaultSSHPassword) > 0);
    }
    else 
    {
      return FALSE;
    }
  }
  else
  {
    return TRUE;
  }
}
#endif /* HAVE_SSH2 */

/***********************************************************************\
* Name   : getTimestamp
* Purpose: get timestamp
* Input  : -
* Output : -
* Return : timestamp [us]
* Notes  : -
\***********************************************************************/

LOCAL uint64 getTimestamp(void)
{
  struct timeval tv;

  if (gettimeofday(&tv,NULL) == 0)
  {
//fprintf(stderr,"%s,%d: %ld %ld\n",__FILE__,__LINE__,tv.tv_sec,tv.tv_usec);
    return (uint64)tv.tv_usec+(uint64)tv.tv_sec*1000000LL;
  }
  else
  {
    return 0LL;
  }
}

/***********************************************************************\
* Name   : udelay
* Purpose: delay program execution
* Input  : time - delay time [us]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void udelay(uint64 time)
{
  struct timespec ts;

//fprintf(stderr,"%s,%d: us=%llu\n",__FILE__,__LINE__,time);
  ts.tv_sec  = (ulong)(time/1000000LL);
  ts.tv_nsec = (ulong)((time%1000000LL)*1000);
  while (nanosleep(&ts,&ts) == -1)
  {
  }  
}

/***********************************************************************\
* Name   : delay
* Purpose: delay program execution
* Input  : time - delay time [ms]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void delay(ulong time)
{
  udelay((uint64)time*1000LL);
}

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
      if (delayTime > 0) udelay(delayTime);

      /* calculate bandwidth */
      storageBandWidth->measurements[storageBandWidth->measurementNextIndex] = (ulong)(((uint64)storageBandWidth->measurementBytes*8LL*1000000LL)/(storageBandWidth->measurementTime+delayTime));
//fprintf(stderr,"%s,%d: %lu\n",__FILE__,__LINE__,storageBandWidth->measurements[storageBandWidth->measurementNextIndex]);
      storageBandWidth->measurementNextIndex = (storageBandWidth->measurementNextIndex+1)%MAX_BAND_WIDTH_MEASUREMENTS;

      storageBandWidth->measurementBytes = 0;
      storageBandWidth->measurementTime  = 0;
    }
  }
}

/*---------------------------------------------------------------------*/

Errors Storage_initAll(void)
{
  defaultSSHPassword = Password_new();

  return ERROR_NONE;
}

void Storage_doneAll(void)
{
  Password_delete(defaultSSHPassword);
}

StorageTypes Storage_getType(const String storageName,
                             String       storageSpecifier
                            )
{
  long i;

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

Errors Storage_prepare(const String  storageName,
                       const Options *options
                      )
{
  String    storageSpecifier;
  SSHServer sshServer;

  assert(storageName != NULL);
  assert(options != NULL);

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
          if (!initSSHPassword(options))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(loginName);
            String_delete(storageSpecifier);
            return ERROR_NO_SSH_PASSWORD;
          }

          /* check if ssh login is possible */
          getSSHServer(hostName,options,&sshServer);
          if (String_empty(loginName)) String_set(loginName,sshServer.loginName);
          error = Network_connect(&socketHandle,
                                  SOCKET_TYPE_SSH,
                                  hostName,
                                  sshServer.port,
                                  loginName,
                                  sshServer.publicKeyFileName,
                                  sshServer.privatKeyFileName,
                                  (sshServer.password != NULL)?sshServer.password:defaultSSHPassword,
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
                    const Options                *options,
                    StorageRequestVolumeFunction storageRequestVolumeFunction,
                    void                         *storageRequestVolumeUserData,
                    String                       fileName
                   )
{
  String    storageSpecifier;
  SSHServer sshServer;

  assert(storageFileHandle != NULL);
  assert(storageName != NULL);
  assert(options != NULL);
  assert(fileName != NULL);

  storageFileHandle->options               = options;
  storageFileHandle->requestVolumeFunction = storageRequestVolumeFunction;
  storageFileHandle->requestVolumeUserData = storageRequestVolumeUserData;
  storageFileHandle->volumeNumber          = 0;
  storageFileHandle->requestedVolumeNumber = 0;

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
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          uint z;

          /* init variables */
          storageFileHandle->type                     = STORAGE_TYPE_SCP;
          storageFileHandle->scp.hostName             = String_new();
          storageFileHandle->scp.hostPort             = 0;
          storageFileHandle->scp.sshLoginName         = String_new();
          storageFileHandle->scp.sshPublicKeyFileName = NULL;
          storageFileHandle->scp.sshPrivatKeyFileName = NULL;
          storageFileHandle->scp.sshPassword          = Password_new();
          storageFileHandle->scp.bandWidth.max        = options->maxBandWidth;
          storageFileHandle->scp.bandWidth.blockSize  = 64*1024;
          for (z = 0; z < MAX_BAND_WIDTH_MEASUREMENTS; z++)
          {
            storageFileHandle->scp.bandWidth.measurements[z] = options->maxBandWidth;
          }
          storageFileHandle->scp.bandWidth.measurementNextIndex = 0;
          storageFileHandle->scp.bandWidth.measurementBytes     = 0;
          storageFileHandle->scp.bandWidth.measurementTime      = 0;

          /* parse storage string */
          if (!Storage_parseSSHSpecifier(storageSpecifier,
                                         storageFileHandle->scp.sshLoginName,
                                         storageFileHandle->scp.hostName,
                                         fileName
                                        )
             )
          {
            Password_delete(storageFileHandle->scp.sshPassword);
            String_delete(storageFileHandle->scp.sshLoginName);
            String_delete(storageFileHandle->scp.hostName);
            String_delete(storageSpecifier);
            return ERROR_INVALID_SSH_SPEFICIER;
          }

          /* get ssh server data */
          getSSHServer(storageFileHandle->scp.hostName,options,&sshServer);
          storageFileHandle->scp.hostPort = sshServer.port;
          if (String_empty(storageFileHandle->scp.sshLoginName)) String_set(storageFileHandle->scp.sshLoginName,sshServer.loginName);
          storageFileHandle->scp.sshPublicKeyFileName = sshServer.publicKeyFileName;
          storageFileHandle->scp.sshPrivatKeyFileName = sshServer.privatKeyFileName;
          Password_set(storageFileHandle->scp.sshPassword,sshServer.password);

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
          storageFileHandle->type                      = STORAGE_TYPE_SFTP;    
          storageFileHandle->sftp.hostName             = String_new();         
          storageFileHandle->sftp.hostPort             = 0;                    
          storageFileHandle->sftp.sshLoginName         = String_new();         
          storageFileHandle->sftp.sshPublicKeyFileName = NULL;                 
          storageFileHandle->sftp.sshPrivatKeyFileName = NULL;                 
          storageFileHandle->sftp.sshPassword          = Password_new();       
          storageFileHandle->sftp.bandWidth.max        = options->maxBandWidth;
          storageFileHandle->sftp.bandWidth.blockSize  = 64*1024;
          for (z = 0; z < MAX_BAND_WIDTH_MEASUREMENTS; z++)
          {
            storageFileHandle->sftp.bandWidth.measurements[z] = options->maxBandWidth;
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
                                         storageFileHandle->sftp.sshLoginName,
                                         storageFileHandle->sftp.hostName,
                                         fileName
                                        )
             )
          {
            Password_delete(storageFileHandle->sftp.sshPassword);
            String_delete(storageFileHandle->sftp.sshLoginName);
            String_delete(storageFileHandle->sftp.hostName);
            String_delete(storageSpecifier);
            return ERROR_INVALID_SSH_SPEFICIER;
          }

          /* get ssh server data */
          getSSHServer(storageFileHandle->sftp.hostName,options,&sshServer);
          storageFileHandle->sftp.hostPort = sshServer.port;
          if (String_empty(storageFileHandle->sftp.sshLoginName)) String_set(storageFileHandle->sftp.sshLoginName,sshServer.loginName);
          storageFileHandle->sftp.sshPublicKeyFileName = sshServer.publicKeyFileName;
          storageFileHandle->sftp.sshPrivatKeyFileName = sshServer.privatKeyFileName;
          Password_set(storageFileHandle->sftp.sshPassword,sshServer.password);
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

        /* parse storage string */
        deviceName = String_new();
        if (!Storage_parseDeviceSpecifier(storageSpecifier,
                                          options->deviceName,
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
        getDevice(deviceName,options,&storageFileHandle->dvd.device);

        /* check space in temporary directory */
        error = File_getFileSystemInfo(options->tmpDirectory,&fileSystemInfo);
        if (error != ERROR_NONE)
        {
          String_delete(deviceName);
          String_delete(storageSpecifier);
          return error;
        }
        if (fileSystemInfo.freeBytes < (storageFileHandle->dvd.device.volumeSize+MAX_DVD_SIZE*(options->errorCorrectionCodesFlag?2:1)))
        {
          printWarning("Insufficient space in temporary directory '%s' (%.1f%s free, %.1f%s recommended)!\n",
                       String_cString(options->tmpDirectory),
                       BYTES_SHORT(fileSystemInfo.freeBytes),BYTES_UNIT(fileSystemInfo.freeBytes),
                       BYTES_SHORT((storageFileHandle->dvd.device.volumeSize+MAX_DVD_SIZE*(options->errorCorrectionCodesFlag?2:1))),BYTES_UNIT((storageFileHandle->dvd.device.volumeSize+MAX_DVD_SIZE*(options->errorCorrectionCodesFlag?2:1)))
                      );
        }

        /* init variables */
        storageFileHandle->type             = STORAGE_TYPE_DVD;
        StringList_init(&storageFileHandle->device.fileNameList);
        storageFileHandle->dvd.name         = deviceName;
        storageFileHandle->dvd.steps        = options->errorCorrectionCodesFlag?3:1;
        storageFileHandle->dvd.directory    = String_new();
        storageFileHandle->dvd.volumeSize   = (storageFileHandle->dvd.device.volumeSize >= 0LL)?storageFileHandle->dvd.device.volumeSize:(options->errorCorrectionCodesFlag?DVD_VOLUME_ECC_SIZE:DVD_VOLUME_SIZE);
        storageFileHandle->dvd.volumeNumber = 0;
        storageFileHandle->dvd.fileName     = String_new();
        storageFileHandle->dvd.totalSize    = 0LL;

        /* create temporary directory for DVD files */
        error = File_getTmpDirectoryName(options->tmpDirectory,storageFileHandle->dvd.directory);
        if (error != ERROR_NONE)
        {
          String_delete(storageFileHandle->device.fileName);
          String_delete(storageFileHandle->device.directory);
          String_delete(deviceName);
          String_delete(storageSpecifier);
          return error;
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
                                          options->deviceName,
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
        getDevice(deviceName,options,&storageFileHandle->device.device);

        /* check space in temporary directory */
        error = File_getFileSystemInfo(options->tmpDirectory,&fileSystemInfo);
        if (error != ERROR_NONE)
        {
          String_delete(deviceName);
          String_delete(storageSpecifier);
          return error;
        }
        if (fileSystemInfo.freeBytes < (storageFileHandle->device.device.volumeSize*2))
        {
          printWarning("Insufficient space in temporary directory '%s' (%.1f%s free, %.1f%s recommended)!\n",
                       String_cString(options->tmpDirectory),
                       BYTES_SHORT(fileSystemInfo.freeBytes),BYTES_UNIT(fileSystemInfo.freeBytes),
                       BYTES_SHORT(storageFileHandle->device.device.volumeSize*2),BYTES_UNIT(storageFileHandle->device.device.volumeSize*2)
                      );
        }

        /* init variables */
        storageFileHandle->type                = STORAGE_TYPE_DEVICE;
        StringList_init(&storageFileHandle->device.fileNameList);
        storageFileHandle->device.name         = deviceName;
        storageFileHandle->device.directory    = String_new();
        storageFileHandle->device.volumeNumber = 0;
        storageFileHandle->device.fileName     = String_new();
        storageFileHandle->device.totalSize    = 0LL;

        /* create temporary directory for device files */
        error = File_getTmpDirectoryName(options->tmpDirectory,storageFileHandle->device.directory);
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
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        Password_delete(storageFileHandle->scp.sshPassword);
        String_delete(storageFileHandle->scp.sshLoginName);
        String_delete(storageFileHandle->scp.hostName);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        free(storageFileHandle->sftp.readAheadBuffer.data);
        Password_delete(storageFileHandle->sftp.sshPassword);
        String_delete(storageFileHandle->scp.sshLoginName);
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
  assert(storageFileHandle != NULL);

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
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
        ExecuteMacro executeMacros[2];
        const char   *command;
        bool         volumeLoadedFlag;

        /* check if new volume is required */
        if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
        {
          executeMacros[0].type = EXECUTE_MACRO_TYPE_STRING; executeMacros[0].name = "%d"; executeMacros[0].string = storageFileHandle->device.name;
          executeMacros[1].type = EXECUTE_MACRO_TYPE_INT;    executeMacros[1].name = "%n"; executeMacros[1].i      = storageFileHandle->requestedVolumeNumber;

          /* sleep a short time to give hardware time for finishing volume, then unload current volume */
          delay(UNLOAD_VOLUME_DELAY_TIME);
          command = (storageFileHandle->dvd.device.unloadVolumeCommand != NULL)?String_cString(storageFileHandle->dvd.device.unloadVolumeCommand):DVD_UNLOAD_VOLUME_COMMAND;
          Misc_executeCommand(command,executeMacros,SIZE_OF_ARRAY(executeMacros),"Unload DVD");

          /* request new volume */
          if      (storageFileHandle->requestVolumeFunction != NULL)
          {
            /* request new volume via call back */
            volumeLoadedFlag = storageFileHandle->requestVolumeFunction(storageFileHandle->requestVolumeUserData,
                                                                        storageFileHandle->requestedVolumeNumber
                                                                       );
          }
          else if (storageFileHandle->dvd.device.requestVolumeCommand != NULL)
          {
            /* request new volume via external command */
            volumeLoadedFlag = (Misc_executeCommand(String_cString(storageFileHandle->dvd.device.unloadVolumeCommand),executeMacros,SIZE_OF_ARRAY(executeMacros),"Request new DVD") == ERROR_NONE);
          }
          else
          {
            volumeLoadedFlag = FALSE;
          }

          /* load volume, then sleep a short time to give hardware time for reading volume information */
          command = (storageFileHandle->dvd.device.loadVolumeCommand != NULL)?String_cString(storageFileHandle->dvd.device.unloadVolumeCommand):DVD_UNLOAD_VOLUME_COMMAND;
          Misc_executeCommand(command,executeMacros,SIZE_OF_ARRAY(executeMacros),"Load DVD");
          delay(LOAD_VOLUME_DELAY_TIME);

          if (volumeLoadedFlag)
          {
            storageFileHandle->volumeNumber = storageFileHandle->requestedVolumeNumber;
          }
          else
          {
            return ERROR_LOAD_VOLUME_FAIL;
          }
        }
      }
      break;
    case STORAGE_TYPE_DEVICE:
      {
        ExecuteMacro executeMacros[2];
        bool         volumeLoadedFlag;

        /* check if new volume is required */
        if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
        {
          executeMacros[0].type = EXECUTE_MACRO_TYPE_STRING; executeMacros[0].name = "%d"; executeMacros[0].string = storageFileHandle->device.name;
          executeMacros[1].type = EXECUTE_MACRO_TYPE_INT;    executeMacros[1].name = "%n"; executeMacros[1].i      = storageFileHandle->requestedVolumeNumber;

          /* sleep a short time to give hardware time for finishing volume; unload current volume */
          delay(UNLOAD_VOLUME_DELAY_TIME);
          Misc_executeCommand(String_cString(storageFileHandle->dvd.device.unloadVolumeCommand),executeMacros,SIZE_OF_ARRAY(executeMacros),"Unload volume");

          /* request new volume */
          if      (storageFileHandle->requestVolumeFunction != NULL)
          {
            /* request new volume via call back */
            volumeLoadedFlag = storageFileHandle->requestVolumeFunction(storageFileHandle->requestVolumeUserData,
                                                                        storageFileHandle->requestedVolumeNumber
                                                                       );
          }
          else if (storageFileHandle->dvd.device.requestVolumeCommand != NULL)
          {
            /* request new volume via external command */
            volumeLoadedFlag = (Misc_executeCommand(String_cString(storageFileHandle->dvd.device.loadVolumeCommand),executeMacros,SIZE_OF_ARRAY(executeMacros),"Request new volume") == ERROR_NONE);
          }
          else
          {
            volumeLoadedFlag = FALSE;
          }

          /* load volume; sleep a short time to give hardware time for reading volume information */
          Misc_executeCommand(String_cString(storageFileHandle->dvd.device.loadVolumeCommand),executeMacros,SIZE_OF_ARRAY(executeMacros),"Load volume");
          delay(LOAD_VOLUME_DELAY_TIME);

          if (volumeLoadedFlag)
          {
            storageFileHandle->volumeNumber = storageFileHandle->requestedVolumeNumber;
          }
          else
          {
            return ERROR_LOAD_VOLUME_FAIL;
          }
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
        String       imageFileName;
        ExecuteMacro executeMacros[3];
        const char    *command;
        String       fileName;
        Errors       error;

        if (finalFlag || (storageFileHandle->device.totalSize > storageFileHandle->dvd.volumeSize))
        {
          /* get temporary image file name */
          imageFileName = String_new();
          error = File_getTmpFileName(storageFileHandle->options->tmpDirectory,imageFileName);
          if (error != ERROR_NONE)
          {
            return error;
          }

          /* init macros */
          executeMacros[0].type = EXECUTE_MACRO_TYPE_STRING; executeMacros[0].name = "%d"; executeMacros[0].string = storageFileHandle->dvd.name;
          executeMacros[1].type = EXECUTE_MACRO_TYPE_STRING; executeMacros[1].name = "%f"; executeMacros[1].string = storageFileHandle->dvd.directory;
          executeMacros[2].type = EXECUTE_MACRO_TYPE_STRING; executeMacros[2].name = "%i"; executeMacros[2].string = imageFileName;
          executeMacros[3].type = EXECUTE_MACRO_TYPE_INT;    executeMacros[3].name = "%n"; executeMacros[3].i      = storageFileHandle->dvd.volumeNumber;

          if (storageFileHandle->options->errorCorrectionCodesFlag)
          {
            /* create DVD image */
            command = (storageFileHandle->dvd.device.imageCommand != NULL)?String_cString(storageFileHandle->dvd.device.imageCommand):DVD_IMAGE_COMMAND;
            if (error == ERROR_NONE) error = Misc_executeCommand(command,executeMacros,SIZE_OF_ARRAY(executeMacros),"Make DVD image #%d",storageFileHandle->volumeNumber);

            /* add error-correction codes to DVD image */
            command = (storageFileHandle->dvd.device.eccCommand != NULL)?String_cString(storageFileHandle->dvd.device.eccCommand):DVD_ECC_COMMAND;
            if (error == ERROR_NONE) error = Misc_executeCommand(command,executeMacros,SIZE_OF_ARRAY(executeMacros),"Make DVD image #%d",storageFileHandle->volumeNumber);

            /* write to DVD */
            command = (storageFileHandle->dvd.device.writeCommand != NULL)?String_cString(storageFileHandle->dvd.device.writeCommand):DVD_WRITE_IMAGE_COMMAND;
            if (error == ERROR_NONE) error = Misc_executeCommand(command,executeMacros,SIZE_OF_ARRAY(executeMacros),"Write DVD #%d",storageFileHandle->volumeNumber);
          }
          else
          {
            /* write to DVD */
            command = (storageFileHandle->dvd.device.writeCommand != NULL)?String_cString(storageFileHandle->dvd.device.writeCommand):DVD_WRITE_COMMAND;
            if (error == ERROR_NONE) error = Misc_executeCommand(command,executeMacros,SIZE_OF_ARRAY(executeMacros),"Write DVD #%d",storageFileHandle->volumeNumber);
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

          storageFileHandle->dvd.totalSize = 0;

          /* request next volume */
          storageFileHandle->requestedVolumeNumber++;
        }
      }
      break;
    case STORAGE_TYPE_DEVICE:
      {
        String       imageFileName;
        ExecuteMacro executeMacros[3];
        String       fileName;
        Errors       error;

        if (finalFlag || (storageFileHandle->device.totalSize > storageFileHandle->device.device.volumeSize))
        {
          /* get temporary image file name */
          imageFileName = String_new();
          error = File_getTmpFileName(storageFileHandle->options->tmpDirectory,imageFileName);
          if (error != ERROR_NONE)
          {
            return error;
          }

          /* init macros */
          executeMacros[0].type = EXECUTE_MACRO_TYPE_STRING; executeMacros[0].name = "%d"; executeMacros[0].string = storageFileHandle->device.name;
          executeMacros[1].type = EXECUTE_MACRO_TYPE_STRING; executeMacros[1].name = "%f"; executeMacros[1].string = storageFileHandle->device.directory;
          executeMacros[2].type = EXECUTE_MACRO_TYPE_STRING; executeMacros[2].name = "%i"; executeMacros[2].string = imageFileName;
          executeMacros[3].type = EXECUTE_MACRO_TYPE_INT;    executeMacros[3].name = "%n"; executeMacros[3].i      = storageFileHandle->device.volumeNumber;

          /* create image */
          if (error == ERROR_NONE) error = Misc_executeCommand(String_cString(storageFileHandle->device.device.imagePreProcessCommand ),executeMacros,SIZE_OF_ARRAY(executeMacros),"Make image pre-processing of volume #%d",storageFileHandle->volumeNumber);
          if (error == ERROR_NONE) error = Misc_executeCommand(String_cString(storageFileHandle->device.device.imageCommand           ),executeMacros,SIZE_OF_ARRAY(executeMacros),"Make image volume #%d",storageFileHandle->volumeNumber);
          if (error == ERROR_NONE) error = Misc_executeCommand(String_cString(storageFileHandle->device.device.imagePostProcessCommand),executeMacros,SIZE_OF_ARRAY(executeMacros),"Make image post-processing of volume #%d",storageFileHandle->volumeNumber);

          /* write onto device */
          if (error == ERROR_NONE) error = Misc_executeCommand(String_cString(storageFileHandle->device.device.writePreProcessCommand ),executeMacros,SIZE_OF_ARRAY(executeMacros),"Write device pre-processing of volume #%d",storageFileHandle->volumeNumber);
          if (error == ERROR_NONE) error = Misc_executeCommand(String_cString(storageFileHandle->device.device.writeCommand           ),executeMacros,SIZE_OF_ARRAY(executeMacros),"Write device volume #%d",storageFileHandle->volumeNumber);
          if (error == ERROR_NONE) error = Misc_executeCommand(String_cString(storageFileHandle->device.device.writePostProcessCommand),executeMacros,SIZE_OF_ARRAY(executeMacros),"Write device post-processing of volume #%d",storageFileHandle->volumeNumber);

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

          storageFileHandle->device.totalSize = 0;

          /* request next volume */
          storageFileHandle->requestedVolumeNumber++;
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
                      const Options     *options
                     )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(fileName != NULL);
  assert(options != NULL);
  
  /* init variables */
  storageFileHandle->mode = STORAGE_MODE_WRITE;

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
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
                                  storageFileHandle->scp.sshLoginName,
                                  storageFileHandle->scp.sshPublicKeyFileName,
                                  storageFileHandle->scp.sshPrivatKeyFileName,
                                  storageFileHandle->scp.sshPassword,
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
                                  storageFileHandle->sftp.sshLoginName,
                                  storageFileHandle->sftp.sshPublicKeyFileName,
                                  storageFileHandle->sftp.sshPrivatKeyFileName,
                                  storageFileHandle->sftp.sshPassword,
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
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_DVD:
      /* create file name */
      String_set(storageFileHandle->dvd.fileName,storageFileHandle->dvd.directory);
      File_appendFileName(storageFileHandle->dvd.fileName,fileName);

      /* open file */
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
                    const Options     *options
                   )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(fileName != NULL);
  assert(options != NULL);

  /* init variables */
  storageFileHandle->mode          = STORAGE_MODE_READ;

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
    case STORAGE_TYPE_SSH:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          struct stat fileInfo;

          /* init variables */
          storageFileHandle->scp.bandWidth.max = options->maxBandWidth;

          /* open network connection */
          error = Network_connect(&storageFileHandle->scp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageFileHandle->scp.hostName,
                                  storageFileHandle->scp.hostPort,
                                  storageFileHandle->scp.sshLoginName,
                                  storageFileHandle->scp.sshPublicKeyFileName,
                                  storageFileHandle->scp.sshPrivatKeyFileName,
                                  storageFileHandle->scp.sshPassword,
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
        String_delete(storageSpecifier);
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
          storageFileHandle->sftp.bandWidth.max          = options->maxBandWidth;
          
          /* open network connection */
          error = Network_connect(&storageFileHandle->sftp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageFileHandle->sftp.hostName,
                                  storageFileHandle->sftp.hostPort,
                                  storageFileHandle->sftp.sshLoginName,
                                  storageFileHandle->sftp.sshPublicKeyFileName,
                                  storageFileHandle->sftp.sshPrivatKeyFileName,
                                  storageFileHandle->sftp.sshPassword,
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
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
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
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        switch (storageFileHandle->mode)
        {
          case STORAGE_MODE_WRITE:
            libssh2_channel_send_eof(storageFileHandle->scp.channel);
            libssh2_channel_wait_eof(storageFileHandle->scp.channel);
            libssh2_channel_wait_closed(storageFileHandle->scp.channel);
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
        Network_disconnect(&storageFileHandle->scp.socketHandle);
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
    case STORAGE_TYPE_SSH:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      break;
    case STORAGE_TYPE_SCP:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      break;
    case STORAGE_TYPE_SFTP:
      return storageFileHandle->sftp.index >= storageFileHandle->sftp.size;
      break;
    case STORAGE_TYPE_DVD:
      return File_eof(&storageFileHandle->dvd.fileHandle);
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
        return ERROR_FUNTION_NOT_SUPPORTED;
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
                return ERROR_IO_ERROR;
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
                return ERROR_IO_ERROR;
              }
            }
            (*bytesRead) += n;
            storageFileHandle->sftp.index += n;
          }
        }
      #else /* not HAVE_SSH2 */
        return ERROR_FUNTION_NOT_SUPPORTED;
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
    case STORAGE_TYPE_SSH:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          ulong  writtenBytes;
          long   n;
          uint64 startTimestamp,endTimestamp;

          writtenBytes = 0;
          while (writtenBytes < size)
          {
            /* get start time */
            startTimestamp = getTimestamp();

            /* send data */
            if (storageFileHandle->scp.bandWidth.max > 0)
            {
              n = MIN(storageFileHandle->scp.bandWidth.blockSize,size-writtenBytes);
            }
            else
            {
              n = size-writtenBytes;
            }
            n = libssh2_channel_write(storageFileHandle->scp.channel,
                                      buffer,
                                      n
                                     );
            if (n < 0)
            {
              return ERROR_NETWORK_SEND;
            }
            buffer = (byte*)buffer+n;
            writtenBytes += n;
            storageFileHandle->sftp.index += (uint64)n;

            /* get end time, transmission time */
            endTimestamp = getTimestamp();
            assert(endTimestamp >= startTimestamp);

            /* limit used band width if requested */
            limitBandWidth(&storageFileHandle->scp.bandWidth,n,endTimestamp-startTimestamp);
          };
        }
      #else /* not HAVE_SSH2 */
        return ERROR_FUNTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          ulong  writtenBytes;
          long   n;
          uint64 startTimestamp,endTimestamp;

          writtenBytes = 0;
          while (writtenBytes < size)
          {
            /* get start time */
            startTimestamp = getTimestamp();

            /* send data */
            if (storageFileHandle->sftp.bandWidth.max > 0)
            {
              n = MIN(storageFileHandle->sftp.bandWidth.blockSize,size-writtenBytes);
            }
            else
            {
              n = size-writtenBytes;
            }
            n = libssh2_sftp_write(storageFileHandle->sftp.sftpHandle,
                                   buffer,
                                   n
                                  );
            if (n < 0)
            {
              return ERROR_NETWORK_SEND;
            }
            buffer = (byte*)buffer+n;
            size -= n;

            /* get end time, transmission time */
            endTimestamp = getTimestamp();
            assert(endTimestamp >= startTimestamp);

            /* limit used band width if requested */
            limitBandWidth(&storageFileHandle->sftp.bandWidth,n,endTimestamp-startTimestamp);
          };
        }
      #else /* not HAVE_SSH2 */
        return ERROR_FUNTION_NOT_SUPPORTED;
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
    case STORAGE_TYPE_SSH:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      break;
    case STORAGE_TYPE_SCP:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      break;
    case STORAGE_TYPE_SFTP:
      return storageFileHandle->sftp.size;
      break;
    case STORAGE_TYPE_DVD:
      return File_getSize(&storageFileHandle->dvd.fileHandle);
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
    case STORAGE_TYPE_SSH:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      break;
    case STORAGE_TYPE_SCP:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SFTP:
      (*offset) = storageFileHandle->sftp.index;
      return ERROR_NONE;
      break;
    case STORAGE_TYPE_DVD:
      return File_tell(&storageFileHandle->dvd.fileHandle,offset);
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
    case STORAGE_TYPE_SSH:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      break;
    case STORAGE_TYPE_SCP:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SFTP:
//??? large file?
//fprintf(stderr,"%s,%d: seek %llu\n",__FILE__,__LINE__,offset);
      libssh2_sftp_seek(storageFileHandle->sftp.sftpHandle,
                        offset
                       );
      storageFileHandle->sftp.index = offset;
      break;
    case STORAGE_TYPE_DVD:
      return File_seek(&storageFileHandle->dvd.fileHandle,offset);
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
                             const Options          *options
                            )
{
  String    storageSpecifier;
  SSHServer sshServer;
  Errors    error;

  assert(storageDirectoryHandle != NULL);
  assert(storageName != NULL);
  assert(options != NULL);

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
    case STORAGE_TYPE_SSH:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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
          getSSHServer(hostName,options,&sshServer);
          if (String_empty(loginName)) String_set(loginName,sshServer.loginName);
          error = Network_connect(&storageDirectoryHandle->sftp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  hostName,
                                  sshServer.port,
                                  loginName,
                                  sshServer.publicKeyFileName,
                                  sshServer.privatKeyFileName,
                                  (sshServer.password != NULL)?sshServer.password:defaultSSHPassword,
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
    case STORAGE_TYPE_SSH:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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
    case STORAGE_TYPE_SSH:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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
    case STORAGE_TYPE_SSH:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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
            return ERROR_IO_ERROR;
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
