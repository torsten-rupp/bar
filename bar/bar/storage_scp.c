/***********************************************************************\
*
* $Revision: 4012 $
* $Date: 2015-04-28 19:02:40 +0200 (Tue, 28 Apr 2015) $
* $Author: torsten $
* Contents: storage SCP functions
* Systems: all
*
\***********************************************************************/

#define __STORAGE_IMPLEMENTATION__

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

#include "common/global.h"
#include "common/autofree.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/files.h"
#include "common/network.h"
#include "common/passwords.h"
#include "common/misc.h"

#include "errors.h"
#include "crypt.h"
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
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_SCP);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->scp);
  assert(storageHandle->scp.oldSendCallback != NULL);

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
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_SCP);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->scp);
  assert(storageHandle->scp.oldReceiveCallback != NULL);

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
  String s,t;

  assert(sshSpecifier != NULL);
  assert(hostName != NULL);
  assert(loginName != NULL);

  String_clear(hostName);
  if (hostPort != NULL) (*hostPort) = 0;
  String_clear(loginName);
  if (loginPassword != NULL) Password_clear(loginPassword);

  s = String_new();
  t = String_new();
  if      (String_matchCString(sshSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@:/]*?):([[:digit:]]+)$",NULL,STRING_NO_ASSIGN,loginName,s,STRING_NO_ASSIGN,hostName,t,NULL))
  {
    // <login name>:<login password>@<host name>:<host port>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (loginPassword != NULL) Password_setString(loginPassword,s);
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(t,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)$",NULL,STRING_NO_ASSIGN,loginName,s,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>:<login password>@<host name>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (loginPassword != NULL) Password_setString(loginPassword,s);

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^:]+?):(\\d*)/{0,1}$",NULL,STRING_NO_ASSIGN,loginName,STRING_NO_ASSIGN,hostName,s,NULL))
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
  String_delete(t);
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

LOCAL String StorageSCP_getName(String                 string,
                                const StorageSpecifier *storageSpecifier,
                                ConstString            archiveName
                               )
{
  ConstString storageFileName;
  const char  *plainPassword;

  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_SCP);

  // get file to use
  if      (!String_isEmpty(archiveName))
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

  String_appendCString(string,"scp://");
  if (!String_isEmpty(storageSpecifier->loginName))
  {
    String_append(string,storageSpecifier->loginName);
    if (!Password_isEmpty(storageSpecifier->loginPassword))
    {
      String_appendChar(string,':');
      plainPassword = Password_deploy(storageSpecifier->loginPassword);
      String_appendCString(string,plainPassword);
      Password_undeploy(storageSpecifier->loginPassword,plainPassword);
    }
    String_appendChar(string,'@');
  }
  String_append(string,storageSpecifier->hostName);
  if (!String_isEmpty(storageFileName))
  {
    String_appendChar(string,'/');
    String_append(string,storageFileName);
  }

  return string;
}

LOCAL void StorageSCP_getPrintableName(String                 string,
                                       const StorageSpecifier *storageSpecifier,
                                       ConstString            fileName
                                      )
{
  ConstString storageFileName;

  assert(string != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_SCP);

  // get file to use
  if      (!String_isEmpty(fileName))
  {
    storageFileName = fileName;
  }
  else if (!String_isEmpty(storageSpecifier->archivePatternString))
  {
    storageFileName = storageSpecifier->archivePatternString;
  }
  else
  {
    storageFileName = storageSpecifier->archiveName;
  }

  String_appendCString(string,"scp://");
  String_append(string,storageSpecifier->hostName);
  if ((storageSpecifier->hostPort != 0) && (storageSpecifier->hostPort != 22))
  {
    String_appendFormat(string,":%d",storageSpecifier->hostPort);
  }
  if (!String_isEmpty(storageFileName))
  {
    String_appendChar(string,'/');
    String_append(string,storageFileName);
  }
}

