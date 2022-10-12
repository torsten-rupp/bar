/***********************************************************************\
*
* $Revision: 4126 $
* $Date: 2015-09-19 10:57:45 +0200 (Sat, 19 Sep 2015) $
* $Author: torsten $
* Contents: Backup ARchiver connector functions
* Systems: all
*
\***********************************************************************/

#define __CONNECTOR_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <locale.h>
#include <time.h>
#include <signal.h>
#include <assert.h>

#include "common/autofree.h"
#include "common/global.h"
#include "common/lists.h"
#include "common/misc.h"
#include "common/network.h"
#include "common/patternlists.h"
#include "common/semaphores.h"
#include "common/semaphores.h"
#include "common/stringmaps.h"
#include "common/strings.h"

#include "entrylists.h"
#include "archive.h"
#include "storage.h"
#include "bar.h"
#include "jobs.h"
#include "server.h"

#include "connector.h"

/****************** Conditional compilation switches *******************/

#define _CONNECTOR_DEBUG

/***************************** Constants *******************************/
#define SLEEP_TIME_STATUS_UPDATE    2000  // [ms]

#define READ_TIMEOUT                ( 5LL*MS_PER_SECOND)
#define CONNECTOR_DEBUG_LEVEL       1
#define CONNECTOR_COMMAND_TIMEOUT   (10LL*MS_PER_MINUTE)

/***************************** Datatypes *******************************/

/***********************************************************************\
* Name   : ConnectorCommandFunction
* Purpose: connector command function
* Input  : clientInfo  - client info
*          indexHandle - index handle or NULL
*          id          - command id
*          argumentMap - argument map
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*ConnectorCommandFunction)(ConnectorInfo   *connectorInfo,
                                        IndexHandle     *indexHandle,
                                        uint            id,
                                        const StringMap argumentMap
                                       );

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define setConnectorState(...) __setConnectorState(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/
LOCAL void connectorThreadCode(ConnectorInfo *connectorInfo);

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif


/***********************************************************************\
* Name   : setConnectorState
* Purpose: set connector state
* Input  : connectorInfo - connector info
*          state         - new connector state
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE void setConnectorState(ConnectorInfo   *connectorInfo,
                                    ConnectorStates state
                                   )
#else /* not NDEBUG */
LOCAL_INLINE void __setConnectorState(const char      *__fileName__,
                                      ulong           __lineNb__,
                                      ConnectorInfo   *connectorInfo,
                                      ConnectorStates state
                                     )
#endif /* NDEBUG */
{
  assert(connectorInfo != NULL);

  #ifndef NDEBUG
    UNUSED_VARIABLE(__fileName__);
    UNUSED_VARIABLE(__lineNb__);
  #endif /* not NDEBUG */

  connectorInfo->state = state;
//{ const char *S[]={"NONE","CONNECTED","AUTHORIZED","DISCONNECTED"}; fprintf(stderr,"%s, %d: setConnectorState %p: %s,%d -> %s\n",__fileName__,__lineNb__,connectorInfo,(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK) ? String_cString(connectorInfo->io.network.name):"",(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK) ? connectorInfo->io.network.port:0,S[state]); }
}

