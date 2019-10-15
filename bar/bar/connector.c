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
#include <poll.h>
#include <pthread.h>
#include <locale.h>
#include <time.h>
#include <signal.h>
#include <assert.h>

#include "common/global.h"
#include "common/autofree.h"
#include "common/lists.h"
#include "common/strings.h"
#include "common/semaphores.h"
#include "common/network.h"
#include "common/patternlists.h"
#include "common/misc.h"

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
#define CONNECTOR_COMMAND_TIMEOUT   (60LL*MS_PER_SECOND)

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
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors connectorConnect(ConnectorInfo *connectorInfo,
                              ConstString   hostName,
                              uint          hostPort
                             )
{
  Errors error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  // connect network server i/o
  error = ServerIO_connectNetwork(&connectorInfo->io,
                                  hostName,
                                  (hostPort != 0) ? hostPort : DEFAULT_SERVER_PORT
                                 );
  if (error != ERROR_NONE)
  {
    return error;
  }

//TODO
  // start SSL
#ifndef WERROR
#warning TODO
#endif
#if 0
  error = Connector_executeCommand(connectorInfo,
                                   CONNECTOR_DEBUG_LEVEL,
                                   CONNECTOR_COMMAND_TIMEOUT,
                                   NULL,
                                   "START_SSL"
                                  );
  if (error != ERROR_NONE)
  {
    ServerIO_disconnect(&connectorInfo->io);
    return error;
  }
#endif

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
  }
  connectorInfo->storageOpenFlag = FALSE;

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
* Name   : Connector_setJobOptionInteger
* Purpose: set job int value
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          value         - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Connector_setJobOptionInteger(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, int value)
{
  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(name != NULL);

  return Connector_executeCommand(connectorInfo,
                                  CONNECTOR_DEBUG_LEVEL,
                                  CONNECTOR_COMMAND_TIMEOUT,
                                  NULL,
                                  "JOB_OPTION_SET jobUUID=%S name=%s value=%d",
                                  jobUUID,
                                  name,
                                  value
                                 );
}