LOCAL Errors StorageSCP_init(StorageInfo                *storageInfo,
                             const StorageSpecifier     *storageSpecifier,
                             const JobOptions           *jobOptions,
                             BandWidthList              *maxBandWidthList,
                             ServerConnectionPriorities serverConnectionPriority
                            )
{
  #ifdef HAVE_SSH2
    AutoFreeList autoFreeList;
    Errors       error;
    SSHServer    sshServer;
    uint         retries;
    Password     password;
  #endif /* HAVE_SSH2 */

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_SCP);
  assert(storageSpecifier != NULL);

  UNUSED_VARIABLE(storageSpecifier);

  #ifdef HAVE_SSH2
    // init variables
    AutoFree_init(&autoFreeList);
    storageInfo->scp.sshPublicKeyFileName   = NULL;
    storageInfo->scp.sshPrivateKeyFileName  = NULL;
    initBandWidthLimiter(&storageInfo->scp.bandWidthLimiter,maxBandWidthList);
    AUTOFREE_ADD(&autoFreeList,&storageInfo->scp.bandWidthLimiter,{ doneBandWidthLimiter(&storageInfo->scp.bandWidthLimiter); });

    // get SSH server settings
    storageInfo->scp.serverId = initSSHServerSettings(&sshServer,storageInfo->storageSpecifier.hostName,jobOptions);
    AUTOFREE_ADD(&autoFreeList,&sshServer,{ doneSSHServerSettings(&sshServer); });
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_set(storageInfo->storageSpecifier.loginName,sshServer.loginName);
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("USER"));
    if (storageInfo->storageSpecifier.hostPort == 0) storageInfo->storageSpecifier.hostPort = sshServer.port;
    storageInfo->sftp.publicKey  = sshServer.publicKey;
    storageInfo->sftp.privateKey = sshServer.privateKey;
    if (String_isEmpty(storageInfo->storageSpecifier.hostName))
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
    if (!allocateServer(storageInfo->scp.serverId,serverConnectionPriority,60*1000L))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,&storageInfo->scp.serverId,{ freeServer(storageInfo->scp.serverId); });

    // check if SSH login is possible
    error = ERROR_SSH_AUTHENTICATION;
    if ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && !Password_isEmpty(storageInfo->storageSpecifier.loginPassword))
    {
      error = checkSSHLogin(storageInfo->storageSpecifier.hostName,
                            storageInfo->storageSpecifier.hostPort,
                            storageInfo->storageSpecifier.loginName,
                            storageInfo->storageSpecifier.loginPassword,
                            storageInfo->scp.publicKey.data,
                            storageInfo->scp.publicKey.length,
                            storageInfo->scp.privateKey.data,
                            storageInfo->scp.privateKey.length
                           );
    }
    if ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && !Password_isEmpty(&sshServer.password))
    {
      error = checkSSHLogin(storageInfo->storageSpecifier.hostName,
                            storageInfo->storageSpecifier.hostPort,
                            storageInfo->storageSpecifier.loginName,
                            &sshServer.password,
                            storageInfo->scp.publicKey.data,
                            storageInfo->scp.publicKey.length,
                            storageInfo->scp.privateKey.data,
                            storageInfo->scp.privateKey.length
                           );
      if (error == ERROR_NONE)
      {
        Password_set(storageInfo->storageSpecifier.loginPassword,&sshServer.password);
      }
    }
    if (Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION)
    {
      // initialize interactive/default password
      retries = 0;
      Password_init(&password);
      while ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && (retries < MAX_PASSWORD_REQUESTS))
      {
        if  (initSSHLogin(storageInfo->storageSpecifier.hostName,
                          storageInfo->storageSpecifier.loginName,
                          &password,
                          jobOptions,
                          CALLBACK(storageInfo->getNamePasswordFunction,storageInfo->getNamePasswordUserData)
                         )
           )
        {
          error = checkSSHLogin(storageInfo->storageSpecifier.hostName,
                                storageInfo->storageSpecifier.hostPort,
                                storageInfo->storageSpecifier.loginName,
                                &password,
                                storageInfo->scp.publicKey.data,
                                storageInfo->scp.publicKey.length,
                                storageInfo->scp.privateKey.data,
                                storageInfo->scp.privateKey.length
                               );
          if (error == ERROR_NONE)
          {
            Password_set(storageInfo->storageSpecifier.loginPassword,&password);
          }
        }
        retries++;
      }
      Password_done(&password);
    }
    if (Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION)
    {
      error = (   !Password_isEmpty(storageInfo->storageSpecifier.loginPassword)
               || !Password_isEmpty(&sshServer.password)
               || !Password_isEmpty(&defaultSSHPassword)
              )
                ? ERRORX_(INVALID_SSH_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName))
                : ERRORX_(NO_SSH_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName));
    }

    // store password as default password
    if (error == ERROR_NONE)
    {
      Password_set(&defaultSSHPassword,storageInfo->storageSpecifier.loginPassword);
    }
    assert(error != ERROR_UNKNOWN);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // free resources
    doneSSHServerSettings(&sshServer);
    AutoFree_done(&autoFreeList);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(maxBandWidthList);
    UNUSED_VARIABLE(serverConnectionPriority);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return ERROR_NONE;
}

