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
  StorageHandle *storageHandle;
  ssize_t       n;

  assert(abstract != NULL);

  storageHandle = *((StorageHandle**)abstract);
  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(storageHandle->scp.oldSendCallback != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  n = storageHandle->scp.oldSendCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageHandle->scp.totalSentBytes += (uint64)n;

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
  StorageHandle *storageHandle;
  ssize_t       n;

  assert(abstract != NULL);

  storageHandle = *((StorageHandle**)abstract);
  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(storageHandle->scp.oldReceiveCallback != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  n = storageHandle->scp.oldReceiveCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageHandle->scp.totalReceivedBytes += (uint64)n;

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
  if      (String_matchCString(sshSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^:]+?):(\\d*)/{0,1}$",NULL,NULL,loginName,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <login name>@<host name>:<host port>
    if (loginName != NULL) String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (hostPort != NULL)
    {
      if (!String_isEmpty(s)) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);
    }

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^/]+)/{0,1}$",NULL,NULL,loginName,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>@<host name>
    if (loginName != NULL) String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^([^@:/]*?):(\\d*)/{0,1}$",NULL,NULL,hostName,s,NULL))
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

LOCAL bool StorageSCP_equalNames(const StorageSpecifier *storageSpecifier1,
                                 const StorageSpecifier *storageSpecifier2
                                )
{
  assert(storageSpecifier1 != NULL);
  assert(storageSpecifier1->type == STORAGE_TYPE_SCP);
  assert(storageSpecifier2 != NULL);
  assert(storageSpecifier2->type == STORAGE_TYPE_SCP);

  return    String_equals(storageSpecifier1->hostName,storageSpecifier2->hostName)
         && String_equals(storageSpecifier1->loginName,storageSpecifier2->loginName)
         && String_equals(storageSpecifier1->archiveName,storageSpecifier2->archiveName);
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
    storageHandle->scp.channel                = NULL;
    storageHandle->scp.oldSendCallback        = NULL;
    storageHandle->scp.oldReceiveCallback     = NULL;
    storageHandle->scp.totalSentBytes         = 0LL;
    storageHandle->scp.totalReceivedBytes     = 0LL;
    storageHandle->scp.readAheadBuffer.offset = 0LL;
    storageHandle->scp.readAheadBuffer.length = 0L;
    initBandWidthLimiter(&storageHandle->scp.bandWidthLimiter,maxBandWidthList);

    // allocate read-ahead buffer
    storageHandle->scp.readAheadBuffer.data = (byte*)malloc(MAX_BUFFER_SIZE);
    if (storageHandle->scp.readAheadBuffer.data == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    AUTOFREE_ADD(&autoFreeList,storageHandle->scp.readAheadBuffer.data,{ free(storageHandle->scp.readAheadBuffer.data); });

    // get SSH server settings
    storageHandle->scp.server = getSSHServerSettings(storageHandle->storageSpecifier.hostName,jobOptions,&sshServer);
    if (String_isEmpty(storageHandle->storageSpecifier.loginName)) String_set(storageHandle->storageSpecifier.loginName,sshServer.loginName);
    if (String_isEmpty(storageHandle->storageSpecifier.loginName)) String_setCString(storageHandle->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageHandle->storageSpecifier.loginName)) String_setCString(storageHandle->storageSpecifier.loginName,getenv("USER"));
    if (storageHandle->storageSpecifier.hostPort == 0) storageHandle->storageSpecifier.hostPort = sshServer.port;
    storageHandle->scp.sshPublicKeyFileName  = sshServer.publicKeyFileName;
    storageHandle->scp.sshPrivateKeyFileName = sshServer.privateKeyFileName;
    if (String_isEmpty(storageHandle->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }

    // allocate SSH server
    if (!allocateServer(storageHandle->scp.server,serverConnectionPriority,60*1000L))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,storageHandle->scp.server,{ freeServer(storageHandle->scp.server); });

    // check if SSH login is possible
    error = ERROR_UNKNOWN;
    if ((error == ERROR_UNKNOWN) && !Password_isEmpty(sshServer.password))
    {
      error = checkSSHLogin(storageHandle->storageSpecifier.hostName,
                            storageHandle->storageSpecifier.hostPort,
                            storageHandle->storageSpecifier.loginName,
                            sshServer.password,
                            storageHandle->scp.sshPublicKeyFileName,
                            storageHandle->scp.sshPrivateKeyFileName
                           );
      if (error == ERROR_NONE)
      {
        Password_set(storageHandle->storageSpecifier.loginPassword,sshServer.password);
      }
    }
    if (error == ERROR_UNKNOWN)
    {
      // initialize default password
      if (   initDefaultSSHPassword(storageHandle->storageSpecifier.hostName,storageHandle->storageSpecifier.loginName,jobOptions)
          && !Password_isEmpty(defaultSSHPassword)
         )
      {
        error = checkSSHLogin(storageHandle->storageSpecifier.hostName,
                              storageHandle->storageSpecifier.hostPort,
                              storageHandle->storageSpecifier.loginName,
                              defaultSSHPassword,
                              storageHandle->scp.sshPublicKeyFileName,
                              storageHandle->scp.sshPrivateKeyFileName
                             );
        if (error == ERROR_NONE)
        {
          Password_set(storageHandle->storageSpecifier.loginPassword,defaultSSHPassword);
        }
      }
      else
      {
        error = (!Password_isEmpty(sshServer.password) || !Password_isEmpty(defaultSSHPassword))
                  ? ERRORX_(INVALID_SSH_PASSWORD,0,String_cString(storageHandle->storageSpecifier.hostName))
                  : ERRORX_(NO_SSH_PASSWORD,0,String_cString(storageHandle->storageSpecifier.hostName));
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
fprintf(stderr,"%s, %d: ffffffffffffffff\n",__FILE__,__LINE__);
    freeServer(storageHandle->scp.server);
    free(storageHandle->scp.readAheadBuffer.data);
  #else /* not HAVE_SSH2 */
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
        serverAllocationPending = isServerAllocationPending(storageHandle->scp.server);
      #else /* not HAVE_SSH2 */
        serverAllocationPending = FALSE;
      #endif /* HAVE_SSH2 */

  return serverAllocationPending;
}

LOCAL Errors StorageSCP_preProcess(StorageHandle *storageHandle,
                                   bool          initialFlag
                                  )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    {
      TextMacro textMacros[1];

      if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
      {
        if (!initialFlag)
        {
          // init macros
          TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageHandle->volumeNumber              );

          if (globalOptions.scp.writePreProcessCommand != NULL)
          {
            // write pre-processing
            if (error == ERROR_NONE)
            {
              printInfo(0,"Write pre-processing...");
              error = Misc_executeCommand(String_cString(globalOptions.scp.writePreProcessCommand),
                                          textMacros,
                                          SIZE_OF_ARRAY(textMacros),
                                          CALLBACK(executeIOOutput,NULL),
                                          CALLBACK(executeIOOutput,NULL)
                                         );
              printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
            }
          }
        }
      }
    }
  #else /* not HAVE_SSH2 */
    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return error;
}

