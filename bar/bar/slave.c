/***********************************************************************\
*
* $Revision: 4126 $
* $Date: 2015-09-19 10:57:45 +0200 (Sat, 19 Sep 2015) $
* $Author: torsten $
* Contents: Backup ARchiver slave functions
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
#include "semaphores.h"

#include "network.h"
#include "entrylists.h"
#include "patternlists.h"
#include "misc.h"
#include "bar.h"

#include "server.h"

#include "slave.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

// list with slaves
typedef struct SlaveNode
{
  LIST_NODE_HEADER(struct SlaveNode);

  String       hostName;
  uint         hostPort;
  bool         sslFlag;

  SessionId    sessionId;
  CryptKey     publicKey,secretKey;

  uint         commandId;

  SocketHandle socketHandle;
} SlaveNode;

typedef struct
{
  LIST_HEADER(SlaveNode);

  Semaphore lock;
} SlaveList;

/***************************** Variables *******************************/
LOCAL SlaveList slaveList;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : findSlave
* Purpose: find slave
* Input  : slaveHost - slave host
* Output : -
* Return : slave or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL SlaveNode *findSlave(const SlaveHost *slaveHost)
{
  SlaveNode *slaveNode;

  assert(slaveHost != NULL);

  LIST_ITERATE(&slaveList,slaveNode)
  {
    if (   (slaveNode->hostPort == slaveHost->port)
        && String_equals(slaveNode->hostName,slaveHost->name)
        && (!slaveHost->forceSSL || slaveNode->sslFlag)
       )
    {
      return slaveNode;
    }
  }
  return NULL;
}

