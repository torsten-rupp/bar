/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/storage.c,v $
* $Revision: 1.6 $
* $Author: torsten $
* Contents: storage functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <libssh2.h>
#include <assert.h>

#include "global.h"
#include "errors.h"
#include "strings.h"
#include "files.h"
#include "network.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL String defaultSSHPassword;
LOCAL String defaultSSHPublicKeyFileName;
LOCAL String defaultSSHPrivatKeyFileName;

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
      while ((ch = getc(inputHandle)) != EOF)
      {
        String_appendChar(defaultSSHPassword,(char)ch);
      }
      if (String_index(defaultSSHPassword,STRING_END) == '\n') String_remove(defaultSSHPassword,STRING_END,1);

      /* close pipe */
      pclose(inputHandle);

      return (String_length(defaultSSHPassword) > 0);
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

/*---------------------------------------------------------------------*/

Errors Storage_init(void)
{
  defaultSSHPassword          = String_new();
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
  String_delete(defaultSSHPassword);
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
        session = libssh2_session_init();
        if (session == NULL)
        {
          Network_disconnect(&socketHandle);
          String_delete(hostFileName);
          String_delete(hostName);
          String_delete(userName);
          String_delete(storageSpecifier);
          return ERROR_SSH_SESSION_FAIL;
        }
        if (libssh2_session_startup(session,
                                    Network_getSocket(&socketHandle)
                                   ) != 0
           )
        {
          libssh2_session_disconnect(session,"");
          libssh2_session_free(session);
          Network_disconnect(&socketHandle);
          String_delete(hostFileName);
          String_delete(hostName);
          String_delete(userName);
          String_delete(storageSpecifier);
          return ERROR_SSH_SESSION_FAIL;
        }

    #if 1
        if (libssh2_userauth_publickey_fromfile(session,
    // NYI
                                                String_cString(userName),
                                                (options->sshPublicKeyFileName != NULL)?options->sshPublicKeyFileName:String_cString(defaultSSHPublicKeyFileName),
                                                (options->sshPrivatKeyFileName != NULL)?options->sshPrivatKeyFileName:String_cString(defaultSSHPrivatKeyFileName),
                                                (options->sshPassword != NULL)?options->sshPassword:String_cString(defaultSSHPassword)
                                               ) != 0
           )
        {
          libssh2_session_disconnect(session,"");
          libssh2_session_free(session);
          Network_disconnect(&socketHandle);
          String_delete(hostFileName);
          String_delete(hostName);
          String_delete(userName);
          String_delete(storageSpecifier);
          return ERROR_SSH_AUTHENTIFICATION;
        }
    #else
        if (libssh2_userauth_keyboard_interactive(session,
                                                  String_cString(userName),
                                                  NULL
                                                ) != 0
           )
        {
          libssh2_session_disconnect(session,"");
          libssh2_session_free(session);
          Network_disconnect(&socketHandle);
          String_delete(hostFileName);
          String_delete(hostName);
          String_delete(userName);
          String_delete(storageSpecifier);
          return ERROR_SSH_AUTHENTIFICATION;
        }
    #endif /* 0 */
        libssh2_session_disconnect(session,"");
        libssh2_session_free(session);
        Network_disconnect(&socketHandle);

        /* free resources */
        String_delete(hostFileName);
        String_delete(hostName);
        String_delete(userName);
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
        storageInfo->scp.session = libssh2_session_init();
        if (storageInfo->scp.session == NULL)
        {
          printError("Init ssh session fail!\n");
          Network_disconnect(&storageInfo->scp.socketHandle);
          String_delete(hostFileName);
          String_delete(hostName);
          String_delete(userName);
          String_delete(storageSpecifier);
          return ERROR_SSH_SESSION_FAIL;
        }
        if (libssh2_session_startup(storageInfo->scp.session,
                                    Network_getSocket(&storageInfo->scp.socketHandle)
                                   ) != 0
           )
        {
          printError("Startup ssh session fail!\n");
          libssh2_session_disconnect(storageInfo->scp.session,"");
          libssh2_session_free(storageInfo->scp.session);
          Network_disconnect(&storageInfo->scp.socketHandle);
          String_delete(hostFileName);
          String_delete(hostName);
          String_delete(userName);
          String_delete(storageSpecifier);
          return ERROR_SSH_SESSION_FAIL;
        }

    #if 1
        if (libssh2_userauth_publickey_fromfile(storageInfo->scp.session,
    // NYI
                                                String_cString(userName),
                                                (options->sshPublicKeyFileName != NULL)?options->sshPublicKeyFileName:String_cString(defaultSSHPublicKeyFileName),
                                                (options->sshPrivatKeyFileName != NULL)?options->sshPrivatKeyFileName:String_cString(defaultSSHPrivatKeyFileName),
                                                (options->sshPassword != NULL)?options->sshPassword:String_cString(defaultSSHPassword)
                                               ) != 0
           )
        {
          printError("ssh authentification fail!\n");
          libssh2_session_disconnect(storageInfo->scp.session,"");
          libssh2_session_free(storageInfo->scp.session);
          Network_disconnect(&storageInfo->scp.socketHandle);
          String_delete(hostFileName);
          String_delete(hostName);
          String_delete(userName);
          String_delete(storageSpecifier);
          return ERROR_SSH_AUTHENTIFICATION;
        }
    #else
        if (libssh2_userauth_keyboard_interactive(storageInfo->scp.session,
                                                  String_cString(userName),
                                                  NULL
                                                ) != 0
           )
        {
          printError("ssh authentification fail!\n");
          libssh2_session_disconnect(storageInfo->scp.session,"");
          libssh2_session_free(storageInfo->scp.session);
          Network_disconnect(&storageInfo->scp.socketHandle);
          String_delete(hostFileName);
          String_delete(hostName);
          String_delete(userName);
          String_delete(storageSpecifier);
          return ERROR_SSH_AUTHENTIFICATION;
        }
    #endif /* 0 */

        /* open channel */
        storageInfo->scp.channel = libssh2_scp_send(storageInfo->scp.session,
                                                    String_cString(hostFileName),
                                  0600,
                                                    fileSize
                                                   );
        if (storageInfo->scp.channel == NULL)
        {
          printError("Init ssh channel fail!\n");
          libssh2_session_disconnect(storageInfo->scp.session, "Errror");
          libssh2_session_free(storageInfo->scp.session);
          Network_disconnect(&storageInfo->scp.socketHandle);
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
      break;
    case STORAGE_TYPE_SFTP:
      /* init variables */
      storageInfo->mode = STORAGE_MODE_WRITE;
      storageInfo->type = STORAGE_TYPE_SFTP;
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
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

Errors Storage_open(StorageInfo *storageInfo, String storageName)
{
  assert(storageInfo != NULL);
  assert(storageName != NULL);

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(storageName);

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
      libssh2_session_disconnect(storageInfo->scp.session, "Normal Shutdown, Thank you for playing");
      libssh2_session_free(storageInfo->scp.session);
      Network_disconnect(&storageInfo->scp.socketHandle);
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

#ifdef __cplusplus
  }
#endif

/* end of file */