/***********************************************************************\
* Name   : Connector_setJobOptionInteger64
* Purpose: set job int64 value
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          value         - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Connector_setJobOptionInteger64(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, int64 value)
{
  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(name != NULL);

  return Connector_executeCommand(connectorInfo,
                                  CONNECTOR_DEBUG_LEVEL,
                                  CONNECTOR_COMMAND_TIMEOUT,
                                  NULL,
                                  "JOB_OPTION_SET jobUUID=%S name=%s value=%lld",
                                  jobUUID,
                                  name,
                                  value
                                 );
}

/***********************************************************************\
* Name   : Connector_setJobOptionBoolean
* Purpose: set job boolean value
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          value         - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Connector_setJobOptionBoolean(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, bool value)
{
  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(name != NULL);

  return Connector_executeCommand(connectorInfo,
                                  CONNECTOR_DEBUG_LEVEL,
                                  CONNECTOR_COMMAND_TIMEOUT,
                                  NULL,
                                  "JOB_OPTION_SET jobUUID=%S name=%s value=%y",
                                  jobUUID,
                                  name,
                                  value
                                 );
}

/***********************************************************************\
* Name   : Connector_setJobOptionString
* Purpose: set job string value
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          value         - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Connector_setJobOptionString(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, ConstString value)
{
  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(name != NULL);

  return Connector_executeCommand(connectorInfo,
                                  CONNECTOR_DEBUG_LEVEL,
                                  CONNECTOR_COMMAND_TIMEOUT,
                                  NULL,
                                  "JOB_OPTION_SET jobUUID=%S name=%s value=%'S",
                                  jobUUID,
                                  name,
                                  value
                                 );
}

/***********************************************************************\
* Name   : Connector_setJobOptionCString
* Purpose: set job c-string value
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          value         - value
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Connector_setJobOptionCString(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, const char *value)
{
  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(name != NULL);

  return Connector_executeCommand(connectorInfo,
                                  CONNECTOR_DEBUG_LEVEL,
                                  CONNECTOR_COMMAND_TIMEOUT,
                                  NULL,
                                  "JOB_OPTION_SET jobUUID=%S name=%s value=%'s",
                                  jobUUID,
                                  name,
                                  value
                                 );
}

/***********************************************************************\
* Name   : Connector_setJobOptionPassword
* Purpose: set job password option
* Input  : connectorInfo - connector info
*          jobUUID       - job UUID
*          name          - value name
*          password      - password
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Connector_setJobOptionPassword(ConnectorInfo *connectorInfo, ConstString jobUUID, const char *name, const Password *password)
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
                                     NULL,
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
      if (error == ERROR_NONE) error = Connector_setJobOptionString(connectorInfo, \
                                                                    jobUUID, \
                                                                    name, \
                                                                    value \
                                                                   ); \
    } \
    while (0)
  #define SET_OPTION_CSTRING(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Connector_setJobOptionCString(connectorInfo, \
                                                                     jobUUID, \
                                                                     name, \
                                                                     value \
                                                                    ); \
    } \
    while (0)
  #define SET_OPTION_PASSWORD(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Connector_setJobOptionPassword(connectorInfo, \
                                                                     jobUUID, \
                                                                     name, \
                                                                     value \
                                                                    ); \
    } \
    while (0)
  #define SET_OPTION_INTEGER(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Connector_setJobOptionInteger(connectorInfo, \
                                                                     jobUUID, \
                                                                     name, \
                                                                     value \
                                                                    ); \
    } \
    while (0)
  #define SET_OPTION_INTEGER64(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Connector_setJobOptionInteger64(connectorInfo, \
                                                                      jobUUID, \
                                                                      name, \
                                                                      value \
                                                                     ); \
    } \
    while (0)
  #define SET_OPTION_BOOLEAN(name,value) \
    do \
    { \
      if (error == ERROR_NONE) error = Connector_setJobOptionBoolean(connectorInfo, \
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
                                   NULL,
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
  SET_OPTION_STRING   ("archive-name",           storageName);
  SET_OPTION_CSTRING  ("archive-type",           ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,jobOptions->archiveType,NULL));

  SET_OPTION_STRING   ("incremental-list-file",  jobOptions->incrementalListFileName);

  SET_OPTION_INTEGER64("archive-part-size",      jobOptions->archivePartSize);

//  SET_OPTION_INTEGER  ("directory-strip",        jobOptions->directoryStripCount);
//  SET_OPTION_STRING   ("destination",            jobOptions->destination);
//  SET_OPTION_STRING   ("owner",                  jobOptions->owner);

  SET_OPTION_CSTRING  ("pattern-type",           ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,jobOptions->patternType,NULL));

  SET_OPTION_STRING   ("compress-algorithm",     String_format(s,
                                                               "%s+%s",
                                                               Compress_algorithmToString(jobOptions->compressAlgorithms.delta,NULL),
                                                               Compress_algorithmToString(jobOptions->compressAlgorithms.byte, NULL)
                                                              )
                      );
  SET_OPTION_CSTRING  ("crypt-algorithm",        Crypt_algorithmToString(jobOptions->cryptAlgorithms[0],NULL));
  SET_OPTION_CSTRING  ("crypt-type",             ConfigValue_selectToString(CONFIG_VALUE_CRYPT_TYPES,
                                                                            Crypt_isEncrypted(jobOptions->cryptAlgorithms[0]) ? jobOptions->cryptType : CRYPT_TYPE_NONE,
                                                                            NULL
                                                                           )
                      );
  SET_OPTION_CSTRING  ("crypt-password-mode",    ConfigValue_selectToString(CONFIG_VALUE_PASSWORD_MODES,jobOptions->cryptPasswordMode,NULL));
  SET_OPTION_PASSWORD ("crypt-password",         &jobOptions->cryptPassword              );
  SET_OPTION_STRING   ("crypt-public-key",       Misc_base64Encode(s,jobOptions->cryptPublicKey.data,jobOptions->cryptPublicKey.length));

  SET_OPTION_STRING   ("pre-command",            jobOptions->slavePreProcessScript       );
  SET_OPTION_STRING   ("post-command",           jobOptions->slavePostProcessScript      );

  SET_OPTION_STRING   ("ftp-login-name",         jobOptions->ftpServer.loginName         );
  SET_OPTION_PASSWORD ("ftp-password",           &jobOptions->ftpServer.password         );

  SET_OPTION_INTEGER  ("ssh-port",               jobOptions->sshServer.port              );
  SET_OPTION_STRING   ("ssh-login-name",         jobOptions->sshServer.loginName         );
  SET_OPTION_PASSWORD ("ssh-password",           &jobOptions->sshServer.password         );
  SET_OPTION_STRING   ("ssh-public-key",         Misc_base64Encode(s,jobOptions->sshServer.publicKey.data,jobOptions->sshServer.publicKey.length));
  SET_OPTION_STRING   ("ssh-private-key",        Misc_base64Encode(s,jobOptions->sshServer.privateKey.data,jobOptions->sshServer.privateKey.length));

  SET_OPTION_INTEGER64("max-storage-size",       jobOptions->maxStorageSize);

  SET_OPTION_INTEGER64("volume-size",            jobOptions->volumeSize                  );
  SET_OPTION_BOOLEAN  ("ecc",                    jobOptions->errorCorrectionCodesFlag);
  SET_OPTION_BOOLEAN  ("blank",                  jobOptions->blankFlag);

  SET_OPTION_BOOLEAN  ("skip-unreadable",        jobOptions->skipUnreadableFlag);
  SET_OPTION_BOOLEAN  ("raw-images",             jobOptions->rawImagesFlag);
  SET_OPTION_CSTRING  ("archive-file-mode",      ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_FILE_MODES,jobOptions->archiveFileMode,NULL));
  SET_OPTION_CSTRING  ("restore-entry-mode",     ConfigValue_selectToString(CONFIG_VALUE_RESTORE_ENTRY_MODES,jobOptions->restoreEntryMode,NULL));
  SET_OPTION_BOOLEAN  ("wait-first-volume",      jobOptions->waitFirstVolumeFlag         );

  SET_OPTION_STRING   ("comment",                jobOptions->comment                     );

  // set lists
  if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                            CONNECTOR_DEBUG_LEVEL,
                                                            CONNECTOR_COMMAND_TIMEOUT,
                                                            NULL,
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
                                                              NULL,
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
                                                            NULL,
                                                            "EXCLUDE_LIST_CLEAR jobUUID=%S",
                                                            jobUUID
                                                           );
  LIST_ITERATE(excludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                              CONNECTOR_DEBUG_LEVEL,
                                                              CONNECTOR_COMMAND_TIMEOUT,
                                                              NULL,
                                                              "EXCLUDE_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                              jobUUID,
                                                              ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->pattern.type,NULL),
                                                              patternNode->string
                                                             );
  }

  if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                            CONNECTOR_DEBUG_LEVEL,
                                                            CONNECTOR_COMMAND_TIMEOUT,
                                                            NULL,
                                                            "MOUNT_LIST_CLEAR jobUUID=%S",
                                                            jobUUID
                                                           );
  LIST_ITERATE(&jobOptions->mountList,mountNode)
  {
    if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                              CONNECTOR_DEBUG_LEVEL,
                                                              CONNECTOR_COMMAND_TIMEOUT,
                                                              NULL,
                                                              "MOUNT_LIST_ADD jobUUID=%S name=%'S",
                                                              jobUUID,
                                                              mountNode->name
                                                             );
  }

  if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                            CONNECTOR_DEBUG_LEVEL,
                                                            CONNECTOR_COMMAND_TIMEOUT,
                                                            NULL,
                                                            "EXCLUDE_COMPRESS_LIST_CLEAR jobUUID=%S",
                                                            jobUUID
                                                           );
  LIST_ITERATE(&jobOptions->compressExcludePatternList,patternNode)
  {
    if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                              CONNECTOR_DEBUG_LEVEL,
                                                              CONNECTOR_COMMAND_TIMEOUT,
                                                              NULL,
                                                              "EXCLUDE_COMPRESS_LIST_ADD jobUUID=%S patternType=%s pattern=%'S",
                                                              jobUUID,
                                                              ConfigValue_selectToString(CONFIG_VALUE_PATTERN_TYPES,patternNode->pattern.type,NULL),
                                                              patternNode->string
                                                             );
  }

  if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                            CONNECTOR_DEBUG_LEVEL,
                                                            CONNECTOR_COMMAND_TIMEOUT,
                                                            NULL,
                                                            "SOURCE_LIST_CLEAR jobUUID=%S",
                                                            jobUUID
                                                           );
  LIST_ITERATE(&jobOptions->deltaSourceList,deltaSourceNode)
  {
    if (error == ERROR_NONE) error = Connector_executeCommand(connectorInfo,
                                                              CONNECTOR_DEBUG_LEVEL,
                                                              CONNECTOR_COMMAND_TIMEOUT,
                                                              NULL,
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
                                   NULL,
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

LOCAL void connectorCommand_storageCreate(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
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
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveName=<name>");
    String_delete(archiveName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"archiveSize",&archiveSize,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveSize=<n>");
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
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
    String_delete(archiveName);
    return;
  }
  connectorInfo->storageOpenFlag = TRUE;

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");

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
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"offset=<n>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"length",&length,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"length=<n>");
    return;
  }
  data = String_new();
  if (!StringMap_getString(argumentMap,"data",data,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"data=<data>");
    String_delete(data);
    return;
  }

  // check if storage is open
  if (!connectorInfo->storageOpenFlag)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_INVALID_STORAGE,"storage not open");
    String_delete(data);
    return;
  }

  // decode data
  buffer = malloc(length);
  if (buffer == NULL)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_INSUFFICIENT_MEMORY,"insufficient memory");
    String_delete(data);
    return;
  }
  if (!Misc_base64Decode(buffer,length,NULL,data,STRING_BEGIN))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_INSUFFICIENT_MEMORY,"decode base64 data fail");
    String_delete(data);
    return;
  }

  // write to storage
  error = Storage_seek(&connectorInfo->storageHandle,offset);
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
    free(buffer);
    String_delete(data);
    return;
  }
  error = Storage_write(&connectorInfo->storageHandle,buffer,length);
  if (error != ERROR_NONE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
    free(buffer);
    String_delete(data);
    return;
  }

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");

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

  // get archive size
  archiveSize = Storage_getSize(&connectorInfo->storageHandle);
UNUSED_VARIABLE(archiveSize);
UNUSED_VARIABLE(indexHandle);
UNUSED_VARIABLE(argumentMap);

  // close storage
  if (connectorInfo->storageOpenFlag)
  {
    Storage_close(&connectorInfo->storageHandle);
  }
  connectorInfo->storageOpenFlag = FALSE;

//TODO: index

  // send result
  ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
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
*            lastExecutedDateTime=<n>
*            lastErrorMessage=<text>
*            executionCount=<n>
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
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUUID,MISC_UUID_STRING_LENGTH);
  IndexId      uuidId;
  uint64       lastExecutedDateTime;
  String       lastErrorMessage;
  ulong        executionCountNormal,executionCountFull,executionCountIncremental,executionCountDifferential,executionCountContinuous;
  uint64       averageDurationNormal,averageDurationFull,averageDurationIncremental,averageDurationDifferential,averageDurationContinuous;
  ulong        totalEntityCount;
  ulong        totalStorageCount;
  uint64       totalStorageSize;
  ulong        totalEntryCount;
  uint64       totalEntrySize;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get jobUUID, scheduleUUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<text>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUUID,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"scheduleUUID=<text>");
    return;
  }

  lastErrorMessage = String_new();

  if (indexHandle != NULL)
  {
    // find job data
    if (Index_findUUID(indexHandle,
                       jobUUID,
                       scheduleUUUID,
                       &uuidId,
                       &lastExecutedDateTime,
                       lastErrorMessage,
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
                      )
       )
    {
      ServerIO_sendResult(&connectorInfo->io,
                          id,
                          TRUE,
                          ERROR_NONE,
                          "uuidId=%lld lastExecutedDateTime=%llu lastErrorMessage=%'S executionCountNormal=%lu executionCountFull=%lu executionCountIncremental=%lu executionCountDifferential=%lu executionCountContinuous=%lu averageDurationNormal=%llu averageDurationFull=%llu averageDurationIncremental=%llu averageDurationDifferential=%llu averageDurationContinuous=%llu totalEntityCount=%lu totalStorageCount=%lu totalStorageSize=%llu totalEntryCount=%lu totalEntrySize=%llu",
                          uuidId,
                          lastExecutedDateTime,
                          lastErrorMessage,
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
      ServerIO_sendResult(&connectorInfo->io,
                          id,
                          TRUE,
                          ERROR_NONE,
                          "uuidId=%lld",
                          INDEX_ID_NONE
                         );
    }
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  String_delete(lastErrorMessage);
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
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<text>");
    return;
  }

  if (indexHandle != NULL)
  {
    // create new UUID
    error = Index_newUUID(indexHandle,jobUUID,&uuidId);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"uuidId=%lld",uuidId);
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
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
*            archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL
*            createdDateTime=<n>
*            locked=yes|no
*          Result:
*            entityId=<n>
\***********************************************************************/