LOCAL Errors StorageSCP_postProcess(StorageHandle *storageHandle,
                                    bool          finalFlag
                                   )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    {
      TextMacro textMacros[1];

      if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
      {
        if (!finalFlag)
        {
          // init macros
          TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageHandle->volumeNumber);

          if (globalOptions.scp.writePostProcessCommand != NULL)
          {
            // write post-process
            if (error == ERROR_NONE)
            {
              printInfo(0,"Write post-processing...");
              error = Misc_executeCommand(String_cString(globalOptions.scp.writePostProcessCommand),
                                          textMacros,
                                          SIZE_OF_ARRAY(textMacros),
                                          CALLBACK(executeIOOutput,NULL),
                                          CALLBACK(executeIOOutput,NULL)
                                         );
              printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
            }
          }
        }
      }
    }
  #else /* not HAVE_SSH2 */
    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return error;
}

LOCAL Errors StorageSCP_create(StorageHandle *storageHandle,
                               ConstString   archiveName,
                               uint64        archiveSize
                              )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(!String_isEmpty(storageHandle->storageSpecifier.archiveName));
  assert(archiveName != NULL);

  UNUSED_VARIABLE(archiveSize);

  #ifdef HAVE_SSH2
    // connect
    error = Network_connect(&storageHandle->scp.socketHandle,
                            SOCKET_TYPE_SSH,
                            storageHandle->storageSpecifier.hostName,
                            storageHandle->storageSpecifier.hostPort,
                            storageHandle->storageSpecifier.loginName,
                            storageHandle->storageSpecifier.loginPassword,
                            storageHandle->scp.sshPublicKeyFileName,
                            storageHandle->scp.sshPrivateKeyFileName,
                            0
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }
    libssh2_session_set_timeout(Network_getSSHSession(&storageHandle->scp.socketHandle),READ_TIMEOUT);

    // install send/receive callback to track number of sent/received bytes
    storageHandle->scp.totalSentBytes     = 0LL;
    storageHandle->scp.totalReceivedBytes = 0LL;
    (*(libssh2_session_abstract(Network_getSSHSession(&storageHandle->scp.socketHandle)))) = storageHandle;
    storageHandle->scp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,scpSendCallback   );
    storageHandle->scp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,scpReceiveCallback);

    if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
    {
      // open channel and file for writing
      #ifdef HAVE_SSH2_SCP_SEND64
        storageHandle->scp.channel = libssh2_scp_send64(Network_getSSHSession(&storageHandle->scp.socketHandle),
                                                            String_cString(archiveName),
// ???
0600,
                                                            (libssh2_uint64_t)archiveSize,
// ???
                                                            0L,
                                                            0L
                                                           );
      #else /* not HAVE_SSH2_SCP_SEND64 */
        storageHandle->scp.channel = libssh2_scp_send(Network_getSSHSession(&storageHandle->scp.socketHandle),
                                                          String_cString(archiveName),
// ???
0600,
                                                          (size_t)fileSize
                                                         );
      #endif /* HAVE_SSH2_SCP_SEND64 */
      if (storageHandle->scp.channel == NULL)
      {
        char *sshErrorText;

        libssh2_session_last_error(Network_getSSHSession(&storageHandle->scp.socketHandle),&sshErrorText,NULL,0);
        error = ERRORX_(SSH,
                        libssh2_session_last_errno(Network_getSSHSession(&storageHandle->scp.socketHandle)),
                        sshErrorText
                       );
        Network_disconnect(&storageHandle->scp.socketHandle);
        return error;
      }
    }

    DEBUG_ADD_RESOURCE_TRACE("storage create scp",&storageHandle->scp);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(archiveSize);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return ERROR_NONE;
}

