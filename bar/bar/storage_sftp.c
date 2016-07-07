/***********************************************************************\
*
* $Revision: 4036 $
* $Date: 2015-05-30 01:48:57 +0200 (Sat, 30 May 2015) $
* $Author: torsten $
* Contents: storage SFTP functions
* Systems: all
*
\***********************************************************************/

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
  #include <libssh2_sftp.h>
#endif /* HAVE_SSH2 */
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "autofree.h"
#include "strings.h"
#include "stringlists.h"
#include "files.h"
#include "network.h"
#include "errors.h"

#include "errors.h"
#include "crypt.h"
#include "passwords.h"
#include "misc.h"
#include "archive.h"
#include "bar_global.h"
#include "bar.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
/* file data buffer size */
#define BUFFER_SIZE (64*1024)

// different timeouts [ms]
#define SSH_TIMEOUT    (30*1000)
#define READ_TIMEOUT   (60*1000)

#define INITIAL_BUFFER_SIZE   (64*1024)
#define INCREMENT_BUFFER_SIZE ( 8*1024)
#define MAX_BUFFER_SIZE       (64*1024)
#define MAX_FILENAME_LENGTH   ( 8*1024)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

// ----------------------------------------------------------------------

#ifdef HAVE_SSH2
/***********************************************************************\
* Name   : sftpSendCallback
* Purpose: sftp send callback: count total sent bytes and pass to
*          original function
* Input  : socket   - libssh2 socket
*          buffer   - buffer with data
*          length   - length to send
*          flags    - libssh2 flags
*          abstract - pointer to user data
* Output : -
* Return : number of bytes sent
* Notes  : parameters are hidden in LIBSSH2_SEND_FUNC()!
\***********************************************************************/

LOCAL LIBSSH2_SEND_FUNC(sftpSendCallback)
{
  StorageArchiveHandle *storageArchiveHandle;
  ssize_t              n;

  assert(abstract != NULL);

  storageArchiveHandle = *((StorageArchiveHandle**)abstract);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->sftp);
  assert(storageArchiveHandle->sftp.oldSendCallback != NULL);

  n = storageArchiveHandle->sftp.oldSendCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageArchiveHandle->sftp.totalSentBytes += (uint64)n;

  return n;
}

/***********************************************************************\
* Name   : sftpReceiveCallback
* Purpose: sftp receive callback: count total received bytes and pass to
*          original function
* Input  : socket   - libssh2 socket
*          buffer   - buffer with data
*          length   - length to receive
*          flags    - libssh2 flags
*          abstract - pointer to user data
* Output : -
* Return : number of bytes received
* Notes  : parameters are hidden in LIBSSH2_RECV_FUNC()!
\***********************************************************************/

LOCAL LIBSSH2_RECV_FUNC(sftpReceiveCallback)
{
  StorageArchiveHandle *storageArchiveHandle;
  ssize_t              n;

  assert(abstract != NULL);

  storageArchiveHandle = *((StorageArchiveHandle**)abstract);
  assert(storageArchiveHandle != NULL);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->sftp);
  assert(storageArchiveHandle->sftp.oldReceiveCallback != NULL);

  n = storageArchiveHandle->sftp.oldReceiveCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageArchiveHandle->sftp.totalReceivedBytes += (uint64)n;

  return n;
}
#endif /* HAVE_SSH2 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageSFTP_initAll(void)
{
  return ERROR_NONE;
}

LOCAL void StorageSFTP_doneAll(void)
{
}

