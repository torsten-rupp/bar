/***********************************************************************\
*
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

#include "bar_common.h"
#include "configuration.h"
#include "entrylists.h"
#include "connector.h"

/****************** Conditional compilation switches *******************/
#define MAX_CRYPT_ALGORITHMS 4

/***************************** Constants *******************************/

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
  FilePermissions             permissions;                   // restore permissions
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
  String                      slavePreProcessScript;         // script to execute before start of job on slave
  String                      slavePostProcessScript;        // script to execute after after termination of job on slave

  String                      par2Directory;                 // PAR2 checksum output directory or NULL
  uint                        par2BlockSize;                 // PAR2 block size [bytes]
  uint                        par2FileCount;                 // number of PAR2 checksum files to create
  uint                        par2BlockCount;                // number of PAR2 error correction blocks

  bool                        storageOnMasterFlag;           // TRUE for storage operation on master
  FileServer                  fileServer;                    // job specific file server settings
  FTPServer                   ftpServer;                     // job specific FTP server settings
  SSHServer                   sshServer;                     // job specific SSH server settings
  WebDAVServer                webDAVServer;                  // job specific WebDAV server settings
  SMBServer                   smbServer;                     // job specific SMB server settings
  OpticalDisk                 opticalDisk;                   // job specific optical disk settings
  Device                      device;                        // job specific device settings

  uint64                      fragmentSize;                  // fragment size [bytes]
  uint64                      maxStorageSize;                // max. storage size [bytes]

  uint64                      volumeSize;                    // volume size or 0LL for default [bytes]

  String                      comment;                       // comment

  bool                        testCreatedArchivesFlag;       // TRUE to test archives after creation
  bool                        skipUnreadableFlag;            // TRUE for skipping unreadable files
  bool                        forceDeltaCompressionFlag;     // TRUE to force delta compression of files
  bool                        ignoreNoDumpAttributeFlag;     // TRUE for ignoring no-dump attribute
  ArchiveFileModes            archiveFileMode;               // archive files write mode
  RestoreEntryModes           restoreEntryMode;              // overwrite existing entry mode on restore
  bool                        sparseFilesFlag;                    // TRUE to create sparse files
  bool                        errorCorrectionCodesFlag;      // TRUE iff error correction codes should be added
  bool                        alwaysCreateImageFlag;         // TRUE iff always create image for CD/DVD/BD/device
  bool                        blankFlag;                     // TRUE to blank medium before writing
  bool                        waitFirstVolumeFlag;           // TRUE for wait for first volume
  bool                        rawImagesFlag;                 // TRUE for storing raw images
  bool                        noFragmentsCheckFlag;          // TRUE to skip checking file fragments for completeness
  bool                        noIndexDatabaseFlag;           // TRUE for do not store index database for archives
  bool                        forceVerifySignaturesFlag;     // TRUE to force verify signatures of archives
  bool                        skipVerifySignaturesFlag;      // TRUE to not verify signatures of archives
  bool                        noStorage;                     // TRUE to skip create storages
  bool                        noSignatureFlag;               // TRUE for not appending signatures
  bool                        noBAROnMediumFlag;             // TRUE for not storing BAR on medium
  bool                        noStopOnErrorFlag;             // TRUE for not stopping immediately on error
  bool                        noStopOnOwnerErrorFlag;        // TRUE for not stopping immediately on owner error
  bool                        noStopOnAttributeErrorFlag;    // TRUE for not stopping immediately on attribute error
  bool                        dryRun;                        // TRUE for dry-run only
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
    String   name;
    uint     port;
    TLSModes tlsMode;
  }                   slaveHost;                        // slave host

  String              storageName;                      // storage name

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
  JOB_STATE_DONE,
  JOB_STATE_ERROR,
  JOB_STATE_ABORTED,
  JOB_STATE_DISCONNECTED
} JobStates;

typedef Set JobStateSet;