LOCAL Errors StorageSCP_open(StorageHandle *storageHandle, ConstString archiveName)
{
  Errors error;
  #ifdef HAVE_SSH2
    struct stat fileInfo;
  #endif /* HAVE_SSH2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  #ifdef HAVE_SSH2
    // init variables
    storageHandle->scp.index                  = 0LL;
    storageHandle->scp.readAheadBuffer.offset = 0LL;
    storageHandle->scp.readAheadBuffer.length = 0L;

    // connect
    error = Network_connect(&storageHandle->scp.socketHandle,
                            SOCKET_TYPE_SSH,
                            storageHandle->storageSpecifier.hostName,
                            storageHandle->storageSpecifier.hostPort,
                            storageHandle->storageSpecifier.loginName,
                            storageHandle->storageSpecifier.loginPassword,
                            storageHandle->scp.sshPublicKeyFileName,
                            storageHandle->scp.sshPrivateKeyFileName,
                            0
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }
    libssh2_session_set_timeout(Network_getSSHSession(&storageHandle->scp.socketHandle),READ_TIMEOUT);

    // install send/receive callback to track number of sent/received bytes
    storageHandle->scp.totalSentBytes     = 0LL;
    storageHandle->scp.totalReceivedBytes = 0LL;
    (*(libssh2_session_abstract(Network_getSSHSession(&storageHandle->scp.socketHandle)))) = storageHandle;
    storageHandle->scp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,scpSendCallback   );
    storageHandle->scp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,scpReceiveCallback);

    // open channel and file for reading
    storageHandle->scp.channel = libssh2_scp_recv(Network_getSSHSession(&storageHandle->scp.socketHandle),
                                                      String_cString(archiveName),
                                                      &fileInfo
                                                     );
    if (storageHandle->scp.channel == NULL)
    {
      char *sshErrorText;

      libssh2_session_last_error(Network_getSSHSession(&storageHandle->scp.socketHandle),&sshErrorText,NULL,0);
      error = ERRORX_(SSH,
                      libssh2_session_last_errno(Network_getSSHSession(&storageHandle->scp.socketHandle)),
                      sshErrorText
                     );
      Network_disconnect(&storageHandle->scp.socketHandle);
      return error;
    }
    storageHandle->scp.size = (uint64)fileInfo.st_size;

    DEBUG_ADD_RESOURCE_TRACE("storage open scp",&storageHandle->scp);

    return ERROR_NONE;
  #else /* not HAVE_SSH2 */
    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL void StorageSCP_close(StorageHandle *storageHandle)
{
  #ifdef HAVE_SSH2
    int result;
  #endif /* HAVE_SSH2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  #ifdef HAVE_SSH2
    DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->scp);

    switch (storageHandle->mode)
    {
      case STORAGE_MODE_UNKNOWN:
        break;
      case STORAGE_MODE_WRITE:
        if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
        {
          result = 0;

          if (result == 0)
          {
            do
            {
              result = libssh2_channel_send_eof(storageHandle->scp.channel);
              if (result == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*MISC_US_PER_MS);
            }
            while (result == LIBSSH2_ERROR_EAGAIN);
          }
          if (result == 0)
          {
            do
            {
              result = libssh2_channel_wait_eof(storageHandle->scp.channel);
              if (result == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*MISC_US_PER_MS);
            }
            while (result == LIBSSH2_ERROR_EAGAIN);
          }
          if (result == 0)
          {
            do
            {
              result = libssh2_channel_close(storageHandle->scp.channel);
              if (result == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*MISC_US_PER_MS);
            }
            while (result == LIBSSH2_ERROR_EAGAIN);
          }
          if (result == 0)
          {
            do
            {
              result = libssh2_channel_wait_closed(storageHandle->scp.channel);
              if (result == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*MISC_US_PER_MS);
            }
            while (result == LIBSSH2_ERROR_EAGAIN);
          }
          (void)libssh2_channel_free(storageHandle->scp.channel);
        }
        break;
      case STORAGE_MODE_READ:
        (void)libssh2_channel_close(storageHandle->scp.channel);
        (void)libssh2_channel_wait_closed(storageHandle->scp.channel);
        (void)libssh2_channel_free(storageHandle->scp.channel);
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
    Network_disconnect(&storageHandle->scp.socketHandle);
  #else /* not HAVE_SSH2 */
  #endif /* HAVE_SSH2 */
}

