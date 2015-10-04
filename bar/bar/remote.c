/***********************************************************************\
*
* $Revision: 4126 $
* $Date: 2015-09-19 10:57:45 +0200 (Sat, 19 Sep 2015) $
* $Author: torsten $
* Contents: Backup ARchiver server
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_SYS_SELECT_H
  #include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */
#include <pthread.h>
#include <locale.h>
#include <time.h>
#include <signal.h>
#include <assert.h>

#include "global.h"
#include "autofree.h"
#include "lists.h"
#include "strings.h"
#include "stringmaps.h"
#include "arrays.h"
#include "configvalues.h"
#include "threads.h"
#include "semaphores.h"
#include "msgqueues.h"
#include "stringlists.h"

#include "network.h"
#include "patterns.h"
#include "entrylists.h"
#include "patternlists.h"
#include "misc.h"
#include "bar.h"

//#include "commands_create.h"
//#include "commands_restore.h"

#include "server.h"

#include "remote.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
typedef struct RemoteServerNode
{
  LIST_NODE_HEADER(struct RemoteServerNode);

  String       hostName;
  uint         hostPort;
  bool         sslFlag;

  SessionId    sessionId;
  CryptKey     publicKey,secretKey;

  uint         commandId;

  SocketHandle socketHandle;
} RemoteServerNode;

// list with remote servers
typedef struct
{
  LIST_HEADER(RemoteServerNode);

  Semaphore lock;
} RemoteServerList;

/***************************** Variables *******************************/
LOCAL RemoteServerList remoteServerList;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Remote_findServer
* Purpose: find remote server connection
* Input  : remoteHost - remote host
* Output : -
* Return : remote server or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL RemoteServerNode *Remote_findServer(const RemoteHost *remoteHost)
{
  RemoteServerNode *remoteServerNode;

  assert(remoteHost != NULL);

  LIST_ITERATE(&remoteServerList,remoteServerNode)
  {
    if (   (remoteServerNode->hostPort == remoteHost->port)
        && String_equals(remoteServerNode->hostName,remoteHost->name)
        && (!remoteHost->forceSSL || remoteServerNode->sslFlag)
       )
    {
      return remoteServerNode;
    }
  }
  return NULL;
}