/***********************************************************************\
* Name   : serverConnect
* Purpose: connect to server
* Input  : socketHandle - socket handle variable
*          hostName     - host name
*          hostPort     - host port
*          hostForceSSL - TRUE to force SSL
* Output : socketHandle - socket handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors serverConnect(SocketHandle *socketHandle,
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

  // connect to slave host
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

  // free resources
  String_delete(line);

  return ERROR_NONE;
}

#if 0
not used
/***********************************************************************\
* Name   : serverDisconnect
* Purpose: disconnect from server
* Input  : socketHandle - socket handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void serverDisconnect(SocketHandle *socketHandle)
{
  assert(socketHandle != NULL);

  Network_disconnect(socketHandle);
}
#endif

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
* Name   : Slave_setJobOptionInteger
* Purpose: set job int value
* Input  : slaveHost - slave host
*          jobUUID   - job UUID
*          name      - value name
*          value     - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Slave_setJobOptionInteger(const SlaveHost *slaveHost, ConstString jobUUID, const char *name, int value)
{
  assert(slaveHost != NULL);
  assert(name != NULL);

  return Slave_executeCommand(slaveHost,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%d",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Slave_setJobOptionInteger64
* Purpose: set job int64 value
* Input  : slaveHost - slave host
*          jobUUID   - job UUID
*          name      - value name
*          value     - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Slave_setJobOptionInteger64(const SlaveHost *slaveHost, ConstString jobUUID, const char *name, int64 value)
{
  assert(slaveHost != NULL);
  assert(name != NULL);

  return Slave_executeCommand(slaveHost,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%lld",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Slave_setJobOptionBoolean
* Purpose: set job boolean value
* Input  : slaveHost - slave host
*          jobUUID   - job UUID
*          name      - value name
*          value     - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Slave_setJobOptionBoolean(const SlaveHost *slaveHost, ConstString jobUUID, const char *name, bool value)
{
  assert(slaveHost != NULL);
  assert(name != NULL);

  return Slave_executeCommand(slaveHost,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%y",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Slave_setJobOptionString
* Purpose: set job string value
* Input  : slaveHost - slave host
*          jobUUID   - job UUID
*          name      - value name
*          value     - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Slave_setJobOptionString(const SlaveHost *slaveHost, ConstString jobUUID, const char *name, ConstString value)
{
  assert(slaveHost != NULL);
  assert(name != NULL);

  return Slave_executeCommand(slaveHost,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'S",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Slave_setJobOptionCString
* Purpose: set job c-string value
* Input  : slaveHost - slave host
*          jobUUID   - job UUID
*          name      - value name
*          value     - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Slave_setJobOptionCString(const SlaveHost *slaveHost, ConstString jobUUID, const char *name, const char *value)
{
  assert(slaveHost != NULL);
  assert(name != NULL);

  return Slave_executeCommand(slaveHost,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'s",jobUUID,name,value);
}

/***********************************************************************\
* Name   : Slave_setJobOptionPassword
* Purpose: set job password option
* Input  : slaveHost - slave host
*          jobUUID   - job UUID
*          name      - value name
*          password  - password
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Slave_setJobOptionPassword(const SlaveHost *slaveHost, ConstString jobUUID, const char *name, Password *password)
{
  const char *plainPassword;
  Errors     error;

  assert(slaveHost != NULL);
  assert(name != NULL);

  plainPassword = Password_deploy(password);
  error = Slave_executeCommand(slaveHost,NULL,"JOB_OPTION_SET jobUUID=%S name=%s value=%'s",jobUUID,name,plainPassword);
  Password_undeploy(password,plainPassword);

  return error;
}

// ----------------------------------------------------------------------

void Slave_initHost(SlaveHost *slaveHost, uint defaultPort)
{
  assert(slaveHost != NULL);

  slaveHost->name     = String_new();
  slaveHost->port     = defaultPort;
  slaveHost->forceSSL = FALSE;

  DEBUG_ADD_RESOURCE_TRACE(slaveHost,sizeof(SlaveHost));
}

void Slave_doneHost(SlaveHost *slaveHost)
{
  assert(slaveHost != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(slaveHost,sizeof(SlaveHost));

  String_delete(slaveHost->name);
}

void Slave_copyHost(SlaveHost *toSlaveHost, const SlaveHost *fromSlaveHost)
{
  assert(toSlaveHost != NULL);
  assert(fromSlaveHost != NULL);

  String_set(toSlaveHost->name,fromSlaveHost->name);
  toSlaveHost->port     = fromSlaveHost->port;
  toSlaveHost->forceSSL = fromSlaveHost->forceSSL;
}

void Slave_duplicateHost(SlaveHost *toSlaveHost, const SlaveHost *fromSlaveHost)
{
  assert(toSlaveHost != NULL);
  assert(fromSlaveHost != NULL);

  Slave_initHost(toSlaveHost,0);
  Slave_copyHost(toSlaveHost,fromSlaveHost);
}

Errors Slave_connect(const SlaveHost *slaveHost)
{
  String       line;
  SocketHandle socketHandle;
  Errors       error;
  bool         sslFlag;
  SlaveNode    *slaveNode;

  assert(slaveHost != NULL);
  assert(slaveHost->name != NULL);

  // init variables
  line = String_new();

//TODO
  // get default ports

  // connect to slave host
  error = Network_connect(&socketHandle,
                          slaveHost->forceSSL ? SOCKET_TYPE_TLS : SOCKET_TYPE_PLAIN,
                          slaveHost->name,
                          slaveHost->port,
//TODO: SSL
                          NULL,  // loginName
                          NULL,  // password
                          NULL,  // sshPublicKey
                          0,
                          NULL,  // sshPrivateKey
                          0,
                          SOCKET_FLAG_NONE
                         );
  if (error != ERROR_NONE)
  {
    String_delete(line);
    return error;
  }

//TODO
sslFlag = TRUE;

  // authorize
  error = Network_readLine(&socketHandle,line,30LL*1000);
  if (error != ERROR_NONE)
  {
    Network_disconnect(&socketHandle);
    String_delete(line);
    return error;
  }
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,String_cString(line));

  // add slave
  slaveNode = LIST_NEW_NODE(SlaveNode);
  if (slaveNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  slaveNode->hostName     = String_duplicate(slaveHost->name);
  slaveNode->hostPort     = slaveHost->port;
  slaveNode->sslFlag      = sslFlag;
  slaveNode->commandId    = 0;
  slaveNode->socketHandle = socketHandle;
  List_append(&slaveList,slaveNode);

  // free resources
  String_delete(line);

  printInfo(2,"Connected slave host '%s:%d'\n",String_cString(slaveNode->hostName),slaveNode->hostPort);

  return ERROR_NONE;
}

void Slave_disconnect(const SlaveHost *slaveHost)
{
  SemaphoreLock semaphoreLock;
  SlaveNode     *slaveNode;

  // find and remove slave
  SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // find slave server
    slaveNode = findSlave(slaveHost);

    // disconnect
    if (slaveNode != NULL)
    {
      List_remove(&slaveList,slaveNode);

      Network_disconnect(&slaveNode->socketHandle);

      String_delete(slaveNode->hostName);
      LIST_DELETE_NODE(slaveNode);
    }
  }
}

bool Slave_isConnected(const SlaveHost *slaveHost)
{
  bool          isConnected;
  SemaphoreLock semaphoreLock;
  SlaveNode     *slaveNode;

  assert(slaveHost != NULL);

  isConnected = FALSE;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    slaveNode = findSlave(slaveHost);
    if (slaveNode != NULL)
    {
      if (Network_isConnected(&slaveNode->socketHandle))
      {
        isConnected = TRUE;
      }
      else
      {
        List_remove(&slaveList,slaveNode);

        Network_disconnect(&slaveNode->socketHandle);

        String_delete(slaveNode->hostName);
        LIST_DELETE_NODE(slaveNode);
      }
    }
  }

  return isConnected;
}

//TODO
LOCAL Errors Slave_vexecuteCommand(const SlaveHost *slaveHost,
                                   StringMap       resultMap,
                                   const char      *format,
                                   va_list         arguments
                                  )
{
  String         line;
  SemaphoreLock  semaphoreLock;
  SlaveNode      *slaveNode;
  SocketHandle   socketHandle;
  bool           sslFlag;
  locale_t       locale;
  Errors         error;
  uint           commandId;
  bool           completedFlag;
  uint           errorCode;
  long           index;

  assert(slaveHost != NULL);

  // init variables
  line = String_new();

  SEMAPHORE_LOCKED_DO(semaphoreLock,&slaveList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // find slave server
    slaveNode = findSlave(slaveHost);

    // check if slave server known
    if (slaveNode == NULL)
    {
      // try to connect
      error = serverConnect(&socketHandle,slaveHost->name,slaveHost->port,slaveHost->forceSSL);
      if (error != ERROR_NONE)
      {
        Semaphore_unlock(&slaveList.lock);
        String_delete(line);
        return error;
      }
//TODO
sslFlag = TRUE;

      // add slave
      slaveNode = LIST_NEW_NODE(SlaveNode);
      if (slaveNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      slaveNode->hostName     = String_duplicate(slaveHost->name);
      slaveNode->hostPort     = slaveHost->port;
      slaveNode->sslFlag      = sslFlag;
      slaveNode->commandId    = 0;
      slaveNode->socketHandle = socketHandle;
      List_append(&slaveList,slaveNode);
    }

    // create new command id
    slaveNode->commandId++;

    // format command
    locale = uselocale(POSIXLocale);
    {
      String_format(line,"%d ",slaveNode->commandId);
      String_vformat(line,format,arguments);
      String_appendChar(line,'\n');
    }
    uselocale(locale);

    // send command
    (void)Network_send(&slaveNode->socketHandle,String_cString(line),String_length(line));
    printInfo(4,"Sent slave command: %s",String_cString(line));

    // wait for result
    error = Network_readLine(&slaveNode->socketHandle,line,30LL*1000);
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&slaveList.lock);
      String_delete(line);
      return error;
    }
    printInfo(4,"Received slave result: %s\n",String_cString(line));

    // parse result
//fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,String_cString(line));
    if (!String_parse(line,STRING_BEGIN,"%u %y %u",&index,&commandId,&completedFlag,&errorCode))
    {
      Semaphore_unlock(&slaveList.lock);
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
        Semaphore_unlock(&slaveList.lock);
        String_delete(line);
        return ERROR_INVALID_RESPONSE;
      }
    }
  }

  // free resources
  String_delete(line);

  return ERROR_NONE;
}

Errors Slave_executeCommand(const SlaveHost *slaveHost,
                            StringMap       resultMap,
                            const char      *format,
                            ...
                           )
{
  va_list  arguments;
  Errors   error;

  assert(slaveHost != NULL);

  va_start(arguments,format);
  error = Slave_vexecuteCommand(slaveHost,resultMap,format,arguments);
  va_end(arguments);

  return error;
}

Errors Slave_jobStart(const SlaveHost                 *slaveHost,
                      ConstString                     name,
                      ConstString                     jobUUID,
                      ConstString                     scheduleUUID,
                      ConstString                     storageName,
                      const EntryList                 *includeEntryList,
                      const PatternList               *excludePatternList,
                      MountList                       *mountList,
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
  MountNode       *mountNode;
  DeltaSourceNode *deltaSourceNode;
//  bool            quitFlag;

UNUSED_VARIABLE(scheduleUUID);
UNUSED_VARIABLE(archiveType);
UNUSED_VARIABLE(scheduleTitle);
UNUSED_VARIABLE(scheduleCustomText);
UNUSED_VARIABLE(storageRequestVolumeFunction);
UNUSED_VARIABLE(storageRequestVolumeUserData);
error = ERROR_STILL_NOT_IMPLEMENTED;

  assert(slaveHost != NULL);
  assert(jobUUID != NULL);

  // init variables
  s = String_new();

  error = ERROR_NONE;

  // create temporary job
fprintf(stderr,"%s, %d: Slave_executeCommand\n",__FILE__,__LINE__);
  error = Slave_executeCommand(slaveHost,NULL,"JOB_NEW name=%'S jobUUID=%S master=%'S",name,jobUUID,Network_getHostName(s));
  if (error != ERROR_NONE)
  {
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
    return error;
  }

  // set options
  error = ERROR_NONE;
/*EntryNode

  CONFIG_STRUCT_VALUE_STRING   ("ssh-public-key",          JobNode,jobOptions.sshServer.publicKeyFileName  ),
  CONFIG_STRUCT_VALUE_STRING   ("ssh-private-key",         JobNode,jobOptions.sshServer.privateKeyFileName ),
*/
  if (error == ERROR_NONE) error = Slave_setJobOptionString   (slaveHost,jobUUID,"archive-name",storageName);
  if (error == ERROR_NONE) error = Slave_setJobOptionCString  (slaveHost,
                                                               jobUUID,
                                                               "archive-type",
                                                               ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,jobOptions->archiveType,NULL)
                                                              );
  if (error == ERROR_NONE) error = Slave_setJobOptionString   (slaveHost,jobUUID,"incremental-list-file",  jobOptions->incrementalListFileName     );
  if (error == ERROR_NONE) error = Slave_setJobOptionInteger64(slaveHost,jobUUID,"archive-part-size",      jobOptions->archivePartSize             );
