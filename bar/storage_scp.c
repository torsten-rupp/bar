/***********************************************************************\
*
* Contents: storage SCP functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
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

#include "bar.h"
#include "bar_common.h"
#include "errors.h"
#include "crypt.h"
#include "archives.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
/* file data buffer size */
#define BUFFER_SIZE (64*1024)

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
  assert(abstract != NULL);

  StorageHandle *storageHandle = *((StorageHandle**)abstract);
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(storageHandle->scp.oldSendCallback != NULL);

  ssize_t n = storageHandle->scp.oldSendCallback(socket,buffer,length,flags,abstract);
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
  assert(abstract != NULL);

  StorageHandle *storageHandle = *((StorageHandle**)abstract);
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(storageHandle->scp.oldReceiveCallback != NULL);

  ssize_t n = storageHandle->scp.oldReceiveCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageHandle->scp.totalReceivedBytes += (uint64)n;

  return n;
}

#endif /* HAVE_SSH2 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageSCP_initAll(void)
{
  return ERROR_NONE;
}

LOCAL void StorageSCP_doneAll(void)
{
}

LOCAL bool StorageSCP_parseSpecifier(ConstString sshSpecifier,
                                     String      hostName,
                                     uint        *hostPort,
                                     String      userName,
                                     Password    *password
                                    )
{
  const char* LOGINNAME_MAP_FROM[] = {"\\@"};
  const char* LOGINNAME_MAP_TO[]   = {"@"};

  bool result;

  assert(sshSpecifier != NULL);
  assert(hostName != NULL);
  assert(userName != NULL);

  String_clear(hostName);
  if (hostPort != NULL) (*hostPort) = 0;
  String_clear(userName);
  if (password != NULL) Password_clear(password);

  String s = String_new();
  String t = String_new();
  if      (String_matchCString(sshSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@:/]*?):([[:digit:]]+)$",NULL,STRING_NO_ASSIGN,userName,s,STRING_NO_ASSIGN,hostName,t,NULL))
  {
    // <login name>:<login password>@<host name>:<host port>
    String_mapCString(userName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (password != NULL) Password_setString(password,s);
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(t,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)$",NULL,STRING_NO_ASSIGN,userName,s,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>:<login password>@<host name>
    String_mapCString(userName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (password != NULL) Password_setString(password,s);

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^:]+?):(\\d*)/{0,1}$",NULL,STRING_NO_ASSIGN,userName,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <login name>@<host name>:<host port>
    if (userName != NULL) String_mapCString(userName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (hostPort != NULL)
    {
      if (!String_isEmpty(s)) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);
    }

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^/]+)/{0,1}$",NULL,STRING_NO_ASSIGN,userName,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>@<host name>
    if (userName != NULL) String_mapCString(userName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);

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
         && String_equals(archiveName1,archiveName2);
}

