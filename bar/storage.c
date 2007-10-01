/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/storage.c,v $
* $Revision: 1.4 $
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

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL String sshPassword;
LOCAL String sshPublicKeyFileName;
LOCAL String sshPrivatKeyFileName;

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

LOCAL bool initSSHPassword(void)
{
  const char *sshAskPassword;

  if      (globalOptions.sshPassword != NULL)
  {
    /* use command line argument */
    String_setCString(sshPassword,globalOptions.sshPassword);

    return TRUE;
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
      return FALSE;
    }

    /* read password, discard last LF */
    while ((ch = getc(inputHandle)) != EOF)
    {
      String_appendChar(sshPassword,(char)ch);
    }
    if (String_index(sshPassword,STRING_END) == '\n') String_remove(sshPassword,STRING_END,1);

    /* close pipe */
    pclose(inputHandle);

    return (String_length(sshPassword) > 0);
  }
  else 
  {
    return FALSE;
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

Errors Storage_prepare(const String storageName)
{
  String storageSpecifier;

  assert(storageName != NULL);

  storageSpecifier = String_new();
  switch (getStorageType(storageName,storageSpecifier))
  {
    case STORAGE_TYPE_FILE:
      break;
    case STORAGE_TYPE_SCP:
    case STORAGE_TYPE_SFTP:
      if (!parseSSHSpecifier(storageSpecifier,NULL,NULL,NULL))
      {
        String_delete(storageSpecifier);
        return ERROR_SSH_SESSION_FAIL;
      }

      if (!initSSHPassword())
      {
        String_delete(storageSpecifier);
        return ERROR_NO_SSH_PASSWORD;
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

Errors Storage_create(StorageInfo *storageInfo,
                      String      storageName,
                      uint64      fileSize
                     )
{
  String storageSpecifier;
  Errors error;

  assert(storageInfo != NULL);
  assert(storageName != NULL);

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
        userName = String_new();
        hostName = String_new();
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
        error = Network_connect(&storageInfo->scp.socketHandle,hostName,globalOptions.sshPort);
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
          libssh2_session_disconnect(storageInfo->scp.session, "Normal Shutdown, Thank you for playing");
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
                                                String_cString(sshPublicKeyFileName),
                                                String_cString(sshPrivatKeyFileName),
                                                String_cString(sshPassword)
                                               ) != 0
           )
        {
          printError("ssh authentification fail!\n");
          libssh2_session_disconnect(storageInfo->scp.session, "Normal Shutdown, Thank you for playing");
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
          libssh2_session_disconnect(storageInfo->scp.session, "Normal Shutdown, Thank you for playing");
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
