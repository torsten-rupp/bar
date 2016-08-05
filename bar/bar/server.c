/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver server
* Systems: all
*
\***********************************************************************/

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
#include "misc.h"

#include "passwords.h"
#include "network.h"
#include "files.h"
#include "devices.h"
#include "patterns.h"
#include "entrylists.h"
#include "patternlists.h"
#include "archive.h"
#include "storage.h"
#include "index.h"
#include "continuous.h"
#include "bar.h"

#include "commands_create.h"
#include "commands_restore.h"
#include "remote.h"

#include "server.h"

/****************** Conditional compilation switches *******************/

#define _SERVER_DEBUG
#define _NO_SESSION_ID
#define _SIMULATOR

/***************************** Constants *******************************/

#define SESSION_KEY_SIZE                    1024    // number of session key bits

#define MAX_NETWORK_CLIENT_THREADS          3       // number of threads for a client
#define LOCK_TIMEOUT                        (10*60*1000)  // general lock timeout [ms]

#define AUTHORIZATION_PENALITY_TIME         500     // delay processing by failCount^2*n [ms]
#define MAX_AUTHORIZATION_HISTORY_KEEP_TIME 30000   // max. time to keep entries in authorization fail history [ms]
#define MAX_AUTHORIZATION_FAIL_HISTORY      64      // max. length of history of authorization fail clients

// sleep times [s]
//TODO
//#define SLEEP_TIME_REMOTE_THREAD            ( 5*60)
#define SLEEP_TIME_REMOTE_THREAD            ( 20)
#define SLEEP_TIME_SCHEDULER_THREAD         ( 1*60)
#define SLEEP_TIME_PAUSE_THREAD             ( 1*60)
#define SLEEP_TIME_INDEX_THREAD             ( 1*60)
#define SLEEP_TIME_AUTO_INDEX_UPDATE_THREAD (10*60)
#define SLEEP_TIME_PURGE_EXPIRED_THREAD     (10*60)

/***************************** Datatypes *******************************/

// server states
typedef enum
{
  SERVER_STATE_RUNNING,
  SERVER_STATE_PAUSE,
  SERVER_STATE_SUSPENDED,
} ServerStates;

// schedule
typedef struct
{
  int year;                                    // year or SCHEDULE_ANY
  int month;                                   // month or SCHEDULE_ANY
  int day;                                     // day or SCHEDULE_ANY
} ScheduleDate;
typedef WeekDaySet ScheduleWeekDaySet;
typedef struct
{
  int hour;                                    // hour or SCHEDULE_ANY
  int minute;                                  // minute or SCHEDULE_ANY
} ScheduleTime;
typedef struct ScheduleNode
{
  LIST_NODE_HEADER(struct ScheduleNode);

  String             uuid;                     // unique id
  String             parentUUID;               // unique parent id or NULL
  ScheduleDate       date;
  ScheduleWeekDaySet weekDaySet;
  ScheduleTime       time;
  ArchiveTypes       archiveType;              // archive type to create
  uint               interval;                 // continuous interval [min]
  String             customText;               // custom text
  uint               minKeep,maxKeep;          // min./max keep count
  uint               maxAge;                   // max. age [days]
  bool               noStorage;                // TRUE to skip storage, only create incremental data file
  bool               enabled;                  // TRUE iff enabled
  String             preProcessScript;         // script to execute before start of job
  String             postProcessScript;        // script to execute after after termination of job
} ScheduleNode;

typedef struct
{
  LIST_HEADER(ScheduleNode);
} ScheduleList;

// job type
typedef enum
{
  JOB_TYPE_CREATE,
  JOB_TYPE_RESTORE,
} JobTypes;

// job states
typedef enum
{
  JOB_STATE_NONE,
  JOB_STATE_WAITING,
  JOB_STATE_RUNNING,
  JOB_STATE_REQUEST_FTP_PASSWORD,
  JOB_STATE_REQUEST_SSH_PASSWORD,
  JOB_STATE_REQUEST_WEBDAV_PASSWORD,
  JOB_STATE_REQUEST_CRYPT_PASSWORD,
  JOB_STATE_REQUEST_VOLUME,
  JOB_STATE_DONE,
  JOB_STATE_ERROR,
  JOB_STATE_ABORTED
} JobStates;

// job node
typedef struct JobNode
{
  LIST_NODE_HEADER(struct JobNode);

  // job file
  String          fileName;                             // file name
  uint64          fileModified;                         // file modified date/time (timestamp)

  // job config
  String          uuid;                                 // unique id
  JobTypes        jobType;                              // job type: backup, restore
  String          name;                                 // name of job
  RemoteHost      remoteHost;                           // remote host
  String          archiveName;                          // archive name
  EntryList       includeEntryList;                     // included entries
  String          includeFileCommand;                   // include file command
  String          includeImageCommand;                  // include image command
  PatternList     excludePatternList;                   // excluded entry patterns
  String          excludeCommand;                       // exclude entries command
  MountList       mountList;                            // mount list
  PatternList     compressExcludePatternList;           // excluded compression patterns
  DeltaSourceList deltaSourceList;                      // delta sources
  ScheduleList    scheduleList;                         // schedule list
  JobOptions      jobOptions;                           // options for job

  // modified info
  bool            modifiedFlag;                         // TRUE iff job config modified
  bool            scheduleModifiedFlag;
//  uint64          lastIncludeExcludeModified;
//  uint64          lastScheduleModified;

  // schedule info
  uint64          lastExecutedDateTime;                 // last execution date/time (timestamp)
  uint64          lastCheckDateTime;                    // last check date/time (timestamp)

  // comment
  String          comment;                              // comment

  // job passwords
  Password        *ftpPassword;                         // FTP password if password mode is 'ask'
  Password        *sshPassword;                         // SSH password if password mode is 'ask'
  Password        *cryptPassword;                       // crypt password if password mode is 'ask'

  // job running state
  uint64          timestamp;                            // timestamp of last change

  String          master;                               // master initiate job or NULL

  JobStates       state;                                // current state of job
  ArchiveTypes    archiveType;                          // archive type to create
  bool            dryRun;                               // TRUE iff dry-run (no storage, no index update)
  struct                                                // schedule data which triggered job
  {
    String        uuid;                                 // UUID or empty
    String        customText;                           // custom text or empty
    bool          noStorage;                            // TRUE to skip storage, only create incremental data file
  } schedule;
  bool            requestedAbortFlag;                   // request abort
  uint            requestedVolumeNumber;                // requested volume number
  uint            volumeNumber;                         // loaded volume number
  bool            volumeUnloadFlag;                     // TRUE to unload volume

  // running info
  struct
  {
    Errors            error;                            // error code
    ulong             estimatedRestTime;                // estimated rest running time [s]
    ulong             doneCount;                        // number of processed entries
    uint64            doneSize;                         // sum of processed bytes
    ulong             totalEntryCount;                  // total number of entries
    uint64            totalEntrySize;                   // total size of entries [bytes]
    bool              collectTotalSumDone;              // TRUE if sum of total entries are collected
    ulong             skippedEntryCount;                // number of skipped entries
    uint64            skippedEntrySize;                 // sum of skippped bytes
    ulong             errorEntryCount;                  // number of entries with errors
    uint64            errorEntrySize;                   // sum of bytes of files with errors
    double            entriesPerSecond;                 // average processed entries per second last 10s
    double            bytesPerSecond;                   // average processed bytes per second last 10s
    double            storageBytesPerSecond;            // average processed storage bytes per second last 10s
    uint64            archiveSize;                      // number of bytes stored in archive
    double            compressionRatio;                 // compression ratio: saved "space" [%]
    String            entryName;                        // current entry name
    uint64            entryDoneSize;                    // current entry bytes done
    uint64            entryTotalSize;                   // current entry bytes total
    String            storageName;                      // current storage name
    uint64            storageDoneSize;                  // current storage bytes done
    uint64            storageTotalSize;                 // current storage bytes total
    uint              volumeNumber;                     // current volume number
    double            volumeProgress;                   // current volume progress
    String            message;                          // message text

    PerformanceFilter entriesPerSecondFilter;
    PerformanceFilter bytesPerSecondFilter;
    PerformanceFilter storageBytesPerSecondFilter;
  } runningInfo;
} JobNode;

// list with jobs
typedef struct
{
  LIST_HEADER(JobNode);

  Semaphore lock;
  uint      activeCount;
} JobList;

// directory info node
typedef struct DirectoryInfoNode
{
  LIST_NODE_HEADER(struct DirectoryInfoNode);

  String              pathName;
  uint64              fileCount;
  uint64              totalFileSize;
  StringList          pathNameList;
  bool                directoryOpenFlag;
  DirectoryListHandle directoryListHandle;
} DirectoryInfoNode;

// directory info list
typedef struct
{
  LIST_HEADER(DirectoryInfoNode);
} DirectoryInfoList;

typedef struct IndexCryptPasswordNode
{
  LIST_NODE_HEADER(struct IndexCryptPasswordNode);

  Password *cryptPassword;
  String   cryptPrivateKeyFileName;
} IndexCryptPasswordNode;

// list with index decrypt passwords
typedef struct
{
  LIST_HEADER(IndexCryptPasswordNode);
} IndexCryptPasswordList;

// client types
typedef enum
{
  CLIENT_TYPE_NONE,
  CLIENT_TYPE_BATCH,
  CLIENT_TYPE_NETWORK,
} ClientTypes;

// authorization states
typedef enum
{
  AUTHORIZATION_STATE_WAITING,
  AUTHORIZATION_STATE_OK,
  AUTHORIZATION_STATE_FAIL,
} AuthorizationStates;

// authorization fail node
typedef struct AuthorizationFailNode
{
  LIST_NODE_HEADER(struct AuthorizationFailNode);

  String clientName;
  uint   count;
  uint64 lastTimestamp;
} AuthorizationFailNode;

// authorization fail list
typedef struct
{
  LIST_HEADER(AuthorizationFailNode);
} AuthorizationFailList;

// client info
typedef struct
{
  ClientTypes           type;

  SessionId             sessionId;
  CryptKey              publicKey,secretKey;
  AuthorizationStates   authorizationState;
  AuthorizationFailNode *authorizationFailNode;
  bool                  quitFlag;

  struct
  {
    Semaphore           lock;
    Errors              error;
    StringMap           resultMap;
  } action;

  uint                  abortCommandId;                    // command id to abort
//TODO:
bool abortFlag;

  union
  {
    // i/o via file
    struct
    {
      FileHandle   *fileHandle;
    } file;

    // i/o via network and separated processing thread
    struct
    {
      // connection
      String       name;
      uint         port;
      SocketHandle socketHandle;

      // threads
      Semaphore    writeLock;                              // write synchronization lock
      Thread       threads[MAX_NETWORK_CLIENT_THREADS];    // command processing threads
      MsgQueue     commandMsgQueue;                        // commands send by client
    } network;
  };

  EntryList           includeEntryList;
  PatternList         excludePatternList;
  MountList           mountList;
  PatternList         compressExcludePatternList;
  DeltaSourceList     deltaSourceList;
  JobOptions          jobOptions;
  DirectoryInfoList   directoryInfoList;

  Array               indexIdArray;                        // ids of uuid/entity/storage ids to list/restore
  Array               entryIdArray;                        // ids of entries to restore
} ClientInfo;

// client node
typedef struct ClientNode
{
  LIST_NODE_HEADER(struct ClientNode);

  ClientInfo clientInfo;
  String     commandString;
} ClientNode;

// client list
typedef struct
{
  LIST_HEADER(ClientNode);
} ClientList;

/***********************************************************************\
* Name   : ServerCommandFunction
* Purpose: server command function
* Input  : clientInfo  - client info
*          indexHandle - index handle or NULL
*          id          - command id
*          argumentMap - argument map
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*ServerCommandFunction)(ClientInfo      *clientInfo,
                                     IndexHandle     *indexHandle,
                                     uint            id,
                                     const StringMap argumentMap
                                    );

// server command message
typedef struct
{
  ServerCommandFunction serverCommandFunction;
  AuthorizationStates   authorizationState;
  uint                  id;
  StringMap             argumentMap;
} CommandMsg;

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

// handle deprecated configuration values
LOCAL bool configValueParseDeprecatedSchedule(void *userData, void *variable, const char *name, const char *value, char errorMessage[], uint errorMessageSize);

LOCAL const ConfigValue JOB_CONFIG_VALUES[] = CONFIG_VALUE_ARRAY
(
  CONFIG_STRUCT_VALUE_STRING    ("UUID",                    JobNode,uuid                                    ),
  CONFIG_STRUCT_VALUE_STRING    ("remote-host-name",        JobNode,remoteHost.name                         ),
  CONFIG_STRUCT_VALUE_INTEGER   ("remote-host-port",        JobNode,remoteHost.port,                        0,65535,NULL),
  CONFIG_STRUCT_VALUE_BOOLEAN   ("remote-host-force-ssl",   JobNode,remoteHost.forceSSL                     ),
  CONFIG_STRUCT_VALUE_STRING    ("archive-name",            JobNode,archiveName                             ),
  CONFIG_STRUCT_VALUE_SELECT    ("archive-type",            JobNode,jobOptions.archiveType,                 CONFIG_VALUE_ARCHIVE_TYPES),

  CONFIG_STRUCT_VALUE_STRING    ("incremental-list-file",   JobNode,jobOptions.incrementalListFileName      ),

  CONFIG_STRUCT_VALUE_INTEGER64 ("archive-part-size",       JobNode,jobOptions.archivePartSize,             0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),

  CONFIG_STRUCT_VALUE_INTEGER   ("directory-strip",         JobNode,jobOptions.directoryStripCount,         -1,MAX_INT,NULL),
  CONFIG_STRUCT_VALUE_STRING    ("destination",             JobNode,jobOptions.destination                  ),
  CONFIG_STRUCT_VALUE_SPECIAL   ("owner",                   JobNode,jobOptions.owner,                       configValueParseOwner,configValueFormatInitOwner,NULL,configValueFormatOwner,NULL),

  CONFIG_STRUCT_VALUE_SELECT    ("pattern-type",            JobNode,jobOptions.patternType,                 CONFIG_VALUE_PATTERN_TYPES),

  CONFIG_STRUCT_VALUE_SPECIAL   ("compress-algorithm",      JobNode,jobOptions.compressAlgorithms,          configValueParseCompressAlgorithms,configValueFormatInitCompressAlgorithms,configValueFormatDoneCompressAlgorithms,configValueFormatCompressAlgorithms,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL   ("compress-exclude",        JobNode,compressExcludePatternList,             configValueParsePattern,configValueFormatInitPattern,configValueFormatDonePattern,configValueFormatPattern,NULL),

  CONFIG_STRUCT_VALUE_SELECT    ("crypt-algorithm",         JobNode,jobOptions.cryptAlgorithm,              CONFIG_VALUE_CRYPT_ALGORITHMS),
  CONFIG_STRUCT_VALUE_SELECT    ("crypt-type",              JobNode,jobOptions.cryptType,                   CONFIG_VALUE_CRYPT_TYPES),
  CONFIG_STRUCT_VALUE_SELECT    ("crypt-password-mode",     JobNode,jobOptions.cryptPasswordMode,           CONFIG_VALUE_PASSWORD_MODES),
  CONFIG_STRUCT_VALUE_SPECIAL   ("crypt-password",          JobNode,jobOptions.cryptPassword,               configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
  CONFIG_STRUCT_VALUE_STRING    ("crypt-public-key",        JobNode,jobOptions.cryptPublicKeyFileName       ),

  CONFIG_STRUCT_VALUE_STRING    ("pre-command",             JobNode,jobOptions.preProcessScript             ),
  CONFIG_STRUCT_VALUE_STRING    ("post-command",            JobNode,jobOptions.postProcessScript            ),

  CONFIG_STRUCT_VALUE_STRING    ("ftp-login-name",          JobNode,jobOptions.ftpServer.loginName          ),
  CONFIG_STRUCT_VALUE_SPECIAL   ("ftp-password",            JobNode,jobOptions.ftpServer.password,          configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),

  CONFIG_STRUCT_VALUE_INTEGER   ("ssh-port",                JobNode,jobOptions.sshServer.port,              0,65535,NULL),
  CONFIG_STRUCT_VALUE_STRING    ("ssh-login-name",          JobNode,jobOptions.sshServer.loginName          ),
  CONFIG_STRUCT_VALUE_SPECIAL   ("ssh-password",            JobNode,jobOptions.sshServer.password,          configValueParsePassword,configValueFormatInitPassord,configValueFormatDonePassword,configValueFormatPassword,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL   ("ssh-public-key",          JobNode,jobOptions.sshServer.publicKey,         configValueParseKey,NULL,NULL,NULL,NULL),
//  CONFIG_STRUCT_VALUE_SPECIAL   ("ssh-public-key-data",     JobNode,jobOptions.sshServer.publicKey,         configValueParseKey,NULL,NULL,NULL,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL   ("ssh-private-key",         JobNode,jobOptions.sshServer.privateKey,        configValueParseKey,NULL,NULL,NULL,NULL),
//  CONFIG_STRUCT_VALUE_SPECIAL   ("ssh-private-key-data",    JobNode,jobOptions.sshServer.privateKey,        configValueParseKey,NULL,NULL,NULL,NULL),

  CONFIG_STRUCT_VALUE_SPECIAL   ("include-file",            JobNode,includeEntryList,                       configValueParseFileEntryPattern,configValueFormatInitEntryPattern,configValueFormatDoneEntryPattern,configValueFormatFileEntryPattern,NULL),
  CONFIG_STRUCT_VALUE_STRING    ("include-file-command",    JobNode,includeFileCommand                      ),
  CONFIG_STRUCT_VALUE_SPECIAL   ("include-image",           JobNode,includeEntryList,                       configValueParseImageEntryPattern,configValueFormatInitEntryPattern,configValueFormatDoneEntryPattern,configValueFormatImageEntryPattern,NULL),
  CONFIG_STRUCT_VALUE_STRING    ("include-image-command",   JobNode,includeImageCommand                     ),
  CONFIG_STRUCT_VALUE_SPECIAL   ("exclude",                 JobNode,excludePatternList,                     configValueParsePattern,configValueFormatInitPattern,configValueFormatDonePattern,configValueFormatPattern,NULL),
  CONFIG_STRUCT_VALUE_STRING    ("exclude-command",         JobNode,excludeCommand                          ),
  CONFIG_STRUCT_VALUE_SPECIAL   ("delta-source",            JobNode,deltaSourceList,                        configValueParseDeltaSource,configValueFormatInitDeltaSource,configValueFormatDoneDeltaSource,configValueFormatDeltaSource,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL   ("mount",                   JobNode,mountList,                              configValueParseMount,configValueFormatInitMount,configValueFormatDoneMount,configValueFormatMount,NULL),

  CONFIG_STRUCT_VALUE_INTEGER64 ("max-storage-size",        JobNode,jobOptions.maxStorageSize,              0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
//TODO
#if 0
  CONFIG_STRUCT_VALUE_INTEGER   ("min-keep",                JobNode,jobOptions.minKeep,                     0,MAX_INT,NULL),
  CONFIG_STRUCT_VALUE_INTEGER   ("max-keep",                JobNode,jobOptions.maxKeep,                     0,MAX_INT,NULL),
  CONFIG_STRUCT_VALUE_INTEGER   ("max-age",                 JobNode,jobOptions.maxAge,                      0,MAX_INT,NULL),
#endif
  CONFIG_STRUCT_VALUE_INTEGER64 ("volume-size",             JobNode,jobOptions.volumeSize,                  0LL,MAX_INT64,CONFIG_VALUE_BYTES_UNITS),
  CONFIG_STRUCT_VALUE_BOOLEAN   ("ecc",                     JobNode,jobOptions.errorCorrectionCodesFlag     ),

  CONFIG_STRUCT_VALUE_BOOLEAN   ("skip-unreadable",         JobNode,jobOptions.skipUnreadableFlag           ),
  CONFIG_STRUCT_VALUE_BOOLEAN   ("raw-images",              JobNode,jobOptions.rawImagesFlag                ),
  CONFIG_STRUCT_VALUE_SELECT    ("archive-file-mode",       JobNode,jobOptions.archiveFileMode,             CONFIG_VALUE_ARCHIVE_FILE_MODES),
  CONFIG_STRUCT_VALUE_BOOLEAN   ("overwrite-files",         JobNode,jobOptions.overwriteEntriesFlag         ),
  CONFIG_STRUCT_VALUE_BOOLEAN   ("wait-first-volume",       JobNode,jobOptions.waitFirstVolumeFlag          ),

  CONFIG_VALUE_BEGIN_SECTION("schedule",-1),
  CONFIG_STRUCT_VALUE_STRING    ("UUID",                    ScheduleNode,uuid                               ),
  CONFIG_STRUCT_VALUE_STRING    ("parentUUID",              ScheduleNode,parentUUID                         ),
  CONFIG_STRUCT_VALUE_SPECIAL   ("date",                    ScheduleNode,date,                              configValueParseScheduleDate,configValueFormatInitScheduleDate,configValueFormatDoneScheduleDate,configValueFormatScheduleDate,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL   ("weekdays",                ScheduleNode,weekDaySet,                        configValueParseScheduleWeekDaySet,configValueFormatInitScheduleWeekDaySet,configValueFormatDoneScheduleWeekDaySet,configValueFormatScheduleWeekDaySet,NULL),
  CONFIG_STRUCT_VALUE_SPECIAL   ("time",                    ScheduleNode,time,                              configValueParseScheduleTime,configValueFormatInitScheduleTime,configValueFormatDoneScheduleTime,configValueFormatScheduleTime,NULL),
  CONFIG_STRUCT_VALUE_SELECT    ("archive-type",            ScheduleNode,archiveType,                       CONFIG_VALUE_ARCHIVE_TYPES),
  CONFIG_STRUCT_VALUE_INTEGER   ("interval",                ScheduleNode,interval,                          0,MAX_INT,NULL),
  CONFIG_STRUCT_VALUE_STRING    ("text",                    ScheduleNode,customText                         ),
  CONFIG_STRUCT_VALUE_INTEGER   ("min-keep",                ScheduleNode,minKeep,                           0,MAX_INT,NULL),
  CONFIG_STRUCT_VALUE_INTEGER   ("max-keep",                ScheduleNode,maxKeep,                           0,MAX_INT,NULL),
  CONFIG_STRUCT_VALUE_INTEGER   ("max-age",                 ScheduleNode,maxAge,                            0,MAX_INT,NULL),
  CONFIG_STRUCT_VALUE_BOOLEAN   ("no-storage",              ScheduleNode,noStorage                          ),
  CONFIG_STRUCT_VALUE_BOOLEAN   ("enabled",                 ScheduleNode,enabled                            ),
  CONFIG_VALUE_END_SECTION(),

  CONFIG_STRUCT_VALUE_STRING    ("comment",                 JobNode,comment                                 ),

  // deprecated
  CONFIG_STRUCT_VALUE_DEPRECATED("mount-device",            JobNode,mountList,                              configValueParseDeprecatedMountDevice,NULL,"mount"),
  CONFIG_STRUCT_VALUE_DEPRECATED("schedule",                JobNode,scheduleList,                           configValueParseDeprecatedSchedule,NULL,"schedule section"),
//TODO
  CONFIG_STRUCT_VALUE_IGNORE    ("overwrite-archive-files"                                                  ),
  CONFIG_STRUCT_VALUE_DEPRECATED("stop-on-error",           JobNode,jobOptions.noStopOnErrorFlag,           configValueParseDeprecatedStopOnError,NULL,"no-stop-on-error"),
);

/***************************** Variables *******************************/
LOCAL AuthorizationFailList authorizationFailList;  // list with failed client authorizations
LOCAL uint                  serverPort;
LOCAL const Key             *serverCA;
LOCAL const Key             *serverCert;
LOCAL const Key             *serverKey;
LOCAL const Password        *serverPassword;
LOCAL const char            *serverJobsDirectory;
LOCAL const JobOptions      *serverDefaultJobOptions;
LOCAL JobList               jobList;                // job list
LOCAL Semaphore             statusLock;             // status update lock
LOCAL Thread                jobThread;              // thread executing jobs create/restore
LOCAL Thread                schedulerThread;        // thread scheduling jobs
LOCAL Thread                pauseThread;
LOCAL Thread                remoteConnectThread;    // thread to connect remote BAR instances
LOCAL Thread                remoteThread;           // thread to execute remote jobs
LOCAL Semaphore             indexThreadTrigger;
LOCAL Thread                indexThread;            // thread to add/update index
LOCAL Thread                autoIndexThread;        // thread to collect BAR files for auto-index
LOCAL Thread                purgeExpiredThread;     // thread to purge expired archive files
LOCAL Semaphore             serverStateLock;
LOCAL ServerStates          serverState;            // current server state
LOCAL struct
      {
        bool create;
        bool storage;
        bool restore;
        bool indexUpdate;
      } pauseFlags;                                 // TRUE iff pause
LOCAL uint64                pauseEndDateTime;       // pause end date/time [s]
LOCAL IndexHandle           *indexHandle;           // index handle
LOCAL bool                  quitFlag;               // TRUE iff quit requested

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : parseServerType
* Purpose: parse server type
* Input  : name - file|ftp|ssh|webdav
* Output : serverType - server type
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

LOCAL bool parseServerType(const char *name, ServerTypes *serverType)
{
  assert(name != NULL);
  assert(serverType != NULL);

  if      (stringEqualsIgnoreCase(name,"FILE"  )) (*serverType) = SERVER_TYPE_FILE;
  else if (stringEqualsIgnoreCase(name,"FTP"   )) (*serverType) = SERVER_TYPE_FTP;
  else if (stringEqualsIgnoreCase(name,"SSH"   )) (*serverType) = SERVER_TYPE_SSH;
  else if (stringEqualsIgnoreCase(name,"WEBDAV")) (*serverType) = SERVER_TYPE_WEBDAV;
  else return FALSE;

  return TRUE;
}

/***********************************************************************\
* Name   : parseArchiveType
* Purpose: parse archive type
* Input  : name - normal|full|incremental|differential|continuous
* Output : archiveType - archive type
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

LOCAL bool parseArchiveType(const char *name, ArchiveTypes *archiveType)
{
  const ConfigValueSelect *configValueSelect;

  assert(name != NULL);
  assert(archiveType != NULL);

  configValueSelect = CONFIG_VALUE_ARCHIVE_TYPES;
  while (   (configValueSelect->name != NULL)
         && !stringEqualsIgnoreCase(configValueSelect->name,name)
        )
  {
    configValueSelect++;
  }
  if (configValueSelect->name != NULL)
  {
    (*archiveType) = (ArchiveTypes)configValueSelect->value;
    return TRUE;
  }
  else
  {
    (*archiveType) = ARCHIVE_TYPE_NONE;
    return FALSE;
  }
}

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
  scheduleNode->uuid        = String_new();
  scheduleNode->parentUUID  = NULL;
  scheduleNode->date.year   = DATE_ANY;
  scheduleNode->date.month  = DATE_ANY;
  scheduleNode->date.day    = DATE_ANY;
  scheduleNode->weekDaySet  = WEEKDAY_SET_ANY;
  scheduleNode->time.hour   = TIME_ANY;
  scheduleNode->time.minute = TIME_ANY;
  scheduleNode->archiveType = ARCHIVE_TYPE_NORMAL;
  scheduleNode->interval    = 0;
  scheduleNode->customText  = String_new();
  scheduleNode->minKeep     = 0;
  scheduleNode->maxKeep     = 0;
  scheduleNode->maxAge      = 0;
  scheduleNode->noStorage   = FALSE;
  scheduleNode->enabled     = FALSE;

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
  scheduleNode->uuid        = Misc_getUUID(String_new());
  scheduleNode->parentUUID  = String_duplicate(fromScheduleNode->parentUUID);
  scheduleNode->date.year   = fromScheduleNode->date.year;
  scheduleNode->date.month  = fromScheduleNode->date.month;
  scheduleNode->date.day    = fromScheduleNode->date.day;
  scheduleNode->weekDaySet  = fromScheduleNode->weekDaySet;
  scheduleNode->time.hour   = fromScheduleNode->time.hour;
  scheduleNode->time.minute = fromScheduleNode->time.minute;
  scheduleNode->archiveType = fromScheduleNode->archiveType;
  scheduleNode->interval    = fromScheduleNode->interval;
  scheduleNode->customText  = String_duplicate(fromScheduleNode->customText);
  scheduleNode->minKeep     = fromScheduleNode->minKeep;
  scheduleNode->maxKeep     = fromScheduleNode->maxKeep;
  scheduleNode->maxAge      = fromScheduleNode->maxAge;
  scheduleNode->noStorage   = fromScheduleNode->noStorage;
  scheduleNode->enabled     = fromScheduleNode->enabled;

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

/***********************************************************************\
* Name   : equalsScheduleNode
* Purpose: compare schedule nodes if equals
* Input  : scheduleNode1,scheduleNode2 - schedule nodes
* Output : -
* Return : 1 iff scheduleNode1 = scheduleNode2, 0 otherwise
* Notes  : -
\***********************************************************************/

LOCAL int equalsScheduleNode(const ScheduleNode *scheduleNode1, const ScheduleNode *scheduleNode2)
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
      String_format(line,"%d",scheduleDate->year);
    }
    else
    {
      String_appendCString(line,"*");
    }
    String_appendChar(line,'-');
    if (scheduleDate->month != DATE_ANY)
    {
      String_format(line,"%d",scheduleDate->month);
    }
    else
    {
      String_appendCString(line,"*");
    }
    String_appendChar(line,'-');
    if (scheduleDate->day != DATE_ANY)
    {
      String_format(line,"%d",scheduleDate->day);
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
* Purpose: done format of config schedule statements
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

LOCAL bool configValueFormatScheduleTime(void **formatUserData, void *userData, String line)
{
  const ScheduleTime *scheduleTime;

  assert(formatUserData != NULL);

  UNUSED_VARIABLE(userData);
  UNUSED_VARIABLE(userData);

  scheduleTime = (const ScheduleTime*)(*formatUserData);
  if (scheduleTime != NULL)
  {
    if (scheduleTime->hour != TIME_ANY)
    {
      String_format(line,"%d",scheduleTime->hour);
    }
    else
    {
      String_appendCString(line,"*");
    }
    String_appendChar(line,':');
    if (scheduleTime->minute != TIME_ANY)
    {
      String_format(line,"%d",scheduleTime->minute);
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
    if (!parseArchiveType(String_cString(s),archiveType))
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
* Name   : configValueParseDeprecatedSchedule
* Purpose: config value option call back for parsing schedule
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

  // append to list
  List_append((ScheduleList*)variable,scheduleNode);

  return TRUE;
}

/***********************************************************************\
* Name   : parseScheduleDateTime
* Purpose: parse schedule date/time
* Input  : date        - date string (<year|*>-<month|*>-<day|*>)
*          weekDays    - week days string (<day>,...)
*          time        - time string <hour|*>:<minute|*>
* Output :
* Return : scheduleNode or NULL on error
* Notes  : month names: jan, feb, mar, apr, may, jun, jul, aug, sep, oct
*          nov, dec
*          week day names: mon, tue, wed, thu, fri, sat, sun
\***********************************************************************/

LOCAL ScheduleNode *parseScheduleDateTime(ConstString date,
                                          ConstString weekDays,
                                          ConstString time
                                         )
{
  ScheduleNode *scheduleNode;
  bool         errorFlag;
  String       s0,s1,s2;

  assert(date != NULL);
  assert(weekDays != NULL);
  assert(time != NULL);

  // allocate new schedule node
  scheduleNode = newScheduleNode();
  Misc_getUUID(scheduleNode->uuid);

  // parse date
  errorFlag = FALSE;
  s0 = String_new();
  s1 = String_new();
  s2 = String_new();
  if      (String_parse(date,STRING_BEGIN,"%S-%S-%S",NULL,s0,s1,s2))
  {
    if (!parseDateTimeNumber(s0,&scheduleNode->date.year)) errorFlag = TRUE;
    if (!parseDateMonth     (s1,&scheduleNode->date.month)) errorFlag = TRUE;
    if (!parseDateTimeNumber(s2,&scheduleNode->date.day)) errorFlag = TRUE;
  }
  else
  {
    errorFlag = TRUE;
  }

  // parse week days
  if (!parseWeekDaySet(String_cString(weekDays),&scheduleNode->weekDaySet))
  {
    errorFlag = TRUE;
  }

  // parse time
  if (String_parse(time,STRING_BEGIN,"%S:%S",NULL,s0,s1))
  {
    if (!parseDateTimeNumber(s0,&scheduleNode->time.hour  )) errorFlag = TRUE;
    if (!parseDateTimeNumber(s1,&scheduleNode->time.minute)) errorFlag = TRUE;
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
    LIST_DELETE_NODE(scheduleNode);
    return NULL;
  }

  return scheduleNode;
}

/***********************************************************************\
* Name   : isServerRunning
* Purpose: check if server is runnging
* Input  : -
* Output : -
* Return : TRUE iff server is running
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isServerRunning(void)
{
  return serverState == SERVER_STATE_RUNNING;
}

/***********************************************************************\
* Name   : storageRequestVolume
* Purpose: request volume call-back
* Input  : volumeNumber - volume number
*          userData     - user data: job node
* Output : -
* Return : request result; see StorageRequestResults
* Notes  : -
\***********************************************************************/

LOCAL StorageRequestResults storageRequestVolume(uint volumeNumber,
                                                 void *userData
                                                )
{
  JobNode               *jobNode = (JobNode*)userData;
  StorageRequestResults storageRequestResult;
  SemaphoreLock         semaphoreLock;

  assert(jobNode != NULL);

  storageRequestResult = STORAGE_REQUEST_VOLUME_NONE;

  SEMAPHORE_LOCKED_DO(semaphoreLock,&statusLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // request volume, set job state
    assert(jobNode->state == JOB_STATE_RUNNING);
    jobNode->requestedVolumeNumber = volumeNumber;
    String_format(String_clear(jobNode->runningInfo.message),"Please insert DVD #%d",volumeNumber);
    jobNode->state = JOB_STATE_REQUEST_VOLUME;
    Semaphore_signalModified(&jobList.lock);

    // wait until volume is available or job is aborted
    storageRequestResult = STORAGE_REQUEST_VOLUME_NONE;
    do
    {
      Semaphore_waitModified(&statusLock,LOCK_TIMEOUT);

      if      (jobNode->volumeUnloadFlag)
      {
        storageRequestResult = STORAGE_REQUEST_VOLUME_UNLOAD;
        jobNode->volumeUnloadFlag = FALSE;
      }
      else if (jobNode->volumeNumber == jobNode->requestedVolumeNumber)
      {
        storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
      }
      else if (jobNode->requestedAbortFlag)
      {
        storageRequestResult = STORAGE_REQUEST_VOLUME_ABORTED;
      }
    }
    while (   !quitFlag
           && (storageRequestResult == STORAGE_REQUEST_VOLUME_NONE)
          );

    // clear request volume, reset job state
    String_clear(jobNode->runningInfo.message);
    jobNode->state = JOB_STATE_RUNNING;
  }

  return storageRequestResult;
}

/***********************************************************************\
* Name   : resetJobRunningInfo
* Purpose: reset job running info
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void resetJobRunningInfo(JobNode *jobNode)
{
  assert(jobNode != NULL);

  jobNode->runningInfo.error                 = ERROR_NONE;
  jobNode->runningInfo.estimatedRestTime     = 0L;
  jobNode->runningInfo.doneCount             = 0L;
  jobNode->runningInfo.doneSize              = 0LL;
  jobNode->runningInfo.totalEntryCount       = 0L;
  jobNode->runningInfo.totalEntrySize        = 0LL;
  jobNode->runningInfo.collectTotalSumDone   = FALSE;
  jobNode->runningInfo.skippedEntryCount     = 0L;
  jobNode->runningInfo.skippedEntrySize      = 0LL;
  jobNode->runningInfo.errorEntryCount       = 0L;
  jobNode->runningInfo.errorEntrySize        = 0LL;
  jobNode->runningInfo.entriesPerSecond      = 0.0;
  jobNode->runningInfo.bytesPerSecond        = 0.0;
  jobNode->runningInfo.storageBytesPerSecond = 0.0;
  jobNode->runningInfo.archiveSize           = 0LL;
  jobNode->runningInfo.compressionRatio      = 0.0;
  String_clear(jobNode->runningInfo.entryName);
  jobNode->runningInfo.entryDoneSize         = 0LL;
  jobNode->runningInfo.entryTotalSize        = 0LL;
  String_clear(jobNode->runningInfo.storageName);
  jobNode->runningInfo.storageDoneSize       = 0LL;
  jobNode->runningInfo.storageTotalSize      = 0LL;
  jobNode->runningInfo.volumeNumber          = 0;
  jobNode->runningInfo.volumeProgress        = 0.0;
  String_clear(jobNode->runningInfo.message);

  Misc_performanceFilterClear(&jobNode->runningInfo.entriesPerSecondFilter     );
  Misc_performanceFilterClear(&jobNode->runningInfo.bytesPerSecondFilter       );
  Misc_performanceFilterClear(&jobNode->runningInfo.storageBytesPerSecondFilter);
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
  assert(jobNode->uuid != NULL);
  assert(jobNode->name != NULL);

  UNUSED_VARIABLE(userData);

  Misc_performanceFilterDone(&jobNode->runningInfo.storageBytesPerSecondFilter);
  Misc_performanceFilterDone(&jobNode->runningInfo.bytesPerSecondFilter       );
  Misc_performanceFilterDone(&jobNode->runningInfo.entriesPerSecondFilter     );

  String_delete(jobNode->runningInfo.message);
  String_delete(jobNode->runningInfo.storageName);
  String_delete(jobNode->runningInfo.entryName);

  String_delete(jobNode->schedule.customText);
  String_delete(jobNode->schedule.uuid);

  if (jobNode->cryptPassword != NULL) Password_delete(jobNode->cryptPassword);
  if (jobNode->sshPassword != NULL) Password_delete(jobNode->sshPassword);
  if (jobNode->ftpPassword != NULL) Password_delete(jobNode->ftpPassword);
  String_delete(jobNode->master);

  doneJobOptions(&jobNode->jobOptions);
  String_delete(jobNode->comment);
  List_done(&jobNode->scheduleList,CALLBACK((ListNodeFreeFunction)freeScheduleNode,NULL));
  DeltaSourceList_done(&jobNode->deltaSourceList);
  PatternList_done(&jobNode->compressExcludePatternList);
  List_done(&jobNode->mountList,CALLBACK((ListNodeFreeFunction)freeMountNode,NULL));
  String_delete(jobNode->excludeCommand);
  PatternList_done(&jobNode->excludePatternList);
  String_delete(jobNode->includeImageCommand);
  String_delete(jobNode->includeFileCommand);
  EntryList_done(&jobNode->includeEntryList);
  String_delete(jobNode->archiveName);
  Remote_doneHost(&jobNode->remoteHost);
  String_delete(jobNode->name);
  String_delete(jobNode->uuid);
  String_delete(jobNode->fileName);
}

/***********************************************************************\
* Name   : newJob
* Purpose: create new job
* Input  : jobType  - job type
*          fileName - file name or NULL
*          uuid     - UUID or NULL
* Output : -
* Return : job node
* Notes  : -
\***********************************************************************/

LOCAL JobNode *newJob(JobTypes jobType, ConstString fileName, ConstString uuid)
{
  JobNode *jobNode;

  // allocate job node
  jobNode = LIST_NEW_NODE(JobNode);
  if (jobNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init job node
  jobNode->jobType                        = jobType;
  jobNode->fileName                       = String_duplicate(fileName);
  jobNode->fileModified                   = 0LL;

  jobNode->uuid                           = String_new();
  if (!String_isEmpty(uuid))
  {
    String_set(jobNode->uuid,uuid);
  }
  else
  {
    Misc_getUUID(jobNode->uuid);
  }
  jobNode->name                           = File_getFileBaseName(String_new(),fileName);
  Remote_initHost(&jobNode->remoteHost,serverPort);
  jobNode->archiveName                    = String_new();
  EntryList_init(&jobNode->includeEntryList);
  jobNode->includeFileCommand             = String_new();
  jobNode->includeImageCommand            = String_new();
  PatternList_init(&jobNode->excludePatternList);
  jobNode->excludeCommand                 = String_new();
  List_init(&jobNode->mountList);
  PatternList_init(&jobNode->compressExcludePatternList);
  DeltaSourceList_init(&jobNode->deltaSourceList);
  List_init(&jobNode->scheduleList);
  initDuplicateJobOptions(&jobNode->jobOptions,serverDefaultJobOptions);
  jobNode->modifiedFlag                   = FALSE;
  jobNode->scheduleModifiedFlag           = FALSE;

  jobNode->lastExecutedDateTime           = 0LL;
  jobNode->lastCheckDateTime              = 0LL;

  jobNode->comment                        = String_new();

  jobNode->ftpPassword                    = NULL;
  jobNode->sshPassword                    = NULL;
  jobNode->cryptPassword                  = NULL;

  jobNode->timestamp                      = Misc_getTimestamp();
  jobNode->master                         = String_new();
  jobNode->state                          = JOB_STATE_NONE;
  jobNode->archiveType                    = ARCHIVE_TYPE_NORMAL;
  jobNode->dryRun                         = FALSE;
  jobNode->schedule.uuid                  = String_new();
  jobNode->schedule.customText            = String_new();
  jobNode->schedule.noStorage             = FALSE;
  jobNode->requestedAbortFlag             = FALSE;
  jobNode->requestedVolumeNumber          = 0;
  jobNode->volumeNumber                   = 0;
  jobNode->volumeUnloadFlag               = FALSE;

  jobNode->runningInfo.entryName          = String_new();
  jobNode->runningInfo.storageName        = String_new();
  jobNode->runningInfo.message            = String_new();

  Misc_performanceFilterInit(&jobNode->runningInfo.entriesPerSecondFilter,     10*60);
  Misc_performanceFilterInit(&jobNode->runningInfo.bytesPerSecondFilter,       10*60);
  Misc_performanceFilterInit(&jobNode->runningInfo.storageBytesPerSecondFilter,10*60);

  resetJobRunningInfo(jobNode);

  return jobNode;
}

/***********************************************************************\
* Name   : copyJob
* Purpose: copy job node
* Input  : jobNode  - job node
*          fileName - file name
* Output : -
* Return : new job node
* Notes  : -
\***********************************************************************/

LOCAL JobNode *copyJob(JobNode     *jobNode,
                       ConstString fileName
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
  newJobNode->fileName                       = String_duplicate(fileName);
  newJobNode->fileModified                   = 0LL;

  newJobNode->uuid                           = String_new();
  newJobNode->jobType                        = jobNode->jobType;
  newJobNode->name                           = File_getFileBaseName(String_new(),fileName);
  Remote_duplicateHost(&newJobNode->remoteHost,&jobNode->remoteHost);
  newJobNode->archiveName                    = String_duplicate(jobNode->archiveName);
  EntryList_initDuplicate(&newJobNode->includeEntryList,
                          &jobNode->includeEntryList,
                          CALLBACK(NULL,NULL)
                         );
  newJobNode->includeFileCommand             = String_duplicate(jobNode->includeFileCommand);
  newJobNode->includeImageCommand            = String_duplicate(jobNode->includeImageCommand);
  PatternList_initDuplicate(&newJobNode->excludePatternList,
                            &jobNode->excludePatternList,
                            CALLBACK(NULL,NULL)
                           );
  newJobNode->excludeCommand                 = String_duplicate(jobNode->excludeCommand);
  List_initDuplicate(&newJobNode->mountList,
                     &jobNode->mountList,
                     CALLBACK(NULL,NULL),
                     CALLBACK((ListNodeDuplicateFunction)duplicateMountNode,&newJobNode->mountList)
                    );
  PatternList_initDuplicate(&newJobNode->compressExcludePatternList,
                            &jobNode->compressExcludePatternList,
                            CALLBACK(NULL,NULL)
                           );
  DeltaSourceList_initDuplicate(&newJobNode->deltaSourceList,
                                &jobNode->deltaSourceList,
                                CALLBACK(NULL,NULL)
                               );
  List_initDuplicate(&newJobNode->scheduleList,
                     &jobNode->scheduleList,
                     CALLBACK(NULL,NULL),
                     CALLBACK((ListNodeDuplicateFunction)duplicateScheduleNode,NULL)
                    );
  initDuplicateJobOptions(&newJobNode->jobOptions,&jobNode->jobOptions);
  newJobNode->modifiedFlag                   = TRUE;
  newJobNode->scheduleModifiedFlag           = TRUE;

  newJobNode->lastExecutedDateTime           = 0LL;
  newJobNode->lastCheckDateTime              = 0LL;

  newJobNode->comment                        = String_duplicate(jobNode->comment);

  newJobNode->ftpPassword                    = NULL;
  newJobNode->sshPassword                    = NULL;
  newJobNode->cryptPassword                  = NULL;

  jobNode->timestamp                         = Misc_getTimestamp();
  jobNode->master                            = String_new();
  newJobNode->state                          = JOB_STATE_NONE;
  newJobNode->archiveType                    = ARCHIVE_TYPE_NORMAL;
  newJobNode->dryRun                         = FALSE;
  newJobNode->schedule.uuid                  = String_new();
  newJobNode->schedule.customText            = String_new();
  newJobNode->schedule.noStorage             = FALSE;
  newJobNode->requestedAbortFlag             = FALSE;
  newJobNode->requestedVolumeNumber          = 0;
  newJobNode->volumeNumber                   = 0;
  newJobNode->volumeUnloadFlag               = FALSE;

  newJobNode->runningInfo.entryName          = String_new();
  newJobNode->runningInfo.storageName        = String_new();
  newJobNode->runningInfo.message            = String_new();

  Misc_performanceFilterInit(&newJobNode->runningInfo.entriesPerSecondFilter,     10*60);
  Misc_performanceFilterInit(&newJobNode->runningInfo.bytesPerSecondFilter,       10*60);
  Misc_performanceFilterInit(&newJobNode->runningInfo.storageBytesPerSecondFilter,10*60);

  resetJobRunningInfo(newJobNode);

  return newJobNode;
}

// still not used
/***********************************************************************\
* Name   : deleteJob
* Purpose: delete job
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#if 0
// still not used
LOCAL void deleteJob(JobNode *jobNode)
{
  assert(jobNode != NULL);

  freeJobNode(jobNode,NULL);
  LIST_DELETE_NODE(jobNode);
}
#endif

/***********************************************************************\
* Name   : isJobLocal
* Purpose: check if local job
* Input  : jobNode - job node
* Output : -
* Return : TRUE iff local job
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isJobLocal(const JobNode *jobNode)
{
  assert(jobNode != NULL);

  return String_isEmpty(jobNode->remoteHost.name);
}

/***********************************************************************\
* Name   : isJobRemote
* Purpose: check if a remote job
* Input  : jobNode - job node
* Output : -
* Return : TRUE iff remote job
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isJobRemote(const JobNode *jobNode)
{
  assert(jobNode != NULL);

  return !String_isEmpty(jobNode->remoteHost.name);
}

/***********************************************************************\
* Name   : isJobActive
* Purpose: check if job is active (waiting/running)
* Input  : jobNode - job node
* Output : -
* Return : TRUE iff job is active
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isJobActive(const JobNode *jobNode)
{
  assert(jobNode != NULL);

  return (   (jobNode->state == JOB_STATE_WAITING) \
          || (jobNode->state == JOB_STATE_RUNNING) \
          || (jobNode->state == JOB_STATE_REQUEST_FTP_PASSWORD) \
          || (jobNode->state == JOB_STATE_REQUEST_SSH_PASSWORD) \
          || (jobNode->state == JOB_STATE_REQUEST_WEBDAV_PASSWORD) \
          || (jobNode->state == JOB_STATE_REQUEST_CRYPT_PASSWORD) \
          || (jobNode->state == JOB_STATE_REQUEST_VOLUME) \
         );
}

/***********************************************************************\
* Name   : isSomeJobActive
* Purpose: check if some job is active
* Input  : -
* Output : -
* Return : TRUE iff some job is active
* Notes  : -
\***********************************************************************/

LOCAL bool isSomeJobActive(void)
{
#if 0
//TODO: old implemenetation, remove
  SemaphoreLock semaphoreLock;
  const JobNode *jobNode;
  bool          activeFlag;

  activeFlag = FALSE;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    LIST_ITERATE(&jobList,jobNode)
    {
      if (isJobActive(jobNode))
      {
        activeFlag = TRUE;
        break;
      }
    }
  }

  return activeFlag;
#else
  return jobList.activeCount > 0;
#endif
}

/***********************************************************************\
* Name   : isJobRunning
* Purpose: check if job is running
* Input  : jobNode - job node
* Output : -
* Return : TRUE iff job is running
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool isJobRunning(const JobNode *jobNode)
{
  assert(jobNode != NULL);

  return (   (jobNode->state == JOB_STATE_RUNNING) \
          || (jobNode->state == JOB_STATE_REQUEST_FTP_PASSWORD) \
          || (jobNode->state == JOB_STATE_REQUEST_SSH_PASSWORD) \
          || (jobNode->state == JOB_STATE_REQUEST_WEBDAV_PASSWORD) \
          || (jobNode->state == JOB_STATE_REQUEST_CRYPT_PASSWORD) \
          || (jobNode->state == JOB_STATE_REQUEST_VOLUME) \
         );
}

/***********************************************************************\
* Name   : isSomeJobRunning
* Purpose: check if some job is runnging
* Input  : -
* Output : -
* Return : TRUE iff some job is running
* Notes  : -
\***********************************************************************/

LOCAL bool isSomeJobRunning(void)
{
  SemaphoreLock semaphoreLock;
  const JobNode *jobNode;
  bool          runningFlag;

  runningFlag = FALSE;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    LIST_ITERATE(&jobList,jobNode)
    {
      if (isJobRunning(jobNode))
      {
        runningFlag = TRUE;
        break;
      }
    }
  }

  return runningFlag;
}

/***********************************************************************\
* Name   : triggerJob
* Purpose: trogger job run
* Input  : jobNode - job node
*          archiveType - archive type to create
*          dryRun      - TRUE for dry-run, FALSE otherwise
*          scheduleUUID - schedule UUID or NULL
*          scheduleCustomText - schedule custom text or NULL
*          noStorage          - TRUE for no-strage, FALSE otherwise
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void triggerJob(JobNode      *jobNode,
                      ArchiveTypes archiveType,
                      bool         dryRun,
                      ConstString  scheduleUUID,
                      ConstString  scheduleCustomText,
                      bool         noStorage
                     )
{
  assert(jobNode != NULL);

  jobNode->state                 = JOB_STATE_WAITING;
  jobNode->archiveType           = archiveType;
  jobNode->dryRun                = dryRun;
  String_set(jobNode->schedule.uuid,scheduleUUID);
  String_set(jobNode->schedule.customText,scheduleCustomText);
  jobNode->schedule.noStorage    = noStorage;
  jobNode->requestedAbortFlag    = FALSE;
  jobNode->requestedVolumeNumber = 0;
  jobNode->volumeNumber          = 0;
  jobNode->volumeUnloadFlag      = FALSE;
  resetJobRunningInfo(jobNode);

//TODO
//fprintf(stderr,"%s, %d: isJobRemote=%d\n",__FILE__,__LINE__,isJobRemote(jobNode));
  if (isJobRemote(jobNode))
  {
     // start remote create job
     jobNode->runningInfo.error = Remote_jobStart(&jobNode->remoteHost,
                                                  jobNode->uuid,
                                                  NULL,
                                                  jobNode->archiveName,
                                                  &jobNode->includeEntryList,
                                                  &jobNode->excludePatternList,
                                                  &jobNode->mountList,
                                                  &jobNode->compressExcludePatternList,
                                                  &jobNode->deltaSourceList,
                                                  &jobNode->jobOptions,
                                                  archiveType,
                                                  NULL,  // scheduleTitle,
                                                  NULL,  // scheduleCustomText,
//                                                      CALLBACK(getCryptPassword,jobNode),
//                                                      CALLBACK(updateCreateStatusInfo,jobNode),
                                                  CALLBACK(storageRequestVolume,jobNode)
                                                 );
  }
}


/***********************************************************************\
* Name   : startJob
* Purpose: start job (store running data)
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void startJob(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  jobNode->state             = JOB_STATE_RUNNING;
  jobNode->runningInfo.error = ERROR_NONE;

  // increment active counter
  jobList.activeCount++;
}

/***********************************************************************\
* Name   : doneJob
* Purpose: done job (store running data, free job data, e. g. passwords)
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneJob(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  // decrement active counter
  assert(jobList.activeCount > 0);
  jobList.activeCount--;

  // set executed time, state
  jobNode->lastExecutedDateTime = Misc_getCurrentDateTime();
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
  String_clear(jobNode->schedule.uuid);
  String_clear(jobNode->schedule.customText);
}

/***********************************************************************\
* Name   : jobIncludeExcludeChanged
* Purpose: called when include/exclude lists changed
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void jobIncludeExcludeChanged(JobNode *jobNode)
{
  ScheduleNode *scheduleNode;

  assert(Semaphore_isLocked(&jobList.lock));

  // check if continuous schedule exists, update continuous notifies
  LIST_ITERATE(&jobNode->scheduleList,scheduleNode)
  {
    if (scheduleNode->archiveType == ARCHIVE_TYPE_CONTINUOUS)
    {
      if (scheduleNode->enabled)
      {
        Continuous_initNotify(jobNode->name,
                              jobNode->uuid,
                              scheduleNode->uuid,
                              &jobNode->includeEntryList
                             );
      }
      else
      {
        Continuous_doneNotify(jobNode->name,
                              jobNode->uuid,
                              scheduleNode->uuid
                             );
      }
    }
  }
}

/***********************************************************************\
* Name   : jobScheduleChanged
* Purpose: notify schedule related actions
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void jobScheduleChanged(const JobNode *jobNode)
{
  ScheduleNode *scheduleNode;

  assert(Semaphore_isLocked(&jobList.lock));

  // check if continuous schedule exists, update continuous notifies
  LIST_ITERATE(&jobNode->scheduleList,scheduleNode)
  {
    if (scheduleNode->archiveType == ARCHIVE_TYPE_CONTINUOUS)
    {
      if (scheduleNode->enabled)
      {
        Continuous_initNotify(jobNode->name,
                              jobNode->uuid,
                              scheduleNode->uuid,
                              &jobNode->includeEntryList
                             );
      }
      else
      {
        Continuous_doneNotify(jobNode->name,
                              jobNode->uuid,
                              scheduleNode->uuid
                             );
      }
    }
  }
}

/***********************************************************************\
* Name   : jobDeleted
* Purpose: delete job node
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#if 0
// still not used
LOCAL void jobDeleted(JobNode *jobNode)
{
  assert(Semaphore_isLocked(&jobList.lock));

  Continuous_doneNotify(jobNode->uuid,
                        NULL  // scheduleUUID
                       );
}
#endif

/***********************************************************************\
* Name   : findJobByName
* Purpose: find job by name
* Input  : name - job name
* Output : -
* Return : job node or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL JobNode *findJobByName(ConstString name)
{
  JobNode *jobNode;

  assert(Semaphore_isLocked(&jobList.lock));

//TODO: LIST_FIND
  jobNode = jobList.head;
  while ((jobNode != NULL) && !String_equals(jobNode->name,name))
  {
    jobNode = jobNode->next;
  }

  if (jobNode != NULL)
  {
    jobNode->timestamp = Misc_getTimestamp();
  }

  return jobNode;
}

/***********************************************************************\
* Name   : findJobByUUID
* Purpose: find job by uuid
* Input  : uuid - job uuid
* Output : -
* Return : job node or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL JobNode *findJobByUUID(ConstString uuid)
{
  JobNode *jobNode;

  assert(Semaphore_isLocked(&jobList.lock));

// TODO: list find
  jobNode = jobList.head;
  while ((jobNode != NULL) && !String_equals(jobNode->uuid,uuid))
  {
    jobNode = jobNode->next;
  }

  if (jobNode != NULL)
  {
    jobNode->timestamp = Misc_getTimestamp();
  }

  return jobNode;
}

/***********************************************************************\
* Name   : findScheduleByUUID
* Purpose: find schedule by uuid
* Input  : jobNode      - job node (can be NULL)
*          scheduleUUID - schedule UUID
* Output : -
* Return : schedule node or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL ScheduleNode *findScheduleByUUID(const JobNode *jobNode, ConstString scheduleUUID)
{
  ScheduleNode *scheduleNode;

  assert(Semaphore_isLocked(&jobList.lock));

  if (jobNode != NULL)
  {
    scheduleNode = jobNode->scheduleList.head;
    while ((scheduleNode != NULL) && !String_equals(scheduleNode->uuid,scheduleUUID))
    {
      scheduleNode = scheduleNode->next;
    }
  }
  else
  {
    scheduleNode = NULL;
    jobNode      = jobList.head;
    while ((jobNode != NULL) && (scheduleNode == NULL))
    {
      scheduleNode = jobNode->scheduleList.head;
      while ((scheduleNode != NULL) && !String_equals(scheduleNode->uuid,scheduleUUID))
      {
        scheduleNode = scheduleNode->next;
      }
      jobNode = jobNode->next;
    }
  }

  return scheduleNode;
}

/***********************************************************************\
* Name   : readJobScheduleInfo
* Purpose: read job schedule info
* Input  : jobNode - job node
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors readJobScheduleInfo(JobNode *jobNode)
{
  String     fileName,pathName,baseName;
  FileHandle fileHandle;
  Errors     error;
  String     line;
  uint64     n;

  assert(jobNode != NULL);

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
    while (File_getLine(&fileHandle,line,NULL,NULL))
    {
      // parse
      if (String_parse(line,STRING_BEGIN,"%lld",NULL,&n))
      {
        jobNode->lastExecutedDateTime = n;
        jobNode->lastCheckDateTime    = n;
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

  // free resources
  String_delete(fileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : writeJobScheduleInfo
* Purpose: write job schedule info
* Input  : jobNode - job node
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeJobScheduleInfo(JobNode *jobNode)
{
  String     fileName,pathName,baseName;
  FileHandle fileHandle;
  Errors     error;

  assert(jobNode != NULL);

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

    // close file
    File_close(&fileHandle);

    // free resources
    String_delete(fileName);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : updateJob
* Purpose: update job file
* Input  : jobNode - job node
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors updateJob(JobNode *jobNode)
{
  StringList        jobLinesList;
  String            line;
  Errors            error;
  int               i;
  StringNode        *nextStringNode;
  ScheduleNode      *scheduleNode;
  ConfigValueFormat configValueFormat;

  assert(jobNode != NULL);

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

    // delete old schedule sections, get position for insert new schedule sections
    nextStringNode = ConfigValue_deleteSections(&jobLinesList,"schedule");
    if (!List_isEmpty(&jobNode->scheduleList))
    {
      StringList_insertCString(&jobLinesList,"",nextStringNode);
      LIST_ITERATE(&jobNode->scheduleList,scheduleNode)
      {
        // insert new schedule sections
        String_format(String_clear(line),"[schedule]");
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

/***********************************************************************\
* Name   : updateAllJobs
* Purpose: update all job files
* Input  : -
* Output : -
* Return : -
* Notes  : update jobList
\***********************************************************************/

LOCAL void updateAllJobs(void)
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
        error = updateJob(jobNode);
        if (error != ERROR_NONE)
        {
          printWarning("Cannot update job '%s' (error: %s)\n",String_cString(jobNode->fileName),Error_getText(error));
        }
      }
    }
  }
}

/***********************************************************************\
* Name   : readJob
* Purpose: read job from file
* Input  : fileName - file name
* Output : -
* Return : TRUE iff job read, FALSE otherwise (error)
* Notes  : -
\***********************************************************************/

LOCAL bool readJob(JobNode *jobNode)
{
  Errors     error;
  FileHandle fileHandle;
  bool       failFlag;
  uint       lineNb;
  String     line;
  String     title;
  String     name,value;
  long       nextIndex;

  assert(jobNode != NULL);
  assert(jobNode->fileName != NULL);

  // reset job values
  String_clear(jobNode->uuid);
  String_clear(jobNode->archiveName);
  EntryList_clear(&jobNode->includeEntryList);
  PatternList_clear(&jobNode->excludePatternList);
  PatternList_clear(&jobNode->compressExcludePatternList);
  DeltaSourceList_clear(&jobNode->deltaSourceList);
  List_clear(&jobNode->scheduleList,CALLBACK((ListNodeFreeFunction)freeScheduleNode,NULL));
  jobNode->jobOptions.archiveType                  = ARCHIVE_TYPE_NORMAL;
  jobNode->jobOptions.archivePartSize              = 0LL;
  String_clear(jobNode->jobOptions.incrementalListFileName);
  jobNode->jobOptions.directoryStripCount          = DIRECTORY_STRIP_NONE;
  String_clear(jobNode->jobOptions.destination);
  jobNode->jobOptions.patternType                  = PATTERN_TYPE_GLOB;
  jobNode->jobOptions.compressAlgorithms.delta     = COMPRESS_ALGORITHM_NONE;
  jobNode->jobOptions.compressAlgorithms.byte      = COMPRESS_ALGORITHM_NONE;
  jobNode->jobOptions.cryptAlgorithm               = CRYPT_ALGORITHM_NONE;
  #ifdef HAVE_GCRYPT
    jobNode->jobOptions.cryptType                  = CRYPT_TYPE_SYMMETRIC;
  #else /* not HAVE_GCRYPT */
    jobNode->jobOptions.cryptType                  = CRYPT_TYPE_NONE;
  #endif /* HAVE_GCRYPT */
  jobNode->jobOptions.cryptPasswordMode            = PASSWORD_MODE_DEFAULT;
  String_clear(jobNode->jobOptions.cryptPublicKeyFileName);
  String_clear(jobNode->jobOptions.ftpServer.loginName);
  if (jobNode->jobOptions.ftpServer.password != NULL) Password_clear(jobNode->jobOptions.ftpServer.password);
  jobNode->jobOptions.sshServer.port               = 0;
  String_clear(jobNode->jobOptions.sshServer.loginName);
  if (jobNode->jobOptions.sshServer.password != NULL) Password_clear(jobNode->jobOptions.sshServer.password);
  clearKey(&jobNode->jobOptions.sshServer.publicKey);
  clearKey(&jobNode->jobOptions.sshServer.privateKey);
  String_clear(jobNode->jobOptions.preProcessScript);
  String_clear(jobNode->jobOptions.postProcessScript);
  jobNode->jobOptions.device.volumeSize            = 0LL;
  jobNode->jobOptions.waitFirstVolumeFlag          = FALSE;
  jobNode->jobOptions.errorCorrectionCodesFlag     = FALSE;
  jobNode->jobOptions.skipUnreadableFlag           = FALSE;
  jobNode->jobOptions.rawImagesFlag                = FALSE;
  jobNode->jobOptions.archiveFileMode              = ARCHIVE_FILE_MODE_STOP;
  jobNode->jobOptions.archiveFileModeOverwriteFlag = FALSE;
  jobNode->modifiedFlag                            = FALSE;
  jobNode->scheduleModifiedFlag                    = TRUE;

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
  title       = String_new();
  name        = String_new();
  value       = String_new();
  while (File_getLine(&fileHandle,line,&lineNb,"#") && !failFlag)
  {
    // parse line
    if      (String_parse(line,STRING_BEGIN,"[schedule %S]",NULL,title))
    {
      ScheduleNode *scheduleNode;

      scheduleNode = newScheduleNode();
      while (   File_getLine(&fileHandle,line,&lineNb,"#")
             && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
             && !failFlag
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
      if (String_isEmpty(scheduleNode->uuid))
      {
        Misc_getUUID(scheduleNode->uuid);
        jobNode->modifiedFlag = TRUE;
      }

      if (!List_appendUniq(&jobNode->scheduleList,scheduleNode,CALLBACK((ListNodeEqualsFunction)equalsScheduleNode,scheduleNode)))
      {
        deleteScheduleNode(scheduleNode);
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
  String_delete(title);
  String_delete(line);
  if (failFlag)
  {
    (void)File_close(&fileHandle);
    return FALSE;
  }

  // close file
  (void)File_close(&fileHandle);

  // set UUID if not exists
  if (String_isEmpty(jobNode->uuid))
  {
    Misc_getUUID(jobNode->uuid);
    jobNode->modifiedFlag = TRUE;
  }

  // save job if modified
  if (jobNode->modifiedFlag)
  {
    error = updateJob(jobNode);
    if (error != ERROR_NONE)
    {
      printWarning("Cannot update job '%s' (error: %s)\n",String_cString(jobNode->fileName),Error_getText(error));
    }
  }

  // save time modified
  jobNode->fileModified = File_getFileTimeModified(jobNode->fileName);

  // read schedule info
  (void)readJobScheduleInfo(jobNode);

  return TRUE;
}

/***********************************************************************\
* Name   : rereadAllJobs
* Purpose: re-read all job files
* Input  : jobsDirectory - directory with job files
* Output : -
* Return : ERROR_NONE or error code
* Notes  : update jobList
\***********************************************************************/

LOCAL Errors rereadAllJobs(const char *jobsDirectory)
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
  File_setFileNameCString(fileName,jobsDirectory);
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
    File_getFileBaseName(baseName,fileName);

    // check if readable file and not ".*"
    if (File_isFile(fileName) && File_isReadable(fileName) && !String_startsWithChar(baseName,'.'))
    {
      SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
      {
        // find/create job
        jobNode = findJobByName(baseName);
        if (jobNode == NULL)
        {
          jobNode = newJob(JOB_TYPE_CREATE,fileName,NULL);
          assert(jobNode != NULL);
          Misc_getUUID(jobNode->uuid);
          List_append(&jobList,jobNode);
        }

        if (   !isJobActive(jobNode)
            && (File_getFileTimeModified(fileName) > jobNode->fileModified)
           )
        {
          // read job
          readJob(jobNode);

          // notify about changes
          jobIncludeExcludeChanged(jobNode);
          jobScheduleChanged(jobNode);
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
        File_setFileNameCString(fileName,jobsDirectory);
        File_appendFileName(fileName,jobNode->name);
        if (File_isFile(fileName) && File_isReadable(fileName))
        {
          // exists => ok
          jobNode = jobNode->next;
        }
        else
        {
          // do not exists anymore => delete job node

          // notify about changes
          jobIncludeExcludeChanged(jobNode);
          jobScheduleChanged(jobNode);

          // remove
          jobNode = List_removeAndFree(&jobList,jobNode,CALLBACK((ListNodeFreeFunction)freeJobNode,NULL));
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
        if (String_equals(jobNode1->uuid,jobNode2->uuid))
        {
          printWarning("Duplicate job UUID in '%s' and '%s'!\n",String_cString(jobNode1->name),String_cString(jobNode2->name));
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

/***********************************************************************\
* Name   : getAllJobUUIDs
* Purpose: get all job UUIDs
* Input  : jobUUIDList - list variable
* Output : jobUUIDList - updated list
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void getAllJobUUIDs(StringList *jobUUIDList)
{
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(jobUUIDList != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    LIST_ITERATE(&jobList,jobNode)
    {
      StringList_append(jobUUIDList,jobNode->uuid);
    }
  }
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : getCryptPassword
* Purpose: get crypt password call-back
* Input  : loginName     - login name variable (not used)
*          password      - crypt password variable
*          passwordType  - password type (not used)
*          text          - text (not used)
*          validateFlag  - TRUE to validate input, FALSE otherwise (not
*                          used)
*          weakCheckFlag - TRUE for weak password checking, FALSE
*                          otherwise (print warning if password seems to
*                          be a weak password) (not used)
*          userData      - user data: job node
* Output : password - crypt password
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getCryptPassword(String        loginName,
                              Password      *password,
                              PasswordTypes passwordType,
                              const char    *text,
                              bool          validateFlag,
                              bool          weakCheckFlag,
                              void          *userData
                             )
{
  JobNode *jobNode = (JobNode*)userData;

  assert(jobNode != NULL);

  UNUSED_VARIABLE(loginName);
  UNUSED_VARIABLE(passwordType);
  UNUSED_VARIABLE(text);
  UNUSED_VARIABLE(validateFlag);
  UNUSED_VARIABLE(weakCheckFlag);

  if (jobNode->cryptPassword != NULL)
  {
    Password_set(password,jobNode->cryptPassword);
    return ERROR_NONE;
  }
  else
  {
    return ERROR_NO_CRYPT_PASSWORD;
  }
}

/***********************************************************************\
* Name   : updateCreateStatusInfo
* Purpose: update create status info
* Input  : error            - error code
*          createStatusInfo - create status info data
*          userData         - user data: job node
* Output : -
* Return :
* Notes  : -
\***********************************************************************/

LOCAL void updateCreateStatusInfo(Errors                 error,
                                  const CreateStatusInfo *createStatusInfo,
                                  void                   *userData
                                 )
{
  JobNode       *jobNode = (JobNode*)userData;
  SemaphoreLock semaphoreLock;
  double        entriesPerSecondAverage,bytesPerSecondAverage,storageBytesPerSecondAverage;
  ulong         restFiles;
  uint64        restBytes;
  uint64        restStorageBytes;
  ulong         estimatedRestTime;

  assert(jobNode != NULL);
  assert(createStatusInfo != NULL);
  assert(createStatusInfo->entryName != NULL);
  assert(createStatusInfo->storageName != NULL);

  // Note: only try for 2s
  SEMAPHORE_LOCKED_DO(semaphoreLock,&statusLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,2*1000L)
  {
    // calculate statics values
    Misc_performanceFilterAdd(&jobNode->runningInfo.entriesPerSecondFilter,     createStatusInfo->doneCount);
    Misc_performanceFilterAdd(&jobNode->runningInfo.bytesPerSecondFilter,       createStatusInfo->doneSize);
    Misc_performanceFilterAdd(&jobNode->runningInfo.storageBytesPerSecondFilter,createStatusInfo->storageDoneSize);
    entriesPerSecondAverage      = Misc_performanceFilterGetAverageValue(&jobNode->runningInfo.entriesPerSecondFilter     );
    bytesPerSecondAverage        = Misc_performanceFilterGetAverageValue(&jobNode->runningInfo.bytesPerSecondFilter       );
    storageBytesPerSecondAverage = Misc_performanceFilterGetAverageValue(&jobNode->runningInfo.storageBytesPerSecondFilter);

    restFiles        = (createStatusInfo->totalEntryCount  > createStatusInfo->doneCount      ) ? createStatusInfo->totalEntryCount -createStatusInfo->doneCount       : 0L;
    restBytes        = (createStatusInfo->totalEntrySize   > createStatusInfo->doneSize       ) ? createStatusInfo->totalEntrySize  -createStatusInfo->doneSize        : 0LL;
    restStorageBytes = (createStatusInfo->storageTotalSize > createStatusInfo->storageDoneSize) ? createStatusInfo->storageTotalSize-createStatusInfo->storageDoneSize : 0LL;
    estimatedRestTime = 0L;
    if (entriesPerSecondAverage      > 0.0) { estimatedRestTime = MAX(estimatedRestTime,(ulong)lround((double)restFiles       /entriesPerSecondAverage     )); }
    if (bytesPerSecondAverage        > 0.0) { estimatedRestTime = MAX(estimatedRestTime,(ulong)lround((double)restBytes       /bytesPerSecondAverage       )); }
    if (storageBytesPerSecondAverage > 0.0) { estimatedRestTime = MAX(estimatedRestTime,(ulong)lround((double)restStorageBytes/storageBytesPerSecondAverage)); }

#if 0
fprintf(stderr,"%s,%d: doneCount=%lu doneSize=%llu skippedEntryCount=%lu skippedEntrySize=%llu totalEntryCount=%lu totalEntrySize %llu -- entriesPerSecond=%lf bytesPerSecond=%lf estimatedRestTime=%lus\n",__FILE__,__LINE__,
createStatusInfo->doneCount,
createStatusInfo->doneSize,
createStatusInfo->skippedEntryCount,
createStatusInfo->skippedEntrySize,
createStatusInfo->totalEntryCount,
createStatusInfo->totalEntrySize,
jobNode->runningInfo.entriesPerSecond,
jobNode->runningInfo.bytesPerSecond,
jobNode->runningInfo.estimatedRestTime
);
#endif

    jobNode->runningInfo.error                 = error;
    jobNode->runningInfo.doneCount             = createStatusInfo->doneCount;
    jobNode->runningInfo.doneSize              = createStatusInfo->doneSize;
    jobNode->runningInfo.totalEntryCount       = createStatusInfo->totalEntryCount;
    jobNode->runningInfo.totalEntrySize        = createStatusInfo->totalEntrySize;
    jobNode->runningInfo.collectTotalSumDone   = createStatusInfo->collectTotalSumDone;
    jobNode->runningInfo.skippedEntryCount     = createStatusInfo->skippedEntryCount;
    jobNode->runningInfo.skippedEntrySize      = createStatusInfo->skippedEntrySize;
    jobNode->runningInfo.errorEntryCount       = createStatusInfo->errorEntryCount;
    jobNode->runningInfo.errorEntrySize        = createStatusInfo->errorEntrySize;
    jobNode->runningInfo.entriesPerSecond      = Misc_performanceFilterGetValue(&jobNode->runningInfo.entriesPerSecondFilter     ,10);
    jobNode->runningInfo.bytesPerSecond        = Misc_performanceFilterGetValue(&jobNode->runningInfo.bytesPerSecondFilter       ,10);
    jobNode->runningInfo.storageBytesPerSecond = Misc_performanceFilterGetValue(&jobNode->runningInfo.storageBytesPerSecondFilter,10);
    jobNode->runningInfo.archiveSize           = createStatusInfo->archiveSize;
    jobNode->runningInfo.compressionRatio      = createStatusInfo->compressionRatio;
    jobNode->runningInfo.estimatedRestTime     = estimatedRestTime;
    String_set(jobNode->runningInfo.entryName,createStatusInfo->entryName);
    jobNode->runningInfo.entryDoneSize         = createStatusInfo->entryDoneSize;
    jobNode->runningInfo.entryTotalSize        = createStatusInfo->entryTotalSize;
    String_set(jobNode->runningInfo.storageName,createStatusInfo->storageName);
    jobNode->runningInfo.storageDoneSize       = createStatusInfo->storageDoneSize;
    jobNode->runningInfo.storageTotalSize      = createStatusInfo->storageTotalSize;
    jobNode->runningInfo.volumeNumber          = createStatusInfo->volumeNumber;
    jobNode->runningInfo.volumeProgress        = createStatusInfo->volumeProgress;
    if (error != ERROR_NONE)
    {
      String_setCString(jobNode->runningInfo.message,Error_getText(error));
    }
  }
}

/***********************************************************************\
* Name   : restoreUpdateStatusInfo
* Purpose: update restore status info
* Input  : restoreStatusInfo - create status info data
*          userData          - user data: job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void restoreUpdateStatusInfo(const RestoreStatusInfo *restoreStatusInfo,
                                   void                    *userData
                                  )
{
  JobNode       *jobNode = (JobNode*)userData;
  SemaphoreLock semaphoreLock;
//NYI:  double        entriesPerSecond,bytesPerSecond,storageBytesPerSecond;

  assert(jobNode != NULL);
  assert(restoreStatusInfo != NULL);
  assert(restoreStatusInfo->storageName != NULL);
  assert(restoreStatusInfo->entryName != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&statusLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,2*1000L)
  {
    // calculate estimated rest time
    Misc_performanceFilterAdd(&jobNode->runningInfo.entriesPerSecondFilter,     restoreStatusInfo->doneCount);
    Misc_performanceFilterAdd(&jobNode->runningInfo.bytesPerSecondFilter,       restoreStatusInfo->doneSize);
    Misc_performanceFilterAdd(&jobNode->runningInfo.storageBytesPerSecondFilter,restoreStatusInfo->storageDoneSize);

/*
fprintf(stderr,"%s,%d: restoreStatusInfo->doneCount=%lu restoreStatusInfo->doneSize=%llu jobNode->runningInfo.totalEntryCount=%lu jobNode->runningInfo.totalEntrySize %llu -- entriesPerSecond=%lf bytesPerSecond=%lf estimatedRestTime=%lus\n",__FILE__,__LINE__,
restoreStatusInfo->doneCount,
restoreStatusInfo->doneSize,
jobNode->runningInfo.totalEntryCount,
jobNode->runningInfo.totalEntrySize,
entriesPerSecond,bytesPerSecond,estimatedRestTime);
*/

//TODO
//    jobNode->runningInfo.error                 = error;
    jobNode->runningInfo.doneCount             = restoreStatusInfo->doneCount;
    jobNode->runningInfo.doneSize              = restoreStatusInfo->doneSize;
    jobNode->runningInfo.skippedEntryCount     = restoreStatusInfo->skippedEntryCount;
    jobNode->runningInfo.skippedEntrySize      = restoreStatusInfo->skippedEntrySize;
    jobNode->runningInfo.errorEntryCount       = restoreStatusInfo->errorEntryCount;
    jobNode->runningInfo.errorEntrySize        = restoreStatusInfo->errorEntrySize;
    jobNode->runningInfo.archiveSize           = 0LL;
    jobNode->runningInfo.compressionRatio      = 0.0;
    jobNode->runningInfo.estimatedRestTime     = 0;
    String_set(jobNode->runningInfo.entryName,restoreStatusInfo->entryName);
    jobNode->runningInfo.entryDoneSize         = restoreStatusInfo->entryDoneSize;
    jobNode->runningInfo.entryTotalSize        = restoreStatusInfo->entryTotalSize;
    String_set(jobNode->runningInfo.storageName,restoreStatusInfo->storageName);
    jobNode->runningInfo.storageDoneSize       = restoreStatusInfo->storageDoneSize;
    jobNode->runningInfo.storageTotalSize      = restoreStatusInfo->storageTotalSize;
    jobNode->runningInfo.volumeNumber          = 0; // ???
    jobNode->runningInfo.volumeProgress        = 0.0; // ???
  }
}

/***********************************************************************\
* Name   : jobThreadCode
* Purpose: job thread entry
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void jobThreadCode(void)
{
  StorageSpecifier storageSpecifier;
  String           storageName;
  String           directory;
  SemaphoreLock    semaphoreLock;
  JobNode          *jobNode;
  LogHandle        logHandle;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  String           hostName;
  RemoteHost       remoteHost;
  EntryList        includeEntryList;
  PatternList      excludePatternList;
  MountList        mountList;
  PatternList      compressExcludePatternList;
  DeltaSourceList  deltaSourceList;
  JobOptions       jobOptions;
  ArchiveTypes     archiveType;
  StaticString     (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String           scheduleCustomText;
  String           script;
  IndexHandle      *indexHandle;
  uint64           startDateTime,endDateTime;
  StringList       archiveFileNameList;
  TextMacro        textMacros[4];
  StaticString     (s,64);
  uint             n;
  Errors           error;

  // initialize variables
  Storage_initSpecifier(&storageSpecifier);
  storageName        = String_new();
  directory          = String_new();
  hostName           = String_new();
  Remote_initHost(&remoteHost,serverPort);
  EntryList_init(&includeEntryList);
  PatternList_init(&excludePatternList);
  List_init(&mountList);
  PatternList_init(&compressExcludePatternList);
  DeltaSourceList_init(&deltaSourceList);
  scheduleCustomText = String_new();

  // open index
  indexHandle = Index_open(INDEX_PRIORITY_HIGH,INDEX_TIMEOUT);

  jobNode     = NULL;
  archiveType = ARCHIVE_ENTRY_TYPE_UNKNOWN;
  while (!quitFlag)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      // wait for and get next job to execute
      do
      {
        // first check for continuous
        jobNode = jobList.head;
        while (   !quitFlag
               && (jobNode != NULL)
               && (   (jobNode->archiveType != ARCHIVE_TYPE_CONTINUOUS)
                   || (isJobRemote(jobNode) || (jobNode->state != JOB_STATE_WAITING))
                  )
              )
        {
          jobNode = jobNode->next;
        }

        if (jobNode == NULL)
        {
          // next check for other types
          jobNode = jobList.head;
          while (   !quitFlag
                 && (jobNode != NULL)
                 && (isJobRemote(jobNode) || (jobNode->state != JOB_STATE_WAITING))
                )
          {
            jobNode = jobNode->next;
          }
        }

        if (!quitFlag && (jobNode == NULL)) Semaphore_waitModified(&jobList.lock,LOCK_TIMEOUT);
      }
      while (!quitFlag && (jobNode == NULL));
      if (quitFlag)
      {
        Semaphore_unlock(&jobList.lock);
        break;
      }
      assert(jobNode != NULL);

      // start job
      startJob(jobNode);

      // get copy of mandatory job data
      String_set(storageName,jobNode->archiveName);
      String_set(jobUUID,jobNode->uuid);
      Network_getHostName(hostName);
      Remote_copyHost(&remoteHost,&jobNode->remoteHost);
      EntryList_clear(&includeEntryList); EntryList_copy(&jobNode->includeEntryList,&includeEntryList,CALLBACK(NULL,NULL));
      PatternList_clear(&excludePatternList); PatternList_copy(&jobNode->excludePatternList,&excludePatternList,CALLBACK(NULL,NULL));
      List_clear(&mountList,CALLBACK((ListNodeFreeFunction)freeMountNode,NULL)); List_copy(&jobNode->mountList,&mountList,NULL,NULL,NULL,CALLBACK((ListNodeDuplicateFunction)duplicateMountNode,NULL));
      PatternList_clear(&compressExcludePatternList); PatternList_copy(&jobNode->compressExcludePatternList,&compressExcludePatternList,CALLBACK(NULL,NULL));
      DeltaSourceList_clear(&deltaSourceList); DeltaSourceList_copy(&jobNode->deltaSourceList,&deltaSourceList,CALLBACK(NULL,NULL));
      initDuplicateJobOptions(&jobOptions,&jobNode->jobOptions);
      archiveType = jobNode->archiveType;
      jobOptions.dryRunFlag = jobNode->dryRun;
      if (!String_isEmpty(jobNode->schedule.uuid))
      {
        String_set(scheduleUUID,      jobNode->schedule.uuid);
        String_set(scheduleCustomText,jobNode->schedule.customText);
        jobOptions.noStorageFlag = jobNode->schedule.noStorage;
      }
      else
      {
        String_clear(scheduleUUID);
        String_clear(scheduleCustomText);
        jobOptions.noStorageFlag = FALSE;
      }
    }
    if (jobNode == NULL)
    {
      break;
    }

    // Note: job is now protected by running state)

    // get start date/time
    startDateTime = Misc_getCurrentDateTime();

    // init log
    initLog(&logHandle);

    // parse storage name
    if (jobNode->runningInfo.error == ERROR_NONE)
    {
      jobNode->runningInfo.error = Storage_parseName(&storageSpecifier,storageName);
      if (jobNode->runningInfo.error != ERROR_NONE)
      {
        logMessage(&logHandle,
                   LOG_TYPE_ALWAYS,
                   "Aborted job '%s': invalid storage '%s' (error: %s)\n",
                   String_cString(jobNode->name),
                   Storage_getPrintableNameCString(&storageSpecifier,NULL),
                   Error_getText(jobNode->runningInfo.error)
                  );
      }
    }

    // get include/excluded entries from commands
    if (!String_isEmpty(jobNode->includeFileCommand))
    {
      if (jobNode->runningInfo.error == ERROR_NONE)
      {
        jobNode->runningInfo.error = addIncludeListCommand(ENTRY_TYPE_FILE,&includeEntryList,String_cString(jobNode->includeFileCommand));
      }
    }
    if (!String_isEmpty(jobNode->includeImageCommand))
    {
      if (jobNode->runningInfo.error == ERROR_NONE)
      {
        jobNode->runningInfo.error = addIncludeListCommand(ENTRY_TYPE_IMAGE,&includeEntryList,String_cString(jobNode->includeImageCommand));
      }
    }
    if (!String_isEmpty(jobNode->excludeCommand))
    {
      if (jobNode->runningInfo.error == ERROR_NONE)
      {
        jobNode->runningInfo.error = addExcludeListCommand(&excludePatternList,String_cString(jobNode->excludeCommand));
      }
    }

    // pre-process command
    if (jobNode->runningInfo.error == ERROR_NONE)
    {
      if (jobNode->jobOptions.preProcessScript != NULL)
      {
        // get script
        TEXT_MACRO_N_STRING (textMacros[0],"%name",     jobNode->name,NULL);
        TEXT_MACRO_N_STRING (textMacros[1],"%archive",  storageName,NULL);
        TEXT_MACRO_N_STRING (textMacros[2],"%directory",File_getFilePathName(directory,storageSpecifier.archiveName),NULL);
        TEXT_MACRO_N_STRING (textMacros[3],"%file",     storageSpecifier.archiveName,NULL);
        script = expandTemplate(String_cString(jobNode->jobOptions.preProcessScript),
                                EXPAND_MACRO_MODE_STRING,
                                startDateTime,
                                TRUE,
                                textMacros,
                                SIZE_OF_ARRAY(textMacros)
                               );
        if (script != NULL)
        {
          // execute script
          jobNode->runningInfo.error = Misc_executeScript(String_cString(script),
                                                          CALLBACK(executeIOOutput,NULL),
                                                          CALLBACK(executeIOOutput,NULL)
                                                         );
          String_delete(script);
        }
        else
        {
          jobNode->runningInfo.error = ERROR_EXPAND_TEMPLATE;
        }

        if (jobNode->runningInfo.error != ERROR_NONE)
        {
          logMessage(&logHandle,
                     LOG_TYPE_ALWAYS,
                     "Aborted job '%s': pre-command fail (error: %s)\n",
                     String_cString(jobNode->name),
                     Error_getText(jobNode->runningInfo.error)
                    );
        }
      }
    }

    // run job
    if (jobNode->runningInfo.error == ERROR_NONE)
    {
      #ifdef SIMULATOR
        {
          int z;

          jobNode->runningInfo.estimatedRestTime=120;

          jobNode->runningInfo.totalEntryCount += 60;
          jobNode->runningInfo.totalEntrySize += 6000;

          for (z=0;z<120;z++)
          {
            extern void sleep(int);
            if (jobNode->requestedAbortFlag) break;

            sleep(1);

            if (z==40) {
              jobNode->runningInfo.totalEntryCount += 80;
              jobNode->runningInfo.totalEntrySize += 8000;
            }

            jobNode->runningInfo.doneCount++;
            jobNode->runningInfo.doneSize += 100;
//            jobNode->runningInfo.totalEntryCount += 3;
//            jobNode->runningInfo.totalEntrySize += 181;
            jobNode->runningInfo.estimatedRestTime=120-z;
            String_clear(jobNode->runningInfo.fileName);String_format(jobNode->runningInfo.fileName,"file %d",z);
            String_clear(jobNode->runningInfo.storageName);String_format(jobNode->runningInfo.storageName,"storage %d%d",z,z);
          }
        }
      #else
        switch (jobNode->jobType)
        {
          case JOB_TYPE_CREATE:
            String_clear(s);
            if (jobOptions.dryRunFlag || jobOptions.noStorageFlag)
            {
              String_appendCString(s," (");
              n = 0;
              if (jobOptions.dryRunFlag)
              {
                if (n > 0) String_appendCString(s,", ");
                String_appendCString(s,"dry-run");
                n++;
              }
              if (jobOptions.noStorageFlag)
              {
                if (n > 0) String_appendCString(s,", ");
                String_appendCString(s,"no-storage");
                n++;
              }
              String_appendCString(s,")");
            }
            logMessage(&logHandle,
                       LOG_TYPE_ALWAYS,
                       "Start job '%s'%s\n",
                       String_cString(jobNode->name),
                       !String_isEmpty(s) ? String_cString(s) : ""
                      );

            // create archive
            jobNode->runningInfo.error = Command_create(jobUUID,
                                                        scheduleUUID,
                                                        storageName,
                                                        &includeEntryList,
                                                        &excludePatternList,
                                                        &mountList,
                                                        &compressExcludePatternList,
                                                        &deltaSourceList,
                                                        &jobOptions,
                                                        archiveType,
NULL,//                                                        scheduleTitle,
                                                        scheduleCustomText,
                                                        CALLBACK(getCryptPassword,jobNode),
                                                        CALLBACK(updateCreateStatusInfo,jobNode),
                                                        CALLBACK(storageRequestVolume,jobNode),
                                                        &pauseFlags.create,
                                                        &pauseFlags.storage,
//TODO access jobNode?
                                                        &jobNode->requestedAbortFlag,
                                                        &logHandle
                                                       );

            // get end date/time
            endDateTime = Misc_getCurrentDateTime();

            if (jobNode->requestedAbortFlag)
            {
              // aborted
              logMessage(&logHandle,
                         LOG_TYPE_ALWAYS,
                         "Aborted job '%s'\n",
                         String_cString(jobNode->name)
                        );

              // create history entry
              if (indexHandle != NULL)
              {
                error = Index_newHistory(indexHandle,
                                         jobUUID,
                                         scheduleUUID,
                                         hostName,
                                         archiveType,
                                         Misc_getTimestamp(),
                                         "aborted",
                                         endDateTime-startDateTime,
                                         jobNode->runningInfo.totalEntryCount,
                                         jobNode->runningInfo.totalEntrySize,
                                         jobNode->runningInfo.skippedEntryCount,
                                         jobNode->runningInfo.skippedEntrySize,
                                         jobNode->runningInfo.errorEntryCount,
                                         jobNode->runningInfo.errorEntrySize,
                                         NULL  // historyId
                                        );
                if (error != ERROR_NONE)
                {
                  logMessage(&logHandle,
                             LOG_TYPE_ALWAYS,
                             "Cannot insert history information for '%s' (error: %s)\n",
                             String_cString(jobNode->name),
                             Error_getText(error)
                            );
                }
              }
            }
            else if (jobNode->runningInfo.error != ERROR_NONE)
            {
              // error
              logMessage(&logHandle,
                         LOG_TYPE_ALWAYS,
                         "Done job '%s' (error: %s)\n",
                         String_cString(jobNode->name),
                         Error_getText(jobNode->runningInfo.error)
                        );

              // create history entry
              if (indexHandle != NULL)
              {
                error = Index_newHistory(indexHandle,
                                         jobUUID,
                                         scheduleUUID,
                                         hostName,
                                         archiveType,
                                         Misc_getTimestamp(),
                                         Error_getText(jobNode->runningInfo.error),
                                         endDateTime-startDateTime,
                                         jobNode->runningInfo.totalEntryCount,
                                         jobNode->runningInfo.totalEntrySize,
                                         jobNode->runningInfo.skippedEntryCount,
                                         jobNode->runningInfo.skippedEntrySize,
                                         jobNode->runningInfo.errorEntryCount,
                                         jobNode->runningInfo.errorEntrySize,
                                         NULL  // historyId
                                        );
                if (error != ERROR_NONE)
                {
                  logMessage(&logHandle,
                             LOG_TYPE_ALWAYS,
                             "Cannot insert history information for '%s' (error: %s)\n",
                             String_cString(jobNode->name),
                             Error_getText(error)
                            );
                }
              }
            }
            else
            {
              // success
              logMessage(&logHandle,
                         LOG_TYPE_ALWAYS,
                         "Done job '%s' (duration: %llus)\n",
                         String_cString(jobNode->name),
                         endDateTime-startDateTime
                        );

              // create history entry
              if (indexHandle != NULL)
              {
                error = Index_newHistory(indexHandle,
                                         jobUUID,
                                         scheduleUUID,
                                         hostName,
                                         archiveType,
                                         Misc_getTimestamp(),
                                         NULL,  // errorMessage
                                         endDateTime-startDateTime,
                                         jobNode->runningInfo.totalEntryCount,
                                         jobNode->runningInfo.totalEntrySize,
                                         jobNode->runningInfo.skippedEntryCount,
                                         jobNode->runningInfo.skippedEntrySize,
                                         jobNode->runningInfo.errorEntryCount,
                                         jobNode->runningInfo.errorEntrySize,
                                         NULL  // historyId
                                        );
                if (error != ERROR_NONE)
                {
                  logMessage(&logHandle,
                             LOG_TYPE_ALWAYS,
                             "Cannot insert history information for '%s' (error: %s)\n",
                             String_cString(jobNode->name),
                             Error_getText(error)
                            );
                }
              }
            }
            break;
          case JOB_TYPE_RESTORE:
            logMessage(&logHandle,
                       LOG_TYPE_ALWAYS,
                       "Start restore archive\n"
                      );

            // restore archive
            StringList_init(&archiveFileNameList);
            StringList_append(&archiveFileNameList,storageName);
            jobNode->runningInfo.error = Command_restore(&archiveFileNameList,
                                                         &includeEntryList,
                                                         &excludePatternList,
                                                         &deltaSourceList,
                                                         &jobOptions,
                                                         CALLBACK(restoreUpdateStatusInfo,jobNode),
                                                         CALLBACK(NULL,NULL),  // restoreHandleError
                                                         CALLBACK(getCryptPassword,jobNode),
                                                         &pauseFlags.restore,
                                                         &jobNode->requestedAbortFlag,
                                                         &logHandle
                                                        );
            StringList_done(&archiveFileNameList);

            // get end date/time
            endDateTime = Misc_getCurrentDateTime();

            if (jobNode->runningInfo.error != ERROR_NONE)
            {
              logMessage(&logHandle,
                         LOG_TYPE_ALWAYS,
                         "Done restore archive (error: %s)\n",
                         Error_getText(jobNode->runningInfo.error)
                        );
            }
            else
            {
              logMessage(&logHandle,
                         LOG_TYPE_ALWAYS,
                         "Done restore archive\n"
                        );
            }
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break;
          #endif /* NDEBUG */
        }
      #endif /* SIMULATOR */

      logPostProcess(&logHandle,jobNode->name,&jobNode->jobOptions,archiveType,scheduleCustomText);
    }

    // post-process command
    if (jobNode->runningInfo.error == ERROR_NONE)
    {
      if (jobNode->jobOptions.postProcessScript != NULL)
      {
        // get script
        TEXT_MACRO_N_STRING (textMacros[0],"%name",     jobNode->name,NULL);
        TEXT_MACRO_N_STRING (textMacros[1],"%archive",  storageName,NULL);
        TEXT_MACRO_N_STRING (textMacros[2],"%directory",File_getFilePathName(directory,storageSpecifier.archiveName),NULL);
        TEXT_MACRO_N_STRING (textMacros[3],"%file",     storageSpecifier.archiveName,NULL);
        script = expandTemplate(String_cString(jobNode->jobOptions.postProcessScript),
                                EXPAND_MACRO_MODE_STRING,
                                startDateTime,
                                TRUE,
                                textMacros,
                                SIZE_OF_ARRAY(textMacros)
                               );
        if (script != NULL)
        {
          // execute script
          jobNode->runningInfo.error = Misc_executeScript(String_cString(script),
                                                          CALLBACK(executeIOOutput,NULL),
                                                          CALLBACK(executeIOOutput,NULL)
                                                         );
          String_delete(script);
        }
        else
        {
          jobNode->runningInfo.error = ERROR_EXPAND_TEMPLATE;
        }

        if (jobNode->runningInfo.error != ERROR_NONE)
        {
          logMessage(&logHandle,
                     LOG_TYPE_ALWAYS,
                     "Aborted job '%s': post-command fail (error: %s)\n",
                     String_cString(jobNode->name),
                     Error_getText(jobNode->runningInfo.error)
                    );
        }
      }
    }

    // get error message
    if (jobNode->runningInfo.error != ERROR_NONE)
    {
      String_setCString(jobNode->runningInfo.message,Error_getText(jobNode->runningInfo.error));
    }

    // done log
    doneLog(&logHandle);

    SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      // free resources
      doneJobOptions(&jobOptions);
      DeltaSourceList_clear(&deltaSourceList);
      PatternList_clear(&compressExcludePatternList);
      List_clear(&mountList,CALLBACK((ListNodeFreeFunction)freeMountNode,NULL));
      PatternList_clear(&excludePatternList);
      EntryList_clear(&includeEntryList);

      // done job
      doneJob(jobNode);

      if (!jobNode->jobOptions.dryRunFlag)
      {
        // store schedule info
        writeJobScheduleInfo(jobNode);
      }
    }
  }

  // close index
  Index_close(indexHandle);

  // free resources
  String_delete(scheduleCustomText);
  DeltaSourceList_done(&deltaSourceList);
  PatternList_done(&compressExcludePatternList);
  List_done(&mountList,CALLBACK((ListNodeFreeFunction)freeMountNode,NULL));
  PatternList_done(&excludePatternList);
  EntryList_done(&includeEntryList);
  Remote_doneHost(&remoteHost);
  String_delete(hostName);
  String_delete(directory);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : delayRemoteConnectThread
* Purpose: delay remote connect thread code
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void delayRemoteConnectThread(void)
{
  uint sleepTime;

  sleepTime = 0;
  while (!quitFlag && (sleepTime < SLEEP_TIME_REMOTE_THREAD))
  {
    Misc_udelay(10LL*US_PER_SECOND);
    sleepTime += 10;
  }
}

/***********************************************************************\
* Name   : remoteConnectThreadCode
* Purpose: remote connect thread entry
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void remoteConnectThreadCode(void)
{
  typedef struct RemoteJobInfoNode
  {
    LIST_NODE_HEADER(struct RemoteJobInfoNode);

    String     jobUUID;
    RemoteHost remoteHost;

  } RemoteJobInfoNode;

  typedef struct
  {
    LIST_HEADER(RemoteJobInfoNode);
  } RemoteJobInfoList;

  StringList        jobUUIDList;
  StaticString      (jobUUID,MISC_UUID_STRING_LENGTH);
  RemoteHost        remoteHost;
  bool              tryConnectFlag;
  SemaphoreLock     semaphoreLock;

  RemoteJobInfoList remoteJobInfoList;
  RemoteJobInfoList newRemoteJobInfoList;
  JobNode           *jobNode;
  RemoteJobInfoNode *remoteJobInfoNode;
  Errors            error;
//  double            entriesPerSecond,bytesPerSecond,storageBytesPerSecond;
//  ulong             restFiles;
//  uint64            restBytes;
//  uint64            restStorageBytes;
//  ulong             estimatedRestTime;

  /***********************************************************************\
  * Name   : freeRemoteJobInfoNode
  * Purpose: free remote info node
  * Input  : remoteJobInfoNode - remote job info node
  *          userData          - not used
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  void freeRemoteJobInfoNode(RemoteJobInfoNode *remoteJobInfoNode, void *userData)
  {
    assert(remoteJobInfoNode != NULL);
    assert(remoteJobInfoNode->jobUUID != NULL);
    assert(remoteJobInfoNode->remoteHost.name != NULL);

    UNUSED_VARIABLE(userData);

    String_delete(remoteJobInfoNode->remoteHost.name);
  }

  /***********************************************************************\
  * Name   : findRemoteJobInfoByUUID
  * Purpose: find remote job info by UUID
  * Input  : jobUUID - job UUID
  * Output : -
  * Return : remote job info node or NULL if not found
  * Notes  : -
  \***********************************************************************/

  RemoteJobInfoNode *findRemoteJobInfoByUUID(ConstString jobUUID)
  {
    RemoteJobInfoNode *remoteJobInfoNode;

    LIST_ITERATE(&remoteJobInfoList,remoteJobInfoNode)
    {
      if (String_equals(remoteJobInfoNode->jobUUID,jobUUID))
      {
        return remoteJobInfoNode;
      }
    }

    return NULL;
  }

  // init variables
  StringList_init(&jobUUIDList);
  List_init(&remoteJobInfoList);
  List_init(&newRemoteJobInfoList);
  Remote_initHost(&remoteHost,serverPort);

  while (!quitFlag)
  {
    // get job UUIDs
    getAllJobUUIDs(&jobUUIDList);

    while (!StringList_isEmpty(&jobUUIDList))
    {
      StringList_getFirst(&jobUUIDList,jobUUID);

      // check if remote job and currently not connected
      tryConnectFlag = FALSE;
      SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
      {
        jobNode = findJobByUUID(jobUUID);
        if (jobNode != NULL)
        {
//fprintf(stderr,"%s, %d: id=%s %s\n",__FILE__,__LINE__,String_cString(jobUUID),String_cString(jobNode->remoteHost.name));
          if (isJobRemote(jobNode) && !Remote_isConnected(&jobNode->remoteHost))
          {
            Remote_copyHost(&remoteHost,&jobNode->remoteHost);
            tryConnectFlag = TRUE;
          }
        }
      }

      // try to connect
      if (tryConnectFlag)
      {
        (void)Remote_connect(&remoteHost);
      }
    }

    // update list of all active remote jobs and remote host info
    SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
    {
      LIST_ITERATE(&jobList,jobNode)
      {
        if (isJobRemote(jobNode) && String_isEmpty(jobNode->master) && isJobActive(jobNode))
        {
          remoteJobInfoNode = findRemoteJobInfoByUUID(jobNode->uuid);
          if (remoteJobInfoNode != NULL)
          {
//fprintf(stderr,"%s, %d: update remoteJobInfoNode %s\n",__FILE__,__LINE__,String_cString(jobNode->name));
            List_remove(&remoteJobInfoList,remoteJobInfoNode);
            Remote_copyHost(&remoteJobInfoNode->remoteHost,&jobNode->remoteHost);
          }
          else
          {
//fprintf(stderr,"%s, %d: new remoteJobInfoNode %s\n",__FILE__,__LINE__,String_cString(jobNode->name));
            remoteJobInfoNode = LIST_NEW_NODE(RemoteJobInfoNode);
            if (remoteJobInfoNode == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }
            remoteJobInfoNode->jobUUID = String_duplicate(jobNode->uuid);
            Remote_duplicateHost(&remoteJobInfoNode->remoteHost,&jobNode->remoteHost);
          }
          List_append(&newRemoteJobInfoList,remoteJobInfoNode);
        }
      }
    }
    List_done(&remoteJobInfoList,(ListNodeFreeFunction)freeRemoteJobInfoNode,NULL);
    List_move(&newRemoteJobInfoList,&remoteJobInfoList,NULL,NULL,NULL);
    assert(List_isEmpty(&newRemoteJobInfoList));

    // get remote job info
//fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
    LIST_ITERATE(&remoteJobInfoList,remoteJobInfoNode)
    {
      // get job info
//      error = Remote_executeCommand(&remoteJobInfoNode->remoteHost,resultMap,"JOB_INFO jobUUID=%S",remoteJobInfoNode->jobUUID);
      if (error == ERROR_NONE)
      {
      }
    }

    // sleep
    delayRemoteConnectThread();
  }

  // free resources
  Remote_doneHost(&remoteHost);
  List_done(&newRemoteJobInfoList,(ListNodeFreeFunction)freeRemoteJobInfoNode,NULL);
  List_done(&remoteJobInfoList,(ListNodeFreeFunction)freeRemoteJobInfoNode,NULL);
  StringList_done(&jobUUIDList);
}

/***********************************************************************\
* Name   : delayRemoteThread
* Purpose: delay remote thread code
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void delayRemoteThread(void)
{
  uint sleepTime;

  sleepTime = 0;
  while (!quitFlag && (sleepTime < SLEEP_TIME_REMOTE_THREAD))
  {
    Misc_udelay(10LL*US_PER_SECOND);
    sleepTime += 10;
  }
}

/***********************************************************************\
* Name   : remoteThreadCode
* Purpose: remote thread entry
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void remoteThreadCode(void)
{
  typedef struct RemoteJobInfoNode
  {
    LIST_NODE_HEADER(struct RemoteJobInfoNode);

    String     jobUUID;
    RemoteHost remoteHost;

  } RemoteJobInfoNode;

  typedef struct
  {
    LIST_HEADER(RemoteJobInfoNode);
  } RemoteJobInfoList;

  RemoteJobInfoList remoteJobInfoList;
  RemoteJobInfoList newRemoteJobInfoList;
  StringMap         resultMap;
  SemaphoreLock     semaphoreLock;
  JobNode           *jobNode;
  RemoteJobInfoNode *remoteJobInfoNode;
//  String            jobUUID;
  Errors            error;
//  double            entriesPerSecond,bytesPerSecond,storageBytesPerSecond;
//  ulong             restFiles;
//  uint64            restBytes;
//  uint64            restStorageBytes;
//  ulong             estimatedRestTime;

  /***********************************************************************\
  * Name   : parseJobState
  * Purpose: parse job state text
  * Input  : stateText - job state text
  *          jobState - job state variable
  * Output : jobState - job state
  * Return : always TRUE
  * Notes  : -
  \***********************************************************************/

  JobStates parseJobState(const char *stateText, uint *jobState)
  {
    assert(stateText != NULL);
    assert(jobState != NULL);

    if      (stringEqualsIgnoreCase(stateText,"-"                      )) (*jobState) = JOB_STATE_NONE;
    else if (stringEqualsIgnoreCase(stateText,"waiting"                )) (*jobState) = JOB_STATE_WAITING;
    else if (stringEqualsIgnoreCase(stateText,"dry-run"                )) (*jobState) = JOB_STATE_RUNNING;
    else if (stringEqualsIgnoreCase(stateText,"running"                )) (*jobState) = JOB_STATE_RUNNING;
    else if (stringEqualsIgnoreCase(stateText,"request FTP password"   )) (*jobState) = JOB_STATE_REQUEST_FTP_PASSWORD;
    else if (stringEqualsIgnoreCase(stateText,"request SSH password"   )) (*jobState) = JOB_STATE_REQUEST_SSH_PASSWORD;
    else if (stringEqualsIgnoreCase(stateText,"request webDAV password")) (*jobState) = JOB_STATE_REQUEST_WEBDAV_PASSWORD;
    else if (stringEqualsIgnoreCase(stateText,"request crypt password" )) (*jobState) = JOB_STATE_REQUEST_CRYPT_PASSWORD;
    else if (stringEqualsIgnoreCase(stateText,"request volume"         )) (*jobState) = JOB_STATE_REQUEST_VOLUME;
    else if (stringEqualsIgnoreCase(stateText,"done"                   )) (*jobState) = JOB_STATE_DONE;
    else if (stringEqualsIgnoreCase(stateText,"ERROR"                  )) (*jobState) = JOB_STATE_ERROR;
    else if (stringEqualsIgnoreCase(stateText,"aborted"                )) (*jobState) = JOB_STATE_ABORTED;
    else                                                                  (*jobState) = JOB_STATE_NONE;

    return TRUE;
  }

  /***********************************************************************\
  * Name   : findRemoteJobInfoByUUID
  * Purpose: find remote job info by UUID
  * Input  : jobUUID - job UUID
  * Output : -
  * Return : remote job info node or NULL if not found
  * Notes  : -
  \***********************************************************************/

  RemoteJobInfoNode *findRemoteJobInfoByUUID(ConstString jobUUID)
  {
    RemoteJobInfoNode *remoteJobInfoNode;

    LIST_ITERATE(&remoteJobInfoList,remoteJobInfoNode)
    {
      if (String_equals(remoteJobInfoNode->jobUUID,jobUUID))
      {
        return remoteJobInfoNode;
      }
    }

    return NULL;
  }

  /***********************************************************************\
  * Name   : freeRemoteJobInfoNode
  * Purpose: free remote info node
  * Input  : remoteJobInfoNode - remote job info node
  *          userData          - not used
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  void freeRemoteJobInfoNode(RemoteJobInfoNode *remoteJobInfoNode, void *userData)
  {
    assert(remoteJobInfoNode != NULL);
    assert(remoteJobInfoNode->jobUUID != NULL);
    assert(remoteJobInfoNode->remoteHost.name != NULL);

    UNUSED_VARIABLE(userData);

    String_delete(remoteJobInfoNode->remoteHost.name);
  }

  /***********************************************************************\
  * Name   : deleteRemoteJobInfoNode
  * Purpose: delete remote info node
  * Input  : remoteJobInfoNode - remote job info node
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  void deleteRemoteJobInfoNode(RemoteJobInfoNode *remoteJobInfoNode)
  {
    assert(remoteJobInfoNode != NULL);

    freeRemoteJobInfoNode(remoteJobInfoNode,NULL);
    LIST_DELETE_NODE(remoteJobInfoNode);
  }

  // init variables
  List_init(&remoteJobInfoList);
  List_init(&newRemoteJobInfoList);
  resultMap = StringMap_new();
  if (resultMap == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  while (!quitFlag)
  {
    // update list of all remote job and remote host info
    SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
    {
      LIST_ITERATE(&jobList,jobNode)
      {
        if (isJobRemote(jobNode) && String_isEmpty(jobNode->master) && isJobActive(jobNode))
        {
          remoteJobInfoNode = findRemoteJobInfoByUUID(jobNode->uuid);
          if (remoteJobInfoNode != NULL)
          {
            List_remove(&remoteJobInfoList,remoteJobInfoNode);
            Remote_copyHost(&remoteJobInfoNode->remoteHost,&jobNode->remoteHost);
          }
          else
          {
            remoteJobInfoNode = LIST_NEW_NODE(RemoteJobInfoNode);
            if (remoteJobInfoNode == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }
            remoteJobInfoNode->jobUUID = String_duplicate(jobNode->uuid);
            Remote_duplicateHost(&remoteJobInfoNode->remoteHost,&jobNode->remoteHost);
          }
          List_append(&newRemoteJobInfoList,remoteJobInfoNode);
        }
      }
    }
    List_done(&remoteJobInfoList,CALLBACK((ListNodeFreeFunction)freeRemoteJobInfoNode,NULL));
    List_exchange(&remoteJobInfoList,&newRemoteJobInfoList);
    assert(List_isEmpty(&newRemoteJobInfoList));

    // get remote job info
    LIST_ITERATE(&remoteJobInfoList,remoteJobInfoNode)
    {
      // get job info
      error = Remote_executeCommand(&remoteJobInfoNode->remoteHost,resultMap,"JOB_INFO jobUUID=%S",remoteJobInfoNode->jobUUID);
      if (error == ERROR_NONE)
      {
        SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
        {
          // find job
          jobNode = findJobByUUID(remoteJobInfoNode->jobUUID);
          if (jobNode != NULL)
          {
            // parse and store
            StringMap_getEnum  (resultMap,"state",                &jobNode->state,(StringMapParseEnumFunction)parseJobState,JOB_STATE_NONE);
            StringMap_getULong (resultMap,"doneCount",            &jobNode->runningInfo.doneCount,0L);
            StringMap_getUInt64(resultMap,"doneSize",             &jobNode->runningInfo.doneSize,0LL);
            StringMap_getULong (resultMap,"totalEntryCount",      &jobNode->runningInfo.totalEntryCount,0L);
            StringMap_getUInt64(resultMap,"totalEntrySize",       &jobNode->runningInfo.totalEntrySize,0LL);
            StringMap_getBool  (resultMap,"collectTotalSumDone",  &jobNode->runningInfo.collectTotalSumDone,FALSE);
            StringMap_getULong (resultMap,"skippedEntryCount",    &jobNode->runningInfo.skippedEntryCount,0L);
            StringMap_getUInt64(resultMap,"skippedEntrySize",     &jobNode->runningInfo.skippedEntrySize,0LL);
            StringMap_getULong (resultMap,"errorEntryCount",      &jobNode->runningInfo.errorEntryCount,0L);
            StringMap_getUInt64(resultMap,"errorEntrySize",       &jobNode->runningInfo.errorEntrySize,0LL);
            StringMap_getDouble(resultMap,"entriesPerSecond",     &jobNode->runningInfo.entriesPerSecond,0.0);
            StringMap_getDouble(resultMap,"bytesPerSecond",       &jobNode->runningInfo.bytesPerSecond,0.0);
            StringMap_getDouble(resultMap,"storageBytesPerSecond",&jobNode->runningInfo.storageBytesPerSecond,0.0);
            StringMap_getUInt64(resultMap,"archiveSize",          &jobNode->runningInfo.archiveSize,0LL);
            StringMap_getDouble(resultMap,"compressionRatio",     &jobNode->runningInfo.compressionRatio,0.0);
            StringMap_getULong (resultMap,"estimatedRestTime",    &jobNode->runningInfo.estimatedRestTime,0L);
            StringMap_getString(resultMap,"entryName",            jobNode->runningInfo.entryName,NULL);
            StringMap_getUInt64(resultMap,"entryDoneSize",        &jobNode->runningInfo.entryDoneSize,0LL);
            StringMap_getUInt64(resultMap,"entryTotalSize",       &jobNode->runningInfo.entryTotalSize,0LL);
            StringMap_getString(resultMap,"storageName",          jobNode->runningInfo.storageName,NULL);
            StringMap_getUInt64(resultMap,"storageDoneSize",      &jobNode->runningInfo.storageDoneSize,0L);
            StringMap_getUInt64(resultMap,"storageTotalSize",     &jobNode->runningInfo.storageTotalSize,0L);
            StringMap_getUInt  (resultMap,"volumeNumber",         &jobNode->runningInfo.volumeNumber,0);
            StringMap_getDouble(resultMap,"volumeProgress",       &jobNode->runningInfo.volumeProgress,0.0);
            StringMap_getString(resultMap,"message",              jobNode->runningInfo.message,NULL);
          }
        }
      }
    }

    // sleep
    delayRemoteThread();
  }

  // free resources
  StringMap_delete(resultMap);
  List_done(&newRemoteJobInfoList,CALLBACK((ListNodeFreeFunction)freeRemoteJobInfoNode,NULL));
  List_done(&remoteJobInfoList,CALLBACK((ListNodeFreeFunction)freeRemoteJobInfoNode,NULL));
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : deleteStorage
* Purpose: delete storage
* Input  : indexHandle - index handle
*          storageId   - storage to delete
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors deleteStorage(IndexHandle *indexHandle, IndexId storageId)
{
  Errors           resultError;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  String           storageName;
  SemaphoreLock    semaphoreLock;
  const JobNode    *jobNode;
  StorageSpecifier storageSpecifier;
  Errors           error;
  StorageHandle    storageHandle;

  assert(indexHandle != NULL);

  // init variables
  resultError = ERROR_UNKNOWN;
  storageName = String_new();

  // find storage
  if (!Index_findStorageById(indexHandle,
                             storageId,
                             jobUUID,
                             NULL,  // scheduleUUID
                             NULL,  // uuidId
                             NULL,  // entityId
                             storageName,
                             NULL,  // indexState
                             NULL  // lastCheckedDateTime
                            )
     )
  {
    String_delete(storageName);
    return ERROR_DATABASE_INDEX_NOT_FOUND;
  }

  if (!String_isEmpty(storageName))
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
    {
      // find job if possible
      jobNode = findJobByUUID(jobUUID);

      // delete storage file
      Storage_initSpecifier(&storageSpecifier);
      resultError = Storage_parseName(&storageSpecifier,storageName);
      if (resultError == ERROR_NONE)
      {
#ifndef WERROR
#warning NYI: move this special handling of limited scp into Storage_delete()?
#endif
        // init storage
        if (storageSpecifier.type == STORAGE_TYPE_SCP)
        {
          // try to init scp-storage first with sftp
          storageSpecifier.type = STORAGE_TYPE_SFTP;
          resultError = Storage_init(&storageHandle,
                                     &storageSpecifier,
                                     (jobNode != NULL) ? &jobNode->jobOptions : NULL,
                                     &globalOptions.indexDatabaseMaxBandWidthList,
                                     SERVER_CONNECTION_PRIORITY_HIGH,
                                     CALLBACK(NULL,NULL),  // updateStatusInfo
                                     CALLBACK(NULL,NULL),  // getPassword
                                     CALLBACK(NULL,NULL)  // requestVolume
                                    );
          if (resultError != ERROR_NONE)
          {
            // init scp-storage
            storageSpecifier.type = STORAGE_TYPE_SCP;
            resultError = Storage_init(&storageHandle,
                                       &storageSpecifier,
                                       (jobNode != NULL) ? &jobNode->jobOptions : NULL,
                                       &globalOptions.indexDatabaseMaxBandWidthList,
                                       SERVER_CONNECTION_PRIORITY_HIGH,
                                       CALLBACK(NULL,NULL),  // updateStatusInfo
                                       CALLBACK(NULL,NULL),  // getPassword
                                       CALLBACK(NULL,NULL)  // requestVolume
                                      );
          }
        }
        else
        {
          // init other storage types
          resultError = Storage_init(&storageHandle,
                                     &storageSpecifier,
                                     (jobNode != NULL) ? &jobNode->jobOptions : NULL,
                                     &globalOptions.indexDatabaseMaxBandWidthList,
                                     SERVER_CONNECTION_PRIORITY_HIGH,
                                     CALLBACK(NULL,NULL),  // updateStatusInfo
                                     CALLBACK(NULL,NULL),  // getPassword
                                     CALLBACK(NULL,NULL)  // requestVolume
                                    );
        }
        if (resultError == ERROR_NONE)
        {
          if (Storage_exists(&storageHandle,
                             NULL  // archiveName
                            )
             )
          {
            // delete storage
            resultError = Storage_delete(&storageHandle,
                                         NULL  // archiveName
                                        );
          }

          // prune empty directories
          Storage_pruneDirectories(&storageHandle,
                                   NULL  // archiveName
                                  );

          // close storage
          Storage_done(&storageHandle);
        }
      }
      Storage_doneSpecifier(&storageSpecifier);
    }
  }

  // delete index
  error = Index_deleteStorage(indexHandle,storageId);
  if (error != ERROR_NONE)
  {
    resultError = error;
  }

  // free resources
  String_delete(storageName);

  return resultError;
}

/***********************************************************************\
* Name   : deleteEntity
* Purpose: delete entity index and all attached storage files
* Input  : indexHandle - index handle
*          entityId    - database id of entity
* Output : -
* Return : ERROR_NONE or error code
* Notes  : No error is reported if a storage file cannot be deleted
\***********************************************************************/

LOCAL Errors deleteEntity(IndexHandle *indexHandle,
                          IndexId     entityId
                         )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;

  assert(indexHandle != NULL);

  // delete all storage with entity id
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 entityId,
                                 NULL,  // jobUUID
                                 NULL,  // indexIds
                                 0,  // indexIdCount
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 INDEX_STORAGE_SORT_MODE_NONE,
                                 DATABASE_ORDERING_NONE,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (   !quitFlag
         && !isSomeJobActive()
         && Index_getNextStorage(&indexQueryHandle,
                                 NULL,  // uuidId
                                 NULL,  // jobUUID
                                 NULL,  // entityId
                                 NULL,  // scheduleUUID
                                 NULL,  // archiveType
                                 &storageId,
                                 NULL,  // storageName
                                 NULL,  // createdDateTime
                                 NULL,  // totalEntryCount
                                 NULL,  // totalEntrySize
                                 NULL,  // indexState
                                 NULL,  // indexMode
                                 NULL,  // lastCheckedDateTime
                                 NULL  // errorMessage
                                )
        )
  {
    (void)deleteStorage(indexHandle,storageId);
  }
  Index_doneList(&indexQueryHandle);
  if (quitFlag || isSomeJobActive())
  {
    return ERROR_INTERRUPTED;
  }

  // delete entity index
  error = Index_deleteEntity(indexHandle,entityId);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : deleteUUID
* Purpose: delete all entities of UUID and all attached storage files
* Input  : indexHandle - index handle
*          entityId    - database id of entity
* Output : -
* Return : ERROR_NONE or error code
* Notes  : No error is reported if a storage file cannot be deleted
\***********************************************************************/

LOCAL Errors deleteUUID(IndexHandle *indexHandle,
                        ConstString jobUUID
                       )
{
  Errors           error;
  IndexId          uuidId;
  IndexQueryHandle indexQueryHandle;
  IndexId          entityId;

  assert(indexHandle != NULL);

  // find UUID
  if (!Index_findUUIDByJobUUID(indexHandle,
                               jobUUID,
                               &uuidId,
                               NULL,  // lastCreatedDateTime,
                               NULL,  // lastErrorMessage,
                               NULL,  // executionCount,
                               NULL,  // averageDuration,
                               NULL,  // totalEntityCount,
                               NULL,  // totalStorageCount,
                               NULL,  // totalStorageSize,
                               NULL,  // totalEntryCount,
                               NULL  // totalEntrySize
                              )
     )
  {
//TODO: which error?
    return ERROR_DATABASE_INDEX_NOT_FOUND;
  }

  // delete all entities with uuid id
  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 uuidId,
                                 NULL,  // jobUUID
                                 NULL,  // scheduleId,
                                 NULL,  // name
                                 DATABASE_ORDERING_NONE,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (   !quitFlag
         && !isSomeJobActive()
         && Index_getNextEntity(&indexQueryHandle,
                                NULL,  // uuidId
                                NULL,  // jobUUID
                                NULL,  // scheduleUUID
                                &entityId,
                                NULL,  // archiveType
                                NULL,  // createdDateTime
                                NULL,  // lastErrorMessage
                                NULL,  // entries
                                NULL  // size
                               )
        )
  {
    (void)deleteEntity(indexHandle,entityId);
  }
  Index_doneList(&indexQueryHandle);
  if (quitFlag || isSomeJobActive())
  {
    return ERROR_INTERRUPTED;
  }

  // delete UUID
  error = Index_deleteUUID(indexHandle,uuidId);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : delayScheduleThread
* Purpose: delay scheduler code
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void delayScheduleThread(void)
{
  uint sleepTime;

  sleepTime = 0;
  while (!quitFlag && (sleepTime < SLEEP_TIME_SCHEDULER_THREAD))
  {
    Misc_udelay(10LL*US_PER_SECOND);
    sleepTime += 10;
  }
}

/***********************************************************************\
* Name   : schedulerThreadCode
* Purpose: schedule thread entry
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void schedulerThreadCode(void)
{
  IndexHandle  *indexHandle;
  JobNode      *jobNode;
  uint64       currentDateTime;
  uint64       dateTime;
  uint         year,month,day,hour,minute;
  WeekDays     weekDay;
  ScheduleNode *executeScheduleNode;
  ScheduleNode *scheduleNode;
  bool         pendingFlag;

  // init index
  indexHandle = Index_open(INDEX_PRIORITY_MEDIUM,INDEX_TIMEOUT);

  while (!quitFlag)
  {
    // update job files
    updateAllJobs();

    // re-read job config files
    rereadAllJobs(serverJobsDirectory);

    // check for jobs triggers
    pendingFlag = FALSE;
    Semaphore_lock(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT);
    {
      currentDateTime = Misc_getCurrentDateTime();
//TODO: LIST_ITERATEX()
      jobNode         = jobList.head;
      while (!quitFlag && !pendingFlag && (jobNode != NULL))
      {
        if (!isJobActive(jobNode))
        {
          executeScheduleNode = NULL;

          // check if job have to be executed by regular schedule (check backward in time)
          if (executeScheduleNode == NULL)
          {
            dateTime = currentDateTime;
            while (   !pendingFlag
                   && !quitFlag
                   && ((dateTime/60LL) > (jobNode->lastCheckDateTime/60LL))
                   && ((dateTime/60LL) > (jobNode->lastExecutedDateTime/60LL))
                   && (executeScheduleNode == NULL)
                  )
            {
              // get date/time values
              Misc_splitDateTime(dateTime,
                                 &year,
                                 &month,
                                 &day,
                                 &hour,
                                 &minute,
                                 NULL,
                                 &weekDay
                                );

              // check if matching with some schedule list node
              scheduleNode = jobNode->scheduleList.head;
              while ((scheduleNode != NULL) && (executeScheduleNode == NULL))
              {
                if (   scheduleNode->enabled
                    && (scheduleNode->archiveType != ARCHIVE_TYPE_CONTINUOUS)
                    && ((scheduleNode->date.year     == DATE_ANY       ) || (scheduleNode->date.year   == (int)year  ))
                    && ((scheduleNode->date.month    == DATE_ANY       ) || (scheduleNode->date.month  == (int)month ))
                    && ((scheduleNode->date.day      == DATE_ANY       ) || (scheduleNode->date.day    == (int)day   ))
                    && ((scheduleNode->weekDaySet    == WEEKDAY_SET_ANY) || IN_SET(scheduleNode->weekDaySet,weekDay)  )
                    && ((scheduleNode->time.hour     == TIME_ANY       ) || (scheduleNode->time.hour   == (int)hour  ))
                    && ((scheduleNode->time.minute   == TIME_ANY       ) || (scheduleNode->time.minute == (int)minute))
                   )
                {
                  executeScheduleNode = scheduleNode;
                }
                scheduleNode = scheduleNode->next;
              }

              // check if another thread is pending for job list
              pendingFlag = Semaphore_isLockPending(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);

              // next time
              dateTime -= 60LL;
            }
          }

          // check if job have to be executed by continuous schedule
          if (executeScheduleNode == NULL)
          {
            // get date/time values
            Misc_splitDateTime(currentDateTime,
                               &year,
                               &month,
                               &day,
                               &hour,
                               &minute,
                               NULL,
                               &weekDay
                              );

            // check if matching with some schedule list node
            scheduleNode = jobNode->scheduleList.head;
            while ((scheduleNode != NULL) && (executeScheduleNode == NULL))
            {
              if (   scheduleNode->enabled
                  && (scheduleNode->archiveType == ARCHIVE_TYPE_CONTINUOUS)
                  && ((scheduleNode->date.year     == DATE_ANY       ) || (scheduleNode->date.year   == (int)year  ))
                  && ((scheduleNode->date.month    == DATE_ANY       ) || (scheduleNode->date.month  == (int)month ))
                  && ((scheduleNode->date.day      == DATE_ANY       ) || (scheduleNode->date.day    == (int)day   ))
                  && ((scheduleNode->weekDaySet    == WEEKDAY_SET_ANY) || IN_SET(scheduleNode->weekDaySet,weekDay)  )
                  && ((scheduleNode->time.hour     == TIME_ANY       ) || (scheduleNode->time.hour   == (int)hour  ))
                  && ((scheduleNode->time.minute   == TIME_ANY       ) || (scheduleNode->time.minute == (int)minute))
                  && (currentDateTime >= (jobNode->lastExecutedDateTime + (uint64)scheduleNode->interval*60LL))
                  && Continuous_isAvailable(jobNode->uuid,scheduleNode->uuid)
                 )
              {
                executeScheduleNode = scheduleNode;
              }
//fprintf(stderr,"%s, %d: check %s %llu %llu -> %llu: scheduleNode %d %d %p\n",__FILE__,__LINE__,String_cString(jobNode->name),currentDateTime,jobNode->lastExecutedDateTime,currentDateTime-jobNode->lastExecutedDateTime,scheduleNode->archiveType,scheduleNode->interval,executeScheduleNode);
              scheduleNode = scheduleNode->next;
            }
          }

          // check for quit
          if (quitFlag)
          {
            break;
          }

          // trigger job
          if (executeScheduleNode != NULL)
          {
            triggerJob(jobNode,
                       executeScheduleNode->archiveType,
                       FALSE,
                       executeScheduleNode->uuid,
                       executeScheduleNode->customText,
                       executeScheduleNode->noStorage
                      );
          }

          // store last check time
          jobNode->lastCheckDateTime = currentDateTime;

          // check if another thread is pending for job list
          pendingFlag = Semaphore_isLockPending(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);
        }

        // next job
        jobNode = jobNode->next;
      }
    }
    Semaphore_unlock(&jobList.lock);

    if (!pendingFlag)
    {
      // sleep
      delayScheduleThread();
    }
    else
    {
      // short sleep
      Misc_udelay(1LL*US_PER_SECOND);
    }
  }

  // done index
  Index_close(indexHandle);
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : pauseThreadCode
* Purpose: pause thread entry
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void pauseThreadCode(void)
{
  SemaphoreLock semaphoreLock;
  uint          sleepTime;

  while (!quitFlag)
  {
    // decrement pause time, continue
    SEMAPHORE_LOCKED_DO(semaphoreLock,&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      if (serverState == SERVER_STATE_PAUSE)
      {
        if (Misc_getCurrentDateTime() > pauseEndDateTime)
        {
          serverState            = SERVER_STATE_RUNNING;
          pauseFlags.create      = FALSE;
          pauseFlags.storage     = FALSE;
          pauseFlags.restore     = FALSE;
          pauseFlags.indexUpdate = FALSE;
        }
      }
    }

    // sleep, check update and quit flag
    sleepTime = 0;
    while (!quitFlag && (sleepTime < SLEEP_TIME_PAUSE_THREAD))
    {
      Misc_udelay(10LL*US_PER_SECOND);
      sleepTime += 10;
    }
  }
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : delayPurgeExpiredThread
* Purpose: delay purge expired code
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void delayPurgeExpiredThread(void)
{
  uint sleepTime;

  sleepTime = 0;
  while (!quitFlag && (sleepTime < SLEEP_TIME_PURGE_EXPIRED_THREAD))
  {
    Misc_udelay(10LL*US_PER_SECOND);
    sleepTime += 10;
  }
}

/***********************************************************************\
* Name   : purgeExpiredThreadCode
* Purpose: purge expired thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void purgeExpiredThreadCode(void)
{
  const uint64 SECONDS_PER_DAY = 24*60*60;

  String             jobName;
  String             string;
  uint64             now;
  IndexHandle        *indexHandle;
  Errors             error;
  IndexQueryHandle   indexQueryHandle1,indexQueryHandle2;
  IndexId            entityId;
  StaticString       (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString       (scheduleUUID,MISC_UUID_STRING_LENGTH);
  uint               minKeep,maxKeep,maxAge;
  SemaphoreLock      semaphoreLock;
  const JobNode      *jobNode;
  const ScheduleNode *scheduleNode;
  uint64             createdDateTime;
  ArchiveTypes       archiveType;
  ulong              totalEntryCount;
  uint64             totalEntrySize;

  // init variables
  jobName = String_new();
  string  = String_new();
  now     = Misc_getCurrentDateTime();

  // init index
  indexHandle = Index_open(INDEX_PRIORITY_MEDIUM,INDEX_TIMEOUT);
  if (indexHandle == NULL)
  {
    plogMessage(NULL,  // logHandle,
                LOG_TYPE_INDEX,
                "INDEX",
                "Open index database fail - disabled purge expired\n"
               );
    return;
  }

  while (!quitFlag)
  {
    // check entities
    error = Index_initListEntities(&indexQueryHandle1,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   NULL,  // jobUUID
                                   NULL,  // scheduldUUID,
                                   NULL,  // name
                                   DATABASE_ORDERING_ASCENDING,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error == ERROR_NONE)
    {
      while (   !quitFlag
             && !isSomeJobActive()
             && Index_getNextEntity(&indexQueryHandle1,
                                    NULL,  // uudId,
                                    jobUUID,
                                    scheduleUUID,
                                    &entityId,
                                    NULL,  // archiveType,
                                    NULL,  // createdDateTime,
                                    NULL,  // lastErrorMessage
                                    NULL,  // totalEntryCount,
                                    NULL  // totalEntrySize,
                                   )
            )
      {
        if (!String_isEmpty(scheduleUUID))
        {
          // get job name, schedule min./max. keep, max. age
          minKeep = 0;
          maxKeep = 0;
          maxAge  = 0;
          SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
          {
            jobNode = findJobByUUID(jobUUID);
            if (jobNode != NULL)
            {
              String_set(jobName,jobNode->name);
            }
            else
            {
              String_clear(jobName);
            }
            scheduleNode = findScheduleByUUID(jobNode,scheduleUUID);
            if (scheduleNode != NULL)
            {
              minKeep = scheduleNode->minKeep;
              maxKeep = scheduleNode->maxKeep;
              maxAge  = scheduleNode->maxAge;
            }
          }

          // check if expired
          if ((maxKeep > 0) || (maxAge > 0))
          {
            // delete expired entities
            if (maxAge > 0)
            {
              error = Index_initListEntities(&indexQueryHandle2,
                                             indexHandle,
                                             INDEX_ID_ANY,  // uuidId
                                             jobUUID,
                                             scheduleUUID,
                                             NULL,  // name
                                             DATABASE_ORDERING_DESCENDING,
                                             (ulong)minKeep,
                                             INDEX_UNLIMITED
                                            );
              if (error == ERROR_NONE)
              {
                while (   !quitFlag
                       && !isSomeJobActive()
                       && Index_getNextEntity(&indexQueryHandle2,
                                              NULL,  // uudId,
                                              NULL,  // jobUUID
                                              NULL,  // scheduleUUID
                                              &entityId,
                                              &archiveType,
                                              &createdDateTime,
                                              NULL,  // lastErrorMessage
                                              &totalEntryCount,
                                              &totalEntrySize
                                             )
                      )
                {
                  if (now > (createdDateTime+maxAge*SECONDS_PER_DAY))
                  {
                    error = deleteEntity(indexHandle,entityId);
                    if (error == ERROR_NONE)
                    {
                      plogMessage(NULL,  // logHandle,
                                  LOG_TYPE_INDEX,
                                  "INDEX",
                                  "Purged expired entity of job '%s': %s, created at %s, %llu entries/%.1f%s (%llu bytes)\n",
                                  String_cString(jobName),
                                  ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,archiveType,"normal"),
                                  String_cString(Misc_formatDateTime(string,createdDateTime,NULL)),
                                  totalEntryCount,
                                  BYTES_SHORT(totalEntrySize),
                                  BYTES_UNIT(totalEntrySize),
                                  totalEntrySize
                                 );
                    }
                  }
                }
                Index_doneList(&indexQueryHandle2);
              }
            }

            // delete surplus entities
            if ((maxKeep > 0) && (maxKeep >= minKeep))
            {
              error = Index_initListEntities(&indexQueryHandle2,
                                             indexHandle,
                                             INDEX_ID_ANY,  // uuidId
                                             jobUUID,
                                             scheduleUUID,
                                             NULL,  // name
                                             DATABASE_ORDERING_DESCENDING,
                                             (ulong)maxKeep,
                                             INDEX_UNLIMITED
                                            );
              if (error == ERROR_NONE)
              {
                while (   !quitFlag
                       && !isSomeJobActive()
                       && Index_getNextEntity(&indexQueryHandle2,
                                              NULL,  // uudId,
                                              NULL,  // jobUUID
                                              NULL,  // scheduleUUID
                                              &entityId,
                                              &archiveType,
                                              &createdDateTime,
                                              NULL,  // lastErrorMessage
                                              &totalEntryCount,
                                              &totalEntrySize
                                             )
                      )
                {
                  error = deleteEntity(indexHandle,entityId);
                  if (error == ERROR_NONE)
                  {
                    plogMessage(NULL,  // logHandle,
                                LOG_TYPE_INDEX,
                                "INDEX",
                                "Purged surplus entity of job '%s': %s, created at %s, %llu entries/%.1f%s (%llu bytes)\n",
                                String_cString(jobName),
                                ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,archiveType,"normal"),
                                String_cString(Misc_formatDateTime(string,createdDateTime,NULL)),
                                totalEntryCount,
                                BYTES_SHORT(totalEntrySize),
                                BYTES_UNIT(totalEntrySize),
                                totalEntrySize
                               );
                  }
                }
                Index_doneList(&indexQueryHandle2);
              }
            }
          }
        }
      }
      Index_doneList(&indexQueryHandle1);
    }

    // sleep
    delayScheduleThread();
  }

  // done index
  Index_close(indexHandle);

  // free resources
  String_delete(string);
  String_delete(jobName);
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : addIndexCryptPasswordNode
* Purpose: add crypt password to index crypt password list
* Input  : indexCryptPasswordList  - index crypt password list
*          cryptPassword           - crypt password
*          cryptPrivateKeyFileName - crypt private key file name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addIndexCryptPasswordNode(IndexCryptPasswordList *indexCryptPasswordList, Password *cryptPassword, String cryptPrivateKeyFileName)
{
  IndexCryptPasswordNode *indexCryptPasswordNode;

  indexCryptPasswordNode = LIST_NEW_NODE(IndexCryptPasswordNode);
  if (indexCryptPasswordNode == NULL)
  {
    return;
  }

  indexCryptPasswordNode->cryptPassword           = Password_duplicate(cryptPassword);
  indexCryptPasswordNode->cryptPrivateKeyFileName = String_duplicate(cryptPrivateKeyFileName);

  List_append(indexCryptPasswordList,indexCryptPasswordNode);
}

/***********************************************************************\
* Name   : freeIndexCryptPasswordNode
* Purpose: free index crypt password
* Input  : indexCryptPasswordNode - crypt password node
*          userData               - user data (ignored)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeIndexCryptPasswordNode(IndexCryptPasswordNode *indexCryptPasswordNode, void *userData)
{
  assert(indexCryptPasswordNode != NULL);

  UNUSED_VARIABLE(userData);

  if (indexCryptPasswordNode->cryptPrivateKeyFileName != NULL) String_delete(indexCryptPasswordNode->cryptPrivateKeyFileName);
  if (indexCryptPasswordNode->cryptPassword != NULL) Password_delete(indexCryptPasswordNode->cryptPassword);
}

/***********************************************************************\
* Name   : indexPauseCallback
* Purpose: check if pause
* Input  : userData - not used
* Output : -
* Return : TRUE on pause, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool indexPauseCallback(void *userData)
{
  UNUSED_VARIABLE(userData);

  return pauseFlags.indexUpdate;
}

/***********************************************************************\
* Name   : indexAbortCallback
* Purpose: check if abort
* Input  : userData - not used
* Output : -
* Return : TRUE on create/restore/quit, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool indexAbortCallback(void *userData)
{
  UNUSED_VARIABLE(userData);

  return quitFlag;
}

/***********************************************************************\
* Name   : pauseIndexUpdate
* Purpose: pause index update
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void pauseIndexUpdate(void)
{
  while (   pauseFlags.indexUpdate
         && !quitFlag
        )
  {
    Misc_udelay(500L*1000L);
  }
}

/***********************************************************************\
* Name   : delayIndexThread
* Purpose: delay index thread code
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void delayIndexThread(void)
{
  SemaphoreLock semaphoreLock;
  uint          sleepTime;

  sleepTime = 0;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&indexThreadTrigger,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    while (   !quitFlag
           && (sleepTime < SLEEP_TIME_INDEX_THREAD)
           && !Semaphore_waitModified(&indexThreadTrigger,10*MS_PER_SECOND)
          )
    {
      Misc_udelay(10LL*US_PER_SECOND);
      sleepTime += 10;
    }
fprintf(stderr,"%s, %d: delayIndexThread DOOOOOOOOOOOOOOOOO\n",__FILE__,__LINE__);
  }
}

/***********************************************************************\
* Name   : indexThreadCode
* Purpose: index thread entry
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void indexThreadCode(void)
{
  IndexHandle            *indexHandle;
  IndexId                storageId;
  StorageSpecifier       storageSpecifier;
  String                 storageName,printableStorageName;
  StorageHandle          storageHandle;
  IndexCryptPasswordList indexCryptPasswordList;
  SemaphoreLock          semaphoreLock;
  JobOptions             jobOptions;
  uint64                 startTimestamp,endTimestamp;
  Errors                 error;
  JobNode                *jobNode;
  IndexCryptPasswordNode *indexCryptPasswordNode;
  uint64                 totalTimeLastChanged;
  uint64                 totalEntryCount,totalEntrySize;

  // initialize variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();
  List_init(&indexCryptPasswordList);

  // init index
  indexHandle = Index_open(INDEX_PRIORITY_MEDIUM,INDEX_TIMEOUT);
  if (indexHandle == NULL)
  {
    List_done(&indexCryptPasswordList,CALLBACK((ListNodeFreeFunction)freeIndexCryptPasswordNode,NULL));
    String_delete(printableStorageName);
    String_delete(storageName);
    Storage_doneSpecifier(&storageSpecifier);

    plogMessage(NULL,  // logHandle,
                LOG_TYPE_INDEX,
                "INDEX",
                "Open index database fail - disabled index update\n"
               );
    return;
  }

  // add/update index database
  while (!quitFlag)
  {
    // pause
    pauseIndexUpdate();

    // get all job crypt passwords and crypt private keys (including no password and default crypt password)
    addIndexCryptPasswordNode(&indexCryptPasswordList,NULL,NULL);
    addIndexCryptPasswordNode(&indexCryptPasswordList,globalOptions.cryptPassword,NULL);
    SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
    {
      LIST_ITERATE(&jobList,jobNode)
      {
        if (jobNode->jobOptions.cryptPassword != NULL)
        {
          addIndexCryptPasswordNode(&indexCryptPasswordList,jobNode->jobOptions.cryptPassword,jobNode->jobOptions.cryptPrivateKeyFileName);
        }
      }
    }

    // update index entries
    while (   !quitFlag
           && Index_findStorageByState(indexHandle,
                                       INDEX_STATE_SET(INDEX_STATE_UPDATE_REQUESTED),
                                       NULL,  // uuidId
                                       NULL,  // jobUUID
                                       NULL,  // entityId
                                       NULL,  // scheduleUUID
                                       &storageId,
                                       storageName,
                                       NULL  // lastCheckedDateTime
                                      )
          )
    {
      // pause
      pauseIndexUpdate();

      // parse storage name, get printable name
      error = Storage_parseName(&storageSpecifier,storageName);
      if (error == ERROR_NONE)
      {
        String_set(printableStorageName,Storage_getPrintableName(&storageSpecifier,NULL));
      }
      else
      {
        String_set(printableStorageName,storageName);
      }
//fprintf(stderr,"%s, %d: fileName=%s\n",__FILE__,__LINE__,String_cString(storageSpecifier.fileName));

      // init storage
      startTimestamp = 0LL;
      endTimestamp   = 0LL;
      initJobOptions(&jobOptions);
      error = Storage_init(&storageHandle,
                           &storageSpecifier,
                           &jobOptions,
                           &globalOptions.indexDatabaseMaxBandWidthList,
                           SERVER_CONNECTION_PRIORITY_LOW,
                           CALLBACK(NULL,NULL),  // updateStatusInfo
                           CALLBACK(NULL,NULL),  // getPassword
                           CALLBACK(NULL,NULL)  // requestVolume
                          );
      if (error == ERROR_NONE)
      {
        // try to create index
        LIST_ITERATE(&indexCryptPasswordList,indexCryptPasswordNode)
        {
          // index update
#ifndef WERROR
#warning todo init?
#endif
          startTimestamp = Misc_getTimestamp();
          jobOptions.cryptPassword           = Password_duplicate(indexCryptPasswordNode->cryptPassword);
          jobOptions.cryptPrivateKeyFileName = String_duplicate(indexCryptPasswordNode->cryptPrivateKeyFileName);
          error = Archive_updateIndex(indexHandle,
                                      storageId,
                                      &storageHandle,
                                      storageName,
                                      &jobOptions,
                                      &totalTimeLastChanged,
                                      &totalEntryCount,
                                      &totalEntrySize,
                                      CALLBACK(indexPauseCallback,NULL),
                                      CALLBACK(indexAbortCallback,NULL),
                                      NULL  // logHandle
                                     );
          endTimestamp = Misc_getTimestamp();

          // stop if done, interrupted, or quit
          if (   (error == ERROR_NONE)
              || (Error_getCode(error) == ERROR_INTERRUPTED)
              || quitFlag
             )
          {
            break;
          }
        }

        // done storage
        (void)Storage_done(&storageHandle);
      }
      else
      {
        Index_setState(indexHandle,
                       storageId,
                       INDEX_STATE_ERROR,
                       0LL,
                       "Cannot initialise storage (error: %s)",
                       Error_getText(error)
                      );
      }
      doneJobOptions(&jobOptions);

      if (!quitFlag)
      {
        if (error == ERROR_NONE)
        {
          plogMessage(NULL,  // logHandle,
                      LOG_TYPE_INDEX,
                      "INDEX",
                      "Created index for '%s', %llu entries/%.1f%s (%llu bytes), %lus\n",
                      String_cString(printableStorageName),
                      totalEntryCount,
                      BYTES_SHORT(totalEntrySize),
                      BYTES_UNIT(totalEntrySize),
                      totalEntrySize,
                      (endTimestamp-startTimestamp)/US_PER_SECOND
                     );
        }
        else if (Error_getCode(error) == ERROR_INTERRUPTED)
        {
#ifndef WERROR
#warning needed?
#endif
#if 0
          plogMessage(NULL,  // logHandle,
                      LOG_TYPE_INDEX,
                      "INDEX",
                      "Interrupted create index for '%s' - postpone\n",
                      String_cString(printableStorageName),
                      Error_getText(error)
                     );
#endif
        }
        else
        {
          plogMessage(NULL,  // logHandle,
                      LOG_TYPE_INDEX,
                      "INDEX",
                      "Cannot create index for '%s' (error: %s)\n",
                      String_cString(printableStorageName),
                      Error_getText(error)
                     );
        }
      }
    }

    // free resources
    List_done(&indexCryptPasswordList,(ListNodeFreeFunction)freeIndexCryptPasswordNode,NULL);

    // sleep, check quit flag
    delayIndexThread();
  }

  // done index
  Index_close(indexHandle);

  // free resources
  List_done(&indexCryptPasswordList,CALLBACK((ListNodeFreeFunction)freeIndexCryptPasswordNode,NULL));
  String_delete(printableStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);
}

/***********************************************************************\
* Name   : getStorageDirectories
* Purpose: get list of storage directories from jobs
* Input  : storageDirectoryList - storage directory list
* Output : storageDirectoryList - updated storage directory list
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void getStorageDirectories(StringList *storageDirectoryList)
{
  String        storagePathName;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  // collect storage locations to check for BAR files
  storagePathName = String_new();
  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    LIST_ITERATE(&jobList,jobNode)
    {
      File_getFilePathName(storagePathName,jobNode->archiveName);
      if (!String_isEmpty(storagePathName))
      {
        if (StringList_find(storageDirectoryList,storagePathName) == NULL)
        {
          StringList_append(storageDirectoryList,storagePathName);
        }
      }
    }
  }
  String_delete(storagePathName);
}

/***********************************************************************\
* Name   : delayAutoIndexThread
* Purpose: delay auto index thread code
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void delayAutoIndexThread(void)
{
  uint sleepTime;

  sleepTime = 0;
  while (!quitFlag && (sleepTime < SLEEP_TIME_AUTO_INDEX_UPDATE_THREAD))
  {
    Misc_udelay(10LL*US_PER_SECOND);
    sleepTime += 10;
  }
}

/***********************************************************************\
* Name   : autoIndexThreadCode
* Purpose: auto index thread entry
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void autoIndexThreadCode(void)
{
  IndexHandle                *indexHandle;
  StringList                 storageDirectoryList;
  StorageSpecifier           storageSpecifier;
  String                     baseName;
  String                     pattern;
  String                     printableStorageName;
  String                     storageName;
  String                     storageDirectoryName;
  JobOptions                 jobOptions;
  Errors                     error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  IndexId                    storageId;
  uint64                     createdDateTime;
  IndexStates                indexState;
  uint64                     lastCheckedDateTime;
  uint64                     now;
  String                     string;
  IndexQueryHandle           indexQueryHandle;
  IndexModes                 indexMode;

  // initialize variables
  StringList_init(&storageDirectoryList);
  Storage_initSpecifier(&storageSpecifier);
  baseName             = String_new();
  pattern              = String_new();
  printableStorageName = String_new();
  storageName          = String_new();

  // init index (Note: timeout not important; auto-index should not block)
  indexHandle = Index_open(INDEX_PRIORITY_MEDIUM,5L*1000L);
  if (indexHandle == NULL)
  {
    String_delete(storageName);
    String_delete(printableStorageName);
    String_delete(pattern);
    String_delete(baseName);
    Storage_doneSpecifier(&storageSpecifier);
    StringList_done(&storageDirectoryList);

    plogMessage(NULL,  // logHandle,
                LOG_TYPE_INDEX,
                "INDEX",
                "Open index database fail - disabled auto-index\n"
               );
    return;
  }

  // run continous check for auto index
  while (!quitFlag)
  {
    // pause
    pauseIndexUpdate();

    // collect storage locations to check for BAR files
    getStorageDirectories(&storageDirectoryList);

    // check storage locations for BAR files, send index update request
    initDuplicateJobOptions(&jobOptions,serverDefaultJobOptions);
    while (!StringList_isEmpty(&storageDirectoryList))
    {
      storageDirectoryName = StringList_getFirst(&storageDirectoryList,NULL);

      error = Storage_parseName(&storageSpecifier,storageDirectoryName);
      if (error == ERROR_NONE)
      {
        if (   (storageSpecifier.type == STORAGE_TYPE_FILESYSTEM)
            || (storageSpecifier.type == STORAGE_TYPE_FTP       )
            || (storageSpecifier.type == STORAGE_TYPE_SSH       )
            || (storageSpecifier.type == STORAGE_TYPE_SCP       )
            || (storageSpecifier.type == STORAGE_TYPE_SFTP      )
            || (storageSpecifier.type == STORAGE_TYPE_WEBDAV    )
           )
        {
          // get base directory
          File_setFileName(baseName,storageSpecifier.archiveName);
          do
          {
            error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                              &storageSpecifier,
                                              &jobOptions,
                                              SERVER_CONNECTION_PRIORITY_LOW,
                                              baseName
                                             );
            if (error == ERROR_NONE)
            {
              Storage_closeDirectoryList(&storageDirectoryListHandle);
            }
            else
            {
              File_getFilePathName(baseName,baseName);
            }
          }
          while ((error != ERROR_NONE) && !String_isEmpty(baseName));

          if (!String_isEmpty(baseName))
          {
            // read directory and scan all sub-directories for .bar files if possible
            pprintInfo(4,
                       "INDEX",
                       "Auto-index scan '%s'\n",
                       String_cString(Storage_getPrintableName(&storageSpecifier,baseName))
                      );
            File_appendFileNameCString(File_setFileName(pattern,baseName),"*.bar");
            (void)Storage_forAll(pattern,
                                 CALLBACK_INLINE(Errors,(ConstString storageName, const FileInfo *fileInfo, void *userData),
                                 {
                                   Errors error;

                                   assert(fileInfo != NULL);

                                   UNUSED_VARIABLE(userData);

                                   error = Storage_parseName(&storageSpecifier,storageName);
                                   if (error == ERROR_NONE)
                                   {
                                     // check entry type and file name
                                     switch (fileInfo->type)
                                     {
                                       case FILE_TYPE_FILE:
                                       case FILE_TYPE_LINK:
                                       case FILE_TYPE_HARDLINK:
                                         // get index id, request index update
                                         if (Index_findStorageByName(indexHandle,
                                                                     &storageSpecifier,
                                                                     NULL,  // archiveName
                                                                     NULL,  // uuidId
                                                                     NULL,  // entityId
                                                                     NULL,  // jobUUID
                                                                     NULL,  // scheduleUUID
                                                                     &storageId,
                                                                     &indexState,
                                                                     &lastCheckedDateTime
                                                                    )
                                            )
                                         {
                                           if      (fileInfo->timeModified > lastCheckedDateTime)
                                           {
                                             // request update index
                                             error = Index_setState(indexHandle,
                                                                    storageId,
                                                                    INDEX_STATE_UPDATE_REQUESTED,
                                                                    Misc_getCurrentDateTime(),
                                                                    NULL
                                                                   );
                                             if (error == ERROR_NONE)
                                             {
                                               plogMessage(NULL,  // logHandle,
                                                           LOG_TYPE_INDEX,
                                                           "INDEX",
                                                           "Requested update index for '%s'\n",
                                                           String_cString(Storage_getPrintableName(&storageSpecifier,NULL))
                                                          );
                                             }
                                           }
                                           else if (indexState == INDEX_STATE_OK)
                                           {
                                             // set last checked date/time
                                             error = Index_setState(indexHandle,
                                                                    storageId,
                                                                    INDEX_STATE_OK,
                                                                    Misc_getCurrentDateTime(),
                                                                    NULL
                                                                   );
                                           }
                                         }
                                         else
                                         {
                                           // add to index
                                           error = Index_newStorage(indexHandle,
                                                                    INDEX_ID_NONE, // entityId
                                                                    storageName,
                                                                    INDEX_STATE_UPDATE_REQUESTED,
                                                                    INDEX_MODE_AUTO,
                                                                    &storageId
                                                                   );
                                           if (error == ERROR_NONE)
                                           {
                                             plogMessage(NULL,  // logHandle,
                                                         LOG_TYPE_INDEX,
                                                         "INDEX",
                                                         "Requested add index for '%s'\n",
                                                         String_cString(Storage_getPrintableName(&storageSpecifier,NULL))
                                                        );
                                           }
                                         }
                                         break;
                                       default:
                                         break;
                                     }
                                   }

                                   return error;
                                 },NULL)
                                );
          }
        }
      }

      String_delete(storageDirectoryName);
    }
    doneJobOptions(&jobOptions);

    // delete not existing and expired indizes
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   INDEX_ID_ANY,  // entity id
                                   NULL,  // jobUUID
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // name
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error == ERROR_NONE)
    {
      now    = Misc_getCurrentDateTime();
      string = String_new();
      while (   !quitFlag
             && Index_getNextStorage(&indexQueryHandle,
                                     NULL,  // uuidId
                                     NULL,  // jobUUID
                                     NULL,  // entityId
                                     NULL,  // scheduleUUID
                                     NULL,  // archiveType
                                     &storageId,
                                     storageName,
                                     &createdDateTime,
                                     NULL,  // totalEntryCount
                                     NULL,  // totalEntrySize
                                     &indexState,
                                     &indexMode,
                                     &lastCheckedDateTime,
                                     NULL  // errorMessage
                                    )
            )
      {
        // get printable name (if possible)
        error = Storage_parseName(&storageSpecifier,storageName);
        if (error == ERROR_NONE)
        {
          String_set(printableStorageName,Storage_getPrintableName(&storageSpecifier,NULL));
        }
        else
        {
          String_set(printableStorageName,storageName);
        }

        if (   (indexMode == INDEX_MODE_AUTO)
            && (indexState != INDEX_STATE_UPDATE_REQUESTED)
            && (indexState != INDEX_STATE_UPDATE)
            && (now > (createdDateTime+globalOptions.indexDatabaseKeepTime))
            && (now > (lastCheckedDateTime+globalOptions.indexDatabaseKeepTime))
           )
        {
          Index_deleteStorage(indexHandle,storageId);

          plogMessage(NULL,  // logHandle,
                      LOG_TYPE_INDEX,
                      "INDEX",
                      "Deleted index for '%s', last checked %s -- xxxx %d\n",
                      String_cString(printableStorageName),
                      String_cString(Misc_formatDateTime(string,lastCheckedDateTime,NULL)),
globalOptions.indexDatabaseKeepTime
                     );
        }
      }
      Index_doneList(&indexQueryHandle);
      String_delete(string);
    }

    // sleep
    delayAutoIndexThread();
  }

  // done index
  Index_close(indexHandle);

  // free resources
  String_delete(storageName);
  String_delete(printableStorageName);
  String_delete(pattern);
  String_delete(baseName);
  Storage_doneSpecifier(&storageSpecifier);
  StringList_done(&storageDirectoryList);
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : sendClient
* Purpose: send data to client
* Input  : clientInfo - client info
*          data       - data string
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void sendClient(ClientInfo *clientInfo, ConstString data)
{
  SemaphoreLock semaphoreLock;

  assert(clientInfo != NULL);
  assert(data != NULL);

  #ifdef SERVER_DEBUG
    fprintf(stderr,"DEBUG: result=%s",String_cString(data));
  #endif /* SERVER_DEBUG */

  if (!clientInfo->quitFlag)
  {
    switch (clientInfo->type)
    {
      case CLIENT_TYPE_BATCH:
        (void)File_write(clientInfo->file.fileHandle,String_cString(data),String_length(data));
        (void)File_flush(clientInfo->file.fileHandle);
        break;
      case CLIENT_TYPE_NETWORK:
        SEMAPHORE_LOCKED_DO(semaphoreLock,&clientInfo->network.writeLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
        {
          (void)Network_send(&clientInfo->network.socketHandle,String_cString(data),String_length(data));
        }
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }
}

/***********************************************************************\
* Name   : sendClientResult
* Purpose: send result to client
* Input  : clientInfo   - client info
*          id           - command id
*          completeFlag - TRUE if command is completed, FALSE otherwise
*          errorCode    - error code
*          format       - format string
*          ...          - optional arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void sendClientResult(ClientInfo *clientInfo, uint id, bool completeFlag, uint errorCode, const char *format, ...)
{
  locale_t locale;
  String   result;
  va_list  arguments;

  result = String_new();

  locale = uselocale(POSIXLocale);
  {
    String_format(result,"%d %d %d ",id,completeFlag ? 1 : 0,Error_getCode(errorCode));
    va_start(arguments,format);
    String_vformat(result,format,arguments);
    va_end(arguments);
    String_appendChar(result,'\n');
  }
  uselocale(locale);

  sendClient(clientInfo,result);

  String_delete(result);
}

/***********************************************************************\
* Name   : clientAction
* Purpose: execute client action
* Input  : clientInfo    - client info
*          id            - command id
*          resultMap     - result map variable
*          actionCommand - action command
*          timeout       - timeout or WAIT_FOREVER
*          format        - arguments format string
*          ...           - optional arguments
* Output : resultMap - results
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors clientAction(ClientInfo *clientInfo, uint id, StringMap resultMap, const char *actionCommand, long timeout, const char *format, ...)
{
  locale_t      locale;
  String        result;
  va_list       arguments;
  SemaphoreLock semaphoreLock;
  Errors        error;

  assert(clientInfo != NULL);
  assert(actionCommand != NULL);

  error = ERROR_UNKNOWN;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&clientInfo->action.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // clear action
    clientInfo->action.error = ERROR_UNKNOWN;

    // send action
    result = String_new();
    locale = uselocale(POSIXLocale);
    {
      String_format(result,"%u 0 0 action=%s ",id,actionCommand);
      va_start(arguments,format);
      String_vformat(result,format,arguments);
      va_end(arguments);
      String_appendChar(result,'\n');
    }
    uselocale(locale);
    sendClient(clientInfo,result);
    String_delete(result);

    // wait for result, timeout, or quit
    while ((clientInfo->action.error == ERROR_UNKNOWN) && !clientInfo->quitFlag)
    {
      if (!Semaphore_waitModified(&clientInfo->action.lock,timeout))
      {
        Semaphore_unlock(&clientInfo->action.lock);
        return ERROR_NETWORK_TIMEOUT;
      }
    }
    if (clientInfo->quitFlag)
    {
      Semaphore_unlock(&clientInfo->action.lock);
      return ERROR_ABORTED;
    }

    // get action result
    error = clientInfo->action.error;
    if (resultMap != NULL)
    {
      StringMap_move(resultMap,clientInfo->action.resultMap);
    }
    else
    {
      StringMap_clear(clientInfo->action.resultMap);
    }
  }

  return error;
}

/***********************************************************************\
* Name   : isCommandAborted
* Purpose: check if command was aborted
* Input  : clientInfo - client info
* Output : -
* Return : TRUE if command execution aborted or client disconnected,
*          FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool isCommandAborted(ClientInfo *clientInfo, uint commandId)
{
  bool abortedFlag;

  assert(clientInfo != NULL);

  abortedFlag = (clientInfo->abortCommandId == commandId);
  switch (clientInfo->type)
  {
    case CLIENT_TYPE_BATCH:
      break;
    case CLIENT_TYPE_NETWORK:
      if (clientInfo->quitFlag) abortedFlag = TRUE;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return abortedFlag;
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : encodeHex
* Purpose: encoded data as hex-string
* Input  : string     - string variable
*          data       - data to encode
*          dataLength - length of data [bytes]
* Output : string - string
* Return : string
* Notes  : -
\***********************************************************************/

LOCAL String encodeHex(String string, const byte *data, uint length)
{
  uint z;

  assert(string != NULL);

  for (z = 0; z < length; z++)
  {
    String_format(string,"%02x",data[z]);
  }

  return string;
}

/***********************************************************************\
* Name   : decodeHex
* Purpose: decode hex-string into data
* Input  : s             - hex-string
*          maxDataLength - max. data length  [bytes]
* Output : data       - data
*          dataLength - length of data [bytes]
* Return : TRUE iff data decoded
* Notes  : -
\***********************************************************************/

LOCAL bool decodeHex(const char *s, byte *data, uint *dataLength, uint maxDataLength)
{
  uint i;
  char t[3];
  char *w;

  assert(s != NULL);
  assert(data != NULL);
  assert(dataLength != NULL);

  i = 0;
  while (((*s) != '\0') && (i < maxDataLength))
  {
    t[0] = (*s); s++;
    if ((*s) != '\0')
    {
      t[1] = (*s); s++;
      t[2] = '\0';

      data[i] = (byte)strtol(t,&w,16);
      if ((*w) != '\0') return FALSE;
      i++;
    }
    else
    {
      return FALSE;
    }
  }
  (*dataLength) = i;

  return TRUE;
}

/***********************************************************************\
* Name   : decryptPassword
* Purpose: decrypt password from hex-string
* Input  : password          - password
*          clientInfo        - client info
*          encryptType       - encrypt type
*          encryptedPassword - encrypted password
* Output : -
* Return : TRUE iff encrypted password equals password
* Notes  : -
\***********************************************************************/

LOCAL bool decryptPassword(Password *password, const ClientInfo *clientInfo, ConstString encryptType, ConstString encryptedPassword)
{
  byte encryptedBuffer[1024];
  uint encryptedBufferLength;
  byte encodedBuffer[1024];
  uint encodedBufferLength;
  uint i;

  assert(password != NULL);

  // decode hex-string
  if (!decodeHex(String_cString(encryptedPassword),encryptedBuffer,&encryptedBufferLength,sizeof(encryptedBuffer)))
  {
    return FALSE;
  }

  // decrypt password
  if      (String_equalsIgnoreCaseCString(encryptType,"RSA") && Crypt_isAsymmetricSupported())
  {
//fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,encryptedBufferLength);
    Crypt_keyDecrypt(&clientInfo->secretKey,
                     encryptedBuffer,
                     encryptedBufferLength,
                     encodedBuffer,
                     &encodedBufferLength,
                     sizeof(encodedBuffer)
                    );
  }
  else if (String_equalsIgnoreCaseCString(encryptType,"NONE"))
  {
    memcpy(encodedBuffer,encryptedBuffer,encryptedBufferLength);
    encodedBufferLength = encryptedBufferLength;
  }
  else
  {
    return FALSE;
  }

//fprintf(stderr,"%s, %d: n=%d s='",__FILE__,__LINE__,encodedBufferLength); for (i = 0; i < encodedBufferLength; i++) { fprintf(stderr,"%c",encodedBuffer[i]^clientInfo->sessionId[i]); } fprintf(stderr,"'\n");

  // decode password (XOR with session id)
  Password_clear(password);
  i = 0;
  while (   (i < encodedBufferLength)
         && ((char)(encodedBuffer[i]^clientInfo->sessionId[i]) != '\0')
        )
  {
    Password_appendChar(password,(char)(encodedBuffer[i]^clientInfo->sessionId[i]));
    i++;
  }

  return TRUE;
}

/***********************************************************************\
* Name   : checkPassword
* Purpose: check password
* Input  : clientInfo        - client info
*          encryptType       - encrypt type
*          encryptedPassword - encrypted password
*          password          - password
* Output : -
* Return : TRUE iff encrypted password equals password
* Notes  : -
\***********************************************************************/

LOCAL bool checkPassword(const ClientInfo *clientInfo, ConstString encryptType, ConstString encryptedPassword, const Password *password)
{
  byte encryptedBuffer[1024];
  uint encryptedBufferLength;
  byte encodedBuffer[1024];
  uint encodedBufferLength;
  uint n;
  uint i;
  bool okFlag;

  // decode hex-string
  if (!decodeHex(String_cString(encryptedPassword),encryptedBuffer,&encryptedBufferLength,sizeof(encryptedBuffer)))
  {
    return FALSE;
  }

  // decrypt password
  if      (String_equalsIgnoreCaseCString(encryptType,"RSA") && Crypt_isAsymmetricSupported())
  {
//fprintf(stderr,"%s, %d: %d\n",__FILE__,__LINE__,encryptedBufferLength);
    if (Crypt_keyDecrypt(&clientInfo->secretKey,
                         encryptedBuffer,
                         encryptedBufferLength,
                         encodedBuffer,
                         &encodedBufferLength,
                         sizeof(encodedBuffer)
                        ) != ERROR_NONE
       )
    {
      return FALSE;
    }
  }
  else if (String_equalsIgnoreCaseCString(encryptType,"NONE"))
  {
    memcpy(encodedBuffer,encryptedBuffer,encryptedBufferLength);
    encodedBufferLength = encryptedBufferLength;
  }
  else
  {
    return FALSE;
  }

//fprintf(stderr,"%s, %d: n=%d s='",__FILE__,__LINE__,encodedBufferLength); for (i = 0; i < encodedBufferLength; i++) { fprintf(stderr,"%c",encodedBuffer[i]^clientInfo->sessionId[i]); } fprintf(stderr,"'\n");

  // check password length
  n = 0;
  while (   (n < encodedBufferLength)
         && ((char)(encodedBuffer[n]^clientInfo->sessionId[n]) != '\0')
        )
  {
    n++;
  }
  if (password != NULL)
  {
    if (Password_length(password) != n)
    {
      return FALSE;
    }
  }

  // check encoded password
  if (password != NULL)
  {
    okFlag = TRUE;
    i = 0;
    while ((i < Password_length(password)) && okFlag)
    {
      okFlag = (Password_getChar(password,i) == (encodedBuffer[i]^clientInfo->sessionId[i]));
      i++;
    }
    if (!okFlag)
    {
      return FALSE;
    }
  }

  return TRUE;
}

/***********************************************************************\
* Name   : getJobStateText
* Purpose: get text for job state
* Input  : jobState   - job state
*          jobOptions - job options
* Output : -
* Return : text
* Notes  : -
\***********************************************************************/

LOCAL const char *getJobStateText(JobStates jobState, const JobOptions *jobOptions)
{
  const char *stateText;

  assert(jobOptions != NULL);

  stateText = "UNKNOWN";
  switch (jobState)
  {
    case JOB_STATE_NONE:
      stateText = "none";
      break;
    case JOB_STATE_WAITING:
      stateText = "waiting";
      break;
    case JOB_STATE_RUNNING:
      stateText = (jobOptions->dryRunFlag) ? "dry_running" : "running";
      break;
    case JOB_STATE_REQUEST_FTP_PASSWORD:
      stateText = "request_ftp_password";
      break;
    case JOB_STATE_REQUEST_SSH_PASSWORD:
      stateText = "request_ssh_password";
      break;
    case JOB_STATE_REQUEST_WEBDAV_PASSWORD:
      stateText = "request_webdav_password";
      break;
    case JOB_STATE_REQUEST_CRYPT_PASSWORD:
      stateText = "request_crypt_password";
      break;
    case JOB_STATE_REQUEST_VOLUME:
      stateText = "request_volume";
      break;
    case JOB_STATE_DONE:
      stateText = "done";
      break;
    case JOB_STATE_ERROR:
      stateText = "error";
      break;
    case JOB_STATE_ABORTED:
      stateText = "aborted";
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return stateText;
}

/***********************************************************************\
* Name   : newDirectoryInfo
* Purpose: create new directory info
* Input  : pathName  - path name
* Output : -
* Return : directory info node
* Notes  : -
\***********************************************************************/

LOCAL DirectoryInfoNode *newDirectoryInfo(ConstString pathName)
{
  DirectoryInfoNode *directoryInfoNode;

  assert(pathName != NULL);

  // allocate job node
  directoryInfoNode = LIST_NEW_NODE(DirectoryInfoNode);
  if (directoryInfoNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init directory info node
  directoryInfoNode->pathName          = String_duplicate(pathName);
  directoryInfoNode->fileCount         = 0LL;
  directoryInfoNode->totalFileSize     = 0LL;
  directoryInfoNode->directoryOpenFlag = FALSE;

  // init path name list
  StringList_init(&directoryInfoNode->pathNameList);
  StringList_append(&directoryInfoNode->pathNameList,pathName);

  return directoryInfoNode;
}

/***********************************************************************\
* Name   : freeDirectoryInfoNode
* Purpose: free directory info node
* Input  : directoryInfoNode - directory info node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeDirectoryInfoNode(DirectoryInfoNode *directoryInfoNode, void *userData)
{
  assert(directoryInfoNode != NULL);

  UNUSED_VARIABLE(userData);

  if (directoryInfoNode->directoryOpenFlag)
  {
    File_closeDirectoryList(&directoryInfoNode->directoryListHandle);
  }
  StringList_done(&directoryInfoNode->pathNameList);
  String_delete(directoryInfoNode->pathName);
}

/***********************************************************************\
* Name   : findDirectoryInfo
* Purpose: find job by id
* Input  : directoryInfoList - directory info list
*          pathName          - path name
* Output : -
* Return : directory info node or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL DirectoryInfoNode *findDirectoryInfo(DirectoryInfoList *directoryInfoList, ConstString pathName)
{
  DirectoryInfoNode *directoryInfoNode;

  assert(directoryInfoList != NULL);
  assert(pathName != NULL);

  directoryInfoNode = directoryInfoList->head;
  while ((directoryInfoNode != NULL) && !String_equals(directoryInfoNode->pathName,pathName))
  {
    directoryInfoNode = directoryInfoNode->next;
  }

  return directoryInfoNode;
}

/***********************************************************************\
* Name   : getDirectoryInfo
* Purpose: get directory info: number of files, total file size
* Input  : directory - path name
*          timeout   - timeout [ms] or -1 for no timeout
* Output : fileCount     - number of files
*          totalFileSize - total file size (bytes)
*          timedOut      - TRUE iff timed out, FALSE otherwise
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void getDirectoryInfo(DirectoryInfoNode *directoryInfoNode,
                            long              timeout,
                            uint64            *fileCount,
                            uint64            *totalFileSize,
                            bool              *timedOut
                           )
{
  uint64   startTimestamp;
  String   pathName;
  String   fileName;
  FileInfo fileInfo;
  Errors   error;
//uint n;

  assert(directoryInfoNode != NULL);
  assert(fileCount != NULL);
  assert(totalFileSize != NULL);

  // get start timestamp
  startTimestamp = Misc_getTimestamp();

  pathName = String_new();
  fileName = String_new();
  if (timedOut != NULL) (*timedOut) = FALSE;
  while (   !quitFlag
         && (   !StringList_isEmpty(&directoryInfoNode->pathNameList)
             || directoryInfoNode->directoryOpenFlag
            )
         && ((timedOut == NULL) || !(*timedOut))
        )
  {
    if (!directoryInfoNode->directoryOpenFlag)
    {
      // process FIFO for deep-first search; this keep the directory list shorter
      StringList_getLast(&directoryInfoNode->pathNameList,pathName);

      // open directory for reading
      error = File_openDirectoryList(&directoryInfoNode->directoryListHandle,pathName);
      if (error != ERROR_NONE)
      {
        continue;
      }
      directoryInfoNode->directoryOpenFlag = TRUE;
    }

    // read directory content
    while (   !quitFlag
           && !File_endOfDirectoryList(&directoryInfoNode->directoryListHandle)
           && ((timedOut == NULL) || !(*timedOut))
          )
    {
      // read next directory entry
      error = File_readDirectoryList(&directoryInfoNode->directoryListHandle,fileName);
      if (error != ERROR_NONE)
      {
        continue;
      }
      directoryInfoNode->fileCount++;
      error = File_getFileInfo(fileName,&fileInfo);
      if (error != ERROR_NONE)
      {
        continue;
      }

      switch (File_getType(fileName))
      {
        case FILE_TYPE_FILE:
          directoryInfoNode->totalFileSize += fileInfo.size;
          break;
        case FILE_TYPE_DIRECTORY:
          StringList_append(&directoryInfoNode->pathNameList,fileName);
          break;
        case FILE_TYPE_LINK:
          break;
        case FILE_TYPE_HARDLINK:
          directoryInfoNode->totalFileSize += fileInfo.size;
          break;
        case FILE_TYPE_SPECIAL:
          break;
        default:
          break;
      }

      // check for timeout
      if ((timeout >= 0) && (timedOut != NULL))
      {
        (*timedOut) = (Misc_getTimestamp() >= (startTimestamp+timeout*1000));
      }
    }

    if ((timedOut == NULL) || !(*timedOut))
    {
      // close diretory
      directoryInfoNode->directoryOpenFlag = FALSE;
      File_closeDirectoryList(&directoryInfoNode->directoryListHandle);
    }

    // check for timeout
    if ((timeout >= 0) && (timedOut != NULL))
    {
      (*timedOut) = (Misc_getTimestamp() >= (startTimestamp+timeout*1000));
    }
  }
  String_delete(pathName);
  String_delete(fileName);

  // get values
  (*fileCount)     = directoryInfoNode->fileCount;
  (*totalFileSize) = directoryInfoNode->totalFileSize;
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : serverCommand_errorInfo
* Purpose: get error info
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            error=<n>
*          Result:
*            errorMessage=<text>
\***********************************************************************/

LOCAL void serverCommand_errorInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint64 n;
  Errors error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get error code
  if (!StringMap_getUInt64(argumentMap,"error",&n,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected errorCode=<n>");
    return;
  }
  error = (Errors)n;

  // format result
  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,
                   "errorMessage=%'s",
                   Error_getText(error)
                  );
}

/***********************************************************************\
* Name   : serverCommand_startSSL
* Purpose: start SSL connection
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_startSSL(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  #ifdef HAVE_GNU_TLS
    Errors error;
  #endif /* HAVE_GNU_TLS */

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  #ifdef HAVE_GNU_TLS
    if ((serverCA == NULL) || (serverCA->data == NULL))
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_NO_TLS_CA,"no server certificate authority data");
      return;
    }
    if ((serverCert == NULL) || (serverCert->data == NULL))
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_NO_TLS_CERTIFICATE,"no server certificate data");
      return;
    }
    if ((serverKey == NULL) || (serverKey->data == NULL))
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_NO_TLS_KEY,"no server key data");
      return;
    }

    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

    error = Network_startSSL(&clientInfo->network.socketHandle,
                             serverCA->data,
                             serverCA->length,
                             serverCert->data,
                             serverCert->length,
                             serverKey->data,
                             serverKey->length
                            );
    if (error != ERROR_NONE)
    {
      Network_disconnect(&clientInfo->network.socketHandle);
    }
  #else /* not HAVE_GNU_TLS */
    sendClientResult(clientInfo,id,TRUE,ERROR_FUNCTION_NOT_SUPPORTED,"not available");
  #endif /* HAVE_GNU_TLS */

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_authorize
* Purpose: user authorization: check password
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            encryptType=<type>
*            encryptedPassword=<encrypted password>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_authorize(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String encryptType;
  String encryptedPassword;
  bool   okFlag;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get encrypt type, encrypted password
  encryptType = String_new();
  if (!StringMap_getString(argumentMap,"encryptType",encryptType,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected encryptType=<type>");
    return;
  }
  encryptedPassword = String_new();
  if (!StringMap_getString(argumentMap,"encryptedPassword",encryptedPassword,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected encryptedPassword=<encrypted password>");
    String_delete(encryptedPassword);
    String_delete(encryptType);
    return;
  }
//fprintf(stderr,"%s, %d: %s %d\n",__FILE__,__LINE__,String_cString(encryptedPassword),String_length(encryptedPassword));

  // check password
  if (!globalOptions.serverDebugFlag)
  {
    okFlag = checkPassword(clientInfo,encryptType,encryptedPassword,serverPassword);
  }
  else
  {
    okFlag = TRUE;
  }

  // authorization state
  if (okFlag)
  {
    clientInfo->authorizationState = AUTHORIZATION_STATE_OK;
    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
//fprintf(stderr,"%s, %d: encryptedPassword='%s' %d\n",__FILE__,__LINE__,String_cString(encryptedPassword),String_length(encryptedPassword));
    clientInfo->authorizationState = AUTHORIZATION_STATE_FAIL;
    sendClientResult(clientInfo,id,TRUE,ERROR_AUTHORIZATION,"authorization failure");
    printInfo(1,"Client authorization failure: '%s:%u'\n",String_cString(clientInfo->network.name),clientInfo->network.port);
  }

  // free resources
  String_delete(encryptedPassword);
  String_delete(encryptType);
}

/***********************************************************************\
* Name   : serverCommand_version
* Purpose: get protocol version
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            major=<major version>
*            minor=<minor version>
\***********************************************************************/

LOCAL void serverCommand_version(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"major=%d minor=%d",SERVER_PROTOCOL_VERSION_MAJOR,SERVER_PROTOCOL_VERSION_MINOR);
}

/***********************************************************************\
* Name   : serverCommand_quit
* Purpose: quit server
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_quit(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  if (globalOptions.serverDebugFlag)
  {
    quitFlag = TRUE;
    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_FUNCTION_NOT_SUPPORTED,"not in debug mode");
  }
}

/***********************************************************************\
* Name   : serverCommand_quit
* Purpose: quit server
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            error=<n>
*            ...
*          Result:
\***********************************************************************/

LOCAL void serverCommand_actionResult(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  SemaphoreLock  semaphoreLock;
  uint           stringMapIterator;
  uint64         n;
  const char     *name;
  StringMapTypes type;
  StringMapValue value;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&clientInfo->action.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // get error
    StringMap_getUInt64(argumentMap,"error",&n,ERROR_UNKNOWN);
    clientInfo->action.error = (Errors)n;

    // get arguments
    STRINGMAP_ITERATE(argumentMap,stringMapIterator,name,type,value)
    {
      if (!stringEquals(name,"error"))
      {
        StringMap_putValue(clientInfo->action.resultMap,name,type,&value);
      }
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_get
* Purpose: get setting from server
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<name>
*          Result:
*            value=<value>
\***********************************************************************/

LOCAL void serverCommand_get(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String name;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get name
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    return;
  }

  // send value
  if (String_equalsCString(name,"FILE_SEPARATOR"))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"value=%c",FILES_PATHNAME_SEPARATOR_CHAR);
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"%S",name);
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_serverOptionGet
* Purpose: get server option
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<name>
*          Result:
*            value=<value>
\***********************************************************************/

LOCAL void serverCommand_serverOptionGet(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String            name;
  int               i;
  String            value;
  ConfigValueFormat configValueFormat;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get name
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    return;
  }

  // find config value
  i = ConfigValue_valueIndex(CONFIG_VALUES,NULL,String_cString(name));
  if (i < 0)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown server config value '%S'",name);
    String_delete(name);
    return;
  }

  // send value
  value = String_new();
  ConfigValue_formatInit(&configValueFormat,
                         &CONFIG_VALUES[i],
                         CONFIG_VALUE_FORMAT_MODE_VALUE,
                         &globalOptions
                        );
  ConfigValue_format(&configValueFormat,value);
  ConfigValue_formatDone(&configValueFormat);
  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"value=%S",value);
  String_delete(value);

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_serverOptionSet
* Purpose: set server option
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<name>
*            value=<value>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_serverOptionSet(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String name,value;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get name, value
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    String_delete(name);
    return;
  }
  value = String_new();
  if (!StringMap_getString(argumentMap,"value",value,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected value=<value>");
    String_delete(value);
    String_delete(name);
    return;
  }

  // parse
  if (!ConfigValue_parse(String_cString(name),
                         String_cString(value),
                         CONFIG_VALUES,
                         NULL, // sectionName
                         NULL, // outputHandle
                         NULL, // errorPrefix
                         NULL, // warningPrefix
                         &globalOptions
                        )
     )
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown server config value '%S'",name);
    String_delete(value);
    String_delete(name);
    return;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(value);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_serverOptionFlush
* Purpose: flush server options to config file
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_serverOptionFlush(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  Errors error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  error = updateConfig();
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,error,"write config file fail");
    return;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_serverList
* Purpose: get job server list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            id=<n>
*            name=<name>
*            serverType=filesystem|ftp|ssh|webdav
*            path=<path|URL>
*            loginName=<name>
*            port=<n>
*            maxConnectionCount=<n>
*            maxStorageSize=<n>
*            ...
\***********************************************************************/

LOCAL void serverCommand_serverList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  SemaphoreLock semaphoreLock;
  ServerNode    *serverNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    LIST_ITERATE(&globalOptions.serverList,serverNode)
    {
      switch (serverNode->server.type)
      {
        case SERVER_TYPE_NONE:
          break;
        case SERVER_TYPE_FILE:
          sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                           "id=%u name=%'S serverType=%s maxStorageSize=%llu",
                           serverNode->id,
                           serverNode->server.name,
                           "FILE",
                           serverNode->server.maxStorageSize
                          );
          break;
        case SERVER_TYPE_FTP:
          sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                           "id=%u name=%'S serverType=%s loginName=%'S maxConnectionCount=%d maxStorageSize=%llu",
                           serverNode->id,
                           serverNode->server.name,
                           "FTP",
                           serverNode->server.ftp.loginName,
                           serverNode->server.maxConnectionCount,
                           serverNode->server.maxStorageSize
                          );
          break;
        case SERVER_TYPE_SSH:
          sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                           "id=%u name=%'S serverType=%s port=%d loginName=%'S maxConnectionCount=%d maxStorageSize=%llu",
                           serverNode->id,
                           serverNode->server.name,
                           "SSH",
                           serverNode->server.ssh.port,
                           serverNode->server.ssh.loginName,
                           serverNode->server.maxConnectionCount,
                           serverNode->server.maxStorageSize
                          );
          break;
        case SERVER_TYPE_WEBDAV:
          sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                           "id=%u name=%'S serverType=%s loginName=%'S maxConnectionCount=%d maxStorageSize=%llu",
                           serverNode->id,
                           serverNode->server.name,
                           "WEBDAV",
                           serverNode->server.webDAV.loginName,
                           serverNode->server.maxConnectionCount,
                           serverNode->server.maxStorageSize
                          );
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_serverListAdd
* Purpose: add entry to server list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<name>
*            serverType=filesystem|ftp|ssh|webdav
*            path=<path|URL>
*            port=<n>
*            loginName=<name>
*            [password=<password>]
*            [publicKey=<data>]
*            [privateKey=<data>]
*          Result:
*            id=<n>
\***********************************************************************/

LOCAL void serverCommand_serverListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String        name;
  ServerTypes   serverType;
  uint          port;
  String        loginName;
  String        password;
  String        publicKey;
  String        privateKey;
  uint          maxConnectionCount;
  uint64        maxStorageSize;
  SemaphoreLock semaphoreLock;
  ServerNode    *serverNode;
  Errors        error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get name, server type, login name, port, password, public/private key, max. connections, max. storage size
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"serverType",&serverType,(StringMapParseEnumFunction)parseServerType,SERVER_TYPE_FILE))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected serverType=<FILE|FTP|SSH|WEBDAV>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"port",&port,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected loginName=<name>");
    String_delete(name);
    return;
  }
  loginName = String_new();
  if (!StringMap_getString(argumentMap,"loginName",loginName,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected loginName=<name>");
    String_delete(loginName);
    String_delete(name);
    return;
  }
  password = String_new();
  if (!StringMap_getString(argumentMap,"password",password,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected password=<password>");
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  publicKey = String_new();
  if (!StringMap_getString(argumentMap,"publicKey",publicKey,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected publicKey=<data>");
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  privateKey = String_new();
  if (!StringMap_getString(argumentMap,"privateKey",privateKey,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected privateKey=<data>");
    String_delete(privateKey);
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"maxConnectionCount",&maxConnectionCount,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected maxConnectionCount=<n>");
    String_delete(privateKey);
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"maxStorageSize",&maxStorageSize,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected maxStorageSize=<n>");
    String_delete(privateKey);
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }

  // allocate storage server node
  serverNode = newServerNode(name,serverType);
  assert(serverNode != NULL);

  // init storage server settings
  switch (serverType)
  {
    case SERVER_TYPE_NONE:
      break;
    case SERVER_TYPE_FILE:
      break;
    case SERVER_TYPE_FTP:
      String_set(serverNode->server.ftp.loginName,loginName);
      if (!String_isEmpty(password))
      {
        if (serverNode->server.ftp.password == NULL)
        {
          serverNode->server.ftp.password = Password_newString(password);
        }
        else
        {
          Password_setString(serverNode->server.ftp.password,password);
        }
      }
      break;
    case SERVER_TYPE_SSH:
      serverNode->server.ssh.port = port;
      String_set(serverNode->server.ssh.loginName,loginName);
      if (!String_isEmpty(password))
      {
        if (serverNode->server.ssh.password == NULL)
        {
          serverNode->server.ssh.password = Password_newString(password);
        }
        else
        {
          Password_setString(serverNode->server.ssh.password,password);
        }
      }
      setKeyString(&serverNode->server.ssh.publicKey,publicKey);
      setKeyString(&serverNode->server.ssh.privateKey,privateKey);
      break;
    case SERVER_TYPE_WEBDAV:
      String_set(serverNode->server.webDAV.loginName,loginName);
      if (!String_isEmpty(password))
      {
        if (serverNode->server.webDAV.password == NULL)
        {
          serverNode->server.webDAV.password = Password_newString(password);
        }
        else
        {
          Password_setString(serverNode->server.webDAV.password,password);
        }
      }
      setKeyString(&serverNode->server.webDAV.publicKey,publicKey);
      setKeyString(&serverNode->server.webDAV.privateKey,privateKey);
      break;
  }
  serverNode->server.maxConnectionCount = maxConnectionCount;
  serverNode->server.maxStorageSize     = maxStorageSize;

  // add to server list
  SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    List_append(&globalOptions.serverList,serverNode);
  }

  // update config files
  error = updateConfig();
  if (error != ERROR_NONE)
  {
    Semaphore_unlock(&globalOptions.serverList.lock);
    sendClientResult(clientInfo,id,TRUE,error,"write config file fail: %s",Error_getText(error));
    String_delete(privateKey);
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"id=%u",serverNode->id);

  // free resources
  String_delete(privateKey);
  String_delete(publicKey);
  String_delete(password);
  String_delete(loginName);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_serverListUpdate
* Purpose: update entry in server list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            id=<n>
*            name=<name>
*            serverType=filesystem|ftp|ssh|webdav
*            path=<path|URL>
*            loginName=<name>
*            port=<n>
*            [password=<password>]
*            [publicKey=<data>]
*            [privateKey=<data>]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_serverListUpdate(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint          serverId;
  String        name;
  ServerTypes   serverType;
  uint          port;
  String        loginName;
  String        password;
  String        publicKey;
  String        privateKey;
  uint          maxConnectionCount;
  uint64        maxStorageSize;
  SemaphoreLock semaphoreLock;
  ServerNode    *serverNode;
  Errors        error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get id, name, server type, login name, port, password, public/private key, max. connections, max. storage size
  if (!StringMap_getUInt(argumentMap,"id",&serverId,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected id=<n>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    String_delete(name);
    return;
  }
  if (!StringMap_getEnum(argumentMap,"serverType",&serverType,(StringMapParseEnumFunction)parseServerType,SERVER_TYPE_FILE))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected serverType=<FILE|FTP|SSH|WEBDAV>");
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"port",&port,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected loginName=<name>");
    String_delete(name);
    return;
  }
  loginName = String_new();
  if (!StringMap_getString(argumentMap,"loginName",loginName,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected loginName=<name>");
    String_delete(loginName);
    String_delete(name);
    return;
  }
  password = String_new();
  if (!StringMap_getString(argumentMap,"password",password,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected password=<password>");
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  publicKey = String_new();
  if (!StringMap_getString(argumentMap,"publicKey",publicKey,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected publicKey=<data>");
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  privateKey = String_new();
  if (!StringMap_getString(argumentMap,"privateKey",privateKey,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected privateKey=<data>");
    String_delete(privateKey);
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"maxConnectionCount",&maxConnectionCount,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected maxConnectionCount=<n>");
    String_delete(privateKey);
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }
  if (!StringMap_getUInt64(argumentMap,"maxStorageSize",&maxStorageSize,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected maxStorageSize=<n>");
    String_delete(privateKey);
    String_delete(publicKey);
    String_delete(password);
    String_delete(loginName);
    String_delete(name);
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find storage server
    serverNode = LIST_FIND(&globalOptions.serverList,serverNode,serverNode->id == serverId);
    if (serverNode == NULL)
    {
      Semaphore_unlock(&globalOptions.serverList.lock);
      sendClientResult(clientInfo,serverId,TRUE,ERROR_JOB_NOT_FOUND,"storage server with id #%u not found",serverId);
      String_delete(privateKey);
      String_delete(publicKey);
      String_delete(password);
      String_delete(loginName);
      String_delete(name);
      return;
    }

    // update storage server settings
    String_set(serverNode->server.name,name);
    switch (serverType)
    {
      case SERVER_TYPE_NONE:
        break;
      case SERVER_TYPE_FILE:
        break;
      case SERVER_TYPE_FTP:
        String_set(serverNode->server.ftp.loginName,loginName);
        if (!String_isEmpty(password))
        {
          if (serverNode->server.ftp.password == NULL)
          {
            serverNode->server.ftp.password = Password_newString(password);
          }
          else
          {
            Password_setString(serverNode->server.ftp.password,password);
          }
        }
        break;
      case SERVER_TYPE_SSH:
        serverNode->server.ssh.port = port;
        String_set(serverNode->server.ssh.loginName,loginName);
        if (!String_isEmpty(password))
        {
          if (serverNode->server.ssh.password == NULL)
          {
            serverNode->server.ssh.password = Password_newString(password);
          }
          else
          {
            Password_setString(serverNode->server.ssh.password,password);
          }
        }
        setKeyString(&serverNode->server.ssh.publicKey,publicKey);
        setKeyString(&serverNode->server.ssh.privateKey,privateKey);
        break;
      case SERVER_TYPE_WEBDAV:
        String_set(serverNode->server.webDAV.loginName,loginName);
        if (!String_isEmpty(password))
        {
          if (serverNode->server.webDAV.password == NULL)
          {
            serverNode->server.webDAV.password = Password_newString(password);
          }
          else
          {
            Password_setString(serverNode->server.webDAV.password,password);
          }
        }
        setKeyString(&serverNode->server.webDAV.publicKey,publicKey);
        setKeyString(&serverNode->server.webDAV.privateKey,privateKey);
        break;
    }
    serverNode->server.maxConnectionCount = maxConnectionCount;
    serverNode->server.maxStorageSize     = maxStorageSize;

    // update config files
    error = updateConfig();
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&globalOptions.serverList.lock);
      sendClientResult(clientInfo,id,TRUE,error,"write config file fail");
      String_delete(privateKey);
      String_delete(publicKey);
      String_delete(password);
      String_delete(loginName);
      String_delete(name);
      return;
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(privateKey);
  String_delete(publicKey);
  String_delete(password);
  String_delete(loginName);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_serverListRemove
* Purpose: delete entry in server list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            id=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_serverListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint          serverId;
  SemaphoreLock semaphoreLock;
  ServerNode    *serverNode;
  Errors        error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get storage server id
  if (!StringMap_getUInt(argumentMap,"id",&serverId,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected id=<n>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find storage server
    serverNode = LIST_FIND(&globalOptions.serverList,serverNode,serverNode->id == serverId);
    if (serverNode == NULL)
    {
      Semaphore_unlock(&globalOptions.serverList.lock);
      sendClientResult(clientInfo,serverId,TRUE,ERROR_JOB_NOT_FOUND,"storage server with id #%u not found",serverId);
      return;
    }

    // delete storage server
    List_removeAndFree(&globalOptions.serverList,serverNode,CALLBACK((ListNodeFreeFunction)freeServerNode,NULL));

    // update config file
    error = updateConfig();
    if (error != ERROR_NONE)
    {
      Semaphore_unlock(&globalOptions.serverList.lock);
      sendClientResult(clientInfo,id,TRUE,error,"write config file fail");
      return;
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_abort
* Purpose: abort command execution
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            commandId=<command id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_abort(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint commandId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get command id
  if (!StringMap_getUInt(argumentMap,"commandId",&commandId,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected commandId=<command id>");
    return;
  }

  // abort command
  clientInfo->abortCommandId = commandId;
//TODO:
clientInfo->abortFlag = TRUE;

  // format result
  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_status
* Purpose: get status
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            state=<status>
*            time=<pause time [s]>
\***********************************************************************/

LOCAL void serverCommand_status(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint64 now;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // format result
  Semaphore_lock(&serverStateLock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT);
  {
    switch (serverState)
    {
      case SERVER_STATE_RUNNING:
        sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"state=running");
        break;
      case SERVER_STATE_PAUSE:
        now = Misc_getCurrentDateTime();
        sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"state=pause time=%llu",(pauseEndDateTime > now) ? pauseEndDateTime-now : 0LL);
        break;
      case SERVER_STATE_SUSPENDED:
        sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"state=suspended");
        break;
    }
  }
  Semaphore_unlock(&serverStateLock);
}

/***********************************************************************\
* Name   : serverCommand_pause
* Purpose: pause job execution
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            time=<pause time [s]>
*            modeMask=CREATE,STORAGE,RESTORE,INDEX_UPDATE
*          Result:
\***********************************************************************/

LOCAL void serverCommand_pause(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint            pauseTime;
  String          modeMask;
  SemaphoreLock   semaphoreLock;
  StringTokenizer stringTokenizer;
  ConstString     token;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get pause time
  if (!StringMap_getUInt(argumentMap,"time",&pauseTime,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected time=<time>");
    return;
  }
  modeMask = String_new();
  if (!StringMap_getString(argumentMap,"modeMask",modeMask,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected modeMask=CREATE,STORAGE,RESTORE,INDEX_UPDATE|ALL");
    return;
  }

  // set pause time
  SEMAPHORE_LOCKED_DO(semaphoreLock,&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    serverState = SERVER_STATE_PAUSE;
    if (modeMask == NULL)
    {
      pauseFlags.create      = TRUE;
      pauseFlags.storage     = TRUE;
      pauseFlags.restore     = TRUE;
      pauseFlags.indexUpdate = TRUE;
    }
    else
    {
      String_initTokenizer(&stringTokenizer,modeMask,STRING_BEGIN,",",NULL,TRUE);
      while (String_getNextToken(&stringTokenizer,&token,NULL))
      {
        if (String_equalsIgnoreCaseCString(token,"CREATE"))
        {
          pauseFlags.create      = TRUE;
        }
        else if (String_equalsIgnoreCaseCString(token,"STORAGE"))
        {
          pauseFlags.storage     = TRUE;
        }
        else if (String_equalsIgnoreCaseCString(token,"RESTORE"))
        {
          pauseFlags.restore     = TRUE;
        }
        else if (String_equalsIgnoreCaseCString(token,"INDEX_UPDATE"))
        {
          pauseFlags.indexUpdate = TRUE;
        }
        else if (String_equalsIgnoreCaseCString(token,"ALL"))
        {
          pauseFlags.create      = TRUE;
          pauseFlags.storage     = TRUE;
          pauseFlags.restore     = TRUE;
          pauseFlags.indexUpdate = TRUE;
        }
      }
      String_doneTokenizer(&stringTokenizer);
    }
    pauseEndDateTime = Misc_getCurrentDateTime()+(uint64)pauseTime;

    String_clear(modeMask);
    if (pauseFlags.create     ) String_joinCString(modeMask,"create",',');
    if (pauseFlags.storage    ) String_joinCString(modeMask,"storage",',');
    if (pauseFlags.restore    ) String_joinCString(modeMask,"restore",',');
    if (pauseFlags.indexUpdate) String_joinCString(modeMask,"indexUpdate",',');
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               "Pause server for %dmin: %s\n",
               pauseTime/60,
               String_cString(modeMask)
              );
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(modeMask);
}

/***********************************************************************\
* Name   : serverCommand_suspend
* Purpose: suspend job execution
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            modeMask=CREATE,STORAGE,RESTORE,INDEX_UPDATE
*          Result:
\***********************************************************************/

LOCAL void serverCommand_suspend(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String          modeMask;
  SemaphoreLock   semaphoreLock;
  StringTokenizer stringTokenizer;
  ConstString     token;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // get mode
  modeMask = String_new();
  if (!StringMap_getString(argumentMap,"modeMask",modeMask,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected modeMask=CREATE,STORAGE,RESTORE,INDEX_UPDATE");
    return;
  }

  // set suspend
  SEMAPHORE_LOCKED_DO(semaphoreLock,&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    serverState = SERVER_STATE_SUSPENDED;
    if (modeMask == NULL)
    {
      pauseFlags.create      = TRUE;
      pauseFlags.storage     = TRUE;
      pauseFlags.restore     = TRUE;
      pauseFlags.indexUpdate = TRUE;
    }
    else
    {
      String_initTokenizer(&stringTokenizer,modeMask,STRING_BEGIN,",",NULL,TRUE);
      while (String_getNextToken(&stringTokenizer,&token,NULL))
      {
        if (String_equalsIgnoreCaseCString(token,"CREATE"))
        {
          pauseFlags.create      = TRUE;
        }
        else if (String_equalsIgnoreCaseCString(token,"STORAGE"))
        {
          pauseFlags.storage     = TRUE;
        }
        else if (String_equalsIgnoreCaseCString(token,"RESTORE"))
        {
          pauseFlags.restore     = TRUE;
        }
        else if (String_equalsIgnoreCaseCString(token,"INDEX_UPDATE"))
        {
          pauseFlags.indexUpdate = TRUE;
        }
      }
      String_doneTokenizer(&stringTokenizer);
    }
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               "suspend server\n"
              );
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(modeMask);
}

/***********************************************************************\
* Name   : serverCommand_continue
* Purpose: continue job execution
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_continue(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  SemaphoreLock semaphoreLock;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // clear pause/suspend
  SEMAPHORE_LOCKED_DO(semaphoreLock,&serverStateLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    serverState            = SERVER_STATE_RUNNING;
    pauseFlags.create      = FALSE;
    pauseFlags.storage     = FALSE;
    pauseFlags.restore     = FALSE;
    pauseFlags.indexUpdate = FALSE;
    logMessage(NULL,  // logHandle,
               LOG_TYPE_ALWAYS,
               "Continue server\n"
              );
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_deviceList
* Purpose: get device list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            name=<name>
*            size=<n [bytes]>
*            mounted=yes|no
\***********************************************************************/

LOCAL void serverCommand_deviceList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  Errors           error;
  DeviceListHandle deviceListHandle;
  String           deviceName;
  DeviceInfo       deviceInfo;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // open device list
  error = Device_openDeviceList(&deviceListHandle);
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,error,"cannot open device list: %s",Error_getText(error));
    return;
  }

  // read device list entries
  deviceName = String_new();
  while (!Device_endOfDeviceList(&deviceListHandle))
  {
    // read device list entry
    error = Device_readDeviceList(&deviceListHandle,deviceName);
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,error,"cannot read device list: %s",Error_getText(error));
      Device_closeDeviceList(&deviceListHandle);
      String_delete(deviceName);
      return;
    }

    // get device info
    error = Device_getDeviceInfo(&deviceInfo,deviceName);
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,error,"cannot read device info: %s",Error_getText(error));
      Device_closeDeviceList(&deviceListHandle);
      String_delete(deviceName);
      return;
    }

    if (   (deviceInfo.type == DEVICE_TYPE_BLOCK)
        && (deviceInfo.size > 0)
       )
    {
      sendClientResult(clientInfo,
                       id,FALSE,ERROR_NONE,
                       "name=%'S size=%lld mounted=%y",
                       deviceName,
                       deviceInfo.size,
                       deviceInfo.mounted
                      );
    }
  }
  String_delete(deviceName);

  // close device
  Device_closeDeviceList(&deviceListHandle);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_rootList
* Purpose: root list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            name=<name> size=<n [bytes]>
\***********************************************************************/

LOCAL void serverCommand_rootList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  Errors         error;
  RootListHandle rootListHandle;
  String         name;
  DeviceInfo     deviceInfo;
  uint64         size;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // open root list
  error = File_openRootList(&rootListHandle);
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,error,"open root list fail: %s",Error_getText(error));
    return;
  }

  // read root list entries
  name = String_new();
  while (!File_endOfRootList(&rootListHandle))
  {
    error = File_readRootList(&rootListHandle,name);
    if (error == ERROR_NONE)
    {
      error = Device_getDeviceInfo(&deviceInfo,name);
      if (error == ERROR_NONE)
      {
        size = deviceInfo.size;
      }
      else
      {
        size = 0;
      }

      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "name=%'S size=%llu",
                       name,
                       size
                      );
    }
    else
    {
      sendClientResult(clientInfo,id,FALSE,error,"open root list fail");
    }
  }
  String_delete(name);

  // close root list
  File_closeRootList(&rootListHandle);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_fileInfo
* Purpose: get file info
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<name>
*          Result:
*            fileType=FILE name=<name> size=<n [bytes]> dateTime=<time stamp> noDump=yes|no
*            fileType=DIRECTORY name=<name> dateTime=<time stamp> noBackup=yes|no noDump=yes|no
*            fileType=LINK name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=HARDLINK name=<name> size=<n [bytes]> dateTime=<time stamp> noDump=yes|no
*            fileType=DEVICE CHARACTER name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=DEVICE BLOCK name=<name> size=<n [bytes]> dateTime=<time stamp> noDump=yes|no
*            fileType=FIFO name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=SOCKET name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=SPECIAL name=<name> dateTime=<time stamp> noDump=yes|no
\***********************************************************************/

LOCAL void serverCommand_fileInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String   name;
  Errors   error;
  String              noBackupFileName;
  FileInfo fileInfo;
  bool                noBackupExists;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get name
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    return;
  }

  // read file info
  noBackupFileName = String_new();
  error = File_getFileInfo(name,&fileInfo);
  if (error == ERROR_NONE)
  {
    switch (fileInfo.type)
    {
      case FILE_TYPE_FILE:
        sendClientResult(clientInfo,id,TRUE,ERROR_NONE,
                         "fileType=FILE name=%'S size=%llu dateTime=%llu noDump=%y",
                         name,
                         fileInfo.size,
                         fileInfo.timeModified,
                         ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                        );
        break;
      case FILE_TYPE_DIRECTORY:
        // check if .nobackup exists
        noBackupExists = FALSE;
        if (!noBackupExists) noBackupExists = File_isFile(File_appendFileNameCString(File_setFileName(noBackupFileName,name),".nobackup"));
        if (!noBackupExists) noBackupExists = File_isFile(File_appendFileNameCString(File_setFileName(noBackupFileName,name),".NOBACKUP"));

        sendClientResult(clientInfo,id,TRUE,ERROR_NONE,
                         "fileType=DIRECTORY name=%'S dateTime=%llu noBackup=%y noDump=%y",
                         name,
                         fileInfo.timeModified,
                         noBackupExists,
                         ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                        );
        break;
      case FILE_TYPE_LINK:
        sendClientResult(clientInfo,id,TRUE,ERROR_NONE,
                         "fileType=LINK name=%'S dateTime=%llu noDump=%y",
                         name,
                         fileInfo.timeModified,
                         ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                        );
        break;
      case FILE_TYPE_HARDLINK:
        sendClientResult(clientInfo,id,TRUE,ERROR_NONE,
                         "fileType=HARDLINK name=%'S dateTime=%llu noDump=%y",
                         name,
                         fileInfo.timeModified,
                         ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                        );
        break;
      case FILE_TYPE_SPECIAL:
        switch (fileInfo.specialType)
        {
          case FILE_SPECIAL_TYPE_CHARACTER_DEVICE:
            sendClientResult(clientInfo,id,TRUE,ERROR_NONE,
                             "fileType=SPECIAL name=%'S specialType=DEVICE_CHARACTER dateTime=%llu noDump=%y",
                             name,
                             fileInfo.timeModified,
                             ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                            );
            break;
          case FILE_SPECIAL_TYPE_BLOCK_DEVICE:
            sendClientResult(clientInfo,id,TRUE,ERROR_NONE,
                             "fileType=SPECIAL name=%'S size=%llu specialType=DEVICE_BLOCK dateTime=%llu noDump=%y",
                             name,
                             fileInfo.size,
                             fileInfo.timeModified,
                             ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                            );
            break;
          case FILE_SPECIAL_TYPE_FIFO:
            sendClientResult(clientInfo,id,TRUE,ERROR_NONE,
                             "fileType=SPECIAL name=%'S specialType=FIFO dateTime=%llu noDump=%y",
                             name,
                             fileInfo.timeModified,
                             ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                            );
            break;
          case FILE_SPECIAL_TYPE_SOCKET:
            sendClientResult(clientInfo,id,TRUE,ERROR_NONE,
                             "fileType=SPECIAL name=%'S specialType=SOCKET dateTime=%llu noDump=%y",
                             name,
                             fileInfo.timeModified,
                             ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                            );
            break;
          default:
            sendClientResult(clientInfo,id,TRUE,ERROR_NONE,
                             "fileType=SPECIAL name=%'S specialType=OTHER dateTime=%llu noDump=%y",
                             name,
                             fileInfo.timeModified,
                             ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                            );
            break;
        }
        break;
      default:
        // skipped
        break;
    }
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,error,"read file info fail: %s",Error_getText(error));
  }
  String_delete(noBackupFileName);

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_fileList
* Purpose: file list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            directory=<name>
*          Result:
*            fileType=FILE name=<name> size=<n [bytes]> dateTime=<time stamp> noDump=yes|no
*            fileType=DIRECTORY name=<name> dateTime=<time stamp> noBackup=yes|no noDump=yes|no
*            fileType=LINK name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=HARDLINK name=<name> size=<n [bytes]> dateTime=<time stamp> noDump=yes|no
*            fileType=DEVICE CHARACTER name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=DEVICE BLOCK name=<name> size=<n [bytes]> dateTime=<time stamp> noDump=yes|no
*            fileType=FIFO name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=SOCKET name=<name> dateTime=<time stamp> noDump=yes|no
*            fileType=SPECIAL name=<name> dateTime=<time stamp> noDump=yes|no
\***********************************************************************/

LOCAL void serverCommand_fileList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String              directory;
  Errors              error;
  DirectoryListHandle directoryListHandle;
  String              fileName;
  String              noBackupFileName;
  FileInfo            fileInfo;
  bool                noBackupExists;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get directory
  directory = String_new();
  if (!StringMap_getString(argumentMap,"directory",directory,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected directory=<name>");
    return;
  }

  // open directory
  error = File_openDirectoryList(&directoryListHandle,
                                 directory
                                );
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,error,"open directory fail: %s",Error_getText(error));
    String_delete(directory);
    return;
  }

  // read directory entries
  fileName         = String_new();
  noBackupFileName = String_new();
  while (!File_endOfDirectoryList(&directoryListHandle))
  {
    error = File_readDirectoryList(&directoryListHandle,fileName);
    if (error == ERROR_NONE)
    {
      error = File_getFileInfo(fileName,&fileInfo);
      if (error == ERROR_NONE)
      {
        switch (fileInfo.type)
        {
          case FILE_TYPE_FILE:
            sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                             "fileType=FILE name=%'S size=%llu dateTime=%llu noDump=%y",
                             fileName,
                             fileInfo.size,
                             fileInfo.timeModified,
                             ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                            );
            break;
          case FILE_TYPE_DIRECTORY:
            // check if .nobackup exists
            noBackupExists = FALSE;
            if (!noBackupExists) noBackupExists = File_isFile(File_appendFileNameCString(File_setFileName(noBackupFileName,fileName),".nobackup"));
            if (!noBackupExists) noBackupExists = File_isFile(File_appendFileNameCString(File_setFileName(noBackupFileName,fileName),".NOBACKUP"));

            sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                             "fileType=DIRECTORY name=%'S dateTime=%llu noBackup=%y noDump=%y",
                             fileName,
                             fileInfo.timeModified,
                             noBackupExists,
                             ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                            );
            break;
          case FILE_TYPE_LINK:
            sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                             "fileType=LINK name=%'S dateTime=%llu noDump=%y",
                             fileName,
                             fileInfo.timeModified,
                             ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                            );
            break;
          case FILE_TYPE_HARDLINK:
            sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                             "fileType=HARDLINK name=%'S dateTime=%llu noDump=%y",
                             fileName,
                             fileInfo.timeModified,
                             ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                            );
            break;
          case FILE_TYPE_SPECIAL:
            switch (fileInfo.specialType)
            {
              case FILE_SPECIAL_TYPE_CHARACTER_DEVICE:
                sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                                 "fileType=SPECIAL name=%'S specialType=DEVICE_CHARACTER dateTime=%llu noDump=%y",
                                 fileName,
                                 fileInfo.timeModified,
                                 ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                                );
                break;
              case FILE_SPECIAL_TYPE_BLOCK_DEVICE:
                sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                                 "fileType=SPECIAL name=%'S size=%llu specialType=DEVICE_BLOCK dateTime=%llu noDump=%y",
                                 fileName,
                                 fileInfo.size,
                                 fileInfo.timeModified,
                                 ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                                );
                break;
              case FILE_SPECIAL_TYPE_FIFO:
                sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                                 "fileType=SPECIAL name=%'S specialType=FIFO dateTime=%llu noDump=%y",
                                 fileName,
                                 fileInfo.timeModified,
                                 ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                                );
                break;
              case FILE_SPECIAL_TYPE_SOCKET:
                sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                                 "fileType=SPECIAL name=%'S specialType=SOCKET dateTime=%llu noDump=%y",
                                 fileName,
                                 fileInfo.timeModified,
                                 ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                                );
                break;
              default:
                sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                                 "fileType=SPECIAL name=%'S specialType=OTHER dateTime=%llu noDump=%y",
                                 fileName,
                                 fileInfo.timeModified,
                                 ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
                                );
                break;
            }
            break;
          default:
            // skipped
            break;
        }
      }
      else
      {
        sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                         "get file info fail: %s",
                         Error_getText(error)
                        );
      }
    }
    else
    {
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "read directory entry file: %s",
                       Error_getText(error)
                      );
    }
  }
  String_delete(noBackupFileName);
  String_delete(fileName);

  // close directory
  File_closeDirectoryList(&directoryListHandle);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(directory);
}

/***********************************************************************\
* Name   : serverCommand_fileAttributeGet
* Purpose: get file attribute
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            attribute=NOBACKUP|NODUMP
*            name=<name>
*          Result:
*            value=<value>
\***********************************************************************/

LOCAL void serverCommand_fileAttributeGet(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  String        attribute;
  String        name;
  SemaphoreLock semaphoreLock;
  const JobNode *jobNode;
  String        noBackupFileName;
  bool          noBackupExists;
  Errors        error;
  FileInfo      fileInfo;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, name
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  attribute = String_new();
  if (!StringMap_getString(argumentMap,"attribute",attribute,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected attribute=<name>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    String_delete(attribute);
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(name);
      String_delete(attribute);
      return;
    }

    // send attribute value
    if      (String_equalsCString(attribute,"NOBACKUP"))
    {
      noBackupFileName = String_new();

      noBackupExists = FALSE;
      if (File_isDirectory(name))
      {
        if (!noBackupExists) noBackupExists = File_isFile(File_appendFileNameCString(File_setFileName(noBackupFileName,name),".nobackup"));
        if (!noBackupExists) noBackupExists = File_isFile(File_appendFileNameCString(File_setFileName(noBackupFileName,name),".NOBACKUP"));
      }
      sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"value=%y",FALSE);

      String_delete(noBackupFileName);
    }
    else if (String_equalsCString(attribute,"NODUMP"))
    {
      error = File_getFileInfo(name,&fileInfo);
      if (error == ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"value=%y",(fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP);
      }
      else
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"get file info fail for '%S': %s",attribute,name,Error_getText(error));
      }
    }
    else
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown file attribute %S for '%S'",attribute,name);
    }
  }

  // free resources
  String_delete(name);
  String_delete(attribute);
}

/***********************************************************************\
* Name   : serverCommand_fileAttributeSet
* Purpose: set file attribute
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            attribute=NOBACKUP|NODUMP
*            name=<name>
*            value=<value>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_fileAttributeSet(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  String        attribute;
  String        name;
  String        value;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  String        noBackupFileName;
  Errors        error;
  FileInfo      fileInfo;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, name, value
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  attribute = String_new();
  if (!StringMap_getString(argumentMap,"attribute",attribute,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected attribute=<name>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    String_delete(attribute);
    return;
  }
  value = String_new();
  StringMap_getString(argumentMap,"value",value,NULL);
//TODO: value still not used
UNUSED_VARIABLE(value);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(value);
      String_delete(name);
      String_delete(attribute);
      return;
    }

    // set attribute
    if      (String_equalsCString(attribute,"NOBACKUP"))
    {
      if (!File_isDirectory(name))
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_NOT_A_DIRECTORY,"not a directory '%S'",name);
        Semaphore_unlock(&jobList.lock);
        String_delete(value);
        String_delete(name);
        String_delete(attribute);
        return;
      }

      noBackupFileName = String_new();
      File_appendFileNameCString(File_setFileName(noBackupFileName,name),".nobackup");
      if (!File_exists(noBackupFileName))
      {
        error = File_touch(noBackupFileName);
        if (error != ERROR_NONE)
        {
          String_delete(noBackupFileName);
          sendClientResult(clientInfo,id,TRUE,error,"Cannot create .nobackup file: %s",Error_getText(error));
          Semaphore_unlock(&jobList.lock);
          String_delete(value);
          String_delete(name);
          String_delete(attribute);
          return;
        }
      }
      String_delete(noBackupFileName);

      sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
    }
    else if (String_equalsCString(attribute,"NODUMP"))
    {
      error = File_getFileInfo(name,&fileInfo);
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,error,"get file info fail for '%S': %s",name,Error_getText(error));
        Semaphore_unlock(&jobList.lock);
        String_delete(value);
        String_delete(name);
        String_delete(attribute);
        return;
      }

      fileInfo.attributes |= FILE_ATTRIBUTE_NO_DUMP;
      error = File_setFileInfo(name,&fileInfo);
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,error,"set attribute no-dump fail for '%S': %s",name,Error_getText(error));
        Semaphore_unlock(&jobList.lock);
        String_delete(value);
        String_delete(name);
        String_delete(attribute);
        return;
      }

      sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
    }
    else
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown file attribute %S for '%S'",attribute,name);
    }
  }

  // free resources
  String_delete(value);
  String_delete(name);
  String_delete(attribute);
}

/***********************************************************************\
* Name   : serverCommand_fileAttributeClear
* Purpose: clear file attribute
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            attribute=NOBACKUP|NODUMP
*            name=<name>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_fileAttributeClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  String        attribute;
  String        name;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  String        noBackupFileName;
  Errors        error;
  FileInfo      fileInfo;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, name
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  attribute = String_new();
  if (!StringMap_getString(argumentMap,"attribute",attribute,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected attribute=<name>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    String_delete(attribute);
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(name);
      String_delete(attribute);
      return;
    }

    // clear attribute
    if      (String_equalsCString(attribute,"NOBACKUP"))
    {
      if (!File_isDirectory(name))
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_NOT_A_DIRECTORY,"not a directory '%S'",name);
        Semaphore_unlock(&jobList.lock);
        String_delete(name);
        String_delete(attribute);
        return;
      }

      noBackupFileName = String_new();
      File_appendFileNameCString(File_setFileName(noBackupFileName,name),".nobackup");
      if (File_exists(noBackupFileName))
      {
        error = File_delete(noBackupFileName,FALSE);
        if (error != ERROR_NONE)
        {
          String_delete(noBackupFileName);
          sendClientResult(clientInfo,id,TRUE,error,"Cannot delete .nobackup file: %s",Error_getText(error));
          Semaphore_unlock(&jobList.lock);
          String_delete(name);
          String_delete(attribute);
          return;
        }
      }
      String_delete(noBackupFileName);

      sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
    }
    else if (String_equalsCString(attribute,"NODUMP"))
    {
      error = File_getFileInfo(name,&fileInfo);
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,error,"get file info fail for '%S': %s",name,Error_getText(error));
        Semaphore_unlock(&jobList.lock);
        String_delete(name);
        String_delete(attribute);
        return;
      }

      if ((fileInfo.attributes & FILE_ATTRIBUTE_NO_DUMP) == FILE_ATTRIBUTE_NO_DUMP)
      {
        fileInfo.attributes &= ~FILE_ATTRIBUTE_NO_DUMP;
        error = File_setFileInfo(name,&fileInfo);
        if (error != ERROR_NONE)
        {
          sendClientResult(clientInfo,id,TRUE,error,"set attribute no-dump fail for '%S': %s",name,Error_getText(error));
          Semaphore_unlock(&jobList.lock);
          String_delete(name);
          String_delete(attribute);
          return;
        }
      }

      sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
    }
    else
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown file attribute %S for '%S'",attribute,name);
    }
  }

  // free resources
  String_delete(name);
  String_delete(attribute);
}

/***********************************************************************\
* Name   : serverCommand_directoryInfo
* Purpose: get directory info
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<directory>
*            timeout=<n [s]>
*          Result:
*            count=<file count> size=<total file size> timedOut=yes|no
\***********************************************************************/

LOCAL void serverCommand_directoryInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String            name;
  int64             timeout;
  DirectoryInfoNode *directoryInfoNode;
  uint64            fileCount;
  uint64            fileSize;
  bool              timedOut;
  FileInfo          fileInfo;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get path/file name
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<directory name>");
    String_delete(name);
    return;
  }
  StringMap_getInt64(argumentMap,"timeout",&timeout,0LL);

  fileCount = 0LL;
  fileSize  = 0LL;
  timedOut  = FALSE;
  if (File_isDirectory(name))
  {
    // find/create directory info
    directoryInfoNode = findDirectoryInfo(&clientInfo->directoryInfoList,name);
    if (directoryInfoNode == NULL)
    {
      directoryInfoNode = newDirectoryInfo(name);
      List_append(&clientInfo->directoryInfoList,directoryInfoNode);
    }

    // get total size of directoy/file
    getDirectoryInfo(directoryInfoNode,
                     timeout,
                     &fileCount,
                     &fileSize,
                     &timedOut
                    );
  }
  else
  {
    // get file size
    if (File_getFileInfo(name,&fileInfo) == ERROR_NONE)
    {
      fileCount = 1LL;
      fileSize  = fileInfo.size;
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"count=%llu size=%llu timedOut=%y",fileCount,fileSize,timedOut);

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_testScript
* Purpose: test script
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            script=<directory>
*          Result:
*            line=<text>
\***********************************************************************/

LOCAL void serverCommand_testScript(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String script;
  Errors error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get script
  script = String_new();
  if (!StringMap_getString(argumentMap,"script",script,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected script=<script>");
    String_delete(script);
    return;
  }

  // execute script
  error = Misc_executeScript(String_cString(script),
                             CALLBACK_INLINE(void,(ConstString line, void *userData),
                             {
                               UNUSED_VARIABLE(userData);

                               sendClientResult(clientInfo,id,FALSE,ERROR_NONE,"line=%'S",line);
                             },NULL),
                             CALLBACK_INLINE(void,(ConstString line, void *userData),
                             {
                               UNUSED_VARIABLE(userData);

                               sendClientResult(clientInfo,id,FALSE,ERROR_NONE,"line=%'S",line);
                             },NULL)
                            );

  sendClientResult(clientInfo,id,TRUE,error,"");

  // free resources
  String_delete(script);
}

/***********************************************************************\
* Name   : serverCommand_jobOptionGet
* Purpose: get job option
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            name=<name>
*          Result:
*            value=<value>
\***********************************************************************/

LOCAL void serverCommand_jobOptionGet(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString      (jobUUID,MISC_UUID_STRING_LENGTH);
  String            name;
  SemaphoreLock     semaphoreLock;
  const JobNode     *jobNode;
  int               i;
  String            s;
  ConfigValueFormat configValueFormat;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, name
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(name);
      return;
    }

    // find config value
    i = ConfigValue_valueIndex(JOB_CONFIG_VALUES,NULL,String_cString(name));
    if (i < 0)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown job config value '%S'",name);
      Semaphore_unlock(&jobList.lock);
      String_delete(name);
      return;
    }

    // send value
    s = String_new();
    ConfigValue_formatInit(&configValueFormat,
                           &JOB_CONFIG_VALUES[i],
                           CONFIG_VALUE_FORMAT_MODE_VALUE,
                           jobNode
                          );
    ConfigValue_format(&configValueFormat,s);
    ConfigValue_formatDone(&configValueFormat);
    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"value=%S",s);
    String_delete(s);
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_jobOptionSet
* Purpose: set job option
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            name=<name>
*            value=<value>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobOptionSet(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  String        name,value;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, name, value
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    return;
  }
  value = String_new();
  if (!StringMap_getString(argumentMap,"value",value,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected value=<value>");
    String_delete(name);
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(value);
      String_delete(name);
      return;
    }

    // parse
    if (ConfigValue_parse(String_cString(name),
                          String_cString(value),
                          JOB_CONFIG_VALUES,
                          NULL, // sectionName
                          NULL, // outputHandle
                          NULL, // errorPrefix
                          NULL, // warningPrefix
                          jobNode
                         )
       )
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

      // set modified
      jobNode->modifiedFlag = TRUE;
    }
    else
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown job config value '%S'",name);
    }
  }

  // free resources
  String_delete(value);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_jobOptionDelete
* Purpose: delete job option
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            name=<name>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobOptionDelete(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  String        name;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  int           i;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, name
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(name);
      return;
    }

    // find config value
    i = ConfigValue_valueIndex(JOB_CONFIG_VALUES,NULL,String_cString(name));
    if (i < 0)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown job config value '%S'",name);
      Semaphore_unlock(&jobList.lock);
      String_delete(name);
      return;
    }

    // delete value
#ifndef WERROR
#warning todo?
#endif
//    ConfigValue_reset(&JOB_CONFIG_VALUES[z],jobNode);
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_jobList
* Purpose: get job list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            jobUUID=<uuid> \
*            name=<name> \
*            state=<state> \
*            hostName=<name> \
*            hostPort=<n> \
*            archiveType=<type> \
*            archivePartSize=<part size> \
*            deltaCompressAlgorithm=<delta compress algorithm> \
*            byteCompressAlgorithm=<byte compress alrogithm> \
*            cryptAlgorithm=<crypt algorithm> \
*            cryptType=<crypt type> \
*            cryptPasswordMode=<password mode> \
*            lastExecutedDateTime=<last executed time> \
*            estimatedRestTime=<estimated rest time>
\***********************************************************************/

LOCAL void serverCommand_jobList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  SemaphoreLock semaphoreLock;
  const JobNode *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
//TODO: LIST_ITERATEX()
    jobNode = jobList.head;
    while (   (jobNode != NULL)
           && !isCommandAborted(clientInfo,id)
          )
    {
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "jobUUID=%S name=%'S state=%'s remoteHostName=%'S remoteHostPort=%d remoteHostForceSSL=%y archiveType=%s archivePartSize=%llu deltaCompressAlgorithm=%s byteCompressAlgorithm=%s cryptAlgorithm=%'s cryptType=%'s cryptPasswordMode=%'s lastExecutedDateTime=%llu estimatedRestTime=%lu",
                       jobNode->uuid,
                       jobNode->name,
                       getJobStateText(jobNode->state,&jobNode->jobOptions),
                       jobNode->remoteHost.name,
                       jobNode->remoteHost.port,
                       jobNode->remoteHost.forceSSL,
                       ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,
                                                  (   (jobNode->archiveType == ARCHIVE_TYPE_FULL        )
                                                   || (jobNode->archiveType == ARCHIVE_TYPE_INCREMENTAL )
                                                   || (jobNode->archiveType == ARCHIVE_TYPE_DIFFERENTIAL)
                                                  )
                                                    ? jobNode->archiveType
                                                    : jobNode->jobOptions.archiveType,
                                                  NULL
                                                 ),
                       jobNode->jobOptions.archivePartSize,
                       Compress_algorithmToString(jobNode->jobOptions.compressAlgorithms.delta),
                       Compress_algorithmToString(jobNode->jobOptions.compressAlgorithms.byte),
                       (jobNode->jobOptions.cryptAlgorithm != CRYPT_ALGORITHM_NONE) ? Crypt_algorithmToString(jobNode->jobOptions.cryptAlgorithm,"unknown") : "none",
                       (jobNode->jobOptions.cryptAlgorithm != CRYPT_ALGORITHM_NONE) ? Crypt_typeToString(jobNode->jobOptions.cryptType) : "none",
                       ConfigValue_selectToString(CONFIG_VALUE_PASSWORD_MODES,jobNode->jobOptions.cryptPasswordMode,NULL),
                       jobNode->lastExecutedDateTime,
                       jobNode->runningInfo.estimatedRestTime
                      );

      jobNode = jobNode->next;
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_jobInfo
* Purpose: get job info
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
*            lastCreatedDateTime=<time stamp>
*            lastErrorMessage=<text>
*            executionCount=<n>
*            averageDuration=<n>
*            totalEntityCount=<n>
*            totalStorageCount=<n>
*            totalStorageSize=<n>
*            totalEntryCount=<n>
*            totalEntrySize=<n>
\***********************************************************************/

LOCAL void serverCommand_jobInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock semaphoreLock;
  const JobNode *jobNode;
  String        lastErrorMessage;
  uint64        lastCreatedDateTime;
  ulong         executionCount;
  uint64        averageDuration;
  ulong         totalEntityCount;
  ulong         totalStorageCount;
  uint64        totalStorageSize;
  ulong         totalEntryCount;
  uint64        totalEntrySize;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }

  // init variables
  lastErrorMessage = String_new();

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // get job info
    if (!Index_findUUIDByJobUUID(indexHandle,
                                 jobNode->uuid,
                                 NULL,  // uuidId,
                                 &lastCreatedDateTime,
                                 lastErrorMessage,
                                 &executionCount,
                                 &averageDuration,
                                 &totalEntityCount,
                                 &totalStorageCount,
                                 &totalStorageSize,
                                 &totalEntryCount,
                                 &totalEntrySize
                                )
       )
    {
      lastCreatedDateTime = 0LL;
      String_clear(lastErrorMessage);
      executionCount      = 0L;
      averageDuration     = 0LL;
      totalEntityCount    = 0L;
      totalStorageCount   = 0L;
      totalStorageSize    = 0LL;
      totalEntryCount     = 0L;
      totalEntrySize      = 0LL;
    }

    // format and send result
    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,
                     "lastCreatedDateTime=%llu lastErrorMessage=%'S executionCount=%lu averageDuration=%llu totalEntityCount=%lu totalStorageCount=%lu totalStorageSize=%llu totalEntryCount=%lu totalEntrySize=%llu",
                     lastCreatedDateTime,
                     lastErrorMessage,
                     executionCount,
                     averageDuration,
                     totalEntityCount,
                     totalStorageCount,
                     totalStorageSize,
                     totalEntryCount,
                     totalEntrySize
                    );
  }

  // free resources
  String_delete(lastErrorMessage);
}

/***********************************************************************\
* Name   : serverCommand_jobNew
* Purpose: create new job
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<name>
*            [uuid=<uuid>]
*            [master=<name>]
*          Result:
*            jobUUID=<uuid>
\***********************************************************************/

LOCAL void serverCommand_jobNew(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String        name;
  StaticString  (uuid,MISC_UUID_STRING_LENGTH);
  String        master;
  SemaphoreLock semaphoreLock;
  String        fileName;
  FileHandle    fileHandle;
  Errors        error;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get uuid, name, master
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    String_delete(name);
    return;
  }
  StringMap_getString(argumentMap,"uuid",uuid,NULL);
  master = String_new();
  StringMap_getString(argumentMap,"master",master,NULL);

  jobNode = NULL;

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    if (String_isEmpty(master))
    {
      // add new job

      // check if job already exists
      if (findJobByName(name) != NULL)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"job '%s' already exists",String_cString(name));
        Semaphore_unlock(&jobList.lock);
        String_delete(name);
        return;
      }

      // create empty job file
      fileName = File_appendFileName(File_setFileNameCString(String_new(),serverJobsDirectory),name);
      error = File_open(&fileHandle,fileName,FILE_OPEN_CREATE);
      if (error != ERROR_NONE)
      {
        String_delete(fileName);
        sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"create job '%s' fail: %s",String_cString(name),Error_getText(error));
        Semaphore_unlock(&jobList.lock);
        String_delete(name);
        return;
      }
      File_close(&fileHandle);
      (void)File_setPermission(fileName,FILE_PERMISSION_USER_READ|FILE_PERMISSION_USER_WRITE);

      // create new job
      jobNode = newJob(JOB_TYPE_CREATE,fileName,NULL);
      assert(jobNode != NULL);

      // free resources
      String_delete(fileName);

      // write job to file
      error = updateJob(jobNode);
      if (error != ERROR_NONE)
      {
        printWarning("Cannot update job '%s' (error: %s)\n",String_cString(jobNode->fileName),Error_getText(error));
      }

      // add new job to list
      List_append(&jobList,jobNode);
    }
    else
    {
      // temporary add job from master

      // create new job
      jobNode = newJob(JOB_TYPE_CREATE,NULL,uuid);
      assert(jobNode != NULL);
      String_set(jobNode->master,master);

      // add new job to list
      List_append(&jobList,jobNode);
    }

    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"jobUUID=%S",jobNode->uuid);
  }

  // free resources
  String_delete(master);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_jobClone
* Purpose: copy job
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            name=<name>
*          Result:
*            jobUUID=<new uuid>
\***********************************************************************/

LOCAL void serverCommand_jobClone(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  String        name;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  String        fileName;
  FileHandle    fileHandle;
  Errors        error;
  JobNode       *newJobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    String_delete(name);
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // check if mew job already exists
    if (findJobByName(name) != NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"job '%s' already exists",String_cString(name));
      Semaphore_unlock(&jobList.lock);
      String_delete(name);
      return;
    }

    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(name);
      return;
    }

    // create empty job file
    fileName = File_appendFileName(File_setFileNameCString(String_new(),serverJobsDirectory),name);
    error = File_open(&fileHandle,fileName,FILE_OPEN_CREATE);
    if (error != ERROR_NONE)
    {
      String_delete(fileName);
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"create job '%s' fail: %s",String_cString(name),Error_getText(error));
      Semaphore_unlock(&jobList.lock);
      String_delete(name);
      return;
    }
    File_close(&fileHandle);
    (void)File_setPermission(fileName,FILE_PERMISSION_USER_READ|FILE_PERMISSION_USER_WRITE);

    // copy job
    newJobNode = copyJob(jobNode,fileName);
    assert(newJobNode != NULL);
    Misc_getUUID(newJobNode->uuid);

    // free resources
    String_delete(fileName);

    // write job to file
    error = updateJob(jobNode);
    if (error != ERROR_NONE)
    {
      printWarning("Cannot update job '%s' (error: %s)\n",String_cString(jobNode->fileName),Error_getText(error));
    }

    // add new job to list
    List_append(&jobList,newJobNode);

    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"jobUUID=%S",newJobNode->uuid);
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_jobRename
* Purpose: rename job
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            name=<new job name>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobRename(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  String        newName;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  String        fileName;
  Errors        error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  newName = String_new();
  if (!StringMap_getString(argumentMap,"newName",newName,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected newName=<name>");
    String_delete(newName);
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // check if job already exists
    if (findJobByName(newName) != NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"job '%s' already exists",String_cString(newName));
      Semaphore_unlock(&jobList.lock);
      String_delete(newName);
      return;
    }

    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(newName);
      return;
    }

    // rename job
    fileName = File_appendFileName(File_setFileNameCString(String_new(),serverJobsDirectory),newName);
    error = File_rename(jobNode->fileName,fileName,NULL);
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"error renaming job %S: %s",jobUUID,Error_getText(error));
      Semaphore_unlock(&jobList.lock);
      String_delete(newName);
      return;
    }

    // free resources
    String_delete(fileName);

    // store new file name
    File_appendFileName(File_setFileNameCString(jobNode->fileName,serverJobsDirectory),newName);
    String_set(jobNode->name,newName);
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(newName);
}

/***********************************************************************\
* Name   : serverCommand_jobDelete
* Purpose: delete job
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobDelete(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  Errors        error;
  String        fileName,pathName,baseName;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // remove job in list if not running or requested volume
    if (isJobRunning(jobNode))
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"job %S running",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    if (jobNode->fileName != NULL)
    {
      // delete job file, state file
      error = File_delete(jobNode->fileName,FALSE);
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_JOB,"error deleting job %S: %s",jobUUID,Error_getText(error));
        Semaphore_unlock(&jobList.lock);
        return;
      }

      // delete job schedule file
      fileName = String_new();
      File_splitFileName(jobNode->fileName,&pathName,&baseName);
      File_setFileName(fileName,pathName);
      File_appendFileName(fileName,String_insertChar(baseName,0,'.'));
      String_delete(baseName);
      String_delete(pathName);
      (void)File_delete(fileName,FALSE);
      String_delete(fileName);
    }

    // remove from list
    List_removeAndFree(&jobList,jobNode,CALLBACK((ListNodeFreeFunction)freeJobNode,NULL));
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_jobStart
* Purpose: start job execution
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS
*            [dryRun=yes|no]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobStart(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes  archiveType;
  bool          dryRun;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, archive type, dry-run
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)parseArchiveType,ARCHIVE_TYPE_UNKNOWN))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
    return;
  }
  StringMap_getBool(argumentMap,"dryRun",&dryRun,FALSE);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // run job
    if  (!isJobActive(jobNode))
    {
      // trigger job
      triggerJob(jobNode,
                 archiveType,
                 dryRun,
                 NULL,  // scheduleUUID
                 NULL,  // scheduleCustomText
                 FALSE  // noStorage
                );
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_jobAbort
* Purpose: abort job execution
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobAbort(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // abort job
    if      (isJobRunning(jobNode))
    {
      jobNode->requestedAbortFlag = TRUE;
      Semaphore_signalModified(&jobList.lock);

      if (isJobLocal(jobNode))
      {
        while (isJobRunning(jobNode))
        {
          Semaphore_waitModified(&jobList.lock,LOCK_TIMEOUT);
        }
      }
      else
      {
         jobNode->runningInfo.error = Remote_jobAbort(&jobNode->remoteHost,
                                                      jobNode->uuid
                                                     );
         doneJob(jobNode);
      }
    }
    else if (isJobActive(jobNode))
    {
      jobNode->lastExecutedDateTime = Misc_getCurrentDateTime();
      jobNode->state                = JOB_STATE_NONE;
    }

    // store schedule info
    writeJobScheduleInfo(jobNode);
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_jobFlush
* Purpose: flush job data (write to disk)
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_jobFlush(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // update all job files
  updateAllJobs();

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_jobStatus
* Purpose: get job status
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
*            state=<state>
*            doneCount=<n>
*            doneSize=<n [bytes]>
*            totalEntryCount=<n>
*            totalEntrySize=<n [bytes]>
*            skippedEntryCount=<n>
*            skippedEntrySize=<n [bytes]>
*            errorEntryCount=<n>
*            errorEntrySize=<n [bytes]>
*            entriesPerSecond=<n [1/s]>
*            bytesPerSecond=<n [bytes/s]>
*            storageBytesPerSecond=<n [bytes/s]>
*            archiveSize=<n [bytes]>
*            compressionRation=<ratio>
*            entryName=<name>
*            entryBytes=<n [bytes]>
*            entryTotalSize=<n [bytes]>
*            storageName=<name>
*            storageDoneSize=<n [bytes]>
*            storageTotalSize=<n [bytes]>
*            volumeNumber=<number>
*            volumeProgress=<n [0..100]>
*            requestedVolumeNumber=<n>
*            message=<text>
\***********************************************************************/

LOCAL void serverCommand_jobStatus(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock semaphoreLock;
  const JobNode *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // format and send result
    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,
                     "state=%'s doneCount=%lu doneSize=%llu totalEntryCount=%lu totalEntrySize=%llu collectTotalSumDone=%y skippedEntryCount=%lu skippedEntrySize=%llu errorEntryCount=%lu errorEntrySize=%llu entriesPerSecond=%lf bytesPerSecond=%lf storageBytesPerSecond=%lf archiveSize=%llu compressionRatio=%lf estimatedRestTime=%lu entryName=%'S entryDoneSize=%llu entryTotalSize=%llu storageName=%'S storageDoneSize=%llu storageTotalSize=%llu volumeNumber=%d volumeProgress=%lf requestedVolumeNumber=%d message=%'S",
                     getJobStateText(jobNode->state,&jobNode->jobOptions),
                     jobNode->runningInfo.doneCount,
                     jobNode->runningInfo.doneSize,
                     jobNode->runningInfo.totalEntryCount,
                     jobNode->runningInfo.totalEntrySize,
                     jobNode->runningInfo.collectTotalSumDone,
                     jobNode->runningInfo.skippedEntryCount,
                     jobNode->runningInfo.skippedEntrySize,
                     jobNode->runningInfo.errorEntryCount,
                     jobNode->runningInfo.errorEntrySize,
                     jobNode->runningInfo.entriesPerSecond,
                     jobNode->runningInfo.bytesPerSecond,
                     jobNode->runningInfo.storageBytesPerSecond,
                     jobNode->runningInfo.archiveSize,
                     jobNode->runningInfo.compressionRatio,
                     jobNode->runningInfo.estimatedRestTime,
                     jobNode->runningInfo.entryName,
                     jobNode->runningInfo.entryDoneSize,
                     jobNode->runningInfo.entryTotalSize,
                     jobNode->runningInfo.storageName,
                     jobNode->runningInfo.storageDoneSize,
                     jobNode->runningInfo.storageTotalSize,
                     jobNode->runningInfo.volumeNumber,
                     jobNode->runningInfo.volumeProgress,
                     jobNode->requestedVolumeNumber,
                     jobNode->runningInfo.message
                    );
  }
}

/***********************************************************************\
* Name   : serverCommand_includeList
* Purpose: get job include list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
*            id=<n> entryType=<type> pattern=<text> patternType=<type>
*            ...
\***********************************************************************/

LOCAL void serverCommand_includeList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  EntryNode     *entryNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // send include list
    LIST_ITERATE(&jobNode->includeEntryList,entryNode)
    {
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "id=%u entryType=%s pattern=%'S patternType=%s",
                       entryNode->id,
                       EntryList_entryTypeToString(entryNode->type,"unknown"),
                       entryNode->string,
                       Pattern_patternTypeToString(entryNode->pattern.type,"unknown")
                      );
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_includeListClear
* Purpose: clear job include list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_includeListClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // clear include list
    EntryList_clear(&jobNode->includeEntryList);

    // notify about changed lists
    jobIncludeExcludeChanged(jobNode);

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_includeListAdd
* Purpose: add entry to job include list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            entryType=<type>
*            pattern=<text>
*            [patternType=<type>]
*          Result:
*            id=<n>
\***********************************************************************/

LOCAL void serverCommand_includeListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  EntryTypes    entryType;
  String        patternString;
  PatternTypes  patternType;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  uint          entryId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, entry type, pattern, pattern type
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"entryType",&entryType,(StringMapParseEnumFunction)EntryList_parseEntryType,ENTRY_TYPE_UNKNOWN))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected entryType=FILE|IMAGE");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(patternString);
      return;
    }

    // add to include list
    EntryList_append(&jobNode->includeEntryList,entryType,patternString,patternType,&entryId);

    // notify about changed lists
    jobIncludeExcludeChanged(jobNode);

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"id=%u",entryId);

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_includeListUpdate
* Purpose: update entry to job include list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*            entryType=<type>
*            pattern=<text>
*            [patternType=<type>]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_includeListUpdate(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  uint          entryId;
  EntryTypes    entryType;
  PatternTypes  patternType;
  String        patternString;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, id, entry type, pattern type, pattern
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&entryId,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected id=<n>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"entryType",&entryType,(StringMapParseEnumFunction)EntryList_parseEntryType,ENTRY_TYPE_UNKNOWN))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected entryType=FILE|IMAGE");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(patternString);
      return;
    }

    // update include list
    EntryList_update(&jobNode->includeEntryList,entryId,entryType,patternString,patternType);

    // notify about changed lists
    jobIncludeExcludeChanged(jobNode);

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_includeListRemove
* Purpose: remove entry from job include list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_includeListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  uint          entryId;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, id
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&entryId,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected id=<n>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // remove from include list
    if (!EntryList_remove(&jobNode->includeEntryList,entryId))
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"entry with id #%u not found",entryId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // notify about changed lists
    jobIncludeExcludeChanged(jobNode);

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeList
* Purpose: get job exclude list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
*            id=<n> pattern=<text> patternType=<type>
*            ...
\***********************************************************************/

LOCAL void serverCommand_excludeList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  PatternNode   *patternNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // send exclude list
    LIST_ITERATE(&jobNode->excludePatternList,patternNode)
    {
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "id=%u pattern=%'S patternType=%s",
                       patternNode->id,
                       patternNode->string,
                       Pattern_patternTypeToString(patternNode->pattern.type,"unknown")
                      );
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeListClear
* Purpose: clear job exclude list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_excludeListClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // clear exclude list
    PatternList_clear(&jobNode->excludePatternList);

    // notify about changed lists
    jobIncludeExcludeChanged(jobNode);

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeListAdd
* Purpose: add entry to job exclude list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            pattern=<text>
*            [patternType=<type>]
*          Result:
*            id=<n>
\***********************************************************************/

LOCAL void serverCommand_excludeListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  PatternTypes  patternType;
  String        patternString;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  uint          patternId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, pattern type, pattern
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(patternString);
      return;
    }

    // add to exclude list
    PatternList_append(&jobNode->excludePatternList,patternString,patternType,&patternId);

    // notify about changed lists
    jobIncludeExcludeChanged(jobNode);

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"id=%u",patternId);

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_excludeListUpdate
* Purpose: update entry in job exclude list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*            pattern=<text>
*            [patternType=<type>]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_excludeListUpdate(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  uint          patternId;
  PatternTypes  patternType;
  String        patternString;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, pattern id, pattern type, pattern
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&patternId,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected id=<n>");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(patternString);
      return;
    }

    // update exclude list
    PatternList_update(&jobNode->excludePatternList,patternId,patternString,patternType);

    // notify about changed lists
    jobIncludeExcludeChanged(jobNode);

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_excludeListRemove
* Purpose: remove entry from job exclude list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_excludeListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  uint          patternId;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, id
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&patternId,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected id=<n>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // remove from exclude list
    if (!PatternList_remove(&jobNode->excludePatternList,patternId))
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"pattern with id #%u not found",patternId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // notify about changed lists
    jobIncludeExcludeChanged(jobNode);

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_mountList
* Purpose: get job mount list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
*            id=<n> name=<name> device=<name> alwaysUnmount=<yes|no>
*            ...
\***********************************************************************/

LOCAL void serverCommand_mountList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  MountNode     *mountNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // send mount list
    LIST_ITERATE(&jobNode->mountList,mountNode)
    {
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "id=%u name=%'S device=%'S alwaysUnmount=%y",
                       mountNode->id,
                       mountNode->name,
                       mountNode->device,
                       mountNode->alwaysUnmount
                      );
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_mountListAdd
* Purpose: add entry to job mountlist
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            name=<name>
*            device=<name>
*            alwaysUnmount=yes|no
*          Result:
*            id=<n>
\***********************************************************************/

LOCAL void serverCommand_mountListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  String        name;
  String        device;
  bool          alwaysUnmount;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  MountNode     *mountNode;
  uint          mountId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get jobUUID, name, device, alwaysUnmount
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    String_delete(name);
    return;
  }
  device = String_new();
  if (!StringMap_getString(argumentMap,"device",device,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected device=<name>");
    String_delete(device);
    String_delete(name);
    return;
  }
  if (!StringMap_getBool(argumentMap,"alwaysUnmount",&alwaysUnmount,FALSE))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected alwaysUnmount=yes|no");
    String_delete(device);
    String_delete(name);
    return;
  }

  mountId = 0;
  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(device);
      String_delete(name);
      return;
    }

    // add to mount list
    mountNode = newMountNode(name,device,alwaysUnmount);
    assert(mountNode != NULL);
    List_append(&jobNode->mountList,mountNode);

    // get id
    mountId = mountNode->id;

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"id=%u",mountId);

  // free resources
  String_delete(device);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_mountListUpdate
* Purpose: update entry to job mount list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            name=<name>
*            device=<name>
*            alwaysUnmount=yes|no
*          Result:
\***********************************************************************/

LOCAL void serverCommand_mountListUpdate(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  uint          mountId;
  String        name;
  String        device;
  bool          alwaysUnmount;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  MountNode     *mountNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get jobUUID, mount id, name, alwaysUnmount
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&mountId,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected id=<n>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    String_delete(name);
    return;
  }
  device = String_new();
  if (!StringMap_getString(argumentMap,"device",device,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected device=<name>");
    String_delete(device);
    String_delete(name);
    return;
  }
  if (!StringMap_getBool(argumentMap,"alwaysUnmount",&alwaysUnmount,FALSE))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected alwaysUnmount=yes|no");
    String_delete(device);
    String_delete(name);
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(device);
      String_delete(name);
      return;
    }

    // get mount
    mountNode = LIST_FIND(&jobNode->mountList,mountNode,mountNode->id == mountId);
    if (mountNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"mount %S not found",name);
      Semaphore_unlock(&jobList.lock);
      String_delete(name);
      return;
    }

    // update mount list
    String_set(mountNode->name,name);
    String_set(mountNode->device,device);
    mountNode->alwaysUnmount = alwaysUnmount;

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(device);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_mountListRemove
* Purpose: remove entry from job mount list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_mountListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  uint          mountId;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  MountNode     *mountNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, mount id
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&mountId,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected id=<n>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // get mount
    mountNode = LIST_FIND(&jobNode->mountList,mountNode,mountNode->id == mountId);
    if (mountNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"mount with id #%u not found",mountId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // remove from mount list
    List_remove(&jobNode->mountList,mountNode);

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_sourceList
* Purpose: get soource list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
*            id=<n> pattern=<text> patternType=<type>
*            ...
\***********************************************************************/

LOCAL void serverCommand_sourceList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString    (jobUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock   semaphoreLock;
  JobNode         *jobNode;
  DeltaSourceNode *deltaSourceNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // send delta source list
    LIST_ITERATE(&jobNode->deltaSourceList,deltaSourceNode)
    {
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "id=%u pattern=%'S patternType=%s",
                       deltaSourceNode->id,
//TODO
                       deltaSourceNode->storageName,
                       Pattern_patternTypeToString(PATTERN_TYPE_GLOB,"unknown")
                      );
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_sourceListClear
* Purpose: clear source list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_sourceListClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // clear source list
    DeltaSourceList_clear(&jobNode->deltaSourceList);

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_sourceListAdd
* Purpose: add entry to source list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            pattern=<text>
*            [patternType=<type>]
*          Result:
*            id=<n>
\***********************************************************************/

LOCAL void serverCommand_sourceListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  PatternTypes  patternType;
  String        patternString;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  uint          deltaSourceId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, type, pattern type, pattern
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(patternString);
      return;
    }

    // add to source list
    DeltaSourceList_append(&jobNode->deltaSourceList,patternString,patternType,&deltaSourceId);

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"id=%u",deltaSourceId);

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_sourceListUpdate
* Purpose: update entry in source list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*            pattern=<text>
*            [patternType=<type>]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_sourceListUpdate(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  uint          deltaSourceId;
  String        patternString;
  PatternTypes  patternType;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, id, type, pattern type, pattern
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&deltaSourceId,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected id=<n>");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(patternString);
      return;
    }

    // update source list
    DeltaSourceList_update(&jobNode->deltaSourceList,deltaSourceId,patternString,patternType);

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_sourceListRemove
* Purpose: remove entry from source list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_sourceListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  uint          deltaSourceId;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, id
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&deltaSourceId,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected id=<n>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // remove from source list
    if (!DeltaSourceList_remove(&jobNode->deltaSourceList,deltaSourceId))
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"delta source with id #%u not found",deltaSourceId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_excludeCompressList
* Purpose: get job exclude compress list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
*            pattern=<text> patternType=<type>
*            ...
\***********************************************************************/

LOCAL void serverCommand_excludeCompressList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  PatternNode   *patternNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // send exclude list
    LIST_ITERATE(&jobNode->compressExcludePatternList,patternNode)
    {
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "pattern=%'S patternType=%s",
                       patternNode->string,
                       Pattern_patternTypeToString(patternNode->pattern.type,"unknown")
                      );
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeCompressListClear
* Purpose: clear job exclude compress list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_excludeCompressListClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // clear exclude list
    PatternList_clear(&jobNode->compressExcludePatternList);

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_excludeCompressListAdd
* Purpose: add entry to job exclude compress list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            pattern=<text>
*            [patternType=<type>]
*          Result:
*            id=<n>
\***********************************************************************/

LOCAL void serverCommand_excludeCompressListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  String        patternString;
  PatternTypes  patternType;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  uint          patternId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, type, pattern type, pattern
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(patternString);
      return;
    }

    // add to exclude list
    PatternList_append(&jobNode->compressExcludePatternList,patternString,patternType,&patternId);

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"id=%u",patternId);

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_excludeCompressListUpdate
* Purpose: update entry in job exclude compress list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*            pattern=<text>
*            [patternType=<type>]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_excludeCompressListUpdate(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  uint          patternId;
  String        patternString;
  PatternTypes  patternType;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, id, type, pattern type, pattern
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&patternId,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected id=<n>");
    return;
  }
  patternString = String_new();
  if (!StringMap_getString(argumentMap,"pattern",patternString,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern=<text>");
    String_delete(patternString);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(patternString);
      return;
    }

    // update exclude list
    PatternList_update(&jobNode->compressExcludePatternList,patternId,patternString,patternType);

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(patternString);
}

/***********************************************************************\
* Name   : serverCommand_excludeCompressListRemove
* Purpose: remove entry from job exclude compress list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            id=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_excludeCompressListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  uint          patternId;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, id
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"id",&patternId,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected id=<n>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // add to exclude list
    // remove from exclude list
    if (!PatternList_remove(&jobNode->compressExcludePatternList,patternId))
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"pattern with id #%u not found",patternId);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // set modified
    jobNode->modifiedFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_scheduleList
* Purpose: get job schedule list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
*            scheduleUUID=<uuid> \
*            archiveType=normal|full|incremental|differential
*            date=<year>|*-<month>|*-<day>|* \
*            weekDay=<week day>|* \
*            time=<hour>|*:<minute>|* \
*            interval=<n>
*            customText=<text> \
*            minKeep=<n>|0 \
*            maxKeep=<n>|0 \
*            maxAage=<n>|0 \
*            noStorage=yes|no \
*            enabled=yes|no \
*            totalEntities=<n>|0 \
*            totalEntryCount=<n>|0 \
*            totalEntrySize=<n>|0 \
*            ...
\***********************************************************************/

LOCAL void serverCommand_scheduleList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock    semaphoreLock;
  const JobNode    *jobNode;
  ScheduleNode     *scheduleNode;
  String           date,weekDays,time;
  uint64           lastExecutedDateTime;
  ulong            totalEntities;
  ulong            totalEntryCount;
  uint64           totalEntrySize;
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  uint64           createdDateTime;
  ulong            entries;
  uint64           size;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // send schedule list
    date     = String_new();
    weekDays = String_new();
    time     = String_new();
    LIST_ITERATE(&jobNode->scheduleList,scheduleNode)
    {
      // get date string
      String_clear(date);
      if (scheduleNode->date.year != DATE_ANY)
      {
        String_format(date,"%d",scheduleNode->date.year);
      }
      else
      {
        String_appendCString(date,"*");
      }
      String_appendChar(date,'-');
      if (scheduleNode->date.month != DATE_ANY)
      {
        String_format(date,"%02d",scheduleNode->date.month);
      }
      else
      {
        String_appendCString(date,"*");
      }
      String_appendChar(date,'-');
      if (scheduleNode->date.day != DATE_ANY)
      {
        String_format(date,"%02d",scheduleNode->date.day);
      }
      else
      {
        String_appendCString(date,"*");
      }

      // get weekdays string
      String_clear(weekDays);
      if (scheduleNode->weekDaySet != WEEKDAY_SET_ANY)
      {
        if (IN_SET(scheduleNode->weekDaySet,WEEKDAY_MON)) { String_joinCString(weekDays,"Mon",','); }
        if (IN_SET(scheduleNode->weekDaySet,WEEKDAY_TUE)) { String_joinCString(weekDays,"Tue",','); }
        if (IN_SET(scheduleNode->weekDaySet,WEEKDAY_WED)) { String_joinCString(weekDays,"Wed",','); }
        if (IN_SET(scheduleNode->weekDaySet,WEEKDAY_THU)) { String_joinCString(weekDays,"Thu",','); }
        if (IN_SET(scheduleNode->weekDaySet,WEEKDAY_FRI)) { String_joinCString(weekDays,"Fri",','); }
        if (IN_SET(scheduleNode->weekDaySet,WEEKDAY_SAT)) { String_joinCString(weekDays,"Sat",','); }
        if (IN_SET(scheduleNode->weekDaySet,WEEKDAY_SUN)) { String_joinCString(weekDays,"Sun",','); }
      }
      else
      {
        String_appendCString(weekDays,"*");
      }

      // get time string
      String_clear(time);
      if (scheduleNode->time.hour != TIME_ANY)
      {
        String_format(time,"%02d",scheduleNode->time.hour);
      }
      else
      {
        String_appendCString(time,"*");
      }
      String_appendChar(time,':');
      if (scheduleNode->time.minute != TIME_ANY)
      {
        String_format(time,"%02d",scheduleNode->time.minute);
      }
      else
      {
        String_appendCString(time,"*");
      }

      // get last executed date/time, total entities, entries, size
      lastExecutedDateTime = 0LL;
      totalEntities        = 0L;
      totalEntryCount      = 0L;
      totalEntrySize       = 0LL;
      if ((indexHandle != NULL))
      {
        error = Index_initListEntities(&indexQueryHandle,
                                       indexHandle,
                                       INDEX_ID_ANY,  // uuidId
                                       NULL,  // jobUUID,
                                       scheduleNode->uuid,
                                       NULL,  // name
                                       DATABASE_ORDERING_ASCENDING,
                                       0LL,  // offset
                                       INDEX_UNLIMITED
                                      );
        if (error == ERROR_NONE)
        {
          while (Index_getNextEntity(&indexQueryHandle,
                                     NULL,  // uuidId,
                                     NULL,  // jobUUID,
                                     NULL,  // scheduleUUID,
                                     NULL,  // entityId,
                                     NULL,  // archiveType,
                                     &createdDateTime,  // createdDateTime,
                                     NULL,  // lastErrorMessage
                                     &entries,
                                     &size
                                    )
                )
          {
            if (createdDateTime > lastExecutedDateTime) lastExecutedDateTime = createdDateTime;
            totalEntities   += 1;
            totalEntryCount += entries;
            totalEntrySize  += size;
          }
          Index_doneList(&indexQueryHandle);
        }
      }

      sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                       "scheduleUUID=%S archiveType=%s date=%S weekDays=%S time=%S interval=%u customText=%'S minKeep=%u maxKeep=%u maxAge=%u noStorage=%y enabled=%y lastExecutedDateTime=%llu totalEntities=%lu totalEntryCount=%lu totalEntrySize=%llu",
                       scheduleNode->uuid,
                       (scheduleNode->archiveType != ARCHIVE_TYPE_UNKNOWN) ? ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,scheduleNode->archiveType,NULL) : "*",
                       date,
                       weekDays,
                       time,
                       scheduleNode->interval,
                       scheduleNode->customText,
                       scheduleNode->minKeep,
                       scheduleNode->maxKeep,
                       scheduleNode->maxAge,
                       scheduleNode->noStorage,
                       scheduleNode->enabled,
                       lastExecutedDateTime,
                       totalEntities,
                       totalEntryCount,
                       totalEntrySize
                      );
    }
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_scheduleAdd
* Purpose: add entry to job schedule list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            archiveType=normal|full|incremental|differential|continuous
*            date=<year>|*-<month>|*-<day>|*
*            weekDays=<week day>,...|*
*            time=<hour>|*:<minute>|*
*            interval=<n>
*            customText=<text>
*            minKeep=<n>|0
*            maxKeep=<n>|0
*            maxAage=<n>|0
*            noStorage=yes|no
*            enabled=yes|no
*          Result:
*            scheduleUUID=<uuid>
\***********************************************************************/

LOCAL void serverCommand_scheduleAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  String        title;
  String        date;
  String        weekDays;
  String        time;
  ArchiveTypes  archiveType;
  uint          interval;
  String        customText;
  uint          minKeep,maxKeep;
  uint          maxAge;
  bool          noStorage;
  bool          enabled;
  SemaphoreLock semaphoreLock;
  ScheduleNode  *scheduleNode;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, date, weekday, time, archive type, custome text, min./max keep, max. age, enabled
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  title = String_new();
  StringMap_getString(argumentMap,"title",title,NULL);
  date = String_new();
  if (!StringMap_getString(argumentMap,"date",date,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected date=<date>|*");
    String_delete(date);
    String_delete(title);
    return;
  }
  weekDays = String_new();
  if (!StringMap_getString(argumentMap,"weekDays",weekDays,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected weekDays=<name>|*");
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  time = String_new();
  if (!StringMap_getString(argumentMap,"time",time,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected time=<time>|*");
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  if   (stringEquals(StringMap_getTextCString(argumentMap,"archiveType","*"),"*"))
  {
    archiveType = ARCHIVE_TYPE_NORMAL;
  }
  else
  {
    if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)parseArchiveType,ARCHIVE_TYPE_UNKNOWN))
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS");
      String_delete(time);
      String_delete(weekDays);
      String_delete(date);
      String_delete(title);
      return;
    }
  }
  StringMap_getUInt(argumentMap,"interval",&interval,0);
  customText = String_new();
  StringMap_getString(argumentMap,"customText",customText,NULL);
  if (!StringMap_getUInt(argumentMap,"minKeep",&minKeep,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected minKeep=<n>");
    String_delete(customText);
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"maxKeep",&maxKeep,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected maxKeep=<n>");
    String_delete(customText);
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  if (!StringMap_getUInt(argumentMap,"maxAge",&maxAge,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected maxAge=<n>");
    String_delete(customText);
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  if (!StringMap_getBool(argumentMap,"noStorage",&noStorage,FALSE))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected noStorage=yes|no");
    String_delete(customText);
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  if (!StringMap_getBool(argumentMap,"enabled",&enabled,FALSE))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected enabled=yes|no");
    String_delete(customText);
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }

  // parse schedule
  scheduleNode = parseScheduleDateTime(date,
                                       weekDays,
                                       time
                                      );
  if (scheduleNode == NULL)
  {
    sendClientResult(clientInfo,
                     id,
                     TRUE,
                     ERROR_PARSING,
                     "cannot parse schedule '%S %S %S'",
                     date,
                     weekDays,
                     time
                    );
    String_delete(customText);
    String_delete(time);
    String_delete(weekDays);
    String_delete(date);
    String_delete(title);
    return;
  }
  scheduleNode->archiveType = archiveType;
  scheduleNode->interval    = interval;
  String_set(scheduleNode->customText,customText);
  scheduleNode->minKeep     = minKeep;
  scheduleNode->maxKeep     = maxKeep;
  scheduleNode->maxAge      = maxAge;
  scheduleNode->noStorage   = noStorage;
  scheduleNode->enabled     = enabled;

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      deleteScheduleNode(scheduleNode);
      String_delete(customText);
      String_delete(time);
      String_delete(weekDays);
      String_delete(date);
      String_delete(title);
      return;
    }

    // add to schedule list
    List_append(&jobNode->scheduleList,scheduleNode);
    jobNode->modifiedFlag         = TRUE;
    jobNode->scheduleModifiedFlag = TRUE;

    // notify about changed schedule
    jobScheduleChanged(jobNode);
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"scheduleUUID=%S",scheduleNode->uuid);

  // free resources
  String_delete(customText);
  String_delete(time);
  String_delete(weekDays);
  String_delete(date);
  String_delete(title);
}

/***********************************************************************\
* Name   : serverCommand_scheduleRemove
* Purpose: remove entry from job schedule list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            scheduleUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_scheduleRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString  (scheduleUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  ScheduleNode  *scheduleNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, schedule UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected scheduleUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // find schedule
    scheduleNode = findScheduleByUUID(jobNode,scheduleUUID);
    if (scheduleNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"schedule %S of job %S not found",scheduleUUID,jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // remove from list
    List_removeAndFree(&jobNode->scheduleList,scheduleNode,CALLBACK((ListNodeFreeFunction)freeScheduleNode,NULL));
    jobNode->modifiedFlag         = TRUE;
    jobNode->scheduleModifiedFlag = TRUE;

    // notify about changed schedule
    jobScheduleChanged(jobNode);
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_scheduleOptionGet
* Purpose: get schedule options
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            scheduleUUID=<uuid>
*            name=<name>
*          Result:
*            value=<value>
\***********************************************************************/

LOCAL void serverCommand_scheduleOptionGet(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString       (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString       (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String             name;
  SemaphoreLock      semaphoreLock;
  const JobNode      *jobNode;
  const ScheduleNode *scheduleNode;
  uint               i;
  String             s;
  ConfigValueFormat  configValueFormat;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, schedule UUID, name
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected scheduleUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // find schedule
    scheduleNode = findScheduleByUUID(jobNode,scheduleUUID);
    if (scheduleNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"schedule %S of job %S not found",scheduleUUID,jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(name);
      return;
    }

    // find config value
#ifndef WERROR
#warning todo
#endif
    i = ConfigValue_valueIndex(JOB_CONFIG_VALUES,"schedule",String_cString(name));
    i = 0;
    while (   (JOB_CONFIG_VALUES[i].name != NULL)
           && !String_equalsCString(name,JOB_CONFIG_VALUES[i].name)
          )
    {
      i++;
    }
    if (JOB_CONFIG_VALUES[i].name == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown schedule config value '%S'",name);
      Semaphore_unlock(&jobList.lock);
      String_delete(name);
      return;
    }

    // send value
    s = String_new();
    ConfigValue_formatInit(&configValueFormat,
                           &JOB_CONFIG_VALUES[i],
                           CONFIG_VALUE_FORMAT_MODE_VALUE,
                           jobNode
                          );
    ConfigValue_format(&configValueFormat,s);
    ConfigValue_formatDone(&configValueFormat);
    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"value=%S",s);
    String_delete(s);
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_scheduleOptionSet
* Purpose: set schedule option
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            scheduleUUID=<uuid>
*            name=<name>
*            value=<value>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_scheduleOptionSet(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString  (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String        name,value;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  ScheduleNode  *scheduleNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, schedule UUID, name, value
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected scheduleUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    return;
  }
  value = String_new();
  if (!StringMap_getString(argumentMap,"value",value,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected value=<value>");
    String_delete(name);
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // find schedule
    scheduleNode = findScheduleByUUID(jobNode,scheduleUUID);
    if (scheduleNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"schedule %S of job %S not found",scheduleUUID,jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(value);
      String_delete(name);
      return;
    }

    // parse
    if (ConfigValue_parse(String_cString(name),
                          String_cString(value),
                          JOB_CONFIG_VALUES,
                          "schedule",
                          NULL, // outputHandle
                          NULL, // errorPrefix
                          NULL, // warningPrefix
                          scheduleNode
                         )
       )
    {
      jobNode->modifiedFlag         = TRUE;
      jobNode->scheduleModifiedFlag = TRUE;
      sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
    }
    else
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown schedule config value '%S'",name);
    }

    // notify about changed schedule
    jobScheduleChanged(jobNode);
  }

  // free resources
  String_delete(value);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_scheduleOptionDelete
* Purpose: delete schedule option
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            scheduleUUID=<uuid>
*            name=<name>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_scheduleOptionDelete(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString  (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String        name;
  SemaphoreLock semaphoreLock;
  const JobNode *jobNode;
  ScheduleNode  *scheduleNode;
  uint          i;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, schedule UUID, name
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected scheduleUUID=<uuid>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<name>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // find schedule
    scheduleNode = findScheduleByUUID(jobNode,scheduleUUID);
    if (scheduleNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"schedule %S of job %S not found",scheduleUUID,jobUUID);
      Semaphore_unlock(&jobList.lock);
      String_delete(name);
      return;
    }

    // find config value
#ifndef WERROR
#warning todo
#endif
    i = ConfigValue_valueIndex(JOB_CONFIG_VALUES,"schedule",String_cString(name));
    i = 0;
    while (   (JOB_CONFIG_VALUES[i].name != NULL)
           && !String_equalsCString(name,JOB_CONFIG_VALUES[i].name)
          )
    {
      i++;
    }
    if (JOB_CONFIG_VALUES[i].name == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_UNKNOWN_VALUE,"unknown schedule config value '%S'",name);
      Semaphore_unlock(&jobList.lock);
      String_delete(name);
      return;
    }

    // delete value
#ifndef WERROR
#warning todo?
#endif
//    ConfigValue_reset(&JOB_CONFIG_VALUES[z],jobNode);

    // notify about changed schedule
    jobScheduleChanged(jobNode);
  }

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_scheduleTrigger
* Purpose: trigger job schedule
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            scheduleUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_scheduleTrigger(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString  (scheduleUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;
  ScheduleNode  *scheduleNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, schedule UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getString(argumentMap,"scheduleUUID",scheduleUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected scheduleUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // find schedule
    scheduleNode = findScheduleByUUID(jobNode,scheduleUUID);
    if (scheduleNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"schedule %S of job %S not found",scheduleUUID,jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // check if active
    if (isJobActive(jobNode))
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job already scheduled");
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // trigger job
    triggerJob(jobNode,
               scheduleNode->archiveType,
               FALSE,  // dryRun
               scheduleNode->uuid,
               scheduleNode->customText,
               scheduleNode->noStorage
              );
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_decryptPasswordsClear
* Purpose: clear decrypt passwords in internal list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_decryptPasswordsClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // clear decrypt password list
  Archive_clearDecryptPasswords();

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_decryptPasswordAdd
* Purpose: add password to internal list of decrypt passwords
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            encryptType=<type>
*            encryptedPassword=<encrypted password>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_decryptPasswordAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String   encryptType;
  String   encryptedPassword;
  Password password;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // get encrypt type, encrypted password
  encryptType = String_new();
  if (!StringMap_getString(argumentMap,"encryptType",encryptType,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected encryptType=<type>");
    return;
  }
  encryptedPassword = String_new();
  if (!StringMap_getString(argumentMap,"encryptedPassword",encryptedPassword,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected encryptedPassword=<encrypted password>");
    String_delete(encryptedPassword);
    String_delete(encryptType);
    return;
  }

  // decrypt password and add to list
  Password_init(&password);
  if (!decryptPassword(&password,clientInfo,encryptType,encryptedPassword))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_INVALID_CRYPT_PASSWORD,"");
    Password_done(&password);
    String_delete(encryptedPassword);
    String_delete(encryptType);
    return;
  }

  // add to list
  Archive_appendDecryptPassword(&password);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  Password_done(&password);
  String_delete(encryptedPassword);
  String_delete(encryptType);
}

/***********************************************************************\
* Name   : serverCommand_ftpPassword
* Purpose: set job FTP password
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            encryptType=<type>
*            encryptedPassword=<encrypted password>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_ftpPassword(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String encryptType;
  String encryptedPassword;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get encrypt type, encrypted password
  encryptType = String_new();
  if (!StringMap_getString(argumentMap,"encryptType",encryptType,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected encryptType=<type>");
    return;
  }
  encryptedPassword = String_new();
  if (!StringMap_getString(argumentMap,"encryptedPassword",encryptedPassword,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected encryptedPassword=<encrypted password>");
    String_delete(encryptedPassword);
    String_delete(encryptType);
    return;
  }

  // decrypt password
  if (clientInfo->jobOptions.ftpServer.password == NULL) clientInfo->jobOptions.ftpServer.password = Password_new();
  if (!decryptPassword(clientInfo->jobOptions.ftpServer.password,clientInfo,encryptType,encryptedPassword))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_INVALID_FTP_PASSWORD,"");
    String_delete(encryptedPassword);
    String_delete(encryptType);
    return;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(encryptedPassword);
  String_delete(encryptType);
}

/***********************************************************************\
* Name   : serverCommand_sshPassword
* Purpose: set job SSH password
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            encryptType=<type>
*            encryptedPassword=<encrypted password>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_sshPassword(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String encryptType;
  String encryptedPassword;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get encrypt type, encrypted password
  encryptType = String_new();
  if (!StringMap_getString(argumentMap,"encryptType",encryptType,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected encryptType=<type>");
    return;
  }
  encryptedPassword = String_new();
  if (!StringMap_getString(argumentMap,"encryptedPassword",encryptedPassword,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected encryptedPassword=<encrypted password>");
    String_delete(encryptedPassword);
    String_delete(encryptType);
    return;
  }

  // decrypt password
  if (clientInfo->jobOptions.sshServer.password == NULL) clientInfo->jobOptions.sshServer.password = Password_new();
  if (!decryptPassword(clientInfo->jobOptions.sshServer.password,clientInfo,encryptType,encryptedPassword))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_INVALID_SSH_PASSWORD,"");
    String_delete(encryptedPassword);
    String_delete(encryptType);
    return;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(encryptedPassword);
  String_delete(encryptType);
}

/***********************************************************************\
* Name   : serverCommand_webdavPassword
* Purpose: set job Webdav password
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            encryptType=<type>
*            encryptedPassword=<encrypted password>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_webdavPassword(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String encryptType;
  String encryptedPassword;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get encrypt type, encrypted password
  encryptType = String_new();
  if (!StringMap_getString(argumentMap,"encryptType",encryptType,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected encryptType=<type>");
    return;
  }
  encryptedPassword = String_new();
  if (!StringMap_getString(argumentMap,"encryptedPassword",encryptedPassword,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected encryptedPassword=<encrypted password>");
    String_delete(encryptedPassword);
    String_delete(encryptType);
    return;
  }

  // decrypt password
  if (clientInfo->jobOptions.webDAVServer.password == NULL) clientInfo->jobOptions.webDAVServer.password = Password_new();
  if (!decryptPassword(clientInfo->jobOptions.webDAVServer.password,clientInfo,encryptType,encryptedPassword))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_INVALID_WEBDAV_PASSWORD,"");
    String_delete(encryptedPassword);
    String_delete(encryptType);
    return;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(encryptedPassword);
  String_delete(encryptType);
}

/***********************************************************************\
* Name   : serverCommand_cryptPassword
* Purpose: set job encryption password
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>|*
*            encryptType=<type>
*            encryptedPassword=<encrypted password>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_cryptPassword(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  String        encryptType;
  String        encryptedPassword;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, get encrypt type, encrypted password
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  encryptType = String_new();
  if (!StringMap_getString(argumentMap,"encryptType",encryptType,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected encryptType=<type>");
    return;
  }
  encryptedPassword = String_new();
  if (!StringMap_getString(argumentMap,"encryptedPassword",encryptedPassword,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected encryptedPassword=<encrypted password>");
    String_delete(encryptedPassword);
    String_delete(encryptType);
    return;
  }

  if (!String_equalsCString(jobUUID,"*"))
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
    {
      // find job
      jobNode = findJobByUUID(jobUUID);
      if (jobNode == NULL)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
        Semaphore_unlock(&jobList.lock);
        String_delete(encryptedPassword);
        String_delete(encryptType);
        return;
      }

      // decrypt password
      if (jobNode->cryptPassword == NULL) jobNode->cryptPassword = Password_new();
      if (!decryptPassword(jobNode->cryptPassword,clientInfo,encryptType,encryptedPassword))
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_INVALID_CRYPT_PASSWORD,"");
        Semaphore_unlock(&jobList.lock);
        String_delete(encryptedPassword);
        String_delete(encryptType);
        return;
      }
    }
  }
  else
  {
    // decrypt password
    if (!decryptPassword(clientInfo->jobOptions.cryptPassword,clientInfo,encryptType,encryptedPassword))
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_INVALID_CRYPT_PASSWORD,"");
      String_delete(encryptedPassword);
      String_delete(encryptType);
      return;
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(encryptedPassword);
  String_delete(encryptType);
}

/***********************************************************************\
* Name   : serverCommand_passwordsClear
* Purpose: clear ssh/ftp/crypt passwords stored in memory
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_passwordsClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  if (clientInfo->jobOptions.ftpServer.password != NULL) Password_clear(clientInfo->jobOptions.ftpServer.password);
  if (clientInfo->jobOptions.sshServer.password != NULL) Password_clear(clientInfo->jobOptions.sshServer.password);
  if (clientInfo->jobOptions.cryptPassword != NULL) Password_clear(clientInfo->jobOptions.cryptPassword);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_volumeLoad
* Purpose: set number of loaded volume
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            volumeNnumber=<n>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_volumeLoad(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  uint          volumeNumber;
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID, volume number
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getUInt(argumentMap,"volumeNumber",&volumeNumber,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected volumeNumber=<n>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // set volume number
    jobNode->volumeNumber = volumeNumber;
    Semaphore_signalModified(&statusLock);
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_volumeUnload
* Purpose: unload volumne
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_volumeUnload(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  SemaphoreLock semaphoreLock;
  JobNode       *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get job UUID
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,0))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }

  SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    // find job
    jobNode = findJobByUUID(jobUUID);
    if (jobNode == NULL)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_JOB_NOT_FOUND,"job %S not found",jobUUID);
      Semaphore_unlock(&jobList.lock);
      return;
    }

    // set unload flag
    jobNode->volumeUnloadFlag = TRUE;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_archiveList
* Purpose: list content of archive
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<storage name>
*          Result:
*            fileType=FILE name=<name> size=<n [bytes]> dateTime=<time stamp> archiveSize=<n [bytes]> deltaCompressAlgorithm=<n> \
*            byteCompressAlgorithm=<n> cryptAlgorithm=<n> cryptType=<n> deltaSourceName=<name> \
*            deltaSourceSize=<n [bytes]> fragmentOffset=<n> fragmentSize=<n [bytes]>
*
*            fileType=IMAGE name=<name> size=<n [bytes]> archiveSize=<n [bytes]> deltaCompressAlgorithm=<n> \
*            byteCompressAlgorithm=<n> cryptAlgorithm=<n> cryptType=<n> deltaSourceName=<name> \
*            deltaSourceSize=<n [bytes]> blockSize=<n [bytes]> blockOffset=<n> blockCount=<n>
*
*            fileType=DIRECTORY name=<name> dateTime=<time stamp> cryptAlgorithm=<n> cryptType=<n>
*
*            fileType=LINK linkName=<link name> name=<name> cryptAlgorithm=<n> cryptType=<n>
*
*            fileType=HARDLINK name=<name> size=<n [bytes]> dateTime=<time stamp> archiveSize=<n [bytes]> deltaCompressAlgorithm=<n> \
*            byteCompressAlgorithm=<n> cryptAlgorithm=<n> cryptType=<n> deltaSourceName=<name> \
*            deltaSourceSize=<n [bytes]> fragmentOffset=<n> fragmentSize=<n [bytes]>
*
*            fileType=SPECIAL name=<name> cryptAlgorithm=<n> cryptType=<n> fileSpecialType=<n> major=<n> minor=<n>
*            ...
\***********************************************************************/

LOCAL void serverCommand_archiveList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String            storageName;
  StorageSpecifier  storageSpecifier;
  StorageHandle     storageHandle;
  Errors            error;
  ArchiveInfo       archiveInfo;
  ArchiveEntryInfo  archiveEntryInfo;
  ArchiveEntryTypes archiveEntryType;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);

  // get storage name, pattern
  storageName = String_new();
  if (!StringMap_getString(argumentMap,"name",storageName,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<storage name>");
    return;
  }

  // parse storage name
  Storage_initSpecifier(&storageSpecifier);
  error = Storage_parseName(&storageSpecifier,storageName);
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,error,"%s",Error_getText(error));
    Storage_doneSpecifier(&storageSpecifier);
    String_delete(storageName);
    return;
  }

  // init storage
  error = Storage_init(&storageHandle,
                       &storageSpecifier,
                       &clientInfo->jobOptions,
                       &globalOptions.maxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_HIGH,
                       CALLBACK(NULL,NULL),  // updateStatusInfo
                       CALLBACK(NULL,NULL),  // getPassword
                       CALLBACK(NULL,NULL)  // requestVolume
                      );
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,error,"%s",Error_getText(error));
    Storage_doneSpecifier(&storageSpecifier);
    String_delete(storageName);
    return;
  }

  // open archive
  error = Archive_open(&archiveInfo,
                       &storageHandle,
                       &storageSpecifier,
                       NULL,  // archive name
                       NULL,  // deltaSourceList
                       &clientInfo->jobOptions,
                       CALLBACK(NULL,NULL),
                       NULL  // logHandle
                      );
  if (error != ERROR_NONE)
  {
    Storage_doneSpecifier(&storageSpecifier);
    sendClientResult(clientInfo,id,TRUE,error,"%s",Error_getText(error));
    Storage_doneSpecifier(&storageSpecifier);
    String_delete(storageName);
    return;
  }

  // list contents
  error = ERROR_NONE;
  while (   !Archive_eof(&archiveInfo,TRUE)
         && (error == ERROR_NONE)
         && !isCommandAborted(clientInfo,id)
        )
  {
    // get next file type
    error = Archive_getNextArchiveEntryType(&archiveInfo,
                                            &archiveEntryType,
                                            TRUE
                                           );
    if (error == ERROR_NONE)
    {
      // read entry
      switch (archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          {
            CompressAlgorithms deltaCompressAlgorithm;
            CompressAlgorithms byteCompressAlgorithm;
            CryptAlgorithms    cryptAlgorithm;
            CryptTypes         cryptType;
            String             fileName;
            FileInfo           fileInfo;
            String             deltaSourceName;
            uint64             deltaSourceSize;
            uint64             fragmentOffset,fragmentSize;

            // open archive file
            fileName        = String_new();
            deltaSourceName = String_new();
            error = Archive_readFileEntry(&archiveEntryInfo,
                                          &archiveInfo,
                                          &deltaCompressAlgorithm,
                                          &byteCompressAlgorithm,
                                          &cryptAlgorithm,
                                          &cryptType,
                                          fileName,
                                          &fileInfo,
                                          NULL, // fileExtendedAttributeList
                                          deltaSourceName,
                                          &deltaSourceSize,
                                          &fragmentOffset,
                                          &fragmentSize
                                         );
            if (error != ERROR_NONE)
            {
              sendClientResult(clientInfo,id,TRUE,error,"Cannot read content of storage '%S': %s",storageName,Error_getText(error));
              String_delete(deltaSourceName);
              String_delete(fileName);
              break;
            }

            // match pattern
            if (   (List_isEmpty(&clientInfo->includeEntryList) || EntryList_match(&clientInfo->includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(&clientInfo->excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                               "fileType=FILE name=%'S size=%llu dateTime=%llu archiveSize=%llu deltaCompressAlgorithm=%d byteCompressAlgorithm=%d cryptAlgorithm=%d cryptType=%d deltaSourceName=%'S deltaSourceSize=%llu fragmentOffset=%llu fragmentSize=%llu",
                               fileName,
                               fileInfo.size,
                               fileInfo.timeModified,
                               archiveEntryInfo.file.chunkFileData.info.size,
                               deltaCompressAlgorithm,
                               byteCompressAlgorithm,
                               cryptAlgorithm,
                               cryptType,
                               deltaSourceName,
                               deltaSourceSize,
                               fragmentOffset,
                               fragmentSize
                              );
            }

            // close archive file, free resources
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(deltaSourceName);
            String_delete(fileName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            CompressAlgorithms deltaCompressAlgorithm;
            CompressAlgorithms byteCompressAlgorithm;
            CryptAlgorithms    cryptAlgorithm;
            CryptTypes         cryptType;
            String             imageName;
            DeviceInfo         deviceInfo;
            FileSystemTypes    fileSystemType;
            String             deltaSourceName;
            uint64             deltaSourceSize;
            uint64             blockOffset,blockCount;

            // open archive file
            imageName       = String_new();
            deltaSourceName = String_new();
            error = Archive_readImageEntry(&archiveEntryInfo,
                                           &archiveInfo,
                                           &deltaCompressAlgorithm,
                                           &byteCompressAlgorithm,
                                           &cryptAlgorithm,
                                           &cryptType,
                                           imageName,
                                           &deviceInfo,
                                           &fileSystemType,
                                           deltaSourceName,
                                           &deltaSourceSize,
                                           &blockOffset,
                                           &blockCount
                                          );
            if (error != ERROR_NONE)
            {
              sendClientResult(clientInfo,id,TRUE,error,"Cannot read content of storage '%S': %s",storageName,Error_getText(error));
              String_delete(deltaSourceName);
              String_delete(imageName);
              break;
            }

            // match pattern
            if (   (List_isEmpty(&clientInfo->includeEntryList) || EntryList_match(&clientInfo->includeEntryList,imageName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(&clientInfo->excludePatternList,imageName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                               "fileType=IMAGE name=%'S size=%llu archiveSize=%llu deltaCompressAlgorithm=%d byteCompressAlgorithm=%d cryptAlgorithm=%d cryptType=%d deltaSourceName=%'S deltaSourceSize=%llu fileSystemType=%s blockSize=%u blockOffset=%llu blockCount=%llu",
                               imageName,
                               deviceInfo.size,
                               archiveEntryInfo.image.chunkImageData.info.size,
                               deltaCompressAlgorithm,
                               byteCompressAlgorithm,
                               cryptAlgorithm,
                               cryptType,
                               deltaSourceName,
                               deltaSourceSize,
                               FileSystem_getName(fileSystemType),
                               deviceInfo.blockSize,
                               blockOffset,
                               blockCount
                              );
            }

            // close archive file, free resources
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(deltaSourceName);
            String_delete(imageName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          {
            CryptAlgorithms cryptAlgorithm;
            CryptTypes      cryptType;
            String          directoryName;
            FileInfo        fileInfo;

            // open archive directory
            directoryName = String_new();
            error = Archive_readDirectoryEntry(&archiveEntryInfo,
                                               &archiveInfo,
                                               &cryptAlgorithm,
                                               &cryptType,
                                               directoryName,
                                               &fileInfo,
                                               NULL   // fileExtendedAttributeList
                                              );
            if (error != ERROR_NONE)
            {
              sendClientResult(clientInfo,id,TRUE,error,"Cannot read content of storage '%S': %s",storageName,Error_getText(error));
              String_delete(directoryName);
              break;
            }

            // match pattern
            if (   (List_isEmpty(&clientInfo->includeEntryList) || EntryList_match(&clientInfo->includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(&clientInfo->excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                               "fileType=DIRECTORY name=%'S dateTime=%llu cryptAlgorithm=%d cryptType=%d",
                               directoryName,
                               fileInfo.timeModified,
                               cryptAlgorithm,
                               cryptType
                              );
            }

            // close archive file, free resources
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(directoryName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          {
            CryptAlgorithms cryptAlgorithm;
            CryptTypes      cryptType;
            String          linkName;
            String          name;

            // open archive link
            linkName = String_new();
            name     = String_new();
            error = Archive_readLinkEntry(&archiveEntryInfo,
                                          &archiveInfo,
                                          &cryptAlgorithm,
                                          &cryptType,
                                          linkName,
                                          name,
                                          NULL,  // fileInfo
                                          NULL   // fileExtendedAttributeList
                                         );
            if (error != ERROR_NONE)
            {
              sendClientResult(clientInfo,id,TRUE,error,"Cannot read content of storage '%S': %s",storageName,Error_getText(error));
              String_delete(name);
              String_delete(linkName);
              break;
            }

            // match pattern
            if (   (List_isEmpty(&clientInfo->includeEntryList) || EntryList_match(&clientInfo->includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(&clientInfo->excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                               "fileType=LINK linkName=%'S name=%'S cryptAlgorithm=%d cryptType=%d",
                               linkName,
                               name,
                               cryptAlgorithm,
                               cryptType
                              );
            }

            // close archive file, free resources
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(name);
            String_delete(linkName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          {
            CompressAlgorithms deltaCompressAlgorithm;
            CompressAlgorithms byteCompressAlgorithm;
            CryptAlgorithms    cryptAlgorithm;
            CryptTypes         cryptType;
            StringList         fileNameList;
            FileInfo           fileInfo;
            String             deltaSourceName;
            uint64             deltaSourceSize;
            uint64             fragmentOffset,fragmentSize;

            // open archive hardlink
            StringList_init(&fileNameList);
            deltaSourceName = String_new();
            error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                              &archiveInfo,
                                              &deltaCompressAlgorithm,
                                              &byteCompressAlgorithm,
                                              &cryptAlgorithm,
                                              &cryptType,
                                              &fileNameList,
                                              &fileInfo,
                                              NULL,  // fileExtendedAttributeList
                                              deltaSourceName,
                                              &deltaSourceSize,
                                              &fragmentOffset,
                                              &fragmentSize
                                             );
            if (error != ERROR_NONE)
            {
              sendClientResult(clientInfo,id,TRUE,error,"Cannot read content of storage '%S': %s",storageName,Error_getText(error));
              String_delete(deltaSourceName);
              StringList_done(&fileNameList);
              break;
            }

            // match pattern
            if (   (List_isEmpty(&clientInfo->includeEntryList) || EntryList_matchStringList(&clientInfo->includeEntryList,&fileNameList,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_matchStringList(&clientInfo->excludePatternList,&fileNameList,PATTERN_MATCH_MODE_EXACT)
               )
            {
              sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                               "fileType=HARDLINK name=%'S size=%llu dateTime=%llu archiveSize=%llu deltaCompressAlgorithm=%d byteCompressAlgorithm=%d cryptAlgorithm=%d cryptType=%d deltaSourceName=%'S deltaSourceSize=%llu fragmentOffset=%llu fragmentSize=%llu",
                               StringList_first(&fileNameList,NULL),
                               fileInfo.size,
                               fileInfo.timeModified,
                               archiveEntryInfo.file.chunkFileData.info.size,
                               deltaCompressAlgorithm,
                               byteCompressAlgorithm,
                               cryptAlgorithm,
                               cryptType,
                               deltaSourceName,
                               deltaSourceSize,
                               fragmentOffset,
                               fragmentSize
                              );
            }

            // close archive file, free resources
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(deltaSourceName);
            StringList_done(&fileNameList);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          {
            String          name;
            CryptAlgorithms cryptAlgorithm;
            CryptTypes      cryptType;
            FileInfo        fileInfo;

            // open archive link
            name = String_new();
            error = Archive_readSpecialEntry(&archiveEntryInfo,
                                             &archiveInfo,
                                             &cryptAlgorithm,
                                             &cryptType,
                                             name,
                                             &fileInfo,
                                             NULL   // fileExtendedAttributeList
                                            );
            if (error != ERROR_NONE)
            {
              sendClientResult(clientInfo,id,TRUE,error,"Cannot read content of storage '%S': %s",storageName,Error_getText(error));
              String_delete(name);
              break;
            }

            // match pattern
            if (   (List_isEmpty(&clientInfo->includeEntryList) || EntryList_match(&clientInfo->includeEntryList,name,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(&clientInfo->excludePatternList,name,PATTERN_MATCH_MODE_EXACT)
               )
            {
              sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                               "fileType=SPECIAL name=%'S cryptAlgorithm=%d cryptType=%d fileSpecialType=%d major=%d minor=%d",
                               name,
                               cryptAlgorithm,
                               cryptType,
                               fileInfo.specialType,
                               fileInfo.major,
                               fileInfo.minor
                              );
            }

            // close archive file, free resources
            Archive_closeEntry(&archiveEntryInfo);
            String_delete(name);
          }
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break; /* not reached */
      }
    }
    else
    {
      sendClientResult(clientInfo,id,TRUE,error,"Cannot read next entry of storage '%S': %s",storageName,Error_getText(error));
      break;
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // close archive
  Archive_close(&archiveInfo);

  // done storage
  (void)Storage_done(&storageHandle);

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(storageName);
}

/***********************************************************************\
* Name   : serverCommand_storageList
* Purpose: list selected storages
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            storageId=<n> name=<text> totalEntryCount=<n> totalEntrySize=<n>
\***********************************************************************/

LOCAL void serverCommand_storageList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String           storageName;
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;
  ulong            totalEntryCount;
  uint64           totalEntrySize;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(argumentMap);

  // init variables
  storageName = String_new();

  // list storage
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 INDEX_ID_ANY,  // entityId,
                                 NULL,  // jobUUID
                                 Array_cArray(&clientInfo->indexIdArray),
                                 Array_length(&clientInfo->indexIdArray),
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 INDEX_STORAGE_SORT_MODE_NONE,
                                 DATABASE_ORDERING_NONE,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    String_delete(storageName);
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"init list storage fail: %s",Error_getText(error));
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && Index_getNextStorage(&indexQueryHandle,
                                 NULL,  // uuidId
                                 NULL,  // jobUUID
                                 NULL,  // entityId
                                 NULL,  // scheduleUUID
                                 NULL,  // archiveType
                                 &storageId,
                                 storageName,
                                 NULL,  // createdDateTime
                                 &totalEntryCount,
                                 &totalEntrySize,
                                 NULL,  // indexState
                                 NULL,  // indexMode
                                 NULL,  // lastCheckedDateTime
                                 NULL  // errorMessage
                                )
        )
  {
    sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                     "storageId=%llu name=%'S totalEntryCount=%lu totalEntrySize=%llu",
                     storageId,
                     storageName,
                     totalEntryCount,
                     totalEntrySize
                    );
  }
  Index_doneList(&indexQueryHandle);

  String_delete(storageName);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_storageListClear
* Purpose: clear selected storage list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_storageListClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  // clear index ids
  Array_clear(&clientInfo->indexIdArray);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_storageListAdd
* Purpose: add to selected storage list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            indexId=<id>|0
*          Result:
\***********************************************************************/

LOCAL void serverCommand_storageListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId indexId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get index id
  if (!StringMap_getInt64(argumentMap,"indexId",&indexId,INDEX_ID_NONE))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexId=<id>");
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  // store index id
  Array_append(&clientInfo->indexIdArray,&indexId);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_storageListRemove
* Purpose: remove from selected storage list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            indexId=<id>|0
*          Result:
\***********************************************************************/

LOCAL void serverCommand_storageListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId indexId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get index id
  if (!StringMap_getInt64(argumentMap,"indexId",&indexId,INDEX_ID_NONE))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexId=<id>");
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  // remove index id
  Array_removeAll(&clientInfo->indexIdArray,&indexId,CALLBACK(NULL,NULL));

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_storageListInfo
* Purpose: get selected storage list info
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            storageCount=<n> \
*            totalEntryCount=<n> \
*            totalEntrySize=<n [bytes]>
*            totalEntryContentSize=<n [bytes]>
\***********************************************************************/

LOCAL void serverCommand_storageListInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  Errors error;
  ulong  storageCount,totalEntryCount;
  uint64 totalEntrySize,totalEntryContentSize;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(argumentMap);

  // check if index database is available
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  error = Index_getStoragesInfo(indexHandle,
                                INDEX_ID_ANY,  // uuidId
                                INDEX_ID_ANY,  // entityId,
                                NULL,  // jobUUID
                                Array_cArray(&clientInfo->indexIdArray),
                                Array_length(&clientInfo->indexIdArray),
                                INDEX_STATE_SET_ALL,
                                INDEX_MODE_SET_ALL,
                                NULL,
                                &storageCount,
                                &totalEntryCount,
                                &totalEntrySize,
                                &totalEntryContentSize
                               );
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,error,"get storages info from index database fail: %s",Error_getText(error));
    return;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"storageCount=%lu totalEntryCount=%lu totalEntrySize=%llu totalEntryContentSize=%llu",storageCount,totalEntryCount,totalEntrySize,totalEntryContentSize);
}

/***********************************************************************\
* Name   : serverCommand_entryList
* Purpose: list selected entries
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            name=<text> size=<n>
\***********************************************************************/

LOCAL void serverCommand_entryList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String           entryName;
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          entryId;
  const char       *type;
  uint64           size;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(argumentMap);

  // init variables
  entryName = String_new();

  // list entries
  error = Index_initListEntries(&indexQueryHandle,
                                indexHandle,
                                NULL, // indexIds
                                0, // indexIdCount
                                Array_cArray(&clientInfo->entryIdArray),
                                Array_length(&clientInfo->entryIdArray),
                                INDEX_TYPE_SET_ANY_ENTRY,
                                NULL, // name
                                DATABASE_ORDERING_NONE,
                                FALSE,  // newestOnly,
                                0,
                                INDEX_UNLIMITED
                               );
  if (error != ERROR_NONE)
  {
    String_delete(entryName);
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"init list storage fail: %s",Error_getText(error));
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && Index_getNextEntry(&indexQueryHandle,
                               NULL, // uuidId,
                               NULL, // jobUUID,
                               NULL, // entityId,
                               NULL, // scheduleUUID,
                               NULL, // archiveType,
                               NULL, // storageId,
                               NULL, // storageName
                               NULL, // storageDateTime
                               &entryId,
                               entryName,
                               NULL,  // destinationName
                               NULL,  // fileSystemType
                               &size,  // size
                               NULL,  // timeModified
                               NULL,  // userId
                               NULL,  // groupId
                               NULL,  // permission
                               NULL,  // fragmentOrBlockOffset
                               NULL  // fragmentSizeOrBlockCount
                              )
        )
  {
    type = "";
    switch (Index_getType(entryId))
    {
      case INDEX_TYPE_FILE:      type = "FILE";      break;
      case INDEX_TYPE_IMAGE:     type = "IMAGE";     break;
      case INDEX_TYPE_DIRECTORY: type = "DIRECTORY"; break;
      case INDEX_TYPE_LINK:      type = "LINK";      break;
      case INDEX_TYPE_HARDLINK:  type = "HARDLINK";  break;
      case INDEX_TYPE_SPECIAL:   type = "SPECIAL";   break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }

    sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                     "entryId=%llu name=%'S type=%s size=%llu",
                     entryId,
                     entryName,
                     type,
                     size
                    );
  }
  Index_doneList(&indexQueryHandle);

  String_delete(entryName);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_entryListClear
* Purpose: clear selected entry list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_entryListClear(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  Array_clear(&clientInfo->entryIdArray);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_entryListAdd
* Purpose: add to selected entry list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            entryId=<id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_entryListAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId entryId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get entry ids
  if (!StringMap_getInt64(argumentMap,"entryId",&entryId,INDEX_ID_NONE))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected entryId=<n>");
    return;
  }

  // check if index database is available, check if index database is ready
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  // add to id array
  Array_append(&clientInfo->entryIdArray,&entryId);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_entryListRemove
* Purpose: remove from selected entry list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            entryId=<id>
*          Result:
\***********************************************************************/

LOCAL void serverCommand_entryListRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId entryId;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get entry ids
  if (!StringMap_getInt64(argumentMap,"entryId",&entryId,INDEX_ID_NONE))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected entryId=<n>");
    return;
  }

  // check if index database is available, check if index database is ready
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  // add to id array
  Array_removeAll(&clientInfo->entryIdArray,&entryId,CALLBACK(NULL,NULL));

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_entryListInfo
* Purpose: get restore entry list info
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            totalEntryCount=<n> \
*            totalEntrySize=<n [bytes]>
*            totalEntryContentSize=<n [bytes]>
\***********************************************************************/

LOCAL void serverCommand_entryListInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  Errors error;
  ulong  totalEntryCount;
  uint64 totalEntrySize,totalEntryContentSize;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(argumentMap);

  // check if index database is available, check if index database is ready
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  error = Index_getEntriesInfo(indexHandle,
                               NULL,  // storageIds
                               0,  // storageIdCount
                               Array_cArray(&clientInfo->entryIdArray),
                               Array_length(&clientInfo->entryIdArray),
                               INDEX_TYPE_SET_ANY_ENTRY,
                               NULL,  // pattern,
                               FALSE,  // newestOnly
                               &totalEntryCount,
                               &totalEntrySize,
                               &totalEntryContentSize
                              );
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,error,"get entries info from index database fail: %s",Error_getText(error));
    return;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"totalEntryCount=%lu totalEntrySize=%llu totalEntryContentSize=%llu",totalEntryCount,totalEntrySize,totalEntryContentSize);
}

/***********************************************************************\
* Name   : serverCommand_storageDelete
* Purpose: delete storage and remove database index
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>|"" and/or
*            entityId=<id>|0 and/or
*            storageId=<id>|0
*          Result:
\***********************************************************************/

LOCAL void serverCommand_storageDelete(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  IndexId      entityId;
  IndexId      storageId;
  Errors       error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get uuid, job id, and/or storage id
  String_clear(jobUUID);
  entityId  = INDEX_ID_NONE;
  storageId = INDEX_ID_NONE;
  if (   !StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL)
      && !StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE)
      && !StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE)
     )
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid> or entityId=<id> or storageId=<id>");
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  // delete all storage files with job UUID
  if (!String_isEmpty(jobUUID))
  {
    error = deleteUUID(indexHandle,jobUUID);
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"%s",Error_getText(error));
      return;
    }
  }

  // delete all storage files of entity
  if (entityId != INDEX_ID_NONE)
  {
    error = deleteEntity(indexHandle,entityId);
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"%s",Error_getText(error));
      return;
    }
  }

  if (storageId != INDEX_ID_NONE)
  {
    // delete storage file
    error = deleteStorage(indexHandle,storageId);
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"%s",Error_getText(error));
      return;
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_restore
* Purpose: restore archives/files
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            type=ARCHIVES|ENTRIES
*            destination=<name>
*            directoryContent=yes|no
*            overwriteEntries=yes|no
*          Result:
\***********************************************************************/

LOCAL void serverCommand_restore(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  typedef enum
  {
    ARCHIVES,
    ENTRIES,
    UNKNOWN
  } Types;

  typedef struct
  {
    ClientInfo *clientInfo;
    uint       id;
  } RestoreCommandInfo;

  /***********************************************************************\
  * Name   : parseRestoreType
  * Purpose: parse restore type
  * Input  : naem - name
  *          type - type variable
  * Output : type - type
  * Return : TRUE iff parsed
  * Notes  : -
  \***********************************************************************/

  bool parseRestoreType(const char *name, Types *type)
  {
    assert(name != NULL);
    assert(type != NULL);

    if      (stringEqualsIgnoreCase("archives",name))
    {
      (*type) = ARCHIVES;
      return TRUE;
    }
    else if (stringEqualsIgnoreCase("entries",name))
    {
      (*type) = ENTRIES;
      return TRUE;
    }
    else
    {
      return FALSE;
    }
  }

  /***********************************************************************\
  * Name   : restoreUpdateStatusInfo
  * Purpose: update restore status info
  * Input  : restoreStatusInfo - create status info data,
  *          userData          - user data
  * Output : -
  * Return : TRUE to continue, FALSE to abort
  * Notes  : -
  \***********************************************************************/

  void restoreUpdateStatusInfo(const RestoreStatusInfo *restoreStatusInfo,
                               void                    *userData
                              )
  {
    RestoreCommandInfo *restoreCommandInfo = (RestoreCommandInfo*)userData;

    assert(restoreCommandInfo != NULL);
    assert(restoreStatusInfo != NULL);
    assert(restoreStatusInfo->entryName != NULL);
    assert(restoreStatusInfo->storageName != NULL);

    sendClientResult(restoreCommandInfo->clientInfo,
                     restoreCommandInfo->id,
                     FALSE,
                     ERROR_NONE,
                     "state=%s storageName=%'S storageDoneSize=%llu storageTotalSize=%llu entryName=%'S entryDoneSize=%llu entryTotalSize=%llu",
                     "running",
                     restoreStatusInfo->storageName,
                     restoreStatusInfo->storageDoneSize,
                     restoreStatusInfo->storageTotalSize,
                     restoreStatusInfo->entryName,
                     restoreStatusInfo->entryDoneSize,
                     restoreStatusInfo->entryTotalSize
                    );
  }

  /***********************************************************************\
  * Name   : restoreHandleError
  * Purpose: handle restore error
  * Input  : error             - error code
  *          restoreStatusInfo - create status info data,
  *          userData          - user data
  * Output : -
  * Return : ERROR_NONE or error code
  * Notes  : -
  \***********************************************************************/

  Errors restoreHandleError(Errors                  error,
                            const RestoreStatusInfo *restoreStatusInfo,
                            void                    *userData
                           )
  {
    RestoreCommandInfo *restoreCommandInfo = (RestoreCommandInfo*)userData;

    assert(restoreCommandInfo != NULL);

    // handle error
    error = clientAction(restoreCommandInfo->clientInfo,
                         restoreCommandInfo->id,
                         NULL,  // resultMap
                         "CONFIRM",
                         60*1000,
                         "error=%d errorMessage=%'s storage=%'S entry=%'S",
                         error,
                         Error_getText(error),
                         restoreStatusInfo->storageName,
                         restoreStatusInfo->entryName
                        );

    return error;
  }

  /***********************************************************************\
  * Name   : getPassword
  * Purpose: get password
  * Input  : name          - name (can be NULL)
  *          password      - password variable
  *          passwordType  - password type
  *          text          - text (file name, host name, etc.)
  *          validateFlag  - TRUE to validate input, FALSE otherwise
  *          weakCheckFlag - TRUE for weak password checking, FALSE
  *                          otherwise (print warning if password seems to
  *                          be a weak password)
  *          userData      - user data
  * Output : -
  * Return : ERROR_NONE or error code
  * Notes  : -
  \***********************************************************************/

  Errors getPassword(String        name,
                     Password      *password,
                     PasswordTypes passwordType,
                     const char    *text,
                     bool          validateFlag,
                     bool          weakCheckFlag,
                     void          *userData
                    )
  {
    RestoreCommandInfo *restoreCommandInfo = (RestoreCommandInfo*)userData;
    StringMap          resultMap;
    Errors             error;
    String             encryptType;
    String             encryptedPassword;

    assert(password != NULL);
    assert(restoreCommandInfo != NULL);

    UNUSED_VARIABLE(validateFlag);
    UNUSED_VARIABLE(weakCheckFlag);

    // init variables
    resultMap = StringMap_new();

    // request password
    error = clientAction(restoreCommandInfo->clientInfo,
                         restoreCommandInfo->id,
                         resultMap,
                         "REQUEST_PASSWORD",
                         60*1000,
                         "name=%'S passwordType=%'s passwordText=%'s",
                         name,
                         getPasswordTypeName(passwordType),
                         text
                        );
    if (error != ERROR_NONE)
    {
      StringMap_delete(resultMap);
      return error;
    }

    // get name, password
    if (name != NULL)
    {
      if (!StringMap_getString(resultMap,"name",name,NULL))
      {
        StringMap_delete(resultMap);
        return ERROR_EXPECTED_PARAMETER;
      }
    }
    encryptType = String_new();
    if (!StringMap_getString(resultMap,"encryptType",encryptType,NULL))
    {
      StringMap_delete(resultMap);
      return ERROR_EXPECTED_PARAMETER;
    }
    encryptedPassword = String_new();
    if (!StringMap_getString(resultMap,"encryptedPassword",encryptedPassword,NULL))
    {
      String_delete(encryptedPassword);
      StringMap_delete(resultMap);
      return ERROR_EXPECTED_PARAMETER;
    }
    if (!decryptPassword(password,clientInfo,encryptType,encryptedPassword))
    {
      String_delete(encryptedPassword);
      StringMap_delete(resultMap);
      return ERROR_INVALID_PASSWORD;
    }

    return ERROR_NONE;
  }

  Types              type;
  bool               directoryContentFlag;
  String             storageName;
  IndexId            entryId;
  String             entryName;
  StringList         storageNameList;
  IndexQueryHandle   indexQueryHandle;
  EntryList          restoreEntryList;
  RestoreCommandInfo restoreCommandInfo;
  Errors             error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get type, destination, directory content flag, overwrite flag
  if (!StringMap_getEnum(argumentMap,"type",&type,(StringMapParseEnumFunction)parseRestoreType,UNKNOWN))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected type=ARCHIVES|ENTRIES");
    return;
  }
  if (!StringMap_getString(argumentMap,"destination",clientInfo->jobOptions.destination,NULL))
  {
    String_delete(clientInfo->jobOptions.destination);
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected destination=<name>");
    return;
  }
  StringMap_getBool(argumentMap,"directoryContent",&directoryContentFlag,FALSE);
  if (!StringMap_getBool(argumentMap,"overwriteEntries",&clientInfo->jobOptions.overwriteEntriesFlag,FALSE))
  {
    String_delete(clientInfo->jobOptions.destination);
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected overwriteEntries=yes|no");
    return;
  }

  // init variables
  storageName = String_new();
  entryName   = String_new();

  // get storage/entry list
  error = ERROR_NONE;
  StringList_init(&storageNameList);
  EntryList_init(&restoreEntryList);
  switch (type)
  {
    case ARCHIVES:
      if (error == ERROR_NONE)
      {
        error = Index_initListStorages(&indexQueryHandle,
                                       indexHandle,
                                       INDEX_ID_ANY,  // uuidId
                                       INDEX_ID_ANY,  // entityId
                                       NULL,  // jobUUID
                                       Array_cArray(&clientInfo->indexIdArray),
                                       Array_length(&clientInfo->indexIdArray),
                                       INDEX_STATE_SET_ALL,
                                       INDEX_MODE_SET_ALL,
                                       NULL,  // name
                                       INDEX_STORAGE_SORT_MODE_NONE,
                                       DATABASE_ORDERING_NONE,
                                       0LL,  // offset
                                       INDEX_UNLIMITED
                                      );
        if (error == ERROR_NONE)
        {
          while (   !isCommandAborted(clientInfo,id)
                 && Index_getNextStorage(&indexQueryHandle,
                                         NULL,  // uuidId
                                         NULL,  // jobUUID
                                         NULL,  // entityId
                                         NULL,  // scheduleUUID
                                         NULL,  // archiveType
                                         NULL,  // storageId
                                         storageName,
                                         NULL,  // createdDateTime
                                         NULL,  // totalEntryCount
                                         NULL,  // totalEntrySize
                                         NULL,  // indexState,
                                         NULL,  // indexMode,
                                         NULL,  // lastCheckedDateTime,
                                         NULL   // errorMessage
                                        )
                )
          {
            StringList_append(&storageNameList,storageName);
          }
          Index_doneList(&indexQueryHandle);
        }
      }
      break;
    case ENTRIES:
      error = Index_initListEntries(&indexQueryHandle,
                                    indexHandle,
                                    NULL, // indexIds
                                    0, // indexIdCount
                                    Array_cArray(&clientInfo->entryIdArray),
                                    Array_length(&clientInfo->entryIdArray),
                                    INDEX_TYPE_SET_ANY_ENTRY,
                                    NULL, // name
                                    DATABASE_ORDERING_NONE,
                                    FALSE,  // newestOnly,
                                    0,
                                    INDEX_UNLIMITED
                                   );
      if (error == ERROR_NONE)
      {
        while (   !isCommandAborted(clientInfo,id)
               && Index_getNextEntry(&indexQueryHandle,
                                     NULL, // uuidId,
                                     NULL, // jobUUID,
                                     NULL, // entityId,
                                     NULL, // scheduleUUID,
                                     NULL, // archiveType,
                                     NULL, // storageId,
                                     storageName, // storageName
                                     NULL, // storageDateTime
                                     &entryId,
                                     entryName,
                                     NULL,  // destinationName
                                     NULL,  // fileSystemType
                                     NULL,  // size
                                     NULL,  // timeModified
                                     NULL,  // userId
                                     NULL,  // groupId
                                     NULL,  // permission
                                     NULL,  // fragmentOrBlockOffset
                                     NULL // fragmentSizeOrBlockCount
                                    )
              )
        {
          if (!StringList_contain(&storageNameList,storageName))
          {
            StringList_append(&storageNameList,storageName);
          }
          EntryList_append(&restoreEntryList,ENTRY_TYPE_FILE,entryName,PATTERN_TYPE_GLOB,NULL);
          if (directoryContentFlag && (Index_getType(entryId) == INDEX_TYPE_DIRECTORY))
          {
            String_appendCString(entryName,"/*");
            EntryList_append(&restoreEntryList,ENTRY_TYPE_FILE,entryName,PATTERN_TYPE_GLOB,NULL);
          }
        }
        Index_doneList(&indexQueryHandle);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  if (error != ERROR_NONE)
  {
    String_delete(entryName);
    String_delete(storageName);
    StringList_done(&storageNameList);
    sendClientResult(clientInfo,id,TRUE,error,"%s",Error_getText(error));
    return;
  }

  // restore
  restoreCommandInfo.clientInfo = clientInfo;
  restoreCommandInfo.id         = id;
  error = Command_restore(&storageNameList,
                          &restoreEntryList,
                          NULL,  // excludePatternList
                          NULL,  // deltaSourceList
                          &clientInfo->jobOptions,
                          CALLBACK(restoreUpdateStatusInfo,&restoreCommandInfo),
                          CALLBACK(restoreHandleError,&restoreCommandInfo),
                          CALLBACK(getPassword,&restoreCommandInfo),
                          NULL,  // pauseFlag
//TODO: callback
                          &restoreCommandInfo.clientInfo->abortFlag,
                          NULL  // logHandle
                         );
//isCommandAborted(restoreCommandInfo->clientInfo,restoreCommandInfo->id);
restoreCommandInfo.clientInfo->abortFlag = FALSE;
  sendClientResult(clientInfo,id,TRUE,error,"%s",Error_getText(error));
  EntryList_done(&restoreEntryList);
  StringList_done(&storageNameList);

  // free resources
  EntryList_done(&restoreEntryList);
  StringList_done(&storageNameList);
  String_delete(entryName);
  String_delete(storageName);
}

/***********************************************************************\
* Name   : serverCommand_restoreContinue
* Purpose: continue restore archives/files
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
\***********************************************************************/

LOCAL void serverCommand_restoreContinue(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_indexUUIDList
* Purpose: get index database UUID list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            [name=<text>]
*          Result:
*            uuidId=<n>
*            jobUUID=<uuid> \
*            name=<name> \
*            lastCreatedDateTime=<time stamp [s]> \
*            lastErrorMessage=<text> \
*            totalEntryCount=<n> \
*            totalEntrySize=<n> \
*            ...
\***********************************************************************/

LOCAL void serverCommand_indexUUIDList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String           name;
  String           lastErrorMessage;
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          uuidId;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  uint64           lastCreatedDateTime;
  ulong            totalEntryCount;
  uint64           totalEntrySize;
  SemaphoreLock    semaphoreLock;
  JobNode          *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // filter name
  name = String_new();
  StringMap_getString(argumentMap,"name",name,NULL);

  // check if index database is available
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  // init variables
  lastErrorMessage = String_new();

  error = Index_initListUUIDs(&indexQueryHandle,
                              indexHandle,
                              name,
                              0LL,  // offset
                              INDEX_UNLIMITED
                             );
  if (error != ERROR_NONE)
  {
    String_delete(lastErrorMessage);

    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"init uuid list fail: %s",Error_getText(error));

    String_delete(name);
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && Index_getNextUUID(&indexQueryHandle,
                              &uuidId,
                              jobUUID,
                              &lastCreatedDateTime,
                              lastErrorMessage,
                              &totalEntryCount,
                              &totalEntrySize
                             )
        )
  {
    // get job name
    String_set(name,jobUUID);
    SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
    {
      jobNode = findJobByUUID(jobUUID);
      if (jobNode != NULL)
      {
        String_set(name,jobNode->name);
      }
    }

    // send result
    sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                     "uuidId=%llu jobUUID=%S name=%'S lastCreatedDateTime=%llu lastErrorMessage=%'S totalEntryCount=%llu totalEntrySize=%llu",
                     uuidId,
                     jobUUID,
                     name,
                     lastCreatedDateTime,
                     lastErrorMessage,
                     totalEntryCount,
                     totalEntrySize
                    );
  }
  Index_doneList(&indexQueryHandle);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(lastErrorMessage);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_indexEntityList
* Purpose: get index database entity list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            [uuidId=<id>]
*            [name=<text>]
*          Result:
*            jobUUID=<uuid> \
*            scheduleUUID=<uuid> \
*            entityId=<id> \
*            archiveType=<type> \
*            lastCreatedDateTime=<time stamp [s]> \
*            lastErrorMessage=<error message>
*            totalEntryCount=<n> \
*            totalEntrySize=<n> \
*            ...
\***********************************************************************/

LOCAL void serverCommand_indexEntityList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  IndexId          uuidId;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  String           name;
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          entityId;
  StaticString     (scheduleUUID,MISC_UUID_STRING_LENGTH);
  uint64           createdDateTime;
  ArchiveTypes     archiveType;
  String           lastErrorMessage;
  ulong            totalEntryCount;
  uint64           totalEntrySize;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get uuid, name
  name = String_new();
  StringMap_getInt64(argumentMap,"uuidId",&uuidId,INDEX_ID_ANY);
  StringMap_getString(argumentMap,"name",name,NULL);

  // check if index database is available
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  // initialize variables
  lastErrorMessage = String_new();

  // get entities
  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 uuidId,
                                 NULL,  // jobUUID,
                                 NULL,  // scheduldUUID
                                 name,
                                 DATABASE_ORDERING_ASCENDING,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"init entity list fail: %s",Error_getText(error));
    String_delete(lastErrorMessage);
    String_delete(name);
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && Index_getNextEntity(&indexQueryHandle,
                                NULL,  // uudId,
                                jobUUID,
                                scheduleUUID,
                                &entityId,
                                &archiveType,
                                &createdDateTime,
                                lastErrorMessage,
                                &totalEntryCount,
                                &totalEntrySize
                               )
        )
  {
    sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                     "jobUUID=%S scheduleUUID=%S entityId=%lld archiveType=%s lastCreatedDateTime=%llu lastErrorMessage=%'S totalEntryCount=%lu totalEntrySize=%llu",
                     jobUUID,
                     scheduleUUID,
                     entityId,
                     ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,archiveType,"normal"),
                     createdDateTime,
                     lastErrorMessage,
                     totalEntryCount,
                     totalEntrySize
                    );
  }
  Index_doneList(&indexQueryHandle);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(lastErrorMessage);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_indexStorageList
* Purpose: get index database storage list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            entityId=<id>|0|*
*            name=<text>
*            indexStateSet=<state set>|*
*            indexModeSet=<mode set>|*
*            [offset=<n>]
*            [limit=<n>]
*            [sortBy=]
*          Result:
*            uuidId=<id>
*            jobUUID=<uuid>
*            jobName=<name>
*            entityId=<id>
*            scheduleUUID=<uuid>
*            archiveType=<type>
*            storageId=<id>
*            name=<name>
*            dateTime=<date/time>
*            totalEntryCount=<n>
*            totalEntrySize=<n>
*            indexState=<state>
*            indexMode=<mode>
*            lastCheckedDateTime=<date/time>
*            errorMessage=<text>
\***********************************************************************/

LOCAL void serverCommand_indexStorageList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint64           n;
  IndexId          entityId;
  String           name;
  bool             indexStateAny;
  IndexStateSet    indexStateSet;
  bool             indexModeAny;
  IndexModeSet     indexModeSet;
  uint64           offset;
  uint64           limit;
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  StorageSpecifier storageSpecifier;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  StaticString     (scheduleUUID,MISC_UUID_STRING_LENGTH);
  String           jobName;
  String           storageName;
  String           printableStorageName;
  String           errorMessage;
  IndexId          uuidId,storageId;
  ArchiveTypes     archiveType;
  uint64           dateTime;
  ulong            totalEntryCount;
  uint64           totalEntrySize;
  IndexStates      indexState;
  IndexModes       indexMode;
  uint64           lastCheckedDateTime;
  SemaphoreLock    semaphoreLock;
  JobNode          *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get entity id, filter storage pattern, index state set, index mode set, offset, limit
  if      (stringEquals(StringMap_getTextCString(argumentMap,"entityId","*"),"*"))
  {
    entityId = INDEX_ID_ANY;
  }
  else if (stringEquals(StringMap_getTextCString(argumentMap,"entityId","NONE"),"NONE"))
  {
    entityId = INDEX_ID_ENTITY_NONE;
  }
  else if (StringMap_getUInt64(argumentMap,"entityId",&n,INDEX_ID_ANY))
  {
    entityId = (IndexId)n;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected entityId=<id>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<text>");
    String_delete(name);
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexStateSet","*"),"*"))
  {
    indexStateAny = TRUE;
  }
  else if (StringMap_getEnumSet(argumentMap,"indexStateSet",&indexStateSet,(StringMapParseEnumFunction)Index_parseState,INDEX_STATE_SET_ALL,"|",INDEX_STATE_NONE))
  {
    indexStateAny = FALSE;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexStateSet=OK|CREATE|UPDATE_REQUESTED|UPDATE|ERROR|*");
    String_delete(name);
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexModeSet","*"),"*"))
  {
    indexModeAny = TRUE;
  }
  if (StringMap_getEnumSet(argumentMap,"indexModeSet",&indexModeSet,(StringMapParseEnumFunction)Index_parseMode,INDEX_MODE_SET_ALL,"|",INDEX_MODE_UNKNOWN))
  {
    indexModeAny = FALSE;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexModeSet=MANUAL|AUTO|*");
    String_delete(name);
    return;
  }
  StringMap_getUInt64(argumentMap,"offset",&offset,0);
  StringMap_getUInt64(argumentMap,"limit",&limit,INDEX_UNLIMITED);

  // check if index database is available
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  // initialize variables
  Storage_initSpecifier(&storageSpecifier);
  jobName              = String_new();
  storageName          = String_new();
  errorMessage         = String_new();
  printableStorageName = String_new();

  // list index
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 entityId,
                                 NULL,  // jobUUID
                                 NULL,  // indexIds
                                 0,  // indexIdCount
                                 indexStateAny ? INDEX_STATE_SET_ALL : indexStateSet,
                                 indexModeAny ? INDEX_MODE_SET_ALL : indexModeSet,
                                 name,
                                 INDEX_STORAGE_SORT_MODE_NONE,
                                 DATABASE_ORDERING_NONE,
                                 offset,
                                 limit
                                );
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"init storage list fail: %s",Error_getText(error));
    String_delete(printableStorageName);
    String_delete(errorMessage);
    String_delete(storageName);
    String_delete(jobName);
    Storage_doneSpecifier(&storageSpecifier);
    String_delete(name);
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && Index_getNextStorage(&indexQueryHandle,
                                 &uuidId,
                                 jobUUID,
                                 &entityId,
                                 scheduleUUID,
                                 &archiveType,
                                 &storageId,
                                 storageName,
                                 &dateTime,
                                 &totalEntryCount,
                                 &totalEntrySize,
                                 &indexState,
                                 &indexMode,
                                 &lastCheckedDateTime,
                                 errorMessage
                                )
        )
  {
    // get job name
    String_set(jobName,jobUUID);
    SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
    {
      jobNode = findJobByUUID(jobUUID);
      if (jobNode != NULL)
      {
        String_set(jobName,jobNode->name);
      }
    }

    // get printable name (if possible)
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error == ERROR_NONE)
    {
      String_set(printableStorageName,Storage_getPrintableName(&storageSpecifier,NULL));
    }
    else
    {
      String_set(printableStorageName,storageName);
    }

    sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                     "uuidId=%llu jobUUID=%S jobName=%'S entityId=%llu scheduleUUID=%S archiveType='%s' storageId=%llu name=%'S dateTime=%llu totalEntryCount=%lu totalEntrySize=%llu indexState=%'s indexMode=%'s lastCheckedDateTime=%llu errorMessage=%'S",
                     uuidId,
                     jobUUID,
                     jobName,
                     entityId,
                     scheduleUUID,
                     ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,archiveType,"normal"),
                     storageId,
                     printableStorageName,
                     dateTime,
                     totalEntryCount,
                     totalEntrySize,
                     Index_stateToString(indexState,"unknown"),
                     Index_modeToString(indexMode,"unknown"),
                     lastCheckedDateTime,
                     errorMessage
                    );
  }
  Index_doneList(&indexQueryHandle);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(printableStorageName);
  String_delete(errorMessage);
  String_delete(storageName);
  String_delete(jobName);
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_indexEntryList
* Purpose: get index database entry list
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<text>
*            indexType=<text>|*
*            newestOnly=yes|no
*            [offset=<n>]
*            [limit=<n>]
*          Result:
*            jobName=<name> archiveType=<type> \
*            storageName=<name> storageDateTime=<time stamp> entryId=<n> name=<name> entryType=FILE size=<n [bytes]> dateTime=<time stamp> \
*            userId=<n> groupId=<n> permission=<n> fragmentOffset=<n [bytes]> fragmentSize=<n [bytes]>
*
*            jobName=<name> archiveType=<type> \
*            storageName=<name> storageDateTime=<time stamp> entryId=<n> entryType=IMAGE name=<name> size=<n [bztes]> dateTime=<time stamp> \
*            blockOffset=<n [bytes]> blockCount=<n>
*
*            jobName=<name> archiveType=<type> \
*            storageName=<name> storageDateTime=<time stamp> entryId=<n> entryType=DIRECTORY name=<name> size=<n [bztes]> dateTime=<time stamp> \
*            userId=<n> groupId=<n> permission=<n>
*
*            jobName=<name> archiveType=<type> \
*            storageName=<name> storageDateTime=<time stamp> entryId=<n> entryType=LINK linkName=<name> name=<name> \
*            dateTime=<time stamp> userId=<n> groupId=<n> permission=<n>
*
*            jobName=<name> archiveType=<type> \
*            storageName=<name> storageDateTime=<time stamp> entryId=<n> entryType=HARDLINK name=<name> dateTime=<time stamp>
*            userId=<n> groupId=<n> permission=<n> fragmentOffset=<n [bytes]> fragmentSize=<n [bytes]>
*
*            jobName=<name> archiveType=<type> \
*            storageName=<name> storageDateTime=<time stamp> entryId=<n> entryType=SPECIAL name=<name> dateTime=<time stamp> \
*            userId=<n> groupId=<n> permission=<n>
\***********************************************************************/

LOCAL void serverCommand_indexEntryList(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  #define SEND_FILE_ENTRY(jobName,archiveType,storageName,storageDateTime,entryId,name,size,dateTime,userId,groupId,permission,fragmentOffset,fragmentSize) \
    do \
    { \
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE, \
                       "jobName=%'S archiveType=%s storageName=%'S storageDateTime=%llu entryId=%lld entryType=FILE name=%'S size=%llu dateTime=%llu userId=%u groupId=%u permission=%u fragmentOffset=%llu fragmentSize=%llu", \
                       jobName, \
                       ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,archiveType,"normal"), \
                       storageName, \
                       storageDateTime, \
                       entryId, \
                       name, \
                       size, \
                       dateTime, \
                       userId, \
                       groupId, \
                       permission, \
                       fragmentOffset, \
                       fragmentSize \
                      ); \
    } \
    while (0)
  #define SEND_IMAGE_ENTRY(jobName,archiveType,storageName,storageDateTime,entryId,name,fileSystemType,size,blockOffset,blockCount) \
    do \
    { \
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE, \
                       "jobName=%'S archiveType=%s storageName=%'S storageDateTime=%llu entryId=%lld entryType=IMAGE name=%'S fileSystemType=%s size=%llu blockOffset=%llu blockCount=%llu", \
                       jobName, \
                       ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,archiveType,"normal"), \
                       storageName, \
                       storageDateTime, \
                       entryId, \
                       name, \
                       FileSystem_getName(fileSystemType), \
                       size, \
                       blockOffset, \
                       blockCount \
                      ); \
    } \
    while (0)
  #define SEND_DIRECTORY_ENTRY(jobName,archiveType,storageName,storageDateTime,entryId,name,size,dateTime,userId,groupId,permission) \
    do \
    { \
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE, \
                       "jobName=%'S archiveType=%s storageName=%'S storageDateTime=%llu entryId=%lld entryType=DIRECTORY name=%'S size=%llu dateTime=%llu userId=%u groupId=%u permission=%u", \
                       jobName, \
                       ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,archiveType,"normal"), \
                       storageName, \
                       storageDateTime, \
                       entryId, \
                       name, \
                       size, \
                       dateTime, \
                       userId, \
                       groupId, \
                       permission \
                      ); \
    } \
    while (0)
  #define SEND_LINK_ENTRY(jobName,archiveType,storageName,storageDateTime,entryId,name,destinationName,dateTime,userId,groupId,permission) \
    do \
    { \
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE, \
                       "jobName=%'S archiveType=%s storageName=%'S storageDateTime=%llu entryId=%lld entryType=LINK name=%'S destinationName=%'S dateTime=%llu userId=%u groupId=%u permission=%u", \
                       jobName, \
                       ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,archiveType,"normal"), \
                       storageName, \
                       storageDateTime, \
                       entryId, \
                       name, \
                       destinationName, \
                       dateTime, \
                       userId, \
                       groupId, \
                       permission \
                      ); \
    } \
    while (0)
  #define SEND_HARDLINK_ENTRY(jobName,archiveType,storageName,storageDateTime,entryId,name,size,dateTime,userId,groupId,permission,fragmentOffset,fragmentSize) \
    do \
    { \
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE, \
                       "jobName=%'S archiveType=%s storageName=%'S storageDateTime=%llu entryId=%lld entryType=HARDLINK name=%'S size=%lld dateTime=%llu userId=%u groupId=%u permission=%u fragmentOffset=%llu fragmentSize=%llu", \
                       jobName, \
                       ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,archiveType,"normal"), \
                       storageName, \
                       storageDateTime, \
                       entryId, \
                       name, \
                       size, \
                       dateTime, \
                       userId, \
                       groupId, \
                       permission, \
                       fragmentOffset, \
                       fragmentSize \
                      ); \
    } \
    while (0)
  #define SEND_SPECIAL_ENTRY(jobName,archiveType,storageName,storageDateTime,entryId,name,dateTime,userId,groupId,permission) \
    do \
    { \
      sendClientResult(clientInfo,id,FALSE,ERROR_NONE, \
                       "jobName=%'S archiveType=%s storageName=%'S storageDateTime=%llu entryId=%lld entryType=SPECIAL name=%'S dateTime=%llu userId=%u groupId=%u permission=%u", \
                       jobName, \
                       ConfigValue_selectToString(CONFIG_VALUE_ARCHIVE_TYPES,archiveType,NULL), \
                       storageName, \
                       storageDateTime, \
                       entryId, \
                       name, \
                       dateTime, \
                       userId, \
                       groupId, \
                       permission \
                      ); \
    } \
    while (0)

  String           name;
  bool             indexTypeAny;
  IndexTypes       indexType;
  bool             newestOnly;
  uint64           offset;
  uint64           limit;
  IndexId          prevUUIDId;
  String           jobName;
  String           storageName;
  uint64           storageDateTime;
  String           entryName;
  String           destinationName;
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  FileSystemTypes  fileSystemType;
  IndexId          uuidId,entityId,storageId,entryId;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes     archiveType;
  uint64           size;
  uint64           timeModified;
  uint             userId,groupId;
  uint             permission;
  uint64           fragmentOrBlockOffset,fragmentSizeOrBlockCount;
  SemaphoreLock    semaphoreLock;
  const JobNode    *jobNode;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // filter entry pattern, index type, new entries only, offset, limit
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<text>");
    String_delete(name);
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexType","*"),"*"))
  {
    indexTypeAny = TRUE;
  }
  else if (StringMap_getEnum(argumentMap,"indexType",&indexType,(StringMapParseEnumFunction)Index_parseType,INDEX_TYPE_FILE))
  {
    indexTypeAny = FALSE;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexType=FILE|IMAGE|DIRECTORY|LINK|HARDLINK|SPECIAL|*");
    String_delete(name);
    return;
  }
  if (!StringMap_getBool(argumentMap,"newestOnly",&newestOnly,FALSE))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected newestOnly=yes|no");
    String_delete(name);
    return;
  }
  StringMap_getUInt64(argumentMap,"offset",&offset,0);
  StringMap_getUInt64(argumentMap,"limit",&limit,INDEX_UNLIMITED);

  // check if index database is available, check if index database is ready
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  // initialize variables
  prevUUIDId      = INDEX_ID_NONE;
  jobName         = String_new();
  storageName     = String_new();
  entryName       = String_new();
  destinationName = String_new();
  error = Index_initListEntries(&indexQueryHandle,
                                indexHandle,
                                Array_cArray(&clientInfo->indexIdArray),
                                Array_length(&clientInfo->indexIdArray),
                                NULL,  // entryIds
                                0,  // entryIdCount
                                indexTypeAny ? INDEX_TYPE_SET_ANY_ENTRY : SET_VALUE(indexType),
                                name,
                                DATABASE_ORDERING_NONE,
                                newestOnly,
                                offset,
                                limit
                               );
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"init list entries fail: %s",Error_getText(error));
    String_delete(name);
    String_delete(storageName);
    String_delete(name);
    return;
  }
  while (   !isCommandAborted(clientInfo,id)
         && Index_getNextEntry(&indexQueryHandle,
                               &uuidId,
                               jobUUID,
                               &entityId,
                               NULL,  // scheduleUUID,
                               &archiveType,
                               &storageId,
                               storageName,
                               &storageDateTime,
                               &entryId,
                               entryName,
                               destinationName,
                               &fileSystemType,
                               &size,
                               &timeModified,
                               &userId,
                               &groupId,
                               &permission,
                               &fragmentOrBlockOffset,
                               &fragmentSizeOrBlockCount
                              )
        )
  {
    // get job name
    if (uuidId != prevUUIDId)
    {
      SEMAPHORE_LOCKED_DO(semaphoreLock,&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
      {
        jobNode = findJobByUUID(jobUUID);
        if (jobNode != NULL)
        {
          String_set(jobName,jobNode->name);
        }
        else
        {
          String_clear(jobName);
        }
      }
      prevUUIDId = uuidId;
    }
    if (String_isEmpty(jobName)) String_set(jobName,jobUUID);

    // send entry data
    switch (Index_getType(entryId))
    {
      case INDEX_TYPE_FILE:
        SEND_FILE_ENTRY(jobName,archiveType,storageName,storageDateTime,entryId,entryName,size,timeModified,userId,groupId,permission,fragmentOrBlockOffset,fragmentSizeOrBlockCount);
        break;
      case INDEX_TYPE_IMAGE:
        SEND_IMAGE_ENTRY(jobName,archiveType,storageName,storageDateTime,entryId,entryName,fileSystemType,size,fragmentOrBlockOffset,fragmentSizeOrBlockCount);
        break;
      case INDEX_TYPE_DIRECTORY:
        SEND_DIRECTORY_ENTRY(jobName,archiveType,storageName,storageDateTime,entryId,entryName,size,timeModified,userId,groupId,permission);
        break;
      case INDEX_TYPE_LINK:
        SEND_LINK_ENTRY(jobName,archiveType,storageName,storageDateTime,entryId,entryName,destinationName,timeModified,userId,groupId,permission);
        break;
      case INDEX_TYPE_HARDLINK:
        SEND_HARDLINK_ENTRY(jobName,archiveType,storageName,storageDateTime,entryId,entryName,size,timeModified,userId,groupId,permission,fragmentOrBlockOffset,fragmentSizeOrBlockCount);
        break;
      case INDEX_TYPE_SPECIAL:
        SEND_SPECIAL_ENTRY(jobName,archiveType,storageName,storageDateTime,entryId,entryName,timeModified,userId,groupId,permission);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }
  Index_doneList(&indexQueryHandle);
  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
  String_delete(destinationName);
  String_delete(entryName);
  String_delete(storageName);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_indexEntityAdd
* Purpose: add entity to index database
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            jobUUID=<uuid>
*            archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS
*            [createdDateTime=<time stamp [s]>]
*          Result:
*            entityId=<id>
\***********************************************************************/

LOCAL void serverCommand_indexEntityAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes archiveType;
  uint64       createdDateTime;
  IndexId      entityId;
  Errors       error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get jobUUID, archive type
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)parseArchiveType,ARCHIVE_TYPE_UNKNOWN))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL");
    return;
  }
  StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL);

  // check if index database is available
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  // create new entity
  error = Index_newEntity(indexHandle,
                           jobUUID,
                           NULL,  // scheduleUUID,
                           archiveType,
                           createdDateTime,
                           &entityId
                          );
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,error,"add entity fail");
    return;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"entityId=%llu",entityId);

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_indexStorageAdd
* Purpose: add storage to index database (if not already exists)
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            pattern=<text>
*            [patternType=<type>]
*            [forceRefresh=yes|no]
*          Result:
*            storageId=<id>
\***********************************************************************/

LOCAL void serverCommand_indexStorageAdd(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String           pattern;
  PatternTypes     patternType;
  bool             forceRefresh;
  StorageSpecifier storageSpecifier;
  StorageHandle    storageHandle;
  IndexId          storageId;
  Errors           error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get pattern type, pattern, force refresh
  pattern = String_new();
  if (!StringMap_getString(argumentMap,"pattern",pattern,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected pattern=<text>");
    String_delete(pattern);
    return;
  }
  StringMap_getEnum(argumentMap,"patternType",&patternType,(StringMapParseEnumFunction)Pattern_parsePatternType,PATTERN_TYPE_GLOB);
  StringMap_getBool(argumentMap,"forceRefresh",&forceRefresh,FALSE);

  // check if index database is available
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(pattern);
    return;
  }

  // init variables
  Storage_initSpecifier(&storageSpecifier);

  // create index for matching files
//TODO
#ifndef WERROR
#warning remove
#endif

  error = ERROR_UNKNOWN;

  // try to open as storage file
  if (error != ERROR_NONE)
  {
    if (   (Storage_parseName(&storageSpecifier,pattern) == ERROR_NONE)
        && !Storage_isPatternSpecifier(&storageSpecifier)
//        && String_endsWithCString(storageSpecifier.archiveName,FILE_NAME_EXTENSION_ARCHIVE_FILE)
       )
    {
      if (Storage_init(&storageHandle,
                       &storageSpecifier,
                       NULL, // jobOptions
                       &globalOptions.indexDatabaseMaxBandWidthList,
                       SERVER_CONNECTION_PRIORITY_LOW,
                       CALLBACK(NULL,NULL),  // updateStatusInfo
                       CALLBACK(NULL,NULL),  // getPassword
                       CALLBACK(NULL,NULL)  // requestVolume
                      ) == ERROR_NONE
         )
      {
        if (Index_findStorageByName(indexHandle,
                                    &storageSpecifier,
                                    NULL,  // archiveName
                                    NULL,  // uuidId
                                    NULL,  // entityId
                                    NULL,  // jobUUID
                                    NULL,  // scheduleUUID
                                    &storageId,
                                    NULL,  // indexState,
                                    NULL  // lastCheckedDateTime
                                   )
           )
        {
          if (forceRefresh)
          {
            Index_setState(indexHandle,
                           storageId,
                           INDEX_STATE_UPDATE_REQUESTED,
                           Misc_getCurrentDateTime(),
                           NULL
                          );
          }
        }
        else
        {
          error = Index_newStorage(indexHandle,
                                   INDEX_ID_NONE, // entityId
                                   Storage_getPrintableName(&storageSpecifier,NULL),
                                   INDEX_STATE_UPDATE_REQUESTED,
                                   INDEX_MODE_MANUAL,
                                   &storageId
                                  );
          if (error == ERROR_NONE)
          {
            sendClientResult(clientInfo,id,FALSE,ERROR_NONE,"storageId=%llu name=%'S",
                             storageId,
                             Storage_getPrintableName(&storageSpecifier,NULL)
                            );
          }
        }
        Storage_done(&storageHandle);
      }
    }
  }

  // try to open as directory: add all matching entries
  if (error != ERROR_NONE)
  {
    error = Storage_forAll(pattern,
                           CALLBACK_INLINE(Errors,(ConstString storageName, const FileInfo *fileInfo, void *userData),
                           {
                             UNUSED_VARIABLE(fileInfo);
                             UNUSED_VARIABLE(userData);

                             error = Storage_parseName(&storageSpecifier,storageName);
                             if (error == ERROR_NONE)
                             {
                               if (String_endsWithCString(storageSpecifier.archiveName,FILE_NAME_EXTENSION_ARCHIVE_FILE))
                               {
//fprintf(stderr,"%s, %d: storageName=%s\n",__FILE__,__LINE__,String_cString(storageName));
                                 if (Index_findStorageByName(indexHandle,
                                                             &storageSpecifier,
                                                             NULL,  // archiveName
                                                             NULL,  // uuidId
                                                             NULL,  // entityId
                                                             NULL,  // jobUUID
                                                             NULL,  // scheduleUUID
                                                             &storageId,
                                                             NULL,  // indexState,
                                                             NULL  // lastCheckedDateTime
                                                            )
                                    )
                                 {
                                   if (forceRefresh)
                                   {
                                     Index_setState(indexHandle,
                                                    storageId,
                                                    INDEX_STATE_UPDATE_REQUESTED,
                                                    Misc_getCurrentDateTime(),
                                                    NULL
                                                   );
                                   }
                                 }
                                 else
                                 {
                                   error = Index_newStorage(indexHandle,
                                                            INDEX_ID_NONE, // entityId
                                                            storageName,
                                                            INDEX_STATE_UPDATE_REQUESTED,
                                                            INDEX_MODE_MANUAL,
                                                            &storageId
                                                           );
                                   if (error != ERROR_NONE)
                                   {
                                     return error;
                                   }

                                   sendClientResult(clientInfo,id,FALSE,ERROR_NONE,"storageId=%llu name=%'S",
                                                    storageId,
                                                    Storage_getPrintableName(&storageSpecifier,storageName)
                                                   );
                                 }
                               }
                             }

                             return !isCommandAborted(clientInfo,id) ? ERROR_NONE : ERROR_ABORTED;
                           },NULL)
                          );
  }

  if (error == ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,error,"%s",Error_getText(error));
  }

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(pattern);
}

//TODO: obsolete
#if 0
/***********************************************************************\
* Name   : serverCommand_indexEntitySet
* Purpose: set entity type in index database
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            entityId=<id>
*            archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS
*            [createdDateTime=<time stamp [s]>]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_indexEntitySet(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString (jobUUID,MISC_UUID_STRING_LENGTH);
  ArchiveTypes archiveType;
  uint64       createdDateTime;
  IndexId      entityId;
  Errors       error;

#ifndef WERROR
#warning TODO
#endif
  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(argumentMap);

  // get jobUUID, archive type, created
  if (!StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid>");
    return;
  }
  if (!StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)parseArchiveType,ARCHIVE_TYPE_UNKNOWN))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL");
    return;
  }
  StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL);

  // check if index database is available
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  // create new entity
  error = Index_newEntity(indexHandle,
                           jobUUID,
                           NULL,  // scheduleUUID,
                           archiveType,
                           createdDateTime,
                           &entityId
                          );
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,error,"add entity fail");
    return;
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"entityId=%llu",entityId);

  // free resources
}
#endif

/***********************************************************************\
* Name   : serverCommand_indexAssign
* Purpose: assign index database for storage; create entity if requested
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            toJobUUID=<uuid>|""
*            [archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL|CONTINUOUS|]
*            jobUUID=<uuid>|"" or entityId=<id>|0 or storageId=<id>|0
*
*          or
*
*            toEntityId=<id>|0
*            archiveType=NORMAL|FULL|INCREMENTAL|DIFFERENTIAL
*            createdDateTime=<n>|0
*            jobUUID=<uuid>|"" or entityId=<id>|0 or storageId=<id>|0
*
*          Result:
\***********************************************************************/

LOCAL void serverCommand_indexAssign(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  StaticString     (toJobUUID,MISC_UUID_STRING_LENGTH);
  IndexId          toEntityId;
  ArchiveTypes     archiveType;
  uint64           createdDateTime;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  IndexId          entityId;
  IndexId          storageId;
  Errors           error;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get toJobUUID, toEntityId, archive type, jobUUID/entityId/storageId
  String_clear(toJobUUID);
  toEntityId = INDEX_ID_NONE;
  if (   !StringMap_getString(argumentMap,"toJobUUID",toJobUUID,NULL)
      && !StringMap_getInt64(argumentMap,"toEntityId",&toEntityId,INDEX_ID_NONE)
     )
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected toJobUUID=<uuid> or toEntityId=<id>");
    return;
  }
  StringMap_getEnum(argumentMap,"archiveType",&archiveType,(StringMapParseEnumFunction)parseArchiveType,ARCHIVE_TYPE_NONE);
  StringMap_getUInt64(argumentMap,"createdDateTime",&createdDateTime,0LL);
  String_clear(jobUUID);
  entityId  = INDEX_ID_NONE;
  storageId = INDEX_ID_NONE;
  if (   !StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL)
      && !StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE)
      && !StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE)
     )
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid> or entityId=<id> or storageId=<id>");
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  if (!String_isEmpty(jobUUID))
  {
    if      (toEntityId != INDEX_ID_NONE)
    {
      // assign all storages of all entities of job to other entity
      error = Index_assignTo(indexHandle,
                             jobUUID,
                             INDEX_ID_NONE,  // entityId,
                             INDEX_ID_NONE,  // storageId
                             toEntityId,
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"assign storage fail: %s",Error_getText(error));
        return;
      }
    }
    else if (!String_isEmpty(toJobUUID))
    {
      // create entity for other job
      error = Index_newEntity(indexHandle,
                              toJobUUID,
                              NULL,  // scheduleUUID
                              archiveType,
                              createdDateTime,
                              &toEntityId
                             );
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"cannot create entity for %S: %s",toJobUUID,Error_getText(error));
        return;
      }

      // assign all storages of all entities of job to other entity
      error = Index_assignTo(indexHandle,
                             jobUUID,
                             INDEX_ID_NONE,  // entityId
                             INDEX_ID_NONE,  // storageId
                             toEntityId,
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"assign storage fail: %s",Error_getText(error));
        return;
      }
    }
  }

  if (entityId != INDEX_ID_NONE)
  {
    // assign entity to another entity
    if      (toEntityId != INDEX_ID_NONE)
    {
      // assign all storages of entity to other entity
      error = Index_assignTo(indexHandle,
                             NULL,  // jobUUID
                             entityId,
                             INDEX_ID_NONE,  // storageId
                             toEntityId,
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"assign storage fail: %s",Error_getText(error));
        return;
      }
    }
    else if (!String_isEmpty(toJobUUID))
    {
      // create entity for other job
      error = Index_newEntity(indexHandle,
                              toJobUUID,
                              NULL,  // scheduleUUID
                              archiveType,
                              createdDateTime,
                              &toEntityId
                             );
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"cannot create entity for %S: %s",toJobUUID,Error_getText(error));
        return;
      }

      // assign all storages of entity to other entity
      error = Index_assignTo(indexHandle,
                             NULL,  // jobUUID
                             INDEX_ID_NONE,  // entityId
                             storageId,
                             toEntityId,
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"assign storage fail: %s",Error_getText(error));
        return;
      }
    }
  }

  if (storageId != INDEX_ID_NONE)
  {
    if      (toEntityId != INDEX_ID_NONE)
    {
      // assign storages to another entity
      error = Index_assignTo(indexHandle,
                             NULL,  // jobUUID
                             INDEX_ID_NONE,  // entityId
                             storageId,
                             toEntityId,
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"assign storage fail: %s",Error_getText(error));
        return;
      }
    }
    else if (!String_isEmpty(toJobUUID))
    {
      // create entity for other job
      error = Index_newEntity(indexHandle,
                              toJobUUID,
                              NULL,
                              archiveType,
                              createdDateTime,
                              &toEntityId
                             );
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"cannot create entity for %S: %s",toJobUUID,Error_getText(error));
        return;
      }

      // assign storage to another entity
      error = Index_assignTo(indexHandle,
                             NULL,  // jobUUID
                             INDEX_ID_NONE,  // entityId
                             storageId,
                             toEntityId,
                             archiveType,
                             INDEX_ID_NONE  // toStorageId
                            );
      if (error != ERROR_NONE)
      {
        sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"assign storage fail: %s",Error_getText(error));
        return;
      }
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_indexRefresh
* Purpose: refresh index database for storage
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            state=<state>|*
*            uuidId=<id>|0 and/or
*            entityId=<id>|0 and/or
*            storageId=<id>|0 and/or
*            jobUUID=<uuid>|"" and/or
*            pattern=<text>
*            [patternType=<type>]
*          Result:
\***********************************************************************/

LOCAL void serverCommand_indexRefresh(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  bool             stateAny;
  IndexStates      state;
  IndexId          uuidId;
  IndexId          entityId;
  IndexId          storageId;
  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
  String           name;
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexStates      indexState;
  Pattern          pattern;
  String           storageName;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // state, uuidId/entityId/storageId/jobUUID/name
  if      (stringEquals(StringMap_getTextCString(argumentMap,"state","*"),"*"))
  {
    stateAny = TRUE;
  }
  else if (StringMap_getEnum(argumentMap,"state",&state,(StringMapParseEnumFunction)Index_parseState,INDEX_STATE_NONE))
  {
    stateAny = FALSE;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected filter state=OK|UPDATE_REQUESTED|UPDATE|ERROR|*");
    return;
  }
  name = String_new();
  if (   !StringMap_getInt64(argumentMap,"uuidId",&uuidId,INDEX_ID_NONE)
      && !StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE)
      && !StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE)
      && !StringMap_getString(argumentMap,"jobUUID",jobUUID,NULL)
      && !StringMap_getString(argumentMap,"pattern",name,NULL)
     )
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected uuidId=<id> or entityId=<id> or storageId=<id> or jobUUID=<uuid> or name=<text>");
    String_delete(name);
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  storageName = String_new();

  if ((uuidId == INDEX_ID_NONE) && (storageId == INDEX_ID_NONE) && (entityId == INDEX_ID_NONE) && String_isEmpty(jobUUID) && String_isEmpty(name))
  {
    // refresh all storage with specific state
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   INDEX_ID_ANY,  // entity id
                                   NULL,  // jobUUID
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // name
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"init list storage fail: %s",Error_getText(error));
      String_delete(storageName);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidId
                                   NULL,  // jobUUID
                                   NULL,  // entityId
                                   NULL,  // scheduleUUID
                                   NULL,  // archiveType
                                   &storageId,
                                   NULL,  // storageName
                                   NULL,  // createdDateTime
                                   NULL,  // totalEntryCount
                                   NULL,  // totalEntrySize
                                   &indexState,
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL  // errorMessage
                                  )
          )
    {
      if (stateAny || (state == indexState))
      {
        // set state
        Index_setState(indexHandle,
                       storageId,
                       INDEX_STATE_UPDATE_REQUESTED,
                       0LL,
                       NULL
                      );
      }
    }
    Index_doneList(&indexQueryHandle);
  }

  if (uuidId != INDEX_ID_NONE)
  {
    // refresh all storage of uuid
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   uuidId,
                                   INDEX_ID_ANY,  // entityId,
                                   NULL,  // jobUUID
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // name
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"init list storage fail: %s",Error_getText(error));
      String_delete(storageName);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidId
                                   NULL,  // jobUUID
                                   NULL,  // entityId
                                   NULL,  // scheduleUUID
                                   NULL,  // archiveType
                                   &storageId,
                                   NULL,  // storageName
                                   NULL,  // createdDateTime
                                   NULL,  // totalEntryCount
                                   NULL,  // totalEntrySize
                                   NULL,  // indexState
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL  // errorMessage
                                  )
          )
    {
      if (stateAny || (state == indexState))
      {
        // set state
        Index_setState(indexHandle,
                       storageId,
                       INDEX_STATE_UPDATE_REQUESTED,
                       0LL,
                       NULL
                      );
      }
    }
    Index_doneList(&indexQueryHandle);
  }

  if (entityId != INDEX_ID_NONE)
  {
    // refresh all storage of entity
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   entityId,
                                   NULL,  // jobUUID
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // name
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"init list storage fail: %s",Error_getText(error));
      String_delete(storageName);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidId
                                   NULL,  // jobUUID
                                   NULL,  // entityId
                                   NULL,  // scheduleUUID
                                   NULL,  // archiveType
                                   &storageId,
                                   NULL,  // storageName
                                   NULL,  // createdDateTime
                                   NULL,  // totalEntryCount
                                   NULL,  // totalEntrySize
                                   NULL,  // indexState
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL  // errorMessage
                                  )
          )
    {
      if (stateAny || (state == indexState))
      {
        // set state
        Index_setState(indexHandle,
                       storageId,
                       INDEX_STATE_UPDATE_REQUESTED,
                       0LL,
                       NULL
                      );
      }
    }
    Index_doneList(&indexQueryHandle);
  }

  if (storageId != INDEX_ID_NONE)
  {
    // refresh storage
    Index_setState(indexHandle,
                   storageId,
                   INDEX_STATE_UPDATE_REQUESTED,
                   0LL,
                   NULL
                  );
  }


  if (!String_isEmpty(jobUUID))
  {
    // refresh all storage of all entities of job
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   INDEX_ID_ANY,  // entity id
                                   jobUUID,
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // name
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"init list storage fail: %s",Error_getText(error));
      String_delete(storageName);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidId
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID
                                   NULL,  // entityId
                                   NULL,  // archiveType
                                   &storageId,
                                   NULL,  // storageName
                                   NULL,  // createdDateTime
                                   NULL,  // totalEntryCount
                                   NULL,  // totalEntrySize
                                   NULL,  // indexState
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL  // errorMessage
                                  )
          )
    {
      if (stateAny || (state == indexState))
      {
        // set state
        Index_setState(indexHandle,
                       storageId,
                       INDEX_STATE_UPDATE_REQUESTED,
                       0LL,
                       NULL
                      );
      }
    }
    Index_doneList(&indexQueryHandle);
  }

  if (!String_isEmpty(name))
  {
    // refresh all storage which match name
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   INDEX_ID_ANY,  // entityId,
                                   NULL,  // jobUUID
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   name,
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"init list storage fail: %s",Error_getText(error));
      Pattern_done(&pattern);
      String_delete(storageName);
      String_delete(name);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidId
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID
                                   NULL,  // entityId
                                   NULL,  // archiveType
                                   &storageId,
                                   storageName,
                                   NULL,  // createdDateTime
                                   NULL,  // totalEntryCount
                                   NULL,  // totalEntrySize
                                   &indexState,
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL  // errorMessage
                                  )
          )
    {
      if (stateAny || (state == indexState))
      {
        // set state
        Index_setState(indexHandle,
                       storageId,
                       INDEX_STATE_UPDATE_REQUESTED,
                       0LL,
                       NULL
                      );
      }
    }
    Index_doneList(&indexQueryHandle);
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // trigger index thread
  Semaphore_signalModified(&indexThreadTrigger);

  // free resources
  String_delete(storageName);
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_indexRemove
* Purpose: remove job/entity/storage from index database
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            <state>|*
*            uuidId=<id>|0 or entityId=<id>|0 or storageId=<id>|0
*          Result:
\***********************************************************************/

LOCAL void serverCommand_indexRemove(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  bool             stateAny;
  IndexStates      state;
  IndexId          uuidId;
  IndexId          entityId;
  IndexId          storageId;
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  StorageSpecifier storageSpecifier;
  String           storageName;
  String           printableStorageName;
  IndexStates      indexState;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get state and jobUUID, entityId, or storageId
  if      (stringEquals(StringMap_getTextCString(argumentMap,"state","*"),"*"))
  {
    stateAny = TRUE;
  }
  else if (StringMap_getEnum(argumentMap,"state",&state,(StringMapParseEnumFunction)Index_parseState,INDEX_STATE_NONE))
  {
    stateAny = FALSE;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected filter state=OK|UPDATE_REQUESTED|UPDATE|ERROR|*");
    return;
  }
  uuidId    = INDEX_ID_NONE;
  entityId  = INDEX_ID_NONE;
  storageId = INDEX_ID_NONE;
  if (   !StringMap_getInt64(argumentMap,"uuidId",&uuidId,INDEX_ID_NONE)
      && !StringMap_getInt64(argumentMap,"entityId",&entityId,INDEX_ID_NONE)
      && !StringMap_getInt64(argumentMap,"storageId",&storageId,INDEX_ID_NONE)
     )
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected jobUUID=<uuid> or entityId=<id> or storageId=<id>");
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    return;
  }

  if (   (uuidId == INDEX_ID_NONE)
      && (storageId == INDEX_ID_NONE)
      && (entityId == INDEX_ID_NONE)
     )
  {
    // initialize variables
    Storage_initSpecifier(&storageSpecifier);
    storageName          = String_new();
    printableStorageName = String_new();

    // delete all indizes with specific state
    error = Index_initListStorages(&indexQueryHandle,
                                   indexHandle,
                                   INDEX_ID_ANY,  // uuidId
                                   INDEX_ID_ANY, // entity id
                                   NULL,  // jobUUID
                                   NULL,  // indexIds
                                   0,  // indexIdCount
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // name
                                   INDEX_STORAGE_SORT_MODE_NONE,
                                   DATABASE_ORDERING_NONE,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE,"init list storage fail: %s",Error_getText(error));
      String_delete(printableStorageName);
      String_delete(storageName);
      Storage_doneSpecifier(&storageSpecifier);
      return;
    }
    while (   !isCommandAborted(clientInfo,id)
           && Index_getNextStorage(&indexQueryHandle,
                                   NULL,  // uuidId
                                   NULL,  // jobUUID
                                   NULL,  // scheduleUUID
                                   NULL,  // entityId
                                   NULL,  // archiveType
                                   &storageId,
                                   storageName,
                                   NULL,  // createdDateTime
                                   NULL,  // totalEntryCount
                                   NULL,  // totalEntrySize
                                   &indexState,
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL  // errorMessage
                                  )
          )
    {
      if (stateAny || (state == indexState))
      {
        // get printable name (if possible)
        error = Storage_parseName(&storageSpecifier,storageName);
        if (error == ERROR_NONE)
        {
          String_set(printableStorageName,Storage_getPrintableName(&storageSpecifier,NULL));
        }
        else
        {
          String_set(printableStorageName,storageName);
        }

        // delete index
        error = Index_deleteStorage(indexHandle,storageId);
        if (error == ERROR_NONE)
        {
          sendClientResult(clientInfo,id,FALSE,ERROR_NONE,
                           "storageId=%llu name=%'S",
                           storageId,
                           printableStorageName
                          );
        }
        else
        {
          sendClientResult(clientInfo,id,TRUE,error,"%s",Error_getText(error));
          Index_doneList(&indexQueryHandle);
          return;
        }
      }
    }
    Index_doneList(&indexQueryHandle);

    // free resources
    String_delete(printableStorageName);
    String_delete(storageName);
    Storage_doneSpecifier(&storageSpecifier);
  }

  if (uuidId != INDEX_ID_NONE)
  {
    // delete UUID index
    error = Index_deleteUUID(indexHandle,uuidId);
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,error,"%s",Error_getText(error));
      return;
    }
  }

  if (entityId != INDEX_ID_NONE)
  {
    // delete entity index
    error = Index_deleteEntity(indexHandle,entityId);
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,error,"%s",Error_getText(error));
      return;
    }
  }

  if (storageId != INDEX_ID_NONE)
  {
    // delete storage index
    error = Index_deleteStorage(indexHandle,storageId);
    if (error != ERROR_NONE)
    {
      sendClientResult(clientInfo,id,TRUE,error,"%s",Error_getText(error));
      return;
    }
  }

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");

  // free resources
}

/***********************************************************************\
* Name   : serverCommand_indexStoragesInfo
* Purpose: get index database storage info
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            entityId=<id>|0|*
*            name=<text>
*            indexStateSet=<state set>|*
*            indexModeSet=<mode set>|*
*          Result:
*            storageCount=<n> \
*            totalEntryCount=<n> \
*            totalEntrySize=<n> [bytes]
*            totalEntryContentSize=<n> [bytes]
\***********************************************************************/

LOCAL void serverCommand_indexStoragesInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  uint64        n;
  IndexId       entityId;
  String        name;
  bool          indexStateAny;
  IndexStateSet indexStateSet;
  bool          indexModeAny;
  IndexModeSet  indexModeSet;
  Errors        error;
  ulong         storageCount,totalEntryCount;
  uint64        totalEntrySize,totalEntryContentSize;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  UNUSED_VARIABLE(argumentMap);

  // get entryId, name, index state set, index mode set
  if      (stringEquals(StringMap_getTextCString(argumentMap,"entityId","*"),"*"))
  {
    entityId = INDEX_ID_ANY;
  }
  else if (stringEquals(StringMap_getTextCString(argumentMap,"entityId","NONE"),"NONE"))
  {
    entityId = INDEX_ID_ENTITY_NONE;
  }
  else if (StringMap_getUInt64(argumentMap,"entityId",&n,INDEX_ID_ANY))
  {
    entityId = (IndexId)n;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected entityId=<id>");
    return;
  }
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<text>");
    String_delete(name);
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexStateSet","*"),"*"))
  {
    indexStateAny = TRUE;
  }
  else if (StringMap_getEnumSet(argumentMap,"indexStateSet",&indexStateSet,(StringMapParseEnumFunction)Index_parseState,INDEX_STATE_SET_ALL,"|",INDEX_STATE_NONE))
  {
    indexStateAny = FALSE;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexState=OK|CREATE|UPDATE_REQUESTED|UPDATE|ERROR|*");
    String_delete(name);
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexModeSet","*"),"*"))
  {
    indexModeAny = TRUE;
  }
  if (StringMap_getEnumSet(argumentMap,"indexModeSet",&indexModeSet,(StringMapParseEnumFunction)Index_parseMode,INDEX_MODE_SET_ALL,"|",INDEX_MODE_UNKNOWN))
  {
    indexModeAny = FALSE;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexModeSet=MANUAL|AUTO|*");
    String_delete(name);
    return;
  }

  // check if index database is available
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  // get index info
  error = Index_getStoragesInfo(indexHandle,
                                INDEX_ID_ANY,  // uuidId
                                entityId,
                                NULL,  // jobUUID
                                NULL,  // indexIds,
                                0,  // indexIdCount,
                                indexStateAny ? INDEX_STATE_SET_ALL : indexStateSet,
                                indexModeAny ? INDEX_MODE_SET_ALL : indexModeSet,
                                name,
                                &storageCount,
                                &totalEntryCount,
                                &totalEntrySize,
                                &totalEntryContentSize
                              );
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,error,"get storages info from index database fail: %s",Error_getText(error));
    String_delete(name);
    return;
  }

  // send data
  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"storageCount=%lu totalEntryCount=%lu totalEntrySize=%llu totalEntryContentSize=%llu",storageCount,totalEntryCount,totalEntrySize,totalEntryContentSize);

  // free resources
  String_delete(name);
}

/***********************************************************************\
* Name   : serverCommand_indexEntriesInfo
* Purpose: get index database entries info
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments
*          argumentCount - command arguments count
* Output : -
* Return : -
* Notes  : Arguments:
*            name=<text>
*            indexType=<type>|*
*            newestOnly=yes|no
*          Result:
*            totalEntryCount=<n>
*            totalEntrySize=<n [bytes]>
\***********************************************************************/

LOCAL void serverCommand_indexEntriesInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  String     name;
  bool       indexTypeAny;
  IndexTypes indexType;
  bool       newestOnly;
  Errors     error;
  ulong      totalEntryCount;
  uint64     totalEntrySize,totalEntryContentSize;

  assert(clientInfo != NULL);
  assert(argumentMap != NULL);

  // get entry pattern, index type, new entries only
  name = String_new();
  if (!StringMap_getString(argumentMap,"name",name,NULL))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected name=<text>");
    String_delete(name);
    return;
  }
  if      (stringEquals(StringMap_getTextCString(argumentMap,"indexType","*"),"*"))
  {
    indexTypeAny = TRUE;
  }
  else if (StringMap_getEnum(argumentMap,"indexType",&indexType,(StringMapParseEnumFunction)Index_parseType,INDEX_TYPE_FILE))
  {
    indexTypeAny = FALSE;
  }
  else
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected indexType=FILE|IMAGE|DIRECTORY|LINK|HARDLINK|SPECIAL|*");
    String_delete(name);
    return;
  }
  if (!StringMap_getBool(argumentMap,"newestOnly",&newestOnly,FALSE))
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_EXPECTED_PARAMETER,"expected newestOnly=yes|no");
    String_delete(name);
    return;
  }

  // check if index database is available, check if index database is ready
  if (indexHandle == NULL)
  {
    sendClientResult(clientInfo,id,TRUE,ERROR_DATABASE_INDEX_NOT_FOUND,"no index database available");
    String_delete(name);
    return;
  }

  // get index info
  error = Index_getEntriesInfo(indexHandle,
                               Array_cArray(&clientInfo->indexIdArray),
                               Array_length(&clientInfo->indexIdArray),
                               NULL,  // entryIds
                               0,  // entryIdCount
                               indexTypeAny ? INDEX_TYPE_SET_ANY_ENTRY : SET_VALUE(indexType),
                               name,
                               newestOnly,
                               &totalEntryCount,
                               &totalEntrySize,
                               &totalEntryContentSize
                              );
  if (error != ERROR_NONE)
  {
    sendClientResult(clientInfo,id,TRUE,error,"get entries info index database fail: %s",Error_getText(error));
    String_delete(name);
    return;
  }

  // send data
  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"totalEntryCount=%lu totalEntrySize=%llu totalEntryContentSize=%llu",totalEntryCount,totalEntrySize,totalEntryContentSize);

  // free resources
  String_delete(name);
}

#ifndef NDEBUG
/***********************************************************************\
* Name   : serverCommand_debugPrintStatistics
* Purpose: print array/string/file statistics info
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments (not used)
*          argumentCount - command arguments count (not used)
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            <error text>
\***********************************************************************/

LOCAL void serverCommand_debugPrintStatistics(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  Array_debugPrintStatistics();
  String_debugPrintStatistics();
  File_debugPrintStatistics();

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_debugPrintMemoryInfo
* Purpose: print array/string debug info
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments (not used)
*          argumentCount - command arguments count (not used)
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            <error text>
*          Result:
*            <error text>
\***********************************************************************/

LOCAL void serverCommand_debugPrintMemoryInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  assert(clientInfo != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  Array_debugPrintInfo(CALLBACK_INLINE(bool,(const Array *array, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
                                       {
                                         UNUSED_VARIABLE(array);
                                         UNUSED_VARIABLE(fileName);
                                         UNUSED_VARIABLE(lineNb);
                                         UNUSED_VARIABLE(userData);

                                         sendClientResult(clientInfo,id,FALSE,ERROR_NONE,"type=ARRAY n=%lu count=%lu",n,count);

                                         return TRUE;
                                       },
                                       NULL
                                      )
                      );
  String_debugPrintInfo(CALLBACK_INLINE(bool,(ConstString string, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
                                        {
                                          UNUSED_VARIABLE(string);
                                          UNUSED_VARIABLE(fileName);
                                          UNUSED_VARIABLE(lineNb);
                                          UNUSED_VARIABLE(userData);

                                          sendClientResult(clientInfo,id,FALSE,ERROR_NONE,"type=STRING n=%lu count=%lu",n,count);

                                          return TRUE;
                                        },
                                        NULL
                                       )
                       );
  File_debugPrintInfo(CALLBACK_INLINE(bool,(const FileHandle *fileHandle, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
                                      {
                                        UNUSED_VARIABLE(fileHandle);
                                        UNUSED_VARIABLE(fileName);
                                        UNUSED_VARIABLE(lineNb);
                                        UNUSED_VARIABLE(userData);

                                        sendClientResult(clientInfo,id,FALSE,ERROR_NONE,"type=FILE n=%lu count=%lu",n,count);

                                        return TRUE;
                                      },
                                      NULL
                                     )
                     );

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}

/***********************************************************************\
* Name   : serverCommand_debugDumpMemoryInfo
* Purpose: dump array/string/file debug info into file "bar-memory.dump"
* Input  : clientInfo    - client info
*          indexHandle   - index handle
*          id            - command id
*          arguments     - command arguments (not used)
*          argumentCount - command arguments count (not used)
* Output : -
* Return : -
* Notes  : Arguments:
*          Result:
*            <error text>
\***********************************************************************/

LOCAL void serverCommand_debugDumpMemoryInfo(ClientInfo *clientInfo, IndexHandle *indexHandle, uint id, const StringMap argumentMap)
{
  FILE *handle;

  assert(clientInfo != NULL);

  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(argumentMap);

  handle = fopen("bar-memory.dump","w");
  if (handle == NULL)
  {
    sendClientResult(clientInfo,id,FALSE,ERROR_CREATE_FILE,"Cannot create 'bar-memory.dump'");
    return;
  }

  // Note: no abort because debug functions may hold a lock while dumping information
  Array_debugDumpInfo(handle,
                      CALLBACK_INLINE(bool,(const Array *array, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
                                      {
                                        UNUSED_VARIABLE(array);
                                        UNUSED_VARIABLE(fileName);
                                        UNUSED_VARIABLE(lineNb);
                                        UNUSED_VARIABLE(userData);

                                        sendClientResult(clientInfo,id,FALSE,ERROR_NONE,"type=ARRAY n=%lu count=%lu",n,count);

                                        return TRUE;
                                      },
                                      NULL
                                     )
                     );
  String_debugDumpInfo(handle,
                       CALLBACK_INLINE(bool,(ConstString string, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
                                       {
                                         UNUSED_VARIABLE(string);
                                         UNUSED_VARIABLE(fileName);
                                         UNUSED_VARIABLE(lineNb);
                                         UNUSED_VARIABLE(userData);

                                         sendClientResult(clientInfo,id,FALSE,ERROR_NONE,"type=STRING n=%lu count=%lu",n,count);

                                         return TRUE;
                                       },
                                       NULL
                                      )
                     );
  File_debugDumpInfo(handle,
                     CALLBACK_INLINE(bool,(const FileHandle *fileHandle, const char *fileName, ulong lineNb, ulong n, ulong count, void *userData),
                                     {
                                       UNUSED_VARIABLE(fileHandle);
                                       UNUSED_VARIABLE(fileName);
                                       UNUSED_VARIABLE(lineNb);
                                       UNUSED_VARIABLE(userData);

                                       sendClientResult(clientInfo,id,FALSE,ERROR_NONE,"type=FILE n=%lu count=%lu",n,count);

                                       return TRUE;
                                     },
                                     NULL
                                    )
                    );
  fclose(handle);

  sendClientResult(clientInfo,id,TRUE,ERROR_NONE,"");
}
#endif /* NDEBUG */

// server commands
const struct
{
  const char            *name;
  ServerCommandFunction serverCommandFunction;
  AuthorizationStates   authorizationState;
}
SERVER_COMMANDS[] =
{
  { "ERROR_INFO",                  serverCommand_errorInfo,                AUTHORIZATION_STATE_OK      },
  { "START_SSL",                   serverCommand_startSSL,                 AUTHORIZATION_STATE_WAITING },
  { "AUTHORIZE",                   serverCommand_authorize,                AUTHORIZATION_STATE_WAITING },
  { "VERSION",                     serverCommand_version,                  AUTHORIZATION_STATE_OK      },
  { "QUIT",                        serverCommand_quit,                     AUTHORIZATION_STATE_OK      },
  { "ACTION_RESULT",               serverCommand_actionResult,             AUTHORIZATION_STATE_OK      },
  { "SERVER_OPTION_GET",           serverCommand_serverOptionGet,          AUTHORIZATION_STATE_OK      },
  { "SERVER_OPTION_SET",           serverCommand_serverOptionSet,          AUTHORIZATION_STATE_OK      },
  { "SERVER_OPTION_FLUSH",         serverCommand_serverOptionFlush,        AUTHORIZATION_STATE_OK      },
  { "SERVER_LIST",                 serverCommand_serverList,               AUTHORIZATION_STATE_OK      },
  { "SERVER_LIST_ADD",             serverCommand_serverListAdd,            AUTHORIZATION_STATE_OK      },
  { "SERVER_LIST_UPDATE",          serverCommand_serverListUpdate,         AUTHORIZATION_STATE_OK      },
  { "SERVER_LIST_REMOVE",          serverCommand_serverListRemove,         AUTHORIZATION_STATE_OK      },
//TODO: obsolete?
  { "GET",                         serverCommand_get,                      AUTHORIZATION_STATE_OK      },
  { "ABORT",                       serverCommand_abort,                    AUTHORIZATION_STATE_OK      },
  { "STATUS",                      serverCommand_status,                   AUTHORIZATION_STATE_OK      },
  { "PAUSE",                       serverCommand_pause,                    AUTHORIZATION_STATE_OK      },
  { "SUSPEND",                     serverCommand_suspend,                  AUTHORIZATION_STATE_OK      },
  { "CONTINUE",                    serverCommand_continue,                 AUTHORIZATION_STATE_OK      },
  { "DEVICE_LIST",                 serverCommand_deviceList,               AUTHORIZATION_STATE_OK      },
  { "ROOT_LIST",                   serverCommand_rootList,                 AUTHORIZATION_STATE_OK      },
  { "FILE_INFO",                   serverCommand_fileInfo,                 AUTHORIZATION_STATE_OK      },
  { "FILE_LIST",                   serverCommand_fileList,                 AUTHORIZATION_STATE_OK      },
  { "FILE_ATTRIBUTE_GET",          serverCommand_fileAttributeGet,         AUTHORIZATION_STATE_OK      },
  { "FILE_ATTRIBUTE_SET",          serverCommand_fileAttributeSet,         AUTHORIZATION_STATE_OK      },
  { "FILE_ATTRIBUTE_CLEAR",        serverCommand_fileAttributeClear,       AUTHORIZATION_STATE_OK      },
  { "DIRECTORY_INFO",              serverCommand_directoryInfo,            AUTHORIZATION_STATE_OK      },
  { "TEST_SCRIPT",                 serverCommand_testScript,               AUTHORIZATION_STATE_OK      },
  { "JOB_LIST",                    serverCommand_jobList,                  AUTHORIZATION_STATE_OK      },
  { "JOB_INFO",                    serverCommand_jobInfo,                  AUTHORIZATION_STATE_OK      },
  { "JOB_NEW",                     serverCommand_jobNew,                   AUTHORIZATION_STATE_OK      },
  { "JOB_CLONE",                   serverCommand_jobClone,                 AUTHORIZATION_STATE_OK      },
  { "JOB_RENAME",                  serverCommand_jobRename,                AUTHORIZATION_STATE_OK      },
  { "JOB_DELETE",                  serverCommand_jobDelete,                AUTHORIZATION_STATE_OK      },
  { "JOB_START",                   serverCommand_jobStart,                 AUTHORIZATION_STATE_OK      },
  { "JOB_ABORT",                   serverCommand_jobAbort,                 AUTHORIZATION_STATE_OK      },
  { "JOB_FLUSH",                   serverCommand_jobFlush,                 AUTHORIZATION_STATE_OK      },
  { "JOB_OPTION_GET",              serverCommand_jobOptionGet,             AUTHORIZATION_STATE_OK      },
  { "JOB_OPTION_SET",              serverCommand_jobOptionSet,             AUTHORIZATION_STATE_OK      },
  { "JOB_OPTION_DELETE",           serverCommand_jobOptionDelete,          AUTHORIZATION_STATE_OK      },
  { "JOB_STATUS",                  serverCommand_jobStatus,                AUTHORIZATION_STATE_OK      },
  { "INCLUDE_LIST",                serverCommand_includeList,              AUTHORIZATION_STATE_OK      },
//TODO remove
  { "INCLUDE_LIST_CLEAR",          serverCommand_includeListClear,         AUTHORIZATION_STATE_OK      },
  { "INCLUDE_LIST_ADD",            serverCommand_includeListAdd,           AUTHORIZATION_STATE_OK      },
  { "INCLUDE_LIST_UPDATE",         serverCommand_includeListUpdate,        AUTHORIZATION_STATE_OK      },
  { "INCLUDE_LIST_REMOVE",         serverCommand_includeListRemove,        AUTHORIZATION_STATE_OK      },
  { "MOUNT_LIST",                  serverCommand_mountList,                AUTHORIZATION_STATE_OK      },
  { "MOUNT_LIST_ADD",              serverCommand_mountListAdd,             AUTHORIZATION_STATE_OK      },
  { "MOUNT_LIST_UPDATE",           serverCommand_mountListUpdate,          AUTHORIZATION_STATE_OK      },
  { "MOUNT_LIST_REMOVE",           serverCommand_mountListRemove,          AUTHORIZATION_STATE_OK      },
 { "EXCLUDE_LIST",                 serverCommand_excludeList,              AUTHORIZATION_STATE_OK      },
//TODO remove
  { "EXCLUDE_LIST_CLEAR",          serverCommand_excludeListClear,         AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_LIST_ADD",            serverCommand_excludeListAdd,           AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_LIST_UPDATE",         serverCommand_excludeListUpdate,        AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_LIST_REMOVE",         serverCommand_excludeListRemove,        AUTHORIZATION_STATE_OK      },
  { "SOURCE_LIST",                 serverCommand_sourceList,               AUTHORIZATION_STATE_OK      },
//TODO remove
  { "SOURCE_LIST_CLEAR",           serverCommand_sourceListClear,          AUTHORIZATION_STATE_OK      },
  { "SOURCE_LIST_ADD",             serverCommand_sourceListAdd,            AUTHORIZATION_STATE_OK      },
  { "SOURCE_LIST_UPDATE",          serverCommand_sourceListUpdate,         AUTHORIZATION_STATE_OK      },
  { "SOURCE_LIST_REMOVE",          serverCommand_sourceListRemove,         AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_COMPRESS_LIST",       serverCommand_excludeCompressList,      AUTHORIZATION_STATE_OK      },
//TODO remove
  { "EXCLUDE_COMPRESS_LIST_CLEAR", serverCommand_excludeCompressListClear, AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_COMPRESS_LIST_ADD",   serverCommand_excludeCompressListAdd,   AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_COMPRESS_LIST_UPDATE",serverCommand_excludeCompressListUpdate,AUTHORIZATION_STATE_OK      },
  { "EXCLUDE_COMPRESS_LIST_REMOVE",serverCommand_excludeCompressListRemove,AUTHORIZATION_STATE_OK      },
  { "SCHEDULE_LIST",               serverCommand_scheduleList,             AUTHORIZATION_STATE_OK      },
  { "SCHEDULE_ADD",                serverCommand_scheduleAdd,              AUTHORIZATION_STATE_OK      },
  { "SCHEDULE_REMOVE",             serverCommand_scheduleRemove,           AUTHORIZATION_STATE_OK      },
  { "SCHEDULE_OPTION_GET",         serverCommand_scheduleOptionGet,        AUTHORIZATION_STATE_OK      },
  { "SCHEDULE_OPTION_SET",         serverCommand_scheduleOptionSet,        AUTHORIZATION_STATE_OK      },
  { "SCHEDULE_OPTION_DELETE",      serverCommand_scheduleOptionDelete,     AUTHORIZATION_STATE_OK      },
  { "SCHEDULE_TRIGGER",            serverCommand_scheduleTrigger,          AUTHORIZATION_STATE_OK      },
  { "DECRYPT_PASSWORD_CLEAR",      serverCommand_decryptPasswordsClear,    AUTHORIZATION_STATE_OK      },
  { "DECRYPT_PASSWORD_ADD",        serverCommand_decryptPasswordAdd,       AUTHORIZATION_STATE_OK      },
  { "FTP_PASSWORD",                serverCommand_ftpPassword,              AUTHORIZATION_STATE_OK      },
  { "SSH_PASSWORD",                serverCommand_sshPassword,              AUTHORIZATION_STATE_OK      },
  { "WEBDAV_PASSWORD",             serverCommand_webdavPassword,           AUTHORIZATION_STATE_OK      },
  { "CRYPT_PASSWORD",              serverCommand_cryptPassword,            AUTHORIZATION_STATE_OK      },
  { "PASSWORDS_CLEAR",             serverCommand_passwordsClear,           AUTHORIZATION_STATE_OK      },
  { "VOLUME_LOAD",                 serverCommand_volumeLoad,               AUTHORIZATION_STATE_OK      },
  { "VOLUME_UNLOAD",               serverCommand_volumeUnload,             AUTHORIZATION_STATE_OK      },

  { "ARCHIVE_LIST",                serverCommand_archiveList,              AUTHORIZATION_STATE_OK      },

//TODO: replace by single list?
  { "STORAGE_LIST",                serverCommand_storageList,              AUTHORIZATION_STATE_OK      },
  { "STORAGE_LIST_CLEAR",          serverCommand_storageListClear,         AUTHORIZATION_STATE_OK      },
  { "STORAGE_LIST_ADD",            serverCommand_storageListAdd,           AUTHORIZATION_STATE_OK      },
  { "STORAGE_LIST_REMOVE",         serverCommand_storageListRemove,        AUTHORIZATION_STATE_OK      },
  { "STORAGE_LIST_INFO",           serverCommand_storageListInfo,          AUTHORIZATION_STATE_OK      },
  { "ENTRY_LIST",                  serverCommand_entryList,                AUTHORIZATION_STATE_OK      },
  { "ENTRY_LIST_CLEAR",            serverCommand_entryListClear,           AUTHORIZATION_STATE_OK      },
  { "ENTRY_LIST_ADD",              serverCommand_entryListAdd,             AUTHORIZATION_STATE_OK      },
  { "ENTRY_LIST_REMOVE",           serverCommand_entryListRemove,             AUTHORIZATION_STATE_OK      },
  { "ENTRY_LIST_INFO",             serverCommand_entryListInfo,            AUTHORIZATION_STATE_OK      },

  { "STORAGE_DELETE",              serverCommand_storageDelete,            AUTHORIZATION_STATE_OK      },

  { "RESTORE",                     serverCommand_restore,                  AUTHORIZATION_STATE_OK      },
  { "RESTORE_CONTINUE",            serverCommand_restoreContinue,          AUTHORIZATION_STATE_OK      },

  { "INDEX_UUID_LIST",             serverCommand_indexUUIDList,            AUTHORIZATION_STATE_OK      },
  { "INDEX_ENTITY_LIST",           serverCommand_indexEntityList,          AUTHORIZATION_STATE_OK      },
  { "INDEX_STORAGE_LIST",          serverCommand_indexStorageList,         AUTHORIZATION_STATE_OK      },
  { "INDEX_ENTRY_LIST",            serverCommand_indexEntryList,           AUTHORIZATION_STATE_OK      },

  { "INDEX_ENTITY_ADD",            serverCommand_indexEntityAdd,           AUTHORIZATION_STATE_OK      },
  { "INDEX_STORAGE_ADD",           serverCommand_indexStorageAdd,          AUTHORIZATION_STATE_OK      },

//TODO: obsolete
//  { "INDEX_ENTITY_SET",            serverCommand_indexEntitySet,           AUTHORIZATION_STATE_OK      },
  { "INDEX_ASSIGN",                serverCommand_indexAssign,              AUTHORIZATION_STATE_OK      },
  { "INDEX_REFRESH",               serverCommand_indexRefresh,             AUTHORIZATION_STATE_OK      },
  { "INDEX_REMOVE",                serverCommand_indexRemove,              AUTHORIZATION_STATE_OK      },

  { "INDEX_STORAGES_INFO",         serverCommand_indexStoragesInfo,        AUTHORIZATION_STATE_OK      },
  { "INDEX_ENTRIES_INFO",          serverCommand_indexEntriesInfo,         AUTHORIZATION_STATE_OK      },

  // obsolete
  { "OPTION_GET",                  serverCommand_jobOptionGet,             AUTHORIZATION_STATE_OK      },
  { "OPTION_SET",                  serverCommand_jobOptionSet,             AUTHORIZATION_STATE_OK      },
  { "OPTION_DELETE",               serverCommand_jobOptionDelete,          AUTHORIZATION_STATE_OK      },

  #ifndef NDEBUG
  { "DEBUG_PRINT_STATISTICS",      serverCommand_debugPrintStatistics,     AUTHORIZATION_STATE_OK      },
  { "DEBUG_PRINT_MEMORY_INFO",     serverCommand_debugPrintMemoryInfo,     AUTHORIZATION_STATE_OK      },
  { "DEBUG_DUMP_MEMORY_INFO",      serverCommand_debugDumpMemoryInfo,      AUTHORIZATION_STATE_OK      },
  #endif /* NDEBUG */
};

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : freeCommandMsg
* Purpose: free command msg
* Input  : commandMsg - command message
*          userData   - user data (ignored)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeCommandMsg(CommandMsg *commandMsg, void *userData)
{
  assert(commandMsg != NULL);

  UNUSED_VARIABLE(userData);

  StringMap_delete(commandMsg->argumentMap);
}

/***********************************************************************\
* Name   : parseCommand
* Purpose: parse command
* Input  : string - command
* Output : commandMsg - command message
* Return : TRUE if no error, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool parseCommand(CommandMsg  *commandMsg,
                        ConstString string
                       )
{
  String command;
  String arguments;
  uint   z;

  assert(commandMsg != NULL);

  // initialize variables
  commandMsg->serverCommandFunction = NULL;
  commandMsg->authorizationState    = AUTHORIZATION_STATE_WAITING;
  commandMsg->id                    = 0;
  commandMsg->argumentMap           = NULL;
  command   = String_new();
  arguments = String_new();

  // parse
  if (!String_parse(string,STRING_BEGIN,"%d %S % S",NULL,&commandMsg->id,command,arguments))
  {
    String_delete(arguments);
    String_delete(command);
    return FALSE;
  }

  // find command
  z = 0;
  while ((z < SIZE_OF_ARRAY(SERVER_COMMANDS)) && !String_equalsCString(command,SERVER_COMMANDS[z].name))
  {
    z++;
  }
  if (z >= SIZE_OF_ARRAY(SERVER_COMMANDS))
  {
    String_delete(arguments);
    String_delete(command);
    return FALSE;
  }
  commandMsg->serverCommandFunction = SERVER_COMMANDS[z].serverCommandFunction;
  commandMsg->authorizationState    = SERVER_COMMANDS[z].authorizationState;

  // parse arguments
  commandMsg->argumentMap = StringMap_new();
  if (commandMsg->argumentMap == NULL)
  {
    String_delete(arguments);
    String_delete(command);
    return FALSE;
  }
  if (!StringMap_parse(commandMsg->argumentMap,arguments,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,STRING_BEGIN,NULL))
  {
    String_delete(arguments);
    String_delete(command);
    StringMap_delete(commandMsg->argumentMap);
    return FALSE;
  }

  // free resources
  String_delete(arguments);
  String_delete(command);

  return TRUE;
}

/***********************************************************************\
* Name   : networkClientThreadCode
* Purpose: processing thread for network clients
* Input  : clientInfo - client info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void networkClientThreadCode(ClientInfo *clientInfo)
{
  String      result;
  CommandMsg  commandMsg;
  #ifdef SERVER_DEBUG
    uint64 t0,t1;
  #endif /* SERVER_DEBUG */

  assert(clientInfo != NULL);

  // init variables
  result = String_new();

  while (   !clientInfo->quitFlag
         && MsgQueue_get(&clientInfo->network.commandMsgQueue,&commandMsg,NULL,sizeof(commandMsg),WAIT_FOREVER)
        )
  {
    // check authorization (if not in server debug mode)
    if (globalOptions.serverDebugFlag || (commandMsg.authorizationState == clientInfo->authorizationState))
    {
      // execute command
      #ifdef SERVER_DEBUG
        t0 = Misc_getTimestamp();
      #endif /* SERVER_DEBUG */
      commandMsg.serverCommandFunction(clientInfo,
                                       indexHandle,
                                       commandMsg.id,
                                       commandMsg.argumentMap
                                      );
      #ifdef SERVER_DEBUG
        t1 = Misc_getTimestamp();
        fprintf(stderr,"DEBUG: time=%llums\n",(t1-t0)/1000LL);
      #endif /* SERVER_DEBUG */
    }
    else
    {
      // authorization failure -> mark for disconnect
      clientInfo->authorizationState = AUTHORIZATION_STATE_FAIL;
      sendClientResult(clientInfo,commandMsg.id,TRUE,ERROR_AUTHORIZATION,"authorization failure");
    }

    // free resources
    freeCommandMsg(&commandMsg,NULL);
  }

  // free resources
  String_delete(result);
}

/***********************************************************************\
* Name   : initSession
* Purpose: init new session
* Input  : clientInfo - client info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initSession(ClientInfo *clientInfo)
{
  assert(clientInfo != NULL);

  #ifndef NO_SESSION_ID
    Crypt_randomize(clientInfo->sessionId,sizeof(SessionId));
  #else /* not NO_SESSION_ID */
    memset(clientInfo->sessionId,0,sizeof(SessionId));
  #endif /* NO_SESSION_ID */
  (void)Crypt_createKeys(&clientInfo->publicKey,&clientInfo->secretKey,SESSION_KEY_SIZE,CRYPT_PADDING_TYPE_PKCS1);
}

/***********************************************************************\
* Name   : doneSession
* Purpose: done session
* Input  : clientInfo - client info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneSession(ClientInfo *clientInfo)
{
  assert(clientInfo != NULL);

  Crypt_doneKey(&clientInfo->publicKey);
  Crypt_doneKey(&clientInfo->secretKey);
}

/***********************************************************************\
* Name   : sendSessionId
* Purpose: send session id to client
* Input  : clientInfo - client info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void sendSessionId(ClientInfo *clientInfo)
{
  String id;
  String n,e;
  String s;

  assert(clientInfo != NULL);

  // format session data
  id = encodeHex(String_new(),clientInfo->sessionId,sizeof(SessionId));
  n  = Crypt_getKeyModulus(&clientInfo->publicKey);
  e  = Crypt_getKeyExponent(&clientInfo->publicKey);
  if ((n !=NULL) && (e != NULL))
  {
    s  = String_format(String_new(),
                       "SESSION id=%S encryptTypes=%s n=%S e=%S",
                       id,
                       "RSA,NONE",
                       n,
                       e
                      );
  }
  else
  {
    s  = String_format(String_new(),
                       "SESSION id=%S encryptTypes=%s",
                       id,
                       "NONE"
                      );
  }
  String_appendChar(s,'\n');

  // send session data to client
  sendClient(clientInfo,s);

  // free resources
  String_delete(s);
  String_delete(e);
  String_delete(n);
  String_delete(id);
}

/***********************************************************************\
* Name   : initClient
* Purpose: initialize client
* Input  : clientInfo - client info to initialize
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initClient(ClientInfo *clientInfo)
{
  assert(clientInfo != NULL);

  // initialize
  clientInfo->type                  = CLIENT_TYPE_NONE;
  clientInfo->authorizationState    = AUTHORIZATION_STATE_WAITING;
  clientInfo->authorizationFailNode = NULL;
  clientInfo->quitFlag              = FALSE;
  clientInfo->action.error          = ERROR_NONE;
  clientInfo->action.resultMap      = StringMap_new();
  if (clientInfo->action.resultMap == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  clientInfo->abortCommandId        = 0;
clientInfo->abortFlag = FALSE;

  Semaphore_init(&clientInfo->action.lock);
  EntryList_init(&clientInfo->includeEntryList);
  PatternList_init(&clientInfo->excludePatternList);
  PatternList_init(&clientInfo->compressExcludePatternList);
  DeltaSourceList_init(&clientInfo->deltaSourceList);
  initJobOptions(&clientInfo->jobOptions);
  List_init(&clientInfo->directoryInfoList);
  Array_init(&clientInfo->indexIdArray,sizeof(IndexId),64,CALLBACK_NULL,CALLBACK_NULL);
  Array_init(&clientInfo->entryIdArray,sizeof(IndexId),64,CALLBACK_NULL,CALLBACK_NULL);
}

/***********************************************************************\
* Name   : initBatchClient
* Purpose: create batch client with file i/o
* Input  : clientInfo - client info to initialize
*          fileHandle - client file handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initBatchClient(ClientInfo *clientInfo,
                           FileHandle *fileHandle
                          )
{
  assert(clientInfo != NULL);

  clientInfo->type               = CLIENT_TYPE_BATCH;
  clientInfo->authorizationState = AUTHORIZATION_STATE_OK;
  clientInfo->file.fileHandle    = fileHandle;
}

/***********************************************************************\
* Name   : initNetworkClient
* Purpose: init new client with network i/o
* Input  : clientInfo   - client info to initialize
*          name         - client name
*          port         - client port
*          socketHandle - client socket handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initNetworkClient(ClientInfo   *clientInfo,
                             ConstString  name,
                             uint         port,
                             SocketHandle socketHandle
                            )
{
  uint z;

  assert(clientInfo != NULL);

  // initialize
  clientInfo->type                 = CLIENT_TYPE_NETWORK;
  clientInfo->network.name         = String_duplicate(name);
  clientInfo->network.port         = port;
  clientInfo->network.socketHandle = socketHandle;

  if (!MsgQueue_init(&clientInfo->network.commandMsgQueue,0))
  {
    HALT_FATAL_ERROR("Cannot initialize client command message queue!");
  }
  Semaphore_init(&clientInfo->network.writeLock);
  for (z = 0; z < MAX_NETWORK_CLIENT_THREADS; z++)
  {
    if (!Thread_init(&clientInfo->network.threads[z],"Client",0,networkClientThreadCode,clientInfo))
    {
      HALT_FATAL_ERROR("Cannot initialize client thread!");
    }
  }

  // init and send session id
  initSession(clientInfo);
  sendSessionId(clientInfo);
}

/***********************************************************************\
* Name   : doneClient
* Purpose: deinitialize client
* Input  : clientInfo - client info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneClient(ClientInfo *clientInfo)
{
  int z;

  assert(clientInfo != NULL);

  clientInfo->quitFlag = TRUE;
  switch (clientInfo->type)
  {
    case CLIENT_TYPE_NONE:
      break;
    case CLIENT_TYPE_BATCH:
      break;
    case CLIENT_TYPE_NETWORK:
      // stop client threads
      Semaphore_setEnd(&clientInfo->action.lock);
      MsgQueue_setEndOfMsg(&clientInfo->network.commandMsgQueue);
      for (z = MAX_NETWORK_CLIENT_THREADS-1; z >= 0; z--)
      {
        if (!Thread_join(&clientInfo->network.threads[z]))
        {
          HALT_INTERNAL_ERROR("Cannot stop client thread!");
        }
      }

      // free resources
      doneSession(clientInfo);
      for (z = MAX_NETWORK_CLIENT_THREADS-1; z >= 0; z--)
      {
        Thread_done(&clientInfo->network.threads[z]);
      }
      Semaphore_done(&clientInfo->network.writeLock);
      MsgQueue_done(&clientInfo->network.commandMsgQueue,CALLBACK((MsgQueueMsgFreeFunction)freeCommandMsg,NULL));
      String_delete(clientInfo->network.name);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  Array_done(&clientInfo->entryIdArray);
  Array_done(&clientInfo->indexIdArray);
  List_done(&clientInfo->directoryInfoList,CALLBACK((ListNodeFreeFunction)freeDirectoryInfoNode,NULL));
  doneJobOptions(&clientInfo->jobOptions);
  DeltaSourceList_done(&clientInfo->deltaSourceList);
  PatternList_done(&clientInfo->compressExcludePatternList);
  PatternList_done(&clientInfo->excludePatternList);
  EntryList_done(&clientInfo->includeEntryList);
  Semaphore_done(&clientInfo->action.lock);
  StringMap_delete(clientInfo->action.resultMap);
}

/***********************************************************************\
* Name   : freeClientNode
* Purpose: free client node
* Input  : clientNode - client node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeClientNode(ClientNode *clientNode, void *userData)
{
  assert(clientNode != NULL);

  UNUSED_VARIABLE(userData);

  doneClient(&clientNode->clientInfo);
  String_delete(clientNode->commandString);
}

/***********************************************************************\
* Name   : newClient
* Purpose: create new client
* Input  : -
* Output : -
* Return : client node
* Notes  : -
\***********************************************************************/

LOCAL ClientNode *newClient(void)
{
  ClientNode *clientNode;

  // create new client node
  clientNode = LIST_NEW_NODE(ClientNode);
  if (clientNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  clientNode->commandString = String_new();

  // initialize node
  initClient(&clientNode->clientInfo);

  return clientNode;
}

/***********************************************************************\
* Name   : newNetworkClient
* Purpose: create new network client
* Input  : -
* Output : -
* Return : client node
* Notes  : -
\***********************************************************************/

LOCAL ClientNode *newNetworkClient(ConstString  name,
                                   uint         port,
//TODO pointer?
                                   SocketHandle socketHandle
                                  )
{
  ClientNode *clientNode;

  clientNode = newClient();
  assert(clientNode != NULL);
  initNetworkClient(&clientNode->clientInfo,name,port,socketHandle);

  return clientNode;
}

/***********************************************************************\
* Name   : deleteClient
* Purpose: delete client
* Input  : clientNode - client node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deleteClient(ClientNode *clientNode)
{
  assert(clientNode != NULL);

  freeClientNode(clientNode,NULL);
  LIST_DELETE_NODE(clientNode);
}

/***********************************************************************\
* Name   : freeAuthorizationFailNode
* Purpose: free authorazation fail node
* Input  : authorizationFailNode - authorization fail node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeAuthorizationFailNode(AuthorizationFailNode *authorizationFailNode, void *userData)
{
  assert(authorizationFailNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(authorizationFailNode->clientName);
}

/***********************************************************************\
* Name   : getAuthorizationFailNode
* Purpose: new authorazation fail node
* Input  : clientName - client name
* Output : -
* Return : authorization fail node
* Notes  : -
\***********************************************************************/

LOCAL AuthorizationFailNode *newAuthorizationFailNode(ConstString clientName)
{
  AuthorizationFailNode *authorizationFailNode;

  authorizationFailNode = LIST_NEW_NODE(AuthorizationFailNode);
  if (authorizationFailNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  authorizationFailNode->clientName    = String_duplicate(clientName);
  authorizationFailNode->count         = 0;
  authorizationFailNode->lastTimestamp = Misc_getTimestamp();

  return authorizationFailNode;
}

#if 0
// still not used
/***********************************************************************\
* Name   : deleteAuthorizationFailNode
* Purpose: delete authorazation fail node
* Input  : authorizationFailNode - authorization fail node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deleteAuthorizationFailNode(AuthorizationFailNode *authorizationFailNode)
{
  assert(authorizationFailNode != NULL);

  freeAuthorizationFailNode(authorizationFailNode,NULL);
  LIST_DELETE_NODE(authorizationFailNode);
}
#endif /* 0 */

/***********************************************************************\
* Name   : processCommand
* Purpose: process client command
* Input  : clientNode - client node
*          commadn    - command to process
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void processCommand(ClientInfo *clientInfo, ConstString command)
{
  CommandMsg commandMsg;

  assert(clientInfo != NULL);

  #ifdef SERVER_DEBUG
    fprintf(stderr,"DEBUG: command=%s\n",String_cString(command));
  #endif // SERVER_DEBUG

  // parse command
  if (!parseCommand(&commandMsg,command))
  {
    sendClientResult(clientInfo,commandMsg.id,TRUE,ERROR_PARSING,"parse error");
    return;
  }

  switch (clientInfo->type)
  {
    case CLIENT_TYPE_BATCH:
      // check authorization (if not in server debug mode)
      if (globalOptions.serverDebugFlag || (commandMsg.authorizationState == clientInfo->authorizationState))
      {
        // execute
        commandMsg.serverCommandFunction(clientInfo,
                                         NULL,  // indexHandle,
                                         commandMsg.id,
                                         commandMsg.argumentMap
                                        );
      }
      else
      {
        // authorization failure -> mark for disconnect
        clientInfo->authorizationState = AUTHORIZATION_STATE_FAIL;
        sendClientResult(clientInfo,commandMsg.id,TRUE,ERROR_AUTHORIZATION,"authorization failure");
      }

      // free resources
      freeCommandMsg(&commandMsg,NULL);
      break;
    case CLIENT_TYPE_NETWORK:
      switch (clientInfo->authorizationState)
      {
        case AUTHORIZATION_STATE_WAITING:
          // check authorization (if not in server debug mode)
          if (globalOptions.serverDebugFlag || (commandMsg.authorizationState == AUTHORIZATION_STATE_WAITING))
          {
            // execute command
            commandMsg.serverCommandFunction(clientInfo,
                                             NULL,  // indexHandle,
                                             commandMsg.id,
                                             commandMsg.argumentMap
                                            );
          }
          else
          {
            // authorization failure -> mark for disconnect
            clientInfo->authorizationState = AUTHORIZATION_STATE_FAIL;
            sendClientResult(clientInfo,commandMsg.id,TRUE,ERROR_AUTHORIZATION,"authorization failure");
          }

          // free resources
          freeCommandMsg(&commandMsg,NULL);
          break;
        case AUTHORIZATION_STATE_OK:
          // send command to client thread for asynchronous processing
          (void)MsgQueue_put(&clientInfo->network.commandMsgQueue,&commandMsg,sizeof(commandMsg));
          break;
        case AUTHORIZATION_STATE_FAIL:
          break;
      }
      break;
    default:
      // free resources
      freeCommandMsg(&commandMsg,NULL);
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
}

/*---------------------------------------------------------------------*/

Errors Server_initAll(void)
{
  return ERROR_NONE;
}

void Server_doneAll(void)
{
}

Errors Server_run(uint              port,
                  uint              tlsPort,
                  const Certificate *ca,
                  const Certificate *cert,
                  const Key         *key,
                  const Password    *password,
                  uint              maxConnections,
                  const char        *jobsDirectory,
                  const char        *indexDatabaseFileName,
                  const JobOptions  *defaultJobOptions
                 )
{
  AutoFreeList          autoFreeList;
  Errors                error;
  bool                  serverFlag,serverTLSFlag;
  ServerSocketHandle    serverSocketHandle,serverTLSSocketHandle;
  sigset_t              signalMask;
  struct pollfd         *pollfds;
  uint                  maxPollfdCount;
  uint                  pollfdCount;
  uint                  pollServerSocketIndex,pollServerTLSSocketIndex;
  uint64                nowTimestamp,waitTimeout,nextTimestamp;
  bool                  clientOkFlag;
  struct timespec       pollTimeout;
  AuthorizationFailNode *authorizationFailNode,*oldestAuthorizationFailNode;
  ClientNode            *clientNode;
  SocketHandle          socketHandle;
  ClientList            clientList;
  String                clientName;
  uint                  clientPort;
  uint                  pollfdIndex;
  char                  buffer[2028];
  ulong                 receivedBytes;
  ulong                 i;
  ClientNode            *disconnectClientNode;

  // initialize variables
  AutoFree_init(&autoFreeList);
  List_init(&authorizationFailList);
  serverPort              = port;
  serverCA                = ca;
  serverCert              = cert;
  serverKey               = key;
  serverPassword          = password;
  serverJobsDirectory     = jobsDirectory;
  serverDefaultJobOptions = defaultJobOptions;
  List_init(&jobList);
  Semaphore_init(&jobList.lock);
  jobList.activeCount     = 0;
  Semaphore_init(&statusLock);
  Semaphore_init(&serverStateLock);
  serverState             = SERVER_STATE_RUNNING;
  pauseFlags.create       = FALSE;
  pauseFlags.restore      = FALSE;
  pauseFlags.indexUpdate  = FALSE;
  pauseEndDateTime        = 0LL;
  indexHandle             = NULL;
  quitFlag                = FALSE;
  AUTOFREE_ADD(&autoFreeList,&authorizationFailList,{ List_done(&clientList,CALLBACK((ListNodeFreeFunction)freeAuthorizationFailNode,NULL)); });
  AUTOFREE_ADD(&autoFreeList,&jobList, { List_done(&jobList,CALLBACK((ListNodeFreeFunction)freeJobNode,NULL)); });
  AUTOFREE_ADD(&autoFreeList,&jobList.lock,{ Semaphore_done(&jobList.lock); });
  AUTOFREE_ADD(&autoFreeList,&statusLock,{ Semaphore_done(&serverStateLock); });
  AUTOFREE_ADD(&autoFreeList,&serverStateLock,{ Semaphore_done(&serverStateLock); });

  logMessage(NULL,  // logHandle,
             LOG_TYPE_ALWAYS,
             "Started BAR server\n"
            );

  // create jobs directory if necessary
  if (!File_existsCString(serverJobsDirectory))
  {
    error = File_makeDirectoryCString(serverJobsDirectory,
                                      FILE_DEFAULT_USER_ID,
                                      FILE_DEFAULT_GROUP_ID,
                                      FILE_DEFAULT_PERMISSION
                                     );
    if (error != ERROR_NONE)
    {
      printError("Cannot create directory '%s' (error: %s)\n",
                 serverJobsDirectory,
                 Error_getText(error)
                );
      return error;
    }
  }
  if (!File_isDirectoryCString(serverJobsDirectory))
  {
    printError("'%s' is not a directory!\n",serverJobsDirectory);
    AutoFree_cleanup(&autoFreeList);
    return ERROR_NOT_A_DIRECTORY;
  }

  // open index database
  if (!stringIsEmpty(indexDatabaseFileName))
  {
    printInfo(1,"Init index database '%s'...",indexDatabaseFileName);
    error = Index_init(indexDatabaseFileName);
    if (error != ERROR_NONE)
    {
      printInfo(1,"FAIL!\n");
      printError("Cannot init index database '%s' (error: %s)!\n",
                 indexDatabaseFileName,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    printInfo(1,"ok\n");
    AUTOFREE_ADD(&autoFreeList,indexDatabaseFileName,{ Index_done(); });
  }

  // init server sockets
  serverFlag    = FALSE;
  serverTLSFlag = FALSE;
  if (port != 0)
  {
    error = Network_initServer(&serverSocketHandle,
                               port,
                               SERVER_SOCKET_TYPE_PLAIN
#if 0
                               NULL,
                               0,
                               NULL,
                               0,
                               NULL,
                               0
#endif
                              );
    if (error != ERROR_NONE)
    {
      printError("Cannot initialize server at port %u (error: %s)!\n",
                 port,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    printInfo(1,"Started BAR server on port %d\n",port);
    serverFlag = TRUE;
    AUTOFREE_ADD(&autoFreeList,&serverSocketHandle,{ Network_doneServer(&serverSocketHandle); });
  }
  if (tlsPort != 0)
  {
    if (   isCertificateAvailable(ca)
        && isCertificateAvailable(cert)
        && isKeyAvailable(key)
       )
    {
      #ifdef HAVE_GNU_TLS
        error = Network_initServer(&serverTLSSocketHandle,
                                   tlsPort,
                                   SERVER_SOCKET_TYPE_TLS
                                  );
        if (error != ERROR_NONE)
        {
          printError("Cannot initialize TLS/SSL server at port %u (error: %s)!\n",
                     tlsPort,
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return FALSE;
        }
        AUTOFREE_ADD(&autoFreeList,&serverTLSSocketHandle,{ Network_doneServer(&serverTLSSocketHandle); });
        printInfo(1,"Started BAR TLS/SSL server on port %u\n",tlsPort);
        serverTLSFlag = TRUE;
      #else /* not HAVE_GNU_TLS */
        UNUSED_VARIABLE(ca);
        UNUSED_VARIABLE(cert);
        UNUSED_VARIABLE(key);

        printError("TLS/SSL server is not supported!\n");
        if (serverFlag) Network_doneServer(&serverSocketHandle);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_GNU_TLS */
    }
    else
    {
      if (!isCertificateAvailable(ca)) printWarning("No certificate authority data (bar-ca.pem file) - TLS server not started.\n");
      if (!isCertificateAvailable(cert)) printWarning("No certificate data (bar-server-cert.pem file) - TLS server not started.\n");
      if (!isKeyAvailable(key)) printWarning("No key data (bar-server-key.pem file) - TLS server not started.\n");
    }
  }
  if (!serverFlag && !serverTLSFlag)
  {
    if ((port == 0) && (tlsPort == 0))
    {
      printError("Cannot start any server (error: no port numbers specified)!\n");
    }
    else
    {
      printError("Cannot start any server!\n");
    }
    AutoFree_cleanup(&autoFreeList);
    return ERROR_INVALID_ARGUMENT;
  }
  if (Password_isEmpty(password))
  {
    printWarning("No server password set!\n");
  }

  // read job list
  rereadAllJobs(serverJobsDirectory);

//TODO
  // init database pause callbacks
  Index_setPauseCallback(CALLBACK(indexPauseCallback,NULL));

  // start threads
  if (!Thread_init(&jobThread,"BAR job",globalOptions.niceLevel,jobThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize job thread!");
  }
  if (!Thread_init(&schedulerThread,"BAR scheduler",globalOptions.niceLevel,schedulerThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize scheduler thread!");
  }
  if (!Thread_init(&pauseThread,"BAR pause",globalOptions.niceLevel,pauseThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize pause thread!");
  }
  if (!Thread_init(&remoteConnectThread,"BAR remote connect",globalOptions.niceLevel,remoteConnectThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize remote thread!");
  }
  if (!Thread_init(&remoteThread,"BAR remote",globalOptions.niceLevel,remoteThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize remote thread!");
  }
  if (Index_isAvailable())
  {
    Semaphore_init(&indexThreadTrigger);
    if (!Thread_init(&indexThread,"BAR index",globalOptions.niceLevel,indexThreadCode,NULL))
    {
      HALT_FATAL_ERROR("Cannot initialize index thread!");
    }
    if (globalOptions.indexDatabaseAutoUpdateFlag)
    {
      if (!Thread_init(&autoIndexThread,"BAR auto index",globalOptions.niceLevel,autoIndexThreadCode,NULL))
      {
        HALT_FATAL_ERROR("Cannot initialize index update thread!");
      }
    }
    if (!Thread_init(&purgeExpiredThread,"BAR purge expired",globalOptions.niceLevel,purgeExpiredThreadCode,NULL))
    {
      HALT_FATAL_ERROR("Cannot initialize purge expire thread!");
    }
  }

  // init index
  indexHandle = Index_open(INDEX_PRIORITY_IMMEDIATE,INDEX_TIMEOUT);

  // run as server
  if (globalOptions.serverDebugFlag)
  {
    printWarning("Server is running in debug mode. No authorization is done and additional debug commands are enabled!\n");
  }

  // Note: ignore SIGALRM in ppoll()
  sigemptyset(&signalMask);
  sigaddset(&signalMask,SIGALRM);

  // process client requests
  maxPollfdCount = 64;  // initial max. number of parallel connections
  pollfds = (struct pollfd*)malloc(maxPollfdCount * sizeof(struct pollfd));
  if (pollfds == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  List_init(&clientList);
  clientName = String_new();
  while (!quitFlag)
  {
    // wait for connect or command
    pollfdCount = 0;

    if (serverFlag    && ((maxConnections == 0) || (List_count(&clientList) < maxConnections)))
    {
      if (pollfdCount >= pollfdCount)
      {
        maxPollfdCount += 64;
        pollfds = (struct pollfd*)realloc(pollfds,maxPollfdCount);
        if (pollfds == NULL) HALT_INSUFFICIENT_MEMORY();
      }
      pollfds[pollfdCount].fd     = Network_getServerSocket(&serverSocketHandle);
      pollfds[pollfdCount].events = POLLIN|POLLERR|POLLNVAL;
      pollServerSocketIndex = pollfdCount;
      pollfdCount++;
    }
    if (serverTLSFlag && ((maxConnections == 0) || (List_count(&clientList) < maxConnections)))
    {
      if (pollfdCount >= maxPollfdCount)
      {
        maxPollfdCount += 64;
        pollfds = (struct pollfd*)realloc(pollfds,maxPollfdCount);
        if (pollfds == NULL) HALT_INSUFFICIENT_MEMORY();
      }
      pollfds[pollfdCount].fd     = Network_getServerSocket(&serverTLSSocketHandle);
      pollfds[pollfdCount].events = POLLIN|POLLERR|POLLNVAL;
      pollServerTLSSocketIndex = pollfdCount;
      pollfdCount++;
    }

    nowTimestamp = Misc_getTimestamp();
    waitTimeout  = 60LL*US_PER_MINUTE; // wait for network connection max. 60min [us]
    LIST_ITERATE(&clientList,clientNode)
    {
      clientOkFlag = TRUE;

      // check if client should be served now
      if (clientNode->clientInfo.authorizationFailNode != NULL)
      {
        nextTimestamp = clientNode->clientInfo.authorizationFailNode->lastTimestamp+(uint64)SQUARE(clientNode->clientInfo.authorizationFailNode->count)*(uint64)AUTHORIZATION_PENALITY_TIME*1000LL;
        if (nowTimestamp <= nextTimestamp)
        {
          clientOkFlag = FALSE;
          if ((nextTimestamp-nowTimestamp) < waitTimeout)
          {
            waitTimeout = nextTimestamp-nowTimestamp;
          }
        }
      }

      // add clients to be served to select set
      if (clientOkFlag)
      {
        if (pollfdCount >= maxPollfdCount)
        {
          maxPollfdCount += 64;
          pollfds = (struct pollfd*)realloc(pollfds,maxPollfdCount);
          if (pollfds == NULL) HALT_INSUFFICIENT_MEMORY();
        }
        pollfds[pollfdCount].fd     = Network_getSocket(&clientNode->clientInfo.network.socketHandle);
        pollfds[pollfdCount].events = POLLIN|POLLERR|POLLNVAL;
        pollfdCount++;
      }
    }
    pollTimeout.tv_sec  = (long)(waitTimeout / 1000000LL);
    pollTimeout.tv_nsec = (long)((waitTimeout % 1000000LL) * 1000LL);
    (void)ppoll(pollfds,pollfdCount,&pollTimeout,&signalMask);

    // connect new clients
    if (   serverFlag
        && ((maxConnections == 0) || (List_count(&clientList) < maxConnections))
        && (pollfds[pollServerSocketIndex].revents == POLLIN)
       )
    {
      error = Network_accept(&socketHandle,
                             &serverSocketHandle,
                             SOCKET_FLAG_NON_BLOCKING
                            );
      if (error == ERROR_NONE)
      {
        Network_getRemoteInfo(&socketHandle,clientName,&clientPort);

        // initialize network for client
        clientNode = newNetworkClient(clientName,clientPort,socketHandle);

        // append to list of connected clients
        List_append(&clientList,clientNode);

        // find authorization fail node
        LIST_ITERATE(&authorizationFailList,authorizationFailNode)
        {
          if (String_equals(authorizationFailNode->clientName,clientName))
          {
            clientNode->clientInfo.authorizationFailNode = authorizationFailNode;
            break;
          }
        }

        printInfo(1,"Connected client '%s:%u'\n",String_cString(clientNode->clientInfo.network.name),clientNode->clientInfo.network.port);
      }
      else
      {
        printError("Cannot establish client connection (error: %s)!\n",
                   Error_getText(error)
                  );
      }
    }
    if (   serverTLSFlag
        && ((maxConnections == 0) || (List_count(&clientList) < maxConnections))
        && (pollfds[pollServerTLSSocketIndex].revents == POLLIN)
       )
    {
      error = Network_accept(&socketHandle,
                             &serverTLSSocketHandle,
                             SOCKET_FLAG_NON_BLOCKING
                            );
      if (error == ERROR_NONE)
      {
        Network_getRemoteInfo(&socketHandle,clientName,&clientPort);

        // start SSL
        error = Network_startSSL(&socketHandle,
                                 serverCA->data,
                                 serverCA->length,
                                 serverCert->data,
                                 serverCert->length,
                                 serverKey->data,
                                 serverKey->length
                                );
        if (error != ERROR_NONE)
        {
          printError("Cannot initialize TLS/SSL session for client '%s:%d' (error: %s)!\n",
                     String_cString(clientName),
                     clientPort,
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return FALSE;
        }

        // initialize network for client
        clientNode = newNetworkClient(clientName,clientPort,socketHandle);

        // append to list of connected clients
        List_append(&clientList,clientNode);

        // find authorization fail node
        LIST_ITERATE(&authorizationFailList,authorizationFailNode)
        {
          if (String_equals(authorizationFailNode->clientName,clientName))
          {
            clientNode->clientInfo.authorizationFailNode = authorizationFailNode;
            break;
          }
        }

        printInfo(1,"Connected client '%s:%u' (TLS/SSL)\n",String_cString(clientNode->clientInfo.network.name),clientNode->clientInfo.network.port);
      }
      else
      {
        printError("Cannot establish client TLS connection (error: %s)!\n",
                   Error_getText(error)
                  );
      }
    }

    // process client commands/disconnect clients
    for (pollfdIndex = 0; pollfdIndex < pollfdCount; pollfdIndex++)
    {
      if (pollfds[pollfdIndex].revents != 0)
      {
        // find client node
        clientNode = LIST_FIND(&clientList,
                               clientNode,
                               pollfds[pollfdIndex].fd == Network_getSocket(&clientNode->clientInfo.network.socketHandle)
                              );
        if (clientNode != NULL)
        {
          if ((pollfds[pollfdIndex].revents & POLLIN) != 0)
          {
            // receive data from client
            Network_receive(&clientNode->clientInfo.network.socketHandle,buffer,sizeof(buffer),WAIT_FOREVER,&receivedBytes);
            if (receivedBytes > 0)
            {
              // received data -> process
              do
              {
                for (i = 0; i < receivedBytes; i++)
                {
                  if (buffer[i] != '\n')
                  {
                    String_appendChar(clientNode->commandString,buffer[i]);
                  }
                  else
                  {
                    processCommand(&clientNode->clientInfo,clientNode->commandString);
                    String_clear(clientNode->commandString);
                  }
                }
                error = Network_receive(&clientNode->clientInfo.network.socketHandle,buffer,sizeof(buffer),WAIT_FOREVER,&receivedBytes);
              }
              while ((error == ERROR_NONE) && (receivedBytes > 0));
            }
            else
            {
              // remove from client list
              disconnectClientNode = clientNode;
              List_remove(&clientList,disconnectClientNode);

              // disconnect
              Network_disconnect(&disconnectClientNode->clientInfo.network.socketHandle);

              // update authorization fail info
              switch (disconnectClientNode->clientInfo.authorizationState)
              {
                case AUTHORIZATION_STATE_WAITING:
                  break;
                case AUTHORIZATION_STATE_OK:
                  // remove from authorization fail list
#ifndef WERROR
#warning ERRRORRRRRR: two clients from same host, then disconnect: node is deleted twice
#endif
                  authorizationFailNode = disconnectClientNode->clientInfo.authorizationFailNode;
                  if (authorizationFailNode != NULL)
                  {
                    List_removeAndFree(&authorizationFailList,
                                       authorizationFailNode,
                                       CALLBACK((ListNodeFreeFunction)freeAuthorizationFailNode,NULL)
                                      );
                  }
                  break;
                case AUTHORIZATION_STATE_FAIL:
                  // add to/update authorization fail list
                  authorizationFailNode = disconnectClientNode->clientInfo.authorizationFailNode;
                  if (authorizationFailNode == NULL)
                  {
                    authorizationFailNode = newAuthorizationFailNode(disconnectClientNode->clientInfo.network.name);
                    assert(authorizationFailNode != NULL);
                    List_append(&authorizationFailList,authorizationFailNode);
                  }
                  authorizationFailNode->count++;
                  authorizationFailNode->lastTimestamp = Misc_getTimestamp();
                  break;
              }
              printInfo(1,"Disconnected client '%s:%u'\n",String_cString(disconnectClientNode->clientInfo.network.name),disconnectClientNode->clientInfo.network.port);

              // free resources
              deleteClient(disconnectClientNode);
            }
          }
          else if ((pollfds[pollfdIndex].revents & (POLLERR|POLLNVAL)) != 0)
          {
            // error/disconnect -> remove from client list
            disconnectClientNode = clientNode;
            clientNode = List_remove(&clientList,disconnectClientNode);

            // disconnect
            Network_disconnect(&disconnectClientNode->clientInfo.network.socketHandle);

            // update authorization fail info
            switch (disconnectClientNode->clientInfo.authorizationState)
            {
              case AUTHORIZATION_STATE_WAITING:
                break;
              case AUTHORIZATION_STATE_OK:
                // remove from authorization fail list
#ifndef WERROR
#warning ERRRORRRRRR: two clients from same host, then disconnect: node is deleted twice
#endif
                authorizationFailNode = disconnectClientNode->clientInfo.authorizationFailNode;
                if (authorizationFailNode != NULL)
                {
                  List_removeAndFree(&authorizationFailList,
                                     authorizationFailNode,
                                     CALLBACK((ListNodeFreeFunction)freeAuthorizationFailNode,NULL)
                                    );
                }
                break;
              case AUTHORIZATION_STATE_FAIL:
                // add to/update authorization fail list
                authorizationFailNode = disconnectClientNode->clientInfo.authorizationFailNode;
                if (authorizationFailNode == NULL)
                {
                  authorizationFailNode = newAuthorizationFailNode(disconnectClientNode->clientInfo.network.name);
                  assert(authorizationFailNode != NULL);
                  List_append(&authorizationFailList,authorizationFailNode);
                }
                authorizationFailNode->count++;
                authorizationFailNode->lastTimestamp = Misc_getTimestamp();
                break;
            }
            printInfo(1,"Disconnected client '%s:%u'\n",String_cString(disconnectClientNode->clientInfo.network.name),disconnectClientNode->clientInfo.network.port);

            // free resources
            deleteClient(disconnectClientNode);
          }
          else
          {
//TODO: remove unknown
  fprintf(stderr,"%s, %d: %x\n",__FILE__,__LINE__,pollfds[pollfdIndex].revents);
          }
        }
      }
    }

    // disconnect clients because of authorization failure
    clientNode = clientList.head;
    while (clientNode != NULL)
    {
      if (clientNode->clientInfo.authorizationState == AUTHORIZATION_STATE_FAIL)
      {
        // remove from connected list
        disconnectClientNode = clientNode;
        clientNode = List_remove(&clientList,disconnectClientNode);

        // disconnect
        Network_disconnect(&disconnectClientNode->clientInfo.network.socketHandle);

        // add to/update authorization fail list
        authorizationFailNode = disconnectClientNode->clientInfo.authorizationFailNode;
        if (authorizationFailNode == NULL)
        {
          authorizationFailNode = newAuthorizationFailNode(disconnectClientNode->clientInfo.network.name);
          assert(authorizationFailNode != NULL);
          List_append(&authorizationFailList,authorizationFailNode);
        }
        authorizationFailNode->count++;
        authorizationFailNode->lastTimestamp = Misc_getTimestamp();

        printInfo(1,"Disconnected client '%s:%u'\n",String_cString(disconnectClientNode->clientInfo.network.name),disconnectClientNode->clientInfo.network.port);
      }
      else
      {
        // next client
        clientNode = clientNode->next;
      }
    }

    // clean-up authorization failure list
    nowTimestamp          = Misc_getTimestamp();
    authorizationFailNode = authorizationFailList.head;
    while (authorizationFailNode != NULL)
    {
      // find active client
      LIST_ITERATE(&clientList,clientNode)
      {
        if (clientNode->clientInfo.authorizationFailNode == authorizationFailNode) break;
      }

      // check if authorization fail timed out for not active clients
      if (   (clientNode == NULL)
          && (nowTimestamp > (authorizationFailNode->lastTimestamp+(uint64)MAX_AUTHORIZATION_HISTORY_KEEP_TIME*1000LL))
         )
      {
        authorizationFailNode = List_removeAndFree(&authorizationFailList,
                                                   authorizationFailNode,
                                                   CALLBACK((ListNodeFreeFunction)freeAuthorizationFailNode,NULL)
                                                  );
      }
      else
      {
        authorizationFailNode = authorizationFailNode->next;
      }
    }

    assert(MAX_AUTHORIZATION_FAIL_HISTORY);
    while (List_count(&authorizationFailList) > MAX_AUTHORIZATION_FAIL_HISTORY)
    {
      // find oldest authorization failure
      oldestAuthorizationFailNode = authorizationFailList.head;
      LIST_ITERATE(&authorizationFailList,authorizationFailNode)
      {
        // find active client
        LIST_ITERATE(&clientList,clientNode)
        {
          if (clientNode->clientInfo.authorizationFailNode == authorizationFailNode) break;
        }

        // get oldest not active client
        if (   (clientNode == NULL)
            && (authorizationFailNode->lastTimestamp < oldestAuthorizationFailNode->lastTimestamp)
           )
        {
          oldestAuthorizationFailNode = authorizationFailNode;
        }
      }

      // remove oldest authorization failure from list
      List_removeAndFree(&authorizationFailList,
                         oldestAuthorizationFailNode,
                         CALLBACK((ListNodeFreeFunction)freeAuthorizationFailNode,NULL)
                        );
    }
  }
  String_delete(clientName);
  List_done(&clientList,CALLBACK((ListNodeFreeFunction)freeJobNode,NULL));
#ifndef USE_POLL
  free(pollfds);
#endif

  // disconnect all clients
  while (!List_isEmpty(&clientList))
  {
    clientNode = (ClientNode*)List_getFirst(&clientList);

    Network_disconnect(&clientNode->clientInfo.network.socketHandle);
    deleteClient(clientNode);
  }

  // done index
  Index_close(indexHandle);

  // wait for thread exit
  Semaphore_setEnd(&jobList.lock);
  if (Index_isAvailable())
  {
    Thread_join(&purgeExpiredThread);
    if (globalOptions.indexDatabaseAutoUpdateFlag)
    {
      Thread_join(&autoIndexThread);
    }
    Thread_join(&indexThread);
  }
  Thread_join(&remoteThread);
  Thread_join(&remoteConnectThread);
  Thread_join(&pauseThread);
  Thread_join(&schedulerThread);
  Thread_join(&jobThread);

//TODO
  // done database pause callbacks
  Index_setPauseCallback(CALLBACK(NULL,NULL));

  // done server
  if (serverFlag   ) Network_doneServer(&serverSocketHandle);
  if (serverTLSFlag) Network_doneServer(&serverTLSSocketHandle);

  // free resources
  if (Index_isAvailable())
  {
    if (globalOptions.indexDatabaseAutoUpdateFlag)
    {
      Thread_done(&autoIndexThread);
    }
    Thread_done(&indexThread);
    Semaphore_done(&indexThreadTrigger);
  }
  Thread_done(&remoteThread);
  Thread_done(&remoteConnectThread);
  Thread_done(&pauseThread);
  Thread_done(&schedulerThread);
  Thread_done(&jobThread);
  Semaphore_done(&serverStateLock);
  if (!stringIsEmpty(indexDatabaseFileName)) Index_done();
  List_done(&clientList,CALLBACK((ListNodeFreeFunction)freeClientNode,NULL));
  Semaphore_done(&statusLock);
  Semaphore_done(&jobList.lock);
  List_done(&jobList,CALLBACK((ListNodeFreeFunction)freeJobNode,NULL));
  List_done(&authorizationFailList,CALLBACK((ListNodeFreeFunction)freeAuthorizationFailNode,NULL));
  AutoFree_done(&autoFreeList);

  logMessage(NULL,  // logHandle,
             LOG_TYPE_ALWAYS,
             "Terminated BAR server\n"
            );

  return ERROR_NONE;
}

Errors Server_batch(int inputDescriptor,
                    int outputDescriptor
                   )
{
  Errors     error;
  FileHandle inputFileHandle,outputFileHandle;
  ClientInfo clientInfo;
  String     commandString;

  // initialize variables
  List_init(&jobList);
  quitFlag = FALSE;

  // initialize input/output
  error = File_openDescriptor(&inputFileHandle,inputDescriptor,FILE_OPEN_READ|FILE_STREAM);
  if (error != ERROR_NONE)
  {
    fprintf(stderr,
            "ERROR: Cannot initialize input (error: %s)!\n",
            Error_getText(error)
           );
    return error;
  }
  error = File_openDescriptor(&outputFileHandle,outputDescriptor,FILE_OPEN_APPEND|FILE_STREAM);
fprintf(stderr,"%s, %d: %x\n",__FILE__,__LINE__,error);
  if (error != ERROR_NONE)
  {
    fprintf(stderr,
            "ERROR: Cannot initialize output (error: %s)!\n",
            Error_getText(error)
           );
    File_close(&inputFileHandle);
    return error;
  }
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

  // init index
  indexHandle = Index_open(INDEX_PRIORITY_IMMEDIATE,INDEX_TIMEOUT);

  // run in batch mode
  if (globalOptions.serverDebugFlag)
  {
    printWarning("Server is running in debug mode. No authorization is done and additional debug commands are enabled!\n");
  }

  // init client
  initClient(&clientInfo);
  initBatchClient(&clientInfo,&outputFileHandle);

  // send info
  File_printLine(&outputFileHandle,
                 "BAR VERSION %d %d\n",
                 SERVER_PROTOCOL_VERSION_MAJOR,
                 SERVER_PROTOCOL_VERSION_MINOR
                );
  File_flush(&outputFileHandle);

  // process client requests
  commandString = String_new();
#if 1
  while (!quitFlag && !File_eof(&inputFileHandle))
  {
    // read command line
    File_readLine(&inputFileHandle,commandString);

    // process
//TODO
    processCommand(&clientInfo,commandString);
  }
#else /* 0 */
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);
String_setCString(commandString,"1 SET crypt-password password='muster'");processCommand(&clientInfo,commandString);
String_setCString(commandString,"2 ADD_INCLUDE_PATTERN type=REGEX pattern=test/[^/]*");processCommand(&clientInfo,commandString);
String_setCString(commandString,"3 ARCHIVE_LIST name=test.bar");processCommand(&clientInfo,commandString);
//String_setCString(commandString,"3 ARCHIVE_LIST backup/backup-torsten-bar-000.bar");processCommand(&clientInfo,commandString);
processCommand(&clientInfo,commandString);
#endif /* 0 */
  String_delete(commandString);

  // done index
  Index_close(indexHandle);

  // free resources
  doneClient(&clientInfo);
  File_close(&outputFileHandle);
  File_close(&inputFileHandle);
  List_done(&jobList,CALLBACK((ListNodeFreeFunction)freeJobNode,NULL));

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