LOCAL bool StorageSFTP_parseSpecifier(ConstString sshSpecifier,
                                      String      hostName,
                                      uint        *hostPort,
                                      String      loginName,
                                      Password    *loginPassword
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
  if (loginPassword != NULL) Password_clear(loginPassword);

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

LOCAL bool StorageSFTP_equalSpecifiers(const StorageSpecifier *storageSpecifier1,
                                       ConstString            archiveName1,
                                       const StorageSpecifier *storageSpecifier2,
                                       ConstString            archiveName2
                                      )
{
  assert(storageSpecifier1 != NULL);
  assert(storageSpecifier1->type == STORAGE_TYPE_SFTP);
  assert(storageSpecifier2 != NULL);
  assert(storageSpecifier2->type == STORAGE_TYPE_SFTP);

  if (archiveName1 == NULL) archiveName1 = storageSpecifier1->archiveName;
  if (archiveName2 == NULL) archiveName2 = storageSpecifier2->archiveName;

  return    String_equals(storageSpecifier1->hostName,storageSpecifier2->hostName)
         && String_equals(storageSpecifier1->loginName,storageSpecifier2->loginName)
         && String_equals(archiveName1,archiveName2);
}

LOCAL String StorageSFTP_getName(StorageSpecifier *storageSpecifier,
                                 ConstString      archiveName
                                )
{
  ConstString storageFileName;
  const char  *plainLoginPassword;

  assert(storageSpecifier != NULL);

  // get file to use
  if      (archiveName != NULL)
  {
    storageFileName = archiveName;
  }
  else if (storageSpecifier->archivePatternString != NULL)
  {
    storageFileName = storageSpecifier->archivePatternString;
  }
  else
  {
    storageFileName = storageSpecifier->archiveName;
  }

  String_appendCString(storageSpecifier->storageName,"sftp://");
  if (!String_isEmpty(storageSpecifier->loginName))
  {
    String_append(storageSpecifier->storageName,storageSpecifier->loginName);
    if (!Password_isEmpty(storageSpecifier->loginPassword))
    {
      String_appendChar(storageSpecifier->storageName,':');
      plainLoginPassword = Password_deploy(storageSpecifier->loginPassword);
      String_appendCString(storageSpecifier->storageName,plainLoginPassword);
      Password_undeploy(storageSpecifier->loginPassword);
    }
    String_appendChar(storageSpecifier->storageName,'@');
  }
  String_append(storageSpecifier->storageName,storageSpecifier->hostName);
  if (!String_isEmpty(storageFileName))
  {
    String_appendChar(storageSpecifier->storageName,'/');
    String_append(storageSpecifier->storageName,storageFileName);
  }

  return storageSpecifier->storageName;
}

LOCAL ConstString StorageSFTP_getPrintableName(StorageSpecifier *storageSpecifier,
                                               ConstString      archiveName
                                              )
{
  ConstString storageFileName;

  assert(storageSpecifier != NULL);

  // get file to use
  if      (!String_isEmpty(archiveName))
  {
    storageFileName = archiveName;
  }
  else if (!String_isEmpty(storageSpecifier->archivePatternString))
  {
    storageFileName = storageSpecifier->archivePatternString;
  }
  else
  {
    storageFileName = storageSpecifier->archiveName;
  }

  String_appendCString(storageSpecifier->storageName,"sftp://");
  String_append(storageSpecifier->storageName,storageSpecifier->hostName);
  if ((storageSpecifier->hostPort != 0) && (storageSpecifier->hostPort != 22))
  {
    String_format(storageSpecifier->storageName,":%d",storageSpecifier->hostPort);
  }
  if (!String_isEmpty(storageFileName))
  {
    String_appendChar(storageSpecifier->storageName,'/');
    String_append(storageSpecifier->storageName,storageFileName);
  }

  return storageSpecifier->storageName;
}

LOCAL Errors StorageSFTP_init(StorageHandle                *storageHandle,
                              const StorageSpecifier       *storageSpecifier,
                              const JobOptions             *jobOptions,
                              BandWidthList                *maxBandWidthList,
                              ServerConnectionPriorities   serverConnectionPriority
                             )
{
  #ifdef HAVE_SSH2
    AutoFreeList autoFreeList;
    Errors       error;
    SSHServer    sshServer;
  #endif /* HAVE_SSH2 */

  assert(storageHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_SFTP);

  UNUSED_VARIABLE(storageSpecifier);

  #ifdef HAVE_SSH2
    // init variables
    AutoFree_init(&autoFreeList);
    storageHandle->sftp.sshPublicKeyFileName   = NULL;
    storageHandle->sftp.sshPrivateKeyFileName  = NULL;
    initBandWidthLimiter(&storageHandle->sftp.bandWidthLimiter,maxBandWidthList);
    AUTOFREE_ADD(&autoFreeList,&storageHandle->sftp.bandWidthLimiter,{ doneBandWidthLimiter(&storageHandle->sftp.bandWidthLimiter); });

    // get SSH server settings
    storageHandle->sftp.serverId = getSSHServerSettings(storageHandle->storageSpecifier.hostName,jobOptions,&sshServer);
    if (String_isEmpty(storageHandle->storageSpecifier.loginName)) String_set(storageHandle->storageSpecifier.loginName,sshServer.loginName);
    if (String_isEmpty(storageHandle->storageSpecifier.loginName)) String_setCString(storageHandle->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageHandle->storageSpecifier.loginName)) String_setCString(storageHandle->storageSpecifier.loginName,getenv("USER"));
    if (storageHandle->storageSpecifier.hostPort == 0) storageHandle->storageSpecifier.hostPort = sshServer.port;
    storageHandle->sftp.publicKey  = sshServer.publicKey;
    storageHandle->sftp.privateKey = sshServer.privateKey;
    if (String_isEmpty(storageHandle->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }
    if (sshServer.publicKey.data == NULL)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_SSH_PUBLIC_KEY;
    }
    if (sshServer.privateKey.data == NULL)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_SSH_PRIVATE_KEY;
    }

    // allocate SSH server
    if (!allocateServer(storageHandle->sftp.serverId,serverConnectionPriority,60*1000L))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,&storageHandle->sftp.serverId,{ freeServer(storageHandle->sftp.serverId); });

    // check if SSH login is possible
    error = ERROR_UNKNOWN;
    if ((error != ERROR_NONE) && (sshServer.password != NULL))
    {
      error = checkSSHLogin(storageHandle->storageSpecifier.hostName,
                            storageHandle->storageSpecifier.hostPort,
                            storageHandle->storageSpecifier.loginName,
                            sshServer.password,
                            storageHandle->sftp.publicKey.data,
                            storageHandle->sftp.publicKey.length,
                            storageHandle->sftp.privateKey.data,
                            storageHandle->sftp.privateKey.length
                           );
      if (error == ERROR_NONE)
      {
        Password_set(storageHandle->storageSpecifier.loginPassword,sshServer.password);
      }
    }
    if (error != ERROR_NONE)
    {
      // initialize default password
      while (   (error != ERROR_NONE)
             && initSSHLogin(storageHandle->storageSpecifier.hostName,
                             storageHandle->storageSpecifier.loginName,
                             storageHandle->storageSpecifier.loginPassword,
                             jobOptions,
                             CALLBACK(storageHandle->getPasswordFunction,storageHandle->getPasswordUserData)
                            )
            )
      {
        error = checkSSHLogin(storageHandle->storageSpecifier.hostName,
                              storageHandle->storageSpecifier.hostPort,
                              storageHandle->storageSpecifier.loginName,
                              storageHandle->storageSpecifier.loginPassword,
                              storageHandle->sftp.publicKey.data,
                              storageHandle->sftp.publicKey.length,
                              storageHandle->sftp.privateKey.data,
                              storageHandle->sftp.privateKey.length
                             );
        if (error == ERROR_NONE)
        {
          Password_set(storageHandle->storageSpecifier.loginPassword,defaultSSHPassword);
        }
      }
      if (error != ERROR_NONE)
      {
        error = (!Password_isEmpty(sshServer.password) || !Password_isEmpty(defaultSSHPassword))
                  ? ERRORX_(INVALID_SSH_PASSWORD,0,"%s",String_cString(storageHandle->storageSpecifier.hostName))
                  : ERRORX_(NO_SSH_PASSWORD,0,"%s",String_cString(storageHandle->storageSpecifier.hostName));
      }

      // store passwrd as default SSH password
      if (error == ERROR_NONE)
      {
        if (defaultSSHPassword == NULL) defaultSSHPassword = Password_new();
        Password_set(defaultSSHPassword,storageHandle->storageSpecifier.loginPassword);
      }
    }
    assert(error != ERROR_UNKNOWN);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // free resources
    AutoFree_done(&autoFreeList);

    return ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(storageSpecifier);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(maxBandWidthList);
    UNUSED_VARIABLE(serverConnectionPriority);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL Errors StorageSFTP_done(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);

  #ifdef HAVE_SSH2
    freeServer(storageHandle->sftp.serverId);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_SSH2 */

  return ERROR_NONE;
}

LOCAL bool StorageSFTP_isServerAllocationPending(StorageHandle *storageHandle)
{
  bool serverAllocationPending;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);

  serverAllocationPending = FALSE;
  #if defined(HAVE_SSH2)
    serverAllocationPending = isServerAllocationPending(storageHandle->sftp.serverId);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);

    serverAllocationPending = FALSE;
  #endif /* HAVE_SSH2 */

  return serverAllocationPending;
}