LOCAL Errors StorageSCP_done(StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_SCP);

  // free SSH server connection
  #ifdef HAVE_SSH2
    freeServer(storageInfo->scp.serverId);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
  #endif /* HAVE_SSH2 */

  return ERROR_NONE;
}

LOCAL bool StorageSCP_isServerAllocationPending(const StorageInfo *storageInfo)
{
  bool serverAllocationPending;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_SCP);

  serverAllocationPending = FALSE;
  #if defined(HAVE_SSH2)
    serverAllocationPending = isServerAllocationPending(storageInfo->scp.serverId);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);

    serverAllocationPending = FALSE;
  #endif /* HAVE_SSH2 */

  return serverAllocationPending;
}

LOCAL Errors StorageSCP_preProcess(const StorageInfo *storageInfo,
                                   ConstString       archiveName,
                                   time_t            timestamp,
                                   bool              initialFlag
                                  )
{
  Errors error;
  #ifdef HAVE_SSH2
    TextMacro textMacros[2];
  #endif /* HAVE_SSH2 */

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_SCP);

  error = ERROR_NONE;

  #ifdef HAVE_SSH2
    if (!initialFlag)
    {
      // init macros
      TEXT_MACRO_N_STRING (textMacros[0],"%file",  archiveName,                NULL);
      TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageInfo->volumeNumber,NULL);

      if (!String_isEmpty(globalOptions.scp.writePreProcessCommand))
      {
        printInfo(1,"Write pre-processing...");
        error = executeTemplate(String_cString(globalOptions.scp.writePreProcessCommand),
                                timestamp,
                                textMacros,
                                SIZE_OF_ARRAY(textMacros)
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(timestamp);
    UNUSED_VARIABLE(initialFlag);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return error;
}

LOCAL Errors StorageSCP_postProcess(const StorageInfo *storageInfo,
                                    ConstString       archiveName,
                                    time_t            timestamp,
                                    bool              finalFlag
                                   )
{
  Errors error;
  #ifdef HAVE_SSH2
    TextMacro textMacros[2];
  #endif /* HAVE_SSH2 */

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_SCP);

  error = ERROR_NONE;

  #ifdef HAVE_SSH2
    if (!finalFlag)
    {
      // init macros
      TEXT_MACRO_N_STRING (textMacros[0],"%file",  archiveName,                NULL);
      TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageInfo->volumeNumber,NULL);

      if (!String_isEmpty(globalOptions.scp.writePostProcessCommand))
      {
        printInfo(1,"Write post-processing...");
        error = executeTemplate(String_cString(globalOptions.scp.writePostProcessCommand),
                                timestamp,
                                textMacros,
                                SIZE_OF_ARRAY(textMacros)
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(timestamp);
    UNUSED_VARIABLE(finalFlag);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return error;
}

LOCAL bool StorageSCP_exists(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_exists(archiveName);
}

LOCAL bool StorageSCP_isFile(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_exists(archiveName);
}

LOCAL bool StorageSCP_isDirectory(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_exists(archiveName);
}

LOCAL bool StorageSCP_isReadable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_exists(archiveName);
}

LOCAL bool StorageSCP_isWritable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_exists(archiveName);
}

LOCAL Errors StorageSCP_getTmpName(String archiveName, const StorageInfo *storageInfo)
{
  assert(archiveName != NULL);
  assert(!String_isEmpty(archiveName));
  assert(storageInfo != NULL);

  UNUSED_VARIABLE(storageInfo);

return ERROR_STILL_NOT_IMPLEMENTED;
  return File_getTmpFileName(archiveName,String_cString(archiveName),NULL);
}