/***********************************************************************\
* Name   :
* Purpose:
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#if 0
//typedef bool(*StringMapParseEnumFunction)(const char *name, uint *value);
LOCAL bool parseEnumState(const char *name, uint *value)
{
//  if (stringEqualsIgnoreCase(name,"running")) value = SERVER_STATE_RUNNING;
//  else return FALSE;

  return TRUE;
}
#endif

/***********************************************************************\
* Name   : Remote_setJobOptionInteger
* Purpose: set job int value
* Input  : remoteHost - remote host
*          jobUUID    - job UUID
*          name       - value name
*          value      - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Remote_setJobOptionInteger(const RemoteHost *remoteHost, ConstString jobUUID, const char *name, int value)
{
  assert(remoteHost != NULL);
  assert(name != NULL);

  return Remote_executeCommand(remoteHost,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%d",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Remote_setJobOptionInteger64
* Purpose: set job int64 value
* Input  : remoteHost - remote host
*          jobUUID    - job UUID
*          name       - value name
*          value      - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Remote_setJobOptionInteger64(const RemoteHost *remoteHost, ConstString jobUUID, const char *name, int64 value)
{
  assert(remoteHost != NULL);
  assert(name != NULL);

  return Remote_executeCommand(remoteHost,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%lld",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Remote_setJobOptionBoolean
* Purpose: set job boolean value
* Input  : remoteHost - remote host
*          jobUUID    - job UUID
*          name       - value name
*          value      - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Remote_setJobOptionBoolean(const RemoteHost *remoteHost, ConstString jobUUID, const char *name, bool value)
{
  assert(remoteHost != NULL);
  assert(name != NULL);

  return Remote_executeCommand(remoteHost,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%y",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Remote_setJobOptionString
* Purpose: set job string value
* Input  : remoteHost - remote host
*          jobUUID    - job UUID
*          name       - value name
*          value      - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Remote_setJobOptionString(const RemoteHost *remoteHost, ConstString jobUUID, const char *name, ConstString value)
{
  assert(remoteHost != NULL);
  assert(name != NULL);

  return Remote_executeCommand(remoteHost,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'S",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Remote_setJobOptionCString
* Purpose: set job c-string value
* Input  : remoteHost - remote host
*          jobUUID    - job UUID
*          name       - value name
*          value      - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Remote_setJobOptionCString(const RemoteHost *remoteHost, ConstString jobUUID, const char *name, const char *value)
{
  assert(remoteHost != NULL);
  assert(name != NULL);

  return Remote_executeCommand(remoteHost,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'s",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Remote_setJobOptionPassword
* Purpose: set job password option
* Input  : remoteHost - remote host
*          jobUUID    - job UUID
*          name       - value name
*          password   - password
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Remote_setJobOptionPassword(const RemoteHost *remoteHost, ConstString jobUUID, const char *name, Password *password)
{
  const char *plainPassword;
  Errors     error;

  assert(remoteHost != NULL);
  assert(name != NULL);

  plainPassword = Password_deploy(password);
  error = Remote_executeCommand(remoteHost,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'s",jobUUID,name,plainPassword);
  Password_undeploy(password);

  return error;
}

// ----------------------------------------------------------------------

void Remote_initHost(RemoteHost *remoteHost)
{
  assert(remoteHost != NULL);

  remoteHost->name     = String_new();
  remoteHost->port     = 0;
  remoteHost->forceSSL = FALSE;
}

void Remote_doneHost(RemoteHost *remoteHost)
{
  assert(remoteHost != NULL);

  String_delete(remoteHost->name);
}

void Remote_copyHost(RemoteHost *toRemoteHost, const RemoteHost *fromRemoteHost)
{
  assert(toRemoteHost != NULL);
  assert(fromRemoteHost != NULL);

  String_set(toRemoteHost->name,fromRemoteHost->name);
  toRemoteHost->port     = fromRemoteHost->port;
  toRemoteHost->forceSSL = fromRemoteHost->forceSSL;
}

void Remote_duplicateHost(RemoteHost *toRemoteHost, const RemoteHost *fromRemoteHost)
{
  assert(toRemoteHost != NULL);
  assert(fromRemoteHost != NULL);

  Remote_initHost(toRemoteHost);
  Remote_copyHost(toRemoteHost,fromRemoteHost);
}

Errors Remote_serverConnect(SocketHandle *socketHandle,
                            ConstString  hostName,
                            uint         hostPort,
                            bool         hostForceSSL
                           )
{
  String line;
  Errors error;

  assert(socketHandle != NULL);

  // init variables
  line = String_new();

  // connect to remote host
  error = Network_connect(socketHandle,
                          hostForceSSL ? SOCKET_TYPE_TLS : SOCKET_TYPE_PLAIN,
                          hostName,
                          hostPort,
                          NULL,  // loginName
                          NULL,  // password
                          NULL,  // sshPublicKeyFileName
                          0,
                          NULL,  // sshPrivateKeyFileName
                          0,
                          SOCKET_FLAG_NONE
                         );
  if (error != ERROR_NONE)
  {
    String_delete(line);
    return error;
  }

  // authorize
  error = Network_readLine(socketHandle,line,30LL*1000);
  if (error != ERROR_NONE)
  {
    String_delete(line);
    return error;
  }
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,String_cString(line));

  // free resources
  String_delete(line);

  return ERROR_NONE;
}

void Remote_serverDisconnect(SocketHandle *socketHandle)
{
  assert(socketHandle != NULL);

  Network_disconnect(socketHandle);
}

//TODO
LOCAL Errors Remote_vexecuteCommand(const RemoteHost *remoteHost,
                              StringMap  resultMap,
                              const char *format,
                              va_list    arguments
                             )
{
  String        line;
  SemaphoreLock semaphoreLock;
  RemoteServerNode *remoteServerNode;
  SocketHandle  socketHandle;
  bool          sslFlag;
  locale_t locale;
  Errors   error;
  uint     commandId;
  bool     completedFlag;
  uint     errorCode;
  long     index;

  assert(remoteHost != NULL);

  // init variables
  line = String_new();

  SEMAPHORE_LOCKED_DO(semaphoreLock,&remoteServerList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    // find remote server
    remoteServerNode = Remote_findServer(remoteHost);

    if (remoteServerNode == NULL)
    {
      // try to connect
      error = Remote_serverConnect(&socketHandle,remoteHost->name,remoteHost->port,remoteHost->forceSSL);
      if (error != ERROR_NONE)
      {
        Semaphore_unlock(&remoteServerList.lock);
        String_delete(line);
        return error;
      }
//TODO
sslFlag = TRUE;

      // add remote server
      remoteServerNode = LIST_NEW_NODE(RemoteServerNode);
      if (remoteServerNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      remoteServerNode->hostName     = String_duplicate(remoteHost->name);
      remoteServerNode->hostPort     = remoteHost->port;
      remoteServerNode->sslFlag      = sslFlag;
      remoteServerNode->commandId    = 0;
      remoteServerNode->socketHandle = socketHandle;
      List_append(&remoteServerList,remoteServerNode);
    }

    // new command id
    remoteServerNode->commandId++;

    // format command
    locale = uselocale(POSIXLocale);
    {
      String_format(line,"%d ",remoteServerNode->commandId);
      String_vformat(line,format,arguments);
      String_appendChar(line,'\n');
    }
    uselocale(locale);

    // send command
    (void)Network_send(&remoteServerNode->socketHandle,String_cString(line),String_length(line));
//fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,String_cString(line));

    // wait for result
    error = Network_readLine(&remoteServerNode->socketHandle,line,30LL*1000);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&remoteServerList.lock);
      String_delete(line);
      return error;
    }

    // parse result
//fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,String_cString(line));
    if (!String_parse(line,STRING_BEGIN,"%u %y %u",&index,&commandId,&completedFlag,&errorCode))
    {
      Semaphore_unlock(&remoteServerList.lock);
      String_delete(line);
      return ERROR_INVALID_RESPONSE;
    }
UNUSED_VARIABLE(commandId);
UNUSED_VARIABLE(completedFlag);

    // get result
    if (resultMap != NULL)
    {
      StringMap_clear(resultMap);
      if (!StringMap_parse(resultMap,line,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,index,NULL))
      {
        Semaphore_unlock(&remoteServerList.lock);
        String_delete(line);
        return ERROR_INVALID_RESPONSE;
      }
    }
  }

  // free resources
  String_delete(line);

  return ERROR_NONE;
}

Errors Remote_executeCommand(const RemoteHost *remoteHost,
                             StringMap        resultMap,
                             const char       *format,
                             ...
                            )
{
  va_list  arguments;
  Errors   error;

  assert(remoteHost != NULL);

  va_start(arguments,format);
  error = Remote_vexecuteCommand(remoteHost,resultMap,format,arguments);
  va_end(arguments);

  return error;
}

Errors Remote_jobStart(const RemoteHost                *remoteHost,
                       ConstString                     jobUUID,
                       ConstString                     scheduleUUID,
                       ConstString                     storageName,
                       const EntryList                 *includeEntryList,
                       const PatternList               *excludePatternList,
                       const PatternList               *compressExcludePatternList,
                       DeltaSourceList                 *deltaSourceList,
                       JobOptions                      *jobOptions,
                       ArchiveTypes                    archiveType,
                       ConstString                     scheduleTitle,
                       ConstString                     scheduleCustomText,
  //                     ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
  //                     void                            *archiveGetCryptPasswordUserData,
  //                     CreateStatusInfoFunction        createStatusInfoFunction,
  //                     void                            *createStatusInfoUserData,
                       StorageRequestVolumeFunction    storageRequestVolumeFunction,
                       void                            *storageRequestVolumeUserData
                      )
{
  String          s;
  Errors          error;
  EntryNode       *entryNode;
  const char      *entryTypeText;
  PatternNode     *patternNode;
  DeltaSourceNode *deltaSourceNode;
  bool            quitFlag;

error = ERROR_STILL_NOT_IMPLEMENTED;

  assert(remoteHost != NULL);
  assert(jobUUID != NULL);

  // init variables
  s = String_new();

  error = ERROR_NONE;

  // create temporary job
  error = Remote_executeCommand(remoteHost,NULL,"JOB_NEW uuid=%S name=%'S master=%'S",jobUUID,jobUUID/*TODO name*/,Network_getHostName(s));
  if (error != ERROR_NONE)
  {
    return error;
  }

  // set options
  error = ERROR_NONE;