LOCAL Errors StorageSFTP_preProcess(StorageHandle *storageHandle,
                                    ConstString   archiveName,
                                    time_t        timestamp,
                                    bool          initialFlag
                                   )
{
  Errors error;
  #ifdef HAVE_SSH2
    TextMacro textMacros[2];
    String    script;
  #endif /* HAVE_SSH2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);

  error = ERROR_NONE;

  #ifdef HAVE_SSH2
    {
      if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
      {
        if (!initialFlag)
        {
          // init macros
          TEXT_MACRO_N_STRING (textMacros[0],"%file",  archiveName,                NULL);
          TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageHandle->volumeNumber,NULL);

          // write pre-processing
          if (globalOptions.sftp.writePreProcessCommand != NULL)
          {
            printInfo(1,"Write pre-processing...");

            // get script
            script = expandTemplate(String_cString(globalOptions.sftp.writePreProcessCommand),
                                    EXPAND_MACRO_MODE_STRING,
                                    timestamp,
                                    initialFlag,
                                    textMacros,
                                    SIZE_OF_ARRAY(textMacros)
                                   );
            if (script != NULL)
            {
              // execute script
              error = Misc_executeScript(String_cString(script),
                                         CALLBACK(executeIOOutput,NULL),
                                         CALLBACK(executeIOOutput,NULL)
                                        );
              String_delete(script);
            }
            else
            {
              error = ERROR_EXPAND_TEMPLATE;
            }

            printInfo(1,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
          }
        }
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(timestamp);
    UNUSED_VARIABLE(initialFlag);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return error;
}

LOCAL Errors StorageSFTP_postProcess(StorageHandle *storageHandle,
                                     ConstString   archiveName,
                                     time_t        timestamp,
                                     bool          finalFlag
                                    )
{
  Errors error;
  #ifdef HAVE_SSH2
    TextMacro textMacros[2];
    String    script;
  #endif /* HAVE_SSH2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);

  error = ERROR_NONE;

  #ifdef HAVE_SSH2
    {

      if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
      {
        if (!finalFlag)
        {
          // init macros
          TEXT_MACRO_N_STRING (textMacros[0],"%file",  archiveName,                NULL);
          TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageHandle->volumeNumber,NULL);

          // write post-process
          if (globalOptions.sftp.writePostProcessCommand != NULL)
          {
            printInfo(1,"Write post-processing...");

            // get script
            script = expandTemplate(String_cString(globalOptions.sftp.writePostProcessCommand),
                                    EXPAND_MACRO_MODE_STRING,
                                    timestamp,
                                    finalFlag,
                                    textMacros,
                                    SIZE_OF_ARRAY(textMacros)
                                   );
            if (script != NULL)
            {
              // execute script
              error = Misc_executeScript(String_cString(script),
                                         CALLBACK(executeIOOutput,NULL),
                                         CALLBACK(executeIOOutput,NULL)
                                        );
              String_delete(script);
            }
            else
            {
              error = ERROR_EXPAND_TEMPLATE;
            }

            printInfo(1,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
          }
        }
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(timestamp);
    UNUSED_VARIABLE(finalFlag);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return error;
}

LOCAL bool StorageSFTP_exists(StorageHandle *storageHandle, ConstString archiveName)
{
  assert(storageHandle != NULL);
  assert(!String_isEmpty(archiveName));

HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
  UNUSED_VARIABLE(storageHandle);

  return File_exists(archiveName);
}

LOCAL Errors StorageSFTP_create(StorageArchiveHandle *storageArchiveHandle,
                                ConstString   archiveName,
                                uint64        archiveSize
                               )
{
  #ifdef HAVE_SSH2
    Errors error;
  #endif /* HAVE_SSH2 */

  assert(storageArchiveHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(archiveSize);

  #ifdef HAVE_SSH2
    {
      // init variables
      storageArchiveHandle->sftp.oldSendCallback        = NULL;
      storageArchiveHandle->sftp.oldReceiveCallback     = NULL;
      storageArchiveHandle->sftp.totalSentBytes         = 0LL;
      storageArchiveHandle->sftp.totalReceivedBytes     = 0LL;
      storageArchiveHandle->sftp.sftp                   = NULL;
      storageArchiveHandle->sftp.sftpHandle             = NULL;
      storageArchiveHandle->sftp.index                  = 0LL;
      storageArchiveHandle->sftp.size                   = 0LL;
      storageArchiveHandle->sftp.readAheadBuffer.offset = 0LL;
      storageArchiveHandle->sftp.readAheadBuffer.length = 0L;
      DEBUG_ADD_RESOURCE_TRACE(&storageArchiveHandle->sftp,sizeof(storageArchiveHandle->sftp));

      // connect
      error = Network_connect(&storageArchiveHandle->sftp.socketHandle,
                              SOCKET_TYPE_SSH,
                              storageArchiveHandle->storageHandle->storageSpecifier.hostName,
                              storageArchiveHandle->storageHandle->storageSpecifier.hostPort,
                              storageArchiveHandle->storageHandle->storageSpecifier.loginName,
                              storageArchiveHandle->storageHandle->storageSpecifier.loginPassword,
                              storageArchiveHandle->storageHandle->sftp.publicKey.data,
                              storageArchiveHandle->storageHandle->sftp.publicKey.length,
                              storageArchiveHandle->storageHandle->sftp.privateKey.data,
                              storageArchiveHandle->storageHandle->sftp.privateKey.length,
                              0
                             );
      if (error != ERROR_NONE)
      {
        DEBUG_REMOVE_RESOURCE_TRACE(&storageArchiveHandle->sftp,sizeof(storageArchiveHandle->sftp));
        return error;
      }
      libssh2_session_set_timeout(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle),READ_TIMEOUT);

      // install send/receive callback to track number of sent/received bytes
      storageArchiveHandle->sftp.totalSentBytes     = 0LL;
      storageArchiveHandle->sftp.totalReceivedBytes = 0LL;
      (*(libssh2_session_abstract(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle)))) = storageArchiveHandle;
      storageArchiveHandle->sftp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle),LIBSSH2_CALLBACK_SEND,sftpSendCallback   );
      storageArchiveHandle->sftp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle),LIBSSH2_CALLBACK_RECV,sftpReceiveCallback);

      // init session
      storageArchiveHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle));
      if (storageArchiveHandle->sftp.sftp == NULL)
      {
        char *sshErrorText;

        libssh2_session_last_error(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle),&sshErrorText,NULL,0);
        error = ERRORX_(SSH,
                        libssh2_session_last_errno(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle)),
                        "%s",
                        sshErrorText
                       );
        Network_disconnect(&storageArchiveHandle->sftp.socketHandle);
        DEBUG_REMOVE_RESOURCE_TRACE(&storageArchiveHandle->sftp,sizeof(storageArchiveHandle->sftp));
        return error;
      }

      if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
      {
        // create file
        storageArchiveHandle->sftp.sftpHandle = libssh2_sftp_open(storageArchiveHandle->sftp.sftp,
                                                               String_cString(archiveName),
                                                               LIBSSH2_FXF_CREAT|LIBSSH2_FXF_WRITE|LIBSSH2_FXF_TRUNC,
// ???
LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR
                                                              );
        if (storageArchiveHandle->sftp.sftpHandle == NULL)
        {
          char *sshErrorText;

          libssh2_session_last_error(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle),&sshErrorText,NULL,0);
          error = ERRORX_(SSH,
                          libssh2_session_last_errno(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle)),
                          "%s",
                          sshErrorText
                         );
          libssh2_sftp_shutdown(storageArchiveHandle->sftp.sftp);
          Network_disconnect(&storageArchiveHandle->sftp.socketHandle);
          DEBUG_REMOVE_RESOURCE_TRACE(&storageArchiveHandle->sftp,sizeof(storageArchiveHandle->sftp));
          return error;
        }
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageArchiveHandle);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(archiveSize);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return ERROR_NONE;
}

