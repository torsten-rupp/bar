/***********************************************************************\
*
* $Revision: 8947 $
* $Date: 2018-11-29 13:04:59 +0100 (Thu, 29 Nov 2018) $
* $Author: torsten $
* Contents: Backup ARchiver job functions
* Systems: all
*
\***********************************************************************/

#define __JOBS_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <pthread.h>
#include <locale.h>
#include <time.h>
#include <signal.h>
#include <assert.h>

#include "common/global.h"
#include "common/autofree.h"
#include "common/lists.h"
#include "common/strings.h"
#include "common/stringmaps.h"
#include "common/arrays.h"
#include "common/configvalues.h"
#include "common/threads.h"
#include "common/semaphores.h"
#include "common/msgqueues.h"
#include "common/stringlists.h"
#include "common/misc.h"
#include "common/network.h"
#include "common/files.h"
#include "common/devices.h"
#include "common/patterns.h"
#include "common/patternlists.h"
#include "common/passwords.h"

#include "errors.h"
#include "configuration.h"
#include "entrylists.h"
#include "archive.h"
#include "storage.h"
#include "index.h"
#include "continuous.h"
#include "server_io.h"
#include "connector.h"
#include "bar.h"

#include "jobs.h"

/****************** Conditional compilation switches *******************/

#define _NO_SESSION_ID
#define _SIMULATOR
#define _SIMULATE_PURGE

/***************************** Constants *******************************/

#define SESSION_KEY_SIZE                         1024     // number of session key bits

#define MAX_NETWORK_CLIENT_THREADS               3        // number of threads for a client
#define LOCK_TIMEOUT                             (10L*60L*MS_PER_SECOND)  // general lock timeout [ms]

#define SLAVE_DEBUG_LEVEL                        1
#define SLAVE_COMMAND_TIMEOUT                    (10L*MS_PER_SECOND)

#define AUTHORIZATION_PENALITY_TIME              500      // delay processing by failCount^2*n [ms]
#define MAX_AUTHORIZATION_PENALITY_TIME          30000    // max. penality time [ms]
#define MAX_AUTHORIZATION_HISTORY_KEEP_TIME      30000    // max. time to keep entries in authorization fail history [ms]
#define MAX_AUTHORIZATION_FAIL_HISTORY           64       // max. length of history of authorization fail clients
#define MAX_ABORT_COMMAND_IDS                    512      // max. aborted command ids history

#define MAX_SCHEDULE_CATCH_TIME                  30       // max. schedule catch time [days]

#define PAIRING_MASTER_TIMEOUT                   120      // timeout pairing new master [s]

// sleep times [s]
#define SLEEP_TIME_SLAVE_CONNECT_THREAD          ( 1*60)  // [s]
#define SLEEP_TIME_PAIRING_THREAD                ( 1*60)  // [s]
#define SLEEP_TIME_SCHEDULER_THREAD              ( 1*60)  // [s]
#define SLEEP_TIME_PAUSE_THREAD                  ( 1*60)  // [s]
#define SLEEP_TIME_INDEX_THREAD                  ( 1*60)  // [s]
#define SLEEP_TIME_AUTO_INDEX_UPDATE_THREAD      (10*60)  // [s]
#define SLEEP_TIME_PURGE_EXPIRED_ENTITIES_THREAD (10*60)  // [s]

// id none
#define ID_NONE                                  0

// keep all
#define KEEP_ALL                                 -1

// forever age
#define AGE_FOREVER                              -1

/***************************** Datatypes *******************************/

// parse special options
LOCAL bool configValueScheduleDateParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueScheduleDateFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);
LOCAL bool configValueScheduleWeekDaySetParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueScheduleWeekDaySetFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);
LOCAL bool configValueScheduleTimeParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueScheduleTimeFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

LOCAL bool configValuePersistenceMinKeepParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValuePersistenceMinKeepFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);
LOCAL bool configValuePersistenceMaxKeepParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValuePersistenceMaxKeepFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);
LOCAL bool configValuePersistenceMaxAgeParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValuePersistenceMaxAgeFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData);

// handle shortcuts

// handle deprecated configuration values
LOCAL bool configValueDeprecatedRemoteHostParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueDeprecatedRemotePortParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueDeprecatedArchiveFileModeOverwriteParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueDeprecatedRestoreEntryModeOverwriteParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueDeprecatedMountDeviceParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueDeprecatedScheduleMinKeepParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueDeprecatedScheduleMaxKeepParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueDeprecatedScheduleMaxAgeParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueDeprecatedStopOnErrorParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

