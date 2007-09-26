/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/storage.c,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: storage functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "errors.h"
#include "strings.h"
#include "files.h"
#include "network.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

typedef enum
{
  SSH_PASSWORD_STATE_ASK,
  SSH_PASSWORD_STATE_INIT,
  SSH_PASSWORD_STATE_FAIL,
} SSHPasswordStates;

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL SSHPasswordStates sshPasswordState;
LOCAL String            sshPassword;
LOCAL String            sshPublicKeyFileName;
LOCAL String            sshPrivatKeyFileName;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : getSSHPassword
* Purpose: get ssh password
* Input  : -
* Output : -
* Return : ssh password or NULL if not found/failure
* Notes  : -
\***********************************************************************/

LOCAL const char *getSSHPassword(void)
{
  const char *sshAskPassword;
  const char *password;

  switch (sshPasswordState)
  {
    case SSH_PASSWORD_STATE_ASK:
      if      (globalOptions.sshPassword != NULL)
      {
        /* use command line argument */
        String_setCString(sshPassword,globalOptions.sshPassword);

        password = String_cString(sshPassword);
        sshPasswordState = SSH_PASSWORD_STATE_INIT;
      }
      else if ((sshAskPassword = getenv("SSH_ASKPASS")) != NULL)
      {
        /* call external password program */
        FILE *inputHandle;
        int  ch;

        /* open pipe to external password program */
        inputHandle = popen(sshAskPassword,"r");
        if (inputHandle == NULL)
        {
          sshPasswordState = SSH_PASSWORD_STATE_FAIL;
          return NULL;
        }

        /* read password, discard last LF */
        while ((ch = getc(inputHandle)) != EOF)
        {
          String_appendChar(sshPassword,(char)ch);
        }
        if (String_index(sshPassword,STRING_END) == '\n') String_remove(sshPassword,STRING_END,1);

        /* close pipe */
        pclose(inputHandle);

        password = String_cString(sshPassword);
        sshPasswordState = SSH_PASSWORD_STATE_INIT;
      }
      else 
      {
        password = NULL;
        sshPasswordState = SSH_PASSWORD_STATE_FAIL;
        return NULL;
      }
      break;
    case SSH_PASSWORD_STATE_INIT:
      password = String_cString(sshPassword);
      break;
    case SSH_PASSWORD_STATE_FAIL:
      password = NULL;
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return password;
}

/*---------------------------------------------------------------------*/

Errors Storage_init(void)
{
  sshPasswordState     = SSH_PASSWORD_STATE_ASK;
  sshPassword          = String_new();
  sshPublicKeyFileName = String_new();
  sshPrivatKeyFileName = String_new();
  if (globalOptions.sshPublicKeyFile != NULL)
  {
    File_setFileNameCString(sshPublicKeyFileName,globalOptions.sshPublicKeyFile);
  }
  else
  {
    File_setFileNameCString(sshPublicKeyFileName,getenv("HOME"));
    File_appendFileNameCString(sshPublicKeyFileName,".ssh");
    File_appendFileNameCString(sshPublicKeyFileName,"id_rsa.pub");
  }
  if (globalOptions.sshPrivatKeyFile != NULL)
  {
    File_setFileNameCString(sshPrivatKeyFileName,globalOptions.sshPrivatKeyFile);
  }
  else
  {
    File_setFileNameCString(sshPrivatKeyFileName,getenv("HOME"));
    File_appendFileNameCString(sshPrivatKeyFileName,".ssh");
    File_appendFileNameCString(sshPrivatKeyFileName,"id_rsa");
  }

  return ERROR_NONE;
}

void Storage_done(void)
{
  String_delete(sshPrivatKeyFileName);
  String_delete(sshPublicKeyFileName);
  String_delete(sshPassword);
}

Errors Storage_create(StorageInfo *storageInfo,
                      String      storageName,
                      uint64      fileSize
                     )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageName != NULL);

  if      (String_findCString(storageName,STRING_BEGIN,"scp:") == 0)
  {
    long       i0,i1;
    String     userName;
    String     hostName;
    String     hostFileName;
    const char *sshPassword;

    /* init variables */
    storageInfo->mode = STORAGE_MODE_WRITE;
    storageInfo->type = STORAGE_TYPE_SCP;

    /* parse connection string */
    i0 = 4;
    i1 = String_findChar(storageName,i0,'@');
    if (i1 < 0)
    {
      printError("No user name given in 'scp' string!\n");
      return ERROR_SSH_SESSION_FAIL;
    }
    userName = String_sub(String_new(),storageName,i0,i1-i0);
    i0 = i1+1;
    i1 = String_findChar(storageName,i0,':');
    if (i1 < 0)
    {
      printError("No host name given in 'scp' string!\n");
      String_delete(userName);
      return ERROR_SSH_SESSION_FAIL;
    }
    hostName = String_sub(String_new(),storageName,i0,i1-i0);
    i0 = i1+1;
    hostFileName = String_sub(String_new(),storageName,i0,STRING_END);

#if 1
    /* get password */
    sshPassword = getSSHPassword();
    if (sshPassword == NULL)
    {
      printError("No ssh password given!\n");
      String_delete(hostFileName);
      String_delete(hostName);
      String_delete(userName);
      return ERROR_SSH_SESSION_FAIL;
    }
#endif /* 0 */

    /* open network connection */
    error = Network_connect(&storageInfo->scp.socketHandle,hostName,globalOptions.sshPort);
    if (error != ERROR_NONE)
    {
      String_delete(hostFileName);
      String_delete(hostName);
      String_delete(userName);
      return error;
    }

    /* init session */
    storageInfo->scp.session = libssh2_session_init();
    if (storageInfo->scp.session == NULL)
    {
      printError("Init ssh session fail!\n");
      Network_disconnect(storageInfo->scp.socketHandle);
      String_delete(hostFileName);
      String_delete(hostName);
      String_delete(userName);
      return ERROR_SSH_SESSION_FAIL;
    }
    if (libssh2_session_startup(storageInfo->scp.session,storageInfo->scp.socketHandle) != 0)
    {
      printError("Startup ssh session fail!\n");
      libssh2_session_disconnect(storageInfo->scp.session, "Normal Shutdown, Thank you for playing");
      libssh2_session_free(storageInfo->scp.session);
      Network_disconnect(storageInfo->scp.socketHandle);
      String_delete(hostFileName);
      String_delete(hostName);
      String_delete(userName);
      return ERROR_SSH_SESSION_FAIL;
    }

#if 1
    if (libssh2_userauth_publickey_fromfile(storageInfo->scp.session,
// NYI
                                            String_cString(userName),
                                            String_cString(sshPublicKeyFileName),
                                            String_cString(sshPrivatKeyFileName),
                                            getSSHPassword()
                                           ) != 0
       )
    {
      printError("ssh authentification fail!\n");
      libssh2_session_disconnect(storageInfo->scp.session, "Normal Shutdown, Thank you for playing");
      libssh2_session_free(storageInfo->scp.session);
      Network_disconnect(storageInfo->scp.socketHandle);
      String_delete(hostFileName);
      String_delete(hostName);
      String_delete(userName);
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
      libssh2_session_disconnect(storageInfo->scp.session, "Normal Shutdown, Thank you for playing");
      libssh2_session_free(storageInfo->scp.session);
      Network_disconnect(storageInfo->scp.socketHandle);
      String_delete(hostFileName);
      String_delete(hostName);
      String_delete(userName);
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
      Network_disconnect(storageInfo->scp.socketHandle);
      String_delete(hostFileName);
      String_delete(hostName);
      String_delete(userName);
      return ERROR_SSH_SESSION_FAIL;
    }

    /* free resources */
    String_delete(hostFileName);
    String_delete(hostName);
    String_delete(userName);
  }
  else if (String_findCString(storageName,STRING_BEGIN,"sftp:") == 0)
  {
    /* init variables */
    storageInfo->mode = STORAGE_MODE_WRITE;
    storageInfo->type = STORAGE_TYPE_SFTP;
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
  }
  else
  {
    /* init variables */
    storageInfo->mode          = STORAGE_MODE_WRITE;
    storageInfo->type          = STORAGE_TYPE_FILE;
    storageInfo->file.fileName = String_copy(storageName);

    /* open file */
    error = File_open(&storageInfo->file.fileHandle,storageInfo->file.fileName,FILE_OPENMODE_CREATE);
    if (error != ERROR_NONE)
    {
      String_delete(storageInfo->file.fileName);
      return error;
    }
  }

  return ERROR_NONE;
}

Errors Storage_open(StorageInfo *storageInfo, String storageName)
{
  assert(storageInfo != NULL);
  assert(storageName != NULL);

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
      libssh2_channel_send_eof(storageInfo->scp.channel);
      libssh2_channel_wait_eof(storageInfo->scp.channel);
      libssh2_channel_wait_closed(storageInfo->scp.channel);
      libssh2_channel_free(storageInfo->scp.channel);
      libssh2_session_disconnect(storageInfo->scp.session, "Normal Shutdown, Thank you for playing");
      libssh2_session_free(storageInfo->scp.session);
      Network_disconnect(storageInfo->scp.socketHandle);
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
            return ERROR_IO_ERROR;
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
