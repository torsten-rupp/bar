/***********************************************************************\
*
* $Revision: 8947 $
* $Date: 2018-11-29 13:04:59 +0100 (Thu, 29 Nov 2018) $
* $Author: torsten $
* Contents: Backup ARchiver job functions
* Systems: all
*
\***********************************************************************/

#ifndef __JOBS__
#define __JOBS__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"
#include "common/patternlists.h"

#include "bar_global.h"
#include "connector.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
extern const ConfigValue JOB_CONFIG_VALUES[];

/***************************** Datatypes *******************************/

// job owner
typedef struct
{
  uint32 userId;                                              // user id
  uint32 groupId;                                             // group id
} JobOptionsOwner;

// job compress algorithms
typedef struct
{
  struct
  {
    CompressAlgorithms delta;                                 // delta compress algorithm to use
    CompressAlgorithms byte;                                  // byte compress algorithm to use
  } value;
  bool               isSet;                                   // TRUE if byte compress algorithm command line option is set
} JobOptionsCompressAlgorithms;

// job crypt algorithm
typedef struct
{
  CryptAlgorithms              values[4];                     // crypt algorithms to use
  bool                         isSet;                         // TRUE if byte crypt algorithm command line option is set
} JobOptionCryptAlgorithms;

// job comment
typedef struct
{
  String                       value;                         // comment
  bool                         isSet;                         // TRUE if comment command line option is set
} JobComment;

// see forward declaration in forward.h
struct JobOptions
{
  String                       uuid;
  ArchiveTypes                 archiveType;                   // archive type (normal, full, incremental, differential)

  uint64                       archivePartSize;               // archive part size [bytes]

  String                       incrementalListFileName;       // name of incremental list file

  int                          directoryStripCount;           // number of directories to strip in restore or DIRECTORY_STRIP_ANY for all
  String                       destination;                   // destination for restore
  JobOptionsOwner              owner;                         // restore owner
  FilePermission               permissions;                   // restore permissions

  PatternTypes                 patternType;                   // pattern type

  JobOptionsCompressAlgorithms compressAlgorithms;            // compress algorithms

  CryptTypes                   cryptType;                     // crypt type (symmetric, asymmetric)
  JobOptionCryptAlgorithms     cryptAlgorithms;               // crypt algorithms to use
  PasswordModes                cryptPasswordMode;             // crypt password mode
  Password                     cryptPassword;                 // crypt password
  Key                          cryptPublicKey;
  Key                          cryptPrivateKey;

  String                       preProcessScript;              // script to execute before start of job
  String                       postProcessScript;             // script to execute after after termination of job

  FileServer                   fileServer;                    // job specific file server settings
  FTPServer                    ftpServer;                     // job specific FTP server settings
  SSHServer                    sshServer;                     // job specific SSH server settings
  WebDAVServer                 webDAVServer;                  // job specific WebDAV server settings
  OpticalDisk                  opticalDisk;                   // job specific optical disk settings
  String                       deviceName;                    // device name to use
  Device                       device;                        // job specific device settings

  uint64                       maxFragmentSize;               // max. fragment size [bytes]
  uint64                       maxStorageSize;                // max. storage size [bytes]
//TODO
#if 0
  uint                         minKeep,maxKeep;               // min./max keep count
  uint                         maxAge;                        // max. age [days]
#endif
  uint64                       volumeSize;                    // volume size or 0LL for default [bytes]

  JobComment                   comment;                       // comment

  bool                         skipUnreadableFlag;            // TRUE for skipping unreadable files
  bool                         forceDeltaCompressionFlag;     // TRUE to force delta compression of files
  bool                         ignoreNoDumpAttributeFlag;     // TRUE for ignoring no-dump attribute
  ArchiveFileModes             archiveFileMode;               // archive files write mode
  RestoreEntryModes            restoreEntryMode;              // overwrite existing entry mode on restore
  bool                         errorCorrectionCodesFlag;      // TRUE iff error correction codes should be added
  bool                         alwaysCreateImageFlag;         // TRUE iff always create image for CD/DVD/BD/device
  bool                         blankFlag;                     // TRUE to blank medium before writing
  bool                         waitFirstVolumeFlag;           // TRUE for wait for first volume
  bool                         rawImagesFlag;                 // TRUE for storing raw images
  bool                         noFragmentsCheckFlag;          // TRUE to skip checking file fragments for completeness
  bool                         noIndexDatabaseFlag;           // TRUE for do not store index database for archives
  bool                         forceVerifySignaturesFlag;     // TRUE to force verify signatures of archives
  bool                         skipVerifySignaturesFlag;      // TRUE to not verify signatures of archives
  bool                         noStorageFlag;                 // TRUE to skip storage, only create incremental data file
  bool                         noSignatureFlag;               // TRUE for not appending signatures
  bool                         noBAROnMediumFlag;             // TRUE for not storing BAR on medium
  bool                         noStopOnErrorFlag;             // TRUE for not stopping immediately on error
};

