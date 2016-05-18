/***********************************************************************\
*
* $Revision: 4012 $
* $Date: 2015-04-28 19:02:40 +0200 (Tue, 28 Apr 2015) $
* $Author: torsten $
* Contents: storage SCP functions
* Systems: all
*
\***********************************************************************/

#define __STORAGE_IMPLEMENATION__

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

#ifdef HAVE_SSH2
/***********************************************************************\
* Name   : scpSendCallback
* Purpose: scp send callback: count total send bytes and pass to
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

LOCAL LIBSSH2_SEND_FUNC(scpSendCallback)
{
  StorageArchiveHandle *storageArchiveHandle;
  ssize_t              n;

  assert(abstract != NULL);

  storageArchiveHandle = *((StorageArchiveHandle**)abstract);
  assert(storageArchiveHandle != NULL);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->scp);
  assert(storageArchiveHandle->scp.oldSendCallback != NULL);

  n = storageArchiveHandle->scp.oldSendCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageArchiveHandle->scp.totalSentBytes += (uint64)n;

  return n;
}

/***********************************************************************\
* Name   : scpReceiveCallback
* Purpose: scp receive callback: count total received bytes and pass to
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

LOCAL LIBSSH2_RECV_FUNC(scpReceiveCallback)
{
  StorageArchiveHandle *storageArchiveHandle;
  ssize_t              n;

  assert(abstract != NULL);

  storageArchiveHandle = *((StorageArchiveHandle**)abstract);
  assert(storageArchiveHandle != NULL);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->scp);
  assert(storageArchiveHandle->scp.oldReceiveCallback != NULL);

  n = storageArchiveHandle->scp.oldReceiveCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageArchiveHandle->scp.totalReceivedBytes += (uint64)n;

  return n;
}

#endif /* HAVE_SSH2 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageSCP_initAll(void)
{
  Errors error;

  error = ERROR_NONE;

  return error;
}

LOCAL void StorageSCP_doneAll(void)
{
}

LOCAL bool StorageSCP_parseSpecifier(ConstString sshSpecifier,
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

LOCAL bool StorageSCP_equalSpecifiers(const StorageSpecifier *storageSpecifier1,
                                      ConstString            archiveName1,
                                      const StorageSpecifier *storageSpecifier2,
                                      ConstString            archiveName2
                                     )
{
  assert(storageSpecifier1 != NULL);
  assert(storageSpecifier1->type == STORAGE_TYPE_SCP);
  assert(storageSpecifier2 != NULL);
  assert(storageSpecifier2->type == STORAGE_TYPE_SCP);

  if (archiveName1 == NULL) archiveName1 = storageSpecifier1->archiveName;
  if (archiveName2 == NULL) archiveName2 = storageSpecifier2->archiveName;

  return    String_equals(storageSpecifier1->hostName,storageSpecifier2->hostName)
         && String_equals(storageSpecifier1->loginName,storageSpecifier2->loginName)
         && String_equals(archiveName1,archiveName2);
}

LOCAL String StorageSCP_getName(StorageSpecifier *storageSpecifier,
                                ConstString      archiveName
                               )
{
  ConstString storageFileName;
  const char  *plainLoginPassword;

  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_SCP);

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

  String_appendCString(storageSpecifier->storageName,"scp://");
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

LOCAL ConstString StorageSCP_getPrintableName(StorageSpecifier *storageSpecifier,
                                              ConstString      archiveName
                                             )
{
  ConstString storageFileName;

  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_SCP);

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

  String_appendCString(storageSpecifier->storageName,"scp://");
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

LOCAL Errors StorageSCP_init(StorageHandle              *storageHandle,
                             const StorageSpecifier     *storageSpecifier,
                             const JobOptions           *jobOptions,
                             BandWidthList              *maxBandWidthList,
                             ServerConnectionPriorities serverConnectionPriority
                            )
{
  #ifdef HAVE_SSH2
    AutoFreeList autoFreeList;
    Errors       error;
    SSHServer   sshServer;
  #endif /* HAVE_SSH2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(storageSpecifier != NULL);

  UNUSED_VARIABLE(storageSpecifier);

  #ifdef HAVE_SSH2
    // init variables
    AutoFree_init(&autoFreeList);
    storageHandle->scp.sshPublicKeyFileName   = NULL;
    storageHandle->scp.sshPrivateKeyFileName  = NULL;
    initBandWidthLimiter(&storageHandle->scp.bandWidthLimiter,maxBandWidthList);
    AUTOFREE_ADD(&autoFreeList,&storageHandle->scp.bandWidthLimiter,{ doneBandWidthLimiter(&storageHandle->scp.bandWidthLimiter); });

    // get SSH server settings
    storageHandle->scp.serverId = getSSHServerSettings(storageHandle->storageSpecifier.hostName,jobOptions,&sshServer);
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
    if (!allocateServer(storageHandle->scp.serverId,serverConnectionPriority,60*1000L))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,&storageHandle->scp.serverId,{ freeServer(storageHandle->scp.serverId); });

    // check if SSH login is possible
    error = ERROR_UNKNOWN;
    if ((error == ERROR_UNKNOWN) && !Password_isEmpty(sshServer.password))
    {
      error = checkSSHLogin(storageHandle->storageSpecifier.hostName,
                            storageHandle->storageSpecifier.hostPort,
                            storageHandle->storageSpecifier.loginName,
                            sshServer.password,
                            storageHandle->scp.publicKey.data,
                            storageHandle->scp.publicKey.length,
                            storageHandle->scp.privateKey.data,
                            storageHandle->scp.privateKey.length
                           );
      if (error == ERROR_NONE)
      {
        Password_set(storageHandle->storageSpecifier.loginPassword,sshServer.password);
      }
    }
    if (error == ERROR_UNKNOWN)
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
                              storageHandle->scp.publicKey.data,
                              storageHandle->scp.publicKey.length,
                              storageHandle->scp.privateKey.data,
                              storageHandle->scp.privateKey.length
                             );
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
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(maxBandWidthList);
    UNUSED_VARIABLE(serverConnectionPriority);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return ERROR_NONE;
}

LOCAL Errors StorageSCP_done(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  // free SSH server connection
  #ifdef HAVE_SSH2
    freeServer(storageHandle->scp.serverId);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_SSH2 */

  return ERROR_NONE;
}