#define JOB_STATESET_ALL \
    SET_VALUE(JOB_STATE_NONE) \
  | SET_VALUE(JOB_STATE_WAITING) \
  | SET_VALUE(JOB_STATE_RUNNING) \
  | SET_VALUE(JOB_STATE_DONE) \
  | SET_VALUE(JOB_STATE_ERROR) \
  | SET_VALUE(JOB_STATE_ABORTED) \
  | SET_VALUE(JOB_STATE_DISCONNECTED)

// slave states
typedef enum
{
  SLAVE_STATE_OFFLINE,
  SLAVE_STATE_ONLINE,
  SLAVE_STATE_WRONG_MODE,
  SLAVE_STATE_WRONG_PROTOCOL_VERSION,
  SLAVE_STATE_PAIRED
} SlaveStates;

typedef Set SlaveStateSet;

#define SLAVE_STATESET_ALL \
    SET_VALUE(SLAVE_STATE_OFFLINE) \
  | SET_VALUE(SLAVE_STATE_ONLINE) \
  | SET_VALUE(SLAVE_STATE_WRONG_MODE) \
  | SET_VALUE(SLAVE_STATE_WRONG_PROTOCOL_VERSION) \
  | SET_VALUE(SLAVE_STATE_PAIRED)

// job node
typedef struct JobNode
{
  LIST_NODE_HEADER(struct JobNode);

  // job
  Job                 job;
  String              name;                             // name of job
  JobTypes            jobType;                          // job type

  // modified flags
  bool                modifiedFlag;                     // TRUE iff job config modified
  bool                includeExcludeModifiedFlag;
  bool                mountModifiedFlag;
  bool                scheduleModifiedFlag;
  bool                persistenceModifiedFlag;

  // schedule info
  uint64              lastScheduleCheckDateTime;        // last check date/time (timestamp)

  // job file/master
  String              fileName;                         // file name or NULL
  uint64              fileModified;                     // file modified date/time (timestamp)

  ServerIO            *masterIO;                        // master i/o or NULL if not a slave job

  // job running state
  JobStates           jobState;
//TODO: required?
  SlaveStates         slaveState;
  bool                slaveTLS;                         // TRUE if slave TLS connection established
  bool                slaveInsecureTLS;                 // TRUE if insecure slave TLS connection established

  String              scheduleUUID;                     // schedule UUID or empty
  ArchiveTypes        archiveType;                      // archive type to create
  String              customText;                       // custom text or empty
  bool                testCreatedArchives;              // TRUE to test created archives
  bool                noStorage;                        // TRUE to skip create storages
  bool                dryRun;                           // TRUE for dry-run only
  uint64              startDateTime;                    // start date/time [s]
  String              byName;                           // state changed by name

  bool                requestedAbortFlag;               // request abort current job execution
  String              abortedByInfo;                    // aborted by info
  uint                volumeNumber;                     // load volume number
  bool                volumeUnloadFlag;                 // TRUE to unload volume

// TODO: combine
//  StatusInfo          statusInfo;
  RunningInfo         runningInfo;

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
  uint      activeCount;                                // number of currently active jobs

  #ifndef NDEBUG
    uint64 lockTimestamp;
    #ifdef HAVE_BACKTRACE
      const void * lockStackTrace[16];
      int          lockStackTraceSize;
    #endif /* HAVE_BACKTRACE */
  #endif /* NDEBUG */
} JobList;

//TODO: move to connector.h?
// slave list
typedef struct SlaveNode
{
  LIST_NODE_HEADER(struct SlaveNode);

  String        name;
  uint          port;
  TLSModes      tlsMode;
  ConnectorInfo connectorInfo;
  uint64        lastOnlineDateTime;
  bool          authorizedFlag;
  uint          lockCount;                              // >0 iff locked
} SlaveNode;

typedef struct
{
  LIST_HEADER(SlaveNode);

  Semaphore lock;
} SlaveList;