// schedule date/time
typedef struct
{
  int year;                                             // year or SCHEDULE_ANY
  int month;                                            // month or SCHEDULE_ANY
  int day;                                              // day or SCHEDULE_ANY
} ScheduleDate;
typedef WeekDaySet ScheduleWeekDaySet;
typedef struct
{
  int hour;                                             // hour or SCHEDULE_ANY
  int minute;                                           // minute or SCHEDULE_ANY
} ScheduleTime;
typedef struct ScheduleNode
{
  LIST_NODE_HEADER(struct ScheduleNode);

  String             uuid;                              // unique id
  String             parentUUID;                        // unique parent id or NULL
  ScheduleDate       date;
  ScheduleWeekDaySet weekDaySet;
  ScheduleTime       time;
  ArchiveTypes       archiveType;                       // archive type to create
  uint               interval;                          // continuous interval [min]
  String             customText;                        // custom text
  bool               noStorage;                         // TRUE to skip storage, only create incremental data file
  bool               enabled;                           // TRUE iff enabled
  String             preProcessScript;                  // script to execute before start of job
  String             postProcessScript;                 // script to execute after after termination of job

  uint64             lastExecutedDateTime;              // last execution date/time (timestamp) (Note: read from <jobs dir>/.<job name>)
  String             lastErrorMessage;                  // last error message
  ulong              executionCount;                    // number of executions
  uint64             averageDuration;                   // average duration [s]
  ulong              totalEntityCount;                  // total number of entities
  ulong              totalStorageCount;                 // total number of storage files
  uint64             totalStorageSize;                  // total size of storage files
  ulong              totalEntryCount;                   // total number of entries
  uint64             totalEntrySize;                    // total size of entities

  // deprecated
  bool               deprecatedPersistenceFlag;         // TRUE iff deprecated persistance data is set
  int                minKeep,maxKeep;                   // min./max keep count
  int                maxAge;                            // max. age [days]
} ScheduleNode;

typedef struct
{
  LIST_HEADER(ScheduleNode);
} ScheduleList;

// persistence
typedef struct PersistenceNode
{
  LIST_NODE_HEADER(struct PersistenceNode);

  uint         id;
  ArchiveTypes archiveType;                             // archive type to create
  int          minKeep,maxKeep;                         // min./max keep count
  int          maxAge;                                  // max. age [days]
} PersistenceNode;

typedef struct
{
  LIST_HEADER(PersistenceNode);
  uint64 lastModificationTimestamp;                     // last modification timestamp
} PersistenceList;

// job type
typedef enum
{
  JOB_TYPE_CREATE,
  JOB_TYPE_RESTORE,
} JobTypes;

// job
typedef struct
{
//  String              uuid;                             // unique id
  JobTypes            jobType;                          // job type: backup, restore
//  String              name;                             // name of job
//  struct
//  {
//    String name;
//    uint   port;
//    bool   forceSSL;
//  }                   slaveHost;                        // slave host
  String              archiveName;                      // archive name
  EntryList           includeEntryList;                 // included entries
  String              includeFileCommand;               // include file command
  String              includeImageCommand;              // include image command
  PatternList         excludePatternList;               // excluded entry patterns
  String              excludeCommand;                   // exclude entries command
  MountList           mountList;                        // mount list
  PatternList         compressExcludePatternList;       // excluded compression patterns
  DeltaSourceList     deltaSourceList;                  // delta sources
//  ScheduleList        scheduleList;                     // schedule list (unordered)
//  PersistenceList     persistenceList;                  // persistence list (ordered)
  JobOptions          jobOptions;                       // options for job
} Job;

