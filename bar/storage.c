/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/storage.c,v $
* $Revision: 1.8 $
* $Author: torsten $
* Contents: storage functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
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
* Name   : getStorageType
* Purpose: get storage type from storage name
* Input  : storageName - storage name
* Output : storageSpecifier - storage specific data (can be NULL)
* Return : storage type
* Notes  : -
\***********************************************************************/

LOCAL StorageTypes getStorageType(const String storageName, String storageSpecifier)
{
  if      (String_findCString(storageName,STRING_BEGIN,"scp:") == 0)
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
    return STORAGE_TYPE_FILE;
  }
}

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
* Name   : parseSSHSpecifier
* Purpose: parse ssh specifier: <user name>@<host name>:<host file name>
* Input  : sshSpecifier - ssh specifier string
* Output : userName     - user name (can be NULL)
*          hostName     - host name (can be NULL)
*          hostFileName - host file name (can be NULL)
* Return : TRUE if ssh specifier parsed, FALSE if specifier invalid
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SSH2
LOCAL bool parseSSHSpecifier(const String sshSpecifier,
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
    printError("Init ssh session fail!\n");
    return ERROR_SSH_SESSION_FAIL;
  }
  if (libssh2_session_startup(*session,
                              Network_getSocket(socketHandle)
                             ) != 0
     )
  {
    printError("Startup ssh session fail!\n");
    libssh2_session_disconnect(*session,"");
    libssh2_session_free(*session);
    return ERROR_SSH_SESSION_FAIL;
  }