/***************************** Variables *******************************/
extern SlaveList slaveList;
extern JobList   jobList;

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define Job_listLock(...)        __Job_listLock       (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Job_listUnlock(...)      __Job_listUnlock     (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Job_listWaitModifed(...) __Job_listWaitModifed(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

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
  for (SemaphoreLock __semaphoreLock ## __COUNTER__ = Job_listLock(semaphoreLockType,timeout); \
       __semaphoreLock ## __COUNTER__; \
       Job_listUnlock(), __semaphoreLock ## __COUNTER__ = FALSE \
      )

/***********************************************************************\
* Name   : JOB_LIST_ITERATE
* Purpose: iterated over list and execute block
* Input  : variable - iteration variable
* Output : -
* Return : -
* Notes  : variable will contain all entries in list
*          usage:
*            JOB_LIST_ITERATE(variable)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

//TODO:
#if 0
#define JOB_LIST_ITERATE(variable) \
  for (JobNode *variable = jobList.head; \
       variable != NULL; \
       variable = (variable)->next \
      )
#endif
#define JOB_LIST_ITERATE(variable) \
  for ((variable) = jobList.head; \
       (variable) != NULL; \
       (variable) = (variable)->next \
      )

/***********************************************************************\
* Name   : JOB_LIST_ITERATEX
* Purpose: iterated over list and execute block with condition
* Input  : variable  - iteration variable
*          condition - additional condition
* Output : -
* Return : -
* Notes  : variable will contain all entries in list
*          usage:
*            JOB_LIST_ITERATEX(variable,TRUE)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define JOB_LIST_ITERATEX(variable,condition) \
  for ((variable) = jobList.head; \
       ((variable) != NULL) && (condition); \
       (variable) = (variable)->next \
      )

/***********************************************************************\
* Name   : JOB_LIST_FIND
* Purpose: find job in job list
* Input  : variable  - variable name
*          condition - condition code
* Output : -
* Return : slave node or NULL if not found
* Notes  : usage:
*          slaveNode = LIST_FIND(variable,variable->...)
\***********************************************************************/

#define JOB_LIST_FIND(variable,condition) \
  ({ \
    auto typeof(variable) __closure__ (void); \
    typeof(variable) __closure__ (void) \
    { \
      variable = (typeof(variable))jobList.head; \
      while ((variable != NULL) && !(condition)) \
      { \
        variable = variable->next; \
      } \
      \
      return variable; \
    } \
    __closure__(); \
  })

/***********************************************************************\
* Name   : JOB_LIST_CONTAINS
* Purpose: check if job is in job list
* Input  : list      - list
*          variable  - variable name
*          condition - condition code
* Output : -
* Return : TRUE iff in list
* Notes  : usage:
*          boolean = JOB_LIST_CONTAINS(variable,variable->... == ...)
\***********************************************************************/

#define JOB_LIST_CONTAINS(variable,condition) (JOB_LIST_FIND(variable,condition) != NULL)


/***********************************************************************\
* Name   : JOB_SLAVE_LIST_HEAD
* Purpose: get job slave list head (first node)
* Input  : -
* Output : -
* Return : slave list head
* Notes  : -
\***********************************************************************/

#define JOB_SLAVE_LIST_HEAD() slaveList.head

/***********************************************************************\
* Name   : JOB_SLAVE_LIST_LOCKED_DO
* Purpose: execute block with slave list locked
* Input  : semaphoreLockType - lock type; see SemaphoreLockTypes
*          timeout           - timeout [ms] or NO_WAIT, WAIT_FOREVER
* Output : -
* Return : -
* Notes  : usage:
*            SLAVE_LIST_LOCKED_DO(semaphoreLockType,timeout)
*            {
*              ...
*            }
*
*          semaphore must be unlocked manually if 'break' is used!
\***********************************************************************/

#define JOB_SLAVE_LIST_LOCKED_DO(semaphoreLockType,timeout) \
  for (SemaphoreLock __semaphoreLock ## __COUNTER__ = Semaphore_lock(&slaveList.lock,semaphoreLockType,timeout); \
       __semaphoreLock ## __COUNTER__; \
       Semaphore_unlock(&slaveList.lock), __semaphoreLock ## __COUNTER__ = FALSE \
      )

/***********************************************************************\
* Name   : JOB_SLAVE_LIST_ITERATE
* Purpose: iterated over slave list and execute block
* Input  : variable - iteration variable
* Output : -
* Return : -
* Notes  : variable will contain all entries in list
*          usage:
*            SLAVE_LIST_ITERATE(variable)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define JOB_SLAVE_LIST_ITERATE(variable) \
  for ((variable) = slaveList.head; \
       (variable) != NULL; \
       (variable) = (variable)->next \
      )

/***********************************************************************\
* Name   : JOB_SLAVE_LIST_ITERATEX
* Purpose: lock and iterated over list and execute block
* Input  : variable  - iteration variable
*          condition - additional condition
* Output : -
* Return : -
* Notes  : variable will contain all entries in list
*          usage:
*            SLAVE_LIST_ITERATEX(variable,TRUE)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define JOB_SLAVE_LIST_ITERATEX(variable,condition) \
  for ((variable) = slaveList.head; \
       ((variable) != NULL) && (condition); \
       (variable) = (variable)->next \
      )

/***********************************************************************\
* Name   : JOB_SLAVE_LIST_FIND
* Purpose: find slave in job slave list
* Input  : variable  - variable name
*          condition - condition code
* Output : -
* Return : slave node or NULL if not found
* Notes  : usage:
*          slaveNode = LIST_FIND(variable,variable->...)
\***********************************************************************/

#define JOB_SLAVE_LIST_FIND(variable,condition) \
  ({ \
    auto typeof(variable) __closure__ (void); \
    typeof(variable) __closure__ (void) \
    { \
      variable = (typeof(variable))slaveList.head; \
      while ((variable != NULL) && !(condition)) \
      { \
        variable = variable->next; \
      } \
      \
      return variable; \
    } \
    __closure__(); \
  })

/***********************************************************************\
* Name   : JOB_SLAVE_LIST_CONTAINS
* Purpose: check if slave is in job slave list
* Input  : list      - list
*          variable  - variable name
*          condition - condition code
* Output : -
* Return : TRUE iff in list
* Notes  : usage:
*          boolean = JOB_SLAVE_LIST_CONTAINS(variable,variable->... == ...)
\***********************************************************************/

#define JOB_SLAVE_LIST_CONTAINS(variable,condition) (JOB_SLAVE_LIST_FIND(variable,condition) != NULL)

/***********************************************************************\
* Name   : JOB_CONNECTOR_LOCKED_DO
* Purpose: execute block with job connector locked
* Input  : jobNode           - job node
*          semaphoreLockType - lock type; see SemaphoreLockTypes
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

#define JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,timeout) \
  for (connectorInfo = Job_connectorLock(jobNode,timeout); \
       connectorInfo != NULL; \
       Job_connectorUnlock(connectorInfo,timeout), connectorInfo = NULL \
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
* Name   : Job_newScheduleNode
* Purpose: allocate new schedule node
* Input  : scheduleUUID - schedule UUID or NULL
* Output : -
* Return : new schedule node
* Notes  : -
\***********************************************************************/

ScheduleNode *Job_newScheduleNode(ConstString scheduleUUID);

/***********************************************************************\
* Name   : Job_duplicateScheduleNode
* Purpose: duplicate schedule node
* Input  : fromScheduleNode - from schedule node
*          userData      - user data (not used)
* Output : -
* Return : duplicated schedule node
* Notes  : -
\***********************************************************************/

ScheduleNode *Job_duplicateScheduleNode(ScheduleNode *fromScheduleNode,
                                        void         *userData
                                       );

/***********************************************************************\
* Name   : Job_freeScheduleNode
* Purpose: free schedule node
* Input  : scheduleNode - schedule node
*          userData     - not used
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_freeScheduleNode(ScheduleNode *scheduleNode, void *userData);

/***********************************************************************\
* Name   : Job_deleteScheduleNode
* Purpose: delete schedule node
* Input  : scheduleNode - schedule node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_deleteScheduleNode(ScheduleNode *scheduleNode);

/***********************************************************************\
* Name   : Job_newPersistenceNode
* Purpose: allocate new persistence node
* Input  : archiveType     - archive type; see ArchiveTypes
*          minKeep,maxKeep - min./max. keep
*          maxAge          - max. age [days] or AGE_FOREVER
*          moveTo          - move-to URI or ""
* Output : -
* Return : new persistence node
* Notes  : -
\***********************************************************************/

PersistenceNode *Job_newPersistenceNode(ArchiveTypes archiveType,
                                        int          minKeep,
                                        int          maxKeep,
                                        int          maxAge,
                                        ConstString  moveTo
                                       );

/***********************************************************************\
* Name   : Job_duplicatePersistenceNode
* Purpose: duplicate persistence node
* Input  : fromPersistenceNode - from persistence node
*          userData            - user data (not used)
* Output : -
* Return : duplicated persistence node
* Notes  : -
\***********************************************************************/

PersistenceNode *Job_duplicatePersistenceNode(PersistenceNode *fromPersistenceNode,
                                              void            *userData
                                             );

/***********************************************************************\
* Name   : Job_insertPersistenceNode
* Purpose: insert persistence node into list
* Input  : persistenceList - persistence list
*          persistenceNode - persistence node
* Output : -
* Return : -
* Notes  : peristennce list sorted ascending in age
\***********************************************************************/

void Job_insertPersistenceNode(PersistenceList *persistenceList,
                               PersistenceNode *persistenceNode
                              );

/***********************************************************************\
* Name   : freePersistenceNode
* Purpose: free persistence node
* Input  : persistenceNode - persistence node
*          userData        - not used
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_freePersistenceNode(PersistenceNode *persistenceNode, void *userData);

/***********************************************************************\
* Name   : Job_deletePersistenceNode
* Purpose: delete persistence node
* Input  : persistenceNode - persistence node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_deletePersistenceNode(PersistenceNode *persistenceNode);

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

#ifdef NDEBUG
INLINE bool Job_listLock(SemaphoreLockTypes semaphoreLockType,
                         long               timeout
                        );
#else /* not NDEBUG */
INLINE bool __Job_listLock(const char         *__fileName__,
                           ulong              __lineNb__,
                           SemaphoreLockTypes semaphoreLockType,
                           long               timeout
                          );
#endif /* NDEBUG */
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
#ifdef NDEBUG
INLINE bool Job_listLock(SemaphoreLockTypes semaphoreLockType,
                         long               timeout
                        )
#else /* not NDEBUG */
INLINE bool __Job_listLock(const char         *__fileName__,
                           ulong              __lineNb__,
                           SemaphoreLockTypes semaphoreLockType,
                           long               timeout
                          )
#endif /* NDEBUG */
{
  bool locked;

  #ifndef NDEBUG
    locked = __Semaphore_lock(__fileName__,__lineNb__,&jobList.lock,semaphoreLockType,timeout);
  #else /* not NDEBUG */
    locked = Semaphore_lock(&jobList.lock,semaphoreLockType,timeout);
  #endif /* NDEBUG */

  #ifndef NDEBUG
    if (locked)
    {
      jobList.lockTimestamp = Misc_getTimestamp();
      #ifdef HAVE_BACKTRACE
        jobList.lockStackTraceSize = getStackTrace(jobList.lockStackTrace,SIZE_OF_ARRAY(jobList.lockStackTrace));
      #endif /* HAVE_BACKTRACE */
    }
  #endif /* NDEBUG */

  return locked;
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

#ifdef NDEBUG
INLINE void Job_listUnlock(void);
#else /* not NDEBUG */
INLINE void __Job_listUnlock(const char *__fileName__,
                             ulong      __lineNb__
                            );
#endif /* NDEBUG */
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
#ifdef NDEBUG
INLINE void Job_listUnlock(void)
#else /* not NDEBUG */
INLINE void __Job_listUnlock(const char *__fileName__,
                             ulong      __lineNb__
                            )
#endif /* NDEBUG */
{
  #ifndef NDEBUG
    uint64 dt;
  #endif /* NDEBUG */

  #ifndef NDEBUG
    dt = Misc_getTimestamp()-jobList.lockTimestamp;
    if ((dt > 2*US_PER_S) && (Semaphore_lockCount(&jobList.lock) == 1))
    {
      fprintf(stderr,"Warning: long job list lock: %"PRIu64"ms\n",dt/(uint64)US_PER_MS);
      #ifdef HAVE_BACKTRACE
        debugDumpStackTrace(stderr,
                            0,
                            DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
                            jobList.lockStackTrace,
                            jobList.lockStackTraceSize,
                            0
                           );
      #endif /* HAVE_BACKTRACE */
    }
  #endif /* NDEBUG */

  #ifndef NDEBUG
    __Semaphore_unlock(__fileName__,__lineNb__,&jobList.lock);
  #else /* not NDEBUG */
    Semaphore_unlock(&jobList.lock);
  #endif /* NDEBUG */
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_isListLocked
* Purpose: check if job list locked
* Input  : -
* Output : -
* Return : TRUE iff locked
* Notes  : debug only
\***********************************************************************/

#ifndef NDEBUG
INLINE bool Job_isListLocked(void);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE bool Job_isListLocked(void)
{
  return Semaphore_isLocked(&jobList.lock);
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Job_listSignalModifed
* Purpose: check if job list lock pending
* Input  : -
* Output : -
* Return : TRUE iff pendind
* Notes  : -
\***********************************************************************/

INLINE bool Job_isListLockPending(void);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE bool Job_isListLockPending(void)
{
  return Semaphore_isLockPending(&jobList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_listSignalModifed
* Purpose: signal job list modified
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Job_listSignalModifed(void);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE void Job_listSignalModifed(void)
{
  Semaphore_signalModified(&jobList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_listWaitModifed
* Purpose: wait job list modified
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
INLINE void Job_listWaitModifed(long timeout);
#else /* not NDEBUG */
INLINE void __Job_listWaitModifed(const char *__fileName__,
                                  ulong      __lineNb__,
                                  long timeout
                                 );
#endif /* NDEBUG */
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
#ifdef NDEBUG
INLINE void Job_listWaitModifed(long timeout)
#else /* not NDEBUG */
INLINE void __Job_listWaitModifed(const char *__fileName__,
                                  ulong      __lineNb__,
                                  long timeout
                                 )
#endif /* NDEBUG */
{
  #ifndef NDEBUG
    __Semaphore_waitModified(__fileName__,__lineNb__,&jobList.lock,timeout);
  #else /* not NDEBUG */
    Semaphore_waitModified(&jobList.lock,timeout);
  #endif /* NDEBUG */
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_listSetEnd
* Purpose: set end-flag in job list
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Job_listSetEnd(void);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE void Job_listSetEnd(void)
{
  Semaphore_setEnd(&jobList.lock);
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
* Name   : Job_setListModified
* Purpose: called when job list changed
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_setListModified(void);

/***********************************************************************\
* Name   : Job_listAppend
* Purpose: append job to job list
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Job_listAppend(JobNode *jobNode);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE void Job_listAppend(JobNode *jobNode)
{
  assert(jobNode != NULL);

  List_append(&jobList,jobNode);
  Job_setListModified();
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_listRemove
* Purpose: remove job from job list
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Job_listRemove(JobNode *jobNode);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE void Job_listRemove(JobNode *jobNode)
{
  assert(jobNode != NULL);

  List_remove(&jobList,jobNode);
  Job_setListModified();
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
* Name   : Job_parseState
* Purpose: parse job state
* Input  : name     - name of job state
* Output : jobState  - job state
*          noStorage - no storage (can be NULL)
*          dryRun    - dry run (can be NULL)
* Return : TRUE if parsed
* Notes  : -
\***********************************************************************/

bool Job_parseState(const char *name, JobStates *jobState, bool *noStorage, bool *dryRun);

/***********************************************************************\
* Name   : Job_getStateText
* Purpose: get text for job state
* Input  : jobState  - job state
*          noStorage - TRUE to skip create storages
*          dryRun    - TRUE for dry-run only
* Output : -
* Return : text
* Notes  : -
\***********************************************************************/

const char *Job_getStateText(JobStates jobState, bool noStorage, bool dryRun);

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
* Name   : Job_setModified
* Purpose: set job modified
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_setModified(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_setIncludeExcludeModified
* Purpose: actions when includes/excludes modified, set modified
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Job_setIncludeExcludeModified(JobNode *jobNode);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE void Job_setIncludeExcludeModified(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  jobNode->includeExcludeModifiedFlag = TRUE;
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_setMountModified
* Purpose: actions when mounts modified, set modified
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Job_setMountModified(JobNode *jobNode);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE void Job_setMountModified(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  jobNode->mountModifiedFlag = TRUE;
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_setScheduleModified
* Purpose: actions when schedule modified, set modified
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_setScheduleModified(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_setPersistenceModified
* Purpose: actions when persistence modified, set modified
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Job_setPersistenceModified(JobNode *jobNode);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE void Job_setPersistenceModified(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  jobNode->persistenceModifiedFlag = TRUE;
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_flushModified
* Purpose: flush modified job data
* Input  : jobNode - job node
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

void Job_flush(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_flushAll
* Purpose: flush all modified job data
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

void Job_flushAll(void);

/***********************************************************************\
* Name   : Job_readScheduleInfo
* Purpose: read job schedule info
* Input  : jobNode - job node
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Job_readScheduleInfo(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_writeScheduleInfo
* Purpose: write job schedule info
* Input  : jobNode            - job node
*          archiveType        - archive type
*          executeEndDateTime - executed date/time (timestamp)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Job_writeScheduleInfo(JobNode *jobNode, ArchiveTypes archiveType, uint64 executeEndDateTime);

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
* Name   : Job_write
* Purpose: write (update) job file
* Input  : jobNode - job node
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Job_write(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_trigger
* Purpose: trogger job run
* Input  : jobNode             - job node
*          scheduleUUID        - schedule UUID or NULL
*          scheduleCustomText  - schedule custom text or NULL
*          archiveType         - archive type to create
*          testCreatedArchives - TRUE for test created archives
*          noStorage           - TRUE for skip create storages
*          dryRun              - TRUE for dry-run only
*          startDateTime       - date/time of start [s]
*          byName              - by name or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_trigger(JobNode      *jobNode,
                 ConstString  scheduleUUID,
                 ArchiveTypes archiveType,
                 ConstString  customText,
                 bool         testCreatedArchives,
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
* Name   : Job_end
* Purpose: end job (store running data, free resources)
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_end(JobNode *jobNode);

/***********************************************************************\
* Name   : Job_abort
* Purpose: abort job
* Input  : jobNode       - job node
*          abortedByInfo - abort-by info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_abort(JobNode *jobNode, const char *abortedByInfo);

/***********************************************************************\
* Name   : Job_reset
* Purpose: reset job
* Input  : jobNode - job node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_reset(JobNode *jobNode);

#if 0
/***********************************************************************\
* Name   : Job_add
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
* Name   : Job_copyOptions
* Purpose: copy job options
* Input  : jobOptions     - job options variable
*          fromJobOptions - source job options
* Output : jobOptions - initialized job options
* Return : -
* Notes  : -
\***********************************************************************/

void Job_copyOptions(JobOptions *jobOptions, const JobOptions *fromJobOptions);

/***********************************************************************\
* Name   : Job_getOptions
* Purpose: get job name and options of job
* Input  : jobName    - job name variable (can be NULL)
*          jobOptions - job options variable
*          uuid       - job UUID
* Output : jobName    - job name or UUID
*          jobOptions - initialized job options
* Return : -
* Notes  : -
\***********************************************************************/

void Job_getOptions(String jobName, JobOptions *jobOptions, ConstString uuid);

/***********************************************************************\
* Name   : Job_doneOptions
* Purpose: done job options
* Input  : jobOptions - job options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_doneOptions(JobOptions *jobOptions);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Job_duplicatePersistenceList
* Purpose: duplicate persistence list
* Input  : persistenceList     - persistence list
*          fromPersistenceList - from persistence list
* Output : persistenceList - persistence list
* Return : -
* Notes  : -
\***********************************************************************/

void Job_duplicatePersistenceList(PersistenceList *persistenceList, const PersistenceList *fromPersistenceList);

/***********************************************************************\
* Name   : Job_donePersistenceList
* Purpose: done persistence list
* Input  : persistenceList - persistence list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_donePersistenceList(PersistenceList *persistenceList);

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : Job_addSlave
* Purpose: add slave
* Input  : name    - host name
*          port    - host port
*          tlsMode - TLS mode; see TLS_MODES_...
* Output : -
* Return : slave node
* Notes  : -
\***********************************************************************/

SlaveNode *Job_addSlave(ConstString name, uint port, TLSModes tlsMode);

/***********************************************************************\
* Name   : Job_removeSlave
* Purpose: remove slave
* Input  : slaveNode - slave node
* Output : -
* Return : next slave node
* Notes  : -
\***********************************************************************/

SlaveNode *Job_removeSlave(SlaveNode *slaveNode);

/***********************************************************************\
* Name   : Job_isSlaveLocked
* Purpose: check if slave is locked
* Input  : slaveNode - slave node
* Output : -
* Return : TRUE iff slave is locked
* Notes  : -
\***********************************************************************/

INLINE bool Job_isSlaveLocked(SlaveNode *slaveNode);
#if defined(NDEBUG) || defined(__JOBS_IMPLEMENTATION__)
INLINE bool Job_isSlaveLocked(SlaveNode *slaveNode)
{
  assert(slaveNode != NULL);
  assert(Semaphore_isLocked(&slaveList.lock));

  return slaveNode->lockCount > 0;
}
#endif /* NDEBUG || __JOBS_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : Job_connectorLock
* Purpose: lock connector info
* Input  : jobNode - job node
*          timeout - timeout or NO_WAIT/WAIT_FOREVER
* Output : -
* Return : connector info or NULL
* Notes  : -
\***********************************************************************/

ConnectorInfo *Job_connectorLock(const JobNode *jobNode, long timeout);

/***********************************************************************\
* Name   : Job_connectorUnlock
* Purpose: unlock connector info
* Input  : connectorInfo - connector info
*          timeout       - timeout or NO_WAIT/WAIT_FOREVER
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_connectorUnlock(ConnectorInfo *connectorInfo, long timeout);

/***********************************************************************\
* Name   : Job_updateNotifies
* Purpose: update notifies of job
* Input  : jobNode - job
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_updateNotifies(const JobNode *jobNode);

/***********************************************************************\
* Name   : Job_updateAllNotifies
* Purpose: update notifies of all jobs
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Job_updateAllNotifies(void);

#ifdef __cplusplus
  }
#endif

#endif /* __JOBS__ */

/* end of file */