// slave states
typedef enum
{
  SLAVE_STATE_OFFLINE,
  SLAVE_STATE_ONLINE,
  SLAVE_STATE_PAIRED
} SlaveStates;

// job node
typedef struct JobNode
{
  LIST_NODE_HEADER(struct JobNode);

  // job
  Job                 job;
  String              uuid;                             // unique id
  String              name;                             // name of job
  struct
  {
    String name;
    uint   port;
    bool   forceSSL;
  }                   slaveHost;                        // slave host
  ScheduleList        scheduleList;                     // schedule list (unordered)
  PersistenceList     persistenceList;                  // persistence list (ordered)
#if 0
  String              uuid;                             // unique id
  JobTypes            jobType;                          // job type: backup, restore
  String              name;                             // name of job
  struct
  {
    String name;
    uint   port;
    bool   forceSSL;
  }                   slaveHost;                        // slave host
  String              archiveName;                      // archive name
  EntryList           includeEntryList;                 // included entries
  String              includeFileCommand;               // include file command
  String              includeImageCommand;              // include image command
  PatternList         excludePatternList;               // excluded entry patterns
  String              excludeCommand;                   // exclude entries command
  MountList           mountList;                        // mount list
  PatternList         compressExcludePatternList;       // excluded compression patterns
  DeltaSourceList     deltaSourceList;                  // delta sources
  ScheduleList        scheduleList;                     // schedule list (unordered)
  PersistenceList     persistenceList;                  // persistence list (ordered)
  JobOptions          jobOptions;                       // options for job
#endif

  // modified info
  bool                modifiedFlag;                     // TRUE iff job config modified
//TODO: remove?
//  uint64              lastIncludeExcludeModified;
//  uint64              lastScheduleModified;

  // schedule info
  uint64              lastScheduleCheckDateTime;        // last check date/time (timestamp)

  // job passwords
  Password            *ftpPassword;                     // FTP password if password mode is 'ask'
  Password            *sshPassword;                     // SSH password if password mode is 'ask'
  Password            *cryptPassword;                   // crypt password if password mode is 'ask'

  // job file/master
  String              fileName;                         // file name or NULL
  uint64              fileModified;                     // file modified date/time (timestamp)

  ServerIO            *masterIO;                        // master i/o or NULL if not a slave job

  // job running state
  ConnectorInfo       connectorInfo;

  JobStates           state;                            // current state of job
  SlaveStates         slaveState;

  StatusInfo          statusInfo;

  String              scheduleUUID;                     // schedule UUID or empty
  String              scheduleCustomText;               // schedule custom text or empty
  ArchiveTypes        archiveType;                      // archive type to create
  bool                noStorage;                        // TRUE to skip storage, only create incremental data file
  bool                dryRun;                           // TRUE iff dry-run (no storage, no index update)
  uint64              startDateTime;                    // start date/time [s]
  String              byName;                           // state changed by name

  bool                requestedAbortFlag;               // request abort current job execution
  String              abortedByInfo;                    // aborted by info
  uint                requestedVolumeNumber;            // requested volume number
  uint                volumeNumber;                     // load volume number
  String              volumeMessage;                    // load volume message
  bool                volumeUnloadFlag;                 // TRUE to unload volume

  uint64              lastExecutedDateTime;             // last execution date/time (timestamp) (Note: read from <jobs dir>/.<job name>)
  String              lastErrorMessage;                 // last error message
  struct
  {
    ulong normal;
    ulong full;
    ulong incremental;
    ulong differential;
    ulong continuous;
  }                   executionCount;                   // number of executions
  struct
  {
    uint64 normal;
    uint64 full;
    uint64 incremental;
    uint64 differential;
    uint64 continuous;
  }                   averageDuration;                  // average duration [s]
  ulong               totalEntityCount;                 // total number of entities
  ulong               totalStorageCount;                // total number of storage files
  uint64              totalStorageSize;                 // total size of storage files
  ulong               totalEntryCount;                  // total number of entries
  uint64              totalEntrySize;                   // total size of entities

  // running info
  struct
  {
    PerformanceFilter entriesPerSecondFilter;
    PerformanceFilter bytesPerSecondFilter;
    PerformanceFilter storageBytesPerSecondFilter;

    Errors            error;                            // error code

    double            entriesPerSecond;                 // average processed entries last 10s [1/s]
    double            bytesPerSecond;                   // average processed bytes last 10s [1/s]
    double            storageBytesPerSecond;            // average processed storage bytes last 10s [1/s]
    ulong             estimatedRestTime;                // estimated rest running time [s]
  }                   runningInfo;
} JobNode;

