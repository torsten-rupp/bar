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
#include "entrylists.h"
#include "connector.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
extern const ConfigValue JOB_CONFIG_VALUES[];

/***************************** Datatypes *******************************/

// see forward declaration in forward.h
struct JobOptions
{
  String                      uuid;

  String                      includeFileListFileName;       // include files list file name
  String                      includeFileCommand;            // include files command
  String                      includeImageListFileName;      // include images list file name
  String                      includeImageCommand;           // include images command
  String                      excludeListFileName;           // exclude entries list file name
  String                      excludeCommand;                // exclude entries command

  MountList                   mountList;                     // mount list
  PatternList                 compressExcludePatternList;    // excluded compression patterns

  DeltaSourceList             deltaSourceList;               // delta sources

  ScheduleList                scheduleList;                  // schedule list (unordered)

  PersistenceList             persistenceList;               // persistence list (ordered)


  ArchiveTypes                archiveType;                   // archive type (normal, full, incremental, differential)

  uint64                      archivePartSize;               // archive part size [bytes]

  String                      incrementalListFileName;       // name of incremental list file

  int                         directoryStripCount;           // number of directories to strip in restore or DIRECTORY_STRIP_ANY for all
  String                      destination;                   // destination for restore
  Owner                       owner;                         // restore owner
  FilePermission              permissions;                   // restore permissions

  PatternTypes                patternType;                   // pattern type

  CompressAlgorithmsDeltaByte compressAlgorithms;            // compress algorithms delta/byte

  CryptTypes                  cryptType;                     // crypt type (symmetric, asymmetric)
  CryptAlgorithms             cryptAlgorithms[4];            // crypt algorithms to use
  PasswordModes               cryptPasswordMode;             // crypt password mode
  Password                    cryptPassword;                 // crypt password
  Key                         cryptPublicKey;
  Key                         cryptPrivateKey;

  String                      preProcessScript;              // script to execute before start of job
  String                      postProcessScript;             // script to execute after after termination of job

  FileServer                  fileServer;                    // job specific file server settings
  FTPServer                   ftpServer;                     // job specific FTP server settings
  SSHServer                   sshServer;                     // job specific SSH server settings
  WebDAVServer                webDAVServer;                  // job specific WebDAV server settings
  OpticalDisk                 opticalDisk;                   // job specific optical disk settings
  String                      deviceName;                    // device name to use
  Device                      device;                        // job specific device settings

  uint64                      fragmentSize;                  // fragment size [bytes]
  uint64                      maxStorageSize;                // max. storage size [bytes]
  uint64                      volumeSize;                    // volume size or 0LL for default [bytes]

  String                      comment;                       // comment

  bool                        skipUnreadableFlag;            // TRUE for skipping unreadable files
  bool                        forceDeltaCompressionFlag;     // TRUE to force delta compression of files
  bool                        ignoreNoDumpAttributeFlag;     // TRUE for ignoring no-dump attribute
  ArchiveFileModes            archiveFileMode;               // archive files write mode
  RestoreEntryModes           restoreEntryMode;              // overwrite existing entry mode on restore
  bool                        errorCorrectionCodesFlag;      // TRUE iff error correction codes should be added
  bool                        alwaysCreateImageFlag;         // TRUE iff always create image for CD/DVD/BD/device
  bool                        blankFlag;                     // TRUE to blank medium before writing
  bool                        waitFirstVolumeFlag;           // TRUE for wait for first volume
  bool                        rawImagesFlag;                 // TRUE for storing raw images
  bool                        noFragmentsCheckFlag;          // TRUE to skip checking file fragments for completeness
  bool                        noIndexDatabaseFlag;           // TRUE for do not store index database for archives
  bool                        forceVerifySignaturesFlag;     // TRUE to force verify signatures of archives
  bool                        skipVerifySignaturesFlag;      // TRUE to not verify signatures of archives
  bool                        noSignatureFlag;               // TRUE for not appending signatures
  bool                        noBAROnMediumFlag;             // TRUE for not storing BAR on medium
  bool                        noStopOnErrorFlag;             // TRUE for not stopping immediately on error
  bool                        noStopOnAttributeErrorFlag;    // TRUE for not stopping immediately on attribute error
};