LOCAL Errors StorageSCP_create(StorageHandle *storageHandle,
                               ConstString   fileName,
                               uint64        fileSize
                              )
{
  #ifdef HAVE_SSH2
    Errors error;
  #endif /* HAVE_SSH2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_SCP);
  assert(!String_isEmpty(fileName));

  UNUSED_VARIABLE(fileSize);

  // check if file exists
  if (   (storageHandle->storageInfo->jobOptions != NULL)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_APPEND)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_OVERWRITE)
      && StorageSCP_exists(storageHandle->storageInfo,fileName)
     )
  {
    return ERRORX_(FILE_EXISTS_,0,"%s",String_cString(fileName));
  }

  #ifdef HAVE_SSH2
    // init variables
    storageHandle->scp.channel                = NULL;
    storageHandle->scp.oldSendCallback        = NULL;
    storageHandle->scp.oldReceiveCallback     = NULL;
    storageHandle->scp.totalSentBytes         = 0LL;
    storageHandle->scp.totalReceivedBytes     = 0LL;
    storageHandle->scp.index                  = 0LL;
    storageHandle->scp.size                   = 0LL;
    storageHandle->scp.readAheadBuffer.offset = 0LL;
    storageHandle->scp.readAheadBuffer.length = 0L;

    // connect
    error = Network_connect(&storageHandle->scp.socketHandle,
                            SOCKET_TYPE_SSH,
                            storageHandle->storageInfo->storageSpecifier.hostName,
                            storageHandle->storageInfo->storageSpecifier.hostPort,
                            storageHandle->storageInfo->storageSpecifier.loginName,
                            storageHandle->storageInfo->storageSpecifier.loginPassword,
                            storageHandle->storageInfo->scp.publicKey.data,
                            storageHandle->storageInfo->scp.publicKey.length,
                            storageHandle->storageInfo->scp.privateKey.data,
                            storageHandle->storageInfo->scp.privateKey.length,
                              SOCKET_FLAG_NONE
                            | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                            | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0)
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

    // open channel and file for writing
    #ifdef HAVE_SSH2_SCP_SEND64
      storageHandle->scp.channel = libssh2_scp_send64(Network_getSSHSession(&storageHandle->scp.socketHandle),
                                                          String_cString(fileName),
// ???
0600,
                                                          (libssh2_uint64_t)fileSize,
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
                      "%s",
                      sshErrorText
                     );
      libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,storageHandle->scp.oldReceiveCallback);
      libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,storageHandle->scp.oldSendCallback);
      Network_disconnect(&storageHandle->scp.socketHandle);
      return error;
    }

    DEBUG_ADD_RESOURCE_TRACE(&storageHandle->scp,StorageHandleSCP);

    return ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(fileSize);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL Errors StorageSCP_open(StorageHandle *storageHandle,
                             ConstString   archiveName
                            )
{
  #ifdef HAVE_SSH2
    Errors      error;
    struct stat fileInfo;
  #endif /* HAVE_SSH2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_SCP);
  assert(!String_isEmpty(archiveName));

  #ifdef HAVE_SSH2
    // init variables
    storageHandle->scp.channel                = NULL;
    storageHandle->scp.oldSendCallback        = NULL;
    storageHandle->scp.oldReceiveCallback     = NULL;
    storageHandle->scp.totalSentBytes         = 0LL;
    storageHandle->scp.totalReceivedBytes     = 0LL;
    storageHandle->scp.index                  = 0LL;
    storageHandle->scp.size                   = 0LL;
    storageHandle->scp.readAheadBuffer.offset = 0LL;
    storageHandle->scp.readAheadBuffer.length = 0L;

    // allocate read-ahead buffer
    storageHandle->scp.readAheadBuffer.data = (byte*)malloc(MAX_BUFFER_SIZE);
    if (storageHandle->scp.readAheadBuffer.data == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    // connect
    error = Network_connect(&storageHandle->scp.socketHandle,
                            SOCKET_TYPE_SSH,
                            storageHandle->storageInfo->storageSpecifier.hostName,
                            storageHandle->storageInfo->storageSpecifier.hostPort,
                            storageHandle->storageInfo->storageSpecifier.loginName,
                            storageHandle->storageInfo->storageSpecifier.loginPassword,
                            storageHandle->storageInfo->scp.publicKey.data,
                            storageHandle->storageInfo->scp.publicKey.length,
                            storageHandle->storageInfo->scp.privateKey.data,
                            storageHandle->storageInfo->scp.privateKey.length,
                              SOCKET_FLAG_NONE
                            | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                            | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0)
                           );
    if (error != ERROR_NONE)
    {
      free(storageHandle->scp.readAheadBuffer.data);
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
                      "%s",
                      sshErrorText
                     );
      libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,storageHandle->scp.oldReceiveCallback);
      libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,storageHandle->scp.oldSendCallback);
      Network_disconnect(&storageHandle->scp.socketHandle);
      free(storageHandle->scp.readAheadBuffer.data);
      return error;
    }
    storageHandle->scp.size = (uint64)fileInfo.st_size;

    DEBUG_ADD_RESOURCE_TRACE(&storageHandle->scp,StorageHandleSCP);

    return ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(archiveName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL void StorageSCP_close(StorageHandle *storageHandle)
{
  #ifdef HAVE_SSH2
    int result;
  #endif /* HAVE_SSH2 */

  #ifdef HAVE_SSH2
  assert(storageHandle != NULL);
    DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->scp);
  #endif /* HAVE_SSH2 */
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_SCP);

  #ifdef HAVE_SSH2
    DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->scp,StorageHandleSCP);

    libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,storageHandle->scp.oldReceiveCallback);
    libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,storageHandle->scp.oldSendCallback);

    switch (storageHandle->mode)
    {
      case STORAGE_MODE_READ:
        (void)libssh2_channel_close(storageHandle->scp.channel);
        (void)libssh2_channel_wait_closed(storageHandle->scp.channel);
        (void)libssh2_channel_free(storageHandle->scp.channel);
        free(storageHandle->scp.readAheadBuffer.data);
        break;
      case STORAGE_MODE_WRITE:
        result = 0;

        if (result == 0)
        {
          do
          {
            result = libssh2_channel_send_eof(storageHandle->scp.channel);
            if (result == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
          }
          while (result == LIBSSH2_ERROR_EAGAIN);
        }
        if (result == 0)
        {
          do
          {
            result = libssh2_channel_wait_eof(storageHandle->scp.channel);
            if (result == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
          }
          while (result == LIBSSH2_ERROR_EAGAIN);
        }
        if (result == 0)
        {
          do
          {
            result = libssh2_channel_close(storageHandle->scp.channel);
            if (result == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
          }
          while (result == LIBSSH2_ERROR_EAGAIN);
        }
        if (result == 0)
        {
          do
          {
            result = libssh2_channel_wait_closed(storageHandle->scp.channel);
            if (result == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
          }
          while (result == LIBSSH2_ERROR_EAGAIN);
        }
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
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_SSH2 */
}

LOCAL bool StorageSCP_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->scp);
  #endif /* HAVE_SSH2 */
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_SCP);

  #ifdef HAVE_SSH2
    return storageHandle->scp.index >= storageHandle->scp.size;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);

    return TRUE;
  #endif /* HAVE_SSH2 */
}