/***********************************************************************\
* Name   : connectorConnect
* Purpose: connect to connector and get session id/public key
* Input  : connectorInfo - connector info
*          hostName      - host name
*          hostPort      - host port
*          tlsMode       - TLS mode; see TLS_MODES_...
*          caData        - TLS CA data or NULL
*          caLength      - TLS CA data length
*          cert          - TLS cerificate or NULL
*          certLength    - TLS cerificate data length
*          keyData       - key data or NULL
*          keyLength     - key data length
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors connectorConnect(ConnectorInfo *connectorInfo,
                              ConstString   hostName,
                              uint          hostPort,
                              TLSModes      tlsMode,
                              const void    *caData,
                              uint          caLength,
                              const void    *certData,
                              uint          certLength,
                              const void    *keyData,
                              uint          keyLength
                             )
{
  Errors error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  // connect network server i/o
  error = ServerIO_connectNetwork(&connectorInfo->io,
                                  hostName,
                                  (hostPort != 0) ? hostPort : DEFAULT_SERVER_PORT,
                                  tlsMode,
                                  caData,
                                  caLength,
                                  certData,
                                  certLength,
                                  keyData,
                                  keyLength
                                 );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // start connector thread
  if (!Thread_init(&connectorInfo->thread,"BAR connector",globalOptions.niceLevel,connectorThreadCode,connectorInfo))
  {
    HALT_FATAL_ERROR("Cannot initialize connector thread!");
  }

  // set state
  setConnectorState(connectorInfo,CONNECTOR_STATE_CONNECTED);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : connectorDisconnect
* Purpose: disconnect from connector
* Input  : connectorInfo - connector info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void connectorDisconnect(ConnectorInfo *connectorInfo)
{
  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // close storage
  if (connectorInfo->storageOpenFlag)
  {
    Storage_close(&connectorInfo->storageHandle);
    connectorInfo->storageOpenFlag = FALSE;
  }

  // stop connector thread
  Thread_quit(&connectorInfo->thread);
  if (!Thread_join(&connectorInfo->thread))
  {
    HALT_FATAL_ERROR("Cannot terminate connector thread!");
  }
  Thread_done(&connectorInfo->thread);

  // disconnect
  ServerIO_disconnect(&connectorInfo->io);

  // set state
  setConnectorState(connectorInfo,CONNECTOR_STATE_NONE);
}

/***********************************************************************\
* Name   : setJobOptionInteger
* Purpose: set job int value
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          value         - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors setJobOptionInteger(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, int value)
{
  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(name != NULL);

  return Connector_executeCommand(connectorInfo,
                                  CONNECTOR_DEBUG_LEVEL,
                                  CONNECTOR_COMMAND_TIMEOUT,
                                  CALLBACK_(NULL,NULL),
                                  "JOB_OPTION_SET jobUUID=%S name=%s value=%d",
                                  jobUUID,
                                  name,
                                  value
                                 );
}

/***********************************************************************\
* Name   : setJobOptionInteger64
* Purpose: set job int64 value
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          value         - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors setJobOptionInteger64(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, int64 value)
{
  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(name != NULL);

  return Connector_executeCommand(connectorInfo,
                                  CONNECTOR_DEBUG_LEVEL,
                                  CONNECTOR_COMMAND_TIMEOUT,
                                  CALLBACK_(NULL,NULL),
                                  "JOB_OPTION_SET jobUUID=%S name=%s value=%lld",
                                  jobUUID,
                                  name,
                                  value
                                 );
}

/***********************************************************************\
* Name   : setJobOptionBoolean
* Purpose: set job boolean value
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          value         - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors setJobOptionBoolean(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, bool value)
{
  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(name != NULL);

  return Connector_executeCommand(connectorInfo,
                                  CONNECTOR_DEBUG_LEVEL,
                                  CONNECTOR_COMMAND_TIMEOUT,
                                  CALLBACK_(NULL,NULL),
                                  "JOB_OPTION_SET jobUUID=%S name=%s value=%y",
                                  jobUUID,
                                  name,
                                  value
                                 );
}

/***********************************************************************\
* Name   : setJobOptionString
* Purpose: set job string value
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          value         - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors setJobOptionString(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, ConstString value)
{
  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(name != NULL);

  return Connector_executeCommand(connectorInfo,
                                  CONNECTOR_DEBUG_LEVEL,
                                  CONNECTOR_COMMAND_TIMEOUT,
                                  CALLBACK_(NULL,NULL),
                                  "JOB_OPTION_SET jobUUID=%S name=%s value=%'S",
                                  jobUUID,
                                  name,
                                  value
                                 );
}

/***********************************************************************\
* Name   : setJobOptionCString
* Purpose: set job c-string value
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          value         - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors setJobOptionCString(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, const char *value)
{
  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(name != NULL);

  return Connector_executeCommand(connectorInfo,
                                  CONNECTOR_DEBUG_LEVEL,
                                  CONNECTOR_COMMAND_TIMEOUT,
                                  CALLBACK_(NULL,NULL),
                                  "JOB_OPTION_SET jobUUID=%S name=%s value=%'s",
                                  jobUUID,
                                  name,
                                  value
                                 );
}

/***********************************************************************\
* Name   : setJobOptionPassword
* Purpose: set job password option
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          password      - password
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors setJobOptionPassword(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, const Password *password)
{
  Errors error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(name != NULL);

  error = ERROR_UNKNOWN;

  PASSWORD_DEPLOY_DO(plainPassword,password)
  {
    error = Connector_executeCommand(connectorInfo,
                                     CONNECTOR_DEBUG_LEVEL,
                                     CONNECTOR_COMMAND_TIMEOUT,
                                     CALLBACK_(NULL,NULL),
                                     "JOB_OPTION_SET jobUUID=%S name=%s value=%'s",
                                     jobUUID,
                                     name,
                                     plainPassword
                                    );
  }

  return error;
}

/***********************************************************************\
* Name   : transmitJob
* Purpose: transmit job config to slave
* Input  : connectorInfo      - connector info
*          name               - job name
*          jobUUID            - job UUID
*          scheduleUUID       - schedule UUID
*          storageName        - storage name
*          includeEntryList   - include entry list
*          excludePatternList - exclude pattern list
*          jobOptions         - job options
*          archiveType        - archive type
*          scheduleTitle      - schedule title
*          scheduleCustomText - schedule custom text
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors transmitJob(ConnectorInfo     *connectorInfo,
                         ConstString       name,
                         ConstString       jobUUID,
                         ConstString       scheduleUUID,
                         ConstString       storageName,
                         const EntryList   *includeEntryList,
                         const PatternList *excludePatternList,
                         const JobOptions  *jobOptions,
                         ArchiveTypes      archiveType,
                         ConstString       scheduleTitle,
                         ConstString       scheduleCustomText
                        )
{
  #define SET_OPTION_STRING(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = setJobOptionString(connectorInfo, \
                                                          jobUUID, \
                                                          name, \
                                                          value \
                                                         ); \
    } \
    while (0)
  #define SET_OPTION_CSTRING(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = setJobOptionCString(connectorInfo, \
                                                           jobUUID, \
                                                           name, \
                                                           value \
                                                          ); \
    } \
    while (0)
  #define SET_OPTION_PASSWORD(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = setJobOptionPassword(connectorInfo, \
                                                            jobUUID, \
                                                            name, \
                                                            value \
                                                           ); \
    } \
    while (0)
  #define SET_OPTION_INTEGER(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = setJobOptionInteger(connectorInfo, \
                                                           jobUUID, \
                                                           name, \
                                                           value \
                                                          ); \
    } \
    while (0)
  #define SET_OPTION_INTEGER64(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = setJobOptionInteger64(connectorInfo, \
                                                             jobUUID, \
                                                             name, \
                                                             value \
                                                            ); \
    } \
    while (0)
  #define SET_OPTION_BOOLEAN(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = setJobOptionBoolean(connectorInfo, \
                                                           jobUUID, \
                                                           name, \
                                                           value \
                                                          ); \
    } \
    while (0)

  String          s;
  Errors          error;
  EntryNode       *entryNode;
  const char      *entryTypeText;
  PatternNode     *patternNode;
  MountNode       *mountNode;
  DeltaSourceNode *deltaSourceNode;

UNUSED_VARIABLE(scheduleUUID);
UNUSED_VARIABLE(archiveType);
UNUSED_VARIABLE(scheduleTitle);
UNUSED_VARIABLE(scheduleCustomText);

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(jobUUID != NULL);

  // init variables
  s = String_new();

  // create temporary job
  error = Connector_executeCommand(connectorInfo,
                                   CONNECTOR_DEBUG_LEVEL,
                                   CONNECTOR_COMMAND_TIMEOUT,
                                   CALLBACK_(NULL,NULL),
                                   "JOB_NEW name=%'S jobUUID=%S scheduleUUID=%S master=%'S",
                                   name,
                                   jobUUID,
                                   scheduleUUID,
                                   Network_getHostName(s)
                                  );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // set options
  error = ERROR_NONE;
  SET_OPTION_STRING   ("archive-name",              storageName);
  SET_OPTION_CSTRING  ("archive-type",              ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,jobOptions->archiveType,NULL));

  SET_OPTION_STRING   ("incremental-list-file",     jobOptions->incrementalListFileName);

  SET_OPTION_INTEGER64("archive-part-size",         jobOptions->archivePartSize);

//  SET_OPTION_INTEGER  ("directory-strip",           jobOptions->directoryStripCount);
//  SET_OPTION_STRING   ("destination",               jobOptions->destination);
//  SET_OPTION_STRING   ("owner",                     jobOptions->owner);

  SET_OPTION_CSTRING  ("pattern-type",              ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,jobOptions->patternType,NULL));

  SET_OPTION_STRING   ("compress-algorithm",        String_format(s,
                                                                  "%s+%s",
                                                                  Compress_algorithmToString(jobOptions->compressAlgorithms.delta,NULL),
                                                                  Compress_algorithmToString(jobOptions->compressAlgorithms.byte, NULL)
                                                                 )
                      );
  SET_OPTION_CSTRING  ("crypt-algorithm",           Crypt_algorithmToString(jobOptions->cryptAlgorithms[0],NULL));
  SET_OPTION_CSTRING  ("crypt-type",                ConfigValue_selectToString(CONFIG_VALUE_CRYPT_TYPES,
                                                                               Crypt_isEncrypted(jobOptions->cryptAlgorithms[0]) ? jobOptions->cryptType : CRYPT_TYPE_NONE,
                                                                               NULL
                                                                              )
                      );
  SET_OPTION_CSTRING  ("crypt-password-mode",       ConfigValue_selectToString(CONFIG_VALUE_PASSWORD_MODES,jobOptions->cryptPasswordMode,NULL));
  SET_OPTION_PASSWORD ("crypt-password",            &jobOptions->cryptPassword              );
  SET_OPTION_STRING   ("crypt-public-key",          Misc_base64Encode(s,jobOptions->cryptPublicKey.data,jobOptions->cryptPublicKey.length));

  SET_OPTION_STRING   ("pre-command",               jobOptions->slavePreProcessScript       );
  SET_OPTION_STRING   ("post-command",              jobOptions->slavePostProcessScript      );

  SET_OPTION_BOOLEAN  ("storage-on-master",         jobOptions->storageOnMasterFlag);

  SET_OPTION_STRING   ("ftp-login-name",            jobOptions->ftpServer.loginName         );
  SET_OPTION_PASSWORD ("ftp-password",              &jobOptions->ftpServer.password         );

  SET_OPTION_INTEGER  ("ssh-port",                  jobOptions->sshServer.port              );
  SET_OPTION_STRING   ("ssh-login-name",            jobOptions->sshServer.loginName         );
  SET_OPTION_PASSWORD ("ssh-password",              &jobOptions->sshServer.password         );
  SET_OPTION_STRING   ("ssh-public-key",            Misc_base64Encode(s,jobOptions->sshServer.publicKey.data,jobOptions->sshServer.publicKey.length));
  SET_OPTION_STRING   ("ssh-private-key",           Misc_base64Encode(s,jobOptions->sshServer.privateKey.data,jobOptions->sshServer.privateKey.length));

  SET_OPTION_INTEGER64("max-storage-size",          jobOptions->maxStorageSize);
  SET_OPTION_BOOLEAN  ("test-created-archives",     jobOptions->testCreatedArchivesFlag);

  SET_OPTION_INTEGER64("volume-size",               jobOptions->volumeSize                  );
  SET_OPTION_BOOLEAN  ("ecc",                       jobOptions->errorCorrectionCodesFlag);
  SET_OPTION_BOOLEAN  ("blank",                     jobOptions->blankFlag);

  SET_OPTION_BOOLEAN  ("skip-unreadable",           jobOptions->skipUnreadableFlag);
  SET_OPTION_BOOLEAN  ("no-stop-on-error",          jobOptions->noStopOnErrorFlag);
  SET_OPTION_BOOLEAN  ("no-stop-on-attribute-error",jobOptions->noStopOnAttributeErrorFlag);
  SET_OPTION_BOOLEAN  ("raw-images",                jobOptions->rawImagesFlag);
  SET_OPTION_CSTRING  ("archive-file-mode",         ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_FILE_MODES,jobOptions->archiveFileMode,NULL));
  SET_OPTION_CSTRING  ("restore-entry-mode",        ConfigValue_selectToString(CONFIG_VALUE_RESTORE_ENTRY_MODES,jobOptions->restoreEntryMode,NULL));
  SET_OPTION_BOOLEAN  ("wait-first-volume",         jobOptions->waitFirstVolumeFlag         );

  SET_OPTION_STRING   ("comment",                   jobOptions->comment                     );

  // set lists
  if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                            CONNECTOR_DEBUG_LEVEL,
                                                            CONNECTOR_COMMAND_TIMEOUT,
                                                            CALLBACK_(NULL,NULL),
                                                            "INCLUDE_LIST_CLEAR jobUUID=%S",
                                                            jobUUID
                                                           );
  LIST_ITERATE(includeEntryList,entryNode)
  {
    switch (entryNode->type)
    {
      case ENTRY_TYPE_FILE :   entryTypeText = "FILE";    break;
      case ENTRY_TYPE_IMAGE:   entryTypeText = "IMAGE";   break;
      case ENTRY_TYPE_UNKNOWN:
      default:                 entryTypeText = "UNKNOWN"; break;
    }
    if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                              CONNECTOR_DEBUG_LEVEL,
                                                              CONNECTOR_COMMAND_TIMEOUT,
                                                              CALLBACK_(NULL,NULL),
                                                              "INCLUDE_LIST_ADD jobUUID=%S entryType=%s patternType=%s pattern=%'S",
                                                              jobUUID,
                                                              entryTypeText,
                                                              ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,entryNode->patternType,NULL),
                                                              entryNode->string
                                                             );
  }

  if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                            CONNECTOR_DEBUG_LEVEL,
                                                            CONNECTOR_COMMAND_TIMEOUT,
                                                            CALLBACK_(NULL,NULL),
                                                            "EXCLUDE_LIST_CLEAR jobUUID=%S",
                                                            jobUUID
                                                           );
  LIST_ITERATE(excludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                              CONNECTOR_DEBUG_LEVEL,
                                                              CONNECTOR_COMMAND_TIMEOUT,
                                                              CALLBACK_(NULL,NULL),
                                                              "EXCLUDE_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                              jobUUID,
                                                              ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->pattern.type,NULL),
                                                              patternNode->string
                                                             );
  }

  if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                            CONNECTOR_DEBUG_LEVEL,
                                                            CONNECTOR_COMMAND_TIMEOUT,
                                                            CALLBACK_(NULL,NULL),
                                                            "MOUNT_LIST_CLEAR jobUUID=%S",
                                                            jobUUID
                                                           );
  LIST_ITERATE(&jobOptions->mountList,mountNode)
  {
    if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                              CONNECTOR_DEBUG_LEVEL,
                                                              CONNECTOR_COMMAND_TIMEOUT,
                                                              CALLBACK_(NULL,NULL),
                                                              "MOUNT_LIST_ADD jobUUID=%S name=%'S",
                                                              jobUUID,
                                                              mountNode->name
                                                             );
  }

  if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                            CONNECTOR_DEBUG_LEVEL,
                                                            CONNECTOR_COMMAND_TIMEOUT,
                                                            CALLBACK_(NULL,NULL),
                                                            "EXCLUDE_COMPRESS_LIST_CLEAR jobUUID=%S",
                                                            jobUUID
                                                           );
  LIST_ITERATE(&jobOptions->compressExcludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                              CONNECTOR_DEBUG_LEVEL,
                                                              CONNECTOR_COMMAND_TIMEOUT,
                                                              CALLBACK_(NULL,NULL),
                                                              "EXCLUDE_COMPRESS_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                              jobUUID,
                                                              ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->pattern.type,NULL),
                                                              patternNode->string
                                                             );
  }

  if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                            CONNECTOR_DEBUG_LEVEL,
                                                            CONNECTOR_COMMAND_TIMEOUT,
                                                            CALLBACK_(NULL,NULL),
                                                            "SOURCE_LIST_CLEAR jobUUID=%S",
                                                            jobUUID
                                                           );
  LIST_ITERATE(&jobOptions->deltaSourceList,deltaSourceNode)
  {
    if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                              CONNECTOR_DEBUG_LEVEL,
                                                              CONNECTOR_COMMAND_TIMEOUT,
                                                              CALLBACK_(NULL,NULL),
                                                              "SOURCE_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                              jobUUID,
                                                              ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,deltaSourceNode->patternType,NULL),
                                                              deltaSourceNode->storageName
                                                             );
  }

  // check for error
  if (error != ERROR_NONE)
  {
    (void)Connector_executeCommand(connectorInfo,
                                   CONNECTOR_DEBUG_LEVEL,
                                   CONNECTOR_COMMAND_TIMEOUT,
                                   CALLBACK_(NULL,NULL),
                                   "JOB_DELETE jobUUID=%S",
                                   jobUUID
                                  );
    String_delete(s);
    return error;
  }

  // free resources
  String_delete(s);

  return ERROR_NONE;

  #undef SET_OPTION_BOOLEAN
  #undef SET_OPTION_INTEGER64
  #undef SET_OPTION_INTEGER
  #undef SET_OPTION_PASSWORD
  #undef SET_OPTION_CSTRING
  #undef SET_OPTION_STRING
}

/***********************************************************************\
* Name   : sendResult
* Purpose: send command result
* Input  : connectorInfo - connector info
*          id            - command id
*          completedFlag - TRUE iff completed
*          error         - error code
*          format        - command format string
*          ...           - optional arguments for command format string
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void sendResult(ConnectorInfo  *connectorInfo,
                      uint           id,
                      bool           completedFlag,
                      Errors         error,
                      const char     *format,
                      ...
                     )
{
  va_list arguments;

  va_start(arguments,format);
  ServerIO_vsendResult(&connectorInfo->io,id,completedFlag,error,format,arguments);
  va_end(arguments);

  #ifdef CONNECTOR_DEBUG
    fprintf(stderr,"DEBUG connector sent result: %u %u %u ",id,completedFlag ? 1 : 0, Error_getCode(error));
    va_start(arguments,format);
    vfprintf(stderr,format,arguments);
    va_end(arguments);
    fprintf(stderr,"\n");
  #endif
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : connectorCommand_storageCreate
* Purpose: create storage
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            archiveName=<name>
*            archiveSize=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_storageCreate(ConnectorInfo   *connectorInfo,
                                          IndexHandle     *indexHandle,
                                          uint            id,
                                          const StringMap argumentMap
                                         )
{
  String archiveName;
  uint64 archiveSize;
  Errors error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  UNUSED_VARIABLE(indexHandle);

  // get archive name, archive size
  archiveName = String_new();
  if (!StringMap_getString(argumentMap,"archiveName",archiveName,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveName=<name>");
    String_delete(archiveName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"archiveSize",&archiveSize,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveSize=<n>");
    String_delete(archiveName);
    return;
  }

  // check if storage initialized
  if (!connectorInfo->storageInitFlag)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_INIT_STORAGE,"create storage");
    String_delete(archiveName);
    return;
  }

  // create storage
  error = Storage_create(&connectorInfo->storageHandle,
                         &connectorInfo->storageInfo,
                         archiveName,
                         archiveSize,
                         FALSE  // forceFlag
                        );
  if (error != ERROR_NONE)
  {
    sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
    String_delete(archiveName);
    return;
  }
  connectorInfo->storageOpenFlag = TRUE;

  // send result
  sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(archiveName);
}

/***********************************************************************\
* Name   : connectorCommand_storageWrite
* Purpose: write storage
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            offset=<n>
*            length=<n>
*            data=<base64 encoded data>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_storageWrite(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint64 offset;
  uint   length;
  String data;
  void   *buffer;
  Errors error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  UNUSED_VARIABLE(indexHandle);

  // get offset, length, data
  if (!StringMap_getUInt64(argumentMap,"offset",&offset,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"offset=<n>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"length",&length,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"length=<n>");
    return;
  }
  data = String_new();
  if (!StringMap_getString(argumentMap,"data",data,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"data=<data>");
    String_delete(data);
    return;
  }

  // check if storage is open
  if (!connectorInfo->storageOpenFlag)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_INVALID_STORAGE,"storage not open");
    String_delete(data);
    return;
  }

  // decode data
  buffer = malloc(length);
  if (buffer == NULL)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_INSUFFICIENT_MEMORY,"insufficient memory");
    String_delete(data);
    return;
  }
  if (!Misc_base64Decode(buffer,length,NULL,data,STRING_BEGIN))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_INSUFFICIENT_MEMORY,"decode base64 data fail");
    String_delete(data);
    return;
  }

  // write to storage
  error = Storage_seek(&connectorInfo->storageHandle,offset);
  if (error != ERROR_NONE)
  {
    sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
    free(buffer);
    String_delete(data);
    return;
  }
  error = Storage_write(&connectorInfo->storageHandle,buffer,length);
  if (error != ERROR_NONE)
  {
    sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
    free(buffer);
    String_delete(data);
    return;
  }

  // send result
  sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  free(buffer);
  String_delete(data);
}

/***********************************************************************\
* Name   : connectorCommand_storageClose
* Purpose: close storage
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_storageClose(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint64 archiveSize;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // check if storage initialized
  if (!connectorInfo->storageInitFlag)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_INIT_STORAGE,"storage close");
    return;
  }

  // get archive size
  archiveSize = Storage_getSize(&connectorInfo->storageHandle);
UNUSED_VARIABLE(archiveSize);
UNUSED_VARIABLE(indexHandle);
UNUSED_VARIABLE(argumentMap);

  // close storage
  if (connectorInfo->storageOpenFlag)
  {
    Storage_close(&connectorInfo->storageHandle);
    connectorInfo->storageOpenFlag = FALSE;
  }

//TODO: index

  // send result
  sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : connectorCommand_storageExists
* Purpose: check if storage exists
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments: archiveName=<name>
*          Result: existsFlag=yes|no
\***********************************************************************/