LOCAL void connectorCommand_indexNewEntity(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString (scheduleUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes archiveType;
  uint64       createdDateTime;
  bool         locked;
  Errors       error;
  IndexId      entityId;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get jobUUID, scheduleUUID, archiveType, createdDateTime, locked
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<text>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"scheduleUUID=<text>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)Archive_parseType,ARCHIVE_TYPE_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    return;
  }
  StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL);
  StringMap_getBool(argumentMap,"locked",&locked,FALSE);

  if (indexHandle != NULL)
  {
    // create new entity
    error = Index_newEntity(indexHandle,jobUUID,scheduleUUID,archiveType,createdDateTime,locked,&entityId);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"entityId=%lld",entityId);
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
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
*            entityId=<n>
*            storageName=<name>
*            createdDateTime=<n>
*            size=<n>
*            indexState=<n>
*            indexMode=<n>
*          Result:
*            storageId=<n>
\***********************************************************************/

LOCAL void connectorCommand_indexNewStorage(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId      entityId;
  String       storageName;
  uint64       createdDateTime;
  uint64       size;
  IndexStates  indexState;
  IndexModes   indexMode;
  Errors       error;
  IndexId      storageId;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get entityId, storageName, createdDateTime, size, indexMode, indexState
  if (!StringMap_getInt64(argumentMap,"entityId",&entityId,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<n>");
    return;
  }
  storageName = String_new();
  if (!StringMap_getString(argumentMap,"storageName",storageName,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageName=<name>");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"createdDateTime=<n>");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"size=<n>");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"indexState",&indexState,(StringMapParseEnumFunction)Index_parseState,INDEX_STATE_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexState=NONE|OK|CREATE|UPDATE_REQUESTED|UPDATE|ERROR");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"indexMode",&indexMode,(StringMapParseEnumFunction)Index_parseMode,INDEX_MODE_MANUAL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexMode=MANUAL|AUTO");
    String_delete(storageName);
    return;
  }

  if (indexHandle != NULL)
  {
    // create new storage
    error = Index_newStorage(indexHandle,
                             entityId,
                             connectorInfo->io.network.name,
                             storageName,
                             createdDateTime,
                             size,
                             indexState,
                             indexMode,
                             &storageId
                            );
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      String_delete(storageName);
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"storageId=%lld",storageId);
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
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
  IndexId storageId;
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

  // get storageId, name, size, timeLastAccess, timeModified, timeLastChanged, userId, groupId, permission, fragmentOffset, fragmentSize
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"a not a storage index id %llx",storageId);
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"size=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastAccess=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeModified=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastChanged=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"userId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"groupId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"permission=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentOffset",&fragmentOffset,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"fragmentOffset=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentSize",&fragmentSize,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"fragmentSize=<n>");
    String_delete(name);
    return;
  }

  if (indexHandle != NULL)
  {
    // add index file entry
    error = Index_addFile(indexHandle,
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
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      String_delete(name);
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
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
  IndexId         storageId;
  String          name;
  FileSystemTypes fileSystemType;
  uint64          size;
  ulong           blockSize;
  uint64          blockOffset;
  uint64          blockCount;
  Errors          error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get storageId, name, fileSystemType, size, blockSize, blockOffset, blockCount
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"b not a storage index id %llx",storageId);
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"fileSystemType",&fileSystemType,(StringMapParseEnumFunction)FileSystem_parseFileSystemType,FILE_SYSTEM_TYPE_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"fileSystemType=CHARACTER_DEVICE|BLOCK_DEVICE|FIFO|SOCKET|OTHER");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"size=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getULong(argumentMap,"blockSize",&blockSize,0L))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"blockSize=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"blockOffset",&blockOffset,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"blockOffset=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"blockCount",&blockCount,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"blockCount=<n>");
    String_delete(name);
    return;
  }

  if (indexHandle != NULL)
  {
    // add index image entry
    error = Index_addImage(indexHandle,
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
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      String_delete(name);
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
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
  IndexId storageId;
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

  // get storageId, name, timeLastAccess, timeModified, timeLastChanged, userId, groupId, permission
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"c not a storage index id %llx",storageId);
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastAccess=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeModified=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastChanged=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"userId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"groupId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"permission=<n>");
    String_delete(name);
    return;
  }

  if (indexHandle != NULL)
  {
    // add index directory entry
    error = Index_addDirectory(indexHandle,
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
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      String_delete(name);
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
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
  IndexId storageId;
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

  // get storageId, name, destinationName, timeLastAccess, timeModified, timeLastChanged, userId, groupId, permission
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"d not a storage index id %llx",storageId);
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  destinationName = String_new();
  if (!StringMap_getString(argumentMap,"destinationName",destinationName,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"destinationName=<name>");
    String_delete(destinationName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastAccess=<n>");
    String_delete(destinationName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeModified=<n>");
    String_delete(destinationName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastChanged=<n>");
    String_delete(destinationName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"userId=<n>");
    String_delete(destinationName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"groupId=<n>");
    String_delete(destinationName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"permission=<n>");
    String_delete(destinationName);
    String_delete(name);
    return;
  }

  if (indexHandle != NULL)
  {
    // add index link entry
    error = Index_addLink(indexHandle,
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
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      String_delete(destinationName);
      String_delete(name);
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
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
  IndexId storageId;
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

  // get storageId, name, size, timeLastAccess, timeModified, timeLastChanged, userId, groupId, permission, fragmentOffset, fragmentSize
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"e not a storage index id %llx",storageId);
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"size",&size,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"size=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastAccess=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeModified=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastChanged=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"userId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"groupId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"permission=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentOffset",&fragmentOffset,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"fragmentOffset=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"fragmentSize",&fragmentSize,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"fragmentSize=<n>");
    String_delete(name);
    return;
  }

  if (indexHandle != NULL)
  {
    // add index hardlink entry
    error = Index_addHardlink(indexHandle,
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
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      String_delete(name);
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
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
  IndexId          storageId;
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

  // get storageId, name, specialType, timeLastAccess, timeModified, timeLastChanged, userId, groupId, permission, fragmentOffset, fragmentSize
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"f not a storage index id %llx",storageId);
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"specialType",&specialType,(StringMapParseEnumFunction)File_parseFileSpecialType,FILE_SPECIAL_TYPE_OTHER))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"specialType=CHARACTER_DEVICE|BLOCK_DEVICE|FIFO|SOCKET|OTHER");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastAccess",&timeLastAccess,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastAccess=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeModified",&timeModified,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeModified=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"timeLastChanged",&timeLastChanged,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"timeLastChanged=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"userId",&userId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"userId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"groupId",&groupId,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"groupId=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"permission",&permission,0))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"permission=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"major",&major,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"major=<n>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"minor",&minor,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"minor=<n>");
    String_delete(name);
    return;
  }

  if (indexHandle != NULL)
  {
    // add index special entry
    error = Index_addSpecial(indexHandle,
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
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      String_delete(name);
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : connectorCommand_indexPruneUUID
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

LOCAL void connectorCommand_indexPruneUUID(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId uuidId;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get uuidId
  if (!StringMap_getInt64(argumentMap,"uuidId",&uuidId,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"uuidId=<n>");
    return;
  }

  if (indexHandle != NULL)
  {
    // prune UUID
    error = Index_pruneUUID(indexHandle,uuidId);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
}

/***********************************************************************\
* Name   : connectorCommand_indexPruneEntity
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

LOCAL void connectorCommand_indexPruneEntity(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId entityId;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get entityId
  if (!StringMap_getInt64(argumentMap,"entityId",&entityId,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"entityId=<n>");
    return;
  }

  if (indexHandle != NULL)
  {
    // prune entity
    error = Index_pruneEntity(indexHandle,entityId);
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
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
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexId=<n>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"indexState",&indexState,(StringMapParseEnumFunction)Index_parseState,INDEX_STATE_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"indexState=NONE|OK|CREATE|UPDATE_REQUESTED|UPDATE|ERROR");
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"lastCheckedDateTime",&lastCheckedDateTime,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"lastCheckedDateTime=<n>");
    return;
  }
  errorMessage = String_new();
  if (!StringMap_getString(argumentMap,"errorMessage",errorMessage,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"errorMessage=<name>");
    String_delete(errorMessage);
    return;
  }

  if (indexHandle != NULL)
  {
    // set state
    error = Index_setState(indexHandle,
                           indexId,
                           indexState,
                           lastCheckedDateTime,
                           "%s",
                           String_cString(errorMessage)
                          );
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      String_delete(errorMessage);
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  String_delete(errorMessage);
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
*            storageName=<text>
*            storageSize=<n>
*          Result:
\***********************************************************************/

LOCAL void connectorCommand_indexStorageUpdate(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId storageId;
  String  storageName;
  uint64  storageSize;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get storageId, storageName, storageSize
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"g not a storage index id %llx",storageId);
    return;
  }
  storageName = String_new();
  if (!StringMap_getString(argumentMap,"storageName",storageName,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageName=<name>");
    String_delete(storageName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"storageSize",&storageSize,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageSize=<n>");
    String_delete(storageName);
    return;
  }

  if (indexHandle != NULL)
  {
    // update storage
    error = Index_storageUpdate(indexHandle,
                                storageId,
                                storageName,
                                storageSize
                               );
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      String_delete(storageName);
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
  String_delete(storageName);
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
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"h not a storage index id %llx",storageId);
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
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
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

LOCAL void connectorCommand_indexStorageDelete(ConnectorInfo *connectorInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId storageId;
  Errors  error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get storageId
  if (!StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"storageId=<n>");
    return;
  }
  if (Index_getType(storageId) != INDEX_TYPE_STORAGE)
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INVALID_INDEX,"i not a storage index id %llx",storageId);
    return;
  }

  if (indexHandle != NULL)
  {
    // delete storage
    error = Index_deleteStorage(indexHandle,
                                storageId
                               );
    if (error != ERROR_NONE)
    {
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
  }

  // free resources
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
  ulong        totalEntryCount;
  uint64       totalEntrySize;
  ulong        skippedEntryCount;
  uint64       skippedEntrySize;
  ulong        errorEntryCount;
  uint64       errorEntrySize;
  Errors       error;
  IndexId      historyId;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);
  assert(connectorInfo->io.type == SERVER_IO_TYPE_NETWORK);

  // get jobUUID, scheduleUUID, hostName, archiveType, createdDateTime, errorMessage, duration, totalEntryCount, totalEntrySize, skippedEntryCount, skippedEntrySize, errorEntryCount, errorEntrySize
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"jobUUID=<text>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"scheduleUUID=<text>");
    return;
  }
  hostName = String_new();
  if (!StringMap_getString(argumentMap,"hostName",hostName,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"hostName=<text>");
    String_delete(hostName);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)Archive_parseType,ARCHIVE_TYPE_NONE))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"createdDateTime=<n>");
    String_delete(hostName);
    return;
  }
  errorMessage = String_new();
  if (!StringMap_getString(argumentMap,"errorMessage",errorMessage,NULL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"hostName=<text>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"duration",&duration,0L))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"duration=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getULong(argumentMap,"totalEntryCount",&totalEntryCount,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"totalEntryCount=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"totalEntrySize",&totalEntrySize,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"totalEntrySize=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getULong(argumentMap,"skippedEntryCount",&skippedEntryCount,0L))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"skippedEntryCount=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"skippedEntrySize",&skippedEntrySize,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"skippedEntrySize=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getULong(argumentMap,"errorEntryCount",&errorEntryCount,0L))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"errorEntryCount=<n>");
    String_delete(errorMessage);
    String_delete(hostName);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"errorEntrySize",&errorEntrySize,0LL))
  {
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_EXPECTED_PARAMETER,"errorEntrySize=<n>");
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
      ServerIO_sendResult(&connectorInfo->io,id,TRUE,error,"%s",Error_getData(error));
      String_delete(errorMessage);
      String_delete(hostName);
      return;
    }

    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_NONE,"historyId=%lld",historyId);
  }
  else
  {
    // send result
    ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
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
  { "INDEX_PRUNE_UUID",          connectorCommand_indexPruneUUID          },
  { "INDEX_PRUNE_ENTITY",        connectorCommand_indexPruneEntity        },

  { "INDEX_SET_STATE",           connectorCommand_indexSetState           },
  { "INDEX_STORAGE_UPDATE",      connectorCommand_indexStorageUpdate      },
  { "INDEX_STORAGE_UPDATE_INFOS",connectorCommand_indexStorageUpdateInfos },
  { "INDEX_STORAGE_DELETE",      connectorCommand_indexStorageDelete      },

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
  i = 0;
  while ((i < SIZE_OF_ARRAY(CONNECTOR_COMMANDS)) && !String_equalsCString(name,CONNECTOR_COMMANDS[i].name))
  {
    i++;
  }
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
  sigset_t                 signalMask;
  IndexHandle              *indexHandle;
  struct pollfd            pollfds[1];
  struct timespec          pollTimeout;
  int                      n;
  uint                     id;
  ConnectorCommandFunction connectorCommandFunction;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  // init variables
  name        = String_new();
  argumentMap = StringMap_new();

  // Note: ignore SIGALRM in ppoll()
  sigemptyset(&signalMask);
  sigaddset(&signalMask,SIGALRM);

  // init index
  indexHandle = Index_open(NULL,INDEX_TIMEOUT);

  // process client requests
  while (   Connector_isConnected(connectorInfo)
         && !Thread_isQuit(&connectorInfo->thread)
        )
  {
    // wait for disconnect, command, or result
    pollfds[0].fd     = Network_getSocket(&connectorInfo->io.network.socketHandle);
    pollfds[0].events = POLLIN|POLLERR|POLLNVAL;
    pollTimeout.tv_sec  = (long)(TIMEOUT /MS_PER_SECOND);
    pollTimeout.tv_nsec = (long)((TIMEOUT%MS_PER_SECOND)*1000LL);
    n = ppoll(pollfds,1,&pollTimeout,&signalMask);
    if (n > 0)
    {
      if      ((pollfds[0].revents & POLLIN) != 0)
      {
        // process commands
        if (ServerIO_receiveData(&connectorInfo->io))
        {
          while (ServerIO_getCommand(&connectorInfo->io,
                                     &id,
                                     name,
                                     argumentMap
                                    )
                )
          {
            // find command
            #ifdef CONNECTOR_DEBUG
//TODO: enable
              fprintf(stderr,"DEBUG: received command '%s'\n",String_cString(name));
              #ifndef NDEBUG
                StringMap_debugPrint(2,argumentMap);
              #endif
            #endif
            if (!findConnectorCommand(name,&connectorCommandFunction))
            {
              ServerIO_sendResult(&connectorInfo->io,id,TRUE,ERROR_UNKNOWN_COMMAND,"%S",name);
              continue;
            }
            assert(connectorCommandFunction != NULL);

            // process command
            connectorCommandFunction(connectorInfo,indexHandle,id,argumentMap);
          }
        }
        else
        {
          // disconnect -> stop
          setConnectorState(connectorInfo,CONNECTOR_STATE_DISCONNECTED);
        }
      }
      else if ((pollfds[0].revents & (POLLERR|POLLNVAL)) != 0)
      {
        // error/disconnect -> stop
        setConnectorState(connectorInfo,CONNECTOR_STATE_DISCONNECTED);
      }
      #ifndef NDEBUG
        else
        {
          HALT_INTERNAL_ERROR("unknown poll events 0x%x",pollfds[0].revents);
        }
      #endif /* NDEBUG */
    }
  }

  // done index
  Index_close(indexHandle);

  // free resources
  StringMap_delete(argumentMap);
  String_delete(name);
}