LOCAL Errors StorageSCP_read(StorageHandle *storageHandle,
                             void          *buffer,
                             ulong         bufferSize,
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

  assert(storageHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->scp);
  #endif /* HAVE_SSH2 */
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_SCP);
  assert(buffer != NULL);

  if (bytesRead != NULL) (*bytesRead) = 0L;
  #ifdef HAVE_SSH2
    assert(storageHandle->scp.channel != NULL);
    assert(storageHandle->scp.readAheadBuffer.data != NULL);

    error = ERROR_NONE;
    while (   (bufferSize > 0L)
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
        bytesAvail = MIN(bufferSize,storageHandle->scp.readAheadBuffer.length-index);
        memcpy(buffer,storageHandle->scp.readAheadBuffer.data+index,bytesAvail);

        // adjust buffer, bufferSize, bytes read, index
        buffer = (byte*)buffer+bytesAvail;
        bufferSize -= bytesAvail;
        if (bytesRead != NULL) (*bytesRead) += bytesAvail;
        storageHandle->scp.index += (uint64)bytesAvail;
      }

      // read rest of data
      if (bufferSize > 0)
      {
        assert(storageHandle->scp.index >= (storageHandle->scp.readAheadBuffer.offset+storageHandle->scp.readAheadBuffer.length));

        // get max. number of bytes to receive in one step
        if (storageHandle->storageInfo->scp.bandWidthLimiter.maxBandWidthList != NULL)
        {
          length = MIN(storageHandle->storageInfo->scp.bandWidthLimiter.blockSize,bufferSize);
        }
        else
        {
          length = bufferSize;
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
            if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
          }
          while (n == LIBSSH2_ERROR_EAGAIN);
          if (n < 0)
          {
            error = ERROR_(IO,errno);
            break;
          }
          storageHandle->scp.readAheadBuffer.offset = storageHandle->scp.index;
          storageHandle->scp.readAheadBuffer.length = (ulong)n;
//fprintf(stderr,"%s,%d: n=%ld storageHandle->scp.bufferOffset=%llu storageHandle->scp.bufferLength=%lu\n",__FILE__,__LINE__,n,
//storageHandle->scp.readAheadBuffer.offset,storageHandle->scp.readAheadBuffer.length);

          // copy data from read-ahead buffer
          bytesAvail = MIN(length,storageHandle->scp.readAheadBuffer.length);
          memcpy(buffer,storageHandle->scp.readAheadBuffer.data,bytesAvail);

          // adjust buffer, bufferSize, bytes read, index
          buffer = (byte*)buffer+bytesAvail;
          bufferSize -= bytesAvail;
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
            if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
          }
          while (n == LIBSSH2_ERROR_EAGAIN);
          if (n < 0)
          {
            error = ERROR_(IO,errno);
            break;
          }

          // adjust buffer, bufferSize, bytes read, index
          buffer = (byte*)buffer+(ulong)n;
          bufferSize -= (ulong)n;
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
          limitBandWidth(&storageHandle->storageInfo->scp.bandWidthLimiter,
                         endTotalReceivedBytes-startTotalReceivedBytes,
                         endTimestamp-startTimestamp
                        );
        }
      }
    }

    return error;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferSize);
    UNUSED_VARIABLE(bytesRead);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL Errors StorageSCP_write(StorageHandle *storageHandle,
                              const void    *buffer,
                              ulong         bufferLength
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

  assert(storageHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->scp);
  #endif /* HAVE_SSH2 */
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_SCP);
  assert(buffer != NULL);

  #ifdef HAVE_SSH2
    assert(storageHandle->scp.channel != NULL);

    error = ERROR_NONE;
    writtenBytes = 0L;
    while (writtenBytes < bufferLength)
    {
      // get max. number of bytes to send in one step
      if (storageHandle->storageInfo->scp.bandWidthLimiter.maxBandWidthList != NULL)
      {
        length = MIN(storageHandle->storageInfo->scp.bandWidthLimiter.blockSize,bufferLength-writtenBytes);
      }
      else
      {
        length = bufferLength-writtenBytes;
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
        if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
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
        limitBandWidth(&storageHandle->storageInfo->scp.bandWidthLimiter,
                       endTotalSentBytes-startTotalSentBytes,
                       endTimestamp-startTimestamp
                      );
      }
    }
    assert(error != ERROR_UNKNOWN);

    return error;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferLength);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL uint64 StorageSCP_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->scp);
  #endif /* HAVE_SSH2 */
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_SCP);

  size = 0LL;
  #ifdef HAVE_SSH2
    size = storageHandle->scp.size;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_SSH2 */

  return size;
}