LOCAL void connectorCommand_storageExists(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String archiveName;
  bool existsFlags;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  UNUSED_VARIABLE(indexHandle);

  // get archive name, archive size
  archiveName = String_new();
  if (!StringMap_getString(argumentMap,"archiveName",archiveName,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveName=<name>");
    String_delete(archiveName);
    return;
  }

  // check if storage initialized
  if (!connectorInfo->storageInitFlag)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_INIT_STORAGE,"storage exists");
    return;
  }

  // check if exists
  existsFlags = Storage_exists(&connectorInfo->storageInfo,archiveName);

  // send result
  sendResult(connectorInfo,id,TRUE,ERROR_NONE,"existsFlag=%y",existsFlags);
}

/***********************************************************************\
* Name   : connectorCommand_indexFindUUID
* Purpose: find index UUID
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<text>
*            scheduleUUUID=<text>
*          Result:
*            uuidId=<n>
*            executionCountNormal=<n>
*            executionCountFull=<n>
*            executionCountIncremental=<n>
*            executionCountContinuous=<n>
*            averageDurationNormal=<n>
*            averageDurationFull=<n>
*            averageDurationIncremental=<n>
*            averageDurationDifferential=<n>
*            averageDurationContinuous=<n>
*            totalEntityCount=<n>
*            totalStorageCount=<n>
*            totalStorageSize=<n>
*            totalEntryCount=<n>
*            totalEntrySize=<n>
\***********************************************************************/

LOCAL void connectorCommand_indexFindUUID(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  Errors       error;
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUUID,MISC_UUID_STRING_LENGTH);
  IndexId      uuidId;
  uint         executionCountNormal,executionCountFull,executionCountIncremental,executionCountDifferential,executionCountContinuous;
  uint64       averageDurationNormal,averageDurationFull,averageDurationIncremental,averageDurationDifferential,averageDurationContinuous;
  uint         totalEntityCount;
  uint         totalStorageCount;
  uint64       totalStorageSize;
  uint         totalEntryCount;
  uint64       totalEntrySize;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get jobUUID, scheduleUUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<text>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUUID,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"scheduleUUID=<text>");
    return;
  }

  if (indexHandle != NULL)
  {
    // find job data
    error = Index_findUUID(indexHandle,
                           String_cString(jobUUID),
                           String_cString(scheduleUUUID),
                           &uuidId,
                           &executionCountNormal,
                           &executionCountFull,
                           &executionCountIncremental,
                           &executionCountDifferential,
                           &executionCountContinuous,
                           &averageDurationNormal,
                           &averageDurationFull,
                           &averageDurationIncremental,
                           &averageDurationDifferential,
                           &averageDurationContinuous,
                           &totalEntityCount,
                           &totalStorageCount,
                           &totalStorageSize,
                           &totalEntryCount,
                           &totalEntrySize
                          );
    if (error == ERROR_NONE)
    {
      sendResult(connectorInfo,
                 id,
                 TRUE,
                 ERROR_NONE,
                 "uuidId=%lld executionCountNormal=%u executionCountFull=%u executionCountIncremental=%u executionCountDifferential=%u executionCountContinuous=%u averageDurationNormal=%"PRIu64" averageDurationFull=%"PRIu64" averageDurationIncremental=%"PRIu64" averageDurationDifferential=%"PRIu64" averageDurationContinuous=%"PRIu64" totalEntityCount=%u totalStorageCount=%u totalStorageSize=%"PRIu64" totalEntryCount=%u totalEntrySize=%"PRIu64,
                 uuidId,
                 executionCountNormal,
                 executionCountFull,
                 executionCountIncremental,
                 executionCountDifferential,
                 executionCountContinuous,
                 averageDurationNormal,
                 averageDurationFull,
                 averageDurationIncremental,
                 averageDurationDifferential,
                 averageDurationContinuous,
                 totalEntityCount,
                 totalStorageCount,
                 totalStorageSize,
                 totalEntryCount,
                 totalEntrySize
                );
    }
    else
    {
      sendResult(connectorInfo,
                 id,
                 TRUE,
                 error,
                 ""
                );
    }
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }
}

/***********************************************************************\
* Name   : connectorCommand_indexNewUUID
* Purpose: add new index UUID
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<text>
*          Result:
*            uuidId=<n>
\***********************************************************************/

LOCAL void connectorCommand_indexNewUUID(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  Errors       error;
  IndexId      uuidId;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get jobUUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<text>");
    return;
  }

  if (indexHandle != NULL)
  {
    // create new UUID
    error = Index_newUUID(indexHandle,
                          String_cString(jobUUID),
                          &uuidId
                         );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"uuidId=%lld",uuidId);
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
}

/***********************************************************************\
* Name   : connectorCommand_indexNewEntity
* Purpose: create new index entity
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            scheduleUUID=<uuid>
*            hostName=<name>
*            userName=<name>
*            archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL
*            createdDateTime=<n>
*            locked=yes|no
*          Result:
*            entityId=<n>
\***********************************************************************/

LOCAL void connectorCommand_indexNewEntity(ConnectorInfo   *connectorInfo,
                                           IndexHandle     *indexHandle,
                                           uint            id,
                                           const StringMap argumentMap
                                          )
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String       hostName,userName;
  ArchiveTypes archiveType;
  uint64       createdDateTime;
  bool         locked;
  Errors       error;
  IndexId      entityId;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get jobUUID, scheduleUUID, hostName, userName, archiveType, createdDateTime, locked
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<text>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"scheduleUUID=<text>");
    return;
  }
  hostName = String_new();
  if (!StringMap_getString(argumentMap,"hostName",hostName,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"hostName=<name>");
    return;
  }
  userName = String_new();
  if (!StringMap_getString(argumentMap,"userName",userName,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"userName=<name>");
    String_delete(hostName);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,CALLBACK_((StringMapParseEnumFunction)Archive_parseType,NULL),ARCHIVE_TYPE_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    String_delete(userName);
    String_delete(hostName);
    return;
  }
  StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL);
  StringMap_getBool(argumentMap,"locked",&locked,FALSE);

  if (indexHandle != NULL)
  {
    // create new entity
    error = Index_newEntity(indexHandle,
                            String_cString(jobUUID),
                            String_cString(scheduleUUID),
                            String_cString(hostName),
                            String_cString(userName),
                            archiveType,
                            createdDateTime,
                            locked,
                            &entityId
                           );
    if (error != ERROR_NONE)
    {
      String_delete(userName);
      String_delete(hostName);
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"entityId=%lld",entityId);
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  String_delete(userName);
  String_delete(hostName);
}

/***********************************************************************\
* Name   : connectorCommand_indexNewStorage
* Purpose: create new index storage
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            uuidId=<n>
*            entityId=<n>
*            storageName=<name>
*            dateTime=<n>
*            size=<n>
*            indexState=<n>
*            indexMode=<n>
*          Result:
*            storageId=<n>
\***********************************************************************/