LOCAL bool StorageSCP_isServerAllocationPending(StorageHandle *storageHandle)
{
  bool serverAllocationPending;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  serverAllocationPending = FALSE;
  #if defined(HAVE_SSH2)
    serverAllocationPending = isServerAllocationPending(storageHandle->scp.serverId);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);

    serverAllocationPending = FALSE;
  #endif /* HAVE_SSH2 */

  return serverAllocationPending;
}

LOCAL Errors StorageSCP_preProcess(StorageHandle *storageHandle,
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
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

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

          if (globalOptions.scp.writePreProcessCommand != NULL)
          {
            // write pre-processing
            if (error == ERROR_NONE)
            {
              printInfo(0,"Write pre-processing...");

              // get script
              script = expandTemplate(String_cString(globalOptions.scp.writePreProcessCommand),
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

              printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
            }
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

LOCAL Errors StorageSCP_postProcess(StorageHandle *storageHandle,
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
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

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

          if (globalOptions.scp.writePostProcessCommand != NULL)
          {
            // write post-process
            if (error == ERROR_NONE)
            {
              printInfo(0,"Write post-processing...");

              // get script
              script = expandTemplate(String_cString(globalOptions.scp.writePostProcessCommand),
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

              printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
            }
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

LOCAL bool StorageSCP_exists(StorageHandle *storageHandle, ConstString archiveName)
{
  assert(storageHandle != NULL);
  assert(!String_isEmpty(archiveName));

HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
  UNUSED_VARIABLE(storageHandle);

  return File_exists(archiveName);
}

LOCAL Errors StorageSCP_create(StorageArchiveHandle *storageArchiveHandle,
                               ConstString   archiveName,
                               uint64        archiveSize
                              )
{
  #ifdef HAVE_SSH2
    Errors error;
  #endif /* HAVE_SSH2 */

  assert(storageArchiveHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(archiveSize);

  #ifdef HAVE_SSH2
    // init variables
    storageArchiveHandle->scp.channel                = NULL;
    storageArchiveHandle->scp.oldSendCallback        = NULL;
    storageArchiveHandle->scp.oldReceiveCallback     = NULL;
    storageArchiveHandle->scp.totalSentBytes         = 0LL;
    storageArchiveHandle->scp.totalReceivedBytes     = 0LL;
    storageArchiveHandle->scp.index                  = 0LL;
    storageArchiveHandle->scp.size                   = 0LL;
    storageArchiveHandle->scp.readAheadBuffer.offset = 0LL;
    storageArchiveHandle->scp.readAheadBuffer.length = 0L;
    DEBUG_ADD_RESOURCE_TRACE(&storageArchiveHandle->scp,sizeof(storageArchiveHandle->scp));

    // connect
    error = Network_connect(&storageArchiveHandle->scp.socketHandle,
                            SOCKET_TYPE_SSH,
                            storageArchiveHandle->storageHandle->storageSpecifier.hostName,
                            storageArchiveHandle->storageHandle->storageSpecifier.hostPort,
                            storageArchiveHandle->storageHandle->storageSpecifier.loginName,
                            storageArchiveHandle->storageHandle->storageSpecifier.loginPassword,
                            storageArchiveHandle->storageHandle->scp.publicKey.data,
                            storageArchiveHandle->storageHandle->scp.publicKey.length,
                            storageArchiveHandle->storageHandle->scp.privateKey.data,
                            storageArchiveHandle->storageHandle->scp.privateKey.length,
                            0
                           );
    if (error != ERROR_NONE)
    {
      DEBUG_REMOVE_RESOURCE_TRACE(&storageArchiveHandle->scp,sizeof(storageArchiveHandle->scp));
      return error;
    }
    libssh2_session_set_timeout(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),READ_TIMEOUT);

    // install send/receive callback to track number of sent/received bytes
    storageArchiveHandle->scp.totalSentBytes     = 0LL;
    storageArchiveHandle->scp.totalReceivedBytes = 0LL;
    (*(libssh2_session_abstract(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle)))) = storageArchiveHandle;
    storageArchiveHandle->scp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,scpSendCallback   );
    storageArchiveHandle->scp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,scpReceiveCallback);

    if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
    {
      // open channel and file for writing
      #ifdef HAVE_SSH2_SCP_SEND64
        storageArchiveHandle->scp.channel = libssh2_scp_send64(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),
                                                            String_cString(archiveName),
// ???
0600,
                                                            (libssh2_uint64_t)archiveSize,
// ???
                                                            0L,
                                                            0L
                                                           );
      #else /* not HAVE_SSH2_SCP_SEND64 */
        storageArchiveHandle->scp.channel = libssh2_scp_send(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),
                                                          String_cString(archiveName),
// ???
0600,
                                                          (size_t)fileSize
                                                         );
      #endif /* HAVE_SSH2_SCP_SEND64 */
      if (storageArchiveHandle->scp.channel == NULL)
      {
        char *sshErrorText;

        libssh2_session_last_error(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),&sshErrorText,NULL,0);
        error = ERRORX_(SSH,
                        libssh2_session_last_errno(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle)),
                        "%s",
                        sshErrorText
                       );
        libssh2_session_callback_set(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,storageArchiveHandle->scp.oldReceiveCallback);
        libssh2_session_callback_set(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,storageArchiveHandle->scp.oldSendCallback);
        Network_disconnect(&storageArchiveHandle->scp.socketHandle);
        DEBUG_REMOVE_RESOURCE_TRACE(&storageArchiveHandle->scp,sizeof(storageArchiveHandle->scp));
        return error;
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

LOCAL Errors StorageSCP_open(StorageArchiveHandle *storageArchiveHandle,
                             ConstString          archiveName
                            )
{
  #ifdef HAVE_SSH2
    Errors      error;
    struct stat fileInfo;
  #endif /* HAVE_SSH2 */

  assert(storageArchiveHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(!String_isEmpty(archiveName));

  #ifdef HAVE_SSH2
    // init variables
    storageArchiveHandle->scp.channel                = NULL;
    storageArchiveHandle->scp.oldSendCallback        = NULL;
    storageArchiveHandle->scp.oldReceiveCallback     = NULL;
    storageArchiveHandle->scp.totalSentBytes         = 0LL;
    storageArchiveHandle->scp.totalReceivedBytes     = 0LL;
    storageArchiveHandle->scp.index                  = 0LL;
    storageArchiveHandle->scp.size                   = 0LL;
    storageArchiveHandle->scp.readAheadBuffer.offset = 0LL;
    storageArchiveHandle->scp.readAheadBuffer.length = 0L;
    DEBUG_ADD_RESOURCE_TRACE(&storageArchiveHandle->scp,sizeof(storageArchiveHandle->scp));

    // allocate read-ahead buffer
    storageArchiveHandle->scp.readAheadBuffer.data = (byte*)malloc(MAX_BUFFER_SIZE);
    if (storageArchiveHandle->scp.readAheadBuffer.data == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    // connect
    error = Network_connect(&storageArchiveHandle->scp.socketHandle,
                            SOCKET_TYPE_SSH,
                            storageArchiveHandle->storageHandle->storageSpecifier.hostName,
                            storageArchiveHandle->storageHandle->storageSpecifier.hostPort,
                            storageArchiveHandle->storageHandle->storageSpecifier.loginName,
                            storageArchiveHandle->storageHandle->storageSpecifier.loginPassword,
                            storageArchiveHandle->storageHandle->scp.publicKey.data,
                            storageArchiveHandle->storageHandle->scp.publicKey.length,
                            storageArchiveHandle->storageHandle->scp.privateKey.data,
                            storageArchiveHandle->storageHandle->scp.privateKey.length,
                            0
                           );
    if (error != ERROR_NONE)
    {
      free(storageArchiveHandle->scp.readAheadBuffer.data);
      DEBUG_REMOVE_RESOURCE_TRACE(&storageArchiveHandle->scp,sizeof(storageArchiveHandle->scp));
      return error;
    }
    libssh2_session_set_timeout(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),READ_TIMEOUT);

    // install send/receive callback to track number of sent/received bytes
    storageArchiveHandle->scp.totalSentBytes     = 0LL;
    storageArchiveHandle->scp.totalReceivedBytes = 0LL;
    (*(libssh2_session_abstract(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle)))) = storageArchiveHandle;
    storageArchiveHandle->scp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,scpSendCallback   );
    storageArchiveHandle->scp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,scpReceiveCallback);

    // open channel and file for reading
    storageArchiveHandle->scp.channel = libssh2_scp_recv(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),
                                                      String_cString(archiveName),
                                                      &fileInfo
                                                     );
    if (storageArchiveHandle->scp.channel == NULL)
    {
      char *sshErrorText;

      libssh2_session_last_error(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),&sshErrorText,NULL,0);
      error = ERRORX_(SSH,
                      libssh2_session_last_errno(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle)),
                      "%s",
                      sshErrorText
                     );
      libssh2_session_callback_set(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,storageArchiveHandle->scp.oldReceiveCallback);
      libssh2_session_callback_set(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,storageArchiveHandle->scp.oldSendCallback);
      Network_disconnect(&storageArchiveHandle->scp.socketHandle);
      free(storageArchiveHandle->scp.readAheadBuffer.data);
      DEBUG_REMOVE_RESOURCE_TRACE(&storageArchiveHandle->scp,sizeof(storageArchiveHandle->scp));
      return error;
    }
    storageArchiveHandle->scp.size = (uint64)fileInfo.st_size;

    return ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageArchiveHandle);
    UNUSED_VARIABLE(archiveName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL void StorageSCP_close(StorageArchiveHandle *storageArchiveHandle)
{
  #ifdef HAVE_SSH2
    int result;
  #endif /* HAVE_SSH2 */

  #ifdef HAVE_SSH2
  assert(storageArchiveHandle != NULL);
    DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->scp);
  #endif /* HAVE_SSH2 */
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  #ifdef HAVE_SSH2
    libssh2_session_callback_set(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,storageArchiveHandle->scp.oldReceiveCallback);
    libssh2_session_callback_set(Network_getSSHSession(&storageArchiveHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,storageArchiveHandle->scp.oldSendCallback);

    switch (storageArchiveHandle->mode)
    {
      case STORAGE_MODE_READ:
        (void)libssh2_channel_close(storageArchiveHandle->scp.channel);
        (void)libssh2_channel_wait_closed(storageArchiveHandle->scp.channel);
        (void)libssh2_channel_free(storageArchiveHandle->scp.channel);
        free(storageArchiveHandle->scp.readAheadBuffer.data);
        break;
      case STORAGE_MODE_WRITE:
        if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
        {
          result = 0;

          if (result == 0)
          {
            do
            {
              result = libssh2_channel_send_eof(storageArchiveHandle->scp.channel);
              if (result == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*MISC_US_PER_MS);
            }
            while (result == LIBSSH2_ERROR_EAGAIN);
          }
          if (result == 0)
          {
            do
            {
              result = libssh2_channel_wait_eof(storageArchiveHandle->scp.channel);
              if (result == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*MISC_US_PER_MS);
            }
            while (result == LIBSSH2_ERROR_EAGAIN);
          }
          if (result == 0)
          {
            do
            {
              result = libssh2_channel_close(storageArchiveHandle->scp.channel);
              if (result == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*MISC_US_PER_MS);
            }
            while (result == LIBSSH2_ERROR_EAGAIN);
          }
          if (result == 0)
          {
            do
            {
              result = libssh2_channel_wait_closed(storageArchiveHandle->scp.channel);
              if (result == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*MISC_US_PER_MS);
            }
            while (result == LIBSSH2_ERROR_EAGAIN);
          }
          (void)libssh2_channel_free(storageArchiveHandle->scp.channel);
        }
        break;
      case STORAGE_MODE_UNKNOWN:
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
    Network_disconnect(&storageArchiveHandle->scp.socketHandle);
    DEBUG_REMOVE_RESOURCE_TRACE(&storageArchiveHandle->scp,sizeof(storageArchiveHandle->scp));
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageArchiveHandle);
  #endif /* HAVE_SSH2 */
}

LOCAL bool StorageSCP_eof(StorageArchiveHandle *storageArchiveHandle)
{
  assert(storageArchiveHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->scp);
  #endif /* HAVE_SSH2 */
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->mode == STORAGE_MODE_READ);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  #ifdef HAVE_SSH2
    if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
    {
      return storageArchiveHandle->scp.index >= storageArchiveHandle->scp.size;
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

LOCAL Errors StorageSCP_read(StorageArchiveHandle *storageArchiveHandle,
                             void          *buffer,
                             ulong         size,
                             ulong         *bytesRead
                            )
{
  #ifdef HAVE_SSH2
    Errors  error;
    ulong   index;
    ulong   bytesAvail;
    ulong   length;
    uint64  startTimestamp,endTimestamp;
    uint64  startTotalReceivedBytes,endTotalReceivedBytes;
    ssize_t n;
  #endif /* HAVE_SSH2 */

  assert(storageArchiveHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->scp);
  #endif /* HAVE_SSH2 */
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->mode == STORAGE_MODE_READ);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(buffer != NULL);

//fprintf(stderr,"%s,%d: size=%lu\n",__FILE__,__LINE__,size);
  if (bytesRead != NULL) (*bytesRead) = 0L;
  #ifdef HAVE_SSH2
    error = ERROR_NONE;
    if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
    {
      assert(storageArchiveHandle->scp.channel != NULL);
      assert(storageArchiveHandle->scp.readAheadBuffer.data != NULL);

      while (   (size > 0L)
             && (error == ERROR_NONE)
            )
      {
        // copy as much data as available from read-ahead buffer
        if (   (storageArchiveHandle->scp.index >= storageArchiveHandle->scp.readAheadBuffer.offset)
            && (storageArchiveHandle->scp.index < (storageArchiveHandle->scp.readAheadBuffer.offset+storageArchiveHandle->scp.readAheadBuffer.length))
           )
        {
          // copy data from read-ahead buffer
          index      = (ulong)(storageArchiveHandle->scp.index-storageArchiveHandle->scp.readAheadBuffer.offset);
          bytesAvail = MIN(size,storageArchiveHandle->scp.readAheadBuffer.length-index);
          memcpy(buffer,storageArchiveHandle->scp.readAheadBuffer.data+index,bytesAvail);

          // adjust buffer, size, bytes read, index
          buffer = (byte*)buffer+bytesAvail;
          size -= bytesAvail;
          if (bytesRead != NULL) (*bytesRead) += bytesAvail;
          storageArchiveHandle->scp.index += (uint64)bytesAvail;
        }

        // read rest of data
        if (size > 0)
        {
          assert(storageArchiveHandle->scp.index >= (storageArchiveHandle->scp.readAheadBuffer.offset+storageArchiveHandle->scp.readAheadBuffer.length));

          // get max. number of bytes to receive in one step
          if (storageArchiveHandle->storageHandle->scp.bandWidthLimiter.maxBandWidthList != NULL)
          {
            length = MIN(storageArchiveHandle->storageHandle->scp.bandWidthLimiter.blockSize,size);
          }
          else
          {
            length = size;
          }
          assert(length > 0L);

          // get start time, start received bytes
          startTimestamp          = Misc_getTimestamp();
          startTotalReceivedBytes = storageArchiveHandle->scp.totalReceivedBytes;

          if (length < MAX_BUFFER_SIZE)
          {
            // read into read-ahead buffer
            do
            {
              n = libssh2_channel_read(storageArchiveHandle->scp.channel,
                                       (char*)storageArchiveHandle->scp.readAheadBuffer.data,
                                       MIN((size_t)(storageArchiveHandle->scp.size-storageArchiveHandle->scp.index),MAX_BUFFER_SIZE)
                                     );
              if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*MISC_US_PER_MS);
            }
            while (n == LIBSSH2_ERROR_EAGAIN);
            if (n < 0)
            {
              error = ERROR_(IO_ERROR,errno);
              break;
            }
            storageArchiveHandle->scp.readAheadBuffer.offset = storageArchiveHandle->scp.index;
            storageArchiveHandle->scp.readAheadBuffer.length = (ulong)n;
//fprintf(stderr,"%s,%d: n=%ld storageArchiveHandle->scp.bufferOffset=%llu storageArchiveHandle->scp.bufferLength=%lu\n",__FILE__,__LINE__,n,
//storageArchiveHandle->scp.readAheadBuffer.offset,storageArchiveHandle->scp.readAheadBuffer.length);

            // copy data from read-ahead buffer
            bytesAvail = MIN(length,storageArchiveHandle->scp.readAheadBuffer.length);
            memcpy(buffer,storageArchiveHandle->scp.readAheadBuffer.data,bytesAvail);

            // adjust buffer, size, bytes read, index
            buffer = (byte*)buffer+bytesAvail;
            size -= bytesAvail;
            if (bytesRead != NULL) (*bytesRead) += bytesAvail;
            storageArchiveHandle->scp.index += (uint64)bytesAvail;
          }
          else
          {
            // read direct
            do
            {
              n = libssh2_channel_read(storageArchiveHandle->scp.channel,
                                               buffer,
                                               length
                                              );
              if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*MISC_US_PER_MS);
            }
            while (n == LIBSSH2_ERROR_EAGAIN);
            if (n < 0)
            {
              error = ERROR_(IO_ERROR,errno);
              break;
            }

            // adjust buffer, size, bytes read, index
            buffer = (byte*)buffer+(ulong)n;
            size -= (ulong)n;
            if (bytesRead != NULL) (*bytesRead) += (ulong)n;
            storageArchiveHandle->scp.index += (uint64)n;
          }

          // get end time, end received bytes
          endTimestamp          = Misc_getTimestamp();
          endTotalReceivedBytes = storageArchiveHandle->scp.totalReceivedBytes;
          assert(endTotalReceivedBytes >= startTotalReceivedBytes);

          /* limit used band width if requested (note: when the system time is
             changing endTimestamp may become smaller than startTimestamp;
             thus do not check this with an assert())
          */
          if (endTimestamp >= startTimestamp)
          {
            limitBandWidth(&storageArchiveHandle->storageHandle->scp.bandWidthLimiter,
                           endTotalReceivedBytes-startTotalReceivedBytes,
                           endTimestamp-startTimestamp
                          );
          }
        }
      }
    }

    return error;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageArchiveHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(size);
    UNUSED_VARIABLE(bytesRead);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL Errors StorageSCP_write(StorageArchiveHandle *storageArchiveHandle,
                              const void    *buffer,
                              ulong         size
                             )
{
  #ifdef HAVE_SSH2
    Errors  error;
    ulong   writtenBytes;
    ulong   length;
    uint64  startTimestamp,endTimestamp;
    uint64  startTotalSentBytes,endTotalSentBytes;
    ssize_t n;
  #endif /* HAVE_SSH2 */

  assert(storageArchiveHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->scp);
  #endif /* HAVE_SSH2 */
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->mode == STORAGE_MODE_WRITE);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(buffer != NULL);

  #ifdef HAVE_SSH2
    error = ERROR_NONE;
    if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
    {
      assert(storageArchiveHandle->scp.channel != NULL);

      writtenBytes = 0L;
      while (writtenBytes < size)
      {
        // get max. number of bytes to send in one step
        if (storageArchiveHandle->storageHandle->scp.bandWidthLimiter.maxBandWidthList != NULL)
        {
          length = MIN(storageArchiveHandle->storageHandle->scp.bandWidthLimiter.blockSize,size-writtenBytes);
        }
        else
        {
          length = size-writtenBytes;
        }
        assert(length > 0L);

        // workaround for libssh2-problem: it seems sending of blocks >=4k cause problems, e. g. corrupt ssh MAC?
        length = MIN(length,4*1024);

        // get start time, start received bytes
        startTimestamp      = Misc_getTimestamp();
        startTotalSentBytes = storageArchiveHandle->scp.totalSentBytes;

        // send data
        do
        {
          n = libssh2_channel_write(storageArchiveHandle->scp.channel,
                                    buffer,
                                    length
                                   );
          if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*MISC_US_PER_MS);
        }
        while (n == LIBSSH2_ERROR_EAGAIN);

        // get end time, end received bytes
        endTimestamp      = Misc_getTimestamp();
        endTotalSentBytes = storageArchiveHandle->scp.totalSentBytes;
        assert(endTotalSentBytes >= startTotalSentBytes);

// ??? is it possible in blocking-mode that write() return 0 and this is not an error?
#if 1
        if      (n == 0)
        {
          // should not happen in blocking-mode: bug? libssh2 API changed somewhere between 0.18 and 1.2.4? => wait for data
          if (!waitSSHSessionSocket(&storageArchiveHandle->scp.socketHandle))
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
          limitBandWidth(&storageArchiveHandle->storageHandle->scp.bandWidthLimiter,
                         endTotalSentBytes-startTotalSentBytes,
                         endTimestamp-startTimestamp
                        );
        }
      }
    }
    assert(error != ERROR_UNKNOWN);

    return error;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageArchiveHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(size);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL uint64 StorageSCP_getSize(StorageArchiveHandle *storageArchiveHandle)
{
  uint64 size;

  assert(storageArchiveHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->scp);
  #endif /* HAVE_SSH2 */
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  size = 0LL;
  #ifdef HAVE_SSH2
    if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
    {
      size = storageArchiveHandle->scp.size;
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageArchiveHandle);
  #endif /* HAVE_SSH2 */

  return size;
}

