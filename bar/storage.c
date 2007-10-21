/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/storage.c,v $
* $Revision: 1.11 $
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

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define MAX_BUFFER_SIZE     (4*1024)
#define MAX_FILENAME_LENGTH (4*1024)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL Password *defaultSSHPassword;
LOCAL String   defaultSSHPublicKeyFileName;
LOCAL String   defaultSSHPrivatKeyFileName;

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

  if (options->sshPassword == NULL)
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
* Name   : initSSHSession
* Purpose: initialize a SSH session
* Input  : socketHandle - network socket handle
*          userName     - user name
*          options      - options
* Output : session - initialized session
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SSH2
LOCAL Errors initSSHSession(SocketHandle    *socketHandle,
                            const String    userName,
                            const Options   *options,
                            LIBSSH2_SESSION **session
                           )
{
  const char *password;

  assert(socketHandle != NULL);
  assert(userName != NULL);
  assert(options != NULL);
  assert(session != NULL);

  /* init session */
  (*session) = libssh2_session_init();
  if ((*session) == NULL)
  {
    return ERROR_SSH_SESSION_FAIL;
  }
  if (libssh2_session_startup(*session,
                              Network_getSocket(socketHandle)
                             ) != 0
     )
  {
    libssh2_session_disconnect(*session,"");
    libssh2_session_free(*session);
    return ERROR_SSH_SESSION_FAIL;
  }

#if 1
  password = Password_deploy((options->sshPassword != NULL)?options->sshPassword:defaultSSHPassword);
  if (libssh2_userauth_publickey_fromfile(*session,
// NYI
                                          String_cString(userName),
                                          String_cString((options->sshPublicKeyFileName != NULL)?options->sshPublicKeyFileName:defaultSSHPublicKeyFileName),
                                          String_cString((options->sshPrivatKeyFileName != NULL)?options->sshPrivatKeyFileName:defaultSSHPrivatKeyFileName),
                                          password
                                         ) != 0
     )
  {
    Password_undeploy((options->sshPassword != NULL)?options->sshPassword:defaultSSHPassword);
    libssh2_session_disconnect(*session,"");
    libssh2_session_free(*session);
    return ERROR_SSH_AUTHENTIFICATION;
  }
  Password_undeploy((options->sshPassword != NULL)?options->sshPassword:defaultSSHPassword);
#else
  if (libssh2_userauth_keyboard_interactive(*session,
                                            String_cString(userName),
                                            NULL
                                          ) != 0
     )
  {
    libssh2_session_disconnect(*session,"");
    libssh2_session_free(*session);
    return ERROR_SSH_AUTHENTIFICATION;
  }
#endif /* 0 */

  return ERROR_NONE;
}
#endif /* HAVE_SSH2 */

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SSH2
LOCAL void doneSSHSession(LIBSSH2_SESSION *session)
{
  assert(session != NULL);

  libssh2_session_disconnect(session,"");
  libssh2_session_free(session);
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
* Name   : delay
* Purpose: delay program execution
* Input  : time - delay time [us]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void delay(uint64 time)
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
* Name   : limitBandWidth
* Purpose: limit used band width
* Input  : storageFileHandle - storage file handle
*          transmittedBytes  - transmitted bytes
*          transmissionTime  - time for transmission
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void limitBandWidth(StorageFileHandle *storageFileHandle,
                          ulong             transmittedBytes,
                          uint64            transmissionTime
                         )
{
  uint   z;
  ulong  averageBandWidth;
  uint64 delayTime;

  if (storageFileHandle->bandWidth.max > 0)
  {
    storageFileHandle->bandWidth.measurementBytes += transmittedBytes;
    storageFileHandle->bandWidth.measurementTime += transmissionTime;
/*
fprintf(stderr,"%s,%d: storageFileHandle->bandWidth.blockSize=%ld sum=%lu %llu\n",__FILE__,__LINE__,
storageFileHandle->bandWidth.blockSize,
storageFileHandle->bandWidth.measurementBytes,
storageFileHandle->bandWidth.measurementTime
);
*/

    if ((ulong)(storageFileHandle->bandWidth.measurementTime/1000LL) > 100L)   // to small time values are not reliable, thus accumlate time
    {
      /* calculate average band width */
      averageBandWidth = 0;
      for (z = 0; z < MAX_BAND_WIDTH_MEASUREMENTS; z++)
      {
        averageBandWidth += storageFileHandle->bandWidth.measurements[z];
      }
      averageBandWidth /= MAX_BAND_WIDTH_MEASUREMENTS;
//fprintf(stderr,"%s,%d: averageBandWidth=%lu storageFileHandle->bandWidth.max=%lu deleta=%llu\n",__FILE__,__LINE__,averageBandWidth,storageFileHandle->bandWidth.max,storageFileHandle->bandWidth.measurementTime);

      /* delay if needed, recalculate optimal band width block size */
      if      (averageBandWidth > (storageFileHandle->bandWidth.max+1000*8))
      {
//fprintf(stderr,"%s,%d: -- averageBandWidth=%lu storageFileHandle->bandWidth.max=%lu deleta=%llu\n",__FILE__,__LINE__,averageBandWidth,storageFileHandle->bandWidth.max,storageFileHandle->bandWidth.measurementTime);
        delayTime = ((uint64)(averageBandWidth-storageFileHandle->bandWidth.max)*1000000LL)/averageBandWidth;
//fprintf(stderr,"%s,%d: %llu %llu\n",__FILE__,__LINE__,(storageFileHandle->bandWidth.measurementBytes*8LL*1000000LL)/storageFileHandle->bandWidth.max,storageFileHandle->bandWidth.measurementTime);
        delayTime = (storageFileHandle->bandWidth.measurementBytes*8LL*1000000LL)/storageFileHandle->bandWidth.max-storageFileHandle->bandWidth.measurementTime;
//        if (storageFileHandle->bandWidth.blockSize > 1024) storageFileHandle->bandWidth.blockSize -= 1024;
      }
      else if (averageBandWidth < (storageFileHandle->bandWidth.max-1000*8))
      {
        delayTime = 0LL;
  fprintf(stderr,"%s,%d: ++ averageBandWidth=%lu storageFileHandle->bandWidth.max=%lu deleta=%llu\n",__FILE__,__LINE__,averageBandWidth,storageFileHandle->bandWidth.max,storageFileHandle->bandWidth.measurementTime);
//        storageFileHandle->bandWidth.blockSize += 1024;
      }
else {
        delayTime = 0LL;
fprintf(stderr,"%s,%d: == averageBandWidth=%lu storageFileHandle->bandWidth.max=%lu deleta=%llu\n",__FILE__,__LINE__,averageBandWidth,storageFileHandle->bandWidth.max,storageFileHandle->bandWidth.measurementTime);
}
      if (delayTime > 0) delay(delayTime);

      /* calculate bandwidth */
      storageFileHandle->bandWidth.measurements[storageFileHandle->bandWidth.measurementNextIndex] = (ulong)(((uint64)storageFileHandle->bandWidth.measurementBytes*8LL*1000000LL)/(storageFileHandle->bandWidth.measurementTime+delayTime));
//fprintf(stderr,"%s,%d: %lu\n",__FILE__,__LINE__,storageFileHandle->bandWidth.measurements[storageFileHandle->bandWidth.measurementNextIndex]);
      storageFileHandle->bandWidth.measurementNextIndex = (storageFileHandle->bandWidth.measurementNextIndex+1)%MAX_BAND_WIDTH_MEASUREMENTS;

      storageFileHandle->bandWidth.measurementBytes = 0;
      storageFileHandle->bandWidth.measurementTime  = 0;
    }
  }
}

/*---------------------------------------------------------------------*/

Errors Storage_init(void)
{
  defaultSSHPassword          = Password_new();
  defaultSSHPublicKeyFileName = String_new();
  defaultSSHPrivatKeyFileName = String_new();
  File_setFileNameCString(defaultSSHPublicKeyFileName,getenv("HOME"));
  File_appendFileNameCString(defaultSSHPublicKeyFileName,".ssh");
  File_appendFileNameCString(defaultSSHPublicKeyFileName,"id_rsa.pub");
  File_setFileNameCString(defaultSSHPrivatKeyFileName,getenv("HOME"));
  File_appendFileNameCString(defaultSSHPrivatKeyFileName,".ssh");
  File_appendFileNameCString(defaultSSHPrivatKeyFileName,"id_rsa");

  return ERROR_NONE;
}

void Storage_done(void)
{
  String_delete(defaultSSHPrivatKeyFileName);
  String_delete(defaultSSHPublicKeyFileName);
  Password_delete(defaultSSHPassword);
}

StorageTypes Storage_getType(const String storageName, String storageSpecifier)
{
  if      (String_findCString(storageName,STRING_BEGIN,"ssh:") == 0)
  {
    if (storageSpecifier != NULL) String_sub(storageSpecifier,storageName,4,STRING_END);
    return STORAGE_TYPE_SSH;
  }
  else if (String_findCString(storageName,STRING_BEGIN,"scp:") == 0)
  {
    if (storageSpecifier != NULL) String_sub(storageSpecifier,storageName,4,STRING_END);
    return STORAGE_TYPE_SCP;
  }
  else if (String_findCString(storageName,STRING_BEGIN,"sftp:") == 0)
  {
    if (storageSpecifier != NULL) String_sub(storageSpecifier,storageName,5,STRING_END);
    return STORAGE_TYPE_SFTP;
  }
  else
  {
    if (storageSpecifier != NULL) String_set(storageSpecifier,storageName);
    return STORAGE_TYPE_FILESYSTEM;
  }
}

bool Storage_parseSSHSpecifier(const String sshSpecifier,
                               String       userName,
                               String       hostName,
                               String       hostFileName
                              )
{
  long i0,i1;

  assert(sshSpecifier != NULL);

  /* parse connection string */
  i0 = 0;
  i1 = String_findChar(sshSpecifier,i0,'@');
  if (i1 < 0)
  {
    printError("No user name given in 'scp' string!\n");
    return FALSE;
  }
  if (userName != NULL) String_sub(userName,sshSpecifier,i0,i1-i0);
  i0 = i1+1;
  i1 = String_findChar(sshSpecifier,i0,':');
  if (i1 < 0)
  {
    printError("No host name given in 'scp' string!\n");
    return FALSE;
  }
  if (hostName != NULL) String_sub(hostName,sshSpecifier,i0,i1-i0);
  i0 = i1+1;
  if (hostFileName != NULL) String_sub(hostFileName,sshSpecifier,i0,STRING_END);

  return TRUE;
}

Errors Storage_prepare(const String  storageName,
                       const Options *options
                      )
{
  String storageSpecifier;

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
        File_delete(storageName);
      }
      break;
    case STORAGE_TYPE_SSH:
    case STORAGE_TYPE_SCP:
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          String          userName;
          String          hostName;
          String          hostFileName;
          Errors          error;
          SocketHandle    socketHandle;
          LIBSSH2_SESSION *session;

          /* parse ssh login specification */
          userName     = String_new();
          hostName     = String_new();
          hostFileName = String_new();
          if (!Storage_parseSSHSpecifier(storageSpecifier,userName,hostName,hostFileName))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* initialize password */
          if (!initSSHPassword(options))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_NO_SSH_PASSWORD;
          }

          /* check if ssh login is possible */
          error = Network_connect(&socketHandle,hostName,options->sshPort,0);
          if (error != ERROR_NONE)
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return error;
          }
          error = initSSHSession(&socketHandle,
                                 userName,
                                 options,
                                 &session
                                );
          if (error != ERROR_NONE)
          {
            Network_disconnect(&socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return error;
          }
          doneSSHSession(session);
          Network_disconnect(&socketHandle);

          /* free resources */
          String_delete(hostFileName);
          String_delete(hostName);
          String_delete(userName);
        }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
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