LOCAL Errors StorageSCP_tell(StorageHandle *storageHandle,
                             uint64        *offset
                            )
{
  Errors error;

  assert(storageHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->scp);
  #endif /* HAVE_SSH2 */
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_SCP);
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    (*offset) = storageHandle->scp.index;
    error     = ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageSCP_seek(StorageHandle *storageHandle,
                             uint64        offset
                            )
{
  #ifdef HAVE_SSH2
    Errors error;
  #endif /* HAVE_SSH2 */

  assert(storageHandle != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->scp);
  #endif /* HAVE_SSH2 */
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_SCP);

  #ifdef HAVE_SSH2
    /* scp protocol does not support a seek-function. Thus try to
       read and discard data to position the read index to the
       requested offset.
       Note: this is slow!
    */

    assert(storageHandle->scp.channel != NULL);
    assert(storageHandle->scp.readAheadBuffer.data != NULL);

    error = ERROR_NONE;
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
            error = ERROR_(IO,errno);
            break;
          }

          // read data
          readBytes = libssh2_channel_read(storageHandle->scp.channel,
                                           (char*)storageHandle->scp.readAheadBuffer.data,
                                           MIN((size_t)skip,MAX_BUFFER_SIZE)
                                          );
          if (readBytes < 0)
          {
            error = ERROR_(IO,errno);
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
    assert(error != ERROR_UNKNOWN);

    return error;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL Errors StorageSCP_rename(const StorageInfo *storageInfo,
                               ConstString       fromArchiveName,
                               ConstString       toArchiveName
                              )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_SCP);