//  if (error == ERROR_NONE) error = Slave_setJobOptionInt(slaveHost,jobUUID,"directory-strip",jobOptions->directoryStripCount);
//  if (error == ERROR_NONE) error = Slave_setJobOptionString(slaveHost,jobUUID,"destination",jobOptions->destination);
//  if (error == ERROR_NONE) error = Slave_setJobOptionString(slaveHost,jobUUID,"owner",jobOptions->owner);
//  if (error == ERROR_NONE) error = Slave_setJobOptionString   (slaveHost,jobUUID,"pattern-type",           jobOptions->pattern-type                );
  if (error == ERROR_NONE) error = Slave_setJobOptionString   (slaveHost,
                                                               jobUUID,
                                                               "compress-algorithm",
                                                               String_format(String_clear(s),
                                                                             "%s+%s",
                                                                             ConfigValue_selectToString(CONFIG_VALUE_COMPRESS_ALGORITHMS,jobOptions->compressAlgorithms.delta,NULL),
                                                                             ConfigValue_selectToString(CONFIG_VALUE_COMPRESS_ALGORITHMS,jobOptions->compressAlgorithms.byte ,NULL)
                                                                            )
                                                              );
//TODO
  if (error == ERROR_NONE) error = Slave_setJobOptionCString  (slaveHost,
                                                               jobUUID,
                                                               "crypt-algorithm",
                                                               ConfigValue_selectToString(CONFIG_VALUE_CRYPT_ALGORITHMS,jobOptions->cryptAlgorithms[0],NULL)
                                                              );
  if (error == ERROR_NONE) error = Slave_setJobOptionCString  (slaveHost,
                                                               jobUUID,
                                                               "crypt-type",
                                                               ConfigValue_selectToString(CONFIG_VALUE_CRYPT_TYPES,jobOptions->cryptType,NULL)
                                                              );
  if (error == ERROR_NONE) error = Slave_setJobOptionCString  (slaveHost,
                                                               jobUUID,
                                                               "crypt-password-mode",
                                                               ConfigValue_selectToString(CONFIG_VALUE_PASSWORD_MODES,jobOptions->cryptPasswordMode,NULL)
                                                              );
  if (error == ERROR_NONE) error = Slave_setJobOptionPassword (slaveHost,jobUUID,"crypt-password",         jobOptions->cryptPassword               );