LOCAL void connectorCommand_indexNewStorage(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId      uuidId,entityId;
  String       storageName;
  uint64       dateTime;
  uint64       size;
  IndexStates  indexState;
  IndexModes   indexMode;
  Errors       error;
  IndexId      storageId;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get uuidId, entityId, storageName, dateTime, size, indexMode, indexState
  if (!StringMap_getInt64(argumentMap,"uuidId",&uuidId,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"uuidId=<n>");
    return;
  }
  if (!StringMap_getInt64(argumentMap,"entityId",&entityId,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<n>");
    return;
  }
  storageName = String_new();
  if (!StringMap_getString(argumentMap,"storageName",storageName,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageName=<name>");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"dateTime",&dateTime,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"dateTime=<n>");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"size=<n>");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"indexState",&indexState,CALLBACK_((StringMapParseEnumFunction)Index_parseState,NULL),INDEX_STATE_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexState=NONE|OK|CREATE|UPDATE_REQUESTED|UPDATE|ERROR");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"indexMode",&indexMode,CALLBACK_((StringMapParseEnumFunction)Index_parseMode,NULL),INDEX_MODE_MANUAL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexMode=MANUAL|AUTO");
    String_delete(storageName);
    return;
  }

  if (indexHandle != NULL)
  {
    // create new storage
    error = Index_newStorage(indexHandle,
                             uuidId,
                             entityId,
                             connectorInfo->io.network.name,
                             NULL,  // userName
                             storageName,
                             dateTime,
                             size,
                             indexState,
                             indexMode,
                             &storageId
                            );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      String_delete(storageName);
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"storageId=%lld",storageId);
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  String_delete(storageName);
}

/***********************************************************************\
* Name   : connectorCommand_indexAddFile
* Purpose: add index file entry
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            uuidId=<n>
*            entityId=<n>
*            storageId=<n>
*            name=<name>
*            size=<n>
*            timeLastAccess=<n>
*            timeModified=<n>
*            timeLastChanged=<n>
*            userId=<n>
*            groupId=<n>
*            permission=<n>
*            fragmentOffset=<n>
*            fragmentSize=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexAddFile(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId uuidId,entityId,storageId;
  String  name;
  uint64  size;
  uint64  timeLastAccess;
  uint64  timeModified;
  uint64  timeLastChanged;
  uint32  userId;
  uint32  groupId;
  uint32  permission;
  uint64  fragmentOffset;
  uint64  fragmentSize;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get uuidId, entityId, storageId, name, size, timeLastAccess, timeModified, timeLastChanged, userId, groupId, permission, fragmentOffset, fragmentSize
  if (!StringMap_getInt64(argumentMap,"uuidId",&uuidId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"uuidId=<n>");
    return;
  }
  if (!StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<n>");
    return;
  }
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"not a storage index id %llx",storageId);
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"size=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastAccess=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeModified=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastChanged=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"userId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"groupId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"permission=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentOffset",&fragmentOffset,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"fragmentOffset=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentSize",&fragmentSize,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"fragmentSize=<n>");
    String_delete(name);
    return;
  }

  if (indexHandle != NULL)
  {
    // add index file entry
    error = Index_addFile(indexHandle,
                          uuidId,
                          entityId,
                          storageId,
                          name,
                          size,
                          timeLastAccess,
                          timeModified,
                          timeLastChanged,
                          userId,
                          groupId,
                          permission,
                          fragmentOffset,
                          fragmentSize
                         );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      String_delete(name);
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : connectorCommand_indexAddImage
* Purpose: add index file entry
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            uuidId=<n>
*            entityId=<n>
*            storageId=<n>
*            name=<name>
*            fileSystemType=<text>
*            size=<n>
*            timeLastAccess=<n>
*            timeModified=<n>
*            timeLastChanged=<n>
*            userId=<n>
*            groupId=<n>
*            permission=<n>
*            fragmentOffset=<n>
*            fragmentSize=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexAddImage(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId         uuidId,entityId,storageId;
  String          name;
  FileSystemTypes fileSystemType;
  uint64          size;
  uint            blockSize;
  uint64          blockOffset;
  uint64          blockCount;
  Errors          error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get uuidId, entityId, storageId, name, fileSystemType, size, blockSize, blockOffset, blockCount
  if (!StringMap_getInt64(argumentMap,"uuidId",&uuidId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"uuidId=<n>");
    return;
  }
  if (!StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<n>");
    return;
  }
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"not a storage index id %llx",storageId);
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"fileSystemType",&fileSystemType,CALLBACK_((StringMapParseEnumFunction)FileSystem_parseFileSystemType,NULL),FILE_SYSTEM_TYPE_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"fileSystemType=CHARACTER_DEVICE|BLOCK_DEVICE|FIFO|SOCKET|OTHER");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"size=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"blockSize",&blockSize,0L))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"blockSize=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"blockOffset",&blockOffset,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"blockOffset=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"blockCount",&blockCount,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"blockCount=<n>");
    String_delete(name);
    return;
  }

  if (indexHandle != NULL)
  {
    // add index image entry
    error = Index_addImage(indexHandle,
                           uuidId,
                           entityId,
                           storageId,
                           name,
                           fileSystemType,
                           size,
                           blockSize,
                           blockOffset,
                           blockCount
                          );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      String_delete(name);
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : connectorCommand_indexAddDirectory
* Purpose: add index file entry
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            uuidId=<n>
*            entityId=<n>
*            storageId=<n>
*            name=<name>
*            size=<n>
*            timeLastAccess=<n>
*            timeModified=<n>
*            timeLastChanged=<n>
*            userId=<n>
*            groupId=<n>
*            permission=<n>
*            fragmentOffset=<n>
*            fragmentSize=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexAddDirectory(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId uuidId,entityId,storageId;
  String  name;
  uint64  timeLastAccess;
  uint64  timeModified;
  uint64  timeLastChanged;
  uint32  userId;
  uint32  groupId;
  uint32  permission;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get uuidId, entityId, storageId, name, timeLastAccess, timeModified, timeLastChanged, userId, groupId, permission
  if (!StringMap_getInt64(argumentMap,"uuidId",&uuidId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"uuidId=<n>");
    return;
  }
  if (!StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<n>");
    return;
  }
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"not a storage index id %llx",storageId);
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastAccess=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeModified=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastChanged=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"userId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"groupId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"permission=<n>");
    String_delete(name);
    return;
  }

  if (indexHandle != NULL)
  {
    // add index directory entry
    error = Index_addDirectory(indexHandle,
                               uuidId,
                               entityId,
                               storageId,
                               name,
                               timeLastAccess,
                               timeModified,
                               timeLastChanged,
                               userId,
                               groupId,
                               permission
                              );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      String_delete(name);
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : connectorCommand_indexAddLink
* Purpose: add index file entry
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            uuidId=<n>
*            entityId=<n>
*            storageId=<n>
*            name=<name>
*            destinationName=<name>
*            size=<n>
*            timeLastAccess=<n>
*            timeModified=<n>
*            timeLastChanged=<n>
*            userId=<n>
*            groupId=<n>
*            permission=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexAddLink(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId uuidId,entityId,storageId;
  String  name;
  String  destinationName;
  uint64  timeLastAccess;
  uint64  timeModified;
  uint64  timeLastChanged;
  uint32  userId;
  uint32  groupId;
  uint32  permission;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get uuidId, entityId, storageId, name, destinationName, timeLastAccess, timeModified, timeLastChanged, userId, groupId, permission
  if (!StringMap_getInt64(argumentMap,"uuidId",&uuidId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"uuidId=<n>");
    return;
  }
  if (!StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<n>");
    return;
  }
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"not a storage index id %llx",storageId);
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  destinationName = String_new();
  if (!StringMap_getString(argumentMap,"destinationName",destinationName,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"destinationName=<name>");
    String_delete(destinationName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastAccess=<n>");
    String_delete(destinationName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeModified=<n>");
    String_delete(destinationName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastChanged=<n>");
    String_delete(destinationName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"userId=<n>");
    String_delete(destinationName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"groupId=<n>");
    String_delete(destinationName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"permission=<n>");
    String_delete(destinationName);
    String_delete(name);
    return;
  }

  if (indexHandle != NULL)
  {
    // add index link entry
    error = Index_addLink(indexHandle,
                          uuidId,
                          entityId,
                          storageId,
                          name,
                          destinationName,
                          timeLastAccess,
                          timeModified,
                          timeLastChanged,
                          userId,
                          groupId,
                          permission
                         );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      String_delete(destinationName);
      String_delete(name);
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  String_delete(destinationName);
  String_delete(name);
}

/***********************************************************************\
* Name   : connectorCommand_indexAddHardlink
* Purpose: add index file entry
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            uuidId=<n>
*            entityId=<n>
*            storageId=<n>
*            name=<name>
*            size=<n>
*            timeLastAccess=<n>
*            timeModified=<n>
*            timeLastChanged=<n>
*            userId=<n>
*            groupId=<n>
*            permission=<n>
*            fragmentOffset=<n>
*            fragmentSize=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexAddHardlink(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId uuidId,entityId,storageId;
  String  name;
  uint64  size;
  uint64  timeLastAccess;
  uint64  timeModified;
  uint64  timeLastChanged;
  uint32  userId;
  uint32  groupId;
  uint32  permission;
  uint64  fragmentOffset;
  uint64  fragmentSize;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get uuidId, entityId, storageId, name, size, timeLastAccess, timeModified, timeLastChanged, userId, groupId, permission, fragmentOffset, fragmentSize
  if (!StringMap_getInt64(argumentMap,"uuidId",&uuidId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"uuidId=<n>");
    return;
  }
  if (!StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<n>");
    return;
  }
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"not a storage index id %llx",storageId);
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"size=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastAccess=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeModified=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastChanged=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"userId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"groupId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"permission=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentOffset",&fragmentOffset,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"fragmentOffset=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentSize",&fragmentSize,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"fragmentSize=<n>");
    String_delete(name);
    return;
  }

  if (indexHandle != NULL)
  {
    // add index hardlink entry
    error = Index_addHardlink(indexHandle,
                              uuidId,
                              entityId,
                              storageId,
                              name,
                              size,
                              timeLastAccess,
                              timeModified,
                              timeLastChanged,
                              userId,
                              groupId,
                              permission,
                              fragmentOffset,
                              fragmentSize
                             );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      String_delete(name);
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : connectorCommand_indexAddSpecial
* Purpose: add index file entry
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            uuidId=<n>
*            entityId=<n>
*            storageId=<n>
*            fileName=<name>
*            size=<n>
*            timeLastAccess=<n>
*            timeModified=<n>
*            timeLastChanged=<n>
*            userId=<n>
*            groupId=<n>
*            permission=<n>
*            fragmentOffset=<n>
*            fragmentSize=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexAddSpecial(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId          uuidId,entityId,storageId;
  String           name;
  FileSpecialTypes specialType;
  uint64           timeLastAccess;
  uint64           timeModified;
  uint64           timeLastChanged;
  uint32           userId;
  uint32           groupId;
  uint32           permission;
  uint32           major,minor;
  Errors           error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get uuidId, entityId, storageId, name, specialType, timeLastAccess, timeModified, timeLastChanged, userId, groupId, permission, fragmentOffset, fragmentSize
  if (!StringMap_getInt64(argumentMap,"uuidId",&uuidId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"uuidId=<n>");
    return;
  }
  if (!StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<n>");
    return;
  }
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"not a storage index id %llx",storageId);
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"specialType",&specialType,CALLBACK_((StringMapParseEnumFunction)File_parseFileSpecialType,NULL),FILE_SPECIAL_TYPE_OTHER))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"specialType=CHARACTER_DEVICE|BLOCK_DEVICE|FIFO|SOCKET|OTHER");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastAccess=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeModified=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastChanged=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"userId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"groupId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"permission=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"major",&major,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"major=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"minor",&minor,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"minor=<n>");
    String_delete(name);
    return;
  }

  if (indexHandle != NULL)
  {
    // add index special entry
    error = Index_addSpecial(indexHandle,
                             uuidId,
                             entityId,
                             storageId,
                             name,
                             specialType,
                             timeLastAccess,
                             timeModified,
                             timeLastChanged,
                             userId,
                             groupId,
                             permission,
                             major,
                             minor
                            );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      String_delete(name);
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : connectorCommand_indexUUIDPrune
* Purpose: purge index UUID
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            uuidId=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexUUIDPurge(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId uuidId;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get uuidId
  if (!StringMap_getInt64(argumentMap,"uuidId",&uuidId,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"uuidId=<n>");
    return;
  }

  if (indexHandle != NULL)
  {
    // prune UUID
    error = Index_purgeUUID(indexHandle,uuidId);
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
}

/***********************************************************************\
* Name   : connectorCommand_indexUUIDPrune
* Purpose: prune index UUID
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            uuidId=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexUUIDPrune(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId uuidId;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get uuidId
  if (!StringMap_getInt64(argumentMap,"uuidId",&uuidId,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"uuidId=<n>");
    return;
  }

  if (indexHandle != NULL)
  {
    // prune UUID
    error = Index_pruneUUID(indexHandle,uuidId);
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
}

/***********************************************************************\
* Name   : connectorCommand_indexEntityPurge
* Purpose: purge index entity
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            entityId=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexEntityPurge(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId entityId;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get entityId
  if (!StringMap_getInt64(argumentMap,"entityId",&entityId,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<n>");
    return;
  }

  if (indexHandle != NULL)
  {
    // purge entity
    error = Index_purgeEntity(indexHandle,entityId);
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
}

/***********************************************************************\
* Name   : connectorCommand_indexEntityPrune
* Purpose: prune index entity
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            entityId=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexEntityPrune(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId entityId;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get entityId
  if (!StringMap_getInt64(argumentMap,"entityId",&entityId,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<n>");
    return;
  }

  if (indexHandle != NULL)
  {
    // prune entity
    error = Index_pruneEntity(indexHandle,entityId);
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
}

/***********************************************************************\
* Name   : connectorCommand_indexSetState
* Purpose: set index state
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            indexId=<n>
*            indexState=<state>
*            lastCheckedDateTime=<n>
*            errorMessage=<text>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexSetState(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId     indexId;
  IndexStates indexState;
  uint64      lastCheckedDateTime;
  String      errorMessage;
  Errors      error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get indexId, indexState, lastCheckedDateTime, errorMessage
  if (!StringMap_getInt64(argumentMap,"indexId",&indexId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexId=<n>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"indexState",&indexState,CALLBACK_((StringMapParseEnumFunction)Index_parseState,NULL),INDEX_STATE_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexState=NONE|OK|CREATE|UPDATE_REQUESTED|UPDATE|ERROR");
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"lastCheckedDateTime",&lastCheckedDateTime,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"lastCheckedDateTime=<n>");
    return;
  }
  errorMessage = String_new();
  if (!StringMap_getString(argumentMap,"errorMessage",errorMessage,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"errorMessage=<name>");
    String_delete(errorMessage);
    return;
  }

  if (indexHandle != NULL)
  {
    // set state
    error = Index_setStorageState(indexHandle,
                                  indexId,
                                  indexState,
                                  lastCheckedDateTime,
                                  "%s",
                                  String_cString(errorMessage)
                                 );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      String_delete(errorMessage);
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  String_delete(errorMessage);
}

/***********************************************************************\
* Name   : connectorCommand_indexUUIDUpdateInfos
* Purpose: update UUID infos
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            uuidId=<n>
*          Result:
\***********************************************************************/

#if 0
//TODO: still not used
LOCAL void connectorCommand_indexUUIDUpdateInfos(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId uuidId;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get uuidId
  if (!StringMap_getInt64(argumentMap,"uuidId",&uuidId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"uuidId=<n>");
    return;
  }
  if (Index_getType(uuidId) != INDEX_TYPE_UUID)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"not an UUID index id %llx",uuidId);
    return;
  }

  if (indexHandle != NULL)
  {
    // update entity infos
    error = Index_updateUUIDInfos(indexHandle,
                                  uuidId
                                 );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
}
#endif