LOCAL String StorageSCP_getName(String                 string,
                                const StorageSpecifier *storageSpecifier,
                                ConstString            archiveName
                               )
{
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_SCP);

  // get file to use
  ConstString storageFileName;
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

  String_appendCString(string,"scp://");
  if (!String_isEmpty(storageSpecifier->userName))
  {
    String_append(string,storageSpecifier->userName);
    if (!Password_isEmpty(&storageSpecifier->password))
    {
      String_appendChar(string,':');
      PASSWORD_DEPLOY_DO(plainPassword,&storageSpecifier->password)
      {
        String_appendCString(string,plainPassword);
      }
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

/***********************************************************************\
* Name   : StorageSCP_getPrintableName
* Purpose: get printable storage name (without password)
* Input  : string           - name variable (can be NULL)
*          storageSpecifier - storage specifier string
*          archiveName      - archive name (can be NULL)
* Output : -
* Return : printable storage name
* Notes  : if archiveName is NULL file name from storageSpecifier is used
\***********************************************************************/

LOCAL void StorageSCP_getPrintableName(String                 string,
                                       const StorageSpecifier *storageSpecifier,
                                       ConstString            fileName
                                      )
{
  assert(string != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_SCP);

  // get file to use
  ConstString storageFileName;
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

/***********************************************************************\
* Name   : StorageSCP_init
* Purpose: init new storage
* Input  : storageInfo                     - storage info variable
*          jobOptions                      - job options or NULL
*          maxBandWidthList                - list with max. band width
*                                            to use [bits/s] or NULL
*          serverConnectionPriority        - server connection priority
* Output : storageInfo - initialized storage info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSCP_init(StorageInfo                *storageInfo,
                             const JobOptions           *jobOptions,
                             BandWidthList              *maxBandWidthList,
                             ServerConnectionPriorities serverConnectionPriority
                            )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);

  #ifdef HAVE_SSH2
    // init variables
    AutoFreeList autoFreeList;
    AutoFree_init(&autoFreeList);
    initBandWidthLimiter(&storageInfo->scp.bandWidthLimiter,maxBandWidthList);
    AUTOFREE_ADD(&autoFreeList,&storageInfo->scp.bandWidthLimiter,{ doneBandWidthLimiter(&storageInfo->scp.bandWidthLimiter); });

    // get SSH server settings
    SSHServer sshServer;
    storageInfo->scp.serverId = Configuration_initSSHServerSettings(&sshServer,storageInfo->storageSpecifier.hostName,jobOptions);
    AUTOFREE_ADD(&autoFreeList,&sshServer,{ Configuration_doneSSHServerSettings(&sshServer); });
    if (String_isEmpty(storageInfo->storageSpecifier.userName)) String_set(storageInfo->storageSpecifier.userName,sshServer.userName);
    if (String_isEmpty(storageInfo->storageSpecifier.userName)) String_setCString(storageInfo->storageSpecifier.userName,getenv("LOGNAME"));
    if (String_isEmpty(storageInfo->storageSpecifier.userName)) String_setCString(storageInfo->storageSpecifier.userName,getenv("USER"));
    if (storageInfo->storageSpecifier.hostPort == 0) storageInfo->storageSpecifier.hostPort = sshServer.port;
    if (Password_isEmpty(&storageInfo->storageSpecifier.password)) Password_set(&storageInfo->storageSpecifier.password,&sshServer.password);
    Configuration_duplicateKey(&storageInfo->scp.publicKey, &sshServer.publicKey );
    Configuration_duplicateKey(&storageInfo->scp.privateKey,&sshServer.privateKey);
    AUTOFREE_ADD(&autoFreeList,&storageInfo->scp.publicKey,{ Configuration_doneKey(&storageInfo->scp.publicKey); });
    AUTOFREE_ADD(&autoFreeList,&storageInfo->scp.privateKey,{ Configuration_doneKey(&storageInfo->scp.privateKey); });
    if (String_isEmpty(storageInfo->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }

    // allocate SSH server
    if (!allocateServer(storageInfo->scp.serverId,serverConnectionPriority,ALLOCATE_SERVER_TIMEOUT))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,&storageInfo->scp.serverId,{ freeServer(storageInfo->scp.serverId); });

    // check if SSH login, get correct password
    error = ERROR_SSH_AUTHENTICATION;
    if ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && !Password_isEmpty(&storageInfo->storageSpecifier.password))
    {
      error = checkSSHLogin(storageInfo->storageSpecifier.hostName,
                            storageInfo->storageSpecifier.hostPort,
                            storageInfo->storageSpecifier.userName,
                            &storageInfo->storageSpecifier.password,
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
                            storageInfo->storageSpecifier.userName,
                            &sshServer.password,
                            storageInfo->scp.publicKey.data,
                            storageInfo->scp.publicKey.length,
                            storageInfo->scp.privateKey.data,
                            storageInfo->scp.privateKey.length
                           );
      if (error == ERROR_NONE)
      {
        Password_set(&storageInfo->storageSpecifier.password,&sshServer.password);
      }
    }
    if ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && !Password_isEmpty(&sshServer.password))
    {
      error = checkSSHLogin(storageInfo->storageSpecifier.hostName,
                            storageInfo->storageSpecifier.hostPort,
                            storageInfo->storageSpecifier.userName,
                            &defaultSSHPassword,
                            storageInfo->scp.publicKey.data,
                            storageInfo->scp.publicKey.length,
                            storageInfo->scp.privateKey.data,
                            storageInfo->scp.privateKey.length
                           );
      if (error == ERROR_NONE)
      {
        Password_set(&storageInfo->storageSpecifier.password,&defaultSSHPassword);
      }
    }
    if (Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION)
    {
      // initialize interactive/default password
      uint     retries = 0;
      Password password;
      Password_init(&password);
      while ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && (retries < MAX_PASSWORD_REQUESTS))
      {
        if  (initSSHLogin(storageInfo->storageSpecifier.hostName,
                          storageInfo->storageSpecifier.userName,
                          &password,
                          jobOptions,
                          CALLBACK_(storageInfo->getNamePasswordFunction,storageInfo->getNamePasswordUserData)
                         )
           )
        {
          error = checkSSHLogin(storageInfo->storageSpecifier.hostName,
                                storageInfo->storageSpecifier.hostPort,
                                storageInfo->storageSpecifier.userName,
                                &password,
                                storageInfo->scp.publicKey.data,
                                storageInfo->scp.publicKey.length,
                                storageInfo->scp.privateKey.data,
                                storageInfo->scp.privateKey.length
                               );
          if (error == ERROR_NONE)
          {
            Password_set(&storageInfo->storageSpecifier.password,&password);
          }
        }
        retries++;
      }
      Password_done(&password);
    }
    if (Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION)
    {
      error = (   !Password_isEmpty(&storageInfo->storageSpecifier.password)
               || !Password_isEmpty(&sshServer.password)
               || !Password_isEmpty(&defaultSSHPassword)
              )
                ? ERRORX_(INVALID_SSH_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName))
                : ERRORX_(NO_SSH_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName));
    }

    // store password as default password
    if (error == ERROR_NONE)
    {
      Password_set(&defaultSSHPassword,&storageInfo->storageSpecifier.password);
    }
    assert(error != ERROR_UNKNOWN);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // free resources
    Configuration_doneSSHServerSettings(&sshServer);
    AutoFree_done(&autoFreeList);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(maxBandWidthList);
    UNUSED_VARIABLE(serverConnectionPriority);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return error;
}

LOCAL Errors StorageSCP_done(StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);

  // free SSH server connection
  #ifdef HAVE_SSH2
    Configuration_doneKey(&storageInfo->scp.privateKey);
    Configuration_doneKey(&storageInfo->scp.publicKey);
    freeServer(storageInfo->scp.serverId);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
  #endif /* HAVE_SSH2 */

  return ERROR_NONE;
}

LOCAL bool StorageSCP_isServerAllocationPending(const StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);

  bool serverAllocationPending = FALSE;
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
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);

  Errors error = ERROR_NONE;

  #ifdef HAVE_SSH2
    if (!initialFlag)
    {
      // init variables
      String directory = String_new();

      // init macros
      TextMacros (textMacros,3);
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_STRING("%directory",File_getDirectoryName(directory,archiveName),NULL);
        TEXT_MACRO_X_STRING("%file",     archiveName,                                 NULL);
        TEXT_MACRO_X_INT   ("%number",   storageInfo->volumeNumber,                   NULL);
      }

      if (!String_isEmpty(globalOptions.scp.writePreProcessCommand))
      {
        printInfo(1,"Write pre-processing...");
        error = executeTemplate(String_cString(globalOptions.scp.writePreProcessCommand),
                                timestamp,
                                textMacros.data,
                                textMacros.count,
                                CALLBACK_(executeIOOutput,NULL),
                                globalOptions.commandTimeout
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }

      // free resources
      String_delete(directory);
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
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);

  Errors error = ERROR_NONE;

  #ifdef HAVE_SSH2
    if (!finalFlag)
    {
      // init variables
      String directory = String_new();

      // init macros
      TextMacros (textMacros,3);
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_STRING("%directory",File_getDirectoryName(directory,archiveName),NULL);
        TEXT_MACRO_X_STRING("%file",     archiveName,                                 NULL);
        TEXT_MACRO_X_INT   ("%number",   storageInfo->volumeNumber,                   NULL);
      }

      if (!String_isEmpty(globalOptions.scp.writePostProcessCommand))
      {
        printInfo(1,"Write post-processing...");
        error = executeTemplate(String_cString(globalOptions.scp.writePostProcessCommand),
                                timestamp,
                                textMacros.data,
                                textMacros.count,
                                CALLBACK_(executeIOOutput,NULL),
                                globalOptions.commandTimeout
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }

      // free resources
      String_delete(directory);
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

LOCAL bool StorageSCP_exists(StorageInfo *storageInfo,
                             ConstString archiveName
                            )
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(!String_isEmpty(archiveName));

  bool existsFlag = FALSE;

  #ifdef HAVE_SSH2
    // connect
    SocketHandle socketHandle;
    Errors error = Network_connect(&socketHandle,
                                   SOCKET_TYPE_SSH,
                                   storageInfo->storageSpecifier.hostName,
                                   storageInfo->storageSpecifier.hostPort,
                                   storageInfo->storageSpecifier.userName,
                                   &storageInfo->storageSpecifier.password,
                                   NULL,  // caData
                                   0,     // caLength
                                   NULL,  // certData
                                   0,     // certLength
                                   storageInfo->scp.publicKey.data,
                                   storageInfo->scp.publicKey.length,
                                   storageInfo->scp.privateKey.data,
                                   storageInfo->scp.privateKey.length,
                                     SOCKET_FLAG_NONE
                                   | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                                   | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                                   30*MS_PER_SECOND
                                  );
    if (error != ERROR_NONE)
    {
      return FALSE;
    }
    libssh2_session_set_timeout(Network_getSSHSession(&socketHandle),READ_TIMEOUT);

    // check if file can be read
    struct stat     fileInfo;
    LIBSSH2_CHANNEL *channel = libssh2_scp_recv2(Network_getSSHSession(&socketHandle),
                                                 String_cString(archiveName),
                                                 &fileInfo
                                                );
    if (channel != NULL)
    {
      existsFlag = TRUE;
      (void)libssh2_channel_close(channel);
      (void)libssh2_channel_wait_closed(channel);
      (void)libssh2_channel_free(channel);
    }

    // disconnect
    Network_disconnect(&socketHandle);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
  #endif /* HAVE_SSH2 */

  return existsFlag;
}

LOCAL bool StorageSCP_isFile(StorageInfo *storageInfo,
                             ConstString archiveName
                            )
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(!String_isEmpty(archiveName));

  bool result = FALSE;

  #ifdef HAVE_SSH2
    // connect
    SocketHandle socketHandle;
    Errors error = Network_connect(&socketHandle,
                                   SOCKET_TYPE_SSH,
                                   storageInfo->storageSpecifier.hostName,
                                   storageInfo->storageSpecifier.hostPort,
                                   storageInfo->storageSpecifier.userName,
                                   &storageInfo->storageSpecifier.password,
                                   NULL,  // caData
                                   0,     // caLength
                                   NULL,  // certData
                                   0,     // certLength
                                   storageInfo->scp.publicKey.data,
                                   storageInfo->scp.publicKey.length,
                                   storageInfo->scp.privateKey.data,
                                   storageInfo->scp.privateKey.length,
                                     SOCKET_FLAG_NONE
                                   | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                                   | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                                   30*MS_PER_SECOND
                                  );
    if (error != ERROR_NONE)
    {
      return FALSE;
    }
    libssh2_session_set_timeout(Network_getSSHSession(&socketHandle),READ_TIMEOUT);

    // check if file can be read
    struct stat     fileInfo;
    LIBSSH2_CHANNEL *channel = libssh2_scp_recv2(Network_getSSHSession(&socketHandle),
                                                 String_cString(archiveName),
                                                 &fileInfo
                                                );
    if ((channel != NULL) && S_ISREG(fileInfo.st_mode))
    {
      result = TRUE;
      (void)libssh2_channel_close(channel);
      (void)libssh2_channel_wait_closed(channel);
      (void)libssh2_channel_free(channel);
    }

    // disconnect
    Network_disconnect(&socketHandle);

    return ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return result;
}