/*EntryNode

  CONFIG_STRUCT_VALUE_STRING   ("ssh-public-key",          JobNode,jobOptions.sshServer.publicKeyFileName  ),
  CONFIG_STRUCT_VALUE_STRING   ("ssh-private-key",         JobNode,jobOptions.sshServer.privateKeyFileName ),
*/
  if (error == ERROR_NONE) error = Remote_setJobOptionString   (remoteHost,jobUUID,"archive-name",storageName);
  if (error == ERROR_NONE) error = Remote_setJobOptionString   (remoteHost,jobUUID,"incremental-list-file",  jobOptions->incrementalListFileName     );
  if (error == ERROR_NONE) error = Remote_setJobOptionInteger64(remoteHost,jobUUID,"archive-part-size",      jobOptions->archivePartSize             );
//  if (error == ERROR_NONE) error = Remote_setJobOptionInt(remoteHost,jobUUID,"directory-strip",jobOptions->directoryStripCount);
//  if (error == ERROR_NONE) error = Remote_setJobOptionString(remoteHost,jobUUID,"destination",jobOptions->destination);
//  if (error == ERROR_NONE) error = Remote_setJobOptionString(remoteHost,jobUUID,"owner",jobOptions->owner);
//  if (error == ERROR_NONE) error = Remote_setJobOptionString   (remoteHost,jobUUID,"pattern-type",           jobOptions->pattern-type                );
  ;
  if (error == ERROR_NONE) error = Remote_setJobOptionString   (remoteHost,jobUUID,
                                                                "compress-algorithm",
                                                                String_format(String_clear(s),
                                                                              "%s+%s",
                                                                              ConfigValue_selectToString(CONFIG_VALUE_COMPRESS_ALGORITHMS,jobOptions->compressAlgorithms.delta,NULL),
                                                                              ConfigValue_selectToString(CONFIG_VALUE_COMPRESS_ALGORITHMS,jobOptions->compressAlgorithms.byte ,NULL)
                                                                             )
                                                               );
  if (error == ERROR_NONE) error = Remote_setJobOptionCString  (remoteHost,jobUUID,
                                                                "crypt-algorithm",
                                                                ConfigValue_selectToString(CONFIG_VALUE_CRYPT_ALGORITHMS,jobOptions->cryptAlgorithm,NULL)
                                                               );
  if (error == ERROR_NONE) error = Remote_setJobOptionCString  (remoteHost,jobUUID,
                                                                "crypt-type",
                                                                ConfigValue_selectToString(CONFIG_VALUE_CRYPT_TYPES,jobOptions->cryptType,NULL)
                                                               );
  if (error == ERROR_NONE) error = Remote_setJobOptionCString  (remoteHost,
                                                                jobUUID,
                                                                "crypt-password-mode",
                                                                ConfigValue_selectToString(CONFIG_VALUE_PASSWORD_MODES,jobOptions->cryptPasswordMode,NULL)
                                                               );
  if (error == ERROR_NONE) error = Remote_setJobOptionPassword (remoteHost,jobUUID,"crypt-password",         jobOptions->cryptPassword               );
  if (error == ERROR_NONE) error = Remote_setJobOptionString   (remoteHost,jobUUID,"crypt-public-key",       jobOptions->cryptPublicKeyFileName      );
  if (error == ERROR_NONE) error = Remote_setJobOptionString   (remoteHost,jobUUID,"mount-device",           jobOptions->mountDeviceName             );
  if (error == ERROR_NONE) error = Remote_setJobOptionString   (remoteHost,jobUUID,"pre-command",            jobOptions->preProcessCommand           );
  if (error == ERROR_NONE) error = Remote_setJobOptionString   (remoteHost,jobUUID,"post-command",           jobOptions->postProcessCommand          );
  if (error == ERROR_NONE) error = Remote_setJobOptionString   (remoteHost,jobUUID,"ftp-login-name",         jobOptions->ftpServer.loginName         );
  if (error == ERROR_NONE) error = Remote_setJobOptionPassword (remoteHost,jobUUID,"ftp-password",           jobOptions->ftpServer.password          );
  if (error == ERROR_NONE) error = Remote_setJobOptionInteger  (remoteHost,jobUUID,"ssh-port",               jobOptions->sshServer.port              );
  if (error == ERROR_NONE) error = Remote_setJobOptionString   (remoteHost,jobUUID,"ssh-login-name",         jobOptions->sshServer.loginName         );
  if (error == ERROR_NONE) error = Remote_setJobOptionPassword (remoteHost,jobUUID,"ssh-password",           jobOptions->sshServer.password          );
  if (error == ERROR_NONE) error = Remote_setJobOptionString   (remoteHost,
                                                                jobUUID,
                                                                "ssh-public-key-data",
                                                                Misc_base64Encode(s,jobOptions->sshServer.publicKey.data,jobOptions->sshServer.publicKey.length)
                                                               );
  if (error == ERROR_NONE) error = Remote_setJobOptionString   (remoteHost,jobUUID,
                                                                "ssh-private-key-data",
                                                                Misc_base64Encode(s,jobOptions->sshServer.privateKey.data,jobOptions->sshServer.privateKey.length)
                                                               );
  if (error == ERROR_NONE) error = Remote_setJobOptionInteger64(remoteHost,jobUUID,"volume-size",            jobOptions->volumeSize                  );
  if (error == ERROR_NONE) error = Remote_setJobOptionBoolean  (remoteHost,jobUUID,"ecc",                    jobOptions->errorCorrectionCodesFlag    );
  if (error == ERROR_NONE) error = Remote_setJobOptionBoolean  (remoteHost,jobUUID,"skip-unreadable",        jobOptions->skipUnreadableFlag          );
  if (error == ERROR_NONE) error = Remote_setJobOptionBoolean  (remoteHost,jobUUID,"raw-images",             jobOptions->rawImagesFlag               );
  if (error == ERROR_NONE) error = Remote_setJobOptionBoolean  (remoteHost,jobUUID,"overwrite-archive-files",jobOptions->overwriteArchiveFilesFlag   );
  if (error == ERROR_NONE) error = Remote_setJobOptionBoolean  (remoteHost,jobUUID,"overwrite-files",        jobOptions->overwriteFilesFlag          );
  if (error == ERROR_NONE) error = Remote_setJobOptionBoolean  (remoteHost,jobUUID,"wait-first-volume",      jobOptions->waitFirstVolumeFlag         );