//TODO
#ifndef WERROR
#warning TODO
#endif
//  if (error == ERROR_NONE) error = Slave_setJobOptionString   (slaveHost,jobUUID,"crypt-public-key",       jobOptions->cryptPublicKeyFileName      );
  if (error == ERROR_NONE) error = Slave_setJobOptionString   (slaveHost,jobUUID,"pre-command",            jobOptions->preProcessScript            );
  if (error == ERROR_NONE) error = Slave_setJobOptionString   (slaveHost,jobUUID,"post-command",           jobOptions->postProcessScript           );
  if (error == ERROR_NONE) error = Slave_setJobOptionString   (slaveHost,jobUUID,"ftp-login-name",         jobOptions->ftpServer.loginName         );
  if (error == ERROR_NONE) error = Slave_setJobOptionPassword (slaveHost,jobUUID,"ftp-password",           jobOptions->ftpServer.password          );
  if (error == ERROR_NONE) error = Slave_setJobOptionInteger  (slaveHost,jobUUID,"ssh-port",               jobOptions->sshServer.port              );
  if (error == ERROR_NONE) error = Slave_setJobOptionString   (slaveHost,jobUUID,"ssh-login-name",         jobOptions->sshServer.loginName         );
  if (error == ERROR_NONE) error = Slave_setJobOptionPassword (slaveHost,jobUUID,"ssh-password",           jobOptions->sshServer.password          );
  if (error == ERROR_NONE) error = Slave_setJobOptionString   (slaveHost,
                                                                jobUUID,
                                                                "ssh-public-key-data",
                                                                Misc_base64Encode(s,jobOptions->sshServer.publicKey.data,jobOptions->sshServer.publicKey.length)
                                                               );
  if (error == ERROR_NONE) error = Slave_setJobOptionString   (slaveHost,
                                                               jobUUID,
                                                               "ssh-private-key-data",
                                                               Misc_base64Encode(s,jobOptions->sshServer.privateKey.data,jobOptions->sshServer.privateKey.length)
                                                              );
  if (error == ERROR_NONE) error = Slave_setJobOptionInteger64(slaveHost,jobUUID,"volume-size",            jobOptions->volumeSize                  );
  if (error == ERROR_NONE) error = Slave_setJobOptionBoolean  (slaveHost,jobUUID,"ecc",                    jobOptions->errorCorrectionCodesFlag    );
  if (error == ERROR_NONE) error = Slave_setJobOptionBoolean  (slaveHost,jobUUID,"skip-unreadable",        jobOptions->skipUnreadableFlag          );
  if (error == ERROR_NONE) error = Slave_setJobOptionBoolean  (slaveHost,jobUUID,"raw-images",             jobOptions->rawImagesFlag               );
  if (error == ERROR_NONE) error = Slave_setJobOptionCString  (slaveHost,jobUUID,
                                                               "archive-file-mode",
                                                               ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_FILE_MODES,jobOptions->archiveFileMode,NULL)
                                                              );
  if (error == ERROR_NONE) error = Slave_setJobOptionBoolean  (slaveHost,jobUUID,"overwrite-files",        jobOptions->overwriteEntriesFlag        );
  if (error == ERROR_NONE) error = Slave_setJobOptionBoolean  (slaveHost,jobUUID,"wait-first-volume",      jobOptions->waitFirstVolumeFlag         );

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveHost,NULL,"INCLUDE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(includeEntryList,entryNode)
  {
    switch (entryNode->type)
    {
      case ENTRY_TYPE_FILE :   entryTypeText = "FILE";    break;
      case ENTRY_TYPE_IMAGE:   entryTypeText = "IMAGE";   break;
      case ENTRY_TYPE_UNKNOWN:
      default:                 entryTypeText = "UNKNOWN"; break;
    }
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveHost,
                                                          NULL,
                                                          "INCLUDE_LIST_ADD jobUUID=%S entryType=%s patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          entryTypeText,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,entryNode->patternType,NULL),
                                                          entryNode->string
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveHost,NULL,"EXCLUDE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(excludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveHost,
                                                          NULL,
                                                          "EXCLUDE_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->patternType,NULL),
                                                          patternNode->string
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveHost,NULL,"MOUNT_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(mountList,mountNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveHost,
                                                          NULL,
                                                          "MOUNT_LIST_ADD jobUUID=%S name=%'S alwaysUnmount=%y",
                                                          jobUUID,
                                                          mountNode->name,
                                                          mountNode->alwaysUnmount
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveHost,NULL,"EXCLUDE_COMPRESS_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(compressExcludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveHost,
                                                          NULL,
                                                          "EXCLUDE_COMPRESS_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->patternType,NULL),
                                                          patternNode->string
                                                         );
  }

  if (error == ERROR_NONE) error = Slave_executeCommand(slaveHost,NULL,"SOURCE_LIST_CLEAR jobUUID=%S",jobUUID);
  LIST_ITERATE(deltaSourceList,deltaSourceNode)
  {
    if (error == ERROR_NONE) error = Slave_executeCommand(slaveHost,
                                                          NULL,
                                                          "SOURCE_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                          jobUUID,
                                                          ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,deltaSourceNode->patternType,NULL),
                                                          deltaSourceNode->storageName
                                                         );
  }
  if (error != ERROR_NONE)
  {
    (void)Slave_executeCommand(slaveHost,NULL,"JOB_DELETE jobUUID=%S",jobUUID);
    String_delete(s);
    return error;
  }
fprintf(stderr,"%s, %d: %d: Slave_jobStart %s\n",__FILE__,__LINE__,error,Error_getText(error));

  // start execute job
  error = Slave_executeCommand(slaveHost,NULL,"JOB_START jobUUID=%S archiveType=%s dryRun=%y",jobUUID,"FULL"/*archiveType*/,FALSE);
  if (error != ERROR_NONE)
  {
    (void)Slave_executeCommand(slaveHost,NULL,"JOB_DELETE jobUUID=%S",jobUUID);
    String_delete(s);
    return error;
  }

  // free resources
  String_delete(s);

  return ERROR_NONE;
}

Errors Slave_jobAbort(const SlaveHost *slaveHost,
                      ConstString     jobUUID
                     )
{
  Errors error;

  assert(slaveHost != NULL);
  assert(jobUUID != NULL);

  error = ERROR_NONE;

  // abort execute job
  error = Slave_executeCommand(slaveHost,NULL,"JOB_ABORT jobUUID=%S",jobUUID);
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