/***********************************************************************\
* Name   : connectorCommand_indexEntityUpdateInfos
* Purpose: update entity infos
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            entityId=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexEntityUpdateInfos(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId entityId;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get entityId
  if (!StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<n>");
    return;
  }
  if (Index_getType(entityId) != INDEX_TYPE_ENTITY)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"not an entity index id %llx",entityId);
    return;
  }

  if (indexHandle != NULL)
  {
    // update entity infos
    error = Index_updateEntityInfos(indexHandle,
                                    entityId
                                   );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
}


/***********************************************************************\
* Name   : connectorCommand_indexEntityUnlock
* Purpose: unlock entity
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            entityId=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexEntityUnlock(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId entityId;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get entityId
  if (!StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<n>");
    return;
  }
  if (Index_getType(entityId) != INDEX_TYPE_ENTITY)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"not an entity index id %llx",entityId);
    return;
  }

  if (indexHandle != NULL)
  {
    // updunlockate entity
    error = Index_unlockEntity(indexHandle,
                               entityId
                              );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
}

/***********************************************************************\
* Name   : connectorCommand_indexEntityDelete
* Purpose: delete entity
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            entityId=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexEntityDelete(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId entityId;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get storageId
  if (!StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<n>");
    return;
  }
  if (Index_getType(entityId) != INDEX_TYPE_ENTITY)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"not a entity index id %llx",entityId);
    return;
  }

  if (indexHandle != NULL)
  {
    // delete storage
    error = Index_deleteEntity(indexHandle,
                               entityId
                              );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
}

/***********************************************************************\
* Name   : connectorCommand_indexStorageUpdate
* Purpose: update storage
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            storageId=<n>
*            hostName=<text>
*            userName=<text>
*            storageName=<text>
*            storageSize=<n>
*            comment=<text>
*            updateNewest=yes|no
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexStorageUpdate(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId storageId;
  String  hostName,userName;
  String  storageName;
  uint64  dateTime;
  uint64  storageSize;
  String  comment;
  bool    updateNewest;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get storageId, storageName, storageSize
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"not a storage index id %llx",storageId);
    return;
  }
  hostName = String_new();
  StringMap_getString(argumentMap,"hostName",hostName,NULL);
  userName = String_new();
  StringMap_getString(argumentMap,"userName",userName,NULL);
  storageName = String_new();
  StringMap_getString(argumentMap,"storageName",storageName,NULL);
  StringMap_getUInt64(argumentMap,"dateTime",&dateTime,0LL);
  if (!StringMap_getUInt64(argumentMap,"storageSize",&storageSize,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageSize=<n>");
    String_delete(storageName);
    String_delete(userName);
    return;
  }
  comment = String_new();
  StringMap_getString(argumentMap,"comment",comment,NULL);
  StringMap_getBool(argumentMap,"updateNewest",&updateNewest,FALSE);

  if (indexHandle != NULL)
  {
    // update storage
    error = Index_updateStorage(indexHandle,
                                storageId,
                                hostName,
                                userName,
                                storageName,
                                dateTime,
                                storageSize,
                                comment,
                                updateNewest
                               );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      String_delete(comment);
      String_delete(storageName);
      String_delete(userName);
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  String_delete(comment);
  String_delete(storageName);
  String_delete(userName);
}

/***********************************************************************\
* Name   : connectorCommand_indexStorageUpdateInfos
* Purpose: update storage infos
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            storageId=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexStorageUpdateInfos(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId storageId;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get storageId
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"not a storage index id %llx",storageId);
    return;
  }

  if (indexHandle != NULL)
  {
    // update storage infos
    error = Index_updateStorageInfos(indexHandle,
                                     storageId
                                    );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
}

/***********************************************************************\
* Name   : connectorCommand_indexStoragePurge
* Purpose: purge storage (mark as "deleted")
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            storageId=<n>
*            storageName=<name>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexStoragePurge(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId          storageId;
  String           storageName;
  StorageSpecifier storageSpecifier;
  Errors           error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get storageId
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"not a storage index id %llx",storageId);
    return;
  }
  storageName = String_new();
  if (!StringMap_getString(argumentMap,"storageName",storageName,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageName=<name>");
    String_delete(storageName);
    return;
  }

  Storage_initSpecifier(&storageSpecifier);
  error = Storage_parseName(&storageSpecifier,storageName);
  if (error != ERROR_NONE)
  {
    sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
    Storage_doneSpecifier(&storageSpecifier);
    String_delete(storageName);
    return;
  }

  if (indexHandle != NULL)
  {
    // purge storage
    error = Index_purgeStorage(indexHandle,
                               storageId
                              );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      Storage_doneSpecifier(&storageSpecifier);
      String_delete(storageName);
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(storageName);
}

/***********************************************************************\
* Name   : connectorCommand_indexStoragePurgeAll
* Purpose: purge storage (mark as "deleted")
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            entityId=<id>
*            storageName=<name>
*            keepStorageId=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexStoragePurgeAll(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId          entityId;
  String           storageName;
  IndexId          keepStorageId;
  StorageSpecifier storageSpecifier;
  Errors           error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get entity id, storage name, keep storage id
  StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE);
  storageName = String_new();
  StringMap_getString(argumentMap,"storageName",storageName,NULL);
  StringMap_getInt64(argumentMap,"keepStorageId",&keepStorageId,INDEX_ID_NONE);
  if ((keepStorageId != INDEX_ID_NONE) && (Index_getType(keepStorageId) != INDEX_TYPE_STORAGE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"not a storage index id %llx",keepStorageId);
    String_delete(storageName);
    return;
  }

  if (indexHandle != NULL)
  {
    if (entityId != INDEX_ID_NONE)
    {
      error = Index_purgeAllStoragesById(indexHandle,
                                         entityId,
                                         keepStorageId
                                        );
      if (error != ERROR_NONE)
      {
        sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
        Storage_doneSpecifier(&storageSpecifier);
        String_delete(storageName);
        return;
      }
    }

    if (!String_isEmpty(storageName))
    {
      Storage_initSpecifier(&storageSpecifier);
      error = Storage_parseName(&storageSpecifier,storageName);
      if (error != ERROR_NONE)
      {
        sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
        Storage_doneSpecifier(&storageSpecifier);
        String_delete(storageName);
        return;
      }

      // purge storage
      error = Index_purgeAllStoragesByName(indexHandle,
                                           &storageSpecifier,
                                           NULL,
                                           keepStorageId
                                          );
      if (error != ERROR_NONE)
      {
        sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
        Storage_doneSpecifier(&storageSpecifier);
        String_delete(storageName);
        return;
      }
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(storageName);
}

/***********************************************************************\
* Name   : connectorCommand_indexNewHistory
* Purpose: new index history entry
* Input  : connectorInfo - connector info
*          indexHandle   - index handle
*          id            - command id
*          argumentMap   - command arguments
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            scheduleUUID=<uuid>
*            hostName=<name>
*            archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL
*            createdDateTime=<n>
*            errorMessage=<text>
*            duration=<n>
*            totalEntryCount=<n>
*            totalEntrySize=<n>
*            skippedEntryCount=<n>
*            skippedEntrySize=<n>
*            errorEntryCount=<n>
*            errorEntrySize=<n>
*          Result:
*            errorMessage=<text>
\***********************************************************************/