LOCAL Errors StorageSCP_tell(StorageArchiveHandle *storageArchiveHandle,
                             uint64        *offset
                            )
{
  Errors error;

  assert(storageArchiveHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->scp);
  #endif /* HAVE_SSH2 */
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
    {
      (*offset) = storageArchiveHandle->scp.index;
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

LOCAL Errors StorageSCP_seek(StorageArchiveHandle *storageArchiveHandle,
                             uint64        offset
                            )
{
  #ifdef HAVE_SSH2
    Errors error;
  #endif /* HAVE_SSH2 */

  assert(storageArchiveHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->scp);
  #endif /* HAVE_SSH2 */
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  #ifdef HAVE_SSH2
    /* scp protocol does not support a seek-function. Thus try to
       read and discard data to position the read index to the
       requested offset.
       Note: this is slow!
    */

    error = ERROR_NONE;
    if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
    {
      assert(storageArchiveHandle->scp.channel != NULL);
      assert(storageArchiveHandle->scp.readAheadBuffer.data != NULL);

      if      (offset > storageArchiveHandle->scp.index)
      {
        uint64  skip;
        uint64  i;
        uint64  n;
        ssize_t readBytes;

        skip = offset-storageArchiveHandle->scp.index;
        while (skip > 0LL)
        {
          // skip data in read-ahead buffer
          if (   (storageArchiveHandle->scp.index >= storageArchiveHandle->scp.readAheadBuffer.offset)
              && (storageArchiveHandle->scp.index < (storageArchiveHandle->scp.readAheadBuffer.offset+storageArchiveHandle->scp.readAheadBuffer.length))
             )
          {
            i = storageArchiveHandle->scp.index-storageArchiveHandle->scp.readAheadBuffer.offset;
            n = MIN(skip,storageArchiveHandle->scp.readAheadBuffer.length-i);
            skip -= n;
            storageArchiveHandle->scp.index += (uint64)n;
          }

          if (skip > 0LL)
          {
            assert(storageArchiveHandle->scp.index >= (storageArchiveHandle->scp.readAheadBuffer.offset+storageArchiveHandle->scp.readAheadBuffer.length));

            // wait for data
            if (!waitSSHSessionSocket(&storageArchiveHandle->scp.socketHandle))
            {
              error = ERROR_(IO_ERROR,errno);
              break;
            }

            // read data
            readBytes = libssh2_channel_read(storageArchiveHandle->scp.channel,
                                             (char*)storageArchiveHandle->scp.readAheadBuffer.data,
                                             MIN((size_t)skip,MAX_BUFFER_SIZE)
                                            );
            if (readBytes < 0)
            {
              error = ERROR_(IO_ERROR,errno);
              break;
            }
            storageArchiveHandle->scp.readAheadBuffer.offset = storageArchiveHandle->scp.index;
            storageArchiveHandle->scp.readAheadBuffer.length = (uint64)readBytes;
          }
        }
      }
      else if (offset < storageArchiveHandle->scp.index)
      {
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      }
      assert(error != ERROR_UNKNOWN);

      return error;
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageArchiveHandle);
    UNUSED_VARIABLE(offset);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL Errors StorageSCP_delete(StorageHandle *storageHandle,
                               ConstString   archiveName
                              )
{
//  ConstString deleteFileName;
  Errors      error;

  assert(storageHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  #endif /* HAVE_SSH2 */
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(!String_isEmpty(archiveName));

//  deleteFileName = (storageFileName != NULL) ? storageFileName : storageHandle->storageSpecifier.archiveName;
UNUSED_VARIABLE(storageHandle);
UNUSED_VARIABLE(archiveName);

  error = ERROR_UNKNOWN;
  #ifdef HAVE_SSH2
#if 0
whould this be a possible implementation?
    {
      String command;

      assert(storageHandle->scp.channel != NULL);

      // there is no unlink command for scp: execute either 'rm' or 'del' on remote server
      command = String_new();
      String_format(String_clear(command),"rm %'S",deleteFileName);
      error = (libssh2_channel_exec(storageHandle->scp.channel,
                                    String_cString(command)
                                   ) != 0
              ) ? ERROR_NONE : ERROR_DELETE_FILE;
      if (error != ERROR_NONE)
      {
        String_format(String_clear(command),"del %'S",deleteFileName);
        error = (libssh2_channel_exec(storageHandle->scp.channel,
                                      String_cString(command)
                                     ) != 0
                ) ? ERROR_NONE : ERROR_DELETE_FILE;
      }
      String_delete(command);
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(archiveName);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
#endif /* 0 */

  error = ERROR_FUNCTION_NOT_SUPPORTED;
  assert(error != ERROR_UNKNOWN);

  return error;
}

#if 0
still not complete
LOCAL Errors StorageSCP_getFileInfo(StorageHandle *storageHandle,
                                    ConstString   fileName,
                                    FileInfo      *fileInfo
                                   )
{
  String infoFileName;
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(fileInfo != NULL);

  infoFileName = (fileName != NULL) ? fileName : storageHandle->storageSpecifier.archiveName;
  memset(fileInfo,0,sizeof(fileInfo));

  error = ERROR_UNKNOWN;
  switch (storageHandle->storageSpecifier.type)
      error = ERROR_FUNCTION_NOT_SUPPORTED;
  assert(error != ERROR_UNKNOWN);

  return error;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageSCP_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                          const StorageSpecifier     *storageSpecifier,
                                          const JobOptions           *jobOptions,
                                          ServerConnectionPriorities serverConnectionPriority,
                                          ConstString                archiveName
                                         )
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_SCP);
  assert(jobOptions != NULL);

  UNUSED_VARIABLE(storageDirectoryListHandle);
  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(jobOptions);
  UNUSED_VARIABLE(serverConnectionPriority);
  UNUSED_VARIABLE(archiveName);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

LOCAL void StorageSCP_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  UNUSED_VARIABLE(storageDirectoryListHandle);

  HALT_INTERNAL_ERROR("scp does not support directory operations");
}

LOCAL bool StorageSCP_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  UNUSED_VARIABLE(storageDirectoryListHandle);

  HALT_INTERNAL_ERROR("scp does not support directory operations");

  return TRUE;
}

LOCAL Errors StorageSCP_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                          String                     fileName,
                                          FileInfo                   *fileInfo
                                         )
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  UNUSED_VARIABLE(storageDirectoryListHandle);
  UNUSED_VARIABLE(fileName);
  UNUSED_VARIABLE(fileInfo);

  HALT_INTERNAL_ERROR("scp does not support directory operations");

  return ERROR_STILL_NOT_IMPLEMENTED;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