// job type
typedef enum
{
  JOB_TYPE_NONE,
  JOB_TYPE_CREATE,
  JOB_TYPE_RESTORE,
} JobTypes;

// job
typedef struct
{
  String              uuid;                             // unique id

  struct
  {
    String name;
    uint   port;
    bool   forceSSL;
  }                   slaveHost;                        // slave host

//TODO: rename: storageName
  String              archiveName;                      // archive name

  bool                storageNameListStdin;             // read storage names from stdin
  String              storageNameListFileName;          // storage names list file name
  String              storageNameCommand;               // storage names command

  EntryList           includeEntryList;                 // included entries
  PatternList         excludePatternList;               // excluded entry patterns

  JobOptions          options;                          // options for job
} Job;

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
  JOB_STATE_ABORTED,
  JOB_STATE_DISCONNECTED
} JobStates;

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
  String              name;                             // name of job
  JobTypes            jobType;                          // job type

  // modified info
  bool                modifiedFlag;                     // TRUE iff job config modified

  // schedule info
  uint64              lastScheduleCheckDateTime;        // last check date/time (timestamp)

  // job file/master
  String              fileName;                         // file name or NULL
  uint64              fileModified;                     // file modified date/time (timestamp)

  ServerIO            *masterIO;                        // master i/o or NULL if not a slave job

  // job running state
  ConnectorInfo       connectorInfo;

  JobStates           state;                            // current state of job
  SlaveStates         slaveState;

  StatusInfo          statusInfo;

  String              scheduleUUID;                     // current schedule UUID or empty
  String              scheduleCustomText;               // schedule custom text or empty
  ArchiveTypes        archiveType;                      // archive type to create
  StorageFlags        storageFlags;                     // storage flags; see STORAGE_FLAG_...
  uint64              startDateTime;                    // start date/time [s]
  String              byName;                           // state changed by name

  bool                requestedAbortFlag;               // request abort current job execution
  String              abortedByInfo;                    // aborted by info
  uint                requestedVolumeNumber;            // requested volume number
  uint                volumeNumber;                     // load volume number
  String              volumeMessage;                    // load volume message
  bool                volumeUnloadFlag;                 // TRUE to unload volume

  uint64              lastExecutedDateTime;             // last execution date/time (timestamp) (Note: read from <jobs dir>/.<job name>)
  String              lastErrorMessage;

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

  // cached statistics info
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
* Input  : semaphoreLockType - lock type; see SemaphoreLockTypes
*          timeout           - timeout [ms] or NO_WAIT, WAIT_FOREVER
* Output : -
* Return : -
* Notes  : usage:
*            JOB_LIST_LOCKED_DO(semaphoreLockType,timeout)
*            {
*              ...
*            }
*
*          semaphore must be unlocked manually if 'break' is used!
\***********************************************************************/