LOCAL void connectorCommand_indexNewHistory(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String       hostName;
  ArchiveTypes archiveType;
  uint64       createdDateTime;
  String       errorMessage;
  uint64       duration;
  uint         totalEntryCount;
  uint64       totalEntrySize;
  uint         skippedEntryCount;
  uint64       skippedEntrySize;
  uint         errorEntryCount;
  uint64       errorEntrySize;
  Errors       error;
  IndexId      historyId;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get jobUUID, scheduleUUID, hostName, archiveType, createdDateTime, errorMessage, duration, totalEntryCount, totalEntrySize, skippedEntryCount, skippedEntrySize, errorEntryCount, errorEntrySize
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<text>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"scheduleUUID=<text>");
    return;
  }
  hostName = String_new();
  if (!StringMap_getString(argumentMap,"hostName",hostName,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"hostName=<text>");
    String_delete(hostName);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,CALLBACK_((StringMapParseEnumFunction)Archive_parseType,NULL),ARCHIVE_TYPE_NONE))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"createdDateTime=<n>");
    String_delete(hostName);
    return;
  }
  errorMessage = String_new();
  if (!StringMap_getString(argumentMap,"errorMessage",errorMessage,NULL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"hostName=<text>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"duration",&duration,0L))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"duration=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"totalEntryCount",&totalEntryCount,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"totalEntryCount=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"totalEntrySize",&totalEntrySize,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"totalEntrySize=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"skippedEntryCount",&skippedEntryCount,0L))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"skippedEntryCount=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"skippedEntrySize",&skippedEntrySize,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"skippedEntrySize=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"errorEntryCount",&errorEntryCount,0L))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"errorEntryCount=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"errorEntrySize",&errorEntrySize,0LL))
  {
    sendResult(connectorInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"errorEntrySize=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }

  if (indexHandle != NULL)
  {
    // add index history entry
    error = Index_newHistory(indexHandle,
                             jobUUID,
                             scheduleUUID,
                             hostName,
                             NULL,  // userName
                             archiveType,
                             createdDateTime,
                             String_cString(errorMessage),
                             duration,
                             totalEntryCount,
                             totalEntrySize,
                             skippedEntryCount,
                             skippedEntrySize,
                             errorEntryCount,
                             errorEntrySize,
                             &historyId
                            );
    if (error != ERROR_NONE)
    {
      sendResult(connectorInfo,id,TRUE,error,"%s",Error_getData(error));
      String_delete(errorMessage);
      String_delete(hostName);
      return;
    }

    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_NONE,"historyId=%lld",historyId);
  }
  else
  {
    // send result
    sendResult(connectorInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  String_delete(errorMessage);
  String_delete(hostName);
}

// server commands
const struct
{
  const char           *name;
  ConnectorCommandFunction connectorCommandFunction;
}
CONNECTOR_COMMANDS[] =
{
  { "STORAGE_CREATE",            connectorCommand_storageCreate           },
  { "STORAGE_WRITE",             connectorCommand_storageWrite            },
  { "STORAGE_CLOSE",             connectorCommand_storageClose            },
  { "STORAGE_EXISTS",            connectorCommand_storageExists           },

  { "INDEX_FIND_UUID",           connectorCommand_indexFindUUID           },
  { "INDEX_NEW_UUID",            connectorCommand_indexNewUUID            },
  { "INDEX_NEW_ENTITY",          connectorCommand_indexNewEntity          },
  { "INDEX_NEW_STORAGE",         connectorCommand_indexNewStorage         },
  { "INDEX_ADD_FILE",            connectorCommand_indexAddFile            },
  { "INDEX_ADD_IMAGE",           connectorCommand_indexAddImage           },
  { "INDEX_ADD_DIRECTORY",       connectorCommand_indexAddDirectory       },
  { "INDEX_ADD_LINK",            connectorCommand_indexAddLink            },
  { "INDEX_ADD_HARDLINK",        connectorCommand_indexAddHardlink        },
  { "INDEX_ADD_SPECIAL",         connectorCommand_indexAddSpecial         },

  { "INDEX_UUID_PURGE",          connectorCommand_indexUUIDPurge          },
  { "INDEX_UUID_PRUNE",          connectorCommand_indexUUIDPrune          },

  { "INDEX_ENTITY_PURGE",        connectorCommand_indexEntityPurge        },
  { "INDEX_ENTITY_PRUNE",        connectorCommand_indexEntityPrune        },

  { "INDEX_SET_STATE",           connectorCommand_indexSetState           },
  { "INDEX_ENTITY_UPDATE_INFOS", connectorCommand_indexEntityUpdateInfos  },
  { "INDEX_ENTITY_UNLOCK",       connectorCommand_indexEntityUnlock       },
  { "INDEX_ENTITY_DELETE",       connectorCommand_indexEntityDelete       },

  { "INDEX_STORAGE_UPDATE",      connectorCommand_indexStorageUpdate      },
  { "INDEX_STORAGE_UPDATE_INFOS",connectorCommand_indexStorageUpdateInfos },
  { "INDEX_STORAGE_PURGE",       connectorCommand_indexStoragePurge       },
  { "INDEX_STORAGE_PURGE_ALL",   connectorCommand_indexStoragePurgeAll    },

  { "INDEX_NEW_HISTORY",         connectorCommand_indexNewHistory         },
};

/***********************************************************************\
* Name   : findConnectorCommand
* Purpose: find connector command
* Input  : name - command name
* Output : connectorCommandFunction - connector command function
* Return : TRUE if command found, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool findConnectorCommand(ConstString              name,
                                ConnectorCommandFunction *connectorCommandFunction
                               )
{
  uint i;

  assert(name != NULL);
  assert(connectorCommandFunction != NULL);

  // find command by name
  i = ARRAY_FIND(CONNECTOR_COMMANDS,SIZE_OF_ARRAY(CONNECTOR_COMMANDS),i,String_equalsCString(name,CONNECTOR_COMMANDS[i].name));
  if (i >= SIZE_OF_ARRAY(CONNECTOR_COMMANDS))
  {
    return FALSE;
  }
  (*connectorCommandFunction) = CONNECTOR_COMMANDS[i].connectorCommandFunction;

  return TRUE;
}

/***********************************************************************\
* Name   : connectorThreadCode
* Purpose: connector thread code
* Input  : connectorInfo - connector info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void connectorThreadCode(ConnectorInfo *connectorInfo)
{
  #define TIMEOUT (5*MS_PER_SECOND)

  String                   name;
  StringMap                argumentMap;
  IndexHandle              indexHandle;
  SignalMask               signalMask;
  uint                     events;
  uint                     id;
  ConnectorCommandFunction connectorCommandFunction;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  // init variables
  name        = String_new();
  argumentMap = StringMap_new();

  // Note: ignore SIGALRM in Misc_waitHandle()
  MISC_SIGNAL_MASK_CLEAR(signalMask);
  #ifdef HAVE_SIGALRM
    MISC_SIGNAL_MASK_SET(signalMask,SIGALRM);
  #endif /* HAVE_SIGALRM */

  // open index
  while (   !Thread_isQuit(&connectorInfo->thread)
         && (Index_open(&indexHandle,NULL,10*MS_PER_SECOND) != ERROR_NONE)
        )
  {
    // nothing to do
  }

  // process client requests
  while (   !Thread_isQuit(&connectorInfo->thread)
         && Connector_isConnected(connectorInfo)
        )
  {
    // process server i/o commands
    while (ServerIO_getCommand(&connectorInfo->io,
                               &id,
                               name,
                               argumentMap
                              )
          )
    {
      // find command
      #if   defined(CONNECTOR_DEBUG)
//TODO: enable
        fprintf(stderr,"DEBUG connector received command: %u %s\n",id,String_cString(name));
        #ifndef NDEBUG
          StringMap_debugPrint(2,argumentMap);
        #endif
      #elif !defined(NDEBUG)
        if (globalOptions.debug.serverLevel >= 1)
        {
          fprintf(stderr,"DEBUG: received command #%u %s\n",id,String_cString(name));
        }
      #endif
      if (!findConnectorCommand(name,&connectorCommandFunction))
      {
        sendResult(connectorInfo,id,TRUE,ERROR_UNKNOWN_COMMAND,"%S",name);
        continue;
      }
      assert(connectorCommandFunction != NULL);

      // process command
      connectorCommandFunction(connectorInfo,&indexHandle,id,argumentMap);
    }

    // wait for disconnect, data, or result
    events = Misc_waitHandle(Network_getSocket(&connectorInfo->io.network.socketHandle),
                             &signalMask,
                             HANDLE_EVENT_INPUT|HANDLE_EVENT_ERROR|HANDLE_EVENT_HANGUP|HANDLE_EVENT_INVALID,
                             TIMEOUT
                            );
    if (events != 0)
    {
      if      (Misc_isHandleEvent(events,HANDLE_EVENT_INPUT))
      {
        if (ServerIO_receiveData(&connectorInfo->io))
        {
          // process server i/o commands
          while (ServerIO_getCommand(&connectorInfo->io,
                                     &id,
                                     name,
                                     argumentMap
                                    )
                )
          {
            // find command
            #if   defined(CONNECTOR_DEBUG)
//TODO: enable
              fprintf(stderr,"DEBUG connector received command: %u %s\n",id,String_cString(name));
              #ifndef NDEBUG
                StringMap_debugPrint(2,argumentMap);
              #endif
            #elif !defined(NDEBUG)
              if (globalOptions.debug.serverLevel >= 1)
              {
                fprintf(stderr,"DEBUG: received command #%u %s\n",id,String_cString(name));
              }
            #endif
            if (!findConnectorCommand(name,&connectorCommandFunction))
            {
              sendResult(connectorInfo,id,TRUE,ERROR_UNKNOWN_COMMAND,"%S",name);
              continue;
            }
            assert(connectorCommandFunction != NULL);

            // process command
            connectorCommandFunction(connectorInfo,&indexHandle,id,argumentMap);
          }
        }
        else
        {
          // no data -> shut down
          setConnectorState(connectorInfo,CONNECTOR_STATE_SHUTDOWN);
        }
      }
      else if ((events & (HANDLE_EVENT_ERROR|HANDLE_EVENT_HANGUP|HANDLE_EVENT_INVALID)) != 0)
      {
        // error/hang-up/invalid -> shut down
        setConnectorState(connectorInfo,CONNECTOR_STATE_SHUTDOWN);
      }
      #ifndef NDEBUG
        else
        {
          HALT_INTERNAL_ERROR("unknown event in 0x%x",events);
        }
      #endif /* NDEBUG */
    }
  }

  // done index
  Index_close(&indexHandle);

  // free resources
  StringMap_delete(argumentMap);
  String_delete(name);
}

/***********************************************************************\
* Name   : vexecuteCommand
* Purpose: execute command on connector host
* Input  : connectorInfo         - connector info
*          debugLevel            - debug level
*          timeout               - timeout [ms] or WAIT_FOREVER
*          commandResultFunction - command result function (can be NULL)
*          commandResultUserData - user data for command result function
*          format                - command
*          arguments             - arguments
* Output : resultMap - result map
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors vexecuteCommand(ConnectorInfo                  *connectorInfo,
                             uint                           debugLevel,
                             long                           timeout,
                             ConnectorCommandResultFunction commandResultFunction,
                             void                           *commandResultUserData,
                             const char                     *format,
                             va_list                        arguments
                            )
{
  Errors error;

  assert(connectorInfo != NULL);

  // init variables

  error = ServerIO_vexecuteCommand(&connectorInfo->io,
                                   debugLevel,
                                   timeout,
                                   CALLBACK_(commandResultFunction,commandResultUserData),
                                   format,
                                   arguments
                                  );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // free resources

  return ERROR_NONE;
}
// ----------------------------------------------------------------------

Errors Connector_initAll(void)
{
  // init variables

  return ERROR_NONE;
}

void Connector_doneAll(void)
{
}

#ifdef NDEBUG
  void Connector_init(ConnectorInfo *connectorInfo)
#else /* not NDEBUG */
  void __Connector_init(const char       *__fileName__,
                        ulong            __lineNb__,
                        ConnectorInfo *connectorInfo
                       )
#endif /* NDEBUG */
{
  assert(connectorInfo != NULL);

//TODO: remove
//  connectorInfo->forceSSL        = forceSSL;
  connectorInfo->state           = CONNECTOR_STATE_NONE;
  connectorInfo->storageInitFlag = FALSE;
  connectorInfo->storageOpenFlag = FALSE;

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(connectorInfo,ConnectorInfo);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,connectorInfo,ConnectorInfo);
  #endif /* NDEBUG */
}

