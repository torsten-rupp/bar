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
#define LOCK_TIMEOUT                             (10*60*1000)  // general lock timeout [ms]

#define SLAVE_DEBUG_LEVEL                        1
#define SLAVE_COMMAND_TIMEOUT                    (10LL*MS_PER_SECOND)

#define AUTHORIZATION_PENALITY_TIME              500      // delay processing by failCount^2*n [ms]
#define MAX_AUTHORIZATION_PENALITY_TIME          30000    // max. penality time [ms]
#define MAX_AUTHORIZATION_HISTORY_KEEP_TIME      30000    // max. time to keep entries in authorization fail history [ms]
#define MAX_AUTHORIZATION_FAIL_HISTORY           64       // max. length of history of authorization fail clients
#define MAX_ABORT_COMMAND_IDS                    512      // max. aborted command ids history

#define MAX_SCHEDULE_CATCH_TIME                  30       // max. schedule catch time [days]

#define PAIRING_MASTER_TIMEOUT                   120      // timeout pairing new master [s]

// sleep times [s]
//TODO
//#define SLEEP_TIME_SLAVE_CONNECT_THREAD                 ( 1*60)  // [s]
#define SLEEP_TIME_SLAVE_CONNECT_THREAD          (   10)  // [s]
//#define SLEEP_TIME_SLAVE_THREAD                  (    1)  // [s]
//TODO
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
LOCAL bool configValueParseScheduleDate(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL void configValueFormatInitScheduleDate(void **formatUserData, void *userData, void *variable);
LOCAL void configValueFormatDoneScheduleDate(void **formatUserData, void *userData);
LOCAL bool configValueFormatScheduleDate(void **formatUserData, void *userData, String line);
LOCAL bool configValueParseScheduleWeekDaySet(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL void configValueFormatInitScheduleWeekDaySet(void **formatUserData, void *userData, void *variable);
LOCAL void configValueFormatDoneScheduleWeekDaySet(void **formatUserData, void *userData);
LOCAL bool configValueFormatScheduleWeekDaySet(void **formatUserData, void *userData, String line);
LOCAL bool configValueParseScheduleTime(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL void configValueFormatInitScheduleTime(void **formatUserData, void *userData, void *variable);
LOCAL void configValueFormatDoneScheduleTime(void **formatUserData, void *userData);
LOCAL bool configValueFormatScheduleTime(void **formatUserData, void *userData, String line);

LOCAL bool configValueParsePersistenceMinKeep(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL void configValueFormatInitPersistenceMinKeep(void **formatUserData, void *userData, void *variable);
LOCAL void configValueFormatDonePersistenceMinKeep(void **formatUserData, void *userData);
LOCAL bool configValueFormatPersistenceMinKeep(void **formatUserData, void *userData, String line);
LOCAL bool configValueParsePersistenceMaxKeep(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL void configValueFormatInitPersistenceMaxKeep(void **formatUserData, void *userData, void *variable);
LOCAL void configValueFormatDonePersistenceMaxKeep(void **formatUserData, void *userData);
LOCAL bool configValueFormatPersistenceMaxKeep(void **formatUserData, void *userData, String line);
LOCAL bool configValueParsePersistenceMaxAge(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL void configValueFormatInitPersistenceMaxAge(void **formatUserData, void *userData, void *variable);
LOCAL void configValueFormatDonePersistenceMaxAge(void **formatUserData, void *userData);
LOCAL bool configValueFormatPersistenceMaxAge(void **formatUserData, void *userData, String line);

// handle deprecated configuration values
LOCAL bool configValueParseDeprecatedRemoteHost(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueParseDeprecatedRemotePort(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueParseDeprecatedRemoteForceSSL(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

LOCAL bool configValueParseDeprecatedSchedule(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueParseDeprecatedScheduleMinKeep(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueParseDeprecatedScheduleMaxKeep(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueParseDeprecatedScheduleMaxAge(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

LOCAL bool configValueParseDeprecatedMountDevice(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueParseDeprecatedStopOnError(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);
LOCAL bool configValueParseDeprecatedOverwriteFiles(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

const ConfigValue JOB_CONFIG_VALUES[] = CONFIG_VALUE_ARRAY
(
  CONFIG_STRUCT_VALUE_STRING      ("UUID",                    JobNode,job.uuid                                    ),
  CONFIG_STRUCT_VALUE_STRING      ("slave-host-name",         JobNode,slaveHost.name                          ),
  CONFIG_STRUCT_VALUE_INTEGER     ("slave-host-port",         JobNode,slaveHost.port,                         0,65535,NULL),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("slave-host-force-ssl",    JobNode,slaveHost.forceSSL                      ),
  CONFIG_STRUCT_VALUE_STRING      ("archive-name",            JobNode,job.archiveName                             ),
  CONFIG_STRUCT_VALUE_SELECT      ("archive-type",            JobNode,job.options.archiveType,                 CONFIG_VALUE_ARCHIVE_TYPES),

  CONFIG_STRUCT_VALUE_STRING      ("incremental-list-file",   JobNode,job.options.incrementalListFileName      ),

  CONFIG_STRUCT_VALUE_INTEGER64   ("archive-part-size",       JobNode,job.options.archivePartSize,             0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_STRUCT_VALUE_INTEGER     ("directory-strip",         JobNode,job.options.directoryStripCount,         -1,MAX_INT,NULL),
  CONFIG_STRUCT_VALUE_STRING      ("destination",             JobNode,job.options.destination                  ),
  CONFIG_STRUCT_VALUE_SPECIAL     ("owner",                   JobNode,job.options.owner,                       configValueParseOwner,configValueFormatInitOwner,NULL,configValueFormatOwner,NULL),

  CONFIG_STRUCT_VALUE_SELECT      ("pattern-type",            JobNode,job.options.patternType,                 CONFIG_VALUE_PATTERN_TYPES),

  CONFIG_STRUCT_VALUE_SPECIAL     ("compress-algorithm",      JobNode,job.options.compressAlgorithms,          configValueParseCompressAlgorithms,configValueFormatInitCompressAlgorithms,configValueFormatDoneCompressAlgorithms,configValueFormatCompressAlgorithms,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL     ("compress-exclude",        JobNode,job.compressExcludePatternList,          configValueParsePattern,configValueFormatInitPattern,configValueFormatDonePattern,configValueFormatPattern,NULL),

  CONFIG_STRUCT_VALUE_SPECIAL     ("crypt-algorithm",         JobNode,job.options.cryptAlgorithms,             configValueParseCryptAlgorithms,configValueFormatInitCryptAlgorithms,configValueFormatDoneCryptAlgorithms,configValueFormatCryptAlgorithms,NULL),
  CONFIG_STRUCT_VALUE_SELECT      ("crypt-type",              JobNode,job.options.cryptType,                   CONFIG_VALUE_CRYPT_TYPES),
  CONFIG_STRUCT_VALUE_SELECT      ("crypt-password-mode",     JobNode,job.options.cryptPasswordMode,           CONFIG_VALUE_PASSWORD_MODES),
  CONFIG_STRUCT_VALUE_SPECIAL     ("crypt-password",          JobNode,job.options.cryptPassword,               configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL     ("crypt-public-key",        JobNode,job.options.cryptPublicKey,              configValueParseKeyData,NULL,NULL,NULL,NULL),

  CONFIG_STRUCT_VALUE_STRING      ("pre-command",             JobNode,job.options.preProcessScript             ),
  CONFIG_STRUCT_VALUE_STRING      ("post-command",            JobNode,job.options.postProcessScript            ),

  CONFIG_STRUCT_VALUE_STRING      ("ftp-login-name",          JobNode,job.options.ftpServer.loginName          ),
  CONFIG_STRUCT_VALUE_SPECIAL     ("ftp-password",            JobNode,job.options.ftpServer.password,          configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),

  CONFIG_STRUCT_VALUE_INTEGER     ("ssh-port",                JobNode,job.options.sshServer.port,              0,65535,NULL),
  CONFIG_STRUCT_VALUE_STRING      ("ssh-login-name",          JobNode,job.options.sshServer.loginName          ),
  CONFIG_STRUCT_VALUE_SPECIAL     ("ssh-password",            JobNode,job.options.sshServer.password,          configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL     ("ssh-public-key",          JobNode,job.options.sshServer.publicKey,         configValueParseKeyData,NULL,NULL,NULL,NULL),
//  CONFIG_STRUCT_VALUE_SPECIAL     ("ssh-public-key-data",     JobNode,job.options.sshServer.publicKey,         configValueParseKeyData,NULL,NULL,NULL,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL     ("ssh-private-key",         JobNode,job.options.sshServer.privateKey,        configValueParseKeyData,NULL,NULL,NULL,NULL),
//  CONFIG_STRUCT_VALUE_SPECIAL     ("ssh-private-key-data",    JobNode,job.options.sshServer.privateKey,        configValueParseKeyData,NULL,NULL,NULL,NULL),

  CONFIG_STRUCT_VALUE_SPECIAL     ("include-file",            JobNode,job.includeEntryList,                       configValueParseFileEntryPattern,configValueFormatInitEntryPattern,configValueFormatDoneEntryPattern,configValueFormatFileEntryPattern,NULL),
  CONFIG_STRUCT_VALUE_STRING      ("include-file-command",    JobNode,job.includeFileCommand                      ),
  CONFIG_STRUCT_VALUE_SPECIAL     ("include-image",           JobNode,job.includeEntryList,                       configValueParseImageEntryPattern,configValueFormatInitEntryPattern,configValueFormatDoneEntryPattern,configValueFormatImageEntryPattern,NULL),
  CONFIG_STRUCT_VALUE_STRING      ("include-image-command",   JobNode,job.includeImageCommand                     ),
  CONFIG_STRUCT_VALUE_SPECIAL     ("exclude",                 JobNode,job.excludePatternList,                     configValueParsePattern,configValueFormatInitPattern,configValueFormatDonePattern,configValueFormatPattern,NULL),
  CONFIG_STRUCT_VALUE_STRING      ("exclude-command",         JobNode,job.excludeCommand                          ),
  CONFIG_STRUCT_VALUE_SPECIAL     ("delta-source",            JobNode,job.deltaSourceList,                        configValueParseDeltaSource,configValueFormatInitDeltaSource,configValueFormatDoneDeltaSource,configValueFormatDeltaSource,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL     ("mount",                   JobNode,job.mountList,                              configValueParseMount,configValueFormatInitMount,configValueFormatDoneMount,configValueFormatMount,NULL),

  CONFIG_STRUCT_VALUE_INTEGER64   ("max-storage-size",        JobNode,job.options.maxStorageSize,              0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_STRUCT_VALUE_INTEGER64   ("volume-size",             JobNode,job.options.volumeSize,                  0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("ecc",                     JobNode,job.options.errorCorrectionCodesFlag     ),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("blank",                   JobNode,job.options.blankFlag                    ),

  CONFIG_STRUCT_VALUE_BOOLEAN     ("skip-unreadable",         JobNode,job.options.skipUnreadableFlag           ),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("raw-images",              JobNode,job.options.rawImagesFlag                ),
  CONFIG_STRUCT_VALUE_SELECT      ("archive-file-mode",       JobNode,job.options.archiveFileMode,             CONFIG_VALUE_ARCHIVE_FILE_MODES),
  CONFIG_STRUCT_VALUE_SELECT      ("restore-entry-mode",      JobNode,job.options.restoreEntryMode,            CONFIG_VALUE_RESTORE_ENTRY_MODES),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("wait-first-volume",       JobNode,job.options.waitFirstVolumeFlag          ),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("no-signature",            JobNode,job.options.noSignatureFlag              ),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("no-bar-on-medium",        JobNode,job.options.noBAROnMediumFlag            ),
  CONFIG_STRUCT_VALUE_BOOLEAN     ("no-stop-on-error",        JobNode,job.options.noStopOnErrorFlag            ),

  CONFIG_VALUE_BEGIN_SECTION("schedule",-1),
    CONFIG_STRUCT_VALUE_STRING    ("UUID",                    ScheduleNode,uuid                               ),
    CONFIG_STRUCT_VALUE_STRING    ("parentUUID",              ScheduleNode,parentUUID                         ),
    CONFIG_STRUCT_VALUE_SPECIAL   ("date",                    ScheduleNode,date,                              configValueParseScheduleDate,configValueFormatInitScheduleDate,configValueFormatDoneScheduleDate,configValueFormatScheduleDate,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL   ("weekdays",                ScheduleNode,weekDaySet,                        configValueParseScheduleWeekDaySet,configValueFormatInitScheduleWeekDaySet,configValueFormatDoneScheduleWeekDaySet,configValueFormatScheduleWeekDaySet,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL   ("time",                    ScheduleNode,time,                              configValueParseScheduleTime,configValueFormatInitScheduleTime,configValueFormatDoneScheduleTime,configValueFormatScheduleTime,NULL),
    CONFIG_STRUCT_VALUE_SELECT    ("archive-type",            ScheduleNode,archiveType,                       CONFIG_VALUE_ARCHIVE_TYPES),
    CONFIG_STRUCT_VALUE_INTEGER   ("interval",                ScheduleNode,interval,                          0,MAX_INT,NULL),
    CONFIG_STRUCT_VALUE_STRING    ("text",                    ScheduleNode,customText                         ),
    CONFIG_STRUCT_VALUE_BOOLEAN   ("no-storage",              ScheduleNode,noStorage                          ),
    CONFIG_STRUCT_VALUE_BOOLEAN   ("enabled",                 ScheduleNode,enabled                            ),

    // deprecated
    CONFIG_STRUCT_VALUE_DEPRECATED("min-keep",                                                                configValueParseDeprecatedScheduleMinKeep,NULL,NULL,TRUE),
    CONFIG_STRUCT_VALUE_DEPRECATED("max-keep",                                                                configValueParseDeprecatedScheduleMaxKeep,NULL,NULL,TRUE),
    CONFIG_STRUCT_VALUE_DEPRECATED("max-age",                                                                 configValueParseDeprecatedScheduleMaxAge,NULL,NULL,TRUE),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_VALUE_BEGIN_SECTION("persistence",-1),
    CONFIG_STRUCT_VALUE_SPECIAL   ("min-keep",                PersistenceNode,minKeep,                        configValueParsePersistenceMinKeep,configValueFormatInitPersistenceMinKeep,configValueFormatDonePersistenceMinKeep,configValueFormatPersistenceMinKeep,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL   ("max-keep",                PersistenceNode,maxKeep,                        configValueParsePersistenceMaxKeep,configValueFormatInitPersistenceMaxKeep,configValueFormatDonePersistenceMaxKeep,configValueFormatPersistenceMaxKeep,NULL),
    CONFIG_STRUCT_VALUE_SPECIAL   ("max-age",                 PersistenceNode,maxAge,                         configValueParsePersistenceMaxAge,configValueFormatInitPersistenceMaxAge,configValueFormatDonePersistenceMaxAge,configValueFormatPersistenceMaxAge,NULL),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_STRUCT_VALUE_STRING      ("comment",                 JobNode,job.options.comment                      ),

  // deprecated
  CONFIG_STRUCT_VALUE_DEPRECATED  ("remote-host-name",                                                        configValueParseDeprecatedRemoteHost,NULL,NULL,FALSE),
  CONFIG_STRUCT_VALUE_DEPRECATED  ("remote-host-port",                                                        configValueParseDeprecatedRemotePort,NULL,NULL,FALSE),
  CONFIG_STRUCT_VALUE_DEPRECATED  ("remote-host-force-ssl",                                                   configValueParseDeprecatedRemoteForceSSL,NULL,NULL,FALSE),
  CONFIG_STRUCT_VALUE_DEPRECATED  ("mount-device",                                                            configValueParseDeprecatedMountDevice,NULL,NULL,FALSE),
  CONFIG_STRUCT_VALUE_DEPRECATED  ("schedule",                                                                configValueParseDeprecatedSchedule,NULL,NULL,FALSE),
//TODO
  CONFIG_STRUCT_VALUE_IGNORE      ("overwrite-archive-files"                                                  ),
  // Note: shortcut for --restore-entries-mode=overwrite
  CONFIG_STRUCT_VALUE_DEPRECATED  ("overwrite-files",                                                         configValueParseDeprecatedOverwriteFiles,NULL,NULL,FALSE),
  CONFIG_STRUCT_VALUE_DEPRECATED  ("stop-on-error",                                                           configValueParseDeprecatedStopOnError,NULL,NULL,FALSE),
);

/***************************** Variables *******************************/
JobList jobList;                // job list
//LOCAL Thread                jobThread;              // thread executing jobs create/restore

//LOCAL bool                  quitFlag;               // TRUE iff quit requested

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

  String_delete(scheduleNode->lastErrorMessage);
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
  scheduleNode->lastErrorMessage          = String_new();
  scheduleNode->executionCount            = 0L;
  scheduleNode->averageDuration           = 0LL;
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
  scheduleNode->lastErrorMessage          = String_new();
  scheduleNode->executionCount            = 0L;
  scheduleNode->averageDuration           = 0LL;
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
  persistenceNode->id          = Misc_getId();
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
* Name   : configValueParseScheduleDate
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

LOCAL bool configValueParseScheduleDate(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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
    if (!parseDateTimeNumber(s0,&date.year )) errorFlag = TRUE;
    if (!parseDateMonth     (s1,&date.month)) errorFlag = TRUE;
    if (!parseDateTimeNumber(s2,&date.day  )) errorFlag = TRUE;
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
* Name   : configValueFormatInitScheduleDate
* Purpose: init format config schedule
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void configValueFormatInitScheduleDate(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (ScheduleDate*)variable;
}

/***********************************************************************\
* Name   : configValueFormatDoneScheduleDate
* Purpose: done format of config schedule statements
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void configValueFormatDoneScheduleDate(void **formatUserData, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

/***********************************************************************\
* Name   : configValueFormatScheduleDate
* Purpose: format schedule config statement
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueFormatScheduleDate(void **formatUserData, void *userData, String line)
{
  const ScheduleDate *scheduleDate;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  scheduleDate = (const ScheduleDate*)(*formatUserData);
  if (scheduleDate != NULL)
  {
    if (scheduleDate->year != DATE_ANY)
    {
      String_appendFormat(line,"%d",scheduleDate->year);
    }
    else
    {
      String_appendCString(line,"*");
    }
    String_appendChar(line,'-');
    if (scheduleDate->month != DATE_ANY)
    {
      String_appendFormat(line,"%d",scheduleDate->month);
    }
    else
    {
      String_appendCString(line,"*");
    }
    String_appendChar(line,'-');
    if (scheduleDate->day != DATE_ANY)
    {
      String_appendFormat(line,"%d",scheduleDate->day);
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

/***********************************************************************\
* Name   : configValueParseScheduleWeekDaySet
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

LOCAL bool configValueParseScheduleWeekDaySet(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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
* Name   : configValueFormatInitScheduleWeekDaySet
* Purpose: init format config schedule week day set
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void configValueFormatInitScheduleWeekDaySet(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (WeekDaySet*)variable;
}

/***********************************************************************\
* Name   : configValueFormatDoneScheduleWeekDays
* Purpose: done format of config schedule week day set
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void configValueFormatDoneScheduleWeekDaySet(void **formatUserData, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

/***********************************************************************\
* Name   : configValueFormatScheduleWeekDaySet
* Purpose: format schedule config week day set
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueFormatScheduleWeekDaySet(void **formatUserData, void *userData, String line)
{
  const ScheduleWeekDaySet *scheduleWeekDaySet;
  String                   names;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  scheduleWeekDaySet = (ScheduleWeekDaySet*)(*formatUserData);
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

/***********************************************************************\
* Name   : configValueParseScheduleTime
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

LOCAL bool configValueParseScheduleTime(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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
    if (!parseDateTimeNumber(s0,&time.hour  )) errorFlag = TRUE;
    if (!parseDateTimeNumber(s1,&time.minute)) errorFlag = TRUE;
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
* Name   : configValueFormatInitScheduleTime
* Purpose: init format config schedule
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void configValueFormatInitScheduleTime(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (ScheduleTime*)variable;
}

/***********************************************************************\
* Name   : configValueFormatDoneScheduleTime
* Purpose: done format of config schedule
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void configValueFormatDoneScheduleTime(void **formatUserData, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

/***********************************************************************\
* Name   : configValueFormatScheduleTime
* Purpose: format schedule config
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueFormatScheduleTime(void **formatUserData, void *userData, String line)
{
  const ScheduleTime *scheduleTime;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  scheduleTime = (const ScheduleTime*)(*formatUserData);
  if (scheduleTime != NULL)
  {
    if (scheduleTime->hour != TIME_ANY)
    {
      String_appendFormat(line,"%d",scheduleTime->hour);
    }
    else
    {
      String_appendCString(line,"*");
    }
    String_appendChar(line,':');
    if (scheduleTime->minute != TIME_ANY)
    {
      String_appendFormat(line,"%d",scheduleTime->minute);
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

/***********************************************************************\
* Name   : configValueParsePersistenceMinKeep
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

LOCAL bool configValueParsePersistenceMinKeep(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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
* Name   : configValueFormatInitPersistenceMinKeep
* Purpose: init format config min. keep
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void configValueFormatInitPersistenceMinKeep(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (PersistenceNode*)variable;
}

/***********************************************************************\
* Name   : configValueFormatDonePersistenceMinKeep
* Purpose: done format of config min. keep
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void configValueFormatDonePersistenceMinKeep(void **formatUserData, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

/***********************************************************************\
* Name   : configValueFormatPersistenceMinKeep
* Purpose: format min. keep config
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueFormatPersistenceMinKeep(void **formatUserData, void *userData, String line)
{
  const int *minKeep;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  minKeep = *((int**)formatUserData);
  if (minKeep != NULL)
  {
    if ((*minKeep) != KEEP_ALL)
    {
      String_appendFormat(line,"%d",*minKeep);
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

/***********************************************************************\
* Name   : configValueParsePersistenceMaxKeep
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

LOCAL bool configValueParsePersistenceMaxKeep(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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
* Name   : configValueFormatInitPersistenceMaxKeep
* Purpose: init format config max. keep
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void configValueFormatInitPersistenceMaxKeep(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (PersistenceNode*)variable;
}

/***********************************************************************\
* Name   : configValueFormatDonePersistenceMaxKeep
* Purpose: done format of config max. keep
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void configValueFormatDonePersistenceMaxKeep(void **formatUserData, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

/***********************************************************************\
* Name   : configValueFormatPersistenceMaxKeep
* Purpose: format max. keep config
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueFormatPersistenceMaxKeep(void **formatUserData, void *userData, String line)
{
  const int *maxKeep;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  maxKeep = *((int**)formatUserData);
  if (maxKeep != NULL)
  {
    if ((*maxKeep) != KEEP_ALL)
    {
      String_appendFormat(line,"%d",*maxKeep);
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

LOCAL bool configValueParsePersistenceMaxAge(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
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
* Name   : configValueFormatInitPersistenceMaxAge
* Purpose: init format config max. age
* Input  : userData - user data
*          variable - config variable
* Output : formatUserData - format user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void configValueFormatInitPersistenceMaxAge(void **formatUserData, void *userData, void *variable)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  (*formatUserData) = (PersistenceNode*)variable;
}

/***********************************************************************\
* Name   : configValueFormatDonePersistenceMaxAge
* Purpose: done format of config max. age
* Input  : formatUserData - format user data
*          userData       - user data
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void configValueFormatDonePersistenceMaxAge(void **formatUserData, void *userData)
{
  assert(formatUserData != NULL);

  UNUSED_VARIABLE(formatUserData);
  UNUSED_VARIABLE(userData);
}

/***********************************************************************\
* Name   : configValueFormatPersistenceMaxAge
* Purpose: format max. age config
* Input  : formatUserData - format user data
*          userData       - user data
*          line           - line variable
*          name           - config name
* Output : line - formated line
* Return : TRUE if config statement formated, FALSE if end of data
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool configValueFormatPersistenceMaxAge(void **formatUserData, void *userData, String line)
{
  const int *maxAge;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);

  maxAge = *((int**)formatUserData);
  if (maxAge != NULL)
  {
    if ((*maxAge) != AGE_FOREVER)
    {
      String_appendFormat(line,"%d",*maxAge);
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

/***********************************************************************\
* Name   : parseScheduleArchiveType
* Purpose: parse archive type
* Input  : s - string to parse
* Output : archiveType - archive type
* Return : TRUE iff archive type parsed
* Notes  : -
\***********************************************************************/

LOCAL bool parseScheduleArchiveType(ConstString s, ArchiveTypes *archiveType)
{
  assert(s != NULL);
  assert(archiveType != NULL);

  if (String_equalsCString(s,"*"))
  {
    (*archiveType) = ARCHIVE_TYPE_NORMAL;
  }
  else
  {
    if (!Archive_parseType(String_cString(s),archiveType,NULL))
    {
      return FALSE;
    }
  }

  return TRUE;
}

/***********************************************************************\
* Name   : parseSchedule
* Purpose: parse schedule (old style)
* Input  : s - schedule string
* Output :
* Return : scheduleNode or NULL on error
* Notes  : string format
*            <year|*>-<month|*>-<day|*> [<week day|*>] <hour|*>:<minute|*> <0|1> <archive type>
*          month names: jan, feb, mar, apr, may, jun, jul, aug, sep, oct
*          nov, dec
*          week day names: mon, tue, wed, thu, fri, sat, sun
*          archive type names: normal, full, incremental, differential
\***********************************************************************/

LOCAL ScheduleNode *parseSchedule(ConstString s)
{
  ScheduleNode *scheduleNode;
  bool         errorFlag;
  String       s0,s1,s2;
  bool         b;
  long         nextIndex;

  assert(s != NULL);

  // allocate new schedule node
  scheduleNode = newScheduleNode();
  assert(scheduleNode != NULL);
  Misc_getUUID(scheduleNode->uuid);

  // parse schedule. Format: date [weekday] time enabled [type]
  errorFlag = FALSE;
  s0 = String_new();
  s1 = String_new();
  s2 = String_new();
  nextIndex = STRING_BEGIN;
  if      (String_parse(s,nextIndex,"%S-%S-%S",&nextIndex,s0,s1,s2))
  {
    if (!parseDateTimeNumber(s0,&scheduleNode->date.year )) errorFlag = TRUE;
    if (!parseDateMonth     (s1,&scheduleNode->date.month)) errorFlag = TRUE;
    if (!parseDateTimeNumber(s2,&scheduleNode->date.day  )) errorFlag = TRUE;
  }
  else
  {
    errorFlag = TRUE;
  }
  if      (String_parse(s,nextIndex,"%S %S:%S",&nextIndex,s0,s1,s2))
  {
    if (!parseWeekDaySet(String_cString(s0),&scheduleNode->weekDaySet)) errorFlag = TRUE;
    if (!parseDateTimeNumber(s1,&scheduleNode->time.hour  )) errorFlag = TRUE;
    if (!parseDateTimeNumber(s2,&scheduleNode->time.minute)) errorFlag = TRUE;
  }
  else if (String_parse(s,nextIndex,"%S:%S",&nextIndex,s0,s1))
  {
    if (!parseDateTimeNumber(s0,&scheduleNode->time.hour  )) errorFlag = TRUE;
    if (!parseDateTimeNumber(s1,&scheduleNode->time.minute)) errorFlag = TRUE;
  }
  else
  {
    errorFlag = TRUE;
  }
  if (String_parse(s,nextIndex,"%y",&nextIndex,&b))
  {
/* It seems gcc has a bug in option -fno-schedule-insns2: if -O2 is used this
   option is enabled. Then either the program crashes with a SigSegV or parsing
   boolean values here fail. It seems the address of 'b' is not received in the
   function. Because this problem disappear when -fno-schedule-insns2 is given
   it looks like the gcc do some rearrangements in the generated machine code
   which is not valid anymore. How can this be tracked down? Is this problem
   known?
*/
if ((b != FALSE) && (b != TRUE)) HALT_INTERNAL_ERROR("parsing boolean string value fail - C compiler bug?");
    scheduleNode->enabled = b;
  }
  else
  {
    errorFlag = TRUE;
  }
//fprintf(stderr,"%s,%d: scheduleNode->enabled=%d %p\n",__FILE__,__LINE__,scheduleNode->enabled,&b);
  if (nextIndex != STRING_END)
  {
    if (String_parse(s,nextIndex,"%S",&nextIndex,s0))
    {
      if (!parseScheduleArchiveType(s0,&scheduleNode->archiveType)) errorFlag = TRUE;
    }
  }
  String_delete(s2);
  String_delete(s1);
  String_delete(s0);

  if (errorFlag || (nextIndex != STRING_END))
  {
    LIST_DELETE_NODE(scheduleNode);
    return NULL;
  }

  return scheduleNode;
}

/***********************************************************************\
* Name   : configValueParseDeprecatedRemoteHost
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

bool configValueParseDeprecatedRemoteHost(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  String_setCString(((JobNode*)variable)->slaveHost.name,value);

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseDeprecatedRemotePort
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

bool configValueParseDeprecatedRemotePort(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  uint n;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if (!stringToUInt(value,&n))
  {
    return FALSE;
  }
  ((JobNode*)variable)->slaveHost.port = n;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseDeprecatedRemoteForceSSL
* Purpose: config value option call back for deprecated remote force SSL
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

bool configValueParseDeprecatedRemoteForceSSL(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if      (   stringEqualsIgnoreCase(value,"1")
           || stringEqualsIgnoreCase(value,"true")
           || stringEqualsIgnoreCase(value,"on")
           || stringEqualsIgnoreCase(value,"yes")
          )
  {
    (*(bool*)variable) = TRUE;
  }
  else if (   stringEqualsIgnoreCase(value,"0")
           || stringEqualsIgnoreCase(value,"false")
           || stringEqualsIgnoreCase(value,"off")
           || stringEqualsIgnoreCase(value,"no")
          )
  {
    ((JobNode*)variable)->slaveHost.forceSSL = FALSE;
  }
  else
  {
    return FALSE;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseDeprecatedSchedule
* Purpose: config value option call back for parsing deprecated schedule
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

LOCAL bool configValueParseDeprecatedSchedule(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  ScheduleNode *scheduleNode;
  String       s;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);

  // parse schedule (old style)
  s = String_newCString(value);
  scheduleNode = parseSchedule(s);
  if (scheduleNode == NULL)
  {
    snprintf(errorMessage,errorMessageSize,"Cannot parse schedule '%s'",value);
    String_delete(s);
    return FALSE;
  }
  String_delete(s);

  // get schedule info (if possible)
  scheduleNode->lastExecutedDateTime = 0LL;
  String_clear(scheduleNode->lastErrorMessage);
  scheduleNode->executionCount       = 0L;
  scheduleNode->averageDuration      = 0LL;
  scheduleNode->totalEntityCount     = 0L;
  scheduleNode->totalStorageCount    = 0L;
  scheduleNode->totalStorageSize     = 0LL;
  scheduleNode->totalEntryCount      = 0L;
  scheduleNode->totalEntrySize       = 0LL;
#ifndef WERROR
#warning TODO
#endif
#if 0
  if (indexHandle != NULL)
  {
    (void)Index_findUUID(indexHandle,
                         NULL, // jobUUID
                         scheduleNode->uuid,
                         NULL,  // uuidId,
                         &scheduleNode->lastExecutedDateTime,
                         scheduleNode->lastErrorMessage,
                         (scheduleNode->archiveType == ARCHIVE_TYPE_NORMAL      ) ? &scheduleNode->executionCount  : NULL,
                         (scheduleNode->archiveType == ARCHIVE_TYPE_FULL        ) ? &scheduleNode->executionCount  : NULL,
                         (scheduleNode->archiveType == ARCHIVE_TYPE_INCREMENTAL ) ? &scheduleNode->executionCount  : NULL,
                         (scheduleNode->archiveType == ARCHIVE_TYPE_DIFFERENTIAL) ? &scheduleNode->executionCount  : NULL,
                         (scheduleNode->archiveType == ARCHIVE_TYPE_CONTINUOUS  ) ? &scheduleNode->executionCount  : NULL,
                         (scheduleNode->archiveType == ARCHIVE_TYPE_NORMAL      ) ? &scheduleNode->averageDuration : NULL,
                         (scheduleNode->archiveType == ARCHIVE_TYPE_FULL        ) ? &scheduleNode->averageDuration : NULL,
                         (scheduleNode->archiveType == ARCHIVE_TYPE_INCREMENTAL ) ? &scheduleNode->averageDuration : NULL,
                         (scheduleNode->archiveType == ARCHIVE_TYPE_DIFFERENTIAL) ? &scheduleNode->averageDuration : NULL,
                         (scheduleNode->archiveType == ARCHIVE_TYPE_CONTINUOUS  ) ? &scheduleNode->averageDuration : NULL,
                         &scheduleNode->totalEntityCount,
                         &scheduleNode->totalStorageCount,
                         &scheduleNode->totalStorageSize,
                         &scheduleNode->totalEntryCount,
                         &scheduleNode->totalEntrySize
                        );
  }
#endif

  // append to list
  List_append((ScheduleList*)variable,scheduleNode);

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseDeprecatedScheduleMinKeep
* Purpose: config value option call back for deprecated min-keep
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

LOCAL bool configValueParseDeprecatedScheduleMinKeep(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  uint n;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if (!stringToUInt(value,&n))
  {
    return FALSE;
  }
  ((ScheduleNode*)variable)->deprecatedPersistenceFlag = TRUE;
  ((ScheduleNode*)variable)->minKeep                   = n;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseDeprecatedScheduleMaxKeep
* Purpose: config value option call back for deprecated max-keep
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

LOCAL bool configValueParseDeprecatedScheduleMaxKeep(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  uint n;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if (!stringToUInt(value,&n))
  {
    return FALSE;
  }
  ((ScheduleNode*)variable)->deprecatedPersistenceFlag = TRUE;
  ((ScheduleNode*)variable)->maxKeep                   = n;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseDeprecatedScheduleMaxAge
* Purpose: config value option call back for deprecated max-age
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

LOCAL bool configValueParseDeprecatedScheduleMaxAge(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  uint n;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if (!stringToUInt(value,&n))
  {
    return FALSE;
  }
  ((ScheduleNode*)variable)->deprecatedPersistenceFlag = TRUE;
  ((ScheduleNode*)variable)->maxAge                    = n;

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseDeprecatedMountDevice
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

LOCAL bool configValueParseDeprecatedMountDevice(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  MountNode *mountNode;

  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  if (!stringIsEmpty(value))
  {
    // add to mount list
    mountNode = newMountNodeCString(value,
                                    NULL,  // deviceName
                                    FALSE  // alwaysUnmount
                                   );
    if (mountNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    List_append(&((JobNode*)variable)->job.mountList,mountNode);
  }

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseDeprecatedStopOnError
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

LOCAL bool configValueParseDeprecatedStopOnError(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  ((JobNode*)variable)->job.options.noStopOnErrorFlag = !(   (value == NULL)
                                                         || stringEquals(value,"1")
                                                         || stringEqualsIgnoreCase(value,"true")
                                                         || stringEqualsIgnoreCase(value,"on")
                                                         || stringEqualsIgnoreCase(value,"yes")
                                                        );

  return TRUE;
}

/***********************************************************************\
* Name   : configValueParseDeprecatedOverwriteFiles
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

LOCAL bool configValueParseDeprecatedOverwriteFiles(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize)
{
  assert(variable != NULL);
  assert(value != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(value);
  UNUSED_VARIABLE(errorMessage);
  UNUSED_VARIABLE(errorMessageSize);

  ((JobNode*)variable)->job.options.restoreEntryMode = RESTORE_ENTRY_MODE_OVERWRITE;

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

  doneStatusInfo(&jobNode->statusInfo);

  String_delete(jobNode->lastErrorMessage);

  String_delete(jobNode->volumeMessage);

  String_delete(jobNode->abortedByInfo);

  String_delete(jobNode->scheduleCustomText);
  String_delete(jobNode->scheduleUUID);

  if (jobNode->cryptPassword != NULL) Password_delete(jobNode->cryptPassword);
  if (jobNode->sshPassword != NULL) Password_delete(jobNode->sshPassword);
  if (jobNode->ftpPassword != NULL) Password_delete(jobNode->ftpPassword);
  String_delete(jobNode->byName);

  Connector_done(&jobNode->connectorInfo);

  Job_done(&jobNode->job);
  String_delete(jobNode->slaveHost.name);
  String_delete(jobNode->name);
  String_delete(jobNode->fileName);
}

/*---------------------------------------------------------------------*/

Errors Job_initAll(void)
{
  Semaphore_init(&jobList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&jobList);

  return ERROR_NONE;
}

void Job_doneAll(void)
{
  List_done(&jobList,CALLBACK((ListNodeFreeFunction)freeJobNode,NULL));
  Semaphore_done(&jobList.lock);
}

void Job_init(Job *job)
{
  assert(job != NULL);

  job->uuid                                      = String_new();
  job->archiveName                               = String_new();
  EntryList_init(&job->includeEntryList);
  job->includeFileCommand                        = String_new();
  job->includeImageCommand                       = String_new();
  PatternList_init(&job->excludePatternList);
  job->excludeCommand                            = String_new();
  List_init(&job->mountList);
  PatternList_init(&job->compressExcludePatternList);
  DeltaSourceList_init(&job->deltaSourceList);
  List_init(&job->scheduleList);
  List_init(&job->persistenceList);
  job->persistenceList.lastModificationTimestamp = 0LL;
  Job_initOptions(&job->options);

  DEBUG_ADD_RESOURCE_TRACE(job,Job);
}

void Job_duplicate(Job *job, const Job *fromJob)
{
  assert(job != NULL);
  assert(fromJob != NULL);

  DEBUG_CHECK_RESOURCE_TRACE(fromJob);

  job->uuid                                      = String_duplicate(fromJob->uuid);
  job->archiveName                               = String_duplicate(fromJob->archiveName);
  EntryList_initDuplicate(&job->includeEntryList,
                          &fromJob->includeEntryList,
                          CALLBACK(NULL,NULL)
                         );
  job->includeFileCommand                        = String_duplicate(fromJob->includeFileCommand);
  job->includeImageCommand                       = String_duplicate(fromJob->includeImageCommand);
  PatternList_initDuplicate(&job->compressExcludePatternList,
                            &fromJob->compressExcludePatternList,
                            CALLBACK(NULL,NULL)
                           );
  job->excludeCommand                            = String_duplicate(fromJob->excludeCommand);
  List_initDuplicate(&job->mountList,
                     &fromJob->mountList,
                     CALLBACK(NULL,NULL),
                     CALLBACK((ListNodeDuplicateFunction)duplicateMountNode,NULL)
                    );
  PatternList_initDuplicate(&job->compressExcludePatternList,
                            &fromJob->compressExcludePatternList,
                            CALLBACK(NULL,NULL)
                           );
  DeltaSourceList_initDuplicate(&job->deltaSourceList,
                                &fromJob->deltaSourceList,
                                CALLBACK(NULL,NULL)
                               );
  List_initDuplicate(&job->scheduleList,
                     &fromJob->scheduleList,
                     CALLBACK(NULL,NULL),
                     CALLBACK((ListNodeDuplicateFunction)duplicateScheduleNode,NULL)
                    );
  List_initDuplicate(&job->persistenceList,
                     &fromJob->persistenceList,
                     CALLBACK(NULL,NULL),
                     CALLBACK((ListNodeDuplicateFunction)duplicatePersistenceNode,NULL)
                    );
  job->persistenceList.lastModificationTimestamp = 0LL;
  Job_duplicateOptions(&job->options,&fromJob->options);

  DEBUG_ADD_RESOURCE_TRACE(job,Job);
}

void Job_done(Job *job)
{
  assert(job != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(job,Job);

  Job_doneOptions(&job->options);
  List_done(&job->persistenceList,CALLBACK((ListNodeFreeFunction)freePersistenceNode,NULL));
  List_done(&job->scheduleList,CALLBACK((ListNodeFreeFunction)freeScheduleNode,NULL));
  DeltaSourceList_done(&job->deltaSourceList);
  PatternList_done(&job->compressExcludePatternList);
  List_done(&job->mountList,CALLBACK((ListNodeFreeFunction)freeMountNode,NULL));
  String_delete(job->excludeCommand);
  PatternList_done(&job->excludePatternList);
  String_delete(job->includeImageCommand);
  String_delete(job->includeFileCommand);
  EntryList_done(&job->includeEntryList);
  String_delete(job->archiveName);
  String_delete(job->uuid);
}

JobNode *Job_new(JobTypes         jobType,
                 ConstString      name,
                 ConstString      jobUUID,
                 ConstString      fileName,
                 const JobOptions *defaultJobOptions
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
  jobNode->name                                      = String_duplicate(name);
  jobNode->slaveHost.name                            = String_new();
  if (defaultJobOptions != NULL)
  {
    Job_setOptions(&jobNode->job.options,defaultJobOptions);
  }
  jobNode->jobType                                   = jobType;
  jobNode->modifiedFlag                              = FALSE;

  jobNode->lastScheduleCheckDateTime                 = 0LL;

  jobNode->ftpPassword                               = NULL;
  jobNode->sshPassword                               = NULL;
  jobNode->cryptPassword                             = NULL;

  jobNode->fileName                                  = String_duplicate(fileName);
  jobNode->fileModified                              = 0LL;

  jobNode->masterIO                                  = NULL;

  Connector_init(&jobNode->connectorInfo);

  jobNode->state                                     = JOB_STATE_NONE;
  jobNode->slaveState                                = SLAVE_STATE_OFFLINE;

  jobNode->scheduleUUID                              = String_new();
  jobNode->scheduleCustomText                        = String_new();
  jobNode->archiveType                               = ARCHIVE_TYPE_NORMAL;
  jobNode->noStorage                                 = FALSE;
  jobNode->dryRun                                    = FALSE;
  jobNode->byName                                    = String_new();

  jobNode->requestedAbortFlag                        = FALSE;
  jobNode->abortedByInfo                             = String_new();
  jobNode->requestedVolumeNumber                     = 0;
  jobNode->volumeNumber                              = 0;
  jobNode->volumeMessage                             = String_new();
  jobNode->volumeUnloadFlag                          = FALSE;

  jobNode->lastExecutedDateTime                      = 0LL;
  jobNode->lastErrorMessage                          = String_new();
  jobNode->executionCount.normal                     = 0L;
  jobNode->executionCount.full                       = 0L;
  jobNode->executionCount.incremental                = 0L;
  jobNode->executionCount.differential               = 0L;
  jobNode->executionCount.continuous                 = 0L;
  jobNode->averageDuration.normal                    = 0LL;
  jobNode->averageDuration.full                      = 0LL;
  jobNode->averageDuration.incremental               = 0LL;
  jobNode->averageDuration.differential              = 0LL;
  jobNode->averageDuration.continuous                = 0LL;
  jobNode->totalEntityCount                          = 0L;
  jobNode->totalStorageCount                         = 0L;
  jobNode->totalStorageSize                          = 0LL;
  jobNode->totalEntryCount                           = 0L;
  jobNode->totalEntrySize                            = 0LL;

  initStatusInfo(&jobNode->statusInfo);

  Misc_performanceFilterInit(&jobNode->runningInfo.entriesPerSecondFilter,     10*60);
  Misc_performanceFilterInit(&jobNode->runningInfo.bytesPerSecondFilter,       10*60);
  Misc_performanceFilterInit(&jobNode->runningInfo.storageBytesPerSecondFilter,10*60);

  Job_resetRunningInfo(jobNode);

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
  Job_duplicate(&newJobNode->job,&jobNode->job);
  newJobNode->name                                      = File_getBaseName(String_new(),fileName);
  newJobNode->slaveHost.name                            = String_duplicate(jobNode->slaveHost.name);
  newJobNode->slaveHost.port                            = jobNode->slaveHost.port;
  newJobNode->slaveHost.forceSSL                        = jobNode->slaveHost.forceSSL;
  newJobNode->jobType                                   = jobNode->jobType;

  newJobNode->lastScheduleCheckDateTime                 = 0LL;

  newJobNode->ftpPassword                               = NULL;
  newJobNode->sshPassword                               = NULL;
  newJobNode->cryptPassword                             = NULL;

  newJobNode->fileName                                  = String_duplicate(fileName);
  newJobNode->fileModified                              = 0LL;

  newJobNode->masterIO                                  = NULL;

  Connector_duplicate(&newJobNode->connectorInfo,&jobNode->connectorInfo);

  newJobNode->state                                     = JOB_STATE_NONE;
  newJobNode->slaveState                                = SLAVE_STATE_OFFLINE;

  newJobNode->scheduleUUID                              = String_new();
  newJobNode->scheduleCustomText                        = String_new();
  newJobNode->archiveType                               = ARCHIVE_TYPE_NORMAL;
  newJobNode->noStorage                                 = FALSE;
  newJobNode->dryRun                                    = FALSE;
  newJobNode->byName                                    = String_new();

  newJobNode->requestedAbortFlag                        = FALSE;
  newJobNode->abortedByInfo                             = String_new();
  newJobNode->requestedVolumeNumber                     = 0;
  newJobNode->volumeNumber                              = 0;
  newJobNode->volumeMessage                             = String_new();
  newJobNode->volumeUnloadFlag                          = FALSE;

  newJobNode->lastExecutedDateTime                      = 0LL;
  newJobNode->lastErrorMessage                          = String_new();
  newJobNode->executionCount.normal                     = 0L;
  newJobNode->executionCount.full                       = 0L;
  newJobNode->executionCount.incremental                = 0L;
  newJobNode->executionCount.differential               = 0L;
  newJobNode->executionCount.continuous                 = 0L;
  newJobNode->averageDuration.normal                    = 0LL;
  newJobNode->averageDuration.full                      = 0LL;
  newJobNode->averageDuration.incremental               = 0LL;
  newJobNode->averageDuration.differential              = 0LL;
  newJobNode->averageDuration.continuous                = 0LL;
  newJobNode->totalEntityCount                          = 0L;
  newJobNode->totalStorageCount                         = 0L;
  newJobNode->totalStorageSize                          = 0LL;
  newJobNode->totalEntryCount                           = 0L;
  newJobNode->totalEntrySize                            = 0LL;

  newJobNode->modifiedFlag                              = TRUE;

  initStatusInfo(&newJobNode->statusInfo);

  Misc_performanceFilterInit(&newJobNode->runningInfo.entriesPerSecondFilter,     10*60);
  Misc_performanceFilterInit(&newJobNode->runningInfo.bytesPerSecondFilter,       10*60);
  Misc_performanceFilterInit(&newJobNode->runningInfo.storageBytesPerSecondFilter,10*60);

  Job_resetRunningInfo(newJobNode);

  return newJobNode;
}

void Job_delete(JobNode *jobNode)
{
  assert(jobNode != NULL);

  freeJobNode(jobNode,NULL);
  LIST_DELETE_NODE(jobNode);
}

bool Job_isSomeRunning(void)
{
  SemaphoreLock semaphoreLock;
  const JobNode *jobNode;
  bool          runningFlag;

  runningFlag = FALSE;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    LIST_ITERATE(&jobList,jobNode)
    {
      if (Job_isRunning(jobNode->state))
      {
        runningFlag = TRUE;
        break;
      }
    }
  }

  return runningFlag;
}

JobNode *Job_find(ConstString name)
{
  JobNode *jobNode;

  assert(name != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  jobNode = LIST_FIND(&jobList,jobNode,String_equals(jobNode->name,name));

  return jobNode;
}

bool Job_exists(ConstString name)
{
  JobNode *jobNode;

  assert(Semaphore_isLocked(&jobList.lock));

  return LIST_CONTAINS(&jobList,jobNode,String_equals(jobNode->name,name));
}

JobNode *Job_findByUUID(ConstString uuid)
{
  JobNode *jobNode;

  assert(uuid != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  jobNode = LIST_FIND(&jobList,jobNode,String_equals(jobNode->job.uuid,uuid));

  return jobNode;
}

JobNode *Job_findByName(ConstString name)
{
  JobNode *jobNode;

  assert(uuid != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  jobNode = LIST_FIND(&jobList,jobNode,String_equals(jobNode->name,name));

  return jobNode;
}

ScheduleNode *Job_findScheduleByUUID(const JobNode *jobNode, ConstString scheduleUUID)
{
  ScheduleNode *scheduleNode;

  assert(jobNode != NULL);
  assert(scheduleUUID != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  if (jobNode != NULL)
  {
    scheduleNode = LIST_FIND(&jobNode->job.scheduleList,scheduleNode,String_equals(scheduleNode->uuid,scheduleUUID));
  }
  else
  {
    scheduleNode = NULL;
    LIST_ITERATEX(&jobList,jobNode,scheduleNode == NULL)
    {
      scheduleNode = LIST_FIND(&jobNode->job.scheduleList,scheduleNode,String_equals(scheduleNode->uuid,scheduleUUID));
    }
  }

  return scheduleNode;
}

void Job_listChanged(void)
{
}

void Job_includeExcludeChanged(JobNode *jobNode)
{
  const ScheduleNode *scheduleNode;

  assert(Semaphore_isLocked(&jobList.lock));

  // check if continuous schedule exists, update continuous notifies
  LIST_ITERATE(&jobNode->job.scheduleList,scheduleNode)
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

  LIST_ITERATE(&jobNode->job.mountList,mountNode)
  {
  }
}

void Job_scheduleChanged(const JobNode *jobNode)
{
  const ScheduleNode *scheduleNode;

  assert(Semaphore_isLocked(&jobList.lock));

  // check if continuous schedule exists, update continuous notifies
  LIST_ITERATE(&jobNode->job.scheduleList,scheduleNode)
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

//TODO: required?
Errors Job_writeScheduleInfo(JobNode *jobNode)
{
  String             fileName,pathName,baseName;
  FileHandle         fileHandle;
  Errors             error;
  ArchiveTypes       archiveType;
  uint64             lastExecutedDateTime;
  const ScheduleNode *scheduleNode;

  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

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

    // write file
    error = File_printLine(&fileHandle,"%lld",jobNode->lastExecutedDateTime);
    if (error != ERROR_NONE)
    {
      File_close(&fileHandle);
      String_delete(fileName);
      return error;
    }
    for (archiveType = ARCHIVE_TYPE_MIN; archiveType <= ARCHIVE_TYPE_MAX; archiveType++)
    {
      lastExecutedDateTime = 0LL;
      LIST_ITERATE(&jobNode->job.scheduleList,scheduleNode)
      {
        if ((scheduleNode->archiveType == archiveType) && (scheduleNode->lastExecutedDateTime > lastExecutedDateTime))
        {
          lastExecutedDateTime = scheduleNode->lastExecutedDateTime;
        }
      }
      if (lastExecutedDateTime > 0LL)
      {
        error = File_printLine(&fileHandle,"%lld %s",lastExecutedDateTime,Archive_archiveTypeToString(archiveType,"UNKNOWN"));
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

//TODO: required?
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

  // reset variables
  jobNode->lastExecutedDateTime = 0LL;

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

    // read file
    line = String_new();
    if (File_getLine(&fileHandle,line,NULL,NULL))
    {
      // parse
      if (String_parse(line,STRING_BEGIN,"%lld",NULL,&n))
      {
        jobNode->lastScheduleCheckDateTime = n;
        jobNode->lastExecutedDateTime      = n;
        LIST_ITERATE(&jobNode->job.scheduleList,scheduleNode)
        {
          scheduleNode->lastExecutedDateTime = n;
        }
      }
    }
    while (File_getLine(&fileHandle,line,NULL,NULL))
    {
      // parse
      if (String_parse(line,STRING_BEGIN,"%lld %64s",NULL,&n,s))
      {
        if (Archive_parseType(s,&archiveType,NULL))
        {
          LIST_ITERATE(&jobNode->job.scheduleList,scheduleNode)
          {
            if (scheduleNode->archiveType == archiveType)
            {
              scheduleNode->lastExecutedDateTime = n;
            }
          }
        }
      }
    }
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
  LIST_ITERATE(&jobNode->job.scheduleList,scheduleNode)
  {
    if (scheduleNode->deprecatedPersistenceFlag)
    {
      // map 0 -> all/forever
      minKeep = (scheduleNode->minKeep != 0) ? scheduleNode->minKeep : KEEP_ALL;
      maxKeep = (scheduleNode->maxKeep != 0) ? scheduleNode->maxKeep : KEEP_ALL;
      maxAge  = (scheduleNode->maxAge  != 0) ? scheduleNode->maxAge  : AGE_FOREVER;

      // find existing persistence node
      persistenceNode = LIST_FIND(&jobNode->job.persistenceList,
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
        insertPersistenceNode(&jobNode->job.persistenceList,persistenceNode);
      }
    }
  }

//TODO: remove
  // update "forever"-nodes
//  insertForeverPersistenceNodes(&jobNode->job.persistenceList);

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

  if (jobNode->fileName != NULL)
  {
    // init variables
    StringList_init(&jobLinesList);
    line = String_new();

    // read file
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
    if (!List_isEmpty(&jobNode->job.scheduleList))
    {
      StringList_insertCString(&jobLinesList,"",nextStringNode);
      LIST_ITERATE(&jobNode->job.scheduleList,scheduleNode)
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
    if (!List_isEmpty(&jobNode->job.persistenceList))
    {
      StringList_insertCString(&jobLinesList,"",nextStringNode);
      LIST_ITERATE(&jobNode->job.persistenceList,persistenceNode)
      {
        // insert new persistence sections
        String_format(line,"[persistence %s]",Archive_archiveTypeToString(persistenceNode->archiveType,"normal"));
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

    // write file
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
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  Errors        error;

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    LIST_ITERATE(&jobList,jobNode)
    {
      if (jobNode->modifiedFlag)
      {
        error = Job_write(jobNode);
        if (error != ERROR_NONE)
        {
          printWarning("Cannot update job '%s' (error: %s)\n",String_cString(jobNode->fileName),Error_getText(error));
        }
      }
    }
  }
}

bool Job_read(JobNode *jobNode)
{
  uint       i;
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
  String_clear(jobNode->slaveHost.name);
  jobNode->slaveHost.port = 0;
  jobNode->slaveHost.forceSSL = FALSE;
  String_clear(jobNode->job.archiveName);
  EntryList_clear(&jobNode->job.includeEntryList);
  PatternList_clear(&jobNode->job.excludePatternList);
  PatternList_clear(&jobNode->job.compressExcludePatternList);
  DeltaSourceList_clear(&jobNode->job.deltaSourceList);
  List_clear(&jobNode->job.scheduleList,CALLBACK((ListNodeFreeFunction)freeScheduleNode,NULL));
  List_clear(&jobNode->job.persistenceList,CALLBACK((ListNodeFreeFunction)freePersistenceNode,NULL));
  jobNode->job.persistenceList.lastModificationTimestamp = 0LL;
  jobNode->job.options.archiveType                    = ARCHIVE_TYPE_NORMAL;
  jobNode->job.options.archivePartSize                = 0LL;
  String_clear(jobNode->job.options.incrementalListFileName);
  jobNode->job.options.directoryStripCount            = DIRECTORY_STRIP_NONE;
  String_clear(jobNode->job.options.destination);
  jobNode->job.options.patternType                    = PATTERN_TYPE_GLOB;
  jobNode->job.options.compressAlgorithms.value.delta = COMPRESS_ALGORITHM_NONE;
  jobNode->job.options.compressAlgorithms.value.byte  = COMPRESS_ALGORITHM_NONE;
  jobNode->job.options.compressAlgorithms.isSet       = FALSE;
  for (i = 0; i < 4; i++)
  {
    jobNode->job.options.cryptAlgorithms.values[i] = CRYPT_ALGORITHM_NONE;
  }
  jobNode->job.options.cryptAlgorithms.isSet          = FALSE;
  #ifdef HAVE_GCRYPT
    jobNode->job.options.cryptType                    = CRYPT_TYPE_SYMMETRIC;
  #else /* not HAVE_GCRYPT */
    jobNode->job.options.cryptType                    = CRYPT_TYPE_NONE;
  #endif /* HAVE_GCRYPT */
  jobNode->job.options.cryptPasswordMode              = PASSWORD_MODE_DEFAULT;
  Password_clear(&jobNode->job.options.cryptPassword);
  clearKey(&jobNode->job.options.cryptPublicKey);
  clearKey(&jobNode->job.options.cryptPrivateKey);
  String_clear(jobNode->job.options.ftpServer.loginName);
  Password_clear(&jobNode->job.options.ftpServer.password);
  jobNode->job.options.sshServer.port                 = 0;
  String_clear(jobNode->job.options.sshServer.loginName);
  Password_clear(&jobNode->job.options.sshServer.password);
  clearKey(&jobNode->job.options.sshServer.publicKey);
  clearKey(&jobNode->job.options.sshServer.privateKey);
  String_clear(jobNode->job.options.preProcessScript);
  String_clear(jobNode->job.options.postProcessScript);
  jobNode->job.options.device.volumeSize              = 0LL;
  jobNode->job.options.waitFirstVolumeFlag            = FALSE;
  jobNode->job.options.errorCorrectionCodesFlag       = FALSE;
  jobNode->job.options.blankFlag                      = FALSE;
  jobNode->job.options.skipUnreadableFlag             = FALSE;
  jobNode->job.options.rawImagesFlag                  = FALSE;
  jobNode->job.options.archiveFileMode                = ARCHIVE_FILE_MODE_STOP;

  // open file
  error = File_open(&fileHandle,jobNode->fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    printError("Cannot open job file '%s' (error: %s)!\n",
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
          String_unquote(value,STRING_QUOTES);
          String_unescape(value,
                          STRING_ESCAPE_CHARACTER,
                          STRING_ESCAPE_CHARACTERS_MAP_TO,
                          STRING_ESCAPE_CHARACTERS_MAP_FROM,
                          STRING_ESCAPE_CHARACTER_MAP_LENGTH
                        );
          if (!ConfigValue_parse(String_cString(name),
                                 String_cString(value),
                                 JOB_CONFIG_VALUES,
                                 "schedule",
                                 stderr,"ERROR: ","Warning: ",
                                 scheduleNode
                                )
             )
          {
            printError("Unknown or invalid config value '%s' in section '%s' in %s, line %ld - skipped\n",
                       String_cString(name),
                       "schedule",
                       String_cString(jobNode->fileName),
                       lineNb
                      );
          }
        }
        else
        {
          printError("Syntax error in %s, line %ld: '%s' - skipped\n",
                     String_cString(jobNode->fileName),
                     lineNb,
                     String_cString(line)
                    );
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
      String_clear(scheduleNode->lastErrorMessage);
      scheduleNode->executionCount       = 0L;
      scheduleNode->averageDuration      = 0LL;
      scheduleNode->totalEntityCount     = 0L;
      scheduleNode->totalStorageCount    = 0L;
      scheduleNode->totalStorageSize     = 0LL;
      scheduleNode->totalEntryCount      = 0L;
      scheduleNode->totalEntrySize       = 0LL;
#ifndef WERROR
#warning TODO
#endif
#if 0
      if (indexHandle != NULL)
      {
        (void)Index_findUUID(indexHandle,
                             jobNode->job.uuid,
                             scheduleNode->uuid,
                             NULL,  // uuidId,
                             &scheduleNode->lastExecutedDateTime,
                             scheduleNode->lastErrorMessage,
                             (scheduleNode->archiveType == ARCHIVE_TYPE_NORMAL      ) ? &scheduleNode->executionCount  : NULL,
                             (scheduleNode->archiveType == ARCHIVE_TYPE_FULL        ) ? &scheduleNode->executionCount  : NULL,
                             (scheduleNode->archiveType == ARCHIVE_TYPE_INCREMENTAL ) ? &scheduleNode->executionCount  : NULL,
                             (scheduleNode->archiveType == ARCHIVE_TYPE_DIFFERENTIAL) ? &scheduleNode->executionCount  : NULL,
                             (scheduleNode->archiveType == ARCHIVE_TYPE_CONTINUOUS  ) ? &scheduleNode->executionCount  : NULL,
                             (scheduleNode->archiveType == ARCHIVE_TYPE_NORMAL      ) ? &scheduleNode->averageDuration : NULL,
                             (scheduleNode->archiveType == ARCHIVE_TYPE_FULL        ) ? &scheduleNode->averageDuration : NULL,
                             (scheduleNode->archiveType == ARCHIVE_TYPE_INCREMENTAL ) ? &scheduleNode->averageDuration : NULL,
                             (scheduleNode->archiveType == ARCHIVE_TYPE_DIFFERENTIAL) ? &scheduleNode->averageDuration : NULL,
                             (scheduleNode->archiveType == ARCHIVE_TYPE_CONTINUOUS  ) ? &scheduleNode->averageDuration : NULL,
                             &scheduleNode->totalEntityCount,
                             &scheduleNode->totalStorageCount,
                             &scheduleNode->totalStorageSize,
                             &scheduleNode->totalEntryCount,
                             &scheduleNode->totalEntrySize
                            );
      }
#endif

      // append to list (if not a duplicate)
      if (!LIST_CONTAINS(&jobNode->job.scheduleList,
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
        List_append(&jobNode->job.scheduleList,scheduleNode);
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
            String_unquote(value,STRING_QUOTES);
            String_unescape(value,
                            STRING_ESCAPE_CHARACTER,
                            STRING_ESCAPE_CHARACTERS_MAP_TO,
                            STRING_ESCAPE_CHARACTERS_MAP_FROM,
                            STRING_ESCAPE_CHARACTER_MAP_LENGTH
                          );
            if (!ConfigValue_parse(String_cString(name),
                                   String_cString(value),
                                   JOB_CONFIG_VALUES,
                                   "persistence",
                                   stderr,"ERROR: ","Warning: ",
                                   persistenceNode
                                  )
               )
            {
              printError("Unknown or invalid config value '%s' in section '%s' in %s, line %ld - skipped\n",
                         String_cString(name),
                         "persistence",
                         String_cString(jobNode->fileName),
                         lineNb
                        );
            }
          }
          else
          {
            printError("Syntax error in %s, line %ld: '%s' - skipped\n",
                       String_cString(jobNode->fileName),
                       lineNb,
                       String_cString(line)
                      );
          }
        }
        File_ungetLine(&fileHandle,line,&lineNb);

        // insert into persistence list (if not a duplicate)
        if (!LIST_CONTAINS(&jobNode->job.persistenceList,
                           existingPersistenceNode,
                              (existingPersistenceNode->archiveType == persistenceNode->archiveType)
                           && (existingPersistenceNode->minKeep     == persistenceNode->minKeep    )
                           && (existingPersistenceNode->maxKeep     == persistenceNode->maxKeep    )
                           && (existingPersistenceNode->maxAge      == persistenceNode->maxAge     )
                          )
           )
        {
          // insert into persistence list
          insertPersistenceNode(&jobNode->job.persistenceList,persistenceNode);
        }
        else
        {
          // duplicate -> discard
          deletePersistenceNode(persistenceNode);
        }
      }
      else
      {
        printError("Unknown archive type '%s' in section '%s' in %s, line %ld - skipped\n",
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
      String_unquote(value,STRING_QUOTES);
      String_unescape(value,
                      STRING_ESCAPE_CHARACTER,
                      STRING_ESCAPE_CHARACTERS_MAP_TO,
                      STRING_ESCAPE_CHARACTERS_MAP_FROM,
                      STRING_ESCAPE_CHARACTER_MAP_LENGTH
                    );
      if (!ConfigValue_parse(String_cString(name),
                             String_cString(value),
                             JOB_CONFIG_VALUES,
                             NULL, // sectionName
                             stderr,"ERROR: ","Warning: ",
                             jobNode
                            )
         )
      {
        printError("Unknown or invalid config value '%s' in %s, line %ld - skipped\n",
                   String_cString(name),
                   String_cString(jobNode->fileName),
                   lineNb
                  );
      }
    }
    else
    {
      printError("Syntax error in %s, line %ld: '%s' - skipped\n",
                 String_cString(jobNode->fileName),
                 lineNb,
                 String_cString(line)
                );
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

  // get job info (if possible)
  String_clear(jobNode->lastErrorMessage);
  jobNode->executionCount.normal        = 0L;
  jobNode->executionCount.full          = 0L;
  jobNode->executionCount.incremental   = 0L;
  jobNode->executionCount.differential  = 0L;
  jobNode->executionCount.continuous    = 0L;
  jobNode->averageDuration.normal       = 0LL;
  jobNode->averageDuration.full         = 0LL;
  jobNode->averageDuration.incremental  = 0LL;
  jobNode->averageDuration.differential = 0LL;
  jobNode->averageDuration.continuous   = 0LL;
  jobNode->totalEntityCount             = 0L;
  jobNode->totalStorageCount            = 0L;
  jobNode->totalStorageSize             = 0LL;
  jobNode->totalEntryCount              = 0L;
  jobNode->totalEntrySize               = 0LL;
#ifndef WERROR
#warning TODO
#endif
#if 0
  if (indexHandle != NULL)
  {
    (void)Index_findUUID(indexHandle,
                         jobNode->job.uuid,
                         NULL,  // scheduleUUID,
                         NULL,  // uuidId,
                         NULL,  // lastExecutedDateTime
                         jobNode->lastErrorMessage,
                         &jobNode->executionCount.normal,
                         &jobNode->executionCount.full,
                         &jobNode->executionCount.incremental,
                         &jobNode->executionCount.differential,
                         &jobNode->executionCount.continuous,
                         &jobNode->averageDuration.normal,
                         &jobNode->averageDuration.full,
                         &jobNode->averageDuration.incremental,
                         &jobNode->averageDuration.differential,
                         &jobNode->averageDuration.continuous,
                         &jobNode->totalEntityCount,
                         &jobNode->totalStorageCount,
                         &jobNode->totalStorageSize,
                         &jobNode->totalEntryCount,
                         &jobNode->totalEntrySize
                        );
  }
#endif

  // read schedule info
  (void)Job_readScheduleInfo(jobNode);

  // reset job modified
  jobNode->modifiedFlag = FALSE;

  return TRUE;
}

Errors Job_rereadAll(ConstString      jobsDirectory,
                     const JobOptions *defaultJobOptions
                    )
{
  Errors              error;
  DirectoryListHandle directoryListHandle;
  String              fileName;
  String              baseName;
  SemaphoreLock       semaphoreLock;
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
      SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
      {
        // find/create job
        jobNode = Job_find(baseName);
        if (jobNode == NULL)
        {
          // create new job
          jobNode = Job_new(JOB_TYPE_CREATE,
                            baseName,
                            NULL, // jobUUID
                            fileName,
                            defaultJobOptions
                           );
          assert(jobNode != NULL);
          List_append(&jobList,jobNode);

          // notify about changes
          Job_listChanged();
        }

        if (   !Job_isActive(jobNode->state)
            && (File_getFileTimeModified(fileName) > jobNode->fileModified)
           )
        {
          // read job
          Job_read(jobNode);

          // notify about changes
          Job_includeExcludeChanged(jobNode);
          Job_mountChanged(jobNode);
          Job_scheduleChanged(jobNode);
        }
      }
    }
  }
  String_delete(baseName);
  File_closeDirectoryList(&directoryListHandle);

  // remove not existing jobs
  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    jobNode = jobList.head;
    while (jobNode != NULL)
    {
      if (jobNode->state == JOB_STATE_NONE)
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
          jobNode = List_removeAndFree(&jobList,jobNode,CALLBACK((ListNodeFreeFunction)freeJobNode,NULL));

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
  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    jobNode1 = jobList.head;
    while (jobNode1 != NULL)
    {
      jobNode2 = jobNode1->next;
      while (jobNode2 != NULL)
      {
        if (String_equals(jobNode1->job.uuid,jobNode2->job.uuid))
        {
          printWarning("Duplicate UUID in jobs '%s' and '%s'!\n",String_cString(jobNode1->name),String_cString(jobNode2->name));
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

void Job_trigger(JobNode      *jobNode,
                 ArchiveTypes archiveType,
                 ConstString  scheduleUUID,
                 ConstString  scheduleCustomText,
                 bool         noStorage,
                 bool         dryRun,
                 uint64       startDateTime,
                 const char   *byName
                )
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  // set job state
  jobNode->state                 = JOB_STATE_WAITING;
  String_set(jobNode->scheduleUUID,scheduleUUID);
  String_set(jobNode->scheduleCustomText,scheduleCustomText);
  jobNode->archiveType           = archiveType;
  jobNode->noStorage             = noStorage;
  jobNode->dryRun                = dryRun;
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
  jobNode->state             = JOB_STATE_RUNNING;
  jobNode->runningInfo.error = ERROR_NONE;
  Semaphore_signalModified(&jobList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);

  // increment active counter
  jobList.activeCount++;
}

void Job_end(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  // clear passwords
  if (jobNode->cryptPassword != NULL)
  {
    Password_delete(jobNode->cryptPassword);
    jobNode->cryptPassword = NULL;
  }
  if (jobNode->cryptPassword != NULL)
  {
    Password_delete(jobNode->sshPassword);
    jobNode->sshPassword = NULL;
  }
  if (jobNode->cryptPassword != NULL)
  {
    Password_delete(jobNode->ftpPassword);
    jobNode->ftpPassword = NULL;
  }

  // clear schedule
  String_clear(jobNode->scheduleUUID);
  String_clear(jobNode->scheduleCustomText);

  // set state
  if      (jobNode->requestedAbortFlag)
  {
    jobNode->state = JOB_STATE_ABORTED;
  }
  else if (jobNode->runningInfo.error != ERROR_NONE)
  {
    jobNode->state = JOB_STATE_ERROR;
  }
  else
  {
    jobNode->state = JOB_STATE_DONE;
  }
  Semaphore_signalModified(&jobList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);

  // decrement active counter
  assert(jobList.activeCount > 0);
  jobList.activeCount--;
}

void Job_abort(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  if      (Job_isRunning(jobNode->state))
  {
    // request abort job
    jobNode->requestedAbortFlag = TRUE;
    Semaphore_signalModified(&jobList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);

    if (Job_isLocal(jobNode))
    {
      // wait until local job terminated
      while (Job_isRunning(jobNode->state))
      {
        Semaphore_waitModified(&jobList.lock,LOCK_TIMEOUT);
      }
    }
    else
    {
      // abort slave job
      jobNode->runningInfo.error = Connector_jobAbort(&jobNode->connectorInfo,
                                                      jobNode->job.uuid
                                                     );
    }
  }
  else if (Job_isActive(jobNode->state))
  {
    jobNode->state = JOB_STATE_NONE;
  }

  // store schedule info
  Job_writeScheduleInfo(jobNode);
}

void Job_reset(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  if (!Job_isActive(jobNode->state))
  {
    jobNode->state = JOB_STATE_NONE;
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
  jobOptions->uuid                            = String_new();
  jobOptions->archiveType                     = ARCHIVE_TYPE_NORMAL;
  jobOptions->archivePartSize                 = 0LL;
  jobOptions->incrementalListFileName         = String_new();
  jobOptions->directoryStripCount             = DIRECTORY_STRIP_NONE;
  jobOptions->destination                     = String_new();
  jobOptions->owner.userId                    = FILE_DEFAULT_USER_ID;
  jobOptions->owner.groupId                   = FILE_DEFAULT_GROUP_ID;
  jobOptions->patternType                     = PATTERN_TYPE_GLOB;
  jobOptions->compressAlgorithms.value.delta  = COMPRESS_ALGORITHM_NONE;
  jobOptions->compressAlgorithms.value.byte   = COMPRESS_ALGORITHM_NONE;
  jobOptions->compressAlgorithms.isSet        = FALSE;
  for (i = 0; i < 4; i++)
  {
    jobOptions->cryptAlgorithms.values[i] = CRYPT_ALGORITHM_NONE;
  }
  jobOptions->cryptAlgorithms.isSet           = FALSE;
  #ifdef HAVE_GCRYPT
    jobOptions->cryptType                     = CRYPT_TYPE_SYMMETRIC;
  #else /* not HAVE_GCRYPT */
    jobOptions->cryptType                     = CRYPT_TYPE_NONE;
  #endif /* HAVE_GCRYPT */
  jobOptions->cryptPasswordMode               = PASSWORD_MODE_DEFAULT;
  Password_init(&jobOptions->cryptPassword);
  initKey(&jobOptions->cryptPublicKey);
  initKey(&jobOptions->cryptPrivateKey);

  jobOptions->preProcessScript                = NULL;
  jobOptions->postProcessScript               = NULL;

  Password_init(&jobOptions->ftpServer.password);

  Password_init(&jobOptions->sshServer.password);
  initKey(&jobOptions->sshServer.publicKey);
  initKey(&jobOptions->sshServer.privateKey);

  Password_init(&jobOptions->webDAVServer.password);
  initKey(&jobOptions->webDAVServer.publicKey);
  initKey(&jobOptions->webDAVServer.privateKey);

  jobOptions->maxFragmentSize                 = 0LL;
  jobOptions->maxStorageSize                  = 0LL;
  jobOptions->volumeSize                      = 0LL;
  jobOptions->comment.value                   = String_new();
  jobOptions->comment.isSet                   = FALSE;
  jobOptions->skipUnreadableFlag              = TRUE;
  jobOptions->forceDeltaCompressionFlag       = FALSE;
  jobOptions->ignoreNoDumpAttributeFlag       = FALSE;
  jobOptions->archiveFileMode                 = ARCHIVE_FILE_MODE_STOP;
  jobOptions->restoreEntryMode                = RESTORE_ENTRY_MODE_STOP;
  jobOptions->errorCorrectionCodesFlag        = FALSE;
  jobOptions->blankFlag                       = FALSE;
  jobOptions->alwaysCreateImageFlag           = FALSE;
  jobOptions->waitFirstVolumeFlag             = FALSE;
  jobOptions->rawImagesFlag                   = FALSE;
  jobOptions->noFragmentsCheckFlag            = FALSE;
  jobOptions->noIndexDatabaseFlag             = FALSE;
  jobOptions->skipVerifySignaturesFlag        = FALSE;
  jobOptions->noStorageFlag                   = FALSE;
  jobOptions->noBAROnMediumFlag               = FALSE;
  jobOptions->noStopOnErrorFlag               = FALSE;

  DEBUG_ADD_RESOURCE_TRACE(jobOptions,JobOptions);
}

void Job_duplicateOptions(JobOptions *jobOptions, const JobOptions *fromJobOptions)
{
  DEBUG_CHECK_RESOURCE_TRACE(fromJobOptions);

  Job_initOptions(jobOptions);
  Job_setOptions(jobOptions,fromJobOptions);
}

void Job_setOptions(JobOptions *jobOptions, const JobOptions *fromJobOptions)
{
  uint i;

  assert(jobOptions != NULL);
  assert(fromJobOptions != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(jobOptions);
  DEBUG_CHECK_RESOURCE_TRACE(fromJobOptions);

  String_set(jobOptions->uuid,                                fromJobOptions->uuid);
  jobOptions->archiveType                                   = fromJobOptions->archiveType;
  jobOptions->archivePartSize                               = fromJobOptions->archivePartSize;
  String_set(jobOptions->incrementalListFileName,             fromJobOptions->incrementalListFileName);
  jobOptions->directoryStripCount                           = fromJobOptions->directoryStripCount;
  String_set(jobOptions->destination,                         fromJobOptions->destination);
  jobOptions->owner.userId                                  = fromJobOptions->owner.userId;
  jobOptions->owner.groupId                                 = fromJobOptions->owner.groupId;
  jobOptions->patternType                                   = fromJobOptions->patternType;
  jobOptions->compressAlgorithms.value.delta                = fromJobOptions->compressAlgorithms.value.delta;
  jobOptions->compressAlgorithms.value.byte                 = fromJobOptions->compressAlgorithms.value.byte;
  jobOptions->compressAlgorithms.isSet                      = FALSE;
  for (i = 0; i < 4; i++)
  {
    jobOptions->cryptAlgorithms.values[i] = fromJobOptions->cryptAlgorithms.values[i];
  }
  jobOptions->cryptAlgorithms.isSet                         = FALSE;
  jobOptions->cryptType                                     = fromJobOptions->cryptType;
  jobOptions->cryptPasswordMode                             = fromJobOptions->cryptPasswordMode;
  Password_set(&jobOptions->cryptPassword,                    &fromJobOptions->cryptPassword);
  setKey(&jobOptions->cryptPublicKey,fromJobOptions->cryptPublicKey.data,fromJobOptions->cryptPublicKey.length);
  setKey(&jobOptions->cryptPrivateKey,fromJobOptions->cryptPrivateKey.data,fromJobOptions->cryptPrivateKey.length);
  String_set(jobOptions->preProcessScript,                    fromJobOptions->preProcessScript);
  String_set(jobOptions->postProcessScript,                   fromJobOptions->postProcessScript);

  String_set(jobOptions->ftpServer.loginName,                 fromJobOptions->ftpServer.loginName);
  Password_set(&jobOptions->ftpServer.password,               &fromJobOptions->ftpServer.password);

  String_set(jobOptions->sshServer.loginName,                 fromJobOptions->sshServer.loginName);
  Password_set(&jobOptions->sshServer.password,               &fromJobOptions->sshServer.password);
  setKey(&jobOptions->sshServer.publicKey,fromJobOptions->sshServer.publicKey.data,fromJobOptions->sshServer.publicKey.length);
  setKey(&jobOptions->sshServer.privateKey,fromJobOptions->sshServer.privateKey.data,fromJobOptions->sshServer.privateKey.length);

  String_set(jobOptions->webDAVServer.loginName,              fromJobOptions->webDAVServer.loginName);
  Password_set(&jobOptions->webDAVServer.password,            &fromJobOptions->webDAVServer.password);
  setKey(&jobOptions->webDAVServer.publicKey,fromJobOptions->webDAVServer.publicKey.data,fromJobOptions->webDAVServer.publicKey.length);
  setKey(&jobOptions->webDAVServer.privateKey,fromJobOptions->webDAVServer.privateKey.data,fromJobOptions->webDAVServer.privateKey.length);

  jobOptions->maxFragmentSize                               = fromJobOptions->maxFragmentSize;
  jobOptions->maxStorageSize                                = fromJobOptions->maxStorageSize;
  jobOptions->volumeSize                                    = fromJobOptions->volumeSize;
  String_set(jobOptions->comment.value,                       fromJobOptions->comment.value);
  jobOptions->comment.isSet                                 = FALSE;
  jobOptions->skipUnreadableFlag                            = fromJobOptions->skipUnreadableFlag;
  jobOptions->forceDeltaCompressionFlag                     = fromJobOptions->forceDeltaCompressionFlag;
  jobOptions->ignoreNoDumpAttributeFlag                     = fromJobOptions->ignoreNoDumpAttributeFlag;
  jobOptions->archiveFileMode                               = fromJobOptions->archiveFileMode;
  jobOptions->restoreEntryMode                              = fromJobOptions->restoreEntryMode;
  jobOptions->errorCorrectionCodesFlag                      = fromJobOptions->errorCorrectionCodesFlag;
  jobOptions->blankFlag                                     = fromJobOptions->blankFlag;
  jobOptions->alwaysCreateImageFlag                         = fromJobOptions->alwaysCreateImageFlag;
  jobOptions->waitFirstVolumeFlag                           = fromJobOptions->waitFirstVolumeFlag;
  jobOptions->rawImagesFlag                                 = fromJobOptions->rawImagesFlag;
  jobOptions->noFragmentsCheckFlag                          = fromJobOptions->noFragmentsCheckFlag;
  jobOptions->noIndexDatabaseFlag                           = fromJobOptions->noIndexDatabaseFlag;
  jobOptions->skipVerifySignaturesFlag                      = fromJobOptions->skipVerifySignaturesFlag;
  jobOptions->noStorageFlag                                 = fromJobOptions->noStorageFlag;
  jobOptions->noBAROnMediumFlag                             = fromJobOptions->noBAROnMediumFlag;
  jobOptions->noStopOnErrorFlag                             = fromJobOptions->noStopOnErrorFlag;

  String_set(jobOptions->opticalDisk.requestVolumeCommand,    fromJobOptions->opticalDisk.requestVolumeCommand);
  String_set(jobOptions->opticalDisk.unloadVolumeCommand,     fromJobOptions->opticalDisk.unloadVolumeCommand);
  String_set(jobOptions->opticalDisk.imagePreProcessCommand,  fromJobOptions->opticalDisk.imagePreProcessCommand);
  String_set(jobOptions->opticalDisk.imagePostProcessCommand, fromJobOptions->opticalDisk.imagePostProcessCommand);
  String_set(jobOptions->opticalDisk.imageCommand,            fromJobOptions->opticalDisk.imageCommand);
  String_set(jobOptions->opticalDisk.eccPreProcessCommand,    fromJobOptions->opticalDisk.eccPreProcessCommand);
  String_set(jobOptions->opticalDisk.eccPostProcessCommand,   fromJobOptions->opticalDisk.eccPostProcessCommand);
  String_set(jobOptions->opticalDisk.eccCommand,              fromJobOptions->opticalDisk.eccCommand);
  String_set(jobOptions->opticalDisk.blankCommand,            fromJobOptions->opticalDisk.blankCommand);
  String_set(jobOptions->opticalDisk.writePreProcessCommand,  fromJobOptions->opticalDisk.writePreProcessCommand);
  String_set(jobOptions->opticalDisk.writePostProcessCommand, fromJobOptions->opticalDisk.writePostProcessCommand);
  String_set(jobOptions->opticalDisk.writeCommand,            fromJobOptions->opticalDisk.writeCommand);

  String_set(jobOptions->deviceName,                          fromJobOptions->deviceName);
  String_set(jobOptions->device.requestVolumeCommand,         fromJobOptions->device.requestVolumeCommand);
  String_set(jobOptions->device.unloadVolumeCommand,          fromJobOptions->device.unloadVolumeCommand);
  String_set(jobOptions->device.imagePreProcessCommand,       fromJobOptions->device.imagePreProcessCommand);
  String_set(jobOptions->device.imagePostProcessCommand,      fromJobOptions->device.imagePostProcessCommand);
  String_set(jobOptions->device.imageCommand,                 fromJobOptions->device.imageCommand);
  String_set(jobOptions->device.eccPreProcessCommand,         fromJobOptions->device.eccPreProcessCommand);
  String_set(jobOptions->device.eccPostProcessCommand,        fromJobOptions->device.eccPostProcessCommand);
  String_set(jobOptions->device.eccCommand,                   fromJobOptions->device.eccCommand);
  String_set(jobOptions->device.blankCommand,                 fromJobOptions->device.blankCommand);
  String_set(jobOptions->device.writePreProcessCommand,       fromJobOptions->device.writePreProcessCommand);
  String_set(jobOptions->device.writePostProcessCommand,      fromJobOptions->device.writePostProcessCommand);
  String_set(jobOptions->device.writeCommand,                 fromJobOptions->device.writeCommand);
}

void Job_doneOptions(JobOptions *jobOptions)
{
  assert(jobOptions != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(jobOptions,JobOptions);

  String_delete(jobOptions->device.writeCommand);
  String_delete(jobOptions->device.writePostProcessCommand);
  String_delete(jobOptions->device.writePreProcessCommand);
  String_delete(jobOptions->device.blankCommand);
  String_delete(jobOptions->device.eccCommand);
  String_delete(jobOptions->device.eccPostProcessCommand);
  String_delete(jobOptions->device.eccPreProcessCommand);
  String_delete(jobOptions->device.imageCommand);
  String_delete(jobOptions->device.imagePostProcessCommand);
  String_delete(jobOptions->device.imagePreProcessCommand);
  String_delete(jobOptions->device.unloadVolumeCommand);
  String_delete(jobOptions->device.requestVolumeCommand);
  String_delete(jobOptions->deviceName);

  String_delete(jobOptions->opticalDisk.writeCommand);
  String_delete(jobOptions->opticalDisk.writePostProcessCommand);
  String_delete(jobOptions->opticalDisk.writePreProcessCommand);
  String_delete(jobOptions->opticalDisk.blankCommand);
  String_delete(jobOptions->opticalDisk.eccCommand);
  String_delete(jobOptions->opticalDisk.eccPostProcessCommand);
  String_delete(jobOptions->opticalDisk.eccPreProcessCommand);
  String_delete(jobOptions->opticalDisk.imageCommand);
  String_delete(jobOptions->opticalDisk.imagePostProcessCommand);
  String_delete(jobOptions->opticalDisk.imagePreProcessCommand);
  String_delete(jobOptions->opticalDisk.unloadVolumeCommand);
  String_delete(jobOptions->opticalDisk.requestVolumeCommand);

  doneKey(&jobOptions->webDAVServer.privateKey);
  doneKey(&jobOptions->webDAVServer.publicKey);
  Password_done(&jobOptions->webDAVServer.password);
  String_delete(jobOptions->webDAVServer.loginName);

  doneKey(&jobOptions->sshServer.privateKey);
  doneKey(&jobOptions->sshServer.publicKey);
  Password_done(&jobOptions->sshServer.password);
  String_delete(jobOptions->sshServer.loginName);

  Password_done(&jobOptions->ftpServer.password);
  String_delete(jobOptions->ftpServer.loginName);

  String_delete(jobOptions->comment.value);

  String_delete(jobOptions->postProcessScript);
  String_delete(jobOptions->preProcessScript);

  doneKey(&jobOptions->cryptPrivateKey);
  doneKey(&jobOptions->cryptPublicKey);
  Password_done(&jobOptions->cryptPassword);

  String_delete(jobOptions->destination);
  String_delete(jobOptions->incrementalListFileName);
  String_delete(jobOptions->uuid);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