UNUSED_VARIABLE(storageInfo);
UNUSED_VARIABLE(fromArchiveName);
UNUSED_VARIABLE(toArchiveName);
error = ERROR_STILL_NOT_IMPLEMENTED;

  return error;
}

LOCAL Errors StorageSCP_delete(const StorageInfo *storageInfo,
                               ConstString       archiveName
                              )
{
//  ConstString deleteFileName;
  Errors      error;

  assert(storageInfo != NULL);
  #ifdef HAVE_SSH2
    DEBUG_CHECK_RESOURCE_TRACE(storageInfo);
  #endif /* HAVE_SSH2 */
  assert(storageInfo->type == STORAGE_TYPE_SCP);
  assert(!String_isEmpty(archiveName));

//  deleteFileName = (storageFileName != NULL) ? storageFileName : storageInfo->storageSpecifier.archiveName;
UNUSED_VARIABLE(storageInfo);
UNUSED_VARIABLE(archiveName);

  error = ERROR_UNKNOWN;
  #ifdef HAVE_SSH2
#if 0
whould this be a possible implementation?
    {
      String command;

      assert(storageInfo->scp.channel != NULL);

      // there is no unlink command for scp: execute either 'rm' or 'del' on remote server
      command = String_new();
      String_format(command,"rm %'S",deleteFileName);
      error = (libssh2_channel_exec(storageInfo->scp.channel,
                                    String_cString(command)
                                   ) != 0
              ) ? ERROR_NONE : ERROR_DELETE_FILE;
      if (error != ERROR_NONE)
      {
        String_format(command,"del %'S",deleteFileName);
        error = (libssh2_channel_exec(storageInfo->scp.channel,
                                      String_cString(command)
                                     ) != 0
                ) ? ERROR_NONE : ERROR_DELETE_FILE;
      }
      String_delete(command);
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
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
LOCAL Errors StorageSCP_getInfo(const StorageInfo *storageInfo,
                                ConstString       fileName,
                                FileInfo          *fileInfo
                               )
{
  String infoFileName;
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_SCP);
  assert(fileInfo != NULL);

  infoFileName = (fileName != NULL) ? fileName : storageInfo->storageSpecifier.archiveName;
  memset(fileInfo,0,sizeof(fileInfo));

  error = ERROR_UNKNOWN;
  switch (storageInfo->type)
      error = ERROR_FUNCTION_NOT_SUPPORTED;
  assert(error != ERROR_UNKNOWN);

  return error;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageSCP_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                          const StorageSpecifier     *storageSpecifier,
                                          ConstString                pathName,
                                          const JobOptions           *jobOptions,
                                          ServerConnectionPriorities serverConnectionPriority
                                         )
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_SCP);
  assert(pathName != NULL);
  assert(jobOptions != NULL);

  UNUSED_VARIABLE(storageDirectoryListHandle);
  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(pathName);
  UNUSED_VARIABLE(jobOptions);
  UNUSED_VARIABLE(serverConnectionPriority);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

LOCAL void StorageSCP_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  UNUSED_VARIABLE(storageDirectoryListHandle);
}

LOCAL bool StorageSCP_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  UNUSED_VARIABLE(storageDirectoryListHandle);

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

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