LOCAL Errors StorageSFTP_open(StorageArchiveHandle *storageArchiveHandle,
                              ConstString          archiveName
                             )
{
  #ifdef HAVE_SSH2
    Errors error;
  #endif /* HAVE_SSH2 */

  assert(storageArchiveHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(!String_isEmpty(archiveName));

  #ifdef HAVE_SSH2
    {
      LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;

      // init variables
      storageArchiveHandle->sftp.oldSendCallback        = NULL;
      storageArchiveHandle->sftp.oldReceiveCallback     = NULL;
      storageArchiveHandle->sftp.totalSentBytes         = 0LL;
      storageArchiveHandle->sftp.totalReceivedBytes     = 0LL;
      storageArchiveHandle->sftp.sftp                   = NULL;
      storageArchiveHandle->sftp.sftpHandle             = NULL;
      storageArchiveHandle->sftp.index                  = 0LL;
      storageArchiveHandle->sftp.size                   = 0LL;
      storageArchiveHandle->sftp.readAheadBuffer.offset = 0LL;
      storageArchiveHandle->sftp.readAheadBuffer.length = 0L;
      DEBUG_ADD_RESOURCE_TRACE(&storageArchiveHandle->sftp,sizeof(storageArchiveHandle->sftp));

      // allocate read-ahead buffer
      storageArchiveHandle->sftp.readAheadBuffer.data = (byte*)malloc(MAX_BUFFER_SIZE);
      if (storageArchiveHandle->sftp.readAheadBuffer.data == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }

      // connect
      error = Network_connect(&storageArchiveHandle->sftp.socketHandle,
                              SOCKET_TYPE_SSH,
                              storageArchiveHandle->storageHandle->storageSpecifier.hostName,
                              storageArchiveHandle->storageHandle->storageSpecifier.hostPort,
                              storageArchiveHandle->storageHandle->storageSpecifier.loginName,
                              storageArchiveHandle->storageHandle->storageSpecifier.loginPassword,
                              storageArchiveHandle->storageHandle->sftp.publicKey.data,
                              storageArchiveHandle->storageHandle->sftp.publicKey.length,
                              storageArchiveHandle->storageHandle->sftp.privateKey.data,
                              storageArchiveHandle->storageHandle->sftp.privateKey.length,
                              0
                             );
      if (error != ERROR_NONE)
      {
        free(storageArchiveHandle->sftp.readAheadBuffer.data);
        DEBUG_REMOVE_RESOURCE_TRACE(&storageArchiveHandle->sftp,sizeof(storageArchiveHandle->sftp));
        return error;
      }
      libssh2_session_set_timeout(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle),READ_TIMEOUT);

      // install send/receive callback to track number of sent/received bytes
      storageArchiveHandle->sftp.totalSentBytes     = 0LL;
      storageArchiveHandle->sftp.totalReceivedBytes = 0LL;
      (*(libssh2_session_abstract(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle)))) = storageArchiveHandle;
      storageArchiveHandle->sftp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle),LIBSSH2_CALLBACK_SEND,sftpSendCallback   );
      storageArchiveHandle->sftp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle),LIBSSH2_CALLBACK_RECV,sftpReceiveCallback);

      // init session
      storageArchiveHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle));
      if (storageArchiveHandle->sftp.sftp == NULL)
      {
        error = ERROR_(SSH,libssh2_session_last_errno(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle)));
        Network_disconnect(&storageArchiveHandle->sftp.socketHandle);
        free(storageArchiveHandle->sftp.readAheadBuffer.data);
        DEBUG_REMOVE_RESOURCE_TRACE(&storageArchiveHandle->sftp,sizeof(storageArchiveHandle->sftp));
        return error;
      }

      // open file
      storageArchiveHandle->sftp.sftpHandle = libssh2_sftp_open(storageArchiveHandle->sftp.sftp,
                                                             String_cString(archiveName),
                                                             LIBSSH2_FXF_READ,
                                                             0
                                                            );
      if (storageArchiveHandle->sftp.sftpHandle == NULL)
      {
        char *sshErrorText;

        libssh2_session_last_error(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle),&sshErrorText,NULL,0);
        error = ERRORX_(SSH,
                        libssh2_session_last_errno(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle)),
                        "%s",
                        sshErrorText
                       );
        libssh2_sftp_shutdown(storageArchiveHandle->sftp.sftp);
        Network_disconnect(&storageArchiveHandle->sftp.socketHandle);
        free(storageArchiveHandle->sftp.readAheadBuffer.data);
        DEBUG_REMOVE_RESOURCE_TRACE(&storageArchiveHandle->sftp,sizeof(storageArchiveHandle->sftp));
        return error;
      }

      // get file size
      if (libssh2_sftp_fstat(storageArchiveHandle->sftp.sftpHandle,
                             &sftpAttributes
                            ) != 0
         )
      {
        char *sshErrorText;

        libssh2_session_last_error(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle),&sshErrorText,NULL,0);
        error = ERRORX_(SSH,
                        libssh2_session_last_errno(Network_getSSHSession(&storageArchiveHandle->sftp.socketHandle)),
                        "%s",
                        sshErrorText
                       );
        libssh2_sftp_close(storageArchiveHandle->sftp.sftpHandle);
        libssh2_sftp_shutdown(storageArchiveHandle->sftp.sftp);
        Network_disconnect(&storageArchiveHandle->sftp.socketHandle);
        free(storageArchiveHandle->sftp.readAheadBuffer.data);
        DEBUG_REMOVE_RESOURCE_TRACE(&storageArchiveHandle->sftp,sizeof(storageArchiveHandle->sftp));
        return error;
      }
      storageArchiveHandle->sftp.size = sftpAttributes.filesize;
    }

    return ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageArchiveHandle);
    UNUSED_VARIABLE(archiveName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL void StorageSFTP_close(StorageArchiveHandle *storageArchiveHandle)
{
  assert(storageArchiveHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->sftp);
  #endif /* HAVE_SSH2 */
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);

  #ifdef HAVE_SSH2
    switch (storageArchiveHandle->mode)
    {
      case STORAGE_MODE_READ:
        (void)libssh2_sftp_close(storageArchiveHandle->sftp.sftpHandle);
        free(storageArchiveHandle->sftp.readAheadBuffer.data);
        break;
      case STORAGE_MODE_WRITE:
        if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
        {
          (void)libssh2_sftp_close(storageArchiveHandle->sftp.sftpHandle);
        }
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
    libssh2_sftp_shutdown(storageArchiveHandle->sftp.sftp);
    Network_disconnect(&storageArchiveHandle->sftp.socketHandle);
    DEBUG_REMOVE_RESOURCE_TRACE(&storageArchiveHandle->sftp,sizeof(storageArchiveHandle->sftp));
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageArchiveHandle);
  #endif /* HAVE_SSH2 */
}