void Connector_done(ConnectorInfo *connectorInfo)
{
  assert(connectorInfo != NULL);

  if (Connector_isConnected(connectorInfo))
  {
    connectorDisconnect(connectorInfo);
  }

  DEBUG_REMOVE_RESOURCE_TRACE(connectorInfo,ConnectorInfo);
}

Errors Connector_connect(ConnectorInfo *connectorInfo,
                         ConstString   hostName,
                         uint          hostPort,
                         TLSModes      tlsMode,
                         const void    *caData,
                         uint          caLength,
                         const void    *certData,
                         uint          certLength,
                         const void    *keyData,
                         uint          keyLength
                        )
{
  Errors error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(hostName != NULL);

  // init variables

  // connect connector, get session id/public key
  error = connectorConnect(connectorInfo,
                           hostName,
                           hostPort,
                           tlsMode,
                           caData,
                           caLength,
                           certData,
                           certLength,
                           keyData,
                           keyLength
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  printInfo(2,"Connected connector '%s:%d'\n",String_cString(hostName),hostPort);

  // free resources

  return ERROR_NONE;
}

void Connector_disconnect(ConnectorInfo *connectorInfo)
{
  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  connectorDisconnect(connectorInfo);
}

Errors Connector_authorize(ConnectorInfo *connectorInfo, long timeout)
{
  Errors error;
  String hostName;
  String encryptedUUID;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  // init variables
  hostName      = String_new();
  encryptedUUID = String_new();

  // get encrypted UUID for authorization
  error = ServerIO_encryptData(&connectorInfo->io,
                               String_cString(instanceUUID),
                               String_length(instanceUUID),
                               encryptedUUID
                              );
  if (error != ERROR_NONE)
  {
    String_delete(encryptedUUID);
    String_delete(hostName);
    return error;
  }
//fprintf(stderr,"%s, %d: uuid=%s encryptedUUID=%s\n",__FILE__,__LINE__,String_cString(instanceUUID),String_cString(encryptedUUID));
//assert(ServerIO_decryptString(&connectorInfo->io,string,SERVER_IO_ENCRYPT_TYPE_RSA,encryptedUUID)==ERROR_NONE); fprintf(stderr,"%s, %d: dectecryp encryptedUUID: %s\n",__FILE__,__LINE__,String_cString(string));

  // authorize with UUID
  Network_getHostName(hostName);
  error = Connector_executeCommand(connectorInfo,
                                   CONNECTOR_DEBUG_LEVEL,
                                   timeout,
                                   CALLBACK_(NULL,NULL),
                                   "AUTHORIZE encryptType=%s name=%'S encryptedUUID=%'S",
                                   ServerIO_encryptTypeToString(connectorInfo->io.encryptType,"NONE"),
                                   hostName,
                                   encryptedUUID
                                  );
  if (error != ERROR_NONE)
  {
    String_delete(encryptedUUID);
    String_delete(hostName);
    return error;
  }

  // set state
  setConnectorState(connectorInfo,CONNECTOR_STATE_AUTHORIZED);

  // free resources
  String_delete(encryptedUUID);
  String_delete(hostName);

  return ERROR_NONE;
}

Errors Connector_getVersion(ConnectorInfo *connectorInfo,
                            uint          *protocolVersionMajor,
                            uint          *protocolVersionMinor,
                            ServerModes   *serverMode
                           )
{
  /***********************************************************************\
  * Name   : parseServerMode
  * Purpose: parse server mode text
  * Input  : serverModeText - server mode text
  *          serverMode     - server mode variable
  *          userData       - user data (not used)
  * Output : serverMode - job state
  * Return : always TRUE
  * Notes  : -
  \***********************************************************************/

  auto bool parseServerMode(const char *serverModeText, ServerModes *serverMode, void *userData);
  bool parseServerMode(const char *serverModeText, ServerModes *serverMode, void *userData)
  {
    assert(serverModeText != NULL);
    assert(serverMode != NULL);

    UNUSED_VARIABLE(userData);

    if      (stringEqualsIgnoreCase(serverModeText,"master")) (*serverMode) = SERVER_MODE_MASTER;
    else if (stringEqualsIgnoreCase(serverModeText,"slave" )) (*serverMode) = SERVER_MODE_SLAVE;
    else                                                      (*serverMode) = SERVER_MODE_MASTER;

    return TRUE;
  }

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(protocolVersionMajor != NULL);

  return Connector_executeCommand(connectorInfo,
                                  CONNECTOR_DEBUG_LEVEL,
                                  CONNECTOR_COMMAND_TIMEOUT,
                                  CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                  {
                                    assert(resultMap != NULL);

                                    UNUSED_VARIABLE(userData);

                                    StringMap_getUInt(resultMap,"major",protocolVersionMajor,0);
                                    if (protocolVersionMinor != NULL)
                                    {
                                      StringMap_getUInt(resultMap,"minor",protocolVersionMinor,0);
                                    }
                                    if (serverMode != NULL)
                                    {
                                      StringMap_getEnum(resultMap,"mode",serverMode,CALLBACK_((StringMapParseEnumFunction)parseServerMode,NULL),SERVER_MODE_MASTER);
                                    }

                                    return ERROR_NONE;
                                  },NULL),
                                  "VERSION"
                                 );
}

Errors Connector_initStorage(ConnectorInfo *connectorInfo,
                             ConstString   storageName,
                             JobOptions    *jobOptions
                            )
{
  String           printableStorageName;
  Errors           error;
  StorageSpecifier storageSpecifier;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(storageName != NULL);
  assert(jobOptions != NULL);

  // init variables
  printableStorageName = String_new();

  // parse storage name
  Storage_initSpecifier(&storageSpecifier);
  error = Storage_parseName(&storageSpecifier,storageName);
  if (error != ERROR_NONE)
  {
    printError("cannot initialize storage '%s' (error: %s)",
               String_cString(storageName),
               Error_getText(error)
              );
    Storage_doneSpecifier(&storageSpecifier);
    return error;
  }
  DEBUG_TESTCODE() { Storage_doneSpecifier(&storageSpecifier); return DEBUG_TESTCODE_ERROR(); }

  // get printable storage name
  Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);

  // init storage
  error = Storage_init(&connectorInfo->storageInfo,
                       NULL,  // masterIO
                       &storageSpecifier,
                       jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
//TODO
CALLBACK_(NULL,NULL),//                       CALLBACK_(updateStorageStatusInfo,connectorInfo),
CALLBACK_(NULL,NULL),//                       CALLBACK_(getPasswordFunction,getPasswordUserData),
CALLBACK_(NULL,NULL),//                       CALLBACK_(storageRequestVolumeFunction,storageRequestVolumeUserData)
                       CALLBACK_(NULL,NULL),  // isPause
                       CALLBACK_(NULL,NULL),  // isAborted
                       NULL  // logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError("cannot initialize storage '%s' (error: %s)",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    Storage_doneSpecifier(&storageSpecifier);
    return error;
  }
  DEBUG_TESTCODE() { Storage_done(&connectorInfo->storageInfo); Storage_doneSpecifier(&storageSpecifier); return DEBUG_TESTCODE_ERROR(); }
  connectorInfo->storageInitFlag = TRUE;

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(printableStorageName);

  return ERROR_NONE;
}

Errors Connector_doneStorage(ConnectorInfo *connectorInfo)
{
  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  // close storage
  if (connectorInfo->storageOpenFlag)
  {
    Storage_close(&connectorInfo->storageHandle);
    connectorInfo->storageOpenFlag = FALSE;
  }

  // done storage
  connectorInfo->storageInitFlag = FALSE;
  Storage_done(&connectorInfo->storageInfo);

  return ERROR_NONE;
}

Errors Connector_executeCommand(ConnectorInfo                  *connectorInfo,
                                uint                           debugLevel,
                                long                           timeout,
                                ConnectorCommandResultFunction commandResultFunction,
                                void                           *commandResultUserData,
                                const char                     *format,
                                ...
                               )
{
  va_list  arguments;
  Errors   error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  va_start(arguments,format);
  error = vexecuteCommand(connectorInfo,
                          debugLevel,
                          timeout,
                          commandResultFunction,
                          commandResultUserData,
                          format,
                          arguments
                         );
  va_end(arguments);

  return error;
}