LOCAL bool StorageSCP_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(storageHandle->mode == STORAGE_MODE_READ);

  #ifdef HAVE_SSH2
    if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
    {
      return storageHandle->scp.index >= storageHandle->scp.size;
    }
    else
    {
      return TRUE;
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    return TRUE;
  #endif /* HAVE_SSH2 */
}

LOCAL Errors StorageSCP_read(StorageHandle *storageHandle,
                             void          *buffer,
                             ulong         size,
                             ulong         *bytesRead
                            )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(buffer != NULL);

//fprintf(stderr,"%s,%d: size=%lu\n",__FILE__,__LINE__,size);
  if (bytesRead != NULL) (*bytesRead) = 0L;
  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    {
      ulong   index;
      ulong   bytesAvail;
      ulong   length;
      uint64  startTimestamp,endTimestamp;
      uint64  startTotalReceivedBytes,endTotalReceivedBytes;
      ssize_t n;

      if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
      {
        assert(storageHandle->scp.channel != NULL);
        assert(storageHandle->scp.readAheadBuffer.data != NULL);

        while (   (size > 0L)
               && (error == ERROR_NONE)
              )
        {
          // copy as much data as available from read-ahead buffer
          if (   (storageHandle->scp.index >= storageHandle->scp.readAheadBuffer.offset)
              && (storageHandle->scp.index < (storageHandle->scp.readAheadBuffer.offset+storageHandle->scp.readAheadBuffer.length))
             )
          {
            // copy data from read-ahead buffer
            index      = (ulong)(storageHandle->scp.index-storageHandle->scp.readAheadBuffer.offset);
            bytesAvail = MIN(size,storageHandle->scp.readAheadBuffer.length-index);
            memcpy(buffer,storageHandle->scp.readAheadBuffer.data+index,bytesAvail);

            // adjust buffer, size, bytes read, index
            buffer = (byte*)buffer+bytesAvail;
            size -= bytesAvail;
            if (bytesRead != NULL) (*bytesRead) += bytesAvail;
            storageHandle->scp.index += (uint64)bytesAvail;
          }

          // read rest of data
          if (size > 0)
          {
            assert(storageHandle->scp.index >= (storageHandle->scp.readAheadBuffer.offset+storageHandle->scp.readAheadBuffer.length));

            // get max. number of bytes to receive in one step
            if (storageHandle->scp.bandWidthLimiter.maxBandWidthList != NULL)
            {
              length = MIN(storageHandle->scp.bandWidthLimiter.blockSize,size);
            }
            else
            {
              length = size;
            }
            assert(length > 0L);

            // get start time, start received bytes
            startTimestamp          = Misc_getTimestamp();
            startTotalReceivedBytes = storageHandle->scp.totalReceivedBytes;

            if (length < MAX_BUFFER_SIZE)
            {
              // read into read-ahead buffer
              do
              {
                n = libssh2_channel_read(storageHandle->scp.channel,
                                         (char*)storageHandle->scp.readAheadBuffer.data,
                                         MIN((size_t)(storageHandle->scp.size-storageHandle->scp.index),MAX_BUFFER_SIZE)
                                       );
                if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*MISC_US_PER_MS);
              }
              while (n == LIBSSH2_ERROR_EAGAIN);
              if (n < 0)
              {
                error = ERROR_(IO_ERROR,errno);
                break;
              }
              storageHandle->scp.readAheadBuffer.offset = storageHandle->scp.index;
              storageHandle->scp.readAheadBuffer.length = (ulong)n;
//fprintf(stderr,"%s,%d: n=%ld storageHandle->scp.bufferOffset=%llu storageHandle->scp.bufferLength=%lu\n",__FILE__,__LINE__,n,
//storageHandle->scp.readAheadBuffer.offset,storageHandle->scp.readAheadBuffer.length);

              // copy data from read-ahead buffer
              bytesAvail = MIN(length,storageHandle->scp.readAheadBuffer.length);
              memcpy(buffer,storageHandle->scp.readAheadBuffer.data,bytesAvail);

              // adjust buffer, size, bytes read, index
              buffer = (byte*)buffer+bytesAvail;
              size -= bytesAvail;
              if (bytesRead != NULL) (*bytesRead) += bytesAvail;
              storageHandle->scp.index += (uint64)bytesAvail;
            }
            else
            {
              // read direct
              do
              {
                n = libssh2_channel_read(storageHandle->scp.channel,
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
              storageHandle->scp.index += (uint64)n;
            }

            // get end time, end received bytes
            endTimestamp          = Misc_getTimestamp();
            endTotalReceivedBytes = storageHandle->scp.totalReceivedBytes;
            assert(endTotalReceivedBytes >= startTotalReceivedBytes);

            /* limit used band width if requested (note: when the system time is
               changing endTimestamp may become smaller than startTimestamp;
               thus do not check this with an assert())
            */
            if (endTimestamp >= startTimestamp)
            {
              limitBandWidth(&storageHandle->scp.bandWidthLimiter,
                             endTotalReceivedBytes-startTotalReceivedBytes,
                             endTimestamp-startTimestamp
                            );
            }
          }
        }
      }
    }
  #else /* not HAVE_SSH2 */
    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return error;
}