// list with jobs
typedef struct
{
  LIST_HEADER(JobNode);

  Semaphore lock;
  uint      activeCount;
} JobList;

/***************************** Variables *******************************/
extern JobList jobList;                // job list

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : JOB_LIST_LOCKED_DO
* Purpose: execute block with job list locked
* Input  : semaphoreLock     - lock flag variable (SemaphoreLock)
*          semaphoreLockType - lock type; see SemaphoreLockTypes
*          timeout           - timeout [ms] or NO_WAIT, WAIT_FOREVER
* Output : -
* Return : -
* Notes  : usage:
*            SemaphoreLock semaphoreLock;
*            JOB_LIST_LOCKED_DO(semaphoreLock,semaphoreLockType,timeout)
*            {
*              ...
*            }
*
*          semaphore must be unlocked manually if 'break' is used!
\***********************************************************************/

#define JOB_LIST_LOCKED_DO(semaphoreLock,semaphoreLockType,timeout) \
  for (semaphoreLock = Job_listLock(semaphoreLockType,timeout); \
       semaphoreLock; \
       Job_listUnlock(), semaphoreLock = FALSE \
      )

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Job_initAll
* Purpose: initialize jobs
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Job_initAll(void);

/***********************************************************************\
* Name   : Job_doneAll
* Purpose: deinitialize jobs
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_doneAll(void);

/***********************************************************************\
* Name   : Job_listLock
* Purpose: lock job list
* Input  : semaphoreLockType - lock type; see SemaphoreLockTypes
*          timeout           - timeout [ms] or NO_WAIT, WAIT_FOREVER
* Output : -
* Return : TRUE iff locked
* Notes  : -
\***********************************************************************/