fprintf(stderr,"%s, %d: %d: Remote_jobStart %s\n",__FILE__,__LINE__,error,Error_getText(error));

  if (error == ERROR_NONE) error = Remote_executeCommand(remoteHost,NULL,"INCLUDE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(includeEntryList,entryNode)
  {
    switch (entryNode->type)
    {
      case ENTRY_TYPE_FILE :   entryTypeText = "FILE";    break;
      case ENTRY_TYPE_IMAGE:   entryTypeText = "IMAGE";   break;
      case ENTRY_TYPE_UNKNOWN: entryTypeText = "UNKNOWN"; break;
    }
    if (error == ERROR_NONE) error = Remote_executeCommand(remoteHost,
                                                           NULL,
                                                           "INCLUDE_LIST_ADD jobUUID=%S entryType=%s patternType=%s pattern=%'S",
                                                           jobUUID,
                                                           entryTypeText,
                                                           ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,entryNode->patternType,NULL),
                                                           entryNode->string
                                                          );
  }

  if (error == ERROR_NONE) error = Remote_executeCommand(remoteHost,NULL,"EXCLUDE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(excludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Remote_executeCommand(remoteHost,
                                                           NULL,
                                                           "EXCLUDE_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                           jobUUID,
                                                           ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->patternType,NULL),
                                                           patternNode->string
                                                          );
  }

  if (error == ERROR_NONE) error = Remote_executeCommand(remoteHost,NULL,"EXCLUDE_COMPRESS_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(compressExcludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Remote_executeCommand(remoteHost,
                                                           NULL,
                                                           "EXCLUDE_COMPRESS_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                           jobUUID,
                                                           ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->patternType,NULL),
                                                           patternNode->string
                                                          );
  }

  if (error == ERROR_NONE) error = Remote_executeCommand(remoteHost,NULL,"SOURCE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(compressExcludePatternList,deltaSourceNode)
  {
    if (error == ERROR_NONE) error = Remote_executeCommand(remoteHost,
                                                           NULL,
                                                           "SOURCE_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                           jobUUID,
                                                           ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,deltaSourceNode->patternType,NULL),
                                                           deltaSourceNode->storageName
                                                          );
  }
  if (error != ERROR_NONE)
  {
    (void)Remote_executeCommand(remoteHost,NULL,"JOB_DELETE jobUUID=%S",jobUUID);
    String_delete(s);
    return error;
  }

//exit(123);
  // start execute job
  error = Remote_executeCommand(remoteHost,NULL,"JOB_START jobUUID=%S archiveType=%s dryRun=%y",jobUUID,"FULL"/*archiveType*/,FALSE);
  if (error != ERROR_NONE)
  {
    (void)Remote_executeCommand(remoteHost,NULL,"JOB_DELETE jobUUID=%S",jobUUID);
    String_delete(s);
    return error;
  }

  // free resources
  String_delete(s);

  return ERROR_NONE;
}

Errors Remote_jobAbort(const RemoteHost *remoteHost,
                       ConstString      jobUUID
                      )
{
  Errors error;

  assert(remoteHost != NULL);
  assert(jobUUID != NULL);

  error = ERROR_NONE;

  // abort execute job
  error = Remote_executeCommand(remoteHost,NULL,"JOB_ABORT jobUUID=%S",jobUUID);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // delete temporary job
  error = Remote_executeCommand(remoteHost,NULL,"JOB_DELETE jobUUID=%S",jobUUID);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // free resources

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