LOCAL Errors StorageSCP_write(StorageHandle *storageHandle,
                              const void    *buffer,
                              ulong         size
                             )
{
  Errors error;
  #ifdef HAVE_SSH2
    ulong   writtenBytes;
    ulong   length;
    uint64  startTimestamp,endTimestamp;
    uint64  startTotalSentBytes,endTotalSentBytes;
    ssize_t n;
  #endif /* HAVE_SSH2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(buffer != NULL);

  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
    {
      assert(storageHandle->scp.channel != NULL);

      writtenBytes = 0L;
      while (writtenBytes < size)
      {
        // get max. number of bytes to send in one step
        if (storageHandle->scp.bandWidthLimiter.maxBandWidthList != NULL)
        {
          length = MIN(storageHandle->scp.bandWidthLimiter.blockSize,size-writtenBytes);
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
        startTotalSentBytes = storageHandle->scp.totalSentBytes;

        // send data
        do
        {
          n = libssh2_channel_write(storageHandle->scp.channel,
                                    buffer,
                                    length
                                   );
          if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*MISC_US_PER_MS);
        }
        while (n == LIBSSH2_ERROR_EAGAIN);

        // get end time, end received bytes
        endTimestamp      = Misc_getTimestamp();
        endTotalSentBytes = storageHandle->scp.totalSentBytes;
        assert(endTotalSentBytes >= startTotalSentBytes);

// ??? is it possible in blocking-mode that write() return 0 and this is not an error?
#if 1
        if      (n == 0)
        {
          // should not happen in blocking-mode: bug? libssh2 API changed somewhere between 0.18 and 1.2.4? => wait for data
          if (!waitSSHSessionSocket(&storageHandle->scp.socketHandle))
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
          limitBandWidth(&storageHandle->scp.bandWidthLimiter,
                         endTotalSentBytes-startTotalSentBytes,
                         endTimestamp-startTimestamp
                        );
        }
      }
    }
  #else /* not HAVE_SSH2 */
    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL uint64 StorageSCP_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  size = 0LL;
  #ifdef HAVE_SSH2
    if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
    {
      size = storageHandle->scp.size;
    }
  #else /* not HAVE_SSH2 */
  #endif /* HAVE_SSH2 */

  return size;
}