/***********************************************************************\
* Name   : Connector_vexecuteCommand
* Purpose: execute command on connector host
* Input  : connectorInfo - connector info
*          timeout       - timeout [ms] or WAIT_FOREVER
*          resultMap     - result map variable (can be NULL)
*          format        - command
*          arguments     - arguments
* Output : resultMap - result map
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors Connector_vexecuteCommand(ConnectorInfo *connectorInfo,
                                       uint          debugLevel,
                                       long          timeout,
                                       StringMap     resultMap,
                                       const char    *format,
                                       va_list       arguments
                                      )
{
  Errors error;

  assert(connectorInfo != NULL);

//TODO
  // init variables

  error = ServerIO_vexecuteCommand(&connectorInfo->io,
                                   debugLevel,
                                   timeout,
                                   resultMap,
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
                         uint          hostPort
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
                           hostPort
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

Errors Connector_authorize(ConnectorInfo *connectorInfo)
{
  Errors error;
  String hostName;
  String encryptedUUID;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  // init variables
  hostName      = String_new();
  encryptedUUID = String_new();

  // get host name/encrypted UUID for authorization
  error = ServerIO_encryptData(&connectorInfo->io,
                               String_cString(uuid),
                               String_length(uuid),
                               encryptedUUID
                              );
  if (error != ERROR_NONE)
  {
    String_delete(encryptedUUID);
    String_delete(hostName);
    return error;
  }
//fprintf(stderr,"%s, %d: uuid=%s encryptedUUID=%s\n",__FILE__,__LINE__,String_cString(uuid),String_cString(encryptedUUID));
//assert(ServerIO_decryptString(&connectorInfo->io,string,SERVER_IO_ENCRYPT_TYPE_RSA,encryptedUUID)==ERROR_NONE); fprintf(stderr,"%s, %d: dectecryp encryptedUUID: %s\n",__FILE__,__LINE__,String_cString(string));

  // authorize with UUID
  Network_getHostName(hostName);
  error = Connector_executeCommand(connectorInfo,
                                   CONNECTOR_DEBUG_LEVEL,
                                   CONNECTOR_COMMAND_TIMEOUT,
                                   NULL,  // resultMap
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

Errors Connector_initStorage(ConnectorInfo *connectorInfo,
                             ConstString   storageName,
                             JobOptions    *jobOptions,
                             StorageFlags  storageFlags
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
    printError("Cannot initialize storage '%s' (error: %s)",
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
                       storageFlags,
//TODO
CALLBACK(NULL,NULL),//                       CALLBACK(updateStorageStatusInfo,connectorInfo),
CALLBACK(NULL,NULL),//                       CALLBACK(getPasswordFunction,getPasswordUserData),
CALLBACK(NULL,NULL),//                       CALLBACK(storageRequestVolumeFunction,storageRequestVolumeUserData)
                       CALLBACK(NULL,NULL),  // isPause
                       CALLBACK(NULL,NULL),  // isAborted
                       NULL  // logHandle
                      );
  if (error != ERROR_NONE)
  {
    printError("Cannot initialize storage '%s' (error: %s)",
               String_cString(printableStorageName),
               Error_getText(error)
              );
    Storage_doneSpecifier(&storageSpecifier);
    return error;
  }
  DEBUG_TESTCODE() { Storage_done(&connectorInfo->storageInfo); Storage_doneSpecifier(&storageSpecifier); return DEBUG_TESTCODE_ERROR(); }

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
  }
  connectorInfo->storageOpenFlag = FALSE;

  // done storage
  Storage_done(&connectorInfo->storageInfo);

  return ERROR_NONE;
}

Errors Connector_executeCommand(ConnectorInfo *connectorInfo,
                                uint          debugLevel,
                                long          timeout,
                                StringMap     resultMap,
                                const char    *format,
                                ...
                               )
{
  va_list  arguments;
  Errors   error;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

  va_start(arguments,format);
  error = Connector_vexecuteCommand(connectorInfo,debugLevel,timeout,resultMap,format,arguments);
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
                                   NULL,
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
                        StorageFlags                 storageFlags,
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
    else                                                                     (*jobState) = JOB_STATE_NONE;

    return TRUE;
  }

  Errors     error;
  StringMap  resultMap;
  JobStates  state;
  uint       errorCode;
  String     errorData;
  StatusInfo statusInfo;

  assert(connectorInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(connectorInfo);

UNUSED_VARIABLE(getNamePasswordFunction);
UNUSED_VARIABLE(getNamePasswordUserData);
UNUSED_VARIABLE(storageRequestVolumeFunction);
UNUSED_VARIABLE(storageRequestVolumeUserData);

  // init variables
  errorData = String_new();
  initStatusInfo(&statusInfo);
  resultMap = StringMap_new();
  if (resultMap == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

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
                                   NULL,
                                   "JOB_DELETE jobUUID=%S",
                                   jobUUID
                                  );
    StringMap_delete(resultMap);
    doneStatusInfo(&statusInfo);
    String_delete(errorData);
    return error;
  }

  // start execute job
  error = Connector_executeCommand(connectorInfo,
                                   CONNECTOR_DEBUG_LEVEL,
                                   CONNECTOR_COMMAND_TIMEOUT,
                                   NULL,
                                   "JOB_START jobUUID=%S scheduleUUID=%S scheduleCustomText=%'S noStorage=%y archiveType=%s dryRun=%y noStorage=%y",
                                   jobUUID,
                                   scheduleUUID,
                                   NULL,  // scheduleCustomText
                                   FALSE,  // noStorage
                                   Archive_archiveTypeToString(archiveType),
                                   storageFlags.dryRun,
                                   storageFlags.noStorage
                                  );
  if (error != ERROR_NONE)
  {
    (void)Connector_executeCommand(connectorInfo,
                                   CONNECTOR_DEBUG_LEVEL,
                                   CONNECTOR_COMMAND_TIMEOUT,
                                   NULL,
                                   "JOB_DELETE jobUUID=%S",
                                   jobUUID
                                  );
    StringMap_delete(resultMap);
    doneStatusInfo(&statusInfo);
    String_delete(errorData);
    return error;
  }

  // wait until job terminated
  do
  {
    // get slave job status
    error = Connector_executeCommand(connectorInfo,
                                     CONNECTOR_DEBUG_LEVEL,
                                     CONNECTOR_COMMAND_TIMEOUT,
                                     resultMap,
                                     "JOB_STATUS jobUUID=%S",
                                     jobUUID
                                    );
    if (error == ERROR_NONE)
    {
      // get status values
      StringMap_getEnum  (resultMap,"state",                &state,(StringMapParseEnumFunction)parseJobState,JOB_STATE_NONE);
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
//      StringMap_getULong (resultMap,"entriesPerSecond",    &statusInfo.entriesPerSecond,0L);
//      StringMap_getULong (resultMap,"bytesPerSecond",    &statusInfo.bytesPerSecond,0L);
//      StringMap_getULong (resultMap,"storageBytesPerSecond",    &statusInfo.storageBytesPerSecond,0L);
//      StringMap_getULong (resultMap,"estimatedRestTime",    &statusInfo.estimatedRestTime,0L);

      // get error
      if (errorCode == ERROR_CODE_NONE)
      {
        error = ERROR_NONE;
      }
      else
      {
        error = Errorx_(errorCode,0,"%s",String_cString(errorData));
      }

      // update job status
      statusInfoFunction(error,&statusInfo,statusInfoUserData);
    }

    // sleep a short time
    Misc_mdelay(SLEEP_TIME_STATUS_UPDATE);
  }
  while (   Job_isRunning(state)
         && (error == ERROR_NONE)
         && Connector_isConnected(connectorInfo)
        );

  // free resources
  StringMap_delete(resultMap);
  doneStatusInfo(&statusInfo);
  String_delete(errorData);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