#if 1
  password = Password_deploy((options->sshPassword != NULL)?options->sshPassword:defaultSSHPassword);
  if (libssh2_userauth_publickey_fromfile(*session,
// NYI
                                          String_cString(userName),
                                          (options->sshPublicKeyFileName != NULL)?options->sshPublicKeyFileName:String_cString(defaultSSHPublicKeyFileName),
                                          (options->sshPrivatKeyFileName != NULL)?options->sshPrivatKeyFileName:String_cString(defaultSSHPrivatKeyFileName),
                                          password
                                         ) != 0
     )
  {
    printError("ssh authentification fail!\n");
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
    printError("ssh authentification fail!\n");
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

Errors Storage_prepare(const String  storageName,
                       const Options *options
                      )
{
  String storageSpecifier;

  assert(storageName != NULL);
  assert(options != NULL);

  storageSpecifier = String_new();
  switch (getStorageType(storageName,storageSpecifier))
  {
    case STORAGE_TYPE_FILE:
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
          if (!parseSSHSpecifier(storageSpecifier,userName,hostName,hostFileName))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* initialise password */
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
            return ERROR_SSH_SESSION_FAIL;
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

Errors Storage_create(StorageInfo   *storageInfo,
                      String        storageName,
                      uint64        fileSize,
                      const Options *options
                     )
{
  String storageSpecifier;
  Errors error;

  assert(storageInfo != NULL);
  assert(storageName != NULL);
  assert(options != NULL);

  storageSpecifier = String_new();
  switch (getStorageType(storageName,storageSpecifier))
  {
    case STORAGE_TYPE_FILE:
      /* init variables */
      storageInfo->mode          = STORAGE_MODE_WRITE;
      storageInfo->type          = STORAGE_TYPE_FILE;
      storageInfo->file.fileName = String_copy(storageSpecifier);

      /* open file */
      error = File_open(&storageInfo->file.fileHandle,storageInfo->file.fileName,FILE_OPENMODE_CREATE);
      if (error != ERROR_NONE)
      {
        String_delete(storageInfo->file.fileName);
        String_delete(storageSpecifier);
        return error;
      }
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          String userName;
          String hostName;
          String hostFileName;

          /* init variables */
          storageInfo->mode = STORAGE_MODE_WRITE;
          storageInfo->type = STORAGE_TYPE_SCP;

          /* parse connection string */
          userName     = String_new();
          hostName     = String_new();
          hostFileName = String_new();
          if (!parseSSHSpecifier(storageSpecifier,userName,hostName,hostFileName))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* open network connection */
          error = Network_connect(&storageInfo->scp.socketHandle,hostName,options->sshPort,0);
          if (error != ERROR_NONE)
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return error;
          }

          /* init session */
          error = initSSHSession(&storageInfo->scp.socketHandle,
                                 userName,
                                 options,
                                 &storageInfo->scp.session
                                );
          if (error != ERROR_NONE)
          {
            Network_disconnect(&storageInfo->scp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* open channel */
          storageInfo->scp.channel = libssh2_scp_send(storageInfo->scp.session,
                                                      String_cString(hostFileName),
  0600,
                                                      fileSize
                                                     );
          if (storageInfo->scp.channel == NULL)
          {
            printError("Init ssh channel fail!\n");
            doneSSHSession(storageInfo->sftp.session);
            Network_disconnect(&storageInfo->scp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }
        }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          String userName;
          String hostName;
          String hostFileName;

          /* init variables */
          storageInfo->mode = STORAGE_MODE_WRITE;
          storageInfo->type = STORAGE_TYPE_SFTP;

          /* parse connection string */
          userName     = String_new();
          hostName     = String_new();
          hostFileName = String_new();
          if (!parseSSHSpecifier(storageSpecifier,userName,hostName,hostFileName))
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* open network connection */
          error = Network_connect(&storageInfo->sftp.socketHandle,hostName,options->sshPort,0);
          if (error != ERROR_NONE)
          {
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return error;
          }

          /* init session */
          error = initSSHSession(&storageInfo->sftp.socketHandle,
                                 userName,
                                 options,
                                 &storageInfo->sftp.session
                                );
          if (error != ERROR_NONE)
          {
            Network_disconnect(&storageInfo->sftp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* open FTP session */
          storageInfo->sftp.sftp = libssh2_sftp_init(storageInfo->sftp.session);
          if (storageInfo->sftp.sftp == NULL)
          {
            printError("Init sftp fail!\n");
            doneSSHSession(storageInfo->sftp.session);
            Network_disconnect(&storageInfo->sftp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
            String_delete(storageSpecifier);
            return ERROR_SSH_SESSION_FAIL;
          }

          /* open FTP file */
          storageInfo->sftp.sftpHandle = libssh2_sftp_open(storageInfo->sftp.sftp,
                                                           String_cString(hostFileName),
                                                           LIBSSH2_FXF_CREAT|LIBSSH2_FXF_WRITE|LIBSSH2_FXF_TRUNC,
  LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR
                                                          );
          if (storageInfo->sftp.sftpHandle == NULL)
          {
            printError("Create sftp file fail!\n");
            libssh2_sftp_shutdown(storageInfo->sftp.sftp);
            doneSSHSession(storageInfo->sftp.session);
            Network_disconnect(&storageInfo->sftp.socketHandle);
            String_delete(hostFileName);
            String_delete(hostName);
            String_delete(userName);
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

Errors Storage_open(StorageInfo   *storageInfo,
                    const String  storageName,
                    const Options *options
                   )
{
  assert(storageInfo != NULL);
  assert(storageName != NULL);

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(storageName);
  UNUSED_VARIABLE(options);

  HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();

  return ERROR_NONE;
}

void Storage_close(StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);

  switch (storageInfo->type)
  {
    case STORAGE_TYPE_FILE:
      File_close(&storageInfo->file.fileHandle);
      String_delete(storageInfo->file.fileName);
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        switch (storageInfo->mode)
        {
          case STORAGE_MODE_WRITE:
            libssh2_channel_send_eof(storageInfo->scp.channel);
            libssh2_channel_wait_eof(storageInfo->scp.channel);
            libssh2_channel_wait_closed(storageInfo->scp.channel);
            break;
          case STORAGE_MODE_READ:
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
          #endif /* NDEBUG */
        }
        libssh2_channel_free(storageInfo->scp.channel);
        doneSSHSession(storageInfo->sftp.session);
        Network_disconnect(&storageInfo->scp.socketHandle);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        libssh2_sftp_close(storageInfo->sftp.sftpHandle);
        libssh2_sftp_shutdown(storageInfo->sftp.sftp);
        doneSSHSession(storageInfo->sftp.session);
        Network_disconnect(&storageInfo->scp.socketHandle);
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

uint64 Storage_getSize(StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);

  switch (storageInfo->type)
  {
    case STORAGE_TYPE_FILE:
      return File_getSize(&storageInfo->file.fileHandle);
      break;
    case STORAGE_TYPE_SCP:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      break;
    case STORAGE_TYPE_SFTP:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return 0;
}

Errors Storage_read(StorageInfo *storageInfo, void *buffer, ulong size, ulong *readBytes)
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->mode == STORAGE_MODE_READ);
  assert(buffer != NULL);
  assert(readBytes != NULL);

  switch (storageInfo->type)
  {
    case STORAGE_TYPE_FILE:
      error = File_read(&storageInfo->file.fileHandle,buffer,size,readBytes);
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    case STORAGE_TYPE_SCP:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      break;
    case STORAGE_TYPE_SFTP:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return ERROR_NONE;
}

Errors Storage_write(StorageInfo *storageInfo, const void *buffer, ulong size)
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->mode == STORAGE_MODE_WRITE);
  assert(buffer != NULL);

  switch (storageInfo->type)
  {
    case STORAGE_TYPE_FILE:
      error = File_write(&storageInfo->file.fileHandle,buffer,size);
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          long n;

          while (size > 0)
          {
            n = libssh2_channel_write(storageInfo->scp.channel,
                                      buffer,
                                      size
                                     );
            if (n < 0)
            {
              return ERROR_NETWORK_SEND;
            }
            buffer = (byte*)buffer+n;
            size -= n;
          };
        }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          long n;

          while (size > 0)
          {
            n = libssh2_sftp_write(storageInfo->sftp.sftpHandle,
                                   buffer,
                                   size
                                  );
            if (n < 0)
            {
              return ERROR_NETWORK_SEND;
            }
            buffer = (byte*)buffer+n;
            size -= n;
          };
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

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Storage_openDirectory(StorageInfo   *storageInfo,
                             const String  storageName,
                             const Options *options
                            )
{
}

void Storage_closeDirectory(StorageInfo *storageInfo)
{
}

Errors Storage_readDirectory(StorageInfo *storageInfo,
                             String      fileName
                            )
{
}

#ifdef __cplusplus
  }
#endif

/* end of file */