INLINE bool Job_listLock(SemaphoreLockTypes semaphoreLockType,
                         long               timeout
                        );
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE bool Job_listLock(SemaphoreLockTypes semaphoreLockType,
                         long               timeout
                        )
{
  return Semaphore_lock(&jobList.lock,semaphoreLockType,timeout);
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_listUnlock
* Purpose: unlock job list
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Job_listUnlock(void);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE void Job_listUnlock(void)
{
  Semaphore_unlock(&jobList.lock);
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_new
* Purpose: create new job
* Input  : jobType           - job type
*          name              - name
*          jobUUID           - job UUID or NULL for generate new UUID
*          fileName          - file name or NULL
*          defaultJobOptions - default job options or NULL
* Output : -
* Return : job node
* Notes  : -
\***********************************************************************/

JobNode *Job_new(JobTypes         jobType,
                 ConstString      name,
                 ConstString      jobUUID,
                 ConstString      fileName,
                 const JobOptions *defaultJobOptions
                );

/***********************************************************************\
* Name   : Job_copy
* Purpose: copy job node
* Input  : jobNode  - job node
*          fileName - file name
* Output : -
* Return : new job node
* Notes  : -
\***********************************************************************/

JobNode *Job_copy(const JobNode *jobNode,
                  ConstString   fileName
                 );

/***********************************************************************\
* Name   : Job_delete
* Purpose: delete job
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_delete(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_isLocal
* Purpose: check if local job
* Input  : jobNode - job node
* Output : -
* Return : TRUE iff local job
* Notes  : -
\***********************************************************************/

INLINE bool Job_isLocal(const JobNode *jobNode);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE bool Job_isLocal(const JobNode *jobNode)
{
  assert(jobNode != NULL);

  return String_isEmpty(jobNode->slaveHost.name);
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_isRemote
* Purpose: check if a remove job on slave
* Input  : jobNode - job node
* Output : -
* Return : TRUE iff slave job
* Notes  : -
\***********************************************************************/

INLINE bool Job_isRemote(const JobNode *jobNode);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE bool Job_isRemote(const JobNode *jobNode)
{
  assert(jobNode != NULL);

  return !String_isEmpty(jobNode->slaveHost.name);
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_isWaiting
* Purpose: check if job is waiting
* Input  : jobNode - job node
* Output : -
* Return : TRUE iff job is waiting
* Notes  : -
\***********************************************************************/

INLINE bool Job_isWaiting(JobStates jobState);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE bool Job_isWaiting(JobStates jobState)
{
  return (jobState == JOB_STATE_WAITING);
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_isActive
* Purpose: check if job is active (waiting/running)
* Input  : jobNode - job node
* Output : -
* Return : TRUE iff job is active
* Notes  : -
\***********************************************************************/

INLINE bool Job_isActive(JobStates jobState);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE bool Job_isActive(JobStates jobState)
{
  return (   (jobState == JOB_STATE_WAITING)
          || (jobState == JOB_STATE_RUNNING)
          || (jobState == JOB_STATE_REQUEST_FTP_PASSWORD)
          || (jobState == JOB_STATE_REQUEST_SSH_PASSWORD)
          || (jobState == JOB_STATE_REQUEST_WEBDAV_PASSWORD)
          || (jobState == JOB_STATE_REQUEST_CRYPT_PASSWORD)
          || (jobState == JOB_STATE_REQUEST_VOLUME)
          || (jobState == JOB_STATE_DISCONNECTED)
         );
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_isRunning
* Purpose: check if job is running
* Input  : jobNode - job node
* Output : -
* Return : TRUE iff job is running
* Notes  : -
\***********************************************************************/

INLINE bool Job_isRunning(JobStates jobState);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE bool Job_isRunning(JobStates jobState)
{
  return (   (jobState == JOB_STATE_RUNNING)
          || (jobState == JOB_STATE_REQUEST_FTP_PASSWORD)
          || (jobState == JOB_STATE_REQUEST_SSH_PASSWORD)
          || (jobState == JOB_STATE_REQUEST_WEBDAV_PASSWORD)
          || (jobState == JOB_STATE_REQUEST_CRYPT_PASSWORD)
          || (jobState == JOB_STATE_REQUEST_VOLUME)
          || (jobState == JOB_STATE_DISCONNECTED)
         );
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_isSomeActive
* Purpose: check if some job is active
* Input  : -
* Output : -
* Return : TRUE iff some job is active
* Notes  : -
\***********************************************************************/

INLINE bool Job_isSomeActive(void);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE bool Job_isSomeActive(void)
{
  return jobList.activeCount > 0;
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_isSomeRunning
* Purpose: check if some job is runnging
* Input  : -
* Output : -
* Return : TRUE iff some job is running
* Notes  : -
\***********************************************************************/

bool Job_isSomeRunning(void);

/***********************************************************************\
* Name   : Job_find
* Purpose: find job by name
* Input  : name - job name
* Output : -
* Return : job node or NULL if not found
* Notes  : -
\***********************************************************************/

JobNode *Job_find(ConstString name);

/***********************************************************************\
* Name   : Job_exists
* Purpose: check if job exists by name
* Input  : name - job name
* Output : -
* Return : TRUE iff job exists
* Notes  : -
\***********************************************************************/

bool Job_exists(ConstString name);

/***********************************************************************\
* Name   : Job_findByUUID
* Purpose: find job by uuid
* Input  : uuid - job uuid
* Output : -
* Return : job node or NULL if not found
* Notes  : -
\***********************************************************************/

JobNode *Job_findByUUID(ConstString uuid);

/***********************************************************************\
* Name   : Job_findByName
* Purpose: find job by name
* Input  : name - job name
* Output : -
* Return : job node or NULL if not found
* Notes  : -
\***********************************************************************/

JobNode *Job_findByName(ConstString name);

/***********************************************************************\
* Name   : Job_findScheduleByUUID
* Purpose: find schedule by uuid
* Input  : jobNode      - job node (can be NULL)
*          scheduleUUID - schedule UUID
* Output : -
* Return : schedule node or NULL if not found
* Notes  : -
\***********************************************************************/

ScheduleNode *Job_findScheduleByUUID(const JobNode *jobNode, ConstString scheduleUUID);

/***********************************************************************\
* Name   : Job_listChanged
* Purpose: called when job list changed
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_listChanged(void);

/***********************************************************************\
* Name   : Job_includeExcludeChanged
* Purpose: called when include/exclude lists changed
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_includeExcludeChanged(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_mountChanged
* Purpose: called when mount lists changed
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_mountChanged(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_scheduleChanged
* Purpose: notify schedule related actions
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_scheduleChanged(const JobNode *jobNode);

/***********************************************************************\
* Name   : Job_jpersistenceChanged
* Purpose: notify persistence related actions
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_persistenceChanged(const JobNode *jobNode);

/***********************************************************************\
* Name   : Job_writeScheduleInfo
* Purpose: write job schedule info
* Input  : jobNode - job node
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

//TODO: required?
Errors Job_writeScheduleInfo(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_readScheduleInfo
* Purpose: read job schedule info
* Input  : jobNode - job node
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

//TODO: required?
Errors Job_readScheduleInfo(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_write
* Purpose: write (update) job file
* Input  : jobNode - job node
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Job_write(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_writeModifiedAll
* Purpose: write (update) modified job files
* Input  : -
* Output : -
* Return : -
* Notes  : update jobList
\***********************************************************************/

void Job_writeModifiedAll(void);

/***********************************************************************\
* Name   : Job_read
* Purpose: read job from file
* Input  : fileName - file name
* Output : -
* Return : TRUE iff job read, FALSE otherwise (error)
* Notes  : -
\***********************************************************************/

bool Job_read(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_rereadAll
* Purpose: re-read all job files
* Input  : jobsDirectory     - directory with job files
*          defaultJobOptions - default job options or NULL
* Output : -
* Return : ERROR_NONE or error code
* Notes  : update jobList
\***********************************************************************/

Errors Job_rereadAll(ConstString      jobsDirectory,
                     const JobOptions *defaultJobOptions
                    );

/***********************************************************************\
* Name   : Job_trigger
* Purpose: trogger job run
* Input  : jobNode            - job node
*          archiveType        - archive type to create
*          scheduleUUID       - schedule UUID or NULL
*          scheduleCustomText - schedule custom text or NULL
*          noStorage          - TRUE for no-strage, FALSE otherwise
*          dryRun             - TRUE for dry-run, FALSE otherwise
*          startDateTime      - date/time of start [s]
*          byName             - by name or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_trigger(JobNode      *jobNode,
                 ArchiveTypes archiveType,
                 ConstString  scheduleUUID,
                 ConstString  scheduleCustomText,
                 bool         noStorage,
                 bool         dryRun,
                 uint64       startDateTime,
                 const char   *byName
                );

/***********************************************************************\
* Name   : Job_start
* Purpose: start job (store running data)
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_start(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_done
* Purpose: done job (store running data, free job data, e. g. passwords)
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_done(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_abort
* Purpose: abort job
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_abort(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_reset
* Purpose: reset job
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_reset(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_resetRunningInfo
* Purpose: reset job running info
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_resetRunningInfo(JobNode *jobNode);

#if 0
/***********************************************************************\
* Name   : Server_addJob
* Purpose: add new job to server for execution
* Input  : jobType            - job type
           name               - name of job
*          archiveName        - archive name
*          includePatternList - include pattern list
*          excludePatternList - exclude pattern list
*          jobOptions         - job options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Job_add(JobTypes          jobType,
                     const String      name,
                     const String      archiveName,
                     const PatternList *includePatternList,
                     const PatternList *excludePatternList,
                     const JobOptions  *jobOptions
                    );
#endif /* 0 */

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Job_initOptions
* Purpose: init job options
* Input  : jobOptions - job options variable
* Output : jobOptions - initialized job options variable
* Return : -
* Notes  : -
\***********************************************************************/

void Job_initOptions(JobOptions *jobOptions);

/***********************************************************************\
* Name   : Job_duplicateOptions
* Purpose: duplicated job options
* Input  : jobOptions     - job options variable
*          fromJobOptions - source job options
* Output : jobOptions - initialized job options variable
* Return : -
* Notes  : -
\***********************************************************************/

void Job_duplicateOptions(JobOptions *jobOptions, const JobOptions *fromJobOptions);

/***********************************************************************\
* Name   : Job_setOptions
* Purpose: set job options
* Input  : jobOptions     - job options variable
*          fromJobOptions - source job options
* Output : jobOptions - initialized job options variable
* Return : -
* Notes  : -
\***********************************************************************/

void Job_setOptions(JobOptions *jobOptions, const JobOptions *fromJobOptions);

/***********************************************************************\
* Name   : doneJobOptions
* Purpose: done job options
* Input  : jobOptions - job options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_doneOptions(JobOptions *jobOptions);

#ifdef __cplusplus
  }
#endif

#endif /* __JOBS__ */

/* end of file */