Errors Storage_create(StorageFileHandle *storageFileHandle,
                      const String      storageName,
                      uint64            fileSize,
                      const Options     *options
                     )
{
  uint   z;
  String storageSpecifier;
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageName != NULL);
  assert(options != NULL);

  /* init variables */
  storageFileHandle->mode                = STORAGE_MODE_WRITE;
  storageFileHandle->bandWidth.max       = options->maxBandWidth;
  storageFileHandle->bandWidth.blockSize = 64*1024;
  for (z = 0; z < MAX_BAND_WIDTH_MEASUREMENTS; z++)
  {
    storageFileHandle->bandWidth.measurements[z] = options->maxBandWidth;
  }
  storageFileHandle->bandWidth.measurementNextIndex = 0;
  storageFileHandle->bandWidth.measurementBytes     = 0;
  storageFileHandle->bandWidth.measurementTime      = 0;

  storageSpecifier = String_new();
  switch (Storage_getType(storageName,storageSpecifier))
  {
    case STORAGE_TYPE_FILESYSTEM:
      /* init variables */
      storageFileHandle->type                = STORAGE_TYPE_FILESYSTEM;
      storageFileHandle->fileSystem.fileName = String_copy(storageSpecifier);

      /* open file */
      error = File_open(&storageFileHandle->fileSystem.fileHandle,
                        storageFileHandle->fileSystem.fileName,
                        FILE_OPENMODE_CREATE
                       );
      if (error != ERROR_NONE)
      {
        String_delete(storageFileHandle->fileSystem.fileName);
        String_delete(storageSpecifier);
        return error;
      }
      break;
    case STORAGE_TYPE_SSH:
      String_delete(storageSpecifier);
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          String userName;
          String hostName;
          String hostFileName;

          /* init variables */
          storageFileHandle->type = STORAGE_TYPE_SCP;

          /* parse connection string */
          userName     = String_new();
          hostName     = String_new();
          hostFileName = String_new();
          if (!Storage_parseSSHSpecifier(storageSpecifier,userName,hostName,hostFileName))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* open network connection */
          error = Network_connect(&storageFileHandle->scp.socketHandle,
                                  hostName,
                                  options->sshPort,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return error;
          }

          /* init session */
          error = initSSHSession(&storageFileHandle->scp.socketHandle,
                                 userName,
                                 options,
                                 &storageFileHandle->scp.session
                                );
          if (error != ERROR_NONE)
          {
            Network_disconnect(&storageFileHandle->scp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return error;
          }

          /* open channel */
          storageFileHandle->scp.channel = libssh2_scp_send(storageFileHandle->scp.session,
                                                            String_cString(hostFileName),
// ???
0600,
                                                            fileSize
                                                           );
          if (storageFileHandle->scp.channel == NULL)
          {
            printError("Init ssh channel fail!\n");
            doneSSHSession(storageFileHandle->sftp.session);
            Network_disconnect(&storageFileHandle->scp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* free resources */
          String_delete(hostFileName);
          String_delete(hostName);
          String_delete(userName);
        }
      #else /* not HAVE_SSH2 */
        String_delete(storageSpecifier);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          String userName;
          String hostName;
          String hostFileName;

          /* init variables */
          storageFileHandle->type = STORAGE_TYPE_SFTP;

          /* parse connection string */
          userName     = String_new();
          hostName     = String_new();
          hostFileName = String_new();
          if (!Storage_parseSSHSpecifier(storageSpecifier,userName,hostName,hostFileName))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* open network connection */
          error = Network_connect(&storageFileHandle->sftp.socketHandle,
                                  hostName,
                                  options->sshPort,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return error;
          }

          /* init session */
          error = initSSHSession(&storageFileHandle->sftp.socketHandle,
                                 userName,
                                 options,
                                 &storageFileHandle->sftp.session
                                );
          if (error != ERROR_NONE)
          {
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return error;
          }

          /* open FTP session */
          storageFileHandle->sftp.sftp = libssh2_sftp_init(storageFileHandle->sftp.session);
          if (storageFileHandle->sftp.sftp == NULL)
          {
            printError("Init sftp fail!\n");
            doneSSHSession(storageFileHandle->sftp.session);
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* open FTP file */
          storageFileHandle->sftp.sftpHandle = libssh2_sftp_open(storageFileHandle->sftp.sftp,
                                                                 String_cString(hostFileName),
                                                                 LIBSSH2_FXF_CREAT|LIBSSH2_FXF_WRITE|LIBSSH2_FXF_TRUNC,
// ???
LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR
                                                                );
          if (storageFileHandle->sftp.sftpHandle == NULL)
          {
            printError("Create sftp file fail!\n");
            libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
            doneSSHSession(storageFileHandle->sftp.session);
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* free resources */
          String_delete(hostFileName);
          String_delete(hostName);
          String_delete(userName);
        }
      #else /* not HAVE_SSH2 */
        String_delete(storageSpecifier);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
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

Errors Storage_open(StorageFileHandle *storageFileHandle,
                    const String      storageName,
                    const Options     *options
                   )
{
  String storageSpecifier;
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageName != NULL);
  assert(options != NULL);

  /* init variables */
  storageFileHandle->mode          = STORAGE_MODE_READ;
  storageFileHandle->bandWidth.max = options->maxBandWidth;

  storageSpecifier = String_new();
  switch (Storage_getType(storageName,storageSpecifier))
  {
    case STORAGE_TYPE_FILESYSTEM:
      /* init variables */
      storageFileHandle->type                = STORAGE_TYPE_FILESYSTEM;
      storageFileHandle->fileSystem.fileName = String_copy(storageSpecifier);

      /* open file */
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
      break;
    case STORAGE_TYPE_SSH:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      String_delete(storageSpecifier);
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          String      userName;
          String      hostName;
          String      hostFileName;
          struct stat fileInfo;

          /* init variables */
          storageFileHandle->type = STORAGE_TYPE_SCP;

          /* parse connection string */
          userName     = String_new();
          hostName     = String_new();
          hostFileName = String_new();
          if (!Storage_parseSSHSpecifier(storageSpecifier,userName,hostName,hostFileName))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* open network connection */
          error = Network_connect(&storageFileHandle->scp.socketHandle,
                                  hostName,
                                  options->sshPort,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return error;
          }

          /* init session */
          error = initSSHSession(&storageFileHandle->scp.socketHandle,
                                 userName,
                                 options,
                                 &storageFileHandle->scp.session
                                );
          if (error != ERROR_NONE)
          {
            Network_disconnect(&storageFileHandle->scp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return error;
          }

          /* open channel */
          storageFileHandle->scp.channel = libssh2_scp_recv(storageFileHandle->scp.session,
                                                          String_cString(hostFileName),
                                                          &fileInfo
                                                         );
          if (storageFileHandle->scp.channel == NULL)
          {
            printError("Init ssh channel fail!\n");
            doneSSHSession(storageFileHandle->sftp.session);
            Network_disconnect(&storageFileHandle->scp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* free resources */
          String_delete(hostFileName);
          String_delete(hostName);
          String_delete(userName);
        }
      #else /* not HAVE_SSH2 */
        String_delete(storageSpecifier);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          String                  userName;
          String                  hostName;
          String                  hostFileName;
          LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;

          /* init variables */
          storageFileHandle->type                        = STORAGE_TYPE_SFTP;
          storageFileHandle->sftp.index                  = 0LL;
          storageFileHandle->sftp.readAheadBuffer.offset = 0LL;
          storageFileHandle->sftp.readAheadBuffer.length = 0L;
          
          storageFileHandle->sftp.readAheadBuffer.data = (byte*)malloc(MAX_BUFFER_SIZE);
          if (storageFileHandle->sftp.readAheadBuffer.data == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }

          /* parse connection string */
          userName     = String_new();
          hostName     = String_new();
          hostFileName = String_new();
          if (!Storage_parseSSHSpecifier(storageSpecifier,userName,hostName,hostFileName))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* open network connection */
          error = Network_connect(&storageFileHandle->sftp.socketHandle,
                                  hostName,
                                  options->sshPort,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return error;
          }

          /* init session */
          error = initSSHSession(&storageFileHandle->sftp.socketHandle,
                                 userName,
                                 options,
                                 &storageFileHandle->sftp.session
                                );
          if (error != ERROR_NONE)
          {
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return error;
          }

          /* open FTP session */
          storageFileHandle->sftp.sftp = libssh2_sftp_init(storageFileHandle->sftp.session);
          if (storageFileHandle->sftp.sftp == NULL)
          {
            printError("Init sftp fail!\n");
            doneSSHSession(storageFileHandle->sftp.session);
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* open FTP file */
          storageFileHandle->sftp.sftpHandle = libssh2_sftp_open(storageFileHandle->sftp.sftp,
                                                                 String_cString(hostFileName),
                                                                 LIBSSH2_FXF_READ,
                                                                 0
                                                                );
          if (storageFileHandle->sftp.sftpHandle == NULL)
          {
            printError("Create sftp file fail!\n");
            libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
            doneSSHSession(storageFileHandle->sftp.session);
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
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
            doneSSHSession(storageFileHandle->sftp.session);
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }
          storageFileHandle->sftp.size = sftpAttributes.filesize;

          /* free resources */
          String_delete(hostFileName);
          String_delete(hostName);
          String_delete(userName);
        }
      #else /* not HAVE_SSH2 */
        String_delete(storageSpecifier);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
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

void Storage_close(StorageFileHandle *storageFileHandle)
{
  assert(storageFileHandle != NULL);

  switch (storageFileHandle->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
      File_close(&storageFileHandle->fileSystem.fileHandle);
      String_delete(storageFileHandle->fileSystem.fileName);
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
        doneSSHSession(storageFileHandle->sftp.session);
        Network_disconnect(&storageFileHandle->scp.socketHandle);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        libssh2_sftp_close(storageFileHandle->sftp.sftpHandle);
        libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
        doneSSHSession(storageFileHandle->sftp.session);
        Network_disconnect(&storageFileHandle->sftp.socketHandle);
        free(storageFileHandle->sftp.readAheadBuffer.data);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
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
            if (storageFileHandle->bandWidth.max > 0)
            {
              n = MIN(storageFileHandle->bandWidth.blockSize,size-writtenBytes);
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
            limitBandWidth(storageFileHandle,n,endTimestamp-startTimestamp);
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
            if (storageFileHandle->bandWidth.max > 0)
            {
              n = MIN(storageFileHandle->bandWidth.blockSize,size-writtenBytes);
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
            limitBandWidth(storageFileHandle,n,endTimestamp-startTimestamp);
          };
        }
      #else /* not HAVE_SSH2 */
        return ERROR_FUNTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
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
  String storageSpecifier;
  Errors error;

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
          String userName;
          String hostName;
          String hostFileName;

          /* init variables */
          storageDirectoryHandle->type          = STORAGE_TYPE_SFTP;
          storageDirectoryHandle->sftp.pathName = String_new();

          /* parse connection string */
          userName     = String_new();
          hostName     = String_new();
          hostFileName = String_new();
          if (!Storage_parseSSHSpecifier(storageSpecifier,userName,hostName,hostFileName))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageDirectoryHandle->sftp.pathName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }
          String_set(storageDirectoryHandle->sftp.pathName,hostFileName);

          /* open network connection */
          error = Network_connect(&storageDirectoryHandle->sftp.socketHandle,
                                  hostName,
                                  options->sshPort,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageDirectoryHandle->sftp.pathName);
            String_delete(storageSpecifier);
            return error;
          }

          /* init session */
          error = initSSHSession(&storageDirectoryHandle->sftp.socketHandle,
                                 userName,
                                 options,
                                 &storageDirectoryHandle->sftp.session
                                );
          if (error != ERROR_NONE)
          {
            Network_disconnect(&storageDirectoryHandle->sftp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageDirectoryHandle->sftp.pathName);
            String_delete(storageSpecifier);
            return error;
          }

          /* open FTP session */
          storageDirectoryHandle->sftp.sftp = libssh2_sftp_init(storageDirectoryHandle->sftp.session);
          if (storageDirectoryHandle->sftp.sftp == NULL)
          {
            printError("Init sftp fail!\n");
            doneSSHSession(storageDirectoryHandle->sftp.session);
            Network_disconnect(&storageDirectoryHandle->sftp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
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
            doneSSHSession(storageDirectoryHandle->sftp.session);
            Network_disconnect(&storageDirectoryHandle->sftp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageDirectoryHandle->sftp.pathName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }
        }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
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
        doneSSHSession(storageDirectoryHandle->sftp.session);
        Network_disconnect(&storageDirectoryHandle->sftp.socketHandle);
        String_delete(storageDirectoryHandle->sftp.pathName);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
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
        }

        endOfDirectoryFlag = !storageDirectoryHandle->sftp.entryReadFlag;
      }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
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
        }

        String_set(fileName,storageDirectoryHandle->sftp.pathName);
        File_appendFileNameBuffer(fileName,storageDirectoryHandle->sftp.buffer,n);

        storageDirectoryHandle->sftp.entryReadFlag = FALSE;
      }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
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