LOCAL bool StorageSCP_isDirectory(StorageInfo *storageInfo,
                                  ConstString archiveName
                                 )
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(!String_isEmpty(archiveName));

  bool result = FALSE;

  #ifdef HAVE_SSH2
    // connect
    SocketHandle socketHandle;
    Errors error = Network_connect(&socketHandle,
                                   SOCKET_TYPE_SSH,
                                   storageInfo->storageSpecifier.hostName,
                                   storageInfo->storageSpecifier.hostPort,
                                   storageInfo->storageSpecifier.userName,
                                   &storageInfo->storageSpecifier.password,
                                   NULL,  // caData
                                   0,     // caLength
                                   NULL,  // certData
                                   0,     // certLength
                                   storageInfo->scp.publicKey.data,
                                   storageInfo->scp.publicKey.length,
                                   storageInfo->scp.privateKey.data,
                                   storageInfo->scp.privateKey.length,
                                     SOCKET_FLAG_NONE
                                   | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                                   | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                                   30*MS_PER_SECOND
                                  );
    if (error != ERROR_NONE)
    {
      return FALSE;
    }
    libssh2_session_set_timeout(Network_getSSHSession(&socketHandle),READ_TIMEOUT);

    // check if file can be read
    struct stat     fileInfo;
    LIBSSH2_CHANNEL *channel = libssh2_scp_recv2(Network_getSSHSession(&socketHandle),
                                                 String_cString(archiveName),
                                                 &fileInfo
                                                );
    if ((channel != NULL) && S_ISDIR(fileInfo.st_mode))
    {
      result = TRUE;
      (void)libssh2_channel_close(channel);
      (void)libssh2_channel_wait_closed(channel);
      (void)libssh2_channel_free(channel);
    }

    // disconnect
    Network_disconnect(&socketHandle);

    return ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return result;
}

LOCAL bool StorageSCP_isReadable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  return FALSE;
}

LOCAL bool StorageSCP_isWritable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  return FALSE;
}