LOCAL bool StorageSFTP_eof(StorageArchiveHandle *storageArchiveHandle)
{
  assert(storageArchiveHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->sftp);
  #endif /* HAVE_SSH2 */
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->mode == STORAGE_MODE_READ);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);

  #ifdef HAVE_SSH2
    if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
    {
      return storageArchiveHandle->sftp.index >= storageArchiveHandle->sftp.size;
    }
    else
    {
      return TRUE;
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageArchiveHandle);

    return TRUE;
  #endif /* HAVE_SSH2 */
}

LOCAL Errors StorageSFTP_read(StorageArchiveHandle *storageArchiveHandle,
                              void          *buffer,
                              ulong         size,
                              ulong         *bytesRead
                             )
{
  Errors error;

  assert(storageArchiveHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->sftp);
  #endif /* HAVE_SSH2 */
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->mode == STORAGE_MODE_READ);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(buffer != NULL);

//fprintf(stderr,"%s,%d: size=%lu\n",__FILE__,__LINE__,size);
  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    {
      ulong   index;
      ulong   bytesAvail;
      ulong   length;
      uint64  startTimestamp,endTimestamp;
      uint64  startTotalReceivedBytes,endTotalReceivedBytes;
      ssize_t n;

      if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
      {
        assert(storageArchiveHandle->sftp.sftpHandle != NULL);
        assert(storageArchiveHandle->sftp.readAheadBuffer.data != NULL);

        while (   (size > 0)
               && (error == ERROR_NONE)
              )
        {
          // copy as much data as available from read-ahead buffer
          if (   (storageArchiveHandle->sftp.index >= storageArchiveHandle->sftp.readAheadBuffer.offset)
              && (storageArchiveHandle->sftp.index < (storageArchiveHandle->sftp.readAheadBuffer.offset+storageArchiveHandle->sftp.readAheadBuffer.length))
             )
          {
            // copy data from read-ahead buffer
            index      = (ulong)(storageArchiveHandle->sftp.index-storageArchiveHandle->sftp.readAheadBuffer.offset);
            bytesAvail = MIN(size,storageArchiveHandle->sftp.readAheadBuffer.length-index);
            memcpy(buffer,storageArchiveHandle->sftp.readAheadBuffer.data+index,bytesAvail);

            // adjust buffer, size, bytes read, index
            buffer = (byte*)buffer+bytesAvail;
            size -= bytesAvail;
            if (bytesRead != NULL) (*bytesRead) += bytesAvail;
            storageArchiveHandle->sftp.index += (uint64)bytesAvail;
          }

          // read rest of data
          if (size > 0)
          {
            assert(storageArchiveHandle->sftp.index >= (storageArchiveHandle->sftp.readAheadBuffer.offset+storageArchiveHandle->sftp.readAheadBuffer.length));

            // get max. number of bytes to receive in one step
            if (storageArchiveHandle->storageHandle->sftp.bandWidthLimiter.maxBandWidthList != NULL)
            {
              length = MIN(storageArchiveHandle->storageHandle->sftp.bandWidthLimiter.blockSize,size);
            }
            else
            {
              length = size;
            }
            assert(length > 0L);

            // get start time, start received bytes
            startTimestamp          = Misc_getTimestamp();
            startTotalReceivedBytes = storageArchiveHandle->sftp.totalReceivedBytes;

            #if   defined(HAVE_SSH2_SFTP_SEEK64)
              libssh2_sftp_seek64(storageArchiveHandle->sftp.sftpHandle,storageArchiveHandle->sftp.index);
            #elif defined(HAVE_SSH2_SFTP_SEEK2)
              libssh2_sftp_seek2(storageArchiveHandle->sftp.sftpHandle,storageArchiveHandle->sftp.index);
            #else /* not HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
              libssh2_sftp_seek(storageArchiveHandle->sftp.sftpHandle,storageArchiveHandle->sftp.index);
            #endif /* HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */

            if (length <= MAX_BUFFER_SIZE)
            {
              // read into read-ahead buffer
              n = libssh2_sftp_read(storageArchiveHandle->sftp.sftpHandle,
                                    (char*)storageArchiveHandle->sftp.readAheadBuffer.data,
                                    MIN((size_t)(storageArchiveHandle->sftp.size-storageArchiveHandle->sftp.index),MAX_BUFFER_SIZE)
                                   );
              if (n < 0)
              {
                error = ERROR_(IO_ERROR,errno);
                break;
              }
              storageArchiveHandle->sftp.readAheadBuffer.offset = storageArchiveHandle->sftp.index;
              storageArchiveHandle->sftp.readAheadBuffer.length = (ulong)n;

              // copy data from read-ahead buffer
              bytesAvail = MIN(length,storageArchiveHandle->sftp.readAheadBuffer.length);
              memcpy(buffer,storageArchiveHandle->sftp.readAheadBuffer.data,bytesAvail);

              // adjust buffer, size, bytes read, index
              buffer = (byte*)buffer+bytesAvail;
              size -= bytesAvail;
              if (bytesRead != NULL) (*bytesRead) += bytesAvail;
              storageArchiveHandle->sftp.index += (uint64)bytesAvail;
            }
            else
            {
              // read direct
              n = libssh2_sftp_read(storageArchiveHandle->sftp.sftpHandle,
                                    buffer,
                                    length
                                   );
              if (n < 0)
              {
                error = ERROR_(IO_ERROR,errno);
                break;
              }
              bytesAvail = (ulong)n;

              // adjust buffer, size, bytes read, index
              buffer = (byte*)buffer+(ulong)bytesAvail;
              size -= (ulong)bytesAvail;
              if (bytesRead != NULL) (*bytesRead) += (ulong)bytesAvail;
              storageArchiveHandle->sftp.index += (uint64)bytesAvail;
            }

            // get end time, end received bytes
            endTimestamp          = Misc_getTimestamp();
            endTotalReceivedBytes = storageArchiveHandle->sftp.totalReceivedBytes;
            assert(endTotalReceivedBytes >= startTotalReceivedBytes);

            /* limit used band width if requested (note: when the system time is
               changing endTimestamp may become smaller than startTimestamp;
               thus do not check this with an assert())
            */
            if (endTimestamp >= startTimestamp)
            {
              limitBandWidth(&storageArchiveHandle->storageHandle->sftp.bandWidthLimiter,
                             endTotalReceivedBytes-startTotalReceivedBytes,
                             endTimestamp-startTimestamp
                            );
            }
          }
        }
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageArchiveHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(size);
    UNUSED_VARIABLE(bytesRead);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageSFTP_write(StorageArchiveHandle *storageArchiveHandle,
                               const void    *buffer,
                               ulong         size
                              )
{
  Errors error;

  assert(storageArchiveHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->sftp);
  #endif /* HAVE_SSH2 */
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->mode == STORAGE_MODE_WRITE);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(buffer != NULL);

  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    {
      ulong   writtenBytes;
      ulong   length;
      uint64  startTimestamp,endTimestamp;
      uint64  startTotalSentBytes,endTotalSentBytes;
      ssize_t n;

      if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
      {
        assert(storageArchiveHandle->sftp.sftpHandle != NULL);

        writtenBytes = 0L;
        while (writtenBytes < size)
        {
          // get max. number of bytes to send in one step
          if (storageArchiveHandle->storageHandle->sftp.bandWidthLimiter.maxBandWidthList != NULL)
          {
            length = MIN(storageArchiveHandle->storageHandle->sftp.bandWidthLimiter.blockSize,size-writtenBytes);
          }
          else
          {
            length = size-writtenBytes;
          }
          assert(length > 0L);

          // get start time, start received bytes
          startTimestamp      = Misc_getTimestamp();
          startTotalSentBytes = storageArchiveHandle->sftp.totalSentBytes;

          // send data
          do
          {
            n = libssh2_sftp_write(storageArchiveHandle->sftp.sftpHandle,
                                   buffer,
                                   length
                                  );
            if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*MISC_US_PER_MS);
          }
          while (n == LIBSSH2_ERROR_EAGAIN);

          // get end time, end received bytes
          endTimestamp      = Misc_getTimestamp();
          endTotalSentBytes = storageArchiveHandle->sftp.totalSentBytes;
          assert(endTotalSentBytes >= startTotalSentBytes);

// ??? is it possible in blocking-mode that write() return 0 and this is not an error?
#if 1
          if      (n == 0)
          {
            // should not happen in blocking-mode: bug? libssh2 API changed somewhere between 0.18 and 1.2.4? => wait for data
            if (!waitSSHSessionSocket(&storageArchiveHandle->sftp.socketHandle))
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
          writtenBytes += (ulong)n;

          /* limit used band width if requested (note: when the system time is
             changing endTimestamp may become smaller than startTimestamp;
             thus do not check this with an assert())
          */
          if (endTimestamp >= startTimestamp)
          {
            limitBandWidth(&storageArchiveHandle->storageHandle->sftp.bandWidthLimiter,
                           endTotalSentBytes-startTotalSentBytes,
                           endTimestamp-startTimestamp
                          );
          }
        }
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageArchiveHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(size);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL uint64 StorageSFTP_getSize(StorageArchiveHandle *storageArchiveHandle)
{
  uint64 size;

  assert(storageArchiveHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->sftp);
  #endif /* HAVE_SSH2 */
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);

  size = 0LL;
  #ifdef HAVE_SSH2
    if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
    {
      size = storageArchiveHandle->sftp.size;
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageArchiveHandle);
  #endif /* HAVE_SSH2 */

  return size;
}

LOCAL Errors StorageSFTP_tell(StorageArchiveHandle *storageArchiveHandle,
                              uint64        *offset
                             )
{
  Errors error;

  assert(storageArchiveHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->sftp);
  #endif /* HAVE_SSH2 */
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
    {
      (*offset) = storageArchiveHandle->sftp.index;
      error     = ERROR_NONE;
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageArchiveHandle);
    UNUSED_VARIABLE(offset);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageSFTP_seek(StorageArchiveHandle *storageArchiveHandle,
                              uint64        offset
                             )
{
  Errors error;

  assert(storageArchiveHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->sftp);
  #endif /* HAVE_SSH2 */
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);

  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
    {
      assert(storageArchiveHandle->sftp.sftpHandle != NULL);
      assert(storageArchiveHandle->sftp.readAheadBuffer.data != NULL);

      if      (offset > storageArchiveHandle->sftp.index)
      {
        uint64 skip;
        uint64 i;
        uint64 n;

        skip = offset-storageArchiveHandle->sftp.index;
        if (skip > 0LL)
        {
          // skip data in read-ahead buffer
          if (   (storageArchiveHandle->sftp.index >= storageArchiveHandle->sftp.readAheadBuffer.offset)
              && (storageArchiveHandle->sftp.index < (storageArchiveHandle->sftp.readAheadBuffer.offset+storageArchiveHandle->sftp.readAheadBuffer.length))
             )
          {
            i = storageArchiveHandle->sftp.index-storageArchiveHandle->sftp.readAheadBuffer.offset;
            n = MIN(skip,storageArchiveHandle->sftp.readAheadBuffer.length-i);
            skip -= n;
            storageArchiveHandle->sftp.index += (uint64)n;
          }

          if (skip > 0LL)
          {
            #if   defined(HAVE_SSH2_SFTP_SEEK64)
              libssh2_sftp_seek64(storageArchiveHandle->sftp.sftpHandle,offset);
            #elif defined(HAVE_SSH2_SFTP_SEEK2)
              libssh2_sftp_seek2(storageArchiveHandle->sftp.sftpHandle,offset);
            #else /* not HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
              libssh2_sftp_seek(storageArchiveHandle->sftp.sftpHandle,(size_t)offset);
            #endif /* HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
            storageArchiveHandle->sftp.index = offset;
          }
        }
      }
      else if (offset < storageArchiveHandle->sftp.index)
      {
// NYI: ??? support seek backward
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageArchiveHandle);
    UNUSED_VARIABLE(offset);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageSFTP_delete(StorageHandle *storageHandle,
                                ConstString   archiveName
                               )
{
  Errors       error;
  #ifdef HAVE_SSH2
    SocketHandle        socketHandle;
    LIBSSH2_SFTP        *sftp;
//    LIBSSH2_SFTP_HANDLE *sftpHandle;
  #endif /* HAVE_SSH2 */

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(!String_isEmpty(archiveName));

  error = ERROR_UNKNOWN;
  #ifdef HAVE_SSH2
    error = Network_connect(&socketHandle,
                            SOCKET_TYPE_SSH,
                            storageHandle->storageSpecifier.hostName,
                            storageHandle->storageSpecifier.hostPort,
                            storageHandle->storageSpecifier.loginName,
                            storageHandle->storageSpecifier.loginPassword,
                            storageHandle->sftp.publicKey.data,
                            storageHandle->sftp.publicKey.length,
                            storageHandle->sftp.privateKey.data,
                            storageHandle->sftp.privateKey.length,
                            0
                           );
    if (error == ERROR_NONE)
    {
      libssh2_session_set_timeout(Network_getSSHSession(&socketHandle),READ_TIMEOUT);

      // init session
      sftp = libssh2_sftp_init(Network_getSSHSession(&socketHandle));
      if (sftp != NULL)
      {
        if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
        {
          // delete file
          if (libssh2_sftp_unlink(sftp,
                                  String_cString(archiveName)
                                 ) == 0
             )
          {
            error = ERROR_NONE;
          }
          else
          {
             char *sshErrorText;

             libssh2_session_last_error(Network_getSSHSession(&socketHandle),&sshErrorText,NULL,0);
             error = ERRORX_(SSH,
                             libssh2_session_last_errno(Network_getSSHSession(&socketHandle)),
                             "%s",
                             sshErrorText
                            );
          }
        }
        else
        {
          error = ERROR_NONE;
        }

        libssh2_sftp_shutdown(sftp);
      }
      else
      {
        char *sshErrorText;

        libssh2_session_last_error(Network_getSSHSession(&socketHandle),&sshErrorText,NULL,0);
        error = ERRORX_(SSH,
                        libssh2_session_last_errno(Network_getSSHSession(&socketHandle)),
                        "%s",
                        sshErrorText
                       );
        Network_disconnect(&socketHandle);
      }
      Network_disconnect(&socketHandle);
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(archiveName);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

#if 0
still not complete
LOCAL Errors StorageSFTP_getFileInfo(StorageHandle *storageHandle,
                                     ConstString   fileName,
                                     FileInfo      *fileInfo
                                    )
{
  String infoFileName;
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(fileInfo != NULL);

  infoFileName = (fileName != NULL) ? fileName : storageHandle->storageSpecifier.archiveName;
  memset(fileInfo,0,sizeof(fileInfo));

  error = ERROR_UNKNOWN;
  #ifdef HAVE_SSH2
    {
      LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;

      error = Network_connect(&storageHandle->sftp.socketHandle,
                              SOCKET_TYPE_SSH,
                              storageHandle->storageSpecifier.hostName,
                              storageHandle->storageSpecifier.hostPort,
                              storageHandle->storageSpecifier.loginName,
                              storageHandle->storageSpecifier.loginPassword,
                              sshServer.publicKey,
                              sshServer.publicKeyLength,
                              sshServer.privateKey,
                              sshServer.privateKeyLength,
                              0
                             );
      if (error == ERROR_NONE)
      {
        libssh2_session_set_timeout(Network_getSSHSession(&storageHandle->sftp.socketHandle),READ_TIMEOUT);

        // init session
        storageHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageHandle->sftp.socketHandle));
        if (storageHandle->sftp.sftp != NULL)
        {
          // get file fino
          if (libssh2_sftp_lstat(storageHandle->sftp.sftp,
                                 String_cString(infoFileName),
                                 &sftpAttributes
                                ) == 0
             )
          {
            if      (LIBSSH2_SFTP_S_ISREG (sftpAttributes.flags)) fileInfo->type = FILE_TYPE_FILE;
            else if (LIBSSH2_SFTP_S_ISDIR (sftpAttributes.flags)) fileInfo->type = FILE_TYPE_DIRECTORY;
            else if (LIBSSH2_SFTP_S_ISLNK (sftpAttributes.flags)) fileInfo->type = FILE_TYPE_LINK;
            else if (LIBSSH2_SFTP_S_ISCHR (sftpAttributes.flags)) fileInfo->type = FILE_TYPE_SPECIAL;
            else if (LIBSSH2_SFTP_S_ISBLK (sftpAttributes.flags)) fileInfo->type = FILE_TYPE_SPECIAL;
            else if (LIBSSH2_SFTP_S_ISFIFO(sftpAttributes.flags)) fileInfo->type = FILE_TYPE_SPECIAL;
            else if (LIBSSH2_SFTP_S_ISSOCK(sftpAttributes.flags)) fileInfo->type = FILE_TYPE_SPECIAL;
            else                                                  fileInfo->type = FILE_TYPE_UNKNOWN;
            fileInfo->size            = (uint64)sftpAttributes.filesize;
            fileInfo->timeLastAccess  = (uint64)sftpAttributes.atime;
            fileInfo->timeModified    = (uint64)sftpAttributes.mtime;
            fileInfo->userId          = sftpAttributes.uid;
            fileInfo->groupId         = sftpAttributes.gid;
            fileInfo->permission      = (FilePermission)sftpAttributes.permissions;

            error = ERROR_NONE;
          }
          else
          {
             char *sshErrorText;

             libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&sshErrorText,NULL,0);
             error = ERRORX_(SSH,
                             libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle)),
                             "%s",
                             sshErrorText
                            );
          }

          libssh2_sftp_shutdown(storageHandle->sftp.sftp);
        }
        else
        {
          char *sshErrorText;

          libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&sshErrorText,NULL,0);
          error = ERRORX_(SSH,
                          libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle)),
                          "%s",
                          sshErrorText
                         );
          Network_disconnect(&storageHandle->sftp.socketHandle);
        }
        Network_disconnect(&storageHandle->sftp.socketHandle);
      }
    }
  #else /* not HAVE_SSH2 */
    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageSFTP_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                           const StorageSpecifier     *storageSpecifier,
                                           const JobOptions           *jobOptions,
                                           ServerConnectionPriorities serverConnectionPriority,
                                           ConstString                archiveName
                                          )
{
  #ifdef HAVE_SSH2
    AutoFreeList autoFreeList;
    Errors       error;
    SSHServer    sshServer;
  #endif /* HAVE_SSH2 */

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_SFTP);

  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(serverConnectionPriority);

  #ifdef HAVE_SSH2
    // init variables
    AutoFree_init(&autoFreeList);
    storageDirectoryListHandle->type               = STORAGE_TYPE_SFTP;
    storageDirectoryListHandle->sftp.pathName      = String_new();
    storageDirectoryListHandle->sftp.buffer        = (char*)malloc(MAX_FILENAME_LENGTH);
    if (storageDirectoryListHandle->sftp.buffer == NULL)
    {
      String_delete(storageDirectoryListHandle->sftp.pathName);
      AutoFree_cleanup(&autoFreeList);
      return ERROR_INSUFFICIENT_MEMORY;
    }
    storageDirectoryListHandle->sftp.bufferLength  = 0;
    storageDirectoryListHandle->sftp.entryReadFlag = FALSE;
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->sftp.buffer,{ free(storageDirectoryListHandle->sftp.buffer); });
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->sftp.pathName,{ String_delete(storageDirectoryListHandle->sftp.pathName); });

    // set pathname
    String_set(storageDirectoryListHandle->sftp.pathName,archiveName);

    // get SSH server settings
    getSSHServerSettings(storageDirectoryListHandle->storageSpecifier.hostName,jobOptions,&sshServer);
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_set(storageDirectoryListHandle->storageSpecifier.loginName,sshServer.loginName);
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("USER"));
    if (storageDirectoryListHandle->storageSpecifier.hostPort == 0) storageDirectoryListHandle->storageSpecifier.hostPort = sshServer.port;
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }
    if (sshServer.publicKey.data == NULL)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_SSH_PUBLIC_KEY;
    }
    if (sshServer.privateKey.data == NULL)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_SSH_PRIVATE_KEY;
    }

    // open network connection
    error = ERROR_UNKNOWN;
    if ((error == ERROR_UNKNOWN) && !Password_isEmpty(sshServer.password))
    {
      error = Network_connect(&storageDirectoryListHandle->sftp.socketHandle,
                              SOCKET_TYPE_SSH,
                              storageDirectoryListHandle->storageSpecifier.hostName,
                              storageDirectoryListHandle->storageSpecifier.hostPort,
                              storageDirectoryListHandle->storageSpecifier.loginName,
                              sshServer.password,
                              sshServer.publicKey.data,
                              sshServer.publicKey.length,
                              sshServer.privateKey.data,
                              sshServer.privateKey.length,
                              0
                             );
    }
    if (error == ERROR_UNKNOWN)
    {
      // initialize default password
      while (   (error != ERROR_NONE)
             && initSSHLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                             storageDirectoryListHandle->storageSpecifier.loginName,
                             storageDirectoryListHandle->storageSpecifier.loginPassword,
                             jobOptions,
//TODO
                             CALLBACK(NULL,NULL) // CALLBACK(storageHandle->getPasswordFunction,storageHandle->getPasswordUserData)
                            )
         )
      {
        error = Network_connect(&storageDirectoryListHandle->sftp.socketHandle,
                                SOCKET_TYPE_SSH,
                                storageDirectoryListHandle->storageSpecifier.hostName,
                                storageDirectoryListHandle->storageSpecifier.hostPort,
                                storageDirectoryListHandle->storageSpecifier.loginName,
                                storageDirectoryListHandle->storageSpecifier.loginPassword,
                                sshServer.publicKey.data,
                                sshServer.publicKey.length,
                                sshServer.privateKey.data,
                                sshServer.privateKey.length,
                                0
                               );
      }
      if (error != ERROR_NONE)
      {
        error = (!Password_isEmpty(sshServer.password) || !Password_isEmpty(defaultSSHPassword))
                  ? ERRORX_(INVALID_SSH_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName))
                  : ERRORX_(NO_SSH_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName));
      }

      // store passwrd as default SSH password
      if (error == ERROR_NONE)
      {
        if (defaultSSHPassword == NULL) defaultSSHPassword = Password_new();
        Password_set(defaultSSHPassword,storageDirectoryListHandle->storageSpecifier.loginPassword);
      }
    }
    assert(error != ERROR_UNKNOWN);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    libssh2_session_set_timeout(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle),READ_TIMEOUT);
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->sftp.socketHandle,{ Network_disconnect(&storageDirectoryListHandle->sftp.socketHandle); });

    // init SFTP session
    storageDirectoryListHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle));
    if (storageDirectoryListHandle->sftp.sftp == NULL)
    {
      error = ERROR_(SSH,libssh2_session_last_errno(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle)));
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->sftp.sftp,{ libssh2_sftp_shutdown(storageDirectoryListHandle->sftp.sftp); });

    // open directory for reading
    storageDirectoryListHandle->sftp.sftpHandle = libssh2_sftp_opendir(storageDirectoryListHandle->sftp.sftp,
                                                                       String_cString(storageDirectoryListHandle->sftp.pathName)
                                                                      );
    if (storageDirectoryListHandle->sftp.sftpHandle == NULL)
    {
      error = ERROR_(SSH,libssh2_session_last_errno(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle)));
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // free resources

    AutoFree_done(&autoFreeList);

    return ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageDirectoryListHandle);
    UNUSED_VARIABLE(storageSpecifier);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(serverConnectionPriority);
    UNUSED_VARIABLE(archiveName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL void StorageSFTP_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);

  #ifdef HAVE_SSH2
    libssh2_sftp_closedir(storageDirectoryListHandle->sftp.sftpHandle);
    libssh2_sftp_shutdown(storageDirectoryListHandle->sftp.sftp);
    Network_disconnect(&storageDirectoryListHandle->sftp.socketHandle);
    free(storageDirectoryListHandle->sftp.buffer);
    String_delete(storageDirectoryListHandle->sftp.pathName);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageDirectoryListHandle);
  #endif /* HAVE_SSH2 */
}

LOCAL bool StorageSFTP_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  bool endOfDirectoryFlag;

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);

  endOfDirectoryFlag = TRUE;
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
    UNUSED_VARIABLE(storageDirectoryListHandle);
  #endif /* HAVE_SSH2 */

  return endOfDirectoryFlag;
}

LOCAL Errors StorageSFTP_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                           String                     fileName,
                                           FileInfo                   *fileInfo
                                          )
{
  Errors error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);

  error = ERROR_NONE;
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
            error = ERROR_(IO_ERROR,errno);
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
          #ifdef HAVE_S_ISLNK
          else if (S_ISLNK(storageDirectoryListHandle->sftp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_LINK;
          }
          #endif /* HAVE_S_ISLNK */
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
          #ifdef HAVE_S_ISSOCK
          else if (S_ISSOCK(storageDirectoryListHandle->sftp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_SPECIAL;
            fileInfo->specialType = FILE_SPECIAL_TYPE_SOCKET;
          }
          #endif /* HAVE_S_ISSOCK */
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
          memset(&fileInfo->cast,0,sizeof(FileCast));
        }

        storageDirectoryListHandle->sftp.entryReadFlag = FALSE;
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageDirectoryListHandle);
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(fileInfo);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