ConfigValue JOB_CONFIG_VALUES[] = CONFIG_VALUE_ARRAY
(
  CONFIG_STRUCT_VALUE_STRING      ("UUID",                      JobNode,job.uuid                                 ,"<uuid>"),
  CONFIG_STRUCT_VALUE_STRING      ("slave-host-name",           JobNode,job.slaveHost.name                       ,"<name>"),
  CONFIG_STRUCT_VALUE_INTEGER     ("slave-host-port",           JobNode,job.slaveHost.port,                      0,65535,NULL,"<n>"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("slave-host-force-tls",      JobNode,job.slaveHost.forceTLS,                  "yes|no"),
  CONFIG_STRUCT_VALUE_STRING      ("archive-name",              JobNode,job.storageName                          ,"<name>"),
  CONFIG_STRUCT_VALUE_SELECT      ("archive-type",              JobNode,job.options.archiveType,                 CONFIG_VALUE_ARCHIVE_TYPES,"<type>"),

  CONFIG_STRUCT_VALUE_STRING      ("incremental-list-file",     JobNode,job.options.incrementalListFileName      ,"<file name>"),

  CONFIG_STRUCT_VALUE_INTEGER64   ("archive-part-size",         JobNode,job.options.archivePartSize,             0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS,"<size>"),

  CONFIG_STRUCT_VALUE_INTEGER     ("directory-strip",           JobNode,job.options.directoryStripCount,         -1,MAX_INT,NULL,"<n>"),
  CONFIG_STRUCT_VALUE_STRING      ("destination",               JobNode,job.options.destination                  ,"<directory>"),
  CONFIG_STRUCT_VALUE_SPECIAL     ("owner",                     JobNode,job.options.owner,                       configValueOwnerParse,configValueOwnerFormat,NULL),

  CONFIG_STRUCT_VALUE_SELECT      ("pattern-type",              JobNode,job.options.patternType,                 CONFIG_VALUE_PATTERN_TYPES,"<type>"),

  CONFIG_STRUCT_VALUE_SPECIAL     ("compress-algorithm",        JobNode,job.options.compressAlgorithms,          configValueCompressAlgorithmsParse,configValueCompressAlgorithmsFormat,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL     ("compress-exclude",          JobNode,job.options.compressExcludePatternList,  configValuePatternParse,configValuePatternFormat,NULL),

  CONFIG_STRUCT_VALUE_SPECIAL     ("crypt-algorithm",           JobNode,job.options.cryptAlgorithms,             configValueCryptAlgorithmsParse,configValueCryptAlgorithmsFormat,NULL),
  CONFIG_STRUCT_VALUE_SELECT      ("crypt-type",                JobNode,job.options.cryptType,                   CONFIG_VALUE_CRYPT_TYPES,"<type>"),
  CONFIG_STRUCT_VALUE_SELECT      ("crypt-password-mode",       JobNode,job.options.cryptPasswordMode,           CONFIG_VALUE_PASSWORD_MODES,"<mode>"),
  CONFIG_STRUCT_VALUE_SPECIAL     ("crypt-password",            JobNode,job.options.cryptPassword,               configValuePasswordParse,configValuePasswordFormat,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL     ("crypt-public-key",          JobNode,job.options.cryptPublicKey,              configValueKeyParse,configValueKeyFormat,NULL),

  CONFIG_STRUCT_VALUE_STRING      ("pre-command",               JobNode,job.options.preProcessScript             ,"<command>"),
  CONFIG_STRUCT_VALUE_STRING      ("post-command",              JobNode,job.options.postProcessScript            ,"<command>"),
  CONFIG_STRUCT_VALUE_STRING      ("slave-pre-command",         JobNode,job.options.slavePreProcessScript        ,"<command>"),
  CONFIG_STRUCT_VALUE_STRING      ("slave-post-command",        JobNode,job.options.slavePostProcessScript       ,"<command>"),

  CONFIG_STRUCT_VALUE_BOOLEAN     ("storage-on-master",         JobNode,job.options.storageOnMaster,             "yes|no"),

  CONFIG_STRUCT_VALUE_STRING      ("ftp-login-name",            JobNode,job.options.ftpServer.loginName          ,"<name>"),
  CONFIG_STRUCT_VALUE_SPECIAL     ("ftp-password",              JobNode,job.options.ftpServer.password,          configValuePasswordParse,configValuePasswordFormat,NULL),

  CONFIG_STRUCT_VALUE_INTEGER     ("ssh-port",                  JobNode,job.options.sshServer.port,              0,65535,NULL,"<n>"),
  CONFIG_STRUCT_VALUE_STRING      ("ssh-login-name",            JobNode,job.options.sshServer.loginName          ,"<name>"),
  CONFIG_STRUCT_VALUE_SPECIAL     ("ssh-password",              JobNode,job.options.sshServer.password,          configValuePasswordParse,configValuePasswordFormat,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL     ("ssh-public-key",            JobNode,job.options.sshServer.publicKey,         configValueKeyParse,configValueKeyFormat,NULL),
//  CONFIG_STRUCT_VALUE_SPECIAL     ("ssh-public-key-data",       JobNode,job.options.sshServer.publicKey,         configValueKeyParse,configValueKeyFormat,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL     ("ssh-private-key",           JobNode,job.options.sshServer.privateKey,        configValueKeyParse,configValueKeyFormat,NULL),
//  CONFIG_STRUCT_VALUE_SPECIAL     ("ssh-private-key-data",      JobNode,job.options.sshServer.privateKey,        configValueKeyParse,configValueKeyFormat,NULL),

  CONFIG_STRUCT_VALUE_SPECIAL     ("include-file",              JobNode,job.includeEntryList,                    configValueFileEntryPatternParse,configValueFileEntryPatternFormat,NULL),
  CONFIG_STRUCT_VALUE_STRING      ("include-file-list",         JobNode,job.options.includeFileListFileName      ,"<file name>"),
  CONFIG_STRUCT_VALUE_STRING      ("include-file-command",      JobNode,job.options.includeFileCommand           ,"<command>"),
  CONFIG_STRUCT_VALUE_SPECIAL     ("include-image",             JobNode,job.includeEntryList,                    configValueImageEntryPatternParse,configValueImageEntryPatternFormat,NULL),
  CONFIG_STRUCT_VALUE_STRING      ("include-image-list",        JobNode,job.options.includeImageListFileName     ,"<file name>"),
  CONFIG_STRUCT_VALUE_STRING      ("include-image-command",     JobNode,job.options.includeImageCommand          ,"<command>"),
  CONFIG_STRUCT_VALUE_SPECIAL     ("exclude",                   JobNode,job.excludePatternList,                  configValuePatternParse,configValuePatternFormat,NULL),
  CONFIG_STRUCT_VALUE_STRING      ("exclude-list",              JobNode,job.options.excludeListFileName          ,"<file name>"),
  CONFIG_STRUCT_VALUE_STRING      ("exclude-command",           JobNode,job.options.excludeCommand               ,"<command>"),
  CONFIG_STRUCT_VALUE_SPECIAL     ("delta-source",              JobNode,job.options.deltaSourceList,             configValueDeltaSourceParse,configValueDeltaSourceFormat,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL     ("mount",                     JobNode,job.options.mountList,                   configValueMountParse,configValueMountFormat,NULL),

  CONFIG_STRUCT_VALUE_INTEGER64   ("max-storage-size",          JobNode,job.options.maxStorageSize,              0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS,"<size>"),
  CONFIG_STRUCT_VALUE_INTEGER64   ("volume-size",               JobNode,job.options.volumeSize,                  0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS,"<size>"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("ecc",                       JobNode,job.options.errorCorrectionCodesFlag,    "yes|no"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("blank",                     JobNode,job.options.blankFlag,                   "yes|no"),

  CONFIG_STRUCT_VALUE_BOOLEAN     ("skip-unreadable",           JobNode,job.options.skipUnreadableFlag,          "yes|no"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("raw-images",                JobNode,job.options.rawImagesFlag,               "yes|no"),
  CONFIG_STRUCT_VALUE_SELECT      ("archive-file-mode",         JobNode,job.options.archiveFileMode,             CONFIG_VALUE_ARCHIVE_FILE_MODES,"<mode>"),
  CONFIG_STRUCT_VALUE_SELECT      ("restore-entry-mode",        JobNode,job.options.restoreEntryMode,            CONFIG_VALUE_RESTORE_ENTRY_MODES,"<mode>"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("wait-first-volume",         JobNode,job.options.waitFirstVolumeFlag,         "yes|no"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("no-signature",              JobNode,job.options.noSignatureFlag,             "yes|no"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("no-bar-on-medium",          JobNode,job.options.noBAROnMediumFlag,           "yes|no"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("no-stop-on-error",          JobNode,job.options.noStopOnErrorFlag,           "yes|no"),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("no-stop-on-attribute-error",JobNode,job.options.noStopOnAttributeErrorFlag,  "yes|no"),

  CONFIG_VALUE_BEGIN_SECTION("schedule",NULL,-1,NULL,NULL,NULL,NULL),
    CONFIG_STRUCT_VALUE_STRING    ("UUID",                      ScheduleNode,uuid                                ,"<uuid>"),
    CONFIG_STRUCT_VALUE_STRING    ("parentUUID",                ScheduleNode,parentUUID                          ,"<uuid>"),
    CONFIG_STRUCT_VALUE_SPECIAL   ("date",                      ScheduleNode,date,                               configValueScheduleDateParse,configValueScheduleDateFormat,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL   ("weekdays",                  ScheduleNode,weekDaySet,                         configValueScheduleWeekDaySetParse,configValueScheduleWeekDaySetFormat,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL   ("time",                      ScheduleNode,time,                               configValueScheduleTimeParse,configValueScheduleTimeFormat,NULL),
    CONFIG_STRUCT_VALUE_SELECT    ("archive-type",              ScheduleNode,archiveType,                        CONFIG_VALUE_ARCHIVE_TYPES,"<type>"),
    CONFIG_STRUCT_VALUE_INTEGER   ("interval",                  ScheduleNode,interval,                           0,MAX_INT,NULL,"<n>"),
    CONFIG_STRUCT_VALUE_STRING    ("text",                      ScheduleNode,customText                          ,"<text>"),
    CONFIG_STRUCT_VALUE_BOOLEAN   ("no-storage",                ScheduleNode,noStorage,                          "yes|no"),
    CONFIG_STRUCT_VALUE_BOOLEAN   ("enabled",                   ScheduleNode,enabled,                            "yes|no"),

    // deprecated
    CONFIG_VALUE_DEPRECATED       ("min-keep",                  NULL,-1,                                         configValueDeprecatedScheduleMinKeepParse,NULL,NULL,FALSE),
    CONFIG_VALUE_DEPRECATED       ("max-keep",                  NULL,-1,                                         configValueDeprecatedScheduleMaxKeepParse,NULL,NULL,FALSE),
    CONFIG_VALUE_DEPRECATED       ("max-age",                   NULL,-1,                                         configValueDeprecatedScheduleMaxAgeParse,NULL,NULL,FALSE),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_VALUE_BEGIN_SECTION("persistence",NULL,-1,NULL,NULL,NULL,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL   ("min-keep",                  PersistenceNode,minKeep,                         configValuePersistenceMinKeepParse,configValuePersistenceMinKeepFormat,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL   ("max-keep",                  PersistenceNode,maxKeep,                         configValuePersistenceMaxKeepParse,configValuePersistenceMaxKeepFormat,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL   ("max-age",                   PersistenceNode,maxAge,                          configValuePersistenceMaxAgeParse,configValuePersistenceMaxAgeFormat,NULL),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_STRUCT_VALUE_STRING      ("comment",                   JobNode,job.options.comment                      ,"<text>"),

  // deprecated
  CONFIG_STRUCT_VALUE_DEPRECATED  ("remote-host-name",          JobNode,job.slaveHost.name,                      configValueDeprecatedRemoteHostParse,NULL,"slave-host-name",TRUE),
  CONFIG_STRUCT_VALUE_DEPRECATED  ("remote-host-port",          JobNode,job.slaveHost.port,                      configValueDeprecatedRemotePortParse,NULL,"slave-host-port",TRUE),
  CONFIG_STRUCT_VALUE_DEPRECATED  ("remote-host-force-ssl",     JobNode,job.slaveHost.forceTLS,                  ConfigValue_parseDeprecatedBoolean,NULL,"slave-host-force-tls",TRUE),
  CONFIG_STRUCT_VALUE_DEPRECATED  ("slave-host-force-ssl",      JobNode,job.slaveHost.forceTLS,                  ConfigValue_parseDeprecatedBoolean,NULL,"slave-host-force-tls",TRUE),
  // Note: archive-file-mode=overwrite
  CONFIG_STRUCT_VALUE_DEPRECATED  ("overwrite-archive-files",   JobNode,job.options.archiveFileMode,             configValueDeprecatedArchiveFileModeOverwriteParse,NULL,"archive-file-mode",TRUE),
  // Note: restore-entry-mode=overwrite
  CONFIG_STRUCT_VALUE_DEPRECATED  ("overwrite-files",           JobNode,job.options.restoreEntryMode,            configValueDeprecatedRestoreEntryModeOverwriteParse,NULL,"restore-entry-mode=overwrite",TRUE),
  CONFIG_STRUCT_VALUE_DEPRECATED  ("mount-device",              JobNode,job.options.mountList,                   configValueDeprecatedMountDeviceParse,NULL,"mount",TRUE),
  CONFIG_STRUCT_VALUE_DEPRECATED  ("stop-on-error",             JobNode,job.options.noStopOnErrorFlag,           configValueDeprecatedStopOnErrorParse,NULL,"no-stop-on-error",TRUE),

  // ignored
  CONFIG_VALUE_IGNORE             ("schedule",                                                                   NULL,TRUE),
);

/***************************** Variables *******************************/
SlaveList slaveList;
JobList   jobList;

#ifndef NDEBUG
uint64 jobListLockTimestamp;
#endif /* NDEBUG */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : freeScheduleNode
* Purpose: free schedule node
* Input  : scheduleNode - schedule node
*          userData     - not used
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeScheduleNode(ScheduleNode *scheduleNode, void *userData)
{
  assert(scheduleNode != NULL);
  assert(scheduleNode->uuid != NULL);
  assert(scheduleNode->customText != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(scheduleNode->customText);
  String_delete(scheduleNode->parentUUID);
  String_delete(scheduleNode->uuid);
}

/***********************************************************************\
* Name   : newScheduleNode
* Purpose: allocate new schedule node
* Input  : -
* Output : -
* Return : new schedule node
* Notes  : -
\***********************************************************************/

LOCAL ScheduleNode *newScheduleNode(void)
{
  ScheduleNode *scheduleNode;

  scheduleNode = LIST_NEW_NODE(ScheduleNode);
  if (scheduleNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  scheduleNode->uuid                      = String_new();
  scheduleNode->parentUUID                = NULL;
  scheduleNode->date.year                 = DATE_ANY;
  scheduleNode->date.month                = DATE_ANY;
  scheduleNode->date.day                  = DATE_ANY;
  scheduleNode->weekDaySet                = WEEKDAY_SET_ANY;
  scheduleNode->time.hour                 = TIME_ANY;
  scheduleNode->time.minute               = TIME_ANY;
  scheduleNode->archiveType               = ARCHIVE_TYPE_NORMAL;
  scheduleNode->interval                  = 0;
  scheduleNode->customText                = String_new();
  scheduleNode->deprecatedPersistenceFlag = FALSE;
  scheduleNode->minKeep                   = 0;
  scheduleNode->maxKeep                   = 0;
  scheduleNode->maxAge                    = AGE_FOREVER;
  scheduleNode->noStorage                 = FALSE;
  scheduleNode->enabled                   = FALSE;

  scheduleNode->lastExecutedDateTime      = 0LL;
  scheduleNode->totalEntityCount          = 0L;
  scheduleNode->totalStorageCount         = 0L;
  scheduleNode->totalStorageSize          = 0LL;
  scheduleNode->totalEntryCount           = 0LL;
  scheduleNode->totalEntrySize            = 0LL;

  return scheduleNode;
}

/***********************************************************************\
* Name   : duplicateScheduleNode
* Purpose: duplicate schedule node
* Input  : fromScheduleNode - from schedule node
*          userData      - user data (not used)
* Output : -
* Return : duplicated schedule node
* Notes  : -
\***********************************************************************/

LOCAL ScheduleNode *duplicateScheduleNode(ScheduleNode *fromScheduleNode,
                                          void         *userData
                                         )
{
  ScheduleNode *scheduleNode;

  assert(fromScheduleNode != NULL);

  UNUSED_VARIABLE(userData);

  scheduleNode = LIST_NEW_NODE(ScheduleNode);
  if (scheduleNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  scheduleNode->uuid                      = Misc_getUUID(String_new());
  scheduleNode->parentUUID                = String_duplicate(fromScheduleNode->parentUUID);
  scheduleNode->date.year                 = fromScheduleNode->date.year;
  scheduleNode->date.month                = fromScheduleNode->date.month;
  scheduleNode->date.day                  = fromScheduleNode->date.day;
  scheduleNode->weekDaySet                = fromScheduleNode->weekDaySet;
  scheduleNode->time.hour                 = fromScheduleNode->time.hour;
  scheduleNode->time.minute               = fromScheduleNode->time.minute;
  scheduleNode->archiveType               = fromScheduleNode->archiveType;
  scheduleNode->interval                  = fromScheduleNode->interval;
  scheduleNode->customText                = String_duplicate(fromScheduleNode->customText);
  scheduleNode->deprecatedPersistenceFlag = fromScheduleNode->deprecatedPersistenceFlag;
  scheduleNode->minKeep                   = fromScheduleNode->minKeep;
  scheduleNode->maxKeep                   = fromScheduleNode->maxKeep;
  scheduleNode->maxAge                    = fromScheduleNode->maxAge;
  scheduleNode->noStorage                 = fromScheduleNode->noStorage;
  scheduleNode->enabled                   = fromScheduleNode->enabled;

  scheduleNode->lastExecutedDateTime      = fromScheduleNode->lastExecutedDateTime;
  scheduleNode->totalEntityCount          = 0L;
  scheduleNode->totalStorageCount         = 0L;
  scheduleNode->totalStorageSize          = 0LL;
  scheduleNode->totalEntryCount           = 0LL;
  scheduleNode->totalEntrySize            = 0LL;

  return scheduleNode;
}

/***********************************************************************\
* Name   : deleteScheduleNode
* Purpose: delete schedule node
* Input  : scheduleNode - schedule node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deleteScheduleNode(ScheduleNode *scheduleNode)
{
  assert(scheduleNode != NULL);

  freeScheduleNode(scheduleNode,NULL);
  LIST_DELETE_NODE(scheduleNode);
}

#if 0
//TODO: remove
/***********************************************************************\
* Name   : equalsScheduleNode
* Purpose: compare schedule nodes if equals
* Input  : scheduleNode1,scheduleNode2 - schedule nodes
* Output : -
* Return : TRUE iff scheduleNode1 = scheduleNode2
* Notes  : -
\***********************************************************************/

LOCAL bool equalsScheduleNode(const ScheduleNode *scheduleNode1, const ScheduleNode *scheduleNode2)
{
  assert(scheduleNode1 != NULL);
  assert(scheduleNode2 != NULL);

  if (   (scheduleNode1->date.year  != scheduleNode2->date.year )
      || (scheduleNode1->date.month != scheduleNode2->date.month)
      || (scheduleNode1->date.day   != scheduleNode2->date.day  )
     )
  {
    return 0;
  }

  if (scheduleNode1->weekDaySet != scheduleNode2->weekDaySet)
  {
    return 0;
  }

  if (   (scheduleNode1->time.hour   != scheduleNode2->time.hour )
      || (scheduleNode1->time.minute != scheduleNode2->time.minute)
     )
  {
    return 0;
  }

  if (scheduleNode1->archiveType != scheduleNode2->archiveType)
  {
    return 0;
  }

  if (scheduleNode1->interval != scheduleNode2->interval)
  {
    return 0;
  }

  if (!String_equals(scheduleNode1->customText,scheduleNode2->customText))
  {
    return 0;
  }

  if (scheduleNode1->minKeep != scheduleNode2->minKeep)
  {
    return 0;
  }

  if (scheduleNode1->maxKeep != scheduleNode2->maxKeep)
  {
    return 0;
  }

  if (scheduleNode1->maxAge != scheduleNode2->maxAge)
  {
    return 0;
  }

  if (scheduleNode1->noStorage != scheduleNode2->noStorage)
  {
    return 0;
  }

  return 1;
}
#endif

/***********************************************************************\
* Name   : freePersistenceNode
* Purpose: free persistence node
* Input  : persistenceNode - persistence node
*          userData        - not used
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freePersistenceNode(PersistenceNode *persistenceNode, void *userData)
{
  assert(persistenceNode != NULL);

  UNUSED_VARIABLE(persistenceNode);
  UNUSED_VARIABLE(userData);
}

/***********************************************************************\
* Name   : newPersistenceNode
* Purpose: allocate new persistence node
* Input  : archiveType     - archive type; see ArchiveTypes
*          minKeep,maxKeep - min./max. keep
*          maxAge          - max. age [days] or AGE_FOREVER
* Output : -
* Return : new persistence node
* Notes  : -
\***********************************************************************/

LOCAL PersistenceNode *newPersistenceNode(ArchiveTypes archiveType,
                                          int          minKeep,
                                          int          maxKeep,
                                          int          maxAge
                                         )
{
  PersistenceNode *persistenceNode;

  persistenceNode = LIST_NEW_NODE(PersistenceNode);
  if (persistenceNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  persistenceNode->id          = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
  persistenceNode->archiveType = archiveType;
  persistenceNode->minKeep     = minKeep;
  persistenceNode->maxKeep     = maxKeep;
  persistenceNode->maxAge      = maxAge;

  return persistenceNode;
}

/***********************************************************************\
* Name   : duplicatePersistenceNode
* Purpose: duplicate persistence node
* Input  : fromPersistenceNode - from persistence node
*          userData            - user data (not used)
* Output : -
* Return : duplicated persistence node
* Notes  : -
\***********************************************************************/

LOCAL PersistenceNode *duplicatePersistenceNode(PersistenceNode *fromPersistenceNode,
                                                void            *userData
                                               )
{
  PersistenceNode *persistenceNode;

  assert(fromPersistenceNode != NULL);

  UNUSED_VARIABLE(userData);

  persistenceNode = LIST_NEW_NODE(PersistenceNode);
  if (persistenceNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  persistenceNode->id          = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
  persistenceNode->archiveType = fromPersistenceNode->archiveType;
  persistenceNode->minKeep     = fromPersistenceNode->minKeep;
  persistenceNode->maxKeep     = fromPersistenceNode->maxKeep;
  persistenceNode->maxAge      = fromPersistenceNode->maxAge;

  return persistenceNode;
}

/***********************************************************************\
* Name   : deletePersistenceNode
* Purpose: delete persistence node
* Input  : persistenceNode - persistence node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deletePersistenceNode(PersistenceNode *persistenceNode)
{
  assert(persistenceNode != NULL);

  freePersistenceNode(persistenceNode,NULL);
  LIST_DELETE_NODE(persistenceNode);
}

/***********************************************************************\
* Name   : insertPersistenceNode
* Purpose: insert persistence node into list
* Input  : persistenceList - persistence list
*          persistenceNode - persistence node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void insertPersistenceNode(PersistenceList *persistenceList,
                                 PersistenceNode *persistenceNode
                                )
{
  PersistenceNode *nextPersistenceNode;

  assert(persistenceList != NULL);
  assert(persistenceNode != NULL);

  // find position in persistence list
  nextPersistenceNode = LIST_FIND_FIRST(persistenceList,
                                        nextPersistenceNode,
                                        (persistenceNode->maxAge != AGE_FOREVER) && ((nextPersistenceNode->maxAge == AGE_FOREVER) || (nextPersistenceNode->maxAge > persistenceNode->maxAge))
                                       );

  // insert into persistence list
  List_insert(persistenceList,persistenceNode,nextPersistenceNode);
}

/***********************************************************************\
* Name   : initOptionsFileServer
* Purpose: init file server
* Input  : fileServer - file server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initOptionsFileServer(FileServer *fileServer)
{
  assert(fileServer != NULL);
  assert(globalOptions.defaultFileServer != NULL);

  UNUSED_VARIABLE(fileServer);
}

/***********************************************************************\
* Name   : duplicateOptionsFileServer
* Purpose: duplicate file server
* Input  : fileServer     - file server
*          fromFileServer - from file server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void duplicateOptionsFileServer(FileServer *fileServer, const FileServer *fromFileServer)
{
  assert(fileServer != NULL);
  assert(fromFileServer != NULL);

  UNUSED_VARIABLE(fileServer);
  UNUSED_VARIABLE(fromFileServer);
}

/***********************************************************************\
* Name   : clearOptionsFileServer
* Purpose: clear file server
* Input  : fileServer - file server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearOptionsFileServer(FileServer *fileServer)
{
  assert(fileServer != NULL);

  UNUSED_VARIABLE(fileServer);
}

/***********************************************************************\
* Name   : doneOptionsFileServer
* Purpose: done file server
* Input  : fileServer - file server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneOptionsFileServer(FileServer *fileServer)
{
  assert(fileServer != NULL);

  UNUSED_VARIABLE(fileServer);
}

/***********************************************************************\
* Name   : initOptionsFTPServer
* Purpose: init FTP server
* Input  : ftpServer - FTP server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initOptionsFTPServer(FTPServer *ftpServer)
{
  assert(ftpServer != NULL);
  assert(globalOptions.defaultFTPServer != NULL);

  ftpServer->loginName = String_new();
  Password_initDuplicate(&ftpServer->password,&globalOptions.defaultFTPServer->ftp.password);
}

/***********************************************************************\
* Name   : duplicateOptionsFTPServer
* Purpose: duplicate FTP server
* Input  : ftpServer     - FTP server
*          fromFTPServer - from FTP server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void duplicateOptionsFTPServer(FTPServer *ftpServer, const FTPServer *fromFTPServer)
{
  assert(ftpServer != NULL);
  assert(fromFTPServer != NULL);

  ftpServer->loginName = String_duplicate(fromFTPServer->loginName);
  Password_initDuplicate(&ftpServer->password,&fromFTPServer->password);
}

/***********************************************************************\
* Name   : clearOptionsFTPServer
* Purpose: clear FTP server
* Input  : ftpServer - FTP server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearOptionsFTPServer(FTPServer *ftpServer)
{
  assert(ftpServer != NULL);

  String_clear(ftpServer->loginName);
  Password_clear(&ftpServer->password);
}

/***********************************************************************\
* Name   : doneOptionsFTPServer
* Purpose: done FTP server
* Input  : ftpServer - FTP server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneOptionsFTPServer(FTPServer *ftpServer)
{
  assert(ftpServer != NULL);

  Password_done(&ftpServer->password);
  String_delete(ftpServer->loginName);
}

/***********************************************************************\
* Name   : initOptionsSSHServer
* Purpose: init SSH server
* Input  : sshServer - SSH server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initOptionsSSHServer(SSHServer *sshServer)
{
  assert(sshServer != NULL);
  assert(globalOptions.defaultSSHServer != NULL);

  sshServer->port      = 0;
  sshServer->loginName = String_new();
  Password_initDuplicate(&sshServer->password,&globalOptions.defaultSSHServer->ssh.password);
  duplicateKey(&sshServer->publicKey,&globalOptions.defaultSSHServer->ssh.publicKey);
  duplicateKey(&sshServer->privateKey,&globalOptions.defaultSSHServer->ssh.privateKey);
}

/***********************************************************************\
* Name   : duplicateOptionsSSHServer
* Purpose: duplicate SSH server
* Input  : sshServer     - SSH server
*          fromSSHServer - from SSH server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void duplicateOptionsSSHServer(SSHServer *sshServer, const SSHServer *fromSSHServer)
{
  assert(sshServer != NULL);
  assert(fromSSHServer != NULL);

  sshServer->port      = fromSSHServer->port;
  sshServer->loginName = String_duplicate(fromSSHServer->loginName);
  Password_initDuplicate(&sshServer->password,&fromSSHServer->password);
  duplicateKey(&sshServer->publicKey,&fromSSHServer->publicKey);
  duplicateKey(&sshServer->privateKey,&fromSSHServer->privateKey);
}

/***********************************************************************\
* Name   : clearOptionsSSHServer
* Purpose: clear SSH server
* Input  : sshServer - SSH server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearOptionsSSHServer(SSHServer *sshServer)
{
  assert(sshServer != NULL);

  sshServer->port = 0;
  String_clear(sshServer->loginName);
  Password_clear(&sshServer->password);
  clearKey(&sshServer->publicKey);
  clearKey(&sshServer->privateKey);
}

/***********************************************************************\
* Name   : doneOptionsSSHServer
* Purpose: done SSH server
* Input  : sshServer - SSH server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneOptionsSSHServer(SSHServer *sshServer)
{
  assert(sshServer != NULL);

  doneKey(&sshServer->privateKey);
  doneKey(&sshServer->publicKey);
  Password_done(&sshServer->password);
  String_delete(sshServer->loginName);
}

/***********************************************************************\
* Name   : initOptionsWebDAVServer
* Purpose: init webDAV server
* Input  : webDAVServer - webDAV server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initOptionsWebDAVServer(WebDAVServer *webDAVServer)
{
  assert(webDAVServer != NULL);
  assert(globalOptions.defaultWebDAVServer != NULL);

  webDAVServer->loginName = String_new();
  Password_initDuplicate(&webDAVServer->password,&globalOptions.defaultWebDAVServer->webDAV.password);
  duplicateKey(&webDAVServer->publicKey,&globalOptions.defaultWebDAVServer->webDAV.publicKey);
  duplicateKey(&webDAVServer->privateKey,&globalOptions.defaultWebDAVServer->webDAV.privateKey);
}

/***********************************************************************\
* Name   : duplicateOptionsWebDAVServer
* Purpose: duplicate webDAV server
* Input  : webDAVServer     - webDAV server
*          fromWEBDAVServer - from webDAV server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void duplicateOptionsWebDAVServer(WebDAVServer *webDAVServer, const WebDAVServer *fromWEBDAVServer)
{
  assert(webDAVServer != NULL);
  assert(fromWEBDAVServer != NULL);

  webDAVServer->loginName = String_duplicate(fromWEBDAVServer->loginName);
  Password_initDuplicate(&webDAVServer->password,&fromWEBDAVServer->password);
  duplicateKey(&webDAVServer->publicKey,&fromWEBDAVServer->publicKey);
  duplicateKey(&webDAVServer->privateKey,&fromWEBDAVServer->privateKey);
}

/***********************************************************************\
* Name   : clearOptionsWebDAVServer
* Purpose: clear webDAV server
* Input  : webDAVServer - webDAV server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearOptionsWebDAVServer(WebDAVServer *webDAVServer)
{
  assert(webDAVServer != NULL);

  String_clear(webDAVServer->loginName);
  Password_clear(&webDAVServer->password);
  clearKey(&webDAVServer->publicKey);
  clearKey(&webDAVServer->privateKey);
}

/***********************************************************************\
* Name   : doneOptionsWebDAVServer
* Purpose: done webDAV server
* Input  : webDAVServer - webDAV server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneOptionsWebDAVServer(WebDAVServer *webDAVServer)
{
  assert(webDAVServer != NULL);

  doneKey(&webDAVServer->privateKey);
  doneKey(&webDAVServer->publicKey);
  Password_done(&webDAVServer->password);
  String_delete(webDAVServer->loginName);
}

/***********************************************************************\
* Name   : initOptionsOpticalDisk
* Purpose: init options optical disk
* Input  : opticalDisk - optical disk options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initOptionsOpticalDisk(OpticalDisk *opticalDisk)
{
  assert(opticalDisk != NULL);

  opticalDisk->deviceName              = String_new();
  opticalDisk->requestVolumeCommand    = String_new();
  opticalDisk->unloadVolumeCommand     = String_new();
  opticalDisk->loadVolumeCommand       = String_new();
  opticalDisk->volumeSize              = 0LL;
  opticalDisk->imagePreProcessCommand  = String_new();
  opticalDisk->imagePostProcessCommand = String_new();
  opticalDisk->imageCommand            = String_new();
  opticalDisk->eccPreProcessCommand    = String_new();
  opticalDisk->eccPostProcessCommand   = String_new();
  opticalDisk->eccCommand              = String_new();
  opticalDisk->blankCommand            = String_new();
  opticalDisk->writePreProcessCommand  = String_new();
  opticalDisk->writePostProcessCommand = String_new();
  opticalDisk->writeCommand            = String_new();
  opticalDisk->writeImageCommand       = String_new();
}

/***********************************************************************\
* Name   : duplicateOptionsOpticalDisk
* Purpose: duplicate options optical disk
* Input  : opticalDisk     - optical disk options
*          fromOpticalDisk - from optical disk options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void duplicateOptionsOpticalDisk(OpticalDisk *opticalDisk, const OpticalDisk *fromOpticalDisk)
{
  assert(opticalDisk != NULL);
  assert(fromOpticalDisk != NULL);

  opticalDisk->deviceName              = String_duplicate(fromOpticalDisk->deviceName             );
  opticalDisk->requestVolumeCommand    = String_duplicate(fromOpticalDisk->requestVolumeCommand   );
  opticalDisk->unloadVolumeCommand     = String_duplicate(fromOpticalDisk->unloadVolumeCommand    );
  opticalDisk->loadVolumeCommand       = String_duplicate(fromOpticalDisk->loadVolumeCommand      );
  opticalDisk->volumeSize              = fromOpticalDisk->volumeSize;
  opticalDisk->imagePreProcessCommand  = String_duplicate(fromOpticalDisk->imagePreProcessCommand );
  opticalDisk->imagePostProcessCommand = String_duplicate(fromOpticalDisk->imagePostProcessCommand);
  opticalDisk->imageCommand            = String_duplicate(fromOpticalDisk->imageCommand           );
  opticalDisk->eccPreProcessCommand    = String_duplicate(fromOpticalDisk->eccPreProcessCommand   );
  opticalDisk->eccPostProcessCommand   = String_duplicate(fromOpticalDisk->eccPostProcessCommand  );
  opticalDisk->eccCommand              = String_duplicate(fromOpticalDisk->eccCommand             );
  opticalDisk->blankCommand            = String_duplicate(fromOpticalDisk->blankCommand           );
  opticalDisk->writePreProcessCommand  = String_duplicate(fromOpticalDisk->writePreProcessCommand );
  opticalDisk->writePostProcessCommand = String_duplicate(fromOpticalDisk->writePostProcessCommand);
  opticalDisk->writeCommand            = String_duplicate(fromOpticalDisk->writeCommand           );
  opticalDisk->writeImageCommand       = String_duplicate(fromOpticalDisk->writeImageCommand      );
}

/***********************************************************************\
* Name   : clearOptionsOpticalDisk
* Purpose: clear options optical disk
* Input  : opticalDisk - optical disc options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearOptionsOpticalDisk(OpticalDisk *opticalDisk)
{
  assert(opticalDisk != NULL);

  String_clear(opticalDisk->deviceName             );
  String_clear(opticalDisk->requestVolumeCommand   );
  String_clear(opticalDisk->unloadVolumeCommand    );
  String_clear(opticalDisk->loadVolumeCommand      );
  opticalDisk->volumeSize = 0LL;
  String_clear(opticalDisk->imagePreProcessCommand );
  String_clear(opticalDisk->imagePostProcessCommand);
  String_clear(opticalDisk->imageCommand           );
  String_clear(opticalDisk->eccPreProcessCommand   );
  String_clear(opticalDisk->eccPostProcessCommand  );
  String_clear(opticalDisk->eccCommand             );
  String_clear(opticalDisk->blankCommand           );
  String_clear(opticalDisk->writePreProcessCommand );
  String_clear(opticalDisk->writePostProcessCommand);
  String_clear(opticalDisk->writeCommand           );
  String_clear(opticalDisk->writeImageCommand      );
}

/***********************************************************************\
* Name   : doneOptionsOpticalDisk
* Purpose: done options optical disk
* Input  : opticalDisk - optical disc options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneOptionsOpticalDisk(OpticalDisk *opticalDisk)
{
  assert(opticalDisk != NULL);

  String_delete(opticalDisk->writeImageCommand      );
  String_delete(opticalDisk->writeCommand           );
  String_delete(opticalDisk->writePostProcessCommand);
  String_delete(opticalDisk->writePreProcessCommand );
  String_delete(opticalDisk->blankCommand           );
  String_delete(opticalDisk->eccCommand             );
  String_delete(opticalDisk->eccPostProcessCommand  );
  String_delete(opticalDisk->eccPreProcessCommand   );
  String_delete(opticalDisk->imageCommand           );
  String_delete(opticalDisk->imagePostProcessCommand);
  String_delete(opticalDisk->imagePreProcessCommand );
  String_delete(opticalDisk->loadVolumeCommand      );
  String_delete(opticalDisk->unloadVolumeCommand    );
  String_delete(opticalDisk->requestVolumeCommand   );
  String_delete(opticalDisk->deviceName             );
}

/***********************************************************************\
* Name   : initOptionsDevice
* Purpose: init options device
* Input  : device - device options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initOptionsDevice(Device *device)
{
  assert(device != NULL);

  device->name                    = String_new();
  device->requestVolumeCommand    = String_new();
  device->unloadVolumeCommand     = String_new();
  device->loadVolumeCommand       = String_new();
  device->volumeSize              = 0LL;
  device->imagePreProcessCommand  = String_new();
  device->imagePostProcessCommand = String_new();
  device->imageCommand            = String_new();
  device->eccPreProcessCommand    = String_new();
  device->eccPostProcessCommand   = String_new();
  device->eccCommand              = String_new();
  device->blankCommand            = String_new();
  device->writePreProcessCommand  = String_new();
  device->writePostProcessCommand = String_new();
  device->writeCommand            = String_new();
}

/***********************************************************************\
* Name   : duplicateOptionsDevice
* Purpose: duplicate options device
* Input  : device     - device options
*          fromDevice - from device options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void duplicateOptionsDevice(Device *device, const Device *fromDevice)
{
  assert(device != NULL);
  assert(fromDevice != NULL);

  device->name                    = String_duplicate(fromDevice->name                   );
  device->requestVolumeCommand    = String_duplicate(fromDevice->requestVolumeCommand   );
  device->unloadVolumeCommand     = String_duplicate(fromDevice->unloadVolumeCommand    );
  device->loadVolumeCommand       = String_duplicate(fromDevice->loadVolumeCommand      );
  device->volumeSize              = fromDevice->volumeSize;
  device->imagePreProcessCommand  = String_duplicate(fromDevice->imagePreProcessCommand );
  device->imagePostProcessCommand = String_duplicate(fromDevice->imagePostProcessCommand);
  device->imageCommand            = String_duplicate(fromDevice->imageCommand           );
  device->eccPreProcessCommand    = String_duplicate(fromDevice->eccPreProcessCommand   );
  device->eccPostProcessCommand   = String_duplicate(fromDevice->eccPostProcessCommand  );
  device->eccCommand              = String_duplicate(fromDevice->eccCommand             );
  device->blankCommand            = String_duplicate(fromDevice->blankCommand           );
  device->writePreProcessCommand  = String_duplicate(fromDevice->writePreProcessCommand );
  device->writePostProcessCommand = String_duplicate(fromDevice->writePostProcessCommand);
  device->writeCommand            = String_duplicate(fromDevice->writeCommand           );
}

/***********************************************************************\
* Name   : clearOptionsDevice
* Purpose: clear device
* Input  : device - device options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearOptionsDevice(Device *device)
{
  assert(device != NULL);

  String_clear(device->name                   );
  String_clear(device->requestVolumeCommand   );
  String_clear(device->unloadVolumeCommand    );
  String_clear(device->loadVolumeCommand      );
  device->volumeSize = 0LL;
  String_clear(device->imagePreProcessCommand );
  String_clear(device->imagePostProcessCommand);
  String_clear(device->imageCommand           );
  String_clear(device->eccPreProcessCommand   );
  String_clear(device->eccPostProcessCommand  );
  String_clear(device->eccCommand             );
  String_clear(device->blankCommand           );
  String_clear(device->writePreProcessCommand );
  String_clear(device->writePostProcessCommand);
  String_clear(device->writeCommand           );
}

/***********************************************************************\
* Name   : doneOptionsDevice
* Purpose: done device
* Input  : device - device options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneOptionsDevice(Device *device)
{
  assert(device != NULL);

  String_delete(device->writeCommand           );
  String_delete(device->writePostProcessCommand);
  String_delete(device->writePreProcessCommand );
  String_delete(device->blankCommand           );
  String_delete(device->eccCommand             );
  String_delete(device->eccPostProcessCommand  );
  String_delete(device->eccPreProcessCommand   );
  String_delete(device->imageCommand           );
  String_delete(device->imagePostProcessCommand);
  String_delete(device->imagePreProcessCommand );
  String_delete(device->loadVolumeCommand      );
  String_delete(device->unloadVolumeCommand    );
  String_delete(device->requestVolumeCommand   );
  String_delete(device->name                   );
}

/***********************************************************************\
* Name   : clearOptions
* Purpose: clear options
* Input  : jobOptions - job options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearOptions(JobOptions *jobOptions)
{
  uint i;

  assert(jobOptions != NULL);

  String_clear(jobOptions->uuid);

  String_clear(jobOptions->includeFileListFileName);
  String_clear(jobOptions->includeFileCommand);
  String_clear(jobOptions->includeImageListFileName);
  String_clear(jobOptions->includeImageCommand);
  String_clear(jobOptions->excludeListFileName);
  String_clear(jobOptions->excludeCommand);

  List_clear(&jobOptions->mountList,CALLBACK_((ListNodeFreeFunction)freeMountNode,NULL));
  PatternList_clear(&jobOptions->compressExcludePatternList);
  DeltaSourceList_clear(&jobOptions->deltaSourceList);
  List_clear(&jobOptions->scheduleList,CALLBACK_((ListNodeFreeFunction)freeScheduleNode,NULL));
  List_clear(&jobOptions->persistenceList,CALLBACK_((ListNodeFreeFunction)freePersistenceNode,NULL));
  jobOptions->persistenceList.lastModificationDateTime = 0LL;

  jobOptions->archiveType                = ARCHIVE_TYPE_NORMAL;

  jobOptions->archivePartSize            = globalOptions.archivePartSize;
  String_clear(jobOptions->incrementalListFileName);
  jobOptions->directoryStripCount        = DIRECTORY_STRIP_NONE;
  String_clear(jobOptions->destination);
  jobOptions->owner                      = globalOptions.owner;
  jobOptions->permissions                = globalOptions.permissions;
  jobOptions->patternType                = globalOptions.patternType;

  jobOptions->compressAlgorithms.delta   = COMPRESS_ALGORITHM_NONE;
  jobOptions->compressAlgorithms.byte    = COMPRESS_ALGORITHM_NONE;

  #ifdef HAVE_GCRYPT
    jobOptions->cryptType                = CRYPT_TYPE_SYMMETRIC;
  #else /* not HAVE_GCRYPT */
    jobOptions->cryptType                = CRYPT_TYPE_NONE;
  #endif /* HAVE_GCRYPT */
  for (i = 0; i < 4; i++)
  {
    jobOptions->cryptAlgorithms[i] = CRYPT_ALGORITHM_NONE;
  }
  jobOptions->cryptPasswordMode                 = PASSWORD_MODE_DEFAULT;
  Password_clear(&jobOptions->cryptPassword);
  clearKey(&jobOptions->cryptPublicKey);
  clearKey(&jobOptions->cryptPrivateKey);

  String_clear(jobOptions->preProcessScript );
  String_clear(jobOptions->postProcessScript);
  String_clear(jobOptions->slavePreProcessScript );
  String_clear(jobOptions->slavePostProcessScript);

  jobOptions->storageOnMaster            = TRUE;
  clearOptionsFileServer(&jobOptions->fileServer);
  clearOptionsFTPServer(&jobOptions->ftpServer);
  clearOptionsSSHServer(&jobOptions->sshServer);
  clearOptionsWebDAVServer(&jobOptions->webDAVServer);
  clearOptionsOpticalDisk(&jobOptions->opticalDisk);
  clearOptionsDevice(&jobOptions->device);

  String_clear(jobOptions->comment);

  jobOptions->skipUnreadableFlag         = FALSE;
  jobOptions->forceDeltaCompressionFlag  = FALSE;
  jobOptions->ignoreNoDumpAttributeFlag  = FALSE;
  jobOptions->archiveFileMode            = ARCHIVE_FILE_MODE_STOP;
  jobOptions->restoreEntryMode           = RESTORE_ENTRY_MODE_STOP;
  jobOptions->errorCorrectionCodesFlag   = FALSE;
  jobOptions->alwaysCreateImageFlag      = FALSE;
  jobOptions->blankFlag                  = FALSE;
  jobOptions->waitFirstVolumeFlag        = FALSE;
  jobOptions->rawImagesFlag              = FALSE;
  jobOptions->noFragmentsCheckFlag       = FALSE;
//TODO: job option or better global option only?
  jobOptions->noIndexDatabaseFlag        = FALSE;
  jobOptions->forceVerifySignaturesFlag  = FALSE;
  jobOptions->skipVerifySignaturesFlag   = FALSE;
  jobOptions->noSignatureFlag            = FALSE;
//TODO: job option or better global option only?
  jobOptions->noBAROnMediumFlag          = FALSE;
  jobOptions->noStopOnErrorFlag          = FALSE;
  jobOptions->noStopOnAttributeErrorFlag = FALSE;
}

/***********************************************************************\
* Name   : configValueScheduleDateParse
* Purpose: config value option call back for parsing schedule date
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValueScheduleDateParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  bool         errorFlag;
  String       s0,s1,s2;
  ScheduleDate date;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  errorFlag = FALSE;
  s0 = String_new();
  s1 = String_new();
  s2 = String_new();
  if      (String_parseCString(value,"%S-%S-%S",NULL,s0,s1,s2))
  {
    if (!parseDateNumber(s0,&date.year )) errorFlag = TRUE;
    if (!parseDateMonth (s1,&date.month)) errorFlag = TRUE;
    if (!parseDateNumber(s2,&date.day  )) errorFlag = TRUE;
  }
  else
  {
    errorFlag = TRUE;
  }
  String_delete(s2);
  String_delete(s1);
  String_delete(s0);
  if (errorFlag)
  {
    snprintf(errorMessage,errorMessageSize,"Cannot parse schedule date '%s'",value);
    return FALSE;
  }

  // store values
  (*(ScheduleDate*)variable) = date;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueScheduleDateFormat
* Purpose: format schedule config statement
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueScheduleDateFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (ScheduleDate*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<yyyy>|*-<mm>|*-<dd>|*");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const ScheduleDate *scheduleDate = (const ScheduleDate*)(*formatUserData);
        String             line          = (String)data;

        if (scheduleDate != NULL)
        {
          if (scheduleDate->year != DATE_ANY)
          {
            String_appendFormat(line,"%4d",scheduleDate->year);
          }
          else
          {
            String_appendCString(line,"*");
          }
          String_appendChar(line,'-');
          if (scheduleDate->month != DATE_ANY)
          {
            String_appendFormat(line,"%02d",scheduleDate->month);
          }
          else
          {
            String_appendCString(line,"*");
          }
          String_appendChar(line,'-');
          if (scheduleDate->day != DATE_ANY)
          {
            String_appendFormat(line,"%02d",scheduleDate->day);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueScheduleWeekDaySetParse
* Purpose: config value option call back for parsing schedule week day
*          set
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValueScheduleWeekDaySetParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  WeekDaySet weekDaySet;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  if (!parseWeekDaySet(value,&weekDaySet))
  {
    snprintf(errorMessage,errorMessageSize,"Cannot parse schedule weekday '%s'",value);
    return FALSE;
  }

  // store value
  (*(WeekDaySet*)variable) = weekDaySet;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueScheduleWeekDaySetFormat
* Purpose: format schedule config week day set
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueScheduleWeekDaySetFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (ScheduleWeekDaySet*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"Mon|Tue|Wed|Thu|Fri|Sat|Sun|*,...");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const ScheduleWeekDaySet *scheduleWeekDaySet = (const ScheduleWeekDaySet*)(*formatUserData);
        String                   names;
        String                   line                = (String)data;

        if (scheduleWeekDaySet != NULL)
        {
          if ((*scheduleWeekDaySet) != WEEKDAY_SET_ANY)
          {
            names = String_new();

            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_MON)) { String_joinCString(names,"Mon",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_TUE)) { String_joinCString(names,"Tue",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_WED)) { String_joinCString(names,"Wed",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_THU)) { String_joinCString(names,"Thu",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_FRI)) { String_joinCString(names,"Fri",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_SAT)) { String_joinCString(names,"Sat",','); }
            if (IN_SET(*scheduleWeekDaySet,WEEKDAY_SUN)) { String_joinCString(names,"Sun",','); }

            String_append(line,names);
            String_appendChar(line,' ');

            String_delete(names);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueScheduleTimeParse
* Purpose: config value option call back for parsing schedule time
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValueScheduleTimeParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  bool         errorFlag;
  String       s0,s1;
  ScheduleTime time;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  errorFlag = FALSE;
  s0 = String_new();
  s1 = String_new();
  if (String_parseCString(value,"%S:%S",NULL,s0,s1))
  {
    if (!parseTimeNumber(s0,&time.hour  )) errorFlag = TRUE;
    if (!parseTimeNumber(s1,&time.minute)) errorFlag = TRUE;
  }
  String_delete(s1);
  String_delete(s0);
  if (errorFlag)
  {
    snprintf(errorMessage,errorMessageSize,"Cannot parse schedule time '%s'",value);
    return FALSE;
  }

  // store values
  (*(ScheduleTime*)variable) = time;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueScheduleTimeFormat
* Purpose: format schedule config
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueScheduleTimeFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (ScheduleTime*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<hh>|*:<mm>|*");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const ScheduleTime *scheduleTime = (ScheduleTime*)(*formatUserData);
        String             line          = (String)data;

        if (scheduleTime != NULL)
        {
          if (scheduleTime->hour != TIME_ANY)
          {
            String_appendFormat(line,"%02d",scheduleTime->hour);
          }
          else
          {
            String_appendCString(line,"*");
          }
          String_appendChar(line,':');
          if (scheduleTime->minute != TIME_ANY)
          {
            String_appendFormat(line,"%02d",scheduleTime->minute);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValuePersistenceMinKeepParse
* Purpose: config value option call back for parsing min. keep
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValuePersistenceMinKeepParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  int minKeep;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  if (!stringEquals(value,"*"))
  {
    if (!String_parseCString(value,"%d",NULL,&minKeep))
    {
      snprintf(errorMessage,errorMessageSize,"Cannot parse persistence min. keep '%s'",value);
      return FALSE;
    }
  }
  else
  {
    minKeep = KEEP_ALL;
  }

  // store values
  (*(int*)variable) = minKeep;

  return TRUE;
}

/***********************************************************************\
* Name   : configValuePersistenceMinKeepFormat
* Purpose: format min. keep config
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValuePersistenceMinKeepFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (PersistenceNode*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<n>|*");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const PersistenceNode *persistenceNode = (const PersistenceNode*)(*formatUserData);
        String                line             = (String)data;

        if (persistenceNode != NULL)
        {
          if (persistenceNode->minKeep != KEEP_ALL)
          {
            String_appendFormat(line,"%d",persistenceNode->minKeep);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValuePersistenceMaxKeepParse
* Purpose: config value option call back for parsing max. keep
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValuePersistenceMaxKeepParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  int maxKeep;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  if (!stringEquals(value,"*"))
  {
    if (!String_parseCString(value,"%d",NULL,&maxKeep))
    {
      snprintf(errorMessage,errorMessageSize,"Cannot parse persistence max. keep '%s'",value);
      return FALSE;
    }
  }
  else
  {
    maxKeep = KEEP_ALL;
  }

  // store values
  (*(int*)variable) = maxKeep;

  return TRUE;
}

/***********************************************************************\
* Name   : configValuePersistenceMaxKeepFormat
* Purpose: format max. keep config
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValuePersistenceMaxKeepFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (PersistenceNode*)data;

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (PersistenceNode*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<n>|*");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const PersistenceNode *persistenceNode = (const PersistenceNode*)(*formatUserData);
        String                line             = (String)data;

        if (persistenceNode != NULL)
        {
          if (persistenceNode->maxKeep != KEEP_ALL)
          {
            String_appendFormat(line,"%d",persistenceNode->maxKeep);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParsePersistenceMaxAge
* Purpose: config value option call back for parsing max. age
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValuePersistenceMaxAgeParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  int maxAge;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse
  if (!stringEquals(value,"*"))
  {
    if (!String_parseCString(value,"%d",NULL,&maxAge))
    {
      snprintf(errorMessage,errorMessageSize,"Cannot parse persistence max. age '%s'",value);
      return FALSE;
    }
  }
  else
  {
    maxAge = AGE_FOREVER;
  }

  // store values
  (*(int*)variable) = maxAge;

  return TRUE;
}

/***********************************************************************\
* Name   : configValuePersistenceMaxAgeFormat
* Purpose: format max. age config
* Input  : formatUserData  - format user data
*          formatOperation - format operation
*          data            - operation data
*          userData        - user data
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValuePersistenceMaxAgeFormat(void **formatUserData, ConfigValueFormatOperations formatOperation, void *data, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  switch (formatOperation)
  {
    case CONFIG_VALUE_FORMAT_OPERATION_INIT:
      (*formatUserData) = (PersistenceNode*)data;
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_DONE:
      break;
    case CONFIG_VALUE_FORMAT_OPERATION_TEMPLATE:
      {
        String line = (String)data;
        String_appendFormat(line,"<n>|*");
      }
      break;
    case CONFIG_VALUE_FORMAT_OPERATION:
      {
        const PersistenceNode *persistenceNode = (const PersistenceNode*)(*formatUserData);
        String                line             = (String)data;

        if (persistenceNode != NULL)
        {
          if (persistenceNode->maxAge != AGE_FOREVER)
          {
            String_appendFormat(line,"%d",persistenceNode->maxAge);
          }
          else
          {
            String_appendCString(line,"*");
          }

          (*formatUserData) = NULL;

          return TRUE;
        }
        else
        {
          return FALSE;
        }
      }
      break;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedRemoteHostParse
* Purpose: config value option call back for deprecated remote host
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueDeprecatedRemoteHostParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  String string;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  // unquote/unescape
  string = String_newCString(value);
  String_unquote(string,STRING_QUOTES);
  String_unescape(string,
                  STRING_ESCAPE_CHARACTER,
                  STRING_ESCAPE_CHARACTERS_MAP_TO,
                  STRING_ESCAPE_CHARACTERS_MAP_FROM,
                  STRING_ESCAPE_CHARACTER_MAP_LENGTH
                );

  String_set(*((String*)variable),string);

  // free resources
  String_delete(string);

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedRemotePortParse
* Purpose: config value option call back for deprecated remote port
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool configValueDeprecatedRemotePortParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  uint n;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  if (!stringToUInt(value,&n))
  {
    stringFormat(errorMessage,errorMessageSize,"expected port number: 0..65535");
    return FALSE;
  }
  (*(uint*)variable) = n;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedArchiveFileModeOverwriteParse
* Purpose: config value option call back for deprecated overwrite-files
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValueDeprecatedArchiveFileModeOverwriteParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(value);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  (*(ArchiveFileModes*)variable) = ARCHIVE_FILE_MODE_OVERWRITE;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedRestoreEntryModeOverwriteParse
* Purpose: config value option call back for deprecated overwrite-files
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValueDeprecatedRestoreEntryModeOverwriteParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(value);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  (*(RestoreEntryModes*)variable) = RESTORE_ENTRY_MODE_OVERWRITE;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedMountDeviceParse
* Purpose: config value option call back for deprecated mount-device
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValueDeprecatedMountDeviceParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  String    string;
  MountNode *mountNode;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  // unquote/unescape
  string = String_newCString(value);
  String_unquote(string,STRING_QUOTES);
  String_unescape(string,
                  STRING_ESCAPE_CHARACTER,
                  STRING_ESCAPE_CHARACTERS_MAP_TO,
                  STRING_ESCAPE_CHARACTERS_MAP_FROM,
                  STRING_ESCAPE_CHARACTER_MAP_LENGTH
                );

  if (!stringIsEmpty(value))
  {
    // add to mount list
    mountNode = newMountNode(string,
                             NULL  // deviceName
                            );
    if (mountNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    List_append((MountList*)variable,mountNode);
  }

  // free resources
  String_delete(string);

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedScheduleMinKeepParse
* Purpose: config value option call back for deprecated min. keep of
*          schedule
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValueDeprecatedScheduleMinKeepParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  ((ScheduleNode*)variable)->minKeep                   = strtol(value,NULL,0);
  ((ScheduleNode*)variable)->deprecatedPersistenceFlag = TRUE;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedScheduleMaxKeepParse
* Purpose: config value option call back for deprecated max. keep of
*          schedule
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValueDeprecatedScheduleMaxKeepParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  ((ScheduleNode*)variable)->maxKeep                   = strtol(value,NULL,0);
  ((ScheduleNode*)variable)->deprecatedPersistenceFlag = TRUE;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedScheduleMaxAgeParse
* Purpose: config value option call back for deprecated max. age of
*          schedule
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValueDeprecatedScheduleMaxAgeParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  ((ScheduleNode*)variable)->maxAge                    = strtol(value,NULL,0);
  ((ScheduleNode*)variable)->deprecatedPersistenceFlag = TRUE;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueDeprecatedStopOnErrorParse
* Purpose: config value option call back for deprecated stop-on-error
* Input  : userData              - user data
*          variable              - config variable
*          name                  - config name
*          value                 - config value
*          maxErrorMessageLength - max. length of error message text
* Output : errorMessage - error message text
* Return : TRUE if config value parsed and stored in variable, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool configValueDeprecatedStopOnErrorParse(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if (value != NULL)
  {
    (*(bool*)variable) = !(   (stringEqualsIgnoreCase(value,"1") == 0)
                           || (stringEqualsIgnoreCase(value,"true") == 0)
                           || (stringEqualsIgnoreCase(value,"on") == 0)
                           || (stringEqualsIgnoreCase(value,"yes") == 0)
                          );
  }
  else
  {
    (*(bool*)variable) = FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : freeJobNode
* Purpose: free job node
* Input  : jobNode  - job node
*          userData - user data (no used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeJobNode(JobNode *jobNode, void *userData)
{
  assert(jobNode != NULL);
  assert(jobNode->job.uuid != NULL);
  assert(jobNode->name != NULL);

  UNUSED_VARIABLE(userData);

  Misc_performanceFilterDone(&jobNode->runningInfo.storageBytesPerSecondFilter);
  Misc_performanceFilterDone(&jobNode->runningInfo.bytesPerSecondFilter       );
  Misc_performanceFilterDone(&jobNode->runningInfo.entriesPerSecondFilter     );

  String_delete(jobNode->runningInfo.lastErrorMessage);

  String_delete(jobNode->volumeMessage);

  String_delete(jobNode->abortedByInfo);

  String_delete(jobNode->scheduleCustomText);
  String_delete(jobNode->scheduleUUID);

  doneStatusInfo(&jobNode->statusInfo);

  String_delete(jobNode->byName);

  Job_done(&jobNode->job);
  String_delete(jobNode->name);
  String_delete(jobNode->fileName);
}

/***********************************************************************\
* Name   : freeSlaveNode
* Purpose: free slave node
* Input  : slaveNode - slave node
*          userData  - user data (no used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeSlaveNode(SlaveNode *slaveNode, void *userData)
{
  assert(slaveNode != NULL);

  UNUSED_VARIABLE(userData);

  Connector_done(&slaveNode->connectorInfo);
  String_delete(slaveNode->name);
}

/*---------------------------------------------------------------------*/

Errors Job_initAll(void)
{
  Semaphore_init(&slaveList.lock,SEMAPHORE_TYPE_BINARY);
  Semaphore_init(&jobList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&jobList);

  return ERROR_NONE;
}

void Job_doneAll(void)
{
  List_done(&jobList,CALLBACK_((ListNodeFreeFunction)freeJobNode,NULL));
  Semaphore_done(&jobList.lock);
  List_done(&slaveList,CALLBACK_((ListNodeFreeFunction)freeSlaveNode,NULL));
  Semaphore_done(&slaveList.lock);
}

void Job_init(Job *job)
{
  assert(job != NULL);

  job->uuid                    = String_new();
  job->slaveHost.name          = String_new();
  job->slaveHost.port          = 0;
  job->slaveHost.forceTLS      = FALSE;
  job->storageName             = String_new();
  job->storageNameListStdin    = FALSE;
  job->storageNameListFileName = String_new();
  job->storageNameCommand      = String_new();
  EntryList_init(&job->includeEntryList);
  PatternList_init(&job->excludePatternList);
  Job_initOptions(&job->options);

  DEBUG_ADD_RESOURCE_TRACE(job,Job);
}

void Job_initDuplicate(Job *job, const Job *fromJob)
{
  assert(job != NULL);
  assert(fromJob != NULL);

  DEBUG_CHECK_RESOURCE_TRACE(fromJob);

  job->uuid                    = String_duplicate(fromJob->uuid);
  job->slaveHost.name          = String_duplicate(fromJob->slaveHost.name);
  job->slaveHost.port          = fromJob->slaveHost.port;
  job->slaveHost.forceTLS      = fromJob->slaveHost.forceTLS;

  job->storageName             = String_duplicate(fromJob->storageName);
  job->storageNameListStdin    = fromJob->storageNameListStdin;
  job->storageNameListFileName = String_duplicate(fromJob->storageNameListFileName);
  job->storageNameCommand      = String_duplicate(fromJob->storageNameCommand);

  EntryList_initDuplicate(&job->includeEntryList,
                          &fromJob->includeEntryList,
                          NULL,  // fromEntryListFromNode
                          NULL  // fromEntryListToNode
                         );
  PatternList_initDuplicate(&job->excludePatternList,
                            &fromJob->excludePatternList,
                            NULL,  // fromPatternListFromNode
                            NULL  // fromPatternListToNode
                           );
  Job_duplicateOptions(&job->options,&fromJob->options);

  DEBUG_ADD_RESOURCE_TRACE(job,Job);
}

void Job_done(Job *job)
{
  assert(job != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(job,Job);

  Job_doneOptions(&job->options);

  PatternList_done(&job->excludePatternList);
  EntryList_done(&job->includeEntryList);

  String_delete(job->storageNameCommand);
  String_delete(job->storageNameListFileName);
  String_delete(job->storageName);

  String_delete(job->slaveHost.name);
  String_delete(job->uuid);
}

JobNode *Job_new(JobTypes    jobType,
                 ConstString name,
                 ConstString jobUUID,
                 ConstString fileName
                )
{
  JobNode *jobNode;

  // allocate job node
  jobNode = LIST_NEW_NODE(JobNode);
  if (jobNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init job node
  Job_init(&jobNode->job);
  if (!String_isEmpty(jobUUID))
  {
    String_set(jobNode->job.uuid,jobUUID);
  }
  else
  {
    Misc_getUUID(jobNode->job.uuid);
  }
  jobNode->name                             = String_duplicate(name);
  jobNode->jobType                          = jobType;

  jobNode->modifiedFlag                     = FALSE;

  jobNode->lastScheduleCheckDateTime        = 0LL;

  jobNode->fileName                         = String_duplicate(fileName);
  jobNode->fileModified                     = 0LL;

  jobNode->masterIO                         = NULL;

  jobNode->jobState                         = JOB_STATE_NONE;
  jobNode->slaveState                       = SLAVE_STATE_OFFLINE;

  initStatusInfo(&jobNode->statusInfo);

  jobNode->scheduleUUID                     = String_new();
  jobNode->scheduleCustomText               = String_new();
  jobNode->archiveType                      = ARCHIVE_TYPE_NORMAL;
  jobNode->storageFlags                     = STORAGE_FLAGS_NONE;
  jobNode->byName                           = String_new();

  jobNode->requestedAbortFlag               = FALSE;
  jobNode->abortedByInfo                    = String_new();
  jobNode->requestedVolumeNumber            = 0;
  jobNode->volumeNumber                     = 0;
  jobNode->volumeMessage                    = String_new();
  jobNode->volumeUnloadFlag                 = FALSE;

  jobNode->runningInfo.lastExecutedDateTime = 0LL;
  jobNode->runningInfo.lastErrorMessage     = String_new();

  Misc_performanceFilterInit(&jobNode->runningInfo.entriesPerSecondFilter,     10*60);
  Misc_performanceFilterInit(&jobNode->runningInfo.bytesPerSecondFilter,       10*60);
  Misc_performanceFilterInit(&jobNode->runningInfo.storageBytesPerSecondFilter,10*60);

  Job_resetRunningInfo(jobNode);

  jobNode->executionCount.normal            = 0L;
  jobNode->executionCount.full              = 0L;
  jobNode->executionCount.incremental       = 0L;
  jobNode->executionCount.differential      = 0L;
  jobNode->executionCount.continuous        = 0L;
  jobNode->averageDuration.normal           = 0LL;
  jobNode->averageDuration.full             = 0LL;
  jobNode->averageDuration.incremental      = 0LL;
  jobNode->averageDuration.differential     = 0LL;
  jobNode->averageDuration.continuous       = 0LL;
  jobNode->totalEntityCount                 = 0L;
  jobNode->totalStorageCount                = 0L;
  jobNode->totalStorageSize                 = 0L;
  jobNode->totalEntryCount                  = 0L;
  jobNode->totalEntrySize                   = 0L;

  return jobNode;
}

JobNode *Job_copy(const JobNode *jobNode,
                  ConstString   fileName
                 )
{
  JobNode *newJobNode;

  // allocate job node
  newJobNode = LIST_NEW_NODE(JobNode);
  if (newJobNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init job node
  Job_initDuplicate(&newJobNode->job,&jobNode->job);
  newJobNode->name                             = File_getBaseName(String_new(),fileName);
  newJobNode->jobType                          = jobNode->jobType;

  newJobNode->modifiedFlag                     = TRUE;

  newJobNode->lastScheduleCheckDateTime        = 0LL;

  newJobNode->fileName                         = String_duplicate(fileName);
  newJobNode->fileModified                     = 0LL;

  newJobNode->masterIO                         = NULL;

  newJobNode->jobState                         = JOB_STATE_NONE;
  newJobNode->slaveState                       = SLAVE_STATE_OFFLINE;

  initStatusInfo(&newJobNode->statusInfo);

  newJobNode->scheduleUUID                     = String_new();
  newJobNode->scheduleCustomText               = String_new();
  newJobNode->archiveType                      = ARCHIVE_TYPE_NORMAL;
  newJobNode->storageFlags                     = STORAGE_FLAGS_NONE;
  newJobNode->byName                           = String_new();

  newJobNode->requestedAbortFlag               = FALSE;
  newJobNode->abortedByInfo                    = String_new();
  newJobNode->requestedVolumeNumber            = 0;
  newJobNode->volumeNumber                     = 0;
  newJobNode->volumeMessage                    = String_new();
  newJobNode->volumeUnloadFlag                 = FALSE;

  newJobNode->runningInfo.lastExecutedDateTime = 0LL;
  newJobNode->runningInfo.lastErrorMessage     = String_new();

  Misc_performanceFilterInit(&newJobNode->runningInfo.entriesPerSecondFilter,     10*60);
  Misc_performanceFilterInit(&newJobNode->runningInfo.bytesPerSecondFilter,       10*60);
  Misc_performanceFilterInit(&newJobNode->runningInfo.storageBytesPerSecondFilter,10*60);


  Job_resetRunningInfo(newJobNode);

  newJobNode->executionCount.normal            = 0L;
  newJobNode->executionCount.full              = 0L;
  newJobNode->executionCount.incremental       = 0L;
  newJobNode->executionCount.differential      = 0L;
  newJobNode->executionCount.continuous        = 0L;
  newJobNode->averageDuration.normal           = 0LL;
  newJobNode->averageDuration.full             = 0LL;
  newJobNode->averageDuration.incremental      = 0LL;
  newJobNode->averageDuration.differential     = 0LL;
  newJobNode->averageDuration.continuous       = 0LL;
  newJobNode->totalEntityCount                 = 0L;
  newJobNode->totalStorageCount                = 0L;
  newJobNode->totalStorageSize                 = 0L;
  newJobNode->totalEntryCount                  = 0L;
  newJobNode->totalEntrySize                   = 0L;

  return newJobNode;
}

void Job_delete(JobNode *jobNode)
{
  assert(jobNode != NULL);

  freeJobNode(jobNode,NULL);
  LIST_DELETE_NODE(jobNode);
}

void Job_listChanged(void)
{
}

bool Job_isSomeRunning(void)
{
  const JobNode *jobNode;
  bool          runningFlag;

  runningFlag = FALSE;
  SEMAPHORE_LOCKED_DO(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    LIST_ITERATE(&jobList,jobNode)
    {
      if (Job_isRunning(jobNode->jobState))
      {
        runningFlag = TRUE;
        break;
      }
    }
  }

  return runningFlag;
}

const char *Job_getStateText(JobStates jobState, StorageFlags storageFlags)
{
  const char *stateText;

  stateText = "UNKNOWN";
  switch (jobState)
  {
    case JOB_STATE_NONE:
      stateText = "NONE";
      break;
    case JOB_STATE_WAITING:
      stateText = "WAITING";
      break;
    case JOB_STATE_RUNNING:
      if      (storageFlags.noStorage)
      {
        stateText = "NO_STORAGE";
      }
      else if (storageFlags.dryRun)
      {
        stateText = "DRY_RUNNING";
      }
      else
      {
        stateText = "RUNNING";
      }
      break;
    case JOB_STATE_REQUEST_FTP_PASSWORD:
      stateText = "REQUEST_FTP_PASSWORD";
      break;
    case JOB_STATE_REQUEST_SSH_PASSWORD:
      stateText = "REQUEST_SSH_PASSWORD";
      break;
    case JOB_STATE_REQUEST_WEBDAV_PASSWORD:
      stateText = "REQUEST_WEBDAV_PASSWORD";
      break;
    case JOB_STATE_REQUEST_CRYPT_PASSWORD:
      stateText = "request_crypt_password";
      break;
    case JOB_STATE_REQUEST_VOLUME:
      stateText = "REQUEST_VOLUME";
      break;
    case JOB_STATE_DONE:
      stateText = "DONE";
      break;
    case JOB_STATE_ERROR:
      stateText = "ERROR";
      break;
    case JOB_STATE_ABORTED:
      stateText = "ABORTED";
      break;
    case JOB_STATE_DISCONNECTED:
      stateText = "DISCONNECTED";
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return stateText;
}

ScheduleNode *Job_findScheduleByUUID(const JobNode *jobNode, ConstString scheduleUUID)
{
  ScheduleNode *scheduleNode;

  assert(jobNode != NULL);
  assert(scheduleUUID != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  if (jobNode != NULL)
  {
    scheduleNode = LIST_FIND(&jobNode->job.options.scheduleList,scheduleNode,String_equals(scheduleNode->uuid,scheduleUUID));
  }
  else
  {
    scheduleNode = NULL;
    LIST_ITERATEX(&jobList,jobNode,scheduleNode == NULL)
    {
      scheduleNode = LIST_FIND(&jobNode->job.options.scheduleList,scheduleNode,String_equals(scheduleNode->uuid,scheduleUUID));
    }
  }

  return scheduleNode;
}

void Job_includeExcludeChanged(JobNode *jobNode)
{
  const ScheduleNode *scheduleNode;

  assert(Semaphore_isLocked(&jobList.lock));

  // check if continuous schedule exists, update continuous notifies
  LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
  {
    if (scheduleNode->archiveType == ARCHIVE_TYPE_CONTINUOUS)
    {
      if (scheduleNode->enabled)
      {
        Continuous_initNotify(jobNode->name,
                              jobNode->job.uuid,
                              scheduleNode->uuid,
                              &jobNode->job.includeEntryList
                             );
      }
      else
      {
        Continuous_doneNotify(jobNode->name,
                              jobNode->job.uuid,
                              scheduleNode->uuid
                             );
      }
    }
  }
}

void Job_mountChanged(JobNode *jobNode)
{
  const MountNode *mountNode;

  assert(Semaphore_isLocked(&jobList.lock));

  LIST_ITERATE(&jobNode->job.options.mountList,mountNode)
  {
  }
}

void Job_scheduleChanged(const JobNode *jobNode)
{
  const ScheduleNode *scheduleNode;

  assert(Semaphore_isLocked(&jobList.lock));

  // check if continuous schedule exists, update continuous notifies
  LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
  {
    if (scheduleNode->archiveType == ARCHIVE_TYPE_CONTINUOUS)
    {
      if (scheduleNode->enabled)
      {
        Continuous_initNotify(jobNode->name,
                              jobNode->job.uuid,
                              scheduleNode->uuid,
                              &jobNode->job.includeEntryList
                             );
      }
      else
      {
        Continuous_doneNotify(jobNode->name,
                              jobNode->job.uuid,
                              scheduleNode->uuid
                             );
      }
    }
  }
}

void Job_persistenceChanged(const JobNode *jobNode)
{
  assert(Semaphore_isLocked(&jobList.lock));

  UNUSED_VARIABLE(jobNode);
}

Errors Job_readScheduleInfo(JobNode *jobNode)
{
  String          fileName,pathName,baseName;
  FileHandle      fileHandle;
  Errors          error;
  String          line;
  uint64          n;
  char            s[64];
  ArchiveTypes    archiveType;
  ScheduleNode    *scheduleNode;
  int             minKeep,maxKeep;
  int             maxAge;
  PersistenceNode *persistenceNode;

  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  // init variables
  LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
  {
    scheduleNode->lastExecutedDateTime = 0LL;
  }
  jobNode->runningInfo.lastExecutedDateTime = 0LL;

  // get filename
  fileName = String_new();
  File_splitFileName(jobNode->fileName,&pathName,&baseName);
  File_setFileName(fileName,pathName);
  File_appendFileName(fileName,String_insertChar(baseName,0,'.'));
  String_delete(baseName);
  String_delete(pathName);

  if (File_exists(fileName))
  {
    // open file .name
    error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
    if (error != ERROR_NONE)
    {
      String_delete(fileName);
      return error;
    }

    line = String_new();

    // read file
    if (File_getLine(&fileHandle,line,NULL,NULL))
    {
      // first line: <last execution time stamp> or <last execution time stamp>+<type>
      if      (String_parse(line,STRING_BEGIN,"%"PRIu64" %64s",NULL,&n,s))
      {
        if (Archive_parseType(s,&archiveType,NULL))
        {
          LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
          {
            if (scheduleNode->archiveType == archiveType)
            {
              scheduleNode->lastExecutedDateTime = n;
            }
          }
        }
        if (n > jobNode->runningInfo.lastExecutedDateTime) jobNode->runningInfo.lastExecutedDateTime = n;
      }
      else if (String_parse(line,STRING_BEGIN,"%"PRIu64,NULL,&n))
      {
        LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
        {
          scheduleNode->lastExecutedDateTime = n;
        }
        if (n > jobNode->runningInfo.lastExecutedDateTime) jobNode->runningInfo.lastExecutedDateTime = n;
      }

      // other lines: <last execution time stamp>+<type>
      while (File_getLine(&fileHandle,line,NULL,NULL))
      {
        if (String_parse(line,STRING_BEGIN,"%"PRIu64" %64s",NULL,&n,s))
        {
          if (Archive_parseType(s,&archiveType,NULL))
          {
            LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
            {
              if (scheduleNode->archiveType == archiveType)
              {
                scheduleNode->lastExecutedDateTime = n;
              }
            }
          }
          if (n > jobNode->runningInfo.lastExecutedDateTime) jobNode->runningInfo.lastExecutedDateTime = n;
        }
      }
    }
    jobNode->lastScheduleCheckDateTime = jobNode->runningInfo.lastExecutedDateTime;

    String_delete(line);

    // close file
    File_close(&fileHandle);

    if (error != ERROR_NONE)
    {
      String_delete(fileName);
      return error;
    }
  }

  // set max. last schedule check date/time
  if (jobNode->lastScheduleCheckDateTime == 0LL)
  {
    jobNode->lastScheduleCheckDateTime = Misc_getCurrentDate()-MAX_SCHEDULE_CATCH_TIME*S_PER_DAY;
  }

  // convert deprecated schedule persistence -> persistence data
  LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
  {
    if (scheduleNode->deprecatedPersistenceFlag)
    {
      // map 0 -> all/forever
      minKeep = (scheduleNode->minKeep != 0) ? scheduleNode->minKeep : KEEP_ALL;
      maxKeep = (scheduleNode->maxKeep != 0) ? scheduleNode->maxKeep : KEEP_ALL;
      maxAge  = (scheduleNode->maxAge  != 0) ? scheduleNode->maxAge  : AGE_FOREVER;

      // find existing persistence node
      persistenceNode = LIST_FIND(&jobNode->job.options.persistenceList,
                                  persistenceNode,
                                     (persistenceNode->archiveType == scheduleNode->archiveType)
                                  && (persistenceNode->minKeep     == minKeep                  )
                                  && (persistenceNode->maxKeep     == maxKeep                  )
                                  && (persistenceNode->maxAge      == maxAge                   )
                                 );
      if (persistenceNode == NULL)
      {
        // create new persistence node
        persistenceNode = newPersistenceNode(scheduleNode->archiveType,
                                             minKeep,
                                             maxKeep,
                                             maxAge
                                            );
        assert(persistenceNode != NULL);

        // insert into persistence list
        insertPersistenceNode(&jobNode->job.options.persistenceList,persistenceNode);
      }
    }
  }

  // free resources
  String_delete(fileName);

  return ERROR_NONE;
}

Errors Job_writeScheduleInfo(JobNode *jobNode, ArchiveTypes archiveType, uint64 executeEndDateTime)
{
  ScheduleNode *scheduleNode;
  String       fileName,pathName,baseName;
  FileHandle   fileHandle;
  Errors       error;
  uint64       lastExecutedDateTime;

  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  // set last executed
  LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
  {
    if (scheduleNode->archiveType == archiveType)
    {
      scheduleNode->lastExecutedDateTime = executeEndDateTime;
    }
  }

  if (!String_isEmpty(jobNode->fileName))
  {
    // get filename
    fileName = String_new();
    File_splitFileName(jobNode->fileName,&pathName,&baseName);
    File_setFileName(fileName,pathName);
    File_appendFileName(fileName,String_insertChar(baseName,0,'.'));
    String_delete(baseName);
    String_delete(pathName);

    // create file .name
    error = File_open(&fileHandle,fileName,FILE_OPEN_CREATE);
    if (error != ERROR_NONE)
    {
      String_delete(fileName);
      return error;
    }

    // write file: last execution time stamp
    error = File_printLine(&fileHandle,"%"PRIu64,executeEndDateTime);
    if (error != ERROR_NONE)
    {
      File_close(&fileHandle);
      String_delete(fileName);
      return error;
    }

    // write file: last execution time stamp+type
    for (archiveType = ARCHIVE_TYPE_MIN; archiveType <= ARCHIVE_TYPE_MAX; archiveType++)
    {
      // get last executed date/time
      lastExecutedDateTime = 0LL;
      LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
      {
        if ((scheduleNode->archiveType == archiveType) && (scheduleNode->lastExecutedDateTime > lastExecutedDateTime))
        {
          lastExecutedDateTime = scheduleNode->lastExecutedDateTime;
        }
      }

      // write <last execution time stamp>+<type>
      if (lastExecutedDateTime > 0LL)
      {
        error = File_printLine(&fileHandle,"%"PRIu64" %s",lastExecutedDateTime,Archive_archiveTypeToString(archiveType));
        if (error != ERROR_NONE)
        {
          File_close(&fileHandle);
          String_delete(fileName);
          return error;
        }
      }
    }

    // close file
    File_close(&fileHandle);

    // free resources
    String_delete(fileName);
  }

  return ERROR_NONE;
}

bool Job_read(JobNode *jobNode)
{
  Errors     error;
  FileHandle fileHandle;
  bool       failFlag;
  uint       lineNb;
  String     line;
  String     s;
  String     name,value;
  long       nextIndex;

  assert(jobNode != NULL);
  assert(jobNode->fileName != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  // reset job values
  String_clear(jobNode->job.uuid);
  String_clear(jobNode->job.slaveHost.name);
  jobNode->job.slaveHost.port          = 0;
  jobNode->job.slaveHost.forceTLS      = FALSE;
  String_clear(jobNode->job.storageName);
  EntryList_clear(&jobNode->job.includeEntryList);
  PatternList_clear(&jobNode->job.excludePatternList);
  clearOptions(&jobNode->job.options);

  // open file
  error = File_open(&fileHandle,jobNode->fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    printError("Cannot open job file '%s' (error: %s)!",
               String_cString(jobNode->fileName),
               Error_getText(error)
              );
    return FALSE;
  }

  // parse file
  failFlag    = FALSE;
  line        = String_new();
  lineNb      = 0;
  s           = String_new();
  name        = String_new();
  value       = String_new();
  while (File_getLine(&fileHandle,line,&lineNb,"#") && !failFlag)
  {
    // parse line
    if      (String_parse(line,STRING_BEGIN,"[schedule %S]",NULL,s))
    {
      ScheduleNode       *scheduleNode;
      const ScheduleNode *existingScheduleNode;

      // new schedule
      scheduleNode = newScheduleNode();
      assert(scheduleNode != NULL);
      while (   File_getLine(&fileHandle,line,&lineNb,"#")
             && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
            )
      {
        if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
        {
          ConfigValue_parse(String_cString(name),
                            String_cString(value),
                            JOB_CONFIG_VALUES,
                            "schedule",
                            LAMBDA(void,(const char *errorMessage, void *userData),
                            {
                              UNUSED_VARIABLE(userData);

                              printError("%s in %s, line %ld: '%s'",errorMessage,String_cString(jobNode->fileName),lineNb,String_cString(line));
                            }),NULL,
                            LAMBDA(void,(const char *warningMessage, void *userData),
                            {
                              UNUSED_VARIABLE(userData);

                              printWarning("%s in %s, line %ld: '%s'",warningMessage,String_cString(jobNode->fileName),lineNb,String_cString(line));
                            }),NULL,
                            scheduleNode,
NULL // commentLineList
                           );
        }
        else
        {
          printError("Syntax error in %s, line %ld: '%s' - skipped",
                     String_cString(jobNode->fileName),
                     lineNb,
                     String_cString(line)
                    );
          failFlag = TRUE;
        }
      }
      File_ungetLine(&fileHandle,line,&lineNb);

      // init schedule uuid
      if (String_isEmpty(scheduleNode->uuid))
      {
        Misc_getUUID(scheduleNode->uuid);
        jobNode->modifiedFlag = TRUE;
      }

      // get schedule info (if possible)
      scheduleNode->lastExecutedDateTime = 0LL;
      scheduleNode->totalEntityCount     = 0L;
      scheduleNode->totalStorageCount    = 0L;
      scheduleNode->totalStorageSize     = 0LL;
      scheduleNode->totalEntryCount      = 0L;
      scheduleNode->totalEntrySize       = 0LL;

      // append to list (if not a duplicate)
      if (!LIST_CONTAINS(&jobNode->job.options.scheduleList,
                         existingScheduleNode,
                            (existingScheduleNode->date.year   == scheduleNode->date.year            )
                         && (existingScheduleNode->date.month  == scheduleNode->date.month           )
                         && (existingScheduleNode->date.day    == scheduleNode->date.day             )
                         && (existingScheduleNode->weekDaySet  == scheduleNode->weekDaySet           )
                         && (existingScheduleNode->time.hour   == scheduleNode->time.hour            )
                         && (existingScheduleNode->time.minute == scheduleNode->time.minute          )
                         && (existingScheduleNode->archiveType == scheduleNode->archiveType          )
                         && (existingScheduleNode->interval    == scheduleNode->interval             )
                         && (String_equals(existingScheduleNode->customText,scheduleNode->customText))
                         && (existingScheduleNode->minKeep     == scheduleNode->minKeep              )
                         && (existingScheduleNode->maxKeep     == scheduleNode->maxKeep              )
                         && (existingScheduleNode->maxAge      == scheduleNode->maxAge               )
                         && (existingScheduleNode->noStorage   == scheduleNode->noStorage            )
                        )
        )
      {
        // append to schedule list
        List_append(&jobNode->job.options.scheduleList,scheduleNode);
      }
      else
      {
        // duplicate -> discard
        deleteScheduleNode(scheduleNode);
      }
    }
    else if (String_parse(line,STRING_BEGIN,"[persistence %S]",NULL,s))
    {
      ArchiveTypes          archiveType;
      PersistenceNode       *persistenceNode;
      const PersistenceNode *existingPersistenceNode;

      if (Archive_parseType(String_cString(s),&archiveType,NULL))
      {
        // new persistence
        persistenceNode = newPersistenceNode(archiveType,0,0,0);
        assert(persistenceNode != NULL);
        while (   File_getLine(&fileHandle,line,&lineNb,"#")
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
          {
            ConfigValue_parse(String_cString(name),
                              String_cString(value),
                              JOB_CONFIG_VALUES,
                              "persistence",
                              LAMBDA(void,(const char *errorMessage, void *userData),
                              {
                                UNUSED_VARIABLE(userData);

                                printError("%s in %s, line %ld: '%s'",errorMessage,String_cString(jobNode->fileName),lineNb,String_cString(line));
                              }),NULL,
                              LAMBDA(void,(const char *warningMessage, void *userData),
                              {
                                UNUSED_VARIABLE(userData);

                                printWarning("%s in %s, line %ld: '%s'",warningMessage,String_cString(jobNode->fileName),lineNb,String_cString(line));
                              }),NULL,
                              persistenceNode,
NULL // commentLineList
                             );
          }
          else
          {
            printError("Syntax error in %s, line %ld: '%s' - skipped",
                       String_cString(jobNode->fileName),
                       lineNb,
                       String_cString(line)
                      );
            failFlag = TRUE;
          }
        }
        File_ungetLine(&fileHandle,line,&lineNb);

        // insert into persistence list (if not a duplicate)
        if (!LIST_CONTAINS(&jobNode->job.options.persistenceList,
                           existingPersistenceNode,
                              (existingPersistenceNode->archiveType == persistenceNode->archiveType)
                           && (existingPersistenceNode->minKeep     == persistenceNode->minKeep    )
                           && (existingPersistenceNode->maxKeep     == persistenceNode->maxKeep    )
                           && (existingPersistenceNode->maxAge      == persistenceNode->maxAge     )
                          )
           )
        {
          // insert into persistence list
          insertPersistenceNode(&jobNode->job.options.persistenceList,persistenceNode);
        }
        else
        {
          // duplicate -> discard
          deletePersistenceNode(persistenceNode);
        }
      }
      else
      {
        printError("Unknown archive type '%s' in section '%s' in %s, line %ld - skipped",
                   String_cString(s),
                   "persistence",
                   String_cString(jobNode->fileName),
                   lineNb
                  );

        // skip rest of section
        while (   File_getLine(&fileHandle,line,&lineNb,"#")
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          // ignored
        }
        File_ungetLine(&fileHandle,line,&lineNb);

        failFlag = TRUE;
      }
    }
    else if (String_parse(line,STRING_BEGIN,"[global]",NULL))
    {
      // nothing to do
    }
    else if (String_parse(line,STRING_BEGIN,"[end]",NULL))
    {
      // nothing to do
    }
    else if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
    {
      ConfigValue_parse(String_cString(name),
                        String_cString(value),
                        JOB_CONFIG_VALUES,
                        NULL, // sectionName
                        LAMBDA(void,(const char *errorMessage, void *userData),
                        {
                          UNUSED_VARIABLE(userData);

                          printError("%s in %s, line %ld: '%s'",errorMessage,String_cString(jobNode->fileName),lineNb,String_cString(line));
                        }),NULL,
                        LAMBDA(void,(const char *warningMessage, void *userData),
                        {
                          UNUSED_VARIABLE(userData);

                          printWarning("%s in %s, line %ld: '%s'",warningMessage,String_cString(jobNode->fileName),lineNb,String_cString(line));
                        }),NULL,
                        jobNode,
NULL // commentLineList
                       );
    }
    else
    {
      printError("Syntax error in %s, line %ld: '%s' - skipped",
                 String_cString(jobNode->fileName),
                 lineNb,
                 String_cString(line)
                );
      failFlag = TRUE;
    }
  }
  String_delete(value);
  String_delete(name);
  String_delete(s);
  String_delete(line);

  // close file
  (void)File_close(&fileHandle);
  jobNode->fileModified = File_getFileTimeModified(jobNode->fileName);
  if (failFlag)
  {
    return FALSE;
  }

  // set UUID if not exists
  if (String_isEmpty(jobNode->job.uuid))
  {
    Misc_getUUID(jobNode->job.uuid);
    jobNode->modifiedFlag = TRUE;
  }

  // read schedule info (ignore errors)
  (void)Job_readScheduleInfo(jobNode);

  // reset job modified
  jobNode->modifiedFlag = FALSE;

  return failFlag;
}

Errors Job_rereadAll(ConstString jobsDirectory)
{
  Errors              error;
  DirectoryListHandle directoryListHandle;
  String              fileName;
  String              baseName;
  JobNode             *jobNode;
  const JobNode       *jobNode1,*jobNode2;

  assert(jobsDirectory != NULL);

  // init variables
  fileName = String_new();

  // add new/update jobs
  File_setFileName(fileName,jobsDirectory);
  error = File_openDirectoryList(&directoryListHandle,fileName);
  if (error != ERROR_NONE)
  {
    String_delete(fileName);
    return error;
  }
  baseName = String_new();
  while (!File_endOfDirectoryList(&directoryListHandle))
  {
    // read directory entry
    if (File_readDirectoryList(&directoryListHandle,fileName) != ERROR_NONE)
    {
      continue;
    }

    // get base name
    File_getBaseName(baseName,fileName);

    // check if readable file and not ".*"
    if (File_isFile(fileName) && File_isReadable(fileName) && !String_startsWithChar(baseName,'.'))
    {
      SEMAPHORE_LOCKED_DO(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
      {
        // find/create job
        jobNode = Job_find(baseName);
        if (jobNode == NULL)
        {
          // create new job
          jobNode = Job_new(JOB_TYPE_CREATE,
                            baseName,
                            NULL, // jobUUID
                            fileName
                           );
          assert(jobNode != NULL);
          List_append(&jobList,jobNode);

          // notify about changes
          Job_listChanged();
        }

        if (   !Job_isActive(jobNode->jobState)
            && (File_getFileTimeModified(fileName) > jobNode->fileModified)
           )
        {
          // read job
          (void)Job_read(jobNode);

          // notify about changes
          Job_includeExcludeChanged(jobNode);
          Job_mountChanged(jobNode);
          Job_scheduleChanged(jobNode);
          Job_persistenceChanged(jobNode);
        }
      }
    }
  }
  String_delete(baseName);
  File_closeDirectoryList(&directoryListHandle);

  // remove not existing jobs
  SEMAPHORE_LOCKED_DO(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    jobNode = jobList.head;
    while (jobNode != NULL)
    {
      if (jobNode->jobState == JOB_STATE_NONE)
      {
        File_setFileName(fileName,jobsDirectory);
        File_appendFileName(fileName,jobNode->name);
        if (File_isFile(fileName) && File_isReadable(fileName))
        {
          // exists => ok
          jobNode = jobNode->next;
        }
        else
        {
          // do not exists anymore => remove and delete job node
          jobNode = List_removeAndFree(&jobList,jobNode,CALLBACK_((ListNodeFreeFunction)freeJobNode,NULL));

          // notify about changes
          Job_listChanged();
        }
      }
      else
      {
        jobNode = jobNode->next;
      }
    }
  }

  // check for duplicate UUIDs
  SEMAPHORE_LOCKED_DO(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    jobNode1 = jobList.head;
    while (jobNode1 != NULL)
    {
      jobNode2 = jobNode1->next;
      while (jobNode2 != NULL)
      {
        if (String_equals(jobNode1->job.uuid,jobNode2->job.uuid))
        {
          printWarning("Duplicate UUID in jobs '%s' and '%s'!",String_cString(jobNode1->name),String_cString(jobNode2->name));
        }
        jobNode2 = jobNode2->next;
      }
      jobNode1 = jobNode1->next;
    }
  }

  // free resources
  String_delete(fileName);

  return ERROR_NONE;
}

Errors Job_write(JobNode *jobNode)
{
  StringList            jobLinesList;
  String                line;
  Errors                error;
  int                   i;
  StringNode            *nextStringNode;
  const ScheduleNode    *scheduleNode;
  const PersistenceNode *persistenceNode;
  ConfigValueFormat     configValueFormat;

  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  if (String_isSet(jobNode->fileName))
  {
    // init variables
    StringList_init(&jobLinesList);
    line = String_new();

    // read config file lines
    error = ConfigValue_readConfigFileLines(jobNode->fileName,&jobLinesList);
    if (error != ERROR_NONE)
    {
      StringList_done(&jobLinesList);
      String_delete(line);
      return error;
    }

    // correct config values
    switch (jobNode->job.options.cryptPasswordMode)
    {
      case PASSWORD_MODE_DEFAULT:
      case PASSWORD_MODE_ASK:
        Password_clear(&jobNode->job.options.cryptPassword);
        break;
      case PASSWORD_MODE_NONE:
      case PASSWORD_MODE_CONFIG:
        // nothing to do
        break;
    }

    // update line list
    CONFIG_VALUE_ITERATE(JOB_CONFIG_VALUES,NULL,i)
    {
      // delete old entries, get position for insert new entries
      nextStringNode = ConfigValue_deleteEntries(&jobLinesList,NULL,JOB_CONFIG_VALUES[i].name);

      // insert new entries
      ConfigValue_formatInit(&configValueFormat,
                             &JOB_CONFIG_VALUES[i],
                             CONFIG_VALUE_FORMAT_MODE_LINE,
                             jobNode
                            );
      while (ConfigValue_format(&configValueFormat,line))
      {
        StringList_insert(&jobLinesList,line,nextStringNode);
      }
      ConfigValue_formatDone(&configValueFormat);
    }

    // delete old schedule sections, get position for insert new schedule sections, write new schedule sections
    nextStringNode = ConfigValue_deleteSections(&jobLinesList,"schedule");
    if (!List_isEmpty(&jobNode->job.options.scheduleList))
    {
      StringList_insertCString(&jobLinesList,"",nextStringNode);
      LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
      {
        // insert new schedule sections
        String_format(line,"[schedule]");
        StringList_insert(&jobLinesList,line,nextStringNode);

        CONFIG_VALUE_ITERATE(JOB_CONFIG_VALUES,"schedule",i)
        {
          ConfigValue_formatInit(&configValueFormat,
                                 &JOB_CONFIG_VALUES[i],
                                 CONFIG_VALUE_FORMAT_MODE_LINE,
                                 scheduleNode
                                );
          while (ConfigValue_format(&configValueFormat,line))
          {
            StringList_insert(&jobLinesList,line,nextStringNode);
          }
          ConfigValue_formatDone(&configValueFormat);
        }

        StringList_insertCString(&jobLinesList,"[end]",nextStringNode);
        StringList_insertCString(&jobLinesList,"",nextStringNode);
      }
    }

    // delete old persistence sections, get position for insert new persistence sections, write new persistence sections
    nextStringNode = ConfigValue_deleteSections(&jobLinesList,"persistence");
    if (!List_isEmpty(&jobNode->job.options.persistenceList))
    {
      StringList_insertCString(&jobLinesList,"",nextStringNode);
      LIST_ITERATE(&jobNode->job.options.persistenceList,persistenceNode)
      {
        // insert new persistence sections
        String_format(line,"[persistence %s]",Archive_archiveTypeToString(persistenceNode->archiveType));
        StringList_insert(&jobLinesList,line,nextStringNode);

        CONFIG_VALUE_ITERATE(JOB_CONFIG_VALUES,"persistence",i)
        {
          ConfigValue_formatInit(&configValueFormat,
                                 &JOB_CONFIG_VALUES[i],
                                 CONFIG_VALUE_FORMAT_MODE_LINE,
                                 persistenceNode
                                );
          while (ConfigValue_format(&configValueFormat,line))
          {
            StringList_insert(&jobLinesList,line,nextStringNode);
          }
          ConfigValue_formatDone(&configValueFormat);
        }

        StringList_insertCString(&jobLinesList,"[end]",nextStringNode);
        StringList_insertCString(&jobLinesList,"",nextStringNode);
      }
    }

    // write config file lines
    error = ConfigValue_writeConfigFileLines(jobNode->fileName,&jobLinesList);
    if (error != ERROR_NONE)
    {
      String_delete(line);
      StringList_done(&jobLinesList);
      return error;
    }
    error = File_setPermission(jobNode->fileName,FILE_PERMISSION_USER_READ|FILE_PERMISSION_USER_WRITE);
    if (error != ERROR_NONE)
    {
      logMessage(NULL,  // logHandle
                 LOG_TYPE_WARNING,
                 "cannot set file permissions of job '%s' (error: %s)\n",
                 String_cString(jobNode->fileName),
                 Error_getText(error)
                );
    }

    // save time modified
    jobNode->fileModified = File_getFileTimeModified(jobNode->fileName);

    // free resources
    String_delete(line);
    StringList_done(&jobLinesList);
  }

  // reset modified flag
  jobNode->modifiedFlag = FALSE;

  return ERROR_NONE;
}

void Job_writeModifiedAll(void)
{
  JobNode *jobNode;
  Errors  error;

  SEMAPHORE_LOCKED_DO(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    LIST_ITERATE(&jobList,jobNode)
    {
      if (jobNode->modifiedFlag)
      {
        error = Job_write(jobNode);
        if (error != ERROR_NONE)
        {
          printWarning("Cannot update job '%s' (error: %s)",String_cString(jobNode->fileName),Error_getText(error));
        }
      }
    }
  }
}

void Job_trigger(JobNode      *jobNode,
                 ConstString  scheduleUUID,
                 ConstString  scheduleCustomText,
                 ArchiveTypes archiveType,
                 StorageFlags storageFlags,
                 uint64       startDateTime,
                 const char   *byName
                )
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  // set job state
  jobNode->jobState              = JOB_STATE_WAITING;
  String_set(jobNode->scheduleUUID,scheduleUUID);
  String_set(jobNode->scheduleCustomText,scheduleCustomText);
  jobNode->archiveType           = archiveType;
  jobNode->storageFlags          = storageFlags;
  jobNode->startDateTime         = startDateTime;
  String_setCString(jobNode->byName,byName);

  jobNode->requestedAbortFlag    = FALSE;
  String_clear(jobNode->abortedByInfo);
  jobNode->requestedVolumeNumber = 0;
  jobNode->volumeNumber          = 0;
  String_clear(jobNode->volumeMessage);
  jobNode->volumeUnloadFlag      = FALSE;
  Semaphore_signalModified(&jobList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);

  // reset running info
  Job_resetRunningInfo(jobNode);
}

void Job_start(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  // set job state, reset last error
  jobNode->jobState          = JOB_STATE_RUNNING;
  jobNode->runningInfo.error = ERROR_NONE;
  Semaphore_signalModified(&jobList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);

  // increment active counter
  jobList.activeCount++;
}

void Job_end(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  // clear schedule
  String_clear(jobNode->scheduleUUID);
  String_clear(jobNode->scheduleCustomText);

  // set state
  if      (jobNode->requestedAbortFlag)
  {
    jobNode->jobState = JOB_STATE_ABORTED;
  }
  else if (jobNode->runningInfo.error != ERROR_NONE)
  {
    jobNode->jobState = JOB_STATE_ERROR;
  }
  else
  {
    jobNode->jobState = JOB_STATE_DONE;
  }

  // signal modified
  Semaphore_signalModified(&jobList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);

  // decrement active counter
  assert(jobList.activeCount > 0);
  jobList.activeCount--;
}

void Job_abort(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  if      (Job_isRunning(jobNode->jobState))
  {
    // request abort job
    jobNode->requestedAbortFlag = TRUE;
    Semaphore_signalModified(&jobList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);

    if (Job_isLocal(jobNode))
    {
      // wait until local job terminated
      while (Job_isRunning(jobNode->jobState))
      {
        Semaphore_waitModified(&jobList.lock,LOCK_TIMEOUT);
      }
    }
    else
    {
      // abort slave job
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        jobNode->runningInfo.error = Connector_jobAbort(connectorInfo,
                                                        jobNode->job.uuid
                                                       );
      }
    }
  }
  else if (Job_isActive(jobNode->jobState))
  {
    jobNode->jobState = JOB_STATE_NONE;
  }
}

void Job_reset(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  if (!Job_isActive(jobNode->jobState))
  {
    jobNode->jobState = JOB_STATE_NONE;
    Job_resetRunningInfo(jobNode);
  }
}

void Job_resetRunningInfo(JobNode *jobNode)
{
  assert(jobNode != NULL);

  resetStatusInfo(&jobNode->statusInfo);

  jobNode->runningInfo.error                 = ERROR_NONE;
  jobNode->runningInfo.entriesPerSecond      = 0.0;
  jobNode->runningInfo.bytesPerSecond        = 0.0;
  jobNode->runningInfo.storageBytesPerSecond = 0.0;
  jobNode->runningInfo.estimatedRestTime     = 0L;

  Misc_performanceFilterClear(&jobNode->runningInfo.entriesPerSecondFilter     );
  Misc_performanceFilterClear(&jobNode->runningInfo.bytesPerSecondFilter       );
  Misc_performanceFilterClear(&jobNode->runningInfo.storageBytesPerSecondFilter);
}

void Job_initOptions(JobOptions *jobOptions)
{
  uint i;

  assert(jobOptions != NULL);

  memClear(jobOptions,sizeof(JobOptions));

  jobOptions->uuid                                      = String_new();

  jobOptions->includeFileListFileName                   = String_duplicate(globalOptions.includeFileListFileName);
  jobOptions->includeFileCommand                        = String_duplicate(globalOptions.includeFileCommand);
  jobOptions->includeImageListFileName                  = String_duplicate(globalOptions.includeImageListFileName);
  jobOptions->includeImageCommand                       = String_duplicate(globalOptions.includeImageCommand);
  jobOptions->excludeListFileName                       = String_duplicate(globalOptions.excludeListFileName);
  jobOptions->excludeCommand                            = String_duplicate(globalOptions.excludeCommand);

  List_initDuplicate(&jobOptions->mountList,
                     &globalOptions.mountList,
                     NULL,  // fromListFromNode
                     NULL,  // fromListToNode
                     CALLBACK_((ListNodeDuplicateFunction)duplicateMountNode,NULL)
                    );
  PatternList_initDuplicate(&jobOptions->compressExcludePatternList,
                            &globalOptions.compressExcludePatternList,
                            NULL,  // fromPatternListFromNode
                            NULL  // fromPatternListToNode
                           );
  DeltaSourceList_initDuplicate(&jobOptions->deltaSourceList,
                                &globalOptions.deltaSourceList,
                                NULL,  // fromDeltaSourceListFromNode
                                NULL  // fromDeltaSourceListToNode
                               );
  List_init(&jobOptions->scheduleList);
  List_init(&jobOptions->persistenceList);
  jobOptions->persistenceList.lastModificationDateTime  = 0LL;

  jobOptions->archiveType                               = globalOptions.archiveType;

  jobOptions->archivePartSize                           = globalOptions.archivePartSize;
  jobOptions->incrementalListFileName                   = String_duplicate(globalOptions.incrementalListFileName);
  jobOptions->directoryStripCount                       = globalOptions.directoryStripCount;
  jobOptions->destination                               = String_duplicate(globalOptions.destination);
  jobOptions->owner                                     = globalOptions.owner;
  jobOptions->permissions                               = globalOptions.permissions;
  jobOptions->patternType                               = globalOptions.patternType;

  jobOptions->compressAlgorithms.delta                  = globalOptions.compressAlgorithms.delta;
  jobOptions->compressAlgorithms.byte                   = globalOptions.compressAlgorithms.byte;

  for (i = 0; i < 4; i++)
  {
    jobOptions->cryptAlgorithms[i] = globalOptions.cryptAlgorithms[i];
  }
  jobOptions->cryptType                                 = globalOptions.cryptType;
  jobOptions->cryptPasswordMode                         = globalOptions.cryptPasswordMode;
  Password_initDuplicate(&jobOptions->cryptPassword,&globalOptions.cryptPassword);
  duplicateKey(&jobOptions->cryptPublicKey,&globalOptions.cryptPublicKey);
  duplicateKey(&jobOptions->cryptPrivateKey,&globalOptions.cryptPrivateKey);

  jobOptions->preProcessScript                          = String_new();
  jobOptions->postProcessScript                         = String_new();
  jobOptions->slavePreProcessScript                     = String_new();
  jobOptions->slavePostProcessScript                    = String_new();

  jobOptions->storageOnMaster                           = TRUE;
  initOptionsFileServer(&jobOptions->fileServer);
  initOptionsFTPServer(&jobOptions->ftpServer);
  initOptionsSSHServer(&jobOptions->sshServer);
  initOptionsWebDAVServer(&jobOptions->webDAVServer);
  initOptionsOpticalDisk(&jobOptions->opticalDisk);
  initOptionsDevice(&jobOptions->device);

  jobOptions->fragmentSize                              = globalOptions.fragmentSize;
  jobOptions->maxStorageSize                            = globalOptions.maxStorageSize;
  jobOptions->volumeSize                                = globalOptions.volumeSize;

  jobOptions->comment                                   = String_duplicate(globalOptions.comment);

  jobOptions->skipUnreadableFlag                        = globalOptions.skipUnreadableFlag;
  jobOptions->forceDeltaCompressionFlag                 = globalOptions.forceDeltaCompressionFlag;
  jobOptions->ignoreNoDumpAttributeFlag                 = globalOptions.ignoreNoDumpAttributeFlag;
  jobOptions->archiveFileMode                           = globalOptions.archiveFileMode;
  jobOptions->restoreEntryMode                          = globalOptions.restoreEntryMode;
  jobOptions->errorCorrectionCodesFlag                  = globalOptions.errorCorrectionCodesFlag;
  jobOptions->alwaysCreateImageFlag                     = globalOptions.alwaysCreateImageFlag;
  jobOptions->blankFlag                                 = globalOptions.blankFlag;
  jobOptions->waitFirstVolumeFlag                       = globalOptions.waitFirstVolumeFlag;
  jobOptions->rawImagesFlag                             = globalOptions.rawImagesFlag;
  jobOptions->noFragmentsCheckFlag                      = globalOptions.noFragmentsCheckFlag;
//TODO: job option or better global option only?
  jobOptions->noIndexDatabaseFlag                       = globalOptions.noIndexDatabaseFlag;
  jobOptions->forceVerifySignaturesFlag                 = globalOptions.forceVerifySignaturesFlag;
  jobOptions->skipVerifySignaturesFlag                  = globalOptions.skipVerifySignaturesFlag;
  jobOptions->noSignatureFlag                           = globalOptions.noSignatureFlag;
//TODO: job option or better global option only?
  jobOptions->noBAROnMediumFlag                         = globalOptions.noBAROnMediumFlag;
  jobOptions->noStopOnErrorFlag                         = globalOptions.noStopOnErrorFlag;
  jobOptions->noStopOnAttributeErrorFlag                = globalOptions.noStopOnAttributeErrorFlag;

  DEBUG_ADD_RESOURCE_TRACE(jobOptions,JobOptions);
}

void Job_duplicateOptions(JobOptions *jobOptions, const JobOptions *fromJobOptions)
{
  uint i;

  assert(jobOptions != NULL);
  assert(fromJobOptions != NULL);

  DEBUG_CHECK_RESOURCE_TRACE(fromJobOptions);

  memClear(jobOptions,sizeof(JobOptions));

  jobOptions->uuid                                      = String_new();

  jobOptions->includeFileListFileName                   = String_duplicate(fromJobOptions->includeFileListFileName);
  jobOptions->includeFileCommand                        = String_duplicate(fromJobOptions->includeFileCommand);
  jobOptions->includeImageListFileName                  = String_duplicate(fromJobOptions->includeImageListFileName);
  jobOptions->includeImageCommand                       = String_duplicate(fromJobOptions->includeImageCommand);
  jobOptions->excludeListFileName                       = String_duplicate(fromJobOptions->excludeListFileName);
  jobOptions->excludeCommand                            = String_duplicate(fromJobOptions->excludeCommand);

  List_initDuplicate(&jobOptions->mountList,
                     &fromJobOptions->mountList,
                     NULL,  // fromListFromNode
                     NULL,  // fromListToNode
                     CALLBACK_((ListNodeDuplicateFunction)duplicateMountNode,NULL)
                    );
  PatternList_initDuplicate(&jobOptions->compressExcludePatternList,
                            &fromJobOptions->compressExcludePatternList,
                            NULL,  // fromPatternListFromNode
                            NULL  // fromPatternListToNode
                           );
  DeltaSourceList_initDuplicate(&jobOptions->deltaSourceList,
                                &fromJobOptions->deltaSourceList,
                                NULL,  // fromDeltaSourceListFromNode
                                NULL  // fromDeltaSourceListToNode
                               );
  List_initDuplicate(&jobOptions->scheduleList,
                     &fromJobOptions->scheduleList,
                     NULL,  // fromListFromNode
                     NULL,  // fromListToNode
                     CALLBACK_((ListNodeDuplicateFunction)duplicateScheduleNode,NULL)
                    );
  List_initDuplicate(&jobOptions->persistenceList,
                     &fromJobOptions->persistenceList,
                     NULL,  // fromListFromNode
                     NULL,  // fromListToNode
                     CALLBACK_((ListNodeDuplicateFunction)duplicatePersistenceNode,NULL)
                    );
  jobOptions->persistenceList.lastModificationDateTime  = 0LL;

  jobOptions->archiveType                               = fromJobOptions->archiveType;

  jobOptions->archivePartSize                           = fromJobOptions->archivePartSize;
  jobOptions->incrementalListFileName                   = String_duplicate(fromJobOptions->incrementalListFileName);
  jobOptions->directoryStripCount                       = fromJobOptions->directoryStripCount;
  jobOptions->destination                               = String_duplicate(fromJobOptions->destination);
  jobOptions->owner                                     = fromJobOptions->owner;
  jobOptions->permissions                               = fromJobOptions->permissions;
  jobOptions->patternType                               = fromJobOptions->patternType;

  jobOptions->compressAlgorithms.delta                  = fromJobOptions->compressAlgorithms.delta;
  jobOptions->compressAlgorithms.byte                   = fromJobOptions->compressAlgorithms.byte;

  for (i = 0; i < 4; i++)
  {
    jobOptions->cryptAlgorithms[i] = fromJobOptions->cryptAlgorithms[i];
  }
  #ifdef HAVE_GCRYPT
    jobOptions->cryptType                               = fromJobOptions->cryptType;
  #else /* not HAVE_GCRYPT */
    jobOptions->cryptType                               = fromJobOptions->cryptType;
  #endif /* HAVE_GCRYPT */
  jobOptions->cryptPasswordMode                         = fromJobOptions->cryptPasswordMode;
  Password_initDuplicate(&jobOptions->cryptPassword,&fromJobOptions->cryptPassword);
  duplicateKey(&jobOptions->cryptPublicKey,&fromJobOptions->cryptPublicKey);
  duplicateKey(&jobOptions->cryptPrivateKey,&fromJobOptions->cryptPrivateKey);

  jobOptions->preProcessScript                          = String_duplicate(fromJobOptions->preProcessScript);
  jobOptions->postProcessScript                         = String_duplicate(fromJobOptions->postProcessScript);
  jobOptions->slavePreProcessScript                     = String_duplicate(fromJobOptions->slavePreProcessScript);
  jobOptions->slavePostProcessScript                    = String_duplicate(fromJobOptions->slavePostProcessScript);

  jobOptions->storageOnMaster                           = fromJobOptions->storageOnMaster;
  duplicateOptionsFileServer(&jobOptions->fileServer,&fromJobOptions->fileServer);
  duplicateOptionsFTPServer(&jobOptions->ftpServer,&fromJobOptions->ftpServer);
  duplicateOptionsSSHServer(&jobOptions->sshServer,&fromJobOptions->sshServer);
  duplicateOptionsWebDAVServer(&jobOptions->webDAVServer,&fromJobOptions->webDAVServer);
  duplicateOptionsOpticalDisk(&jobOptions->opticalDisk,&fromJobOptions->opticalDisk);
  duplicateOptionsDevice(&jobOptions->device,&fromJobOptions->device);

  jobOptions->fragmentSize                              = fromJobOptions->fragmentSize;
  jobOptions->maxStorageSize                            = fromJobOptions->maxStorageSize;
  jobOptions->volumeSize                                = fromJobOptions->volumeSize;

  jobOptions->comment                                   = String_duplicate(fromJobOptions->comment);

  jobOptions->skipUnreadableFlag                        = fromJobOptions->skipUnreadableFlag;
  jobOptions->forceDeltaCompressionFlag                 = fromJobOptions->forceDeltaCompressionFlag;
  jobOptions->ignoreNoDumpAttributeFlag                 = fromJobOptions->ignoreNoDumpAttributeFlag;
  jobOptions->archiveFileMode                           = fromJobOptions->archiveFileMode;
  jobOptions->restoreEntryMode                          = fromJobOptions->restoreEntryMode;
  jobOptions->errorCorrectionCodesFlag                  = fromJobOptions->errorCorrectionCodesFlag;
  jobOptions->alwaysCreateImageFlag                     = fromJobOptions->alwaysCreateImageFlag;
  jobOptions->blankFlag                                 = fromJobOptions->blankFlag;
  jobOptions->waitFirstVolumeFlag                       = fromJobOptions->waitFirstVolumeFlag;
  jobOptions->rawImagesFlag                             = fromJobOptions->rawImagesFlag;
  jobOptions->noFragmentsCheckFlag                      = fromJobOptions->noFragmentsCheckFlag;
//TODO: job option or better global option only?
  jobOptions->noIndexDatabaseFlag                       = fromJobOptions->noIndexDatabaseFlag;
  jobOptions->forceVerifySignaturesFlag                 = fromJobOptions->forceVerifySignaturesFlag;
  jobOptions->skipVerifySignaturesFlag                  = fromJobOptions->skipVerifySignaturesFlag;
  jobOptions->noSignatureFlag                           = fromJobOptions->noSignatureFlag;
//TODO: job option or better global option only?
  jobOptions->noBAROnMediumFlag                         = fromJobOptions->noBAROnMediumFlag;
  jobOptions->noStopOnErrorFlag                         = fromJobOptions->noStopOnErrorFlag;
  jobOptions->noStopOnAttributeErrorFlag                = fromJobOptions->noStopOnAttributeErrorFlag;

  DEBUG_ADD_RESOURCE_TRACE(jobOptions,JobOptions);
}

void Job_doneOptions(JobOptions *jobOptions)
{
  assert(jobOptions != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(jobOptions,JobOptions);

  String_delete(jobOptions->comment);

  doneOptionsDevice(&jobOptions->device);
  doneOptionsOpticalDisk(&jobOptions->opticalDisk);

  doneOptionsWebDAVServer(&jobOptions->webDAVServer);
  doneOptionsSSHServer(&jobOptions->sshServer);
  doneOptionsFTPServer(&jobOptions->ftpServer);
  doneOptionsFileServer(&jobOptions->fileServer);

  String_delete(jobOptions->slavePostProcessScript);
  String_delete(jobOptions->slavePreProcessScript);
  String_delete(jobOptions->postProcessScript);
  String_delete(jobOptions->preProcessScript);

  doneKey(&jobOptions->cryptPrivateKey);
  doneKey(&jobOptions->cryptPublicKey);
  Password_done(&jobOptions->cryptPassword);

  String_delete(jobOptions->destination);
  String_delete(jobOptions->incrementalListFileName);

  List_done(&jobOptions->persistenceList,CALLBACK_((ListNodeFreeFunction)freePersistenceNode,NULL));
  List_done(&jobOptions->scheduleList,CALLBACK_((ListNodeFreeFunction)freeScheduleNode,NULL));
  DeltaSourceList_done(&jobOptions->deltaSourceList);
  PatternList_done(&jobOptions->compressExcludePatternList);
  List_done(&jobOptions->mountList,CALLBACK_((ListNodeFreeFunction)freeMountNode,NULL));

  String_delete(jobOptions->excludeCommand);
  String_delete(jobOptions->excludeListFileName);
  String_delete(jobOptions->includeImageCommand);
  String_delete(jobOptions->includeImageListFileName);
  String_delete(jobOptions->includeFileCommand);
  String_delete(jobOptions->includeFileListFileName);

  String_delete(jobOptions->uuid);
}

void Job_duplicatePersistenceList(PersistenceList *persistenceList, const PersistenceList *fromPersistenceList)
{
  assert(persistenceList != NULL);
  assert(fromPersistenceList != NULL);

  List_initDuplicate(persistenceList,
                     fromPersistenceList,
                     NULL,  // fromListFromNode
                     NULL,  // fromListToNode
                     CALLBACK_((ListNodeDuplicateFunction)duplicatePersistenceNode,NULL)
                    );
}

void Job_donePersistenceList(PersistenceList *persistenceList)
{
  List_done(persistenceList,CALLBACK_((ListNodeFreeFunction)freePersistenceNode,NULL));
}

SlaveNode *Job_addSlave(ConstString name, uint port)
{
  SlaveNode *slaveNode;

  assert(name != NULL);
  assert(Semaphore_isLocked(&slaveList.lock));

  slaveNode = LIST_NEW_NODE(SlaveNode);
  if (slaveNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  slaveNode->name               = String_duplicate(name);
  slaveNode->port               = port;
  Connector_init(&slaveNode->connectorInfo);
  slaveNode->lastOnlineDateTime = 0LL;
  slaveNode->authorizedFlag     = FALSE;
  slaveNode->lockCount          = 0;

  List_append(&slaveList,slaveNode);

  return slaveNode;
}

SlaveNode *Job_removeSlave(SlaveNode *slaveNode)
{
  assert(slaveNode != NULL);
  assert(Semaphore_isLocked(&slaveList.lock));
  assert(slaveNode->lockCount == 0);

  if (Connector_isConnected(&slaveNode->connectorInfo))
  {
    Connector_disconnect(&slaveNode->connectorInfo);
  }

  return List_removeAndFree(&slaveList,slaveNode,CALLBACK_((ListNodeFreeFunction)freeSlaveNode,NULL));
}

ConnectorInfo *Job_connectorLock(const JobNode *jobNode, long timeout)
{
  ConnectorInfo *connectorInfo;
  SlaveNode     *slaveNode;

  assert(jobNode != NULL);

  connectorInfo = NULL;
  JOB_SLAVE_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,timeout)
  {
    slaveNode = LIST_FIND(&slaveList,
                              slaveNode,
                                 (slaveNode->port == jobNode->job.slaveHost.port)
                              && String_equals(slaveNode->name,jobNode->job.slaveHost.name)
                             );
    if (slaveNode != NULL)
    {
      connectorInfo = &slaveNode->connectorInfo;
      slaveNode->lockCount++;
    }
  }

  return connectorInfo;
}

void Job_connectorUnlock(ConnectorInfo *connectorInfo)
{
  SlaveNode *slaveNode;

  assert(connectorInfo != NULL);

//TODO: timeout
  JOB_SLAVE_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,5*MS_PER_SECOND)
  {
    slaveNode = LIST_FIND(&slaveList,
                          slaveNode,
                          &slaveNode->connectorInfo == connectorInfo
                         );
    if (slaveNode != NULL)
    {
      slaveNode->lockCount--;
    }
  }
}

#ifdef __cplusplus
  }
#endif

/* end of file */