LOCAL Errors StorageSCP_getTmpName(String archiveName, const StorageInfo *storageInfo)
{
  assert(archiveName != NULL);
  assert(!String_isEmpty(archiveName));
  assert(storageInfo != NULL);

  UNUSED_VARIABLE(archiveName);
  UNUSED_VARIABLE(storageInfo);

//TODO
  return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL Errors StorageSCP_create(StorageHandle *storageHandle,
                               ConstString   fileName,
                               uint64        fileSize,
                               bool          forceFlag
                              )
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(!String_isEmpty(fileName));

  UNUSED_VARIABLE(fileSize);

  // check if file exists
  if (   !forceFlag
      && (storageHandle->storageInfo->jobOptions != NULL)
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
    storageHandle->scp.sftp                   = NULL;
    storageHandle->scp.sftpHandle             = NULL;
    storageHandle->scp.index                  = 0LL;
    storageHandle->scp.size                   = 0LL;
    storageHandle->scp.readAheadBuffer.offset = 0LL;
    storageHandle->scp.readAheadBuffer.length = 0L;

    Errors error;

    // connect
    error = Network_connect(&storageHandle->scp.socketHandle,
                            SOCKET_TYPE_SSH,
                            storageHandle->storageInfo->storageSpecifier.hostName,
                            storageHandle->storageInfo->storageSpecifier.hostPort,
                            storageHandle->storageInfo->storageSpecifier.userName,
                            &storageHandle->storageInfo->storageSpecifier.password,
                            NULL,  // caData
                            0,     // caLength
                            NULL,  // certData
                            0,     // certLength
                            storageHandle->storageInfo->scp.publicKey.data,
                            storageHandle->storageInfo->scp.publicKey.length,
                            storageHandle->storageInfo->scp.privateKey.data,
                            storageHandle->storageInfo->scp.privateKey.length,
                              SOCKET_FLAG_NONE
                            | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                            | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                            30*MS_PER_SECOND
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }
    libssh2_session_set_timeout(Network_getSSHSession(&storageHandle->scp.socketHandle),READ_TIMEOUT);

    // install send/receive callback to track number of sent/received bytes
    (*(libssh2_session_abstract(Network_getSSHSession(&storageHandle->scp.socketHandle)))) = storageHandle;
    storageHandle->scp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,scpSendCallback   );
    storageHandle->scp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,scpReceiveCallback);

    // try to open file first with sftp-protocol, then scp-protocol
    error = ERROR_UNKNOWN;

    if (error != ERROR_NONE)
    {
      // init SFTP session
      int ssh2ErrorCode = 0;
      do
      {
        storageHandle->scp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageHandle->scp.socketHandle));
        if (storageHandle->scp.sftp == NULL)
        {
          ssh2ErrorCode = libssh2_session_last_errno(Network_getSSHSession(&storageHandle->scp.socketHandle));
          if (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
        }
      }
      while ((storageHandle->scp.sftp == NULL) && (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN));
      if (storageHandle->scp.sftp == NULL)
      {
        char *ssh2ErrorText;
        libssh2_session_last_error(Network_getSSHSession(&storageHandle->scp.socketHandle),&ssh2ErrorText,NULL,0);
        error = ERRORX_(SSH,
                        libssh2_session_last_errno(Network_getSSHSession(&storageHandle->scp.socketHandle)),
                        "%s",
                        ssh2ErrorText
                       );
        Network_disconnect(&storageHandle->scp.socketHandle);
        return error;
      }

      // create directories if not existing
      String directoryName = File_getDirectoryName(String_new(),fileName);
      String name = String_new();
      error = ERROR_NONE;
      FILE_PATH_ITERATEX(directoryName,name,TRUE,error == ERROR_NONE)
      {
        LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;
        if (libssh2_sftp_lstat(storageHandle->scp.sftp,
                               String_cString(name),
                               &sftpAttributes
                              ) == 0
           )
        {
          // check if directory
          if (!LIBSSH2_SFTP_S_ISDIR(sftpAttributes.permissions))
          {
            error = ERRORX_(NOT_A_DIRECTORY,0,"%s",String_cString(name));
          }
        }
        else
        {
          // create directory
          if (libssh2_sftp_mkdir(storageHandle->scp.sftp,
                                 String_cString(name),
                                 sftpGetPermissions(File_getDefaultDirectoryPermissions())
                                ) != 0
             )
          {
            char *ssh2ErrorText;
            libssh2_session_last_error(Network_getSSHSession(&storageHandle->scp.socketHandle),&ssh2ErrorText,NULL,0);
            error = ERRORX_(SSH,
                            libssh2_session_last_errno(Network_getSSHSession(&storageHandle->scp.socketHandle)),
                            "create '%s' fail: %s",
                            String_cString(name),
                            ssh2ErrorText
                           );
          }
        }
      }
      String_delete(name);
      String_delete(directoryName);
      if (error != ERROR_NONE)
      {
        (void)libssh2_sftp_shutdown(storageHandle->scp.sftp);
        Network_disconnect(&storageHandle->scp.socketHandle);
        return error;
      }

      // create file
      storageHandle->scp.sftpHandle = libssh2_sftp_open(storageHandle->scp.sftp,
                                                        String_cString(fileName),
                                                        (storageHandle->storageInfo->jobOptions->archiveFileMode == ARCHIVE_FILE_MODE_APPEND)
                                                          ? LIBSSH2_FXF_CREAT|LIBSSH2_FXF_WRITE|LIBSSH2_FXF_APPEND
                                                          : LIBSSH2_FXF_CREAT|LIBSSH2_FXF_WRITE|LIBSSH2_FXF_TRUNC,
                                                        sftpGetPermissions(File_getDefaultFilePermissions())
                                                       );
      if (storageHandle->scp.sftpHandle == NULL)
      {
        char *ssh2ErrorText;
        libssh2_session_last_error(Network_getSSHSession(&storageHandle->scp.socketHandle),&ssh2ErrorText,NULL,0);
        error = ERRORX_(SSH,
                        libssh2_session_last_errno(Network_getSSHSession(&storageHandle->scp.socketHandle)),
                        "%s",
                        ssh2ErrorText
                       );
        (void)libssh2_sftp_shutdown(storageHandle->scp.sftp);
        Network_disconnect(&storageHandle->scp.socketHandle);
        return error;
      }
    }

    if (error != ERROR_NONE)
    {
      // open channel and file for writing
      #ifdef HAVE_SSH2_SCP_SEND64
        storageHandle->scp.channel = libssh2_scp_send64(Network_getSSHSession(&storageHandle->scp.socketHandle),
                                                        String_cString(fileName),
                                                        (int)File_getDefaultFilePermissions(),
                                                        (libssh2_uint64_t)fileSize,
                                                        0L,  // mtime: use remote date/time
                                                        0L  // atime: use remote date/time
                                                       );
      #else /* not HAVE_SSH2_SCP_SEND64 */
        storageHandle->scp.channel = libssh2_scp_send(Network_getSSHSession(&storageHandle->scp.socketHandle),
                                                      String_cString(fileName),
                                                      (int)File_getDefaultFilePermissions(),
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
    }

    assert(error != ERROR_UNKNOWN);

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
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(!String_isEmpty(archiveName));

  #ifdef HAVE_SSH2
    // init variables
    storageHandle->scp.archiveName            = String_duplicate(archiveName);
    storageHandle->scp.oldSendCallback        = NULL;
    storageHandle->scp.oldReceiveCallback     = NULL;
    storageHandle->scp.totalSentBytes         = 0LL;
    storageHandle->scp.totalReceivedBytes     = 0LL;
    storageHandle->scp.channel                = NULL;
    storageHandle->scp.sftp                   = NULL;
    storageHandle->scp.sftpHandle             = NULL;
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

    Errors error;

    // connect
    error = Network_connect(&storageHandle->scp.socketHandle,
                            SOCKET_TYPE_SSH,
                            storageHandle->storageInfo->storageSpecifier.hostName,
                            storageHandle->storageInfo->storageSpecifier.hostPort,
                            storageHandle->storageInfo->storageSpecifier.userName,
                            &storageHandle->storageInfo->storageSpecifier.password,
                            NULL,  // caData
                            0,     // caLength
                            NULL,  // certData
                            0,     // certLength
                            storageHandle->storageInfo->scp.publicKey.data,
                            storageHandle->storageInfo->scp.publicKey.length,
                            storageHandle->storageInfo->scp.privateKey.data,
                            storageHandle->storageInfo->scp.privateKey.length,
                              SOCKET_FLAG_NONE
                            | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                            | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                            30*MS_PER_SECOND
                           );
    if (error != ERROR_NONE)
    {
      free(storageHandle->scp.readAheadBuffer.data);
      String_delete(storageHandle->scp.archiveName);
      return error;
    }
    libssh2_session_set_timeout(Network_getSSHSession(&storageHandle->scp.socketHandle),READ_TIMEOUT);

    // install send/receive callback to track number of sent/received bytes
    (*(libssh2_session_abstract(Network_getSSHSession(&storageHandle->scp.socketHandle)))) = storageHandle;
    storageHandle->scp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,scpSendCallback   );
    storageHandle->scp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,scpReceiveCallback);

    // try to open file first with sftp-protocol, then scp-protocol
    error = ERROR_UNKNOWN;

    if (error != ERROR_NONE)
    {
      int  ssh2ErrorCode = 0;
      char *ssh2ErrorText;

      // init SFTP session
      if (ssh2ErrorCode == 0)
      {
        do
        {
          storageHandle->scp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageHandle->scp.socketHandle));
          if (storageHandle->scp.sftp == NULL)
          {
            ssh2ErrorCode = libssh2_session_last_errno(Network_getSSHSession(&storageHandle->scp.socketHandle));
            if (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
          }
        }
        while ((storageHandle->scp.sftp == NULL) && (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN));
      }

      // open file
      if (ssh2ErrorCode == 0)
      {
        storageHandle->scp.sftpHandle = libssh2_sftp_open(storageHandle->scp.sftp,
                                                          String_cString(archiveName),
                                                          LIBSSH2_FXF_READ,
                                                          0
                                                         );
        if (storageHandle->scp.sftpHandle == NULL)
        {
          ssh2ErrorCode = libssh2_session_last_error(Network_getSSHSession(&storageHandle->scp.socketHandle),&ssh2ErrorText,NULL,0);
          (void)libssh2_sftp_shutdown(storageHandle->scp.sftp);
        }
      }

      // get file size
      if (ssh2ErrorCode == 0)
      {
        LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;
        if (libssh2_sftp_fstat(storageHandle->scp.sftpHandle,
                               &sftpAttributes
                              ) == 0
           )
        {
          storageHandle->scp.size = sftpAttributes.filesize;
        }
        else
        {
          ssh2ErrorCode = libssh2_session_last_error(Network_getSSHSession(&storageHandle->scp.socketHandle),&ssh2ErrorText,NULL,0);
          (void)libssh2_sftp_close(storageHandle->scp.sftpHandle);
          (void)libssh2_sftp_shutdown(storageHandle->scp.sftp);
        }
      }

      if (ssh2ErrorCode == 0)
      {
        error = ERROR_NONE;
      }
      else
      {
        error = ERRORX_(SSH,
                        ssh2ErrorCode,
                        "%s",
                        ssh2ErrorText
                       );
      }
    }

    if (error != ERROR_NONE)
    {
      int  ssh2ErrorCode = 0;
      char *ssh2ErrorText;

      // open channel and file for reading, get file size
      struct stat fileInfo;
      storageHandle->scp.channel = libssh2_scp_recv2(Network_getSSHSession(&storageHandle->scp.socketHandle),
                                                     String_cString(archiveName),
                                                     &fileInfo
                                                    );
      if (storageHandle->scp.channel != NULL)
      {
        storageHandle->scp.size = (uint64)fileInfo.st_size;
      }
      else
      {
        ssh2ErrorCode = libssh2_session_last_error(Network_getSSHSession(&storageHandle->scp.socketHandle),&ssh2ErrorText,NULL,0);
      }

      if (ssh2ErrorCode == 0)
      {
        error = ERROR_NONE;
      }
      else
      {
        error = ERRORX_(SSH,
                        ssh2ErrorCode,
                        "%s",
                        ssh2ErrorText
                       );
      }
    }
    assert(error != ERROR_UNKNOWN);

    if (error != ERROR_NONE)
    {
      libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,storageHandle->scp.oldReceiveCallback);
      libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,storageHandle->scp.oldSendCallback);
      Network_disconnect(&storageHandle->scp.socketHandle);
      free(storageHandle->scp.readAheadBuffer.data);
      String_delete(storageHandle->scp.archiveName);
      return error;
    }

    return ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(archiveName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL void StorageSCP_close(StorageHandle *storageHandle)
{
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert((storageHandle->scp.sftpHandle != NULL) || (storageHandle->scp.channel != NULL));

  #ifdef HAVE_SSH2
    switch (storageHandle->mode)
    {
      case STORAGE_MODE_READ:
        if (storageHandle->scp.sftpHandle != NULL)
        {
          (void)libssh2_sftp_close(storageHandle->scp.sftpHandle);
          (void)libssh2_sftp_shutdown(storageHandle->scp.sftp);
        }
        else
        {
          (void)libssh2_channel_close(storageHandle->scp.channel);
          (void)libssh2_channel_wait_closed(storageHandle->scp.channel);
          (void)libssh2_channel_free(storageHandle->scp.channel);
        }
        free(storageHandle->scp.readAheadBuffer.data);
        break;
      case STORAGE_MODE_WRITE:
        if (storageHandle->scp.sftpHandle != NULL)
        {
          (void)libssh2_sftp_close(storageHandle->scp.sftpHandle);
          (void)libssh2_sftp_shutdown(storageHandle->scp.sftp);
        }
        else
        {
          int result = 0;

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
        }
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
    libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,storageHandle->scp.oldReceiveCallback);
    libssh2_session_callback_set(Network_getSSHSession(&storageHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,storageHandle->scp.oldSendCallback);
    Network_disconnect(&storageHandle->scp.socketHandle);
    String_delete(storageHandle->scp.archiveName);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_SSH2 */
}

LOCAL bool StorageSCP_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);

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
                             ulong         *readBytes
                            )
{
  assert(storageHandle != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(buffer != NULL);

  if (readBytes != NULL) (*readBytes) = 0L;

  #ifdef HAVE_SSH2
    assert(storageHandle->scp.readAheadBuffer.data != NULL);

    Errors error = ERROR_NONE;

    // try sftp seek if possible, then scp functionality
    if (storageHandle->scp.sftp != NULL)
    {
      while (bufferSize > 0)
      {
        // copy as much data as available from read-ahead buffer
        if (   (storageHandle->scp.index >= storageHandle->scp.readAheadBuffer.offset)
            && (storageHandle->scp.index < (storageHandle->scp.readAheadBuffer.offset+storageHandle->scp.readAheadBuffer.length))
           )
        {
          // copy data from read-ahead buffer
          ulong index      = (ulong)(storageHandle->scp.index-storageHandle->scp.readAheadBuffer.offset);
          ulong bytesAvail = MIN(bufferSize,storageHandle->scp.readAheadBuffer.length-index);
          memCopyFast(buffer,bytesAvail,storageHandle->scp.readAheadBuffer.data+index,bytesAvail);

          // adjust buffer, bufferSize, bytes read, index
          buffer = (byte*)buffer+bytesAvail;
          bufferSize -= bytesAvail;
          if (readBytes != NULL) (*readBytes) += bytesAvail;
          storageHandle->scp.index += (uint64)bytesAvail;
        }

        // read rest of data
        if (bufferSize > 0)
        {
          assert(storageHandle->scp.index >= (storageHandle->scp.readAheadBuffer.offset+storageHandle->scp.readAheadBuffer.length));

          // get max. number of bytes to receive in one step
          ulong length;
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
          uint64 startTimestamp          = Misc_getTimestamp();
          uint64 startTotalReceivedBytes = storageHandle->scp.totalReceivedBytes;

          #if   defined(HAVE_SSH2_SFTP_SEEK64)
            libssh2_sftp_seek64(storageHandle->scp.sftpHandle,storageHandle->scp.index);
          #elif defined(HAVE_SSH2_SFTP_SEEK2)
            libssh2_sftp_seek2(storageHandle->scp.sftpHandle,storageHandle->scp.index);
          #else /* not HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
            libssh2_sftp_seek(storageHandle->scp.sftpHandle,storageHandle->scp.index);
          #endif /* HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */

          if (length <= MAX_BUFFER_SIZE)
          {
            // read into read-ahead buffer
            ulong bytesAvail;
            error = sftpRead(&storageHandle->scp.socketHandle,
                             storageHandle->scp.sftp,
                             storageHandle->scp.sftpHandle,
                             storageHandle->scp.readAheadBuffer.data,
                             MIN((size_t)(storageHandle->scp.size-storageHandle->scp.index),MAX_BUFFER_SIZE),
                             &bytesAvail
                            );
            if (error != ERROR_NONE)
            {
              break;
            }
            storageHandle->scp.readAheadBuffer.offset = storageHandle->scp.index;
            storageHandle->scp.readAheadBuffer.length = bytesAvail;

            // copy data from read-ahead buffer
            bytesAvail = MIN(length,storageHandle->scp.readAheadBuffer.length);
            memcpy(buffer,storageHandle->scp.readAheadBuffer.data,bytesAvail);

            // adjust buffer, bufferSize, bytes read, index
            buffer = (byte*)buffer+bytesAvail;
            bufferSize -= bytesAvail;
            if (readBytes != NULL) (*readBytes) += bytesAvail;
            storageHandle->scp.index += (uint64)bytesAvail;
          }
          else
          {
            // read direct
            ulong bytesAvail;
            error = sftpRead(&storageHandle->scp.socketHandle,
                             storageHandle->scp.sftp,
                             storageHandle->scp.sftpHandle,
                             buffer,
                             length,
                             &bytesAvail
                            );
            if (error != ERROR_NONE)
            {
              break;
            }

            // adjust buffer, bufferSize, bytes read, index
            buffer = (byte*)buffer+(ulong)bytesAvail;
            bufferSize -= (ulong)bytesAvail;
            if (readBytes != NULL) (*readBytes) += (ulong)bytesAvail;
            storageHandle->scp.index += (uint64)bytesAvail;
          }

          // get end time, end received bytes
          uint64 endTimestamp          = Misc_getTimestamp();
          uint64 endTotalReceivedBytes = storageHandle->scp.totalReceivedBytes;
          assert(endTotalReceivedBytes >= startTotalReceivedBytes);

          /* limit used band width if requested (note: when the system time is
             changing endTimestamp may become smaller than startTimestamp;
             thus do not check this with an assert())
          */
          if (endTimestamp >= startTimestamp)
          {
            SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
            {
              limitBandWidth(&storageHandle->storageInfo->scp.bandWidthLimiter,
                             endTotalReceivedBytes-startTotalReceivedBytes,
                             endTimestamp-startTimestamp
                            );
            }
          }
        }
      }
    }
    else
    {
      assert(storageHandle->scp.channel != NULL);

      while (bufferSize > 0L)
      {
        // copy as much data as available from read-ahead buffer
        if (   (storageHandle->scp.index >= storageHandle->scp.readAheadBuffer.offset)
            && (storageHandle->scp.index < (storageHandle->scp.readAheadBuffer.offset+storageHandle->scp.readAheadBuffer.length))
           )
        {
          // copy data from read-ahead buffer
          ulong index      = (ulong)(storageHandle->scp.index-storageHandle->scp.readAheadBuffer.offset);
          ulong bytesAvail = MIN(bufferSize,storageHandle->scp.readAheadBuffer.length-index);
          memcpy(buffer,storageHandle->scp.readAheadBuffer.data+index,bytesAvail);

          // adjust buffer, bufferSize, bytes read, index
          buffer = (byte*)buffer+bytesAvail;
          bufferSize -= bytesAvail;
          if (readBytes != NULL) (*readBytes) += bytesAvail;
          storageHandle->scp.index += (uint64)bytesAvail;
        }

        // read rest of data
        if (bufferSize > 0)
        {
          assert(storageHandle->scp.index >= (storageHandle->scp.readAheadBuffer.offset+storageHandle->scp.readAheadBuffer.length));

          // get max. number of bytes to receive in one step
          ulong length = MIN((size_t)(storageHandle->scp.size-storageHandle->scp.index),bufferSize);
          if (storageHandle->storageInfo->scp.bandWidthLimiter.maxBandWidthList != NULL)
          {
            length = MIN(length,
                         storageHandle->storageInfo->scp.bandWidthLimiter.blockSize
                        );
          }
          assert(length > 0L);

          // get start time, start received bytes
          uint64 startTimestamp          = Misc_getTimestamp();
          uint64 startTotalReceivedBytes = storageHandle->scp.totalReceivedBytes;

          if (length < MAX_BUFFER_SIZE)
          {
            // read into read-ahead buffer
            ssize_t n;
            do
            {
              n = libssh2_channel_read(storageHandle->scp.channel,
                                       (char*)storageHandle->scp.readAheadBuffer.data,
                                       length
                                      );
              if (n == LIBSSH2_ERROR_EAGAIN)
              {
                Misc_mdelay(100);
              }
            }
            while (n == LIBSSH2_ERROR_EAGAIN);
            if (n < 0)
            {
              error = ERROR_(IO,errno);
              break;
            }
            storageHandle->scp.readAheadBuffer.offset = storageHandle->scp.index;
            storageHandle->scp.readAheadBuffer.length = (ulong)n;
//fprintf(stderr,"%s,%d: n=%ld storageHandle->scp.bufferOffset=%"PRIu64" storageHandle->scp.bufferLength=%lu\n",__FILE__,__LINE__,n,storageHandle->scp.readAheadBuffer.offset,storageHandle->scp.readAheadBuffer.length);

            // copy data from read-ahead buffer
            ulong bytesAvail = MIN(length,storageHandle->scp.readAheadBuffer.length);
            memcpy(buffer,storageHandle->scp.readAheadBuffer.data,bytesAvail);

            // adjust buffer, bufferSize, bytes read, index
            buffer = (byte*)buffer+bytesAvail;
            bufferSize -= bytesAvail;
            if (readBytes != NULL) (*readBytes) += bytesAvail;
            storageHandle->scp.index += (uint64)bytesAvail;
          }
          else
          {
            // read direct
            ssize_t n;
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
            if (readBytes != NULL) (*readBytes) += (ulong)n;
            storageHandle->scp.index += (uint64)n;
          }

          // get end time, end received bytes
          uint64 endTimestamp          = Misc_getTimestamp();
          uint64 endTotalReceivedBytes = storageHandle->scp.totalReceivedBytes;
          assert(endTotalReceivedBytes >= startTotalReceivedBytes);

          /* limit used band width if requested (note: when the system time is
             changing endTimestamp may become smaller than startTimestamp;
             thus do not check this with an assert())
          */
          if (endTimestamp >= startTimestamp)
          {
            SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
            {
              limitBandWidth(&storageHandle->storageInfo->scp.bandWidthLimiter,
                             endTotalReceivedBytes-startTotalReceivedBytes,
                             endTimestamp-startTimestamp
                            );
            }
          }
        }
      }
    }

    return error;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferSize);
    UNUSED_VARIABLE(readBytes);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL Errors StorageSCP_write(StorageHandle *storageHandle,
                              const void    *buffer,
                              ulong         bufferLength
                             )
{
  assert(storageHandle != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(buffer != NULL);

  #ifdef HAVE_SSH2
    Errors error = ERROR_NONE;

    // try sftp seek if possible, then scp functionality
    ulong writtenBytes = 0L;
    if (storageHandle->scp.sftp != NULL)
    {
      while (writtenBytes < bufferLength)
      {
        // get max. number of bytes to send in one step
        ulong length;
        if (storageHandle->storageInfo->scp.bandWidthLimiter.maxBandWidthList != NULL)
        {
          length = MIN(storageHandle->storageInfo->scp.bandWidthLimiter.blockSize,bufferLength-writtenBytes);
        }
        else
        {
          length = bufferLength-writtenBytes;
        }
        assert(length > 0L);

        // get start time, start received bytes
        uint64 startTimestamp      = Misc_getTimestamp();
        uint64 startTotalSentBytes = storageHandle->scp.totalSentBytes;

        // send data
        ulong n;
        error = sftpWrite(&storageHandle->scp.socketHandle,
                          storageHandle->scp.sftp,
                          storageHandle->scp.sftpHandle,
                          buffer,
                          length,
                          &n
                         );
        if (error != ERROR_NONE)
        {
          break;
        }
        buffer = (byte*)buffer+n;
        writtenBytes += n;

        // get end time, end received bytes
        uint64 endTimestamp      = Misc_getTimestamp();
        uint64 endTotalSentBytes = storageHandle->scp.totalSentBytes;
        assert(endTotalSentBytes >= startTotalSentBytes);

        /* limit used band width if requested (note: when the system time is
           changing endTimestamp may become smaller than startTimestamp;
           thus do not check this with an assert())
        */
        if (endTimestamp >= startTimestamp)
        {
          SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
          {
            limitBandWidth(&storageHandle->storageInfo->scp.bandWidthLimiter,
                           endTotalSentBytes-startTotalSentBytes,
                           endTimestamp-startTimestamp
                          );
          }
        }
      }
    }
    else
    {
      assert(storageHandle->scp.channel != NULL);

      while (writtenBytes < bufferLength)
      {
        // get max. number of bytes to send in one step
        ulong length;
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
        uint64 startTimestamp      = Misc_getTimestamp();
        uint64 startTotalSentBytes = storageHandle->scp.totalSentBytes;

        // send data
        ssize_t n;
        ssize_t retryCount = 5;
        do
        {
          n = libssh2_channel_write(storageHandle->scp.channel,
                                    buffer,
                                    length
                                   );
          if (n == LIBSSH2_ERROR_EAGAIN)
          {
            retryCount--;
            if (retryCount >= 0)
            {
              Misc_mdelay(100);
            }
          }
        }
        while ((n == LIBSSH2_ERROR_EAGAIN) && (retryCount >= 0));

        // get end time, end received bytes
        uint64 endTimestamp      = Misc_getTimestamp();
        uint64 endTotalSentBytes = storageHandle->scp.totalSentBytes;
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
          SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
          {
            limitBandWidth(&storageHandle->storageInfo->scp.bandWidthLimiter,
                           endTotalSentBytes-startTotalSentBytes,
                           endTimestamp-startTimestamp
                          );
          }
        }
      }
    }
    storageHandle->scp.size += writtenBytes;

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
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);

  uint64 size = 0LL;
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
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(offset != NULL);

  (*offset) = 0LL;

  Errors error = ERROR_NONE;
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
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);

  #ifdef HAVE_SSH2
    assert(storageHandle->scp.readAheadBuffer.data != NULL);

    Errors error = ERROR_NONE;

    // try sftp seek if possible, then scp functionality
    if (storageHandle->scp.sftp != NULL)
    {
      if      (offset > storageHandle->scp.index)
      {
        uint64 skip = offset-storageHandle->scp.index;
        if (skip > 0LL)
        {
          // skip data in read-ahead buffer
          if (   (storageHandle->scp.index >= storageHandle->scp.readAheadBuffer.offset)
              && (storageHandle->scp.index < (storageHandle->scp.readAheadBuffer.offset+storageHandle->scp.readAheadBuffer.length))
             )
          {
            uint64 i = storageHandle->scp.index-storageHandle->scp.readAheadBuffer.offset;
            uint64 n = MIN(skip,storageHandle->scp.readAheadBuffer.length-i);
            skip -= n;
            storageHandle->scp.index += (uint64)n;
          }

          if (skip > 0LL)
          {
            #if   defined(HAVE_SSH2_SFTP_SEEK64)
              libssh2_sftp_seek64(storageHandle->scp.sftpHandle,offset);
            #elif defined(HAVE_SSH2_SFTP_SEEK2)
              libssh2_sftp_seek2(storageHandle->scp.sftpHandle,offset);
            #else /* not HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
              libssh2_sftp_seek(storageHandle->scp.sftpHandle,(size_t)offset);
            #endif /* HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
            storageHandle->scp.readAheadBuffer.offset = offset;
            storageHandle->scp.readAheadBuffer.length = 0L;

            storageHandle->scp.index = offset;
          }
        }
      }
      else if (offset < storageHandle->scp.index)
      {
        uint64 skip = storageHandle->scp.index-offset;
        if (skip > 0LL)
        {
          // skip data in read-ahead buffer
          if (   (storageHandle->scp.index >= storageHandle->scp.readAheadBuffer.offset)
              && (storageHandle->scp.index < (storageHandle->scp.readAheadBuffer.offset+storageHandle->scp.readAheadBuffer.length))
             )
          {
            uint64 i = storageHandle->scp.index-storageHandle->scp.readAheadBuffer.offset;
            uint64 n = MIN(skip,i);
            skip -= n;
            storageHandle->scp.index -= (uint64)n;
          }

          if (skip > 0LL)
          {
            #if   defined(HAVE_SSH2_SFTP_SEEK64)
              libssh2_sftp_seek64(storageHandle->scp.sftpHandle,offset);
            #elif defined(HAVE_SSH2_SFTP_SEEK2)
              libssh2_sftp_seek2(storageHandle->scp.sftpHandle,offset);
            #else /* not HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
              libssh2_sftp_seek(storageHandle->scp.sftpHandle,(size_t)offset);
            #endif /* HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
            storageHandle->scp.readAheadBuffer.offset = offset;
            storageHandle->scp.readAheadBuffer.length = 0L;

            storageHandle->scp.index = offset;
          }
        }
      }
    }
    else
    {
      /* scp protocol does not support a seek-function. Thus try to
         read and discard data to position the read index to the
         requested offset.
         Note: this is slow!
      */

      assert(storageHandle->scp.channel != NULL);

      error = ERROR_NONE;

      /* Note: a seek backward is only possible by reseting the channel
               and read the file from the beginning to the required
               index position.
      */
      if (offset < storageHandle->scp.index)
      {
        // close channel
        if (storageHandle->scp.channel != NULL)
        {
          (void)libssh2_channel_close(storageHandle->scp.channel);
          (void)libssh2_channel_wait_closed(storageHandle->scp.channel);
          (void)libssh2_channel_free(storageHandle->scp.channel);
        }

        // reopen channel
        storageHandle->scp.channel = libssh2_scp_recv2(Network_getSSHSession(&storageHandle->scp.socketHandle),
                                                       String_cString(storageHandle->scp.archiveName),
                                                       NULL  // fileInfo
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
        }

        // reset index, read ahead buffer
        storageHandle->scp.index                  = 0LL;
        storageHandle->scp.readAheadBuffer.offset = 0LL;
        storageHandle->scp.readAheadBuffer.length = 0L;
      }

      if      (offset > storageHandle->scp.index)
      {
        uint64 skipSize = offset-storageHandle->scp.index;
        while (   (skipSize > 0LL)
               && (error == ERROR_NONE)
              )
        {
          // skip data in read-ahead buffer
          if (   (storageHandle->scp.index >= storageHandle->scp.readAheadBuffer.offset)
              && (storageHandle->scp.index < (storageHandle->scp.readAheadBuffer.offset+storageHandle->scp.readAheadBuffer.length))
             )
          {
            // skip data in read-ahead buffer, adjust skipSize, index
            ulong index      = (ulong)(storageHandle->scp.index-storageHandle->scp.readAheadBuffer.offset);
            ulong bytesAvail = MIN(skipSize,storageHandle->scp.readAheadBuffer.length-index);
            skipSize -= bytesAvail;
            storageHandle->scp.index += (uint64)bytesAvail;
          }

          if (skipSize > 0LL)
          {
            assert(storageHandle->scp.index >= (storageHandle->scp.readAheadBuffer.offset+storageHandle->scp.readAheadBuffer.length));

            // get max. number of bytes to receive in one step
            ulong length = (ulong)MIN(MIN((size_t)(storageHandle->scp.size-storageHandle->scp.index),skipSize),MAX_BUFFER_SIZE);
            if (storageHandle->storageInfo->scp.bandWidthLimiter.maxBandWidthList != NULL)
            {
              length = MIN(length,
                           storageHandle->storageInfo->scp.bandWidthLimiter.blockSize
                          );
            }
            assert(length > 0L);
//fprintf(stderr,"%s, %d: skipSize=%"PRIu64" length=%lu\n",__FILE__,__LINE__,skipSize,length);

            // get start time, start received bytes
            uint64 startTimestamp          = Misc_getTimestamp();
            uint64 startTotalReceivedBytes = storageHandle->scp.totalReceivedBytes;

            // read data
            ssize_t readBytes;
            do
            {
              readBytes = libssh2_channel_read(storageHandle->scp.channel,
                                               (char*)storageHandle->scp.readAheadBuffer.data,
                                               length
                                              );
              if (readBytes == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
            }
            while (readBytes == LIBSSH2_ERROR_EAGAIN);
            if (readBytes < 0)
            {
              error = ERROR_(IO,errno);
              continue;
            }
            storageHandle->scp.readAheadBuffer.offset = storageHandle->scp.index;
            storageHandle->scp.readAheadBuffer.length = (ulong)readBytes;
//fprintf(stderr,"%s,%d: readBytes=%ld storageHandle->scp.bufferOffset=%"PRIu64" storageHandle->scp.bufferLength=%lu\n",__FILE__,__LINE__,readBytes,storageHandle->scp.readAheadBuffer.offset,storageHandle->scp.readAheadBuffer.length);

            // skip data in read-ahead buffer, adjust skipSize, index
            ulong bytesAvail = storageHandle->scp.readAheadBuffer.length;
            skipSize -= bytesAvail;
            storageHandle->scp.index += (uint64)bytesAvail;

            // get end time, end received bytes
            uint64 endTimestamp          = Misc_getTimestamp();
            uint64 endTotalReceivedBytes = storageHandle->scp.totalReceivedBytes;
            assert(endTotalReceivedBytes >= startTotalReceivedBytes);

            /* limit used band width if requested (note: when the system time is
               changing endTimestamp may become smaller than startTimestamp;
               thus do not check this with an assert())
            */
            if (endTimestamp >= startTimestamp)
            {
              SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
              {
                limitBandWidth(&storageHandle->storageInfo->scp.bandWidthLimiter,
                               endTotalReceivedBytes-startTotalReceivedBytes,
                               endTimestamp-startTimestamp
                              );
              }
            }
          }
        }
      }
    }

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
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(fromArchiveName);
  UNUSED_VARIABLE(toArchiveName);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

LOCAL Errors StorageSCP_makeDirectory(const StorageInfo *storageInfo,
                                      ConstString       directoryName
                                     )
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(!String_isEmpty(directoryName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(directoryName);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

LOCAL Errors StorageSCP_delete(const StorageInfo *storageInfo,
                               ConstString       archiveName
                              )
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(!String_isEmpty(archiveName));

//  deleteFileName = (storageFileName != NULL) ? storageFileName : storageInfo->storageSpecifier.archiveName;
UNUSED_VARIABLE(storageInfo);
UNUSED_VARIABLE(archiveName);

  Errors error = ERROR_UNKNOWN;

  #ifdef HAVE_SSH2
#if 0
// TODO: whould this be a possible implementation?
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

/***********************************************************************\
* Name   : StorageSCP_getFileInfo
* Purpose: get storage file info
* Input  : fileInfo    - file info variable
*          storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : fileInfo - file info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSCP_getFileInfo(FileInfo          *fileInfo,
                                    const StorageInfo *storageInfo,
                                    ConstString       archiveName
                                   )
{
  assert(fileInfo != NULL);
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SCP);
  assert(archiveName != NULL);

  memClear(fileInfo,sizeof(fileInfo));
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

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

  // try to read directory content with sftp-protocol

  #ifdef HAVE_SSH2
    // init variables
    AutoFreeList autoFreeList;
    AutoFree_init(&autoFreeList);
    storageDirectoryListHandle->scp.pathName      = String_new();
    storageDirectoryListHandle->scp.buffer        = (char*)malloc(MAX_FILENAME_LENGTH);
    if (storageDirectoryListHandle->scp.buffer == NULL)
    {
      String_delete(storageDirectoryListHandle->scp.pathName);
      AutoFree_cleanup(&autoFreeList);
      return ERROR_INSUFFICIENT_MEMORY;
    }
    storageDirectoryListHandle->scp.bufferLength  = 0;
    storageDirectoryListHandle->scp.entryReadFlag = FALSE;
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->scp.buffer,{ free(storageDirectoryListHandle->scp.buffer); });
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->scp.pathName,{ String_delete(storageDirectoryListHandle->scp.pathName); });

    // set pathname
    String_set(storageDirectoryListHandle->scp.pathName,pathName);

    // get SSH server settings
    SSHServer sshServer;
    storageDirectoryListHandle->scp.serverId = Configuration_initSSHServerSettings(&sshServer,storageDirectoryListHandle->storageSpecifier.hostName,jobOptions);
    AUTOFREE_ADD(&autoFreeList,&sshServer,{ Configuration_doneSSHServerSettings(&sshServer); });
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.userName)) String_set(storageDirectoryListHandle->storageSpecifier.userName,sshServer.userName);
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.userName)) String_setCString(storageDirectoryListHandle->storageSpecifier.userName,getenv("LOGNAME"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.userName)) String_setCString(storageDirectoryListHandle->storageSpecifier.userName,getenv("USER"));
    if (storageDirectoryListHandle->storageSpecifier.hostPort == 0) storageDirectoryListHandle->storageSpecifier.hostPort = sshServer.port;
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }

    // allocate SSH server
    if (!allocateServer(storageDirectoryListHandle->scp.serverId,serverConnectionPriority,ALLOCATE_SERVER_TIMEOUT))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->scp.serverId,{ freeServer(storageDirectoryListHandle->scp.serverId); });

    Errors error;

    // check if SSH login is possible
    error = ERROR_SSH_AUTHENTICATION;
    if ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && !Password_isEmpty(&storageDirectoryListHandle->storageSpecifier.password))
    {
      error = checkSSHLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.hostPort,
                            storageDirectoryListHandle->storageSpecifier.userName,
                            &storageDirectoryListHandle->storageSpecifier.password,
                            sshServer.publicKey.data,
                            sshServer.publicKey.length,
                            sshServer.privateKey.data,
                            sshServer.privateKey.length
                           );
    }
    if ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && !Password_isEmpty(&sshServer.password))
    {
      error = checkSSHLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.hostPort,
                            storageDirectoryListHandle->storageSpecifier.userName,
                            &sshServer.password,
                            sshServer.publicKey.data,
                            sshServer.publicKey.length,
                            sshServer.privateKey.data,
                            sshServer.privateKey.length
                           );
      if (error == ERROR_NONE)
      {
        Password_set(&storageDirectoryListHandle->storageSpecifier.password,&sshServer.password);
      }
    }
    if (Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION)
    {
      // initialize interactive/default password
      uint retries = 0;
      while ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && (retries < MAX_PASSWORD_REQUESTS))
      {
        if (initSSHLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                         storageDirectoryListHandle->storageSpecifier.userName,
                         &storageDirectoryListHandle->storageSpecifier.password,
                         jobOptions,
// TODO:
CALLBACK_(NULL,NULL)//                         CALLBACK_(storageDirectoryListHandle->getNamePasswordFunction,storageDirectoryListHandle->getNamePasswordUserData)
                        )
           )
        {
          error = checkSSHLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                                storageDirectoryListHandle->storageSpecifier.hostPort,
                                storageDirectoryListHandle->storageSpecifier.userName,
                                &storageDirectoryListHandle->storageSpecifier.password,
                                sshServer.publicKey.data,
                                sshServer.publicKey.length,
                                sshServer.privateKey.data,
                                sshServer.privateKey.length
                               );
        }
        retries++;
      }
    }
    if (Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION)
    {
      error = (   !Password_isEmpty(&storageDirectoryListHandle->storageSpecifier.password)
               || !Password_isEmpty(&sshServer.password)
               || !Password_isEmpty(&defaultSSHPassword)
              )
                ? ERRORX_(INVALID_SSH_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName))
                : ERRORX_(NO_SSH_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName));
    }
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_FUNCTION_NOT_SUPPORTED;
    }

    // store password as default SSH password
    Password_set(&defaultSSHPassword,&storageDirectoryListHandle->storageSpecifier.password);

    // connect
    error = Network_connect(&storageDirectoryListHandle->scp.socketHandle,
                            SOCKET_TYPE_SSH,
                            storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.hostPort,
                            storageDirectoryListHandle->storageSpecifier.userName,
                            &defaultSSHPassword,
                            NULL,  // caData
                            0,     // caLength
                            NULL,  // certData
                            0,     // certLength
                            sshServer.publicKey.data,
                            sshServer.publicKey.length,
                            sshServer.privateKey.data,
                            sshServer.privateKey.length,
                              SOCKET_FLAG_NONE
                            | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                            | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                            30*MS_PER_SECOND
                           );
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_FUNCTION_NOT_SUPPORTED;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->scp.socketHandle,{ Network_disconnect(&storageDirectoryListHandle->scp.socketHandle); });

    // init scp session
    int ssh2ErrorCode = 0;
    do
    {
      storageDirectoryListHandle->scp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageDirectoryListHandle->scp.socketHandle));
      if (storageDirectoryListHandle->scp.sftp == NULL)
      {
        ssh2ErrorCode = libssh2_session_last_errno(Network_getSSHSession(&storageDirectoryListHandle->scp.socketHandle));
        if (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
      }
    }
    while ((storageDirectoryListHandle->scp.sftp == NULL) && (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN));
    if (storageDirectoryListHandle->scp.sftp == NULL)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_FUNCTION_NOT_SUPPORTED;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->scp.sftp,{ libssh2_sftp_shutdown(storageDirectoryListHandle->scp.sftp); });

    // open directory for reading
    storageDirectoryListHandle->scp.sftpHandle = libssh2_sftp_opendir(storageDirectoryListHandle->scp.sftp,
                                                                      !String_isEmpty(storageDirectoryListHandle->scp.pathName)
                                                                        ? String_cString(storageDirectoryListHandle->scp.pathName)
                                                                        : "."
                                                                     );
    if (storageDirectoryListHandle->scp.sftpHandle == NULL)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_FUNCTION_NOT_SUPPORTED;
    }

    // free resources
    Configuration_doneSSHServerSettings(&sshServer);
    AutoFree_done(&autoFreeList);

    return ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageDirectoryListHandle);
    UNUSED_VARIABLE(storageSpecifier);
    UNUSED_VARIABLE(pathName);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(serverConnectionPriority);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

LOCAL void StorageSCP_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  #ifdef HAVE_SSH2
    (void)libssh2_sftp_closedir(storageDirectoryListHandle->scp.sftpHandle);
    (void)libssh2_sftp_shutdown(storageDirectoryListHandle->scp.sftp);
    Network_disconnect(&storageDirectoryListHandle->scp.socketHandle);
    free(storageDirectoryListHandle->scp.buffer);
    String_delete(storageDirectoryListHandle->scp.pathName);
    freeServer(storageDirectoryListHandle->scp.serverId);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageDirectoryListHandle);
  #endif /* HAVE_SSH2 */
}

LOCAL bool StorageSCP_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  bool endOfDirectoryFlag = TRUE;
  #ifdef HAVE_SSH2
    {
      // read entry iff not already read
      while (!storageDirectoryListHandle->scp.entryReadFlag)
      {
        int n = libssh2_sftp_readdir(storageDirectoryListHandle->scp.sftpHandle,
                                     storageDirectoryListHandle->scp.buffer,
                                     MAX_FILENAME_LENGTH,
                                     &storageDirectoryListHandle->scp.attributes
                                    );
        if (n > 0)
        {
          if (   !LIBSSH2_SFTP_S_ISDIR(storageDirectoryListHandle->scp.attributes.permissions)
              || (   ((n != 1) || (strncmp(storageDirectoryListHandle->scp.buffer,".", 1) != 0))
                  && ((n != 2) || (strncmp(storageDirectoryListHandle->scp.buffer,"..",2) != 0))
                 )
             )
          {
            storageDirectoryListHandle->scp.bufferLength = n;
            storageDirectoryListHandle->scp.entryReadFlag = TRUE;
          }
        }
        else
        {
          break;
        }
      }

      endOfDirectoryFlag = !storageDirectoryListHandle->scp.entryReadFlag;
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageDirectoryListHandle);
  #endif /* HAVE_SSH2 */

  return endOfDirectoryFlag;
}

LOCAL Errors StorageSCP_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                          String                     fileName,
                                          FileInfo                   *fileInfo
                                         )
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SCP);

  Errors error = ERROR_UNKNOWN;

  #ifdef HAVE_SSH2
    {
      if (!storageDirectoryListHandle->scp.entryReadFlag)
      {
        do
        {
          int n = libssh2_sftp_readdir(storageDirectoryListHandle->scp.sftpHandle,
                                       storageDirectoryListHandle->scp.buffer,
                                       MAX_FILENAME_LENGTH,
                                       &storageDirectoryListHandle->scp.attributes
                                      );
          if      (n > 0)
          {
            if (   !LIBSSH2_SFTP_S_ISDIR(storageDirectoryListHandle->scp.attributes.permissions)
                || (   ((n != 1) || (strncmp(storageDirectoryListHandle->scp.buffer,".", 1) != 0))
                    && ((n != 2) || (strncmp(storageDirectoryListHandle->scp.buffer,"..",2) != 0))
                   )
               )
            {
              storageDirectoryListHandle->scp.entryReadFlag = TRUE;
              error = ERROR_NONE;
            }
          }
          else
          {
            error = ERROR_(IO,errno);
          }
        }
        while (!storageDirectoryListHandle->scp.entryReadFlag && (error == ERROR_NONE));
      }
      else
      {
        error = ERROR_NONE;
      }

      if (storageDirectoryListHandle->scp.entryReadFlag)
      {
        String_set(fileName,storageDirectoryListHandle->scp.pathName);
        File_appendFileNameBuffer(fileName,storageDirectoryListHandle->scp.buffer,storageDirectoryListHandle->scp.bufferLength);

        if (fileInfo != NULL)
        {
          if      (LIBSSH2_SFTP_S_ISREG(storageDirectoryListHandle->scp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_FILE;
          }
          else if (LIBSSH2_SFTP_S_ISDIR(storageDirectoryListHandle->scp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_DIRECTORY;
          }
          else if (LIBSSH2_SFTP_S_ISLNK(storageDirectoryListHandle->scp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_LINK;
          }
          else if (LIBSSH2_SFTP_S_ISCHR(storageDirectoryListHandle->scp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_SPECIAL;
            fileInfo->specialType = FILE_SPECIAL_TYPE_CHARACTER_DEVICE;
          }
          else if (LIBSSH2_SFTP_S_ISBLK(storageDirectoryListHandle->scp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_SPECIAL;
            fileInfo->specialType = FILE_SPECIAL_TYPE_BLOCK_DEVICE;
          }
          else if (LIBSSH2_SFTP_S_ISFIFO(storageDirectoryListHandle->scp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_SPECIAL;
            fileInfo->specialType = FILE_SPECIAL_TYPE_FIFO;
          }
          else if (LIBSSH2_SFTP_S_ISSOCK(storageDirectoryListHandle->scp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_SPECIAL;
            fileInfo->specialType = FILE_SPECIAL_TYPE_SOCKET;
          }
          else
          {
            fileInfo->type        = FILE_TYPE_UNKNOWN;
          }
          fileInfo->size            = storageDirectoryListHandle->scp.attributes.filesize;
          fileInfo->timeLastAccess  = storageDirectoryListHandle->scp.attributes.atime;
          fileInfo->timeModified    = storageDirectoryListHandle->scp.attributes.mtime;
          fileInfo->timeLastChanged = storageDirectoryListHandle->scp.attributes.mtime;  // Note: no timestamp for meta data - use timestamp for content
          fileInfo->userId          = storageDirectoryListHandle->scp.attributes.uid;
          fileInfo->groupId         = storageDirectoryListHandle->scp.attributes.gid;
          fileInfo->permissions     = storageDirectoryListHandle->scp.attributes.permissions;
          fileInfo->major           = 0;
          fileInfo->minor           = 0;
          memClear(&fileInfo->cast,sizeof(FileCast));
        }

        storageDirectoryListHandle->scp.entryReadFlag = FALSE;
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