Errors Connector_jobAbort(ConnectorInfo *connectorInfo,
                          ConstString   jobUUID
                         )
{
  Errors error;

  assert(connectorInfo != NULL);
  assert(jobUUID != NULL);

  error = ERROR_NONE;

  // abort execute job
  error = Connector_executeCommand(connectorInfo,
                                   CONNECTOR_DEBUG_LEVEL,
                                   CONNECTOR_COMMAND_TIMEOUT,
                                   CALLBACK_(NULL,NULL),
                                   "JOB_ABORT jobUUID=%S",
                                   jobUUID
                                  );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // free resources

  return ERROR_NONE;
}

Errors Connector_create(ConnectorInfo                *connectorInfo,
                        ConstString                  jobName,
                        ConstString                  jobUUID,
                        ConstString                  scheduleUUID,
                        ConstString                  storageName,
                        const EntryList              *includeEntryList,
                        const PatternList            *excludePatternList,
                        const JobOptions             *jobOptions,
                        ArchiveTypes                 archiveType,
                        ConstString                  scheduleTitle,
                        ConstString                  scheduleCustomText,
                        GetNamePasswordFunction      getNamePasswordFunction,
                        void                         *getNamePasswordUserData,
                        StatusInfoFunction           statusInfoFunction,
                        void                         *statusInfoUserData,
                        StorageRequestVolumeFunction storageRequestVolumeFunction,
                        void                         *storageRequestVolumeUserData
                       )
{
  /***********************************************************************\
  * Name   : parseJobState
  * Purpose: parse job state text
  * Input  : jobStateText - job state text
  *          jobState     - job state variable
  *          userData     - user data (not used)
  * Output : jobState - job state
  * Return : always TRUE
  * Notes  : -
  \***********************************************************************/

  auto bool parseJobState(const char *jobStateText, JobStates *jobState, void *userData);
  bool parseJobState(const char *jobStateText, JobStates *jobState, void *userData)
  {
    assert(jobStateText != NULL);
    assert(jobState != NULL);

    UNUSED_VARIABLE(userData);

    if      (stringEqualsIgnoreCase(jobStateText,"-"                      )) (*jobState) = JOB_STATE_NONE;
    else if (stringEqualsIgnoreCase(jobStateText,"waiting"                )) (*jobState) = JOB_STATE_WAITING;
    else if (stringEqualsIgnoreCase(jobStateText,"dry-run"                )) (*jobState) = JOB_STATE_RUNNING;
    else if (stringEqualsIgnoreCase(jobStateText,"running"                )) (*jobState) = JOB_STATE_RUNNING;
    else if (stringEqualsIgnoreCase(jobStateText,"request FTP password"   )) (*jobState) = JOB_STATE_REQUEST_FTP_PASSWORD;
    else if (stringEqualsIgnoreCase(jobStateText,"request SSH password"   )) (*jobState) = JOB_STATE_REQUEST_SSH_PASSWORD;
    else if (stringEqualsIgnoreCase(jobStateText,"request webDAV password")) (*jobState) = JOB_STATE_REQUEST_WEBDAV_PASSWORD;
    else if (stringEqualsIgnoreCase(jobStateText,"request crypt password" )) (*jobState) = JOB_STATE_REQUEST_CRYPT_PASSWORD;
    else if (stringEqualsIgnoreCase(jobStateText,"request volume"         )) (*jobState) = JOB_STATE_REQUEST_VOLUME;
    else if (stringEqualsIgnoreCase(jobStateText,"done"                   )) (*jobState) = JOB_STATE_DONE;
    else if (stringEqualsIgnoreCase(jobStateText,"ERROR"                  )) (*jobState) = JOB_STATE_ERROR;
    else if (stringEqualsIgnoreCase(jobStateText,"aborted"                )) (*jobState) = JOB_STATE_ABORTED;
    else                                                                     (*jobState) = JOB_STATE_ERROR;

    return TRUE;
  }

  AutoFreeList autoFreeList;
  Errors       error;
  StringMap    resultMap;
  JobStates    state;
  uint         errorCode;
  String       errorData;
  StatusInfo   statusInfo;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

UNUSED_VARIABLE(getNamePasswordFunction);
UNUSED_VARIABLE(getNamePasswordUserData);
UNUSED_VARIABLE(storageRequestVolumeFunction);
UNUSED_VARIABLE(storageRequestVolumeUserData);

  // init variables
  AutoFree_init(&autoFreeList);
  errorData = String_new();
  initStatusInfo(&statusInfo);
  resultMap = StringMap_new();
  if (resultMap == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  AUTOFREE_ADD(&autoFreeList,errorData,{ String_delete(errorData); });
  AUTOFREE_ADD(&autoFreeList,&statusInfo,{ doneStatusInfo(&statusInfo); });
  AUTOFREE_ADD(&autoFreeList,resultMap,{ StringMap_delete(resultMap); });

  // transmit job to slave
  error = transmitJob(connectorInfo,
                      jobName,
                      jobUUID,
                      scheduleUUID,
                      storageName,
                      includeEntryList,
                      excludePatternList,
                      jobOptions,
                      archiveType,
                      scheduleTitle,
                      scheduleCustomText
                     );
  if (error != ERROR_NONE)
  {
    (void)Connector_executeCommand(connectorInfo,
                                   CONNECTOR_DEBUG_LEVEL,
                                   CONNECTOR_COMMAND_TIMEOUT,
                                   CALLBACK_(NULL,NULL),
                                   "JOB_DELETE jobUUID=%S",
                                   jobUUID
                                  );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // start execute job
  error = Connector_executeCommand(connectorInfo,
                                   CONNECTOR_DEBUG_LEVEL,
                                   CONNECTOR_COMMAND_TIMEOUT,
                                   CALLBACK_(NULL,NULL),
                                   "JOB_START jobUUID=%S scheduleUUID=%S scheduleCustomText=%'S archiveType=%s dryRun=%y noStorage=%y",
                                   jobUUID,
                                   scheduleUUID,
                                   NULL,  // scheduleCustomText
                                   Archive_archiveTypeToString(archiveType),
                                   jobOptions->dryRun,
                                   jobOptions->noStorage
                                  );
  if (error != ERROR_NONE)
  {
    (void)Connector_executeCommand(connectorInfo,
                                   CONNECTOR_DEBUG_LEVEL,
                                   CONNECTOR_COMMAND_TIMEOUT,
                                   CALLBACK_(NULL,NULL),
                                   "JOB_DELETE jobUUID=%S",
                                   jobUUID
                                  );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }

  // wait until job terminated
  do
  {
    // get slave job status
    error = Connector_executeCommand(connectorInfo,
                                     CONNECTOR_DEBUG_LEVEL,
                                     CONNECTOR_COMMAND_TIMEOUT,
                                     CALLBACK_INLINE(Errors,(const StringMap resultMap, void *userData),
                                     {
                                       assert(resultMap != NULL);

                                       UNUSED_VARIABLE(userData);

                                       // get status values
                                       StringMap_getEnum  (resultMap,"state",                &state,CALLBACK_((StringMapParseEnumFunction)parseJobState,NULL),JOB_STATE_NONE);
                                       StringMap_getUInt  (resultMap,"errorCode",            &errorCode,ERROR_CODE_NONE);
                                       StringMap_getString(resultMap,"errorData",            errorData,NULL);
                                       StringMap_getULong (resultMap,"doneCount",            &statusInfo.done.count,0L);
                                       StringMap_getUInt64(resultMap,"doneSize",             &statusInfo.done.size,0LL);
                                       StringMap_getULong (resultMap,"totalEntryCount",      &statusInfo.total.count,0L);
                                       StringMap_getUInt64(resultMap,"totalEntrySize",       &statusInfo.total.size,0LL);
                                       StringMap_getBool  (resultMap,"collectTotalSumDone",  &statusInfo.collectTotalSumDone,FALSE);
                                       StringMap_getULong (resultMap,"skippedEntryCount",    &statusInfo.skipped.count,0L);
                                       StringMap_getUInt64(resultMap,"skippedEntrySize",     &statusInfo.skipped.size,0LL);
                                       StringMap_getULong (resultMap,"errorEntryCount",      &statusInfo.error.count,0L);
                                       StringMap_getUInt64(resultMap,"errorEntrySize",       &statusInfo.error.size,0LL);
                                       StringMap_getUInt64(resultMap,"archiveSize",          &statusInfo.archiveSize,0LL);
                                       StringMap_getDouble(resultMap,"compressionRatio",     &statusInfo.compressionRatio,0.0);
                                       StringMap_getString(resultMap,"entryName",            statusInfo.entry.name,NULL);
                                       StringMap_getUInt64(resultMap,"entryDoneSize",        &statusInfo.entry.doneSize,0LL);
                                       StringMap_getUInt64(resultMap,"entryTotalSize",       &statusInfo.entry.totalSize,0LL);
                                       StringMap_getString(resultMap,"storageName",          statusInfo.storage.name,NULL);
                                       StringMap_getUInt64(resultMap,"storageDoneSize",      &statusInfo.storage.doneSize,0L);
                                       StringMap_getUInt64(resultMap,"storageTotalSize",     &statusInfo.storage.totalSize,0L);
                                       StringMap_getUInt  (resultMap,"volumeNumber",         &statusInfo.volume.number,0);
                                       StringMap_getDouble(resultMap,"volumeProgress",       &statusInfo.volume.progress,0.0);
                                       StringMap_getString(resultMap,"message",              statusInfo.message,NULL);
//TODO
//                                       StringMap_getULong (resultMap,"entriesPerSecond",    &statusInfo.entriesPerSecond,0L);
//                                       StringMap_getULong (resultMap,"bytesPerSecond",    &statusInfo.bytesPerSecond,0L);
//                                       StringMap_getULong (resultMap,"storageBytesPerSecond",    &statusInfo.storageBytesPerSecond,0L);
//                                       StringMap_getULong (resultMap,"estimatedRestTime",    &statusInfo.estimatedRestTime,0L);

                                       return (errorCode != ERROR_CODE_NONE)
                                                ? ERRORF_(errorCode,"%s",String_cString(errorData))
                                                : ERROR_NONE;
                                     },NULL),
                                     "JOB_STATUS jobUUID=%S",
                                     jobUUID
                                    );

    // update job status
    statusInfoFunction(error,&statusInfo,statusInfoUserData);

    // sleep a short time
    Misc_mdelay(SLEEP_TIME_STATUS_UPDATE);
  }
  while (   Job_isRunning(state)
         && (error == ERROR_NONE)
         && Connector_isConnected(connectorInfo)
        );

  // close storage
  if (connectorInfo->storageOpenFlag)
  {
    Storage_close(&connectorInfo->storageHandle);
    connectorInfo->storageOpenFlag = FALSE;
  }

  // free resources
  StringMap_delete(resultMap);
  doneStatusInfo(&statusInfo);
  String_delete(errorData);
  AutoFree_done(&autoFreeList);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