LOCAL Errors StorageSCP_tell(StorageHandle *storageHandle,
                             uint64        *offset
                            )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
    {
      (*offset) = storageHandle->scp.index;
      error     = ERROR_NONE;
    }
  #else /* not HAVE_SSH2 */
    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageSCP_seek(StorageHandle *storageHandle,
                             uint64        offset
                            )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    /* scp protocol does not support a seek-function. Thus try to
       read and discard data to position the read index to the
       requested offset.
       Note: this is slow!
    */

    if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
    {
      assert(storageHandle->scp.channel != NULL);
      assert(storageHandle->scp.readAheadBuffer.data != NULL);

      if      (offset > storageHandle->scp.index)
      {
        uint64  skip;
        uint64  i;
        uint64  n;
        ssize_t readBytes;

        skip = offset-storageHandle->scp.index;
        while (skip > 0LL)
        {
          // skip data in read-ahead buffer
          if (   (storageHandle->scp.index >= storageHandle->scp.readAheadBuffer.offset)
              && (storageHandle->scp.index < (storageHandle->scp.readAheadBuffer.offset+storageHandle->scp.readAheadBuffer.length))
             )
          {
            i = storageHandle->scp.index-storageHandle->scp.readAheadBuffer.offset;
            n = MIN(skip,storageHandle->scp.readAheadBuffer.length-i);
            skip -= n;
            storageHandle->scp.index += (uint64)n;
          }

          if (skip > 0LL)
          {
            assert(storageHandle->scp.index >= (storageHandle->scp.readAheadBuffer.offset+storageHandle->scp.readAheadBuffer.length));

            // wait for data
            if (!waitSSHSessionSocket(&storageHandle->scp.socketHandle))
            {
              error = ERROR_(IO_ERROR,errno);
              break;
            }

            // read data
            readBytes = libssh2_channel_read(storageHandle->scp.channel,
                                             (char*)storageHandle->scp.readAheadBuffer.data,
                                             MIN((size_t)skip,MAX_BUFFER_SIZE)
                                            );
            if (readBytes < 0)
            {
              error = ERROR_(IO_ERROR,errno);
              break;
            }
            storageHandle->scp.readAheadBuffer.offset = storageHandle->scp.index;
            storageHandle->scp.readAheadBuffer.length = (uint64)readBytes;
          }
        }
      }
      else if (offset < storageHandle->scp.index)
      {
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      }
    }
  #else /* not HAVE_SSH2 */
    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageSCP_delete(StorageHandle *storageHandle,
                               ConstString   storageFileName
                              )
{
//  ConstString deleteFileName;
  Errors      error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

//  deleteFileName = (storageFileName != NULL) ? storageFileName : storageHandle->storageSpecifier.archiveName;
UNUSED_VARIABLE(storageHandle);
UNUSED_VARIABLE(storageFileName);

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
                                          ServerConnectionPriorities serverConnectionPriority
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