#define JOB_LIST_LOCKED_DO(semaphoreLockType,timeout) \
  for (SemaphoreLock semaphoreLock = Job_listLock(semaphoreLockType,timeout); \
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
* Name   : Job_init
* Purpose: initialize job
* Input  : job - job
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_init(Job *job);

/***********************************************************************\
* Name   : Job_initDuplicate
* Purpose: init duplicate job
* Input  : job     - job
*          fromJob - copy from job
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_initDuplicate(Job *job, const Job *fromJob);

/***********************************************************************\
* Name   : Job_done
* Purpose: done job
* Input  : job - job
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_done(Job *job);

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
* Input  : jobType  - job type
*          name     - name
*          jobUUID  - job UUID or NULL for generate new UUID
*          fileName - file name or NULL
* Output : -
* Return : job node
* Notes  : -
\***********************************************************************/

JobNode *Job_new(JobTypes    jobType,
                 ConstString name,
                 ConstString jobUUID,
                 ConstString fileName
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

  return String_isEmpty(jobNode->job.slaveHost.name);
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

  return !String_isEmpty(jobNode->job.slaveHost.name);
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
* Name   : Job_getStateText
* Purpose: get text for job state
* Input  : jobState     - job state
*          storageFlags - storage flags; see STORAGE_FLAG_...
* Output : -
* Return : text
* Notes  : -
\***********************************************************************/

const char *Job_getStateText(JobStates jobState, StorageFlags storageFlags);

/***********************************************************************\
* Name   : Job_find
* Purpose: find job by name
* Input  : name - job name
* Output : -
* Return : job node or NULL if not found
* Notes  : -
\***********************************************************************/

INLINE JobNode *Job_find(ConstString name);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE JobNode *Job_find(ConstString name)
{
  JobNode *jobNode;

  assert(name != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  jobNode = LIST_FIND(&jobList,jobNode,String_equals(jobNode->name,name));

  return jobNode;
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_exists
* Purpose: check if job exists by name
* Input  : name - job name
* Output : -
* Return : TRUE iff job exists
* Notes  : -
\***********************************************************************/

INLINE bool Job_exists(ConstString name);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE bool Job_exists(ConstString name)
{
  JobNode *jobNode;

  assert(Semaphore_isLocked(&jobList.lock));

  return LIST_CONTAINS(&jobList,jobNode,String_equals(jobNode->name,name));
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_findByUUID
* Purpose: find job by uuid
* Input  : uuid - job uuid
* Output : -
* Return : job node or NULL if not found
* Notes  : -
\***********************************************************************/

INLINE JobNode *Job_findByUUID(ConstString uuid);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE JobNode *Job_findByUUID(ConstString uuid)
{
  JobNode *jobNode;

  assert(uuid != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  jobNode = LIST_FIND(&jobList,jobNode,String_equals(jobNode->job.uuid,uuid));

  return jobNode;
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_findByName
* Purpose: find job by name
* Input  : name - job name
* Output : -
* Return : job node or NULL if not found
* Notes  : -
\***********************************************************************/

INLINE JobNode *Job_findByName(ConstString name);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE JobNode *Job_findByName(ConstString name)
{
  JobNode *jobNode;

  assert(name != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  jobNode = LIST_FIND(&jobList,jobNode,String_equals(jobNode->name,name));

  return jobNode;
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

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
* Name   : Job_persistenceChanged
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
* Input  : jobsDirectory - directory with job files
* Output : -
* Return : ERROR_NONE or error code
* Notes  : update jobList
\***********************************************************************/

Errors Job_rereadAll(ConstString jobsDirectory);

/***********************************************************************\
* Name   : Job_setModified
* Purpose: set job modified
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Job_setModified(JobNode *jobNode);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE void Job_setModified(JobNode *jobNode)
{
  jobNode->modifiedFlag = TRUE;
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_trigger
* Purpose: trogger job run
* Input  : jobNode            - job node
*          scheduleUUID       - schedule UUID or NULL
*          scheduleCustomText - schedule custom text or NULL
*          archiveType        - archive type to create
*          storageFlags       - storage flags; see STORAGE_FLAG_...
*          startDateTime      - date/time of start [s]
*          byName             - by name or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_trigger(JobNode      *jobNode,
                 ConstString  scheduleUUID,
                 ConstString  scheduleCustomText,
                 ArchiveTypes archiveType,
                 StorageFlags storageFlags,
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
* Name   : Job_end
* Purpose: end job (store running data, free job data, e. g. passwords)
* Input  : jobNode            - job node
*          executeEndDateTime - executed date/time (timestamp)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_end(JobNode *jobNode, uint64 executeEndDateTime);

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
* Name   : Job_clearOptions
* Purpose: clear job options
* Input  : jobOptions - job options variable
* Output : jobOptions - cleared job options
* Return : -
* Notes  : -
\***********************************************************************/

void Job_clearOptions(JobOptions *jobOptions);

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
